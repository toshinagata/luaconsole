/*
 *  luacon.h
 *  LuaConsole
 *
 *  Created by 永田 央 on 16/08/20.
 *  Copyright 2016 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef __LUACON_H__
#define __LUACON_H__

#include <sys/types.h>

#if defined(_WIN32)
#include <stdint.h>
typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
#endif

#if defined(_WIN32)
#include <malloc.h>
#else
#include <alloca.h>
#endif

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#ifndef BYTES_PER_PIXEL
#define BYTES_PER_PIXEL 4
#endif

#if BYTES_PER_PIXEL == 2
typedef unsigned short  pixel_t;
#define RGBAFLOAT(rf, gf, bf, af) \
((((pixel_t)((rf)*31+0.5)) << 11) | \
(((pixel_t)((gf)*63+0.5)) << 5) | \
((pixel_t)((bf)*31+0.5)))
#define REDCOMPONENT(pix) ((((pix) >> 11) & 31) / 31.0)
#define GREENCOMPONENT(pix) ((((pix) >> 5) & 63) / 63.0)
#define BLUECOMPONENT(pix) (((pix) & 31) / 31.0)
#define ALPHACOMPONENT(pix) (1.0)
#else
typedef unsigned long   pixel_t;
#if __RASPBERRY_PI__
#define RGBAFLOAT(rf, gf, bf, af) \
((((pixel_t)((rf)*255+0.5)) << 24) | \
(((pixel_t)((gf)*255+0.5)) << 16) | \
(((pixel_t)((bf)*255+0.5)) << 8) | \
((pixel_t)((af)*255+0.5)))
#define REDCOMPONENT(pix) ((((pix) >> 24) & 255) / 255.0)
#define GREENCOMPONENT(pix) ((((pix) >> 16) & 255) / 255.0)
#define BLUECOMPONENT(pix) ((((pix) >> 8) & 255) / 255.0)
#define ALPHACOMPONENT(pix) (((pix) & 255) / 255.0)
#else
#define RGBAFLOAT(rf, gf, bf, af) \
((((pixel_t)((af)*255+0.5)) << 24) | \
(((pixel_t)((bf)*255+0.5)) << 16) | \
(((pixel_t)((gf)*255+0.5)) << 8) | \
((pixel_t)((rf)*255+0.5)))
#define REDCOMPONENT(pix) (((pix) & 255) / 255.0)
#define GREENCOMPONENT(pix) ((((pix) >> 8) & 255) / 255.0)
#define BLUECOMPONENT(pix) ((((pix) >> 16) & 255) / 255.0)
#define ALPHACOMPONENT(pix) ((((pix) >> 24) & 255) / 255.0)
#endif
#endif

#define RGBFLOAT(rf, gf, bf) RGBAFLOAT(rf, gf, bf, 1.0)

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
extern int asprintf(char **ret, const char *fmt, ...);
#endif

/*  Lua state machine  */
extern lua_State *gL;

/*  Run mode: LM_COMMAND_RUNMODE, command mode; LM_EDIT_RUNMODE, edit mode; LM_APP_RUNMODE, app mode  */
enum {
	LM_AUTORUN_RUNMODE = 1,
	LM_COMMAND_RUNMODE,
	LM_BATCH_RUNMODE,
	LM_LUAERROR_RUNMODE,
	LM_EDIT_RUNMODE,
	LM_APP_RUNMODE,
	LM_READLINE_RUNMODE,
	LM_EXIT_RUNMODE,
	LM_HALT_RUNMODE
};

extern u_int8_t lm_runmode;

extern u_int32_t lm_tick;

extern char *lm_base_dir;
extern char *lm_filename;
	
#define LM_BUFSIZE 1024

extern char lm_readline_buf[LM_BUFSIZE];

/*  Base pointer of the source program area  */
extern u_int8_t *gSourceBasePtr;

/*  Size of the current source program  */
extern u_int32_t gSourceEnd;

/*  Size of the source program area  */
extern u_int32_t gSourceLimit;

/*  True if the on-memory program is modified  */
extern u_int8_t gSourceTouched;

extern int lua_setup(void);
extern int lua_loop(void);
extern int lua_setup_platform(lua_State *L);
	
extern int lm_interrupt(lua_State *L);
extern const char *lm_platform_name(void);
	
#ifdef __cplusplus
}
#endif
		
#endif /* __GRAPH_H__ */
