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

#ifndef _NR5G_FAPI_FAP2MAC_P7_PVT_PROC_H_
#define _NR5G_FAPI_FAP2MAC_P7_PVT_PROC_H_

uint8_t nr5g_fapi_rach_indication_to_fapi_translation(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    PRXRACHIndicationStruct p_phy_rach_ind,
    fapi_rach_indication_t * p_fapi_rach_ind);

uint8_t nr5g_fapi_crc_indication_to_fapi_translation(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    PCRCIndicationStruct p_phy_crc_ind,
    fapi_crc_ind_t * p_fapi_crc_ind,
    fapi_vendor_ext_snr_t * p_fapi_snr);

uint8_t nr5g_fapi_rx_data_indication_to_fapi_translation(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    PRXULSCHIndicationStruct p_phy_rx_ulsch_ind,
    fapi_rx_data_indication_t * p_fapi_rx_data_ind);

uint8_t nr5g_fapi_rx_data_uci_indication_to_fapi_translation(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    PRXULSCHUCIIndicationStruct p_phy_rx_ulsch_uci_ind,
    fapi_uci_indication_t * p_fapi_uci_ind);

uint8_t nr5g_fapi_uci_indication_to_fapi_translation(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    PRXUCIIndicationStruct p_phy_uci_ind,
    fapi_uci_indication_t * p_fapi_uci_ind,
    fapi_vendor_ext_snr_t * p_fapi_snr);

uint8_t nr5g_fapi_srs_indication_to_fapi_translation(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    PRXSRSIndicationStruct p_phy_srs_ind,
    fapi_srs_indication_t * p_fapi_srs_ind,
    fapi_vendor_ext_srs_ind_t * p_fapi_vend_srs_ind);

nr5g_fapi_pusch_info_t *nr5g_fapi_get_pusch_info(
    uint16_t ue_id,
    nr5g_fapi_ul_slot_info_t * p_ul_slot_info);

nr5g_fapi_srs_info_t *nr5g_fapi_get_srs_info(
    uint16_t nUEId,
    nr5g_fapi_ul_slot_info_t * p_ul_slot_info);

#endif                          //_NR5G_FAPI_FAP2MAC_P7_PVT_PROC_H_
