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
extern int32_t app_stop(void);

#ifdef POLL_EBBU_OFFLOAD
#include "xran_printf.h"
#include "xran_common.h"
#include "xran_timer.h"

#define NSEC_PER_SEC   1000000000L
#define NSEC_PER_USEC  1000L
#define THRESHOLD      35
#define SEC_MOD_STOP   60

extern enum xran_if_state xran_if_current_state;
extern uint64_t ticks_per_usec;
extern long fine_tuning[5][2];
extern uint8_t slots_per_subframe[5];

/* this static variable is used to record the current status of symbol pollling function */
static bool firstCall = false;
/* this static variable is used to count the accumulated time lenght */
static long sym_time_acc = 0;
/* this static variable is used to count the total symbol number */
static long sym_number_acc = 0;
#endif

int32_t test_func_A(void *pCookies);
int32_t test_func_B(void *pCookies);
int32_t test_func_C(void *pCookies);

void test_pre_func_A(uint32_t nSubframe, uint16_t nCellIdx, TaskPreGen *pPara);
void test_pre_func_B(uint32_t nSubframe, uint16_t nCellIdx, TaskPreGen *pPara);

int32_t test_func_gen(eBbuPoolHandler pHandler, int32_t nCell, int32_t nSlot, int32_t eventId, uint8_t mu);

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
#ifdef POLL_EBBU_OFFLOAD
    { SYM_POLL,         EVENT_NAME(SYM_POLL),        CAT_B,  app_bbu_pool_task_sym_poll,      NULL,                   0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { PKT_POLL,         EVENT_NAME(PKT_POLL),        CAT_B,  app_bbu_pool_task_pkt_proc_poll, NULL,                   0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { DL_CP_POLL,       EVENT_NAME(DL_CP_POLL),      CAT_B,  app_bbu_pool_task_dl_cp_poll,    NULL,                   0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { UL_CP_POLL,       EVENT_NAME(UL_CP_POLL),      CAT_B,  app_bbu_pool_task_ul_cp_poll,    NULL,                   0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
    { TTI_POLL,         EVENT_NAME(TTI_POLL),        CAT_B,  app_bbu_pool_task_tti_poll,      NULL,                   0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
#endif
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
    { UL_TX,            EVENT_NAME(UL_TX),           CAT_D, app_bbu_pool_task_ul_tx, app_bbu_pool_pre_task_ul_tx,     0,           0,                        0,            0x00ffffffffffffff,          0xfffffffffffffff},
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

#ifdef POLL_EBBU_OFFLOAD
    // SYM_POLL
    {-1,                           -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // PKT_POLL
    {-1,                           -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // DL_CP_POLL
    {-1,                           -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // UL_CP_POLL
    {-1,                           -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

    // TTI_POLL
    {-1,                           -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},

#endif
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

    // UL_TX
    {-1,                           -1,                 -1,                  -1,                  -1,                  -1,                  -1,                  -1},
};

__attribute__((aligned(IA_ALIGN))) EventCtrlStruct gEventCtrl[EBBU_POOL_MAX_TEST_CELL][MAX_TEST_CTX][MAX_TASK_NUM_G_NB][XRAN_MAX_NUM_MU][MAX_TEST_SPLIT_NUM];
static __attribute__((aligned(IA_ALIGN))) EventStruct gEvent[EBBU_POOL_MAX_TEST_CELL][MAX_TEST_CTX][MAX_TASK_NUM_G_NB][MAX_TEST_SPLIT_NUM];
__attribute__((aligned(IA_ALIGN))) EventChainDescStruct gEventChain[EBBU_POOL_MAX_TEST_CELL][MAX_TEST_CTX][XRAN_MAX_NUM_MU];
static SampleSplitStruct gsSampleSplit[EBBU_POOL_MAX_TEST_CELL][MAX_TEST_CTX][MAX_TEST_SPLIT_NUM];

static uint64_t dl_start_mlog[EBBU_POOL_MAX_TEST_CELL][MAX_TEST_CTX];
static uint64_t ul_start_mlog[EBBU_POOL_MAX_TEST_CELL][MAX_TEST_CTX];
extern volatile uint64_t ttistart;
extern int32_t dl_ul_count, dl_count, ul_count;
int32_t test_buffer_create()
{
    int32_t iCell, iCtx, iTask, iSplit, iMu;
    for(iCell = 0; iCell < EBBU_POOL_MAX_TEST_CELL; iCell ++){
        for(iCtx = 0; iCtx < MAX_TEST_CTX; iCtx ++){
            for(iTask = 0; iTask < MAX_TASK_NUM_G_NB; iTask ++){
                for(iMu=0;iMu<XRAN_MAX_NUM_MU;iMu++){
                    for(iSplit = 0; iSplit < MAX_TEST_SPLIT_NUM; iSplit ++){
                        gEventCtrl[iCell][iCtx][iTask][iMu][iSplit].dummy0 = (float *)_mm_malloc(sizeof(float), IA_ALIGN);
                    }
                }
            }
        }
    }

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
    uint8_t mu = pEvenCtrl->mu;
    EventChainDescStruct * pEventChain = &gEventChain[nCell][nCtx][mu];


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
            test_func_gen(pHandler, nCell, nSlot, nextEventId, mu);
        }
    }

    return 0;
}

int32_t test_func_gen(eBbuPoolHandler pHandler, int32_t nCell, int32_t nSlot, int32_t eventId, uint8_t mu)
{
    int j;
    if(eventId >= MAX_TASK_NUM_G_NB || nCell >= EBBU_POOL_MAX_TEST_CELL)
    {
        printf("\nError! Wrong eventId %d max %d nCell %d",eventId, MAX_TASK_NUM_G_NB, nCell);
        exit(-1);
    }

    int32_t nCtx = nSlot % MAX_TEST_CTX;
    int32_t iNext, iNextEventId, nSplitIdx;
    EventChainDescStruct * pEventChain = &gEventChain[nCell][nCtx][mu];
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

    //Currently only supoort 1 numa node configuration for sample-app
    sEventSend.nNodeIdx = 0;

    //send the splitted events together, save ebbupool internal overhead
    for(nSplitIdx = 0; nSplitIdx < nSplit; nSplitIdx++)
    {
        pEventCtrl = &gEventCtrl[nCell][nCtx][eventId][mu][nSplitIdx];
        pEventCtrl->nEventId = eventId;
        pEventCtrl->nSplitIdx = nSplitIdx;
        pEventCtrl->nCellIdx = nCell;
        pEventCtrl->nSlotIdx = nSlot;
        pEventCtrl->pTaskPara = sPara.pTaskExePara[nSplitIdx];
        pEventCtrl->pHandler = pHandler;
        pEventCtrl->mu = mu;
    }

    set_split_event_info(&gEventCtrl[nCell][nCtx][eventId][mu][0], eventId, nSplit, &sEventSend);
    ret = ebbu_pool_send_event(pHandler, &sEventSend);

    if(0 != ret){
        app_stop();
        printf("\nEvent %d gen failed!",eventId);
    }

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

    //pPara->nTaskNum = nSplitNumCell[nCellIdx];

    //MLogTask(MAX_TASK_NUM_G_NB * pInputPara->nCellIdx + pEvenCtrl->nEventId, t1, MLogTick());
}
int32_t test_func_A(void *pCookies)
{
    EventCtrlStruct *pEvenCtrl = (EventCtrlStruct *)pCookies;

    uint64_t t1 = MLogTick();
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

#ifdef POLL_EBBU_OFFLOAD
uint32_t app_pool_polling_event_info_set(EventCtrlStruct *pEventCtrl, int32_t nSplit, EventSendStruct *psEventSend, uint32_t nDelay)
{
    int32_t nTaskId = pEventCtrl[0].nEventId;
    int32_t nCellIdx = pEventCtrl[0].nCellIdx;
    int32_t nSlotIdx = pEventCtrl[0].nSlotIdx;
    int32_t nCtx = nSlotIdx % gQueueCtxNum;
    int32_t nQueueIdx = nSlotIdx;
    int32_t iSplit = 0;
    int32_t nPrio = testEventTable[nTaskId].nEventPrio;

    for(; iSplit < nSplit; iSplit++)
    {
        EventStruct *pEvent = &gEvent[nCellIdx][nCtx][nTaskId][iSplit];
        pEvent->pEventFunc = testEventTable[nTaskId].pEventFunc;
        pEvent->pEventArgs = &pEventCtrl[iSplit];
        pEvent->nEventId = nTaskId;
        pEvent->nEventSentTime = ebbu_pool_tick();
        pEvent->nEventSentTimeMlog = MLogTick();
        pEvent->nEventExpectedTime = pEvent->nEventSentTime + (nDelay*ticks_per_usec);
        pEvent->nEventAliveTime = 100000000;
        pEvent->nCoreAffinityMask = _mm256_set_epi64x(0,0,testEventTable[nTaskId].nCoreMask1,testEventTable[nTaskId].nCoreMask0);
        pEvent->nEventStatus = EBBUPOOL_EVENT_VALID;
        psEventSend->psEventStruct[iSplit] = pEvent;
    }
    psEventSend->eDisposFlag = EBBUPOOL_NON_DISPOSABLE;

    psEventSend->ePrioCat = (EventPrioEnum)eventSendDic[nPrio];
    psEventSend->nQueueCtx = 0;
    if(gQueueCtxNum > 1)
    {
        if ((nPrio > CAT_A) && (nPrio < CAT_D))
        {
            psEventSend->nQueueCtx = (nQueueIdx % gQueueCtxNum);
        }
    }

    psEventSend->nPreFlag = testEventTable[nTaskId].nPrefetchFlag;
    psEventSend->nEventNum = nSplit;

    return psEventSend->ePrioCat;
}

int32_t app_bbu_polling_event_gen(uint32_t nCellIdx, uint32_t nTaskId, uint32_t nDelay, void *pTaskPara)
{
    eBbuPoolHandler pHandler = app_get_ebbu_pool_handler();
    EventSendStruct sEventSend;
    TaskPreGen sPara;

    uint32_t nSlotIdx = 0;
    uint32_t nSplit = 1;
    int32_t nCtx = 0;
    uint8_t mu = 0;


    EventCtrlStruct *pEventCtrl;
    uint32_t nSplitIdx;

    int32_t ret;

    if (nTaskId >= MAX_TASK_NUM_G_NB)
    {
        printf("app_bbu_polling_event_gen ERROR: nTaskId(%d) >= MAX_TASK_NUM_G_NB[%d]\n", nTaskId, MAX_TASK_NUM_G_NB);
        return FAILURE;
    }

    // Klocwork check
    for (nSplitIdx = 0; nSplitIdx < MAX_TEST_SPLIT_NUM; nSplitIdx++)
        sPara.pTaskExePara[nSplitIdx] = (void *)&gsSampleSplit[nCellIdx%EBBU_POOL_MAX_TEST_CELL][nSlotIdx%MAX_TEST_CTX][nSplitIdx];

    // Klockwork Check
    sPara.nTaskNum = 1;
    for(nSplitIdx = 0; nSplitIdx < sPara.nTaskNum; nSplitIdx ++)
    {
        sPara.pTaskExePara[nSplitIdx] = pTaskPara;
    }

    if (testEventTable[nTaskId].pPreEventFunc)
    {
        /* Run Pre Event and Find out how many split */
        sPara.nTaskNum = 1;
        testEventTable[nTaskId].pPreEventFunc(nSlotIdx, nCellIdx, &sPara);
        nSplit = sPara.nTaskNum;
    }

    sEventSend.nNodeIdx = 0;

    for(nSplitIdx = 0; nSplitIdx < sPara.nTaskNum; nSplitIdx++)
    {
        pEventCtrl = &gEventCtrl[nCellIdx][nCtx][nTaskId][mu][nSplitIdx];
        pEventCtrl->nEventId    = nTaskId;
        pEventCtrl->nSplitIdx   = nSplitIdx;
        pEventCtrl->nCellIdx    = nCellIdx;
        pEventCtrl->nSlotIdx    = nSlotIdx;
        pEventCtrl->pHandler    = pHandler;
        pEventCtrl->pTaskPara   = sPara.pTaskExePara[nSplitIdx];
    }

    /* Send the splitted events together, save ebbupool internal overhead */
    app_pool_polling_event_info_set(&gEventCtrl[nCellIdx][nCtx][nTaskId][mu][0], nSplit, &sEventSend, nDelay);

    ret = ebbu_pool_send_polling_event(pHandler, &sEventSend);
    if (0 != ret)
    {
        printf("\napp_bbu_polling_event_gen: Event %d gen failed!", nTaskId);
    }

    return ret;
}

int32_t app_bbu_pool_task_sym_poll(void *pCookies)
{
    uint32_t timerMu = xran_timingsource_get_numerology();
    int32_t debugStop = timing_get_debug_stop();
    int32_t debugStopCount = timing_get_debug_stop_count();
    uint64_t started_second = timing_get_start_second();
    PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();

    struct xran_common_counters *pCnt = xran_fh_counters_ebbu_offload();
    struct timespec* p_cur_time = &pCtx->ebbu_offload_cur_time;
    struct timespec* p_last_time = &pCtx->ebbu_offload_last_time;
    long last_time = 0, delta = 0;
    long interval_ns = xran_timer_get_interval_ebbu_offload();
    long tm_threshold_high = interval_ns * N_SYM_PER_SLOT * 2;//2 slots
    long tm_threshold_low = interval_ns * 2; //2 symbols
    long tm_threshold_sym_up_window = interval_ns * pCtx->sym_up_window; //U-Plane TX window symbols
    int32_t i,j,nDelay = 5;
    uint64_t t1, t2;

    if(likely(XRAN_RUNNING == xran_if_current_state))
    {
        if(firstCall == false)
        {
            clock_gettime(CLOCK_REALTIME, p_last_time);
            pCtx->current_second = p_last_time->tv_sec;
            firstCall = true;
        }

        last_time = (p_last_time->tv_sec * NSEC_PER_SEC + p_last_time->tv_nsec);
        clock_gettime(CLOCK_REALTIME, p_cur_time);

        timing_adjust_gps_second_ebbu_offload(p_cur_time);

        delta = (p_cur_time->tv_sec * NSEC_PER_SEC + p_cur_time->tv_nsec) - last_time;

        //add tm exception handling
        if(unlikely(labs(delta) > tm_threshold_low) && pCtx->first_call)
        {
            print_dbg("poll_next_tick exceed 2 symbols threshold with delta:%ld(ns), used_tick:%ld(tick) \n",
                    delta, pCtx->used_tick);
            pCnt->timer_missed_sym++;

            if(unlikely(labs(delta) > tm_threshold_sym_up_window))
            {
                print_dbg("poll_next_tick exceed UP TX window! delta:%ld(ns), sym_up_window is %ld, used_tick:%ld(tick) \n",
                        delta, tm_threshold_sym_up_window, pCtx->used_tick);
                pCnt->timer_missed_sym_window++;
            }

            if(unlikely(labs(delta) > tm_threshold_high))
            {
                print_dbg("poll_next_tick exceed 2 slots threshold, stop xran! delta:%ld(ns), used_tick:%ld(tick) \n",
                        delta, pCtx->used_tick);
                printf("poll_next_tick exceed 2 slots threshold! delta:%ld(ns), used_tick:%ld(tick) \n",
                        delta, pCtx->used_tick);
                pCnt->timer_missed_slot++;
            }
        }

        if(delta > (interval_ns - THRESHOLD))
        {
            pCtx->used_tick = 0;
            if (debugStop &&(debugStopCount > 0) && (pCnt->tx_counter >= debugStopCount))
            {
                printf("STOP:[%ld.%09ld], debugStopCount %d, tx_counter %ld\n",
                        p_cur_time->tv_sec, p_cur_time->tv_nsec, debugStopCount, pCnt->tx_counter);
                xran_if_current_state = XRAN_STOPPED;
            }

            while(delta > (interval_ns - THRESHOLD))
            {
                if(pCtx->current_second != p_cur_time->tv_sec)
                {
                    pCtx->current_second = p_cur_time->tv_sec;

                    xran_updateSfnSecStart();

                    for (i=0; i < XRAN_MAX_NUM_MU; i++)
                    {
                        pCtx->ebbu_offload_ota_sym_cnt_mu[i] = 0;
                        pCtx->ebbu_offload_ota_sym_idx_mu[i] = 0;
                        for(j=0;j<XRAN_PORTS_NUM;j++){
                            pCtx->ebbu_offload_ota_tti_cnt_mu[j][i] = 0;
                        }
                    }
                    sym_number_acc = sym_time_acc = 0;
                    print_dbg("ToS:C Sync timestamp: [%ld.%09ld]\n", p_cur_time->tv_sec, p_cur_time->tv_nsec);

                    if(debugStop)
                    {
                        if(p_cur_time->tv_sec > started_second && ((p_cur_time->tv_sec % SEC_MOD_STOP) == 0))
                        {
                            uint32_t tti         = pCtx->ebbu_offload_ota_tti_cnt_mu[0][timerMu];
                            uint32_t slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME(interval_us));
                            uint32_t subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME(interval_us),  SUBFRAMES_PER_SYSTEMFRAME);
                            uint32_t frame_id    = XranGetFrameNum(tti, pCtx->xran_SFN_at_Sec_Start,
                                                    SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval_us));

                            printf("STOP:[%ld.%09ld] (%d : %d : %d)\n",
                                    p_cur_time->tv_sec, p_cur_time->tv_nsec,frame_id, subframe_id, slot_id);

                            xran_if_current_state = XRAN_STOPPED;
                        }
                    }
                    p_cur_time->tv_nsec = 0; // adjust to 1pps
                } /* second changed */
                else
                {
                    pCtx->ebbu_offload_ota_sym_cnt_mu[timerMu] = XranIncrementSymIdx(pCtx->ebbu_offload_ota_sym_cnt_mu[timerMu],
                                                            XRAN_NUM_OF_SYMBOL_PER_SLOT*slots_per_subframe[timerMu]);

                    /* timerMu is highest configured numerology. Update the rest */
                    for (i = timerMu - 1; i >= 0; i--)
                    {
                        pCtx->ebbu_offload_ota_sym_cnt_mu[i] = pCtx->ebbu_offload_ota_sym_cnt_mu[timerMu] >> (timerMu - i);
                    }

                    pCtx->ebbu_offload_ota_sym_cnt_mu[XRAN_NBIOT_MU] = pCtx->ebbu_offload_ota_sym_cnt_mu[0];      /*mu kept for NB-IOT*/

                    /* adjust to sym boundary */
                    if(sym_number_acc & 1)
                        sym_time_acc +=  fine_tuning[timerMu][0];
                    else
                        sym_time_acc +=  fine_tuning[timerMu][1];

                    /* fine tune to second boundary */
                    if(sym_number_acc % 13 == 0)
                        sym_time_acc += 1;

                    p_cur_time->tv_nsec = sym_time_acc;
                    sym_number_acc++;
                }

                if(likely(XRAN_RUNNING == xran_if_current_state))
                {
                    t1 = xran_tick();
                    xran_sym_poll_callback_task_ebbu_offload();
                    t2 = xran_tick();
                    pCtx->used_tick += get_ticks_diff(t2, t1);

                    for(i=0;i<XRAN_MAX_NUM_MU;i++)
                    {
                        /* All numerologies for all RUs are processed now.
                        * If current symbol for a given mu (ebbu_offload_ota_sym_cnt_mu) matches symbol_idx%14 then that means
                        * processing for that symbol has completed above and we can increment 'current symbol' (i.e. ebbu_offload_ota_sym_cnt_mu)
                        * If it doesn't then processing for ebbu_offload_ota_sym_cnt_mu hasn't happenned yet and we don't increment it.
                        */

                        if(XranGetSymNum(pCtx->ebbu_offload_ota_sym_cnt_mu[i], XRAN_NUM_OF_SYMBOL_PER_SLOT) == pCtx->ebbu_offload_ota_sym_idx_mu[i])
                        {
                            pCtx->ebbu_offload_ota_sym_idx_mu[i]++;
                            if(pCtx->ebbu_offload_ota_sym_idx_mu[i] >= N_SYM_PER_SLOT)
                            {
                                pCtx->ebbu_offload_ota_sym_idx_mu[i] = 0;
                            }
                        }
                    }
                }
                delta -= interval_ns;
            }
            p_last_time->tv_sec = p_cur_time->tv_sec;
            p_last_time->tv_nsec = p_cur_time->tv_nsec;

            t1 = xran_tick();
            xran_sym_poll_task_ebbu_offload();
            t2 = xran_tick();
            pCtx->used_tick += get_ticks_diff(t2, t1);

        }
        else
        {
              if(likely((xran_if_current_state == XRAN_RUNNING)||(xran_if_current_state == XRAN_OWDM)))
              {
                  t1 = xran_tick();
                  xran_sym_poll_task_ebbu_offload();
                  t2 = xran_tick();
                  pCtx->used_tick += get_ticks_diff(t2, t1);
              }
        }
    }

     if(XRAN_STOPPED != xran_if_current_state)
    {
        if(pCtx->ebbu_offload_ota_sym_idx_mu[timerMu] == 0 || ((interval_ns - delta) < (nDelay * 1000)))
            nDelay = 0;
        app_bbu_polling_event_gen(0, SYM_POLL, nDelay, NULL); // Call symbol polling every 5us
    }
    else
    {
        xran_timing_destroy_cbs((void*)xran_dev_get_ctx_ebbu_offload());
        print_dbg("pCnt->timer_missed_sym is %ld, pCnt->timer_missed_sym_window is %ld, pCnt->timer_missed_slot is %ld\n", \
        pCnt->timer_missed_sym, pCnt->timer_missed_sym_window, pCnt->timer_missed_slot);
    }

    return EBBUPOOL_CORRECT;
}

int32_t app_bbu_pool_task_pkt_proc_poll(void *pCookies)
{
    int32_t nDelay = 2;

    if(likely(XRAN_RUNNING == xran_if_current_state))
    {
        xran_pkt_proc_poll_task_ebbu_offload();
    }

    if(XRAN_STOPPED != xran_if_current_state)
    {
        app_bbu_polling_event_gen(0, PKT_POLL, nDelay, NULL); // Call symbol polling every 2 us
    }

    return EBBUPOOL_CORRECT;
}
int32_t app_bbu_pool_task_dl_cp_poll(void *pCookies)
{
    EventCtrlStruct *pEventCtrl = (EventCtrlStruct *)pCookies;

    if(likely(XRAN_RUNNING == xran_if_current_state))
    {
        xran_task_dl_cp_ebbu_offload(pEventCtrl->pTaskPara);
    }

    return EBBUPOOL_CORRECT;
}
int32_t app_bbu_pool_task_ul_cp_poll(void *pCookies)
{
    EventCtrlStruct *pEventCtrl = (EventCtrlStruct *)pCookies;

    if(likely(XRAN_RUNNING == xran_if_current_state))
    {
        xran_task_ul_cp_ebbu_offload(pEventCtrl->pTaskPara);
    }

    return EBBUPOOL_CORRECT;
}
int32_t app_bbu_pool_task_tti_poll(void *pCookies)
{
    EventCtrlStruct *pEventCtrl = (EventCtrlStruct *)pCookies;

    if(likely(XRAN_RUNNING == xran_if_current_state))
    {
        xran_task_tti_ebbu_offload(pEventCtrl->pTaskPara);
    }

    return EBBUPOOL_CORRECT;
}
#endif
