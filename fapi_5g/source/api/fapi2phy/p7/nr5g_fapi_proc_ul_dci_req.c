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
 * This file consist of implementation of FAPI UL_DCI.request message.
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
 *  @param[in]  p_fapi_req Pointer to FAPI UL_DCI.request message structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This message includes DCI content used for the scheduling of PUSCH.
 *
**/
uint8_t nr5g_fapi_ul_dci_request(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_ul_dci_req_t * p_fapi_req,
    fapi_vendor_msg_t * p_fapi_vendor_msg)
{
    PULDCIRequestStruct p_ia_ul_dci_req;
    PMAC2PHY_QUEUE_EL p_list_elem;
    nr5g_fapi_stats_t *p_stats;

    if (NULL == p_phy_instance) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[UL_DCI.request] Invalid " "phy instance"));
        return FAILURE;
    }
    p_stats = &p_phy_instance->stats;
    p_stats->fapi_stats.fapi_ul_dci_req++;

    if (NULL == p_fapi_req) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[UL_DCI.request] Invalid fapi " "message"));
        return FAILURE;
    }

    p_list_elem = nr5g_fapi_fapi2phy_create_api_list_elem((uint8_t)
        MSG_TYPE_PHY_UL_DCI_REQ, 1, (uint32_t) sizeof(ULDCIRequestStruct));
    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[UL_DCI.request] Unable to create "
                "list element. Out of memory!!!"));
        return FAILURE;
    }

    p_ia_ul_dci_req = (PULDCIRequestStruct) (p_list_elem + 1);
    NR5G_FAPI_MEMSET(p_ia_ul_dci_req, sizeof(PULDCIRequestStruct), 0,
        sizeof(PULDCIRequestStruct));
    p_ia_ul_dci_req->sMsgHdr.nMessageType = MSG_TYPE_PHY_UL_DCI_REQ;
    p_ia_ul_dci_req->sMsgHdr.nMessageLen =
        (uint16_t) sizeof(ULDCIRequestStruct);
    p_ia_ul_dci_req->sSFN_Slot.nSFN = p_fapi_req->sfn;
    p_ia_ul_dci_req->sSFN_Slot.nSlot = p_fapi_req->slot;
    p_ia_ul_dci_req->sSFN_Slot.nCarrierIdx = p_phy_instance->phy_id;

    if (FAILURE == nr5g_fapi_ul_dci_req_to_phy_translation(p_phy_instance,
            p_fapi_req, p_ia_ul_dci_req)) {
        nr5g_fapi_fapi2phy_destroy_api_list_elem(p_list_elem);
        NR5G_FAPI_LOG(DEBUG_LOG, ("[UL_DCI.request][%d][%d,%d] Not Sent",
                p_phy_instance->phy_id, p_ia_ul_dci_req->sSFN_Slot.nSFN,
                p_ia_ul_dci_req->sSFN_Slot.nSlot));
        return FAILURE;
    }
    nr5g_fapi_fapi2phy_add_to_api_list(is_urllc, p_list_elem);

    p_stats->iapi_stats.iapi_ul_dci_req++;


    if (NULL != p_fapi_vendor_msg) {
        nr5g_fapi_ul_dci_req_to_phy_translation_vendor_ext(p_phy_instance,
                                                        p_fapi_vendor_msg,
                                                        p_ia_ul_dci_req);
    }

    NR5G_FAPI_LOG(DEBUG_LOG, ("[UL_DCI.request][%u][%u,%u,%u] is_urllc %u",
            p_phy_instance->phy_id,
            p_ia_ul_dci_req->sSFN_Slot.nSFN, p_ia_ul_dci_req->sSFN_Slot.nSlot,
            p_ia_ul_dci_req->sSFN_Slot.nSym, is_urllc));

    return SUCCESS;
}

 /** @ingroup group_source_api_p7_fapi2phy_proc
 *
 *  @param[in]   p_fapi_req Pointer to FAPI UL_DCI.request structure.
 *  @param[out]  p_ia_ul_dci_req Pointer to IAPI UL_DCI.request structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This function converts FAPI UL_DCI.request to IAPI UL_DCI.request
 *  structure.
 *
**/
uint8_t nr5g_fapi_ul_dci_req_to_phy_translation(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_ul_dci_req_t * p_fapi_req,
    PULDCIRequestStruct p_ia_ul_dci_req)
{
    int idx;
    int ruidx;
    fapi_dci_pdu_t *p_fapi_dci_pdu;
    DCIPDUStruct *p_ia_dci_pdu;
    nr5g_fapi_stats_t *p_stats;
    uint8_t *p_ia_curr, *p_freq_dom_res = NULL;

    p_stats = &p_phy_instance->stats;

    p_ia_ul_dci_req->nDCI = p_fapi_req->numPdus;

    p_ia_curr = (uint8_t *) p_ia_ul_dci_req->sULDCIPDU;

    for (idx = 0; idx < p_ia_ul_dci_req->nDCI; idx++) {
        p_stats->fapi_stats.fapi_ul_dci_pdus++;
        p_fapi_dci_pdu = &p_fapi_req->pdus[idx];
        p_ia_dci_pdu = (DCIPDUStruct *) p_ia_curr;
        p_ia_dci_pdu->sPDUHdr.nPDUType = DL_PDU_TYPE_DCI;
        p_ia_dci_pdu->sPDUHdr.nPDUSize = RUP32B(sizeof(DCIPDUStruct));
        p_ia_dci_pdu->nRNTI = p_fapi_dci_pdu->pdcchPduConfig.dlDci[0].rnti;
        p_ia_dci_pdu->nBWPSize = p_fapi_dci_pdu->pdcchPduConfig.bwpSize;
        p_ia_dci_pdu->nBWPStart = p_fapi_dci_pdu->pdcchPduConfig.bwpStart;
        p_ia_dci_pdu->nSubcSpacing =
            p_fapi_dci_pdu->pdcchPduConfig.subCarrierSpacing;
        p_ia_dci_pdu->nCpType = p_fapi_dci_pdu->pdcchPduConfig.cyclicPrefix;
        p_freq_dom_res = &p_fapi_dci_pdu->pdcchPduConfig.freqDomainResource[0];
        p_ia_dci_pdu->nFreqDomain[0] =
            ((uint32_t) (p_freq_dom_res[0])) |
            (((uint32_t) (p_freq_dom_res[1])) << 8) |
            (((uint32_t) (p_freq_dom_res[2])) << 16) |
            (((uint32_t) (p_freq_dom_res[3])) << 24);
        p_ia_dci_pdu->nFreqDomain[1] =
            ((uint32_t) (p_freq_dom_res[4])) |
            (((uint32_t) (p_freq_dom_res[5])) << 8);
        p_ia_dci_pdu->nStartSymbolIndex =
            p_fapi_dci_pdu->pdcchPduConfig.startSymbolIndex;
        p_ia_dci_pdu->nNrOfSymbols =
            p_fapi_dci_pdu->pdcchPduConfig.durationSymbols;
        p_ia_dci_pdu->nCCEToREGType =
            p_fapi_dci_pdu->pdcchPduConfig.cceRegMappingType;
        p_ia_dci_pdu->nREGBundleSize =
            p_fapi_dci_pdu->pdcchPduConfig.regBundleSize;
        p_ia_dci_pdu->nShift = p_fapi_dci_pdu->pdcchPduConfig.shiftIndex;
        p_ia_dci_pdu->nScid =
            p_fapi_dci_pdu->pdcchPduConfig.dlDci[0].scramblingId;
        p_ia_dci_pdu->nCCEStartIndex =
            p_fapi_dci_pdu->pdcchPduConfig.dlDci[0].cceIndex;
        p_ia_dci_pdu->nAggrLvl =
            p_fapi_dci_pdu->pdcchPduConfig.dlDci[0].aggregationLevel;
        p_ia_dci_pdu->nInterleaveSize =
            p_fapi_dci_pdu->pdcchPduConfig.interleaverSize;
        p_ia_dci_pdu->nCoreSetType = p_fapi_dci_pdu->pdcchPduConfig.coreSetType;
        p_ia_dci_pdu->nRNTIScramb =
            p_fapi_dci_pdu->pdcchPduConfig.dlDci[0].scramblingRnti;
        p_ia_dci_pdu->nTotalBits =
            p_fapi_dci_pdu->pdcchPduConfig.dlDci[0].payloadSizeBits;


        if (USE_VENDOR_EPREXSSB != p_phy_instance->phy_config.use_vendor_EpreXSSB)
        {
        p_ia_dci_pdu->nEpreRatioOfPDCCHToSSB =
                nr5g_fapi_calculate_nEpreRatioOfPDCCHToSSB(p_fapi_dci_pdu->
                    pdcchPduConfig.dlDci[0].beta_pdcch_1_0);
        p_ia_dci_pdu->nEpreRatioOfDmrsToSSB =
                nr5g_fapi_calculate_nEpreRatioOfDmrsToSSB(p_fapi_dci_pdu->
                    pdcchPduConfig.dlDci[0].powerControlOffsetSS);
        }

        p_ia_dci_pdu->nTotalBits =
            p_fapi_dci_pdu->pdcchPduConfig.dlDci[0].payloadSizeBits;
        if (FAILURE == NR5G_FAPI_MEMCPY(p_ia_dci_pdu->nDciBits,
                sizeof(uint8_t) * MAX_DCI_BIT_BYTE_LEN,
                p_fapi_dci_pdu->pdcchPduConfig.dlDci[0].payload,
                sizeof(uint8_t) * MAX_DCI_BIT_BYTE_LEN)) {
            NR5G_FAPI_LOG(ERROR_LOG,
                ("UL_DCI Pdu: RNTI: %d -- DCI Bits copy" " failed.",
                    p_fapi_dci_pdu->pdcchPduConfig.dlDci[0].rnti));
            return FAILURE;
        }
        p_ia_dci_pdu->nID = p_ia_dci_pdu->nScid;
        p_ia_dci_pdu->nNrofTxRU = 0x0;
        p_ia_dci_pdu->nBeamId = 0x0;

        for (ruidx = 0; ruidx < MAX_TXRU_NUM; ruidx++) {
            p_ia_dci_pdu->nTxRUIdx[ruidx] = 0;
        }
        p_ia_curr += RUP32B(sizeof(DCIPDUStruct));
    }

    p_stats->iapi_stats.iapi_ul_dci_pdus++;
    return SUCCESS;
}

 /** @ingroup group_source_api_p7_fapi2phy_proc
 *
 *  @param[in]   p_fapi_vendor_msg  Pointer to FAPI UL_DCI.request vendor message.
 *  @param[out]  p_ia_ul_dci_req    Pointer to IAPI UL_DCI.request structure.
 *  
 *  @return     no return.
 *
 *  @description
 *  This function fills fields for UL_DCI.request structure that come from
 *  a vendor extension.
 *
**/
void nr5g_fapi_ul_dci_req_to_phy_translation_vendor_ext(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_vendor_msg_t * p_fapi_vendor_msg,
    PULDCIRequestStruct p_ia_ul_dci_req)
{
    int idx = 0;

    fapi_vendor_dci_pdu_t *p_vendor_dci_pdu;
    DCIPDUStruct *p_ia_dci_pdu;
    uint8_t *p_ia_curr = NULL;

    p_ia_ul_dci_req->sSFN_Slot.nSym = p_fapi_vendor_msg->p7_req_vendor.ul_dci_req.sym;

    p_ia_curr = (uint8_t *) p_ia_ul_dci_req->sULDCIPDU;

    for (idx = 0; idx < p_ia_ul_dci_req->nDCI; idx++) {
        p_ia_dci_pdu = (DCIPDUStruct *) p_ia_curr;
        if (USE_VENDOR_EPREXSSB == p_phy_instance->phy_config.use_vendor_EpreXSSB)
        {
            p_vendor_dci_pdu = &p_fapi_vendor_msg->p7_req_vendor.ul_dci_req.pdus[idx];
            p_ia_dci_pdu->nEpreRatioOfPDCCHToSSB = p_vendor_dci_pdu->
                pdcch_pdu_config.dl_dci[0].epre_ratio_of_pdcch_to_ssb;
            p_ia_dci_pdu->nEpreRatioOfDmrsToSSB = p_vendor_dci_pdu->
                pdcch_pdu_config.dl_dci[0].epre_ratio_of_dmrs_to_ssb;
        }
        p_ia_curr += RUP32B(sizeof(DCIPDUStruct));
    }
}