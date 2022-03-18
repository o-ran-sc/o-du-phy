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
 * @brief XRAN main functionality module
 * @file xran_main.c
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#define _GNU_SOURCE
#include <sched.h>
#include <assert.h>
#include <err.h>
#include <libgen.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <malloc.h>
#include <immintrin.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_version.h>
#include <rte_flow.h>
#if (RTE_VER_YEAR >= 21) /* eCPRI flow supported with DPDK 21.02 or later */
#include <rte_ecpri.h>
#endif
#include "xran_fh_o_du.h"
#include "xran_main.h"

#include "ethdi.h"
#include "xran_mem_mgr.h"
#include "xran_tx_proc.h"
#include "xran_rx_proc.h"
#include "xran_pkt.h"
#include "xran_up_api.h"
#include "xran_cp_api.h"
#include "xran_sync_api.h"
#include "xran_lib_mlog_tasks_id.h"
#include "xran_timer.h"
#include "xran_common.h"
#include "xran_dev.h"
#include "xran_frame_struct.h"
#include "xran_printf.h"
#include "xran_app_frag.h"
#include "xran_cp_proc.h"
#include "xran_tx_proc.h"
#include "xran_rx_proc.h"
#include "xran_cb_proc.h"
#include "xran_ecpri_owd_measurements.h"

#include "xran_mlog_lnx.h"

static xran_cc_handle_t pLibInstanceHandles[XRAN_PORTS_NUM][XRAN_MAX_SECTOR_NR] = {NULL};

uint64_t interval_us = 1000; //the TTI interval of the cell with maximum numerology

uint32_t xran_lib_ota_tti[XRAN_PORTS_NUM] = {0,0,0,0}; /**< Slot index in a second [0:(1000000/TTI-1)] */
uint32_t xran_lib_ota_sym[XRAN_PORTS_NUM] = {0,0,0,0}; /**< Symbol index in a slot [0:13] */
uint32_t xran_lib_ota_sym_idx[XRAN_PORTS_NUM] = {0,0,0,0}; /**< Symbol index in a second [0 : 14*(1000000/TTI)-1]
                                                where TTI is TTI interval in microseconds */

uint16_t xran_SFN_at_Sec_Start   = 0; /**< SFN at current second start */
uint16_t xran_max_frame          = 1023; /**< value of max frame used. expected to be 99 (old compatibility mode) and 1023 as per section 9.7.2	System Frame Number Calculation */

static uint64_t xran_total_tick = 0, xran_used_tick = 0;
static uint32_t xran_num_cores_used = 0;
static uint32_t xran_core_used[64] = {0};
static int32_t first_call = 0;

struct cp_up_tx_desc * xran_pkt_gen_desc_alloc(void);
int32_t xran_pkt_gen_desc_free(struct cp_up_tx_desc *p_desc);

void tti_ota_cb(struct rte_timer *tim, void *arg);
void tti_to_phy_cb(struct rte_timer *tim, void *arg);

int32_t xran_pkt_gen_process_ring(struct rte_ring *r);

void
xran_updateSfnSecStart(void)
{
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    struct xran_common_counters * pCnt = &p_xran_dev_ctx->fh_counters;
    int32_t xran_ports  = p_xran_dev_ctx->fh_init.xran_ports;
    int32_t o_xu_id = 0;
    uint64_t currentSecond = timing_get_current_second();
    // Assume always positive
    uint64_t gpsSecond = currentSecond - UNIX_TO_GPS_SECONDS_OFFSET;
    uint64_t nFrames = gpsSecond * NUM_OF_FRAMES_PER_SECOND;
    uint16_t sfn = (uint16_t)(nFrames % (xran_max_frame + 1));
    xran_SFN_at_Sec_Start = sfn;

    for(o_xu_id = 0; o_xu_id < xran_ports; o_xu_id++){
    pCnt->tx_bytes_per_sec = pCnt->tx_bytes_counter;
    pCnt->rx_bytes_per_sec = pCnt->rx_bytes_counter;
    pCnt->tx_bytes_counter = 0;
    pCnt->rx_bytes_counter = 0;
        p_xran_dev_ctx++;
        pCnt = &p_xran_dev_ctx->fh_counters;
    }
}

static inline int32_t
xran_getSlotIdxSecond(uint32_t interval)
{
    int32_t frameIdxSecond = xran_getSfnSecStart();
    int32_t slotIndxSecond = frameIdxSecond * SLOTS_PER_SYSTEMFRAME(interval);
    return slotIndxSecond;
}

enum xran_if_state
xran_get_if_state(void)
        {
    return xran_if_current_state;
}

int32_t xran_is_prach_slot(uint8_t PortId, uint32_t subframe_id, uint32_t slot_id)
{
    int32_t is_prach_slot = 0;
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx_by_id(PortId);
    if (p_xran_dev_ctx == NULL)
{
        print_err("PortId %d not exist\n", PortId);
        return is_prach_slot;
}
    struct xran_prach_cp_config *pPrachCPConfig = &(p_xran_dev_ctx->PrachCPConfig);
    uint8_t nNumerology = xran_get_conf_numerology(p_xran_dev_ctx);

    if (nNumerology < 2){
        //for FR1, in 38.211 tab 6.3.3.2-2&3 it is subframe index
        if (pPrachCPConfig->isPRACHslot[subframe_id] == 1){
            if (pPrachCPConfig->nrofPrachInSlot == 0){
                if(slot_id == 0)
                    is_prach_slot = 1;
            }
            else if (pPrachCPConfig->nrofPrachInSlot == 2)
                is_prach_slot = 1;
            else{
                if (nNumerology == 0)
                    is_prach_slot = 1;
                else if (slot_id == 1)
                    is_prach_slot = 1;
            }
        }
    } else if (nNumerology == 3){
        //for FR2, 38.211 tab 6.3.3.4 it is slot index of 60kHz slot
        uint32_t slotidx;
        slotidx = subframe_id * SLOTNUM_PER_SUBFRAME(p_xran_dev_ctx->interval_us_local) + slot_id;
        if (pPrachCPConfig->nrofPrachInSlot == 2){
            if (pPrachCPConfig->isPRACHslot[slotidx>>1] == 1)
                is_prach_slot = 1;
        } else {
            if ((pPrachCPConfig->isPRACHslot[slotidx>>1] == 1) && ((slotidx % 2) == 1)){
                is_prach_slot = 1;
            }
        }
    } else
        print_err("Numerology %d not supported", nNumerology);
    return is_prach_slot;
}

int32_t
xran_init_srs(struct xran_fh_config* pConf, struct xran_device_ctx * p_xran_dev_ctx)
{
    struct xran_srs_config *p_srs = &(p_xran_dev_ctx->srs_cfg);

    if(p_srs){
        p_srs->symbMask = pConf->srs_conf.symbMask;
        p_srs->eAxC_offset = pConf->srs_conf.eAxC_offset;
        print_dbg("SRS sym         %d\n", p_srs->symbMask );
        print_dbg("SRS eAxC_offset %d\n", p_srs->eAxC_offset);
    }
    return (XRAN_STATUS_SUCCESS);
}

int32_t
xran_init_prach_lte(struct xran_fh_config* pConf, struct xran_device_ctx * p_xran_dev_ctx)
{
    /* update Rach for LTE */
    return xran_init_prach(pConf, p_xran_dev_ctx);
}

int32_t
xran_init_prach(struct xran_fh_config* pConf, struct xran_device_ctx * p_xran_dev_ctx)
{
    int32_t i;
    uint8_t slotNr;
    struct xran_prach_config* pPRACHConfig = &(pConf->prach_conf);
    const xRANPrachConfigTableStruct *pxRANPrachConfigTable;
    uint8_t nNumerology = pConf->frame_conf.nNumerology;
    uint8_t nPrachConfIdx = pPRACHConfig->nPrachConfIdx;
    struct xran_prach_cp_config *pPrachCPConfig = &(p_xran_dev_ctx->PrachCPConfig);

    if (nNumerology > 2)
        pxRANPrachConfigTable = &gxranPrachDataTable_mmw[nPrachConfIdx];
    else if (pConf->frame_conf.nFrameDuplexType == 1)
        pxRANPrachConfigTable = &gxranPrachDataTable_sub6_tdd[nPrachConfIdx];
    else
        pxRANPrachConfigTable = &gxranPrachDataTable_sub6_fdd[nPrachConfIdx];

    uint8_t preambleFmrt = pxRANPrachConfigTable->preambleFmrt[0];
    const xRANPrachPreambleLRAStruct *pxranPreambleforLRA = &gxranPreambleforLRA[preambleFmrt];
    memset(pPrachCPConfig, 0, sizeof(struct xran_prach_cp_config));
    if(pConf->log_level)
        printf("xRAN open PRACH config: Numerology %u ConfIdx %u, preambleFmrt %u startsymb %u, numSymbol %u, occassionsInPrachSlot %u\n", nNumerology, nPrachConfIdx, preambleFmrt, pxRANPrachConfigTable->startingSym, pxRANPrachConfigTable->duration, pxRANPrachConfigTable->occassionsInPrachSlot);

    pPrachCPConfig->filterIdx = XRAN_FILTERINDEX_PRACH_ABC;         // 3, PRACH preamble format A1~3, B1~4, C0, C2
    pPrachCPConfig->startSymId = pxRANPrachConfigTable->startingSym;
    pPrachCPConfig->startPrbc = pPRACHConfig->nPrachFreqStart;
    pPrachCPConfig->numPrbc = (preambleFmrt >= FORMAT_A1)? 12 : 70;
    pPrachCPConfig->timeOffset = pxranPreambleforLRA->nRaCp;
    pPrachCPConfig->freqOffset = xran_get_freqoffset(pPRACHConfig->nPrachFreqOffset, pPRACHConfig->nPrachSubcSpacing);
    pPrachCPConfig->x = pxRANPrachConfigTable->x;
    pPrachCPConfig->nrofPrachInSlot = pxRANPrachConfigTable->nrofPrachInSlot;
    pPrachCPConfig->y[0] = pxRANPrachConfigTable->y[0];
    pPrachCPConfig->y[1] = pxRANPrachConfigTable->y[1];
    if (preambleFmrt >= FORMAT_A1)
    {
        pPrachCPConfig->numSymbol = pxRANPrachConfigTable->duration;
        pPrachCPConfig->occassionsInPrachSlot = pxRANPrachConfigTable->occassionsInPrachSlot;
    }
    else
    {
        pPrachCPConfig->numSymbol = 1;
        pPrachCPConfig->occassionsInPrachSlot = 1;
    }

    if(pConf->log_level)
        printf("PRACH: x %u y[0] %u, y[1] %u prach slot: %u ..", pPrachCPConfig->x, pPrachCPConfig->y[0], pPrachCPConfig->y[1], pxRANPrachConfigTable->slotNr[0]);
    pPrachCPConfig->isPRACHslot[pxRANPrachConfigTable->slotNr[0]] = 1;
    for (i=1; i < XRAN_PRACH_CANDIDATE_SLOT; i++)
    {
        slotNr = pxRANPrachConfigTable->slotNr[i];
        if (slotNr > 0){
            pPrachCPConfig->isPRACHslot[slotNr] = 1;
            if(pConf->log_level)
                printf(" %u ..", slotNr);
        }
    }
    printf("\n");
    for (i = 0; i < XRAN_MAX_SECTOR_NR; i++){
        p_xran_dev_ctx->prach_start_symbol[i] = pPrachCPConfig->startSymId;
        p_xran_dev_ctx->prach_last_symbol[i] = pPrachCPConfig->startSymId + pPrachCPConfig->numSymbol * pPrachCPConfig->occassionsInPrachSlot - 1;
    }
    if(pConf->log_level){
        printf("PRACH start symbol %u lastsymbol %u\n", p_xran_dev_ctx->prach_start_symbol[0], p_xran_dev_ctx->prach_last_symbol[0]);
    }

    pPrachCPConfig->eAxC_offset = xran_get_num_eAxc(p_xran_dev_ctx);
    print_dbg("PRACH eAxC_offset %d\n",  pPrachCPConfig->eAxC_offset);

    /* Save some configs for app */
    pPRACHConfig->startSymId    = pPrachCPConfig->startSymId;
    pPRACHConfig->lastSymId     = pPrachCPConfig->startSymId + pPrachCPConfig->numSymbol * pPrachCPConfig->occassionsInPrachSlot - 1;
    pPRACHConfig->startPrbc     = pPrachCPConfig->startPrbc;
    pPRACHConfig->numPrbc       = pPrachCPConfig->numPrbc;
    pPRACHConfig->timeOffset    = pPrachCPConfig->timeOffset;
    pPRACHConfig->freqOffset    = pPrachCPConfig->freqOffset;
    pPRACHConfig->eAxC_offset   = pPrachCPConfig->eAxC_offset;

        return (XRAN_STATUS_SUCCESS);
        }

uint32_t
xran_slotid_convert(uint16_t slot_id, uint16_t dir) //dir = 0, from PHY slotid to xran spec slotid as defined in 5.3.2, dir=1, from xran slotid to phy slotid
{
    return slot_id;
#if 0
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    uint8_t mu = p_xran_dev_ctx->fh_cfg.frame_conf.nNumerology;
    uint8_t FR = 1;
    if (mu > 2)
        FR=2;
    if (dir == 0)
    {
        if (FR == 1)
        {
            return (slot_id << (2-mu));
        }
        else
        {
            return (slot_id << (3-mu));
        }
    }
    else
    {
        if (FR == 1)
        {
            return (slot_id >> (2-mu));
        }
        else
        {
            return (slot_id >> (3-mu));
        }
    }
#endif
}

void
sym_ota_cb(struct rte_timer *tim, void *arg, unsigned long *used_tick)
{
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)arg;
    long t1 = MLogTick(), t2;
    long t3;

    if(XranGetSymNum(xran_lib_ota_sym_idx[p_xran_dev_ctx->xran_port_id], XRAN_NUM_OF_SYMBOL_PER_SLOT) == 0){
        t3 = xran_tick();
        tti_ota_cb(NULL, (void*)p_xran_dev_ctx);
        *used_tick += get_ticks_diff(xran_tick(), t3);
    }

            t3 = xran_tick();
    if (xran_process_tx_sym(p_xran_dev_ctx))
    {
        *used_tick += get_ticks_diff(xran_tick(), t3);
    }

    /* check if there is call back to do something else on this symbol */
    struct cb_elem_entry *cb_elm;
    LIST_FOREACH(cb_elm, &p_xran_dev_ctx->sym_cb_list_head[xran_lib_ota_sym[p_xran_dev_ctx->xran_port_id]], pointers){
        if(cb_elm){
            cb_elm->pSymCallback(&p_xran_dev_ctx->dpdk_timer[p_xran_dev_ctx->ctx % MAX_NUM_OF_DPDK_TIMERS], cb_elm->pSymCallbackTag, cb_elm->p_dev_ctx);
            p_xran_dev_ctx->ctx = DpdkTimerIncrementCtx(p_xran_dev_ctx->ctx);
        }
    }

    t2 = MLogTick();
    MLogTask(PID_SYM_OTA_CB, t1, t2);
}

uint32_t
xran_schedule_to_worker(enum xran_job_type_id job_type_id, struct xran_device_ctx * p_xran_dev_ctx)
{
    struct xran_ethdi_ctx* eth_ctx = xran_ethdi_get_ctx();
    uint32_t tim_lcore = eth_ctx->io_cfg.timing_core; /* default to timing core */

    if(eth_ctx) {
        if(eth_ctx->num_workers == 0) { /* no workers */
            tim_lcore = eth_ctx->io_cfg.timing_core;
        } else if (eth_ctx->num_workers == 1) { /* one worker */
            switch (job_type_id)
            {
                case XRAN_JOB_TYPE_OTA_CB:
                    tim_lcore = eth_ctx->io_cfg.timing_core;
                    break;
                case XRAN_JOB_TYPE_CP_DL:
                case XRAN_JOB_TYPE_CP_UL:
                case XRAN_JOB_TYPE_DEADLINE:
                case XRAN_JOB_TYPE_SYM_CB:
                    tim_lcore = eth_ctx->worker_core[0];
                    break;
                default:
                    print_err("incorrect job type id %d\n", job_type_id);
                    tim_lcore = eth_ctx->io_cfg.timing_core;
                    break;
            }
        } else if (eth_ctx->num_workers >= 2 && eth_ctx->num_workers <= 6) {
            switch (job_type_id)
            {
                case XRAN_JOB_TYPE_OTA_CB:
                    tim_lcore = eth_ctx->worker_core[0];
                    break;
                case XRAN_JOB_TYPE_CP_DL:
                    tim_lcore = eth_ctx->worker_core[p_xran_dev_ctx->job2wrk_id[XRAN_JOB_TYPE_CP_DL]];
                    break;
                case XRAN_JOB_TYPE_CP_UL:
                    tim_lcore = eth_ctx->worker_core[p_xran_dev_ctx->job2wrk_id[XRAN_JOB_TYPE_CP_UL]];
                    break;
                case XRAN_JOB_TYPE_DEADLINE:
                case XRAN_JOB_TYPE_SYM_CB:
                    tim_lcore = eth_ctx->worker_core[0];
                    break;
                default:
                    print_err("incorrect job type id %d\n", job_type_id);
                    tim_lcore = eth_ctx->io_cfg.timing_core;
                    break;
            }
        } else {
            print_err("incorrect eth_ctx->num_workers id %d\n", eth_ctx->num_workers);
            tim_lcore = eth_ctx->io_cfg.timing_core;
        }
    }

    return tim_lcore;
}

void
tti_ota_cb(struct rte_timer *tim, void *arg)
{
    uint32_t    frame_id    = 0;
    uint32_t    subframe_id = 0;
    uint32_t    slot_id     = 0;
    uint32_t    next_tti    = 0;

    uint32_t mlogVar[10];
    uint32_t mlogVarCnt = 0;
    uint64_t t1 = MLogTick();
    uint64_t t3 = 0;
    uint32_t reg_tti  = 0;
    uint32_t reg_sfn  = 0;
    uint32_t i;

    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)arg;
    struct xran_timer_ctx *pTCtx = (struct xran_timer_ctx *)p_xran_dev_ctx->timer_ctx;
    uint8_t PortId = p_xran_dev_ctx->xran_port_id;
    uint32_t interval_us_local = p_xran_dev_ctx->interval_us_local;

    unsigned tim_lcore =  xran_schedule_to_worker(XRAN_JOB_TYPE_OTA_CB, p_xran_dev_ctx);

    MLogTask(PID_TTI_TIMER, t1, MLogTick());

    if(p_xran_dev_ctx->xran_port_id == 0){
    /* To match TTbox */
        if(xran_lib_ota_tti[0] == 0)
            reg_tti = xran_fs_get_max_slot(PortId) - 1;
    else
            reg_tti = xran_lib_ota_tti[0] -1;

    MLogIncrementCounter();
        reg_sfn    = XranGetFrameNum(reg_tti,xran_getSfnSecStart(),SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval_us))*10 + XranGetSubFrameNum(reg_tti,SLOTNUM_PER_SUBFRAME(interval_us), SUBFRAMES_PER_SYSTEMFRAME);;
    /* subframe and slot */
        MLogRegisterFrameSubframe(reg_sfn, reg_tti % (SLOTNUM_PER_SUBFRAME(interval_us)));
    MLogMark(1, t1);
    }

    slot_id     = XranGetSlotNum(xran_lib_ota_tti[PortId], SLOTNUM_PER_SUBFRAME(interval_us_local));
    subframe_id = XranGetSubFrameNum(xran_lib_ota_tti[PortId], SLOTNUM_PER_SUBFRAME(interval_us_local),  SUBFRAMES_PER_SYSTEMFRAME);
    frame_id    = XranGetFrameNum(xran_lib_ota_tti[PortId],xran_getSfnSecStart(),SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval_us_local));

    pTCtx[(xran_lib_ota_tti[PortId] & 1) ^ 1].tti_to_process = xran_lib_ota_tti[PortId];

    mlogVar[mlogVarCnt++] = 0x11111111;
    mlogVar[mlogVarCnt++] = xran_lib_ota_tti[PortId];
    mlogVar[mlogVarCnt++] = xran_lib_ota_sym_idx[PortId];
    mlogVar[mlogVarCnt++] = xran_lib_ota_sym_idx[PortId] / 14;
    mlogVar[mlogVarCnt++] = frame_id;
    mlogVar[mlogVarCnt++] = subframe_id;
    mlogVar[mlogVarCnt++] = slot_id;
    mlogVar[mlogVarCnt++] = 0;
    MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());


    if(p_xran_dev_ctx->fh_init.io_cfg.id == ID_O_DU)
        next_tti = xran_lib_ota_tti[PortId] + 1;
    else{
        next_tti = xran_lib_ota_tti[PortId];
    }

    if(next_tti>= xran_fs_get_max_slot(PortId)){
        print_dbg("[%d]SFN %d sf %d slot %d\n",next_tti, frame_id, subframe_id, slot_id);
        next_tti=0;
    }

    slot_id     = XranGetSlotNum(next_tti, SLOTNUM_PER_SUBFRAME(interval_us_local));
    subframe_id = XranGetSubFrameNum(next_tti,SLOTNUM_PER_SUBFRAME(interval_us_local),  SUBFRAMES_PER_SYSTEMFRAME);
    frame_id    = XranGetFrameNum(next_tti,xran_getSfnSecStart(),SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval_us_local));

    print_dbg("[%d]SFN %d sf %d slot %d\n",next_tti, frame_id, subframe_id, slot_id);

    if(p_xran_dev_ctx->fh_init.io_cfg.id == ID_O_DU){
        pTCtx[(xran_lib_ota_tti[PortId] & 1)].tti_to_process = next_tti;
    } else {
        pTCtx[(xran_lib_ota_tti[PortId] & 1)].tti_to_process = pTCtx[(xran_lib_ota_tti[PortId] & 1)^1].tti_to_process;
    }

    if(p_xran_dev_ctx->ttiCb[XRAN_CB_TTI]) {
    p_xran_dev_ctx->phy_tti_cb_done = 0;
        xran_timer_arm_ex(&p_xran_dev_ctx->tti_to_phy_timer[xran_lib_ota_tti[PortId] % MAX_TTI_TO_PHY_TIMER], tti_to_phy_cb, (void*)p_xran_dev_ctx, tim_lcore);
    }
    //slot index is increased to next slot at the beginning of current OTA slot
    xran_lib_ota_tti[PortId]++;
    if(xran_lib_ota_tti[PortId] >= xran_fs_get_max_slot(PortId)) {
        print_dbg("[%d]SFN %d sf %d slot %d\n",xran_lib_ota_tti[PortId], frame_id, subframe_id, slot_id);
        xran_lib_ota_tti[PortId] = 0;
    }
    MLogTask(PID_TTI_CB, t1, MLogTick());
}

void
tx_cp_dl_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogTick();
    int tti, buf_id;
    uint32_t slot_id, subframe_id, frame_id;
    int cc_id;
    uint8_t ctx_id;
    uint8_t ant_id, num_eAxc, num_CCPorts;
    void *pHandle;
    int num_list;
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)arg;
    if(!p_xran_dev_ctx)
    {
        print_err("Null xRAN context!!\n");
        return;
    }
    struct xran_timer_ctx *pTCtx = (struct xran_timer_ctx *)&p_xran_dev_ctx->timer_ctx[0];
    uint32_t interval_us_local = p_xran_dev_ctx->interval_us_local;
    uint8_t PortId = p_xran_dev_ctx->xran_port_id;
    pHandle     = p_xran_dev_ctx;

    num_eAxc    = xran_get_num_eAxc(pHandle);
    num_CCPorts = xran_get_num_cc(pHandle);

    if(first_call && p_xran_dev_ctx->enableCP) {

        tti = pTCtx[(xran_lib_ota_tti[PortId] & 1) ^ 1].tti_to_process;
        buf_id = tti % XRAN_N_FE_BUF_LEN;

        slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME(interval_us_local));
        subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME(interval_us_local),  SUBFRAMES_PER_SYSTEMFRAME);
        frame_id    = XranGetFrameNum(tti,xran_getSfnSecStart(),SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval_us_local));
        if (tti == 0){
            /* Wrap around to next second */
            frame_id = (frame_id + NUM_OF_FRAMES_PER_SECOND) & 0x3ff;
        }

        ctx_id      = XranGetSlotNum(tti, SLOTS_PER_SYSTEMFRAME(interval_us_local)) % XRAN_MAX_SECTIONDB_CTX;

        print_dbg("[%d]SFN %d sf %d slot %d\n", tti, frame_id, subframe_id, slot_id);
        for(ant_id = 0; ant_id < num_eAxc; ++ant_id) {
            for(cc_id = 0; cc_id < num_CCPorts; cc_id++ ) {
                /* start new section information list */
                xran_cp_reset_section_info(pHandle, XRAN_DIR_DL, cc_id, ant_id, ctx_id);
                if(xran_fs_get_slot_type(PortId, cc_id, tti, XRAN_SLOT_TYPE_DL) == 1) {
                    if(p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[buf_id][cc_id][ant_id].sBufferList.pBuffers) {
                    if(p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[buf_id][cc_id][ant_id].sBufferList.pBuffers->pData){
                        num_list = xran_cp_create_and_send_section(pHandle, ant_id, XRAN_DIR_DL, tti, cc_id,
                            (struct xran_prb_map *)p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[buf_id][cc_id][ant_id].sBufferList.pBuffers->pData,
                            p_xran_dev_ctx->fh_cfg.ru_conf.xranCat, ctx_id);
                    } else {
                            print_err("[%d]SFN %d sf %d slot %d: ant_id %d cc_id %d [pData]\n", tti, frame_id, subframe_id, slot_id, ant_id, cc_id);
                        }
                    } else {
                        print_err("[%d]SFN %d sf %d slot %d: ant_id %d cc_id %d [pBuffers] \n", tti, frame_id, subframe_id, slot_id, ant_id, cc_id);
                    }
                } /* if(xran_fs_get_slot_type(cc_id, tti, XRAN_SLOT_TYPE_DL) == 1) */
            } /* for(cc_id = 0; cc_id < num_CCPorts; cc_id++) */
        } /* for(ant_id = 0; ant_id < num_eAxc; ++ant_id) */
        MLogTask(PID_CP_DL_CB, t1, MLogTick());
    }
}

void
rx_ul_deadline_half_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogTick();
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)arg;
    xran_status_t status;
    /* half of RX for current TTI as measured against current OTA time */
    int32_t rx_tti;
    int32_t cc_id;
    uint32_t nFrameIdx;
    uint32_t nSubframeIdx;
    uint32_t nSlotIdx;
    uint64_t nSecond;
    struct xran_timer_ctx* p_timer_ctx = NULL;
    /*xran_get_slot_idx(&nFrameIdx, &nSubframeIdx, &nSlotIdx, &nSecond);
    rx_tti = nFrameIdx*SUBFRAMES_PER_SYSTEMFRAME*SLOTNUM_PER_SUBFRAME
           + nSubframeIdx*SLOTNUM_PER_SUBFRAME
           + nSlotIdx;*/
    if(p_xran_dev_ctx->xran2phy_mem_ready == 0)
        return;

    p_timer_ctx = &p_xran_dev_ctx->cb_timer_ctx[p_xran_dev_ctx->timer_put++ % MAX_CB_TIMER_CTX];
    if (p_xran_dev_ctx->timer_put >= MAX_CB_TIMER_CTX)
        p_xran_dev_ctx->timer_put = 0;

    rx_tti = p_timer_ctx->tti_to_process;

    for(cc_id = 0; cc_id < xran_get_num_cc(p_xran_dev_ctx); cc_id++) {
        if(p_xran_dev_ctx->rx_packet_callback_tracker[rx_tti % XRAN_N_FE_BUF_LEN][cc_id] == 0){
            if(p_xran_dev_ctx->pCallback[cc_id]) {
            struct xran_cb_tag *pTag = p_xran_dev_ctx->pCallbackTag[cc_id];
                if(pTag) {
                    //pTag->cellId = cc_id;
            pTag->slotiId = rx_tti;
            pTag->symbol  = 0; /* last 7 sym means full slot of Symb */
            status = XRAN_STATUS_SUCCESS;

               p_xran_dev_ctx->pCallback[cc_id](p_xran_dev_ctx->pCallbackTag[cc_id], status);
                }
            }
        } else {
            p_xran_dev_ctx->rx_packet_callback_tracker[rx_tti % XRAN_N_FE_BUF_LEN][cc_id] = 0;
        }
    }

    if(p_xran_dev_ctx->ttiCb[XRAN_CB_HALF_SLOT_RX]){
        if(p_xran_dev_ctx->SkipTti[XRAN_CB_HALF_SLOT_RX] <= 0){
            p_xran_dev_ctx->ttiCb[XRAN_CB_HALF_SLOT_RX](p_xran_dev_ctx->TtiCbParam[XRAN_CB_HALF_SLOT_RX]);
        }else{
            p_xran_dev_ctx->SkipTti[XRAN_CB_HALF_SLOT_RX]--;
        }
    }

    MLogTask(PID_UP_UL_HALF_DEAD_LINE_CB, t1, MLogTick());
}

void
rx_ul_deadline_full_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogTick();
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)arg;
    xran_status_t status = 0;
    int32_t rx_tti = 0;// = (int32_t)XranGetTtiNum(xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
    int32_t cc_id = 0;
    uint32_t nFrameIdx;
    uint32_t nSubframeIdx;
    uint32_t nSlotIdx;
    uint64_t nSecond;
    struct xran_timer_ctx* p_timer_ctx = NULL;

    if(p_xran_dev_ctx->xran2phy_mem_ready == 0)
        return;

    /*xran_get_slot_idx(&nFrameIdx, &nSubframeIdx, &nSlotIdx, &nSecond);
    rx_tti = nFrameIdx*SUBFRAMES_PER_SYSTEMFRAME*SLOTNUM_PER_SUBFRAME
        + nSubframeIdx*SLOTNUM_PER_SUBFRAME
        + nSlotIdx;*/
    p_timer_ctx = &p_xran_dev_ctx->cb_timer_ctx[p_xran_dev_ctx->timer_put++ % MAX_CB_TIMER_CTX];

    if (p_xran_dev_ctx->timer_put >= MAX_CB_TIMER_CTX)
        p_xran_dev_ctx->timer_put = 0;

    rx_tti = p_timer_ctx->tti_to_process;
#if 1
    if(rx_tti == 0)
       rx_tti = (xran_fs_get_max_slot_SFN(p_xran_dev_ctx->xran_port_id)-1);
    else
       rx_tti -= 1; /* end of RX for prev TTI as measured against current OTA time */
#endif
    /* U-Plane */
    for(cc_id = 0; cc_id < xran_get_num_cc(p_xran_dev_ctx); cc_id++) {
        if(p_xran_dev_ctx->pCallback[cc_id]){
        struct xran_cb_tag *pTag = p_xran_dev_ctx->pCallbackTag[cc_id];
            if(pTag) {
                //pTag->cellId = cc_id;
        pTag->slotiId = rx_tti;
        pTag->symbol  = 7; /* last 7 sym means full slot of Symb */
        status = XRAN_STATUS_SUCCESS;
            p_xran_dev_ctx->pCallback[cc_id](p_xran_dev_ctx->pCallbackTag[cc_id], status);
            }
        }

        if(p_xran_dev_ctx->pPrachCallback[cc_id]){
            struct xran_cb_tag *pTag = p_xran_dev_ctx->pPrachCallbackTag[cc_id];
            if(pTag) {
                //pTag->cellId = cc_id;
            pTag->slotiId = rx_tti;
            pTag->symbol  = 7; /* last 7 sym means full slot of Symb */
            p_xran_dev_ctx->pPrachCallback[cc_id](p_xran_dev_ctx->pPrachCallbackTag[cc_id], status);
        }
        }

        if(p_xran_dev_ctx->pSrsCallback[cc_id]){
            struct xran_cb_tag *pTag = p_xran_dev_ctx->pSrsCallbackTag[cc_id];
            if(pTag) {
                //pTag->cellId = cc_id;
            pTag->slotiId = rx_tti;
            pTag->symbol  = 7; /* last 7 sym means full slot of Symb */
            p_xran_dev_ctx->pSrsCallback[cc_id](p_xran_dev_ctx->pSrsCallbackTag[cc_id], status);
        }
    }
    }

    /* user call backs if any */
    if(p_xran_dev_ctx->ttiCb[XRAN_CB_FULL_SLOT_RX]){
        if(p_xran_dev_ctx->SkipTti[XRAN_CB_FULL_SLOT_RX] <= 0){
            p_xran_dev_ctx->ttiCb[XRAN_CB_FULL_SLOT_RX](p_xran_dev_ctx->TtiCbParam[XRAN_CB_FULL_SLOT_RX]);
        }else{
            p_xran_dev_ctx->SkipTti[XRAN_CB_FULL_SLOT_RX]--;
        }
    }

    MLogTask(PID_UP_UL_FULL_DEAD_LINE_CB, t1, MLogTick());
}

void
rx_ul_user_sym_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogTick();
    struct xran_device_ctx * p_dev_ctx = NULL;
    struct cb_user_per_sym_ctx *p_sym_cb_ctx = (struct cb_user_per_sym_ctx *)arg;
    xran_status_t status = 0;
    int32_t rx_tti = 0; //(int32_t)XranGetTtiNum(xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
    int32_t cc_id = 0;
    uint32_t nFrameIdx;
    uint32_t nSubframeIdx;
    uint32_t nSlotIdx;
    uint64_t nSecond;
    uint32_t interval, ota_sym_idx = 0;
    uint8_t nNumerology = 0;
    struct xran_timer_ctx* p_timer_ctx =  NULL;

    if(p_sym_cb_ctx->p_dev)
        p_dev_ctx = (struct xran_device_ctx *)p_sym_cb_ctx->p_dev;
    else
        rte_panic("p_sym_cb_ctx->p_dev == NULL");

    if(p_dev_ctx->xran2phy_mem_ready == 0)
        return;
    nNumerology = xran_get_conf_numerology(p_dev_ctx);
    interval = p_dev_ctx->interval_us_local;

    p_timer_ctx = &p_sym_cb_ctx->user_cb_timer_ctx[p_sym_cb_ctx->user_timer_get++ % MAX_CB_TIMER_CTX];
    if (p_sym_cb_ctx->user_timer_get >= MAX_CB_TIMER_CTX)
        p_sym_cb_ctx->user_timer_get = 0;

    rx_tti = p_timer_ctx->tti_to_process;

    if( p_sym_cb_ctx->sym_diff > 0)
        /* + advacne TX Wind: at OTA Time we indicating event in future */
        ota_sym_idx = ((p_timer_ctx->ota_sym_idx + p_sym_cb_ctx->sym_diff) % xran_max_ota_sym_idx(nNumerology));
    else if (p_sym_cb_ctx->sym_diff < 0) {
        /* - dealy RX Win: at OTA Time we indicate event in the past */
        if(p_timer_ctx->ota_sym_idx >= abs(p_sym_cb_ctx->sym_diff)) {
            ota_sym_idx = p_timer_ctx->ota_sym_idx + p_sym_cb_ctx->sym_diff;
        } else {
            ota_sym_idx = ((xran_max_ota_sym_idx(nNumerology) + p_timer_ctx->ota_sym_idx) + p_sym_cb_ctx->sym_diff) % xran_max_ota_sym_idx(nNumerology);
        }
    } else /* 0 - OTA exact time */
        ota_sym_idx = p_timer_ctx->ota_sym_idx;

    rx_tti = (int32_t)XranGetTtiNum(ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);

    if(p_sym_cb_ctx->symCbTimeInfo) {
            struct xran_sense_of_time *p_sense_time = p_sym_cb_ctx->symCbTimeInfo;
            p_sense_time->type_of_event = p_sym_cb_ctx->cb_type_id;
            p_sense_time->nSymIdx       = p_sym_cb_ctx->symb_num_req;
            p_sense_time->tti_counter   = rx_tti;
            p_sense_time->nSlotIdx      = (uint32_t)XranGetSlotNum(rx_tti, SLOTNUM_PER_SUBFRAME(interval));
            p_sense_time->nSubframeIdx  = (uint32_t)XranGetSubFrameNum(rx_tti,SLOTNUM_PER_SUBFRAME(interval),  SUBFRAMES_PER_SYSTEMFRAME);
            p_sense_time->nFrameIdx     = (uint32_t)XranGetFrameNum(rx_tti, p_timer_ctx->xran_sfn_at_sec_start,SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval));
            p_sense_time->nSecond       = p_timer_ctx->current_second;
    }

    /* user call backs if any */
    if(p_sym_cb_ctx->symCb){
        p_sym_cb_ctx->symCb(p_sym_cb_ctx->symCbParam, p_sym_cb_ctx->symCbTimeInfo);
    }

    MLogTask(PID_UP_UL_USER_DEAD_LINE_CB, t1, MLogTick());
}

void
tx_cp_ul_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogTick();
    int tti, buf_id;
    int ret;
    uint32_t slot_id, subframe_id, frame_id;
    int32_t cc_id;
    int ant_id, prach_port_id;
    uint16_t occasionid;
    uint16_t beam_id;
    uint8_t num_eAxc, num_CCPorts;
    uint8_t ctx_id;

    void *pHandle;
    int num_list;

    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)arg;
    if(!p_xran_dev_ctx)
    {
        print_err("Null xRAN context!!\n");
        return;
    }
    struct xran_prach_cp_config *pPrachCPConfig = &(p_xran_dev_ctx->PrachCPConfig);
    struct xran_timer_ctx *pTCtx =  &p_xran_dev_ctx->timer_ctx[0];
    uint32_t interval = p_xran_dev_ctx->interval_us_local;
    uint8_t PortId = p_xran_dev_ctx->xran_port_id;

    tti = pTCtx[(xran_lib_ota_tti[PortId] & 1) ^ 1].tti_to_process;
    buf_id = tti % XRAN_N_FE_BUF_LEN;
    slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME(interval));
    subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME(interval),  SUBFRAMES_PER_SYSTEMFRAME);
    frame_id    = XranGetFrameNum(tti,xran_getSfnSecStart(),SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval));
    if (tti == 0) {
        //Wrap around to next second
        frame_id = (frame_id + NUM_OF_FRAMES_PER_SECOND) & 0x3ff;
    }
    ctx_id      = XranGetSlotNum(tti, SLOTS_PER_SYSTEMFRAME(interval)) % XRAN_MAX_SECTIONDB_CTX;

    pHandle = p_xran_dev_ctx;
    if(xran_get_ru_category(pHandle) == XRAN_CATEGORY_A)
        num_eAxc    = xran_get_num_eAxc(pHandle);
    else
        num_eAxc    = xran_get_num_eAxcUl(pHandle);
    num_CCPorts = xran_get_num_cc(pHandle);

    if(first_call && p_xran_dev_ctx->enableCP) {

        print_dbg("[%d]SFN %d sf %d slot %d\n", tti, frame_id, subframe_id, slot_id);

        for(ant_id = 0; ant_id < num_eAxc; ++ant_id) {
            for(cc_id = 0; cc_id < num_CCPorts; cc_id++) {
                if(xran_fs_get_slot_type(PortId, cc_id, tti, XRAN_SLOT_TYPE_UL) == 1
                /*  || xran_fs_get_slot_type(cc_id, tti, XRAN_SLOT_TYPE_SP) == 1*/ ) {
                    /* start new section information list */
                    xran_cp_reset_section_info(pHandle, XRAN_DIR_UL, cc_id, ant_id, ctx_id);
                    if(p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[buf_id][cc_id][ant_id].sBufferList.pBuffers){
                        if(p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[buf_id][cc_id][ant_id].sBufferList.pBuffers->pData){
                    num_list = xran_cp_create_and_send_section(pHandle, ant_id, XRAN_DIR_UL, tti, cc_id,
                        (struct xran_prb_map *)p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[buf_id][cc_id][ant_id].sBufferList.pBuffers->pData,
                        p_xran_dev_ctx->fh_cfg.ru_conf.xranCat, ctx_id);
                        }
                    }
                }
            }
        }

        if(p_xran_dev_ctx->enablePrach) {
            uint32_t is_prach_slot = xran_is_prach_slot(PortId, subframe_id, slot_id);
            if(((frame_id % pPrachCPConfig->x) == pPrachCPConfig->y[0]) && (is_prach_slot==1)) {   //is prach slot
                for(ant_id = 0; ant_id < num_eAxc; ++ant_id) {
                    for(cc_id = 0; cc_id < num_CCPorts; cc_id++) {
                        for (occasionid = 0; occasionid < pPrachCPConfig->occassionsInPrachSlot; occasionid++) {
                        struct xran_cp_gen_params params;
                        struct xran_section_gen_info sect_geninfo[8];
                        struct rte_mbuf *mbuf = xran_ethdi_mbuf_alloc();
                        prach_port_id = ant_id + num_eAxc;
                        /* start new section information list */
                        xran_cp_reset_section_info(pHandle, XRAN_DIR_UL, cc_id, prach_port_id, ctx_id);

                        beam_id = xran_get_beamid(pHandle, XRAN_DIR_UL, cc_id, prach_port_id, slot_id);
                        ret = generate_cpmsg_prach(pHandle, &params, sect_geninfo, mbuf, p_xran_dev_ctx,
                                    frame_id, subframe_id, slot_id,
                                        beam_id, cc_id, prach_port_id, occasionid,
                                    xran_get_cp_seqid(pHandle, XRAN_DIR_UL, cc_id, prach_port_id));
                        if (ret == XRAN_STATUS_SUCCESS)
                            send_cpmsg(pHandle, mbuf, &params, sect_geninfo,
                                cc_id, prach_port_id, xran_get_cp_seqid(pHandle, XRAN_DIR_UL, cc_id, prach_port_id));
                    }
                }
            }
        }
        }
    } /* if(p_xran_dev_ctx->enableCP) */

    MLogTask(PID_CP_UL_CB, t1, MLogTick());
}

void
tti_to_phy_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogTick();
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)arg;
    uint32_t interval = p_xran_dev_ctx->interval_us_local;

    p_xran_dev_ctx->phy_tti_cb_done = 1; /* DPDK called CB */
    if (first_call){
        if(p_xran_dev_ctx->ttiCb[XRAN_CB_TTI]){
            if(p_xran_dev_ctx->SkipTti[XRAN_CB_TTI] <= 0){
                p_xran_dev_ctx->ttiCb[XRAN_CB_TTI](p_xran_dev_ctx->TtiCbParam[XRAN_CB_TTI]);
            }else{
                p_xran_dev_ctx->SkipTti[XRAN_CB_TTI]--;
            }
        }
    } else {
        if(p_xran_dev_ctx->ttiCb[XRAN_CB_TTI]){
            int32_t tti = (int32_t)XranGetTtiNum(xran_lib_ota_sym_idx[p_xran_dev_ctx->xran_port_id], XRAN_NUM_OF_SYMBOL_PER_SLOT);
            uint32_t slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME(interval));
            uint32_t subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME(interval),  SUBFRAMES_PER_SYSTEMFRAME);
            uint32_t frame_id = XranGetFrameNum(tti,xran_getSfnSecStart(),SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval));
            if((frame_id == xran_max_frame)&&(subframe_id==9)&&(slot_id == SLOTNUM_PER_SUBFRAME(interval)-1)) {  //(tti == xran_fs_get_max_slot()-1)
                first_call = 1;
            }
        }
    }

    MLogTask(PID_TTI_CB_TO_PHY, t1, MLogTick());
}

int32_t
xran_timing_source_thread(void *args)
{
    int res = 0;
    cpu_set_t cpuset;
    int32_t   do_reset = 0;
    uint64_t  t1 = 0;
    uint64_t  delta;
    int32_t   result1,i,j;

    uint32_t xran_port_id = 0;
    static int owdm_init_done = 0;

    struct sched_param sched_param;
    struct xran_device_ctx * p_dev_ctx = (struct xran_device_ctx *) args ;
    uint64_t tWake = 0, tWakePrev = 0, tUsed = 0;
    struct cb_elem_entry * cb_elm = NULL;

    struct xran_device_ctx * p_dev_ctx_run = NULL;
    /* ToS = Top of Second start +- 1.5us */
    struct timespec ts;
    char thread_name[32];
    char buff[100];

    printf("%s [CPU %2d] [PID: %6d]\n", __FUNCTION__,  rte_lcore_id(), getpid());
    memset(&sched_param, 0, sizeof(struct sched_param));
    /* set main thread affinity mask to CPU2 */
    sched_param.sched_priority = XRAN_THREAD_DEFAULT_PRIO;
    CPU_ZERO(&cpuset);
    CPU_SET(p_dev_ctx->fh_init.io_cfg.timing_core, &cpuset);

    if (result1 = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset))
    {
        printf("pthread_setaffinity_np failed: coreId = 2, result1 = %d\n",result1);
    }
    if ((result1 = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched_param)))
    {
        printf("priority is not changed: coreId = 2, result1 = %d\n",result1);
    }

    snprintf(thread_name, RTE_DIM(thread_name), "%s-%d", "fh_main_poll", rte_lcore_id());
    if ((res = pthread_setname_np(pthread_self(), thread_name))) {
        printf("[core %d] pthread_setname_np = %d\n",rte_lcore_id(), res);
        }

    printf("TTI interval %ld [us]\n", interval_us);

    if (!p_dev_ctx->fh_init.io_cfg.eowd_cmn[p_dev_ctx->fh_init.io_cfg.id].owdm_enable) {
        if ((res = xran_timing_create_cbs(args)) < 0){
        return res;
        }
        }

        do {
           timespec_get(&ts, TIME_UTC);
        }while (ts.tv_nsec >1500);

        struct tm * ptm = gmtime(&ts.tv_sec);
        if(ptm){
            strftime(buff, sizeof buff, "%D %T", ptm);
        printf("%s: thread_run start time: %s.%09ld UTC [%ld]\n",
        (p_dev_ctx->fh_init.io_cfg.id == O_DU ? "O-DU": "O-RU"), buff, ts.tv_nsec, interval_us);
    }

    do {
       timespec_get(&ts, TIME_UTC);
    }while (ts.tv_nsec == 0);

    p_dev_ctx->timing_source_thread_running = 1;
    while(1) {

        /* Check if owdm finished to create the timing cbs based on measurement results */
        if ((p_dev_ctx->fh_init.io_cfg.eowd_cmn[p_dev_ctx->fh_init.io_cfg.id].owdm_enable)&&(!owdm_init_done)&&unlikely(XRAN_RUNNING == xran_if_current_state)) {
            // Adjust Windows based on Delay Measurement results
            xran_adjust_timing_parameters(p_dev_ctx);
            if ((res = xran_timing_create_cbs(args)) < 0){
                return res;
                }
            printf("TTI interval %ld [us]\n", interval_us);
            owdm_init_done = 1;

        }



        /* Update Usage Stats */
        tWake = xran_tick();
        xran_used_tick += tUsed;
        if (tWakePrev)
        {
            xran_total_tick += get_ticks_diff(tWake, tWakePrev);
        }
        tWakePrev = tWake;
        tUsed = 0;

        delta = poll_next_tick(interval_us*1000L/N_SYM_PER_SLOT, &tUsed);
        if (XRAN_STOPPED == xran_if_current_state)
            break;

        if (likely(XRAN_RUNNING == xran_if_current_state)) {
            for(xran_port_id =  0; xran_port_id < XRAN_PORTS_NUM; xran_port_id++ ) {
                p_dev_ctx_run = xran_dev_get_ctx_by_id(xran_port_id);
                if(p_dev_ctx_run) {
                    if(p_dev_ctx_run->xran_port_id == xran_port_id) {
                        if(XranGetSymNum(xran_lib_ota_sym_idx[p_dev_ctx_run->xran_port_id], XRAN_NUM_OF_SYMBOL_PER_SLOT) == xran_lib_ota_sym[xran_port_id])
                        {
                            sym_ota_cb(&p_dev_ctx_run->sym_timer, p_dev_ctx_run, &tUsed);
                            xran_lib_ota_sym[xran_port_id]++;
                            if(xran_lib_ota_sym[xran_port_id] >= N_SYM_PER_SLOT)
                                xran_lib_ota_sym[xran_port_id]=0;
                        }
                    }
                    else  {
                        rte_panic("p_dev_ctx_run == xran_port_id");
    }
                }
            }
        }
    }

    xran_timing_destroy_cbs(args);
    printf("Closing timing source thread...\n");
    return res;
}

/* Handle ecpri format. */
#define MBUFS_CNT 16

int32_t handle_ecpri_ethertype(struct rte_mbuf* pkt_q[], uint16_t xport_id, struct xran_eaxc_info *p_cid, uint16_t num)
{
    struct rte_mbuf* pkt, * pkt0;
    uint16_t i;
    struct rte_ether_hdr* eth_hdr;
    struct xran_ecpri_hdr* ecpri_hdr;
    union xran_ecpri_cmn_hdr* ecpri_cmn;
    unsigned long t1;
    int32_t ret = MBUF_FREE;
    uint32_t ret_data[MBUFS_CNT] = { MBUFS_CNT * MBUF_FREE };
    struct xran_device_ctx* p_dev_ctx = xran_dev_get_ctx_by_id(xport_id);
    uint16_t num_data = 0, num_control = 0, num_meas = 0;
    struct rte_mbuf* pkt_data[MBUFS_CNT], * pkt_control[MBUFS_CNT], * pkt_meas[MBUFS_CNT], *pkt_adj[MBUFS_CNT];
    static uint32_t owdm_rx_first_pass = 1;

    if (p_dev_ctx == NULL)
        return ret;

    for (i = 0; i < num; i++)
    {
        pkt = pkt_q[i];

//        rte_prefetch0(rte_pktmbuf_mtod(pkt, void*));

        rte_pktmbuf_adj(pkt, sizeof(*eth_hdr));
    ecpri_hdr = rte_pktmbuf_mtod(pkt, struct xran_ecpri_hdr *);

        p_dev_ctx->fh_counters.rx_bytes_counter += rte_pktmbuf_pkt_len(pkt);

        pkt_adj[i] = pkt;
        switch (ecpri_hdr->cmnhdr.bits.ecpri_mesg_type)
        {
        case ECPRI_IQ_DATA:
                pkt_data[num_data++] = pkt;
            break;
        // For RU emulation
        case ECPRI_RT_CONTROL_DATA:
                pkt_control[num_control++] = pkt;
            break;
            case ECPRI_DELAY_MEASUREMENT:
                if (owdm_rx_first_pass != 0)
{
                    // Initialize and verify that Payload Length is in range */
                    xran_initialize_and_verify_owd_pl_length((void*)p_dev_ctx);
                    owdm_rx_first_pass = 0;

                }
                pkt_meas[num_meas++] = pkt;
                break;
            default:
                if (p_dev_ctx->fh_init.io_cfg.id == O_DU) {
                    print_err("Invalid eCPRI message type - %d", ecpri_hdr->cmnhdr.bits.ecpri_mesg_type);
        }
                break;
    }
}

    if(num_data == MBUFS_CNT && p_dev_ctx->fh_cfg.ru_conf.xranCat == XRAN_CATEGORY_B) /* w/a for Cat A issue */
{
        for (i = 0; i < MBUFS_CNT; i++)
{
            ret_data[i] == MBUF_FREE;
}

        if (p_dev_ctx->fh_init.io_cfg.id == O_DU || p_dev_ctx->fh_init.io_cfg.id == O_RU)
{
            if (p_dev_ctx->xran2phy_mem_ready != 0)
                ret = process_mbuf_batch(pkt_data, (void*)p_dev_ctx, MBUFS_CNT, p_cid,  ret_data );
            for (i = 0; i < MBUFS_CNT; i++)
                    {
                if (ret_data[i] == MBUF_FREE)
                    rte_pktmbuf_free(pkt_data[i]);
                    }
            }
    else
{
            for (i = 0; i < MBUFS_CNT; i++)
{
                if (ret_data[i] == MBUF_FREE)
                    rte_pktmbuf_free(pkt_data[i]);
            }
            print_err("incorrect dev type %d\n", p_dev_ctx->fh_init.io_cfg.id);
        }
        }
    else
{
        for (i = 0; i < num_data; i++)
    {
            ret = process_mbuf(pkt_data[i], (void*)p_dev_ctx, p_cid);
            if (ret == MBUF_FREE)
                rte_pktmbuf_free(pkt_data[i]);
    }

        for (i = 0; i < num_control; i++)
    {
            t1 = MLogTick();
            if (p_dev_ctx->fh_init.io_cfg.id == O_RU)
        {
                ret = process_cplane(pkt_control[i], (void*)p_dev_ctx);
                p_dev_ctx->fh_counters.rx_counter++;
                if (ret == MBUF_FREE)
                    rte_pktmbuf_free(pkt_control[i]);
        }
        else
        {
                print_err("O-DU recevied C-Plane message!");
        }
            MLogTask(PID_PROCESS_CP_PKT, t1, MLogTick());
    }

        for (i = 0; i < num_meas; i++)
        {
            t1 = MLogTick();
            ret = process_delay_meas(pkt_meas[i], (void*)p_dev_ctx, xport_id);
            //                printf("Got delay_meas_pkt xport_id %d p_dev_ctx %08"PRIx64"\n", xport_id,(int64_t*)p_dev_ctx) ;
            if (ret == MBUF_FREE)
                rte_pktmbuf_free(pkt_meas[i]);
            MLogTask(PID_PROCESS_DELAY_MEAS_PKT, t1, MLogTick());
    }
            }

    return MBUF_FREE;
}

int32_t
xran_packet_and_dpdk_timer_thread(void *args)
{
    struct xran_ethdi_ctx *const ctx = xran_ethdi_get_ctx();

    uint64_t prev_tsc = 0;
    uint64_t cur_tsc = rte_rdtsc();
    uint64_t diff_tsc = cur_tsc - prev_tsc;
    cpu_set_t cpuset;
    struct sched_param sched_param;
    int res = 0;
    printf("%s [CPU %2d] [PID: %6d]\n", __FUNCTION__,  rte_lcore_id(), getpid());

    memset(&sched_param, 0, sizeof(struct sched_param));
    sched_param.sched_priority = XRAN_THREAD_DEFAULT_PRIO;

    if ((res  = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched_param)))
    {
        printf("priority is not changed: coreId = %d, result1 = %d\n",rte_lcore_id(), res);
    }

    while(1){

        cur_tsc  = rte_rdtsc();
        diff_tsc = cur_tsc - prev_tsc;
        if (diff_tsc > TIMER_RESOLUTION_CYCLES) {
            rte_timer_manage();
            prev_tsc = cur_tsc;
        }

        if (XRAN_STOPPED == xran_if_current_state)
            break;
    }

    printf("Closing pkts timer thread...\n");
    return 0;
}

void xran_initialize_ecpri_owd_meas_cmn( struct xran_io_cfg *ptr)
{
//    ptr->eowd_cmn.initiator_en = 0; // Initiator 1, Recipient 0
//    ptr->eowd_cmn.filterType = 0;  // 0 Simple average based on number of measurements
    // Set default values if the Timeout and numberOfSamples are not set
    if ( ptr->eowd_cmn[ptr->id].responseTo == 0)
        ptr->eowd_cmn[ptr->id].responseTo = 10E6; // 10 ms timeout expressed in ns
    if ( ptr->eowd_cmn[ptr->id].numberOfSamples == 0)
        ptr->eowd_cmn[ptr->id].numberOfSamples = 8; // Number of samples to be averaged
}
void xran_initialize_ecpri_owd_meas_per_port (int i, struct xran_io_cfg *ptr )
{
   /* This function initializes one_way delay measurements on a per port basis,
      most variables default to zero    */
   ptr->eowd_port[ptr->id][i].portid = (uint8_t)i;
}

int32_t
xran_init(int argc, char *argv[],
           struct xran_fh_init *p_xran_fh_init, char *appName, void ** pXranLayerHandle)
{
    int32_t ret = XRAN_STATUS_SUCCESS;
    int32_t i;
    int32_t j;
    int32_t o_xu_id = 0;

    struct xran_io_cfg      *p_io_cfg       = NULL;
    struct xran_device_ctx * p_xran_dev_ctx = NULL;

    int32_t  lcore_id = 0;
    char filename[64];

    const char *version = rte_version();

    if (version == NULL)
        rte_panic("version == NULL");

    printf("'%s'\n", version);

    if (p_xran_fh_init->xran_ports < 1 || p_xran_fh_init->xran_ports > XRAN_PORTS_NUM) {
        ret = XRAN_STATUS_INVALID_PARAM;
        print_err("fh_init xran_ports= %d is wrong [%d]\n", p_xran_fh_init->xran_ports, ret);
        return ret;
    }

    p_io_cfg = (struct xran_io_cfg *)&p_xran_fh_init->io_cfg;

    if ((ret = xran_dev_create_ctx(p_xran_fh_init->xran_ports)) < 0) {
        print_err("context allocation error [%d]\n", ret);
        return ret;
    }

    for(o_xu_id = 0; o_xu_id < p_xran_fh_init->xran_ports; o_xu_id++){
        p_xran_dev_ctx  = xran_dev_get_ctx_by_id(o_xu_id);
    memset(p_xran_dev_ctx, 0, sizeof(struct xran_device_ctx));
        p_xran_dev_ctx->xran_port_id  = o_xu_id;

    /* copy init */
    p_xran_dev_ctx->fh_init = *p_xran_fh_init;
    printf(" %s: MTU %d\n", __FUNCTION__, p_xran_dev_ctx->fh_init.mtu);

    memcpy(&(p_xran_dev_ctx->eAxc_id_cfg), &(p_xran_fh_init->eAxCId_conf), sizeof(struct xran_eaxcid_config));
    /* To make sure to set default functions */
    p_xran_dev_ctx->send_upmbuf2ring    = NULL;
    p_xran_dev_ctx->send_cpmbuf2ring    = NULL;
        // Ecpri initialization for One Way delay measurements common variables to default values
        xran_initialize_ecpri_owd_meas_cmn(&p_xran_dev_ctx->fh_init.io_cfg);
    }

    /* default values if not set */
    if(p_io_cfg->nEthLinePerPort == 0)
        p_io_cfg->nEthLinePerPort = 1;

    if(p_io_cfg->nEthLineSpeed == 0)
        p_io_cfg->nEthLineSpeed = 25;

    /** at least 1 RX Q */
    if(p_io_cfg->num_rxq == 0)
        p_io_cfg->num_rxq = 1;

    if (p_io_cfg->id == 1) {
        /* 1 HW for O-RU */
        p_io_cfg->num_rxq =  1;
    }

#if (RTE_VER_YEAR < 21) /* eCPRI flow supported with DPDK 21.02 or later */
    if (p_io_cfg->num_rxq > 1){
        p_io_cfg->num_rxq =  1;
        printf("%s does support eCPRI flows. Set rxq to %d\n", version, p_io_cfg->num_rxq);
    }
#endif
    printf("PF Eth line speed %dG\n",p_io_cfg->nEthLineSpeed);
    printf("PF Eth lines per O-xU port %d\n",p_io_cfg->nEthLinePerPort);
    printf("RX HW queues per O-xU Eth line %d \n",p_io_cfg->num_rxq);

    if(p_xran_fh_init->xran_ports * p_io_cfg->nEthLinePerPort *(2 - 1* p_io_cfg->one_vf_cu_plane)  != p_io_cfg->num_vfs) {
        print_err("Incorrect VFs configurations: For %d O-xUs with %d Ethernet ports expected number of VFs is %d. [provided %d]\n",
            p_xran_fh_init->xran_ports, p_io_cfg->nEthLinePerPort,
            p_xran_fh_init->xran_ports * p_io_cfg->nEthLinePerPort *(2 - 1* p_io_cfg->one_vf_cu_plane), p_io_cfg->num_vfs);
    }

    xran_if_current_state = XRAN_INIT;
    xran_register_ethertype_handler(ETHER_TYPE_ECPRI, handle_ecpri_ethertype);
    if (p_io_cfg->id == 0)
        xran_ethdi_init_dpdk_io(p_xran_fh_init->filePrefix,
                           p_io_cfg,
                           &lcore_id,
                           (struct rte_ether_addr *)p_xran_fh_init->p_o_du_addr,
                           (struct rte_ether_addr *)p_xran_fh_init->p_o_ru_addr,
                           p_xran_dev_ctx->fh_init.mtu);
    else
        xran_ethdi_init_dpdk_io(p_xran_fh_init->filePrefix,
                           p_io_cfg,
                           &lcore_id,
                           (struct rte_ether_addr *)p_xran_fh_init->p_o_ru_addr,
                           (struct rte_ether_addr *)p_xran_fh_init->p_o_du_addr,
                           p_xran_dev_ctx->fh_init.mtu);

    for(o_xu_id = 0; o_xu_id < p_xran_fh_init->xran_ports; o_xu_id++){
        p_xran_dev_ctx  = xran_dev_get_ctx_by_id(o_xu_id);

        for(i = 0; i < MAX_TTI_TO_PHY_TIMER; i++ )
            rte_timer_init(&p_xran_dev_ctx->tti_to_phy_timer[i]);

        rte_timer_init(&p_xran_dev_ctx->sym_timer);
    for (i = 0; i< MAX_NUM_OF_DPDK_TIMERS; i++)
            rte_timer_init(&p_xran_dev_ctx->dpdk_timer[i]);

    p_xran_dev_ctx->direct_pool   = socket_direct_pool;
    p_xran_dev_ctx->indirect_pool = socket_indirect_pool;


        for (j = 0; j< XRAN_NUM_OF_SYMBOL_PER_SLOT; j++){
            LIST_INIT (&p_xran_dev_ctx->sym_cb_list_head[j]);
    }

    }

    for (i=0; i<XRAN_PORTS_NUM; i++){
    for (uint32_t nCellIdx = 0; nCellIdx < XRAN_MAX_SECTOR_NR; nCellIdx++){
            xran_fs_clear_slot_type(i,nCellIdx);
        }
    }

    *pXranLayerHandle = xran_dev_get_ctx();


    // The ecpri initialization loop needs to be done per pf and vf (Outer loop pf and inner loop vf)
    for (i=0;  i< p_io_cfg->num_vfs; i++)
    {
        /* Initialize ecpri one-way delay measurement info on a per vf port basis */
        xran_initialize_ecpri_owd_meas_per_port (i, p_io_cfg);
    }

    return ret;
}

int32_t
xran_sector_get_instances (uint32_t xran_port, void * pDevHandle, uint16_t nNumInstances,
               xran_cc_handle_t * pSectorInstanceHandles)
{
    xran_status_t nStatus = XRAN_STATUS_FAIL;
    struct xran_device_ctx *pDev = (struct xran_device_ctx *)pDevHandle;
    XranSectorHandleInfo *pCcHandle = NULL;
    int32_t i = 0;

    pDev += xran_port;

    /* Check for the Valid Parameters */
    CHECK_NOT_NULL (pSectorInstanceHandles, XRAN_STATUS_INVALID_PARAM);

    if (!nNumInstances) {
        print_dbg("Instance is not assigned for this function !!! \n");
        return XRAN_STATUS_INVALID_PARAM;
    }

    for (i = 0; i < nNumInstances; i++) {

        /* Allocate Memory for CC handles */
        pCcHandle = (XranSectorHandleInfo *) _mm_malloc( /*"xran_cc_handles",*/ sizeof (XranSectorHandleInfo), 64);

        if(pCcHandle == NULL)
            return XRAN_STATUS_RESOURCE;

        memset (pCcHandle, 0, (sizeof (XranSectorHandleInfo)));

        pCcHandle->nIndex    = i;
        pCcHandle->nXranPort = pDev->xran_port_id;

        printf("%s [%d]: CC %d handle %p\n", __FUNCTION__, pDev->xran_port_id, i, pCcHandle);
        pLibInstanceHandles[pDev->xran_port_id][i] = pSectorInstanceHandles[i] = pCcHandle;

        printf("Handle: %p Instance: %p\n",
            &pSectorInstanceHandles[i], pSectorInstanceHandles[i]);
    }

    return XRAN_STATUS_SUCCESS;
}


int32_t
xran_5g_fronthault_config (void * pHandle,
                struct xran_buffer_list *pSrcBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pSrcCpBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pDstBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pDstCpBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                xran_transport_callback_fn pCallback,
                void *pCallbackTag)
{
    int j, i = 0, z, k;
    XranSectorHandleInfo* pXranCc = NULL;
    struct xran_device_ctx * p_xran_dev_ctx = NULL;

    if(NULL == pHandle) {
        printf("Handle is NULL!\n");
        return XRAN_STATUS_FAIL;
    }

    pXranCc = (XranSectorHandleInfo*) pHandle;
    p_xran_dev_ctx = xran_dev_get_ctx_by_id(pXranCc->nXranPort);
    if (p_xran_dev_ctx == NULL) {
        printf ("p_xran_dev_ctx is NULL\n");
        return XRAN_STATUS_FAIL;
    }

    i = pXranCc->nIndex;

    for(j = 0; j < XRAN_N_FE_BUF_LEN; j++) {
        for(z = 0; z < XRAN_MAX_ANTENNA_NR; z++){
            /* U-plane TX */

            p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].bValid = 0;
            p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
            p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
            p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
            p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
            p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &p_xran_dev_ctx->sFrontHaulTxBuffers[j][i][z][0];

            if(pSrcBuffer[z][j])
                p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList =   *pSrcBuffer[z][j];
            else
                memset(&p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList, 0, sizeof(*pSrcBuffer[z][j]));


            /* C-plane TX */
            p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].bValid = 0;
            p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
            p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
            p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
            p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
            p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &p_xran_dev_ctx->sFrontHaulTxPrbMapBuffers[j][i][z][0];

            if(pSrcCpBuffer[z][j])
                p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList =   *pSrcCpBuffer[z][j];
            else
                memset(&p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList, 0, sizeof(*pSrcCpBuffer[z][j]));
            /* U-plane RX */

            p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].bValid = 0;
            p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
            p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
            p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
            p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
            p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &p_xran_dev_ctx->sFrontHaulRxBuffers[j][i][z][0];

            if(pDstBuffer[z][j])
                p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList =   *pDstBuffer[z][j];
            else
                memset(&p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList, 0, sizeof(*pDstBuffer[z][j]));


            /* C-plane RX */
            p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].bValid = 0;
            p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
            p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
            p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
            p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
            p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &p_xran_dev_ctx->sFrontHaulRxPrbMapBuffers[j][i][z][0];

            if(pDstCpBuffer[z][j])
                p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList =   *pDstCpBuffer[z][j];
            else
                memset(&p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList, 0, sizeof(*pDstCpBuffer[z][j]));

        }
    }

    p_xran_dev_ctx->pCallback[i]    = pCallback;
    p_xran_dev_ctx->pCallbackTag[i] = pCallbackTag;
    print_dbg("%s: [p %d CC  %d] Cb %p cb %p\n",__FUNCTION__,
        p_xran_dev_ctx->xran_port_id, i, p_xran_dev_ctx->pCallback[i], p_xran_dev_ctx->pCallbackTag[i]);

    p_xran_dev_ctx->xran2phy_mem_ready = 1;

    return XRAN_STATUS_SUCCESS;
}

int32_t
xran_5g_prach_req (void *  pHandle,
                struct xran_buffer_list *pDstBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pDstBufferDecomp[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],                
                xran_transport_callback_fn pCallback,
                void *pCallbackTag)
{
    int j, i = 0, z;
    XranSectorHandleInfo* pXranCc = NULL;
    struct xran_device_ctx * p_xran_dev_ctx = NULL;

    if(NULL == pHandle) {
        printf("Handle is NULL!\n");
        return XRAN_STATUS_FAIL;
    }

    pXranCc = (XranSectorHandleInfo*) pHandle;
    p_xran_dev_ctx = xran_dev_get_ctx_by_id(pXranCc->nXranPort);
    if (p_xran_dev_ctx == NULL) {
        printf ("p_xran_dev_ctx is NULL\n");
        return XRAN_STATUS_FAIL;
    }

    i = pXranCc->nIndex;

    for(j = 0; j < XRAN_N_FE_BUF_LEN; j++) {
        for(z = 0; z < XRAN_MAX_ANTENNA_NR; z++){
           p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].bValid = 0;
           p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
           p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
           p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
           p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_MAX_ANTENNA_NR; // ant number.
           p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &p_xran_dev_ctx->sFHPrachRxBuffers[j][i][z][0];
           if(pDstBuffer[z][j])
               p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList =   *pDstBuffer[z][j];
            else
                memset(&p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList, 0, sizeof(*pDstBuffer[z][j]));
                
            p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrlDecomp[j][i][z].sBufferList.pBuffers = &p_xran_dev_ctx->sFHPrachRxBuffersDecomp[j][i][z][0];
            if(pDstBufferDecomp[z][j])
                p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrlDecomp[j][i][z].sBufferList =   *pDstBufferDecomp[z][j];
                
        }
    }

    p_xran_dev_ctx->pPrachCallback[i]    = pCallback;
    p_xran_dev_ctx->pPrachCallbackTag[i] = pCallbackTag;

    print_dbg("%s: [p %d CC  %d] Cb %p cb %p\n",__FUNCTION__,
        p_xran_dev_ctx->xran_port_id, i, p_xran_dev_ctx->pPrachCallback[i], p_xran_dev_ctx->pPrachCallbackTag[i]);

    return XRAN_STATUS_SUCCESS;
}

int32_t
xran_5g_srs_req (void *  pHandle,
                struct xran_buffer_list *pDstBuffer[XRAN_MAX_ANT_ARRAY_ELM_NR][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pDstCpBuffer[XRAN_MAX_ANT_ARRAY_ELM_NR][XRAN_N_FE_BUF_LEN],
                xran_transport_callback_fn pCallback,
                void *pCallbackTag)
{
    int j, i = 0, z;
    XranSectorHandleInfo* pXranCc = NULL;
    struct xran_device_ctx * p_xran_dev_ctx = NULL;

    if(NULL == pHandle) {
        printf("Handle is NULL!\n");
        return XRAN_STATUS_FAIL;
    }

    pXranCc = (XranSectorHandleInfo*) pHandle;
    p_xran_dev_ctx = xran_dev_get_ctx_by_id(pXranCc->nXranPort);
    if (p_xran_dev_ctx == NULL) {
        printf ("p_xran_dev_ctx is NULL\n");
        return XRAN_STATUS_FAIL;
    }

    i = pXranCc->nIndex;

    for(j=0; j<XRAN_N_FE_BUF_LEN; j++) {
        for(z = 0; z < XRAN_MAX_ANT_ARRAY_ELM_NR; z++){
           p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[j][i][z].bValid = 0;
           p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
           p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
           p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
           p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_MAX_ANT_ARRAY_ELM_NR; // ant number.
           p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &p_xran_dev_ctx->sFHSrsRxBuffers[j][i][z][0];
           if(pDstBuffer[z][j])
               p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[j][i][z].sBufferList =   *pDstBuffer[z][j];
            else
                memset(&p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[j][i][z].sBufferList, 0, sizeof(*pDstBuffer[z][j]));

            /* C-plane SRS */
            p_xran_dev_ctx->sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].bValid = 0;
            p_xran_dev_ctx->sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
            p_xran_dev_ctx->sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
            p_xran_dev_ctx->sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
            p_xran_dev_ctx->sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
            p_xran_dev_ctx->sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &p_xran_dev_ctx->sFHSrsRxPrbMapBuffers[j][i][z];

            if(pDstCpBuffer[z][j])
                p_xran_dev_ctx->sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList =   *pDstCpBuffer[z][j];
            else
                memset(&p_xran_dev_ctx->sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList, 0, sizeof(*pDstCpBuffer[z][j]));

        }
    }

    p_xran_dev_ctx->pSrsCallback[i]    = pCallback;
    p_xran_dev_ctx->pSrsCallbackTag[i] = pCallbackTag;

    print_dbg("%s: [p %d CC  %d] Cb %p cb %p\n",__FUNCTION__,
        p_xran_dev_ctx->xran_port_id, i, p_xran_dev_ctx->pSrsCallback[i], p_xran_dev_ctx->pSrsCallbackTag[i]);

    return XRAN_STATUS_SUCCESS;
}

uint32_t
xran_get_time_stats(uint64_t *total_time, uint64_t *used_time, uint32_t *num_core_used, uint32_t *core_used, uint32_t clear)
{
    uint32_t i;

    *num_core_used = xran_num_cores_used;
    for (i = 0; i < xran_num_cores_used; i++)
    {
        core_used[i] = xran_core_used[i];
    }

    *total_time = xran_total_tick;
    *used_time = xran_used_tick;

    if (clear)
    {
        xran_total_tick = 0;
        xran_used_tick = 0;
    }

    return 0;
}

uint8_t*
xran_add_cp_hdr_offset(uint8_t  *dst)
{
    dst += (RTE_PKTMBUF_HEADROOM +
            sizeof(struct xran_ecpri_hdr) +
            sizeof(struct xran_cp_radioapp_section1_header) +
            sizeof(struct xran_cp_radioapp_section1));

    dst = RTE_PTR_ALIGN_CEIL(dst, 64);

    return dst;
}

uint8_t*
xran_add_hdr_offset(uint8_t  *dst, int16_t compMethod)
{
    dst+= (RTE_PKTMBUF_HEADROOM +
          sizeof (struct xran_ecpri_hdr) +
          sizeof (struct radio_app_common_hdr) +
          sizeof(struct data_section_hdr));
    if(compMethod != XRAN_COMPMETHOD_NONE)
          dst += sizeof (struct data_section_compression_hdr);
    dst = RTE_PTR_ALIGN_CEIL(dst, 64);

    return dst;
}

int32_t
xran_pkt_gen_process_ring(struct rte_ring *r)
{
    assert(r);
    int32_t     retval = 0;
    struct rte_mbuf *mbufs[16];
    int i;
    uint32_t remaining;
    uint64_t t1;
    struct xran_io_cfg *p_io_cfg = &(xran_ethdi_get_ctx()->io_cfg);
    const uint16_t dequeued = rte_ring_dequeue_burst(r, (void **)mbufs,
        RTE_DIM(mbufs), &remaining);

    if (!dequeued)
        return 0;

    t1 = MLogTick();
    for (i = 0; i < dequeued; ++i) {
        struct cp_up_tx_desc * p_tx_desc =  (struct cp_up_tx_desc *)rte_pktmbuf_mtod(mbufs[i],  struct cp_up_tx_desc *);
        retval = xran_process_tx_sym_cp_on_opt(p_tx_desc->pHandle,
                                        p_tx_desc->ctx_id,
                                        p_tx_desc->tti,
                                        p_tx_desc->cc_id,
                                        p_tx_desc->ant_id,
                                        p_tx_desc->frame_id,
                                        p_tx_desc->subframe_id,
                                        p_tx_desc->slot_id,
                                        p_tx_desc->sym_id,
                                        (enum xran_comp_hdr_type)p_tx_desc->compType,
                                        (enum xran_pkt_dir) p_tx_desc->direction,
                                        p_tx_desc->xran_port_id,
                                        (PSECTION_DB_TYPE)p_tx_desc->p_sec_db);

        xran_pkt_gen_desc_free(p_tx_desc);
        if (XRAN_STOPPED == xran_if_current_state){
            MLogTask(PID_PROCESS_TX_SYM, t1, MLogTick());
            return -1;
        }
    }

    if(p_io_cfg->io_sleep)
       nanosleep(&sleeptime,NULL);

    MLogTask(PID_PROCESS_TX_SYM, t1, MLogTick());

    return remaining;
}

int32_t
xran_dl_pkt_ring_processing_func(void* args)
{
    struct xran_ethdi_ctx *const ctx = xran_ethdi_get_ctx();
    uint16_t xran_port_mask = (uint16_t)((uint64_t)args & 0xFFFF);
    uint16_t current_port;

    rte_timer_manage();

    for (current_port = 0; current_port < XRAN_PORTS_NUM;  current_port++) {
        if( xran_port_mask & (1<<current_port)) {
            xran_pkt_gen_process_ring(ctx->up_dl_pkt_gen_ring[current_port]);
        }
    }

    if (XRAN_STOPPED == xran_if_current_state)
        return -1;

    return 0;
}

/** Function to peforms serves of DPDK times */
int32_t
xran_processing_timer_only_func(void* args)
{
    rte_timer_manage();
    if (XRAN_STOPPED == xran_if_current_state)
        return -1;

    return 0;
}

/** Function to peforms parsing of RX packets on all ports and does TX and RX on ETH device */
int32_t
xran_all_tasks(void* arg)
{

    ring_processing_func(arg);
    process_dpdk_io(arg);
    return 0;
}

/** Function to pefromrm TX and RX on ETH device */
int32_t
xran_eth_trx_tasks(void* arg)
{
    process_dpdk_io(arg);
    return 0;
}

/** Function to pefromrm RX on ETH device */
int32_t
xran_eth_rx_tasks(void* arg)
{
    process_dpdk_io_rx(arg);
    return 0;
}

/** Function to porcess ORAN FH packet per port */
int32_t
ring_processing_func_per_port(void* args)
{
    struct xran_ethdi_ctx *const ctx = xran_ethdi_get_ctx();
    int16_t retPoll = 0;
    int32_t i;
    uint64_t t1, t2;
    uint16_t port_id = (uint16_t)((uint64_t)args & 0xFFFF);
    queueid_t qi;

    for (i = 0; i < ctx->io_cfg.num_vfs && i < XRAN_VF_MAX; i = i+1) {
        if (ctx->vf2xran_port[i] == port_id) {
            for(qi = 0; qi < ctx->rxq_per_port[port_id]; qi++){
                if (process_ring(ctx->rx_ring[i][qi], i, qi))
                    return 0;
            }
        }
    }

    if (XRAN_STOPPED == xran_if_current_state)
        return -1;

    return 0;
}

/** Fucntion generate configuration of worker threads and creates them base on sceanrio and used platform */
int32_t
xran_spawn_workers(void)
{
    uint64_t nWorkerCore = 1LL;
    uint32_t coreNum     = sysconf(_SC_NPROCESSORS_CONF);
    int32_t  i = 0;
    uint32_t total_num_cores  = 1; /*start with timing core */
    uint32_t worker_num_cores = 0;
    uint32_t icx_cpu = 0;
    int32_t core_map[2*sizeof(uint64_t)*8];
    uint32_t xran_port_mask = 0;

    struct xran_ethdi_ctx  *eth_ctx   = xran_ethdi_get_ctx();
    struct xran_device_ctx *p_dev     = NULL;
    struct xran_fh_init    *fh_init   = NULL;
    struct xran_fh_config  *fh_cfg    = NULL;
    struct xran_worker_th_ctx* pThCtx = NULL;

    p_dev =  xran_dev_get_ctx_by_id(0);
    if(p_dev == NULL) {
        print_err("p_dev\n");
        return XRAN_STATUS_FAIL;
    }

    fh_init = &p_dev->fh_init;
    if(fh_init == NULL) {
        print_err("fh_init\n");
        return XRAN_STATUS_FAIL;
    }

    fh_cfg = &p_dev->fh_cfg;
    if(fh_cfg == NULL) {
        print_err("fh_cfg\n");
        return XRAN_STATUS_FAIL;
    }

    for (i = 0; i < coreNum && i < 64; i++) {
        if (nWorkerCore & (uint64_t)eth_ctx->io_cfg.pkt_proc_core) {
            core_map[worker_num_cores++] = i;
            total_num_cores++;
        }
        nWorkerCore = nWorkerCore << 1;
    }

    nWorkerCore = 1LL;
    for (i = 64; i < coreNum && i < 128; i++) {
        if (nWorkerCore & (uint64_t)eth_ctx->io_cfg.pkt_proc_core_64_127) {
            core_map[worker_num_cores++] = i;
            total_num_cores++;
        }
        nWorkerCore = nWorkerCore << 1;
    }

    extern int _may_i_use_cpu_feature(unsigned __int64);
    icx_cpu = _may_i_use_cpu_feature(_FEATURE_AVX512IFMA52);

    printf("O-XU      %d\n", eth_ctx->io_cfg.id);
    printf("HW        %d\n", icx_cpu);
    printf("Num cores %d\n", total_num_cores);
    printf("Num ports %d\n", fh_init->xran_ports);
    printf("O-RU Cat  %d\n", fh_cfg->ru_conf.xranCat);
    printf("O-RU CC   %d\n", fh_cfg->nCC);
    printf("O-RU eAxC %d\n", fh_cfg->neAxc);

    for (i = 0; i < fh_init->xran_ports; i++){
        xran_port_mask |= 1<<i;
    }

    for (i = 0; i < fh_init->xran_ports; i++) {
        struct xran_device_ctx * p_dev_update =  xran_dev_get_ctx_by_id(i);
        if(p_dev_update == NULL){
            print_err("p_dev_update\n");
            return XRAN_STATUS_FAIL;
        }
        p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_DL] = 1;
        p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_UL] = 1;
        printf("p:%d XRAN_JOB_TYPE_CP_DL worker id %d\n", i, p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_DL]);
        printf("p:%d XRAN_JOB_TYPE_CP_UL worker id %d\n", i, p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_UL]);
    }

    if(fh_cfg->ru_conf.xranCat == XRAN_CATEGORY_A) {
        switch(total_num_cores) {
            case 1: /** only timing core */
                eth_ctx->time_wrk_cfg.f = xran_all_tasks;
                eth_ctx->time_wrk_cfg.arg   = NULL;
                eth_ctx->time_wrk_cfg.state = 1;
            break;
            case 2:
                eth_ctx->time_wrk_cfg.f = xran_eth_trx_tasks;
                eth_ctx->time_wrk_cfg.arg   = NULL;
                eth_ctx->time_wrk_cfg.state = 1;

                pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                if(pThCtx == NULL){
                    print_err("pThCtx allocation error\n");
                    return XRAN_STATUS_FAIL;
                }
                memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                pThCtx->worker_id    = 0;
                pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_rx_bbdev", core_map[pThCtx->worker_id]);
                pThCtx->task_func = ring_processing_func;
                pThCtx->task_arg  = NULL;
                eth_ctx->pkt_wrk_cfg[0].f     = xran_generic_worker_thread;
                eth_ctx->pkt_wrk_cfg[0].arg   = pThCtx;
            break;
            case 3:
                /* timing core */
                eth_ctx->time_wrk_cfg.f     = xran_eth_trx_tasks;
                eth_ctx->time_wrk_cfg.arg   = NULL;
                eth_ctx->time_wrk_cfg.state = 1;

                /* workers */
                /** 0 **/
                pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                if(pThCtx == NULL){
                    print_err("pThCtx allocation error\n");
                    return XRAN_STATUS_FAIL;
                }
                memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                pThCtx->worker_id      = 0;
                pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_rx_bbdev", core_map[pThCtx->worker_id]);
                pThCtx->task_func = ring_processing_func;
                pThCtx->task_arg  = NULL;
                eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                for (i = 0; i < fh_init->xran_ports; i++) {
                    struct xran_device_ctx * p_dev_update =  xran_dev_get_ctx_by_id(i);
                    if(p_dev_update == NULL) {
                        print_err("p_dev_update\n");
                        return XRAN_STATUS_FAIL;
                    }
                    p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_DL] = pThCtx->worker_id;
                    p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_UL] = pThCtx->worker_id;
                    printf("p:%d XRAN_JOB_TYPE_CP_DL worker id %d\n", i,  p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_DL]);
                    printf("p:%d XRAN_JOB_TYPE_CP_UL worker id %d\n", i,  p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_UL]);
                }

                /** 1 - CP GEN **/
                pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                if(pThCtx == NULL){
                    print_err("pThCtx allocation error\n");
                    return XRAN_STATUS_FAIL;
                }
                memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                pThCtx->worker_id      = 1;
                pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_cp_gen", core_map[pThCtx->worker_id]);
                pThCtx->task_func = xran_dl_pkt_ring_processing_func;
                pThCtx->task_arg  = (void*)xran_port_mask;
                eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;
            break;
            default:
                print_err("unsupported configuration Cat %d numports %d total_num_cores = %d\n", fh_cfg->ru_conf.xranCat, fh_init->xran_ports, total_num_cores);
                return XRAN_STATUS_FAIL;
        }
    } else if (fh_cfg->ru_conf.xranCat == XRAN_CATEGORY_B && fh_init->xran_ports == 1) {
        switch(total_num_cores) {
            case 1: /** only timing core */
                print_err("unsupported configuration Cat %d numports %d total_num_cores = %d\n", fh_cfg->ru_conf.xranCat, fh_init->xran_ports, total_num_cores);
                return XRAN_STATUS_FAIL;
            break;
            case 2:
                eth_ctx->time_wrk_cfg.f     = xran_eth_trx_tasks;
                eth_ctx->time_wrk_cfg.arg   = NULL;
                eth_ctx->time_wrk_cfg.state = 1;

                p_dev->tx_sym_gen_func = xran_process_tx_sym_cp_on_opt;

                pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                if(pThCtx == NULL){
                    print_err("pThCtx allocation error\n");
                    return XRAN_STATUS_FAIL;
                }
                memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                pThCtx->worker_id    = 0;
                pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_rx_bbdev", core_map[pThCtx->worker_id]);
                pThCtx->task_func = ring_processing_func;
                pThCtx->task_arg  = NULL;
                eth_ctx->pkt_wrk_cfg[0].f     = xran_generic_worker_thread;
                eth_ctx->pkt_wrk_cfg[0].arg   = pThCtx;
            break;
            case 3:
                if(icx_cpu) {
                    /* timing core */
                    eth_ctx->time_wrk_cfg.f     = xran_eth_trx_tasks;
                    eth_ctx->time_wrk_cfg.arg   = NULL;
                    eth_ctx->time_wrk_cfg.state = 1;

                    /* workers */
                    /** 0 **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id      = 0;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_rx_bbdev", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = ring_processing_func;
                    pThCtx->task_arg  = NULL;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    for (i = 0; i < fh_init->xran_ports; i++) {
                        struct xran_device_ctx * p_dev_update =  xran_dev_get_ctx_by_id(i);
                        if(p_dev_update == NULL) {
                            print_err("p_dev_update\n");
                            return XRAN_STATUS_FAIL;
                        }
                        p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_DL] = pThCtx->worker_id;
                        p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_UL] = pThCtx->worker_id;
                        printf("p:%d XRAN_JOB_TYPE_CP_DL worker id %d\n", i,  p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_DL]);
                        printf("p:%d XRAN_JOB_TYPE_CP_UL worker id %d\n", i,  p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_UL]);
                    }

                    /** 1 - CP GEN **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id      = 1;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_cp_gen", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = xran_dl_pkt_ring_processing_func;
                    pThCtx->task_arg  = (void*)xran_port_mask;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;
                } else {
                    print_err("unsupported configuration Cat %d numports %d total_num_cores = %d\n", fh_cfg->ru_conf.xranCat, fh_init->xran_ports, total_num_cores);
                    return XRAN_STATUS_FAIL;
                }
            break;
            case 4:
                if(icx_cpu) {
                    /* timing core */
                    eth_ctx->time_wrk_cfg.f     = xran_eth_trx_tasks;
                    eth_ctx->time_wrk_cfg.arg   = NULL;
                    eth_ctx->time_wrk_cfg.state = 1;

                    /* workers */
                    /** 0 **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id      = 0;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_rx_bbdev", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = ring_processing_func;
                    pThCtx->task_arg  = NULL;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 1 - CP GEN **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id      = 1;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_cp_gen", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = xran_dl_pkt_ring_processing_func;
                    pThCtx->task_arg  = (void*)(((1<<1) | (1<<2) |(1<<0)) & xran_port_mask);
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 2 UP GEN **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id    = 2;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_tx_gen", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = xran_dl_pkt_ring_processing_func;
                    pThCtx->task_arg  = (void*)((1<<0) & xran_port_mask);
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    for (i = 1; i < fh_init->xran_ports; i++) {
                        struct xran_device_ctx * p_dev_update =  xran_dev_get_ctx_by_id(i);
                        if(p_dev_update == NULL) {
                            print_err("p_dev_update\n");
                            return XRAN_STATUS_FAIL;
                        }
                        p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_DL] = pThCtx->worker_id;
                        p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_UL] = pThCtx->worker_id;
                        printf("p:%d XRAN_JOB_TYPE_CP_DL worker id %d\n", i,  p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_DL]);
                        printf("p:%d XRAN_JOB_TYPE_CP_UL worker id %d\n", i,  p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_UL]);
                    }
                } else {
                    print_err("unsupported configuration Cat %d numports %d total_num_cores = %d\n", fh_cfg->ru_conf.xranCat, fh_init->xran_ports, total_num_cores);
                    return XRAN_STATUS_FAIL;
                }
                break;
            case 5:
                if(icx_cpu) {
                    /* timing core */
                    eth_ctx->time_wrk_cfg.f     = xran_eth_rx_tasks;
                    eth_ctx->time_wrk_cfg.arg   = NULL;
                    eth_ctx->time_wrk_cfg.state = 1;

                    /* workers */
                    /** 0 **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id      = 0;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_rx_bbdev", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = ring_processing_func;
                    pThCtx->task_arg  = NULL;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 1 - CP GEN **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id      = 1;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_cp_gen", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = xran_dl_pkt_ring_processing_func;
                    pThCtx->task_arg  = (void*)(((1<<1) | (1<<2) |(1<<0)) & xran_port_mask);
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 2 UP GEN **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id    = 2;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_tx_gen", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = xran_dl_pkt_ring_processing_func;
                    pThCtx->task_arg  = (void*)((1<<0) & xran_port_mask);
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 3 UP GEN **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id    = 3;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_tx_gen", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = xran_dl_pkt_ring_processing_func;
                    pThCtx->task_arg  = (void*)((1<<0) & xran_port_mask);
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    for (i = 1; i < fh_init->xran_ports; i++) {
                        struct xran_device_ctx * p_dev_update =  xran_dev_get_ctx_by_id(i);
                        if(p_dev_update == NULL) {
                            print_err("p_dev_update\n");
                            return XRAN_STATUS_FAIL;
                        }
                        p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_DL] = pThCtx->worker_id;
                        p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_UL] = pThCtx->worker_id;
                        printf("p:%d XRAN_JOB_TYPE_CP_DL worker id %d\n", i,  p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_DL]);
                        printf("p:%d XRAN_JOB_TYPE_CP_UL worker id %d\n", i,  p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_UL]);
                    }
                } else {
                    print_err("unsupported configuration Cat %d numports %d total_num_cores = %d\n", fh_cfg->ru_conf.xranCat, fh_init->xran_ports, total_num_cores);
                    return XRAN_STATUS_FAIL;
                }
                break;
            case 6:
                if(eth_ctx->io_cfg.id == O_DU) {
                    /* timing core */
                    eth_ctx->time_wrk_cfg.f     = xran_eth_rx_tasks;
                    eth_ctx->time_wrk_cfg.arg   = NULL;
                    eth_ctx->time_wrk_cfg.state = 1;

                    /* workers */
                    /** 0 **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id      = 0;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_rx_bbdev", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = ring_processing_func;
                    pThCtx->task_arg  = NULL;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 1 Eth Tx **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);

                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id = 1;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_eth_tx", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = process_dpdk_io_tx;
                    pThCtx->task_arg  = (void*)2;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 2 - CP GEN **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id      = 2;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_cp_gen", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = xran_dl_pkt_ring_processing_func;
                    pThCtx->task_arg  = (void*)(((1<<1) | (1<<2) |(1<<0)) & xran_port_mask);
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 3 UP GEN **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id    = 3;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_tx_gen", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = xran_dl_pkt_ring_processing_func;
                    pThCtx->task_arg  = (void*)((1<<0) & xran_port_mask);
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 4 UP GEN **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id    = 4;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_tx_gen", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = xran_dl_pkt_ring_processing_func;
                    pThCtx->task_arg  = (void*)((1<<0) & xran_port_mask);
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    for (i = 0; i < fh_init->xran_ports; i++) {
                        struct xran_device_ctx * p_dev_update =  xran_dev_get_ctx_by_id(i);
                        if(p_dev_update == NULL) {
                            print_err("p_dev_update\n");
                            return XRAN_STATUS_FAIL;
                        }
                        p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_DL] = 0; //pThCtx->worker_id;
                        p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_UL] = 0; //pThCtx->worker_id;
                        printf("p:%d XRAN_JOB_TYPE_CP_DL worker id %d\n", i,  p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_DL]);
                        printf("p:%d XRAN_JOB_TYPE_CP_UL worker id %d\n", i,  p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_UL]);
                    }
                } else if(eth_ctx->io_cfg.id == O_RU) {
                    /*** O_RU specific config */
                    /* timing core */
                    eth_ctx->time_wrk_cfg.f     = NULL;
                    eth_ctx->time_wrk_cfg.arg   = NULL;
                    eth_ctx->time_wrk_cfg.state = 1;

                    /* workers */
                    /** 0  Eth RX */
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id = 0;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_eth_rx", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = process_dpdk_io_rx;
                    pThCtx->task_arg  = NULL;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 1  FH RX and BBDEV */
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id = 1;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_rx_p0", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = ring_processing_func_per_port;
                    pThCtx->task_arg  = (void*)0;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 2  FH RX and BBDEV */
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id = 2;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_rx_p1", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = ring_processing_func_per_port;
                    pThCtx->task_arg  = (void*)1;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 3  FH RX and BBDEV */
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id = 3;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_rx_p2", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = ring_processing_func_per_port;
                    pThCtx->task_arg  = (void*)2;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /**  FH TX and BBDEV */
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id = 4;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_eth_tx", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = process_dpdk_io_tx;
                    pThCtx->task_arg  = (void*)2;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;
                } else {
                    print_err("unsupported configuration Cat %d numports %d total_num_cores = %d\n", fh_cfg->ru_conf.xranCat, fh_init->xran_ports, total_num_cores);
                    return XRAN_STATUS_FAIL;
                }
                break;
            default:
                print_err("unsupported configuration\n");
                return XRAN_STATUS_FAIL;
        }
    } else if (fh_cfg->ru_conf.xranCat == XRAN_CATEGORY_B && fh_init->xran_ports > 1) {
        switch(total_num_cores) {
            case 1:
            case 2:
                print_err("unsupported configuration Cat %d numports %d total_num_cores = %d\n", fh_cfg->ru_conf.xranCat, fh_init->xran_ports, total_num_cores);
                return XRAN_STATUS_FAIL;
            break;
            case 3:
                if(icx_cpu) {
                    /* timing core */
                    eth_ctx->time_wrk_cfg.f     = xran_eth_trx_tasks;
                    eth_ctx->time_wrk_cfg.arg   = NULL;
                    eth_ctx->time_wrk_cfg.state = 1;

                    /* workers */
                    /** 0 **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id      = 0;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_rx_bbdev", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = ring_processing_func;
                    pThCtx->task_arg  = NULL;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    for (i = 1; i < fh_init->xran_ports; i++) {
                        struct xran_device_ctx * p_dev_update =  xran_dev_get_ctx_by_id(i);
                        if(p_dev_update == NULL) {
                            print_err("p_dev_update\n");
                            return XRAN_STATUS_FAIL;
                        }
                        p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_DL] = pThCtx->worker_id;
                        p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_UL] = pThCtx->worker_id;
                        printf("p:%d XRAN_JOB_TYPE_CP_DL worker id %d\n", i,  p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_DL]);
                        printf("p:%d XRAN_JOB_TYPE_CP_UL worker id %d\n", i,  p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_UL]);
                    }

                    /** 1 - CP GEN **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id      = 1;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_cp_gen", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = xran_dl_pkt_ring_processing_func;
                    pThCtx->task_arg  = (void*)xran_port_mask;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;
                } else {
                    print_err("unsupported configuration Cat %d numports %d total_num_cores = %d\n", fh_cfg->ru_conf.xranCat, fh_init->xran_ports, total_num_cores);
                    return XRAN_STATUS_FAIL;
                }
            break;
            case 4:
                if(icx_cpu) {
                    /* timing core */
                    eth_ctx->time_wrk_cfg.f     = xran_eth_trx_tasks;
                    eth_ctx->time_wrk_cfg.arg   = NULL;
                    eth_ctx->time_wrk_cfg.state = 1;

                    /* workers */
                    /** 0 **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id      = 0;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_rx_bbdev", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = ring_processing_func;
                    pThCtx->task_arg  = NULL;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 1 - CP GEN **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id      = 1;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_cp_gen", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = xran_dl_pkt_ring_processing_func;
                    pThCtx->task_arg  = (void*)(((1<<1) | (1<<2)) & xran_port_mask);
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 2 UP GEN **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id    = 2;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_tx_gen", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = xran_dl_pkt_ring_processing_func;
                    pThCtx->task_arg  = (void*)((1<<0) & xran_port_mask);
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    for (i = 1; i < fh_init->xran_ports; i++) {
                        struct xran_device_ctx * p_dev_update =  xran_dev_get_ctx_by_id(i);
                        if(p_dev_update == NULL) {
                            print_err("p_dev_update\n");
                            return XRAN_STATUS_FAIL;
                        }
                        p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_DL] = pThCtx->worker_id;
                        p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_UL] = pThCtx->worker_id;
                        printf("p:%d XRAN_JOB_TYPE_CP_DL worker id %d\n", i,  p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_DL]);
                        printf("p:%d XRAN_JOB_TYPE_CP_UL worker id %d\n", i,  p_dev_update->job2wrk_id[XRAN_JOB_TYPE_CP_UL]);
                    }
                } else {
                    print_err("unsupported configuration Cat %d numports %d total_num_cores = %d\n", fh_cfg->ru_conf.xranCat, fh_init->xran_ports, total_num_cores);
                    return XRAN_STATUS_FAIL;
                }
            break;
            case 5:
                    /* timing core */
                    eth_ctx->time_wrk_cfg.f     = xran_eth_trx_tasks;
                    eth_ctx->time_wrk_cfg.arg   = NULL;
                    eth_ctx->time_wrk_cfg.state = 1;

                    /* workers */
                    /** 0  FH RX and BBDEV */
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id = 0;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_rx_bbdev", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = ring_processing_func;
                    pThCtx->task_arg  = NULL;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 1 - CP GEN **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id = 1;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_cp_gen", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = xran_dl_pkt_ring_processing_func;
                    pThCtx->task_arg  = (void*)(1<<0);
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 2 UP GEN **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id = 2;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_up_gen", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = xran_dl_pkt_ring_processing_func;
                    pThCtx->task_arg  = (void*)(1<<1);
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 3 UP GEN **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id = 3;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_up_gen", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = xran_dl_pkt_ring_processing_func;
                    pThCtx->task_arg  = (void*)(1<<2);
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;
            break;
            case 6:
                if(eth_ctx->io_cfg.id == O_DU){
                    /* timing core */
                    eth_ctx->time_wrk_cfg.f     = xran_eth_trx_tasks;
                    eth_ctx->time_wrk_cfg.arg   = NULL;
                    eth_ctx->time_wrk_cfg.state = 1;

                    /* workers */
                    /** 0 **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id      = 0;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_rx_bbdev", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = ring_processing_func;
                    pThCtx->task_arg  = NULL;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 1 - CP GEN **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id      = 1;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_cp_gen", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = xran_processing_timer_only_func;
                    pThCtx->task_arg  = NULL;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 2 UP GEN **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id    = 2;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_tx_gen", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = xran_dl_pkt_ring_processing_func;
                    pThCtx->task_arg  = (void*)(1<<0);
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 3 UP GEN **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id    = 3;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_tx_gen", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = xran_dl_pkt_ring_processing_func;
                    pThCtx->task_arg  = (void*)(1<<1);
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 4 UP GEN **/
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id    = 4;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_tx_gen", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = xran_dl_pkt_ring_processing_func;
                    pThCtx->task_arg  = (void*)(1<<2);
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;
                } else {
                    /*** O_RU specific config */
                    /* timing core */
                    eth_ctx->time_wrk_cfg.f     = NULL;
                    eth_ctx->time_wrk_cfg.arg   = NULL;
                    eth_ctx->time_wrk_cfg.state = 1;

                    /* workers */
                    /** 0  Eth RX */
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id = 0;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_eth_rx", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = process_dpdk_io_rx;
                    pThCtx->task_arg  = NULL;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 1  FH RX and BBDEV */
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id = 1;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_rx_p0", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = ring_processing_func_per_port;
                    pThCtx->task_arg  = (void*)0;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 2  FH RX and BBDEV */
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id = 2;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_rx_p1", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = ring_processing_func_per_port;
                    pThCtx->task_arg  = (void*)1;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /** 3  FH RX and BBDEV */
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id = 3;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_rx_p2", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = ring_processing_func_per_port;
                    pThCtx->task_arg  = (void*)2;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;

                    /**  FH TX and BBDEV */
                    pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
                    if(pThCtx == NULL){
                        print_err("pThCtx allocation error\n");
                        return XRAN_STATUS_FAIL;
                    }
                    memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
                    pThCtx->worker_id = 4;
                    pThCtx->worker_core_id = core_map[pThCtx->worker_id];
                    snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", "fh_eth_tx", core_map[pThCtx->worker_id]);
                    pThCtx->task_func = process_dpdk_io_tx;
                    pThCtx->task_arg  = (void*)2;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
                    eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;
                }
            break;
            default:
                print_err("unsupported configuration Cat %d numports %d total_num_cores = %d\n", fh_cfg->ru_conf.xranCat, fh_init->xran_ports, total_num_cores);
                return XRAN_STATUS_FAIL;
        }
    } else {
        print_err("unsupported configuration\n");
        return XRAN_STATUS_FAIL;
    }

    nWorkerCore = 1LL;
    if(eth_ctx->io_cfg.pkt_proc_core) {
        for (i = 0; i < coreNum && i < 64; i++) {
            if (nWorkerCore & (uint64_t)eth_ctx->io_cfg.pkt_proc_core) {
                xran_core_used[xran_num_cores_used++] = i;
                if (rte_eal_remote_launch(eth_ctx->pkt_wrk_cfg[eth_ctx->num_workers].f, eth_ctx->pkt_wrk_cfg[eth_ctx->num_workers].arg, i))
                    rte_panic("eth_ctx->pkt_wrk_cfg[eth_ctx->num_workers].f() failed to start\n");
                eth_ctx->pkt_wrk_cfg[i].state = 1;
                if(eth_ctx->pkt_proc_core_id == 0)
                    eth_ctx->pkt_proc_core_id = i;
                printf("spawn worker %d core %d\n",eth_ctx->num_workers, i);
                eth_ctx->worker_core[eth_ctx->num_workers++] = i;
            }
            nWorkerCore = nWorkerCore << 1;
        }
    }

    nWorkerCore = 1LL;
    if(eth_ctx->io_cfg.pkt_proc_core_64_127) {
        for (i = 64; i < coreNum && i < 128; i++) {
            if (nWorkerCore & (uint64_t)eth_ctx->io_cfg.pkt_proc_core_64_127) {
                xran_core_used[xran_num_cores_used++] = i;
                if (rte_eal_remote_launch(eth_ctx->pkt_wrk_cfg[eth_ctx->num_workers].f, eth_ctx->pkt_wrk_cfg[eth_ctx->num_workers].arg, i))
                    rte_panic("eth_ctx->pkt_wrk_cfg[eth_ctx->num_workers].f() failed to start\n");
                eth_ctx->pkt_wrk_cfg[i].state = 1;
                if(eth_ctx->pkt_proc_core_id == 0)
                    eth_ctx->pkt_proc_core_id = i;
                printf("spawn worker %d core %d\n",eth_ctx->num_workers, i);
                eth_ctx->worker_core[eth_ctx->num_workers++] = i;
            }
            nWorkerCore = nWorkerCore << 1;
        }
    }

    return XRAN_STATUS_SUCCESS;
}
int32_t
xran_open(void *pHandle, struct xran_fh_config* pConf)
{
    int32_t ret = XRAN_STATUS_SUCCESS;
    int32_t i;
    uint8_t nNumerology = 0;
    int32_t  lcore_id = 0;
    struct xran_device_ctx  *p_xran_dev_ctx = NULL;
    struct xran_fh_config   *pFhCfg  = NULL;
    struct xran_fh_init     *fh_init = NULL;
    struct xran_ethdi_ctx   *eth_ctx = xran_ethdi_get_ctx();
    int32_t wait_time = 10;
    int64_t offset_sec, offset_nsec;

     if(pConf->dpdk_port < XRAN_PORTS_NUM) {
        p_xran_dev_ctx  = xran_dev_get_ctx_by_id(pConf->dpdk_port);
    } else {
        print_err("@0x%08p [ru %d ] pConf->dpdk_port > XRAN_PORTS_NUM\n", pConf,  pConf->dpdk_port);
        return XRAN_STATUS_FAIL;
    }

    if(p_xran_dev_ctx == NULL) {
        print_err("[ru %d] p_xran_dev_ctx == NULL ", pConf->dpdk_port);
        return XRAN_STATUS_FAIL;
    }

    pFhCfg = &p_xran_dev_ctx->fh_cfg;
    memcpy(pFhCfg, pConf, sizeof(struct xran_fh_config));

    fh_init = &p_xran_dev_ctx->fh_init;
    if(fh_init == NULL)
        return XRAN_STATUS_FAIL;

    if(pConf->log_level) {
        printf(" %s: %s Category %s\n", __FUNCTION__,
        (pFhCfg->ru_conf.xranTech == XRAN_RAN_5GNR) ? "5G NR" : "LTE",
        (pFhCfg->ru_conf.xranCat == XRAN_CATEGORY_A) ? "A" : "B");
    }

    p_xran_dev_ctx->enableCP    = pConf->enableCP;
    p_xran_dev_ctx->enablePrach = pConf->prachEnable;
    p_xran_dev_ctx->enableSrs   = pConf->srsEnable;
    p_xran_dev_ctx->puschMaskEnable = pConf->puschMaskEnable;
    p_xran_dev_ctx->puschMaskSlot = pConf->puschMaskSlot;
    p_xran_dev_ctx->DynamicSectionEna = pConf->DynamicSectionEna;

    if(pConf->GPS_Alpha || pConf->GPS_Beta ){
        offset_sec = pConf->GPS_Beta / 100;    /* resolution of beta is 10ms */
        offset_nsec = (pConf->GPS_Beta - offset_sec * 100) * 1e7 + pConf->GPS_Alpha;
        p_xran_dev_ctx->offset_sec = offset_sec;
        p_xran_dev_ctx->offset_nsec = offset_nsec;
    }else {
        p_xran_dev_ctx->offset_sec  = 0;
        p_xran_dev_ctx->offset_nsec = 0;
    }


    nNumerology = xran_get_conf_numerology(p_xran_dev_ctx);

    if (pConf->nCC > XRAN_MAX_SECTOR_NR) {
        if(pConf->log_level)
            printf("Number of cells %d exceeds max number supported %d!\n", pConf->nCC, XRAN_MAX_SECTOR_NR);
        pConf->nCC = XRAN_MAX_SECTOR_NR;
    }

    if(pConf->ru_conf.iqOrder != XRAN_I_Q_ORDER  || pConf->ru_conf.byteOrder != XRAN_NE_BE_BYTE_ORDER ) {
        print_err("Byte order and/or IQ order is not supported [IQ %d byte %d]\n", pConf->ru_conf.iqOrder, pConf->ru_conf.byteOrder);
        return XRAN_STATUS_FAIL;
    }

    if(p_xran_dev_ctx->fh_init.io_cfg.id == O_RU) {
        if((ret = xran_ruemul_init(p_xran_dev_ctx)) < 0) {
            return ret;
        }
    }

    /* setup PRACH configuration for C-Plane */
    if(pConf->ru_conf.xranTech == XRAN_RAN_5GNR) {
        if((ret  = xran_init_prach(pConf, p_xran_dev_ctx))< 0){
            return ret;
        }
    } else if (pConf->ru_conf.xranTech == XRAN_RAN_LTE) {
        if((ret  =  xran_init_prach_lte(pConf, p_xran_dev_ctx))< 0){
            return ret;
        }
    }

    if((ret  = xran_init_srs(pConf, p_xran_dev_ctx))< 0){
        return ret;
    }

    if((ret  = xran_cp_init_sectiondb(p_xran_dev_ctx)) < 0){
        return ret;
    }

    if((ret  = xran_init_sectionid(p_xran_dev_ctx)) < 0){
        return ret;
    }

    if((ret  = xran_init_seqid(p_xran_dev_ctx)) < 0){
        return ret;
    }

    if((uint16_t)eth_ctx->io_cfg.port[XRAN_UP_VF] != 0xFFFF){
        if((ret  = xran_init_vfs_mapping(p_xran_dev_ctx)) < 0) {
            return ret;
        }

        if(p_xran_dev_ctx->fh_init.io_cfg.id == O_DU && p_xran_dev_ctx->fh_init.io_cfg.num_rxq > 1) {
            if((ret  = xran_init_vf_rxq_to_pcid_mapping(p_xran_dev_ctx)) < 0) {
                return ret;
            }
        }
    }

    if(pConf->ru_conf.xran_max_frame) {
       xran_max_frame = pConf->ru_conf.xran_max_frame;
       printf("xran_max_frame %d\n", xran_max_frame);
    }

    p_xran_dev_ctx->interval_us_local = xran_fs_get_tti_interval(nNumerology);
    if (interval_us > p_xran_dev_ctx->interval_us_local)
    {
        interval_us = xran_fs_get_tti_interval(nNumerology); //only update interval_us based on maximum numerology
    }

//    if(pConf->log_level){
        printf("%s: interval_us=%ld, interval_us_local=%d\n", __FUNCTION__, interval_us, p_xran_dev_ctx->interval_us_local);
//    }
    if (nNumerology >= timing_get_numerology())
    {
    timing_set_numerology(nNumerology);
    }

    for(i = 0 ; i <pConf->nCC; i++){
        xran_fs_set_slot_type(pConf->dpdk_port, i, pConf->frame_conf.nFrameDuplexType, pConf->frame_conf.nTddPeriod,
            pConf->frame_conf.sSlotConfig);
    }

    xran_fs_slot_limit_init(pConf->dpdk_port, xran_fs_get_tti_interval(nNumerology));

    /* if send_xpmbuf2ring needs to be changed from default functions,
     * then those should be set between xran_init and xran_open */
    if(p_xran_dev_ctx->send_cpmbuf2ring == NULL)
        p_xran_dev_ctx->send_cpmbuf2ring    = xran_ethdi_mbuf_send_cp;
    if(p_xran_dev_ctx->send_upmbuf2ring == NULL)
        p_xran_dev_ctx->send_upmbuf2ring    = xran_ethdi_mbuf_send;

    if(pFhCfg->ru_conf.xranCat == XRAN_CATEGORY_A) {
        if(p_xran_dev_ctx->tx_sym_gen_func == NULL )
            p_xran_dev_ctx->tx_sym_gen_func = xran_process_tx_sym_cp_on_opt;
    } else {
        if(p_xran_dev_ctx->tx_sym_gen_func == NULL )
            p_xran_dev_ctx->tx_sym_gen_func = xran_process_tx_sym_cp_on_dispatch_opt;
    }

    if(pConf->dpdk_port == 0) {
        /* create all thread on open of port 0 */
        xran_num_cores_used = 0;
        if(eth_ctx->io_cfg.bbdev_mode != XRAN_BBDEV_NOT_USED){
            eth_ctx->bbdev_dec = pConf->bbdev_dec;
            eth_ctx->bbdev_enc = pConf->bbdev_enc;
        }

        if((uint16_t)eth_ctx->io_cfg.port[XRAN_UP_VF] != 0xFFFF){
            printf("XRAN_UP_VF: 0x%04x\n", eth_ctx->io_cfg.port[XRAN_UP_VF]);
            p_xran_dev_ctx->timing_source_thread_running = 0;
            xran_core_used[xran_num_cores_used++] = eth_ctx->io_cfg.timing_core;
            if (rte_eal_remote_launch(xran_timing_source_thread, xran_dev_get_ctx(), eth_ctx->io_cfg.timing_core))
            rte_panic("thread_run() failed to start\n");
        } else if(pConf->log_level) {
                printf("Eth port was not open. Processing thread was not started\n");
        }
    } else {
        if((uint16_t)eth_ctx->io_cfg.port[XRAN_UP_VF] != 0xFFFF) {
            if ((ret = xran_timing_create_cbs(p_xran_dev_ctx)) < 0) {
                return ret;
            }
        }
    }

    if((uint16_t)eth_ctx->io_cfg.port[XRAN_UP_VF] != 0xFFFF){
        if(pConf->dpdk_port == (fh_init->xran_ports - 1)) {
            if((ret = xran_spawn_workers()) < 0) {
                return ret;
                }
            }
        printf("%s [CPU %2d] [PID: %6d]\n", __FUNCTION__,  sched_getcpu(), getpid());
        printf("Waiting on Timing thread...\n");
        while (p_xran_dev_ctx->timing_source_thread_running == 0 && wait_time--) {
            usleep(100);
        }
    }

    print_dbg("%s : %d", __FUNCTION__, pConf->dpdk_port);
    return ret;
}

int32_t
xran_start(void *pHandle)
{
    struct tm * ptm;
    /* ToS = Top of Second start +- 1.5us */
    struct timespec ts;
    char buff[100];

    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    if(xran_get_if_state() == XRAN_RUNNING) {
        print_err("Already STARTED!!");
        return (-1);
        }
    timespec_get(&ts, TIME_UTC);
    ptm = gmtime(&ts.tv_sec);
    if(ptm){
        strftime(buff, sizeof(buff), "%D %T", ptm);
        printf("%s: XRAN start time: %s.%09ld UTC [%ld]\n",
            (p_xran_dev_ctx->fh_init.io_cfg.id == O_DU ? "O-DU": "O-RU"), buff, ts.tv_nsec, interval_us);
    }

    if (p_xran_dev_ctx->fh_init.io_cfg.eowd_cmn[p_xran_dev_ctx->fh_init.io_cfg.id].owdm_enable)
        {
        xran_if_current_state = XRAN_OWDM;
        }
    else
        {
    xran_if_current_state = XRAN_RUNNING;
        }
    return 0;
}

int32_t
xran_stop(void *pHandle)
{
    if(xran_get_if_state() == XRAN_STOPPED) {
        print_err("Already STOPPED!!");
        return (-1);
        }

    xran_if_current_state = XRAN_STOPPED;
    return 0;
}

int32_t
xran_close(void *pHandle)
{
    int32_t ret = XRAN_STATUS_SUCCESS;
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();

    xran_if_current_state = XRAN_STOPPED;
    ret = xran_cp_free_sectiondb(p_xran_dev_ctx);

    if(p_xran_dev_ctx->fh_init.io_cfg.id == O_RU)
        xran_ruemul_release(p_xran_dev_ctx);

#ifdef RTE_LIBRTE_PDUMP
    /* uninitialize packet capture framework */
    rte_pdump_uninit();
#endif
    return ret;
}

/* send_cpmbuf2ring and send_upmbuf2ring should be set between xran_init and xran_open
 * each cb will be set by default duing open if it is set by NULL */
int32_t
xran_register_cb_mbuf2ring(xran_ethdi_mbuf_send_fn mbuf_send_cp, xran_ethdi_mbuf_send_fn mbuf_send_up)
{
    struct xran_device_ctx *p_xran_dev_ctx;

    if(xran_get_if_state() == XRAN_RUNNING) {
        print_err("Cannot register callback while running!!\n");
        return (-1);
        }

    p_xran_dev_ctx = xran_dev_get_ctx();

    p_xran_dev_ctx->send_cpmbuf2ring    = mbuf_send_cp;
    p_xran_dev_ctx->send_upmbuf2ring    = mbuf_send_up;

    p_xran_dev_ctx->tx_sym_gen_func = xran_process_tx_sym_cp_on_opt;

    return (0);
}

int32_t
xran_get_slot_idx (uint32_t PortId, uint32_t *nFrameIdx, uint32_t *nSubframeIdx,  uint32_t *nSlotIdx, uint64_t *nSecond)
{
    int32_t tti = 0;
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx_by_id(PortId);
    if (!p_xran_dev_ctx)
{
        print_err("Null xRAN context on port id %u!!\n", PortId);
        return 0;
}

    tti           = (int32_t)XranGetTtiNum(xran_lib_ota_sym_idx[PortId], XRAN_NUM_OF_SYMBOL_PER_SLOT);
    *nSlotIdx     = (uint32_t)XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME(p_xran_dev_ctx->interval_us_local));
    *nSubframeIdx = (uint32_t)XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME(p_xran_dev_ctx->interval_us_local),  SUBFRAMES_PER_SYSTEMFRAME);
    *nFrameIdx    = (uint32_t)XranGetFrameNum(tti,xran_getSfnSecStart(),SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(p_xran_dev_ctx->interval_us_local));
    *nSecond      = timing_get_current_second();

    return tti;
}

int32_t
xran_set_debug_stop(int32_t value, int32_t count)
{
    return timing_set_debug_stop(value, count);
    }
