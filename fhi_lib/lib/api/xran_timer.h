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

#include "xran_fh_o_du.h"
#include "xran_ethdi.h"

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

#define MAX_TTI_TO_PHY_TIMER         (10)

#define XranIncrementSymIdx(sym_idx, numSymPerMs)  (((uint32_t)sym_idx >= (((uint32_t)numSymPerMs * MSEC_PER_SEC) - 1)) ? 0 : (uint32_t)sym_idx+1)
#define XranDecrementSymIdx(sym_idx, numSymPerMs)  (((uint32_t)sym_idx == 0) ? (((uint32_t)numSymPerMs * MSEC_PER_SEC)) - 1) : (uint32_t)sym_idx-1)

enum xran_tmthread_state
{
    XRAN_TMTHREAD_STAT_EXIT = -1,
    XRAN_TMTHREAD_STAT_STOP = 0,
    XRAN_TMTHREAD_STAT_RUN  = 1,
    XRAN_TMTHRED_STAT_MAX
};


struct xran_timing_source_ctx
{
   enum xran_tmthread_state state;

   struct timespec started_time;
   struct timespec last_time;
   struct timespec cur_time;
   struct timespec sleeptime;

   volatile unsigned long current_second;
   unsigned long started_second;

   uint64_t curr_tick;
   uint64_t last_tick;

   uint64_t total_tick;
   uint64_t used_tick;

   uint8_t  timerMu;
   uint16_t sfnAtSecond;   /* SFN at current second start */
   uint16_t maxFrame;      /* value of max frame used for System Frame Number Calculation
                             * expected to be 99 (old compatibility mode) or 1023 as per section 9.7.2 */
   int64_t  offset_sec;
   int64_t  offset_nsec;    //offset to GPS time calculated based on alpha and beta

   //uint32_t ota_tti;
   xran_fh_tti_callback_fn ttiCb[XRAN_CB_MAX];
   void *TtiCbParam[XRAN_CB_MAX];
   uint32_t SkipTti[XRAN_CB_MAX];
   struct rte_timer tti_to_phy_timer[MAX_TTI_TO_PHY_TIMER][XRAN_MAX_NUM_MU];

   uint64_t timer_missed_sym;
   uint64_t timer_missed_slot;
#ifdef POLL_EBBU_OFFLOAD
   uint64_t timer_missed_sym_window;
#endif
};


int timing_set_debug_stop(int value, int count);
int timing_get_debug_stop(void);

inline unsigned long get_ticks_diff(unsigned long curr_tick, unsigned long last_tick)
{
   if (curr_tick >= last_tick)
      return (unsigned long)(curr_tick - last_tick);
   else
      return (unsigned long)(0xFFFFFFFFFFFFFFFF - last_tick + curr_tick);
}

inline uint64_t xran_tick(void)
{
    uint32_t hi, lo;
    __asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (uint64_t)lo)|( ((uint64_t)hi)<<32 );
}

int xran_timingsource_set_gpsoffset(int64_t offset_sec, int64_t offset_nsec);
//uint32_t xran_timingsource_get_coreid(void);
long xran_timingsource_poll_next_tick(long interval_ns, unsigned long *used_tick);
long xran_timingsource_sleep_next_tick(long interval);
//long poll_next_tick(long interval_ns, unsigned long *used_tick);
//long sleep_next_tick(long interval);

inline uint32_t xran_timingsource_get_max_ota_sym_idx(uint8_t numerlogy)
{
   extern const uint8_t slots_per_subframe[5];
   return (XRAN_NUM_OF_SYMBOL_PER_SLOT * slots_per_subframe[numerlogy] * MSEC_PER_SEC);
}

inline struct xran_timing_source_ctx *xran_timingsource_get_ctx(void)
{
   extern struct xran_timing_source_ctx xran_timerCtx;
   return (&xran_timerCtx);
}

inline uint64_t xran_timingsource_get_current_second(void)
{
    return xran_timingsource_get_ctx()->current_second;
}

/* Get the numerology used by timer (this will be the highest numerology in use) */
inline uint8_t xran_timingsource_get_numerology(void)
{
    return (xran_timingsource_get_ctx()->timerMu);
}

/* Set the numerology used by timer. This could be called multiple times during xran_open().
 * At the end, the highest numerology will be set */
int xran_timingsource_set_numerology(uint8_t value);

inline enum xran_tmthread_state xran_timingsource_get_state(void)
{
    return(xran_timingsource_get_ctx()->state);
}
inline void xran_timingsource_set_state(enum xran_tmthread_state state)
{
    xran_timingsource_get_ctx()->state = state;
}

inline uint32_t xran_timingsource_get_coreid(void)
{
   return(xran_ethdi_get_ctx()->io_cfg.timing_core);
}

#ifdef POLL_EBBU_OFFLOAD
int timing_get_debug_stop_count(void);
int timing_get_start_second(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
