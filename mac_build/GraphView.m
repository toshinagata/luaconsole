//
//  GraphView.m
//  LuaConsole
//
//  Created by 永田 央 on 16/01/09.
//  Copyright 2016 __MyCompanyName__. All rights reserved.
//

#import "GraphView.h"
#include "screen.h"

//  CoreGraphics header
#include <ApplicationServices/ApplicationServices.h>

/*  Color palette  */
#if BYTES_PER_PIXEL == 2
static pixel_t sPalette[65536];
#endif

static CGContextRef s_text_context, s_graphic_context, s_active_context;
static pixel_t *s_text_pixels, *s_graphic_pixels;

static int s_path_drawflag = 1;  /*  Stroke only  */
static pixel_t s_stroke_color = (BYTES_PER_PIXEL == 2 ? 0xffff : 0xffffffff);
static pixel_t s_fill_color = 0;

GraphView *s_graphView;

@implementation GraphView

#define CREATE_FONT 0

#if CREATE_FONT
- (void)createFontData
{
	//  The following code snippet was used in generating the bitmap data
	static uint32_t *sBitmapBuffer;
	NSDictionary *attr;
	NSGraphicsContext *sTextContext;
	NSBitmapImageRep *rep;
	unichar ch[10];
	int i, j, k, count, xx, yy;
	u_int16_t b;
	u_int32_t uc, eu, idx;
	FILE *fp1, *fp2, *fp3;
	NSString *s;
	char buf[64], *p;
	char buf2[16][80];
	
	/*  See graph.h for details  */
	static u_int16_t sConvTable[65536];
	static u_int8_t sFontData[16*188];
	static u_int8_t sKanjiData[32*11844];
	
	sBitmapBuffer = (uint32_t *)calloc(sizeof(uint32_t), 1024);
	rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:(uint8_t **)&sBitmapBuffer pixelsWide:32 pixelsHigh:32 bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSCalibratedRGBColorSpace bytesPerRow:0 bitsPerPixel:0];
	sTextContext = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
	[sTextContext setShouldAntialias:NO];
	[NSGraphicsContext saveGraphicsState];
	[NSGraphicsContext setCurrentContext:sTextContext];
	attr = [NSDictionary dictionaryWithObjectsAndKeys:[NSFont fontWithName:@"Sazanami-Gothic-Regular" size:15], NSFontAttributeName, [NSColor whiteColor], NSForegroundColorAttributeName, nil];
	
	fp2 = fopen("fontdata.ascii", "w");
	fp3 = fopen("../../euc-jis-2004-with-char-u8.txt", "r");
	count = 0;
	for (i = 0; i < 65536; i++)
		sConvTable[i] = 0xffff;
	while (fgets(buf, sizeof buf, fp3) != NULL) {
		j = k = 0;
		for (i = 0; buf[i] != 0; i++) {
			if (buf[i] == '\t') {
				if (j == 0)
					j = i + 3;
				else if (k == 0) {
					k = i + 3;
					break;
				}
			}
		}
		eu = strtol(buf + j, NULL, 16);
		uc = strtol(buf + k, &p, 16);
		if (*p == '+')
			uc = uc * 65536 + strtol(p + 1, &p, 16);  /*  Two-character pair  */
		if (uc >= 65536) {
			count++;
			printf("%d: Cannot convert: %s", count, buf);
			continue;
		}
		if (eu >= 0x8fa1a1) {
			if (eu >= 0x8feea1) {
				idx = 10434 + (((eu >> 8) & 0xff) - 0xee) * 94 + ((eu & 0xff) - 0xa1);
			} else {
				idx = 9024 + (((eu >> 8) & 0xff) - 0xa1) * 94 + ((eu & 0xff) - 0xa1);
			}
		} else if (eu >= 0xa1a1) {
			idx = 188 + (((eu >> 8) & 0xff) - 0xa1) * 94 + ((eu & 0xff) - 0xa1);
		} else if (eu >= 0x8ea1) {
			idx = 94 + ((eu & 0xff) - 0xa1);
		} else if (eu >= 0x21 && eu <= 0x7e) {
			idx = eu - 0x21;
		} else continue; /* Silently skip */
		sConvTable[uc] = idx;
	}
	fclose(fp3);
	memset(sFontData, 0, sizeof(sFontData));
	memset(sKanjiData, 0, sizeof(sKanjiData));
	for (uc = 0; uc < 65536; uc++) {
		idx = sConvTable[uc];
		if (idx == 0xffff)
			continue;
		ch[0] = uc;
		s = [[NSString alloc] initWithCharacters:ch length:1];
		memset(sBitmapBuffer, 0, sizeof(sBitmapBuffer[0]) * 1024);
		[s drawWithRect:NSMakeRect(0, 0, 16, 32) options:NSStringDrawingUsesLineFragmentOrigin attributes:attr];
		[s release];
		for (yy = 0; yy < 16; yy++) {
			b = 0;
			if (idx < 188) {
				for (xx = 0; xx < 8; xx++) {
					if (sBitmapBuffer[(yy + 6) * 32 + xx] != 0)
						b |= (1 << xx);
				}
				sFontData[idx * 16 + yy] = b;
			} else {
				for (xx = 0; xx < 16; xx++) {
					if (sBitmapBuffer[(yy + 6) * 32 + xx] != 0)
						b |= (1 << xx);
				}
				j = (idx - 188) * 32 + yy * 2;
				sKanjiData[j] = b & 0xff;
				sKanjiData[j + 1] = (b >> 8) & 0xff;
			}
		}
	}
	for (idx = 0; idx < 12032; idx++) {
		if (idx < 188) {
			i = idx % 8;
			if (i == 0) {
				for (j = 0; j < 8; j++) {
					k = j + idx;
					if (k < 94)
						eu = k + 0x21;
					else
						eu = k + 0x8ea1;
					fprintf(fp2, "%04X%s", eu, (j == 7 ? "\n" : "     "));
				}
				memset(buf2, ' ', sizeof(buf2));
			}
			for (yy = 0; yy < 16; yy++) {
				b = sFontData[idx * 16 + yy];
				for (xx = 0; xx < 8; xx++) {
					buf2[yy][i * 9 + xx] = ((b & (1 << xx)) != 0 ? '*' : ' ');
				}
			}
			if (i == 7 || idx == 187) {
				for (yy = 0; yy < 16; yy++) {
					buf2[yy][70] = '\n';
					buf2[yy][71] = 0;
					fputs(buf2[yy], fp2);
				}
			}
		} else {
			i = (idx - 188) % 4;
			if (i == 0) {
				for (j = 0; j < 4; j++) {
					k = j + idx;
					if (k < 9024) {
						eu = 0xa1a1 + ((k - 188) / 94) * 256 + (k - 188) % 94;
						fprintf(fp2, "%04X%s", eu, (j == 3 ? "\n" : "             "));
					} else {
						if (idx < 10434)
							eu = 0x8fa1a1 + ((k - 9024) / 94) * 256 + (k - 9024) % 94;
						else
							eu = 0x8feea1 + ((k - 10434) / 94) * 256 + (k - 10434) % 94;
						fprintf(fp2, "%06X%s", eu, (j == 3 ? "\n" : "           "));
					}
				}
				memset(buf2, ' ', sizeof(buf2));
			}
			for (yy = 0; yy < 16; yy++) {
				b = sKanjiData[(idx - 188) * 32 + yy * 2] + (sKanjiData[(idx - 188) * 32 + yy * 2 + 1] * 256);
				for (xx = 0; xx < 16; xx++) {
					buf2[yy][i * 17 + xx] = ((b & (1 << xx)) != 0 ? '*' : ' ');
				}
			}
			if (i == 3 || idx == 12031) {
				for (yy = 0; yy < 16; yy++) {
					buf2[yy][70] = '\n';
					buf2[yy][71] = 0;
					fputs(buf2[yy], fp2);
				}
			}
		}
	}
	[NSGraphicsContext restoreGraphicsState];
	fclose(fp2);
	
	/*  Write the font data to file  */
	fp2 = fopen("fontdata.c", "w");
	fprintf(fp2, "unsigned short gConvTable[%d] = {\n  ", (int)sizeof(sConvTable)/2);
	for (uc = 0; uc < 65536; uc++) {
		fprintf(fp2, "%d", sConvTable[uc]);
		if (uc == 65535)
			fputs("\n};\n", fp2);
		else if (uc % 16 == 15)
			fputs(",\n  ", fp2);
		else fputc(',', fp2);		
	}
	fprintf(fp2, "unsigned char gFontData[%d] = {\n  ", (int)sizeof(sFontData));
	for (i = 0; i < sizeof(sFontData); i++) {
		fprintf(fp2, "%d", sFontData[i]);
		if (i == sizeof(sFontData) - 1)
			fputs("\n};\n", fp2);
		else if (i % 16 == 15)
			fputs(",\n  ", fp2);
		else fputc(',', fp2);
	}
	fprintf(fp2, "unsigned char gKanjiData[%d] = {\n  ", (int)sizeof(sKanjiData));
	for (i = 0; i < sizeof(sKanjiData); i++) {
		fprintf(fp2, "%d", sKanjiData[i]);
		if (i == sizeof(sKanjiData) - 1)
			fputs("\n};\n", fp2);
		else if (i % 32 == 31)
			fputs(",\n  ", fp2);
		else fputc(',', fp2);
	}
	fclose(fp2);
	fp1 = fopen("fontdata.bin", "wb");
	
	i = NSSwapHostIntToLittle(sizeof(sConvTable) / 2);
	fwrite(&i, 1, 4, fp1);
	i = NSSwapHostIntToLittle(sizeof(sFontData));
	fwrite(&i, 1, 4, fp1);
	i = NSSwapHostIntToLittle(sizeof(sKanjiData));
	fwrite(&i, 1, 4, fp1);
	
	for (uc = 0; uc < 65536; uc++) {
		/*  Convert to little endian  */
		u_int16_t si;
		((char *)(&si))[0] = sConvTable[uc] & 0xff;
		((char *)(&si))[1] = (sConvTable[uc] >> 8) & 0xff;
	}
	fwrite(sConvTable, 1, sizeof(sConvTable), fp1);
	fwrite(sFontData, 1, sizeof(sFontData), fp1);
	fwrite(sKanjiData, 1, sizeof(sKanjiData), fp1);
	fclose(fp1);
	
}
#endif

- (void)awakeFromNib
{
	NSRect frame = [self frame];
	
	my_width = frame.size.width - 8;
	my_height = frame.size.height - 8;
	
	my_width_gmode0 = my_width;
	my_height_gmode0 = my_height;
	
	s_graphView = self;
	
	[self setBounds:NSMakeRect(-4, -4, my_width + 8, my_height + 8)];
	[self setNeedsDisplay:YES];
	
#if CREATE_FONT
	[self createFontData];
#endif

}

- (void)drawRect:(NSRect)dirtyRect
{
	lm_draw_platform(&dirtyRect);
}

//  Key inputs
#define kKeyBufferMax 128
static u_int16_t sKeyBuffer[kKeyBufferMax];
static int sKeyBufferBase, sKeyBufferCount;

- (BOOL)acceptsFirstResponder
{
	return YES;
}

static void
s_RegisterKeyDown(u_int16_t *ch)
{
	while (*ch != 0) {
		if (sKeyBufferCount < kKeyBufferMax) {
			sKeyBuffer[(sKeyBufferBase + sKeyBufferCount) % kKeyBufferMax] = *ch;
			sKeyBufferCount++;
		}
		ch++;
	}
}

/*  TODO: is it possible to interface with anthy for Japanese input?  */
/*  (https://osdn.jp/projects/anthy/)  */

- (void)keyDown: (NSEvent *)theEvent
{
	unichar ch[6];
	ch[0] = [[theEvent characters] characterAtIndex:0];
	ch[1] = 0;
	switch (ch[0]) {
		case NSUpArrowFunctionKey: ch[0] = 0x1b; ch[1] = '['; ch[2] = 'A'; ch[3] = 0; break;
		case NSDownArrowFunctionKey: ch[0] = 0x1b; ch[1] = '['; ch[2] = 'B'; ch[3] = 0; break;
		case NSLeftArrowFunctionKey: ch[0] = 0x1b; ch[1] = '['; ch[2] = 'D'; ch[3] = 0; break;
		case NSRightArrowFunctionKey: ch[0] = 0x1b; ch[1] = '['; ch[2] = 'C'; ch[3] = 0; break;
		case NSDeleteFunctionKey: ch[0] = 127; break;
		case NSBreakFunctionKey: ch[0] = 27; break;
		default: break;
	}
	s_RegisterKeyDown(ch);
}

@end

#if 0
#pragma mark ====== LuaCon Interface ======
#endif

int
lm_init_screen_platform(void)
{
	//  my_width_gmode0 and my_height_gmode0 should be set before calling this
	CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
	s_text_pixels = (pixel_t *)calloc(sizeof(pixel_t), my_width_gmode0 * my_height_gmode0);
	s_graphic_pixels = (pixel_t *)calloc(sizeof(pixel_t), my_width_gmode0 * my_height_gmode0);
	s_text_context = CGBitmapContextCreate(s_text_pixels,
										   my_width_gmode0, my_height_gmode0,
										   8, 4 * my_width_gmode0, space,
										   kCGImageAlphaPremultipliedLast);
	s_graphic_context = CGBitmapContextCreate(s_graphic_pixels,
											  my_width_gmode0, my_height_gmode0,
											  8, 4 * my_width_gmode0, space,
											  kCGImageAlphaPremultipliedLast);
	CGColorSpaceRelease(space);
	return 0;
}

void
lm_lock(void)
{
}

void
lm_unlock(void)
{
}

/*  Request redraw of a part of the screen  */
void
lm_redraw(int16_t x, int16_t y, int16_t width, int16_t height)
{
	NSRect aRect = NSMakeRect(x, y, width, height);
	lm_lock();
	[s_graphView setNeedsDisplayInRect:aRect];
	lm_unlock();
}

void
lm_update_screen(void)
{
	[s_graphView displayIfNeeded];
}

void
lm_draw_platform(void *ref)
{
	CGContextRef cref = [[NSGraphicsContext currentContext] graphicsPort];
	CGRect r;
	CGColorRef color;
	CGImageRef image;
	NSRect dirtyRect = *((NSRect *)ref);
	
	color = CGColorCreateGenericRGB(0, 0, 0, 1);
	CGContextSetFillColorWithColor(cref, color);
	r = CGRectMake(dirtyRect.origin.x, dirtyRect.origin.y, dirtyRect.size.width, dirtyRect.size.height);
	CGContextFillRect(cref, r);
	CGColorRelease(color);
	if (s_text_context == (CGContextRef)0)
		return;
	CGContextSaveGState(cref);
	r = CGRectMake(0, 0, my_width_gmode0, my_height_gmode0);
	
	/*  Draw graphic layer  */
	image = CGBitmapContextCreateImage(s_graphic_context);
	CGContextDrawImage(cref, r, image);
	CGImageRelease(image);
	
	/*  Draw text layer  */
	CGContextBeginTransparencyLayer(cref, NULL);
	image = CGBitmapContextCreateImage(s_text_context);
	CGContextDrawImage(cref, r, image);
	CGImageRelease(image);
	if (my_show_cursor) {
		r.origin.x = (my_cursor_x + my_cursor_xofs) * 8;
		r.origin.y = my_height - (my_cursor_y + my_cursor_yofs + 1) * 16;
		r.size.width = 8;
		r.size.height = 16;
		color = CGColorCreateGenericRGB(1, 1, 1, 0.6);
		CGContextSetFillColorWithColor(cref, color);
		CGContextFillRect(cref, r);
		CGColorRelease(color);
	}
	CGContextEndTransparencyLayer(cref);
	
	CGContextRestoreGState(cref);
}

/*  Get the key input.   */
int
lm_getch_with_timeout(int32_t usec)
{
	int ch;
	NSEvent *event;
	if (usec < 0) {
		usec = 0;
		/*  Scan only  */
	/*	event = [NSApp nextEventMatchingMask:NSKeyDownMask untilDate:nil inMode:NSModalPanelRunLoopMode dequeue:NO];
		if (event != nil)
			return [[event characters] characterAtIndex:0];
		else return -1; */
	}
	while ((event = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:nil inMode:NSDefaultRunLoopMode dequeue:YES]) != NULL) {
		[NSApp sendEvent:event];
	}
	if (sKeyBufferCount == 0 && usec > 0) {
		NSDate *date = [NSDate dateWithTimeIntervalSinceNow:usec / 1000000.0];
		NSEvent *event = [NSApp nextEventMatchingMask:NSKeyDownMask untilDate:date inMode:NSDefaultRunLoopMode dequeue:YES];
		if (event != nil) {
			[NSApp sendEvent:event];
		}
	}
	if (sKeyBufferCount > 0) {
		ch = sKeyBuffer[sKeyBufferBase];
		sKeyBufferBase = (sKeyBufferBase + 1) % kKeyBufferMax;
		sKeyBufferCount--;
	} else ch = -1;
	return ch;
}

int
lm_select_active_buffer(int active)
{
	if (active == TEXT_ACTIVE) {
		s_active_context = s_text_context;
	} else if (active == GRAPHIC_ACTIVE) {
		s_active_context = s_graphic_context;
	} else {
		/*  None  */
	}
	return 0;
}

int
tty_puts(const char *s)
{
	fputs(s, stderr);
	return 0;
}

int
lm_gmode_platform(int gmode)
{
	if (gmode == 1) {
		int ww, wh;
		my_width = 320;
		my_height = 240;
		ww = my_width_gmode0 / 2 + 4;
		wh = my_height_gmode0 / 2 + 4;
		[s_graphView setBounds:NSMakeRect((my_width - ww) / 2, (my_height - wh) / 2, ww, wh)];
	} else {
		my_width = my_width_gmode0;
		my_height = my_height_gmode0;
		[s_graphView setBounds:NSMakeRect(-4, -4, my_width + 8, my_height + 8)];
	}
	[s_graphView setNeedsDisplay:YES];
	return 0;
}

const char *
lm_platform_name(void)
{
	return "mac";
}

int
lua_setup_platform(lua_State *L)
{
	/*  Nothing to do  */
	return 0;
}

#if 0
#pragma mark ====== Drawing primitives ======
#endif

void
lm_clear_box(int x, int y, int width, int height)
{
	CGRect r = {x, y, width, height};
	CGContextClearRect(s_active_context, r);
	lm_redraw(x, y, width, height);
}

void
lm_put_pattern(const pixel_t *p, int x, int y, int width, int height)
{
	int i, j, x1, x2, y1, y2, d1, d2;
	pixel_t *pd;
	x1 = x;
	y1 = y;
	x2 = x + width;
	y2 = y + height;
	if (x1 < 0)
		x1 = 0;
	if (y1 < 0)
		y1 = 0;
	if (x2 >= my_width)
		x2 = my_width;
	if (y2 >= my_height)
		y2 = my_height;
	if (x1 >= x2 || y1 >= y2)
		return;
	d1 = width - (x2 - x1);
	d2 = my_width_gmode0 - (x2 - x1);
	pd = (s_active_context == s_text_context ? s_text_pixels : s_graphic_pixels);
	pd += my_width_gmode0 * (my_height_gmode0 - y2) + x1;
	for (j = y1; j < y2; j++) {
		for (i = x1; i < x2; i++) {
			*pd++ = *p++;
		}
		p += d1;
		pd += d2;
	}
	lm_redraw(x1, y1, x2 - x1, y2 - y1);
}

void
lm_get_pattern(pixel_t *p, int x, int y, int width, int height)
{
	int i, j, x1, x2, y1, y2, d1, d2;
	pixel_t *pd;
	x1 = x;
	y1 = y;
	x2 = x + width;
	y2 = y + height;
	if (x1 < 0)
		x1 = 0;
	if (y1 < 0)
		y1 = 0;
	if (x2 >= my_width)
		x2 = my_width;
	if (y2 >= my_height)
		y2 = my_height;
	if (x1 >= x2 || y1 >= y2)
		return;
	d1 = width - (x2 - x1);
	d2 = my_width_gmode0 - (x2 - x1);
	pd = (s_active_context == s_text_context ? s_text_pixels : s_graphic_pixels);
	pd += my_width_gmode0 * y1 + x1;
	memset(p, 0, width * height * sizeof(pixel_t));
	for (j = y1; j < y2; j++) {
		for (i = x1; i < x2; i++) {
			*p++ = *pd++;
		}
		pd += d2;
	}
}

void
lm_copy_pixels(int dx, int dy, int sx, int sy, int width, int height)
{
	pixel_t *basep = (s_active_context == s_text_context ? s_text_pixels : s_graphic_pixels);
	int w1, h1, yy, sy1, dy1;
	pixel_t *srcp, *dstp;
	w1 = width;
	if (sx < 0) {
		w1 += sx;
		dx -= sx;
		sx = 0;
	}
	if (dx < 0) {
		w1 += dx;
		sx -= dx;
		dx = 0;
	}
	if (sx + w1 > my_width)
		w1 = my_width - sx;
	if (dx + w1 > my_width)
		w1 = my_width - dx;
	h1 = height;
	if (sy < 0) {
		h1 += sy;
		dy -= sy;
		sy = 0;
	}
	if (dy < 0) {
		h1 += dy;
		sy -= dy;
		dy = 0;
	}
	if (sy + h1 > my_height)
		h1 = my_height - sy;
	if (dy + h1 > my_height)
		h1 = my_height - dy;
	if (sy < 0 || dy < 0)
		return;
	/*  The origin of the bitmap is left-top  */
	sy1 = my_height_gmode0 - height - sy;
	dy1 = my_height_gmode0 - height - dy;
	if (sy1 < dy1) {
		/*  Down scroll: move bottom to top  */
		for (yy = h1 - 1; yy >= 0; yy--) {
			srcp = basep + sx + (sy1 + yy) * my_width_gmode0;
			dstp = basep + dx + (dy1 + yy) * my_width_gmode0;
			memmove(dstp, srcp, w1 * sizeof(pixel_t));
		}
	} else {
		/*  Up scroll: move top to bottom  */
		for (yy = 0; yy < h1; yy++) {
			srcp = basep + sx + (sy1 + yy) * my_width_gmode0;
			dstp = basep + dx + (dy1 + yy) * my_width_gmode0;
			memmove(dstp, srcp, w1 * sizeof(pixel_t));
		}
	}
	lm_redraw(sx, sy, w1, h1);	
	lm_redraw(dx, dy, w1, h1);	
}

#if 0
#pragma mark ====== Path implementation ======
#endif

static CGMutablePathRef *s_paths = NULL;
int max_number_of_paths = 1024;

int
lm_create_path(void)
{
	int i;
	if (s_paths == NULL) {
		s_paths = (CGMutablePathRef *)malloc(sizeof(CGPathRef) * max_number_of_paths);
		if (s_paths == NULL)
			return -1;  /*  Cannot allocate  */
		for (i = 0; i < max_number_of_paths; i++)
			s_paths[i] = NULL;
	}
	for (i = 0; i < max_number_of_paths; i++) {
		if (s_paths[i] == nil) {
			CGMutablePathRef path = CGPathCreateMutable();
			if (path == NULL)
				return -1;  /*  Cannot allocate  */
			s_paths[i] = path;
			return i;
		}
	}
	return -1;  /*  Cannot allocate  */
}

int
lm_destroy_path(int idx)
{
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == NULL)
		return -1;  /*  Invalid index  */
	CGPathRelease(s_paths[idx]);
	s_paths[idx] = NULL;
	return idx;
}

int
lm_path_moveto(int idx, float x, float y)
{
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == NULL)
		return -1;  /*  Invalid index  */
	CGPathMoveToPoint(s_paths[idx], NULL, x, y);
	return 0;
}

int
lm_path_lineto(int idx, float x, float y)
{
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == NULL)
		return -1;  /*  Invalid index  */
	CGPathAddLineToPoint(s_paths[idx], NULL, x, y);
	return 0;
}

int
lm_path_arc(int idx, float cx, float cy, float st, float et, float ra, float rb, float rot)
{
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == NULL)
		return -1;  /*  Invalid index  */
	st *= (3.1415926536 / 180.0);
	et *= (3.1415926536 / 180.0);
	rot *= (3.1415926536 / 180.0);
	if (ra == rb) {
		CGPathAddArc(s_paths[idx], NULL, cx, cy, ra, st + rot, et + rot, 0);
	} else {
		float sinr, cosr;
		sinr = sin(rot);
		cosr = cos(rot);
		CGAffineTransform m = CGAffineTransformMake(ra * cosr, ra * sinr, -rb * sinr, rb * cosr, cx, cy);
		CGPathAddArc(s_paths[idx], &m, 0, 0, 1, st, et, 0);
	}
	return 0;
}

int
lm_path_cubic(int idx, float x1, float y1, float x2, float y2, float x3, float y3)
{
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == NULL)
		return -1;  /*  Invalid index  */
	CGPathAddCurveToPoint(s_paths[idx], NULL, x1, y1, x2, y2, x3, y3);
	return 0;
}

int
lm_path_close(int idx)
{
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == NULL)
		return -1;  /*  Invalid index  */
	CGPathCloseSubpath(s_paths[idx]);
	return 0;	
}

int
lm_path_line(int idx, float x1, float y1, float x2, float y2)
{
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == NULL)
		return -1;  /*  Invalid index  */
	CGPathMoveToPoint(s_paths[idx], NULL, x1, y1);
	CGPathAddLineToPoint(s_paths[idx], NULL, x2, y2);
	return 0;
}

int
lm_path_rect(int idx, float x, float y, float width, float height)
{
	CGRect r;
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == NULL)
		return -1;  /*  Invalid index  */
	r = CGRectMake(x, y, width, height);
	CGPathAddRect(s_paths[idx], NULL, r);
	return 0;
}

int
lm_path_roundrect(int idx, float x, float y, float width, float height, float rx, float ry)
{
	CGAffineTransform m;
	CGMutablePathRef path;
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == NULL)
		return -1;  /*  Invalid index  */
	path = s_paths[idx];
	if (rx == ry)
		m = CGAffineTransformMake(1, 0, 0, 1, x, y);
	else {
		float r = ry / rx;
		m = CGAffineTransformMake(1, 0, 0, r, x, y);
		height /= r;
	}
	CGPathMoveToPoint(path, &m, rx, 0);
	CGPathAddArc(path, &m, width - rx, rx, rx, -3.1415926537/2, 0, 0);
	CGPathAddArc(path, &m, width - rx, height - rx, rx, 0, 3.1415926537/2, 0);
	CGPathAddArc(path, &m, rx, height - rx, rx, 3.1415926537/2, 3.1415926537, 0);
	CGPathAddArc(path, &m, rx, rx, rx, 3.1415926537, 3.1415926537*3/2, 0);
	CGPathCloseSubpath(path);
	return 0;
}

int
lm_path_ellipse(int idx, float x, float y, float rx, float ry)
{
	CGRect r;
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == NULL)
		return -1;  /*  Invalid index  */
	r = CGRectMake(x - rx, y - ry, rx * 2, ry * 2);
	CGPathAddEllipseInRect(s_paths[idx], NULL, r);
	return 0;
}

int
lm_path_fan(int idx, float cx, float cy, float st, float et, float ra, float rb, float rot)
{
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == NULL)
		return -1;  /*  Invalid index  */
	CGPathMoveToPoint(s_paths[idx], NULL, cx, cy);
	lm_path_arc(idx, cx, cy, st, et, ra, rb, rot);
	CGPathCloseSubpath(s_paths[idx]);
	return 0;
}

int
lm_set_stroke_color(pixel_t col, int enable)
{
	if (enable == 0)
		s_path_drawflag &= ~1;
	else {
		CGColorRef cref;
		s_path_drawflag |= 1;
		lm_select_active_buffer(GRAPHIC_ACTIVE);
		if (enable != -1)
			s_stroke_color = col;
		cref = CGColorCreateGenericRGB(REDCOMPONENT(s_stroke_color),
									   GREENCOMPONENT(s_stroke_color),
									   BLUECOMPONENT(s_stroke_color),
									   ALPHACOMPONENT(s_stroke_color));
		CGContextSetStrokeColorWithColor(s_active_context, cref);
		CGColorRelease(cref);
	}
	return 0;
}

int
lm_set_fill_color(pixel_t col, int enable)
{
	if (enable == 0)
		s_path_drawflag &= ~2;
	else {
		CGColorRef cref;
		s_path_drawflag |= 2;
		lm_select_active_buffer(GRAPHIC_ACTIVE);
		if (enable != -1)
			s_fill_color = col;
		cref = CGColorCreateGenericRGB(REDCOMPONENT(s_fill_color),
									   GREENCOMPONENT(s_fill_color),
									   BLUECOMPONENT(s_fill_color),
									   ALPHACOMPONENT(s_fill_color));
		CGContextSetFillColorWithColor(s_active_context, cref);
		CGColorRelease(cref);
	}
	return 0;
}

int
lm_draw_path(int idx)
{
	CGPathDrawingMode mode;
	CGRect r;
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == NULL)
		return -1;  /*  Invalid index  */
	lm_select_active_buffer(GRAPHIC_ACTIVE);
	switch (s_path_drawflag & 3) {
		case 0: return 0;  /*  No drawing  */
		case 1: mode = kCGPathStroke; break;
		case 2: mode = kCGPathFill; break;
		case 3: mode = kCGPathFillStroke; break;
	}
	CGContextAddPath(s_active_context, s_paths[idx]);
	CGContextDrawPath(s_active_context, mode);
	r = CGPathGetBoundingBox(s_paths[idx]);
	lm_redraw(r.origin.x, r.origin.y, r.size.width, r.size.height);
	return 0;
}
