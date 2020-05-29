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
#ifndef NR5G_FAPI_ARGS_H_
#define NR5G_FAPI_ARGS_H_

#include "nr5g_fapi_std.h"
#include "nr5g_fapi_common_types.h"
#include "nr5g_fapi_dpdk.h"

const char *nr5g_fapi_parse_args(
    int argc,
    char **argv);

#endif                          // NR5G_FAPI_ARGS_H_
