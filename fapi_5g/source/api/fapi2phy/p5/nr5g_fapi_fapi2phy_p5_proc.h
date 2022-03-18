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
 * to PHY P5 messages
 *
 **/

#ifndef _NR5G_FAPI_FAP2PHY_P5_PROC_H_
#define _NR5G_FAPI_FAP2PHY_P5_PROC_H_

uint8_t nr5g_fapi_config_request(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_config_req_t * p_fapi_req,
    fapi_vendor_msg_t * p_fapi_vendor_msg);

uint8_t nr5g_fapi_start_request(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_start_req_t * p_fapi_req,
    fapi_vendor_msg_t * p_fapi_vendor_msg);

uint8_t nr5g_fapi_stop_request(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_stop_req_t * p_fapi_req,
    fapi_vendor_msg_t * p_fapi_vendor_msg);

uint8_t nr5g_fapi_shutdown_request(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_vendor_ext_shutdown_req_t * p_fapi_req);

#ifdef DEBUG_MODE
uint8_t nr5g_fapi_dl_iq_samples_request(
    bool is_urllc,
    fapi_vendor_ext_iq_samples_req_t * p_fapi_req);
uint8_t nr5g_fapi_ul_iq_samples_request(
    bool is_urllc,
    fapi_vendor_ext_iq_samples_req_t * p_fapi_req);
uint8_t nr5g_fapi_add_remove_core_message(
    bool is_urllc,
    fapi_vendor_ext_add_remove_core_msg_t * p_fapi_req);
#endif

#endif                          //_NR5G_FAPI_FAP2PHY_P5_PROC_H_
