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
 * This file consist of implementation of FAPI UL_IQ_SAMPLES.request message.
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
 *  @param[in]  p_fapi_req Pointer to FAPI UL_IQ_SAMPLES.request message structure.
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This is a timer mode specific message used to transmit IQ Sample API to L1.
 *  In UL direction, L1 opens the IQ samples file and stores it into local
 *  buffers for processing. 
 *
 *
**/
#ifdef DEBUG_MODE
uint8_t nr5g_fapi_ul_iq_samples_request(
    bool is_urllc,
    fapi_vendor_ext_iq_samples_req_t * p_fapi_req)
{
    uint16_t num_ant;

    fapi_vendor_ext_iq_samples_info_t *p_file_info;
    PMAC2PHY_QUEUE_EL p_list_elem;

    if (NULL == p_fapi_req) {
        NR5G_FAPI_LOG(ERROR_LOG, (" [UL_IQ_SAMPLES.request] Invalid fapi "
                "message"));
        return FAILURE;
    }

    p_list_elem = nr5g_fapi_fapi2phy_create_api_list_elem((uint8_t)
        MSG_TYPE_PHY_UL_IQ_SAMPLES, 1,
        (uint32_t) sizeof(fapi_vendor_ext_iq_samples_info_t));
    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[UL_IQ_SAMPLES.request] Unable to create "
                "list element. Out of memory!!!"));
        return FAILURE;
    }

    p_file_info = (fapi_vendor_ext_iq_samples_info_t *) (p_list_elem + 1);
    p_file_info->carrNum = p_fapi_req->iq_samples_info.carrNum;
    p_file_info->numSubframes = p_fapi_req->iq_samples_info.numSubframes;
    p_file_info->nIsRadioMode = p_fapi_req->iq_samples_info.nIsRadioMode;
    p_file_info->timerModeFreqDomain =
        p_fapi_req->iq_samples_info.timerModeFreqDomain;
    p_file_info->phaseCompensationEnable =
        p_fapi_req->iq_samples_info.phaseCompensationEnable;
    p_file_info->startFrameNum = p_fapi_req->iq_samples_info.startFrameNum;
    p_file_info->startSlotNum = p_fapi_req->iq_samples_info.startSlotNum;
    p_file_info->startSymNum = p_fapi_req->iq_samples_info.startSymNum;

    p_file_info->nDLCompressionIdx = p_fapi_req->iq_samples_info.nDLCompressionIdx;
    p_file_info->nDLCompiqWidth = p_fapi_req->iq_samples_info.nDLCompiqWidth;
    p_file_info->nDLCompScaleFactor = p_fapi_req->iq_samples_info.nDLCompScaleFactor;
    p_file_info->nDLCompreMask = p_fapi_req->iq_samples_info.nDLCompreMask;
    p_file_info->nULDecompressionIdx = p_fapi_req->iq_samples_info.nULDecompressionIdx;
    p_file_info->nULDecompiqWidth = p_fapi_req->iq_samples_info.nULDecompiqWidth;

    if (FAILURE == NR5G_FAPI_MEMCPY(p_file_info->buffer,
            sizeof(uint8_t) * FAPI_MAX_IQ_SAMPLE_BUFFER_SIZE,
            p_fapi_req->iq_samples_info.buffer, sizeof(CONFIGREQUESTStruct))) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[UL_IQ_Samples.request] Buffer copy "
                "failed!!!"));
    }

    for (num_ant = 0; num_ant < FAPI_MAX_IQ_SAMPLE_UL_VIRTUAL_PORTS; num_ant++) {
        if (FAILURE == NR5G_FAPI_STRCPY(p_file_info->filename_in_ul_iq[num_ant],
                sizeof(uint8_t) * FAPI_MAX_IQ_SAMPLE_FILE_SIZE,
                p_fapi_req->iq_samples_info.filename_in_ul_iq[num_ant],
                sizeof(uint8_t) * FAPI_MAX_IQ_SAMPLE_FILE_SIZE)) {
            NR5G_FAPI_LOG(ERROR_LOG, ("[UL_IQ_Samples.request] file name copy "
                    "failed!!!"));
        }

        if (FAILURE ==
            NR5G_FAPI_STRCPY(p_file_info->filename_in_prach_iq[num_ant],
                sizeof(uint8_t) * FAPI_MAX_IQ_SAMPLE_FILE_SIZE,
                p_fapi_req->iq_samples_info.filename_in_prach_iq[num_ant],
                sizeof(uint8_t) * FAPI_MAX_IQ_SAMPLE_FILE_SIZE)) {
            NR5G_FAPI_LOG(ERROR_LOG,
                ("[UL_IQ_Samples.request] PRACH file name " "copy failed!!!"));
        }

        if (FAILURE ==
            NR5G_FAPI_STRCPY(p_file_info->filename_in_ul_iq_compressed[num_ant],
                sizeof(uint8_t) * FAPI_MAX_IQ_SAMPLE_FILE_SIZE,
                p_fapi_req->iq_samples_info.filename_in_ul_iq_compressed[num_ant],
                sizeof(uint8_t) * FAPI_MAX_IQ_SAMPLE_FILE_SIZE)) {
            NR5G_FAPI_LOG(ERROR_LOG,
                ("[UL_IQ_Samples.request] compressed file name copy failed!!!"));
        }
    }

    for (num_ant = 0; num_ant < FAPI_MAX_IQ_SAMPLE_UL_ANTENNA; num_ant++) {
        if (FAILURE ==
            NR5G_FAPI_STRCPY(p_file_info->filename_in_srs_iq[num_ant],
                sizeof(uint8_t) * FAPI_MAX_IQ_SAMPLE_FILE_SIZE,
                p_fapi_req->iq_samples_info.filename_in_srs_iq[num_ant],
                sizeof(uint8_t) * FAPI_MAX_IQ_SAMPLE_FILE_SIZE)) {
            NR5G_FAPI_LOG(ERROR_LOG,
                ("[UL_IQ_Samples.request] SRS file name " "copy failed!!!"));
        }
    }

    nr5g_fapi_fapi2phy_add_to_api_list(is_urllc, p_list_elem);
    NR5G_FAPI_LOG(INFO_LOG, ("[UL_IQ_SAMPLES.request][%d]",
            p_fapi_req->iq_samples_info.carrNum));

    return SUCCESS;
}
#endif
