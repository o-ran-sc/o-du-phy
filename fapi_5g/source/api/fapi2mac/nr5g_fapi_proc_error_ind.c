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
 * This file consist of implementation of FAPI ERROR.indication message.
 *
 **/

#include "nr5g_fapi_framework.h"
#include "gnb_l1_l2_api.h"
#include "nr5g_fapi_fapi2mac_api.h"
#include "nr5g_fapi_proc_error_ind.h"

 /** @ingroup group_source_api_fapi2mac_proc
 *
 *  @param[in]  p_phy_ctx Pointer to PHY context.
 *  @param[in]  p_iapi_resp Pointer to IAPI ERROR.indication message structure.
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This message allows PHY to report errors to L2/L3.
 *
**/
uint8_t nr5g_fapi_error_indication(
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    PERRORIndicationStruct p_iapi_resp)
{
    uint8_t phy_id;
    fapi_error_ind_t *p_fapi_error_ind;
    p_fapi_api_queue_elem_t p_list_elem;
    p_nr5g_fapi_phy_instance_t p_phy_instance = NULL;

    if (NULL == p_phy_ctx) {
        NR5G_FAPI_LOG(ERROR_LOG,
            ("[NR5G_FAPI] [ERROR.indication] Invalid handle to "
                "phy context"));
        return FAILURE;
    }

    if (NULL == p_iapi_resp) {
        NR5G_FAPI_LOG(ERROR_LOG,
            ("[NR5G_FAPI] [ERROR.indication] Invalid handle to iapi "
                "stop indication message"));
        return FAILURE;
    }

    phy_id = p_iapi_resp->sSFN_Slot.nCarrierIdx;
    p_phy_instance = &p_phy_ctx->phy_instance[phy_id];
    if (p_phy_instance->phy_id != p_iapi_resp->sSFN_Slot.nCarrierIdx) {
        NR5G_FAPI_LOG(ERROR_LOG,
            ("[NR5G_FAPI] [ERROR.indication] Invalid handle"
                "to phy instance"));
        return FAILURE;
    }

    p_list_elem =
        nr5g_fapi_fapi2mac_create_api_list_elem(FAPI_ERROR_INDICATION, 1,
        sizeof(fapi_error_ind_t));
    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG,
            ("[NR5G_FAPI] [ERROR.indication] Unable to create "
                "list element. Out of memory!!!"));
        return FAILURE;
    }

    p_fapi_error_ind = (fapi_error_ind_t *) (p_list_elem + 1);
    p_fapi_error_ind->header.msg_id = FAPI_ERROR_INDICATION;
    p_fapi_error_ind->header.length = p_iapi_resp->sMsgHdr.nMessageLen;
    p_fapi_error_ind->sfn = p_iapi_resp->sSFN_Slot.nSFN;
    p_fapi_error_ind->slot = p_iapi_resp->sSFN_Slot.nSlot;
    // p_fapi_error_ind->message_id     =   ; // TODO message id is not supported in IAPI error indication
    p_fapi_error_ind->error_code = p_iapi_resp->nStatus;

    nr5g_fapi_fapi2mac_add_api_to_list(phy_id, p_list_elem, false);

    // phyStats->iaL1ApiStats.errorInd++; //TODO
    NR5G_FAPI_LOG(INFO_LOG, ("[NR5G_FAPI][ERROR.indication][%d][%d,%d]",
            p_phy_instance->phy_id,
            p_iapi_resp->sSFN_Slot.nSFN, p_iapi_resp->sSFN_Slot.nSlot));

    return SUCCESS;
}
