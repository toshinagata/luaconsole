/*
 *  inttypes.h
 *  LuaConsole
 *
 *  Created by 永田 央 on 16/01/09.
 *  Copyright 2016 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef __INTTYPES_H__
#define __INTTYPES_H__

#include <sys/types.h>

#if 0
#ifndef _INT8_T
#define _INT8_T
#ifndef __INT8_T__
#define __INT8_T__ char
#endif
typedef	signed __INT8_T__   int8_t;
typedef	unsigned __INT8_T__	u_int8_t;
#endif
#ifndef _INT16_T
#define _INT16_T
#ifndef __INT16_T__
#define __INT16_T__ short
#endif
typedef	__INT16_T__	         int16_t;
typedef	unsigned __INT16_T__ u_int16_t;
#endif
#ifndef _INT32_T
#define _INT32_T
#ifndef __INT32_T__
#define __INT32_T__ long
#endif
typedef	__INT32_T__ int32_t;
typedef	unsigned __INT32_T__ u_int32_t;
#endif
#ifndef _INT64_T
#define _INT64_T
typedef	long long		int64_t;
#endif
#endif

#endif /* __INTTYPES_H__ */
