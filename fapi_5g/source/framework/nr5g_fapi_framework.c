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
#include "nr5g_fapi_wls.h"
#include "nr5g_fapi_fapi2mac_wls.h"
#include "nr5g_fapi_fapi2phy_wls.h"
#include "nr5g_fapi_fapi2phy_api.h"
#include "rte_memzone.h"
#include "nr5g_fapi_memory.h"

static nr5g_fapi_phy_ctx_t nr5g_fapi_phy_ctx;

inline p_nr5g_fapi_phy_ctx_t nr5g_fapi_get_nr5g_fapi_phy_ctx(
    )
{
    return &nr5g_fapi_phy_ctx;
}

uint8_t nr5g_fapi_dpdk_init(
    p_nr5g_fapi_cfg_t p_cfg)
{
    printf("init dev name: %s\n", p_cfg->dpdk.memory_zone);
    char *const file_prefix = basename(p_cfg->dpdk.memory_zone);
    printf("init basename: %s\n", file_prefix);
    char whitelist[32], iova_mode[64];
    uint8_t i;

    char *argv[] = { p_cfg->prgname, "--proc-type=secondary",
        "--file-prefix", file_prefix, whitelist, iova_mode
    };
#if 0
    char coremask[32];
    char *argv[] = { p_cfg->prgname, coremask, "--proc-type=secondary",
        "--file-prefix", file_prefix, whitelist
    };
    snprintf(coremask, 32, "-l %d,%d", p_cfg->mac2phy_worker.core_id,
        p_cfg->phy2mac_worker.core_id);
#endif
    int argc = RTE_DIM(argv);

    /* initialize EAL first */
    snprintf(whitelist, 32, "-a%s", "0000:00:06.0");

    if (p_cfg->dpdk.iova_mode == 0)
        snprintf(iova_mode, 64, "%s", "--iova-mode=pa");
    else
        snprintf(iova_mode, 64, "%s", "--iova-mode=va");

    printf("Calling rte_eal_init: ");
    for (i = 0; i < RTE_DIM(argv); i++) {
        printf("%s ", argv[i]);
    }
    printf("\n");

    if (rte_eal_init(argc, argv) < 0)
        rte_panic("Cannot init EAL\n");

    return SUCCESS;
}

uint8_t nr5g_fapi_dpdk_wait(
    p_nr5g_fapi_cfg_t p_cfg)
{
#if 0
    uint32_t worker_core;
    /* wait per-lcore */
    RTE_LCORE_FOREACH_SLAVE(worker_core) {
        if (rte_eal_wait_lcore(worker_core) < 0) {
            return FAILURE;
        }
    }
#else
    pthread_join(p_cfg->mac2phy_thread_params.thread_info.thread_id, NULL);
    pthread_join(p_cfg->phy2mac_thread_params.thread_info.thread_id, NULL);
    pthread_join(p_cfg->urllc_phy2mac_thread_params.thread_info.thread_id, NULL);
    pthread_join(p_cfg->urllc_mac2phy_thread_params.thread_info.thread_id, NULL);
#endif
    return SUCCESS;
}

void nr5g_fapi_set_ul_slot_info(
    uint16_t frame_no,
    uint16_t slot_no,
    uint8_t symbol_no,
    nr5g_fapi_ul_slot_info_t * p_ul_slot_info)
{
    NR5G_FAPI_MEMSET(p_ul_slot_info, sizeof(nr5g_fapi_ul_slot_info_t), 0, sizeof(nr5g_fapi_ul_slot_info_t));

    p_ul_slot_info->cookie = frame_no;
    p_ul_slot_info->slot_no = slot_no;
    p_ul_slot_info->symbol_no = symbol_no;
}

nr5g_fapi_ul_slot_info_t *nr5g_fapi_get_ul_slot_info(
    bool is_urllc,
    uint16_t frame_no,
    uint16_t slot_no,
    uint8_t symbol_no,
    p_nr5g_fapi_phy_instance_t p_phy_instance)
{
    uint8_t i, j;
    nr5g_fapi_ul_slot_info_t *p_ul_slot_info;

    for (i = 0; i < MAX_UL_SLOT_INFO_COUNT; i++) {
        for(j = 0; j < MAX_UL_SYMBOL_INFO_COUNT; j++) {
            p_ul_slot_info = &p_phy_instance->ul_slot_info[is_urllc][i][j];
        if ((slot_no == p_ul_slot_info->slot_no) &&
                (frame_no == p_ul_slot_info->cookie) &&
                (symbol_no == p_ul_slot_info->symbol_no)) {
            return p_ul_slot_info;
        }
    }
    }
    return NULL;
}

uint8_t nr5g_fapi_prepare_thread(
    nr5g_fapi_thread_params_t* thread_params,
    char* thread_name,
    void* thread_fun(void*))
{
    struct sched_param param;
    pthread_attr_t* p_thread_attr = &thread_params->thread_info.thread_attr;
    pthread_attr_init(p_thread_attr);
    if (!pthread_attr_getschedparam(p_thread_attr, &param)) {
        param.sched_priority = thread_params->thread_worker.thread_priority;
        pthread_attr_setschedparam(p_thread_attr, &param);
        pthread_attr_setschedpolicy(p_thread_attr, SCHED_FIFO);
    }

    if (0 != pthread_create(&thread_params->thread_info.thread_id,
            p_thread_attr, thread_fun, (void *)
            nr5g_fapi_get_nr5g_fapi_phy_ctx())) {
        printf("Error: Unable to create threads\n");
        if (p_thread_attr)
            pthread_attr_destroy(p_thread_attr);
        return FAILURE;
    }
    pthread_setname_np(thread_params->thread_info.thread_id,
                       thread_name);

    return SUCCESS;
}

uint8_t nr5g_fapi_initialise_sempahore(
    nr5g_fapi_urllc_thread_params_t* urllc_thread_params)
{
    memset(&urllc_thread_params->urllc_sem_process, 0, sizeof(sem_t));
    memset(&urllc_thread_params->urllc_sem_done, 0, sizeof(sem_t));
     
    pthread_mutex_init(&urllc_thread_params->lock, NULL);
    urllc_thread_params->p_urllc_list_elem = NULL;
    if (0 != sem_init(&urllc_thread_params->urllc_sem_process, 0, 0)) {
        printf("Error: Unable to init urllc_sem_process semaphore\n");
        return FAILURE;
    }
    if (0 != sem_init(&urllc_thread_params->urllc_sem_done, 0, 1)) {
        printf("Error: Unable to init urllc_sem_done semaphore\n");
        return FAILURE;
    }

    return SUCCESS;
}

void nr5g_fapi_init_thread(uint8_t worker_core_id)
{
    cpu_set_t cpuset;
    pthread_t thread = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(worker_core_id, &cpuset);
    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

    usleep(1000);  
}

void nr5g_fapi_urllc_thread_callback(
    void *p_list_elem,
    nr5g_fapi_urllc_thread_params_t* urllc_params)
{
    if (nr5g_fapi_get_nr5g_fapi_phy_ctx()->is_urllc_enabled){
        sem_wait(&urllc_params->urllc_sem_done);
        pthread_mutex_lock(&urllc_params->lock);
        urllc_params->p_urllc_list_elem = p_list_elem;
        pthread_mutex_unlock(&urllc_params->lock);
        sem_post(&urllc_params->urllc_sem_process);
    }
    else {
        NR5G_FAPI_LOG(ERROR_LOG, ("[URLLC] Threads are not running"));
    }
}

uint8_t nr5g_fapi_framework_init(
    p_nr5g_fapi_cfg_t p_cfg)
{
    p_nr5g_fapi_phy_ctx_t p_phy_ctx = nr5g_fapi_get_nr5g_fapi_phy_ctx();

    nr5g_fapi_set_log_level(p_cfg->logger.level);
    // Set up WLS
    if (FAILURE == nr5g_fapi_wls_init(p_cfg)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[FAPI_INT] WLS init Failed"));
        return FAILURE;
    }
    NR5G_FAPI_LOG(INFO_LOG, ("[FAPI_INT] WLS init Successful"));

    p_phy_ctx->phy2mac_worker_core_id = p_cfg->phy2mac_thread_params.thread_worker.core_id;
    p_phy_ctx->mac2phy_worker_core_id = p_cfg->mac2phy_thread_params.thread_worker.core_id;
    p_phy_ctx->urllc_phy2mac_worker_core_id = p_cfg->urllc_phy2mac_thread_params.thread_worker.core_id;
    p_phy_ctx->urllc_mac2phy_worker_core_id = p_cfg->urllc_mac2phy_thread_params.thread_worker.core_id;
    p_phy_ctx->is_urllc_enabled = p_cfg->is_urllc_enabled;

    if (nr5g_fapi_prepare_thread(&p_cfg->phy2mac_thread_params,
                                 "nr5g_fapi_phy2mac_thread",
                                 nr5g_fapi_phy2mac_thread_func) == FAILURE) {
        return FAILURE;
    }


    if (nr5g_fapi_prepare_thread(&p_cfg->mac2phy_thread_params,
                                 "nr5g_fapi_mac2phy_thread",
                                 nr5g_fapi_mac2phy_thread_func) == FAILURE) {
        return FAILURE;
    }

    if (p_cfg->is_urllc_enabled)
    {
        if (nr5g_fapi_initialise_sempahore(&p_phy_ctx->urllc_phy2mac_params) == FAILURE) {
            return FAILURE;
    }

        if (nr5g_fapi_initialise_sempahore(&p_phy_ctx->urllc_mac2phy_params) == FAILURE) {
        return FAILURE;
    }

        if (nr5g_fapi_prepare_thread(&p_cfg->urllc_mac2phy_thread_params,
                                     "nr5g_fapi_urllc_mac2phy_thread",
                                     nr5g_fapi_urllc_mac2phy_thread_func) == FAILURE) {
            return FAILURE;
    }


        if (nr5g_fapi_prepare_thread(&p_cfg->urllc_phy2mac_thread_params,
                                     "nr5g_fapi_urllc_phy2mac_thread",
                                     nr5g_fapi_urllc_phy2mac_thread_func) == FAILURE) {
        return FAILURE;
    }

    }
    return SUCCESS;
}

void nr5g_fapi_clean(
    p_nr5g_fapi_phy_instance_t p_phy_instance)
{
    p_phy_instance->phy_config.n_nr_of_rx_ant = 0;
    p_phy_instance->phy_config.phy_cell_id = 0;
    p_phy_instance->phy_config.sub_c_common = 0;
    p_phy_instance->phy_config.use_vendor_EpreXSSB = 0;
    p_phy_instance->shutdown_test_type = 0; 
    p_phy_instance->phy_id = 0;
    p_phy_instance->state = FAPI_STATE_IDLE ;
    
    memset(p_phy_instance->ul_slot_info, 0, sizeof(nr5g_fapi_ul_slot_info_t));
    wls_fapi_free_send_free_list_urllc();
}