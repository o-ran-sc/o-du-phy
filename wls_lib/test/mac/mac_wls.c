/******************************************************************************
*
*   Copyright (c) 2019 Intel.
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

/**
 * @brief This file has Shared Memory interface functions between MAC and PHY
 * @file testmac_wls.c
 * @ingroup group_testmac
 * @author Intel Corporation
 **/


#include <sys/ioctl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>

#include <rte_eal.h>
#include <rte_cfgfile.h>
#include <rte_string_fns.h>
#include <rte_common.h>
#include <rte_string_fns.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_launch.h>

#define DPDK_WLS
#include "wls.h"
#include "wls_lib.h"
#include "mac_wls.h"
//#include "phy_printf.h"
//#include "aux_sys.h"
//#include "aux_timer.h"
//#include "mlog_lnx.h"

//#include "nr5g_testmac_config_test.h"
//#include "nr5g_testmac_mac_phy_api_proc.h"


#define MSG_MAXSIZE                         ( 16384 * 16 )


#define TO_FREE_SIZE                        ( 10 )
#define TOTAL_FREE_BLOCKS                   ( 50 * 12)
#define ALLOC_TRACK_SIZE                    ( 16384 )

//#define MEMORY_CORRUPTION_DETECT
#define MEMORY_CORRUPTION_DETECT_FLAG       (0xAB)

typedef struct wls_mac_mem_array
{
    void **ppFreeBlock;
    void *pStorage;
    void *pEndOfStorage;
    uint32_t nBlockSize;
    uint32_t nBlockCount;
} WLS_MAC_MEM_SRUCT, *PWLS_MAC_MEM_SRUCT;

typedef struct wls_mac_ctx
{
    void *hWls;
    void *pWlsMemBase;
    void *pWlsMemBaseUsable;
    WLS_MAC_MEM_SRUCT sWlsStruct;

    uint64_t nTotalMemorySize;
    uint64_t nTotalMemorySizeUsable;
    uint32_t nBlockSize;
    uint32_t nTotalBlocks;
    uint32_t nAllocBlocks;
    uint32_t nTotalAllocCnt;
    uint32_t nTotalFreeCnt;
    uint32_t nTotalUlBufAllocCnt;
    uint32_t nTotalUlBufFreeCnt;
    uint32_t nTotalDlBufAllocCnt;
    uint32_t nTotalDlBufFreeCnt;
//  Support for FAPI Translator
    uint32_t nPartitionMemSize;
    void     *pPartitionMemBase;

    volatile pthread_mutex_t lock;
    volatile pthread_mutex_t lock_alloc;
} WLS_MAC_CTX, *PWLS_MAC_CTX;

static pthread_t *pwls_testmac_thread = NULL;
static WLS_MAC_CTX wls_mac_iface;
static int gwls_mac_ready = 0;
static pid_t gwls_pid = 0;
static uint32_t gToFreeListCnt[TO_FREE_SIZE] = {0};
static uint64_t gpToFreeList[TO_FREE_SIZE][TOTAL_FREE_BLOCKS] = {{0L}};
static uint8_t alloc_track[ALLOC_TRACK_SIZE];
static uint64_t gTotalTick = 0, gUsedTick = 0;

//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]   ptr Pointer to display
 *  @param[in]   size Size of data
 *
 *  @return  void
 *
 *  @description
 *  This function displays content of Buffer - Used for debugging
 *
**/
//-------------------------------------------------------------------------------------------
void wls_mac_show_data(void* ptr, uint32_t size)
{
    uint8_t *d = ptr;
    int i;

    for(i = 0; i < size; i++)
    {
        if ( !(i & 0xf) )
            printf("\n");
        printf("%02x ", d[i]);
    }
    printf("\n");
}



//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param void
 *
 *  @return  Pointer to WLS_MAC_CTX stucture
 *
 *  @description
 *  This function returns the WLS Local structure which has WLS related parameters
 *
**/
//-------------------------------------------------------------------------------------------
static PWLS_MAC_CTX wls_mac_get_ctx(void)
{
    return &wls_mac_iface;
}

void wls_mac_print_stats(void)
{
    PWLS_MAC_CTX pWls = wls_mac_get_ctx();

    printf("wls_mac_free_list_all:\n");
    printf("        nTotalBlocks[%d] nAllocBlocks[%d] nFreeBlocks[%d]\n", pWls->nTotalBlocks, pWls->nAllocBlocks, (pWls->nTotalBlocks- pWls->nAllocBlocks));
    printf("        nTotalAllocCnt[%d] nTotalFreeCnt[%d] Diff[%d]\n", pWls->nTotalAllocCnt, pWls->nTotalFreeCnt, (pWls->nTotalAllocCnt- pWls->nTotalFreeCnt));
    printf("        nDlBufAllocCnt[%d] nDlBufFreeCnt[%d] Diff[%d]\n", pWls->nTotalDlBufAllocCnt, pWls->nTotalDlBufFreeCnt, (pWls->nTotalDlBufAllocCnt- pWls->nTotalDlBufFreeCnt));
    printf("        nUlBufAllocCnt[%d] nUlBufFreeCnt[%d] Diff[%d]\n\n", pWls->nTotalUlBufAllocCnt, pWls->nTotalUlBufFreeCnt, (pWls->nTotalUlBufAllocCnt- pWls->nTotalUlBufFreeCnt));
}



//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]   ptr Address to convert
 *
 *  @return  Converted address
 *
 *  @description
 *  This function converts Virtual Address to Physical Address
 *
**/
//-------------------------------------------------------------------------------------------
uint64_t wls_mac_va_to_pa(void *ptr)
{
    PWLS_MAC_CTX pWls =  wls_mac_get_ctx();
    uint64_t ret = (uint64_t)WLS_VA2PA(pWls->hWls, ptr);

    //printf("wls_mac_va_to_pa: %p ->%p\n", ptr, (void*)ret);

    return ret;
}



//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]   ptr Address to convert
 *
 *  @return  Converted address
 *
 *  @description
 *  This function converts Physical Address to Virtual Address
 *
**/
//-------------------------------------------------------------------------------------------
void *wls_mac_pa_to_va(uint64_t ptr)
{
    PWLS_MAC_CTX pWls = wls_mac_get_ctx();
    void *ret = WLS_PA2VA(pWls->hWls, ptr);

    //printf("wls_mac_pa_to_va: %p -> %p\n", (void*)ptr, ret);

    return ret;
}



//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]   pMemArray Pointer to WLS Memory management structure
 *  @param[in]   pMemArrayMemory Pointer to flat buffer that was allocated
 *  @param[in]   totalSize Total Size of flat buffer allocated
 *  @param[in]   nBlockSize Size of each block that needs to be partitioned by memory manager
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function creates memory blocks from a flat buffer which will be used for communciation
 *  between MAC and PHY
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t wls_mac_create_mem_array(PWLS_MAC_MEM_SRUCT pMemArray,
                              void *pMemArrayMemory,
                              uint32_t totalSize, uint32_t nBlockSize)
{
    int numBlocks = totalSize / nBlockSize;
    void **ptr;
    uint32_t i;

    printf("wls_mac_create_mem_array: pMemArray[%p] pMemArrayMemory[%p] totalSize[%d] nBlockSize[%d] numBlocks[%d]\n",
        pMemArray, pMemArrayMemory, totalSize, nBlockSize, numBlocks);

    // Can't be less than pointer size
    if (nBlockSize < sizeof(void *))
    {
        return FAILURE;
    }

    // Can't be less than one block
    if (totalSize < sizeof(void *))
    {
        return FAILURE;
    }

    pMemArray->ppFreeBlock = (void **)pMemArrayMemory;
    pMemArray->pStorage = pMemArrayMemory;
    pMemArray->pEndOfStorage = ((unsigned long*)pMemArrayMemory) + numBlocks * nBlockSize / sizeof(unsigned long);
    pMemArray->nBlockSize = nBlockSize;
    pMemArray->nBlockCount = numBlocks;

    // Initialize single-linked list of free blocks;
    ptr = (void **)pMemArrayMemory;
    for (i = 0; i < pMemArray->nBlockCount; i++)
    {
#ifdef MEMORY_CORRUPTION_DETECT
        // Fill with some pattern
        uint8_t *p = (uint8_t *)ptr;
        uint32_t j;

        p += (nBlockSize - 16);
        for (j = 0; j < 16; j++)
        {
            p[j] = MEMORY_CORRUPTION_DETECT_FLAG;
        }
#endif

        if (i == pMemArray->nBlockCount - 1)
        {
            *ptr = NULL;      // End of list
        }
        else
        {
            // Points to the next block
            *ptr = (void **)(((uint8_t*)ptr) + nBlockSize);
            ptr += nBlockSize / sizeof(unsigned long);
        }
    }

    memset(alloc_track, 0, sizeof(uint8_t) * ALLOC_TRACK_SIZE);

    return SUCCESS;
}




//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]    pMemArray Pointer to WLS Memory management structure
 *  @param[out]   ppBlock Pointer where allocated memory block is stored
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function allocated a memory block from pool
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t wls_mac_alloc_mem_array(PWLS_MAC_MEM_SRUCT pMemArray, void **ppBlock)
{
    int idx;

    if (pMemArray->ppFreeBlock == NULL)
    {
        printf("wls_mac_alloc_mem_array pMemArray->ppFreeBlock = NULL\n");
        return FAILURE;
    }

    // FIXME: Remove after debugging
    if (((void *) pMemArray->ppFreeBlock < pMemArray->pStorage) ||
        ((void *) pMemArray->ppFreeBlock >= pMemArray->pEndOfStorage))
    {
        printf("wls_mac_alloc_mem_array ERROR: Corrupted MemArray;Arr=%p,Stor=%p,Free=%p\n",
                pMemArray, pMemArray->pStorage, pMemArray->ppFreeBlock);
        return FAILURE;
    }

    pMemArray->ppFreeBlock = (void **)((unsigned long)pMemArray->ppFreeBlock & 0xFFFFFFFFFFFFFFF0);
    *pMemArray->ppFreeBlock = (void **)((unsigned long)*pMemArray->ppFreeBlock & 0xFFFFFFFFFFFFFFF0);

    if ((*pMemArray->ppFreeBlock != NULL) &&
        (((*pMemArray->ppFreeBlock) < pMemArray->pStorage) ||
        ((*pMemArray->ppFreeBlock) >= pMemArray->pEndOfStorage)))
    {
        fprintf(stderr, "ERROR: Corrupted MemArray;Arr=%p,Stor=%p,Free=%p,Curr=%p\n",
                pMemArray, pMemArray->pStorage, pMemArray->ppFreeBlock,
                *pMemArray->ppFreeBlock);
        return FAILURE;
    }

    *ppBlock = (void *) pMemArray->ppFreeBlock;
    pMemArray->ppFreeBlock = (void **) (*pMemArray->ppFreeBlock);

    idx = (((uint64_t)*ppBlock - (uint64_t)pMemArray->pStorage)) / pMemArray->nBlockSize;
    if (alloc_track[idx])
    {
        printf("wls_mac_alloc_mem_array Double alloc Arr=%p,Stor=%p,Free=%p,Curr=%p\n",
            pMemArray, pMemArray->pStorage, pMemArray->ppFreeBlock,
            *pMemArray->ppFreeBlock);
    }
    else
    {
#ifdef MEMORY_CORRUPTION_DETECT
        uint32_t nBlockSize = pMemArray->nBlockSize, i;
        uint8_t *p = (uint8_t *)*ppBlock;

        p += (nBlockSize - 16);
        for (i = 0; i < 16; i++)
        {
            p[i] = MEMORY_CORRUPTION_DETECT_FLAG;
        }
#endif
        alloc_track[idx] = 1;
    }

    //printf("Block allocd [%p,%p]\n", pMemArray, *ppBlock);

    return SUCCESS;
}




//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]   pMemArray Pointer to WLS Memory management structure
 *  @param[in]   pBlock Pointer to block that needs to be added back to pool
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function frees a WLS block of memory and adds it back to the pool
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t wls_mac_free_mem_array(PWLS_MAC_MEM_SRUCT pMemArray, void *pBlock)
{
    int idx;
    unsigned long mask = (((unsigned long)pMemArray->nBlockSize) - 1);

    pBlock = (void *)((unsigned long)pBlock & ~mask);

    if ((pBlock < pMemArray->pStorage) || (pBlock >= pMemArray->pEndOfStorage))
    {
        printf("wls_mac_free_mem_array WARNING: Trying to free foreign block;Arr=%p,Blk=%p pStorage [%p .. %p]\n",
               pMemArray, pBlock, pMemArray->pStorage, pMemArray->pEndOfStorage);

        return FAILURE;
    }

    idx = (int)(((uint64_t)pBlock - (uint64_t)pMemArray->pStorage)) / pMemArray->nBlockSize;

    if (alloc_track[idx] == 0)
    {
        printf("wls_mac_free_mem_array ERROR: Double free Arr=%p,Stor=%p,Free=%p,Curr=%p\n",
            pMemArray, pMemArray->pStorage, pMemArray->ppFreeBlock,
            pBlock);

        return SUCCESS;

    }
    else
    {
#ifdef MEMORY_CORRUPTION_DETECT
        uint32_t nBlockSize = pMemArray->nBlockSize, i;
        uint8_t *p = (uint8_t *)pBlock;

        p += (nBlockSize - 16);
        for (i = 0; i < 16; i++)
        {
            if (p[i] != MEMORY_CORRUPTION_DETECT_FLAG)
            {
                printf("ERROR: Corruption\n");
                wls_mac_print_stats();
                exit(-1);
            }
        }
#endif
        alloc_track[idx] = 0;
    }

    if (((void *) pMemArray->ppFreeBlock) == pBlock)
    {
        // Simple protection against freeing of already freed block
        return SUCCESS;
    }

    // FIXME: Remove after debugging
    if ((pMemArray->ppFreeBlock != NULL)
        && (((void *) pMemArray->ppFreeBlock < pMemArray->pStorage)
        || ((void *) pMemArray->ppFreeBlock >= pMemArray->pEndOfStorage)))
    {
        printf("wls_mac_free_mem_array ERROR: Corrupted MemArray;Arr=%p,Stor=%p,Free=%p\n",
                pMemArray, pMemArray->pStorage, pMemArray->ppFreeBlock);
        return FAILURE;
    }

    // FIXME: Remove after debugging
    if ((pBlock < pMemArray->pStorage) ||
        (pBlock >= pMemArray->pEndOfStorage))
    {
        printf("wls_mac_free_mem_array ERROR: Invalid block;Arr=%p,Blk=%p\n",
                pMemArray, pBlock);
        return FAILURE;
    }

    *((void **)pBlock) = (void **)((unsigned long)pMemArray->ppFreeBlock & 0xFFFFFFFFFFFFFFF0);
    pMemArray->ppFreeBlock = (void **) ((unsigned long)pBlock & 0xFFFFFFFFFFFFFFF0);

    //printf("Block freed [%p,%p]\n", pMemArray, pBlock);

    return SUCCESS;
}


//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param   void
 *
 *  @return  Pointer to the memory block
 *
 *  @description
 *  This function allocates a block of memory from the pool
 *
**/
//-------------------------------------------------------------------------------------------
void *wls_mac_alloc_buffer(uint32_t size, uint32_t loc)
{
    void *pBlock = NULL;
    PWLS_MAC_CTX pWls =  wls_mac_get_ctx();
    PWLS_MAC_MEM_SRUCT pMemArray = &pWls->sWlsStruct;

    pthread_mutex_lock((pthread_mutex_t *)&pWls->lock_alloc);

    if (wls_mac_alloc_mem_array(&pWls->sWlsStruct, &pBlock) != SUCCESS)
    {
        printf("wls_mac_alloc_buffer alloc error size[%d] loc[%d]\n", size, loc);
        wls_mac_print_stats();
        exit(-1);
    }
    else
    {
        pWls->nAllocBlocks++;
    }

    //printf("----------------wls_mac_alloc_buffer: size[%d] loc[%d] buf[%p] nAllocBlocks[%d]\n", size, loc, pBlock, pWls->nAllocBlocks);

    //printf("[%p]\n", pBlock);

    pWls->nTotalAllocCnt++;
    if (loc < MAX_DL_BUF_LOCATIONS)
        pWls->nTotalDlBufAllocCnt++;
    else if (loc < MAX_UL_BUF_LOCATIONS)
        pWls->nTotalUlBufAllocCnt++;

    pthread_mutex_unlock((pthread_mutex_t *)&pWls->lock_alloc);

    return pBlock;
}



//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]   *pMsg Pointer to free
 *
 *  @return  void
 *
 *  @description
 *  This function frees a block of memory and adds it back to the pool
 *
**/
//-------------------------------------------------------------------------------------------
void wls_mac_free_buffer(void *pMsg, uint32_t loc)
{
    PWLS_MAC_CTX pWls =  wls_mac_get_ctx();
    PWLS_MAC_MEM_SRUCT pMemArray = &pWls->sWlsStruct;

    pthread_mutex_lock((pthread_mutex_t *)&pWls->lock_alloc);

    //printf("----------------wls_mac_free_buffer: buf[%p] loc[%d]\n", pMsg, loc);
    if (wls_mac_free_mem_array(&pWls->sWlsStruct, (void *)pMsg) == SUCCESS)
    {
        pWls->nAllocBlocks--;
    }
    else
    {
        printf("wls_mac_free_buffer Free error\n");
        wls_mac_print_stats();
        exit(-1);
    }

    pWls->nTotalFreeCnt++;
    if (loc < MAX_DL_BUF_LOCATIONS)
        pWls->nTotalDlBufFreeCnt++;
    else if (loc < MAX_UL_BUF_LOCATIONS)
        pWls->nTotalUlBufFreeCnt++;

    pthread_mutex_unlock((pthread_mutex_t *)&pWls->lock_alloc);
}



//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param   void
 *
 *  @return  Number of free blocks
 *
 *  @description
 *  This function queries the number of free blocks in the system
 *
**/
//-------------------------------------------------------------------------------------------
int wls_mac_num_free_blocks(void)
{
    PWLS_MAC_CTX pWls = wls_mac_get_ctx();

    return (pWls->nTotalBlocks- pWls->nAllocBlocks);
}



void wls_mac_free_list_all(void)
{
    PWLS_MAC_CTX pWls = wls_mac_get_ctx();
    uint32_t idx;

    for (idx = 0; idx < TO_FREE_SIZE; idx++)
    {
        wls_mac_free_list(idx);
    }

    wls_mac_print_stats();
}


//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]   pWls Pointer to the WLS_MAC_CTX structure
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function created a partition and blocks of WLS memory for API exchange between MAC and PHY
 *
**/
//-------------------------------------------------------------------------------------------
int wls_mac_create_partition(PWLS_MAC_CTX pWls)
{
    memset(pWls->pWlsMemBase, 0xCC, pWls->nTotalMemorySize);
    pWls->pPartitionMemBase = pWls->pWlsMemBase;
    pWls->nPartitionMemSize = pWls->nTotalMemorySize/2;
    pWls->nTotalBlocks = pWls->nTotalMemorySize / MSG_MAXSIZE;
    return wls_mac_create_mem_array(&pWls->sWlsStruct, pWls->pPartitionMemBase, pWls->nPartitionMemSize, MSG_MAXSIZE);
}



static volatile int gWlsMacPrintThreadInfo = 0;

void wls_mac_print_thread_info(void)
{
    gWlsMacPrintThreadInfo = 1;

    return;
}

void wls_mac_get_time_stats(uint64_t *pTotal, uint64_t *pUsed, uint32_t nClear)
{
    *pTotal = gTotalTick;
    *pUsed = gUsedTick;

    if (nClear)
    {
        gTotalTick = 0;
        gUsedTick = 0;
    }
}

//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]   pMsgHeader Pointer to TxSdu Message Block
 *  @param[in]   count Location in Free List Array
 *  @param[in]   pToFreeList Array where all the blocks to free are stored
 *
 *  @return  New Location in free list array
 *
 *  @description
 *  This function adds all the messages in a subframe coming from L1 to L2 to a free array to be
 *  freed back to the queue at a later point in time.
 *
**/
//-------------------------------------------------------------------------------------------
int wls_mac_sdu_zbc_block_add_to_free(void* pMsgHeaderHead, int count, uint64_t *pToFreeList)
{
    fapi_msg_t *p_fapi_msg = (fapi_msg_t *) pMsgHeaderHead;

    if (p_fapi_msg->msg_id == FAPI_TX_DATA_REQUEST)
    {
        fapi_tx_data_req_t *p_tx_data_req =  (fapi_tx_data_req_t *) p_fapi_msg;
        p_fapi_api_queue_elem_t p_list_elm = ((p_fapi_api_queue_elem_t) p_tx_data_req) - 1;
        p_fapi_api_queue_elem_t p_sdu_elm = p_list_elm->p_tx_data_elm_list;
        while(p_sdu_elm)
        {
            if (count < TOTAL_FREE_BLOCKS)
            {
                pToFreeList[count++] = (uint64_t) p_sdu_elm;
            }
            else
            {
                printf("wls_mac_sdu_zbc_block_add_to_free: ERROR: Reached max Number of Free Blocks\n");
                return count;
            }
            p_sdu_elm = p_sdu_elm->p_next;
        }
    }

    return count;
}


//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]   pListElem Pointer to List element header
 *  @param[in]   idx Subframe Number
 *
 *  @return  Number of blocks freed
 *
 *  @description
 *  This function Frees all the blocks in a List Element Linked List coming from L1 by storing
 *  them into an array to be freed at a later point in time.
 *
**/
//-------------------------------------------------------------------------------------------
int wls_mac_add_to_free(p_fapi_api_queue_elem_t pListElem, uint32_t idx)
{
    p_fapi_api_queue_elem_t pNextMsg = NULL;
    void* pMsgHeader;
    int count = gToFreeListCnt[idx], nZbcBlocks;

    pNextMsg = pListElem;

    while (pNextMsg)
    {
        if (count < TOTAL_FREE_BLOCKS)
        {
            gpToFreeList[idx][count] = (uint64_t)pNextMsg;
        }
        else
        {
            printf("wls_mac_add_to_free: ERROR: Reached max Number of Free Blocks\n");
            return count;
        }

        if (pNextMsg->msg_type != FAPI_VENDOR_MSG_HEADER_IND)
        {
            pMsgHeader = (void *) (pNextMsg + 1);
            count++;
            count = wls_mac_sdu_zbc_block_add_to_free(pMsgHeader, count, gpToFreeList[idx]);
        }

        if (pNextMsg->p_next)
        {
            pNextMsg = (p_fapi_api_queue_elem_t)(pNextMsg->p_next);
        }
        else
        {
            pNextMsg = 0;
        }
    }

    gpToFreeList[idx][count] = 0L;
    gToFreeListCnt[idx] = count;

    printf("To Free %d\n", count);

    return count;
}


//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]   idx subframe Number
 *
 *  @return  Number of blocks freed
 *
 *  @description
 *  This function frees all blocks that have been added to the free array
 *
**/
//-------------------------------------------------------------------------------------------
int wls_mac_free_list(uint32_t idx)
{
    p_fapi_api_queue_elem_t pNextMsg = NULL;
    int count = 0;

    if(idx >= TO_FREE_SIZE){
        printf("Error idx %d\n", idx);
        return 0;
    }

    pNextMsg = (p_fapi_api_queue_elem_t)gpToFreeList[idx][count];

    while (pNextMsg)
    {
        wls_mac_free_buffer(pNextMsg, MIN_DL_BUF_LOCATIONS+0);
        gpToFreeList[idx][count] = (uint64_t) NULL;
        count++;
        if (gpToFreeList[idx][count])
            pNextMsg = (p_fapi_api_queue_elem_t)gpToFreeList[idx][count];
        else
            pNextMsg = 0;
    }

    printf("Free %d\n", count);
    gToFreeListCnt[idx] = 0;

    return count;
}


//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param void
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function is called at WLS init and waits in an infinite for L1 to respond back with some information
 *  needed by the L2
 *
**/
//-------------------------------------------------------------------------------------------
int wls_mac_ready(void)
{
    int ret = 0;
    PWLS_MAC_CTX pWls =  wls_mac_get_ctx();
    ret = WLS_Ready(pWls->hWls);

    return ret;
}

//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param   void
 *
 *  @return  Number of blocks of APIs received
 *
 *  @description
 *  This functions waits in a infinite loop for L1 to send a list of APIs to MAC. This is called
 *  during runtime when L2 sends a API to L1 and then waits for response back.
 *
**/
//-------------------------------------------------------------------------------------------
int wls_mac_wait(void)
{
    int ret = 0;
    PWLS_MAC_CTX pWls =  wls_mac_get_ctx();

    ret = WLS_Wait(pWls->hWls);

    return ret;
}

//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[out]   data Location where First API from L1 is stored
 *
 *  @return  Size of Message sent from L1
 *
 *  @description
 *  This function queries the APIs sent from L1 to L2 and gets the first pointer to the linked list
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t wls_mac_recv(uint64_t *data, uint16_t *nFlags)
{
    PWLS_MAC_CTX pWls =  wls_mac_get_ctx();
    uint32_t msgSize = 0;
    uint16_t msgType = 0;

    *data = WLS_Get(pWls->hWls, &msgSize, &msgType, nFlags);

    return msgSize;
}


//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]   pMsg Pointer to API block that needs to be sent to L1
 *  @param[in]   MsgSize Size of Message
 *  @param[in]   MsgTypeID Message Id
 *  @param[in]   Flags Special Flags needed for WLS
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function adds a block of API from L2 to L1 which will be sent later
 *
**/
//-------------------------------------------------------------------------------------------
int wls_mac_put(uint64_t pMsg, uint32_t MsgSize, uint16_t MsgTypeID, uint16_t Flags)
{
    int ret = 0;
    PWLS_MAC_CTX pWls =  wls_mac_get_ctx();

    //printf("wls_mac_put: %p size: %d type: %d nFlags: %d\n", (void*)pMsg, MsgSize, MsgTypeID, Flags);
    //  wls_mac_show_data((void*)wls_alloc_buffer(pMsg), MsgSize);
    ret = WLS_Put(pWls->hWls, pMsg, MsgSize, MsgTypeID, Flags);

    return ret;
}



//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]   pMsgHeader Pointer to the TxSduReq Message block
 *  @param[in]   nFlags Special nFlags needed for WLS
 *  @param[in]   nZbcBlocks Number of ZBC blocks in list
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function adds all the ZBC blocks in a TXSDU Message and prepares them to be sent to the L1
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t wls_mac_send_zbc_blocks(void *pMsgHeaderHead, uint16_t nFlags, int *nZbcBlocks, uint32_t nFlagsUrllc)
{
    fapi_tx_data_req_t *p_tx_data_req = (fapi_tx_data_req_t *) pMsgHeaderHead;
    p_fapi_api_queue_elem_t p_list_elm = ((p_fapi_api_queue_elem_t) p_tx_data_req) - 1;
    p_list_elm = p_list_elm->p_tx_data_elm_list;

    int ret = 0;
    uint8_t nMsgType;
    uint32_t isLast, nPduLen;
    uint16_t list_flags = nFlags;
    void *pPayload = NULL;

    printf("wls_mac_put ZBC blocks: %d\n", nFlags);

    while (p_list_elm)
    {
        nPduLen = p_list_elm->msg_len + sizeof(fapi_api_queue_elem_t);
        pPayload = (void *) p_list_elm;
        nMsgType = FAPI_VENDOR_MSG_PHY_ZBC_BLOCK_REQ;

        if (p_list_elm->p_next)
            isLast = 0;
        else
            isLast = 1;

        if ((list_flags & WLS_TF_FIN) && isLast)
            nFlags = WLS_SG_LAST; // TXSDU.req was last block in the list hence ZBC block is last
        else
            nFlags = WLS_SG_NEXT; // more blocks in the list

        printf("wls_mac_put 0x%016lx  msg type: %x nFlags %x\n", (uint64_t) pPayload, nMsgType, nFlags);
        ret = wls_mac_put((uint64_t) pPayload, nPduLen, nMsgType, nFlags);
        if (ret != 0)
        {
            printf("Error ZBC block 0x%016lx\n", (uint64_t) pPayload);
            return FAILURE;
        }
        p_list_elm = p_list_elm->p_next;
    }

    return SUCCESS;
}



//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]    pMsgHeader Pointer to the TxSDuReq Message block
 *  @param[out]   nZbcBlocks Number of ZBC blocks
 *
 *  @return  1 if this block is a TxSduReq message. 0 else.
 *
 *  @description
 *  This function checks if a block is a TxSduReq messages and counts the number of ZBC blocks in this
 *  API
 *
**/
//-------------------------------------------------------------------------------------------
int wls_mac_is_sdu_zbc_block(void* pMsgHeaderHead, int *nZbcBlocks)
{
    fapi_tx_data_req_t *p_tx_data_req = (fapi_tx_data_req_t *) pMsgHeaderHead;
    p_fapi_api_queue_elem_t p_list_elm = 
        ((p_fapi_api_queue_elem_t) p_tx_data_req) - 1;
    *nZbcBlocks = 0;

    if (p_tx_data_req->header.msg_id == FAPI_TX_DATA_REQUEST &&
            p_list_elm->p_tx_data_elm_list)
    {
        return 1;
    }

    return 0;
}



//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]   data Pointer to the Linked list header
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function sends a list of APIs to the L1
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t wls_mac_send_msg_to_phy(void *data)
{
    uint32_t  ret = SUCCESS;
    PWLS_MAC_CTX pWls =  wls_mac_get_ctx();
    PWLS_MAC_MEM_SRUCT pMemArray = &pWls->sWlsStruct;
    p_fapi_api_queue_elem_t pCurrMsg = NULL;
    p_fapi_api_queue_elem_t pListElem = NULL;
    static uint32_t idx = 0;

    fapi_msg_t *pMsgHeader;
    uint16_t nFlags;
    int nZbcBlocks = 0, isZbc = 0, count = 0;

    printf("wls_mac_send_msg_to_phy\n");
    printf("data (0x%lX) sending to phy...\n", (unsigned long)data);

    pthread_mutex_lock((pthread_mutex_t *)&pWls->lock);

    if (gwls_mac_ready)
    {
        pListElem = (p_fapi_api_queue_elem_t)data;
        wls_mac_add_to_free(pListElem, idx);
        count++;


        ret = wls_mac_put(wls_mac_va_to_pa(pListElem),
                pListElem->msg_len + sizeof(fapi_api_queue_elem_t),
                pMsgHeader->msg_id, nFlags);
        if (ret != 0)
        {
            printf("Error\n");
            pthread_mutex_unlock((pthread_mutex_t *)&pWls->lock);
            return FAILURE;
        }
    }

    pthread_mutex_unlock((pthread_mutex_t *)&pWls->lock);
    return ret;
}


//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param   void
 *
 *  @return  Number of blocks added
 *
 *  @description
 *  This function add WLS blocks to the L1 Array which will be used by L1 in every TTI to
 *  populate and send back APIs to the MAC
 *
**/
//-------------------------------------------------------------------------------------------
int wls_mac_add_blocks_to_ul(void)
{
    int ret = 0;
    PWLS_MAC_CTX pWls =  wls_mac_get_ctx();

    void *pMsg = wls_mac_alloc_buffer(0, MIN_UL_BUF_LOCATIONS+0);

    if(pMsg)
    {
        /* allocate blocks for UL transmittion */
        while(WLS_EnqueueBlock(pWls->hWls,(uint64_t)wls_mac_va_to_pa(pMsg)))
        {
            ret++;
            pMsg = wls_mac_alloc_buffer(0, MIN_UL_BUF_LOCATIONS+1);
            if(WLS_EnqueueBlock(pWls->hWls,(uint64_t)wls_mac_va_to_pa(pMsg)))
            {
                ret++;
                pMsg = wls_mac_alloc_buffer(0, MIN_UL_BUF_LOCATIONS+1);
            }

            if(!pMsg)
                break;
        }

        // free not enqueued block
        if(pMsg)
        {
            wls_mac_free_buffer(pMsg, MIN_UL_BUF_LOCATIONS+3);
        }
    }

    return ret;
}


//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]   data Thread Local Context Structure Pointer
 *
 *  @return  NULL
 *
 *  @description
 *  This is the WLS Receiver thread that is created at Testmac Init and is responsible for receiving
 *  APIs from L1 to MAC
 *
**/
//-------------------------------------------------------------------------------------------
void *wls_mac_rx_task()
{
    void*    buffer_va = 0;
    uint64_t      buffer_pa = 0;
    uint32_t get,i, rc = 0;

    uint32_t size  = 0;
    uint64_t tWake = 0, tWakePrev = 0, tSleep = 0;
    uint16_t nFlags;
    p_fapi_api_queue_elem_t pElm   = NULL;
    p_fapi_api_queue_elem_t pFirst = NULL;
    p_fapi_api_queue_elem_t pPrev  = NULL;


    usleep(1000);

    wls_mac_ready();

    while (1)
    {
        get = wls_mac_wait();

        if (get == 0)
        {
            continue;
        }
        printf("Got %d messages from FAPI Translator\n", get);
        while(get--)
        {
            size =  wls_mac_recv((uint64_t *)&buffer_pa, &nFlags);
            buffer_va =  wls_mac_pa_to_va(buffer_pa);
            pElm = (p_fapi_api_queue_elem_t) buffer_va;


            if (pFirst == NULL)
                pFirst = pElm;

            if (nFlags != WLS_TF_FIN)
            {
                wls_mac_print_recv_list((p_fapi_api_queue_elem_t) pElm, i);
                i++;
						}
            if(pPrev)
                pPrev->p_next = pElm;

            pPrev = pElm;

            if ((nFlags & WLS_TF_FIN))
            {
                // send to MAC
                if (pPrev)
                {
                    pPrev->p_next =  NULL;
                }

                wls_mac_print_recv_list((p_fapi_api_queue_elem_t) pFirst, i);

                pFirst= NULL;
                pPrev = NULL;
                return NULL;
            }
            else
            {
            }
        }
        wls_mac_add_blocks_to_ul();

    }

    return NULL;
}

void wls_mac_print_recv_list(p_fapi_api_queue_elem_t list, uint32_t i)
{
    printf("\nMAC received response %d from FAPI\n",i);
}

p_fapi_api_queue_elem_t wls_mac_create_elem(uint16_t num_msg, uint32_t align_offset, uint32_t msg_type, uint32_t n_loc)
{
    p_fapi_api_queue_elem_t p_list_elem;

    p_list_elem = (p_fapi_api_queue_elem_t)wls_mac_alloc_buffer(num_msg * align_offset + sizeof(fapi_api_queue_elem_t), n_loc);

    //Fill header for link list of API messages
    if (p_list_elem)
    {
        p_list_elem->msg_type = (uint8_t)msg_type;
        p_list_elem->num_message_in_block = 1;
        p_list_elem->align_offset = (uint16_t)align_offset;
        p_list_elem->msg_len = num_msg * align_offset;
        p_list_elem->p_next = NULL;
    }

    return p_list_elem;
}

//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param   void
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function initialized the WLS threads for the Testmac and allocates memory needed to
 *  exchange APIs between MAC and PHY
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t wls_mac_init(char * wls_device_name, uint64_t nBlockSize)
{
    uint64_t nWlsMacMemSize;
    uint64_t nWlsPhyMemSize;
    uint32_t ret = FAILURE;
    PWLS_MAC_CTX pWls =  wls_mac_get_ctx();
    uint8_t *pMemZone;
    static const struct rte_memzone *mng_memzone;
    wls_drv_ctx_t *pDrv_ctx;

    sleep(1);

    pthread_mutex_init((pthread_mutex_t *)&pWls->lock, NULL);
    pthread_mutex_init((pthread_mutex_t *)&pWls->lock_alloc, NULL);

    pWls->nTotalAllocCnt = 0;
    pWls->nTotalFreeCnt = 0;
    pWls->nTotalUlBufAllocCnt = 0;
    pWls->nTotalUlBufFreeCnt = 0;
    pWls->nTotalDlBufAllocCnt = 0;
    pWls->nTotalDlBufFreeCnt = 0;

    pWls->hWls = WLS_Open(wls_device_name, WLS_MASTER_CLIENT, &nWlsMacMemSize, &nWlsPhyMemSize);
    if (pWls->hWls)
    {
        /* allocate chuck of memory */
        pWls->pWlsMemBase = WLS_Alloc(pWls->hWls, nWlsMacMemSize+nWlsPhyMemSize);
        if (pWls->pWlsMemBase)
        {
            pWls->nTotalMemorySize = nWlsMacMemSize;
            // pWls->nBlockSize       = wls_mac_check_block_size(nBlockSize);

            ret = wls_mac_create_partition(pWls);

            if (ret == SUCCESS)
            {
                int nBlocks = 0;
                gwls_mac_ready = 1;

                nBlocks = WLS_EnqueueBlock(pWls->hWls, wls_mac_va_to_pa(wls_mac_alloc_buffer(0, MIN_UL_BUF_LOCATIONS+2)));
                /* allocate blocks for UL transmition */
                while(WLS_EnqueueBlock(pWls->hWls, wls_mac_va_to_pa(wls_mac_alloc_buffer(0, MIN_UL_BUF_LOCATIONS+3))))
                {
                    nBlocks++;
                }

                printf("WLS inited ok [%d]\n\n", nBlocks);
            }
            else
            {
                printf("can't create WLS Partition");
                return FAILURE;
            }

        }
        else
        {
            printf("can't allocate WLS memory");
            return FAILURE;
        }
    }
    else
    {
        printf("can't open WLS instance");
        return FAILURE;
    }

    return SUCCESS;
}


//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param   void
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function destroys the WLS layer for the testmac and de-allocates any memory used
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t wls_mac_destroy(void)
{
    PWLS_MAC_CTX pWls = wls_mac_get_ctx();

    if (pwls_testmac_thread)
    {
        pthread_cancel(*pwls_testmac_thread);

        free(pwls_testmac_thread);
        pwls_testmac_thread = NULL;

        if(pWls->pWlsMemBase)
        {
            WLS_Free(pWls->hWls, pWls->pWlsMemBase);
        }

        WLS_Close(pWls->hWls);
        printf("wls_mac_rx_task:          [PID: %6d]... Stopping\n", gwls_pid);
    }

    return SUCCESS;
}

uint8_t mac_dpdk_init()
{
    uint8_t retval;
    char whitelist[32];
    uint8_t i;

    char *argv[] = {"mac_app", "--proc-type=secondary",
        "--file-prefix", "wls", whitelist};
    
    int argc = RTE_DIM(argv);

    /* initialize EAL first */
    sprintf(whitelist, "-a%s",  "0000:00:06.0");
    printf("[MAC] Calling rte_eal_init: ");

    for (i = 0; i < RTE_DIM(argv); i++)
    {
        printf("%s ", argv[i]);
    }
    printf("\n");

    if (rte_eal_init(argc, argv) < 0)
        rte_panic("Cannot init EAL\n");

    return SUCCESS;
}
