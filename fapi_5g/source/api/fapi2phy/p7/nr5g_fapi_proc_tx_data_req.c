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
 * This file consist of implementation of FAPI TX_Data.request message.
 *
 **/

#include "nr5g_fapi_framework.h"
#include "gnb_l1_l2_api.h"
#include "nr5g_fapi_fapi2mac_api.h"
#include "nr5g_fapi_fapi2phy_api.h"
#include "nr5g_fapi_fapi2phy_p7_proc.h"
#include "nr5g_fapi_fapi2phy_p7_pvt_proc.h"

 /** @ingroup group_source_api_p7_fapi2phy_proc
 *
 *  @param[in]  p_phy_instance Pointer to PHY instance.
 *  @param[in]  p_fapi_req Pointer to FAPI TX_Data.request message structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This message contains the MAC PDU data for transmission over the air 
 *  interface. The PDUs described in this message must follow the same order 
 *  as DL_TTI.request. 
 *
**/
uint8_t nr5g_fapi_tx_data_request(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_tx_data_req_t * p_fapi_req,
    fapi_vendor_msg_t * p_fapi_vendor_msg)
{
    PTXRequestStruct p_ia_tx_req;
    PMAC2PHY_QUEUE_EL p_list_elem;
    nr5g_fapi_stats_t *p_stats;
    UNUSED(p_fapi_vendor_msg);

    if (NULL == p_phy_instance) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[TX_Data.request] Invalid " "phy instance"));
        return FAILURE;
    }

    if (NULL == p_fapi_req) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[TX_Data.request] Invalid fapi " "message"));
        return FAILURE;
    }

    p_stats = &p_phy_instance->stats;
    p_stats->fapi_stats.fapi_tx_data_req++;
    p_list_elem = nr5g_fapi_fapi2phy_create_api_list_elem((uint8_t)
        MSG_TYPE_PHY_TX_REQ, 1, (uint32_t) sizeof(TXRequestStruct));
    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[TX_Data.request] Unable to create "
                "list element. Out of memory!!!"));
        return FAILURE;
    }

    p_ia_tx_req = (PTXRequestStruct) (p_list_elem + 1);
    nr5g_fapi_tx_data_req_to_phy_translation(p_phy_instance, p_fapi_req,
        p_ia_tx_req);
    nr5g_fapi_fapi2phy_add_to_api_list(p_list_elem);

    p_stats->iapi_stats.iapi_tx_req++;
    NR5G_FAPI_LOG(DEBUG_LOG, ("[TX_Data.request][%d][%d,%d]",
            p_phy_instance->phy_id, p_ia_tx_req->sSFN_Slot.nSFN,
            p_ia_tx_req->sSFN_Slot.nSlot));

    return SUCCESS;
}

 /** @ingroup group_source_api_p7_fapi2phy_proc
 *
 *  @param[in]  p_fapi_req Pointer to FAPI TX_Data.request structure.
 *  @param[in]  p_ia_tx_req Pointer to IAPI TX_Data.request structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This function converts FAPI TX_Data.request to IAPI TX.request
 *  structure.
 *
**/
uint8_t nr5g_fapi_tx_data_req_to_phy_translation(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_tx_data_req_t * p_fapi_req,
    PTXRequestStruct p_phy_req)
{
    uint16_t idx;
    uint32_t tlv;
    fapi_tx_pdu_desc_t *p_fapi_pdu = NULL;
    PDLPDUDataStruct p_phy_pdu = NULL;

    p_phy_req->sMsgHdr.nMessageType = MSG_TYPE_PHY_TX_REQ;
    p_phy_req->sMsgHdr.nMessageLen = (uint16_t) sizeof(TXRequestStruct);
    p_phy_req->sSFN_Slot.nCarrierIdx = p_phy_instance->phy_id;
    p_phy_req->sSFN_Slot.nSFN = p_fapi_req->sfn;
    p_phy_req->sSFN_Slot.nSlot = p_fapi_req->slot;
    p_phy_req->nPDU = p_fapi_req->num_pdus;
    p_phy_pdu = (PDLPDUDataStruct) (p_phy_req + 1);
    for (idx = 0; idx < p_fapi_req->num_pdus; idx++) {
        p_fapi_pdu = &p_fapi_req->pdu_desc[idx];
        for (tlv = 0; tlv < p_fapi_pdu->num_tlvs; tlv++) {
            p_phy_pdu->nPduIdx = p_fapi_pdu->pdu_index;
            p_phy_pdu->nPduLen1 = 0;
            p_phy_pdu->nPduLen2 = 0;
            p_phy_pdu->pPayload1 = NULL;
            p_phy_pdu->pPayload2 = NULL;
            if (tlv == 0) {
                p_phy_pdu->nPduLen1 = p_fapi_pdu->tlvs[tlv].tl.length;
                p_phy_pdu->pPayload1 = p_fapi_pdu->tlvs[tlv].value;
            } else {
                p_phy_pdu->nPduLen2 = p_fapi_pdu->tlvs[tlv].tl.length;
                p_phy_pdu->pPayload2 = p_fapi_pdu->tlvs[tlv].value;
            }
        }
        p_phy_pdu++;
    }
    return SUCCESS;
}
