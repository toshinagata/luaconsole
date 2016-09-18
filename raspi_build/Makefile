#  Makefile
#  LuaConsole for Raspberry Pi
#  2016.7.16. Toshi Nagata
#  Copyright 2016 Toshi Nagata. All rights reserved.

SPEC_SRCS := raspi.c raspi_screen.c

DMX_INCLUDES = -I/opt/vc/include -I/opt/vc/include/interface/vmcs_host/ -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux

CFLAGS = -Wall -D_GNU_SOURCE -D__RASPBERRY_PI__=1 $(DMX_INCLUDES)
LDFLAGS = -L/opt/vc/lib/ -lbcm_host -lvcos -lvchiq_arm -lpthread -L/opt/vc/lib -lEGL -lGLESv2 -lwiringPi

-include ../Makefile.inc

binary: all
	tar --exclude='.*' --exclude='luacon/luacon_config' --exclude='luacon/lua/_*' -cvzf ../binaries/luacon_raspi.tar.gz luacon
