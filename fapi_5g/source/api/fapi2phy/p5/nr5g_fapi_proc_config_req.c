/******************************************************************************
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
 * This file consist of implementation of FAPI CONFIG.request message.
 *
 **/

#include "nr5g_fapi_framework.h"
#include "gnb_l1_l2_api.h"
#include "nr5g_fapi_fapi2mac_api.h"
#include "nr5g_fapi_fapi2phy_api.h"
#include "nr5g_fapi_fapi2phy_p5_proc.h"
#include "nr5g_fapi_fapi2phy_p5_pvt_proc.h"
#include "nr5g_fapi_memory.h"

 /** @ingroup group_source_api_p5_fapi2phy_proc
 *
 *  @param[in]  p_phy_instance Pointer to PHY instance.
 *  @param[in]  p_fapi_req Pointer to FAPI CONFIG.request message structure.
 *  @param[in]  p_fapi_vendor_msg Pointer to FAPI vendor message structure.
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This message instructs how the PHY should be configured.
 *
 *  The *carrier_aggregation_level* parameter is a vendor specific 
 *  configuration and programmed through Vendor Specific Message structure 
 *  ::fapi_config_req_vendor_msg_t 
 *
**/
uint8_t nr5g_fapi_config_request(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_config_req_t * p_fapi_req,
    fapi_vendor_msg_t * p_fapi_vendor_msg)
{
    PCONFIGREQUESTStruct p_ia_config_req;
    PMAC2PHY_QUEUE_EL p_list_elem;
    nr5g_fapi_stats_t *p_stats;

#ifndef DEBUG_MODE
    /* Below print is for better logging on console in radio mode. */
    NR5G_FAPI_LOG(INFO_LOG, (""));
#endif

    if (NULL == p_phy_instance) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[CONFIG.request] Invalid " "phy instance"));
        return FAILURE;
    }
    p_stats = &p_phy_instance->stats;
    p_stats->fapi_stats.fapi_config_req++;

    if (NULL == p_fapi_req) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[CONFIG.request] Invalid fapi " "message"));
        return FAILURE;
    }

    if (NULL == p_fapi_vendor_msg) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[CONFIG.request] Invalid fapi "
                "vendor message"));
        return FAILURE;
    }

    if (FAPI_STATE_RUNNING == p_phy_instance->state) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[CONFIG.request] Message not "
                "supported by PHY in Running State"));
        return FAILURE;
    }

    p_list_elem = nr5g_fapi_fapi2phy_create_api_list_elem((uint8_t)
        MSG_TYPE_PHY_CONFIG_REQ, 1, (uint32_t) sizeof(CONFIGREQUESTStruct));
    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[CONFIG.request] Unable to create "
                "list element. Out of memory!!!"));
        return FAILURE;
    }

    p_ia_config_req = (PCONFIGREQUESTStruct) (p_list_elem + 1);
    NR5G_FAPI_MEMSET(p_ia_config_req, sizeof(CONFIGREQUESTStruct), 0,
        sizeof(CONFIGREQUESTStruct));
    p_ia_config_req->sMsgHdr.nMessageType = MSG_TYPE_PHY_CONFIG_REQ;
    p_ia_config_req->sMsgHdr.nMessageLen =
        (uint16_t) sizeof(CONFIGREQUESTStruct);
    p_ia_config_req->nCarrierIdx = p_phy_instance->phy_id;

    nr5g_fapi_config_req_to_phy_translation(p_phy_instance, p_fapi_req,
        p_ia_config_req);
    /* Vendor Parameters */
    if (p_fapi_vendor_msg) {
        p_ia_config_req->nCarrierAggregationLevel =
            p_fapi_vendor_msg->config_req_vendor.carrier_aggregation_level;
        p_ia_config_req->nGroupHopFlag =
            p_fapi_vendor_msg->config_req_vendor.group_hop_flag;
        p_ia_config_req->nSequenceHopFlag =
            p_fapi_vendor_msg->config_req_vendor.sequence_hop_flag;
        p_ia_config_req->nHoppingId =
            p_fapi_vendor_msg->config_req_vendor.hopping_id;
        p_ia_config_req->nUrllcCapable =
            p_fapi_vendor_msg->config_req_vendor.urllc_capable;
        p_ia_config_req->nUrllcMiniSlotMask =
            p_fapi_vendor_msg->config_req_vendor.urllc_mini_slot_mask;
        p_ia_config_req->nPrachNrofRxRU =
            p_fapi_vendor_msg->config_req_vendor.prach_nr_of_rx_ru;
        p_ia_config_req->nNrOfDLPorts =
            p_fapi_vendor_msg->config_req_vendor.nr_of_dl_ports;
        p_ia_config_req->nNrOfULPorts =
            p_fapi_vendor_msg->config_req_vendor.nr_of_ul_ports;
        p_ia_config_req->nSSBSubcSpacing =
            p_fapi_vendor_msg->config_req_vendor.ssb_subc_spacing;
        p_phy_instance->phy_config.use_vendor_EpreXSSB =
            p_fapi_vendor_msg->config_req_vendor.use_vendor_EpreXSSB;
    }

    p_ia_config_req->nDLFftSize =
        nr5g_fapi_calc_fft_size(p_ia_config_req->nSubcCommon,
        p_ia_config_req->nDLBandwidth);
    p_ia_config_req->nULFftSize =
        nr5g_fapi_calc_fft_size(p_ia_config_req->nSubcCommon,
        p_ia_config_req->nULBandwidth);

    /* Add element to send list */
    nr5g_fapi_fapi2phy_add_to_api_list(is_urllc, p_list_elem);

    p_stats->iapi_stats.iapi_config_req++;
    NR5G_FAPI_LOG(INFO_LOG, ("[CONFIG.request][%d]", p_phy_instance->phy_id));
    return SUCCESS;
}

 /** @ingroup group_source_api_p5_fapi2phy_proc
 *
 *  @param[in]  p_phy_instance Pointer to PHY instance.
 *  @param[in]  p_fapi_req Pointer to FAPI CONFIG.request structure.
 *  @param[in]  p_ia_config_req Pointer to IAPI CONFIG.request structure.
 *  
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This function converts FAPI CONFIG.request TLVs to IAPI Config.request
 *  structure.
 *
**/
uint8_t nr5g_fapi_config_req_to_phy_translation(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_config_req_t * p_fapi_req,
    PCONFIGREQUESTStruct p_ia_config_req)
{
    fapi_uint32_tlv_t *tlvs = p_fapi_req->tlvs;
    uint32_t i = 0, j = 0, k = 0;
    uint32_t ss_pbch_power = 0;
    uint32_t mib = 0;
    uint32_t n_ssb_mask_idx = 0;
    uint32_t n_beamid_idx = 0;
    SLOTCONFIGStruct *p_sslot_Config = NULL;

    while (i < p_fapi_req->number_of_tlvs) {
        switch (tlvs[i].tl.tag) {
            /***** Carrier Config *****/
            case FAPI_DL_BANDWIDTH_TAG:
                p_ia_config_req->nDLBandwidth =
                    GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length);
                break;

            case FAPI_DL_FREQUENCY_TAG:
                p_ia_config_req->nDLAbsFrePointA = tlvs[i++].value;
                break;

                /* FAPI_DL_K0_TAG - NA */
                /* FAPI_DL_GRIDSIZE_TAG - NA */

            case FAPI_NUM_TX_ANT_TAG:
                p_ia_config_req->nNrOfTxAnt =
                    GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length);
                break;

            case FAPI_UPLINK_BANDWIDTH_TAG:
                p_ia_config_req->nULBandwidth =
                    GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length);
                break;

            case FAPI_UPLINK_FREQUENCY_TAG:
                p_ia_config_req->nULAbsFrePointA = tlvs[i++].value;
                break;

                /* FAPI_UL_K0_TAG - NA */
                /* FAPI_UL_GRIDSIZE_TAG - NA */

            case FAPI_NUM_RX_ANT_TAG:
                p_phy_instance->phy_config.n_nr_of_rx_ant =
                    p_ia_config_req->nNrOfRxAnt =
                    GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length);
                break;

                /* FAPI_FREQUENCY_SHIFT_7P5_KHZ_TAG - NA */

            /***** Cell Config *****/
            case FAPI_PHY_CELL_ID_TAG:
                p_phy_instance->phy_config.phy_cell_id =
                    p_ia_config_req->nPhyCellId =
                    GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length);
                break;

            case FAPI_FRAME_DUPLEX_TYPE_TAG:
                p_ia_config_req->nFrameDuplexType =
                    GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length);
                break;

            /***** SSB Config *****/
            case FAPI_SS_PBCH_POWER_TAG:
                ss_pbch_power = tlvs[i++].value;
                if (0 == ss_pbch_power) {
                    p_ia_config_req->nSSBPwr = 1;
                } else if (54002 == ss_pbch_power) {
                    p_ia_config_req->nSSBPwr = 20000;
                } else {
                    p_ia_config_req->nSSBPwr = ss_pbch_power - 54000;
                }
                break;

                /* FAPI_BCH_PAYLOAD_TAG - NA */

            case FAPI_SCS_COMMON_TAG:
                p_ia_config_req->nSubcCommon =
                    p_ia_config_req->nSSBSubcSpacing =
                    p_phy_instance->phy_config.sub_c_common =
                    GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length);
                break;

            /***** PRACH Config *****/
            case FAPI_PRACH_SUBC_SPACING_TAG:
                p_ia_config_req->nPrachSubcSpacing =
                    GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length);
                break;

            case FAPI_RESTRICTED_SET_CONFIG_TAG:
                p_ia_config_req->nPrachRestrictSet =
                    GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length);
                break;

            case FAPI_NUM_PRACH_FD_OCCASIONS_TAG:
                p_ia_config_req->nPrachFdm =
                    GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length);
                break;

            case FAPI_PRACH_CONFIG_INDEX_TAG:
                p_ia_config_req->nPrachConfIdx =
                    GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length);
                break;

            case FAPI_PRACH_ROOT_SEQUENCE_INDEX_TAG:
                p_ia_config_req->nPrachRootSeqIdx =
                    GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length);
                break;

            case FAPI_K1_TAG:
                p_ia_config_req->nPrachFreqStart =
                    GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length);
                break;

            case FAPI_PRACH_ZERO_CORR_CONF_TAG:
                p_ia_config_req->nPrachZeroCorrConf =
                    GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length);
                break;

            case FAPI_SSB_PER_RACH_TAG:
                p_ia_config_req->nPrachSsbRach =
                    GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length);
                break;
            /***** SSB Table *****/
            case FAPI_SSB_OFFSET_POINT_A_TAG:
                p_ia_config_req->nSSBPrbOffset =
                    GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length) / (pow(2,
                        p_ia_config_req->nSubcCommon));
                break;

            case FAPI_SSB_PERIOD_TAG:
                p_ia_config_req->nSSBPeriod =
                    GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length);
                break;

            case FAPI_SSB_SUBCARRIER_OFFSET_TAG:
                p_ia_config_req->nSSBSubcOffset =
                    (tlvs[i].value >> tlvs[i++].tl.length);
                break;

            case FAPI_MIB_TAG:
                mib = tlvs[i++].value;
                p_ia_config_req->nMIB[0] = (uint8_t) (mib >> 24);
                p_ia_config_req->nMIB[1] = (uint8_t) (mib >> 16);
                p_ia_config_req->nMIB[2] = (uint8_t) (mib >> 8);
                break;

            case FAPI_DMRS_TYPE_A_POS_TAG:
                p_ia_config_req->nDMRSTypeAPos =
                    GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length);
                break;

            case FAPI_SSB_MASK_TAG:
                if (n_ssb_mask_idx < 2) {
                    p_ia_config_req->nSSBMask[n_ssb_mask_idx++] =
                        tlvs[i++].value;
                }
                break;

            case FAPI_BEAM_ID_TAG:
                if (n_beamid_idx < MAX_NUM_ANT) {
                    p_ia_config_req->nBeamId[n_beamid_idx++] =
                        GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length);
                }
                break;

                /* FAPI_SS_PBCH_MULTIPLE_CARRIERS_IN_A_BAND_TAG - NA */
                /* FAPI_MULTIPLE_CELLS_SS_PBCH_IN_A_CARRIER_TAG - NA */

            /***** TDD Table *****/
            case FAPI_TDD_PERIOD_TAG:
                p_ia_config_req->nTddPeriod =
                    nr5g_fapi_calc_phy_tdd_period((uint8_t)
                    GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length),
                    p_ia_config_req->nSubcCommon);
                break;

            case FAPI_SLOT_CONFIG_TAG:
                for (j = 0; j < p_ia_config_req->nTddPeriod; j++) {
                    p_sslot_Config = &p_ia_config_req->sSlotConfig[j];
                    for (k = 0; k < MAX_NUM_OF_SYMBOL_PER_SLOT; k++) {
                        p_sslot_Config->nSymbolType[k] =
                            GETVLFRM32B(tlvs[i].value, tlvs[i++].tl.length);
                    }
                }
                break;

            /***** Measurement Config *****/
                /* FAPI_RSSI_MEASUREMENT_TAG - NA */

            /***** Beamforming Table *****/

            /***** Precoding Table *****/
            default:
                {
                    NR5G_FAPI_LOG(ERROR_LOG, ("[CONFIG.request] Unsupported "
                            "TLV tag : 0x%x", tlvs[i].tl.tag));
                }
                break;
        }
    }
    nr5g_fapi_config_req_fill_dependent_fields(p_ia_config_req);
    return SUCCESS;
}

 /** @ingroup group_source_api_p5_fapi2phy_proc
 *
 *  @param[in,out]  p_ia_config_req Pointer to IAPI CONFIG.request structure.
 *
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This function converts IAPI Config.request structure fields that depend on
 *  others. The order ofLV 5G FAPI 222.10.02 - 3.3.2.1
 *
**/
uint8_t nr5g_fapi_config_req_fill_dependent_fields(
    PCONFIGREQUESTStruct p_ia_config_req)
{
    if (0 == p_ia_config_req->nFrameDuplexType) { // FDD
        p_ia_config_req->nTddPeriod = 0;
    }

    return SUCCESS;
}

 /** @ingroup group_source_api_p5_fapi2phy_proc
 *
 *  @param[in]  fapi_tdd_period DL UL Transmission Periodicity. 
 *  @param[in]  n_subc_common subcarrierSpacing for common.
 *  
 *  @return     IAPI *nTddPeriod*.
 *
 *  @description
 *  This function converts FAPI *TddPeriod* to IAPI *nTddPeriod* 
 *  structure.
 *
**/
uint8_t nr5g_fapi_calc_phy_tdd_period(
    uint8_t fapi_tdd_period,
    uint8_t n_subc_common)
{
    switch (n_subc_common) {
        case 0:
            return
                nr5g_fapi_calc_phy_tdd_period_for_n_subc_common_0
                (fapi_tdd_period);
        case 1:
            return
                nr5g_fapi_calc_phy_tdd_period_for_n_subc_common_1
                (fapi_tdd_period);
        case 2:
            return
                nr5g_fapi_calc_phy_tdd_period_for_n_subc_common_2
                (fapi_tdd_period);
        case 3:
            return
                nr5g_fapi_calc_phy_tdd_period_for_n_subc_common_3
                (fapi_tdd_period);
        default:
            break;
    }

    return 0;
}

 /** @ingroup group_source_api_p5_fapi2phy_proc
 *
 *  @param[in]  fapi_tdd_period DL UL Transmission Periodicity. 
 *  
 *  @return     IAPI *nTddPeriod*.
 *
 *  @description
 *  This function converts FAPI *TddPeriod* to IAPI *nTddPeriod* 
 *  structure based on *subCarrierSpacingCommon - 0*.
 *
**/
uint8_t nr5g_fapi_calc_phy_tdd_period_for_n_subc_common_0(
    uint8_t fapi_tdd_period)
{
    if (2 == fapi_tdd_period)
        return 1;
    else if (4 == fapi_tdd_period)
        return 2;
    else if (6 == fapi_tdd_period)
        return 5;
    else if (7 == fapi_tdd_period)
        return 10;
    else
        return 0;

    return 0;
}

 /** @ingroup group_source_api_p5_fapi2phy_proc
 *
 *  @param[in]  fapi_tdd_period DL UL Transmission Periodicity. 
 *  
 *  @return     IAPI *nTddPeriod*.
 *
 *  @description
 *  This function converts FAPI *TddPeriod* to IAPI *nTddPeriod* 
 *  structure based on *subCarrierSpacingCommon - 1*.
 *
**/
uint8_t nr5g_fapi_calc_phy_tdd_period_for_n_subc_common_1(
    uint8_t fapi_tdd_period)
{
    if (0 == fapi_tdd_period)
        return 1;
    else if (2 == fapi_tdd_period)
        return 2;
    else if (4 == fapi_tdd_period)
        return 4;
    else if (5 == fapi_tdd_period)
        return 5;
    else if (6 == fapi_tdd_period)
        return 10;
    else if (7 == fapi_tdd_period)
        return 20;
    else
        return 0;

    return 0;
}

 /** @ingroup group_source_api_p5_fapi2phy_proc
 *
 *  @param[in]  fapi_tdd_period DL UL Transmission Periodicity. 
 *  
 *  @return     IAPI *nTddPeriod*.
 *
 *  @description
 *  This function converts FAPI *TddPeriod* to IAPI *nTddPeriod* 
 *  structure based on *subCarrierSpacingCommon - 2*.
 *
**/
uint8_t nr5g_fapi_calc_phy_tdd_period_for_n_subc_common_2(
    uint8_t fapi_tdd_period)
{
    if (0 == fapi_tdd_period)
        return 2;
    else if (2 == fapi_tdd_period)
        return 4;
    else if (3 == fapi_tdd_period)
        return 5;
    else if (4 == fapi_tdd_period)
        return 8;
    else if (5 == fapi_tdd_period)
        return 10;
    else if (6 == fapi_tdd_period)
        return 20;
    else if (7 == fapi_tdd_period)
        return 40;
    else
        return 0;

    return 0;
}

 /** @ingroup group_source_api_p5_fapi2phy_proc
 *
 *  @param[in]  fapi_tdd_period DL UL Transmission Periodicity. 
 *  
 *  @return     IAPI *nTddPeriod*.
 *
 *  @description
 *  This function converts FAPI *TddPeriod* to IAPI *nTddPeriod* 
 *  structure based on *subCarrierSpacingCommon - 3*.
 *
**/
uint8_t nr5g_fapi_calc_phy_tdd_period_for_n_subc_common_3(
    uint8_t fapi_tdd_period)
{
    if (0 == fapi_tdd_period)
        return 4;
    else if (1 == fapi_tdd_period)
        return 5;
    else if (2 == fapi_tdd_period)
        return 8;
    else if (3 == fapi_tdd_period)
        return 10;
    else if (4 == fapi_tdd_period)
        return 16;
    else if (5 == fapi_tdd_period)
        return 20;
    else if (6 == fapi_tdd_period)
        return 40;
    else if (7 == fapi_tdd_period)
        return 80;
    else
        return 0;

    return 0;
}

 /** @ingroup group_source_api_p5_fapi2phy_proc
 *
 *  @param[in]  nSubcCommon  Sub carrier spacing
 *  @param[in]  nDLBandwidth/ nULBandwidth  Sub carrier spacing
 *  
 *  
 *  @return     IAPI FFT size for DL UL
 *
 *  @description
 *  This function converts FAPI *TddPeriod* to IAPI *nTddPeriod* 
 *  structure based on *subCarrierSpacingCommon - 3*.
 *
**/
uint16_t nr5g_fapi_calc_fft_size(
    uint8_t sub_carrier_common,
    uint16_t bw)
{
    if (FAPI_SUBCARRIER_SPACING_15 == sub_carrier_common) {
        if (FAPI_BANDWIDTH_5_MHZ == bw)
            return FAPI_FFT_SIZE_512;
        else if (FAPI_BANDWIDTH_10_MHZ == bw)
            return FAPI_FFT_SIZE_1024;
        else if ((FAPI_BANDWIDTH_15_MHZ == bw) || (FAPI_BANDWIDTH_20_MHZ == bw))
            return FAPI_FFT_SIZE_2048;
        else if ((FAPI_BANDWIDTH_25_MHZ == bw) || (FAPI_BANDWIDTH_30_MHZ == bw)
            || (FAPI_BANDWIDTH_40_MHZ == bw) || (FAPI_BANDWIDTH_50_MHZ == bw))
            return FAPI_FFT_SIZE_4096;
    } else if (FAPI_SUBCARRIER_SPACING_30 == sub_carrier_common) {
        if ((FAPI_BANDWIDTH_5_MHZ == bw) || (FAPI_BANDWIDTH_10_MHZ == bw))
            return FAPI_FFT_SIZE_512;
        else if ((FAPI_BANDWIDTH_15_MHZ == bw) || (FAPI_BANDWIDTH_20_MHZ == bw))
            return FAPI_FFT_SIZE_1024;
        else if ((FAPI_BANDWIDTH_25_MHZ == bw) || (FAPI_BANDWIDTH_30_MHZ == bw)
            || (FAPI_BANDWIDTH_40_MHZ == bw) || (FAPI_BANDWIDTH_50_MHZ == bw))
            return FAPI_FFT_SIZE_2048;
        else if ((FAPI_BANDWIDTH_60_MHZ == bw) || (FAPI_BANDWIDTH_70_MHZ == bw)
            || (FAPI_BANDWIDTH_80_MHZ == bw) || ((FAPI_BANDWIDTH_90_MHZ == bw))
            || (FAPI_BANDWIDTH_100_MHZ == bw))
            return FAPI_FFT_SIZE_4096;
    } else if (FAPI_SUBCARRIER_SPACING_60 == sub_carrier_common) {
        if ((FAPI_BANDWIDTH_10_MHZ == bw) || (FAPI_BANDWIDTH_15_MHZ == bw) ||
            (FAPI_BANDWIDTH_20_MHZ == bw) || (FAPI_BANDWIDTH_25_MHZ == bw))
            return FAPI_FFT_SIZE_512;
        else if ((FAPI_BANDWIDTH_30_MHZ == bw) || (FAPI_BANDWIDTH_40_MHZ == bw)
            || (FAPI_BANDWIDTH_50_MHZ == bw))
            return FAPI_FFT_SIZE_1024;
        else if ((FAPI_BANDWIDTH_60_MHZ == bw) || (FAPI_BANDWIDTH_70_MHZ == bw)
            || (FAPI_BANDWIDTH_80_MHZ == bw) || (FAPI_BANDWIDTH_90_MHZ == bw) ||
            (FAPI_BANDWIDTH_100_MHZ == bw))
            return FAPI_FFT_SIZE_2048;
        else if (FAPI_BANDWIDTH_200_MHZ == bw)
            return FAPI_FFT_SIZE_4096;
    } else if (FAPI_SUBCARRIER_SPACING_120 == sub_carrier_common) {
        if (FAPI_BANDWIDTH_50_MHZ == bw)
            return FAPI_FFT_SIZE_512;
        else if (FAPI_BANDWIDTH_100_MHZ == bw)
            return FAPI_FFT_SIZE_1024;
        else if (FAPI_BANDWIDTH_200_MHZ == bw)
            return FAPI_FFT_SIZE_2048;
        else if (FAPI_BANDWIDTH_400_MHZ == bw)
            return FAPI_FFT_SIZE_4096;
    } else {
    }

    return 0;
}
