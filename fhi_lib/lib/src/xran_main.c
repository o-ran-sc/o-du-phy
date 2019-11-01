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
#include <malloc.h>

#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_mbuf.h>
#include <rte_ring.h>

#include "xran_fh_o_du.h"

#include "ethdi.h"
#include "xran_pkt.h"
#include "xran_up_api.h"
#include "xran_cp_api.h"
#include "xran_sync_api.h"
#include "xran_lib_mlog_tasks_id.h"
#include "xran_timer.h"
#include "xran_common.h"
#include "xran_frame_struct.h"
#include "xran_printf.h"
#include "xran_app_frag.h"

#include "xran_mlog_lnx.h"

#define DIV_ROUND_OFFSET(X,Y) ( X/Y + ((X%Y)?1:0) )

#define XranOffsetSym(offSym, otaSym, numSymTotal)  (((int32_t)offSym > (int32_t)otaSym) ? \
                            ((int32_t)otaSym + ((int32_t)numSymTotal) - (uint32_t)offSym) : \
                            (((int32_t)otaSym - (int32_t)offSym) >= numSymTotal) ?  \
                                    (((int32_t)otaSym - (int32_t)offSym) - numSymTotal) : \
                                    ((int32_t)otaSym - (int32_t)offSym))

#define MAX_NUM_OF_XRAN_CTX          (2)
#define XranIncrementCtx(ctx)                             ((ctx >= (MAX_NUM_OF_XRAN_CTX-1)) ? 0 : (ctx+1))
#define XranDecrementCtx(ctx)                             ((ctx == 0) ? (MAX_NUM_OF_XRAN_CTX-1) : (ctx-1))

#define MAX_NUM_OF_DPDK_TIMERS       (10)
#define DpdkTiemerIncrementCtx(ctx)          ((ctx >= (MAX_NUM_OF_DPDK_TIMERS-1)) ? 0 : (ctx+1))
#define DpdkTiemerDecrementCtx(ctx)          ((ctx == 0) ? (MAX_NUM_OF_DPDK_TIMERS-1) : (ctx-1))


//#define XRAN_CREATE_RBMAP /**< generate slot map base on symbols */


struct xran_timer_ctx {
    uint32_t    tti_to_process;
};

static xran_cc_handle_t pLibInstanceHandles[XRAN_PORTS_NUM][XRAN_MAX_SECTOR_NR] = {NULL};
static struct xran_device_ctx g_xran_dev_ctx[XRAN_PORTS_NUM] = { 0 };

struct xran_timer_ctx timer_ctx[MAX_NUM_OF_XRAN_CTX];

static struct rte_timer tti_to_phy_timer[10];
static struct rte_timer sym_timer;
static struct rte_timer dpdk_timer[MAX_NUM_OF_DPDK_TIMERS];

long interval_us = 1000;

uint32_t xran_lib_ota_tti        = 0; /* [0:(1000000/TTI-1)] */
uint32_t xran_lib_ota_sym        = 0; /* [0:1000/TTI-1] */
uint32_t xran_lib_ota_sym_idx    = 0; /* [0 : 14*(1000000/TTI)-1]
                                                where TTI is TTI interval in microseconds */

static uint8_t xran_cp_seq_id_num[XRAN_MAX_CELLS_PER_PORT][XRAN_DIR_MAX][XRAN_MAX_ANTENNA_NR * 2]; //XRAN_MAX_ANTENNA_NR * 2 for PUSCH and PRACH
static uint8_t xran_updl_seq_id_num[XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR];
static uint8_t xran_upul_seq_id_num[XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR * 2];

static uint8_t xran_section_id_curslot[XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR * 2];
static uint16_t xran_section_id[XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR * 2];

void xran_timer_arm(struct rte_timer *tim, void* arg);

int xran_process_tx_sym(void *arg);

int xran_process_rx_sym(void *arg,
                        struct rte_mbuf *mbuf,
                        void *iq_data_start,
                        uint16_t size,
                        uint8_t CC_ID,
                        uint8_t Ant_ID,
                        uint8_t frame_id,
                        uint8_t subframe_id,
                        uint8_t slot_id,
                        uint8_t symb_id,
                        uint16_t num_prbu,
                        uint16_t start_prbu,
                        uint16_t sym_inc,
                        uint16_t rb,
                        uint16_t sect_id,
                        uint32_t *mb_free);

int xran_process_prach_sym(void *arg,
                        struct rte_mbuf *mbuf,
                        void *iq_data_start,
                        uint16_t size,
                        uint8_t CC_ID,
                        uint8_t Ant_ID,
                        uint8_t frame_id,
                        uint8_t subframe_id,
                        uint8_t slot_id,
                        uint8_t symb_id,
                        uint16_t num_prbu,
                        uint16_t start_prbu,
                        uint16_t sym_inc,
                        uint16_t rb,
                        uint16_t sect_id);

void tti_ota_cb(struct rte_timer *tim, void *arg);
void tti_to_phy_cb(struct rte_timer *tim, void *arg);
void xran_timer_arm_ex(struct rte_timer *tim, void* CbFct, void *CbArg, unsigned tim_lcore);

struct xran_device_ctx *xran_dev_get_ctx(void)
{
    return &g_xran_dev_ctx[0];
}

static inline struct xran_fh_config *xran_lib_get_ctx_fhcfg(void)
{
    return (&(xran_dev_get_ctx()->fh_cfg));
}

uint16_t xran_get_beamid(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id)
{
    return (0);     // NO BEAMFORMING
}

enum xran_if_state xran_get_if_state(void) 
{
    return xran_if_current_state;
}

int xran_isPRACHSlot(uint32_t subframe_id, uint32_t slot_id)
{
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    xRANPrachCPConfigStruct *pPrachCPConfig = &(p_xran_dev_ctx->PrachCPConfig);
    int isPRACHslot = 0;

    if (p_xran_dev_ctx->fh_cfg.frame_conf.nNumerology < 2){
        //for FR1, in 38.211 tab 6.3.3.2-2&3 it is subframe index
        if (pPrachCPConfig->isPRACHslot[subframe_id] == 1){
            if (pPrachCPConfig->nrofPrachInSlot != 1)
                isPRACHslot = 1;
            else{
                if (p_xran_dev_ctx->fh_cfg.frame_conf.nNumerology == 0)
                    isPRACHslot = 1;
                else if (slot_id == 1)
                    isPRACHslot = 1;
            }
        }
    }
    else if (p_xran_dev_ctx->fh_cfg.frame_conf.nNumerology == 3){
        //for FR2, 38.211 tab 6.3.3.4 it is slot index of 60kHz slot
        uint32_t slotidx;
        slotidx = subframe_id * SLOTNUM_PER_SUBFRAME + slot_id;
        if (pPrachCPConfig->nrofPrachInSlot == 2){
            if (pPrachCPConfig->isPRACHslot[slotidx>>1] == 1)
                isPRACHslot = 1;
        }
        else{
            if ((pPrachCPConfig->isPRACHslot[slotidx>>1] == 1) && (slotidx % 2 == 1))
                isPRACHslot = 1;
        }
    }
    else
        print_err("Numerology %d not supported", p_xran_dev_ctx->fh_cfg.frame_conf.nNumerology);
    return isPRACHslot;
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

int xran_init_prach(struct xran_fh_config* pConf, struct xran_device_ctx * p_xran_dev_ctx)
{
    int32_t i;
    uint8_t slotNr;
    struct xran_prach_config* pPRACHConfig = &(pConf->prach_conf);
    const xRANPrachConfigTableStruct *pxRANPrachConfigTable;
    uint8_t nNumerology = pConf->frame_conf.nNumerology;
    uint8_t nPrachConfIdx = pPRACHConfig->nPrachConfIdx;
    xRANPrachCPConfigStruct *pPrachCPConfig = &(p_xran_dev_ctx->PrachCPConfig);

    if (nNumerology > 2)
        pxRANPrachConfigTable = &gxranPrachDataTable_mmw[nPrachConfIdx];
    else if (pConf->frame_conf.nFrameDuplexType == 1)
        pxRANPrachConfigTable = &gxranPrachDataTable_sub6_tdd[nPrachConfIdx];
    else
        pxRANPrachConfigTable = &gxranPrachDataTable_sub6_fdd[nPrachConfIdx];

    uint8_t preambleFmrt = pxRANPrachConfigTable->preambleFmrt[0];
    const xRANPrachPreambleLRAStruct *pxranPreambleforLRA = &gxranPreambleforLRA[preambleFmrt];
    memset(pPrachCPConfig, 0, sizeof(xRANPrachCPConfigStruct));
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

    return (XRAN_STATUS_SUCCESS);
}

inline uint16_t xran_alloc_sectionid(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id)
{
    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (0);
        }
    if(ant_id >= XRAN_MAX_ANTENNA_NR * 2) {  //for PRACH, ant_id starts from num_ant
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

int xran_init_seqid(void *pHandle)
{
    int cell, dir, ant;

    for(cell=0; cell < XRAN_MAX_CELLS_PER_PORT; cell++) {
        for(dir=0; dir < XRAN_DIR_MAX; dir++) {
            for(ant=0; ant < XRAN_MAX_ANTENNA_NR * 2; ant++)
                xran_cp_seq_id_num[cell][dir][ant] = 0;
            }
        for(ant=0; ant < XRAN_MAX_ANTENNA_NR; ant++)
                xran_updl_seq_id_num[cell][ant] = 0;
        for(ant=0; ant < XRAN_MAX_ANTENNA_NR * 2; ant++)
                xran_upul_seq_id_num[cell][ant] = 0;
        }

    return (0);
}

static inline uint8_t xran_get_cp_seqid(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ant_id)
{
    if(dir >= XRAN_DIR_MAX) {
        print_err("Invalid direction - %d", dir);
        return (0);
        }
    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (0);
        }
    if(ant_id >= XRAN_MAX_ANTENNA_NR * 2) {
        print_err("Invalid antenna ID - %d", ant_id);
        return (0);
        }

    return(xran_cp_seq_id_num[cc_id][dir][ant_id]++);
}
static inline uint8_t xran_get_updl_seqid(void *pHandle, uint8_t cc_id, uint8_t ant_id)
{
    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (0);
        }
    if(ant_id >= XRAN_MAX_ANTENNA_NR) {
        print_err("Invalid antenna ID - %d", ant_id);
        return (0);
        }

    /* Only U-Plane DL needs to get sequence ID in O-DU */
    return(xran_updl_seq_id_num[cc_id][ant_id]++);
}
static inline uint8_t *xran_get_updl_seqid_addr(void *pHandle, uint8_t cc_id, uint8_t ant_id)
{
    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (0);
        }
    if(ant_id >= XRAN_MAX_ANTENNA_NR) {
        print_err("Invalid antenna ID - %d", ant_id);
        return (0);
        }

    /* Only U-Plane DL needs to get sequence ID in O-DU */
    return(&xran_updl_seq_id_num[cc_id][ant_id]);
}
static inline int8_t xran_check_upul_seqid(void *pHandle, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id, uint8_t seq_id)
{

    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (-1);
    }

    if(ant_id >= XRAN_MAX_ANTENNA_NR * 2) {
        print_err("Invalid antenna ID - %d", ant_id);
        return (-1);
    }

    /* O-DU needs to check the sequence ID of U-Plane UL from O-RU */
    xran_upul_seq_id_num[cc_id][ant_id]++;
    if(xran_upul_seq_id_num[cc_id][ant_id] == seq_id) { /* expected sequence */
        return (XRAN_STATUS_SUCCESS);
    } else {
        print_err("expected seqid %u received %u, slot %u, ant %u cc %u", xran_upul_seq_id_num[cc_id][ant_id], seq_id, slot_id, ant_id, cc_id);
        xran_upul_seq_id_num[cc_id][ant_id] = seq_id; // for next
        return (-1);
    }
}

//////////////////////////////////////////
// For RU emulation
static inline uint8_t xran_get_upul_seqid(void *pHandle, uint8_t cc_id, uint8_t ant_id)
{
    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (0);
        }
    if(ant_id >= XRAN_MAX_ANTENNA_NR * 2) {
        print_err("Invalid antenna ID - %d", ant_id);
        return (0);
        }

    return(xran_upul_seq_id_num[cc_id][ant_id]++);
}
static inline uint8_t *xran_get_upul_seqid_addr(void *pHandle, uint8_t cc_id, uint8_t ant_id)
{
    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (0);
        }
    if(ant_id >= XRAN_MAX_ANTENNA_NR * 2) {
        print_err("Invalid antenna ID - %d", ant_id);
        return (0);
        }

    return(&xran_upul_seq_id_num[cc_id][ant_id]);
}
static inline int8_t xran_check_cp_seqid(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ant_id, uint8_t seq_id)
{
    if(dir >= XRAN_DIR_MAX) {
        print_err("Invalid direction - %d", dir);
        return (-1);
        }
    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (-1);
        }
    if(ant_id >= XRAN_MAX_ANTENNA_NR * 2) {
        print_err("Invalid antenna ID - %d", ant_id);
        return (-1);
        }

    xran_cp_seq_id_num[cc_id][dir][ant_id]++;
    if(xran_cp_seq_id_num[cc_id][dir][ant_id] == seq_id) { /* expected sequence */
        return (0);
        }
    else {
        xran_cp_seq_id_num[cc_id][dir][ant_id] = seq_id;
        return (-1);
        }
}
static inline int8_t xran_check_updl_seqid(void *pHandle, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id, uint8_t seq_id)
{
    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (-1);
    }

    if(ant_id >= XRAN_MAX_ANTENNA_NR) {
        print_err("Invalid antenna ID - %d", ant_id);
        return (-1);
    }

    /* O-RU needs to check the sequence ID of U-Plane DL from O-DU */
    xran_updl_seq_id_num[cc_id][ant_id]++;
    if(xran_updl_seq_id_num[cc_id][ant_id] == seq_id) {
        /* expected sequence */
        return (0);
    } else {
        xran_updl_seq_id_num[cc_id][ant_id] = seq_id;
        return (-1);
    }
}


static struct xran_section_gen_info cpSections[XRAN_MAX_NUM_SECTIONS];
static struct xran_cp_gen_params cpInfo;
int process_cplane(struct rte_mbuf *pkt)
{
  struct xran_recv_packet_info recv;

    cpInfo.sections = cpSections;
    xran_parse_cp_pkt(pkt, &cpInfo, &recv);

    return (MBUF_FREE);
}
//////////////////////////////////////////

void sym_ota_cb(struct rte_timer *tim, void *arg)
{
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    struct xran_timer_ctx *pTCtx = (struct xran_timer_ctx *)arg;
    long t1 = MLogTick();
    static int32_t ctx = 0;

    if(XranGetSymNum(xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT) == 0){
        tti_ota_cb(NULL, arg);
    }

    if(XranGetSymNum(xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT) == 3){
        if(p_xran_dev_ctx->phy_tti_cb_done == 0){
            /* rearm timer to deliver TTI event to PHY */
            p_xran_dev_ctx->phy_tti_cb_done = 0;
            xran_timer_arm_ex(&tti_to_phy_timer[xran_lib_ota_tti % 10], tti_to_phy_cb, (void*)pTCtx, p_xran_dev_ctx->fh_init.io_cfg.timing_core);
        }
    }

    xran_process_tx_sym(timer_ctx);
    /* check if there is call back to do something else on this symbol */
    if(p_xran_dev_ctx->pSymCallback[0][xran_lib_ota_sym]){
        p_xran_dev_ctx->pSymCallback[0][xran_lib_ota_sym](&dpdk_timer[ctx], p_xran_dev_ctx->pSymCallbackTag[0][xran_lib_ota_sym]);
        ctx = DpdkTiemerIncrementCtx(ctx);
    }

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
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();

    MLogTask(PID_TTI_TIMER, t1, MLogTick());

    /* To match TTbox */
    if(xran_lib_ota_tti == 0)
        reg_tti = xran_fs_get_max_slot() - 1;
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

    if(p_xran_dev_ctx->fh_init.io_cfg.id == ID_LLS_CU)
        next_tti = xran_lib_ota_tti + 1;
    else
        next_tti = xran_lib_ota_tti;

    if(next_tti>= xran_fs_get_max_slot()){
        print_dbg("[%d]SFN %d sf %d slot %d\n",next_tti, frame_id, subframe_id, slot_id);
        next_tti=0;
    }

    slot_id     = XranGetSlotNum(next_tti, SLOTNUM_PER_SUBFRAME);
    subframe_id = XranGetSubFrameNum(next_tti,SLOTNUM_PER_SUBFRAME,  SUBFRAMES_PER_SYSTEMFRAME);
    frame_id    = XranGetFrameNum(next_tti,SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME);

    print_dbg("[%d]SFN %d sf %d slot %d\n",next_tti, frame_id, subframe_id, slot_id);

    if(p_xran_dev_ctx->fh_init.io_cfg.id == ID_LLS_CU){
        pTCtx[(xran_lib_ota_tti & 1)].tti_to_process = next_tti;
    } else {
        pTCtx[(xran_lib_ota_tti & 1)].tti_to_process = pTCtx[(xran_lib_ota_tti & 1)^1].tti_to_process;
    }

    p_xran_dev_ctx->phy_tti_cb_done = 0;
    xran_timer_arm_ex(&tti_to_phy_timer[xran_lib_ota_tti % 10], tti_to_phy_cb, (void*)pTCtx, p_xran_dev_ctx->fh_init.io_cfg.timing_core);

    xran_lib_ota_tti++;
    if(xran_lib_ota_tti >= xran_fs_get_max_slot()){
        print_dbg("[%d]SFN %d sf %d slot %d\n",xran_lib_ota_tti, frame_id, subframe_id, slot_id);
        xran_lib_ota_tti=0;
    }
    MLogTask(PID_TTI_CB, t1, MLogTick());
}

void xran_timer_arm(struct rte_timer *tim, void* arg)
{
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    uint64_t t3 = MLogTick();

    if (xran_if_current_state == XRAN_RUNNING){
        rte_timer_cb_t fct = (rte_timer_cb_t)arg;
        rte_timer_init(tim);
        rte_timer_reset_sync(tim, 0, SINGLE, p_xran_dev_ctx->fh_init.io_cfg.timing_core, fct, &timer_ctx[0]);
    }
    MLogTask(PID_TIME_ARM_TIMER, t3, MLogTick());
}

void xran_timer_arm_ex(struct rte_timer *tim, void* CbFct, void *CbArg, unsigned tim_lcore)
{
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    uint64_t t3 = MLogTick();

    if (xran_if_current_state == XRAN_RUNNING){
        rte_timer_cb_t fct = (rte_timer_cb_t)CbFct;
        rte_timer_init(tim);
        rte_timer_reset_sync(tim, 0, SINGLE, tim_lcore, fct, CbArg);
    }
    MLogTask(PID_TIME_ARM_TIMER, t3, MLogTick());
}

int xran_cp_create_and_send_section(void *pHandle, uint8_t ru_port_id, int dir, int tti, int cc_id,
        struct xran_prb_map *prbMapElem)
{
    struct xran_cp_gen_params params;
    struct xran_section_gen_info sect_geninfo[XRAN_MAX_NUM_SECTIONS];
    struct rte_mbuf *mbuf;
    int ret = 0;
    uint32_t i;
    uint32_t nsection = prbMapElem->nPrbElm;
    struct xran_prb_elm *pPrbMapElem = &prbMapElem->prbMap[0];
    struct xran_prb_elm *pPrbMapElemPrev;
    uint32_t slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME);
    uint32_t subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME,  SUBFRAMES_PER_SYSTEMFRAME);
    uint32_t frame_id    = XranGetFrameNum(tti,SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME);
    uint8_t seq_id = xran_get_cp_seqid(pHandle, XRAN_DIR_DL, cc_id, ru_port_id);

    params.dir                  = dir;
    params.sectionType          = XRAN_CP_SECTIONTYPE_1;        // Most DL/UL Radio Channels
    params.hdr.filterIdx        = XRAN_FILTERINDEX_STANDARD;
    params.hdr.frameId          = frame_id;
    params.hdr.subframeId       = subframe_id;
    params.hdr.slotId           = slot_id;
    params.hdr.startSymId       = pPrbMapElem->nStartSymb;
    params.hdr.iqWidth          = xran_get_conf_iqwidth(pHandle);
    params.hdr.compMeth         = pPrbMapElem->compMethod;

    for (i=0; i<nsection; i++)
    {
        pPrbMapElem = &prbMapElem->prbMap[i];
        sect_geninfo[i].info.type        = params.sectionType;       // for database
        sect_geninfo[i].info.startSymId  = params.hdr.startSymId;    // for database
        sect_geninfo[i].info.iqWidth     = params.hdr.iqWidth;       // for database
        sect_geninfo[i].info.compMeth    = params.hdr.compMeth;      // for database
        sect_geninfo[i].info.id          = xran_alloc_sectionid(pHandle, dir, cc_id, ru_port_id, slot_id);
        sect_geninfo[i].info.rb          = XRAN_RBIND_EVERY;
        sect_geninfo[i].info.startPrbc   = pPrbMapElem->nRBStart;
        sect_geninfo[i].info.numPrbc     = pPrbMapElem->nRBSize;
        sect_geninfo[i].info.numSymbol   = pPrbMapElem->numSymb;
        sect_geninfo[i].info.reMask      = 0xfff;
        sect_geninfo[i].info.beamId      = pPrbMapElem->nBeamIndex;
        if (i==0)
            sect_geninfo[i].info.symInc      = XRAN_SYMBOLNUMBER_NOTINC;
        else
        {
            pPrbMapElemPrev = &prbMapElem->prbMap[i-1];
            if (pPrbMapElemPrev->nStartSymb == pPrbMapElem->nStartSymb)
            {
                sect_geninfo[i].info.symInc      = XRAN_SYMBOLNUMBER_NOTINC;
                if (pPrbMapElemPrev->numSymb != pPrbMapElem->numSymb)
                    print_err("section info error: previous numSymb %d not equal to current numSymb %d\n", pPrbMapElemPrev->numSymb, pPrbMapElem->numSymb);
            }
            else
            {
                sect_geninfo[i].info.symInc      = XRAN_SYMBOLNUMBER_INC;
                if (pPrbMapElem->nStartSymb != (pPrbMapElemPrev->nStartSymb + pPrbMapElemPrev->numSymb))
                    print_err("section info error: current startSym %d not equal to previous endSymb %d\n", pPrbMapElem->nStartSymb, pPrbMapElemPrev->nStartSymb + pPrbMapElemPrev->numSymb);
            }
        }

        /* extension is not supported */
        sect_geninfo[nsection].info.ef          = 0;
        sect_geninfo[nsection].exDataSize       = 0;
        //sect_geninfo[nsection].exData           = NULL;
    }
    params.numSections          = nsection;
    params.sections             = sect_geninfo;

    mbuf = xran_ethdi_mbuf_alloc();
    if(unlikely(mbuf == NULL)) {
        print_err("Alloc fail!\n");
        return (-1);
        }

    ret = xran_prepare_ctrl_pkt(mbuf, &params, cc_id, ru_port_id, seq_id);
    if(ret < 0) {
        print_err("Fail to build control plane packet - [%d:%d:%d] dir=%d\n",
                    frame_id, subframe_id, slot_id, dir);
    } else {
        /* add in the ethernet header */
        struct ether_hdr *const h = (void *)rte_pktmbuf_prepend(mbuf, sizeof(*h));
        xran_ethdi_mbuf_send_cp(mbuf, ETHER_TYPE_ECPRI);
        tx_counter++;
        for(i=0; i<nsection; i++)
            xran_cp_add_section_info(pHandle,
                    dir, cc_id, ru_port_id,
                    (slot_id + subframe_id*SLOTNUM_PER_SUBFRAME)%XRAN_MAX_SECTIONDB_CTX,
                    &sect_geninfo[i].info);
    }

    return ret;
}
#if 0
int xran_cp_create_rbmap(int dir, int tti, int cc_id,
        struct xran_flat_buffer *prbMapElem,
        struct xran_cp_rbmap_list *rbMapList)
{
  struct xran_prb_map *prb_map;
  int list_index = 0;
  int sym_id;
  int i, j;

    prb_map  = (struct xran_prb_map *)prbMapElm[0].pData;
    cc_id   = prb_map->cc_id;
    tti     = prb_map->tti_id;
    dir     = prb_map->dir;


    list_index = -1;
    for(sym_id = 0; sym_id < N_SYM_PER_SLOT; sym_id++) {
        /* skip symbol, if not matched with given direction */
        int type_sym = xran_fs_get_symbol_type(cc_id, tti, sym_id);
        if(type_sym != XRAN_SYMBOL_TYPE_FDD && type_sym != dir)
            continue;

        /* retrieve the information of RB allocation */
        prb_map = (struct xran_prb_map *)prbMapElem[sym_id].pData;
        if(unlikely(prb_map == NULL)) {
            print_err("RB allocation table is NULL! (tti:%d, cc:%d, sym_id:%d)", tti, cc_id, sym_id);
            continue;
            }

        /* creating 2D mapping */
        for(i=0; i < prb_map->nPrbElm; i++) {
            if(list_index < 0) {   /* create first entry */
                list_index = 0;
                rbMapList[list_index].grp_id    = 0;
                rbMapList[list_index].sym_start = sym_id;   // prb_map->sym_id
                rbMapList[list_index].sym_num   = 1;
                rbMapList[list_index].rb_start  = prb_map->prbMap[i].nRBStart;
                rbMapList[list_index].rb_num    = prb_map->prbMap[i].nRBSize;
                rbMapList[list_index].beam_id   = prb_map->prbMap[i].nBeamIndex;
                rbMapList[list_index].comp_meth = prb_map->prbMap[i].compMethod;
                }
            else {
                /* find consecutive allocation from list */
                for(j=0; j<list_index+1; j++) {
                    if(prb_map->prbMap[i].nRBStart   != rbMapList[j].rb_start
                            || prb_map->prbMap[i].nRBSize    != rbMapList[j].rb_num
                            || prb_map->prbMap[i].nBeamIndex != rbMapList[j].beam_id
                            || prb_map->prbMap[i].compMethod != rbMapList[j].comp_meth
                            || sym_id != (rbMapList[j].sym_start+rbMapList[j].sym_num)) {
                        /* move to next */
                        continue;
                        }
                    else {
                        /* consecutive allocation has been found */
                        rbMapList[j].sym_num++;
                        break;
                        }
                    }

                if(j == list_index+1) { /* different allocation, create new entry */
                    list_index++;
                    rbMapList[list_index].grp_id    = 0;
                    rbMapList[list_index].sym_start = sym_id;   // prb_map->sym_id
                    rbMapList[list_index].sym_num   = 1;
                    rbMapList[list_index].rb_start  = prb_map->prbMap[i].nRBStart;
                    rbMapList[list_index].rb_num    = prb_map->prbMap[i].nRBSize;
                    rbMapList[list_index].beam_id   = prb_map->prbMap[i].nBeamIndex;
                    rbMapList[list_index].comp_meth = prb_map->prbMap[i].compMethod;
                    }
                }
            } /* for(i=0; i < prb_map->nPrbElm; i++) */
        } /* for(sym_id = 0; sym_id < N_SYM_PER_SLOT; sym_id++) */

    list_index++;

#if 0
    for(i=0; i<list_index; i++) {
        printf("[%c:%d-%d] %d - symstart=%d, symnum=%d, rbstart=%d, rbnum=%d, beamid=%d, comp=%d\n",
                dir?'U':'D', tti, cc_id, i,
                rbMapList[i].sym_start, rbMapList[i].sym_num,
                rbMapList[i].rb_start, rbMapList[i].rb_num,
                rbMapList[i].beam_id, rbMapList[i].comp_meth);
        }
#endif
    return (list_index);
}
#endif

void tx_cp_dl_cb(struct rte_timer *tim, void *arg)
{
  long t1 = MLogTick();
  int tti, buf_id;
  int i, ret;
  uint32_t slot_id, subframe_id, frame_id;
  int cc_id;
  uint8_t ctx_id;
  uint8_t ant_id, num_eAxc, num_CCPorts;
  void *pHandle;
  int num_list;
  struct xran_cp_rbmap_list rb_map_list[XRAN_MAX_PRBS*N_SYM_PER_SLOT];    /* array size can be reduced */

  struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
  struct xran_timer_ctx *pTCtx = (struct xran_timer_ctx *)arg;

    pHandle = NULL;     // TODO: temp implemantation
    num_eAxc    = xran_get_num_eAxc(pHandle);
    num_CCPorts = xran_get_num_cc(pHandle);

    if(p_xran_dev_ctx->enableCP) {

        tti = pTCtx[(xran_lib_ota_tti & 1) ^ 1].tti_to_process;
        buf_id = tti % XRAN_N_FE_BUF_LEN;

        slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME);
        subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME,  SUBFRAMES_PER_SYSTEMFRAME);
        frame_id    = XranGetFrameNum(tti,SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME);
        ctx_id      = XranGetSlotNum(tti, SLOTS_PER_SYSTEMFRAME) % XRAN_MAX_SECTIONDB_CTX;

        print_dbg("[%d]SFN %d sf %d slot %d\n", tti, frame_id, subframe_id, slot_id);

        for(ant_id = 0; ant_id < num_eAxc; ++ant_id) {
            for(cc_id = 0; cc_id < num_CCPorts; cc_id++ ) {

                /* start new section information list */
                xran_cp_reset_section_info(pHandle, XRAN_DIR_DL, cc_id, ant_id, ctx_id);

                if(xran_fs_get_slot_type(cc_id, tti, XRAN_SLOT_TYPE_DL) == 1) { // 1 when FDD, DL slot or DL symbol is present in SP slot
                    if (p_xran_dev_ctx->DynamicSectionEna){
                        num_list = xran_cp_create_and_send_section(pHandle, ant_id, XRAN_SYMBOL_TYPE_DL, tti, cc_id,
                            (struct xran_prb_map *)p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[buf_id][cc_id][ant_id].sBufferList.pBuffers->pData);
                    }
                    else{
                        struct xran_cp_gen_params params;
                        struct xran_section_gen_info sect_geninfo[8];
                        struct rte_mbuf *mbuf = xran_ethdi_mbuf_alloc();

                        /* use symb 0 only with constant RBs for full slot */
                        struct xran_prb_map *prb_map = (struct xran_prb_map *)p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[buf_id][cc_id][ant_id].sBufferList.pBuffers->pData;
                        num_list = 1;
                        rb_map_list[0].sym_start = 0;
                        rb_map_list[0].sym_num   = 14;
                        rb_map_list[0].rb_start  = prb_map->prbMap[0].nRBStart;
                        rb_map_list[0].rb_num    = prb_map->prbMap[0].nRBSize;
                        rb_map_list[0].beam_id   = prb_map->prbMap[0].nBeamIndex;
                        rb_map_list[0].comp_meth = prb_map->prbMap[0].compMethod;

                        for(i=0; i<num_list; i++) {
                            ret = generate_cpmsg_dlul(pHandle, &params, sect_geninfo, mbuf, XRAN_DIR_DL,
                                    frame_id, subframe_id, slot_id,
                                    rb_map_list[i].sym_start, rb_map_list[i].sym_num,
                                    rb_map_list[i].rb_start, rb_map_list[i].rb_num,
                                    rb_map_list[i].beam_id, cc_id, ant_id, rb_map_list[i].comp_meth,
                                    xran_get_cp_seqid(pHandle, XRAN_DIR_DL, cc_id, ant_id), XRAN_SYMBOLNUMBER_NOTINC);

                            if (ret == XRAN_STATUS_SUCCESS)
                                send_cpmsg(pHandle, mbuf, &params, sect_geninfo,
                                    cc_id, ant_id, xran_get_cp_seqid(pHandle, XRAN_DIR_UL, cc_id, ant_id));
                        }
                    }
                } /* if(xran_fs_get_slot_type(cc_id, tti, XRAN_SLOT_TYPE_DL) == 1) */
            } /* for(cc_id = 0; cc_id < num_CCPorts; cc_id++) */
        } /* for(ant_id = 0; ant_id < num_eAxc; ++ant_id) */
    } /* if(p_xran_dev_ctx->enableCP) */

    MLogTask(PID_CP_DL_CB, t1, MLogTick());
}

void rx_ul_deadline_half_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogTick();
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    xran_status_t status;
    /* half of RX for current TTI as measured against current OTA time */
    int32_t rx_tti = (int32_t)XranGetTtiNum(xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
    int32_t cc_id;

    if(p_xran_dev_ctx->xran2phy_mem_ready == 0)
        return;

    for(cc_id = 0; cc_id < xran_get_num_cc(p_xran_dev_ctx); cc_id++) {
        if(p_xran_dev_ctx->rx_packet_callback_tracker[rx_tti % XRAN_N_FE_BUF_LEN][cc_id] == 0){
            status = (rx_tti << 16) | 0; /* base on PHY side implementation first 7 sym of slot */
            if(p_xran_dev_ctx->pCallback[cc_id])
               p_xran_dev_ctx->pCallback[cc_id](p_xran_dev_ctx->pCallbackTag[cc_id], status);
        } else {
            p_xran_dev_ctx->rx_packet_callback_tracker[rx_tti % XRAN_N_FE_BUF_LEN][cc_id] = 0;
        }
    }
    MLogTask(PID_UP_UL_HALF_DEAD_LINE_CB, t1, MLogTick());
}

void rx_ul_deadline_full_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogTick();
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    xran_status_t status = 0;
    int32_t rx_tti = (int32_t)XranGetTtiNum(xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
    int32_t cc_id = 0;

    if(rx_tti == 0)
       rx_tti = (xran_fs_get_max_slot()-1);
    else
       rx_tti -= 1; /* end of RX for prev TTI as measured against current OTA time */

    if(p_xran_dev_ctx->xran2phy_mem_ready == 0)
        return;

    /* U-Plane */
    for(cc_id = 0; cc_id < xran_get_num_cc(p_xran_dev_ctx); cc_id++) {
        status = (rx_tti << 16) | 7; /* last 7 sym means full slot of Symb */
        if(p_xran_dev_ctx->pCallback[cc_id])
           p_xran_dev_ctx->pCallback[cc_id](p_xran_dev_ctx->pCallbackTag[cc_id], status);

        if(p_xran_dev_ctx->pPrachCallback[cc_id])
           p_xran_dev_ctx->pPrachCallback[cc_id](p_xran_dev_ctx->pPrachCallbackTag[cc_id], status);

    }

    MLogTask(PID_UP_UL_FULL_DEAD_LINE_CB, t1, MLogTick());
}


void tx_cp_ul_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogTick();
    int tti, buf_id;
    int i, ret;
    uint32_t slot_id, subframe_id, frame_id;
    int32_t cc_id;
    int ant_id, prach_port_id;
    uint16_t beam_id;
    uint8_t num_eAxc, num_CCPorts;
    uint8_t ctx_id;

    void *pHandle;
    int num_list;
    struct xran_cp_rbmap_list rb_map_list[XRAN_MAX_PRBS*N_SYM_PER_SLOT];    /* array size can be reduced */

    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    xRANPrachCPConfigStruct *pPrachCPConfig = &(p_xran_dev_ctx->PrachCPConfig);
    struct xran_timer_ctx *pTCtx = (struct xran_timer_ctx *)arg;

    pHandle     = NULL;     // TODO: temp implemantation
    num_eAxc    = xran_get_num_eAxc(pHandle);
    num_CCPorts = xran_get_num_cc(pHandle);
    tti = pTCtx[(xran_lib_ota_tti & 1) ^ 1].tti_to_process;
    buf_id = tti % XRAN_N_FE_BUF_LEN;
    slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME);
    subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME,  SUBFRAMES_PER_SYSTEMFRAME);
    frame_id    = XranGetFrameNum(tti,SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME);
    ctx_id      = XranGetSlotNum(tti, SLOTS_PER_SYSTEMFRAME) % XRAN_MAX_SECTIONDB_CTX;

    if(p_xran_dev_ctx->enableCP) {

        print_dbg("[%d]SFN %d sf %d slot %d\n", tti, frame_id, subframe_id, slot_id);

        for(ant_id = 0; ant_id < num_eAxc; ++ant_id) {
            for(cc_id = 0; cc_id < num_CCPorts; cc_id++) {
                if(xran_fs_get_slot_type(cc_id, tti, XRAN_SLOT_TYPE_UL) == 1 ||
                    xran_fs_get_slot_type(cc_id, tti, XRAN_SLOT_TYPE_SP) == 1 ){
                    /* start new section information list */
                    xran_cp_reset_section_info(pHandle, XRAN_DIR_UL, cc_id, ant_id, ctx_id);
                    if (p_xran_dev_ctx->DynamicSectionEna){
                        /* create a map of RB allocation to generate proper C-Plane */
                        num_list = xran_cp_create_and_send_section(pHandle, ant_id, XRAN_SYMBOL_TYPE_UL, tti, cc_id,
                            (struct xran_prb_map *)p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[buf_id][cc_id][ant_id].sBufferList.pBuffers->pData);
                    }
                    else{
                        struct xran_cp_gen_params params;
                        struct xran_section_gen_info sect_geninfo[8];
                        struct rte_mbuf *mbuf = xran_ethdi_mbuf_alloc();
                        /* use symb 0 only with constant RBs for full slot */
                        struct xran_prb_map *prb_map = (struct xran_prb_map *)p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[buf_id][cc_id][ant_id].sBufferList.pBuffers->pData;
                        num_list = 1;
                        rb_map_list[0].sym_start = 0;
                        rb_map_list[0].sym_num   = 14;
                        rb_map_list[0].rb_start  = prb_map->prbMap[0].nRBStart;
                        rb_map_list[0].rb_num    = prb_map->prbMap[0].nRBSize;
                        rb_map_list[0].beam_id   = prb_map->prbMap[0].nBeamIndex;
                        rb_map_list[0].comp_meth = prb_map->prbMap[0].compMethod;

                        for(i=0; i<num_list; i++) {
                            ret = generate_cpmsg_dlul(pHandle, &params, sect_geninfo, mbuf, XRAN_DIR_UL,
                                    frame_id, subframe_id, slot_id,
                                    rb_map_list[i].sym_start, rb_map_list[i].sym_num,
                                    rb_map_list[i].rb_start, rb_map_list[i].rb_num,
                                    rb_map_list[i].beam_id, cc_id, ant_id, rb_map_list[i].comp_meth,
                                    xran_get_cp_seqid(pHandle, XRAN_DIR_UL, cc_id, ant_id), XRAN_SYMBOLNUMBER_NOTINC);
                            if (ret == XRAN_STATUS_SUCCESS)
                                send_cpmsg(pHandle, mbuf, &params, sect_geninfo,
                                    cc_id, ant_id, xran_get_cp_seqid(pHandle, XRAN_DIR_UL, cc_id, ant_id));
                        }
                    }
                } /* if(xran_fs_get_slot_type(cc_id, tti, XRAN_SLOT_TYPE_UL) == 1 || */

            } /* for(cc_id = 0; cc_id < num_CCPorts; cc_id++) */
        } /* for(ant_id = 0; ant_id < num_eAxc; ++ant_id) */
    } /* if(p_xran_dev_ctx->enableCP) */

    if(p_xran_dev_ctx->enablePrach) {
        uint32_t isPRACHslot = xran_isPRACHSlot(subframe_id, slot_id);
        if((frame_id % pPrachCPConfig->x == pPrachCPConfig->y[0]) && (isPRACHslot==1)) {   //is prach slot
            for(ant_id = 0; ant_id < num_eAxc; ++ant_id) {
                for(cc_id = 0; cc_id < num_CCPorts; cc_id++) {
                    struct xran_cp_gen_params params;
                    struct xran_section_gen_info sect_geninfo[8];
                    struct rte_mbuf *mbuf = xran_ethdi_mbuf_alloc();
                    prach_port_id = ant_id + num_eAxc;
                    /* start new section information list */
                    xran_cp_reset_section_info(pHandle, XRAN_DIR_UL, cc_id, prach_port_id, ctx_id);

                    beam_id = xran_get_beamid(pHandle, XRAN_DIR_UL, cc_id, prach_port_id, slot_id); /* TODO: */
                    ret = generate_cpmsg_prach(pHandle, &params, sect_geninfo, mbuf, p_xran_dev_ctx,
                                frame_id, subframe_id, slot_id,
                                beam_id, cc_id, prach_port_id,
                                xran_get_cp_seqid(pHandle, XRAN_DIR_UL, cc_id, prach_port_id));
                    if (ret == XRAN_STATUS_SUCCESS)
                        send_cpmsg(pHandle, mbuf, &params, sect_geninfo,
                            cc_id, prach_port_id, xran_get_cp_seqid(pHandle, XRAN_DIR_UL, cc_id, prach_port_id));
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
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();

    static int first_call = 0;
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
            int32_t tti = (int32_t)XranGetTtiNum(xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
            if(tti == xran_fs_get_max_slot()-1)
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
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();

    /* ToS = Top of Second start +- 1.5us */
    struct timespec ts;

    char buff[100];

    printf("%s [CPU %2d] [PID: %6d]\n", __FUNCTION__,  rte_lcore_id(), getpid());

    /* set main thread affinity mask to CPU2 */
    sched_param.sched_priority = 98;

    CPU_ZERO(&cpuset);
    CPU_SET(p_xran_dev_ctx->fh_init.io_cfg.timing_core, &cpuset);
    if (result1 = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset))
    {
        printf("pthread_setaffinity_np failed: coreId = 2, result1 = %d\n",result1);
    }
    if ((result1 = pthread_setschedparam(pthread_self(), 1, &sched_param)))
    {
        printf("priority is not changed: coreId = 2, result1 = %d\n",result1);
    }

    if (p_xran_dev_ctx->fh_init.io_cfg.id == O_DU) {
        do {
           timespec_get(&ts, TIME_UTC);
        }while (ts.tv_nsec >1500);
        struct tm * ptm = gmtime(&ts.tv_sec);
        if(ptm){
            strftime(buff, sizeof buff, "%D %T", ptm);
            printf("lls-CU: thread_run start time: %s.%09ld UTC [%ld]\n", buff, ts.tv_nsec, interval_us);
        }

        delay_cp_dl = interval_us - p_xran_dev_ctx->fh_init.T1a_max_cp_dl;
        delay_cp_ul = interval_us - p_xran_dev_ctx->fh_init.T1a_max_cp_ul;
        delay_up    = p_xran_dev_ctx->fh_init.T1a_max_up;
        delay_up_ul = p_xran_dev_ctx->fh_init.Ta4_max;

        delay_cp2up = delay_up-delay_cp_dl;

        sym_cp_dl = delay_cp_dl*1000/(interval_us*1000/N_SYM_PER_SLOT)+1;
        sym_cp_ul = delay_cp_ul*1000/(interval_us*1000/N_SYM_PER_SLOT)+1;
        sym_up_ul = delay_up_ul*1000/(interval_us*1000/N_SYM_PER_SLOT);
        p_xran_dev_ctx->sym_up = sym_up = -(delay_up*1000/(interval_us*1000/N_SYM_PER_SLOT)+1);
        p_xran_dev_ctx->sym_up_ul = sym_up_ul = (delay_up_ul*1000/(interval_us*1000/N_SYM_PER_SLOT)+1);

        printf("Start C-plane DL %d us after TTI  [trigger on sym %d]\n", delay_cp_dl, sym_cp_dl);
        printf("Start C-plane UL %d us after TTI  [trigger on sym %d]\n", delay_cp_ul, sym_cp_ul);
        printf("Start U-plane DL %d us before OTA [offset  in sym %d]\n", delay_up, sym_up);
        printf("Start U-plane UL %d us OTA        [offset  in sym %d]\n", delay_up_ul, sym_up_ul);

        printf("C-plane to U-plane delay %d us after TTI\n", delay_cp2up);
        printf("Start Sym timer %ld ns\n", TX_TIMER_INTERVAL/N_SYM_PER_SLOT);

        p_xran_dev_ctx->pSymCallback[0][sym_cp_dl]    = xran_timer_arm;
        p_xran_dev_ctx->pSymCallbackTag[0][sym_cp_dl] = tx_cp_dl_cb;

        p_xran_dev_ctx->pSymCallback[0][sym_cp_ul]    = xran_timer_arm;
        p_xran_dev_ctx->pSymCallbackTag[0][sym_cp_ul] = tx_cp_ul_cb;

        /* Full slot UL OTA + delay_up_ul */
        p_xran_dev_ctx->pSymCallback[0][sym_up_ul]    = xran_timer_arm;
        p_xran_dev_ctx->pSymCallbackTag[0][sym_up_ul] = rx_ul_deadline_full_cb;

        /* Half slot UL OTA + delay_up_ul*/
        p_xran_dev_ctx->pSymCallback[0][sym_up_ul + N_SYM_PER_SLOT/2]    = xran_timer_arm;
        p_xran_dev_ctx->pSymCallbackTag[0][sym_up_ul + N_SYM_PER_SLOT/2] = rx_ul_deadline_half_cb;

    } else {    // APP_O_RU
        /* calcualte when to send UL U-plane */
        delay_up = p_xran_dev_ctx->fh_init.Ta3_min;
        p_xran_dev_ctx->sym_up = sym_up = delay_up*1000/(interval_us*1000/N_SYM_PER_SLOT)+1;
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

    printf("interval_us %ld\n", interval_us);
    do {
       timespec_get(&ts, TIME_UTC);
    }while (ts.tv_nsec == 0);

    while(1) {
        delta = poll_next_tick(interval_us*1000L/N_SYM_PER_SLOT);
        if (XRAN_STOPPED == xran_if_current_state)
            break;

        if (likely(XRAN_RUNNING == xran_if_current_state))
            sym_ota_cb(&sym_timer, timer_ctx);
    }
    printf("Closing timing source thread...tx counter %lu, rx counter %lu\n", tx_counter, rx_counter);

    return 0;
}

/* Handle ecpri format. */
int handle_ecpri_ethertype(struct rte_mbuf *pkt, uint64_t rx_time)
{
    const struct xran_ecpri_hdr *ecpri_hdr;
    unsigned long t1;
    int32_t ret = MBUF_FREE;

    if (rte_pktmbuf_data_len(pkt) < sizeof(struct xran_ecpri_hdr)) {
        print_err("Packet too short - %d bytes", rte_pktmbuf_data_len(pkt));
        return 0;
    }

    /* check eCPRI header. */
    ecpri_hdr = rte_pktmbuf_mtod(pkt, struct xran_ecpri_hdr *);
    if(ecpri_hdr == NULL){
        print_err("ecpri_hdr error\n");
        return MBUF_FREE;
    }

    switch(ecpri_hdr->cmnhdr.ecpri_mesg_type) {
        case ECPRI_IQ_DATA:
           // t1 = MLogTick();
            ret = process_mbuf(pkt);
          //  MLogTask(PID_PROCESS_UP_PKT, t1, MLogTick());
            break;
        // For RU emulation
        case ECPRI_RT_CONTROL_DATA:
            t1 = MLogTick();
            if(xran_dev_get_ctx()->fh_init.io_cfg.id == O_RU) {
                ret = process_cplane(pkt);
            } else {
                print_err("O-DU recevied C-Plane message!");
            }
            MLogTask(PID_PROCESS_CP_PKT, t1, MLogTick());
            break;
        default:
            print_err("Invalid eCPRI message type - %d", ecpri_hdr->cmnhdr.ecpri_mesg_type);
        }

    return ret;
}

int xran_process_prach_sym(void *arg,
                        struct rte_mbuf *mbuf,
                        void *iq_data_start,
                        uint16_t size,
                        uint8_t CC_ID,
                        uint8_t Ant_ID,
                        uint8_t frame_id,
                        uint8_t subframe_id,
                        uint8_t slot_id,
                        uint8_t symb_id,
                        uint16_t num_prbu,
                        uint16_t start_prbu,
                        uint16_t sym_inc,
                        uint16_t rb,
                        uint16_t sect_id)
{
    char        *pos = NULL;
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    uint8_t symb_id_offset;
    uint32_t tti = 0;
    xran_status_t status;
    void *pHandle = NULL;
    struct rte_mbuf *mb;

    uint16_t iq_sample_size_bits = 16;

    if(p_xran_dev_ctx->xran2phy_mem_ready == 0)
        return 0;

    tti = frame_id * SLOTS_PER_SYSTEMFRAME + subframe_id * SLOTNUM_PER_SUBFRAME + slot_id;

    status = tti << 16 | symb_id;

    if(tti < xran_fs_get_max_slot() && CC_ID < XRAN_MAX_SECTOR_NR && Ant_ID < XRAN_MAX_ANTENNA_NR && symb_id < XRAN_NUM_OF_SYMBOL_PER_SLOT){
        symb_id_offset = symb_id - p_xran_dev_ctx->prach_start_symbol[CC_ID]; //make the storing of prach packets to start from 0 for easy of processing within PHY
        pos = (char*) p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id_offset].pData;
        if(pos && iq_data_start && size){
            if (p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder == XRAN_CPU_LE_BYTE_ORDER) {
                int idx = 0;
                uint16_t *psrc = (uint16_t *)iq_data_start;
                uint16_t *pdst = (uint16_t *)pos;
                /* network byte (be) order of IQ to CPU byte order (le) */
                for (idx = 0; idx < size/sizeof(int16_t); idx++){
                    pdst[idx]  = (psrc[idx]>>8) | (psrc[idx]<<8); //rte_be_to_cpu_16(psrc[idx]);
                }
            }else {
                mb = p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id_offset].pCtrl;
                rte_pktmbuf_free(mb);
                p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id_offset].pData = iq_data_start;
                p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id_offset].pCtrl = mbuf;
            }
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

/*    if (symb_id == p_xran_dev_ctx->prach_last_symbol[CC_ID] ){
        p_xran_dev_ctx->rx_packet_prach_tracker[tti % XRAN_N_FE_BUF_LEN][CC_ID][symb_id]++;
        if(p_xran_dev_ctx->rx_packet_prach_tracker[tti % XRAN_N_FE_BUF_LEN][CC_ID][symb_id] >= xran_get_num_eAxc(pHandle)){
            if(p_xran_dev_ctx->pPrachCallback[0])
               p_xran_dev_ctx->pPrachCallback[0](p_xran_dev_ctx->pPrachCallbackTag[0], status);
            p_xran_dev_ctx->rx_packet_prach_tracker[tti % XRAN_N_FE_BUF_LEN][CC_ID][symb_id] = 0;
        }
    }
*/
    return size;
}

int32_t xran_pkt_validate(void *arg,
                        struct rte_mbuf *mbuf,
                        void *iq_data_start,
                        uint16_t size,
                        uint8_t CC_ID,
                        uint8_t Ant_ID,
                        uint8_t frame_id,
                        uint8_t subframe_id,
                        uint8_t slot_id,
                        uint8_t symb_id,
                        struct ecpri_seq_id *seq_id,
                        uint16_t num_prbu,
                        uint16_t start_prbu,
                        uint16_t sym_inc,
                        uint16_t rb,
                        uint16_t sect_id)
{
    struct xran_device_ctx * pctx = xran_dev_get_ctx();
    struct xran_common_counters *pCnt = &pctx->fh_counters;

    if(pctx->fh_init.io_cfg.id == O_DU) {
        if(xran_check_upul_seqid(NULL, CC_ID, Ant_ID, slot_id, seq_id->seq_id) != XRAN_STATUS_SUCCESS) {
            pCnt->Rx_pkt_dupl++;
            return (XRAN_STATUS_FAIL);
        }
    }else if(pctx->fh_init.io_cfg.id == O_RU) {
        if(xran_check_updl_seqid(NULL, CC_ID, Ant_ID, slot_id, seq_id->seq_id) != XRAN_STATUS_SUCCESS) {
            pCnt->Rx_pkt_dupl++;
            return (XRAN_STATUS_FAIL);
        }
    }else {
        print_err("incorrect dev type %d\n", pctx->fh_init.io_cfg.id);
    }

    rx_counter++;

    pCnt->Rx_on_time++;
    pCnt->Total_msgs_rcvd++;

    return XRAN_STATUS_SUCCESS;
}

int xran_process_rx_sym(void *arg,
                        struct rte_mbuf *mbuf,
                        void *iq_data_start,
                        uint16_t size,
                        uint8_t CC_ID,
                        uint8_t Ant_ID,
                        uint8_t frame_id,
                        uint8_t subframe_id,
                        uint8_t slot_id,
                        uint8_t symb_id,
                        uint16_t num_prbu,
                        uint16_t start_prbu,
                        uint16_t sym_inc,
                        uint16_t rb,
                        uint16_t sect_id,
                        uint32_t *mb_free)
{
    char        *pos = NULL;
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    uint32_t tti = 0;
    xran_status_t status;
    void *pHandle = NULL;
    struct rte_mbuf *mb = NULL;

    uint16_t iq_sample_size_bits = 16;

    tti = frame_id * SLOTS_PER_SYSTEMFRAME + subframe_id * SLOTNUM_PER_SUBFRAME + slot_id;

    status = tti << 16 | symb_id;

    if(tti < xran_fs_get_max_slot() && CC_ID < XRAN_MAX_SECTOR_NR && Ant_ID < XRAN_MAX_ANTENNA_NR && symb_id < XRAN_NUM_OF_SYMBOL_PER_SLOT){
        pos = (char*) p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id].pData;
        pos += start_prbu * N_SC_PER_PRB*(iq_sample_size_bits/8)*2;
        if(pos && iq_data_start && size){
            if (p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder == XRAN_CPU_LE_BYTE_ORDER) {
                int idx = 0;
                uint16_t *psrc = (uint16_t *)iq_data_start;
                uint16_t *pdst = (uint16_t *)pos;
                rte_panic("XRAN_CPU_LE_BYTE_ORDER is not supported 0x16%lx\n", (long)mb);
                /* network byte (be) order of IQ to CPU byte order (le) */
                for (idx = 0; idx < size/sizeof(int16_t); idx++){
                    pdst[idx]  = (psrc[idx]>>8) | (psrc[idx]<<8); //rte_be_to_cpu_16(psrc[idx]);
                }
            } else if (likely(p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder == XRAN_NE_BE_BYTE_ORDER)){
                if (likely (p_xran_dev_ctx->fh_init.mtu >=
                              p_xran_dev_ctx->fh_cfg.nULRBs * N_SC_PER_PRB*(iq_sample_size_bits/8)*2)) {
                    /* no fragmentation */
                    mb = p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id].pCtrl;
                    if(mb){
                       rte_pktmbuf_free(mb);
                    }else{
                       print_err("mb==NULL\n");
                    }
                    p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id].pData = iq_data_start;
                    p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id].pCtrl = mbuf;
                    *mb_free = MBUF_KEEP;
                } else {
                    /* packet can be fragmented copy RBs */
                    rte_memcpy(pos, iq_data_start, size);
                    *mb_free = MBUF_FREE;
                }
            }
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

    return size;
}

/* Send burst of packets on an output interface */
static inline int
xran_send_burst(struct xran_device_ctx *dev, uint16_t n, uint16_t port)
{
    struct rte_mbuf **m_table;
    struct rte_mbuf *m;
    int32_t i   = 0;
    int j;
    int32_t ret = 0;

    m_table = (struct rte_mbuf **)dev->tx_mbufs[port].m_table;

    for(i = 0; i < n; i++){
        rte_mbuf_sanity_check(m_table[i], 0);
        /*rte_pktmbuf_dump(stdout, m_table[i], 256);*/
        tx_counter++;
        ret += xran_ethdi_mbuf_send(m_table[i], ETHER_TYPE_ECPRI);
    }


    if (unlikely(ret < n)) {
        print_err("ret < n\n");
    }

    return 0;
}


int xran_process_tx_sym(void *arg)
{
    uint32_t    tti=0;
#if XRAN_MLOG_VAR
    uint32_t    mlogVar[10];
    uint32_t    mlogVarCnt = 0;
#endif
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
    void        *mb  = NULL;
    int         prb_num = 0;
    uint16_t    iq_sample_size_bits = 16; // TODO: make dynamic per

    struct xran_section_info *sectinfo;
    uint32_t next;
    uint32_t num_sections;
    uint8_t ctx_id;

    enum xran_pkt_dir  direction;

    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    struct xran_timer_ctx *pTCtx = (struct xran_timer_ctx *)arg;

    if(p_xran_dev_ctx->xran2phy_mem_ready == 0)
        return 0;

    if(p_xran_dev_ctx->fh_init.io_cfg.id == O_DU) {
        direction = XRAN_DIR_DL; /* lls-CU */
        prb_num = p_xran_dev_ctx->fh_cfg.nDLRBs;
    } else {
        direction = XRAN_DIR_UL; /* RU */
        prb_num = p_xran_dev_ctx->fh_cfg.nULRBs;
    }

    /* RU: send symb after OTA time with delay (UL) */
    /* lls-CU:send symb in advance of OTA time (DL) */
    sym_idx     = XranOffsetSym(p_xran_dev_ctx->sym_up, xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT*SLOTNUM_PER_SUBFRAME*1000);

    tti         = XranGetTtiNum(sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
    slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME);
    subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME,  SUBFRAMES_PER_SYSTEMFRAME);
    frame_id    = XranGetFrameNum(tti,SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME);
    sym_id      = XranGetSymNum(sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
    ctx_id      = XranGetSlotNum(tti, SLOTS_PER_SYSTEMFRAME) % XRAN_MAX_SECTIONDB_CTX;

    print_dbg("[%d]SFN %d sf %d slot %d\n", tti, frame_id, subframe_id, slot_id);

#if XRAN_MLOG_VAR
    mlogVar[mlogVarCnt++] = 0xAAAAAAAA;
    mlogVar[mlogVarCnt++] = xran_lib_ota_sym_idx;
    mlogVar[mlogVarCnt++] = sym_idx;
    mlogVar[mlogVarCnt++] = abs(p_xran_dev_ctx->sym_up);
    mlogVar[mlogVarCnt++] = tti;
    mlogVar[mlogVarCnt++] = frame_id;
    mlogVar[mlogVarCnt++] = subframe_id;
    mlogVar[mlogVarCnt++] = slot_id;
    mlogVar[mlogVarCnt++] = sym_id;
    MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());
#endif

    if(frame_id > 99) {
        print_err("OTA %d: TX:[sym_idx %d: TTI %d] fr %d sf %d slot %d sym %d\n",xran_lib_ota_sym_idx,  sym_idx, tti, frame_id, subframe_id, slot_id, sym_id);
        xran_if_current_state = XRAN_STOPPED;
    }

    num_eAxc    = xran_get_num_eAxc(pHandle);
    num_CCPorts = xran_get_num_cc(pHandle);

    /* U-Plane */
    for(ant_id = 0; ant_id < num_eAxc; ant_id++) {
        for(cc_id = 0; cc_id < num_CCPorts; cc_id++) {
            if(p_xran_dev_ctx->fh_init.io_cfg.id == O_DU
                    && p_xran_dev_ctx->enableCP) {
                /*==== lls-CU and C-Plane has been enabled ===*/
                next = 0;
                num_sections = xran_cp_getsize_section_info(pHandle, direction, cc_id, ant_id, ctx_id);
                /* iterate C-Plane configuration to generate corresponding U-Plane */
                while(next < num_sections) {
                    sectinfo = xran_cp_iterate_section_info(pHandle, direction, cc_id, ant_id, ctx_id, &next);

                    if(sectinfo == NULL)
                        break;

                    if(sectinfo->type != XRAN_CP_SECTIONTYPE_1) {   /* only supports type 1 */
                        print_err("Invalid section type in section DB - %d", sectinfo->type);
                        continue;
                    }

                    /* skip, if not scheduled */
                    if(sym_id < sectinfo->startSymId || sym_id >= sectinfo->startSymId + sectinfo->numSymbol)
                        continue;

                   /* if(sectinfo->compMeth)
                        iq_sample_size_bits = sectinfo->iqWidth;*/

                    if(iq_sample_size_bits != 16) {/* TODO: support for compression */
                        print_err("Incorrect iqWidth %d", iq_sample_size_bits);
                        iq_sample_size_bits = 16;
                    }

                    print_dbg(">>> sym %2d [%d] type%d, id %d, startPrbc=%d, numPrbc=%d, numSymbol=%d\n", sym_id, next,
                                    sectinfo->type, sectinfo->id, sectinfo->startPrbc,
                                    sectinfo->numPrbc, sectinfo->numSymbol);

                    p_xran_dev_ctx->tx_mbufs[0].len = 0;
                    uint16_t len  = p_xran_dev_ctx->tx_mbufs[0].len;
                    int16_t len2 = 0;
                    uint16_t i    = 0;

                    //Added for Klocworks
                    if (len >= MBUF_TABLE_SIZE)
                        len = MBUF_TABLE_SIZE - 1;

                    pos = (char*) p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id].pData;
                    mb  = p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id].pCtrl;

                    /* first all PRBs */
                    prepare_symbol_ex(direction, sectinfo->id,
                                      mb,
                                      (struct rb_map *)pos,
                                      p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder,
                                      frame_id, subframe_id, slot_id, sym_id,
                                      sectinfo->startPrbc, sectinfo->numPrbc,
                                      cc_id, ant_id,
                                      xran_get_updl_seqid(pHandle, cc_id, ant_id),
                                      0);

                    /* if we don't need to do any fragmentation */
                    if (likely (p_xran_dev_ctx->fh_init.mtu >=
                                    sectinfo->numPrbc * N_SC_PER_PRB*(iq_sample_size_bits/8)*2)) {
                        /* no fragmentation */
                        p_xran_dev_ctx->tx_mbufs[0].m_table[len] = mb;
                        len2 = 1;
                    } else {
                        /* fragmentation */
                        len2 = xran_app_fragment_packet(mb,
                                                    &p_xran_dev_ctx->tx_mbufs[0].m_table[len],
                                                    (uint16_t)(MBUF_TABLE_SIZE - len),
                                                    p_xran_dev_ctx->fh_init.mtu,
                                                    p_xran_dev_ctx->direct_pool,
                                                    p_xran_dev_ctx->indirect_pool,
                                                    sectinfo,
                                                    xran_get_updl_seqid_addr(pHandle, cc_id, ant_id));

                        /* Free input packet */
                        rte_pktmbuf_free(mb);

                        /* If we fail to fragment the packet */
                        if (unlikely (len2 < 0)){
                            print_err("len2= %d\n", len2);
                            return 0;
                        }
                    }

                    if(len2 > 1){
                        for (i = len; i < len + len2; i ++) {
                            struct rte_mbuf *m;
                            m = p_xran_dev_ctx->tx_mbufs[0].m_table[i];
                            struct ether_hdr *eth_hdr = (struct ether_hdr *)
                                rte_pktmbuf_prepend(m, (uint16_t)sizeof(struct ether_hdr));
                            if (eth_hdr == NULL) {
                                rte_panic("No headroom in mbuf.\n");
                            }
                        }
                    }

                    len += len2;

                    if (unlikely(len > XRAN_MAX_PKT_BURST_PER_SYM)) {
                          rte_panic("XRAN_MAX_PKT_BURST_PER_SYM\n");
                    }

                    /* Transmit packets */
                    xran_send_burst(p_xran_dev_ctx, (uint16_t)len, 0);
                    p_xran_dev_ctx->tx_mbufs[0].len = 0;
                } /* while(section) */

            }
            else {
                /*==== RU or C-Plane is disabled ===*/
                xRANPrachCPConfigStruct *pPrachCPConfig = &(p_xran_dev_ctx->PrachCPConfig);

                if(xran_fs_get_slot_type(cc_id, tti, ((p_xran_dev_ctx->fh_init.io_cfg.id == O_DU)? XRAN_SLOT_TYPE_DL : XRAN_SLOT_TYPE_UL)) ==  1
                        || xran_fs_get_slot_type(cc_id, tti, XRAN_SLOT_TYPE_SP) ==  1
                        || xran_fs_get_slot_type(cc_id, tti, XRAN_SLOT_TYPE_FDD) ==  1){

                    if(xran_fs_get_symbol_type(cc_id, tti, sym_id) == ((p_xran_dev_ctx->fh_init.io_cfg.id == O_DU)? XRAN_SYMBOL_TYPE_DL : XRAN_SYMBOL_TYPE_UL)
                       || xran_fs_get_symbol_type(cc_id, tti, sym_id) == XRAN_SYMBOL_TYPE_FDD){

                        if(iq_sample_size_bits != 16)
                            print_err("Incorrect iqWidth %d\n", iq_sample_size_bits );

                        pos = (char*) p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id].pData;
                        mb  = (void*) p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id].pCtrl;

                        if( prb_num > 136 || prb_num == 0) {
                            uint16_t sec_id  = xran_alloc_sectionid(pHandle, direction, cc_id, ant_id, slot_id);
                            /* first 136 PRBs */
                            send_symbol_ex(direction,
                                            sec_id,
                                            NULL,
                                            (struct rb_map *)pos,
                                            p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder,
                                            frame_id, subframe_id, slot_id, sym_id,
                                            0, 136,
                                            cc_id, ant_id,
                                            (p_xran_dev_ctx->fh_init.io_cfg.id == O_DU) ?
                                                xran_get_updl_seqid(pHandle, cc_id, ant_id) :
                                                xran_get_upul_seqid(pHandle, cc_id, ant_id));

                             pos += 136 * N_SC_PER_PRB * (iq_sample_size_bits/8)*2;
                             /* last 137 PRBs */
                             send_symbol_ex(direction, sec_id,
                                             NULL,
                                             (struct rb_map *)pos,
                                             p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder,
                                             frame_id, subframe_id, slot_id, sym_id,
                                             136, 137,
                                             cc_id, ant_id,
                                             (p_xran_dev_ctx->fh_init.io_cfg.id == O_DU) ?
                                                xran_get_updl_seqid(pHandle, cc_id, ant_id) :
                                                xran_get_upul_seqid(pHandle,  cc_id, ant_id));
                        } else {
#ifdef DEBUG_XRAN_BUFFERS
                            if (pos[0] != tti % XRAN_N_FE_BUF_LEN ||
                                pos[1] != cc_id ||
                                pos[2] != ant_id ||
                                pos[3] != sym_id)
                                    printf("%d %d %d %d\n", pos[0], pos[1], pos[2], pos[3]);
#endif
                            send_symbol_ex(direction,
                                    xran_alloc_sectionid(pHandle, direction, cc_id, ant_id, slot_id),
                                    (struct rte_mbuf *)mb,
                                    (struct rb_map *)pos,
                                    p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder,
                                    frame_id, subframe_id, slot_id, sym_id,
                                    0, prb_num,
                                    cc_id, ant_id,
                                    (p_xran_dev_ctx->fh_init.io_cfg.id == O_DU) ?
                                        xran_get_updl_seqid(pHandle, cc_id, ant_id) :
                                        xran_get_upul_seqid(pHandle, cc_id, ant_id));
                        }

                        if(p_xran_dev_ctx->enablePrach
                                && (p_xran_dev_ctx->fh_init.io_cfg.id == O_RU)) {   /* Only RU needs to send PRACH I/Q */
                            uint32_t isPRACHslot = xran_isPRACHSlot(subframe_id, slot_id);
                            if((frame_id % pPrachCPConfig->x == pPrachCPConfig->y[0])
                                    && (isPRACHslot == 1)
                                    && (sym_id >= p_xran_dev_ctx->prach_start_symbol[cc_id])
                                    && (sym_id <= p_xran_dev_ctx->prach_last_symbol[cc_id])) {  //is prach slot
                                for(ant_id = 0; ant_id < num_eAxc; ant_id++) {
                                    int prach_port_id = ant_id + num_eAxc;
                                    pos = (char*) p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[0].pData;
                                    pos += (sym_id - p_xran_dev_ctx->prach_start_symbol[cc_id]) * pPrachCPConfig->numPrbc * N_SC_PER_PRB * 4;
                                    mb  = NULL;//(void*) p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[0].pCtrl;
                                    send_symbol_ex(direction,
                                            xran_alloc_sectionid(pHandle, direction, cc_id, prach_port_id, slot_id),
                                            (struct rte_mbuf *)mb,
                                            (struct rb_map *)pos,
                                            p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder,
                                            frame_id, subframe_id, slot_id, sym_id,
                                            pPrachCPConfig->startPrbc, pPrachCPConfig->numPrbc,
                                            cc_id, prach_port_id,
                                            xran_get_upul_seqid(pHandle, cc_id, prach_port_id));
                                }
                            } /* if((frame_id % pPrachCPConfig->x == pPrachCPConfig->y[0]) .... */
                        } /* if(p_xran_dev_ctx->enablePrach ..... */

                    } /* RU mode or C-Plane is not used */
                }
            }
        } /* for(cc_id = 0; cc_id < num_CCPorts; cc_id++) */
    } /* for(ant_id = 0; ant_id < num_eAxc; ant_id++) */

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


int32_t
xran_init(int argc, char *argv[],
           struct xran_fh_init *p_xran_fh_init, char *appName, void ** pXranLayerHandle)
{
    int32_t i;
    int32_t j;

    struct xran_io_loop_cfg *p_io_cfg = (struct xran_io_loop_cfg *)&p_xran_fh_init->io_cfg;
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();

    int32_t  lcore_id = 0;
    char filename[64];

    memset(p_xran_dev_ctx, 0, sizeof(struct xran_device_ctx));

    /* copy init */
    p_xran_dev_ctx->fh_init = *p_xran_fh_init;

    printf(" %s: MTU %d\n", __FUNCTION__, p_xran_dev_ctx->fh_init.mtu);

    xran_if_current_state = XRAN_INIT;

    memcpy(&(p_xran_dev_ctx->eAxc_id_cfg), &(p_xran_fh_init->eAxCId_conf), sizeof(struct xran_eaxcid_config));

    p_xran_dev_ctx->enableCP    = p_xran_fh_init->enableCP;
    p_xran_dev_ctx->enablePrach = p_xran_fh_init->prachEnable;
    p_xran_dev_ctx->DynamicSectionEna = p_xran_fh_init->DynamicSectionEna;

    xran_register_ethertype_handler(ETHER_TYPE_ECPRI, handle_ecpri_ethertype);
    if (p_io_cfg->id == 0)
        xran_ethdi_init_dpdk_io(p_xran_fh_init->filePrefix,
                           p_io_cfg,
                           &lcore_id,
                           (struct ether_addr *)p_xran_fh_init->p_o_du_addr,
                           (struct ether_addr *)p_xran_fh_init->p_o_ru_addr,
                           p_xran_fh_init->cp_vlan_tag,
                           p_xran_fh_init->up_vlan_tag);
    else
        xran_ethdi_init_dpdk_io(p_xran_fh_init->filePrefix,
                           p_io_cfg,
                           &lcore_id,
                           (struct ether_addr *)p_xran_fh_init->p_o_ru_addr,
                           (struct ether_addr *)p_xran_fh_init->p_o_du_addr,
                           p_xran_fh_init->cp_vlan_tag,
                           p_xran_fh_init->up_vlan_tag);

    for(i = 0; i < 10; i++ )
        rte_timer_init(&tti_to_phy_timer[i]);

    rte_timer_init(&sym_timer);
    for (i = 0; i< MAX_NUM_OF_DPDK_TIMERS; i++)
        rte_timer_init(&dpdk_timer[i]);

    p_xran_dev_ctx->direct_pool   = socket_direct_pool;
    p_xran_dev_ctx->indirect_pool = socket_indirect_pool;

    printf("Set debug stop %d, debug stop count %d\n", p_xran_fh_init->debugStop, p_xran_fh_init->debugStopCount);
    timing_set_debug_stop(p_xran_fh_init->debugStop, p_xran_fh_init->debugStopCount);

    for (uint32_t nCellIdx = 0; nCellIdx < XRAN_MAX_SECTOR_NR; nCellIdx++){
        xran_fs_clear_slot_type(nCellIdx);
    }

    *pXranLayerHandle = p_xran_dev_ctx;

    return 0;
}

int32_t xran_sector_get_instances (void * pDevHandle, uint16_t nNumInstances,
               xran_cc_handle_t * pSectorInstanceHandles)
{
    xran_status_t nStatus = XRAN_STATUS_FAIL;
    struct xran_device_ctx *pDev = (struct xran_device_ctx *)pDevHandle;
    XranSectorHandleInfo *pCcHandle = NULL;
    int32_t i = 0;

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

int32_t xran_mm_init (void * pHandle, uint64_t nMemorySize,
            uint32_t nMemorySegmentSize)
{
    /* we use mbuf from dpdk memory */
    return 0;
}

int32_t xran_bm_init (void * pHandle, uint32_t * pPoolIndex, uint32_t nNumberOfBuffers, uint32_t nBufferSize)
{
    XranSectorHandleInfo* pXranCc = (XranSectorHandleInfo*) pHandle;
    uint32_t nAllocBufferSize;

    char pool_name[RTE_MEMPOOL_NAMESIZE];

    snprintf(pool_name, RTE_MEMPOOL_NAMESIZE, "ru_%d_cc_%d_idx_%d",
        pXranCc->nXranPort, pXranCc->nIndex, pXranCc->nBufferPoolIndex);

    nAllocBufferSize = nBufferSize + sizeof(struct ether_hdr) +
        sizeof (struct xran_ecpri_hdr) +
        sizeof (struct radio_app_common_hdr) +
        sizeof(struct data_section_hdr) + 256;


    printf("%s: [ handle %p %d %d ] [nPoolIndex %d] nNumberOfBuffers %d nBufferSize %d\n", pool_name,
                        pXranCc, pXranCc->nXranPort, pXranCc->nIndex, pXranCc->nBufferPoolIndex, nNumberOfBuffers, nBufferSize);

    pXranCc->p_bufferPool[pXranCc->nBufferPoolIndex] = rte_pktmbuf_pool_create(pool_name, nNumberOfBuffers,
                                                                               MBUF_CACHE, 0, nAllocBufferSize, rte_socket_id());

    if(pXranCc->p_bufferPool[pXranCc->nBufferPoolIndex] == NULL){
        rte_panic("rte_pktmbuf_pool_create failed [ handle %p %d %d ] [nPoolIndex %d] nNumberOfBuffers %d nBufferSize %d errno %s\n",
                    pXranCc, pXranCc->nXranPort, pXranCc->nIndex, pXranCc->nBufferPoolIndex, nNumberOfBuffers, nBufferSize, rte_strerror(rte_errno));
        return -1;
    }

    pXranCc->bufferPoolElmSz[pXranCc->nBufferPoolIndex]  = nBufferSize;
    pXranCc->bufferPoolNumElm[pXranCc->nBufferPoolIndex] = nNumberOfBuffers;

    printf("CC:[ handle %p ru %d cc_idx %d ] [nPoolIndex %d] mb pool %p \n",
                pXranCc, pXranCc->nXranPort, pXranCc->nIndex,
                    pXranCc->nBufferPoolIndex,  pXranCc->p_bufferPool[pXranCc->nBufferPoolIndex]);

    *pPoolIndex = pXranCc->nBufferPoolIndex++;

    return 0;
}

int32_t xran_bm_allocate_buffer(void * pHandle, uint32_t nPoolIndex, void **ppData,  void **ppCtrl)
{
    XranSectorHandleInfo* pXranCc = (XranSectorHandleInfo*) pHandle;
    *ppData = NULL;
    *ppCtrl = NULL;

    struct rte_mbuf * mb =  rte_pktmbuf_alloc(pXranCc->p_bufferPool[nPoolIndex]);

    if(mb){
        char * start     = rte_pktmbuf_append(mb, pXranCc->bufferPoolElmSz[nPoolIndex]);
        char * ethhdr    = rte_pktmbuf_prepend(mb, sizeof(struct ether_hdr));

        if(start && ethhdr){
            char * iq_offset = rte_pktmbuf_mtod(mb, char * );
            /* skip headers */
            iq_offset = iq_offset + sizeof(struct ether_hdr) +
                                    sizeof (struct xran_ecpri_hdr) +
                                    sizeof (struct radio_app_common_hdr) +
                                    sizeof(struct data_section_hdr);

            if (0) /* if compression */
                iq_offset += sizeof (struct data_section_compression_hdr);

            *ppData = (void *)iq_offset;
            *ppCtrl  = (void *)mb;
        }
        else {
            print_err("[nPoolIndex %d] start ethhdr failed \n", nPoolIndex );
            return -1;
        }
    }else {
        print_err("[nPoolIndex %d] mb alloc failed \n", nPoolIndex );
        return -1;
    }

    if (*ppData ==  NULL){
        print_err("[nPoolIndex %d] rte_pktmbuf_append for %d failed \n", nPoolIndex, pXranCc->bufferPoolElmSz[nPoolIndex]);
        return -1;
    }

    return 0;
}

int32_t xran_bm_free_buffer(void * pHandle, void *pData, void *pCtrl)
{
    XranSectorHandleInfo* pXranCc = (XranSectorHandleInfo*) pHandle;

    if(pCtrl)
        rte_pktmbuf_free(pCtrl);

    return 0;
}

int32_t xran_5g_fronthault_config (void * pHandle,
                struct xran_buffer_list *pSrcBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pSrcCpBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pDstBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pDstCpBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                xran_transport_callback_fn pCallback,
                void *pCallbackTag)
{
    XranSectorHandleInfo* pXranCc = (XranSectorHandleInfo*) pHandle;
    xran_status_t nStatus = XRAN_STATUS_SUCCESS;
    int j, i = 0, z, k;
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();

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

    i = pXranCc->nIndex;

    for(j=0; j<XRAN_N_FE_BUF_LEN; j++)
    {
        for(z = 0; z < XRAN_MAX_ANTENNA_NR; z++){
            /* U-plane TX */

            p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].bValid = 0;
            p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
            p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
            p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
            p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
            p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &p_xran_dev_ctx->sFrontHaulTxBuffers[j][i][z][0];

            p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList =   *pSrcBuffer[z][j];

            /* C-plane TX */
            p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].bValid = 0;
            p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
            p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
            p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
            p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
            p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &p_xran_dev_ctx->sFrontHaulTxPrbMapBuffers[j][i][z][0];

            p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList =   *pSrcCpBuffer[z][j];

            /* U-plane RX */

            p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].bValid = 0;
            p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
            p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
            p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
            p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
            p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &p_xran_dev_ctx->sFrontHaulRxBuffers[j][i][z][0];

            p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList =   *pDstBuffer[z][j];

            /* C-plane RX */
            p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].bValid = 0;
            p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
            p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
            p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
            p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
            p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &p_xran_dev_ctx->sFrontHaulRxPrbMapBuffers[j][i][z][0];

            p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList =   *pDstCpBuffer[z][j];
        }
    }

#if 0
    for(j=0; j<XRAN_N_FE_BUF_LEN; j++)
        for(z = 0; z < XRAN_MAX_ANTENNA_NR; z++){
            printf("TTI:TX 0x%02x Sec %d Ant%d\n",j,i,z);
            for(k = 0; k <XRAN_NUM_OF_SYMBOL_PER_SLOT; k++){
                uint8_t *ptr = p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pData;
                printf("    sym: %2d %p 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", k, ptr, ptr[0],ptr[1], ptr[2], ptr[3], ptr[4]);
            }
        }

    for(j=0; j<XRAN_N_FE_BUF_LEN; j++)
        for(z = 0; z < XRAN_MAX_ANTENNA_NR; z++){
            printf("TTI:RX 0x%02x Sec %d Ant%d\n",j,i,z);
            for(k = 0; k <XRAN_NUM_OF_SYMBOL_PER_SLOT; k++){
                uint8_t *ptr = p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pData;
                printf("    sym: %2d %p 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", k, ptr, ptr[0],ptr[1], ptr[2], ptr[3], ptr[4]);
            }
        }
#endif

    p_xran_dev_ctx->pCallback[i]    = pCallback;
    p_xran_dev_ctx->pCallbackTag[i] = pCallbackTag;

    p_xran_dev_ctx->xran2phy_mem_ready = 1;

    return nStatus;
}

int32_t xran_5g_prach_req (void *  pHandle,
                struct xran_buffer_list *pDstBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                xran_transport_callback_fn pCallback,
                void *pCallbackTag)
{
    XranSectorHandleInfo* pXranCc = (XranSectorHandleInfo*) pHandle;
    xran_status_t nStatus = XRAN_STATUS_SUCCESS;
    int j, i = 0, z;
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();

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

    i = pXranCc->nIndex;

    for(j=0; j<XRAN_N_FE_BUF_LEN; j++)
    {
        for(z = 0; z < XRAN_MAX_ANTENNA_NR; z++){
           p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].bValid = 0;
           p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
           p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
           p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
           p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_MAX_ANTENNA_NR; // ant number.
           p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &p_xran_dev_ctx->sFHPrachRxBuffers[j][i][z][0];
           p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList =   *pDstBuffer[z][j];
        }
    }

    p_xran_dev_ctx->pPrachCallback[i]    = pCallback;
    p_xran_dev_ctx->pPrachCallbackTag[i] = pCallbackTag;

    return 0;
}

int32_t xran_open(void *pHandle, struct xran_fh_config* pConf)
{
    int32_t i;
    uint8_t nNumerology = 0;
    int32_t  lcore_id = 0;
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    struct xran_fh_config *pFhCfg;
    pFhCfg = &(p_xran_dev_ctx->fh_cfg);

    memcpy(pFhCfg, pConf, sizeof(struct xran_fh_config));

    nNumerology = xran_get_conf_numerology(pHandle);

    if (pConf->nCC > XRAN_MAX_SECTOR_NR)
    {
        if(pConf->log_level)
            printf("Number of cells %d exceeds max number supported %d!\n", pConf->nCC, XRAN_MAX_SECTOR_NR);
        pConf->nCC = XRAN_MAX_SECTOR_NR;

    }
    if(pConf->ru_conf.iqOrder != XRAN_I_Q_ORDER
        || pConf->ru_conf.byteOrder != XRAN_NE_BE_BYTE_ORDER ){

        print_err("Byte order and/or IQ order is not suppirted [IQ %d byte %d]\n", pConf->ru_conf.iqOrder, pConf->ru_conf.byteOrder);
        return XRAN_STATUS_FAIL;
    }

    //setup PRACH configuration for C-Plane
    xran_init_prach(pConf, p_xran_dev_ctx);

    xran_cp_init_sectiondb(pHandle);
    xran_init_sectionid(pHandle);
    xran_init_seqid(pHandle);

    interval_us = xran_fs_get_tti_interval(nNumerology);

    if(pConf->log_level){
        printf("%s: interval_us=%ld\n", __FUNCTION__, interval_us);
    }
    timing_set_numerology(nNumerology);

    for(i = 0 ; i <pConf->nCC; i++){
        xran_fs_set_slot_type(i, pConf->frame_conf.nFrameDuplexType, pConf->frame_conf.nTddPeriod,
            pConf->frame_conf.sSlotConfig);
    }

    xran_fs_slot_limit_init(xran_fs_get_tti_interval(nNumerology));

    if(xran_ethdi_get_ctx()->io_cfg.bbdev_mode != XRAN_BBDEV_NOT_USED){
        p_xran_dev_ctx->bbdev_dec = pConf->bbdev_dec;
        p_xran_dev_ctx->bbdev_enc = pConf->bbdev_enc;
    }

    /* Start packet processing thread */
    if((uint16_t)xran_ethdi_get_ctx()->io_cfg.port[XRAN_UP_VF] != 0xFFFF &&
        (uint16_t)xran_ethdi_get_ctx()->io_cfg.port[XRAN_CP_VF] != 0xFFFF ){
        if(pConf->log_level){
            print_dbg("XRAN_UP_VF: 0x%04x\n", xran_ethdi_get_ctx()->io_cfg.port[XRAN_UP_VF]);
            print_dbg("XRAN_CP_VF: 0x%04x\n", xran_ethdi_get_ctx()->io_cfg.port[XRAN_CP_VF]);
        }
        if (rte_eal_remote_launch(xran_timing_source_thread, xran_dev_get_ctx(), xran_ethdi_get_ctx()->io_cfg.timing_core))
            rte_panic("thread_run() failed to start\n");
    } else
        if(pConf->log_level){
            printf("Eth port was not open. Processing thread was not started\n");
        }



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
    //TODO: fix memory leak xran_cp_free_sectiondb(pHandle);
    //rte_eal_mp_wait_lcore();
    //xran_ethdi_ports_stats();

    return 0;
}

int32_t xran_mm_destroy (void * pHandle)
{
    /* functionality is not yet implemented */
    return -1;
}

int32_t xran_reg_sym_cb(void *pHandle, xran_callback_sym_fn symCb, void * symCbParam, uint8_t symb,  uint8_t ant)
{
    /* functionality is not yet implemented */
    return -1;
}

int32_t xran_reg_physide_cb(void *pHandle, xran_fh_tti_callback_fn Cb, void *cbParam, int skipTtiNum, enum callback_to_phy_id id)
{
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();

    p_xran_dev_ctx->ttiCb[id]      = Cb;
    p_xran_dev_ctx->TtiCbParam[id] = cbParam;
    p_xran_dev_ctx->SkipTti[id]    = skipTtiNum;

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
 * @brief Get the configuration of eAxC ID
 *
 * @return the pointer of configuration
 */
inline struct xran_eaxcid_config *xran_get_conf_eAxC(void *pHandle)
{
    return (&(xran_dev_get_ctx()->eAxc_id_cfg));
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
 * @return Configured numerology
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
    struct xran_fh_config *pFhCfg;

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
 * @brief Get the configuration of the number of component carriers
 *
 * @return Configured the number of component carriers
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

int32_t xran_get_common_counters(void *pXranLayerHandle, struct xran_common_counters *pStats)
{
    struct xran_device_ctx* pDev = (struct xran_device_ctx*)pXranLayerHandle;

    if(pStats && pDev) {
        *pStats  =  pDev->fh_counters;
        return XRAN_STATUS_SUCCESS;
    } else {
        return XRAN_STATUS_INVALID_PARAM;
    }
}

