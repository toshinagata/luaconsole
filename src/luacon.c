/*
 *  luacon.c
 *  LuaConsole
 *
 *  Created by 永田 央 on 16/07/14.
 *  Copyright 2016 __MyCompanyName__. All rights reserved.
 *
 */

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "screen.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>

extern int lm_load_file(const char *fname);
extern int lm_list(int sline, int eline);

lua_State *gL;

u_int8_t gSourceTouched;

u_int8_t lm_runmode;

u_int32_t lm_tick;

char lm_readline_buf[LM_BUFSIZE];

char *lm_base_dir;
char *lm_filename;

int
lm_interrupt(lua_State *L)
{
	return luaL_error(L, "Interrupt");
}

static u_int32_t lm_hook_count = 0;

static void
s_lm_hook(lua_State *L, lua_Debug *ar)
{
	static struct timeval tval0 = {0, 0}, tval;
	if (++lm_hook_count % 200 == 0) {
		if (tval0.tv_sec == 0)
			gettimeofday(&tval0, NULL);
		gettimeofday(&tval, NULL);
		if ((tval.tv_sec - tval0.tv_sec) * 1000000 + tval.tv_usec - tval0.tv_usec > 100000) {
			/*  Every 0.1 seconds (sparse enough to avoid retarding execution, and
			    frequently enough to respond to user request within reasonable time)  */
			/*  Also, the screen update is performed at this time.  */
			tval0 = tval;
			if (lm_getch_with_timeout(0) == 3)
				lm_interrupt(L);
		}
	}
}

/*  os.exit replacement  */
static int
my_os_exit(lua_State *L)
{
	lua_pushstring(L, "Exit");
	return lua_error(L);
}
		
static int
l_list(lua_State *L)
{
	int st, en;
	int nargs = lua_gettop(L);
	st = 1;
	en = 999999;
	if (nargs > 0) {
		st = en = luaL_checkinteger(L, 1);
		if (nargs > 1) {
			en = luaL_checkinteger(L, 2);
		} else en += 9;
	}
	lm_list(st, en);
	return 0;
}

static int
l_edit(lua_State *L)
{
	extern int lm_edit(void);	
	gSourceTouched = 1;
	lm_edit();
	return 0;
}

static int
l_load(lua_State *L)
{
	const char *p;
	char *pp;
	size_t len;
	int n;
	p = luaL_checklstring(L, -1, &len);
	if (p != NULL) {
		asprintf(&pp, "%s/%s", lm_base_dir, p);
		n = lm_load_file(pp);
		free(pp);
		if (n == 1)
			luaL_error(L, "Cannot open file %s", p);
		else if (n == 2)
			luaL_error(L, "Source file too large");
		if (lm_filename != NULL)
			free(lm_filename);
		lm_filename = strdup(p);
	}
	gSourceTouched = 0;
	return 0;
}

static int
l_save(lua_State *L)
{
	const char *p;
	char *pp;
	FILE *fp;
	if (gSourceEnd == 0)
		luaL_error(L, "No source file");
	if (lua_gettop(L) == 0) {
		if (lm_filename == NULL)
			luaL_error(L, "File name needs to be specified");
		p = lm_filename;
	} else {
		size_t len;
		p = luaL_checklstring(L, -1, &len);
	}
	asprintf(&pp, "%s/%s", lm_base_dir, p);
	
	/*  Overwrite check  */
	fp = fopen(pp, "r");
	if (fp != NULL) {
		if (p != lm_filename) {
			char *ppp;
			char buf[40];
			asprintf(&ppp, "File %s already exists. Overwrite? (y/N) ", p);
			lm_puts(ppp);
			free(ppp);
			if (lm_getline(buf, sizeof(buf)) <= 0 || (buf[0] != 'y' && buf[0] != 'Y')) {
				free(pp);
				fclose(fp);
				lm_puts("Save canceled.\n");
				return 0;
			}
		}
		fclose(fp);
	}

	fp = fopen(pp, "w");
	if (fp == NULL) {
		free(pp);
		luaL_error(L, "Cannot open file %s", p);
	}
	if (fwrite(gSourceBasePtr, 1, gSourceEnd, fp) < gSourceEnd) {
		fclose(fp);
		free(pp);
		luaL_error(L, "File write error %s: %s", p, strerror(errno));
	}
	fclose(fp);
	free(pp);
	if (lm_filename != p) {
		if (lm_filename != NULL)
			free(lm_filename);
		lm_filename = strdup(p);
	}
	return 0;
}

static int
l_wait(lua_State *L)
{
	double d = luaL_checknumber(L, 1);
	usleep(d * 1000);
	return 0;
}

static const struct luaL_Reg sys_lib[] = {
	{"load", l_load},
	{"save", l_save},
	{"edit", l_edit},
	{"list", l_list},
	{"wait", l_wait},
	{NULL, NULL}
};

int
luaopen_sys(lua_State *L) 
{
#if LUA_VERSION_NUM >= 502
	luaL_newlib(L, sys_lib);
#else
	luaL_register(L, "sys", sys_lib);
#endif
	
	/*  Replace os.exit  */
	lua_getglobal(L, "os");
	lua_pushcfunction(L, my_os_exit);
	lua_setfield(L, -2, "exit");
	lua_pop(L, 1);
	
	return 1;
}

static lua_State *
s_lua_init(void)
{
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);
	luaL_requiref(L, "con", luaopen_con, 1);
	luaL_requiref(L, "sys", luaopen_sys, 1);
	
	/*  Define sys.platform as a string  */
	lua_getglobal(L, "sys");
	lua_pushstring(L, lm_platform_name());
	lua_setfield(L, -2, "platform");
	lua_pop(L, 1);
		
	lua_setup_platform(L);
	
	gSourceLimit = 65536;
	gSourceBasePtr = (u_int8_t *)realloc(gSourceBasePtr, gSourceLimit);
	gSourceEnd = 0;
	
	return L;
}

int
lua_setup(void)
{
	const char *version_str;
	char *p;
	
	gL = s_lua_init();
	
	lua_getglobal(gL, "_VERSION");
	version_str = lua_tostring(gL, -1);

	lm_puts("Lua Console [るあこん] v0.1\n");
	lm_puts(version_str);
	lm_puts(", Copyright © 1994-2006 Lua.org, PUC-Rio\n");

	lm_runmode = LM_AUTORUN_RUNMODE;

	lua_sethook(gL, s_lm_hook, LUA_MASKLINE, 0);
	
	p = getenv("LUACON_DIR");
	if (p != NULL) {
		if (chdir(p) != 0) {
			lm_puts("Cannot change directory to %s\n");
		}
	}
	
	if (chdir("lua") != 0) {
		int n;
		lm_puts("The directory 'lua' was not found");
#if defined(_WIN32)
		n = mkdir("lua");
#else
		n = mkdir("lua", 0755);
#endif
		if (n == 0) {
			lm_puts(". Created one.\n");
			chdir("lua");
		} else {
			lm_puts(", and cannot be created.\n");
		}
	}
	
	/*  Base directory = sys.basedir  */
	lm_base_dir = getcwd(NULL, 0);
	lua_getglobal(gL, "sys");
	lua_pushstring(gL, lm_base_dir);
	lua_setfield(gL, -2, "basedir");
	lua_pop(gL, 1);
	
	gSourceTouched = 0;
	
	return 0;
}

int
lua_loop(void)
{
	FILE *fp;
	int n, n1, n2, n3;

redo:
	if (lm_runmode == LM_AUTORUN_RUNMODE) {
		fp = fopen("../startup.lua", "r");
		if (fp != NULL) {
			fclose(fp);
			lm_puts("Running startup.lua...\n");
			n = luaL_dofile(gL, "../startup.lua");
			if (n != 0) {
				lm_runmode = LM_LUAERROR_RUNMODE;
				goto redo;
			}
		} else {
			lm_puts("startup.lua was not found.\n");
		}
		lm_puts("Ready.\n");
		lm_runmode = LM_COMMAND_RUNMODE;
		return 0;
		
	}
	
	if (lm_runmode == LM_LUAERROR_RUNMODE) {

		/*  The error message should be on the top of Lua stack  */
		size_t len;
		const char *p;
		p = luaL_checklstring(gL, -1, &len);
		if (strcmp(p, "Exit") != 0) {
			if (strncmp(p, "[string \"\"]:", 12) == 0) {
				p += 12;
				lm_puts("Line ");
			} else if (strncmp(p, "[string \"S\"]:1", 14) == 0) {
				p += 14;
				lm_puts("Line 0");
			}
			lm_puts(p);
			lm_puts("\n");
		}

		n = lua_gettop(gL);
		lua_pop(gL, n);

		lm_runmode = LM_COMMAND_RUNMODE;
		
	}
	
	if (lm_runmode == LM_COMMAND_RUNMODE) {
		static char s[1024];	
	retry:
		lm_puts(">> ");
		n = lm_getline(s + 4, sizeof s - 6);
		if (n <= 0) {
			if (n < 0)
				lm_puts("\n");  /*  Interrupted  */
			goto retry;
		} else {
			/*  Offset by 4, for prefix 'sys.'  */
			for (n = 4; s[n] > 0 && s[n] <= ' '; n++);
			if (s[n] == 0)
				return 0;
			if (isalpha(s[n])) {
				for (n1 = n + 1; isalnum(s[n1]); n1++);
				n1 -= n;
				if (strncmp(s + n, "new", n1) == 0) {
					/*  Initialize the program and lua engine  */
					if (gSourceEnd > 0) {
						lm_puts("The program will be deleted. Do you really want to do this? (y/N) ");
						if (lm_getline(s, sizeof s) < 1 || (s[0] != 'y' && s[0] != 'Y'))
							return 0;
					}
					gSourceEnd = 0;
					gSourceBasePtr[0] = 0;
					lua_close(gL);
					gL = s_lua_init();
					gSourceTouched = 0;
					return 0;
				}
				if (strncmp(s + n, "quit", n1) == 0) {
					lm_puts("[[[ LuaCon Finished ]]]\n");
					lm_update_screen();
					lm_runmode = LM_EXIT_RUNMODE;
					return 0;
				}
				if (strncmp(s + n, "run", n1) == 0) {
					if (gSourceEnd == 0)
						return 0;
					lm_runmode = LM_BATCH_RUNMODE;
					n = (luaL_loadbuffer(gL, (char *)gSourceBasePtr, gSourceEnd, "")
						|| lua_pcall(gL, 0, LUA_MULTRET, 0));
					lm_runmode = (n == 0 ? LM_COMMAND_RUNMODE : LM_LUAERROR_RUNMODE);
					return 0;
				}
				if (strncmp(s + n, "load", n1) == 0 ||
					strncmp(s + n, "save", n1) == 0 ||
					strncmp(s + n, "edit", n1) == 0 ||
					strncmp(s + n, "list", n1) == 0) {
					/*  Insert '()' if not present  */
					n3 = n + strlen(s + n);
					while (s[n3 - 1] <= ' ')
						n3--; /*  strip whitespace at the end  */
					for (n2 = n + n1; n2 <= n3; n2++) {
						if (s[n2] == '(') {
							/*  '(' is already present  */
							break;
						} else if (s[n2] > ' ' || n2 == n3) {
							/*  No '()': we need to insert them  */
							memmove(s + n2 + 1, s + n2, n3 - n2);
							s[n2] = '(';
							s[n3 + 1] = ')';
							s[n3 + 2] = 0;
							break;
						}
					}
					strncpy(s + n - 4, "sys.", 4);
					n -= 4;
				}
			}
			lm_runmode = LM_BATCH_RUNMODE;
			n = (luaL_loadbuffer(gL, s + n, strlen(s + n), "S")
				|| lua_pcall(gL, 0, LUA_MULTRET, 0));
			lm_runmode = (n == 0 ? LM_COMMAND_RUNMODE : LM_LUAERROR_RUNMODE);
		}
	}
	return 0;
}
