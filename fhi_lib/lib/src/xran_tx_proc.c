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
 * @brief XRAN TX functionality
 * @file xran_tx.c
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
#include <rte_ethdev.h>

#include "xran_fh_o_du.h"

#include "xran_ethdi.h"
#include "xran_pkt.h"
#include "xran_up_api.h"
#include "xran_cp_api.h"
#include "xran_sync_api.h"
#include "xran_lib_mlog_tasks_id.h"
#include "xran_timer.h"
#include "xran_main.h"
#include "xran_common.h"
#include "xran_dev.h"
#include "xran_frame_struct.h"
#include "xran_printf.h"
#include "xran_tx_proc.h"
#include "xran_cp_proc.h"

#include "xran_mlog_lnx.h"

#if (RTE_VER_YEAR == 20)
#ifndef RTE_MBUF_F_EXTERNAL
#define RTE_MBUF_F_EXTERNAL EXT_ATTACHED_MBUF
#endif
#endif

enum xran_in_period
{
     XRAN_IN_PREV_PERIOD  = 0,
     XRAN_IN_CURR_PERIOD,
     XRAN_IN_NEXT_PERIOD
};

extern int32_t first_call;
extern uint8_t xran_intra_sym_div[];

struct rte_mbuf *
xran_attach_up_ext_buf(uint16_t vf_id, int8_t* p_ext_buff_start, int8_t* p_ext_buff, uint16_t ext_buff_len,
                struct rte_mbuf_ext_shared_info * p_share_data,
                enum xran_compression_method compMeth, enum xran_comp_hdr_type staticEn);


static void
extbuf_free_callback(void *addr __rte_unused, void *opaque __rte_unused)
{
    /*long t1 = MLogTick();
    MLogTask(77777, t1, t1+100);*/
}

static inline int32_t XranOffsetSym(int32_t offSym, int32_t otaSym, int32_t numSymTotal /*in sec*/, enum xran_in_period* pInPeriod)
{
    int32_t sym;

    // Suppose the offset is usually small
    if (unlikely(offSym > otaSym))
    {
        sym = numSymTotal - offSym + otaSym;
        *pInPeriod = XRAN_IN_PREV_PERIOD;
    }
    else
    {
        sym = otaSym - offSym;

        if (unlikely(sym >= numSymTotal))
        {
            sym -= numSymTotal;
            *pInPeriod = XRAN_IN_NEXT_PERIOD;
        }
        else
        {
            *pInPeriod = XRAN_IN_CURR_PERIOD;
        }
    }

    return sym;
}

// Return SFN at current second start, 10 bits, [0, 1023]
uint16_t xran_getSfnSecStart(void)
{
#ifndef POLL_EBBU_OFFLOAD
    return xran_SFN_at_Sec_Start;
#else
    PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();
    return pCtx->xran_SFN_at_Sec_Start;
#endif
}

int32_t xran_process_tx_sym_cp_off(void *pHandle, uint8_t ctx_id, uint32_t tti, int32_t cc_id,
        int32_t ant_id, uint32_t frame_id, uint32_t subframe_id, uint32_t slot_id, uint32_t sym_id,
    int32_t do_srs, uint8_t mu, uint32_t tti_for_ring, uint32_t sym_id_for_ring)
{
    int32_t     retval = 0;
    char        *pos = NULL;
    char        *p_sec_iq = NULL;
    void        *mb  = NULL;
    void        *send_mb  = NULL;
    uint16_t    vf_id = 0 , num_sections = 0, curr_sect_id = 0 ;
    uint16_t    p_id = 0;
    uint8_t     ru_port_id;
    struct      rte_ring *ring = NULL;

    struct xran_prb_map *prb_map = NULL;

    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)pHandle;
    if (p_xran_dev_ctx == NULL)
        return retval;
    struct xran_common_counters * pCnt = &p_xran_dev_ctx->fh_counters;
    struct xran_ethdi_ctx* eth_ctx = xran_ethdi_get_ctx();

    enum xran_pkt_dir direction;
    enum xran_comp_hdr_type staticEn = XRAN_COMP_HDR_TYPE_DYNAMIC;

    struct rte_mbuf *eth_oran_hdr = NULL;
    char        *ext_buff = NULL;
    uint16_t    ext_buff_len = 0;
    struct rte_mbuf *tmp = NULL;
    rte_iova_t ext_buff_iova = 0;
    uint8_t PortId = p_xran_dev_ctx->xran_port_id;

    if(PortId >= XRAN_PORTS_NUM)
        rte_panic("incorrect PORT ID\n");
    if((cc_id < 0) || (cc_id >= XRAN_MAX_SECTOR_NR))
        rte_panic("incorrect CC ID\n");
    if((ant_id < 0) || (ant_id >= XRAN_MAX_ANTENNA_NR))
        rte_panic("incorrect ANT ID\n");
    if(mu >= XRAN_MAX_NUM_MU)
        rte_panic("incorrect MU\n");

    staticEn = p_xran_dev_ctx->fh_cfg.ru_conf.xranCompHdrType;
    ru_port_id = ant_id + p_xran_dev_ctx->perMu[mu].eaxcOffset;

    struct rte_mbuf_ext_shared_info * p_share_data = NULL;
    bool isNb375 = false;
    int appMode = xran_get_syscfg_appmode();

    if(appMode == O_DU)
    {
        direction = XRAN_DIR_DL; /* O-DU */
    }
    else
    {
        direction = XRAN_DIR_UL; /* RU */

        if(mu==XRAN_NBIOT_MU && XRAN_NBIOT_UL_SCS_3_75==p_xran_dev_ctx->fh_cfg.perMu[mu].nbIotUlScs)
            isNb375=true;
    }
    uint16_t mtu = p_xran_dev_ctx->mtu;

    if(xran_fs_get_slot_type(PortId, cc_id, tti, ((appMode == O_DU)? XRAN_SLOT_TYPE_DL : XRAN_SLOT_TYPE_UL), mu) ==  1
            || xran_fs_get_slot_type(PortId, cc_id, tti, XRAN_SLOT_TYPE_SP, mu) ==  1
            || xran_fs_get_slot_type(PortId, cc_id, tti, XRAN_SLOT_TYPE_FDD, mu) ==  1){

        if(xran_fs_get_symbol_type(PortId, cc_id, tti, sym_id, mu) == ((appMode == O_DU)? XRAN_SYMBOL_TYPE_DL : XRAN_SYMBOL_TYPE_UL)
           || xran_fs_get_symbol_type(PortId, cc_id, tti, sym_id, mu) == XRAN_SYMBOL_TYPE_FDD){

            vf_id = xran_map_ecpriPcid_to_vf(p_xran_dev_ctx, direction, cc_id, ru_port_id);
            p_id = eth_ctx->io_cfg.port[vf_id];
            pos = (char*) p_xran_dev_ctx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id].pData;
            mb  = (void*) p_xran_dev_ctx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id].pCtrl;
            prb_map  = (struct xran_prb_map *) p_xran_dev_ctx->perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers->pData;
            ring    = p_xran_dev_ctx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[tti_for_ring % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id_for_ring].pRing;

            if(prb_map){
                int32_t elmIdx = 0;
                uint8_t seq_id;
                for (elmIdx = 0; elmIdx < prb_map->nPrbElm && elmIdx < XRAN_MAX_SECTIONS_PER_SLOT; elmIdx++){
                    //print_err("tti is %d, cc_id is %d, ant_id is %d, prb_map->nPrbElm id - %d", tti % XRAN_N_FE_BUF_LEN, cc_id, ant_id, prb_map->nPrbElm);
                    struct xran_prb_elm * prb_map_elm = &prb_map->prbMap[elmIdx];
                    struct xran_section_desc * p_sec_desc = NULL;

                    if(unlikely(prb_map_elm == NULL)){
                        rte_panic("p_sec_desc == NULL\n");
                    }

                    uint16_t sec_id  = prb_map_elm->startSectId;
                    p_share_data = &p_xran_dev_ctx->share_data.sh_data[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id][sec_id];

                    if(unlikely(sym_id < prb_map_elm->nStartSymb || sym_id >= (prb_map_elm->nStartSymb + prb_map_elm->numSymb)))
                        continue;

                    p_sec_desc = &prb_map_elm->sec_desc[sym_id];
                    uint8_t section_pos = p_sec_desc->section_pos;

                    if(appMode == O_RU
                            && xran_get_ru_category(pHandle) == XRAN_CATEGORY_B)
                    {
                        num_sections = (prb_map_elm->bf_weight.extType == 1) ? prb_map_elm->bf_weight.numSetBFWs : 1 ;
                        if (prb_map_elm->bf_weight.extType != 1)
                            curr_sect_id = sec_id;
                    }
                    else
                        num_sections = 1;

                    if (NULL == send_mb)
                    {
                        //new pkg start
                    }
                    else
                    {
                        uint32_t send_flag = 1;
                        if (section_pos == 0)
                        {
                            send_flag = 1;
                        }
                        else
                        {
                            uint16_t pkg_len = rte_pktmbuf_pkt_len((struct rte_mbuf *)send_mb);
                            uint16_t appand_len = sizeof(struct data_section_hdr) * num_sections;
                            appand_len += ((0 == prb_map_elm->iqWidth)?16:prb_map_elm->iqWidth)*3*prb_map_elm->UP_nRBSize;
                            if ((prb_map_elm->compMethod != XRAN_COMPMETHOD_NONE)&&(staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC)){
                                appand_len += sizeof (struct data_section_compression_hdr) * num_sections;
                                appand_len += prb_map_elm->UP_nRBSize;
                            }
                            send_flag = (mtu < pkg_len + appand_len) ? 1 : 0;
                        }

                        if (send_flag)
                        {
                            rte_mbuf_sanity_check((struct rte_mbuf *)send_mb, 0);
                            if(xran_get_syscfg_bbuoffload() == 1)
                            {
                                struct rte_mbuf *send_mb_temp = (struct rte_mbuf *)send_mb;
                                send_mb_temp->port = eth_ctx->io_cfg.port[vf_id];
                                xran_add_eth_hdr_vlan(&eth_ctx->entities[vf_id][ID_O_RU], ETHER_TYPE_ECPRI, send_mb_temp);

                                if(likely(ring)) {
                                    if(rte_ring_enqueue(ring, (struct rte_mbuf *)send_mb)) {
                                        rte_panic("Ring enqueue failed. Ring free count [%d]\n",rte_ring_free_count(ring));
                                        rte_pktmbuf_free((struct rte_mbuf *)send_mb);
                                    }
                                } else
                                    rte_panic("Ring is NULL\n");
                            } else {
                                p_xran_dev_ctx->send_upmbuf2ring((struct rte_mbuf *)send_mb, ETHER_TYPE_ECPRI, vf_id);
                            }
                            pCnt->tx_counter++;
                            pCnt->tx_bytes_counter += rte_pktmbuf_pkt_len((struct rte_mbuf *)send_mb);
                            send_mb = NULL;
                        }
                    }
                    p_sec_iq     = ((char*)pos + p_sec_desc->iq_buffer_offset);
                    
                    if (NULL == send_mb)
                    {
                        if (section_pos == 2)
                        {
                            rte_panic("section_pos not fill success %d\n",section_pos);
                        }
                        /* calculate offset for external buffer */
                        ext_buff_len = p_sec_desc->iq_buffer_len;
                        ext_buff = p_sec_iq - (RTE_PKTMBUF_HEADROOM +
                                sizeof (struct xran_ecpri_hdr) +
                                sizeof (struct radio_app_common_hdr) +
                                sizeof(struct data_section_hdr));

                        ext_buff_len += RTE_PKTMBUF_HEADROOM +
                            sizeof (struct xran_ecpri_hdr) +
                            sizeof (struct radio_app_common_hdr) +
                            sizeof(struct data_section_hdr) + 18;

                        if ((prb_map_elm->compMethod != XRAN_COMPMETHOD_NONE)&&(staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC)){
                            ext_buff     -= sizeof (struct data_section_compression_hdr);
                            ext_buff_len += sizeof (struct data_section_compression_hdr);
                        }

                        eth_oran_hdr = rte_pktmbuf_alloc(_eth_mbuf_pool_vf_small[p_id]);
                        if (unlikely (( eth_oran_hdr) == NULL)) {
                            rte_panic("Failed rte_pktmbuf_alloc\n");
                        }

                        p_share_data->free_cb = extbuf_free_callback;
                        p_share_data->fcb_opaque = NULL;
                        rte_mbuf_ext_refcnt_set(p_share_data, 1);

                        ext_buff_iova = rte_mempool_virt2iova(mb);
                        if (unlikely (( ext_buff_iova) == 0)) {
                            rte_panic("Failed rte_mem_virt2iova \n");
                        }

                        if (unlikely (( (rte_iova_t)ext_buff_iova) == RTE_BAD_IOVA)) {
                            rte_panic("Failed rte_mem_virt2iova RTE_BAD_IOVA \n");
                        }

                        rte_pktmbuf_attach_extbuf(eth_oran_hdr,
                                ext_buff,
                                ext_buff_iova + RTE_PTR_DIFF(ext_buff , mb),
                                ext_buff_len,
                                p_share_data);

                        rte_pktmbuf_reset_headroom(eth_oran_hdr);

                        tmp = (struct rte_mbuf *)rte_pktmbuf_prepend(eth_oran_hdr, sizeof(struct rte_ether_hdr));
                        if (unlikely (( tmp) == NULL)) {
                            rte_panic("Failed rte_pktmbuf_prepend \n");
                        }
                        send_mb = eth_oran_hdr;

                        seq_id = (appMode == O_DU) ?
                            xran_get_updl_seqid(PortId, cc_id, ru_port_id) :
                            xran_get_upul_seqid(PortId, cc_id, ru_port_id);

                        /* first all PRBs */
                        prepare_symbol_ex(direction, curr_sect_id,
                                send_mb,
                                (uint8_t *)p_sec_iq,
                                prb_map_elm->compMethod,
                                prb_map_elm->iqWidth,
                                p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder,
                                frame_id, subframe_id, slot_id, sym_id,
                                prb_map_elm->UP_nRBStart, prb_map_elm->UP_nRBSize,
                                cc_id, ru_port_id,
                                seq_id,
                                0,
                                staticEn,
                                num_sections,
                                p_sec_desc->iq_buffer_offset, mu, isNb375,PortId);

                        curr_sect_id += num_sections;
                    }
                    else
                    {
                                                /* first all PRBs */
                        prepare_symbol_ex_appand(direction, curr_sect_id,
                                send_mb,
                                (uint8_t *)p_sec_iq,
                                prb_map_elm->compMethod,
                                prb_map_elm->iqWidth,
                                p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder,
                                frame_id, subframe_id, slot_id, sym_id,
                                prb_map_elm->UP_nRBStart, prb_map_elm->UP_nRBSize,
                                cc_id, ru_port_id,
                                seq_id,
                                0,
                                staticEn,
                                num_sections,
                                p_sec_desc->iq_buffer_offset, mu, isNb375,PortId);

                        curr_sect_id += num_sections;
                    }
                } /* for (elmIdx = 0; elmIdx < prb_map->nPrbElm && elmIdx < XRAN_MAX_SECTIONS_PER_SLOT; elmIdx++) */
                if (send_mb)
                {
                    rte_mbuf_sanity_check((struct rte_mbuf *)send_mb, 0);

                    if(xran_get_syscfg_bbuoffload() == 1)
                    {
                        struct rte_mbuf *send_mb_temp = (struct rte_mbuf *)send_mb;
                        send_mb_temp->port = eth_ctx->io_cfg.port[vf_id];
                        xran_add_eth_hdr_vlan(&eth_ctx->entities[vf_id][ID_O_RU], ETHER_TYPE_ECPRI, send_mb_temp);

                        if(likely(ring)) {
                            if(rte_ring_enqueue(ring, (struct rte_mbuf *)send_mb)) {
                                rte_panic("Ring enqueue failed. Ring free count [%d]\n",rte_ring_free_count(ring));
                                rte_pktmbuf_free((struct rte_mbuf *)send_mb);
                            }
                        } else
                            rte_panic("Ring is NULL\n");
                    } else {
                        p_xran_dev_ctx->send_upmbuf2ring((struct rte_mbuf *)send_mb, ETHER_TYPE_ECPRI, vf_id);
                    }
                    pCnt->tx_counter++;
                    pCnt->tx_bytes_counter += rte_pktmbuf_pkt_len((struct rte_mbuf *)send_mb);
                    send_mb = NULL;
                }
            } else {
                printf("(%d %d %d %d) prb_map == NULL\n", tti % XRAN_N_FE_BUF_LEN, cc_id, ant_id, sym_id);
            }

        } /* RU mode or C-Plane is not used */
    }
    return retval;
}
int32_t xran_process_tx_prach_cp_off(void *pHandle, uint8_t ctx_id, uint32_t tti, int32_t cc_id, int32_t ant_id,
        uint32_t frame_id, uint32_t subframe_id, uint32_t slot_id, uint32_t sym_id, uint8_t mu,
        uint32_t tti_for_ring, uint32_t sym_id_for_ring)
{
    int32_t     retval = 0;
    char        *pos = NULL;
    void        *mb  = NULL;
    bool        sendPrach = false;

    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)pHandle;
    if (p_xran_dev_ctx == NULL)
        return retval;
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

    enum xran_pkt_dir direction = XRAN_DIR_UL;
    uint8_t PortId = p_xran_dev_ctx->xran_port_id;


    if(PortId >= XRAN_PORTS_NUM)
        rte_panic("incorrect PORT ID\n");


    if(p_xran_dev_ctx->perMu[mu].enablePrach
            && (xran_get_syscfg_appmode() == O_RU) && (ant_id < XRAN_MAX_PRACH_ANT_NUM)){

        if(xran_fs_get_symbol_type(PortId, cc_id, tti, sym_id, mu) == XRAN_SYMBOL_TYPE_UL
                || xran_fs_get_symbol_type(PortId, cc_id, tti, sym_id, mu) == XRAN_SYMBOL_TYPE_FDD) {   /* Only RU needs to send PRACH I/Q */

            uint32_t is_prach_slot;
            if(mu != XRAN_NBIOT_MU)
            {
                is_prach_slot = xran_is_prach_slot(PortId, subframe_id, slot_id, mu);
                if(((frame_id % pPrachCPConfig->x) == pPrachCPConfig->y[0])
                                    && (is_prach_slot == 1)
                                    && (sym_id >= p_xran_dev_ctx->prach_start_symbol[cc_id])
                                    && (sym_id <= p_xran_dev_ctx->prach_last_symbol[cc_id]))
                {
                    sendPrach = 1;
                }
                else
                    sendPrach =0;
            }
            else
            {
                sendPrach=0;
                for(uint8_t k=0; k<NBIOT_PRACH_NUM_SYM_GROUPS; k++)
                {
                    if( (p_xran_dev_ctx->perMu[mu].sfSymToXmitPrach[k].sf == subframe_id) &&
                            (p_xran_dev_ctx->perMu[mu].sfSymToXmitPrach[k].sym == sym_id) )
                    {
                        sendPrach=1;
                    }
                }
            }

            if(sendPrach)
            {
                int prach_port_id = ant_id + pPrachCPConfig->prachEaxcOffset;
                int compMethod;
                //int parm_size;
                uint8_t symb_id_offset = sym_id - p_xran_dev_ctx->prach_start_symbol[cc_id];
                struct rte_ring *pRing = NULL;
                uint16_t vf_id = 0;

                compMethod = p_xran_dev_ctx->fh_cfg.ru_conf.compMeth_PRACH;
                pos = (char*) p_xran_dev_ctx->perMu[mu].sFHPrachRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[symb_id_offset].pData;
                //pos += (sym_id - p_xran_dev_ctx->prach_start_symbol[cc_id]) * pPrachCPConfig->numPrbc * N_SC_PER_PRB * 4;
                /*pos += (sym_id - p_xran_dev_ctx->prach_start_symbol[cc_id])
                 * (3*p_xran_dev_ctx->fh_cfg.ru_conf.iqWidth + parm_size)
                 * pPrachCPConfig->numPrbc;*/
                mb  = NULL;//(void*) p_xran_dev_ctx->perMu[mu].sFHPrachRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[0].pCtrl;
                vf_id = xran_map_ecpriPcid_to_vf(p_xran_dev_ctx, direction, cc_id, prach_port_id);
                pRing    = p_xran_dev_ctx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[tti_for_ring % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id_for_ring].pRing;


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


                if (1500 == p_xran_dev_ctx->mtu && pPrachCPConfig->filterIdx == XRAN_FILTERINDEX_PRACH_012)
                {
                    pos = (char*) p_xran_dev_ctx->perMu[mu].sFHPrachRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[0].pData;
                    mb  = (void*) p_xran_dev_ctx->perMu[mu].sFHPrachRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[0].pCtrl;
                    /*one prach for more then one pkg*/
                    send_symbol_mult_section_ex(pHandle,
                        direction,
                        xran_alloc_sectionid(pHandle, direction, cc_id, prach_port_id, tti),
                        (struct rte_mbuf *)mb,
                        (uint8_t *)pos,
                        compMethod,
                        p_xran_dev_ctx->fh_cfg.ru_conf.iqWidth,
                        p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder,
                        frame_id, subframe_id, slot_id, sym_id,
                        pPrachCPConfig->startPrbc, pPrachCPConfig->numPrbc,
                        pRing, vf_id,
                        cc_id, prach_port_id,
                        0, mu);
                }
                else{
                    send_symbol_ex(pHandle,
                        direction,
                        xran_alloc_sectionid(pHandle, direction, cc_id, prach_port_id, tti),
                        (struct rte_mbuf *)mb,
                        (uint8_t *)pos,
                        compMethod,
                        p_xran_dev_ctx->fh_cfg.ru_conf.iqWidth_PRACH,
                        p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder,
                        frame_id, subframe_id, slot_id, sym_id,
                        pPrachCPConfig->startPrbc, pPrachCPConfig->numPrbc,
                        pRing, vf_id,
                        cc_id, prach_port_id,
                        xran_get_upul_seqid(PortId, cc_id, prach_port_id), mu);
                }
                retval = 1;
            }
        } /* if(p_xran_dev_ctx->enablePrach ..... */
    }
  return retval;
}

int32_t
xran_process_tx_srs_cp_off(void *pHandle, uint8_t ctx_id, uint32_t tti, int32_t cc_id, int32_t ant_id,
            uint32_t frame_id, uint32_t subframe_id, uint32_t slot_id)
{
    uint8_t     mu;
    int32_t     retval = 0;
    char        *pos = NULL;
    char        *p_sec_iq = NULL;
    void        *mb  = NULL;
    char        *ext_buff = NULL;
    uint16_t    ext_buff_len = 0 , num_sections=0; //, section_id=0;
    int32_t     antElm_eAxC_id;
    uint32_t    vf_id = 0;
    int32_t     elmIdx;
    uint32_t    sym_id;
    enum xran_pkt_dir direction;
    enum xran_comp_hdr_type staticEn;

    rte_iova_t ext_buff_iova = 0;
    struct rte_mbuf *tmp = NULL;
    struct xran_prb_map *prb_map = NULL;
    struct xran_device_ctx * p_xran_dev_ctx;
    struct xran_common_counters *pCnt;
    //struct xran_prach_cp_config *pPrachCPConfig;
    struct xran_srs_config *p_srs_cfg;
    struct rte_mbuf *eth_oran_hdr = NULL;
    struct rte_mbuf_ext_shared_info *p_share_data = NULL;


    p_xran_dev_ctx  = (struct xran_device_ctx *)pHandle;
    if(p_xran_dev_ctx == NULL)
    {
        print_err("dev_ctx is NULL. ctx_id=%d, tti=%d, cc_id=%d, ant_id=%d, frame_id=%d, subframe_id=%d, slot_id=%d\n",
                    ctx_id, tti, cc_id, ant_id, frame_id, subframe_id, slot_id);
        return 0;
    }
    mu = p_xran_dev_ctx->fh_cfg.mu_number[0];
    if(p_xran_dev_ctx->xran_port_id >= XRAN_PORTS_NUM)
        rte_panic("incorrect PORT ID\n");

    pCnt            = &p_xran_dev_ctx->fh_counters;
    //pPrachCPConfig  = &(p_xran_dev_ctx->perMu[mu].PrachCPConfig);
    p_srs_cfg       = &(p_xran_dev_ctx->srs_cfg);

    /* Only O-RU sends SRS U-Plane */
    direction   = XRAN_DIR_UL;
    staticEn    = p_xran_dev_ctx->fh_cfg.ru_conf.xranCompHdrType;
    antElm_eAxC_id  = ant_id + p_srs_cfg->srsEaxcOffset;

    prb_map = (struct xran_prb_map *)p_xran_dev_ctx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers->pData;
    if(prb_map)
    {
        for(elmIdx = 0; elmIdx < prb_map->nPrbElm && elmIdx < XRAN_MAX_SECTIONS_PER_SLOT; elmIdx++)
        {
            struct xran_prb_elm *prb_map_elm = &prb_map->prbMap[elmIdx];
            struct xran_section_desc * p_sec_desc = NULL;

            if(prb_map_elm == NULL)
                rte_panic("p_sec_desc == NULL\n");

            sym_id  = prb_map->prbMap[elmIdx].nStartSymb;
            pos     = (char*)p_xran_dev_ctx->perMu[mu].sFHSrsRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id].pData;
            mb      = (void*)p_xran_dev_ctx->perMu[mu].sFHSrsRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id].pCtrl;


            p_share_data    = &p_xran_dev_ctx->srs_share_data.sh_data[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id];
            p_sec_desc      = &prb_map_elm->sec_desc[sym_id];
            p_sec_iq        = ((char*)pos + p_sec_desc->iq_buffer_offset);

            /* calculate offset for external buffer */
            ext_buff_len = p_sec_desc->iq_buffer_len;

            ext_buff = p_sec_iq - (RTE_PKTMBUF_HEADROOM +
                            sizeof (struct xran_ecpri_hdr) +
                            sizeof (struct radio_app_common_hdr) +
                            sizeof(struct data_section_hdr));

            ext_buff_len += RTE_PKTMBUF_HEADROOM +
                            sizeof (struct xran_ecpri_hdr) +
                            sizeof (struct radio_app_common_hdr) +
                            sizeof(struct data_section_hdr) + 18;

            if((prb_map_elm->compMethod != XRAN_COMPMETHOD_NONE)
                && (staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC))
            {
                ext_buff     -= sizeof (struct data_section_compression_hdr);
                ext_buff_len += sizeof (struct data_section_compression_hdr);
            }

            eth_oran_hdr = xran_ethdi_mbuf_indir_alloc();
            if(unlikely(eth_oran_hdr == NULL))
                rte_panic("Failed rte_pktmbuf_alloc\n");

            p_share_data->free_cb = extbuf_free_callback;
            p_share_data->fcb_opaque = NULL;
            rte_mbuf_ext_refcnt_set(p_share_data, 1);

            ext_buff_iova = rte_mempool_virt2iova(mb);
            if(unlikely(ext_buff_iova == 0 || ext_buff_iova == RTE_BAD_IOVA))
                rte_panic("Failed rte_mem_virt2iova : %lu\n", ext_buff_iova);

            rte_pktmbuf_attach_extbuf(eth_oran_hdr,
                                      ext_buff,
                                      ext_buff_iova + RTE_PTR_DIFF(ext_buff , mb),
                                      ext_buff_len,
                                      p_share_data);

            rte_pktmbuf_reset_headroom(eth_oran_hdr);

            tmp = (struct rte_mbuf *)rte_pktmbuf_prepend(eth_oran_hdr, sizeof(struct rte_ether_hdr));
            if(unlikely(tmp == NULL))
                rte_panic("Failed rte_pktmbuf_prepend \n");

            uint8_t seq_id = xran_get_upul_seqid(p_xran_dev_ctx->xran_port_id, cc_id, antElm_eAxC_id);

            num_sections = (prb_map_elm->bf_weight.extType == 1) ? prb_map_elm->bf_weight.numSetBFWs : 1 ;

            prepare_symbol_ex(direction, prb_map_elm->startSectId,
                              (void *)eth_oran_hdr, (uint8_t *)p_sec_iq,
                              prb_map_elm->compMethod, prb_map_elm->iqWidth,
                              p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder,
                              frame_id, subframe_id, slot_id, sym_id,
                              prb_map_elm->UP_nRBStart, prb_map_elm->UP_nRBSize,
                              cc_id, antElm_eAxC_id,
                              seq_id,
                              0,
                              staticEn,
                              num_sections,
                              0,
                              mu, false,
                              XRAN_GET_OXU_PORT_ID(p_xran_dev_ctx));

            //section_id += num_sections;

            rte_mbuf_sanity_check(eth_oran_hdr, 0);

            vf_id = xran_map_ecpriPcid_to_vf(p_xran_dev_ctx, direction, cc_id, antElm_eAxC_id);
            pCnt->tx_counter++;
            pCnt->tx_bytes_counter += rte_pktmbuf_pkt_len(eth_oran_hdr);
            p_xran_dev_ctx->send_upmbuf2ring(eth_oran_hdr, ETHER_TYPE_ECPRI, vf_id);
        } /* for(elmIdx = 0; elmIdx < prb_map->nPrbElm && elmIdx < XRAN_MAX_SECTIONS_PER_SLOT; elmIdx++) */
    } /* if(prb_map) */
    else
    {
        printf("(%d %d %d) prb_map == NULL\n", tti % XRAN_N_FE_BUF_LEN, cc_id, antElm_eAxC_id);
    }

    return retval;
}

struct rte_mbuf *
xran_attach_up_ext_buf(uint16_t vf_id, int8_t* p_ext_buff_start, int8_t* p_ext_buff, uint16_t ext_buff_len,
                struct rte_mbuf_ext_shared_info * p_share_data,
                enum xran_compression_method compMeth, enum xran_comp_hdr_type staticEn)
{
    struct xran_ethdi_ctx* eth_ctx = xran_ethdi_get_ctx();
	uint16_t p_id = eth_ctx->io_cfg.port[vf_id];
    struct rte_mbuf *mb_oran_hdr_ext = NULL;
    struct rte_mbuf *tmp             = NULL;
    int8_t          *ext_buff        = NULL;
    rte_iova_t ext_buff_iova         = 0;
    ext_buff =      p_ext_buff - (RTE_PKTMBUF_HEADROOM +
                    sizeof(struct xran_ecpri_hdr) +
                    sizeof(struct radio_app_common_hdr) +
                    sizeof(struct data_section_hdr));

    ext_buff_len += RTE_PKTMBUF_HEADROOM +
                    sizeof(struct xran_ecpri_hdr) +
                    sizeof(struct radio_app_common_hdr) +
                    sizeof(struct data_section_hdr) + 18;
    if ((compMeth != XRAN_COMPMETHOD_NONE)&&(staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC)) {
        ext_buff     -= sizeof (struct data_section_compression_hdr);
        ext_buff_len += sizeof (struct data_section_compression_hdr);
    }
    mb_oran_hdr_ext =  rte_pktmbuf_alloc(_eth_mbuf_pool_vf_small[p_id]);

    if (unlikely (( mb_oran_hdr_ext) == NULL)) {
        rte_panic("[core %d]Failed rte_pktmbuf_alloc on vf %d\n", rte_lcore_id(), vf_id);
    }

    p_share_data->free_cb = extbuf_free_callback;
    p_share_data->fcb_opaque = NULL;
    rte_mbuf_ext_refcnt_set(p_share_data, 1);

    ext_buff_iova = rte_mempool_virt2iova(p_ext_buff_start);
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

    tmp = (struct rte_mbuf *)rte_pktmbuf_prepend(mb_oran_hdr_ext, sizeof(struct rte_ether_hdr));
    if (unlikely (( tmp) == NULL)) {
        rte_panic("Failed rte_pktmbuf_prepend \n");
    }

    return mb_oran_hdr_ext;
}

int32_t xran_process_tx_sym_cp_on_dispatch_opt(void* pHandle, uint8_t ctx_id, uint32_t tti, int32_t start_cc, int32_t num_cc, int32_t start_ant, int32_t num_ant, uint32_t frame_id,
    uint32_t subframe_id, uint32_t slot_id, uint32_t sym_id, enum xran_comp_hdr_type compType, enum xran_pkt_dir direction,
    uint16_t xran_port_id, PSECTION_DB_TYPE p_sec_db, uint8_t mu, uint32_t tti_for_ring, uint32_t sym_id_for_ring, bool isVmu)
{
    int32_t     retval = 0;
    struct cp_up_tx_desc*   p_desc = NULL;
    struct xran_ethdi_ctx*  eth_ctx = xran_ethdi_get_ctx();
    struct xran_device_ctx* p_xran_dev_ctx = (struct xran_device_ctx*)pHandle;

    p_desc = xran_pkt_gen_desc_alloc();
    if(p_desc) {
        p_desc->pHandle     = pHandle;
        p_desc->ctx_id      = ctx_id;
        p_desc->tti         = tti;
        p_desc->start_cc    = start_cc;
        p_desc->cc_num      = num_cc;
        p_desc->start_ant   = start_ant;
        p_desc->ant_num     = num_ant;
        p_desc->frame_id    = frame_id;
        p_desc->subframe_id = subframe_id;
        p_desc->slot_id     = slot_id;
        p_desc->sym_id      = sym_id;
        p_desc->compType    = (uint32_t)compType;
        p_desc->direction   = (uint32_t)direction;
        p_desc->xran_port_id= xran_port_id;
        p_desc->p_sec_db    = (void*)p_sec_db;
        p_desc->mu          = mu;
        p_desc->tti_for_ring = tti_for_ring;
        p_desc->sym_id_for_ring = sym_id_for_ring;

        if(likely(p_xran_dev_ctx->xran_port_id < XRAN_PORTS_NUM)) {
            if (rte_ring_enqueue(eth_ctx->up_dl_pkt_gen_ring[p_xran_dev_ctx->xran_port_id], p_desc->mb) == 0)
                return 1;   /* success */
            else
            {
                rte_panic("\n\nFailed to enqueue : RU%d CC%d Ant%02d sym%02d tti=%d\n", p_xran_dev_ctx->xran_port_id,
                    start_cc, start_ant, sym_id, tti);
                xran_pkt_gen_desc_free(p_desc);
            }
        } else {
            rte_panic("incorrect port %d", p_xran_dev_ctx->xran_port_id);
        }
    } else {
        rte_panic("xran_pkt_gen_desc_alloc failure %d", p_xran_dev_ctx->xran_port_id);
    }

    return retval;
}

int32_t do_nothing(void* pHandle, uint8_t ctx_id, uint32_t tti, int32_t start_cc, int32_t num_cc, int32_t start_ant,  int32_t num_ant, uint32_t frame_id,
    uint32_t subframe_id, uint32_t slot_id, uint32_t sym_id, enum xran_comp_hdr_type compType, enum xran_pkt_dir direction,
    uint16_t xran_port_id, PSECTION_DB_TYPE p_sec_db, uint8_t mu, uint32_t tti_for_ring, uint32_t sym_id_for_ring, bool isVmu)
{
    return 0;
}

int32_t xran_enqueue_timing_info_for_tx(void* pHandle, uint8_t ctx_id, uint32_t tti, int32_t start_cc, int32_t num_cc, int32_t start_ant, int32_t num_ant, uint32_t frame_id,
    uint32_t subframe_id, uint32_t slot_id, uint32_t sym_id, enum xran_comp_hdr_type compType, enum xran_pkt_dir direction,
    uint16_t xran_port_id, PSECTION_DB_TYPE p_sec_db, uint8_t mu, uint32_t tti_for_ring, uint32_t sym_id_for_ring)
{
    int32_t     retval = 0;
    struct cp_up_tx_desc*   p_desc = NULL;
    struct xran_ethdi_ctx*  eth_ctx = xran_ethdi_get_ctx();
    struct xran_device_ctx* p_xran_dev_ctx = (struct xran_device_ctx*)pHandle;

    p_desc = xran_pkt_gen_desc_alloc();
    if(p_desc) {
        p_desc->pHandle     = pHandle;
        p_desc->ctx_id      = ctx_id;
        p_desc->tti         = tti;
        p_desc->start_cc    = start_cc;
        p_desc->cc_num      = num_cc;
        p_desc->start_ant   = start_ant;
        p_desc->ant_num     = num_ant;
        p_desc->frame_id    = frame_id;
        p_desc->subframe_id = subframe_id;
        p_desc->slot_id     = slot_id;
        p_desc->sym_id      = sym_id;
        p_desc->compType    = (uint32_t)compType;
        p_desc->direction   = (uint32_t)direction;
        p_desc->xran_port_id= xran_port_id;
        p_desc->p_sec_db    = (void*)p_sec_db;
        p_desc->mu          = mu;
        p_desc->tti_for_ring = tti_for_ring;
        p_desc->sym_id_for_ring = sym_id_for_ring;

        if(likely(p_xran_dev_ctx->xran_port_id < XRAN_PORTS_NUM)) {
            if (rte_ring_enqueue(eth_ctx->up_dl_pkt_gen_ring[p_xran_dev_ctx->xran_port_id], p_desc->mb) == 0)
            {
                //printf("timing info enqueue success\n");
                return 1;   /* success */
            }
            else{
             //       printf("timing info enqueue failed\n");
                xran_pkt_gen_desc_free(p_desc);
                }
        } else {
            rte_panic("incorrect port %d", p_xran_dev_ctx->xran_port_id);
        }
    } else {
        print_err("xran_pkt_gen_desc_alloc failure %d\n", p_xran_dev_ctx->xran_port_id);
    }

    return retval;
}

int32_t xran_tx_sym_cp_off(void* arg, uint8_t mu)
{
    int32_t     retval = 0;
    struct tx_sym_desc*   p_desc = NULL;
    struct xran_ethdi_ctx*  eth_ctx = xran_ethdi_get_ctx();
    struct xran_device_ctx* p_xran_dev_ctx = (struct xran_device_ctx*)arg;

    p_desc = xran_tx_sym_gen_desc_alloc();
    if(p_desc) {

        p_desc->arg = arg;
        p_desc->mu = mu;

        if(likely(p_xran_dev_ctx->xran_port_id < XRAN_PORTS_NUM)) {
            if (rte_ring_enqueue(eth_ctx->up_dl_pkt_gen_ring[p_xran_dev_ctx->xran_port_id], p_desc->mb) == 0)
                return 1;   /* success */
            else
                xran_tx_sym_gen_desc_free(p_desc);
        } else {
            rte_panic("incorrect port %d", p_xran_dev_ctx->xran_port_id);
        }
    } else {
        print_dbg("xran_tx_sym_cp_off failure %d", p_xran_dev_ctx->xran_port_id);
    }

    return retval;
}


int32_t
xran_prepare_up_dl_sym(uint16_t xran_port_id, uint32_t nSlotIdx,  uint32_t nCcStart, uint32_t nCcNum, uint32_t nSymMask, uint32_t nAntStart,
                            uint32_t nAntNum, uint32_t nSymStart, uint32_t nSymNum, uint8_t mu)

{
    int32_t     retval = 0;
    uint32_t    tti=0, tti_for_ring=0, xranTti=0;
    uint32_t    numSlotMu1 = 5;
#if XRAN_MLOG_VAR
    uint32_t    mlogVar[15];
    uint32_t    mlogVarCnt = 0;
#endif
    unsigned long t1 = MLogXRANTick();

    void        *pHandle = NULL;
    int32_t     ant_id   = 0;
    int32_t     cc_id    = 0;
    uint8_t     num_eAxc = 0;
    uint8_t     num_eAxc_prach = 0;
    uint8_t     num_eAxAntElm = 0;
    uint8_t     num_CCPorts = 0;
    uint32_t    frame_id    = 0;
    uint32_t    subframe_id = 0;
    uint32_t    slot_id     = 0;
    uint32_t    sym_id      = 0;
    int32_t     sym_idx_to_send  = 0, sym_idx_for_ring=0, sym_id_for_ring=0;
    uint8_t     portId      = 0;
    uint32_t    idxSym;
    uint8_t     ctx_id;
    enum xran_in_period inPeriod;
    uint32_t interval;

    struct xran_device_ctx * p_xran_dev_ctx = NULL;

    p_xran_dev_ctx = xran_dev_get_ctx_by_id(xran_port_id);

    if(p_xran_dev_ctx == NULL)
        return 0;

    if(p_xran_dev_ctx->xran2phy_mem_ready == 0)
        return 0;

    if(mu == XRAN_DEFAULT_MU)
        mu = p_xran_dev_ctx->fh_cfg.mu_number[0];

    if(unlikely(p_xran_dev_ctx->xran_port_id >= XRAN_PORTS_NUM))
    {
        print_err("Invalid Port id - %d", p_xran_dev_ctx->xran_port_id);
        return 0;
    }

    interval = xran_fs_get_tti_interval(mu);
    portId = p_xran_dev_ctx->xran_port_id;

    pHandle =  p_xran_dev_ctx;

    xranTti = nSlotIdx % xran_fs_get_max_slot(mu);

    for (idxSym = nSymStart; idxSym < (nSymStart + nSymNum) && idxSym < XRAN_NUM_OF_SYMBOL_PER_SLOT; idxSym++) {
        t1 = MLogXRANTick();
        if(((1 << idxSym) & nSymMask) ) {
            sym_idx_to_send = xranTti*XRAN_NUM_OF_SYMBOL_PER_SLOT + idxSym;

            /* We are assuming that we will send in the n-1 slot wrt ota here */
            if((sym_idx_to_send + p_xran_dev_ctx->perMu[mu].sym_up) < 0)
            {
                // sym_idx_for_ring = XRAN_NUM_OF_SYMBOL_PER_SLOT*SLOTNUM_PER_SUBFRAME(interval)*1000 + p_xran_dev_ctx->perMu[mu].sym_up;
                sym_idx_for_ring = XRAN_NUM_OF_SYMBOL_PER_SLOT*SLOTNUM_PER_SUBFRAME(interval)*1000 + idxSym + p_xran_dev_ctx->perMu[mu].sym_up;
            }
            else
            {
                sym_idx_for_ring = sym_idx_to_send + p_xran_dev_ctx->perMu[mu].sym_up;
            }

            sym_idx_to_send = XranOffsetSym(0, sym_idx_to_send, XRAN_NUM_OF_SYMBOL_PER_SLOT*SLOTNUM_PER_SUBFRAME(interval)*1000, &inPeriod);

            tti         = XranGetTtiNum(sym_idx_to_send, XRAN_NUM_OF_SYMBOL_PER_SLOT);
            tti_for_ring = XranGetTtiNum(sym_idx_for_ring, XRAN_NUM_OF_SYMBOL_PER_SLOT);
            slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME(interval));
            subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME(interval),  SUBFRAMES_PER_SYSTEMFRAME);

            frame_id    =  (nSlotIdx / SLOTS_PER_SYSTEMFRAME(interval)) & 0x3FF;
            // ORAN frameId, 8 bits, [0, 255]
            frame_id = (frame_id & 0xff);

            sym_id      = XranGetSymNum(sym_idx_to_send, XRAN_NUM_OF_SYMBOL_PER_SLOT);
            sym_id_for_ring      = XranGetSymNum(sym_idx_for_ring, XRAN_NUM_OF_SYMBOL_PER_SLOT);
            ctx_id      = tti % XRAN_MAX_SECTIONDB_CTX;

           /* if(nAntStart==0 && idxSym==0)
            printf("UP otaTti=%u, ttiInPkt=%u, symIdPkt=%u, tti_to_send_on=%u, sym_id_to_send_on=%u\n",
                XranGetTtiNum(xran_lib_ota_sym_idx_mu[mu], XRAN_NUM_OF_SYMBOL_PER_SLOT) % XRAN_N_FE_BUF_LEN,
                nSlotIdx % XRAN_N_FE_BUF_LEN, sym_id, tti_for_ring% XRAN_N_FE_BUF_LEN, sym_id_for_ring);
            static int x=5000;
            if(0==x--)
                {exit(1);}*/

            print_dbg("[%d]SFN %d sf %d slot %d\n", tti, frame_id, subframe_id, slot_id);

#if XRAN_MLOG_VAR
            mlogVarCnt = 0;
            mlogVar[mlogVarCnt++] = 0xAAAAAAAA;
            mlogVar[mlogVarCnt++] = nSlotIdx;
            mlogVar[mlogVarCnt++] = idxSym;
            mlogVar[mlogVarCnt++] = abs(p_xran_dev_ctx->perMu[mu].sym_up);
            mlogVar[mlogVarCnt++] = tti;
            mlogVar[mlogVarCnt++] = frame_id;
            mlogVar[mlogVarCnt++] = subframe_id;
            mlogVar[mlogVarCnt++] = slot_id;
            mlogVar[mlogVarCnt++] = sym_id;
            mlogVar[mlogVarCnt++] = portId;
            MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());
#endif

            int appMode = xran_get_syscfg_appmode();

            if(appMode == O_RU)
            {
                    num_eAxc    = xran_get_num_eAxcUl(pHandle);
            }
            else
            {
                    num_eAxc    = xran_get_num_eAxc(pHandle);
            }

            num_eAxc_prach = ((num_eAxc > XRAN_MAX_PRACH_ANT_NUM)? XRAN_MAX_PRACH_ANT_NUM : num_eAxc);
            num_CCPorts = xran_get_num_cc(pHandle);

            /* U-Plane */
            if(appMode == O_DU && p_xran_dev_ctx->enableCP) {
                enum xran_comp_hdr_type compType;
                enum xran_pkt_dir direction;
                //uint32_t prb_num;
                uint32_t loc_ret = 1;
                PSECTION_DB_TYPE p_sec_db = NULL;

                compType = p_xran_dev_ctx->fh_cfg.ru_conf.xranCompHdrType;

                if(appMode == O_DU)
                {
                    direction = XRAN_DIR_DL; /* O-DU */
                    //prb_num = p_xran_dev_ctx->fh_cfg.nDLRBs;
                }
                else
                {
                    direction = XRAN_DIR_UL; /* RU */
                    //prb_num = p_xran_dev_ctx->fh_cfg.nULRBs;
                }

                if(unlikely(ctx_id > XRAN_MAX_SECTIONDB_CTX))
                {
                    print_err("Invalid Context id - %d", ctx_id);
                    loc_ret = 0;
                }

                if(unlikely(direction > XRAN_DIR_MAX))
                {
                    print_err("Invalid direction - %d", direction);
                    loc_ret = 0;
                }

                if(unlikely(num_CCPorts > XRAN_COMPONENT_CARRIERS_MAX))
                {
                    print_err("Invalid CC id - %d", num_CCPorts);
                    loc_ret = 0;
                }

                if(unlikely(num_eAxc > (XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR)))
                {
                    print_err("Invalid eAxC id - %d", num_eAxc);
                    loc_ret = 0;
                }

                p_sec_db = p_sectiondb[portId][mu];
                if(unlikely(p_sec_db == NULL))
                {
                    print_err("p_sec_db == NULL\n");
                    loc_ret = 0;
                }

                if (loc_ret) {
                    retval = xran_process_tx_sym_cp_on_opt(pHandle, ctx_id, tti,
                                        nCcStart, nCcNum, (nAntStart + p_xran_dev_ctx->perMu[mu].eaxcOffset), nAntNum, frame_id, subframe_id, slot_id, sym_id,
                                        compType, direction, portId, p_sec_db, mu, tti_for_ring, sym_id_for_ring, 0);
                } else {
                    print_err("loc_ret %d\n", loc_ret);
                    retval = 0;
                }

                if(xran_get_ru_category(pHandle) == XRAN_CATEGORY_B &&
                        p_xran_dev_ctx->csirsEnable && nAntStart == 0)
                {
                    retval = xran_process_tx_csirs_cp_on(pHandle, ctx_id, tti,
                                        nCcStart, nCcNum, 0 /*pCsirsCfg->csirsEaxcOffset*/, XRAN_MAX_CSIRS_PORTS, frame_id, subframe_id, slot_id, sym_id,
                                        compType, XRAN_DIR_DL, portId, p_sec_db, mu, tti_for_ring, sym_id_for_ring);
                }

                if(xran_get_ru_category(pHandle) == XRAN_CATEGORY_A && p_xran_dev_ctx->vMuInfo.ssbInfo.ssbMu > mu){
                    if(mu == 3 && p_xran_dev_ctx->vMuInfo.ssbInfo.ssbMu == 4){

                        uint8_t NumSsbSymPerTimerSym = (1 << (p_xran_dev_ctx->vMuInfo.ssbInfo.ssbMu - mu));
                        PSECTION_DB_TYPE ssb_p_sec_db = p_sectiondb[portId][p_xran_dev_ctx->vMuInfo.ssbInfo.ssbMu];

                        if(likely(loc_ret)){
                            for(uint8_t m = 0; m < NumSsbSymPerTimerSym; ++m){
                                // Calculate SSB sym, slot, sf id corresponding to sym_idx_to_send
                                uint32_t ssbSymIdx_to_send = (sym_idx_to_send * NumSsbSymPerTimerSym) + m;
                                uint32_t ssbSym_id         = ssbSymIdx_to_send % XRAN_NUM_OF_SYMBOL_PER_SLOT;
                                uint32_t ssbTti            = ssbSymIdx_to_send / XRAN_NUM_OF_SYMBOL_PER_SLOT;
                                uint32_t ssbSlotId         = ssbTti % SLOTNUM_PER_SUBFRAME_MU(p_xran_dev_ctx->vMuInfo.ssbInfo.ssbMu);
                                uint32_t ssbSfId           = (ssbSlotId / SLOTNUM_PER_SUBFRAME_MU(p_xran_dev_ctx->vMuInfo.ssbInfo.ssbMu)) % SUBFRAMES_PER_SYSTEMFRAME;

                                retval = xran_process_tx_sym_cp_on_opt(pHandle, (ssbTti % XRAN_MAX_SECTIONDB_CTX), ssbTti, nCcStart, nCcNum, 
                                                            p_xran_dev_ctx->vMuInfo.ssbInfo.ruPortId_offset, nAntNum, frame_id, ssbSfId, ssbSlotId, ssbSym_id, compType, XRAN_DIR_DL,
                                                            p_xran_dev_ctx->xran_port_id, ssb_p_sec_db, mu, tti_for_ring, sym_id_for_ring, 1);

                                if(unlikely(retval != XRAN_STATUS_SUCCESS)){
                                    print_err("SSB UP failed");
                                }
                            }
                        }
                    }
                }

            } else {
                for (ant_id = 0; ant_id < num_eAxc; ant_id++) {
                    for (cc_id = 0; cc_id < num_CCPorts; cc_id++) {
                        //struct xran_srs_config *p_srs_cfg = &(p_xran_dev_ctx->srs_cfg);
                        if(p_xran_dev_ctx->puschMaskEnable)
                        {
                            if((tti % numSlotMu1) != p_xran_dev_ctx->puschMaskSlot)
                                retval = xran_process_tx_sym_cp_off(pHandle, ctx_id, tti, cc_id, ant_id, frame_id, subframe_id, slot_id, sym_id, 0, mu, tti_for_ring, sym_id_for_ring);
                        }
                        else
                            retval = xran_process_tx_sym_cp_off(pHandle, ctx_id, tti, cc_id, ant_id, frame_id, subframe_id, slot_id, sym_id, 0, mu, tti_for_ring, sym_id_for_ring);

                        if(p_xran_dev_ctx->perMu[mu].enablePrach && (ant_id < num_eAxc_prach) )
                        {
                            retval = xran_process_tx_prach_cp_off(pHandle, ctx_id, tti, cc_id, ant_id, frame_id, subframe_id, slot_id, sym_id, mu, tti_for_ring, sym_id_for_ring);
                        }
                    }
                }
            }

            /* SRS U-Plane, only for O-RU emulation with Cat B */
            if(appMode == O_RU
                    && xran_get_ru_category(pHandle) == XRAN_CATEGORY_B
                    && p_xran_dev_ctx->enableSrs
                    && ((p_xran_dev_ctx->srs_cfg.symbMask >> idxSym)&1))
            {
                struct xran_srs_config *pSrsCfg = &(p_xran_dev_ctx->srs_cfg);

                for(cc_id = 0; cc_id < num_CCPorts; cc_id++)
                {
                    /* check special frame */
                    if((xran_fs_get_slot_type(portId, cc_id, tti, XRAN_SLOT_TYPE_SP, mu) ==  1)
                        || (xran_fs_get_slot_type(portId, cc_id, tti, XRAN_SLOT_TYPE_UL, mu) ==  1))
                    {
                        if(((tti % p_xran_dev_ctx->fh_cfg.frame_conf.nTddPeriod) == pSrsCfg->slot)
                            && (p_xran_dev_ctx->ndm_srs_scheduled == 0))
                        {
                            struct xran_prb_map *prb_map;
                            prb_map = (struct xran_prb_map *)p_xran_dev_ctx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][0].sBufferList.pBuffers->pData;

                            /* if PRB map is present in first antenna, assume SRS might be scheduled. */
                            if(prb_map && prb_map->nPrbElm)
                            {
                                /* NDM U-Plane is not enabled */
                                if(pSrsCfg->ndm_offset == 0)
                                {

                                    if (prb_map->nPrbElm > 0)
                                    {
                                        /* Check symbol range in PRB Map */
                                        if(sym_id >= prb_map->prbMap[0].nStartSymb
                                            && sym_id < (prb_map->prbMap[0].nStartSymb + prb_map->prbMap[0].numSymb))
                                            for(ant_id=0; ant_id < xran_get_num_ant_elm(pHandle); ant_id++)
                                                xran_process_tx_srs_cp_off(pHandle, ctx_id, tti, cc_id, ant_id, frame_id, subframe_id, slot_id);
                                    }

                                }
                                /* NDM U-Plane is enabled, SRS U-Planes will be transmitted after ndm_offset (in slots) */
                                else
                                {
                                    p_xran_dev_ctx->ndm_srs_scheduled   = 1;
                                    p_xran_dev_ctx->ndm_srs_tti         = tti;
                                    p_xran_dev_ctx->ndm_srs_txtti       = (tti + pSrsCfg->ndm_offset)%2000;
                                    p_xran_dev_ctx->ndm_srs_schedperiod = pSrsCfg->slot;
                                }
                            }
                        }
                    }
                    /* check SRS NDM UP has been scheduled in non special slots */
                    else if(p_xran_dev_ctx->ndm_srs_scheduled
                            && p_xran_dev_ctx->ndm_srs_txtti == tti)
                    {
                        int ndm_step;
                        uint32_t srs_tti, srsFrame, srsSubframe, srsSlot;
                        uint8_t  srsCtx;

                        srs_tti     = p_xran_dev_ctx->ndm_srs_tti;
                        num_eAxAntElm = xran_get_num_ant_elm(pHandle);
                        ndm_step    = num_eAxAntElm / pSrsCfg->ndm_txduration;

                        srsSlot     = XranGetSlotNum(srs_tti, SLOTNUM_PER_SUBFRAME(interval));
                        srsSubframe = XranGetSubFrameNum(srs_tti,SLOTNUM_PER_SUBFRAME(interval),  SUBFRAMES_PER_SYSTEMFRAME);
                        srsFrame    =  (nSlotIdx / SLOTS_PER_SYSTEMFRAME(interval)) & 0x3FF;
                        srsFrame    = (srsFrame & 0xff);
                        srsCtx      =  srs_tti % XRAN_MAX_SECTIONDB_CTX;

                        if(sym_id < pSrsCfg->ndm_txduration)
                        {
                            for(ant_id=sym_id*ndm_step; ant_id < (sym_id+1)*ndm_step; ant_id++)
                                xran_process_tx_srs_cp_off(pHandle, srsCtx, srs_tti, cc_id, ant_id, srsFrame, srsSubframe, srsSlot);
                        }
                        else
                        {
                            p_xran_dev_ctx->ndm_srs_scheduled   = 0;
                            p_xran_dev_ctx->ndm_srs_tti         = 0;
                            p_xran_dev_ctx->ndm_srs_txtti       = 0;
                            p_xran_dev_ctx->ndm_srs_schedperiod = 0;
                        }
                    }
                }
            }
        }
        MLogXRANTask(PID_PREPARE_TX_SYM, t1, MLogXRANTick());
    }

    return retval;
}

#if 0
int32_t antStat[XRAN_PORTS_NUM][XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_CSIRS_PORTS] = { 0 };
int32_t vfStat[XRAN_PORTS_NUM*2] = { 0 };
#endif

static inline uint16_t xran_tx_sym_from_ring(struct xran_device_ctx* p_xran_dev_ctx, struct rte_ring *r, uint16_t vf_id)
{
    struct rte_mbuf *mbufs[XRAN_MAX_MEM_IF_RING_SIZE];
    uint16_t dequeued, sent = 0;
    uint32_t remaining;

    dequeued = rte_ring_dequeue_burst(r, (void **)mbufs, XRAN_MAX_MEM_IF_RING_SIZE,
            &remaining);
    if (!dequeued)
        return 0;   /* Nothing to send. */

#if 0
{
    int i;
    for(i = 0; i < dequeued; i++)
    {
        struct rte_ether_hdr *eth_hdr;
        struct xran_ecpri_hdr *ecpri_hdr;
        struct xran_eaxc_info result;
        uint32_t cc_id, ant_id;

        eth_hdr     = rte_pktmbuf_mtod(mbufs[i], struct rte_ether_hdr *);
        ecpri_hdr   = rte_pktmbuf_mtod_offset(mbufs[i], struct xran_ecpri_hdr *, sizeof(*eth_hdr));
        xran_decompose_cid(p_xran_dev_ctx->xran_port_id, (uint16_t)ecpri_hdr->ecpri_xtc_id, &result);
        cc_id  = result.ccId;
        ant_id = result.ruPortId;

        antStat[p_xran_dev_ctx->xran_port_id][cc_id][ant_id]++;
        vfStat[vf_id]++;

#if 0
        static uint8_t exp_seq_id[XRAN_PORTS_NUM][XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_CSIRS_PORTS] = { 0 };
        uint32_t frame_id, subframe_id, slot_id, sym_id, seq_id;
        static int loopcnt = 0;
        struct radio_app_common_hdr *radio_hdr = (void *)rte_pktmbuf_mtod_offset(mbufs[i], void *, (sizeof(*ecpri_hdr)+sizeof(*eth_hdr)));
        struct radio_app_common_hdr rhdr;

        seq_id = ecpri_hdr->ecpri_seq_id.bits.seq_id;
        rhdr.sf_slot_sym.value = rte_be_to_cpu_16(radio_hdr->sf_slot_sym.value);
        frame_id    = radio_hdr->frame_id;
        subframe_id = rhdr.sf_slot_sym.subframe_id;
        slot_id     = xran_slotid_convert(rhdr.sf_slot_sym.slot_id, 1);
        sym_id      = rhdr.sf_slot_sym.symb_id;
#if 0
        if(p_xran_dev_ctx->xran_port_id == 0 && (ant_id == 0 /*|| ant_id == 15 */))
        {
            printf("[%d] RU%d CC%d ANT%d [%d:%d:%d-%d] packet=%3d exp=%3d\n", i,
                p_xran_dev_ctx->xran_port_id, cc_id, ant_id,
                frame_id, subframe_id, slot_id, sym_id,
                seq_id, exp_seq_id[p_xran_dev_ctx->xran_port_id][cc_id][ant_id]);
        }
#endif
        uint32_t nFrameIdx, nSubframeIdx, nSlotIdx;
        uint64_t nSecond;

        xran_timingsource_get_slotidx(&nFrameIdx, &nSubframeIdx, &nSlotIdx, &nSecond, 1);
        if(seq_id != exp_seq_id[p_xran_dev_ctx->xran_port_id][cc_id][ant_id])
        {
            if(loopcnt > 140000)
            // && loopcnt < 145000)
            {
                if(p_xran_dev_ctx->xran_port_id == 0 && ant_id == 0 /*|| ant_id == 15 */)
                {
                    printf("[%d] RU%d CC%d ANT%02d [%3d:%d:%d-%2d] [%3d:%d:%d] packet=%3d, exp=%3d\n", i,
                            p_xran_dev_ctx->xran_port_id, cc_id, ant_id,
                            frame_id, subframe_id, slot_id, sym_id,
                            nFrameIdx%256, nSubframeIdx, nSlotIdx,
                            seq_id, exp_seq_id[p_xran_dev_ctx->xran_port_id][cc_id][ant_id]);
                }
            }
            exp_seq_id[p_xran_dev_ctx->xran_port_id][cc_id][ant_id] = (seq_id+1);
            loopcnt++;
        }
        else
            exp_seq_id[p_xran_dev_ctx->xran_port_id][cc_id][ant_id]++;
#endif
    }
}
#endif

    while (1)
    {
        sent += rte_eth_tx_burst(vf_id, 0, &mbufs[sent], dequeued - sent);
        if (sent == dequeued)
        {
            return remaining;
        }
    }
}


int32_t
xran_process_tx_sym_cp_on_ring(void* pHandle, uint8_t ctx_id, uint32_t tti, int32_t start_cc, int32_t num_cc, int32_t start_ant,  int32_t num_ant, uint32_t frame_id,
    uint32_t subframe_id, uint32_t slot_id, uint32_t sym_id, enum xran_comp_hdr_type compType, enum xran_pkt_dir direction,
    uint16_t xran_port_id, PSECTION_DB_TYPE p_sec_db, uint8_t mu, uint32_t tti_for_ring, uint32_t sym_id_for_ring, bool isVmu)
{
    long t1 = MLogXRANTick();
    struct rte_ring *ring = NULL;
    struct xran_device_ctx* p_xran_dev_ctx = (struct xran_device_ctx*)pHandle;
    int32_t cc_id  = 0;
    int32_t ant_id = 0;
    uint16_t vf_id = 0;
    int32_t ru_port_id;

/*if(first_call && sym_id_for_ring==0 && start_ant==0)
{
    printf("upXmit ota_tti=%u, tti %u, sym=%u\n", (xran_lib_ota_sym_idx_mu[mu]/(14))%XRAN_N_FE_BUF_LEN,
       tti_for_ring% XRAN_N_FE_BUF_LEN, sym_id_for_ring);
}
*/
    /*Check if it is intra symbol boundary of current symbol and transmit the next sym packet (advance Tx)*/
    // if(unlikely(p_xran_dev_ctx->perMu[mu].adv_tx_factor != 0)){
    //     if(p_xran_dev_ctx->perMu[mu].adv_tx_factor <= xran_intra_sym_div[mu]){
    //         sym_id_for_ring = sym_id_for_ring + 1;
    //         tti_for_ring = (tti_for_ring + sym_id_for_ring/XRAN_NUM_OF_SYMBOL_PER_SLOT) % XRAN_N_FE_BUF_LEN;
    //         sym_id_for_ring = XranGetSymNum(sym_id_for_ring, XRAN_NUM_OF_SYMBOL_PER_SLOT);
    //     }
    //     else
    //         return 0;
    // }

    for (cc_id = start_cc; cc_id < (start_cc + num_cc); cc_id++)
    {
        if(!xran_isactive_cc(p_xran_dev_ctx, cc_id))
            continue;

        for (ant_id = start_ant; ant_id < (start_ant + num_ant); ant_id++)
        {
            ru_port_id = ant_id + p_xran_dev_ctx->perMu[mu].eaxcOffset;
            vf_id = p_xran_dev_ctx->map2vf[direction][cc_id][ru_port_id][XRAN_UP_VF];
            ring    = p_xran_dev_ctx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[tti_for_ring % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id_for_ring].pRing;
            xran_tx_sym_from_ring(p_xran_dev_ctx, ring, vf_id);
        }
    }
    MLogXRANTask(PID_RADIO_ETH_TX_BURST, t1, MLogXRANTick());
    return 0;
}

int32_t
xran_process_tx_sym_cp_on_ring_opt(void* pHandle, int32_t start_cc, int32_t num_cc, int32_t start_ant,  int32_t num_ant,
                                   enum xran_pkt_dir direction, uint8_t mu, uint32_t tti_for_ring, uint8_t sym_id_for_ring)
{
    long t1 = MLogXRANTick();
    struct rte_ring *ring = NULL;
    struct xran_device_ctx* p_xran_dev_ctx = (struct xran_device_ctx*)pHandle;
    int32_t cc_id  = 0;
    int32_t ant_id = 0;
    uint16_t vf_id = 0;
    int32_t ru_port_id;

/*if(first_call && sym_id_for_ring==0 && start_ant==0)
{
    printf("upXmit ota_tti=%u, tti %u, sym=%u\n", (xran_lib_ota_sym_idx_mu[mu]/(14))%XRAN_N_FE_BUF_LEN,
       tti_for_ring% XRAN_N_FE_BUF_LEN, sym_id_for_ring);
}
    static int xx=5000;
    if(0==xx--)
        exit(1);*/

    for (cc_id = start_cc; cc_id < (start_cc + num_cc); cc_id++)
    {
        if(!xran_isactive_cc(p_xran_dev_ctx, cc_id))
            continue;

        for (ant_id = start_ant; ant_id < (start_ant + num_ant); ant_id++)
        {
            ru_port_id = ant_id + p_xran_dev_ctx->perMu[mu].eaxcOffset;
            vf_id = p_xran_dev_ctx->map2vf[direction][cc_id][ru_port_id][XRAN_UP_VF];

            ring    = p_xran_dev_ctx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[tti_for_ring % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id_for_ring].pRing;
            xran_tx_sym_from_ring(p_xran_dev_ctx, ring, vf_id);
        }
    }
    MLogXRANTask(PID_RADIO_ETH_TX_BURST, t1, MLogXRANTick());
    return 0;
}

//#define TRANSMIT_BURST
//#define ENABLE_DEBUG_COREDUMP

#define ETHER_TYPE_ECPRI_BE (0xFEAE)

/* xran_process_tx_sym_cp_on_opt:
pHandle: device context handle
ctx_id: cp-up database index to be used
tti: tti for which to send the data
start_cc: start component carrier
num_cc: number of component carriers to process
start_ant: starting ant id
num_ant: number of antennas to process
frame_id, subframe_id, slot_id: Values to be added in header fields
sym_id: symbol_id to be processed. IQ Buffers/packets corresponding to this sym_id will be packetized and sent out
compType: compression type to be used
direction: DL/UL
xran_port_id: port_id corresponding to pHandle (TODO: redundant. remove it. derive this in the function)
p_sec_db: cp-up section database to be used
mu: numerology to process
tti_for_ring, sym_id_for_ring: Used to determine the tx ring to enqueue the packets. We could process the packet in advance
(e.g. when bbupool is enabled) but we must send it at the right time as governed by transmission window.
{tti, symbol} specific rings are created at initialization. Caller of this function should figure out the exact {tti, sym} to start sending
these packets and provide them here.
*/
int32_t xran_process_tx_sym_cp_on_opt(void* pHandle, uint8_t ctx_id, uint32_t tti, int32_t start_cc, int32_t num_cc, int32_t start_ant,  int32_t num_ant,
    uint32_t frame_id, uint32_t subframe_id, uint32_t slot_id, uint32_t sym_id, enum xran_comp_hdr_type compType, enum xran_pkt_dir direction,
    uint16_t xran_port_id, PSECTION_DB_TYPE p_sec_db, uint8_t mu, uint32_t tti_for_ring, uint32_t sym_id_for_ring , bool isVmu)
{
    struct xran_up_pkt_gen_params *pxp;
    struct data_section_hdr *pDataSec;
    char *ext_buff, *temp_buff;
    void  *mb_base;
    struct rte_ring *ring;
    char* pStart;
    struct xran_ethdi_ctx* eth_ctx = xran_ethdi_get_ctx();
    struct xran_section_info* sectinfo;
    struct xran_device_ctx* p_xran_dev_ctx = (struct xran_device_ctx*)pHandle;
    struct rte_mbuf_ext_shared_info* p_share_data;
    struct xran_sectioninfo_db* ptr_sect_elm = NULL;
    struct rte_mbuf* mb_oran_hdr_ext = NULL;
    struct xran_ecpri_hdr* ecpri_hdr = NULL;
    uint16_t* __restrict pDst = NULL;

    uint16_t next;
    uint16_t ext_buff_len = 0;
    uint16_t iq_sample_size_bytes=0;
    uint16_t num_sections = 0, total_sections = 0;
    uint16_t n_bytes;
    uint64_t elm_bytes = 0;
    uint16_t section_id;
    uint16_t nPktSize=0, iq_offset = 0;
    uint16_t cid;
    uint16_t vf_id;
	uint16_t p_id;
    const int16_t rte_mempool_objhdr_size = sizeof(struct rte_mempool_objhdr);
    uint8_t seq_id = 0;
    uint8_t cc_id, ant_id, ant_index;
    xran_vMu_proc_type_t vMu_proc_type;

#ifdef TRANSMIT_BURST
    uint16_t len = 0;
#endif
    //uint16_t len2 = 0, len_frag = 0;
    uint8_t compMeth;
    uint8_t iqWidth;
#ifdef TRANSMIT_BURST
    struct mbuf_table  loc_tx_mbufs;
    struct mbuf_table  loc_tx_mbufs_fragmented = {0};
#endif
#if 0
    uint8_t fragNeeded=0;
#endif
    uint8_t ssbMu;

    const uint8_t rte_ether_hdr_size = sizeof(struct rte_ether_hdr);
    uint8_t comp_head_upd = 0;

    const uint8_t total_header_size = (RTE_PKTMBUF_HEADROOM +
        sizeof(struct xran_ecpri_hdr) +
        sizeof(struct radio_app_common_hdr) +
        sizeof(struct data_section_hdr));

    if(isVmu){
        if(mu == 3 && p_xran_dev_ctx->vMuInfo.ssbInfo.ssbMu == 4){
            vMu_proc_type = XRAN_VMU_PROC_SSB;
            ssbMu = p_xran_dev_ctx->vMuInfo.ssbInfo.ssbMu;
        }
        else{
            print_err("Unsupported virtual numerology");
            return XRAN_STATUS_UNSUPPORTED;
        }
    }

    for(cc_id = start_cc; cc_id < (start_cc + num_cc); cc_id++)
    {
        if(!xran_isactive_cc(p_xran_dev_ctx, cc_id))
            continue;

        for(ant_id = start_ant; ant_id < (start_ant + num_ant); ant_id++)
        {
            if(!isVmu){
                ant_index = ant_id - p_xran_dev_ctx->perMu[mu].eaxcOffset;
            }
            else{
                if(vMu_proc_type == XRAN_VMU_PROC_SSB){
                    ant_index = ant_id - p_xran_dev_ctx->vMuInfo.ssbInfo.ruPortId_offset;
                }
            }
            ptr_sect_elm = p_sec_db->p_sectiondb_elm[ctx_id][direction][cc_id][ant_id];
            if(unlikely(ptr_sect_elm == NULL))
            {
                rte_panic("ptr_sect_elm == NULL\n");
                return (0);
            }

            if(0!=ptr_sect_elm->cur_index)
            {
                num_sections = ptr_sect_elm->cur_index;
                /* iterate C-Plane configuration to generate corresponding U-Plane */
                // here
                vf_id = p_xran_dev_ctx->map2vf[direction][cc_id][ant_id][XRAN_UP_VF];
                p_id = eth_ctx->io_cfg.port[vf_id];
                ring    = p_xran_dev_ctx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[tti_for_ring % XRAN_N_FE_BUF_LEN][cc_id][ant_index].sBufferList.pBuffers[sym_id_for_ring].pRing;
                if(!isVmu){
                    mb_base = p_xran_dev_ctx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_index].sBufferList.pBuffers[sym_id].pCtrl;
                    temp_buff = ((char*)p_xran_dev_ctx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_index].sBufferList.pBuffers[sym_id].pData);
                }
                else{
                    if(vMu_proc_type == XRAN_VMU_PROC_SSB){
                        mb_base = p_xran_dev_ctx->vMuInfo.ssbInfo.sFHSsbTxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_index].sBufferList.pBuffers[sym_id].pCtrl;
                        temp_buff = (char *)p_xran_dev_ctx->vMuInfo.ssbInfo.sFHSsbTxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_index].sBufferList.pBuffers[sym_id].pData;
                    }
                }

                if (unlikely(mb_base == NULL))
                {
                    rte_panic("mb == NULL\n");
                }
                
                cid = ((cc_id << p_xran_dev_ctx->eAxc_id_cfg.bit_ccId) & p_xran_dev_ctx->eAxc_id_cfg.mask_ccId) | ((ant_id << p_xran_dev_ctx->eAxc_id_cfg.bit_ruPortId) & p_xran_dev_ctx->eAxc_id_cfg.mask_ruPortId);
                cid = rte_cpu_to_be_16(cid);

#ifdef TRANSMIT_BURST
                loc_tx_mbufs.len = 0;
#endif

#pragma loop_count min=1, max=16 //XRAN_MAX_SECTIONS_PER_SYM
                for (next=0; next< num_sections; next++)
                {
                    sectinfo = &ptr_sect_elm->list[next];

                    if (unlikely(sectinfo == NULL)) {
                        print_err("sectinfo == NULL\n");
                        break;
                    }
                    if (unlikely(sectinfo->type != XRAN_CP_SECTIONTYPE_1 && sectinfo->type != XRAN_CP_SECTIONTYPE_3))
                    {   /* only supports type 1, 3 */
                        print_err("Invalid section type in section DB - %d", sectinfo->type);
                        continue;
                    }
                    /* skip, if not scheduled */
                    if (unlikely(sym_id < sectinfo->startSymId || sym_id >= sectinfo->startSymId + sectinfo->numSymbol))
                    {
                        continue;
                    }

                    compMeth = sectinfo->compMeth;
                    iqWidth = sectinfo->iqWidth;
                    section_id = sectinfo->id;

                    iqWidth = (iqWidth == 0) ? 16 : iqWidth;

                    comp_head_upd = ((compMeth != XRAN_COMPMETHOD_NONE) && (compType == XRAN_COMP_HDR_TYPE_DYNAMIC));

                    if(sectinfo->prbElemBegin || p_xran_dev_ctx->RunSlotPrbMapBySymbolEnable)
                    {
                        if(xran_get_syscfg_appmode() == O_DU)
                        {
                            seq_id = xran_get_updl_seqid(xran_port_id, cc_id, ant_id);
                        }
                        else
                        {
                            seq_id = xran_get_upul_seqid(xran_port_id, cc_id, ant_id);
                        }
                        iq_sample_size_bytes = 18 +   sizeof(struct xran_ecpri_hdr) +
                                sizeof(struct radio_app_common_hdr);
                    }

                    iq_sample_size_bytes += sizeof(struct data_section_hdr) ;

                    if(comp_head_upd)
                        iq_sample_size_bytes += sizeof(struct data_section_compression_hdr);

                    iq_sample_size_bytes += xran_get_iqdata_len(sectinfo->numPrbc, iqWidth, compMeth);

#ifdef TRANSMIT_BURST
                    len = loc_tx_mbufs.len;
                    //Added for Klocworks
                    if (unlikely(len >= MBUF_TABLE_SIZE))
                    {
                        len = MBUF_TABLE_SIZE - 1;
                        rte_panic("len >= MBUF_TABLE_SIZE\n");
                    }
#endif
                    if(sectinfo->prbElemBegin || p_xran_dev_ctx->RunSlotPrbMapBySymbolEnable)
                    {
                        p_share_data = &p_xran_dev_ctx->share_data.sh_data[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id][section_id];
                        p_share_data->free_cb = extbuf_free_callback;
                        p_share_data->fcb_opaque = NULL;
                        rte_mbuf_ext_refcnt_set(p_share_data, 1);
                        ext_buff = (char*) (xran_add_hdr_offset((uint8_t*)temp_buff, compMeth));

                        /* Create ethernet + eCPRI + radio app header */
                        iq_offset = xran_get_iqdata_len(sectinfo->numPrbc, iqWidth, compMeth);
                        ext_buff_len = iq_offset;
                        temp_buff = ext_buff + iq_offset;

                        ext_buff -= total_header_size;

                        ext_buff_len += (total_header_size + 18);

                        if (comp_head_upd)
                        {
                            ext_buff -= sizeof(struct data_section_compression_hdr);
                            ext_buff_len += sizeof(struct data_section_compression_hdr);
                        }

                        mb_oran_hdr_ext = rte_pktmbuf_alloc(_eth_mbuf_pool_vf_small[p_id]);
                        if (unlikely((mb_oran_hdr_ext) == NULL))
                        {
                            rte_panic("[core %d]Failed rte_pktmbuf_alloc on vf %d\n", rte_lcore_id(), p_id);
                        }

#ifdef ENABLE_DEBUG_COREDUMP
                        if (unlikely((struct rte_mempool_objhdr*)RTE_PTR_SUB(mb_base, rte_mempool_objhdr_size)->iova == 0))
                        {
                            rte_panic("Failed rte_mem_virt2iova\n");
                        }
                        if (unlikely(((rte_iova_t)(struct rte_mempool_objhdr*)RTE_PTR_SUB(mb_base, rte_mempool_objhdr_size)->iova) == RTE_BAD_IOVA))
                        {
                            rte_panic("Failed rte_mem_virt2iova RTE_BAD_IOVA \n");
                        }
#endif

                        mb_oran_hdr_ext->buf_addr = ext_buff;   //here the address points to the start of  the packet
                        mb_oran_hdr_ext->buf_iova = ((struct rte_mempool_objhdr*)RTE_PTR_SUB(mb_base, rte_mempool_objhdr_size))->iova + RTE_PTR_DIFF(ext_buff, mb_base);
                        mb_oran_hdr_ext->buf_len = ext_buff_len;
                        mb_oran_hdr_ext->ol_flags |= RTE_MBUF_F_EXTERNAL;
                        mb_oran_hdr_ext->shinfo = p_share_data;
                        mb_oran_hdr_ext->data_off = (uint16_t)RTE_MIN((uint16_t)RTE_PKTMBUF_HEADROOM, (uint16_t)mb_oran_hdr_ext->buf_len) - rte_ether_hdr_size;
                        mb_oran_hdr_ext->data_len = (uint16_t)(mb_oran_hdr_ext->data_len + rte_ether_hdr_size);
                        mb_oran_hdr_ext->pkt_len = mb_oran_hdr_ext->pkt_len + rte_ether_hdr_size;
                        mb_oran_hdr_ext->port = eth_ctx->io_cfg.port[vf_id];

                        /* free previously used mbuf pointed by to_free_mbuf */
                        if (p_xran_dev_ctx->perMu[mu].to_free_mbuf[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id][sym_id][section_id])
                        {
                            rte_pktmbuf_free(p_xran_dev_ctx->perMu[mu].to_free_mbuf[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id][sym_id][section_id]);
                        }

                        /* to_free_mbuf now points to newly allocated mbuf */
                        p_xran_dev_ctx->perMu[mu].to_free_mbuf[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id][sym_id][section_id] = (void*)mb_oran_hdr_ext;

                        /* make sure eth won't free our mbuf being used */
                        rte_pktmbuf_refcnt_update(mb_oran_hdr_ext, 1);

                        pStart = (char*)((char*)mb_oran_hdr_ext->buf_addr + mb_oran_hdr_ext->data_off);

                        /* Fill in the ethernet header. */
#ifndef TRANSMIT_BURST
#if (RTE_VER_YEAR >= 21)
                        rte_eth_macaddr_get(mb_oran_hdr_ext->port, &((struct rte_ether_hdr*)pStart)->src_addr);         /* set source addr */
                        ((struct rte_ether_hdr*)pStart)->dst_addr = eth_ctx->entities[vf_id][ID_O_RU];                  /* set dst addr */
#else
                        rte_eth_macaddr_get(mb_oran_hdr_ext->port, &((struct rte_ether_hdr*)pStart)->s_addr);         /* set source addr */
                        ((struct rte_ether_hdr*)pStart)->d_addr = eth_ctx->entities[vf_id][ID_O_RU];                  /* set dst addr */
#ifdef DEBUG
                        struct rte_ether_hdr *h = (struct rte_ether_hdr *)rte_pktmbuf_mtod(mb_oran_hdr_ext, struct rte_ether_hdr*);
                        struct rte_ether_addr *s = &h->s_addr;
                        struct rte_ether_addr *dst = &((struct rte_ether_hdr*)pStart)->d_addr;
                        printf("src=%x:%x:%x:%x:%x:%x, dst=%x:%x:%x:%x:%x:%x\n",
                                    s->addr_bytes[0], s->addr_bytes[1], s->addr_bytes[2],
                                    s->addr_bytes[3], s->addr_bytes[4], s->addr_bytes[5],
                                    dst->addr_bytes[0], dst->addr_bytes[1], dst->addr_bytes[2],
                                    dst->addr_bytes[3], dst->addr_bytes[4], dst->addr_bytes[5]);
#endif
#endif
                        ((struct rte_ether_hdr*)pStart)->ether_type = ETHER_TYPE_ECPRI_BE;                            /* ethertype */
#endif
                        nPktSize = sizeof(struct rte_ether_hdr)
                                                + sizeof(struct xran_ecpri_hdr)
                                                + sizeof(struct radio_app_common_hdr) ;

                        ecpri_hdr = (struct xran_ecpri_hdr*)(pStart + sizeof(struct rte_ether_hdr));

                        ecpri_hdr->cmnhdr.data.data_num_1 = 0x0;
                        ecpri_hdr->cmnhdr.bits.ecpri_ver = XRAN_ECPRI_VER;
                        ecpri_hdr->cmnhdr.bits.ecpri_mesg_type = ECPRI_IQ_DATA;

                        /* one to one lls-CU to RU only and band sector is the same */
                        ecpri_hdr->ecpri_xtc_id = cid;

                        /* no transport layer fragmentation supported */
                        ecpri_hdr->ecpri_seq_id.data.data_num_1 = 0x8000;
                        ecpri_hdr->ecpri_seq_id.bits.seq_id = seq_id;
                        ecpri_hdr->cmnhdr.bits.ecpri_payl_size =  sizeof(struct radio_app_common_hdr) + XRAN_ECPRI_HDR_SZ; //xran_get_ecpri_hdr_size();;;

                    } /* if(sectinfo->prbElemBegin) */

                    /* Prepare U-Plane section hdr */
                    n_bytes = xran_get_iqdata_len(sectinfo->numPrbc, iqWidth, compMeth);
                    n_bytes = RTE_MIN(n_bytes, XRAN_MAX_MBUF_LEN);

                    /* Ethernet & eCPRI added already */
                    nPktSize += sizeof(struct data_section_hdr) + n_bytes;

                    if (comp_head_upd)
                        nPktSize += sizeof(struct data_section_compression_hdr);

                    if(likely((ecpri_hdr!=NULL)))
                    {
                        ecpri_hdr->cmnhdr.bits.ecpri_payl_size += sizeof(struct data_section_hdr) + n_bytes ;

                        if (comp_head_upd)
                            ecpri_hdr->cmnhdr.bits.ecpri_payl_size += sizeof(struct data_section_compression_hdr);
                    }
                    else
                    {
                        print_err("ecpri_hdr should not be NULL\n");
                    }
                    //ecpri_hdr->cmnhdr.bits.ecpri_payl_size += ecpri_payl_size;

                    /* compression */

                    if(sectinfo->prbElemBegin || p_xran_dev_ctx->RunSlotPrbMapBySymbolEnable)
                    {
                        pDst = (uint16_t*)(pStart + sizeof(struct rte_ether_hdr) + sizeof(struct xran_ecpri_hdr));
                        pxp = (struct xran_up_pkt_gen_params *)pDst;
                        /* radio app header */
                        pxp->app_params.data_feature.value = 0x10;
                        pxp->app_params.data_feature.data_direction = direction;
                        pxp->app_params.frame_id = frame_id;
                        pxp->app_params.sf_slot_sym.subframe_id = subframe_id;
                        pxp->app_params.sf_slot_sym.slot_id = slot_id;
                        pxp->app_params.sf_slot_sym.symb_id = sym_id;
                        /* convert to network byte order */
                        pxp->app_params.sf_slot_sym.value = rte_cpu_to_be_16(pxp->app_params.sf_slot_sym.value);
                        pDst += 2;
                    }

                    pDataSec = (struct data_section_hdr *)pDst;
                    if(pDataSec){
                        if(!isVmu)
                            pDataSec->fields.sect_id = XRAN_BASE_SECT_ID_TO_MU_SECT_ID(section_id,mu);
                        else{
                            if(vMu_proc_type == XRAN_VMU_PROC_SSB)
                                pDataSec->fields.sect_id = XRAN_BASE_SECT_ID_TO_MU_SECT_ID(section_id,ssbMu);
                        }

                        pDataSec->fields.num_prbu = (uint8_t)XRAN_CONVERT_NUMPRBC(sectinfo->numPrbc);
                        pDataSec->fields.start_prbu = (sectinfo->startPrbc & 0x03ff);
                        pDataSec->fields.sym_inc = 0;
                        pDataSec->fields.rb = 0;
                        /* network byte order */
                        pDataSec->fields.all_bits = rte_cpu_to_be_32(pDataSec->fields.all_bits);
                        pDst += 2;
                    }
                    else
                    {
                        print_err("pDataSec is NULL idx = %u num_sections = %u\n", next, num_sections);
                        // return 0;
                    }

                    if (comp_head_upd)
                    {
                        if(pDst == NULL){
                            print_err("pDst == NULL\n");
                            return 0;
                        }
                        ((struct data_section_compression_hdr *)pDst)->ud_comp_hdr.ud_comp_meth = compMeth;
                        ((struct data_section_compression_hdr *)pDst)->ud_comp_hdr.ud_iq_width = XRAN_CONVERT_IQWIDTH(iqWidth);
                        ((struct data_section_compression_hdr *)pDst)->rsrvd = 0;
                        pDst++;
                    }

                    //Increment by IQ data len
                    pDst = (uint16_t *)((uint8_t *)pDst + n_bytes) ;
                    if(mb_oran_hdr_ext){
                        rte_pktmbuf_pkt_len(mb_oran_hdr_ext) = nPktSize;
                        rte_pktmbuf_data_len(mb_oran_hdr_ext) = nPktSize;
                    }

                    if(sectinfo->prbElemEnd || p_xran_dev_ctx->RunSlotPrbMapBySymbolEnable) /* Transmit the packet */
                    {
                        if(likely((ecpri_hdr!=NULL)))
                            ecpri_hdr->cmnhdr.bits.ecpri_payl_size = rte_cpu_to_be_16(ecpri_hdr->cmnhdr.bits.ecpri_payl_size);
                        else
                            print_err("ecpri_hdr should not be NULL\n");
                        /* if we don't need to do any fragmentation */
                        if (likely(p_xran_dev_ctx->mtu >= (iq_sample_size_bytes)))
                        {
                            /* no fragmentation */
                            //len2 = 1;
#ifdef TRANSMIT_BURST
                            loc_tx_mbufs.m_table[len++] = (void*)mb_oran_hdr_ext;
                            if (unlikely(len > XRAN_MAX_PKT_BURST_PER_SYM))
                            {
                                rte_panic("XRAN_MAX_PKT_BURST_PER_SYM\n");
                            }
                            loc_tx_mbufs.len = len;
#else

                            if(xran_get_syscfg_bbuoffload())
                            {
#ifdef DEBUG
                                if(true == xran_check_if_late_transmission(tti_for_ring % XRAN_N_FE_BUF_LEN, sym_id_for_ring, mu))
                                {
                                    print_err("\nxran_port_id=%u, uplane too late:ttiSymInPkt={%u, %u, %u}, ttiSymToSend={%u,%u} \n\n",
                                        xran_port_id, tti, tti% XRAN_N_FE_BUF_LEN, sym_id, tti_for_ring% XRAN_N_FE_BUF_LEN, sym_id_for_ring);
                                    rte_panic("\n");
                                }
#endif
                                if(likely(ring)) {
                                    if(rte_ring_enqueue(ring, mb_oran_hdr_ext)){
                                        rte_panic("Ring enqueue failed. Ring free count [%d].p_xran_dev_ctx->port_id = %d\n",rte_ring_free_count(ring), p_xran_dev_ctx->xran_port_id);
                                        rte_pktmbuf_free(mb_oran_hdr_ext);
                                    }
                                } else
                                    rte_panic("Ring is empty.\n");
                            } else {
                                xran_enqueue_mbuf(mb_oran_hdr_ext, eth_ctx->tx_ring[vf_id]);
                            }
#endif
                        }
                        else
                        {
                            /* current code should not go to fragmentation as it should be taken care of by section allocation already */
                            // print_err("should not go into fragmentation mtu %d packet size %d\n", p_xran_dev_ctx->mtu, sectinfo->numPrbc * (3*iq_sample_size_bits + 1));
                            return 0;
                        }
                        elm_bytes += nPktSize;
                    } /* if(prbElemEnd) */
                }/* section loop */
            } /* if ptr_sect_elm->cur_index */

            total_sections += num_sections;

            /* Transmit packets */
#ifdef TRANSMIT_BURST
            if (loc_tx_mbufs.len)
            {
                for (int32_t i = 0; i < loc_tx_mbufs.len; i++)
                {
                    if(xran_get_syscfg_bbuoffload())
                        rte_ring_enqueue(ring, loc_tx_mbufs_fragmented.m_table[i]);
                    else
                        p_xran_dev_ctx->send_upmbuf2ring(loc_tx_mbufs.m_table[i], ETHER_TYPE_ECPRI, vf_id);
                }
                loc_tx_mbufs.len = 0;
            }
#endif
#if 0   /* There is no logic populating loc_tx_mbufs_fragmented. hence disabling this code */
            /* Transmit fragmented packets */
            if (unlikely(fragNeeded))
            {
                for (int32_t i = 0; i < loc_tx_mbufs_fragmented.len; i++)
                {
                    if(xran_get_syscfg_bbuoffload())
                        rte_ring_enqueue(ring, loc_tx_mbufs_fragmented.m_table[i]);
                    else
                        p_xran_dev_ctx->send_upmbuf2ring(loc_tx_mbufs_fragmented.m_table[i], ETHER_TYPE_ECPRI, vf_id);
                }
            }
#endif
        } /* for(cc_id = 0; cc_id < num_CCPorts; cc_id++) */
    } /* for(ant_id = 0; ant_id < num_eAxc; ant_id++) */

    struct xran_common_counters* pCnt = &p_xran_dev_ctx->fh_counters;
    pCnt->tx_counter += total_sections;
    pCnt->tx_bytes_counter += elm_bytes;

    return 1;
}

int32_t
xran_process_tx_srs_cp_on(void* pHandle, uint8_t ctx_id, uint32_t tti, int32_t start_cc, int32_t num_cc, int32_t start_ant,  int32_t num_ant, uint32_t frame_id,
    uint32_t subframe_id, uint32_t slot_id, uint32_t sym_id, enum xran_comp_hdr_type compType, enum xran_pkt_dir direction,
    uint16_t xran_port_id, PSECTION_DB_TYPE p_sec_db, uint8_t mu)
{
    struct xran_up_pkt_gen_params *pxp;
    struct data_section_hdr *pDataSec;
    int32_t antElm_eAxC_id = 0;//  = ant_id + p_srs_cfg->srsEaxcOffset;

    struct xran_srs_config *p_srs_cfg;

    char* ext_buff;
    void  *mb_base;
    char* pStart;
    struct xran_ethdi_ctx* eth_ctx = xran_ethdi_get_ctx();
    struct xran_section_info* sectinfo;
    struct xran_device_ctx* p_xran_dev_ctx = (struct xran_device_ctx*)pHandle;
    p_srs_cfg       = &(p_xran_dev_ctx->srs_cfg);
    struct rte_mbuf_ext_shared_info* p_share_data;
    struct xran_sectioninfo_db* ptr_sect_elm = NULL;
    struct rte_mbuf* mb_oran_hdr_ext = NULL;
    struct xran_ecpri_hdr* ecpri_hdr = NULL;
    uint16_t* __restrict pDst = NULL;

    uint16_t next;
    uint16_t ext_buff_len = 0;
    uint16_t iq_sample_size_bytes=0;
    uint16_t num_sections = 0, total_sections = 0;
    uint16_t n_bytes;
    uint16_t elm_bytes = 0;
    uint16_t section_id;
    uint16_t nPktSize=0;
    uint16_t cid;
    uint16_t vf_id;
    const int16_t rte_mempool_objhdr_size = sizeof(struct rte_mempool_objhdr);
    uint8_t seq_id = 0;
    uint8_t cc_id, ant_id;
    uint8_t compMeth;
    uint8_t iqWidth;

    const uint8_t rte_ether_hdr_size = sizeof(struct rte_ether_hdr);
    uint8_t comp_head_upd = 0;

    const uint8_t total_header_size = (RTE_PKTMBUF_HEADROOM +
        sizeof(struct xran_ecpri_hdr) +
        sizeof(struct radio_app_common_hdr) +
        sizeof(struct data_section_hdr));

    for (cc_id = start_cc; cc_id < (start_cc + num_cc); cc_id++)
    {
        if(!xran_isactive_cc(p_xran_dev_ctx, cc_id))
            continue;
        for (ant_id = start_ant; ant_id < (start_ant + num_ant); ant_id++)
        {
            antElm_eAxC_id  = ant_id + p_srs_cfg->srsEaxcOffset;
            ptr_sect_elm = p_sec_db->p_sectiondb_elm[ctx_id][direction][cc_id][antElm_eAxC_id];

            if (unlikely(ptr_sect_elm == NULL)){
                printf("ant_id = %d ctx_id = %d,start_ant = %d, num_ant = %d, antElm_eAxC_id = %d\n",ant_id,ctx_id,start_ant,num_ant,antElm_eAxC_id);
                rte_panic("ptr_sect_elm == NULL\n");
                return (0);
            }
            if(0!=ptr_sect_elm->cur_index)
            {
                num_sections = ptr_sect_elm->cur_index;
                /* iterate C-Plane configuration to generate corresponding U-Plane */
                vf_id = xran_map_ecpriPcid_to_vf(p_xran_dev_ctx, direction, cc_id, antElm_eAxC_id);//p_xran_dev_ctx->map2vf[direction][cc_id][antElm_eAxC_id][XRAN_UP_VF];
                mb_base = p_xran_dev_ctx->perMu[mu].sFHSrsRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id].pCtrl;
                if (unlikely(mb_base == NULL))
                {
                    rte_panic("mb == NULL\n");
                }
                cid = ((cc_id << p_xran_dev_ctx->eAxc_id_cfg.bit_ccId) & p_xran_dev_ctx->eAxc_id_cfg.mask_ccId) | ((antElm_eAxC_id << p_xran_dev_ctx->eAxc_id_cfg.bit_ruPortId) & p_xran_dev_ctx->eAxc_id_cfg.mask_ruPortId);
                cid = rte_cpu_to_be_16(cid);
#pragma loop_count min=1, max=16
                for (next=0; next< num_sections; next++)
                {
                    sectinfo = &ptr_sect_elm->list[next];

                    if (unlikely(sectinfo == NULL)) {
                        print_err("sectinfo == NULL\n");
                        break;
                    }
                    if (unlikely(sectinfo->type != XRAN_CP_SECTIONTYPE_1 && sectinfo->type != XRAN_CP_SECTIONTYPE_3))
                    {   /* only supports type 1 or type 3*/
                        print_err("Invalid section type in section DB - %d", sectinfo->type);
                        continue;
                    }
                    /* skip, if not scheduled */
                    if (unlikely(sym_id < sectinfo->startSymId || sym_id >= sectinfo->startSymId + sectinfo->numSymbol))
                        continue;
                    compMeth = sectinfo->compMeth;
                    iqWidth = sectinfo->iqWidth;
                    section_id = sectinfo->id;
                    iqWidth = (iqWidth == 0) ? 16 : iqWidth;

                    comp_head_upd = ((compMeth != XRAN_COMPMETHOD_NONE) && (compType == XRAN_COMP_HDR_TYPE_DYNAMIC));

                    if(sectinfo->prbElemBegin)
                    {
                        seq_id = xran_get_upul_seqid(p_xran_dev_ctx->xran_port_id, cc_id, antElm_eAxC_id);
                        iq_sample_size_bytes = 18 +   sizeof(struct xran_ecpri_hdr) +
                                sizeof(struct radio_app_common_hdr);
                    }

                    iq_sample_size_bytes += sizeof(struct data_section_hdr) ;

                    if(comp_head_upd)
                        iq_sample_size_bytes += sizeof(struct data_section_compression_hdr);

                    iq_sample_size_bytes += xran_get_iqdata_len(sectinfo->numPrbc, iqWidth, compMeth);

                    print_dbg(">>> sym %2d [%d] type%d id %d startPrbc=%d numPrbc=%d startSymId=%d numSymbol=%d\n", sym_id, next,
                            sectinfo->type, sectinfo->id, sectinfo->startPrbc,
                            sectinfo->numPrbc, sectinfo->startSymId, sectinfo->numSymbol);

                    if(sectinfo->prbElemBegin)
                    {
                        p_share_data    = &p_xran_dev_ctx->srs_share_data.sh_data[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id];
                        p_share_data->free_cb = extbuf_free_callback;
                        p_share_data->fcb_opaque = NULL;
                        rte_mbuf_ext_refcnt_set(p_share_data, 1);

                        /* Create ethernet + eCPRI + radio app header */
                        ext_buff_len = sectinfo->sec_desc[sym_id].iq_buffer_len;

                        ext_buff = ((char*)p_xran_dev_ctx->perMu[mu].sFHSrsRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id].pData + sectinfo->sec_desc[sym_id].iq_buffer_offset) - total_header_size;
                        ext_buff_len += (total_header_size + 18);

                        if (comp_head_upd)
                        {
                            ext_buff -= sizeof(struct data_section_compression_hdr);
                            ext_buff_len += sizeof(struct data_section_compression_hdr);
                        }

                        mb_oran_hdr_ext = xran_ethdi_mbuf_indir_alloc();
                        if (unlikely((mb_oran_hdr_ext) == NULL))
                        {
                            rte_panic("[core %d]Failed rte_pktmbuf_alloc on vf %d\n", rte_lcore_id(), vf_id);
                        }

#ifdef ENABLE_DEBUG_COREDUMP
                        if (unlikely((struct rte_mempool_objhdr*)RTE_PTR_SUB(mb_base, rte_mempool_objhdr_size)->iova == 0))
                        {
                            rte_panic("Failed rte_mem_virt2iova\n");
                        }
                        if (unlikely(((rte_iova_t)(struct rte_mempool_objhdr*)RTE_PTR_SUB(mb_base, rte_mempool_objhdr_size)->iova) == RTE_BAD_IOVA))
                        {
                            rte_panic("Failed rte_mem_virt2iova RTE_BAD_IOVA \n");
                        }
#endif
                        mb_oran_hdr_ext->buf_addr = ext_buff;
                        mb_oran_hdr_ext->buf_iova = ((struct rte_mempool_objhdr*)RTE_PTR_SUB(mb_base, rte_mempool_objhdr_size))->iova + RTE_PTR_DIFF(ext_buff, mb_base);
                        mb_oran_hdr_ext->buf_len = ext_buff_len;
                        mb_oran_hdr_ext->ol_flags |= RTE_MBUF_F_EXTERNAL;
                        mb_oran_hdr_ext->shinfo = p_share_data;
                        mb_oran_hdr_ext->data_off = (uint16_t)RTE_MIN((uint16_t)RTE_PKTMBUF_HEADROOM, (uint16_t)mb_oran_hdr_ext->buf_len) - rte_ether_hdr_size;
                        mb_oran_hdr_ext->data_len = (uint16_t)(mb_oran_hdr_ext->data_len + rte_ether_hdr_size);
                        mb_oran_hdr_ext->pkt_len = mb_oran_hdr_ext->pkt_len + rte_ether_hdr_size;
                        mb_oran_hdr_ext->port = eth_ctx->io_cfg.port[vf_id];
                        pStart = (char*)((char*)mb_oran_hdr_ext->buf_addr + mb_oran_hdr_ext->data_off);

                        /* Fill in the ethernet header. */
#if (RTE_VER_YEAR >= 21)
                        rte_eth_macaddr_get(mb_oran_hdr_ext->port, &((struct rte_ether_hdr*)pStart)->src_addr);         /* set source addr */
                        ((struct rte_ether_hdr*)pStart)->dst_addr = eth_ctx->entities[vf_id][ID_O_RU];                  /* set dst addr */
#else
                        rte_eth_macaddr_get(mb_oran_hdr_ext->port, &((struct rte_ether_hdr*)pStart)->s_addr);         /* set source addr */
                        ((struct rte_ether_hdr*)pStart)->d_addr = eth_ctx->entities[vf_id][ID_O_RU];                  /* set dst addr */
#endif
                        ((struct rte_ether_hdr*)pStart)->ether_type = ETHER_TYPE_ECPRI_BE;                            /* ethertype */

                        nPktSize = sizeof(struct rte_ether_hdr)
                                                + sizeof(struct xran_ecpri_hdr)
                                                + sizeof(struct radio_app_common_hdr) ;

                        ecpri_hdr = (struct xran_ecpri_hdr*)(pStart + sizeof(struct rte_ether_hdr));

                        ecpri_hdr->cmnhdr.data.data_num_1 = 0x0;
                        ecpri_hdr->cmnhdr.bits.ecpri_ver = XRAN_ECPRI_VER;
                        ecpri_hdr->cmnhdr.bits.ecpri_mesg_type = ECPRI_IQ_DATA;

                        /* one to one lls-CU to RU only and band sector is the same */
                        ecpri_hdr->ecpri_xtc_id = cid;

                        /* no transport layer fragmentation supported */
                        ecpri_hdr->ecpri_seq_id.data.data_num_1 = 0x8000;
                        ecpri_hdr->ecpri_seq_id.bits.seq_id = seq_id;
                        ecpri_hdr->cmnhdr.bits.ecpri_payl_size =  sizeof(struct radio_app_common_hdr) + XRAN_ECPRI_HDR_SZ; //xran_get_ecpri_hdr_size();;;

                    } /* if(sectinfo->prbElemBegin) */

                    /* Prepare U-Plane section hdr */
                    n_bytes = xran_get_iqdata_len(sectinfo->numPrbc, iqWidth, compMeth);
                    n_bytes = RTE_MIN(n_bytes, XRAN_MAX_MBUF_LEN);

                    /* Ethernet & eCPRI added already */
                    nPktSize += sizeof(struct data_section_hdr) + n_bytes;

                    if (comp_head_upd)
                        nPktSize += sizeof(struct data_section_compression_hdr);

                    if(likely((ecpri_hdr!=NULL)))
                    {
                        ecpri_hdr->cmnhdr.bits.ecpri_payl_size += sizeof(struct data_section_hdr) + n_bytes ;

                        if (comp_head_upd)
                            ecpri_hdr->cmnhdr.bits.ecpri_payl_size += sizeof(struct data_section_compression_hdr);
                    }
                    else
                    {
                        print_err("ecpri_hdr should not be NULL\n");
                    }

                    if(sectinfo->prbElemBegin)
                    {
                        pDst = (uint16_t*)(pStart + sizeof(struct rte_ether_hdr) + sizeof(struct xran_ecpri_hdr));
                        pxp = (struct xran_up_pkt_gen_params *)pDst;
                        /* radio app header */
                        pxp->app_params.data_feature.value = 0x10;
                        pxp->app_params.data_feature.data_direction = direction;
                        pxp->app_params.frame_id = frame_id;
                        pxp->app_params.sf_slot_sym.subframe_id = subframe_id;
                        pxp->app_params.sf_slot_sym.slot_id = slot_id;
                        pxp->app_params.sf_slot_sym.symb_id = sym_id;
                        /* convert to network byte order */
                        pxp->app_params.sf_slot_sym.value = rte_cpu_to_be_16(pxp->app_params.sf_slot_sym.value);
                        pDst += 2;
                    }

                    pDataSec = (struct data_section_hdr *)pDst;
                    if(pDataSec){
                        pDataSec->fields.sect_id = XRAN_BASE_SECT_ID_TO_MU_SECT_ID(section_id,mu);
                        pDataSec->fields.num_prbu = (uint8_t)XRAN_CONVERT_NUMPRBC(sectinfo->numPrbc);
                        pDataSec->fields.start_prbu = (sectinfo->startPrbc & 0x03ff);
                        pDataSec->fields.sym_inc = 0;
                        pDataSec->fields.rb = 0;
                        /* network byte order */
                        pDataSec->fields.all_bits = rte_cpu_to_be_32(pDataSec->fields.all_bits);
                        pDst += 2;
                    }
                    else
                    {
                        print_err("pDataSec is NULL idx = %u num_sections = %u\n", next, num_sections);
                        // return 0;
                    }

                    if (comp_head_upd)
                    {
                        if(pDst == NULL){
                            print_err("pDst == NULL\n");
                            return 0;
                        }
                        ((struct data_section_compression_hdr *)pDst)->ud_comp_hdr.ud_comp_meth = compMeth;
                        ((struct data_section_compression_hdr *)pDst)->ud_comp_hdr.ud_iq_width = XRAN_CONVERT_IQWIDTH(iqWidth);
                        ((struct data_section_compression_hdr *)pDst)->rsrvd = 0;
                        pDst++;
                    }

                    //Increment by IQ data len
                    pDst = (uint16_t *)((uint8_t *)pDst + n_bytes) ;
                    if(mb_oran_hdr_ext){
                        rte_pktmbuf_pkt_len(mb_oran_hdr_ext) = nPktSize;
                        rte_pktmbuf_data_len(mb_oran_hdr_ext) = nPktSize;
                    }

                    if(sectinfo->prbElemEnd) /* Transmit the packet */
                    {
                        if(likely((ecpri_hdr!=NULL)))
                            ecpri_hdr->cmnhdr.bits.ecpri_payl_size = rte_cpu_to_be_16(ecpri_hdr->cmnhdr.bits.ecpri_payl_size);
                        else
                            print_err("ecpri_hdr should not be NULL\n");
                        /* if we don't need to do any fragmentation */
                        if (likely(p_xran_dev_ctx->mtu >= (iq_sample_size_bytes)))
                        {
                            p_xran_dev_ctx->send_upmbuf2ring(mb_oran_hdr_ext, ETHER_TYPE_ECPRI, vf_id);
                        }
                        else
                        {
                            return 0;
                        }
                        elm_bytes += nPktSize;
                    } /* if(prbElemEnd) */
                }/* section loop */
            } /* if ptr_sect_elm->cur_index */
            total_sections += num_sections;
        } /* for(cc_id = 0; cc_id < num_CCPorts; cc_id++) */
    } /* for(ant_id = 0; ant_id < num_eAxc; ant_id++) */

    struct xran_common_counters* pCnt = &p_xran_dev_ctx->fh_counters;
    pCnt->tx_counter += total_sections;
    pCnt->tx_bytes_counter += elm_bytes;
    return 1;
}

int32_t
xran_process_tx_csirs_cp_on(void* pHandle, uint8_t ctx_id, uint32_t tti, int32_t start_cc, int32_t num_cc, int32_t start_ant,  int32_t num_ant, uint32_t frame_id,
    uint32_t subframe_id, uint32_t slot_id, uint32_t sym_id, enum xran_comp_hdr_type compType, enum xran_pkt_dir direction,
    uint16_t xran_port_id, PSECTION_DB_TYPE p_sec_db, uint8_t mu, uint32_t tti_for_ring, uint32_t sym_id_for_ring)
{
    struct xran_up_pkt_gen_params *pxp;
    struct data_section_hdr *pDataSec;
    int32_t antElm_eAxC_id = 0;

    struct xran_csirs_config *p_csirs_cfg;
    struct rte_ring *ring;
    char *ext_buff, *temp_buff;
    void  *mb_base;
    char* pStart;
    struct xran_ethdi_ctx* eth_ctx = xran_ethdi_get_ctx();
    struct xran_section_info* sectinfo;
    struct xran_device_ctx* p_xran_dev_ctx = (struct xran_device_ctx*)pHandle;
    p_csirs_cfg       = &(p_xran_dev_ctx->csirs_cfg);
    struct rte_mbuf_ext_shared_info* p_share_data;
    struct xran_sectioninfo_db* ptr_sect_elm = NULL;
    struct rte_mbuf* mb_oran_hdr_ext = NULL;
    struct xran_ecpri_hdr* ecpri_hdr = NULL;
    uint16_t* __restrict pDst = NULL;

    uint16_t next;
    uint16_t ext_buff_len = 0;
    uint16_t iq_sample_size_bytes=0;
    uint16_t num_sections = 0, total_sections = 0;
    uint16_t n_bytes;
    uint16_t elm_bytes = 0;
    uint16_t section_id;
    uint16_t nPktSize=0, iq_offset = 0;
    uint16_t cid;
    uint16_t vf_id;
    const int16_t rte_mempool_objhdr_size = sizeof(struct rte_mempool_objhdr);
    uint8_t seq_id = 0;
    uint8_t cc_id, ant_id;
    uint8_t compMeth;
    uint8_t iqWidth;
    uint8_t num_eAxc = xran_get_num_eAxc(p_xran_dev_ctx);
    uint8_t num_CCPorts = xran_get_num_cc(p_xran_dev_ctx);

    const uint8_t rte_ether_hdr_size = sizeof(struct rte_ether_hdr);
    uint8_t comp_head_upd = 0;

    const uint8_t total_header_size = (RTE_PKTMBUF_HEADROOM +
        sizeof(struct xran_ecpri_hdr) +
        sizeof(struct radio_app_common_hdr) +
        sizeof(struct data_section_hdr));

    /*direction should be always DL for CSI-RS*/

    for (cc_id = start_cc; cc_id < (start_cc + num_cc); cc_id++)
    {
        if(!xran_isactive_cc(p_xran_dev_ctx, cc_id))
            continue;

        for (ant_id = start_ant; ant_id < (start_ant + num_ant); ant_id++)
        {
            antElm_eAxC_id  = ant_id + p_csirs_cfg->csirsEaxcOffset;
            if(p_xran_dev_ctx->perMu[mu].sFHCsirsTxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers)
                mb_base = p_xran_dev_ctx->perMu[mu].sFHCsirsTxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id].pCtrl;
            else
                continue;
            ptr_sect_elm = p_sec_db->p_sectiondb_elm[ctx_id][direction][cc_id][antElm_eAxC_id];

            if (unlikely(ptr_sect_elm == NULL)){
                printf("ant_id = %d ctx_id = %d,start_ant = %d, num_ant = %d, antElm_eAxC_id = %d\n",ant_id,ctx_id,start_ant,num_ant,antElm_eAxC_id);
                rte_panic("ptr_sect_elm == NULL\n");
                return (0);
            }
            if(0!=ptr_sect_elm->cur_index)
            {
                num_sections = ptr_sect_elm->cur_index;
                /* iterate C-Plane configuration to generate corresponding U-Plane */
                vf_id = xran_map_ecpriPcid_to_vf(p_xran_dev_ctx, direction, cc_id, antElm_eAxC_id);//p_xran_dev_ctx->map2vf[direction][cc_id][antElm_eAxC_id][XRAN_UP_VF];
                ring    = p_xran_dev_ctx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[tti_for_ring % XRAN_N_FE_BUF_LEN][cc_id % num_CCPorts][ant_id % num_eAxc].sBufferList.pBuffers[sym_id_for_ring].pRing;
                temp_buff = ((char*)p_xran_dev_ctx->perMu[mu].sFHCsirsTxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id].pData);
                if (unlikely(mb_base == NULL))
                {
                    rte_panic("ant_id = %d mb == NULL\n",ant_id);
                }
                cid = ((cc_id << p_xran_dev_ctx->eAxc_id_cfg.bit_ccId) & p_xran_dev_ctx->eAxc_id_cfg.mask_ccId) | ((antElm_eAxC_id << p_xran_dev_ctx->eAxc_id_cfg.bit_ruPortId) & p_xran_dev_ctx->eAxc_id_cfg.mask_ruPortId);
                cid = rte_cpu_to_be_16(cid);
#pragma loop_count min=1, max=16
                for (next=0; next< num_sections; next++)
                {
                    sectinfo = &ptr_sect_elm->list[next];

                    if (unlikely(sectinfo == NULL)) {
                        print_err("sectinfo == NULL\n");
                        break;
                    }
                    if (unlikely(sectinfo->type != XRAN_CP_SECTIONTYPE_1 && sectinfo->type != XRAN_CP_SECTIONTYPE_3))
                    {   /* only supports type 1 or type 3*/
                        print_err("Invalid section type in section DB - %d", sectinfo->type);
                        continue;
                    }
                    /* skip, if not scheduled */
                    if (unlikely(sym_id < sectinfo->startSymId || sym_id >= sectinfo->startSymId + sectinfo->numSymbol))
                        continue;
                    compMeth = sectinfo->compMeth;
                    iqWidth = sectinfo->iqWidth;
                    section_id = sectinfo->id;
                    iqWidth = (iqWidth == 0) ? 16 : iqWidth;

                    comp_head_upd = ((compMeth != XRAN_COMPMETHOD_NONE) && (compType == XRAN_COMP_HDR_TYPE_DYNAMIC));

                    if(sectinfo->prbElemBegin)
                    {
                        seq_id = xran_get_updl_seqid(p_xran_dev_ctx->xran_port_id, cc_id, antElm_eAxC_id);
                        iq_sample_size_bytes = 18 +   sizeof(struct xran_ecpri_hdr) +
                                sizeof(struct radio_app_common_hdr);
                    }

                    iq_sample_size_bytes += sizeof(struct data_section_hdr) ;

                    if(comp_head_upd)
                        iq_sample_size_bytes += sizeof(struct data_section_compression_hdr);

                    iq_sample_size_bytes += xran_get_iqdata_len(sectinfo->numPrbc, iqWidth, compMeth);

                    print_dbg(">>> sym %2d [%d] type%d id %d startPrbc=%d numPrbc=%d startSymId=%d numSymbol=%d\n", sym_id, next,
                            sectinfo->type, sectinfo->id, sectinfo->startPrbc,
                            sectinfo->numPrbc, sectinfo->startSymId, sectinfo->numSymbol);

                    if(sectinfo->prbElemBegin)
                    {
                        p_share_data    = &p_xran_dev_ctx->csirs_share_data.sh_data[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id];
                        p_share_data->free_cb = extbuf_free_callback;
                        p_share_data->fcb_opaque = NULL;
                        rte_mbuf_ext_refcnt_set(p_share_data, 1);
                        ext_buff = (char*) (xran_add_hdr_offset((uint8_t*)temp_buff, compMeth));

                        /* Create ethernet + eCPRI + radio app header */
                        // ext_buff_len = sectinfo->sec_desc[sym_id].iq_buffer_len;

                        iq_offset = xran_get_iqdata_len(sectinfo->numPrbc, iqWidth, compMeth);
                        ext_buff_len = iq_offset;
                        temp_buff = ext_buff + iq_offset;

                        ext_buff -= total_header_size;

                        // ext_buff = ((char*)p_xran_dev_ctx->perMu[mu].sFHCsirsTxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id].pData + sectinfo->sec_desc[sym_id].iq_buffer_offset) - total_header_size;

                        ext_buff_len += (total_header_size + 18);
                        if (comp_head_upd)
                        {
                            ext_buff -= sizeof(struct data_section_compression_hdr);
                            ext_buff_len += sizeof(struct data_section_compression_hdr);
                        }

                        mb_oran_hdr_ext = xran_ethdi_mbuf_indir_alloc();
                        if (unlikely((mb_oran_hdr_ext) == NULL))
                        {
                            rte_panic("[core %d]Failed rte_pktmbuf_alloc on vf %d\n", rte_lcore_id(), vf_id);
                        }

#ifdef ENABLE_DEBUG_COREDUMP
                        if (unlikely((struct rte_mempool_objhdr*)RTE_PTR_SUB(mb_base, rte_mempool_objhdr_size)->iova == 0))
                        {
                            rte_panic("Failed rte_mem_virt2iova\n");
                        }
                        if (unlikely(((rte_iova_t)(struct rte_mempool_objhdr*)RTE_PTR_SUB(mb_base, rte_mempool_objhdr_size)->iova) == RTE_BAD_IOVA))
                        {
                            rte_panic("Failed rte_mem_virt2iova RTE_BAD_IOVA \n");
                        }
#endif
                        mb_oran_hdr_ext->buf_addr = ext_buff; //here the address points to the start of the packet
                        mb_oran_hdr_ext->buf_iova = ((struct rte_mempool_objhdr*)RTE_PTR_SUB(mb_base, rte_mempool_objhdr_size))->iova + RTE_PTR_DIFF(ext_buff, mb_base);
                        mb_oran_hdr_ext->buf_len = ext_buff_len;
                        mb_oran_hdr_ext->ol_flags |= RTE_MBUF_F_EXTERNAL;
                        mb_oran_hdr_ext->shinfo = p_share_data;
                        mb_oran_hdr_ext->data_off = (uint16_t)RTE_MIN((uint16_t)RTE_PKTMBUF_HEADROOM, (uint16_t)mb_oran_hdr_ext->buf_len) - rte_ether_hdr_size;
                        mb_oran_hdr_ext->data_len = (uint16_t)(mb_oran_hdr_ext->data_len + rte_ether_hdr_size);
                        mb_oran_hdr_ext->pkt_len = mb_oran_hdr_ext->pkt_len + rte_ether_hdr_size;
                        mb_oran_hdr_ext->port = eth_ctx->io_cfg.port[vf_id];
                        pStart = (char*)((char*)mb_oran_hdr_ext->buf_addr + mb_oran_hdr_ext->data_off);

                        /* Fill in the ethernet header. */
#if (RTE_VER_YEAR >= 21)
                        rte_eth_macaddr_get(mb_oran_hdr_ext->port, &((struct rte_ether_hdr*)pStart)->src_addr);         /* set source addr */
                        ((struct rte_ether_hdr*)pStart)->dst_addr = eth_ctx->entities[vf_id][ID_O_RU];                  /* set dst addr */
#else
                        rte_eth_macaddr_get(mb_oran_hdr_ext->port, &((struct rte_ether_hdr*)pStart)->s_addr);         /* set source addr */
                        ((struct rte_ether_hdr*)pStart)->d_addr = eth_ctx->entities[vf_id][ID_O_RU];                  /* set dst addr */
#endif
                        ((struct rte_ether_hdr*)pStart)->ether_type = ETHER_TYPE_ECPRI_BE;                            /* ethertype */

                        nPktSize = sizeof(struct rte_ether_hdr)
                                                + sizeof(struct xran_ecpri_hdr)
                                                + sizeof(struct radio_app_common_hdr) ;

                        ecpri_hdr = (struct xran_ecpri_hdr*)(pStart + sizeof(struct rte_ether_hdr));

                        ecpri_hdr->cmnhdr.data.data_num_1 = 0x0;
                        ecpri_hdr->cmnhdr.bits.ecpri_ver = XRAN_ECPRI_VER;
                        ecpri_hdr->cmnhdr.bits.ecpri_mesg_type = ECPRI_IQ_DATA;

                        /* one to one lls-CU to RU only and band sector is the same */
                        ecpri_hdr->ecpri_xtc_id = cid;

                        /* no transport layer fragmentation supported */
                        ecpri_hdr->ecpri_seq_id.data.data_num_1 = 0x8000;
                        ecpri_hdr->ecpri_seq_id.bits.seq_id = seq_id;
                        ecpri_hdr->cmnhdr.bits.ecpri_payl_size =  sizeof(struct radio_app_common_hdr) + XRAN_ECPRI_HDR_SZ; //xran_get_ecpri_hdr_size();;;

                    } /* if(sectinfo->prbElemBegin) */

                    /* Prepare U-Plane section hdr */
                    n_bytes = xran_get_iqdata_len(sectinfo->numPrbc, iqWidth, compMeth);
                    n_bytes = RTE_MIN(n_bytes, XRAN_MAX_MBUF_LEN);

                    /* Ethernet & eCPRI added already */
                    nPktSize += sizeof(struct data_section_hdr) + n_bytes;

                    if (comp_head_upd)
                        nPktSize += sizeof(struct data_section_compression_hdr);

                    if(likely((ecpri_hdr!=NULL)))
                    {
                        ecpri_hdr->cmnhdr.bits.ecpri_payl_size += sizeof(struct data_section_hdr) + n_bytes ;

                        if (comp_head_upd)
                            ecpri_hdr->cmnhdr.bits.ecpri_payl_size += sizeof(struct data_section_compression_hdr);
                    }
                    else
                    {
                        print_err("ecpri_hdr should not be NULL\n");
                    }

                    if(sectinfo->prbElemBegin)
                    {
                        pDst = (uint16_t*)(pStart + sizeof(struct rte_ether_hdr) + sizeof(struct xran_ecpri_hdr));
                        pxp = (struct xran_up_pkt_gen_params *)pDst;
                        /* radio app header */
                        pxp->app_params.data_feature.value = 0x10;
                        pxp->app_params.data_feature.data_direction = direction;
                        pxp->app_params.frame_id = frame_id;
                        pxp->app_params.sf_slot_sym.subframe_id = subframe_id;
                        pxp->app_params.sf_slot_sym.slot_id = slot_id;
                        pxp->app_params.sf_slot_sym.symb_id = sym_id;
                        /* convert to network byte order */
                        pxp->app_params.sf_slot_sym.value = rte_cpu_to_be_16(pxp->app_params.sf_slot_sym.value);
                        pDst += 2;
                    }

                    pDataSec = (struct data_section_hdr *)pDst;
                    if(pDataSec){
                        pDataSec->fields.sect_id = XRAN_BASE_SECT_ID_TO_MU_SECT_ID(section_id, mu);
                        pDataSec->fields.num_prbu = (uint8_t)XRAN_CONVERT_NUMPRBC(sectinfo->numPrbc);
                        pDataSec->fields.start_prbu = (sectinfo->startPrbc & 0x03ff);
                        pDataSec->fields.sym_inc = 0;
                        pDataSec->fields.rb = 0;
                        /* network byte order */
                        pDataSec->fields.all_bits = rte_cpu_to_be_32(pDataSec->fields.all_bits);
                        pDst += 2;
                    }
                    else
                    {
                        print_err("pDataSec is NULL idx = %u num_sections = %u\n", next, num_sections);
                        // return 0;
                    }

                    if (comp_head_upd)
                    {
                        if(pDst == NULL){
                            print_err("pDst == NULL\n");
                            return 0;
                        }
                        ((struct data_section_compression_hdr *)pDst)->ud_comp_hdr.ud_comp_meth = compMeth;
                        ((struct data_section_compression_hdr *)pDst)->ud_comp_hdr.ud_iq_width = XRAN_CONVERT_IQWIDTH(iqWidth);
                        ((struct data_section_compression_hdr *)pDst)->rsrvd = 0;
                        pDst++;
                    }

                    //Increment by IQ data len
                    pDst = (uint16_t *)((uint8_t *)pDst + n_bytes) ;
                    if(mb_oran_hdr_ext){
                        rte_pktmbuf_pkt_len(mb_oran_hdr_ext) = nPktSize;
                        rte_pktmbuf_data_len(mb_oran_hdr_ext) = nPktSize;
                    }

                    if(sectinfo->prbElemEnd) /* Transmit the packet */
                    {
                        if(likely((ecpri_hdr!=NULL)))
                            ecpri_hdr->cmnhdr.bits.ecpri_payl_size = rte_cpu_to_be_16(ecpri_hdr->cmnhdr.bits.ecpri_payl_size);
                        else
                            print_err("ecpri_hdr should not be NULL\n");
                        /* if we don't need to do any fragmentation */
                        if(xran_get_syscfg_bbuoffload())
                        {
                            if(likely(ring)) {
                                if(rte_ring_enqueue(ring, mb_oran_hdr_ext)){
                                    rte_panic("Ring enqueue failed. Ring free count [%d].\n",rte_ring_free_count(ring));
                                    rte_pktmbuf_free(mb_oran_hdr_ext);
                                }
                            } else
                                rte_panic("Ring is empty.\n");
                        } else {
                            if (likely(p_xran_dev_ctx->mtu >= (iq_sample_size_bytes)))
                            {
                                int ret = 0;
                                ret = xran_enqueue_mbuf(mb_oran_hdr_ext, eth_ctx->tx_ring[vf_id]);
                                if(ret == 0)
                                    print_err("vf_id = %d sectinfo->startSymId = %d sectinfo->numSymbol = %d",vf_id, sectinfo->startSymId,sectinfo->numSymbol);

                                // p_xran_dev_ctx->send_upmbuf2ring(mb_oran_hdr_ext, ETHER_TYPE_ECPRI, vf_id);
                            }
                            else
                            {
                                return 0;
                            }
                        }
                        elm_bytes += nPktSize;
                    } /* if(prbElemEnd) */
                }/* section loop */
            } /* if ptr_sect_elm->cur_index */
            total_sections += num_sections;
        } /* for(cc_id = 0; cc_id < num_CCPorts; cc_id++) */
    } /* for(ant_id = 0; ant_id < num_eAxc; ant_id++) */

    struct xran_common_counters* pCnt = &p_xran_dev_ctx->fh_counters;
    pCnt->tx_counter += total_sections;
    pCnt->tx_bytes_counter += elm_bytes;
    return 1;
}
int32_t xran_process_tx_sym_oru(void *arg, uint8_t mu)
{
    int32_t     retval = 0;
    uint32_t    tti=0;
    uint32_t    numSlotMu1 = 5;
#if XRAN_MLOG_VAR
    uint32_t    mlogVar[15];
    uint32_t    mlogVarCnt = 0;
#endif
    unsigned long t1 = MLogXRANTick();

    void        *pHandle = NULL;
    int32_t     ant_id   = 0;
    int32_t     cc_id    = 0;
    uint8_t     num_eAxc = 0;
    uint8_t     num_eAxc_prach = 0;
    uint8_t     num_eAxAntElm = 0;
    uint8_t     num_CCPorts = 0;
    uint32_t    frame_id    = 0;
    uint32_t    subframe_id = 0;
    uint32_t    slot_id     = 0;
    uint32_t    sym_id      = 0;
    uint32_t    sym_idx     = 0;
    uint8_t     portId      = 0;
    uint8_t     ctx_id      = 0;
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *) arg;
    enum xran_in_period inPeriod;
    uint32_t interval = xran_fs_get_tti_interval(mu);
    uint8_t eaxcOffset = p_xran_dev_ctx->perMu[mu].eaxcOffset;

    if(p_xran_dev_ctx->xran2phy_mem_ready == 0)
        return 0;

    if(unlikely(p_xran_dev_ctx->xran_port_id >= XRAN_PORTS_NUM))
    {
        print_err("Invalid Port id - %d", p_xran_dev_ctx->xran_port_id);
        return 0;
    }
    portId = p_xran_dev_ctx->xran_port_id;

    pHandle =  p_xran_dev_ctx;

    /* O-RU: send symb after OTA time with delay (UL) */
    /* O-DU: send symb in advance of OTA time (DL) */
    if(xran_get_syscfg_bbuoffload())
    {
        sym_idx = XranOffsetSym(0, xran_lib_ota_sym_idx_mu[mu], XRAN_NUM_OF_SYMBOL_PER_SLOT * SLOTNUM_PER_SUBFRAME(interval) * 1000, &inPeriod);
    }
    else
    {
        sym_idx = XranOffsetSym(p_xran_dev_ctx->perMu[mu].sym_up, xran_lib_ota_sym_idx_mu[mu], XRAN_NUM_OF_SYMBOL_PER_SLOT * SLOTNUM_PER_SUBFRAME(interval) * 1000, &inPeriod);
    }
    tti         = XranGetTtiNum(sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
    slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME(interval));
    subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME(interval),  SUBFRAMES_PER_SYSTEMFRAME);

    uint16_t sfnSecStart = xran_getSfnSecStart();
    if(unlikely(inPeriod == XRAN_IN_NEXT_PERIOD))
    {
        // For DU
        sfnSecStart = (sfnSecStart + NUM_OF_FRAMES_PER_SECOND) & 0x3ff;
    }
    else if(unlikely(inPeriod == XRAN_IN_PREV_PERIOD))
    {
        // For RU
        if (sfnSecStart >= NUM_OF_FRAMES_PER_SECOND)
        {
            sfnSecStart -= NUM_OF_FRAMES_PER_SECOND;
        }
        else
        {
            sfnSecStart += NUM_OF_FRAMES_PER_SFN_PERIOD - NUM_OF_FRAMES_PER_SECOND;
        }
    }

    frame_id    = XranGetFrameNum(tti, sfnSecStart, SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval));
    // ORAN frameId, 8 bits, [0, 255]
    frame_id = (frame_id & 0xff);

    sym_id      = XranGetSymNum(sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
    ctx_id      = tti % XRAN_MAX_SECTIONDB_CTX;

    print_dbg("[%d]SFN %d sf %d slot %d\n", tti, frame_id, subframe_id, slot_id);

#if XRAN_MLOG_VAR
    mlogVar[mlogVarCnt++] = 0xAAAAAAAA;
    mlogVar[mlogVarCnt++] = xran_lib_ota_sym_idx_mu[mu];
    mlogVar[mlogVarCnt++] = sym_idx;
    mlogVar[mlogVarCnt++] = abs(p_xran_dev_ctx->sym_up);
    mlogVar[mlogVarCnt++] = tti;
    mlogVar[mlogVarCnt++] = frame_id;
    mlogVar[mlogVarCnt++] = subframe_id;
    mlogVar[mlogVarCnt++] = slot_id;
    mlogVar[mlogVarCnt++] = sym_id;
    mlogVar[mlogVarCnt++] = portId;
    MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());
#endif

//    if(p_xran_dev_ctx->fh_init.io_cfg.id == O_RU) // this functions is called only with RU mode
    {
        if( (XRAN_NBIOT_MU==mu) && (XRAN_NBIOT_UL_SCS_3_75 == p_xran_dev_ctx->fh_cfg.perMu[mu].nbIotUlScs) )
        { /* for NB-IOT 3.75 UL: RU will only transmit on specific symbols
             This implementation is based on draft: SOL-2021.10.05-WG4-C.Section-Type-1-3-for-NB-IoT_v6
             This could change approved ORAN CUS specification is different */
            if(!(tti%2))
            {
                switch(sym_id)
                {
                case 0:
                case 3:
                case 7:
                case 11:
                    break;
                default:
                    //printf("mu=4, returning from first switch: tti=%u, sym_id=%u\n", tti, sym_id);
                    return 0;
                }
            }
            else
            {
                switch(sym_id)
                {
                case 1:
                case 5:
                case 9:
                    break;
                default:
                    //printf("mu=4, returning from second switch: tti=%u, sym_id=%u\n", tti, sym_id);
                    return 0;
                }
            }
        }
        num_eAxc    = xran_get_num_eAxcUl(pHandle);
    }

    num_eAxc_prach = ((num_eAxc > XRAN_MAX_PRACH_ANT_NUM)? XRAN_MAX_PRACH_ANT_NUM : num_eAxc);
    num_CCPorts = xran_get_num_cc(pHandle);

    /* U-Plane */
    {
        if((xran_get_syscfg_bbuoffload() == 0) && first_call)
        {
            if(p_xran_dev_ctx->enableCP)
            {
                enum xran_comp_hdr_type compType;
                PSECTION_DB_TYPE p_sec_db = NULL;

                if(xran_fs_get_slot_type(portId, cc_id, tti, XRAN_SLOT_TYPE_UL, mu) ==  1
                    || xran_fs_get_slot_type(portId, cc_id, tti, XRAN_SLOT_TYPE_SP, mu) ==  1
                    || xran_fs_get_slot_type(portId, cc_id, tti, XRAN_SLOT_TYPE_FDD, mu) ==  1)
                {

                    if(xran_fs_get_symbol_type(portId, cc_id, tti, sym_id, mu) == XRAN_SYMBOL_TYPE_UL
                        || xran_fs_get_symbol_type(portId, cc_id, tti, sym_id, mu) == XRAN_SYMBOL_TYPE_FDD)
                    {

                        uint8_t loc_ret = 1;
                        compType = p_xran_dev_ctx->fh_cfg.ru_conf.xranCompHdrType;


                        if(unlikely(ctx_id > XRAN_MAX_SECTIONDB_CTX))
                        {
                            print_err("Invalid Context id - %d", ctx_id);
                            loc_ret = 0;
                        }

                        if(unlikely(num_CCPorts > XRAN_COMPONENT_CARRIERS_MAX))
                        {
                            print_err("Invalid CC id - %d", num_CCPorts);
                            loc_ret = 0;
                        }

                        if(unlikely(num_eAxc > (XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR)))
                        {
                            print_err("Invalid eAxC id - %d", num_eAxc);
                            loc_ret = 0;
                        }

                        p_sec_db = p_sectiondb[portId][mu];

                        if (loc_ret)
                        {
                            xran_process_tx_sym_cp_on_opt(pHandle, ctx_id, tti,
                                                0, num_CCPorts, eaxcOffset, num_eAxc, frame_id, subframe_id, slot_id, sym_id,
                                                compType, XRAN_DIR_UL, portId, p_sec_db, mu, tti, sym_id, 0);
                        }
                        else
                        {
                            retval = 0;
                        }
                    }
                }

                if(xran_get_ru_category(pHandle) == XRAN_CATEGORY_B
                        && p_xran_dev_ctx->enableSrs)
                {
                    p_sec_db = p_sectiondb[portId][mu];
                    compType = p_xran_dev_ctx->fh_cfg.ru_conf.xranCompHdrType;
                    struct xran_srs_config *pSrsCfg = &(p_xran_dev_ctx->srs_cfg);
                    struct xran_prb_map *prb_map;
                    /* check SRS NDM UP has been scheduled in slots */
                    if(p_xran_dev_ctx->ndm_srs_scheduled
                            && p_xran_dev_ctx->ndm_srs_txtti == tti)
                    {
                        prb_map = (struct xran_prb_map *)p_xran_dev_ctx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][0].sBufferList.pBuffers->pData;
                        p_sec_db = p_sectiondb[portId][mu];
                        int ndm_step;
                        uint32_t srs_tti, srsFrame, srsSubframe, srsSlot, srs_sym;
                        uint8_t  srsCtx;
                        if(prb_map && prb_map->nPrbElm)
                        {
                            srs_sym = prb_map->prbMap[0].nStartSymb;

                            srs_tti     = p_xran_dev_ctx->ndm_srs_tti;
                            num_eAxAntElm = xran_get_num_ant_elm(pHandle);
                            ndm_step    = num_eAxAntElm / pSrsCfg->ndm_txduration;

                            srsSlot     = XranGetSlotNum(srs_tti, SLOTNUM_PER_SUBFRAME(interval));
                            srsSubframe = XranGetSubFrameNum(srs_tti,SLOTNUM_PER_SUBFRAME(interval),  SUBFRAMES_PER_SYSTEMFRAME);
                            srsFrame    = XranGetFrameNum(srs_tti,sfnSecStart,SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval));
                            srsFrame    = (srsFrame & 0xff);
                            srsCtx      = srs_tti % XRAN_MAX_SECTIONDB_CTX;

                            if(sym_id < pSrsCfg->ndm_txduration)
                            {
                                retval = xran_process_tx_srs_cp_on(pHandle, srsCtx, srs_tti,
                                        0, num_CCPorts, sym_id*ndm_step, ndm_step, srsFrame, srsSubframe, srsSlot, srs_sym,
                                        compType, XRAN_DIR_UL, portId, p_sec_db, mu);
                            }
                            else
                            {
                                p_xran_dev_ctx->ndm_srs_scheduled   = 0;
                                p_xran_dev_ctx->ndm_srs_tti         = 0;
                                p_xran_dev_ctx->ndm_srs_txtti       = 0;
                                p_xran_dev_ctx->ndm_srs_schedperiod = 0;
                            }
                        }
                    }
                    /* check special frame or uplink frame*/
                    else  if((xran_fs_get_slot_type(portId, cc_id, tti, XRAN_SLOT_TYPE_SP, mu) ==  1)
                        || (xran_fs_get_slot_type(portId, cc_id, tti, XRAN_SLOT_TYPE_UL, mu) ==  1))
                    {
                        if(((tti % p_xran_dev_ctx->fh_cfg.frame_conf.nTddPeriod) == pSrsCfg->slot)
                            && (p_xran_dev_ctx->ndm_srs_scheduled == 0))
                        {

                            prb_map = (struct xran_prb_map *)p_xran_dev_ctx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][0].sBufferList.pBuffers->pData;
                            /* NDM U-Plane is not enabled */
                            if(pSrsCfg->ndm_offset == 0)
                            {
                                retval = xran_process_tx_srs_cp_on(pHandle, ctx_id, tti,
                                            0, num_CCPorts, 0, xran_get_num_ant_elm(pHandle), frame_id, subframe_id, slot_id, sym_id,
                                            compType, XRAN_DIR_UL, portId, p_sec_db, mu);
                            }
                            /* NDM U-Plane is enabled, SRS U-Planes will be transmitted after ndm_offset (in slots) */
                            else
                            {
                                p_xran_dev_ctx->ndm_srs_scheduled   = 1;
                                p_xran_dev_ctx->ndm_srs_tti         = tti;
                                p_xran_dev_ctx->ndm_srs_txtti       = (tti + pSrsCfg->ndm_offset)%2000;
                                p_xran_dev_ctx->ndm_srs_schedperiod = pSrsCfg->slot;
                            }

                        }
                    }
                }
            } else {
            for (ant_id = 0; ant_id < num_eAxc; ant_id++)
            {
                for (cc_id = 0; cc_id < num_CCPorts; cc_id++)
                {
                    if(xran_isactive_cc(p_xran_dev_ctx, cc_id))
                        continue;
                    //struct xran_srs_config *p_srs_cfg = &(p_xran_dev_ctx->srs_cfg);
                    if(p_xran_dev_ctx->puschMaskEnable)
                    {
                        if((tti % numSlotMu1) != p_xran_dev_ctx->puschMaskSlot)
                        {
                            if(p_xran_dev_ctx->tx_sym_gen_func && p_xran_dev_ctx->use_tx_sym_gen_func){
                                p_xran_dev_ctx->tx_sym_gen_func(pHandle, ctx_id, tti, cc_id, 1, ant_id, 1, frame_id, subframe_id, slot_id, sym_id,
                                                                p_xran_dev_ctx->fh_cfg.ru_conf.xranCompHdrType, XRAN_DIR_UL, p_xran_dev_ctx->xran_port_id,
                                                                NULL, mu, 0, 0, 0);
                            }
                            else{
                                retval = xran_process_tx_sym_cp_off(pHandle, ctx_id, tti, cc_id, ant_id, frame_id, subframe_id, slot_id, sym_id, 0, mu, tti, sym_id);
                            }
                        }
                    }
                    else{

                            if(p_xran_dev_ctx->tx_sym_gen_func && p_xran_dev_ctx->use_tx_sym_gen_func){
                                p_xran_dev_ctx->tx_sym_gen_func(pHandle, ctx_id, tti, cc_id, 1, ant_id, 1, frame_id, subframe_id, slot_id, sym_id,
                                                                    p_xran_dev_ctx->fh_cfg.ru_conf.xranCompHdrType, XRAN_DIR_UL, p_xran_dev_ctx->xran_port_id,
                                                                    NULL, mu, 0, 0, 0);

                            }
                            else{
                                retval = xran_process_tx_sym_cp_off(pHandle, ctx_id, tti, cc_id, ant_id, frame_id, subframe_id, slot_id, sym_id, 0, mu, tti, sym_id);
                            }

                    }
                    if(p_xran_dev_ctx->perMu[mu].enablePrach && (ant_id < num_eAxc_prach) )
                    {
                        retval = xran_process_tx_prach_cp_off(pHandle, ctx_id, tti, cc_id, ant_id, frame_id, subframe_id, slot_id, sym_id, mu, tti, sym_id);
                    }
                }
            }

            if(xran_get_ru_category(pHandle) == XRAN_CATEGORY_B
                    && p_xran_dev_ctx->enableSrs)
            {
                struct xran_srs_config *pSrsCfg = &(p_xran_dev_ctx->srs_cfg);

                for(cc_id = 0; cc_id < num_CCPorts; cc_id++)
                {
                    /* check SRS NDM UP has been scheduled in non special slots */
                    /*NDM feature enables the spread of SRS packets
                      Non delay measurement SRS PDSCH PUSCH delay measure it*/
                    if(p_xran_dev_ctx->ndm_srs_scheduled
                            && p_xran_dev_ctx->ndm_srs_txtti == tti)
                    {
                        int ndm_step;
                        uint32_t srs_tti, srsFrame, srsSubframe, srsSlot;
                        uint8_t  srsCtx;

                        srs_tti     = p_xran_dev_ctx->ndm_srs_tti;
                        num_eAxAntElm = xran_get_num_ant_elm(pHandle);
                        ndm_step    = num_eAxAntElm / pSrsCfg->ndm_txduration;

                        srsSlot     = XranGetSlotNum(srs_tti, SLOTNUM_PER_SUBFRAME(interval));
                        srsSubframe = XranGetSubFrameNum(srs_tti,SLOTNUM_PER_SUBFRAME(interval),  SUBFRAMES_PER_SYSTEMFRAME);
                        srsFrame    = XranGetFrameNum(srs_tti,sfnSecStart,SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval));
                        srsFrame    = (srsFrame & 0xff);
                        srsCtx      = srs_tti % XRAN_MAX_SECTIONDB_CTX;

                        if(sym_id < pSrsCfg->ndm_txduration)
                        {
                            uint64_t t1 = MLogXRANTick();
                            for(ant_id=sym_id*ndm_step; ant_id < (sym_id+1)*ndm_step; ant_id++)
                                xran_process_tx_srs_cp_off(pHandle, srsCtx, srs_tti, cc_id, ant_id, srsFrame, srsSubframe, srsSlot);
                            MLogXRANTask(PID_PROCESS_TX_SYM_SRS_NDM, t1, MLogXRANTick());
                        }
                        else
                        {
                            p_xran_dev_ctx->ndm_srs_scheduled   = 0;
                            p_xran_dev_ctx->ndm_srs_tti         = 0;
                            p_xran_dev_ctx->ndm_srs_txtti       = 0;
                            p_xran_dev_ctx->ndm_srs_schedperiod = 0;
                        }
                    }
                    /* check special frame or uplink frame */
                    else if((xran_fs_get_slot_type(portId, cc_id, tti, XRAN_SLOT_TYPE_SP, mu) ==  1) ||
                        (xran_fs_get_slot_type(portId, cc_id, tti, XRAN_SLOT_TYPE_UL, mu) ==  1))
                    {
                        if(((tti % p_xran_dev_ctx->fh_cfg.frame_conf.nTddPeriod) == pSrsCfg->slot)
                            && (p_xran_dev_ctx->ndm_srs_scheduled == 0))
                        {
                            struct xran_prb_map *prb_map;
                            prb_map = (struct xran_prb_map *)p_xran_dev_ctx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][0].sBufferList.pBuffers->pData;

                            /* if PRB map is present in first antenna, assume SRS might be scheduled. */
                            if(prb_map && prb_map->nPrbElm)
                            {
                                /* NDM U-Plane is not enabled */
                                if(pSrsCfg->ndm_offset == 0)
                                {
                                    if (prb_map->nPrbElm > 0)
                                    {
                                        if(sym_id >= prb_map->prbMap[0].nStartSymb
                                                && sym_id < (prb_map->prbMap[0].nStartSymb + prb_map->prbMap[0].numSymb))
                                            for(ant_id=0; ant_id < xran_get_num_ant_elm(pHandle); ant_id++)
                                                xran_process_tx_srs_cp_off(pHandle, ctx_id, tti, cc_id, ant_id, frame_id, subframe_id, slot_id);

                                    }
                                }
                                /* NDM U-Plane is enabled, SRS U-Planes will be transmitted after ndm_offset (in slots) */
                                else
                                {
                                    p_xran_dev_ctx->ndm_srs_scheduled   = 1;
                                    p_xran_dev_ctx->ndm_srs_tti         = tti;
                                    p_xran_dev_ctx->ndm_srs_txtti       = (tti + pSrsCfg->ndm_offset)%2000;
                                    p_xran_dev_ctx->ndm_srs_schedperiod = pSrsCfg->slot;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    else if(xran_get_syscfg_bbuoffload() == 1)
    {
        if(p_xran_dev_ctx->tx_sym_gen_func)
        {
            enum xran_comp_hdr_type compType;
            uint8_t loc_ret = 1;
            PSECTION_DB_TYPE p_sec_db = NULL;

            compType = p_xran_dev_ctx->fh_cfg.ru_conf.xranCompHdrType;

            if(unlikely(ctx_id > XRAN_MAX_SECTIONDB_CTX))
            {
                print_err("Invalid Context id - %d", ctx_id);
                loc_ret = 0;
            }

            if(unlikely(num_CCPorts > XRAN_COMPONENT_CARRIERS_MAX))
            {
                print_err("Invalid CC id - %d", num_CCPorts);
                loc_ret = 0;
            }

            if(unlikely(num_eAxc > (XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR)))
            {
                print_err("Invalid eAxC id - %d", num_eAxc);
                loc_ret = 0;
            }

            p_sec_db = p_sectiondb[portId][mu];

            if (loc_ret)
            {
                p_xran_dev_ctx->tx_sym_gen_func(pHandle, ctx_id, tti,
                                    0, num_CCPorts, eaxcOffset, num_eAxc, frame_id, subframe_id, slot_id, sym_id,
                                compType, XRAN_DIR_UL, portId, p_sec_db, mu, tti, sym_id, 0);
            }
            else
            {
                retval = 0;
            }
        }
            else
            {
                rte_panic("p_xran_dev_ctx->tx_sym_gen_func== NULL\n");
            }
        }
    }

    MLogXRANTask(PID_PROCESS_TX_SYM, t1, MLogXRANTick());
    return retval;
}


int32_t xran_process_tx_sym(void *arg, uint8_t mu)
{
    int32_t     retval = 0;
    uint32_t    tti=0;
    uint32_t    numSlotMu1 = 5;
#if XRAN_MLOG_VAR
    uint32_t    mlogVar[15];
    uint32_t    mlogVarCnt = 0;
#endif
    unsigned long t1 = MLogXRANTick();

    void        *pHandle = NULL;
    int32_t     ant_id   = 0;
    int32_t     cc_id    = 0;
    uint8_t     num_eAxc = 0;
    uint8_t     num_eAxc_prach = 0;
    uint8_t     num_eAxAntElm = 0;
    uint8_t     num_CCPorts = 0;
    uint32_t    frame_id    = 0;
    uint32_t    subframe_id = 0;
    uint32_t    slot_id     = 0;
    uint32_t    sym_id      = 0;
    uint32_t    sym_idx     = 0;
    uint8_t     ctx_id      = 0;
    uint8_t     portId      = 0;

    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *) arg;
    enum xran_in_period inPeriod;
    uint32_t interval = xran_fs_get_tti_interval(mu);
    uint8_t eaxcOffset = p_xran_dev_ctx->perMu[mu].eaxcOffset;
    uint32_t taskid = PID_PROCESS_TX_SYM;
#ifdef POLL_EBBU_OFFLOAD
    PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();
    first_call = pCtx->first_call;
#endif
    if(p_xran_dev_ctx->xran2phy_mem_ready == 0)
        return 0;

    if(unlikely(p_xran_dev_ctx->xran_port_id >= XRAN_PORTS_NUM))
    {
        print_err("Invalid Port id - %d", p_xran_dev_ctx->xran_port_id);
        return 0;
    }

    portId = p_xran_dev_ctx->xran_port_id;
    pHandle =  p_xran_dev_ctx;

    int appMode = xran_get_syscfg_appmode();

    if(appMode == O_RU)
    {
        if(p_xran_dev_ctx->tx_sym_func)
        {
            p_xran_dev_ctx->tx_sym_func(arg, mu);
            MLogXRANTask(PID_PROCESS_TX_SYM, t1, MLogXRANTick());
            return 0;
        }
    }

    /* O-RU: send symb after OTA time with delay (UL) */
    /* O-DU: send symb in advance of OTA time (DL) */
#ifndef POLL_EBBU_OFFLOAD
    if(xran_get_syscfg_bbuoffload())
    {
        sym_idx = XranOffsetSym(0, xran_lib_ota_sym_idx_mu[mu], XRAN_NUM_OF_SYMBOL_PER_SLOT * SLOTNUM_PER_SUBFRAME(interval) * 1000, &inPeriod);
    } else{
        sym_idx = XranOffsetSym(p_xran_dev_ctx->perMu[mu].sym_up, xran_lib_ota_sym_idx_mu[mu], XRAN_NUM_OF_SYMBOL_PER_SLOT * SLOTNUM_PER_SUBFRAME(interval) * 1000, &inPeriod);
    }
#else
    if(xran_get_syscfg_bbuoffload())
    {
        sym_idx = XranOffsetSym(0, pCtx->ebbu_offload_ota_sym_cnt_mu[mu], XRAN_NUM_OF_SYMBOL_PER_SLOT * SLOTNUM_PER_SUBFRAME(interval) * 1000, &inPeriod);
    }
    else
    {
        sym_idx = XranOffsetSym(p_xran_dev_ctx->perMu[mu].sym_up, pCtx->ebbu_offload_ota_sym_cnt_mu[mu], XRAN_NUM_OF_SYMBOL_PER_SLOT * SLOTNUM_PER_SUBFRAME(interval) * 1000, &inPeriod);
    }
#endif
    tti         = XranGetTtiNum(sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
    slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME(interval));
    subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME(interval),  SUBFRAMES_PER_SYSTEMFRAME);

    uint16_t sfnSecStart = xran_getSfnSecStart();
    if(unlikely(inPeriod == XRAN_IN_NEXT_PERIOD))
    {
        // For DU
        sfnSecStart = (sfnSecStart + NUM_OF_FRAMES_PER_SECOND) & 0x3ff;
    }
    else if(unlikely(inPeriod == XRAN_IN_PREV_PERIOD))
    {
        // For RU
        if (sfnSecStart >= NUM_OF_FRAMES_PER_SECOND)
        {
            sfnSecStart -= NUM_OF_FRAMES_PER_SECOND;
        }
        else
        {
            sfnSecStart += NUM_OF_FRAMES_PER_SFN_PERIOD - NUM_OF_FRAMES_PER_SECOND;
        }
    }

    frame_id    = XranGetFrameNum(tti, sfnSecStart, SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval));
    // ORAN frameId, 8 bits, [0, 255]
    frame_id = (frame_id & 0xff);
    sym_id      = XranGetSymNum(sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
    ctx_id      = tti % XRAN_MAX_SECTIONDB_CTX;

    print_dbg("[%d]SFN %d sf %d slot %d\n", tti, frame_id, subframe_id, slot_id);

#if XRAN_MLOG_VAR
    mlogVar[mlogVarCnt++] = 0xAAAAAAAA;
#ifndef POLL_EBBU_OFFLOAD
    mlogVar[mlogVarCnt++] = xran_lib_ota_sym_idx_mu[mu];
#else
    mlogVar[mlogVarCnt++] = pCtx->ebbu_offload_ota_sym_cnt_mu[mu];
#endif
    mlogVar[mlogVarCnt++] = sym_idx;
    mlogVar[mlogVarCnt++] = abs(p_xran_dev_ctx->sym_up);
    mlogVar[mlogVarCnt++] = tti;
    mlogVar[mlogVarCnt++] = frame_id;
    mlogVar[mlogVarCnt++] = subframe_id;
    mlogVar[mlogVarCnt++] = slot_id;
    mlogVar[mlogVarCnt++] = sym_id;
    mlogVar[mlogVarCnt++] = portId;
    MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());
#endif

    if(appMode == O_RU)
    {
        if( (XRAN_NBIOT_MU==mu) && (XRAN_NBIOT_UL_SCS_3_75 == p_xran_dev_ctx->fh_cfg.perMu[mu].nbIotUlScs) )
        { /* for NB-IOT 3.75 UL: RU will only transmit on specific symbols
             This implementation is based on draft: SOL-2021.10.05-WG4-C.Section-Type-1-3-for-NB-IoT_v6
             This could change approved ORAN CUS specification is different */
            if(!(tti%2))
            {
                switch(sym_id)
                {
                case 0:
                case 3:
                case 7:
                case 11:
                    break;
                default:
                    //printf("mu=4, returning from first switch: tti=%u, sym_id=%u\n", tti, sym_id);
                    return 0;
                }
            }
            else
            {
                switch(sym_id)
                {
                case 1:
                case 5:
                case 9:
                    break;
                default:
                    //printf("mu=4, returning from second switch: tti=%u, sym_id=%u\n", tti, sym_id);
                    return 0;
                }
            }
        }
        num_eAxc    = xran_get_num_eAxcUl(pHandle);
    }
    else
    {
            num_eAxc    = xran_get_num_eAxc(pHandle);
    }

    num_eAxc_prach = ((num_eAxc > XRAN_MAX_PRACH_ANT_NUM)? XRAN_MAX_PRACH_ANT_NUM : num_eAxc);
    num_CCPorts = xran_get_num_cc(pHandle);

    /* U-Plane */
    if(appMode == O_DU && p_xran_dev_ctx->enableCP)
    {
        if(p_xran_dev_ctx->tx_sym_gen_func)
        {
            enum xran_comp_hdr_type compType;
            uint8_t loc_ret = 1;
            PSECTION_DB_TYPE p_sec_db = NULL;

            compType = p_xran_dev_ctx->fh_cfg.ru_conf.xranCompHdrType;

            if(unlikely(ctx_id > XRAN_MAX_SECTIONDB_CTX))
            {
                print_err("Invalid Context id - %d", ctx_id);
                loc_ret = 0;
            }

            if(unlikely(num_CCPorts > XRAN_COMPONENT_CARRIERS_MAX))
            {
                print_err("Invalid CC id - %d", num_CCPorts);
                loc_ret = 0;
            }

            if(unlikely(num_eAxc > (XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR)))
            {
                print_err("Invalid eAxC id - %d", num_eAxc);
                loc_ret = 0;
            }

            p_sec_db = p_sectiondb[portId][mu];

            if(loc_ret)
            {
                p_xran_dev_ctx->tx_sym_gen_func(pHandle, ctx_id, tti,
                                    0, num_CCPorts, eaxcOffset, num_eAxc, frame_id, subframe_id, slot_id, sym_id,
                                    compType, XRAN_DIR_DL, portId, p_sec_db, mu, tti, sym_id, 0);
                if(portId != 0)
                    taskid = PID_DISPATCH_TX_SYM;
            }
            else
            {
                retval = 0;
            }

            if(p_xran_dev_ctx->csirsEnable && xran_get_syscfg_bbuoffload() == 0
                    && xran_get_ru_category(pHandle) == XRAN_CATEGORY_B)
            {
                retval = xran_process_tx_csirs_cp_on(pHandle, ctx_id, tti,
                                    0, num_CCPorts, 0 /*pCsirsCfg->csirsEaxcOffset*/, XRAN_MAX_CSIRS_PORTS, frame_id, subframe_id, slot_id, sym_id,
                                    compType, XRAN_DIR_DL, portId, p_sec_db, mu, tti, sym_id);
            }

            if(p_xran_dev_ctx->vMuInfo.ssbInfo.ssbMu > mu
                    && xran_get_ru_category(pHandle) == XRAN_CATEGORY_A
                    && xran_get_syscfg_bbuoffload() == 0)
            {
                if((p_xran_dev_ctx->vMuInfo.ssbInfo.ssbMu == 4) && (mu == 3)){

                    uint8_t ssbMu = p_xran_dev_ctx->vMuInfo.ssbInfo.ssbMu;
                    uint8_t numSsbSymPerTimerMuSym = 1 << (ssbMu - mu), k;

                    for(k = 0; k < numSsbSymPerTimerMuSym; ++k){
                        
                        // 1. sym_idx is the symbol being processed now (ota + T1a_max_up)- Derive SSB timing from this
                        uint32_t ssbSymIdx = (sym_idx * numSsbSymPerTimerMuSym) + k; //SSB symIdx in a sec
                        uint32_t ssbSymId  = XranGetSymNum(ssbSymIdx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
                        uint32_t ssbTti    = XranGetTtiNum(ssbSymIdx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
                        uint32_t ssbSlotId = ssbTti % SLOTNUM_PER_SUBFRAME_MU(ssbMu);
                        uint32_t ssbSfId   = (ssbTti / SLOTNUM_PER_SUBFRAME_MU(ssbMu)) % SUBFRAMES_PER_SYSTEMFRAME;
                        PSECTION_DB_TYPE ssb_p_secdb = p_sectiondb[portId][ssbMu];

                        if(unlikely(xran_process_tx_sym_cp_on_opt(pHandle, (ssbTti % XRAN_MAX_SECTIONDB_CTX), ssbTti, 0, num_CCPorts,
                                                         p_xran_dev_ctx->vMuInfo.ssbInfo.ruPortId_offset, num_eAxc, frame_id, 
                                                         ssbSfId, ssbSlotId, ssbSymId, compType, XRAN_DIR_DL, XRAN_GET_OXU_PORT_ID(p_xran_dev_ctx), 
                                                         ssb_p_secdb, mu, tti, sym_id, 1)) != 1)
                        {
                            print_err("SSB UP failed: sym %u tti %u",ssbSymId,ssbTti);
                        }
                    
                    }
                }
            }
        }
        else
        {
            rte_panic("p_xran_dev_ctx->tx_sym_gen_func== NULL\n");
        }
    }
    else if(appMode == O_RU)
    {
        if(xran_get_syscfg_bbuoffload() == 0 && first_call)
        {
            if(p_xran_dev_ctx->enableCP)
            {
                enum xran_comp_hdr_type compType;
                PSECTION_DB_TYPE p_sec_db = NULL;

                if(xran_fs_get_slot_type(portId, cc_id, tti, XRAN_SLOT_TYPE_UL, mu) ==  1
                    || xran_fs_get_slot_type(portId, cc_id, tti, XRAN_SLOT_TYPE_SP, mu) ==  1
                    || xran_fs_get_slot_type(portId, cc_id, tti, XRAN_SLOT_TYPE_FDD, mu) ==  1)
                {

                    if(xran_fs_get_symbol_type(portId, cc_id, tti, sym_id, mu) == XRAN_SYMBOL_TYPE_UL
                        || xran_fs_get_symbol_type(portId, cc_id, tti, sym_id, mu) == XRAN_SYMBOL_TYPE_FDD)
                    {

                        uint8_t loc_ret = 1;
                        compType = p_xran_dev_ctx->fh_cfg.ru_conf.xranCompHdrType;

                        if(unlikely(ctx_id > XRAN_MAX_SECTIONDB_CTX))
                        {
                            print_err("Invalid Context id - %d", ctx_id);
                            loc_ret = 0;
                        }

                        if(unlikely(num_CCPorts > XRAN_COMPONENT_CARRIERS_MAX))
                        {
                            print_err("Invalid CC id - %d", num_CCPorts);
                            loc_ret = 0;
                        }

                        if(unlikely(num_eAxc > (XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR)))
                        {
                            print_err("Invalid eAxC id - %d", num_eAxc);
                            loc_ret = 0;
                        }

                        p_sec_db = p_sectiondb[portId][mu];

                        if (loc_ret)
                        {
                            xran_process_tx_sym_cp_on_opt(pHandle, ctx_id, tti,
                                                0, num_CCPorts, eaxcOffset, num_eAxc, frame_id, subframe_id, slot_id, sym_id,
                                                compType, XRAN_DIR_UL, portId, p_sec_db, mu, tti, sym_id, 0);
                        }
                        else
                        {
                            retval = 0;
                        }
                    }
                }

                if(xran_get_ru_category(pHandle) == XRAN_CATEGORY_B
                        && p_xran_dev_ctx->enableSrs)
                {
                    p_sec_db = p_sectiondb[portId][mu];
                    compType = p_xran_dev_ctx->fh_cfg.ru_conf.xranCompHdrType;
                    struct xran_srs_config *pSrsCfg = &(p_xran_dev_ctx->srs_cfg);
                    struct xran_prb_map *prb_map;
                    /* check SRS NDM UP has been scheduled in slots */
                    if(p_xran_dev_ctx->ndm_srs_scheduled
                            && p_xran_dev_ctx->ndm_srs_txtti == tti)
                    {
                        prb_map = (struct xran_prb_map *)p_xran_dev_ctx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][0].sBufferList.pBuffers->pData;
                        p_sec_db = p_sectiondb[portId][mu];
                        int ndm_step;
                        uint32_t srs_tti, srsFrame, srsSubframe, srsSlot, srs_sym;
                        uint8_t  srsCtx;
                        if(prb_map && prb_map->nPrbElm)
                        {
                            srs_sym = prb_map->prbMap[0].nStartSymb;

                            srs_tti     = p_xran_dev_ctx->ndm_srs_tti;
                            num_eAxAntElm = xran_get_num_ant_elm(pHandle);
                            ndm_step    = num_eAxAntElm / pSrsCfg->ndm_txduration;

                            srsSlot     = XranGetSlotNum(srs_tti, SLOTNUM_PER_SUBFRAME(interval));
                            srsSubframe = XranGetSubFrameNum(srs_tti,SLOTNUM_PER_SUBFRAME(interval),  SUBFRAMES_PER_SYSTEMFRAME);
                            srsFrame    = XranGetFrameNum(srs_tti,sfnSecStart,SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval));
                            srsFrame    = (srsFrame & 0xff);
                            srsCtx      = srs_tti % XRAN_MAX_SECTIONDB_CTX;

                            if(sym_id < pSrsCfg->ndm_txduration)
                            {
                                retval = xran_process_tx_srs_cp_on(pHandle, srsCtx, srs_tti,
                                        0, num_CCPorts, sym_id*ndm_step, ndm_step, srsFrame, srsSubframe, srsSlot, srs_sym,
                                        compType, XRAN_DIR_UL, portId, p_sec_db, mu);
                            }
                            else
                            {
                                p_xran_dev_ctx->ndm_srs_scheduled   = 0;
                                p_xran_dev_ctx->ndm_srs_tti         = 0;
                                p_xran_dev_ctx->ndm_srs_txtti       = 0;
                                p_xran_dev_ctx->ndm_srs_schedperiod = 0;
                            }
                        }
                    }
                    /* check special frame or uplink frame*/
                    else  if((xran_fs_get_slot_type(portId, cc_id, tti, XRAN_SLOT_TYPE_SP, mu) ==  1)
                        || (xran_fs_get_slot_type(portId, cc_id, tti, XRAN_SLOT_TYPE_UL, mu) ==  1))
                    {
                        if(((tti % p_xran_dev_ctx->fh_cfg.frame_conf.nTddPeriod) == pSrsCfg->slot)
                            && (p_xran_dev_ctx->ndm_srs_scheduled == 0))
                        {

                            prb_map = (struct xran_prb_map *)p_xran_dev_ctx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][0].sBufferList.pBuffers->pData;
                            /* NDM U-Plane is not enabled */
                            if(pSrsCfg->ndm_offset == 0)
                            {
                                retval = xran_process_tx_srs_cp_on(pHandle, ctx_id, tti,
                                            0, num_CCPorts, 0, xran_get_num_ant_elm(pHandle), frame_id, subframe_id, slot_id, sym_id,
                                            compType, XRAN_DIR_UL, portId, p_sec_db, mu);
                            }
                            /* NDM U-Plane is enabled, SRS U-Planes will be transmitted after ndm_offset (in slots) */
                            else
                            {
                                p_xran_dev_ctx->ndm_srs_scheduled   = 1;
                                p_xran_dev_ctx->ndm_srs_tti         = tti;
                                p_xran_dev_ctx->ndm_srs_txtti       = (tti + pSrsCfg->ndm_offset)%2000;
                                p_xran_dev_ctx->ndm_srs_schedperiod = pSrsCfg->slot;
                            }

                        }
                    }
                }
            } else {
                for (ant_id = 0; ant_id < num_eAxc; ant_id++)
                {
                    for (cc_id = 0; cc_id < num_CCPorts; cc_id++)
                    {
                        if(!xran_isactive_cc(p_xran_dev_ctx, cc_id))
                            continue;

                        //struct xran_srs_config *p_srs_cfg = &(p_xran_dev_ctx->srs_cfg);
                        if(p_xran_dev_ctx->puschMaskEnable)
                        {
                            if((tti % numSlotMu1) != p_xran_dev_ctx->puschMaskSlot)
                            {
                                if(p_xran_dev_ctx->tx_sym_gen_func && p_xran_dev_ctx->use_tx_sym_gen_func){
                                    p_xran_dev_ctx->tx_sym_gen_func(pHandle, ctx_id, tti, cc_id, 1, ant_id, 1, frame_id, subframe_id, slot_id, sym_id,
                                                                    p_xran_dev_ctx->fh_cfg.ru_conf.xranCompHdrType, XRAN_DIR_UL, portId,
                                                                    NULL, mu, 0, 0, 0);
                                }
                                else{
                                    retval = xran_process_tx_sym_cp_off(pHandle, ctx_id, tti, cc_id, ant_id, frame_id, subframe_id, slot_id, sym_id, 0, mu, tti, sym_id);
                                }

                            }
                        }
                        else{

                            if(p_xran_dev_ctx->tx_sym_gen_func && p_xran_dev_ctx->use_tx_sym_gen_func){
                                p_xran_dev_ctx->tx_sym_gen_func(pHandle, ctx_id, tti, cc_id, 1, ant_id, 1, frame_id, subframe_id, slot_id, sym_id,
                                                                    p_xran_dev_ctx->fh_cfg.ru_conf.xranCompHdrType, XRAN_DIR_UL, portId,
                                                                    NULL, mu, 0, 0, 0);

                            }
                            else{
                                retval = xran_process_tx_sym_cp_off(pHandle, ctx_id, tti, cc_id, ant_id, frame_id, subframe_id, slot_id, sym_id, 0, mu, tti, sym_id);
                            }

                        }

                        if(p_xran_dev_ctx->perMu[mu].enablePrach && (ant_id < num_eAxc_prach) )
                        {
                            retval = xran_process_tx_prach_cp_off(pHandle, ctx_id, tti, cc_id, ant_id, frame_id, subframe_id, slot_id, sym_id, mu, tti, sym_id);
                        }
                    }
                }

                if(xran_get_ru_category(pHandle) == XRAN_CATEGORY_B
                        && p_xran_dev_ctx->enableSrs)
                {
                    struct xran_srs_config *pSrsCfg = &(p_xran_dev_ctx->srs_cfg);

                    for(cc_id = 0; cc_id < num_CCPorts; cc_id++)
                    {
                        if(!xran_isactive_cc(p_xran_dev_ctx, cc_id))
                            continue;

                        /* check SRS NDM UP has been scheduled in non special slots */
                        /*NDM feature enables the spread of SRS packets
                        Non delay measurement SRS PDSCH PUSCH delay measure it*/
                        if(p_xran_dev_ctx->ndm_srs_scheduled
                                && p_xran_dev_ctx->ndm_srs_txtti == tti)
                        {
                            int ndm_step;
                            uint32_t srs_tti, srsFrame, srsSubframe, srsSlot;
                            uint8_t  srsCtx;

                            srs_tti     = p_xran_dev_ctx->ndm_srs_tti;
                            num_eAxAntElm = xran_get_num_ant_elm(pHandle);
                            ndm_step    = num_eAxAntElm / pSrsCfg->ndm_txduration;

                            srsSlot     = XranGetSlotNum(srs_tti, SLOTNUM_PER_SUBFRAME(interval));
                            srsSubframe = XranGetSubFrameNum(srs_tti,SLOTNUM_PER_SUBFRAME(interval),  SUBFRAMES_PER_SYSTEMFRAME);
                            srsFrame    = XranGetFrameNum(srs_tti,sfnSecStart,SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval));
                            srsFrame    = (srsFrame & 0xff);
                            srsCtx      = srs_tti % XRAN_MAX_SECTIONDB_CTX;

                            if(sym_id < pSrsCfg->ndm_txduration)
                            {
                                for(ant_id=sym_id*ndm_step; ant_id < (sym_id+1)*ndm_step; ant_id++)
                                    xran_process_tx_srs_cp_off(pHandle, srsCtx, srs_tti, cc_id, ant_id, srsFrame, srsSubframe, srsSlot);
                            }
                            else
                            {
                                p_xran_dev_ctx->ndm_srs_scheduled   = 0;
                                p_xran_dev_ctx->ndm_srs_tti         = 0;
                                p_xran_dev_ctx->ndm_srs_txtti       = 0;
                                p_xran_dev_ctx->ndm_srs_schedperiod = 0;
                            }
                        }
                        /* check special frame or uplink frame */
                        else if((xran_fs_get_slot_type(portId, cc_id, tti, XRAN_SLOT_TYPE_SP, mu) ==  1) ||
                            (xran_fs_get_slot_type(portId, cc_id, tti, XRAN_SLOT_TYPE_UL, mu) ==  1))
                        {
                            if(((tti % p_xran_dev_ctx->fh_cfg.frame_conf.nTddPeriod) == pSrsCfg->slot)
                                && (p_xran_dev_ctx->ndm_srs_scheduled == 0))
                            {
                                struct xran_prb_map *prb_map;
                                prb_map = (struct xran_prb_map *)p_xran_dev_ctx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][0].sBufferList.pBuffers->pData;

                                /* if PRB map is present in first antenna, assume SRS might be scheduled. */
                                if(prb_map && prb_map->nPrbElm)
                                {
                                    /* NDM U-Plane is not enabled */
                                    if(pSrsCfg->ndm_offset == 0)
                                    {
                                        if (prb_map->nPrbElm > 0)
                                        {
                                            if(sym_id >= prb_map->prbMap[0].nStartSymb
                                                    && sym_id < (prb_map->prbMap[0].nStartSymb + prb_map->prbMap[0].numSymb))
                                                for(ant_id=0; ant_id < xran_get_num_ant_elm(pHandle); ant_id++)
                                                    xran_process_tx_srs_cp_off(pHandle, ctx_id, tti, cc_id, ant_id, frame_id, subframe_id, slot_id);

                                        }
                                    }
                                    /* NDM U-Plane is enabled, SRS U-Planes will be transmitted after ndm_offset (in slots) */
                                    else
                                    {
                                        p_xran_dev_ctx->ndm_srs_scheduled   = 1;
                                        p_xran_dev_ctx->ndm_srs_tti         = tti;
                                        p_xran_dev_ctx->ndm_srs_txtti       = (tti + pSrsCfg->ndm_offset)%2000;
                                        p_xran_dev_ctx->ndm_srs_schedperiod = pSrsCfg->slot;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else if(xran_get_syscfg_bbuoffload() == 1)
        {
            if(p_xran_dev_ctx->tx_sym_gen_func)
            {
                enum xran_comp_hdr_type compType;
                uint8_t loc_ret = 1;
                PSECTION_DB_TYPE p_sec_db = NULL;

                compType = p_xran_dev_ctx->fh_cfg.ru_conf.xranCompHdrType;

                if(unlikely(ctx_id > XRAN_MAX_SECTIONDB_CTX))
                {
                    print_err("Invalid Context id - %d", ctx_id);
                    loc_ret = 0;
                }

                if(unlikely(num_CCPorts > XRAN_COMPONENT_CARRIERS_MAX))
                {
                    print_err("Invalid CC id - %d", num_CCPorts);
                    loc_ret = 0;
                }

                if(unlikely(num_eAxc > (XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR)))
                {
                    print_err("Invalid eAxC id - %d", num_eAxc);
                    loc_ret = 0;
                }

                p_sec_db = p_sectiondb[portId][mu];

                if (loc_ret) {
                    p_xran_dev_ctx->tx_sym_gen_func(pHandle, ctx_id, tti,
                                        0, num_CCPorts, eaxcOffset, num_eAxc, frame_id, subframe_id, slot_id, sym_id,
                                    compType, XRAN_DIR_UL, portId, p_sec_db, mu, tti, sym_id, 0);
                }
                else
                {
                    retval = 0;
                }
            }
            else
            {
                rte_panic("p_xran_dev_ctx->tx_sym_gen_func== NULL\n");
            }
        }
    }

    MLogXRANTask(taskid, t1, MLogXRANTick());
    return retval;
}

struct cp_up_tx_desc *
xran_pkt_gen_desc_alloc(void)
{
    struct rte_mbuf * mb =  rte_pktmbuf_alloc(_eth_mbuf_pkt_gen);
    struct cp_up_tx_desc * p_desc = NULL;
    char * start     = NULL;

    if(mb){
        start     = rte_pktmbuf_append(mb, sizeof(struct cp_up_tx_desc));
        if(start) {
            p_desc = rte_pktmbuf_mtod(mb,  struct cp_up_tx_desc *);
            if(p_desc){
                p_desc->mb = mb;
                return p_desc;
            }
        }
    }
    return p_desc;
}

int32_t
xran_pkt_gen_desc_free(struct cp_up_tx_desc *p_desc)
{
    if (p_desc){
        if(p_desc->mb){
            rte_pktmbuf_free(p_desc->mb);
            return 0;
        } else {
            rte_panic("p_desc->mb == NULL\n");
        }
    }
    return -1;
}

int32_t
xran_prepare_up_ul_tx_sym(uint16_t xran_port_id, uint32_t nSlotIdx,  uint32_t nCcStart, uint32_t nCcNum, uint32_t nSymMask, uint32_t nAntStart,
                            uint32_t nAntNum, uint32_t nSymStart, uint32_t nSymNum, uint8_t mu)
{
    enum    xran_in_period  inPeriod;
    struct  xran_device_ctx *p_xran_dev_ctx = NULL;
    void    *pHandle = NULL;
    int32_t retval  = 0;
    int32_t ant_id  = 0;
    int32_t cc_id   = 0;
    uint8_t ctx_id  = 0;
    uint8_t PortId  = 0;
    uint8_t num_eAxc        = 0;
    uint8_t num_eAxc_prach  = 0;
    uint8_t num_eAxAntElm   = 0;
    uint8_t num_CCPorts     = 0;
    uint32_t sym_idx_to_send    = 0;
    uint32_t numSlotMu1         = 5;
    uint32_t subframe_id        = 0;
    uint32_t frame_id   = 0;
    uint32_t tti        = 0;
    uint32_t slot_id    = 0;
    uint32_t sym_id     = 0;
    uint32_t idxSym     = 0;
    uint32_t interval   = 0;
    uint32_t sym_idx_for_ring=0, sym_id_for_ring=0;
    uint32_t tti_for_ring=0;

#if XRAN_MLOG_VAR
    uint32_t    mlogVar[15];
    uint32_t    mlogVarCnt = 0;
#endif
    unsigned long t1 = MLogXRANTick();

    p_xran_dev_ctx = xran_dev_get_ctx_by_id(xran_port_id);
    if(p_xran_dev_ctx == NULL)
        return 0;

    if(p_xran_dev_ctx->xran2phy_mem_ready == 0)
        return 0;

    if(mu == XRAN_DEFAULT_MU)
        mu = p_xran_dev_ctx->fh_cfg.mu_number[0];

    interval = xran_fs_get_tti_interval(mu);

    pHandle =  p_xran_dev_ctx;

    uint32_t xranSlot = nSlotIdx % xran_fs_get_max_slot(mu);

    for (idxSym = nSymStart; idxSym < (nSymStart + nSymNum) && idxSym < XRAN_NUM_OF_SYMBOL_PER_SLOT; idxSym++) {
        t1 = MLogXRANTick();
        if(((1 << idxSym) & nSymMask) ) {
            sym_idx_to_send = xranSlot*XRAN_NUM_OF_SYMBOL_PER_SLOT + idxSym;
            /* We are assuming that we will send in the n-1 slot wrt ota here */
            sym_idx_for_ring = sym_idx_to_send + p_xran_dev_ctx->perMu[mu].sym_up;/*XranOffsetSym(p_xran_dev_ctx->perMu[mu].sym_up, sym_idx_to_send - XRAN_NUM_OF_SYMBOL_PER_SLOT,
                                XRAN_NUM_OF_SYMBOL_PER_SLOT*SLOTNUM_PER_SUBFRAME(interval)*1000, &inPeriod);*/

            sym_idx_to_send = XranOffsetSym(0, sym_idx_to_send, XRAN_NUM_OF_SYMBOL_PER_SLOT*SLOTNUM_PER_SUBFRAME(interval)*1000, &inPeriod);

            tti         = XranGetTtiNum(sym_idx_to_send, XRAN_NUM_OF_SYMBOL_PER_SLOT);
            tti_for_ring = XranGetTtiNum(sym_idx_for_ring, XRAN_NUM_OF_SYMBOL_PER_SLOT);
            slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME(interval));
            subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME(interval),  SUBFRAMES_PER_SYSTEMFRAME);

            frame_id    =  (nSlotIdx / SLOTS_PER_SYSTEMFRAME(interval)) & 0x3FF;
            // ORAN frameId, 8 bits, [0, 255]
            frame_id = (frame_id & 0xff);
            sym_id      = XranGetSymNum(sym_idx_to_send, XRAN_NUM_OF_SYMBOL_PER_SLOT);
            sym_id_for_ring      = XranGetSymNum(sym_idx_for_ring, XRAN_NUM_OF_SYMBOL_PER_SLOT);
            ctx_id      = tti % XRAN_MAX_SECTIONDB_CTX;

            print_dbg("[%d]SFN %d sf %d slot %d\n", tti, frame_id, subframe_id, slot_id);

#if XRAN_MLOG_VAR
            mlogVarCnt = 0;
            mlogVar[mlogVarCnt++] = 0xAAAAAAAA;
            mlogVar[mlogVarCnt++] = xran_lib_ota_sym_idx[xran_port_id];
            mlogVar[mlogVarCnt++] = idxSym;
            mlogVar[mlogVarCnt++] = abs(p_xran_dev_ctx->perMu[mu].sym_up);
            mlogVar[mlogVarCnt++] = tti;
            mlogVar[mlogVarCnt++] = frame_id;
            mlogVar[mlogVarCnt++] = subframe_id;
            mlogVar[mlogVarCnt++] = slot_id;
            mlogVar[mlogVarCnt++] = sym_id;
            mlogVar[mlogVarCnt++] = xran_port_id;
            MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());
#endif

            if( (XRAN_NBIOT_MU==mu) && (XRAN_NBIOT_UL_SCS_3_75 == p_xran_dev_ctx->fh_cfg.perMu[mu].nbIotUlScs) )
            { /* for NB-IOT 3.75 UL: RU will only transmit on specific symbols
                This implementation is based on draft: SOL-2021.10.05-WG4-C.Section-Type-1-3-for-NB-IoT_v6
                This could change approved ORAN CUS specification is different */
                if(!(tti%2))
                {
                    switch(sym_id)
                    {
                    case 0:
                    case 3:
                    case 7:
                    case 11:
                        break;
                    default:
                        //printf("mu=4, returning from first switch: tti=%u, sym_id=%u\n", tti, sym_id);
                        return 0;
                    }
                }
                else
                {
                    switch(sym_id)
                    {
                    case 1:
                    case 5:
                    case 9:
                        break;
                    default:
                        //printf("mu=4, returning from second switch: tti=%u, sym_id=%u\n", tti, sym_id);
                        return 0;
                    }
                }
            }

            num_eAxc    = xran_get_num_eAxcUl(pHandle);

            num_eAxc_prach = ((num_eAxc > XRAN_MAX_PRACH_ANT_NUM)? XRAN_MAX_PRACH_ANT_NUM : num_eAxc);
            num_CCPorts = xran_get_num_cc(pHandle);
            if(first_call && p_xran_dev_ctx->enableCP){ /* U-Plane, SRS Uplink CP-enable=1 (PRACH disabled)*/

                    enum xran_comp_hdr_type compType;
                    PSECTION_DB_TYPE p_sec_db = NULL;

                    if(xran_fs_get_slot_type(PortId, cc_id, tti, XRAN_SLOT_TYPE_UL, mu) ==  1
                        || xran_fs_get_slot_type(PortId, cc_id, tti, XRAN_SLOT_TYPE_SP, mu) ==  1
                        || xran_fs_get_slot_type(PortId, cc_id, tti, XRAN_SLOT_TYPE_FDD, mu) ==  1){

                        if(xran_fs_get_symbol_type(PortId, cc_id, tti, sym_id, mu) == XRAN_SYMBOL_TYPE_UL
                            || xran_fs_get_symbol_type(PortId, cc_id, tti, sym_id, mu) == XRAN_SYMBOL_TYPE_FDD){

                            uint8_t loc_ret = 1;
                            compType = p_xran_dev_ctx->fh_cfg.ru_conf.xranCompHdrType;

                            if(unlikely(ctx_id > XRAN_MAX_SECTIONDB_CTX))
                            {
                                print_err("Invalid Context id - %d", ctx_id);
                                loc_ret = 0;
                            }

                            if(unlikely(num_CCPorts > XRAN_COMPONENT_CARRIERS_MAX))
                            {
                                print_err("Invalid CC id - %d", num_CCPorts);
                                loc_ret = 0;
                            }

                            if(unlikely(num_eAxc > (XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR)))
                            {
                                print_err("Invalid eAxC id - %d", num_eAxc);
                                loc_ret = 0;
                            }
                            p_sec_db = p_sectiondb[xran_port_id][mu];

                            if (loc_ret) {
                                xran_process_tx_sym_cp_on_opt(pHandle, ctx_id, tti,
                                            nCcStart, nCcNum, (nAntStart + p_xran_dev_ctx->perMu[mu].eaxcOffset), nAntNum, frame_id, subframe_id, slot_id, sym_id,
                                            compType, XRAN_DIR_UL, PortId, p_sec_db, mu, tti_for_ring, sym_id_for_ring, 0);
                            }
                            else
                            {
                                retval = 0;
                            }
                        }
                    }

                    if(xran_get_ru_category(pHandle) == XRAN_CATEGORY_B
                            && p_xran_dev_ctx->enableSrs
                            && nAntStart == 0) //required to avoid repeated SRS transmission with different splitgroups
                    {
                        p_sec_db = p_sectiondb[xran_port_id][mu];
                        compType = p_xran_dev_ctx->fh_cfg.ru_conf.xranCompHdrType;
                        struct xran_srs_config *pSrsCfg = &(p_xran_dev_ctx->srs_cfg);
                        struct xran_prb_map *prb_map;

                        /* check SRS NDM UP has been scheduled in slots */
                        if(p_xran_dev_ctx->ndm_srs_scheduled
                                && p_xran_dev_ctx->ndm_srs_txtti == tti)
                        {
                            prb_map = (struct xran_prb_map *)p_xran_dev_ctx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][0].sBufferList.pBuffers->pData;
                            p_sec_db = p_sectiondb[xran_port_id][mu];
                            int ndm_step;
                            uint32_t srs_tti, srsFrame, srsSubframe, srsSlot, srs_sym;
                            uint8_t  srsCtx;
                            if(prb_map && prb_map->nPrbElm)
                            {
                                srs_sym = prb_map->prbMap[0].nStartSymb;

                                srs_tti     = p_xran_dev_ctx->ndm_srs_tti;
                                num_eAxAntElm = xran_get_num_ant_elm(pHandle);
                                ndm_step    = num_eAxAntElm / pSrsCfg->ndm_txduration;

                                srsSlot     = XranGetSlotNum(srs_tti, SLOTNUM_PER_SUBFRAME(interval));
                                srsSubframe = XranGetSubFrameNum(srs_tti,SLOTNUM_PER_SUBFRAME(interval),  SUBFRAMES_PER_SYSTEMFRAME);
                                srsFrame    = XranGetFrameNum(srs_tti,xran_getSfnSecStart(),SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval));
                                srsFrame    = (srsFrame & 0xff);
                                srsCtx      = srs_tti % XRAN_MAX_SECTIONDB_CTX;

                                if(sym_id < pSrsCfg->ndm_txduration)
                                {
                                    retval = xran_process_tx_srs_cp_on(pHandle, srsCtx, srs_tti,
                                            nCcStart, nCcNum, sym_id*ndm_step, ndm_step, srsFrame, srsSubframe, srsSlot, srs_sym,
                                            compType, XRAN_DIR_UL, xran_port_id, p_sec_db, mu);
                                }
                                else
                                {
                                    p_xran_dev_ctx->ndm_srs_scheduled   = 0;
                                    p_xran_dev_ctx->ndm_srs_tti         = 0;
                                    p_xran_dev_ctx->ndm_srs_txtti       = 0;
                                    p_xran_dev_ctx->ndm_srs_schedperiod = 0;
                                }
                            }
                        }
                        /* check special frame or uplink frame*/
                        else  if((xran_fs_get_slot_type(PortId, cc_id, tti, XRAN_SLOT_TYPE_SP, mu) ==  1)
                            || (xran_fs_get_slot_type(PortId, cc_id, tti, XRAN_SLOT_TYPE_UL, mu) ==  1))
                        {
                            if(((tti % p_xran_dev_ctx->fh_cfg.frame_conf.nTddPeriod) == pSrsCfg->slot)
                                && (p_xran_dev_ctx->ndm_srs_scheduled == 0))
                            {

                                prb_map = (struct xran_prb_map *)p_xran_dev_ctx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][0].sBufferList.pBuffers->pData;
                                /* NDM U-Plane is not enabled */
                                if(pSrsCfg->ndm_offset == 0)
                                {
                                    retval = xran_process_tx_srs_cp_on(pHandle, ctx_id, tti,
                                                nCcStart, nCcNum, 0, xran_get_num_ant_elm(pHandle), frame_id, subframe_id, slot_id, sym_id,
                                                compType, XRAN_DIR_UL, xran_port_id, p_sec_db, mu);
                                }
                                /* NDM U-Plane is enabled, SRS U-Planes will be transmitted after ndm_offset (in slots) */
                                else
                                {
                                    p_xran_dev_ctx->ndm_srs_scheduled   = 1;
                                    p_xran_dev_ctx->ndm_srs_tti         = tti;
                                    p_xran_dev_ctx->ndm_srs_txtti       = (tti + pSrsCfg->ndm_offset)%2000;
                                    p_xran_dev_ctx->ndm_srs_schedperiod = pSrsCfg->slot;
                                }

                            }
                        }
                    }
            }
            else { /* U-Plane, PRACH, SRS Uplink CP-enable=0*/
                    for (ant_id = nAntStart; ant_id < (nAntStart + nAntNum); ant_id++)
                    {
                        for (cc_id = nCcStart; cc_id < (nCcStart + nCcNum); cc_id++)
                        {

                            if(p_xran_dev_ctx->puschMaskEnable)
                            {
                                if((tti % numSlotMu1) != p_xran_dev_ctx->puschMaskSlot)
                                    retval = xran_process_tx_sym_cp_off(pHandle, ctx_id, tti, cc_id, ant_id, frame_id, subframe_id, slot_id, sym_id, 0, mu, tti_for_ring, sym_id_for_ring);
                            }
                            else
                                retval = xran_process_tx_sym_cp_off(pHandle, ctx_id, tti, cc_id, ant_id, frame_id, subframe_id, slot_id, sym_id, 0, mu, tti_for_ring, sym_id_for_ring);

                            // It should be as in the initialization i.e ant_id < Nrx_antennas // Nrx_antennas = RTE_MIN(pXranConf->neAxcUl, XRAN_MAX_PRACH_ANT_NUM);
                            if(p_xran_dev_ctx->perMu[mu].enablePrach && (ant_id < num_eAxc_prach) )
                            {
                                retval = xran_process_tx_prach_cp_off(pHandle, ctx_id, tti, cc_id, ant_id, frame_id, subframe_id, slot_id, sym_id, mu, tti_for_ring, sym_id_for_ring);
                            }
                        }
                    }

                    /* SRS U-Plane, only for O-RU emulation with Cat B */
                    if(xran_get_ru_category(pHandle) == XRAN_CATEGORY_B
                            && p_xran_dev_ctx->enableSrs
                            && nAntStart == 0)  //required to avoid repeated SRS transmission with different splitgroups
                    {
                        struct xran_srs_config *pSrsCfg = &(p_xran_dev_ctx->srs_cfg);

                        for(cc_id = nCcStart; cc_id < nCcNum; cc_id++)
                        {
                            /* check SRS NDM UP has been scheduled in non special slots */
                            /*NDM feature enables the spread of SRS packets
                            Non delay measurement SRS PDSCH PUSCH delay measure it*/
                            if(p_xran_dev_ctx->ndm_srs_scheduled
                                    && p_xran_dev_ctx->ndm_srs_txtti == tti)
                            {
                                int ndm_step;
                                uint32_t srs_tti, srsFrame, srsSubframe, srsSlot;
                                uint8_t  srsCtx;

                                srs_tti     = p_xran_dev_ctx->ndm_srs_tti;
                                num_eAxAntElm = xran_get_num_ant_elm(pHandle);
                                ndm_step    = num_eAxAntElm / pSrsCfg->ndm_txduration;

                                srsSlot     = XranGetSlotNum(srs_tti, SLOTNUM_PER_SUBFRAME(interval));
                                srsSubframe = XranGetSubFrameNum(srs_tti,SLOTNUM_PER_SUBFRAME(interval),  SUBFRAMES_PER_SYSTEMFRAME);
                                srsFrame    = XranGetFrameNum(srs_tti,xran_getSfnSecStart(),SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval));
                                srsFrame    = (srsFrame & 0xff);
                                srsCtx      = srs_tti % XRAN_MAX_SECTIONDB_CTX;

                                if(sym_id < pSrsCfg->ndm_txduration)
                                {
                                    for(ant_id=sym_id*ndm_step; ant_id < (sym_id+1)*ndm_step; ant_id++)
                                        xran_process_tx_srs_cp_off(pHandle, srsCtx, srs_tti, cc_id, ant_id, srsFrame, srsSubframe, srsSlot);
                                }
                                else
                                {
                                    p_xran_dev_ctx->ndm_srs_scheduled   = 0;
                                    p_xran_dev_ctx->ndm_srs_tti         = 0;
                                    p_xran_dev_ctx->ndm_srs_txtti       = 0;
                                    p_xran_dev_ctx->ndm_srs_schedperiod = 0;
                                }
                            }

                            /* check special frame or uplink frame */
                            else if((xran_fs_get_slot_type(PortId, cc_id, tti, XRAN_SLOT_TYPE_SP, mu) ==  1) ||
                                (xran_fs_get_slot_type(PortId, cc_id, tti, XRAN_SLOT_TYPE_UL, mu) ==  1))
                            {
                                if(((tti % p_xran_dev_ctx->fh_cfg.frame_conf.nTddPeriod) == pSrsCfg->slot)
                                    && (p_xran_dev_ctx->ndm_srs_scheduled == 0))
                                {
                                    struct xran_prb_map *prb_map;
                                    prb_map = (struct xran_prb_map *)p_xran_dev_ctx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][0].sBufferList.pBuffers->pData;

                                    /* if PRB map is present in first antenna, assume SRS might be scheduled. */
                                    if(prb_map && prb_map->nPrbElm)
                                    {
                                        /* NDM U-Plane is not enabled */
                                        if(pSrsCfg->ndm_offset == 0)
                                        {
                                            if (prb_map->nPrbElm > 0)
                                            {
                                                if(sym_id >= prb_map->prbMap[0].nStartSymb
                                                        && sym_id < (prb_map->prbMap[0].nStartSymb + prb_map->prbMap[0].numSymb))
                                                    for(ant_id=0; ant_id < xran_get_num_ant_elm(pHandle); ant_id++)
                                                        xran_process_tx_srs_cp_off(pHandle, ctx_id, tti, cc_id, ant_id, frame_id, subframe_id, slot_id);
                                            }
                                        }
                                        /* NDM U-Plane is enabled, SRS U-Planes will be transmitted after ndm_offset (in slots) */
                                        else
                                        {
                                            p_xran_dev_ctx->ndm_srs_scheduled   = 1;
                                            p_xran_dev_ctx->ndm_srs_tti         = tti;
                                            p_xran_dev_ctx->ndm_srs_txtti       = (tti + pSrsCfg->ndm_offset)%2000;
                                            p_xran_dev_ctx->ndm_srs_schedperiod = pSrsCfg->slot;
                                        }
                                    }
                                }
                            }
                        }
                    }
            }
        }
        MLogXRANTask(PID_DISPATCH_TX_SYM, t1, MLogXRANTick());
    }

    return retval;
}

struct tx_sym_desc *
xran_tx_sym_gen_desc_alloc(void)
{
    struct rte_mbuf * mb =  rte_pktmbuf_alloc(_eth_mbuf_pkt_gen);
    struct tx_sym_desc * p_desc = NULL;
    char * start     = NULL;

    if(mb){
        start     = rte_pktmbuf_append(mb, sizeof(struct tx_sym_desc));
        if(start) {
            p_desc = rte_pktmbuf_mtod(mb,  struct tx_sym_desc *);
            if(p_desc){
                p_desc->mb = mb;
                return p_desc;
            }
        }
    }
    return p_desc;
}

int32_t
xran_tx_sym_gen_desc_free(struct tx_sym_desc *p_desc)
{
    if (p_desc){
        if(p_desc->mb){
            rte_pktmbuf_free(p_desc->mb);
            return 0;
        } else {
            rte_panic("p_desc->mb == NULL\n");
        }
    }
    return -1;
}
