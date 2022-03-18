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
 * to PHY P5 messages
 *
 **/

#ifndef _NR5G_FAPI_FAP2PHY_P5_PVT_PROC_H_
#define _NR5G_FAPI_FAP2PHY_P5_PVT_PROC_H_

//x is 32 bit variable, y is length in bytes
#define GETVLFRM32B(x, y)  ((x) & ((0xFFFFFFFF) >> (32 - (y << 3))))

uint8_t nr5g_fapi_config_req_to_phy_translation(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    fapi_config_req_t * p_fapi_req,
    PCONFIGREQUESTStruct p_ia_config_req);

uint8_t nr5g_fapi_config_req_fill_dependent_fields(
    PCONFIGREQUESTStruct p_ia_config_req);

uint8_t nr5g_fapi_calc_phy_tdd_period(
    uint8_t fapi_tdd_period,
    uint8_t n_subc_common);

uint8_t nr5g_fapi_calc_phy_tdd_period_for_n_subc_common_0(
    uint8_t fapi_tdd_period);

uint8_t nr5g_fapi_calc_phy_tdd_period_for_n_subc_common_1(
    uint8_t fapi_tdd_period);

uint8_t nr5g_fapi_calc_phy_tdd_period_for_n_subc_common_2(
    uint8_t fapi_tdd_period);

uint8_t nr5g_fapi_calc_phy_tdd_period_for_n_subc_common_3(
    uint8_t fapi_tdd_period);

uint16_t nr5g_fapi_calc_fft_size(
    uint8_t nSubcCommon,
    uint16_t bw);

#endif                          //_NR5G_FAPI_FAP2PHY_P5_PVT_PROC_H_
