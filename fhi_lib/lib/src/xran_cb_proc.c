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
 * @brief XRAN Callback processing functionality and helper functions
 * @file xran_cb_proc.c
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#include <unistd.h>
#include <stdio.h>
#include <immintrin.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_mbuf.h>
#include <rte_timer.h>

#include "xran_ethdi.h"
#include "xran_fh_o_du.h"
#include "xran_main.h"
#include "xran_dev.h"
#include "xran_common.h"
#include "xran_cb_proc.h"
#include "xran_mlog_lnx.h"
#include "xran_lib_mlog_tasks_id.h"
#include "xran_printf.h"
#include "xran_frame_struct.h"

typedef void (*rx_dpdk_sym_cb_fn)(struct rte_timer *tim, void *arg);


void xran_timer_arm_cp_dl(struct rte_timer *tim, void* arg, void *p_dev_ctx, uint8_t mu)
{
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)p_dev_ctx;
    uint64_t t3 = MLogXRANTick();

    if (xran_if_current_state == XRAN_RUNNING){
#ifdef POLL_EBBU_OFFLOAD
        PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();
        pCtx->pFn(p_xran_dev_ctx->xran_port_id, TMNG_DL_CP_POLL, 0, &(p_xran_dev_ctx->perMu[mu]));    /* Generate Task TMNG_DL_CP_POLL with no delay*/
#else
        unsigned tim_lcore = xran_schedule_to_worker(XRAN_JOB_TYPE_CP_DL, p_xran_dev_ctx);
        rte_timer_cb_t fct = (rte_timer_cb_t)arg;
        rte_timer_reset_sync(tim, 0, SINGLE, tim_lcore, fct, &(p_xran_dev_ctx->perMu[mu]));
#endif
    }
    MLogXRANTask(PID_TIME_ARM_TIMER, t3, MLogXRANTick());
}

void xran_timer_arm_cp_ul(struct rte_timer *tim, void* arg, void *p_dev_ctx, uint8_t mu)
{
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)p_dev_ctx;
    uint64_t t3 = MLogXRANTick();

    if (xran_if_current_state == XRAN_RUNNING){
#ifdef POLL_EBBU_OFFLOAD
        PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();
        pCtx->pFn(p_xran_dev_ctx->xran_port_id, TMNG_UL_CP_POLL, 0, &(p_xran_dev_ctx->perMu[mu]));    /* Generate Task TMNG_UL_CP_POLL with no delay*/
#else
        unsigned tim_lcore = xran_schedule_to_worker(XRAN_JOB_TYPE_CP_UL, p_xran_dev_ctx);
        rte_timer_cb_t fct = (rte_timer_cb_t)arg;
        rte_timer_reset_sync(tim, 0, SINGLE, tim_lcore, fct, &(p_xran_dev_ctx->perMu[mu]));
#endif
    }
    MLogXRANTask(PID_TIME_ARM_TIMER, t3, MLogXRANTick());
}

void xran_timer_arm_for_deadline(struct rte_timer *tim, void* arg,  void *p_dev_ctx, uint8_t mu)
{
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)p_dev_ctx;
    uint64_t t3 = MLogXRANTick();

    int32_t rx_tti;
    uint32_t nFrameIdx;
    uint32_t nSubframeIdx;
    uint32_t nSlotIdx;
    uint64_t nSecond;

    xran_get_slot_idx(p_xran_dev_ctx->xran_port_id, &nFrameIdx, &nSubframeIdx, &nSlotIdx, &nSecond, mu);

    rx_tti = nFrameIdx*SUBFRAMES_PER_SYSTEMFRAME*SLOTNUM_PER_SUBFRAME(xran_fs_get_tti_interval(mu))
           + nSubframeIdx*SLOTNUM_PER_SUBFRAME(xran_fs_get_tti_interval(mu))
           + nSlotIdx;

    p_xran_dev_ctx->perMu[mu].cb_timer_ctx[p_xran_dev_ctx->perMu[mu].timer_put %  MAX_CB_TIMER_CTX].tti_to_process = rx_tti;
    p_xran_dev_ctx->perMu[mu].timer_get = p_xran_dev_ctx->perMu[mu].timer_put;
    p_xran_dev_ctx->perMu[mu].timer_put = (p_xran_dev_ctx->perMu[mu].timer_put + 1) %  MAX_CB_TIMER_CTX;
    if (xran_if_current_state == XRAN_RUNNING){
#ifdef POLL_EBBU_OFFLOAD
        ((XranSymCb)(arg))(NULL, &(p_xran_dev_ctx->perMu[mu]));
#else
        rte_timer_cb_t fct = (rte_timer_cb_t)arg;
        unsigned tim_lcore = xran_schedule_to_worker(XRAN_JOB_TYPE_DEADLINE, p_xran_dev_ctx);
        rte_timer_reset_sync(tim, 0, SINGLE, tim_lcore, fct, &(p_xran_dev_ctx->perMu[mu]));
#endif
    }

    MLogXRANTask(PID_TIME_ARM_TIMER_DEADLINE, t3, MLogXRANTick());
}

void xran_timer_arm_user_cb(struct rte_timer *tim, void* arg,  void *p_ctx, uint8_t mu)
{
    /* p_ctx/p_sym_cb_ctx is allocated per numerology in xran_reg_sym_cb. Hence we don't need to have
     * per numerology internal elements
     */
    struct cb_user_per_sym_ctx* p_sym_cb_ctx = (struct cb_user_per_sym_ctx *)p_ctx;
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)p_sym_cb_ctx->p_dev;
    uint64_t t3 = MLogXRANTick();

    unsigned tim_lcore = xran_schedule_to_worker(XRAN_JOB_TYPE_SYM_CB, NULL);

    int32_t rx_tti;
    uint32_t nFrameIdx = 0;
    uint32_t nSubframeIdx = 0;
    uint32_t nSlotIdx = 0;
    uint64_t nSecond = 0;

    xran_get_slot_idx(p_xran_dev_ctx->xran_port_id, &nFrameIdx, &nSubframeIdx, &nSlotIdx, &nSecond, mu);

    rx_tti = nFrameIdx*SUBFRAMES_PER_SYSTEMFRAME*SLOTNUM_PER_SUBFRAME(xran_fs_get_tti_interval(mu))
           + nSubframeIdx*SLOTNUM_PER_SUBFRAME(xran_fs_get_tti_interval(mu))
           + nSlotIdx;

    p_sym_cb_ctx->user_cb_timer_ctx[p_sym_cb_ctx->user_timer_put % MAX_CB_TIMER_CTX].tti_to_process = rx_tti;
    p_sym_cb_ctx->user_cb_timer_ctx[p_sym_cb_ctx->user_timer_put % MAX_CB_TIMER_CTX].ota_sym_idx = xran_lib_ota_sym_idx_mu[mu];
    p_sym_cb_ctx->user_cb_timer_ctx[p_sym_cb_ctx->user_timer_put % MAX_CB_TIMER_CTX].xran_sfn_at_sec_start = xran_getSfnSecStart();
    p_sym_cb_ctx->user_cb_timer_ctx[p_sym_cb_ctx->user_timer_put % MAX_CB_TIMER_CTX].current_second = nSecond;

    if (xran_if_current_state == XRAN_RUNNING){
        rte_timer_cb_t fct = (rte_timer_cb_t)arg;
        rte_timer_reset_sync(tim, 0, SINGLE, tim_lcore, fct, p_sym_cb_ctx);
        if (++p_sym_cb_ctx->user_timer_put >= MAX_CB_TIMER_CTX)
            p_sym_cb_ctx->user_timer_put = 0;
    }

    MLogXRANTask(PID_TIME_ARM_USER_TIMER_DEADLINE, t3, MLogXRANTick());
}

void xran_timer_arm_ex(struct rte_timer *tim, void* CbFct, void *CbArg, unsigned tim_lcore)
{
    uint64_t t3 = MLogXRANTick();

    if (xran_if_current_state == XRAN_RUNNING){
        rte_timer_cb_t fct = (rte_timer_cb_t)CbFct;
        rte_timer_reset_sync(tim, 0, SINGLE, tim_lcore, fct, CbArg);
    }
    MLogXRANTask(PID_TIME_ARM_TIMER, t3, MLogXRANTick());
}

void xran_remove_cb_list(struct sym_cb_elem_list* list_head)
{
    struct cb_elem_entry *cb_elm;
    struct cb_elem_entry *cb_elm_to_destroy = NULL;
    LIST_FOREACH(cb_elm, list_head, pointers)
    {
        if(cb_elm_to_destroy)   // destroy previously traversed element
        {
            xran_destroy_cb(cb_elm_to_destroy);
        }

        // remove currently traversed element from list - will be destroyed in next loop pass
        if(cb_elm)
        {
            LIST_REMOVE(cb_elm, pointers);
            cb_elm_to_destroy = cb_elm;
        }
    }

    // destroy last element
    if(cb_elm_to_destroy)
    {
        xran_destroy_cb(cb_elm_to_destroy);
    }
}

int32_t xran_timing_create_cbs(void *args)
{
    uint32_t delay_cp_dl_max, delay_cp_dl_min;
    uint32_t delay_cp_ul;
    uint32_t delay_up;
    uint32_t time_diff_us;
    uint32_t delay_cp2up;
    uint32_t sym_cp_dl_max, sym_cp_dl_min;
    uint32_t sym_cp_ul;
    uint32_t time_diff_nSymb;
    uint32_t lower_bound_window;
    uint32_t min_dl_delay_offset, max_dl_delay_offset;
    int32_t  sym_up;
    struct xran_device_ctx *p_dev_ctx =  (struct xran_device_ctx *)args;
    struct cb_elem_entry * cb_elm = NULL;
    uint32_t interval_us_local, ul_delay_offset;
    uint8_t ru_id, mu, numSlots, max_dl_offset_sym, min_dl_offset_sym, ul_offset_sym;
    /* ToS = Top of Second start +- 1.5us */
    struct timespec ts;
    char buff[100];

#ifdef POLL_EBBU_OFFLOAD
    uint32_t delay_up_min;
    int32_t sym_up_min;
#endif

    ru_id = p_dev_ctx->xran_port_id;

    for(int32_t i = 0; i < p_dev_ctx->fh_cfg.numMUs; i++)
    {
        mu = p_dev_ctx->fh_cfg.mu_number[i];
        interval_us_local = xran_fs_get_tti_interval(mu);

        if(xran_get_syscfg_appmode() == O_DU)
        {
            delay_cp_dl_max = interval_us_local - p_dev_ctx->fh_cfg.perMu[mu].T1a_max_cp_dl;
            delay_cp_dl_min = interval_us_local - p_dev_ctx->fh_cfg.perMu[mu].T1a_min_cp_dl;
            delay_cp_ul = interval_us_local - p_dev_ctx->fh_cfg.perMu[mu].T1a_max_cp_ul;

            numSlots = 0; /* How many slots you need to go backwards from OTA */
            max_dl_delay_offset = interval_us_local; /* Start of the slot in which you will start CP DL */

            while(p_dev_ctx->fh_cfg.perMu[mu].T1a_max_cp_dl > max_dl_delay_offset)
            {
                max_dl_delay_offset += interval_us_local;
                numSlots++;
            }

            /* Delay from start of 'a' slot */
            delay_cp_dl_max = max_dl_delay_offset - p_dev_ctx->fh_cfg.perMu[mu].T1a_max_cp_dl;

            /* Symbol on which we will start CP transmission */
            sym_cp_dl_max = delay_cp_dl_max*1000/(interval_us_local*1000/N_SYM_PER_SLOT)+1;

            /* Backward offset from OTA in terms of symbols when Cp transmission will start
             * i.e. cp transmission will start 'max_dl_offset_sym' symbols before OTA
             */
            max_dl_offset_sym = (numSlots+1)*N_SYM_PER_SLOT - sym_cp_dl_max;

            /* Handle corner case of symbol-0*/
            sym_cp_dl_max%=N_SYM_PER_SLOT;

            min_dl_delay_offset = interval_us_local;
            numSlots=0;

            while(p_dev_ctx->fh_cfg.perMu[mu].T1a_min_cp_dl > min_dl_delay_offset)
            {
                min_dl_delay_offset += interval_us_local;
                numSlots++;
            }

            delay_cp_dl_min = min_dl_delay_offset - p_dev_ctx->fh_cfg.perMu[mu].T1a_min_cp_dl;
            sym_cp_dl_min = delay_cp_dl_min*1000/(interval_us_local*1000/N_SYM_PER_SLOT) - 1;
            min_dl_offset_sym   = (numSlots+1)*N_SYM_PER_SLOT - sym_cp_dl_min;
            sym_cp_dl_min%=N_SYM_PER_SLOT;

            ul_delay_offset = interval_us_local;
            numSlots=0;

            while(p_dev_ctx->fh_cfg.perMu[mu].T1a_max_cp_ul > ul_delay_offset)
            {
                ul_delay_offset += interval_us_local;
                numSlots++;
            }

            delay_cp_ul = ul_delay_offset - p_dev_ctx->fh_cfg.perMu[mu].T1a_max_cp_ul;
            sym_cp_ul = (delay_cp_ul*1000/(interval_us_local*1000/N_SYM_PER_SLOT)+1);
            p_dev_ctx->perMu[mu].ulCpSlotOffset = numSlots;
            ul_offset_sym   = (numSlots+1)*N_SYM_PER_SLOT - sym_cp_ul;
            sym_cp_ul%=N_SYM_PER_SLOT;

            printf("RU%d mu%u, delay_cp_dl_max=%u, sym_cp_dl_max=%u, max_dl_offset_sym=%u\n"
                   "         delay_cp_dl_min=%u, sym_cp_dl_min=%u, min_dl_offset_sym=%u\n"
                   "         delay_cp_ul=%u,     sym_cp_ul=%u,     ul_offset_sym=%u\n",
                    ru_id, mu, delay_cp_dl_max, sym_cp_dl_max, max_dl_offset_sym,
                    delay_cp_dl_min, sym_cp_dl_min, min_dl_offset_sym,
                    delay_cp_ul, sym_cp_ul, ul_offset_sym);


            delay_up    = p_dev_ctx->fh_cfg.perMu[mu].T1a_max_up;
            time_diff_us = p_dev_ctx->fh_cfg.perMu[mu].Ta4_max;
            lower_bound_window=(p_dev_ctx->fh_cfg.perMu[mu].Ta3_min>p_dev_ctx->fh_cfg.perMu[mu].Ta4_min?p_dev_ctx->fh_cfg.perMu[mu].Ta3_min:p_dev_ctx->fh_cfg.perMu[mu].Ta4_min);
            delay_cp2up = delay_up-delay_cp_dl_max;

            time_diff_nSymb = time_diff_us*1000/(interval_us_local*1000/N_SYM_PER_SLOT);
            p_dev_ctx->perMu[mu].sym_up       = sym_up = -(delay_up*1000/(interval_us_local*1000/N_SYM_PER_SLOT));
            p_dev_ctx->perMu[mu].sym_up_ul_ub    = time_diff_nSymb = (time_diff_us*1000/(interval_us_local*1000/N_SYM_PER_SLOT)+1);
            p_dev_ctx->perMu[mu].sym_up_ul_lb = (lower_bound_window*1000/(interval_us_local*1000/N_SYM_PER_SLOT)+1);

#ifdef POLL_EBBU_OFFLOAD
            delay_up_min    = p_dev_ctx->fh_cfg.perMu[mu].T1a_min_up;
            sym_up_min      = -(delay_up_min*1000/(interval_us_local*1000/N_SYM_PER_SLOT)+1);
            printf("RU%d mu%u, Start U-plane DL %d us at least before OTA [offset  in sym %d]\n", ru_id, mu, delay_up_min, sym_up_min);
            PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();
            pCtx->sym_up_window = (sym_up_min - sym_up);
#endif

            printf("RU%d mu%u: Start C-plane DL from %d us after TTI  [trigger on sym %d] to %d us after TTI [trigger on sym %d]\n",
                    ru_id, mu, delay_cp_dl_max, sym_cp_dl_max, delay_cp_dl_min, sym_cp_dl_min);
            printf("RU%d mu%u, Start C-plane UL %d us after TTI  [trigger on sym %d]\n", ru_id, mu, delay_cp_ul, sym_cp_ul);
            printf("RU%d mu%u, Start U-plane DL %d us before OTA [offset  in sym %d]\n", ru_id, mu, delay_up, sym_up);
            printf("RU%d mu%u, Start U-plane UL %d us OTA        [offset  in sym %d]\n", ru_id, mu, time_diff_us, time_diff_nSymb);
            printf("RU%d mu%u, C-plane to U-plane delay %d us after TTI\n", ru_id, mu, delay_cp2up);
            printf("Start Sym timer %ld ns\n", TX_TIMER_INTERVAL/N_SYM_PER_SLOT);

            if(0 == p_dev_ctx->dlCpProcBurst && p_dev_ctx->DynamicSectionEna == 0)
            {
                if(max_dl_offset_sym >= min_dl_offset_sym) /* corner case where only 1 symbol is available for transmission */
                    p_dev_ctx->numSymsForDlCP = max_dl_offset_sym - min_dl_offset_sym + 1;
                else
                    p_dev_ctx->numSymsForDlCP = 1;
            }
            else
                p_dev_ctx->numSymsForDlCP = N_SYM_PER_SLOT;

            int count=0;
            if(xran_get_syscfg_bbuoffload())
            {
                p_dev_ctx->perMu[mu].dlCpTxSym = sym_cp_dl_max; /* Cp burst spread logic wont be used with bbu pooling enabled */
                p_dev_ctx->perMu[mu].ulCpTxSym = sym_cp_ul;

                /*Advance transmission factor calculation for downlink packets - enabled/supported in case of BBU offload*/
                p_dev_ctx->perMu[mu].adv_tx_factor  = (p_dev_ctx->fh_cfg.perMu[mu].adv_tx_time) ? (interval_us_local/(p_dev_ctx->fh_cfg.perMu[mu].adv_tx_time * N_SYM_PER_SLOT)):0;
                if(p_dev_ctx->perMu[mu].adv_tx_factor >= XRAN_INTRA_SYM_MAX_DIV)
                    p_dev_ctx->perMu[mu].adv_tx_factor = 0;
                printf("RU%d mu%d, adv_tx_factor=%d\n", ru_id, mu, p_dev_ctx->perMu[mu].adv_tx_factor);
            }
            else
            {
                while(count < p_dev_ctx->numSymsForDlCP)
                {
                    cb_elm = xran_create_cb(xran_timer_arm_cp_dl, tx_cp_dl_cb, (void *) p_dev_ctx);
                    if(cb_elm)
                    {
                        LIST_INSERT_HEAD(&p_dev_ctx->perMu[mu].sym_cb_list_head[sym_cp_dl_max],
                                cb_elm, pointers);
                    }
                    else
                    {
                        print_err ("cb_elm is NULL\n");
                        goto err0;
                    }

                    printf("RU%d mu=%u, created DL CP callback for symbol %u\n", ru_id, mu, sym_cp_dl_max);

                    sym_cp_dl_max = (sym_cp_dl_max+1)%N_SYM_PER_SLOT;
                    max_dl_offset_sym--;
                    count++;
                }

                cb_elm = xran_create_cb(xran_timer_arm_cp_ul, tx_cp_ul_cb, (void*)p_dev_ctx);
                if(cb_elm)
                {
                    LIST_INSERT_HEAD(&p_dev_ctx->perMu[mu].sym_cb_list_head[sym_cp_ul],
                            cb_elm, pointers);
                }
                else
                {
                    print_err("cb_elm is NULL\n");
                    //res =  XRAN_STATUS_FAIL;
                    goto err0;
                }
            }

            /* Full slot UL OTA + time_diff_us */
            if(time_diff_nSymb > N_SYM_PER_SLOT)
            {
                p_dev_ctx->perMu[mu].deadline_slot_advance[XRAN_SLOT_FULL_CB] = (time_diff_nSymb) / N_SYM_PER_SLOT;
            }
            print_dbg("Full slot UL %d [%d]\n", p_dev_ctx->perMu[mu].deadline_slot_advance[XRAN_SLOT_FULL_CB], time_diff_nSymb);
            cb_elm = xran_create_cb(xran_timer_arm_for_deadline, rx_ul_deadline_full_cb, (void*)p_dev_ctx);
            if(cb_elm)
            {
                LIST_INSERT_HEAD(&p_dev_ctx->perMu[mu].sym_cb_list_head[time_diff_nSymb % XRAN_NUM_OF_SYMBOL_PER_SLOT],
                        cb_elm, pointers);
            }
            else
            {
                print_err("cb_elm is NULL\n");
                goto err0;
            }

            /* 1/4 UL OTA + time_diff_us*/
            if(time_diff_nSymb + 1*(N_SYM_PER_SLOT/4) > N_SYM_PER_SLOT)
            {
                p_dev_ctx->perMu[mu].deadline_slot_advance[XRAN_SLOT_1_4_CB] = (time_diff_nSymb + 1*(N_SYM_PER_SLOT/4)) / N_SYM_PER_SLOT;
            }
            print_dbg("1/4 UL OTA  %d [%d]\n", p_dev_ctx->perMu[mu].deadline_slot_advance[XRAN_SLOT_1_4_CB], time_diff_nSymb);
            cb_elm = xran_create_cb(xran_timer_arm_for_deadline, rx_ul_deadline_one_fourths_cb, (void*)p_dev_ctx);
            if(cb_elm)
            {
                LIST_INSERT_HEAD(&p_dev_ctx->perMu[mu].sym_cb_list_head[(time_diff_nSymb + 1*(N_SYM_PER_SLOT/4)) % XRAN_NUM_OF_SYMBOL_PER_SLOT],
                        cb_elm, pointers);
            }
            else
            {
                print_err("cb_elm is NULL\n");
                goto err0;
            }

            /* Half slot UL OTA + time_diff_us*/
            if(time_diff_nSymb + N_SYM_PER_SLOT/2 > N_SYM_PER_SLOT)
            {
                p_dev_ctx->perMu[mu].deadline_slot_advance[XRAN_SLOT_HALF_CB] = (time_diff_nSymb + N_SYM_PER_SLOT/2) / N_SYM_PER_SLOT;
            }
            print_dbg("Half slot UL   %d [%d]\n", p_dev_ctx->perMu[mu].deadline_slot_advance[XRAN_SLOT_HALF_CB], time_diff_nSymb);
            cb_elm = xran_create_cb(xran_timer_arm_for_deadline, rx_ul_deadline_half_cb, (void*)p_dev_ctx);
            if(cb_elm)
            {
                LIST_INSERT_HEAD(&p_dev_ctx->perMu[mu].sym_cb_list_head[(time_diff_nSymb + N_SYM_PER_SLOT/2) % XRAN_NUM_OF_SYMBOL_PER_SLOT],
                        cb_elm, pointers);
            }
            else
            {
                print_err("cb_elm is NULL\n");
                goto err0;
            }

            /* 3/4 UL OTA + time_diff_us*/
            if(time_diff_nSymb + 4*(N_SYM_PER_SLOT/4))
            {
                p_dev_ctx->perMu[mu].deadline_slot_advance[XRAN_SLOT_3_4_CB] = (time_diff_nSymb + 4*(N_SYM_PER_SLOT/4)) / N_SYM_PER_SLOT;
            }
            print_dbg("3/4 UL   %d [%d]\n", p_dev_ctx->perMu[mu].deadline_slot_advance[XRAN_SLOT_3_4_CB], time_diff_nSymb);
            cb_elm = xran_create_cb(xran_timer_arm_for_deadline, rx_ul_deadline_three_fourths_cb, (void*)p_dev_ctx);
            if(cb_elm)
            {
                LIST_INSERT_HEAD(&p_dev_ctx->perMu[mu].sym_cb_list_head[(time_diff_nSymb + 4*(N_SYM_PER_SLOT/4)) % XRAN_NUM_OF_SYMBOL_PER_SLOT],
                        cb_elm, pointers);
            }
            else
            {
                print_err("cb_elm is NULL\n");
                goto err0;
            }

            /* static srs Full slot UL OTA + time_diff_us + SrsDealySym */
            if (0 == p_dev_ctx->enableSrsCp)
            {
                uint16_t nSrsDealySym = p_dev_ctx->nSrsDelaySym;
                printf("Start U-plane static SRS %d us OTA        [offset  in sym %d]\n", time_diff_us, time_diff_nSymb + nSrsDealySym);
                cb_elm = xran_create_cb(xran_timer_arm_for_deadline, rx_ul_static_srs_cb, (void*)p_dev_ctx);
                if(cb_elm)
                {
                    LIST_INSERT_HEAD(&p_dev_ctx->perMu[mu].sym_cb_list_head[(time_diff_nSymb + nSrsDealySym) % XRAN_NUM_OF_SYMBOL_PER_SLOT],
                                cb_elm,
                                pointers);
                }
                else
                {
                    print_err("cb_elm is NULL\n");
                    //res =  XRAN_STATUS_FAIL;
                    goto err0;
                }
            }
        } // APP_O_DU
        else
        {    // APP_O_RU
            /* calculate when to send UL U-plane */
            delay_up = p_dev_ctx->fh_cfg.perMu[mu].Ta3_min;
            p_dev_ctx->perMu[mu].sym_up = sym_up = delay_up*1000/(interval_us_local*1000/N_SYM_PER_SLOT)+1;
            printf("RU%d mu=%u, Start UL U-plane %d us after OTA [offset in sym %d]\n", ru_id, mu, delay_up, sym_up);

            /* calculate when to Receive DL U-plane */
            delay_up = p_dev_ctx->fh_cfg.perMu[mu].T2a_max_up;
            sym_up = delay_up*1000/(interval_us_local*1000/N_SYM_PER_SLOT)+1;
            printf("RU%d mu=%u, Receive DL U-plane %d us after OTA [offset in sym %d]\n", ru_id, mu, delay_up, sym_up);

            uint32_t ruRxMinUp = p_dev_ctx->fh_cfg.perMu[mu].T2a_min_up;
            uint32_t ruRxMaxUp = p_dev_ctx->fh_cfg.perMu[mu].T2a_max_up;

            ruRxMinUp = ruRxMinUp*1000/(interval_us_local*1000/N_SYM_PER_SLOT);
            p_dev_ctx->perMu[mu].ruRxSymUpMin    = ruRxMinUp;
            ruRxMaxUp = ruRxMaxUp*1000/(interval_us_local*1000/N_SYM_PER_SLOT);
            p_dev_ctx->perMu[mu].ruRxSymUpMax    = ruRxMaxUp;
            printf("RU%d T2a_min_up=%u, T2a_max_up=%u, ruRxSymUpMin=%u, ruRxSymUpMax=%u\n", ru_id,
                p_dev_ctx->fh_cfg.perMu[mu].T2a_min_up, p_dev_ctx->fh_cfg.perMu[mu].T2a_max_up,
                p_dev_ctx->perMu[mu].ruRxSymUpMin, p_dev_ctx->perMu[mu].ruRxSymUpMax);

            /* Full slot UL OTA + time_diff_us */
            cb_elm = xran_create_cb(xran_timer_arm_for_deadline, rx_ul_deadline_full_cb, (void*)p_dev_ctx);
            if(cb_elm)
            {
                LIST_INSERT_HEAD(&p_dev_ctx->perMu[mu].sym_cb_list_head[sym_up % XRAN_NUM_OF_SYMBOL_PER_SLOT],
                        cb_elm,
                        pointers);
            }
            else
            {
                print_err("cb_elm is NULL\n");
                goto err0;
            }

            do {
                timespec_get(&ts, TIME_UTC);
            } while (ts.tv_nsec >1500);

            struct tm * ptm = gmtime(&ts.tv_sec);
            if(ptm)
            {
                strftime(buff, sizeof buff, "%D %T", ptm);
                printf("RU%d: thread_run start time: %s.%09ld UTC [%d]\n", ru_id, buff, ts.tv_nsec, interval_us_local);
            }
        }
    } // for all MUs

    return XRAN_STATUS_SUCCESS;

err0:
    for(int32_t i = 0; i < XRAN_MAX_NUM_MU; i++)
    {
        for(int32_t j = 0; j < XRAN_NUM_OF_SYMBOL_PER_SLOT; j++)
        {
            xran_remove_cb_list(&p_dev_ctx->perMu[i].sym_cb_list_head[j]);
        }
    }

    return XRAN_STATUS_FAIL;
}

int32_t xran_timing_destroy_cbs(void *args)
{
    //int res = XRAN_STATUS_SUCCESS;
    struct xran_device_ctx * p_dev_ctx = (struct xran_device_ctx *)args;

    for(int32_t i = 0; i < p_dev_ctx->fh_cfg.numMUs; i++)
    {
        uint8_t mu = p_dev_ctx->fh_cfg.mu_number[i];
        for(int32_t j = 0; j< XRAN_NUM_OF_SYMBOL_PER_SLOT; j++)
        {
            xran_remove_cb_list(&p_dev_ctx->perMu[mu].sym_cb_list_head[j]);
        }
    }

    return XRAN_STATUS_SUCCESS;
}

static int32_t
xran_reg_sym_cb_ota(struct xran_device_ctx * p_dev_ctx, xran_callback_sym_fn symCb, void * symCbParam, struct xran_sense_of_time* symCbTime,  uint8_t symb,
    struct cb_user_per_sym_ctx **p_sym_cb_ctx, uint8_t mu)
{
    int32_t ret = XRAN_STATUS_SUCCESS;
    struct cb_user_per_sym_ctx *p_loc_sym_cb_ctx = &p_dev_ctx->perMu[mu].symCbCtx[symb][XRAN_CB_SYM_OTA_TIME];
    if(p_loc_sym_cb_ctx->status){
        ret =  XRAN_STATUS_RESOURCE;
        print_err("timer sym %d type id %d was already created",symb, XRAN_CB_SYM_OTA_TIME);
        return ret;
    }
    printf("requested symb %d OTA coresponds to symb %d OTA time\n", symb, symb);

    p_loc_sym_cb_ctx->symb_num_req  = symb;
    p_loc_sym_cb_ctx->sym_diff      = 0; /* OTA and Request Symb are the same */
    p_loc_sym_cb_ctx->symb_num_ota  = symb;
    p_loc_sym_cb_ctx->cb_type_id    = XRAN_CB_SYM_OTA_TIME;
    p_loc_sym_cb_ctx->p_dev         = p_dev_ctx;

    p_loc_sym_cb_ctx->symCb         = symCb;
    p_loc_sym_cb_ctx->symCbParam    = symCbParam;
    p_loc_sym_cb_ctx->symCbTimeInfo = symCbTime;
    p_loc_sym_cb_ctx->mu            = mu;

    p_loc_sym_cb_ctx->status        = 1;

    *p_sym_cb_ctx = p_loc_sym_cb_ctx;

    return ret;
}

static int32_t
xran_reg_sym_cb_rx_win_end(struct xran_device_ctx * p_dev_ctx, xran_callback_sym_fn symCb, void * symCbParam, struct xran_sense_of_time* symCbTime,
    uint8_t symb, struct cb_user_per_sym_ctx **p_sym_cb_ctx, uint8_t mu)
{
    int32_t ret = XRAN_STATUS_SUCCESS;
    struct cb_user_per_sym_ctx *p_loc_sym_cb_ctx = &p_dev_ctx->perMu[mu].symCbCtx[symb][XRAN_CB_SYM_RX_WIN_END];
    uint32_t time_diff_us      = 0;
    uint32_t time_diff_nSymb   = 0;
    uint32_t absolute_ota_sym  = 0;
    uint32_t interval_us_local = xran_fs_get_tti_interval(mu);

    if(p_loc_sym_cb_ctx->status) {
        ret =  XRAN_STATUS_RESOURCE;
        print_err("timer sym %d type id %d was already created",symb, XRAN_CB_SYM_RX_WIN_END);
        return ret;
    }

    time_diff_us = p_dev_ctx->fh_cfg.perMu[mu].Ta4_max;
    printf("RX WIN end Ta4_max is %d [us] where TTI is %d [us] \n", time_diff_us, interval_us_local);
    time_diff_nSymb = time_diff_us*1000/(interval_us_local*1000/N_SYM_PER_SLOT);
    if ((time_diff_nSymb/1000/(interval_us_local*1000/N_SYM_PER_SLOT)) < time_diff_us) {
        time_diff_nSymb+=1;
        printf("time duration %d rounded up to duration of %d symbols\n", time_diff_us, time_diff_nSymb);
    }
    printf("U-plane UL delay %d [us] measured against OTA time [offset in symbols is %d]\n", time_diff_us, time_diff_nSymb);
    absolute_ota_sym =  (symb + time_diff_nSymb) % XRAN_NUM_OF_SYMBOL_PER_SLOT;
    printf("requested symb %d pkt arrival time [deadline] coresponds to symb %d OTA time\n", symb, absolute_ota_sym);

    p_loc_sym_cb_ctx->symb_num_req  = symb;
    p_loc_sym_cb_ctx->sym_diff      = -time_diff_nSymb;
    p_loc_sym_cb_ctx->symb_num_ota  = absolute_ota_sym;
    p_loc_sym_cb_ctx->cb_type_id    = XRAN_CB_SYM_RX_WIN_END;
    p_loc_sym_cb_ctx->p_dev         = p_dev_ctx;

    p_loc_sym_cb_ctx->symCb         = symCb;
    p_loc_sym_cb_ctx->symCbParam    = symCbParam;
    p_loc_sym_cb_ctx->symCbTimeInfo = symCbTime;
    p_loc_sym_cb_ctx->mu            = mu;

    p_loc_sym_cb_ctx->status        = 1;

    *p_sym_cb_ctx  =p_loc_sym_cb_ctx;

    return ret;
}

static int32_t
xran_reg_sym_cb_rx_win_begin(struct xran_device_ctx * p_dev_ctx, xran_callback_sym_fn symCb, void * symCbParam, struct xran_sense_of_time* symCbTime,
    uint8_t symb, struct cb_user_per_sym_ctx **p_sym_cb_ctx, uint8_t mu)
{
    int32_t ret = XRAN_STATUS_SUCCESS;
    struct cb_user_per_sym_ctx *p_loc_sym_cb_ctx = &p_dev_ctx->perMu[mu].symCbCtx[symb][XRAN_CB_SYM_RX_WIN_BEGIN];
    uint32_t time_diff_us      = 0;
    uint32_t time_diff_nSymb   = 0;
    uint32_t absolute_ota_sym  = 0;
    uint32_t interval_us_local = xran_fs_get_tti_interval(mu);

    if(p_loc_sym_cb_ctx->status) {
        ret =  XRAN_STATUS_RESOURCE;
        print_err("timer sym %d type id %d was already created",symb, XRAN_CB_SYM_RX_WIN_BEGIN);
        return ret;
    }

    time_diff_us = p_dev_ctx->fh_cfg.perMu[mu].Ta4_min;
    printf("RX WIN begin Ta4_min is %d [us] where TTI is %d [us] \n", time_diff_us, interval_us_local);
    time_diff_nSymb = time_diff_us*1000/(interval_us_local*1000/N_SYM_PER_SLOT);
    printf("U-plane UL delay %d [us] measured against OTA time [offset in symbols is %d]\n", time_diff_us, time_diff_nSymb);
    absolute_ota_sym =  (symb + time_diff_nSymb) % XRAN_NUM_OF_SYMBOL_PER_SLOT;
    printf("requested symb %d pkt arrival time [deadline] coresponds to symb %d OTA time\n", symb, absolute_ota_sym);

    p_loc_sym_cb_ctx->symb_num_req  = symb;
    p_loc_sym_cb_ctx->sym_diff      = -time_diff_nSymb;
    p_loc_sym_cb_ctx->symb_num_ota  = absolute_ota_sym;
    p_loc_sym_cb_ctx->cb_type_id    = XRAN_CB_SYM_RX_WIN_BEGIN;
    p_loc_sym_cb_ctx->p_dev         = p_dev_ctx;

    p_loc_sym_cb_ctx->symCb         = symCb;
    p_loc_sym_cb_ctx->symCbParam    = symCbParam;
    p_loc_sym_cb_ctx->symCbTimeInfo = symCbTime;
    p_loc_sym_cb_ctx->mu            = mu;

    p_loc_sym_cb_ctx->status        = 1;

    *p_sym_cb_ctx  =p_loc_sym_cb_ctx;

    return ret;
}

static int32_t
xran_reg_sym_cb_tx_win_end(struct xran_device_ctx * p_dev_ctx, xran_callback_sym_fn symCb, void * symCbParam, struct xran_sense_of_time* symCbTime,
    uint8_t symb, struct cb_user_per_sym_ctx **p_sym_cb_ctx, uint8_t mu)
{
    int32_t ret = XRAN_STATUS_SUCCESS;
    struct cb_user_per_sym_ctx *p_loc_sym_cb_ctx = &p_dev_ctx->perMu[mu].symCbCtx[symb][XRAN_CB_SYM_TX_WIN_END];
    uint32_t time_diff_us      = 0;
    uint32_t time_diff_nSymb   = 0;
    uint32_t absolute_ota_sym  = 0;
    uint32_t interval_us_local = xran_fs_get_tti_interval(mu);

    if(p_loc_sym_cb_ctx->status) {
        ret =  XRAN_STATUS_RESOURCE;
        print_err("timer sym %d type id %d was already created",symb, XRAN_CB_SYM_TX_WIN_END);
        return ret;
    }

    time_diff_us = p_dev_ctx->fh_cfg.perMu[mu].T1a_min_up;
    printf("TX WIN end -T1a_min_up is %d [us] where TTI is %d [us] \n", time_diff_us, interval_us_local);
    time_diff_nSymb = time_diff_us*1000/(interval_us_local*1000/N_SYM_PER_SLOT);
    if ((time_diff_nSymb/1000/(interval_us_local*1000/N_SYM_PER_SLOT)) < time_diff_us) {
        time_diff_nSymb +=1;
        printf("time duration %d rounded up to duration of %d symbols\n", time_diff_us, time_diff_nSymb);
    }
    printf("U-plane DL advance is %d [us] measured against OTA time [offset in symbols is %d]\n", time_diff_us, -time_diff_nSymb);
    absolute_ota_sym =  ((symb + XRAN_NUM_OF_SYMBOL_PER_SLOT) - time_diff_nSymb) % XRAN_NUM_OF_SYMBOL_PER_SLOT;
    printf("requested symb %d pkt tx time [deadline] corresponds to symb %d OTA time\n", symb, absolute_ota_sym);

    p_loc_sym_cb_ctx->symb_num_req  = symb;
    p_loc_sym_cb_ctx->sym_diff      = time_diff_nSymb;
    p_loc_sym_cb_ctx->symb_num_ota  = absolute_ota_sym;
    p_loc_sym_cb_ctx->cb_type_id    = XRAN_CB_SYM_TX_WIN_END;
    p_loc_sym_cb_ctx->p_dev         = p_dev_ctx;

    p_loc_sym_cb_ctx->symCb         = symCb;
    p_loc_sym_cb_ctx->symCbParam    = symCbParam;
    p_loc_sym_cb_ctx->symCbTimeInfo = symCbTime;
    p_loc_sym_cb_ctx->mu            = mu;

    p_loc_sym_cb_ctx->status        = 1;

    *p_sym_cb_ctx  = p_loc_sym_cb_ctx;

    return ret;
}

static int32_t
xran_reg_sym_cb_tx_win_begin(struct xran_device_ctx * p_dev_ctx, xran_callback_sym_fn symCb, void * symCbParam, struct xran_sense_of_time* symCbTime,
     uint8_t symb, struct cb_user_per_sym_ctx **p_sym_cb_ctx, uint8_t mu)
{
    int32_t ret = XRAN_STATUS_SUCCESS;
    struct cb_user_per_sym_ctx *p_loc_sym_cb_ctx = &p_dev_ctx->perMu[mu].symCbCtx[symb][XRAN_CB_SYM_TX_WIN_BEGIN];
    uint32_t time_diff_us      = 0;
    uint32_t time_diff_nSymb   = 0;
    uint32_t absolute_ota_sym  = 0;
    uint32_t interval_us_local = xran_fs_get_tti_interval(mu);

    if(p_loc_sym_cb_ctx->status) {
        ret =  XRAN_STATUS_RESOURCE;
        print_err("timer sym %d type id %d was already created",symb, XRAN_CB_SYM_TX_WIN_BEGIN);
        return ret;
    }

    time_diff_us = p_dev_ctx->fh_cfg.perMu[mu].T1a_max_up;
    printf("TX WIN begin -T1a_max_up is %d [us] where TTI is %d [us] \n", time_diff_us, interval_us_local);
    time_diff_nSymb = (time_diff_us*1000/(interval_us_local*1000/N_SYM_PER_SLOT));
    if ((time_diff_nSymb/1000/(interval_us_local*1000/N_SYM_PER_SLOT)) < time_diff_us) {
        time_diff_nSymb +=1;
        printf("time duration %d rounded up to duration of %d symbols\n", time_diff_us, time_diff_nSymb);
    }
    printf("U-plane DL advance is %d [us] measured against OTA time [offset in symbols is %d]\n", time_diff_us, -time_diff_nSymb);
    printf("requested symb %d pkt tx time [deadline] corresponds to symb %d OTA time\n", symb, absolute_ota_sym);
    absolute_ota_sym =  ((symb + XRAN_NUM_OF_SYMBOL_PER_SLOT) - time_diff_nSymb) % XRAN_NUM_OF_SYMBOL_PER_SLOT;

    p_loc_sym_cb_ctx->symb_num_req  = symb;
    p_loc_sym_cb_ctx->sym_diff      = time_diff_nSymb;
    p_loc_sym_cb_ctx->symb_num_ota  = absolute_ota_sym;
    p_loc_sym_cb_ctx->cb_type_id    = XRAN_CB_SYM_TX_WIN_BEGIN;
    p_loc_sym_cb_ctx->p_dev         = p_dev_ctx;

    p_loc_sym_cb_ctx->symCb         = symCb;
    p_loc_sym_cb_ctx->symCbParam    = symCbParam;
    p_loc_sym_cb_ctx->symCbTimeInfo = symCbTime;
    p_loc_sym_cb_ctx->mu            = mu;

    p_loc_sym_cb_ctx->status        = 1;

    *p_sym_cb_ctx  =p_loc_sym_cb_ctx;

    return ret;
}

int32_t xran_reg_sym_cb(void *pHandle, xran_callback_sym_fn symCb, void * symCbParam, struct xran_sense_of_time* symCbTime,
        uint8_t symb, enum cb_per_sym_type_id cb_sym_t_id,uint8_t mu)
{
    int32_t ret = XRAN_STATUS_SUCCESS;
    struct xran_device_ctx * p_dev_ctx       = NULL;
    struct cb_elem_entry * cb_elm            = NULL;
    struct cb_user_per_sym_ctx *p_sym_cb_ctx = NULL;
    rx_dpdk_sym_cb_fn dpdk_cb_to_arm         = NULL;

    if(xran_get_if_state() == XRAN_RUNNING) {
        print_err("Cannot register callback while running!!");
        return (-1);
    }

    if(pHandle) {
        p_dev_ctx = (struct xran_device_ctx *)pHandle;
    } else {
        print_err("pHandle==NULL");
        ret = XRAN_STATUS_INVALID_PARAM;
        return ret;
    }
    if(mu == XRAN_DEFAULT_MU)
        mu = p_dev_ctx->fh_cfg.mu_number[0];

    switch (cb_sym_t_id) {
        case XRAN_CB_SYM_OTA_TIME:
            ret = xran_reg_sym_cb_ota(p_dev_ctx, symCb, symCbParam, symCbTime, symb, &p_sym_cb_ctx, mu);
            if(ret != XRAN_STATUS_SUCCESS)
                return ret;
            dpdk_cb_to_arm = rx_ul_user_sym_cb;
        break;
        case XRAN_CB_SYM_RX_WIN_BEGIN:
            ret = xran_reg_sym_cb_rx_win_begin(p_dev_ctx, symCb, symCbParam, symCbTime, symb, &p_sym_cb_ctx, mu);
            if(ret != XRAN_STATUS_SUCCESS)
                return ret;
            dpdk_cb_to_arm = rx_ul_user_sym_cb;
            break;
        case XRAN_CB_SYM_RX_WIN_END:
            ret = xran_reg_sym_cb_rx_win_end(p_dev_ctx, symCb, symCbParam, symCbTime, symb, &p_sym_cb_ctx, mu);
            if(ret != XRAN_STATUS_SUCCESS)
                return ret;
            dpdk_cb_to_arm = rx_ul_user_sym_cb;
        break;
        case XRAN_CB_SYM_TX_WIN_BEGIN:
            ret = xran_reg_sym_cb_tx_win_begin(p_dev_ctx, symCb, symCbParam, symCbTime, symb, &p_sym_cb_ctx, mu);
            if(ret != XRAN_STATUS_SUCCESS)
                return ret;
            dpdk_cb_to_arm = rx_ul_user_sym_cb;
            break;
        case XRAN_CB_SYM_TX_WIN_END:
            ret = xran_reg_sym_cb_tx_win_end(p_dev_ctx, symCb, symCbParam, symCbTime, symb, &p_sym_cb_ctx, mu);
            if(ret != XRAN_STATUS_SUCCESS)
                return ret;
            dpdk_cb_to_arm = rx_ul_user_sym_cb;
        break;
        default:
            /* functionality is not yet implemented */
            print_err("Functionality is not yet implemented !");
            ret = XRAN_STATUS_INVALID_PARAM;
            return ret;
    }

    cb_elm = xran_create_cb(xran_timer_arm_user_cb, dpdk_cb_to_arm, (void*)p_sym_cb_ctx);
    if(cb_elm){
        LIST_INSERT_HEAD(&p_dev_ctx->perMu[mu].sym_cb_list_head[p_sym_cb_ctx->symb_num_ota],
                            cb_elm,
                            pointers);
    } else {
        print_err("cb_elm is NULL\n");
        ret =  XRAN_STATUS_FAIL;
        return ret;
    }

    return ret;
}

int32_t xran_reg_physide_oam_cb(void *pHandle, xran_callback_oam_notify_fn Cb)
{
    struct xran_system_config *p_syscfg = xran_get_systemcfg();

    if(xran_get_if_state() == XRAN_RUNNING) {
        print_err("Cannot register callback while running!!\n");
        return (-1);
    }
    p_syscfg->oam_notify_cb  = Cb;

    return 0;
}
