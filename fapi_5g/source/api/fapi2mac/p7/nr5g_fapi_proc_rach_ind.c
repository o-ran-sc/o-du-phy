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
 * This file consist of implementation of FAPI RACH.indication message.
 *
 **/

#include "nr5g_fapi_framework.h"
#include "gnb_l1_l2_api.h"
#include "nr5g_fapi_fapi2mac_api.h"
#include "nr5g_fapi_fapi2mac_p7_proc.h"
#include "nr5g_fapi_fapi2mac_p7_pvt_proc.h"

 /** @ingroup group_source_api_p7_fapi2mac_proc
 *
 *  @param[in]  p_phy_ctx Pointer to PHY Context.
 *  @param[in]  p_phy_rach_ind Pointer to FAPI RACH.indication message structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This message includes RACH PDU. 
 *
**/
uint8_t nr5g_fapi_rach_indication(
    bool is_urllc,
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    PRXRACHIndicationStruct p_phy_rach_ind)
{
    uint8_t phy_id;

    fapi_rach_indication_t *p_fapi_rach_ind;
    p_fapi_api_queue_elem_t p_list_elem;
    p_nr5g_fapi_phy_instance_t p_phy_instance = NULL;
    nr5g_fapi_stats_t *p_stats;

    if (NULL == p_phy_ctx) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[RACH.indication] Invalid " "phy context"));
        return FAILURE;
    }

    if (NULL == p_phy_rach_ind) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[RACH.indication] Invalid handle to phy "
                "RACH indication"));
        return FAILURE;
    }

    phy_id = p_phy_rach_ind->sSFN_Slot.nCarrierIdx;
    p_phy_instance = &p_phy_ctx->phy_instance[phy_id];
    if ((p_phy_instance->phy_id != phy_id)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[RACH.indication] Invalid " "phy instance"));
        return FAILURE;
    }

    p_stats = &p_phy_instance->stats;
    p_stats->iapi_stats.iapi_rach_ind++;

    p_list_elem =
        nr5g_fapi_fapi2mac_create_api_list_elem(FAPI_RACH_INDICATION, 1,
        sizeof(fapi_rach_indication_t));

    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[RACH.indication] Unable to create "
                "list element. Out of memory!!!"));
        return FAILURE;
    }

    p_fapi_rach_ind = (fapi_rach_indication_t *) (p_list_elem + 1);
    p_fapi_rach_ind->header.msg_id = FAPI_RACH_INDICATION;
    p_fapi_rach_ind->header.length = (uint16_t) sizeof(fapi_rach_indication_t);

    if (nr5g_fapi_rach_indication_to_fapi_translation(is_urllc, p_phy_instance,
            p_phy_rach_ind, p_fapi_rach_ind)) {
        NR5G_FAPI_LOG(ERROR_LOG,
            ("[RACH.indication] L1 to FAPI " "translation failed"));
        return FAILURE;
    }

    nr5g_fapi_fapi2mac_add_api_to_list(phy_id, p_list_elem, false);

    p_stats->fapi_stats.fapi_rach_ind++;
    NR5G_FAPI_LOG(DEBUG_LOG, ("[RACH.indication][%u][%u,%u,%u] is_urllc %u",
            p_phy_instance->phy_id,
            p_phy_rach_ind->sSFN_Slot.nSFN, p_phy_rach_ind->sSFN_Slot.nSlot,
            p_phy_rach_ind->sSFN_Slot.nSym, is_urllc));

    return SUCCESS;
}

 /** @ingroup group_source_api_p7_fapi2mac_proc
 *
 *  @param[in]   slot_index Variable holding nStartSlotdx received in IAPI RACH.Indication.
 *  @param[in]   freq_index Variable holding nFreqIdx received in IAPI RACH.Indication.
 *  @param[in]   symbol_index Variable holding nSymbolIdx received in IAPI RACH.Indication.
 *  @param[in]   num_pdus Variable holding the num_pdus filled in FAPI RACH.Indication till then.
 *  @param[in]   p_fapi_rach_ind Pointer to FAPI RACH.indication structure.
 *  
 *  @return     Returns pdu_index at which slot_index and freq_index match occurs
 *                         
 *
 *  @description
 *  This function returns the pdu_index at which slot_index and freq_index match
 *  occurs in FAPI RACH.Indication populated till then
 *
**/
uint8_t nr5g_fapi_start_slot_freq_idx_occ(
    uint8_t slot_index,
    uint8_t freq_index,
    uint8_t symbol_index,
    uint8_t num_pdus,
    fapi_rach_indication_t * p_fapi_rach_ind)
{
    uint8_t i, pdu_index = 0xFF;

    fapi_rach_pdu_t *p_fapi_rach_pdu;

    for (i = 0; i < num_pdus; i++) {
        p_fapi_rach_pdu = &p_fapi_rach_ind->rachPdu[i];
        if ((slot_index == p_fapi_rach_pdu->slotIndex)
            && (freq_index == p_fapi_rach_pdu->freqIndex)
            && (symbol_index == p_fapi_rach_pdu->symbolIndex)) {
            pdu_index = i;
            break;
        }
    }
    return pdu_index;
}

 /** @ingroup group_source_api_p7_fapi2mac_proc
 *
 *  @param[in]  p_phy_instance Pointer to PHY instance.
 *  @param[in]   p_phy_rach_ind Pointer to IAPI RACH.indication structure.
 *  @param[out]  p_fapi_rach_ind Pointer to FAPI RACH.indication structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This function converts IAPI RACH.indication to FAPI RACH.indication
 *  structure.
 *
**/
uint8_t nr5g_fapi_rach_indication_to_fapi_translation(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    PRXRACHIndicationStruct p_phy_rach_ind,
    fapi_rach_indication_t * p_fapi_rach_ind)
{
    uint8_t num_preamble, num_pdus = 0, i;
    uint8_t symbol_no, preamble_no;
    uint8_t slot_freq_idx_entry;
    uint8_t slot_index, freq_index, symbol_index;
    uint16_t slot_no, frame_no;

    fapi_rach_pdu_t *p_fapi_rach_pdu;
    fapi_rach_pdu_t *p_fapi_rach_pdu_match;
    fapi_preamble_info_t *p_fapi_preamble_info;
    nr5g_fapi_ul_slot_info_t *p_ul_slot_info;
    nr5g_fapi_stats_t *p_stats;
    PreambleStruct *p_phy_preamble_struct;

    p_stats = &p_phy_instance->stats;

    frame_no = p_fapi_rach_ind->sfn = p_phy_rach_ind->sSFN_Slot.nSFN;
    slot_no = p_fapi_rach_ind->slot = p_phy_rach_ind->sSFN_Slot.nSlot;
    symbol_no = p_phy_rach_ind->sSFN_Slot.nSym;

    p_ul_slot_info =
        nr5g_fapi_get_ul_slot_info(is_urllc, frame_no, slot_no, symbol_no, p_phy_instance);

    if (p_ul_slot_info == NULL) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[RACH.indication] No Valid data available "
                "for frame :%d and slot: %d", frame_no, slot_no));
        return FAILURE;
    }

    if (p_ul_slot_info->rach_presence == 0) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[RACHindication] RACH is not requested"
                "for frame :%d and slot: %d", frame_no, slot_no));
        return FAILURE;
    }

    num_preamble = p_phy_rach_ind->nNrOfPreamb;
    for (i = 0; i < num_preamble; i++) {
        p_stats->iapi_stats.iapi_rach_preambles++;
        p_phy_preamble_struct = &p_phy_rach_ind->sPreambleStruct[i];
        slot_index = p_phy_preamble_struct->nStartSlotdx;
        freq_index = p_phy_preamble_struct->nFreqIdx;
        symbol_index = p_phy_preamble_struct->nStartSymbIdx;
        //returns 0xFF if its a new preamble, else pdu index of p_fapi_rach_ind;
        slot_freq_idx_entry = nr5g_fapi_start_slot_freq_idx_occ(slot_index,
            freq_index, symbol_index, num_pdus, p_fapi_rach_ind);
        if (slot_freq_idx_entry == 0xFF) {
            p_fapi_rach_pdu = &p_fapi_rach_ind->rachPdu[i];
            p_fapi_rach_pdu->phyCellId = p_ul_slot_info->rach_info.phy_cell_id;
            p_fapi_rach_pdu->numPreamble = 0;
            p_fapi_rach_pdu->symbolIndex = p_phy_preamble_struct->nStartSymbIdx;
            p_fapi_rach_pdu->slotIndex = slot_index;
            p_fapi_rach_pdu->freqIndex = freq_index;
            p_fapi_rach_pdu->avgRssi = 0xFF;
            p_fapi_rach_pdu->avgSnr = 0xFF;
            preamble_no = p_fapi_rach_pdu->numPreamble++;
            p_fapi_preamble_info = &p_fapi_rach_pdu->preambleInfo[preamble_no];
            num_pdus++;
            p_stats->fapi_stats.fapi_rach_ind_pdus++;
        } else {
            p_fapi_rach_pdu_match =
                &p_fapi_rach_ind->rachPdu[slot_freq_idx_entry];
            preamble_no = p_fapi_rach_pdu_match->numPreamble++;
            p_fapi_preamble_info =
                &p_fapi_rach_pdu_match->preambleInfo[preamble_no];
        }
        p_fapi_preamble_info->preambleIndex = p_phy_preamble_struct->nPreambIdx;
        p_fapi_preamble_info->timingAdvance = p_phy_preamble_struct->nTa;
        p_fapi_preamble_info->preamblePwr = p_phy_preamble_struct->nPreambPwr;
    }
    p_fapi_rach_ind->numPdus = num_pdus;

    return SUCCESS;
}
