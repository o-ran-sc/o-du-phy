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
 * @brief Header file to implementation of BBU
 * @file app_bbu.h
 * @ingroup xran
 * @author Intel Corporation
 *
 **/

#ifndef _APP_BBU_POOL_H_
#define _APP_BBU_POOL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ebbu_pool_api.h"
#include "ebbu_pool_cfg.h"

#include "config.h"
#include "xran_fh_o_du.h"
#include "xran_mlog_lnx.h"

#ifndef SUCCESS
/** SUCCESS = 0 */
#define SUCCESS     0
#endif /* #ifndef SUCCESS */

#ifndef FAILURE
/** FAILURE = 1 */
#define FAILURE     1
#endif /* #ifndef FAILURE */

#define MAX_NEXT_TASK_NUM  8
#define MAX_TEST_CTX       4
#define MAX_TEST_SPLIT_NUM 55 // then largest 1000 events per TTI by pre-defined event chain

#define  EVENT_NAME(EVENT_TYPE) #EVENT_TYPE

#define MAX_PHY_INSTANCES   ( 24 )
#define MAX_NUM_OF_SF_5G_CTX ( 8 )

/******Processing Latencies***/
#define DL_PROC_ADVANCE_MU0                 ( 1 )
#define DL_PROC_ADVANCE_MU1                 ( 2 )
#define DL_PROC_ADVANCE_MU3                 ( 2 )

#define UL_PROC_ADVANCE_MU0                 ( 1 )
#define UL_PROC_ADVANCE_MU1                 ( 1 )
#define UL_PROC_ADVANCE_MU3                 ( 1 )

extern uint32_t gMaxSlotNum[MAX_PHY_INSTANCES];
extern uint32_t gNumDLCtx[MAX_PHY_INSTANCES];
extern uint32_t gNumULCtx[MAX_PHY_INSTANCES];
extern uint32_t gNumDLBufferCtx[MAX_PHY_INSTANCES];
extern uint32_t gNumULBufferCtx[MAX_PHY_INSTANCES];
extern uint32_t gDLProcAdvance[MAX_PHY_INSTANCES];
extern int32_t gULProcAdvance[MAX_PHY_INSTANCES];

#define get_dl_sf_idx(nSlotNum, nCellIdx)     ((nSlotNum + gDLProcAdvance[nCellIdx]) % gMaxSlotNum[nCellIdx])
#define get_ul_sf_idx(nSlotNum, nCellIdx)     ((nSlotNum + gULProcAdvance[nCellIdx]) % gMaxSlotNum[nCellIdx])
#define get_dl_sf_ctx(nSlotNum, nCellIdx)     (nSlotNum % gNumDLCtx[nCellIdx])
#define get_ul_sf_ctx(nSlotNum, nCellIdx)     (nSlotNum % gNumULCtx[nCellIdx])

typedef enum
{
    TTI_START = 0,              /* 0 First task that will schedule all the other tasks for all Cells */
    SYM2_WAKE_UP,               /* 1 Sym2 Arrival which will wake up UL Tasks for all cells */
    SYM6_WAKE_UP,               /* 2 Sym6 Arrival which will wake up UL Tasks for all cells */
    SYM11_WAKE_UP,              /* 3 Sym11 Arrival which will wake up UL Tasks for all cells */
    SYM13_WAKE_UP,              /* 4 Sym13 Arrival which will wake up UL Tasks for all cells */
    PRACH_WAKE_UP,              /* 5 PRACH Arrival which will wake up will wake up PRACH for all cells */
    SRS_WAKE_UP,                /* 6 (Massive MIMO) SRS Arrival which will wake up SRS Decompression for all cells */
    DL_CONFIG,                  /* 7 */
    DL_PDSCH_TB,                /* 8 */
    DL_PDSCH_SCRM,              /* 9 */
    DL_PDSCH_SYM,               /* 10 */
    DL_PDSCH_RS,                /* 11 */
    DL_CTRL,                    /* 12 */
    UL_CONFIG,                  /* 13 */
    UL_IQ_DECOMP2,              /* 14 */
    UL_IQ_DECOMP6,              /* 15 */
    UL_IQ_DECOMP11,             /* 16 */
    UL_IQ_DECOMP13,             /* 17 */
    UL_PUSCH_CE0,               /* 18 */
    UL_PUSCH_CE7,               /* 19 */
    UL_PUSCH_EQL0,              /* 20 */
    UL_PUSCH_EQL7,              /* 21 */
    UL_PUSCH_LLR,               /* 22 */
    UL_PUSCH_DEC,               /* 23 */
    UL_PUSCH_TB,                /* 24 */
    UL_PUCCH,                   /* 25 */
    UL_PRACH,                   /* 26 */
    UL_SRS_DECOMP,              /* 27 */
    UL_SRS_CE,                  /* 28 */
    UL_SRS_POST,                /* 29 */
    DL_POST,                    /* 30 */
    UL_POST,                    /* 31 */
    DL_BEAM_GEN,                /* 32 */
    DL_BEAM_TX,                 /* 33 */
    UL_BEAM_GEN,                /* 34 */
    UL_BEAM_TX,                 /* 35 */
    MAX_TASK_NUM_G_NB           /* 36 */
} TaskTypeEnum;

///defines the parameters that multi-tasks are generated.
typedef struct
{
    /*! Indicate how many tasks of the generating type. 1 means that no task splitting. */
    uint16_t nTaskNum;
    /*! the parameter list for each splitted task */
    void *pTaskExePara[MAX_TEST_SPLIT_NUM];
} TaskPreGen;

typedef enum
{
    RB_SPLIT = 0,
    UE_GROUP_SPLIT = 1,
    LAYER_SPLIT = 2,
    UE_SPLIT = 3,
    PORT_SPLIT = 4,
    CE_RX_SPLIT = 5,
    OFDM_SYMB_SPLIT = 6
} TaskSplitType;

typedef struct tSampleSplitStruct
{
    int16_t nGroupStart;
    int16_t nGroupNum;
    int16_t nUeStart;
    int16_t nUeNum;
    int16_t nSymbStart;
    int16_t nSymbNum;
    int16_t nLayerStart;
    int16_t nLayerNum;
    int16_t nSplitIndex;
    TaskSplitType eSplitType;
} SampleSplitStruct;

typedef struct
{
    int32_t eventChainDepth;
    int32_t nextEventChain[MAX_TASK_NUM_G_NB][MAX_NEXT_TASK_NUM];
    int32_t nextEventCount[MAX_TASK_NUM_G_NB];
    int32_t preEventCount[MAX_TASK_NUM_G_NB];
    int32_t preEventCountSave[MAX_TASK_NUM_G_NB];
    int32_t preEventStat[MAX_TASK_NUM_G_NB];
} __attribute__((aligned(IA_ALIGN))) EventChainDescStruct;

typedef void (*PreEventExeFunc) (uint32_t nSfIdx, uint16_t nCellIdx, TaskPreGen *pPara);

typedef struct
{
    int32_t nEventId;
    char sTaskName[32];
    int32_t nEventPrio;
    EventExeFunc pEventFunc;
    PreEventExeFunc pPreEventFunc;
    uint32_t nWakeOnExtrernalEvent;
    uint32_t nPrefetchFlag;
    uint32_t nCoreMaskType;
    uint64_t nCoreMask0;
    uint64_t nCoreMask1;
    //uint64_t nCoreMask2;
    //uint64_t nCoreMask3;
    //uint64_t nCoreMask4;
    //uint64_t nCoreMask5;
    //uint64_t nCoreMask6;
    //uint64_t nCoreMask7;
} __attribute__((aligned(IA_ALIGN))) EventConfigStruct;

typedef struct
{
    int32_t nEventId;
    int32_t nSplitIdx;
    int32_t nCellIdx;
    int32_t nSlotIdx;
    void *pTaskPara;
    void *pHandler;
    float *dummy0;
    uint64_t tSendTime;
    uint8_t nBuffer[240];
} __attribute__((aligned(IA_ALIGN))) EventCtrlStruct;

typedef struct
{
    int32_t nEventId;
    int32_t nEventPrio;
    EventExeFunc pEventFunc;
} __attribute__((aligned(IA_ALIGN))) EventInfo;

typedef struct
{
    int32_t nCellInd;
    EventStruct *pEventStruct;
    int16_t *pCEtp;
    int16_t *pMIMOouttp;
    int16_t *pWeighttp;
} __attribute__((aligned(IA_ALIGN))) gNBCellStruct;

extern EventChainDescStruct gEventChain[EBBU_POOL_MAX_TEST_CELL][MAX_TEST_CTX];
extern EventCtrlStruct gEventCtrl[EBBU_POOL_MAX_TEST_CELL][MAX_TEST_CTX][MAX_TASK_NUM_G_NB][MAX_TEST_SPLIT_NUM];

int32_t event_chain_gen(EventChainDescStruct *psEventChain);
int32_t event_chain_reset(EventChainDescStruct *psEventChain);
int32_t test_buffer_create();

eBbuPoolHandler app_get_ebbu_pool_handler(void);

int32_t app_bbu_init(int argc, char *argv[], char cfgName[512], UsecaseConfig* p_use_cfg,  RuntimeConfig* p_o_xu_cfg[],
                    uint64_t nActiveCoreMask[EBBUPOOL_MAX_CORE_MASK]);
int32_t app_bbu_close(void);


int32_t app_bbu_dl_tti_call_back(void * param);

int32_t test_func_gen(eBbuPoolHandler pHandler, int32_t nCell, int32_t nSlot, int32_t eventId);
int32_t next_event_unlock(void *pCookies);

/** tasks */
int32_t app_bbu_pool_task_dl_post(void *pCookies);
void app_bbu_pool_pre_task_dl_post(uint32_t nSubframe, uint16_t nCellIdx, TaskPreGen *pPara);
int32_t app_bbu_pool_task_dl_config(void *pCookies);
void app_bbu_pool_pre_task_dl_cfg(uint32_t nSubframe, uint16_t nCellIdx, TaskPreGen *pPara);
int32_t app_bbu_pool_task_ul_config(void * pCookies);
void app_bbu_pool_pre_task_ul_cfg(uint32_t nSubframe, uint16_t nCellIdx, TaskPreGen *pPara);

int32_t app_bbu_pool_task_sym2_wakeup(void *pCookies);
int32_t app_bbu_pool_task_sym6_wakeup(void *pCookies);
int32_t app_bbu_pool_task_sym11_wakeup(void *pCookies);
int32_t app_bbu_pool_task_sym13_wakeup(void *pCookies);
int32_t app_bbu_pool_task_prach_wakeup(void *pCookies);
int32_t app_bbu_pool_task_srs_wakeup(void *pCookies);

void app_io_xran_fh_bbu_rx_callback(void *pCallbackTag, xran_status_t status);
void app_io_xran_fh_bbu_rx_bfw_callback(void *pCallbackTag, xran_status_t status);
void app_io_xran_fh_bbu_rx_prach_callback(void *pCallbackTag, xran_status_t status);
void app_io_xran_fh_bbu_rx_srs_callback(void *pCallbackTag, xran_status_t status);


#ifdef __cplusplus
}
#endif
#endif /*_APP_BBU_POOL_H_*/