#  Makefile.inc
#  LuaConsole
#  2016.7.16. Toshi Nagata
#  Copyright 2016 Toshi Nagata. All rights reserved.
#
#  This file is to be included from SOME_DIR/Makefile
#  It assumes that the current directory is SOME_DIR

SUBDIR := $(shell basename $(PWD))

LUA_SRCS := lapi.c lauxlib.c lbaselib.c lbitlib.c lcode.c lcorolib.c lctype.c ldblib.c ldebug.c ldo.c ldump.c lfunc.c lgc.c linit.c liolib.c llex.c lmathlib.c lmem.c loadlib.c lobject.c lopcodes.c loslib.c lparser.c lstate.c lstring.c lstrlib.c ltable.c ltablib.c ltm.c lundump.c lutf8lib.c lvm.c lzio.c

COMMON_SRCS := edit.c graph.c luacon.c screen.c

SRCS := $(patsubst %.c,../lua_src/%.c,$(LUA_SRCS)) \
 $(patsubst %.c,../src/%.c,$(COMMON_SRCS)) \
 $(patsubst %.c,../$(SUBDIR)/%.c,$(patsubst %.cpp,../$(SUBDIR)/%.cpp,$(SPEC_SRCS)))

ifeq ($(MAKECMDGOALS),)
 DEBUG = 1
endif
ifeq ($(MAKECMDGOALS),debug)
 DEBUG = 1
endif

ifeq ($(DEBUG),1)
 BUILD = build_debug
 CFLAGS := -O0 -g $(CFLAGS)
else
 BUILD = build
 CFLAGS := -O2 -g $(CFLAGS)
endif

CC ?= gcc
CXX ?= g++
TARGET ?= luacon$(EXE)

OBJS := $(patsubst ../%.c,$(BUILD)/%.o,$(patsubst ../%.cpp,$(BUILD)/%.cpp.o,$(SRCS))) 
DEPS := $(patsubst ../%.c,$(BUILD)/%.d,$(patsubst ../%.cpp,$(BUILD)/%.d,$(SRCS)))

INCLUDES += -I. -I../src -I../lua_src

CFLAGS += $(ARCHFLAGS) $(BASEFLAGS) $(INCLUDES) -DLUA_C89_NUMBERS
LDFLAGS += $(ARCHFLAGS)
LIBS += -lm

all: subdir luacon/$(TARGET)

debug: all

-include $(DEPS)

subdir:
	mkdir -p $(BUILD)/$(SUBDIR) $(BUILD)/lua_src $(BUILD)/src luacon

luacon/$(TARGET): $(OBJS)
ifeq ($(BAREMETAL),)
	$(CC) -o $@ $(OBJS) $(SYS_OBJS) $(LDFLAGS) $(LIBS)
else
	$(CXX) $(LDFLAGS) -Wl,-Map=build/kernel.map -o build/kernel.elf $(OBJS) $(LIBS)
	$(OBJCOPY) build/kernel.elf -O binary $@
endif
	mkdir -p luacon/lua
	cp -f ../fontdata.bin ../*.lua luacon
	cp -f ../lua/* luacon/lua

clean:
	rm -rf build build_debug luacon

y.tab.c y.tab.h: grammar.y
	bison -d -y $<

$(BUILD)/%.cpp.o: ../%.cpp Makefile
	$(CXX) $(CFLAGS) -MMD -c $< -o $@

$(BUILD)/%.o: ../%.c Makefile
	$(CC) $(CFLAGS) -MMD -c $< -o $@

