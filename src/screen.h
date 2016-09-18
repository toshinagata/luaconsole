/*
 *  screen.h
 *  LuaConsole
 *
 *  Created by 永田 央 on 16/01/09.
 *  Copyright 2016 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef __SCREEN_H__
#define __SCREEN_H__

#include "luacon.h"

#if __RASPBERRY_PI__ && __BAREMETAL__
#include <fb.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
	
/*  UTF16 to Font Index conversion table  */
/*  Font Index <-> EUC-JIS-2004  */
/*  0-93 <-> 21-7E (94) */
/*  94-187 <-> 8E/A1-FE (94, hankaku kana)  */
/*  188-9023 <-> A1-FE/A1-FE (94*94)  */
/*  9024-10433 <-> 8F/A1-AF/A1-FE (15*94)  */
/*  10434-12031 <-> 8F/EE-FE/A1-FE (17*94)  */
extern u_int16_t gConvTable[65536];

/*  Font data for hankaku characters (index 0-187)  */
extern u_int8_t gFontData[16*188];
/*  Font data for zenkaku characters (index 188-12031)  */
extern u_int8_t gKanjiData[32*11844];

/*  Lock/unlock shared memory
 (should be unnecessary if we are running in a single-thread mode)  */
extern void lm_lock(void);
extern void lm_unlock(void);

/*  Select active buffer for offscreen drawing  */
/*  TEXT_ACTIVE: text, GRAPHIC_ACTIVE: graphic  */
extern int lm_select_active_buffer(int active);

enum {
	TEXT_ACTIVE = 0,
	GRAPHIC_ACTIVE,
	PHYSICAL_ACTIVE  /*  Used internally  */
};

/*  Console output  */
enum {
	LM_CONSOLE_GRAPH,
	LM_CONSOLE_TTY
};

extern int16_t my_console;

/*  Screen size  */
extern int16_t my_width, my_height;

/*  Screen size at gmode 0 */
extern int16_t my_width_gmode0, my_height_gmode0;

/*  Screen size in text unit  */
extern int16_t my_max_x, my_max_y;

/*  Tab base and width  */
extern int16_t my_tab_base, my_tab_width;
	
/*  Internal bitmap buffer  */
/*extern pixel_t *my_textscreen;
extern pixel_t *my_graphscreen;
extern pixel_t *my_composedscreen; */

/*  Bitmap buffer for user-defined patterns (to avoid excessive malloc() calls)  */
extern pixel_t *my_patbuffer;
extern u_int32_t my_patbuffer_size;
extern u_int32_t my_patbuffer_count;

/*  Text cursor  */
extern int16_t my_cursor_x, my_cursor_y;
extern int16_t my_cursor_xofs, my_cursor_yofs;
	
/*  Show cursor?  */
extern u_int8_t my_show_cursor;

/*  Exchange the destination buffer, i.e. text drawing to graphic buffer,
 and graphic drawing to text buffer  */
/* extern u_int8_t my_exchange_buffer; */

/*  Graphic mode  */
extern int16_t my_graphic_mode;

/*  Origin of the visible part of the screen (expansion mode)  */
extern int16_t my_origin_x, my_origin_y;

/*  User-defined patterns  */
typedef struct LPattern {
	u_int16_t width, height;   /*  Size  */
	int32_t ofs;      /*  Offset in my_patbuffer  */
} LPattern;

#define NUMBER_OF_PATTERNS 256

extern LPattern my_patterns[NUMBER_OF_PATTERNS];

/*  Graphic buffer handler  */
extern void lm_redraw(int16_t x, int16_t y, int16_t width, int16_t height);

/*  Read font data   */
extern int lm_read_fontdata(const char *path);

/*  Show/hide cursor  */
extern void lm_show_cursor(int flag);

/*  Key input  */
extern u_int16_t lm_getch(int wait);
extern int lm_getline(char *buf, int size);

/*  OS dependent input function  */
extern int lm_getch_with_timeout(int32_t usec);

/*  Code conversion  */
extern u_int16_t lm_utf8_to_utf16(const char *s, char **outpos);
extern char *lm_utf16_to_utf8(u_int16_t uc, char *s);

extern int lm_character_width(u_int16_t uc);

extern void lm_scroll(int16_t bufindex, int16_t x, int16_t y, int16_t width, int16_t height, int16_t dx, int16_t dy);
extern void lm_erase_to_eol(void);
extern void lm_cls(void);
extern int lm_locate(int x, int y);
extern pixel_t lm_tcolor(pixel_t col);
extern pixel_t lm_bgcolor(pixel_t col);
extern void lm_puts(const char *s);
extern void lm_puts_format(const char *fmt, ...);

/*  Drawing primitives  */
extern void lm_clear_box(int x, int y, int width, int height);
extern void lm_put_pattern(const pixel_t *p, int x, int y, int width, int height);
extern void lm_get_pattern(pixel_t *p, int x, int y, int width, int height);
extern void lm_copy_pixels(int dx, int dy, int sx, int sy, int width, int height);
extern void lm_update_screen(void);
	
/*  Debug message  */
extern int tty_puts(const char *s);

extern volatile u_int8_t g_init_screen_done;

/*  Platform specific procedures  */
extern int lm_init_screen_platform(void);
extern void lm_draw_platform(void *ref);

/*  Platform specific drawing functions  */
extern void lm_write_pixels(const pixel_t *p, int x, int y, int width, int height);

extern int lm_register_graph_builtins(void);

extern int luaopen_con(lua_State *L);

/*  Base pointer of the source program area  */
extern u_int8_t *gSourceBasePtr;

/*  Size of the current source program  */
extern u_int32_t gSourceEnd;

/*  Size of the source program area  */
extern u_int32_t gSourceLimit;

/*  True if the on-memory program is modified  */
extern u_int8_t gSourceTouched;

#ifdef __cplusplus
}
#endif
		
#endif /* __GRAPH_H__ */
