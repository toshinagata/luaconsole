//
//  MacLuaConAppDelegate.m
//  LuaConsole
//
//  Created by 永田 央 on 16/01/09.
//  Copyright 2016 __MyCompanyName__. All rights reserved.
//

#import "MacLuaConAppDelegate.h"
#include "luacon.h"

@implementation MacLuaConAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	lua_setup();
	while (lm_runmode != LM_EXIT_RUNMODE) {
		lua_loop();
	}
	usleep(2000000);
	[NSApp terminate:self];
}

@end
