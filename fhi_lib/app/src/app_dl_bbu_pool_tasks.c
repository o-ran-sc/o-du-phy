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
 * @brief This module provides implementation of BBU tasks for sample app
 * @file app_bbu.c
 * @ingroup xran
 * @author Intel Corporation
 *
 **/

#include <unistd.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <immintrin.h>

#include "common.h"
#include "app_bbu_pool.h"
#include "app_io_fh_xran.h"
#include "xran_compression.h"
#include "xran_cp_api.h"
#include "xran_fh_o_du.h"
#include "xran_mlog_task_id.h"

extern RuntimeConfig* p_startupConfiguration[XRAN_PORTS_NUM];
static SampleSplitStruct gsDlPostSymbolTaskSplit[MAX_PHY_INSTANCES][MAX_NUM_OF_SF_5G_CTX][MAX_TEST_SPLIT_NUM];
static SampleSplitStruct gsDlCfgAxCTaskSplit[MAX_PHY_INSTANCES][MAX_NUM_OF_SF_5G_CTX][MAX_TEST_SPLIT_NUM];

void app_bbu_pool_pre_task_dl_post(uint32_t nSubframe, uint16_t nCellIdx, TaskPreGen *pPara)
{
    int32_t nSplitGroup = 0;
    int32_t iTask = 0;
    uint32_t nSfIdx = get_dl_sf_idx(nSubframe, nCellIdx);
    uint32_t nCtxNum = get_dl_sf_ctx(nSfIdx, nCellIdx);
    SampleSplitStruct *pTaskSplitPara;
    int32_t nGroupNum = 0;
    int32_t nSymbStart = 0, nSymbPerSplit = 0;
    int32_t nTotalLayers = 0, nLayerStart = 0,  nLayerPerSplit = 0;
    struct bbu_xran_io_if *psXranIoIf  = app_io_xran_if_get();
    struct xran_fh_config*  pXranConf = NULL;
    // struct xran_io_shared_ctrl *psIoCtrl = NULL;
    uint32_t nRuCcidx = 0;
    int32_t xran_port = 0;

    if(psXranIoIf == NULL)
        rte_panic("psXranIoIf == NULL");

    if(nCellIdx >= MAX_PHY_INSTANCES)
        rte_panic("nCellIdx >= MAX_PHY_INSTANCES");

    xran_port = app_io_xran_map_cellid_to_port(psXranIoIf, nCellIdx, &nRuCcidx);

    if(xran_port < 0) {
        printf("incorrect xran_port\n");
        return /*EBBUPOOL_CORRECT*/;
    }

    // psIoCtrl = app_io_xran_if_ctrl_get(xran_port);
    pXranConf = &app_io_xran_fh_config[xran_port];
    if(pXranConf == NULL)
        rte_panic("pXranConf");

    nTotalLayers = pXranConf->neAxc;
    nSplitGroup  =  pXranConf->neAxc;

    /* all symp per eAxC */
    nSymbStart = 0;
    // nTotalSymb = XRAN_NUM_OF_SYMBOL_PER_SLOT;
    nSymbPerSplit  = XRAN_NUM_OF_SYMBOL_PER_SLOT;

    nLayerPerSplit = nTotalLayers/nSplitGroup;

    pPara->nTaskNum = nSplitGroup;
    for (iTask = 0; iTask < (nSplitGroup-1) && iTask < (MAX_TEST_SPLIT_NUM-1); iTask ++)
    {
        pTaskSplitPara = &(gsDlPostSymbolTaskSplit[nCellIdx][nCtxNum][iTask]);
        pTaskSplitPara->nSymbStart  = nSymbStart;
        pTaskSplitPara->nSymbNum    = nSymbPerSplit;
        pTaskSplitPara->eSplitType  = LAYER_SPLIT;
        pTaskSplitPara->nSplitIndex = iTask;
        pTaskSplitPara->nGroupStart = 0;
        pTaskSplitPara->nGroupNum   = nGroupNum;
        pTaskSplitPara->nLayerStart = nLayerStart;
        pTaskSplitPara->nLayerNum   = nLayerPerSplit;
        pPara->pTaskExePara[iTask]  = pTaskSplitPara;
        //nSymbStart += nSymbPerSplit;
        nLayerStart += nLayerPerSplit;
    }

    pTaskSplitPara = &(gsDlPostSymbolTaskSplit[nCellIdx][nCtxNum][iTask]);
    pTaskSplitPara->nSymbStart  = nSymbStart;
    pTaskSplitPara->nSymbNum    = nSymbPerSplit;
    pTaskSplitPara->eSplitType  = LAYER_SPLIT;
    pTaskSplitPara->nSplitIndex = iTask;
    pTaskSplitPara->nGroupStart = 0;
    pTaskSplitPara->nGroupNum   = nGroupNum;
    pTaskSplitPara->nLayerStart = nLayerStart;
    pTaskSplitPara->nLayerNum   = nTotalLayers - nLayerStart;
    pPara->pTaskExePara[iTask]  =  pTaskSplitPara;

    return;
}

void app_bbu_pool_pre_task_dl_cfg(uint32_t nSubframe, uint16_t nCellIdx, TaskPreGen *pPara)
{
    int32_t nSplitGroup = 0;
    int32_t iTask = 0;
    uint32_t nSfIdx = get_dl_sf_idx(nSubframe, nCellIdx);
    uint32_t nCtxNum = get_dl_sf_ctx(nSfIdx, nCellIdx);
    SampleSplitStruct *pTaskSplitPara;
    int32_t nGroupNum = 0;
    int32_t nSymbStart = 0, nSymbPerSplit = 0;
    int32_t nTotalLayers = 0, nLayerStart = 0,  nLayerPerSplit = 0;
    struct bbu_xran_io_if *psXranIoIf  = app_io_xran_if_get();
    struct xran_fh_config*  pXranConf = NULL;
    // struct xran_io_shared_ctrl *psIoCtrl = NULL;
    uint32_t nRuCcidx = 0;
    int32_t xran_port = 0;
    uint32_t neAxc = 0;

    if(psXranIoIf == NULL)
        rte_panic("psXranIoIf == NULL");

    if(nCellIdx >= MAX_PHY_INSTANCES)
        rte_panic("nCellIdx >= MAX_PHY_INSTANCES");

    xran_port = app_io_xran_map_cellid_to_port(psXranIoIf, nCellIdx, &nRuCcidx);

    if(xran_port < 0) {
        printf("incorrect xran_port\n");
        return /*EBBUPOOL_CORRECT*/;
    }

    // psIoCtrl = app_io_xran_if_ctrl_get(xran_port);
    pXranConf = &app_io_xran_fh_config[xran_port];
    if(pXranConf == NULL)
        rte_panic("pXranConf");

    pXranConf = &app_io_xran_fh_config[xran_port];
    if(pXranConf == NULL)
        rte_panic("pXranConf");

    if(pXranConf->ru_conf.xranCat == XRAN_CATEGORY_A){
        neAxc = pXranConf->neAxc;
        nSplitGroup  =  1;
    } else if (pXranConf->ru_conf.xranCat == XRAN_CATEGORY_B) {
        neAxc = pXranConf->neAxc;
        nSplitGroup  =  neAxc;
    } else
        rte_panic("neAxc");

    nTotalLayers = neAxc;

    /* all symb per eAxC */
    nSymbStart = 0;
    nSymbPerSplit  = XRAN_NUM_OF_SYMBOL_PER_SLOT;

    nLayerPerSplit = nTotalLayers/nSplitGroup;

    pPara->nTaskNum = nSplitGroup;
    for (iTask = 0; iTask < (nSplitGroup-1) && iTask < (MAX_TEST_SPLIT_NUM-1); iTask ++)
    {
        pTaskSplitPara = &(gsDlCfgAxCTaskSplit[nCellIdx][nCtxNum][iTask]);
        pTaskSplitPara->nSymbStart  = nSymbStart;
        pTaskSplitPara->nSymbNum    = nSymbPerSplit;
        pTaskSplitPara->eSplitType  = LAYER_SPLIT;
        pTaskSplitPara->nSplitIndex = iTask;
        pTaskSplitPara->nGroupStart = 0;
        pTaskSplitPara->nGroupNum   = nGroupNum;
        pTaskSplitPara->nLayerStart = nLayerStart;
        pTaskSplitPara->nLayerNum   = nLayerPerSplit;
        pPara->pTaskExePara[iTask]  = pTaskSplitPara;
        //nSymbStart += nSymbPerSplit;
        nLayerStart += nLayerPerSplit;
    }

    pTaskSplitPara = &(gsDlCfgAxCTaskSplit[nCellIdx][nCtxNum][iTask]);
    pTaskSplitPara->nSymbStart  = nSymbStart;
    pTaskSplitPara->nSymbNum    = nSymbPerSplit;
    pTaskSplitPara->eSplitType  = LAYER_SPLIT;
    pTaskSplitPara->nSplitIndex = iTask;
    pTaskSplitPara->nGroupStart = 0;
    pTaskSplitPara->nGroupNum   = nGroupNum;
    pTaskSplitPara->nLayerStart = nLayerStart;
    pTaskSplitPara->nLayerNum   = nTotalLayers - nLayerStart;
    pPara->pTaskExePara[iTask]  =  pTaskSplitPara;

    return;
}

//-------------------------------------------------------------------------------------------
/** @ingroup xran
*
*  @param[in]   pCookies task input parameter
*  @return  0 if SUCCESS
*
*  @description
*  This function takes the DL Config from MAC and stores it into PHY Internal structures.
*  and initials the parameter of UL DCI.
*
**/
//-------------------------------------------------------------------------------------------
int32_t
app_bbu_pool_task_dl_config(void *pCookies)
{
    EventCtrlStruct *pEventCtrl = (EventCtrlStruct *)pCookies;
    uint16_t nCellIdx = pEventCtrl->nCellIdx;
    uint32_t nSfIdx = get_dl_sf_idx(pEventCtrl->nSlotIdx, nCellIdx);
    uint32_t nCtxNum = get_dl_sf_ctx(nSfIdx, nCellIdx);
    uint32_t mlogVariablesCnt, mlogVariables[50];
    uint64_t mlog_start = MLogTick();
    uint32_t nRuCcidx = 0;
    int32_t xran_port = 0;
    SampleSplitStruct *pTaskPara = (SampleSplitStruct*)pEventCtrl->pTaskPara;
    struct bbu_xran_io_if *psXranIoIf  = app_io_xran_if_get();
    struct xran_fh_config*  pXranConf = NULL;
    xran_status_t status;
    struct xran_io_shared_ctrl *psIoCtrl = NULL;
    int32_t cc_id, ant_id, sym_id, tti;
    int32_t flowId;
    struct o_xu_buffers    * p_iq     = NULL;
    int32_t nSymbMask = 0b11111111111111;
    RuntimeConfig *p_o_xu_cfg = NULL;
    uint16_t nLayerStart = 0, nLayer = 0;

    if(psXranIoIf == NULL)
        rte_panic("psXranIoIf == NULL");

    xran_port = app_io_xran_map_cellid_to_port(psXranIoIf, nCellIdx, &nRuCcidx);

    if(xran_port < 0) {
        printf("incorrect xran_port\n");
        return EBBUPOOL_CORRECT;
    }
    psIoCtrl = app_io_xran_if_ctrl_get(xran_port);
    pXranConf = &app_io_xran_fh_config[xran_port];
    if(pXranConf == NULL)
        rte_panic("pXranConf");

    mlogVariablesCnt = 0;
    mlogVariables[mlogVariablesCnt++] = 0xCCBBCCBB;
    mlogVariables[mlogVariablesCnt++] = pEventCtrl->nSlotIdx;
    mlogVariables[mlogVariablesCnt++] = 0;
    mlogVariables[mlogVariablesCnt++] = nCellIdx;
    mlogVariables[mlogVariablesCnt++] = nSfIdx;
    mlogVariables[mlogVariablesCnt++] = nCtxNum;
    mlogVariables[mlogVariablesCnt++] = xran_port;
    mlogVariables[mlogVariablesCnt++] = nRuCcidx;

    p_o_xu_cfg = p_startupConfiguration[xran_port];


    mlog_start = MLogTick();

    if(LAYER_SPLIT == pTaskPara->eSplitType) {
        // iSplit = pTaskPara->nSplitIndex;
        nLayerStart = pTaskPara->nLayerStart;
        nLayer      = pTaskPara->nLayerNum;
        //printf("\nsf %d nSymbStart %d nSymb %d iSplit %d", nSfIdx, nSymbStart, nSymb, iSplit);
    } else {
        rte_panic("LAYER_SPLIT == pTaskPara->eSplitType");
    }

    if(p_o_xu_cfg->p_buff) {
        p_iq = p_o_xu_cfg->p_buff;
    } else {
        rte_panic("Error p_o_xu_cfg->p_buff\n");
    }
    tti = nSfIdx;
    for(cc_id = nRuCcidx; cc_id < psXranIoIf->num_cc_per_port[xran_port]; cc_id++) {
        if (cc_id >= XRAN_MAX_SECTOR_NR)
        {
            rte_panic("cell id %d exceeding max number", cc_id);
        }
        for(ant_id = nLayerStart; ant_id < (nLayerStart + nLayer); ant_id++) {
            if(p_o_xu_cfg->appMode == APP_O_DU) {
                flowId = p_o_xu_cfg->numAxc * cc_id + ant_id;
            } else {
                flowId = p_o_xu_cfg->numUlAxc * cc_id + ant_id;
            }
            for(sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
                if(((1 << sym_id) & nSymbMask)) {
                    if ((status = app_io_xran_iq_content_init_cp_tx(p_o_xu_cfg->appMode, pXranConf,
                                                    psXranIoIf, psIoCtrl, p_iq,
                                                    cc_id, ant_id, sym_id, tti, flowId)) != 0) {
                        rte_panic("app_io_xran_iq_content_init_cp_tx");
                    }
                }
            }
        }
    }

    xran_prepare_cp_dl_slot(xran_port, nSfIdx, nRuCcidx, /*psXranIoIf->num_cc_per_port[xran_port]*/ 1, nSymbMask, nLayerStart,
                            nLayer, 0, XRAN_NUM_OF_SYMBOL_PER_SLOT);

    if (mlogVariablesCnt)
        MLogAddVariables((uint32_t)mlogVariablesCnt, (uint32_t *)mlogVariables, mlog_start);

    //unlock the next task
    next_event_unlock(pCookies);
    MLogTask(PCID_GNB_DL_CFG_CC0+nCellIdx, mlog_start, MLogTick());

    return EBBUPOOL_CORRECT;
}

int32_t
app_io_xran_dl_pack_func(uint16_t nCellIdx, uint32_t nSfIdx, uint32_t nSymMask,
    uint32_t nAntStart, uint32_t nAntNum, uint32_t nSymStart, uint32_t nSymNum)
{
    xran_status_t status;
    uint32_t nSlotIdx = get_dl_sf_idx(nSfIdx, nCellIdx);
    // struct xran_io_shared_ctrl * psBbuXranIo = NULL;
    struct bbu_xran_io_if *psXranIoIf  = app_io_xran_if_get();
    int32_t xran_port = 0;
    uint32_t nRuCcidx = 0;
    struct o_xu_buffers    * p_iq     = NULL;
    RuntimeConfig *p_o_xu_cfg = NULL;
    int32_t flowId = 0;
    struct xran_fh_config  *pXranConf = NULL;
    int32_t cc_id, ant_id, sym_id, tti;
    struct xran_io_shared_ctrl *psIoCtrl = NULL;

    xran_port = app_io_xran_map_cellid_to_port(psXranIoIf, nCellIdx, &nRuCcidx);

    if(xran_port < 0)
    {
        printf("incorrect xran_port\n");
        return FAILURE;
    }

    psIoCtrl = app_io_xran_if_ctrl_get(xran_port);

    if(psIoCtrl == NULL) {
        printf("psIoCtrl == NULL\n");
        return FAILURE;
    }

    p_o_xu_cfg = p_startupConfiguration[xran_port];
    if(p_o_xu_cfg == NULL) {
        printf("p_o_xu_cfg == NULL\n");
        return FAILURE;
    }

    if(p_o_xu_cfg->p_buff) {
        p_iq = p_o_xu_cfg->p_buff;
    } else {
        rte_panic("Error p_o_xu_cfg->p_buff\n");
    }

    pXranConf = &app_io_xran_fh_config[xran_port];

    tti = nSlotIdx;
    for(cc_id = nRuCcidx; cc_id < psXranIoIf->num_cc_per_port[xran_port]; cc_id++) {
        for(ant_id = nAntStart; ant_id < (nAntStart + nAntNum) && ant_id <  pXranConf->neAxc; ant_id++) {
            if(p_o_xu_cfg->appMode == APP_O_DU) {
                flowId = p_o_xu_cfg->numAxc * cc_id + ant_id;
            } else {
                flowId = p_o_xu_cfg->numUlAxc * cc_id + ant_id;
            }
            for(sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
                if(((1 << sym_id) & nSymMask)) {
                    if ((status = app_io_xran_iq_content_init_up_tx(p_o_xu_cfg->appMode, pXranConf,
                                                    psXranIoIf, psIoCtrl, p_iq,
                                                    cc_id, ant_id, sym_id, tti, flowId)) != 0) {
                        rte_panic("app_io_xran_iq_content_init_up_tx");
                    }
                }
            }
        }
    }

    xran_prepare_up_dl_sym(xran_port, nSlotIdx, nRuCcidx, 1, nSymMask, nAntStart, nAntNum, nSymStart, nSymNum);
    return SUCCESS;
}

int32_t
app_io_xran_dl_post_func(uint16_t nCellIdx, uint32_t nSfIdx, uint32_t nSymMask,  uint32_t nAntStart, uint32_t nAntNum)
{
    uint16_t phyInstance = nCellIdx;
    // uint32_t Ntx_antennas;
    uint16_t nOranCellIdx;

    uint64_t tTotal = MLogTick();
    // struct xran_io_shared_ctrl * psBbuXranIo = NULL;
    struct bbu_xran_io_if *psXranIoIf  = app_io_xran_if_get();
    int32_t xran_port = 0;
    uint32_t nRuCcidx = 0;
    // struct xran_fh_config  *pXranConf = NULL;

    nSymMask = nSymMask + 0;

    nOranCellIdx = nCellIdx;
    xran_port =  app_io_xran_map_cellid_to_port(psXranIoIf, nOranCellIdx, &nRuCcidx);
    if(xran_port < 0) {
        printf("incorrect xran_port\n");
        return FAILURE;
    }

    // pXranConf = &app_io_xran_fh_config[xran_port];
//    Ntx_antennas = pXranConf->neAxc;

    app_io_xran_dl_pack_func(nCellIdx, nSfIdx, nSymMask, nAntStart, nAntNum, 0, XRAN_NUM_OF_SYMBOL_PER_SLOT);

    MLogTask(PCID_GNB_DL_IQ_COMPRESS_CC0 + phyInstance, tTotal, MLogTick());

    return SUCCESS;
}

//-------------------------------------------------------------------------------------------
/** @ingroup group_nr5g_source_phy_pdsch
*
*  @param[in]   pCookies task input parameter
*  @return  0 if SUCCESS
*
*  @description
*  This function will reset phy dl buffers.
*
**/
//-------------------------------------------------------------------------------------------
int32_t app_bbu_pool_task_dl_post(void *pCookies)
{
    EventCtrlStruct *pEventCtrl = (EventCtrlStruct *)pCookies;
    uint16_t nCellIdx  = pEventCtrl->nCellIdx;
    uint32_t nSfIdx    = get_dl_sf_idx(pEventCtrl->nSlotIdx, nCellIdx);
    SampleSplitStruct *pTaskPara = (SampleSplitStruct*)pEventCtrl->pTaskPara;
    uint16_t nSymbStart = 0, nSymb = 0, iOfdmSymb = 0, iSplit = 0;
    uint32_t nSymMask = 0;
    uint64_t mlog_start;
    uint32_t mlogVar[10];
    uint32_t mlogVarCnt = 0;
    uint16_t nLayerStart = 0, nLayer = 0;
    mlog_start = MLogTick();

    if(LAYER_SPLIT == pTaskPara->eSplitType) {
        nSymbStart = pTaskPara->nSymbStart;
        nSymb = pTaskPara->nSymbNum;
        iSplit = pTaskPara->nSplitIndex;
        nLayerStart = pTaskPara->nLayerStart;
        nLayer      = pTaskPara->nLayerNum;
        //printf("\nsf %d nSymbStart %d nSymb %d iSplit %d", nSfIdx, nSymbStart, nSymb, iSplit);
    } else if(OFDM_SYMB_SPLIT == pTaskPara->eSplitType) {
        nSymbStart = pTaskPara->nSymbStart;
        nSymb = pTaskPara->nSymbNum;
        iSplit = pTaskPara->nSplitIndex;
        rte_panic("\nsf %d nSymbStart %d nSymb %d iSplit %d", nSfIdx, nSymbStart, nSymb, iSplit);
    } else {
        rte_panic("OFDM_SYMB_SPLIT == pTaskPara->eSplitType");
    }

    // This is the loop of real OFDM symbol index
    for(iOfdmSymb = nSymbStart; iOfdmSymb < (nSymbStart + nSymb); iOfdmSymb ++)
        nSymMask |= (1 << iOfdmSymb);

    app_io_xran_dl_post_func(pEventCtrl->nCellIdx, pEventCtrl->nSlotIdx, /*0x3FFF*/ nSymMask, nLayerStart, nLayer);

#if 1
    {
        mlogVar[mlogVarCnt++] = 0xefefefef;
        mlogVar[mlogVarCnt++] = nCellIdx;
        mlogVar[mlogVarCnt++] = nSfIdx;
        mlogVar[mlogVarCnt++] = nSymbStart;
        mlogVar[mlogVarCnt++] = nSymb;
        mlogVar[mlogVarCnt++] = nLayerStart;
        mlogVar[mlogVarCnt++] = nLayer;
        mlogVar[mlogVarCnt++] = iSplit;
        MLogAddVariables(mlogVarCnt, mlogVar, mlog_start);
    }
#endif

    //unlock the next task
    next_event_unlock(pCookies);

    MLogTask(PCID_GNB_DL_POST_CC0+nCellIdx, mlog_start, MLogTick());
    return EBBUPOOL_CORRECT;
}

