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
#include <pthread.h>

#include "nr5g_fapi_std.h"
#include "nr5g_fapi_common_types.h"
#include "nr5g_fapi_args.h"
#include "nr5g_fapi_config_loader.h"
#include "nr5g_fapi_dpdk.h"
#include "nr5g_fapi_framework.h"
#include "nr5g_fapi_cmd.h"

int main(
    int argc,
    char **argv)
{
    const char *config_file = NULL;
    p_nr5g_fapi_cfg_t config = NULL;

    if (NULL == (config_file = nr5g_fapi_parse_args(argc, argv))) {
        NR5G_FAPI_LOG(ERROR_LOG, ("Config file parsing error"));
        exit(1);
    }

    if (NULL == (config = nr5g_fapi_config_loader(argv[0], config_file))) {
        NR5G_FAPI_LOG(ERROR_LOG, ("Config file loading error"));
        exit(1);
    }

    nr5g_fapi_dpdk_init(config);

    if (FAILURE == nr5g_fapi_framework_init(config)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("ORAN_5G_FAPI init failed"));
        exit(1);
    }
    NR5G_FAPI_LOG(INFO_LOG, ("ORAN_5G_FAPI init successful"));

    nr5g_fapi_cmgr((void *)config);
    nr5g_fapi_dpdk_wait(config);
    pthread_attr_destroy(&config->phy2mac_thread_info.thread_attr);
    pthread_attr_destroy(&config->mac2phy_thread_info.thread_attr);
    pthread_attr_destroy(&config->urllc_thread_info.thread_attr);
    free(config);
    return 0;
}
