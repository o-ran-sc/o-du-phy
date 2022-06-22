/******************************************************************************
*
*   Copyright (c) 2022 Intel.
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
#include "nr5g_fapi_std.h"
#include "nr5g_fapi_framework.h"
#include "nr5g_fapi_phy2mac_thread.h"
#include "nr5g_fapi_mac2phy_thread.h"
#include "nr5g_fapi_fapi2mac_api.h"
#include "nr5g_fapi_fapi2phy_api.h"

void *nr5g_fapi_urllc_phy2mac_thread_func(
    void *config)
{
    p_nr5g_fapi_phy_ctx_t p_phy_ctx = (p_nr5g_fapi_phy_ctx_t) config;

    NR5G_FAPI_LOG(INFO_LOG, ("[URLLC_PHY2MAC] Thread %s launched LWP:%ld on "
            "Core: %d\n", __func__, pthread_self(),
            p_phy_ctx->urllc_phy2mac_worker_core_id));

    nr5g_fapi_init_thread(p_phy_ctx->urllc_phy2mac_worker_core_id);

    while (!p_phy_ctx->process_exit) {
        sem_wait(&p_phy_ctx->urllc_phy2mac_params.urllc_sem_process);
        pthread_mutex_lock(&p_phy_ctx->urllc_phy2mac_params.lock);
        if (p_phy_ctx->urllc_phy2mac_params.p_urllc_list_elem)
        {
            nr5g_fapi_phy2mac_api_recv_handler(true, config, 
                (PMAC2PHY_QUEUE_EL) p_phy_ctx->urllc_phy2mac_params.
                p_urllc_list_elem);
            nr5g_fapi_fapi2mac_send_api_list(true);

            p_phy_ctx->urllc_phy2mac_params.p_urllc_list_elem = NULL;
        }
        pthread_mutex_unlock(&p_phy_ctx->urllc_phy2mac_params.lock);
        sem_post(&p_phy_ctx->urllc_phy2mac_params.urllc_sem_done);
    }
 
    pthread_exit(NULL);
}