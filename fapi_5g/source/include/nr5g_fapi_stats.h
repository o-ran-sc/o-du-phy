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
 * @file This file consist of fapi logging macro.
 *
 **/

#ifndef NR5G_FAPI_STATS_H_
#define NR5G_FAPI_STATS_H_

#include<sys/stat.h>
#include <fcntl.h>
#include<stdint.h>
#include<stdio.h>
#include "nr5g_fapi_framework.h"

void nr5g_fapi_print_phy_instance_stats(
    p_nr5g_fapi_phy_instance_t p_phy_instance);
inline int nr5g_fapi_check_for_file_link(
    char *fname);
inline int nr5g_fapi_change_file_permission(
    int fd,
    mode_t mode);

#endif                          // NR5G_FAPI_STATS_H_
