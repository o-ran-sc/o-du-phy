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

#ifndef __SYSLIB_H__
#define __SYSLIB_H__

typedef unsigned char  U8;      /* unsigned 8-bit  integer */
typedef unsigned short U16;     /* unsigned 16-bit integer */
typedef unsigned int   U32;     /* unsigned 32-bit integer */
#ifdef __x86_64__
typedef unsigned long  U64;     /* unsigned 64-bit integer */
#else
typedef unsigned long long  U64;     /* unsigned 64-bit integer */
#endif

typedef volatile unsigned char  V8;
typedef volatile unsigned short V16;
typedef volatile unsigned int   V32;
typedef volatile unsigned long  V4;

typedef signed char  S8;         /* 8-bit  signed integer */
typedef signed short S16;       /* 16-bit signed integer */
typedef signed int   S32;        /* 32-bit signed integer */

#ifdef __x86_64__
typedef signed long  S64;          /* unsigned 64-bit integer */
#else
typedef signed long long  S64;     /* unsigned 64-bit integer */
#endif

#ifndef _PVOID_
#define _PVOID_
typedef void *PVOID;
#endif

typedef U64 (*wls_us_addr_conv)(void*, U64);


#define K 			1024
#define M			(K*K)
#define KHZ         1000
#define MHZ         (KHZ * KHZ)

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef NULL
#define NULL ((PVOID)(0))
#endif
#define HANDLE	PVOID

#ifdef __KERNEL__
#define DSB()  smp_mb()
#define DMB()  smp_wmb()
#else
#define	DSB()  __asm__ __volatile__("mfence": : :"memory")
#define	DMB()  __asm__ __volatile__("sfence": : :"memory")
#endif

typedef struct tagWLS_MSG_HANDLE
{
    U64   pIaPaMsg;
    U32   pWlsPaMsg;
    U32   MsgSize;
    U16   TypeID;         // used to identify destination
    U16   flags;
    U32   res1;
} WLS_MSG_HANDLE, *PWLS_MSG_HANDLE; /* 4 x QW */

typedef struct tagFASTQUEUE {
    U64 pStorage;
	U32 BlockSize;
	U32 sema;
	V32 get;
	V32 put;
	U32 size;
    U32 res;
} FASTQUEUE, *PFASTQUEUE;

typedef struct tagWLS_MSG_QUEUE {
    U64 pStorage;
	U32 sema;
	V32 get;
	V32 put;
	U32 size;
} WLS_MSG_QUEUE, *PWLS_MSG_QUEUE;

#define COUNT(some_array) ( sizeof(some_array)/sizeof((some_array)[0]) )

void SFL_DefQueue(PFASTQUEUE pq, void *pStorage, int StorageSize);
int	SFL_WlsEnqueue(PFASTQUEUE pq, U64 pData, wls_us_addr_conv change_addr, void* hWls);
int	SFL_Enqueue_NoSync(PFASTQUEUE pq, PVOID pData);
U64 SFL_WlsDequeue(PFASTQUEUE pq, wls_us_addr_conv change_addr, void *hWls);

PVOID SFL_Dequeue_NoSync(PFASTQUEUE pq);
U32 SFL_Queue_BatchRead( PFASTQUEUE pq, unsigned long *pDestArr, U32 Count);
U32 SFL_Queue_BatchWrite( PFASTQUEUE pq, unsigned long *pSrcArr, U32 Count);

void WLS_MsgDefineQueue(PWLS_MSG_QUEUE pq, PWLS_MSG_HANDLE pStorage, U32 size, U32 sema);
U32 WLS_MsgEnqueue(PWLS_MSG_QUEUE pq, U64  pIaPaMsg, U32 MsgSize, U16 TypeID, U16   flags, wls_us_addr_conv change_addr, void* h);
int WLS_MsgDequeue(PWLS_MSG_QUEUE pq, PWLS_MSG_HANDLE pDestItem, wls_us_addr_conv change_addr, void *hWls);
U32 WLS_GetNumItemsInTheQueue(PWLS_MSG_QUEUE fpq);
U32 SFL_GetNumItemsInTheQueue(FASTQUEUE *fpq);





#endif // __SYSLIB_H__


