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

#ifndef _NR5G_FAPI2PHY_WLS_H_
#define _NR5G_FAPI2PHY_WLS_H_

#include "gnb_l1_l2_api.h"

uint8_t nr5g_fapi_fapi2phy_is_valid_wls_ptr(
    void *data);
uint8_t nr5g_fapi_fapi2phy_wls_send(
    void *data,
    bool is_urllc);
PMAC2PHY_QUEUE_EL nr5g_fapi_fapi2phy_wls_recv(
    );
inline uint32_t nr5g_fapi_fapi2phy_wls_wait(
    );
void wls_fapi_add_send_apis_to_free(
    PMAC2PHY_QUEUE_EL pListElem,
    uint32_t idx);
void wls_fapi_free_send_free_list(
    uint32_t idx);
void wls_fapi_add_send_apis_to_free_urllc(
    PMAC2PHY_QUEUE_EL pListElem,
    uint32_t idx);
void wls_fapi_free_send_free_list_urllc(
    uint32_t idx);
void wls_fapi_add_recv_apis_to_free(
    PMAC2PHY_QUEUE_EL pListElem,
    uint32_t idx);
void wls_fapi_free_recv_free_list(
    uint32_t idx);

#endif /*_NR5G_FAPI2PHY_WLS_H_*/
