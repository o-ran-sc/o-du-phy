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

#define NR5G_FAPI_STATS_FNAME "FapiStats.txt"

typedef enum _nr5g_fapi_log_types_t {
    INFO_LOG = 0,
    DEBUG_LOG,
    ERROR_LOG,
    TRACE_LOG,
    HEXDUMP_LOG,
    NONE_LOG                    // default
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
        printf("[NR5G_FAPI][%s]", get_logger_type_str(TYPE)); \
        printf MSG ;\
        printf("\n");\
    }   \
    else \
    { \
        if(nr5g_fapi_log_level_g == NONE_LOG) { \
        }   \
        else if(TYPE <= nr5g_fapi_log_level_g) { \
            printf("[NR5G_FAPI][%s]", get_logger_type_str(TYPE)); \
            printf MSG ;\
            printf("\n");\
        }   \
        else if(TYPE <= nr5g_fapi_log_level_g && TYPE == DEBUG_LOG) { \
            printf("[NR5G_FAPI][%s]", get_logger_type_str(TYPE)); \
            printf MSG ;\
            printf("\n");\
        }   \
        else if(TYPE == nr5g_fapi_log_level_g && TYPE == TRACE_LOG) { \
            printf("[NR5G_FAPI][%s]", get_logger_type_str(TYPE)); \
            printf MSG ;\
            printf("\n");\
        }   \
        else {}\
    } \
} while(0)

#endif                          // NR5G_FAPI_LOG_H_
