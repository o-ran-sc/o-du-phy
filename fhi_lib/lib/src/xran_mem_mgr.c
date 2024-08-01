/******************************************************************************
*
*   Copyright (c) 2020 Intel.
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
 * @brief XRAN memory management
 * @file xran_mem_mgr.c
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#define _GNU_SOURCE
#include <sched.h>
#include <assert.h>
#include <err.h>
#include <libgen.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <malloc.h>
#include <immintrin.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_mbuf.h>
#include <rte_ring.h>

#include "xran_ethernet.h"
#include "xran_mem_mgr.h"
#include "xran_dev.h"
#include "xran_printf.h"

#define XRAN_MEM_MAX_DPDK_ALLOC_INFO       ( 40000 * XRAN_MAX_SECTOR_NR )
#define XRAN_MEM_MAX_STRING_DISP_SIZE    ( 24 )
#define XRAN_MEM_MAX_NAME_SIZE           ( 32 )

#define XRAN_MM_DEBUG_PRINT (0)

typedef struct tXRAN_MEM_ALLOC_INFO
{
    char        sVarName[XRAN_MEM_MAX_NAME_SIZE];
    uint8_t     *pBuffer;
    uint32_t    nAllocSize;
} XRAN_MEM_ALLOC_INFO, *PXRAN_MEM_ALLOC_INFO;

typedef struct tXRAN_MEM_LEAK_DETECTOR_HOST
{
    uint32_t                   nIsInit;
    uint32_t                   nNumBufAlloc;
    XRAN_MEM_ALLOC_INFO        sBufAllocInfo[XRAN_MEM_MAX_DPDK_ALLOC_INFO];
} XRAN_MEM_LEAK_DETECTOR_HOST, *PXRAN_MEM_LEAK_DETECTOR_HOST;


uint32_t xran_mem_mgr_leak_detector_add(uint32_t nSize, char *pString, void* pMemBlk);
uint32_t xran_mem_mgr_leak_detector_remove(void* pMemBlk);

static PXRAN_MEM_LEAK_DETECTOR_HOST gpxRANMemLeakDetector = NULL;
static xran_mem_mgr_leak_detector_add_cb_fn gpxRANAddFn = xran_mem_mgr_leak_detector_add;
static xran_mem_mgr_leak_detector_remove_cb_fn gpxRANRemoveFn = xran_mem_mgr_leak_detector_remove;
static int32_t gDpdkProcessIdTag = 0;

int32_t xran_get_dpdk_process_id_tag()
{
    return gDpdkProcessIdTag;
}
void xran_config_dpdk_process_id_tag(int32_t Tag)
{
    gDpdkProcessIdTag = Tag;
}

int32_t xran_mem_mgr_leak_detector_register_cb_fn(xran_mem_mgr_leak_detector_add_cb_fn pxRANAddFn, xran_mem_mgr_leak_detector_remove_cb_fn pxRANRemoveFn)
{
    gpxRANAddFn = xran_mem_mgr_leak_detector_add;
    gpxRANRemoveFn = xran_mem_mgr_leak_detector_remove;

    if (pxRANAddFn && pxRANRemoveFn)
    {
        gpxRANAddFn = pxRANAddFn;
        gpxRANRemoveFn = pxRANRemoveFn;

        return 0;
    }

    return -1;
}

void xran_mem_mgr_leak_detector_clean(PXRAN_MEM_ALLOC_INFO pxRANMemAllocInfo)
{
    strcpy((char*)pxRANMemAllocInfo->sVarName, "");
    pxRANMemAllocInfo->pBuffer    = NULL;
    pxRANMemAllocInfo->nAllocSize = 0;

    return;
}

uint32_t xran_mem_mgr_leak_detector_init(void)
{
    PXRAN_MEM_LEAK_DETECTOR_HOST pxRANMemLeakDetector;
    PXRAN_MEM_ALLOC_INFO pxRANMemAllocInfo;
    uint32_t j;

    if (gpxRANMemLeakDetector == NULL)
    {
        gpxRANMemLeakDetector = (XRAN_MEM_LEAK_DETECTOR_HOST *)malloc(sizeof(XRAN_MEM_LEAK_DETECTOR_HOST));
        if (gpxRANMemLeakDetector)
        {
            pxRANMemLeakDetector = gpxRANMemLeakDetector;
            pxRANMemLeakDetector->nNumBufAlloc = 0;
            pxRANMemAllocInfo = pxRANMemLeakDetector->sBufAllocInfo;

            for (j = 0; j < XRAN_MEM_MAX_DPDK_ALLOC_INFO; j++)
            {
                xran_mem_mgr_leak_detector_clean(&pxRANMemAllocInfo[j]);
            }

            pxRANMemLeakDetector->nIsInit = 1;
        }
        else
        {
            print_err("Cannot Allocate gpxRANMemLeakDetector of size: %lu\n", sizeof(XRAN_MEM_LEAK_DETECTOR_HOST));
            return 1;
        }
    }
    else
    {
        print_err("Error: xran_mem_mgr_leak_detector already initialized!");
    }

    return 0;
}

void xran_mem_mgr_leak_detector_destroy(void)
{
    if (gpxRANMemLeakDetector)
    {
        free(gpxRANMemLeakDetector);
        gpxRANMemLeakDetector = NULL;
    }
    return;
}


void xran_mem_mgr_leak_detector_construct_string(char *pDst, char *pSrc)
{
    uint32_t i, size;

    size = (uint32_t)strlen((char*)pSrc);
    memcpy(pDst, pSrc, size);

    if (size < XRAN_MEM_MAX_STRING_DISP_SIZE-1)
    {
        for (i = size; i < XRAN_MEM_MAX_STRING_DISP_SIZE - 1; i++)
        {
            pDst[i] = ' ';
        }
    }

    pDst[XRAN_MEM_MAX_STRING_DISP_SIZE-1] = '\0';
}

void xran_mem_mgr_leak_detector_construct_num(char *pDst, uint64_t nNumber, uint32_t nStringSize)
{
    uint32_t i, len, max_len, j, extra, num_space;
    char disp[30], dispFinal[30];

#ifndef _WIN32
    //to satisfy Klocworks.   uint64_t and int64_t are defined differently for windows and linux.
    //in linux uint64_t -> unsigned long int
    sprintf(disp, "%lu", nNumber);
#else
    //in windows uint64_t -> unsigned long long int
    sprintf(disp, "%llu", nNumber);
#endif
    len = (uint32_t)strlen(disp);
    max_len = sizeof(dispFinal);

    if (nStringSize == 0)
        nStringSize = 1;
    else if (nStringSize > max_len)
        nStringSize = max_len;

    extra = len % 3;
    if (extra== 0)
    {
        extra = 3;
    }

    for (i = 0, j = 0; i < len; i++)
    {
        dispFinal[j++] = disp[i];
        extra--;
        if (extra == 0)
        {
            dispFinal[j++] = ',';
            extra = 3;
        }
    }

    if(j > 0)
    {
        dispFinal[j-1] = '\0';
    }
    else
    {
        dispFinal[0] = '\0';
    }
    
    // Store in 30 long string for Tabluar format
    len = (uint32_t)strlen(dispFinal);
    num_space = nStringSize - len - 1;
    if (num_space > (max_len - 1))
        num_space = max_len - 1;
    for (i = 0; i < num_space; i++)
    {
        pDst[i] = ' ';
    }
    for (j = 0; i < (nStringSize-1); i++, j++)
    {
        pDst[i] = dispFinal[j];
    }
    pDst[i] = '\0';

    return;
}


uint32_t xran_mem_mgr_leak_detector_display(uint32_t last)
{
    PXRAN_MEM_LEAK_DETECTOR_HOST pxRANMemLeakDetector = gpxRANMemLeakDetector;
    PXRAN_MEM_ALLOC_INFO pxRANMemAllocInfo;

    uint32_t j;
    char pName[64];
    char string1[32], string2[32];
    uint32_t numAlloc = 0;
    uint64_t nTotal = 0;
    uint64_t nStartAddr;
    FILE *pMemLogFile = NULL;

    if (pxRANMemLeakDetector == NULL)
    {
        print_err("xran_mem_mgr_leak_detector_display: pxRANMemLeakDetector is NULL.");
        return 1;
    }
    if (last == 0)
    {
        pMemLogFile = fopen("xran_mem_log_start.txt", "w");
    }
    else if (last == 2)
    {
        pMemLogFile = fopen("xran_mem_log_shutdown.txt", "w");
    }

    pxRANMemAllocInfo = pxRANMemLeakDetector->sBufAllocInfo;

    printf(" ------------------------------------------------------------------------------------\n");
    printf(" xRAN allocated buffer using DPDK: \n");
    printf(" ------------------------------------------------------------------------------------\n");
    printf("           String Name                 Start              Size(Bytes)\n");
    printf(" ------------------------------------------------------------------------------------\n");
    if (pMemLogFile)
    {
        fprintf(pMemLogFile, " ------------------------------------------------------------------------------------\n");
        fprintf(pMemLogFile, "           String Name                 Start              Size(Bytes)\n");
        fprintf(pMemLogFile, " ------------------------------------------------------------------------------------\n");
    }

    for (j = 0; j < XRAN_MEM_MAX_DPDK_ALLOC_INFO; j++)
    {
        if (pxRANMemAllocInfo[j].pBuffer)
        {
            xran_mem_mgr_leak_detector_construct_string(pName, pxRANMemAllocInfo[j].sVarName);
            xran_mem_mgr_leak_detector_construct_num(string1, pxRANMemAllocInfo[j].nAllocSize, 15);
            nStartAddr = (uint64_t)pxRANMemAllocInfo[j].pBuffer;
            numAlloc++;
            nTotal += pxRANMemAllocInfo[j].nAllocSize;

#ifdef PRINTF_INFO_LOG_OK
            printf(" %7d  %s (%016lx ) %s\n", numAlloc, pName, nStartAddr, string1);
#endif
            if (pMemLogFile)
                fprintf(pMemLogFile, " %7d  %s (%016lx ) %s\n", numAlloc, pName, nStartAddr, string1);
        }
    }

    xran_mem_mgr_leak_detector_construct_num(string1, numAlloc, 10);
    xran_mem_mgr_leak_detector_construct_num(string2, nTotal, 15);
    printf(" ------------------------------------------------------------------------------------\n");
    printf("                     Buffers Alloc: %s /         Size Alloc: %s\n", string1, string2);
    printf(" ------------------------------------------------------------------------------------\n\n\n");
    if (pMemLogFile)
    {
        fprintf(pMemLogFile, " ------------------------------------------------------------------------------------\n");
        fprintf(pMemLogFile, "                     Buffers Alloc: %s /         Size Alloc: %s\n", string1, string2);
        fprintf(pMemLogFile, " ------------------------------------------------------------------------------------\n\n\n");
        fclose(pMemLogFile);
    }

    return 0;
}



uint32_t xran_mem_mgr_leak_detector_add(uint32_t nSize, char *pString, void* pMemBlk)
{
    PXRAN_MEM_LEAK_DETECTOR_HOST pxRANMemLeakDetector = gpxRANMemLeakDetector;
    PXRAN_MEM_ALLOC_INFO pxRANMemAllocInfo;
    uint32_t j, allocated;

    if (pMemBlk == NULL) return 1;

    if (pxRANMemLeakDetector != NULL)
    {
        if (pxRANMemLeakDetector->nIsInit == 0)
        {
            printf("xran_mem_mgr_leak_detector_add: not initialized!\n");
            return 1;
        }
        pxRANMemAllocInfo = pxRANMemLeakDetector->sBufAllocInfo;

        if (pxRANMemAllocInfo == NULL)
        {
            rte_panic("xran_mem_mgr_leak_detector_add: pxRANMemAllocInfo is NULL %u %s %p\n", nSize, pString, pMemBlk);
        }
        else
        {
            allocated = 0;
            for (j = 0; j < XRAN_MEM_MAX_DPDK_ALLOC_INFO; j++)
            {
                if (pxRANMemAllocInfo[j].pBuffer == NULL)
                {
                    if (pString == NULL)
                    {
                        sprintf((char*)pxRANMemAllocInfo[j].sVarName, "Mem%u", pxRANMemLeakDetector->nNumBufAlloc);
                    }
                    else
                    {
                        strncpy((char*)pxRANMemAllocInfo[j].sVarName, (char*)pString, XRAN_MEM_MAX_NAME_SIZE-1);
                        pxRANMemAllocInfo[j].sVarName[XRAN_MEM_MAX_NAME_SIZE-1] = '\0';
                    }
                    pxRANMemAllocInfo[j].pBuffer    = (uint8_t*)pMemBlk;
                    pxRANMemAllocInfo[j].nAllocSize = nSize;
                    pxRANMemLeakDetector->nNumBufAlloc++;
                    allocated = 1;
                    break;
                }
            }
            if (allocated == 0)
            {
                xran_mem_mgr_leak_detector_display(1);
                rte_panic("xran_mem_mgr_leak_detector_add: LeakDetector buffers full\n");
            }
        }

    }
    else
    {
        rte_panic("xran_mem_mgr_leak_detector_add: pHeapLeakDetector is NULL\n");
    }
    return 0;
}

uint32_t xran_mem_mgr_leak_detector_remove(void* pMemBlk)
{
    PXRAN_MEM_LEAK_DETECTOR_HOST pxRANMemLeakDetector = gpxRANMemLeakDetector;
    PXRAN_MEM_ALLOC_INFO pxRANMemAllocInfo;
    uint32_t j, freed;

    if (pxRANMemLeakDetector != NULL)
    {
        if (pxRANMemLeakDetector->nIsInit == 0)
        {
            printf("xran_mem_mgr_leak_detector_remove: not initialized!\n");
            return 1;
        }
        pxRANMemAllocInfo = pxRANMemLeakDetector->sBufAllocInfo;

        if (pxRANMemAllocInfo == NULL)
        {
            rte_panic("mem_mgr_leak_detector_remove: pHeapAllocInfo is NULL %p\n", pMemBlk);
        }
        else
        {
            freed = 0;
            for (j = 0; j < XRAN_MEM_MAX_DPDK_ALLOC_INFO; j++)
            {
                if (pxRANMemAllocInfo[j].pBuffer == pMemBlk)
                {
                    xran_mem_mgr_leak_detector_clean(&pxRANMemAllocInfo[j]);
                    pxRANMemLeakDetector->nNumBufAlloc--;
                    freed = 1;
                    break;
                }
            }
            if (freed == 0)
            {
                printf("rte_mem_mgr_leak_detector_remove: Buffer was not added %p\n", pMemBlk);
                printf("nNumBufAlloc in this block: %u\n", pxRANMemLeakDetector->nNumBufAlloc);
                xran_mem_mgr_leak_detector_display(1);
                rte_panic("Exitting\n");
            }
        }
    }
    else
    {
        rte_panic("xran_mem_mgr_leak_detector_remove: pHeapLeakDetector is NULL %p\n", pMemBlk);
   }

    return 0;
}


int32_t xran_mm_init(void *pHandle, uint64_t nMemorySize, uint32_t nMemorySegmentSize)
{
    /* we use mbuf from dpdk memory */
    return 0;
}

int32_t xran_bm_init (void *pHandle, uint32_t * pPoolIndex, uint32_t nNumberOfBuffers, uint32_t nBufferSize)
{
    uint32_t nDpdkProcessID = xran_get_dpdk_process_id_tag();
    XranSectorHandleInfo *pXranCc;
    uint32_t nAllocBufferSize;
    char pool_name[RTE_MEMPOOL_NAMESIZE];

    if(pHandle)
        pXranCc = (XranSectorHandleInfo*)pHandle;
    else
        return (-1);

    if(nNumberOfBuffers == 280)
        nNumberOfBuffers = 560;
#if (XRAN_MM_DEBUG_PRINT)
    printf("nNumberOfBuffers=%u\n", nNumberOfBuffers);
#endif

    snprintf(pool_name, RTE_MEMPOOL_NAMESIZE, "ru_%d_cc_%d_idx_%d_%d",
        pXranCc->nXranPort, pXranCc->nIndex, pXranCc->nBufferPoolIndex, nDpdkProcessID);

    nAllocBufferSize = nBufferSize + sizeof(struct rte_ether_hdr) +
        sizeof (struct xran_ecpri_hdr) +
        sizeof (struct radio_app_common_hdr) +
        sizeof(struct data_section_hdr) + 256;

    if(nAllocBufferSize >= UINT16_MAX)
    {
        rte_panic("nAllocBufferSize is failed [ handle %p %d %d ] [nPoolIndex %d] nNumberOfBuffers %d nBufferSize %d nAllocBufferSize %d\n",
                    pXranCc, pXranCc->nXranPort, pXranCc->nIndex, pXranCc->nBufferPoolIndex, nNumberOfBuffers, nBufferSize, nAllocBufferSize);
        return -1;
    }

#if (XRAN_MM_DEBUG_PRINT)
    printf("%s: [ handle %p %d %d ] [nPoolIndex %d] nNumberOfBuffers %d nBufferSize %d socket_id %d\n", pool_name,
                        pXranCc, pXranCc->nXranPort, pXranCc->nIndex, pXranCc->nBufferPoolIndex, nNumberOfBuffers, nBufferSize, rte_socket_id());
#endif
    pXranCc->p_bufferPool[pXranCc->nBufferPoolIndex] = xran_pktmbuf_pool_create(pool_name, nNumberOfBuffers,
                                                                               /*MBUF_CACHE*/0, 0, nAllocBufferSize, rte_socket_id());


    if(pXranCc->p_bufferPool[pXranCc->nBufferPoolIndex] == NULL)
    {
        rte_panic("rte_pktmbuf_pool_create failed [poolName=%s, handle %p %d %d ] [nPoolIndex %d] nNumberOfBuffers %d nBufferSize %d errno %s\n",
                    pool_name, pXranCc, pXranCc->nXranPort, pXranCc->nIndex, pXranCc->nBufferPoolIndex, nNumberOfBuffers, nBufferSize, rte_strerror(rte_errno));
        return -1;
    }

    pXranCc->bufferPoolElmSz[pXranCc->nBufferPoolIndex]  = nBufferSize;
    pXranCc->bufferPoolNumElm[pXranCc->nBufferPoolIndex] = nNumberOfBuffers;

#if (XRAN_MM_DEBUG_PRINT)
    printf("CC:[ handle %p ru %d cc_idx %d ] [nPoolIndex %d] mb pool %p \n",
                pXranCc, pXranCc->nXranPort, pXranCc->nIndex,
                pXranCc->nBufferPoolIndex,  pXranCc->p_bufferPool[pXranCc->nBufferPoolIndex]);
#endif
    *pPoolIndex = pXranCc->nBufferPoolIndex++;

    return 0;
}

int32_t xran_bm_release(void *pHandle, uint32_t *pPoolIndex __attribute__((unused)))
{
    XranSectorHandleInfo *pXranCc;
    struct rte_mempool *mp;
    int numPools;
    int i;

    if(pHandle)
        pXranCc = (XranSectorHandleInfo*)pHandle;
    else
        return (-1);

    numPools = pXranCc->nBufferPoolIndex;
#if (XRAN_MM_DEBUG_PRINT)
    printf("Total number of pools = %d\n", numPools);
#endif

    for(i=0; i < numPools; i++)
    {
        mp = pXranCc->p_bufferPool[i];
        if(mp)
        {
#if (XRAN_MM_DEBUG_PRINT)
            printf("Port%d CC%d %d:[%s] socket=%d, size=%d, elt_size=%d, populated_size=%d, nb_mem_chunks=%d\n",
                    pXranCc->nXranPort, pXranCc->nIndex, i,
                    mp->name, mp->socket_id, mp->size, mp->elt_size, mp->populated_size, mp->nb_mem_chunks);
#endif
            gpxRANRemoveFn(mp);
            rte_mempool_free(mp);

            pXranCc->p_bufferPool[i]        = NULL;
            pXranCc->bufferPoolElmSz[i]     = 0;
            pXranCc->bufferPoolNumElm[i]    = 0;
        }
        else continue;
    }

    pXranCc->nBufferPoolIndex = 0;

    return 0;
}

int32_t xran_bm_allocate_buffer(void *pHandle, uint32_t nPoolIndex, void **ppData,  void **ppCtrl)
{
    XranSectorHandleInfo* pXranCc;
    *ppData = NULL;
    *ppCtrl = NULL;

    if(pHandle)
        pXranCc = (XranSectorHandleInfo*)pHandle;
    else
        return (-1);

    struct rte_mbuf * mb =  rte_pktmbuf_alloc(pXranCc->p_bufferPool[nPoolIndex]);

    if(mb)
    {
        char *start     = rte_pktmbuf_append(mb, pXranCc->bufferPoolElmSz[nPoolIndex]);
        char *ethhdr    = rte_pktmbuf_prepend(mb, sizeof(struct rte_ether_hdr));

        if(start && ethhdr)
        {
            char * iq_offset = rte_pktmbuf_mtod(mb, char * );
            /* skip headers */
            iq_offset = iq_offset + sizeof(struct rte_ether_hdr) +
                                    sizeof (struct xran_ecpri_hdr) +
                                    sizeof (struct radio_app_common_hdr) +
                                    sizeof(struct data_section_hdr);

            //if (0) /* if compression */
            //    iq_offset += sizeof (struct data_section_compression_hdr);

            *ppData = (void *)iq_offset;
            *ppCtrl = (void *)mb;
        }
        else
        {
            print_err("[nPoolIndex %d] start ethhdr failed \n", nPoolIndex );
            return -1;
        }
    }
    else
    {
        print_err("[nPoolIndex %d] mb alloc failed \n", nPoolIndex );
        return -1;
    }

    if (*ppData ==  NULL){
        print_err("[nPoolIndex %d] rte_pktmbuf_append for %d failed \n", nPoolIndex, pXranCc->bufferPoolElmSz[nPoolIndex]);
        return -1;
    }

    return 0;
}

int32_t xran_bm_free_buffer(void *pHandle __attribute__((unused)), void *pData __attribute__((unused)), void *pCtrl)
{
    //XranSectorHandleInfo* pXranCc = (XranSectorHandleInfo*) pHandle;

    if(pCtrl)
        rte_pktmbuf_free(pCtrl);

    return 0;
}

int32_t xran_bm_allocate_ring(void *pHandle, const char *rng_name_prefix, uint16_t cc_id, uint16_t buff_id, uint16_t ant_id, uint16_t symb_id, void **ppRing)
{
    int32_t ret = 0;
    XranSectorHandleInfo *pXranCc;
    uint32_t xran_port_id;
    char ring_name[32]    = "";
    struct rte_ring *ring =  NULL;
    ssize_t r_size;

    if(pHandle)
    {
        pXranCc         = (XranSectorHandleInfo*) pHandle;
        xran_port_id    = pXranCc->nXranPort;
        *ppRing         = NULL;

        r_size = rte_ring_get_memsize(XRAN_MAX_MEM_IF_RING_SIZE);
        snprintf(ring_name, RTE_DIM(ring_name), "%srb%dp%dcc%dant%dsym%d", rng_name_prefix, buff_id, xran_port_id, cc_id, ant_id, symb_id);
        print_dbg("%s\n", ring_name);
        ring = (struct rte_ring *)xran_malloc(ring_name, r_size, 64);
        if(ring ==  NULL)
        {
            print_err("[%srb%dp%dcc%dant%dsym%d] ring alloc failed \n", rng_name_prefix, buff_id, xran_port_id, cc_id, ant_id, symb_id);
            return -1;
        }

        ret = rte_ring_init(ring, ring_name, XRAN_MAX_MEM_IF_RING_SIZE, /*RING_F_SC_DEQ*/0);
        if(ret != 0)
        {
            print_err("[%srb%dp%dcc%dant%dsym%d] rte_ring_init failed \n", rng_name_prefix, buff_id, xran_port_id, cc_id, ant_id, symb_id);
            return -1;
        }

        *ppRing  = (void *)ring;
    }
    else
    {
        print_err("pHandle failed \n");
        return -1;
    }

    return 0;
}

int32_t xran_bm_free_ring(void *pHandle, void *pRing)
{

    if(pHandle && pRing)
    {
        struct rte_ring *ring = (struct rte_ring *)pRing;
//        {
//        XranSectorHandleInfo *pXranCc;
//        pXranCc = (XranSectorHandleInfo *)pHandle;
//        printf("Port%d CC%d [%s] size=%d, capacity=%d\n",
//                    pXranCc->nXranPort, pXranCc->nIndex, ring->name, ring->size, ring->capacity);
//        }
        xran_free(ring);

/*  //for some reason the xran_bm_allocate_ring() did not use rte_ring_create to create ring, thus here we should not use rte_ring_free to free it
        gpxRANRemoveFn(ring);
        rte_ring_free(ring);*/
    }
    else
    {
        return (-1);
    }

    return (0);
}

void* xran_malloc(char *name, size_t buf_len, uint32_t align)
{
    void *buf = rte_malloc("External buffer", buf_len, align);
    gpxRANAddFn(buf_len, name, buf);
    return buf;
}
void* xran_zmalloc(char *name, size_t buf_len, uint32_t align)
{
    void *buf = rte_zmalloc("External buffer", buf_len, align);
    gpxRANAddFn(buf_len, name, buf);
    return buf;
}

void xran_free(void *addr)
{
    gpxRANRemoveFn(addr);
    return rte_free(addr);
}
struct rte_mempool * xran_pktmbuf_pool_create(const char *name, unsigned int n,
	unsigned int cache_size, uint16_t priv_size, uint16_t data_room_size,
	int socket_id)
{
    struct rte_mempool *buf = rte_pktmbuf_pool_create(name, n,	cache_size, priv_size, data_room_size, socket_id);
    gpxRANAddFn(n * data_room_size, (char *)name, (void*)buf);
    return buf;
}

int32_t xran_mm_destroy (void * pHandle)
{
    if(xran_get_if_state() == XRAN_RUNNING)
    {
        print_err("Please STOP first !!");
        return (-1);
    }

    /* functionality is not yet implemented */
    return 0;
}
