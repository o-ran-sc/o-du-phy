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
 * @brief XRAN main module header file
 * @file xran_main.h
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#ifndef _XRAN_MAIN_H_
#define _XRAN_MAIN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/queue.h>

#include <rte_timer.h>

#include "xran_dev.h"

extern uint32_t xran_lib_ota_tti[];
extern uint32_t xran_lib_ota_sym[];
extern uint32_t xran_lib_ota_sym_idx[];

extern uint16_t xran_SFN_at_Sec_Start;
extern uint16_t xran_max_frame;

static struct timespec sleeptime = {.tv_nsec = 1E3 }; /* 1 us */

uint32_t xran_schedule_to_worker(enum xran_job_type_id job_type_id, struct xran_device_ctx * p_xran_dev_ctx);
uint16_t xran_getSfnSecStart(void);
void tx_cp_dl_cb(struct rte_timer *tim, void *arg);
void tx_cp_ul_cb(struct rte_timer *tim, void *arg);
void tti_to_phy_cb(struct rte_timer *tim, void *arg);

void rx_ul_deadline_full_cb(struct rte_timer *tim, void *arg);
void rx_ul_user_sym_cb(struct rte_timer *tim, void *arg);
void rx_ul_deadline_half_cb(struct rte_timer *tim, void *arg);

#ifdef __cplusplus
}
#endif

#endif /* _XRAN_MAIN_H_ */
