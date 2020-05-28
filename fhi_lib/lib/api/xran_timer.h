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
 * @brief This file provides interface to Timing for XRAN.
 *
 * @file xran_timer.h
 * @ingroup group_lte_source_xran
 * @author Intel Corporation
 *
 **/

#ifndef _XRAN_TIMER_H
#define _XRAN_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define MSEC_PER_SEC 1000L

#define XranIncrementSymIdx(sym_idx, numSymPerMs)  (((uint32_t)sym_idx >= (((uint32_t)numSymPerMs * MSEC_PER_SEC) - 1)) ? 0 : (uint32_t)sym_idx+1)
#define XranDecrementSymIdx(sym_idx, numSymPerMs)  (((uint32_t)sym_idx == 0) ? (((uint32_t)numSymPerMs * MSEC_PER_SEC)) - 1) : (uint32_t)sym_idx-1)

uint64_t xran_tick(void);
unsigned long get_ticks_diff(unsigned long curr_tick, unsigned long last_tick);
long poll_next_tick(long interval_ns, unsigned long *used_tick);
long sleep_next_tick(long interval);
int timing_set_debug_stop(int value, int count);
int timing_get_debug_stop(void);
inline uint64_t timing_get_current_second(void);
int timing_set_numerology(uint8_t value);

#ifdef __cplusplus
}
#endif

#endif
