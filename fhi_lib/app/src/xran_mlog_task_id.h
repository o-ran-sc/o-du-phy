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
//#define NR5G_SUBTASK_PROFILING_ON
//#define WLS_SUBTASK_ON

//--------------------------------------------------------------------
// MAC2PHY API PROC
//--------------------------------------------------------------------
#define PID_MAC2PHY_API_HANDLER                         1
#define PID_MAC2PHY_API_HANDLER_NULL                    2
#define PID_MAC2PHY_API_CHECK_LATE_API                  3
#define PID_MAC2PHY_API_RECV                            4
#define PID_MAC2PHY_API_RECV_NULL                       5
#define PID_MAC2PHY_API_CLEANUP                         6
#define PID_MAC2PHY_API_ERROR_CHECK                     7
#define PID_MAC2PHY_API_PARSE                           8
#define PID_MAC2PHY_TX_SDU_PROC                         9
#define PID_MAC2PHY_TX_VECTOR_PROC_DATA                 10
#define PID_MAC2PHY_TX_SDU_ZBC                          11
#define PID_MAC2PHY_RX_VECTOR_PROC                      12
#define PID_MAC2PHY_API_PROC                            13

//--------------------------------------------------------------------
// PHY2MAC API PROC
//--------------------------------------------------------------------
#define PID_PHY2MAC_API_PROC_PUSCH                      20
#define PID_PHY2MAC_API_PROC_PUCCH                      21
#define PID_PHY2MAC_API_PROC_UPDATE                     22
#define PID_PHY2MAC_API_PROC_SEND                       23
#define PID_PHY2MAC_URLLC_API_PROC_SEND                 24

//--------------------------------------------------------------------
// PHYSTATS
//--------------------------------------------------------------------
#define PID_PHYSTATS                                    30

//--------------------------------------------------------------------
// PHYDI
//--------------------------------------------------------------------
#define PID_PHYDI_IQ_COPY_DL                            35
#define PID_PHYDI_IQ_COPY_UL                            36
#define PID_PHYDI_IQ_COPY_DL_FRB                        37
#define PID_PHYDI_IQ_COPY_UL_FRB                        38
#define PID_PHYDI_IQ_COPY_PRACH_UL                      39
#define PID_PHYDI_IQ_COPY_SRS_UL                        40

//--------------------------------------------------------------------
// DISPATCH eBbuPool TASKS
//--------------------------------------------------------------------
#define PID_GNB_TTI_START_GEN_EXECUTE                   43
#define PID_GNB_SYM2_WAKEUP_GEN_EXECUTE                 44
#define PID_GNB_SYM6_WAKEUP_GEN_EXECUTE                 45
#define PID_GNB_SYM11_WAKEUP_GEN_EXECUTE                46
#define PID_GNB_SYM13_WAKEUP_GEN_EXECUTE                47
#define PID_GNB_PRACH_WAKEUP_GEN_EXECUTE                48
#define PID_GNB_SRS_WAKEUP_GEN_EXECUTE                  49

//--------------------------------------------------------------------
// POLLING
//--------------------------------------------------------------------
#define PID_AUX_BBDEV_DL_POLL                           50
#define PID_AUX_BBDEV_DL_POLL_DISPATCH                  51
#define PID_AUX_BBDEV_UL_POLL                           52
#define PID_AUX_BBDEV_UL_POLL_DISPATCH                  53

//--------------------------------------------------------------------
// WLS
//--------------------------------------------------------------------
#define PID_AUX_WLS_RX_PROCESS                          55
#define PID_AUX_WLS_SEND_API                            56
#define PID_AUX_WLS_ADD_TO_QUEUE                        57
#define PID_AUX_WLS_REMOVE_FROM_QUEUE                   58
#define PID_AUX_WLS_URLLC_RX_PROCESS                    59

//--------------------------------------------------------------------
// BBU-POOL-TASKS
//--------------------------------------------------------------------
#define PID_BBUPOOL_TTI_COMPLETE                        60
#define PID_BBUPOOL_TTI_COMPLETE_PRINT                  61
#define PID_BBUPOOL_TTI_TO_TTI_DURATION                 62

#define PID_BBUPOOL_ACTIVATE_CELL                       63
#define PID_BBUPOOL_DE_ACTIVATE_CELL                    64
#define PID_BBUPOOL_CREATE_EMPTY_LIST                   65
#define PID_BBUPOOL_RX_HANDLER                          66

//--------------------------------------------------------------------
// Timing Tasks
//--------------------------------------------------------------------
#define PID_GNB_PROC_TIMING                             70
#define PID_GNB_PROC_TIMING_TIMEOUT                     71
#define PID_GNB_TTI_START                               72
#define PID_GNB_SYM2_WAKEUP                             73
#define PID_GNB_SYM6_WAKEUP                             74
#define PID_GNB_SYM11_WAKEUP                            75
#define PID_GNB_SYM13_WAKEUP                            76
#define PID_GNB_PRACH_WAKEUP                            77
#define PID_GNB_SRS_WAKEUP                              78

//--------------------------------------------------------------------
// URLLC Tasks
//--------------------------------------------------------------------
#define PID_GNB_URLLC_DL_TASK                           80
#define PID_GNB_URLLC_DL_TOTAL_TASK                     81
#define PID_GNB_URLLC_UL_TASK                           82
#define PID_GNB_URLLC_UL_TOTAL_TASK                     83
#define PID_GNB_URLLC_TASK                              84
#define PID_GNB_URLLC_DL_CALL_BACK                      85
#define PID_GNB_URLLC_UL_CALL_BACK                      86
#define PID_GNB_URLLC_API_CALL_BACK                     87

//--------------------------------------------------------------------
// Latency Tasks  (Need 4 values (one per Numerology))
//--------------------------------------------------------------------
#define PID_GNB_DL_LINK_PRINT                           88
#define PID_GNB_UL_LINK_PRINT                           92
#define PID_GNB_SRS_LINK_PRINT                          96

//--------------------------------------------------------------------
// GNB UL BBU Tasks (there is gap of 24 for 24 cell support)
//--------------------------------------------------------------------
#define PCID_GNB_FH_RX_DATA_CC0                         100
#define PCID_GNB_FH_RX_SRS_CC0                          124
#define PCID_GNB_PUSCH_CE_SYMB0_CC0                     148
#define PCID_GNB_PUSCH_MMSE_SYMB0_CC0                   172
#define PCID_GNB_PUSCH_MMSE_SYMB7_CC0                   196
#define PCID_GNB_PUSCH_REDEMAP_SYMB0_CC0                220
#define PCID_GNB_PUSCH_REDEMAP_SYMB7_CC0                244
#define PCID_GNB_PUSCH_LAYDEMAP_SYMB0_CC0               268
#define PCID_GNB_PUSCH_LAYDEMAP_SYMB7_CC0               292
#define PCID_GNB_PUSCH_PN_SYMB0_CC0                     316
#define PCID_GNB_PUSCH_PN_SYMB7_CC0                     340
#define PCID_GNB_PUSCH_DEMOD_SYMB0_CC0                  364
#define PCID_GNB_PUSCH_DEMOD_SYMB7_CC0                  388
#define PCID_GNB_PUSCH_DESCRAMBLE_CC0                   412
#define PCID_GNB_PUSCH_DECODER_CC0                      436
#define PCID_GNB_PUSCH_TB_CC0                           460
#define PCID_GNB_UL_CFG_CC0                             484
#define PCID_GNB_PUSCH_DECODER_CB_CC0                   508
#define PCID_GNB_PUSCH_RX_SYMB0_CC0                     532
#define PCID_GNB_PUSCH_RX_SYMB7_CC0                     556
#define PCID_GNB_PRACH_PROCESS_CC0                      580
#define PCID_GNB_PUCCH_RX_CC0                           604
#define PCID_GNB_SRS_RX_CC0                             628
#define PCID_GNB_PUSCH_UCI_DECODER_CC0                  652
#define PCID_GNB_UL_POST_CC0                            676
#define PCID_GNB_UL_IQ_LOG_CC0                          700
#define PCID_GNB_FH_RX_PRACH_CC0                        724
#define PCID_GNB_PUSCH_RX_LINK_CC0                      748
#define PCID_GNB_UL_LINK_CC0                            772
#define PCID_GNB_PUSCH_CE_SYMB7_CC0                     796
#define PCID_GNB_SRS_RX_LINK_CC0                        820


#define PID_GNB_TASKLIST_NOT_COMPLETED                   899
//--------------------------------------------------------------------
// GNB DL BBU Tasks (there is gap of 24 for 24 cell support)
//--------------------------------------------------------------------
#define PCID_GNB_DL_CFG_CC0                             900
#define PCID_GNB_PDSCH_TB_CC0                           924
#define PCID_GNB_PDSCH_SCRAMBLER_CC0                    948
#define PCID_GNB_PDSCH_MOD_CC0                          972
#define PCID_GNB_PDSCH_PRECODE_CC0                      996
#define PCID_GNB_PDSCH_RS_CC0                           1020
#define PCID_GNB_PDSCH_REMAP_CC0                        1044
#define PCID_GNB_DL_RESET_BUF_CC0                       1068
#define PCID_GNB_DL_SYMBOL_PROC_CC0                     1092
#define PCID_GNB_DL_CSI_PROC_CC0                        1116
#define PCID_GNB_DL_DCI_PROC_CC0                        1140
#define PCID_GNB_DL_UCI_PROC_CC0                        1164
#define PCID_GNB_DL_PBCH_PROC_CC0                       1188
#define PCID_GNB_DL_POST_CC0                            1212
#define PCID_GNB_PDSCH_TB_QUEUE_CC0                     1236
#define PCID_GNB_DL_LINK_CC0                            1260
#define PCID_GNB_DL_DCI_PRECODER_CC0                    1284
#define PCID_GNB_PDSCH_TB_CRC_CC0                       1308
#define PCID_GNB_PDSCH_CB_SETUP_CC0                     1332

//--------------------------------------------------------------------
// Other DL / UL tasks (there is gap of 24 for 24 cell support)
//--------------------------------------------------------------------
#define PCID_GNB_PUSCH_TB_CRC_CC0                       1500
#define PCID_GNB_PUSCH_CB_SETUP_CC0                     1524
#define PCID_GNB_DL_BEAM_WEIGHT_TASK_CC0                1548
#define PCID_GNB_UL_BEAM_WEIGHT_TASK_CC0                1572
#define PCID_GNB_SRS_CE_CC0                             1596
#define PCID_GNB_SRS_REPORT_CC0                         1620
#define PCID_GNB_DL_BEAM_WEIGHT_COMPRESS_CC0            1644
#define PCID_GNB_UL_BEAM_WEIGHT_COMPRESS_CC0            1668
#define PCID_GNB_DL_IQ_COMPRESS_CC0                     1692
#define PCID_GNB_UL_IQ_DECOMPRESS_CC0                   1712
#define PCID_GNB_UL_IQ_FROM_XRAN_CC0                    1736
#define PCID_GNB_UL_IQ_SP_SLOT_FROM_XRAN_CC0            1760
#define PCID_GNB_UL_SRS_IQ_DECOMPRESS_CC0               1784
#define PCID_GNB_DL_OFDM_CTRL_COMPRESS_CC0              1808
#define PCID_GNB_DL_OFDM_RS_COMPRESS_CC0                1832
#define PCID_GNB_DL_OFDM_DATA_COMPRESS_CC0              1856

//--------------------------------------------------------------------
// GNB UL Sub Tasks
//--------------------------------------------------------------------
#define PID_GNB_PUCCH_F0_SEQ_GEN                        2000
#define PID_GNB_PUCCH_F0_DETECT                         2001
#define PID_GNB_PUCCH_F1_SEQ_GEN1                       2002
#define PID_GNB_PUCCH_F1_SEQ_GEN2                       2003
#define PID_GNB_PUCCH_F1_DESPRD                         2004
#define PID_GNB_PUCCH_F1_DEMOD                          2005
#define PID_GNB_PUCCH_F2_DMRS_GEN                       2006
#define PID_GNB_PUCCH_F2_CE                             2007
#define PID_GNB_PUCCH_F2_EQU                            2008
#define PID_GNB_PUCCH_F2_DEMOD                          2009
#define PID_GNB_PUCCH_F2_DESCR                          2010
#define PID_GNB_PUCCH_F2_DEC                            2011
#define PID_GNB_PUCCH_F3_F4_DMRS_GEN                    2012
#define PID_GNB_PUCCH_F3_F4_CE                          2013
#define PID_GNB_PUCCH_F3_F4_EQU                         2014
#define PID_GNB_PUCCH_F3_F4_IDFT                        2015
#define PID_GNB_PUCCH_F3_F4_DESPRD                      2016
#define PID_GNB_PUCCH_F3_F4_DEMOD                       2017
#define PID_GNB_PUCCH_F3_F4_DESCR                       2018
#define PID_GNB_PUCCH_F3_F4_DEC                         2019

//--------------------------------------------------------------------
// GNB UL BBU Creation Tasks
//--------------------------------------------------------------------
#define PID_GNB_PUSCH_DMRS0_GEN_BYPASS                  2700
#define PID_GNB_PUSCH_DMRS0_GEN_EXECUTE                 2701
#define PID_GNB_PUSCH_DMRS1_GEN_BYPASS                  2702
#define PID_GNB_PUSCH_DMRS1_GEN_EXECUTE                 2703
#define PID_GNB_PRACH_GEN_BYPASS                        2704
#define PID_GNB_PRACH_GEN_EXECUTE                       2705
#define PID_GNB_PUCCH_GEN_BYPASS                        2706
#define PID_GNB_PUCCH_GEN_EXECUTE                       2707
#define PID_GNB_SRS_GEN_BYPASS                          2708
#define PID_GNB_SRS_GEN_EXECUTE                         2709
#define PID_GNB_UL_CFG_GEN_BYPASS                       2710
#define PID_GNB_UL_CFG_GEN_EXECUTE                      2711
#define PID_GNB_PUSCH_TB_TASK_GEN_BYPASS                2712
#define PID_GNB_PUSCH_TB_TASK_GEN_EXECUTE               2713
#define PID_GNB_PUSCH_DECODE_TASK_GEN_BYPASS            2714
#define PID_GNB_PUSCH_DECODE_TASK_GEN_EXECUTE           2715
#define PID_GNB_PUSCH_DATA0_GEN_BYPASS                  2716
#define PID_GNB_PUSCH_DATA0_GEN_EXECUTE                 2717
#define PID_GNB_PUSCH_DATA1_GEN_BYPASS                  2718
#define PID_GNB_PUSCH_DATA1_GEN_EXECUTE                 2719

//--------------------------------------------------------------------
// GNB DL BBU Creation Tasks
//--------------------------------------------------------------------
#define PID_GNB_DL_SCRAMBLER_GEN_BYPASS                 2720
#define PID_GNB_DL_SCRAMBLER_GEN_EXECUTE                2721
#define PID_GNB_DL_CONFIG_GEN_BYPASS                    2722
#define PID_GNB_DL_CONFIG_GEN_EXECUTE                   2723
#define PID_GNB_DL_BEAM_GEN_BYPASS                      2724
#define PID_GNB_DL_BEAM_GEN_EXECUTE                     2725

//--------------------------------------------------------------------
// GNB Pre Tasks
//--------------------------------------------------------------------
#define PID_GNB_DL_PDSCH_SYMBOL_PRE_TASK                2730
#define PID_GNB_UL_PUSCH_CE0_PRE_TASK                   2731
#define PID_GNB_UL_PUSCH_CE7_PRE_TASK                   2732
#define PID_GNB_UL_PUSCH_MMSE0_PRE_TASK                 2733
#define PID_GNB_UL_PUSCH_MMSE7_PRE_TASK                 2734
#define PID_GNB_UL_PUCCH_PRE_TASK                       2735
#define PID_GNB_UL_SRS_PRE_TASK                         2736
#define PID_GNB_UL_PUSCH_LLR_RX_PRE_TASK                2737
#define PID_GNB_DL_BEAM_WEIGHT_PRE_TASK                 2738
#define PID_GNB_UL_BEAM_WEIGHT_PRE_TASK                 2739
#define PID_GNB_UL_PUSCH_DECODE_PRE_TASK                2740

//--------------------------------------------------------------------
// GNB Post Tasks
//--------------------------------------------------------------------
#define PID_GNB_UL_PUCCH_POST_TASK                      2745

//--------------------------------------------------------------------
// Other tasks
//--------------------------------------------------------------------
#define PID_GNB_DL_IFFT0                                2750
#define PID_GNB_DL_IFFT1                                2751
#define PID_GNB_DL_IFFT2                                2752
#define PID_GNB_DL_IFFT3                                2753
#define PID_GNB_DL_IFFT4                                2754
#define PID_GNB_DL_IFFT5                                2755
#define PID_GNB_DL_IFFT6                                2756
#define PID_GNB_DL_IFFT7                                2757
#define PID_GNB_UL_FFT0                                 2758
#define PID_GNB_UL_FFT1                                 2759
#define PID_GNB_UL_FFT2                                 2760
#define PID_GNB_UL_FFT3                                 2761
#define PID_GNB_UL_FFT4                                 2762
#define PID_GNB_UL_FFT5                                 2763
#define PID_GNB_UL_FFT6                                 2764
#define PID_GNB_UL_FFT7                                 2765

#define PID_DLIFFT                                      2766
#define PID_DLIFFT_ADD_CP                               2767
#define PID_ULFFT                                       2768

//--------------------------------------------------------------------
// AUX RADIO
//--------------------------------------------------------------------
#define PID_AUX_RADIO_RX_BYPASS_PROC                    2900
#define PID_AUX_RADIO_RX_STOP                           2901
#define PID_AUX_RADIO_RX_UL_IQ                          2902
#define PID_AUX_RADIO_PRACH_PKT                         2903
#define PID_AUX_RADIO_FE_COMPRESS                       2904
#define PID_AUX_RADIO_FE_DECOMPRESS                     2905
#define PID_AUX_RADIO_TX_BYPASS_PROC                    2906
#define PID_AUX_RADIO_ETH_TX_BURST                      2907
#define PID_AUX_RADIO_TX_DL_IQ                          2908
#define PID_AUX_RADIO_RX_VALIDATE                       2909
#define PID_AUX_RADIO_RX_IRQ_ON                         2910
#define PID_AUX_RADIO_RX_IRQ_OFF                        2911
#define PID_AUX_RADIO_RX_EPOLL_WAIT                     2912
#define PID_AUX_RADIO_TX_LTEMODE_PROC                   2913
#define PID_AUX_RADIO_RX_LTEMODE_PROC                   2914
#define PID_AUX_RADIO_TX_PLAY_BACK_IQ                   2915

#define PCID_BBUPOOL_RADIO_DL_COMPRESSION_TASK_CC0      2940
#define PCID_BBUPOOL_RADIO_DL_IQ_LOG_LTE_TASK_CC0       2960
#define PCID_BBUPOOL_RADIO_UL_IQ_LOG_LTE_TASK_CC0       2980

//--------------------------------------------------------------------
// XRAN
//--------------------------------------------------------------------
#define PID_XRAN_TTI_TIMER                              3100
#define PID_XRAN_TTI_CB                                 3101
#define PID_XRAN_SYM_TIMER                              3102
#define PID_XRAN_PROC_TIMING_TIMEOUT                    3103
#define PID_XRAN_TIME_SYSTIME_POLL                      3104
#define PID_XRAN_TIME_SYSTIME_STOP                      3105
#define PID_XRAN_TIME_ARM_TIMER                         3106

#define PID_XRAN_FREQ_RX_PKT                            3107
#define PID_XRAN_RX_STOP                                3108
#define PID_XRAN_RX_UL_IQ                               3109
#define PID_XRAN_PRACH_PKT                              3110
#define PID_XRAN_FE_COMPRESS                            3111
#define PID_XRAN_FE_DECOMPRESS                          3112
#define PID_XRAN_TX_BYPASS_PROC                         3113
#define PID_XRAN_ETH_TX_BURST                           3114
#define PID_XRAN_TX_DL_IQ                               3115
#define PID_XRAN_RX_VALIDATE                            3116
#define PID_XRAN_RX_IRQ_ON                              3117
#define PID_XRAN_RX_IRQ_OFF                             3118
#define PID_XRAN_RX_EPOLL_WAIT                          3119
#define PID_XRAN_TX_LTEMODE_PROC                        3120
#define PID_XRAN_RX_LTEMODE_PROC                        3121
#define PID_XRAN_TX_PLAY_BACK_IQ                        3122
#define PID_XRAN_PROCESS_TX_SYM                         3123
#define PID_XRAN_DISPATCH_TX_SYM                        3124
#define PID_XRAN_PREPARE_TX_PKT                         3125
#define PID_XRAN_ATTACH_EXT_BUF                         3126
#define PID_XRAN_ETH_ENQUEUE_BURST                      3127

#define PID_XRAN_CP_DL_CB                               3128
#define PID_XRAN_CP_UL_CB                               3129
#define PID_XRAN_UP_DL_CB                               3130
#define PID_XRAN_SYM_OTA_CB                             3131
#define PID_XRAN_TTI_CB_TO_PHY                          3132
#define PID_XRAN_HALF_SLOT_CB_TO_PHY                    3133
#define PID_XRAN_FULL_SLOT_CB_TO_PHY                    3134
#define PID_XRAN_UP_UL_HALF_DEAD_LINE_CB                3135
#define PID_XRAN_UP_UL_FULL_DEAD_LINE_CB                3136
#define PID_XRAN_UP_UL_USER_DEAD_LINE_CB                3137

#define PID_XRAN_PROCESS_UP_PKT                         3140
#define PID_XRAN_PROCESS_UP_PKT_SRS                     3141
#define PID_XRAN_PROCESS_UP_PKT_PARSE                   3142
#define PID_XRAN_PROCESS_CP_PKT                         3143
#define PID_XRAN_PROCESS_DELAY_MEAS_PKT                 3144

#define PID_XRAN_TIME_ARM_TIMER_DEADLINE                3150
#define PID_XRAN_TIME_ARM_USER_TIMER_DEADLINE           3151

#ifdef __cplusplus
}
#endif

#endif /* _XRAN_TASK_ID_H_ */

