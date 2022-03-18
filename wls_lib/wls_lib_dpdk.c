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

#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_branch_prediction.h>

#include "ttypes.h"
#include "wls_lib.h"
#include "wls.h"
#include "syslib.h"

#define WLS_MAP_SHM 1

#define WLS_PHY_SHM_FILE_NAME "hp_"

#define HUGE_PAGE_FILE_NAME "/mnt/huge/page"

#define DIV_ROUND_OFFSET(X,Y) ( X/Y + ((X%Y)?1:0) )

#define WLS_LIB_USER_SPACE_CTX_SIZE DMA_MAP_MAX_BLOCK_SIZE

#define PLIB_ERR(x, args...)   printf("wls_lib: "x, ## args);
#define PLIB_INFO(x, args...)  printf("wls_lib: "x, ## args);

#ifdef _DEBUG_
#define PLIB_DEBUG(x, args...)  printf("wls_lib debug: "x, ## args);
#else
#define PLIB_DEBUG(x, args...)  do { } while(0)
#endif

extern int gethugepagesizes(long pagesizes[], int n_elem);


static uint32_t get_hugepagesz_flag(uint64_t hugepage_sz)
{
    unsigned size_flag = 0;

    switch (hugepage_sz) {
        case RTE_PGSIZE_256K:
            size_flag = RTE_MEMZONE_256KB;
            break;
        case RTE_PGSIZE_2M:
            size_flag = RTE_MEMZONE_2MB;
            break;
        case RTE_PGSIZE_16M:
            size_flag = RTE_MEMZONE_16MB;
            break;
        case RTE_PGSIZE_256M:
            size_flag = RTE_MEMZONE_256MB;
            break;
        case RTE_PGSIZE_512M:
            size_flag = RTE_MEMZONE_512MB;
            break;
        case RTE_PGSIZE_1G:
            size_flag = RTE_MEMZONE_1GB;
            break;
        case RTE_PGSIZE_4G:
            size_flag = RTE_MEMZONE_4GB;
            break;
        case RTE_PGSIZE_16G:
            size_flag = RTE_MEMZONE_16GB;
            break;
        default:
            PLIB_INFO("Unknown hugepage size %lu\n", hugepage_sz);
            break;
    }
    return size_flag;
}

static pthread_mutex_t wls_put_lock;
static pthread_mutex_t wls_get_lock;
static pthread_mutex_t wls_put_lock1;
static pthread_mutex_t wls_get_lock1;

static wls_us_ctx_t* wls_us_ctx = NULL;
static wls_us_ctx_t* wls_us_ctx1 = NULL;
static long hugePageSize = WLS_HUGE_DEF_PAGE_SIZE;

static inline int wls_check_ctx(void *h)
{
    if (h != wls_us_ctx) {
        PLIB_ERR("Incorrect user space context\n");
        return -1;
    }
    return 0;
}

static inline int wls_check_ctx1(void *h)
{
    if (h != wls_us_ctx1) {
        PLIB_ERR("Incorrect user space context1\n");
        return -1;
    }
    return 0;
}

static int wls_VirtToIova(const void* virtAddr, uint64_t* iovaAddr)
{
    *iovaAddr = rte_mem_virt2iova(virtAddr);
    if (*iovaAddr==RTE_BAD_IOVA)
        return -1;
    return 0;
}

static void wls_mutex_destroy(pthread_mutex_t* pMutex)
{
    pthread_mutex_destroy(pMutex);
}

static void wls_mutex_init(pthread_mutex_t* pMutex)
{
    pthread_mutexattr_t prior;
    pthread_mutexattr_init(&prior);
    pthread_mutexattr_setprotocol(&prior, PTHREAD_PRIO_INHERIT);
    pthread_mutex_init(pMutex, &prior);
    pthread_mutexattr_destroy(&prior);
}

static void wls_mutex_lock(pthread_mutex_t* pMutex)
{
    pthread_mutex_lock(pMutex);
}

static void wls_mutex_unlock(pthread_mutex_t* pMutex)
{
    pthread_mutex_unlock(pMutex);
}

static int wls_initialize(const char *ifacename, uint64_t nWlsMemorySize)
{
    int ret;
    pthread_mutexattr_t attr;
    uint64_t nSize = nWlsMemorySize + WLS_RUP512B(sizeof(wls_drv_ctx_t));
    struct rte_memzone *mng_memzone;
    wls_drv_ctx_t *mng_ctx;

    if (rte_eal_process_type() != RTE_PROC_PRIMARY)
    {
        PLIB_ERR("Only DPDK primary process can perform initialization \n");
        return -1;
    }

    // Get memory from 1GB huge page and align by 4 Cache Lines
    mng_memzone = (struct rte_memzone *)rte_memzone_reserve_aligned(ifacename, nSize, rte_socket_id(), get_hugepagesz_flag(hugePageSize), hugePageSize);
    if (mng_memzone == NULL) {
        PLIB_ERR("Cannot reserve memory zone[%s]: %s\n", ifacename, rte_strerror(rte_errno));
        return -1;
    }

    mng_ctx = (wls_drv_ctx_t *)(mng_memzone->addr);
    memset(mng_ctx, 0, sizeof(wls_drv_ctx_t));

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    if (ret = pthread_mutex_init(&mng_ctx->mng_mutex, &attr)) {
        pthread_mutexattr_destroy(&attr);
        PLIB_ERR("Failed to initialize mng_mutex %d\n", ret);
        return ret;
    }

    pthread_mutexattr_destroy(&attr);
    PLIB_DEBUG("Run wls_initialized\n");
    return 0;
}

static wls_us_ctx_t* wls_create_us_ctx(wls_drv_ctx_t *pDrv_ctx)
{
    int idx;

    wls_mutex_lock(&pDrv_ctx->mng_mutex);

    if (unlikely(pDrv_ctx->nWlsClients >= WLS_US_CLIENTS_MAX)) {
        PLIB_ERR("Maximum number of clients reached");
        wls_mutex_unlock(&pDrv_ctx->mng_mutex);
        return NULL;
    }

    wls_us_ctx_t *pUsCtx = &pDrv_ctx->p_wls_us_ctx[pDrv_ctx->nWlsClients];
    wls_us_priv_t *pUs_priv = &pUsCtx->wls_us_private;

    PLIB_DEBUG("wls_create_us_ctx for %d client\n", pDrv_ctx->nWlsClients);
    memset(pUsCtx, 0, sizeof (wls_us_ctx_t));

    SFL_DefQueue(&pUsCtx->ul_free_block_pq, pUsCtx->ul_free_block_storage,
        UL_FREE_BLOCK_QUEUE_SIZE * sizeof (void*));
    WLS_MsgDefineQueue(&pUsCtx->get_queue, pUsCtx->get_storage, WLS_GET_QUEUE_N_ELEMENTS, 0);
    WLS_MsgDefineQueue(&pUsCtx->put_queue, pUsCtx->put_storage, WLS_PUT_QUEUE_N_ELEMENTS, 0);

    memset(pUs_priv, 0, sizeof (wls_us_priv_t));
    if (sem_init(&pUs_priv->sema.sem, 1, 0)) {
        PLIB_ERR("Failed to initialize semaphore %s\n", strerror(errno));
        memset(pUsCtx, 0, sizeof (wls_us_ctx_t));
        wls_mutex_unlock(&pDrv_ctx->mng_mutex);
        return NULL;
    }
    rte_atomic16_init(&pUs_priv->sema.is_irq);

    idx = pDrv_ctx->nWlsClients;
    pDrv_ctx->p_wls_us_ctx[idx].dst_user_va = (uint64_t) & pDrv_ctx->p_wls_us_ctx[idx ^ 1];
    PLIB_INFO("link: %d <-> %d\n", idx, idx ^ 1);

    pUs_priv->pid = getpid();
    pDrv_ctx->nWlsClients++;

    wls_mutex_unlock(&pDrv_ctx->mng_mutex);
    return pUsCtx;
}

static int wls_destroy_us_ctx(wls_us_ctx_t *pUsCtx, wls_drv_ctx_t *pDrv_ctx)
{
    wls_us_priv_t* pUs_priv = NULL;

    wls_mutex_lock(&pDrv_ctx->mng_mutex);
    if (pDrv_ctx->p_wls_us_ctx[0].wls_us_private.pid
        && pDrv_ctx->p_wls_us_ctx[1].wls_us_private.pid) {
        PLIB_INFO("un-link: 0 <-> 1\n");
        pDrv_ctx->p_wls_us_ctx[0].dst_user_va = 0ULL;
        pDrv_ctx->p_wls_us_ctx[1].dst_user_va = 0ULL;
    }


    if (pUsCtx) {
        pUs_priv = (wls_us_priv_t*) & pUsCtx->wls_us_private;
        if (pUs_priv) {
            sem_destroy(&pUs_priv->sema.sem);
            memset(pUsCtx, 0, sizeof (wls_us_ctx_t));
            pDrv_ctx->nWlsClients--;
        }
    }

    wls_mutex_unlock(&pDrv_ctx->mng_mutex);
    return 0;
}

static int wls_destroy_us_ctx1(wls_us_ctx_t *pUsCtx, wls_drv_ctx_t *pDrv_ctx)
{
    wls_us_priv_t* pUs_priv = NULL;

    wls_mutex_lock(&pDrv_ctx->mng_mutex);
    if (pDrv_ctx->p_wls_us_ctx[2].wls_us_private.pid
        && pDrv_ctx->p_wls_us_ctx[3].wls_us_private.pid) {
        PLIB_INFO("un-link: 2 <-> 3\n");
        pDrv_ctx->p_wls_us_ctx[2].dst_user_va = 0ULL;
        pDrv_ctx->p_wls_us_ctx[3].dst_user_va = 0ULL;
    }


    if (pUsCtx) {
        pUs_priv = (wls_us_priv_t*) & pUsCtx->wls_us_private;
        if (pUs_priv) {
            sem_destroy(&pUs_priv->sema.sem);
            memset(pUsCtx, 0, sizeof (wls_us_ctx_t));
            pDrv_ctx->nWlsClients--;
        }
    }

    wls_mutex_unlock(&pDrv_ctx->mng_mutex);
    return 0;
}

static int wls_wake_up_user_thread(char *buf, wls_sema_priv_t *semap)
{
    if (unlikely(rte_atomic16_read(&semap->is_irq) >= FIFO_LEN))
        return 0;

    unsigned int put = semap->drv_block_put + 1;
    if (put >= FIFO_LEN)
        put = 0;
    memcpy(&semap->drv_block[put], buf, sizeof (wls_wait_req_t));
    semap->drv_block_put = put;
    rte_atomic16_inc(&semap->is_irq);
    PLIB_DEBUG("PUT: put=%d get=%d T=%lu is_irq=%d\n",
            semap->drv_block_put, semap->drv_block_get,
            semap->drv_block[put].start_time, rte_atomic16_read(&semap->is_irq));
    sem_post(&semap->sem);
    return 0;
}

static int wls_process_put(wls_us_ctx_t *src, wls_us_ctx_t *dst)
{
    int ret = 0;
    WLS_MSG_HANDLE hMsg;
    int n = 0;

    wls_us_priv_t* pDstPriv = NULL;
    wls_wait_req_t drv_block;

    if (NULL == src || NULL == dst) {
        PLIB_DEBUG("Bad input addresses\n");
        return -1;
    }

    n = WLS_GetNumItemsInTheQueue(&src->put_queue);

    while (n--) {
        if (WLS_MsgDequeue(&src->put_queue, &hMsg, NULL, (void*) src) == FALSE) {
            PLIB_ERR("WLS_MsgDequeue src failed\n");
            ret = -1;
            break;
        }
        PLIB_DEBUG("WLS_Get %lx %d type %d\n", (U64) hMsg.pIaPaMsg, hMsg.MsgSize, hMsg.TypeID);
        if (WLS_MsgEnqueue(&dst->get_queue, hMsg.pIaPaMsg, hMsg.MsgSize, hMsg.TypeID,
                hMsg.flags, NULL, (void*) dst) == FALSE) { // try to send
            if (WLS_MsgEnqueue(&src->put_queue, hMsg.pIaPaMsg, hMsg.MsgSize, hMsg.TypeID,
                    hMsg.flags, NULL, (void*) src) == FALSE) { // return back
                PLIB_ERR("wls_process_put: Cannot return block to back to queue \n");
                ret = -1;
            }
            break;
        }
    }

    if (dst->wls_us_private.pid) {
        pDstPriv = (wls_us_priv_t*) & dst->wls_us_private;

        drv_block.start_time = wls_rdtsc();
        pDstPriv->NeedToWakeUp = 1;
        wls_wake_up_user_thread((char *) &drv_block, &pDstPriv->sema);
    } else
        ret = -1;

    return ret;
}

static int wls_process_wakeup(void* h)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;
    int ret = 0;
    wls_wait_req_t drv_block;
    if (wls_check_ctx(h))
        return -1;
    if (!pWls_us->wls_us_private.pid) {
        PLIB_ERR("wakeup failed");
        return -1;
    }
    drv_block.start_time = wls_rdtsc();
    wls_wake_up_user_thread((char *) &drv_block, &pWls_us->wls_us_private.sema);

    return ret;
}

static int wls_process_wakeup1(void* h)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;
    int ret = 0;
    wls_wait_req_t drv_block;
    if (wls_check_ctx1(h))
        return -1;
    if (!pWls_us->wls_us_private.pid) {
        PLIB_ERR("wakeup failed");
        return -1;
    }
    drv_block.start_time = wls_rdtsc();
    wls_wake_up_user_thread((char *) &drv_block, &pWls_us->wls_us_private.sema);

    return ret;
}

static int wls_wait(wls_sema_priv_t *priv)
{
    if (!rte_atomic16_read(&priv->is_irq)) {
        if (sem_wait(&priv->sem) || !rte_atomic16_read(&priv->is_irq)) {
            return -1;
        }
    }

    rte_atomic16_dec(&priv->is_irq);

    if (priv->drv_block_put != priv->drv_block_get) {
        unsigned int get = priv->drv_block_get + 1;

        if (get >= FIFO_LEN)
            get = 0;

        priv->drv_block_get = get;

        PLIB_DEBUG("GET: put=%d get=%d T=%lu is_irq=%d\n",
                priv->drv_block_put, priv->drv_block_get,
                priv->drv_block[get].start_time, rte_atomic16_read(&priv->is_irq));
    } else {
        PLIB_DEBUG("[wrong computation of queueing\n");
    }

    return 0;
}

static int wls_process_wait(void* h)
{
    wls_us_ctx_t* pUsCtx = (wls_us_ctx_t*) h;

    if (wls_check_ctx(h))
        return -1;

    if (pUsCtx == NULL) {
        PLIB_ERR("Wait failed on User context");
        return -1;
    }


    if (!pUsCtx->wls_us_private.pid) {
        PLIB_ERR("Wait failed");
        return -1;
    }
    pUsCtx->wls_us_private.isWait = 1;
    wls_wait(&pUsCtx->wls_us_private.sema);
    pUsCtx->wls_us_private.isWait = 0;
    return WLS_GetNumItemsInTheQueue(&pUsCtx->get_queue);
}

static int wls_process_wait1(void* h)
{
    wls_us_ctx_t* pUsCtx = (wls_us_ctx_t*) h;

    if (wls_check_ctx1(h))
        return -1;

    if (pUsCtx == NULL) {
        PLIB_ERR("Wait failed on User context");
        return -1;
    }


    if (!pUsCtx->wls_us_private.pid) {
        PLIB_ERR("Wait failed");
        return -1;
    }
    pUsCtx->wls_us_private.isWait = 1;
    wls_wait(&pUsCtx->wls_us_private.sema);
    pUsCtx->wls_us_private.isWait = 0;
    return WLS_GetNumItemsInTheQueue(&pUsCtx->get_queue);
}


void *WLS_Open(const char *ifacename, unsigned int mode, uint64_t *nWlsMacMemorySize, uint64_t *nWlsPhyMemorySize)
{
    wls_us_ctx_t* pWls_us = NULL;
    wls_drv_ctx_t *pWlsDrvCtx;
    int i, len;
    char temp[WLS_DEV_SHM_NAME_LEN] = {0};
    static const struct rte_memzone *mng_memzone;

    gethugepagesizes(&hugePageSize, 1);

    if (wls_us_ctx)
        return wls_us_ctx;

    strncpy(temp, ifacename, WLS_DEV_SHM_NAME_LEN - 1);
    PLIB_INFO("Open %s (DPDK memzone)\n", temp);

    mng_memzone = (struct rte_memzone *)rte_memzone_lookup(temp);
    if (mng_memzone == NULL)
    {
        if (mode == WLS_SLAVE_CLIENT)
        {
            wls_initialize(temp, *nWlsMacMemorySize+*nWlsPhyMemorySize);
            mng_memzone = (struct rte_memzone *)rte_memzone_lookup(temp);
            if (mng_memzone == NULL)
            {
        PLIB_ERR("Cannot initialize wls shared memory: %s\n", temp);
        return NULL;
    }
        }
        else
        {
            PLIB_ERR("Cannot locate memory zone: %s. Is the Primary Process running?\n", temp);
            return NULL;
        }
    }

    pWlsDrvCtx = (wls_drv_ctx_t *)(mng_memzone->addr);
    PLIB_INFO("WLS_Open %p\n", pWlsDrvCtx);
    if (mode == WLS_SLAVE_CLIENT)
    {
        pWlsDrvCtx->nMacBufferSize = *nWlsMacMemorySize;
        pWlsDrvCtx->nPhyBufferSize = *nWlsPhyMemorySize;
    }
    else
    {
        *nWlsMacMemorySize = pWlsDrvCtx->nMacBufferSize;
        *nWlsPhyMemorySize = pWlsDrvCtx->nPhyBufferSize;
    }

    if ((pWls_us = wls_create_us_ctx(pWlsDrvCtx)) == NULL)
    {
        PLIB_ERR("WLS_Open failed to create context\n");
        return NULL;
    }

    PLIB_DEBUG("Local: pWls_us %p\n", pWls_us);

    pWls_us->padding_wls_us_user_space_va = 0LL;
    pWls_us->wls_us_user_space_va = pWls_us;
    pWls_us->wls_us_ctx_size = sizeof (*pWls_us);

    wls_mutex_init(&wls_put_lock);
    wls_mutex_init(&wls_get_lock);

    pWls_us->mode = mode;
    pWls_us->secmode = WLS_SEC_NA;
    pWls_us->dualMode = WLS_SINGLE_MODE;
    PLIB_INFO("Mode %d\n", pWls_us->mode);

    PLIB_INFO("WLS shared management memzone: %s\n", temp);
    strncpy(pWls_us->wls_dev_name, temp, WLS_DEV_SHM_NAME_LEN - 1);
    wls_us_ctx = pWls_us;

    return wls_us_ctx;
}

void *WLS_Open_Dual(const char *ifacename, unsigned int mode, uint64_t *nWlsMacMemorySize, uint64_t *nWlsPhyMemorySize, void** handle1)
{
    wls_us_ctx_t* pWls_us = NULL;
    wls_us_ctx_t* pWls_us1 = NULL;
    wls_drv_ctx_t *pWlsDrvCtx;
    int i, len;
    char temp[WLS_DEV_SHM_NAME_LEN] = {0};
    static const struct rte_memzone *mng_memzone;

    gethugepagesizes(&hugePageSize, 1);

    if (wls_us_ctx)
        return wls_us_ctx;

    strncpy(temp, ifacename, WLS_DEV_SHM_NAME_LEN - 1);
    PLIB_INFO("Open %s (DPDK memzone)\n", temp);

    mng_memzone = (struct rte_memzone *)rte_memzone_lookup(temp);
    if (mng_memzone == NULL)
    {
        if (mode == WLS_SLAVE_CLIENT)
        {
            wls_initialize(temp, *nWlsMacMemorySize+*nWlsPhyMemorySize);
            mng_memzone = (struct rte_memzone *)rte_memzone_lookup(temp);
            if (mng_memzone == NULL)
            {
        PLIB_ERR("Cannot initialize wls shared memory: %s\n", temp);
        return NULL;
    }
        }
        else
        {
            PLIB_ERR("Cannot locate memory zone: %s. Is the Primary Process running?\n", temp);
            return NULL;
        }
    }

    pWlsDrvCtx = (wls_drv_ctx_t *)(mng_memzone->addr);
    *nWlsMacMemorySize = pWlsDrvCtx->nMacBufferSize;
    *nWlsPhyMemorySize = pWlsDrvCtx->nPhyBufferSize;
    PLIB_INFO("nWlsMemorySize is %lu\n", *nWlsMacMemorySize+*nWlsPhyMemorySize);
    PLIB_INFO("WLS_Open Dual 1 %p\n", pWlsDrvCtx);

    if ((pWls_us = wls_create_us_ctx(pWlsDrvCtx)) == NULL)
    {
        PLIB_ERR("WLS_Open Dual 1 failed to create context\n");
        return NULL;
     }
     else
     {    
        PLIB_DEBUG("Local: pWls_us %p\n", pWls_us);
 
        pWls_us->padding_wls_us_user_space_va = 0LL;
        pWls_us->wls_us_user_space_va = pWls_us;
        pWls_us->wls_us_ctx_size = sizeof (*pWls_us);

        wls_mutex_init(&wls_put_lock);
        wls_mutex_init(&wls_get_lock);

        pWls_us->mode = mode;
        pWls_us->secmode = WLS_SEC_MASTER;
        pWls_us->dualMode = WLS_DUAL_MODE;
        PLIB_INFO("Mode %d SecMode %d \n", pWls_us->mode, pWls_us->secmode);

        PLIB_INFO("WLS shared management memzone 1: %s\n", temp);
        strncpy(pWls_us->wls_dev_name, temp, WLS_DEV_SHM_NAME_LEN - 1);
        wls_us_ctx = pWls_us;
	    PLIB_INFO("pWLs_us is %p\n", wls_us_ctx);
	    *handle1 = pWls_us;  // Now the first context is for L1-FT_iapi
     }
     
     // Create second context to support the second wls shared memory interface
    if ((pWls_us1 = wls_create_us_ctx(pWlsDrvCtx)) == NULL)
    {
        PLIB_ERR("WLS_Open Dual failed to create context 1\n");
        return NULL;
     }
     else
     {
        PLIB_DEBUG("Local: pWls_us1 %p\n", pWls_us1);

        pWls_us1->padding_wls_us_user_space_va = 0LL;
        pWls_us1->wls_us_user_space_va = pWls_us1;
        pWls_us1->wls_us_ctx_size = sizeof (*pWls_us1);

        wls_mutex_init(&wls_put_lock1);
        wls_mutex_init(&wls_get_lock1);

        pWls_us1->mode = mode;
        pWls_us1->secmode = WLS_SEC_NA;
        PLIB_INFO("Mode %d SecMode %d \n", pWls_us1->mode, pWls_us1->secmode);

        PLIB_INFO("WLS shared management memzone 2: %s\n", temp);
        strncpy(pWls_us1->wls_dev_name, temp, WLS_DEV_SHM_NAME_LEN - 1);
    	wls_us_ctx1 = pWls_us1; // Now the second context is for the L2-FT_fapi
	    PLIB_INFO("pWLs_us1 is %p\n", wls_us_ctx1);
     }
    return wls_us_ctx1; // returning second context preserves the L2 legacy code
}

int WLS_Ready(void* h)
{
    wls_us_ctx_t *pWls_us = (wls_us_ctx_t*) h;
    wls_us_ctx_t *pWls_usRem = (wls_us_ctx_t*) pWls_us->dst_user_va;

    if (!wls_us_ctx) {
        PLIB_ERR("Library was not opened\n");
        return -1;
    }

    if (pWls_usRem->wls_us_private.pid) {
        return 0;
    }
    return -1;
}

int WLS_Ready1(void* h)
{
    wls_us_ctx_t *pWls_us = (wls_us_ctx_t*) h;
    wls_us_ctx_t *pWls_usRem = (wls_us_ctx_t*) pWls_us->dst_user_va;

    if (!wls_us_ctx1) {
        PLIB_ERR("Library was not opened for Context 1\n");
        return -1;
    }

    if (pWls_usRem->wls_us_private.pid) {
        return 0;
    }
    return -1;
}

int WLS_Close(void* h)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;
    wls_drv_ctx_t *pDrv_ctx;
    struct rte_memzone *mng_memzone;
    int ret = 0;

    if (!wls_us_ctx) {
        PLIB_ERR("Library was not opened\n");
        return -1;
    }

    if (wls_check_ctx(h))
        return -1;

    mng_memzone = (struct rte_memzone *)rte_memzone_lookup(pWls_us->wls_dev_name);
    if (mng_memzone == NULL) {
        PLIB_ERR("Cannot find mng memzone: %s %s\n",
                    pWls_us->wls_dev_name, rte_strerror(rte_errno));
        return -1;
    }
    pDrv_ctx = (wls_drv_ctx_t *)(mng_memzone->addr);

    PLIB_INFO("WLS_Close\n");
    if ((ret = wls_destroy_us_ctx(pWls_us, pDrv_ctx)) < 0) {
        PLIB_ERR("Close failed [%d]\n", ret);
        return ret;
    }

    wls_mutex_destroy(&wls_put_lock);
    wls_mutex_destroy(&wls_get_lock);

    wls_us_ctx = NULL;

    printf("WLS_Close: nWlsClients[%d]\n", pDrv_ctx->nWlsClients);
    if (0 == pDrv_ctx->nWlsClients) {
        printf("WLS_Close: Freeing Allocated MemZone\n");
        wls_mutex_destroy(&pDrv_ctx->mng_mutex);
        rte_memzone_free(mng_memzone);
    }
    return 0;
}

int WLS_Close1(void* h)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;
    wls_drv_ctx_t *pDrv_ctx;
    struct rte_memzone *mng_memzone;
    int ret = 0;

    if (!wls_us_ctx1) {
        PLIB_ERR("Library was not opened\n");
        return -1;
    }

    if (wls_check_ctx1(h))
        return -1;

    mng_memzone = (struct rte_memzone *)rte_memzone_lookup(pWls_us->wls_dev_name);
    if (mng_memzone == NULL) {
        PLIB_ERR("Cannot find mng memzone: %s %s\n",
                    pWls_us->wls_dev_name, rte_strerror(rte_errno));
        return -1;
    }
    pDrv_ctx = (wls_drv_ctx_t *)(mng_memzone->addr);

    PLIB_INFO("WLS_Close1\n");
    if ((ret = wls_destroy_us_ctx1(pWls_us, pDrv_ctx)) < 0) {
        PLIB_ERR("Close failed [%d]\n", ret);
        return ret;
    }

    wls_mutex_destroy(&wls_put_lock1);
    wls_mutex_destroy(&wls_get_lock1);

    wls_us_ctx1 = NULL;

    printf("WLS_Close1: nWlsClients[%d]\n", pDrv_ctx->nWlsClients);
    if (0 == pDrv_ctx->nWlsClients) {
        printf("WLS_Close1: Freeing Allocated MemZone\n");
        wls_mutex_destroy(&pDrv_ctx->mng_mutex);
        rte_memzone_free(mng_memzone);
    }
    return 0;
}

uint32_t WLS_SetMode(void* h, unsigned int mode)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;
    pWls_us->mode = mode;
    return 0;
    }

void* WLS_Alloc(void* h, uint64_t size)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;
    wls_drv_ctx_t *pDrv_ctx;
    hugepage_tabl_t* pHugePageTlb = &pWls_us->hugepageTbl[0];
    void *pvirtAddr = NULL;
    struct rte_memzone *mng_memzone;
    uint32_t nHugePage;
    uint64_t HugePageMask, alloc_base, alloc_end;

    // TODOINFO null/value check was removed. Check if it works properly.
    PLIB_INFO("hugePageSize on the system is %ld 0x%08lx\n", hugePageSize, hugePageSize);

    mng_memzone = (struct rte_memzone *)rte_memzone_lookup(pWls_us->wls_dev_name);
    if (mng_memzone == NULL)
    {
        PLIB_ERR("Cannot find mng memzone: %s %s\n",
                    pWls_us->wls_dev_name, rte_strerror(rte_errno));
        return NULL;
    }

    pDrv_ctx = mng_memzone->addr;

    pvirtAddr = (void *)((uint8_t *)pDrv_ctx + WLS_RUP512B(sizeof(wls_drv_ctx_t)));
    HugePageMask = ((unsigned long) hugePageSize - 1);
    alloc_base = (uint64_t) pvirtAddr & ~HugePageMask;
    alloc_end = (uint64_t) pvirtAddr + (size - WLS_RUP512B(sizeof(wls_drv_ctx_t)));

    nHugePage = 0;
    while(1)
    {
        /*Increment virtual address to next hugepage to create table*/
        pHugePageTlb[nHugePage].pageVa = (alloc_base + (nHugePage * hugePageSize));

        if (pHugePageTlb[nHugePage].pageVa > alloc_end)
            break;

        /* Creating dummy page fault in process for each page inorder to get pagemap */
        (*(unsigned char*) pHugePageTlb[nHugePage].pageVa) = 1;

        if (wls_VirtToIova((uint64_t*) pHugePageTlb[nHugePage].pageVa, &pHugePageTlb[nHugePage].pagePa) == -1)
        {
            PLIB_ERR("Virtual to physical conversion failed\n");
            return NULL;
        }

        nHugePage++;
    }

    PLIB_DEBUG("count is %d, pHugePageTlb->pageVa is %p  pHugePageTlb1->pageVa is %p  pHugePageTlb2->pageVa is %p\n",count, pHugePageTlb->pageVa, pHugePageTlb1->pageVa, pHugePageTlb2->pageVa);
    PLIB_INFO("WLS_Alloc Size Requested [%ld] bytes HugePageSize [0x%08lx] nHugePagesMapped[%d]\n", size, hugePageSize, nHugePage);

    pWls_us->HugePageSize = (uint32_t) hugePageSize;
    pWls_us->alloc_buffer = pvirtAddr;
    pWls_us->nHugePage    = nHugePage;

    if ((pWls_us->mode == WLS_MASTER_CLIENT)||(pWls_us->secmode == WLS_SEC_MASTER)) {
        wls_us_ctx_t *pWls_usRem = (wls_us_ctx_t*) pWls_us->dst_user_va;
        PLIB_INFO("Connecting to remote peer ...\n");
        while (pWls_usRem->wls_us_private.pid == 0) { // wait for slave
        }

        PLIB_INFO("Connected to remote peer\n");
        pWls_us->dst_user_va = (uint64_t) pWls_usRem;
    }
    return pvirtAddr;
}

int WLS_Free(void* h, PVOID pMsg)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;
    wls_drv_ctx_t *pDrv_ctx;
    struct rte_memzone *mng_memzone;

    mng_memzone = (struct rte_memzone *)rte_memzone_lookup(pWls_us->wls_dev_name);
    if (mng_memzone == NULL) {
        PLIB_ERR("Cannot find mng memzone: %s %s\n",
                    pWls_us->wls_dev_name, rte_strerror(rte_errno));
        return -1;
    }
    pDrv_ctx = mng_memzone->addr;

    if (pMsg !=  pWls_us->alloc_buffer) {
        PLIB_ERR("incorrect pMsg %p [expected %p]\n", pMsg, pWls_us->alloc_buffer);
        return -1;
    }

    if ((pWls_us->mode == WLS_MASTER_CLIENT)||(pWls_us->mode == WLS_SEC_MASTER)) {
        if (pWls_us->dst_user_va) {
            pWls_us->dst_user_va = 0;
        }
    }

    PLIB_DEBUG("WLS_Free %s\n", shm_name);

    return 0;
}

int WLS_Put(void *h, unsigned long long pMsg, unsigned int MsgSize, unsigned short MsgTypeID, unsigned short Flags)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;
    int ret = 0;
    unsigned short nFlags = Flags & (~WLS_TF_URLLC);

    if (wls_check_ctx(h))
        return -1;

    if (!WLS_IS_ONE_HUGE_PAGE(pMsg, MsgSize, hugePageSize)) {
        PLIB_ERR("WLS_Put input error: buffer is crossing 2MB page boundary %lx size %u\n",
                (U64) pMsg, MsgSize);
    }

    wls_mutex_lock(&wls_put_lock);

    if ((WLS_FLAGS_MASK & nFlags)) { // multi block transaction
        if (nFlags & WLS_TF_SYN) {
            PLIB_DEBUG("WLS_SG_FIRST\n");
            if (WLS_MsgEnqueue(&pWls_us->put_queue, pMsg, MsgSize, MsgTypeID,
                    Flags, NULL, (void*) pWls_us)) {
                PLIB_DEBUG("WLS_Get %lx %d type %d\n", (U64) pMsg, MsgSize, MsgTypeID);
            }
        } else if ((nFlags & WLS_TF_SCATTER_GATHER)
                    && !(nFlags & WLS_TF_SYN)
                    && !(nFlags & WLS_TF_FIN)) {
            PLIB_DEBUG("WLS_SG_NEXT\n");
            if (WLS_MsgEnqueue(&pWls_us->put_queue, pMsg, MsgSize, MsgTypeID,
                    Flags, NULL, (void*) pWls_us)) {
                PLIB_DEBUG("WLS_Put %lx %d type %d\n", (U64) pMsg, MsgSize, MsgTypeID);
            }
        } else if (nFlags & WLS_TF_FIN) {
            if (WLS_MsgEnqueue(&pWls_us->put_queue, pMsg, MsgSize, MsgTypeID,
                    Flags, NULL, (void*) pWls_us)) {
                PLIB_DEBUG("WLS_Put %lx %d type %d\n", (U64) pMsg, MsgSize, MsgTypeID);
            }

            PLIB_DEBUG("List: call wls_process_put\n");
            if (pWls_us->dst_user_va) {
                if ((ret = wls_process_put(pWls_us, (wls_us_ctx_t*) pWls_us->dst_user_va)) < 0) {
                    PLIB_ERR("Put failed [%d]\n", ret);
                    wls_mutex_unlock(&wls_put_lock);
                    return -1;
                }
            }
        } else
            PLIB_ERR("unsupported flags %x\n", WLS_FLAGS_MASK & Flags);
    } else { // one block transaction
        if (WLS_MsgEnqueue(&pWls_us->put_queue, pMsg, MsgSize, MsgTypeID,
                Flags, NULL, (void*) pWls_us)) {
            PLIB_DEBUG("WLS_Put %lx %d type %d\n", (U64) pMsg, MsgSize, MsgTypeID);
        }

        PLIB_DEBUG("One block: call wls_process_put\n");
        if (likely(pWls_us->dst_user_va)) {
            if ((ret = wls_process_put(pWls_us, (wls_us_ctx_t*) pWls_us->dst_user_va)) < 0) {
                PLIB_ERR("Put failed [%d]\n", ret);
                wls_mutex_unlock(&wls_put_lock);
                return -1;
            }
        } else {
            PLIB_ERR("Destination address is empty\n");
            wls_mutex_unlock(&wls_put_lock);
            return -1;
        }
    }
    wls_mutex_unlock(&wls_put_lock);

    return 0;
}

int WLS_Put1(void *h, unsigned long long pMsg, unsigned int MsgSize, unsigned short MsgTypeID, unsigned short Flags)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;
    int ret = 0;
    unsigned short nFlags = Flags & (~WLS_TF_URLLC);    

    if (wls_check_ctx1(h))
        return -1;

    if (!WLS_IS_ONE_HUGE_PAGE(pMsg, MsgSize, hugePageSize)) {
        PLIB_ERR("WLS_Put input error: buffer is crossing 2MB page boundary %lx size %u\n",
                (U64) pMsg, MsgSize);
    }

    wls_mutex_lock(&wls_put_lock1);

    if ((WLS_FLAGS_MASK & nFlags)) { // multi block transaction
        if (nFlags & WLS_TF_SYN) {
            PLIB_DEBUG("WLS_SG_FIRST\n");
            if (WLS_MsgEnqueue(&pWls_us->put_queue, pMsg, MsgSize, MsgTypeID,
                    Flags, NULL, (void*) pWls_us)) {
                PLIB_DEBUG("WLS_Get %lx %d type %d\n", (U64) pMsg, MsgSize, MsgTypeID);
            }
        } else if ((nFlags & WLS_TF_SCATTER_GATHER)
                    && !(nFlags & WLS_TF_SYN)
                    && !(nFlags & WLS_TF_FIN)) {
            PLIB_DEBUG("WLS_SG_NEXT\n");
            if (WLS_MsgEnqueue(&pWls_us->put_queue, pMsg, MsgSize, MsgTypeID,
                    Flags, NULL, (void*) pWls_us)) {
                PLIB_DEBUG("WLS_Put %lx %d type %d\n", (U64) pMsg, MsgSize, MsgTypeID);
            }
        } else if (nFlags & WLS_TF_FIN) {
            if (WLS_MsgEnqueue(&pWls_us->put_queue, pMsg, MsgSize, MsgTypeID,
                    Flags, NULL, (void*) pWls_us)) {
                PLIB_DEBUG("WLS_Put %lx %d type %d\n", (U64) pMsg, MsgSize, MsgTypeID);
            }

            PLIB_DEBUG("List: call wls_process_put\n");
            if (pWls_us->dst_user_va) {
                if ((ret = wls_process_put(pWls_us, (wls_us_ctx_t*) pWls_us->dst_user_va)) < 0) {
                    PLIB_ERR("Put failed [%d]\n", ret);
                    wls_mutex_unlock(&wls_put_lock1);
                    return -1;
                }
            }
        } else
            PLIB_ERR("unsupported flags %x\n", WLS_FLAGS_MASK & Flags);
    } else { // one block transaction
        if (WLS_MsgEnqueue(&pWls_us->put_queue, pMsg, MsgSize, MsgTypeID,
                Flags, NULL, (void*) pWls_us)) {
            PLIB_DEBUG("WLS_Put %lx %d type %d\n", (U64) pMsg, MsgSize, MsgTypeID);
        }

        PLIB_DEBUG("One block: call wls_process_put\n");
        if (likely(pWls_us->dst_user_va)) {
            if ((ret = wls_process_put(pWls_us, (wls_us_ctx_t*) pWls_us->dst_user_va)) < 0) {
                PLIB_ERR("Put failed [%d]\n", ret);
                wls_mutex_unlock(&wls_put_lock1);
                return -1;
            }
        } else {
            PLIB_ERR("Destination address is empty\n");
            wls_mutex_unlock(&wls_put_lock1);
            return -1;
        }
    }
    wls_mutex_unlock(&wls_put_lock1);

    return 0;
}

int WLS_Check(void* h)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;

    if (wls_check_ctx(h))
        return 0;

    return WLS_GetNumItemsInTheQueue(&pWls_us->get_queue);
}

int WLS_Check1(void* h)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;

    if (wls_check_ctx1(h))
        return 0;

    return WLS_GetNumItemsInTheQueue(&pWls_us->get_queue);
}

unsigned long long WLS_Get(void* h, unsigned int *MsgSize, unsigned short *MsgTypeID, unsigned short *Flags)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;
    WLS_MSG_HANDLE hMsg;
    uint64_t pMsg = (uint64_t) NULL;

    if (wls_check_ctx(h))
    {
    	PLIB_ERR("WLS_Get fails wls_check_ctx\n");
        return 0;
    }

    wls_mutex_lock(&wls_get_lock);

    if (WLS_MsgDequeue(&pWls_us->get_queue, &hMsg, NULL, (void*) pWls_us)) {
        PLIB_DEBUG("WLS_Get %lx %d type %d\n", (U64) hMsg.pIaPaMsg, hMsg.MsgSize, hMsg.TypeID);
        pMsg = hMsg.pIaPaMsg;
        *MsgSize = hMsg.MsgSize;
        *MsgTypeID = hMsg.TypeID;
        *Flags = hMsg.flags;
    }

    wls_mutex_unlock(&wls_get_lock);

    return pMsg;
}

unsigned long long WLS_Get1(void* h, unsigned int *MsgSize, unsigned short *MsgTypeID, unsigned short *Flags)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;
    WLS_MSG_HANDLE hMsg;
    uint64_t pMsg = (uint64_t) NULL;

    if (wls_check_ctx1(h))
        return 0;

    wls_mutex_lock(&wls_get_lock1);

    if (WLS_MsgDequeue(&pWls_us->get_queue, &hMsg, NULL, (void*) pWls_us)) {
        PLIB_DEBUG("WLS_Get %lx %d type %d\n", (U64) hMsg.pIaPaMsg, hMsg.MsgSize, hMsg.TypeID);
        pMsg = hMsg.pIaPaMsg;
        *MsgSize = hMsg.MsgSize;
        *MsgTypeID = hMsg.TypeID;
        *Flags = hMsg.flags;
    }

    wls_mutex_unlock(&wls_get_lock1);

    return pMsg;
}

int WLS_WakeUp(void* h)
{
    if (!wls_us_ctx) {
        PLIB_ERR("Library was not opened\n");
        return -1;
    }
    if (wls_check_ctx(h))
        return -1;

    PLIB_DEBUG("WLS_WakeUp\n");

    return wls_process_wakeup(h);


    return 0;
}

int WLS_WakeUp1(void* h)
{
    if (!wls_us_ctx1) {
        PLIB_ERR("Library was not opened\n");
        return -1;
    }
    if (wls_check_ctx1(h))
        return -1;

    PLIB_DEBUG("WLS_WakeUp1\n");

    return wls_process_wakeup1(h);


    return 0;
}

int WLS_Wait(void* h)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;

    if (!wls_us_ctx || (wls_us_ctx != pWls_us)) {
        PLIB_ERR("Library was not opened\n");
        return -1;
    }

    return wls_process_wait(h);
}

int WLS_Wait1(void* h)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;

    if (!wls_us_ctx1 || (wls_us_ctx1 != pWls_us)) {
        PLIB_ERR("Library was not opened\n");
        return -1;
    }

    return wls_process_wait1(h);
}

unsigned long long WLS_WGet(void* h, unsigned int *MsgSize, unsigned short *MsgTypeID, unsigned short *Flags)
{
    uint64_t pRxMsg = WLS_Get(h, MsgSize, MsgTypeID, Flags);

    if (pRxMsg)
        return pRxMsg;

    WLS_Wait(h);
    return WLS_Get(h, MsgSize, MsgTypeID, Flags);
}

unsigned long long WLS_WGet1(void* h, unsigned int *MsgSize, unsigned short *MsgTypeID, unsigned short *Flags)
{
    uint64_t pRxMsg = WLS_Get1(h, MsgSize, MsgTypeID, Flags);

    if (pRxMsg)
        return pRxMsg;

    WLS_Wait1(h);
    return WLS_Get1(h, MsgSize, MsgTypeID, Flags);
}

unsigned long long WLS_VA2PA(void* h, PVOID pMsg)
{
    uint64_t ret = 0;
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;

    unsigned long alloc_base;
    hugepage_tabl_t* pHugePageTlb;
    uint64_t hugePageBase;
    uint64_t hugePageOffet;
    unsigned int count = 0;

    uint64_t HugePageMask = ((unsigned long) pWls_us->HugePageSize - 1);
    if (pWls_us->alloc_buffer == NULL) {
        PLIB_ERR("WLS_VA2PA: nothing was allocated [%ld]\n", ret);
        return (uint64_t) ret;
    }

    alloc_base = (uint64_t) pWls_us->alloc_buffer & ~HugePageMask;

    pHugePageTlb = &pWls_us->hugepageTbl[0];

    hugePageBase = (uint64_t) pMsg & ~HugePageMask;
    hugePageOffet = (uint64_t) pMsg & HugePageMask;

    count = (hugePageBase - alloc_base) / pWls_us->HugePageSize;
    PLIB_DEBUG("WLS_VA2PA %lx base %llx off %llx  count %u\n", (unsigned long) pMsg,
            (uint64_t) hugePageBase, (uint64_t) hugePageOffet, count);

    if (count < MAX_N_HUGE_PAGES)
    {

    ret = pHugePageTlb[count].pagePa + hugePageOffet;
    }
    else
    {
        PLIB_ERR("WLS_VA2PA: Out of range [%p]\n", pMsg);
        return 0;
    }

    //printf("       WLS_VA2PA: %p -> %p   HugePageSize[%d] HugePageMask[%p] count[%d] pagePa[%p] hugePageBase[%p] alloc_buffer[%p] hugePageOffet[%lld]\n",
    //    pMsg, (void*)ret, pWls_us->HugePageSize, (void*)HugePageMask, count, (void*)pHugePageTlb[count].pagePa, (void*)hugePageBase, pWls_us->alloc_buffer, hugePageOffet);

    return (uint64_t) ret;
}

void* WLS_PA2VA(void* h, unsigned long long pMsg)
{
    unsigned long ret = 0;
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;

    hugepage_tabl_t* pHugePageTlb;
    uint64_t hugePageBase;
    uint64_t hugePageOffet;
    unsigned int nHugePage;
    int i;
    uint64_t HugePageMask = ((uint64_t) pWls_us->HugePageSize - 1);

    if (pWls_us->alloc_buffer == NULL) {
        PLIB_ERR("WLS_PA2VA: nothing was allocated [%ld]\n", ret);
        return (void*) ret;
    }

    pHugePageTlb = &pWls_us->hugepageTbl[0];

    hugePageBase = (uint64_t) pMsg & ~HugePageMask;
    hugePageOffet = (uint64_t) pMsg & HugePageMask;

    nHugePage = pWls_us->nHugePage;

    PLIB_DEBUG("WLS_PA2VA %llx base %llx off %llx  nHugePage %d\n", (uint64_t) pMsg,
            (uint64_t) hugePageBase, (uint64_t) hugePageOffet, nHugePage);

    for (i = 0; i < nHugePage; i++) {
        if (pHugePageTlb[i].pagePa == hugePageBase) {
            ret = (unsigned long) pHugePageTlb[i].pageVa;
            ret += hugePageOffet;
            return (void*) ret;
        }
    }

    //printf("       WLS_VA2PA: %p -> %p   HugePageSize[%d] HugePageMask[%p] nHugePage[%d] pagePa[%p] hugePageBase[%p] alloc_buffer[%p] hugePageOffet[%lld]\n",
    //    (void*)pMsg, (void*)ret, pWls_us->HugePageSize, (void*)HugePageMask, nHugePage, (void*)pHugePageTlb[nHugePage].pagePa, (void*)hugePageBase, pWls_us->alloc_buffer, hugePageOffet);
    PLIB_ERR("WLS_PA2VA: Out of range [%p]\n", (void*)pMsg);

    return (void*) (ret);
}

int WLS_EnqueueBlock(void* h, unsigned long long pMsg)
{
    int ret = 0;
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;

    if (!wls_us_ctx) {
        PLIB_ERR("Library was not opened\n");
        return -1;
    }


    if (pMsg == 0) {
        PLIB_ERR("WLS_EnqueueBlock: Null\n");
        return -1;
    }

    if (pWls_us->dst_user_va) {
        wls_us_ctx_t* pDstWls_us = (wls_us_ctx_t*) pWls_us->dst_user_va;
        ret = SFL_WlsEnqueue(&pDstWls_us->ul_free_block_pq, pMsg, NULL, pWls_us);
        if (ret == 1) {
            unsigned long* ptr = (unsigned long*) WLS_PA2VA(pWls_us, pMsg);
            if (ptr) {
                *ptr = 0xFFFFFFFFFFFFFFFF;
            }
        }
    } else
        ret = -1;

    PLIB_DEBUG("SFL_WlsEnqueue %d\n", ret);
    return ret;
}

int WLS_EnqueueBlock1(void* h, unsigned long long pMsg)
{
    int ret = 0;
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;

    if (!wls_us_ctx1) {
        PLIB_ERR("Library was not opened for a second context\n");
        return -1;
    }

    if (pWls_us->mode == WLS_SLAVE_CLIENT) {
        PLIB_ERR("Slave doesn't support memory allocation\n");
        return -1;
    }

    if (pMsg == 0) {
        PLIB_ERR("WLS_EnqueueBlock: Null\n");
        return -1;
    }

    if (pWls_us->dst_user_va) {
        wls_us_ctx_t* pDstWls_us = (wls_us_ctx_t*) pWls_us->dst_user_va;
        ret = SFL_WlsEnqueue(&pDstWls_us->ul_free_block_pq, pMsg, NULL, pWls_us);
        if (ret == 1) {
            unsigned long* ptr = (unsigned long*) WLS_PA2VA(pWls_us, pMsg);
            if (ptr) {
                *ptr = 0xFFFFFFFFFFFFFFFF;
            }
        }
    } else
        ret = -1;

    PLIB_DEBUG("SFL_WlsEnqueue %d\n", ret);
    return ret;
}


unsigned long long WLS_DequeueBlock(void* h)
{
    unsigned long long retval = 0;
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;

    if ((pWls_us->mode == WLS_SLAVE_CLIENT)&&(pWls_us->secmode==WLS_SEC_NA))
    {
        // local
        return SFL_WlsDequeue(&pWls_us->ul_free_block_pq, NULL, h);
    }    
    if (!pWls_us->dst_user_va)
    {
        return retval;
    }
        // remote
    wls_us_ctx_t* pDstWls_us = (wls_us_ctx_t*) pWls_us->dst_user_va;
    retval = SFL_WlsDequeue(&pDstWls_us->ul_free_block_pq, NULL, pDstWls_us);
    if (retval) {
        unsigned long* ptr = (unsigned long*) WLS_PA2VA(pWls_us, retval);
        if (ptr) {
            if (*ptr != 0xFFFFFFFFFFFFFFFF) {
                PLIB_ERR("WLS_EnqueueBlock: incorrect content pa: 0x%016lx: 0x%016lx\n",
                         (unsigned long) retval, *ptr);
            }
        }
    }

    return retval;
}

int WLS_NumBlocks(void* h)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*) h;
    int n = 0;

    if (pWls_us->mode == WLS_SLAVE_CLIENT) {
        // local
        n = SFL_GetNumItemsInTheQueue(&pWls_us->ul_free_block_pq);
    } else if (pWls_us->dst_user_va) {
        // remote
        wls_us_ctx_t* pDstWls_us = (wls_us_ctx_t*) pWls_us->dst_user_va;
        n = SFL_GetNumItemsInTheQueue(&pDstWls_us->ul_free_block_pq);
    }

    return n;
}
