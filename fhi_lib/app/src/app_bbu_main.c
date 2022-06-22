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
 * @file app_bbu_main.c
 * @ingroup xran
 * @author Intel Corporation
 *
 **/

#define _GNU_SOURCE
#include <sched.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>


#include "app_bbu_pool.h"
#include "app_io_fh_xran.h"
#include "xran_mlog_task_id.h"

/**
  * @file   app_bbu_main.c
  * @brief  example pipeline code to use Enhanced BBUPool Framework
*/

uint32_t gRunCount = 0;
volatile uint32_t nStopFlag = 1;
int32_t gQueueCtxNum = 1;

int32_t nSplitNumCell[EBBU_POOL_MAX_TEST_CELL];

int32_t nTestCell = 0;
int32_t nTestCore = 0;
volatile uint64_t ttistart = 0;
int32_t dl_ul_count, dl_count, ul_count;

uint32_t gMaxSlotNum[MAX_PHY_INSTANCES];
uint32_t gNumDLCtx[MAX_PHY_INSTANCES];
uint32_t gNumULCtx[MAX_PHY_INSTANCES];
uint32_t gNumDLBufferCtx[MAX_PHY_INSTANCES];
uint32_t gNumULBufferCtx[MAX_PHY_INSTANCES];
uint32_t gDLProcAdvance[MAX_PHY_INSTANCES];
int32_t gULProcAdvance[MAX_PHY_INSTANCES];


#define EX_FUNC_NUM 9
int32_t exGenDlFunc[EX_FUNC_NUM] =
{
    DL_CONFIG,
    DL_PDSCH_SCRM,
};
int32_t exGenUlFunc[EX_FUNC_NUM] =
{
    UL_CONFIG,
    UL_IQ_DECOMP2,
    UL_IQ_DECOMP6,
    UL_IQ_DECOMP11,
    UL_IQ_DECOMP13,
    UL_PRACH,
    UL_PUSCH_TB
};

int32_t exGenAllFunc[EX_FUNC_NUM] =
{
    DL_CONFIG,
    UL_CONFIG,
    DL_PDSCH_SCRM,
    UL_IQ_DECOMP2,
    UL_IQ_DECOMP6,
    UL_IQ_DECOMP11,
    UL_IQ_DECOMP13,
    UL_PRACH,
    UL_PUSCH_TB
};

peBbuPoolCfgVarsStruct pCfg;
pthread_t tCtrlThread;
eBbuPoolHandler pHandler = NULL;
clock_t tStart, tEnd;
uint32_t ttiCell[EBBU_POOL_MAX_TEST_CELL] = {0};
uint64_t ttiCountCell[EBBU_POOL_MAX_TEST_CELL] = {0UL};
uint32_t frameFormatCell[EBBU_POOL_MAX_TEST_CELL] = {0};
uint64_t tTTI = 0;


eBbuPoolHandler app_get_ebbu_pool_handler(void)
{
    return pHandler;
}


int32_t
app_bbu_dl_tti_call_back(void * param)
{
    int32_t ret = 0;
    uint64_t t1 = MLogTick();
    int32_t iCell = 0;
    int32_t nCtx[EBBU_POOL_MAX_TEST_CELL] = {0}, iExFunc = 0, eventId = 0;
    int32_t *exGenFuncDl = NULL, *exGenFuncUl = NULL;
    int32_t nFuncNumDl = 0, nFuncNumUl = 0;
    uint8_t Numerlogy = app_io_xran_fh_config[0].frame_conf.nNumerology;
    uint8_t nNrOfSlotInSf = 1<<Numerlogy;
    int32_t sfIdx = app_io_xran_sfidx_get(nNrOfSlotInSf);
    uint64_t t = MLogTick();
    uint32_t mlogVars[10], mlogVarsCnt = 0;

    exGenFuncDl = exGenDlFunc;
    nFuncNumDl = 2;
    exGenFuncUl = exGenUlFunc;
    nFuncNumUl = 7;

    mlogVars[mlogVarsCnt++] = 0x99999999;
    mlogVars[mlogVarsCnt++] = sfIdx % gMaxSlotNum[0];
    MLogAddVariables(mlogVarsCnt, mlogVars, t);

    for(iCell = 0; iCell < nTestCell ; iCell ++)
    {
        //check is it a tti for this cell
        if(sfIdx % ttiCell[iCell] == 0)
        {
            ttistart = MLogTick();
            dl_ul_count = nTestCell;
            dl_ul_count += nTestCell;
            ttiCountCell[iCell] = sfIdx;
            nCtx[iCell] = ttiCountCell[iCell] % MAX_TEST_CTX;

            event_chain_reset(&gEventChain[iCell][nCtx[iCell]]);

            //if(nD2USwitch[frameFormatCell[iCell]][ttiCountCell[iCell]%EBBU_POOL_TDD_PERIOD] & EBBU_POOL_TEST_DL)
            {
                //printf("\ncell %d dl", iCell);
                for(iExFunc = 0; iExFunc < nFuncNumDl; iExFunc ++)
                {
                    eventId = exGenFuncDl[iExFunc];
                    ret = test_func_gen(pHandler, iCell, ttiCountCell[iCell], eventId);
                }
            }

            //if(nD2USwitch[frameFormatCell[iCell]][ttiCountCell[iCell]%EBBU_POOL_TDD_PERIOD] & EBBU_POOL_TEST_UL)
            {
                //printf("\ncell %d ul", iCell);
                for(iExFunc = 0; iExFunc < nFuncNumUl; iExFunc ++)
                {
                    eventId = exGenFuncUl[iExFunc];
                    ret = test_func_gen(pHandler, iCell, ttiCountCell[iCell], eventId);
                }
            }
        }

        if(EBBUPOOL_ERROR == ret)
        {
            printf("\nFail to send cell %d, sf %lu, tsc %llu", iCell, ttiCountCell[iCell], ebbu_pool_tick()/tTTI/2);
        }
    }
    MLogTask(PID_GNB_PROC_TIMING, t1, MLogTick());
    return 0;
}

int32_t app_bbu_init(int argc, char *argv[], char cfgName[512], UsecaseConfig* p_use_cfg,  RuntimeConfig* p_o_xu_cfg[], uint64_t nActiveCoreMask[EBBUPOOL_MAX_CORE_MASK])
{
    tStart = clock();
    uint32_t nFh_cell = 0;
    int32_t i;

    ebbu_pool_cfg_set_cfg_filename(argc, argv, cfgName);

    ebbu_pool_cfg_init_from_xml();
    pCfg = ebbu_pool_cfg_get_ctx();

    nTestCell = pCfg->testCellNum;

    for (i = 0; i < p_use_cfg->oXuNum; i++)
        nFh_cell += p_o_xu_cfg[i]->numCC;

    if (nFh_cell != nTestCell) {
        rte_panic("WARNING: miss match between BBU Cells (%d) and O-RAN FH Cells (%d)", nTestCell, nFh_cell);
    }

    if(nTestCell > EBBU_POOL_MAX_TEST_CELL || nTestCell <= 0)
    {
        printf("Wrong cell num %d\n",nTestCell);
        nTestCell = 0;
    }
    nTestCore = pCfg->testCoreNum;
    if(nTestCore > EBBU_POOL_MAX_TEST_CORE || nTestCore <= 0)
    {
        printf("Wrong core num %d\n",nTestCore);
        nTestCore = 0;
    }

    gRunCount = 0;

    printf("Test cell %d, test total core num %d, test slots %d\n", nTestCell, nTestCore, gRunCount);

    // Step 1: create Framework handler
    ebbu_pool_create_handler(&pHandler, 1, pCfg->mainThreadCoreId);
    if(NULL == pHandler)
    {
        printf("\nFail to init Framework!");
        exit(-1);
    }

    // Step 2: create priority queues
    // Assume 3 queues
    uint32_t nPrioQueue = pCfg->queueNum;
    uint32_t nQueueSize = pCfg->queueDepth;
    uint32_t pPrioQueueSize[8] = {nQueueSize, nQueueSize, nQueueSize, nQueueSize,nQueueSize, nQueueSize, nQueueSize, nQueueSize};
    QueueConfigStruct sQueueConfig;
    sQueueConfig.pQueueDepth = pPrioQueueSize;
    sQueueConfig.nPriQueueNum = nPrioQueue;
    sQueueConfig.nPriQueueCtxNum = pCfg->ququeCtxNum;
    sQueueConfig.nPriQueueCtxMaxFetch = pCfg->ququeCtxNum;

    ebbu_pool_create_queues(pHandler, sQueueConfig);

    ebbu_pool_queue_ctx_set_threshold(pHandler, 2*nTestCell);

    // Step 3: create one report for Framework
    ReportCfg sReport;
    sReport.nEventHoldNum = 2000;
    sReport.pHandler = (void *)pHandler;
    ebbu_pool_create_report(sReport);

    // Step 4: start control thread
   // pthread_create(&tCtrlThread, NULL, controlThread, (void *)pHandler);

    peBbuPoolCfgVarsStruct pCfg = ebbu_pool_cfg_get_ctx();
    int32_t iCell = 0;
    int32_t iCtx = 0;
    uint32_t xran_timer = (1000/(1 << p_o_xu_cfg[0]->mu_number)); /* base on O-RU 0 */

    printf("xran_timer for TTI [%d] us\n", xran_timer);
    // Init base timing as per xran_timer
    uint64_t tStart = ebbu_pool_tick();
    usleep(xran_timer);
    tTTI = ebbu_pool_tick() - tStart;

    // cell specific TTI init
    // create cell and create consumer thread
    // set events num per cell
    for(iCell = 0; iCell < nTestCell; iCell ++)
    {
        ttiCell[iCell] = pCfg->sTestCell[iCell].tti/xran_timer;
        printf("\nttiCell[%d] %d", iCell, ttiCell[iCell]);

        frameFormatCell[iCell] = pCfg->sTestCell[iCell].frameFormat;
        if(frameFormatCell[iCell] >= EBBU_POOL_MAX_FRAME_FORMAT)
            frameFormatCell[iCell] = 0;

        for(iCtx = 0; iCtx < MAX_TEST_CTX; iCtx ++)
        {
            event_chain_gen(&gEventChain[iCell][iCtx]);
        }
        nSplitNumCell[iCell] = 1;//((pCfg->sTestCell[iCell].eventPerTti)-11)/18; //current event chain has 29 events, 19 of them can be split
        printf("\nnSplitNumCell[%d] %d", iCell, nSplitNumCell[iCell]);
        if(nSplitNumCell[iCell] > MAX_TEST_SPLIT_NUM)
            nSplitNumCell[iCell] = MAX_TEST_SPLIT_NUM;

    }
    test_buffer_create();

    // add consumer thread

    /*int32_t corePool[EBBU_POOL_MAX_TEST_CORE] = {4,24,5,25,6,26,7,27,
                                  8,28,9,29,10,30,11,31,
                                  12,32,13,33,14,34,15,35,
                                  16,36,17,37,18,38,19,39,
                                  2,22,3,23};
    */

    int32_t iCore = 0;
    uint64_t nMask0 = 0UL, nMask1 = 0UL;
    uint32_t iCoreIdx = 0;
    uint64_t nCoreMask[EBBUPOOL_MAX_CORE_MASK];

    for(; iCore < nTestCore; iCore ++)
    {
        iCoreIdx = pCfg->testCoreList[iCore];
        if(iCoreIdx < 64)
            nMask0 |= 1UL << iCoreIdx;
        else
            nMask1 |= 1UL << iCoreIdx;

    }
    //printf("\nnStartMask %016lx\n", nStartMask);

    nActiveCoreMask[0] = nCoreMask[0] = nMask0;
    nActiveCoreMask[0] = nCoreMask[1] = nMask1;
    nActiveCoreMask[0] = nCoreMask[2] = 0;
    nActiveCoreMask[0] = nCoreMask[3] = 0;

    ebbu_pool_consumer_set_thread_params(pHandler, 55, SCHED_FIFO, pCfg->sleepFlag);
    ebbu_pool_consumer_set_thread_mask(pHandler, nCoreMask);

    usleep(100000);

    /* mMIMO with 64TRX */
    for(iCell = 0; iCell < nTestCell; iCell ++) {

        gDLProcAdvance[iCell] = DL_PROC_ADVANCE_MU1;
        gULProcAdvance[iCell] = UL_PROC_ADVANCE_MU1;
        gNumDLCtx[iCell] = 5;
        gNumDLBufferCtx[iCell] = 3;
        gNumULCtx[iCell] = 8;
        gNumULBufferCtx[iCell] = 2;
        gMaxSlotNum[iCell] = 10240 * (1 << p_o_xu_cfg[0]->mu_number);
    }

    return 0;
}

int32_t app_bbu_close(void)
{
    // Close Mlog Buffer and write to File
   // if(pCfg->mlogEnable)
     //   MLogPrint(NULL);

    // Step 5: release all the threads
    ebbu_pool_release_threads(pHandler);

    // Step 6: get report for Framework
    int32_t testResult = ebbu_pool_status_report(pHandler);
    // Save the test status, 0 means pass, other means fail
    char resultString[8] = {'\0'};
    if(testResult == 0)
        sprintf(resultString, "PASS");
    else
        sprintf(resultString, "FAIL");

    tEnd = clock();

    char fResultName[64] = {"sample-app_bbu_pool_test_results.txt"};
    FILE *pFile = fopen(fResultName, "a");
    if(pFile != NULL)
    {
        fprintf(pFile, "sample-app test case\n");
        fprintf(pFile, "Execution time: %.3f second\n", (double)(tEnd - tStart)/1000000);
        fprintf(pFile, "Result: %s\n\n", resultString);
        fclose(pFile);
    }

    // Step 7: release report for Framework
    ebbu_pool_release_report(pHandler);

    // Step 8: release all allocated queues
    ebbu_pool_release_queues(pHandler);

    // Step 9: release handler for Framework
    ebbu_pool_release_handler(&pHandler);
    printf("\n");

    return 0;
}

void
app_io_xran_fh_bbu_rx_callback(void *pCallbackTag, xran_status_t status)
{
    eBbuPoolHandler pHandler = app_get_ebbu_pool_handler();
    uint64_t t1 = MLogTick();
    uint32_t mlogVar[10];
    uint32_t mlogVarCnt = 0;
    int32_t nCellIdx;
    int32_t nCcIdx;
    int32_t sym, nSlotIdx, ntti;
    uint64_t mlog_start;
    struct xran_cb_tag *pTag = (struct xran_cb_tag *) pCallbackTag;
    int32_t o_xu_id = pTag->oXuId;
    struct xran_io_shared_ctrl *psIoCtrl = app_io_xran_if_ctrl_get(o_xu_id);
    struct bbu_xran_io_if *psXranIoIf  = app_io_xran_if_get();
    struct xran_fh_config  *pXranConf = &app_io_xran_fh_config[o_xu_id];
    uint32_t xran_max_antenna_nr = RTE_MAX(pXranConf->neAxc, pXranConf->neAxcUl);
    uint32_t ant_id, sym_id, idxElm;
    struct xran_prb_map *pRbMap = NULL;
    struct xran_prb_elm *pRbElm = NULL;

    mlog_start = MLogTick();
    nCcIdx   = pTag->cellId;
    nCellIdx = psXranIoIf->map_cell_id2port[o_xu_id][nCcIdx];
    nSlotIdx = pTag->slotiId; ///((status >> 16) & 0xFFFF);  /** TTI aka slotIdx */
    sym      = pTag->symbol & 0xFF; /* sym */
    ntti = (nSlotIdx + XRAN_N_FE_BUF_LEN-1) % XRAN_N_FE_BUF_LEN;

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

    if(psIoCtrl == NULL)
    {
        printf("psIoCtrl NULL! o_xu_id= %d\n", o_xu_id);
        return;
    }

    if (sym == XRAN_ONE_FOURTHS_CB_SYM) {
        // 1/4 of slot
        test_func_gen(pHandler, nCellIdx, nSlotIdx, SYM2_WAKE_UP);
    } else if (sym == XRAN_HALF_CB_SYM) {
        // First Half
        test_func_gen(pHandler, nCellIdx, nSlotIdx, SYM6_WAKE_UP);
    } else if (sym == XRAN_THREE_FOURTHS_CB_SYM) {
        // 2/4 of slot
        test_func_gen(pHandler, nCellIdx, nSlotIdx, SYM11_WAKE_UP);
    } else if (sym == XRAN_FULL_CB_SYM) {
        // Second Half
        test_func_gen(pHandler, nCellIdx, nSlotIdx, SYM13_WAKE_UP);
    } else {
        /* error */
        MLogTask(PID_GNB_SYM_CB, t1, MLogTick());

        rte_panic("app_io_xran_fh_bbu_rx_callback: sym\n");
        return;
    }

    if(sym == XRAN_FULL_CB_SYM)  //full slot callback only
    {
        for(ant_id = 0; ant_id < xran_max_antenna_nr; ant_id++) {
            pRbMap = (struct xran_prb_map *) psIoCtrl->sFrontHaulRxPrbMapBbuIoBufCtrl[ntti][nCcIdx][ant_id].sBufferList.pBuffers->pData;
            if(pRbMap == NULL){
                printf("(%d:%d:%d)pRbMap == NULL\n", nCcIdx, ntti, ant_id);
                exit(-1);
            }
            for(sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
                for(idxElm = 0; idxElm < pRbMap->nPrbElm; idxElm++ ) {
                    pRbElm = &pRbMap->prbMap[idxElm];
                    pRbElm->nSecDesc[sym_id] = 0;
                }
            }
        }
    }

    rte_pause();

    MLogTask(PCID_GNB_FH_RX_DATA_CC0+nCellIdx, mlog_start, MLogTick());
    return;
}

void
app_io_xran_fh_bbu_rx_prach_callback(void *pCallbackTag, xran_status_t status)
{
    eBbuPoolHandler pHandler = app_get_ebbu_pool_handler();
    int32_t nCellIdx, nCcIdx;
    int32_t sym, nSlotIdx, ntti;
    struct xran_cb_tag *pTag = (struct xran_cb_tag *) pCallbackTag;
    int32_t o_xu_id = pTag->oXuId;
    uint64_t mlog_start = MLogTick();
    uint32_t mlogVar[10];
    struct bbu_xran_io_if *psXranIoIf  = app_io_xran_if_get();
    struct xran_io_shared_ctrl *psIoCtrl = app_io_xran_if_ctrl_get(o_xu_id);
    uint32_t mlogVarCnt = 0;

    if(psIoCtrl == NULL || psXranIoIf == NULL)
    {
        printf("psIoCtrl NULL! o_xu_id= %d\n", o_xu_id);
        return;
    }

    nCcIdx   = pTag->cellId;
    nCellIdx = psXranIoIf->map_cell_id2port[o_xu_id][nCcIdx];
    nSlotIdx = pTag->slotiId; ///((status >> 16) & 0xFFFF);  /** TTI aka slotIdx */
    sym      = pTag->symbol & 0xFF; /* sym */
    ntti = (nSlotIdx + XRAN_N_FE_BUF_LEN-1) % XRAN_N_FE_BUF_LEN;

    mlogVar[mlogVarCnt++] = 0xDDDDDDDD;
    mlogVar[mlogVarCnt++] = o_xu_id;
    mlogVar[mlogVarCnt++] = nCellIdx;
    mlogVar[mlogVarCnt++] = sym;
    mlogVar[mlogVarCnt++] = nSlotIdx;
    mlogVar[mlogVarCnt++] = ntti;
    MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());
    test_func_gen(pHandler, nCellIdx, nSlotIdx, PRACH_WAKE_UP);

    MLogTask(PCID_GNB_FH_RX_PRACH_CC0+nCellIdx, mlog_start, MLogTick());
}

void
app_io_xran_fh_bbu_rx_srs_callback(void *pCallbackTag, xran_status_t status)
{
    eBbuPoolHandler pHandler = app_get_ebbu_pool_handler();
    int32_t nCellIdx;
    int32_t nCcIdx;
    int32_t sym, nSlotIdx, ntti;
    struct xran_cb_tag *pTag = (struct xran_cb_tag *) pCallbackTag;
    int32_t o_xu_id = pTag->oXuId;
    struct xran_io_shared_ctrl *psIoCtrl = app_io_xran_if_ctrl_get(o_xu_id);
    struct bbu_xran_io_if *psXranIoIf  = app_io_xran_if_get();
    struct xran_fh_config  *pXranConf = &app_io_xran_fh_config[o_xu_id];
    uint32_t xran_max_antenna_nr = RTE_MAX(pXranConf->neAxc, pXranConf->neAxcUl);
    uint32_t xran_max_ant_array_elm_nr = RTE_MAX(pXranConf->nAntElmTRx, xran_max_antenna_nr);
    uint32_t ant_id, sym_id, idxElm;
    struct xran_prb_map *pRbMap = NULL;
    struct xran_prb_elm *pRbElm = NULL;
    uint64_t mlog_start = MLogTick();
    uint32_t mlogVar[10];
    uint32_t mlogVarCnt = 0;

    if(psIoCtrl == NULL || psXranIoIf == NULL)
    {
        printf("psIoCtrl NULL! o_xu_id= %d\n", o_xu_id);
        return;
    }
    nCcIdx   = pTag->cellId;
    nCellIdx = psXranIoIf->map_cell_id2port[o_xu_id][nCcIdx];
    nSlotIdx = pTag->slotiId; ///((status >> 16) & 0xFFFF);  /** TTI aka slotIdx */
    sym      = pTag->symbol & 0xFF; /* sym */
    ntti = (nSlotIdx + XRAN_N_FE_BUF_LEN-1) % XRAN_N_FE_BUF_LEN;

    mlogVar[mlogVarCnt++] = 0xCCCCCCCC;
    mlogVar[mlogVarCnt++] = o_xu_id;
    mlogVar[mlogVarCnt++] = nCellIdx;
    mlogVar[mlogVarCnt++] = sym;
    mlogVar[mlogVarCnt++] = nSlotIdx;
    mlogVar[mlogVarCnt++] = ntti;
    MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());
    test_func_gen(pHandler, nCellIdx, nSlotIdx, SRS_WAKE_UP);

    if(sym == XRAN_FULL_CB_SYM)  //full slot callback only
    {
        for(ant_id = 0; ant_id < xran_max_ant_array_elm_nr; ant_id++) {
            pRbMap = (struct xran_prb_map *) psIoCtrl->sFHSrsRxPrbMapBbuIoBufCtrl[ntti][nCcIdx][ant_id].sBufferList.pBuffers->pData;
            if(pRbMap == NULL){
                printf("(%d:%d:%d)pRbMap == NULL\n", nCcIdx, ntti, ant_id);
                exit(-1);
            }
            for(sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
                for(idxElm = 0; idxElm < pRbMap->nPrbElm; idxElm++ ) {
                    pRbElm = &pRbMap->prbMap[idxElm];
                    pRbElm->nSecDesc[sym_id] = 0;
                }
            }
        }
    }
    MLogTask(PCID_GNB_FH_RX_SRS_CC0+nCellIdx, mlog_start, MLogTick());
}
