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

#ifndef NR5G_FAPI_SNR_CONVERSION_H_
#define NR5G_FAPI_SNR_CONVERSION_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t nr5g_fapi_convert_snr_iapi_to_fapi(const int16_t snr);

#ifdef __cplusplus
}
#endif

#endif  // NR5G_FAPI_SNR_CONVERSION_H_
