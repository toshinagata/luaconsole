/*
 *  edit.h
 *  LuaConsole
 *
 *  Created by 永田 央 on 16/01/09.
 *  Copyright 2016 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef __EDIT_H__
#define __EDIT_H__

#include "luacon.h"

#ifdef __cplusplus
extern "C" {
#endif
	
/*  Syntax coloring  */
extern pixel_t gSyntaxColors[];

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
		
#endif /* __EDIT_H__ */
