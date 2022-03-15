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
#ifndef _NR5G_FAPI_MAC2PHY_THREAD_H_
#define _NR5G_FAPI_MAC2PHY_THREAD_H_

#include "nr5g_fapi_framework.h"
#include "fapi_interface.h"
#include "fapi_vendor_extension.h"

void nr5g_fapi_mac2phy_api_recv_handler(
    bool is_urllc,
    void *config,
    p_fapi_api_queue_elem_t p_msg_list);

void nr5g_fapi_mac2phy_api_processing_handler(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    p_fapi_api_queue_elem_t p_msg_list);

uint8_t nr5g_fapi_check_api_ordering(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    p_fapi_api_queue_elem_t p_msg_list);
#endif
