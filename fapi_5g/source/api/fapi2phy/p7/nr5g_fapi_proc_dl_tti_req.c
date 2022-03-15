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
 * This file consist of implementation of FAPI DL_TTI.request message.
 *
 **/
#include <rte_memcpy.h>
#include "nr5g_fapi_framework.h"
#include "gnb_l1_l2_api.h"
#include "nr5g_fapi_dpdk.h"
#include "nr5g_fapi_fapi2mac_api.h"
#include "nr5g_fapi_fapi2phy_api.h"
#include "nr5g_fapi_fapi2phy_p7_proc.h"
#include "nr5g_fapi_fapi2phy_p7_pvt_proc.h"
#include "nr5g_fapi_memory.h"

/** @ingroup group_source_api_p5_fapi2phy_proc
 *
 *  @param[in]  p_phy_instance Pointer to PHY instance.
 *  @param[in]  p_fapi_req Pointer to FAPI DL_TTI.request message structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This message indicates the control information for a downlink slot. 
 *
**/
uint8_t nr5g_fapi_dl_tti_request(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_dl_tti_req_t * p_fapi_req,
    fapi_vendor_msg_t * p_fapi_vendor_msg)
{
    PDLConfigRequestStruct p_ia_dl_config_req;
    PMAC2PHY_QUEUE_EL p_list_elem;
    nr5g_fapi_stats_t *p_stats;

    if (NULL == p_phy_instance) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[DL_TTI.request] Invalid " "Phy Instance"));
        return FAILURE;
    }

    p_stats = &p_phy_instance->stats;
    p_stats->fapi_stats.fapi_dl_tti_req++;

    if (NULL == p_fapi_req) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[DL_TTI.request] Invalid fapi " "message"));
        return FAILURE;
    }

    p_list_elem = nr5g_fapi_fapi2phy_create_api_list_elem((uint8_t)
        MSG_TYPE_PHY_DL_CONFIG_REQ, 1,
        (uint32_t) sizeof(DLConfigRequestStruct));
    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[DL_TTI.request] Unable to create "
                "list element. Out of memory!!!"));
        return FAILURE;
    }

    p_ia_dl_config_req = (PDLConfigRequestStruct) (p_list_elem + 1);
    if (FAILURE == nr5g_fapi_dl_tti_req_to_phy_translation(p_phy_instance,
            p_fapi_req, p_fapi_vendor_msg, p_ia_dl_config_req)) {
        nr5g_fapi_fapi2phy_destroy_api_list_elem(p_list_elem);
        NR5G_FAPI_LOG(DEBUG_LOG, ("[DL_TTI.request][%d][%d,%d] Not Sent",
                p_phy_instance->phy_id, p_ia_dl_config_req->sSFN_Slot.nSFN,
                p_ia_dl_config_req->sSFN_Slot.nSlot));
        return FAILURE;
    }
    /* Add element to send list */
    nr5g_fapi_fapi2phy_add_to_api_list(is_urllc, p_list_elem);

    p_stats->iapi_stats.iapi_dl_config_req++;
    NR5G_FAPI_LOG(DEBUG_LOG, ("[DL_TTI.request][%u][%u,%u,%u] is_urllc %u",
        p_phy_instance->phy_id,
        p_ia_dl_config_req->sSFN_Slot.nSFN, p_ia_dl_config_req->sSFN_Slot.nSlot,
        p_ia_dl_config_req->sSFN_Slot.nSym, is_urllc));

    return SUCCESS;
}

 /** @ingroup group_source_api_p7_fapi2phy_proc
 *
 *  @param[in]   p_fapi_req Pointer to FAPI DL_TTI.request structure.
 *  @param[out]  p_ia_dl_config_req Pointer to IAPI DL_TTI.request structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This function converts FAPI DL_TTI.request to IAPI DL_Config.request
 *  structure.
 *
**/
uint8_t nr5g_fapi_dl_tti_req_to_phy_translation(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_dl_tti_req_t * p_fapi_req,
    fapi_vendor_msg_t * p_fapi_vendor_msg,
    PDLConfigRequestStruct p_phy_req)
{
    int idx = 0, nDCI = 0, jdx = 0;
    uint32_t pdsch_size, pdcch_size, pbch_size, csirs_size, total_size;
    nr5g_fapi_stats_t *p_stats;

    fapi_dl_tti_req_pdu_t *p_fapi_dl_tti_req_pdu = NULL;
    fapi_dl_pdcch_pdu_t *p_pdcch_pdu = NULL;
    fapi_dl_pdsch_pdu_t *p_pdsch_pdu = NULL;
    fapi_dl_csi_rs_pdu_t *p_csi_rs_pdu = NULL;
    fapi_dl_ssb_pdu_t *p_ssb_pdu = NULL;
    fapi_ue_info_t *p_ueGrpInfo = NULL;

    PPDUStruct pPduStruct = NULL;
    PDLSCHPDUStruct p_dlsch_pdu = NULL;
    PDCIPDUStruct p_dci_pdu = NULL;
    PBCHPDUStruct p_bch_pdu = NULL;
    PCSIRSPDUStruct pCSIRSPdu = NULL;
    PPDSCHGroupInfoStruct pPDSCHGroupInfoStruct = NULL;

    total_size = sizeof(DLConfigRequestStruct);
    pdsch_size = RUP32B(sizeof(DLSCHPDUStruct));
    pdcch_size = RUP32B(sizeof(DCIPDUStruct));
    pbch_size = RUP32B(sizeof(BCHPDUStruct));
    csirs_size = RUP32B(sizeof(CSIRSPDUStruct));

    p_stats = &p_phy_instance->stats;
    NR5G_FAPI_MEMSET(p_phy_req, sizeof(DLConfigRequestStruct), 0, total_size);
    p_phy_req->sMsgHdr.nMessageType = MSG_TYPE_PHY_DL_CONFIG_REQ;
    p_phy_req->sSFN_Slot.nCarrierIdx = p_phy_instance->phy_id;
    p_phy_req->nGroup = p_fapi_req->nGroup;
    p_phy_req->nPDU = p_fapi_req->nPdus;
    p_phy_req->sSFN_Slot.nSFN = p_fapi_req->sfn;
    p_phy_req->sSFN_Slot.nSlot = p_fapi_req->slot;

    for (idx = 0; idx < p_phy_req->nGroup; ++idx) {
        p_ueGrpInfo = &p_fapi_req->ue_grp_info[idx];
        pPDSCHGroupInfoStruct = &p_phy_req->sPDSCHGroupInfoStruct[idx];
        pPDSCHGroupInfoStruct->nUE = p_ueGrpInfo->nUe;
        pPDSCHGroupInfoStruct->rsv1[0] = 0;
        pPDSCHGroupInfoStruct->rsv1[1] = 0;
        pPDSCHGroupInfoStruct->rsv1[2] = 0;
        for (jdx = 0; jdx < p_ueGrpInfo->nUe; ++jdx) {
            pPDSCHGroupInfoStruct->nPduIdx[jdx] = p_ueGrpInfo->pduIdx[jdx];
        }
    }

    pPduStruct = p_phy_req->sDLPDU;
    for (idx = 0; idx < p_phy_req->nPDU; ++idx) {
        p_stats->fapi_stats.fapi_dl_tti_pdus++;
        p_fapi_dl_tti_req_pdu = &p_fapi_req->pdus[idx];
        switch (p_fapi_dl_tti_req_pdu->pduType) {
            case FAPI_PDCCH_PDU_TYPE:
                nDCI++;
                p_dci_pdu = (PDCIPDUStruct) pPduStruct;
                NR5G_FAPI_MEMSET(p_dci_pdu, RUP32B(sizeof(DCIPDUStruct)), 0,
                    pdcch_size);
                p_pdcch_pdu = &p_fapi_dl_tti_req_pdu->pdu.pdcch_pdu;
                p_dci_pdu->sPDUHdr.nPDUType = DL_PDU_TYPE_DCI;
                p_dci_pdu->sPDUHdr.nPDUSize = pdcch_size;
                total_size += pdcch_size;
                nr5g_fapi_fill_dci_pdu(p_phy_instance, p_pdcch_pdu, p_dci_pdu);
                break;

            case FAPI_PDSCH_PDU_TYPE:
                p_dlsch_pdu = (PDLSCHPDUStruct) pPduStruct;
                p_pdsch_pdu = &p_fapi_dl_tti_req_pdu->pdu.pdsch_pdu;
                NR5G_FAPI_MEMSET(p_dlsch_pdu, RUP32B(sizeof(DLSCHPDUStruct)), 0,
                    pdsch_size);
                p_dlsch_pdu->sPDUHdr.nPDUType = DL_PDU_TYPE_DLSCH;
                p_dlsch_pdu->sPDUHdr.nPDUSize = pdsch_size;
                total_size += pdsch_size;
                nr5g_fapi_fill_pdsch_pdu(p_phy_instance, p_pdsch_pdu, p_dlsch_pdu);
                break;

            case FAPI_PBCH_PDU_TYPE:
                p_bch_pdu = (PBCHPDUStruct) pPduStruct;
                p_ssb_pdu = &p_fapi_dl_tti_req_pdu->pdu.ssb_pdu;
                NR5G_FAPI_MEMSET(p_bch_pdu, RUP32B(sizeof(BCHPDUStruct)), 0,
                    pbch_size);
                p_bch_pdu->sPDUHdr.nPDUType = DL_PDU_TYPE_PBCH;
                p_bch_pdu->sPDUHdr.nPDUSize = pbch_size;
                total_size += pbch_size;
                nr5g_fapi_fill_ssb_pdu(p_phy_instance, p_bch_pdu, p_ssb_pdu);
                break;

            case FAPI_CSIRS_PDU_TYPE:
                pCSIRSPdu = (PCSIRSPDUStruct) pPduStruct;
                p_csi_rs_pdu = &p_fapi_dl_tti_req_pdu->pdu.csi_rs_pdu;
                NR5G_FAPI_MEMSET(pCSIRSPdu, RUP32B(sizeof(CSIRSPDUStruct)), 0,
                    csirs_size);
                pCSIRSPdu->sPDUHdr.nPDUType = DL_PDU_TYPE_CSIRS;
                pCSIRSPdu->sPDUHdr.nPDUSize = csirs_size;
                total_size += csirs_size;
                nr5g_fapi_fill_csi_rs_pdu(p_phy_instance, p_csi_rs_pdu, pCSIRSPdu);
                break;

            default:
                NR5G_FAPI_LOG(ERROR_LOG, ("[DL_TTI] Invalid Pdu Type: %d",
                        pPduStruct->nPDUType));
                return FAILURE;
        }
        pPduStruct =
            (PDUStruct *) ((uint8_t *) pPduStruct + pPduStruct->nPDUSize);
        p_stats->iapi_stats.iapi_dl_tti_pdus++;
    }

    p_phy_req->nDCI = nDCI;
    p_phy_req->sMsgHdr.nMessageLen = total_size;

    if (NULL != p_fapi_vendor_msg)
    {
        nr5g_fapi_dl_tti_req_to_phy_translation_vendor_ext(p_phy_instance,
                                                           p_fapi_vendor_msg,
                                                           p_phy_req);
    }

    return SUCCESS;
}

 /** @ingroup group_source_api_p5_fapi2phy_proc
 *
 *  @param[in]   p_fapi_vendor_msg  Pointer to FAPI DL_TTI.request vendor message.
 *  @param[out]  p_ia_dl_config_req Pointer to IAPI DL_TTI.request structure.
 *
 *  @return     no return.
 *
 *  @description
 *  This function fills fields for DL_TTI.request structure that come from
 *  a vendor extension.
 *
**/
void nr5g_fapi_dl_tti_req_to_phy_translation_vendor_ext(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_vendor_msg_t * p_fapi_vendor_msg,
    PDLConfigRequestStruct p_phy_req)
{
    int idx = 0;

    fapi_vendor_dl_tti_req_t *p_vendor_dl_tti_req = NULL;
    fapi_vendor_dl_pdcch_pdu_t *p_vendor_pdcch_pdu = NULL;
    fapi_vendor_dl_pdsch_pdu_t *p_vendor_pdsch_pdu = NULL;
    fapi_vendor_csi_rs_pdu_t *p_vendor_csi_rs_pdu = NULL;

    PPDUStruct pPduStruct = NULL;
    PDCIPDUStruct p_dci_pdu = NULL;
    PDLSCHPDUStruct p_dlsch_pdu = NULL;
    PCSIRSPDUStruct p_CSIRS_pdu = NULL;

    p_vendor_dl_tti_req = &p_fapi_vendor_msg->p7_req_vendor.dl_tti_req;

    p_phy_req->sSFN_Slot.nSym = p_fapi_vendor_msg->p7_req_vendor.dl_tti_req.sym;

    p_phy_req->nLte_CRS_Present = p_fapi_vendor_msg->p7_req_vendor.dl_tti_req.lte_crs_present;
    p_phy_req->nLte_CRS_carrierFreqDL = p_fapi_vendor_msg->p7_req_vendor.dl_tti_req.lte_crs_carrier_freq_dl;
    p_phy_req->nLte_CRS_carrierBandwidthDL = p_fapi_vendor_msg->p7_req_vendor.dl_tti_req.lte_crs_carrier_bandwidth_dl;
    p_phy_req->nLte_CRS_nrofCRS_Ports = p_fapi_vendor_msg->p7_req_vendor.dl_tti_req.lte_crs_nr_of_crs_ports;
    p_phy_req->nLte_CRS_v_shift = p_fapi_vendor_msg->p7_req_vendor.dl_tti_req.lte_crs_v_shift;
    p_phy_req->nPdcchPrecoderEn = p_fapi_vendor_msg->p7_req_vendor.dl_tti_req.pdcch_precoder_en;
    p_phy_req->nSSBPrecoderEn = p_fapi_vendor_msg->p7_req_vendor.dl_tti_req.ssb_precoder_en;

    pPduStruct = p_phy_req->sDLPDU;
    for (idx = 0; idx < p_phy_req->nPDU; ++idx) {
        switch (pPduStruct->nPDUType) {
            case DL_PDU_TYPE_DCI: 
                p_dci_pdu = (PDCIPDUStruct) pPduStruct;
                p_vendor_pdcch_pdu =
                    &p_vendor_dl_tti_req->pdus[idx].pdu.pdcch_pdu;

                if (USE_VENDOR_EPREXSSB == p_phy_instance->phy_config.use_vendor_EpreXSSB)
                {
                    p_dci_pdu->nEpreRatioOfPDCCHToSSB =
                        p_vendor_pdcch_pdu->dl_dci[0].epre_ratio_of_pdcch_to_ssb;
                    p_dci_pdu->nEpreRatioOfDmrsToSSB =
                        p_vendor_pdcch_pdu->dl_dci[0].epre_ratio_of_dmrs_to_ssb;
                }
                break;

            case DL_PDU_TYPE_DLSCH:
                p_dlsch_pdu = (PDLSCHPDUStruct) pPduStruct;
                p_vendor_pdsch_pdu =
                    &p_vendor_dl_tti_req->pdus[idx].pdu.pdsch_pdu;

                p_dlsch_pdu->nNrOfAntennaPorts = p_vendor_pdsch_pdu->nr_of_antenna_ports;

                if (USE_VENDOR_EPREXSSB == p_phy_instance->phy_config.use_vendor_EpreXSSB)
                {
                    p_dlsch_pdu->nEpreRatioOfDmrsToSSB =
                        p_vendor_pdsch_pdu->epre_ratio_of_dmrs_to_ssb;
                    p_dlsch_pdu->nEpreRatioOfPDSCHToSSB =
                        p_vendor_pdsch_pdu->epre_ratio_of_pdsch_to_ssb;
                }

                NR5G_FAPI_MEMCPY(p_dlsch_pdu->nTxRUIdx, sizeof(p_dlsch_pdu->nTxRUIdx),
                    p_vendor_pdsch_pdu->tx_ru_idx, sizeof(p_vendor_pdsch_pdu->tx_ru_idx));
                break;

            case DL_PDU_TYPE_PBCH:
                // No vendor ext
                break;

            case DL_PDU_TYPE_CSIRS:
                p_CSIRS_pdu = (PCSIRSPDUStruct) pPduStruct;
                p_vendor_csi_rs_pdu = &p_vendor_dl_tti_req->pdus[idx].pdu.csi_rs_pdu;

                if (USE_VENDOR_EPREXSSB == p_phy_instance->phy_config.use_vendor_EpreXSSB)
                {
                    p_CSIRS_pdu->nEpreRatioToSSB = p_vendor_csi_rs_pdu->epre_ratio_to_ssb;
                }
                break;

            default:
                NR5G_FAPI_LOG(ERROR_LOG, ("[DL_TTI] Invalid Pdu Type: %d",
                        pPduStruct->nPDUType));
                return;
        }
        pPduStruct =
            (PDUStruct *) ((uint8_t *) pPduStruct + pPduStruct->nPDUSize);
    }
}

/** @ingroup group_nr5g_test_config
 *
 *  @param[in]   p_pdcch_pdu
 *  @param[out]  p_dci_pdu
 *
 *  @return      void
 *
 *  @description
 *  This function fills IAPI DCIPdu from FAPI PDCCH Pdu
 *
**/
void nr5g_fapi_fill_dci_pdu(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_dl_pdcch_pdu_t * p_pdcch_pdu,
    PDCIPDUStruct p_dci_pdu)
{
    nr5g_fapi_stats_t *p_stats = NULL;

    p_stats = &p_phy_instance->stats;
    p_stats->fapi_stats.fapi_dl_tti_pdcch_pdus++;

    p_dci_pdu->nBWPSize = p_pdcch_pdu->bwpSize;
    p_dci_pdu->nBWPStart = p_pdcch_pdu->bwpStart;
    p_dci_pdu->nSubcSpacing = p_pdcch_pdu->subCarrierSpacing;
    p_dci_pdu->nCpType = p_pdcch_pdu->cyclicPrefix;
    p_dci_pdu->nCCEStartIndex = p_pdcch_pdu->startSymbolIndex;
    p_dci_pdu->nNrOfSymbols = p_pdcch_pdu->durationSymbols;
    p_dci_pdu->nStartSymbolIndex = p_pdcch_pdu->startSymbolIndex;
    p_dci_pdu->nFreqDomain[0] = ((uint32_t) p_pdcch_pdu->freqDomainResource[0] |
        (((uint32_t) p_pdcch_pdu->freqDomainResource[1]) << 8) |
        (((uint32_t) p_pdcch_pdu->freqDomainResource[2]) << 16) |
        (((uint32_t) p_pdcch_pdu->freqDomainResource[3]) << 24));
    p_dci_pdu->nFreqDomain[1] = ((uint32_t) p_pdcch_pdu->freqDomainResource[4] |
        (((uint32_t) p_pdcch_pdu->freqDomainResource[5]) << 8));
    p_dci_pdu->nCCEToREGType = p_pdcch_pdu->cceRegMappingType;
    p_dci_pdu->nREGBundleSize = p_pdcch_pdu->regBundleSize;
    p_dci_pdu->nInterleaveSize = p_pdcch_pdu->interleaverSize;
    p_dci_pdu->nShift = p_pdcch_pdu->shiftIndex;
    p_dci_pdu->nCoreSetType = p_pdcch_pdu->coreSetType;
    p_dci_pdu->nCCEStartIndex = p_pdcch_pdu->dlDci[0].cceIndex;
    p_dci_pdu->nAggrLvl = p_pdcch_pdu->dlDci[0].aggregationLevel;
    p_dci_pdu->nScid = p_pdcch_pdu->dlDci[0].scramblingId;
    p_dci_pdu->nID = p_pdcch_pdu->dlDci[0].scramblingId;
    p_dci_pdu->nRNTIScramb = p_pdcch_pdu->dlDci[0].scramblingRnti;
    p_dci_pdu->nRNTI = p_pdcch_pdu->dlDci[0].rnti;
    p_dci_pdu->nTotalBits = p_pdcch_pdu->dlDci[0].payloadSizeBits;

    if (USE_VENDOR_EPREXSSB != p_phy_instance->phy_config.use_vendor_EpreXSSB) {
    p_dci_pdu->nEpreRatioOfPDCCHToSSB =
        nr5g_fapi_calculate_nEpreRatioOfPDCCHToSSB(p_pdcch_pdu->
        dlDci[0].beta_pdcch_1_0);
    p_dci_pdu->nEpreRatioOfDmrsToSSB =
            nr5g_fapi_calculate_nEpreRatioOfDmrsToSSB(p_pdcch_pdu->
                dlDci[0].powerControlOffsetSS);
    }

    if (FAILURE == NR5G_FAPI_MEMCPY(p_dci_pdu->nDciBits,
            sizeof(uint8_t) * MAX_DCI_BIT_BYTE_LEN,
            p_pdcch_pdu->dlDci[0].payload,
            sizeof(uint8_t) * MAX_DCI_BIT_BYTE_LEN)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("PDCCH: RNTI: %d -- DCI Bits copy error.",
                p_pdcch_pdu->dlDci[0].rnti));
    }
    p_stats->iapi_stats.iapi_dl_tti_pdcch_pdus++;
}

/** @ingroup group_nr5g_test_config
 *
 *  @param[in]    p_pdsch_pdu
 *  @param[out]   p_dlsch_pdu
 *
 *  @return      void
 *
 *  @description
 *  This function fills FAPI PDSCH Pdu from IAPI DLSCHPDU
 *
**/
void nr5g_fapi_fill_pdsch_pdu(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_dl_pdsch_pdu_t * p_pdsch_pdu,
    PDLSCHPDUStruct p_dlsch_pdu)
{
    uint8_t idx, port_index = 0;
    nr5g_fapi_stats_t *p_stats;

    p_stats = &p_phy_instance->stats;
    p_stats->fapi_stats.fapi_dl_tti_pdsch_pdus++;

    p_dlsch_pdu->nBWPSize = p_pdsch_pdu->bwpSize;
    p_dlsch_pdu->nBWPStart = p_pdsch_pdu->bwpStart;
    p_dlsch_pdu->nSubcSpacing = p_pdsch_pdu->subCarrierSpacing;
    p_dlsch_pdu->nCpType = p_pdsch_pdu->cyclicPrefix;
    p_dlsch_pdu->nRNTI = p_pdsch_pdu->rnti;
    p_dlsch_pdu->nUEId = p_pdsch_pdu->pdu_index;

    // Codeword Information
    p_dlsch_pdu->nNrOfCodeWords = p_pdsch_pdu->nrOfCodeWords;
    p_dlsch_pdu->nMCS[0] = p_pdsch_pdu->cwInfo[0].mcsIndex;
    p_dlsch_pdu->nMcsTable = p_pdsch_pdu->cwInfo[0].mcsTable;
    p_dlsch_pdu->nRV[0] = p_pdsch_pdu->cwInfo[0].rvIndex;
    p_dlsch_pdu->nTBSize[0] = p_pdsch_pdu->cwInfo[0].tbSize;
    if (p_pdsch_pdu->nrOfCodeWords == 2) {
        p_dlsch_pdu->nMCS[1] = p_pdsch_pdu->cwInfo[1].mcsIndex;
        p_dlsch_pdu->nRV[1] = p_pdsch_pdu->cwInfo[1].rvIndex;
        p_dlsch_pdu->nTBSize[1] = p_pdsch_pdu->cwInfo[1].tbSize;
    }
    //p_dlsch_pdu->nNrOfAntennaPorts = p_phy_instance->phy_config.n_nr_of_rx_ant;
    p_dlsch_pdu->nNrOfLayers = p_pdsch_pdu->nrOfLayers;
    p_dlsch_pdu->nNrOfAntennaPorts = p_dlsch_pdu->nNrOfLayers;
    p_dlsch_pdu->nTransmissionScheme = p_pdsch_pdu->transmissionScheme;
    p_dlsch_pdu->nDMRSConfigType = p_pdsch_pdu->dmrsConfigType;
    p_dlsch_pdu->nNIDnSCID = p_pdsch_pdu->dlDmrsScramblingId;
    p_dlsch_pdu->nNid = p_pdsch_pdu->dataScramblingId;
    p_dlsch_pdu->nSCID = p_pdsch_pdu->scid;

    for (idx = 0;
        (idx < FAPI_MAX_DMRS_PORTS && port_index < p_dlsch_pdu->nNrOfLayers);
        idx++) {
        if ((p_pdsch_pdu->dmrsPorts >> idx) && 0x0001) {
            p_dlsch_pdu->nPortIndex[port_index++] = idx;
        }
    }

    // Resource Allocation Information
    if (FAILURE == NR5G_FAPI_MEMCPY(p_dlsch_pdu->nRBGIndex,
            sizeof(uint32_t) * MAX_DL_RBG_BIT_NUM,
            p_pdsch_pdu->rbBitmap, sizeof(uint32_t) * MAX_DL_RBG_BIT_NUM)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("PDSCH: RNTI: %d Pdu Index: %d -- RB Bitmap"
                "cpy error.", p_pdsch_pdu->rnti, p_pdsch_pdu->pdu_index));
    }
    p_dlsch_pdu->nResourceAllocType = p_pdsch_pdu->resourceAlloc;
    p_dlsch_pdu->nRBStart = p_pdsch_pdu->rbStart;
    p_dlsch_pdu->nRBSize = p_pdsch_pdu->rbSize;
    p_dlsch_pdu->nPMI = (p_pdsch_pdu->preCodingAndBeamforming.numPrgs > 0)
        ? p_pdsch_pdu->preCodingAndBeamforming.pmi_bfi[0].pmIdx
        : 0;
    p_dlsch_pdu->nVRBtoPRB = p_pdsch_pdu->vrbToPrbMapping;
    p_dlsch_pdu->nStartSymbolIndex = p_pdsch_pdu->startSymbIndex;
    p_dlsch_pdu->nNrOfSymbols = p_pdsch_pdu->nrOfSymbols;
    p_dlsch_pdu->nNrOfCDMs = p_pdsch_pdu->numDmrsCdmGrpsNoData;
    p_dlsch_pdu->nMappingType = p_pdsch_pdu->mappingType;
    p_dlsch_pdu->nNrOfDMRSSymbols = p_pdsch_pdu->nrOfDmrsSymbols;
    p_dlsch_pdu->nDMRSAddPos = p_pdsch_pdu->dmrsAddPos;

    // PTRS Information
    p_dlsch_pdu->nPTRSTimeDensity = p_pdsch_pdu->ptrsTimeDensity;
    p_dlsch_pdu->nPTRSFreqDensity = p_pdsch_pdu->ptrsFreqDensity;
    p_dlsch_pdu->nPTRSReOffset = p_pdsch_pdu->ptrsReOffset;
    p_dlsch_pdu->nEpreRatioOfPDSCHToPTRS = p_pdsch_pdu->nEpreRatioOfPdschToPtrs;

    if (USE_VENDOR_EPREXSSB != p_phy_instance->phy_config.use_vendor_EpreXSSB) {
        p_dlsch_pdu->nEpreRatioOfDmrsToSSB =
            nr5g_fapi_calculate_nEpreRatioOfDmrsToSSB(
                p_pdsch_pdu->powerControlOffsetSS);
        p_dlsch_pdu->nEpreRatioOfPDSCHToSSB =
            nr5g_fapi_calculate_nEpreRatioOfPDSCHToSSB(
                p_pdsch_pdu->powerControlOffset);
    }

    // PTRS Information
    p_dlsch_pdu->nPTRSPresent = p_pdsch_pdu->pduBitMap & 0x0001;
    p_dlsch_pdu->nNrOfPTRSPorts =
        __builtin_popcount(p_pdsch_pdu->ptrsPortIndex);
    for (idx = 0; idx < p_dlsch_pdu->nNrOfPTRSPorts &&
        idx < MAX_DL_PER_UE_PTRS_PORT_NUM; idx++) {
        p_dlsch_pdu->nPTRSPortIndex[idx] = idx;
    }

    // Don't Cares
    p_dlsch_pdu->nNrOfDMRSAssPTRS[0] = 0x1;
    p_dlsch_pdu->nNrOfDMRSAssPTRS[1] = 0x1;
    p_dlsch_pdu->n1n2 = 0x201;

    p_dlsch_pdu->nNrofTxRU = port_index;

    p_stats->iapi_stats.iapi_dl_tti_pdsch_pdus++;
}

/** @ingroup group_nr5g_test_config
 *
 *  @param[in]  beta_pdcch_1_0
 *
 *  @return     uint16_t mapping
 *
 *  @description
 *  This function maps FAPI to IAPI value range.
 *
 *
 * Please refer 5G FAPI-IAPI Translator Module SW Design Document for details on
 * the mapping.
 *
 * |-----------------------------------------|
 * | nEpreRatioOfPDCCHToSSb | beta_PDCCH_1_0 |
 * |-----------------------------------------|
 * |             1          |      0 - 2     |
 * |          1000          |        3       |
 * |          2000          |        4       |
 * |          3000          |        5       |
 * |          4000          |        6       |
 * |          5000          |        7       |
 * |          6000          |        8       |
 * |          7000          |        9       |
 * |          8000          |       10       |
 * |          9000          |       11       |
 * |         10000          |       12       |
 * |         11000          |       13       |
 * |         12000          |       14       |
 * |         13000          |       15       |
 * |         14000          |       16       |
 * |         14000          |       17       |
 * |-----------------------------------------|
 *
**/
uint16_t nr5g_fapi_calculate_nEpreRatioOfPDCCHToSSB(
    uint8_t beta_pdcch_1_0)
{
    if (beta_pdcch_1_0 > 0 && beta_pdcch_1_0 <= 2) {
        return 1;
    } else if (beta_pdcch_1_0 > 1 && beta_pdcch_1_0 < 17) {
        return ((beta_pdcch_1_0 - 2) * 1000);
    } else if (beta_pdcch_1_0 == 17) {
        return ((beta_pdcch_1_0 - 3) * 1000);
    } else {
        return 0;
    }
}

/** @ingroup group_nr5g_test_config
 *
 *  @param[in]  power_control_offset_ss
 *
 *  @return     uint16_t mapping
 *
 *  @description
 *  This function maps FAPI to IAPI value range.
 *
 *
 * nEpreRatioOfDmrsToSSB: 1->20000, 0.001dB step, -6dB to 14dB
 * powerControlOffsetSS:  0->3, 3dB step, -3dB to 6dB
 * |----------------------------------------------|
 * | nEpreRatioOfDmrsToSSB | powerControlOffsetSS |
 * |----------------------------------------------|
 * |     3000              |          0           |
 * |     6000              |          1           |
 * |     9000              |          2           |
 * |     12000             |          3           |
 * |----------------------------------------------|
 *
**/
uint16_t nr5g_fapi_calculate_nEpreRatioOfDmrsToSSB(
    uint8_t power_control_offset_ss)
{
    switch(power_control_offset_ss)
    {
        case 0:
            return 3000;
        case 1:
            return 6000;
        case 2:
            return 9000;
        case 3:
            return 12000;
        default:
            NR5G_FAPI_LOG(ERROR_LOG,
                ("Unsupported value of power_control_offset_ss."));
            return 0;
    }
}


/** @ingroup group_nr5g_test_config
 *
 *  @param[in]  power_control_offset
 *
 *  @return     uint16_t mapping
 *
 *  @description
 *  This function maps FAPI to IAPI value range.
 *
 *
 * nEpreRatioOfPDSCHToSSB: 1->20000, 0.001dB step, -6dB to 14dB
 * powerControlOffset:  0->23, 1dB step, -8dB to 15dB
 * |----------------------------------------------|
 * | nEpreRatioOfPDSCHToSSB | powerControlOffset  |
 * |----------------------------------------------|
 * |             1          |        0-2          |
 * |          1000          |          3          |
 * |          2000          |          4          |
 * |          3000          |          5          |
 * |          4000          |          6          |
 * |          5000          |          7          |
 * |          6000          |          8          |
 * |          7000          |          9          |
 * |          8000          |         10          |
 * |          9000          |         11          |
 * |         10000          |         12          |
 * |         11000          |         13          |
 * |         12000          |         14          |
 * |         13000          |         15          |
 * |         14000          |         16          |
 * |         15000          |         17          |
 * |         16000          |         18          |
 * |         17000          |         19          |
 * |         18000          |         20          |
 * |         19000          |         21          |
 * |         20000          |      22-23          |
 * |----------------------------------------------|
 *
**/
uint16_t nr5g_fapi_calculate_nEpreRatioOfPDSCHToSSB(uint8_t power_control_offset)
{
    static const uint8_t MAPPING_SIZE = 24;
    static const uint16_t power_control_offset_to_epre_ratio[MAPPING_SIZE] = {
    //      0      1      2      3      4      5      6      7
            1,     1,     1,  1000,  2000,  3000,  4000,  5000,
    //      8      9     10     11     12     13     14     15
         6000,  7000,  8000,  9000, 10000, 11000, 12000, 13000,
    //     16     17     18     19     20     21     22     23
        14000, 15000, 16000, 17000, 18000, 19000, 20000, 20000
    };

    if(MAPPING_SIZE > power_control_offset)
    {
        return power_control_offset_to_epre_ratio[power_control_offset];
    }
    else
    {
        NR5G_FAPI_LOG(ERROR_LOG,
            ("Unsupported value of power_control_offset=%u.",
            power_control_offset));
        return 0;
    }
}

/** @ingroup group_nr5g_test_config
 *
 *  @param[in]  ssb_offset_point_a
 *  @param[in]  sub_c_common
 *
 *  @return     uint8_t nSSBPrbOffset
 *
 *  @description
 *  This function maps FAPI to IAPI value range.
 *
 * Please refer 5G FAPI-IAPI Translator Module SW Design Document for details on
 * the mapping.
 *
**/
uint8_t nr5g_fapi_calculate_nSSBPrbOffset(
    uint16_t ssb_offset_point_a, uint8_t sub_c_common)
{
    return ssb_offset_point_a/pow(2, sub_c_common);
}

/** @ingroup group_nr5g_test_config
 *
 *  @param[in]   p_dlsch_pdu
 *  @param[out]  p_pdsch_pdu
 *
 *  @return      void
 *
 *  @description
 *  This function fills FAPI PDSCH Pdu from IAPI DLSCHPDU
 *
**/
void nr5g_fapi_fill_ssb_pdu(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    PBCHPDUStruct p_bch_pdu,
    fapi_dl_ssb_pdu_t * p_ssb_pdu)
{
    uint8_t *p_mib = (uint8_t *) & p_bch_pdu->nMIB[0];
    uint8_t *payload = (uint8_t *) & p_ssb_pdu->bchPayload.bchPayload;
    nr5g_fapi_stats_t *p_stats;

    p_mib[0] = payload[0];
    p_mib[1] = payload[1];
    p_mib[2] = payload[2];
    p_stats = &p_phy_instance->stats;
    p_stats->fapi_stats.fapi_dl_tti_ssb_pdus++;
    p_bch_pdu->nSSBSubcOffset = p_ssb_pdu->ssbSubCarrierOffset;
    p_bch_pdu->nSSBPrbOffset =
        nr5g_fapi_calculate_nSSBPrbOffset(p_ssb_pdu->ssbOffsetPointA,
            p_phy_instance->phy_config.sub_c_common);
    p_stats->iapi_stats.iapi_dl_tti_ssb_pdus++;
}

/** @ingroup group_nr5g_test_config
 *
 *  @param[in]   p_dlsch_pdu
 *  @param[out]  p_pdsch_pdu
 *
 *  @return      void
 *
 *  @description
 *  This function fills FAPI PDSCH Pdu from IAPI DLSCHPDU
 *
**/
void nr5g_fapi_fill_csi_rs_pdu(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_dl_csi_rs_pdu_t * p_csi_rs_pdu,
    PCSIRSPDUStruct p_CSIRS_pdu)
{
    nr5g_fapi_stats_t *p_stats;

    p_stats = &p_phy_instance->stats;
    p_stats->fapi_stats.fapi_dl_tti_csi_rs_pdus++;

    p_CSIRS_pdu->nBWPSize = p_csi_rs_pdu->bwpSize;
    p_CSIRS_pdu->nBWPStart = p_csi_rs_pdu->bwpStart;
    p_CSIRS_pdu->nCDMType = p_csi_rs_pdu->cdmType;
    p_CSIRS_pdu->nCSIType = p_csi_rs_pdu->csiType;
    p_CSIRS_pdu->nCpType = p_csi_rs_pdu->cyclicPrefix;
    p_CSIRS_pdu->nFreqDensity = p_csi_rs_pdu->freqDensity;
    p_CSIRS_pdu->nFreqDomain = p_csi_rs_pdu->freqDomain;
    p_CSIRS_pdu->nNrOfRBs = p_csi_rs_pdu->nrOfRbs;
    p_CSIRS_pdu->nScrambId = p_csi_rs_pdu->scramId;
    p_CSIRS_pdu->nStartRB = p_csi_rs_pdu->startRb;
    p_CSIRS_pdu->nSubcSpacing = p_csi_rs_pdu->subCarrierSpacing;
    p_CSIRS_pdu->nSymbL0 = p_csi_rs_pdu->symbL0;
    p_CSIRS_pdu->nSymbL1 = p_csi_rs_pdu->symbL1;
    p_CSIRS_pdu->nRow = p_csi_rs_pdu->row;
    // Not mapping the beamforming parameters
    // p_CSIRS_pdu->powerControlOffset = p_csi_rs_pdu->powerControlOffset;

    if (USE_VENDOR_EPREXSSB != p_phy_instance->phy_config.use_vendor_EpreXSSB) {
        p_CSIRS_pdu->nEpreRatioToSSB = nr5g_fapi_calculate_nEpreRatioOfDmrsToSSB(p_csi_rs_pdu->powerControlOffsetSs);
    }

    p_stats->iapi_stats.iapi_dl_tti_csi_rs_pdus++;
}
