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
#ifdef __KERNEL__
#include <linux/slab.h>
#include <linux/kernel.h>
#else
#include <stdio.h>
#include <string.h>
#endif
#include "syslib.h"
#include "wls.h"

#ifdef __KERNEL__
#ifdef _DEBUG_
#define PRINT_DEBUG(format, args...)            \
do {                                          \
    printk(KERN_INFO "wls debug: " format,##args); \
}while(0)
#else
#define PRINT_DEBUG(x, args...)  do { } while(0)
#endif
#else
#ifdef _DEBUG_
#define PRINT_DEBUG(x, args...)  printf("wls_lib debug: "x, ## args);
#else
#define PRINT_DEBUG(x, args...)  do { } while(0)
#endif
#endif



#define SFL_memcpy  memcpy
/******************************************************************************
*                                                                             *
*     Generic fast queue that operates with pointers (derived from ICC)       *
*                                                                             *
******************************************************************************/

int	SFL_WlsEnqueue(PFASTQUEUE pq, U64 pData, wls_us_addr_conv change_addr, void* hWls)
{
	U32 put = pq->put;
	U32 new_put = put + 1;

    PRINT_DEBUG("off %lx put %d get %d  size %d storage %lx\n",(unsigned long)pq - (unsigned long) hWls,  pq->put, pq->get, pq->size, pq->pStorage);

	if (new_put >= pq->size)
		new_put = 0;

	if (new_put != pq->get)
	{ // the queue is not full

        U64*  pLocalStorage = (U64*) pq->pStorage; // kernel VA
        if (change_addr)
             pLocalStorage = (U64*)change_addr(hWls, (U64)pq->pStorage); // user VA

        PRINT_DEBUG("pLocalStorage %lx\n", (U64)pLocalStorage);

		pLocalStorage[put] = pData;

		DSB();
		pq->put = new_put;
		return TRUE;
	}
	return FALSE;
}


U64 SFL_WlsDequeue(PFASTQUEUE pq,
    wls_us_addr_conv change_addr,
    void *hWls)
{
	U64 p;
    U32   get = pq->get;

	if ((pq->put - get) != 0)
	{
        U64* pLocalStorage = (U64*) pq->pStorage; // kernel VA

        DSB();
        if (change_addr)
            pLocalStorage = (U64 *)change_addr(hWls, (U64)pLocalStorage); //convert to user VA

		p =  pLocalStorage[get++];
		if (get >= pq->size)
			get = 0;

		pq->get = get;
		return p;
	}
	return 0;
}

/*
int	SFL_Enqueue_NoSync(PFASTQUEUE pq, PVOID pData)
{
	U32 put = pq->put;
	U32 new_put = put + 1;
	if (new_put >= pq->size)
		new_put = 0;

	if (new_put != pq->get)
	{ // the queue is not full
		pq->pStorage[ put ] = pData;
		pq->put = new_put;
		return TRUE;
	}
	return FALSE;
}*/

/*
PVOID SFL_Dequeue_NoSync(PFASTQUEUE pq)
{
	PVOID p;
	U32   get = pq->get;

	if ((pq->put - get) != 0)
	{
		p = pq->pStorage[get++];
		if (get >= pq->size)
			get = 0;
		pq->get = get;
		return p;
	}
	return NULL;
}*/

void SFL_DefQueue(PFASTQUEUE pq, void *pStorage, int StorageSize)
{
	memset( (void*) pq, 0x00, sizeof( FASTQUEUE) );
	// always define storage as U64 []
	pq->size = StorageSize >> 3;
	pq->pStorage = (U64)pStorage;

    PRINT_DEBUG("put %d get %d  size %d pq->pStorage %lx\n",pq->put, pq->get, pq->size, pq->pStorage);

}

static U32 sfl_SafeQueueLevel(U32 put, U32 get, U32 size)
{
	U32 nItems;

	if (put >= get)
		nItems = put - get;
	else
		nItems = size + put - get;

	return nItems;
}

U32 WLS_GetNumItemsInTheQueue(PWLS_MSG_QUEUE fpq)
{
	return sfl_SafeQueueLevel(fpq->put, fpq->get, fpq->size);
}

U32 SFL_GetNumItemsInTheQueue(FASTQUEUE *fpq)
{
	return sfl_SafeQueueLevel(fpq->put, fpq->get, fpq->size);
}

/*

U32 SFL_Queue_BatchRead( PFASTQUEUE pq, unsigned long *pDestArr, U32 Count)
{
	if (Count)
	{
		U32 write_index = 0;
		U32 nReads = 0;
		//U32 iMask = SFL_IDisable();
		U32 put = pq->put; // fetch the put atomicly (as app may change it!)
		U32 get = pq->get; // cache the volatile "get index"

		//printf("nItems_%d ", SFL_GetNumItemsInTheQueue(pq));

		if ( (nReads = sfl_SafeQueueLevel(put, get, pq->size)) < Count )
			Count = nReads;
		else
			nReads = Count;

		if (Count >= pq->size - get)
		{
			U32 n = pq->size - get;
			SFL_memcpy( pDestArr, &pq->pStorage[get], sizeof(pDestArr[0]) * n);
			get = 0;
			Count -= n;
			write_index += n;
		}

		if (Count)
		{
			SFL_memcpy( &pDestArr[write_index], &pq->pStorage[get], sizeof(pDestArr[0]) * Count);
			get += Count;
		}

		DSB();
		pq->get = get;

		//printf("nItems_%d ", SFL_GetNumItemsInTheQueue(pq));

		//SFL_IControl(iMask);

		return nReads;
	}
	return FALSE;
}


// the routine does not keep the fifo order (it is used to take items away from the queue)
U32 SFL_Queue_BatchUnload(PFASTQUEUE pq, unsigned long* pDestArr, U32 Count)
{
	if (Count)
	{
		U32 write_index = 0;
		U32 nReads = 0;
		//U32 iMask = SFL_IDisable();
		U32 put = pq->put; // lets cache the volatile "put index"
		U32 get = pq->get; // fetch the get index atomicly (as app may change it)

		//printf("nItems_%d ", SFL_GetNumItemsInTheQueue(pq));

		nReads = sfl_SafeQueueLevel(put, get, pq->size);
		if (nReads)
			nReads -= 1; // decrement is used to cover the case when a reader already started reading from head

		if ( nReads < Count )
			Count = nReads;
		else
			nReads = Count;

		if (!put)
			put = pq->size;

		if (Count >= put)
		{
			U32 n = put;
			SFL_memcpy( pDestArr, &pq->pStorage[0], sizeof(pDestArr[0]) * n);
			put = pq->size;
			Count -= n;
			write_index += n;
		}

		if (Count)
		{
			put -= Count;
			SFL_memcpy( &pDestArr[write_index], &pq->pStorage[put], sizeof(pDestArr[0]) * Count);
		}

		if (put >= pq->size)
			put = 0;

		DSB();
		pq->put = put;

		//printf("nItems_%d ", SFL_GetNumItemsInTheQueue(pq));

		//SFL_IControl(iMask);

		return nReads;
	}
	return FALSE;
}


U32 SFL_Queue_BatchWrite( PFASTQUEUE pq, unsigned long *pSrcArr, U32 Count)
{

	U32 nWrites = Count;

	if (Count)
	{
		U32 read_index = 0;
		U32 put = pq->put;
		//U32 iMask = SFL_IDisable();

		if (pq->size - put <= Count)
		{
			U32 n = pq->size - put;
			SFL_memcpy( &pq->pStorage[put], pSrcArr, sizeof(pSrcArr[0]) * n);
			put = 0;
			Count -= n;
			read_index += n;
		}

		if (Count)
		{
			SFL_memcpy( &pq->pStorage[put], &pSrcArr[read_index], sizeof(pSrcArr[0]) * Count);
			put += Count;
		}

		DSB();
		pq->put = put;

		//SFL_IControl(iMask);
		return nWrites;
	}
	return 0;
}
*/
void WLS_MsgDefineQueue(
	PWLS_MSG_QUEUE pq,
	PWLS_MSG_HANDLE pStorage,
	U32 size,
	U32 sema)
{
	memset( pq, 0x00, sizeof(WLS_MSG_QUEUE));
	pq->pStorage = (U64) pStorage;
	pq->get = 0;
	pq->put = 0;
	pq->size = size; // number of items
	pq->sema = sema;
}

U32 WLS_MsgEnqueue(
	PWLS_MSG_QUEUE pq,
    U64   pIaPaMsg,
    U32   MsgSize,
    U16   TypeID,
    U16   flags,
    wls_us_addr_conv change_addr,
    void *hWls)
{
	U32 rc = 0;
	// below is protected section.
	U32 put = pq->put;
	U32 put_new = put + 1;

	if (put_new >= pq->size)
		put_new = 0;

	if (put_new != pq->get)
	{
        PWLS_MSG_HANDLE pLocalStorage = (PWLS_MSG_HANDLE)pq->pStorage; // kernel VA
        PWLS_MSG_HANDLE pItem;

        PRINT_DEBUG("Kernel VA pq->pStorage %lx put [%d] %d %d\n", pq->pStorage, put_new, pq->get, pq->size);

        if (change_addr)
            pLocalStorage = (PWLS_MSG_HANDLE)change_addr(hWls, (U64)pq->pStorage);

        pItem = &pLocalStorage[put];

		pItem->pIaPaMsg = pIaPaMsg;
		pItem->MsgSize  = MsgSize;
		pItem->TypeID   = TypeID;
		pItem->flags    = flags;
		DSB();
		pq->put = put_new;
		rc = 1;
	}

	return rc;
}

int WLS_MsgDequeue(
	PWLS_MSG_QUEUE pq,
	PWLS_MSG_HANDLE pDestItem,
	wls_us_addr_conv change_addr,
	void *hWls)
{
	int retval = FALSE;
	U32 get = pq->get;
    PWLS_MSG_HANDLE pLocalStorage;

	if (!pDestItem)
		return retval;

    if (get >= pq->size)
    {

        PRINT_DEBUG("error WLS_MsgDequeue get %d size %d\n", get, pq->size);

        return retval;
    }

    pLocalStorage = (PWLS_MSG_HANDLE) pq->pStorage; // kernel VA

    if (pq->put != get)
	{

        DSB();
        if (change_addr)
            pLocalStorage = (PWLS_MSG_HANDLE)change_addr(hWls, (U64) pq->pStorage); //convert to user VA

		*pDestItem = pLocalStorage[get];

		if (++get == pq->size)
			get = 0;

		pq->get = get;
		retval = TRUE;
	}

	return retval;
}
