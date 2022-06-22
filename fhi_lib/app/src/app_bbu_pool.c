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

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <immintrin.h>

#include "app_bbu_pool.h"

/**
  * @file   gnb_main_ebbu_pool.c
  * @brief  example pipeline code to use Enhanced BBUPool Framework
*/

extern int32_t gQueueCtxNum;
extern int32_t nSplitNumCell[EBBU_POOL_MAX_TEST_CELL];

int32_t test_func_A(void *pCookies);
int32_t test_func_B(void *pCookies);
int32_t test_func_C(void *pCookies);

void test_pre_func_A(uint32_t nSubframe, uint16_t nCellIdx, TaskPreGen *pPara);
void test_pre_func_B(uint32_t nSubframe, uint16_t nCellIdx, TaskPreGen *pPara);

int32_t test_func_gen(eBbuPoolHandler pHandler, int32_t nCell, int32_t nSlot, int32_t eventId);

int32_t simulate_traffic(void *pCookies, int32_t testCount);

typedef enum
{
    CAT_A = 0, //directly execute
    CAT_B,     //highest priority
    CAT_C,     //first priority
    CAT_D,     //second priority
    CAT_E,     //third priority
    CAT_F,     //forth priority
    CAT_G,     //fifth priority
    CAT_H,     //sixth priority
    CAT_I,     //seventh priority
    CAT_NUM
} EventCatEnum;

static int32_t eventSendDic[CAT_NUM] =
{
    EBBUPOOL_PRIO_EXECUTE,
    EBBUPOOL_PRIO_HIGHEST,
    EBBUPOOL_PRIO_ONE,
    EBBUPOOL_PRIO_TWO,
    EBBUPOOL_PRIO_THREE,
    EBBUPOOL_PRIO_FOUR,
    EBBUPOOL_PRIO_FIVE,
    EBBUPOOL_PRIO_SIX,
    EBBUPOOL_PRIO_SEVEN
};

EventConfigStruct testEventTable[MAX_TASK_NUM_G_NB] =
{
    /* Event ID*/        /* Event Name*/           /* pri */   /* event function */  /* pre event function */   /* nExtEvent */  /*prefetch flag */  /*core mask type */   /* core affinity 0~63 */ /* core affinity 64~127 */
    { TTI_START,        EVENT_NAME(TTI_START),       CAT_B,     test_func_A,                  NULL,                   0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { SYM2_WAKE_UP,     EVENT_NAME(SYM2_WAKE_UP),    CAT_B,  app_bbu_pool_task_sym2_wakeup,   NULL,                   0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { SYM6_WAKE_UP,     EVENT_NAME(SYM6_WAKE_UP),    CAT_B,  app_bbu_pool_task_sym6_wakeup,   NULL,                   0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { SYM11_WAKE_UP,    EVENT_NAME(SYM11_WAKE_UP),   CAT_B,  app_bbu_pool_task_sym11_wakeup,  NULL,                   0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { SYM13_WAKE_UP,    EVENT_NAME(SYM13_WAKE_UP),   CAT_B,  app_bbu_pool_task_sym13_wakeup,  NULL,                   0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { PRACH_WAKE_UP,    EVENT_NAME(PRACH_WAKE_UP),   CAT_B,  app_bbu_pool_task_prach_wakeup,  NULL,                   0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { SRS_WAKE_UP,      EVENT_NAME(SRS_WAKE_UP),     CAT_B,  app_bbu_pool_task_srs_wakeup,    NULL,                   0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { DL_CONFIG,        EVENT_NAME(DL_CONFIG),       CAT_B, app_bbu_pool_task_dl_config,app_bbu_pool_pre_task_dl_cfg, 0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { DL_PDSCH_TB,      EVENT_NAME(DL_PDSCH_TB),     CAT_B,     test_func_A,             test_pre_func_B,             0,           0,                        0,            0x000000ffffffffff,          0xfffffffffffffff},
    { DL_PDSCH_SCRM,    EVENT_NAME(DL_PDSCH_SCRM),   CAT_C,     test_func_A,                  NULL,                   0,           0,                        0,            0x000000ffffffffff,          0xfffffffffffffff},
    { DL_PDSCH_SYM,     EVENT_NAME(DL_PDSCH_SYM),    CAT_C,     test_func_B,             test_pre_func_A,             0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { DL_PDSCH_RS,      EVENT_NAME(DL_PDSCH_RS),     CAT_C,     test_func_A,             test_pre_func_B,             0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { DL_CTRL,          EVENT_NAME(DL_CTRL),         CAT_C,     test_func_A,             test_pre_func_B,             0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { UL_CONFIG,        EVENT_NAME(UL_CONFIG),       CAT_C, app_bbu_pool_task_ul_config,app_bbu_pool_pre_task_ul_cfg, 0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { UL_IQ_DECOMP2,    EVENT_NAME(UL_IQ_DECOMP2),   CAT_D,     test_func_A,                  NULL,                   0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { UL_IQ_DECOMP6,    EVENT_NAME(UL_IQ_DECOMP6),   CAT_D,     test_func_A,                  NULL,                   0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { UL_IQ_DECOMP11,   EVENT_NAME(UL_IQ_DECOMP11),  CAT_D,     test_func_A,                  NULL,                   0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { UL_IQ_DECOMP13,   EVENT_NAME(UL_IQ_DECOMP13),  CAT_D,     test_func_A,                  NULL,                   0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { UL_PUSCH_CE0,     EVENT_NAME(UL_PUSCH_CE0),    CAT_D,     test_func_B,             test_pre_func_A,             0,           0,                        0,            0x000000ffffffffff,          0xfffffffffffffff},
    { UL_PUSCH_CE7,     EVENT_NAME(UL_PUSCH_CE7),    CAT_D,     test_func_B,             test_pre_func_A,             0,           0,                        0,            0x000000ffffffffff,          0xfffffffffffffff},
    { UL_PUSCH_EQL0,    EVENT_NAME(UL_PUSCH_EQL0),   CAT_D,     test_func_B,             test_pre_func_A,             0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { UL_PUSCH_EQL7,    EVENT_NAME(UL_PUSCH_EQL7),   CAT_D,     test_func_B,             test_pre_func_A,             0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { UL_PUSCH_LLR,     EVENT_NAME(UL_PUSCH_LLR),    CAT_C,     test_func_A,             test_pre_func_B,             0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { UL_PUSCH_DEC,     EVENT_NAME(UL_PUSCH_DEC),    CAT_C,     test_func_A,             test_pre_func_B,             0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { UL_PUSCH_TB,      EVENT_NAME(UL_PUSCH_TB),     CAT_C,     test_func_A,                  NULL,                   0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { UL_PUCCH,         EVENT_NAME(UL_PUCCH),        CAT_E,     test_func_A,             test_pre_func_A,             0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { UL_PRACH,         EVENT_NAME(UL_PRACH),        CAT_E,     test_func_A,                  NULL,                   0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { UL_SRS_DECOMP,    EVENT_NAME(UL_SRS_DECOMP),   CAT_E,     test_func_A,             test_pre_func_B,             0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { UL_SRS_CE,        EVENT_NAME(UL_SRS_CE),       CAT_E,     test_func_B,             test_pre_func_B,             0,           0,                        0,            0x000000ffffffffff,          0xfffffffffffffff},
    { UL_SRS_POST,      EVENT_NAME(UL_SRS_POST),     CAT_E,     test_func_A,             test_pre_func_B,             0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { DL_POST,          EVENT_NAME(DL_POST),         CAT_B, app_bbu_pool_task_dl_post, app_bbu_pool_pre_task_dl_post, 0,           1,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { UL_POST,          EVENT_NAME(UL_POST),         CAT_A,     test_func_C,                  NULL,                   0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { DL_BEAM_GEN,      EVENT_NAME(DL_BEAM_GEN),     CAT_D,     test_func_B,             test_pre_func_B,             0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { DL_BEAM_TX,       EVENT_NAME(DL_BEAM_TX),      CAT_D,     test_func_A,             test_pre_func_B,             0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { UL_BEAM_GEN,      EVENT_NAME(UL_BEAM_GEN),     CAT_D,     test_func_B,             test_pre_func_B,             0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { UL_BEAM_TX,       EVENT_NAME(UL_BEAM_TX),      CAT_D,     test_func_A,             test_pre_func_B,             0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
};

int32_t gNBNextTaskMap[MAX_TASK_NUM_G_NB][MAX_NEXT_TASK_NUM] =
{
    // TTI_START
    {-1,                           -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // SYM2_WAKE_UP
    {-1,                           -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // SYM6_WAKE_UP
    {-1,                           -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // SYM11_WAKE_UP
    {-1,                           -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // SYM13_WAKE_UP
    {-1,                           -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // PRACH_WAKE_UP
    {-1,                           -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // SRS_WAKE_UP
    {-1,                           -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // DL_CONFIG
    {DL_PDSCH_TB,             DL_CTRL,         DL_PDSCH_RS,         DL_BEAM_GEN,                 -1,                  -1,                  -1,                  -1},

    // DL_PDSCH_TB
    {DL_PDSCH_SCRM,                -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // DL_PDSCH_SCRM
    {DL_PDSCH_SYM,                 -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // DL_PDSCH_SYM
    {DL_POST,                      -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // DL_PDSCH_RS
    {DL_PDSCH_SYM,                 -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // DL_CTRL
    {DL_POST,                      -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // UL_CONFIG
    {UL_IQ_DECOMP2,    UL_IQ_DECOMP6,      UL_IQ_DECOMP11,      UL_IQ_DECOMP13,            UL_PUCCH,            UL_PRACH,       UL_SRS_DECOMP,         UL_BEAM_GEN},

    // UL_IQ_DECOMP2
    {UL_PUSCH_CE0,       UL_PUSCH_CE7,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // UL_IQ_DECOMP6
    {UL_PUSCH_EQL0,     UL_PUSCH_EQL7,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // UL_IQ_DECOMP11
    {UL_PUSCH_CE7,                 -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // UL_IQ_DECOMP13
    {UL_PUSCH_EQL7,          UL_PUCCH,      UL_SRS_DECOMP,                  -1,                  -1,                  -1,                  -1,                  -1},

    // UL_PUSCH_CE0
    {UL_PUSCH_EQL0,     UL_PUSCH_EQL7,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // UL_PUSCH_CE7
    {UL_PUSCH_EQL7,                -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // UL_PUSCH_EQL0
    {UL_PUSCH_LLR,                 -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // UL_PUSCH_EQL7
    {UL_PUSCH_LLR,                 -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // UL_PUSCH_LLR
    {UL_PUSCH_DEC,                 -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // UL_PUSCH_DEC
    {UL_PUSCH_TB,                  -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // UL_PUSCH_TB
    {UL_POST,                      -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // UL_PUCCH
    {UL_POST,                      -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // UL_PRACH
    {UL_POST,                      -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // UL_SRS_DECOMP
    {UL_SRS_CE,                    -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // UL_SRS_CE
    {UL_SRS_POST,                  -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // UL_SRS_POST
    {UL_POST,                      -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // DL_POST
    {-1,                           -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // UL_POST
    {-1,                           -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // DL_BEAM_GEN
    {DL_BEAM_TX,                   -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // DL_BEAM_TX
    {DL_POST,                      -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // UL_BEAM_GEN
    {UL_BEAM_TX,                   -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // UL_BEAM_TX
    {UL_POST,                      -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},
};

__attribute__((aligned(IA_ALIGN))) EventCtrlStruct gEventCtrl[EBBU_POOL_MAX_TEST_CELL][MAX_TEST_CTX][MAX_TASK_NUM_G_NB][MAX_TEST_SPLIT_NUM];
static __attribute__((aligned(IA_ALIGN))) EventStruct gEvent[EBBU_POOL_MAX_TEST_CELL][MAX_TEST_CTX][MAX_TASK_NUM_G_NB][MAX_TEST_SPLIT_NUM];
__attribute__((aligned(IA_ALIGN))) EventChainDescStruct gEventChain[EBBU_POOL_MAX_TEST_CELL][MAX_TEST_CTX];
static SampleSplitStruct gsSampleSplit[EBBU_POOL_MAX_TEST_CELL][MAX_TEST_CTX][MAX_TEST_SPLIT_NUM];

static uint64_t dl_start_mlog[EBBU_POOL_MAX_TEST_CELL][MAX_TEST_CTX];
static uint64_t ul_start_mlog[EBBU_POOL_MAX_TEST_CELL][MAX_TEST_CTX];
extern volatile uint64_t ttistart;
extern int32_t dl_ul_count, dl_count, ul_count;
int32_t test_buffer_create()
{
    int32_t iCell, iCtx, iTask, iSplit;
    for(iCell = 0; iCell < EBBU_POOL_MAX_TEST_CELL; iCell ++)
        for(iCtx = 0; iCtx < MAX_TEST_CTX; iCtx ++)
            for(iTask = 0; iTask < MAX_TASK_NUM_G_NB; iTask ++)
                for(iSplit = 0; iSplit < MAX_TEST_SPLIT_NUM; iSplit ++)
                    gEventCtrl[iCell][iCtx][iTask][iSplit].dummy0 = (float *)_mm_malloc(sizeof(float), IA_ALIGN);
    return 0;
}

int32_t event_chain_gen(EventChainDescStruct *psEventChain)
{
    /*Construct the next event chain by copying existing array */
    psEventChain->eventChainDepth = MAX_TASK_NUM_G_NB;
    memcpy((void *)psEventChain->nextEventChain, (void *)gNBNextTaskMap, sizeof(gNBNextTaskMap));
    //printf("\nCopy gNBNextTaskMap with size %d", sizeof(gNBNextTaskMap));
    memset((void *)&psEventChain->nextEventCount, 0 , sizeof(psEventChain->nextEventCount));
    memset((void *)&psEventChain->preEventCount, 0 , sizeof(psEventChain->preEventCount));
    memset((void *)&psEventChain->preEventStat, 0 , sizeof(psEventChain->preEventStat));

    /*For each event, find all preceding dependent event */
    int32_t iEvent = 0;
    int32_t iNext = 0;

    /* Set the external event Wakeup Dependencies (apart from Task Dependency) */
    for(iEvent = 0; iEvent < MAX_TASK_NUM_G_NB; iEvent++)
    {
        psEventChain->preEventCountSave[iEvent] = testEventTable[iEvent].nWakeOnExtrernalEvent;
    }

    for(iEvent = 0; iEvent < MAX_TASK_NUM_G_NB; iEvent ++)
    {
        for(iNext = 0; iNext < MAX_NEXT_TASK_NUM; iNext ++)
        {
            if(psEventChain->nextEventChain[iEvent][iNext] != -1)
            {
                psEventChain->preEventCountSave[psEventChain->nextEventChain[iEvent][iNext]] ++;
                psEventChain->nextEventCount[iEvent] ++;
            }
        }
    }

    /*
    for(iEvent = 0; iEvent < MAX_TASK_NUM_G_NB; iEvent ++)
    {
        printf("\nEvent %d preEvent %d",iEvent,psEventChain->preEventCount[iEvent]);
    }
    */
    return 0;
}

int32_t event_chain_reset(EventChainDescStruct *psEventChain)
{
    memset((void *)&psEventChain->preEventStat, 0 , sizeof(psEventChain->preEventStat));

    /*For each event, find all preceding dependent event */
    int32_t iEvent = 0;

    /* Set the external event Wakeup Dependencies (apart from Task Dependency) */
    for(iEvent = 0; iEvent < MAX_TASK_NUM_G_NB; iEvent++)
    {
        psEventChain->preEventCount[iEvent] = psEventChain->preEventCountSave[iEvent];
    }
    return 0;
}

static void set_event_info(EventCtrlStruct *pEvenCtrl, int32_t eventId,
     int32_t iSplit, EventSendStruct *psEventSend)
{
    int32_t nCell = pEvenCtrl->nCellIdx;
    int32_t nSlot = pEvenCtrl->nSlotIdx;
    int32_t nCtx = nSlot % MAX_TEST_CTX;
    int32_t nQueueCtxNum = nSlot % gQueueCtxNum;
    EventStruct * pEvent = &gEvent[nCell][nCtx][eventId][iSplit];
    pEvent->pEventFunc = testEventTable[eventId].pEventFunc;
    pEvent->pEventArgs = pEvenCtrl;
    pEvent->nEventId = eventId;
    pEvent->nEventSentTime = ebbu_pool_tick();
    pEvent->nEventSentTimeMlog = MLogTick();
    pEvent->nEventAliveTime = 10000000;
    pEvent->nCoreAffinityMask = _mm256_set_epi64x(0,0,testEventTable[eventId].nCoreMask1,testEventTable[eventId].nCoreMask0);
    pEvent->nEventStatus = EBBUPOOL_EVENT_VALID;

    psEventSend->eDisposFlag = EBBUPOOL_NON_DISPOSABLE;

    psEventSend->ePrioCat = eventSendDic[testEventTable[eventId].nEventPrio];
    psEventSend->nQueueCtx = 0;
    if(gQueueCtxNum > 1)
        psEventSend->nQueueCtx = nQueueCtxNum;

    psEventSend->psEventStruct[0] = pEvent;
    psEventSend->nEventNum = 1;

    psEventSend->nPreFlag = testEventTable[eventId].nPrefetchFlag;

    return;
}

static void set_split_event_info(EventCtrlStruct *pEvenCtrl, int32_t eventId,
     int32_t nSplit, EventSendStruct *psEventSend)
{
    int32_t nCell = pEvenCtrl[0].nCellIdx;
    int32_t nSlot = pEvenCtrl[0].nSlotIdx;
    int32_t nCtx = nSlot % MAX_TEST_CTX;
    int32_t nQueueCtxNum = nSlot % gQueueCtxNum;
    int32_t iSplit = 0;
    for(; iSplit < nSplit; iSplit ++)
    {
        EventStruct *pEvent = &gEvent[nCell][nCtx][eventId][iSplit];
        pEvent->pEventFunc = testEventTable[eventId].pEventFunc;
        pEvent->pEventArgs = &pEvenCtrl[iSplit];
        pEvent->nEventId = eventId;
        pEvent->nEventSentTime = ebbu_pool_tick();
        pEvent->nEventSentTimeMlog = MLogTick();
        pEvent->nEventAliveTime = 10000000;
        pEvent->nCoreAffinityMask = _mm256_set_epi64x(0,0,testEventTable[eventId].nCoreMask1,testEventTable[eventId].nCoreMask0);
        pEvent->nEventStatus = EBBUPOOL_EVENT_VALID;
        psEventSend->psEventStruct[iSplit] = pEvent;
    }
    pEvenCtrl[0].tSendTime = MLogTick();
    psEventSend->eDisposFlag = EBBUPOOL_DISPOSABLE;
    psEventSend->ePrioCat = eventSendDic[testEventTable[eventId].nEventPrio];
    psEventSend->nQueueCtx = 0;
    if(gQueueCtxNum > 1)
        psEventSend->nQueueCtx = nQueueCtxNum;
    psEventSend->nEventNum = nSplit;
    psEventSend->nPreFlag = testEventTable[eventId].nPrefetchFlag;

    return;
}

int32_t next_event_unlock(void *pCookies)
{
    EventCtrlStruct *pEvenCtrl = (EventCtrlStruct *)pCookies;
    eBbuPoolHandler pHandler = (eBbuPoolHandler)pEvenCtrl->pHandler;
    int32_t nCell = pEvenCtrl->nCellIdx;
    int32_t nSlot = pEvenCtrl->nSlotIdx;
    int32_t nCtx = nSlot % MAX_TEST_CTX;
    int32_t eventId = pEvenCtrl->nEventId;
    EventChainDescStruct * pEventChain = &gEventChain[nCell][nCtx];

    if(eventId == DL_POST||eventId == UL_POST)
        ebbu_pool_queue_ctx_add(pHandler, nCtx);

    /*Set and check the status of all next event */
    /*Then decide whether to send next event or not */
    int32_t iNext = 0;
    int32_t nextEventId = 0;

    for(iNext = 0; iNext < pEventChain->nextEventCount[eventId]; iNext++)
    {
        nextEventId = pEventChain->nextEventChain[eventId][iNext];
        /*printf("\nnSlot %d event %d nextEventCount %d inext %d next %d next_pre_count %d next_pre_stat %d",
            nSlotIdx, nTaskId, pEventChain->nextEventCount[nTaskId], iNext, nextEventId,
            pEventChain->preEventCount[nextEventId], pEventChain->preEventStat[nextEventId]);
        */

        if(__atomic_add_fetch(&pEventChain->preEventStat[nextEventId], 1, __ATOMIC_ACQ_REL) ==
           __atomic_load_n(&pEventChain->preEventCount[nextEventId], __ATOMIC_ACQUIRE))
        {
            test_func_gen(pHandler, nCell, nSlot, nextEventId);
        }
    }

    return 0;
}

int32_t test_func_gen(eBbuPoolHandler pHandler, int32_t nCell, int32_t nSlot, int32_t eventId)
{
    int j;
    if(eventId >= MAX_TASK_NUM_G_NB || nCell >= EBBU_POOL_MAX_TEST_CELL)
    {
        printf("\nError! Wrong eventId %d max %d nCell %d",eventId, MAX_TASK_NUM_G_NB, nCell);
        exit(-1);
    }

    int32_t nCtx = nSlot % MAX_TEST_CTX;
    int32_t iNext, iNextEventId, nSplitIdx;
    EventChainDescStruct * pEventChain = &gEventChain[nCell][nCtx];
    EventSendStruct sEventSend;
    EventCtrlStruct *pEventCtrl;
    TaskPreGen sPara;
    int32_t nSplit = 1, ret = 0;

    uint64_t t1 = MLogTick();

    if(DL_CONFIG == eventId)
        dl_start_mlog[nCell][nCtx] = t1;
    else if(UL_CONFIG == eventId)
        ul_start_mlog[nCell][nCtx] = t1;

    // Klocwork check
    for (j = 0; j < MAX_TEST_SPLIT_NUM; j++)
        sPara.pTaskExePara[j] = (void *)&gsSampleSplit[nCell%EBBU_POOL_MAX_TEST_CELL][nSlot%MAX_TEST_CTX][j];

    if (testEventTable[eventId].pPreEventFunc)
    {
        /* Run Pre Event and Find out how many split */
        sPara.nTaskNum = 1;
        testEventTable[eventId].pPreEventFunc(nSlot, nCell, &sPara);
        nSplit = sPara.nTaskNum;
        if(nSplit > 1)
        {
            /* Add the split to all the Nex Next Dependencies */
            for(iNext = 0; iNext < pEventChain->nextEventCount[eventId]; iNext++)
            {
                iNextEventId = pEventChain->nextEventChain[eventId][iNext];
                __atomic_add_fetch(&pEventChain->preEventCount[iNextEventId], nSplit - 1, __ATOMIC_ACQ_REL);
            }
        }
    }

    //send the splitted events together, save ebbupool internal overhead
    for(nSplitIdx = 0; nSplitIdx < nSplit; nSplitIdx++)
    {
        pEventCtrl = &gEventCtrl[nCell][nCtx][eventId][nSplitIdx];
        pEventCtrl->nEventId = eventId;
        pEventCtrl->nSplitIdx = nSplitIdx;
        pEventCtrl->nCellIdx = nCell;
        pEventCtrl->nSlotIdx = nSlot;
        pEventCtrl->pTaskPara = sPara.pTaskExePara[nSplitIdx];
        pEventCtrl->pHandler = pHandler;
    }

    set_split_event_info(&gEventCtrl[nCell][nCtx][eventId][0], eventId, nSplit, &sEventSend);
    ret = ebbu_pool_send_event(pHandler, sEventSend);

    if(0 != ret)
        printf("\nEvent %d gen failed!",eventId);

    MLogTask(MAX_TASK_NUM_G_NB * nCell + eventId + 2000, t1, MLogTick());

    return 0;
}
void test_pre_func_A(uint32_t nSubframe, uint16_t nCellIdx, TaskPreGen *pPara)
{
    // uint64_t t1 = MLogTick();
    //printf("\nfunc pre A event %d",pEvenCtrl->nEventId);
    // int32_t ret = 0;
    //do some traffic
    //ret = simulate_traffic(pCookies, 1000);

    pPara->nTaskNum = nSplitNumCell[nCellIdx];
    int32_t iSplit = 0;
    for(iSplit = 0; iSplit < pPara->nTaskNum; iSplit ++)
    {
        pPara->pTaskExePara[iSplit] = (void *)&gsSampleSplit[nCellIdx%EBBU_POOL_MAX_TEST_CELL][nSubframe%MAX_TEST_CTX][iSplit];
    }
    return;
    //MLogTask(MAX_TASK_NUM_G_NB * pInputPara->nCellIdx + pEvenCtrl->nEventId, t1, MLogTick());
}


void test_pre_func_B(uint32_t nSubframe, uint16_t nCellIdx, TaskPreGen *pPara)
{
    // uint64_t t1 = MLogTick();
    //printf("\nfunc pre A event %d",pEvenCtrl->nEventId);
    // int32_t ret = 0;

    //do some traffic
    //ret = simulate_traffic(pCookies, 1000);
    int32_t iSplit = 0;
    for(iSplit = 0; iSplit < pPara->nTaskNum; iSplit ++)
    {
        pPara->pTaskExePara[iSplit] = (void *)&gsSampleSplit[nCellIdx%EBBU_POOL_MAX_TEST_CELL][nSubframe%MAX_TEST_CTX][iSplit];
    }
    return;

    pPara->nTaskNum = nSplitNumCell[nCellIdx];

    //MLogTask(MAX_TASK_NUM_G_NB * pInputPara->nCellIdx + pEvenCtrl->nEventId, t1, MLogTick());
}

int32_t test_func_A(void *pCookies)
{
    EventCtrlStruct *pEvenCtrl = (EventCtrlStruct *)pCookies;

    uint64_t t1 = MLogTick();
#if 0
    //printf("\nfunc A event %d",pEvenCtrl->nEventId);
    if(DL_CONFIG == pEvenCtrl->nEventId)
    {
        app_bbu_pool_task_dl_config(pCookies);
        MLogTask(pEvenCtrl->nCellIdx + 4000, pEvenCtrl->tSendTime, t1);
    }

    if(UL_CONFIG == pEvenCtrl->nEventId)
    {
        app_bbu_pool_task_ul_config(pCookies);
        MLogTask(pEvenCtrl->nCellIdx + 5000, pEvenCtrl->tSendTime, t1);
    }
#endif
    // int32_t ret = 0;

    //do some traffic
    //ret = simulate_traffic(pCookies, 3000);
    //usleep(10);

    //unlock the next task
    next_event_unlock(pCookies);

    MLogTask(MAX_TASK_NUM_G_NB * pEvenCtrl->nCellIdx + pEvenCtrl->nEventId, t1, MLogTick());
    //printf("\nfunc a latency %llu",MLogTick()-t1);

    return 0;

}

int32_t test_func_B(void *pCookies)
{
    EventCtrlStruct *pEvenCtrl = (EventCtrlStruct *)pCookies;

    uint64_t t1 = MLogTick();
    //printf("\nfunc B event %d",pEvenCtrl->nEventId);
    // int32_t ret = 0;

    //do some traffic
    //ret = simulate_traffic(pCookies, 5000);
    //usleep(10);

    //unlock the next task
    next_event_unlock(pCookies);

    MLogTask(MAX_TASK_NUM_G_NB * pEvenCtrl->nCellIdx + pEvenCtrl->nEventId, t1, MLogTick());

    return 0;
}

int32_t test_func_C(void *pCookies)
{
    EventCtrlStruct *pEvenCtrl = (EventCtrlStruct *)pCookies;

    uint64_t t1 = MLogTick();
    //printf("\nfunc B event %d",pEvenCtrl->nEventId);
    int32_t ret = 0;

    //do some traffic

    MLogTask(MAX_TASK_NUM_G_NB * pEvenCtrl->nCellIdx + pEvenCtrl->nEventId, t1, MLogTick());

    if(pEvenCtrl->nEventId == DL_POST || pEvenCtrl->nEventId == UL_POST)
    {
        if(__atomic_sub_fetch(&dl_ul_count, 1, __ATOMIC_ACQ_REL) == 0)
        {
            MLogTask(MAX_TASK_NUM_G_NB * pEvenCtrl->nCellIdx + 6000, ttistart, MLogTick());
        }
    }

    MLogTask(77777, t1, MLogTick());
    return ret;
}

#if 0
int32_t simulate_traffic(void *pCookies, int32_t testCount)
{
    //printf("\ndo traffic!");
    EventCtrlStruct *pEvenCtrl = (EventCtrlStruct *)pCookies;
    __m256 sigma2 = _mm256_set1_ps(testCount/1234.5);
    __m256 ftemp1, ftemp2;

    int32_t m = testCount;
    m = m/2;

    while(m > 0)
    {
        ftemp1 = _mm256_rcp_ps(sigma2);
        ftemp2 = _mm256_sub_ps(_mm256_set1_ps(0), sigma2);
        ftemp2 = _mm256_fmadd_ps(ftemp1, sigma2, ftemp2);
        sigma2 = _mm256_rcp_ps(ftemp2);
        m --;
    }

    int32_t nfloat = 8; //256bits has eight 32bits
    float *dummy = (float *)&sigma2;
    *pEvenCtrl->dummy0 = 0;
    for(m = 0; m < nfloat; m++)
        *pEvenCtrl->dummy0 += dummy[m];

    return 0;
}
#endif
