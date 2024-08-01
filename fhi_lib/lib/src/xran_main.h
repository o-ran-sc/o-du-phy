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

extern uint32_t xran_lib_ota_sym_idx[];
extern uint32_t xran_lib_ota_sym_mu[];
extern uint32_t xran_lib_ota_sym_idx_mu[];

extern uint16_t xran_SFN_at_Sec_Start;
extern uint16_t xran_max_frame;

uint32_t xran_schedule_to_worker(enum xran_job_type_id job_type_id, struct xran_device_ctx *p_xran_dev_ctx);
uint16_t xran_getSfnSecStart(void);
void tti_ota_cb(struct rte_timer *tim, uint8_t mu);
void sym_ota_cb(void *arg, unsigned long *used_tick, uint8_t mu);
void tx_cp_dl_cb(struct rte_timer *tim, void *arg);
void tx_cp_ul_cb(struct rte_timer *tim, void *arg);
void tti_to_phy_cb(struct rte_timer *tim, void *arg);
#ifdef POLL_EBBU_OFFLOAD
void tti_ota_cb_ebbu_offload(struct rte_timer *tim, void *arg, uint8_t mu);
#endif

void rx_ul_deadline_full_cb(struct rte_timer *tim, void *arg);
void rx_ul_user_sym_cb(struct rte_timer *tim, void *arg);
void rx_ul_deadline_half_cb(struct rte_timer *tim, void *arg);
void rx_ul_deadline_one_fourths_cb(struct rte_timer *tim, void *arg);
void rx_ul_deadline_three_fourths_cb(struct rte_timer *tim, void *arg);
void rx_ul_static_srs_cb(struct rte_timer *tim, void *arg);
int32_t xran_fh_rx_and_up_tx_processing(void *port_mask);

#ifdef __cplusplus
}
#endif

#endif /* _XRAN_MAIN_H_ */
