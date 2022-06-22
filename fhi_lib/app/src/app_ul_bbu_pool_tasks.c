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


/*******************************************************************************
 * Include public/global header files
 *******************************************************************************/
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
static SampleSplitStruct gsUlCfgAxCTaskSplit[MAX_PHY_INSTANCES][MAX_NUM_OF_SF_5G_CTX][MAX_TEST_SPLIT_NUM];

void app_bbu_pool_pre_task_ul_cfg(uint32_t nSubframe, uint16_t nCellIdx, TaskPreGen *pPara)
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
    uint32_t  neAxc = 0;

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

    if(pXranConf->ru_conf.xranCat == XRAN_CATEGORY_A) {
        neAxc = pXranConf->neAxc;
        nSplitGroup  =  1;
    } else if (pXranConf->ru_conf.xranCat == XRAN_CATEGORY_B) {
        neAxc = pXranConf->neAxcUl;
        nSplitGroup  =  neAxc;
    } else
        rte_panic("neAxc");

    nTotalLayers = neAxc;

    /* all symb per eAxC */
    nSymbStart = 0;
    // nTotalSymb = XRAN_NUM_OF_SYMBOL_PER_SLOT;
    nSymbPerSplit  = XRAN_NUM_OF_SYMBOL_PER_SLOT;

    nLayerPerSplit = nTotalLayers/nSplitGroup;

    pPara->nTaskNum = nSplitGroup;
    for (iTask = 0; iTask < (nSplitGroup-1) && iTask < (MAX_TEST_SPLIT_NUM-1); iTask ++)
    {
        pTaskSplitPara = &(gsUlCfgAxCTaskSplit[nCellIdx][nCtxNum][iTask]);
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

    pTaskSplitPara = &(gsUlCfgAxCTaskSplit[nCellIdx][nCtxNum][iTask]);
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

/*! \brief Task function for UL configuration in PHY.
    \param [in] pCookies Task input parameter.
    \return BBU pool state
*/
int32_t app_bbu_pool_task_ul_config(void * pCookies)
{
    EventCtrlStruct *pEventCtrl = (EventCtrlStruct *)pCookies;
    uint16_t nCellIdx = pEventCtrl->nCellIdx;
    uint32_t nSfIdx = get_ul_sf_idx(pEventCtrl->nSlotIdx, nCellIdx);
    uint32_t nCtxNum = get_ul_sf_ctx(nSfIdx, nCellIdx);
    uint64_t mlog_start = MLogTick();// nTtiStartTime = gTtiStartTime;
    uint32_t mlogVariablesCnt, mlogVariables[50];
    uint32_t nRuCcidx = 0;
    int32_t xran_port = 0;
    struct bbu_xran_io_if *psXranIoIf  = app_io_xran_if_get();
    struct xran_fh_config*  pXranConf = NULL;
    // uint32_t  neAxc = 0;
    xran_status_t status;
    struct xran_io_shared_ctrl *psIoCtrl = NULL;
    int32_t cc_id, ant_id, sym_id, tti;
    int32_t flowId;
    struct o_xu_buffers    * p_iq     = NULL;
    int32_t nSymbMask = 0b11111111111111;
    RuntimeConfig *p_o_xu_cfg = NULL;
    SampleSplitStruct *pTaskPara = (SampleSplitStruct*)pEventCtrl->pTaskPara;
    uint16_t nLayerStart = 0, nLayer = 0;//, iSplit =0;

    if(psXranIoIf == NULL)
        rte_panic("psXranIoIf == NULL");

    xran_port = app_io_xran_map_cellid_to_port(psXranIoIf, nCellIdx, &nRuCcidx);

    if(xran_port < 0) {
        printf("incorrect xran_port\n");
        return EBBUPOOL_CORRECT;
    }
    psIoCtrl = app_io_xran_if_ctrl_get(xran_port);
    if(psIoCtrl == NULL)
        rte_panic("psIoCtrl");

    pXranConf = &app_io_xran_fh_config[xran_port];
    if(pXranConf == NULL)
        rte_panic("pXranConf");

#if 0
    if(pXranConf->ru_conf.xranCat == XRAN_CATEGORY_A)
        neAxc = pXranConf->neAxc;
    else if (pXranConf->ru_conf.xranCat == XRAN_CATEGORY_B)
        neAxc = pXranConf->neAxcUl;
    else
        rte_panic("neAxc");
#endif
    mlogVariablesCnt = 0;
    mlogVariables[mlogVariablesCnt++] = 0xCCEECCEE;
    mlogVariables[mlogVariablesCnt++] = pEventCtrl->nSlotIdx;
    mlogVariables[mlogVariablesCnt++] = 0;
    mlogVariables[mlogVariablesCnt++] = nCellIdx;
    mlogVariables[mlogVariablesCnt++] = nSfIdx;
    mlogVariables[mlogVariablesCnt++] = nCtxNum;
    mlogVariables[mlogVariablesCnt++] = xran_port;
    mlogVariables[mlogVariablesCnt++] = nRuCcidx;

    p_o_xu_cfg = p_startupConfiguration[xran_port];
    if(p_o_xu_cfg == NULL)
            rte_panic("p_o_xu_cfg");

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
                    if ((status = app_io_xran_iq_content_init_cp_rx(p_o_xu_cfg->appMode, pXranConf,
                                                    psXranIoIf, psIoCtrl, p_iq,
                                                    cc_id, ant_id, sym_id, tti, flowId)) != 0) {
                        rte_panic("app_io_xran_iq_content_init_cp_rx");
                    }
                }
            }
        }
    }

    xran_prepare_cp_ul_slot(xran_port, nSfIdx, nRuCcidx, /*psXranIoIf->num_cc_per_port[xran_port]*/ 1, nSymbMask, nLayerStart,
                            nLayer, 0, XRAN_NUM_OF_SYMBOL_PER_SLOT);

    if (mlogVariablesCnt)
        MLogAddVariables((uint32_t)mlogVariablesCnt, (uint32_t *)mlogVariables, mlog_start);

    //unlock the next task
    next_event_unlock(pCookies);
    MLogTask(PCID_GNB_UL_CFG_CC0+nCellIdx, mlog_start, MLogTick());

    return EBBUPOOL_CORRECT;
}

int32_t
app_io_xran_ul_decomp_func(uint16_t nCellIdx, uint32_t nSfIdx, uint32_t nSymMask,
                          uint32_t nAntStart, uint32_t nAntNum, uint32_t nSymStart, uint32_t nSymNum)
{
    xran_status_t status;
    struct bbu_xran_io_if *psXranIoIf  = app_io_xran_if_get();
    int32_t xran_port = 0;
    uint32_t nRuCcidx = 0;
    struct o_xu_buffers    * p_iq     = NULL;
    RuntimeConfig *p_o_xu_cfg = NULL;
    int32_t flowId = 0;
    struct xran_fh_config  *pXranConf = NULL;
    int32_t cc_id, ant_id, sym_id, tti;
    struct xran_io_shared_ctrl *psIoCtrl = NULL;
    uint32_t xran_max_antenna_nr;
    // uint32_t xran_max_ant_array_elm_nr;
    // uint32_t xran_max_antenna_nr_prach;

    xran_port = app_io_xran_map_cellid_to_port(psXranIoIf, nCellIdx, &nRuCcidx);

    if(xran_port < 0) {
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

    xran_max_antenna_nr = RTE_MAX(p_o_xu_cfg->numAxc, p_o_xu_cfg->numUlAxc);
    // xran_max_ant_array_elm_nr = RTE_MAX(p_o_xu_cfg->antElmTRx, xran_max_antenna_nr);
    // xran_max_antenna_nr_prach = RTE_MIN(xran_max_antenna_nr, XRAN_MAX_PRACH_ANT_NUM);

    tti = nSfIdx;
    for(cc_id = nRuCcidx; cc_id < psXranIoIf->num_cc_per_port[xran_port]; cc_id++) {
        for(ant_id = nAntStart; ant_id < (nAntStart + nAntNum) && ant_id <  xran_max_antenna_nr; ant_id++) {
            if(p_o_xu_cfg->appMode == APP_O_DU) {
                flowId = p_o_xu_cfg->numUlAxc * cc_id + ant_id;
            } else {
                flowId = p_o_xu_cfg->numAxc * cc_id + ant_id;
            }
            for(sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
                if(((1 << sym_id) & nSymMask)) {
                    if ((status = app_io_xran_iq_content_get_up_rx(p_o_xu_cfg->appMode, pXranConf,
                                                    psXranIoIf, psIoCtrl, p_iq,
                                                    cc_id, ant_id, sym_id, tti, flowId)) != 0) {
                        rte_panic("app_io_xran_iq_content_get_up_rx");
                    }
                }
            }
        }
    }

    return SUCCESS;
}

int32_t
app_io_xran_prach_decomp_func(uint16_t nCellIdx, uint32_t nSfIdx, uint32_t nSymMask,
                          uint32_t nAntStart, uint32_t nAntNum, uint32_t nSymStart, uint32_t nSymNum)
{
    xran_status_t status;
    struct bbu_xran_io_if *psXranIoIf  = app_io_xran_if_get();
    int32_t xran_port = 0;
    uint32_t nRuCcidx = 0;
    struct o_xu_buffers    * p_iq     = NULL;
    RuntimeConfig *p_o_xu_cfg = NULL;
    int32_t flowId = 0;
    struct xran_fh_config  *pXranConf = NULL;
    int32_t cc_id, ant_id, sym_id, tti;
    struct xran_io_shared_ctrl *psIoCtrl = NULL;
    uint32_t xran_max_antenna_nr;
    // uint32_t xran_max_ant_array_elm_nr;
    uint32_t xran_max_antenna_nr_prach;

    xran_port = app_io_xran_map_cellid_to_port(psXranIoIf, nCellIdx, &nRuCcidx);

    if(xran_port < 0) {
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

    xran_max_antenna_nr = RTE_MAX(p_o_xu_cfg->numAxc, p_o_xu_cfg->numUlAxc);
    // xran_max_ant_array_elm_nr = RTE_MAX(p_o_xu_cfg->antElmTRx, xran_max_antenna_nr);
    xran_max_antenna_nr_prach = RTE_MIN(xran_max_antenna_nr, XRAN_MAX_PRACH_ANT_NUM);

    tti = nSfIdx;
    for(cc_id = nRuCcidx; cc_id < psXranIoIf->num_cc_per_port[xran_port]; cc_id++) {
        for(ant_id = nAntStart; ant_id < (nAntStart + nAntNum) && ant_id <  xran_max_antenna_nr_prach; ant_id++) {
            flowId = xran_max_antenna_nr_prach * cc_id + ant_id;

            for(sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
                if(((1 << sym_id) & nSymMask)) {
                    if ((status = app_io_xran_iq_content_get_up_prach(p_o_xu_cfg->appMode, pXranConf,
                                                    psXranIoIf, psIoCtrl, p_iq,
                                                    cc_id, ant_id, sym_id, tti, flowId)) != 0) {
                        rte_panic("app_io_xran_iq_content_get_up_prach");
                    }
                }
            }

        }
    }

    return SUCCESS;
}

int32_t
app_io_xran_srs_decomp_func(uint16_t nCellIdx, uint32_t nSfIdx, uint32_t nSymMask,
                          uint32_t nAntStart, uint32_t nAntNum, uint32_t nSymStart, uint32_t nSymNum)
{
    xran_status_t status;
    struct bbu_xran_io_if *psXranIoIf  = app_io_xran_if_get();
    int32_t xran_port = 0;
    uint32_t nRuCcidx = 0;
    struct o_xu_buffers    * p_iq     = NULL;
    RuntimeConfig *p_o_xu_cfg = NULL;
    int32_t flowId = 0;
    struct xran_fh_config  *pXranConf = NULL;
    int32_t cc_id, ant_id, sym_id, tti;
    struct xran_io_shared_ctrl *psIoCtrl = NULL;
    uint32_t xran_max_antenna_nr;
    uint32_t xran_max_ant_array_elm_nr;

    xran_port = app_io_xran_map_cellid_to_port(psXranIoIf, nCellIdx, &nRuCcidx);

    if(xran_port < 0) {
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

    if(p_o_xu_cfg->appMode == APP_O_DU && p_o_xu_cfg->enableSrs){
        if(p_o_xu_cfg->p_buff) {
            p_iq = p_o_xu_cfg->p_buff;
        } else {
            rte_panic("Error p_o_xu_cfg->p_buff\n");
        }

        pXranConf = &app_io_xran_fh_config[xran_port];

        xran_max_antenna_nr = RTE_MAX(p_o_xu_cfg->numAxc, p_o_xu_cfg->numUlAxc);
        xran_max_ant_array_elm_nr = RTE_MAX(p_o_xu_cfg->antElmTRx, xran_max_antenna_nr);

        tti = nSfIdx;
        for(cc_id = nRuCcidx; cc_id < psXranIoIf->num_cc_per_port[xran_port]; cc_id++) {
            for(ant_id = nAntStart; ant_id < (nAntStart + nAntNum) && ant_id < xran_max_ant_array_elm_nr; ant_id++) {
                flowId = pXranConf->nAntElmTRx*cc_id + ant_id;
                for(sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
                    if(((1 << sym_id) & nSymMask)) {
                        if ((status = app_io_xran_iq_content_get_up_srs(p_o_xu_cfg->appMode, pXranConf,
                                                        psXranIoIf, psIoCtrl, p_iq,
                                                        cc_id, ant_id, sym_id, tti, flowId)) != 0) {
                            rte_panic("app_io_xran_iq_content_get_up_srs");
                        }
                    }
                }
            }
        }
    }
    return SUCCESS;
}


int32_t
app_bbu_pool_task_symX_wakeup(void *pCookies, uint32_t nSym)
{
    EventCtrlStruct *pEventCtrl = (EventCtrlStruct *)pCookies;
    uint16_t nCellIdx = pEventCtrl->nCellIdx;
    uint32_t nSfIdx   = pEventCtrl->nSlotIdx;/*get_ul_sf_idx(pEventCtrl->nSlotIdx, nCellIdx);*/

    uint32_t nSymbMask  = 0;
    uint32_t nSymStart  = 0;
    // uint32_t nSymNum    = 0;

    uint32_t Nrx_antennas;
    uint16_t nOranCellIdx;

    struct bbu_xran_io_if *psXranIoIf  = app_io_xran_if_get();

    int32_t xran_port = 0;
    uint32_t nRuCcidx = 0;
    struct xran_fh_config  *pXranConf = NULL;

    nOranCellIdx = nCellIdx;
    xran_port =  app_io_xran_map_cellid_to_port(psXranIoIf, nOranCellIdx, &nRuCcidx);
    if(xran_port < 0) {
        printf("incorrect xran_port\n");
        return FAILURE;
    }

    pXranConf = &app_io_xran_fh_config[xran_port];
    Nrx_antennas = pXranConf->neAxcUl;

    if(Nrx_antennas == 0)
        rte_panic("[p %d cell %d] Nrx_antennas == 0\n", xran_port, nCellIdx);

    nSymStart = 0;
    // nSymNum   = XRAN_NUM_OF_SYMBOL_PER_SLOT;

    switch(nSym)
    {
        case 2:     /* [0,1,2] */
            nSymbMask = 0x7;
            break;
        case 6:     /* [3,4,5,6] */
            nSymbMask = 0x78;
            break;
        case 11:   /* [7,8,9,10,11] */
            nSymbMask = 0xF80;
            break;
        case 13:   /* [12,13] */
            nSymbMask = 0x3000;
            break;
        default:
            rte_panic("nSym %d\n", nSym);
    }


    if (nSym == 13) /* w/a to run copy to IQ buffer as single short */
    {
        nSymbMask = 0b11111111111111;
        app_io_xran_ul_decomp_func(nCellIdx, nSfIdx, nSymbMask, 0, Nrx_antennas, nSymStart, XRAN_NUM_OF_SYMBOL_PER_SLOT);
    }

    return EBBUPOOL_CORRECT;
}

int32_t
app_bbu_pool_task_sym2_wakeup(void *pCookies)
{
    int32_t ret = 0;
    // EventCtrlStruct *pEventCtrl = (EventCtrlStruct *)pCookies;
    // uint16_t nCellIdx = pEventCtrl->nCellIdx;
    uint64_t mlog_start = MLogTick();

    ret = app_bbu_pool_task_symX_wakeup(pCookies, 2);

    //unlock the next task
    next_event_unlock(pCookies);
    MLogTask(PID_GNB_SYM2_WAKEUP, mlog_start, MLogTick());

    return ret;
}

int32_t
app_bbu_pool_task_sym6_wakeup(void *pCookies)
{
    int32_t ret = 0;
    // EventCtrlStruct *pEventCtrl = (EventCtrlStruct *)pCookies;
    uint64_t mlog_start = MLogTick();

    ret = app_bbu_pool_task_symX_wakeup(pCookies, 6);

    //unlock the next task
    next_event_unlock(pCookies);
    MLogTask(PID_GNB_SYM6_WAKEUP, mlog_start, MLogTick());
    return ret;
}

int32_t
app_bbu_pool_task_sym11_wakeup(void *pCookies)
{
    int32_t ret = 0;
    // EventCtrlStruct *pEventCtrl = (EventCtrlStruct *)pCookies;
    uint64_t mlog_start = MLogTick();

    ret = app_bbu_pool_task_symX_wakeup(pCookies, 11);

    //unlock the next task
    next_event_unlock(pCookies);
    MLogTask(PID_GNB_SYM11_WAKEUP, mlog_start, MLogTick());
    return ret;
}

int32_t
app_bbu_pool_task_sym13_wakeup(void *pCookies)
{
    int32_t ret = 0;
    // EventCtrlStruct *pEventCtrl = (EventCtrlStruct *)pCookies;
    uint64_t mlog_start = MLogTick();

    ret = app_bbu_pool_task_symX_wakeup(pCookies, 13);

    //unlock the next task
    next_event_unlock(pCookies);
    MLogTask(PID_GNB_SYM13_WAKEUP, mlog_start, MLogTick());
    return ret;
}

int32_t
app_bbu_pool_task_prach_wakeup(void *pCookies)
{
    EventCtrlStruct *pEventCtrl = (EventCtrlStruct *)pCookies;
    uint16_t nCellIdx = pEventCtrl->nCellIdx;
    uint32_t nSfIdx = pEventCtrl->nSlotIdx;// get_ul_sf_idx(pEventCtrl->nSlotIdx, nCellIdx);

    uint32_t nSymbMask  = 0;
    uint32_t nSymStart  = 0;
    // uint32_t nSymNum    = 0;

    uint32_t Nrx_antennas;
    uint16_t nOranCellIdx;

    struct bbu_xran_io_if *psXranIoIf  = app_io_xran_if_get();

    int32_t xran_port = 0;
    uint32_t nRuCcidx = 0;
    struct xran_fh_config  *pXranConf = NULL;
    uint64_t mlog_start = MLogTick();
    nOranCellIdx = nCellIdx;
    xran_port =  app_io_xran_map_cellid_to_port(psXranIoIf, nOranCellIdx, &nRuCcidx);
    if(xran_port < 0) {
        printf("incorrect xran_port\n");
        return FAILURE;
    }

    pXranConf = &app_io_xran_fh_config[xran_port];
    Nrx_antennas = RTE_MIN(pXranConf->neAxcUl, XRAN_MAX_PRACH_ANT_NUM);

    if(Nrx_antennas == 0)
        rte_panic("Nrx_antennas == 0\n");

    nSymStart = 0;
    // nSymNum   = XRAN_NUM_OF_SYMBOL_PER_SLOT;
    nSymbMask = 0b11111111111111;

    app_io_xran_prach_decomp_func(nCellIdx, nSfIdx, nSymbMask, 0, Nrx_antennas, nSymStart, XRAN_NUM_OF_SYMBOL_PER_SLOT);

    //unlock the next task
    next_event_unlock(pCookies);
    MLogTask(PID_GNB_PRACH_WAKEUP, mlog_start, MLogTick());
    return EBBUPOOL_CORRECT;
}

int32_t
app_bbu_pool_task_srs_wakeup(void *pCookies)
{
    int32_t ret = 0;
    EventCtrlStruct *pEventCtrl = (EventCtrlStruct *)pCookies;
    uint16_t nCellIdx = pEventCtrl->nCellIdx;
    uint32_t nSfIdx = pEventCtrl->nSlotIdx;// get_ul_sf_idx(pEventCtrl->nSlotIdx, nCellIdx);

    uint32_t nSymbMask  = 0;
    uint32_t nSymStart  = 0;
    // uint32_t nSymNum    = 0;

    uint32_t Nrx_antennas;
    uint16_t nOranCellIdx;

    struct bbu_xran_io_if *psXranIoIf  = app_io_xran_if_get();

    int32_t xran_port = 0;
    uint32_t nRuCcidx = 0;
    struct xran_fh_config  *pXranConf = NULL;
    uint64_t mlog_start = MLogTick();
    nOranCellIdx = nCellIdx;
    xran_port =  app_io_xran_map_cellid_to_port(psXranIoIf, nOranCellIdx, &nRuCcidx);
    if(xran_port < 0) {
        printf("incorrect xran_port\n");
        return FAILURE;
    }

    pXranConf = &app_io_xran_fh_config[xran_port];
    Nrx_antennas = pXranConf->nAntElmTRx;

    nSymStart = 0;
    // nSymNum   = XRAN_NUM_OF_SYMBOL_PER_SLOT;
    nSymbMask = 0b11111111111111;

    ret = app_io_xran_srs_decomp_func(nCellIdx, nSfIdx, nSymbMask, 0, Nrx_antennas, nSymStart, XRAN_NUM_OF_SYMBOL_PER_SLOT);
    //unlock the next task
    next_event_unlock(pCookies);
    MLogTask(PID_GNB_SRS_WAKEUP, mlog_start, MLogTick());
    return ret;
}
