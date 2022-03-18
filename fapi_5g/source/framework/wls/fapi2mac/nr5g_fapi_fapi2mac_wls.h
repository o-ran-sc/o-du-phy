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
 *
 * @file This file has definitions of Shared Memory interface functions between
 * FAPI and PHY.
 *
 **/

#ifndef _5G_FAPI_FAPI2MAC_WLS_H
#define _5G_FAPI_FAPI2MAC_WLS_H

#include "fapi_interface.h"
#include "fapi_vendor_extension.h"

uint8_t nr5g_fapi_fapi2mac_is_valid_wls_ptr(
    void *data);

uint8_t nr5g_fapi_fapi2mac_wls_send(
    p_fapi_api_queue_elem_t p_list_elem,
    bool is_urllc);

p_fapi_api_queue_elem_t nr5g_fapi_fapi2mac_wls_recv(
    );

uint8_t nr5g_fapi_fapi2mac_wls_ready(
    );
uint32_t nr5g_fapi_fapi2mac_wls_wait(
    );

void *nr5g_fapi_fapi2mac_wls_alloc_buffer(
    );

void nr5g_fapi_fapi2mac_wls_free_buffer(
    void *buffers);

#endif /*_5G_FAPI_FAPI2MAC_WLS_H*/
