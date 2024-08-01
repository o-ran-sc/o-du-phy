/*******************************************************************************
 *
 * <COPYRIGHT_TAG>
 *
 *******************************************************************************/

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/sched.h>
#endif
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include "pool.h"

/*static void pool_mutex_destroy(pthread_mutex_t* pMutex)
{
    pthread_mutex_destroy(pMutex);
}*/

static void pool_mutex_init(pthread_mutex_t* pMutex)
{
   pthread_mutexattr_t prior;
   pthread_mutexattr_init(&prior);
   pthread_mutexattr_setprotocol(&prior, PTHREAD_PRIO_INHERIT);
   pthread_mutex_init(pMutex, &prior);
   pthread_mutexattr_destroy(&prior);
}

static void pool_mutex_lock(pthread_mutex_t* pMutex)
{
    int nLockRet = 0;
    
    nLockRet = pthread_mutex_lock(pMutex);

    /* Add this to fix Klockwork SV.RVT.RETVAL_NOTTESTED issue */
    if (0 != nLockRet)
    {
        printf("pool mutex lock Error %d\n", nLockRet);
    }
}

static void pool_mutex_unlock(pthread_mutex_t* pMutex)
{
    int nLockRet = 0;
    
    nLockRet = pthread_mutex_unlock(pMutex);

    /* Add this to fix Klockwork SV.RVT.RETVAL_NOTTESTED issue */
    if (0 != nLockRet)
    {
        printf("pool mutex unlock Error %d\n", nLockRet);
    }
}


unsigned int PoolInit (PPOOL pPool, void * pStorage, unsigned int nBlockNum, unsigned int nBlockSize, unsigned long long* pFreePtr, unsigned long long* pUsedPtr)
{
    unsigned int i;

    memset (pPool, 0, sizeof (*pPool));

#ifdef __KERNEL__
    mutex_init(&pPool->lock);
#else
    pool_mutex_init(&pPool->lock);
#endif

    pPool->StoragePtr = (unsigned char*)pStorage;
    pPool->BlockSize  = nBlockSize;
    pPool->BlockNum   = nBlockNum;

    pPool->FreePtr = pFreePtr;
    pPool->UsedPtr = pUsedPtr;

    // to put the indexes to the free storage

    i = 0;

    while (i < nBlockNum)
    {
        PoolFree (pPool, pPool->StoragePtr + (pPool->BlockSize * i));
        i++;
    }

    return 0;
}

void* PoolAlloc(PPOOL pPool)
{
    unsigned long long nIndex;
    void* ret  = NULL;

#ifdef __KERNEL__
    mutex_lock(&pPool->lock);
#else
    pool_mutex_lock(&pPool->lock);
#endif

    if (pPool->FreeGet == pPool->FreePut){
#ifdef __KERNEL__
        mutex_unlock(&pPool->lock);
#else
        pool_mutex_unlock(&pPool->lock);
#endif
        return ret;
    }

    nIndex = pPool->FreePtr[pPool->FreeGet++];

    if (pPool->FreeGet >= (pPool->BlockNum+1))
        pPool->FreeGet = 0;

    ret = pPool->StoragePtr + (pPool->BlockSize * nIndex);

#ifdef __KERNEL__
    mutex_unlock(&pPool->lock);
#else
    pool_mutex_unlock(&pPool->lock);
#endif

    return ret;
}

unsigned int PoolFree(PPOOL pPool, void * pBlock)
{
    unsigned long long index;

#ifdef __KERNEL__
    mutex_lock(&pPool->lock);
#else
    pool_mutex_lock(&pPool->lock);
#endif

    index = (U64)((U64)pBlock - (U64)pPool->StoragePtr) / pPool->BlockSize;

    pPool->FreePtr [pPool->FreePut ++] = index;

    if (pPool->FreePut >= (pPool->BlockNum+1))
        pPool->FreePut = 0;

#ifdef __KERNEL__
    mutex_unlock(&pPool->lock);
#else
    pool_mutex_unlock(&pPool->lock);
#endif

    return 1;
}

unsigned int PoolGetFreeNum(PPOOL pPool)
{
    unsigned int nCount;

    if (pPool==NULL)
        return 0;

    if (pPool->FreePut >= pPool->FreeGet)
    {
        nCount = pPool->FreePut - pPool->FreeGet;
    }
    else
    {
        // the queue size is bigger on one element than a partition
        // to prevent data loss

        nCount = (pPool->BlockNum+1) - (pPool->FreeGet - pPool->FreePut);
    }

    return nCount;
}

unsigned int PoolGetAllocNum(PPOOL pPool)
{
    unsigned int nCount;

    if (pPool==NULL)
        return 0;

    if (pPool->UsedPut >= pPool->UsedGet)
    {
        nCount = pPool->UsedPut - pPool->UsedGet;
    }
    else
    {
        // the queue size is bigger on one element than a partition
        // to prevent data loss

        nCount = (pPool->BlockNum+1) - (pPool->UsedGet - pPool->UsedPut);
    }

    return nCount;
}

