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
 * This file consist of implementation of FAPI Start.request message.
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
 *  @param[in]  p_fapi_req Pointer to FAPI SHUTDOWN.request message structure.
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This vendor specific message  instructs a configured PHY to Shutdown its processing 
 *  by cleaning up the resources alloted when PHY is up and running.
 *
**/
uint8_t nr5g_fapi_shutdown_request(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_vendor_ext_shutdown_req_t * p_fapi_req)
{
    PSHUTDOWNREQUESTStruct p_shutdown_req;
    PMAC2PHY_QUEUE_EL p_list_elem;
    nr5g_fapi_stats_t *p_stats;

    if (NULL == p_phy_instance) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[SHUTDOWN.request] Invalid "
                "phy instance"));
        return FAILURE;
    }
    p_stats = &p_phy_instance->stats;
    p_stats->fapi_stats.fapi_vext_shutdown_req++;

    if (NULL == p_fapi_req) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[SHUTDOWN.request] Invalid handle to fapi "
                "message"));
        return FAILURE;
    }

    p_list_elem = nr5g_fapi_fapi2phy_create_api_list_elem((uint8_t)
        MSG_TYPE_PHY_SHUTDOWN_REQ, 1, (uint32_t) sizeof(SHUTDOWNREQUESTStruct));
    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[SHUTDOWN.request] Unable to create "
                "list element. Out of memory!!!"));
        return FAILURE;
    }
    p_shutdown_req = (PSHUTDOWNREQUESTStruct) (p_list_elem + 1);
    p_shutdown_req->sMsgHdr.nMessageType = MSG_TYPE_PHY_SHUTDOWN_REQ;
    p_shutdown_req->sMsgHdr.nMessageLen =
        (uint16_t) sizeof(SHUTDOWNREQUESTStruct);
    p_shutdown_req->sSFN_Slot.nSFN = p_fapi_req->sfn;
    p_shutdown_req->sSFN_Slot.nSlot = p_fapi_req->slot;
    p_shutdown_req->sSFN_Slot.nCarrierIdx = p_phy_instance->phy_id;
    p_phy_instance->shutdown_test_type = p_shutdown_req->nTestType =
        p_fapi_req->test_type;

    /* Add element to send list */
    nr5g_fapi_fapi2phy_add_to_api_list(is_urllc, p_list_elem);

    p_stats->iapi_stats.iapi_shutdown_req++;
    NR5G_FAPI_LOG(INFO_LOG, ("[SHUTDOWN.request][%d]", p_phy_instance->phy_id));

    return SUCCESS;
}
