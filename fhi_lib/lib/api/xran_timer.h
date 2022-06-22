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

/* Difference between Unix seconds to GPS seconds
   GPS epoch: 1980.1.6 00:00:00 (UTC); Unix time epoch: 1970:1.1 00:00:00 UTC
   Value is calculated on Sep.6 2019. Need to be change if International
   Earth Rotation and Reference Systems Service (IERS) adds more leap seconds
   1970:1.1 - 1980.1.6: 3657 days
   3657*24*3600=315 964 800 seconds (unix seconds value at 1980.1.6 00:00:00 (UTC))
   There are 18 leap seconds inserted after 1980.1.6 00:00:00 (UTC), which means
   GPS is 18 larger. 315 964 800 - 18 = 315 964 782
*/
#define UNIX_TO_GPS_SECONDS_OFFSET 315964782UL
#define NUM_OF_FRAMES_PER_SFN_PERIOD 1024
#define NUM_OF_FRAMES_PER_SECOND 100

#define MSEC_PER_SEC 1000L

#define XranIncrementSymIdx(sym_idx, numSymPerMs)  (((uint32_t)sym_idx >= (((uint32_t)numSymPerMs * MSEC_PER_SEC) - 1)) ? 0 : (uint32_t)sym_idx+1)
#define XranDecrementSymIdx(sym_idx, numSymPerMs)  (((uint32_t)sym_idx == 0) ? (((uint32_t)numSymPerMs * MSEC_PER_SEC)) - 1) : (uint32_t)sym_idx-1)

uint64_t xran_tick(void);
unsigned long get_ticks_diff(unsigned long curr_tick, unsigned long last_tick);
long poll_next_tick(long interval_ns, unsigned long *used_tick);
long sleep_next_tick(long interval);
int timing_set_debug_stop(int value, int count);
int timing_get_debug_stop(void);
uint64_t timing_get_current_second(void);
uint8_t timing_get_numerology(void);
int timing_set_numerology(uint8_t value);
uint32_t xran_max_ota_sym_idx(uint8_t numerlogy);

#ifdef __cplusplus
}
#endif

#endif
