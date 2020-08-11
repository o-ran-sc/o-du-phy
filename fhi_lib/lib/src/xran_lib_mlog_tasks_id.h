/******************************************************************************
*
*   Copyright (c) 2019 Intel.
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

#ifndef _XRAN_TASK_ID_H_
#define _XRAN_TASK_ID_H_

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
#define PID_XRAN_BBDEV_DL_POLL                  51
#define PID_XRAN_BBDEV_DL_POLL_DISPATCH         52
#define PID_XRAN_BBDEV_UL_POLL                  53
#define PID_XRAN_BBDEV_UL_POLL_DISPATCH         54

#define PID_TTI_TIMER                           2100
#define PID_TTI_CB                              2101

#define PID_SYM_TIMER                           2102
#define PID_GNB_PROC_TIMING_TIMEOUT             2103

#define PID_TIME_SYSTIME_POLL                   2104
#define PID_TIME_SYSTIME_STOP                   2105
#define PID_TIME_ARM_TIMER                      2106
#define PID_TIME_ARM_TIMER_DEADLINE             2107



#define PID_RADIO_FREQ_RX_PKT                   2400
#define PID_RADIO_RX_STOP                       2401
#define PID_RADIO_RX_UL_IQ                      2402
#define PID_RADIO_PRACH_PKT                     2403
#define PID_RADIO_FE_COMPRESS                   2404
#define PID_RADIO_FE_DECOMPRESS                 2405
#define PID_RADIO_TX_BYPASS_PROC                2406
#define PID_RADIO_ETH_TX_BURST                  2407
#define PID_RADIO_TX_DL_IQ                      2408
#define PID_RADIO_RX_VALIDATE                   2409

#define PID_RADIO_RX_IRQ_ON                     2410
#define PID_RADIO_RX_IRQ_OFF                    2411
#define PID_RADIO_RX_EPOLL_WAIT                 2412

#define PID_RADIO_TX_LTEMODE_PROC               2413
#define PID_RADIO_RX_LTEMODE_PROC               2414

#define PID_RADIO_TX_PLAY_BACK_IQ               2415

#define PID_PROCESS_TX_SYM                      2416

#define PID_CP_DL_CB                               2500
#define PID_CP_UL_CB                               2501
#define PID_UP_DL_CB                               2502
#define PID_SYM_OTA_CB                             2503
#define PID_TTI_CB_TO_PHY                          2504
#define PID_HALF_SLOT_CB_TO_PHY                    2505
#define PID_FULL_SLOT_CB_TO_PHY                    2506
#define PID_UP_UL_HALF_DEAD_LINE_CB                2507
#define PID_UP_UL_FULL_DEAD_LINE_CB                2508

#define PID_PROCESS_UP_PKT                      2600
#define PID_PROCESS_CP_PKT                      2700


#ifdef __cplusplus
}
#endif

#endif /* _XRAN_TASK_ID_H_ */

