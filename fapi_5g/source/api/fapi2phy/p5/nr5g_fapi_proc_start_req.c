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
 * This file consist of implementation of FAPI START.request message.
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
 *  @param[in]  p_fapi_req Pointer to FAPI START.request message structure.
 *  @param[in]  p_fapi_vendor_msg Pointer to FAPI vendor message structure.
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This message instructs a configured PHY to start transmitting as an eNB. If
 *  the PHY is in the CONFIGURED state, it will issue SLOT indication.
 *  After the PHY has sent its first SLOT.indication message it enters the
 *  RUNNING state. If the PHY receives a START.request in either the IDLE or
 *  RUNNING state it will return an ERROR.indication including an INVALID_STATE
 *  error.
 *
 *  The *nSFN*, *nSlot*, and *nCarrierIdx* parameters are vendor specific 
 *  configuration and programmed through Vendor Specific Message structure 
 *  ::fapi_start_req_vendor_msg_t 
 *
 *  The *phyMode*, *ttiPeriod*, and *numSubframes* parameters of Vendor Specific 
 *  Message structure ::fapi_start_req_vendor_msg_t are applicable only for 
 *  TIMER mode (used for debugging only).
 *
**/
uint8_t nr5g_fapi_start_request(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_start_req_t * p_fapi_req,
    fapi_vendor_msg_t * p_fapi_vendor_msg)
{
    PSTARTREQUESTStruct p_start_req;
    PMAC2PHY_QUEUE_EL p_list_elem;
    nr5g_fapi_stats_t *p_stats;

    if (NULL == p_phy_instance) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[START.request] Invalid " "phy instance"));
        return FAILURE;
    }
    p_stats = &p_phy_instance->stats;
    p_stats->fapi_stats.fapi_start_req++;

    if (FAPI_STATE_CONFIGURED != p_phy_instance->state) {
        // TODO: Return Error indication to mac 
        //         // Please refer PHY State transition.
        NR5G_FAPI_LOG(ERROR_LOG, ("[START.request] Received in "
                "other than CONFIGURED State"));
        return FAILURE;
    }

    if (NULL == p_fapi_req) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[START.request] Invalid fapi " "message"));
        return FAILURE;
    }

    if (NULL == p_fapi_vendor_msg) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[START.request] Invalid fapi "
                "vendor message"));
        return FAILURE;
    }

    if (p_fapi_vendor_msg->start_req_vendor.mode > 4) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[START.request] PHY Mode %u invalid",
                p_fapi_vendor_msg->start_req_vendor.mode));
        return FAILURE;
    }

    p_list_elem = nr5g_fapi_fapi2phy_create_api_list_elem((uint8_t)
        MSG_TYPE_PHY_START_REQ, 1, (uint32_t) sizeof(STARTREQUESTStruct));
    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG, (" [START.request] Unable to create "
                "list element. Out of memory!!!"));
        return FAILURE;
    }

    p_start_req = (PSTARTREQUESTStruct) (p_list_elem + 1);
    p_start_req->sMsgHdr.nMessageType = MSG_TYPE_PHY_START_REQ;
    p_start_req->sMsgHdr.nMessageLen = (uint16_t) sizeof(STARTREQUESTStruct);
    p_start_req->sSFN_Slot.nSFN = p_fapi_vendor_msg->start_req_vendor.sfn;
    p_start_req->sSFN_Slot.nSlot = p_fapi_vendor_msg->start_req_vendor.slot;
    p_start_req->sSFN_Slot.nCarrierIdx = p_phy_instance->phy_id;
    p_start_req->nMode = p_fapi_vendor_msg->start_req_vendor.mode;
#ifdef DEBUG_MODE
    /* Setting period and count only for timer mode */
    if (1 == p_fapi_vendor_msg->start_req_vendor.mode) {
        p_start_req->nCount = p_fapi_vendor_msg->start_req_vendor.count;
        p_start_req->nPeriod = p_fapi_vendor_msg->start_req_vendor.period;
    }
#endif

    /* Add element to send list */
    nr5g_fapi_fapi2phy_add_to_api_list(is_urllc, p_list_elem);

    p_stats->iapi_stats.iapi_start_req++;
    NR5G_FAPI_LOG(INFO_LOG, ("[START.request][%d]", p_phy_instance->phy_id));

    return SUCCESS;
}
