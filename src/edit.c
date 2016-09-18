/*
 *  edit.c
 *  LuaConsole
 *
 *  Created by Toshi Nagata on 16/02/15.
 *  Copyright 2016 Toshi Nagata. All rights reserved.
 *
 */

#include "screen.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/*  Screen editor  */

u_int8_t gEnableSyntaxColoring;

pixel_t gSyntaxColors[] = {
	RGBFLOAT(1.0, 1.0, 1.0),
	RGBFLOAT(0.9, 0.9, 0.4),
	RGBFLOAT(0.5, 1.0, 0.5),
	RGBFLOAT(0.5, 1.0, 1.0),
	RGBFLOAT(0.9, 0.4, 0.9),
	RGBFLOAT(1.0, 0.5, 1.0),
	RGBFLOAT(1.0, 0.5, 0.5),
	RGBFLOAT(1.0, 0.6, 0.6),
	RGBFLOAT(0.2, 0.1, 0.4),
	RGBFLOAT(0.8, 0.8, 0.8)
};

/*  Base pointer of the source program area  */
u_int8_t *gSourceBasePtr;

/*  Size of the current source program  */
u_int32_t gSourceEnd;

/*  Size of the source program area  */
u_int32_t gSourceLimit;

/*  X offset (for display of line numbers)  */
#define LM_X_OFFSET 5

/*  The text in memory is divided into two. The first part is 0 ~ s_edit_gap_bottom (excluding),
    and the second part is s_edit_gap_top to s_edit_limit (= gSourceLimit).  */
/*  s_edit_limit[0] is '\0'.  */

static int32_t s_edit_gap_bottom;
static int32_t s_edit_gap_top;

#define s_edit_limit gSourceLimit

#define LM_EDIT_GAP (s_edit_gap_top - s_edit_gap_bottom)
#define LM_EDIT_NEXT(p, n) (p += n, p == s_edit_gap_bottom ? (p = s_edit_gap_top) : p)
#define LM_EDIT_LAST(p, n) ((p == s_edit_gap_top ? (p = s_edit_gap_bottom) : p), p -= n)
#define LM_EDIT_POS2OFS(p) (p - (p >= s_edit_gap_top ? LM_EDIT_GAP : 0))
#define LM_EDIT_OFS2POS(o) (o >= s_edit_gap_bottom ? o + LM_EDIT_GAP : o)

/*  Text position of top-left of the screen  */
static int32_t s_edit_topleft;

/*  Cursor position  */
static int32_t s_edit_cursor;

/*  Syntax color at the top-left of the screen  */
static int s_edit_color_topleft;

/*  Line number at the top-left of the screen  */
static int s_edit_lineno_topleft;

/*  Previous column position, after moving cursor up/down  */
static int s_edit_last_column;

/*  s_edit_last_column is reset to -1 after any editing activity (including 
    cursor movement), unless s_edit_last_column_enabled is non-zero.  */
/*  This flag ensures that s_edit_last_column is 'forgotten' unless explicitly
    told to be remembered.  */
static u_int8_t s_edit_last_column_enabled;

/*  Syntax color for each column in a row (only used temporarily)  */
static int8_t *s_edit_color_for_column;

#if 0
/*  Line attribute  */
typedef struct RowAttr {
	int32_t pos;    /*  Offset from the storage top; if negative, then no data  */
	int32_t len;
	int32_t lineno; /*  Line number; if negative, then it is not the top of the line  */
	u_int8_t color; /*  0: normal, 1: reserved words, 2: built-ins, 3: labels,
					    4: other symbols, 5: numbers, 6: string literals, 7: comments  */
} RowAttr;

/*  An array of RowAttr. The number of elements is s_height * 2, so that the
 second half can be used for working area  */
static RowAttr *s_row_attrs;
#endif

/*  Reserved words  */
static char *s_pb_reserved_words[] = {
	"and", "break", "do", "else", "elseif", "end",
	"false", "for", "function", "goto", "if", "in",
	"local", "nil", "not", "or", "repeat", "return",
	"then", "true", "until", "while", NULL };

/*  Height and width of the editing area  */
/*  s_width = my_max_x - LM_X_OFFSET, s_height = my_max_y - 1  */
static int s_height;
static int s_width;

static void
s_pb_show_info(int tcolor, int bgcolor, char *msg)
{
	lm_locate(0, s_height);
	lm_erase_to_eol();
	lm_tcolor(tcolor);
	lm_bgcolor(bgcolor);
	lm_puts(msg);
}

static void
s_pb_show_error_info(char *msg)
{
	s_pb_show_info(RGBFLOAT(0.5, 0, 0), 0xffff, msg);
}

static int
s_pb_lookup_reserved_words(u_int8_t *p)
{
	int i;
	i = sizeof(s_pb_reserved_words) / sizeof(s_pb_reserved_words[0]) - 1;
	while (--i >= 0) {
		if (strcmp((char *)p, s_pb_reserved_words[i]) == 0)
			return i;
	}
	return -1;
}

/*  Set edit gap position; i.e. move the text after the given position to
 the end of free area, and make the gap start at the given position .  */
static void
s_pb_set_edit_gap(int32_t gap_bottom)
{
	int dpos = (int)gap_bottom - (int)s_edit_gap_bottom;
	if (dpos > 0) {
		memmove(s_edit_gap_bottom + gSourceBasePtr, s_edit_gap_top + gSourceBasePtr, dpos);
		s_edit_gap_bottom += dpos;
		s_edit_gap_top += dpos;
	} else if (dpos < 0) {
		s_edit_gap_bottom += dpos;
		s_edit_gap_top += dpos;
		memmove(s_edit_gap_top + gSourceBasePtr, s_edit_gap_bottom + gSourceBasePtr, -dpos);
	}
}

/*  Calculate the screen position of the given character, assuming top_ofs is at
    (0, 0) screen position.  */
/*  new_top_ofs and drout is meaningful only when ofs < topofs. In this case, the 
    top of the logical line that includes the position of ofs should be looked for. 
    The offset of this top of the logical line is returned in *new_top_ofs, and
    the number of rows between *new_top_ofs and top_ofs is returned in *drout. */
static void
s_pb_calc_screen_pos_for_character(int32_t ofs, int32_t top_ofs, int *rout, int *cout, int32_t *new_top_ofs, int *drout)
{
	int r, c, dr, wid;
	int32_t p, pp, pnext;

	pp = LM_EDIT_OFS2POS(ofs);
	p = LM_EDIT_OFS2POS(top_ofs);

	if (ofs < top_ofs) {
		/*  We should go backward to previous lines  */
		/*  Search for the logical line that includes the position of ofs  */
		int32_t p1 = p;
		int32_t prev_ofs;
		while (pp < p1) {
			/*  if p1 is at the top of a logical line, then start searching from p1 - 1 */
			if (gSourceBasePtr[p1 - 1] == '\n')
				p1--;
			while (p1 > 0) {
				/*  Move to the top of a logical line  */
				LM_EDIT_LAST(p1, 1);
				if (gSourceBasePtr[p1] == '\n') {
					LM_EDIT_NEXT(p1, 1);
					break;
				}
			}
		}
		/*  Calc screen pos of p relative to p1; the screen pos should be (0, y)  */
		prev_ofs = LM_EDIT_POS2OFS(p1);
		s_pb_calc_screen_pos_for_character(top_ofs, prev_ofs, &dr, &c, NULL, NULL);
		if (c != 0) {
			s_pb_show_error_info("Error 116");
			return;
		}
		top_ofs = prev_ofs;
		p = p1;
		if (new_top_ofs != NULL)
			*new_top_ofs = top_ofs;
		if (drout != NULL)
			*drout = dr;
	} else dr = 0;

	p = LM_EDIT_OFS2POS(top_ofs);
	r = c = 0;
	while (1) {
		if (gSourceBasePtr[p] >= 0x80) {
			char *ptr;
			/*  Look forward for the next character  */
			wid = lm_character_width(lm_utf8_to_utf16((char *)(gSourceBasePtr + p), &ptr));
			pnext = ptr - (char *)gSourceBasePtr;
			if (c + wid > s_width) {
				/*  The current character should be in the next row  */
				c = 0;
				r++;
			}
		} else if (gSourceBasePtr[p] == '\t') {
			wid = my_tab_width - (c % my_tab_width);
			pnext = p;
			LM_EDIT_NEXT(pnext, 1);
		} else {
			wid = 1;
			pnext = p;
			LM_EDIT_NEXT(pnext, 1);
		}
		if (p >= pp)
			break;
		if (gSourceBasePtr[p] == '\n') {
			r++;
			c = 0;
		} else {
			c += wid;
			if (c >= s_width) {
				r++;
				c = 0;
			}
		}
		p = pnext;
	}
	if (rout != NULL)
		*rout = r - dr;
	if (cout != NULL)
		*cout = c;
}

/*  Find the character at the given screen position, assuming that top_ofs is at (0, 0).
 If no character is present, then returns the position of the 'last' character that
 does not exceed the given screen position, and also returns the screen position of
 that character in *rout and *cout.  */
/*  new_top_ofs and drout is meaningful only when row < 0. In this case, the 
 top of the logical line with the row-number less than row should be looked for. 
 The offset of this top of the logical line is returned in *new_top_ofs, and
 the number of rows between *new_top_ofs and top_ofs is returned in *drout. */
static int32_t
s_pb_find_character_at_screen_pos(int32_t top_ofs, int row, int column, int *rout, int *cout, int32_t *new_top_ofs, int *drout)
{
	int32_t p = LM_EDIT_OFS2POS(top_ofs);
	int32_t pp, pnext;
	int r, c, rr, cc, dr, wid;
	
	if (row < 0) {
		/*  We should go backward to previous lines  */
		/*  Search for the logical line top with the row-number less than row  */
		int32_t p1 = p;
		int32_t prev_ofs;
		dr = 0;
		while (dr > row && p1 > 0) {
			LM_EDIT_LAST(p1, 1);
			if (p1 < 0 || gSourceBasePtr[p1] != '\n')
				LM_EDIT_NEXT(p1, 1);
			while (p1 > 0) {
				LM_EDIT_LAST(p1, 1);
				if (p1 < 0 || gSourceBasePtr[p1] == '\n') {
					LM_EDIT_NEXT(p1, 1);
					break;
				}
			}
			prev_ofs = LM_EDIT_POS2OFS(p1);
			s_pb_calc_screen_pos_for_character(top_ofs, prev_ofs, &rr, &cc, NULL, NULL);
			if (cc != 0) {
				s_pb_show_error_info("Error 189");
				return 0;
			}
			if (rr == 0)
				break;
			dr -= rr;
			top_ofs = prev_ofs;
		}
		dr = -dr;
		if (new_top_ofs != NULL)
			*new_top_ofs = top_ofs;
		if (drout != NULL)
			*drout = dr;
		row += dr;
	} else {
		dr = 0;
		if (new_top_ofs != NULL)
			*new_top_ofs = s_edit_topleft;
		if (drout != NULL)
			*drout = 0;
	}
	
	if (row < 0) {
		/*  Cannot go up further: look for (0, column) position  */
		row = 0;
	/*	if (rout != NULL)
			*rout = 0;
		if (cout != NULL)
			*cout = c;
		return LM_EDIT_POS2OFS(gSource); */
	}
	
	p = LM_EDIT_OFS2POS(top_ofs);
	r = c = 0;
	pp = p;
	rr = r;
	cc = c;
	while (1) {
		if (gSourceBasePtr[p] == 0) {
			/*  End of text  */
			if (r > row || (r == row && c > column)) {
				/*  If this is beyond the target position, then
				    return the last character  */
				r = rr;
				c = cc;
				p = pp;
			}
			break;
		}
		if (gSourceBasePtr[p] >= 0x80) {
			/*  Look forward for the next character  */
			char *ptr;
			wid = lm_character_width(lm_utf8_to_utf16((char *)(gSourceBasePtr + p), &ptr));
			pnext = ptr - (char *)gSourceBasePtr;
			if (c + wid > s_width) {
				/*  The current character should be in the next row  */
				c = 0;
				r++;
			}
		} else if (gSourceBasePtr[p] == '\t') {
			wid = my_tab_width - (c % my_tab_width);
			pnext = p;
			pnext++;
		} else {
			wid = 1;
			pnext = p;
			pnext++;
		}
		LM_EDIT_NEXT(pnext, 0);  /*  Jump over the GAP  */
		if (r == row && c == column)
			break;
		else if (r > row || (r == row && c > column)) {
			/*  If this is beyond the target position, then return the last character  */
			/*  (We need to do this check twice, because c/r are updated twice in this loop) */
			r = rr;
			c = cc;
			p = pp;
			break;
		}
		rr = r;
		cc = c;
		pp = p;
		if (gSourceBasePtr[p] == '\n') {
			r++;
			c = 0;
		} else {
			c += wid;
			if (c >= s_width) {
				r++;
				c = 0;
			}
		}
		p = pnext;
	}
	if (rout != NULL)
		*rout = r - dr;
	if (cout != NULL)
		*cout = c;
	return LM_EDIT_POS2OFS(p);
}

/*  Process one row  */
/*  If show_line is true, then display the content on the screen  */
/*  If color is not NULL, *color should contain the syntax color at the given offset.  */
/*  In this case, the syntax color at the end of this row is returned in *color.  */
/*  Return value: 0, normal; 1, this row ends with '\n'; 2, this row ends with '\0' */
/*  IMPORTANT NOTE: the editing gap may change during processing row!!  */
static int
s_pb_process_row(int32_t *ofsp, int *colorp, int *lineno, int show_line)
{
	int32_t p, p_next, psave, ofs, ofs_next;
	int32_t cl;
	u_int8_t c, csave, emark;
	int my_tab_base_save;

	ofs = *ofsp;
	p = LM_EDIT_OFS2POS(ofs);
	if (colorp != NULL)
		cl = *colorp;
	else cl = 0;
	
	emark = 0;

	/*  Show line number  */
	if (show_line) {
		int c1, c2;
		char buf[6];
		lm_locate(0, my_cursor_y);
		lm_erase_to_eol();
		c1 = lm_bgcolor(gSyntaxColors[9]);
		c2 = lm_tcolor(gSyntaxColors[8]);
		if (*lineno > 0 && gSourceBasePtr[p] != 0) {
			snprintf(buf, sizeof buf, "%4d", *lineno);
			lm_puts(buf);
		} else {
			lm_puts("    ");
		}
		lm_bgcolor(c1);
		lm_tcolor(c2);
		lm_locate(LM_X_OFFSET, my_cursor_y);
	}

	/*  Move the editing gap before the top of the row. This simplify syntax coloring. */
	/*  This happens only once per processing of rows, so it should not be a huge overhead */
	ofs_next = s_edit_gap_bottom;
	if (ofs_next > ofs) {
		s_pb_set_edit_gap(ofs);
		p = LM_EDIT_OFS2POS(ofs);
	}
	
	/*  Position of the top of the next row  */
	ofs_next = s_pb_find_character_at_screen_pos(ofs, 1, 0, NULL, NULL, NULL, NULL);
	if (ofs_next == ofs) {
		p_next = p;
		emark = 2;
	} else {
		ofs_next--;
		p_next = LM_EDIT_OFS2POS(ofs_next);
		if (gSourceBasePtr[p_next] == '\n')
			emark = 1;
		LM_EDIT_NEXT(p_next, 1);
		if (emark == 0 && gSourceBasePtr[p_next] == 0)
			emark = 2;
		ofs_next++;
	}
	
	psave = p;

	my_tab_base_save = my_tab_base;
	if (show_line)
		my_tab_base = LM_X_OFFSET;
	
	while (1) {
		int clsave = cl;
		if ((cl >= 1 && cl <= 4) || cl == -1) {
			/*  Look for the end of the word: may exceed p_next but not go beyond '\n' or 0  */
			while (((c = gSourceBasePtr[p]) >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
				   (c >= '0' && c <= '9') || c == '_') {
				p++;
			}
		/*	if (c == LM_SUFFIX_FLT_CHAR || c == LM_SUFFIX_STR_CHAR || c == LM_SUFFIX_BYTE_CHAR) {
				p++;
			} */
			if (cl == -1) {
				if (p >= p_next || show_line) {
					/*  We need to determine which type of symbol is this  */
					csave = gSourceBasePtr[p];
					gSourceBasePtr[p] = 0;  /*  Temporarily terminate the word  */
					if (s_pb_lookup_reserved_words(gSourceBasePtr + psave) >= 0) {
						cl = 1;
				/*	} else if (lm_lookup_builtin_entry(gSourceBasePtr + psave, 1) >= 0) {
						cl = 2; */
					} else cl = 3;
					gSourceBasePtr[p] = csave;
					clsave = cl;
				}
			}
		} else if (cl == 5) {
			int hex = 0;
			c = gSourceBasePtr[p];
			if (p > 0 && gSourceBasePtr[p - 1] == '0' && (c == 'x' || c == 'X')) {
				/*  Hexadecimal  */
				hex = 1;
				p++;
			}
			while (1) {
				c = gSourceBasePtr[p];
				if ((c >= '0' && c <= '9') || (hex && ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))) 
					p++;
				else break;
			}
			if (p < p_next)
				cl = 0;
		} else if (cl == 6) {
			while ((c = gSourceBasePtr[p]) != '\"' && c != '\n' && c != 0 && p < p_next)
				p++;
			if (p < p_next) {
				if (c == '\"' || c == '\n')
					p++;
				cl = 0;
			}
		} else if (cl == 7) {
			while ((c = gSourceBasePtr[p]) != '\n' && c != 0 && p < p_next)
				p++;
			if (p < p_next) {
				if (c == '\n')
					p++;
				cl = 0;
			}
		}
		if (p > p_next)
			p = p_next;
		if (show_line && p > psave) {
			if (clsave < 0)
				clsave = 0;
			lm_tcolor(gSyntaxColors[clsave]);
			csave = gSourceBasePtr[p];
			gSourceBasePtr[p] = 0;
			lm_puts((char *)(gSourceBasePtr + psave));
			gSourceBasePtr[p] = csave;
		}
		if (p == p_next)
			break;
		
		/*  Look for the first character to be colored  */
		psave = p;
		do {
			if (((c = gSourceBasePtr[p]) >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
				cl = -1;
				break;
			} else if (c == '@') {
				cl = 4;
				break;
			} else if (c >= '0' && c <= '9') {
				cl = 5;
				break;
			} else if (c == '\"') {
				cl = 6;
				break;
			} else if (c == '-' && gSourceBasePtr[p + 1] == '-') {
				cl = 7;
				break;
			}
			p++;
		} while (p < p_next);
		
		if (show_line && p > psave) {
			lm_tcolor(gSyntaxColors[0]);
			csave = gSourceBasePtr[p];
			gSourceBasePtr[p] = 0;
			lm_puts((char *)(gSourceBasePtr + psave));
			gSourceBasePtr[p] = csave;
		}
		if (p == p_next)
			break;
		psave = p;
		p++;
	}		
	
	my_tab_base = my_tab_base_save;
					
	if (colorp != NULL)
		*colorp = cl;
	*ofsp = LM_EDIT_POS2OFS(p);
	
	if (lineno != NULL) {
		if (emark == 1) {
			if (*lineno > 0)
				(*lineno)++;
			else *lineno = 1 - *lineno;
		} else if (emark == 0) {
			if (*lineno > 0)
				*lineno = -*lineno;
		}
	}
	
	return emark;
}

static void
s_pb_show_lines(int row, int nrows)
{
	int32_t ofs;
	int color;
	int lineno;
	int r, rlim;
	if (row < 0 || row >= s_height) {
		s_pb_show_error_info("Error 478");
		return;
	}
	rlim = row + nrows;
	if (rlim > s_height)
		rlim = s_height;
	ofs = s_edit_topleft;
	color = s_edit_color_topleft;
	lineno = s_edit_lineno_topleft;
	for (r = 0; r < rlim; r++) {
		lm_locate(0, r);
		s_pb_process_row(&ofs, &color, &lineno, (r >= row));
	}
}

/*  Scroll by N lines (negative N: up, positive N: down)  */
/*  s_edit_lineno_topleft and s_edit_color_topleft are also updated  */
/*  If redraw is false, then the screen is not redrawn (this is used when
	the whole screen is redrawn afterwards)  */
static void
s_pb_edit_scroll(int n, int redraw)
{
	int32_t ofs;
	int r, dr, dn, color, lineno, lineno_save;
	if (n < 0) {
		for (r = 0; r < -n; r++)
			s_pb_process_row(&s_edit_topleft, &s_edit_color_topleft, &s_edit_lineno_topleft, 0);
	} else {
		s_pb_find_character_at_screen_pos(s_edit_topleft, -n, 0, NULL, NULL, &ofs, &dr);
		s_edit_color_topleft = 0;
		lineno = 1;
		for (r = 0; r < dr - n; r++)
			s_pb_process_row(&ofs, &s_edit_color_topleft, &lineno, 0);
		s_edit_topleft = ofs;
		lineno_save = lineno;
		/*  To calculate correct lineno, we need to process more lines (without updating color) */
		while (r++ < dr)
			s_pb_process_row(&ofs, NULL, &lineno, 0);
		dn = (s_edit_lineno_topleft > 0 ? s_edit_lineno_topleft : -s_edit_lineno_topleft);
		dn -= (lineno > 0 ? lineno : -lineno);
		if (lineno_save > 0) {
			s_edit_lineno_topleft = lineno_save + dn;
		} else {
			s_edit_lineno_topleft = lineno_save - dn;
		}
	}
	lm_scroll(0, 0, 16, (s_width + LM_X_OFFSET) * 8, s_height * 16, 0, -n * 16);
	ofs = s_edit_topleft;
	color = s_edit_color_topleft;
	lineno = s_edit_lineno_topleft;
	if (n < 0) {
		for (r = 0; r < s_height; r++)
			s_pb_process_row(&ofs, &color, &lineno, (r >= s_height + n));
	} else {
		for (r = 0; r < n; r++)
			s_pb_process_row(&ofs, &color, &lineno, 1);
	}
}

/*  Show the given character on the screen. Scroll the screen if necessary.
 The cursor position is returned in *rout and *cout  */
static int
s_pb_make_position_visible(int32_t ofs, int *rout, int *cout)
{
	int r;
	s_pb_calc_screen_pos_for_character(ofs, s_edit_topleft, &r, cout, NULL, NULL);
	if (r >= s_height) {
		/*  Scroll up  */
		s_pb_edit_scroll(-(r - s_height + 1), 1);
		r = s_height - 1;
	}
	if (r < 0) {
		/*  Scroll up  */
		s_pb_edit_scroll(-r, 1);
		r = 0;
	}
	*rout = r;
	return 0;
}

/*  Move forward by n characters  */
static int
s_pb_edit_move_forward(int n)
{
	int32_t p = LM_EDIT_OFS2POS(s_edit_cursor);
	int r, c;
	while (n-- > 0 && gSourceBasePtr[p] != 0) {
		char *ptr;
		lm_utf8_to_utf16((char *)(gSourceBasePtr + p), &ptr);
		p = ptr - (char *)gSourceBasePtr;
	}
	s_edit_cursor = LM_EDIT_POS2OFS(p);
	s_pb_calc_screen_pos_for_character(s_edit_cursor, s_edit_topleft, &r, &c, NULL, NULL);
	if (r >= s_height) {
		/*  Scroll up  */
		s_pb_edit_scroll(-(r - s_height + 1), 1);
	}
	return 0;
}		

/*  Move backward by n characters  */
static int
s_pb_edit_move_backward(int n)
{
	int32_t p = LM_EDIT_OFS2POS(s_edit_cursor);
	int r, c;
	s_edit_last_column = -1;
	LM_EDIT_LAST(p, 0);  /*  In case p == s_edit_gap_top, jump backwards over the gap  */
	while (n-- > 0 && p > 0) {
		do {
			LM_EDIT_LAST(p, 1);
		} while ((c = gSourceBasePtr[p]) >= 0x80 && c <= 0xbf);
	}
	s_edit_cursor = LM_EDIT_POS2OFS(p);
	s_pb_calc_screen_pos_for_character(s_edit_cursor, s_edit_topleft, &r, &c, NULL, NULL);
	if (r < 0) {
		/*  Scroll up  */
		s_pb_edit_scroll(-r, 1);
	}
	return 0;
}

/*  Move down by n rows  */
static int
s_pb_edit_move_down(int n)
{
	int r, c, rr, cc;
	int32_t ofs;
	r = my_cursor_y + n;
	if ((c = s_edit_last_column) < 0)
		c = s_edit_last_column = my_cursor_x - LM_X_OFFSET;
	ofs = s_pb_find_character_at_screen_pos(s_edit_topleft, r, c, &rr, &cc, NULL, NULL);
	if (rr >= s_height)
		s_pb_edit_scroll(-(rr - s_height + 1), 1);
	s_edit_cursor = ofs;
	s_edit_last_column_enabled = 1;
	return 0;
}

/*  Move up by n rows  */
static int
s_pb_edit_move_up(int n)
{
	int r, c, rr, cc, dr;
	int32_t ofs, newtop;
	r = my_cursor_y - n;
	if ((c = s_edit_last_column) < 0)
		c = s_edit_last_column = my_cursor_x - LM_X_OFFSET;
	ofs = s_pb_find_character_at_screen_pos(s_edit_topleft, r, c, &rr, &cc, &newtop, &dr);
	if (rr < 0)
		s_pb_edit_scroll(-rr, 1);
	s_edit_cursor = ofs;
	s_edit_last_column_enabled = 1;
	return 0;
}

/*  Insert character  */
static int
s_pb_edit_insert_printable_char(int ch)
{
	char buf[8];
	int len = 1, r, c;
	buf[0] = ch;
	if (ch >= 0xc2 && ch <= 0xdf) {
		/*  2 bytes  */
		buf[len++] = lm_getch(1);
	} else if (ch >= 0xe0 && ch <= 0xef) {
		/*  3 bytes  */
		buf[len++] = lm_getch(1);
		buf[len++] = lm_getch(1);
	}
	buf[len] = 0;
	s_pb_set_edit_gap(s_edit_cursor);
	if (s_edit_gap_top - s_edit_gap_bottom < len + 1024) {
		s_pb_show_error_info("Out of memory");
		return -1;
	}
	memmove(gSourceBasePtr + s_edit_gap_bottom, buf, len);
	s_edit_gap_bottom += len;
	s_edit_cursor += len;
	s_pb_show_lines(my_cursor_y, s_height - my_cursor_y);
	s_pb_make_position_visible(s_edit_cursor, &r, &c);
	lm_locate(c, r);
	return 0;
}

/*  Backspace  */
static int
s_pb_edit_delete_backwards(void)
{
	int cy;
	u_int8_t ch;
	int32_t p;

	if (s_edit_cursor == 0)
		return 0;  /*  Do nothing  */

	/*  Move to the previous character  */
	s_pb_set_edit_gap(s_edit_cursor);
	p = s_edit_gap_bottom;
	while ((ch = gSourceBasePtr[--p]) >= 0x80 && ch <= 0xbf);

	cy = my_cursor_y;
	if (my_cursor_x == LM_X_OFFSET)
		cy--;
	
	/*  If we are at the top-left of the screen, we need to scroll down  */
	if (s_edit_cursor == s_edit_topleft) {
		s_pb_edit_scroll(1, 0);
		cy++;
	}
	
	/*  Delete one character  */
	s_edit_gap_bottom = p;
	s_edit_cursor = s_edit_gap_bottom;
	
	s_pb_show_lines(cy, s_height - cy);
	
	return 0;
}

/*  Insert newline  */
static int
s_pb_edit_insert_newline(void)
{
	int cy;
	if (s_edit_gap_top - s_edit_gap_bottom <= 1024) {
		s_pb_show_error_info("Out of memory");
		return -1;
	}
	cy = my_cursor_y;
	if (cy == s_height - 1) {
		/*  We need to scroll up  */
		s_pb_edit_scroll(-1, 0);
		cy--;
	}
	s_pb_set_edit_gap(s_edit_cursor);
	gSourceBasePtr[s_edit_gap_bottom++] = '\n';
	s_edit_cursor++;
	s_pb_show_lines(cy, s_height - cy);
	return 0;
}

int
lm_list(int sline, int eline)
{
	int nn, color, lineno, count;
	u_int8_t c;
	int32_t ofs;
	s_edit_gap_bottom = s_edit_gap_top = 0;
	s_height = my_max_y - 1;
	s_width = my_max_x - LM_X_OFFSET;
	lineno = 1;
	ofs = 0;
	color = 0;
	count = 0;
	while (lineno <= eline && gSourceBasePtr[ofs] != 0) {
		if (lineno >= sline) {
			nn = s_pb_process_row(&ofs, &color, &lineno, 1);
			if (nn == 2)
				break; /*  End of program  */
			if (nn == 1) {  /*  Next line (lineno is already incremented)  */
				color = 0;  /*  Reset syntax coloring  */
			}
			if (++count % 5 == 0) {
				/*  Check break  */
				if (lm_getch_with_timeout(0) == 3) { /* ctrl-C */
					lm_puts("Break\n");
					return 0;
				}
			}
		} else {
			while ((c = gSourceBasePtr[ofs]) != 0 && c != '\n')
				ofs++;
			if (c == 0)
				break;
			else ofs++;
			lineno++;
			color = 0;
		}
	}
	return 0;
}

int
lm_load_file(const char *path)
{
	size_t size, msize;
	FILE *fp;
	u_int8_t *p;
	fp = fopen(path, "r");
	if (fp == NULL)
		return 1;
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	msize = (size / 32768 + 2) * 32768;
	p = (u_int8_t *)realloc(gSourceBasePtr, msize);
	if (p == NULL)
		return 2;
	gSourceBasePtr = p;
	gSourceLimit = msize;
	fread(gSourceBasePtr, 1, size, fp);
	fclose(fp);
	gSourceBasePtr[size] = 0;
	gSourceEnd = size;
	
	return 0;
}

int
lm_edit(void)
{
	s_edit_limit = gSourceLimit - 1;
	s_edit_gap_bottom = gSourceEnd;
	s_edit_gap_top = s_edit_limit;
	gSourceBasePtr[s_edit_gap_top] = 0;
	
	s_height = my_max_y - 1;
	s_width = my_max_x - LM_X_OFFSET;

	/* 	s_width = 40;    *//*  for debug  */
/*	s_height = 12;   *//*  for debug  */
	s_edit_topleft = 0;
	s_edit_color_topleft = 0;
	s_edit_cursor = 0;
	s_edit_lineno_topleft = 1;
	s_edit_last_column = -1;
	
	if (s_edit_color_for_column != NULL)
		free(s_edit_color_for_column);
	s_edit_color_for_column = (int8_t *)calloc(sizeof(int8_t), s_width);
	
	lm_cls();
	
	s_pb_show_lines(0, s_height);
	s_pb_show_info(RGBFLOAT(0.8, 0.8, 0), 0, "<esc>=end edit");

	while (1) {
		int ch, c, r;
		s_pb_calc_screen_pos_for_character(s_edit_cursor, s_edit_topleft, &r, &c, NULL, NULL);
		lm_locate(c + LM_X_OFFSET, r);
		s_edit_last_column_enabled = 0;
		ch = lm_getch(1);
		if (ch == 27)
			break;
		else if (ch == 29)
			s_pb_edit_move_backward(1);
		else if (ch == 28)
			s_pb_edit_move_forward(1);
		else if (ch == 31)
			s_pb_edit_move_down(1);
		else if (ch == 30)
			s_pb_edit_move_up(1);
		else if (ch == 8 || ch == 0x7f)
			s_pb_edit_delete_backwards();
		else if (ch == 13)
			s_pb_edit_insert_newline();
		else if (ch >= ' ')
			s_pb_edit_insert_printable_char(ch);
		/*  s_edit_last_column is forgotten every time unless explicitly told to be remembered  */
		if (!s_edit_last_column_enabled)
			s_edit_last_column = -1;
	}
	
	/*  End edit  */
	s_pb_set_edit_gap(gSourceLimit - (s_edit_gap_top - s_edit_gap_bottom));
	gSourceEnd = s_edit_gap_bottom;
	gSourceBasePtr[s_edit_gap_bottom] = 0;

	lm_locate(0, s_height);
	lm_erase_to_eol();
	lm_tcolor(RGBFLOAT(1, 1, 1));
	lm_bgcolor(0);
	return 0;
}

