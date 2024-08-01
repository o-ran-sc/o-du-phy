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
                        uint32_t *mb_free,
                        uint8_t mu)
{
//    char        *pos = NULL;
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)arg;
    uint8_t symb_id_offset;
    uint32_t tti = 0;
    uint32_t ttt_det = 0;
    //xran_status_t status;
    struct xran_prb_map *pRbMap    = NULL;
    struct rte_mbuf *mb;
    uint32_t interval = xran_fs_get_tti_interval(mu);

    if(p_xran_dev_ctx->xran2phy_mem_ready == 0)
        return 0;

    tti = frame_id * SLOTS_PER_SYSTEMFRAME(interval) + subframe_id * SLOTNUM_PER_SUBFRAME(interval) + slot_id;

    //status = tti << 16 | symb_id;


    struct xran_prach_cp_config *pPrachCPConfig;
    if(p_xran_dev_ctx->dssEnable){
        int i = tti % p_xran_dev_ctx->dssPeriod;
        if(p_xran_dev_ctx->technology[i]==1) {
            pPrachCPConfig = &(p_xran_dev_ctx->perMu[mu].PrachCPConfig);
        }
        else{
            pPrachCPConfig = &(p_xran_dev_ctx->perMu[mu].PrachCPConfigLTE);
        }
    }
    else{
        pPrachCPConfig = &(p_xran_dev_ctx->perMu[mu].PrachCPConfig);
    }

    if(/*CC_ID < XRAN_MAX_SECTOR_NR && */ Ant_ID < XRAN_MAX_ANTENNA_NR && symb_id < XRAN_NUM_OF_SYMBOL_PER_SLOT){
        uint8_t numerology = mu; //xran_get_conf_numerology(p_xran_dev_ctx);
        if (numerology > 0 && pPrachCPConfig->filterIdx == XRAN_FILTERINDEX_PRACH_012) ttt_det = (1<<numerology) - 1;
        else ttt_det = 0;

        pRbMap = (struct xran_prb_map *) p_xran_dev_ctx->perMu[mu].sFHPrachRxPrbMapBbuIoBufCtrl[(tti + ttt_det) % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers->pData;
        if(unlikely(pRbMap == NULL)) goto prach_counter_free_mbuf_return_size;
        if(iq_data_start && size)
        {
            symb_id_offset = symb_id - p_xran_dev_ctx->prach_start_symbol[CC_ID];
            struct xran_rx_packet_ctl *pFrontHaulRxPacketCtrl = &pRbMap->sFrontHaulRxPacketCtrl[symb_id_offset];
            int32_t npkts = pFrontHaulRxPacketCtrl->nRxPkt;
            if (unlikely(npkts >= XRAN_MAX_RX_PKT_PER_SYM))
            {
                print_err("(%d : %d : %d : %d)Received %d type-1 packets on symbol %d\n", frame_id, subframe_id, slot_id, Ant_ID, npkts, symb_id);
                goto prach_counter_free_mbuf_return_size;
            }
            else
            {
                pFrontHaulRxPacketCtrl->nRxPkt = npkts + 1;
            }
            mb = pFrontHaulRxPacketCtrl->pCtrl[npkts];
            if(mb){
               rte_pktmbuf_free(mb);
            }
            pFrontHaulRxPacketCtrl->nRBStart[npkts] = start_prbu;
            pFrontHaulRxPacketCtrl->nRBSize[npkts] = num_prbu;
            pFrontHaulRxPacketCtrl->nSectid[npkts] = sect_id;
            pFrontHaulRxPacketCtrl->pData[npkts] = iq_data_start;
            pFrontHaulRxPacketCtrl->pCtrl[npkts] = mbuf;
            
            *mb_free = MBUF_KEEP;
            return size;
        }
    }
    else
    {
        print_dbg("TTI %d(f_%d sf_%d slot_%d) CC %d Ant_ID %d symb_id %d\n",tti, frame_id, subframe_id, slot_id, CC_ID, Ant_ID, symb_id);
    }

prach_counter_free_mbuf_return_size:
    ++p_xran_dev_ctx->fh_counters.rx_err_prach;
    ++p_xran_dev_ctx->fh_counters.rx_err_up;
    ++p_xran_dev_ctx->fh_counters.rx_err_drop;
    *mb_free = MBUF_FREE;

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
                        uint8_t iqWidth,
                        uint8_t mu)
{
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)arg;
    uint32_t tti = 0;
    struct rte_mbuf *mb = NULL;
    struct xran_prb_map * pRbMap    = NULL;
    uint32_t interval = xran_fs_get_tti_interval(mu);
    static uint32_t firstprint = 0;

    if(p_xran_dev_ctx->xran2phy_mem_ready == 0)
        return 0;

    tti = frame_id * SLOTS_PER_SYSTEMFRAME(interval) + subframe_id * SLOTNUM_PER_SUBFRAME(interval) + slot_id;

    if(/* CC_ID < XRAN_MAX_SECTOR_NR && */ Ant_ID < p_xran_dev_ctx->fh_cfg.nAntElmTRx && symb_id < XRAN_NUM_OF_SYMBOL_PER_SLOT)
    {
        pRbMap = (struct xran_prb_map *) p_xran_dev_ctx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers->pData;
        if(likely(pRbMap))
        {
            if (p_xran_dev_ctx->enableSrsCp)
            {
                if(unlikely(sect_id >= pRbMap->nPrbElm))
                {
                    print_dbg("sect_id %d !=pRbMap->nPrbElm %d\n", sect_id,pRbMap->nPrbElm);
                    goto increment_counter_free_mbuf_return_size;
                }
            }
        }
        else
        {
            print_dbg("pRbMap==NULL\n");
            goto increment_counter_free_mbuf_return_size;                
        }

        if(expect_comp && compMeth != XRAN_COMPMETHOD_NONE)
        {
            if(unlikely(iqWidth != p_xran_dev_ctx->fh_cfg.ru_conf.iqWidth))
            {
                print_dbg("iqWidth in rx pkt (%hhu) != expected iqWidth (%hd)", iqWidth, p_xran_dev_ctx->fh_cfg.ru_conf.iqWidth);
                goto increment_counter_free_mbuf_return_size;
            }
        }

        if(likely(iq_data_start && size))
        {
            if (p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder == XRAN_CPU_LE_BYTE_ORDER)
            {
                print_dbg("XRAN_CPU_LE_BYTE_ORDER is not supported 0x16%lx\n", (long)mb);
                goto increment_counter_free_mbuf_return_size;                

                #if 0
                int idx = 0;
                uint16_t *psrc = (uint16_t *)iq_data_start;
                uint16_t *pdst = (uint16_t *)pos;
                /* network byte (be) order of IQ to CPU byte order (le) */
                for (idx = 0; idx < size/sizeof(int16_t); idx++)
                {
                    pdst[idx]  = (psrc[idx]>>8) | (psrc[idx]<<8); //rte_be_to_cpu_16(psrc[idx]);
                }
                #endif
            }
            else if (likely(p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder == XRAN_NE_BE_BYTE_ORDER))
            {
                struct xran_rx_packet_ctl *pFrontHaulRxPacketCtrl = &pRbMap->sFrontHaulRxPacketCtrl[symb_id];
                int32_t npkts = pFrontHaulRxPacketCtrl->nRxPkt;
                if (unlikely(npkts >= XRAN_MAX_RX_PKT_PER_SYM))
                {
                    if (firstprint == 0) print_err("(%d : %d : %d : %d)Received %d SRS packets on symbol %d\n",frame_id, subframe_id, slot_id, Ant_ID, npkts, symb_id);
                    firstprint = 1;
                    goto increment_counter_free_mbuf_return_size;
                }
                else
                {
                    pFrontHaulRxPacketCtrl->nRxPkt = npkts + 1;
                }
                mb = pFrontHaulRxPacketCtrl->pCtrl[npkts];
                if(mb)
                {
                   rte_pktmbuf_free(mb);
                }
                pFrontHaulRxPacketCtrl->nRBStart[npkts] = start_prbu;
                pFrontHaulRxPacketCtrl->nRBSize[npkts] = num_prbu;
                pFrontHaulRxPacketCtrl->nSectid[npkts] = sect_id;
                pFrontHaulRxPacketCtrl->pData[npkts] = iq_data_start;
                pFrontHaulRxPacketCtrl->pCtrl[npkts] = mbuf;
                *mb_free = MBUF_KEEP;
                goto return_size;
            } /* else if (likely(p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder == XRAN_NE_BE_BYTE_ORDER)) */
        } /* if(pos && iq_data_start && size) */
        else
        {
            print_dbg("iq_data_start %p size %d\n", iq_data_start, size);
        }
    } /* if(CC_ID < XRAN_MAX_SECTOR_NR && Ant_ID < p_xran_dev_ctx->fh_cfg.nAntElmTRx && symb_id < XRAN_NUM_OF_SYMBOL_PER_SLOT) */
    else
    {
        print_dbg("o-xu%d: TTI %d(f_%d sf_%d slot_%d) CC %d Ant_ID %d symb_id %d\n",p_xran_dev_ctx->xran_port_id, tti, frame_id, subframe_id, slot_id, CC_ID, Ant_ID, symb_id);
    }

increment_counter_free_mbuf_return_size:
    ++p_xran_dev_ctx->fh_counters.rx_err_srs;
    ++p_xran_dev_ctx->fh_counters.rx_err_up;
    ++p_xran_dev_ctx->fh_counters.rx_err_drop;
    *mb_free = MBUF_FREE;

return_size:
    return size;
}

int32_t xran_process_csirs_sym(void *arg,
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
                        uint8_t mu)
{
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)arg;
    uint32_t tti = 0, idxElm = 0;
    struct rte_mbuf *mb = NULL;
    struct xran_prb_map * pRbMap    = NULL;
    uint32_t interval = xran_fs_get_tti_interval(mu);
    uint8_t prb_elem_id = 0;
    if(p_xran_dev_ctx->xran2phy_mem_ready == 0)
        return 0;

    tti = frame_id * SLOTS_PER_SYSTEMFRAME(interval) + subframe_id * SLOTNUM_PER_SUBFRAME(interval) + slot_id;

    if(unlikely(CC_ID != 0))
    {
        print_dbg("CC_ID != 0");
        goto increment_counter_free_mbuf_return_size;                
    }

    if(/* CC_ID < XRAN_MAX_SECTOR_NR
        &&*/ Ant_ID < XRAN_MAX_CSIRS_PORTS
        && symb_id < XRAN_NUM_OF_SYMBOL_PER_SLOT)
    {
        
        pRbMap = (struct xran_prb_map*)p_xran_dev_ctx->perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers->pData;

        if (likely(pRbMap))
        {
            /** Get the prb_elem_id */
            for(idxElm=0 ; idxElm < pRbMap->nPrbElm ; ++idxElm)
            {
                if(sect_id == pRbMap->prbMap[idxElm].startSectId)
                {
                    prb_elem_id = idxElm;
                    break;
                }
            }

            if (unlikely(prb_elem_id >= pRbMap->nPrbElm))
            {
                print_dbg("sect_id %d, prb_elem_id %d !=pRbMap->nPrbElm %d\n", sect_id, prb_elem_id, pRbMap->nPrbElm);
                goto increment_counter_free_mbuf_return_size;
            }
        }
        else
            goto increment_counter_free_mbuf_return_size;

        /* Validate IQ width */
        if(expect_comp && compMeth != XRAN_COMPMETHOD_NONE)
        {
            if(unlikely(iqWidth != p_xran_dev_ctx->fh_cfg.ru_conf.iqWidth))
                goto increment_counter_free_mbuf_return_size;
        }

        if(iq_data_start && size)
        {
            struct xran_rx_packet_ctl *pFrontHaulRxPacketCtrl = &pRbMap->sFrontHaulRxPacketCtrl[symb_id];
            int32_t npkts = pFrontHaulRxPacketCtrl->nRxPkt;
            if (unlikely(npkts >= XRAN_MAX_RX_PKT_PER_SYM))
            {
                print_err("Received %d type-1 CSI-RS packets on symbol %d\n", npkts, symb_id);
                goto increment_counter_free_mbuf_return_size;
            }
            else
            {
                pFrontHaulRxPacketCtrl->nRxPkt = npkts + 1;
            }
            mb = pFrontHaulRxPacketCtrl->pCtrl[npkts];
            if(mb){
               rte_pktmbuf_free(mb);
            }
            pFrontHaulRxPacketCtrl->nRBStart[npkts] = start_prbu;
            pFrontHaulRxPacketCtrl->nRBSize[npkts] = num_prbu;
            pFrontHaulRxPacketCtrl->nSectid[npkts] = sect_id;
            pFrontHaulRxPacketCtrl->pData[npkts] = iq_data_start;
            pFrontHaulRxPacketCtrl->pCtrl[npkts] = mbuf;
            
            *mb_free = MBUF_KEEP;
            goto return_size;
        }

    } /* if(CC_ID < XRAN_MAX_SECTOR_NR && Ant_ID < XRAN_MAX_CSIRS_PORTS && symb_id < XRAN_NUM_OF_SYMBOL_PER_SLOT) */
    else
    {
        print_dbg("o-xu%d: TTI %d(f_%d sf_%d slot_%d) CC %d Ant_ID %d symb_id %d\n",p_xran_dev_ctx->xran_port_id, tti, frame_id, subframe_id, slot_id, CC_ID, Ant_ID, symb_id);
    }

increment_counter_free_mbuf_return_size:
    ++p_xran_dev_ctx->fh_counters.rx_err_csirs;
    ++p_xran_dev_ctx->fh_counters.rx_err_up;
    ++p_xran_dev_ctx->fh_counters.rx_err_drop;
    *mb_free = MBUF_FREE;

return_size:
    return size;
}

int32_t xran_validate_seq_id(void *arg, uint8_t CC_ID, uint8_t Ant_ID, uint8_t slot_id, union ecpri_seq_id *seq_id)
{
#if 1
    struct xran_device_ctx *p_dev_ctx = (struct xran_device_ctx *)arg;

    ++p_dev_ctx->fh_counters.rx_counter;
    ++p_dev_ctx->fh_counters.Total_msgs_rcvd;

    return XRAN_STATUS_SUCCESS;
#else
    /* Need to disable the check since current checking is to compare in-sequence order,
    * but the packets could be arrived out-of-order sequence */

    struct xran_device_ctx *p_dev_ctx = (struct xran_device_ctx *)arg;
    struct xran_common_counters *pCnt;
    int8_t xran_port = 0;
    int appMode;
    int ret = XRAN_STATUS_FAIL;

    xran_port =  xran_dev_ctx_get_port_id(p_dev_ctx);
    if(unlikely((xran_port < 0)
                || (xran_port >= XRAN_PORTS_NUM)
                || (CC_ID >= XRAN_MAX_CELLS_PER_PORT)))
    {
        print_dbg("Invalid parameter: pHandle=%p  port=%d  CC ID=%d", p_dev_ctx, xran_port, CC_ID);
    }
    else
    {
        pCnt = &p_dev_ctx->fh_counters;
        appMode = xran_get_syscfg_appmode();
        if(appMode == O_DU)
        {
            /* AntID allocation - PUSCH (neAxC) -> PRACH -> SRS (nAntElmTRx)
            srsEaxcOffset calculation includes PUSCH + PRACH ant num */
            if(unlikely(Ant_ID >= 2*XRAN_MAX_ANTENNA_NR + p_dev_ctx->fh_cfg.nAntElmTRx))
            {
                print_dbg("Invalid antenna ID - %d", Ant_ID);
            }
            else
            {
                ret = xran_check_upul_seqid(xran_port, CC_ID, Ant_ID, slot_id, seq_id->bits.seq_id);
                if(unlikely(ret != XRAN_STATUS_SUCCESS))
                {
                    xran_set_upul_seqid(xran_port, CC_ID, Ant_ID, seq_id->bits.seq_id);  // for next
                    ++p_dev_ctx->fh_counters.Rx_pkt_dupl;
                }
            }
        }
        else
        {
            /* AntID allocation - PDSCH -> CSI-RS */
            if(unlikely(Ant_ID >= p_dev_ctx->fh_cfg.neAxc + XRAN_MAX_CSIRS_PORTS))
            {
                print_dbg("Invalid antenna ID - %d", Ant_ID);
            }
            else
            {
                ret = xran_check_updl_seqid(xran_port, CC_ID, Ant_ID, slot_id, seq_id->bits.seq_id);
                if(unlikely(ret != XRAN_STATUS_SUCCESS))
                {
                    xran_set_updl_seqid(xran_port, CC_ID, Ant_ID, seq_id->bits.seq_id);
                    ++p_dev_ctx->fh_counters.Rx_pkt_dupl;
                }
            }
        }

        if(ret == XRAN_STATUS_SUCCESS)
        {
            ++pCnt->rx_counter;
            ++pCnt->Total_msgs_rcvd;
            return(XRAN_STATUS_SUCCESS);
        }
    }

    ++p_dev_ctx->fh_counters.rx_err_drop;
    ++p_dev_ctx->fh_counters.rx_err_up;
    return(XRAN_STATUS_FAIL);
#endif
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
                        uint8_t iqWidth,
                        uint8_t mu,
                        uint16_t nSectionIdx)
{
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)arg;
    uint32_t tti = 0;
    //xran_status_t status;
    struct rte_mbuf *mb = NULL;
    struct xran_prb_map * pRbMap    = NULL;
    struct xran_prb_elm * prbMapElm = NULL;
    uint32_t interval = xran_fs_get_tti_interval(mu);
    uint16_t i=0, prb_elem_id = 0;
    static uint32_t firstprint = 0;
    Ant_ID -= p_xran_dev_ctx->perMu[mu].eaxcOffset;

    tti = frame_id * SLOTS_PER_SYSTEMFRAME(interval) + subframe_id * SLOTNUM_PER_SUBFRAME(interval) + slot_id;

    //status = tti << 16 | symb_id;

    if(/*CC_ID  < XRAN_MAX_SECTOR_NR && */
       Ant_ID < (xran_get_syscfg_appmode() == O_DU ? p_xran_dev_ctx->fh_cfg.neAxcUl : p_xran_dev_ctx->fh_cfg.neAxc) && 
       symb_id < XRAN_NUM_OF_SYMBOL_PER_SLOT)
    {
        pRbMap = (struct xran_prb_map *) p_xran_dev_ctx->perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers->pData;
        if(likely(pRbMap))
        {
            /** Get the prb_elem_id */
            for(i=0 ; i < pRbMap->nPrbElm ; i++)
            {
                if(sect_id == pRbMap->prbMap[i].startSectId)
                {
                    prb_elem_id = i;
                    break;
                }
            }
            if (prb_elem_id >= pRbMap->nPrbElm)
            {
                // print_err("sect_id %d, prb_elem_id %d !=pRbMap->nPrbElm %d\n", sect_id, prb_elem_id, pRbMap->nPrbElm);
                goto inc_counter_free_mbuf_return_size;
            }
            prbMapElm = &pRbMap->prbMap[prb_elem_id];
        } 
        else
        {
            // print_err("pRbMap==NULL\n");
            goto inc_counter_free_mbuf_return_size;
        }

        /* Validate IQ width */
        if(expect_comp && compMeth != XRAN_COMPMETHOD_NONE){
            if(unlikely(iqWidth != prbMapElm->iqWidth))
            {
                // print_err("iqWidth (%hhu) != prbMapElm->iqWidth (%hd)",iqWidth, prbMapElm->iqWidth);
                goto inc_counter_free_mbuf_return_size;
            }
        }
        
        if(iq_data_start && size){
            if (unlikely(p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder == XRAN_CPU_LE_BYTE_ORDER))
            {
                rte_panic("XRAN_CPU_LE_BYTE_ORDER is not supported 0x16%lx\n", (long)mb);
            /* network byte (be) order of IQ to CPU byte order (le) */
            }
            else if (likely(p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder == XRAN_NE_BE_BYTE_ORDER))
            {
                struct xran_rx_packet_ctl *pFrontHaulRxPacketCtrl = &pRbMap->sFrontHaulRxPacketCtrl[symb_id];
                int32_t npkts = pFrontHaulRxPacketCtrl->nRxPkt;
                /*if (unlikely(npkts >= 20))
                    printf("npkt %d (%d : %d : %d : %d) (start %d num %d sect %d)\n",npkts,frame_id, subframe_id, slot_id, Ant_ID,start_prbu,num_prbu,sect_id);*/
                if (unlikely(npkts >= XRAN_MAX_RX_PKT_PER_SYM))
                {
                    if (firstprint == 0) print_err("(%d : %d : %d : %d)Received %d type-1 packets on symbol %d\n", frame_id, subframe_id, slot_id, Ant_ID, npkts, symb_id);
                    firstprint = 1;
                    goto inc_counter_free_mbuf_return_size;
                }
                else
                {
                    pFrontHaulRxPacketCtrl->nRxPkt = npkts + 1;
                }
                mb = pFrontHaulRxPacketCtrl->pCtrl[npkts];
                if(mb){
                   rte_pktmbuf_free(mb);
                }
                pFrontHaulRxPacketCtrl->nRBStart[npkts] = start_prbu;
                pFrontHaulRxPacketCtrl->nRBSize[npkts] = num_prbu;
                pFrontHaulRxPacketCtrl->nSectid[npkts] = sect_id;
                pFrontHaulRxPacketCtrl->pData[npkts] = iq_data_start;
                pFrontHaulRxPacketCtrl->pCtrl[npkts] = (0 == nSectionIdx) ? mbuf : NULL;
                *mb_free = MBUF_KEEP;
                return size;
            }
        } 
    } 
    // else {
    //     print_err("o-xu%d: TTI %d(f_%d sf_%d slot_%d) CC %d Ant_ID %d symb_id %d\n",p_xran_dev_ctx->xran_port_id, tti, frame_id, subframe_id, slot_id, CC_ID, Ant_ID, symb_id);
    // }

inc_counter_free_mbuf_return_size:
    ++p_xran_dev_ctx->fh_counters.rx_err_drop;
    ++p_xran_dev_ctx->fh_counters.rx_err_up;
    ++p_xran_dev_ctx->fh_counters.rx_err_pusch;
    *mb_free = MBUF_FREE;
    return size;
}
