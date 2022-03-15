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
 * This file consist of implementation of FAPI SHUTDOWN.response message.
 *
 **/
#include "nr5g_fapi_framework.h"
#include "gnb_l1_l2_api.h"
#include "nr5g_fapi_fapi2mac_api.h"
#include "nr5g_fapi_fapi2phy_api.h"
#include "nr5g_fapi_fapi2mac_p5_proc.h"
#include "nr5g_fapi_stats.h"
#include "nr5g_fapi_fapi2phy_p5_proc.h"
#include "nr5g_fapi_fapi2phy_p5_pvt_proc.h"
#include "nr5g_fapi_memory.h"

 /** @ingroup group_source_api_p5_fapi2phy_proc
 *
 *  @param[in]  p_phy_ctx Pointer to PHY context.
 *  @param[in]  p_fapi_resp Pointer to IAPI SHUTDOWN.response message structure.
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This message allows PHY to indicate whether it has successfully cleaned up 
 *  the PHY resources alloted or not.
 *
**/
uint8_t nr5g_fapi_shutdown_response(
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    PSHUTDOWNRESPONSEStruct p_iapi_resp)
{
    uint8_t phy_id;
#ifdef DEBUG_MODE
    fapi_vendor_ext_shutdown_res_t *p_fapi_resp;
#else
    fapi_stop_ind_t *p_stop_ind;
#endif
    fapi_error_ind_t *p_fapi_error_ind;
    p_fapi_api_queue_elem_t p_list_elem;
    p_nr5g_fapi_phy_instance_t p_phy_instance = NULL;
    nr5g_fapi_stats_t *p_stats;

    if (NULL == p_phy_ctx) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[SHUTDOWN.response] Invalid "
                "phy instance"));
        return FAILURE;
    }

    if (NULL == p_iapi_resp) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[SHUTDOWN.response] Invalid iapi "
                "shutdown response message"));
        return FAILURE;
    }

    phy_id = p_iapi_resp->sSFN_Slot.nCarrierIdx;
    p_phy_instance = &p_phy_ctx->phy_instance[phy_id];
    if (p_phy_instance->phy_id != phy_id) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[SHUTDOWN.response] Invalid "
                "phy instance"));
        return FAILURE;
    }
    p_stats = &p_phy_instance->stats;
    p_stats->iapi_stats.iapi_shutdown_res++;

    if (p_iapi_resp->nStatus == SUCCESS) {
#ifdef DEBUG_MODE
        p_list_elem =
            nr5g_fapi_fapi2mac_create_api_list_elem
            (FAPI_VENDOR_EXT_SHUTDOWN_RESPONSE, 1,
            sizeof(fapi_vendor_ext_shutdown_res_t));
        if (!p_list_elem) {
            NR5G_FAPI_LOG(ERROR_LOG, ("[SHUTDOWN.response] Unable to create "
                    "list element. Out of memory!!!"));
            return FAILURE;
        }

        p_fapi_resp = (fapi_vendor_ext_shutdown_res_t *) (p_list_elem + 1);
        p_fapi_resp->header.msg_id = FAPI_VENDOR_EXT_SHUTDOWN_RESPONSE;
        p_fapi_resp->header.length =
            (uint16_t) sizeof(fapi_vendor_ext_shutdown_res_t);
        p_fapi_resp->sfn = p_iapi_resp->sSFN_Slot.nSFN;
        p_fapi_resp->slot = p_iapi_resp->sSFN_Slot.nSlot;
        p_fapi_resp->nStatus = p_iapi_resp->nStatus;

        /* Add element to send list */
        nr5g_fapi_fapi2mac_add_api_to_list(phy_id, p_list_elem, false);

        p_stats->fapi_stats.fapi_vext_shutdown_res++;
        NR5G_FAPI_LOG(INFO_LOG, ("[SHUTDOWN.response][%d]", phy_id));
        p_phy_instance->shutdown_retries = 0;
#else
        p_list_elem =
            nr5g_fapi_fapi2mac_create_api_list_elem(FAPI_STOP_INDICATION, 1,
            sizeof(fapi_stop_ind_t));
        if (!p_list_elem) {
            NR5G_FAPI_LOG(ERROR_LOG,
                ("[SHUTDOWN.response][STOP.Indication] Unable to create "
                    "list element. Out of memory!!!"));
            return FAILURE;
        }
        p_stop_ind = (fapi_stop_ind_t *) (p_list_elem + 1);
        p_stop_ind->header.msg_id = FAPI_STOP_INDICATION;
        p_stop_ind->header.length = sizeof(fapi_stop_ind_t);

        /* Add element to send list */
        nr5g_fapi_fapi2mac_add_api_to_list(phy_id, p_list_elem, false);

        p_stats->fapi_stats.fapi_stop_ind++;
        NR5G_FAPI_LOG(INFO_LOG, ("[STOP.Indication][%d]", phy_id));
        p_phy_instance->shutdown_retries = 0;
#endif
    } else {
        /* PHY SHUTDOWN  Failed. Retrigger Shutdown request for 3 tries before
         * triggering error indication towards  L2 */
        p_phy_instance->shutdown_retries++;
        if (p_phy_instance->shutdown_retries <= 3) {
            fapi_vendor_ext_shutdown_req_t fapi_req;
            fapi_req.header.msg_id = FAPI_VENDOR_EXT_SHUTDOWN_REQUEST;
            fapi_req.header.length = sizeof(fapi_vendor_ext_shutdown_req_t);
            fapi_req.sfn = 0;
            fapi_req.slot = 0;
            fapi_req.test_type = p_phy_instance->shutdown_test_type;
            nr5g_fapi_shutdown_request(0, p_phy_instance, &fapi_req);
            nr5g_fapi_fapi2phy_send_api_list(0);
        } else {
            NR5G_FAPI_LOG(ERROR_LOG, ("[SHUTDOWN.response] Invalid status "
                    "from PHY, hence triggering Error Indication"));
            p_list_elem =
                nr5g_fapi_fapi2mac_create_api_list_elem(FAPI_ERROR_INDICATION,
                1, sizeof(fapi_error_ind_t));

            if (!p_list_elem) {
                NR5G_FAPI_LOG(ERROR_LOG,
                    ("[SHUTDOWN.response][Error.Indication] Unable to create "
                        "list element. Out of memory!!!"));
                return FAILURE;
            }
            p_fapi_error_ind = (fapi_error_ind_t *) (p_list_elem + 1);
            p_fapi_error_ind->header.msg_id = FAPI_ERROR_INDICATION;
            p_fapi_error_ind->header.length =
                (uint16_t) sizeof(fapi_error_ind_t);
            p_fapi_error_ind->sfn = p_iapi_resp->sSFN_Slot.nSFN;
            p_fapi_error_ind->slot = p_iapi_resp->sSFN_Slot.nSlot;
            p_fapi_error_ind->message_id = FAPI_VENDOR_EXT_SHUTDOWN_REQUEST;
            p_fapi_error_ind->error_code = p_iapi_resp->nStatus;

            /* Add element to send list */
            nr5g_fapi_fapi2mac_add_api_to_list(phy_id, p_list_elem, false);
            p_stats->fapi_stats.fapi_error_ind++;
            p_phy_instance->shutdown_retries = 0;
            NR5G_FAPI_LOG(INFO_LOG, ("[Error.Indication][%d]", phy_id));
        }
    }

    nr5g_fapi_print_phy_instance_stats(p_phy_instance);
    NR5G_FAPI_MEMSET(&p_phy_instance->stats, sizeof(nr5g_fapi_stats_t), 0,
        sizeof(nr5g_fapi_stats_t));
    return SUCCESS;
}
