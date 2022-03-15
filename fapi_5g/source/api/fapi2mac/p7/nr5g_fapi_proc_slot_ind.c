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
 * This file consist of implementation of FAPI SLOT.indication message.
 *
 **/

#include "nr5g_fapi_framework.h"
#include "gnb_l1_l2_api.h"
#include "nr5g_fapi_fapi2mac_api.h"
#include "nr5g_fapi_fapi2mac_p7_proc.h"

 /** @ingroup group_source_api_p7_fapi2mac_proc
 *
 *  @param[in]  p_phy_ctx   Pointer to PHY context.
 *  @param[in]  p_fapi_resp Pointer to IAPI SLOT.indication message structure.
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This message allows PHY to send slot indication message periodically 
 *  to L2/L3 based on the highest numerology cconfigured in CONFIG.request
 *
**/
uint8_t nr5g_fapi_slot_indication(
    bool is_urllc,
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    p_fapi_api_stored_vendor_queue_elems vendor_extension_elems,
    PSlotIndicationStruct p_iapi_resp)
{
    uint8_t phy_id;

    fapi_slot_ind_t *p_fapi_slot_ind;
    p_fapi_api_queue_elem_t p_list_elem;
    p_nr5g_fapi_phy_instance_t p_phy_instance = NULL;
    nr5g_fapi_stats_t *p_stats;

    if (NULL == p_phy_ctx) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[SLOT.indication] Invalid handle to "
                "phy context"));
        return FAILURE;
    }

    if (NULL == p_iapi_resp) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[SLOT.indication] Invalid handle to iapi "
                "slot indication message"));
        return FAILURE;
    }

    for (phy_id = 0; phy_id < FAPI_MAX_PHY_INSTANCES; phy_id++) {
        if (FAPI_STATE_RUNNING == p_phy_ctx->phy_instance[phy_id].state) {
            p_phy_instance = &p_phy_ctx->phy_instance[phy_id];
            if ((p_phy_instance->phy_id != phy_id)) {
                NR5G_FAPI_LOG(ERROR_LOG,
                    ("[SLOT.indication] Invalid " "phy instance"));
                return FAILURE;
            }

            p_stats = &p_phy_instance->stats;
            p_stats->iapi_stats.iapi_slot_ind++;

            p_list_elem =
                nr5g_fapi_fapi2mac_create_api_list_elem(FAPI_SLOT_INDICATION, 1,
                sizeof(fapi_slot_ind_t));

            if (!p_list_elem) {
                NR5G_FAPI_LOG(ERROR_LOG, ("[SLOT.indication] Unable to create "
                        "list element. Out of memory!!!"));
                return FAILURE;
            }

            p_fapi_slot_ind = (fapi_slot_ind_t *) (p_list_elem + 1);
            p_fapi_slot_ind->header.msg_id = FAPI_SLOT_INDICATION;
            p_fapi_slot_ind->header.length = (uint16_t) sizeof(fapi_slot_ind_t);
            p_fapi_slot_ind->sfn = p_iapi_resp->sSFN_Slot.nSFN;
            p_fapi_slot_ind->slot = p_iapi_resp->sSFN_Slot.nSlot;

            fapi_vendor_p7_ind_msg_t* p_fapi_vend_p7 =
                nr5g_fapi_proc_vendor_p7_msg_get(vendor_extension_elems, phy_id);
            fapi_vendor_ext_slot_ind_t* p_fapi_vend_slot_ind = p_fapi_vend_p7 ? &p_fapi_vend_p7->slot_ind : NULL;
            
            if (p_fapi_vend_slot_ind) {
                p_fapi_vend_slot_ind->carrier_idx = p_iapi_resp->sSFN_Slot.nCarrierIdx;
                p_fapi_vend_slot_ind->sym = p_iapi_resp->sSFN_Slot.nSym;
            }

            /* Add element to send list */
            nr5g_fapi_fapi2mac_add_api_to_list(phy_id, p_list_elem, is_urllc);

            p_stats->fapi_stats.fapi_slot_ind++;
            NR5G_FAPI_LOG(DEBUG_LOG, ("[SLOT.indication][%u][%u,%u,%u] is_urllc %u",
                    p_phy_instance->phy_id,
                p_iapi_resp->sSFN_Slot.nSFN, p_iapi_resp->sSFN_Slot.nSlot,
                p_iapi_resp->sSFN_Slot.nSym, is_urllc));
        }
    }
    return SUCCESS;
}
