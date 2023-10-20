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
 * This file consist of implementation of FAPI UL_TTI.request message.
 *
 **/

#include "nr5g_fapi_framework.h"
#include "gnb_l1_l2_api.h"
#include "nr5g_fapi_fapi2mac_api.h"
#include "nr5g_fapi_fapi2phy_api.h"
#include "nr5g_fapi_fapi2phy_p7_proc.h"
#include "nr5g_fapi_fapi2phy_p7_pvt_proc.h"
#include "nr5g_fapi_memory.h"
#include <math.h>

 /** @ingroup group_source_api_p7_fapi2phy_proc
 *
 *  @param[in]  p_phy_instance Pointer to PHY instance.
 *  @param[in]  p_fapi_req Pointer to FAPI UL_TTI.request message structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This message indicates the control information for an uplink slot.
 *
**/
uint8_t nr5g_fapi_ul_tti_request(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_ul_tti_req_t * p_fapi_req,
    fapi_vendor_msg_t * p_fapi_vendor_msg)
{
    PULConfigRequestStruct p_ia_ul_config_req;
    PMAC2PHY_QUEUE_EL p_list_elem;
    nr5g_fapi_stats_t *p_stats;
    UNUSED(p_fapi_vendor_msg);

    if (NULL == p_phy_instance) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[NR5G_FAPI][UL_TTI.request] Invalid "
                "phy instance"));
        return FAILURE;
    }
    p_stats = &p_phy_instance->stats;
    p_stats->fapi_stats.fapi_ul_tti_req++;

    if (NULL == p_fapi_req) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[NR5G_FAPI][UL_TTI.request] Invalid fapi "
                "message"));
        return FAILURE;
    }

    p_list_elem = nr5g_fapi_fapi2phy_create_api_list_elem((uint8_t)
        MSG_TYPE_PHY_UL_CONFIG_REQ, 1,
        (uint32_t) sizeof(ULConfigRequestStruct));
    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG,
            ("[NR5G_FAPI][UL_TTI.request] Unable to create "
                "list element. Out of memory!!!"));
        return FAILURE;
    }

    p_ia_ul_config_req = (PULConfigRequestStruct) (p_list_elem + 1);
    NR5G_FAPI_MEMSET(p_ia_ul_config_req, sizeof(ULConfigRequestStruct), 0,
        sizeof(ULConfigRequestStruct));
    p_ia_ul_config_req->sMsgHdr.nMessageType = MSG_TYPE_PHY_UL_CONFIG_REQ;
    p_ia_ul_config_req->sMsgHdr.nMessageLen =
        (uint16_t) sizeof(ULConfigRequestStruct);

    if (FAILURE == nr5g_fapi_ul_tti_req_to_phy_translation(p_phy_instance,
            p_fapi_req, p_ia_ul_config_req)) {
        nr5g_fapi_fapi2phy_destroy_api_list_elem(p_list_elem);
        NR5G_FAPI_LOG(DEBUG_LOG, ("[UL_TTI.request][%d][%d,%d] Not Sent",
                p_phy_instance->phy_id, p_ia_ul_config_req->sSFN_Slot.nSFN,
                p_ia_ul_config_req->sSFN_Slot.nSlot));
        return FAILURE;
    }

    nr5g_fapi_fapi2phy_add_to_api_list(p_list_elem);

    p_stats->iapi_stats.iapi_ul_config_req++;
    NR5G_FAPI_LOG(DEBUG_LOG, ("[UL_TTI.request][%d][%d,%d]",
            p_phy_instance->phy_id,
            p_ia_ul_config_req->sSFN_Slot.nSFN,
            p_ia_ul_config_req->sSFN_Slot.nSlot));

    return SUCCESS;
}

 /** @ingroup group_source_api_p7_fapi2phy_proc
 *
 *  @param[in]  bwp_size  Variable holding the Bandwidth part size.
 *  
 *  @return     Returns ::RBG Size.
 *
 *  @description
 *  This functions calculates and return RBG Size from Bandwidth part size provided. 
 *
**/
uint8_t nr5g_fapi_calc_n_rbg_size(
    uint16_t bwp_size)
{
    uint8_t n_rbg_size = 0;
    if (bwp_size >= 1 && bwp_size <= 36) {
        n_rbg_size = 2;
    } else if (bwp_size >= 37 && bwp_size <= 72) {
        n_rbg_size = 4;
    } else if (bwp_size >= 73 && bwp_size <= 144) {
        n_rbg_size = 8;
    } else if (bwp_size >= 145 && bwp_size <= 275) {
        n_rbg_size = 16;
    } else {
        n_rbg_size = 0;
    }
    return n_rbg_size;
}

 /** @ingroup group_source_api_p7_fapi2phy_proc
 *
 *  @param[in]  n_rbg_size  Variable holding the RBG Size
 *  @param[in]  p_push_pdu Pointer to FAPI PUSCH Pdu
 *  
 *  @return     Returns ::RBG Bitmap entry
 *
 *  @description
 *  This functions derives the RBG Bitmap entry for PUSCH Type-0 allocation. 
 *
**/
uint32_t nr5g_fapi_calc_n_rbg_index_entry(
    uint8_t n_rbg_size,
    fapi_ul_pusch_pdu_t * p_pusch_pdu)
{
    uint8_t i, temp, num_bits = 0;
    uint32_t n_rbg_bitmap = 0;
    uint8_t rb_bitmap_entries, rb_bitmap;

    rb_bitmap_entries = ceil(n_rbg_size / 8);
    for (i = 0; i < rb_bitmap_entries; i++) {
        num_bits = 0;
        temp = 0;
        rb_bitmap = p_pusch_pdu->rbBitmap[i];
        while (num_bits < 8) {
            if (rb_bitmap & (1 << num_bits)) {
                temp |= (1 << (7 - num_bits));
            }
            num_bits++;
        }
        n_rbg_bitmap |= ((n_rbg_bitmap | temp) << (32 - (8 * (i + 1))));
    }
    return n_rbg_bitmap;
}

 /** @ingroup group_source_api_p7_fapi2phy_proc
 *
 *  @param[in]  fapi_alpha_scaling  Variable holding the FAPI Alpha Scaling Value.
 *  
 *  @return     Returns ::PHY equivalent Alpha Scaling Value.
 *
 *  @description
 *  This functions derives the PHY equivalent Alpha Scaling value from FAPI Alpha Scaling Value. 
 *
**/
uint8_t nr5g_fapi_calc_alpha_scaling(
    uint8_t fapi_alpha_scaling)
{
    uint8_t alpha_scaling;

    switch (fapi_alpha_scaling) {
        case 0:
            alpha_scaling = 127;
            break;

        case 1:
            alpha_scaling = 166;
            break;

        case 2:
            alpha_scaling = 205;
            break;

        case 3:
            alpha_scaling = 255;
            break;

        default:
            alpha_scaling = 0;
            break;
    }
    return alpha_scaling;
}

/** @ingroup group_source_api_p7_fapi2phy_proc
 *
 *  @param[in]  p_pusch_data Pointer to FAPI Optional PUSCH Data structure.
 *  @param[in]  p_ul_data_chan Pointer to IAPI ULSCH PDU structure.
 *  
 *  @return     None.
 *
 *  @description
 *  This function converts FAPI UL_TTI.request's Optional PUSCH Data to IAPI UL_Config.request's
 *  ULSCH PDU structure.
 *
**/
void nr5g_fapi_pusch_data_to_phy_ulsch_translation(
    nr5g_fapi_pusch_info_t * p_pusch_info,
    fapi_pusch_data_t * p_pusch_data,
    ULSCHPDUStruct * p_ul_data_chan)
{
    p_ul_data_chan->nRV = p_pusch_data->rvIndex;
    p_pusch_info->harq_process_id = p_ul_data_chan->nHARQID =
        p_pusch_data->harqProcessId;
    p_ul_data_chan->nNDI = p_pusch_data->newDataIndicator;
    p_ul_data_chan->nTBSize = p_pusch_data->tbSize;
    //numCb and cbPresentAndPoistion[] is ignored as per design 
}

 /** @ingroup group_source_api_p7_fapi2phy_proc
 *
 *  @param[in]  p_pusch_uci Pointer to FAPI Optional PUSCH UCI structure.
 *  @param[in]  p_ul_data_chan Pointer to IAPI ULSCH PDU structure.
 *  
 *  @return     None.
 *
 *  @description
 *  This function converts FAPI UL_TTI.request's Optional PUSCH UCI to IAPI UL_Config.request's
 *  ULSCH PDU structure.
 *
**/
void nr5g_fapi_pusch_uci_to_phy_ulsch_translation(
    fapi_pusch_uci_t * p_pusch_uci,
    ULSCHPDUStruct * p_ul_data_chan)
{
    p_ul_data_chan->nAck = p_pusch_uci->harqAckBitLength;
    //csiPart1BitLength and csiPart2BitLength are ignored as per design
    p_ul_data_chan->nAlphaScaling =
        nr5g_fapi_calc_alpha_scaling(p_pusch_uci->alphaScaling);
    //p_ul_data_chan->nAlphaScaling = 0;
    p_ul_data_chan->nBetaOffsetACKIndex = p_pusch_uci->betaOffsetHarqAck;
    //betaOffsetCsi1 and betaOffsetCsi2 are ignored as per design
}

/** @ingroup group_source_api_p7_fapi2phy_proc
 *
 *  @param[in]  p_pusch_ptrs Pointer to FAPI Optional PUSCH PTRS structure.
 *  @param[in]  p_ul_data_chan Pointer to IAPI ULSCH PDU structure.
 *  
 *  @return     None.
 *
 *  @description
 *  This function converts FAPI UL_TTI.request's Optional PUSCH PTRS to IAPI UL_Config.request's
 *  ULSCH PDU structure.
 *
**/
void nr5g_fapi_pusch_ptrs_to_phy_ulsch_translation(
    fapi_pusch_ptrs_t * p_pusch_ptrs,
    ULSCHPDUStruct * p_ul_data_chan)
{
    uint8_t i, num_ptrs_ports = 0, port_index = 0;
    fapi_ptrs_info_t *p_ptrs_info;

    if (p_pusch_ptrs->ptrsTimeDensity <= 2) {
        p_ul_data_chan->nPTRSTimeDensity =
            pow(2, p_pusch_ptrs->ptrsTimeDensity);
    }
    if (p_pusch_ptrs->ptrsFreqDensity == 0 ||
        p_pusch_ptrs->ptrsFreqDensity == 1) {
        p_ul_data_chan->nPTRSFreqDensity =
            pow(2, (p_pusch_ptrs->ptrsFreqDensity + 1));
    }
    if (p_pusch_ptrs->numPtrsPorts > 0) {
        num_ptrs_ports = p_ul_data_chan->nNrOfPTRSPorts = 1;
    }
    for (i = 0; (i < num_ptrs_ports && port_index < FAPI_MAX_PTRS_PORTS); i++) {
        p_ptrs_info = &p_pusch_ptrs->ptrsInfo[i];
        if ((p_ptrs_info->ptrsPortIndex >> i) & 0x01) {
            p_ul_data_chan->nPTRSPortIndex[port_index++] = i;
        }
        //PTRSDmrsPort is ignored as per Design
        p_ul_data_chan->nPTRSReOffset = p_ptrs_info->ptrsReOffset;
    }
}

 /** @ingroup group_source_api_p7_fapi2phy_proc
 *
 *  @param[in]  p_phy_instance Pointer to PHY instance.
 *  @param[in]  p_pusch_pdu Pointer to FAPI PUSCH PDU structure.
 *  @param[out]  p_ul_data_chan Pointer to IAPI ULSCH PDU structure.
 *  
 *  @return     None.
 *
 *  @description
 *  This function converts FAPI UL_TTI.request's PUSCH PDU to IAPI UL_Config.request's
 *  ULSCH PDU structure.
 *
**/
void nr5g_fapi_pusch_to_phy_ulsch_translation(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    nr5g_fapi_pusch_info_t * p_pusch_info,
    fapi_ul_pusch_pdu_t * p_pusch_pdu,
    ULSCHPDUStruct * p_ul_data_chan)
{
    uint8_t mcs_table, nr_of_layers, n_rbg_size;
    uint8_t i, port_index = 0;
    uint16_t dmrs_ports, bwp_size, bwp_start, pdu_bitmap;
    nr5g_fapi_stats_t *p_stats;

    p_stats = &p_phy_instance->stats;
    p_stats->fapi_stats.fapi_ul_tti_pusch_pdus++;

    NR5G_FAPI_MEMSET(p_ul_data_chan, sizeof(ULSCHPDUStruct), 0,
        sizeof(ULSCHPDUStruct));

    p_ul_data_chan->sPDUHdr.nPDUType = UL_PDU_TYPE_ULSCH;
    p_ul_data_chan->sPDUHdr.nPDUSize = RUP32B(sizeof(ULSCHPDUStruct));

    p_ul_data_chan->nRNTI = p_pusch_pdu->rnti;

    p_pusch_info->handle = p_ul_data_chan->nUEId =
        (uint16_t) p_pusch_pdu->handle;

    bwp_size = p_ul_data_chan->nBWPSize = p_pusch_pdu->bwpSize;
    bwp_start = p_ul_data_chan->nBWPStart = p_pusch_pdu->bwpStart;

    p_ul_data_chan->nSubcSpacing = p_pusch_pdu->subCarrierSpacing;
    p_ul_data_chan->nCpType = p_pusch_pdu->cyclicPrefix;
    p_ul_data_chan->nMCS = p_pusch_pdu->mcsIndex;
    p_ul_data_chan->nTransPrecode = !(p_pusch_pdu->transformPrecoding);
    mcs_table = p_pusch_pdu->mcsTable;
    if (mcs_table <= 2) {
        p_ul_data_chan->nMcsTable = mcs_table;
    }
    if ((mcs_table > 2) && (mcs_table <= 4)) {
        p_ul_data_chan->nTransPrecode = 1;

        if (mcs_table == 3) {
            p_ul_data_chan->nMcsTable = 0;
        } else {
            p_ul_data_chan->nMcsTable = 2;
        }
    }

    p_ul_data_chan->nNid = p_pusch_pdu->dataScramblingId;
    p_ul_data_chan->nDMRSConfigType = p_pusch_pdu->dmrsConfigType;
    p_ul_data_chan->nMappingType = p_pusch_pdu->mappingType;
    p_ul_data_chan->nNrOfDMRSSymbols = p_pusch_pdu->nrOfDmrsSymbols;
    p_ul_data_chan->nDMRSAddPos = p_pusch_pdu->dmrsAddPos;
    p_ul_data_chan->nNIDnSCID = p_pusch_pdu->ulDmrsScramblingId;
    p_ul_data_chan->nSCID = p_pusch_pdu->scid;
    p_ul_data_chan->nNrOfCDMs = p_pusch_pdu->numDmrsCdmGrpsNoData;

    dmrs_ports = p_pusch_pdu->dmrsPorts;
    nr_of_layers = p_ul_data_chan->nNrOfLayers = p_pusch_pdu->nrOfLayers;
    for (i = 0; (i < FAPI_MAX_DMRS_PORTS && port_index < nr_of_layers); i++) {
        if (port_index < FAPI_MAX_UL_LAYERS) {
            if ((dmrs_ports >> i) & 0x0001) {
                p_ul_data_chan->nPortIndex[port_index++] = i;
            }
        } else {
            break;
        }
    }
    p_ul_data_chan->nTPPuschID = p_pusch_pdu->nTpPuschId;
    p_ul_data_chan->nTpPi2BPSK = p_pusch_pdu->tpPi2Bpsk;
    //Config-1 alone is supported
    n_rbg_size = p_ul_data_chan->nRBGSize = nr5g_fapi_calc_n_rbg_size(bwp_size);
    if (n_rbg_size > 0) {
        p_ul_data_chan->nNrOfRBGs =
            ceil((bwp_size + (bwp_start % n_rbg_size)) / n_rbg_size);
    }
    //First entry would be sufficient as maximum no of RBG's is at max 18.
    p_ul_data_chan->nRBGIndex[0] =
        nr5g_fapi_calc_n_rbg_index_entry(n_rbg_size, p_pusch_pdu);
    p_ul_data_chan->nRBStart = p_pusch_pdu->rbStart;
    p_ul_data_chan->nRBSize = p_pusch_pdu->rbSize;
    p_ul_data_chan->nVRBtoPRB = p_pusch_pdu->vrbToPrbMapping;
    p_ul_data_chan->nResourceAllocType = p_pusch_pdu->resourceAlloc;
    p_ul_data_chan->nStartSymbolIndex = p_pusch_pdu->startSymbIndex;
    p_ul_data_chan->nNrOfSymbols = p_pusch_pdu->nrOfSymbols;

    p_ul_data_chan->nPTRSPresent = 0;
    pdu_bitmap = p_pusch_pdu->pduBitMap;
    if (pdu_bitmap & 0x01) {
        nr5g_fapi_pusch_data_to_phy_ulsch_translation(p_pusch_info,
            &p_pusch_pdu->puschData, p_ul_data_chan);
    }
    if (pdu_bitmap & 0x02) {
        nr5g_fapi_pusch_uci_to_phy_ulsch_translation(&p_pusch_pdu->puschUci,
            p_ul_data_chan);
    }
    if (pdu_bitmap & 0x04) {
        nr5g_fapi_pusch_ptrs_to_phy_ulsch_translation(&p_pusch_pdu->puschPtrs,
            p_ul_data_chan);
    }
    if ((pdu_bitmap & (1 << 15))) {
        p_ul_data_chan->nPTRSPresent = 1;
    }
    p_ul_data_chan->nULType = 0;
    p_ul_data_chan->nNrOfAntennaPorts =
        p_phy_instance->phy_config.n_nr_of_rx_ant;
    p_ul_data_chan->nRBBundleSize = 0;
    p_ul_data_chan->nPMI = 0;
    p_ul_data_chan->nTransmissionScheme = 0;
    p_ul_data_chan->rsv1 = 0;
    p_ul_data_chan->nNrofRxRU = p_phy_instance->phy_config.n_nr_of_rx_ant;
    p_stats->iapi_stats.iapi_ul_tti_pusch_pdus++;
}

 /** @ingroup group_source_api_p7_fapi2phy_proc
 *
 *  @param[in]  num_groups Variable holding the groups for which PUCCH resources 
                are unique
 *  @param[in]  initial_cyclic_shift Variable holding the parameter initial_cyclic 
                shift to be verified against the already receieved pucch resources.
 *  @param[in]  nr_of_layers Variable holding the parameter nr_of_symbols
                to be verified against the already received pucch resources.
 *  @param[in]  start_symbol_index  Variable holding the parameter start_symbol_index
                to be verified against the already received pucch resources.
 *  @param[in]  time_domain_occ_idx Variable holding the parameter time_domain_occ_idx
                to be verified against the already received pucch resources.
 *  @param[in]  p_pucch_resources Pointer pointing to the received pucch resources.
 
*  @return     group_id, if pucch_resources match with parameters passed.
 *             0xFF, if pucch_resources not match with parameters passed.
 *  @description
 *  This function returns the group_id if parameters  passed already available in
 *  pucch_resources received earlier.
 *
**/
uint8_t nr5g_get_pucch_resources_group_id(
    uint8_t num_groups,
    uint16_t initial_cyclic_shift,
    uint8_t nr_of_symbols,
    uint8_t start_symbol_index,
    uint8_t time_domain_occ_idx,
    nr5g_fapi_pucch_resources_t * p_pucch_resources)
{
    uint8_t i, group_id = 0xFF;
    for (i = 0; i < num_groups; i++) {
        if ((initial_cyclic_shift == p_pucch_resources[i].initial_cyclic_shift)
            && (nr_of_symbols == p_pucch_resources[i].nr_of_symbols)
            && (start_symbol_index == p_pucch_resources[i].start_symbol_index)
            && (time_domain_occ_idx ==
                p_pucch_resources[i].time_domain_occ_idx)) {
            group_id = p_pucch_resources[i].group_id;
            break;
        }
    }
    return group_id;
}

 /** @ingroup group_source_api_p7_fapi2phy_proc
 *
 *  @param[in]  p_phy_instance Pointer to PHY instance.
 *  @param[in]  p_pucch_pdu Pointer to FAPI PUSCH PDU structure.
 *  @param[in]  p_ul_ctrl_chan Pointer to IAPI ULCCH_UCIPDU structure.
 *  
 *  @return     None.
 *
 *  @description
 *  This function converts FAPI UL_TTI.request's PUCCH PDU to IAPI UL_Config.request's
 *  ULCCH_UCI PDU structure.
 *
**/
void nr5g_fapi_pucch_to_phy_ulcch_uci_translation(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    nr5g_fapi_pucch_info_t * p_pucch_info,
    fapi_ul_pucch_pdu_t * p_pucch_pdu,
    uint8_t * num_group_ids,
    nr5g_fapi_pucch_resources_t * p_pucch_resources,
    ULCCHUCIPDUStruct * p_ul_ctrl_chan)
{
    uint8_t group_id;
    nr5g_fapi_stats_t *p_stats;
    uint8_t initial_cyclic_shift, nr_of_symbols;
    uint8_t start_symbol_index, time_domain_occ_idx;
    uint8_t num_groups = *num_group_ids;

    p_stats = &p_phy_instance->stats;
    p_stats->fapi_stats.fapi_ul_tti_pucch_pdus++;
    NR5G_FAPI_MEMSET(p_ul_ctrl_chan, sizeof(ULCCHUCIPDUStruct), 0,
        sizeof(ULCCHUCIPDUStruct));
    p_ul_ctrl_chan->sPDUHdr.nPDUType = UL_PDU_TYPE_ULCCH_UCI;
    p_ul_ctrl_chan->sPDUHdr.nPDUSize = RUP32B(sizeof(ULCCHUCIPDUStruct));

    p_ul_ctrl_chan->nRNTI = p_pucch_pdu->rnti;
    p_pucch_info->handle = p_ul_ctrl_chan->nUEId =
        (uint16_t) p_pucch_pdu->handle;

    p_ul_ctrl_chan->nBWPSize = p_pucch_pdu->bwpSize;
    p_ul_ctrl_chan->nBWPStart = p_pucch_pdu->bwpStart;
    p_ul_ctrl_chan->nSubcSpacing = p_pucch_pdu->subCarrierSpacing;
    p_ul_ctrl_chan->nCpType = p_pucch_pdu->cyclicPrefix;
    p_pucch_info->pucch_format = p_ul_ctrl_chan->nFormat =
        p_pucch_pdu->formatType;
    if (p_pucch_pdu->pi2Bpsk) {
        p_ul_ctrl_chan->modType = 1;
    } else {                    //QPSK
        p_ul_ctrl_chan->modType = 2;
    }
    p_ul_ctrl_chan->nStartPRB = p_pucch_pdu->prbStart;
    p_ul_ctrl_chan->nPRBs = p_pucch_pdu->prbSize;
    start_symbol_index = p_ul_ctrl_chan->nStartSymbolx =
        p_pucch_pdu->startSymbolIndex;
    nr_of_symbols = p_ul_ctrl_chan->nSymbols = p_pucch_pdu->nrOfSymbols;
    p_ul_ctrl_chan->nFreqHopFlag = p_pucch_pdu->freqHopFlag;

    p_ul_ctrl_chan->n2ndHopPRB = p_pucch_pdu->secondHopPrb;
    initial_cyclic_shift = p_ul_ctrl_chan->nM0 =
        p_pucch_pdu->initialCyclicShift;
    p_ul_ctrl_chan->nID = p_pucch_pdu->dataScramblingId;
    time_domain_occ_idx = p_ul_ctrl_chan->nFmt1OrthCCodeIdx =
        p_pucch_pdu->timeDomainOccIdx;
    p_ul_ctrl_chan->nFmt4OrthCCodeIdx = p_pucch_pdu->preDftOccIdx;
    p_ul_ctrl_chan->nFmt4OrthCCodeLength = p_pucch_pdu->preDftOccLen;
    p_ul_ctrl_chan->nAddDmrsFlag = p_pucch_pdu->addDmrsFlag;
    p_ul_ctrl_chan->nScramID = p_pucch_pdu->dmrsScramblingId;
    p_ul_ctrl_chan->nSRPriodAriv = p_pucch_pdu->srFlag;
    p_ul_ctrl_chan->nBitLenUci = p_pucch_pdu->bitLenHarq;
    p_ul_ctrl_chan->nNrofRxRU = p_phy_instance->phy_config.n_nr_of_rx_ant;

    group_id =
        nr5g_get_pucch_resources_group_id(num_groups, initial_cyclic_shift,
        nr_of_symbols, start_symbol_index, time_domain_occ_idx,
        p_pucch_resources);
    if (group_id == 0xFF) {
        p_pucch_resources[num_groups].group_id = num_groups;
        p_pucch_resources[num_groups].initial_cyclic_shift =
            initial_cyclic_shift;
        p_pucch_resources[num_groups].nr_of_symbols = nr_of_symbols;
        p_pucch_resources[num_groups].start_symbol_index = start_symbol_index;
        p_pucch_resources[num_groups].time_domain_occ_idx = time_domain_occ_idx;
        p_ul_ctrl_chan->nGroupId = num_groups;
        num_groups++;
    } else {
        p_ul_ctrl_chan->nGroupId = group_id;
    }
    *num_group_ids = num_groups;
    p_stats->iapi_stats.iapi_ul_tti_pucch_pdus++;
}

/** @ingroup group_source_api_p7_fapi2phy_proc
 *
 *  @param[in]  p_srs_pdu Pointer to FAPI SRS PDU structure.
 *  @param[in]  p_ul_srs_chan Pointer to IAPI SRS PDU structure.
 *  
 *  @return     None.
 *
 *  @description
 *  This function converts FAPI UL_TTI.request's SRS PDU to IAPI UL_Config.request's
 *  SRS PDU structure.
 *
**/
void nr5g_fapi_srs_to_phy_srs_translation(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_ul_srs_pdu_t * p_srs_pdu,
    nr5g_fapi_srs_info_t * p_srs_info,
    SRSPDUStruct * p_ul_srs_chan)
{
    nr5g_fapi_stats_t *p_stats;

    p_stats = &p_phy_instance->stats;
    p_stats->fapi_stats.fapi_ul_tti_srs_pdus++;
    NR5G_FAPI_MEMSET(p_ul_srs_chan, sizeof(SRSPDUStruct), 0,
        sizeof(SRSPDUStruct));
    p_ul_srs_chan->sPDUHdr.nPDUType = UL_PDU_TYPE_SRS;
    p_ul_srs_chan->sPDUHdr.nPDUSize = RUP32B(sizeof(SRSPDUStruct));

    p_ul_srs_chan->nRNTI = p_srs_pdu->rnti;
    p_srs_info->handle = p_ul_srs_chan->nUEId = (uint16_t) p_srs_pdu->handle;

    p_ul_srs_chan->nSubcSpacing = p_srs_pdu->subCarrierSpacing;
    p_ul_srs_chan->nCpType = p_srs_pdu->cyclicPrefix;
    p_ul_srs_chan->nNrOfSrsPorts = pow(2, p_srs_pdu->numAntPorts);
    p_ul_srs_chan->nNrOfSymbols = pow(2, p_srs_pdu->numSymbols);
    p_ul_srs_chan->nRepetition = pow(2, p_srs_pdu->numRepetitions);
    if (p_ul_srs_chan->nCpType) {   //Extended Cyclic Prefix
        p_ul_srs_chan->nStartPos = 11 - p_srs_pdu->timeStartPosition;
    } else {
        p_ul_srs_chan->nStartPos = 13 - p_srs_pdu->timeStartPosition;
    }
    p_ul_srs_chan->nCsrs = p_srs_pdu->configIndex;
    p_ul_srs_chan->nBsrs = p_srs_pdu->bandwidthIndex;
    p_ul_srs_chan->nSrsId = p_srs_pdu->sequenceId;

    if (p_srs_pdu->combSize) {
        p_ul_srs_chan->nComb = 4;
    } else {
        p_ul_srs_chan->nComb = 2;
    }
    p_ul_srs_chan->nCombOffset = p_srs_pdu->combOffset;
    p_ul_srs_chan->nCyclicShift = p_srs_pdu->cyclicShift;
    p_ul_srs_chan->nFreqPos = p_srs_pdu->frequencyPosition;
    p_ul_srs_chan->nFreqShift = p_srs_pdu->frequencyShift;
    p_ul_srs_chan->nBHop = p_srs_pdu->frequencyHopping;
    p_ul_srs_chan->nHopping = p_srs_pdu->groupOrSequenceHopping;
    p_ul_srs_chan->nResourceType = p_srs_pdu->resourceType;
    p_ul_srs_chan->nTsrs = p_srs_pdu->tSrs;
    p_ul_srs_chan->nToffset = p_srs_pdu->tOffset;
    p_ul_srs_chan->nToffset = p_srs_pdu->tOffset;
    p_ul_srs_chan->nNrofRxRU = p_phy_instance->phy_config.n_nr_of_rx_ant;

    p_stats->iapi_stats.iapi_ul_tti_srs_pdus++;
}

 /** @ingroup group_source_api_p7_fapi2phy_proc
 *
 *  @param[in]  p_fapi_req Pointer to FAPI UL_TTI.request structure.
 *  @param[in]  p_ia_ul_config_req Pointer to IAPI UL_TTI.request structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This function converts FAPI UL_TTI.request to IAPI UL_Config.request
 *  structure.
 *
**/
uint8_t nr5g_fapi_ul_tti_req_to_phy_translation(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_ul_tti_req_t * p_fapi_req,
    PULConfigRequestStruct p_ia_ul_config_req)
{
    uint8_t i, j, num_group_id = 0;
    uint8_t num_fapi_pdus, num_groups, num_ue = 0;
    uint16_t frame_no;
    uint8_t slot_no;

    fapi_ul_tti_req_pdu_t *p_fapi_ul_tti_req_pdu;
    fapi_ue_info_t *p_fapi_ue_grp_info;
    ULSCHPDUStruct *p_ul_data_chan;
    ULCCHUCIPDUStruct *p_ul_ctrl_chan;
    SRSPDUStruct *p_ul_srs_chan;
    PUSCHGroupInfoStruct *p_pusch_grp_info;
    PDUStruct *p_pdu_head;
    nr5g_fapi_ul_slot_info_t *p_ul_slot_info;
    nr5g_fapi_stats_t *p_stats;
    nr5g_fapi_pucch_resources_t pucch_resources[FAPI_MAX_NUM_PUCCH_PDU];

    p_stats = &p_phy_instance->stats;

    frame_no = p_ia_ul_config_req->sSFN_Slot.nSFN = p_fapi_req->sfn;
    slot_no = p_ia_ul_config_req->sSFN_Slot.nSlot = p_fapi_req->slot;
    p_ia_ul_config_req->sSFN_Slot.nCarrierIdx = p_phy_instance->phy_id;

    p_ul_slot_info =
        &p_phy_instance->ul_slot_info[(slot_no % MAX_UL_SLOT_INFO_COUNT)];
    nr5g_fapi_set_ul_slot_info(frame_no, slot_no, p_ul_slot_info);

    num_fapi_pdus = p_ia_ul_config_req->nPDU = p_fapi_req->nPdus;
    num_groups = p_ia_ul_config_req->nGroup = p_fapi_req->nGroup;
    p_ia_ul_config_req->nUlsch = p_fapi_req->nUlsch;
    p_ia_ul_config_req->nUlcch = p_fapi_req->nUlcch;
    p_ia_ul_config_req->nRachPresent = p_fapi_req->rachPresent;
    if (p_fapi_req->rachPresent) {
        p_ul_slot_info->rach_presence = 1;
        p_ul_slot_info->rach_info.phy_cell_id =
            p_phy_instance->phy_config.phy_cell_id;
    }
    p_ia_ul_config_req->nUlsrs = 0;
    for (i = 0; i < num_groups; i++) {
        p_pusch_grp_info = &p_ia_ul_config_req->sPUSCHGroupInfoStruct[i];
        p_fapi_ue_grp_info = &p_fapi_req->ueGrpInfo[i];
        num_ue = p_pusch_grp_info->nUE = p_fapi_ue_grp_info->nUe;
        for (j = 0; j < num_ue; j++) {
            p_pusch_grp_info->nPduIdx[j] = p_fapi_ue_grp_info->pduIdx[j];
        }
    }
    p_pdu_head =
        (PDUStruct *) ((uint8_t *) p_ia_ul_config_req +
        sizeof(ULConfigRequestStruct));

    for (i = 0; i < num_fapi_pdus; i++) {
        p_pdu_head->nPDUSize = 0;
        p_stats->fapi_stats.fapi_ul_tti_pdus++;
        p_fapi_ul_tti_req_pdu = &p_fapi_req->pdus[i];

        switch (p_fapi_ul_tti_req_pdu->pduType) {
            case FAPI_PRACH_PDU_TYPE:
                {
                    p_ia_ul_config_req->nPDU--;
                    p_stats->fapi_stats.fapi_ul_tti_prach_pdus++;
                }
                break;

            case FAPI_PUSCH_PDU_TYPE:
                {
                    p_ul_data_chan = (ULSCHPDUStruct *) p_pdu_head;
                    nr5g_fapi_pusch_to_phy_ulsch_translation(p_phy_instance,
                        &p_ul_slot_info->pusch_info[p_ul_slot_info->num_ulsch],
                        &p_fapi_ul_tti_req_pdu->pdu.pusch_pdu, p_ul_data_chan);
                    p_ul_slot_info->num_ulsch++;
                }
                break;

            case FAPI_PUCCH_PDU_TYPE:
                {
                    p_ul_ctrl_chan = (ULCCHUCIPDUStruct *) p_pdu_head;
                    nr5g_fapi_pucch_to_phy_ulcch_uci_translation(p_phy_instance,
                        &p_ul_slot_info->pucch_info[p_ul_slot_info->num_ulcch],
                        &p_fapi_ul_tti_req_pdu->pdu.pucch_pdu, &num_group_id,
                        pucch_resources, p_ul_ctrl_chan);
                    p_ul_slot_info->num_ulcch++;
                }
                break;

            case FAPI_SRS_PDU_TYPE:
                {
                    p_ia_ul_config_req->nUlsrs++;
                    p_ul_srs_chan = (SRSPDUStruct *) p_pdu_head;
                    nr5g_fapi_srs_to_phy_srs_translation(p_phy_instance,
                        &p_fapi_ul_tti_req_pdu->pdu.srs_pdu,
                        &p_ul_slot_info->srs_info[p_ul_slot_info->num_srs],
                        p_ul_srs_chan);
                    p_ul_slot_info->num_srs++;
                }
                break;

            default:
                {
                    NR5G_FAPI_LOG(ERROR_LOG,
                        ("[NR5G_FAPI] [UL_TTI.request] Unknown PDU Type :%d",
                            p_fapi_ul_tti_req_pdu->pduType));
                    return FAILURE;
                }
        }
        p_pdu_head =
            (PDUStruct *) ((uint8_t *) p_pdu_head + p_pdu_head->nPDUSize);
        p_stats->iapi_stats.iapi_ul_tti_pdus++;
    }
    return SUCCESS;
}
