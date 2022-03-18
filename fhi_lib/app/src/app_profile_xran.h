/******************************************************************************
*
*   Copyright (c) 2020 Intel.
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

#ifndef _APP_PROFILE_XRAN_H_
#define _APP_PROFILE_XRAN_H_

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

struct xran_mlog_times
{
    uint64_t ticks_per_usec;
    uint64_t core_total_time;   /* in us */
    uint64_t core_used_time;    /* in us */
    uint64_t xran_total_time;   /* in us */
    uint64_t mlog_total_time;   /* in us */
};

struct xran_mlog_stats
{
    uint32_t cnt;
    uint32_t max;
    uint32_t min;
    float avg;
};

struct xran_mlog_taskid
{
    uint16_t taskId;
    uint16_t taskType;
    char taskName[80];
};

enum xran_mlog_task_type {
    XRAN_TASK_TYPE_GNB = 0,
    XRAN_TASK_TYPE_BBDEV,
    XRAN_TASK_TYPE_TIMER,
    XRAN_TASK_TYPE_RADIO,
    XRAN_TASK_TYPE_CP,
    XRAN_TASK_TYPE_UP,
    XRAN_TASK_TYPE_MAX, /* The last entry : total# of types */
};

#define XRAN_REPORT_FILE    "xran_mlog_stats"

#ifdef MLOG_ENABLED
int32_t app_profile_xran_print_mlog_stats(char *usecase_file);
#else
#define app_profile_xran_print_mlog_stats(a)
#endif

#ifndef WIN32
#ifdef PRINTF_ERR_OK
#define print_err(fmt, args...) printf("%s:[err] " fmt "\n", __FUNCTION__, ## args)
#else  /* PRINTF_LOG_OK */
#define print_err(fmt, args...)
#endif  /* PRINTF_LOG_OK */
#else
#define print_err(fmt, ...) printf("%s:[err] " fmt "\n", __FUNCTION__, __VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif

extern struct xran_mlog_times mlog_times;

#endif /* _APP_PROFILE_XRAN_ */
