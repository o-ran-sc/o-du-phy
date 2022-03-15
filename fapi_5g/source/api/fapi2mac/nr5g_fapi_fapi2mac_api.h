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
 * from FAPI to MAC
 *
 **/

#ifndef NR5G_FAPI_FAPI2MAC_API_H
#define NR5G_FAPI_FAPI2MAC_API_H

#include "fapi_vendor_extension.h"
#include "nr5g_fapi_std.h"

typedef struct _nr5g_fapi_fapi2mac_queue {
    volatile pthread_mutex_t lock;
    volatile p_fapi_api_queue_elem_t p_send_list_head;  // list head to, send to MAC
    volatile p_fapi_api_queue_elem_t p_send_list_tail;  // list tail to, send to MAC
} nr5g_fapi_fapi2mac_queue_t,
*p_nr5g_fapi_fapi2mac_queue_t;

// Function definitions
// -----------------------------------------------------------------------------
p_fapi_api_queue_elem_t nr5g_fapi_fapi2mac_create_api_list_elem(
    uint32_t msg_type,
    uint16_t num_message_in_block,
    uint32_t align_offset);

void nr5g_fapi_fapi2mac_send_api_list(
    bool is_urllc);

void nr5g_fapi_fapi2mac_add_api_to_list(
    uint8_t phy_id,
    p_fapi_api_queue_elem_t p_list_elem,
    bool is_urllc);

void nr5g_fapi_fapi2mac_init_api_list(
    );
#endif
