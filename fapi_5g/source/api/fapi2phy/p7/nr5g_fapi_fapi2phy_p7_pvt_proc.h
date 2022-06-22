/******************************************************************************
*
*   Copyright (c) 2021 Intel.
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

#ifndef _NR5G_FAPI_FAP2PHY_P7_PVT_PROC_H_
#define _NR5G_FAPI_FAP2PHY_P7_PVT_PROC_H_

#include "nr5g_mac_phy_api.h"
#include "fapi_interface.h"
#include "fapi_vendor_extension.h"
#include "nr5g_fapi_framework.h"

#ifdef __cplusplus
extern "C" {
#endif

// DL/UL_TTI.req common
uint8_t nr5g_fapi_calc_n_rbg_size(
    uint16_t bwp_size);

uint32_t nr5g_fapi_calc_rbg_index(
    const uint8_t rb_bitmap[FAPI_RB_BITMAP_SIZE],
    uint16_t bwp_start,
    uint16_t bwp_size,
    uint32_t(*get_rbg_index_mask)(uint32_t nth_bit));

uint16_t nr5g_fapi_get_rb_bits_for_rbg(
    const uint8_t rb_bitmap[FAPI_RB_BITMAP_SIZE],
    uint32_t rbg_bit,
    uint8_t rbg_size,
    uint16_t rb_bitmap_mask);

// DL_TTI.req
uint8_t nr5g_fapi_dl_tti_req_to_phy_translation(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_dl_tti_req_t * p_fapi_req,
    fapi_vendor_msg_t * p_fapi_vendor_msg,
    PDLConfigRequestStruct p_ia_dl_config_req);

void nr5g_fapi_dl_tti_req_to_phy_translation_vendor_ext(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_vendor_msg_t * p_fapi_vendor_msg,
    PDLConfigRequestStruct p_ia_dl_config_req);

void nr5g_fapi_fill_dci_pdu(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_dl_pdcch_pdu_t * p_pdcch_pdu,
    PDCIPDUStruct p_dci_pdu);

void nr5g_fapi_fill_pdsch_pdu(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_dl_pdsch_pdu_t * p_pdsch_pdu,
    PDLSCHPDUStruct p_dlsch_pdu);

uint16_t nr5g_fapi_calculate_nEpreRatioOfPDCCHToSSB(
    uint8_t beta_pdcch_1_0);

uint16_t nr5g_fapi_calculate_nEpreRatioOfDmrsToSSB(
    uint8_t power_control_offset_ss);

uint16_t nr5g_fapi_calculate_nEpreRatioOfPDSCHToSSB(
    uint8_t power_control_offset);

uint32_t nr5g_fapi_calc_pdsch_rbg_index(
    const uint8_t rb_bitmap[FAPI_RB_BITMAP_SIZE],
    uint16_t bwp_start,
    uint16_t bwp_size);

void nr5g_fapi_fill_ssb_pdu(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    PBCHPDUStruct p_bch_pdu,
    fapi_dl_ssb_pdu_t * p_ssb_pdu);

void nr5g_fapi_fill_csi_rs_pdu(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_dl_csi_rs_pdu_t * p_csi_rs_pdu,
    PCSIRSPDUStruct pCSIRSPdu);

// UL_TTI.req
uint32_t nr5g_fapi_calc_pusch_rbg_index(
    const uint8_t rb_bitmap[FAPI_RB_BITMAP_SIZE],
    uint16_t bwp_start,
    uint16_t bwp_size);

void nr5g_fapi_pusch_data_to_phy_ulsch_translation(
    nr5g_fapi_pusch_info_t * p_pusch_info,
    fapi_pusch_data_t * p_pusch_data,
    ULSCHPDUStruct * p_ul_data_chan);

void nr5g_fapi_pusch_uci_to_phy_ulsch_translation(
    fapi_pusch_uci_t * p_pusch_uci,
    ULSCHPDUStruct * p_ul_data_chan);

void nr5g_fapi_pusch_ptrs_to_phy_ulsch_translation(
    fapi_pusch_ptrs_t * p_pusch_ptrs,
    ULSCHPDUStruct * p_ul_data_chan);

void nr5g_fapi_pusch_to_phy_ulsch_translation(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    nr5g_fapi_pusch_info_t * p_pusch_info,
    fapi_ul_pusch_pdu_t * p_pusch_pdu,
    ULSCHPDUStruct * p_ul_data_chan);

uint8_t nr5g_get_pucch_resources_group_id(
    uint8_t num_groups,
    uint16_t initial_cyclic_shift,
    uint8_t nr_of_symbols,
    uint8_t start_symbol_index,
    uint8_t time_domain_occ_idx,
    nr5g_fapi_pucch_resources_t * p_pucch_resources);

void nr5g_fapi_pucch_to_phy_ulcch_uci_translation(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    nr5g_fapi_pucch_info_t * p_pucch_info,
    fapi_ul_pucch_pdu_t * p_pucch_pdu,
    ULCCHUCIPDUStruct * p_ul_ctrl_chan);

void nr5g_fapi_srs_to_phy_srs_translation(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_ul_srs_pdu_t * p_srs_pdu,
    nr5g_fapi_srs_info_t * p_srs_info,
    SRSPDUStruct * p_ul_srs_chan);

uint8_t nr5g_fapi_ul_tti_req_to_phy_translation(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_ul_tti_req_t * p_fapi_req,
    fapi_vendor_msg_t * p_fapi_vendor_msg,
    PULConfigRequestStruct p_ia_ul_config_req);

void nr5g_fapi_ul_tti_req_to_phy_translation_vendor_ext(
    fapi_vendor_msg_t * p_fapi_vendor_msg,
    PULConfigRequestStruct p_ia_ul_config_req);

uint8_t nr5g_fapi_ul_tti_req_to_phy_translation_vendor_ext_symbol_no(
    bool is_urllc,
    fapi_vendor_msg_t * p_fapi_vendor_msg,
    PULConfigRequestStruct p_ia_ul_config_req,
    uint8_t* symbol_no);

uint8_t nr5g_fapi_ul_dci_req_to_phy_translation(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_ul_dci_req_t * p_fapi_req,
    PULDCIRequestStruct p_ia_ul_dci_req);

void nr5g_fapi_ul_dci_req_to_phy_translation_vendor_ext(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_vendor_msg_t * p_fapi_vendor_msg,
    PULDCIRequestStruct p_ia_ul_dci_req);

uint8_t nr5g_fapi_tx_data_req_to_phy_translation(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_tx_data_req_t * p_fapi_req,
    fapi_vendor_msg_t * p_fapi_vendor_msg,
    PTXRequestStruct p_ia_tx_req);

void nr5g_fapi_tx_data_req_to_phy_translation_vendor_ext(
    fapi_vendor_msg_t * p_fapi_vendor_msg,
    PTXRequestStruct p_phy_req);

#ifdef __cplusplus
}
#endif

#endif                          //_NR5G_FAPI_FAP2PHY_P7_PVT_PROC_H_
