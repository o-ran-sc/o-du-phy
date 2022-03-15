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

#ifndef NR5G_FAPI_LOG_H_
#define NR5G_FAPI_LOG_H_

#include <stdio.h>

#define NR5G_FAPI_STATS_FNAME "FapiStats.txt"

typedef enum _nr5g_fapi_log_types_t {
    NONE_LOG = 0,
    INFO_LOG,                    // default
    DEBUG_LOG,
    ERROR_LOG,
    TRACE_LOG,
} nr5g_fapi_log_types_t;

extern nr5g_fapi_log_types_t nr5g_fapi_log_level_g;
// get_logger_type_str is utility function, returns logging lever string.
char *get_logger_type_str(
    nr5g_fapi_log_types_t e);

void nr5g_fapi_set_log_level(
    nr5g_fapi_log_types_t new_level);

nr5g_fapi_log_types_t nr5g_fapi_get_log_level(
    );

// NR5G_FAPI__LOG utility Macro for logging.
#define NR5G_FAPI_LOG(TYPE, MSG) do { \
    if(TYPE == ERROR_LOG) { \
        printf("[%s]", get_logger_type_str(TYPE)); \
        printf MSG ;\
        printf("\n");\
    }   \
    else \
    { \
        if((nr5g_fapi_log_level_g > NONE_LOG) && (TYPE <= nr5g_fapi_log_level_g)) { \
            printf("[%s]", get_logger_type_str(TYPE)); \
            printf MSG ;\
            printf("\n");\
        }   \
    } \
} while(0)

typedef struct _nr5g_fapi_performance_statistic {
    uint64_t min_cycle;
    uint64_t max_cycle;
    uint64_t avg_cycle;
    uint32_t count;
} nr5g_fapi_performance_statistic_t;

extern nr5g_fapi_log_types_t nr5g_fapi_log_level_g;
extern nr5g_fapi_performance_statistic_t fapi_statis_info_wls_get_dl;
extern nr5g_fapi_performance_statistic_t fapi_statis_info_parse_dl;
extern nr5g_fapi_performance_statistic_t fapi_statis_info_wls_send_dl;
extern nr5g_fapi_performance_statistic_t fapi_statis_info_wls_get_ul;
extern nr5g_fapi_performance_statistic_t fapi_statis_info_parse_ul;
extern nr5g_fapi_performance_statistic_t fapi_statis_info_wls_send_ul;
extern uint64_t tick_total_wls_get_per_tti_dl;
extern uint64_t tick_total_parse_per_tti_dl;
extern uint64_t tick_total_wls_send_per_tti_dl;
extern uint64_t tick_total_wls_get_per_tti_ul;
extern uint64_t tick_total_parse_per_tti_ul;
extern uint64_t tick_total_wls_send_per_tti_ul;
extern uint16_t g_statistic_start_flag;

uint16_t nr5g_fapi_statistic_info_set(
    nr5g_fapi_performance_statistic_t * fapi_statis_info,
    uint64_t * tick_val_in,
    uint16_t start_flag);

uint16_t nr5g_fapi_statistic_info_init(
    );

uint16_t nr5g_fapi_statistic_info_print(
    );
uint16_t nr5g_fapi_statistic_info_set_all(
    );

#endif                          // NR5G_FAPI_LOG_H_
