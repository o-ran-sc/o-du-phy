/******************************************************************************
*
*   Copyright (c) 2022 Intel.
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
 * This file consist of implementation of FAPI PARAM.response message.
 *
 **/

#include "nr5g_fapi_framework.h"
#include "nr5g_fapi_fapi2mac_api.h"
#include "nr5g_fapi_fapi2mac_p5_proc.h"

 /** @ingroup group_source_api_p5_fapi2mac_proc
 *
 *  @param[in]  p_phy_instance Pointer to PHY instance.
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This message allows PHY to report L2/L3 about PARAM.request's status.
 *
**/
uint8_t nr5g_fapi_param_response(
    p_nr5g_fapi_phy_instance_t p_phy_instance)
{
    fapi_param_resp_t *p_fapi_resp;
    p_fapi_api_queue_elem_t p_list_elem;
    nr5g_fapi_stats_t *p_stats;

    // Create FAPI message header
    nr5g_fapi_message_header_per_phy(p_phy_instance->phy_id, false);

    p_stats = &p_phy_instance->stats;
    p_stats->iapi_stats.iapi_param_res++;
    p_list_elem =
        nr5g_fapi_fapi2mac_create_api_list_elem(FAPI_PARAM_RESPONSE, 1,
        sizeof(fapi_param_resp_t));
    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[PARAM.response] Unable to create "
                "list element. Out of memory!!!"));
        return FAILURE;
    }

    p_fapi_resp = (fapi_param_resp_t *) (p_list_elem + 1);
    p_fapi_resp->header.msg_id = FAPI_PARAM_RESPONSE;
    p_fapi_resp->header.length = (uint16_t) sizeof(fapi_param_resp_t);
    p_fapi_resp->error_code =
        (p_phy_instance->state == FAPI_STATE_RUNNING) ? MSG_INVALID_STATE : MSG_OK;

    /* TLV report is not supported in PHY */
    p_fapi_resp->number_of_tlvs = 0;

    // Add element to send list
    nr5g_fapi_fapi2mac_add_api_to_list(p_phy_instance->phy_id, p_list_elem, false);

    p_stats->fapi_stats.fapi_param_res++;
    NR5G_FAPI_LOG(INFO_LOG, ("[PARAM.response][%d]", p_phy_instance->phy_id));
    nr5g_fapi_fapi2mac_send_api_list(false);

    return SUCCESS;
}
