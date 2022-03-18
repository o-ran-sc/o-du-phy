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
#include "nr5g_fapi_memory.h"

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
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_tx_data_req_t * p_fapi_req,
    fapi_vendor_msg_t * p_fapi_vendor_msg)
{
    PTXRequestStruct p_ia_tx_req;
    PMAC2PHY_QUEUE_EL p_list_elem;
    nr5g_fapi_stats_t *p_stats;

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
    nr5g_fapi_tx_data_req_to_phy_translation(p_phy_instance, p_fapi_req, p_fapi_vendor_msg, p_ia_tx_req);
    nr5g_fapi_fapi2phy_add_to_api_list(is_urllc, p_list_elem);

    p_stats->iapi_stats.iapi_tx_req++;
    NR5G_FAPI_LOG(DEBUG_LOG, ("[TX_Data.request][%u][%u,%u,%u] is_urllc %u",
        p_phy_instance->phy_id,
        p_ia_tx_req->sSFN_Slot.nSFN, p_ia_tx_req->sSFN_Slot.nSlot,
        p_ia_tx_req->sSFN_Slot.nSym, is_urllc));

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
    fapi_vendor_msg_t * p_fapi_vendor_msg,
    PTXRequestStruct p_phy_req)
{
#define GATHER_SIZE 3
#define GATHER_PDU_IDX 0
#define GATHER_CW1 1
#define GATHER_CW2 2
    int gather[FAPI_MAX_NUMBER_DL_PDUS_PER_TTI][GATHER_SIZE];
    int gathered_count = 0;
    uint8_t *tag;
    uint32_t length;
    uint16_t idx, count, found;
    fapi_tx_pdu_desc_t *p_fapi_pdu = NULL;
    PDLPDUDataStruct p_phy_pdu = NULL;

    NR5G_FAPI_MEMSET(gather,
        (sizeof(int) * FAPI_MAX_NUMBER_DL_PDUS_PER_TTI * 3), -1,
        (sizeof(int) * FAPI_MAX_NUMBER_DL_PDUS_PER_TTI * 3));
    NR5G_FAPI_MEMSET(p_phy_req, sizeof(TXRequestStruct), 0,
        sizeof(TXRequestStruct));
    p_phy_req->sMsgHdr.nMessageType = MSG_TYPE_PHY_TX_REQ;
    p_phy_req->sMsgHdr.nMessageLen = (uint16_t) sizeof(TXRequestStruct);
    p_phy_req->sSFN_Slot.nCarrierIdx = p_phy_instance->phy_id;
    p_phy_req->sSFN_Slot.nSFN = p_fapi_req->sfn;
    p_phy_req->sSFN_Slot.nSlot = p_fapi_req->slot;

    if (NULL != p_fapi_vendor_msg) {
        nr5g_fapi_tx_data_req_to_phy_translation_vendor_ext(p_fapi_vendor_msg,
                                                            p_phy_req);
    }

    p_phy_req->nPDU = p_fapi_req->num_pdus;
    p_phy_pdu = (PDLPDUDataStruct) (p_phy_req + 1);

    for (idx = 0; idx < p_fapi_req->num_pdus; idx++) {
        found = FALSE;
        p_fapi_pdu = &p_fapi_req->pdu_desc[idx];
        for (count = 0; count < gathered_count; count++) {
            if (gather[count][GATHER_PDU_IDX] == p_fapi_pdu->pdu_index) {
                found = TRUE;
                break;
            }
        }

        if (found) {
            gather[count][GATHER_CW2] = idx;
        } else {
            if (gathered_count < FAPI_MAX_NUMBER_DL_PDUS_PER_TTI) {
                gather[gathered_count][GATHER_PDU_IDX] = p_fapi_pdu->pdu_index;
                gather[gathered_count][GATHER_CW1] = idx;
                gathered_count++;
            } else {
                NR5G_FAPI_LOG(ERROR_LOG,
                    ("exceeded Max DL Pdus supported per tti: %d [%d] ",
                        gathered_count, FAPI_MAX_NUMBER_DL_PDUS_PER_TTI));
            }
        }
    }

    for (count = 0; count < gathered_count; count++) {
        p_phy_pdu->nPduLen1 = 0;
        p_phy_pdu->nPduLen2 = 0;
        p_phy_pdu->pPayload1 = NULL;
        p_phy_pdu->pPayload2 = NULL;
        if (gather[count][GATHER_CW1] >= 0) {
            p_fapi_pdu = &p_fapi_req->pdu_desc[gather[count][GATHER_CW1]];
            p_phy_pdu->nPduIdx = p_fapi_pdu->pdu_index;
            tag = (uint8_t *) & p_fapi_pdu->tlvs[0].tl.tag;
            if (*tag == FAPI_TX_DATA_PTR_TO_PAYLOAD_64) {
                tag++;
                length =
                    (((uint32_t) *
                        tag) << 16) | (uint32_t) p_fapi_pdu->tlvs[0].tl.length;
                p_phy_pdu->nPduLen1 = length;
                p_phy_pdu->pPayload1 = p_fapi_pdu->tlvs[0].value;
            } else {
                NR5G_FAPI_LOG(ERROR_LOG,
                    ("Only 64 bit Ptr to Payload in TX_DATA.req is supported"));
            }
        }
        if (gather[count][GATHER_CW2] >= 0) {
            p_fapi_pdu = &p_fapi_req->pdu_desc[gather[count][GATHER_CW2]];
            tag = (uint8_t *) & p_fapi_pdu->tlvs[0].tl.tag;
            if (*tag == FAPI_TX_DATA_PTR_TO_PAYLOAD_64) {
                tag++;
                length =
                    (((uint32_t) *
                        tag) << 16) | (uint32_t) p_fapi_pdu->tlvs[0].tl.length;
                p_phy_pdu->nPduLen2 = length;
                p_phy_pdu->pPayload2 = p_fapi_pdu->tlvs[0].value;
            } else {
                NR5G_FAPI_LOG(ERROR_LOG,
                    ("Only 64 bit Ptr to Payload in TX_DATA.req is supported"));
            }
        }
        p_phy_pdu++;
    }
    return SUCCESS;
}

 /** @ingroup group_source_api_p7_fapi2phy_proc
 *
 *  @param[in]  p_fapi_vendor_msg Pointer to FAPI TX_Data.request structure.
 *  @param[in]  p_ia_tx_req       Pointer to IAPI TX_Data.request structure.
 *  
 *  @return     no return.
 *
 *  @description
 *  This function fills fields for TX.Data structure that come from
 *  a vendor extension.
 *
**/
void nr5g_fapi_tx_data_req_to_phy_translation_vendor_ext(
    fapi_vendor_msg_t * p_fapi_vendor_msg,
    PTXRequestStruct p_phy_req)
{
    p_phy_req->sSFN_Slot.nSym = p_fapi_vendor_msg->p7_req_vendor.tx_data_req.sym;
}