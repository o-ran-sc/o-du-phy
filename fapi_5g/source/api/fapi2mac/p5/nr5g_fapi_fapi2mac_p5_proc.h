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
 * @file
 * This file consist of macros, structures and prototypes of all FAPI
 * to MAC P5 messages
 *
 **/

#ifndef _NR5G_FAPI_FAP2MAC_P5_PROC_H_
#define _NR5G_FAPI_FAP2MAC_P5_PROC_H_

#include "gnb_l1_l2_api.h"

uint8_t nr5g_fapi_message_header(
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    bool is_urllc);

uint8_t nr5g_fapi_message_header_per_phy(
    uint8_t phy_id,
    bool is_urllc);

uint8_t nr5g_fapi_config_response(
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    PCONFIGRESPONSEStruct p_iapi_resp);

uint8_t nr5g_fapi_stop_indication(
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    PSTOPRESPONSEStruct p_iapi_resp);

uint8_t nr5g_fapi_shutdown_response(
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    PSHUTDOWNRESPONSEStruct p_iapi_resp);

uint8_t nr5g_fapi_start_resp(
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    PSTARTRESPONSEStruct p_iapi_resp);

#ifdef DEBUG_MODE
uint8_t nr5g_fapi_dl_iq_samples_response(
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    PADD_REMOVE_BBU_CORES p_iapi_resp);

uint8_t nr5g_fapi_ul_iq_samples_response(
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    PADD_REMOVE_BBU_CORES p_iapi_resp);

uint8_t nr5g_fapi_message_header_for_ul_iq_samples(
    uint8_t phy_id);

#endif
#endif                          //_NR5G_FAPI_FAP2MAC_P5_PROC_H_
