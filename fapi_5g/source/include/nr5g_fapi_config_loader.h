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
 * @file This file consist of FAPI internal functions.
 *
 **/

#ifndef NR5G_FAPI_CONFIG_LOADER_H_
#define NR5G_FAPI_CONFIG_LOADER_H_

#include "nr5g_fapi_std.h"
#include "nr5g_fapi_common_types.h"
#include "nr5g_fapi_dpdk.h"
#include "nr5g_fapi_log.h"

#define NR5G_FAPI_DEVICE_NAME_LEN   512
#define NR5G_FAPI_MEMORY_ZONE_NAME_LEN  512

enum {
    DPDK_IOVA_PA_MODE = 0,
    DPDK_IOVA_VA_MODE,
    DPDK_IOVA_MAX_MODE
};

typedef struct _nr5g_fapi_thread_info {
    pthread_t thread_id;        /* ID returned by pthread_create() */
    pthread_attr_t thread_attr;
} nr5g_fapi_thread_info_t;

typedef struct _nr5g_fapi_config_worker_cfg {
    uint8_t core_id;
    uint8_t thread_priority;
    uint8_t thread_sched_policy;
} nr5g_fapi_config_worker_cfg_t;

typedef struct _nr5g_fapi_config_wls_cfg {
    char device_name[NR5G_FAPI_DEVICE_NAME_LEN];
    uint64_t shmem_size;
} nr5g_fapi_config_wls_cfg_t;

typedef struct nr5g_fapi_config_dpdk_cfg_t {
    uint8_t iova_mode;          /*0 - PA mode, 1 - VA mode */
    char memory_zone[NR5G_FAPI_MEMORY_ZONE_NAME_LEN];
} nr5g_fapi_config_dpdk_cft_t;

typedef struct _nr5g_fapi_config_log_cfg {
    nr5g_fapi_log_types_t level;
} nr5g_fapi_config_log_cfg_t;

typedef struct _nr5g_fapi_cfg {
    char *prgname;
    nr5g_fapi_config_worker_cfg_t mac2phy_worker;
    nr5g_fapi_config_worker_cfg_t phy2mac_worker;
    nr5g_fapi_config_worker_cfg_t urllc_worker;
    nr5g_fapi_config_wls_cfg_t wls;
    nr5g_fapi_config_log_cfg_t logger;
    nr5g_fapi_thread_info_t mac2phy_thread_info;
    nr5g_fapi_thread_info_t phy2mac_thread_info;
    nr5g_fapi_thread_info_t urllc_thread_info;
    nr5g_fapi_config_dpdk_cft_t dpdk;
} nr5g_fapi_cfg_t,
*p_nr5g_fapi_cfg_t;

p_nr5g_fapi_cfg_t nr5g_fapi_config_loader(
    char *prgname,
    const char *cfg_fname);

#endif                          // NR5G_FAPI_CONFIG_LOADER_H_
