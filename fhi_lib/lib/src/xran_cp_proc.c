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
#include "xran_cp_proc.h"
#include "xran_tx_proc.h"

#include "xran_mlog_lnx.h"

uint8_t xran_cp_seq_id_num[XRAN_PORTS_NUM][XRAN_MAX_CELLS_PER_PORT][XRAN_DIR_MAX][XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR]; /* XRAN_MAX_ANTENNA_NR * 2 for PUSCH and PRACH */
uint8_t xran_updl_seq_id_num[XRAN_PORTS_NUM][XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR];
uint8_t xran_upul_seq_id_num[XRAN_PORTS_NUM][XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR]; /**< PUSCH, PRACH, SRS for Cat B */
uint8_t xran_section_id_curslot[XRAN_PORTS_NUM][XRAN_DIR_MAX][XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR * 2+ XRAN_MAX_ANT_ARRAY_ELM_NR];
uint16_t xran_section_id[XRAN_PORTS_NUM][XRAN_DIR_MAX][XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR * 2+ XRAN_MAX_ANT_ARRAY_ELM_NR];

struct xran_recv_packet_info parse_recv[XRAN_PORTS_NUM];

//////////////////////////////////////////
// For RU emulation
struct xran_section_recv_info *recvSections[XRAN_PORTS_NUM] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};
struct xran_cp_recv_params recvCpInfo[XRAN_PORTS_NUM];

extern int32_t first_call;

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

int32_t
xran_init_seqid(void *pHandle)
{
    int cell, dir, ant;
    int8_t xran_port = 0;
    if((xran_port =  xran_dev_ctx_get_port_id(pHandle)) < 0 ){
        print_err("Invalid pHandle - %p", pHandle);
        return (0);
    }


    for(cell=0; cell < XRAN_MAX_CELLS_PER_PORT; cell++) {
        for(dir=0; dir < XRAN_DIR_MAX; dir++) {
            for(ant=0; ant < XRAN_MAX_ANTENNA_NR * 2; ant++)
                xran_cp_seq_id_num[xran_port][cell][dir][ant] = 0;
            }
        for(ant=0; ant < XRAN_MAX_ANTENNA_NR; ant++)
                xran_updl_seq_id_num[xran_port][cell][ant] = 0;
        for(ant=0; ant < XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR; ant++)
                xran_upul_seq_id_num[xran_port][cell][ant] = 0;
        }

    return (0);
}

int32_t
process_cplane(struct rte_mbuf *pkt, void* handle)
{
    uint32_t mb_free = MBUF_FREE;
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)handle;

    if(p_xran_dev_ctx && xran_if_current_state == XRAN_RUNNING) {
        if(xran_dev_get_ctx_by_id(0)->fh_cfg.debugStop) /* check CP with standard tests only */
            xran_parse_cp_pkt(pkt, &recvCpInfo[p_xran_dev_ctx->xran_port_id], &parse_recv[p_xran_dev_ctx->xran_port_id],(void*)p_xran_dev_ctx, &mb_free);
    }
    return (mb_free);
}

int32_t
xran_check_symbolrange(int symbol_type, uint32_t PortId, int cc_id, int tti,
                        int start_sym, int numsym_in, int *numsym_out)
{
    int i;
    int first_pos, last_pos;
    int start_pos, end_pos;

    first_pos = last_pos = -1;

    /* Find first symbol which is same with given symbol type */
    for(i=0; i < XRAN_NUM_OF_SYMBOL_PER_SLOT; i++)
        if(xran_fs_get_symbol_type(PortId, cc_id, tti, i) == symbol_type) {
            first_pos = i; break;
            }

    if(first_pos < 0) {
//        for(i=0; i < XRAN_NUM_OF_SYMBOL_PER_SLOT; i++)
//            printf("symbol_type %d - %d:%d\n", symbol_type, i, xran_fs_get_symbol_type(cc_id, tti, i));
        *numsym_out = 0;
        return (first_pos);
        }

    /* Find the rest of consecutive symbols which are same with given symbol type */
    for( ; i < XRAN_NUM_OF_SYMBOL_PER_SLOT; i++)
        if(xran_fs_get_symbol_type(PortId, cc_id, tti, i) != symbol_type)
            break;
    last_pos = i;

    start_pos = (first_pos > start_sym) ?  first_pos : start_sym;
    end_pos = ((start_sym + numsym_in) > last_pos) ? last_pos : (start_sym + numsym_in);
    *numsym_out = end_pos - start_pos;

    return (start_pos);
}

struct rte_mbuf *
xran_attach_cp_ext_buf(uint16_t vf_id, int8_t* p_ext_buff_start/*ext_start*/, int8_t* p_ext_buff/*ext-section*/, uint16_t ext_buff_len,
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

/* TO DO: __thread is slow. We should allocate global 2D array and index it using current core index
 * for better performance.
 */
__thread struct xran_section_gen_info sect_geninfo[XRAN_MAX_SECTIONS_PER_SLOT];

int32_t
xran_cp_create_and_send_section(void *pHandle, uint8_t ru_port_id, int dir, int tti, int cc_id,
        struct xran_prb_map *prbMap, struct xran_prb_elm_proc_info_t *prbElmProcInfo, enum xran_category category,  uint8_t ctx_id)
{
    int32_t ret = 0;
    struct xran_device_ctx *p_x_ctx   = (struct xran_device_ctx *)pHandle;
    struct xran_common_counters *pCnt = &p_x_ctx->fh_counters;
    struct xran_cp_gen_params params;
    struct rte_mbuf *mbuf;
    uint32_t interval = p_x_ctx->interval_us_local;
    uint8_t PortId = p_x_ctx->xran_port_id;
    int16_t numCPSections=0, ext_offset=0, start_sect_id=0;

    uint32_t i, j, loc_sym,idx;
    uint32_t nsection = 0;
    struct xran_prb_elm *pPrbMapElem = NULL;
    // struct xran_prb_elm *pPrbMapElemPrev = NULL;
    uint32_t slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME(interval));
    uint32_t subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME(interval),  SUBFRAMES_PER_SYSTEMFRAME);
    uint32_t frame_id    = XranGetFrameNum(tti,xran_getSfnSecStart(),SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval));

    uint8_t seq_id = 0;
    uint16_t vf_id = 0 , curr_sec_id = 0 , prb_per_section, start_Prb;
    int32_t startSym = 0, numSyms = 0;

    int next=0;
    struct xran_sectionext1_info ext1;
    struct xran_sectionext4_info ext4 = {0};
    struct xran_sectionext9_info ext9;
    struct xran_sectionext11_info ext11;

    frame_id = (frame_id & 0xff); /* ORAN frameId, 8 bits, [0, 255] */

    if(unlikely((category != XRAN_CATEGORY_A) && (category != XRAN_CATEGORY_B)))
    {
        print_err("Unsupported Category %d\n", category);
        return (-1);
    }

    /* Generate a C-Plane message per each section,
     * not a C-Plane message with multi sections */
    if(0 == p_x_ctx->RunSlotPrbMapBySymbolEnable)
    {
    if(prbMap) {

            nsection = prbMap->nPrbElm;
            i=0;
            if(XRAN_DIR_DL == dir)
            {
                if(0 == p_x_ctx->numSymsForDlCP)
                {
                    print_dbg("No symbol available for DL CP transmission\n");
                    return (-1);
                }

                if(prbMap->nPrbElm == prbElmProcInfo->nPrbElmProcessed && 0 != prbElmProcInfo->numSymsRemaining)
                {
                    prbElmProcInfo->numSymsRemaining--;
                    print_dbg("All sections already processed\n");
                    return (-1);
                }

                if(0== prbElmProcInfo->numSymsRemaining)
                { /* new slot */
                    prbElmProcInfo->numSymsRemaining = p_x_ctx->numSymsForDlCP;
                    prbElmProcInfo->nPrbElmPerSym = prbMap->nPrbElm/p_x_ctx->numSymsForDlCP;
                    prbElmProcInfo->nPrbElmProcessed = 0;
                }

                if(1 == prbElmProcInfo->numSymsRemaining)
                {/* last symbol:: send all remaining */
        nsection = prbMap->nPrbElm;
                }
                else
                {
                    if(0 == prbElmProcInfo->nPrbElmPerSym)
                        nsection=prbElmProcInfo->nPrbElmProcessed + 1;
                    else
                        nsection = prbElmProcInfo->nPrbElmProcessed + prbElmProcInfo->nPrbElmPerSym;
                }

                i=prbElmProcInfo->nPrbElmProcessed;
                prbElmProcInfo->numSymsRemaining--;

            } //dir = DL
            else
            {
                nsection = prbMap->nPrbElm;
                i=0;
            } //dir = UL

        pPrbMapElem = &prbMap->prbMap[0];
        }
        else
        {
        print_err("prbMap is NULL\n");
        return (-1);
    }


        curr_sec_id = 0;
        if(pPrbMapElem->bf_weight.extType == 1)
        {
            for(j=0;j<i;j++)
                curr_sec_id += prbMap->prbMap[j].bf_weight.numSetBFWs;
        }
        else
            curr_sec_id = i;

        // start_id=curr_sec_id;
        uint8_t generateCpPkt=0;
        uint8_t replacePrbStartNSize=0; /* In case of application fragmentation, we send 1 cplane packets for multiple
                                           uplane packets i.e. 1 cp packet for multiple PRBs. This flag is used to
                                           achieve that by setting different values for cp packet preparation and for
                                           cp-up database update */

    /* Generate a C-Plane message per each section,
     * not a C-Plane message with multi sections */
        for (; i < nsection; i++) {
        int startSym, numSyms;

        pPrbMapElem                 = &prbMap->prbMap[i];
            prb_per_section = pPrbMapElem->bf_weight.numBundPrb;
            start_Prb       = pPrbMapElem->nRBStart;

            if((pPrbMapElem->bf_weight.extType == 1) &&
                    (((i+1)<nsection && prbMap->prbMap[i+1].IsNewSect==1) ||
                    (i+1) == nsection))
            { /*ext1*/
                generateCpPkt=1;
            }
            else if(pPrbMapElem->IsNewSect)
                generateCpPkt=1;
            else
                generateCpPkt=0;


       /* For Special Subframe,
        * Check validity of given symbol range with slot configuration
        * and adjust symbol range accordingly. */
        if(xran_fs_get_slot_type(PortId, cc_id, tti, XRAN_SLOT_TYPE_FDD) != 1
                && xran_fs_get_slot_type(PortId, cc_id, tti, XRAN_SLOT_TYPE_SP) == 1)
            {
            /* This function cannot handle two or more groups of consecutive same type of symbol.
                * If there are two or more, then it might cause an error */
            startSym = xran_check_symbolrange(
                                ((dir==XRAN_DIR_DL)?XRAN_SYMBOL_TYPE_DL:XRAN_SYMBOL_TYPE_UL),
                                PortId, cc_id, tti,
                                pPrbMapElem->nStartSymb,
                                pPrbMapElem->numSymb, &numSyms);
                if(startSym < 0 || numSyms == 0)
                {
                /* if start symbol is not valid, then skip this section */
                print_err("Skip section %d due to invalid symbol range - [%d:%d], [%d:%d]",
                            i,
                            pPrbMapElem->nStartSymb, pPrbMapElem->numSymb,
                            startSym, numSyms);
                continue;
            }
            }
            else
            {
            startSym    = pPrbMapElem->nStartSymb;
            numSyms     = pPrbMapElem->numSymb;
        }

        vf_id  = xran_map_ecpriRtcid_to_vf(p_x_ctx, dir, cc_id, ru_port_id);
        params.dir                  = dir;
        params.sectionType          = XRAN_CP_SECTIONTYPE_1;
        params.hdr.filterIdx        = XRAN_FILTERINDEX_STANDARD;
        params.hdr.frameId          = frame_id;
        params.hdr.subframeId       = subframe_id;
        params.hdr.slotId           = slot_id;
        params.hdr.startSymId       = startSym;
        params.hdr.iqWidth          = pPrbMapElem->iqWidth;
        params.hdr.compMeth         = pPrbMapElem->compMethod;

        print_dbg("cp[%d:%d:%d] ru_port_id %d dir=%d\n",
                               frame_id, subframe_id, slot_id, ru_port_id, dir);

            if(pPrbMapElem->bf_weight.extType == 1)
            {
                /* Send multiple CP sections per prbElement for ext-1 */
                numCPSections   = pPrbMapElem->bf_weight.numSetBFWs;
            }
            else
            {
                numCPSections   = 1;
                replacePrbStartNSize = 1;   /* in case of no app fragmentation, UP_nRBSize will be same as nRBSize. So,
                                                always replacing the elements when ext1 is not in use */
            }

            /** Prepare section info for multiple sections in a PRB element */
            for(idx=0; idx < numCPSections; idx++) {

                sect_geninfo[curr_sec_id].exDataSize=0;
                sect_geninfo[curr_sec_id].info = xran_cp_get_section_info_ptr(pHandle, dir, cc_id, ru_port_id, ctx_id);
                if(unlikely(sect_geninfo[curr_sec_id].info == NULL))
                {
                    rte_panic("xran_cp_get_section_info_ptr failed\n");
                    }

                struct xran_section_info *info = sect_geninfo[curr_sec_id].info;
                info->prbElemBegin  = (idx == 0 ) ?  1 : 0;
                info->prbElemEnd    = (idx + 1 == numCPSections) ?  1 : 0;
                info->ef            = 0;
                info->freqOffset    = 0;
                info->ueId          = 0;
                info->regFactor     = 0;

                if((idx+1)*prb_per_section > pPrbMapElem->nRBSize){
                    prb_per_section = pPrbMapElem->nRBSize - idx*prb_per_section;
                }

                if(numCPSections == 1)
                {
                    info->startPrbc = pPrbMapElem->nRBStart;
                    info->numPrbc   = pPrbMapElem->nRBSize;
                }
                else
                {
                    info->startPrbc = start_Prb;
                    info->numPrbc   = prb_per_section;
                    start_Prb       += prb_per_section;
            }

                info->type        = params.sectionType;
                info->startSymId  = params.hdr.startSymId;
                info->iqWidth     = params.hdr.iqWidth;
                info->compMeth    = params.hdr.compMeth;
                info->id          = curr_sec_id;

                if(info->prbElemBegin && pPrbMapElem->IsNewSect==1)
                {
                    start_sect_id = info->id;
        }

                if(unlikely(info->id > XRAN_MAX_SECTIONS_PER_SLOT))
                    print_err("sectinfo->id %d\n", info->id);

                info->rb          = XRAN_RBIND_EVERY;
                info->numSymbol   = numSyms;
                info->reMask      = 0xfff;
                info->beamId      = pPrbMapElem->nBeamIndex;
                info->symInc      = XRAN_SYMBOLNUMBER_NOTINC;

                for(loc_sym = 0; loc_sym < XRAN_NUM_OF_SYMBOL_PER_SLOT; loc_sym++)
                {
                    struct xran_section_desc *p_sec_desc =  &pPrbMapElem->sec_desc[loc_sym][0];

                    if(p_sec_desc)
                    {
                        info->sec_desc[loc_sym].iq_buffer_offset = p_sec_desc->iq_buffer_offset;
                        info->sec_desc[loc_sym].iq_buffer_len    = p_sec_desc->iq_buffer_len;

                        p_sec_desc->section_id   = info->id;
            }
                    else
                    {
                        print_err("section desc is NULL\n");
        }

                } /* for(loc_sym = 0; loc_sym < XRAN_NUM_OF_SYMBOL_PER_SLOT; loc_sym++) */

        /* Add extentions if required */
                if((category == XRAN_CATEGORY_B) && (pPrbMapElem->bf_weight_update))
                {
                    if(pPrbMapElem->bf_weight.extType == 1) /* Prepare section data for ext-1 */
                    {
        next = 0;
                        sect_geninfo[curr_sec_id].exDataSize  = 0;

                        memset(&ext1, 0, sizeof (struct xran_sectionext1_info));
                        ext1.bfwNumber      = pPrbMapElem->bf_weight.nAntElmTRx;
                        ext1.bfwIqWidth     = pPrbMapElem->iqWidth;
                        ext1.bfwCompMeth    = pPrbMapElem->compMethod;
                        /* ext-1 buffer contains CP sections */
                        ext1.bfwIQ_sz       = ONE_EXT_LEN(pPrbMapElem); //76

                        ext_offset          = (idx*ONE_CPSEC_EXT_LEN(pPrbMapElem)) + sizeof(struct xran_cp_radioapp_section1);
                        ext1.p_bfwIQ        = (int8_t*)(pPrbMapElem->bf_weight.p_ext_section + ext_offset);

                        sect_geninfo[curr_sec_id].exData[next].type   = XRAN_CP_SECTIONEXTCMD_1;
                        sect_geninfo[curr_sec_id].exData[next].len    = sizeof(ext1);
                        sect_geninfo[curr_sec_id].exData[next].data   = &ext1;

                        info->ef = 1;
                        sect_geninfo[curr_sec_id].exDataSize++;
                        next++;
                    }
                    else
                    {
                        /*ext-11*/
                    }

                } /* if((category == XRAN_CATEGORY_B) && (pPrbMapElem->bf_weight_update)) */

                curr_sec_id++;
            } /* for(idx=0; idx < numCPSections;idx++) */

            if (dir==XRAN_DIR_UL || generateCpPkt) //only send actual new CP section
            {
        /* Extension 4 for modulation compression */
                if(pPrbMapElem->compMethod == XRAN_COMPMETHOD_MODULATION)
                {
            mbuf = xran_ethdi_mbuf_alloc();

            ext4.csf                            = 0;  //no shift for now only
            ext4.modCompScaler                  = pPrbMapElem->ScaleFactor;
                    /* TO DO: Should this be the current section id? */
            sect_geninfo[0].exData[next].type   = XRAN_CP_SECTIONEXTCMD_4;
            sect_geninfo[0].exData[next].len    = sizeof(ext4);
            sect_geninfo[0].exData[next].data   = &ext4;

                    sect_geninfo[0].info->ef             = 1;
            sect_geninfo[0].exDataSize++;
            next++;
        }

        /* Extension 1 or 11 for Beam forming weights */
                /* add section extention for BF Weights if update is needed */
                if((category == XRAN_CATEGORY_B) && (pPrbMapElem->bf_weight_update))
                {
                    if(pPrbMapElem->bf_weight.extType == 1) /* Using Extension 1 */
                    {
                        //TODO: Should this change ?
                        struct rte_mbuf_ext_shared_info * p_share_data =
                        &p_x_ctx->cp_share_data.sh_data[tti % XRAN_N_FE_BUF_LEN][cc_id][ru_port_id][sect_geninfo[0].info->id];

                        if(pPrbMapElem->bf_weight.p_ext_start)
                        {
                    /* use buffer with BF Weights for mbuf */
                    mbuf = xran_attach_cp_ext_buf(vf_id, pPrbMapElem->bf_weight.p_ext_start,
                                                pPrbMapElem->bf_weight.p_ext_section,
                                                pPrbMapElem->bf_weight.ext_section_sz, p_share_data);
                        }
                        else
                        {
                    print_err("p %d cc %d dir %d Alloc fail!\n", PortId, cc_id, dir);
                            ret=-1;
                            goto _create_and_send_section_error;
                }
                    } /* if(pPrbMapElem->bf_weight.extType == 1) */
                    else
                    {
                /* Using Extension 11 */
                struct rte_mbuf_ext_shared_info *shared_info;
                        next = 0;

                        shared_info = &p_x_ctx->bfw_share_data.sh_data[tti % XRAN_N_FE_BUF_LEN][cc_id][ru_port_id][sect_geninfo[0].info->id];
                shared_info->free_cb     = NULL;
                shared_info->fcb_opaque  = NULL;

                mbuf = xran_ethdi_mbuf_indir_alloc();
                if(unlikely(mbuf == NULL)) {
                    rte_panic("Alloc fail!\n");
                }
                //mbuf = rte_pktmbuf_alloc(_eth_mbuf_pool_vf_small[vf_id]);
                        if(xran_cp_attach_ext_buf(mbuf, (uint8_t *)pPrbMapElem->bf_weight.p_ext_start, pPrbMapElem->bf_weight.maxExtBufSize, shared_info) < 0)
                        {
                    rte_pktmbuf_free(mbuf);
                            ret=-1;
                            goto _create_and_send_section_error;
                }

                rte_mbuf_ext_refcnt_update(shared_info, 0);

                ext11.RAD           = pPrbMapElem->bf_weight.RAD;
                ext11.disableBFWs   = pPrbMapElem->bf_weight.disableBFWs;

                ext11.numBundPrb    = pPrbMapElem->bf_weight.numBundPrb;
                ext11.numSetBFWs    = pPrbMapElem->bf_weight.numSetBFWs;

                ext11.bfwCompMeth   = pPrbMapElem->bf_weight.bfwCompMeth;
                ext11.bfwIqWidth    = pPrbMapElem->bf_weight.bfwIqWidth;

                ext11.maxExtBufSize = pPrbMapElem->bf_weight.maxExtBufSize;
                ext11.pExtBufShinfo = shared_info;

                ext11.pExtBuf       = (uint8_t *)pPrbMapElem->bf_weight.p_ext_start;
                ext11.totalBfwIQLen = pPrbMapElem->bf_weight.ext_section_sz;

                sect_geninfo[0].exData[next].type   = XRAN_CP_SECTIONEXTCMD_11;
                sect_geninfo[0].exData[next].len    = sizeof(ext11);
                sect_geninfo[0].exData[next].data   = &ext11;

                        sect_geninfo[0].info->ef       = 1;
                sect_geninfo[0].exDataSize++;
                next++;
            }
                } /* if((category == XRAN_CATEGORY_B) && (pPrbMapElem->bf_weight_update)) */
                else
                {
            mbuf = xran_ethdi_mbuf_alloc();

                    sect_geninfo[0].info->ef          = 0;
            sect_geninfo[0].exDataSize       = 0;

                    if(p_x_ctx->dssEnable == 1) {
                        uint8_t dssSlot = 0;
                        dssSlot = tti % (p_x_ctx->dssPeriod);

                        ext9.technology = p_x_ctx->technology[dssSlot];
                        ext9.reserved = 0;

                        sect_geninfo[0].exData[next].type   = XRAN_CP_SECTIONEXTCMD_9;
                        sect_geninfo[0].exData[next].len    = sizeof(ext9);
                        sect_geninfo[0].exData[next].data   = &ext9;

                        sect_geninfo[0].info->ef       = 1;
                        sect_geninfo[0].exDataSize++;
                        next++;
                    }
        }

                if(unlikely(mbuf == NULL))
                {
            print_err("Alloc fail!\n");
                    ret=-1;
                    goto _create_and_send_section_error;
        }

                params.numSections          = numCPSections;
        params.sections             = sect_geninfo;

                seq_id = xran_get_cp_seqid(pHandle, ((XRAN_DIR_DL == dir)? XRAN_DIR_DL : XRAN_DIR_UL), cc_id, ru_port_id);
                ret = xran_prepare_ctrl_pkt(mbuf, &params, cc_id, ru_port_id, seq_id,start_sect_id);
            } /* if (dir==XRAN_DIR_UL || generateCpPkt) */

            if(replacePrbStartNSize && XRAN_DIR_DL == dir)
            {
                sect_geninfo[curr_sec_id-1].info->startPrbc = pPrbMapElem->UP_nRBStart;
                sect_geninfo[curr_sec_id-1].info->numPrbc   = pPrbMapElem->UP_nRBSize;
            }

            if(ret < 0)
            {
            print_err("Fail to build control plane packet - [%d:%d:%d] dir=%d\n",
                        frame_id, subframe_id, slot_id, dir);
            }
            else
            {
                if((dir==XRAN_DIR_UL) || generateCpPkt) //only send actual new CP section
                {
            int32_t cp_sent = 0;
            int32_t pkt_len = 0;
            /* add in the ethernet header */
            struct rte_ether_hdr *const h = (void *)rte_pktmbuf_prepend(mbuf, sizeof(*h));
            pkt_len = rte_pktmbuf_pkt_len(mbuf);
            pCnt->tx_counter++;
            pCnt->tx_bytes_counter += pkt_len; //rte_pktmbuf_pkt_len(mbuf);
            if(pkt_len > p_x_ctx->fh_init.mtu)
                rte_panic("section %d: pkt_len = %d maxExtBufSize %d\n", i, pkt_len, pPrbMapElem->bf_weight.maxExtBufSize);

            cp_sent = p_x_ctx->send_cpmbuf2ring(mbuf, ETHER_TYPE_ECPRI, vf_id);
                    if(cp_sent != 1)
                    {
                rte_pktmbuf_free(mbuf);
            }
                }
        }
    } /* for (i=0; i<nsection; i++) */
    }
#if 1
    else
    {
        /* Generate a C-Plane message with multi sections,
        * a C-Plane message for each section*/
        if(prbMap)
        {
            if(0 == prbMap->nPrbElm)
            {
                print_dbg("prbMap->nPrbElm is %d\n",prbMap->nPrbElm);
                return 0;
            }

            nsection = prbMap->nPrbElm;
            i=0;
            if(XRAN_DIR_DL == dir)
            {
                prbElmProcInfo->numSymsRemaining = 0;
                prbElmProcInfo->nPrbElmProcessed = 0;
                prbElmProcInfo->nPrbElmPerSym = prbMap->nPrbElm;
                nsection = prbMap->nPrbElm;
            } //dir = DL
            else
            {
                nsection = prbMap->nPrbElm;
            } //dir = UL
        }
        else
        {
            print_err("prbMap is NULL\n");
            return (-1);
        }

        pPrbMapElem = &prbMap->prbMap[0];

        if(xran_fs_get_slot_type(PortId, cc_id, tti, XRAN_SLOT_TYPE_FDD) != 1
            && xran_fs_get_slot_type(PortId, cc_id, tti, XRAN_SLOT_TYPE_SP) == 1)
        {
            startSym = xran_check_symbolrange(
                                ((dir==XRAN_DIR_DL)?XRAN_SYMBOL_TYPE_DL:XRAN_SYMBOL_TYPE_UL),
                                PortId, cc_id, tti,
                                pPrbMapElem->nStartSymb,
                                pPrbMapElem->numSymb, &numSyms);

            if(startSym < 0 || numSyms == 0)
            {
                /* if start symbol is not valid, then skip this section */
                print_err("Skip section %d due to invalid symbol range - [%d:%d], [%d:%d]",
                            i,
                            pPrbMapElem->nStartSymb, pPrbMapElem->numSymb,
                            startSym, numSyms);
            }
        }
        else
        {
            startSym    = pPrbMapElem->nStartSymb;
            numSyms     = pPrbMapElem->numSymb;
        }

        vf_id  = xran_map_ecpriRtcid_to_vf(p_x_ctx, dir, cc_id, ru_port_id);
        params.dir                  = dir;
        params.sectionType          = XRAN_CP_SECTIONTYPE_1;
        params.hdr.filterIdx        = XRAN_FILTERINDEX_STANDARD;
        params.hdr.frameId          = frame_id;
        params.hdr.subframeId       = subframe_id;
        params.hdr.slotId           = slot_id;
        params.hdr.startSymId       = startSym;
        params.hdr.iqWidth          = pPrbMapElem->iqWidth;
        params.hdr.compMeth         = pPrbMapElem->compMethod;
        params.sections             = sect_geninfo;

        for (i = 0, j = 0; j < nsection; j++)
        {
            sect_geninfo[i].exDataSize=0;
            sect_geninfo[i].info = xran_cp_get_section_info_ptr(pHandle, dir, cc_id, ru_port_id, ctx_id);
            sect_geninfo[i].info->prbElemBegin = ((j == 0 ) ?  1 : 0);
            sect_geninfo[i].info->prbElemEnd   = ((j + 1 == nsection) ?  1 : 0);
            if(sect_geninfo[i].info == NULL)
            {
                rte_panic("xran_cp_get_section_info_ptr failed\n");
            }
            pPrbMapElem = &prbMap->prbMap[j];

            sect_geninfo[i].info->type        = XRAN_CP_SECTIONTYPE_1;
            sect_geninfo[i].info->startSymId  = pPrbMapElem->nStartSymb;
            sect_geninfo[i].info->iqWidth     = params.hdr.iqWidth;
            sect_geninfo[i].info->compMeth    = params.hdr.compMeth;
            sect_geninfo[i].info->id          = pPrbMapElem->nSectId;

            if(sect_geninfo[i].info->id > XRAN_MAX_SECTIONS_PER_SLOT)
                print_err("sectinfo->id %d\n", sect_geninfo[i].info->id);

            sect_geninfo[i].info->rb          = XRAN_RBIND_EVERY;
            sect_geninfo[i].info->startPrbc   = pPrbMapElem->UP_nRBStart;
            sect_geninfo[i].info->numPrbc     = pPrbMapElem->UP_nRBSize;
            sect_geninfo[i].info->numSymbol   = pPrbMapElem->numSymb;
            sect_geninfo[i].info->reMask      = 0xfff;
            sect_geninfo[i].info->beamId      = pPrbMapElem->nBeamIndex;

            if(startSym == pPrbMapElem->nStartSymb)
                sect_geninfo[i].info->symInc  = XRAN_SYMBOLNUMBER_NOTINC;
            else
            {
                if((startSym + numSyms) == pPrbMapElem->nStartSymb)
                {
                    sect_geninfo[i].info->symInc  = XRAN_SYMBOLNUMBER_INC;
                    startSym  =   pPrbMapElem->nStartSymb;
                    numSyms   =   pPrbMapElem->numSymb;
                }
                else
                {
                    sect_geninfo[i].info->startSymId = startSym;
                    sect_geninfo[i].info->numSymbol  = numSyms;
                    print_dbg("Last startSym is %d. Last numSyms is %d. But current pPrbMapElem->nStartSymb is %d.\n", startSym, numSyms, pPrbMapElem->nStartSymb);
                }
            }


            for(loc_sym = 0; loc_sym < XRAN_NUM_OF_SYMBOL_PER_SLOT; loc_sym++)
            {
                struct xran_section_desc *p_sec_desc =  &pPrbMapElem->sec_desc[loc_sym][0];
                if(p_sec_desc)
                {
                    p_sec_desc->section_id   = sect_geninfo[i].info->id;

                    sect_geninfo[i].info->sec_desc[loc_sym].iq_buffer_offset = p_sec_desc->iq_buffer_offset;
                    sect_geninfo[i].info->sec_desc[loc_sym].iq_buffer_len    = p_sec_desc->iq_buffer_len;
                }
                else
                {
                    print_err("section desc is NULL\n");
                }
            }

            next = 0;
            sect_geninfo[i].exDataSize       = 0;

          /* Extension 4 for modulation compression */
            if(pPrbMapElem->compMethod == XRAN_COMPMETHOD_MODULATION)
            {
                // print_dbg("[%s]:%d Modulation Compression need to verify for this code branch and may not be available\n");
                print_err("[%s]:%d Modulation Compression need to verify for this code branch and may not be available\n",__FUNCTION__, __LINE__);
            }
            /* Extension 1 or 11 for Beam forming weights */
            /* add section extention for BF Weights if update is needed */
            if((category == XRAN_CATEGORY_B) && (pPrbMapElem->bf_weight_update))
            {
                // print_dbg("[%s]:%d Category B need to verify for this code branch and may not be available\n");
                print_err("[%s]:%d Category B need to verify for this code branch and may not be available\n",__FUNCTION__, __LINE__);
            } /* if((category == XRAN_CATEGORY_B) && (pPrbMapElem->bf_weight_update)) */
            else
            {
                sect_geninfo[i].info->ef          = 0;
                sect_geninfo[i].exDataSize       = 0;

                if(p_x_ctx->dssEnable == 1) {
                    uint8_t dssSlot = 0;
                    dssSlot = tti % (p_x_ctx->dssPeriod);

                    ext9.technology = p_x_ctx->technology[dssSlot];
                    ext9.reserved = 0;

                    sect_geninfo[i].exData[next].type   = XRAN_CP_SECTIONEXTCMD_9;
                    sect_geninfo[i].exData[next].len    = sizeof(ext9);
                    sect_geninfo[i].exData[next].data   = &ext9;

                    sect_geninfo[i].info->ef       = 1;
                    sect_geninfo[i].exDataSize++;
                    next++;
                }
            }

            // xran_cp_add_section_info(pHandle, dir, cc_id, ru_port_id, ctx_id, &sect_geninfo[i].info);

            if(pPrbMapElem->IsNewSect == 1)
            {
                sect_geninfo[i].info->startPrbc   = pPrbMapElem->nRBStart;
                sect_geninfo[i].info->numPrbc     = pPrbMapElem->nRBSize;
                i++;
            }
        }

        params.numSections          = i;

        mbuf = xran_ethdi_mbuf_alloc();
        if(unlikely(mbuf == NULL))
        {
            print_err("Alloc fail!\n");
            ret=-1;
            goto _create_and_send_section_error;
        }

        seq_id = xran_get_cp_seqid(pHandle, ((XRAN_DIR_DL == dir)? XRAN_DIR_DL : XRAN_DIR_UL), cc_id, ru_port_id);
        ret = xran_prepare_ctrl_pkt(mbuf, &params, cc_id, ru_port_id, seq_id,start_sect_id);

        if(ret < 0)
        {
            print_err("Fail to build control plane packet - [%d:%d:%d] dir=%d\n",
                        frame_id, subframe_id, slot_id, dir);
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
            if(pkt_len > p_x_ctx->fh_init.mtu)
                rte_panic("section %d: pkt_len = %d maxExtBufSize %d\n", i, pkt_len, pPrbMapElem->bf_weight.maxExtBufSize);

            cp_sent = p_x_ctx->send_cpmbuf2ring(mbuf, ETHER_TYPE_ECPRI, vf_id);
            if(cp_sent != 1)
            {
                rte_pktmbuf_free(mbuf);
            }
        }

        struct xran_section_info *info;
        for (j = 0; j < nsection; j++)
        {
            pPrbMapElem = &prbMap->prbMap[j];
            info = xran_cp_find_section_info(pHandle, dir, cc_id, ru_port_id, ctx_id,j);
            if(info == NULL)
            {
                rte_panic("xran_cp_get_section_info_ptr failed\n");
            }
            info->startPrbc   = pPrbMapElem->UP_nRBStart;
            info->numPrbc     = pPrbMapElem->UP_nRBSize;
        }
    }
#endif
_create_and_send_section_error:
    if(XRAN_DIR_DL == dir)
    {
        prbElmProcInfo->nPrbElmProcessed = nsection;
    }

    return ret;
}

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

int32_t
xran_ruemul_release(void *pHandle)
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

    if(xran_port_id < XRAN_PORTS_NUM){
        if(recvSections[xran_port_id]) {
            free(recvSections[xran_port_id]);
            recvCpInfo[xran_port_id].sections = NULL;
        }
    } else {
        print_err("Incorrect xran port %d\n", xran_port_id);
        return (-1);
    }

    return (0);
}
