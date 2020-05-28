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
 * @brief This file provides interface to synchronization related APIs (PTP/1588)
 *        for XRAN.
 *
 * @file xran_sync_api.h
 * @ingroup group_lte_source_xran
 * @author Intel Corporation
 *
 **/

#ifndef _XRAN_SYNC_API_H_
#define _XRAN_SYNC_API_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Function checks if machine is synchronized using PTP for Linux
 *        software.
 *
 * @return int Returns 0 if synchronized, otherwise positive.
 */
int xran_is_synchronized(void);

#ifdef __cplusplus
}
#endif

#endif /* _XRAN_SYNC_API_H_ */
