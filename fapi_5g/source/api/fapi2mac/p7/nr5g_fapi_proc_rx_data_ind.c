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
 * This file consist of implementation of FAPI CRC.indication message.
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
 *  @param[in]  p_phy_rx_data_ind Pointer to FAPI RX_DATA.indication message structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This message includes RX_DATA to be sent to L2.
 *
**/
uint8_t nr5g_fapi_rx_data_indication(
    bool is_urllc,
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    p_fapi_api_stored_vendor_queue_elems vendor_extension_elems,
    PRXULSCHIndicationStruct p_phy_rx_ulsch_ind)
{
    uint8_t phy_id;
    fapi_rx_data_indication_t *p_fapi_rx_data_ind;
    p_fapi_api_queue_elem_t p_list_elem;
    p_nr5g_fapi_phy_instance_t p_phy_instance = NULL;
    nr5g_fapi_stats_t *p_stats;

    if (NULL == p_phy_ctx) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[NR5G_FAPI][RX_DATA.indication] Invalid "
                "Phy Context"));
        return FAILURE;
    }

    if (NULL == p_phy_rx_ulsch_ind) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[NR5G_FAPI][RX_DATA.indication] Invalid "
                "RX_USLCH indication"));
        return FAILURE;
    }

    phy_id = p_phy_rx_ulsch_ind->sSFN_Slot.nCarrierIdx;
    p_phy_instance = &p_phy_ctx->phy_instance[phy_id];
    if (p_phy_instance->phy_id != phy_id) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[NR5G_FAPI][RX_DATA.indication] Invalid "
                "handle to phy instance"));
        return FAILURE;
    }

    p_stats = &p_phy_instance->stats;
    p_stats->iapi_stats.iapi_rx_data_ind++;

    p_list_elem =
        nr5g_fapi_fapi2mac_create_api_list_elem(FAPI_RX_DATA_INDICATION, 1,
        sizeof(fapi_rx_data_indication_t));

    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG,
            ("[NR5G_FAPI][RX_DATA.indication] Unable to create "
                "list element. Out of memory!!!"));
        return FAILURE;
    }

    p_fapi_rx_data_ind = (fapi_rx_data_indication_t *) (p_list_elem + 1);
    p_fapi_rx_data_ind->header.msg_id = FAPI_RX_DATA_INDICATION;
    p_fapi_rx_data_ind->header.length =
        (uint16_t) sizeof(fapi_rx_data_indication_t);

    fapi_vendor_p7_ind_msg_t* p_fapi_vend_p7 =
        nr5g_fapi_proc_vendor_p7_msg_get(vendor_extension_elems, phy_id);
    fapi_vendor_ext_rx_data_ind_t* p_fapi_vend_rx_data_ind =
        p_fapi_vend_p7 ? &p_fapi_vend_p7->rx_data_ind : NULL;
        
    if (p_fapi_vend_rx_data_ind) {
        p_fapi_vend_rx_data_ind->carrier_idx = phy_id;
        p_fapi_vend_rx_data_ind->sym = p_phy_rx_ulsch_ind->sSFN_Slot.nSym;
    }

    if (nr5g_fapi_rx_data_indication_to_fapi_translation(is_urllc, p_phy_instance,
            p_phy_rx_ulsch_ind, p_fapi_rx_data_ind)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[NR5G_FAPI][RX_DATA.indication] L1 to FAPI "
                "translation failed"));
        return FAILURE;
    }

    nr5g_fapi_fapi2mac_add_api_to_list(phy_id, p_list_elem, is_urllc);

    p_stats->fapi_stats.fapi_rx_data_ind++;
    NR5G_FAPI_LOG(DEBUG_LOG, ("[RX_DATA.indication][%u][%u,%u,%u] is_urllc %u",
            p_phy_instance->phy_id,
        p_phy_rx_ulsch_ind->sSFN_Slot.nSFN, p_phy_rx_ulsch_ind->sSFN_Slot.nSlot,
        p_phy_rx_ulsch_ind->sSFN_Slot.nSym, is_urllc));

    return SUCCESS;
}

 /** @ingroup group_source_api_p7_fapi2mac_proc
 *
 *  @param[in]   ue_id  Variable holding ue_id received in RX_DATA.Indication..
 *  @param[in]   p_ul_slot_info Pointer to ul slot info structure that stores the 
 *               UL_TTI.request PDU info.
 *  
 *  @return     Returns Pointer to pusch info, if handle of p_ul_slot_info matches ue_id.
                        NULL, if handle of p_ul_slot_info not matches ue_id
 *
 *  @description
 *  This function retrieves the pusch info stored during corresponding UL_TTI.request processing.  
 *  based on ue_id.
 *
**/
nr5g_fapi_pusch_info_t *nr5g_fapi_get_pusch_info(
    uint16_t ue_id,
    nr5g_fapi_ul_slot_info_t * p_ul_slot_info)
{
    uint8_t i, num_ulsch;

    nr5g_fapi_pusch_info_t *p_pusch_info;

    num_ulsch = p_ul_slot_info->num_ulsch;
    for (i = 0; i < num_ulsch; i++) {
        p_pusch_info = &p_ul_slot_info->pusch_info[i];
        if (p_pusch_info->handle == ue_id) {
            return p_pusch_info;
        }
    }
    return NULL;
}

 /** @ingroup group_source_api_p7_fapi2mac_proc
 *
 *  @param[in]   p_phy_instance Pointer to PHY instance.
 *  @param[in]   p_phy_rx_ulsch_ind Pointer to IAPI RX_ULSCH.indication structure.
 *  @param[out]  p_fapi_rx_data_ind Pointer to FAPI RX_DATA.indication structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This function converts IAPI RX_DATA.indication to FAPI CRC.indication
 *  structure.
 *
**/
uint8_t nr5g_fapi_rx_data_indication_to_fapi_translation(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    PRXULSCHIndicationStruct p_phy_rx_ulsch_ind,
    fapi_rx_data_indication_t * p_fapi_rx_data_ind)
{
    uint8_t num_ulsch, i;
    uint8_t symbol_no;
    uint16_t slot_no, frame_no;

    nr5g_fapi_pusch_info_t *p_pusch_info;
    fapi_pdu_ind_info_t *p_fapi_pdu_ind_info;
    nr5g_fapi_ul_slot_info_t *p_ul_slot_info;
    nr5g_fapi_stats_t *p_stats;
    ULSCHPDUDataStruct *p_rx_ulsch_pdu_data;

    p_stats = &p_phy_instance->stats;

    frame_no = p_fapi_rx_data_ind->sfn = p_phy_rx_ulsch_ind->sSFN_Slot.nSFN;
    slot_no = p_fapi_rx_data_ind->slot = p_phy_rx_ulsch_ind->sSFN_Slot.nSlot;
    symbol_no = p_phy_rx_ulsch_ind->sSFN_Slot.nSym;

    p_ul_slot_info =
        nr5g_fapi_get_ul_slot_info(is_urllc, frame_no, slot_no, symbol_no, p_phy_instance);

    if (p_ul_slot_info == NULL) {
        NR5G_FAPI_LOG(ERROR_LOG,
            ("[NR5G_FAPI] [RX_DATA.indication] No Valid data available "
                "for frame :%d and slot: %d", frame_no, slot_no));
        return FAILURE;
    }

    num_ulsch = p_fapi_rx_data_ind->numPdus = p_phy_rx_ulsch_ind->nUlsch;
    for (i = 0; i < num_ulsch; i++) {
        p_stats->iapi_stats.iapi_rx_data_ind_pdus++;
        p_fapi_pdu_ind_info = &p_fapi_rx_data_ind->pdus[i];
        p_rx_ulsch_pdu_data = &p_phy_rx_ulsch_ind->sULSCHPDUDataStruct[i];
        p_pusch_info =
            nr5g_fapi_get_pusch_info(p_rx_ulsch_pdu_data->nUEId,
            p_ul_slot_info);
        if (p_pusch_info == NULL) {
            NR5G_FAPI_LOG(ERROR_LOG,
                ("[NR5G_FAPI] [RX_DATA.indication] No Valid data available "
                    "nUEId:%d, frame_no:%d, slot_no:%d, urllc %u",
                    p_rx_ulsch_pdu_data->nUEId, frame_no, slot_no, is_urllc));
            return FAILURE;
        }

        p_fapi_pdu_ind_info->handle = p_pusch_info->handle;
        p_fapi_pdu_ind_info->rnti = p_rx_ulsch_pdu_data->nRNTI;
        p_fapi_pdu_ind_info->harqId = p_pusch_info->harq_process_id;
        p_fapi_pdu_ind_info->ul_cqi = p_pusch_info->ul_cqi;
        p_fapi_pdu_ind_info->timingAdvance = p_pusch_info->timing_advance;
        p_fapi_pdu_ind_info->rssi = 880;
        p_fapi_pdu_ind_info->pdu_length = p_rx_ulsch_pdu_data->nPduLen;
        if (p_fapi_pdu_ind_info->pdu_length > 0)
        {
        p_fapi_pdu_ind_info->pduData = (void *)p_rx_ulsch_pdu_data->pPayload;
        }

        p_stats->fapi_stats.fapi_rx_data_ind_pdus++;
    }
    return SUCCESS;
}
