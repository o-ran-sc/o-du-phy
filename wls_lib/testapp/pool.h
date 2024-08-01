/**********************************************************************
*
* <COPYRIGHT_TAG>
*
**********************************************************************/

#ifndef _POOL_API_H_
#define _POOL_API_H_

#ifdef __KERNEL__
#include <linux/mutex.h>
#endif
#include <pthread.h>
#include "ttypes.h"


typedef struct _POOL_
{
    unsigned char*      StoragePtr;     // The pointer to the storage where blocks are located
    unsigned int      BlockNum;       // The number of blocks in storage
    unsigned int      BlockSize;      // The size of block in bytes

    unsigned long long*     FreePtr;        // The pointer to the storage with free object indexes
    volatile unsigned long long     FreePut;        // PUT index used to put the new item to 'free' storage
    volatile unsigned long long     FreeGet;        // GET index used to get the new free item from 'free' storage

    unsigned long long*     UsedPtr;        // The pointer to the storage with 'Used' object indexes
    volatile unsigned long long     UsedPut;        // PUT index used to put the new item to 'already used' storage
    volatile unsigned long long     UsedGet;        // GET index used to get the item from 'already used' storage
#ifdef __KERNEL__
    struct mutex lock;
#else
    pthread_mutex_t lock;
#endif
}POOL, *PPOOL;


unsigned int PoolInit (PPOOL pPool, void * pStorage, unsigned int nBlockNum, unsigned int nBlockSize, unsigned long long* pFreePtr, unsigned long long* pUsedPtr);
void*  PoolAlloc(PPOOL pPool);
unsigned int PoolFree(PPOOL pPool, void * pBlock);
unsigned int PoolGetFreeNum(PPOOL pPool);
unsigned int PoolGetAllocNum(PPOOL pPool);

#endif //_POOL_API_H_
