/******************************************************************************
*
*   Copyright (c) 2021 Intel.
*
*   Licensed under the Apache License, Version 2.0 (the "License");
*   you may not use this file except in compliance with the License.
*   You may obtain a copy of the License at
*
*       http://www.apache.org/licenses/LICENSE-2.0
*
*   Unless required by applicable law or agreed to in writing, software
*   distributed under the License is distributed on an "AS IS" BASIS,
*   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*   See the License for the specific language governing permissions and
*   limitations under the License.
*
*******************************************************************************/

#ifndef __TTYPES_H__
#define __TTYPES_H__

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef TRUE
#define TRUE    1
#endif /*TRUE*/

#ifndef FALSE
#define FALSE   0
#endif /*FALSE*/

#ifndef NULL
#define NULL   (void*)0
#endif /*NULL*/

/************************************************************************/
/*     SINT64, SINT32, SINT16 and SINT8 definition                      */
/************************************************************************/
#ifndef _SINT64_
#define _SINT64_
typedef long long SINT64, *PSINT64;
#endif /*_SINT64_*/

#ifndef _SINT32_
#define _SINT32_
typedef int SINT32, *PSINT32;
#endif /*_SINT32_*/

#ifndef _SINT16_
#define _SINT16_
typedef short SINT16, *PSINT16;
#endif /*_SINT16_*/

#ifndef _SINT8_
#define _SINT8_
typedef char SINT8, *PSINT8;
#endif /*_SINT8_*/

#ifndef _PVOID_
#define _PVOID_
typedef void *PVOID;
#endif /*_PVOID_*/

#ifndef _BOOL_
#define _BOOL_
typedef unsigned char BOOL;
#endif /*_BOOL_*/

#ifndef _U8_
typedef unsigned char  U8;      /* unsigned 8-bit  integer */
#define _U8_
#endif

#ifndef _U16_
typedef unsigned short U16;     /* unsigned 16-bit integer */
#define _U16_
#endif

#ifndef _U32_
typedef unsigned int   U32;     /* unsigned 32-bit integer */
#define _U32_
#endif

#ifndef _U64_
#ifdef __x86_64__
typedef unsigned long  U64;     /* unsigned 64-bit integer */
#else
typedef unsigned long long  U64;     /* unsigned 64-bit integer */
#endif
#define _U64_
#endif

#ifndef _V8_
typedef volatile unsigned char  V8;
#define _V8_
#endif

#ifndef _V16_
typedef volatile unsigned short V16;
#define _V16_
#endif

#ifndef _V32_
typedef volatile unsigned int  V32;
#define _V32_
#endif

#ifndef _S8_
typedef signed char  S8;         /* 8-bit  signed integer */
#define _S8_
#endif

#ifndef _S16_
typedef signed short S16;       /* 16-bit signed integer */
#define _S16_
#endif

#ifndef _S32_
typedef signed int   S32;        /* 32-bit signed integer */
#define _S32_
#endif

#ifndef _S64_
#ifdef __x86_64__
typedef signed long  S64;          /* unsigned 64-bit integer */
#else
typedef signed long long  S64;     /* unsigned 64-bit integer */
#endif
#define _S64_
#endif

#ifndef _PVOID_
#define _PVOID_
typedef void *PVOID;
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#endif

#define CONV_ENDIAN_32(v) ((v & 0xff) << 24 | (v >> 24) | ((v & 0xff00) << 8) | ((v & 0xff0000) >> 8));


#ifndef MAX
#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#endif

#ifndef MIN
#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#endif

#ifdef __cplusplus
}
#endif

typedef void (*VOIDCB)(void);

typedef void (*CALLBACK)(PVOID);

#define BCTRL_LEN_MASK      0x0000FFFF
#define BCTRL_BLAST_MASK    0x00010000

#define DMAFCTRL_IRQEN      0x00000001
#define DMAFCTRL_INBOFF     0x0000FFF0

#define ARRAY_COUNT(v)      (sizeof(v)/sizeof(v[0]))

#ifndef ROUND
#define ROUND(x, dx)  (((x) + ((dx) - 1) ) & ~((dx) - 1))
#endif

#define REG32CLR(addr, clr_mask) ( REG32(addr) = REG32(addr) & (~(clr_mask)) )
#define REG32SET(addr, set_mask) ( REG32(addr) = REG32(addr) | (set_mask) )
#define REG32UPD(addr, clr_mask, set_mask) ( REG32(addr) = (REG32(addr) & (~(clr_mask))) | (set_mask) )

// Standard function return types
#ifndef _RESULTCODE_
#define _RESULTCODE_
typedef unsigned int RESULTCODE;
#endif

typedef unsigned int RETURNVALUE;

#define SUCCESS                     0
#define FAILURE                     1
#define EXPIRED                     2       // Not an error - wait operation expired
#define RESTART                     3       // Not an error - indicate we need to restart process

//#define _DEBUG_

#endif /*__SYSTYPES_H__ */

