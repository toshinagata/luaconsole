/*
 *  graph.h
 *  LuaConsole
 *
 *  Created by 永田 央 on 16/01/09.
 *  Copyright 2016 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef __GRAPH_H__
#define __GRAPH_H__

#include "luacon.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int max_number_of_paths;

extern int l_gcolor(lua_State *L);
extern int l_fillcolor(lua_State *L);
extern int l_line(lua_State *L);
extern int l_path_line(lua_State *L);
extern int l_circle(lua_State *L);
extern int l_path_circle(lua_State *L);
extern int l_arc(lua_State *L);
extern int l_path_arc(lua_State *L);
extern int l_fan(lua_State *L);
extern int l_path_fan(lua_State *L);
extern int l_poly(lua_State *L);
extern int l_path_poly(lua_State *L);
extern int l_box(lua_State *L);
extern int l_path_box(lua_State *L);
extern int l_rbox(lua_State *L);
extern int l_path_rbox(lua_State *L);
extern int l_path_cubic(lua_State *L);

extern int l_create_path(lua_State *L);
extern int l_destroy_path(lua_State *L);
extern int l_close_path(lua_State *L);
extern int l_draw_path(lua_State *L);

extern int l_gcls(lua_State *L);
extern int l_gmode(lua_State *L);
extern int l_patdef(lua_State *L);
extern int l_patundef(lua_State *L);
extern int l_patdraw(lua_State *L);
extern int l_paterase(lua_State *L);
extern int l_rgb(lua_State *L);

extern int lm_gmode_platform(int gmode);
extern int lm_gmode_common(int gmode);

extern int lm_create_path(void);
extern int lm_destroy_path(int idx);
extern int lm_path_moveto(int idx, float x, float y);
extern int lm_path_lineto(int idx, float x, float y);
extern int lm_path_arc(int idx, float cx, float cy, float st, float et, float ra, float rb, float rot);
extern int lm_path_cubic(int idx, float x1, float y1, float x2, float y2, float x3, float y3);
extern int lm_path_close(int idx);
extern int lm_path_line(int idx, float x1, float y1, float x2, float y2);
extern int lm_path_rect(int idx, float x, float y, float width, float height);
extern int lm_path_roundrect(int idx, float x, float y, float width, float height, float rx, float ry);
extern int lm_path_ellipse(int idx, float x, float y, float rx, float ry);
extern int lm_path_fan(int idx, float cx, float cy, float st, float et, float ra, float rb, float rot);
extern int lm_set_stroke_color(pixel_t col, int enable);
extern int lm_set_fill_color(pixel_t col, int enable);
extern int lm_draw_path(int idx);

#ifdef __cplusplus
}
#endif
		
#endif /* __GRAPH_H__ */
