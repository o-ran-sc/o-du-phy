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
 * @file This file consist of FAPI internal functions.
 *
 **/

#ifndef _NR5G_FAPI_FRAMEWORK_H_
#define _NR5G_FAPI_FRAMEWORK_H_

#include <fcntl.h>
#include "fapi_interface.h"
#include "nr5g_fapi_log.h"
#include "nr5g_fapi_internal.h"
#include "nr5g_fapi_std.h"
#include "nr5g_fapi_common_types.h"
#include "nr5g_fapi_config_loader.h"

// FAPI CONFIG.request parameters
typedef struct _nr5g_fapi_phy_config {
    uint16_t phy_cell_id;
    uint8_t n_nr_of_rx_ant;
    uint8_t use_vendor_EpreXSSB;
    uint8_t sub_c_common;
    uint8_t pad[3];
} nr5g_fapi_phy_config_t,
*pnr5g_fapi_phy_config_t;

typedef struct _nr5g_fapi_rach_info {
    uint16_t phy_cell_id;
} nr5g_fapi_rach_info_t;

typedef struct _nr5g_fapi_srs_info {
    uint32_t handle;
} nr5g_fapi_srs_info_t;

typedef struct _nr5g_fapi_pusch_info {
    uint32_t handle;
    uint8_t harq_process_id;
    uint8_t ul_cqi;
    uint16_t timing_advance;
} nr5g_fapi_pusch_info_t;

typedef struct _nr5g_fapi_pucch_info {
    uint32_t handle;
    uint8_t pucch_format;
} nr5g_fapi_pucch_info_t;

typedef struct _nr5g_fapi_ul_slot_info {
    uint16_t cookie;            //set this to frame_no at UL_TTI.Request and compare the 
    //same during uplink indications. 
    uint8_t slot_no;
    uint8_t symbol_no;
    uint8_t num_ulsch;
    uint8_t num_ulcch;
    uint8_t num_srs;
    uint8_t rach_presence;
    nr5g_fapi_rach_info_t rach_info;    //Only One RACH PDU will be reported for RACH.Indication message  
    nr5g_fapi_srs_info_t srs_info[FAPI_MAX_NUMBER_SRS_PDUS_PER_SLOT];
    nr5g_fapi_pucch_info_t pucch_info[FAPI_MAX_NUMBER_UCI_PDUS_PER_SLOT];
    nr5g_fapi_pusch_info_t pusch_info[FAPI_MAX_NUMBER_OF_ULSCH_PDUS_PER_SLOT];
} nr5g_fapi_ul_slot_info_t;

typedef struct _nr5g_fapi_stats_info_t {
    uint8_t fapi_param_req;
    uint8_t fapi_param_res;
    uint8_t fapi_config_req;
    uint8_t fapi_config_res;
    uint8_t fapi_start_req;
    uint8_t fapi_stop_req;
    uint8_t fapi_stop_ind;
    uint8_t fapi_vendor_msg;
    uint8_t fapi_vext_shutdown_req;
    uint8_t fapi_vext_shutdown_res;
#ifdef DEBUG_MODE
    uint8_t fapi_vext_start_res;
#endif
    uint64_t fapi_dl_tti_req;
    uint64_t fapi_ul_tti_req;
    uint64_t fapi_ul_dci_req;
    uint64_t fapi_tx_data_req;

    uint64_t fapi_slot_ind;
    uint64_t fapi_error_ind;
    uint64_t fapi_crc_ind;
    uint64_t fapi_rx_data_ind;
    uint64_t fapi_uci_ind;
    uint64_t fapi_srs_ind;
    uint64_t fapi_rach_ind;

    uint64_t fapi_dl_tti_pdus;
    uint64_t fapi_dl_tti_pdcch_pdus;
    uint64_t fapi_dl_tti_pdsch_pdus;
    uint64_t fapi_dl_tti_csi_rs_pdus;
    uint64_t fapi_dl_tti_ssb_pdus;

    uint64_t fapi_ul_dci_pdus;

    uint64_t fapi_ul_tti_pdus;
    uint64_t fapi_ul_tti_prach_pdus;
    uint64_t fapi_ul_tti_pusch_pdus;
    uint64_t fapi_ul_tti_pucch_pdus;
    uint64_t fapi_ul_tti_srs_pdus;
    uint64_t fapi_crc_ind_pdus;
    uint64_t fapi_rx_data_ind_pdus;
    uint64_t fapi_uci_ind_pdus;
    uint64_t fapi_srs_ind_pdus;
    uint64_t fapi_rach_ind_pdus;
} nr5g_fapi_stats_info_t;

typedef struct _nr5g_iapi_stats_info_t {
    uint8_t iapi_param_req;
    uint8_t iapi_param_res;
    uint8_t iapi_config_req;
    uint8_t iapi_config_res;
    uint8_t iapi_start_req;
    uint8_t iapi_start_res;
    uint8_t iapi_stop_req;
    uint8_t iapi_stop_ind;
    uint8_t iapi_shutdown_req;
    uint8_t iapi_shutdown_res;
    uint64_t iapi_dl_config_req;
    uint64_t iapi_ul_config_req;
    uint64_t iapi_ul_dci_req;
    uint64_t iapi_tx_req;

    uint64_t iapi_slot_ind;
    uint64_t iapi_error_ind;
    uint64_t iapi_crc_ind;
    uint64_t iapi_rx_data_ind;
    uint64_t iapi_uci_ind;
    uint64_t iapi_srs_ind;
    uint64_t iapi_rach_ind;

    uint64_t iapi_dl_tti_pdus;
    uint64_t iapi_dl_tti_pdcch_pdus;
    uint64_t iapi_dl_tti_pdsch_pdus;
    uint64_t iapi_dl_tti_csi_rs_pdus;
    uint64_t iapi_dl_tti_ssb_pdus;

    uint64_t iapi_ul_dci_pdus;

    uint64_t iapi_ul_tti_pdus;
    uint64_t iapi_ul_tti_prach_pdus;
    uint64_t iapi_ul_tti_pusch_pdus;
    uint64_t iapi_ul_tti_pucch_pdus;
    uint64_t iapi_ul_tti_srs_pdus;
    uint64_t iapi_crc_ind_pdus;
    uint64_t iapi_rx_data_ind_pdus;
    uint64_t iapi_uci_ind_pdus;
    uint64_t iapi_srs_ind_pdus;
    uint64_t iapi_rach_preambles;
} nr5g_iapi_stats_info_t;

typedef struct _nr5g_fapi_stats_t {
    nr5g_fapi_stats_info_t fapi_stats;
    nr5g_iapi_stats_info_t iapi_stats;
} nr5g_fapi_stats_t;

// FAPI phy instance structure
typedef struct _nr5g_fapi_phy_instance {
    uint8_t phy_id;
    uint8_t shutdown_retries;
    uint32_t shutdown_test_type;
    fapi_states_t state;        // FAPI state
    nr5g_fapi_phy_config_t phy_config;  // place holder to store,
    // parameters from config request
    nr5g_fapi_stats_t stats;
    nr5g_fapi_ul_slot_info_t ul_slot_info[FAPI_MAX_SLOT_INFO_URLLC][MAX_UL_SLOT_INFO_COUNT][MAX_UL_SYMBOL_INFO_COUNT];
} nr5g_fapi_phy_instance_t,
*p_nr5g_fapi_phy_instance_t;

// Phy Context
typedef struct _nr5g_fapi_phy_context {
    uint8_t num_phy_instance;
    uint8_t mac2phy_worker_core_id;
    uint8_t phy2mac_worker_core_id;
    uint8_t urllc_worker_core_id;
    pthread_t phy2mac_tid;
    pthread_t mac2phy_tid;
    pthread_t urllc_tid;
    sem_t urllc_sem_process;
    sem_t urllc_sem_done;
    volatile uint64_t process_exit;
    nr5g_fapi_phy_instance_t phy_instance[FAPI_MAX_PHY_INSTANCES];
} nr5g_fapi_phy_ctx_t,
*p_nr5g_fapi_phy_ctx_t;

// Function Declarations
inline p_nr5g_fapi_phy_ctx_t nr5g_fapi_get_nr5g_fapi_phy_ctx(
    );
uint8_t nr5g_fapi_framework_init(
    );
uint8_t nr5g_fapi_framework_stop(
    );
uint8_t nr5g_fapi_framework_finish(
    );
uint8_t nr5g_fapi_dpdk_init(
    p_nr5g_fapi_cfg_t cfg);
uint8_t nr5g_fapi_dpdk_wait(
    p_nr5g_fapi_cfg_t cfg);
void *nr5g_fapi_phy2mac_thread_func(
    void *config);
void *nr5g_fapi_mac2phy_thread_func(
    void *config);
void *nr5g_fapi_urllc_thread_func(
    void *config);
nr5g_fapi_ul_slot_info_t *nr5g_fapi_get_ul_slot_info(
    bool is_urllc,
    uint16_t frame_no,
    uint16_t slot_no,
    uint8_t symbol_no,
    p_nr5g_fapi_phy_instance_t p_phy_instance);
void nr5g_fapi_set_ul_slot_info(
    uint16_t frame_no,
    uint16_t slot_no,
    uint8_t symbol_no,
    nr5g_fapi_ul_slot_info_t * p_ul_slot_info);
#endif                          // _NR5G_FAPI_FRAMEWORK_H_
