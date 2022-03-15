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

#include "ethdi.h"
#include "xran_fh_o_du.h"
#include "xran_main.h"
#include "xran_dev.h"
#include "xran_common.h"
#include "xran_cb_proc.h"
#include "xran_mlog_lnx.h"
#include "xran_lib_mlog_tasks_id.h"
#include "xran_printf.h"

typedef void (*rx_dpdk_sym_cb_fn)(struct rte_timer *tim, void *arg);

void xran_timer_arm(struct rte_timer *tim, void* arg, void *p_dev_ctx)
{
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)p_dev_ctx;
    uint64_t t3 = MLogTick();

    if (xran_if_current_state == XRAN_RUNNING){
        rte_timer_cb_t fct = (rte_timer_cb_t)arg;
        rte_timer_reset_sync(tim, 0, SINGLE, p_xran_dev_ctx->fh_init.io_cfg.timing_core, fct, p_dev_ctx);
    }
    MLogTask(PID_TIME_ARM_TIMER, t3, MLogTick());
}

void xran_timer_arm_cp_dl(struct rte_timer *tim, void* arg, void *p_dev_ctx)
{
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)p_dev_ctx;
    uint64_t t3 = MLogTick();

    unsigned tim_lcore = xran_schedule_to_worker(XRAN_JOB_TYPE_CP_DL, p_xran_dev_ctx);

    if (xran_if_current_state == XRAN_RUNNING){
        rte_timer_cb_t fct = (rte_timer_cb_t)arg;
        rte_timer_reset_sync(tim, 0, SINGLE, tim_lcore, fct, p_dev_ctx);
    }
    MLogTask(PID_TIME_ARM_TIMER, t3, MLogTick());
}

void xran_timer_arm_cp_ul(struct rte_timer *tim, void* arg, void *p_dev_ctx)
{
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)p_dev_ctx;
    uint64_t t3 = MLogTick();

    unsigned tim_lcore = xran_schedule_to_worker(XRAN_JOB_TYPE_CP_UL, p_xran_dev_ctx);

    if (xran_if_current_state == XRAN_RUNNING){
        rte_timer_cb_t fct = (rte_timer_cb_t)arg;
        rte_timer_reset_sync(tim, 0, SINGLE, tim_lcore, fct, p_dev_ctx);
    }
    MLogTask(PID_TIME_ARM_TIMER, t3, MLogTick());
}

void xran_timer_arm_for_deadline(struct rte_timer *tim, void* arg,  void *p_dev_ctx)
{
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)p_dev_ctx;
    uint64_t t3 = MLogTick();

    unsigned tim_lcore = xran_schedule_to_worker(XRAN_JOB_TYPE_DEADLINE, p_xran_dev_ctx);

    int32_t rx_tti;
    int32_t cc_id;
    uint32_t nFrameIdx;
    uint32_t nSubframeIdx;
    uint32_t nSlotIdx;
    uint64_t nSecond;

    xran_get_slot_idx(p_xran_dev_ctx->xran_port_id, &nFrameIdx, &nSubframeIdx, &nSlotIdx, &nSecond);
    rx_tti = nFrameIdx*SUBFRAMES_PER_SYSTEMFRAME*SLOTNUM_PER_SUBFRAME(p_xran_dev_ctx->interval_us_local)
           + nSubframeIdx*SLOTNUM_PER_SUBFRAME(p_xran_dev_ctx->interval_us_local)
           + nSlotIdx;

    p_xran_dev_ctx->cb_timer_ctx[p_xran_dev_ctx->timer_put %  MAX_CB_TIMER_CTX].tti_to_process = rx_tti;
    if (xran_if_current_state == XRAN_RUNNING){
        rte_timer_cb_t fct = (rte_timer_cb_t)arg;
        rte_timer_reset_sync(tim, 0, SINGLE, tim_lcore, fct, p_xran_dev_ctx);
    }

    MLogTask(PID_TIME_ARM_TIMER_DEADLINE, t3, MLogTick());
}

void xran_timer_arm_user_cb(struct rte_timer *tim, void* arg,  void *p_ctx)
{
    struct cb_user_per_sym_ctx* p_sym_cb_ctx = (struct cb_user_per_sym_ctx *)p_ctx;
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)p_sym_cb_ctx->p_dev;
    uint64_t t3 = MLogTick();

    unsigned tim_lcore = xran_schedule_to_worker(XRAN_JOB_TYPE_SYM_CB, NULL);

    int32_t rx_tti;
    int32_t cc_id;
    uint32_t nFrameIdx = 0;
    uint32_t nSubframeIdx = 0;
    uint32_t nSlotIdx = 0;
    uint64_t nSecond = 0;

    xran_get_slot_idx(p_xran_dev_ctx->xran_port_id, &nFrameIdx, &nSubframeIdx, &nSlotIdx, &nSecond);
    rx_tti = nFrameIdx*SUBFRAMES_PER_SYSTEMFRAME*SLOTNUM_PER_SUBFRAME(p_xran_dev_ctx->interval_us_local)
           + nSubframeIdx*SLOTNUM_PER_SUBFRAME(p_xran_dev_ctx->interval_us_local)
           + nSlotIdx;

    p_sym_cb_ctx->user_cb_timer_ctx[p_sym_cb_ctx->user_timer_put % MAX_CB_TIMER_CTX].tti_to_process = rx_tti;
    p_sym_cb_ctx->user_cb_timer_ctx[p_sym_cb_ctx->user_timer_put % MAX_CB_TIMER_CTX].ota_sym_idx = xran_lib_ota_sym_idx[p_xran_dev_ctx->xran_port_id];
    p_sym_cb_ctx->user_cb_timer_ctx[p_sym_cb_ctx->user_timer_put % MAX_CB_TIMER_CTX].xran_sfn_at_sec_start = xran_getSfnSecStart();
    p_sym_cb_ctx->user_cb_timer_ctx[p_sym_cb_ctx->user_timer_put % MAX_CB_TIMER_CTX].current_second = nSecond;

    if (xran_if_current_state == XRAN_RUNNING){
        rte_timer_cb_t fct = (rte_timer_cb_t)arg;
        rte_timer_reset_sync(tim, 0, SINGLE, tim_lcore, fct, p_sym_cb_ctx);
        if (++p_sym_cb_ctx->user_timer_put >= MAX_CB_TIMER_CTX)
            p_sym_cb_ctx->user_timer_put = 0;
    }

    MLogTask(PID_TIME_ARM_USER_TIMER_DEADLINE, t3, MLogTick());
}

void xran_timer_arm_ex(struct rte_timer *tim, void* CbFct, void *CbArg, unsigned tim_lcore)
{
    uint64_t t3 = MLogTick();

    if (xran_if_current_state == XRAN_RUNNING){
        rte_timer_cb_t fct = (rte_timer_cb_t)CbFct;
        rte_timer_reset_sync(tim, 0, SINGLE, tim_lcore, fct, CbArg);
    }
    MLogTask(PID_TIME_ARM_TIMER, t3, MLogTick());
}

int32_t
xran_timing_create_cbs(void *args)
{
    int32_t  res = XRAN_STATUS_SUCCESS;
    int32_t  do_reset = 0;
    uint64_t t1 = 0;
    int32_t  result1,i,j;
    uint32_t delay_cp_dl;
    uint32_t delay_cp_ul;
    uint32_t delay_up;
    uint32_t time_diff_us;
    uint32_t delay_cp2up;
    uint32_t sym_cp_dl;
    uint32_t sym_cp_ul;
    uint32_t time_diff_nSymb;
    int32_t  sym_up;
    struct xran_device_ctx * p_dev_ctx =  (struct xran_device_ctx *)args;
    uint64_t tWake = 0, tWakePrev = 0, tUsed = 0;
    struct cb_elem_entry * cb_elm = NULL;
    uint32_t interval_us_local = p_dev_ctx->interval_us_local;

    /* ToS = Top of Second start +- 1.5us */
    struct timespec ts;
    char buff[100];

    if (p_dev_ctx->fh_init.io_cfg.id == O_DU) {

        delay_cp_dl = interval_us_local - p_dev_ctx->fh_cfg.T1a_max_cp_dl;
        delay_cp_ul = interval_us_local - p_dev_ctx->fh_cfg.T1a_max_cp_ul;
        delay_up    = p_dev_ctx->fh_cfg.T1a_max_up;
        time_diff_us = p_dev_ctx->fh_cfg.Ta4_max;

        delay_cp2up = delay_up-delay_cp_dl;

        sym_cp_dl = delay_cp_dl*1000/(interval_us_local*1000/N_SYM_PER_SLOT)+1;
        sym_cp_ul = delay_cp_ul*1000/(interval_us_local*1000/N_SYM_PER_SLOT)+1;
        time_diff_nSymb = time_diff_us*1000/(interval_us_local*1000/N_SYM_PER_SLOT);
        p_dev_ctx->sym_up = sym_up = -(delay_up*1000/(interval_us_local*1000/N_SYM_PER_SLOT));
        p_dev_ctx->sym_up_ul = time_diff_nSymb = (time_diff_us*1000/(interval_us_local*1000/N_SYM_PER_SLOT)+1);

        printf("Start C-plane DL %d us after TTI  [trigger on sym %d]\n", delay_cp_dl, sym_cp_dl);
        printf("Start C-plane UL %d us after TTI  [trigger on sym %d]\n", delay_cp_ul, sym_cp_ul);
        printf("Start U-plane DL %d us before OTA [offset  in sym %d]\n", delay_up, sym_up);
        printf("Start U-plane UL %d us OTA        [offset  in sym %d]\n", time_diff_us, time_diff_nSymb);

        printf("C-plane to U-plane delay %d us after TTI\n", delay_cp2up);
        printf("Start Sym timer %ld ns\n", TX_TIMER_INTERVAL/N_SYM_PER_SLOT);

        cb_elm = xran_create_cb(xran_timer_arm_cp_dl, tx_cp_dl_cb, (void*)p_dev_ctx);
        if(cb_elm){
            LIST_INSERT_HEAD(&p_dev_ctx->sym_cb_list_head[sym_cp_dl],
                             cb_elm,
                             pointers);
        } else {
            print_err("cb_elm is NULL\n");
            res =  XRAN_STATUS_FAIL;
            goto err0;
        }

        cb_elm = xran_create_cb(xran_timer_arm_cp_ul, tx_cp_ul_cb, (void*)p_dev_ctx);
        if(cb_elm){
            LIST_INSERT_HEAD(&p_dev_ctx->sym_cb_list_head[sym_cp_ul],
                             cb_elm,
                             pointers);
        } else {
            print_err("cb_elm is NULL\n");
            res =  XRAN_STATUS_FAIL;
            goto err0;
        }

        /* Full slot UL OTA + time_diff_us */
        cb_elm = xran_create_cb(xran_timer_arm_for_deadline, rx_ul_deadline_full_cb, (void*)p_dev_ctx);
        if(cb_elm){
            LIST_INSERT_HEAD(&p_dev_ctx->sym_cb_list_head[time_diff_nSymb],
                             cb_elm,
                             pointers);
        } else {
            print_err("cb_elm is NULL\n");
            res =  XRAN_STATUS_FAIL;
            goto err0;
        }

        /* Half slot UL OTA + time_diff_us*/
        cb_elm = xran_create_cb(xran_timer_arm_for_deadline, rx_ul_deadline_half_cb, (void*)p_dev_ctx);
        if(cb_elm){
            LIST_INSERT_HEAD(&p_dev_ctx->sym_cb_list_head[time_diff_nSymb + N_SYM_PER_SLOT/2],
                         cb_elm,
                         pointers);
        } else {
            print_err("cb_elm is NULL\n");
            res =  XRAN_STATUS_FAIL;
            goto err0;
        }
    } else {    // APP_O_RU
        /* calculate when to send UL U-plane */
        delay_up = p_dev_ctx->fh_cfg.Ta3_min;
        p_dev_ctx->sym_up = sym_up = delay_up*1000/(interval_us_local*1000/N_SYM_PER_SLOT)+1;
        printf("Start UL U-plane %d us after OTA [offset in sym %d]\n", delay_up, sym_up);

        /* calcualte when to Receive DL U-plane */
        delay_up = p_dev_ctx->fh_cfg.T2a_max_up;
        sym_up = delay_up*1000/(interval_us_local*1000/N_SYM_PER_SLOT)+1;
        printf("Receive DL U-plane %d us after OTA [offset in sym %d]\n", delay_up, sym_up);

        /* Full slot UL OTA + time_diff_us */
        cb_elm = xran_create_cb(xran_timer_arm_for_deadline, rx_ul_deadline_full_cb, (void*)p_dev_ctx);
        if(cb_elm){
            LIST_INSERT_HEAD(&p_dev_ctx->sym_cb_list_head[sym_up],
                             cb_elm,
                             pointers);
        } else {
            print_err("cb_elm is NULL\n");
            res =  -1;
            goto err0;
        }

        do {
           timespec_get(&ts, TIME_UTC);
        }while (ts.tv_nsec >1500);
        struct tm * ptm = gmtime(&ts.tv_sec);
        if(ptm){
            strftime(buff, sizeof buff, "%D %T", ptm);
            printf("RU: thread_run start time: %s.%09ld UTC [%d]\n", buff, ts.tv_nsec, interval_us_local);
        }
    }

    return XRAN_STATUS_SUCCESS;

    err0:
    for (j = 0; j< XRAN_NUM_OF_SYMBOL_PER_SLOT; j++){
        struct cb_elem_entry *cb_elm;
        LIST_FOREACH(cb_elm, &p_dev_ctx->sym_cb_list_head[j], pointers){
            if(cb_elm){
                LIST_REMOVE(cb_elm, pointers);
                xran_destroy_cb(cb_elm);
            }
        }
    }

    return XRAN_STATUS_FAIL;
}
int32_t
xran_timing_destroy_cbs(void *args)
{
    int res = XRAN_STATUS_SUCCESS;
    int32_t   do_reset = 0;
    uint64_t  t1 = 0;
    int32_t   result1,i,j;
    struct xran_device_ctx * p_dev_ctx = (struct xran_device_ctx *)args;
    struct cb_elem_entry * cb_elm = NULL;

    for (j = 0; j< XRAN_NUM_OF_SYMBOL_PER_SLOT; j++){
        struct cb_elem_entry *cb_elm;
        LIST_FOREACH(cb_elm, &p_dev_ctx->sym_cb_list_head[j], pointers){
            if(cb_elm){
                LIST_REMOVE(cb_elm, pointers);
                xran_destroy_cb(cb_elm);
            }
        }
    }

    return XRAN_STATUS_SUCCESS;
}

static int32_t
xran_reg_sym_cb_ota(struct xran_device_ctx * p_dev_ctx, xran_callback_sym_fn symCb, void * symCbParam, struct xran_sense_of_time* symCbTime,  uint8_t symb,
    struct cb_user_per_sym_ctx **p_sym_cb_ctx)
{
    int32_t ret = XRAN_STATUS_SUCCESS;
    struct cb_user_per_sym_ctx *p_loc_sym_cb_ctx = &p_dev_ctx->symCbCtx[symb][XRAN_CB_SYM_OTA_TIME];
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

    p_loc_sym_cb_ctx->status        = 1;

    *p_sym_cb_ctx = p_loc_sym_cb_ctx;

    return ret;
}

static int32_t
xran_reg_sym_cb_rx_win_end(struct xran_device_ctx * p_dev_ctx, xran_callback_sym_fn symCb, void * symCbParam, struct xran_sense_of_time* symCbTime,
    uint8_t symb, struct cb_user_per_sym_ctx **p_sym_cb_ctx)
{
    int32_t ret = XRAN_STATUS_SUCCESS;
    struct cb_user_per_sym_ctx *p_loc_sym_cb_ctx = &p_dev_ctx->symCbCtx[symb][XRAN_CB_SYM_RX_WIN_END];
    uint32_t time_diff_us      = 0;
    uint32_t time_diff_nSymb   = 0;
    uint32_t absolute_ota_sym  = 0;
    uint32_t interval_us_local = p_dev_ctx->interval_us_local;

    if(p_loc_sym_cb_ctx->status) {
        ret =  XRAN_STATUS_RESOURCE;
        print_err("timer sym %d type id %d was already created",symb, XRAN_CB_SYM_RX_WIN_END);
        return ret;
    }

    time_diff_us = p_dev_ctx->fh_cfg.Ta4_max;
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

    p_loc_sym_cb_ctx->status        = 1;

    *p_sym_cb_ctx  =p_loc_sym_cb_ctx;

    return ret;
}

static int32_t
xran_reg_sym_cb_rx_win_begin(struct xran_device_ctx * p_dev_ctx, xran_callback_sym_fn symCb, void * symCbParam, struct xran_sense_of_time* symCbTime,
    uint8_t symb, struct cb_user_per_sym_ctx **p_sym_cb_ctx)
{
    int32_t ret = XRAN_STATUS_SUCCESS;
    struct cb_user_per_sym_ctx *p_loc_sym_cb_ctx = &p_dev_ctx->symCbCtx[symb][XRAN_CB_SYM_RX_WIN_BEGIN];
    uint32_t time_diff_us      = 0;
    uint32_t time_diff_nSymb   = 0;
    uint32_t absolute_ota_sym  = 0;
    uint32_t interval_us_local = p_dev_ctx->interval_us_local;

    if(p_loc_sym_cb_ctx->status) {
        ret =  XRAN_STATUS_RESOURCE;
        print_err("timer sym %d type id %d was already created",symb, XRAN_CB_SYM_RX_WIN_BEGIN);
        return ret;
    }

    time_diff_us = p_dev_ctx->fh_cfg.Ta4_min;
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

    p_loc_sym_cb_ctx->status        = 1;

    *p_sym_cb_ctx  =p_loc_sym_cb_ctx;

    return ret;
}

static int32_t
xran_reg_sym_cb_tx_win_end(struct xran_device_ctx * p_dev_ctx, xran_callback_sym_fn symCb, void * symCbParam, struct xran_sense_of_time* symCbTime,
    uint8_t symb, struct cb_user_per_sym_ctx **p_sym_cb_ctx)
{
    int32_t ret = XRAN_STATUS_SUCCESS;
    struct cb_user_per_sym_ctx *p_loc_sym_cb_ctx = &p_dev_ctx->symCbCtx[symb][XRAN_CB_SYM_TX_WIN_END];
    uint32_t time_diff_us      = 0;
    uint32_t time_diff_nSymb   = 0;
    uint32_t absolute_ota_sym  = 0;
    uint32_t interval_us_local = p_dev_ctx->interval_us_local;

    if(p_loc_sym_cb_ctx->status) {
        ret =  XRAN_STATUS_RESOURCE;
        print_err("timer sym %d type id %d was already created",symb, XRAN_CB_SYM_TX_WIN_END);
        return ret;
    }

    time_diff_us = p_dev_ctx->fh_cfg.T1a_min_up;
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

    p_loc_sym_cb_ctx->status        = 1;

    *p_sym_cb_ctx  = p_loc_sym_cb_ctx;

    return ret;
}

static int32_t
xran_reg_sym_cb_tx_win_begin(struct xran_device_ctx * p_dev_ctx, xran_callback_sym_fn symCb, void * symCbParam, struct xran_sense_of_time* symCbTime,
     uint8_t symb, struct cb_user_per_sym_ctx **p_sym_cb_ctx)
{
    int32_t ret = XRAN_STATUS_SUCCESS;
    struct cb_user_per_sym_ctx *p_loc_sym_cb_ctx = &p_dev_ctx->symCbCtx[symb][XRAN_CB_SYM_TX_WIN_BEGIN];
    uint32_t time_diff_us      = 0;
    uint32_t time_diff_nSymb   = 0;
    uint32_t absolute_ota_sym  = 0;
    uint32_t interval_us_local = p_dev_ctx->interval_us_local;

    if(p_loc_sym_cb_ctx->status) {
        ret =  XRAN_STATUS_RESOURCE;
        print_err("timer sym %d type id %d was already created",symb, XRAN_CB_SYM_TX_WIN_BEGIN);
        return ret;
    }

    time_diff_us = p_dev_ctx->fh_cfg.T1a_max_up;
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

    p_loc_sym_cb_ctx->status        = 1;

    *p_sym_cb_ctx  =p_loc_sym_cb_ctx;

    return ret;
}

int32_t
xran_reg_sym_cb(void *pHandle, xran_callback_sym_fn symCb, void * symCbParam, struct xran_sense_of_time* symCbTime, uint8_t symb, enum cb_per_sym_type_id cb_sym_t_id)
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

    switch (cb_sym_t_id) {
        case XRAN_CB_SYM_OTA_TIME:
            ret = xran_reg_sym_cb_ota(p_dev_ctx, symCb, symCbParam, symCbTime, symb, &p_sym_cb_ctx);
            if(ret != XRAN_STATUS_SUCCESS)
                return ret;
            dpdk_cb_to_arm = rx_ul_user_sym_cb;
        break;
        case XRAN_CB_SYM_RX_WIN_BEGIN:
            ret = xran_reg_sym_cb_rx_win_begin(p_dev_ctx, symCb, symCbParam, symCbTime, symb, &p_sym_cb_ctx);
            if(ret != XRAN_STATUS_SUCCESS)
                return ret;
            dpdk_cb_to_arm = rx_ul_user_sym_cb;
            break;
        case XRAN_CB_SYM_RX_WIN_END:
            ret = xran_reg_sym_cb_rx_win_end(p_dev_ctx, symCb, symCbParam, symCbTime, symb, &p_sym_cb_ctx);
            if(ret != XRAN_STATUS_SUCCESS)
                return ret;
            dpdk_cb_to_arm = rx_ul_user_sym_cb;
        break;
        case XRAN_CB_SYM_TX_WIN_BEGIN:
            ret = xran_reg_sym_cb_tx_win_begin(p_dev_ctx, symCb, symCbParam, symCbTime, symb, &p_sym_cb_ctx);
            if(ret != XRAN_STATUS_SUCCESS)
                return ret;
            dpdk_cb_to_arm = rx_ul_user_sym_cb;
            break;
        case XRAN_CB_SYM_TX_WIN_END:
            ret = xran_reg_sym_cb_tx_win_end(p_dev_ctx, symCb, symCbParam, symCbTime, symb, &p_sym_cb_ctx);
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
        LIST_INSERT_HEAD(&p_dev_ctx->sym_cb_list_head[p_sym_cb_ctx->symb_num_ota],
                            cb_elm,
                            pointers);
    } else {
        print_err("cb_elm is NULL\n");
        ret =  XRAN_STATUS_FAIL;
        return ret;
    }

    return ret;
}

int32_t
xran_reg_physide_cb(void *pHandle, xran_fh_tti_callback_fn Cb, void *cbParam, int skipTtiNum, enum callback_to_phy_id id)
{
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();

    if(xran_get_if_state() == XRAN_RUNNING) {
        print_err("Cannot register callback while running!!\n");
        return (-1);
    }

    p_xran_dev_ctx->ttiCb[id]      = Cb;
    p_xran_dev_ctx->TtiCbParam[id] = cbParam;
    p_xran_dev_ctx->SkipTti[id]    = skipTtiNum;

    return 0;
}




