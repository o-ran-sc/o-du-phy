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
#ifndef POLL_EBBU_OFFLOAD
int32_t xran_timing_destroy_cbs(void *args);
#else
/* This is duplicated 5GNR task definition from L1 application and it must be aligned with L1 application side */
typedef enum
{
    TMNG_SYM_POLL = 7,               /* 7 Symbol polling task which will do all symbol-level polling tasks*/
    TMNG_PKT_POLL,                   /* 8 RX packet polling processing task which will do RX packet polling processing tasks*/
    TMNG_DL_CP_POLL,                 /* 9 DL CP Polling tasks*/
    TMNG_UL_CP_POLL,                 /* 10 UL CP Polling tasks*/
    TMNG_TTI_POLL,                   /* 11 TTI Polling tasks*/
}eBbuPoolTimingTaskTypeEnumPollEbbuOffload;
#endif

void xran_timer_arm_ex(struct rte_timer *tim, void* CbFct, void *CbArg, unsigned tim_lcore);
void xran_timer_arm_cp_dl(struct rte_timer *tim, void* arg, void* p_dev_ctx, uint8_t mu);
void xran_timer_arm_cp_ul(struct rte_timer *tim, void* arg, void* p_dev_ctx, uint8_t mu);
void xran_timer_arm_ex(struct rte_timer *tim, void* CbFct, void *CbArg, unsigned tim_lcore);

#ifdef __cplusplus
}
#endif

#endif /* _XRAN_CB_PROC_H_ */
