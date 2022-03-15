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
 * @file This file has Shared Memory interface functions between FAPI and PHY
 * @defgroup nr5g_fapi_source_framework_wls_lib_group
 **/

#include "nr5g_fapi_framework.h"
#include "nr5g_fapi_internal.h"
#include "nr5g_fapi_wls.h"
#include "nr5g_fapi_config_loader.h"
#include "nr5g_fapi_log.h"
#include "nr5g_fapi_memory.h"

#define WLS_HUGE_DEF_PAGE_SIZEA 0x40000000LL

nr5g_fapi_wls_context_t g_wls_ctx;

static uint8_t alloc_track[ALLOC_TRACK_SIZE];

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_lib_group
 *
 *  @param void
 *
 *  @return  A pointer to WLS Context stucture
 *
 *  @description
 *  This function returns the WLS Context structure which has WLS related parameters
 *
**/
//------------------------------------------------------------------------------
inline p_nr5g_fapi_wls_context_t nr5g_fapi_wls_context(
    )
{
    return &g_wls_ctx;
}

//----------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_fapi2phy_group
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
//----------------------------------------------------------------------------------
inline uint8_t nr5g_fapi_fapi2phy_wls_ready(
    )
{
    int retval = 0;
    p_nr5g_fapi_wls_context_t p_wls = nr5g_fapi_wls_context();

    retval = WLS_Ready(p_wls->h_wls[NR5G_FAPI2PHY_WLS_INST]);

    return retval;
}

//----------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_fapi2phy_group
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
//----------------------------------------------------------------------------------
inline uint8_t nr5g_fapi_fapi2mac_wls_ready(
    )
{
    int retval = 0;
    p_nr5g_fapi_wls_context_t p_wls = nr5g_fapi_wls_context();

    retval = WLS_Ready1(p_wls->h_wls[NR5G_FAPI2MAC_WLS_INST]);

    return retval;
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_lib_group
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
//------------------------------------------------------------------------------
void nr5g_fapi_wls_show_data(
    void *ptr,
    uint32_t size)
{
    uint8_t *d = ptr;
    int i;

    for (i = 0; i < size; i++) {
        if (!(i & 0xf))
            printf("\n");
        printf("%02x ", d[i]);
    }
    printf("\n");
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_lib_group
 *
 *  @param   N/A
 *
 *  @return  N/A
 *
 *  @description
 *  This function prints to the console FAPI stats
 *
**/
//------------------------------------------------------------------------------
void nr5g_fapi_wls_print_stats(
    void)
{
    p_nr5g_fapi_wls_context_t pWls = nr5g_fapi_wls_context();
    printf("          nTotalBlocks[%5d]  nAllocBlocks[%5d]  nFreeBlocks[%5d]\n",
        pWls->nTotalBlocks, pWls->nAllocBlocks,
        (pWls->nTotalBlocks - pWls->nAllocBlocks));
    printf("        nTotalAllocCnt[%5d] nTotalFreeCnt[%5d]         Diff[%5d]\n",
        pWls->nTotalAllocCnt, pWls->nTotalFreeCnt,
        (pWls->nTotalAllocCnt - pWls->nTotalFreeCnt));
    uint32_t nFinalTotalDlBufAllocCnt = 0, nFinalTotalDlBufFreeCnt = 0, idx;

//#define PRINTF_DEBUG(fmt, args...) //printf(fmt, ## args)
#define PRINTF_DEBUG(fmt, args...)
    PRINTF_DEBUG("\n");
    PRINTF_DEBUG("\n        nDlBufAllocCnt: \n");
    for (idx = 0; idx < MEM_STAT_DEFAULT; idx++) {
        nFinalTotalDlBufAllocCnt += pWls->nTotalDlBufAllocCnt[idx];
        PRINTF_DEBUG("[%3d:%5d] ", idx, pWls->nTotalDlBufAllocCnt[idx]);
    }
    PRINTF_DEBUG("\n");
    PRINTF_DEBUG("\n         nDlBufFreeCnt: \n");
    for (idx = 0; idx < MEM_STAT_DEFAULT; idx++) {
        nFinalTotalDlBufFreeCnt += pWls->nTotalDlBufFreeCnt[idx];
        PRINTF_DEBUG("[%3d:%5d] ", idx, pWls->nTotalDlBufFreeCnt[idx]);
    }
    PRINTF_DEBUG("\n\n");

    printf("        nDlBufAllocCnt[%5d] nDlBufFreeCnt[%5d]         Diff[%5d]\n",
        nFinalTotalDlBufAllocCnt, nFinalTotalDlBufFreeCnt,
        (nFinalTotalDlBufAllocCnt - nFinalTotalDlBufFreeCnt));
    printf
        ("        nUlBufAllocCnt[%5d] nUlBufFreeCnt[%5d]         Diff[%5d]\n\n",
        pWls->nTotalUlBufAllocCnt, pWls->nTotalUlBufFreeCnt,
        (pWls->nTotalUlBufAllocCnt - pWls->nTotalUlBufFreeCnt));
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_lib_group
 *
 *  @param[in]   ptr Address to convert
 *
 *  @return  Converted address
 *
 *  @description
 *  This function converts Virtual Address to Physical Address
 *
**/
//------------------------------------------------------------------------------
uint64_t nr5g_fapi_wls_va_to_pa(
    WLS_HANDLE h_wls,
    void *ptr)
{
    return ((uint64_t) WLS_VA2PA(h_wls, ptr));
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_lib_group
 *
 *  @param[in]   ptr Address to convert
 *
 *  @return  Converted address
 *
 *  @description
 *  This function converts Physical Address to Virtual Address
 *
**/
//------------------------------------------------------------------------------
void *nr5g_fapi_wls_pa_to_va(
    WLS_HANDLE h_wls,
    uint64_t ptr)
{
    return ((void *)WLS_PA2VA(h_wls, ptr));
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_lib_group 
 *
 *  @param   void
 *
 *  @return  Number of blocks added
 *
 *  @description
 *  This function add WLS blocks to the L1 Array which will be used by L1 in 
 *  every TTI to populate and send back APIs to the MAC
 *
**/
//------------------------------------------------------------------------------
uint32_t wls_fapi_add_blocks_to_ul(
    void)
{
    uint32_t num_blocks = 0;
    p_nr5g_fapi_wls_context_t pWls = nr5g_fapi_wls_context();
    WLS_HANDLE h_wls = pWls->h_wls[NR5G_FAPI2PHY_WLS_INST];

    void *pMsg = wls_fapi_alloc_buffer(0, MIN_UL_BUF_LOCATIONS);
    if (!pMsg) {
        return num_blocks;
    }

    /* allocate blocks for UL transmittion */
    while (WLS_EnqueueBlock(h_wls, nr5g_fapi_wls_va_to_pa(h_wls, pMsg)) > 0) {
        num_blocks++;
        pMsg = wls_fapi_alloc_buffer(0, MIN_UL_BUF_LOCATIONS);
        if (!pMsg)
            break;
    }

    // free not enqueued block
    if (pMsg) {
        wls_fapi_free_buffer(pMsg, MIN_UL_BUF_LOCATIONS);
    }

    return num_blocks;
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_lib_group
 *
 *  @param          A pointer to the phy instance table.
 *
 *  @return         0 if SUCCESS
 *
 *  @description    This function initializes WLS layer primitives and allocates
 *                  memory needed to exchange APIs between FAPI and PHY.
**/
//------------------------------------------------------------------------------
uint8_t nr5g_fapi_wls_init(
    p_nr5g_fapi_cfg_t cfg)
{
    uint64_t mac_shmem_size = 0;
    uint64_t phy_shmem_size = 0;

    p_nr5g_fapi_wls_context_t p_wls_ctx = nr5g_fapi_wls_context();

    if (p_wls_ctx->h_wls[NR5G_FAPI2PHY_WLS_INST] &&
        p_wls_ctx->h_wls[NR5G_FAPI2MAC_WLS_INST]) {
        // NR5G_FAPI_LOG(ERROR_LOG, ("WLS instance already opened!"));
        return FAILURE;
    }

    p_wls_ctx->h_wls[NR5G_FAPI2MAC_WLS_INST] =
        WLS_Open_Dual(basename(cfg->wls.device_name), WLS_SLAVE_CLIENT,
        &mac_shmem_size, &phy_shmem_size, &p_wls_ctx->h_wls[NR5G_FAPI2PHY_WLS_INST]);
    
    cfg->wls.shmem_size = mac_shmem_size + phy_shmem_size;
    p_wls_ctx->shmem_size = cfg->wls.shmem_size;
    if ((NULL == p_wls_ctx->h_wls[NR5G_FAPI2PHY_WLS_INST]) &&
        (NULL == p_wls_ctx->h_wls[NR5G_FAPI2MAC_WLS_INST])) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[NR5G_FAPI_ WLS] WLS Open Dual Failed."));
        return FAILURE;
    }
    // Issue WLS_Alloc() for FAPI2MAC
    p_wls_ctx->shmem = WLS_Alloc(p_wls_ctx->h_wls[NR5G_FAPI2MAC_WLS_INST],
        p_wls_ctx->shmem_size);

    if (NULL == p_wls_ctx->shmem) {
        printf("Unable to alloc WLS Memory for FAPI2MAC\n");
        return FAILURE;
    }

    p_wls_ctx->shmem = WLS_Alloc(p_wls_ctx->h_wls[NR5G_FAPI2PHY_WLS_INST],
        p_wls_ctx->shmem_size);
    p_wls_ctx->pWlsMemBase = p_wls_ctx->shmem;
    p_wls_ctx->nTotalMemorySize = mac_shmem_size;
    if (NULL == p_wls_ctx->shmem) {
        printf("Unable to alloc WLS Memory\n");
        return FAILURE;
    }
    // Now the L2 is up so let's make sure that the L1 was started first
    usleep(1000000);
    // First let's wait for the L1 and L2 to be present
    while (nr5g_fapi_fapi2phy_wls_ready()) ;
    NR5G_FAPI_LOG(INFO_LOG, ("L1 is up..."));
    // Now the L2 is up so let's make sure that the L1 was started first
    while (nr5g_fapi_fapi2mac_wls_ready()) ;
    NR5G_FAPI_LOG(INFO_LOG, ("L2 is up..."));

    // Now that the L2 is up and has completed the Common Memory initialization
    usleep(1000000);
    if (FAILURE == nr5g_fapi_wls_memory_init()) {
        return FAILURE;
    }

    pthread_mutex_init((pthread_mutex_t *)
        & p_wls_ctx->fapi2phy_lock_send, NULL);
    pthread_mutex_init((pthread_mutex_t *)
        & p_wls_ctx->fapi2phy_lock_alloc, NULL);
    pthread_mutex_init((pthread_mutex_t *)
        & p_wls_ctx->fapi2mac_lock_send, NULL);
    pthread_mutex_init((pthread_mutex_t *)
        & p_wls_ctx->fapi2mac_lock_alloc, NULL);
    return SUCCESS;
}

//-------------------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_lib_group
 *
 *  @param[in]   pMemArray Pointer to WLS Memory Management Structure
 *  @param[in]   pMemArrayMmeory pointer to flat buffer that was allocated
 *  @param[in]   totalSize total size of flat buffer allocated
 *  @param[in]   nBlockSize Size of each block that needs to be partitoned by the memory manager
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function creates memory blocks from a flat buffer which will be used for communication between FAPI and PHY
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t wls_fapi_create_mem_array(
    PWLS_FAPI_MEM_STRUCT pMemArray,
    void *pMemArrayMemory,
    uint32_t totalSize,
    uint32_t nBlockSize)
{

    int numBlocks = totalSize / nBlockSize;
    void **ptr;
    uint32_t i;

    printf
        ("wls_fapi_create_mem_array: pMemArray[%p] pMemArrayMemory[%p] totalSize[%d] nBlockSize[%d] numBlocks[%d]\n",
        pMemArray, pMemArrayMemory, totalSize, nBlockSize, numBlocks);

    // Can't be less than pointer size
    if (nBlockSize < sizeof(void *)) {
        return FAILURE;
    }
    // Can't be less than one block
    if (totalSize < sizeof(void *)) {
        return FAILURE;
    }

    pMemArray->ppFreeBlock = (void **)pMemArrayMemory;
    pMemArray->pStorage = pMemArrayMemory;
    pMemArray->pEndOfStorage =
        ((unsigned long *)pMemArrayMemory) +
        numBlocks * nBlockSize / sizeof(unsigned long);
    pMemArray->nBlockSize = nBlockSize;
    pMemArray->nBlockCount = numBlocks;

    // Initialize single-linked list of free blocks;
    ptr = (void **)pMemArrayMemory;
    for (i = 0; i < pMemArray->nBlockCount; i++) {
#ifdef MEMORY_CORRUPTION_DETECT
        // Fill with some pattern
        uint8_t *p = (uint8_t *) ptr;
        uint32_t j;

        p += (nBlockSize - 16);
        for (j = 0; j < 16; j++) {
            p[j] = MEMORY_CORRUPTION_DETECT_FLAG;
        }
#endif

        if (i == pMemArray->nBlockCount - 1) {
            *ptr = NULL;        // End of list
        } else {
            // Points to the next block
            *ptr = (void **)(((uint8_t *) ptr) + nBlockSize);
            ptr += nBlockSize / sizeof(unsigned long);
        }
    }

    NR5G_FAPI_MEMSET(alloc_track, sizeof(uint8_t) * ALLOC_TRACK_SIZE, 0,
        sizeof(uint8_t) * ALLOC_TRACK_SIZE);

    return SUCCESS;
}

//-------------------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_lib_group
 *
 *  @param[in]   pWls Pointer to the nr5g_fapi_wls_ctx structure
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function created a partition and blocks of WLS memory for API exchange between FAPI and PHY
 *
**/
//-------------------------------------------------------------------------------------------
uint8_t wls_fapi_create_partition(
    p_nr5g_fapi_wls_context_t pWls)
{
    uint64_t nWlsMemBaseUsable;
    uint64_t nTotalMemorySizeUsable;
    uint64_t nBalance, nBlockSize, nBlockSizeMask, nHugepageSizeMask;

    nBlockSize = MSG_MAXSIZE;
    nWlsMemBaseUsable = (uint64_t)pWls->pWlsMemBase;
    nTotalMemorySizeUsable = pWls->nTotalMemorySize - WLS_HUGE_DEF_PAGE_SIZEA;
    nBlockSizeMask = nBlockSize-1;

    // Align Starting Location
    nWlsMemBaseUsable = (nWlsMemBaseUsable + nBlockSizeMask) & (~nBlockSizeMask);
    nBalance = nWlsMemBaseUsable - (uint64_t)pWls->pWlsMemBase;
    nTotalMemorySizeUsable -= nBalance;

    // Align Ending Location
    nBalance = nTotalMemorySizeUsable % nBlockSize;
    nTotalMemorySizeUsable -= nBalance;

    // Move start location to the next hugepage boundary
    nHugepageSizeMask = WLS_HUGE_DEF_PAGE_SIZEA-1;
    nWlsMemBaseUsable = (nWlsMemBaseUsable + WLS_HUGE_DEF_PAGE_SIZEA) & (~nHugepageSizeMask);


    pWls->pPartitionMemBase = (void *)nWlsMemBaseUsable;
    pWls->nPartitionMemSize = nTotalMemorySizeUsable;
    pWls->nTotalBlocks = pWls->nPartitionMemSize / nBlockSize;
    return wls_fapi_create_mem_array(&pWls->sWlsStruct, pWls->pPartitionMemBase,
        pWls->nPartitionMemSize, nBlockSize);
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_lib_group
 *
 *  @param[in]      A pointer to the FAPI Memory Structure (Initialized by L2)
 *  @param[out]     ppBlock Pointer where the allocated memory block is stored     
 *
 *  @return         0 if SUCCESS
 *
 *  @description    This function allocates a memory block from the pool
 *
**/
//------------------------------------------------------------------------------
uint32_t wls_fapi_alloc_mem_array(
    PWLS_FAPI_MEM_STRUCT pMemArray,
    void **ppBlock)
{
    int idx;

    if (pMemArray->ppFreeBlock == NULL) {
        printf("wls_fapi_alloc_mem_array pMemArray->ppFreeBlock = NULL\n");
        return FAILURE;
    }
    // FIXME: Remove after debugging
    if (((void *)pMemArray->ppFreeBlock < pMemArray->pStorage) ||
        ((void *)pMemArray->ppFreeBlock >= pMemArray->pEndOfStorage)) {
        printf
            ("wls_fapi_alloc_mem_array ERROR: Corrupted MemArray;Arr=%p,Stor=%p,Free=%p\n",
            pMemArray, pMemArray->pStorage, pMemArray->ppFreeBlock);
        return FAILURE;
    }

    pMemArray->ppFreeBlock =
        (void **)((unsigned long)pMemArray->ppFreeBlock & 0xFFFFFFFFFFFFFFF0);
    *pMemArray->ppFreeBlock =
        (void **)((unsigned long)*pMemArray->ppFreeBlock & 0xFFFFFFFFFFFFFFF0);

    if ((*pMemArray->ppFreeBlock != NULL) &&
        (((*pMemArray->ppFreeBlock) < pMemArray->pStorage) ||
            ((*pMemArray->ppFreeBlock) >= pMemArray->pEndOfStorage))) {
        fprintf(stderr,
            "ERROR: Corrupted MemArray;Arr=%p,Stor=%p,Free=%p,Curr=%p\n",
            pMemArray, pMemArray->pStorage, pMemArray->ppFreeBlock,
            *pMemArray->ppFreeBlock);
        return FAILURE;
    }

    *ppBlock = (void *)pMemArray->ppFreeBlock;
    pMemArray->ppFreeBlock = (void **)(*pMemArray->ppFreeBlock);

    idx =
        (((uint64_t) * ppBlock -
            (uint64_t) pMemArray->pStorage)) / pMemArray->nBlockSize;
    if (alloc_track[idx]) {
        printf
            ("wls_fapi_alloc_mem_array Double alloc Arr=%p,Stor=%p,Free=%p,Curr=%p\n",
            pMemArray, pMemArray->pStorage, pMemArray->ppFreeBlock,
            *pMemArray->ppFreeBlock);
        return FAILURE;
    } else {
#ifdef MEMORY_CORRUPTION_DETECT
        uint32_t nBlockSize = pMemArray->nBlockSize, i;
        uint8_t *p = (uint8_t *) * ppBlock;

        p += (nBlockSize - 16);
        for (i = 0; i < 16; i++) {
            p[i] = MEMORY_CORRUPTION_DETECT_FLAG;
        }
#endif
        alloc_track[idx] = 1;
    }

    //printf("Block allocd [%p,%p]\n", pMemArray, *ppBlock);

    return SUCCESS;
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_lib_group
 *
 *  @param[in]      A pointer to the FAPI Memory Structure (Initialized by L2)
 *  @param[in]      pBlock Pointer where the allocated memory block is stored     
 *
 *  @return         0 if SUCCESS
 *
 *  @description    This function frees a WLS block of memory and adds 
 *                  it back to the pool
 *
**/
//------------------------------------------------------------------------------
uint32_t wls_fapi_free_mem_array(
    PWLS_FAPI_MEM_STRUCT pMemArray,
    void *pBlock)
{
    int idx;
    unsigned long mask = (((unsigned long)pMemArray->nBlockSize) - 1);

    pBlock = (void *)((unsigned long)pBlock & ~mask);

    if ((pBlock < pMemArray->pStorage) || (pBlock >= pMemArray->pEndOfStorage)) {
        printf
            ("wls_fapi_free_mem_array WARNING: Trying to free foreign block;Arr=%p,Blk=%p pStorage [%p .. %p]\n",
            pMemArray, pBlock, pMemArray->pStorage, pMemArray->pEndOfStorage);
        return FAILURE;
    }

    idx =
        (int)(((uint64_t) pBlock -
            (uint64_t) pMemArray->pStorage)) / pMemArray->nBlockSize;

    if (alloc_track[idx] == 0) {
        printf
            ("wls_fapi_free_mem_array ERROR: Double free Arr=%p,Stor=%p,Free=%p,Curr=%p\n",
            pMemArray, pMemArray->pStorage, pMemArray->ppFreeBlock, pBlock);
        return SUCCESS;
    } else {
#ifdef MEMORY_CORRUPTION_DETECT
        uint32_t nBlockSize = pMemArray->nBlockSize, i;
        uint8_t *p = (uint8_t *) pBlock;

        p += (nBlockSize - 16);
        for (i = 0; i < 16; i++) {
            if (p[i] != MEMORY_CORRUPTION_DETECT_FLAG) {
                printf("ERROR: Corruption\n");
                nr5g_fapi_wls_print_stats();
                exit(-1);
            }
        }
#endif
        alloc_track[idx] = 0;
    }

    if (((void *)pMemArray->ppFreeBlock) == pBlock) {
        // Simple protection against freeing of already freed block
        return SUCCESS;
    }
    // FIXME: Remove after debugging
    if ((pMemArray->ppFreeBlock != NULL)
        && (((void *)pMemArray->ppFreeBlock < pMemArray->pStorage)
            || ((void *)pMemArray->ppFreeBlock >= pMemArray->pEndOfStorage))) {
        printf
            ("wls_fapi_free_mem_array ERROR: Corrupted MemArray;Arr=%p,Stor=%p,Free=%p\n",
            pMemArray, pMemArray->pStorage, pMemArray->ppFreeBlock);
        return FAILURE;
    }
    // FIXME: Remove after debugging
    if ((pBlock < pMemArray->pStorage) || (pBlock >= pMemArray->pEndOfStorage)) {
        printf("wls_fapi_free_mem_array ERROR: Invalid block;Arr=%p,Blk=%p\n",
            pMemArray, pBlock);
        return FAILURE;
    }

    *((void **)pBlock) =
        (void **)((unsigned long)pMemArray->ppFreeBlock & 0xFFFFFFFFFFFFFFF0);
    pMemArray->ppFreeBlock =
        (void **)((unsigned long)pBlock & 0xFFFFFFFFFFFFFFF0);

    //printf("Block freed [%p,%p]\n", pMemArray, pBlock);

    return SUCCESS;
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_lib_group
 *
 *  @param          size (if 0 fixed size from pool)
 *  @param          Number of locations
 *
 *  @return         0 if SUCCESS
 *
 *  @description    This function initializes WLS layer primitives and allocates
 *                  memory needed to exchange APIs between FAPI and PHY.
**/
//------------------------------------------------------------------------------
void *wls_fapi_alloc_buffer(
    uint32_t size,
    uint32_t loc)
{
    void *pBlock = NULL;
    p_nr5g_fapi_wls_context_t pWls = nr5g_fapi_wls_context();

    if (pthread_mutex_lock((pthread_mutex_t *) & pWls->fapi2phy_lock_alloc)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("unable to get lock alloc pthread mutex"));
        exit(-1);
    }

    if (wls_fapi_alloc_mem_array(&pWls->sWlsStruct, &pBlock) != SUCCESS) {
        printf("wls_fapi_alloc_buffer alloc error size[%d] loc[%d]\n", size,
            loc);
        nr5g_fapi_wls_print_stats();
        exit(-1);
    } else {
        pWls->nAllocBlocks++;
    }

    //printf("----------------wls_fapi_alloc_buffer: size[%d] loc[%d] buf[%p] nAllocBlocks[%d]\n", size, loc, pBlock, pWls->nAllocBlocks);

    //printf("[%p]\n", pBlock);

    pWls->nTotalAllocCnt++;
    if (loc < MAX_DL_BUF_LOCATIONS)
        pWls->nTotalDlBufAllocCnt[loc]++;
    else if (loc < MAX_UL_BUF_LOCATIONS)
        pWls->nTotalUlBufAllocCnt++;

    if (pthread_mutex_unlock((pthread_mutex_t *) & pWls->fapi2phy_lock_alloc)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("unable to unlock alloc pthread mutex"));
        exit(-1);
    }

    return pBlock;
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_lib_group
 *
 *  @param[in]      *pMsg Pointer to free
 *
 *  @return         void
 *
 *  @descriptioni   This function frees a block of memory and adds it back to 
 *                  the pool.
 *
**/
//------------------------------------------------------------------------------
void wls_fapi_free_buffer(
    void *pMsg,
    uint32_t loc)
{
    p_nr5g_fapi_wls_context_t pWls = nr5g_fapi_wls_context();

    if (pthread_mutex_lock((pthread_mutex_t *) & pWls->fapi2phy_lock_alloc)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("unable to lock alloc pthread mutex"));
        exit(-1);
    }
    //printf("----------------wls_fapi_free_buffer: buf[%p] loc[%d]\n", pMsg, loc);
    if (wls_fapi_free_mem_array(&pWls->sWlsStruct, (void *)pMsg) == SUCCESS) {
        pWls->nAllocBlocks--;
    } else {
        printf("wls_fapi_free_buffer Free error\n");
        nr5g_fapi_wls_print_stats();
        exit(-1);
    }

    pWls->nTotalFreeCnt++;
    if (loc < MAX_DL_BUF_LOCATIONS)
        pWls->nTotalDlBufFreeCnt[loc]++;
    else if (loc < MAX_UL_BUF_LOCATIONS)
        pWls->nTotalUlBufFreeCnt++;

    if (pthread_mutex_unlock((pthread_mutex_t *) & pWls->fapi2phy_lock_alloc)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("unable to unlock alloc pthread mutex"));
        exit(-1);
    }
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_lib_group
 *
 *  @param          A pointer to a wls instance
 *
 *  @return         0 if SUCCESS
 *
 *  @description    This function initializes the WLS layer FAPI2PHY interface 
 *                  primitives and allocates memory needed to exchange APIs 
 *                  between FAPI and PHY.
**/
//------------------------------------------------------------------------------
uint8_t nr5g_fapi_wls_memory_init(
    )
{
    uint32_t nBlocks = 0;
    p_nr5g_fapi_wls_context_t p_wls = nr5g_fapi_wls_context();

    if (FAILURE == wls_fapi_create_partition(p_wls))
        return FAILURE;

    if ((nBlocks = wls_fapi_add_blocks_to_ul()) == 0) {
        NR5G_FAPI_LOG(ERROR_LOG, ("Unable to allocate blocks to PHY"));
        return FAILURE;
    }

    return SUCCESS;
}
