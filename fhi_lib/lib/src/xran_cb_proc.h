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

/**
 * @brief XRAN Callback processing module header file
 * @file xran_cb_proc.h
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#ifndef _XRAN_CB_PROC_H_
#define _XRAN_CB_PROC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <unistd.h>

int32_t xran_timing_create_cbs(void *args);
int32_t xran_timing_destroy_cbs(void *args);

void xran_timer_arm_ex(struct rte_timer *tim, void* CbFct, void *CbArg, unsigned tim_lcore);
void xran_timer_arm(struct rte_timer *tim, void* arg, void* p_dev_ctx);
void xran_timer_arm_cp_dl(struct rte_timer *tim, void* arg, void* p_dev_ctx);
void xran_timer_arm_cp_ul(struct rte_timer *tim, void* arg, void* p_dev_ctx);
void xran_timer_arm_ex(struct rte_timer *tim, void* CbFct, void *CbArg, unsigned tim_lcore);

#ifdef __cplusplus
}
#endif

#endif /* _XRAN_CB_PROC_H_ */
