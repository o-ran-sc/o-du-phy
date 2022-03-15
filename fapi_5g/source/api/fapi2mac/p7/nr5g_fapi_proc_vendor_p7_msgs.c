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

#include "nr5g_fapi_framework.h"
#include "gnb_l1_l2_api.h"
#include "nr5g_fapi_fapi2mac_api.h"
#include "nr5g_fapi_fapi2mac_p7_proc.h"


static inline p_fapi_api_queue_elem_t alloc_vendor_p7_msg()
{
    p_fapi_api_queue_elem_t p_vend_elem = nr5g_fapi_fapi2mac_create_api_list_elem(
        FAPI_VENDOR_EXT_P7_IND,
        1, sizeof(fapi_vendor_p7_ind_msg_t));

    if (p_vend_elem)
    {
        fapi_vendor_p7_ind_msg_t* p_vend_p7_ind = (fapi_vendor_p7_ind_msg_t*) (p_vend_elem + 1);
        p_vend_p7_ind->header.msg_id = FAPI_VENDOR_EXT_P7_IND;
        p_vend_p7_ind->header.length = (uint16_t) sizeof(fapi_vendor_p7_ind_msg_t);
    }
    else
    {
        NR5G_FAPI_LOG(ERROR_LOG, ("[VENDOR EXT indication] Unable to create "
            "list element. Out of memory!!!"));
    }
    return p_vend_elem;
}


/** @ingroup group_source_api_p7_fapi2mac_proc
 *
 *  @return  Returns pointer to fapi_vendor_p7_ind_msg_t
 *
 *  @description
 *  Used to access fapi_vendor_p7_ind_msg_t for filling.
 *  Allocates the message if it's not allocated yet.
 *
**/
fapi_vendor_p7_ind_msg_t* nr5g_fapi_proc_vendor_p7_msg_get(
    p_fapi_api_stored_vendor_queue_elems vendor_extension_elems,
    uint8_t phy_id)
{
    if(phy_id >= FAPI_MAX_PHY_INSTANCES)
    {
        NR5G_FAPI_LOG(ERROR_LOG, ("[VENDOR EXT indication] Out of bounds"
            "phy_id=%u", phy_id));
        return NULL;
    }

    p_fapi_api_queue_elem_t p_vend_elem = vendor_extension_elems->vendor_ext[phy_id];
    if(!p_vend_elem)
    {
        NR5G_FAPI_LOG(DEBUG_LOG, ("[VENDOR EXT indication] No vendor element"
            "for phy_id=%u yet. Creating new", phy_id));
        p_vend_elem = alloc_vendor_p7_msg();
        vendor_extension_elems->vendor_ext[phy_id] = p_vend_elem;
    }

    return p_vend_elem ? (fapi_vendor_p7_ind_msg_t*) (p_vend_elem + 1) : NULL;
}

/** @ingroup group_source_api_p7_fapi2mac_proc
 *
 *  @return  none
 *
 *  @description
 *  Adds all cached vendor msgs to api list.
 *  Function shall be called after all other fapi msgs are added.
 *
**/
void nr5g_fapi_proc_vendor_p7_msgs_move_to_api_list(
    bool is_urllc,
    p_fapi_api_stored_vendor_queue_elems vendor_extension_elems)
{
    uint8_t phy_id;
    for(phy_id=0; phy_id<FAPI_MAX_PHY_INSTANCES; phy_id++)
    {
        p_fapi_api_queue_elem_t* p_vend_elem = &(vendor_extension_elems->vendor_ext[phy_id]);
        nr5g_fapi_fapi2mac_add_api_to_list(phy_id, *p_vend_elem, is_urllc);
        *p_vend_elem = NULL;
    }
}
