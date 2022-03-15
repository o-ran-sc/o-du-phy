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
 * @file This file defines all the functions used to send APIs
 * from FAPI to PHY
 *
 **/

#ifndef NR5G_FAPI_FAPI2PHY_API_H
#define NR5G_FAPI_FAPI2PHY_API_H

#include "gnb_l1_l2_api.h"
#include <nr5g_fapi_std.h>

typedef struct _nr5g_fapi_fapi2phy_queue {
    PMAC2PHY_QUEUE_EL p_send_list_head; // list head to, send to PHY
    PMAC2PHY_QUEUE_EL p_send_list_tail; // list tail to, send to PHY
    PMAC2PHY_QUEUE_EL p_recv_list_head; // list head received from PHY
    PMAC2PHY_QUEUE_EL p_recv_list_tail; // list tail received from PHY
} nr5g_fapi_fapi2phy_queue_t,
*p_nr5g_fapi_fapi2phy_queue_t;

// Function definitions
// -----------------------------------------------------------------------------
PMAC2PHY_QUEUE_EL nr5g_fapi_fapi2phy_create_api_list_elem(
    uint32_t msg_type,
    uint16_t num_message_in_block,
    uint32_t align_offset);

void nr5g_fapi_fapi2phy_add_to_api_list(
    bool is_urllc,
    PMAC2PHY_QUEUE_EL p_list_elem);

void nr5g_fapi_fapi2phy_send_api_list(
    bool is_urllc);

void nr5g_fapi_fapi2phy_add_to_free_list(
    PMAC2PHY_QUEUE_EL p_list_elem);

void nr5g_fapi_fapi2phy_destroy_api_list_elem(
    PMAC2PHY_QUEUE_EL p_list_elem);
#endif
