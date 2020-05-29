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
 * @file
 * This file consist of macros, structures and prototypes of all FAPI
 * Error indication message
 *
 **/

#ifndef _NR5G_FAPI_PROC_ERROR_IND_H_
#define _NR5G_FAPI_PROC_ERROR_IND_H_

uint8_t nr5g_fapi_error_indication(
    p_nr5g_fapi_phy_ctx_t p_phy_ctx,
    PERRORIndicationStruct p_iapi_resp);

#endif                          //_NR5G_FAPI_PROC_ERROR_IND_H_
