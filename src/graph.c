/*
 *  graph.c
 *  LuaConsole
 *
 *  Created by 永田 央 on 16/01/09.
 *  Copyright 2016 __MyCompanyName__. All rights reserved.
 *
 */

#include "screen.h"
#include "graph.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <math.h>

#define LUA_AUTOINT(L, n) (lua_isinteger(L, n) ? lua_tointeger(L, n) : (int)floor(luaL_checknumber(L, n) + 0.5))

/*#define DIVINT(n, m) (n >= 0 ? \
	(m > 0 ? (n + m / 2) / m : -((n + (-m) / 2) / (-m))) :\
	(m > 0 ? -((-n + m / 2) / m) : (n + (-m) / 2)) / (-m))
*/

#define DIVINT(n, m) s_divint(n, m)

static int
l_push_path(lua_State *L)
{
	int idx;
	lm_select_active_buffer(GRAPHIC_ACTIVE);
	idx = lm_create_path();
	if (idx < 0)
		return luaL_error(L, "Cannot create path");
	lua_pushinteger(L, idx);
	lua_insert(L, 1);
	return idx;
}

int
l_gcolor(lua_State *L)
{
	int flag;
	pixel_t col = RGBFLOAT(1, 1, 1);
	if (lua_gettop(L) == 0 || lua_toboolean(L, 1) == 0)
		flag = 0;
	else {
		flag = -1;
		if (!lua_isboolean(L, 1)) {
			col = luaL_checkinteger(L, 1);
			flag = 1;
		}
	}
	lm_set_stroke_color(col, flag);
	return 0;
}

int
l_fillcolor(lua_State *L)
{
	int flag;
	pixel_t col = 0;
	if (lua_gettop(L) == 0 || lua_toboolean(L, 1) == 0)
		flag = 0;
	else {
		flag = -1;
		if (!lua_isboolean(L, 1)) {
			col = luaL_checkinteger(L, 1);
			flag = 1;
		}
	}
	lm_set_fill_color(col, flag);
	return 0;
}

int
l_path_line(lua_State *L)
{
	int idx = luaL_checkinteger(L, 1);
	float x1 = luaL_checknumber(L, 2);
	float y1 = luaL_checknumber(L, 3);
	float x2 = luaL_checknumber(L, 4);
	float y2 = luaL_checknumber(L, 5);
	if (lm_path_line(idx, x1, y1, x2, y2) != 0)
		return luaL_error(L, "Cannot add line segment to the path");
	return 0;
}

int
l_line(lua_State *L)
{
	int idx = l_push_path(L);
	l_path_line(L);
	lm_draw_path(idx);
	lm_destroy_path(idx);
	return 0;
}

int
l_path_circle(lua_State *L)
{
	int nargs = lua_gettop(L);
	int idx = luaL_checkinteger(L, 1);
	float ox = luaL_checknumber(L, 2);
	float oy = luaL_checknumber(L, 3);
	float rad1 = luaL_checknumber(L, 4);
	float rad2 = rad1;
	if (nargs >= 5)
		rad2 = lua_tonumber(L, 5);
	if (lm_path_ellipse(idx, ox, oy, rad1, rad2) != 0)
		return luaL_error(L, "Cannot add circle(ellipse) to the path");
	return 0;
}

int
l_circle(lua_State *L)
{
	int idx = l_push_path(L);
	l_path_circle(L);
	lm_draw_path(idx);
	lm_destroy_path(idx);
	return 0;
}

static int
s_path_arc(lua_State *L, int need_moveto)
{
	int nargs = lua_gettop(L);
	int idx = luaL_checkinteger(L, 1);
	float ox = luaL_checknumber(L, 2);
	float oy = luaL_checknumber(L, 3);
	float start = luaL_checknumber(L, 4);
	float end = luaL_checknumber(L, 5);
	float rad1 = LUA_AUTOINT(L, 6);
	float rad2 = rad1;
	float rot = 0.0;
	if (nargs >= 7)
		rad2 = luaL_checknumber(L, 7);
	if (nargs >= 8)
		rot = luaL_checknumber(L, 8);
	if (need_moveto) {
		float ang = start * (3.1415926536 / 180.0);
		float sn = sin(ang);
		float cs = cos(ang);
		float stx = rad1 * cs;
		float sty = rad2 * sn;
		ang = rot * (3.1415926536 / 180.0);
		sn = sin(ang);
		cs = cos(ang);
		if (lm_path_moveto(idx, ox + stx * cs - sty * sn, oy + stx * sn + sty * cs) != 0)
			goto arc_error;
	}
	if (lm_path_arc(idx, ox, oy, start, end, rad1, rad2, rot) == 0)
		return 0;
arc_error:
	return luaL_error(L, "Cannot add arc to the path");
}

int
l_path_arc(lua_State *L)
{
	return s_path_arc(L, 0);
}

int
l_arc(lua_State *L)
{
	int idx = l_push_path(L);
	s_path_arc(L, 1);
	lm_draw_path(idx);
	lm_destroy_path(idx);
	return 0;
}


int
l_path_fan(lua_State *L)
{
	int nargs = lua_gettop(L);
	int idx = luaL_checkinteger(L, 1);
	float ox = luaL_checknumber(L, 2);
	float oy = luaL_checknumber(L, 3);
	float start = luaL_checknumber(L, 4);
	float end = luaL_checknumber(L, 5);
	float rad1 = LUA_AUTOINT(L, 6);
	float rad2 = rad1;
	float rot = 0.0;
	if (nargs >= 7)
		rad2 = luaL_checknumber(L, 7);
	if (nargs >= 8)
		rot = luaL_checknumber(L, 8);
	if (lm_path_fan(idx, ox, oy, start, end, rad1, rad2, rot) != 0)
		return luaL_error(L, "Cannot add fan to the path");
	return 0;
}

int
l_fan(lua_State *L)
{
	int idx = l_push_path(L);
	l_path_fan(L);
	lm_draw_path(idx);
	lm_destroy_path(idx);
	return 0;
}

int
l_path_poly(lua_State *L)
{
	int i, idx, n;
	float x, y;
	idx = luaL_checkinteger(L, 1);
	if (lua_istable(L, 2)) {
		int len = lua_rawlen(L, 2);
		if (len % 2 != 0) {
			return luaL_error(L, "The y-coordinate of the last point is missing");
		}
		for (i = 1; i <= len; i += 2) {
			lua_rawgeti(L, 2, i);
			lua_rawgeti(L, 2, i + 1);
			x = luaL_checknumber(L, 3);
			y = luaL_checknumber(L, 4);
			lua_pop(L, 2);
			if (i == 1)
				n = lm_path_moveto(idx, x, y);
			else
				n = lm_path_lineto(idx, x, y);
			if (n != 0)
				return luaL_error(L, "Cannot add poly-line segment");
		}
		lm_path_close(idx);
	}
	return 0;
}

int
l_poly(lua_State *L)
{
	int idx = l_push_path(L);
	l_path_poly(L);
	lm_draw_path(idx);
	lm_destroy_path(idx);
	return 0;
}

int
l_path_box(lua_State *L)
{
	int idx = luaL_checkinteger(L, 1);
	float x1 = lua_tonumber(L, 2);
	float y1 = lua_tonumber(L, 3);
	float wid = lua_tonumber(L, 4);
	float hei = lua_tonumber(L, 5);
	if (lm_path_rect(idx, x1, y1, wid, hei) != 0)
		return luaL_error(L, "Cannot create box path");
	return 0;
}

int
l_box(lua_State *L)
{
	int idx = l_push_path(L);
	l_path_box(L);
	lm_draw_path(idx);
	lm_destroy_path(idx);
	return 0;
}

int
l_path_rbox(lua_State *L)
{
	int idx = luaL_checkinteger(L, 1);
	float x1 = lua_tonumber(L, 2);
	float y1 = lua_tonumber(L, 3);
	float wid = lua_tonumber(L, 4);
	float hei = lua_tonumber(L, 5);
	float rx = lua_tonumber(L, 6);
	float ry;
	if (lua_gettop(L) >= 7)
		ry = lua_tonumber(L, 7);
	else ry = rx;
	if (rx < 0)
		rx = 0;
	else if (rx > wid / 2)
		rx = wid / 2;
	if (ry < 0)
		ry = 0;
	else if (ry > hei / 2)
		ry = hei / 2;
	lm_path_roundrect(idx, x1, y1, wid, hei, rx, ry);
	return 0;
}

int
l_rbox(lua_State *L)
{
	int idx = l_push_path(L);
	l_path_rbox(L);
	lm_draw_path(idx);
	lm_destroy_path(idx);
	return 0;
}

int
l_path_cubic(lua_State *L)
{
	int idx = luaL_checkinteger(L, 1);
	float x1 = lua_tonumber(L, 2);
	float y1 = lua_tonumber(L, 3);
	float x2 = lua_tonumber(L, 4);
	float y2 = lua_tonumber(L, 5);
	float x3 = lua_tonumber(L, 6);
	float y3 = lua_tonumber(L, 7);
	if (lm_path_cubic(idx, x1, y1, x2, y2, x3, y3) != 0)
		return luaL_error(L, "Cannot add cubic curve segment");
	return 0;
}

int
l_create_path(lua_State *L)
{
	int idx;
	lm_select_active_buffer(GRAPHIC_ACTIVE);
	idx = lm_create_path();
	if (idx < 0)
		return luaL_error(L, "Cannot create path");
	lua_pushinteger(L, idx);
	return 1;	
}

int
l_destroy_path(lua_State *L)
{
	int idx = luaL_checkinteger(L, 1);
	if (lm_destroy_path(idx) != 0)
		return luaL_error(L, "Invalid path identifier");
	return 0;
}

int
l_close_path(lua_State *L)
{
	int idx = luaL_checkinteger(L, 1);
	if (lm_path_close(idx) != 0)
		return luaL_error(L, "Cannot close path");
	return 0;
}

int
l_draw_path(lua_State *L)
{
	int idx = luaL_checkinteger(L, 1);
	if (lm_draw_path(idx) != 0)
		return luaL_error(L, "Cannot draw path");
	return 0;
}

int
l_gcls(lua_State *L)
{
	lm_select_active_buffer(GRAPHIC_ACTIVE);
	lm_clear_box(0, 0, my_width, my_height);
	return 0;
}

int
lm_gmode_common(int mode)
{
	int n = lm_gmode_platform(mode);
	if (n < 0)
		return n;
	my_max_x = my_width / 8;
	my_max_y = my_height / 16;
	if (my_cursor_x >= my_max_x) {
		my_cursor_y++;
		my_cursor_x = 0;
	}
	if (my_cursor_y >+ my_max_y)
		my_cursor_y = my_max_y - 1;
	my_cursor_yofs = my_height / 16 - my_max_y;
	my_graphic_mode = mode;
	lm_redraw(0, 0, my_width, my_height);
	return n;
}

/*  gmode(0): normal, gmode(1): expanded screen  */
int
l_gmode(lua_State *L)
{
	int n;
	int mode = LUA_AUTOINT(L, 1);
	if (my_graphic_mode == mode)
		return 0;  /*  Do nothing  */
	n = lm_gmode_common(mode);
	if (n < 0)
		return luaL_error(L, "Cannot set gmode %d: error code = %d", mode, n);
	return 0;
}

/*  patdef(num, width, height, table)
    Define a pattern #num (num in 1..256). Table is an array of color codes (column first).  */
int
l_patdef(lua_State *L)
{
	int width, height, len, i, num, len2;
	pixel_t *p;
	num = luaL_checkinteger(L, 1);
	width = luaL_checkinteger(L, 2);
	height = luaL_checkinteger(L, 3);
	luaL_argcheck(L, num >= 1 && num <= 256, 1, "must be in 1..256");
	luaL_argcheck(L, width > 0, 2, "must be a positive integer");
	luaL_argcheck(L, height > 0, 3, "must be a positive integer");
	luaL_argcheck(L, lua_istable(L, 4), 4, "table expected");
	len = width * height;
	num--;
	if (my_patbuffer_count + len > my_patbuffer_size)
		return luaL_error(L, "out of pattern memory; pattern size (%dx%d) is too large", width, height);

	len2 = my_patterns[num].width * my_patterns[num].height;
	if (len2 == 0) {
		/*  New entry  */
		if (num == 0)
			my_patterns[num].ofs = 0;
		else {
			LPattern *lp = my_patterns + num - 1;
			(lp + 1)->ofs = lp->ofs + lp->width * lp->height;
		}
	}		
	/*  Move the already existing data  */
	p = my_patbuffer + my_patterns[num].ofs;
	
	memmove(my_patbuffer + len, my_patbuffer + len2, sizeof(u_int16_t) * (my_patbuffer_size - (len > len2 ? len : len2)));
	for (i = num + 1; i < NUMBER_OF_PATTERNS; i++)
		my_patterns[i].ofs += len - len2;

	for (i = 0; i < len; i++) {
		lua_rawgeti(L, 4, i + 1);
		*p++ = luaL_checkinteger(L, -1);
		lua_pop(L, 1);
	}
	my_patterns[num].width = width;
	my_patterns[num].height = height;
	
	return 0;
}

/*  patundef(num)
  Forget the pre-defined pattern #num.  */
int
l_patundef(lua_State *L)
{
	int i, len2;
/*	pixel_t *p; */
	int num = luaL_checkinteger(L, 1);
	luaL_argcheck(L, num >= 1 && num <= 256, 1, "must be in 1..256");
	num--;
	/*  Move the already existing data  */
/*	p = my_patbuffer + my_patterns[num].ofs;  */
	len2 = my_patterns[num].width * my_patterns[num].height;
	memmove(my_patbuffer, my_patbuffer + len2, sizeof(pixel_t) * (my_patbuffer_size - len2));
	for (i = num + 1; i < NUMBER_OF_PATTERNS; i++)
		my_patterns[i].ofs -= len2;
	my_patterns[num].width = my_patterns[num].height = my_patterns[num].ofs = 0;
	return 0;
}

static void
s_patwrite(int n, int x1, int y1, u_int32_t mask, int eraseflag)
{
	int x2, y2;
	int dx, dy, wx, wy, ww;
	int x, y;
/*	int r, g, b;  */
	pixel_t *p, *p1, *p2;
	int need_free = 0;
	
	wx = my_patterns[n].width;
	wy = my_patterns[n].height;
	x2 = x1 + wx;
	y2 = y1 + wy;
	if (x2 < 0 || x1 >= my_width || y2 < 0 || y1 >= my_height)
		return;
	dx = dy = ww = 0;
	if (x1 < 0) {
		dx = -x1;
		wx -= dx;
		x1 = 0;
	}
	if (y1 < 0) {
		dy = -y1;
		wy -= dy;
		y1 = 0;
	}
	if (x2 > my_width) {
		x2 = my_width;
		wx = x2 - x1;
	}
	if (y2 > my_height) {
		y2 = my_height;
		wy = y2 - y1;
	}
	ww = my_patterns[n].width - wx;
	
	p = (pixel_t *)alloca(sizeof(pixel_t) * wx * wy);
	if (p == NULL) {
		p = (pixel_t *)malloc(sizeof(pixel_t) * wx * wy);
		need_free = 1;
	}
	
	lm_select_active_buffer(GRAPHIC_ACTIVE);
	lm_get_pattern(p, x1, y1, wx, wy);
	p1 = p;
	p2 = my_patbuffer + my_patterns[n].ofs + dy * my_patterns[n].width + dx;
	if (eraseflag == 0) {
		for (y = y1; y < y2; y++) {
			for (x = x1; x < x2; x++) {
				u_int32_t col = *p2;
				if (col != 0) {
/*
#if BYTES_PER_PIXEL == 2
					r = (((col >> 11) & 0x1f) * ((mask >> 11) & 0x1f)) / 0x1f;
					g = (((col >> 5) & 0x3f) * ((mask >> 5) & 0x3f)) / 0x3f;
					b = ((col & 0x1f) * (mask & 0x1f)) / 0x1f;
#else
					r = (((col >> 16) & 0xff) * ((mask >> 16) & 0xff)) / 0xff;
					g = (((col >> 8) & 0xff) * ((mask >> 8) & 0xff)) / 0xff;
					b = ((col & 0xff) * (mask & 0xff)) / 0xff;
#endif
*/
					*p1 = col;
				}
				p1++;
				p2++;
			}
			p2 += ww;
		}	
	} else {
		for (y = y1; y < y2; y++) {
			for (x = x1; x < x2; x++) {
				if (*p2 != 0)
					*p1 = 0;
				p1++;
				p2++;
			}
			p2 += ww;
		}	
	}
	lm_put_pattern(p, x1, y1, wx, wy);
	if (need_free)
		free(p);
}

/*  patdraw(n, x, y [, mask])
    Draw the defined pattern (#n) at position (x, y).
    Mask is a color mask, where the original RGB components are attenuated by this RGB values. */
int
l_patdraw(lua_State *L)
{
	int n, x1, y1;
	u_int32_t mask;
	n = luaL_checkinteger(L, 1);
	if (n <= 0 || n > NUMBER_OF_PATTERNS)
		return luaL_argerror(L, 1, "pattern number must be in (1..256)");
	n--;
	if (my_patterns[n].width == 0)
		return luaL_error(L, "pattern (%d) is undefined", n + 1);
	x1 = luaL_checkinteger(L, 2);
	y1 = luaL_checkinteger(L, 3);
	if (lua_gettop(L) >= 4)
		mask = luaL_checkinteger(L, 4);
	else mask = (BYTES_PER_PIXEL == 2 ? 0xffff : 0xffffffff);
	s_patwrite(n, x1, y1, mask, 0);
	return 0;
}

/*  paterase(n, x, y)
 Erase the defined pattern (#n) at position (x, y).  */
int
l_paterase(lua_State *L)
{
	int n, x1, y1;
	n = luaL_checkinteger(L, 1);
	if (n <= 0 || n > NUMBER_OF_PATTERNS)
		return luaL_argerror(L, 1, "pattern number must be in (1..256)");
	n--;
	if (my_patterns[n].width == 0)
		return luaL_error(L, "pattern (%d) is undefined", n + 1);
	x1 = luaL_checkinteger(L, 2);
	y1 = luaL_checkinteger(L, 3);
	s_patwrite(n, x1, y1, 0, 1);
	return 0;
}

/*  rgb(r, g, b[, a])
 Return a color code. The arguments are floating number (0.0 to 1.0).  */
int
l_rgb(lua_State *L)
{
	int n;
	double r, g, b, a;
	n = lua_gettop(L);
	if (n >= 4)
		a = luaL_checknumber(L, 4);
	else a = 1.0;
	r = luaL_checknumber(L, 1);
	g = luaL_checknumber(L, 2);
	b = luaL_checknumber(L, 3);
	lua_pushinteger(L, RGBAFLOAT(r, g, b, a));
	return 1;
}

