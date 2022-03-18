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
 * This file consist of implementation of FAPI STOP.request message.
 *
 **/

#include "nr5g_fapi_framework.h"
#include "gnb_l1_l2_api.h"
#include "nr5g_fapi_fapi2mac_api.h"
#include "nr5g_fapi_fapi2phy_api.h"
#include "nr5g_fapi_fapi2phy_p5_proc.h"
#include "nr5g_fapi_fapi2phy_p5_pvt_proc.h"

 /** @ingroup group_source_api_p5_fapi2phy_proc
 *
 *  @param[in]  p_phy_instance Pointer to PHY instance.
 *  @param[in]  p_fapi_req Pointer to FAPI STOP.request message structure.
 *  @param[in]  p_fapi_vendor_msg Pointer to FAPI vendor message structure.
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This message allows L2/L3 to move the PHY from RUNNING state to CONFIGURED
 *  state. This stops the PHY transmitting as an gNB.
 *
 *  The *nSFN*, *nSlot*, and *nCarrierIdx* parameters are vendor specific 
 *  configuration and programmed through Vendor Specific Message structure 
 *  ::fapi_start_req_vendor_msg_t 
 *
**/
uint8_t nr5g_fapi_stop_request(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_stop_req_t * p_fapi_req,
    fapi_vendor_msg_t * p_fapi_vendor_msg)
{
    PSTOPREQUESTStruct p_stop_req;
    PMAC2PHY_QUEUE_EL p_list_elem;
    nr5g_fapi_stats_t *p_stats;

    if (NULL == p_phy_instance) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[STOP.request] Invalid " "phy instance"));
        return FAILURE;
    }
    p_stats = &p_phy_instance->stats;
    p_stats->fapi_stats.fapi_stop_req++;

    if (NULL == p_fapi_req) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[STOP.request] Invalid fapi " "message"));
        return FAILURE;
    }

    if (NULL == p_fapi_vendor_msg) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[STOP.request] Invalid fapi "
                "vendor message"));
        return FAILURE;
    }

    p_list_elem = nr5g_fapi_fapi2phy_create_api_list_elem((uint8_t)
        MSG_TYPE_PHY_STOP_REQ, 1, (uint32_t) sizeof(STOPREQUESTStruct));
    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[STOP.request] Unable to create "
                "list element. Out of memory!!!"));
        return FAILURE;
    }

    p_stop_req = (PSTOPREQUESTStruct) (p_list_elem + 1);
    p_stop_req->sMsgHdr.nMessageType = MSG_TYPE_PHY_STOP_REQ;
    p_stop_req->sMsgHdr.nMessageLen = (uint16_t) sizeof(STOPREQUESTStruct);
    p_stop_req->sSFN_Slot.nSFN = p_fapi_vendor_msg->stop_req_vendor.sfn;
    p_stop_req->sSFN_Slot.nSlot = p_fapi_vendor_msg->stop_req_vendor.slot;
    p_stop_req->sSFN_Slot.nCarrierIdx = p_phy_instance->phy_id;

    /* Add element to send list */
    nr5g_fapi_fapi2phy_add_to_api_list(is_urllc, p_list_elem);

    p_stats->iapi_stats.iapi_stop_req++;
    NR5G_FAPI_LOG(INFO_LOG, ("[STOP.request][%d]", p_phy_instance->phy_id));

    return SUCCESS;
}
