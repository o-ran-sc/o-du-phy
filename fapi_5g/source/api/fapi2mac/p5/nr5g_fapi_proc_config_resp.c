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
 * This file consist of implementation of FAPI CONFIG.response message.
 *
 **/

#include "nr5g_fapi_framework.h"
#include "gnb_l1_l2_api.h"
#include "nr5g_fapi_fapi2mac_api.h"
#include "nr5g_fapi_fapi2mac_p5_proc.h"

 /** @ingroup group_source_api_p5_fapi2mac_proc
 *
 *  @param[in]  p_phy_instance Pointer to PHY instance.
 *  @param[in]  p_fapi_resp Pointer to IAPI CONFIG.response message structure.
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This message allows PHY to report L2/L3 about CONFIG.request's status.
 *
**/
uint8_t nr5g_fapi_config_response(
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    PCONFIGRESPONSEStruct p_iapi_resp)
{
    uint8_t phy_id;
    fapi_config_resp_t *p_fapi_resp;
    p_fapi_api_queue_elem_t p_list_elem;
    p_nr5g_fapi_phy_instance_t p_phy_instance = NULL;
    nr5g_fapi_stats_t *p_stats;

    if (NULL == p_phy_ctx) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[CONFIG.response] Invalid " "phy context"));
        return FAILURE;
    }

    if (NULL == p_iapi_resp) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[CONFIG.response] Invalid iapi "
                "config response message"));
        return FAILURE;
    }

    phy_id = p_iapi_resp->nCarrierIdx;
    p_phy_instance = &p_phy_ctx->phy_instance[phy_id];
    if (p_phy_instance->phy_id != phy_id) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[CONFIG.response] Invalid " "phy instance"));
        return FAILURE;
    }
    // Create FAPI message header
    if (FAPI_STATE_IDLE == p_phy_instance->state)
        nr5g_fapi_message_header_per_phy(phy_id, false);

    p_stats = &p_phy_instance->stats;
    p_stats->iapi_stats.iapi_config_res++;
    p_list_elem =
        nr5g_fapi_fapi2mac_create_api_list_elem(FAPI_CONFIG_RESPONSE, 1,
        sizeof(fapi_config_resp_t));
    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[CONFIG.response] Unable to create "
                "list element. Out of memory!!!"));
        return FAILURE;
    }

    p_fapi_resp = (fapi_config_resp_t *) (p_list_elem + 1);
    p_fapi_resp->header.msg_id = FAPI_CONFIG_RESPONSE;
    p_fapi_resp->header.length = (uint16_t) sizeof(fapi_config_resp_t);
    p_fapi_resp->error_code = p_iapi_resp->nStatus;

    if (FAPI_STATE_IDLE == p_phy_instance->state && 0 == p_iapi_resp->nStatus) {
        p_phy_ctx->num_phy_instance += 1;
        p_phy_instance->state = FAPI_STATE_CONFIGURED;
    }
    // TODO: Update phy_id to 0 in phy_instance on error.

    /* TLV report is not supported in PHY */
    p_fapi_resp->number_of_invalid_tlvs = 0;
    p_fapi_resp->number_of_inv_tlvs_idle_only = 0;
    p_fapi_resp->number_of_inv_tlvs_running_only = 0;
    p_fapi_resp->number_of_missing_tlvs = 0;

    // Add element to send list
    nr5g_fapi_fapi2mac_add_api_to_list(phy_id, p_list_elem, false);

    p_stats->fapi_stats.fapi_config_res++;
    NR5G_FAPI_LOG(INFO_LOG, ("[CONFIG.response][%d]", phy_id));

    return SUCCESS;
}
