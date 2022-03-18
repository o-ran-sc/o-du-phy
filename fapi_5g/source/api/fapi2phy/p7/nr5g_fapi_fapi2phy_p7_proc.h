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
 * to PHY P7 messages
 *
 **/

#ifndef _NR5G_FAPI_FAP2PHY_P7_PROC_H_
#define _NR5G_FAPI_FAP2PHY_P7_PROC_H_

uint8_t nr5g_fapi_dl_tti_request(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_dl_tti_req_t * p_fapi_req,
    fapi_vendor_msg_t * p_fapi_vendor_msg);

uint8_t nr5g_fapi_ul_tti_request(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_ul_tti_req_t * p_fapi_req,
    fapi_vendor_msg_t * p_fapi_vendor_msg);

uint8_t nr5g_fapi_ul_dci_request(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_ul_dci_req_t * p_fapi_req,
    fapi_vendor_msg_t * p_fapi_vendor_msg);

uint8_t nr5g_fapi_tx_data_request(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_tx_data_req_t * p_fapi_req,
    fapi_vendor_msg_t * p_fapi_vendor_msg);

#endif                          //_NR5G_FAPI_FAP2PHY_P7_PROC_H_
