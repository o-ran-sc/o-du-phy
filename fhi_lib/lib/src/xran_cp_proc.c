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
 * @brief XRAN C plane processing functionality and helper functions
 * @file xran_cp_proc.c
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
#include <rte_malloc.h>
#include <rte_ethdev.h>

#include "xran_fh_o_du.h"

#include "xran_ethdi.h"
#include "xran_pkt.h"
#include "xran_up_api.h"
#include "xran_cp_api.h"
#include "xran_sync_api.h"
#include "xran_lib_mlog_tasks_id.h"
#include "xran_timer.h"
#include "xran_common.h"
#include "xran_dev.h"
#include "xran_frame_struct.h"
#include "xran_printf.h"
#include "xran_cp_proc.h"
#include "xran_tx_proc.h"

#include "xran_main.h"
#include "xran_mlog_lnx.h"

uint8_t xran_cp_seq_id_num[XRAN_PORTS_NUM][XRAN_MAX_CELLS_PER_PORT][XRAN_DIR_MAX][XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR]; /* XRAN_MAX_ANTENNA_NR * 2 for PUSCH and PRACH */
rte_atomic16_t xran_updl_seq_id_num[XRAN_PORTS_NUM][XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_CSIRS_PORTS];
rte_atomic16_t xran_upul_seq_id_num[XRAN_PORTS_NUM][XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR]; /**< PUSCH, PRACH, SRS for Cat B */
uint8_t xran_section_id_curslot[XRAN_PORTS_NUM][XRAN_DIR_MAX][XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR * 2+ XRAN_MAX_ANT_ARRAY_ELM_NR];
uint16_t xran_section_id[XRAN_PORTS_NUM][XRAN_DIR_MAX][XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR * 2+ XRAN_MAX_ANT_ARRAY_ELM_NR];

struct xran_recv_packet_info parse_recv[XRAN_PORTS_NUM];

//////////////////////////////////////////
// For RU emulation
struct xran_section_recv_info *recvSections[XRAN_PORTS_NUM] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};
struct xran_cp_recv_params recvCpInfo[XRAN_PORTS_NUM];

extern int32_t first_call;
extern uint32_t xran_lib_ota_sym_idx_mu[];

/* TO DO: __thread is slow. We should allocate global 2D array and index it using current core index
 * for better performance.
 */
__thread struct xran_section_gen_info sect_geninfo[XRAN_MAX_SECTIONS_PER_SLOT];

extern __rte_always_inline uint8_t xran_get_upul_seqid(int8_t ru_id, uint8_t cc_id, uint8_t ant_id);
extern __rte_always_inline uint8_t xran_read_upul_seqid(int8_t ru_id, uint8_t cc_id, uint8_t ant_id);
extern __rte_always_inline void xran_set_upul_seqid(int8_t ru_id, uint8_t cc_id, uint8_t ant_id, uint8_t seq);
extern __rte_always_inline uint8_t xran_get_updl_seqid(int8_t ru_id, uint8_t cc_id, uint8_t ant_id);
extern __rte_always_inline uint8_t xran_read_updl_seqid(int8_t ru_id, uint8_t cc_id, uint8_t ant_id);
extern __rte_always_inline void xran_set_updl_seqid(int8_t ru_id, uint8_t cc_id, uint8_t ant_id, uint8_t seq);
extern __rte_always_inline int8_t xran_check_updl_seqid(int8_t xran_port, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id, uint8_t seq_id);
extern __rte_always_inline int8_t xran_check_upul_seqid(int8_t xran_port, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id, uint8_t seq_id);

static void
extbuf_free_callback(void *addr __rte_unused, void *opaque __rte_unused)
{
    /*long t1 = MLogTick();
    MLogTask(77777, t1, t1+100);*/
}

int32_t
xran_init_sectionid(void *pHandle)
{
    int cell, ant, dir;
    struct xran_device_ctx* p_dev = NULL;
    uint8_t xran_port_id = 0;

    if(pHandle) {
        p_dev = (struct xran_device_ctx* )pHandle;
        xran_port_id = p_dev->xran_port_id;
    } else {
        print_err("Invalid pHandle - %p", pHandle);
        return (-1);
    }

    for (dir = 0; dir < XRAN_DIR_MAX; dir++){
        for(cell=0; cell < XRAN_MAX_CELLS_PER_PORT; cell++) {
            for(ant=0; ant < XRAN_MAX_ANTENNA_NR; ant++) {
                xran_section_id[xran_port_id][dir][cell][ant] = 0;
                xran_section_id_curslot[xran_port_id][dir][cell][ant] = 255;
            }
        }
    }

    return (0);
}

int32_t xran_init_seqid(void *pHandle)
{
    int cell, dir, ant;
    int8_t xran_port = 0;

    if((xran_port =  xran_dev_ctx_get_port_id(pHandle)) < 0 )
    {
        print_err("Invalid pHandle - %p", pHandle);
        return (0);
    }

    for(cell=0; cell < XRAN_MAX_CELLS_PER_PORT; cell++)
    {
        for(dir=0; dir < XRAN_DIR_MAX; dir++)
        {
            for(ant=0; ant < XRAN_MAX_ANTENNA_NR * 2; ant++)
                xran_cp_seq_id_num[xran_port][cell][dir][ant] = 0;
        }
        for(ant=0; ant < XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_CSIRS_PORTS; ant++)
        {
            rte_atomic16_init(&xran_updl_seq_id_num[xran_port][cell][ant]);
        }
        for(ant=0; ant < XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR; ant++)
        {
            rte_atomic16_init(&xran_upul_seq_id_num[xran_port][cell][ant]);
        }
    }

    return (0);
}

int32_t
process_cplane(struct rte_mbuf *pkt, void* handle)
{
    uint32_t mb_free = MBUF_FREE;
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)handle;
    int32_t ret = XRAN_STATUS_SUCCESS;

    if(p_xran_dev_ctx && xran_if_current_state == XRAN_RUNNING) {
        if(xran_dev_get_ctx_by_id(0)->fh_cfg.debugStop) /* check CP with standard tests only */
            ret = xran_parse_cp_pkt(pkt, &recvCpInfo[p_xran_dev_ctx->xran_port_id],
                    &parse_recv[p_xran_dev_ctx->xran_port_id], (void*)p_xran_dev_ctx, &mb_free);
        if(unlikely(ret != XRAN_STATUS_SUCCESS)){
            print_dbg("Invalid C-plane packet\n");
            ++p_xran_dev_ctx->fh_counters.rx_err_cp;
            ++p_xran_dev_ctx->fh_counters.rx_err_drop;
        }
    }
    return (mb_free);
}

int32_t
xran_check_symbolrange(int symbol_type, uint32_t xranPortId, int ccId, int tti,
                        int start_sym, int numsym_in, int *numsym_out, uint8_t mu)
{
    int i;
    int first_pos, last_pos;
    int start_pos, end_pos;

    first_pos = last_pos = -1;

    /* Find first symbol which is same with given symbol type */
    for(i=0; i < XRAN_NUM_OF_SYMBOL_PER_SLOT; i++)
        if(xran_fs_get_symbol_type(xranPortId, ccId, tti, i, mu) == symbol_type) {
            first_pos = i; break;
            }

    if(first_pos < 0) {
//        for(i=0; i < XRAN_NUM_OF_SYMBOL_PER_SLOT; i++)
//            printf("symbol_type %d - %d:%d\n", symbol_type, i, xran_fs_get_symbol_type(ccId, tti, i));
        *numsym_out = 0;
        return (first_pos);
        }

    /* Find the rest of consecutive symbols which are same with given symbol type */
    for( ; i < XRAN_NUM_OF_SYMBOL_PER_SLOT; i++)
        if(xran_fs_get_symbol_type(xranPortId, ccId, tti, i, mu) != symbol_type)
            break;
    last_pos = i;

    start_pos = (first_pos > start_sym) ?  first_pos : start_sym;
    end_pos = ((start_sym + numsym_in) > last_pos) ? last_pos : (start_sym + numsym_in);
    *numsym_out = end_pos - start_pos;

    return (start_pos);
}

struct rte_mbuf *
xran_attach_cp_ext_buf(int8_t* p_ext_buff_start/*ext_start*/, int8_t* p_ext_buff/*ext-section*/, uint16_t ext_buff_len,
                struct rte_mbuf_ext_shared_info * p_share_data)
{
    struct rte_mbuf *mb_oran_hdr_ext = NULL;
    //struct rte_mbuf *tmp             = NULL;
    int8_t          *ext_buff        = NULL;
    rte_iova_t ext_buff_iova         = 0;

    ext_buff  = p_ext_buff - (RTE_PKTMBUF_HEADROOM +
                sizeof(struct xran_ecpri_hdr) +
                sizeof(struct xran_cp_radioapp_section1_header));

    ext_buff_len += (RTE_PKTMBUF_HEADROOM +
                sizeof(struct xran_ecpri_hdr) +
                sizeof(struct xran_cp_radioapp_section1_header) + 18);

//    mb_oran_hdr_ext =  rte_pktmbuf_alloc(_eth_mbuf_pool_small);
    mb_oran_hdr_ext = xran_ethdi_mbuf_indir_alloc();

    if (unlikely (( mb_oran_hdr_ext) == NULL)) {
        rte_panic("Failed rte_pktmbuf_alloc\n");
    }

    p_share_data->free_cb = extbuf_free_callback;
    p_share_data->fcb_opaque = NULL;
    rte_mbuf_ext_refcnt_set(p_share_data, 1);

    ext_buff_iova = rte_malloc_virt2iova(p_ext_buff_start);
    if (unlikely (( ext_buff_iova) == 0)) {
        rte_panic("Failed rte_mem_virt2iova \n");
    }

    if (unlikely (( (rte_iova_t)ext_buff_iova) == RTE_BAD_IOVA)) {
        rte_panic("Failed rte_mem_virt2iova RTE_BAD_IOVA \n");
    }

    rte_pktmbuf_attach_extbuf(mb_oran_hdr_ext,
                              ext_buff,
                              ext_buff_iova + RTE_PTR_DIFF(ext_buff , p_ext_buff_start),
                              ext_buff_len,
                              p_share_data);

    rte_pktmbuf_reset_headroom(mb_oran_hdr_ext);

    return mb_oran_hdr_ext;
}
/** tti assumed here to be between [0 - 10240 * (1 << mu) -1 ]  where 1024 is number of
 * radio frames hence for full range of TTI with in radio frames */
static inline void get_f_sf_slot(uint8_t mu, uint32_t tti, bool isNb375, uint32_t *frameId, uint32_t *sfId, uint32_t *slotId,uint8_t bbuoffload)
{
    uint32_t interval = xran_fs_get_tti_interval(mu);

    if (bbuoffload)
    {
        // ORAN frameId, 8 bits, [0, 255]
        *frameId    =  (tti / SLOTS_PER_SYSTEMFRAME(interval)) & 0xFF;
    }
    else
    {
        *frameId    = XranGetFrameNum(tti, xran_getSfnSecStart(), SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval));
        *frameId = ((*frameId + ((0 == tti)?NUM_OF_FRAMES_PER_SECOND:0)) & 0xff); /* ORAN frameId, 8 bits, [0, 255] */
    }

    *sfId = XranGetSubFrameNum(tti, SLOTNUM_PER_SUBFRAME(interval), SUBFRAMES_PER_SYSTEMFRAME);

    *slotId     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME(interval));
    if(isNb375)
        *slotId = 0;

} /* get_f_sf_slot */

static inline int get_prb_elm_to_process(
    struct xran_device_ctx *pDevCtx,
    struct xran_prb_map *prbMap,
    struct xran_prb_elm_proc_info_t *prbElmProcInfo,
    xran_pkt_dir dir,
    uint32_t * numPrbElm,
    uint32_t *i
    )
{
    *numPrbElm = prbMap->nPrbElm;
    *i = 0;
    if (XRAN_DIR_DL == dir && 0 == pDevCtx->dlCpProcBurst && pDevCtx->DynamicSectionEna == 0)
    {
        if (0 == pDevCtx->numSymsForDlCP)
        {
            print_dbg("No symbol available for DL CP transmission\n");
            return (-1);
        }
        if (prbMap->nPrbElm == prbElmProcInfo->nPrbElmProcessed && 0 != prbElmProcInfo->numSymsRemaining)
        {
            prbElmProcInfo->numSymsRemaining--;
            print_dbg("All sections already processed\n");
            return (-1);
        }
        if (0 == prbElmProcInfo->numSymsRemaining)
        { /* new slot */
            prbElmProcInfo->numSymsRemaining = pDevCtx->numSymsForDlCP;
            prbElmProcInfo->nPrbElmPerSym = prbMap->nPrbElm / pDevCtx->numSymsForDlCP;
            prbElmProcInfo->nPrbElmProcessed = 0;
        }
        if (1 == prbElmProcInfo->numSymsRemaining)
        { /* last symbol:: send all remaining */
            *numPrbElm = prbMap->nPrbElm;
        }
        else
        {
            if (0 == prbElmProcInfo->nPrbElmPerSym)
                *numPrbElm = prbElmProcInfo->nPrbElmProcessed + 1;
            else
                *numPrbElm = prbElmProcInfo->nPrbElmProcessed + prbElmProcInfo->nPrbElmPerSym;
        }
        *i = prbElmProcInfo->nPrbElmProcessed;
        prbElmProcInfo->numSymsRemaining--;
    } /* if (XRAN_DIR_DL == dir && 0 == pDevCtx->dlCpProcBurst && pDevCtx->DynamicSectionEna == 0) */
    else
    {
        *numPrbElm = prbMap->nPrbElm;
        *i = 0;
    } /* dir = UL, DL - dlCpProcBurst = 1 */

    return 0;
} /* get_prb_elm_to_process */

static inline uint8_t get_fiter_index(uint8_t mu, bool useSectType3, xran_pkt_dir dir, bool isNb375)
{
    if ((useSectType3 && dir == XRAN_DIR_UL && mu == XRAN_NBIOT_MU))
    {
        if (isNb375)
        {
            return XRAN_FILTERINDEX_NPUSCH_375;
        }
        else
        {
            return XRAN_FILTERINDEX_NPUSCH_15;
        }
    }
    else
    {
        return XRAN_FILTERINDEX_STANDARD;
    }
} /* get_fiter_index */

static inline void populate_hardcoded_section_info_fields(struct xran_section_info *info)
{
    info->ef = 0;
    info->ueId = 0;
    info->regFactor = 0;
    info->rb = XRAN_RBIND_EVERY;
    info->symInc = XRAN_SYMBOLNUMBER_NOTINC;
}

static inline int32_t get_freq_offset(struct xran_device_ctx *pDevCtx, bool useSectType3, uint8_t mu)
{ /*Will get selected for particular numerology based on numerology parameter passed*/
    return ((useSectType3) ? pDevCtx->fh_cfg.perMu[mu].freqOffset : 0);
}

static inline void set_iqbuf_offset_and_len_for_uplane(struct xran_prb_elm *pPrbElm, struct xran_section_info *info)
{
    for (uint8_t locSym = 0; locSym < XRAN_NUM_OF_SYMBOL_PER_SLOT; locSym++)
    {
        struct xran_section_desc *pSecDesc = &pPrbElm->sec_desc[locSym];

        if (pSecDesc)
        {
            info->sec_desc[locSym].iq_buffer_offset = pSecDesc->iq_buffer_offset;
            info->sec_desc[locSym].iq_buffer_len = pSecDesc->iq_buffer_len;

            pSecDesc->section_id = info->id;
        }
        else
        {
            print_err("section desc is NULL\n");
        }

    } /* for(locSym = 0; locSym < XRAN_NUM_OF_SYMBOL_PER_SLOT; locSym++) */
}

static inline void populate_ext1_info(uint16_t curSectId, int next, struct xran_sectionext1_info *ext1, struct xran_prb_elm *pPrbElm)
{
    uint32_t extOffset = sizeof(struct xran_cp_radioapp_section1);
    memset(ext1, 0, sizeof(struct xran_sectionext1_info));
    ext1->bfwNumber = pPrbElm->bf_weight.nAntElmTRx;
    ext1->bfwIqWidth = pPrbElm->bf_weight.bfwIqWidth;
    ext1->bfwCompMeth = pPrbElm->bf_weight.bfwCompMeth;

    /* ext-1 buffer contains CP sections */
    ext1->bfwIQ_sz = ONE_EXT_LEN(pPrbElm); // 76
    ext1->p_bfwIQ = (int8_t *)(pPrbElm->bf_weight.p_ext_section + extOffset);

    sect_geninfo[curSectId].exData[next].type = XRAN_CP_SECTIONEXTCMD_1;
    sect_geninfo[curSectId].exData[next].len = sizeof(*ext1);
    sect_geninfo[curSectId].exData[next].data = ext1;
    sect_geninfo[curSectId].exDataSize=1;
}

static inline void populate_ext4_info(int next, struct xran_sectionext4_info *ext4,
    struct xran_prb_elm *pPrbElm)
{
    ext4->csf = 0; // no shift for now only
    ext4->modCompScaler = pPrbElm->ScaleFactor;
    /* TO DO: Should this be the current section id? */
    sect_geninfo[0].exData[next].type = XRAN_CP_SECTIONEXTCMD_4;
    sect_geninfo[0].exData[next].len = sizeof(*ext4);
    sect_geninfo[0].exData[next].data = ext4;

    sect_geninfo[0].info->ef = 1;
    sect_geninfo[0].exDataSize++;
}


static inline void populate_ext11_info(int next, xran_pkt_dir dir, uint16_t curSectId,
    struct xran_prb_elm *pPrbElm, struct xran_sectionext11_info *ext11)
{
    ext11->RAD = pPrbElm->bf_weight.RAD;
    ext11->disableBFWs = pPrbElm->bf_weight.disableBFWs;

    ext11->numBundPrb = pPrbElm->bf_weight.numBundPrb;
    ext11->numSetBFWs = pPrbElm->bf_weight.numSetBFWs;

    ext11->bfwCompMeth = pPrbElm->bf_weight.bfwCompMeth;
    ext11->bfwIqWidth = pPrbElm->bf_weight.bfwIqWidth;

    ext11->maxExtBufSize = pPrbElm->bf_weight.maxExtBufSize;

    ext11->pExtBuf = (uint8_t *)pPrbElm->bf_weight.p_ext_start;
    ext11->totalBfwIQLen = pPrbElm->bf_weight.ext_section_sz;

    sect_geninfo[curSectId].exData[next].type = XRAN_CP_SECTIONEXTCMD_11;
    sect_geninfo[curSectId].exData[next].len = sizeof(*ext11);
    sect_geninfo[curSectId].exData[next].data = ext11;

    sect_geninfo[curSectId].info->ef = 1;
    sect_geninfo[curSectId].exDataSize++;

}

static inline void populate_ext9_info(int next, uint32_t tti, struct xran_device_ctx *pDevCtx,
    struct xran_sectionext9_info *ext9)
{
    uint8_t dssSlot = tti % (pDevCtx->dssPeriod);

    ext9->technology = pDevCtx->technology[dssSlot];
    ext9->reserved = 0;

    sect_geninfo[0].exData[next].type = XRAN_CP_SECTIONEXTCMD_9;
    sect_geninfo[0].exData[next].len = sizeof(*ext9);
    sect_geninfo[0].exData[next].data = ext9;
    sect_geninfo[0].exDataSize++;
}

static inline int enqueue_cp_pkt_to_tx_sym_ring(uint8_t ruPortId, int ccId, uint16_t vfId, uint32_t tti, uint8_t mu,
    struct xran_device_ctx *pDevCtx, struct rte_mbuf *mbuf, xran_pkt_dir dir, int32_t startSym)
{
    uint32_t symIdxToSend = 0;
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    uint32_t interval = xran_fs_get_tti_interval(mu);

    uint32_t num_sym_T1a_max_cp_dl = XRAN_USEC_TO_NUM_SYM(interval, pDevCtx->fh_cfg.perMu[mu].T1a_max_cp_dl);

    mbuf->port = ctx->io_cfg.port[vfId];
    xran_add_eth_hdr_vlan(&ctx->entities[vfId][ID_O_RU], ETHER_TYPE_ECPRI, mbuf);
    struct rte_ring *ring;
    int32_t symToSendOn;

    if(dir == XRAN_DIR_DL)
    {
        if (tti == 0 || (tti * XRAN_NUM_OF_SYMBOL_PER_SLOT + startSym < num_sym_T1a_max_cp_dl))
            symIdxToSend = (xran_fs_get_max_slot(mu) + tti)*XRAN_NUM_OF_SYMBOL_PER_SLOT + startSym - num_sym_T1a_max_cp_dl;
        else
            symIdxToSend = tti * XRAN_NUM_OF_SYMBOL_PER_SLOT + startSym - num_sym_T1a_max_cp_dl;

        symToSendOn = symIdxToSend % XRAN_NUM_OF_SYMBOL_PER_SLOT;
    }

    else
    {
        if (tti == 0)
            symIdxToSend = xran_fs_get_max_slot(mu) * XRAN_NUM_OF_SYMBOL_PER_SLOT - XRAN_NUM_OF_SYMBOL_PER_SLOT;
        else
            symIdxToSend = tti * XRAN_NUM_OF_SYMBOL_PER_SLOT - XRAN_NUM_OF_SYMBOL_PER_SLOT;

        symToSendOn = pDevCtx->perMu[mu].ulCpTxSym;
    }

    uint32_t tti_to_send = XranGetTtiNum(symIdxToSend, XRAN_NUM_OF_SYMBOL_PER_SLOT) % XRAN_N_FE_BUF_LEN;
    uint8_t num_eAxc = xran_get_num_eAxc(pDevCtx);
    uint8_t num_CCPorts = xran_get_num_cc(pDevCtx);
    uint8_t antToSend = ruPortId % num_eAxc;
    uint8_t ccToSend = ccId % num_CCPorts;

    print_dbg("dir %d mu %d tti %d cc %d ant %d sym %d \n", dir, mu, tti, ccToSend, antToSend, symToSendOn);
#ifdef DEBUG
    if(true == xran_check_if_late_transmission(tti_to_send,symToSendOn, mu))
    {
        print_err("late cp paket: cc %d ant %d dir %d mu %d tti %d sym %d \n",
            ccToSend, antToSend, dir, mu, tti, symToSendOn);
        rte_panic("\n");
    }
#endif
    ring = (struct rte_ring *)pDevCtx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[tti_to_send][ccToSend][antToSend].sBufferList.pBuffers[symToSendOn].pRing;

    if (0 == rte_ring_enqueue(ring, mbuf))
        return 1;
    else
        return 0;
}

int32_t xran_transmit_cpmsg_prach(void *pHandle, struct xran_cp_gen_params *params, struct xran_section_gen_info *sect_geninfo, struct rte_mbuf *mbuf, struct xran_device_ctx *pxran_lib_ctx,
                                int tti, uint8_t cc_id, uint8_t prach_port_id, uint8_t seq_id, uint8_t mu)
{
    int32_t ret = 0;


    if(unlikely(pxran_lib_ctx==NULL))
        return XRAN_STATUS_FAIL;

    if(xran_get_syscfg_bbuoffload())
    {
        uint16_t vfId = xran_map_ecpriRtcid_to_vf(pxran_lib_ctx, XRAN_DIR_UL, cc_id, prach_port_id);
        /* add in the ethernet header */
        struct rte_ether_hdr *const h = (void *)rte_pktmbuf_prepend(mbuf, sizeof(*h));

        if(unlikely(enqueue_cp_pkt_to_tx_sym_ring(prach_port_id , cc_id, vfId, tti, mu, pxran_lib_ctx, mbuf, XRAN_DIR_UL, 0) != 1)){
            ret = XRAN_STATUS_FAIL;
            print_err("PRACH CP enQ failed! ru_portId %d ccId %d tti %d", prach_port_id, cc_id, tti);
            rte_pktmbuf_free(mbuf);
        } else {
            /* Increment the counters */
            ++pxran_lib_ctx->fh_counters.tx_counter;
            pxran_lib_ctx->fh_counters.tx_bytes_counter += rte_pktmbuf_pkt_len(mbuf);
        }
    }
    else
        ret = send_cpmsg(pxran_lib_ctx, mbuf, params, sect_geninfo, cc_id, prach_port_id, seq_id, mu);

    return ret;
}

static inline void get_start_sym_and_num_sym(uint8_t xranPortId, int ccId, int tti, uint8_t mu, xran_pkt_dir dir,
    struct xran_prb_elm *pPrbElm, int32_t *startSym, int32_t *numSyms)
{

    if (xran_fs_get_slot_type(xranPortId, ccId, tti, XRAN_SLOT_TYPE_FDD, mu) != 1 && xran_fs_get_slot_type(xranPortId, ccId, tti, XRAN_SLOT_TYPE_SP, mu) == 1)
    {
        /* This function cannot handle two or more groups of consecutive same type of symbol.
         * If there are two or more, then it might cause an error */
        *startSym = xran_check_symbolrange(
            ((dir == XRAN_DIR_DL) ? XRAN_SYMBOL_TYPE_DL : XRAN_SYMBOL_TYPE_UL),
            xranPortId, ccId, tti,
            pPrbElm->nStartSymb,
            pPrbElm->numSymb,
            numSyms, mu);
    }
    else
    {
        *startSym = pPrbElm->nStartSymb;
        *numSyms = pPrbElm->numSymb;
    }
}

int generate_cpmsg_prach(void *pHandle, struct xran_cp_gen_params *params, struct xran_section_gen_info *sect_geninfo, struct rte_mbuf *mbuf, struct xran_device_ctx *pxran_lib_ctx,
                uint8_t frame_id, uint8_t subframe_id, uint8_t slot_id, int tti,
                uint16_t beam_id, uint8_t cc_id, uint8_t prach_port_id, uint16_t occasionid, uint8_t seq_id, uint8_t mu,
                uint8_t sym_id/* sym_id is ignored for non-nbiot calls */)
{
    int nsection, ret;
    uint16_t timeOffset;
    uint8_t startSymId;
    struct xran_prach_cp_config  *pPrachCPConfig = NULL;
    int i=0;
    uint8_t nNumerology = mu;

    if(unlikely(mbuf == NULL)) {
        print_err("Alloc fail!\n");
        return XRAN_STATUS_FAIL;
    }

    if(mu==XRAN_NBIOT_MU){
        pPrachCPConfig = &(pxran_lib_ctx->perMu[mu].PrachCPConfig);
        startSymId = sym_id;
        nNumerology = 0;
    }
    else {
        if(pxran_lib_ctx->dssEnable){
            i = tti % pxran_lib_ctx->dssPeriod;
            if(pxran_lib_ctx->technology[i]==1) {
                pPrachCPConfig = &(pxran_lib_ctx->perMu[mu].PrachCPConfig);
            }
            else
            {
                pPrachCPConfig = &(pxran_lib_ctx->perMu[mu].PrachCPConfigLTE);
            }
        }
        else
        {pPrachCPConfig = &(pxran_lib_ctx->perMu[mu].PrachCPConfig);}

        startSymId = pPrachCPConfig->startSymId + occasionid * pPrachCPConfig->duration; /*will be calculated based on 15khz SCS*/
    }

        timeOffset = pPrachCPConfig->timeOffset; //this is the CP value per 38.211 tab 6.3.3.1-1&2

#if 0
    printf("%d:%d:%d:%d - filter=%d, startSym=%d[%d:%d], numSym=%d, occasions=%d, freqOff=%d\n",
                frame_id, subframe_id, slot_id, prach_port_id,
                pPrachCPConfig->filterIdx,
                pPrachCPConfig->startSymId,
                pPrachCPConfig->startPrbc,
                pPrachCPConfig->numPrbc,
                pPrachCPConfig->numSymbol,
                pPrachCPConfig->occassionsInPrachSlot,
                pPrachCPConfig->freqOffset);
#endif

    if(startSymId > 0)
        timeOffset += startSymId * (2048 + 144);

    if(XRAN_FILTERINDEX_PRACH_ABC == pPrachCPConfig->filterIdx)
    {
        timeOffset = timeOffset >> nNumerology; //original number is Tc, convert to Ts based on mu
        if ((slot_id == 0) || (slot_id == (SLOTNUM_PER_SUBFRAME(xran_fs_get_tti_interval(mu)) >> 1)))
            timeOffset += 16;
    }
    else
    {
        //when prach scs lower than 15khz, timeOffset base 15khz not need to adjust.
    }

    params->dir                  = XRAN_DIR_UL;
    params->sectionType          = XRAN_CP_SECTIONTYPE_3;
    params->hdr.filterIdx        = pPrachCPConfig->filterIdx;
    params->hdr.frameId          = frame_id;
    params->hdr.subframeId       = subframe_id;
    params->hdr.slotId           = slot_id;
    params->hdr.startSymId       = startSymId;
    params->hdr.iqWidth          = xran_get_conf_iqwidth_prach(pHandle);
    params->hdr.compMeth         = xran_get_conf_compmethod_prach(pHandle);
    params->hdr.timeOffset       = timeOffset;
    params->hdr.fftSize          = xran_get_conf_fftsize(pHandle,mu);
	/*convert to o-ran ecpri specs scs index*/
    switch(pPrachCPConfig->filterIdx)
    {
        case XRAN_FILTERINDEX_PRACH_012:
            params->hdr.scs              = 12;
            break;
        case XRAN_FILTERINDEX_NPRACH:
            params->hdr.scs              = 13;
            break;
        case XRAN_FILTERINDEX_PRACH_3:
            params->hdr.scs              = 14;
            break;
        case XRAN_FILTERINDEX_LTE4:
            params->hdr.scs              = 15;
            break;
        case XRAN_FILTERINDEX_PRACH_ABC:
            params->hdr.scs              = xran_get_conf_prach_scs(pHandle,mu);
            break;
        default:
            print_err("prach filterIdx error - [%d:%d:%d]--%d\n", frame_id, subframe_id, slot_id,pPrachCPConfig->filterIdx);
            params->hdr.scs              = 0;
            break;
    }
    params->hdr.cpLength         = 0;

    nsection = 0;
    sect_geninfo[nsection].info->type        = params->sectionType;       // for database
    sect_geninfo[nsection].info->startSymId  = params->hdr.startSymId;    // for database
    sect_geninfo[nsection].info->iqWidth     = params->hdr.iqWidth;       // for database
    sect_geninfo[nsection].info->compMeth    = params->hdr.compMeth;      // for database
    sect_geninfo[nsection].info->id          = xran_alloc_sectionid(pHandle, XRAN_DIR_UL, cc_id, prach_port_id, tti);
    sect_geninfo[nsection].info->rb          = XRAN_RBIND_EVERY;
    sect_geninfo[nsection].info->symInc      = XRAN_SYMBOLNUMBER_NOTINC;
    sect_geninfo[nsection].info->startPrbc   = 0;
    sect_geninfo[nsection].info->numPrbc     = pPrachCPConfig->numPrbc;
    sect_geninfo[nsection].info->numSymbol   = (mu==XRAN_NBIOT_MU ? NBIOT_NUM_SYMS_IN_ONE_GROUP : pPrachCPConfig->numSymbol);
    sect_geninfo[nsection].info->reMask      = 0xfff;
    sect_geninfo[nsection].info->beamId      = beam_id;
    sect_geninfo[nsection].info->freqOffset  = pPrachCPConfig->freqOffset;
    sect_geninfo[nsection].info->prbElemBegin = 1;
    sect_geninfo[nsection].info->prbElemEnd   = 1;
    /*TDOMIXED: check what this parameter does*/
    pxran_lib_ctx->prach_last_symbol[cc_id] = pPrachCPConfig->startSymId + pPrachCPConfig->numSymbol*pPrachCPConfig->occassionsInPrachSlot - 1;

    sect_geninfo[nsection].info->ef         = 0;
    sect_geninfo[nsection].exDataSize       = 0;
    // sect_geninfo[nsection].exData           = NULL;
    nsection++;

    params->numSections          = nsection;
    params->sections             = sect_geninfo;

    ret = xran_prepare_ctrl_pkt(mbuf, params, cc_id, prach_port_id, seq_id, 0, mu, XRAN_GET_OXU_PORT_ID((xran_device_ctx_t*)pHandle) );
    if(ret < 0){
        print_err("Fail to build prach control packet - [%d:%d:%d]\n", frame_id, subframe_id, slot_id);
        rte_pktmbuf_free(mbuf);
    }

    return ret;
}

xran_status_t xran_check_cp_dl_sym_to_send(uint32_t cp_dl_tti, uint32_t cp_dl_sym, uint16_t T1a_max_cp_dl, uint8_t mu)
{
    uint32_t cp_dl_tti_wrap = cp_dl_tti;
    uint32_t interval = xran_fs_get_tti_interval(mu);

    if(cp_dl_tti == 0)
    {
        cp_dl_tti_wrap = xran_fs_get_max_slot(mu);
    }

    if(xran_lib_ota_sym_idx_mu[mu] + XRAN_USEC_TO_NUM_SYM(interval, T1a_max_cp_dl) == (cp_dl_tti_wrap*N_SYM_PER_SLOT + cp_dl_sym))
    {
        return XRAN_STATUS_SUCCESS;
    }

    return XRAN_STATUS_FAIL;
}

/* Function to derive timing info for virtual numerology from timer numerology
   Currently this function supports vMu > Timer Mu
   timerTti2proc - tti in a second */
static inline xran_status_t xran_calculate_vMu_time_info(uint8_t mu, uint8_t vMu, int timerTti2proc, uint16_t *vMuTti, uint16_t *vMuSfId, uint16_t *vMuSlotId, uint16_t *vMuFrameId, uint8_t *vMuSymId, uint8_t ttiIdx){

    uint8_t timeScalingFactor;

    if(vMu > mu){
        timeScalingFactor = (1 << (vMu - mu));
        // vMuSymIdx  = (xran_lib_ota_sym_idx_mu[mu] * timeScalingFactor) + symIdx;
        // *vMuSymId  = vMuSymIdx % XRAN_NUM_OF_SYMBOL_PER_SLOT;
        // *vMuTti    = vMuSymIdx / XRAN_NUM_OF_SYMBOL_PER_SLOT;
        *vMuTti    = (timerTti2proc * timeScalingFactor) + ttiIdx;
        *vMuSlotId = *vMuTti % (1 << vMu);
        *vMuSfId   = (*vMuTti / (1 << vMu)) % SUBFRAMES_PER_SYSTEMFRAME;

        if(xran_get_syscfg_bbuoffload()){
            /* tti value passed for bbuOffload accounts for sys frame num */
            *vMuFrameId = (*vMuTti/(SUBFRAMES_PER_SYSTEMFRAME * (1 << vMu))) & 0xFF;
        }
        else{
            *vMuFrameId = XranGetFrameNum(*vMuTti, xran_getSfnSecStart(), SUBFRAMES_PER_SYSTEMFRAME, (1 << vMu));
            *vMuFrameId = (*vMuFrameId + ((*vMuTti == 0) ? NUM_OF_FRAMES_PER_SECOND : 0)) & 0xFF;
        }
    }
    else{
        return XRAN_STATUS_FAIL;
    }

    return XRAN_STATUS_SUCCESS;
}

xran_status_t xran_reset_vMu_section_db(uint8_t elmIdx, struct xran_device_ctx *pDevCtx, uint8_t ccId, uint8_t ruPortId, uint8_t ctxId, uint8_t vMu){

    xran_status_t retVal = XRAN_STATUS_SUCCESS;
     /* Reset section DB at the start of every SSB tti */
    if((elmIdx == 0 && xran_get_syscfg_bbuoffload() == 0) ||
        xran_get_syscfg_bbuoffload() == 1)
    {
        retVal = xran_cp_reset_section_info((void *)pDevCtx, XRAN_DIR_DL, ccId, ruPortId, ctxId, vMu);

        if(unlikely(retVal != XRAN_STATUS_SUCCESS)){
            print_err("xran_cp_reset_section_info failed dir %u ccId %d ruPortId %hhu ctxId %hhu vMu %hhu",XRAN_DIR_DL, ccId, ruPortId, ctxId, vMu);
            return retVal;
        }
    }
    return retVal;
}
/* Function to prepare SSB packets when SSB is configured as a virtual numerology
   This function will process SSB packets for all SSB mu 4 symbols contained in a mu 3 sym
   at once.
   Note -
   1. App level fragmentation not supported
   2. SSB timing is derived from timer numerology.
      However while transmitting packets timer numerology symbol/tti boundaries are used.
   3. SSB as vMu and timer numerology share same combined data base which is indexed by numerology
      hence should not conflict
      Assumption - This function is called on every symbol of the timer numerology (mu 3), hence it should process two
      SSB symbols (mu 4) at once.
*/
xran_status_t xran_vMu_cp_create_and_send_ssb_pkt(uint8_t xranPortId, uint8_t ruPortId, int dir, int tti2Proc, int ccId, uint8_t vMu, uint8_t mu)
{
    xran_device_ctx_t* pDevCtx = xran_dev_get_ctx_by_id(xranPortId);
    struct xran_prb_map *prbMap;
    struct xran_prb_elm *ssbPrbElem, tempPrbElem;
    uint8_t elmIdx;
    int32_t timerMuStartSym, timerMuNumSym;
    struct xran_cp_gen_params cpPktGenParams;
    struct rte_mbuf *ssb_cp_mbuf;
    uint16_t ssbTti, ssbSfId, ssbSlotId, ssbFrameId, vfId, ssbBufId;
    uint8_t ssbStartSym, ssbNumSym, seqId;
    struct xran_section_gen_info sectgen_info[XRAN_MAX_NUM_SECTIONS];
    struct xran_section_info *p_sectionInfo;
    xran_status_t retVal;
    uint8_t ssbSymId, ctxId, ttiIdx, timeScalingFactor = (1 << (vMu - mu));
    uint8_t ant_id;

    if(unlikely(pDevCtx == NULL)){
        print_err("DevCtx is NULL");
        return XRAN_STATUS_FAIL;
    }

    if(unlikely(pDevCtx->RunSlotPrbMapBySymbolEnable)){
        print_err("RunSlotPrbMapBySymbolEnable not supported");
        return XRAN_STATUS_UNSUPPORTED;
    }

    ant_id = ruPortId - pDevCtx->vMuInfo.ssbInfo.ruPortId_offset;
    vfId = xran_map_ecpriRtcid_to_vf(pDevCtx, XRAN_DIR_DL, ccId, ruPortId);
    // process SSB packets for two SSB mu 4 slots contained in a mu 3 slot at once.
    // Two SSB slot in One PDSCH slot
    for(ttiIdx = 0; ttiIdx < timeScalingFactor; ++ttiIdx){

        retVal = xran_calculate_vMu_time_info(mu, vMu, tti2Proc, &ssbTti, &ssbSfId, &ssbSlotId, &ssbFrameId, &ssbSymId, ttiIdx);
        if(unlikely(retVal != XRAN_STATUS_SUCCESS)){
            print_err("xran_calculate_vMu_time_info failure");
            return retVal;
        }

        ssbBufId = ssbTti % XRAN_N_FE_BUF_LEN;
        ctxId = ssbTti % XRAN_MAX_SECTIONDB_CTX;


        /* Get the PRB map */
        if(likely(pDevCtx->vMuInfo.ssbInfo.sFhSsbTxPrbMapBbuIoBufCtrl[ssbBufId][ccId][ant_id].sBufferList.pBuffers)){
            if(likely(pDevCtx->vMuInfo.ssbInfo.sFhSsbTxPrbMapBbuIoBufCtrl[ssbBufId][ccId][ant_id].sBufferList.pBuffers->pData)){
                prbMap  = (struct xran_prb_map *)pDevCtx->vMuInfo.ssbInfo.sFhSsbTxPrbMapBbuIoBufCtrl[ssbBufId][ccId][ant_id].sBufferList.pBuffers->pData;
            }
            else{
                print_dbg("pData is NULL: ssbBufId %hu ccId %d ant_id %hhu",ssbBufId, ccId, ant_id);
                continue;
            }
        }
        else{
            print_dbg("pBuffers is NULL: ssbBufId %hu ccId %d ant_id %hhu",ssbBufId, ccId, ant_id);
            continue;
        }

        for(elmIdx = 0; elmIdx < prbMap->nPrbElm; ++elmIdx)
        {
            if(unlikely(elmIdx >= XRAN_MAX_SECTIONS_PER_SLOT))
                return XRAN_STATUS_INVALID_PARAM;


            ssbPrbElem = &prbMap->prbMap[elmIdx];

            #if 0
            printf("\nelmIdx %hhu nRBStart %hd nRBSize %hd nStartSymb %hd numSymb %hd iqWidth %hd compMethod %hd",
                    elmIdx, ssbPrbElem->nRBStart, ssbPrbElem->nRBSize, ssbPrbElem->nStartSymb, ssbPrbElem->numSymb, ssbPrbElem->iqWidth, ssbPrbElem->compMethod);

            #endif
            // App level fragmentation not supported - Add a check
            if(ssbPrbElem->generateCpPkt != 1){
                print_dbg("App Level fragmentation not supported");
                continue;
            }


            /* Handling the symbol range for special slot */
            /* 1. Map start symbol & Num symbol from ssbPrbElem to Timer symbol */
            tempPrbElem.nStartSymb = (ssbPrbElem->nStartSymb / timeScalingFactor) + (ttiIdx*(XRAN_NUM_OF_SYMBOL_PER_SLOT/(1 << (vMu - mu))));
            tempPrbElem.numSymb    = (ssbPrbElem->numSymb / timeScalingFactor) + (ssbPrbElem->numSymb % timeScalingFactor);

            /* 2. Check the range for timer symbol */
            get_start_sym_and_num_sym(xranPortId, ccId, tti2Proc, mu, XRAN_DIR_DL, &tempPrbElem , &timerMuStartSym, &timerMuNumSym);

            /* 3. If symbol range is allowed then transmit the symbols else continue */
            if(timerMuStartSym < 0 || timerMuNumSym <= 0)
            {
                /* Force reset DB to handle special & UL slots */
                xran_reset_vMu_section_db(0, pDevCtx, ccId, ruPortId, ctxId, vMu);
                continue;
            }

            /* 4. ReMap timer Mu sym to SSB sym */
            ssbStartSym = (timerMuStartSym * timeScalingFactor) % XRAN_NUM_OF_SYMBOL_PER_SLOT;
            if(ssbPrbElem->numSymb < (timerMuNumSym * timeScalingFactor)){
                ssbNumSym = ssbPrbElem->numSymb;
            }
            else{
                ssbNumSym = timerMuNumSym * timeScalingFactor;
            }

            /* Check if the DL sym can be transmitted */
            if(xran_get_syscfg_bbuoffload() == 0){

                if(xran_check_cp_dl_sym_to_send(tti2Proc, timerMuStartSym, pDevCtx->fh_cfg.perMu[mu].T1a_max_cp_dl, mu)){
                    continue;
                }
            }

            /* Reset section DB at the start of every SSB tti */
            if(xran_reset_vMu_section_db(elmIdx, pDevCtx, ccId, ruPortId, ctxId, vMu) != XRAN_STATUS_SUCCESS){
                continue;
            }

            /*  Radio application common header */
            cpPktGenParams.dir = XRAN_DIR_DL;
            // XRAN_PAYLOAD_VER filled in xran_prepare_radioapp_common_header
            cpPktGenParams.hdr.filterIdx  = XRAN_FILTERINDEX_STANDARD;
            cpPktGenParams.hdr.frameId    = ssbFrameId;
            cpPktGenParams.hdr.subframeId = ssbSfId;
            cpPktGenParams.hdr.slotId     = ssbSlotId;
            cpPktGenParams.hdr.startSymId = ssbStartSym;
            cpPktGenParams.numSections    = 1;
            cpPktGenParams.sectionType    = XRAN_CP_SECTIONTYPE_3;

            /** Radio app ST3 specific **/
            cpPktGenParams.hdr.timeOffset = cpPktGenParams.hdr.startSymId; // 7.5.2.12 - timeOffset points to the same timing pointed by startSymbolId
            cpPktGenParams.hdr.scs        = vMu;
            cpPktGenParams.hdr.fftSize    = pDevCtx->fh_cfg.perMu[mu].nDLFftSize;
            cpPktGenParams.hdr.cpLength   = 0;
            cpPktGenParams.hdr.iqWidth    = ssbPrbElem->iqWidth;
            cpPktGenParams.hdr.compMeth   = ssbPrbElem->compMethod;

            sectgen_info[elmIdx].info = xran_cp_get_section_info_ptr(pDevCtx, XRAN_DIR_DL, ccId, ruPortId, ctxId, vMu);
            sect_geninfo[elmIdx].exDataSize = 0;
            p_sectionInfo = sectgen_info[elmIdx].info;

            if(unlikely(p_sectionInfo == NULL)){
                continue;
            }

            /** CP control section header ST3 */
            // Update database
            p_sectionInfo->prbElemBegin = 1;
            p_sectionInfo->prbElemEnd   = 1;
            p_sectionInfo->type       = cpPktGenParams.sectionType;
            p_sectionInfo->iqWidth    = ssbPrbElem->iqWidth;
            p_sectionInfo->compMeth   = ssbPrbElem->compMethod;
            p_sectionInfo->id         = elmIdx;
            p_sectionInfo->rb         = XRAN_RBIND_EVERY;
            p_sectionInfo->symInc     = XRAN_SYMBOLNUMBER_NOTINC;
            p_sectionInfo->startPrbc  = ssbPrbElem->nRBStart;
            p_sectionInfo->numPrbc    = ssbPrbElem->nRBSize;
            p_sectionInfo->reMask     = 0xfff;
            p_sectionInfo->startSymId = cpPktGenParams.hdr.startSymId;
            p_sectionInfo->numSymbol  = ssbNumSym;
            p_sectionInfo->ef         = 0;
            p_sectionInfo->beamId     = ssbPrbElem->nBeamIndex;
            p_sectionInfo->freqOffset = pDevCtx->vMuInfo.ssbInfo.freqOffset;

            cpPktGenParams.sections = sectgen_info;

            /* Set IQ offset */
            set_iqbuf_offset_and_len_for_uplane(ssbPrbElem, sectgen_info[elmIdx].info);
            // Alloc mbuf
            ssb_cp_mbuf = xran_ethdi_mbuf_alloc();

            if(unlikely(ssb_cp_mbuf == NULL)){
                print_err("Mbuf alloc failed");
                continue;
            }

            seqId = xran_get_cp_seqid(pDevCtx, XRAN_DIR_DL, ccId, ruPortId);

            // Prepare the ORAN CP header
            retVal = xran_prepare_ctrl_pkt(ssb_cp_mbuf, &cpPktGenParams, ccId, ruPortId, seqId, sectgen_info[elmIdx].info->id, vMu, xranPortId);

            if(unlikely(retVal != XRAN_STATUS_SUCCESS)){
                print_err("Failed SSB CP ST3 xran_prepare_ctrl_pkt failed");
                rte_pktmbuf_free(ssb_cp_mbuf);
                continue;
            }

            // Transmit packet
            // if(ssbPrbElem->generateCpPkt)
            {

                int32_t cp_sent = 0;
                int32_t pkt_len = 0;

                /* add in the ethernet header */
                struct rte_ether_hdr *const h = (void *)rte_pktmbuf_prepend(ssb_cp_mbuf, sizeof(*h));
                pkt_len = rte_pktmbuf_pkt_len(ssb_cp_mbuf);

                if(pkt_len > pDevCtx->mtu){
                    print_err("pkt_len (%d) > mtu (%u)",pkt_len, pDevCtx->mtu);
                    rte_pktmbuf_free(ssb_cp_mbuf);
                    continue;
                }

                if(xran_get_syscfg_bbuoffload())
                {
                    cp_sent = enqueue_cp_pkt_to_tx_sym_ring(ruPortId, ccId, vfId, tti2Proc, mu, pDevCtx, ssb_cp_mbuf, XRAN_DIR_DL, timerMuStartSym);
                }
                else
                {
                    cp_sent = pDevCtx->send_cpmbuf2ring(ssb_cp_mbuf, ETHER_TYPE_ECPRI, vfId);
                }

                if(cp_sent != 1)
                {
                    rte_pktmbuf_free(ssb_cp_mbuf);
                }

                /* Increment the counters */
                pDevCtx->fh_counters.tx_counter += 1;
                pDevCtx->fh_counters.tx_bytes_counter += pkt_len;
            }
        } //for elmIdx
    } //for ttiIdx

    return XRAN_STATUS_SUCCESS;
}

int32_t xran_cp_create_and_send_section(uint8_t xranPortId, uint8_t ruPortId, int dir, int tti, int ccId,
        struct xran_prb_map *prbMap, struct xran_prb_elm_proc_info_t *prbElmProcInfo,
        enum xran_category category,  uint8_t ctx_id, uint8_t mu, bool useSectType3, uint8_t bbuoffload)
{
    int32_t ret = 0;
    struct xran_device_ctx *pDevCtx   = xran_dev_get_ctx_by_id(xranPortId);
    if(pDevCtx==NULL)
        return -1;

    struct xran_common_counters *pCnt = &pDevCtx->fh_counters;
    struct xran_cp_gen_params cpPktGenParams;
    struct xran_section_info *info;
    struct rte_mbuf *mbuf;

    uint32_t curPrbElm, numPrbElm, j;
    struct xran_prb_elm *pPrbElm = NULL;
    bool isNb375 =  ( (XRAN_DIR_UL==dir) && (mu==XRAN_NBIOT_MU) && (pDevCtx->fh_cfg.perMu[mu].nbIotUlScs==XRAN_NBIOT_UL_SCS_3_75)? true : false);

    // struct xran_prb_elm *pPrbMapElemPrev = NULL;
    uint32_t frameId, sfId, slotId;
    uint8_t seqId = 0;
    uint16_t vfId = 0 , curSectId = 0;
    int32_t startSym = 0, numSyms = 0;
    int16_t reMask = 0;
    int next=0, curExtInSect=0;
    struct xran_sectionext1_info ext1[XRAN_MAX_SECTIONS_PER_SLOT];
    struct xran_sectionext4_info ext4 = {0};
    struct xran_sectionext9_info ext9;
    struct xran_sectionext11_info ext11;

    /* Set frameId, subframeId, slotId from tti */
    get_f_sf_slot(mu, tti, isNb375, &frameId, &sfId, &slotId,bbuoffload);
    vfId = xran_map_ecpriRtcid_to_vf(pDevCtx, dir, ccId, ruPortId);

    /* Generate a C-Plane message per each section,
     * not a C-Plane message with multi sections */
    if(0 == pDevCtx->RunSlotPrbMapBySymbolEnable)
    {
        /* Set the first prb-element to process - 'curPrbElm' and set number of prbElements to process - numPrbElm */
        if(-1 == get_prb_elm_to_process(pDevCtx, prbMap, prbElmProcInfo, dir, &numPrbElm, &curPrbElm))
            return -1;

        pPrbElm = &prbMap->prbMap[0];
        curSectId = curPrbElm;

        /* Generate a C-Plane message per each section,
        * not a C-Plane message with multi sections */
        /* curPrbElm is set by get_prb_elm_to_process() */
        for (; curPrbElm < numPrbElm; curPrbElm++) {

            pPrbElm  = &prbMap->prbMap[curPrbElm];

            /* For Special Subframe,
             * Check validity of given symbol range with slot configuration
             * and adjust symbol range accordingly. */
            get_start_sym_and_num_sym(xranPortId, ccId, tti, mu, dir, pPrbElm, &startSym, &numSyms);

            if((startSym < 0 || numSyms <= 0))
            {
                /* if start symbol is not valid, then skip this section */
                print_dbg("Skip section %d due to invalid symbol range - TTI = %d [%d:%d], [%d:%d]", curPrbElm, tti,
                            pPrbElm->nStartSymb, pPrbElm->numSymb, startSym, numSyms);
                continue;
            }

            /* Check startSym and determine if CP DL should be transmitted on this symbol */
            if(dir == XRAN_DIR_DL && xran_get_syscfg_bbuoffload() == 0 && pDevCtx->dlCpProcBurst != 0)
            {
                if(xran_check_cp_dl_sym_to_send(tti, startSym, pDevCtx->fh_cfg.perMu[mu].T1a_max_cp_dl, mu) != XRAN_STATUS_SUCCESS)
                    continue;
            }

            cpPktGenParams.dir              = dir;
            cpPktGenParams.sectionType      = (useSectType3 ? XRAN_CP_SECTIONTYPE_3 : XRAN_CP_SECTIONTYPE_1);
            cpPktGenParams.hdr.filterIdx    = get_fiter_index(mu, useSectType3, dir, isNb375);
            cpPktGenParams.hdr.frameId      = frameId;
            cpPktGenParams.hdr.subframeId   = sfId;
            cpPktGenParams.hdr.slotId       = slotId;
            cpPktGenParams.hdr.startSymId   = startSym;
            cpPktGenParams.hdr.iqWidth      = pPrbElm->iqWidth;
            cpPktGenParams.hdr.compMeth     = pPrbElm->compMethod;

            if(useSectType3){
                cpPktGenParams.hdr.scs              = ((useSectType3 && mu == XRAN_NBIOT_MU)? 13 : mu); /* This should be 11 for NB-IOT?*/
                cpPktGenParams.hdr.fftSize          = xran_get_conf_fftsize((void *)pDevCtx, mu);  /*Will be selected based on which numerology is getting processed*/
            }

            print_dbg("cp[%d:%d:%d] ruPortId %d dir=%d\n", frameId, sfId, slotId, ruPortId, dir);

            curExtInSect=0;
            if(unlikely(curSectId >= XRAN_MAX_SECTIONS_PER_SLOT))
            {
                print_err("sectinfo->id %d\n", curSectId);
                return -1;
            }
            reMask = pPrbElm->reMask;
            sect_geninfo[curSectId].exDataSize=0;

            if(unlikely(reMask && reMask != 0xfff)){
                uint32_t prb_id = (pPrbElm->UP_nRBStart*100 + pPrbElm->startSectId);
                /*If Entry exists in DB, just create and send CP packet. replacing the existing entry in database*/
                info = xran_cp_check_db_entry(pDevCtx, dir, ccId, ruPortId, ctx_id, mu, prb_id);
                if( info != NULL)
                    sect_geninfo[curSectId].info = info;
                else
                    sect_geninfo[curSectId].info = xran_cp_get_section_info_ptr(pDevCtx, dir, ccId, ruPortId, ctx_id, mu);
                sect_geninfo[curSectId].info->reMask = reMask;
            }
            else{
                sect_geninfo[curSectId].info = xran_cp_get_section_info_ptr(pDevCtx, dir, ccId, ruPortId, ctx_id, mu);
                sect_geninfo[curSectId].info->reMask = 0xfff;
            }

            if(unlikely(sect_geninfo[curSectId].info == NULL))
            {
                rte_panic("xran_cp_get_section_info_ptr failed\n");
            }

            info = sect_geninfo[curSectId].info;
            populate_hardcoded_section_info_fields(info);

            info->prbElemBegin  = 1;
            info->prbElemEnd    = 1;
            info->freqOffset    = get_freq_offset(pDevCtx, useSectType3, mu);

            info->startPrbc = pPrbElm->nRBStart;
            info->numPrbc   = pPrbElm->nRBSize;

            info->type        = (useSectType3 ? XRAN_CP_SECTIONTYPE_3 : XRAN_CP_SECTIONTYPE_1);
            info->startSymId  = startSym;
            info->iqWidth     = pPrbElm->iqWidth;
            info->compMeth    = pPrbElm->compMethod;
            info->numSymbol   = numSyms;
            info->beamId      = pPrbElm->nBeamIndex;
            info->id          = pPrbElm->startSectId;


            /* pPrbElm contains per symbol iq data offset and len that uplane has to send.
                copy the values in info and set sectionId as well */
            set_iqbuf_offset_and_len_for_uplane(pPrbElm, info);

            /* Add extentions if required */
            if((category == XRAN_CATEGORY_B) && (pPrbElm->bf_weight_update))
            {
                if(pPrbElm->bf_weight.extType == 1) /* Prepare section data for ext-1 */
                {
                    info->ef = 1;
                    populate_ext1_info(curSectId, curExtInSect, &ext1[curSectId], pPrbElm);
                    curExtInSect++;
                }
                else if(pPrbElm->bf_weight.extType == 11)
                {   /* This should get called only once for ext11 */
                    info->ef = 1;
                    populate_ext11_info(curExtInSect, dir, curSectId, pPrbElm, &ext11);
                    curExtInSect++;
                }
            } /* if((category == XRAN_CATEGORY_B) && (pPrbElm->bf_weight_update)) */

            if (pPrbElm->generateCpPkt) //only send actual new CP section
            {
                /* Extension 4 for modulation compression */
                if(pPrbElm->compMethod == XRAN_COMPMETHOD_MODULATION)
                {
                    populate_ext4_info(curExtInSect, &ext4, pPrbElm);
                    curExtInSect++;
                }

                /* Extension 1 or 11 for Beam forming weights */
                /* add section extention for BF Weights if update is needed */
                if((category == XRAN_CATEGORY_B) && (pPrbElm->bf_weight_update))
                {
                    if(pPrbElm->bf_weight.extType == 1) /* Using Extension 1 */
                    {
                        struct rte_mbuf_ext_shared_info * p_share_data =
                        &pDevCtx->cp_share_data.sh_data[tti % XRAN_N_FE_BUF_LEN][ccId][ruPortId][sect_geninfo[0].info->id];

                        if(pPrbElm->bf_weight.p_ext_start)
                        {
                            /* use buffer with BF Weights for mbuf */
                            mbuf = xran_attach_cp_ext_buf(pPrbElm->bf_weight.p_ext_start,
                                                        pPrbElm->bf_weight.p_ext_section,
                                                        pPrbElm->bf_weight.ext_section_sz, p_share_data);
                        }
                        else
                        {
                            print_err("p %d cc %d dir %d Alloc fail!\n", xranPortId, ccId, dir);
                            ret=-1;
                            goto _create_and_send_section_error;
                        }
                    } /* if(pPrbElm->bf_weight.extType == 1) */
                    else
                    {
                        /* Using Extension 11 */
                        struct rte_mbuf_ext_shared_info *shared_info;

                        shared_info = &pDevCtx->bfw_share_data.sh_data[tti % XRAN_N_FE_BUF_LEN][ccId][ruPortId][sect_geninfo[0].info->id];

                        mbuf = xran_ethdi_mbuf_indir_alloc();
                        if (unlikely(mbuf == NULL))
                        {
                            rte_panic("Alloc fail!\n");
                        }
                        // mbuf = rte_pktmbuf_alloc(_eth_mbuf_pool_vf_small[vfId]);
                        if (xran_cp_attach_ext_buf(mbuf, (uint8_t *)pPrbElm->bf_weight.p_ext_start,
                            pPrbElm->bf_weight.maxExtBufSize, shared_info) < 0)
                        {
                            rte_pktmbuf_free(mbuf);
                            ret = -1;
                            goto _create_and_send_section_error;
                        }

                        rte_mbuf_ext_refcnt_update(shared_info, 0);

                        ext11.pExtBufShinfo = shared_info;

                        curExtInSect++;
                    }
                } /* if((category == XRAN_CATEGORY_B) && (pPrbElm->bf_weight_update)) */
                else
                {
                    mbuf = xran_ethdi_mbuf_alloc();

                    sect_geninfo[0].info->ef          = 0;
                    sect_geninfo[0].exDataSize       = 0;

                    if(pDevCtx->dssEnable == 1) {
                        populate_ext9_info(curExtInSect, tti, pDevCtx, &ext9);

                        sect_geninfo[0].info->ef       = 1;
                        curExtInSect++;
                    }
                }

                if(unlikely(mbuf == NULL))
                {
                    print_err("Alloc fail!\n");
                    ret=-1;
                    goto _create_and_send_section_error;
                }

                cpPktGenParams.numSections = 1;
                cpPktGenParams.sections    = sect_geninfo;

                seqId = xran_get_cp_seqid(pDevCtx, ((XRAN_DIR_DL == dir)? XRAN_DIR_DL : XRAN_DIR_UL), ccId, ruPortId);
                ret = xran_prepare_ctrl_pkt(mbuf, &cpPktGenParams, ccId, ruPortId, seqId, curSectId, mu, xranPortId);

            } /* if (generateCpPkt) */

            /* for application level fragmentation need to adjust the start/end PRB for U-plane here after generation of C-plane packet */
            sect_geninfo[curSectId].info->startPrbc = pPrbElm->UP_nRBStart;
            sect_geninfo[curSectId].info->numPrbc = pPrbElm->UP_nRBSize;

            if(unlikely(ret < 0))
            {
                print_err("Fail to build control plane packet - [%d:%d:%d] dir=%d\n",
                            frameId, sfId, slotId, dir);
            }
            else
            {
                if(pPrbElm->generateCpPkt) //only send actual new CP section
                {
                    int32_t cp_sent = 0;
                    int32_t pkt_len = 0;

                    /* add in the ethernet header */
                    struct rte_ether_hdr *const h = (void *)rte_pktmbuf_prepend(mbuf, sizeof(*h));
                    pkt_len = rte_pktmbuf_pkt_len(mbuf);
                    pCnt->tx_counter++;
                    pCnt->tx_bytes_counter += pkt_len; //rte_pktmbuf_pkt_len(mbuf);
                    if(pkt_len > pDevCtx->mtu)
                        rte_panic("section %d: pkt_len = %d maxExtBufSize %d\n", curPrbElm, pkt_len, pPrbElm->bf_weight.maxExtBufSize);

                    if(xran_get_syscfg_bbuoffload())
                    {
                        cp_sent = enqueue_cp_pkt_to_tx_sym_ring(ruPortId, ccId, vfId, tti, mu, pDevCtx, mbuf, dir, startSym);
                    }
                    else
                    {
                        cp_sent = pDevCtx->send_cpmbuf2ring(mbuf, ETHER_TYPE_ECPRI, vfId);
                    }

                    if(cp_sent != 1)
                    {
                        rte_pktmbuf_free(mbuf);
                    }
                }
            }
            curSectId++;
        } /* for (curPrbElm=0; curPrbElm<numPrbElm; curPrbElm++) */
    }
    else
    {
        int16_t startSectId=0;
        /* Generate a C-Plane message with multi sections,
        * a C-Plane message for each section*/
        if(prbMap)
        {
            if(0 == prbMap->nPrbElm)
            {
                print_dbg("prbMap->nPrbElm is %d\n",prbMap->nPrbElm);
                return 0;
            }

            numPrbElm = prbMap->nPrbElm;
            curPrbElm=0;
            if(XRAN_DIR_DL == dir)
            {
                prbElmProcInfo->numSymsRemaining = 0;
                prbElmProcInfo->nPrbElmProcessed = 0;
                prbElmProcInfo->nPrbElmPerSym = prbMap->nPrbElm;
                numPrbElm = prbMap->nPrbElm;
            } //dir = DL
            else
            {
                numPrbElm = prbMap->nPrbElm;
            } //dir = UL
        }
        else
        {
            print_err("prbMap is NULL\n");
            return (-1);
        }

        pPrbElm = &prbMap->prbMap[0];

        if(xran_fs_get_slot_type(xranPortId, ccId, tti, XRAN_SLOT_TYPE_FDD, mu) != 1
            && xran_fs_get_slot_type(xranPortId, ccId, tti, XRAN_SLOT_TYPE_SP, mu) == 1)
        {
            startSym = xran_check_symbolrange(
                                ((dir==XRAN_DIR_DL)?XRAN_SYMBOL_TYPE_DL:XRAN_SYMBOL_TYPE_UL),
                                xranPortId, ccId, tti,
                                pPrbElm->nStartSymb,
                                pPrbElm->numSymb, &numSyms, mu);

            if(startSym < 0 || numSyms == 0)
            {
                /* if start symbol is not valid, then skip this section */
                print_err("Skip section %d due to invalid symbol range - [%d:%d], [%d:%d]",
                            curPrbElm,
                            pPrbElm->nStartSymb, pPrbElm->numSymb,
                            startSym, numSyms);
            }
        }
        else
        {
            startSym    = pPrbElm->nStartSymb;
            numSyms     = pPrbElm->numSymb;
        }

        cpPktGenParams.dir                  = dir;
        cpPktGenParams.sectionType          = (useSectType3 ? XRAN_CP_SECTIONTYPE_3 : XRAN_CP_SECTIONTYPE_1);

        if((useSectType3 && dir == XRAN_DIR_UL && mu == XRAN_NBIOT_MU))
        {
            if(isNb375)
            {
                cpPktGenParams.hdr.filterIdx = XRAN_FILTERINDEX_NPUSCH_375;
            }
            else
            {
                cpPktGenParams.hdr.filterIdx = XRAN_FILTERINDEX_NPUSCH_15;
            }
        }
        else
        {
            cpPktGenParams.hdr.filterIdx = XRAN_FILTERINDEX_STANDARD;
        }

        cpPktGenParams.hdr.frameId          = frameId;
        cpPktGenParams.hdr.subframeId       = sfId;
        cpPktGenParams.hdr.slotId           = slotId;
        cpPktGenParams.hdr.startSymId       = startSym;
        cpPktGenParams.hdr.iqWidth          = pPrbElm->iqWidth;
        cpPktGenParams.hdr.compMeth         = pPrbElm->compMethod;
        cpPktGenParams.sections             = sect_geninfo;

        if(useSectType3){
            cpPktGenParams.sectionType          = XRAN_CP_SECTIONTYPE_3;
            cpPktGenParams.hdr.scs              = 1; /*Numerology parameter passed as parameter to this function, keeping 1 for now */
            cpPktGenParams.hdr.fftSize          = xran_get_conf_fftsize((void *)pDevCtx, mu);  /*Will be selected based on which numerology is getting processed*/
        }

        for (curPrbElm = 0, j = 0; j < numPrbElm; j++)
        {
            pPrbElm = &prbMap->prbMap[j];
            sect_geninfo[curPrbElm].exDataSize=0;
            sect_geninfo[curPrbElm].info = xran_cp_get_section_info_ptr(pDevCtx, dir, ccId, ruPortId, ctx_id, mu);
            if(sect_geninfo[curPrbElm].info == NULL)
            {
                rte_panic("xran_cp_get_section_info_ptr failed\n");
            }

            sect_geninfo[curPrbElm].info->prbElemBegin = ((j == 0 ) ?  1 : 0);
            sect_geninfo[curPrbElm].info->prbElemEnd   = ((j + 1 == numPrbElm) ?  1 : 0);


            sect_geninfo[curPrbElm].info->type        = (useSectType3 ? XRAN_CP_SECTIONTYPE_3 : XRAN_CP_SECTIONTYPE_1);
            sect_geninfo[curPrbElm].info->startSymId  = pPrbElm->nStartSymb;
            sect_geninfo[curPrbElm].info->iqWidth     = cpPktGenParams.hdr.iqWidth;
            sect_geninfo[curPrbElm].info->compMeth    = cpPktGenParams.hdr.compMeth;
            sect_geninfo[curPrbElm].info->id          = pPrbElm->startSectId;

            if(sect_geninfo[curPrbElm].info->id >= XRAN_MAX_SECTIONS_PER_SLOT)
            {
                print_err("sectinfo->id %d\n", sect_geninfo[curPrbElm].info->id);
                return -1;
            }

            sect_geninfo[curPrbElm].info->rb          = XRAN_RBIND_EVERY;
            sect_geninfo[curPrbElm].info->startPrbc   = pPrbElm->UP_nRBStart;
            sect_geninfo[curPrbElm].info->numPrbc     = pPrbElm->UP_nRBSize;
            sect_geninfo[curPrbElm].info->numSymbol   = pPrbElm->numSymb;
            sect_geninfo[curPrbElm].info->reMask      = (pPrbElm->reMask ? pPrbElm->reMask: 0xfff); /*Taking reMask from prbElm, setting to 0xfff if 0*/
            sect_geninfo[curPrbElm].info->beamId      = pPrbElm->nBeamIndex;

            if(startSym == pPrbElm->nStartSymb)
                sect_geninfo[curPrbElm].info->symInc  = XRAN_SYMBOLNUMBER_NOTINC;
            else
            {
                if((startSym + numSyms) == pPrbElm->nStartSymb)
                {
                    sect_geninfo[curPrbElm].info->symInc  = XRAN_SYMBOLNUMBER_INC;
                    startSym  =   pPrbElm->nStartSymb;
                    numSyms   =   pPrbElm->numSymb;
                }
                else
                {
                    sect_geninfo[curPrbElm].info->startSymId = startSym;
                    sect_geninfo[curPrbElm].info->numSymbol  = numSyms;
                    print_dbg("Last startSym is %d. Last numSyms is %d. But current pPrbElm->nStartSymb is %d.\n", startSym, numSyms, pPrbElm->nStartSymb);
                }
            }


            for(uint8_t locSym = 0; locSym < XRAN_NUM_OF_SYMBOL_PER_SLOT; locSym++)
            {
                struct xran_section_desc *pSecDesc =  &pPrbElm->sec_desc[locSym];
                if(pSecDesc)
                {
                    pSecDesc->section_id   = sect_geninfo[curPrbElm].info->id;

                    sect_geninfo[curPrbElm].info->sec_desc[locSym].iq_buffer_offset = pSecDesc->iq_buffer_offset;
                    sect_geninfo[curPrbElm].info->sec_desc[locSym].iq_buffer_len    = pSecDesc->iq_buffer_len;
                }
                else
                {
                    print_err("section desc is NULL\n");
                }
            }

            next = 0;
            sect_geninfo[curPrbElm].exDataSize       = 0;

          /* Extension 4 for modulation compression */
            if(pPrbElm->compMethod == XRAN_COMPMETHOD_MODULATION)
            {
                // print_dbg("[%s]:%d Modulation Compression need to verify for this code branch and may not be available\n");
                print_err("[%s]:%d Modulation Compression need to verify for this code branch and may not be available\n",__FUNCTION__, __LINE__);
            }
            /* Extension 1 or 11 for Beam forming weights */
            /* add section extention for BF Weights if update is needed */
            if((category == XRAN_CATEGORY_B) && (pPrbElm->bf_weight_update))
            {
                // print_dbg("[%s]:%d Category B need to verify for this code branch and may not be available\n");
                print_err("[%s]:%d Category B need to verify for this code branch and may not be available\n",__FUNCTION__, __LINE__);
            } /* if((category == XRAN_CATEGORY_B) && (pPrbElm->bf_weight_update)) */
            else
            {
                sect_geninfo[curPrbElm].info->ef          = 0;
                sect_geninfo[curPrbElm].exDataSize       = 0;

                if(pDevCtx->dssEnable == 1) {
                    uint8_t dssSlot = 0;
                    dssSlot = tti % (pDevCtx->dssPeriod);

                    ext9.technology = pDevCtx->technology[dssSlot];
                    ext9.reserved = 0;

                    sect_geninfo[curPrbElm].exData[next].type   = XRAN_CP_SECTIONEXTCMD_9;
                    sect_geninfo[curPrbElm].exData[next].len    = sizeof(ext9);
                    sect_geninfo[curPrbElm].exData[next].data   = &ext9;

                    sect_geninfo[curPrbElm].info->ef       = 1;
                    sect_geninfo[curPrbElm].exDataSize++;
                    next++;
                }
            }

            // xran_cp_add_section_info(pDevCtx, dir, ccId, ruPortId, ctx_id, &sect_geninfo[curPrbElm].info);

            if(pPrbElm->generateCpPkt == 1)
            {
                sect_geninfo[curPrbElm].info->startPrbc   = pPrbElm->nRBStart;
                sect_geninfo[curPrbElm].info->numPrbc     = pPrbElm->nRBSize;
                curPrbElm++;
            }
        }

        cpPktGenParams.numSections          = curPrbElm;

        mbuf = xran_ethdi_mbuf_alloc();
        if(unlikely(mbuf == NULL))
        {
            print_err("Alloc fail!\n");
            ret=-1;
            goto _create_and_send_section_error;
        }

        seqId = xran_get_cp_seqid(pDevCtx, ((XRAN_DIR_DL == dir)? XRAN_DIR_DL : XRAN_DIR_UL), ccId, ruPortId);
        ret = xran_prepare_ctrl_pkt(mbuf, &cpPktGenParams, ccId, ruPortId, seqId, startSectId, mu, xranPortId);

        if(ret < 0)
        {
            print_err("Fail to build control plane packet - [%d:%d:%d] dir=%d\n",
                        frameId, sfId, slotId, dir);
        }
        else
        {

            int32_t cp_sent = 0;
            int32_t pkt_len = 0;
            /* add in the ethernet header */
            struct rte_ether_hdr *const h = (void *)rte_pktmbuf_prepend(mbuf, sizeof(*h));
            pkt_len = rte_pktmbuf_pkt_len(mbuf);
            pCnt->tx_counter++;
            pCnt->tx_bytes_counter += pkt_len; //rte_pktmbuf_pkt_len(mbuf);
            if(pkt_len > pDevCtx->mtu)
                rte_panic("section %d: pkt_len = %d maxExtBufSize %d\n", curPrbElm, pkt_len, pPrbElm->bf_weight.maxExtBufSize);

            cp_sent = pDevCtx->send_cpmbuf2ring(mbuf, ETHER_TYPE_ECPRI, vfId);
            if(cp_sent != 1)
            {
                rte_pktmbuf_free(mbuf);
            }
        }

        struct xran_section_info *info;
        for (j = 0; j < numPrbElm; j++)
        {
            pPrbElm = &prbMap->prbMap[j];
            info = xran_cp_find_section_info(pDevCtx, dir, ccId, ruPortId, ctx_id, j, mu);
            if(info == NULL)
            {
                rte_panic("xran_cp_get_section_info_ptr failed\n");
            }
            info->startPrbc   = pPrbElm->UP_nRBStart;
            info->numPrbc     = pPrbElm->UP_nRBSize;
        }
    }
_create_and_send_section_error:
    if(XRAN_DIR_DL == dir)
    {
        prbElmProcInfo->nPrbElmProcessed = numPrbElm;
    }

    return ret;
} /* xran_cp_create_and_send_section */

int32_t
xran_ruemul_init(void *pHandle)
{
    uint16_t xran_port_id;
    struct xran_device_ctx* p_dev = NULL;

    if(pHandle) {
        p_dev = (struct xran_device_ctx* )pHandle;
        xran_port_id = p_dev->xran_port_id;
    } else {
        print_err("Invalid pHandle - %p", pHandle);
        return (XRAN_STATUS_FAIL);
    }

    if(xran_port_id < XRAN_PORTS_NUM) {
        if(recvSections[xran_port_id]) {
            print_err("Memory already allocated!");
            return (-1);
            }

        recvSections[xran_port_id] = malloc(sizeof(struct xran_section_recv_info) * XRAN_MAX_NUM_SECTIONS);
        if(recvSections[xran_port_id] == NULL) {
            print_err("Fail to allocate memory!");
            return (-1);
            }

        recvCpInfo[xran_port_id].sections = recvSections[xran_port_id];
    } else {
        print_err("Incorrect xran port %d\n", xran_port_id);
        return (-1);
    }


    return (0);
}

int32_t xran_ruemul_release(void *pHandle)
{
    uint16_t xran_port_id;
    struct xran_device_ctx* p_dev = NULL;

    if(pHandle)
    {
        p_dev = (struct xran_device_ctx* )pHandle;
        xran_port_id = p_dev->xran_port_id;
    }
    else
    {
        print_err("Invalid pHandle - %p", pHandle);
        return (XRAN_STATUS_FAIL);
    }

    if(xran_port_id < XRAN_PORTS_NUM)
    {
        if(recvSections[xran_port_id])
        {
            free(recvSections[xran_port_id]);
            recvSections[xran_port_id] = NULL;
            recvCpInfo[xran_port_id].sections = NULL;
        }
    }
    else
    {
        print_err("Incorrect xran port %d\n", xran_port_id);
        return (-1);
    }

    return (0);
}

