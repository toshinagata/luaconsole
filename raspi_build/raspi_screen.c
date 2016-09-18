/*
 *  raspi_screen.c
 *  LuaConsole
 *
 *  Created by Toshi Nagata on 2016/02/07.
 *  Copyright 2016 Toshi Nagata.  All rights reserved.
 *
 */

#include "screen.h"
#include "graph.h"

#include <assert.h>

#include <pthread.h>
#include <math.h>

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <VG/openvg.h>
#include <VG/vgu.h>
#include <bcm_host.h>

#define SHOW_IF_VGERROR do { int n = vgGetError(); if (n != 0) fprintf(stderr, "vg Error %04x on %s:%d\r\n", n, __FILE__, __LINE__); } while (0)

/*  Clear the screen controlled by the fb device  */
static void
s_clear_framebuffer(void)
{
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	int fbfd;
	char *framebuffer;
	
	/*  Open frame buffer device  */
	fbfd = open("/dev/fb0", O_RDWR);
	if (!fbfd) {
		fprintf(stderr, "Error: cannot open framebuffer device.\n");
		return;
	}
	if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
		fprintf(stderr, "Error reading variable information.\n");
		return;
	}
/*	fprintf(stderr, "Frame buffer %dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel ); */
	
	if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
		printf("Error reading fixed information.\n");
		return;
	}
	
	/*  Map the memory buffer  */
	framebuffer = (char *)mmap(0, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
	if (framebuffer == (char *)-1) {
		fprintf(stderr, "Failed to mmap.\n");
		return;
	}
	
	/*  Fill by zeros  */
	memset(framebuffer, 0, vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8);

	/*  Clean up  */
	munmap(framebuffer, finfo.smem_len);
	close(fbfd);
}

/*  Physical screen  */
DISPMANX_ELEMENT_HANDLE_T dispman_element;
DISPMANX_DISPLAY_HANDLE_T dispman_display;
DISPMANX_UPDATE_HANDLE_T dispman_update;

EGLDisplay egl_display;
EGLConfig egl_config;

/*  s: physical screen, g: graphic, t: text  */
EGLContext s_context, g_context, t_context;
EGLSurface s_surface, g_surface, t_surface;
VGImage g_image = VG_INVALID_HANDLE, t_image = VG_INVALID_HANDLE;

/*  c: composite offscreen image (for gmode = 1)  */
EGLContext c_context;
EGLSurface c_surface;
VGImage c_image = VG_INVALID_HANDLE;

/*  0: text, 1: graphic, 2: physical screen  */
int active_surface;

VGfloat clear_color[4] = {0, 0, 0, 0};

EGL_DISPMANX_WINDOW_T g_nativewindow;

/*  Support for Secondary screen (eg. PiTFT etc.)  */
static DISPMANX_RESOURCE_HANDLE_T s_screen_resource = DISPMANX_NO_HANDLE;
static u_int32_t s_image_prt;
static VC_RECT_T s_rect1;
static int s_fbfd = -1;
static char *s_fbp = NULL;
static struct fb_var_screeninfo s_vinfo;
static struct fb_fix_screeninfo s_finfo;

#if 0
#pragma mark ====== EGL/Dispmanx initialization ======
#endif

void
init_egl(void)
{
    EGLint num_configs;
    EGLBoolean result;
	
    static const EGLint attribute_list[] =
	{
	    EGL_RED_SIZE, 8,
	    EGL_GREEN_SIZE, 8,
	    EGL_BLUE_SIZE, 8,
	    EGL_ALPHA_SIZE, 8,
	    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	    EGL_RENDERABLE_TYPE, EGL_OPENVG_BIT,
	    EGL_NONE
	};
	
/*   static const EGLint context_attributes[] =
	{
	    EGL_CONTEXT_CLIENT_VERSION, 2,
	    EGL_NONE
	}; */
	
    // get an EGL display connection
    egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	
    // initialize the EGL display connection
    result = eglInitialize(egl_display, NULL, NULL);
	
    // get an appropriate EGL frame buffer configuration
    result = eglChooseConfig(egl_display, attribute_list, &egl_config, 1, &num_configs);
    assert(EGL_FALSE != result);
	
    //result = eglBindAPI(EGL_OPENGL_ES_API);
    result = eglBindAPI(EGL_OPENVG_API);
    assert(EGL_FALSE != result);
	
    // create an EGL rendering context
    s_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, NULL);

	// breaks if we use this: context_attributes);
    assert(s_context!=EGL_NO_CONTEXT);
}

void
init_dispmanx(EGL_DISPMANX_WINDOW_T *nativewindow)
{   
    int32_t success = 0;   
    uint32_t screen_width;
    uint32_t screen_height;
	
    VC_RECT_T dst_rect;
    VC_RECT_T src_rect;
	
    bcm_host_init();
	
    // create an EGL window surface
    success = graphics_get_display_size(0 /* LCD */, 
										&screen_width, 
										&screen_height);
    assert( success >= 0 );
	
    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.width = screen_width;
    dst_rect.height = screen_height;
	
    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = screen_width << 16;
    src_rect.height = screen_height << 16;        
	
    dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
    dispman_update = vc_dispmanx_update_start( 0 );
	
    dispman_element = 
	vc_dispmanx_element_add(dispman_update, dispman_display,
							0/*layer*/, &dst_rect, 0/*src*/,
							&src_rect, DISPMANX_PROTECTION_NONE, 
							0 /*alpha*/, 0/*clamp*/, 0/*transform*/);
	
    // Build an EGL_DISPMANX_WINDOW_T from the Dispmanx window
    nativewindow->element = dispman_element;
    nativewindow->width = screen_width;
    nativewindow->height = screen_height;
    vc_dispmanx_update_submit_sync(dispman_update);
	
}

void
egl_from_dispmanx(EGL_DISPMANX_WINDOW_T *nativewindow)
{
    EGLBoolean result;
	
    s_surface = eglCreateWindowSurface(egl_display, egl_config, nativewindow, NULL );
    assert(s_surface != EGL_NO_SURFACE);
	
    // connect the context to the surface
    result = eglMakeCurrent(egl_display, s_surface, s_surface, s_context);
    assert(EGL_FALSE != result);
}

#if 0
#pragma mark ====== Thread control ======
#endif

int inval_x1, inval_x2, inval_y1, inval_y2;

void
lm_lock(void)
{
//	pthread_mutex_lock(&lm_mutex);
}

void
lm_unlock(void)
{
//	pthread_mutex_unlock(&lm_mutex);
}

/*  Select active screen  */
int
lm_select_active_buffer_nolock(int active)
{
	int result = 0;
	if (active_surface != active) {
		active_surface = active;
		switch (active) {
			case PHYSICAL_ACTIVE:
				if (my_graphic_mode == 1)
					result = eglMakeCurrent(egl_display, c_surface, c_surface, c_context);
				else
					result = eglMakeCurrent(egl_display, s_surface, s_surface, s_context);
				break;
			case GRAPHIC_ACTIVE:
				result = eglMakeCurrent(egl_display, g_surface, g_surface, g_context);
				break;
			case TEXT_ACTIVE:
				result = eglMakeCurrent(egl_display, t_surface, t_surface, t_context);
				break;
		}
	}
	return result;
}

int
lm_select_active_buffer(int active)
{
	int result;
	lm_lock();
	result = lm_select_active_buffer_nolock(active);
	lm_unlock();
	return result;
}

/*  Request redraw of a part of the screen  */
static inline void
lm_redraw_nolock(int16_t x, int16_t y, int16_t width, int16_t height)
{
	int x2, y2;
	x2 = x + width;
	y2 = y + height;
	if (x < inval_x1)
		inval_x1 = x;
	if (y < inval_y1)
		inval_y1 = y;
	if (x2 > inval_x2)
		inval_x2 = x2;
	if (y2 > inval_y2)
		inval_y2 = y2;
}

void
lm_redraw(int16_t x, int16_t y, int16_t width, int16_t height)
{
	lm_lock();
	lm_redraw_nolock(x, y, width, height);
	lm_unlock();
}

static VGfloat s_col[4] = {0, 0, 0, 0};
static VGPaint s_paint;

void
lm_draw_platform(void *ref)
{
/*	int format;  */
	int dx1, dy1, dx2, dy2;

	lm_lock();
	dx1 = inval_x1;
	dy1 = inval_y1;
	dx2 = inval_x2;
	dy2 = inval_y2;
	if (dx1 < 0)
		dx1 = 0;
	if (dx2 >= my_width)
		dx2 = my_width - 1;
	if (dy1 < 0)
		dy1 = 0;
	if (dy2 >= my_height)
		dy2 = my_height - 1;
	if (dx1 >= dx2 || dy1 >= dy2 || dx1 >= my_width || dy1 >= my_height || dx1 < 0 || dy1 < 0) {
		lm_unlock();
		return;
	}
	
/*
#if BYTES_PER_PIXEL == 2
	format = VG_sRGB_565;
#else
	format = VG_sRGBA_8888;
#endif
*/

	lm_select_active_buffer_nolock(PHYSICAL_ACTIVE);
	vgSeti(VG_IMAGE_MODE, VG_DRAW_IMAGE_NORMAL);
	
	/*  Clear screen  */
	s_col[0] = s_col[1] = s_col[2] = s_col[3] = 0.0;
	vgSetfv(VG_CLEAR_COLOR, 4, s_col);
	vgClear(0, 0, my_width, my_height);
	
	/*  Draw text screen  */
	vgDrawImage(t_image);
	
	/*  Draw graphic screen (under the graphic screen; it looks like VG_BLEND_SRC_OVER does not work?)  */
	vgSetPaint(s_paint, VG_FILL_PATH);		
	vgSeti(VG_IMAGE_MODE, VG_DRAW_IMAGE_STENCIL);
	vgSeti(VG_BLEND_MODE, VG_BLEND_DST_OVER);
	vgDrawImage(g_image);
	
	if (my_show_cursor) {
		VGPaint cpaint;
		VGPath cpath = vgCreatePath(VG_PATH_FORMAT_STANDARD,
									VG_PATH_DATATYPE_S_16,
									1, 0, 5, 10, VG_PATH_CAPABILITY_APPEND_TO);
		vguRect(cpath, (my_cursor_x + my_cursor_xofs) * 8, my_height - (my_cursor_y + my_cursor_yofs + 1) * 16, 8, 16);
		cpaint = vgCreatePaint();
		vgSetColor(cpaint, 0x808080ff);
		vgSetPaint(cpaint, VG_FILL_PATH);
		vgDrawPath(cpath, VG_FILL_PATH);
		vgDestroyPaint(cpaint);
		vgDestroyPath(cpath);
	}

	/*  Show contents  */
	if (my_graphic_mode == 1) {
		/*  Copy screen to the frame buffer  */
		vgReadPixels(s_fbp + (my_height - 1) * my_width * 2, -my_width * 2, VG_sRGB_565, 0, 0, my_width, my_height);
	} else {
		/*  Swap double buffer  */
		eglSwapBuffers(egl_display, s_surface);		
	}
	
	inval_x1 = my_width;
	inval_x2 = 0;
	inval_y1 = my_height;
	inval_y2 = 0;
	lm_unlock();
}

void
lm_update_screen(void)
{
	lm_draw_platform(NULL);
}

static int
s_create_offscreen_images(int width, int height, int gmode)
{
	static EGLint pbuffer_attribute_list[] =
	{
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENVG_BIT,
		EGL_NONE
	};
	int num_configs;
	VGfloat col[4];
	
	col[0] = col[1] = col[2] = col[3] = 0.0;
	vgSetfv(VG_CLEAR_COLOR, 4, col);
	
	if (g_image != VG_INVALID_HANDLE) {
		eglDestroySurface(egl_display, g_surface);
		eglDestroyContext(egl_display, g_context);
		vgDestroyImage(g_image);
		eglDestroySurface(egl_display, t_surface);
		eglDestroyContext(egl_display, t_context);
		vgDestroyImage(t_image);
	}
	if (c_image != VG_INVALID_HANDLE) {
		eglDestroySurface(egl_display, c_surface);
		eglDestroyContext(egl_display, c_context);
		vgDestroyImage(c_image);
	}
	
	g_image = vgCreateImage(VG_sRGBA_8888, width, height, VG_IMAGE_QUALITY_FASTER);
	vgClearImage(g_image, 0, 0, width, height);
	eglChooseConfig(egl_display, pbuffer_attribute_list, &egl_config, 1, &num_configs);
	g_context = eglCreateContext(egl_display, egl_config, s_context, NULL);
	g_surface = eglCreatePbufferFromClientBuffer(egl_display, EGL_OPENVG_IMAGE,
												 (EGLClientBuffer)g_image, egl_config, NULL);
	t_image = vgCreateImage(VG_sRGBA_8888, width, height, VG_IMAGE_QUALITY_FASTER);
	vgClearImage(t_image, 0, 0, width, height);
	eglChooseConfig(egl_display, pbuffer_attribute_list, &egl_config, 1, &num_configs);
	t_context = eglCreateContext(egl_display, egl_config, s_context, NULL);
	t_surface = eglCreatePbufferFromClientBuffer(egl_display, EGL_OPENVG_IMAGE,
												 (EGLClientBuffer)t_image, egl_config, NULL);
	
	if (gmode == 1) {
		c_image = vgCreateImage(VG_sRGBA_8888, width, height, VG_IMAGE_QUALITY_FASTER);
		vgClearImage(c_image, 0, 0, width, height);
		eglChooseConfig(egl_display, pbuffer_attribute_list, &egl_config, 1, &num_configs);
		c_context = eglCreateContext(egl_display, egl_config, s_context, NULL);
		c_surface = eglCreatePbufferFromClientBuffer(egl_display, EGL_OPENVG_IMAGE,
													 (EGLClientBuffer)c_image, egl_config, NULL);
	}
	return 0;
}

int
lm_gmode_platform(int gmode)
{
	if (gmode == 1) {
		if (s_fbfd == -1) {
			s_fbfd = open("/dev/fb1", O_RDWR);
			if (s_fbfd == -1)
				return -1;
			if (ioctl(s_fbfd, FBIOGET_FSCREENINFO, &s_finfo) ||
				ioctl(s_fbfd, FBIOGET_VSCREENINFO, &s_vinfo)) {
				close(s_fbfd);
				s_fbfd = -1;
				return -2;
			}
			s_fbp = (char *)mmap(0, s_finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, s_fbfd, 0);
			if (s_fbp == (char *)(-1)) {
				close(s_fbfd);
				s_fbp = NULL;
				s_fbfd = -1;
				return -3;
			}
		/*	printf("/dev/fb1 %d %d %d\n", s_vinfo.xres, s_vinfo.yres, s_vinfo.bits_per_pixel); */
			s_create_offscreen_images(s_vinfo.xres, s_vinfo.yres, 1);
			s_screen_resource = vc_dispmanx_resource_create(VC_IMAGE_RGB565, s_vinfo.xres, s_vinfo.yres, &s_image_prt);
			if (s_screen_resource == DISPMANX_NO_HANDLE) {
				munmap(s_fbp, s_finfo.smem_len);
				close(s_fbfd);
				s_fbp = NULL;
				s_fbfd = -1;
				return -4;
			}
			vc_dispmanx_rect_set(&s_rect1, 0, 0, s_vinfo.xres, s_vinfo.yres);
			my_width = s_vinfo.xres;
			my_height = s_vinfo.yres;
		}
	} else {
		if (s_fbfd != -1) {
			vc_dispmanx_resource_delete(s_screen_resource);
			s_screen_resource = DISPMANX_NO_HANDLE;
			munmap(s_fbp, s_finfo.smem_len);
			s_fbp = NULL;
			close(s_fbfd);
			s_fbfd = -1;
			my_width = my_width_gmode0;
			my_height = my_height_gmode0;
		}
		s_create_offscreen_images(my_width, my_height, 0);
	}
	return 0;
}

int
lm_init_screen_platform(void)
{
	my_width = my_height = 0;
	
	s_clear_framebuffer();

	/*  Init physical screen   */
	init_egl();
    init_dispmanx(&g_nativewindow);
    egl_from_dispmanx(&g_nativewindow);
	
	my_width = g_nativewindow.width;
	my_height = g_nativewindow.height;
	
	/*  Init offscreen images  */
	s_create_offscreen_images(my_width, my_height, 0);
	
	inval_x1 = 0;
	inval_x2 = my_width;
	inval_y1 = 0;
	inval_y2 = my_height;
	
	my_width_gmode0 = my_width;
	my_height_gmode0 = my_height;

	s_paint = vgCreatePaint();
	vgSetColor(s_paint, 0xffffffff);

	lm_set_stroke_color(BYTES_PER_PIXEL == 2 ? 0xffff : 0xffffffff, 1);
	lm_set_fill_color(0, 0);
	
	if (lm_gmode_common(1) < 0)
		lm_gmode_common(0);

	return 0;
}

const char *
lm_platform_name(void)
{
	return "raspberrypi";
}

#if 0
#pragma mark ====== Drawing primitives ======
#endif

void
lm_draw_bitmaps(void *context)
{
	/*  Do nothing on this platform  */
}

void
lm_clear_box(int x, int y, int width, int height)
{
	lm_lock();
	vgSetfv(VG_CLEAR_COLOR, 4, clear_color);
	vgClear(x, y, width, height);
	lm_redraw_nolock(x, y, width, height);
	lm_unlock();
}

void
lm_put_pattern(const pixel_t *p, int x, int y, int width, int height)
{
	lm_lock();
	vgWritePixels(p + width * (height - 1), -width * sizeof(pixel_t),
				  (BYTES_PER_PIXEL == 2 ? VG_sRGB_565 : VG_sRGBA_8888),
				  x, y, width, height);
	lm_redraw_nolock(x, y, width, height);
	lm_unlock();
}

void
lm_get_pattern(pixel_t *p, int x, int y, int width, int height)
{
	lm_lock();
	vgReadPixels(p + width * (height - 1), -width * sizeof(pixel_t),
				 (BYTES_PER_PIXEL == 2 ? VG_sRGB_565 : VG_sRGBA_8888),
				 x, y, width, height);
	lm_redraw_nolock(x, y, width, height);
	lm_unlock();
}

void
lm_copy_pixels(int dx, int dy, int sx, int sy, int width, int height)
{
	lm_lock();
	lm_redraw_nolock(dx, dy, width, height);
	lm_redraw_nolock(sx, sy, width, height);
	vgCopyPixels(dx, dy, sx, sy, width, height);
	lm_unlock();
}

#if 0
#pragma mark ====== Path implementation ======
#endif

static VGPath *s_paths = NULL;
int max_number_of_paths = 1024;

int
lm_create_path(void)
{
	int i;
	if (s_paths == NULL) {
		s_paths = (VGPath *)malloc(sizeof(VGPath) * max_number_of_paths);
		if (s_paths == NULL)
			return -1;  /*  Cannot allocate  */
		for (i = 0; i < max_number_of_paths; i++)
			s_paths[i] = VG_INVALID_HANDLE;
	}
	for (i = 0; i < max_number_of_paths; i++) {
		if (s_paths[i] == VG_INVALID_HANDLE) {
			VGPath path = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1, 0, 0, 0,
									   VG_PATH_CAPABILITY_APPEND_TO | VG_PATH_CAPABILITY_PATH_BOUNDS);
			if (path == VG_INVALID_HANDLE)
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
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == VG_INVALID_HANDLE)
		return -1;  /*  Invalid index  */
	vgDestroyPath(s_paths[idx]);
	s_paths[idx] = VG_INVALID_HANDLE;
	return idx;
}

int
lm_path_moveto(int idx, float x, float y)
{
	VGubyte segment;
	VGfloat pdata[2];
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == VG_INVALID_HANDLE)
		return -1;  /*  Invalid index  */
	pdata[0] = x;
	pdata[1] = y;
	segment = VG_MOVE_TO_ABS;
	vgAppendPathData(s_paths[idx], 1, &segment, pdata);
	return 0;
}

int
lm_path_lineto(int idx, float x, float y)
{
	VGubyte segment;
	VGfloat pdata[2];
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == VG_INVALID_HANDLE)
		return -1;  /*  Invalid index  */
	pdata[0] = x;
	pdata[1] = y;
	segment = VG_LINE_TO_ABS;
	vgAppendPathData(s_paths[idx], 1, &segment, pdata);
	return 0;
}

int
lm_path_arc(int idx, float cx, float cy, float st, float et, float ra, float rb, float rot)
{
	VGubyte segments[3];
	VGfloat pdata[7];
	float dx, dy, cosr, sinr;
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == VG_INVALID_HANDLE)
		return -1;  /*  Invalid index  */
	pdata[4] = rot;
	st *= 3.1415926536 / 180.0;
	et *= 3.1415926536 / 180.0;
	rot *= 3.1415926536 / 180.0;
	dx = ra * cos(st);
	dy = rb * sin(st);
	cosr = cos(rot);
	sinr = sin(rot);
	pdata[0] = cx + dx * cosr - dy * sinr;
	pdata[1] = cy + dx * sinr + dy * cosr;
	pdata[2] = ra;
	pdata[3] = rb;
	dx = ra * cos(et);
	dy = rb * sin(et);
	pdata[5] = cx + dx * cosr - dy * sinr;
	pdata[6] = cy + dx * sinr + dy * cosr;
	segments[0] = VG_LINE_TO_ABS;
	et = (et - st) / (2 * 3.1415926536);
	if (et - floor(et) > 0.5)
		segments[1] = VG_LCCWARC_TO_ABS;
	else
		segments[1] = VG_SCCWARC_TO_ABS;
	segments[2] = VG_CLOSE_PATH;
	vgAppendPathData(s_paths[idx], 3, segments, pdata);
	return 0;
}

int
lm_path_cubic(int idx, float x1, float y1, float x2, float y2, float x3, float y3)
{
	VGubyte segment;
	VGfloat pdata[6];
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == VG_INVALID_HANDLE)
		return -1;  /*  Invalid index  */
	pdata[0] = x1;
	pdata[1] = y1;
	pdata[2] = x2;
	pdata[3] = y2;
	pdata[4] = x3;
	pdata[5] = y3;
	segment = VG_CUBIC_TO_ABS;
	vgAppendPathData(s_paths[idx], 1, &segment, pdata);
	return 0;
}

int
lm_path_close(int idx)
{
	VGubyte segment;
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == VG_INVALID_HANDLE)
		return -1;  /*  Invalid index  */
	segment = VG_CLOSE_PATH;
	vgAppendPathData(s_paths[idx], 1, &segment, NULL);
	return 0;	
}

int
lm_path_line(int idx, float x1, float y1, float x2, float y2)
{
	int v;
	if ((v = lm_path_moveto(idx, x1, y1)) ||
		(v = lm_path_lineto(idx, x2, y2)))
		return v;
	else return 0;
}

int
lm_path_rect(int idx, float x, float y, float width, float height)
{
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == VG_INVALID_HANDLE)
		return -1;  /*  Invalid index  */
	if (vguRect(s_paths[idx], x, y, width, height) != 0)
		return 1;
	else return 0;
}

int
lm_path_roundrect(int idx, float x, float y, float width, float height, float rx, float ry)
{
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == VG_INVALID_HANDLE)
		return -1;  /*  Invalid index  */
	if (vguRoundRect(s_paths[idx], x, y, width, height, rx * 2, ry * 2) != 0)
		return 1;
	else return 0;
}

int
lm_path_ellipse(int idx, float x, float y, float rx, float ry)
{
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == VG_INVALID_HANDLE)
		return -1;  /*  Invalid index  */
	if (vguEllipse(s_paths[idx], x, y, rx * 2, ry * 2) != 0)
		return 1;
	else return 0;
}

int
lm_path_fan(int idx, float cx, float cy, float st, float et, float ra, float rb, float rot)
{
	int v;
	if ((v = lm_path_moveto(idx, cx, cy)) ||
		(v = lm_path_arc(idx, cx, cy, st, et, ra, rb, rot)) ||
		(v = lm_path_close(idx)))
		return v;
	else return 0;
}

static int s_path_drawflag = VG_STROKE_PATH;
static VGPaint s_stroke_color = VG_INVALID_HANDLE;
static VGPaint s_fill_color = VG_INVALID_HANDLE;

int
lm_set_stroke_color(pixel_t col, int enable)
{
	VGfloat c[4];
	if (enable == 0)
		s_path_drawflag &= ~VG_STROKE_PATH;
	else {
		s_path_drawflag |= VG_STROKE_PATH;
		lm_select_active_buffer(GRAPHIC_ACTIVE);
		if (enable != -1 || s_stroke_color == VG_INVALID_HANDLE) {
			if (enable == -1)
				col = (BYTES_PER_PIXEL == 2 ? 0xffff : 0xffffffff);
			c[0] = REDCOMPONENT(col);
			c[1] = GREENCOMPONENT(col);
			c[2] = BLUECOMPONENT(col);
			c[3] = ALPHACOMPONENT(col);
			if (s_stroke_color != VG_INVALID_HANDLE) {
				vgDestroyPaint(s_stroke_color);
			}
			s_stroke_color = vgCreatePaint();
			vgSetParameteri(s_stroke_color, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
			vgSetParameterfv(s_stroke_color, VG_PAINT_COLOR, 4, c);
			vgSetPaint(s_stroke_color, VG_STROKE_PATH);
		}
		vgGetParameterfv(s_stroke_color, VG_PAINT_COLOR, 4, c);
	}
	return 0;
}

int
lm_set_fill_color(pixel_t col, int enable)
{
	VGfloat c[4];
	if (enable == 0)
		s_path_drawflag &= ~VG_FILL_PATH;
	else {
		s_path_drawflag |= VG_FILL_PATH;
		lm_select_active_buffer(GRAPHIC_ACTIVE);
		if (enable != -1 || s_fill_color == VG_INVALID_HANDLE) {
			if (enable == -1)
				col = (BYTES_PER_PIXEL == 2 ? 0xffff : 0xffffffff);
			c[0] = REDCOMPONENT(col);
			c[1] = GREENCOMPONENT(col);
			c[2] = BLUECOMPONENT(col);
			c[3] = ALPHACOMPONENT(col);
			if (s_fill_color != VG_INVALID_HANDLE) {
				vgDestroyPaint(s_fill_color);
			}
			s_fill_color = vgCreatePaint();
			vgSetParameteri(s_fill_color, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
			vgSetParameterfv(s_fill_color, VG_PAINT_COLOR, 4, c);
			vgSetPaint(s_fill_color, VG_FILL_PATH);
		}
		vgGetParameterfv(s_fill_color, VG_PAINT_COLOR, 4, c);
	}
	return 0;
}

int
lm_draw_path(int idx)
{
	if (s_paths == NULL || idx < 0 || idx >= max_number_of_paths || s_paths[idx] == VG_INVALID_HANDLE)
		return -1;  /*  Invalid index  */
	if ((s_path_drawflag & VG_STROKE_PATH) && s_stroke_color == VG_INVALID_HANDLE) {
		/*  Default color (white)  */
		lm_set_stroke_color(RGBFLOAT(1, 1, 1), 1);
	}
	if ((s_path_drawflag & (VG_STROKE_PATH | VG_FILL_PATH)) == 0)
		return 0;
	lm_lock();
	lm_select_active_buffer_nolock(GRAPHIC_ACTIVE);
/*	vgSeti(VG_IMAGE_MODE, VG_DRAW_IMAGE_NORMAL);
	SHOW_IF_VGERROR;
	if (s_path_drawflag & VG_STROKE_PATH)
		vgSetPaint(s_stroke_color, VG_STROKE_PATH);
	if (s_path_drawflag & VG_FILL_PATH)
		vgSetPaint(s_fill_color, VG_FILL_PATH); */
	vgDrawPath(s_paths[idx], s_path_drawflag & (VG_STROKE_PATH | VG_FILL_PATH));
	lm_redraw_nolock(0, 0, 1, 1);  /*  Dummy rect (we redraw all screen anyway)  */
	lm_unlock();
	return 0;
}

