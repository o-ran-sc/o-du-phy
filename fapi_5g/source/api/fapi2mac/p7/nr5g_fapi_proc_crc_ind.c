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
#include "nr5g_fapi_snr_conversion.h"

 /** @ingroup group_source_api_p7_fapi2mac_proc
 *
 *  @param[in]  p_phy_ctx Pointer to PHY context.
 *  @param[in]  p_phy_crc_ind Pointer to FAPI CRC.indication message structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This message includes CRC PDU.
 *
**/
uint8_t nr5g_fapi_crc_indication(
    bool is_urllc,
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    p_fapi_api_stored_vendor_queue_elems vendor_extension_elems,
    PCRCIndicationStruct p_phy_crc_ind)
{
    uint8_t phy_id;
    fapi_crc_ind_t *p_fapi_crc_ind;
    p_fapi_api_queue_elem_t p_list_elem;
    p_nr5g_fapi_phy_instance_t p_phy_instance = NULL;
    nr5g_fapi_stats_t *p_stats;

    if (NULL == p_phy_ctx) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[CRC.indication] Invalid " "phy context"));
        return FAILURE;
    }

    if (NULL == p_phy_crc_ind) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[NR5G_FAPI [CRC.indication] Invalid Phy "
                "CRC indication"));
        return FAILURE;
    }

    phy_id = p_phy_crc_ind->sSFN_Slot.nCarrierIdx;
    p_phy_instance = &p_phy_ctx->phy_instance[phy_id];
    if (p_phy_instance->phy_id != phy_id) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[CRC.indication] Invalid " "phy instance"));
        return FAILURE;
    }

    p_stats = &p_phy_instance->stats;
    p_stats->iapi_stats.iapi_crc_ind++;

    p_list_elem =
        nr5g_fapi_fapi2mac_create_api_list_elem(FAPI_CRC_INDICATION, 1,
        sizeof(fapi_crc_ind_t));

    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[CRC.indication] Unable to create "
                "list element. Out of memory!!!"));
        return FAILURE;
    }

    p_fapi_crc_ind = (fapi_crc_ind_t *) (p_list_elem + 1);
    p_fapi_crc_ind->header.msg_id = FAPI_CRC_INDICATION;
    p_fapi_crc_ind->header.length = (uint16_t) sizeof(fapi_crc_ind_t);

    fapi_vendor_p7_ind_msg_t* p_fapi_vend_p7 =
        nr5g_fapi_proc_vendor_p7_msg_get(vendor_extension_elems, phy_id);
    fapi_vendor_ext_snr_t* p_fapi_snr  = p_fapi_vend_p7 ? &p_fapi_vend_p7->crc_snr : NULL;
    fapi_vendor_ext_crc_ind_t* p_fapi_vend_crc_ind = p_fapi_vend_p7 ? &p_fapi_vend_p7->crc_ind : NULL;
    
    if (p_fapi_vend_crc_ind) {
        p_fapi_vend_crc_ind->carrier_idx = phy_id;
        p_fapi_vend_crc_ind->sym = p_phy_crc_ind->sSFN_Slot.nSym;
    }

    if (nr5g_fapi_crc_indication_to_fapi_translation(is_urllc, p_phy_instance,
            p_phy_crc_ind, p_fapi_crc_ind, p_fapi_snr)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[CRC.indication] L1 to FAPI "
                "translation failed"));
        return FAILURE;
    }

    nr5g_fapi_fapi2mac_add_api_to_list(phy_id, p_list_elem, is_urllc);

    p_stats->fapi_stats.fapi_crc_ind++;

    NR5G_FAPI_LOG(DEBUG_LOG, ("[CRC.indication][%u][%u,%u,%u] is_urllc %u",
            p_phy_instance->phy_id,
            p_phy_crc_ind->sSFN_Slot.nSFN, p_phy_crc_ind->sSFN_Slot.nSlot,
            p_phy_crc_ind->sSFN_Slot.nSym, is_urllc));

    return SUCCESS;
}

 /** @ingroup group_source_api_p7_fapi2mac_proc
 *
 *  @param[in]  p_phy_instance Pointer to PHY instance.
 *  @param[in]   p_phy_crc_ind Pointer to IAPI CRC.indication structure.
 *  @param[out]  p_fapi_crc_ind Pointer to FAPI CRC.indication structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This function converts IAPI CRC.indication to FAPI CRC.indication
 *  structure.
 *
**/
uint8_t nr5g_fapi_crc_indication_to_fapi_translation(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    PCRCIndicationStruct p_phy_crc_ind,
    fapi_crc_ind_t * p_fapi_crc_ind,
    fapi_vendor_ext_snr_t * p_fapi_snr)
{
    uint8_t num_crc, i;
    uint8_t symbol_no;
    uint16_t slot_no, frame_no;

    nr5g_fapi_pusch_info_t *p_pusch_info;
    fapi_crc_ind_info_t *p_fapi_crc_ind_info;
    nr5g_fapi_ul_slot_info_t *p_ul_slot_info;
    nr5g_fapi_stats_t *p_stats;
    ULCRCStruct *p_ul_crc_struct;

    p_stats = &p_phy_instance->stats;

    frame_no = p_fapi_crc_ind->sfn = p_phy_crc_ind->sSFN_Slot.nSFN;
    slot_no = p_fapi_crc_ind->slot = p_phy_crc_ind->sSFN_Slot.nSlot;
    symbol_no = p_phy_crc_ind->sSFN_Slot.nSym;

    p_ul_slot_info =
        nr5g_fapi_get_ul_slot_info(is_urllc, frame_no, slot_no, symbol_no, p_phy_instance);

    if (p_ul_slot_info == NULL) {
        NR5G_FAPI_LOG(ERROR_LOG, (" [CRC.indication] No Valid data available "
                "for frame :%d, slot: %d, symbol: %d, urllc %u", frame_no, slot_no, symbol_no, is_urllc));
        return FAILURE;
    }

    num_crc = p_fapi_crc_ind->numCrcs = p_phy_crc_ind->nCrc;
    for (i = 0; i < num_crc; i++) {
        p_stats->iapi_stats.iapi_crc_ind_pdus++;

        p_fapi_crc_ind_info = &p_fapi_crc_ind->crc[i];
        p_ul_crc_struct = &p_phy_crc_ind->sULCRCStruct[i];
        p_pusch_info =
            nr5g_fapi_get_pusch_info(p_ul_crc_struct->nUEId, p_ul_slot_info);

        if (p_pusch_info == NULL) {
            NR5G_FAPI_LOG(ERROR_LOG,
                (" [CRC.indication] No Valid data available "
                    "nUEId:%d, frame_no:%d, slot_no:%d, urllc %u", p_ul_crc_struct->nUEId,
                    frame_no, slot_no, is_urllc));
            return FAILURE;
        }

        p_fapi_crc_ind_info->handle = p_pusch_info->handle;
        p_fapi_crc_ind_info->rnti = p_ul_crc_struct->nRNTI;
        p_fapi_crc_ind_info->harqId = p_pusch_info->harq_process_id;
        p_fapi_crc_ind_info->tbCrcStatus = !(p_ul_crc_struct->nCrcFlag);
        p_fapi_crc_ind_info->ul_cqi = nr5g_fapi_convert_snr_iapi_to_fapi(p_ul_crc_struct->nSNR);
        if(p_fapi_snr)
        {
            p_fapi_snr->nSNR[i] = p_ul_crc_struct->nSNR;
        }
        p_pusch_info->ul_cqi = p_fapi_crc_ind_info->ul_cqi;

        p_fapi_crc_ind_info->numCb = 0;
        p_pusch_info->timing_advance = p_fapi_crc_ind_info->timingAdvance = 31;
#ifdef DEBUG_MODE
        p_pusch_info->timing_advance = p_fapi_crc_ind_info->timingAdvance =
            p_ul_crc_struct->nTA;
#endif
        p_fapi_crc_ind_info->rssi = 880;

        p_stats->fapi_stats.fapi_crc_ind_pdus++;
    }
    return SUCCESS;
}
