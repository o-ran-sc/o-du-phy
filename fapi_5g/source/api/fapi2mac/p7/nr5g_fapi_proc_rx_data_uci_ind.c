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
 * This file consist of implementation of FAPI UCI.indication on PUSCH message.
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
 *  @param[in]  p_phy_ctx Pointer to PHY Context.
 *  @param[in]  p_phy_rx_ulsch_uci_ind Pointer to IAPI RX_ULSCH_UCI.indication message structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This message includes UCI payload on PUSCH.
 *
**/
uint8_t nr5g_fapi_rx_data_uci_indication(
    bool is_urllc,
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    PRXULSCHUCIIndicationStruct p_phy_rx_ulsch_uci_ind)
{
    uint8_t phy_id;

    fapi_uci_indication_t *p_fapi_uci_ind;
    p_fapi_api_queue_elem_t p_list_elem;
    p_nr5g_fapi_phy_instance_t p_phy_instance = NULL;
    nr5g_fapi_stats_t *p_stats;

    if (NULL == p_phy_ctx) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[UCI.indication on PUSCH] Invalid Phy "
                "Context"));
        return FAILURE;        
    }

    if (NULL == p_phy_rx_ulsch_uci_ind) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[UCI.indication on PUSCH] Invalid Phy "
                "RX_ULSCH_UCI indication"));
        return FAILURE;        
    }

    phy_id = p_phy_rx_ulsch_uci_ind->sSFN_Slot.nCarrierIdx;
    p_phy_instance = &p_phy_ctx->phy_instance[phy_id];
    if (p_phy_instance->phy_id != phy_id) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[UCI.indication on PUSCH] Invalid Phy "
                "instance"));
        return FAILURE;
    }
    
    p_stats = &p_phy_instance->stats;
    p_stats->iapi_stats.iapi_uci_ind++;

    p_list_elem = 
        nr5g_fapi_fapi2mac_create_api_list_elem(FAPI_UCI_INDICATION, 1,
            sizeof(fapi_uci_indication_t));
    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[UCI.indication on PUSCH] Unable to create "
                "list element. Out of memory!!!"));
        return FAILURE;        
    }

    p_fapi_uci_ind = (fapi_uci_indication_t *) (p_list_elem + 1);
    p_fapi_uci_ind->header.msg_id = FAPI_UCI_INDICATION;
    p_fapi_uci_ind->header.length = sizeof(fapi_uci_indication_t);

    if (nr5g_fapi_rx_data_uci_indication_to_fapi_translation(p_phy_instance,
            p_phy_rx_ulsch_uci_ind, p_fapi_uci_ind)) {
        NR5G_FAPI_LOG(ERROR_LOG,
            ("[UCI.indication on PUSCH] FAPI to L1 " "translation failed"));
        return FAILURE;
    }

    nr5g_fapi_fapi2mac_add_api_to_list(phy_id, p_list_elem, is_urllc);

    p_stats->fapi_stats.fapi_uci_ind++;

    NR5G_FAPI_LOG(DEBUG_LOG, ("[UCI.indication on PUSCH][%u][%u,%u,%u] is_urllc %u",
        p_phy_instance->phy_id,p_phy_rx_ulsch_uci_ind->sSFN_Slot.nSFN,
        p_phy_rx_ulsch_uci_ind->sSFN_Slot.nSlot,
        p_phy_rx_ulsch_uci_ind->sSFN_Slot.nSym, is_urllc));

    return SUCCESS;
}

/** @ingroup group_source_api_p7_fapi2mac_proc
 *
 *  @param[in]  p_phy_instance Pointer to PHY instance.
 *  @param[in]   p_phy_uci_ind Pointer to IAPI RX_ULSCH_UCI.indication structure.
 *  @param[out]  p_fapi_uci_ind Pointer to FAPI UCI.indication structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This function converts IAPI RX_ULSCH_UCI.indication to FAPI UCI.indication
 *  structure.
 *
**/
uint8_t nr5g_fapi_rx_data_uci_indication_to_fapi_translation(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    PRXULSCHUCIIndicationStruct p_phy_rx_ulsch_uci_ind,
    fapi_uci_indication_t * p_fapi_uci_ind)
{
    uint8_t num_uci, i;
    uint8_t nUciDetected, nUciCsiP1Detected, nUciCsiP2Detected;

    PULSCHUCIPDUDataStruct p_phy_uci_pdu_data_struct;
    fapi_uci_pdu_info_t *p_fapi_uci_pdu_info;
    fapi_uci_o_pusch_t *p_uci_push;
    nr5g_fapi_stats_t *p_stats;

    p_stats = &p_phy_instance->stats;

    p_fapi_uci_ind->sfn = p_phy_rx_ulsch_uci_ind->sSFN_Slot.nSFN;
    p_fapi_uci_ind->slot = p_phy_rx_ulsch_uci_ind->sSFN_Slot.nSlot;
    
    num_uci = p_fapi_uci_ind->numUcis = p_phy_rx_ulsch_uci_ind->nUlschUci;

    for (i = 0; i < num_uci; i++) {
        p_stats->iapi_stats.iapi_uci_ind_pdus++;

        p_fapi_uci_pdu_info = &p_fapi_uci_ind->uciPdu[i];
        p_phy_uci_pdu_data_struct =
            &p_phy_rx_ulsch_uci_ind->sULSCHUCIPDUDataStruct[i];

        p_fapi_uci_pdu_info->pduType = 0;
        p_fapi_uci_pdu_info->pduSize = sizeof(fapi_uci_o_pusch_t);
        
        p_uci_push = &p_fapi_uci_pdu_info->uci.uciPusch;
        memset(p_uci_push, 0, sizeof(fapi_uci_o_pusch_t));

        p_uci_push->handle = p_phy_uci_pdu_data_struct->nUEId;
        p_uci_push->pduBitmap = 0;
        p_uci_push->ul_cqi = 0xff;
        p_uci_push->rnti = p_phy_uci_pdu_data_struct->nRNTI;
        p_uci_push->timingAdvance = 0xffff;
        p_uci_push->rssi = 0xffff;
        
        nUciDetected = p_phy_uci_pdu_data_struct->nUciDetected;
        if (nUciDetected) {
            p_uci_push->pduBitmap |= 0x02;
            p_uci_push->harqInfo.harqCrc = p_phy_uci_pdu_data_struct->nUciCrc;
            p_uci_push->harqInfo.harqBitLen =
                p_phy_uci_pdu_data_struct->nPduUciAckLen;
            NR5G_FAPI_MEMCPY(p_uci_push->harqInfo.harqPayload,
                sizeof(uint8_t) * FAPI_MAX_HARQ_INFO_LEN_BYTES,
                p_phy_uci_pdu_data_struct->nUciAckBits,
                sizeof(uint8_t) * FAPI_MAX_HARQ_INFO_LEN_BYTES);
        }

        nUciCsiP1Detected = p_phy_uci_pdu_data_struct->nUciCsiP1Detected;
        if (nUciCsiP1Detected) {
            p_uci_push->pduBitmap |= 0x04;
            p_uci_push->csiPart1info.csiPart1Crc =
                p_phy_uci_pdu_data_struct->nUciCsiP1Crc;
            p_uci_push->csiPart1info.csiPart1BitLen =
                p_phy_uci_pdu_data_struct->nPduUciCsiP1Len;
            NR5G_FAPI_MEMCPY(p_uci_push->csiPart1info.csiPart1Payload,
                sizeof(uint8_t) * FAPI_MAX_CSI_PART1_DATA_BYTES,
                p_phy_uci_pdu_data_struct->nUciCsiP1Bits,
                sizeof(uint8_t) * FAPI_MAX_CSI_PART1_DATA_BYTES);
        }

        nUciCsiP2Detected = p_phy_uci_pdu_data_struct->nUciCsiP2Detected;
        if (nUciCsiP2Detected) {
            p_uci_push->pduBitmap |= 0x08;
            p_uci_push->csiPart2info.csiPart2Crc =
                p_phy_uci_pdu_data_struct->nUciCsiP2Crc;
            p_uci_push->csiPart2info.csiPart2BitLen =
                p_phy_uci_pdu_data_struct->nPduUciCsiP2Len;
            NR5G_FAPI_MEMCPY(p_uci_push->csiPart2info.csiPart2Payload,
                sizeof(uint8_t) * FAPI_MAX_CSI_PART2_DATA_BYTES,
                p_phy_uci_pdu_data_struct->nUciCsiP2Bits,
                sizeof(uint8_t) * FAPI_MAX_CSI_PART2_DATA_BYTES);
        }
        p_stats->fapi_stats.fapi_uci_ind_pdus++;
    }

    return SUCCESS;
}