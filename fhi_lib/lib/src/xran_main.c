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
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_ring.h>

#include "xran_fh_lls_cu.h"

#include "ethdi.h"
#include "xran_pkt.h"
#include "xran_up_api.h"
#include "xran_cp_api.h"
#include "xran_sync_api.h"
#include "xran_lib_mlog_tasks_id.h"
#include "xran_timer.h"
#include "xran_common.h"
#include "xran_printf.h"

#ifndef MLOG_ENABLED
#include "mlog_lnx_xRAN.h"
#else
#include "mlog_lnx.h"
#endif

#define DIV_ROUND_OFFSET(X,Y) ( X/Y + ((X%Y)?1:0) )

#define XranOffsetSym(offSym, otaSym, numSymTotal)  (((int32_t)offSym > (int32_t)otaSym) ? \
                            ((int32_t)otaSym + ((int32_t)numSymTotal) - (uint32_t)offSym) : \
                            (((int32_t)otaSym - (int32_t)offSym) >= numSymTotal) ?  \
                                    (((int32_t)otaSym - (int32_t)offSym) - numSymTotal) : \
                                    ((int32_t)otaSym - (int32_t)offSym))

#define MAX_NUM_OF_XRAN_CTX       (2)
#define XranIncrementCtx(ctx)                             ((ctx >= (MAX_NUM_OF_XRAN_CTX-1)) ? 0 : (ctx+1))
#define XranDecrementCtx(ctx)                             ((ctx == 0) ? (MAX_NUM_OF_XRAN_CTX-1) : (ctx-1))

struct xran_timer_ctx {
    uint32_t    tti_to_process;
};

static XranLibHandleInfoStruct DevHandle;
static struct xran_lib_ctx g_xran_lib_ctx = { 0 };

struct xran_timer_ctx timer_ctx[MAX_NUM_OF_XRAN_CTX];

static struct rte_timer tti_to_phy_timer[10];
static struct rte_timer tti_timer;
static struct rte_timer sym_timer;
static struct rte_timer tx_cp_dl_timer;
static struct rte_timer tx_cp_ul_timer;
static struct rte_timer tx_up_timer;

static long interval_us = 125;

uint32_t xran_lib_ota_tti        = 0; /* [0:7999] */
uint32_t xran_lib_ota_sym        = 0; /* [0:7] */
uint32_t xran_lib_ota_sym_idx    = 0; /* [0 : 14*8*1000-1] */

uint64_t xran_lib_gps_second = 0;

static uint8_t xran_cp_seq_id_num[XRAN_MAX_CELLS_PER_PORT][XRAN_DIR_MAX][XRAN_MAX_ANTENNA_NR];
static uint8_t xran_section_id_curslot[XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR];
static uint16_t xran_section_id[XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR];

void xran_timer_arm(struct rte_timer *tim, void* arg);
int xran_process_tx_sym(void *arg);

int xran_process_rx_sym(void *arg,
                        void *iq_data_start,
                        uint16_t size,
                        uint8_t CC_ID,
                        uint8_t Ant_ID,
                        uint8_t frame_id,
                        uint8_t subframe_id,
                        uint8_t slot_id,
                        uint8_t symb_id);

void tti_ota_cb(struct rte_timer *tim, void *arg);
void tti_to_phy_cb(struct rte_timer *tim, void *arg);
void xran_timer_arm_ex(struct rte_timer *tim, void* CbFct, void *CbArg, unsigned tim_lcore);

struct xran_lib_ctx *xran_lib_get_ctx(void)
{
    return &g_xran_lib_ctx;
}

static inline XRANFHCONFIG *xran_lib_get_ctx_fhcfg(void)
{
    return (&(xran_lib_get_ctx()->xran_fh_cfg));
}

inline uint16_t xran_get_beamid(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id)
{
    return (0);     // NO BEAMFORMING
}

int xran_init_sectionid(void *pHandle)
{
  int cell, dir, ant;

    for(cell=0; cell < XRAN_MAX_CELLS_PER_PORT; cell++) {
        for(ant=0; ant < XRAN_MAX_ANTENNA_NR; ant++) {
            xran_section_id[cell][ant] = 0;
            xran_section_id_curslot[cell][ant] = 255;
            }
        }

    return (0);
}

int xran_init_seqid(void *pHandle)
{
  int cell, dir, ant;

    for(cell=0; cell < XRAN_MAX_CELLS_PER_PORT; cell++) {
        for(dir=0; dir < XRAN_DIR_MAX; dir++) {
            for(ant=0; ant < XRAN_MAX_ANTENNA_NR; ant++) {
                xran_cp_seq_id_num[cell][dir][ant] = 0;
                }
            }
        }

    return (0);
}

inline uint16_t xran_alloc_sectionid(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id)
{
    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (0);
        }
    if(ant_id >= XRAN_MAX_ANTENNA_NR) {
        print_err("Invalid antenna ID - %d", ant_id);
        return (0);
        }

    /* if new slot has been started,
     * then initializes section id again for new start */
    if(xran_section_id_curslot[cc_id][ant_id] != slot_id) {
        xran_section_id[cc_id][ant_id] = 0;
        xran_section_id_curslot[cc_id][ant_id] = slot_id;
        }
  
    return(xran_section_id[cc_id][ant_id]++);
}

inline uint8_t xran_get_seqid(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id)
{
    if(dir >= XRAN_DIR_MAX) {
        print_err("Invalid direction - %d", dir);
        return (0);
        }
    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (0);
        }
    if(ant_id >= XRAN_MAX_ANTENNA_NR) {
        print_err("Invalid antenna ID - %d", ant_id);
        return (0);
        }

    return(xran_cp_seq_id_num[cc_id][dir][ant_id]++);
}

inline int xran_update_seqid(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id, uint8_t seq_id)
{
    return (0);
}

//////////////////////////////////////////
// For RU emulation
static struct xran_section_gen_info cpSections[255];
static struct xran_cp_gen_params cpInfo;
int process_cplane(struct rte_mbuf *pkt)
{
  int xran_parse_cp_pkt(struct rte_mbuf *mbuf, struct xran_cp_gen_params *result);

    cpInfo.sections = cpSections;
    xran_parse_cp_pkt(pkt, &cpInfo);

    return (0);
}
//////////////////////////////////////////

void sym_ota_cb(struct rte_timer *tim, void *arg)
{
    uint8_t offset = 0;
    struct xran_lib_ctx * p_xran_lib_ctx = xran_lib_get_ctx();
    struct xran_timer_ctx *pTCtx = (struct xran_timer_ctx *)arg;
    long t1 = MLogTick();

    if(XranGetSymNum(xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT) == 0){
        tti_ota_cb(NULL, arg);
    }

    if(XranGetSymNum(xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT) == 1){
        if(p_xran_lib_ctx->phy_tti_cb_done == 0){
            uint64_t t3 = MLogTick();
            /* rearm timer to deliver TTI event to PHY */
            p_xran_lib_ctx->phy_tti_cb_done = 0;
            xran_timer_arm_ex(&tti_to_phy_timer[xran_lib_ota_tti % 10], tti_to_phy_cb, (void*)pTCtx, p_xran_lib_ctx->xran_init_cfg.io_cfg.pkt_proc_core);
            MLogTask(PID_TIME_ARM_TIMER, t3, MLogTick());
        }
    }

    xran_process_tx_sym(timer_ctx);
    /* check if there is call back to do something else on this symbol */
    if(p_xran_lib_ctx->pSymCallback[0][xran_lib_ota_sym])
        p_xran_lib_ctx->pSymCallback[0][xran_lib_ota_sym](&tx_cp_dl_timer, p_xran_lib_ctx->pSymCallbackTag[0][xran_lib_ota_sym]);

    xran_lib_ota_sym++;
    if(xran_lib_ota_sym >= N_SYM_PER_SLOT){
        xran_lib_ota_sym=0;
    }
    MLogTask(PID_SYM_OTA_CB, t1, MLogTick());
}

void tti_ota_cb(struct rte_timer *tim, void *arg)
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
    struct xran_timer_ctx *pTCtx = (struct xran_timer_ctx *)arg;
    struct xran_lib_ctx * p_xran_lib_ctx = xran_lib_get_ctx();

    MLogTask(PID_TTI_TIMER, t1, MLogTick());

    /* To match TTbox */
    if(xran_lib_ota_tti == 0)
        reg_tti = 8000-1;
    else
        reg_tti = xran_lib_ota_tti -1;
    MLogIncrementCounter();
    /* subframe and slot */
    MLogRegisterFrameSubframe(((reg_tti/SLOTNUM_PER_SUBFRAME) % SUBFRAMES_PER_SYSTEMFRAME),
        reg_tti % (SLOTNUM_PER_SUBFRAME));
    MLogMark(1, t1);

    slot_id     = XranGetSlotNum(xran_lib_ota_tti, SLOTNUM_PER_SUBFRAME);
    subframe_id = XranGetSubFrameNum(xran_lib_ota_tti,SLOTNUM_PER_SUBFRAME,  SUBFRAMES_PER_SYSTEMFRAME);
    frame_id    = XranGetFrameNum(xran_lib_ota_tti,SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME);

    pTCtx[(xran_lib_ota_tti & 1) ^ 1].tti_to_process = xran_lib_ota_tti;

    mlogVar[mlogVarCnt++] = 0x11111111;
    mlogVar[mlogVarCnt++] = xran_lib_ota_tti;
    mlogVar[mlogVarCnt++] = xran_lib_ota_sym_idx;
    mlogVar[mlogVarCnt++] = xran_lib_ota_sym_idx / 14;
    mlogVar[mlogVarCnt++] = frame_id;
    mlogVar[mlogVarCnt++] = subframe_id;
    mlogVar[mlogVarCnt++] = slot_id;
    mlogVar[mlogVarCnt++] = 0;
    MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());

    if(p_xran_lib_ctx->xran_init_cfg.io_cfg.id == ID_LLS_CU)
        next_tti = xran_lib_ota_tti + 1;
    else
        next_tti = xran_lib_ota_tti;

    if(next_tti>= SLOTNUM_PER_SUBFRAME*1000){
        print_dbg("[%d]SFN %d sf %d slot %d\n",next_tti, frame_id, subframe_id, slot_id);
        next_tti=0;
    }
    /* [0 - 7] */
    slot_id     = XranGetSlotNum(next_tti, SLOTNUM_PER_SUBFRAME);
    /* sf [0 - 9] */
    subframe_id = XranGetSubFrameNum(next_tti,SLOTNUM_PER_SUBFRAME,  SUBFRAMES_PER_SYSTEMFRAME);
    /* frame [0 - 99] for now */
    frame_id    = XranGetFrameNum(next_tti,SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME);

    print_dbg("[%d]SFN %d sf %d slot %d\n",next_tti, frame_id, subframe_id, slot_id);

    if(p_xran_lib_ctx->xran_init_cfg.io_cfg.id == ID_LLS_CU){
        pTCtx[(xran_lib_ota_tti & 1)].tti_to_process = next_tti;
    } else {
        pTCtx[(xran_lib_ota_tti & 1)].tti_to_process = pTCtx[(xran_lib_ota_tti & 1)^1].tti_to_process;
    }

    t3 = MLogTick();
    p_xran_lib_ctx->phy_tti_cb_done = 0;
    xran_timer_arm_ex(&tti_to_phy_timer[xran_lib_ota_tti % 10], tti_to_phy_cb, (void*)pTCtx, p_xran_lib_ctx->xran_init_cfg.io_cfg.pkt_proc_core);
    MLogTask(PID_TIME_ARM_TIMER, t3, MLogTick());

    xran_lib_ota_tti++;
    /* within 1 sec we have 8000 TTIs as 1000ms/0.125ms where TTI is 125us*/
    if(xran_lib_ota_tti >= SLOTNUM_PER_SUBFRAME*1000){
        print_dbg("[%d]SFN %d sf %d slot %d\n",xran_lib_ota_tti, frame_id, subframe_id, slot_id);
        xran_lib_ota_tti=0;
    }
    MLogTask(PID_TTI_CB, t1, MLogTick());
}

void xran_timer_arm(struct rte_timer *tim, void* arg)
{
    struct xran_lib_ctx * p_xran_lib_ctx = xran_lib_get_ctx();
    if (xran_if_current_state == XRAN_RUNNING){
        rte_timer_cb_t fct = (rte_timer_cb_t)arg;
        rte_timer_reset_sync(tim, 0, SINGLE, p_xran_lib_ctx->xran_init_cfg.io_cfg.pkt_proc_core, fct, timer_ctx);
    }
}

void xran_timer_arm_ex(struct rte_timer *tim, void* CbFct, void *CbArg, unsigned tim_lcore)
{
    struct xran_lib_ctx * p_xran_lib_ctx = xran_lib_get_ctx();
    if (xran_if_current_state == XRAN_RUNNING){
        rte_timer_cb_t fct = (rte_timer_cb_t)CbFct;
        rte_timer_init(tim);
        rte_timer_reset_sync(tim, 0, SINGLE, tim_lcore, fct, CbArg);
    }
}

void tx_cp_dl_cb(struct rte_timer *tim, void *arg)
{
  long t1 = MLogTick();
  int tti, sym;
  uint32_t slot_id, subframe_id, frame_id;
  int ant_id;
  int32_t cc_id = 0;
  uint16_t beam_id;
  uint8_t num_eAxc, num_CCPorts;
  void *pHandle;

  struct xran_lib_ctx * p_xran_lib_ctx = xran_lib_get_ctx();
  struct xran_timer_ctx *pTCtx = (struct xran_timer_ctx *)arg;


    pHandle = NULL;     // TODO: temp implemantation
    num_eAxc    = xran_get_num_eAxc(pHandle);
    num_CCPorts = xran_get_num_cc(pHandle);

    if(p_xran_lib_ctx->enableCP) {

        tti = pTCtx[(xran_lib_ota_tti & 1) ^ 1].tti_to_process;

        slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME);
        subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME,  SUBFRAMES_PER_SYSTEMFRAME);
        frame_id    = XranGetFrameNum(tti,SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME);

        print_dbg("[%d]SFN %d sf %d slot %d\n", tti, frame_id, subframe_id, slot_id);

        for(ant_id = 0; ant_id < num_eAxc; ++ant_id) {
            for(cc_id = 0; cc_id < num_CCPorts; cc_id++ ) {
                 // start new section information list
                xran_cp_reset_section_info(pHandle, XRAN_DIR_DL, cc_id, ant_id);

                beam_id = xran_get_beamid(pHandle, XRAN_DIR_DL, cc_id, ant_id, slot_id);

                send_cpmsg_dlul(pHandle, XRAN_DIR_DL,
                                frame_id, subframe_id, slot_id,
                                0, N_SYM_PER_SLOT, NUM_OF_PRB_IN_FULL_BAND,
                                beam_id, cc_id, ant_id,
                                xran_get_seqid(pHandle, XRAN_DIR_DL, cc_id, ant_id, slot_id));
                }
            }
        }
    MLogTask(PID_CP_DL_CB, t1, MLogTick());
}

void rx_ul_deadline_half_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogTick();
    struct xran_lib_ctx * p_xran_lib_ctx = xran_lib_get_ctx();
    XranStatusInt32 status;
    /* half of RX for current TTI as measured against current OTA time */
    int32_t rx_tti = (int32_t)XranGetTtiNum(xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);

    if(p_xran_lib_ctx->xran2phy_mem_ready == 0)
        return;

    if(p_xran_lib_ctx->rx_packet_callback_tracker[rx_tti % XRAN_N_FE_BUF_LEN][0] == 0){
        status = (rx_tti << 16) | 0; /* base on PHY side implementation first 7 sym of slot */
        if(p_xran_lib_ctx->pCallback[0])
           p_xran_lib_ctx->pCallback[0](p_xran_lib_ctx->pCallbackTag[0], status);
    } else {
        p_xran_lib_ctx->rx_packet_callback_tracker[rx_tti % XRAN_N_FE_BUF_LEN][0] = 0;
    }
    MLogTask(PID_UP_UL_HALF_DEAD_LINE_CB, t1, MLogTick());
}

void rx_ul_deadline_full_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogTick();
    struct xran_lib_ctx * p_xran_lib_ctx = xran_lib_get_ctx();
    XranStatusInt32 status;
    int32_t rx_tti = (int32_t)XranGetTtiNum(xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);

    if(rx_tti >= 8000-1)
       rx_tti = 0;
    else
       rx_tti -= 1; /* end of RX for prev TTI as measured against current OTA time */

    if(p_xran_lib_ctx->xran2phy_mem_ready == 0)
        return;

    if(p_xran_lib_ctx->rx_packet_callback_tracker[rx_tti % XRAN_N_FE_BUF_LEN][0] == 0){
        status = (rx_tti << 16) | 7; /* last 7 sym means full slot of Symb */
        if(p_xran_lib_ctx->pCallback[0])
           p_xran_lib_ctx->pCallback[0](p_xran_lib_ctx->pCallbackTag[0], status);
    } else {
        p_xran_lib_ctx->rx_packet_callback_tracker[rx_tti % XRAN_N_FE_BUF_LEN][0] = 0;
    }

    MLogTask(PID_UP_UL_FULL_DEAD_LINE_CB, t1, MLogTick());
}


void tx_cp_ul_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogTick();
    int sym, tti;
    uint32_t    frame_id    = 0;
    uint32_t    subframe_id = 0;
    uint32_t    slot_id     = 0;

    int32_t cc_id;
    int ant_id, prach_port_id;
    uint16_t beam_id;
    uint8_t num_eAxc, num_CCPorts;

    void *pHandle;

    struct xran_lib_ctx * p_xran_lib_ctx = xran_lib_get_ctx();
    xRANPrachCPConfigStruct *pPrachCPConfig = &(p_xran_lib_ctx->PrachCPConfig);
    struct xran_timer_ctx *pTCtx = (struct xran_timer_ctx *)arg;

    pHandle     = NULL;     // TODO: temp implemantation
    num_eAxc    = xran_get_num_eAxc(pHandle);
    num_CCPorts = xran_get_num_cc(pHandle);

    if (p_xran_lib_ctx->enableCP){
        tti = pTCtx[(xran_lib_ota_tti & 1) ^ 1].tti_to_process;
        slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME);
        subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME,  SUBFRAMES_PER_SYSTEMFRAME);
        frame_id    = XranGetFrameNum(tti,SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME);
        print_dbg("[%d]SFN %d sf %d slot %d\n", tti, frame_id, subframe_id, slot_id);


        for(ant_id = 0; ant_id < num_eAxc; ++ant_id) {
            for(cc_id = 0; cc_id < num_CCPorts; cc_id++ ) {
                // start new section information list
                xran_cp_reset_section_info(pHandle, XRAN_DIR_UL, cc_id, ant_id);

                beam_id = xran_get_beamid(pHandle, XRAN_DIR_UL, cc_id, ant_id, slot_id);
                send_cpmsg_dlul(pHandle, XRAN_DIR_UL,
                                frame_id, subframe_id, slot_id,
                                0, N_SYM_PER_SLOT, NUM_OF_PRB_IN_FULL_BAND,
                                beam_id, cc_id, ant_id,
                                xran_get_seqid(pHandle, XRAN_DIR_UL, cc_id, ant_id, slot_id));
            }
        }

        if ((frame_id % pPrachCPConfig->x == pPrachCPConfig->y[0]) && (pPrachCPConfig->isPRACHslot[slot_id]==1))  //is prach slot
        {
            for(ant_id = 0; ant_id < num_eAxc; ant_id++) {
                for(cc_id = 0; cc_id < num_CCPorts; cc_id++) {
#if !defined(PRACH_USES_SHARED_PORT)
                    prach_port_id = ant_id + num_eAxc;
                    // start new section information list
                    xran_cp_reset_section_info(pHandle, XRAN_DIR_UL, cc_id, prach_port_id);
#else
                    prach_port_id = ant_id;
#endif
                    beam_id = xran_get_beamid(pHandle, XRAN_DIR_UL, cc_id, prach_port_id, slot_id);
                    send_cpmsg_prach(pHandle,
                                    frame_id, subframe_id, slot_id,
                                    beam_id, cc_id, prach_port_id,
                                    xran_get_seqid(pHandle, XRAN_DIR_UL, cc_id, prach_port_id, slot_id));
                }
            }
        }

    }
    MLogTask(PID_CP_UL_CB, t1, MLogTick());
}

void ul_up_full_slot_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogTick();
    rte_pause();
    MLogTask(PID_TTI_CB_TO_PHY, t1, MLogTick());
}

void tti_to_phy_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogTick();
    struct xran_lib_ctx * p_xran_lib_ctx = xran_lib_get_ctx();

    static int first_call = 0;
    p_xran_lib_ctx->phy_tti_cb_done = 1; /* DPDK called CB */
    if (first_call){
        if(p_xran_lib_ctx->ttiCb[XRAN_CB_TTI]){
            if(p_xran_lib_ctx->SkipTti[XRAN_CB_TTI] <= 0){
                p_xran_lib_ctx->ttiCb[XRAN_CB_TTI](p_xran_lib_ctx->TtiCbParam[XRAN_CB_TTI]);
            }else{
                p_xran_lib_ctx->SkipTti[XRAN_CB_TTI]--;
            }
        }
    } else {
        if(p_xran_lib_ctx->ttiCb[XRAN_CB_TTI]){
            int32_t tti = (int32_t)XranGetTtiNum(xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
            if(tti == 8000-1)
                first_call = 1;
        }
    }


    MLogTask(PID_TTI_CB_TO_PHY, t1, MLogTick());
}

int xran_timing_source_thread(void *args)
{
    cpu_set_t cpuset;
    int32_t   do_reset = 0;
    uint64_t  t1 = 0;
    uint64_t  delta;
    int32_t   result1;
    uint32_t delay_cp_dl;
    uint32_t delay_cp_ul;
    uint32_t delay_up;
    uint32_t delay_up_ul;
    uint32_t delay_cp2up;
    uint32_t sym_cp_dl;
    uint32_t sym_cp_ul;
    uint32_t sym_up_ul;
    int32_t sym_up;
    struct sched_param sched_param;
    struct xran_lib_ctx * p_xran_lib_ctx = xran_lib_get_ctx();

    /* ToS = Top of Second start +- 1.5us */
    struct timespec ts;

    char buff[100];

    printf("%s [CPU %2d] [PID: %6d]\n", __FUNCTION__,  rte_lcore_id(), getpid());

    /* set main thread affinity mask to CPU2 */
    sched_param.sched_priority = 98;

    CPU_ZERO(&cpuset);
    CPU_SET(p_xran_lib_ctx->xran_init_cfg.io_cfg.timing_core, &cpuset);
    if (result1 = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset))
    {
        printf("pthread_setaffinity_np failed: coreId = 2, result1 = %d\n",result1);
    }
    if ((result1 = pthread_setschedparam(pthread_self(), 1, &sched_param)))
    {
        printf("priority is not changed: coreId = 2, result1 = %d\n",result1);
    }

    if (p_xran_lib_ctx->xran_init_cfg.io_cfg.id == APP_LLS_CU) {
        do {
           timespec_get(&ts, TIME_UTC);
        }while (ts.tv_nsec >1500);
        struct tm * ptm = gmtime(&ts.tv_sec);
        if(ptm){
            strftime(buff, sizeof buff, "%D %T", ptm);
            printf("lls-CU: thread_run start time: %s.%09ld UTC [%ld]\n", buff, ts.tv_nsec, interval_us);
        }

        delay_cp_dl = p_xran_lib_ctx->xran_init_cfg.ttiPeriod - p_xran_lib_ctx->xran_init_cfg.T1a_max_cp_dl;
        delay_cp_ul = p_xran_lib_ctx->xran_init_cfg.ttiPeriod - p_xran_lib_ctx->xran_init_cfg.T1a_max_cp_ul;
        delay_up    = p_xran_lib_ctx->xran_init_cfg.T1a_max_up;
        delay_up_ul = p_xran_lib_ctx->xran_init_cfg.Ta4_max;

        delay_cp2up = delay_up-delay_cp_dl;

        sym_cp_dl = delay_cp_dl*1000/(interval_us*1000/N_SYM_PER_SLOT)+1;
        sym_cp_ul = delay_cp_ul*1000/(interval_us*1000/N_SYM_PER_SLOT)+1;
        sym_up_ul = delay_up_ul*1000/(interval_us*1000/N_SYM_PER_SLOT);
        p_xran_lib_ctx->sym_up = sym_up = -(delay_up*1000/(interval_us*1000/N_SYM_PER_SLOT)+1);
        p_xran_lib_ctx->sym_up_ul = sym_up_ul = (delay_up_ul*1000/(interval_us*1000/N_SYM_PER_SLOT)+1);

        printf("Start C-plane DL %d us after TTI  [trigger on sym %d]\n", delay_cp_dl, sym_cp_dl);
        printf("Start C-plane UL %d us after TTI  [trigger on sym %d]\n", delay_cp_ul, sym_cp_ul);
        printf("Start U-plane DL %d us before OTA [offset  in sym %d]\n", delay_up, sym_up);
        printf("Start U-plane UL %d us OTA        [offset  in sym %d]\n", delay_up_ul, sym_up_ul);

        printf("C-plane to U-plane delay %d us after TTI\n", delay_cp2up);
        printf("Start Sym timer %ld ns\n", TX_TIMER_INTERVAL/N_SYM_PER_SLOT);

        p_xran_lib_ctx->pSymCallback[0][sym_cp_dl]    = xran_timer_arm;
        p_xran_lib_ctx->pSymCallbackTag[0][sym_cp_dl] = tx_cp_dl_cb;

        p_xran_lib_ctx->pSymCallback[0][sym_cp_ul]    = xran_timer_arm;
        p_xran_lib_ctx->pSymCallbackTag[0][sym_cp_ul] = tx_cp_ul_cb;

        /* Full slot UL OTA + delay_up_ul */
        p_xran_lib_ctx->pSymCallback[0][sym_up_ul]    = xran_timer_arm;
        p_xran_lib_ctx->pSymCallbackTag[0][sym_up_ul] = rx_ul_deadline_full_cb;

        /* Half slot UL OTA + delay_up_ul*/
        p_xran_lib_ctx->pSymCallback[0][sym_up_ul + N_SYM_PER_SLOT/2]    = xran_timer_arm;
        p_xran_lib_ctx->pSymCallbackTag[0][sym_up_ul + N_SYM_PER_SLOT/2] = rx_ul_deadline_half_cb;

    } else {    // APP_RU
        /* calcualte when to send UL U-plane */
        delay_up = p_xran_lib_ctx->xran_init_cfg.Ta3_min;
        p_xran_lib_ctx->sym_up = sym_up = delay_up*1000/(interval_us*1000/N_SYM_PER_SLOT)+1;
        printf("Start UL U-plane %d us after OTA [offset in sym %d]\n", delay_up, sym_up);
        do {
           timespec_get(&ts, TIME_UTC);
        }while (ts.tv_nsec >1500);
        struct tm * ptm = gmtime(&ts.tv_sec);
        if(ptm){
            strftime(buff, sizeof buff, "%D %T", ptm);
            printf("RU: thread_run start time: %s.%09ld UTC [%ld]\n", buff, ts.tv_nsec, interval_us);
        }
    }

    do {
       timespec_get(&ts, TIME_UTC);
    }while (ts.tv_nsec == 0);

    while(1) {
        delta = poll_next_tick(interval_us*1000L/N_SYM_PER_SLOT);
        if (XRAN_STOPPED == xran_if_current_state)
            break;
        sym_ota_cb(&sym_timer, timer_ctx);
    }
    printf("Closing timing source thread...\n");

    return 0;
}

/* Handle ecpri format. */
int handle_ecpri_ethertype(struct rte_mbuf *pkt, uint64_t rx_time)
{
    const struct xran_ecpri_hdr *ecpri_hdr;
    unsigned long t1;

    if (rte_pktmbuf_data_len(pkt) < sizeof(struct xran_ecpri_hdr)) {
        wlog("Packet too short - %d bytes", rte_pktmbuf_data_len(pkt));
        return 0;
    }

    /* check eCPRI header. */
    ecpri_hdr = rte_pktmbuf_mtod(pkt, struct xran_ecpri_hdr *);
    if(ecpri_hdr == NULL)
        return MBUF_FREE;

    switch(ecpri_hdr->ecpri_mesg_type) {
        case ECPRI_IQ_DATA:
            t1 = MLogTick();
            process_mbuf(pkt);
            MLogTask(PID_PROCESS_UP_PKT, t1, MLogTick());
            break;
        // For RU emulation
        case ECPRI_RT_CONTROL_DATA:
            t1 = MLogTick();
            if(xran_lib_get_ctx()->xran_init_cfg.io_cfg.id == APP_RU) {
                process_cplane(pkt);
            } else {
                print_err("LLS-CU recevied CP message!");
            }
            MLogTask(PID_PROCESS_CP_PKT, t1, MLogTick());
            break;
        default:
            wlog("Invalid eCPRI message type - %d", ecpri_hdr->ecpri_mesg_type);
        }
#if 0
//def DEBUG
    return MBUF_KEEP;
#else
    return MBUF_FREE;
#endif
}

int xran_process_rx_sym(void *arg,
                        void *iq_data_start,
                        uint16_t size,
                        uint8_t CC_ID,
                        uint8_t Ant_ID,
                        uint8_t frame_id,
                        uint8_t subframe_id,
                        uint8_t slot_id,
                        uint8_t symb_id)
{
    char        *pos = NULL;
    struct xran_lib_ctx * p_xran_lib_ctx = xran_lib_get_ctx();
    uint32_t tti=0;
    XranStatusInt32 status;
    void *pHandle = NULL;

    if(p_xran_lib_ctx->xran2phy_mem_ready == 0)
        return 0;

    tti = frame_id * SLOTS_PER_SYSTEMFRAME + subframe_id * SLOTNUM_PER_SUBFRAME + slot_id;

    status = tti << 16 | symb_id;

    if(tti < 8000 && CC_ID < XRAN_MAX_SECTOR_NR &&  CC_ID == 0 && Ant_ID < XRAN_MAX_ANTENNA_NR && symb_id < XRAN_NUM_OF_SYMBOL_PER_SLOT){
        pos = (char*) p_xran_lib_ctx->sFrontHaulRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id].pData;
        if(pos && iq_data_start && size){
#ifdef XRAN_BYTE_ORDER_SWAP
            int idx = 0;
            uint16_t *restrict psrc = (uint16_t *)iq_data_start;
            uint16_t *restrict pdst = (uint16_t *)pos;
            /* network byte (be) order of IQ to CPU byte order (le) */
            for (idx = 0; idx < size/sizeof(int16_t); idx++){
                pdst[idx]  = (psrc[idx]>>8) | (psrc[idx]<<8); //rte_be_to_cpu_16(psrc[idx]);
            }
#else
#error xran spec is network byte order
            /* for debug */
            rte_memcpy(pdst, psrc, size);
#endif
#ifdef DEBUG_XRAN_BUFFERS
            if (pos[0] != tti % XRAN_N_FE_BUF_LEN ||
                pos[1] != CC_ID  ||
                pos[2] != Ant_ID ||
                pos[3] != symb_id){
                    printf("%d %d %d %d\n", pos[0], pos[1], pos[2], pos[3]);
            }
#endif
        } else {
            print_err("pos %p iq_data_start %p size %d\n",pos, iq_data_start, size);
        }
    } else {
        print_err("TTI %d(f_%d sf_%d slot_%d) CC %d Ant_ID %d symb_id %d\n",tti, frame_id, subframe_id, slot_id, CC_ID, Ant_ID, symb_id);
    }
    if (symb_id == 7 || symb_id == 13){
        p_xran_lib_ctx->rx_packet_symb_tracker[tti % XRAN_N_FE_BUF_LEN][CC_ID][symb_id]++;

        if(p_xran_lib_ctx->rx_packet_symb_tracker[tti % XRAN_N_FE_BUF_LEN][CC_ID][symb_id] >= xran_get_num_eAxc(pHandle)){
            if(p_xran_lib_ctx->pCallback[0])
               p_xran_lib_ctx->pCallback[0](p_xran_lib_ctx->pCallbackTag[0], status);
            p_xran_lib_ctx->rx_packet_callback_tracker[tti % XRAN_N_FE_BUF_LEN][CC_ID] = 1;
            p_xran_lib_ctx->rx_packet_symb_tracker[tti % XRAN_N_FE_BUF_LEN][CC_ID][symb_id] = 0;
        }
    }
    return size;
}


int xran_process_tx_sym(void *arg)
{
    uint32_t    tti=0;
    uint32_t    mlogVar[10];
    uint32_t    mlogVarCnt = 0;
    unsigned long t1 = MLogTick();

    void        *pHandle = NULL;
    int32_t     ant_id;
    int32_t     cc_id = 0;
    uint8_t     num_eAxc = 0;
    uint8_t     num_CCPorts = 0;

    uint32_t    frame_id    = 0;
    uint32_t    subframe_id = 0;
    uint32_t    slot_id     = 0;
    uint32_t    sym_id      = 0;
    uint32_t    sym_idx     = 0;

    char        *pos = NULL;
    int         prb_num = 0;

    struct xran_section_info *sectinfo;
    uint32_t next;

    enum xran_pkt_dir  direction;

    struct xran_lib_ctx * p_xran_lib_ctx = xran_lib_get_ctx();
    struct xran_timer_ctx *pTCtx = (struct xran_timer_ctx *)arg;


    if(p_xran_lib_ctx->xran2phy_mem_ready == 0)
        return 0;

    if(p_xran_lib_ctx->xran_init_cfg.io_cfg.id == APP_LLS_CU) {
        direction = XRAN_DIR_DL; /* lls-CU */
        prb_num = NUM_OF_PRB_IN_FULL_BAND;
        }
    else {
        direction = XRAN_DIR_UL; /* RU */
        prb_num = NUM_OF_PRB_IN_FULL_BAND; /*TODO: simulation on D-1541 @ 2.10GHz has issue with performace. reduce copy size */
        }

    /* RU: send symb after OTA time with delay (UL) */
    /* lls-CU:send symb in advance of OTA time (DL) */
    sym_idx     = XranOffsetSym(p_xran_lib_ctx->sym_up, xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT*SLOTNUM_PER_SUBFRAME*1000);

    tti         = XranGetTtiNum(sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
    slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME);
    subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME,  SUBFRAMES_PER_SYSTEMFRAME);
    frame_id    = XranGetFrameNum(tti,SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME);
    sym_id      = XranGetSymNum(sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);

    print_dbg("[%d]SFN %d sf %d slot %d\n", tti, frame_id, subframe_id, slot_id);

    mlogVar[mlogVarCnt++] = 0xAAAAAAAA;
    mlogVar[mlogVarCnt++] = xran_lib_ota_sym_idx;
    mlogVar[mlogVarCnt++] = sym_idx;
    mlogVar[mlogVarCnt++] = abs(p_xran_lib_ctx->sym_up);
    mlogVar[mlogVarCnt++] = tti;
    mlogVar[mlogVarCnt++] = frame_id;
    mlogVar[mlogVarCnt++] = subframe_id;
    mlogVar[mlogVarCnt++] = slot_id;
    mlogVar[mlogVarCnt++] = sym_id;
    MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());

    if(frame_id > 99) {
        print_err("OTA %d: TX:[sym_idx %d: TTI %d] fr %d sf %d slot %d sym %d\n",xran_lib_ota_sym_idx,  sym_idx, tti, frame_id, subframe_id, slot_id, sym_id);
        xran_if_current_state =XRAN_STOPPED;
        }

    num_eAxc    = xran_get_num_eAxc(pHandle);
    num_CCPorts = xran_get_num_cc(pHandle);

    /* U-Plane */
    for(ant_id = 0; ant_id < num_eAxc; ant_id++) {
        for(cc_id = 0; cc_id < num_CCPorts; cc_id++) {
            if(p_xran_lib_ctx->xran_init_cfg.io_cfg.id == APP_LLS_CU && p_xran_lib_ctx->enableCP) {
                next = 0;
                while(next < xran_cp_getsize_section_info(pHandle, direction, cc_id, ant_id)) {
                    sectinfo = xran_cp_iterate_section_info(pHandle, direction,
                                        cc_id, ant_id, subframe_id, slot_id, &next);
                    if(sectinfo == NULL)
                        break;

                    /* pointer to IQs input */
                    /* TODO: need to implement the case of partial RB assignment */
                    pos = (char*) p_xran_lib_ctx->sFrontHaulTxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id].pData;
                    print_dbg(">>> [%d] type%d, id %d, startPrbc=%d, numPrbc=%d, numSymbol=%d\n", next,
                            sectinfo->type, sectinfo->id, sectinfo->startPrbc,
                            sectinfo->numPrbc, sectinfo->numSymbol);

                    if(sectinfo->type != XRAN_CP_SECTIONTYPE_1) {
                        print_err("Invalid section type in section DB - %d", sectinfo->type);
                        continue;
                        }

                    send_symbol_ex(direction, sectinfo->id,
                                    (struct rb_map *)pos,
                                    frame_id, subframe_id, slot_id, sym_id,
                                    sectinfo->startPrbc, sectinfo->numPrbc,
                                    cc_id, ant_id,
                                    xran_get_seqid(pHandle, direction, cc_id, ant_id, slot_id));
                    }
                }

            else {  /* if(p_xran_lib_ctx->xran_init_cfg.io_cfg.id == APP_LLS_CU && p_xran_lib_ctx->enableCP) */
                /* pointer to IQs input */
                pos = (char*) p_xran_lib_ctx->sFrontHaulTxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id].pData;
#ifdef DEBUG_XRAN_BUFFERS
                if (pos[0] != tti % XRAN_N_FE_BUF_LEN ||
                    pos[1] != cc_id ||
                    pos[2] != ant_id ||
                    pos[3] != sym_id)
                        printf("%d %d %d %d\n", pos[0], pos[1], pos[2], pos[3]);
#endif
                send_symbol_ex(direction,
                                xran_alloc_sectionid(pHandle, direction, cc_id, ant_id, slot_id),
                                (struct rb_map *)pos,
                                frame_id, subframe_id, slot_id, sym_id,
                                0, prb_num,
                                cc_id, ant_id,
                                xran_get_seqid(pHandle, direction, cc_id, ant_id, slot_id));
                }
            }
        }

    MLogTask(PID_PROCESS_TX_SYM, t1, MLogTick());
    return 0;
}

int xran_packet_and_dpdk_timer_thread(void *args)
{
    struct xran_ethdi_ctx *const ctx = xran_ethdi_get_ctx();

    uint64_t prev_tsc = 0;
    uint64_t cur_tsc = rte_rdtsc();
    uint64_t diff_tsc = cur_tsc - prev_tsc;
    cpu_set_t cpuset;
    struct sched_param sched_param;
    int res = 0;
    printf("%s [CPU %2d] [PID: %6d]\n", __FUNCTION__,  rte_lcore_id(), getpid());

    sched_param.sched_priority = XRAN_THREAD_DEFAULT_PRIO;

    if ((res  = pthread_setschedparam(pthread_self(), 1, &sched_param)))
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


int32_t xran_init(int argc, char *argv[], PXRANFHINIT p_xran_fh_init, char *appName, void ** pHandle)
{
    int i;
    int j;

    struct xran_io_loop_cfg *p_io_cfg = (struct xran_io_loop_cfg *)&p_xran_fh_init->io_cfg;
    struct xran_lib_ctx * p_xran_lib_ctx = xran_lib_get_ctx();

    int  lcore_id = 0;
    char filename[64];

    memset(p_xran_lib_ctx, 0, sizeof(struct xran_lib_ctx));
    /* copy init */
    p_xran_lib_ctx->xran_init_cfg = *p_xran_fh_init;

    xran_if_current_state = XRAN_RUNNING;
    interval_us =  p_xran_fh_init->ttiPeriod;

    p_xran_lib_ctx->llscu_id = p_xran_fh_init->llscuId;
    memcpy(&(p_xran_lib_ctx->eAxc_id_cfg), &(p_xran_fh_init->eAxCId_conf), sizeof(XRANEAXCIDCONFIG));

    p_xran_lib_ctx->enableCP = p_xran_fh_init->enableCP;

    xran_register_ethertype_handler(ETHER_TYPE_ECPRI, handle_ecpri_ethertype);
    if (p_io_cfg->id == 0)
        xran_ethdi_init_dpdk_io(basename(appName),
                           p_io_cfg,
                           &lcore_id,
                           (struct ether_addr *)p_xran_fh_init->p_lls_cu_addr,
                           (struct ether_addr *)p_xran_fh_init->p_ru_addr,
                           p_xran_fh_init->cp_vlan_tag,
                           p_xran_fh_init->up_vlan_tag);
    else
        xran_ethdi_init_dpdk_io(basename(appName),
                           p_io_cfg,
                           &lcore_id,
                           (struct ether_addr *)p_xran_fh_init->p_ru_addr,
                           (struct ether_addr *)p_xran_fh_init->p_lls_cu_addr,
                           p_xran_fh_init->cp_vlan_tag,
                           p_xran_fh_init->up_vlan_tag);

    for(i = 0; i < 10; i++ )
        rte_timer_init(&tti_to_phy_timer[i]);

    rte_timer_init(&tti_timer);
    rte_timer_init(&sym_timer);
    rte_timer_init(&tx_cp_dl_timer);
    rte_timer_init(&tx_cp_ul_timer);
    rte_timer_init(&tx_up_timer);

    for(i = 0; i < XRAN_MAX_SECTOR_NR;  i++ ){
        unsigned n = snprintf(&p_xran_lib_ctx->ring_name[0][i][0], RTE_RING_NAMESIZE, "dl_sym_ring_%u", i);
        p_xran_lib_ctx->dl_sym_idx_ring[i]   = rte_ring_create(&p_xran_lib_ctx->ring_name[0][i][0], XRAN_RING_SIZE,
                                               rte_lcore_to_socket_id(lcore_id), RING_F_SP_ENQ | RING_F_SC_DEQ);
    }


    lcore_id = rte_get_next_lcore(lcore_id, 0, 0);
    PANIC_ON(lcore_id == RTE_MAX_LCORE, "out of lcores for io_loop()");

    /* Start packet processing thread */
    if (rte_eal_remote_launch(ring_processing_thread, NULL, lcore_id))
        rte_panic("ring_processing_thread() failed to start\n");

    if(p_io_cfg->pkt_aux_core > 0){
        lcore_id = rte_get_next_lcore(lcore_id, 0, 0);
        PANIC_ON(lcore_id == RTE_MAX_LCORE, "out of lcores for io_loop()");

        /* Start packet processing thread */
        if (rte_eal_remote_launch(xran_packet_and_dpdk_timer_thread, NULL, lcore_id))
            rte_panic("ring_processing_thread() failed to start\n");
    }

    lcore_id = rte_get_next_lcore(lcore_id, 0, 0);
    PANIC_ON(lcore_id == RTE_MAX_LCORE, "out of lcores for io_loop()");

    /* Start packet processing thread */
    if (rte_eal_remote_launch(xran_timing_source_thread, xran_lib_get_ctx(), lcore_id))
        rte_panic("thread_run() failed to start\n");

    printf("Set debug stop %d\n", p_xran_fh_init->debugStop);
    timing_set_debug_stop(p_xran_fh_init->debugStop);

    memset(&DevHandle, 0, sizeof(XranLibHandleInfoStruct));

    *pHandle = &DevHandle;

    return 0;
}

int32_t xran_sector_get_instances (void * pHandle, uint16_t nNumInstances,
               XranCcInstanceHandleVoidP * pSectorInstanceHandles)
{
    int i;

    /* only one handle as only one CC is currently supported */
    for(i = 0; i < nNumInstances; i++ )
        pSectorInstanceHandles[i] = pHandle;

    return 0;
}

int32_t xran_mm_init (void * pHandle, uint64_t nMemorySize,
            uint32_t nMemorySegmentSize)
{
    /* we use mbuf from dpdk memory */
    return 0;
}

int32_t xran_bm_init (void * pHandle, uint32_t * pPoolIndex, uint32_t nNumberOfBuffers, uint32_t nBufferSize)
{
    XranLibHandleInfoStruct* pXran = (XranLibHandleInfoStruct*) pHandle;

    char pool_name[RTE_MEMPOOL_NAMESIZE];

    snprintf(pool_name, RTE_MEMPOOL_NAMESIZE, "bm_mempool_%ld", pPoolIndex);

    pXran->p_bufferPool[pXran->nBufferPoolIndex] = rte_pktmbuf_pool_create(pool_name, nNumberOfBuffers,
           MBUF_CACHE, 0, XRAN_MAX_MBUF_LEN, rte_socket_id());

    pXran->bufferPoolElmSz[pXran->nBufferPoolIndex]  = nBufferSize;
    pXran->bufferPoolNumElm[pXran->nBufferPoolIndex] = nNumberOfBuffers;

    print_dbg("[nPoolIndex %d] mb pool %p \n", pXran->nBufferPoolIndex,  pXran->p_bufferPool[pXran->nBufferPoolIndex]);

    *pPoolIndex = pXran->nBufferPoolIndex++;

    return 0;
}

int32_t xran_bm_allocate_buffer(void * pHandle, uint32_t nPoolIndex, void **ppVirtAddr)
{
    XranLibHandleInfoStruct* pXran = (XranLibHandleInfoStruct*) pHandle;
    *ppVirtAddr = NULL;

    struct rte_mbuf * mb =  rte_pktmbuf_alloc(pXran->p_bufferPool[nPoolIndex]);

    if(mb){
        *ppVirtAddr = rte_pktmbuf_append(mb, pXran->bufferPoolElmSz[nPoolIndex]);

    }else {
        print_err("[nPoolIndex %d] mb alloc failed \n", nPoolIndex );
        return -1;
    }

    if (*ppVirtAddr ==  NULL){
        print_err("[nPoolIndex %d] rte_pktmbuf_append for %d failed \n", nPoolIndex, pXran->bufferPoolElmSz[nPoolIndex]);
        return -1;
    }

    return 0;
}

int32_t xran_bm_free_buffer(void * pHandle, void *pVirtAddr)
{
    XranLibHandleInfoStruct* pXran = (XranLibHandleInfoStruct*) pHandle;
    rte_pktmbuf_free(pVirtAddr);

    return 0;
}

int32_t xran_5g_fronthault_config (void * pHandle,
                XRANBufferListStruct *pSrcBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                XRANBufferListStruct *pDstBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                XranTransportBlockCallbackFn pCallback,
                void *pCallbackTag)
{
    XranLibHandleInfoStruct *pInfo = (XranLibHandleInfoStruct *) pHandle;
    XranStatusInt32 nStatus = XRAN_STATUS_SUCCESS;
    int j, i = 0, z, k;
    struct xran_lib_ctx * p_xran_lib_ctx = xran_lib_get_ctx();

    print_dbg("%s\n", __FUNCTION__);

    if(NULL == pHandle)
    {
        printf("Handle is NULL!\n");
        return XRAN_STATUS_FAIL;
    }
    if (pCallback == NULL)
    {
        printf ("no callback\n");
        return XRAN_STATUS_FAIL;
    }

    for(j=0; j<XRAN_N_FE_BUF_LEN; j++)
    {
        for(z = 0; z < XRAN_MAX_ANTENNA_NR; z++){
           p_xran_lib_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].bValid = 0;
           p_xran_lib_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
           p_xran_lib_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
           p_xran_lib_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
           p_xran_lib_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
           p_xran_lib_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &p_xran_lib_ctx->sFrontHaulTxBuffers[j][i][z][0];

           p_xran_lib_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList =   *pSrcBuffer[z][j];

           p_xran_lib_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].bValid = 0;
           p_xran_lib_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
           p_xran_lib_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
           p_xran_lib_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
           p_xran_lib_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
           p_xran_lib_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &p_xran_lib_ctx->sFrontHaulRxBuffers[j][i][z][0];
           p_xran_lib_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList =   *pDstBuffer[z][j];
        }
    }

#if 0
    for(j=0; j<XRAN_N_FE_BUF_LEN; j++)
        for(z = 0; z < XRAN_MAX_ANTENNA_NR; z++){
            printf("TTI:TX 0x%02x Sec %d Ant%d\n",j,i,z);
            for(k = 0; k <XRAN_NUM_OF_SYMBOL_PER_SLOT; k++){
                uint8_t *ptr = p_xran_lib_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pData;
                printf("    sym: %2d %p 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", k, ptr, ptr[0],ptr[1], ptr[2], ptr[3], ptr[4]);
            }
        }

    for(j=0; j<XRAN_N_FE_BUF_LEN; j++)
        for(z = 0; z < XRAN_MAX_ANTENNA_NR; z++){
            printf("TTI:RX 0x%02x Sec %d Ant%d\n",j,i,z);
            for(k = 0; k <XRAN_NUM_OF_SYMBOL_PER_SLOT; k++){
                uint8_t *ptr = p_xran_lib_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pData;
                printf("    sym: %2d %p 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", k, ptr, ptr[0],ptr[1], ptr[2], ptr[3], ptr[4]);
            }
        }
#endif

    p_xran_lib_ctx->pCallback[i]    = pCallback;
    p_xran_lib_ctx->pCallbackTag[i] = pCallbackTag;

    p_xran_lib_ctx->xran2phy_mem_ready = 1;

    return nStatus;
}

int32_t xran_5g_prach_req (void *  pHandle,
                XRANBufferListStruct *pDstBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                XranTransportBlockCallbackFn pCallback,
                void *pCallbackTag)
{
    XranLibHandleInfoStruct *pInfo = (XranLibHandleInfoStruct *) pHandle;
    XranStatusInt32 nStatus = XRAN_STATUS_SUCCESS;
    int j, i = 0, z;
    struct xran_lib_ctx * p_xran_lib_ctx = xran_lib_get_ctx();

    if(NULL == pHandle)
    {
        printf("Handle is NULL!\n");
        return XRAN_STATUS_FAIL;
    }
    if (pCallback == NULL)
    {
        printf ("no callback\n");
        return XRAN_STATUS_FAIL;
    }

    for(j=0; j<XRAN_N_FE_BUF_LEN; j++)
    {
        for(z = 0; z < XRAN_MAX_ANTENNA_NR; z++){
           p_xran_lib_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].bValid = 0;
           p_xran_lib_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
           p_xran_lib_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
           p_xran_lib_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
           p_xran_lib_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_MAX_ANTENNA_NR; // ant number.
           p_xran_lib_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &p_xran_lib_ctx->sFHPrachRxBuffers[j][i][z][0];
           p_xran_lib_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList =   *pDstBuffer[z][j];
        }
    }

    p_xran_lib_ctx->pPrachCallback[i]    = pCallback;
    p_xran_lib_ctx->pPrachCallbackTag[i] = pCallbackTag;

    return 0;
}

int32_t xran_5g_pre_compenstor_cfg(void* pHandle,
                uint32_t nTxPhaseCps,
                uint32_t nRxPhaseCps,
                uint8_t nSectorId)
{
    /* functionality is not yet implemented */
    return 0;
}

int32_t xran_open(void *pHandle, PXRANFHCONFIG pConf)
{
    int i;
    uint8_t slotNr;
    XRANFHCONFIG *pFhCfg;
    xRANPrachCPConfigStruct *pPrachCPConfig = &(xran_lib_get_ctx()->PrachCPConfig);
    pFhCfg = &(xran_lib_get_ctx()->xran_fh_cfg);
    memcpy(pFhCfg, pConf, sizeof(XRANFHCONFIG));
    PXRANPRACHCONFIG pPRACHConfig = &pFhCfg->prach_conf;
    uint8_t nPrachConfIdx = pPRACHConfig->nPrachConfIdx;
    const xRANPrachConfigTableStruct *pxRANPrachConfigTable = &gxranPrachDataTable_mmw[nPrachConfIdx];
    uint8_t preambleFmrt = pxRANPrachConfigTable->preambleFmrt[0];
    const xRANPrachPreambleLRAStruct *pxranPreambleforLRA = &gxranPreambleforLRA[preambleFmrt - FORMAT_A1];
    memset(pPrachCPConfig, 0, sizeof(xRANPrachCPConfigStruct));

    //setup PRACH configuration for C-Plane
    pPrachCPConfig->filterIdx = XRAN_FILTERINDEX_PRACH_ABC;         // 3, PRACH preamble format A1~3, B1~4, C0, C2
    pPrachCPConfig->startSymId = pxRANPrachConfigTable->startingSym;
    pPrachCPConfig->startPrbc = pPRACHConfig->nPrachFreqStart;
    pPrachCPConfig->numPrbc = (preambleFmrt >= FORMAT_A1)? 12 : 70;
    pPrachCPConfig->numSymbol = pxRANPrachConfigTable->duration;
    pPrachCPConfig->timeOffset = pxranPreambleforLRA->nRaCp;
    pPrachCPConfig->freqOffset = xran_get_freqoffset(pPRACHConfig->nPrachFreqOffset, pPRACHConfig->nPrachSubcSpacing);
    pPrachCPConfig->occassionsInPrachSlot = pxRANPrachConfigTable->occassionsInPrachSlot;
    pPrachCPConfig->x = pxRANPrachConfigTable->x;
    pPrachCPConfig->y[0] = pxRANPrachConfigTable->y[0];
    pPrachCPConfig->y[1] = pxRANPrachConfigTable->y[1];

    pPrachCPConfig->isPRACHslot[pxRANPrachConfigTable->slotNr[0]] = 1;
    for (i=1; i < XRAN_PRACH_CANDIDATE_SLOT; i++)
    {
        slotNr = pxRANPrachConfigTable->slotNr[i];
        if (slotNr > 0)
            pPrachCPConfig->isPRACHslot[slotNr] = 1;
    }

    xran_cp_init_sectiondb(pHandle);
    xran_init_sectionid(pHandle);
    xran_init_seqid(pHandle);

    return 0;
}

int32_t xran_start(void *pHandle)
{
    xran_if_current_state = XRAN_RUNNING;
    return 0;
}

int32_t xran_stop(void *pHandle)
{
    xran_if_current_state = XRAN_STOPPED;
    return 0;
}

int32_t xran_close(void *pHandle)
{
    xran_if_current_state = XRAN_STOPPED;
    xran_cp_free_sectiondb(pHandle);
    rte_eal_mp_wait_lcore();
    return 0;
}

int32_t xran_mm_destroy (void * pHandle)
{
    /* functionality is not yet implemented */
    return -1;
}

int32_t xran_reg_sym_cb(void *pHandle, XRANFHSYMPROCCB symCb, void * symCbParam, uint8_t symb,  uint8_t ant)
{
    /* functionality is not yet implemented */
    return -1;
}

int32_t xran_reg_physide_cb(void *pHandle, XRANFHTTIPROCCB Cb, void *cbParam, int skipTtiNum, enum callback_to_phy_id id)
{
    struct xran_lib_ctx * p_xran_lib_ctx = xran_lib_get_ctx();

    p_xran_lib_ctx->ttiCb[id]      = Cb;
    p_xran_lib_ctx->TtiCbParam[id] = cbParam;
    p_xran_lib_ctx->SkipTti[id]    = skipTtiNum;

    return 0;
}

int32_t xran_get_slot_idx (uint32_t *nFrameIdx, uint32_t *nSubframeIdx,  uint32_t *nSlotIdx, uint64_t *nSecond)
{
    int32_t tti = 0;

    tti           = (int32_t)XranGetTtiNum(xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
    *nSlotIdx     = (uint32_t)XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME);
    *nSubframeIdx = (uint32_t)XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME,  SUBFRAMES_PER_SYSTEMFRAME);
    *nFrameIdx    = (uint32_t)XranGetFrameNum(tti,SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME);
    *nSecond      = timing_get_current_second();

    return tti;
}

/**
 * @brief Get supported maximum number of sections
 *
 * @return maximum number of sections
 */
inline uint8_t xran_get_max_sections(void *pHandle)
{
    return (XRAN_MAX_NUM_SECTIONS);
}

/**
 * @brief Get the configuration of eAxC ID
 *
 * @return the pointer of configuration
 */
inline XRANEAXCIDCONFIG *xran_get_conf_eAxC(void *pHandle)
{
    return (&(xran_lib_get_ctx()->eAxc_id_cfg));
}

/**
 * @brief Get the configuration of subcarrier spacing for PRACH
 *
 * @return subcarrier spacing value for PRACH
 */
inline uint8_t xran_get_conf_prach_scs(void *pHandle)
{
    return (xran_lib_get_ctx_fhcfg()->prach_conf.nPrachSubcSpacing);
}

/**
 * @brief Get the configuration of FFT size for RU
 *
 * @return FFT size value for RU
 */
inline uint8_t xran_get_conf_fftsize(void *pHandle)
{
    return (xran_lib_get_ctx_fhcfg()->ru_conf.fftSize);
}

/**
 * @brief Get the configuration of nummerology
 *
 * @return subcarrier spacing value for PRACH
 */
inline uint8_t xran_get_conf_numerology(void *pHandle)
{
    return (xran_lib_get_ctx_fhcfg()->frame_conf.nNumerology);
}

/**
 * @brief Get the configuration of IQ bit width for RU
 *
 * @return IQ bit width for RU
 */
inline uint8_t xran_get_conf_iqwidth(void *pHandle)
{
  XRANFHCONFIG *pFhCfg;

    pFhCfg = xran_lib_get_ctx_fhcfg();
    return ((pFhCfg->ru_conf.iqWidth==16)?0:pFhCfg->ru_conf.iqWidth);
}

/**
 * @brief Get the configuration of compression method for RU
 *
 * @return Compression method for RU
 */
inline uint8_t xran_get_conf_compmethod(void *pHandle)
{
    return (xran_lib_get_ctx_fhcfg()->ru_conf.compMeth);
}

/**
 * @brief Get the configuration of lls-cu ID
 *
 * @return Configured lls-cu ID
 */
inline uint8_t xran_get_llscuid(void *pHandle)
{
    return (xran_lib_get_ctx()->llscu_id);
}

/**
 * @brief Get the configuration of lls-cu ID
 *
 * @return Configured lls-cu ID
 */
inline uint8_t xran_get_sectorid(void *pHandle)
{
    return (xran_lib_get_ctx()->sector_id);
}

/**
 * @brief Get the configuration of the number of component carriers
 *
 * @return Configured the number of componen carriers
 */
inline uint8_t xran_get_num_cc(void *pHandle)
{
    return (xran_lib_get_ctx_fhcfg()->nCC);
}

/**
 * @brief Get the configuration of the number of antenna
 *
 * @return Configured the number of antenna
 */
inline uint8_t xran_get_num_eAxc(void *pHandle)
{
    return (xran_lib_get_ctx_fhcfg()->neAxc);
}


