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
 * to MAC P7 messages
 *
 **/

#ifndef _NR5G_FAPI_FAP2MAC_P7_PROC_H_
#define _NR5G_FAPI_FAP2MAC_P7_PROC_H_

typedef struct {
    p_fapi_api_queue_elem_t vendor_ext[FAPI_MAX_PHY_INSTANCES];
} fapi_api_stored_vendor_queue_elems,
*p_fapi_api_stored_vendor_queue_elems;

uint8_t nr5g_fapi_slot_indication(
    bool is_urllc,
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    p_fapi_api_stored_vendor_queue_elems vendor_extension_elems,
    PSlotIndicationStruct p_iapi_resp);
uint8_t nr5g_fapi_rach_indication(
    bool is_urllc,
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    PRXRACHIndicationStruct p_phy_rach_ind);
uint8_t nr5g_fapi_crc_indication(
    bool is_urllc,
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    p_fapi_api_stored_vendor_queue_elems vendor_extension_elems,
    PCRCIndicationStruct p_phy_crc_ind);
uint8_t nr5g_fapi_rx_data_indication(
    bool is_urllc,
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    p_fapi_api_stored_vendor_queue_elems vendor_extension_elems,
    PRXULSCHIndicationStruct p_phy_ulsch_ind);
uint8_t nr5g_fapi_rx_data_uci_indication(
    bool is_urllc,
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    PRXULSCHUCIIndicationStruct p_phy_rx_ulsch_uci_ind);
uint8_t nr5g_fapi_uci_indication(
    bool is_urllc,
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    p_fapi_api_stored_vendor_queue_elems vendor_extension_elems,
    PRXUCIIndicationStruct p_phy_uci_ind);
uint8_t nr5g_fapi_srs_indication(
    bool is_urllc,
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    p_fapi_api_stored_vendor_queue_elems vendor_extension_elems,
    PRXSRSIndicationStruct p_phy_srs_ind);
fapi_vendor_p7_ind_msg_t* nr5g_fapi_proc_vendor_p7_msg_get(
    p_fapi_api_stored_vendor_queue_elems vendor_extension_elems,
    uint8_t phy_id);
void nr5g_fapi_proc_vendor_p7_msgs_move_to_api_list(
    bool is_urllc,
    p_fapi_api_stored_vendor_queue_elems vendor_extension_elems);
#endif                          //_NR5G_FAPI_FAP2MAC_P7_PROC_H_
