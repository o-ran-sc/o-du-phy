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
 * This file consist of implementation of FAPI SRS.indication message.
 *
 **/

#include "nr5g_fapi_framework.h"
#include "gnb_l1_l2_api.h"
#include "nr5g_fapi_fapi2mac_api.h"
#include "nr5g_fapi_fapi2mac_p7_proc.h"
#include "nr5g_fapi_fapi2mac_p7_pvt_proc.h"
#include "nr5g_fapi_memory.h"

 /** @ingroup group_source_api_p7_fapi2mac_proc
 *
 *  @param[in]  p_phy_ctx Pointer to PHY context.
 *  @param[in]  p_phy_srs_ind Pointer to FAPI SRS.indication message structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This message includes SRS PDU. 
 *
**/
uint8_t nr5g_fapi_srs_indication(
    bool is_urllc,
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    p_fapi_api_stored_vendor_queue_elems vendor_extension_elems,
    PRXSRSIndicationStruct p_phy_srs_ind)
{
    uint8_t phy_id;
    fapi_srs_indication_t *p_fapi_srs_ind;
    p_fapi_api_queue_elem_t p_list_elem;
    p_nr5g_fapi_phy_instance_t p_phy_instance = NULL;
    nr5g_fapi_stats_t *p_stats;
    fapi_vendor_p7_ind_msg_t *p_fapi_vend_p7;
    fapi_vendor_ext_srs_ind_t *p_fapi_vend_srs_ind;

    if (NULL == p_phy_ctx) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[SRS.indication] Invalid " "Phy Context"));
        return FAILURE;
    }

    if (NULL == p_phy_srs_ind) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[SRS.indication] Invalid "
                "SRS Indication"));
        return FAILURE;
    }

    phy_id = p_phy_srs_ind->sSFN_Slot.nCarrierIdx;
    p_phy_instance = &p_phy_ctx->phy_instance[phy_id];
    if (p_phy_instance->phy_id != phy_id) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[SRS.indication] Invalid " "Phy Instance"));
        return FAILURE;
    }

    p_stats = &p_phy_instance->stats;
    p_stats->iapi_stats.iapi_srs_ind++;

    p_list_elem =
        nr5g_fapi_fapi2mac_create_api_list_elem(FAPI_SRS_INDICATION, 1,
        sizeof(fapi_srs_indication_t));
    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[SRS.indication] Unable "
                "to create list element. Out of memory!!!"));
        return FAILURE;
    }

    p_fapi_srs_ind = (fapi_srs_indication_t *) (p_list_elem + 1);
    p_fapi_srs_ind->header.msg_id = FAPI_SRS_INDICATION;
    p_fapi_srs_ind->header.length = (uint16_t) sizeof(fapi_srs_indication_t);

    p_fapi_vend_p7 =
        nr5g_fapi_proc_vendor_p7_msg_get(vendor_extension_elems, phy_id);
    p_fapi_vend_srs_ind =
        p_fapi_vend_p7 ? &p_fapi_vend_p7->srs_ind : NULL;
    if (nr5g_fapi_srs_indication_to_fapi_translation(is_urllc, p_phy_instance,
            p_phy_srs_ind, p_fapi_srs_ind, p_fapi_vend_srs_ind)) {
        NR5G_FAPI_LOG(ERROR_LOG,
            ("[SRS.indication] L1 to FAPI " "translation failed"));
        return FAILURE;
    }

    /* Add element to send list */
    nr5g_fapi_fapi2mac_add_api_to_list(phy_id, p_list_elem, is_urllc);

    p_stats->fapi_stats.fapi_srs_ind++;
    NR5G_FAPI_LOG(DEBUG_LOG, ("[SRS.indication][%u][%u,%u,%u] is_urllc %u",
            p_phy_instance->phy_id,
            p_phy_srs_ind->sSFN_Slot.nSFN, p_phy_srs_ind->sSFN_Slot.nSlot,
            p_phy_srs_ind->sSFN_Slot.nSym, is_urllc));

    return SUCCESS;
}

 /** @ingroup group_source_api_p7_fapi2mac_proc
 *
 *  @param[in]   ue_id  Variable holding ue_id received SRS.Indication..
 *  @param[in]   p_ul_slot_info Pointer to ul slot info structure that stores the 
 *               UL_TTI.request PDU info.
 *  
 *  @return     Returns Pointer to srs info, if handle of p_ul_slot_info matches ue_id.
                        NULL, if handle of p_ul_slot_info not matches ue_id
 *
 *  @description
 *  This function retrieves the srs info stored during corresponding UL_TTI.request processing.  
 *  based on ue_id.
 *
**/
nr5g_fapi_srs_info_t *nr5g_fapi_get_srs_info(
    uint16_t nUEId,
    nr5g_fapi_ul_slot_info_t * p_ul_slot_info)
{
    uint8_t i, num_srs;

    nr5g_fapi_srs_info_t *p_srs_info;

    num_srs = p_ul_slot_info->num_srs;
    for (i = 0; i < num_srs; i++) {
        p_srs_info = &p_ul_slot_info->srs_info[i];
        if (p_srs_info->handle == nUEId) {
            return p_srs_info;
        }
    }
    return NULL;
}

 /** @ingroup group_source_api_p7_fapi2mac_proc
 *
 *  @param[in]  p_phy_instance Pointer to PHY instance.
 *  @param[in]   p_phy_srs_ind Pointer to IAPI SRS.indication structure.
 *  @param[out]  p_fapi_srs_ind Pointer to FAPI SRS.indication structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This function converts IAPI SRS.indication to FAPI SRS.indication
 *  structure.
 *
**/
uint8_t nr5g_fapi_srs_indication_to_fapi_translation(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    PRXSRSIndicationStruct p_phy_srs_ind,
    fapi_srs_indication_t * p_fapi_srs_ind,
    fapi_vendor_ext_srs_ind_t * p_fapi_vend_srs_ind)
{
    uint8_t num_srs_pdus, i;
    uint8_t symbol_no, num_rept_symbols, nr_of_symbols;
    uint16_t slot_no, frame_no, num_rbs, j, k;
    int8_t wideband_snr = 0, rb_snr;
    int16_t temp_sum_wideband_snr;

    nr5g_fapi_srs_info_t *p_srs_info;
    fapi_srs_pdu_t *p_fapi_srs_pdu;
    fapi_symb_snr_t *p_fapi_symb_snr;
    nr5g_fapi_ul_slot_info_t *p_ul_slot_info;
    nr5g_fapi_stats_t *p_stats;
    ULSRSEstStruct *p_ul_srs_est_struct;
    fapi_vendor_ext_srs_pdu_t *p_vend_srs_pdu;

    p_stats = &p_phy_instance->stats;

    frame_no = p_fapi_srs_ind->sfn = p_phy_srs_ind->sSFN_Slot.nSFN;
    slot_no = p_fapi_srs_ind->slot = p_phy_srs_ind->sSFN_Slot.nSlot;
    symbol_no = p_phy_srs_ind->sSFN_Slot.nSym;

    p_ul_slot_info =
        nr5g_fapi_get_ul_slot_info(is_urllc, frame_no, slot_no, symbol_no, p_phy_instance);

    if (p_ul_slot_info == NULL) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[SRS.indication] No Valid data available "
                "for frame :%d and slot: %d", frame_no, slot_no));
        return FAILURE;
    }

    num_srs_pdus = p_fapi_srs_ind->numPdus = p_phy_srs_ind->nNrOfSrs;
    for (i = 0; i < num_srs_pdus; i++) {
        temp_sum_wideband_snr = 0;
        p_stats->iapi_stats.iapi_srs_ind_pdus++;
        p_fapi_srs_pdu = &p_fapi_srs_ind->srsPdus[i];
        p_ul_srs_est_struct = &p_phy_srs_ind->sULSRSEstStruct[i];
        p_srs_info =
            nr5g_fapi_get_srs_info(p_ul_srs_est_struct->nUEId, p_ul_slot_info);
        if (p_srs_info == NULL) {
            NR5G_FAPI_LOG(ERROR_LOG,
                ("[SRS.indication] No Valid data available "
                    "for nUEId:%d with frameno:%d, slot_no:%d",
                    p_ul_srs_est_struct->nUEId, frame_no, slot_no));
            return FAILURE;
        }

        p_fapi_srs_pdu->handle = p_srs_info->handle;
        p_fapi_srs_pdu->rnti = p_ul_srs_est_struct->nRNTI;
        p_fapi_srs_pdu->timingAdvance = 0xFFFF;

        nr_of_symbols = p_fapi_srs_pdu->numSymbols =
            p_ul_srs_est_struct->nNrOfSymbols;
        for (j = 0; j < nr_of_symbols; j++) {
            temp_sum_wideband_snr += p_ul_srs_est_struct->nWideBandSNR[j];
        }
        wideband_snr = temp_sum_wideband_snr / nr_of_symbols;

        p_fapi_srs_pdu->wideBandSnr = (wideband_snr + 64) * 2;
        num_rept_symbols = p_fapi_srs_pdu->numReportedSymbols = 1;

        for (j = 0; j < num_rept_symbols; j++) {
            p_fapi_symb_snr = &p_fapi_srs_pdu->symbSnr[j];
            num_rbs = p_fapi_symb_snr->numRbs =
                p_ul_srs_est_struct->nNrOfBlocks * 4;

            for (k = 0; k < num_rbs; k++) {
                rb_snr = p_ul_srs_est_struct->nBlockSNR[k / 68][k % 68];
                p_fapi_symb_snr->rbSNR[k] = (rb_snr + 64) * 2;
            }
        }
        p_stats->fapi_stats.fapi_srs_ind_pdus++;

        if(p_fapi_vend_srs_ind) { // Fill vendor ext
            p_fapi_vend_srs_ind->num_pdus = p_phy_srs_ind->nNrOfSrs;
            p_vend_srs_pdu = &p_fapi_vend_srs_ind->srs_pdus[i];
            p_vend_srs_pdu->nr_of_port = p_ul_srs_est_struct->nNrOfPort;
            p_vend_srs_pdu->nr_of_rx_ant = p_ul_srs_est_struct->nNrOfRxAnt;
            p_vend_srs_pdu->nr_of_rbs = p_ul_srs_est_struct->nNrOfRbs;
            p_vend_srs_pdu->is_chan_est_pres =
                p_ul_srs_est_struct->nIsChanEstPres;
            NR5G_FAPI_MEMCPY(p_vend_srs_pdu->p_srs_chan_est,
                sizeof(p_vend_srs_pdu->p_srs_chan_est),
                p_ul_srs_est_struct->pSrsChanEst,
                sizeof(p_ul_srs_est_struct->pSrsChanEst));
        }
    }

    return SUCCESS;
}
