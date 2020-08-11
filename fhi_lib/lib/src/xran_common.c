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
 * @brief XRAN layer common functionality for both O-DU and O-RU as well as C-plane and
 *    U-plane
 * @file xran_common.c
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#include <assert.h>
#include <err.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>

#include "xran_common.h"
#include "ethdi.h"
#include "xran_pkt.h"
#include "xran_pkt_up.h"
#include "xran_up_api.h"
#include "xran_lib_mlog_tasks_id.h"

#include "../src/xran_printf.h"
#include <rte_mbuf.h>
#include "xran_mlog_lnx.h"

static struct timespec sleeptime = {.tv_nsec = 1E3 }; /* 1 us */

#define MBUFS_CNT 16

extern long interval_us;

extern int xran_process_rx_sym(void *arg,
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
                        uint32_t *mb_free);


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
                        uint32_t *mb_free);

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
                        uint32_t *mb_free);

extern int32_t xran_pkt_validate(void *arg,
                        struct rte_mbuf *mbuf,
                        void *iq_data_start,
                        uint16_t size,
                        uint8_t CC_ID,
                        uint8_t Ant_ID,
                        uint8_t frame_id,
                        uint8_t subframe_id,
                        uint8_t slot_id,
                        uint8_t symb_id,
                        struct ecpri_seq_id *seq_id,
                        uint16_t num_prbu,
                        uint16_t start_prbu,
                        uint16_t sym_inc,
                        uint16_t rb,
                        uint16_t sect_id);


struct cb_elem_entry *xran_create_cb(XranSymCallbackFn cb_fn, void *cb_data)
{
        struct cb_elem_entry * cb_elm = (struct cb_elem_entry *)malloc(sizeof(struct cb_elem_entry));
        if(cb_elm){
            cb_elm->pSymCallback    = cb_fn;
            cb_elm->pSymCallbackTag = cb_data;
        }

        return cb_elm;
}

int xran_destroy_cb(struct cb_elem_entry * cb_elm)
{
    if(cb_elm)
        free(cb_elm);
    return 0;
}

int process_mbuf(struct rte_mbuf *pkt)
{
    void *iq_samp_buf;
    struct ecpri_seq_id seq;
    static int symbol_total_bytes = 0;
    int num_bytes = 0;
    struct xran_device_ctx * p_x_ctx = xran_dev_get_ctx();
    struct xran_common_counters *pCnt = &p_x_ctx->fh_counters;

    uint8_t CC_ID = 0;
    uint8_t Ant_ID = 0;
    uint8_t frame_id = 0;
    uint8_t subframe_id = 0;
    uint8_t slot_id = 0;
    uint8_t symb_id = 0;

    uint16_t num_prbu;
    uint16_t start_prbu;
    uint16_t sym_inc;
    uint16_t rb;
    uint16_t sect_id;

    uint8_t compMeth = 0;
    uint8_t iqWidth = 0;

    void *pHandle = NULL;
    int ret = MBUF_FREE;
    uint32_t mb_free = 0;
    int32_t valid_res = 0;
    int expect_comp  = (p_x_ctx->fh_cfg.ru_conf.compMeth != XRAN_COMPMETHOD_NONE);


    if(p_x_ctx->xran2phy_mem_ready == 0)
        return MBUF_FREE;

    num_bytes = xran_extract_iq_samples(pkt,
                                        &iq_samp_buf,
                                        &CC_ID,
                                        &Ant_ID,
                                        &frame_id,
                                        &subframe_id,
                                        &slot_id,
                                        &symb_id,
                                        &seq,
                                        &num_prbu,
                                        &start_prbu,
                                        &sym_inc,
                                        &rb,
                                        &sect_id,
                                        expect_comp,
                                        &compMeth,
                                        &iqWidth);
    if (num_bytes <= 0){
        print_err("num_bytes is wrong [%d]\n", num_bytes);
        return MBUF_FREE;
    }

    valid_res = xran_pkt_validate(NULL,
                                pkt,
                                iq_samp_buf,
                                num_bytes,
                                CC_ID,
                                Ant_ID,
                                frame_id,
                                subframe_id,
                                slot_id,
                                symb_id,
                                &seq,
                                num_prbu,
                                start_prbu,
                                sym_inc,
                                rb,
                                sect_id);

    if(valid_res != 0) {
        print_dbg("valid_res is wrong [%d] ant %u (%u : %u : %u : %u) seq %u num_bytes %d\n", valid_res, Ant_ID, frame_id, subframe_id, slot_id, symb_id, seq.seq_id, num_bytes);
        return MBUF_FREE;
    }

    if (Ant_ID >= p_x_ctx->srs_cfg.eAxC_offset && p_x_ctx->fh_init.srsEnable) {
        /* SRS packet has ruportid = 2*num_eAxc + ant_id */
        Ant_ID -= p_x_ctx->srs_cfg.eAxC_offset;
        symbol_total_bytes += num_bytes;

        if (seq.e_bit == 1) {
            print_dbg("SRS receiving symbol %d, size=%d bytes\n",
                symb_id, symbol_total_bytes);

            if (symbol_total_bytes) {
               int16_t res = xran_process_srs_sym(NULL,
                                pkt,
                                iq_samp_buf,
                                num_bytes,
                                CC_ID,
                                Ant_ID,
                                frame_id,
                                subframe_id,
                                slot_id,
                                symb_id,
                                num_prbu,
                                start_prbu,
                                sym_inc,
                                rb,
                                sect_id,
                                &mb_free);

                if(res == symbol_total_bytes) {
                    ret = mb_free;
                } else {
                    print_err("res != symbol_total_bytes\n");
                }
                pCnt->rx_srs_packets++;
            }
            symbol_total_bytes = 0;
        }
        else {
            print_dbg("Transport layer fragmentation (eCPRI) is not supported\n");
        }

    } else if (Ant_ID >= p_x_ctx->PrachCPConfig.eAxC_offset && p_x_ctx->fh_init.prachEnable) {
        /* PRACH packet has ruportid = num_eAxc + ant_id */
        Ant_ID -= p_x_ctx->PrachCPConfig.eAxC_offset;
        symbol_total_bytes += num_bytes;
        if (seq.e_bit == 1) {
            print_dbg("Completed receiving PRACH symbol %d, size=%d bytes\n",
                symb_id, num_bytes);

            if (symbol_total_bytes) {
                int16_t res =  xran_process_prach_sym(NULL,
                                                      pkt,
                                                      iq_samp_buf,
                                                      num_bytes,
                                                      CC_ID,
                                                      Ant_ID,
                                                      frame_id,
                                                      subframe_id,
                                                      slot_id,
                                                      symb_id,
                                                      num_prbu,
                                                      start_prbu,
                                                      sym_inc,
                                                      rb,
                                                      sect_id,
                                                      &mb_free);
                if(res == symbol_total_bytes) {
                    ret = mb_free;
                } else {
                    print_err("res != symbol_total_bytes\n");
                }
                pCnt->rx_prach_packets[Ant_ID]++;
            }
            symbol_total_bytes = 0;
        } else {
            print_dbg("Transport layer fragmentation (eCPRI) is not supported\n");
        }

    } else { /* PUSCH */
        symbol_total_bytes += num_bytes;

        if (seq.e_bit == 1) {
            print_dbg("Completed receiving symbol %d, size=%d bytes\n",
                symb_id, symbol_total_bytes);

            if (symbol_total_bytes) {
                int res = xran_process_rx_sym(NULL,
                                pkt,
                                iq_samp_buf,
                                symbol_total_bytes,
                                CC_ID,
                                Ant_ID,
                                frame_id,
                                subframe_id,
                                slot_id,
                                symb_id,
                                num_prbu,
                                start_prbu,
                                sym_inc,
                                rb,
                                sect_id,
                                &mb_free);
                if(res == symbol_total_bytes) {
                    ret = mb_free;
                } else {
                    print_err("res != symbol_total_bytes\n");
                }
                pCnt->rx_pusch_packets[Ant_ID]++;
            }
            symbol_total_bytes = 0;
        } else {
            print_dbg("Transport layer fragmentation (eCPRI) is not supported\n");
        }
    }

    return ret;
}

static int set_iq_bit_width(uint8_t iq_bit_width, struct data_section_compression_hdr *compr_hdr)
{
    if (iq_bit_width == MAX_IQ_BIT_WIDTH)
        compr_hdr->ud_comp_hdr.ud_iq_width = (uint8_t) 0;
    else
        compr_hdr->ud_comp_hdr.ud_iq_width = iq_bit_width;

    return  0;

}

/* Send a single 5G symbol over multiple packets */
int32_t prepare_symbol_ex(enum xran_pkt_dir direction,
                uint16_t section_id,
                struct rte_mbuf *mb,
                struct rb_map *data,
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
                uint32_t do_copy)
{
    int32_t n_bytes = ((prb_num == 0) ? MAX_N_FULLBAND_SC : prb_num) * N_SC_PER_PRB * sizeof(struct rb_map);

    n_bytes  =   ((iqWidth == 0) || (iqWidth == 16)) ? n_bytes : ((3 * iqWidth + 1 ) * prb_num);

    int32_t prep_bytes;

    int16_t nPktSize = sizeof(struct rte_ether_hdr) + sizeof(struct xran_ecpri_hdr) +
            sizeof(struct radio_app_common_hdr)+ sizeof(struct data_section_hdr) + n_bytes;
    uint32_t off;
    struct xran_up_pkt_gen_params xp = { 0 };

    if(compMeth != XRAN_COMPMETHOD_NONE)
        nPktSize += sizeof(struct data_section_compression_hdr);

    n_bytes = RTE_MIN(n_bytes, XRAN_MAX_MBUF_LEN);

    /* radio app header */
    xp.app_params.data_direction = direction;
    xp.app_params.payl_ver       = 1;
    xp.app_params.filter_id      = 0;
    xp.app_params.frame_id       = frame_id;
    xp.app_params.sf_slot_sym.subframe_id    = subframe_id;
    xp.app_params.sf_slot_sym.slot_id        = xran_slotid_convert(slot_id, 0);
    xp.app_params.sf_slot_sym.symb_id        = symbol_no;

    /* convert to network byte order */
    xp.app_params.sf_slot_sym.value = rte_cpu_to_be_16(xp.app_params.sf_slot_sym.value);

    xp.sec_hdr.fields.sect_id    = section_id;
    xp.sec_hdr.fields.num_prbu   = (uint8_t)prb_num;
    xp.sec_hdr.fields.start_prbu = (uint8_t)prb_start;
    xp.sec_hdr.fields.sym_inc    = 0;
    xp.sec_hdr.fields.rb         = 0;
#ifdef FCN_ADAPT
    xp.sec_hdr.udCompHdr         = 0;
    xp.sec_hdr.reserved          = 0;
#endif

    /* compression */
    xp.compr_hdr_param.ud_comp_hdr.ud_comp_meth = compMeth;
    xp.compr_hdr_param.ud_comp_hdr.ud_iq_width  = iqWidth;
    xp.compr_hdr_param.rsrvd                    = 0;

    /* network byte order */
    xp.sec_hdr.fields.all_bits  = rte_cpu_to_be_32(xp.sec_hdr.fields.all_bits);

    if (mb == NULL){
        MLogPrint(NULL);
        errx(1, "out of mbufs after %d packets", 1);
    }

    prep_bytes = xran_prepare_iq_symbol_portion(mb,
                                                  data,
                                                  iq_buf_byte_order,
                                                  n_bytes,
                                                  &xp,
                                                  CC_ID,
                                                  RU_Port_ID,
                                                  seq_id,
                                                  do_copy);
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
int send_symbol_ex(enum xran_pkt_dir direction,
                uint16_t section_id,
                struct rte_mbuf *mb,
                struct rb_map *data,
                const enum xran_input_byte_order iq_buf_byte_order,
                uint8_t frame_id,
                uint8_t subframe_id,
                uint8_t slot_id,
                uint8_t symbol_no,
                int prb_start,
                int prb_num,
                uint8_t CC_ID,
                uint8_t RU_Port_ID,
                uint8_t seq_id)
{
    uint32_t do_copy = 0;
    int32_t n_bytes = ((prb_num == 0) ? MAX_N_FULLBAND_SC : prb_num) * N_SC_PER_PRB * sizeof(struct rb_map);
    struct xran_device_ctx *p_x_ctx = xran_dev_get_ctx();
    struct xran_common_counters *pCnt = &p_x_ctx->fh_counters;


    if (mb == NULL){
        char * pChar = NULL;
        mb = xran_ethdi_mbuf_alloc(); /* will be freede by ETH */
        if(mb ==  NULL){
            MLogPrint(NULL);
            errx(1, "out of mbufs after %d packets", 1);
        }
        pChar = rte_pktmbuf_append(mb, sizeof(struct xran_ecpri_hdr)+ sizeof(struct radio_app_common_hdr)+ sizeof(struct data_section_hdr) + n_bytes);
        if(pChar == NULL){
                MLogPrint(NULL);
                errx(1, "incorrect mbuf size %d packets", 1);
        }
        pChar = rte_pktmbuf_prepend(mb, sizeof(struct rte_ether_hdr));
        if(pChar == NULL){
                MLogPrint(NULL);
                errx(1, "incorrect mbuf size %d packets", 1);
        }
        do_copy = 1; /* new mbuf hence copy of IQs  */
    }else {
        rte_pktmbuf_refcnt_update(mb, 1); /* make sure eth won't free our mbuf */
    }

    int32_t sent = prepare_symbol_ex(direction,
                         section_id,
                         mb,
                         data,
                         0,
                         16,
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
                         do_copy);

    if(sent){
        pCnt->tx_counter++;
        pCnt->tx_bytes_counter += rte_pktmbuf_pkt_len(mb);
        p_x_ctx->send_upmbuf2ring(mb, ETHER_TYPE_ECPRI, xran_map_ecpriPcid_to_vf(direction, CC_ID, RU_Port_ID));
    } else {

    }

#ifdef DEBUG
    printf("Symbol %2d sent (%d packets, %d bytes)\n", symbol_no, i, n_bytes);
#endif

    return sent;
}

int send_cpmsg(void *pHandle, struct rte_mbuf *mbuf,struct xran_cp_gen_params *params,
                struct xran_section_gen_info *sect_geninfo, uint8_t cc_id, uint8_t ru_port_id, uint8_t seq_id)
{
    int ret = 0, nsection, i;
    uint8_t subframe_id = params->hdr.subframeId;
    uint8_t slot_id = params->hdr.slotId;
    uint8_t dir = params->dir;
    struct xran_device_ctx *p_x_ctx = xran_dev_get_ctx();
    struct xran_common_counters *pCnt = &p_x_ctx->fh_counters;

    nsection = params->numSections;

    /* add in the ethernet header */
    struct rte_ether_hdr *const h = (void *)rte_pktmbuf_prepend(mbuf, sizeof(*h));

    pCnt->tx_counter++;
    pCnt->tx_bytes_counter += rte_pktmbuf_pkt_len(mbuf);
    p_x_ctx->send_cpmbuf2ring(mbuf, ETHER_TYPE_ECPRI, xran_map_ecpriRtcid_to_vf(dir, cc_id, ru_port_id));
    for(i=0; i<nsection; i++)
        xran_cp_add_section_info(pHandle, dir, cc_id, ru_port_id,
                (slot_id + subframe_id*SLOTNUM_PER_SUBFRAME)%XRAN_MAX_SECTIONDB_CTX,
                &sect_geninfo[i].info);

    return (ret);
}

int generate_cpmsg_dlul(void *pHandle, struct xran_cp_gen_params *params, struct xran_section_gen_info *sect_geninfo, struct rte_mbuf *mbuf,
    enum xran_pkt_dir dir, uint8_t frame_id, uint8_t subframe_id, uint8_t slot_id,
    uint8_t startsym, uint8_t numsym, uint16_t prb_start, uint16_t prb_num,int16_t iq_buffer_offset, int16_t iq_buffer_len,
    uint16_t beam_id, uint8_t cc_id, uint8_t ru_port_id, uint8_t comp_method, uint8_t iqWidth,  uint8_t seq_id, uint8_t symInc)
{
    int ret = 0, nsection, loc_sym;


    params->dir                  = dir;
    params->sectionType          = XRAN_CP_SECTIONTYPE_1;        // Most DL/UL Radio Channels
    params->hdr.filterIdx        = XRAN_FILTERINDEX_STANDARD;
    params->hdr.frameId          = frame_id;
    params->hdr.subframeId       = subframe_id;
    params->hdr.slotId           = slot_id;
    params->hdr.startSymId       = startsym;                     // start Symbol ID
    params->hdr.iqWidth          = iqWidth;
    params->hdr.compMeth         = comp_method;

    nsection = 0;
    sect_geninfo[nsection].info.type        = params->sectionType;       // for database
    sect_geninfo[nsection].info.startSymId  = params->hdr.startSymId;    // for database
    sect_geninfo[nsection].info.iqWidth     = params->hdr.iqWidth;       // for database
    sect_geninfo[nsection].info.compMeth    = params->hdr.compMeth;      // for database
    sect_geninfo[nsection].info.id          = xran_alloc_sectionid(pHandle, dir, cc_id, ru_port_id, slot_id);
    sect_geninfo[nsection].info.rb          = XRAN_RBIND_EVERY;
    sect_geninfo[nsection].info.symInc      = symInc;
    sect_geninfo[nsection].info.startPrbc   = prb_start;
    sect_geninfo[nsection].info.numPrbc     = prb_num;
    sect_geninfo[nsection].info.numSymbol   = numsym;
    sect_geninfo[nsection].info.reMask      = 0xfff;
    sect_geninfo[nsection].info.beamId      = beam_id;

    for (loc_sym = 0; loc_sym < XRAN_NUM_OF_SYMBOL_PER_SLOT; loc_sym++) {
        sect_geninfo[0].info.sec_desc[loc_sym].iq_buffer_offset = iq_buffer_offset;
        sect_geninfo[0].info.sec_desc[loc_sym].iq_buffer_len    = iq_buffer_len;
    }

    sect_geninfo[nsection].info.ef          = 0;
    sect_geninfo[nsection].exDataSize       = 0;
//    sect_geninfo[nsection].exData           = NULL;
    nsection++;

    params->numSections          = nsection;
    params->sections             = sect_geninfo;

    if(unlikely(mbuf == NULL)) {
        print_err("Alloc fail!\n");
        return (-1);
    }

    ret = xran_prepare_ctrl_pkt(mbuf, params, cc_id, ru_port_id, seq_id);
    if(ret < 0){
        print_err("Fail to build control plane packet - [%d:%d:%d] dir=%d\n",
                    frame_id, subframe_id, slot_id, dir);
        rte_pktmbuf_free(mbuf);
    }

    return (ret);
}

int generate_cpmsg_prach(void *pHandle, struct xran_cp_gen_params *params, struct xran_section_gen_info *sect_geninfo, struct rte_mbuf *mbuf, struct xran_device_ctx *pxran_lib_ctx,
                uint8_t frame_id, uint8_t subframe_id, uint8_t slot_id,
                uint16_t beam_id, uint8_t cc_id, uint8_t prach_port_id, uint8_t seq_id)
{
    int nsection, ret;
    struct xran_prach_cp_config  *pPrachCPConfig = &(pxran_lib_ctx->PrachCPConfig);
    uint16_t timeOffset;
    uint16_t nNumerology = pxran_lib_ctx->fh_cfg.frame_conf.nNumerology;

    if(unlikely(mbuf == NULL)) {
        print_err("Alloc fail!\n");
        return (-1);
    }
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
    timeOffset = pPrachCPConfig->timeOffset; //this is the CP value per 38.211 tab 6.3.3.1-1&2
    timeOffset = timeOffset >> nNumerology; //original number is Tc, convert to Ts based on mu
    if (pPrachCPConfig->startSymId > 0)
    {
        timeOffset += (pPrachCPConfig->startSymId * 2048) >> nNumerology;
        if ((slot_id == 0) || (slot_id == (SLOTNUM_PER_SUBFRAME >> 1)))
            timeOffset += 16;
    }
    params->dir                  = XRAN_DIR_UL;
    params->sectionType          = XRAN_CP_SECTIONTYPE_3;
    params->hdr.filterIdx        = pPrachCPConfig->filterIdx;
    params->hdr.frameId          = frame_id;
    params->hdr.subframeId       = subframe_id;
    params->hdr.slotId           = slot_id;
    params->hdr.startSymId       = pPrachCPConfig->startSymId;
    params->hdr.iqWidth          = xran_get_conf_iqwidth(pHandle);
    params->hdr.compMeth         = xran_get_conf_compmethod(pHandle);
        /* use timeOffset field for the CP length value for prach sequence */
    params->hdr.timeOffset       = timeOffset;
    params->hdr.fftSize          = xran_get_conf_fftsize(pHandle);
    params->hdr.scs              = xran_get_conf_prach_scs(pHandle);
    params->hdr.cpLength         = 0;

    nsection = 0;
    sect_geninfo[nsection].info.type        = params->sectionType;       // for database
    sect_geninfo[nsection].info.startSymId  = params->hdr.startSymId;    // for database
    sect_geninfo[nsection].info.iqWidth     = params->hdr.iqWidth;       // for database
    sect_geninfo[nsection].info.compMeth    = params->hdr.compMeth;      // for database
    sect_geninfo[nsection].info.id          = xran_alloc_sectionid(pHandle, XRAN_DIR_UL, cc_id, prach_port_id, slot_id);
    sect_geninfo[nsection].info.rb          = XRAN_RBIND_EVERY;
    sect_geninfo[nsection].info.symInc      = XRAN_SYMBOLNUMBER_NOTINC;
    sect_geninfo[nsection].info.startPrbc   = pPrachCPConfig->startPrbc;
    sect_geninfo[nsection].info.numPrbc     = pPrachCPConfig->numPrbc,
    sect_geninfo[nsection].info.numSymbol   = pPrachCPConfig->numSymbol*pPrachCPConfig->occassionsInPrachSlot;
    sect_geninfo[nsection].info.reMask      = 0xfff;
    sect_geninfo[nsection].info.beamId      = beam_id;
    sect_geninfo[nsection].info.freqOffset  = pPrachCPConfig->freqOffset;

    pxran_lib_ctx->prach_last_symbol[cc_id] = sect_geninfo[nsection].info.startSymId + sect_geninfo[nsection].info.numSymbol - 1;

    sect_geninfo[nsection].info.ef          = 0;
    sect_geninfo[nsection].exDataSize       = 0;
//    sect_geninfo[nsection].exData           = NULL;
    nsection++;

    params->numSections          = nsection;
    params->sections             = sect_geninfo;

    ret = xran_prepare_ctrl_pkt(mbuf, params, cc_id, prach_port_id, seq_id);
    if(ret < 0){
        print_err("Fail to build prach control packet - [%d:%d:%d]\n", frame_id, subframe_id, slot_id);
        rte_pktmbuf_free(mbuf);
    }
    return ret;
}


int process_ring(struct rte_ring *r)
{
    assert(r);

    struct rte_mbuf *mbufs[MBUFS_CNT];
    int i;
    uint32_t remaining;
    uint64_t t1;
    const uint16_t dequeued = rte_ring_dequeue_burst(r, (void **)mbufs,
        RTE_DIM(mbufs), &remaining);

    if (!dequeued)
        return 0;

    t1 = MLogTick();
    for (i = 0; i < dequeued; ++i) {
        if (xran_ethdi_filter_packet(mbufs[i], 0) == MBUF_FREE)
            rte_pktmbuf_free(mbufs[i]);
    }
    MLogTask(PID_PROCESS_UP_PKT, t1, MLogTick());

    return remaining;
}

int32_t ring_processing_func(void)
{
    struct xran_ethdi_ctx *const ctx = xran_ethdi_get_ctx();
    struct xran_device_ctx *const pxran_lib_ctx = xran_dev_get_ctx();
    int16_t retPoll = 0;
    int32_t i;
    uint64_t t1, t2;

    rte_timer_manage();

    if (pxran_lib_ctx->bbdev_dec) {
        t1 = MLogTick();
        retPoll = pxran_lib_ctx->bbdev_dec();
        if (retPoll != -1)
        {
            t2 = MLogTick();
            MLogTask(PID_XRAN_BBDEV_UL_POLL + retPoll, t1, t2);
        }
    }

    if (pxran_lib_ctx->bbdev_enc) {
        t1 = MLogTick();
        retPoll = pxran_lib_ctx->bbdev_enc();
        if (retPoll != -1)
        {
            t2 = MLogTick();
            MLogTask(PID_XRAN_BBDEV_DL_POLL + retPoll, t1, t2);
        }
    }

    /* UP first */

    for (i = 0; i < ctx->io_cfg.num_vfs && i < (XRAN_VF_MAX - 1); i = i+2){
        if (process_ring(ctx->rx_ring[i]))
            return 0;

        /* CP next */
        if(ctx->io_cfg.id == O_RU) /* process CP only on O-RU */
            if (process_ring(ctx->rx_ring[i+1]))
                return 0;
    }

    if (XRAN_STOPPED == xran_if_current_state)
        return -1;

    return 0;
}

int ring_processing_thread(void *args)
{
    struct sched_param sched_param;
    struct xran_device_ctx *const p_xran_dev_ctx = xran_dev_get_ctx();
    int res = 0;

    memset(&sched_param, 0, sizeof(struct sched_param));

    printf("%s [CPU %2d] [PID: %6d]\n", __FUNCTION__,  rte_lcore_id(), getpid());
    sched_param.sched_priority = XRAN_THREAD_DEFAULT_PRIO;
    if ((res = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched_param))){
        printf("priority is not changed: coreId = %d, result1 = %d\n",rte_lcore_id(), res);
    }

    for (;;){
        if(ring_processing_func() != 0)
            break;

        /* work around for some kernel */
        if(p_xran_dev_ctx->fh_init.io_cfg.io_sleep)
            nanosleep(&sleeptime,NULL);
    }

    puts("Pkt processing thread finished.");
    return 0;
}

