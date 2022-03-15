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
#ifndef _NR5G_FAPI_URLLC_THREAD_H_
#define _NR5G_FAPI_URLLC_THREAD_H_

#include "gnb_l1_l2_api.h"

typedef enum nr5g_fapi_urllc_msg_dir_e {
    NR5G_FAPI_URLLC_MSG_DIR_MAC2PHY = 0,
    NR5G_FAPI_URLLC_MSG_DIR_PHY2MAC,
    NR5G_FAPI_URLLC_MSG_DIR_LAST
} nr5g_fapi_urllc_msg_dir_t;

void nr5g_fapi_urllc_thread_callback(
    nr5g_fapi_urllc_msg_dir_t msg_dir,
    void *p_list_elem);

#endif
