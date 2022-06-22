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
 * @brief This file has the System Debug Trace Logger (Mlog) Task IDs used by XRAN library
 * @file mlog_task_id.h
 * @ingroup group_lte_source_common
 * @author Intel Corporation
 **/

#ifndef _XRAN_LIB_TASK_ID_H_
#define _XRAN_LIB_TASK_ID_H_

#ifdef __cplusplus
extern "C" {
#endif

#define RESOURCE_CORE_0                             0
#define RESOURCE_CORE_1                             1
#define RESOURCE_CORE_2                             2
#define RESOURCE_CORE_3                             3
#define RESOURCE_CORE_4                             4
#define RESOURCE_CORE_5                             5
#define RESOURCE_CORE_6                             6
#define RESOURCE_CORE_7                             7
#define RESOURCE_CORE_8                             8
#define RESOURCE_CORE_9                             9
#define RESOURCE_CORE_10                            10
#define RESOURCE_CORE_11                            11
#define RESOURCE_CORE_12                            12
#define RESOURCE_CORE_13                            13
#define RESOURCE_CORE_14                            14
#define RESOURCE_CORE_15                            15
#define RESOURCE_CORE_16                            16

#define RESOURCE_IA_CORE                            100

//--------------------------------------------------------------------
// XRAN
//--------------------------------------------------------------------

//--------------------------------------------------------------------
// POLLING
//--------------------------------------------------------------------
#define PID_XRAN_MAIN                           101

#define PID_XRAN_BBDEV_DL_POLL                  51
#define PID_XRAN_BBDEV_DL_POLL_DISPATCH         52
#define PID_XRAN_BBDEV_UL_POLL                  53
#define PID_XRAN_BBDEV_UL_POLL_DISPATCH         54

#define PID_TTI_TIMER                           3100
#define PID_TTI_CB                              3101

#define PID_SYM_TIMER                           3102
//#define PID_GNB_PROC_TIMING_TIMEOUT             3103

#define PID_TIME_SYSTIME_POLL                   3104
#define PID_TIME_SYSTIME_STOP                   3105
#define PID_TIME_ARM_TIMER                      3106

#define PID_RADIO_FREQ_RX_PKT                   3107
#define PID_RADIO_RX_STOP                       3108
#define PID_RADIO_RX_UL_IQ                      3109
#define PID_RADIO_PRACH_PKT                     3110
#define PID_RADIO_FE_COMPRESS                   3111
#define PID_RADIO_FE_DECOMPRESS                 3112
#define PID_RADIO_TX_BYPASS_PROC                3113
#define PID_RADIO_ETH_TX_BURST                  3114
#define PID_RADIO_TX_DL_IQ                      3115
#define PID_RADIO_RX_VALIDATE                   3116
#define PID_RADIO_RX_IRQ_ON                     3117
#define PID_RADIO_RX_IRQ_OFF                    3118
#define PID_RADIO_RX_EPOLL_WAIT                 3119
#define PID_RADIO_TX_LTEMODE_PROC               3120
#define PID_RADIO_RX_LTEMODE_PROC               3121
#define PID_RADIO_TX_PLAY_BACK_IQ               3122
#define PID_PROCESS_TX_SYM                      3123
#define PID_DISPATCH_TX_SYM                     3124
#define PID_PREPARE_TX_PKT                      3125
#define PID_ATTACH_EXT_BUF                      3126
#define PID_ETH_ENQUEUE_BURST                   3127

#define PID_CP_DL_CB                            3128
#define PID_CP_UL_CB                            3129
#define PID_UP_DL_CB                            3130
#define PID_SYM_OTA_CB                          3131
#define PID_TTI_CB_TO_PHY                       3132
#define PID_HALF_SLOT_CB_TO_PHY                 3133
#define PID_FULL_SLOT_CB_TO_PHY                 3134
#define PID_UP_UL_HALF_DEAD_LINE_CB             3135
#define PID_UP_UL_FULL_DEAD_LINE_CB             3136
#define PID_UP_UL_USER_DEAD_LINE_CB             3137
#define PID_PROCESS_UP_PKT                      3140
#define PID_PROCESS_UP_PKT_SRS                  3141
#define PID_PROCESS_UP_PKT_PARSE                3142
#define PID_PROCESS_CP_PKT                      3143
#define PID_PROCESS_DELAY_MEAS_PKT              3144
#define PID_UP_UL_ONE_FOURTHS_DEAD_LINE_CB      3145
#define PID_UP_UL_THREE_FOURTHS_DEAD_LINE_CB    3146
#define PID_UP_STATIC_SRS_DEAD_LINE_CB          3147

#define PID_TIME_ARM_TIMER_DEADLINE             3150
#define PID_TIME_ARM_USER_TIMER_DEADLINE        3151

#define PID_REQUEUE_TX_SYM                      3160

#ifdef __cplusplus
}
#endif

#endif /* _XRAN_LIB_TASK_ID_H_ */
