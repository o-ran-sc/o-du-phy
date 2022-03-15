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
#include "nr5g_fapi_framework.h"
#include "nr5g_fapi_wls.h"
#include "nr5g_fapi_fapi2mac_wls.h"
#include "nr5g_fapi_fapi2phy_wls.h"
#include "nr5g_fapi_fapi2phy_api.h"
#include "gnb_l1_l2_api.h"
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
    pthread_join(p_cfg->mac2phy_thread_info.thread_id, NULL);
    pthread_join(p_cfg->phy2mac_thread_info.thread_id, NULL);
    pthread_join(p_cfg->urllc_thread_info.thread_id, NULL);
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

uint8_t nr5g_fapi_framework_init(
    p_nr5g_fapi_cfg_t p_cfg)
{
    p_nr5g_fapi_phy_ctx_t p_phy_ctx = nr5g_fapi_get_nr5g_fapi_phy_ctx();
    pthread_attr_t *p_mac2phy_attr, *p_phy2mac_attr, *p_urllc_attr;
    struct sched_param param;

    nr5g_fapi_set_log_level(p_cfg->logger.level);
    // Set up WLS
    if (FAILURE == nr5g_fapi_wls_init(p_cfg)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[FAPI_INT] WLS init Failed"));
        return FAILURE;
    }
    NR5G_FAPI_LOG(INFO_LOG, ("[FAPI_INT] WLS init Successful"));

    p_phy_ctx->phy2mac_worker_core_id = p_cfg->phy2mac_worker.core_id;
    p_phy_ctx->mac2phy_worker_core_id = p_cfg->mac2phy_worker.core_id;
    p_phy_ctx->urllc_worker_core_id = p_cfg->urllc_worker.core_id;

    memset(&p_phy_ctx->urllc_sem_process, 0, sizeof(sem_t));
    memset(&p_phy_ctx->urllc_sem_done, 0, sizeof(sem_t));
    if (0 != sem_init(&p_phy_ctx->urllc_sem_process, 0, 0)) {
        printf("Error: Unable to init urllc semaphore\n");
        return FAILURE;
    }
    if (0 != sem_init(&p_phy_ctx->urllc_sem_done, 0, 1)) {
        printf("Error: Unable to init urllc_sem_done semaphore\n");
        return FAILURE;
    }

    p_phy2mac_attr = &p_cfg->phy2mac_thread_info.thread_attr;
    pthread_attr_init(p_phy2mac_attr);
    if (!pthread_attr_getschedparam(p_phy2mac_attr, &param)) {
        param.sched_priority = p_cfg->phy2mac_worker.thread_priority;
        pthread_attr_setschedparam(p_phy2mac_attr, &param);
        pthread_attr_setschedpolicy(p_phy2mac_attr, SCHED_FIFO);
    }

    if (0 != pthread_create(&p_cfg->phy2mac_thread_info.thread_id,
            p_phy2mac_attr, nr5g_fapi_phy2mac_thread_func, (void *)
            p_phy_ctx)) {
        printf("Error: Unable to create threads\n");
        if (p_phy2mac_attr)
            pthread_attr_destroy(p_phy2mac_attr);
        return FAILURE;
    }
    pthread_setname_np(p_cfg->phy2mac_thread_info.thread_id,
        "nr5g_fapi_phy2mac_thread");

    p_mac2phy_attr = &p_cfg->mac2phy_thread_info.thread_attr;
    pthread_attr_init(p_mac2phy_attr);
    if (!pthread_attr_getschedparam(p_mac2phy_attr, &param)) {
        param.sched_priority = p_cfg->mac2phy_worker.thread_priority;
        pthread_attr_setschedparam(p_mac2phy_attr, &param);
        pthread_attr_setschedpolicy(p_mac2phy_attr, SCHED_FIFO);
    }

    if (0 != pthread_create(&p_cfg->mac2phy_thread_info.thread_id,
            p_mac2phy_attr, nr5g_fapi_mac2phy_thread_func, (void *)
            p_phy_ctx)) {
        printf("Error: Unable to create threads\n");
        if (p_mac2phy_attr)
            pthread_attr_destroy(p_mac2phy_attr);
        return FAILURE;
    }
    pthread_setname_np(p_cfg->mac2phy_thread_info.thread_id,
        "nr5g_fapi_mac2phy_thread");

    p_urllc_attr = &p_cfg->urllc_thread_info.thread_attr;
    pthread_attr_init(p_urllc_attr);
    if (!pthread_attr_getschedparam(p_urllc_attr, &param)) {
        param.sched_priority = p_cfg->urllc_worker.thread_sched_policy;
        pthread_attr_setschedparam(p_urllc_attr, &param);
        pthread_attr_setschedpolicy(p_urllc_attr, SCHED_FIFO);
    }

    if (0 != pthread_create(&p_cfg->urllc_thread_info.thread_id,
            p_urllc_attr, nr5g_fapi_urllc_thread_func, (void *)
            p_phy_ctx)) {
        printf("Error: Unable to create threads\n");
        if (p_urllc_attr)
            pthread_attr_destroy(p_urllc_attr);
        sem_destroy(&p_phy_ctx->urllc_sem_process);
        sem_destroy(&p_phy_ctx->urllc_sem_done);
        return FAILURE;
    }
    pthread_setname_np(p_cfg->urllc_thread_info.thread_id,
        "nr5g_fapi_urllc_thread");

    return SUCCESS;
}
