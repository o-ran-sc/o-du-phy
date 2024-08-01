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
 * @brief This file has the System Debug Trace Logger (Mlog) Task IDs used by PHY
 * @file mlog_task_id.h
 * @ingroup group_source_xran
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
// XRAN APP
//--------------------------------------------------------------------

#define PID_GNB_PROC_TIMING                             70
#define PID_GNB_PROC_TIMING_TIMEOUT                     71
#define PID_GNB_PRACH_CB                                73
#define PID_GNB_SYM_CB                                  72
#define PID_GNB_SRS_CB                                  74
#define PID_GNB_BFW_CB                                  75
#define PID_GNB_CSIRS_CB                                76
//#define NR5G_SUBTASK_PROFILING_ON
//#define WLS_SUBTASK_ON

//--------------------------------------------------------------------
// MAC2PHY API PROC
//--------------------------------------------------------------------
#define PID_MAC2PHY_API_HANDLER                     1
#define PID_MAC2PHY_API_HANDLER_NULL                2
#define PID_MAC2PHY_API_CHECK_LATE_API              3
#define PID_MAC2PHY_API_RECV                        4
#define PID_MAC2PHY_API_RECV_NULL                   5
#define PID_MAC2PHY_API_CLEANUP                     6
#define PID_MAC2PHY_API_ERROR_CHECK                 7
#define PID_MAC2PHY_API_PARSE                       8
#define PID_MAC2PHY_LTE_TX_SDU_PROC                 9
#define PID_MAC2PHY_LTE_TX_SDU_COPY                 10
#define PID_MAC2PHY_LTE_TX_HI_SDU_COPY              11
#define PID_MAC2PHY_LTE_TX_VECTOR_PROC_DATA         12
#define PID_MAC2PHY_LTE_TX_VECTOR_PROC_CONTROL      13
#define PID_MAC2PHY_LTE_TX_SDU_ZBC                  14
#define PID_MAC2PHY_LTE_RX_VECTOR_PROC              15
#define PID_MAC2PHY_LTE_API_PROC                    16
#define PID_MAC2PHY_NR5G_TX_CONFIG                  17
#define PID_MAC2PHY_NR5G_TX_REQUEST                 18
#define PID_MAC2PHY_NR5G_RX_CONFIG                  19
#define PID_MAC2PHY_API_PROC_NR5G                   20

//--------------------------------------------------------------------
// PHY2MAC API PROC
//--------------------------------------------------------------------
#define PID_PHY2MAC_LTE_API_PROC_UPDATE             30
#define PID_PHY2MAC_LTE_API_PROC_SEND               31
#define PID_PHY2MAC_NR5G_API_PROC_SEND_NR5G         32
#define PID_PHY2MAC_NR5G_URLLC_API_PROC_SEND        33

//--------------------------------------------------------------------
// PHYDI
//--------------------------------------------------------------------
#define PID_PHYDI_IQ_COPY_DL                        35
#define PID_PHYDI_IQ_COPY_UL                        36
#define PID_PHYDI_IQ_COPY_PRACH_UL                  37
#define PID_PHYDI_IQ_COPY_SRS_UL                    38

//--------------------------------------------------------------------
// PHYSTATS
//--------------------------------------------------------------------
#define PID_PHYSTATS_LTE                            40
#define PID_PHYSTATS_NR5G                           41

//--------------------------------------------------------------------
// Infotrace tool Tasks
//--------------------------------------------------------------------
#define PID_INFO_TRACE_TRIGGER                      45
#define PID_INFO_TRACE_PARSE_DL                     46
#define PID_INFO_TRACE_PARSE_UL                     47

//--------------------------------------------------------------------
// POLLING
//--------------------------------------------------------------------
#define PID_AUX_BBDEV_NR5G_DL_POLL                  50
#define PID_AUX_BBDEV_NR5G_DL_POLL_DISPATCH         51
#define PID_AUX_BBDEV_NR5G_UL_POLL                  52
#define PID_AUX_BBDEV_NR5G_UL_POLL_DISPATCH         53
#define PID_AUX_BBDEV_NR5G_UL_SRS_FFT_POLL          54
#define PID_AUX_BBDEV_LTE_DL_POLL                   55
#define PID_AUX_BBDEV_LTE_DL_POLL_DISPATCH          56
#define PID_AUX_BBDEV_LTE_UL_POLL                   57
#define PID_AUX_BBDEV_LTE_UL_POLL_DISPATCH          58

//--------------------------------------------------------------------
// WLS
//--------------------------------------------------------------------
#define PID_AUX_WLS_RX_PROCESS                      60
#define PID_AUX_WLS_SEND_API                        61
#define PID_AUX_WLS_ADD_TO_QUEUE                    62
#define PID_AUX_WLS_REMOVE_FROM_QUEUE               63
#define PID_AUX_WLS_URLLC_RX_PROCESS                64

//--------------------------------------------------------------------
// Timing Tasks
//--------------------------------------------------------------------
#define PID_TIMING_TTI_COMPLETE                     70
#define PID_TIMING_TTI_COMPLETE_PRINT               71
#define PID_TIMING_TTI_TO_TTI_DURATION              72
#define PID_TIMING_RX_HANDLER                       73
#define PID_TIMING_TASKLIST_NOT_COMPLETED           74
#define PID_TIMING_PROC                             75
#define PID_TIMING_PROC_TIMEOUT                     76
#define PID_TIMING_TTI_START                        77
#define PID_TIMING_SYM2_WAKEUP                      78
#define PID_TIMING_SYM6_WAKEUP                      79
#define PID_TIMING_SYM11_WAKEUP                     80
#define PID_TIMING_SYM13_WAKEUP                     81
#define PID_TIMING_PRACH_WAKEUP                     82
#define PID_TIMING_SRS_WAKEUP                       83

//--------------------------------------------------------------------
// 5GNR URLLC Tasks
//--------------------------------------------------------------------
#define PID_NR5G_URLLC_API_TASK                     91
#define PID_NR5G_URLLC_DL_TASK                      92
#define PID_NR5G_URLLC_DL_TOTAL_TASK                93
#define PID_NR5G_URLLC_UL_TASK                      94
#define PID_NR5G_URLLC_UL_TOTAL_TASK                95
#define PID_NR5G_URLLC_TASK                         96
#define PID_NR5G_URLLC_DL_CALL_BACK                 97
#define PID_NR5G_URLLC_UL_CALL_BACK                 98
#define PID_NR5G_URLLC_API_CALL_BACK                99

//--------------------------------------------------------------------
// 5GNR BBU Tasks (there is gap of 30 for 30 cell support)
//--------------------------------------------------------------------
#define PCID_NR5G_UL_CFG_CC0                        100
#define PCID_NR5G_UL_PUSCH_CE_SYMB0_CC0             130
#define PCID_NR5G_UL_PUSCH_CE_SYMB7_CC0             160
#define PCID_NR5G_UL_PUSCH_MMSE_SYMB0_CC0           190
#define PCID_NR5G_UL_PUSCH_MMSE_SYMB7_CC0           220
#define PCID_NR5G_UL_PUSCH_REDEMAP_SYMB0_CC0        250
#define PCID_NR5G_UL_PUSCH_REDEMAP_SYMB7_CC0        280
#define PCID_NR5G_UL_PUSCH_LAYDEMAP_SYMB0_CC0       310
#define PCID_NR5G_UL_PUSCH_LAYDEMAP_SYMB7_CC0       340
#define PCID_NR5G_UL_PUSCH_PN_SYMB0_CC0             370
#define PCID_NR5G_UL_PUSCH_PN_SYMB7_CC0             400
#define PCID_NR5G_UL_PUSCH_DEMOD_SYMB0_CC0          430
#define PCID_NR5G_UL_PUSCH_DEMOD_SYMB7_CC0          460
#define PCID_NR5G_UL_PUSCH_DESCRAMBLE_CC0           490
#define PCID_NR5G_UL_PUSCH_DECODER_CC0              520
#define PCID_NR5G_UL_PUSCH_TB_CRC_CC0               550
#define PCID_NR5G_UL_PUSCH_CB_SETUP_CC0             580
#define PCID_NR5G_UL_PUSCH_TB_CC0                   610
#define PCID_NR5G_UL_PUSCH_UCI_DECODER_CC0          640
#define PCID_NR5G_UL_PUCCH_RX_CC0                   670
#define PCID_NR5G_UL_PUSCH_DECODER_CB_CC0           700
#define PCID_NR5G_UL_PUSCH_RX_SYMB0_CC0             730
#define PCID_NR5G_UL_PUSCH_RX_SYMB7_CC0             760
#define PCID_NR5G_UL_PRACH_PROCESS_CC0              790
#define PCID_NR5G_UL_SRS_RX_CC0                     820
#define PCID_NR5G_UL_SRS_CE_CC0                     850
#define PCID_NR5G_UL_SRS_REPORT_CC0                 880
#define PCID_NR5G_UL_SRS_FFT_CB_SETUP_CC0           910
#define PCID_NR5G_UL_SRS_FFT_CB_CC0                 940
#define PCID_NR5G_UL_SRS_CE_POST_CC0                970
#define PCID_NR5G_UL_IQ_LOG_CC0                     1000
#define PCID_NR5G_UL_POST_CC0                       1030
#define PCID_NR5G_DL_CFG_CC0                        1060
#define PCID_NR5G_DL_PDSCH_TB_CRC_CC0               1090
#define PCID_NR5G_DL_PDSCH_CB_SETUP_CC0             1120
#define PCID_NR5G_DL_PDSCH_TB_QUEUE_CC0             1150
#define PCID_NR5G_DL_PDSCH_TB_CC0                   1180
#define PCID_NR5G_DL_PDSCH_SCRAMBLER_CC0            1210
#define PCID_NR5G_DL_PDSCH_MOD_CC0                  1240
#define PCID_NR5G_DL_PDSCH_PRECODE_CC0              1270
#define PCID_NR5G_DL_PDSCH_RS_CC0                   1300
#define PCID_NR5G_DL_PDSCH_REMAP_CC0                1330
#define PCID_NR5G_DL_SYMBOL_PROC_CC0                1360
#define PCID_NR5G_DL_RESET_BUF_CC0                  1390
#define PCID_NR5G_DL_CSI_PROC_CC0                   1420
#define PCID_NR5G_DL_DCI_PROC_CC0                   1450
#define PCID_NR5G_DL_UCI_PROC_CC0                   1480
#define PCID_NR5G_DL_DCI_PRECODER_CC0               1510
#define PCID_NR5G_DL_PBCH_PROC_CC0                  1540
#define PCID_NR5G_DL_POST_CC0                       1570
#define PCID_NR5G_DL_ORAN_CC0                       1600
#define PCID_NR5G_DL_PDSCH_BEAM_FORMING_CC0         1630
#define PCID_NR5G_DL_BEAM_WEIGHT_TASK_CC0           1660
#define PCID_NR5G_UL_BEAM_WEIGHT_TASK_CC0           1690
#define PCID_NR5G_UL_PUCCH_BEAM_WEIGHT_TASK_CC0     1720
#define PCID_FH_DL_IQ_COMPRESS_CC0                  1750
#define PCID_FH_UL_IQ_DECOMPRESS_CC0                1780
#define PCID_FH_UL_SRS_IQ_DECOMPRESS_CC0            1810
#define PCID_FH_UL_IQ_DECOMP_PUSCH_CC0              1840
#define PCID_FH_UL_IQ_DECOMP_PUCCH_CC0              1870
#define PCID_FH_DL_OFDM_CTRL_COMPRESS_CC0           1900
#define PCID_FH_DL_OFDM_RS_COMPRESS_CC0             1930
#define PCID_FH_DL_OFDM_DATA_COMPRESS_CC0           1960
#define PCID_FH_RX_DATA_CC0                         1990
#define PCID_FH_RX_SRS_CC0                          2020
#define PCID_FH_RX_PRACH_CC0                        2050
#define PCID_FH_DL_BEAM_WEIGHT_COMPRESS_CC0         2080
#define PCID_FH_UL_BEAM_WEIGHT_COMPRESS_CC0         2110
#define PCID_FH_UL_PUCCH_BEAM_WEIGHT_COMPRESS_CC0   2140
#define PCID_FH_UL_TX_CC0                           2170


//--------------------------------------------------------------------
// LTE BBU Tasks (there is gap of 30 for 30 cell support)
//--------------------------------------------------------------------
#define PCID_LTE_API_TASK_CC0                       2300
#define PCID_LTE_DL_FEC_CRC_CC0                     2330
#define PCID_LTE_UL_PREAPRE_TASK_CC0                2360
#define PCID_LTE_UL_FEC_CC0                         2390
#define PCID_LTE_UL_PUSCH_CE_CC0                    2420
#define PCID_LTE_UL_DEMOD_CC0                       2450
#define PCID_LTE_ULFFT_TASK_CC0                     2480
#define PCID_LTE_DLIFFT_TASK_CC0                    2510
#define PCID_LTE_DLIFFT_POST_TASK_CC0               2540
#define PCID_LTE_UL_FEC_PUSCH_TB_CRC_CHECK_CC0      2570
#define PCID_LTE_DL_FEC_RM_TURBO_SCR_CC0            2600
#define PCID_LTE_DL_MOD_CC0                         2630
#define PCID_LTE_UL_PUCCH_CC0                       2660
#define PCID_LTE_UL_SRS_CC0                         2690
#define PCID_LTE_UL_API_SEND_CC0                    2720
#define PCID_LTE_UL_PRACH_CC0                       2750
#define PCID_LTE_UL_POST_CC0                        2780
#define PCID_LTE_DL_OPT_TASK1_CC0                   2810
#define PCID_LTE_DL_OPT_TASK2_CC0                   2840
#define PCID_LTE_UL_OPT_TASK1_CC0                   2870
#define PCID_LTE_UL_OPT_TASK2_CC0                   2900

//--------------------------------------------------------------------
// DL, UL and SRS Links (there is gap of 30 for 30 cell support)
//--------------------------------------------------------------------
#define PCID_DL_FEC_LINK_CC0                        2930
#define PCID_UL_FEC_LINK_CC0                        2960
#define PCID_DL_LINK_CC0                            2990
#define PCID_UL_LINK_CC0                            2020
#define PCID_UL_SRS_RX_LINK_CC0                     3050

//--------------------------------------------------------------------
// Latency Tasks  (Need 5 values each (one per Numerology))
//--------------------------------------------------------------------
#define PID_DL_LINK_PRINT                           3100
#define PID_UL_LINK_PRINT                           3105
#define PID_SRS_LINK_PRINT                          3110

//--------------------------------------------------------------------
// BBU-POOL-TASKS
//--------------------------------------------------------------------
#define PID_BBUPOOL_PRE_LTE_DLIFFT                  3220
#define PID_BBUPOOL_PRE_LTE_DLTURSCR                3221
#define PID_BBUPOOL_PRE_LTE_ULFEC                   3222
#define PID_BBUPOOL_PRE_NR5G_DL_PDSCH_SYMBOL        3223
#define PID_BBUPOOL_PRE_NR5G_DL_BEAM_WGHT           3224
#define PID_BBUPOOL_PRE_NR5G_UL_BEAM_WGHT           3225
#define PID_BBUPOOL_PRE_NR5G_UL_PUSCH_CE0           3226
#define PID_BBUPOOL_PRE_NR5G_UL_PUSCH_CE7           3227
#define PID_BBUPOOL_PRE_NR5G_UL_PUSCH_MMSE0         3228
#define PID_BBUPOOL_PRE_NR5G_UL_PUSCH_MMSE7         3229
#define PID_BBUPOOL_PRE_NR5G_UL_PUSCH_LLR_DEMAP     3230
#define PID_BBUPOOL_PRE_NR5G_UL_PUSCH_DECODE        3231
#define PID_BBUPOOL_PRE_NR5G_UL_PUCCH               3232
#define PID_BBUPOOL_PRE_NR5G_UL_PUCCH_BEAM_WGHT     3233
#define PID_BBUPOOL_PRE_NR5G_UL_SRS                 3234

#define PID_BBUPOOL_POST_NR5G_UL_PUCCH              3235

#define PID_BBUPOOL_GEN_DL_CONFIG                   3236
#define PID_BBUPOOL_GEN_DL_SCRAMBLER                3237
#define PID_BBUPOOL_GEN_UL_CONFIG                   3238
#define PID_BBUPOOL_GEN_SRS_FFT                     3239
#define PID_BBUPOOL_GEN_PUSCH_CE0                   3240
#define PID_BBUPOOL_GEN_PUSCH_CE7                   3241
#define PID_BBUPOOL_GEN_PRACH                       3242
#define PID_BBUPOOL_GEN_PUCCH                       3243
#define PID_BBUPOOL_GEN_SRS                         3244
#define PID_BBUPOOL_GEN_UL_PUSCH_TB                 3245
#define PID_BBUPOOL_GEN_UL_PUSCH_DECODE             3246
#define PID_BBUPOOL_GEN_UL_PUSCH_MMSE0              3247
#define PID_BBUPOOL_GEN_UL_PUSCH_MMSE7              3248
#define PID_BBUPOOL_GEN_DL_BEAM_WGHT                3249
#define PID_BBUPOOL_GEN_TIMING_TTI_START            3250
#define PID_BBUPOOL_GEN_TIMING_SYM2                 3251
#define PID_BBUPOOL_GEN_TIMING_SYM6                 3252
#define PID_BBUPOOL_GEN_TIMING_SYM11                3253
#define PID_BBUPOOL_GEN_TIMING_SYM13                3254
#define PID_BBUPOOL_GEN_TIMING_PRACH                3255
#define PID_BBUPOOL_GEN_TIMING_SRS                  3256

#define PID_BBUPOOL_BYPASS_UL_PUSCH_TB              3257
#define PID_BBUPOOL_BYPASS_DL_SCRAMBLER             3258

//--------------------------------------------------------------------
// XRAN
//--------------------------------------------------------------------
#define PID_XRAN_TTI_TIMER                          3300
#define PID_XRAN_TTI_CB                             3301
#define PID_XRAN_SYM_TIMER                          3302
#define PID_XRAN_PROC_TIMING_TIMEOUT                3303
#define PID_XRAN_TIME_SYSTIME_POLL                  3304
#define PID_XRAN_TIME_SYSTIME_STOP                  3305
#define PID_XRAN_TIME_ARM_TIMER                     3306
#define PID_XRAN_FREQ_RX_PKT                        3307
#define PID_XRAN_RX_STOP                            3308
#define PID_XRAN_RX_UL_IQ                           3309
#define PID_XRAN_PRACH_PKT                          3310
#define PID_XRAN_FE_COMPRESS                        3311
#define PID_XRAN_FE_DECOMPRESS                      3312
#define PID_XRAN_TX_BYPASS_PROC                     3313
#define PID_XRAN_ETH_TX_BURST                       3314
#define PID_XRAN_TX_DL_IQ                           3315
#define PID_XRAN_RX_VALIDATE                        3316
#define PID_XRAN_RX_IRQ_ON                          3317
#define PID_XRAN_RX_IRQ_OFF                         3318
#define PID_XRAN_RX_EPOLL_WAIT                      3319
#define PID_XRAN_TX_LTEMODE_PROC                    3320
#define PID_XRAN_RX_LTEMODE_PROC                    3321
#define PID_XRAN_TX_PLAY_BACK_IQ                    3322
#define PID_XRAN_PROCESS_TX_SYM                     3323
#define PID_XRAN_DISPATCH_TX_SYM                    3324
#define PID_XRAN_PREPARE_TX_PKT                     3325
#define PID_XRAN_ATTACH_EXT_BUF                     3326
#define PID_XRAN_ETH_ENQUEUE_BURST                  3327
#define PID_XRAN_CP_DL_CB                           3328
#define PID_XRAN_CP_UL_CB                           3329
#define PID_XRAN_UP_DL_CB                           3330
#define PID_XRAN_SYM_OTA_CB                         3331
#define PID_XRAN_TTI_CB_TO_PHY                      3332
#define PID_XRAN_HALF_SLOT_CB_TO_PHY                3333
#define PID_XRAN_FULL_SLOT_CB_TO_PHY                3334
#define PID_XRAN_UP_UL_HALF_DEAD_LINE_CB            3335
#define PID_XRAN_UP_UL_FULL_DEAD_LINE_CB            3336
#define PID_XRAN_UP_UL_USER_DEAD_LINE_CB            3337
#define PID_XRAN_PROCESS_UP_PKT                     3338
#define PID_XRAN_PROCESS_UP_PKT_SRS                 3339
#define PID_XRAN_PROCESS_UP_PKT_PARSE               3340
#define PID_XRAN_PROCESS_CP_PKT                     3341
#define PID_XRAN_PROCESS_DELAY_MEAS_PKT             3342
#define PID_XRAN_TIME_ARM_TIMER_DEADLINE            3343
#define PID_XRAN_TIME_ARM_USER_TIMER_DEADLINE       3344
#define PID_XRAN_UL_DECOMPRESS                      3345
#define PID_XRAN_DL_DECOMPRESS                      3346
#define PID_XRAN_SRS_DECOMPRESS                     3347
#define PID_XRAN_PRACH_DECOMPRESS                   3348
#define PID_XRAN_CSIRS_DECOMPRESS                   3349


//--------------------------------------------------------------------
// LTE SUBTASKS
//--------------------------------------------------------------------
#define PID_SUBLTE_DL_FEC_CRC                       3350
#define PID_SUBLTE_DL_FEC_TURBOENCODER_RMATCHING    3351
#define PID_SUBLTE_DL_FEC_SCRAMBLER                 3352
#define PID_SUBLTE_DL_FEC_CRCA                      3353
#define PID_SUBLTE_DL_FEC_CRCB                      3354
#define PID_SUBLTE_DL_FEC_TURBOENCODERSDK           3355
#define PID_SUBLTE_DL_FEC_RATEMATCHINGSDK           3356
#define PID_SUBLTE_DL_MOD_CONTROL_SYM               3357
#define PID_SUBLTE_DL_MOD_DATA_SYM                  3358
#define PID_SUBLTE_DL_MOD_RES_ELEM_MAPPER           3359
#define PID_SUBLTE_DL_MOD_SETUP_SYM_BUFS            3360
#define PID_SUBLTE_DL_MOD_DLIFFT_SETUP              3361
#define PID_SUBLTE_DL_MOD_MAPPER                    3362
#define PID_SUBLTE_DL_MOD_LAYER_MAPPER_PRECODER     3363
#define PID_SUBLTE_DL_MOD_PILOTS                    3364
#define PID_SUBLTE_DL_MOD_PILOTS_POS                3365
#define PID_SUBLTE_DL_MOD_SYNC_SIGNALS              3366
#define PID_SUBLTE_DL_MOD_PILOTS_UE_S               3367
#define PID_SUBLTE_DL_MOD_MEMSET                    3368
#define PID_SUBLTE_DL_MOD_PILOTS_CSI                3369
#define PID_SUBLTE_DL_MOD_PRECODER                  3370
#define PID_SUBLTE_DL_MOD_SETUP_SYM_BUFS_MPDCCH     3371
#define PID_SUBLTE_DL_MOD_MAPPER_MPDCCH             3372
#define PID_SUBLTE_DL_MOD_LMAPPER_PRECODER_MPDCCH   3373
#define PID_SUBLTE_DL_IQ_SETUP                      3374
#define PID_SUBLTE_DL_SDU_SETUP                     3375
#define PID_SUBLTE_DL_PDSCH_FEC_ENQUEUE             3376
#define PID_SUBLTE_DL_PDSCH_POST_FEC                3377
#define PID_SUBLTE_DL_MODULATION_MAPPER             3378
#define PID_SUBLTE_UL_DEMOD_PROC                    3379
#define PID_SUBLTE_UL_DEMOD_PUSCH_CHEST_PART0       3380
#define PID_SUBLTE_UL_DEMOD_PUSCH_CHEST_PART1       3381
#define PID_SUBLTE_UL_DEMOD_PUSCH_CHEST_PART2       3382
#define PID_SUBLTE_UL_DEMOD_PUSCH_RSSI              3383
#define PID_SUBLTE_UL_DEMOD_PUSCH_SNR               3384
#define PID_SUBLTE_UL_DEMOD_PUSCH_MRC               3385
#define PID_SUBLTE_UL_DEMOD_PUSCH_INVDFT_DEMAPPER   3386
#define PID_SUBLTE_UL_DEMOD_MULTIPLEX_PUSCH         3387
#define PID_SUBLTE_UL_DEMOD_FLOAT_TO_FIX            3388
#define PID_SUBLTE_UL_DEMOD_PUSCH_INVDFT            3389
#define PID_SUBLTE_UL_DEMOD_PUSCH_DEMAPPER          3390
#define PID_SUBLTE_UL_DEMOD_PUSCH_PRE_INVDFT_SCALE  3391
#define PID_SUBLTE_UL_DEMOD_PUSCH_POST_INVDFT_SCALE 3392
#define PID_SUBLTE_UL_DEMOD_PUSCH_SIG_LEVEL         3393
#define PID_SUBLTE_UL_DEMOD_PUSCH_NOISE_DEMOD       3394
#define PID_SUBLTE_UL_DEMOD_PUSCH_MRC_MU_MIMO       3395
#define PID_SUBLTE_UL_DEMOD_PUSCH_SNR_MU_MIMO       3396
#define PID_SUBLTE_UL_DEMOD_PUSCH_CEST_MUMIMO_PRT0  3397
#define PID_SUBLTE_UL_DEMOD_MU_MIMO_MATRIX_INVERSE  3398
#define PID_SUBLTE_UL_DEMAP_PROC                    3399
#define PID_SUBLTE_UL_DEMOD_PUSCH_NOISE_EST_MU_MIMO 3400
#define PID_SUBLTE_UL_DEMOD_PUSCH_MU_MIMO_RSSI      3401
#define PID_SUBLTE_UL_DEMOD_PUSCH_SETUP             3402
#define PID_SUBLTE_UL_DEMOD_PUSCH_USR               3403
#define PID_SUBLTE_UL_DEMOD_PUSCH_MU_MIMO_USR       3404
#define PID_SUBLTE_UL_DEMOD_PUSCH_MUMIMO_NOISE_POW  3405
#define PID_SUBLTE_UL_DEMOD_PUSCH_MU_MIMO_MATRIX    3406
#define PID_SUBLTE_UL_DEMOD_PUSCH_MU_MIMO_CALC_G    3407
#define PID_SUBLTE_UL_DEMOD_PUCCH_CHAN              3408
#define PID_SUBLTE_UL_DEMOD_PUCCH_CHAN_EST          3409
#define PID_SUBLTE_UL_DEMOD_PUCCH                   3410
#define PID_SUBLTE_UL_PRACH_PEAK_SEARCH_PROC        3411
#define PID_SUBLTE_UL_PRACH_PEAK_SEARCH_PROC_1      3412
#define PID_SUBLTE_UL_PRACH_PEAK_SEARCH_PROC_2      3413
#define PID_SUBLTE_UL_PRACH_PEAK_SEARCH_PROC_3      3414
#define PID_SUBLTE_UL_PRACH_PEAK_SEARCH_PROC_4      3415
#define PID_SUBLTE_UL_PRACH_PEAK_SEARCH_PROC_5      3416
#define PID_SUBLTE_UL_PRACH_INIT                    3417
#define PID_SUBLTE_UL_PRACH_DWN_SAMPLING            3418
#define PID_SUBLTE_UL_PRACH_FFT                     3419
#define PID_SUBLTE_UL_PRACH_COPY_CORR               3420
#define PID_SUBLTE_UL_PRACH_DEMOD                   3421
#define PID_SUBLTE_UL_PRACH_DWN_STAGE1              3422
#define PID_SUBLTE_UL_PRACH_DWN_STAGE2              3423
#define PID_SUBLTE_UL_PRACH_DWN_STAGE3              3424
#define PID_SUBLTE_UL_DEMOD_SRS                     3425
#define PID_SUBLTE_UL_DEMOD_SRS_CHAN_EST            3426
#define PID_SUBLTE_UL_DEMOD_SRS_BEAMFORMING_DOA_EST 3427
#define PID_SUBLTE_UL_DEMOD_SRS_SNR                 3428
#define PID_SUBLTE_UL_MSRM_RIP_UNUSED_RB            3429
#define PID_SUBLTE_UL_MSRM_RIP_PUSCH                3430
#define PID_SUBLTE_UL_MSRM_RIP_PUCCH                3431
#define PID_SUBLTE_UL_FEC_PUSCH_DESCRAMBLER         3432
#define PID_SUBLTE_UL_FEC_PUSCH_DEINTERL            3433
#define PID_SUBLTE_UL_FEC_PUSCH_COPY                3434
#define PID_SUBLTE_UL_FEC_PUSCH_CODE_BLOCK          3435
#define PID_SUBLTE_UL_FEC_PUSCH_PREP                3436
#define PID_SUBLTE_UL_FEC_PUSCH_HARQ_COMB           3437
#define PID_SUBLTE_UL_FEC_PUSCH_RATE_DEMATCHING     3438
#define PID_SUBLTE_UL_FEC_PUSCH_TB_CRC_CHECK        3439
#define PID_SUBLTE_UL_FEC_PUSCH_FEC_DECODER         3440
#define PID_SUBLTE_UL_FEC_PUSCH_VITERBI_DECODER     3441
#define PID_SUBLTE_UL_FEC_RM_DECODER                3442
#define PID_SUBLTE_UL_FEC_RM_FHT_DECODER            3443
#define PID_SUBLTE_UL_FEC_RM_DECODER_CONF           3444
#define PID_SUBLTE_UL_FEC_CQI_EXTRACTION            3445
#define PID_SUBLTE_UL_IQ_SETUP                      3446
#define PID_SUBLTE_UL_PUSCH_PROC                    3447
#define PID_SUBLTE_UL_PUSCH_ENQUEUE                 3448
#define PID_SUBLTE_UL_PUCCH_PROC                    3449
#define PID_SUBLTE_UL_PRACH_PROC                    3450
#define PID_SUBLTE_UL_SRS_PROC                      3451
#define PID_SUBLTE_UL_PUSCH_POST_FEC                3452
#define PID_SUBLTE_UL_SEND_API                      3453

//--------------------------------------------------------------------
// 5GNR SUBTASKS
//--------------------------------------------------------------------
#define PID_SUBNR5G_PUCCH_F0_SEQ_GEN                3475
#define PID_SUBNR5G_PUCCH_F0_DETECT                 3476
#define PID_SUBNR5G_PUCCH_F1_SEQ_GEN1               3477
#define PID_SUBNR5G_PUCCH_F1_SEQ_GEN2               3478
#define PID_SUBNR5G_PUCCH_F1_DESPRD                 3479
#define PID_SUBNR5G_PUCCH_F1_DEMOD                  3480
#define PID_SUBNR5G_PUCCH_F2_DMRS_GEN               3481
#define PID_SUBNR5G_PUCCH_F2_CE                     3482
#define PID_SUBNR5G_PUCCH_F2_EQU                    3483
#define PID_SUBNR5G_PUCCH_F2_DEMOD                  3484
#define PID_SUBNR5G_PUCCH_F2_DESCR                  3485
#define PID_SUBNR5G_PUCCH_F2_DEC                    3486
#define PID_SUBNR5G_PUCCH_F3_F4_DMRS_GEN            3487
#define PID_SUBNR5G_PUCCH_F3_F4_CE                  3488
#define PID_SUBNR5G_PUCCH_F3_F4_EQU                 3489
#define PID_SUBNR5G_PUCCH_F3_F4_IDFT                3490
#define PID_SUBNR5G_PUCCH_F3_F4_DESPRD              3491
#define PID_SUBNR5G_PUCCH_F3_F4_DEMOD               3492
#define PID_SUBNR5G_PUCCH_F3_F4_DESCR               3493
#define PID_SUBNR5G_PUCCH_F3_F4_DEC                 3494
#define PID_SUBNR5G_PRACH_AGC                       3495
#define PID_SUBNR5G_PRACH_IFFT                      3496
#define PID_SUBNR5G_PRACH_AGC_ALIGN                 3497
#define PID_SUBNR5G_SRS_FFT_CB_SETUP                3498

//--------------------------------------------------------------------
// COMMON SUBTASKS
//--------------------------------------------------------------------
#define PID_SUB_DL_IFFT                             3500
#define PID_SUB_DL_IFFT_ADD_CP                      3501
#define PID_SUB_UL_FFT                              3502

#ifdef __cplusplus
}
#endif

#endif /* _XRAN_TASK_ID_H_ */

