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
 * @brief XRAN RX module
 * @file xran_rx.c
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

#include "xran_fh_o_du.h"

#include "ethdi.h"
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
#include "xran_rx_proc.h"
#include "xran_cp_proc.h"

#include "xran_mlog_lnx.h"

int xran_process_prach_sym(void *arg,
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
                        uint32_t *mb_free)
{
//    char        *pos = NULL;
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)arg;
    uint8_t symb_id_offset;
    uint32_t tti = 0;
    uint32_t ttt_det = 0;
    //xran_status_t status;
    struct rte_mbuf *mb;
    uint32_t interval = p_xran_dev_ctx->interval_us_local;

    if(p_xran_dev_ctx->xran2phy_mem_ready == 0)
        return 0;

    tti = frame_id * SLOTS_PER_SYSTEMFRAME(interval) + subframe_id * SLOTNUM_PER_SUBFRAME(interval) + slot_id;

    //status = tti << 16 | symb_id;


    struct xran_prach_cp_config *pPrachCPConfig;
    uint32_t StartUsedFirstSym;
    if(p_xran_dev_ctx->dssEnable){
        int i = tti % p_xran_dev_ctx->dssPeriod;
        if(p_xran_dev_ctx->technology[i]==1) {
            pPrachCPConfig = &(p_xran_dev_ctx->PrachCPConfig);
        }
        else{
            pPrachCPConfig = &(p_xran_dev_ctx->PrachCPConfigLTE);
        }
    }
    else{
        pPrachCPConfig = &(p_xran_dev_ctx->PrachCPConfig);
    }

    
    if (1500 == p_xran_dev_ctx->fh_init.mtu && pPrachCPConfig->filterIdx == XRAN_FILTERINDEX_PRACH_012)
    {
        /*one prach for more then one pkg*/
        StartUsedFirstSym = 1;
    }
    else{
        StartUsedFirstSym = 0;
    }
    

    if(CC_ID < XRAN_MAX_SECTOR_NR && Ant_ID < XRAN_MAX_ANTENNA_NR && symb_id < XRAN_NUM_OF_SYMBOL_PER_SLOT){
        uint8_t numerology = xran_get_conf_numerology(p_xran_dev_ctx);
        if (numerology > 0 && pPrachCPConfig->filterIdx == XRAN_FILTERINDEX_PRACH_012) ttt_det = (1<<numerology) - 1;
        else ttt_det = 0;

        if (1 == StartUsedFirstSym)
        {
            uint8_t compMeth = p_xran_dev_ctx->fh_cfg.ru_conf.compMeth;
            uint8_t iqWidth = p_xran_dev_ctx->fh_cfg.ru_conf.iqWidth;
            uint32_t iqLenPrePrb,dataOffset,dataLen;
            uint8_t * pdata;
            symb_id_offset = 0;
            if (XRAN_COMPMETHOD_NONE == compMeth) 
            {
                iqLenPrePrb = 48;
            }
            else
            {
                iqLenPrePrb = 3*iqWidth+1;
            }
            dataOffset = start_prbu*iqLenPrePrb;
            dataLen = num_prbu*iqLenPrePrb;
            
            if(iq_data_start && size) {
                pdata = p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrlDecomp[(tti + ttt_det)% XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id_offset].pData + dataOffset;
                mb = p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrlDecomp[(tti + ttt_det)% XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id_offset].pCtrl;
                if(mb)
                    rte_pktmbuf_free(mb);
            
                if(p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder == XRAN_CPU_LE_BYTE_ORDER) {
                    int idx = 0;
                    uint16_t *psrc = (uint16_t *)iq_data_start;
                    uint16_t *pdst = (uint16_t *)pdata;
                    for (idx = 0; idx < dataLen; idx++){
                        pdst[idx]  = (psrc[idx]>>8) | (psrc[idx]<<8); //rte_be_to_cpu_16(psrc[idx]);
                        }
                    //*mb_free = MBUF_FREE;
                    }
                else{
                    memcpy(pdata,iq_data_start,dataLen);
                    }
                
                p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrlDecomp[(tti + ttt_det) % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id_offset].pCtrl = mbuf;
                *mb_free = MBUF_KEEP;
                }
            else {
                //print_err("pos %p iq_data_start %p size %d\n",pos, iq_data_start, size);
                print_err("iq_data_start %p size %d\n", iq_data_start, size);
                }

        }
        else
        {
        symb_id_offset = symb_id - p_xran_dev_ctx->prach_start_symbol[CC_ID]; //make the storing of prach packets to start from 0 for easy of processing within PHY
//        pos = (char*) p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id_offset].pData;
        if(iq_data_start && size) {
                mb = p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrlDecomp[(tti + ttt_det) % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id_offset].pCtrl;
            if(mb)
                rte_pktmbuf_free(mb);

            if(p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder == XRAN_CPU_LE_BYTE_ORDER) {
                int idx = 0;
                uint16_t *psrc = (uint16_t *)iq_data_start;
                uint16_t *pdst = (uint16_t *)iq_data_start;
                for (idx = 0; idx < size/sizeof(int16_t); idx++){
                    pdst[idx]  = (psrc[idx]>>8) | (psrc[idx]<<8); //rte_be_to_cpu_16(psrc[idx]);
                    }
                //*mb_free = MBUF_FREE;
                }

                p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrlDecomp[(tti + ttt_det) % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id_offset].pData = iq_data_start;
                p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrlDecomp[(tti + ttt_det) % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id_offset].pCtrl = mbuf;
            *mb_free = MBUF_KEEP;
            }
        else {
            //print_err("pos %p iq_data_start %p size %d\n",pos, iq_data_start, size);
            print_err("iq_data_start %p size %d\n", iq_data_start, size);
            }
        
        }

    } else {
        print_err("TTI %d(f_%d sf_%d slot_%d) CC %d Ant_ID %d symb_id %d\n",tti, frame_id, subframe_id, slot_id, CC_ID, Ant_ID, symb_id);
    }

    return size;
}

int32_t xran_process_srs_sym(void *arg,
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
                        uint8_t iqWidth)
{
    char        *pos = NULL;
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)arg;
    uint32_t tti = 0;
    struct rte_mbuf *mb = NULL;
    struct xran_prb_map * pRbMap    = NULL;
    struct xran_prb_elm * prbMapElm = NULL;
    uint16_t iq_sample_size_bits = 16;
    uint16_t sec_desc_idx;
    uint32_t interval = p_xran_dev_ctx->interval_us_local;

    if(expect_comp)
        iq_sample_size_bits = iqWidth;

    if(p_xran_dev_ctx->xran2phy_mem_ready == 0)
        return 0;

    tti = frame_id * SLOTS_PER_SYSTEMFRAME(interval) + subframe_id * SLOTNUM_PER_SUBFRAME(interval) + slot_id;

    if(CC_ID != 0)
        rte_panic("CC_ID != 0");


    if(CC_ID < XRAN_MAX_SECTOR_NR
        && Ant_ID < p_xran_dev_ctx->fh_cfg.nAntElmTRx
        && symb_id < XRAN_NUM_OF_SYMBOL_PER_SLOT)
    {
        if (0 == p_xran_dev_ctx->enableSrsCp)
        {

            struct xran_section_desc *p_sec_desc = NULL;
        pos = (char*) p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id].pData;
        pRbMap = (struct xran_prb_map *) p_xran_dev_ctx->sFHSrsRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers->pData;

            
            if(pRbMap && pRbMap->nPrbElm > 0)
            {
                prbMapElm = &pRbMap->prbMap[0];
                if (symb_id < prbMapElm->nStartSymb || symb_id >= (prbMapElm->nStartSymb + prbMapElm->numSymb))
                {
                    print_err("%dnot srs symbole, srs sym start is %d,num is %d\n", symb_id,prbMapElm->nStartSymb,prbMapElm->numSymb);
                    *mb_free = MBUF_FREE;
                    return size;
                }
                sec_desc_idx = prbMapElm->nSecDesc[0];
                p_sec_desc = &(prbMapElm->sec_desc[0][0]);
                if(sec_desc_idx >= XRAN_NUM_OF_SYMBOL_PER_SLOT*XRAN_MAX_FRAGMENT)
                {
                    print_err("sec_desc_idx %d is more then %d\n", sec_desc_idx,XRAN_NUM_OF_SYMBOL_PER_SLOT*XRAN_MAX_FRAGMENT);
                    *mb_free = MBUF_FREE;
                    return size;
                }
                
                pos += start_prbu * XRAN_PAYLOAD_1_RB_SZ(iq_sample_size_bits);
                if(pos && iq_data_start && size)
                {
                   if (p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder == XRAN_CPU_LE_BYTE_ORDER)
                   {
                       rte_panic("XRAN_CPU_LE_BYTE_ORDER is not supported 0x16%lx\n", (long)mb);
                   }
                   else if (likely(p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder == XRAN_NE_BE_BYTE_ORDER))
                   {
                           p_sec_desc += sec_desc_idx;
                           if(p_sec_desc)
                           {
                               mb = p_sec_desc->pCtrl;
                               if(mb)
                               {
                                  rte_pktmbuf_free(mb);
                               }
                               p_sec_desc->pData         = iq_data_start;
                               p_sec_desc->pCtrl         = mbuf;
                               p_sec_desc->start_prbu    = start_prbu;
                               p_sec_desc->num_prbu      = num_prbu;
                               p_sec_desc->iq_buffer_len = size;
                               p_sec_desc->iq_buffer_offset = RTE_PTR_DIFF(iq_data_start, mbuf);
                               prbMapElm->nSecDesc[0] += 1;
                           }
                           else
                           {
                               print_err("p_sec_desc==NULL tti %u ant %d symb_id %d sec_desc_idx %d\n", tti, Ant_ID, symb_id, sec_desc_idx);
                               *mb_free = MBUF_FREE;
                               return size;
                           }
                           *mb_free = MBUF_KEEP;
                   } /* else if (likely(p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder == XRAN_NE_BE_BYTE_ORDER)) */
                } /* if(pos && iq_data_start && size) */
                else
                {
                   print_err("pos %p iq_data_start %p size %d\n",pos, iq_data_start, size);
                }
                
            }
            else
            {
                print_err("pRbMap==NULL\n");
                *mb_free = MBUF_FREE;
                return size;
            }

        }
        else
        {
            pos = (char*) p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id].pData;
            pRbMap = (struct xran_prb_map *) p_xran_dev_ctx->sFHSrsRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers->pData;
            if(pRbMap)
            {
            prbMapElm = &pRbMap->prbMap[sect_id];
                if(sect_id >= pRbMap->nPrbElm)
                {
                print_err("sect_id %d !=pRbMap->nPrbElm %d\n", sect_id,pRbMap->nPrbElm);
                *mb_free = MBUF_FREE;
                return size;
            }
            }
            else
            {
            print_err("pRbMap==NULL\n");
            *mb_free = MBUF_FREE;
            return size;
        }

        pos += start_prbu * XRAN_PAYLOAD_1_RB_SZ(iq_sample_size_bits);
            if(pos && iq_data_start && size)
            {
                if (p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder == XRAN_CPU_LE_BYTE_ORDER)
                {
                int idx = 0;
                uint16_t *psrc = (uint16_t *)iq_data_start;
                uint16_t *pdst = (uint16_t *)pos;
                rte_panic("XRAN_CPU_LE_BYTE_ORDER is not supported 0x16%lx\n", (long)mb);
                /* network byte (be) order of IQ to CPU byte order (le) */
                    for (idx = 0; idx < size/sizeof(int16_t); idx++)
                    {
                    pdst[idx]  = (psrc[idx]>>8) | (psrc[idx]<<8); //rte_be_to_cpu_16(psrc[idx]);
                }
                }
                else if (likely(p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder == XRAN_NE_BE_BYTE_ORDER))
                {
                    /*if (pRbMap->nPrbElm == 1)
                    {
                    if (likely (p_xran_dev_ctx->fh_init.mtu >=
                              p_xran_dev_ctx->fh_cfg.nULRBs * XRAN_PAYLOAD_1_RB_SZ(iq_sample_size_bits)))
                    {
                        // no fragmentation
                        mb = p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id].pCtrl;
                        if(mb){
                           rte_pktmbuf_free(mb);
                        }else{
                           print_err("mb==NULL\n");
                        }
                        p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id].pData = iq_data_start;
                        p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id].pCtrl = mbuf;
                    *mb_free = MBUF_KEEP;
                        }
                        else
                        {
                        // packet can be fragmented copy RBs
                        memcpy(pos, iq_data_start, size);
                        *mb_free = MBUF_FREE;
                    }
                    }
                    else */
                    {
                    struct xran_section_desc *p_sec_desc = NULL;
                    prbMapElm = &pRbMap->prbMap[sect_id];
    //                    sec_desc_idx = 0;//prbMapElm->nSecDesc[symb_id];
                        sec_desc_idx = prbMapElm->nSecDesc[symb_id];

                        if (sec_desc_idx < XRAN_MAX_FRAGMENT)
                        {
                            p_sec_desc =  &prbMapElm->sec_desc[symb_id][sec_desc_idx];
                        }
                        else
                        {
                            print_err("[p %d]sect_id %d: sec_desc_idx %d tti %u ant %d symb_id %d sec_desc_idx %d\n", p_xran_dev_ctx->xran_port_id, sect_id,  sec_desc_idx, tti, Ant_ID, symb_id, sec_desc_idx);
                        prbMapElm->nSecDesc[symb_id] = 0;
                        *mb_free = MBUF_FREE;
                        return size;
                    }

                        if(p_sec_desc)
                        {
                        mb = p_sec_desc->pCtrl;
                            if(mb)
                            {
                           rte_pktmbuf_free(mb);
                        }
                        p_sec_desc->pData         = iq_data_start;
                        p_sec_desc->pCtrl         = mbuf;
                        p_sec_desc->start_prbu    = start_prbu;
                        p_sec_desc->num_prbu      = num_prbu;
                        p_sec_desc->iq_buffer_len = size;
                        p_sec_desc->iq_buffer_offset = RTE_PTR_DIFF(iq_data_start, mbuf);
                            prbMapElm->nSecDesc[symb_id] += 1;
                        }
                        else
                        {
                        print_err("p_sec_desc==NULL tti %u ant %d symb_id %d sec_desc_idx %d\n", tti, Ant_ID, symb_id, sec_desc_idx);
                        *mb_free = MBUF_FREE;
                        return size;
                    }
                    *mb_free = MBUF_KEEP;
                }
                } /* else if (likely(p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder == XRAN_NE_BE_BYTE_ORDER)) */
            } /* if(pos && iq_data_start && size) */
            else
            {
            print_err("pos %p iq_data_start %p size %d\n",pos, iq_data_start, size);
        }
        }
    } /* if(CC_ID < XRAN_MAX_SECTOR_NR && Ant_ID < p_xran_dev_ctx->fh_cfg.nAntElmTRx && symb_id < XRAN_NUM_OF_SYMBOL_PER_SLOT) */
    else
    {
        print_err("o-xu%d: TTI %d(f_%d sf_%d slot_%d) CC %d Ant_ID %d symb_id %d\n",p_xran_dev_ctx->xran_port_id, tti, frame_id, subframe_id, slot_id, CC_ID, Ant_ID, symb_id);
    }

    return size;
}

int32_t xran_pkt_validate(void *arg,
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
                        uint16_t sect_id)
{
    struct xran_device_ctx * p_dev_ctx = (struct xran_device_ctx *)arg;
    struct xran_common_counters *pCnt = &p_dev_ctx->fh_counters;

    if(p_dev_ctx->fh_init.io_cfg.id == O_DU) {
        if(xran_check_upul_seqid(p_dev_ctx, CC_ID, Ant_ID, slot_id, seq_id->bits.seq_id) != XRAN_STATUS_SUCCESS) {
            pCnt->Rx_pkt_dupl++;
            return (XRAN_STATUS_FAIL);
        }
    } else if(p_dev_ctx->fh_init.io_cfg.id == O_RU) {
        if(xran_check_updl_seqid(p_dev_ctx, CC_ID, Ant_ID, slot_id, seq_id->bits.seq_id) != XRAN_STATUS_SUCCESS) {
            pCnt->Rx_pkt_dupl++;
            return (XRAN_STATUS_FAIL);
        }
    }else {
        print_err("incorrect dev type %d\n", p_dev_ctx->fh_init.io_cfg.id);
    }

    pCnt->rx_counter++;

    pCnt->Rx_on_time++;
    pCnt->Total_msgs_rcvd++;

    return XRAN_STATUS_SUCCESS;
}

int32_t xran_process_rx_sym(void *arg,
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
                        uint8_t iqWidth)
{
    char        *pos = NULL;
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)arg;
    uint32_t tti = 0;
    //xran_status_t status;
    struct rte_mbuf *mb = NULL;
    struct xran_prb_map * pRbMap    = NULL;
    struct xran_prb_elm * prbMapElm = NULL;
    uint16_t iq_sample_size_bits = 16;
    uint16_t sec_desc_idx, prb_elem_id=0;
    uint32_t interval = p_xran_dev_ctx->interval_us_local;
    uint16_t i=0, total_sections=0;

    if(expect_comp)
        iq_sample_size_bits = iqWidth;

    tti = frame_id * SLOTS_PER_SYSTEMFRAME(interval) + subframe_id * SLOTNUM_PER_SUBFRAME(interval) + slot_id;

    //status = tti << 16 | symb_id;

    if(CC_ID < XRAN_MAX_SECTOR_NR && Ant_ID < XRAN_MAX_ANTENNA_NR && symb_id < XRAN_NUM_OF_SYMBOL_PER_SLOT){
        pos = (char*) p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id].pData;
        pRbMap = (struct xran_prb_map *) p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers->pData;
        if(pRbMap){
            /** Get the prb_elem_id */
            total_sections=0;
            if(pRbMap->prbMap[0].bf_weight.extType == 1)
            {
                for(i=0 ; i < pRbMap->nPrbElm ; i++)
                {
                    total_sections += pRbMap->prbMap[i].bf_weight.numSetBFWs;
                    if(total_sections >= (sect_id + 1))
                    {
                        prb_elem_id = i;
                        break;
                    }
                }
            }
            else
            {
                prb_elem_id = sect_id;
            }

            prbMapElm = &pRbMap->prbMap[prb_elem_id];
            if(prb_elem_id >= pRbMap->nPrbElm) {
                print_err("sect id %d prb_elem_id %d !=pRbMap->nPrbElm %d\n",sect_id, prb_elem_id,pRbMap->nPrbElm);
                *mb_free = MBUF_FREE;
                return size;
            }
        } else {
            print_err("pRbMap==NULL\n");
            *mb_free = MBUF_FREE;
            return size;
        }

        pos += start_prbu * XRAN_PAYLOAD_1_RB_SZ(iq_sample_size_bits);
        if(pos && iq_data_start && size){
            if (p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder == XRAN_CPU_LE_BYTE_ORDER) {
                int idx = 0;
                uint16_t *psrc = (uint16_t *)iq_data_start;
                uint16_t *pdst = (uint16_t *)pos;
                rte_panic("XRAN_CPU_LE_BYTE_ORDER is not supported 0x16%lx\n", (long)mb);
                /* network byte (be) order of IQ to CPU byte order (le) */
                for (idx = 0; idx < size/sizeof(int16_t); idx++){
                    pdst[idx]  = (psrc[idx]>>8) | (psrc[idx]<<8); //rte_be_to_cpu_16(psrc[idx]);
                }
            } else if (likely(p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder == XRAN_NE_BE_BYTE_ORDER)){
                    struct xran_section_desc *p_sec_desc = NULL;
                prbMapElm = &pRbMap->prbMap[prb_elem_id];
                sec_desc_idx = prbMapElm->nSecDesc[symb_id];

                    if (sec_desc_idx < XRAN_MAX_FRAGMENT) {
                    p_sec_desc = &prbMapElm->sec_desc[symb_id][sec_desc_idx];
                    } else {
                    print_err("[p: %d] sect_id %d: sec_desc_idx %d tti %u ant %d symb_id %d sec_desc_idx %d\n",p_xran_dev_ctx->xran_port_id,
                                sect_id,  sec_desc_idx, tti, Ant_ID, symb_id, sec_desc_idx);
                        prbMapElm->nSecDesc[symb_id] = 0;
                        *mb_free = MBUF_FREE;
                        return size;
                    }

                    if(p_sec_desc){
                        mb = p_sec_desc->pCtrl;
                        if(mb){
                           rte_pktmbuf_free(mb);
                        }
                        p_sec_desc->pData         = iq_data_start;
                        p_sec_desc->pCtrl         = mbuf;
                        p_sec_desc->start_prbu    = start_prbu;
                        p_sec_desc->num_prbu      = num_prbu;
                        p_sec_desc->iq_buffer_len = size;
                        p_sec_desc->iq_buffer_offset = RTE_PTR_DIFF(iq_data_start, mbuf);
                    prbMapElm->nSecDesc[symb_id] += 1;
                    } else {
                        print_err("p_sec_desc==NULL tti %u ant %d symb_id %d sec_desc_idx %d\n", tti, Ant_ID, symb_id, sec_desc_idx);
                        *mb_free = MBUF_FREE;
                        return size;
                    }
                    *mb_free = MBUF_KEEP;

            }
        } else {
            print_err("pos %p iq_data_start %p size %d\n",pos, iq_data_start, size);
        }
    } else {
        print_err("o-xu%d: TTI %d(f_%d sf_%d slot_%d) CC %d Ant_ID %d symb_id %d\n",p_xran_dev_ctx->xran_port_id, tti, frame_id, subframe_id, slot_id, CC_ID, Ant_ID, symb_id);
    }
    return size;
}
