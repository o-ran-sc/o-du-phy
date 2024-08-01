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
 * @brief This file provides implementation to Timing for XRAN.
 *
 * @file xran_timer.c
 * @ingroup group_lte_source_xran
 * @author Intel Corporation
 *
 **/

#define _GNU_SOURCE
#include <sched.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <immintrin.h>

#include "xran_timer.h"
#include "xran_main.h"
#include "xran_printf.h"
#include "xran_mlog_lnx.h"
#include "xran_lib_mlog_tasks_id.h"
#include "xran_fh_o_du.h"
#include "xran_common.h"
#include "xran_cb_proc.h"
#include "xran_frame_struct.h"
#include "xran_ecpri_owd_measurements.h"


#define NSEC_PER_SEC  1000000000L
#define NSEC_PER_USEC 1000L
#define THRESHOLD        35  /**< the avg cost of clock_gettime() in ns */
#define TIMECOMPENSATION 2   /**< time compensation in us, avg latency of clock_nanosleep */

#define SEC_MOD_STOP (60)

struct xran_timing_source_ctx xran_timerCtx =
{
    .state = XRAN_TMTHREAD_STAT_STOP,
    .sleeptime.tv_nsec = 1E3
};

static struct timespec* p_cur_time  = &xran_timerCtx.cur_time;
static struct timespec* p_last_time = &xran_timerCtx.last_time;

extern uint32_t xran_lib_ota_tti_base;
extern uint32_t xran_lib_ota_tti_mu[XRAN_PORTS_NUM][XRAN_MAX_NUM_MU];
extern uint32_t xran_lib_ota_sym_idx_mu[];
extern uint32_t xran_lib_ota_sym_mu[];
extern uint8_t  xran_intra_sym_div[];

static int debugStop = 0;
static int debugStopCount = 0;

const long fine_tuning[5][2] =
{
    {71428L, 71429L},  /* mu = 0 */
    {35714L, 35715L},  /* mu = 1 */
    {0, 0},            /* mu = 2 not supported */
    {8928L, 8929L},    /* mu = 3 */
    {0,0  }            /* mu = 4 not supported */
};

const uint8_t slots_per_subframe[5] =
{
    1,  /* mu = 0 */
    2,  /* mu = 1 */
    4,  /* mu = 2 */
    8,  /* mu = 3 */
    1,
};

extern inline uint64_t xran_tick(void);
extern inline unsigned long get_ticks_diff(unsigned long curr_tick, unsigned long last_tick);

int timing_set_debug_stop(int value, int count)
{
    debugStop = value;
    debugStopCount = count;

    if(debugStop)
    {
        clock_gettime(CLOCK_REALTIME, &xran_timerCtx.started_time);
    }
    return debugStop;
}

int timing_get_debug_stop(void)
{
    return debugStop;
}


extern inline struct xran_timing_source_ctx *xran_timingsource_get_ctx(void);
extern inline uint64_t xran_timingsource_get_current_second(void);
extern inline uint8_t xran_timingsource_get_numerology(void);
extern inline uint32_t xran_timingsource_get_max_ota_sym_idx(uint8_t numerlogy);
extern inline uint32_t xran_timingsource_get_coreid(void);
extern inline enum xran_tmthread_state xran_timingsource_get_state(void);
extern inline void xran_timingsource_set_state(enum xran_tmthread_state state);

int xran_timingsource_set_numerology(uint8_t value)
{
    xran_timingsource_get_ctx()->timerMu = value;
    return (xran_timingsource_get_ctx()->timerMu);
}

int xran_timingsource_set_gpsoffset(int64_t offset_sec, int64_t offset_nsec)
{
    if(xran_timerCtx.offset_sec || xran_timerCtx.offset_nsec)
    {
        printf("GPS offset is already set! (%ld.%ld)\n", 
            xran_timerCtx.offset_sec, xran_timerCtx.offset_nsec);
        return (XRAN_STATUS_FAIL);
    }

    xran_timerCtx.offset_sec    = offset_sec;
    xran_timerCtx.offset_nsec   = offset_nsec;

    return (XRAN_STATUS_SUCCESS);
}

void xran_timingsource_adj_gpssecond(struct timespec* p_time)
{
    if(p_time->tv_nsec >= xran_timerCtx.offset_nsec)
    {
        p_time->tv_nsec -= xran_timerCtx.offset_nsec;
        p_time->tv_sec -= xran_timerCtx.offset_sec;
    }
    else
    {
        p_time->tv_nsec += 1e9 - xran_timerCtx.offset_nsec;
        p_time->tv_sec -= xran_timerCtx.offset_sec + 1;
    }

    return;
}


int32_t xran_timingsource_reg_tticb(__attribute__((unused)) void *pHandle, xran_fh_tti_callback_fn Cb, void *cbParam, int skipTtiNum, enum callback_to_phy_id id)
{
    struct xran_timing_source_ctx *pTmCtx = xran_timingsource_get_ctx();

    if(xran_get_if_state() == XRAN_RUNNING)
    {
        print_err("Cannot register callback while running!!\n");
        return (-1);
    }

    pTmCtx->ttiCb[id]       = Cb;
    pTmCtx->TtiCbParam[id]  = cbParam;
    pTmCtx->SkipTti[id]     = skipTtiNum;

    return 0;
}


long xran_timingsource_poll_next_tick(long interval_ns, unsigned long *used_tick)
{
    extern uint16_t xran_getSfnSecStart(void);

    struct xran_ethdi_ctx *p_eth = xran_ethdi_get_ctx();
    struct xran_io_cfg *p_io_cfg = &(p_eth->io_cfg);
    struct xran_timing_source_ctx *pTmCtx;
    struct timespec *p_temp_time;

    long target_time, sym_start_time;
    long delta, tm_threshold_high, tm_threshold_low;    // Update tm threhsolds
    static bool firstCall = false;
    static long sym_acc = 0;
    static long sym_cnt = 0;
    int16_t i=0, j;
    uint8_t timerMu;

    // printf("interval_ns = %ld\n\n",interval_ns);

    p_eth       = xran_ethdi_get_ctx();
    p_io_cfg    = &(p_eth->io_cfg);
    pTmCtx      = xran_timingsource_get_ctx();

    if(firstCall == false)
    {
        clock_gettime(CLOCK_REALTIME, p_last_time);
        pTmCtx->last_tick = MLogTick();

        if(unlikely(pTmCtx->offset_sec || pTmCtx->offset_nsec))
        {
            xran_timingsource_adj_gpssecond(p_last_time);
        }

        pTmCtx->current_second = p_last_time->tv_sec;
        firstCall = true;
    }

    target_time = (p_last_time->tv_sec * NSEC_PER_SEC + p_last_time->tv_nsec + interval_ns);
    /* Start symbol boundary for current symbol */
    sym_start_time = p_last_time->tv_sec * NSEC_PER_SEC + p_last_time->tv_nsec;

    timerMu = pTmCtx->timerMu;

    while(1)
    {
        clock_gettime(CLOCK_REALTIME, p_cur_time);

        pTmCtx->curr_tick = MLogTick();
        if(unlikely(pTmCtx->offset_sec || pTmCtx->offset_nsec))
        {
            xran_timingsource_adj_gpssecond(p_cur_time);
        }

        delta = (p_cur_time->tv_sec * NSEC_PER_SEC + p_cur_time->tv_nsec) - target_time;

        tm_threshold_high   = interval_ns * N_SYM_PER_SLOT * 2; // 2 slots
        tm_threshold_low    = interval_ns * 2;  // 2 symbols

        //add tm exception handling
        if(unlikely(labs(delta) > tm_threshold_low))
        {
            print_dbg("Polling exceeds 2 symbols threshold with delta:%ld(ns), used_tick:%ld(tick)\n",
                    delta, *used_tick);

            pTmCtx->timer_missed_sym++;
            if(unlikely(labs(delta) > tm_threshold_high))
            {
                print_dbg("Polling exceeds 2 slots threshold, stop xran! delta:%ld(ns), used_tick:%ld(tick) \n",
                        delta,*used_tick);
                pTmCtx->timer_missed_slot++;
            }
        }

        if((delta > 0) || (delta < 0 && labs(delta) < THRESHOLD))
        {
            /* Debug stop works only when RU0 IS ACTIVE */
            if(debugStop &&(debugStopCount > 0)
                && (xran_dev_get_ctx_by_id(0)->fh_counters.tx_counter >= debugStopCount))
            {
                uint64_t t1;
                printf("STOP:[%ld.%09ld], debugStopCount %d, tx_counter %ld\n",
                        p_cur_time->tv_sec, p_cur_time->tv_nsec, debugStopCount,
                        xran_dev_get_ctx_by_id(0)->fh_counters.tx_counter);

                t1 = MLogTick();
                rte_pause();
                MLogTask(PID_TIME_SYSTIME_STOP, t1, MLogTick());
                xran_if_current_state = XRAN_STOPPED;
                xran_timingsource_set_state(XRAN_TMTHREAD_STAT_STOP);
            }

            if(pTmCtx->current_second != p_cur_time->tv_sec)
            {
                pTmCtx->current_second = p_cur_time->tv_sec;

                xran_updateSfnSecStart();

                for (i=0; i < XRAN_MAX_NUM_MU; i++)
                {
                    xran_lib_ota_sym_mu[i]      = 0;
                    xran_lib_ota_sym_idx_mu[i]  = 0;
                    for(j=0; j < XRAN_PORTS_NUM; j++)
                        xran_lib_ota_tti_mu[j][i]      = 0;
                    xran_lib_ota_tti_base       = 0;
                }
                sym_cnt = sym_acc = 0;
                print_dbg("ToS:C Sync timestamp: [%ld.%09ld]\n", p_cur_time->tv_sec, p_cur_time->tv_nsec);

                if(debugStop)
                {
                    if(p_cur_time->tv_sec > pTmCtx->started_time.tv_sec && ((p_cur_time->tv_sec % SEC_MOD_STOP) == 0))
                    {
                        uint64_t t1;
                        uint32_t tti         = xran_lib_ota_tti_base;
                        uint32_t slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME(interval_us));
                        uint32_t subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME(interval_us),  SUBFRAMES_PER_SYSTEMFRAME);
                        uint32_t frame_id    = XranGetFrameNum(tti, xran_getSfnSecStart(),
                                                SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval_us));

                        printf("STOP:[%ld.%09ld] (%d : %d : %d)\n",
                                p_cur_time->tv_sec, p_cur_time->tv_nsec,frame_id, subframe_id, slot_id);

                        t1 = MLogTick();
                        rte_pause();
                        MLogTask(PID_TIME_SYSTIME_STOP, t1, MLogTick());
                        xran_if_current_state = XRAN_STOPPED;
                        xran_timingsource_set_state(XRAN_TMTHREAD_STAT_STOP);
                    }
                }

                p_cur_time->tv_nsec = 0; // adjust to 1pps
            } /* second changed */
            else
            {
                /*This is the start of next symbol. resetting the intra sym counter to 0 before start of next OTA sym*/
                for (i = 0; i <= timerMu; i++)
                    xran_intra_sym_div[i] = 0;

                xran_lib_ota_sym_idx_mu[timerMu] = XranIncrementSymIdx(xran_lib_ota_sym_idx_mu[timerMu],
                                                        XRAN_NUM_OF_SYMBOL_PER_SLOT*slots_per_subframe[timerMu]);

                /* timerMu is highest configured numerology. Update the rest */
                for (i = timerMu - 1; i >= 0; i--)
                    xran_lib_ota_sym_idx_mu[i] = xran_lib_ota_sym_idx_mu[timerMu] >> (timerMu - i);

                xran_lib_ota_sym_idx_mu[XRAN_NBIOT_MU] = xran_lib_ota_sym_idx_mu[0];      /*mu kept for NB-IOT*/

                /* adjust to sym boundary */
                if(sym_cnt & 1)
                    sym_acc +=  fine_tuning[timerMu][0];
                else
                    sym_acc +=  fine_tuning[timerMu][1];

                /* fine tune to second boundary */
                if(sym_cnt % 13 == 0)
                    sym_acc += 1;

                p_cur_time->tv_nsec = sym_acc;
                sym_cnt++;
            }

#ifdef USE_PTP_TIME
            if(debugStop && delta < interval_ns*10)
                MLogTask(PID_TIME_SYSTIME_POLL, (p_last_time->tv_sec * NSEC_PER_SEC + p_last_time->tv_nsec), (p_cur_time->tv_sec * NSEC_PER_SEC + p_cur_time->tv_nsec));
#else
            MLogXRANTask(PID_TIME_SYSTIME_POLL, pTmCtx->last_tick, pTmCtx->curr_tick);
    	    MLogSetTaskCoreMap(TASK_3104);
            pTmCtx->last_tick = pTmCtx->curr_tick;
#endif
            p_temp_time = p_last_time;
            p_last_time = p_cur_time;
            p_cur_time  = p_temp_time;
            break;
        }
        else
        {
            if(likely((xran_if_current_state == XRAN_RUNNING) || (xran_if_current_state == XRAN_OWDM)))
            {
                uint64_t t1, t2;
                t1 = xran_tick();

                if(p_eth->time_wrk_cfg.f)
                    p_eth->time_wrk_cfg.f(p_eth->time_wrk_cfg.arg);

                if(p_io_cfg->io_sleep)
                    nanosleep(&pTmCtx->sleeptime,NULL);

                /*update the counter xran_intra_sym_div[] if the fraction of time for symbol division has passed*/
                if((p_cur_time->tv_sec * NSEC_PER_SEC + p_cur_time->tv_nsec - sym_start_time) >= (interval_ns/XRAN_INTRA_SYM_MAX_DIV)*(xran_intra_sym_div[timerMu] + 1))
                {
                    xran_intra_sym_div[timerMu]++;
                    for(i = timerMu - 1; i >= 0; i--)
                        xran_intra_sym_div[i] = xran_intra_sym_div[timerMu] >> (timerMu - i);  
                }
                t2 = xran_tick();                
                *used_tick += get_ticks_diff(t2, t1);
            }
        }
    }

    return delta;
}

long xran_timingsource_sleep_next_tick(long interval)
{
   struct timespec start_time;
   struct timespec cur_time;
   //struct timespec target_time_convert;
   struct timespec sleep_target_time_convert;
   long target_time;
   long sleep_target_time;
   long delta;

   clock_gettime(CLOCK_REALTIME, &start_time);
   target_time = (start_time.tv_sec * NSEC_PER_SEC + start_time.tv_nsec + interval * NSEC_PER_USEC) / (interval * NSEC_PER_USEC) * interval;
   //printf("target: %ld, current: %ld, %ld\n", target_time, start_time.tv_sec, start_time.tv_nsec);
   sleep_target_time = target_time - TIMECOMPENSATION;
   sleep_target_time_convert.tv_sec = sleep_target_time * NSEC_PER_USEC / NSEC_PER_SEC;
   sleep_target_time_convert.tv_nsec = (sleep_target_time * NSEC_PER_USEC) % NSEC_PER_SEC;

   //target_time_convert.tv_sec = target_time * NSEC_PER_USEC / NSEC_PER_SEC;
   //target_time_convert.tv_nsec = (target_time * NSEC_PER_USEC) % NSEC_PER_SEC;

   clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &sleep_target_time_convert, NULL);

   clock_gettime(CLOCK_REALTIME, &cur_time);

   delta = (cur_time.tv_sec * NSEC_PER_SEC + cur_time.tv_nsec) - target_time * NSEC_PER_USEC;

   return delta;
}

#ifdef POLL_EBBU_OFFLOAD
int timing_get_debug_stop_count(void)
{
    return debugStopCount;
}

int timing_get_start_second(void)
{
    return started_second;
}
#endif


uint32_t xran_timingsource_get_timestats(uint64_t *total_time, uint64_t *used_time, uint32_t *num_core_used, uint32_t *core_used, uint32_t clear)
{
    uint32_t num;

    num = xran_dev_get_num_usedcores();
    *num_core_used = num;
    rte_memcpy(core_used, xran_dev_get_list_usedcores(), num * (sizeof(uint32_t)));

    *total_time = xran_timerCtx.total_tick;
    *used_time = xran_timerCtx.used_tick;

    if(clear)
    {
        xran_timerCtx.total_tick    = 0;
        xran_timerCtx.used_tick     = 0;
    }

    return 0;
}
uint32_t xran_get_time_stats(uint64_t *total_time, uint64_t *used_time, uint32_t *num_core_used, uint32_t *core_used, uint32_t clear)
{
    return(xran_timingsource_get_timestats(total_time, used_time, num_core_used, core_used, clear));
}


int32_t xran_timingsource_thread(__attribute__((unused)) void *args)
{
#ifndef POLL_EBBU_OFFLOAD
    cpu_set_t cpuset;
    int32_t   result;
    struct sched_param sched_param;
    uint8_t mu=0, i=0, j=0;
    uint32_t xran_port_id = 0;
    static int owdm_init_done = 0;
    uint64_t tWake = 0, tWakePrev = 0, tUsed = 0;
    int64_t delta;
    struct xran_device_ctx * p_dev_ctx_run = NULL;
    struct timespec ts;
    char thread_name[32];
    char buff[100];
    struct xran_io_cfg *ioCfg;
    int appMode;
    struct xran_timing_source_ctx *pTmCtx;

    ioCfg   = xran_get_sysiocfg();
    appMode = xran_get_syscfg_appmode();
    pTmCtx  = xran_timingsource_get_ctx();

    printf("%s [CPU %2d] [PID: %6d]\n", __FUNCTION__,  rte_lcore_id(), getpid());

    /* set affinity and scheduling parameters for timing thread */
    memset(&sched_param, 0, sizeof(struct sched_param));
    sched_param.sched_priority = XRAN_THREAD_DEFAULT_PRIO;
    CPU_ZERO(&cpuset);
    CPU_SET(ioCfg->timing_core, &cpuset);

    if((result = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset)))
        printf("pthread_setaffinity_np failed: result = %d\n",result);

    if((result = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched_param)))
        printf("priority is not changed: result = %d\n",result);

    snprintf(thread_name, RTE_DIM(thread_name), "%s-%d", "fh_main_poll", rte_lcore_id());
    if((result = pthread_setname_np(pthread_self(), thread_name)))
        printf("[core %d] pthread_setname_np = %d\n",rte_lcore_id(), result);

    printf("Initial TTI interval : %ld [us]\n", interval_us);

    /* Synchronize with ToS */
    do {
        timespec_get(&ts, TIME_UTC);
    } while (ts.tv_nsec >1500);

    struct tm *ptm = gmtime(&ts.tv_sec);
    if(ptm)
    {
        strftime(buff, sizeof buff, "%D %T", ptm);
        printf("%s: thread_run start time: %s.%09ld UTC [%ld]\n",
                (appMode == O_DU ? "O-DU": "O-RU"), buff, ts.tv_nsec, interval_us);
    }

    do {
       timespec_get(&ts, TIME_UTC);
    } while (ts.tv_nsec == 0);

    xran_timingsource_set_state(XRAN_TMTHREAD_STAT_RUN);

    while(xran_timingsource_get_state() == XRAN_TMTHREAD_STAT_RUN)
    {
        /* Update Usage Stats */
        tWake = xran_tick();
        pTmCtx->used_tick += tUsed;
        if(tWakePrev)
            pTmCtx->total_tick += get_ticks_diff(tWake, tWakePrev);
        tWakePrev = tWake;

        tUsed = 0;
        delta = xran_timingsource_poll_next_tick(interval_us*1000L/N_SYM_PER_SLOT, &tUsed);
        if(delta > 3E5 && tUsed > 0)    //300us about 9 symbols
            print_err("xran_timingsource_poll_next_tick too long, delta:%ld(ns), tUsed:%ld(tick)", delta, tUsed);

        if(likely(XRAN_RUNNING == xran_if_current_state))
        {
            /* TTI callback */
            mu = xran_timingsource_get_numerology();       /* Numerology for TTI interval */
            if((XranGetSymNum(xran_lib_ota_sym_idx_mu[mu], XRAN_NUM_OF_SYMBOL_PER_SLOT) == xran_lib_ota_sym_mu[mu])
                && (XranGetSymNum(xran_lib_ota_sym_idx_mu[mu], XRAN_NUM_OF_SYMBOL_PER_SLOT) == 0))
            {
                long t3 = xran_tick();
                tti_ota_cb(NULL, mu);
                tUsed += get_ticks_diff(xran_tick(), t3);
            }

            for(xran_port_id =  0; xran_port_id < XRAN_PORTS_NUM; xran_port_id++)
            {
                if(!xran_isactive_ru_byid(xran_port_id))
                    continue;

                p_dev_ctx_run = xran_dev_get_ctx_by_id(xran_port_id);
                if(p_dev_ctx_run)
                {
                    if(unlikely(xran_get_numactiveccs_ru(p_dev_ctx_run) == 0))
                        continue;
                    if(likely(p_dev_ctx_run->xran_port_id == xran_port_id))
                    {
                        /* Check if owdm finished to create the timing cbs based on measurement results */
                        if((ioCfg->eowd_cmn[appMode].owdm_enable) && (!owdm_init_done))
                        {
                            // Adjust Windows based on Delay Measurement results
                            xran_adjust_timing_parameters(p_dev_ctx_run);
                            if((result = xran_timing_create_cbs((void *)p_dev_ctx_run)) < 0)
                            {
                                print_err("Failed to create timing callbacks! (%d)", result);
                                return (result);
                            }
                            owdm_init_done = 1;
                        }

                        for(j=0;j<p_dev_ctx_run->fh_cfg.numMUs;j++)
                        {
                            mu = p_dev_ctx_run->fh_cfg.mu_number[j];
                            if(XranGetSymNum(xran_lib_ota_sym_idx_mu[mu], XRAN_NUM_OF_SYMBOL_PER_SLOT) == xran_lib_ota_sym_mu[mu])
                            {
                                sym_ota_cb(p_dev_ctx_run, &tUsed, mu);
                            }

                            struct xran_common_counters *pCnt = &p_dev_ctx_run->fh_counters;
                            uint64_t current_sec = xran_timingsource_get_current_second();
                            if(pCnt->gps_second != current_sec)
                            {
                                if((current_sec - pCnt->gps_second) != 1)
                                    print_dbg("second c %ld p %ld\n", current_sec, pCnt->gps_second);

                                pCnt->gps_second = (uint64_t)current_sec;

                                pCnt->rx_counter_pps    = pCnt->rx_counter - pCnt->old_rx_counter;
                                pCnt->old_rx_counter    = pCnt->rx_counter;
                                pCnt->tx_counter_pps    = pCnt->tx_counter - pCnt->old_tx_counter;
                                pCnt->old_tx_counter    = pCnt->tx_counter;
                                pCnt->rx_bytes_per_sec  = pCnt->rx_bytes_counter;
                                pCnt->tx_bytes_per_sec  = pCnt->tx_bytes_counter;
                                pCnt->rx_bytes_counter  = 0;
                                pCnt->tx_bytes_counter  = 0;
                                pCnt->rx_bits_per_sec   = pCnt->rx_bytes_per_sec * 8 / 1000L;
                                pCnt->tx_bits_per_sec   = pCnt->tx_bytes_per_sec * 8 / 1000L;
                                print_dbg("current_sec %lu\n", current_sec);
                            }
                        }
                    }
                    else
                    {
                        rte_panic("p_dev_ctx_run == xran_port_id");
                    }
                }
            }

            for(i=0;i<XRAN_MAX_NUM_MU;i++)
            {
                /* All numerologies for all RUs are processed now.
                 * If current symbol for a given mu (xran_lib_ota_sym_mu) matches symbol_idx%14 then that means
                 * processing for that symbol has completed above and we can increment 'current symbol' (i.e. xran_lib_ota_sym_mu)
                 * If it doesn't then processing for xran_lib_ota_sym_mu hasn't happenned yet and we don't increment it.
                 */
                if(XranGetSymNum(xran_lib_ota_sym_idx_mu[i], XRAN_NUM_OF_SYMBOL_PER_SLOT) == xran_lib_ota_sym_mu[i])
                {
                    xran_lib_ota_sym_mu[i]++;
                    if(xran_lib_ota_sym_mu[i] >= N_SYM_PER_SLOT)
                    {
                        xran_lib_ota_sym_mu[i] = 0;
                    }
                }
            }
        }
    }   /* while(xran_timingsource_get_state() == XRAN_TMTHREAD_STAT_RUN) */

    xran_timingsource_set_state(XRAN_TMTHREAD_STAT_EXIT);

    printf("Closing timing source thread...\n");
    return (0);

#else
    /* POLL_EBBU_OFFLOAD */
    cpu_set_t cpuset;
    int32_t   result;
    struct sched_param sched_param;
    struct timespec ts;
    char thread_name[32];
    char buff[100];
    struct xran_io_cfg *ioCfg;
    int appMode;

    printf("%s [CPU %2d] [PID: %6d]\n", __FUNCTION__,  sched_getcpu(), getpid());
    PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();
    if(pCtx->pFn == NULL)
    {
        print_err("EBBUPool polling processing feature enabled but eBBUPool polling event generation function unregistered\n");
        return -1;
    }

    ioCfg = xran_get_sysiocfg();
    appMode = xran_get_syscfg_appmode();

    /* set affinity and scheduling parameters for timing thread */
    memset(&sched_param, 0, sizeof(struct sched_param));
    sched_param.sched_priority = XRAN_THREAD_DEFAULT_PRIO;
    CPU_ZERO(&cpuset);
    CPU_SET(ioCfg->timing_core, &cpuset);

    if ((result = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset)))
        printf("pthread_setaffinity_np failed: result = %d\n",result);

    if ((result = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched_param)))
        printf("priority is not changed: result = %d\n",result);

    snprintf(thread_name, RTE_DIM(thread_name), "%s-%d", "fh_main_poll_ebbu_offload", sched_getcpu());
    if ((res = pthread_setname_np(pthread_self(), thread_name)))
        printf("[core %d] pthread_setname_np = %d\n",sched_getcpu(), res);

    printf("Initial TTI interval : %ld [us]\n", interval_us);

    /* Synchronize with ToS */
    do {
        timespec_get(&ts, TIME_UTC);
    } while (ts.tv_nsec >1500);

    struct tm * ptm = gmtime(&ts.tv_sec);
    if(ptm)
    {
        strftime(buff, sizeof buff, "%D %T", ptm);
        printf("%s: thread_run start time: %s.%09ld UTC [%ld]\n",
                (appMode == O_DU ? "O-DU": "O-RU"), buff, ts.tv_nsec, interval_us);
    }

    do {
       timespec_get(&ts, TIME_UTC);
    } while (ts.tv_nsec == 0);

    xran_timingsource_set_state(XRAN_TMTHREAD_STAT_RUN);

    pCtx->pFn(0, TMNG_SYM_POLL, 5, NULL);    /* Generate task TMNG_SYM_POLL with 5 us delay */
    pCtx->pFn(0, TMNG_PKT_POLL, 2, NULL);    /* Generate task TMNG_PKT_POLL with 2 us delay */
    printf("xran_timingsource_thread exiting [core: %d]\n", sched_getcpu());

    pthread_detach(pthread_self());
    return (0);
#endif
}

int32_t xran_timingsource_start(void)
{
    extern int32_t first_call;

    struct xran_io_cfg *ioCfg;
    int wait_time = 0, i, j;

    if(xran_timingsource_get_state() == XRAN_TMTHREAD_STAT_RUN)
    {
        printf("Timing Thread is Running already!\n");
        return XRAN_STATUS_FAIL;
    }

    xran_if_current_state = XRAN_INIT;
    xran_dev_init_num_usedcores();
    first_call = 0;

    for (i=0; i< XRAN_MAX_NUM_MU ; i++)
    {
        xran_lib_ota_sym_mu[i]      = 0;
        xran_lib_ota_sym_idx_mu[i]  = 0;
        for(j=0; j < XRAN_PORTS_NUM; j++)
            xran_lib_ota_tti_mu[j][i]   = 0;
    }
    xran_lib_ota_tti_base       = 0;

    ioCfg = xran_get_sysiocfg();
    if((uint16_t)ioCfg->port[XRAN_UP_VF] != 0xFFFF)
    {
        printf("XRAN_UP_VF: 0x%04x\n", ioCfg->port[XRAN_UP_VF]);
        xran_dev_add_usedcore(ioCfg->timing_core);
    
#ifdef POLL_EBBU_OFFLOAD
        pthread_t xranThread;
        if(pthread_create(&xranThread, NULL, (void *)xran_timingsource_thread, NULL))
#else
        if(rte_eal_remote_launch(xran_timingsource_thread, NULL, ioCfg->timing_core))
#endif
            rte_panic("thread_run() failed to start\n");
    }
    else
    {
        print_err("Eth port was not open. Processing thread was not started\n");
        return XRAN_STATUS_FAIL;
    }

    // Make sure xRAN timing source thread is active before proceeding
    while(xran_timingsource_get_state() != XRAN_TMTHREAD_STAT_RUN)
    {
        printf("Waiting for timing thread to be active ....%d\n", wait_time);
        ++wait_time;
        sleep(1);

        if(wait_time > 10)
        {
            print_err("\nTiming thread not yet started\n");
            return XRAN_STATUS_FAIL;
        }
    }
    printf("\n*** Timer thread active ***\n");

    return XRAN_STATUS_SUCCESS;
}

int32_t xran_timingsource_stop(void)
{
    uint32_t loopCnt;

    if(xran_timingsource_get_state() != XRAN_TMTHREAD_STAT_RUN)
    {
        printf("Timing Source Thread is not running! %d\n", xran_timingsource_get_state());
        return XRAN_STATUS_FAIL;
    }

    xran_timingsource_set_state(XRAN_TMTHREAD_STAT_STOP);

#ifdef POLL_EBBU_OFFLOAD
    xran_timingsource_set_state(XRAN_TMTHREAD_STAT_EXIT);
#endif

    for(loopCnt=0; loopCnt < 10000; loopCnt++)
    {
        if(xran_timingsource_get_state()==XRAN_TMTHREAD_STAT_EXIT)
        {
            printf("ORAN Timing Source Thread STOPPED.. [%d]\n", loopCnt);
            return XRAN_STATUS_SUCCESS;
        }
        usleep(100);
    }

    printf("Timeout from stopping ORAN Timing Source Thread.. [%d]\n", loopCnt);
    return XRAN_STATUS_FAIL;
}
