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
 * @brief Header file for function to work with 5G NR frame structure and related
 *        routines
 * @file xran_frame_struct.h
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#ifndef _XRAN_FRAME_STRUCT_
#define _XRAN_FRAME_STRUCT_

#ifdef __cplusplus
extern "C" {
#endif


#include "xran_fh_o_du.h"

uint32_t xran_fs_get_tti_interval(uint8_t nMu);
uint32_t xran_fs_get_scs(uint8_t nMu);

//-------------------------------------------------------------------------------------------
/** @ingroup group_nr5g_source_phy_common
 *
 *  @param[in]   nNumerology - Numerology determine sub carrier spacing, Value: 0->4 0: 15khz,  1: 30khz,  2: 60khz 3: 120khz, 4: 240khz
 *  @param[in]   nBandwidth - Carrier bandwidth for in MHz. Value: 5->400
 *  @param[in]   nAbsFrePointA - Abs Freq Point A of the Carrier Center Frequency for in KHz Value: 450000->52600000
 *
 *  @return  Number of RBs in cell
 *
 *  @description
 *  Returns number of RBs based on 38.101-1 and 38.101-2 for the cell
 *
**/
//-------------------------------------------------------------------------------------------
uint16_t xran_fs_get_num_rbs(uint32_t nNumerology, uint32_t nBandwidth, uint32_t nAbsFrePointA);

//-------------------------------------------------------------------------------------------
/** @ingroup phy_cal_nrarfcn
 *
 *  @param[in]   center frequency
 *
 *  @return  NR-ARFCN
 *
 *  @description
 *  This calculates NR-ARFCN value according to center frequency
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t xran_fs_cal_nrarfcn(uint32_t nCenterFreq);
int32_t xran_fs_slot_limit(int32_t nSlotIdx);
void xran_fs_clear_slot_type(uint32_t nCcId);
int32_t xran_fs_set_slot_type(uint32_t nCcId, uint32_t nFrameDuplexType, uint32_t nTddPeriod, struct xran_slot_config* psSlotConfig);
int32_t xran_fs_get_slot_type(int32_t nCcId, int32_t nSlotIdx, int32_t nType);
uint32_t xran_fs_slot_limit_init(int32_t tti_interval_us);
uint32_t xran_fs_get_max_slot(void);
uint32_t xran_fs_get_max_slot_SFN(void);
int32_t xran_fs_get_symbol_type(int32_t nCellIdx, int32_t nSlotdx,  int32_t nSymbIdx);

#ifdef __cplusplus
}
#endif

#endif /* _XRAN_FRAME_STRUCT_ */

