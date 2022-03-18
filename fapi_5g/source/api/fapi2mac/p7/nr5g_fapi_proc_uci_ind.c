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
 * This file consist of implementation of FAPI UCI.indication message.
 *
 **/

#include "nr5g_fapi_framework.h"
#include "gnb_l1_l2_api.h"
#include "nr5g_fapi_fapi2mac_api.h"
#include "nr5g_fapi_fapi2mac_p7_proc.h"
#include "nr5g_fapi_fapi2mac_p7_pvt_proc.h"
#include "nr5g_fapi_memory.h"
#include "nr5g_fapi_snr_conversion.h"

 /** @ingroup group_source_api_p7_fapi2mac_proc
 *
 *  @param[in]  p_phy_ctx     Pointer to PHY context.
 *  @param[in]  p_phy_uci_ind Pointer to FAPI UCI.indication message structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This message includes UCI payload in PUCCH or PUSCH. 
 *
**/
uint8_t nr5g_fapi_uci_indication(
    bool is_urllc,
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    p_fapi_api_stored_vendor_queue_elems vendor_extension_elems,
    PRXUCIIndicationStruct p_phy_uci_ind)
{
    uint8_t phy_id;
    fapi_uci_indication_t *p_fapi_uci_ind;
    p_fapi_api_queue_elem_t p_list_elem;
    p_nr5g_fapi_phy_instance_t p_phy_instance = NULL;
    nr5g_fapi_stats_t *p_stats;

    if (NULL == p_phy_ctx) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[UCI.indication] Invalid Phy " "Context"));
        return FAILURE;
    }

    if (NULL == p_phy_uci_ind) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[UCI.indication] Invalid Phy "
                "UCI indication"));
        return FAILURE;
    }

    phy_id = p_phy_uci_ind->sSFN_Slot.nCarrierIdx;
    p_phy_instance = &p_phy_ctx->phy_instance[phy_id];
    if ((p_phy_instance->phy_id != phy_id)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[UCI.indication] Invalid " "phy instance"));
        return FAILURE;
    }

    p_stats = &p_phy_instance->stats;
    p_stats->iapi_stats.iapi_uci_ind++;

    p_list_elem =
        nr5g_fapi_fapi2mac_create_api_list_elem(FAPI_UCI_INDICATION, 1,
        sizeof(fapi_uci_indication_t));
    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[UCI.indication] Unable to create "
                "list element. Out of memory!!!"));
        return FAILURE;
    }

    p_fapi_uci_ind = (fapi_uci_indication_t *) (p_list_elem + 1);
    p_fapi_uci_ind->header.msg_id = FAPI_UCI_INDICATION;
    p_fapi_uci_ind->header.length = (uint16_t) sizeof(fapi_uci_indication_t);

    fapi_vendor_p7_ind_msg_t* p_fapi_vend_p7 =
        nr5g_fapi_proc_vendor_p7_msg_get(vendor_extension_elems, phy_id);
    fapi_vendor_ext_snr_t* p_fapi_snr = p_fapi_vend_p7 ? &p_fapi_vend_p7->uci_snr : NULL;
    fapi_vendor_ext_uci_ind_t* p_fapi_vend_uci_ind = p_fapi_vend_p7 ? &p_fapi_vend_p7->uci_ind : NULL;
    
    if (p_fapi_vend_uci_ind) {
        p_fapi_vend_uci_ind->carrier_idx = phy_id;
        p_fapi_vend_uci_ind->sym = p_phy_uci_ind->sSFN_Slot.nSym;
    }

    if (nr5g_fapi_uci_indication_to_fapi_translation(is_urllc, p_phy_instance,
            p_phy_uci_ind, p_fapi_uci_ind, p_fapi_snr)) {
        NR5G_FAPI_LOG(ERROR_LOG,
            ("[UCI.indication] FAPI to L1 " "translation failed"));
        return FAILURE;
    }
    /* Add element to send list */
    nr5g_fapi_fapi2mac_add_api_to_list(phy_id, p_list_elem, is_urllc);

    p_stats->fapi_stats.fapi_uci_ind++;
    NR5G_FAPI_LOG(DEBUG_LOG, ("[UCI.indication][%u][%u,%u,%u] is_urllc %u",
            p_phy_instance->phy_id,
        p_phy_uci_ind->sSFN_Slot.nSFN, p_phy_uci_ind->sSFN_Slot.nSlot,
        p_phy_uci_ind->sSFN_Slot.nSym, is_urllc));

    return SUCCESS;
}

 /** @ingroup group_source_api_p7_fapi2mac_proc
 *
 *  @param[in]   ue_id  Variable holding ue_id received in RX_DATA.Indication..
 *  @param[in]   p_ul_slot_info Pointer to ul slot info structure that stores the 
 *               UL_TTI.request PDU info.
 *  
 *  @return     Returns Pointer to pucch info, if handle of p_ul_slot_info matches ue_id.
                        NULL, if handle of p_ul_slot_info not matches ue_id
 *
 *  @description
**/
nr5g_fapi_pucch_info_t *nr5g_fapi_get_pucch_info(
    uint16_t ue_id,
    nr5g_fapi_ul_slot_info_t * p_ul_slot_info)
{
    uint8_t i, num_ulcch;
    nr5g_fapi_pucch_info_t *p_pucch_info;

    num_ulcch = p_ul_slot_info->num_ulcch;
    for (i = 0; i < num_ulcch; i++) {
        p_pucch_info = &p_ul_slot_info->pucch_info[i];
        if (p_pucch_info->handle == ue_id) {
            return p_pucch_info;
        }
    }
    return NULL;
}

 /** @ingroup group_source_api_p7_fapi2mac_proc
 *
 *  @param[in]   p_pucch_info Pointer to pucch_info stored in ul slot info during
 *               UL_TTI.request processing
 *  @param[in]   p_uci_pdu_data_struct Pointer to IAPI UCI PDU structure.
 *  @param[out]  p_fapi_uci_pdu_info   Pointer to FAPI UCI PDU structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This function fills  FAPI UCI FORMAT0_FORMAT1 PDU from IAPI UCI PDU
 *  structure.
 *
**/
void nr5g_fapi_fill_uci_format_0_1(
    nr5g_fapi_pucch_info_t * p_pucch_info,
    ULUCIPDUDataStruct * p_uci_pdu_data_struct,
    fapi_uci_pdu_info_t * p_fapi_uci_pdu_info,
    int16_t * p_fapi_snr)
{
    uint8_t pucch_detected, num_harq, i;

    fapi_uci_o_pucch_f0f1_t *p_uci_pucch_f0_f1;
    fapi_sr_f0f1_info_t *p_sr_info;
    fapi_harq_f0f1_info_t *p_harq_info;

    p_uci_pucch_f0_f1 = &p_fapi_uci_pdu_info->uci.uciPucchF0F1;
    NR5G_FAPI_MEMSET(p_uci_pucch_f0_f1, sizeof(fapi_uci_o_pucch_f0f1_t), 0,
        sizeof(fapi_uci_o_pucch_f0f1_t));

    p_uci_pucch_f0_f1->handle = p_pucch_info->handle;
    p_uci_pucch_f0_f1->pduBitmap = 0;
    p_uci_pucch_f0_f1->pucchFormat = p_pucch_info->pucch_format;

    if(p_fapi_snr)
    {
        *p_fapi_snr = p_uci_pdu_data_struct->nSNR;
    }

    p_uci_pucch_f0_f1->ul_cqi = nr5g_fapi_convert_snr_iapi_to_fapi(p_uci_pdu_data_struct->nSNR);
    p_uci_pucch_f0_f1->rnti = p_uci_pdu_data_struct->nRNTI;
    p_uci_pucch_f0_f1->timingAdvance = 31;
    p_uci_pucch_f0_f1->rssi = 880;

    if (p_uci_pdu_data_struct->nSRPresent) {
        p_uci_pucch_f0_f1->pduBitmap |= 0x01;
        p_sr_info = &p_uci_pucch_f0_f1->srInfo;
        p_sr_info->srIndication = 1;
        p_sr_info->srConfidenceLevel = 0xff;
    }
    pucch_detected = p_uci_pdu_data_struct->pucchDetected;
    if (pucch_detected == 1) {
        p_uci_pucch_f0_f1->pduBitmap |= 0x02;
        p_harq_info = &p_uci_pucch_f0_f1->harqInfo;
        p_harq_info->harqConfidenceLevel = 0xff;
        num_harq = p_harq_info->numHarq = p_uci_pdu_data_struct->nPduBitLen;
        for (i = 0; i < num_harq; i++) {
            p_harq_info->harqValue[i] = p_uci_pdu_data_struct->nUciBits[i];
        }
    } else {                    // 0 or 2
        p_uci_pucch_f0_f1->rssi = 0;
    }
#ifdef DEBUG_MODE
    p_uci_pucch_f0_f1->timingAdvance = p_uci_pdu_data_struct->nTA;
    if (pucch_detected == 2) {
        p_uci_pucch_f0_f1->pduBitmap |= 0x80;
        p_harq_info = &p_uci_pucch_f0_f1->harqInfo;
        p_harq_info->harqConfidenceLevel = 0xff;
        num_harq = p_harq_info->numHarq = p_uci_pdu_data_struct->nPduBitLen;
        for (i = 0; i < num_harq; i++) {
            p_harq_info->harqValue[i] = p_uci_pdu_data_struct->nUciBits[i];
        }
    }
#endif
}

 /** @ingroup group_source_api_p7_fapi2mac_proc
 *
 *  @param[in]   p_pucch_info Pointer to pucch_info stored in ul slot info during
 *               UL_TTI.request processing
 *  @param[in]   p_uci_pdu_data_struct Pointer to IAPI UCI PDU structure.
 *  @param[out]  p_fapi_uci_pdu_info   Pointer to FAPI UCI PDU structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This function fills  FAPI UCI FORMAT2_FORMAT3_FORMAT4 PDU from IAPI UCI PDU
 *  structure.
 *
**/
void nr5g_fapi_fill_uci_format_2_3_4(
    nr5g_fapi_pucch_info_t * p_pucch_info,
    ULUCIPDUDataStruct * p_uci_pdu_data_struct,
    fapi_uci_pdu_info_t * p_fapi_uci_pdu_info,
    int16_t * p_fapi_snr)
{
    uint8_t pucch_detected;
    uint16_t num_uci_bits;

    fapi_uci_o_pucch_f2f3f4_t *p_uci_pucch_f2_f3_f4;

    p_uci_pucch_f2_f3_f4 = &p_fapi_uci_pdu_info->uci.uciPucchF2F3F4;

    p_uci_pucch_f2_f3_f4->handle = p_pucch_info->handle;
    p_uci_pucch_f2_f3_f4->pduBitmap = 0;
    p_uci_pucch_f2_f3_f4->pucchFormat = p_pucch_info->pucch_format;
    p_uci_pucch_f2_f3_f4->ul_cqi = nr5g_fapi_convert_snr_iapi_to_fapi(p_uci_pdu_data_struct->nSNR);
    p_uci_pucch_f2_f3_f4->rnti = p_uci_pdu_data_struct->nRNTI;
    p_uci_pucch_f2_f3_f4->timingAdvance = 31;

    if(p_fapi_snr)
    {
        *p_fapi_snr = p_uci_pdu_data_struct->nSNR;
    }

    pucch_detected = p_uci_pdu_data_struct->pucchDetected;
#ifdef DEBUG_MODE
    p_uci_pucch_f2_f3_f4->timingAdvance = p_uci_pdu_data_struct->nTA;
    if (pucch_detected == 2) {
        p_uci_pucch_f2_f3_f4->pduBitmap |= 0x80;
    }
#endif
    p_uci_pucch_f2_f3_f4->rssi = 880;
    if (p_uci_pdu_data_struct->nSRPresent) {
        p_uci_pucch_f2_f3_f4->pduBitmap |= 0x01;
    }
    pucch_detected = p_uci_pdu_data_struct->pucchDetected;
    if (pucch_detected == 1) {
        p_uci_pucch_f2_f3_f4->pduBitmap |= 0x02;
    } else {
        p_uci_pucch_f2_f3_f4->rssi = 0;
    }
    num_uci_bits = p_uci_pucch_f2_f3_f4->num_uci_bits =
        p_uci_pdu_data_struct->nPduBitLen;
    if (num_uci_bits > 0) {
        NR5G_FAPI_MEMCPY(p_uci_pucch_f2_f3_f4->uciBits,
            sizeof(uint8_t) * FAPI_MAX_UCI_BIT_BYTE_LEN,
            p_uci_pdu_data_struct->nUciBits,
            sizeof(uint8_t) * FAPI_MAX_UCI_BIT_BYTE_LEN);
    }
}

 /** @ingroup group_source_api_p7_fapi2mac_proc
 *
 *  @param[in]  p_phy_instance Pointer to PHY instance.
 *  @param[in]   p_phy_uci_ind Pointer to IAPI UCI.indication structure.
 *  @param[out]  p_fapi_uci_ind Pointer to FAPI UCI.indication structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This function converts IAPI UCI.indication to FAPI UCI.indication
 *  structure.
 *
**/
uint8_t nr5g_fapi_uci_indication_to_fapi_translation(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    PRXUCIIndicationStruct p_phy_uci_ind,
    fapi_uci_indication_t * p_fapi_uci_ind,
    fapi_vendor_ext_snr_t * p_fapi_snr)
{
    uint8_t num_uci, i;
    uint8_t symbol_no, pucch_format;
    uint16_t slot_no, frame_no;

    nr5g_fapi_pucch_info_t *p_pucch_info;
    fapi_uci_pdu_info_t *p_fapi_uci_pdu_info;
    nr5g_fapi_ul_slot_info_t *p_ul_slot_info;
    nr5g_fapi_stats_t *p_stats;
    ULUCIPDUDataStruct *p_uci_pdu_data_struct;

    p_stats = &p_phy_instance->stats;

    frame_no = p_fapi_uci_ind->sfn = p_phy_uci_ind->sSFN_Slot.nSFN;
    slot_no = p_fapi_uci_ind->slot = p_phy_uci_ind->sSFN_Slot.nSlot;
    symbol_no = p_phy_uci_ind->sSFN_Slot.nSym;

    p_ul_slot_info =
        nr5g_fapi_get_ul_slot_info(is_urllc, frame_no, slot_no, symbol_no, p_phy_instance);

    if (p_ul_slot_info == NULL) {
        NR5G_FAPI_LOG(ERROR_LOG, (" [UCI.indication] No Valid data available "
                "for frame :%d and slot: %d", frame_no, slot_no));
        return FAILURE;
    }

    num_uci = p_fapi_uci_ind->numUcis = p_phy_uci_ind->nUCI;
    for (i = 0; i < num_uci; i++) {
        p_stats->iapi_stats.iapi_uci_ind_pdus++;
        p_fapi_uci_pdu_info = &p_fapi_uci_ind->uciPdu[i];
        p_uci_pdu_data_struct = &p_phy_uci_ind->sULUCIPDUDataStruct[i];
        p_pucch_info =
            nr5g_fapi_get_pucch_info(p_uci_pdu_data_struct->nUEId,
            p_ul_slot_info);
        if (p_pucch_info == NULL) {
            NR5G_FAPI_LOG(ERROR_LOG,
                (" [UCI.indication] No Valid data available "
                    "for nUEId:%d with frameno:%d, slot_no:%d",
                    p_uci_pdu_data_struct->nUEId, frame_no, slot_no));
            return FAILURE;
        }

        pucch_format = p_pucch_info->pucch_format;
        int16_t* p_fapi_snr_arr = p_fapi_snr ? &p_fapi_snr->nSNR[i] : NULL;

        switch (pucch_format) {
            case FAPI_PUCCH_FORMAT_TYPE_0:
            case FAPI_PUCCH_FORMAT_TYPE_1:
                {
                    p_fapi_uci_pdu_info->pduType = 1;
                    p_fapi_uci_pdu_info->pduSize =
                        sizeof(fapi_uci_o_pucch_f0f1_t);
                    nr5g_fapi_fill_uci_format_0_1(p_pucch_info,
                        p_uci_pdu_data_struct, p_fapi_uci_pdu_info, p_fapi_snr_arr);
                }
                break;

            case FAPI_PUCCH_FORMAT_TYPE_2:
            case FAPI_PUCCH_FORMAT_TYPE_3:
            case FAPI_PUCCH_FORMAT_TYPE_4:
                {
                    p_fapi_uci_pdu_info->pduType = 2;
                    p_fapi_uci_pdu_info->pduSize =
                        sizeof(fapi_uci_o_pucch_f2f3f4_t);
                    nr5g_fapi_fill_uci_format_2_3_4(p_pucch_info,
                        p_uci_pdu_data_struct, p_fapi_uci_pdu_info, p_fapi_snr_arr);
                }
                break;

            default:
                {
                    NR5G_FAPI_LOG(ERROR_LOG,
                        (" [UCI.indication] Invalid PUCCH Format"
                            "pucch_format:%d", pucch_format));
                    return FAILURE;
                }
                break;
        }
        p_stats->fapi_stats.fapi_uci_ind_pdus++;
    }
    return SUCCESS;
}
