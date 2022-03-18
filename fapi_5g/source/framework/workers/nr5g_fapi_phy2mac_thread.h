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
#ifndef _NR5G_FAPI_PHY2MAC_THREAD_H_
#define _NR5G_FAPI_PHY2MAC_THREAD_H_

#include "gnb_l1_l2_api.h"

void nr5g_fapi_phy2mac_api_recv_handler(
    bool is_urllc,
    void *config,
    PMAC2PHY_QUEUE_EL p_msg_list);

#endif
