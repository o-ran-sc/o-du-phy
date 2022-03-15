/******************************************************************************
*
*   Copyright (c) 2021 Intel.
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

#include "fapi_vendor_extension.h"
#include "gnb_l1_l2_api.h"
#include "nr5g_fapi_common_types.h"
#include "nr5g_fapi_fapi2phy_api.h"
#include "nr5g_fapi_log.h"

/**
 * @file
 * This file consist of implementation of FAPI VENDOR ADD_REMOVE_CORE message.
 *
 **/

/** @ingroup group_source_api_p5_fapi2phy_proc
 *
 *  @param[in]  p_fapi_req Pointer to FAPI VENDOR ADD_REMOVE_CORE message structure.
 *  @return     Returns ::SUCCESS and ::FAILURE.
 *
 *  @description
 *  This is a timer mode specific message used to set options on bbupool cores.
 *
 */
#ifdef DEBUG_MODE
uint8_t nr5g_fapi_add_remove_core_message(
    bool is_urllc,
    fapi_vendor_ext_add_remove_core_msg_t * p_fapi_req)
{
    uint32_t i, k;
    PMAC2PHY_QUEUE_EL p_list_elem;
    PADD_REMOVE_BBU_CORES p_add_remove_bbu_cores;

    /* Below print is for better logging on console in debug mode. */
    NR5G_FAPI_LOG(INFO_LOG, (""));

    if (NULL == p_fapi_req) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[FAPI_VENDOR_EXT_ADD_REMOVE_CORE] Invalid fapi message"));
        return FAILURE;
    }

    p_list_elem = nr5g_fapi_fapi2phy_create_api_list_elem(
        (uint8_t)MSG_TYPE_PHY_ADD_REMOVE_CORE, 1, (uint32_t) sizeof(ADD_REMOVE_BBU_CORES));

    if (!p_list_elem) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[FAPI_VENDOR_EXT_ADD_REMOVE_CORE] Unable to create "
                "list element. Out of memory!!!"));
        return FAILURE;
    }

    p_add_remove_bbu_cores = (PADD_REMOVE_BBU_CORES) (p_list_elem + 1);
    p_add_remove_bbu_cores->sMsgHdr.nMessageType = MSG_TYPE_PHY_ADD_REMOVE_CORE;
    p_add_remove_bbu_cores->sMsgHdr.nMessageLen = sizeof(ADD_REMOVE_BBU_CORES);

    for (i = 0; i < FAPI_MAX_NUM_SET_CORE_MASK; ++i)
    {
        for (k = 0; k < FAPI_MAX_MASK_OPTIONS; ++k)
        {
            p_add_remove_bbu_cores->nCoreMask[k][i] = p_fapi_req->add_remove_core_info.nCoreMask[k][i];
        }
    }
    for (i = 0; i < FAPI_NUM_SPLIT_OPTIONS; ++i)
    {
        p_add_remove_bbu_cores->nMacOptions[i] = p_fapi_req->add_remove_core_info.nMacOptions[i];
    }
    p_add_remove_bbu_cores->eOption = (BBUPOOL_CORE_OPERATION)p_fapi_req->add_remove_core_info.eOption;

    nr5g_fapi_fapi2phy_add_to_api_list(is_urllc, p_list_elem);

    NR5G_FAPI_LOG(INFO_LOG, ("[FAPI_VENDOR_EXT_ADD_REMOVE_CORE.message]"));

    return SUCCESS;
}
#endif
