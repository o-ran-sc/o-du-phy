/******************************************************************************
*
*   Copyright (c) 2021 Intel.
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
 * @file This file consist of SNR converter from IntelAPI to FAPI.
 *
 **/

#include "nr5g_fapi_snr_conversion.h"

#include <math.h>

#include "gnb_l1_l2_api.h"

uint8_t nr5g_fapi_convert_snr_iapi_to_fapi(const int16_t snr)
{
    double temp = (double)snr / SINR_STEP_SIZE;
    if (temp < 0)
    {
        return 2 * ((uint8_t)floor(temp) & 0x003F);
    }
    return (2 * ((uint8_t)ceil(temp) & 0x003F)) + 128;
}
