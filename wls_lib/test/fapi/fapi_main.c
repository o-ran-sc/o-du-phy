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
 * @brief This file is test FAPI wls lib main process
 * @file fapi_main.c
 * @ingroup group_testfapiwls
 * @author Intel Corporation
 **/


#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

#define NR5G_FAPI2PHY_WLS_INST 0
#define NR5G_FAPI2MAC_WLS_INST 1
#define SUCCESS 0
#define FAILURE 1
#define WLS_TEST_DEV_NAME "wls"
#define WLS_TEST_MSG_ID   1
#define WLS_TEST_MSG_SIZE 100
#define ALLOC_TRACK_SIZE 16384
#define MIN_UL_BUF_LOCATIONS 50
#define MIN_DL_BUF_LOCATIONS 50
#define MAX_UL_BUF_LOCATIONS 50
#define MAX_DL_BUF_LOCATIONS 50
#define NUM_WLS_INSTANCES 2
#define WLS_HUGE_DEF_PAGE_SIZEA    0x40000000LL
#define MSG_MAXSIZE1 (16*16384)
#define NUM_TEST_MSGS   16




typedef void* WLS_HANDLE;
typedef struct wls_fapi_mem_array
{
    void **ppFreeBlock;
    void *pStorage;
    void *pEndOfStorage;
    uint32_t nBlockSize;
    uint32_t nBlockCount;
} WLS_FAPI_MEM_STRUCT, *PWLS_FAPI_MEM_STRUCT;

// WLS context structure
typedef struct _nr5g_fapi_wls_context {
    void        *shmem;       // shared  memory region.
    uint64_t    shmem_size;   // shared  memory region size.
    WLS_FAPI_MEM_STRUCT sWlsStruct;
    WLS_HANDLE  h_wls[NUM_WLS_INSTANCES]; // WLS context handle
    void *pWlsMemBase;
    uint32_t nTotalMemorySize;
    uint32_t nTotalBlocks;
    uint32_t nAllocBlocks;
    uint32_t nTotalAllocCnt;
    uint32_t nTotalFreeCnt;
    uint32_t nTotalUlBufAllocCnt;
    uint32_t nTotalUlBufFreeCnt;
    uint32_t nTotalDlBufAllocCnt;
    uint32_t nTotalDlBufFreeCnt;
    uint32_t nPartitionMemSize;
    void     *pPartitionMemBase;
    volatile pthread_mutex_t lock;
    volatile pthread_mutex_t lock_alloc;
} nr5g_fapi_wls_context_t, *p_nr5g_fapi_wls_context_t;

nr5g_fapi_wls_context_t  g_wls_ctx;
typedef void* WLS_HANDLE;
static uint8_t alloc_track[ALLOC_TRACK_SIZE];
void *g_shmem;
uint32_t g_shmem_size=0;
uint32_t g_nTotalAllocCnt=0;
uint32_t g_nAllocBlocks=0;
uint32_t gwls_fapi_ready=0;
WLS_FAPI_MEM_STRUCT g_MemArray;
nr5g_fapi_wls_context_t  g_wls_ctx;
uint32_t g_nTotalDlBufAllocCnt=0;
uint32_t g_nTotalUlBufAllocCnt=0;

wls_us_ctx_t gphy_wls;
wls_us_ctx_t gfapi_wls;

WLS_HANDLE   g_phy_wls = &gphy_wls;
WLS_HANDLE   g_fapi_wls = &gfapi_wls;

uint8_t    fapi_dpdk_init(void);
uint8_t    fapi_wls_init(const char *dev_name);
uint64_t   fapi_mac_recv();
uint8_t    fapi_phy_send();
uint64_t   fapi_phy_recv();
uint8_t    fapi_mac_send();
uint8_t    fapi2Phy_wls_init(p_nr5g_fapi_wls_context_t pwls);
uint8_t wls_fapi_create_partition(p_nr5g_fapi_wls_context_t pWls);
uint32_t wls_fapi_alloc_mem_array(PWLS_FAPI_MEM_STRUCT pMemArray, void **ppBlock);
uint32_t wls_fapi_create_mem_array(PWLS_FAPI_MEM_STRUCT pMemArray, void *pMemArrayMemory, uint32_t totalSize, uint32_t nBlockSize);

uint64_t nr5g_fapi_wls_va_to_pa(WLS_HANDLE h_wls, void *ptr)
{
    return ((uint64_t)WLS_VA2PA(h_wls, ptr));
}

inline p_nr5g_fapi_wls_context_t nr5g_fapi_wls_context()
{
    return &g_wls_ctx;
}

static inline WLS_HANDLE nr5g_fapi_fapi2mac_wls_instance()
{
    p_nr5g_fapi_wls_context_t p_wls_ctx = nr5g_fapi_wls_context();

    return p_wls_ctx->h_wls[NR5G_FAPI2MAC_WLS_INST];
}

static inline WLS_HANDLE nr5g_fapi_fapi2phy_wls_instance()
{
    p_nr5g_fapi_wls_context_t p_wls_ctx = nr5g_fapi_wls_context();
    return p_wls_ctx->h_wls[NR5G_FAPI2PHY_WLS_INST];
}
void *wls_fapi_alloc_buffer(uint32_t size, uint32_t loc)
{
    void *pBlock = NULL;
    p_nr5g_fapi_wls_context_t pWls = nr5g_fapi_wls_context();
    PWLS_FAPI_MEM_STRUCT pMemArray = &pWls->sWlsStruct;

    pthread_mutex_lock((pthread_mutex_t *)&pWls->lock_alloc);

    if (wls_fapi_alloc_mem_array(&pWls->sWlsStruct, &pBlock) != SUCCESS)
    {
        printf("wls_fapi_alloc_buffer alloc error size[%d] loc[%d]\n", size, loc);

        exit(-1);
    }
    else
    {
        pWls->nAllocBlocks++;
    }

    //printf("----------------wls_fapi_alloc_buffer: size[%d] loc[%d] buf[%p] nAllocBlocks[%d]\n", size, loc, pBlock, pWls->nAllocBlocks);

    //printf("[%p]\n", pBlock);

    pWls->nTotalAllocCnt++;
    if (loc < MAX_DL_BUF_LOCATIONS)
        pWls->nTotalDlBufAllocCnt++;
    else if (loc < MAX_UL_BUF_LOCATIONS)
        pWls->nTotalUlBufAllocCnt++;

    pthread_mutex_unlock((pthread_mutex_t *)&pWls->lock_alloc);
    return pBlock;
}

uint32_t wls_fapi_create_mem_array(PWLS_FAPI_MEM_STRUCT pMemArray, void *pMemArrayMemory, uint32_t totalSize, uint32_t nBlockSize)
{

    int numBlocks = totalSize / nBlockSize;
    void **ptr;
    uint32_t i;

    printf("wls_fapi_create_mem_array: pMemArray[%p] pMemArrayMemory[%p] totalSize[%d] nBlockSize[%d] numBlocks[%d]\n",
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

uint32_t wls_fapi_alloc_mem_array(PWLS_FAPI_MEM_STRUCT pMemArray, void **ppBlock)
{
    int idx;

    if (pMemArray->ppFreeBlock == NULL)
    {
        printf("wls_fapi_alloc_mem_array pMemArray->ppFreeBlock = NULL\n");
        return FAILURE;
    }

    // FIXME: Remove after debugging
    if (((void *) pMemArray->ppFreeBlock < pMemArray->pStorage) ||
        ((void *) pMemArray->ppFreeBlock >= pMemArray->pEndOfStorage))
    {
        printf("wls_fapi_alloc_mem_array ERROR: Corrupted MemArray;Arr=%p,Stor=%p,Free=%p\n",
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
        printf("wls_fapi_alloc_mem_array Double alloc Arr=%p,Stor=%p,Free=%p,Curr=%p\n",
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



uint8_t wls_fapi_create_partition(p_nr5g_fapi_wls_context_t pWls)
{
static long hugePageSize = WLS_HUGE_DEF_PAGE_SIZEA;
     void *pPartitionMemBase;
     uint32_t nPartitionMemSize;
     uint32_t nTotalBlocks;
     
    pWls->pPartitionMemBase = pWls->pWlsMemBase + hugePageSize;
    pWls->nPartitionMemSize = (pWls->nTotalMemorySize - hugePageSize);

    pWls->nTotalBlocks = pWls->nPartitionMemSize / MSG_MAXSIZE1;

    return wls_fapi_create_mem_array(&pWls->sWlsStruct, pWls->pPartitionMemBase, pWls->nPartitionMemSize, MSG_MAXSIZE1);
}

uint8_t nr5g_fapi2Phy_wls_init(p_nr5g_fapi_wls_context_t pWls)
{
    int nBlocks = 0;
    uint8_t retval = SUCCESS;


    WLS_HANDLE h_wls = 	pWls->h_wls[NR5G_FAPI2PHY_WLS_INST];
    pthread_mutex_init((pthread_mutex_t *)&pWls->lock, NULL);
    pthread_mutex_init((pthread_mutex_t *)&pWls->lock_alloc, NULL);

    pWls->nTotalAllocCnt = 0;
    pWls->nTotalFreeCnt = 0;
    pWls->nTotalUlBufAllocCnt = 0;
    pWls->nTotalUlBufFreeCnt = 0;
    pWls->nTotalDlBufAllocCnt = 0;
    pWls->nTotalDlBufFreeCnt = 0;
    // Need to add wls_fapi_create_partition
    retval = wls_fapi_create_partition(pWls);
    if (retval == SUCCESS)
    {	
    	gwls_fapi_ready = 1;
    	nBlocks = WLS_EnqueueBlock(h_wls, nr5g_fapi_wls_va_to_pa( h_wls, wls_fapi_alloc_buffer(0, MIN_UL_BUF_LOCATIONS+2)));
    	printf("WLS_EnqueueBlock [%d]\n", nBlocks);
    	// Allocate Blocks for UL Transmission
    	while(WLS_EnqueueBlock(h_wls, nr5g_fapi_wls_va_to_pa(h_wls,wls_fapi_alloc_buffer(0, MIN_UL_BUF_LOCATIONS+3))))
    	{
         nBlocks++;
    	}
    	printf ("fapi2Phy UL Buffer Allocation completed\n");
    }
    else
    {
    	printf ("can't create WLS FAPI2PHY partition \n");
    	return FAILURE;
    }
    return retval;
}

uint8_t nr5g_fapi_fapi2mac_wls_ready()
{
    int ret = SUCCESS;
    ret = WLS_Ready1(nr5g_fapi_fapi2mac_wls_instance());
    return ret;
}


inline uint8_t nr5g_fapi_fapi2phy_wls_ready()
{
    int ret = SUCCESS;
    //NR5G_FAPI_LOG(TRACE_LOG, ("Waiting for L1 to respond in WLS Ready"));
    ret = WLS_Ready(nr5g_fapi_fapi2phy_wls_instance());
    return ret;
}

int main()
{
    uint8_t ret;
    uint64_t p_msg;
    uint8_t retval= FAILURE;
    p_nr5g_fapi_wls_context_t pwls;

    // DPDK init
    ret = fapi_dpdk_init();
    if (ret)
    {
        printf("\n[FAPI] DPDK Init - Failed\n");
        return FAILURE;
    }
    printf("\n[FAPI] DPDK Init - Done\n");

    // WLS init
    ret = fapi_wls_init(WLS_TEST_DEV_NAME);
    if(ret)
    {
        printf("\n[FAPI] WLS Init - Failed\n");
        return FAILURE;
    }
    // Need to check for L1 and L2 started before attempting partition creation
    // First let's wait for the L1 and L2 to be present
    while (retval)
    {
    	retval = nr5g_fapi_fapi2phy_wls_ready();
    }
    // Now the L2 is up so let's make sure that the L1 was started first
    retval=FAILURE;
    while (retval)
    {
    	retval = nr5g_fapi_fapi2mac_wls_ready();
    }
    // Now that the L2 is up and has completed the Common Memory initialization the FT needs to initialize the FAPI2PHY buffers
    pwls = nr5g_fapi_wls_context();
    usleep(1000000);
    ret = nr5g_fapi2Phy_wls_init(pwls);
    if(ret)
    {
        printf("\n[FAPI] 2Phy WLS Init - Failed\n");
        return FAILURE;
    }
    
    printf("\n[FAPI] WLS Init - Done\n");

    // Receive from MAC WLS
    p_msg = fapi_mac_recv();
    if (!p_msg)
    {
        printf("\n[FAPI] Receive from MAC - Failed\n");
        return FAILURE;
    }
    printf("\n[FAPI] Receive from MAC - Done\n");

    // Sent to PHY WLS
    ret = fapi_phy_send();
    if (ret)
    {
        printf("\n[FAPI] Send to PHY - Failed\n");
        return FAILURE;
    }
    printf("\n[FAPI] Send to PHY - Done\n");

    // Receive from PHY WLS
    p_msg = fapi_phy_recv();
    if (!p_msg)
    {
        printf("\n[FAPI] Receive from PHY - Failed\n");
        return FAILURE;
    }
    printf("\n[FAPI] Receive from PHY - Done\n");

    // Sent to MAC WLS
    ret = fapi_mac_send();
    if (ret)
    {
        printf("\n[FAPI] Send to MAC - Failed\n");
        return FAILURE;
    }
    printf("\n[FAPI] Send to MAC - Done\n");


    printf("\n[FAPI] Exiting...\n");

    return SUCCESS;
}

uint8_t fapi_dpdk_init(void)
{
    char whitelist[32];
    uint8_t i;

    char *argv[] = {"fapi_app", "--proc-type=secondary",
        "--file-prefix", "wls", whitelist};
    
    int argc = RTE_DIM(argv);

    /* initialize EAL first */
    sprintf(whitelist, "-a%s",  "0000:00:06.0");
    printf("[FAPI] Calling rte_eal_init: ");

    for (i = 0; i < RTE_DIM(argv); i++)
    {
        printf("%s ", argv[i]);
    }
    printf("\n");

    if (rte_eal_init(argc, argv) < 0)
        rte_panic("Cannot init EAL\n");

    return SUCCESS;
}

uint8_t fapi_wls_init(const char *dev_name)
{
    uint64_t nWlsMacMemorySize;
    uint64_t nWlsPhyMemorySize;
    uint8_t *pMemZone;
    static const struct rte_memzone *mng_memzone;
    p_nr5g_fapi_wls_context_t p_wls_ctx = nr5g_fapi_wls_context();    
    wls_drv_ctx_t *pDrv_ctx;

    p_wls_ctx->h_wls[NR5G_FAPI2MAC_WLS_INST] =
     WLS_Open_Dual(dev_name, WLS_SLAVE_CLIENT, &nWlsMacMemorySize, &nWlsPhyMemorySize, &p_wls_ctx->h_wls[NR5G_FAPI2PHY_WLS_INST]);
    if((NULL == p_wls_ctx->h_wls[NR5G_FAPI2PHY_WLS_INST]) && 
            (NULL == p_wls_ctx->h_wls[NR5G_FAPI2MAC_WLS_INST]))
    {
        return FAILURE;
    }
    g_shmem_size = nWlsMacMemorySize + nWlsPhyMemorySize;
    p_wls_ctx->shmem_size = g_shmem_size;
    // Issue WLS_Alloc() for FAPI2MAC
    p_wls_ctx->shmem = WLS_Alloc(
            p_wls_ctx->h_wls[NR5G_FAPI2MAC_WLS_INST],
            p_wls_ctx->shmem_size);

    if (NULL == p_wls_ctx->shmem)
    {
        printf("Unable to alloc WLS Memory for FAPI2MAC\n");
        return FAILURE;
    }            
    p_wls_ctx->shmem = WLS_Alloc(
            p_wls_ctx->h_wls[NR5G_FAPI2PHY_WLS_INST],
            p_wls_ctx->shmem_size);
    p_wls_ctx->pWlsMemBase = p_wls_ctx->shmem;
    p_wls_ctx->nTotalMemorySize = p_wls_ctx->shmem_size;
    if (NULL == p_wls_ctx->shmem)
    {
        printf("Unable to alloc WLS Memory for FAPI2PHY\n");
        return FAILURE;
    }
    
    return SUCCESS;
}    
    

uint64_t fapi_mac_recv()
{
    uint8_t  num_blks = 0;
    uint64_t p_msg;
    uint32_t msg_size;
    uint16_t msg_id;
    uint16_t flags;
    uint32_t i;
 
    WLS_HANDLE wls= nr5g_fapi_fapi2mac_wls_instance();
    
    for (i=0; i < NUM_TEST_MSGS; i++)
    {
    num_blks = WLS_Wait1(wls);
    
    if (num_blks)
    {
        p_msg = WLS_Get1(wls, &msg_size, &msg_id, &flags);
    }
    else
    {
        printf("\n[FAPI] FAPI2MAC WLS wait returned 0 blocks\n");
    }
    		if (p_msg)
    		{
    			printf("\n[FAPI] Receive from MAC Msg  %d-\n", i);
    	  }
    }    	  
    return p_msg;
}

uint8_t fapi_phy_send()
{
    uint64_t pa_block = 0;
    uint8_t ret = FAILURE;
    uint32_t i;
    WLS_HANDLE wls = nr5g_fapi_fapi2mac_wls_instance();
    WLS_HANDLE wlsp = nr5g_fapi_fapi2phy_wls_instance();
    
    for (i=0; i < NUM_TEST_MSGS; i++)
    {
    pa_block = (uint64_t) WLS_DequeueBlock((void*) wls);
    if (!pa_block)
    {
        printf("\n[FAPI] FAPI2MAC WLS Dequeue block error %d\n",i);
        return FAILURE;
    }

    	ret = WLS_Put(wlsp, pa_block, WLS_TEST_MSG_SIZE, WLS_TEST_MSG_ID, (i== (NUM_TEST_MSGS-1))? WLS_TF_FIN:0);
      if (ret)
    	{
        printf("\n[FAPI] Send to PHY %d- Failed\n",i);
        return FAILURE;
    	}
    	else
    	{
    		printf("\n[FAPI] Send to PHY %d done \n",i);
    	}
    }
    return ret;
}

uint64_t fapi_phy_recv()
{
    uint8_t  num_blks = 0;
    uint64_t p_msg;
    uint32_t msg_size;
    uint16_t msg_id;
    uint16_t flags;
    uint32_t i=0;
    WLS_HANDLE wls = nr5g_fapi_fapi2phy_wls_instance();
    
    while (1)
    {
    num_blks = WLS_Wait(wls);
    printf("WLS_Wait returns %d\n",num_blks);
    
    if (num_blks)
    {
        p_msg = WLS_Get(wls, &msg_size, &msg_id, &flags);
        printf("\n[FAPI] FAPI2PHY Received Block %d \n", i);
        i++;
        if (flags & WLS_TF_FIN)
        {
        	return p_msg;
        }
    }
    else
    {
        printf("\n[FAPI] FAPI2MAC WLS wait returned 0 blocks\n");
    }
    }
    return p_msg;
}

uint8_t fapi_mac_send()
{
    uint64_t pa_block = 0;
    uint8_t ret = FAILURE;
    uint32_t i;
    WLS_HANDLE wls = nr5g_fapi_fapi2mac_wls_instance();
    WLS_HANDLE wlsp = nr5g_fapi_fapi2phy_wls_instance();
    
    for (i=0; i < NUM_TEST_MSGS; i++)
    {
    pa_block = (uint64_t) WLS_DequeueBlock((void*) wlsp);
    if (!pa_block)
    {
        printf("\n[FAPI] FAPI2MAC WLS Dequeue block %d error\n",i);
        return FAILURE;
    }

    	ret = WLS_Put1(wls, pa_block, WLS_TEST_MSG_SIZE, WLS_TEST_MSG_ID, (i== (NUM_TEST_MSGS-1))? WLS_TF_FIN:0);
    	printf("\n[FAPI] FAPI2MAC WLS Put1 block %d\n",i);
    }
    return ret;
}
