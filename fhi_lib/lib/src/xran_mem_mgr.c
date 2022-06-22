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

#include "ethernet.h"
#include "xran_mem_mgr.h"
#include "xran_dev.h"
#include "xran_printf.h"

int32_t
xran_mm_init (void * pHandle, uint64_t nMemorySize,
            uint32_t nMemorySegmentSize)
{
    /* we use mbuf from dpdk memory */
    return 0;
}

int32_t
xran_bm_init (void * pHandle, uint32_t * pPoolIndex, uint32_t nNumberOfBuffers, uint32_t nBufferSize)
{
    //printf("nNumberOfBuffers=%u\n", nNumberOfBuffers);
    if(nNumberOfBuffers == 280)
        nNumberOfBuffers = 560;

    XranSectorHandleInfo* pXranCc = (XranSectorHandleInfo*) pHandle;
    uint32_t nAllocBufferSize;

    char pool_name[RTE_MEMPOOL_NAMESIZE];

    snprintf(pool_name, RTE_MEMPOOL_NAMESIZE, "ru_%d_cc_%d_idx_%d",
        pXranCc->nXranPort, pXranCc->nIndex, pXranCc->nBufferPoolIndex);

    nAllocBufferSize = nBufferSize + sizeof(struct rte_ether_hdr) +
        sizeof (struct xran_ecpri_hdr) +
        sizeof (struct radio_app_common_hdr) +
        sizeof(struct data_section_hdr) + 256;

    if(nAllocBufferSize >= UINT16_MAX) {
        rte_panic("nAllocBufferSize is failed [ handle %p %d %d ] [nPoolIndex %d] nNumberOfBuffers %d nBufferSize %d nAllocBufferSize %d\n",
                    pXranCc, pXranCc->nXranPort, pXranCc->nIndex, pXranCc->nBufferPoolIndex, nNumberOfBuffers, nBufferSize, nAllocBufferSize);
        return -1;
    }

    printf("%s: [ handle %p %d %d ] [nPoolIndex %d] nNumberOfBuffers %d nBufferSize %d socket_id %d\n", pool_name,
                        pXranCc, pXranCc->nXranPort, pXranCc->nIndex, pXranCc->nBufferPoolIndex, nNumberOfBuffers, nBufferSize, rte_socket_id());

    pXranCc->p_bufferPool[pXranCc->nBufferPoolIndex] = rte_pktmbuf_pool_create(pool_name, nNumberOfBuffers,
                                                                               /*MBUF_CACHE*/0, 0, nAllocBufferSize, rte_socket_id());


    if(pXranCc->p_bufferPool[pXranCc->nBufferPoolIndex] == NULL){
        rte_panic("rte_pktmbuf_pool_create failed [poolName=%s, handle %p %d %d ] [nPoolIndex %d] nNumberOfBuffers %d nBufferSize %d errno %s\n",
                    pool_name, pXranCc, pXranCc->nXranPort, pXranCc->nIndex, pXranCc->nBufferPoolIndex, nNumberOfBuffers, nBufferSize, rte_strerror(rte_errno));
        return -1;
    }
    //printf("press enter (RTE_MEMPOOL_NAMESIZE=%u)\n", RTE_MEMPOOL_NAMESIZE);
    //getchar();
    pXranCc->bufferPoolElmSz[pXranCc->nBufferPoolIndex]  = nBufferSize;
    pXranCc->bufferPoolNumElm[pXranCc->nBufferPoolIndex] = nNumberOfBuffers;

    printf("CC:[ handle %p ru %d cc_idx %d ] [nPoolIndex %d] mb pool %p \n",
                pXranCc, pXranCc->nXranPort, pXranCc->nIndex,
                    pXranCc->nBufferPoolIndex,  pXranCc->p_bufferPool[pXranCc->nBufferPoolIndex]);

    *pPoolIndex = pXranCc->nBufferPoolIndex++;

    return 0;
}

int32_t
xran_bm_allocate_buffer(void * pHandle, uint32_t nPoolIndex, void **ppData,  void **ppCtrl)
{
    XranSectorHandleInfo* pXranCc = (XranSectorHandleInfo*) pHandle;
    *ppData = NULL;
    *ppCtrl = NULL;

    struct rte_mbuf * mb =  rte_pktmbuf_alloc(pXranCc->p_bufferPool[nPoolIndex]);

    if(mb){
        char * start     = rte_pktmbuf_append(mb, pXranCc->bufferPoolElmSz[nPoolIndex]);
        char * ethhdr    = rte_pktmbuf_prepend(mb, sizeof(struct rte_ether_hdr));

        if(start && ethhdr){
            char * iq_offset = rte_pktmbuf_mtod(mb, char * );
            /* skip headers */
            iq_offset = iq_offset + sizeof(struct rte_ether_hdr) +
                                    sizeof (struct xran_ecpri_hdr) +
                                    sizeof (struct radio_app_common_hdr) +
                                    sizeof(struct data_section_hdr);

            if (0) /* if compression */
                iq_offset += sizeof (struct data_section_compression_hdr);

            *ppData = (void *)iq_offset;
            *ppCtrl  = (void *)mb;
        } else {
            print_err("[nPoolIndex %d] start ethhdr failed \n", nPoolIndex );
            return -1;
        }
    } else {
        print_err("[nPoolIndex %d] mb alloc failed \n", nPoolIndex );
        return -1;
    }

    if (*ppData ==  NULL){
        print_err("[nPoolIndex %d] rte_pktmbuf_append for %d failed \n", nPoolIndex, pXranCc->bufferPoolElmSz[nPoolIndex]);
        return -1;
    }

    return 0;
}

int32_t
xran_bm_allocate_ring(void * pHandle, const char *rng_name_prefix, uint16_t cc_id, uint16_t buff_id, uint16_t ant_id, uint16_t symb_id, void **ppRing)
{
    int32_t ret = 0;
    XranSectorHandleInfo* pXranCc = (XranSectorHandleInfo*) pHandle;
    uint32_t xran_port_id;
    char ring_name[32]    = "";
    struct rte_ring *ring =  NULL;
    ssize_t r_size;

    if(pHandle){
        xran_port_id = pXranCc->nXranPort;
        *ppRing = NULL;
        snprintf(ring_name, RTE_DIM(ring_name), "%srb%dp%dcc%dant%dsym%d", rng_name_prefix, buff_id, xran_port_id, cc_id, ant_id, symb_id);
        print_dbg("%s\n", ring_name);
        r_size = rte_ring_get_memsize(XRAN_MAX_MEM_IF_RING_SIZE);
        ring = (struct rte_ring *)xran_malloc(r_size);
        if(ring ==  NULL) {
            print_err("[%srb%dp%dcc%dant%dsym%d] ring alloc failed \n", rng_name_prefix, buff_id, xran_port_id, cc_id, ant_id, symb_id);
            return -1;
        }
        ret = rte_ring_init(ring, ring_name, XRAN_MAX_MEM_IF_RING_SIZE, /*RING_F_SC_DEQ*/0);
        if(ret != 0){
            print_err("[%srb%dp%dcc%dant%dsym%d] rte_ring_init failed \n", rng_name_prefix, buff_id, xran_port_id, cc_id, ant_id, symb_id);
            return -1;
        }

        if(ring) {
            *ppRing  = (void *)ring;
        }else {
            print_err("[%srb%dp%dcc%dant%dsym%d] ring alloc failed \n", rng_name_prefix, buff_id, xran_port_id, cc_id, ant_id, symb_id);
            return -1;
        }
    } else {
        print_err("pHandle failed \n");
        return -1;
    }

    return 0;
}

int32_t
xran_bm_free_buffer(void * pHandle, void *pData, void *pCtrl)
{
    //XranSectorHandleInfo* pXranCc = (XranSectorHandleInfo*) pHandle;

    if(pCtrl)
        rte_pktmbuf_free(pCtrl);

    return 0;
}

void*
xran_malloc(size_t buf_len)
{
    return rte_malloc("External buffer", buf_len, RTE_CACHE_LINE_SIZE);
}

void
xran_free(void *addr)
{
    return rte_free(addr);
}

int32_t
xran_mm_destroy (void * pHandle)
{
    if(xran_get_if_state() == XRAN_RUNNING) {
        print_err("Please STOP first !!");
        return (-1);
        }

    /* functionality is not yet implemented */
    return 0;
}
