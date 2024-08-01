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
 * @brief XRAN layer common functionality for both O-DU and O-RU as well as C-plane and
 *    U-plane
 * @file xran_common.c
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <immintrin.h>
#include <rte_mbuf.h>
#include <stdio.h>
#include <stdbool.h>

#include "xran_common.h"
#include "xran_ethdi.h"
#include "xran_pkt.h"
#include "xran_pkt_up.h"
#include "xran_cp_api.h"
#include "xran_up_api.h"
#include "xran_cp_proc.h"
#include "xran_dev.h"
#include "xran_lib_mlog_tasks_id.h"
#include "xran_frame_struct.h"

#include "xran_printf.h"
#include "xran_mlog_lnx.h"
#include "xran_timer.h"


extern int32_t first_call;
extern uint16_t xran_getSfnSecStart(void);
#define MBUFS_CNT 16

extern int32_t xran_process_rx_sym(void *arg,
                        struct rte_mbuf *mbuf,
                        void *iq_data_start,
                        uint16_t size,
                        uint8_t CC_ID,
                        uint8_t Ant_ID,
                        uint8_t frame_id,
                        uint8_t subframe_id,
                        uint8_t slot_id,
                        uint8_t symb_id,
                        uint16_t num_prbu,
                        uint16_t start_prbu,
                        uint16_t sym_inc,
                        uint16_t rb,
                        uint16_t sect_id,
                        uint32_t *mb_free,
                        int8_t   expect_comp,
                        uint8_t compMeth,
                        uint8_t iqWidth,
                        uint8_t mu,
                        uint16_t nSectionIdx);


extern int xran_process_prach_sym(void *arg,
                        struct rte_mbuf *mbuf,
                        void *iq_data_start,
                        uint16_t size,
                        uint8_t CC_ID,
                        uint8_t Ant_ID,
                        uint8_t frame_id,
                        uint8_t subframe_id,
                        uint8_t slot_id,
                        uint8_t symb_id,
                        uint16_t num_prbu,
                        uint16_t start_prbu,
                        uint16_t sym_inc,
                        uint16_t rb,
                        uint16_t sect_id,
                        uint32_t *mb_free,
                        uint8_t mu);

extern int32_t xran_process_srs_sym(void *arg,
                        struct rte_mbuf *mbuf,
                        void *iq_data_start,
                        uint16_t size,
                        uint8_t CC_ID,
                        uint8_t Ant_ID,
                        uint8_t frame_id,
                        uint8_t subframe_id,
                        uint8_t slot_id,
                        uint8_t symb_id,
                        uint16_t num_prbu,
                        uint16_t start_prbu,
                        uint16_t sym_inc,
                        uint16_t rb,
                        uint16_t sect_id,
                        uint32_t *mb_free,
                        int8_t  expect_comp,
                        uint8_t compMeth,
                        uint8_t iqWidth,
                        uint8_t mu);

extern int32_t xran_process_csirs_sym(void *arg,
                        struct rte_mbuf *mbuf,
                        void *iq_data_start,
                        uint16_t size,
                        uint8_t CC_ID,
                        uint8_t Ant_ID,
                        uint8_t frame_id,
                        uint8_t subframe_id,
                        uint8_t slot_id,
                        uint8_t symb_id,
                        uint16_t num_prbu,
                        uint16_t start_prbu,
                        uint16_t sym_inc,
                        uint16_t rb,
                        uint16_t sect_id,
                        uint32_t *mb_free,
                        int8_t  expect_comp,
                        uint8_t compMeth,
                        uint8_t iqWidth,
                        uint8_t mu);

extern int32_t xran_validate_seq_id(void *arg,
                        uint8_t CC_ID,
                        uint8_t Ant_ID,
                        uint8_t slot_id,
                        union ecpri_seq_id *seq_id);

/* Today, we are using specific sectionId ranges for different numerologies.
 * sectionId allocation is totally upto DU and this approach avoids maintaining mapping and cycles to retrieve per section per packet
 * In case there is some restriction on sectionId usage, Following code should be enabled to maintain and derive the sectionId <--> mu mapping
 */

#if 0
inline int xran_get_mu_from_sect_id(struct xran_device_ctx *p_xran_dev_ctx, uint8_t frame_id,
        uint8_t subframe_id, uint8_t slot_id, uint16_t sect_id, uint8_t *mu)
{
    uint8_t sectIdMUmapEntry = 0;

    if(p_xran_dev_ctx->sectIdMUmap[0].entryActive && p_xran_dev_ctx->sectIdMUmap[0].frameId == frame_id &&
            p_xran_dev_ctx->sectIdMUmap[0].sfId == subframe_id)
    {
        sectIdMUmapEntry = 0;
    }
    else if(p_xran_dev_ctx->sectIdMUmap[1].entryActive && p_xran_dev_ctx->sectIdMUmap[1].frameId == frame_id &&
            p_xran_dev_ctx->sectIdMUmap[1].sfId == subframe_id)
    {
        sectIdMUmapEntry = 1;
    }
    else
    {
        print_err("sectId-mu maping not available forframeId=%u, sfId=%u\n", frame_id, subframe_id);
        return -1;
    }

    if(1 == p_xran_dev_ctx->fh_cfg.numMUs)
    {
        *mu = p_xran_dev_ctx->fh_cfg.mu_number[0];
    }
    else
    {
        *mu = p_xran_dev_ctx->sectIdMUmap[sectIdMUmapEntry].slotMap[slot_id].sectIdMu[sect_id];
    }

    return 0;
}
#endif

int32_t xran_validate_sectionId(void *arg, uint16_t mu)
{
    struct xran_device_ctx *p_dev_ctx = (struct xran_device_ctx *)arg;
    if(unlikely(mu!= p_dev_ctx->fh_cfg.mu_number[0]))
    {
        if(likely(p_dev_ctx->fh_cfg.numMUs == 1))
        {
            ++p_dev_ctx->fh_counters.rx_err_up;
            ++p_dev_ctx->fh_counters.rx_err_drop;
            print_dbg("Invalid SectionID - numerology mapping for mu = %d",mu);
            return XRAN_STATUS_FAIL;
        }
        else
        {
            uint8_t muIdx = 0;
            for(muIdx = 1; muIdx < p_dev_ctx->fh_cfg.numMUs; muIdx++)
            {
                if(p_dev_ctx->fh_cfg.mu_number[muIdx] == mu)
                    break;
            }
            if(muIdx == p_dev_ctx->fh_cfg.numMUs)
            {
                ++p_dev_ctx->fh_counters.rx_err_up;
                ++p_dev_ctx->fh_counters.rx_err_drop;
                print_dbg("Invalid SectionID - numerology mapping for mu = %d",mu);
                return XRAN_STATUS_FAIL;
            }
        }
    }
    return XRAN_STATUS_SUCCESS;
}

extern uint32_t xran_lib_ota_sym_idx_mu[];
static inline int xran_rx_timing_window_check(struct xran_device_ctx* p_dev_ctx, int tti, uint8_t symId, uint8_t mu, 
uint32_t pktFrameId, uint32_t pktSfId, uint32_t pktSlotId)
{
/* oran spec allows only 8-bit frameId. Hence max value of frameId that we can receive in a packet is 256.
   Hence we have to calculate max-sym-idx (i.e max number of symbols in frames for 256 frame here)
*/
#define MAX_SYM_IDX_FROM_PACKET(interval) (256*SLOTS_PER_SYSTEMFRAME(interval)*XRAN_NUM_OF_SYMBOL_PER_SLOT)

    int symIdxDeadlineMin, symIdxDeadlineMax=0;
    // int symIdxInPkt = tti * XRAN_NUM_OF_SYMBOL_PER_SLOT + symId;

#ifdef POLL_EBBU_OFFLOAD
    int otaSymId = xran_lib_ota_sym_idx_mu[mu] % XRAN_NUM_OF_SYMBOL_PER_SLOT;
    int32_t otaTti = (int32_t)XranGetTtiNum(pCtx->ebbu_offload_ota_sym_cnt_mu[mu], XRAN_NUM_OF_SYMBOL_PER_SLOT);
#else
    int otaSymId = xran_lib_ota_sym_idx_mu[mu] % XRAN_NUM_OF_SYMBOL_PER_SLOT;
    int32_t otaTti = (int32_t)XranGetTtiNum(xran_lib_ota_sym_idx_mu[mu], XRAN_NUM_OF_SYMBOL_PER_SLOT);
#endif
    int32_t interval = xran_fs_get_tti_interval(mu);
    uint32_t otaSlotId = XranGetSlotNum(otaTti, SLOTNUM_PER_SUBFRAME(interval));
    uint32_t otaSfId = XranGetSubFrameNum(otaTti, SLOTNUM_PER_SUBFRAME(interval), SUBFRAMES_PER_SYSTEMFRAME);
    uint32_t otaFrameId = XranGetFrameNum(otaTti, xran_getSfnSecStart(), SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval));
    otaFrameId &= 0xff;
    otaTti = otaFrameId * SLOTS_PER_SYSTEMFRAME(interval) + otaSfId * SLOTNUM_PER_SUBFRAME(interval) + otaSlotId;

    int otaSymIdx=0;
    otaSymIdx = otaTti  * XRAN_NUM_OF_SYMBOL_PER_SLOT + otaSymId;

    if (xran_get_syscfg_appmode() == O_DU)
    {
        if ((0 == otaTti) && (tti != 0))
        { // When received packet is of last tti of a sec
            otaSymIdx = MAX_SYM_IDX_FROM_PACKET(interval) + otaSymId;
        }
        symIdxDeadlineMax = tti * XRAN_NUM_OF_SYMBOL_PER_SLOT + symId + p_dev_ctx->perMu[mu].sym_up_ul_ub;
        symIdxDeadlineMin = tti * XRAN_NUM_OF_SYMBOL_PER_SLOT + symId + p_dev_ctx->perMu[mu].sym_up_ul_lb;
        if (symIdxDeadlineMax < otaSymIdx)
        {
            print_dbg("symUpUlUb=%d, {pktFId=%u, otaFId=%u}, {pktSfId=%u, otaSfId=%u}, \t {pktSlId=%u, otaSlId=%u}, {pktSym=%u, otaSym=%u}, {pktTti=%u, otaTti=%u},otaSymId = %d, xran_lib_ota_sym_idx_mu[mu] = %d\n",
                   p_dev_ctx->perMu[mu].sym_up_ul_ub, pktFrameId, otaFrameId, pktSfId, otaSfId,
                   pktSlotId, otaSlotId, symId, xran_lib_ota_sym_idx_mu[mu] % XRAN_NUM_OF_SYMBOL_PER_SLOT,
                   tti, otaTti,otaSymId, xran_lib_ota_sym_idx_mu[mu]);
            print_dbg("otaSymIdx=%d, symIdxDeadlineMax=%d, ttiPkt=%u, symPkt=%u, otaTti=%u",
                otaSymIdx, symIdxDeadlineMax, tti, symId, otaTti);
            ++p_dev_ctx->fh_counters.Rx_late;
            return -1;
        }
        else if(otaSymIdx < symIdxDeadlineMin){
            print_dbg("symUpUlLb=%d, {pktFId=%u, otaFId=%u}, {pktSfId=%u, otaSfId=%u}, \t {pktSlId=%u, otaSlId=%u}, {pktSym=%u, otaSym=%u}, {pktTti=%u, otaTti=%u},otaSymId = %d, xran_lib_ota_sym_idx_mu[mu] = %d\n",
                   p_dev_ctx->perMu[mu].sym_up_ul_lb, pktFrameId, otaFrameId, pktSfId, otaSfId,
                   pktSlotId, otaSlotId, symId, xran_lib_ota_sym_idx_mu[mu] % XRAN_NUM_OF_SYMBOL_PER_SLOT,
                   tti, otaTti,otaSymId, xran_lib_ota_sym_idx_mu[mu]);
            print_dbg("otaSymIdx=%d, symIdxDeadlineMin=%d, ttiPkt=%u, symPkt=%u, otaTti=%u",
                otaSymIdx, symIdxDeadlineMin, tti, symId, otaTti);
            ++p_dev_ctx->fh_counters.Rx_early;
            return -1;
        }
    }
    else
    {
        symIdxDeadlineMin = tti*XRAN_NUM_OF_SYMBOL_PER_SLOT + symId - p_dev_ctx->perMu[mu].ruRxSymUpMin;
        symIdxDeadlineMax = tti*XRAN_NUM_OF_SYMBOL_PER_SLOT + symId - p_dev_ctx->perMu[mu].ruRxSymUpMax;
        if ((0 == tti) && (otaTti != 0))
        {   // When received packet is of first tti of a sec with otaTti being  previous tti
            symIdxDeadlineMin += MAX_SYM_IDX_FROM_PACKET(interval);
            symIdxDeadlineMax += MAX_SYM_IDX_FROM_PACKET(interval);
        }
        /*Window for RU UP Rx: symIdxDeadlineMax < OTAsym < symIdxDeadlineMin*/
        if(symIdxDeadlineMax > otaSymIdx)
        {
            print_dbg("Rx RU: pktTti=%d, otaTti=%d, symIdxDeadlineMax=%d, otaSymIdx=%d, pktSym=%d, otaSym=%d \n", 
                tti, otaTti, symIdxDeadlineMax, otaSymIdx, symId, xran_lib_ota_sym_idx_mu[mu] % XRAN_NUM_OF_SYMBOL_PER_SLOT);
            ++p_dev_ctx->fh_counters.Rx_late;
            return -1;
        }
        else if(otaSymIdx > symIdxDeadlineMin){
            print_dbg("Rx RU: pktTti=%d, otaTti=%d, otaSymIdx=%d, symIdxDeadlineMin=%d, pktSym=%d, otaSym=%d \n", 
                tti, otaTti, otaSymIdx, symIdxDeadlineMin, symId, xran_lib_ota_sym_idx_mu[mu] % XRAN_NUM_OF_SYMBOL_PER_SLOT);
            ++p_dev_ctx->fh_counters.Rx_early;
            return -1;
        }
    }
    ++p_dev_ctx->fh_counters.Rx_on_time;
    return 0;
}


int32_t xran_validate_numprbs(uint8_t mu, uint16_t start_prbu, uint16_t num_prbs, struct xran_device_ctx *p_dev_ctx)
{
    uint16_t max_prbs;

    /* Validate start PRB and num PRB */
    if(xran_get_syscfg_appmode() == O_DU)
        max_prbs =  p_dev_ctx->fh_cfg.perMu[mu].nULRBs;
    else if(xran_get_syscfg_appmode() == O_RU)
        max_prbs = p_dev_ctx->fh_cfg.perMu[mu].nDLRBs;
    else
        return XRAN_STATUS_FAIL;

    if(unlikely( (start_prbu >= max_prbs) || (start_prbu + num_prbs > max_prbs) ))
    {
        ++p_dev_ctx->fh_counters.rx_err_up;
        ++p_dev_ctx->fh_counters.rx_err_drop;
        return XRAN_STATUS_FAIL;
    }

    return XRAN_STATUS_SUCCESS;
}

int32_t xran_validate_numprbs_prach(uint8_t mu, uint16_t start_prbu, uint16_t num_prbs, struct xran_device_ctx *p_dev_ctx)
{
    uint16_t numPrbc;

    numPrbc =  p_dev_ctx->perMu[mu].PrachCPConfig.numPrbc;
    if(unlikely(start_prbu + num_prbs > numPrbc))
    {
        ++p_dev_ctx->fh_counters.rx_err_up;
        ++p_dev_ctx->fh_counters.rx_err_drop;
        ++p_dev_ctx->fh_counters.rx_err_prach;
        return XRAN_STATUS_FAIL;
    }

    return XRAN_STATUS_SUCCESS;
}

int32_t xran_validate_numprbs_prach_lte(uint8_t mu, uint16_t start_prbu, uint16_t num_prbs, struct xran_device_ctx *p_dev_ctx)
{
    uint16_t numPrbc;

    numPrbc =  p_dev_ctx->perMu[mu].PrachCPConfig.numPrbc;
    if(unlikely(num_prbs != numPrbc))
    {
        ++p_dev_ctx->fh_counters.rx_err_up;
        ++p_dev_ctx->fh_counters.rx_err_drop;
        return XRAN_STATUS_FAIL;
    }

    return XRAN_STATUS_SUCCESS;
}

static int symbol_total_bytes_batch[XRAN_PORTS_NUM][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][0xFF] = { 0 };
/* TO DO: This function has multiple for loops one after the other, each touching the mbuf.
 * This might be inefficient as it could result in lot of cache populate -> evict -> miss cycles.
 * We should check if working on single mbuf at a time is more efficient.
 */
int process_mbuf_batch(struct rte_mbuf* pkt_q[], void* handle, int16_t num, struct xran_eaxc_info *p_cid, uint32_t* ret_data)
{
    uint64_t tt1 = MLogXRANTick();
    struct rte_mbuf* pkt;
    struct xran_device_ctx* p_dev_ctx = (struct xran_device_ctx*)handle;
    void* iq_samp_buf[MBUFS_CNT];
    union ecpri_seq_id seq[MBUFS_CNT];

    int num_bytes[MBUFS_CNT] = { 0 };
    int16_t i, j;

    uint16_t interval_us_local = 0;

    struct xran_common_counters* pCnt = &p_dev_ctx->fh_counters;

    uint8_t CC_ID[MBUFS_CNT] = { 0 };
    uint8_t Ant_ID[MBUFS_CNT] = { 0 };
    uint8_t frame_id[MBUFS_CNT] = { 0 };
    uint8_t subframe_id[MBUFS_CNT] = { 0 };
    uint8_t slot_id[MBUFS_CNT] = { 0 };
    uint8_t symb_id[MBUFS_CNT] = { 0 };

    uint16_t num_prbu[MBUFS_CNT];
    uint16_t start_prbu[MBUFS_CNT];
    uint16_t sym_inc[MBUFS_CNT];
    uint16_t rb[MBUFS_CNT];
    uint16_t sect_id[MBUFS_CNT];
    uint16_t prb_elem_id[MBUFS_CNT] = {0};

    uint8_t compMeth[MBUFS_CNT] = { 0 };
    uint8_t iqWidth[MBUFS_CNT] = { 0 };
    uint8_t mu[MBUFS_CNT] = {0};   /*TODOMIXED: logic to derive mu and identify NBIOT based on eAxCID*/
    uint8_t compMeth_ini = 0;
    uint8_t iqWidth_ini = 0;
    static uint32_t firstprint = 0;

    uint32_t pkt_size[MBUFS_CNT];

    int expect_comp = (p_dev_ctx->fh_cfg.ru_conf.compMeth != XRAN_COMPMETHOD_NONE);
    enum xran_comp_hdr_type staticComp = p_dev_ctx->fh_cfg.ru_conf.xranCompHdrType;

    int16_t num_pusch = 0, num_prach = 0, num_srs = 0, num_csirs = 0;
    int16_t pusch_idx[MBUFS_CNT] = { 0 }, prach_idx[MBUFS_CNT] = { 0 }, srs_idx[MBUFS_CNT] = { 0 }, csirs_idx[MBUFS_CNT] = { 0 };
    int8_t xran_port = xran_dev_ctx_get_port_id(p_dev_ctx);
    struct xran_eaxcid_config* conf;
    uint8_t seq_id[MBUFS_CNT];
    uint16_t cid[MBUFS_CNT];

    struct xran_ecpri_hdr* ecpri_hdr[MBUFS_CNT];
    struct radio_app_common_hdr* radio_hdr[MBUFS_CNT];
    struct data_section_hdr* data_hdr[MBUFS_CNT];
    struct data_section_compression_hdr* data_compr_hdr[MBUFS_CNT];

    const int16_t ecpri_size = sizeof(struct xran_ecpri_hdr);
    const int16_t rad_size = sizeof(struct radio_app_common_hdr);
    const int16_t data_size = sizeof(struct data_section_hdr);
    const int16_t compr_size = sizeof(struct data_section_compression_hdr);

    char* buf_start[MBUFS_CNT];
    uint16_t start_off[MBUFS_CNT];
    uint16_t iq_offset[MBUFS_CNT];
    uint16_t last[MBUFS_CNT];

    uint32_t tti = 0;
    struct rte_mbuf* mb = NULL;
    struct xran_prb_map* pRbMap = NULL;
    //uint16_t iq_sample_size_bits;
    uint16_t idxElm = 0;
    uint64_t t1;
//    uint16_t nSectorNum = 0;

#if XRAN_MLOG_VAR
    uint32_t mlogVar[10];
    uint32_t mlogVarCnt = 0;
#endif

    if (unlikely(xran_port < 0)) {
        print_err("Invalid pHandle");
        return MBUF_FREE;
    }

    if (unlikely(xran_port >= XRAN_PORTS_NUM)) {
        print_err("Invalid port - %d", xran_port);
        return MBUF_FREE;
    }

    if(first_call == 0) {
        for(i = 0; i < num; ++i )
            ret_data[i] = MBUF_FREE;
        return MBUF_FREE;
    }

    conf = &(p_dev_ctx->eAxc_id_cfg);
    if (unlikely(conf == NULL)) {
        rte_panic("conf == NULL");
    }

    if (staticComp == XRAN_COMP_HDR_TYPE_STATIC)
    {
        compMeth_ini = p_dev_ctx->fh_cfg.ru_conf.compMeth;
        iqWidth_ini = p_dev_ctx->fh_cfg.ru_conf.iqWidth;
    }

    for (i = 0; i < MBUFS_CNT; ++i)
    {
        pkt_size[i] = pkt_q[i]->pkt_len;
        buf_start[i] = (char*)pkt_q[i]->buf_addr;
        start_off[i] = pkt_q[i]->data_off;
    }

    bool countCompHdrBytes = expect_comp && (staticComp != XRAN_COMP_HDR_TYPE_STATIC);

    if (countCompHdrBytes)
    {
// #pragma vector always
        for (i = 0; i < MBUFS_CNT; ++i)
        {
#if XRAN_MLOG_VAR
            mlogVarCnt = 0;
#endif
            ecpri_hdr[i] = (void*)(buf_start[i] + start_off[i]);
            radio_hdr[i] = (void*)(buf_start[i] + start_off[i] + ecpri_size);
            data_hdr[i] = (void*)(buf_start[i] + start_off[i] + ecpri_size + rad_size);
            data_compr_hdr[i] = (void*)(buf_start[i] + start_off[i] + ecpri_size + rad_size + data_size);
            seq[i] = ecpri_hdr[i]->ecpri_seq_id;
            seq_id[i] = seq[i].bits.seq_id;
            last[i] = seq[i].bits.e_bit;

            iq_offset[i] = ecpri_size + rad_size + data_size + compr_size;

            iq_samp_buf[i] = (void*)(buf_start[i] + start_off[i] + iq_offset[i]);
            num_bytes[i] = pkt_size[i] - iq_offset[i];

            if (unlikely(ecpri_hdr[i] == NULL ||
                radio_hdr[i] == NULL ||
                data_hdr[i] == NULL ||
                data_compr_hdr[i] == NULL ||
                iq_samp_buf[i] == NULL))
            {
                num_bytes[i] = 0;       /* packet too short */
            }

#if XRAN_MLOG_VAR
            if(radio_hdr[i] != NULL && data_hdr[i] != NULL)
            {
                mlogVar[mlogVarCnt++] = 0xBBBBBBBB;
                mlogVar[mlogVarCnt++] = xran_lib_ota_tti_mu[PortId][mu];
                mlogVar[mlogVarCnt++] = radio_hdr[i]->frame_id;
                mlogVar[mlogVarCnt++] = radio_hdr[i]->sf_slot_sym.subframe_id;
                mlogVar[mlogVarCnt++] = radio_hdr[i]->sf_slot_sym.slot_id;
                mlogVar[mlogVarCnt++] = radio_hdr[i]->sf_slot_sym.symb_id;
                mlogVar[mlogVarCnt++] = data_hdr[i]->fields.sect_id;
                mlogVar[mlogVarCnt++] = data_hdr[i]->fields.start_prbu;
                mlogVar[mlogVarCnt++] = data_hdr[i]->fields.num_prbu;
                mlogVar[mlogVarCnt++] = rte_pktmbuf_pkt_len(pkt_q[i]);
                MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());
            }
#endif
        }
    }
    else
    {
// #pragma vector always
        for (i = 0; i < MBUFS_CNT; ++i)
        {
#if XRAN_MLOG_VAR
            mlogVarCnt = 0;
#endif
            ecpri_hdr[i] = (void*)(buf_start[i] + start_off[i]);
            radio_hdr[i] = (void*)(buf_start[i] + start_off[i] + ecpri_size);
            data_hdr[i] = (void*)(buf_start[i] + start_off[i] + ecpri_size + rad_size);
            seq[i] = ecpri_hdr[i]->ecpri_seq_id;
            seq_id[i] = seq[i].bits.seq_id;
            last[i] = seq[i].bits.e_bit;

            iq_offset[i] = ecpri_size + rad_size + data_size;
            iq_samp_buf[i] = (void*)(buf_start[i] + start_off[i] + iq_offset[i]);
            num_bytes[i] = pkt_size[i] - iq_offset[i];

            if (unlikely(ecpri_hdr[i] == NULL ||
                radio_hdr[i] == NULL ||
                data_hdr[i] == NULL ||
                iq_samp_buf[i] == NULL))
            {
                num_bytes[i] = 0;       /* packet too short */
            }

#if XRAN_MLOG_VAR
            if (radio_hdr[i] != NULL && data_hdr[i] != NULL)
            {
                mlogVar[mlogVarCnt++] = 0xBBBBBBBB;
                mlogVar[mlogVarCnt++] = xran_lib_ota_tti_mu[PortId][mu];
                mlogVar[mlogVarCnt++] = radio_hdr[i]->frame_id;
                mlogVar[mlogVarCnt++] = radio_hdr[i]->sf_slot_sym.subframe_id;
                mlogVar[mlogVarCnt++] = radio_hdr[i]->sf_slot_sym.slot_id;
                mlogVar[mlogVarCnt++] = radio_hdr[i]->sf_slot_sym.symb_id;
                mlogVar[mlogVarCnt++] = data_hdr[i]->fields.sect_id;
                mlogVar[mlogVarCnt++] = data_hdr[i]->fields.start_prbu;
                mlogVar[mlogVarCnt++] = data_hdr[i]->fields.num_prbu;
                mlogVar[mlogVarCnt++] = rte_pktmbuf_pkt_len(pkt_q[i]);
                MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());
            }
#endif
        }
    }

    for (i = 0; i < MBUFS_CNT; ++i) {
        if(p_cid->ccId == 0xFF && p_cid->ruPortId == 0xFF) {
            cid[i] = rte_be_to_cpu_16((uint16_t)ecpri_hdr[i]->ecpri_xtc_id);
            if (num_bytes[i] > 0) {
                CC_ID[i]  = (cid[i] & conf->mask_ccId) >> conf->bit_ccId;
                Ant_ID[i] = (cid[i] & conf->mask_ruPortId) >> conf->bit_ruPortId;
            }
        } else {
            if (num_bytes[i] > 0) {
                CC_ID[i]  = p_cid->ccId;
                Ant_ID[i] = p_cid->ruPortId;
            }
        }
    }

    for (i = 0; i < MBUFS_CNT; ++i)
    {
        radio_hdr[i]->sf_slot_sym.value = rte_be_to_cpu_16(radio_hdr[i]->sf_slot_sym.value);
        data_hdr[i]->fields.all_bits = rte_be_to_cpu_32(data_hdr[i]->fields.all_bits);
    }

    for (i = 0; i < MBUFS_CNT; ++i)
    {
        if (num_bytes[i] > 0)
        {
            compMeth[i] = compMeth_ini;
            iqWidth[i] = iqWidth_ini;

            frame_id[i] = radio_hdr[i]->frame_id;
            subframe_id[i] = radio_hdr[i]->sf_slot_sym.subframe_id;
            slot_id[i] = radio_hdr[i]->sf_slot_sym.slot_id;
            symb_id[i] = radio_hdr[i]->sf_slot_sym.symb_id;

            num_prbu[i] = data_hdr[i]->fields.num_prbu;
            start_prbu[i] = data_hdr[i]->fields.start_prbu;
            sym_inc[i] = data_hdr[i]->fields.sym_inc;
            rb[i] = data_hdr[i]->fields.rb;
            sect_id[i] = data_hdr[i]->fields.sect_id;
            mu[i] = XRAN_GET_MU_FROM_SECT_ID(sect_id[i]);
            sect_id[i] = XRAN_MU_SECT_ID_TO_BASE_SECT_ID(sect_id[i]);

            interval_us_local = xran_fs_get_tti_interval(mu[i]);
            tti = frame_id[i] * SLOTS_PER_SYSTEMFRAME(interval_us_local) +
                      subframe_id[i] * SLOTNUM_PER_SUBFRAME(interval_us_local) + slot_id[i];

            // TODO: Packets marked as invalid here should not be processed further
            if(unlikely(xran_validate_sectionId(p_dev_ctx, mu[i]) != XRAN_STATUS_SUCCESS))
                continue;

            /* Not checking timing constraints for SRS as NDM SRS spreads the transmission of packets. Marking them as on_time */
            if (xran_get_syscfg_appmode() == O_DU && p_dev_ctx->fh_cfg.srsEnable && Ant_ID[i] >= p_dev_ctx->srs_cfg.srsEaxcOffset
                && (Ant_ID[i]< (p_dev_ctx->srs_cfg.srsEaxcOffset + xran_get_num_ant_elm(p_dev_ctx)))){
                    ++p_dev_ctx->fh_counters.Rx_on_time;
            }
            else if (unlikely(-1 == xran_rx_timing_window_check(p_dev_ctx, tti, symb_id[i], mu[i], frame_id[i], subframe_id[i], slot_id[i]))
                && p_dev_ctx->fh_cfg.dropPacketsUp)
            {
                ret_data[i] = MBUF_FREE;
                continue;
            }

            if(unlikely(xran_validate_seq_id(handle,CC_ID[i],Ant_ID[i],slot_id[i],&seq[i]) != XRAN_STATUS_SUCCESS))
                continue;

            if (num_prbu[i] == 0)
                num_prbu[i] = p_dev_ctx->fh_cfg.perMu[mu[i]].nULRBs - start_prbu[i];

            if (countCompHdrBytes)
            {
                compMeth[i] = data_compr_hdr[i]->ud_comp_hdr.ud_comp_meth;
                iqWidth[i] = data_compr_hdr[i]->ud_comp_hdr.ud_iq_width;
            }
            /* Validate CC_ID */
            if(!xran_isactive_cc(p_dev_ctx, CC_ID[i]))
            {
                print_dbg("CC_ID(%d) in rx pkt is not activated.", CC_ID[i]);
                ++pCnt->rx_err_drop;
                ++pCnt->rx_err_up;
                continue;
            }

            struct xran_prach_cp_config *PrachCfg = NULL;
            if(p_dev_ctx->dssEnable)
            {
                int techSlot = (tti % p_dev_ctx->dssPeriod);
                if(p_dev_ctx->technology[techSlot] == 1)
                    PrachCfg  = &(p_dev_ctx->perMu[mu[i]].PrachCPConfig);
                else
                    PrachCfg  = &(p_dev_ctx->perMu[mu[i]].PrachCPConfigLTE);
            }
            else
            {
                PrachCfg = &(p_dev_ctx->perMu[mu[i]].PrachCPConfig);
            }
            if (xran_get_syscfg_appmode() == O_RU && Ant_ID[i] >= p_dev_ctx->csirs_cfg.csirsEaxcOffset && (Ant_ID[i]< (p_dev_ctx->csirs_cfg.csirsEaxcOffset + XRAN_MAX_CSIRS_PORTS))
                    && p_dev_ctx->fh_cfg.csirsEnable)
            {
                if(unlikely(xran_validate_numprbs(mu[i], start_prbu[i], num_prbu[i], p_dev_ctx) != XRAN_STATUS_SUCCESS))
                {
                    ++pCnt->rx_err_csirs;
                    continue;
                }
                Ant_ID[i] -= p_dev_ctx->csirs_cfg.csirsEaxcOffset;
                if (last[i] == 1)
                {
                    csirs_idx[num_csirs] = i;
                    num_csirs += 1;
                    ++pCnt->rx_csirs_packets;
                }
            }
            else if (Ant_ID[i] >= p_dev_ctx->srs_cfg.srsEaxcOffset && (Ant_ID[i]< (p_dev_ctx->srs_cfg.srsEaxcOffset+xran_get_num_ant_elm(p_dev_ctx)) )
                    && p_dev_ctx->fh_cfg.srsEnable)
            {
                if(unlikely(xran_validate_numprbs(mu[i], start_prbu[i], num_prbu[i], p_dev_ctx) != XRAN_STATUS_SUCCESS))
                {
                    ++pCnt->rx_err_srs;
                    continue;
                }

                Ant_ID[i] -= p_dev_ctx->srs_cfg.srsEaxcOffset;
                if (last[i] == 1)
                {
                    srs_idx[num_srs] = i;
                    num_srs += 1;
                    ++pCnt->rx_srs_packets;
                }
            }
            else if (Ant_ID[i] >= PrachCfg->prachEaxcOffset && Ant_ID[i] < (PrachCfg->prachEaxcOffset + xran_get_num_eAxc(p_dev_ctx))
                    && p_dev_ctx->fh_cfg.perMu[mu[i]].prachEnable)
            {
                if(p_dev_ctx->fh_cfg.ru_conf.xranTech == XRAN_RAN_5GNR)
                {
                    if(unlikely(xran_validate_numprbs_prach(mu[i], start_prbu[i], num_prbu[i], p_dev_ctx) != XRAN_STATUS_SUCCESS))
                        continue;
                }
                else if(p_dev_ctx->fh_cfg.ru_conf.xranTech == XRAN_RAN_LTE)
                {
                    if(unlikely(xran_validate_numprbs_prach_lte(mu[i], start_prbu[i], num_prbu[i], p_dev_ctx) != XRAN_STATUS_SUCCESS))
                        continue;
                }

                Ant_ID[i] -= PrachCfg->prachEaxcOffset;
                if (last[i] == 1)
                {
                    prach_idx[num_prach] = i;
                    num_prach += 1;
                    ++pCnt->rx_prach_packets[Ant_ID[i]];
                }
            }
            else
            {
                if(unlikely(xran_validate_numprbs(mu[i], start_prbu[i], num_prbu[i], p_dev_ctx) != XRAN_STATUS_SUCCESS))
                {
                    ++pCnt->rx_err_pusch;
                    continue;
                }
                if (last[i] == 1)
                {
                    pusch_idx[num_pusch] = i;
                    num_pusch += 1;
                    ++pCnt->rx_pusch_packets[Ant_ID[i]];
                }
            }
            symbol_total_bytes_batch[xran_port][CC_ID[i]][Ant_ID[i]][seq_id[i]] += num_bytes[i];
            if (last[i] == 1)
                symbol_total_bytes_batch[xran_port][CC_ID[i]][Ant_ID[i]][seq_id[i]] = 0;
        }
    }

    t1 = MLogXRANTick();
    MLogXRANTask(PID_PROC_UP_BATCH_PKT_PARSE, tt1, t1);
    for (j = 0; j < num_prach; j++)
    {
        i = prach_idx[j];
        pkt = pkt_q[i];

        print_dbg("Completed receiving PRACH symbol %d, size=%d bytes\n", symb_id[i], num_bytes[i]);

        xran_process_prach_sym(p_dev_ctx,
                pkt,
                iq_samp_buf[i],
                num_bytes[i],
                CC_ID[i],
                Ant_ID[i],
                frame_id[i],
                subframe_id[i],
                slot_id[i],
                symb_id[i],
                num_prbu[i],
                start_prbu[i],
                sym_inc[i],
                rb[i],
                sect_id[i],
                &ret_data[i],
                mu[i]);
    }
    if(num_prach)
        MLogXRANTask(PID_PROC_UP_BATCH_PKT_PRACH, t1, MLogXRANTick());
    t1 = MLogXRANTick();
    for (j = 0; j < num_srs; ++j)
    {
        i = srs_idx[j];
        pkt = pkt_q[i];

        print_dbg("SRS receiving symbol %d, size=%d bytes\n",
                symb_id[i], symbol_total_bytes_batch[p_dev_ctx->xran_port_id][CC_ID[i]][Ant_ID[i]][seq_id[i]]);

        xran_process_srs_sym(p_dev_ctx,
                pkt,
                iq_samp_buf[i],
                num_bytes[i],
                CC_ID[i],
                Ant_ID[i],
                frame_id[i],
                subframe_id[i],
                slot_id[i],
                symb_id[i],
                num_prbu[i],
                start_prbu[i],
                sym_inc[i],
                rb[i],
                sect_id[i],
                &ret_data[i],
                expect_comp,
                compMeth[i],
                iqWidth[i],
                mu[i]);
    }
    if(num_srs)
        MLogXRANTask(PID_PROC_UP_BATCH_PKT_SRS, t1, MLogXRANTick());

    t1 = MLogXRANTick();
    for (j = 0; j < num_csirs; ++j)
    {
        i = csirs_idx[j];
        pkt = pkt_q[i];

        print_dbg("CSIRS receiving symbol %d, size=%d bytes\n",symb_id[i],symbol_total_bytes_batch[p_dev_ctx->xran_port_id][CC_ID[i]][Ant_ID[i]][seq_id[i]]);

        xran_process_csirs_sym(p_dev_ctx,
                pkt,
                iq_samp_buf[i],
                num_bytes[i],
                CC_ID[i],
                Ant_ID[i],
                frame_id[i],
                subframe_id[i],
                slot_id[i],
                symb_id[i],
                num_prbu[i],
                start_prbu[i],
                sym_inc[i],
                rb[i],
                sect_id[i],
                &ret_data[i],
                expect_comp,
                compMeth[i],
                iqWidth[i],
                mu[i]);
    }
    if(num_csirs)
        MLogXRANTask(PID_PROCESS_UP_PKT_CSIRS, t1, MLogXRANTick());

    t1 = MLogXRANTick();
    for (j = 0; j < num_pusch; ++j)
    {
        uint32_t nSectionContinue;
        uint16_t nSectionIdx=0;
        uint16_t nSectionLen;
        i = pusch_idx[j];

        interval_us_local = xran_fs_get_tti_interval(mu[i]);
        Ant_ID[i] -= p_dev_ctx->perMu[mu[i]].eaxcOffset;
        tti = frame_id[i] * SLOTS_PER_SYSTEMFRAME(interval_us_local) + subframe_id[i] * SLOTNUM_PER_SUBFRAME(interval_us_local) + slot_id[i];

        pRbMap = (struct xran_prb_map*)p_dev_ctx->perMu[mu[i]].sFrontHaulRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID[i]][Ant_ID[i]].sBufferList.pBuffers->pData;
        ret_data[i] = MBUF_FREE;
        if (likely(pRbMap) && likely(p_dev_ctx->fh_cfg.ru_conf.byteOrder == XRAN_NE_BE_BYTE_ORDER))
        {
            struct xran_rx_packet_ctl *pFrontHaulRxPacketCtrl = &pRbMap->sFrontHaulRxPacketCtrl[symb_id[i]];

            do {
                nSectionContinue = 0;
                nSectionLen = (((iqWidth[i] == 0) ? 16 : iqWidth[i])*3 + ((compMeth[i] != XRAN_COMPMETHOD_NONE) ? 1 : 0))*num_prbu[i];
                if (num_bytes[i] < nSectionLen)
                {
                    break;
                }

                /** Get the prb_elem_id */
                for(idxElm=0 ; idxElm < pRbMap->nPrbElm ; ++idxElm)
                {
                    if(sect_id[i] == pRbMap->prbMap[idxElm].startSectId)
                    {
                        prb_elem_id[i] = idxElm;
                        break;
                    }
                }

                if (unlikely(prb_elem_id[i] >= pRbMap->nPrbElm))
                {
                    print_err("sect_id %d, prb_elem_id %d !=pRbMap->nPrbElm %d\n", sect_id[i], prb_elem_id[i], pRbMap->nPrbElm);
                    break;
                }

                int32_t npkts = pFrontHaulRxPacketCtrl->nRxPkt;
                if (unlikely(npkts >= XRAN_MAX_RX_PKT_PER_SYM))
                {
                    if (firstprint == 0) print_err("batch:(%d : %d : %d : %d)Received %d type-1 packets on symbol %u\n", frame_id[i], subframe_id[i], subframe_id[i], Ant_ID[i], npkts, symb_id[i]);
                    firstprint = 1;
                    break;
                }
                else
                {                
                    mb = pFrontHaulRxPacketCtrl->pCtrl[npkts];
                    if(mb){
                        rte_pktmbuf_free(mb);
                    }
                    pFrontHaulRxPacketCtrl->nRBStart[npkts] = start_prbu[i];
                    pFrontHaulRxPacketCtrl->nRBSize[npkts] = num_prbu[i];
                    pFrontHaulRxPacketCtrl->nSectid[npkts] = sect_id[i];
                    pFrontHaulRxPacketCtrl->pData[npkts] = iq_samp_buf[i];
                    pFrontHaulRxPacketCtrl->pCtrl[npkts] = (0 == nSectionIdx) ? pkt_q[i] : NULL;
                    pFrontHaulRxPacketCtrl->nRxPkt = npkts + 1;

                    if (0 == nSectionIdx) ret_data[i] = MBUF_KEEP;
 
                    if (num_bytes[i] > nSectionLen + sizeof(struct data_section_hdr))
                    {
                        data_hdr[i] = (struct data_section_hdr*)(((char *)iq_samp_buf[i]) + nSectionLen);
                        data_hdr[i]->fields.all_bits = rte_be_to_cpu_32(data_hdr[i]->fields.all_bits);
                        num_prbu[i] = data_hdr[i]->fields.num_prbu;
                        start_prbu[i] = data_hdr[i]->fields.start_prbu;
                        sym_inc[i] = data_hdr[i]->fields.sym_inc;
                        rb[i] = data_hdr[i]->fields.rb;
                        sect_id[i] = data_hdr[i]->fields.sect_id;

                        sect_id[i] = XRAN_MU_SECT_ID_TO_BASE_SECT_ID(sect_id[i]);
                        if (num_prbu[i] == 0)
                            num_prbu[i] = p_dev_ctx->fh_cfg.perMu[mu[i]].nULRBs - start_prbu[i];

                        if (countCompHdrBytes)
                        {
                            data_compr_hdr[i] = (struct data_section_compression_hdr *)((char *)(data_hdr[i]) + data_size);
                            compMeth[i] = data_compr_hdr[i]->ud_comp_hdr.ud_comp_meth;
                            iqWidth[i] = data_compr_hdr[i]->ud_comp_hdr.ud_iq_width;
                            num_bytes[i] -= (nSectionLen + data_size + compr_size);
                            iq_samp_buf[i] += (nSectionLen + data_size + compr_size);
                        }
                        else
                        {
                            num_bytes[i] -= (nSectionLen + data_size);
                            iq_samp_buf[i] += (nSectionLen + data_size);
                        }
                        nSectionContinue = 1;
                    }

                }
                nSectionIdx++;
            }while(nSectionContinue);
        }
    }
    MLogXRANTask(PID_PROC_UP_PKT_PUSCH, t1, MLogXRANTick());

    return MBUF_FREE;
}


static int symbol_total_bytes[XRAN_PORTS_NUM][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][0xFF] = {0};
int32_t process_mbuf(struct rte_mbuf *pkt, void* handle, struct xran_eaxc_info *p_cid)
{
    // uint64_t tt1 = MLogXRANTick();
    struct xran_device_ctx *p_dev_ctx = (struct xran_device_ctx *)handle;
    void *iq_samp_buf;
    union ecpri_seq_id seq;
    int num_bytes = 0;
    struct xran_common_counters *pCnt = &p_dev_ctx->fh_counters;
    struct xran_system_config *sysCfg = xran_get_systemcfg();
    uint16_t interval_us_local = 0;
    uint8_t mu=0;

    uint8_t CC_ID = p_cid->ccId;
    uint8_t Ant_ID = p_cid->ruPortId;
    uint8_t frame_id = 0;
    uint8_t subframe_id = 0;
    uint8_t slot_id = 0;
    uint8_t symb_id = 0;

    uint16_t num_prbu;
    uint16_t start_prbu;
    uint16_t sym_inc;
    uint16_t rb;
    uint16_t sect_id;
//    uint16_t nSectorNum = 0;

    uint8_t compMeth = 0;
    uint8_t iqWidth = 0;

    int ret = MBUF_FREE;
    uint32_t mb_free = 0;
    int32_t valid_res = 0;
    int expect_comp  = (p_dev_ctx->fh_cfg.ru_conf.compMeth != XRAN_COMPMETHOD_NONE);
    enum xran_comp_hdr_type staticComp = p_dev_ctx->fh_cfg.ru_conf.xranCompHdrType;

#ifdef POLL_EBBU_OFFLOAD
    PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();
    first_call = pCtx->first_call;
#endif

    if(first_call == 0)
        return ret;

    if (staticComp == XRAN_COMP_HDR_TYPE_STATIC)
    {
        compMeth = p_dev_ctx->fh_cfg.ru_conf.compMeth;
        iqWidth = p_dev_ctx->fh_cfg.ru_conf.iqWidth;
    }

    if(p_dev_ctx->xran2phy_mem_ready == 0 || first_call == 0)
        return MBUF_FREE;

    num_bytes = xran_extract_iq_samples(pkt, &iq_samp_buf,
                                &CC_ID, &Ant_ID, &frame_id, &subframe_id, &slot_id, &symb_id, &seq,
                                &num_prbu, &start_prbu, &sym_inc, &rb, &sect_id,
                                expect_comp, staticComp, &compMeth, &iqWidth, XRAN_GET_OXU_PORT_ID(p_dev_ctx));
    if (unlikely(num_bytes <= 0))
    {
        print_err("num_bytes is wrong [%d]\n", num_bytes);
        ++pCnt->rx_err_drop;
        ++pCnt->rx_err_up;
        return MBUF_FREE;
    }
    /* Validate CC_ID */
    if(!xran_isactive_cc(p_dev_ctx, CC_ID))
    {
        print_dbg("CC_ID(%d) in rx pkt is not activated.", CC_ID);
        ++pCnt->rx_err_drop;
        ++pCnt->rx_err_up;
        return MBUF_FREE;
    }

    mu = XRAN_GET_MU_FROM_SECT_ID(sect_id);
    sect_id = XRAN_MU_SECT_ID_TO_BASE_SECT_ID(sect_id);
    if (num_prbu == 0)
        num_prbu = (p_dev_ctx->fh_cfg.perMu[mu].nULRBs - start_prbu);

    if(unlikely(xran_validate_sectionId(p_dev_ctx,mu) == XRAN_STATUS_FAIL))
        return MBUF_FREE;

    interval_us_local = xran_fs_get_tti_interval(mu);

    int tti = frame_id * SLOTS_PER_SYSTEMFRAME(interval_us_local) +
                  subframe_id * SLOTNUM_PER_SUBFRAME(interval_us_local) + slot_id;

    /* Not checking timing constraints for SRS as NDM SRS spreads the transmission of packets. Marking them as on_time */
    if (xran_get_syscfg_appmode() == O_DU && p_dev_ctx->fh_cfg.srsEnable && Ant_ID >= p_dev_ctx->srs_cfg.srsEaxcOffset
        && (Ant_ID < p_dev_ctx->srs_cfg.srsEaxcOffset + xran_get_num_ant_elm(p_dev_ctx))){
            ++p_dev_ctx->fh_counters.Rx_on_time;
    }
    else if (unlikely(-1 == xran_rx_timing_window_check(p_dev_ctx, tti, symb_id, mu, frame_id, subframe_id, slot_id))
            && p_dev_ctx->fh_cfg.dropPacketsUp)
        return MBUF_FREE;

    valid_res = xran_validate_seq_id(p_dev_ctx, CC_ID, Ant_ID, slot_id, &seq);
    if(likely(sysCfg->rru_workaround == 0))
    {
        if(valid_res != 0)
        {
            print_dbg("valid_res is wrong [%d] ant %u (%u : %u : %u : %u) seq %u num_bytes %d\n", valid_res, Ant_ID, frame_id, subframe_id, slot_id, symb_id, seq.bits.seq_id, num_bytes);
            return MBUF_FREE;
        }
    }

    // MLogXRANTask(PID_PROCESS_UP_PKT_PARSE, tt1, MLogXRANTick());
    if(xran_get_syscfg_appmode() == O_RU
            && Ant_ID >= p_dev_ctx->csirs_cfg.csirsEaxcOffset
            && (Ant_ID < (p_dev_ctx->csirs_cfg.csirsEaxcOffset + XRAN_MAX_CSIRS_PORTS))
            && p_dev_ctx->fh_cfg.csirsEnable)
    {
        if(unlikely(xran_validate_numprbs(mu, start_prbu, num_prbu, p_dev_ctx) != XRAN_STATUS_SUCCESS))
            return MBUF_FREE;

        Ant_ID -= p_dev_ctx->csirs_cfg.csirsEaxcOffset;
        symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id] += num_bytes;

        if (seq.bits.e_bit == 1)
        {
            print_dbg("CSI-RS receiving symbol %d, size=%d bytes\n",
                symb_id, symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id]);

            if (symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id])
            {
                uint64_t t1 = MLogXRANTick();
                int16_t res = xran_process_csirs_sym(p_dev_ctx,
                                pkt, iq_samp_buf, num_bytes,
                                CC_ID, Ant_ID, frame_id, subframe_id, slot_id, symb_id,
                                num_prbu, start_prbu, sym_inc, rb, sect_id,
                                &mb_free, expect_comp, compMeth, iqWidth, mu);

                if(likely(res == symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id]))
                    ret = mb_free;
                else
                {
                    ++p_dev_ctx->fh_counters.rx_err_csirs;
                    ++p_dev_ctx->fh_counters.rx_err_up;
                    ++p_dev_ctx->fh_counters.rx_err_drop;
                    print_dbg("res != symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id]\n");
                }
                ++pCnt->rx_csirs_packets;
                MLogXRANTask(PID_PROCESS_UP_PKT_CSIRS, t1, MLogXRANTick());
            }
            symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id] = 0;
        }
        else
            print_dbg("Transport layer fragmentation (eCPRI) is not supported\n");
    }   /* if(xran_get_syscfg_appmode() == O_RU ...... */
    /* do not validate for NDM SRS */
    else if (Ant_ID >= p_dev_ctx->srs_cfg.srsEaxcOffset
                && (Ant_ID < p_dev_ctx->srs_cfg.srsEaxcOffset + xran_get_num_ant_elm(p_dev_ctx) )
                && p_dev_ctx->fh_cfg.srsEnable)
    {
        if(unlikely(xran_validate_numprbs(mu, start_prbu, num_prbu, p_dev_ctx) != XRAN_STATUS_SUCCESS))
            return MBUF_FREE;
        /* SRS packet has ruportid = 2*num_eAxc + ant_id */
        Ant_ID -= p_dev_ctx->srs_cfg.srsEaxcOffset;
        symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id] += num_bytes;

        if (seq.bits.e_bit == 1)
        {
            print_dbg("SRS receiving symbol %d, size=%d bytes\n",
                symb_id, symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id]);

            if (symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id])
            {
            //    uint64_t t1 = MLogXRANTick();
               int16_t res = xran_process_srs_sym(p_dev_ctx,
                                pkt, iq_samp_buf, num_bytes,
                                CC_ID, Ant_ID, frame_id, subframe_id, slot_id, symb_id,
                                num_prbu, start_prbu, sym_inc, rb, sect_id,
                                &mb_free, expect_comp, compMeth, iqWidth, mu);

                if(likely(res == symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id]))
                    ret = mb_free;
                else{
                    ++p_dev_ctx->fh_counters.rx_err_srs;
                    ++p_dev_ctx->fh_counters.rx_err_up;
                    ++p_dev_ctx->fh_counters.rx_err_drop;
                    print_dbg("res != symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id]\n");
                }
                ++pCnt->rx_srs_packets;
                // MLogXRANTask(PID_PROC_UP_PKT_SRS, tt1, MLogXRANTick());
            }
            symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id] = 0;
        }
        else
            print_dbg("Transport layer fragmentation (eCPRI) is not supported\n");
    } /* else if (Ant_ID >= p_dev_ctx->srs_cfg.srsEaxcOffset ...... */
    else
    {
        struct xran_prach_cp_config *PrachCfg = NULL;

        if(p_dev_ctx->dssEnable)
        {
            int techSlot = (tti % p_dev_ctx->dssPeriod);
            if(p_dev_ctx->technology[techSlot] == 1)
                PrachCfg  = &(p_dev_ctx->perMu[mu].PrachCPConfig);
            else
                PrachCfg  = &(p_dev_ctx->perMu[mu].PrachCPConfigLTE);
        }
        else
        {
            PrachCfg = &(p_dev_ctx->perMu[mu].PrachCPConfig);
        }

        if (Ant_ID >= PrachCfg->prachEaxcOffset
                && Ant_ID < (PrachCfg->prachEaxcOffset +xran_get_num_eAxc(p_dev_ctx))
                && p_dev_ctx->fh_cfg.perMu[mu].prachEnable)
        {
            if(p_dev_ctx->fh_cfg.ru_conf.xranTech == XRAN_RAN_5GNR)
            {
                if(unlikely(xran_validate_numprbs_prach(mu, start_prbu, num_prbu, p_dev_ctx) != XRAN_STATUS_SUCCESS))
                    return MBUF_FREE;
            }
            else if(p_dev_ctx->fh_cfg.ru_conf.xranTech == XRAN_RAN_LTE)
            {
                if(unlikely(xran_validate_numprbs_prach_lte(mu, start_prbu, num_prbu, p_dev_ctx) != XRAN_STATUS_SUCCESS))
                    return MBUF_FREE;
            }

            /* PRACH packet has ruportid = num_eAxc + ant_id */
            Ant_ID -= PrachCfg->prachEaxcOffset;
            symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id] += num_bytes;
            if (seq.bits.e_bit == 1)
            {
                print_dbg("Completed receiving PRACH symbol %d, size=%d bytes\n",
                    symb_id, num_bytes);

                if (symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id])
                {
                    int16_t res =  xran_process_prach_sym(p_dev_ctx,
                                                          pkt, iq_samp_buf, num_bytes,
                                                          CC_ID, Ant_ID, frame_id, subframe_id, slot_id, symb_id,
                                                          num_prbu, start_prbu, sym_inc, rb, sect_id, &mb_free, mu);

                    if(likely(res == symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id]))
                        ret = mb_free;
                    else
                    {
                        ++p_dev_ctx->fh_counters.rx_err_prach;
                        ++p_dev_ctx->fh_counters.rx_err_up;
                        ++p_dev_ctx->fh_counters.rx_err_drop;
                        print_err("res (%d) != num_bytes (%d)\n",res, num_bytes);
                    }
                    ++pCnt->rx_prach_packets[Ant_ID];
                }
                symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id] = 0;
            }
            else
                print_dbg("Transport layer fragmentation (eCPRI) is not supported\n");
        }
        else
        {
            /* PUSCH */
            uint32_t nSectionContinue;
            uint16_t nSectionIdx=0;
            uint16_t nSectionLen;
            if(unlikely(xran_validate_numprbs(mu, start_prbu, num_prbu, p_dev_ctx) != XRAN_STATUS_SUCCESS))
                return MBUF_FREE;

            symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id] += num_bytes;
            if (seq.bits.e_bit == 1)
            {
                print_dbg("Completed receiving symbol %d, size=%d bytes\n",
                    symb_id, symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id]);

                if (symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id])
                {
                    ret = MBUF_FREE;
                    do 
                    {
                        // uint64_t t1 = MLogXRANTick();
                        if(compMeth != XRAN_COMPMETHOD_MODULATION)   /* cannot calculate size for mod comp */
                        {
                            /* for non compression, U-plane does not have bitwidth w/o udComp */
                            if(expect_comp == 0 && iqWidth == 0)
                                iqWidth = p_dev_ctx->fh_cfg.ru_conf.iqWidth;
                            nSectionLen = (((iqWidth == 0) ? 16 : iqWidth)*3 + ((compMeth != XRAN_COMPMETHOD_NONE) ? 1 : 0))*num_prbu;
                            if(nSectionLen > num_bytes)
                            {
                                ++p_dev_ctx->fh_counters.rx_err_pusch;
                                ++p_dev_ctx->fh_counters.rx_err_up;
                                ++p_dev_ctx->fh_counters.rx_err_drop;
                                print_dbg("revByteNum > symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id]\n");
                                break;
                            }
                        }

                        xran_process_rx_sym(p_dev_ctx,
                                        pkt, iq_samp_buf, num_bytes,
                                        CC_ID, Ant_ID, frame_id, subframe_id, slot_id, symb_id,
                                        num_prbu, start_prbu, sym_inc, rb, sect_id,
                                        &mb_free, expect_comp, compMeth, iqWidth, mu, nSectionIdx);
                        if (0 == nSectionIdx)
                            ret = mb_free;
                        nSectionIdx++;
                        nSectionContinue = 0;

                        if(compMeth != XRAN_COMPMETHOD_MODULATION
                                && mb_free == MBUF_KEEP && nSectionLen < num_bytes)
                        {
                            //check have other sections
                            //move to next section
                            rte_pktmbuf_adj(pkt, nSectionLen);

                            //get section data header
                            num_bytes = xran_extract_iq_samples_dataheader(pkt, &iq_samp_buf,
                                                        &num_prbu, &start_prbu, &sym_inc, &rb, &sect_id,
                                                        expect_comp, staticComp, &compMeth, &iqWidth);
                            if (unlikely(num_bytes <= 0))
                            {
                                print_err("section num_bytes is wrong [%d]\n", num_bytes);
                                ++pCnt->rx_err_drop;
                                ++pCnt->rx_err_up;
                                break;
                            }

                            sect_id = XRAN_MU_SECT_ID_TO_BASE_SECT_ID(sect_id);
                            if (num_prbu == 0)
                                num_prbu = (p_dev_ctx->fh_cfg.perMu[mu].nULRBs - start_prbu);
                            
                            if(unlikely(xran_validate_numprbs(mu, start_prbu, num_prbu, p_dev_ctx) != XRAN_STATUS_SUCCESS))
                                break;

                            nSectionContinue = 1;

                        }
                        ++pCnt->rx_pusch_packets[Ant_ID];
                    }while(nSectionContinue);
                // MLogXRANTask(PID_PROC_UP_PKT_PUSCH, tt1, MLogXRANTick());
                }
                symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID][seq.bits.seq_id] = 0;
            }
            else
                print_dbg("Transport layer fragmentation (eCPRI) is not supported\n");
        }
    } /* else */

    return ret;
}

#if 0
static int set_iq_bit_width(uint8_t iq_bit_width, struct data_section_compression_hdr *compr_hdr)
{
    if (iq_bit_width == MAX_IQ_BIT_WIDTH)
        compr_hdr->ud_comp_hdr.ud_iq_width = (uint8_t) 0;
    else
        compr_hdr->ud_comp_hdr.ud_iq_width = iq_bit_width;

    return  0;

}
#endif

/* Send a single 5G symbol over multiple packets */
inline int32_t prepare_symbol_ex(enum xran_pkt_dir direction,
                uint16_t section_id_start,
                struct rte_mbuf *mb,
                uint8_t *data,
                uint8_t     compMeth,
                uint8_t     iqWidth,
                const enum xran_input_byte_order iq_buf_byte_order,
                uint8_t frame_id,
                uint8_t subframe_id,
                uint8_t slot_id,
                uint8_t symbol_no,
                int prb_start,
                int prb_num,
                uint8_t CC_ID,
                uint8_t RU_Port_ID,
                uint8_t seq_id,
                uint32_t do_copy,
                enum xran_comp_hdr_type staticEn,
                uint16_t num_sections,
                uint16_t iq_offset,
                uint8_t mu,
                bool isNb375,
                uint8_t oxu_port_id)
{
    int32_t n_bytes , iq_len_aggr = 0;
    int32_t prep_bytes;
    int16_t nPktSize,idx, nprb_per_section;
    uint32_t curr_sect_id;
    int parm_size;
    struct xran_up_pkt_gen_params xp[XRAN_MAX_SECTIONS_PER_SLOT] = { 0 };
    bool prbElemBegin , prbElemEnd;

    iqWidth = (iqWidth==0) ? 16 : iqWidth;
    switch(compMeth) {
        case XRAN_COMPMETHOD_BLKFLOAT:      parm_size = 1; break;
        case XRAN_COMPMETHOD_MODULATION:    parm_size = 0; break;
        default:
            parm_size = 0;
        }

    nprb_per_section = prb_num/num_sections;
    if(prb_num%num_sections)
        nprb_per_section++;

    if(isNb375)
    {
        n_bytes = 192; /* TODOMIXED: compression not supported */
    }
    else
    {
        n_bytes = (3 * iqWidth + parm_size)*nprb_per_section;
    }
    // n_bytes = RTE_MIN(n_bytes, XRAN_MAX_MBUF_LEN);

    for(idx=0 ; idx < num_sections ; idx++)
    {
        prbElemBegin = (idx == 0) ? 1 : 0;
        prbElemEnd   = (idx + 1 == num_sections) ? 1 : 0;
        curr_sect_id = section_id_start + idx ;

        iq_len_aggr += n_bytes;

        if(prbElemBegin)
        {
            nPktSize = sizeof(struct rte_ether_hdr)
                        + sizeof(struct xran_ecpri_hdr)
                        + sizeof(struct radio_app_common_hdr) ;
        }

        if(prbElemEnd){
            if(((idx+1)*nprb_per_section) > prb_num){
                nprb_per_section = (prb_num - idx*nprb_per_section);
            }
        }

        nPktSize += sizeof(struct data_section_hdr);

        if ((compMeth != XRAN_COMPMETHOD_NONE) && (staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC))
            nPktSize += sizeof(struct data_section_compression_hdr);

        nPktSize += n_bytes;

        /** radio app header
         *  Setting app_params is redundant , its needed only once to create common Radio app header.
        */
        xp[idx].app_params.data_feature.value = 0x10;
        xp[idx].app_params.data_feature.data_direction = direction;
        // xp[idx].app_params.payl_ver       = 1;
        // xp[idx].app_params.filter_id      = 0;
        xp[idx].app_params.frame_id       = frame_id;
        xp[idx].app_params.sf_slot_sym.subframe_id    = subframe_id;
        xp[idx].app_params.sf_slot_sym.slot_id        = xran_slotid_convert(slot_id, 0);
        xp[idx].app_params.sf_slot_sym.symb_id        = symbol_no;

        /* convert to network byte order */
        xp[idx].app_params.sf_slot_sym.value = rte_cpu_to_be_16(xp[idx].app_params.sf_slot_sym.value);

        // printf("start_prbu = %d, prb_num = %d,num_sections = %d, nprb_per_section = %d,curr_sect_id = %d\n",(prb_start + idx*nprb_per_section),prb_num,num_sections,nprb_per_section,curr_sect_id);
        xp[idx].sec_hdr.fields.all_bits   = 0;
        xp[idx].sec_hdr.fields.sect_id    = XRAN_BASE_SECT_ID_TO_MU_SECT_ID(curr_sect_id, mu);
        xp[idx].sec_hdr.fields.num_prbu   = XRAN_CONVERT_NUMPRBC(nprb_per_section); //(uint8_t)prb_num;
        xp[idx].sec_hdr.fields.start_prbu = prb_start;
        xp[idx].sec_hdr.fields.sym_inc    = 0;
        xp[idx].sec_hdr.fields.rb         = 0;

        /* compression */
        xp[idx].compr_hdr_param.ud_comp_hdr.ud_comp_meth = compMeth;
        xp[idx].compr_hdr_param.ud_comp_hdr.ud_iq_width  = XRAN_CONVERT_IQWIDTH(iqWidth);
        xp[idx].compr_hdr_param.rsrvd                    = 0;
        prb_start += nprb_per_section;

#if 0
        printf("\nidx %hu num_prbu %u sect_id %u start_prbu %u sym_inc %u curr_sec_id %u",idx,(uint32_t)xp[idx].sec_hdr.fields.num_prbu,
                                                                           (uint32_t)xp[idx].sec_hdr.fields.sect_id,
                                                                           (uint32_t)xp[idx].sec_hdr.fields.start_prbu,
                                                                           (uint32_t)xp[idx].sec_hdr.fields.sym_inc, curr_sect_id);

#endif

        /* network byte order */
        xp[idx].sec_hdr.fields.all_bits  = rte_cpu_to_be_32(xp[idx].sec_hdr.fields.all_bits);

        if (mb == NULL){
            MLogPrint(NULL);
            errx(1, "out of mbufs after %d packets", 1);
        }
    } /* for(idx=0 ; idx < num_sections ; idx++) */

    //printf("\niq_len_aggr %u",iq_len_aggr);

    prep_bytes = xran_prepare_iq_symbol_portion(mb,
                                                  data,
                                                  iq_buf_byte_order,
                                                  iq_len_aggr,
                                                  xp,
                                                  CC_ID,
                                                  RU_Port_ID,
                                                  seq_id,
                                                  staticEn,
                                                  do_copy,
                                                  num_sections,
                                                  section_id_start,
                                                  iq_offset,
                                                  oxu_port_id);
    if (prep_bytes <= 0)
        errx(1, "failed preparing symbol");

    rte_pktmbuf_pkt_len(mb)  = nPktSize;
    rte_pktmbuf_data_len(mb) = nPktSize;

#ifdef DEBUG
    printf("Symbol %2d prep_bytes (%d packets, %d bytes)\n", symbol_no, i, n_bytes);
#endif

    return prep_bytes;
}



/* Send a single 5G symbol over multiple packets */
inline int32_t prepare_symbol_ex_appand(enum xran_pkt_dir direction,
                uint16_t section_id_start,
                struct rte_mbuf *mb,
                uint8_t *data,
                uint8_t     compMeth,
                uint8_t     iqWidth,
                const enum xran_input_byte_order iq_buf_byte_order,
                uint8_t frame_id,
                uint8_t subframe_id,
                uint8_t slot_id,
                uint8_t symbol_no,
                int prb_start,
                int prb_num,
                uint8_t CC_ID,
                uint8_t RU_Port_ID,
                uint8_t seq_id,
                uint32_t do_copy,
                enum xran_comp_hdr_type staticEn,
                uint16_t num_sections,
                uint16_t iq_offset,
                uint8_t mu,
                bool isNb375,
                uint8_t oxu_port_id)
{
    int32_t n_bytes , iq_len_aggr = 0;
    int32_t prep_bytes;
    int16_t nPktSize,idx, nprb_per_section;
    uint32_t curr_sect_id;
    int parm_size;
    struct xran_up_pkt_gen_params xp[XRAN_MAX_SECTIONS_PER_SLOT] = { 0 };
    bool prbElemEnd;

    iqWidth = (iqWidth==0) ? 16 : iqWidth;
    switch(compMeth) {
        case XRAN_COMPMETHOD_BLKFLOAT:      parm_size = 1; break;
        case XRAN_COMPMETHOD_MODULATION:    parm_size = 0; break;
        default:
            parm_size = 0;
        }

    nprb_per_section = prb_num/num_sections;
    if(prb_num%num_sections)
        nprb_per_section++;

    if(isNb375)
    {
        n_bytes = 192; /* TODOMIXED: compression not supported */
    }
    else
    {
        n_bytes = (3 * iqWidth + parm_size)*nprb_per_section;
    }
    // n_bytes = RTE_MIN(n_bytes, XRAN_MAX_MBUF_LEN);

    nPktSize = 0;
    for(idx=0 ; idx < num_sections ; idx++)
    {
        prbElemEnd   = (idx + 1 == num_sections) ? 1 : 0;
        curr_sect_id = section_id_start + idx ;

        iq_len_aggr += n_bytes;

        if(prbElemEnd){
            if(((idx+1)*nprb_per_section) > prb_num){
                nprb_per_section = (prb_num - idx*nprb_per_section);
            }
        }

        nPktSize += sizeof(struct data_section_hdr);

        if ((compMeth != XRAN_COMPMETHOD_NONE) && (staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC))
            nPktSize += sizeof(struct data_section_compression_hdr);

        nPktSize += n_bytes;

        /** radio app header
         *  Setting app_params is redundant , its needed only once to create common Radio app header.
        */
        xp[idx].app_params.data_feature.value = 0x10;
        xp[idx].app_params.data_feature.data_direction = direction;
        // xp[idx].app_params.payl_ver       = 1;
        // xp[idx].app_params.filter_id      = 0;
        xp[idx].app_params.frame_id       = frame_id;
        xp[idx].app_params.sf_slot_sym.subframe_id    = subframe_id;
        xp[idx].app_params.sf_slot_sym.slot_id        = xran_slotid_convert(slot_id, 0);
        xp[idx].app_params.sf_slot_sym.symb_id        = symbol_no;

        /* convert to network byte order */
        xp[idx].app_params.sf_slot_sym.value = rte_cpu_to_be_16(xp[idx].app_params.sf_slot_sym.value);

        // printf("start_prbu = %d, prb_num = %d,num_sections = %d, nprb_per_section = %d,curr_sect_id = %d\n",(prb_start + idx*nprb_per_section),prb_num,num_sections,nprb_per_section,curr_sect_id);
        xp[idx].sec_hdr.fields.all_bits   = 0;
        xp[idx].sec_hdr.fields.sect_id    = XRAN_BASE_SECT_ID_TO_MU_SECT_ID(curr_sect_id, mu);
        xp[idx].sec_hdr.fields.num_prbu   = XRAN_CONVERT_NUMPRBC(nprb_per_section); //(uint8_t)prb_num;
        xp[idx].sec_hdr.fields.start_prbu = prb_start;
        xp[idx].sec_hdr.fields.sym_inc    = 0;
        xp[idx].sec_hdr.fields.rb         = 0;

        /* compression */
        xp[idx].compr_hdr_param.ud_comp_hdr.ud_comp_meth = compMeth;
        xp[idx].compr_hdr_param.ud_comp_hdr.ud_iq_width  = XRAN_CONVERT_IQWIDTH(iqWidth);
        xp[idx].compr_hdr_param.rsrvd                    = 0;
        prb_start += nprb_per_section;

#if 0
        printf("\nidx %hu num_prbu %u sect_id %u start_prbu %u sym_inc %u curr_sec_id %u",idx,(uint32_t)xp[idx].sec_hdr.fields.num_prbu,
                                                                           (uint32_t)xp[idx].sec_hdr.fields.sect_id,
                                                                           (uint32_t)xp[idx].sec_hdr.fields.start_prbu,
                                                                           (uint32_t)xp[idx].sec_hdr.fields.sym_inc, curr_sect_id);

#endif

        /* network byte order */
        xp[idx].sec_hdr.fields.all_bits  = rte_cpu_to_be_32(xp[idx].sec_hdr.fields.all_bits);

        if (mb == NULL){
            MLogPrint(NULL);
            errx(1, "out of mbufs after %d packets", 1);
        }
    } /* for(idx=0 ; idx < num_sections ; idx++) */

    //printf("\niq_len_aggr %u",iq_len_aggr);

    prep_bytes = xran_prepare_iq_symbol_portion_appand(mb,
                                                  data,
                                                  iq_buf_byte_order,
                                                  iq_len_aggr,
                                                  xp,
                                                  CC_ID,
                                                  RU_Port_ID,
                                                  seq_id,
                                                  staticEn,
                                                  do_copy,
                                                  num_sections,
                                                  section_id_start,
                                                  iq_offset,
                                                  oxu_port_id);
    if (prep_bytes <= 0)
        errx(1, "failed preparing symbol");

    rte_pktmbuf_pkt_len(mb)  += nPktSize;
    rte_pktmbuf_data_len(mb) += nPktSize;

#ifdef DEBUG
    printf("Symbol %2d prep_bytes (%d packets, %d bytes)\n", symbol_no, i, n_bytes);
#endif

    return prep_bytes;
}


int32_t prepare_sf_slot_sym (enum xran_pkt_dir direction,
                uint8_t frame_id,
                uint8_t subframe_id,
                uint8_t slot_id,
                uint8_t symbol_no,
                struct xran_up_pkt_gen_params *xp)
{
    /* radio app header */
    xp->app_params.data_feature.value = 0x10;
    xp->app_params.data_feature.data_direction = direction;
    //xp->app_params.payl_ver       = 1;
    //xp->app_params.filter_id      = 0;
    xp->app_params.frame_id       = frame_id;
    xp->app_params.sf_slot_sym.subframe_id    = subframe_id;
    xp->app_params.sf_slot_sym.slot_id        = xran_slotid_convert(slot_id, 0);
    xp->app_params.sf_slot_sym.symb_id        = symbol_no;

    /* convert to network byte order */
    xp->app_params.sf_slot_sym.value = rte_cpu_to_be_16(xp->app_params.sf_slot_sym.value);

    return 0;
}

int send_symbol_mult_section_ex(void *handle,
                enum xran_pkt_dir direction,
                uint16_t section_id,
                struct rte_mbuf *mb, uint8_t *data,
                uint8_t compMeth, uint8_t iqWidth,
                const enum xran_input_byte_order iq_buf_byte_order,
                uint8_t frame_id, uint8_t subframe_id,
                uint8_t slot_id, uint8_t symbol_no,
                int prb_start, int prb_num,
                void *pRing,
                uint16_t vf_id,
                uint8_t CC_ID, uint8_t RU_Port_ID, uint8_t seq_id, uint8_t mu)
{
    uint32_t do_copy = 0;
    int32_t n_bytes;
    int hdr_len, parm_size;
    int32_t sent=0;
    uint32_t loop = 0;
    struct xran_device_ctx *p_dev_ctx = (struct xran_device_ctx *)handle;
    struct xran_common_counters *pCnt = &p_dev_ctx->fh_counters;
    enum xran_comp_hdr_type staticEn= XRAN_COMP_HDR_TYPE_DYNAMIC;
    struct xran_ethdi_ctx* eth_ctx = xran_ethdi_get_ctx();
    struct rte_ring *ring = (struct rte_ring *)pRing;

    if (p_dev_ctx != NULL)
    {
        staticEn = p_dev_ctx->fh_cfg.ru_conf.xranCompHdrType;

    hdr_len = sizeof(struct xran_ecpri_hdr)
                + sizeof(struct radio_app_common_hdr)
                + sizeof(struct data_section_hdr);
        if ((compMeth != XRAN_COMPMETHOD_NONE)&&(staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC))
        hdr_len += sizeof(struct data_section_compression_hdr);

    switch(compMeth) {
        case XRAN_COMPMETHOD_BLKFLOAT:      parm_size = 1; break;
        case XRAN_COMPMETHOD_MODULATION:    parm_size = 0; break;
        default:
            parm_size = 0;
        }
    int prb_num_pre_sec = (prb_num+2)/3;
    int prb_offset = 0;
    int data_offset = 0;
    int prb_num_sec;
    struct rte_mbuf *send_mb;
    for (loop = 0; loop < 3;loop++)
    {
        seq_id = xran_get_upul_seqid(p_dev_ctx->xran_port_id, CC_ID, RU_Port_ID);

        prb_num_sec = ((loop+1)*prb_num_pre_sec > prb_num) ? (prb_num - loop*prb_num_pre_sec) : prb_num_pre_sec;
        n_bytes = (3 * iqWidth + parm_size) * prb_num_sec;
        char * pChar = NULL;

        send_mb = xran_ethdi_mbuf_alloc(); /* will be freede by ETH */
        if(send_mb ==  NULL) {
            MLogPrint(NULL);
            errx(1, "out of mbufs after %d packets", 1);
            }

        pChar = rte_pktmbuf_append(send_mb, hdr_len + n_bytes);
        if(pChar == NULL) {
            MLogPrint(NULL);
            errx(1, "incorrect mbuf size %d packets", 1);
            }
        pChar = rte_pktmbuf_prepend(send_mb, sizeof(struct rte_ether_hdr));
        if(pChar == NULL) {
            MLogPrint(NULL);
            errx(1, "incorrect mbuf size %d packets", 1);
            }
        do_copy = 1; /* new mbuf hence copy of IQs  */
        pChar = rte_pktmbuf_mtod(send_mb, char*);
        char *pdata_start = (pChar + sizeof(struct rte_ether_hdr) + hdr_len);
        memcpy(pdata_start,data  + data_offset,n_bytes);


        sent = prepare_symbol_ex(direction,
                             section_id,
                             send_mb,
                             data  + data_offset,
                             compMeth,
                             iqWidth,
                             iq_buf_byte_order,
                             frame_id,
                             subframe_id,
                             slot_id,
                             symbol_no,
                             prb_start+prb_offset,
                             prb_num_sec,
                             CC_ID,
                             RU_Port_ID,
                             seq_id,
                             do_copy,
                             staticEn,
                             1,
                             0, mu,false,
                             XRAN_GET_OXU_PORT_ID(p_dev_ctx)); /*Send a single section */
        prb_offset += prb_num_sec;
        data_offset += n_bytes;
        if(sent) {
            if(xran_get_syscfg_bbuoffload() == 1)
            {
                send_mb->port = eth_ctx->io_cfg.port[vf_id];
                xran_add_eth_hdr_vlan(&eth_ctx->entities[vf_id][ID_O_RU], ETHER_TYPE_ECPRI, send_mb);

                if(likely(ring)) {
                    if(rte_ring_enqueue(ring, (struct rte_mbuf *)send_mb)) {
                        rte_panic("Ring enqueue failed. Ring free count [%d]\n",rte_ring_free_count(ring));
                        rte_pktmbuf_free((struct rte_mbuf *)send_mb);
                    }
                } else
                    rte_panic("Ring is NULL\n");
            } else {
                p_dev_ctx->send_upmbuf2ring(send_mb, ETHER_TYPE_ECPRI, xran_map_ecpriPcid_to_vf(p_dev_ctx, direction, CC_ID, RU_Port_ID));
            }
            pCnt->tx_counter++;
            pCnt->tx_bytes_counter += rte_pktmbuf_pkt_len(send_mb);
            // p_dev_ctx->send_upmbuf2ring(send_mb, ETHER_TYPE_ECPRI, xran_map_ecpriPcid_to_vf(p_dev_ctx, direction, CC_ID, RU_Port_ID));
        }
     }

#ifdef DEBUG
    printf("Symbol %2d sent (%d packets, %d bytes)\n", symbol_no, i, n_bytes);
#endif
    }
    return sent;
}


/* Send a single 5G symbol over multiple packets */
int send_symbol_ex(void *handle,
                enum xran_pkt_dir direction,
                uint16_t section_id,
                struct rte_mbuf *mb, uint8_t *data,
                uint8_t compMeth, uint8_t iqWidth,
                const enum xran_input_byte_order iq_buf_byte_order,
                uint8_t frame_id, uint8_t subframe_id,
                uint8_t slot_id, uint8_t symbol_no,
                int prb_start, int prb_num,
                void *pRing,
                uint16_t vf_id,
                uint8_t CC_ID, uint8_t RU_Port_ID, uint8_t seq_id, uint8_t mu)
{
    uint32_t do_copy = 0;
    int32_t n_bytes;
    int hdr_len, parm_size;
    int32_t sent=0;
    struct xran_device_ctx *p_dev_ctx = (struct xran_device_ctx *)handle;
    struct xran_common_counters *pCnt = &p_dev_ctx->fh_counters;
    enum xran_comp_hdr_type staticEn= XRAN_COMP_HDR_TYPE_DYNAMIC;
    struct xran_ethdi_ctx* eth_ctx = xran_ethdi_get_ctx();
    struct rte_ring *ring = (struct rte_ring *)pRing;

    if (p_dev_ctx != NULL)
    {
        staticEn = p_dev_ctx->fh_cfg.ru_conf.xranCompHdrType;

    hdr_len = sizeof(struct xran_ecpri_hdr)
                + sizeof(struct radio_app_common_hdr)
                + sizeof(struct data_section_hdr);
        if ((compMeth != XRAN_COMPMETHOD_NONE)&&(staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC))
        hdr_len += sizeof(struct data_section_compression_hdr);

    switch(compMeth) {
        case XRAN_COMPMETHOD_BLKFLOAT:      parm_size = 1; break;
        case XRAN_COMPMETHOD_MODULATION:    parm_size = 0; break;
        default:
            parm_size = 0;
        }
    n_bytes = (3 * iqWidth + parm_size) * prb_num;

    if(mb == NULL) {
        char * pChar = NULL;
        mb = xran_ethdi_mbuf_alloc(); /* will be freede by ETH */
        if(mb ==  NULL) {
            MLogPrint(NULL);
            errx(1, "out of mbufs after %d packets", 1);
            }
        pChar = rte_pktmbuf_append(mb, hdr_len + n_bytes);
        if(pChar == NULL) {
            MLogPrint(NULL);
            errx(1, "incorrect mbuf size %d packets", 1);
            }
        pChar = rte_pktmbuf_prepend(mb, sizeof(struct rte_ether_hdr));
        if(pChar == NULL) {
            MLogPrint(NULL);
            errx(1, "incorrect mbuf size %d packets", 1);
            }
        do_copy = 1; /* new mbuf hence copy of IQs  */

        /**copy prach data start**/
        pChar = rte_pktmbuf_mtod(mb, char*);
        char *pdata_start = (pChar + sizeof(struct rte_ether_hdr) + hdr_len);
        memcpy(pdata_start,data,n_bytes);
        /**copy prach data end**/


        }
    else {
        rte_pktmbuf_refcnt_update(mb, 1); /* make sure eth won't free our mbuf */
        }

    sent = prepare_symbol_ex(direction,
                         section_id,
                         mb,
                         data,
                         compMeth,
                         iqWidth,
                         iq_buf_byte_order,
                         frame_id,
                         subframe_id,
                         slot_id,
                         symbol_no,
                         prb_start,
                         prb_num,
                         CC_ID,
                         RU_Port_ID,
                         seq_id,
                         do_copy,
                         staticEn,
                         1,
                         0,
                         mu, false,
                         XRAN_GET_OXU_PORT_ID(p_dev_ctx)); /*Send a single section */

    if(sent) {
        if(xran_get_syscfg_bbuoffload() == 1)
        {
            mb->port = eth_ctx->io_cfg.port[vf_id];
            xran_add_eth_hdr_vlan(&eth_ctx->entities[vf_id][ID_O_RU], ETHER_TYPE_ECPRI, mb);

            if(likely(ring))
            {
                if(rte_ring_enqueue(ring, (struct rte_mbuf *)mb))
                {
                    rte_panic("Ring enqueue failed. Ring free count [%d]\n",rte_ring_free_count(ring));
                    rte_pktmbuf_free((struct rte_mbuf *)mb);
                }
            }
            else
                rte_panic("Ring is NULL\n");
        }
        else
        {
            p_dev_ctx->send_upmbuf2ring(mb, ETHER_TYPE_ECPRI, xran_map_ecpriPcid_to_vf(p_dev_ctx, direction, CC_ID, RU_Port_ID));
        }
        pCnt->tx_counter++;
        pCnt->tx_bytes_counter += rte_pktmbuf_pkt_len(mb);
        // p_dev_ctx->send_upmbuf2ring(mb, ETHER_TYPE_ECPRI, xran_map_ecpriPcid_to_vf(p_dev_ctx, direction, CC_ID, RU_Port_ID));
        }

#ifdef DEBUG
    printf("Symbol %2d sent (%d packets, %d bytes)\n", symbol_no, i, n_bytes);
#endif
    }
    return sent;
}

int send_cpmsg(void *pHandle, struct rte_mbuf *mbuf,struct xran_cp_gen_params *params,
                struct xran_section_gen_info *sect_geninfo, uint8_t cc_id, uint8_t ru_port_id, uint8_t seq_id, uint8_t mu)
{
    int ret = 0; //, nsection;
    uint8_t dir = params->dir;
    struct xran_device_ctx *p_dev_ctx =(struct xran_device_ctx *) pHandle;
    struct xran_common_counters *pCnt = &p_dev_ctx->fh_counters;

    /* add in the ethernet header */
    struct rte_ether_hdr *const h = (void *)rte_pktmbuf_prepend(mbuf, sizeof(*h));

    pCnt->tx_counter++;
    pCnt->tx_bytes_counter += rte_pktmbuf_pkt_len(mbuf);
    p_dev_ctx->send_cpmbuf2ring(mbuf, ETHER_TYPE_ECPRI, xran_map_ecpriRtcid_to_vf(p_dev_ctx, dir, cc_id, ru_port_id));
#if 0
    for(i=0; i<nsection; i++)
        xran_cp_add_section_info(pHandle, dir, cc_id, ru_port_id,
                (slot_id + subframe_id*SLOTNUM_PER_SUBFRAME(xran_fs_get_tti_interval(mu)))%XRAN_MAX_SECTIONDB_CTX,
                sect_geninfo[i].info, mu);
#endif

    return (ret);
}

int process_ring(struct rte_ring *r, uint16_t ring_id, uint16_t q_id)
{
    assert(r);

    struct rte_mbuf *mbufs[MBUFS_CNT];
    uint32_t remaining;
    //uint64_t t1;
    const uint16_t dequeued = rte_ring_dequeue_burst(r, (void **)mbufs,
        RTE_DIM(mbufs), &remaining);

    if (!dequeued)
        return 0;

    //t1 = MLogTick();

    xran_ethdi_filter_packet(mbufs, ring_id, q_id, dequeued);
    //MLogTask(PID_PROC_UP_PKT_PUSCH, t1, MLogTick());

    return remaining;
}

/** FH RX AND BBDEV */
int32_t ring_processing_func(void* args)
{
    struct xran_ethdi_ctx *const ctx = xran_ethdi_get_ctx();
    int16_t retPoll = 0;
    int32_t i;
    queueid_t qi;
    uint64_t t1, t2;
    // int32_t port_id = 0;

#ifndef POLL_EBBU_OFFLOAD
    rte_timer_manage();
#endif
    if (ctx->bbdev_dec) {
        t1 = MLogXRANTick();
        retPoll = ctx->bbdev_dec();
        if (retPoll == 1)
        {
            t2 = MLogXRANTick();
            MLogXRANTask(PID_XRAN_BBDEV_UL_POLL + retPoll, t1, t2);
        }
    }

    if (ctx->bbdev_enc) {
        t1 = MLogXRANTick();
        retPoll = ctx->bbdev_enc();
        if (retPoll == 1)
        {
            t2 = MLogXRANTick();
            MLogXRANTask(PID_XRAN_BBDEV_DL_POLL + retPoll, t1, t2);
        }
    }

    if (ctx->bbdev_srs_fft) {
        t1 = MLogXRANTick();
        retPoll = ctx->bbdev_srs_fft();
        if (retPoll == 1)
        {
            t2 = MLogXRANTick();
            MLogXRANTask(PID_XRAN_BBDEV_SRS_FFT_POLL + retPoll, t1, t2);
        }
    }

    if (ctx->bbdev_prach_ifft) {
        t1 = MLogXRANTick();
        retPoll = ctx->bbdev_prach_ifft();
        if (retPoll == 1)
        {
            t2 = MLogXRANTick();
            MLogXRANTask(PID_XRAN_BBDEV_PRACH_IFFT_POLL + retPoll, t1, t2);
        }
    }

    //process_dpdk_io_port_id(2, 2);

    for (i = 0; i < ctx->io_cfg.num_vfs && i < XRAN_VF_MAX; i++){
        // if (ctx->vf2xran_port[i] == port_id ) {
            for(qi = 0; qi < ctx->rxq_per_port[i]; qi++)
            {
#ifndef POLL_EBBU_OFFLOAD
                rte_timer_manage();
#endif

                if (process_ring(ctx->rx_ring[i][qi], i, qi))
                    return 0;
            }
        // }
    }

    if (XRAN_STOPPED == xran_if_current_state)
        return -1;

    return 0;
}

/** FH RX AND BBDEV */
int32_t ring_processing_func2(void* args)
{
    struct xran_ethdi_ctx *const ctx = xran_ethdi_get_ctx();
    int16_t retPoll = 0;
    queueid_t qi;
    int i;
    uint64_t t1, t2;
    // int32_t port_id = 0;

#ifndef POLL_EBBU_OFFLOAD
    rte_timer_manage();
#endif
    if (ctx->bbdev_dec) {
        t1 = MLogXRANTick();
        retPoll = ctx->bbdev_dec();
        if (retPoll == 1)
        {
            t2 = MLogXRANTick();
            MLogXRANTask(PID_XRAN_BBDEV_UL_POLL + retPoll, t1, t2);
        }
    }

    if (ctx->bbdev_enc) {
        t1 = MLogXRANTick();
        retPoll = ctx->bbdev_enc();
        if (retPoll == 1)
        {
            t2 = MLogXRANTick();
            MLogXRANTask(PID_XRAN_BBDEV_DL_POLL + retPoll, t1, t2);
        }
    }

    if (ctx->bbdev_srs_fft) {
        t1 = MLogXRANTick();
        retPoll = ctx->bbdev_srs_fft();
        if (retPoll == 1)
        {
            t2 = MLogXRANTick();
            MLogXRANTask(PID_XRAN_BBDEV_SRS_FFT_POLL + retPoll, t1, t2);
        }
    }

    if (ctx->bbdev_prach_ifft) {
        t1 = MLogXRANTick();
        retPoll = ctx->bbdev_prach_ifft();
        if (retPoll == 1)
        {
            t2 = MLogXRANTick();
            MLogXRANTask(PID_XRAN_BBDEV_PRACH_IFFT_POLL + retPoll, t1, t2);
        }
    }

    for (i = 0; i < ctx->io_cfg.num_vfs && i < XRAN_VF_MAX; i++)
    {
        if(ctx->vf2xran_port[i] == 0)
        {
            for(qi = 0; qi < ctx->rxq_per_port[0]; qi++)
            {
#ifndef POLL_EBBU_OFFLOAD
                rte_timer_manage();
#endif
                process_ring(ctx->rx_ring[i][qi], i, qi);
            }
        }
    }

    if (XRAN_STOPPED == xran_if_current_state)
        return -1;

    return 0;
}

/** Generic thread to perform task on specific core */
int32_t xran_generic_worker_thread(void *args)
{
    int32_t res = 0;
    struct xran_worker_th_ctx* pThCtx = (struct xran_worker_th_ctx*)args;
    struct sched_param sched_param;
    struct xran_timing_source_ctx *pTmCtx;

    memset(&sched_param, 0, sizeof(struct sched_param));

    printf("Worker%d [CPU%2d] Starting (%s)\n", pThCtx->worker_id, rte_lcore_id(), pThCtx->worker_name);
    sched_param.sched_priority = XRAN_THREAD_DEFAULT_PRIO;
    if((res = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched_param)))
    {
        printf("priority is not changed: coreId = %d, result1 = %d\n",rte_lcore_id(), res);
    }
    pThCtx->worker_policy = SCHED_FIFO;
    if((res = pthread_setname_np(pthread_self(), pThCtx->worker_name)))
    {
        printf("[core %d] pthread_setname_np = %d\n",rte_lcore_id(), res);
    }

    pTmCtx  = xran_timingsource_get_ctx();
    for(;;)
    {
        if(pThCtx->task_func)
        {
            if(pThCtx->task_func(pThCtx->task_arg) != 0)
                break;
        }

        if(XRAN_STOPPED == xran_if_current_state)
            return -1;

        if(xran_get_syscfg_iosleep())
            nanosleep(&pTmCtx->sleeptime, NULL);
    }

    printf("Worker%d [CPU%2d] Finished (%s)\n", pThCtx->worker_id, rte_lcore_id(), pThCtx->worker_name);
    return 0;
}

int ring_processing_thread(void *args)
{
    struct sched_param sched_param;
    int res = 0;
    struct xran_timing_source_ctx *pTmCtx;

    memset(&sched_param, 0, sizeof(struct sched_param));

    printf("%s [CPU %2d] [PID: %6d]\n", __FUNCTION__,  rte_lcore_id(), getpid());
    sched_param.sched_priority = XRAN_THREAD_DEFAULT_PRIO;
    if((res = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched_param)))
    {
        printf("priority is not changed: coreId = %d, result1 = %d\n",rte_lcore_id(), res);
    }

    pTmCtx  = xran_timingsource_get_ctx();
    for(;;)
    {
        if(ring_processing_func(args) != 0)
            break;

        /* work around for some kernel */
        if(xran_get_syscfg_iosleep())
            nanosleep(&pTmCtx->sleeptime, NULL);
    }

    puts("Pkt processing thread finished.");
    return 0;
}

// extern uint32_t xran_lib_ota_sym_idx_mu[];
bool xran_check_if_late_transmission(uint32_t ttiForRing, uint32_t symIdForRing, uint8_t mu)
{
    uint32_t otaSymIdx = xran_lib_ota_sym_idx_mu[mu];
    uint32_t otaTti = XranGetTtiNum(otaSymIdx, XRAN_NUM_OF_SYMBOL_PER_SLOT) % XRAN_N_FE_BUF_LEN;
    int ttiDiff = ttiForRing - otaTti;

    if (0 == ttiDiff)
    {
        uint8_t otaSym = XranGetSymNum(otaSymIdx, XRAN_NUM_OF_SYMBOL_PER_SLOT);

        if (symIdForRing <= otaSym)
        {
            print_err("sym, tti Too late: otaTtiSym={%u,%u} forRingTtiSym={%u,%u}\n",
                   otaTti, otaSym, ttiForRing, symIdForRing);
            return true;
        }
        return false;
    }

    if (ttiDiff > 0)
    {
        if (ttiDiff > 2)
        { /* ota ahead and rolled over */
            print_err("tti Too late: otaTti=%u forRingTti=%u\n",
                   otaTti, ttiForRing);
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        ttiDiff *= (-1);
        if (ttiDiff > 2)
        { /* tti has rolled over */
            return false;
        }
        else
        {
            print_err("tti Too late: otaTti=%u forRingTti=%u\n",
                   otaTti, ttiForRing);
            return true;
        }
    }

    return true;
}

int32_t xran_timingsource_get_threadstate(void)
{
    return (int32_t)xran_timingsource_get_state();
}

extern inline int32_t xran_get_iqdata_len(uint16_t numRbs, uint8_t iqWidth, uint8_t compMeth);
