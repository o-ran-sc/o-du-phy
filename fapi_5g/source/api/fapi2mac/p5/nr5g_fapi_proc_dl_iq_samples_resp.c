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
 * This file consist of implementation of FAPI DL_IQ_SAMPLES.response message.
 *
 **/
#include "nr5g_fapi_framework.h"
#include "gnb_l1_l2_api.h"
#include "nr5g_fapi_fapi2mac_api.h"
#include "nr5g_fapi_fapi2mac_p5_proc.h"

#ifdef DEBUG_MODE
 /** @ingroup group_source_api_p5_fapi2phy_proc
 *
 *  @param[in]  p_phy_ctx Pointer to PHY context.
 *  @param[in]  p_fapi_resp Pointer to IAPI DL_IQ_SAMPLES.response message structure.
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This timer mode vendor specific message used for acknowledging the DL_IQ_SAMPLES.Request
 *  message used by TestMAC to validate Downlink messages
 *
**/
uint8_t nr5g_fapi_dl_iq_samples_response(
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    PADD_REMOVE_BBU_CORES p_iapi_resp)
{
    uint8_t phy_id;
    fapi_vendor_ext_dl_iq_samples_res_t *p_fapi_resp;
    p_fapi_api_queue_elem_t p_list_elem;
    p_nr5g_fapi_phy_instance_t p_phy_instance = NULL;

    if (NULL == p_phy_ctx) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[DL_IQ_SAMPLES.response] Invalid "
                "phy ctx"));
        return FAILURE;
    }

    if (NULL == p_iapi_resp) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[DL_IQ_SAMPLES.response] Invalid iapi "
                "message"));
        return FAILURE;
    }

    phy_id = p_iapi_resp->sSFN_Slot.nCarrierIdx;
    p_phy_instance = &p_phy_ctx->phy_instance[phy_id];

    if (p_phy_instance->phy_id != phy_id) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[DL_IQ_SAMPLES.response] Invalid "
                "phy instance"));
        return FAILURE;
    }

    p_list_elem =
        nr5g_fapi_fapi2mac_create_api_list_elem(FAPI_VENDOR_EXT_DL_IQ_SAMPLES,
        1, sizeof(fapi_vendor_ext_dl_iq_samples_res_t));
    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[DL_IQ_SAMPLES.response] Unable to create "
                "list element. Out of memory!!!"));
        return FAILURE;
    }

    p_fapi_resp = (fapi_vendor_ext_dl_iq_samples_res_t *) (p_list_elem + 1);
    p_fapi_resp->header.msg_id = FAPI_VENDOR_EXT_DL_IQ_SAMPLES;
    p_fapi_resp->header.length =
        (uint16_t) sizeof(fapi_vendor_ext_dl_iq_samples_res_t);

    /* Add element to send list */
    nr5g_fapi_fapi2mac_add_api_to_list(phy_id, p_list_elem, false);

    NR5G_FAPI_LOG(INFO_LOG, ("[DL_IQ_SAMPLES.response][%d]", phy_id));

    return SUCCESS;
}
#endif
