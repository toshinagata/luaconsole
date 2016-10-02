/*
 *  raspi_main.c
 *  LuaConsole
 *
 *  Created by Toshi Nagata on 2016/02/07.
 *  Copyright 2016 Toshi Nagata.  All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

#include <sys/types.h>

/*  For GPIO  */
#include <wiringPi.h>

/*  For SPI  */
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

/*  For touchpanel  */
#include <linux/input.h>

#if 0
#pragma mark ====== Framebuffer ======
#endif

#ifndef __CONSOLE__
#include <linux/fb.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include "luacon.h"
#include "screen.h"
#include "graph.h"

static int s_original_bpp;

static char *s_framebuffer;
static int s_fbfd = 0;

static void
s_pb_raspi_framebuffer(void)
{
#ifndef __CONSOLE__
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	
	/*  Open frame buffer device  */
	s_fbfd = open("/dev/fb0", O_RDWR);
	if (!s_fbfd) {
		printf("Error: cannot open framebuffer device.\n");
		return;
	}
	if (ioctl(s_fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
		printf("Error reading variable information.\n");
		return;
	}
	printf("Frame buffer %dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel );
	
	s_original_bpp = vinfo.bits_per_pixel;
	vinfo.bits_per_pixel = 16;
	if (ioctl(s_fbfd, FBIOPUT_VSCREENINFO, &vinfo)) {
		printf("Error setting variable information.\n");
		return;
	}
	
	my_width = vinfo.xres;
	my_height = vinfo.yres;

	my_width_gmode0 = my_width;
	my_height_gmode0 = my_height;
	
	if (ioctl(s_fbfd, FBIOGET_FSCREENINFO, &finfo)) {
		printf("Error reading fixed information.\n");
		return;
	}
	
	/*  Map the memory buffer  */
	s_framebuffer = (char *)mmap(0, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, s_fbfd, 0);
	if (s_framebuffer == (char *)-1) {
		printf("Failed to mmap.\n");
		return;
	}
	printf("mmap'ed buffer %p\n", mmap);
#endif
}

void
lm_init_framebuffer(void)
{
#ifndef __CONSOLE__
	s_pb_raspi_framebuffer();
#endif
}

#if 0
#pragma mark ====== Key Input ======
#endif

static struct termios s_SaveTermIos;
static struct termios s_RawTermIos;

int
tty_puts(const char *s)
{
	return fputs(s, stdout);
}

int
tty_putc(int c)
{
	return fputc(c, stdout);
}

int
tty_getch(void)
{
	int n;
	u_int8_t c;
	n = read(STDIN_FILENO, &c, 1);
	if (n > 0)
		return c;
	else return -1;	
}

void
set_raw_mode(void)
{
	tcgetattr(STDIN_FILENO, &s_SaveTermIos);
	s_RawTermIos = s_SaveTermIos;
	cfmakeraw(&s_RawTermIos);
	s_RawTermIos.c_cc[VMIN] = 1;
	s_RawTermIos.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, 0, &s_RawTermIos);
	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
	setvbuf(stdout, NULL, _IONBF, 0);
	tty_puts("\x1b[H");     /*  Set cursor home  */
	tty_puts("\x1b[J");     /*  Erase to end of screen  */
	tty_puts("\x1b[?25l");  /*  Hide cursor  */
}

void
unset_raw_mode(void)
{
	char buf[20];
	tcsetattr(STDIN_FILENO, 0, &s_SaveTermIos);
	snprintf(buf, sizeof buf, "\x1b[%d;1H", my_height / 16);  /*  Cursor down full screen  */
	tty_puts(buf);
	tty_puts("\x1b[?25h");  /*  Show cursor  */
}

/*  Key input.  */
/*  The standard input should be set to the RAW mode  */
int
lm_getch_with_timeout(int32_t usec)
{
	int n;
	u_int32_t timer = 0, wait;
	/*  Redraw screen if more than 100 ms has elapsed from last time  */
	{
		static struct timeval tval0 = {0, 0}, tval;
		gettimeofday(&tval, NULL);
		if (tval0.tv_sec == 0 || (tval.tv_sec - tval0.tv_sec) * 1000000 + tval.tv_usec - tval0.tv_usec > 100000) {
			tval0 = tval;
			lm_draw_platform(NULL);
		}
	}
	if (usec < 0)
		usec = 0;
	while ((n = tty_getch()) == -1) {
		if (timer >= usec)
			break;
		wait = (usec < 10000 ? usec : 10000);
		usleep(wait);
		timer += wait;
	}
	return n;
}

#if 0
#pragma mark ====== Touchpanel ======
#endif

static int16_t *s_calib_coords;
static int16_t s_calib_npoints;
static int16_t s_fd = 0;

static int
l_tp_open(lua_State *L, int in_calibration)
{
	char dname[20];
	char name[256] = "Unknown";
	int i;
	if (s_fd == 0) {
		for (i = 0; i < 10; i++) {
			snprintf(dname, sizeof(dname), "/dev/input/event%d", i);
			if ((s_fd = open(dname, O_RDONLY)) < 0)
				continue;
			ioctl(s_fd, EVIOCGNAME(sizeof(name)), name);
			if (strstr(name, "Touchscreen") != NULL)
				break;
			close(s_fd);
			s_fd = -1;
		}
		if (s_fd > 0) {
			/*  Read the configuration data if present  */
			int len;
			int16_t *cp;
			if (s_calib_coords != NULL) {
				free(s_calib_coords);
				s_calib_coords = NULL;
				s_calib_npoints = 0;
			}
			lua_getglobal(L, "sys");
			lua_getfield(L, -1, "config");
			lua_getfield(L, -1, "tp_calibrate");
			len = lua_rawlen(L, -1);
			if (len == 20 || len == 36 || len == 52) {
				cp = (int16_t *)calloc(sizeof(int16_t), len);
				if (cp == NULL)
					return luaL_error(L, "Cannot allocate memory for touchpanel calibration.\n");
				for (i = 0; i < len; i++) {
					lua_rawgeti(L, -1, i + 1);
					cp[i] = luaL_checkinteger(L, -1);
					lua_pop(L, 1);
				}
				s_calib_npoints = len / 4;
				s_calib_coords = cp;
			} else if (!in_calibration) {
				return luaL_error(L, "No calibration info is available for the touchpanel. Try tp.calibrate().\n");
			}				
			lua_pop(L, 3);  /*  Pop tp_calibrate, config, sys  */
		}
	}
	return s_fd;
}

/*  cp[2], cp[3]: screen position (input);
    cp[0], cp[1]: touchpanel position (output)  */
static int
l_tp_calibrate_one(int16_t *cp)
{
	int i, n, ex, ey, ep, endflag;
	struct input_event ev[9];
	lm_set_stroke_color(RGBFLOAT(1, 1, 1), 1);
	n = lm_create_path();
	lm_path_moveto(n, cp[2] - 4, cp[3]);
	lm_path_lineto(n, cp[2] + 4, cp[3]);
	lm_path_moveto(n, cp[2], cp[3] - 4);
	lm_path_lineto(n, cp[2], cp[3] + 4);
	lm_draw_path(n);
	lm_destroy_path(n);
	endflag = 0;
	ex = ey = ep = 0;
	while (endflag == 0) {
		if ((n = lm_getch(0)) == 3 || n == 0x1b)
			return -1;  /*  Abort  */
		n = read(s_fd, ev, sizeof(ev));
		for (i = 0; i < n / sizeof(ev[0]); i++) {
			if (ev[i].type == EV_ABS) {
				if (ev[i].code == ABS_X) {
					ex = ev[i].value;
				} else if (ev[i].code == ABS_Y) {
					ey = ev[i].value;
				} else if (ev[i].code == ABS_PRESSURE) {
					ep = ev[i].value;
				}
				lm_locate(0, 4);
				lm_puts_format("%d %d %d", ex, ey, ep);
				lm_erase_to_eol();
			} else if (ev[i].type == EV_KEY) {
				if (ev[i].code == BTN_TOUCH && ev[i].value == 0) {
					endflag = 1;
					break;
				}
			}
		}
		lm_update_screen();
	}
	cp[0] = ex;
	cp[1] = ey;
	lm_locate(0, 4);
	lm_erase_to_eol();
	lm_update_screen();
	lm_select_active_buffer(GRAPHIC_ACTIVE);
	lm_clear_box(cp[2] - 4, cp[3] - 4, 8, 8);
	return 0;
}

/*  tp.calibrate(n)  Calibrate the touch panel by n points. n = 5 (default), 9, or 13 (not implemented yet)  */
int
l_tp_calibrate(lua_State *L)
{
	int i, n, aborted = 0;
	int16_t *cc;
	if (l_tp_open(L, 1) < 0)
		return 0;
	if (lua_gettop(L) > 0) {
		n = luaL_checkinteger(L, 1);
		if (n > 9)
			n = 13;  /*  Not implemented yet  */
		else if (n > 5)
			n = 9;
		else n = 5;
	} else n = 5;
	cc = (int16_t *)calloc(sizeof(int16_t), n * 4);

	/*  Generate target coordinates  */
	cc[2] = my_width / 2;
	cc[3] = my_height / 2;
	cc[6] = my_width - 12;
	cc[7] = my_height - 12;
	if (n == 5) {
		cc[10] = 12;
		cc[11] = my_height - 12;
		cc[14] = 12;
		cc[15] = 12;
		cc[18] = my_width - 12;
		cc[19] = 12;
	} else {
		cc[10] = my_width / 2;
		cc[11] = my_height - 12;
		cc[14] = 12;
		cc[15] = my_height - 12;
		cc[18] = 12;
		cc[19] = my_height / 2;
		cc[22] = 12;
		cc[23] = 12;
		cc[26] = my_width / 2;
		cc[27] = 12;
		cc[30] = my_width - 12;
		cc[31] = 12;
		cc[34] = my_width - 12;
		cc[35] = my_height / 2;
		if (n == 13) {
			cc[38] = (cc[6] + cc[2]) / 2;
			cc[39] = (cc[7] + cc[3]) / 2;
			cc[42] = (cc[14] + cc[2]) / 2;
			cc[43] = cc[39];
			cc[46] = cc[42];
			cc[47] = (cc[23] + cc[3]) / 2;
			cc[50] = cc[38];
			cc[51] = cc[47];
		}
	}
	
	/*  Display marks and get touch  */
	lm_gmode_common(1);
	lm_select_active_buffer(GRAPHIC_ACTIVE);
	lm_clear_box(0, 0, my_width, my_height);
	lm_select_active_buffer(TEXT_ACTIVE);
	lm_clear_box(0, 0, my_width, my_height);
	lm_locate(0, 1);
	lm_puts("*** Touch the mark as it appears on the screen ***");
	for (i = 0; i < n; i++) {
		if (l_tp_calibrate_one(cc + i * 4)) {
			aborted = 1;
			break;
		}
	}
	if (aborted == 0) {
		/*  Success  */
		/*  Record the calibration data to sys.config  */
		lua_getglobal(L, "sys");
		lua_getfield(L, -1, "config");
		lua_pushstring(L, "tp_calibrate");
		lua_createtable(L, n * 4, 0);
		for (i = 0; i < n * 4; i++) {
			lua_pushinteger(L, cc[i]);
			lua_rawseti(L, -2, i + 1);
		}
		lua_rawset(L, -3);
		lua_pop(L, 1);   /*  Pop sys.config  */
		lua_getfield(L, -1, "save_config");
		lua_call(L, 0, 0);
		lua_pop(L, 1);   /*  Pop sys  */
	}
	if (s_calib_coords != NULL)
		free(s_calib_coords);
	s_calib_coords = cc;
	s_calib_npoints = n;
	lm_select_active_buffer(GRAPHIC_ACTIVE);
	lm_clear_box(0, 0, my_width, my_height);
	if (aborted) {
		lm_puts("Calibration aborted.\n");
	} else {
		lm_puts("Calibration success.\n");
	}

	return 0;
}

static void
l_tp_convert(int si, int ti, int *xi, int *yi)
{
	float s, t, s0, t0, s1, t1, s2, t2;
	float x0, y0, x1, y1, x2, y2;
	float v, w;
	float d;
	int i;
	s0 = s_calib_coords[0];
	t0 = s_calib_coords[1];
	x0 = s_calib_coords[2];
	y0 = s_calib_coords[3];
	s = si - s0;
	t = ti - t0;
	for (i = 1; i < s_calib_npoints; i++) {
		int n = i * 4;
		s1 = s_calib_coords[n++] - s0;
		t1 = s_calib_coords[n++] - t0;
		x1 = s_calib_coords[n++] - x0;
		y1 = s_calib_coords[n++] - y0;
		if (i == s_calib_npoints - 1)
			n = 4;
		s2 = s_calib_coords[n++] - s0;
		t2 = s_calib_coords[n++] - t0;
		x2 = s_calib_coords[n++] - x0;
		y2 = s_calib_coords[n++] - y0;
		d = 1.0 / (s1 * t2 - s2 * t1);
		s1 *= d;
		s2 *= d;
		t1 *= d;
		t2 *= d;
		v = t2 * s - s2 * t;
		w = s1 * t - t1 * s;
		if (v >= 0.0 && w >= 0.0) {
			*xi = (int)(x1 * v + x2 * w + x0);
			*yi = (int)(y1 * v + y2 * w + y0);
			return;
		}
	}
}

/*  getevent() -> (type, x, y, pressure)  */
/*  type = nil: no input, 1: start touch, 2: drag, 3: end touch  */
int
l_tp_getevent(lua_State *L)
{
	int n, x, y;
	static int s_state = 0, s_x, s_y, s_p, s_count;
	static int s_x0, s_y0, s_p0;
	static int s_xlast, s_ylast;
	if (l_tp_open(L, 0) <= 0)
		return 0;
	while (1) {
		struct pollfd pfd;
		struct input_event ev;
		pfd.fd = s_fd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		n = poll(&pfd, 1, 0);
		if (n < 0)
			return luaL_error(L, "Touchpanel error: poll() failed");
		else if (n == 0)
			return 0;  /*  No input  */
		n = read(s_fd, &ev, sizeof(ev));
		if (n < sizeof(ev))
			return luaL_error(L, "Touchpanel error: read() failed");
		if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
			if (ev.value == 1) {
				s_state = 1;  /*  Touch start  */
				s_x0 = s_y0 = s_p0 = 0;
				s_count = 0;
			} else {
				s_state = 0;  /*  Touch end  */
				l_tp_convert(s_x0, s_y0, &x, &y);
				lua_pushinteger(L, 3);
				lua_pushinteger(L, x);
				lua_pushinteger(L, y);
				lua_pushinteger(L, s_p);
				return 4;
			}
		} else if (ev.type == EV_ABS) {
			if (ev.code == ABS_X)
				s_x = ev.value;
			else if (ev.code == ABS_Y)
				s_y = ev.value;
			else if (ev.code == ABS_PRESSURE) {
				s_p = ev.value;
				if (s_count == 0) {
					s_x0 = s_x;
					s_y0 = s_y;
					s_p0 = s_p;
				} else {
					/*  Filter sudden noise  */
					s_x0 = (int)(s_x0 * 0.9 + s_x * 0.1);
					s_y0 = (int)(s_y0 * 0.9 + s_y * 0.1);
					s_p0 = (int)(s_p0 * 0.9 + s_p * 0.1);
				}
				s_count++;
				l_tp_convert(s_x0, s_y0, &x, &y);
				if (s_state == 2) {
					/*  If the position is too close, then do not invoke drag event  */
					int dx = s_xlast - s_x0;
					int dy = s_ylast - s_y0;
					const int lim = 10;
					if (dx < lim && dx > -lim && dy < lim && dy > -lim)
						return 0;
				}
				s_xlast = s_x0;
				s_ylast = s_y0;
				lua_pushinteger(L, (s_state == 0 ? 1 : s_state));
				lua_pushinteger(L, x);
				lua_pushinteger(L, y);
				lua_pushinteger(L, s_p);
				if (s_state == 0 || s_state == 1)
					s_state = 2;  /*  The next event will be 'drag'  */
				return 4;
			}
		}
	}
}

static const struct luaL_Reg tp_lib[] = {
	{"calibrate", l_tp_calibrate},
	{"getevent", l_tp_getevent},
	{NULL, NULL}
};

int
luaopen_tp(lua_State *L) 
{
#if LUA_VERSION_NUM >= 502
	luaL_newlib(L, tp_lib);
#else
	luaL_register(L, "tp", tp_lib);
#endif
	return 1;
}

#if 0
#pragma mark ====== GPIO ======
#endif

/*  gpio.setup()  */
static int
l_gpio_setup(lua_State *L)
{
	static int setup_called = 0;
	if (setup_called)
		return 0;
	setup_called = 1;
	if (getenv("WIRINGPI_GPIOMEM") == NULL)
		setenv("WIRINGPI_GPIOMEM", "1", 1);
	wiringPiSetup();
	return 0;
}

/*  gpio.pinmode(pinno, 0 (input) or 1 (output))  */
static int
l_gpio_pinmode(lua_State *L)
{
	int pinno, mode;
	pinno = luaL_checkinteger(L, 1);
	mode = luaL_checkinteger(L, 2);
	pinMode(pinno, mode);
	return 0;
}

/*  gpio.dwrite(pinno, val)  */
static int
l_gpio_dwrite(lua_State *L)
{
	int pinno, val;
	pinno = luaL_checkinteger(L, 1);
	val = luaL_checkinteger(L, 2);
	digitalWrite(pinno, val);
	return 0;
}

/*  gpio.dread(pinno)  */
static int
l_gpio_dread(lua_State *L)
{
	int pinno, val;
	pinno = luaL_checkinteger(L, 1);
	val = digitalRead(pinno);
	lua_pushinteger(L, val);
	return 1;
}

static const struct luaL_Reg gpio_lib[] = {
	{"setup", l_gpio_setup},
	{"pinmode", l_gpio_pinmode},
	{"dwrite", l_gpio_dwrite},
	{"dread", l_gpio_dread},
	{NULL, NULL}
};

int
luaopen_gpio(lua_State *L) 
{
#if LUA_VERSION_NUM >= 502
	luaL_newlib(L, gpio_lib);
#else
	luaL_register(L, "gpio", gpio_lib);
#endif
	return 1;
}

#if 0
#pragma mark ====== SPI ======
#endif

/*  For spi0.1...spi1.2  */
static int s_spi_fd[6] = {-1, -1, -1, -1, -1, -1};
static int s_spi_speed[6];
static int s_spi_mode[6];
static int s_spi_bpw[6];

static int
l_spi_checkchannel(int ch)
{
	switch (ch) {
		case 0: case 1: case 2: break;
		case 10: case 11: case 12: ch -= 7; break;
		default:
			return luaL_error(gL, "SPI channel must be 0/1/2 or 10/11/12 but %d is given", ch);
	}
	return ch;
}

/*  spi.setup(channel, speed, mode=0, bpw=8))  */
static int
l_spi_setup(lua_State *L)
{
	int n, ch, channel, speed, mode, bpw;
	char dname[16];
	channel = luaL_checkinteger(L, 1);
	speed = luaL_checkinteger(L, 2);
	n = lua_gettop(L);
	mode = 0;
	bpw = 8;
	if (n >= 3) {
		mode = luaL_checkinteger(L, 3);
		if (n >= 4) {
			bpw = luaL_checkinteger(L, 4);
		}
	}
	ch = l_spi_checkchannel(channel);
	if (s_spi_fd[ch] < 0) {
		snprintf(dname, sizeof(dname), "/dev/spidev%d.%d", ch / 3, ch % 3);
		n = open(dname, O_RDWR);
		if (n < 0)
			return luaL_error(L, "Cannot open SPI device %s", dname);
	} else n = s_spi_fd[ch];
	if (ioctl(n, SPI_IOC_WR_MODE, &mode) < 0) {
		close(n);
		return luaL_error(L, "Cannot set SPI write mode");
	}
	if (ioctl(n, SPI_IOC_WR_BITS_PER_WORD, &bpw) < 0) {
		close(n);
		return luaL_error(L, "Cannot set SPI write bits-per-word");
	}
	if (ioctl(n, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
		close(n);
		return luaL_error(L, "Cannot set SPI write speed");
	}
	if (ioctl(n, SPI_IOC_RD_MODE, &mode) < 0) {
		close(n);
		return luaL_error(L, "Cannot set read SPI mode");
	}
	if (ioctl(n, SPI_IOC_RD_BITS_PER_WORD, &bpw) < 0) {
		close(n);
		return luaL_error(L, "Cannot set SPI read bits-per-word");
	}
	if (ioctl(n, SPI_IOC_RD_MAX_SPEED_HZ, &speed) < 0) {
		close(n);
		return luaL_error(L, "Cannot set SPI read speed");
	}
	s_spi_fd[ch] = n;
	s_spi_speed[ch] = speed;
	s_spi_mode[ch] = mode;
	s_spi_bpw[ch] = bpw;
	return 0;
}

/*  Internal routine for SPI read/write; flag = 1: read, 2: write, 3: both  */
static int
l_spi_readwrite_sub(lua_State *L, int flag)
{
	int ch, channel, i;
	unsigned int length, len;
	unsigned char *p;
	struct spi_ioc_transfer tr;
	channel = luaL_checkinteger(L, 1);
	luaL_checktype(L, 2, LUA_TTABLE);
	length = luaL_checkinteger(L, 3);
	ch = l_spi_checkchannel(channel);
	if (length < 32) {
		p = alloca(length * 2);
	} else {
		p = malloc(length * 2);
	}
	if (p == NULL)
		luaL_error(L, "Cannot allocate SPI buffer");
	len = lua_rawlen(L, 2);
	if (flag & 2) {
		for (i = 1; i <= length; i++) {
			char c;
			if (i > len)
				c = 0;
			else {
				lua_rawgeti(L, 2, i);
				c = luaL_checkinteger(L, -1);
				lua_pop(L, 1);
			}
			p[i - 1] = c;
		}
	} else {
		memset(p, 0, length);
	}
	memset(&tr, 0, sizeof(tr));
	tr.tx_buf = (unsigned long)p;
	tr.rx_buf = (unsigned long)(p + length);
	tr.len = length;
	tr.delay_usecs = 0;
	tr.speed_hz = s_spi_speed[ch];
	tr.bits_per_word = s_spi_bpw[ch];
	tr.cs_change = 0;
	if (ioctl(s_spi_fd[ch], SPI_IOC_MESSAGE(1), &tr) < 0) {
		lua_pushinteger(L, 1);
		return 1;
	}
	if (flag & 1) {
		for (i = 1; i <= length; i++) {
			lua_pushinteger(L, (unsigned char)p[i + length - 1]);
			lua_rawseti(L, 2, i);
		}
	}
	if (length >= 32)
		free(p);
	lua_pushinteger(L, 0);
	return 1;
}

/*  spi.read(channel, table, length)  */
static int
l_spi_read(lua_State *L)
{
	return l_spi_readwrite_sub(L, 1);
}

/*  spi.write(channel, table, length)  */
static int
l_spi_write(lua_State *L)
{
	return l_spi_readwrite_sub(L, 2);
}

/*  spi.readwrite(channel, table, length)  */
static int
l_spi_readwrite(lua_State *L)
{
	return l_spi_readwrite_sub(L, 3);
}

/*  spi.close(channel)  */
static int
l_spi_close(lua_State *L)
{
	int ch, channel;
	channel = luaL_checkinteger(L, 1);
	ch = l_spi_checkchannel(channel);
	if (s_spi_fd[ch] < 0)
		return luaL_error(L, "SPI channel %d is not opened", channel);
	close(s_spi_fd[ch]);
	s_spi_fd[ch] = -1;
	return 0;
}

static const struct luaL_Reg spi_lib[] = {
	{"setup", l_spi_setup},
	{"read", l_spi_read},
	{"write", l_spi_write},
	{"readwrite", l_spi_readwrite},
	{"close", l_spi_close},
	{NULL, NULL}
};

int
luaopen_spi(lua_State *L) 
{
#if LUA_VERSION_NUM >= 502
	luaL_newlib(L, spi_lib);
#else
	luaL_register(L, "spi", spi_lib);
#endif
	return 1;
}

#if 0
#pragma mark ====== main ======
#endif

int
lua_setup_platform(lua_State *L)
{
	luaL_requiref(L, "tp", luaopen_tp, 1);
	luaL_requiref(L, "gpio", luaopen_gpio, 1);
	luaL_requiref(L, "spi", luaopen_spi, 1);
	return 0;
}

int
main(int argc, const char **argv)
{
#ifndef __CONSOLE__
	if (argc > 1 && strcmp(argv[1], "-t") == 0)
		my_console = LM_CONSOLE_TTY;
#else
	my_console = LM_CONSOLE_TTY;
#endif
	
	set_raw_mode();
	atexit(unset_raw_mode);

	lua_setup();

	while (lm_runmode != LM_EXIT_RUNMODE) {
		lua_loop();
	}
	usleep(2000000);
	exit(0);
}
