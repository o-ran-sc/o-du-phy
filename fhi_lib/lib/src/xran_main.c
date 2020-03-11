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
#include <sys/queue.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <malloc.h>

#include <rte_common.h>
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
#define DpdkTimerIncrementCtx(ctx)           ((ctx >= (MAX_NUM_OF_DPDK_TIMERS-1)) ? 0 : (ctx+1))
#define DpdkTimerDecrementCtx(ctx)           ((ctx == 0) ? (MAX_NUM_OF_DPDK_TIMERS-1) : (ctx-1))

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
#define NUM_OF_FRAMES_PER_SECOND 100

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

uint64_t interval_us = 1000;

uint32_t xran_lib_ota_tti        = 0; /**< Slot index in a second [0:(1000000/TTI-1)] */
uint32_t xran_lib_ota_sym        = 0; /**< Symbol index in a slot [0:13] */
uint32_t xran_lib_ota_sym_idx    = 0; /**< Symbol index in a second [0 : 14*(1000000/TTI)-1]
                                                where TTI is TTI interval in microseconds */
uint16_t xran_SFN_at_Sec_Start   = 0; /**< SFN at current second start */
uint16_t xran_max_frame          = 1023; /**< value of max frame used. expected to be 99 (old compatibility mode) and 1023 as per section 9.7.2	System Frame Number Calculation */

static uint8_t xran_cp_seq_id_num[XRAN_MAX_CELLS_PER_PORT][XRAN_DIR_MAX][XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR]; /* XRAN_MAX_ANTENNA_NR * 2 for PUSCH and PRACH */
static uint8_t xran_updl_seq_id_num[XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR];
static uint8_t xran_upul_seq_id_num[XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR]; /**< PUSCH, PRACH, SRS for Cat B */

static uint8_t xran_section_id_curslot[XRAN_DIR_MAX][XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR * 2+ XRAN_MAX_ANT_ARRAY_ELM_NR];
static uint16_t xran_section_id[XRAN_DIR_MAX][XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR * 2+ XRAN_MAX_ANT_ARRAY_ELM_NR];
static uint64_t xran_total_tick = 0, xran_used_tick = 0;
static uint32_t xran_core_used = 0;
static int32_t first_call = 0;


static void
extbuf_free_callback(void *addr __rte_unused, void *opaque __rte_unused)
{
}

static struct rte_mbuf_ext_shared_info share_data[XRAN_N_FE_BUF_LEN];

void xran_timer_arm(struct rte_timer *tim, void* arg);

int32_t xran_process_tx_sym(void *arg);

int32_t xran_process_rx_sym(void *arg,
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

int32_t xran_process_prach_sym(void *arg,
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

int32_t xran_process_srs_sym(void *arg,
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


void tti_ota_cb(struct rte_timer *tim, void *arg);
void tti_to_phy_cb(struct rte_timer *tim, void *arg);
void xran_timer_arm_ex(struct rte_timer *tim, void* CbFct, void *CbArg, unsigned tim_lcore);

// Return SFN at current second start, 10 bits, [0, 1023]
static inline uint16_t xran_getSfnSecStart(void)
{
    return xran_SFN_at_Sec_Start;
}
void xran_updateSfnSecStart(void)
{
    uint64_t currentSecond = timing_get_current_second();
    // Assume always positive
    uint64_t gpsSecond = currentSecond - UNIX_TO_GPS_SECONDS_OFFSET;
    uint64_t nFrames = gpsSecond * NUM_OF_FRAMES_PER_SECOND;
    uint16_t sfn = (uint16_t)(nFrames % (xran_max_frame + 1));
    xran_SFN_at_Sec_Start = sfn;

    tx_bytes_per_sec = tx_bytes_counter;
    rx_bytes_per_sec = rx_bytes_counter;
    tx_bytes_counter = 0;
    rx_bytes_counter = 0;
}

static inline int32_t xran_getSlotIdxSecond(void)
{
    int32_t frameIdxSecond = xran_getSfnSecStart();
    int32_t slotIndxSecond = frameIdxSecond * SLOTS_PER_SYSTEMFRAME;
    return slotIndxSecond;
}

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

int xran_is_prach_slot(uint32_t subframe_id, uint32_t slot_id)
{
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    struct xran_prach_cp_config *pPrachCPConfig = &(p_xran_dev_ctx->PrachCPConfig);
    int32_t is_prach_slot = 0;

    if (p_xran_dev_ctx->fh_cfg.frame_conf.nNumerology < 2){
        //for FR1, in 38.211 tab 6.3.3.2-2&3 it is subframe index
        if (pPrachCPConfig->isPRACHslot[subframe_id] == 1){
            if (pPrachCPConfig->nrofPrachInSlot != 1)
                is_prach_slot = 1;
            else{
                if (p_xran_dev_ctx->fh_cfg.frame_conf.nNumerology == 0)
                    is_prach_slot = 1;
                else if (slot_id == 1)
                    is_prach_slot = 1;
            }
        }
    } else if (p_xran_dev_ctx->fh_cfg.frame_conf.nNumerology == 3){
        //for FR2, 38.211 tab 6.3.3.4 it is slot index of 60kHz slot
        uint32_t slotidx;
        slotidx = subframe_id * SLOTNUM_PER_SUBFRAME + slot_id;
        if (pPrachCPConfig->nrofPrachInSlot == 2){
            if (pPrachCPConfig->isPRACHslot[slotidx>>1] == 1)
                is_prach_slot = 1;
        } else {
            if ((pPrachCPConfig->isPRACHslot[slotidx>>1] == 1) && ((slotidx % 2) == 1)){
                is_prach_slot = 1;
            }
        }
    } else
        print_err("Numerology %d not supported", p_xran_dev_ctx->fh_cfg.frame_conf.nNumerology);
    return is_prach_slot;
}

int xran_init_sectionid(void *pHandle)
{
  int cell, ant, dir;

    for (dir = 0; dir < XRAN_DIR_MAX; dir++){
        for(cell=0; cell < XRAN_MAX_CELLS_PER_PORT; cell++) {
            for(ant=0; ant < XRAN_MAX_ANTENNA_NR; ant++) {
                xran_section_id[dir][cell][ant] = 0;
                xran_section_id_curslot[dir][cell][ant] = 255;
            }
        }
    }

    return (0);
}

int xran_init_srs(struct xran_fh_config* pConf, struct xran_device_ctx * p_xran_dev_ctx)
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


int xran_init_prach(struct xran_fh_config* pConf, struct xran_device_ctx * p_xran_dev_ctx)
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

    pPrachCPConfig->eAxC_offset = xran_get_num_eAxc(NULL);
    print_dbg("PRACH eAxC_offset %d\n",  pPrachCPConfig->eAxC_offset);

    return (XRAN_STATUS_SUCCESS);
}

inline uint16_t xran_alloc_sectionid(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id)
{
    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (0);
        }
    if(ant_id >= XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR) {  //for PRACH, ant_id starts from num_ant
        print_err("Invalid antenna ID - %d", ant_id);
        return (0);
    }

    /* if new slot has been started,
     * then initializes section id again for new start */
    if(xran_section_id_curslot[dir][cc_id][ant_id] != slot_id) {
        xran_section_id[dir][cc_id][ant_id] = 0;
        xran_section_id_curslot[dir][cc_id][ant_id] = slot_id;
    }

    return(xran_section_id[dir][cc_id][ant_id]++);
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
        for(ant=0; ant < XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR; ant++)
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
    if(ant_id >= XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR) {
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
        return (NULL);
    }
    if(ant_id >= XRAN_MAX_ANTENNA_NR) {
        print_err("Invalid antenna ID - %d", ant_id);
        return (NULL);
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

    if(ant_id >= XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR) {
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
    if(ant_id >= XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR) {
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
        /*print_dbg("ant %u  cc_id %u : slot_id %u : seq_id %u : expected seq_id %u\n",
            ant_id, cc_id, slot_id, seq_id, xran_updl_seq_id_num[cc_id][ant_id]);*/
        return (0);
    } else {
       /* print_err("ant %u  cc_id %u : slot_id %u : seq_id %u : expected seq_id %u\n",
            ant_id, cc_id, slot_id, seq_id, xran_updl_seq_id_num[cc_id][ant_id]);*/

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

void sym_ota_cb(struct rte_timer *tim, void *arg, unsigned long *used_tick)
{
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    struct xran_timer_ctx *pTCtx = (struct xran_timer_ctx *)arg;
    long t1 = MLogTick(), t2;
    long t3;
    static int32_t ctx = 0;

    if(XranGetSymNum(xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT) == 0){
        t3 = xran_tick();
        tti_ota_cb(NULL, arg);
        *used_tick += get_ticks_diff(xran_tick(), t3);
    }

    if(XranGetSymNum(xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT) == 3){
        if(p_xran_dev_ctx->phy_tti_cb_done == 0){
            /* rearm timer to deliver TTI event to PHY */
            t3 = xran_tick();
            p_xran_dev_ctx->phy_tti_cb_done = 0;
            xran_timer_arm_ex(&tti_to_phy_timer[xran_lib_ota_tti % 10], tti_to_phy_cb, (void*)pTCtx, p_xran_dev_ctx->fh_init.io_cfg.timing_core);
            *used_tick += get_ticks_diff(xran_tick(), t3);
        }
    }

    t3 = xran_tick();
    if (xran_process_tx_sym(timer_ctx))
    {
        *used_tick += get_ticks_diff(xran_tick(), t3);
    }

    /* check if there is call back to do something else on this symbol */

    struct cb_elem_entry *cb_elm;
    LIST_FOREACH(cb_elm, &p_xran_dev_ctx->sym_cb_list_head[0][xran_lib_ota_sym], pointers){
        if(cb_elm){
            cb_elm->pSymCallback(&dpdk_timer[ctx], cb_elm->pSymCallbackTag);
            ctx = DpdkTimerIncrementCtx(ctx);
        }
    }

    // This counter is incremented in advance before it is the time for the next symbol
    xran_lib_ota_sym++;
    if(xran_lib_ota_sym >= N_SYM_PER_SLOT){
        xran_lib_ota_sym=0;
    }

    t2 = MLogTick();
    MLogTask(PID_SYM_OTA_CB, t1, t2);
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
    uint32_t reg_sfn  = 0;
    struct xran_timer_ctx *pTCtx = (struct xran_timer_ctx *)arg;
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();

    MLogTask(PID_TTI_TIMER, t1, MLogTick());

    /* To match TTbox */
    if(xran_lib_ota_tti == 0)
        reg_tti = xran_fs_get_max_slot() - 1;
    else
        reg_tti = xran_lib_ota_tti -1;
    MLogIncrementCounter();
    reg_sfn    = XranGetFrameNum(reg_tti,xran_getSfnSecStart(),SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME)*10 + XranGetSubFrameNum(reg_tti,SLOTNUM_PER_SUBFRAME,  SUBFRAMES_PER_SYSTEMFRAME);;
    /* subframe and slot */
    MLogRegisterFrameSubframe(reg_sfn, reg_tti % (SLOTNUM_PER_SUBFRAME));
    MLogMark(1, t1);

    slot_id     = XranGetSlotNum(xran_lib_ota_tti, SLOTNUM_PER_SUBFRAME);
    subframe_id = XranGetSubFrameNum(xran_lib_ota_tti,SLOTNUM_PER_SUBFRAME,  SUBFRAMES_PER_SYSTEMFRAME);
    frame_id    = XranGetFrameNum(xran_lib_ota_tti,xran_getSfnSecStart(),SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME);

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
    frame_id    = XranGetFrameNum(next_tti,xran_getSfnSecStart(),SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME);

    print_dbg("[%d]SFN %d sf %d slot %d\n",next_tti, frame_id, subframe_id, slot_id);

    if(p_xran_dev_ctx->fh_init.io_cfg.id == ID_LLS_CU){
        pTCtx[(xran_lib_ota_tti & 1)].tti_to_process = next_tti;
    } else {
        pTCtx[(xran_lib_ota_tti & 1)].tti_to_process = pTCtx[(xran_lib_ota_tti & 1)^1].tti_to_process;
    }

    p_xran_dev_ctx->phy_tti_cb_done = 0;
    xran_timer_arm_ex(&tti_to_phy_timer[xran_lib_ota_tti % 10], tti_to_phy_cb, (void*)pTCtx, p_xran_dev_ctx->fh_init.io_cfg.timing_core);

    //slot index is increased to next slot at the beginning of current OTA slot
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
        struct xran_prb_map *prbMap, enum xran_category category,  uint8_t ctx_id)
{
    struct xran_device_ctx *p_x_ctx = xran_dev_get_ctx();
    struct xran_cp_gen_params params;
    struct xran_section_gen_info sect_geninfo[1];
    struct rte_mbuf *mbuf;
    int ret = 0;
    uint32_t i, j, loc_sym;
    uint32_t nsection = 0;
    struct xran_prb_elm *pPrbMapElem = NULL;
    struct xran_prb_elm *pPrbMapElemPrev = NULL;
    uint32_t slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME);
    uint32_t subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME,  SUBFRAMES_PER_SYSTEMFRAME);
    uint32_t frame_id    = XranGetFrameNum(tti,xran_getSfnSecStart(),SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME);

    frame_id = (frame_id & 0xff); /* ORAN frameId, 8 bits, [0, 255] */
    uint8_t seq_id = 0;

    struct xran_sectionext1_info m_ext1;

    if(prbMap) {
        nsection = prbMap->nPrbElm;
        pPrbMapElem = &prbMap->prbMap[0];
        if (nsection < 1){
            print_dbg("cp[%d:%d:%d] ru_port_id %d dir=%d nsection %d\n",
                                   frame_id, subframe_id, slot_id, ru_port_id, dir, nsection);
        }
    } else {
        print_err("prbMap is NULL\n");
        return (-1);
    }
    for (i=0; i<nsection; i++)
    {
        pPrbMapElem                 = &prbMap->prbMap[i];
        params.dir                  = dir;
        params.sectionType          = XRAN_CP_SECTIONTYPE_1;        /* Most DL/UL Radio Channels */
        params.hdr.filterIdx        = XRAN_FILTERINDEX_STANDARD;
        params.hdr.frameId          = frame_id;
        params.hdr.subframeId       = subframe_id;
        params.hdr.slotId           = slot_id;
        params.hdr.startSymId       = pPrbMapElem->nStartSymb;
        params.hdr.iqWidth          = pPrbMapElem->iqWidth; /*xran_get_conf_iqwidth(pHandle);*/
        params.hdr.compMeth         = pPrbMapElem->compMethod;

        print_dbg("cp[%d:%d:%d] ru_port_id %d dir=%d\n",
                               frame_id, subframe_id, slot_id, ru_port_id, dir);

        seq_id = xran_get_cp_seqid(pHandle, XRAN_DIR_DL, cc_id, ru_port_id);

        sect_geninfo[0].info.type        = params.sectionType;       // for database
        sect_geninfo[0].info.startSymId  = params.hdr.startSymId;    // for database
        sect_geninfo[0].info.iqWidth     = params.hdr.iqWidth;       // for database
        sect_geninfo[0].info.compMeth    = params.hdr.compMeth;      // for database
        sect_geninfo[0].info.id          = i; /*xran_alloc_sectionid(pHandle, dir, cc_id, ru_port_id, slot_id);*/

        if(sect_geninfo[0].info.id > 7)
            print_err("sectinfo->id %d\n", sect_geninfo[0].info.id);

        if (dir == XRAN_DIR_UL) {
            for (loc_sym = 0; loc_sym < XRAN_NUM_OF_SYMBOL_PER_SLOT; loc_sym++){
                struct xran_section_desc *p_sec_desc =  pPrbMapElem->p_sec_desc[loc_sym];
                if(p_sec_desc) {
                    p_sec_desc->section_id   = sect_geninfo[0].info.id;
                    if(p_sec_desc->pCtrl) {
                        rte_pktmbuf_free(p_sec_desc->pCtrl);
                        p_sec_desc->pCtrl = NULL;
                        p_sec_desc->pData = NULL;
                    }
                } else {
                    print_err("section desc is NULL\n");
                }
            }
        }

        sect_geninfo[0].info.rb          = XRAN_RBIND_EVERY;
        sect_geninfo[0].info.startPrbc   = pPrbMapElem->nRBStart;
        sect_geninfo[0].info.numPrbc     = pPrbMapElem->nRBSize;
        sect_geninfo[0].info.numSymbol   = pPrbMapElem->numSymb;
        sect_geninfo[0].info.reMask      = 0xfff;
        sect_geninfo[0].info.beamId      = pPrbMapElem->nBeamIndex;

        for (loc_sym = 0; loc_sym < XRAN_NUM_OF_SYMBOL_PER_SLOT; loc_sym++){
            struct xran_section_desc *p_sec_desc =  pPrbMapElem->p_sec_desc[loc_sym];
            if(p_sec_desc) {
                p_sec_desc->section_id   = sect_geninfo[0].info.id;

                sect_geninfo[0].info.sec_desc[loc_sym].iq_buffer_offset = p_sec_desc->iq_buffer_offset;
                sect_geninfo[0].info.sec_desc[loc_sym].iq_buffer_len    = p_sec_desc->iq_buffer_len;
            } else {
                print_err("section desc is NULL\n");
            }
        }

        if (i==0)
            sect_geninfo[0].info.symInc      = XRAN_SYMBOLNUMBER_NOTINC;
        else
        {
            pPrbMapElemPrev = &prbMap->prbMap[i-1];
            if (pPrbMapElemPrev->nStartSymb == pPrbMapElem->nStartSymb)
            {
                sect_geninfo[0].info.symInc      = XRAN_SYMBOLNUMBER_NOTINC;
                if (pPrbMapElemPrev->numSymb != pPrbMapElem->numSymb)
                    print_err("section info error: previous numSymb %d not equal to current numSymb %d\n", pPrbMapElemPrev->numSymb, pPrbMapElem->numSymb);
            }
            else
            {
                sect_geninfo[0].info.symInc      = XRAN_SYMBOLNUMBER_INC;
                if (pPrbMapElem->nStartSymb != (pPrbMapElemPrev->nStartSymb + pPrbMapElemPrev->numSymb))
                    print_err("section info error: current startSym %d not equal to previous endSymb %d\n", pPrbMapElem->nStartSymb, pPrbMapElemPrev->nStartSymb + pPrbMapElemPrev->numSymb);
            }
        }

        if(category == XRAN_CATEGORY_A){
            /* no extention sections for category */
            sect_geninfo[0].info.ef          = 0;
            sect_geninfo[0].exDataSize       = 0;
        } else if (category == XRAN_CATEGORY_B) {
            /*add extantion section for BF Weights if update is needed */
            if(pPrbMapElem->bf_weight_update){
                memset(&m_ext1, 0, sizeof (struct xran_sectionext1_info));
                m_ext1.bfwNumber      = pPrbMapElem->bf_weight.nAntElmTRx;
                m_ext1.bfwiqWidth     = pPrbMapElem->iqWidth;
                m_ext1.bfwCompMeth    = pPrbMapElem->compMethod;
                m_ext1.p_bfwIQ        = (int16_t*)pPrbMapElem->bf_weight.p_ext_section;
                m_ext1.bfwIQ_sz       = pPrbMapElem->bf_weight.ext_section_sz;

                sect_geninfo[0].exData[0].type = XRAN_CP_SECTIONEXTCMD_1;
                sect_geninfo[0].exData[0].len  = sizeof(m_ext1);
                sect_geninfo[0].exData[0].data = &m_ext1;

                sect_geninfo[0].info.ef       = 1;
                sect_geninfo[0].exDataSize    = 1;
            } else {
                sect_geninfo[0].info.ef          = 0;
                sect_geninfo[0].exDataSize       = 0;
            }
        } else {
            print_err("Unsupported Category %d\n", category);
            return (-1);
        }

        params.numSections          = 1;//nsection;
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
            tx_counter++;
            tx_bytes_counter += rte_pktmbuf_pkt_len(mbuf);
            p_x_ctx->send_cpmbuf2ring(mbuf, ETHER_TYPE_ECPRI);

            /*for(i=0; i<nsection; i++)*/
                xran_cp_add_section_info(pHandle,
                        dir, cc_id, ru_port_id,
                        ctx_id,
                        &sect_geninfo[0].info);
        }
    }

    return ret;
}

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
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    struct xran_timer_ctx *pTCtx = (struct xran_timer_ctx *)arg;

    pHandle = NULL;     // TODO: temp implemantation
    num_eAxc    = xran_get_num_eAxc(pHandle);
    num_CCPorts = xran_get_num_cc(pHandle);

    if(first_call && p_xran_dev_ctx->enableCP) {

        tti = pTCtx[(xran_lib_ota_tti & 1) ^ 1].tti_to_process;
        buf_id = tti % XRAN_N_FE_BUF_LEN;

        slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME);
        subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME,  SUBFRAMES_PER_SYSTEMFRAME);
        frame_id    = XranGetFrameNum(tti,xran_getSfnSecStart(),SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME);
        if (tti == 0){
            /* Wrap around to next second */
            frame_id = (frame_id + NUM_OF_FRAMES_PER_SECOND) & 0x3ff;
        }

        ctx_id      = XranGetSlotNum(tti, SLOTS_PER_SYSTEMFRAME) % XRAN_MAX_SECTIONDB_CTX;

        print_dbg("[%d]SFN %d sf %d slot %d\n", tti, frame_id, subframe_id, slot_id);
        for(ant_id = 0; ant_id < num_eAxc; ++ant_id) {
            for(cc_id = 0; cc_id < num_CCPorts; cc_id++ ) {
                /* start new section information list */
                xran_cp_reset_section_info(pHandle, XRAN_DIR_DL, cc_id, ant_id, ctx_id);
                if(xran_fs_get_slot_type(cc_id, tti, XRAN_SLOT_TYPE_DL) == 1) {
                    if(p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[buf_id][cc_id][ant_id].sBufferList.pBuffers->pData){
                        num_list = xran_cp_create_and_send_section(pHandle, ant_id, XRAN_DIR_DL, tti, cc_id,
                            (struct xran_prb_map *)p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[buf_id][cc_id][ant_id].sBufferList.pBuffers->pData,
                            p_xran_dev_ctx->fh_cfg.ru_conf.xranCat, ctx_id);
                    } else {
                        print_err("[%d]SFN %d sf %d slot %d: ant_id %d cc_id %d \n", tti, frame_id, subframe_id, slot_id, ant_id, cc_id);
                    }
                } /* if(xran_fs_get_slot_type(cc_id, tti, XRAN_SLOT_TYPE_DL) == 1) */
            } /* for(cc_id = 0; cc_id < num_CCPorts; cc_id++) */
        } /* for(ant_id = 0; ant_id < num_eAxc; ++ant_id) */
        MLogTask(PID_CP_DL_CB, t1, MLogTick());
    }
}

void rx_ul_deadline_half_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogTick();
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    xran_status_t status;
    /* half of RX for current TTI as measured against current OTA time */
    int32_t rx_tti = (int32_t)XranGetTtiNum(xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
    int32_t cc_id;
    uint32_t nFrameIdx;
    uint32_t nSubframeIdx;
    uint32_t nSlotIdx;
    uint64_t nSecond;

    uint32_t nXranTime  = xran_get_slot_idx(&nFrameIdx, &nSubframeIdx, &nSlotIdx, &nSecond);
    rx_tti = nFrameIdx*SUBFRAMES_PER_SYSTEMFRAME*SLOTNUM_PER_SUBFRAME
           + nSubframeIdx*SLOTNUM_PER_SUBFRAME
           + nSlotIdx;

    if(p_xran_dev_ctx->xran2phy_mem_ready == 0)
        return;

    for(cc_id = 0; cc_id < xran_get_num_cc(p_xran_dev_ctx); cc_id++) {
        if(p_xran_dev_ctx->rx_packet_callback_tracker[rx_tti % XRAN_N_FE_BUF_LEN][cc_id] == 0){
            struct xran_cb_tag *pTag = p_xran_dev_ctx->pCallbackTag[cc_id];
            pTag->slotiId = rx_tti;
            pTag->symbol  = 0; /* last 7 sym means full slot of Symb */
            status = XRAN_STATUS_SUCCESS;
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
    uint32_t nFrameIdx;
    uint32_t nSubframeIdx;
    uint32_t nSlotIdx;
    uint64_t nSecond;

    uint32_t nXranTime  = xran_get_slot_idx(&nFrameIdx, &nSubframeIdx, &nSlotIdx, &nSecond);
    rx_tti = nFrameIdx*SUBFRAMES_PER_SYSTEMFRAME*SLOTNUM_PER_SUBFRAME
        + nSubframeIdx*SLOTNUM_PER_SUBFRAME
        + nSlotIdx;

    if(rx_tti == 0)
       rx_tti = (xran_fs_get_max_slot_SFN()-1);
    else
       rx_tti -= 1; /* end of RX for prev TTI as measured against current OTA time */

    if(p_xran_dev_ctx->xran2phy_mem_ready == 0)
        return;

    /* U-Plane */
    for(cc_id = 0; cc_id < xran_get_num_cc(p_xran_dev_ctx); cc_id++) {
        struct xran_cb_tag *pTag = p_xran_dev_ctx->pCallbackTag[cc_id];
        pTag->slotiId = rx_tti;
        pTag->symbol  = 7; /* last 7 sym means full slot of Symb */
        status = XRAN_STATUS_SUCCESS;
        if(p_xran_dev_ctx->pCallback[cc_id])
            p_xran_dev_ctx->pCallback[cc_id](p_xran_dev_ctx->pCallbackTag[cc_id], status);

        if(p_xran_dev_ctx->pPrachCallback[cc_id]){
            struct xran_cb_tag *pTag = p_xran_dev_ctx->pPrachCallbackTag[cc_id];
            pTag->slotiId = rx_tti;
            pTag->symbol  = 7; /* last 7 sym means full slot of Symb */
            p_xran_dev_ctx->pPrachCallback[cc_id](p_xran_dev_ctx->pPrachCallbackTag[cc_id], status);
        }
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

    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    struct xran_prach_cp_config *pPrachCPConfig = &(p_xran_dev_ctx->PrachCPConfig);
    struct xran_timer_ctx *pTCtx = (struct xran_timer_ctx *)arg;

    pHandle     = NULL;     // TODO: temp implemantation

    if(xran_get_ru_category(pHandle) == XRAN_CATEGORY_A)
        num_eAxc    = xran_get_num_eAxc(pHandle);
    else
        num_eAxc    = xran_get_num_eAxcUl(pHandle);

    num_CCPorts = xran_get_num_cc(pHandle);
    tti = pTCtx[(xran_lib_ota_tti & 1) ^ 1].tti_to_process;
    buf_id = tti % XRAN_N_FE_BUF_LEN;
    slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME);
    subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME,  SUBFRAMES_PER_SYSTEMFRAME);
    frame_id    = XranGetFrameNum(tti,xran_getSfnSecStart(),SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME);
    if (tti == 0) {
        //Wrap around to next second
        frame_id = (frame_id + NUM_OF_FRAMES_PER_SECOND) & 0x3ff;
    }
    ctx_id      = XranGetSlotNum(tti, SLOTS_PER_SYSTEMFRAME) % XRAN_MAX_SECTIONDB_CTX;

    if(first_call && p_xran_dev_ctx->enableCP) {

        print_dbg("[%d]SFN %d sf %d slot %d\n", tti, frame_id, subframe_id, slot_id);

        for(ant_id = 0; ant_id < num_eAxc; ++ant_id) {
            for(cc_id = 0; cc_id < num_CCPorts; cc_id++) {
                if(xran_fs_get_slot_type(cc_id, tti, XRAN_SLOT_TYPE_UL) == 1 ||
                    xran_fs_get_slot_type(cc_id, tti, XRAN_SLOT_TYPE_SP) == 1 ){
                    /* start new section information list */
                    xran_cp_reset_section_info(pHandle, XRAN_DIR_UL, cc_id, ant_id, ctx_id);
                    num_list = xran_cp_create_and_send_section(pHandle, ant_id, XRAN_DIR_UL, tti, cc_id,
                        (struct xran_prb_map *)p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[buf_id][cc_id][ant_id].sBufferList.pBuffers->pData,
                        p_xran_dev_ctx->fh_cfg.ru_conf.xranCat, ctx_id);
                } /* if(xran_fs_get_slot_type(cc_id, tti, XRAN_SLOT_TYPE_UL) == 1 */
            } /* for(cc_id = 0; cc_id < num_CCPorts; cc_id++) */
        } /* for(ant_id = 0; ant_id < num_eAxc; ++ant_id) */

        if(p_xran_dev_ctx->enablePrach) {
            uint32_t is_prach_slot = xran_is_prach_slot(subframe_id, slot_id);
            if(((frame_id % pPrachCPConfig->x) == pPrachCPConfig->y[0]) && (is_prach_slot==1)) {   //is prach slot
                for(ant_id = 0; ant_id < num_eAxc; ++ant_id) {
                    for(cc_id = 0; cc_id < num_CCPorts; cc_id++) {
                        struct xran_cp_gen_params params;
                        struct xran_section_gen_info sect_geninfo[8];
                        struct rte_mbuf *mbuf = xran_ethdi_mbuf_alloc();
                        prach_port_id = ant_id + num_eAxc;
                        /* start new section information list */
                        xran_cp_reset_section_info(pHandle, XRAN_DIR_UL, cc_id, prach_port_id, ctx_id);

                        beam_id = xran_get_beamid(pHandle, XRAN_DIR_UL, cc_id, prach_port_id, slot_id);
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
    } /* if(p_xran_dev_ctx->enableCP) */

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
            uint32_t slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME);
            uint32_t subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME,  SUBFRAMES_PER_SYSTEMFRAME);
            uint32_t frame_id = XranGetFrameNum(tti,xran_getSfnSecStart(),SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME);
            if((frame_id == xran_max_frame)&&(subframe_id==9)&&(slot_id == SLOTNUM_PER_SUBFRAME-1)) {  //(tti == xran_fs_get_max_slot()-1)
                first_call = 1;
            }
        }
    }

    MLogTask(PID_TTI_CB_TO_PHY, t1, MLogTick());
}

int xran_timing_source_thread(void *args)
{
    int res = 0;
    cpu_set_t cpuset;
    int32_t   do_reset = 0;
    uint64_t  t1 = 0;
    uint64_t  delta;
    int32_t   result1,i,j;
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
    uint64_t tWake = 0, tWakePrev = 0, tUsed = 0;
    struct cb_elem_entry * cb_elm = NULL;

    /* ToS = Top of Second start +- 1.5us */
    struct timespec ts;

    char buff[100];

    xran_core_used = rte_lcore_id();
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
            printf("O-DU: thread_run start time: %s.%09ld UTC [%ld]\n", buff, ts.tv_nsec, interval_us);
        }

        delay_cp_dl = interval_us - p_xran_dev_ctx->fh_init.T1a_max_cp_dl;
        delay_cp_ul = interval_us - p_xran_dev_ctx->fh_init.T1a_max_cp_ul;
        delay_up    = p_xran_dev_ctx->fh_init.T1a_max_up;
        delay_up_ul = p_xran_dev_ctx->fh_init.Ta4_max;

        delay_cp2up = delay_up-delay_cp_dl;

        sym_cp_dl = delay_cp_dl*1000/(interval_us*1000/N_SYM_PER_SLOT)+1;
        sym_cp_ul = delay_cp_ul*1000/(interval_us*1000/N_SYM_PER_SLOT)+1;
        sym_up_ul = delay_up_ul*1000/(interval_us*1000/N_SYM_PER_SLOT);
        p_xran_dev_ctx->sym_up = sym_up = -(delay_up*1000/(interval_us*1000/N_SYM_PER_SLOT));
        p_xran_dev_ctx->sym_up_ul = sym_up_ul = (delay_up_ul*1000/(interval_us*1000/N_SYM_PER_SLOT)+1);

        printf("Start C-plane DL %d us after TTI  [trigger on sym %d]\n", delay_cp_dl, sym_cp_dl);
        printf("Start C-plane UL %d us after TTI  [trigger on sym %d]\n", delay_cp_ul, sym_cp_ul);
        printf("Start U-plane DL %d us before OTA [offset  in sym %d]\n", delay_up, sym_up);
        printf("Start U-plane UL %d us OTA        [offset  in sym %d]\n", delay_up_ul, sym_up_ul);

        printf("C-plane to U-plane delay %d us after TTI\n", delay_cp2up);
        printf("Start Sym timer %ld ns\n", TX_TIMER_INTERVAL/N_SYM_PER_SLOT);

        cb_elm = xran_create_cb(xran_timer_arm, tx_cp_dl_cb);
        if(cb_elm){
            LIST_INSERT_HEAD(&p_xran_dev_ctx->sym_cb_list_head[0][sym_cp_dl],
                             cb_elm,
                             pointers);
        } else {
            print_err("cb_elm is NULL\n");
            res =  -1;
            goto err0;
        }

        cb_elm = xran_create_cb(xran_timer_arm, tx_cp_ul_cb);
        if(cb_elm){
            LIST_INSERT_HEAD(&p_xran_dev_ctx->sym_cb_list_head[0][sym_cp_ul],
                             cb_elm,
                             pointers);
        } else {
            print_err("cb_elm is NULL\n");
            res =  -1;
            goto err0;
        }

        /* Full slot UL OTA + delay_up_ul */
        cb_elm = xran_create_cb(xran_timer_arm, rx_ul_deadline_full_cb);
        if(cb_elm){
            LIST_INSERT_HEAD(&p_xran_dev_ctx->sym_cb_list_head[0][sym_up_ul],
                             cb_elm,
                             pointers);
        } else {
            print_err("cb_elm is NULL\n");
            res =  -1;
            goto err0;
        }

        /* Half slot UL OTA + delay_up_ul*/
        cb_elm = xran_create_cb(xran_timer_arm, rx_ul_deadline_half_cb);
        if(cb_elm){
            LIST_INSERT_HEAD(&p_xran_dev_ctx->sym_cb_list_head[0][sym_up_ul + N_SYM_PER_SLOT/2],
                         cb_elm,
                         pointers);
        } else {
            print_err("cb_elm is NULL\n");
            res =  -1;
            goto err0;
        }
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

        if (likely(XRAN_RUNNING == xran_if_current_state))
            sym_ota_cb(&sym_timer, timer_ctx, &tUsed);
    }

    err0:
    for (i = 0; i< XRAN_MAX_SECTOR_NR; i++){
        for (j = 0; j< XRAN_NUM_OF_SYMBOL_PER_SLOT; j++){
            struct cb_elem_entry *cb_elm;
            LIST_FOREACH(cb_elm, &p_xran_dev_ctx->sym_cb_list_head[i][j], pointers){
                if(cb_elm){
                    LIST_REMOVE(cb_elm, pointers);
                    xran_destroy_cb(cb_elm);
                }
            }
        }
    }

    printf("Closing timing source thread...tx counter %lu, rx counter %lu\n", tx_counter, rx_counter);
    return res;
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

    rx_bytes_counter += rte_pktmbuf_pkt_len(pkt);
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
                        uint16_t sect_id,
                        uint32_t *mb_free)
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

    if(CC_ID < XRAN_MAX_SECTOR_NR && Ant_ID < XRAN_MAX_ANTENNA_NR && symb_id < XRAN_NUM_OF_SYMBOL_PER_SLOT){
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
                *mb_free = MBUF_FREE;
            }else {
                mb = p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id_offset].pCtrl;
                if(mb){
                   rte_pktmbuf_free(mb);
                }else{
                   print_err("mb==NULL\n");
                }
                p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id_offset].pData = iq_data_start;
                p_xran_dev_ctx->sFHPrachRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id_offset].pCtrl = mbuf;
                *mb_free = MBUF_KEEP;
            }
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

int32_t xran_process_srs_sym(void *arg,
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

    if(p_xran_dev_ctx->xran2phy_mem_ready == 0)
        return 0;

    tti = frame_id * SLOTS_PER_SYSTEMFRAME + subframe_id * SLOTNUM_PER_SUBFRAME + slot_id;

    status = tti << 16 | symb_id;

    if(CC_ID < XRAN_MAX_SECTOR_NR && Ant_ID < p_xran_dev_ctx->fh_cfg.nAntElmTRx && symb_id < XRAN_NUM_OF_SYMBOL_PER_SLOT) {
        pos = (char*) p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id].pData;
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
                    mb = p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id].pCtrl;
                    if(mb){
                       rte_pktmbuf_free(mb);
                    }else{
                       print_err("mb==NULL\n");
                    }
                    p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id].pData = iq_data_start;
                    p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id].pCtrl = mbuf;
                    *mb_free = MBUF_KEEP;
                } else {
                    /* packet can be fragmented copy RBs */
                    rte_memcpy(pos, iq_data_start, size);
                    *mb_free = MBUF_FREE;
                }
            }
        } else {
            print_err("pos %p iq_data_start %p size %d\n",pos, iq_data_start, size);
        }
    } else {
        print_err("TTI %d(f_%d sf_%d slot_%d) CC %d Ant_ID %d symb_id %d\n",tti, frame_id, subframe_id, slot_id, CC_ID, Ant_ID, symb_id);
    }

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

int32_t xran_process_rx_sym(void *arg,
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
    struct xran_prb_map * pRbMap    = NULL;
    struct xran_prb_elm * prbMapElm = NULL;

    uint16_t iq_sample_size_bits = 16;

    tti = frame_id * SLOTS_PER_SYSTEMFRAME + subframe_id * SLOTNUM_PER_SUBFRAME + slot_id;

    status = tti << 16 | symb_id;

    if(CC_ID < XRAN_MAX_SECTOR_NR && Ant_ID < XRAN_MAX_ANTENNA_NR && symb_id < XRAN_NUM_OF_SYMBOL_PER_SLOT){
        pos = (char*) p_xran_dev_ctx->sFrontHaulRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers[symb_id].pData;
        pRbMap = (struct xran_prb_map *) p_xran_dev_ctx->sFrontHaulRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][CC_ID][Ant_ID].sBufferList.pBuffers->pData;
        if(pRbMap){
            prbMapElm = &pRbMap->prbMap[sect_id];
            if(sect_id >= pRbMap->nPrbElm) {
                print_err("sect_id %d !=pRbMap->nPrbElm %d\n", sect_id,pRbMap->nPrbElm);
                *mb_free = MBUF_FREE;
                return size;
            }
        } else {
            print_err("pRbMap==NULL\n");
            *mb_free = MBUF_FREE;
            return size;
        }

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
                if (/*likely (p_xran_dev_ctx->fh_init.mtu >=
                              p_xran_dev_ctx->fh_cfg.nULRBs * N_SC_PER_PRB*(iq_sample_size_bits/8)*2)
                              &&  p_xran_dev_ctx->fh_init.io_cfg.id == O_DU*/ 1) {
                    if (pRbMap->nPrbElm == 1){
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
                        prbMapElm = &pRbMap->prbMap[sect_id];
                        struct xran_section_desc *p_sec_desc =  prbMapElm->p_sec_desc[symb_id];
                        if(p_sec_desc){
                            mb = p_sec_desc->pCtrl;
                            if(mb){
                               rte_pktmbuf_free(mb);
                            }
                            p_sec_desc->pData         = iq_data_start;
                            p_sec_desc->pCtrl         = mbuf;
                            p_sec_desc->iq_buffer_len = size;
                            p_sec_desc->iq_buffer_offset = RTE_PTR_DIFF(iq_data_start, mbuf);
                        } else {
                            print_err("p_sec_desc==NULL\n");
                            *mb_free = MBUF_FREE;
                            return size;
                        }
                        *mb_free = MBUF_KEEP;
                    }
                } else {
                    /* packet can be fragmented copy RBs */
                    rte_memcpy(pos, iq_data_start, size);
                    *mb_free = MBUF_FREE;
                }
            }
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
        tx_bytes_counter += rte_pktmbuf_pkt_len(m_table[i]);
        ret += dev->send_upmbuf2ring(m_table[i], ETHER_TYPE_ECPRI);
    }


    if (unlikely(ret < n)) {
        print_err("ret < n\n");
    }

    return 0;
}

int32_t xran_process_tx_sym_cp_off(uint8_t ctx_id, uint32_t tti, int32_t cc_id, int32_t ant_id, uint32_t frame_id, uint32_t subframe_id, uint32_t slot_id, uint32_t sym_id,
    int32_t do_srs)
{
    int32_t     retval = 0;
    uint64_t    t1 = MLogTick();

    void        *pHandle = NULL;
    char        *pos = NULL;
    char        *p_sec_iq = NULL;
    char        *p_sect_iq = NULL;
    void        *mb  = NULL;
    int         prb_num = 0;
    uint16_t    iq_sample_size_bits = 16; // TODO: make dynamic per

    struct xran_prb_map *prb_map = NULL;
    uint8_t  num_ant_elm  = 0;

    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    struct xran_prach_cp_config *pPrachCPConfig = &(p_xran_dev_ctx->PrachCPConfig);
    struct xran_srs_config *p_srs_cfg = &(p_xran_dev_ctx->srs_cfg);
    num_ant_elm = xran_get_num_ant_elm(pHandle);
    enum xran_pkt_dir direction;

    struct rte_mbuf *eth_oran_hdr = NULL;
    char        *ext_buff = NULL;
    uint16_t    ext_buff_len = 0;
    struct rte_mbuf *tmp = NULL;
    rte_iova_t ext_buff_iova = 0;

    struct rte_mbuf_ext_shared_info * p_share_data = &share_data[tti % XRAN_N_FE_BUF_LEN];

    if(p_xran_dev_ctx->fh_init.io_cfg.id == O_DU) {
        direction = XRAN_DIR_DL; /* O-DU */
        prb_num = p_xran_dev_ctx->fh_cfg.nDLRBs;
    } else {
        direction = XRAN_DIR_UL; /* RU */
        prb_num = p_xran_dev_ctx->fh_cfg.nULRBs;
    }

    if(xran_fs_get_slot_type(cc_id, tti, ((p_xran_dev_ctx->fh_init.io_cfg.id == O_DU)? XRAN_SLOT_TYPE_DL : XRAN_SLOT_TYPE_UL)) ==  1
            || xran_fs_get_slot_type(cc_id, tti, XRAN_SLOT_TYPE_SP) ==  1
            || xran_fs_get_slot_type(cc_id, tti, XRAN_SLOT_TYPE_FDD) ==  1){

        if(xran_fs_get_symbol_type(cc_id, tti, sym_id) == ((p_xran_dev_ctx->fh_init.io_cfg.id == O_DU)? XRAN_SYMBOL_TYPE_DL : XRAN_SYMBOL_TYPE_UL)
           || xran_fs_get_symbol_type(cc_id, tti, sym_id) == XRAN_SYMBOL_TYPE_FDD){

            if(iq_sample_size_bits != 16)
                print_err("Incorrect iqWidth %d\n", iq_sample_size_bits );

            pos = (char*) p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id].pData;
            mb  = (void*) p_xran_dev_ctx->sFrontHaulTxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers[sym_id].pCtrl;
            prb_map  = (struct xran_prb_map *) p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_id].sBufferList.pBuffers->pData;


            if(prb_map){
                int32_t elmIdx = 0;
                for (elmIdx = 0; elmIdx < prb_map->nPrbElm; elmIdx++){
                    uint16_t sec_id  = elmIdx;
                    struct xran_prb_elm * prb_map_elm = &prb_map->prbMap[elmIdx];
                    struct xran_section_desc * p_sec_desc = NULL;

                    if(prb_map_elm == NULL){
                        rte_panic("p_sec_desc == NULL\n");
                    }

                    p_sec_desc =  prb_map_elm->p_sec_desc[sym_id];

                    if(p_sec_desc == NULL){
                        rte_panic("p_sec_desc == NULL\n");
                    }

#if 1
                    p_sec_iq = ((char*)pos + p_sec_desc->iq_buffer_offset);

                    /* calculete offset for external buffer */
                    ext_buff_len = p_sec_desc->iq_buffer_len;
                    ext_buff = p_sec_iq - (RTE_PKTMBUF_HEADROOM +
                                    sizeof (struct xran_ecpri_hdr) +
                                    sizeof (struct radio_app_common_hdr) +
                                    sizeof(struct data_section_hdr));

                    ext_buff_len += RTE_PKTMBUF_HEADROOM +
                                    sizeof (struct xran_ecpri_hdr) +
                                    sizeof (struct radio_app_common_hdr) +
                                    sizeof(struct data_section_hdr) + 18;

                    if(prb_map_elm->compMethod != XRAN_COMPMETHOD_NONE){
                        ext_buff     -= sizeof (struct data_section_compression_hdr);
                        ext_buff_len += sizeof (struct data_section_compression_hdr);
                    }

                    eth_oran_hdr =  rte_pktmbuf_alloc(_eth_mbuf_pool_small);

                    if (unlikely (( eth_oran_hdr) == NULL)) {
                        rte_panic("Failed rte_pktmbuf_alloc\n");
                    }

                    p_share_data->free_cb = extbuf_free_callback;
                    p_share_data->fcb_opaque = NULL;
                    rte_mbuf_ext_refcnt_set(p_share_data, 1);

                    ext_buff_iova = rte_mempool_virt2iova(mb);
                    if (unlikely (( ext_buff_iova) == 0)) {
                        rte_panic("Failed rte_mem_virt2iova \n");
                    }

                    if (unlikely (( (rte_iova_t)ext_buff_iova) == RTE_BAD_IOVA)) {
                        rte_panic("Failed rte_mem_virt2iova RTE_BAD_IOVA \n");
                    }

                    rte_pktmbuf_attach_extbuf(eth_oran_hdr,
                                              ext_buff,
                                              ext_buff_iova + RTE_PTR_DIFF(ext_buff , mb),
                                              ext_buff_len,
                                              p_share_data);

                    rte_pktmbuf_reset_headroom(eth_oran_hdr);

                    tmp = (struct rte_mbuf *)rte_pktmbuf_prepend(eth_oran_hdr, sizeof(struct ether_hdr));
                    if (unlikely (( tmp) == NULL)) {
                        rte_panic("Failed rte_pktmbuf_prepend \n");
                    }
                    mb = eth_oran_hdr;

                    /* first all PRBs */
                    prepare_symbol_ex(direction, sec_id,
                                      mb,
                                      (struct rb_map *)p_sec_iq,
                                      prb_map_elm->compMethod,
                                      prb_map_elm->iqWidth,
                                      p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder,
                                      frame_id, subframe_id, slot_id, sym_id,
                                      prb_map_elm->nRBStart, prb_map_elm->nRBSize,
                                      cc_id, ant_id,
                                      (p_xran_dev_ctx->fh_init.io_cfg.id == O_DU) ?
                                          xran_get_updl_seqid(pHandle, cc_id, ant_id) :
                                          xran_get_upul_seqid(pHandle, cc_id, ant_id),
                                      0);

                    rte_mbuf_sanity_check((struct rte_mbuf *)mb, 0);
                    tx_counter++;
                    tx_bytes_counter += rte_pktmbuf_pkt_len((struct rte_mbuf *)mb);
                    p_xran_dev_ctx->send_upmbuf2ring((struct rte_mbuf *)mb, ETHER_TYPE_ECPRI);
#else
        p_sect_iq = pos + p_sec_desc->iq_buffer_offset;
        prb_num = prb_map_elm->nRBSize;

        if( prb_num > 136 || prb_num == 0) {
            /* first 136 PRBs */
            rte_panic("first 136 PRBs\n");
            send_symbol_ex(direction,
                            sec_id,
                            NULL,
                            (struct rb_map *)p_sect_iq,
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
                             (struct rb_map *)p_sect_iq,
                             p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder,
                             frame_id, subframe_id, slot_id, sym_id,
                             136, 137,
                             cc_id, ant_id,
                             (p_xran_dev_ctx->fh_init.io_cfg.id == O_DU) ?
                                xran_get_updl_seqid(pHandle, cc_id, ant_id) :
                                xran_get_upul_seqid(pHandle,  cc_id, ant_id));
            retval = 1;
        } else {
            send_symbol_ex(direction,
                    sec_id, /* xran_alloc_sectionid(pHandle, direction, cc_id, ant_id, slot_id)*/
                    /*(struct rte_mbuf *)mb*/ NULL,
                    (struct rb_map *)p_sect_iq,
                    p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder,
                    frame_id, subframe_id, slot_id, sym_id,
                    prb_map_elm->nRBStart, prb_map_elm->nRBSize,
                    cc_id, ant_id,
                    (p_xran_dev_ctx->fh_init.io_cfg.id == O_DU) ?
                        xran_get_updl_seqid(pHandle, cc_id, ant_id) :
                        xran_get_upul_seqid(pHandle, cc_id, ant_id));
            retval = 1;
        }

#endif

                }
            } else {
                printf("(%d %d %d %d) prb_map == NULL\n", tti % XRAN_N_FE_BUF_LEN, cc_id, ant_id, sym_id);
            }

            if(p_xran_dev_ctx->enablePrach
              && (p_xran_dev_ctx->fh_init.io_cfg.id == O_RU)) {   /* Only RU needs to send PRACH I/Q */
                uint32_t is_prach_slot = xran_is_prach_slot(subframe_id, slot_id);
                if(((frame_id % pPrachCPConfig->x) == pPrachCPConfig->y[0])
                        && (is_prach_slot == 1)
                        && (sym_id >= p_xran_dev_ctx->prach_start_symbol[cc_id])
                        && (sym_id <= p_xran_dev_ctx->prach_last_symbol[cc_id])) {  //is prach slot
                        int prach_port_id = ant_id + pPrachCPConfig->eAxC_offset;
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
                        retval = 1;
                } /* if((frame_id % pPrachCPConfig->x == pPrachCPConfig->y[0]) .... */
            } /* if(p_xran_dev_ctx->enablePrach ..... */


            if(p_xran_dev_ctx->enableSrs && (p_xran_dev_ctx->fh_init.io_cfg.id == O_RU)){
                if( p_srs_cfg->symbMask & (1 << sym_id) /* is SRS symbol */
                    && do_srs) {
                    int32_t ant_elm_id = 0;

                    for (ant_elm_id = 0; ant_elm_id < num_ant_elm; ant_elm_id++){
                        int32_t ant_elm_eAxC_id = ant_elm_id + p_srs_cfg->eAxC_offset;

                        pos = (char*) p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_elm_id].sBufferList.pBuffers[sym_id].pData;
                        mb  = (void*) p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][cc_id][ant_elm_id].sBufferList.pBuffers[sym_id].pCtrl;

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
                                            cc_id, ant_elm_eAxC_id,
                                            (p_xran_dev_ctx->fh_init.io_cfg.id == O_DU) ?
                                                xran_get_updl_seqid(pHandle, cc_id, ant_elm_eAxC_id) :
                                                xran_get_upul_seqid(pHandle, cc_id, ant_elm_eAxC_id));

                             pos += 136 * N_SC_PER_PRB * (iq_sample_size_bits/8)*2;
                             /* last 137 PRBs */
                             send_symbol_ex(direction, sec_id,
                                             NULL,
                                             (struct rb_map *)pos,
                                             p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder,
                                             frame_id, subframe_id, slot_id, sym_id,
                                             136, 137,
                                             cc_id, ant_elm_eAxC_id,
                                             (p_xran_dev_ctx->fh_init.io_cfg.id == O_DU) ?
                                                xran_get_updl_seqid(pHandle, cc_id, ant_elm_eAxC_id) :
                                                xran_get_upul_seqid(pHandle, cc_id, ant_elm_eAxC_id));
                        } else {
                            send_symbol_ex(direction,
                                    xran_alloc_sectionid(pHandle, direction, cc_id, ant_elm_eAxC_id, slot_id),
                                    (struct rte_mbuf *)mb,
                                    (struct rb_map *)pos,
                                    p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder,
                                    frame_id, subframe_id, slot_id, sym_id,
                                    0, prb_num,
                                    cc_id, ant_elm_eAxC_id,
                                    (p_xran_dev_ctx->fh_init.io_cfg.id == O_DU) ?
                                        xran_get_updl_seqid(pHandle, cc_id, ant_elm_eAxC_id) :
                                        xran_get_upul_seqid(pHandle, cc_id, ant_elm_eAxC_id));
                            retval = 1;
                        }
                    } /* for ant elem */
                } /* SRS symbol */
            } /* SRS enabled */
        } /* RU mode or C-Plane is not used */
    }

    return retval;
}


int32_t xran_process_tx_sym_cp_on(uint8_t ctx_id, uint32_t tti, int32_t cc_id, int32_t ant_id, uint32_t frame_id, uint32_t subframe_id,
    uint32_t slot_id, uint32_t sym_id)
{
    int32_t     retval = 0;
    uint64_t    t1 = MLogTick();

    struct rte_mbuf *eth_oran_hdr = NULL;
    char        *ext_buff = NULL;
    uint16_t    ext_buff_len = 0;
    struct rte_mbuf *tmp = NULL;
    rte_iova_t ext_buff_iova = 0;
    void        *pHandle  = NULL;
    char        *pos      = NULL;
    char        *p_sec_iq = NULL;
    void        *mb  = NULL;
    int         prb_num = 0;
    uint16_t    iq_sample_size_bits = 16; // TODO: make dynamic per
    uint32_t    next = 0;
    int32_t     num_sections = 0;

    struct xran_section_info *sectinfo = NULL;
    struct xran_device_ctx   *p_xran_dev_ctx = xran_dev_get_ctx();

    struct xran_prach_cp_config *pPrachCPConfig = &(p_xran_dev_ctx->PrachCPConfig);
    struct xran_srs_config *p_srs_cfg = &(p_xran_dev_ctx->srs_cfg);
    enum xran_pkt_dir direction;

    struct rte_mbuf_ext_shared_info * p_share_data = &share_data[tti % XRAN_N_FE_BUF_LEN];

    if(p_xran_dev_ctx->fh_init.io_cfg.id == O_DU) {
        direction = XRAN_DIR_DL; /* O-DU */
        prb_num = p_xran_dev_ctx->fh_cfg.nDLRBs;
    } else {
        direction = XRAN_DIR_UL; /* RU */
        prb_num = p_xran_dev_ctx->fh_cfg.nULRBs;
    }

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

        if(sectinfo->compMeth)
            iq_sample_size_bits = sectinfo->iqWidth;

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

#if 1
        p_sec_iq = ((char*)pos + sectinfo->sec_desc[sym_id].iq_buffer_offset);

        /* calculete offset for external buffer */
        ext_buff_len = sectinfo->sec_desc[sym_id].iq_buffer_len;
        ext_buff = p_sec_iq - (RTE_PKTMBUF_HEADROOM +
                        sizeof (struct xran_ecpri_hdr) +
                        sizeof (struct radio_app_common_hdr) +
                        sizeof(struct data_section_hdr));

        ext_buff_len += RTE_PKTMBUF_HEADROOM +
                        sizeof (struct xran_ecpri_hdr) +
                        sizeof (struct radio_app_common_hdr) +
                        sizeof(struct data_section_hdr) + 18;

        if(sectinfo->compMeth != XRAN_COMPMETHOD_NONE){
            ext_buff     -= sizeof (struct data_section_compression_hdr);
            ext_buff_len += sizeof (struct data_section_compression_hdr);
        }

        eth_oran_hdr =  rte_pktmbuf_alloc(_eth_mbuf_pool_small);

        if (unlikely (( eth_oran_hdr) == NULL)) {
            rte_panic("Failed rte_pktmbuf_alloc\n");
        }

        p_share_data->free_cb = extbuf_free_callback;
        p_share_data->fcb_opaque = NULL;
        rte_mbuf_ext_refcnt_set(p_share_data, 1);

        ext_buff_iova = rte_mempool_virt2iova(mb);
        if (unlikely (( ext_buff_iova) == 0)) {
            rte_panic("Failed rte_mem_virt2iova \n");
        }

        if (unlikely (( (rte_iova_t)ext_buff_iova) == RTE_BAD_IOVA)) {
            rte_panic("Failed rte_mem_virt2iova RTE_BAD_IOVA \n");
        }

        rte_pktmbuf_attach_extbuf(eth_oran_hdr,
                                  ext_buff,
                                  ext_buff_iova + RTE_PTR_DIFF(ext_buff , mb),
                                  ext_buff_len,
                                  p_share_data);

        rte_pktmbuf_reset_headroom(eth_oran_hdr);

        tmp = (struct rte_mbuf *)rte_pktmbuf_prepend(eth_oran_hdr, sizeof(struct ether_hdr));
        if (unlikely (( tmp) == NULL)) {
            rte_panic("Failed rte_pktmbuf_prepend \n");
        }
        mb = eth_oran_hdr;
#else
        rte_pktmbuf_refcnt_update(mb, 1); /* make sure eth won't free our mbuf */
#endif
        /* first all PRBs */
        prepare_symbol_ex(direction, sectinfo->id,
                          mb,
                          (struct rb_map *)p_sec_iq,
                          sectinfo->compMeth,
                          sectinfo->iqWidth,
                          p_xran_dev_ctx->fh_cfg.ru_conf.byteOrder,
                          frame_id, subframe_id, slot_id, sym_id,
                          sectinfo->startPrbc, sectinfo->numPrbc,
                          cc_id, ant_id,
                          xran_get_updl_seqid(pHandle, cc_id, ant_id),
                          0);

        /* if we don't need to do any fragmentation */
        if (likely (p_xran_dev_ctx->fh_init.mtu >=
                        sectinfo->numPrbc * (3*iq_sample_size_bits + 1))) {
            /* no fragmentation */
            p_xran_dev_ctx->tx_mbufs[0].m_table[len] = mb;
            len2 = 1;
        } else {
            /* fragmentation */
            uint8_t * seq_num = xran_get_updl_seqid_addr(pHandle, cc_id, ant_id);
            if(seq_num)
                (*seq_num)--;
            else
                rte_panic("pointer to seq number is NULL [CC %d Ant %d]\n", cc_id, ant_id);

            len2 = xran_app_fragment_packet(mb,
                                        &p_xran_dev_ctx->tx_mbufs[0].m_table[len],
                                        (uint16_t)(MBUF_TABLE_SIZE - len),
                                        p_xran_dev_ctx->fh_init.mtu,
                                        p_xran_dev_ctx->direct_pool,
                                        p_xran_dev_ctx->indirect_pool,
                                        sectinfo,
                                        seq_num);

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
        retval = 1;
    } /* while(section) */

    return retval;
}

int32_t xran_process_tx_sym(void *arg)
{
    int32_t     retval = 0;
    uint32_t    tti=0;
#if XRAN_MLOG_VAR
    uint32_t    mlogVar[10];
    uint32_t    mlogVarCnt = 0;
#endif
    unsigned long t1 = MLogTick();

    void        *pHandle = NULL;
    int32_t     ant_id   = 0;
    int32_t     cc_id    = 0;
    uint8_t     num_eAxc = 0;
    uint8_t     num_CCPorts = 0;
    uint8_t     num_ant_elm = 0;
    uint32_t    frame_id    = 0;
    uint32_t    subframe_id = 0;
    uint32_t    slot_id     = 0;
    uint32_t    sym_id      = 0;
    uint32_t    sym_idx     = 0;

    uint8_t     ctx_id;
    enum xran_pkt_dir  direction;
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();

    if(p_xran_dev_ctx->xran2phy_mem_ready == 0)
        return 0;

    /* O-RU: send symb after OTA time with delay (UL) */
    /* O-DU: send symb in advance of OTA time (DL) */
    sym_idx     = XranOffsetSym(p_xran_dev_ctx->sym_up, xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT*SLOTNUM_PER_SUBFRAME*1000);

    tti         = XranGetTtiNum(sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
    slot_id     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME);
    subframe_id = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME,  SUBFRAMES_PER_SYSTEMFRAME);
    frame_id    = XranGetFrameNum(tti,xran_getSfnSecStart(),SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME);
    // ORAN frameId, 8 bits, [0, 255]
    frame_id = (frame_id & 0xff);

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

    if(p_xran_dev_ctx->fh_init.io_cfg.id == O_RU && xran_get_ru_category(pHandle) == XRAN_CATEGORY_B) {
            num_eAxc    = xran_get_num_eAxcUl(pHandle);
    } else {
            num_eAxc    = xran_get_num_eAxc(pHandle);
    }

    num_CCPorts = xran_get_num_cc(pHandle);
    /* U-Plane */
    for(ant_id = 0; ant_id < num_eAxc; ant_id++) {
        for(cc_id = 0; cc_id < num_CCPorts; cc_id++) {
            if(p_xran_dev_ctx->fh_init.io_cfg.id == O_DU && p_xran_dev_ctx->enableCP){
                retval = xran_process_tx_sym_cp_on(ctx_id, tti, cc_id, ant_id, frame_id, subframe_id, slot_id, sym_id);
            } else {
                retval = xran_process_tx_sym_cp_off(ctx_id, tti, cc_id, ant_id, frame_id, subframe_id, slot_id, sym_id, (ant_id == (num_eAxc - 1)));
            }
        } /* for(cc_id = 0; cc_id < num_CCPorts; cc_id++) */
    } /* for(ant_id = 0; ant_id < num_eAxc; ant_id++) */

    MLogTask(PID_PROCESS_TX_SYM, t1, MLogTick());
    return retval;
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


int32_t xran_init(int argc, char *argv[],
           struct xran_fh_init *p_xran_fh_init, char *appName, void ** pXranLayerHandle)
{
    int32_t i;
    int32_t j;

    struct xran_io_loop_cfg *p_io_cfg = (struct xran_io_loop_cfg *)&p_xran_fh_init->io_cfg;
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();

    int32_t  lcore_id = 0;
    char filename[64];
    int64_t offset_sec, offset_nsec;

    memset(p_xran_dev_ctx, 0, sizeof(struct xran_device_ctx));

    /* copy init */
    p_xran_dev_ctx->fh_init = *p_xran_fh_init;

    printf(" %s: MTU %d\n", __FUNCTION__, p_xran_dev_ctx->fh_init.mtu);

    xran_if_current_state = XRAN_INIT;

    memcpy(&(p_xran_dev_ctx->eAxc_id_cfg), &(p_xran_fh_init->eAxCId_conf), sizeof(struct xran_eaxcid_config));

    p_xran_dev_ctx->enableCP    = p_xran_fh_init->enableCP;
    p_xran_dev_ctx->enablePrach = p_xran_fh_init->prachEnable;
    p_xran_dev_ctx->enableSrs   = p_xran_fh_init->srsEnable;
    p_xran_dev_ctx->DynamicSectionEna = p_xran_fh_init->DynamicSectionEna;

    /* To make sure to set default functions */
    p_xran_dev_ctx->send_upmbuf2ring    = NULL;
    p_xran_dev_ctx->send_cpmbuf2ring    = NULL;

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

    for (i = 0; i< XRAN_MAX_SECTOR_NR; i++){
        for (j = 0; j< XRAN_NUM_OF_SYMBOL_PER_SLOT; j++){
            LIST_INIT (&p_xran_dev_ctx->sym_cb_list_head[i][j]);
        }
    }

    printf("Set debug stop %d, debug stop count %d\n", p_xran_fh_init->debugStop, p_xran_fh_init->debugStopCount);
    timing_set_debug_stop(p_xran_fh_init->debugStop, p_xran_fh_init->debugStopCount);

    for (uint32_t nCellIdx = 0; nCellIdx < XRAN_MAX_SECTOR_NR; nCellIdx++){
        xran_fs_clear_slot_type(nCellIdx);
    }

    *pXranLayerHandle = p_xran_dev_ctx;

    if(p_xran_fh_init->GPS_Alpha || p_xran_fh_init->GPS_Beta ){
        offset_sec = p_xran_fh_init->GPS_Beta / 100;    //resolution of beta is 10ms
        offset_nsec = (p_xran_fh_init->GPS_Beta - offset_sec * 100) * 1e7 + p_xran_fh_init->GPS_Alpha;
        p_xran_dev_ctx->offset_sec = offset_sec;
        p_xran_dev_ctx->offset_nsec = offset_nsec;
    }else {
        p_xran_dev_ctx->offset_sec  = 0;
        p_xran_dev_ctx->offset_nsec = 0;
    }

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

    if(nAllocBufferSize >= UINT16_MAX) {
        rte_panic("nAllocBufferSize is failed [ handle %p %d %d ] [nPoolIndex %d] nNumberOfBuffers %d nBufferSize %d nAllocBufferSize %d\n",
                    pXranCc, pXranCc->nXranPort, pXranCc->nIndex, pXranCc->nBufferPoolIndex, nNumberOfBuffers, nBufferSize, nAllocBufferSize);
        return -1;
    }

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
        } else {
            print_err("[nPoolIndex %d] start ethhdr failed \n", nPoolIndex );
            return -1;
        }
    } else {
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


int32_t xran_5g_srs_req (void *  pHandle,
                struct xran_buffer_list *pDstBuffer[XRAN_MAX_ANT_ARRAY_ELM_NR][XRAN_N_FE_BUF_LEN],
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
        for(z = 0; z < XRAN_MAX_ANT_ARRAY_ELM_NR; z++){
           p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[j][i][z].bValid = 0;
           p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
           p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
           p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
           p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_MAX_ANT_ARRAY_ELM_NR; // ant number.
           p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &p_xran_dev_ctx->sFHSrsRxBuffers[j][i][z][0];
           p_xran_dev_ctx->sFHSrsRxBbuIoBufCtrl[j][i][z].sBufferList =   *pDstBuffer[z][j];
        }
    }

    p_xran_dev_ctx->pSrsCallback[i]    = pCallback;
    p_xran_dev_ctx->pSrsCallbackTag[i] = pCallbackTag;

    return 0;
}

uint32_t xran_get_time_stats(uint64_t *total_time, uint64_t *used_time, uint32_t *core_used, uint32_t clear)
{
    *total_time = xran_total_tick;
    *used_time = xran_used_tick;
    *core_used = xran_core_used;

    if (clear)
    {
        xran_total_tick = 0;
        xran_used_tick = 0;
    }

    return 0;
}

void * xran_malloc(size_t buf_len)
{
    return rte_malloc("External buffer", buf_len, RTE_CACHE_LINE_SIZE);
}

uint8_t  *xran_add_hdr_offset(uint8_t  *dst, int16_t compMethod)
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

int32_t xran_open(void *pHandle, struct xran_fh_config* pConf)
{
    int32_t i;
    uint8_t nNumerology = 0;
    int32_t  lcore_id = 0;
    struct xran_device_ctx * p_xran_dev_ctx = xran_dev_get_ctx();
    struct xran_fh_config *pFhCfg;
    pFhCfg = &(p_xran_dev_ctx->fh_cfg);

    memcpy(pFhCfg, pConf, sizeof(struct xran_fh_config));

    if(pConf->log_level)
        printf(" %s: O-RU Category %s\n", __FUNCTION__, (pFhCfg->ru_conf.xranCat == XRAN_CATEGORY_A) ? "A" : "B");

    nNumerology = xran_get_conf_numerology(pHandle);

    if (pConf->nCC > XRAN_MAX_SECTOR_NR)
    {
        if(pConf->log_level)
            printf("Number of cells %d exceeds max number supported %d!\n", pConf->nCC, XRAN_MAX_SECTOR_NR);
        pConf->nCC = XRAN_MAX_SECTOR_NR;

    }
    if(pConf->ru_conf.iqOrder != XRAN_I_Q_ORDER
        || pConf->ru_conf.byteOrder != XRAN_NE_BE_BYTE_ORDER ){

        print_err("Byte order and/or IQ order is not supported [IQ %d byte %d]\n", pConf->ru_conf.iqOrder, pConf->ru_conf.byteOrder);
        return XRAN_STATUS_FAIL;
    }

    /* setup PRACH configuration for C-Plane */
    xran_init_prach(pConf, p_xran_dev_ctx);
    xran_init_srs(pConf, p_xran_dev_ctx);

    xran_cp_init_sectiondb(pHandle);
    xran_init_sectionid(pHandle);
    xran_init_seqid(pHandle);

    if(pConf->ru_conf.xran_max_frame) {
       xran_max_frame = pConf->ru_conf.xran_max_frame;
       printf("xran_max_frame %d\n", xran_max_frame);
    }

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

    /* if send_xpmbuf2ring needs to be changed from default functions,
     * then those should be set between xran_init and xran_open */
    if(p_xran_dev_ctx->send_cpmbuf2ring == NULL)
        p_xran_dev_ctx->send_cpmbuf2ring    = xran_ethdi_mbuf_send_cp;
    if(p_xran_dev_ctx->send_upmbuf2ring == NULL)
        p_xran_dev_ctx->send_upmbuf2ring    = xran_ethdi_mbuf_send;

    /* Start packet processing thread */
    if((uint16_t)xran_ethdi_get_ctx()->io_cfg.port[XRAN_UP_VF] != 0xFFFF &&
        (uint16_t)xran_ethdi_get_ctx()->io_cfg.port[XRAN_CP_VF] != 0xFFFF ){
        if(pConf->log_level){
            print_dbg("XRAN_UP_VF: 0x%04x\n", xran_ethdi_get_ctx()->io_cfg.port[XRAN_UP_VF]);
            print_dbg("XRAN_CP_VF: 0x%04x\n", xran_ethdi_get_ctx()->io_cfg.port[XRAN_CP_VF]);
        }
        if (rte_eal_remote_launch(xran_timing_source_thread, xran_dev_get_ctx(), xran_ethdi_get_ctx()->io_cfg.timing_core))
            rte_panic("thread_run() failed to start\n");
    } else if(pConf->log_level){
            printf("Eth port was not open. Processing thread was not started\n");
    }

    return 0;
}

int32_t xran_start(void *pHandle)
{
    if(xran_get_if_state() == XRAN_RUNNING) {
        print_err("Already STARTED!!");
        return (-1);
        }

    xran_if_current_state = XRAN_RUNNING;
    return 0;
}

int32_t xran_stop(void *pHandle)
{
    if(xran_get_if_state() == XRAN_STOPPED) {
        print_err("Already STOPPED!!");
        return (-1);
        }

    xran_if_current_state = XRAN_STOPPED;
    return 0;
}

int32_t xran_close(void *pHandle)
{
    xran_if_current_state = XRAN_STOPPED;
    //TODO: fix memory leak xran_cp_free_sectiondb(pHandle);
    //rte_eal_mp_wait_lcore();
    //xran_ethdi_ports_stats();

#ifdef RTE_LIBRTE_PDUMP
    /* uninitialize packet capture framework */
    rte_pdump_uninit();
#endif
    return 0;
}

int32_t xran_mm_destroy (void * pHandle)
{
    if(xran_get_if_state() == XRAN_RUNNING) {
        print_err("Please STOP first !!");
        return (-1);
        }

    /* functionality is not yet implemented */
    return -1;
}

int32_t xran_reg_sym_cb(void *pHandle, xran_callback_sym_fn symCb, void * symCbParam, uint8_t symb,  uint8_t ant)
{
    if(xran_get_if_state() == XRAN_RUNNING) {
        print_err("Cannot register callback while running!!\n");
        return (-1);
        }

    /* functionality is not yet implemented */
    print_err("Functionality is not yet implemented !");
    return -1;
}

int32_t xran_reg_physide_cb(void *pHandle, xran_fh_tti_callback_fn Cb, void *cbParam, int skipTtiNum, enum callback_to_phy_id id)
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

/* send_cpmbuf2ring and send_upmbuf2ring should be set between xran_init and xran_open
 * each cb will be set by default duing open if it is set by NULL */
int xran_register_cb_mbuf2ring(xran_ethdi_mbuf_send_fn mbuf_send_cp, xran_ethdi_mbuf_send_fn mbuf_send_up)
{
    struct xran_device_ctx *p_xran_dev_ctx;

    if(xran_get_if_state() == XRAN_RUNNING) {
        print_err("Cannot register callback while running!!\n");
        return (-1);
        }

    p_xran_dev_ctx = xran_dev_get_ctx();

    p_xran_dev_ctx->send_cpmbuf2ring    = mbuf_send_cp;
    p_xran_dev_ctx->send_upmbuf2ring    = mbuf_send_up;

    return (0);
}


int32_t xran_get_slot_idx (uint32_t *nFrameIdx, uint32_t *nSubframeIdx,  uint32_t *nSlotIdx, uint64_t *nSecond)
{
    int32_t tti = 0;

    tti           = (int32_t)XranGetTtiNum(xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
    *nSlotIdx     = (uint32_t)XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME);
    *nSubframeIdx = (uint32_t)XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME,  SUBFRAMES_PER_SYSTEMFRAME);
    *nFrameIdx    = (uint32_t)XranGetFrameNum(tti,xran_getSfnSecStart(),SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME);
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
 * @brief Get the configuration of the total number of beamforming weights on RU
 *
 * @return Configured the number of beamforming weights
 */
inline uint8_t xran_get_conf_num_bfweights(void *pHandle)
{
    return (xran_dev_get_ctx()->fh_init.totalBfWeights);
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
 * @brief Get the configuration of the number of antenna for UL
 *
 * @return Configured the number of antenna
 */
inline uint8_t xran_get_num_eAxc(void *pHandle)
{
    return (xran_lib_get_ctx_fhcfg()->neAxc);
}

/**
 * @brief Get configuration of O-RU (Cat A or Cat B)
 *
 * @return Configured the number of antenna
 */
inline enum xran_category xran_get_ru_category(void *pHandle)
{
    return (xran_lib_get_ctx_fhcfg()->ru_conf.xranCat);
}

/**
 * @brief Get the configuration of the number of antenna
 *
 * @return Configured the number of antenna
 */
inline uint8_t xran_get_num_eAxcUl(void *pHandle)
{
    return (xran_lib_get_ctx_fhcfg()->neAxcUl);
}

/**
 * @brief Get the configuration of the number of antenna elements
 *
 * @return Configured the number of antenna
 */
inline uint8_t xran_get_num_ant_elm(void *pHandle)
{
    return (xran_lib_get_ctx_fhcfg()->nAntElmTRx);
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

