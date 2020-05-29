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

nr5g_fapi_log_types_t nr5g_fapi_log_level_g;

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

        case HEXDUMP_LOG:
            return ("HEXDUMP_LOG");

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

