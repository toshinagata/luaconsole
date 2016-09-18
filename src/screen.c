/*
 *  screen.c
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

/*  Text output destination  */
int16_t my_console;

/*  Screen size  */
int16_t my_width, my_height;

/*  Screen size at gmode 0 */
int16_t my_width_gmode0, my_height_gmode0;

/*  Internal bitmap buffer  */
pixel_t *my_textscreen;
pixel_t *my_graphscreen;
pixel_t *my_composedscreen;

/*  Text cursor  */
int16_t my_cursor_x, my_cursor_y;
int16_t my_cursor_xofs, my_cursor_yofs;

/*  Show cursor?  */
u_int8_t my_show_cursor;

/*  Exchange the destination buffer, i.e. text drawing to graphic buffer,
    and graphic drawing to text buffer  */
/* u_int8_t my_exchange_buffer; */

int16_t my_max_x, my_max_y;

/*  Tab base and width  */
int16_t my_tab_base, my_tab_width;

/*  Graphic mode  */
/*  0: normal, 1: 2x2 expansion  */
int16_t my_graphic_mode;

/*  Origin of the visible part of the screen (expansion mode)  */
int16_t my_origin_x, my_origin_y;

/*  Graphic patterns  */
pixel_t *my_patbuffer;
u_int32_t my_patbuffer_size;
u_int32_t my_patbuffer_count;
LPattern my_patterns[NUMBER_OF_PATTERNS];

static pixel_t s_tcolor = RGBFLOAT(1, 1, 1);
static pixel_t s_bgcolor = 0;

static pixel_t s_palette[16];
static pixel_t s_initial_palette[16] = {
	0, RGBFLOAT(0, 0, 1), RGBFLOAT(0, 1, 0), RGBFLOAT(0, 1, 1),
	RGBFLOAT(1, 0, 0), RGBFLOAT(1, 0, 1), RGBFLOAT(1, 1, 0), RGBFLOAT(1, 1, 1),
	RGBFLOAT(0.5, 0.5, 0.5), RGBFLOAT(0, 0, 0.67), RGBFLOAT(0, 0.67, 0), RGBFLOAT(0, 0.67, 0.67),
	RGBFLOAT(0.67, 0, 0), RGBFLOAT(0.67, 0, 0.67), RGBFLOAT(0.67, 0.67, 0), RGBFLOAT(0.67, 0.67, 0.67)
};

#if !__CONSOLE__
u_int16_t gConvTable[65536];
u_int8_t gFontData[16*188];
u_int8_t gKanjiData[32*11844];
#endif

/*  Read character data  */
int
lm_read_fontdata(const char *path)
{
#ifndef __CONSOLE__
	FILE *fp = fopen(path, "r");
	u_int8_t buf[4];
	int uc;
	int convsize, fontsize, kanjisize;
	
	if (fp == NULL) {
		fprintf(stderr, "Cannot read font data %s\n", path);
		return 1;
	}
	
	fread(buf, 1, 4, fp);
	convsize = (u_int32_t)buf[0] + 256 * ((u_int32_t)buf[1] + 256 * ((u_int32_t)buf[2] + 256 * (u_int32_t)buf[3]));	
	fread(buf, 1, 4, fp);
	fontsize = (u_int32_t)buf[0] + 256 * ((u_int32_t)buf[1] + 256 * ((u_int32_t)buf[2] + 256 * (u_int32_t)buf[3]));	
	fread(buf, 1, 4, fp);
	kanjisize = (u_int32_t)buf[0] + 256 * ((u_int32_t)buf[1] + 256 * ((u_int32_t)buf[2] + 256 * (u_int32_t)buf[3]));	
	
	fread(gConvTable, 2, convsize, fp);
	for (uc = 0; uc < 65536; uc++) {
		/*  Convert from little endian  */
		u_int16_t si;
		si = ((u_int8_t *)&gConvTable[uc])[0];
		si += 256 * (u_int16_t)((u_int8_t *)&gConvTable[uc])[1];
	}
	fread(gFontData, 1, fontsize, fp);
	fread(gKanjiData, 1, kanjisize, fp);
	
	fclose(fp);
#endif  /*  __CONSOLE__  */
	return 0;
}

/*  Scroll the buffer. bufindex = 0: text buffer, 1: graphic buffer  */
/*  The y coordinates are graphic coordinates (i.e. up is positive)  */
void
lm_scroll(int16_t bufindex, int16_t x, int16_t y, int16_t width, int16_t height, int16_t dx, int16_t dy)
{
	if (my_console == LM_CONSOLE_TTY) {
		int cx, cy, y1, y2;
		char buf[16];
		if (bufindex != 0)
			return;
		cx = my_cursor_x;
		cy = my_cursor_y;
		/*  Only support vertical scrolling  */
		/*  Set scroll area  */
		y1 = (my_height - y - height) / 16 + 1 - my_cursor_yofs;
		y2 = y1 + height / 16 - 1;
		snprintf(buf, sizeof buf, "\x1b[%d;%dr", y1, y2);
		tty_puts(buf);
		dy = dy / 16;
		if (dy < 0) {
			/*  Down scroll  */
			dy = -dy;
			lm_locate(0, 0);
			while (--dy >= 0)
				tty_puts("\x1bM");
		} else if (dy > 0) {
			/*  Up scroll  */
			lm_locate(0, (y + height) / 16 - 1);
			while (--dy >= 0)
				tty_puts("\x1b" "D");
		}
		/*  Reset scroll area  */
		snprintf(buf, sizeof buf, "\x1b[%d;%dr", 1, my_height / 16);
		tty_puts(buf);
		lm_locate(cx, cy);
		return;
	}
#if !__CONSOLE__
	{
		int x1, y1, w1, h1;
		lm_select_active_buffer(TEXT_ACTIVE);
		if (dx > 0) {
			w1 = width - dx;
			x1 = x;
		} else {
			w1 = width + dx;
			x1 = x + dx;
		}
		if (dy > 0) {
			h1 = height - dy;
			y1 = y;
		} else {
			h1 = height + dy;
			y1 = y - dy;
		}
		if (w1 > 0 && h1 > 0) {
			lm_copy_pixels(x1 + dx, y1 + dy, x1, y1, w1, h1);
		}
		if (dx > 0) {
			lm_clear_box(x, y, dx, height);
		} else if (dx < 0) {
			lm_clear_box(x + w1, y, -dx, height);
		}
		if (dy > 0) {
			lm_clear_box(x, y, width, dy);
		} else if (dy < 0) {
			lm_clear_box(x, y + h1, width, -dy);
		}
	}
	
#endif /* !__CONSOLE__ */
}

static void
s_pb_putonechar(int16_t x, int16_t y, u_int16_t ch)
{
#if !__CONSOLE__
	u_int8_t *f;
	static pixel_t buf[256];
	pixel_t *p = buf;
	u_int32_t idx = gConvTable[ch];
	int i, j, width = 8;
	if (ch == 32) {
		for (i = 0; i < 16; i++) {
			for (j = 0; j < 8; j++) {
				*p++ = s_bgcolor;
			}
		}
	} else if (idx == 0xffff)
		return;
	else if (idx < 188) {
		f = gFontData + idx * 16;
		for (i = 0; i < 16; i++) {
			for (j = 0; j < 8; j++) {
				u_int8_t b = (*f & (1 << j));
				*p++ = (b != 0 ? s_tcolor : s_bgcolor);
			}
			f++;
		}
	} else if (idx < 12032) {
		f = gKanjiData + (idx - 188) * 32;
		for (i = 0; i < 16; i++) {
			u_int16_t u = *f++;
			u += (u_int16_t)*f++ * 256;
			for (j = 0; j < 16; j++) {
				u_int16_t b = (u & (1 << j));
				*p++ = (b != 0 ? s_tcolor : s_bgcolor);
			}
		}
		width = 16;
	}
	lm_put_pattern(buf, x, y, width, 16);
#endif  /* !__CONSOLE__ */
}

/*  Erase to end of line  */
void
lm_erase_to_eol(void)
{
	if (my_console == LM_CONSOLE_TTY) {
		tty_puts("\x1b[K");
		return;
	}
#if !__CONSOLE__
	{
	/*	int width = (my_max_x - my_cursor_x) * 8 * sizeof(pixel_t); */
		lm_select_active_buffer(TEXT_ACTIVE);
		lm_clear_box((my_cursor_xofs + my_cursor_x) * 8, my_height - (my_cursor_yofs + my_cursor_y + 1) * 16, my_width - my_cursor_x * 8, 16);
	}
#endif
}

/*  Clear screen  */
void
lm_cls(void)
{
	if (my_console == LM_CONSOLE_TTY) {
		tty_puts("\x1b[2J");
		return;
	}
#if !__CONSOLE__
	lm_select_active_buffer(TEXT_ACTIVE);
	lm_clear_box(0, 0, my_width, my_height);
#endif
}

/*  Character width  */
int
lm_character_width(u_int16_t uc)
{
	if (uc < 0x80 || (uc >= 0xff60 && uc <= 0xff9f))
		return 1;
	else return 2;
}

u_int16_t
lm_utf8_to_utf16(const char *s, char **outpos)
{
	u_int8_t c;
	u_int16_t uc;
	c = *s++;
	if (c < 0x80) {
		uc = c;
	} else if (c >= 0xc2 && c <= 0xdf) {
		uc = (((u_int16_t)c & 0x1f) << 6) + (((u_int16_t)*s++) & 0x3f);
	} else if (c >= 0xe0 && c <= 0xef) {
		uc = (((u_int16_t)c & 0x0f) << 12) + ((((u_int16_t)*s++) & 0x3f) << 6);
		uc += ((u_int16_t)*s++) & 0x3f;
	} else uc = 32;  /*  Out of range  */
	if (outpos != NULL)
		*outpos = (char *)s;
	return uc;
}

/*  The output buffer s[] must have at least three bytes space  */
char *
lm_utf16_to_utf8(u_int16_t uc, char *s)
{
	if (uc < 0x80) {
		*s++ = uc;
	} else if (uc < 0x0800) {
		*s++ = ((uc >> 6) & 31) | 0xc0;
		*s++ = (uc & 63) | 0x80;
	} else {
		*s++ = ((uc >> 12) & 15) | 0xe0;
		*s++ = ((uc >> 6) & 63) | 0x80;
		*s++ = (uc & 63) | 0x80;
	}
	*s = 0;
	return s;
}

/*  Draw a UTF-8 or UTF-16 string at the current cursor position  */
void
lm_puts_utf8or16(const char *s, int is_utf16)
{
	u_int16_t uc;
	u_int16_t *us = (u_int16_t *)s;
	int dx, cx, cy;
	int cx1, cx2, cy1, cy2;
	if (s == NULL)
		return;
	cx = my_cursor_x;
	cy = my_cursor_y;
	cx1 = cx2 = cx;
	cy1 = cy2 = cy;
	lm_select_active_buffer(TEXT_ACTIVE);
	while (1) {
		if (is_utf16)
			uc = *us++;
		else
			uc = lm_utf8_to_utf16(s, (char **)&s);
		if (uc == 0)
			break;
		if (uc == '\n') {
			cx = 0;
			cy++;
			if (cy == my_max_y) {
				/*  Scroll up  */
				lm_scroll(0, 0, 0, my_max_x * 8, my_max_y * 16, 0, 16);
				cy--;
			}
			dx = 0;
			goto cont;
		} else if (uc == '\t') {
			dx = (cx + my_tab_width - my_tab_base) / my_tab_width * my_tab_width + my_tab_base - cx;
			cx += dx;
			if (cx >= my_max_x) {
				cx = 0;
				cy++;
				if (cy == my_max_y) {
					lm_scroll(0, 0, 0, my_max_x * 8, my_max_y * 16, 0, 16);
					cy--;
				}
			}
			goto cont;
		}
		dx = lm_character_width(uc);
		if (dx == 2 && cx == my_max_x - 1) {
			cx = 0;
			cy++;
			if (cy == my_max_y) {
				lm_scroll(0, 0, 0, my_max_x * 8, my_max_y * 16, 0, 16);
				cy--;
			}
		}
		if (my_console == LM_CONSOLE_TTY) {
			char utf8[8];
			lm_utf16_to_utf8(uc, utf8);
			tty_puts(utf8);
		} else s_pb_putonechar((cx + my_cursor_xofs) * 8, my_height - (cy + my_cursor_yofs + 1) * 16, uc);
		cx += dx;
		if (cx < cx1)
			cx1 = cx;
		if (cx > cx2)
			cx2 = cx;
		if (cy < cy1)
			cy1 = cy;
		if (cy > cy2)
			cy2 = cy;
		if (cx >= my_max_x) {
			cx = 0;
			cy++;
			if (cy == my_max_y) {
				lm_scroll(0, 0, 0, my_max_x * 8, my_max_y * 16, 0, 16);
				cy--;
			}
		}
	cont:
		if (dx >= 2 || cx == 0) {
			/*  relocate cursor  */
			lm_locate(cx, cy);
			if (my_console == LM_CONSOLE_TTY)
				usleep(1000);
		}
	}
	my_cursor_x = cx;
	my_cursor_y = cy;
#if !__CONSOLE__
	if (my_console != LM_CONSOLE_TTY)
		lm_redraw((cx1 + my_cursor_xofs) * 8, my_height - (cy2 + my_cursor_yofs + 1) * 16, (cx2 - cx1) * 8, (cy2 - cy1 + 1) * 16);
#endif
}

void
lm_puts(const char *s)
{
	lm_puts_utf8or16(s, 0);
}

void
lm_puts_format(const char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	lm_puts(buf);
}

void
lm_puts_utf16(const u_int16_t *s)
{
	lm_puts_utf8or16((const char *)s, 1);
}

void
lm_show_cursor(int flag)
{
	my_show_cursor = flag;
#if !__CONSOLE__
	lm_redraw((my_cursor_x + my_cursor_xofs) * 8, my_height - (my_cursor_y + my_cursor_yofs + 1) * 16, 8, 16);
#endif
}

int
lm_getline(char *buf, int size)
{
	u_int16_t *ubuf = (u_int16_t *)malloc(sizeof(u_int16_t) * size);
	int usize = size;
	u_int16_t uc;
	char *s;
	int cx, cy, pt, len;
	int i, cx1, cy1, cx2, cy2, dx;
	memset(ubuf, 0, size * 2);
	cx = my_cursor_x;
	cy = my_cursor_y;
	pt = len = 0;
	while ((uc = lm_getch(1)) != 13) {
		if (uc == 3) {
			free(ubuf);
			return -1;  /*  Interrupt  */
		}
		if (uc == 30 || uc == 1) {
			pt = 0;
		} else if (uc == 31 || uc == 5) {
			pt = len;
		} else if (uc == 28 || uc == 6) {
			if (pt < len)
				pt++;
		} else if (uc == 29 || uc == 2) {
			if (pt > 0)
				pt--;
		} else if (uc == 8 || uc == 127) {
			if (pt > 0) {
				memmove(ubuf + pt - 1, ubuf + pt, (len - pt + 1) * sizeof(u_int16_t));
				pt--;
				len--;
				ubuf[len] = 0;
			}
		} else {
			if (len < usize) {
				memmove(ubuf + pt + 1, ubuf + pt, (len - pt + 1) * sizeof(u_int16_t));
				ubuf[pt] = uc;
				pt++;
				len++;
			}
		}
		lm_locate(cx, cy);
		lm_puts_utf16(ubuf);
		lm_puts(" ");  /*  The cursor position  */
		lm_erase_to_eol();
		cx1 = cx2 = cx;
		cy1 = cy2 = cy;
		for (i = 0; i <= len; i++) {
			if (i == len)
				dx = 1;
			else {
				dx = lm_character_width(ubuf[i]);
			}
			if (dx == 2 && cx1 == my_max_x - 1) {
				cx1 = 0;
				cy1++;
			}
			if (i == pt) {
				cx2 = cx1;
				cy2 = cy1;
			}
			cx1 += dx;
			if (cx1 >= my_max_x) {
				cx1 = 0;
				cy1++;
			}
		}
		if (cy1 > my_cursor_y) {
			/*  Up scroll by dy lines  */
			int dy = cy1 - my_cursor_y;
			cy -= dy;
			cy2 -= dy;
		}
		lm_locate(cx2, cy2);
	}
	
	s = buf;
	cx = 0;
	for (i = 0; i < len; i++) {
		s = lm_utf16_to_utf8(ubuf[i], s);
		if (s - buf > size - 4) {
			cx = 1;
			break;  /*  The string is truncated  */
		}
	}
	*s = 0;
	free(ubuf);
	lm_puts("\n");
	return s - buf;
}

/*  Get the key input.  */
/*  wait != 0: wait for input  */
u_int16_t
lm_getch(int wait)
{
	static u_int8_t s_buf[8];
	static int s_bufcount = 0, s_bufindex = 0;
	
	int c;
	int timer;
	int waiting_for_esc = 0;
	u_int32_t usec;

	if (my_console == LM_CONSOLE_TTY && wait == 1)
		tty_puts("\x1b[?25h");  /*  Show cursor (vt100)  */
	
	usec = (wait == 1 ? 50000 : 0);
	
	if (my_console != LM_CONSOLE_TTY)
		lm_show_cursor(1);

	if (s_bufcount == 0) {
		/*  Read from input  */
		s_bufindex = 0;
		timer = 0;
		while (1) {
			c = lm_getch_with_timeout(usec);
			if (c > 0) {
				if (my_console != LM_CONSOLE_TTY)
					lm_show_cursor(0);
				s_buf[s_bufcount++] = c;
				if (s_bufcount == 1 && c == 0x1b) {
					/*  Start ESC sequence  */
					waiting_for_esc = 1;
					timer = 0;
					continue;
				} else if (s_bufcount == 2 && c == '[') {
					/*  Start CSI sequence  */
					waiting_for_esc = 0;
					continue;
				} else if (s_bufcount == 3) {
					s_bufcount = 0;
					if (c == 'A')
						return 0x1e;
					else if (c == 'B')
						return 0x1f;
					else if (c == 'C')
						return 0x1c;
					else if (c == 'D')
						return 0x1d;
					s_bufcount = 3;
					break;
				}
				break;
			}
			if (wait != 1)
				break;
			if (waiting_for_esc && timer == 2)
				break;  /*  Stop waiting for the subsequent character and return 0x1b  */
			if (my_console != LM_CONSOLE_TTY) {
				if (timer % 20 == 0)
					lm_show_cursor(1);
				else if (timer % 20 == 10)
					lm_show_cursor(0);
			}
			if (++timer == 20)
				timer = 0;
		}
	}
	
	if (my_console == LM_CONSOLE_TTY && wait == 1)
		tty_puts("\x1b[?25l");  /*  Hide cursor (vt100)  */
	
	/*  Return the waiting character if any  */
	if (s_bufcount > 0) {
		c = s_buf[s_bufindex];
		if (wait != -1) {
			s_bufindex = (s_bufindex + 1) % sizeof(s_buf);
			s_bufcount--;
		}
		return c;
	}
	return 0;
}

int
lm_locate(int x, int y)
{
	if (my_console == LM_CONSOLE_TTY) {
		char buf[12];
		snprintf(buf, sizeof buf, "\x1b[%d;%dH", y + 1, x + 1);
		tty_puts(buf);
		fflush(stdout);
		my_cursor_x = x;
		my_cursor_y = y;
		return 0;
	}
#if !__CONSOLE__
	if (x >= 0 && x < my_max_x && y >= 0 && y < my_max_y) {
		lm_redraw((my_cursor_x + my_cursor_xofs) * 8, my_height - (my_cursor_y + my_cursor_yofs + 1) * 16, 8, 16);
		my_cursor_x = x;
		my_cursor_y = y;
		lm_redraw((my_cursor_x + my_cursor_xofs) * 8, my_height - (my_cursor_y + my_cursor_yofs + 1) * 16, 8, 16);
	}
	return 0;
#endif
}

pixel_t
lm_tcolor(pixel_t col)
{
	pixel_t oldcol = s_tcolor;
	s_tcolor = col;
	if (my_console == LM_CONSOLE_TTY) {
		/*  Set text color to the nearest one  */
		int r, g, b;
		char buf[8];
#if BYTES_PER_PIXEL == 2
		if (col == 0xffff)
			col = 9;
		else {
			r = (col >> 11) & 0x1f;
			g = (col >> 5) & 0x3f;
			b = (col & 0x1f);
			col = (b >= 24 ? 1 : 0);
			col |= (g >= 48 ? 2 : 0);
			col |= (r >= 24 ? 4 : 0);
		}
#else
		if (col == 0xffffffff)
			col = 9;
		else {
			r = (col >> 16) & 0xff;
			g = (col >> 8) & 0xff;
			b = (col & 0xff);
			col = (b >= 128 ? 1 : 0);
			col |= (g >= 128 ? 2 : 0);
			col |= (r >= 128 ? 4 : 0);
		}
#endif			
		snprintf(buf, sizeof buf, "\x1b[%dm", 30 + (int)col);
		tty_puts(buf);
	}
	return oldcol;
}

pixel_t
lm_bgcolor(pixel_t col)
{
	pixel_t oldcol = s_bgcolor;
	s_bgcolor = col;
	if (my_console == LM_CONSOLE_TTY) {
		/*  Set text color to the nearest one  */
		int r, g, b;
		char buf[8];
		if (col == 0)
			col = 9;
		else {
#if BYTES_PER_PIXEL == 2
			r = (col >> 11) & 0x1f;
			g = (col >> 5) & 0x3f;
			b = (col & 0x1f);
			col = (b >= 24 ? 1 : 0);
			col |= (g >= 48 ? 2 : 0);
			col |= (r >= 24 ? 4 : 0);
#else
			r = (col >> 16) & 0xff;
			g = (col >> 8) & 0xff;
			b = (col & 0xff);
			col = (b >= 128 ? 1 : 0);
			col |= (g >= 128 ? 2 : 0);
			col |= (r >= 128 ? 4 : 0);
#endif			
		}
		snprintf(buf, sizeof buf, "\x1b[%dm", 40 + (int)col);
		tty_puts(buf);
	}
	return oldcol;
}

volatile u_int8_t g_init_screen_done = 0;

int
lm_init_screen(void) 
{
	int i;
	
	my_graphic_mode = 0;
	my_origin_x = my_origin_y = 0;
	my_tab_base = 0;
	my_tab_width = 4;
	
#if !__CONSOLE__
	lm_init_screen_platform();
#endif
	
	for (i = 0; i < 16; i++)
		s_palette[i] = s_initial_palette[i];
	
	/*  Allocate pattern buffer for (32x32)xNUMBER_OF_PATTERNS (=512 KB)  */
	my_patbuffer_size = 32*32*NUMBER_OF_PATTERNS;
	my_patbuffer_count = 0;
	my_patbuffer = (pixel_t *)calloc(sizeof(pixel_t), my_patbuffer_size);
	memset(my_patterns, 0, sizeof(my_patterns));

	lm_redraw(0, 0, my_width, my_height);
	
	if (my_console == LM_CONSOLE_TTY) {
		/*  Determine screen size if possible  */
		char buf[16];
		/*  Default values  */
		my_max_x = 80;
		my_max_y = 24;
		tty_puts("\x1b[18t");
		for (i = 0; i < 16; i++) {
			int ch, j;
			for (j = 100; j >= 0; j--) {
				ch = lm_getch(0);
				if (ch != 0)
					break;
				usleep(10000);
			}
			if (j < 0) {
				buf[i] = 0;
				break;  /*  Timeout  */
			}
			buf[i] = ch;
			if (ch == 't')
				break;
		}
		if (i < 16 && buf[i] == 't') {
			char *p = buf + 4;
			my_max_y = strtol(p, &p, 0);
			p++;
			my_max_x = strtol(p, &p, 0);
		}
		my_width = 8 * my_max_x;
		my_height = 16 * my_max_y;	
	} else {
		my_max_x = my_width / 8;
		my_max_y = my_height / 16;
	}
	
	g_init_screen_done = 1;
	
	return 0;
}

#if 0
#pragma mark ====== Lua Interface ======
#endif

/*  io.read replacement (only 'no argument' type is implemented) */
static int
my_io_read(lua_State *L)
{
	char s_read_buf[256];
	int nargs = lua_gettop(L) - 1;
	if (nargs > 0)
		return luaL_argerror(L, 1, "io.read() with format arguments is not supported");
	if (lm_getline(s_read_buf, sizeof s_read_buf) == -1)
		return lm_interrupt(L);
	lua_pushstring(L, s_read_buf);
	return 1;
}

/*  io.write replacement  */
static int
my_io_write(lua_State *L)
{
	int arg = 1;
	int nargs = lua_gettop(L);
	int status = 1;
	char buf[64];
	for (; --nargs >= 0; arg++) {
		if (lua_type(L, arg) == LUA_TNUMBER) {
			/* optimization: could be done exactly as for strings */
			int len = lua_isinteger(L, arg)
			? snprintf(buf, sizeof buf, LUA_INTEGER_FMT, lua_tointeger(L, arg))
			: snprintf(buf, sizeof buf, LUA_NUMBER_FMT, lua_tonumber(L, arg));
			status = status && (len > 0);
			lm_puts(buf);
		}
		else {
			size_t l;
			const char *s = luaL_checklstring(L, arg, &l);
			lm_puts(s);
		}
	}
	if (status)
		lua_pushboolean(L, 1);
	else lua_pushnil(L);
	return 1;
}

/*  print replacement  */
static int
my_print(lua_State *L)
{
	int n = lua_gettop(L);  /* number of arguments */
	int i;
	lua_getglobal(L, "tostring");
	for (i = 1; i <= n; i++) {
		const char *s;
		size_t l;
		lua_pushvalue(L, -1);  /* function to be called */
		lua_pushvalue(L, i);   /* value to print */
		lua_call(L, 1, 1);
		s = lua_tolstring(L, -1, &l);  /* get result */
		if (s == NULL)
			return luaL_error(L, "'tostring' must return a string to 'print'");
		if (i > 1)
			lm_puts("\t");
		lm_puts(s);
		lua_pop(L, 1);  /* pop result */
	}
	lm_puts("\n");
	return 0;
}

/*  cls
 Clear the text screen. */
static int
l_cls(lua_State *L)
{
	lm_cls();
	my_cursor_x = my_cursor_y = 0;
	return 0;
}

/*  clearline
 Clear to the end of the line.  */
static int
l_clearline(lua_State *L)
{
	lm_erase_to_eol();
	return 0;
}

/*  color(c1[,c2])
 Set the foreground color to c1, and background color to c2 (if given) */
static int
l_color(lua_State *L)
{
	int nargs = lua_gettop(L);
	int n = luaL_checkinteger(L, 1);
	s_tcolor = s_palette[n % 16];
	if (my_console == LM_CONSOLE_TTY)
		lm_tcolor(s_tcolor);
	if (nargs > 1) {
		n = luaL_checkinteger(L, 2);
		s_bgcolor = s_palette[n % 16];
		if (my_console == LM_CONSOLE_TTY)
			lm_bgcolor(s_bgcolor);
	}
	return 0;
}

/*  inkey([mode])
 Get one character from the keyboard.
 mode = 0 (default): wait for input
 mode = 1: no wait for input  */
static int
l_inkey(lua_State *L)
{
	int n = 0;
	int nargs = lua_gettop(L);
	if (nargs >= 1)
		n = luaL_checkinteger(L, 1);
	n = (n == 0);
	n = lm_getch(n);
	if (n == 3)
		return lm_interrupt(L);
	lua_pushinteger(L, n);
	return 1;
}

static int
l_locate(lua_State *L)
{
	int x, y;
	x = luaL_checkinteger(L, 1);
	y = luaL_checkinteger(L, 2);
	lm_locate(x, y);
	return 0;
}

static int
l_puts(lua_State *L)
{
	size_t len;
	const char *p;
	p = luaL_checklstring(L, 1, &len);
	lm_puts(p);
	return 0;
}

static int
l_screensize(lua_State *L)
{
	int width = my_width;
	int height = my_height;
	lua_pushinteger(L, width);
	lua_pushinteger(L, height);
	return 2;
}

static const struct luaL_Reg con_lib[] = {
	{"locate", l_locate},
	{"puts", l_puts},
	{"cls", l_cls},
	{"clearline", l_clearline},
	{"color", l_color},
	{"inkey", l_inkey},
	{"gcolor", l_gcolor},
	{"fillcolor", l_fillcolor},
	{"line", l_line},
	{"circle", l_circle},
	{"arc", l_arc},
	{"fan", l_fan},
	{"poly", l_poly},
	{"box", l_box},
	{"rbox", l_rbox},
	{"p_create", l_create_path},
	{"p_destroy", l_destroy_path},
	{"p_close", l_close_path},
	{"p_draw", l_draw_path},
	{"p_line", l_path_line},
	{"p_circle", l_path_circle},
	{"p_arc", l_path_arc},
	{"p_fan", l_path_fan},
	{"p_poly", l_path_poly},
	{"p_box", l_path_box},
	{"p_rbox", l_path_rbox},
	{"p_cubic", l_path_cubic},
	{"gcls", l_gcls},
	{"gmode", l_gmode},
	{"patdef", l_patdef},
	{"patundef", l_patundef},
	{"patdraw", l_patdraw},
	{"paterase", l_paterase},
	{"rgb", l_rgb},
	{"screensize", l_screensize},
	{NULL, NULL}
};

int
luaopen_con(lua_State *L) 
{
	lm_read_fontdata("fontdata.bin");
	lm_init_screen();

#if LUA_VERSION_NUM >= 502
	luaL_newlib(L, con_lib);
#else
	luaL_register(L, "con", con_lib);
#endif
	
	/*  Replace io.read, io.write, print  */
	lua_pushcfunction(L, my_print);
	lua_setglobal(L, "print");
	lua_getglobal(L, "io");
	lua_pushcfunction(L, my_io_read);
	lua_setfield(L, -2, "read");
	lua_pushcfunction(L, my_io_write);
	lua_setfield(L, -2, "write");
	lua_pop(L, 1);
	
	return 1;
}

