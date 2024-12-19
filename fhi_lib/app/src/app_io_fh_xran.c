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
 * @brief This module provides interface implementation to ORAN FH from Application side
 * @file app_iof_fh_xran.c
 * @ingroup xran
 * @author Intel Corporation
 *
 **/

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <immintrin.h>
#include "common.h"
#include "config.h"
#include "xran_mlog_lnx.h"

#include "xran_fh_o_du.h"
#include "xran_fh_o_ru.h"
#include "xran_compression.h"
#include "xran_cp_api.h"
#include "xran_sync_api.h"
#include "xran_mlog_task_id.h"
#include "app_io_fh_xran.h"
#include <rte_memory.h>
#include <rte_mbuf.h>
#ifdef FWK_ENABLED
#include "app_bbu_pool.h"
#endif
/* buffers size */
uint32_t    nFpgaToSW_FTH_RxBufferLen;
uint32_t    nFpgaToSW_PRACH_RxBufferLen;
uint32_t    nSW_ToFpga_FTH_TxBufferLen;

static struct bbu_xran_io_if  *p_app_io_xran_if;

void *                 app_io_xran_handle[XRAN_PORTS_NUM];
struct xran_fh_init    app_io_xran_fh_init;
struct xran_fh_config  app_io_xran_fh_config[XRAN_PORTS_NUM];

void app_io_xran_fh_rx_callback(void *pCallbackTag, int32_t status, uint8_t mu);
void app_io_xran_fh_rx_prach_callback(void *pCallbackTag, int32_t status, uint8_t mu);
void app_io_xran_fh_rx_srs_callback(void *pCallbackTag, xran_status_t status, uint8_t mu);
void app_io_xran_fh_rx_csirs_callback(void *pCallbackTag, xran_status_t status, uint8_t mu);

#ifndef FWK_ENABLED
void app_io_xran_fh_bbu_rx_callback(void *pCallbackTag, xran_status_t status, uint8_t mu);
void app_io_xran_fh_bbu_rx_bfw_callback(void *pCallbackTag, xran_status_t status, uint8_t mu);
void app_io_xran_fh_bbu_rx_prach_callback(void *pCallbackTag, xran_status_t status, uint8_t mu);
void app_io_xran_fh_bbu_rx_srs_callback(void *pCallbackTag, xran_status_t status, uint8_t mu);
#endif

extern RuntimeConfig* p_startupConfiguration[XRAN_PORTS_NUM];

struct bbu_xran_io_if *app_io_xran_if_alloc(void)
{
    void *ptr = 0;

    ptr = _mm_malloc(sizeof(struct bbu_xran_io_if), 256);
    if (ptr == NULL)
    {
        rte_panic("_mm_malloc: Can't allocate %lu bytes\n", sizeof(struct bbu_xran_io_if));
    }
    p_app_io_xran_if = (struct bbu_xran_io_if *)ptr;
    return p_app_io_xran_if;
}

struct bbu_xran_io_if *app_io_xran_if_get(void)
{
    return p_app_io_xran_if;
}

void app_io_xran_if_free(void)
{
    if(xran_mm_destroy(app_io_xran_handle[0]) != XRAN_STATUS_SUCCESS)
    {
        printf("Failed at xran_mm_destroy!\n");
    }

    if (p_app_io_xran_if == NULL)
    {
        rte_panic("_mm_free: Can't free p_app_io_xran_if\n");
    }
    _mm_free(p_app_io_xran_if);
    return;
}

struct xran_io_shared_ctrl *app_io_xran_if_ctrl_get(uint32_t o_xu_id)
{
    if(o_xu_id < XRAN_PORTS_NUM)
    {
        if(p_app_io_xran_if != NULL)
            return &(p_app_io_xran_if->ioCtrl[o_xu_id]);
        else
            return NULL;
    }
    else
    {
        return NULL;
    }
}

int32_t app_io_xran_sfidx_get(uint8_t mu)
{
    int32_t nSfIdx = -1;
    uint32_t nFrameIdx;
    uint32_t nSubframeIdx;
    uint32_t nSlotIdx;
    uint64_t nSecond;
    uint8_t nNrOfSlotInSf = ((mu == XRAN_NBIOT_MU)? 1 : 1 << mu );  /*mu = 5 represents NB-IOT mu = 0*/

    /*uint32_t nXranTime  = */xran_get_slot_idx(0, &nFrameIdx, &nSubframeIdx, &nSlotIdx, &nSecond, mu);
    nSfIdx = nFrameIdx*NUM_OF_SUBFRAME_PER_FRAME*nNrOfSlotInSf
        + nSubframeIdx*nNrOfSlotInSf
        + nSlotIdx;
#if 0
    printf("\nxranTime is %d, return is %d, radio frame is %d, subframe is %d slot is %d tsc is %llu us",
        nXranTime,
        nSfIdx,
        nFrameIdx,
        nSubframeIdx,
        nSlotIdx,
        __rdtsc()/CPU_HZ);
#endif

    return nSfIdx;
}

void app_io_xran_fh_rx_callback(void *pCallbackTag, xran_status_t status, uint8_t mu)
{
    uint64_t t1 = MLogTick();
    uint32_t mlogVar[10];
    uint32_t mlogVarCnt = 0;
    //uint8_t Numerlogy = app_io_xran_fh_config[0].frame_conf.nNumerology;
    //uint8_t nNrOfSlotInSf = 1<<Numerlogy;
    //int32_t sfIdx = app_io_xran_sfidx_get(nNrOfSlotInSf);
    int32_t nCellIdx;
    int32_t sym, nSlotIdx, ntti;
    uint64_t mlog_start;
    struct xran_cb_tag *pTag = (struct xran_cb_tag *) pCallbackTag;
    int32_t o_xu_id = pTag->oXuId;
    struct xran_io_shared_ctrl *psIoCtrl = app_io_xran_if_ctrl_get(o_xu_id);
    struct xran_fh_config  *pXranConf = &app_io_xran_fh_config[o_xu_id];
    uint32_t xran_max_antenna_nr = RTE_MAX(pXranConf->neAxc, pXranConf->neAxcUl);
    //int32_t nSectorNum = pXranConf->nCC;
    uint32_t ant_id, sym_id;

    mlog_start = MLogTick();

    nCellIdx = pTag->cellId;
    nSlotIdx = pTag->slotiId; ///((status >> 16) & 0xFFFF);  /** TTI aka slotIdx */
    sym      = pTag->symbol & 0xFF; /* sym */
    ntti = (nSlotIdx + XRAN_N_FE_BUF_LEN -1)  % XRAN_N_FE_BUF_LEN;

    {
        mlogVar[mlogVarCnt++] = 0xbcbcbcbc;
        mlogVar[mlogVarCnt++] = o_xu_id;
        mlogVar[mlogVarCnt++] = nCellIdx;
        mlogVar[mlogVarCnt++] = sym;
        mlogVar[mlogVarCnt++] = nSlotIdx;
        mlogVar[mlogVarCnt++] = ntti;
        //mlogVar[mlogVarCnt++] = nSlotIdx % gNumSlotPerSfn[nCellIdx];
        //mlogVar[mlogVarCnt++] = get_slot_type(nCellIdx, nSlotIdx, SLOT_TYPE_UL);

        MLogAddVariables(mlogVarCnt, mlogVar, mlog_start);
    }

    if(unlikely(psIoCtrl == NULL))
    {
        printf("psIoCtrl NULL! o_xu_id= %d\n", o_xu_id);
        return;
    }

    if (sym == XRAN_HALF_CB_SYM) {
        // 1/4 of slot
    } else if (sym == XRAN_HALF_CB_SYM) {
        // First Half
    } else if (sym == XRAN_THREE_FOURTHS_CB_SYM) {
        // 2/4 of slot
    } else if (sym == XRAN_FULL_CB_SYM) {
        // Second Half
    } else {
        /* error */
        MLogTask(PID_GNB_SYM_CB, t1, MLogTick());
        return;
    }

    for(nCellIdx = pTag->cellId; nCellIdx < (pTag->cellId + pXranConf->nCC); ++nCellIdx)
    {
        struct xran_prb_map *pRbMap = NULL;
        if(sym == XRAN_FULL_CB_SYM)  //full slot callback only
        {
            for(ant_id = 0; ant_id < xran_max_antenna_nr; ant_id++) {
                pRbMap = (struct xran_prb_map *) psIoCtrl->io_buff_perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[ntti][nCellIdx][ant_id].sBufferList.pBuffers->pData;
                if(unlikely(pRbMap == NULL)){
                    printf("(%d:%d:%d)pRbMap == NULL\n", nCellIdx, ntti, ant_id);
                    exit(-1);
                }
                for(sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
                    psIoCtrl->io_buff_perMu[mu].nRxPktBufCtrl[ntti][nCellIdx][ant_id][sym_id] = pRbMap->sFrontHaulRxPacketCtrl[sym_id].nRxPkt;
                    pRbMap->sFrontHaulRxPacketCtrl[sym_id].nRxPkt = 0;
                }
            }
        }

        rte_pause();
    } /*for nCellIDx*/

    MLogTask(PID_GNB_SYM_CB, t1, MLogTick());
    return;
}

void
app_io_xran_fh_rx_prach_callback(void *pCallbackTag, xran_status_t status, uint8_t mu)
{
    uint64_t t1 = MLogTick();
    uint32_t mlogVar[10];
    uint32_t mlogVarCnt = 0;
    int32_t nCellIdx;
    struct xran_cb_tag *pTag = (struct xran_cb_tag *) pCallbackTag;
    int32_t o_xu_id = pTag->oXuId;
    struct xran_io_shared_ctrl *psIoCtrl = app_io_xran_if_ctrl_get(o_xu_id);
    struct xran_fh_config  *pXranConf = &app_io_xran_fh_config[o_xu_id];
    uint32_t xran_max_antenna_nr_prach = RTE_MIN(pXranConf->neAxcUl, XRAN_MAX_PRACH_ANT_NUM);
    uint32_t ant_id, sym_id;
    int32_t nSlotIdx, ntti;

    if(unlikely(psIoCtrl == NULL))
    {
        printf("psIoCtrl NULL! o_xu_id= %d\n", o_xu_id);
        return;
    }

    nSlotIdx = pTag->slotiId; ///((status >> 16) & 0xFFFF);  /** TTI aka slotIdx */
    ntti = (nSlotIdx + XRAN_N_FE_BUF_LEN -1)  % XRAN_N_FE_BUF_LEN;

    mlogVar[mlogVarCnt++] = 0xDDDDDDDD;
    mlogVar[mlogVarCnt++] = status >> 16; /* tti */
    mlogVar[mlogVarCnt++] = status & 0xFF; /* sym */
    MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());
    for(nCellIdx = pTag->cellId; nCellIdx < (pTag->cellId + pXranConf->nCC); ++nCellIdx)
    {
        struct xran_prb_map *pRbMap = NULL;
        for(ant_id = 0; ant_id < xran_max_antenna_nr_prach; ant_id++)
        {
            pRbMap = (struct xran_prb_map *) psIoCtrl->io_buff_perMu[mu].sFHPrachRxPrbMapBbuIoBufCtrl[ntti][nCellIdx][ant_id].sBufferList.pBuffers->pData;
            if(unlikely(pRbMap == NULL)){
                printf("(%d:%d:%d)pRbMap == NULL\n", nCellIdx, ntti, ant_id);
                exit(-1);
            }
            for(sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
                psIoCtrl->io_buff_perMu[mu].nPRACHRxPktBufCtrl[ntti][nCellIdx][ant_id][sym_id] = pRbMap->sFrontHaulRxPacketCtrl[sym_id].nRxPkt;
                pRbMap->sFrontHaulRxPacketCtrl[sym_id].nRxPkt = 0;
            }
        }
    }
    rte_pause();

    MLogTask(PID_GNB_PRACH_CB, t1, MLogTick());
}

void
app_io_xran_fh_rx_csirs_callback(void *pCallbackTag, xran_status_t status, uint8_t mu)
{
    uint64_t t1 = MLogTick();
    uint32_t mlogVar[10];
    uint32_t mlogVarCnt = 0;
    int32_t nCellIdx;
    int32_t sym, nSlotIdx, ntti;
    struct xran_cb_tag *pTag = (struct xran_cb_tag *) pCallbackTag;
    int32_t o_xu_id = pTag->oXuId;
    struct xran_io_shared_ctrl *psIoCtrl = app_io_xran_if_ctrl_get(o_xu_id);
    struct xran_fh_config  *pXranConf = &app_io_xran_fh_config[o_xu_id];
    uint32_t ant_id, sym_id;
    struct xran_buffer_list *pBufList;
    nCellIdx = pTag->cellId;
    nSlotIdx = pTag->slotiId; ///((status >> 16) & 0xFFFF);  /** TTI aka slotIdx */
    sym      = pTag->symbol & 0xFF; /* sym */
    ntti = (nSlotIdx + XRAN_N_FE_BUF_LEN-1) % XRAN_N_FE_BUF_LEN;

    {
    mlogVar[mlogVarCnt++] = 0xCCCCCCCC;
        mlogVar[mlogVarCnt++] = o_xu_id;
        mlogVar[mlogVarCnt++] = nCellIdx;
        mlogVar[mlogVarCnt++] = sym;
        mlogVar[mlogVarCnt++] = nSlotIdx;
        mlogVar[mlogVarCnt++] = ntti;
    MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());
    }

    if(unlikely(psIoCtrl == NULL))
    {
        printf("psIoCtrl NULL! o_xu_id= %d\n", o_xu_id);
        return;
    }
    for(nCellIdx = pTag->cellId; nCellIdx < (pTag->cellId + pXranConf->nCC); ++nCellIdx)
    {
        struct xran_prb_map *pRbMap = NULL;
        if(sym == XRAN_FULL_CB_SYM) { //full slot callback only
            for(ant_id = 0; ant_id < XRAN_MAX_CSIRS_PORTS; ant_id++) {
                pBufList = &(psIoCtrl->io_buff_perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[ntti][nCellIdx][ant_id].sBufferList); /* To shorten reference */
                if(pBufList->pBuffers && pBufList->pBuffers->pData)
                {
                    pRbMap = (struct xran_prb_map *) psIoCtrl->io_buff_perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[ntti][nCellIdx][ant_id].sBufferList.pBuffers->pData;
                    if(pRbMap != NULL){
                        for(sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
                            psIoCtrl->io_buff_perMu[mu].nCSIRSRxPktBufCtrl[ntti][nCellIdx][ant_id][sym_id] = pRbMap->sFrontHaulRxPacketCtrl[sym_id].nRxPkt;
                            pRbMap->sFrontHaulRxPacketCtrl[sym_id].nRxPkt = 0;
                        }
                    }
                }
            }
        }
    }
    MLogTask(PID_GNB_CSIRS_CB, t1, MLogTick());
}

void
app_io_xran_fh_rx_srs_callback(void *pCallbackTag, xran_status_t status, uint8_t mu)
{
    uint64_t t1 = MLogTick();
    uint32_t mlogVar[10];
    uint32_t mlogVarCnt = 0;
    //uint8_t Numerlogy = app_io_xran_fh_config[0].frame_conf.nNumerology;
    //uint8_t nNrOfSlotInSf = 1<<Numerlogy;
    //int32_t sfIdx = app_io_xran_sfidx_get(nNrOfSlotInSf);
    int32_t nCellIdx;
    int32_t sym, nSlotIdx, ntti;
    struct xran_cb_tag *pTag = (struct xran_cb_tag *) pCallbackTag;
    int32_t o_xu_id = pTag->oXuId;
    struct xran_io_shared_ctrl *psIoCtrl = app_io_xran_if_ctrl_get(o_xu_id);
    struct xran_fh_config  *pXranConf = &app_io_xran_fh_config[o_xu_id];
    uint32_t xran_max_antenna_nr = RTE_MAX(pXranConf->neAxc, pXranConf->neAxcUl);
    //int32_t nSectorNum = pXranConf->nCC;
    uint32_t ant_id, sym_id;
    uint32_t xran_max_ant_array_elm_nr = RTE_MAX(pXranConf->nAntElmTRx, xran_max_antenna_nr);

    nSlotIdx = pTag->slotiId; ///((status >> 16) & 0xFFFF);  /** TTI aka slotIdx */
    sym      = pTag->symbol & 0xFF; /* sym */
    ntti = (nSlotIdx + XRAN_N_FE_BUF_LEN-1) % XRAN_N_FE_BUF_LEN;

    {
    mlogVar[mlogVarCnt++] = 0xCCCCCCCC;
        mlogVar[mlogVarCnt++] = o_xu_id;
        mlogVar[mlogVarCnt++] = pTag->cellId;
        mlogVar[mlogVarCnt++] = sym;
        mlogVar[mlogVarCnt++] = nSlotIdx;
        mlogVar[mlogVarCnt++] = ntti;
    MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());
    }

    if(unlikely(psIoCtrl == NULL))
    {
        printf("psIoCtrl NULL! o_xu_id= %d\n", o_xu_id);
        return;
    }
    for(nCellIdx = pTag->cellId; nCellIdx < (pTag->cellId + pXranConf->nCC); ++nCellIdx)
    {
        struct xran_prb_map *pRbMap = NULL;
        if(sym == XRAN_FULL_CB_SYM) { //full slot callback only
            for(ant_id = 0; ant_id < xran_max_ant_array_elm_nr; ant_id++) {
                pRbMap = (struct xran_prb_map *) psIoCtrl->io_buff_perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[ntti][nCellIdx][ant_id].sBufferList.pBuffers->pData;
                if(unlikely(pRbMap == NULL)){
                    printf("(%d:%d:%d)pRbMap == NULL\n", nCellIdx, ntti, ant_id);
                    exit(-1);
                }
                for(sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
                    psIoCtrl->io_buff_perMu[mu].nSRSRxPktBufCtrl[ntti][nCellIdx][ant_id][sym_id] = pRbMap->sFrontHaulRxPacketCtrl[sym_id].nRxPkt;
                    pRbMap->sFrontHaulRxPacketCtrl[sym_id].nRxPkt = 0;
                }
            }
        }
    }
    MLogTask(PID_GNB_SRS_CB, t1, MLogTick());
}

void
app_io_xran_fh_rx_bfw_callback(void *pCallbackTag, xran_status_t status, uint8_t mu)
{
    uint64_t t1 = MLogTick();
    uint32_t mlogVar[10];
    uint32_t mlogVarCnt = 0;

    mlogVar[mlogVarCnt++] = 0xCCCCCCCC;
    mlogVar[mlogVarCnt++] = status >> 16; /* tti */
    mlogVar[mlogVarCnt++] = status & 0xFF; /* sym */
    MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());
    rte_pause();

    MLogTask(PID_GNB_BFW_CB, t1, MLogTick());
}

int32_t
app_io_xran_dl_tti_call_back(void * param, uint8_t mu)
{
    uint64_t t1 = MLogTick();
    rte_pause();
    MLogTask(PID_GNB_PROC_TIMING, t1, MLogTick());
    return 0;
}

int32_t
app_io_xran_ul_half_slot_call_back(void * param, uint8_t mu)
{
    uint64_t t1 = MLogTick();
    rte_pause();
    MLogTask(PID_GNB_PROC_TIMING, t1, MLogTick());
    return 0;
}

int32_t
app_io_xran_ul_full_slot_call_back(void * param, uint8_t mu)
{
    uint64_t t1 = MLogTick();
    rte_pause();
    MLogTask(PID_GNB_PROC_TIMING, t1, MLogTick());
    return 0;
}

int32_t
app_io_xran_ul_custom_sym_call_back(void * param, struct xran_sense_of_time* time)
{
    uint64_t t1 = MLogTick();
    uint32_t mlogVar[15];
    uint32_t mlogVarCnt = 0;
    uint32_t sym_idx = 0;

    mlogVar[mlogVarCnt++] = 0xDEADDEAD;
    if(time) {
        mlogVar[mlogVarCnt++] = time->type_of_event;
        mlogVar[mlogVarCnt++] = time->nSymIdx;
        mlogVar[mlogVarCnt++] = time->tti_counter;
        mlogVar[mlogVarCnt++] = time->nFrameIdx;
        mlogVar[mlogVarCnt++] = time->nSubframeIdx;
        mlogVar[mlogVarCnt++] = time->nSlotIdx;
        mlogVar[mlogVarCnt++] = (uint32_t)(time->nSecond);
        mlogVar[mlogVarCnt++] = (uint32_t)(time->nSecond >> 32);
        sym_idx =   time->nSymIdx;
    }
    MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());

    rte_pause();
    MLogTask(PID_GNB_SYM_CB + sym_idx, t1, MLogTick());
    return 0;
}

uint32_t
NEXT_POW2 ( uint32_t  x )
{
    uint32_t  value  =  1 ;
    while  ( value  <=  x)
        value  =  value  <<  1;

    return  value ;
}

int32_t
app_io_xran_interface(uint32_t o_xu_id, RuntimeConfig *p_o_xu_cfg, UsecaseConfig* p_use_cfg, struct xran_fh_init* p_xran_fh_init)
{
    xran_status_t status;
    struct bbu_xran_io_if *psBbuIo = app_io_xran_if_get();
    struct xran_io_shared_ctrl *psIoCtrl = app_io_xran_if_ctrl_get(o_xu_id);
    int32_t nSectorIndex[XRAN_MAX_SECTOR_NR];
    int32_t nSectorNum;
    int32_t i, j, k = 0, z, l;

    void *ptr;
    void *mb;
    void *ring;
    uint32_t *u32dptr;

    if(psBbuIo == NULL)
        rte_panic("psBbuIo == NULL\n");

    if(psIoCtrl == NULL)
        rte_panic("psIoCtrl == NULL\n");

    printf ("XRAN front haul xran_mm_init \n");
    status = xran_mm_init (app_io_xran_handle[0], (uint64_t) SW_FPGA_FH_TOTAL_BUFFER_LEN, SW_FPGA_SEGMENT_BUFFER_LEN);
    if (status != XRAN_STATUS_SUCCESS)
    {
        printf ("Failed at XRAN front haul xran_mm_init \n");
        exit(-1);
    }
    printf("Sucess xran_mm_init \n");

    if(o_xu_id == 0) {
        psBbuIo->num_o_ru = p_use_cfg->oXuNum;
        psBbuIo->bbu_offload = p_xran_fh_init->io_cfg.bbu_offload;
    }

    psIoCtrl->byteOrder = XRAN_NE_BE_BYTE_ORDER;
    psIoCtrl->iqOrder   = XRAN_I_Q_ORDER;

    for (nSectorNum = 0; nSectorNum < XRAN_MAX_SECTOR_NR; nSectorNum++)
    {
        nSectorIndex[nSectorNum] = nSectorNum;
    }

    if(p_use_cfg->oXuNum > 1 && p_use_cfg->oXuNum <= XRAN_PORTS_NUM) {
        nSectorNum = p_o_xu_cfg->numCC;
        psBbuIo->num_cc_per_port[o_xu_id] = p_o_xu_cfg->numCC;
        printf("port %d has %d CCs\n",o_xu_id,  psBbuIo->num_cc_per_port[o_xu_id]);
        for(i = 0; i < XRAN_MAX_SECTOR_NR && i < nSectorNum; i++) {
            psBbuIo->map_cell_id2port[o_xu_id][i] = (o_xu_id*nSectorNum)+i;
            printf("port %d cc_id %d is phy id %d\n", o_xu_id, i, psBbuIo->map_cell_id2port[o_xu_id][i]);
        }
    }
    else {
        nSectorNum = p_o_xu_cfg->numCC;;
        psBbuIo->num_cc_per_port[o_xu_id] = nSectorNum;
        printf("port %d has %d CCs\n",o_xu_id,  psBbuIo->num_cc_per_port[o_xu_id]);
        for(i = 0; i < XRAN_MAX_SECTOR_NR && i < nSectorNum; i++) {
            psBbuIo->map_cell_id2port[o_xu_id][i] = i;
            printf("port %d cc_id %d is phy id %d\n", o_xu_id, i, psBbuIo->map_cell_id2port[o_xu_id][i]);
        }
    }

    psBbuIo->nInstanceNum[o_xu_id] = p_o_xu_cfg->numCC;
    if (o_xu_id < XRAN_PORTS_NUM) {
        status = xran_sector_get_instances (o_xu_id, app_io_xran_handle[0],
                psBbuIo->nInstanceNum[o_xu_id],
                &psBbuIo->nInstanceHandle[o_xu_id][0]);
        if (status != XRAN_STATUS_SUCCESS) {
            printf ("get sector instance failed for XRAN nInstanceNum[%d] %d\n",psBbuIo->nInstanceNum[o_xu_id], o_xu_id);
            exit(-1);
        }
        for (i = 0; i < psBbuIo->nInstanceNum[o_xu_id]; i++) {
            printf("%s: CC %d handle %p\n", __FUNCTION__, i, psBbuIo->nInstanceHandle[o_xu_id][i]);
        }
    } else {
        printf ("Failed at XRAN front haul xran_mm_init \n");
        exit(-1);
    }

    uint32_t xran_max_antenna_nr = RTE_MAX(p_o_xu_cfg->numAxc, p_o_xu_cfg->numUlAxc);
    uint32_t xran_max_ant_array_elm_nr = RTE_MAX(p_o_xu_cfg->antElmTRx, xran_max_antenna_nr);
    if(p_o_xu_cfg->max_sections_per_slot > XRAN_MAX_SECTIONS_PER_SLOT)
    {
        printf("Requested value for max sections per slot (%u) is greater than what we support (%u)\n",
                p_o_xu_cfg->max_sections_per_slot, XRAN_MAX_SECTIONS_PER_SLOT);
        exit(1);
    }

    uint32_t xran_max_antenna_nr_prach = RTE_MIN(xran_max_antenna_nr, XRAN_MAX_PRACH_ANT_NUM);
    SWXRANInterfaceTypeEnum eInterfaceType;

    struct xran_buffer_list *pFthTxBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
    struct xran_buffer_list *pFthTxPrbMapBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
    struct xran_buffer_list *pFthRxBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
    struct xran_buffer_list *pFthRxPrbMapBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
    struct xran_buffer_list *pFthRxRachBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
    struct xran_buffer_list *pFthRxRachBufferDecomp[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
    struct xran_buffer_list *pFthRxSrsBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANT_ARRAY_ELM_NR][XRAN_N_FE_BUF_LEN];
    struct xran_buffer_list *pFthRxSrsPrbMapBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANT_ARRAY_ELM_NR][XRAN_N_FE_BUF_LEN];
    struct xran_buffer_list *pFthTxCsirsBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_CSIRS_PORTS][XRAN_N_FE_BUF_LEN];
    struct xran_buffer_list *pFthTxCsirsPrbMapBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_CSIRS_PORTS][XRAN_N_FE_BUF_LEN];

    struct xran_buffer_list *pFthRxCpPrbMapBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANT_ARRAY_ELM_NR][XRAN_N_FE_BUF_LEN];
    struct xran_buffer_list *pFthTxCpPrbMapBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANT_ARRAY_ELM_NR][XRAN_N_FE_BUF_LEN];

    printf("nSectorNum %d\n", nSectorNum);

    nSectorNum = p_o_xu_cfg->numCC;
    uint32_t numPrbElm, size_of_prb_map;
    /* Init Memory */
    for(i = 0; i < nSectorNum; i++)
    {
        numPrbElm = xran_get_num_prb_elm(p_o_xu_cfg->p_PrbMapDl[p_o_xu_cfg->mu_number[0]], p_o_xu_cfg->mtu);

        if(numPrbElm == 0)
            numPrbElm = XRAN_MAX_SECTIONS_PER_SLOT;

        size_of_prb_map  = sizeof(struct xran_prb_map) + sizeof(struct xran_prb_elm)*(numPrbElm);

        eInterfaceType = XRANFTHTX_OUT;
        printf("nSectorIndex[%d] = %d\n",i,  nSectorIndex[i]);

        status = xran_bm_init(psBbuIo->nInstanceHandle[o_xu_id][i], &psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],
                NEXT_POW2(2*p_o_xu_cfg->numMu*XRAN_N_FE_BUF_LEN*xran_max_antenna_nr*XRAN_NUM_OF_SYMBOL_PER_SLOT)-1,
                nSW_ToFpga_FTH_TxBufferLen);
        if(XRAN_STATUS_SUCCESS != status) {
            rte_panic("Failed at  xran_bm_init , status %d\n", status);
        }

        printf("size_of_prb_map %d\n", size_of_prb_map);

        eInterfaceType = XRANFTHTX_PRB_MAP_OUT;
        status = xran_bm_init(psBbuIo->nInstanceHandle[o_xu_id][i], &psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],
                p_o_xu_cfg->numMu*NEXT_POW2(XRAN_N_FE_BUF_LEN*xran_max_antenna_nr*XRAN_NUM_OF_SYMBOL_PER_SLOT)-1,
                size_of_prb_map);
        if(XRAN_STATUS_SUCCESS != status) {
            rte_panic("Failed at  xran_bm_init , status %d\n", status);
        }

        eInterfaceType = XRANFTHRX_IN;
        status = xran_bm_init(psBbuIo->nInstanceHandle[o_xu_id][i], &psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],
                p_o_xu_cfg->numMu*NEXT_POW2(XRAN_N_FE_BUF_LEN*xran_max_antenna_nr*XRAN_NUM_OF_SYMBOL_PER_SLOT)-1, nSW_ToFpga_FTH_TxBufferLen);
        if(XRAN_STATUS_SUCCESS != status)
        {
            printf("Failed at xran_bm_init, status %d\n", status);
            iAssert(status == XRAN_STATUS_SUCCESS);
        }

        /* C-plane */
        eInterfaceType = XRANFTHRX_PRB_MAP_IN;
        status = xran_bm_init(psBbuIo->nInstanceHandle[o_xu_id][i], &psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],
                p_o_xu_cfg->numMu*NEXT_POW2(XRAN_N_FE_BUF_LEN*xran_max_antenna_nr*XRAN_NUM_OF_SYMBOL_PER_SLOT)-1, size_of_prb_map);
        if(XRAN_STATUS_SUCCESS != status) {
            rte_panic("Failed at xran_bm_init, status %d\n", status);
        }

        if(p_o_xu_cfg->appMode == APP_O_RU){
            /* C-plane Rx */
            eInterfaceType = XRANCP_PRB_MAP_IN_RX;
            status = xran_bm_init(psBbuIo->nInstanceHandle[o_xu_id][i], &psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],
                    p_o_xu_cfg->numMu*XRAN_N_FE_BUF_LEN*xran_max_antenna_nr*XRAN_NUM_OF_SYMBOL_PER_SLOT, size_of_prb_map);
            if(XRAN_STATUS_SUCCESS != status) {
                rte_panic("Failed at xran_bm_init, status %d\n", status);
            }

            eInterfaceType = XRANCP_PRB_MAP_IN_TX;
            status = xran_bm_init(psBbuIo->nInstanceHandle[o_xu_id][i], &psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],
                    p_o_xu_cfg->numMu*NEXT_POW2(XRAN_N_FE_BUF_LEN*xran_max_antenna_nr*XRAN_NUM_OF_SYMBOL_PER_SLOT)-1, size_of_prb_map);
            if(XRAN_STATUS_SUCCESS != status) {
                rte_panic("Failed at xran_bm_init, status %d\n", status);
            }
        }

        eInterfaceType = XRANFTHRACH_IN;
        status = xran_bm_init(psBbuIo->nInstanceHandle[o_xu_id][i],&psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],
                p_o_xu_cfg->numMu*NEXT_POW2(XRAN_N_FE_BUF_LEN*xran_max_antenna_nr_prach*XRAN_NUM_OF_SYMBOL_PER_SLOT)-1, PRACH_PLAYBACK_BUFFER_BYTES);
        if(XRAN_STATUS_SUCCESS != status) {
            rte_panic("Failed at xran_bm_init, status %d\n", status);
        }
        eInterfaceType = XRANFTHRACH_PRB_MAP_IN;
        status = xran_bm_init(psBbuIo->nInstanceHandle[o_xu_id][i], &psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],
                p_o_xu_cfg->numMu*NEXT_POW2(XRAN_N_FE_BUF_LEN*xran_max_antenna_nr*XRAN_NUM_OF_SYMBOL_PER_SLOT)-1,
                size_of_prb_map);
        if(XRAN_STATUS_SUCCESS != status) {
            rte_panic("Failed at  xran_bm_init , status %d\n", status);
        }

        if(xran_max_ant_array_elm_nr) {
            eInterfaceType = XRANSRS_IN;
            status = xran_bm_init(psBbuIo->nInstanceHandle[o_xu_id][i],&psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],
                    p_o_xu_cfg->numMu*NEXT_POW2(XRAN_N_FE_BUF_LEN*xran_max_ant_array_elm_nr*XRAN_MAX_NUM_OF_SRS_SYMBOL_PER_SLOT)-1, nSW_ToFpga_FTH_TxBufferLen);

            if(XRAN_STATUS_SUCCESS != status) {
                rte_panic("Failed at xran_bm_init, status %d\n", status);
            }

            /* SRS C-plane */
            eInterfaceType = XRANSRS_PRB_MAP_IN;
            status = xran_bm_init(psBbuIo->nInstanceHandle[o_xu_id][i], &psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],
                    p_o_xu_cfg->numMu*NEXT_POW2(XRAN_N_FE_BUF_LEN*xran_max_ant_array_elm_nr*XRAN_NUM_OF_SYMBOL_PER_SLOT)-1, size_of_prb_map);
            if(XRAN_STATUS_SUCCESS != status) {
                rte_panic("Failed at xran_bm_init, status %d\n", status);
            }
        }

        if(p_o_xu_cfg->csirsEnable) { /*CSI-RS enable condition*/
            numPrbElm = xran_get_num_prb_elm(p_o_xu_cfg->p_PrbMapCsiRs, p_o_xu_cfg->mtu);
            size_of_prb_map  = sizeof(struct xran_prb_map) + sizeof(struct xran_prb_elm)*(numPrbElm);
            eInterfaceType = XRANCSIRS_TX;
            status = xran_bm_init(psBbuIo->nInstanceHandle[o_xu_id][i],&psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],
                    p_o_xu_cfg->numMu*NEXT_POW2(XRAN_N_FE_BUF_LEN*XRAN_MAX_CSIRS_PORTS*XRAN_NUM_OF_SYMBOL_PER_SLOT)-1, nSW_ToFpga_FTH_TxBufferLen);

            if(XRAN_STATUS_SUCCESS != status) {
                rte_panic("Failed at xran_bm_init, status %d\n", status);
            }
            /* CSIRS C-plane */
            eInterfaceType = XRANCSIRS_PRB_MAP_TX;
            status = xran_bm_init(psBbuIo->nInstanceHandle[o_xu_id][i], &psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],
                    p_o_xu_cfg->numMu*NEXT_POW2(XRAN_N_FE_BUF_LEN*XRAN_MAX_CSIRS_PORTS*XRAN_NUM_OF_SYMBOL_PER_SLOT)-1, size_of_prb_map);
            if(XRAN_STATUS_SUCCESS != status) {
                rte_panic("Failed at xran_bm_init, status %d\n", status);
            }
        }
    } /* per CC xran_bm_init */


    for(l = 0; l < p_o_xu_cfg->numMu; l++)
    {
        uint8_t mu = p_o_xu_cfg->mu_number[l];
        uint32_t xran_max_prb = app_xran_get_num_rbs(p_o_xu_cfg->xranTech, mu,p_o_xu_cfg->perMu[mu].nDLBandwidth, p_o_xu_cfg->nDLAbsFrePointA);
        numPrbElm = xran_get_num_prb_elm(p_o_xu_cfg->p_PrbMapDl[mu], p_o_xu_cfg->mtu);
        size_of_prb_map  = sizeof(struct xran_prb_map) + sizeof(struct xran_prb_elm)*(numPrbElm);

        nSectorNum = p_o_xu_cfg->numCC;

        /* Init Memory - TX (UP, CP) */
        for(i = 0; i < nSectorNum; i++)
        {
            eInterfaceType = XRANFTHTX_OUT;
            for(j = 0; j < XRAN_N_FE_BUF_LEN; j++)
            {
                for(z = 0; z < xran_max_antenna_nr; z++)
                {
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][i][z].bValid = 0;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &psIoCtrl->io_buff_perMu[mu].sFrontHaulTxBuffers[j][i][z][0];

                    for(k = 0; k < XRAN_NUM_OF_SYMBOL_PER_SLOT; k++)
                    {
                        psIoCtrl->io_buff_perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nElementLenInBytes = nSW_ToFpga_FTH_TxBufferLen; // 14 symbols 3200bytes/symbol
                        psIoCtrl->io_buff_perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nNumberOfElements = 1;
                        psIoCtrl->io_buff_perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nOffsetInBytes = 0;

                        status = xran_bm_allocate_buffer(psBbuIo->nInstanceHandle[o_xu_id][i],
                                psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType], &ptr, &mb);
                        if(XRAN_STATUS_SUCCESS != status){
                            rte_panic("Failed at  xran_bm_allocate_buffer , status %d\n",status);
                        }

                        psIoCtrl->io_buff_perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pData = (uint8_t *)ptr;
                        psIoCtrl->io_buff_perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pCtrl = (void *)mb;

                        if(ptr){
                            u32dptr = (uint32_t*)(ptr);
                            memset(u32dptr, 0x0, nSW_ToFpga_FTH_TxBufferLen);
                            // ptr_temp[0] = j; // TTI
                            // ptr_temp[1] = i; // Sec
                            // ptr_temp[2] = z; // Ant
                            // ptr_temp[3] = k; // sym
                        }

                        if(psBbuIo->bbu_offload)
                        {
                            status = xran_bm_allocate_ring(psBbuIo->nInstanceHandle[o_xu_id][i], "TXO", i, j, z, k, &ring);

                            if(XRAN_STATUS_SUCCESS != status){
                                rte_panic("Failed at  xran_bm_allocate_ring , status %d\n",status);
                            }

                            psIoCtrl->io_buff_perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pRing = (void *)ring;
                        }
                    } /* symbol loop */
                } /* antenna loop */
            } /* XRAN_N_FE_BUF_LEN loop */

            eInterfaceType = XRANFTHTX_PRB_MAP_OUT;
            /* ---------- C-plane DL ----------- */
            for(j = 0; j < XRAN_N_FE_BUF_LEN; j++)
            {
                for(z = 0; z < xran_max_antenna_nr; z++)
                {
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].bValid = 0;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &psIoCtrl->io_buff_perMu[mu].sFrontHaulTxPrbMapBuffers[j][i][z];

                    psIoCtrl->io_buff_perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nElementLenInBytes = size_of_prb_map;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nNumberOfElements = 1;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nOffsetInBytes = 0;
                    status = xran_bm_allocate_buffer(psBbuIo->nInstanceHandle[o_xu_id][i], psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],&ptr, &mb);
                    if(XRAN_STATUS_SUCCESS != status) {
                        rte_panic("Failed at  xran_bm_allocate_buffer , status %d\n",status);
                    }

                    psIoCtrl->io_buff_perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->pData = (uint8_t *)ptr;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->pCtrl = (void *)mb;

                    if(ptr){
                        struct xran_prb_map * p_rb_map = (struct xran_prb_map *)ptr;
                        memset(p_rb_map, 0, size_of_prb_map);
                        if (p_o_xu_cfg->appMode == APP_O_DU) {
                            if(p_o_xu_cfg->RunSlotPrbMapEnabled) {
                                if(p_o_xu_cfg->RunSlotPrbMapBySymbolEnable){
                                    xran_init_PrbMap_by_symbol_from_cfg(p_o_xu_cfg->p_RunSlotPrbMap[XRAN_DIR_DL][j][i][z], ptr, p_o_xu_cfg->mtu, xran_max_prb);
                                }
                                else {
                                    xran_init_PrbMap_from_cfg(p_o_xu_cfg->p_RunSlotPrbMap[XRAN_DIR_DL][j][i][z], ptr, p_o_xu_cfg->mtu);
                                }
                            } else {
                                xran_init_PrbMap_from_cfg(p_o_xu_cfg->p_PrbMapDl[mu], ptr, p_o_xu_cfg->mtu);
                            }
                        } else {
                            if(p_o_xu_cfg->RunSlotPrbMapEnabled) {
                                if(p_o_xu_cfg->RunSlotPrbMapBySymbolEnable){
                                    xran_init_PrbMap_by_symbol_from_cfg(p_o_xu_cfg->p_RunSlotPrbMap[XRAN_DIR_UL][j][i][z], ptr, p_o_xu_cfg->mtu, xran_max_prb);
                                }
                                else {
                                    xran_init_PrbMap_from_cfg(p_o_xu_cfg->p_RunSlotPrbMap[XRAN_DIR_UL][j][i][z], ptr, p_o_xu_cfg->mtu);
                                }
                            } else {
                                xran_init_PrbMap_from_cfg(p_o_xu_cfg->p_PrbMapUl[mu], ptr, p_o_xu_cfg->mtu);
                            }
                        }
                    }
                } /* CP antenna loop */
            } /* CP XRAN_N_FE_BUF_LEN loop */
        } /* CC loop */

        /* Init Memory - RX (UP, CP) */
        for(i = 0; i<nSectorNum; i++)
        {
            eInterfaceType = XRANFTHRX_IN;
            for(j = 0;j < XRAN_N_FE_BUF_LEN; j++)
            {
                for(z = 0; z < xran_max_antenna_nr; z++){
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][i][z].bValid = 0;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &psIoCtrl->io_buff_perMu[mu].sFrontHaulRxBuffers[j][i][z][0];
                    for(k = 0; k< XRAN_NUM_OF_SYMBOL_PER_SLOT; k++)
                    {
                        psIoCtrl->io_buff_perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nElementLenInBytes = nFpgaToSW_FTH_RxBufferLen; // 1 symbols 3200bytes
                        psIoCtrl->io_buff_perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nNumberOfElements = 1;
                        psIoCtrl->io_buff_perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nOffsetInBytes = 0;
                        status = xran_bm_allocate_buffer(psBbuIo->nInstanceHandle[o_xu_id][i],psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],&ptr, &mb);
                        if(XRAN_STATUS_SUCCESS != status) {
                            rte_panic("Failed at  xran_bm_allocate_buffer , status %d\n",status);
                        }
                        psIoCtrl->io_buff_perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pData = (uint8_t *)ptr;
                        psIoCtrl->io_buff_perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pCtrl = (void *) mb;
                        if(ptr){
                            u32dptr = (uint32_t*)(ptr);
                            //uint8_t *ptr_temp = (uint8_t *)ptr;
                            memset(u32dptr, 0x0, nFpgaToSW_FTH_RxBufferLen);
                            //   ptr_temp[0] = j; // TTI
                            //   ptr_temp[1] = i; // Sec
                            //   ptr_temp[2] = z; // Ant
                            //   ptr_temp[3] = k; // sym
                        }
                    }
                }
            }

            eInterfaceType = XRANFTHRX_PRB_MAP_IN;
            for(j = 0;j < XRAN_N_FE_BUF_LEN; j++) {
                for(z = 0; z < xran_max_antenna_nr; z++){
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].bValid = 0;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &psIoCtrl->io_buff_perMu[mu].sFrontHaulRxPrbMapBuffers[j][i][z];

                    psIoCtrl->io_buff_perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nElementLenInBytes = size_of_prb_map;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nNumberOfElements = 1;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nOffsetInBytes = 0;
                    status = xran_bm_allocate_buffer(psBbuIo->nInstanceHandle[o_xu_id][i],psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],&ptr, &mb);
                    if(XRAN_STATUS_SUCCESS != status) {
                        rte_panic("Failed at  xran_bm_allocate_buffer , status %d\n",status);
                    }
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->pData = (uint8_t *)ptr;
                    psIoCtrl->io_buff_perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->pCtrl = (void *)mb;
                    if(ptr){
                        struct xran_prb_map * p_rb_map = (struct xran_prb_map *)ptr;
                        memset(p_rb_map, 0, size_of_prb_map);

                        if (p_o_xu_cfg->appMode == APP_O_DU) {
                            if(p_o_xu_cfg->RunSlotPrbMapEnabled) {
                                if(p_o_xu_cfg->RunSlotPrbMapBySymbolEnable){
                                    xran_init_PrbMap_by_symbol_from_cfg(p_o_xu_cfg->p_RunSlotPrbMap[XRAN_DIR_UL][j][i][z], ptr, p_o_xu_cfg->mtu, xran_max_prb);
                                }
                                else {
                                    xran_init_PrbMap_from_cfg(p_o_xu_cfg->p_RunSlotPrbMap[XRAN_DIR_UL][j][i][z], ptr, p_o_xu_cfg->mtu);
                                }
                            } else {
                                xran_init_PrbMap_from_cfg(p_o_xu_cfg->p_PrbMapUl[mu], ptr, p_o_xu_cfg->mtu);
                            }
                        } else {
                            if(p_o_xu_cfg->RunSlotPrbMapEnabled) {
                                if(p_o_xu_cfg->RunSlotPrbMapBySymbolEnable){
                                    xran_init_PrbMap_by_symbol_from_cfg(p_o_xu_cfg->p_RunSlotPrbMap[XRAN_DIR_DL][j][i][z], ptr, p_o_xu_cfg->mtu, xran_max_prb);
                                }
                                else {
                                    xran_init_PrbMap_from_cfg(p_o_xu_cfg->p_RunSlotPrbMap[XRAN_DIR_DL][j][i][z], ptr, p_o_xu_cfg->mtu);
                                }
                            } else {
                                xran_init_PrbMap_from_cfg(p_o_xu_cfg->p_PrbMapDl[mu], ptr, p_o_xu_cfg->mtu);
                            }
                        }
                    }
                }
            }

            if(p_o_xu_cfg->appMode == APP_O_RU){
                /* C-plane Rx */
                eInterfaceType = XRANCP_PRB_MAP_IN_RX;
                for(j = 0;j < XRAN_N_FE_BUF_LEN; j++) {
                    for(z = 0; z < xran_max_antenna_nr; z++){
                        psIoCtrl->io_buff_perMu[mu].sFHCpRxPrbMapBbuIoBufCtrl[j][i][z].bValid = 0;
                        psIoCtrl->io_buff_perMu[mu].sFHCpRxPrbMapBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
                        psIoCtrl->io_buff_perMu[mu].sFHCpRxPrbMapBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
                        psIoCtrl->io_buff_perMu[mu].sFHCpRxPrbMapBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
                        psIoCtrl->io_buff_perMu[mu].sFHCpRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
                        psIoCtrl->io_buff_perMu[mu].sFHCpRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &psIoCtrl->io_buff_perMu[mu].sFrontHaulCpRxPrbMapBbuIoBufCtrl[j][i][z];

                        psIoCtrl->io_buff_perMu[mu].sFHCpRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nElementLenInBytes = size_of_prb_map;
                        psIoCtrl->io_buff_perMu[mu].sFHCpRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nNumberOfElements = 1;
                        psIoCtrl->io_buff_perMu[mu].sFHCpRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nOffsetInBytes = 0;
                        status = xran_bm_allocate_buffer(psBbuIo->nInstanceHandle[o_xu_id][i],psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],&ptr, &mb);
                        if(XRAN_STATUS_SUCCESS != status) {
                            rte_panic("Failed at  xran_bm_allocate_buffer , status %d\n",status);
                        }
                        psIoCtrl->io_buff_perMu[mu].sFHCpRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->pData = (uint8_t *)ptr;
                        psIoCtrl->io_buff_perMu[mu].sFHCpRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->pCtrl = (void *)mb;

                        if(ptr){
                            struct xran_prb_map * p_rb_map = (struct xran_prb_map *)ptr;
                            memset(p_rb_map, 0, size_of_prb_map);

                            if(p_o_xu_cfg->RunSlotPrbMapEnabled) {
                                memcpy(ptr, p_o_xu_cfg->p_RunSlotPrbMap[XRAN_DIR_DL][j][i][z], size_of_prb_map);
                            } else {
                                memcpy(ptr, p_o_xu_cfg->p_PrbMapDl[mu], size_of_prb_map);
                            }
                        }
                    }
                }

                /* C-plane Tx */
                eInterfaceType = XRANCP_PRB_MAP_IN_TX;
                for(j = 0;j < XRAN_N_FE_BUF_LEN; j++) {
                    for(z = 0; z < xran_max_antenna_nr; z++){
                        psIoCtrl->io_buff_perMu[mu].sFHCpTxPrbMapBbuIoBufCtrl[j][i][z].bValid = 0;
                        psIoCtrl->io_buff_perMu[mu].sFHCpTxPrbMapBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
                        psIoCtrl->io_buff_perMu[mu].sFHCpTxPrbMapBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
                        psIoCtrl->io_buff_perMu[mu].sFHCpTxPrbMapBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
                        psIoCtrl->io_buff_perMu[mu].sFHCpTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
                        psIoCtrl->io_buff_perMu[mu].sFHCpTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &psIoCtrl->io_buff_perMu[mu].sFrontHaulCpTxPrbMapBbuIoBufCtrl[j][i][z];

                        psIoCtrl->io_buff_perMu[mu].sFHCpTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nElementLenInBytes = size_of_prb_map;
                        psIoCtrl->io_buff_perMu[mu].sFHCpTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nNumberOfElements = 1;
                        psIoCtrl->io_buff_perMu[mu].sFHCpTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nOffsetInBytes = 0;
                        status = xran_bm_allocate_buffer(psBbuIo->nInstanceHandle[o_xu_id][i],psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],&ptr, &mb);
                        if(XRAN_STATUS_SUCCESS != status) {
                            rte_panic("Failed at  xran_bm_allocate_buffer , status %d\n",status);
                        }
                        psIoCtrl->io_buff_perMu[mu].sFHCpTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->pData = (uint8_t *)ptr;
                        psIoCtrl->io_buff_perMu[mu].sFHCpTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->pCtrl = (void *)mb;
                        if(ptr){
                            struct xran_prb_map * p_rb_map = (struct xran_prb_map *)ptr;
                            memset(p_rb_map, 0, size_of_prb_map);

                            if(p_o_xu_cfg->RunSlotPrbMapEnabled) {
                                memcpy(ptr, p_o_xu_cfg->p_RunSlotPrbMap[XRAN_DIR_DL][j][i][z], size_of_prb_map);
                            } else {
                                xran_init_PrbMap_from_cfg(p_o_xu_cfg->p_PrbMapDl[mu], ptr, p_o_xu_cfg->mtu);
                            }
                        }
                    } /* antenna loop */
                } /* XRAN_N_FE_BUF_LEN loop */
            } /* if RU */
        } /* CC loop */

        // add prach rx buffer
        eInterfaceType = XRANFTHRACH_IN;
        for(i = 0; i<nSectorNum; i++)
        {
            for(j = 0;j < XRAN_N_FE_BUF_LEN; j++)
            {
                for(z = 0; z < xran_max_antenna_nr_prach; z++){
                    psIoCtrl->io_buff_perMu[mu].sFHPrachRxBbuIoBufCtrl[j][i][z].bValid = 0;
                    psIoCtrl->io_buff_perMu[mu].sFHPrachRxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
                    psIoCtrl->io_buff_perMu[mu].sFHPrachRxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
                    psIoCtrl->io_buff_perMu[mu].sFHPrachRxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
                    psIoCtrl->io_buff_perMu[mu].sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = xran_max_antenna_nr_prach; // ant number.
                    psIoCtrl->io_buff_perMu[mu].sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &psIoCtrl->io_buff_perMu[mu].sFHPrachRxBuffers[j][i][z][0];
                    for(k = 0; k< XRAN_NUM_OF_SYMBOL_PER_SLOT; k++)
                    {
                        psIoCtrl->io_buff_perMu[mu].sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nElementLenInBytes = PRACH_PLAYBACK_BUFFER_BYTES;
                        psIoCtrl->io_buff_perMu[mu].sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nNumberOfElements = 1;
                        psIoCtrl->io_buff_perMu[mu].sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nOffsetInBytes = 0;

                        if (p_o_xu_cfg->appMode == APP_O_RU) {
                            status = xran_bm_allocate_buffer(psBbuIo->nInstanceHandle[o_xu_id][i],psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],&ptr, &mb);
                            if(XRAN_STATUS_SUCCESS != status) {
                                rte_panic("Failed at  xran_bm_allocate_buffer, status %d\n",status);
                            }
                            psIoCtrl->io_buff_perMu[mu].sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pData = (uint8_t *)ptr;
                            psIoCtrl->io_buff_perMu[mu].sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pCtrl = (void *)mb;
                            if(ptr){
                                u32dptr = (uint32_t*)(ptr);
                                memset(u32dptr, 0x0, PRACH_PLAYBACK_BUFFER_BYTES);
                            }
                        }
                    }
                }
            }
        }
        // add prach prbmap buffer
        eInterfaceType = XRANFTHRACH_PRB_MAP_IN;
        for(i = 0; i<nSectorNum; i++)
        {
            for(j = 0;j < XRAN_N_FE_BUF_LEN; j++)
            {
                for(z = 0; z < xran_max_antenna_nr_prach; z++){
                    psIoCtrl->io_buff_perMu[mu].sFHPrachRxPrbMapBbuIoBufCtrl[j][i][z].bValid = 0;
                    psIoCtrl->io_buff_perMu[mu].sFHPrachRxPrbMapBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
                    psIoCtrl->io_buff_perMu[mu].sFHPrachRxPrbMapBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
                    psIoCtrl->io_buff_perMu[mu].sFHPrachRxPrbMapBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
                    psIoCtrl->io_buff_perMu[mu].sFHPrachRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
                    psIoCtrl->io_buff_perMu[mu].sFHPrachRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &psIoCtrl->io_buff_perMu[mu].sFHPrachRxPrbMapBuffers[j][i][z];
                    psIoCtrl->io_buff_perMu[mu].sFHPrachRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nElementLenInBytes = size_of_prb_map;
                    psIoCtrl->io_buff_perMu[mu].sFHPrachRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nNumberOfElements = 1;
                    psIoCtrl->io_buff_perMu[mu].sFHPrachRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nOffsetInBytes = 0;
                    status = xran_bm_allocate_buffer(psBbuIo->nInstanceHandle[o_xu_id][i],psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],&ptr, &mb);
                    if(XRAN_STATUS_SUCCESS != status) {
                        rte_panic("Failed at  xran_bm_allocate_buffer, status %d\n",status);
                    }
                    psIoCtrl->io_buff_perMu[mu].sFHPrachRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->pData = (uint8_t *)ptr;
                    psIoCtrl->io_buff_perMu[mu].sFHPrachRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->pCtrl = (void *)mb;
                    if(ptr){
                        u32dptr = (uint32_t*)(ptr);
                        memset(u32dptr, 0x0, size_of_prb_map);
                    }
                }
            }
        }

        /* add SRS rx buffer */
        printf("%s:%d: xran_max_ant_array_elm_nr %d\n", __FUNCTION__, __LINE__, xran_max_ant_array_elm_nr);
        for(i = 0; i<nSectorNum && xran_max_ant_array_elm_nr; i++) {
            eInterfaceType = XRANSRS_IN;
            for(j = 0; j < XRAN_N_FE_BUF_LEN; j++) {
                for(z = 0; z < xran_max_ant_array_elm_nr; z++){
                    psIoCtrl->io_buff_perMu[mu].sFHSrsRxBbuIoBufCtrl[j][i][z].bValid = 0;
                    psIoCtrl->io_buff_perMu[mu].sFHSrsRxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
                    psIoCtrl->io_buff_perMu[mu].sFHSrsRxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
                    psIoCtrl->io_buff_perMu[mu].sFHSrsRxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
                    psIoCtrl->io_buff_perMu[mu].sFHSrsRxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = xran_max_ant_array_elm_nr; /* ant number */
                    psIoCtrl->io_buff_perMu[mu].sFHSrsRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &psIoCtrl->io_buff_perMu[mu].sFHSrsRxBuffers[j][i][z][0];
                    for(k = 0; k < XRAN_MAX_NUM_OF_SRS_SYMBOL_PER_SLOT; k++)
                    {
                        psIoCtrl->io_buff_perMu[mu].sFHSrsRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nElementLenInBytes = nSW_ToFpga_FTH_TxBufferLen;
                        psIoCtrl->io_buff_perMu[mu].sFHSrsRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nNumberOfElements = 1;
                        psIoCtrl->io_buff_perMu[mu].sFHSrsRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nOffsetInBytes = 0;
                        status = xran_bm_allocate_buffer(psBbuIo->nInstanceHandle[o_xu_id][i],psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],&ptr, &mb);
                        if(XRAN_STATUS_SUCCESS != status) {
                            rte_panic("Failed at  xran_bm_allocate_buffer, status %d\n",status);
                        }
                        psIoCtrl->io_buff_perMu[mu].sFHSrsRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pData = (uint8_t *)ptr;
                        psIoCtrl->io_buff_perMu[mu].sFHSrsRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pCtrl = (void *)mb;
                        if(ptr){
                            u32dptr = (uint32_t*)(ptr);
                            memset(u32dptr, 0x0, nSW_ToFpga_FTH_TxBufferLen);
                        }
                    }
                }
            }

            eInterfaceType = XRANSRS_PRB_MAP_IN;
            for(j = 0;j < XRAN_N_FE_BUF_LEN; j++) {
                for(z = 0; z < xran_max_ant_array_elm_nr; z++) {
                    psIoCtrl->io_buff_perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].bValid = 0;
                    psIoCtrl->io_buff_perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
                    psIoCtrl->io_buff_perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
                    psIoCtrl->io_buff_perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
                    psIoCtrl->io_buff_perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
                    psIoCtrl->io_buff_perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &psIoCtrl->io_buff_perMu[mu].sFHSrsRxPrbMapBuffers[j][i][z];

                    psIoCtrl->io_buff_perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nElementLenInBytes = size_of_prb_map;
                    psIoCtrl->io_buff_perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nNumberOfElements = 1;
                    psIoCtrl->io_buff_perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nOffsetInBytes = 0;
                    status = xran_bm_allocate_buffer(psBbuIo->nInstanceHandle[o_xu_id][i],psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],&ptr, &mb);
                    if(XRAN_STATUS_SUCCESS != status) {
                        rte_panic("Failed at  xran_bm_allocate_buffer , status %d\n",status);
                    }
                    psIoCtrl->io_buff_perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->pData = (uint8_t *)ptr;
                    psIoCtrl->io_buff_perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->pCtrl = (void *)mb;

                    if(ptr) {
                        struct xran_prb_map * p_rb_map = (struct xran_prb_map *)ptr;
                        memset(p_rb_map, 0, size_of_prb_map);
                        if(p_o_xu_cfg->RunSlotPrbMapEnabled)
                        {
                            xran_init_PrbMap_from_cfg(p_o_xu_cfg->p_RunSrsSlotPrbMap[XRAN_DIR_UL][j][i][z], ptr, p_o_xu_cfg->mtu);
                        }
                        else
                        {
                            xran_init_PrbMap_from_cfg(p_o_xu_cfg->p_PrbMapSrs, ptr, p_o_xu_cfg->mtu);
                        }
                    }
                }
            }
        }

        /* CSI-RS buffers */
        for(i = 0; i<nSectorNum && (p_o_xu_cfg->csirsEnable); i++) {
            /*Parameters to initialize memory for CSI-RS prbMap*/
            numPrbElm = xran_get_num_prb_elm(p_o_xu_cfg->p_PrbMapCsiRs, p_o_xu_cfg->mtu);
            size_of_prb_map  = sizeof(struct xran_prb_map) + sizeof(struct xran_prb_elm)*(numPrbElm);
            eInterfaceType = XRANCSIRS_TX;
            for(j = 0; j < XRAN_N_FE_BUF_LEN; j++) {
                for(z = 0; z < XRAN_MAX_CSIRS_PORTS; z++){
                    psIoCtrl->io_buff_perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][i][z].bValid = 0;
                    psIoCtrl->io_buff_perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
                    psIoCtrl->io_buff_perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
                    psIoCtrl->io_buff_perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
                    psIoCtrl->io_buff_perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_MAX_CSIRS_PORTS; /* ports number */
                    psIoCtrl->io_buff_perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &psIoCtrl->io_buff_perMu[mu].sFHCsirsTxBuffers[j][i][z][0];
                    for(k = 0; k < XRAN_NUM_OF_SYMBOL_PER_SLOT; k++)
                    {
                        psIoCtrl->io_buff_perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nElementLenInBytes = nSW_ToFpga_FTH_TxBufferLen;
                        psIoCtrl->io_buff_perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nNumberOfElements = 1;
                        psIoCtrl->io_buff_perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nOffsetInBytes = 0;
                        status = xran_bm_allocate_buffer(psBbuIo->nInstanceHandle[o_xu_id][i],psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],&ptr, &mb);
                        if(XRAN_STATUS_SUCCESS != status) {
                            rte_panic("Failed at  xran_bm_allocate_buffer, status %d\n",status);
                        }
                        psIoCtrl->io_buff_perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pData = (uint8_t *)ptr;
                        psIoCtrl->io_buff_perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pCtrl = (void *)mb;
                        if(ptr){
                            u32dptr = (uint32_t*)(ptr);
                            memset(u32dptr, 0x0, nSW_ToFpga_FTH_TxBufferLen);
                        }
                    }
                }
            }

            eInterfaceType = XRANCSIRS_PRB_MAP_TX;
            for(j = 0;j < XRAN_N_FE_BUF_LEN; j++) {
                for(z = 0; z < p_o_xu_cfg->nCsiPorts; z++) {
                    psIoCtrl->io_buff_perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][i][z].bValid = 0;
                    psIoCtrl->io_buff_perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
                    psIoCtrl->io_buff_perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
                    psIoCtrl->io_buff_perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
                    psIoCtrl->io_buff_perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
                    psIoCtrl->io_buff_perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &psIoCtrl->io_buff_perMu[mu].sFHCsirsTxPrbMapBuffers[j][i][z];

                    psIoCtrl->io_buff_perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nElementLenInBytes = size_of_prb_map;
                    psIoCtrl->io_buff_perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nNumberOfElements = 1;
                    psIoCtrl->io_buff_perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nOffsetInBytes = 0;
                    status = xran_bm_allocate_buffer(psBbuIo->nInstanceHandle[o_xu_id][i],psBbuIo->nBufPoolIndex[o_xu_id][nSectorIndex[i]][eInterfaceType],&ptr, &mb);
                    if(XRAN_STATUS_SUCCESS != status) {
                        rte_panic("Failed at  xran_bm_allocate_buffer , status %d\n",status);
                    }
                    psIoCtrl->io_buff_perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->pData = (uint8_t *)ptr;
                    psIoCtrl->io_buff_perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->pCtrl = (void *)mb;

                    if(ptr) {
                        struct xran_prb_map * p_rb_map = (struct xran_prb_map *)ptr;
                        memset(p_rb_map, 0, size_of_prb_map);
                        xran_init_PrbMap_from_cfg(p_o_xu_cfg->p_PrbMapCsiRs, ptr, p_o_xu_cfg->mtu);
                    }
                }
            }
        }

        for(i=0; i<nSectorNum; i++)
        {
            for(j=0; j<XRAN_N_FE_BUF_LEN; j++)
            {
                for(z = 0; z < XRAN_MAX_ANTENNA_NR; z++){
                    pFthTxBuffer[i][z][j]       = NULL;
                    pFthTxPrbMapBuffer[i][z][j] = NULL;
                    pFthRxBuffer[i][z][j]       = NULL;
                    pFthRxPrbMapBuffer[i][z][j] = NULL;
                    pFthRxRachBuffer[i][z][j]   = NULL;
                    pFthRxRachBufferDecomp[i][z][j]   = NULL;
                    pFthRxCpPrbMapBuffer[i][z][j] = NULL;
                    pFthTxCpPrbMapBuffer[i][z][j] = NULL;
                }
                for(z = 0; z < XRAN_MAX_ANT_ARRAY_ELM_NR; z++){
                    pFthRxSrsBuffer[i][z][j] = NULL;
                    pFthRxSrsPrbMapBuffer[i][z][j] = NULL;
                }
            }
        }

        for(i=0; i<nSectorNum; i++)
        {
            for(j=0; j<XRAN_N_FE_BUF_LEN; j++)
            {
                for(z = 0; z < XRAN_MAX_ANTENNA_NR; z++){
                    pFthTxBuffer[i][z][j]     = &(psIoCtrl->io_buff_perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList);
                    pFthTxPrbMapBuffer[i][z][j]     = &(psIoCtrl->io_buff_perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList);
                    pFthRxBuffer[i][z][j]     = &(psIoCtrl->io_buff_perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList);
                    pFthRxPrbMapBuffer[i][z][j]     = &(psIoCtrl->io_buff_perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList);
                    pFthRxRachBuffer[i][z][j] = &(psIoCtrl->io_buff_perMu[mu].sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList);
                    pFthRxRachBufferDecomp[i][z][j] = &(psIoCtrl->io_buff_perMu[mu].sFHPrachRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList);
                    pFthRxCpPrbMapBuffer[i][z][j]     = &(psIoCtrl->io_buff_perMu[mu].sFHCpRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList);
                    pFthTxCpPrbMapBuffer[i][z][j]     = &(psIoCtrl->io_buff_perMu[mu].sFHCpTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList);
                }

                for(z = 0; z < XRAN_MAX_ANT_ARRAY_ELM_NR && xran_max_ant_array_elm_nr; z++){
                    pFthRxSrsBuffer[i][z][j] = &(psIoCtrl->io_buff_perMu[mu].sFHSrsRxBbuIoBufCtrl[j][i][z].sBufferList);
                    pFthRxSrsPrbMapBuffer[i][z][j] = &(psIoCtrl->io_buff_perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList);
                }

                for(z = 0; z < XRAN_MAX_CSIRS_PORTS && (p_o_xu_cfg->csirsEnable); z++){
                    pFthTxCsirsBuffer[i][z][j] = &(psIoCtrl->io_buff_perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][i][z].sBufferList);
                    pFthTxCsirsPrbMapBuffer[i][z][j] = &(psIoCtrl->io_buff_perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList);
                }
            }
        }

        for (i = 0; i<nSectorNum; i++)
        {
            if(NULL != psBbuIo->nInstanceHandle[o_xu_id][i])
            {
                /* add pusch callback */
                psBbuIo->RxCbTag[o_xu_id][i].oXuId = o_xu_id;
                psBbuIo->RxCbTag[o_xu_id][i].cellId = i;
                psBbuIo->RxCbTag[o_xu_id][i].symbol  = 0;
                psBbuIo->RxCbTag[o_xu_id][i].slotiId = 0;
                if(psBbuIo->bbu_offload)
                    xran_5g_fronthault_config (psBbuIo->nInstanceHandle[o_xu_id][i],
                            pFthTxBuffer[i],
                            pFthTxPrbMapBuffer[i],
                            pFthRxBuffer[i],
                            pFthRxPrbMapBuffer[i],
                            app_io_xran_fh_bbu_rx_callback,  &psBbuIo->RxCbTag[o_xu_id][i], mu);
                else
                    xran_5g_fronthault_config (psBbuIo->nInstanceHandle[o_xu_id][i],
                            pFthTxBuffer[i],
                            pFthTxPrbMapBuffer[i],
                            pFthRxBuffer[i],
                            pFthRxPrbMapBuffer[i],
                            app_io_xran_fh_rx_callback,  &psBbuIo->RxCbTag[o_xu_id][i],mu);

                psBbuIo->BfwCbTag[o_xu_id][i].oXuId = o_xu_id;
                psBbuIo->BfwCbTag[o_xu_id][i].cellId = i;
                psBbuIo->BfwCbTag[o_xu_id][i].symbol  = 0;
                psBbuIo->BfwCbTag[o_xu_id][i].slotiId = 0;
#if 0
                if(psBbuIo->bbu_offload)
                    xran_5g_bfw_config(psBbuIo->nInstanceHandle[o_xu_id][i],
                            pFthRxCpPrbMapBuffer[i],
                            pFthTxCpPrbMapBuffer[i],
                            app_io_xran_fh_bbu_rx_bfw_callback,&psBbuIo->BfwCbTag[o_xu_id][i]);
                else
#endif
                    xran_5g_bfw_config(psBbuIo->nInstanceHandle[o_xu_id][i],
                            pFthRxCpPrbMapBuffer[i],
                            pFthTxCpPrbMapBuffer[i],
                            app_io_xran_fh_rx_bfw_callback, &psBbuIo->BfwCbTag[o_xu_id][i], mu);

                psBbuIo->PrachCbTag[o_xu_id][i].oXuId = o_xu_id;
                psBbuIo->PrachCbTag[o_xu_id][i].cellId = i;
                psBbuIo->PrachCbTag[o_xu_id][i].symbol  = 0;
                psBbuIo->PrachCbTag[o_xu_id][i].slotiId = 0;
                if(psBbuIo->bbu_offload)
                    xran_5g_prach_req(psBbuIo->nInstanceHandle[o_xu_id][i], pFthRxRachBuffer[i],pFthRxRachBufferDecomp[i],
                            app_io_xran_fh_bbu_rx_prach_callback, &psBbuIo->PrachCbTag[o_xu_id][i], mu);
                else
                    xran_5g_prach_req(psBbuIo->nInstanceHandle[o_xu_id][i], pFthRxRachBuffer[i],pFthRxRachBufferDecomp[i],
                            app_io_xran_fh_rx_prach_callback, &psBbuIo->PrachCbTag[o_xu_id][i], mu);

                if(xran_max_ant_array_elm_nr)
                {
                    psBbuIo->SrsCbTag[o_xu_id][i].oXuId = o_xu_id;
                    psBbuIo->SrsCbTag[o_xu_id][i].cellId = i;
                    psBbuIo->SrsCbTag[o_xu_id][i].symbol  = 0;
                    psBbuIo->SrsCbTag[o_xu_id][i].slotiId = 0;
                    if(psBbuIo->bbu_offload)
                        xran_5g_srs_req(psBbuIo->nInstanceHandle[o_xu_id][i], pFthRxSrsBuffer[i], pFthRxSrsPrbMapBuffer[i],
                                app_io_xran_fh_bbu_rx_srs_callback,&psBbuIo->SrsCbTag[o_xu_id][i], mu);
                    else
                        xran_5g_srs_req(psBbuIo->nInstanceHandle[o_xu_id][i], pFthRxSrsBuffer[i], pFthRxSrsPrbMapBuffer[i],
                                app_io_xran_fh_rx_srs_callback, &psBbuIo->SrsCbTag[o_xu_id][i], mu);
                }
                /* CSI-RS buffers shared */
                if(p_o_xu_cfg->csirsEnable)
                {
                    psBbuIo->CsirsCbTag[o_xu_id][i].oXuId = o_xu_id;
                    psBbuIo->CsirsCbTag[o_xu_id][i].cellId = i;
                    psBbuIo->CsirsCbTag[o_xu_id][i].symbol  = 0;
                    psBbuIo->CsirsCbTag[o_xu_id][i].slotiId = 0;

                    if(psBbuIo->bbu_offload)
                        xran_5g_csirs_config(psBbuIo->nInstanceHandle[o_xu_id][i], pFthTxCsirsBuffer[i], pFthTxCsirsPrbMapBuffer[i],
                            app_io_xran_fh_bbu_rx_csirs_callback, &psBbuIo->CsirsCbTag[o_xu_id][i], mu);
                    else
                        xran_5g_csirs_config(psBbuIo->nInstanceHandle[o_xu_id][i], pFthTxCsirsBuffer[i], pFthTxCsirsPrbMapBuffer[i],
                            app_io_xran_fh_rx_csirs_callback, &psBbuIo->CsirsCbTag[o_xu_id][i], mu);
                }
            }
        }

    } //mu loop

    return status;
} /* app_io_xran_interface */

int32_t
app_io_xran_ext_type1_populate(struct xran_prb_elm* p_pRbMapElm, char *p_bfw_buffer, uint32_t mtu, uint16_t* numSetBFW_total)
{
    xran_status_t status = XRAN_STATUS_SUCCESS;

    int16_t  ext_len;
    int16_t  ext_sec_total = 0;
    int8_t * ext_buf = NULL;
    int8_t * ext_buf_start = NULL;

    ext_len = p_pRbMapElm->bf_weight.maxExtBufSize = mtu;    /* MAX_RX_LEN; */  /* Maximum space of external buffer */
    if (p_pRbMapElm->bf_weight.p_ext_start)
        ext_buf = (int8_t *)p_pRbMapElm->bf_weight.p_ext_start;
    else
        ext_buf = (int8_t *)xran_malloc("ext1_buf", p_pRbMapElm->bf_weight.maxExtBufSize, RTE_CACHE_LINE_SIZE);

    if(ext_buf == NULL)
        rte_panic("xran_malloc return NULL [sz %d]\n", p_pRbMapElm->bf_weight.maxExtBufSize);

    if(ext_buf) {
        ext_buf_start = ext_buf;
        ext_buf += (RTE_PKTMBUF_HEADROOM +
                    sizeof(struct xran_ecpri_hdr) +
                    sizeof(struct xran_cp_radioapp_section1_header));

        ext_len -= (RTE_PKTMBUF_HEADROOM +
                    sizeof(struct xran_ecpri_hdr) +
                    sizeof(struct xran_cp_radioapp_section1_header));

        ext_sec_total =  xran_cp_populate_section_ext_1((int8_t *)ext_buf,
                                    ext_len,
                                    (int16_t *) (p_bfw_buffer + (*numSetBFW_total*p_pRbMapElm->bf_weight.nAntElmTRx)*4),
                                    p_pRbMapElm);
        if(ext_sec_total > 0) {
            p_pRbMapElm->bf_weight.p_ext_start    = ext_buf_start;
            p_pRbMapElm->bf_weight.p_ext_section  = ext_buf;
            p_pRbMapElm->bf_weight.ext_section_sz = ext_sec_total;
        } else
            rte_panic("xran_cp_populate_section_ext_1 return error [%d]\n", ext_sec_total);
    } else {
        rte_panic("xran_malloc return NULL\n");
    }

    return status;
}

int32_t
app_io_xran_ext_type11_populate(struct xran_prb_elm* p_pRbMapElm, char *p_tx_dl_bfw_buffer, uint32_t mtu)
{
    xran_status_t status = XRAN_STATUS_SUCCESS;

    int32_t i;
    uint8_t *extbuf;
    int32_t n_max_set_bfw;

    p_pRbMapElm->bf_weight.maxExtBufSize = mtu;    /* MAX_RX_LEN; */  /* Maximum space of external buffer */
    if (p_pRbMapElm->bf_weight.p_ext_start)
        extbuf = (uint8_t *)p_pRbMapElm->bf_weight.p_ext_start;
    else
        extbuf = (uint8_t*)xran_malloc("ext11_buf",p_pRbMapElm->bf_weight.maxExtBufSize, RTE_CACHE_LINE_SIZE);
    if(extbuf == NULL)
        rte_panic("xran_malloc return NULL [sz %d]\n", p_pRbMapElm->bf_weight.maxExtBufSize);

    /* Check BFWs can be fit with MTU size */
    n_max_set_bfw = xran_cp_estimate_max_set_bfws(p_pRbMapElm->bf_weight.nAntElmTRx,
                                p_pRbMapElm->bf_weight.bfwIqWidth,
                                p_pRbMapElm->bf_weight.bfwCompMeth,
                                mtu);

    if(p_pRbMapElm->bf_weight.numSetBFWs > n_max_set_bfw) {
        /* PRB elm doesn't fit into packet MTU size */
        rte_panic("BFWs are too large with MTU %d! (cfg:%d / max:%d)\n",
                   mtu, p_pRbMapElm->bf_weight.numSetBFWs, n_max_set_bfw);

    }

    /* Configure source buffer and beam ID of BFWs */
#ifdef XRAN_CP_BF_WEIGHT_STRUCT_OPT
    for(i = 0; i < p_pRbMapElm->bf_weight.numSetBFWs; i++) {
        p_pRbMapElm->bf_weight.bfw[0].pBFWs[i] = (uint8_t *)(p_tx_dl_bfw_buffer + p_pRbMapElm->bf_weight.nAntElmTRx*2*i);
        p_pRbMapElm->bf_weight.bfw[0].beamId[i] = 0x7000+i;
    }
#else
    for(i = 0; i < p_pRbMapElm->bf_weight.numSetBFWs; i++) {
        p_pRbMapElm->bf_weight.bfw[i].pBFWs = (uint8_t *)(p_tx_dl_bfw_buffer + p_pRbMapElm->bf_weight.nAntElmTRx*2*i);
        p_pRbMapElm->bf_weight.bfw[i].beamId = 0x7000+i;
    }
#endif
    n_max_set_bfw = xran_cp_prepare_ext11_bfws(p_pRbMapElm->bf_weight.numSetBFWs,
                                p_pRbMapElm->bf_weight.nAntElmTRx,
                                p_pRbMapElm->bf_weight.bfwIqWidth,
                                p_pRbMapElm->bf_weight.bfwCompMeth,
                                extbuf,
                                p_pRbMapElm->bf_weight.maxExtBufSize,
                                p_pRbMapElm->bf_weight.bfw);
    if(n_max_set_bfw > 0) {
        p_pRbMapElm->bf_weight.ext_section_sz   = n_max_set_bfw;
        p_pRbMapElm->bf_weight.p_ext_start      = (int8_t *)extbuf;
    } else
        rte_panic("Fail to prepare BFWs for extension 11!\n");

    return status;
}

/** c-plane DL */
int32_t
app_io_xran_iq_content_init_dl_cp(uint8_t  appMode, struct xran_fh_config  *pXranConf,
                                  struct bbu_xran_io_if *psBbuIo, struct xran_io_shared_ctrl *psIoCtrl, struct o_xu_buffers * p_iq,
                                  int32_t cc_id, int32_t ant_id, int32_t sym_id, int32_t target_tti, int32_t flowId, uint8_t mu)
{
    int32_t status = 0;
    struct xran_prb_map* pRbMap = NULL;
    char* dl_bfw_pos = NULL;

    int32_t tti_dst =  target_tti % XRAN_N_FE_BUF_LEN;
    int32_t tti_src =  target_tti % p_iq->numSlots;
    int32_t tx_dl_bfw_buffer_position = tti_src * (pXranConf->perMu[mu].nDLRBs*pXranConf->nAntElmTRx)*4;
    uint16_t numSetBFW_total = 0;

    if(p_iq->buff_perMu[mu].p_tx_play_buffer[flowId]) {
        cc_id = cc_id % XRAN_MAX_SECTOR_NR;
        ant_id = ant_id % XRAN_MAX_ANTENNA_NR;
        pRbMap = (struct xran_prb_map *) psIoCtrl->io_buff_perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[tti_dst][cc_id][ant_id].sBufferList.pBuffers->pData;
        dl_bfw_pos  = ((char*)p_iq->buff_perMu[mu].p_tx_dl_bfw_buffer[flowId]) + tx_dl_bfw_buffer_position;
        if(pRbMap) {
            if(pXranConf->ru_conf.xranCat == XRAN_CATEGORY_B
                && appMode == APP_O_DU
                && sym_id == 0) { /* BFWs are per slot */

                int32_t idxElm = 0;
                struct xran_prb_elm* p_pRbMapElm = NULL;

                for(idxElm = 0;  idxElm < pRbMap->nPrbElm; idxElm++) {
                    p_pRbMapElm = &pRbMap->prbMap[idxElm];
                    p_pRbMapElm->bf_weight.nAntElmTRx = pXranConf->nAntElmTRx;

                    if(p_pRbMapElm->BeamFormingType == XRAN_BEAM_WEIGHT && p_pRbMapElm->bf_weight_update) {
                        if(p_pRbMapElm->bf_weight.extType == 1) {
                            app_io_xran_ext_type1_populate(p_pRbMapElm, dl_bfw_pos, app_io_xran_fh_init.mtu, &numSetBFW_total);
                        } else {
                            app_io_xran_ext_type11_populate(p_pRbMapElm, dl_bfw_pos, app_io_xran_fh_init.mtu);
                        }
                    }
                    numSetBFW_total += p_pRbMapElm->bf_weight.numSetBFWs;
                }
            }
        } else {
                printf("DL pRbMap ==NULL [%d][%d][%d]\n", tti_dst, cc_id, ant_id);
            exit(-1);
        }
    } else {
        //printf("flowId %d\n", flowId);
    }

    return status;
}

/** C-plane UL */
int32_t
app_io_xran_iq_content_init_ul_cp(uint8_t  appMode, struct xran_fh_config  *pXranConf,
                                  struct bbu_xran_io_if *psBbuIo, struct xran_io_shared_ctrl *psIoCtrl, struct o_xu_buffers * p_iq,
                                  int32_t cc_id, int32_t ant_id, int32_t sym_id, int32_t target_tti, int32_t flowId, uint8_t mu)
{
    struct xran_prb_map* pRbMap = NULL;
    char* ul_bfw_pos = NULL;

    int32_t tti_dst =  target_tti % XRAN_N_FE_BUF_LEN;
    int32_t tti_src =  target_tti % p_iq->numSlots;
    int32_t tx_ul_bfw_buffer_position = tti_src * (pXranConf->perMu[mu].nULRBs*pXranConf->nAntElmTRx)*4;
    
    uint16_t numSetBFW_total = 0;

    cc_id = cc_id % XRAN_MAX_SECTOR_NR;
    ant_id = ant_id % XRAN_MAX_ANTENNA_NR;

    pRbMap = (struct xran_prb_map *) psIoCtrl->io_buff_perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[tti_dst][cc_id][ant_id].sBufferList.pBuffers->pData;
    ul_bfw_pos =  ((char*)p_iq->buff_perMu[mu].p_tx_ul_bfw_buffer[flowId]) + tx_ul_bfw_buffer_position;
    if(pRbMap) {
        if(pXranConf->ru_conf.xranCat == XRAN_CATEGORY_B
                    && appMode == APP_O_DU
                    && sym_id == 0) {
            int32_t idxElm = 0;
            struct xran_prb_elm* p_pRbMapElm = NULL;

            for(idxElm = 0;  idxElm < pRbMap->nPrbElm; idxElm++) {
                p_pRbMapElm = &pRbMap->prbMap[idxElm];
                p_pRbMapElm->bf_weight.nAntElmTRx = pXranConf->nAntElmTRx;

                if(p_pRbMapElm->BeamFormingType == XRAN_BEAM_WEIGHT && p_pRbMapElm->bf_weight_update) {
                    if(p_pRbMapElm->bf_weight.extType == 1) {
                        app_io_xran_ext_type1_populate(p_pRbMapElm, ul_bfw_pos, app_io_xran_fh_init.mtu, &numSetBFW_total);
                    } else {
                        app_io_xran_ext_type11_populate(p_pRbMapElm, ul_bfw_pos, app_io_xran_fh_init.mtu);
                    }
                } /* if(p_pRbMapElm->BeamFormingType == XRAN_BEAM_WEIGHT && p_pRbMapElm->bf_weight_update) */
                numSetBFW_total += p_pRbMapElm->bf_weight.numSetBFWs;
            } /* for(idxElm = 0;  idxElm < pRbMap->nPrbElm; idxElm++) */
        }
    } else {
        rte_panic("DL pRbMap ==NULL\n");
    }

    return 0;
}

int32_t
app_io_xran_iq_content_init_up_tx(uint8_t  appMode, struct xran_fh_config  *pXranConf,
                                  struct bbu_xran_io_if *psBbuIo, struct xran_io_shared_ctrl *psIoCtrl, struct o_xu_buffers * p_iq,
                                  int32_t cc_id, int32_t ant_id, int32_t sym_id, int32_t target_tti, int32_t flowId, uint8_t mu)
{
    char *pos = NULL;
    void *ptr = NULL;
    uint8_t* u8dptr = NULL;
    struct xran_prb_map* pRbMap = NULL;
    enum xran_comp_hdr_type staticEn = XRAN_COMP_HDR_TYPE_DYNAMIC;

    bool isNb375=false;
    if (pXranConf != NULL)
    {
        if(APP_O_RU == appMode && mu == XRAN_NBIOT_MU && XRAN_NBIOT_UL_SCS_3_75 ==pXranConf->perMu[mu].nbIotUlScs)
        {
            isNb375=true;
        }

        int32_t tti_dst =  target_tti % XRAN_N_FE_BUF_LEN;
        int32_t tti_src =  target_tti % p_iq->numSlots;
        int32_t tx_play_buffer_position;

        tx_play_buffer_position = tti_src * (XRAN_NUM_OF_SYMBOL_PER_SLOT*pXranConf->perMu[mu].nDLRBs*N_SC_PER_PRB(isNb375)*2L*p_iq->iq_dl_bit)
                                    + (sym_id * pXranConf->perMu[mu].nDLRBs*N_SC_PER_PRB(isNb375)*2L*p_iq->iq_dl_bit);
        staticEn = pXranConf->ru_conf.xranCompHdrType;

        pRbMap = (struct xran_prb_map *) psIoCtrl->io_buff_perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[tti_dst][cc_id][ant_id].sBufferList.pBuffers->pData;
        pos =  ((char*)p_iq->buff_perMu[mu].p_tx_play_buffer[flowId]) + tx_play_buffer_position;
        ptr = psIoCtrl->io_buff_perMu[mu].sFrontHaulTxBbuIoBufCtrl[tti_dst][cc_id][ant_id].sBufferList.pBuffers[sym_id].pData;

        if(ptr && pos) {
            int32_t idxElm = 0;
            u8dptr = (uint8_t*)ptr;
            int16_t payload_len = 0;

            uint8_t  *dst = (uint8_t *)u8dptr;
            uint8_t  *src = (uint8_t *)pos;
            uint16_t num_sections, idx, comp_method;
            uint16_t prb_per_section;
            struct xran_prb_elm* p_prbMapElm = &pRbMap->prbMap[idxElm];
            struct xran_prb_elm* p_prev_prbElm;
            dst =  xran_add_hdr_offset(dst, ((staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC) ? p_prbMapElm->compMethod : XRAN_COMPMETHOD_NONE));

            for (idxElm = 0;  idxElm < pRbMap->nPrbElm; idxElm++) {
                struct xran_section_desc *p_sec_desc = NULL;
                p_prbMapElm = &pRbMap->prbMap[idxElm];
                p_sec_desc =  &p_prbMapElm->sec_desc[sym_id];
                /*Same set of IQ data (one UP packet) for overlapping PRBs with different set of reMasks*/
                if(p_prbMapElm->reMask && idxElm > 0){
                    p_prev_prbElm = &pRbMap->prbMap[idxElm-1];
                    if(p_prbMapElm->UP_nRBStart == p_prev_prbElm->UP_nRBStart && p_prbMapElm->UP_nRBSize == p_prev_prbElm->UP_nRBSize && \
                            p_prbMapElm->nStartSymb == p_prev_prbElm->nStartSymb && p_prbMapElm->numSymb == p_prev_prbElm->numSymb)
                                continue;
                }
                
                if(p_prbMapElm->bf_weight.extType == 1)
                {
                    num_sections = p_prbMapElm->bf_weight.numSetBFWs;
                    prb_per_section = p_prbMapElm->bf_weight.numBundPrb;
                }
                else
                {
                    num_sections = 1;
                    prb_per_section = p_prbMapElm->UP_nRBSize;
                }

                if(p_sec_desc == NULL) {
                    rte_panic ("p_sec_desc == NULL\n");
                }

                /* skip, if not scheduled */
                if(sym_id < p_prbMapElm->nStartSymb || sym_id >= p_prbMapElm->nStartSymb + p_prbMapElm->numSymb){
                    p_sec_desc->iq_buffer_offset = 0;
                    p_sec_desc->iq_buffer_len    = 0;
                    continue;
                }

                src = (uint8_t *)(pos + p_prbMapElm->UP_nRBStart*N_SC_PER_PRB(isNb375)*2L*p_iq->iq_dl_bit);
                p_sec_desc->iq_buffer_offset = RTE_PTR_DIFF(dst, u8dptr);
                p_sec_desc->iq_buffer_len = 0;

                for(idx=0; idx < num_sections ; idx++)
                {
                    //printf("\nidx %hu u8dptr %p dst %p",idx,u8dptr,dst);

                    if((idx+1)*prb_per_section > p_prbMapElm->UP_nRBSize){
                        prb_per_section = (p_prbMapElm->UP_nRBSize - idx*prb_per_section);
                    }

                    if(p_prbMapElm->compMethod == XRAN_COMPMETHOD_NONE) {
                        payload_len = prb_per_section*N_SC_PER_PRB(isNb375)*2L*p_iq->iq_dl_bit;
                        memcpy(dst, src, payload_len);
                    } else if ((p_prbMapElm->compMethod == XRAN_COMPMETHOD_BLKFLOAT) || (p_prbMapElm->compMethod == XRAN_COMPMETHOD_MODULATION)) {
                        struct xranlib_compress_request  bfp_com_req;
                        struct xranlib_compress_response bfp_com_rsp;

                        memset(&bfp_com_req, 0, sizeof(struct xranlib_compress_request));
                        memset(&bfp_com_rsp, 0, sizeof(struct xranlib_compress_response));

                        bfp_com_req.data_in    = (int16_t*)src;
                        bfp_com_req.numRBs     = prb_per_section;
                        bfp_com_req.len        = prb_per_section*N_SC_PER_PRB(isNb375)*2L*p_iq->iq_dl_bit;
                        bfp_com_req.compMethod = p_prbMapElm->compMethod;
                        bfp_com_req.iqWidth    = p_prbMapElm->iqWidth;
                        bfp_com_req.ScaleFactor= p_prbMapElm->ScaleFactor;
                        bfp_com_req.reMask     = p_prbMapElm->reMask;

                        bfp_com_rsp.data_out   = (int8_t*)dst;
                        bfp_com_rsp.len        = 0;

                        xranlib_compress(&bfp_com_req, &bfp_com_rsp);
                        payload_len = bfp_com_rsp.len;

                    } else {
                        printf ("p_prbMapElm->compMethod == %d is not supported\n",
                                p_prbMapElm->compMethod);
                        exit(-1);
                    }

                    if(num_sections != 1)
                        src += prb_per_section*N_SC_PER_PRB(isNb375)*2L*p_iq->iq_dl_bit;

                    /* update RB map for given element */
                    //p_sec_desc->iq_buffer_offset = RTE_PTR_DIFF(dst, u8dptr);
                    p_sec_desc->iq_buffer_len += payload_len;

                    /* add headroom for ORAN headers between IQs for chunk of RBs*/
                    dst += payload_len;
                    if(idx+1 == num_sections) /* Create space for (eth + eCPRI + radio app + section + comp) headers required by next prbElement */
                    {
                        dst  = xran_add_hdr_offset(dst, ((staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC) ? p_prbMapElm->compMethod : XRAN_COMPMETHOD_NONE));
                    }
                    else
                    {
                        /* Create space for section/compression header in current prbElement */
                        //TODO: Check if alignment required for this case
                        dst += sizeof(struct data_section_hdr);
                        p_sec_desc->iq_buffer_len += sizeof(struct data_section_hdr);

                        comp_method = ((staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC) ? p_prbMapElm->compMethod : XRAN_COMPMETHOD_NONE);

                        if( comp_method != XRAN_COMPMETHOD_NONE)
                        {
                            dst += sizeof (struct data_section_compression_hdr);
                            p_sec_desc->iq_buffer_len += sizeof(struct data_section_compression_hdr);
                        }
                    }
                } /*for num_section */
            } /* for (idxElm = 0;  idxElm < pRbMap->nPrbElm; idxElm++) */
        } /* if(ptr && pos) */
        else {
            rte_panic("ptr ==NULL\n");
        }
    } /* if (pXranConf != NULL) */

    return 0;
}

int32_t
app_io_xran_iq_content_init_multi_section_up_tx(uint8_t  appMode, struct xran_fh_config  *pXranConf,
                                  struct bbu_xran_io_if *psBbuIo, struct xran_io_shared_ctrl *psIoCtrl, struct o_xu_buffers * p_iq,
                                  int32_t cc_id, int32_t ant_id, int32_t sym_id, int32_t target_tti, int32_t flowId, uint8_t mu, uint16_t mtu)
{
    char *pos = NULL;
    void *ptr = NULL;
    uint8_t* u8dptr = NULL;
    struct xran_prb_map* pRbMap = NULL;
    enum xran_comp_hdr_type staticEn = XRAN_COMP_HDR_TYPE_DYNAMIC;

    bool isNb375=false;
    if (pXranConf != NULL)
    {
        if(APP_O_RU == appMode && mu == XRAN_NBIOT_MU && XRAN_NBIOT_UL_SCS_3_75 ==pXranConf->perMu[mu].nbIotUlScs)
        {
            isNb375=true;
        }

        int32_t tti_dst =  target_tti % XRAN_N_FE_BUF_LEN;
        int32_t tti_src =  target_tti % p_iq->numSlots;
        int32_t tx_play_buffer_position;

        tx_play_buffer_position = tti_src * (XRAN_NUM_OF_SYMBOL_PER_SLOT*pXranConf->perMu[mu].nDLRBs*N_SC_PER_PRB(isNb375)*2L*p_iq->iq_dl_bit)
                                    + (sym_id * pXranConf->perMu[mu].nDLRBs*N_SC_PER_PRB(isNb375)*2L*p_iq->iq_dl_bit);
        staticEn = pXranConf->ru_conf.xranCompHdrType;


        pRbMap = (struct xran_prb_map *) psIoCtrl->io_buff_perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[tti_dst][cc_id][ant_id].sBufferList.pBuffers->pData;
        pos =  ((char*)p_iq->buff_perMu[mu].p_tx_play_buffer[flowId]) + tx_play_buffer_position;
        ptr = psIoCtrl->io_buff_perMu[mu].sFrontHaulTxBbuIoBufCtrl[tti_dst][cc_id][ant_id].sBufferList.pBuffers[sym_id].pData;

        if(ptr && pos) {
            int32_t idxElm = 0;
            u8dptr = (uint8_t*)ptr;
            int16_t payload_len = 0;
            int16_t pkg_len_temp = 0;
            int16_t pkg_header_len = 0;
            int16_t pkg_len = 0;
            int16_t iq_buffer_len = 0;
            

            uint8_t  *dst = (uint8_t *)u8dptr;
            uint8_t  *src = (uint8_t *)pos;
            uint16_t num_sections, idx, comp_method;
            uint16_t prb_per_section;
            struct xran_prb_elm* p_prbMapElm = &pRbMap->prbMap[idxElm];
            struct xran_prb_elm* p_prev_prbElm;
            dst =  xran_add_hdr_offset(dst, ((staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC) ? p_prbMapElm->compMethod : XRAN_COMPMETHOD_NONE));

            for (idxElm = 0;  idxElm < pRbMap->nPrbElm; idxElm++) {
                struct xran_section_desc *p_sec_desc = NULL;
                p_prbMapElm = &pRbMap->prbMap[idxElm];
                p_sec_desc =  &p_prbMapElm->sec_desc[sym_id];
                /*Same set of IQ data (one UP packet) for overlapping PRBs with different set of reMasks*/
                if(p_prbMapElm->reMask && idxElm > 0){
                    p_prev_prbElm = &pRbMap->prbMap[idxElm-1];
                    if(p_prbMapElm->UP_nRBStart == p_prev_prbElm->UP_nRBStart && p_prbMapElm->UP_nRBSize == p_prev_prbElm->UP_nRBSize && \
                            p_prbMapElm->nStartSymb == p_prev_prbElm->nStartSymb && p_prbMapElm->numSymb == p_prev_prbElm->numSymb)
                                continue;
                }
                
                if(p_prbMapElm->bf_weight.extType == 1)
                {
                    num_sections = p_prbMapElm->bf_weight.numSetBFWs;
                    prb_per_section = p_prbMapElm->bf_weight.numBundPrb;
                }
                else
                {
                    num_sections = 1;
                    prb_per_section = p_prbMapElm->UP_nRBSize;
                }

                if(p_sec_desc == NULL) {
                    rte_panic ("p_sec_desc == NULL\n");
                }

                /* skip, if not scheduled */
                if(sym_id < p_prbMapElm->nStartSymb || sym_id >= p_prbMapElm->nStartSymb + p_prbMapElm->numSymb){
                    p_sec_desc->iq_buffer_offset = 0;
                    p_sec_desc->iq_buffer_len    = 0;
                    continue;
                }

                if (1)
                {
                    pkg_header_len = sizeof(struct rte_ether_hdr) +
                             sizeof (struct xran_ecpri_hdr) +
                             sizeof (struct radio_app_common_hdr);
                    comp_method = ((staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC) ? p_prbMapElm->compMethod : XRAN_COMPMETHOD_NONE);
                    pkg_len_temp = sizeof (struct data_section_hdr)*(num_sections);
                    if( comp_method != XRAN_COMPMETHOD_NONE)
                    {
                        pkg_len_temp += sizeof (struct data_section_compression_hdr)*(num_sections);
                        pkg_len_temp += (p_prbMapElm->iqWidth*3)*p_prbMapElm->UP_nRBSize;
                        if (p_prbMapElm->compMethod == XRAN_COMPMETHOD_BLKFLOAT) pkg_len_temp += p_prbMapElm->UP_nRBSize;
                    }
                    if (pkg_len == 0)
                    {
                        //first new pkg
                        pkg_len = pkg_header_len + pkg_len_temp;
                        iq_buffer_len = 0;
                        p_sec_desc->section_pos = 1;
                    }
                    else if (pkg_len + pkg_len_temp > mtu)
                    {
                        //new pkg
                        dst  = xran_add_hdr_offset(dst, ((staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC) ? p_prbMapElm->compMethod : XRAN_COMPMETHOD_NONE));
                        pkg_len = pkg_header_len + pkg_len_temp;
                        iq_buffer_len = 0;
                        p_sec_desc->section_pos = 1;
                    }
                    else //if (pkg_len + pkg_len_temp <= mtu)
                    {
                        //appand pkg
                        /* Create space for section/compression header in current prbElement */
                        dst += sizeof(struct data_section_hdr);
                        iq_buffer_len += sizeof(struct data_section_hdr);

                        comp_method = ((staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC) ? p_prbMapElm->compMethod : XRAN_COMPMETHOD_NONE);

                        if( comp_method != XRAN_COMPMETHOD_NONE)
                        {
                            dst += sizeof (struct data_section_compression_hdr);
                            iq_buffer_len += sizeof(struct data_section_compression_hdr);
                        }
                        pkg_len += pkg_len_temp;
                        p_sec_desc->section_pos = 2;
                    }
                }

                src = (uint8_t *)(pos + p_prbMapElm->UP_nRBStart*N_SC_PER_PRB(isNb375)*2L*p_iq->iq_dl_bit);
                p_sec_desc->iq_buffer_offset = RTE_PTR_DIFF(dst, u8dptr);
                p_sec_desc->iq_buffer_len = iq_buffer_len;


                for(idx=0; idx < num_sections ; idx++)
                {
                    //printf("\nidx %hu u8dptr %p dst %p",idx,u8dptr,dst);
                    if((idx+1)*prb_per_section > p_prbMapElm->UP_nRBSize){
                        prb_per_section = (p_prbMapElm->UP_nRBSize - idx*prb_per_section);
                    }

                    if(p_prbMapElm->compMethod == XRAN_COMPMETHOD_NONE) {
                        payload_len = prb_per_section*N_SC_PER_PRB(isNb375)*2L*p_iq->iq_dl_bit;
                        memcpy(dst, src, payload_len);
                    } else if ((p_prbMapElm->compMethod == XRAN_COMPMETHOD_BLKFLOAT) || (p_prbMapElm->compMethod == XRAN_COMPMETHOD_MODULATION)) {
                        struct xranlib_compress_request  bfp_com_req;
                        struct xranlib_compress_response bfp_com_rsp;

                        memset(&bfp_com_req, 0, sizeof(struct xranlib_compress_request));
                        memset(&bfp_com_rsp, 0, sizeof(struct xranlib_compress_response));

                        bfp_com_req.data_in    = (int16_t*)src;
                        bfp_com_req.numRBs     = prb_per_section;
                        bfp_com_req.len        = prb_per_section*N_SC_PER_PRB(isNb375)*2L*p_iq->iq_dl_bit;
                        bfp_com_req.compMethod = p_prbMapElm->compMethod;
                        bfp_com_req.iqWidth    = p_prbMapElm->iqWidth;
                        bfp_com_req.ScaleFactor= p_prbMapElm->ScaleFactor;
                        bfp_com_req.reMask     = p_prbMapElm->reMask;

                        bfp_com_rsp.data_out   = (int8_t*)dst;
                        bfp_com_rsp.len        = 0;

                        xranlib_compress(&bfp_com_req, &bfp_com_rsp);
                        payload_len = bfp_com_rsp.len;

                    } else {
                        printf ("p_prbMapElm->compMethod == %d is not supported\n",
                                p_prbMapElm->compMethod);
                        exit(-1);
                    }

                    if(num_sections != 1)
                        src += prb_per_section*N_SC_PER_PRB(isNb375)*2L*p_iq->iq_dl_bit;

                    /* update RB map for given element */
                    //p_sec_desc->iq_buffer_offset = RTE_PTR_DIFF(dst, u8dptr);
                    p_sec_desc->iq_buffer_len += payload_len;

                    /* add headroom for ORAN headers between IQs for chunk of RBs*/
                    dst += payload_len;
                    if(idx+1 == num_sections) /* Create space for (eth + eCPRI + radio app + section + comp) headers required by next prbElement */
                    {
                        //dst  = xran_add_hdr_offset(dst, ((staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC) ? p_prbMapElm->compMethod : XRAN_COMPMETHOD_NONE));
                    }
                    else
                    {
                        /* Create space for section/compression header in current prbElement */
                        //TODO: Check if alignment required for this case
                        dst += sizeof(struct data_section_hdr);
                        p_sec_desc->iq_buffer_len += sizeof(struct data_section_hdr);

                        comp_method = ((staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC) ? p_prbMapElm->compMethod : XRAN_COMPMETHOD_NONE);

                        if( comp_method != XRAN_COMPMETHOD_NONE)
                        {
                            dst += sizeof (struct data_section_compression_hdr);
                            p_sec_desc->iq_buffer_len += sizeof(struct data_section_compression_hdr);
                        }
                    }
                } /*for num_section */
                iq_buffer_len = p_sec_desc->iq_buffer_len;
            } /* for (idxElm = 0;  idxElm < pRbMap->nPrbElm; idxElm++) */
        } /* if(ptr && pos) */
        else {
            rte_panic("ptr ==NULL\n");
        }
    } /* if (pXranConf != NULL) */

    return 0;
}


int32_t
app_io_xran_iq_content_init_up_prach(uint8_t  appMode, struct xran_fh_config  *pXranConf,
                                    struct bbu_xran_io_if *psBbuIo, struct xran_io_shared_ctrl *psIoCtrl, struct o_xu_buffers * p_iq,
                                    int32_t cc_id, int32_t ant_id, int32_t sym_id, int32_t tti, int32_t flowId, uint8_t mu)
{
    char *pos = NULL;
    void *ptr = NULL;
    uint32_t* u32dptr = NULL;
    int32_t tti_dst =  tti % XRAN_N_FE_BUF_LEN;

    if(p_iq->buff_perMu[mu].p_tx_prach_play_buffer[flowId]) {
        pos =  ((char*)p_iq->buff_perMu[mu].p_tx_prach_play_buffer[flowId]);
        ptr = psIoCtrl->io_buff_perMu[mu].sFHPrachRxBbuIoBufCtrl[tti_dst][cc_id][ant_id].sBufferList.pBuffers[sym_id].pData;

        if(ptr && pos) {
            int32_t compMethod = pXranConf->ru_conf.compMeth_PRACH;

            if(compMethod == XRAN_COMPMETHOD_NONE) {
                u32dptr = (uint32_t*)(ptr);
                memcpy(u32dptr, pos, RTE_MIN(PRACH_PLAYBACK_BUFFER_BYTES, p_iq->buff_perMu[mu].tx_prach_play_buffer_size[flowId]));
            } else if((compMethod == XRAN_COMPMETHOD_BLKFLOAT)
                    || (compMethod == XRAN_COMPMETHOD_MODULATION)) {
                struct xranlib_compress_request  comp_req;
                struct xranlib_compress_response comp_rsp;

                memset(&comp_req, 0, sizeof(struct xranlib_compress_request));
                memset(&comp_rsp, 0, sizeof(struct xranlib_compress_response));

                /* compress whole playback data */
                comp_req.data_in        = (int16_t *)pos;
                comp_req.len            = RTE_MIN(PRACH_PLAYBACK_BUFFER_BYTES, p_iq->buff_perMu[mu].tx_prach_play_buffer_size[flowId]);
                comp_req.numRBs         = comp_req.len / 12 / 4;  /* 12RE, 4bytes */
                comp_req.compMethod     = compMethod;
                comp_req.iqWidth        = pXranConf->ru_conf.iqWidth_PRACH;
                comp_req.ScaleFactor    = 0;        /* TODO */
                comp_req.reMask         = 0xfff;    /* TODO */

                comp_rsp.data_out       = (int8_t *)ptr;
                comp_rsp.len            = 0;

                xranlib_compress(&comp_req, &comp_rsp);
            } else {
                printf ("p_prbMapElm->compMethod == %d is not supported\n", compMethod);
                exit(-1);
            }
        } else { /*  if(ptr && pos) */
            printf("prach ptr ==NULL\n");
            exit(-1);
        }
    } /* if(p_iq->p_tx_prach_play_buffer[flowId]) */

    return 0;
}

int32_t
app_io_xran_iq_content_init_up_srs(uint8_t  appMode, struct xran_fh_config  *pXranConf,
                                  struct bbu_xran_io_if *psBbuIo, struct xran_io_shared_ctrl *psIoCtrl, struct o_xu_buffers * p_iq,
                                  int32_t cc_id, int32_t ant_id, int32_t sym_id, int32_t tti, int32_t flowId, uint8_t mu)
{
    struct xran_prb_map * pRbMap = NULL;
    char *pos = NULL;
    void *ptr = NULL;
    uint8_t* u8dptr = NULL;
    enum xran_comp_hdr_type staticEn = XRAN_COMP_HDR_TYPE_DYNAMIC;
    int32_t tti_dst =  tti % XRAN_N_FE_BUF_LEN;
    int32_t tti_src =  tti % p_iq->numSlots;

    if (pXranConf != NULL)
    {

        staticEn = pXranConf->ru_conf.xranCompHdrType;
        int32_t tx_play_buffer_position = tti_src * (XRAN_NUM_OF_SYMBOL_PER_SLOT*pXranConf->perMu[mu].nULRBs*N_SC_PER_PRB(false)*4) + (sym_id * pXranConf->perMu[mu].nULRBs*N_SC_PER_PRB(false)*4);


    if(p_iq->buff_perMu[mu].p_tx_srs_play_buffer[flowId]) {
        pos =  ((char*)p_iq->buff_perMu[mu].p_tx_srs_play_buffer[flowId])  + tx_play_buffer_position;
        ptr = psIoCtrl->io_buff_perMu[mu].sFHSrsRxBbuIoBufCtrl[tti_dst][cc_id][ant_id].sBufferList.pBuffers[sym_id].pData;
        pRbMap = (struct xran_prb_map *) psIoCtrl->io_buff_perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[tti_dst][cc_id][ant_id].sBufferList.pBuffers->pData;

        if(ptr && pos && pRbMap) {
            int32_t idxElm = 0;
            u8dptr = (uint8_t*)ptr;
            int16_t payload_len = 0;

            uint8_t  *dst = (uint8_t *)u8dptr;
            uint8_t  *src = (uint8_t *)pos;
            struct xran_prb_elm* p_prbMapElm = &pRbMap->prbMap[idxElm];
                dst =  xran_add_hdr_offset(dst, (staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC) ? p_prbMapElm->compMethod : XRAN_COMPMETHOD_NONE);
            for (idxElm = 0;  idxElm < pRbMap->nPrbElm; idxElm++) {
                struct xran_section_desc *p_sec_desc = NULL;
                p_prbMapElm = &pRbMap->prbMap[idxElm];
                p_sec_desc =  &p_prbMapElm->sec_desc[sym_id];

                if(p_sec_desc == NULL){
                    rte_panic ("p_sec_desc == NULL\n");
                }

                /* skip, if not scheduled */
                if(sym_id < p_prbMapElm->nStartSymb || sym_id >= p_prbMapElm->nStartSymb + p_prbMapElm->numSymb) {
                    p_sec_desc->iq_buffer_offset = 0;
                    p_sec_desc->iq_buffer_len    = 0;
                    continue;
                }

                src = (uint8_t *)(pos + p_prbMapElm->nRBStart*N_SC_PER_PRB(false)*4L);

                if(p_prbMapElm->compMethod == XRAN_COMPMETHOD_NONE) {
                    payload_len = p_prbMapElm->nRBSize*N_SC_PER_PRB(false)*4L;
                    memcpy(dst, src, payload_len);

                } else if (p_prbMapElm->compMethod == XRAN_COMPMETHOD_BLKFLOAT
                        || (p_prbMapElm->compMethod == XRAN_COMPMETHOD_MODULATION)) {
                    struct xranlib_compress_request  bfp_com_req;
                    struct xranlib_compress_response bfp_com_rsp;

                    memset(&bfp_com_req, 0, sizeof(struct xranlib_compress_request));
                    memset(&bfp_com_rsp, 0, sizeof(struct xranlib_compress_response));

                    bfp_com_req.data_in    = (int16_t*)src;
                    bfp_com_req.numRBs     = p_prbMapElm->UP_nRBSize;
                    bfp_com_req.len        = p_prbMapElm->UP_nRBSize*N_SC_PER_PRB(false)*4L;
                    bfp_com_req.compMethod = p_prbMapElm->compMethod;
                    bfp_com_req.iqWidth    = p_prbMapElm->iqWidth;
                    bfp_com_req.ScaleFactor= p_prbMapElm->ScaleFactor;
                    bfp_com_req.reMask     = p_prbMapElm->reMask;

                    bfp_com_rsp.data_out   = (int8_t*)dst;
                    bfp_com_rsp.len        = 0;

                    xranlib_compress(&bfp_com_req, &bfp_com_rsp);
                    payload_len = bfp_com_rsp.len;
                } else {
                    rte_panic ("p_prbMapElm->compMethod == %d is not supported\n", p_prbMapElm->compMethod);
                }

                /* update RB map for given element */
                p_sec_desc->iq_buffer_offset = RTE_PTR_DIFF(dst, u8dptr);
                p_sec_desc->iq_buffer_len = payload_len;

                /* add headroom for ORAN headers between IQs for chunk of RBs*/
                dst += payload_len;
                    dst  = xran_add_hdr_offset(dst, (staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC) ? p_prbMapElm->compMethod : XRAN_COMPMETHOD_NONE);
            }
        } else {
            rte_panic("[%d %d %d] %p %p %p ==NULL\n",tti_dst, ant_id, sym_id, ptr, pos, pRbMap);
        }

        p_iq->buff_perMu[mu].tx_srs_play_buffer_position[flowId] += pXranConf->perMu[mu].nULRBs*N_SC_PER_PRB(false)*4;
        if(p_iq->buff_perMu[mu].tx_srs_play_buffer_position[flowId] >= p_iq->buff_perMu[mu].tx_srs_play_buffer_size[flowId])
            p_iq->buff_perMu[mu].tx_srs_play_buffer_position[flowId] = 0;
        }
    }

    return 0;
}



int32_t
app_io_xran_iq_content_init_up_csirs(uint8_t  appMode, struct xran_fh_config  *pXranConf,
                                  struct bbu_xran_io_if *psBbuIo, struct xran_io_shared_ctrl *psIoCtrl, struct o_xu_buffers * p_iq,
                                  int32_t cc_id, int32_t ant_id, int32_t sym_id, int32_t target_tti, int32_t flowId, uint8_t mu)
{
    struct xran_prb_map * pRbMap = NULL;
    char *pos = NULL;
    void *ptr = NULL;
    uint8_t* u8dptr = NULL;
    enum xran_comp_hdr_type staticEn = XRAN_COMP_HDR_TYPE_DYNAMIC;
    int32_t tti_dst =  target_tti % XRAN_N_FE_BUF_LEN;
    int32_t tti_src =  target_tti % p_iq->numSlots;
    int32_t tx_play_buffer_position;

    if (pXranConf != NULL)
    {
        tx_play_buffer_position = tti_src * (XRAN_NUM_OF_SYMBOL_PER_SLOT*pXranConf->perMu[mu].nDLRBs*N_SC_PER_PRB(false)*4) + (sym_id * pXranConf->perMu[mu].nDLRBs*N_SC_PER_PRB(false)*4);
        staticEn = pXranConf->ru_conf.xranCompHdrType;
        if(p_iq->buff_perMu[mu].p_tx_csirs_play_buffer[flowId]) {
            pos =  ((char*)p_iq->buff_perMu[mu].p_tx_csirs_play_buffer[flowId]) + tx_play_buffer_position;
            ptr = psIoCtrl->io_buff_perMu[mu].sFHCsirsTxBbuIoBufCtrl[tti_dst][cc_id][ant_id].sBufferList.pBuffers[sym_id].pData;
            pRbMap = (struct xran_prb_map *) psIoCtrl->io_buff_perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[tti_dst][cc_id][ant_id].sBufferList.pBuffers->pData;

            if(ptr && pos && pRbMap) {
                int32_t idxElm = 0;
                u8dptr = (uint8_t*)ptr;
                int16_t payload_len = 0;

                uint8_t  *dst = (uint8_t *)u8dptr;
                uint8_t  *src = (uint8_t *)pos;
                struct xran_prb_elm* p_prbMapElm = &pRbMap->prbMap[idxElm];
                dst =  xran_add_hdr_offset(dst, (staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC) ? p_prbMapElm->compMethod : XRAN_COMPMETHOD_NONE);
                for (idxElm = 0;  idxElm < pRbMap->nPrbElm; idxElm++) {
                    struct xran_section_desc *p_sec_desc = NULL;
                    p_prbMapElm = &pRbMap->prbMap[idxElm];
                    p_sec_desc =  &p_prbMapElm->sec_desc[sym_id];

                    if(p_sec_desc == NULL){
                        rte_panic ("p_sec_desc == NULL\n");
                    }

                    /* skip, if not scheduled */
                    if(sym_id < p_prbMapElm->nStartSymb || sym_id >= p_prbMapElm->nStartSymb + p_prbMapElm->numSymb) {
                        p_sec_desc->iq_buffer_offset = 0;
                        p_sec_desc->iq_buffer_len    = 0;
                        continue;
                    }

                    src = (uint8_t *)(pos + p_prbMapElm->nRBStart*N_SC_PER_PRB(false)*4L);

                    if(p_prbMapElm->compMethod == XRAN_COMPMETHOD_NONE) {
                        payload_len = p_prbMapElm->nRBSize*N_SC_PER_PRB(false)*4L;
                        memcpy(dst, src, payload_len);

                    } else if (p_prbMapElm->compMethod == XRAN_COMPMETHOD_BLKFLOAT
                            || (p_prbMapElm->compMethod == XRAN_COMPMETHOD_MODULATION)) {
                        struct xranlib_compress_request  bfp_com_req;
                        struct xranlib_compress_response bfp_com_rsp;

                        memset(&bfp_com_req, 0, sizeof(struct xranlib_compress_request));
                        memset(&bfp_com_rsp, 0, sizeof(struct xranlib_compress_response));

                        bfp_com_req.data_in    = (int16_t*)src;
                        bfp_com_req.numRBs     = p_prbMapElm->UP_nRBSize;
                        bfp_com_req.len        = p_prbMapElm->UP_nRBSize*N_SC_PER_PRB(false)*4L;
                        bfp_com_req.compMethod = p_prbMapElm->compMethod;
                        bfp_com_req.iqWidth    = p_prbMapElm->iqWidth;
                        bfp_com_req.ScaleFactor= p_prbMapElm->ScaleFactor;
                        bfp_com_req.reMask     = p_prbMapElm->reMask;

                        bfp_com_rsp.data_out   = (int8_t*)dst;
                        bfp_com_rsp.len        = 0;

                        xranlib_compress(&bfp_com_req, &bfp_com_rsp);
                        payload_len = bfp_com_rsp.len;
                    } else {
                        rte_panic ("p_prbMapElm->compMethod == %d is not supported\n", p_prbMapElm->compMethod);
                    }

                    /* update RB map for given element */
                    p_sec_desc->iq_buffer_offset = RTE_PTR_DIFF(dst, u8dptr);
                    p_sec_desc->iq_buffer_len = payload_len;

                    /* add headroom for ORAN headers between IQs for chunk of RBs*/
                    dst += payload_len;
                        dst  = xran_add_hdr_offset(dst, (staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC) ? p_prbMapElm->compMethod : XRAN_COMPMETHOD_NONE);
                }
            } else {
                rte_panic("[%d %d %d] %p %p %p ==NULL\n",tti_dst, ant_id, sym_id, ptr, pos, pRbMap);
            }
        }
    }
    return 0;
}

int32_t
app_io_xran_iq_content_init(uint32_t o_xu_id, RuntimeConfig *p_o_xu_cfg)
{
    xran_status_t status;

    struct bbu_xran_io_if *psBbuIo       = app_io_xran_if_get();
    struct xran_io_shared_ctrl *psIoCtrl = app_io_xran_if_ctrl_get(o_xu_id);
    int32_t nSectorIndex[XRAN_MAX_SECTOR_NR];
    int32_t nSectorNum;
    int32_t cc_id, ant_id, sym_id, tti;
    int32_t flowId, mu_idx;
    struct xran_fh_config  *pXranConf = &app_io_xran_fh_config[o_xu_id];
    //struct xran_fh_init    *pXranInit = &app_io_xran_fh_init;
    struct o_xu_buffers    * p_iq     = NULL;

    uint32_t xran_max_antenna_nr = RTE_MAX(p_o_xu_cfg->numAxc, p_o_xu_cfg->numUlAxc);
    uint32_t xran_max_ant_array_elm_nr = RTE_MAX(p_o_xu_cfg->antElmTRx, xran_max_antenna_nr);
    uint32_t xran_max_antenna_nr_prach = RTE_MIN(xran_max_antenna_nr, XRAN_MAX_PRACH_ANT_NUM);

    if(psBbuIo == NULL){
        rte_panic("psBbuIo == NULL\n");
    }

    if(psIoCtrl == NULL){
        rte_panic("psIoCtrl == NULL\n");
    }

    for (nSectorNum = 0; nSectorNum < XRAN_MAX_SECTOR_NR; nSectorNum++) {
        nSectorIndex[nSectorNum] = nSectorNum;
    }
    nSectorNum = p_o_xu_cfg->numCC;
    printf ("app_io_xran_iq_content_init\n");

    if(p_o_xu_cfg->p_buff) {
        p_iq = p_o_xu_cfg->p_buff;
    } else {
        rte_panic("Error p_o_xu_cfg->p_buff\n");
    }
    if (p_o_xu_cfg->nPuschUpMultiSectonSend)
    {
        printf("p_o_xu_cfg->mtu = %d\n",p_o_xu_cfg->mtu);
    }
    /* Init Memory */
    for(mu_idx = 0; mu_idx < p_o_xu_cfg->numMu; mu_idx++) {
        uint8_t mu = p_o_xu_cfg->mu_number[mu_idx];
        for(cc_id = 0; cc_id < nSectorNum; cc_id++) {
            for(tti  = 0; tti  < XRAN_N_FE_BUF_LEN; tti ++) {
                for(ant_id = 0; ant_id < xran_max_antenna_nr; ant_id++){
                    for(sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
                        if(p_o_xu_cfg->appMode == APP_O_DU) {
                            flowId = p_o_xu_cfg->numAxc * cc_id + ant_id;
                        } else {
                            flowId = p_o_xu_cfg->numUlAxc * cc_id + ant_id;
                        }

                        if ((status = app_io_xran_iq_content_init_dl_cp(p_o_xu_cfg->appMode, pXranConf,
                                                        psBbuIo, psIoCtrl, p_iq,
                                                        cc_id, ant_id, sym_id, tti, flowId, mu)) != 0) {
                            rte_panic("app_io_xran_iq_content_init_dl_cp");
                        }
                        if (p_o_xu_cfg->nPuschUpMultiSectonSend)
                        {
                            if ((status = app_io_xran_iq_content_init_multi_section_up_tx(p_o_xu_cfg->appMode, pXranConf,
                                                            psBbuIo, psIoCtrl, p_iq,
                                                            cc_id, ant_id, sym_id, tti, flowId, mu, p_o_xu_cfg->mtu)) != 0) {
                                rte_panic("app_io_xran_iq_content_init_up_tx");
                            }
                        }
                        else
                        {
                            if ((status = app_io_xran_iq_content_init_up_tx(p_o_xu_cfg->appMode, pXranConf,
                                                            psBbuIo, psIoCtrl, p_iq,
                                                            cc_id, ant_id, sym_id, tti, flowId, mu)) != 0) {
                                rte_panic("app_io_xran_iq_content_init_up_tx");
                            }
                        }
                        if ((status = app_io_xran_iq_content_init_ul_cp(p_o_xu_cfg->appMode, pXranConf,
                                                        psBbuIo, psIoCtrl, p_iq,
                                                        cc_id, ant_id, sym_id, tti, flowId, mu)) != 0) {
                            rte_panic("app_io_xran_iq_content_init_ul_cp");
                        }

                    }
                }

                /* prach TX for RU only */
                if(p_o_xu_cfg->appMode == APP_O_RU && p_o_xu_cfg->perMu[mu].prachEnable) {
                    for(ant_id = 0; ant_id < xran_max_antenna_nr_prach; ant_id++) {
                        for(sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
                            flowId = xran_max_antenna_nr_prach * cc_id + ant_id;
                            if ((status = app_io_xran_iq_content_init_up_prach(p_o_xu_cfg->appMode, pXranConf,
                                                            psBbuIo, psIoCtrl, p_iq,
                                                            cc_id, ant_id, sym_id, tti, flowId, mu))  != 0) {
                                rte_panic("app_io_xran_iq_content_init_up_prach");
                            }
                        }
                    }
        #if 0
                    for(sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
                        char fname[32];
                        snprintf(fname, sizeof(fname), "./logs/aftercomp-%d.bin", sym_id);
                        sys_save_buf_to_file(fname,
                                "Compressed PRACH IQ Samples in binary format",
                                psIoCtrl->sFHPrachRxBbuIoBufCtrl[0][0][0].sBufferList.pBuffers[sym_id].pData,
                                RTE_MIN(PRACH_PLAYBACK_BUFFER_BYTES, p_iq->tx_prach_play_buffer_size[0]),
                                1);
                        snprintf(fname, sizeof(fname), "./logs/aftercomp-%d.txt", sym_id);
                        sys_save_buf_to_file_txt(fname,
                                "Compressed PRACH IQ Samples in human readable format",
                                psIoCtrl->sFHPrachRxBbuIoBufCtrl[0][0][0].sBufferList.pBuffers[sym_id].pData,
                                RTE_MIN(PRACH_PLAYBACK_BUFFER_BYTES, p_iq->tx_prach_play_buffer_size[0]),
                                1);
                        }
        #endif
                }
                /* SRS TX for RU only */
                if(p_o_xu_cfg->appMode == APP_O_RU && p_o_xu_cfg->enableSrs) {
                    for(ant_id = 0; ant_id < xran_max_ant_array_elm_nr; ant_id++) {
                        for(sym_id = 0; sym_id < XRAN_MAX_NUM_OF_SRS_SYMBOL_PER_SLOT; sym_id++) {
                            flowId = p_o_xu_cfg->antElmTRx*cc_id + ant_id;
                            if ((status = app_io_xran_iq_content_init_up_srs(p_o_xu_cfg->appMode, pXranConf,
                                                                            psBbuIo, psIoCtrl, p_iq,
                                                                            cc_id, ant_id, sym_id, tti, flowId, mu))  != 0){
                                rte_panic("app_io_xran_iq_content_init_up_srs");
                            }
                        }
                    }
                }
                /* CSI-RS TX packets for DU only */
                if(p_o_xu_cfg->appMode == APP_O_DU && p_o_xu_cfg->csirsEnable) {
                    for(ant_id = 0; ant_id < p_o_xu_cfg->nCsiPorts; ant_id++) {
                        for(sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
                            flowId = p_o_xu_cfg->nCsiPorts*cc_id + ant_id;
                            if ((status = app_io_xran_iq_content_init_up_csirs(p_o_xu_cfg->appMode, pXranConf,
                                                                            psBbuIo, psIoCtrl, p_iq,
                                                                            cc_id, ant_id, sym_id, tti, flowId, mu))  != 0){
                                rte_panic("app_io_xran_iq_content_init_up_csirs");
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}


void app_io_xran_if_stop(uint32_t o_xu_id, RuntimeConfig *p_o_xu_cfg __attribute__((unused)))
{
    printf("Stopping.... %d\n", o_xu_id);
}

void app_io_xran_if_close(uint32_t o_xu_id, RuntimeConfig *p_o_xu_cfg)
{
    struct bbu_xran_io_if *psBbuIo = app_io_xran_if_get();
    int i;

    for(i=0; i < p_o_xu_cfg->numCC; i++)
    {
        xran_bm_release(psBbuIo->nInstanceHandle[o_xu_id][i], NULL);
    }
}

int32_t
app_io_xran_iq_content_get_up_prach(uint8_t  appMode, struct xran_fh_config  *pXranConf,
                                  struct bbu_xran_io_if *psBbuIo, struct xran_io_shared_ctrl *psIoCtrl, struct o_xu_buffers * p_iq,
                                  int32_t cc_id, int32_t ant_id, int32_t sym_id, int32_t target_tti, int32_t flowId, uint8_t mu)
{
    int32_t prach_len = 0;
    void *ptr = NULL;
    char *pos = NULL;

    int32_t tti_src =  target_tti % XRAN_N_FE_BUF_LEN;
    int32_t tti_dst =  target_tti % p_iq->numSlots;
    int32_t prach_log_buffer_position;
    struct xran_prb_map *pRbMap = (struct xran_prb_map *) psIoCtrl->io_buff_perMu[mu].sFHPrachRxPrbMapBbuIoBufCtrl[tti_src][cc_id][ant_id].sBufferList.pBuffers->pData;;
    struct xran_rx_packet_ctl *pFrontHaulRxPacketCtrl = &pRbMap->sFrontHaulRxPacketCtrl[sym_id];
    int32_t i, npkt = pFrontHaulRxPacketCtrl->nRxPkt;
    int16_t iqWidth = pXranConf->ru_conf.iqWidth_PRACH;

    prach_len = (3 * iqWidth) * pXranConf->perMu[mu].prach_conf.numPrbc; /* 12RE*2pairs/8bits (12*2/8=3)*/
    prach_log_buffer_position = tti_dst * (XRAN_NUM_OF_SYMBOL_PER_SLOT*prach_len) + (sym_id * prach_len);

    if(p_iq->buff_perMu[mu].p_prach_log_buffer[flowId]) {
        pos =  ((char*)p_iq->buff_perMu[mu].p_prach_log_buffer[flowId]) + prach_log_buffer_position;
        for (i = 0; i < npkt; i++)
        {
            uint16_t nRBStart = pFrontHaulRxPacketCtrl->nRBStart[i];
            uint16_t nRBSize = pFrontHaulRxPacketCtrl->nRBSize[i];

            ptr = pFrontHaulRxPacketCtrl->pData[i];
            if(ptr) {
                int32_t compMethod = pXranConf->ru_conf.compMeth_PRACH;
                if(compMethod == XRAN_COMPMETHOD_NONE) {
                    memcpy(pos + nRBStart * N_SC_PER_PRB(false) * 2 * p_iq->iq_prach_bit, (uint32_t *)(ptr), nRBSize * N_SC_PER_PRB(false)*2*p_iq->iq_prach_bit);
                } else {
                    struct xranlib_decompress_request   decomp_req;
                    struct xranlib_decompress_response  decomp_rsp;
                    int32_t parm_size;

                    memset(&decomp_req, 0, sizeof(struct xranlib_decompress_request));
                    memset(&decomp_rsp, 0, sizeof(struct xranlib_decompress_response));

                    switch(compMethod) {
                        case XRAN_COMPMETHOD_BLKFLOAT:      parm_size = 1; break;
                        case XRAN_COMPMETHOD_MODULATION:    parm_size = 0; break;
                        default:
                            parm_size = 0;
                    }

                    decomp_req.data_in      = (int8_t *)ptr;
                    decomp_req.numRBs       = pXranConf->perMu[mu].prach_conf.numPrbc;
                    decomp_req.len          = (3 * iqWidth + parm_size) * pXranConf->perMu[mu].prach_conf.numPrbc; /* 12RE*2pairs/8bits (12*2/8=3)*/
                    decomp_req.compMethod   = compMethod;
                    decomp_req.iqWidth      = iqWidth;
                    decomp_req.ScaleFactor  = 0;        /* TODO */
                    decomp_req.reMask       = 0xfff;    /* TODO */
                    decomp_req.SprEnable    = 0;

                    decomp_rsp.data_out     = (int16_t *)(pos + nRBStart * N_SC_PER_PRB(false) * 2 * p_iq->iq_prach_bit);
                    decomp_rsp.len          = 0;

                    xranlib_decompress(&decomp_req, &decomp_rsp);
                }
            }
        }
    } /* if(p_iq->p_prach_log_buffer[flowId]) */

    return XRAN_STATUS_SUCCESS;
}


int32_t
app_io_xran_iq_content_get_up_csirs(uint8_t  appMode, struct xran_fh_config  *pXranConf,
                                  struct bbu_xran_io_if *psBbuIo, struct xran_io_shared_ctrl *psIoCtrl, struct o_xu_buffers * p_iq,
                                  int32_t cc_id, int32_t ant_id, int32_t sym_id, int32_t target_tti, int32_t flowId, uint8_t mu)
{
    int32_t i;
    struct xran_prb_map *pRbMap = NULL;
    void *ptr = NULL;
    char *pos = NULL;

    int32_t tti_src =  target_tti % XRAN_N_FE_BUF_LEN;
    int32_t tti_dst =  target_tti % p_iq->numSlots;
    int32_t csirs_log_buffer_position;
    int16_t compMethod = pXranConf->ru_conf.compMeth;
    int16_t iqWidth = pXranConf->ru_conf.iqWidth;
    int16_t nRBSize, nRBStart;

    csirs_log_buffer_position = tti_dst * (XRAN_NUM_OF_SYMBOL_PER_SLOT*pXranConf->perMu[mu].nDLRBs*N_SC_PER_PRB(false)*2*p_iq->iq_csirs_bit) +
                            (sym_id * pXranConf->perMu[mu].nDLRBs*N_SC_PER_PRB(false)*2*p_iq->iq_csirs_bit);

    pRbMap = (struct xran_prb_map *) psIoCtrl->io_buff_perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[tti_src][cc_id][ant_id].sBufferList.pBuffers->pData;
    if(pRbMap == NULL) {
        rte_panic("pRbMap == NULL\n");
    }
    if(p_iq->buff_perMu[mu].p_csirs_log_buffer[flowId]) {

        struct xran_rx_packet_ctl *pFrontHaulRxPacketCtrl = &pRbMap->sFrontHaulRxPacketCtrl[sym_id];
        int32_t npkts = RTE_MAX(pFrontHaulRxPacketCtrl->nRxPkt, psIoCtrl->io_buff_perMu[mu].nCSIRSRxPktBufCtrl[tti_src][cc_id][ant_id][sym_id]);
        
        pos =  ((char*)p_iq->buff_perMu[mu].p_csirs_log_buffer[flowId]) + csirs_log_buffer_position;
        
        for (i = 0; i < npkts; i++)
        {
            ptr = pFrontHaulRxPacketCtrl->pData[i];
            nRBSize = pFrontHaulRxPacketCtrl->nRBSize[i];
            nRBStart = pFrontHaulRxPacketCtrl->nRBStart[i];
            if(ptr) {
                if (compMethod != XRAN_COMPMETHOD_NONE) {
                    struct xranlib_decompress_request  bfp_decom_req;
                    struct xranlib_decompress_response bfp_decom_rsp;
                    int32_t parm_size;
            
                    memset(&bfp_decom_req, 0, sizeof(struct xranlib_decompress_request));
                    memset(&bfp_decom_rsp, 0, sizeof(struct xranlib_decompress_response));
                    switch(compMethod) {
                        case XRAN_COMPMETHOD_BLKFLOAT:
                            parm_size = 1;
                            break;
                        case XRAN_COMPMETHOD_MODULATION:
                            parm_size = 0;
                            break;
                        default:
                            parm_size = 0;
                        }
            
                    bfp_decom_req.data_in    = (int8_t *)ptr;
                    bfp_decom_req.numRBs     = nRBSize;
                    bfp_decom_req.len        = (3 * iqWidth + parm_size)* nRBSize;
                    bfp_decom_req.compMethod = compMethod;
                    bfp_decom_req.iqWidth    = iqWidth;
                    bfp_decom_req.SprEnable  = 0;
            
                    bfp_decom_rsp.data_out   = (int16_t *)(pos + nRBStart * N_SC_PER_PRB(false) * 2 * p_iq->iq_csirs_bit);
                    bfp_decom_rsp.len        = 0;
            
                    xranlib_decompress(&bfp_decom_req, &bfp_decom_rsp);
                }
                else {
                    memcpy(pos + nRBStart * N_SC_PER_PRB(false)*4 , ptr, nRBSize * N_SC_PER_PRB(false)*2*p_iq->iq_csirs_bit);
                }
            }
        }
    }

    return XRAN_STATUS_SUCCESS;
}

int32_t
app_io_xran_iq_content_get_up_srs(uint8_t  appMode, struct xran_fh_config  *pXranConf,
                                  struct bbu_xran_io_if *psBbuIo, struct xran_io_shared_ctrl *psIoCtrl, struct o_xu_buffers * p_iq,
                                  int32_t cc_id, int32_t ant_id, int32_t sym_id, int32_t target_tti, int32_t flowId, uint8_t mu)
{
    int32_t i;
    struct xran_prb_map *pRbMap = NULL;

    uint8_t *ptr = NULL;
    char *pos = NULL;

    int32_t tti_src =  target_tti % XRAN_N_FE_BUF_LEN;
    int32_t tti_dst =  target_tti % p_iq->numSlots;
    int32_t srs_log_buffer_position;
    int16_t compMethod = pXranConf->ru_conf.compMeth;
    int16_t iqWidth = pXranConf->ru_conf.iqWidth;
    int16_t nRBSize, nRBStart;

    srs_log_buffer_position = tti_dst * (XRAN_NUM_OF_SYMBOL_PER_SLOT*pXranConf->perMu[mu].nULRBs*N_SC_PER_PRB(false)*2*p_iq->iq_srs_bit) +
                            (sym_id * pXranConf->perMu[mu].nULRBs*N_SC_PER_PRB(false)*2*p_iq->iq_srs_bit);

    pRbMap = (struct xran_prb_map *) psIoCtrl->io_buff_perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[tti_src][cc_id][ant_id].sBufferList.pBuffers->pData;
    if(pRbMap == NULL) {
        rte_panic("pRbMap == NULL\n");
    }

    if(p_iq->buff_perMu[mu].p_srs_log_buffer[flowId]) {
        struct xran_rx_packet_ctl *pFrontHaulRxPacketCtrl = &pRbMap->sFrontHaulRxPacketCtrl[sym_id];
        int32_t npkts = RTE_MAX(pFrontHaulRxPacketCtrl->nRxPkt, psIoCtrl->io_buff_perMu[mu].nSRSRxPktBufCtrl[tti_src][cc_id][ant_id][sym_id]);

        pos =  ((char*)p_iq->buff_perMu[mu].p_srs_log_buffer[flowId]) + srs_log_buffer_position;

        for (i = 0; i < npkts; i++)
        {
            ptr = pFrontHaulRxPacketCtrl->pData[i];
            nRBSize = pFrontHaulRxPacketCtrl->nRBSize[i];
            nRBStart = pFrontHaulRxPacketCtrl->nRBStart[i];
            if(ptr) {
                if (compMethod != XRAN_COMPMETHOD_NONE) {
                    struct xranlib_decompress_request  bfp_decom_req;
                    struct xranlib_decompress_response bfp_decom_rsp;
                    int32_t parm_size;
            
                    memset(&bfp_decom_req, 0, sizeof(struct xranlib_decompress_request));
                    memset(&bfp_decom_rsp, 0, sizeof(struct xranlib_decompress_response));
                    switch(compMethod) {
                        case XRAN_COMPMETHOD_BLKFLOAT:
                            parm_size = 1;
                            break;
                        case XRAN_COMPMETHOD_MODULATION:
                            parm_size = 0;
                            break;
                        default:
                            parm_size = 0;
                        }
            
                    bfp_decom_req.data_in    = (int8_t *)ptr;
                    bfp_decom_req.numRBs     = nRBSize;
                    bfp_decom_req.len        = (3 * iqWidth + parm_size)* nRBSize;
                    bfp_decom_req.compMethod = compMethod;
                    bfp_decom_req.iqWidth    = iqWidth;
                    bfp_decom_req.SprEnable  = 0;
            
                    bfp_decom_rsp.data_out   = (int16_t *)(pos + nRBStart * N_SC_PER_PRB(false) * 2 * p_iq->iq_srs_bit);
                    bfp_decom_rsp.len        = 0;
            
                    xranlib_decompress(&bfp_decom_req, &bfp_decom_rsp);
                }
                else {
                    memcpy(pos + nRBStart * N_SC_PER_PRB(false)*4 , ptr, nRBSize * N_SC_PER_PRB(false)* 2 * p_iq->iq_srs_bit);
                }
            }
        }
    }

    return XRAN_STATUS_SUCCESS;
}

int32_t
app_io_xran_iq_content_get_up_rx(uint8_t  appMode, struct xran_fh_config  *pXranConf,
                                  struct bbu_xran_io_if *psBbuIo, struct xran_io_shared_ctrl *psIoCtrl, struct o_xu_buffers * p_iq,
                                  int32_t cc_id, int32_t ant_id, int32_t sym_id, int32_t target_tti, int32_t flowId, uint8_t mu)
{
    int32_t i;
    struct xran_prb_map *pRbMap = NULL;
    struct xran_prb_elm *pRbElm = NULL;

    char *pos = NULL;
    uint32_t *u32dptr;
    uint16_t num_prbu = 0, start_prbu = 0, sect_id;
    char *src;

    bool isNb375=false;
    if(APP_O_DU == appMode && mu == XRAN_NBIOT_MU && XRAN_NBIOT_UL_SCS_3_75 ==pXranConf->perMu[mu].nbIotUlScs)
    {
        isNb375=true;
    }

    int32_t tti_src =  target_tti % XRAN_N_FE_BUF_LEN;
    int32_t tti_dst =  target_tti % p_iq->numSlots;
    int32_t rx_log_buffer_position;

    rx_log_buffer_position = tti_dst *
                (XRAN_NUM_OF_SYMBOL_PER_SLOT*pXranConf->perMu[mu].nULRBs*N_SC_PER_PRB(isNb375)*2*p_iq->iq_ul_bit) +
                (sym_id * pXranConf->perMu[mu].nULRBs*N_SC_PER_PRB(isNb375)*2*p_iq->iq_ul_bit);
    pRbMap = (struct xran_prb_map *) psIoCtrl->io_buff_perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[tti_src][cc_id][ant_id].sBufferList.pBuffers->pData;
    if(pRbMap == NULL) {
        printf("pRbMap == NULL\n");
        exit(-1);
    }

    if (p_iq->buff_perMu[mu].p_rx_log_buffer[flowId])
    {
        struct xran_rx_packet_ctl *pFrontHaulRxPacketCtrl = &pRbMap->sFrontHaulRxPacketCtrl[sym_id];
        int32_t npkts = RTE_MAX(pFrontHaulRxPacketCtrl->nRxPkt, psIoCtrl->io_buff_perMu[mu].nRxPktBufCtrl[tti_src][cc_id][ant_id][sym_id]);
        pos =  ((char*)p_iq->buff_perMu[mu].p_rx_log_buffer[flowId]) + rx_log_buffer_position;
        for (i = 0; i < npkts; i++)
        {
            start_prbu = pFrontHaulRxPacketCtrl->nRBStart[i];
            num_prbu = pFrontHaulRxPacketCtrl->nRBSize[i];
            sect_id = pFrontHaulRxPacketCtrl->nSectid[i];
            src = (char *)pFrontHaulRxPacketCtrl->pData[i];
            pRbElm = &pRbMap->prbMap[sect_id];
            if(src && pRbElm)
            {
                u32dptr = (uint32_t*)(src);
                if (pRbElm->compMethod != XRAN_COMPMETHOD_NONE)
                {
                    struct xranlib_decompress_request  bfp_decom_req;
                    struct xranlib_decompress_response bfp_decom_rsp;
                    int32_t parm_size = 0;
                    
                    memset(&bfp_decom_req, 0, sizeof(struct xranlib_decompress_request));
                    memset(&bfp_decom_rsp, 0, sizeof(struct xranlib_decompress_response));
                    switch(pXranConf->ru_conf.compMeth) {
                    case XRAN_COMPMETHOD_BLKFLOAT:
                        parm_size = 1;
                        break;
                    case XRAN_COMPMETHOD_MODULATION:
                        parm_size = 0;
                        break;
                    default:
                        parm_size = 0;
                    }
                    
                    bfp_decom_req.data_in    = (int8_t *)u32dptr;
                    bfp_decom_req.numRBs     = num_prbu;
                    bfp_decom_req.len        = (3 * pRbElm->iqWidth + parm_size)*num_prbu;
                    bfp_decom_req.compMethod = pRbElm->compMethod;
                    bfp_decom_req.iqWidth    = pRbElm->iqWidth;
                    bfp_decom_req.reMask     = pRbElm->reMask;
                    bfp_decom_req.ScaleFactor= pRbElm->ScaleFactor;
                    bfp_decom_req.SprEnable  = 0;
                    
                    bfp_decom_rsp.data_out   = (int16_t *)(pos + start_prbu*N_SC_PER_PRB(isNb375)*2*p_iq->iq_ul_bit);
                    bfp_decom_rsp.len        = 0;
                    
                    xranlib_decompress(&bfp_decom_req, &bfp_decom_rsp);
                    src += (3 * pRbElm->iqWidth + parm_size)*num_prbu;
                }
                else
                {
                    memcpy(pos + start_prbu*N_SC_PER_PRB(isNb375)*2*p_iq->iq_ul_bit, u32dptr,
                            num_prbu*N_SC_PER_PRB(isNb375)*2*p_iq->iq_ul_bit);
                }
            }
        }
    }
    else
    {
        printf("invalid rx_log_buffer\n");
        exit(-1);
    }

    return XRAN_STATUS_SUCCESS;
}

/* CP - DL for O-RU only */
int32_t app_io_xran_iq_content_get_cp_dl(uint8_t  appMode, struct xran_fh_config  *pXranConf,
                                  struct bbu_xran_io_if *psBbuIo, struct xran_io_shared_ctrl *psIoCtrl, struct o_xu_buffers * p_iq,
                                  int32_t cc_id, int32_t ant_id, int32_t sym_id, int32_t tti, int32_t flowId, uint8_t mu)
{
    xran_status_t status = 0;
    
    uint16_t idxElm = 0;
    int i = 0, len;
    uint8_t *src_buf;
    char *src = NULL;
    struct xran_prb_map *pRbMap = NULL;
    struct xran_prb_elm *pRbElm = NULL;
    int8_t *iq_data = NULL;
    uint16_t N = pXranConf->nAntElmTRx;
    uint8_t parm_size;
    int32_t tti_src =  tti % XRAN_N_FE_BUF_LEN;
    int32_t tti_dst =  tti % p_iq->numSlots ;
    int32_t tx_dl_bfw_buffer_position = tti_dst * (pXranConf->perMu[mu].nDLRBs*pXranConf->nAntElmTRx)*4;
    uint16_t iq_size;
    struct xran_cp_radioapp_section_ext1 * ext1;
    uint8_t bfwIqWidth;
    uint8_t total_ext1_len = 0;
    char *pos = NULL;

    pRbMap = (struct xran_prb_map *) psIoCtrl->io_buff_perMu[mu].sFHCpRxPrbMapBbuIoBufCtrl[tti_src][cc_id][ant_id].sBufferList.pBuffers->pData;
    if(pRbMap == NULL) {
        printf("pRbMap == NULL\n");
        return XRAN_STATUS_FAIL;
    }
    pos = (char*)p_iq->buff_perMu[mu].p_tx_dl_bfw_log_buffer[flowId] + tx_dl_bfw_buffer_position;
    for(idxElm = 0; idxElm < pRbMap->nPrbElm; idxElm++ ) {
        pRbElm = &pRbMap->prbMap[idxElm];
        bfwIqWidth = pRbElm->bf_weight.bfwIqWidth;
        if(p_iq->buff_perMu[mu].p_tx_dl_bfw_log_buffer[flowId]) {
            src = (char *)pRbElm->bf_weight.p_ext_section;
            if(!pRbElm->bf_weight.p_ext_start)
                continue;

            for(i = 0; i < (pRbElm->bf_weight.numSetBFWs); i++) {
                if(src){
                    src_buf = (uint8_t *)src;
                    ext1 = (struct xran_cp_radioapp_section_ext1 *)src_buf;
                    src_buf += sizeof(struct xran_cp_radioapp_section_ext1);

                    iq_data = (int8_t *)(src_buf);
                    total_ext1_len = ext1->extLen * XRAN_SECTIONEXT_ALIGN;
                    if (pRbElm->bf_weight.bfwCompMeth == XRAN_COMPMETHOD_NONE){
                        iq_size = N * bfwIqWidth * 2;  // total in bits
                        parm_size = iq_size>>3;        // total in bytes (/8)
                        if(iq_size%8) parm_size++;     // round up
                        len = parm_size;
                        memcpy(pos,iq_data,len);
                    }
                    else {
                        switch(pRbElm->bf_weight.bfwCompMeth) {
                        case XRAN_BFWCOMPMETHOD_BLKFLOAT:
                            parm_size = 1;
                            break;

                        case XRAN_BFWCOMPMETHOD_BLKSCALE:
                            parm_size = 1;
                            break;

                        case XRAN_BFWCOMPMETHOD_ULAW:
                            parm_size = 1;
                            break;

                        case XRAN_BFWCOMPMETHOD_BEAMSPACE:
                            parm_size = N>>3; if(N%8) parm_size++; parm_size *= 8;
                            break;

                        default:
                            parm_size = 0;
                        }
                        len = parm_size;
                        /* Get BF weights */
                        iq_size = N * bfwIqWidth * 2;  // total in bits
                        parm_size = iq_size>>3;        // total in bytes (/8)
                        if(iq_size%8) parm_size++;     // round up
                        len += parm_size;
                        struct xranlib_decompress_request  bfp_decom_req;
                        struct xranlib_decompress_response bfp_decom_rsp;

                        memset(&bfp_decom_req, 0, sizeof(struct xranlib_decompress_request));
                        memset(&bfp_decom_rsp, 0, sizeof(struct xranlib_decompress_response));

                        bfp_decom_req.data_in         = (int8_t*)iq_data;
                        bfp_decom_req.numRBs          = 1;
                        bfp_decom_req.numDataElements = N*2;
                        bfp_decom_req.len             = len;
                        bfp_decom_req.compMethod      = pRbElm->bf_weight.bfwCompMeth;
                        bfp_decom_req.iqWidth         = bfwIqWidth;

                        bfp_decom_rsp.data_out   = (int16_t *)(pos);
                        bfp_decom_rsp.len        = 0;
                        if (unlikely(status = xranlib_decompress_bfw(&bfp_decom_req, &bfp_decom_rsp) != XRAN_STATUS_SUCCESS)) { 
                            return XRAN_STATUS_FAIL;
                        }
                    }
                    pos += N*4;
                }
                src += (total_ext1_len + sizeof(struct xran_cp_radioapp_section1));
            }
        }
    }
    return XRAN_STATUS_SUCCESS;
}

/* CP - UL for O-RU only */
int32_t app_io_xran_iq_content_get_cp_ul(uint8_t  appMode, struct xran_fh_config  *pXranConf,
                                  struct bbu_xran_io_if *psBbuIo, struct xran_io_shared_ctrl *psIoCtrl, struct o_xu_buffers * p_iq,
                                  int32_t cc_id, int32_t ant_id, int32_t sym_id, int32_t tti, int32_t flowId, uint8_t mu)
{
    xran_status_t status = 0;
    uint16_t idxElm = 0;
    int i = 0, len;
    uint8_t *src_buf;
    char *src = NULL;
    struct xran_prb_map *pRbMap = NULL;
    struct xran_prb_elm *pRbElm = NULL;
    int8_t *iq_data = NULL;
    uint16_t N = pXranConf->nAntElmTRx;
    uint8_t parm_size;
    uint16_t iq_size;
    struct xran_cp_radioapp_section_ext1 * ext1;
    uint8_t bfwIqWidth;
    uint8_t total_ext1_len = 0;
    int32_t tti_src =  tti % XRAN_N_FE_BUF_LEN;
    int32_t tti_dst =  tti % p_iq->numSlots;
    int32_t tx_ul_bfw_buffer_position = tti_dst * (pXranConf->perMu[mu].nULRBs*pXranConf->nAntElmTRx)*4;
    char *pos = NULL;

    pRbMap = (struct xran_prb_map *) psIoCtrl->io_buff_perMu[mu].sFHCpTxPrbMapBbuIoBufCtrl[tti_src][cc_id][ant_id].sBufferList.pBuffers->pData;
    if(pRbMap == NULL) {
        printf("pRbMap == NULL\n");
        return XRAN_STATUS_FAIL;
    }
    pos = ((char*)p_iq->buff_perMu[mu].p_tx_ul_bfw_log_buffer[flowId]) + tx_ul_bfw_buffer_position;
    for(idxElm = 0; idxElm < pRbMap->nPrbElm; idxElm++ ) {
        pRbElm = &pRbMap->prbMap[idxElm];
        bfwIqWidth = pRbElm->bf_weight.bfwIqWidth;
        if(p_iq->buff_perMu[mu].p_tx_ul_bfw_log_buffer[flowId]) {
            src = (char *)pRbElm->bf_weight.p_ext_section;
            if(!pRbElm->bf_weight.p_ext_start)
                continue;

            for(i = 0; i < (pRbElm->bf_weight.numSetBFWs); i++) {
                if(src){
                    src_buf = (uint8_t *)src;
                    ext1 = (struct xran_cp_radioapp_section_ext1 *)src_buf;
                    src_buf += sizeof(struct xran_cp_radioapp_section_ext1);

                    iq_data = (int8_t *)(src_buf);
                    total_ext1_len = ext1->extLen * XRAN_SECTIONEXT_ALIGN;
                    if (pRbElm->bf_weight.bfwCompMeth == XRAN_COMPMETHOD_NONE){
                        iq_size = N * bfwIqWidth * 2;  // total in bits
                        parm_size = iq_size>>3;        // total in bytes (/8)
                        if(iq_size%8) parm_size++;     // round up
                        len = parm_size;
                        memcpy(pos,iq_data,len);
                    }
                    else {
                        switch(pRbElm->bf_weight.bfwCompMeth) {
                        case XRAN_BFWCOMPMETHOD_BLKFLOAT:
                            parm_size = 1;
                            break;

                        case XRAN_BFWCOMPMETHOD_BLKSCALE:
                            parm_size = 1;
                            break;

                        case XRAN_BFWCOMPMETHOD_ULAW:
                            parm_size = 1;
                            break;

                        case XRAN_BFWCOMPMETHOD_BEAMSPACE:
                            parm_size = N>>3; if(N%8) parm_size++; parm_size *= 8;
                            break;

                        default:
                            parm_size = 0;
                        }
                        len = parm_size;
                        /* Get BF weights */
                        iq_size = N * bfwIqWidth * 2;  // total in bits
                        parm_size = iq_size>>3;        // total in bytes (/8)
                        if(iq_size%8) parm_size++;     // round up
                        len += parm_size;
                        struct xranlib_decompress_request  bfp_decom_req;
                        struct xranlib_decompress_response bfp_decom_rsp;

                        memset(&bfp_decom_req, 0, sizeof(struct xranlib_decompress_request));
                        memset(&bfp_decom_rsp, 0, sizeof(struct xranlib_decompress_response));

                        bfp_decom_req.data_in         = (int8_t*)iq_data;
                        bfp_decom_req.numRBs          = 1;
                        bfp_decom_req.numDataElements = N*2;
                        bfp_decom_req.len             = len;
                        bfp_decom_req.compMethod      = pRbElm->bf_weight.bfwCompMeth;
                        bfp_decom_req.iqWidth         = bfwIqWidth;

                        bfp_decom_rsp.data_out   = (int16_t *)(pos);
                        bfp_decom_rsp.len        = 0;
                        if (unlikely(status = xranlib_decompress_bfw(&bfp_decom_req, &bfp_decom_rsp) != XRAN_STATUS_SUCCESS)) { 
                            return XRAN_STATUS_FAIL;
                        }
                        pos += N*4;
                    }
                    src += (total_ext1_len + sizeof(struct xran_cp_radioapp_section1));
                }
            }
        }
    }

    return XRAN_STATUS_SUCCESS;
}

int32_t
app_io_xran_iq_content_get(uint32_t o_xu_id, RuntimeConfig *p_o_xu_cfg)
{
    struct bbu_xran_io_if *psBbuIo = app_io_xran_if_get();
    struct xran_io_shared_ctrl *psIoCtrl = app_io_xran_if_ctrl_get(o_xu_id);
    xran_status_t status;
    int32_t nSectorIndex[XRAN_MAX_SECTOR_NR];
    int32_t nSectorNum;
    int32_t cc_id, ant_id, sym_id, tti, mu_idx;
    int32_t flowId;
    struct xran_fh_config  *pXranConf = &app_io_xran_fh_config[o_xu_id];

    uint32_t xran_max_antenna_nr = RTE_MAX(p_o_xu_cfg->numAxc, p_o_xu_cfg->numUlAxc);
    uint32_t xran_max_ant_array_elm_nr = RTE_MAX(p_o_xu_cfg->antElmTRx, xran_max_antenna_nr);
    uint32_t xran_max_antenna_nr_prach = RTE_MIN(xran_max_antenna_nr, XRAN_MAX_PRACH_ANT_NUM);

    struct o_xu_buffers *p_iq = NULL;

    if(psBbuIo == NULL)
        rte_panic("psBbuIo == NULL\n");

    if(psIoCtrl == NULL)
        rte_panic("psIoCtrl == NULL\n");

    for (nSectorNum = 0; nSectorNum < XRAN_MAX_SECTOR_NR; nSectorNum++) {
        nSectorIndex[nSectorNum] = nSectorNum;
    }

    nSectorNum = p_o_xu_cfg->numCC;
    printf ("app_io_xran_iq_content_get:RU %d mu %d numMu %d\n", o_xu_id, p_o_xu_cfg->mu_number[0], p_o_xu_cfg->numMu);

    if(p_o_xu_cfg->p_buff) {
        p_iq = p_o_xu_cfg->p_buff;
    } else {
        printf("Error p_o_xu_cfg->p_buff\n");
        exit(-1);
    }

    if(p_o_xu_cfg->p_buff) {
        p_iq = p_o_xu_cfg->p_buff;
    } else {
        rte_panic("Error p_o_xu_cfg->p_buff\n");
    }

    if(psBbuIo->bbu_offload == 0) {

        for(mu_idx = 0; mu_idx < p_o_xu_cfg->numMu; mu_idx++){
            uint8_t mu = p_o_xu_cfg->mu_number[mu_idx];
            for(cc_id = 0; cc_id <nSectorNum; cc_id++) {
                for(tti  = 0; tti  < XRAN_N_FE_BUF_LEN; tti++) {
                    for(ant_id = 0; ant_id < xran_max_antenna_nr; ant_id++) {
                        if(p_o_xu_cfg->appMode == APP_O_RU)
                            flowId = p_o_xu_cfg->numAxc * cc_id + ant_id;
                        else
                            flowId = p_o_xu_cfg->numUlAxc * cc_id + ant_id;

                        for(sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
                            if ((status = app_io_xran_iq_content_get_up_rx(p_o_xu_cfg->appMode, pXranConf,
                                    psBbuIo, psIoCtrl, p_iq,
                                    cc_id, ant_id, sym_id, tti, flowId, mu)) != 0) {
                                rte_panic("app_io_xran_iq_content_get_up_rx");
                            }
                        }
                        if(p_o_xu_cfg->appMode == APP_O_DU && p_o_xu_cfg->perMu[mu].prachEnable && (ant_id < xran_max_antenna_nr_prach)) {
                            flowId = xran_max_antenna_nr_prach * cc_id + ant_id;
                            for(sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
                                if ((status = app_io_xran_iq_content_get_up_prach(p_o_xu_cfg->appMode, pXranConf,
                                        psBbuIo, psIoCtrl, p_iq,
                                        cc_id, ant_id, sym_id, tti, flowId, mu)) != 0) {
                                    rte_panic("app_io_xran_iq_content_get_up_prach");
                                }
                            }
                        }
                    } /* for(ant_id = 0; ant_id < xran_max_antenna_nr; ant_id++) */

                    /* SRS RX for O-DU only */
                    if(p_o_xu_cfg->appMode == APP_O_DU && p_o_xu_cfg->enableSrs) {
                        for(ant_id = 0; ant_id < xran_max_ant_array_elm_nr; ant_id++) {
                            flowId = p_o_xu_cfg->antElmTRx*cc_id + ant_id;
                            for(sym_id = 0; sym_id < XRAN_MAX_NUM_OF_SRS_SYMBOL_PER_SLOT; sym_id++) {
                                if ((status = app_io_xran_iq_content_get_up_srs(p_o_xu_cfg->appMode, pXranConf,
                                        psBbuIo, psIoCtrl, p_iq,
                                        cc_id, ant_id, sym_id, tti, flowId, mu)) != 0) {
                                    rte_panic("app_io_xran_iq_content_get_up_srs");
                                }
                            }
                        }
                    }

                    /* CSI-RS Rx at O-RU */
                    if(p_o_xu_cfg->appMode == APP_O_RU && p_o_xu_cfg->csirsEnable) {
                        for(ant_id = 0; ant_id < p_o_xu_cfg->nCsiPorts; ant_id++) {
                            flowId = p_o_xu_cfg->nCsiPorts*cc_id + ant_id;
                            for(sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
                                if ((status = app_io_xran_iq_content_get_up_csirs(p_o_xu_cfg->appMode, pXranConf,
                                        psBbuIo, psIoCtrl, p_iq,
                                        cc_id, ant_id, sym_id, tti, flowId, mu)) != 0) {
                                    rte_panic("app_io_xran_iq_content_get_up_csirs");
                                }
                            }
                        }
                    }
                            
                    /* CP - DL for O-RU only */
                    if(p_o_xu_cfg->appMode == APP_O_RU && p_o_xu_cfg->xranCat == XRAN_CATEGORY_B) {
                        for(ant_id = 0; ant_id < xran_max_antenna_nr; ant_id++) {
                            flowId = p_o_xu_cfg->numAxc * cc_id + ant_id;
                            if ((status = app_io_xran_iq_content_get_cp_dl(p_o_xu_cfg->appMode, pXranConf,
                                        psBbuIo, psIoCtrl, p_iq,
                                        cc_id, ant_id, sym_id, tti, flowId, mu)) != 0) {
                                    rte_panic("app_io_xran_iq_content_get_cp_dl");
                                }
                        } 
                    }

                    /* CP - UL for O-RU only */
                    if(p_o_xu_cfg->appMode == APP_O_RU && p_o_xu_cfg->xranCat == XRAN_CATEGORY_B) {
                        for(ant_id = 0; ant_id < p_o_xu_cfg->numUlAxc; ant_id++) {
                            flowId = p_o_xu_cfg->numUlAxc * cc_id + ant_id;
                            if ((status = app_io_xran_iq_content_get_cp_ul(p_o_xu_cfg->appMode, pXranConf,
                                        psBbuIo, psIoCtrl, p_iq,
                                        cc_id, ant_id, sym_id, tti, flowId, mu)) != 0) {
                                    rte_panic("app_io_xran_iq_content_get_cp_ul");
                                }
                        } 
                    }

                } /*for(tti  = 0; tti  < XRAN_N_FE_BUF_LEN; tti++)*/
            } /*for(cc_id = 0; cc_id <nSectorNum; cc_id++)*/

        } /*for muIdx*/
    }
    return 0;
}

int32_t
app_io_xran_eAxCid_conf_set(struct xran_eaxcid_config *p_eAxC_cfg, RuntimeConfig * p_s_cfg)
{
    int32_t shift;
    uint16_t mask;

    if(p_s_cfg->DU_Port_ID_bitwidth && p_s_cfg->BandSector_ID_bitwidth && p_s_cfg->CC_ID_bitwidth
        && p_s_cfg->RU_Port_ID_bitwidth &&
        (p_s_cfg->DU_Port_ID_bitwidth + p_s_cfg->BandSector_ID_bitwidth + p_s_cfg->CC_ID_bitwidth
                 + p_s_cfg->RU_Port_ID_bitwidth) == 16 /* eAxC ID subfields are 16 bits */
        ){ /* bit mask provided */

        mask = 0;
        p_eAxC_cfg->bit_ruPortId = 0;
        for (shift = 0; shift < p_s_cfg->RU_Port_ID_bitwidth; shift++){
            mask |= 1 << shift;
        }
        p_eAxC_cfg->mask_ruPortId = mask;

        p_eAxC_cfg->bit_ccId = p_s_cfg->RU_Port_ID_bitwidth;
        mask = 0;
        for (shift = p_s_cfg->RU_Port_ID_bitwidth; shift < p_s_cfg->RU_Port_ID_bitwidth + p_s_cfg->CC_ID_bitwidth; shift++){
            mask |= 1 << shift;
        }
        p_eAxC_cfg->mask_ccId = mask;


        p_eAxC_cfg->bit_bandSectorId = p_s_cfg->RU_Port_ID_bitwidth + p_s_cfg->CC_ID_bitwidth;
        mask = 0;
        for (shift = p_s_cfg->RU_Port_ID_bitwidth + p_s_cfg->CC_ID_bitwidth; shift < p_s_cfg->RU_Port_ID_bitwidth + p_s_cfg->CC_ID_bitwidth + p_s_cfg->BandSector_ID_bitwidth; shift++){
            mask |= 1 << shift;
        }
        p_eAxC_cfg->mask_bandSectorId = mask;

        p_eAxC_cfg->bit_cuPortId = p_s_cfg->RU_Port_ID_bitwidth + p_s_cfg->CC_ID_bitwidth + p_s_cfg->BandSector_ID_bitwidth;
        mask = 0;
        for (shift = p_s_cfg->RU_Port_ID_bitwidth + p_s_cfg->CC_ID_bitwidth + p_s_cfg->BandSector_ID_bitwidth;
            shift < p_s_cfg->RU_Port_ID_bitwidth + p_s_cfg->CC_ID_bitwidth + p_s_cfg->BandSector_ID_bitwidth + p_s_cfg->DU_Port_ID_bitwidth; shift++){
            mask |= 1 << shift;
        }
        p_eAxC_cfg->mask_cuPortId = mask;


    } else { /* bit mask config is not provided */
        switch (p_s_cfg->xranCat){
            case XRAN_CATEGORY_A: {
                p_eAxC_cfg->mask_cuPortId      = 0xf000;
                p_eAxC_cfg->mask_bandSectorId  = 0x0f00;
                p_eAxC_cfg->mask_ccId          = 0x00f0;
                p_eAxC_cfg->mask_ruPortId      = 0x000f;
                p_eAxC_cfg->bit_cuPortId       = 12;
                p_eAxC_cfg->bit_bandSectorId   = 8;
                p_eAxC_cfg->bit_ccId           = 4;
                p_eAxC_cfg->bit_ruPortId       = 0;
                break;
            }
            case XRAN_CATEGORY_B: {
                p_eAxC_cfg->mask_cuPortId      = 0xf000;
                p_eAxC_cfg->mask_bandSectorId  = 0x0c00;
                p_eAxC_cfg->mask_ccId          = 0x0300;
                p_eAxC_cfg->mask_ruPortId      = 0x00ff; /* more than [0-127] eAxC */
                p_eAxC_cfg->bit_cuPortId       = 12;
                p_eAxC_cfg->bit_bandSectorId   = 10;
                p_eAxC_cfg->bit_ccId           = 8;
                p_eAxC_cfg->bit_ruPortId       = 0;
                break;
            }
            default:
                rte_panic("Incorrect Category\n");
        }
    }

    if(p_s_cfg->xranCat == XRAN_CATEGORY_A && !(p_s_cfg->numUlAxc))
        p_s_cfg->numUlAxc = p_s_cfg->numAxc;

    printf("eAxCiD config cat-%s\n",(p_s_cfg->xranCat == XRAN_CATEGORY_A)?"A":"B");
    printf("bit_cuPortId     %2d mask 0x%04x\n",p_eAxC_cfg->bit_cuPortId, p_eAxC_cfg->mask_cuPortId);
    printf("bit_bandSectorId %2d mask 0x%04x\n",p_eAxC_cfg->bit_bandSectorId, p_eAxC_cfg->mask_bandSectorId);
    printf("bit_ccId         %2d mask 0x%04x\n",p_eAxC_cfg->bit_ccId, p_eAxC_cfg->mask_ccId);
    printf("ruPortId         %2d mask 0x%04x\n",p_eAxC_cfg->bit_ruPortId, p_eAxC_cfg->mask_ruPortId);

    return 0;
}

int32_t
app_io_xran_fh_config_init(UsecaseConfig* p_use_cfg,  RuntimeConfig* p_o_xu_cfg, struct xran_fh_init* p_xran_fh_init, struct xran_fh_config*  p_xran_fh_cfg)
{
    int32_t ret = 0;
    int32_t i   = 0;
    int32_t o_xu_id      = 0;
    uint32_t nCenterFreq = 0;
    struct xran_prb_map* pRbMap = NULL;
    o_xu_id = p_o_xu_cfg->o_xu_id;
    
    memcpy(&p_xran_fh_cfg->perMu, &p_o_xu_cfg->perMu, sizeof(struct xran_fh_per_mu_cfg)* XRAN_MAX_NUM_MU);
    for(i = 0; i < p_o_xu_cfg->numMu; i++){
        uint8_t mu = p_o_xu_cfg->mu_number[i];
        p_xran_fh_cfg->perMu[mu].nDLRBs = app_xran_get_num_rbs(p_o_xu_cfg->xranTech, mu, p_o_xu_cfg->perMu[mu].nDLBandwidth, p_o_xu_cfg->nDLAbsFrePointA);
        p_xran_fh_cfg->perMu[mu].nULRBs = app_xran_get_num_rbs(p_o_xu_cfg->xranTech, mu, p_o_xu_cfg->perMu[mu].nULBandwidth, p_o_xu_cfg->nULAbsFrePointA);
        p_xran_fh_cfg->perMu[mu].eaxcOffset = p_o_xu_cfg->perMu[mu].eaxcOffset;
        
        if(p_o_xu_cfg->DynamicSectionEna == 0){
            pRbMap = p_o_xu_cfg->p_PrbMapDl[mu];
            pRbMap->dir = XRAN_DIR_DL;
            pRbMap->xran_port = 0;
            pRbMap->band_id = 0;
            pRbMap->cc_id = 0;
            pRbMap->ru_port_id = 0;
            pRbMap->tti_id = 0;
            pRbMap->start_sym_id = 0;
            pRbMap->nPrbElm = 1;
            pRbMap->prbMap[0].nStartSymb = 0;
            pRbMap->prbMap[0].numSymb = 14;
            pRbMap->prbMap[0].nRBStart = 0;
            pRbMap->prbMap[0].nRBSize = p_xran_fh_cfg->perMu[mu].nDLRBs;
            pRbMap->prbMap[0].nBeamIndex = 0;
            pRbMap->prbMap[0].compMethod = XRAN_COMPMETHOD_NONE;
            pRbMap->prbMap[0].iqWidth    = 16;

            pRbMap = p_o_xu_cfg->p_PrbMapUl[mu];
            pRbMap->dir = XRAN_DIR_UL;
            pRbMap->xran_port = 0;
            pRbMap->band_id = 0;
            pRbMap->cc_id = 0;
            pRbMap->ru_port_id = 0;
            pRbMap->tti_id = 0;
            pRbMap->start_sym_id = 0;
            pRbMap->nPrbElm = 1;
            pRbMap->prbMap[0].nStartSymb = 0;
            pRbMap->prbMap[0].numSymb = 14;
            pRbMap->prbMap[0].nRBStart = 0;
            pRbMap->prbMap[0].nRBSize = p_xran_fh_cfg->perMu[mu].nULRBs;
            pRbMap->prbMap[0].nBeamIndex = 0;
            pRbMap->prbMap[0].compMethod = XRAN_COMPMETHOD_NONE;
            pRbMap->prbMap[0].iqWidth    = 16;
        } else {
            pRbMap = p_o_xu_cfg->p_PrbMapDl[mu];
            pRbMap->dir = XRAN_DIR_DL;
            pRbMap->xran_port = 0;
            pRbMap->band_id = 0;
            pRbMap->cc_id = 0;
            pRbMap->ru_port_id = 0;
            pRbMap->tti_id = 0;
            pRbMap->start_sym_id = 0;

            pRbMap = p_o_xu_cfg->p_PrbMapUl[mu];
            pRbMap->dir = XRAN_DIR_UL;
            pRbMap->xran_port = 0;
            pRbMap->band_id = 0;
            pRbMap->cc_id = 0;
            pRbMap->ru_port_id = 0;
            pRbMap->tti_id = 0;
            pRbMap->start_sym_id = 0;

            pRbMap = p_o_xu_cfg->p_PrbMapSrs;
            pRbMap->dir = XRAN_DIR_UL;
            pRbMap->xran_port = 0;
            pRbMap->band_id = 0;
            pRbMap->cc_id = 0;
            pRbMap->ru_port_id = 0;
            pRbMap->tti_id = 0;
            pRbMap->start_sym_id = 0;
        }
        p_xran_fh_cfg->perMu[mu].prach_conf.nPrachSubcSpacing     = mu;
        p_xran_fh_cfg->perMu[mu].prach_conf.nPrachFreqStart       = 0;
        p_xran_fh_cfg->perMu[mu].prach_conf.nPrachFilterIdx       = XRAN_FILTERINDEX_PRACH_ABC;
        p_xran_fh_cfg->perMu[mu].prach_conf.nPrachConfIdx         = p_o_xu_cfg->perMu[mu].prachConfigIndex;
        p_xran_fh_cfg->perMu[mu].prach_conf.nPrachConfIdxLTE      = p_o_xu_cfg->perMu[mu].prachConfigIndexLTE; //will be used in case of dss only
        p_xran_fh_cfg->perMu[mu].prach_conf.nPrachFreqOffset      = -792; /*PRB start is marked as 0. hence this to be adjusted for NB-IOT*/

        p_xran_fh_cfg->srs_conf.symbMask                = p_o_xu_cfg->srsSymMask;   // deprecated

        if(mu==XRAN_NBIOT_MU){
            /*NPRACH Parameters to use only in case of NB-IOT: L1 should fill these */
            p_xran_fh_cfg->perMu[mu].prach_conf.nprachformat      = p_o_xu_cfg->nprachformat;
            p_xran_fh_cfg->perMu[mu].prach_conf.periodicity       = p_o_xu_cfg->periodicity;
            p_xran_fh_cfg->perMu[mu].prach_conf.startTime         = p_o_xu_cfg->startTime;
            p_xran_fh_cfg->perMu[mu].prach_conf.suboffset         = p_o_xu_cfg->suboffset;
            p_xran_fh_cfg->perMu[mu].prach_conf.numSubCarriers    = p_o_xu_cfg->numSubCarriers;
            p_xran_fh_cfg->perMu[mu].prach_conf.nRep              = p_o_xu_cfg->nRep;
            p_xran_fh_cfg->perMu[mu].prach_conf.nPrachFreqOffset  = p_o_xu_cfg->perMu[mu].freqOffset;
        }
        p_xran_fh_cfg->perMu[mu].prachEnable       = p_o_xu_cfg->perMu[mu].prachEnable;
    }

    if(p_o_xu_cfg->xranCat == XRAN_CATEGORY_A && !(p_o_xu_cfg->numUlAxc))
        p_o_xu_cfg->numUlAxc = p_o_xu_cfg->numAxc;

    p_xran_fh_cfg->sector_id                        = 0;
    p_xran_fh_cfg->dpdk_port                        = o_xu_id;
    p_xran_fh_cfg->nCC                              = p_o_xu_cfg->numCC;
    p_xran_fh_cfg->neAxc                            = p_o_xu_cfg->numAxc;
    p_xran_fh_cfg->neAxcUl                          = p_o_xu_cfg->numUlAxc;
    p_xran_fh_cfg->nAntElmTRx                       = p_o_xu_cfg->antElmTRx;

    p_xran_fh_cfg->frame_conf.nFrameDuplexType      = p_o_xu_cfg->nFrameDuplexType;
    p_xran_fh_cfg->numMUs                           = p_o_xu_cfg->numMu;
    p_xran_fh_cfg->frame_conf.nTddPeriod            = p_o_xu_cfg->nTddPeriod;

    for (i = 0; i < p_o_xu_cfg->nTddPeriod; i++){
        p_xran_fh_cfg->frame_conf.sSlotConfig[i] = p_o_xu_cfg->sSlotConfig[i];
    }

    /* TODO: xran code uses numEaxc for prach cp transmission regardless of whether it is
     * greater than XRAN_MAX_PRACH_ANT_NUM. This logic contradicts with that.
     * Also, srsEaxcOffset should be set on top of the antId offset for the numerology used for PDSCH, PUSCH.
     */
    if(p_o_xu_cfg->numAxc > XRAN_MAX_PRACH_ANT_NUM){
        p_xran_fh_cfg->srs_conf.srsEaxcOffset         = p_o_xu_cfg->numAxc + XRAN_MAX_PRACH_ANT_NUM; /* PUSCH, PRACH, SRS */
    }
    else{
        p_xran_fh_cfg->srs_conf.srsEaxcOffset           = 2 * p_o_xu_cfg->numAxc; /* PUSCH, PRACH, SRS */
    }
    p_xran_fh_cfg->csirs_conf.csirsEaxcOffset     = p_o_xu_cfg->numAxc;
    
    p_xran_fh_cfg->srs_conf.slot                    = p_o_xu_cfg->srsSlot;
    p_xran_fh_cfg->srs_conf.ndm_offset              = p_o_xu_cfg->srsNdmOffset;
    p_xran_fh_cfg->srs_conf.ndm_txduration          = p_o_xu_cfg->srsNdmTxDuration;

    p_xran_fh_cfg->ru_conf.xranTech                 = p_o_xu_cfg->xranTech;
    p_xran_fh_cfg->ru_conf.xranCompHdrType          = p_o_xu_cfg->CompHdrType;
    p_xran_fh_cfg->ru_conf.xranCat                  = p_o_xu_cfg->xranCat;

    p_xran_fh_cfg->ru_conf.iqWidth                  = p_o_xu_cfg->p_PrbMapDl[p_o_xu_cfg->mu_number[0]]->prbMap[0].iqWidth;

    if (p_o_xu_cfg->compression == 0)
        p_xran_fh_cfg->ru_conf.compMeth             = XRAN_COMPMETHOD_NONE;
    else
        p_xran_fh_cfg->ru_conf.compMeth             = XRAN_COMPMETHOD_BLKFLOAT;


    p_xran_fh_cfg->ru_conf.compMeth_PRACH           = p_o_xu_cfg->prachCompMethod; 
    if (p_o_xu_cfg->prachCompMethod == 0)
        p_o_xu_cfg->prachiqWidth = 16;
    p_xran_fh_cfg->ru_conf.iqWidth_PRACH            = p_o_xu_cfg->prachiqWidth;
    
    for(i=0; i < p_o_xu_cfg->numMu ; i++){
        uint8_t mu = p_o_xu_cfg->mu_number[i];
        p_xran_fh_cfg->mu_number[i] = p_o_xu_cfg->mu_number[i];
        p_xran_fh_cfg->ru_conf.fftSize[mu] = 0;
        while (p_o_xu_cfg->perMu[mu].nULFftSize >>= 1)
            ++p_xran_fh_cfg->ru_conf.fftSize[mu];
    }
    p_xran_fh_cfg->ru_conf.byteOrder = (p_o_xu_cfg->nebyteorderswap == 1) ? XRAN_NE_BE_BYTE_ORDER : XRAN_CPU_LE_BYTE_ORDER  ;
    p_xran_fh_cfg->ru_conf.iqOrder   = (p_o_xu_cfg->iqswap == 1) ? XRAN_Q_I_ORDER : XRAN_I_Q_ORDER;

    nCenterFreq = p_o_xu_cfg->nDLAbsFrePointA + (((p_xran_fh_cfg->perMu[p_o_xu_cfg->mu_number[0]].nDLRBs * N_SC_PER_PRB(false)) / 2) * app_xran_get_scs(p_o_xu_cfg->mu_number[0]));
    p_xran_fh_cfg->nDLCenterFreqARFCN = app_xran_cal_nrarfcn(nCenterFreq);
    printf("DL center freq %d DL NR-ARFCN  %d\n", nCenterFreq, p_xran_fh_cfg->nDLCenterFreqARFCN);

    nCenterFreq = p_o_xu_cfg->nULAbsFrePointA + (((p_xran_fh_cfg->perMu[p_o_xu_cfg->mu_number[0]].nULRBs * N_SC_PER_PRB(false)) / 2) * app_xran_get_scs(p_o_xu_cfg->mu_number[0]));
    p_xran_fh_cfg->nULCenterFreqARFCN = app_xran_cal_nrarfcn(nCenterFreq);
    printf("UL center freq %d UL NR-ARFCN  %d\n", nCenterFreq, p_xran_fh_cfg->nULCenterFreqARFCN);

    p_xran_fh_cfg->bbdev_dec = NULL;
    p_xran_fh_cfg->bbdev_enc = NULL;
    p_xran_fh_cfg->bbdev_srs_fft = NULL;
    p_xran_fh_cfg->bbdev_prach_ifft = NULL;

    p_xran_fh_cfg->log_level = 1;

    if(p_o_xu_cfg->max_sections_per_slot > XRAN_MAX_SECTIONS_PER_SLOT)
    {
        printf("Requested value for max sections per slot (%u) is greater than what we support (%u)\n",
                p_o_xu_cfg->max_sections_per_slot, XRAN_MAX_SECTIONS_PER_SLOT);
        exit(1);
    }

    p_xran_fh_cfg->max_sections_per_symbol = XRAN_MAX_SECTIONS_PER_SLOT;
    p_xran_fh_cfg->RunSlotPrbMapBySymbolEnable = p_o_xu_cfg->RunSlotPrbMapBySymbolEnable;

    printf("Max Sections: %d per symb %d per slot\n", p_xran_fh_cfg->max_sections_per_slot, p_xran_fh_cfg->max_sections_per_symbol);
    if(p_o_xu_cfg->maxFrameId)
        p_xran_fh_cfg->ru_conf.xran_max_frame = p_o_xu_cfg->maxFrameId;
    p_xran_fh_cfg->enableCP          = p_o_xu_cfg->enableCP;
    p_xran_fh_cfg->dropPacketsUp     = p_o_xu_cfg->DropPacketsUp;
    p_xran_fh_cfg->srsEnable         = p_o_xu_cfg->enableSrs;
    p_xran_fh_cfg->csirsEnable       = p_o_xu_cfg->csirsEnable;
    p_xran_fh_cfg->srsEnableCp       = 1;
    p_xran_fh_cfg->SrsDelaySym       = 4;
    p_xran_fh_cfg->puschMaskEnable   = p_o_xu_cfg->puschMaskEnable;
    p_xran_fh_cfg->puschMaskSlot     = p_o_xu_cfg->puschMaskSlot;
    p_xran_fh_cfg->debugStop         = p_o_xu_cfg->debugStop;
    p_xran_fh_cfg->debugStopCount    = p_o_xu_cfg->debugStopCount;
    p_xran_fh_cfg->DynamicSectionEna = p_o_xu_cfg->DynamicSectionEna;
    p_xran_fh_cfg->GPS_Alpha         = p_o_xu_cfg->GPS_Alpha;
    p_xran_fh_cfg->GPS_Beta          = p_o_xu_cfg->GPS_Beta;

    p_xran_fh_cfg->dssEnable = p_o_xu_cfg->dssEnable;
    p_xran_fh_cfg->dssPeriod = p_o_xu_cfg->dssPeriod;
    for(i=0; i<p_o_xu_cfg->dssPeriod; i++) {
        p_xran_fh_cfg->technology[i] = p_o_xu_cfg->technology[i];
    }

    return ret;
}

int32_t
app_io_xran_fh_init_init(UsecaseConfig* p_use_cfg,  RuntimeConfig* p_o_xu_cfg, struct xran_fh_init* p_xran_fh_init)
{
    int32_t ret = 0;
    int32_t i   = 0;
    int32_t o_xu_id      = 0;
    int32_t pf_link_id   = 0;
    int32_t num_vfs_cu_p = 2;
    void * ptr =  NULL;

    memset(p_xran_fh_init, 0, sizeof(struct xran_fh_init));

    if(p_o_xu_cfg->appMode == APP_O_DU) {
        printf("set O-DU\n");
        p_xran_fh_init->io_cfg.id = 0;/* O-DU */
        p_xran_fh_init->io_cfg.core          = p_use_cfg->io_core;
        p_xran_fh_init->io_cfg.system_core   = p_use_cfg->system_core;
        p_xran_fh_init->io_cfg.pkt_proc_core = p_use_cfg->io_worker; /* do not start */
        p_xran_fh_init->io_cfg.pkt_proc_core_64_127 = p_use_cfg->io_worker_64_127;
        p_xran_fh_init->io_cfg.pkt_aux_core  = 0; /* do not start*/
        p_xran_fh_init->io_cfg.timing_core   = p_use_cfg->io_core;
        p_xran_fh_init->io_cfg.dpdkIoVaMode  = p_use_cfg->iova_mode;
        p_xran_fh_init->io_cfg.eowd_cmn[APP_O_DU].initiator_en    = p_use_cfg->owdmInitEn;
        p_xran_fh_init->io_cfg.eowd_cmn[APP_O_DU].measMethod      = p_use_cfg->owdmMeasMeth;
        p_xran_fh_init->io_cfg.eowd_cmn[APP_O_DU].numberOfSamples = p_use_cfg->owdmNumSamps;
        p_xran_fh_init->io_cfg.eowd_cmn[APP_O_DU].filterType      = p_use_cfg->owdmFltType;
        p_xran_fh_init->io_cfg.eowd_cmn[APP_O_DU].responseTo      = p_use_cfg->owdmRspTo;
        p_xran_fh_init->io_cfg.eowd_cmn[APP_O_DU].measState       = p_use_cfg->owdmMeasState;
        p_xran_fh_init->io_cfg.eowd_cmn[APP_O_DU].measId          = p_use_cfg->owdmMeasId;
        p_xran_fh_init->io_cfg.eowd_cmn[APP_O_DU].owdm_enable     = p_use_cfg->owdmEnable;
        p_xran_fh_init->io_cfg.eowd_cmn[APP_O_DU].owdm_PlLength   = p_use_cfg->owdmPlLength;
        p_xran_fh_init->dlCpProcBurst = p_use_cfg->dlCpProcBurst;

    } else {
        printf("set O-RU\n");
        p_xran_fh_init->io_cfg.id = 1; /* O-RU*/
        p_xran_fh_init->io_cfg.core          = p_use_cfg->io_core;
        p_xran_fh_init->io_cfg.system_core   = p_use_cfg->system_core;
        p_xran_fh_init->io_cfg.pkt_proc_core = p_use_cfg->io_worker; /* do not start */
        p_xran_fh_init->io_cfg.pkt_proc_core_64_127 = p_use_cfg->io_worker_64_127;
        p_xran_fh_init->io_cfg.pkt_aux_core  = 0; /* do not start */
        p_xran_fh_init->io_cfg.timing_core   = p_use_cfg->io_core;
        p_xran_fh_init->io_cfg.dpdkIoVaMode  = p_use_cfg->iova_mode;
        p_xran_fh_init->io_cfg.eowd_cmn[APP_O_RU].initiator_en    = p_use_cfg->owdmInitEn;
        p_xran_fh_init->io_cfg.eowd_cmn[APP_O_RU].measMethod      = p_use_cfg->owdmMeasMeth;
        p_xran_fh_init->io_cfg.eowd_cmn[APP_O_RU].numberOfSamples = p_use_cfg->owdmNumSamps;
        p_xran_fh_init->io_cfg.eowd_cmn[APP_O_RU].filterType      = p_use_cfg->owdmFltType;
        p_xran_fh_init->io_cfg.eowd_cmn[APP_O_RU].responseTo      = p_use_cfg->owdmRspTo;
        p_xran_fh_init->io_cfg.eowd_cmn[APP_O_RU].measState       = p_use_cfg->owdmMeasState;
        p_xran_fh_init->io_cfg.eowd_cmn[APP_O_RU].measId          = p_use_cfg->owdmMeasId;
        p_xran_fh_init->io_cfg.eowd_cmn[APP_O_RU].owdm_enable     = p_use_cfg->owdmEnable;
        p_xran_fh_init->io_cfg.eowd_cmn[APP_O_RU].owdm_PlLength   = p_use_cfg->owdmPlLength;
    }

    if(p_use_cfg->bbu_offload) {
        if (p_xran_fh_init->io_cfg.id == 0) { /* O-DU */
            p_xran_fh_init->dlCpProcBurst  = 1;
        }
        p_xran_fh_init->io_cfg.bbu_offload    = 1;
    } else {
        p_xran_fh_init->io_cfg.bbu_offload    = 0;
    }

    if (p_xran_fh_init->io_cfg.bbu_offload == 0 && XRAN_N_FE_BUF_LEN < 20)
        rte_panic("Sample application with out BBU requires XRAN_N_FE_BUF_LEN to be at least 20 TTIs\n");

    p_xran_fh_init->io_cfg.io_sleep       = p_use_cfg->io_sleep;
    p_xran_fh_init->io_cfg.dpdkMemorySize = p_use_cfg->dpdk_mem_sz;
    p_xran_fh_init->io_cfg.bbdev_mode     = XRAN_BBDEV_NOT_USED;

    p_xran_fh_init->xran_ports             = p_use_cfg->oXuNum;
    p_xran_fh_init->io_cfg.nEthLinePerPort = p_use_cfg->EthLinesNumber;
    p_xran_fh_init->io_cfg.nEthLineSpeed   = p_use_cfg->EthLinkSpeed;

    if(p_use_cfg->mlogxrandisable == 1)
        p_xran_fh_init->mlogxranenable = 0;
    else
        p_xran_fh_init->mlogxranenable = 1;

    i = 0;

    if(p_use_cfg->one_vf_cu_plane == 1){
        num_vfs_cu_p = 1;
    }

    for(o_xu_id = 0; o_xu_id < p_use_cfg->oXuNum; o_xu_id++ ) { /* all O-XU */
        for(pf_link_id = 0; pf_link_id < p_use_cfg->EthLinesNumber && pf_link_id < XRAN_ETH_PF_LINKS_NUM; pf_link_id++ ) { /* all PF ports for each O-XU */
            if(num_vfs_cu_p*i < (XRAN_VF_MAX - 1)) {
                p_xran_fh_init->io_cfg.dpdk_dev[num_vfs_cu_p*i]   = &p_use_cfg->o_xu_pcie_bus_addr[o_xu_id][num_vfs_cu_p*pf_link_id][0]; /* U-Plane */
                rte_ether_addr_copy(&p_use_cfg->remote_o_xu_addr[o_xu_id][num_vfs_cu_p*pf_link_id],  &p_use_cfg->remote_o_xu_addr_copy[num_vfs_cu_p*i]);
                printf("VF[%d] %s\n",num_vfs_cu_p*i,    p_xran_fh_init->io_cfg.dpdk_dev[num_vfs_cu_p*i]);
                if(p_use_cfg->one_vf_cu_plane == 0){
                    p_xran_fh_init->io_cfg.dpdk_dev[num_vfs_cu_p*i+1] = &p_use_cfg->o_xu_pcie_bus_addr[o_xu_id][num_vfs_cu_p*pf_link_id+1][0]; /* C-Plane */
                    rte_ether_addr_copy(&p_use_cfg->remote_o_xu_addr[o_xu_id][num_vfs_cu_p*pf_link_id+1],  &p_use_cfg->remote_o_xu_addr_copy[num_vfs_cu_p*i+1]);
                    printf("VF[%d] %s\n",num_vfs_cu_p*i+1,  p_xran_fh_init->io_cfg.dpdk_dev[num_vfs_cu_p*i+1]);
                }
                i++;
            } else {
                break;
            }
        }
    }

    p_xran_fh_init->io_cfg.one_vf_cu_plane = p_use_cfg->one_vf_cu_plane;
    p_xran_fh_init->io_cfg.num_mbuf_alloc = NUM_MBUFS;
    p_xran_fh_init->io_cfg.num_mbuf_vf_alloc = NUM_MBUFS_VF;

    if(p_xran_fh_init->io_cfg.one_vf_cu_plane) {
        p_use_cfg->num_vfs = i;
    } else {
        p_use_cfg->num_vfs = 2*i;
    }
    printf("p_use_cfg->num_vfs %d\n", p_use_cfg->num_vfs);
    printf("p_use_cfg->num_rxq %d\n", p_use_cfg->num_rxq);

    p_xran_fh_init->io_cfg.num_vfs    = p_use_cfg->num_vfs;
    p_xran_fh_init->io_cfg.num_rxq    = p_use_cfg->num_rxq;
    p_xran_fh_init->mtu               = p_o_xu_cfg->mtu;
    if(p_use_cfg->appMode == APP_O_DU){
        p_xran_fh_init->p_o_du_addr = (int8_t *)p_o_xu_cfg->o_du_addr;
        p_xran_fh_init->p_o_ru_addr = (int8_t *)p_use_cfg->remote_o_xu_addr_copy;
    } else {
        p_xran_fh_init->p_o_du_addr = (int8_t *)p_use_cfg->remote_o_xu_addr_copy;
        p_xran_fh_init->p_o_ru_addr = (int8_t *)p_o_xu_cfg->o_ru_addr;
    }

    snprintf(p_use_cfg->prefix_name, sizeof(p_use_cfg->prefix_name), "wls_%d",p_use_cfg->instance_id);
    p_xran_fh_init->filePrefix        = p_use_cfg->prefix_name;
    p_xran_fh_init->totalBfWeights    = p_o_xu_cfg->totalBfWeights;


    for(o_xu_id = 0; o_xu_id < p_use_cfg->oXuNum && o_xu_id < XRAN_PORTS_NUM; o_xu_id++ ) { /* all O-XU */
        if(p_o_xu_buff[o_xu_id] == NULL) {
            ptr = _mm_malloc(sizeof(struct o_xu_buffers), 256);
            if (ptr == NULL) {
                rte_panic("_mm_malloc: Can't allocate %lu bytes\n", sizeof(struct o_xu_buffers));
            }
            p_o_xu_buff[o_xu_id] = (struct o_xu_buffers*)ptr;
        }

        p_o_xu_cfg->p_buff = p_o_xu_buff[o_xu_id];

        if(app_io_xran_eAxCid_conf_set(&p_xran_fh_init->eAxCId_conf[o_xu_id], p_o_xu_cfg) != 0)
        {
            printf("%s:%d: app_io_xran_eAxCid_conf_set failed", __func__, __LINE__);
            return -1;
        }

        p_o_xu_cfg++;
    }

    p_xran_fh_init->lbmEnable                          = p_use_cfg->lbmEnable;
    p_xran_fh_init->lbm_common_info.LBMPeriodicity     = p_use_cfg->LBMPeriodicity;
    p_xran_fh_init->lbm_common_info.LBRTimeOut         = p_use_cfg->LBRTimeOut;
    p_xran_fh_init->lbm_common_info.numRetransmissions = p_use_cfg->numRetransmissions;

    return ret;
}

int32_t
app_io_xran_buffers_max_sz_set (RuntimeConfig* p_o_xu_cfg)
{
    if(p_o_xu_cfg->max_sections_per_slot > XRAN_MAX_SECTIONS_PER_SLOT)
    {
        printf("Requested value for max sections per slot (%u) is greater than what we support (%u)\n",
                p_o_xu_cfg->max_sections_per_slot, XRAN_MAX_SECTIONS_PER_SLOT);
        exit(1);
    }
    uint32_t xran_max_sections_per_slot = XRAN_MAX_SECTIONS_PER_SLOT;


    if (p_o_xu_cfg->mu_number[0] <= 1){ /*CHECK: How these will change?*/
        if (p_o_xu_cfg->mtu > XRAN_MTU_DEFAULT) {
            nFpgaToSW_FTH_RxBufferLen    = 13168; /* 273*12*4 + 64*/
            nFpgaToSW_PRACH_RxBufferLen  = 8192;
            nSW_ToFpga_FTH_TxBufferLen   = 13168 + /* 273*12*4 + 64* + ETH AND ORAN HDRs */
                            xran_max_sections_per_slot* (RTE_PKTMBUF_HEADROOM + sizeof(struct rte_ether_hdr) +
                            sizeof(struct xran_ecpri_hdr) +
                            sizeof(struct radio_app_common_hdr) +
                            sizeof(struct data_section_hdr));
        } else {
            nFpgaToSW_FTH_RxBufferLen    = XRAN_MTU_DEFAULT; /* 273*12*4 + 64*/
            nFpgaToSW_PRACH_RxBufferLen  = XRAN_MTU_DEFAULT;
            nSW_ToFpga_FTH_TxBufferLen   = 13168 + /* 273*12*4 + 64* + ETH AND ORAN HDRs */
                            xran_max_sections_per_slot* (RTE_PKTMBUF_HEADROOM + sizeof(struct rte_ether_hdr) +
                            sizeof(struct xran_ecpri_hdr) +
                            sizeof(struct radio_app_common_hdr) +
                            sizeof(struct data_section_hdr));
        }
    } else if (p_o_xu_cfg->mu_number[0] == 3) {
        if (p_o_xu_cfg->mtu > XRAN_MTU_DEFAULT) {
            nFpgaToSW_FTH_RxBufferLen    = 3328;
            nFpgaToSW_PRACH_RxBufferLen  = 8192;
            nSW_ToFpga_FTH_TxBufferLen   = 3328 +
                        xran_max_sections_per_slot * (RTE_PKTMBUF_HEADROOM + sizeof(struct rte_ether_hdr) +
                        sizeof(struct xran_ecpri_hdr) +
                        sizeof(struct radio_app_common_hdr) +
                        sizeof(struct data_section_hdr));
        } else {
            nFpgaToSW_FTH_RxBufferLen    = XRAN_MTU_DEFAULT;
            nFpgaToSW_PRACH_RxBufferLen  = XRAN_MTU_DEFAULT;
            nSW_ToFpga_FTH_TxBufferLen   = 3328 +
                        xran_max_sections_per_slot * (RTE_PKTMBUF_HEADROOM + sizeof(struct rte_ether_hdr) +
                        sizeof(struct xran_ecpri_hdr) +
                        sizeof(struct radio_app_common_hdr) +
                        sizeof(struct data_section_hdr));
        }
    } else {
        printf("given numerology is not supported %d\n", p_o_xu_cfg->mu_number[0]);
        exit(-1);
    }
    printf("nSW_ToFpga_FTH_TxBufferLen %d\n", nSW_ToFpga_FTH_TxBufferLen);
    return 0;
}

int32_t
app_io_xran_map_cellid_to_port(struct bbu_xran_io_if * p_xran_io, uint32_t cell_id, uint32_t *ret_cc_id)
{
    int32_t port_id;
    int32_t cc_id;

    if(p_xran_io) {
        if(cell_id < XRAN_PORTS_NUM*XRAN_MAX_SECTOR_NR) {
            for (port_id = 0 ; port_id < XRAN_PORTS_NUM && port_id < p_xran_io->num_o_ru; port_id++) {
                for(cc_id = 0; cc_id < XRAN_MAX_SECTOR_NR && cc_id < p_xran_io->num_cc_per_port[port_id]; cc_id++)
                    if(cell_id == (uint32_t)p_xran_io->map_cell_id2port[port_id][cc_id]) {
                        if(ret_cc_id) {
                            *ret_cc_id = cc_id;
                            return port_id;
                    }
                }
            }
        }
    }

    printf("%s error [cell_id %d]\n", __FUNCTION__, cell_id);
    return -1;
}

#ifndef FWK_ENABLED
void
app_io_xran_fh_bbu_rx_callback(void *pCallbackTag, xran_status_t status, uint8_t mu)
{
    app_io_xran_fh_rx_callback(pCallbackTag, status,mu);
}

void
app_io_xran_fh_bbu_rx_bfw_callback(void *pCallbackTag, xran_status_t status, uint8_t mu)
{
    app_io_xran_fh_rx_bfw_callback(pCallbackTag, status, mu);
}

void
app_io_xran_fh_bbu_rx_prach_callback(void *pCallbackTag, xran_status_t status, uint8_t mu)
{
    app_io_xran_fh_rx_prach_callback(pCallbackTag, status, mu);
}

void
app_io_xran_fh_bbu_rx_srs_callback(void *pCallbackTag, xran_status_t status, uint8_t mu)
{
    app_io_xran_fh_rx_srs_callback(pCallbackTag, status, mu);
}
#endif

void
app_io_xran_fh_bbu_rx_csirs_callback(void *pCallbackTag, xran_status_t status, uint8_t mu)
{
    app_io_xran_fh_rx_csirs_callback(pCallbackTag, status, mu);
}
