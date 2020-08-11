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
 * @file This file defines all the functions used for logging. 
 *
 **/
#include "nr5g_fapi_std.h"
#include "nr5g_fapi_log.h"
#include "nr5g_fapi_memory.h"

nr5g_fapi_log_types_t nr5g_fapi_log_level_g;
nr5g_fapi_performance_statistic_t fapi_statis_info_wls_get_dl;
nr5g_fapi_performance_statistic_t fapi_statis_info_parse_dl;
nr5g_fapi_performance_statistic_t fapi_statis_info_wls_send_dl;
nr5g_fapi_performance_statistic_t fapi_statis_info_wls_get_ul;
nr5g_fapi_performance_statistic_t fapi_statis_info_parse_ul;
nr5g_fapi_performance_statistic_t fapi_statis_info_wls_send_ul;
uint64_t tick_total_wls_get_per_tti_dl = 0;
uint64_t tick_total_parse_per_tti_dl = 0;
uint64_t tick_total_wls_send_per_tti_dl = 0;
uint64_t tick_total_wls_get_per_tti_ul = 0;
uint64_t tick_total_parse_per_tti_ul = 0;
uint64_t tick_total_wls_send_per_tti_ul = 0;
uint16_t g_statistic_start_flag = 0;

//------------------------------------------------------------------------------
/** @ingroup        group_lte_source_phy_api
 *
 *  @param[in]      FAPI log level
 *
 *  @return         String of log type
 *
 *  @description    The returns log level sring
 *
 **/
//------------------------------------------------------------------------------
char *get_logger_type_str(
    nr5g_fapi_log_types_t e)
{
    switch (e) {
        case INFO_LOG:
            return ("INFO_LOG");

        case DEBUG_LOG:
            return ("DEBUG_LOG");

        case ERROR_LOG:
            return ("ERROR_LOG");

        case TRACE_LOG:
            return ("TRACE_LOG");

        default:
            printf("Log Error Type\n");
            return ("");
            //case NONE: Should never be reached.
    }
}

//------------------------------------------------------------------------------
/** @ingroup        group_lte_source_phy_api
 *
 *  @param[in]      log level
 *
 *  @return         void
 *
 *  @description    The function sets log level
 *
 **/
//------------------------------------------------------------------------------
void nr5g_fapi_set_log_level(
    nr5g_fapi_log_types_t new_level)
{
    nr5g_fapi_log_level_g = new_level;
}

//------------------------------------------------------------------------------
/** @ingroup        group_lte_source_phy_api
 *
 *  @param[in]      void
 *
 *  @return         Log level
 *
 *  @description    The function returns log level.
 *
 **/
//------------------------------------------------------------------------------
nr5g_fapi_log_types_t nr5g_fapi_get_log_level(
    )
{
    return nr5g_fapi_log_level_g;
}
#ifdef STATISTIC_MODE
uint16_t nr5g_fapi_statistic_info_set(
    nr5g_fapi_performance_statistic_t * fapi_statis_info,
    uint64_t * tick_val_in,
    uint16_t start_flag)
{

    uint64_t tick_val = *tick_val_in;
    uint64_t cnt = fapi_statis_info->count;

    if (start_flag) {
        if (cnt) {
            if (tick_val < fapi_statis_info->min_cycle) {
                fapi_statis_info->min_cycle = tick_val;
            } else if (tick_val > fapi_statis_info->max_cycle) {
                fapi_statis_info->max_cycle = tick_val;
            }

            fapi_statis_info->avg_cycle += tick_val;
            fapi_statis_info->count++;

        } else {
            fapi_statis_info->avg_cycle = tick_val;
            fapi_statis_info->min_cycle = tick_val;
            fapi_statis_info->max_cycle = tick_val;
            fapi_statis_info->count++;
        }
    }
    *tick_val_in = 0;
    return 0;
}

uint16_t nr5g_fapi_statistic_info_set_all(
    )
{

    nr5g_fapi_statistic_info_set(&fapi_statis_info_wls_get_dl,
        &tick_total_wls_get_per_tti_dl, g_statistic_start_flag);
    nr5g_fapi_statistic_info_set(&fapi_statis_info_parse_dl,
        &tick_total_parse_per_tti_dl, g_statistic_start_flag);
    nr5g_fapi_statistic_info_set(&fapi_statis_info_wls_send_dl,
        &tick_total_wls_send_per_tti_dl, g_statistic_start_flag);
    nr5g_fapi_statistic_info_set(&fapi_statis_info_wls_get_ul,
        &tick_total_wls_get_per_tti_ul, g_statistic_start_flag);
    nr5g_fapi_statistic_info_set(&fapi_statis_info_parse_ul,
        &tick_total_parse_per_tti_ul, g_statistic_start_flag);
    nr5g_fapi_statistic_info_set(&fapi_statis_info_wls_send_ul,
        &tick_total_wls_send_per_tti_ul, g_statistic_start_flag);

    return 0;
}

uint16_t nr5g_fapi_statistic_info_init(
    )
{

    NR5G_FAPI_MEMSET(&fapi_statis_info_wls_get_dl,
        sizeof(nr5g_fapi_performance_statistic_t), 0,
        sizeof(nr5g_fapi_performance_statistic_t));
    NR5G_FAPI_MEMSET(&fapi_statis_info_parse_dl,
        sizeof(nr5g_fapi_performance_statistic_t), 0,
        sizeof(nr5g_fapi_performance_statistic_t));
    NR5G_FAPI_MEMSET(&fapi_statis_info_wls_send_dl,
        sizeof(nr5g_fapi_performance_statistic_t), 0,
        sizeof(nr5g_fapi_performance_statistic_t));
    NR5G_FAPI_MEMSET(&fapi_statis_info_wls_get_ul,
        sizeof(nr5g_fapi_performance_statistic_t), 0,
        sizeof(nr5g_fapi_performance_statistic_t));
    NR5G_FAPI_MEMSET(&fapi_statis_info_parse_ul,
        sizeof(nr5g_fapi_performance_statistic_t), 0,
        sizeof(nr5g_fapi_performance_statistic_t));
    NR5G_FAPI_MEMSET(&fapi_statis_info_wls_send_ul,
        sizeof(nr5g_fapi_performance_statistic_t), 0,
        sizeof(nr5g_fapi_performance_statistic_t));
    return 0;

}

uint16_t nr5g_fapi_statistic_info_print(
    )
{

    printf("\n");
    printf
        ("dl wls get Kcycle (count, min, max, avg)   : %d    %9.2f       %9.2f       %9.2f \n",
        fapi_statis_info_wls_get_dl.count,
        fapi_statis_info_wls_get_dl.min_cycle / 1000.,
        fapi_statis_info_wls_get_dl.max_cycle / 1000.,
        fapi_statis_info_wls_get_dl.avg_cycle /
        fapi_statis_info_wls_get_dl.count / 1000.);
    printf
        ("dl FAPI parse Kcycle (count, min, max, avg): %d    %9.2f       %9.2f       %9.2f \n",
        fapi_statis_info_parse_dl.count,
        fapi_statis_info_parse_dl.min_cycle / 1000.,
        fapi_statis_info_parse_dl.max_cycle / 1000.,
        fapi_statis_info_parse_dl.avg_cycle / fapi_statis_info_parse_dl.count /
        1000.);
    printf
        ("dl wls send Kcycle(count, min, max, avg)   : %d    %9.2f       %9.2f       %9.2f \n",
        fapi_statis_info_wls_send_dl.count,
        fapi_statis_info_wls_send_dl.min_cycle / 1000.,
        fapi_statis_info_wls_send_dl.max_cycle / 1000.,
        fapi_statis_info_wls_send_dl.avg_cycle /
        fapi_statis_info_wls_send_dl.count / 1000.);

    printf
        ("ul wls get Kcycle(count, min, max, avg)    : %d    %9.2f       %9.2f       %9.2f \n",
        fapi_statis_info_wls_get_ul.count,
        fapi_statis_info_wls_get_ul.min_cycle / 1000.,
        fapi_statis_info_wls_get_ul.max_cycle / 1000.,
        fapi_statis_info_wls_get_ul.avg_cycle /
        fapi_statis_info_wls_get_ul.count / 1000.);
    printf
        ("ul FAPI parse Kcycle(count, min, max, avg) : %d    %9.2f       %9.2f       %9.2f \n",
        fapi_statis_info_parse_ul.count,
        fapi_statis_info_parse_ul.min_cycle / 1000.,
        fapi_statis_info_parse_ul.max_cycle / 1000.,
        fapi_statis_info_parse_ul.avg_cycle / fapi_statis_info_parse_ul.count /
        1000.);
    printf
        ("ul wls send Kcycle(count, min, max, avg)   : %d    %9.2f       %9.2f       %9.2f \n",
        fapi_statis_info_wls_send_ul.count,
        fapi_statis_info_wls_send_ul.min_cycle / 1000.,
        fapi_statis_info_wls_send_ul.max_cycle / 1000.,
        fapi_statis_info_wls_send_ul.avg_cycle /
        fapi_statis_info_wls_send_ul.count / 1000.);
    return 0;

}
#else
uint16_t nr5g_fapi_statistic_info_set(
    nr5g_fapi_performance_statistic_t * fapi_statis_info,
    uint64_t * tick_val_in,
    uint16_t start_flag)
{

    fapi_statis_info->avg_cycle = 0;
	*tick_val_in = 0;
	if(start_flag) *tick_val_in = 0;
	return 0;
}

uint16_t nr5g_fapi_statistic_info_set_all(
    )
{


    return 0;
}

uint16_t nr5g_fapi_statistic_info_init(
    )
{

 
    return 0;

}

uint16_t nr5g_fapi_statistic_info_print(
    )
{

    return 0;

}

#endif
