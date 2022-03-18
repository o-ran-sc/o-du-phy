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

#include "xran_common.h"
#include "ethdi.h"
#include "xran_pkt.h"
#include "xran_pkt_up.h"
#include "xran_cp_api.h"
#include "xran_up_api.h"
#include "xran_cp_proc.h"
#include "xran_dev.h"
#include "xran_lib_mlog_tasks_id.h"

#include "xran_printf.h"
#include "xran_mlog_lnx.h"

static struct timespec sleeptime = {.tv_nsec = 1E3 }; /* 1 us */

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
                        uint8_t iqWidth);


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
                        uint32_t *mb_free,
                        int8_t  expect_comp,
                        uint8_t compMeth,
                        uint8_t iqWidth);

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
                        union ecpri_seq_id *seq_id,
                        uint16_t num_prbu,
                        uint16_t start_prbu,
                        uint16_t sym_inc,
                        uint16_t rb,
                        uint16_t sect_id);

int process_mbuf_batch(struct rte_mbuf* pkt_q[], void* handle, int16_t num, struct xran_eaxc_info *p_cid, uint32_t* ret_data)
{
    struct rte_mbuf* pkt;
    struct xran_device_ctx* p_dev_ctx = (struct xran_device_ctx*)handle;
    void* iq_samp_buf[MBUFS_CNT];
    union ecpri_seq_id seq[MBUFS_CNT];
    static int symbol_total_bytes[XRAN_PORTS_NUM][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR] = { 0 };
    int num_bytes[MBUFS_CNT] = { 0 }, num_bytes_pusch[MBUFS_CNT] = { 0 };
    int16_t i, j;

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

    uint8_t compMeth[MBUFS_CNT] = { 0 };
    uint8_t iqWidth[MBUFS_CNT] = { 0 };
    uint8_t compMeth_ini = 0;
    uint8_t iqWidth_ini = 0;

    uint32_t pkt_size[MBUFS_CNT];

    void* pHandle = NULL;
    int32_t valid_res, res_loc;
    int expect_comp = (p_dev_ctx->fh_cfg.ru_conf.compMeth != XRAN_COMPMETHOD_NONE);
    enum xran_comp_hdr_type staticComp = p_dev_ctx->fh_cfg.ru_conf.xranCompHdrType;

    int16_t num_pusch = 0, num_prach = 0, num_srs = 0;
    int16_t pusch_idx[MBUFS_CNT] = { 0 }, prach_idx[MBUFS_CNT] = { 0 }, srs_idx[MBUFS_CNT] = { 0 };
    int8_t xran_port = xran_dev_ctx_get_port_id(p_dev_ctx);
    int16_t max_ant_num = 0;
    uint8_t *ptr_seq_id_num_port;
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
    struct xran_prb_elm* prbMapElm = NULL;
    uint16_t iq_sample_size_bits;

#if XRAN_MLOG_VAR
    uint32_t mlogVar[10];
    uint32_t mlogVarCnt = 0;
#endif

    if (xran_port < 0) {
        print_err("Invalid pHandle - %p", pHandle);
        return MBUF_FREE;
    }

    if (xran_port > XRAN_PORTS_NUM) {
        print_err("Invalid port - %d", xran_port);
        return MBUF_FREE;
    }

    conf = &(p_dev_ctx->eAxc_id_cfg);
    if (conf == NULL) {
        rte_panic("conf == NULL");
    }

    if (p_dev_ctx->fh_init.io_cfg.id == O_DU)
    {
        max_ant_num = XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR;
        ptr_seq_id_num_port = &xran_upul_seq_id_num[xran_port][0][0];
    }
    else if (p_dev_ctx->fh_init.io_cfg.id == O_RU)
    {
        max_ant_num = XRAN_MAX_ANTENNA_NR;
        ptr_seq_id_num_port = &xran_updl_seq_id_num[xran_port][0][0];
    }
    else
{
        rte_panic("incorrect fh_init.io_cfg.id");
        }

    if (staticComp == XRAN_COMP_HDR_TYPE_STATIC)
    {
        compMeth_ini = p_dev_ctx->fh_cfg.ru_conf.compMeth;
        iqWidth_ini = p_dev_ctx->fh_cfg.ru_conf.iqWidth;
}

    for (i = 0; i < MBUFS_CNT; i++)
{
        pkt_size[i] = pkt_q[i]->pkt_len;
        buf_start[i] = (char*)pkt_q[i]->buf_addr;
        start_off[i] = pkt_q[i]->data_off;
}

    if (expect_comp && (staticComp != XRAN_COMP_HDR_TYPE_STATIC))
    {
#pragma vector always
        for (i = 0; i < MBUFS_CNT; i++)
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

            if (ecpri_hdr[i] == NULL ||
                radio_hdr[i] == NULL ||
                data_hdr[i] == NULL ||
                data_compr_hdr[i] == NULL ||
                iq_samp_buf[i] == NULL)
            {
                num_bytes[i] = 0;       /* packet too short */
            }

#if XRAN_MLOG_VAR
            if(radio_hdr[i] != NULL && data_hdr[i] != NULL)
            {
                mlogVar[mlogVarCnt++] = 0xBBBBBBBB;
                mlogVar[mlogVarCnt++] = xran_lib_ota_tti;
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
#pragma vector always
        for (i = 0; i < MBUFS_CNT; i++)
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

            if (ecpri_hdr[i] == NULL ||
                radio_hdr[i] == NULL ||
                data_hdr[i] == NULL ||
                iq_samp_buf[i] == NULL)
            {
                num_bytes[i] = 0;       /* packet too short */
            }

#if XRAN_MLOG_VAR
            if (radio_hdr[i] != NULL && data_hdr[i] != NULL)
            {
                mlogVar[mlogVarCnt++] = 0xBBBBBBBB;
                mlogVar[mlogVarCnt++] = xran_lib_ota_tti;
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

    for (i = 0; i < MBUFS_CNT; i++) {
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

    for (i = 0; i < MBUFS_CNT; i++)
    {
        radio_hdr[i]->sf_slot_sym.value = rte_be_to_cpu_16(radio_hdr[i]->sf_slot_sym.value);
        data_hdr[i]->fields.all_bits = rte_be_to_cpu_32(data_hdr[i]->fields.all_bits);
    }

    for (i = 0; i < MBUFS_CNT; i++)
    {
        if (num_bytes[i] > 0)
        {
            compMeth[i] = compMeth_ini;
            iqWidth[i] = iqWidth_ini;
            valid_res = XRAN_STATUS_SUCCESS;

            frame_id[i] = radio_hdr[i]->frame_id;
            subframe_id[i] = radio_hdr[i]->sf_slot_sym.subframe_id;
            slot_id[i] = radio_hdr[i]->sf_slot_sym.slot_id;
            symb_id[i] = radio_hdr[i]->sf_slot_sym.symb_id;

            num_prbu[i] = data_hdr[i]->fields.num_prbu;
            start_prbu[i] = data_hdr[i]->fields.start_prbu;
            sym_inc[i] = data_hdr[i]->fields.sym_inc;
            rb[i] = data_hdr[i]->fields.rb;
            sect_id[i] = data_hdr[i]->fields.sect_id;

            if (expect_comp && (staticComp != XRAN_COMP_HDR_TYPE_STATIC))
            {
                compMeth[i] = data_compr_hdr[i]->ud_comp_hdr.ud_comp_meth;
                iqWidth[i] = data_compr_hdr[i]->ud_comp_hdr.ud_iq_width;
            }

            if (CC_ID[i] >= XRAN_MAX_CELLS_PER_PORT || Ant_ID[i] >= max_ant_num || symb_id[i] >= XRAN_NUM_OF_SYMBOL_PER_SLOT)
            {
                ptr_seq_id_num_port[CC_ID[i] * max_ant_num + Ant_ID[i]] = seq_id[i]; // for next
                valid_res = XRAN_STATUS_FAIL;
                pCnt->Rx_pkt_dupl++;
//                print_err("Invalid CC ID - %d or antenna ID or Symbol ID- %d", CC_ID[i], Ant_ID[i], symb_id[i]);
            }
            else
            {
                ptr_seq_id_num_port[CC_ID[i] * max_ant_num + Ant_ID[i]]++;
            }

            pCnt->rx_counter++;
            pCnt->Rx_on_time++;
            pCnt->Total_msgs_rcvd++;

            if (Ant_ID[i] >= p_dev_ctx->srs_cfg.eAxC_offset && p_dev_ctx->fh_cfg.srsEnable)
            {
                Ant_ID[i] -= p_dev_ctx->srs_cfg.eAxC_offset;
                if (last[i] == 1)
                {
                    srs_idx[num_srs] = i;
                    num_srs += 1;
                    pCnt->rx_srs_packets++;
                }
            }
            else if (Ant_ID[i] >= p_dev_ctx->PrachCPConfig.eAxC_offset && p_dev_ctx->fh_cfg.prachEnable)
            {
                Ant_ID[i] -= p_dev_ctx->PrachCPConfig.eAxC_offset;
                if (last[i] == 1)
                {
                    prach_idx[num_prach] = i;
                    num_prach += 1;
                    pCnt->rx_prach_packets[Ant_ID[i]]++;
                }
            }
            else
            {
                if (last[i] == 1)
                {
                    pusch_idx[num_pusch] = i;
                    num_pusch += 1;
                    pCnt->rx_pusch_packets[Ant_ID[i]]++;
                }
            }
            symbol_total_bytes[xran_port][CC_ID[i]][Ant_ID[i]] += num_bytes[i];
            num_bytes_pusch[i] = symbol_total_bytes[xran_port][CC_ID[i]][Ant_ID[i]];
            if (last[i] == 1)
                symbol_total_bytes[xran_port][CC_ID[i]][Ant_ID[i]] = 0;
        }
    }

    for (j = 0; j < num_prach; j++)
    {
        i = prach_idx[j];
        pkt = pkt_q[i];

        print_dbg("Completed receiving PRACH symbol %d, size=%d bytes\n", symb_id[i], num_bytes[i]);

        int16_t res = xran_process_prach_sym(p_dev_ctx,
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
                &ret_data[i]);
    }

    for (j = 0; j < num_srs; j++)
    {
        i = srs_idx[j];
        pkt = pkt_q[i];

        print_dbg("SRS receiving symbol %d, size=%d bytes\n",
                symb_id[i], symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID[i]][Ant_ID[i]]);

        uint64_t t1 = MLogTick();
        int16_t res = xran_process_srs_sym(p_dev_ctx,
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
                iqWidth[i]);
        MLogTask(PID_PROCESS_UP_PKT_SRS, t1, MLogTick());
    }

    if (num_pusch == MBUFS_CNT)
    {
        for (i = 0; i < MBUFS_CNT; i++)
        {
            iq_sample_size_bits = 16;
            if (expect_comp)
                iq_sample_size_bits = iqWidth[i];

            tti = frame_id[i] * SLOTS_PER_SYSTEMFRAME(p_dev_ctx->interval_us_local) +
                subframe_id[i] * SLOTNUM_PER_SUBFRAME(p_dev_ctx->interval_us_local) + slot_id[i];

            pRbMap = (struct xran_prb_map*)p_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID[i]][Ant_ID[i]].sBufferList.pBuffers->pData;

            if (pRbMap)
            {
                prbMapElm = &pRbMap->prbMap[sect_id[i]];
                if (sect_id[i] >= pRbMap->nPrbElm)
                {
//                    print_err("sect_id %d !=pRbMap->nPrbElm %d\n", sect_id[i], pRbMap->nPrbElm);
                    ret_data[i] = MBUF_FREE;
                    continue;
                }
            }
            else
            {
//                print_err("pRbMap==NULL\n");
                ret_data[i] = MBUF_FREE;
                continue;
            }

            if (pRbMap->nPrbElm == 1)
            {
                p_dev_ctx->sFrontHaulRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID[i]][Ant_ID[i]].sBufferList.pBuffers[symb_id[i]].pData = iq_samp_buf[i];
                p_dev_ctx->sFrontHaulRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID[i]][Ant_ID[i]].sBufferList.pBuffers[symb_id[i]].pCtrl = pkt_q[i];
                ret_data[i] = MBUF_KEEP;
            }
            else
            {
                struct xran_section_desc* p_sec_desc = NULL;
                prbMapElm = &pRbMap->prbMap[sect_id[i]];
                p_sec_desc = prbMapElm->p_sec_desc[symb_id[i]][0];

                if (p_sec_desc)
                {
                    mb = p_sec_desc->pCtrl;
                    if (mb) {
                        rte_pktmbuf_free(mb);
                    }
                    p_sec_desc->pCtrl = pkt_q[i];
                    p_sec_desc->pData = iq_samp_buf[i];
                    p_sec_desc->start_prbu = start_prbu[i];
                    p_sec_desc->num_prbu = num_prbu[i];
                    p_sec_desc->iq_buffer_len = num_bytes_pusch[i];
                    p_sec_desc->iq_buffer_offset = iq_offset[i];
                    ret_data[i] = MBUF_KEEP;
                }
                else
{
//                    print_err("p_sec_desc==NULL tti %u ant %d symb_id %d\n", tti, Ant_ID[i], symb_id[i]);
                    ret_data[i] = MBUF_FREE;
                }
            }
        }
    }
    else
    {
        for (j = 0; j < num_pusch; j++)
        {
            i = pusch_idx[j];

            iq_sample_size_bits = 16;
            if (expect_comp)
                iq_sample_size_bits = iqWidth[i];

            tti = frame_id[i] * SLOTS_PER_SYSTEMFRAME(p_dev_ctx->interval_us_local) +
                subframe_id[i] * SLOTNUM_PER_SUBFRAME(p_dev_ctx->interval_us_local) + slot_id[i];

            pRbMap = (struct xran_prb_map*)p_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID[i]][Ant_ID[i]].sBufferList.pBuffers->pData;

            if (pRbMap)
            {
                prbMapElm = &pRbMap->prbMap[sect_id[i]];
                if (sect_id[i] >= pRbMap->nPrbElm)
                {
//                    print_err("sect_id %d !=pRbMap->nPrbElm %d\n", sect_id[i], pRbMap->nPrbElm);
                    ret_data[i] = MBUF_FREE;
                    continue;
                }
            }
            else
            {
//                print_err("pRbMap==NULL\n");
                ret_data[i] = MBUF_FREE;
                continue;
            }

            if (pRbMap->nPrbElm == 1)
            {
                p_dev_ctx->sFrontHaulRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID[i]][Ant_ID[i]].sBufferList.pBuffers[symb_id[i]].pData = iq_samp_buf[i];
                p_dev_ctx->sFrontHaulRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID[i]][Ant_ID[i]].sBufferList.pBuffers[symb_id[i]].pCtrl = pkt_q[i];
                ret_data[i] = MBUF_KEEP;
            }
            else
            {
                struct xran_section_desc* p_sec_desc = NULL;
                prbMapElm = &pRbMap->prbMap[sect_id[i]];
                p_sec_desc = prbMapElm->p_sec_desc[symb_id[i]][0];

                if (p_sec_desc)
                {
                    mb = p_sec_desc->pCtrl;
                    if (mb) {
                        rte_pktmbuf_free(mb);
                    }
                    p_sec_desc->pCtrl = pkt_q[i];
                    p_sec_desc->pData = iq_samp_buf[i];
                    p_sec_desc->start_prbu = start_prbu[i];
                    p_sec_desc->num_prbu = num_prbu[i];
                    p_sec_desc->iq_buffer_len = num_bytes_pusch[i];
                    p_sec_desc->iq_buffer_offset = iq_offset[i];
                    ret_data[i] = MBUF_KEEP;
                }
                else
                {
//                    print_err("p_sec_desc==NULL tti %u ant %d symb_id %d\n", tti, Ant_ID[i], symb_id[i]);
                    ret_data[i] = MBUF_FREE;
                }
            }
        }
    }
    return MBUF_FREE;
}

int
process_mbuf(struct rte_mbuf *pkt, void* handle, struct xran_eaxc_info *p_cid)
{
    uint64_t tt1 = MLogTick();
    struct xran_device_ctx *p_dev_ctx = (struct xran_device_ctx *)handle;
    void *iq_samp_buf;
    union ecpri_seq_id seq;
    static int symbol_total_bytes[XRAN_PORTS_NUM][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR] = {0};
    int num_bytes = 0;

    struct xran_common_counters *pCnt = &p_dev_ctx->fh_counters;

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

    uint8_t compMeth = 0;
    uint8_t iqWidth = 0;

    void *pHandle = NULL;
    int ret = MBUF_FREE;
    uint32_t mb_free = 0;
    int32_t valid_res = 0;
    int expect_comp  = (p_dev_ctx->fh_cfg.ru_conf.compMeth != XRAN_COMPMETHOD_NONE);
    enum xran_comp_hdr_type staticComp = p_dev_ctx->fh_cfg.ru_conf.xranCompHdrType;

    if (staticComp == XRAN_COMP_HDR_TYPE_STATIC)
    {
        compMeth = p_dev_ctx->fh_cfg.ru_conf.compMeth;
        iqWidth = p_dev_ctx->fh_cfg.ru_conf.iqWidth;
    }

    if(p_dev_ctx->xran2phy_mem_ready == 0)
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
                                        staticComp,
                                        &compMeth,
                                        &iqWidth);
    if (num_bytes <= 0){
        print_err("num_bytes is wrong [%d]\n", num_bytes);
        return MBUF_FREE;
    }

    valid_res = xran_pkt_validate(p_dev_ctx,
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
#ifndef FCN_ADAPT
    if(valid_res != 0) {
        print_dbg("valid_res is wrong [%d] ant %u (%u : %u : %u : %u) seq %u num_bytes %d\n", valid_res, Ant_ID, frame_id, subframe_id, slot_id, symb_id, seq.seq_id, num_bytes);
        return MBUF_FREE;
    }
#endif
    MLogTask(PID_PROCESS_UP_PKT_PARSE, tt1, MLogTick());
    if (Ant_ID >= p_dev_ctx->srs_cfg.eAxC_offset && p_dev_ctx->fh_cfg.srsEnable) {
        /* SRS packet has ruportid = 2*num_eAxc + ant_id */
        Ant_ID -= p_dev_ctx->srs_cfg.eAxC_offset;
        symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID] += num_bytes;

        if (seq.bits.e_bit == 1) {
            print_dbg("SRS receiving symbol %d, size=%d bytes\n",
                symb_id, symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID]);

            if (symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID]) {
               uint64_t t1 = MLogTick();
               int16_t res = xran_process_srs_sym(p_dev_ctx,
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
                                &mb_free,
                                expect_comp,
                                compMeth,
                                iqWidth);
                if(res == symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID]) {
                    ret = mb_free;
                } else {
                    print_err("res != symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID]\n");
                }
                pCnt->rx_srs_packets++;
                MLogTask(PID_PROCESS_UP_PKT_SRS, t1, MLogTick());
            }
            symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID] = 0;
        }
        else {
            print_dbg("Transport layer fragmentation (eCPRI) is not supported\n");
        }

    } else if (Ant_ID >= p_dev_ctx->PrachCPConfig.eAxC_offset && p_dev_ctx->fh_cfg.prachEnable) {
        /* PRACH packet has ruportid = num_eAxc + ant_id */
        Ant_ID -= p_dev_ctx->PrachCPConfig.eAxC_offset;
        symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID] += num_bytes;
        if (seq.bits.e_bit == 1) {
            print_dbg("Completed receiving PRACH symbol %d, size=%d bytes\n",
                symb_id, num_bytes);

            if (symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID]) {
                int16_t res =  xran_process_prach_sym(p_dev_ctx,
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
                if(res == symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID]) {
                    ret = mb_free;
                } else {
                    print_err("res != symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID]\n");
                }
                pCnt->rx_prach_packets[Ant_ID]++;
            }
            symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID] = 0;
        } else {
            print_dbg("Transport layer fragmentation (eCPRI) is not supported\n");
        }

    } else { /* PUSCH */
        symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID] += num_bytes;

        if (seq.bits.e_bit == 1) {
            print_dbg("Completed receiving symbol %d, size=%d bytes\n",
                symb_id, symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID]);

            if (symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID]) {
                uint64_t t1 = MLogTick();
                int res = xran_process_rx_sym(p_dev_ctx,
                                pkt,
                                iq_samp_buf,
                                symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID],
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
                                &mb_free,
                                expect_comp,
                                compMeth,
                                iqWidth);
                if(res == symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID]) {
                    ret = mb_free;
                } else {
                    print_err("res != symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID]\n");
                }
                pCnt->rx_pusch_packets[Ant_ID]++;
                MLogTask(PID_PROCESS_UP_PKT, t1, MLogTick());
            }
            symbol_total_bytes[p_dev_ctx->xran_port_id][CC_ID][Ant_ID] = 0;
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
inline int32_t prepare_symbol_ex(enum xran_pkt_dir direction,
                uint16_t section_id,
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
                enum xran_comp_hdr_type staticEn)
{
    int32_t n_bytes;
    int32_t prep_bytes;
    int16_t nPktSize;
    uint32_t off;
    int parm_size;
    struct xran_up_pkt_gen_params xp = { 0 };


    iqWidth = (iqWidth==0) ? 16 : iqWidth;
    switch(compMeth) {
        case XRAN_COMPMETHOD_BLKFLOAT:      parm_size = 1; break;
        case XRAN_COMPMETHOD_MODULATION:    parm_size = 0; break;
        default:
            parm_size = 0;
        }
    n_bytes = (3 * iqWidth + parm_size) * prb_num;
    n_bytes = RTE_MIN(n_bytes, XRAN_MAX_MBUF_LEN);

    nPktSize = sizeof(struct rte_ether_hdr)
                + sizeof(struct xran_ecpri_hdr)
                + sizeof(struct radio_app_common_hdr)
                + sizeof(struct data_section_hdr)
                + n_bytes;
    if ((compMeth != XRAN_COMPMETHOD_NONE)&&(staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC))
        nPktSize += sizeof(struct data_section_compression_hdr);

    /* radio app header */
    xp.app_params.data_feature.value = 0x10;
    xp.app_params.data_feature.data_direction = direction;
    //xp.app_params.payl_ver       = 1;
    //xp.app_params.filter_id      = 0;
    xp.app_params.frame_id       = frame_id;
    xp.app_params.sf_slot_sym.subframe_id    = subframe_id;
    xp.app_params.sf_slot_sym.slot_id        = xran_slotid_convert(slot_id, 0);
    xp.app_params.sf_slot_sym.symb_id        = symbol_no;

    /* convert to network byte order */
    xp.app_params.sf_slot_sym.value = rte_cpu_to_be_16(xp.app_params.sf_slot_sym.value);

    xp.sec_hdr.fields.all_bits   = 0;
    xp.sec_hdr.fields.sect_id    = section_id;
    xp.sec_hdr.fields.num_prbu   = (uint8_t)prb_num;
    xp.sec_hdr.fields.start_prbu = (uint8_t)prb_start;
    //xp.sec_hdr.fields.sym_inc    = 0;
    //xp.sec_hdr.fields.rb         = 0;

    /* compression */
    xp.compr_hdr_param.ud_comp_hdr.ud_comp_meth = compMeth;
    xp.compr_hdr_param.ud_comp_hdr.ud_iq_width  = XRAN_CONVERT_IQWIDTH(iqWidth);
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
                                                  staticEn,
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
                uint8_t CC_ID, uint8_t RU_Port_ID, uint8_t seq_id)
{
    uint32_t do_copy = 0;
    int32_t n_bytes;
    int hdr_len, parm_size;
    int32_t sent=0;
    struct xran_device_ctx *p_dev_ctx = (struct xran_device_ctx *)handle;
    struct xran_common_counters *pCnt = &p_dev_ctx->fh_counters;
    enum xran_comp_hdr_type staticEn= XRAN_COMP_HDR_TYPE_DYNAMIC;


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

    if (mb == NULL){
        char * pChar = NULL;
        mb = xran_ethdi_mbuf_alloc(); /* will be freede by ETH */
        if(mb ==  NULL){
            MLogPrint(NULL);
            errx(1, "out of mbufs after %d packets", 1);
        }
        pChar = rte_pktmbuf_append(mb, hdr_len + n_bytes);
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
                         staticEn);

    if(sent){
        pCnt->tx_counter++;
        pCnt->tx_bytes_counter += rte_pktmbuf_pkt_len(mb);
        p_dev_ctx->send_upmbuf2ring(mb, ETHER_TYPE_ECPRI, xran_map_ecpriPcid_to_vf(p_dev_ctx, direction, CC_ID, RU_Port_ID));
    }

#ifdef DEBUG
    printf("Symbol %2d sent (%d packets, %d bytes)\n", symbol_no, i, n_bytes);
#endif
    }
    return sent;
}

int send_cpmsg(void *pHandle, struct rte_mbuf *mbuf,struct xran_cp_gen_params *params,
                struct xran_section_gen_info *sect_geninfo, uint8_t cc_id, uint8_t ru_port_id, uint8_t seq_id)
{
    int ret = 0, nsection, i;
    uint8_t subframe_id = params->hdr.subframeId;
    uint8_t slot_id = params->hdr.slotId;
    uint8_t dir = params->dir;
    struct xran_device_ctx *p_dev_ctx =(struct xran_device_ctx *) pHandle;
    struct xran_common_counters *pCnt = &p_dev_ctx->fh_counters;

    nsection = params->numSections;

    /* add in the ethernet header */
    struct rte_ether_hdr *const h = (void *)rte_pktmbuf_prepend(mbuf, sizeof(*h));

    pCnt->tx_counter++;
    pCnt->tx_bytes_counter += rte_pktmbuf_pkt_len(mbuf);
    p_dev_ctx->send_cpmbuf2ring(mbuf, ETHER_TYPE_ECPRI, xran_map_ecpriRtcid_to_vf(p_dev_ctx, dir, cc_id, ru_port_id));
    for(i=0; i<nsection; i++)
        xran_cp_add_section_info(pHandle, dir, cc_id, ru_port_id,
                (slot_id + subframe_id*SLOTNUM_PER_SUBFRAME(p_dev_ctx->interval_us_local))%XRAN_MAX_SECTIONDB_CTX,
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
                uint16_t beam_id, uint8_t cc_id, uint8_t prach_port_id, uint16_t occasionid, uint8_t seq_id)
{
    int nsection, ret;
    struct xran_prach_cp_config  *pPrachCPConfig = &(pxran_lib_ctx->PrachCPConfig);
    uint16_t timeOffset;
    uint16_t nNumerology = pxran_lib_ctx->fh_cfg.frame_conf.nNumerology;
    uint8_t startSymId;

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
    startSymId = pPrachCPConfig->startSymId + occasionid * pPrachCPConfig->numSymbol;
    if (startSymId > 0)
    {
        timeOffset += startSymId * (2048 + 144);
    }
    timeOffset = timeOffset >> nNumerology; //original number is Tc, convert to Ts based on mu
    if ((slot_id == 0) || (slot_id == (SLOTNUM_PER_SUBFRAME(pxran_lib_ctx->interval_us_local) >> 1)))
        timeOffset += 16;

    params->dir                  = XRAN_DIR_UL;
    params->sectionType          = XRAN_CP_SECTIONTYPE_3;
    params->hdr.filterIdx        = pPrachCPConfig->filterIdx;
    params->hdr.frameId          = frame_id;
    params->hdr.subframeId       = subframe_id;
    params->hdr.slotId           = slot_id;
    params->hdr.startSymId       = startSymId;
    params->hdr.iqWidth          = xran_get_conf_iqwidth_prach(pHandle);
    params->hdr.compMeth         = xran_get_conf_compmethod_prach(pHandle);
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
    sect_geninfo[nsection].info.numSymbol   = pPrachCPConfig->numSymbol;
    sect_geninfo[nsection].info.reMask      = 0xfff;
    sect_geninfo[nsection].info.beamId      = beam_id;
    sect_geninfo[nsection].info.freqOffset  = pPrachCPConfig->freqOffset;

    pxran_lib_ctx->prach_last_symbol[cc_id] = pPrachCPConfig->startSymId + pPrachCPConfig->numSymbol*pPrachCPConfig->occassionsInPrachSlot - 1;

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


int process_ring(struct rte_ring *r, uint16_t ring_id, uint16_t q_id)
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

    //t1 = MLogTick();

    xran_ethdi_filter_packet(mbufs, ring_id, q_id, dequeued);
    //MLogTask(PID_PROCESS_UP_PKT, t1, MLogTick());

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

    rte_timer_manage();

    if (ctx->bbdev_dec) {
        t1 = MLogTick();
        retPoll = ctx->bbdev_dec();
        if (retPoll == 1)
        {
            t2 = MLogTick();
            MLogTask(PID_XRAN_BBDEV_UL_POLL + retPoll, t1, t2);
        }
    }

    if (ctx->bbdev_enc) {
        t1 = MLogTick();
        retPoll = ctx->bbdev_enc();
        if (retPoll == 1)
        {
            t2 = MLogTick();
            MLogTask(PID_XRAN_BBDEV_DL_POLL + retPoll, t1, t2);
        }
    }

    for (i = 0; i < ctx->io_cfg.num_vfs && i < XRAN_VF_MAX; i++){
        for(qi = 0; qi < ctx->rxq_per_port[i]; qi++) {
            if (process_ring(ctx->rx_ring[i][qi], i, qi))
            return 0;
        }
    }

    if (XRAN_STOPPED == xran_if_current_state)
        return -1;

                return 0;
    }

/** Generic thread to perform task on specific core */
int32_t
xran_generic_worker_thread(void *args)
{
    int32_t res = 0;
    struct xran_worker_th_ctx* pThCtx = (struct xran_worker_th_ctx*)args;
    struct sched_param sched_param;
    struct xran_io_cfg * const p_io_cfg = &(xran_ethdi_get_ctx()->io_cfg);

    memset(&sched_param, 0, sizeof(struct sched_param));

    printf("%s [CPU %2d] [PID: %6d]\n", __FUNCTION__,  rte_lcore_id(), getpid());
    sched_param.sched_priority = XRAN_THREAD_DEFAULT_PRIO;
    if ((res = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched_param))){
        printf("priority is not changed: coreId = %d, result1 = %d\n",rte_lcore_id(), res);
    }
    pThCtx->worker_policy = SCHED_FIFO;
    if ((res = pthread_setname_np(pthread_self(), pThCtx->worker_name))) {
        printf("[core %d] pthread_setname_np = %d\n",rte_lcore_id(), res);
    }

    for (;;) {
        if(pThCtx->task_func) {
            if(pThCtx->task_func(pThCtx->task_arg) != 0)
                break;
        }

    if (XRAN_STOPPED == xran_if_current_state)
        return -1;

        if(p_io_cfg->io_sleep)
            nanosleep(&sleeptime,NULL);
    }

    printf("%s worker thread finished on core %d [worker id %d]\n",pThCtx->worker_name, rte_lcore_id(), pThCtx->worker_id);
    return 0;
}

int ring_processing_thread(void *args)
{
    struct sched_param sched_param;
    struct xran_io_cfg * const p_io_cfg = &(xran_ethdi_get_ctx()->io_cfg);
    int res = 0;

    memset(&sched_param, 0, sizeof(struct sched_param));

    printf("%s [CPU %2d] [PID: %6d]\n", __FUNCTION__,  rte_lcore_id(), getpid());
    sched_param.sched_priority = XRAN_THREAD_DEFAULT_PRIO;
    if ((res = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched_param))){
        printf("priority is not changed: coreId = %d, result1 = %d\n",rte_lcore_id(), res);
    }

    for (;;){
        if(ring_processing_func(args) != 0)
            break;

        /* work around for some kernel */
        if(p_io_cfg->io_sleep)
            nanosleep(&sleeptime,NULL);
    }

    puts("Pkt processing thread finished.");
    return 0;
}

