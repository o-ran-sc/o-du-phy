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
#include "nr5g_fapi_std.h"
#include "nr5g_fapi_framework.h"
#include "nr5g_fapi_urllc_thread.h"
#include "nr5g_fapi_phy2mac_thread.h"
#include "nr5g_fapi_mac2phy_thread.h"
#include "nr5g_fapi_fapi2mac_api.h"
#include "nr5g_fapi_fapi2phy_api.h"

static nr5g_fapi_urllc_msg_dir_t urllc_msg_dir = NR5G_FAPI_URLLC_MSG_DIR_LAST;
static void *p_urllc_list_elem = NULL;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void nr5g_fapi_urllc_thread_callback(
    nr5g_fapi_urllc_msg_dir_t msg_dir,
    void *p_list_elem)
{
    p_nr5g_fapi_phy_ctx_t p_phy_ctx = nr5g_fapi_get_nr5g_fapi_phy_ctx();
    sem_wait(&p_phy_ctx->urllc_sem_done);
    pthread_mutex_lock(&lock);
    p_urllc_list_elem = p_list_elem;
    urllc_msg_dir = msg_dir;
    pthread_mutex_unlock(&lock);
    sem_post(&p_phy_ctx->urllc_sem_process);
}

void *nr5g_fapi_urllc_thread_func(
    void *config)
{
    cpu_set_t cpuset;
    pthread_t thread;
    p_nr5g_fapi_phy_ctx_t p_phy_ctx = (p_nr5g_fapi_phy_ctx_t) config;
    uint64_t start_tick;

    NR5G_FAPI_LOG(INFO_LOG, ("[URLLC] Thread %s launched LWP:%ld on "
            "Core: %d\n", __func__, pthread_self(),
            p_phy_ctx->urllc_worker_core_id));

    thread = p_phy_ctx->urllc_tid = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(p_phy_ctx->urllc_worker_core_id, &cpuset);
    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

    usleep(1000);
    while (!p_phy_ctx->process_exit) {
        sem_wait(&p_phy_ctx->urllc_sem_process);
        pthread_mutex_lock(&lock);
        if (p_urllc_list_elem)
        {
            switch (urllc_msg_dir) {
                case NR5G_FAPI_URLLC_MSG_DIR_MAC2PHY:
                    nr5g_fapi_mac2phy_api_recv_handler(true, config, (p_fapi_api_queue_elem_t) p_urllc_list_elem);
                    start_tick = __rdtsc();
                    NR5G_FAPI_LOG(TRACE_LOG, ("[MAC2PHY] Send to PHY urllc.."));
                    nr5g_fapi_fapi2phy_send_api_list(true);
                    tick_total_wls_send_per_tti_dl += __rdtsc() - start_tick;
                    break;
                case NR5G_FAPI_URLLC_MSG_DIR_PHY2MAC:
                    nr5g_fapi_phy2mac_api_recv_handler(true, config, (PMAC2PHY_QUEUE_EL) p_urllc_list_elem);
                    nr5g_fapi_fapi2mac_send_api_list(true);
                    break;
                default:
                    NR5G_FAPI_LOG(ERROR_LOG, ("[URLLC]: Invalid URLLC message direction.\n"));
                    break;
            }

            p_urllc_list_elem = NULL;
            urllc_msg_dir = NR5G_FAPI_URLLC_MSG_DIR_LAST;
        }
        pthread_mutex_unlock(&lock);
        sem_post(&p_phy_ctx->urllc_sem_done);
    }
 
    pthread_exit(NULL);
}
