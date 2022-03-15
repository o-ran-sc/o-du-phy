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
#include "nr5g_fapi_internal.h"

 /** @ingroup group_source_api_p5_fapi2mac_proc
 *
 *  @param[in]  p_phy_instance Pointer to PHY instance.
 *  @param[in]  p_fapi_resp Pointer to IAPI CONFIG.response message structure.
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This Message is the FAPI Message Header.
 *
**/
uint8_t nr5g_fapi_message_header(
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    bool is_urllc)
{
    uint8_t phy_id = 0;

    for (phy_id = 0; phy_id < FAPI_MAX_PHY_INSTANCES; phy_id++) {
        if ((FAPI_STATE_CONFIGURED == p_phy_ctx->phy_instance[phy_id].state) ||
            (FAPI_STATE_RUNNING == p_phy_ctx->phy_instance[phy_id].state)) {
            nr5g_fapi_message_header_per_phy(phy_id, is_urllc);
        }
    }

    return SUCCESS;
}

/** @ingroup group_source_api_p5_fapi2mac_proc
 *
 *  @param[in]  p_phy_instance Pointer to PHY instance.
 *  @param[in]  p_fapi_resp Pointer to IAPI CONFIG.response message structure.
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This Message is the FAPI Message Header.
 *
**/
uint8_t nr5g_fapi_message_header_per_phy(
    uint8_t phy_id,
    bool is_urllc)
{
    p_fapi_api_queue_elem_t p_list_elem = NULL;
    p_fapi_msg_header_t p_fapi_msg_hdr = NULL;

    p_list_elem =
        nr5g_fapi_fapi2mac_create_api_list_elem(FAPI_VENDOR_MSG_HEADER_IND, 1,
        sizeof(fapi_msg_header_t));
    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[FAPI MSG HDR] Unable to create "
                "list element. Out of memory!!!"));
        return FAILURE;
    }

    p_fapi_msg_hdr = (fapi_msg_header_t *) (p_list_elem + 1);
    p_fapi_msg_hdr->num_msg = 0;
    p_fapi_msg_hdr->handle = phy_id;

    // Add element to send list
    nr5g_fapi_fapi2mac_add_api_to_list(phy_id, p_list_elem, is_urllc);
    NR5G_FAPI_LOG(DEBUG_LOG,
        ("[FAPI MSG HDR] FAPI Message Header Added for PHY: %d", phy_id));

    return SUCCESS;
}
