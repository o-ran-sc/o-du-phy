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
#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <malloc.h>
#include <immintrin.h>
#include <numa.h>
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
#include "xran_fh_o_ru.h"
#include "xran_main.h"

#include "xran_ethdi.h"
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
#include "xran_cp_proc.h"
#include "xran_tx_proc.h"
#include "xran_rx_proc.h"
#include "xran_cb_proc.h"
#include "xran_ecpri_owd_measurements.h"

#include "xran_mlog_lnx.h"

static xran_cc_handle_t pLibInstanceHandles[XRAN_PORTS_NUM][XRAN_MAX_SECTOR_NR] = {{NULL}};

#define XRAN_CHECK_ACTIVECC_CB 0    /* Do not enable this */

#ifdef POLL_EBBU_OFFLOAD
XRAN_TIMER_CTX gXranTmrCtx = {0};
#endif
uint64_t interval_us = 1000; //the TTI interval of the cell with maximum numerology

uint32_t xran_lib_ota_sym_idx[XRAN_PORTS_NUM] = {0,0,0,0,0,0,0,0}; /**< Symbol index in a second [0 : 14*(1000000/TTI)-1]
                                                where TTI is TTI interval in microseconds */
uint32_t xran_lib_ota_tti_base = 0; /* For Baseline TTI */
/* For mixed-numerology */
/* Running counter for 'current slot' in a second for all configured numerologies.
 * It gets reset to zero every second (i.e. after second changes).
 * Slot index in a second [0:(1000000/TTI-1)]
 */
uint32_t xran_lib_ota_tti_mu[XRAN_PORTS_NUM][XRAN_MAX_NUM_MU] = { 0 };

/* Running counter for 'current symbol' in a second for all configured numerologies.
 * It gets reset to zero every second (i.e. after second changes).
 * [0 : 14*(1000000/TTI)-1] where TTI is TTI interval in microseconds.
 */
uint32_t xran_lib_ota_sym_idx_mu[XRAN_MAX_NUM_MU] = {0};

/**< Symbol index in a slot [0:13] */
uint32_t xran_lib_ota_sym_mu[XRAN_MAX_NUM_MU] = {0};

/**< Intra sym division counter [0:XRAN_INTRA_SYM_MAX_DIV] */
uint8_t xran_intra_sym_div[XRAN_MAX_NUM_MU] = {0};

/* -- for mixed-numerology */
uint16_t xran_SFN_at_Sec_Start   = 0; /**< SFN at current second start */
uint16_t xran_max_frame          = 1023; /**< value of max frame used. expected to be 99 (old compatibility mode) and 1023 as per section 9.7.2    System Frame Number Calculation */

int32_t first_call = 0;
int32_t mlogxranenable = 0;

struct cp_up_tx_desc * xran_pkt_gen_desc_alloc(void);
int32_t xran_pkt_gen_desc_free(struct cp_up_tx_desc *p_desc);

int32_t xran_pkt_gen_process_ring(struct rte_ring *r);
int32_t xran_dl_pkt_ring_processing_func(void* args);

void
xran_updateSfnSecStart(void)
{
#ifdef POLL_EBBU_OFFLOAD
    PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();
    uint64_t currentSecond = pCtx->current_second;
#else
    uint64_t currentSecond = xran_timingsource_get_current_second();
#endif
    // Assume always positive
    uint64_t gpsSecond = currentSecond - UNIX_TO_GPS_SECONDS_OFFSET;
    uint64_t nFrames = gpsSecond * NUM_OF_FRAMES_PER_SECOND;
    uint16_t sfn = (uint16_t)(nFrames % (xran_max_frame + 1));
#ifdef POLL_EBBU_OFFLOAD
    pCtx->xran_SFN_at_Sec_Start = sfn;
#else
    xran_SFN_at_Sec_Start = sfn;
#endif
}

#ifdef POLL_EBBU_OFFLOAD
struct xran_common_counters* xran_fh_counters_ebbu_offload(void)
{
    struct xran_device_ctx * p_xran_dev_ctx = XRAN_GET_DEV_CTX ;
    struct xran_common_counters * pCnt = &p_xran_dev_ctx->fh_counters;

    return pCnt;
}
#endif

#if 0
static inline int32_t
xran_getSlotIdxSecond(uint32_t interval)
{
    int32_t frameIdxSecond = xran_getSfnSecStart();
    int32_t slotIndxSecond = frameIdxSecond * SLOTS_PER_SYSTEMFRAME(interval);
    return slotIndxSecond;
}
#endif

enum xran_if_state
xran_get_if_state(void)
{
    return xran_if_current_state;
}

int32_t xran_is_prach_slot(uint8_t PortId, uint32_t sfId, uint32_t slotId, uint8_t mu)
{
    int32_t is_prach_slot = 0;
    struct xran_device_ctx * pDevCtx = xran_dev_get_ctx_by_id(PortId);
    if (pDevCtx == NULL)
    {
        print_err("PortId %d not exist\n", PortId);
        return is_prach_slot;
    }
    struct xran_prach_cp_config *pPrachCPConfig = &(pDevCtx->perMu[mu].PrachCPConfig);
    if(mu == XRAN_DEFAULT_MU)
        mu = pDevCtx->fh_cfg.mu_number[0];

    uint8_t nNumerology = mu; //pDevCtx->fh_cfg.mu_number[0];

    if (nNumerology < 2){
        //for FR1, in 38.211 tab 6.3.3.2-2&3 it is subframe index
        if (pPrachCPConfig->isPRACHslot[sfId] == 1){
            if (pPrachCPConfig->nrofPrachInSlot == 0){
                if(slotId == 0)
                    is_prach_slot = 1;
            }
            else if (pPrachCPConfig->nrofPrachInSlot == 2)
                is_prach_slot = 1;
            else{
                if (nNumerology == 0)
                    is_prach_slot = 1;
                else if (slotId == 1)
                    is_prach_slot = 1;
            }
        }
    } else if (nNumerology == 3){
        //for FR2, 38.211 tab 6.3.3.4 it is slot index of 60kHz slot
        uint32_t slotidx;
        slotidx = sfId * SLOTNUM_PER_SUBFRAME(xran_fs_get_tti_interval(nNumerology)) + slotId;
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
xran_init_srs(struct xran_fh_config* pConf, struct xran_device_ctx * pDevCtx)
{
    struct xran_srs_config *p_srs = &(pDevCtx->srs_cfg);

    if(p_srs){
        p_srs->symbMask = pConf->srs_conf.symbMask;     /* deprecated */
        p_srs->slot             = pConf->srs_conf.slot;
        p_srs->ndm_offset       = pConf->srs_conf.ndm_offset;
        p_srs->ndm_txduration   = pConf->srs_conf.ndm_txduration;
        p_srs->srsEaxcOffset    = pConf->srs_conf.srsEaxcOffset;

        print_dbg("SRS sym         %d\n", p_srs->slot);
        print_dbg("SRS NDM offset  %d\n", p_srs->ndm_offset);
        print_dbg("SRS NDM Tx      %d\n", p_srs->ndm_txduration);
        print_dbg("SRS eAxC_offset %d\n", p_srs->srsEaxcOffset);
    }
    return (XRAN_STATUS_SUCCESS);
}

int32_t xran_init_prach_lte(struct xran_fh_config* pConf, struct xran_device_ctx * pDevCtx)
{
    const struct xran_lte_prach_config_table *pPrachCfgTbl;
    struct xran_prach_config *pPRACHConfig;
    struct xran_prach_cp_config *pPrachCPConfig;
    const struct xran_lte_prach_preambleformat_table *pPreambleFormat;
    uint8_t nPrachConfIdx;
    uint8_t preambleFormat;
    int32_t i, offset;

    pPRACHConfig    = &(pConf->perMu[0].prach_conf);
    nPrachConfIdx   = pPRACHConfig->nPrachConfIdx;
    pPrachCPConfig  = &(pDevCtx->perMu[0].PrachCPConfig);

    if(pConf->frame_conf.nFrameDuplexType == XRAN_FDD)
        pPrachCfgTbl = &gxranPrachDataTable_lte_fs1[nPrachConfIdx];
    else
    {
        printf("TDD is not supported yet.\n");
        pPrachCfgTbl = NULL;
        return (XRAN_STATUS_INVALID_PARAM);
    }

    if(pPrachCfgTbl->prachConfigIdx < 0)
    {
        printf("Invalid PRACH Configuration Index - %d\n", pPrachCfgTbl->prachConfigIdx);
        return (XRAN_STATUS_INVALID_PARAM);
    }

    printf("Setup PRACH configuration for LTE\n");
    memset(pPrachCPConfig, 0, sizeof(struct xran_prach_cp_config));

    preambleFormat = pPrachCfgTbl->preambleFmrt;
    switch(preambleFormat)
    {
        case FORMAT_0:
        case FORMAT_1:
        case FORMAT_2:
            pPrachCPConfig->filterIdx = XRAN_FILTERINDEX_PRACH_012;
            break;
        case FORMAT_3:
            pPrachCPConfig->filterIdx = XRAN_FILTERINDEX_PRACH_3;
            break;
        case FORMAT_4:
            pPrachCPConfig->filterIdx = XRAN_FILTERINDEX_LTE4;
            preambleFormat = 4;
            break;
        default:
            printf("Invalid Preamble Format - %d\n", preambleFormat);
            return (XRAN_STATUS_INVALID_PARAM);
    }

    pPreambleFormat = &gxranLtePreambleFormat[preambleFormat];

    pPrachCPConfig->startSymId  = 0;
    pPrachCPConfig->startPrbc   = 0;//pPRACHConfig->nPrachFreqStart;
    pPrachCPConfig->numPrbc     = 72;
    pPrachCPConfig->timeOffset  = pPreambleFormat->Tcp;
    /* The freqOffset for xRAN is defined relative to the center frequency measured in 1/2 PRACH subcarrier spacing
     *  while the MAC offset is relative to the lower band and is in PRBs.
     * Therefore the bottom edge is:
     *      -nULRBs / 2(RB) * 12 (SC/RB) * 12 (PRACH SC/SC) * 2 (halfSC/SC)
     * and then the MAC RACH offset is added. */
    offset = pPRACHConfig->nPrachFreqOffset
                - pConf->perMu[0].nULRBs * N_SC_PER_PRB * 12
                + pPRACHConfig->nPrachFreqStart * N_SC_PER_PRB * 12 * 2;
    pPrachCPConfig->freqOffset  = offset;
    pPrachCPConfig->x           = pPrachCfgTbl->frameNum;   /* even or any */
    pPrachCPConfig->y[0]        = 0;
    pPrachCPConfig->y[1]        = 0;
    pPrachCPConfig->nrofPrachInSlot = 0;
    pPrachCPConfig->numSymbol   = 1;
    pPrachCPConfig->occassionsInPrachSlot = 1;

    /* only use 10 subframes for LTE */
    for(i=0; i < SUBFRAMES_PER_SYSTEMFRAME; i++)
        pPrachCPConfig->isPRACHslot[i] = pPrachCfgTbl->sfnNum[i];


    for (i = 0; i < XRAN_MAX_SECTOR_NR; i++)
    {
        pDevCtx->prach_start_symbol[i] = pPrachCPConfig->startSymId;
        pDevCtx->prach_last_symbol[i] = pPrachCPConfig->startSymId + pPrachCPConfig->numSymbol * pPrachCPConfig->occassionsInPrachSlot - 1;
    }

    if(pConf->log_level)
    {
        printf("xRAN open PRACH config(LTE): ConfIdx %u, preambleFmrt %u, systemframe %u\n",
                    nPrachConfIdx, preambleFormat, pPrachCfgTbl->frameNum);
        printf("PRACH: x %u, y[0] %u, y[1] %u ... [ ",
                    pPrachCPConfig->x, pPrachCPConfig->y[0], pPrachCPConfig->y[1]);
        for(i=0; i < SUBFRAMES_PER_SYSTEMFRAME; i++)
            printf("%d ", pPrachCPConfig->isPRACHslot[i]);
        printf("]\n");
        printf("PRACH: start symbol %u lastsymbol %u\n",
            pDevCtx->prach_start_symbol[0], pDevCtx->prach_last_symbol[0]);
    }

    pPrachCPConfig->prachEaxcOffset = xran_get_num_eAxc(pDevCtx);
    print_dbg("PRACH: eAxC_offset %d\n",  pPrachCPConfig->prachEaxcOffset);
    /* Save some configs for app */
    pPRACHConfig->startSymId    = pPrachCPConfig->startSymId;
    pPRACHConfig->lastSymId     = pPrachCPConfig->startSymId + pPrachCPConfig->numSymbol * pPrachCPConfig->occassionsInPrachSlot - 1;
    pPRACHConfig->startPrbc     = pPrachCPConfig->startPrbc;
    pPRACHConfig->numPrbc       = pPrachCPConfig->numPrbc;
    pPRACHConfig->timeOffset    = pPrachCPConfig->timeOffset;
    pPRACHConfig->freqOffset    = pPrachCPConfig->freqOffset;
    pPRACHConfig->prachEaxcOffset   = pPrachCPConfig->prachEaxcOffset;

    return (XRAN_STATUS_SUCCESS);
}

int32_t
xran_init_prach(struct xran_fh_config* pConf, struct xran_device_ctx * pDevCtx, enum xran_ran_tech xran_tech, uint8_t mu)
{
    int32_t i;
    uint8_t slotNr;
    struct xran_prach_config* pPRACHConfig = &(pConf->perMu[mu].prach_conf);
    const xRANPrachConfigTableStruct *pxRANPrachConfigTable;
    uint8_t nNumerology = mu;
    uint8_t nPrachConfIdx = -1;// = pPRACHConfig->nPrachConfIdx;
    struct xran_prach_cp_config *pPrachCPConfig = NULL;

    if(XRAN_NBIOT_MU==mu){
        pPrachCPConfig = &(pDevCtx->perMu[mu].PrachCPConfig); /*This should be mu of NB-IOT*/
        pPrachCPConfig->filterIdx = XRAN_FILTERINDEX_NPRACH;
        pPrachCPConfig->periodicity = pPRACHConfig->periodicity;
        pPrachCPConfig->nprachformat = pPRACHConfig->nprachformat;
        pPrachCPConfig->startTime = pPRACHConfig->startTime;
        pPrachCPConfig->suboffset = pPRACHConfig->suboffset;
        pPrachCPConfig->startPrbc = pPrachCPConfig->suboffset/SUBCARRIERS_PER_PRB;
        pPrachCPConfig->numSubCarriers = pPRACHConfig->numSubCarriers;
        pPrachCPConfig->numPrbc   = pPrachCPConfig->numSubCarriers/SUBCARRIERS_PER_PRB;
        pPrachCPConfig->nRep = pPRACHConfig->nRep;

        /*( N*P + Cyclic Prefix symbol time measured with 15 SCS)*/
        if(pPrachCPConfig->nprachformat == 0){
            pPrachCPConfig->numSymbol = 5; /* 3.75kHz symbols per SymbolGroup 4*5+1; N = 5 P=5 , CP = 67 us*/ //266us
            uint8_t sym=0;
            uint8_t sf = pPrachCPConfig->startTime/SUBFRAMES_PER_SYSTEMFRAME;

            // uint32_t symGroupDur = NBIOT_PRACH_F0_GROUP_DUR;
            uint16_t numSym = NBIOT_PRACH_F0_GROUP_DUR*1000/(xran_fs_get_tti_interval(mu)*1000/XRAN_NUM_OF_SYMBOL_PER_SLOT) + 1;
            for(uint8_t i=0; i<NBIOT_PRACH_NUM_SYM_GROUPS; i++)
            {
                pDevCtx->perMu[mu].sfSymToXmitPrach[i].sf = sf;
                pDevCtx->perMu[mu].sfSymToXmitPrach[i].sym = sym;

                sf += numSym/XRAN_NUM_OF_SYMBOL_PER_SLOT;
                sym += numSym%XRAN_NUM_OF_SYMBOL_PER_SLOT;
            }
        } else if (pPrachCPConfig->nprachformat == 1){
            pPrachCPConfig->numSymbol = 5; /* 3.75kHz symbols per SymbolGroup. 4*5+4; N = 5 P=5 , CP = 267 us*/
            uint8_t sym=0;
            uint8_t sf = pPrachCPConfig->startTime/SUBFRAMES_PER_SYSTEMFRAME;

            // uint32_t symGroupDur = NBIOT_PRACH_F1_GROUP_DUR;
            uint16_t numSym = NBIOT_PRACH_F1_GROUP_DUR*1000/(xran_fs_get_tti_interval(mu)*1000/XRAN_NUM_OF_SYMBOL_PER_SLOT) + 1;
            for(uint8_t i=0; i<NBIOT_PRACH_NUM_SYM_GROUPS; i++)
            {
                pDevCtx->perMu[mu].sfSymToXmitPrach[i].sf = sf;
                pDevCtx->perMu[mu].sfSymToXmitPrach[i].sym = sym;

                sf += numSym/XRAN_NUM_OF_SYMBOL_PER_SLOT;
                sym += numSym%XRAN_NUM_OF_SYMBOL_PER_SLOT;
            }
        } else {
            print_err("NPRACH preamble format not suported");
        }

    } /* nbiot */
    else {

        if(pConf->dssEnable){
            /*Check Slot type and */
            if(xran_tech == XRAN_RAN_5GNR){
                pPrachCPConfig = &(pDevCtx->perMu[mu].PrachCPConfig);
                nPrachConfIdx = pPRACHConfig->nPrachConfIdx;
            }
            else{
                pPrachCPConfig = &(pDevCtx->perMu[mu].PrachCPConfigLTE);
                nPrachConfIdx = pPRACHConfig->nPrachConfIdxLTE;
            }
        }
        else{
            pPrachCPConfig = &(pDevCtx->perMu[mu].PrachCPConfig);
            nPrachConfIdx = pPRACHConfig->nPrachConfIdx;
        }
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

        // pPrachCPConfig->filterIdx = XRAN_FILTERINDEX_PRACH_ABC;         // 3, PRACH preamble format A1~3, B1~4, C0, C2
        if (preambleFmrt <= 2)
        {
            pPrachCPConfig->filterIdx = XRAN_FILTERINDEX_PRACH_012;         // 1 PRACH preamble format 0 1 2
        }
        else if (preambleFmrt == 3)
        {
            pPrachCPConfig->filterIdx = XRAN_FILTERINDEX_PRACH_3;         // 1 PRACH preamble format 3
        }
        else
        {
            pPrachCPConfig->filterIdx = XRAN_FILTERINDEX_PRACH_ABC;         // 3, PRACH preamble format A1~3, B1~4, C0, C2
        }

        pPrachCPConfig->startSymId = pxRANPrachConfigTable->startingSym;
        pPrachCPConfig->startPrbc = pPRACHConfig->nPrachFreqStart;
        pPrachCPConfig->numPrbc = (preambleFmrt >= FORMAT_A1)? 12 : 72;  /*TODO: This should change for NB-IOT, only 1 PRB is to be allocated - check format accordingly*/
        pPrachCPConfig->timeOffset = pxranPreambleforLRA->nRaCp;
        pPrachCPConfig->freqOffset = xran_get_prach_freqoffset(pConf, mu, pPRACHConfig->nPrachSubcSpacing, pPRACHConfig->nPrachFreqStart);
        pPrachCPConfig->x = pxRANPrachConfigTable->x;
        pPrachCPConfig->nrofPrachInSlot = pxRANPrachConfigTable->nrofPrachInSlot;
        pPrachCPConfig->y[0] = pxRANPrachConfigTable->y[0];
        pPrachCPConfig->y[1] = pxRANPrachConfigTable->y[1];
        if (preambleFmrt >= FORMAT_A1)
        {
            if (preambleFmrt == FORMAT_C2)
                pPrachCPConfig->numSymbol = 4;//in FORMAT_C2, numSymbol=4, duration=6
            else if (preambleFmrt == FORMAT_C0)
                pPrachCPConfig->numSymbol = 1;//in FORMAT_C0, numSymbol=1, duration=2
            else
                pPrachCPConfig->numSymbol = pxRANPrachConfigTable->duration;
            pPrachCPConfig->occassionsInPrachSlot = pxRANPrachConfigTable->occassionsInPrachSlot;
			pPrachCPConfig->duration = pxRANPrachConfigTable->duration;
        }
        else
        {
            pPrachCPConfig->numSymbol = 1;
            pPrachCPConfig->occassionsInPrachSlot = 1;
			pPrachCPConfig->duration = 1;
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

    }/* non NB-IOT*/
    printf("\n");
    for (i = 0; i < XRAN_MAX_SECTOR_NR; i++){
        pDevCtx->prach_start_symbol[i] = pPrachCPConfig->startSymId;
        pDevCtx->prach_last_symbol[i] = pPrachCPConfig->startSymId + pPrachCPConfig->numSymbol * pPrachCPConfig->occassionsInPrachSlot - 1;
    }
    if(pConf->log_level){
        printf("PRACH start symbol %u lastsymbol %u\n", pDevCtx->prach_start_symbol[0], pDevCtx->prach_last_symbol[0]);
    }

    /* match the RU sample-app prachEaxcOffset with DU in case of numerology-3 */
    if(mu == 3)
        pPRACHConfig->prachEaxcOffset = RTE_MAX(xran_get_num_eAxc(pDevCtx), 4);

    if(pPRACHConfig->prachEaxcOffset==0)
    {
        pPrachCPConfig->prachEaxcOffset = pDevCtx->fh_cfg.perMu[mu].eaxcOffset + xran_get_num_eAxc(pDevCtx);
        pPRACHConfig->prachEaxcOffset = pPrachCPConfig->prachEaxcOffset;
    }
    else
    {
        pPrachCPConfig->prachEaxcOffset = pPRACHConfig->prachEaxcOffset;
    }

    print_dbg("PRACH eAxC_offset %d\n",  pPrachCPConfig->prachEaxcOffset);

    /* Save some configs for app */
    pPRACHConfig->startSymId    = pPrachCPConfig->startSymId;
    pPRACHConfig->lastSymId     = pPrachCPConfig->startSymId + pPrachCPConfig->numSymbol * pPrachCPConfig->occassionsInPrachSlot - 1;
    pPRACHConfig->startPrbc     = pPrachCPConfig->startPrbc;
    pPRACHConfig->numPrbc       = pPrachCPConfig->numPrbc;
    pPRACHConfig->timeOffset    = pPrachCPConfig->timeOffset;
    pPRACHConfig->freqOffset    = pPrachCPConfig->freqOffset;

    return (XRAN_STATUS_SUCCESS);
}

uint32_t
xran_slotid_convert(uint16_t slotId, uint16_t dir) //dir = 0, from PHY slotid to xran spec slotid as defined in 5.3.2, dir=1, from xran slotid to phy slotid
{
    return slotId;
#if 0
    struct xran_device_ctx * pDevCtx = XRAN_GET_DEV_CTX;
    uint8_t mu = pDevCtx->fh_cfg.frame_conf.mu_number[0];
    uint8_t FR = 1;
    if (mu > 2)
        FR=2;
    if (dir == 0)
    {
        if (FR == 1)
        {
            return (slotId << (2-mu));
        }
        else
        {
            return (slotId << (3-mu));
        }
    }
    else
    {
        if (FR == 1)
        {
            return (slotId >> (2-mu));
        }
        else
        {
            return (slotId >> (3-mu));
        }
    }
#endif
}

void xran_process_nbiot_prach_cp(struct xran_device_ctx * pDevCtx, uint8_t mu)
{
    int ret;
    struct xran_prach_cp_config *pPrachCPConfig = NULL;
    uint16_t interval_us_local = xran_fs_get_tti_interval(mu);

    uint16_t numSymsForT1a_max_cp_dl = (pDevCtx->fh_cfg.perMu[mu].T1a_max_cp_dl*1000)/(interval_us_local*1000/XRAN_NUM_OF_SYMBOL_PER_SLOT);

    uint8_t  sym_idx    = xran_lib_ota_sym_idx_mu[mu] +  numSymsForT1a_max_cp_dl;
    uint16_t tti        = XranGetTtiNum(sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
    uint8_t sfId = XranGetSubFrameNum(tti, SLOTNUM_PER_SUBFRAME(interval_us_local), SUBFRAMES_PER_SYSTEMFRAME);
    uint16_t frameId   = XranGetFrameNum(tti, xran_getSfnSecStart(), SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval_us_local));
    uint8_t sym_id      = XranGetSymNum(sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);

    pPrachCPConfig = &(pDevCtx->perMu[mu].PrachCPConfig);

    if((frameId %(pPrachCPConfig->periodicity/SUBFRAMES_PER_SYSTEMFRAME) == 0))
    {
        uint16_t num_eAxc = xran_get_num_eAxc(pDevCtx);
        uint16_t num_CCPorts = xran_get_num_cc(pDevCtx);

        for(uint8_t i=0; i<NBIOT_PRACH_NUM_SYM_GROUPS; i++)
        {
            if(pDevCtx->perMu[mu].sfSymToXmitPrach[i].sf ==sfId &&
                    pDevCtx->perMu[mu].sfSymToXmitPrach[i].sym==sym_id)
            {
                for(uint16_t antId = 0; antId < num_eAxc; antId++)
                {
                    uint8_t portId = antId + pPrachCPConfig->prachEaxcOffset;
                    for(uint16_t ccId = 0; ccId < num_CCPorts; ccId++)
                    {
                        if(!xran_isactive_cc(pDevCtx, ccId))
                            continue;

                        /* start new section information list */
                        //xran_cp_reset_section_info(pDevCtx, XRAN_DIR_UL, ccId, portId, ctxId, mu);

                        struct xran_cp_gen_params params;
                        struct xran_section_gen_info sect_geninfo[8];
                        struct xran_section_info sectInfo[8];
                        for(int secId=0;secId<8;secId++)
                            sect_geninfo[secId].info = &sectInfo[secId];

                        struct rte_mbuf *mbuf = xran_ethdi_mbuf_alloc();

                        uint8_t seqid = xran_get_cp_seqid(pDevCtx, XRAN_DIR_UL, ccId, portId);

                        uint16_t beam_id = xran_get_beamid(pDevCtx, XRAN_DIR_UL, ccId, portId, 0);
                        ret = generate_cpmsg_prach(pDevCtx, &params, sect_geninfo, mbuf, pDevCtx,
                                frameId, sfId, 0, tti,
                                beam_id, ccId, portId, 0, seqid, mu, sym_id);
                        if(ret == XRAN_STATUS_SUCCESS)
                        {
                            if(unlikely(xran_transmit_cpmsg_prach(pDevCtx, &params, sect_geninfo, mbuf, pDevCtx, tti, ccId, portId, seqid, mu) != XRAN_STATUS_SUCCESS)){
                                print_err("Error: PRACH CP transmit failed! ru_portId %d ccId %d tti %d\n", portId, ccId, tti);
                            }
                            /*TODO:  send_cpmsg allocates an entry into section-db that is shared with uplane.
                             * There is no Uplane for prach at DL. Hence this code should be disabled in send_cpmsg.
                             * Resetting the entry immediately for nprach here. but xran_cp_add_section_info
                             * should be removed from withing send_cpmsg
                             */
                            xran_cp_reset_section_info(pDevCtx, XRAN_DIR_UL, ccId, portId,
                             (0 + sfId*SLOTNUM_PER_SUBFRAME(xran_fs_get_tti_interval(mu)))%XRAN_MAX_SECTIONDB_CTX, mu);
                        }
                    } /*ccId*/
                }
            }
        }
    }
}


void sym_ota_cb(void *arg, unsigned long *used_tick, uint8_t mu)
{
    struct xran_device_ctx * pDevCtx = (struct xran_device_ctx *)arg;
    long t1 = MLogXRANTick(), t2;
    uint8_t ru_id;

    if(unlikely(xran_get_numactiveccs_ru(pDevCtx) == 0))
        return;

    ru_id = pDevCtx->xran_port_id;
    /* set xran_lib_ota_tti_mu[mu] to the ota slot for which we are processing. */
    if(XranGetSymNum(xran_lib_ota_sym_idx_mu[mu], XRAN_NUM_OF_SYMBOL_PER_SLOT) == 0)
    {
        struct xran_timer_ctx *pTCtx;

        pTCtx = (struct xran_timer_ctx *)pDevCtx->perMu[mu].timer_ctx;

        pTCtx[(xran_lib_ota_tti_mu[ru_id][mu] & 1) ^ 1].tti_to_process = xran_lib_ota_tti_mu[ru_id][mu];

        if(xran_get_syscfg_appmode() == O_DU)
        {
            pTCtx[(xran_lib_ota_tti_mu[ru_id][mu] & 1)].tti_to_process = xran_lib_ota_tti_mu[ru_id][mu] + 1;
        }
        else
        {
            pTCtx[(xran_lib_ota_tti_mu[ru_id][mu] & 1)].tti_to_process = pTCtx[(xran_lib_ota_tti_mu[ru_id][mu] & 1)^1].tti_to_process;
        }

        xran_lib_ota_tti_mu[ru_id][mu]++;
        if(xran_lib_ota_tti_mu[ru_id][mu] >= xran_fs_get_max_slot(mu))
            xran_lib_ota_tti_mu[ru_id][mu] = 0;
    }

    /* If the given numerology is not active in that slot - return. */
    uint8_t bufId = xran_lib_ota_tti_mu[ru_id][mu] % XRAN_N_FE_BUF_LEN;
    if(pDevCtx->fh_cfg.activeMUs->numerology[bufId][mu] == false)
    {
        //print_dbg("mu %u is not active\n", mu);
        return;
    }

    t2 = xran_tick();
    if(xran_process_tx_sym(pDevCtx, mu))
    {
        *used_tick += get_ticks_diff(xran_tick(), t2);
    }

    if(XRAN_NBIOT_MU==mu && pDevCtx->perMu[mu].enablePrach
            && xran_get_syscfg_appmode() == O_DU)
    {
        if(first_call && pDevCtx->enableCP)
            xran_process_nbiot_prach_cp(pDevCtx, mu);
    }

    /* check if there is call back to do something else on this symbol */
    struct cb_elem_entry *cb_elm;
    LIST_FOREACH(cb_elm, &pDevCtx->perMu[mu].sym_cb_list_head[xran_lib_ota_sym_mu[mu]], pointers)
    {
        if(cb_elm)
        {
            cb_elm->pSymCallback(&pDevCtx->dpdk_timer[pDevCtx->ctx % MAX_NUM_OF_DPDK_TIMERS],
                    cb_elm->pSymCallbackTag, cb_elm->p_dev_ctx, mu);

            pDevCtx->ctx = DpdkTimerIncrementCtx(pDevCtx->ctx);
        }
    }

    t2 = MLogXRANTick();
    MLogXRANTask(PID_SYM_OTA_CB, t1, t2);
}

#ifdef POLL_EBBU_OFFLOAD
void sym_ota_cb_ebbu_offload(void *arg, unsigned long *used_tick, uint8_t mu)
{
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)arg;
    PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();
    long t1 = MLogXRANTick(), t2;
    long t3;

    if(XranGetSymNum(pCtx->ebbu_offload_ota_sym_cnt_mu[mu], XRAN_NUM_OF_SYMBOL_PER_SLOT) == 0)
    {
        /* Update slot index to process for cplane for each numerology for this xran_device at the start fo the slot */
        t3 = xran_tick();
        tti_ota_cb_ebbu_offload(NULL, (void*)p_xran_dev_ctx, mu);
        *used_tick += get_ticks_diff(xran_tick(), t3);
    }

    /* tti_ota_cb_ebbu_offload will increment/set ebbu_offload_ota_tti_cnt_mu[mu] to the ota slot for which we are processing.
     * If the given numerology is not active in that slot - return.
     */
    uint8_t buf_id = pCtx->ebbu_offload_ota_tti_cnt_mu[p_xran_dev_ctx->xran_port_id][mu] % XRAN_N_FE_BUF_LEN;
    if(false == p_xran_dev_ctx->fh_cfg.activeMUs->numerology[buf_id][mu])
    {
        print_dbg("mu %u is not active\n", mu);
        return;
    }

    t3 = xran_tick();
    if (!xran_process_tx_sym(p_xran_dev_ctx, mu))
    {
        *used_tick += get_ticks_diff(xran_tick(), t3);
    }

    if(XRAN_NBIOT_MU==mu && p_xran_dev_ctx->perMu[mu].enablePrach
            && xran_get_syscfg_appmode() == O_DU)
    {
        if(pCtx->first_call && p_xran_dev_ctx->enableCP)
            xran_process_nbiot_prach_cp(p_xran_dev_ctx, mu);
    }

    /* check if there is call back to do something else on this symbol */
    struct cb_elem_entry *cb_elm;
    LIST_FOREACH(cb_elm, &p_xran_dev_ctx->perMu[mu].sym_cb_list_head[pCtx->ebbu_offload_ota_sym_idx_mu[mu]], pointers){
        if(cb_elm){
            cb_elm->pSymCallback(&p_xran_dev_ctx->dpdk_timer[p_xran_dev_ctx->ctx % MAX_NUM_OF_DPDK_TIMERS],
                    cb_elm->pSymCallbackTag, cb_elm->p_dev_ctx, mu);

            p_xran_dev_ctx->ctx = DpdkTimerIncrementCtx(p_xran_dev_ctx->ctx);
        }
    }

    t2 = MLogXRANTick();
    MLogXRANTask(PID_SYM_OTA_CB, t1, t2);
}
#endif

uint32_t
xran_schedule_to_worker(enum xran_job_type_id job_type_id, struct xran_device_ctx * pDevCtx)
{
    struct xran_ethdi_ctx* eth_ctx = xran_ethdi_get_ctx();
    uint32_t tim_lcore = eth_ctx->io_cfg.timing_core; /* default to timing core */

    if(eth_ctx->num_workers == 0) { /* no workers */
        tim_lcore = eth_ctx->io_cfg.timing_core;
    } else if (eth_ctx->num_workers == 1) { /* one worker */
        switch (job_type_id)
        {
            case XRAN_JOB_TYPE_OTA_CB:
                if(eth_ctx->io_cfg.bbu_offload)
                    tim_lcore = eth_ctx->worker_core[0];
                else
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
    } else if (eth_ctx->num_workers >= 2 && eth_ctx->num_workers <= 9) {
        switch (job_type_id)
        {
            case XRAN_JOB_TYPE_OTA_CB:
                if(xran_get_syscfg_appmode() == O_DU)
                    tim_lcore = eth_ctx->worker_core[pDevCtx->job2wrk_id[XRAN_JOB_TYPE_OTA_CB]];
                else
                    tim_lcore = eth_ctx->worker_core[0];
                break;
            case XRAN_JOB_TYPE_CP_DL:
                tim_lcore = eth_ctx->worker_core[pDevCtx->job2wrk_id[XRAN_JOB_TYPE_CP_DL]];
                break;
            case XRAN_JOB_TYPE_CP_UL:
                tim_lcore = eth_ctx->worker_core[pDevCtx->job2wrk_id[XRAN_JOB_TYPE_CP_UL]];
                break;
            case XRAN_JOB_TYPE_DEADLINE:
            case XRAN_JOB_TYPE_SYM_CB:
                if(xran_get_syscfg_appmode() == O_DU)
                    tim_lcore = eth_ctx->worker_core[pDevCtx->job2wrk_id[XRAN_JOB_TYPE_SYM_CB]];
                else
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

    return tim_lcore;
}

/* Increament the 'tti_to_process'. Setup the tti values for cplane processing.
 * Should be called on every symbol 0 */
void tti_ota_cb(struct rte_timer *tim, uint8_t mu)
{
    uint64_t t1 = MLogTick();
    uint32_t interval_us_local;
    int8_t   timerMu;
    struct xran_timing_source_ctx *pTmCtx;

    MLogTask(PID_TTI_TIMER, t1, MLogTick());

    timerMu = xran_timingsource_get_numerology();
    interval_us_local = xran_fs_get_tti_interval(timerMu);

    {
        /** tti as seen from PHY */
        uint32_t reg_tti, reg_sfn;
        uint32_t mlogVar[10], mlogVarCnt;
        uint32_t frameId, sfId, slotId;
        int32_t  nSfIdx = -1;
        uint32_t nFrameIdx, nSubframeIdx, nSlotIdx;
        uint64_t nSecond;
        uint8_t  nNrOfSlotInSf = 1 << timerMu;

            /* To match TTbox */
        if(xran_lib_ota_tti_base == 0)
            reg_tti = xran_fs_get_max_slot(timerMu) - 1;
        else
            reg_tti = xran_lib_ota_tti_base - 1;

        MLogIncrementCounter();
        reg_sfn = XranGetFrameNum(reg_tti, xran_getSfnSecStart(), SUBFRAMES_PER_SYSTEMFRAME,
                    SLOTNUM_PER_SUBFRAME(interval_us))*10 + XranGetSubFrameNum(reg_tti,SLOTNUM_PER_SUBFRAME(interval_us),
                    SUBFRAMES_PER_SYSTEMFRAME);
        /* subframe and slot */
        MLogRegisterFrameSubframe(reg_sfn, reg_tti % (SLOTNUM_PER_SUBFRAME(interval_us)));
        MLogMark(1, t1);

        slotId  = XranGetSlotNum(xran_lib_ota_tti_base, SLOTNUM_PER_SUBFRAME(interval_us_local));
        sfId    = XranGetSubFrameNum(xran_lib_ota_tti_base, SLOTNUM_PER_SUBFRAME(interval_us_local),  SUBFRAMES_PER_SYSTEMFRAME);
        frameId = XranGetFrameNum(xran_lib_ota_tti_base, xran_getSfnSecStart(), SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval_us_local));

        xran_get_slot_idx(0, &nFrameIdx, &nSubframeIdx, &nSlotIdx, &nSecond, timerMu);
        nSfIdx = nFrameIdx*SUBFRAMES_PER_SYSTEMFRAME*nNrOfSlotInSf
                    + nSubframeIdx*nNrOfSlotInSf
                    + nSlotIdx;
        mlogVarCnt = 0;
        mlogVar[mlogVarCnt++] = 0x11111111;
        mlogVar[mlogVarCnt++] = xran_lib_ota_tti_base;
        mlogVar[mlogVarCnt++] = xran_lib_ota_sym_idx_mu[timerMu];
        mlogVar[mlogVarCnt++] = xran_lib_ota_sym_idx_mu[timerMu] / 14;
        mlogVar[mlogVarCnt++] = frameId;
        mlogVar[mlogVarCnt++] = sfId;
        mlogVar[mlogVarCnt++] = slotId;
        mlogVar[mlogVarCnt++] = xran_lib_ota_tti_base % XRAN_N_FE_BUF_LEN;
        mlogVar[mlogVarCnt++] = nSfIdx;
        mlogVar[mlogVarCnt++] = nSfIdx % XRAN_N_FE_BUF_LEN;
        MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());
    }

    pTmCtx  = xran_timingsource_get_ctx();
    if(pTmCtx->ttiCb[XRAN_CB_TTI])
    {
        uint32_t core;

        if(xran_get_syscfg_appmode() == O_DU)
            core = xran_timingsource_get_coreid();
        else
            core = xran_ethdi_get_ctx()->worker_core[0];

        xran_timer_arm_ex(&pTmCtx->tti_to_phy_timer[xran_lib_ota_tti_base % MAX_TTI_TO_PHY_TIMER][timerMu % XRAN_MAX_NUM_MU],
                            tti_to_phy_cb, (void*)((uint64_t)timerMu), core);
    }

    // slot index is increased to next slot at the beginning of current OTA slot
    xran_lib_ota_tti_base++;
    if(xran_lib_ota_tti_base >= xran_fs_get_max_slot(timerMu))
    {
        xran_lib_ota_tti_base = 0;
    }

    MLogXRANTask(PID_TTI_CB, t1, MLogTick());
}

#ifdef POLL_EBBU_OFFLOAD
void tti_ota_cb_ebbu_offload(struct rte_timer *tim, void *arg, uint8_t mu)
{
    uint32_t    frame_id    = 0;
    uint32_t    subframe_id = 0;
    uint32_t    slot_id     = 0;
    uint32_t    next_tti    = 0;

    uint32_t mlogVar[10];
    uint32_t mlogVarCnt = 0;
    uint64_t t1 = MLogTick();
    uint32_t reg_tti  = 0;
    uint32_t reg_sfn  = 0;

    struct xran_system_config *p_syscfg = xran_get_systemcfg();
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)arg;
    struct xran_timer_ctx *pTCtx = (struct xran_timer_ctx *)p_xran_dev_ctx->perMu[mu].timer_ctx;
    uint8_t PortId = p_xran_dev_ctx->xran_port_id;
    uint32_t interval_us_local = xran_fs_get_tti_interval(mu);
    PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();


    MLogTask(PID_TTI_TIMER, t1, MLogTick());

    if(xran_timingsource_get_numerology() == mu)
    { /* will get entered for highest configured mu */
        /* To match TTbox */
        if(pCtx->ebbu_offload_ota_tti_cnt_mu[PortId][mu] == 0)
            reg_tti = xran_fs_get_max_slot(mu) - 1;
        else
            reg_tti = pCtx->ebbu_offload_ota_tti_cnt_mu[PortId][mu] -1;

        MLogIncrementCounter();
        reg_sfn    = XranGetFrameNum(reg_tti, xran_getSfnSecStart(), SUBFRAMES_PER_SYSTEMFRAME,
                        SLOTNUM_PER_SUBFRAME(interval_us))*10 + XranGetSubFrameNum(reg_tti,SLOTNUM_PER_SUBFRAME(interval_us),
                        SUBFRAMES_PER_SYSTEMFRAME);
        /* subframe and slot */
        MLogRegisterFrameSubframe(reg_sfn, reg_tti % (SLOTNUM_PER_SUBFRAME(interval_us)));
        MLogMark(1, t1);
    }

    slot_id     = XranGetSlotNum(pCtx->ebbu_offload_ota_tti_cnt_mu[PortId][mu], SLOTNUM_PER_SUBFRAME(interval_us_local));
    subframe_id = XranGetSubFrameNum(pCtx->ebbu_offload_ota_tti_cnt_mu[PortId][mu], SLOTNUM_PER_SUBFRAME(interval_us_local),  SUBFRAMES_PER_SYSTEMFRAME);
    frame_id    = XranGetFrameNum(pCtx->ebbu_offload_ota_tti_cnt_mu[PortId][mu], xran_getSfnSecStart(), SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval_us_local));

    pTCtx[(pCtx->ebbu_offload_ota_tti_cnt_mu[PortId][mu] & 1) ^ 1].tti_to_process = pCtx->ebbu_offload_ota_tti_cnt_mu[PortId][mu];

    /** tti as seen from PHY */
    int32_t nSfIdx = -1;
    uint32_t nFrameIdx;
    uint32_t nSubframeIdx;
    uint32_t nSlotIdx;
    uint64_t nSecond;
    uint8_t nNrOfSlotInSf = 1<<mu;

    xran_get_slot_idx(0, &nFrameIdx, &nSubframeIdx, &nSlotIdx, &nSecond, mu);
    nSfIdx = nFrameIdx*SUBFRAMES_PER_SYSTEMFRAME*nNrOfSlotInSf
             + nSubframeIdx*nNrOfSlotInSf
             + nSlotIdx;

    mlogVar[mlogVarCnt++] = 0x11111111;
    mlogVar[mlogVarCnt++] = pCtx->ebbu_offload_ota_tti_cnt_mu[PortId][mu];
    mlogVar[mlogVarCnt++] = pCtx->ebbu_offload_ota_sym_cnt_mu[mu];
    mlogVar[mlogVarCnt++] = pCtx->ebbu_offload_ota_sym_cnt_mu[mu] / 14;
    mlogVar[mlogVarCnt++] = frame_id;
    mlogVar[mlogVarCnt++] = subframe_id;
    mlogVar[mlogVarCnt++] = slot_id;
    mlogVar[mlogVarCnt++] = pCtx->ebbu_offload_ota_tti_cnt_mu[PortId][mu] % XRAN_N_FE_BUF_LEN;
    mlogVar[mlogVarCnt++] = nSfIdx;
    mlogVar[mlogVarCnt++] = nSfIdx % XRAN_N_FE_BUF_LEN;
    MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());


    if(xran_get_syscfg_appmode() == O_DU)
        next_tti = pCtx->ebbu_offload_ota_tti_cnt_mu[PortId][mu] + 1;
    else
        next_tti = pCtx->ebbu_offload_ota_tti_cnt_mu[PortId][mu];

    if(next_tti>= xran_fs_get_max_slot(mu)){
        print_dbg("[%d]SFN %d sf %d slot %d\n",next_tti, frame_id, subframe_id, slot_id);
        next_tti=0;
    }

    slot_id     = XranGetSlotNum(next_tti, SLOTNUM_PER_SUBFRAME(interval_us_local));
    subframe_id = XranGetSubFrameNum(next_tti,SLOTNUM_PER_SUBFRAME(interval_us_local),  SUBFRAMES_PER_SYSTEMFRAME);
    frame_id    = XranGetFrameNum(next_tti, xran_getSfnSecStart(), SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval_us_local));


    if(xran_get_syscfg_appmode() == O_DU)
        pTCtx[(pCtx->ebbu_offload_ota_tti_cnt_mu[PortId][mu] & 1)].tti_to_process = next_tti;
    else
        pTCtx[(pCtx->ebbu_offload_ota_tti_cnt_mu[PortId][mu] & 1)].tti_to_process = pTCtx[(pCtx->ebbu_offload_ota_tti_cnt_mu[PortId][mu] & 1)^1].tti_to_process;

    if(xran_timingsource_get_numerology() == mu && p_syscfg->ttiCb[XRAN_CB_TTI])
    {
        pCtx->pFn(0, TMNG_TTI_POLL, 0, (void*)&(p_xran_dev_ctx->perMu[mu]));/* Generate Task TMNG_TTI_POLL with no delay*/
    }

    //slot index is increased to next slot at the beginning of current OTA slot
    pCtx->ebbu_offload_ota_tti_cnt_mu[PortId][mu]++;
    if(pCtx->ebbu_offload_ota_tti_cnt_mu[PortId][mu] >= xran_fs_get_max_slot(mu)) {
        print_dbg("[%d]SFN %d sf %d slot %d\n",pCtx->ebbu_offload_ota_tti_cnt_mu[PortId][mu], frame_id, subframe_id, slot_id);
        pCtx->ebbu_offload_ota_tti_cnt_mu[PortId][mu] = 0;
    }

    MLogXRANTask(PID_TTI_CB, t1, MLogTick());
} /* tti_ota_cb_ebbu_offload */
#endif

static inline uint8_t xran_use_sect_type_3(struct xran_device_ctx *pDevCtx, int buff_id, uint16_t dir, uint8_t curr_mu)
{
    uint8_t muCount=0;

    /* When using nb-iot: section-type-3 should be sent only for nb-iot ul 3.75KHz.
     * Other active numerology (e.g. LTE) and NB-iot DL should use sect-type-1
     */
    for(uint8_t i=0;i<XRAN_MAX_NUM_MU;i++)
    {
        if(pDevCtx->fh_cfg.activeMUs->numerology[buff_id][i])
        {
            if( (XRAN_NBIOT_MU!=i) ||
                (curr_mu==XRAN_NBIOT_MU && XRAN_DIR_UL==dir && XRAN_NBIOT_UL_SCS_3_75==pDevCtx->fh_cfg.perMu[i].nbIotUlScs))
                muCount++;
        }
    }

    return ( (muCount==1) ? false : true);
}


int32_t
xran_prepare_cp_dl_slot(uint16_t xran_port_id, uint32_t nSlotIdx,  uint32_t nCcStart, uint32_t nCcNum, uint32_t nSymMask, uint32_t nAntStart,
                            uint32_t nAntNum, uint32_t nSymStart, uint32_t nSymNum, uint8_t mu)
{
    long t1 = MLogXRANTick();
    int32_t ret = XRAN_STATUS_SUCCESS;
    int tti, bufId;
    uint32_t slotId, sfId, frameId;
    int ccId;
    uint8_t ctxId;
    uint8_t antId, num_eAxc, num_CCPorts, ruPortId;
    void *pHandle;
    struct xran_buffer_list *pBufList;
    //int num_list;
    struct xran_device_ctx * pDevCtx = xran_dev_get_ctx_by_id(xran_port_id);
    if(unlikely(!pDevCtx))
    {
        print_err("Null xRAN context!!\n");
        return ret;
    }

    if(unlikely((pDevCtx->fh_cfg.ru_conf.xranCat != XRAN_CATEGORY_A) && (pDevCtx->fh_cfg.ru_conf.xranCat != XRAN_CATEGORY_B)))
    {
        print_err("Unsupported Category %d\n", pDevCtx->fh_cfg.ru_conf.xranCat);
        return -1;
    }

    if(mu == XRAN_DEFAULT_MU)
        mu = pDevCtx->fh_cfg.mu_number[0];
    uint32_t interval_us_local = xran_fs_get_tti_interval(mu);
    uint8_t PortId = pDevCtx->xran_port_id;
    pHandle     = pDevCtx;

    num_eAxc    = xran_get_num_eAxc(pHandle);
    num_CCPorts = xran_get_num_cc(pHandle);

    if(first_call && pDevCtx->enableCP)
    {
        tti = nSlotIdx;
        bufId = tti % XRAN_N_FE_BUF_LEN;

        slotId     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME(interval_us_local));
        sfId       = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME(interval_us_local),  SUBFRAMES_PER_SYSTEMFRAME);
        frameId    = (tti / SLOTS_PER_SYSTEMFRAME(interval_us_local)) & 0x3FF;

        // ORAN frameId, 8 bits, [0, 255]
        frameId = (frameId & 0xff);

        ctxId      = tti % XRAN_MAX_SECTIONDB_CTX;

        print_dbg("[%d]SFN %d sf %d slot %d\n", tti, frameId, sfId, slotId);

        bool useSectType3 = xran_use_sect_type_3(pDevCtx, bufId, XRAN_DIR_DL, mu);

#if defined(__INTEL_COMPILER)
#pragma vector always
#endif
        for(antId = nAntStart; (antId < (nAntStart + nAntNum)  && antId < num_eAxc); ++antId) {
            ruPortId = antId + pDevCtx->perMu[mu].eaxcOffset;
            for(ccId = nCcStart; (ccId < (nCcStart + nCcNum) && ccId < num_CCPorts); ccId++)
            {
                if(!xran_isactive_cc(pDevCtx, ccId))
                    continue;

                /* start new section information list */
                xran_cp_reset_section_info(pHandle, XRAN_DIR_DL, ccId, ruPortId, ctxId, mu);
                if(xran_fs_get_slot_type(PortId, ccId, tti, XRAN_SLOT_TYPE_DL, mu) == 1) {
                    if(pDevCtx->perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[bufId][ccId][antId].sBufferList.pBuffers) {
                        if(pDevCtx->perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[bufId][ccId][antId].sBufferList.pBuffers->pData)
                        {
                            xran_cp_create_and_send_section(PortId, ruPortId, XRAN_DIR_DL, tti, ccId,
                                                            (struct xran_prb_map *)pDevCtx->perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[bufId][ccId][antId].sBufferList.pBuffers->pData,
                                                            &(pDevCtx->prbElmProcInfo[bufId][ccId][antId]),
                                                            pDevCtx->fh_cfg.ru_conf.xranCat, ctxId, mu, useSectType3, 1);
                        }
                        else {
                               print_err("[%d]SFN %d sf %d slot %d: antId %d ccId %d [pData]\n", tti, frameId, sfId, slotId, antId, ccId);
                        }
                    } else {
                        print_err("[%d]SFN %d sf %d slot %d: antId %d ccId %d [pBuffers] \n", tti, frameId, sfId, slotId, antId, ccId);
                    }

                    if(pDevCtx->vMuInfo.ssbInfo.ssbMu > mu) /* If SSB uses a different numerology, process SSB as a virtual numerology */
                    {
                        if(mu == 3 && pDevCtx->vMuInfo.ssbInfo.ssbMu == 4){
                            uint8_t SsbRuPortId = antId + pDevCtx->vMuInfo.ssbInfo.ruPortId_offset;

                            if(unlikely( xran_vMu_cp_create_and_send_ssb_pkt(PortId, SsbRuPortId, XRAN_DIR_DL, tti, ccId, pDevCtx->vMuInfo.ssbInfo.ssbMu, mu) != XRAN_STATUS_SUCCESS)){
                                print_err("xran_vMu_cp_create_and_send_ssb_pkt failed");
                            }
                        }
                        else{
                            print_err("Unsupported mu %hhu ssbMu %hhu",mu,pDevCtx->vMuInfo.ssbInfo.ssbMu);
                        }
                    }
                } /* if(xran_fs_get_slot_type(ccId, tti, XRAN_SLOT_TYPE_DL) == 1) */
            } /* for(ccId = 0; ccId < num_CCPorts; ccId++) */
        } /* for(antId = 0; antId < num_eAxc; ++antId) */

        /* CSI-RS */
        if(pDevCtx->csirsEnable && nAntStart == 0)
        {
            struct xran_csirs_config *pCsirsCfg = &(pDevCtx->csirs_cfg);
            for(antId = 0; antId < XRAN_MAX_CSIRS_PORTS; antId++)
            {
                ruPortId = antId + pCsirsCfg->csirsEaxcOffset;

                for(ccId = nCcStart; (ccId < (nCcStart + nCcNum) && ccId < num_CCPorts); ccId++)
                {
                    if(!xran_isactive_cc(pDevCtx, ccId))
                        continue;

                    /* start new section information list */
                    xran_cp_reset_section_info(pHandle, XRAN_DIR_DL, ccId, ruPortId, ctxId, mu);
                    if(xran_fs_get_slot_type(PortId, ccId, tti, XRAN_SLOT_TYPE_DL, mu) == 1)
                    {
                        pBufList = &(pDevCtx->perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[bufId][ccId][antId].sBufferList); /* To shorten reference */
                        if(pBufList->pBuffers && pBufList->pBuffers->pData)
                        {
                            /*ret = */xran_cp_create_and_send_section(PortId, ruPortId, XRAN_DIR_DL, tti, ccId,
                                                                (struct xran_prb_map *)(pBufList->pBuffers->pData),
                                                                &(pDevCtx->prbElmProcCSIInfo[bufId][ccId][antId]),
                                                                pDevCtx->fh_cfg.ru_conf.xranCat, ctxId, mu, false, 1);
                        }
                    }
                }
            }
        } /* if(pDevCtx->csirsEnable) */

        MLogXRANTask(PID_CP_DL_CB, t1, MLogXRANTick());
	MLogSetTaskCoreMap(TASK_3500);
    }
    return ret;
}

void tx_cp_dl_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogXRANTick();
    int tti, bufId;
    uint32_t slotId, sfId, frameId;
    int ccId;
    uint8_t ctxId;
    uint8_t antId, num_eAxc, num_CCPorts, ruPortId;
    void *pHandle;
    //int num_list;
    xran_device_per_mu_fields *p_MuPerDev = (xran_device_per_mu_fields *)arg;
    struct xran_device_ctx * pDevCtx;
    uint8_t mu;
    struct xran_buffer_list *pBufList;
    uint32_t cp_dl_tti = 0, iloop;
    xran_status_t retVal;

#ifdef POLL_EBBU_OFFLOAD
    PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();
#endif

    if(unlikely(!p_MuPerDev))
    {
        print_err("Null p_MuPerDev context!!\n");
        return;
    }

    pDevCtx = p_MuPerDev->p_dev_ctx;
    mu = p_MuPerDev->mu;

    if(unlikely(!pDevCtx))
    {
        print_err("Null xRAN context!!\n");
        return;
    }

    if(xran_get_syscfg_bbuoffload())
        return;

    if(unlikely((pDevCtx->fh_cfg.ru_conf.xranCat != XRAN_CATEGORY_A) && (pDevCtx->fh_cfg.ru_conf.xranCat != XRAN_CATEGORY_B)))
    {
        print_err("Unsupported Category %d\n", pDevCtx->fh_cfg.ru_conf.xranCat);
        return;
    }

    struct xran_timer_ctx *pTCtx = (struct xran_timer_ctx *)pDevCtx->perMu[mu].timer_ctx;
    uint32_t interval_us_local = xran_fs_get_tti_interval(mu);
    uint8_t PortId = pDevCtx->xran_port_id;
    pHandle     = pDevCtx;

    num_eAxc    = xran_get_num_eAxc(pHandle);
    num_CCPorts = xran_get_num_cc(pHandle);

#ifdef POLL_EBBU_OFFLOAD
    first_call = pCtx->first_call;
#endif

    if(first_call && pDevCtx->enableCP)
    {
#ifndef POLL_EBBU_OFFLOAD
        tti = pTCtx[(xran_lib_ota_tti_mu[PortId][mu] & 1) ^ 1].tti_to_process;
#else
        tti = pTCtx[(pCtx->ebbu_offload_ota_tti_cnt_mu[PortId][mu] & 1) ^ 1].tti_to_process;
#endif
        /**
         * In case CP DL has to be transmitted relative to start symbol-id, for tti_to_process i.e. ota + 1,
         * Symbol 0 will be sent in current slot on sym-id (14 - T1a_max_cp_dl). Thus symbols from sym-id 0 to T1a_max_cp_dl - 1
         * can be transmitted in the ota slot. Sym ids from T1a_max_cp_dl to 13 will have to be transmitted in the next slot.
         * */
        uint32_t prevTti = ((tti == 0) ? (xran_fs_get_max_slot(mu) - 1): (tti - 1));
        for(iloop = prevTti; iloop <= (prevTti + 1); ++iloop)
        {
            /*For the corner case when (prevTti + 1) =xran_fs_get_max_slot(mu) */

            /*When dlCpProcBurst=0, callback for tx_cp_dl_cb is registered on particulat tx sym(s) only.
             * We don't have to process it for previous TTI */
            if(pDevCtx->dlCpProcBurst == 0 && iloop == prevTti)
                continue;

            cp_dl_tti = (iloop % xran_fs_get_max_slot(mu));
            bufId = cp_dl_tti % XRAN_N_FE_BUF_LEN;

            slotId     = XranGetSlotNum(cp_dl_tti, SLOTNUM_PER_SUBFRAME(interval_us_local));
            sfId = XranGetSubFrameNum(cp_dl_tti,SLOTNUM_PER_SUBFRAME(interval_us_local),  SUBFRAMES_PER_SYSTEMFRAME);
            frameId    = XranGetFrameNum(cp_dl_tti,xran_getSfnSecStart(),SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval_us_local));
            if(cp_dl_tti == 0)
            {
                /* Wrap around to next second */
                frameId = (frameId + NUM_OF_FRAMES_PER_SECOND) & 0x3ff;
            }

            ctxId      = cp_dl_tti % XRAN_MAX_SECTIONDB_CTX;
            print_dbg("[%d]SFN %d sf %d slot %d\n", cp_dl_tti, frameId, sfId, slotId);

            bool useSectType3 = xran_use_sect_type_3(pDevCtx, bufId, XRAN_DIR_DL, mu);
            for(antId = 0; antId < num_eAxc; ++antId)
            {
                ruPortId = antId + pDevCtx->perMu[mu].eaxcOffset;
                for(ccId = 0; ccId < num_CCPorts; ccId++ )
                {
                    if(!xran_isactive_cc(pDevCtx, ccId))
                        continue;
                    /* numSymsRemaining will be 14 hence section info will be reset only on sym 0*/
                    if((xran_lib_ota_sym_idx_mu[mu] % N_SYM_PER_SLOT == 0
                                && (cp_dl_tti == tti))    /*Ensure that we don't reset the DB for previous tti*/
                        || (0 == pDevCtx->dlCpProcBurst
                                && pDevCtx->prbElmProcInfo[bufId][ccId][antId].numSymsRemaining == 0
                                && pDevCtx->DynamicSectionEna == 0))
                    {
                        xran_cp_reset_section_info(pHandle, XRAN_DIR_DL, ccId, ruPortId, ctxId, mu);
                    }
                    if(xran_fs_get_slot_type(PortId, ccId, cp_dl_tti, XRAN_SLOT_TYPE_DL, mu) == 1)
                    {
                        if(pDevCtx->perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[bufId][ccId][antId].sBufferList.pBuffers)
                        {
                            if (pDevCtx->perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[bufId][ccId][antId].sBufferList.pBuffers->pData)
                            {
                                xran_cp_create_and_send_section(PortId, ruPortId, XRAN_DIR_DL, cp_dl_tti, ccId,
                                                                (struct xran_prb_map *)pDevCtx->perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[bufId][ccId][antId].sBufferList.pBuffers->pData,
                                                                &(pDevCtx->prbElmProcInfo[bufId][ccId][antId]),
                                                                pDevCtx->fh_cfg.ru_conf.xranCat, ctxId, mu, useSectType3, 0);
                            }
                            else
                                print_err("[%d]SFN %d sf %d slot %d: antId %d ccId %d [pData]\n", cp_dl_tti, frameId, sfId, slotId, antId, ccId);
                        }

                        if(pDevCtx->vMuInfo.ssbInfo.ssbMu > mu) /* If SSB uses a different numerology, process SSB as a virtual numerology */
                        {
                            if(mu == 3 && pDevCtx->vMuInfo.ssbInfo.ssbMu == 4){
                                uint8_t SsbRuPortId = antId + pDevCtx->vMuInfo.ssbInfo.ruPortId_offset;
                                retVal = xran_vMu_cp_create_and_send_ssb_pkt(PortId, SsbRuPortId, XRAN_DIR_DL, cp_dl_tti, ccId, pDevCtx->vMuInfo.ssbInfo.ssbMu, mu);

                                if(unlikely( retVal != XRAN_STATUS_SUCCESS)){
                                    print_err("xran_vMu_cp_create_and_send_ssb_pkt failed");
                                }
                            }
                            else{
                                print_err("Unsupported mu %hhu ssbMu %hhu",mu,pDevCtx->vMuInfo.ssbInfo.ssbMu);
                            }
                        }
                    } /* if(xran_fs_get_slot_type(ccId, tti, XRAN_SLOT_TYPE_DL) == 1) */
                } /* for(ccId = 0; ccId < num_CCPorts; ccId++) */
            } /* for(antId = 0; antId < num_eAxc; ++antId) */

            /* CSI */
            if(pDevCtx->csirsEnable)
            {
                struct xran_csirs_config *pCsirsCfg = &(pDevCtx->csirs_cfg);
                for(antId = 0; antId < XRAN_MAX_CSIRS_PORTS; antId++)
                {
                    ruPortId = antId + pCsirsCfg->csirsEaxcOffset;
                    for(ccId = 0; ccId < num_CCPorts; ccId++)
                    {
                        if(!xran_isactive_cc(pDevCtx, ccId))
                            continue;

                        /* start new section information list */
                        if ((xran_lib_ota_sym_idx_mu[mu] % N_SYM_PER_SLOT == 0 && (cp_dl_tti == tti)/*Ensure that we don't reset the DB for previous tti*/)|| (0 == pDevCtx->dlCpProcBurst && pDevCtx->prbElmProcCSIInfo[bufId][ccId][ruPortId].numSymsRemaining == 0 && pDevCtx->DynamicSectionEna == 0)){
                            xran_cp_reset_section_info(pHandle, XRAN_DIR_DL, ccId, ruPortId, ctxId, mu);
                        }
                        if(xran_fs_get_slot_type(PortId, ccId, cp_dl_tti, XRAN_SLOT_TYPE_DL, mu) == 1)
                        {
                            pBufList = &(pDevCtx->perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[bufId][ccId][antId].sBufferList); /* To shorten reference */
                            if(pBufList->pBuffers && pBufList->pBuffers->pData)
                            {
                                /*ret = */xran_cp_create_and_send_section(PortId, ruPortId, XRAN_DIR_DL, cp_dl_tti, ccId,
                                                                    (struct xran_prb_map *)(pBufList->pBuffers->pData),
                                                                    &(pDevCtx->prbElmProcCSIInfo[bufId][ccId][antId]),
                                                                    pDevCtx->fh_cfg.ru_conf.xranCat, ctxId, mu, false, 0);
                            }
                        }
                    }
                }
            } /* if(pDevCtx->csirsEnable) */
        } /*cp_dl_tti*/

        MLogXRANTask(PID_CP_DL_CB, t1, MLogXRANTick());
    }
}

void rx_ul_static_srs_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogXRANTick();
    xran_device_per_mu_fields *p_MuPerDev = (xran_device_per_mu_fields *)arg;
    struct xran_device_ctx * pDevCtx = p_MuPerDev->p_dev_ctx;
    uint8_t mu = p_MuPerDev->mu;
    xran_status_t status = 0;
    int32_t rx_tti = 0;// = (int32_t)XranGetTtiNum(xran_lib_ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);
    int32_t ccId = 0;
    //uint32_t nFrameIdx;
    //uint32_t nSubframeIdx;
    //uint32_t nSlotIdx;
    //uint64_t nSecond;
    struct xran_timer_ctx* p_timer_ctx = NULL;

    if(pDevCtx->xran2phy_mem_ready == 0)
        return;

    p_timer_ctx = &pDevCtx->perMu[mu].cb_timer_ctx[pDevCtx->perMu[mu].timer_get % MAX_CB_TIMER_CTX];

    rx_tti = p_timer_ctx->tti_to_process;

    rx_tti = (rx_tti + xran_fs_get_max_slot(mu) - 1 - pDevCtx->perMu[mu].deadline_slot_advance[XRAN_SLOT_HALF_CB]) % xran_fs_get_max_slot(mu);

    /* U-Plane */
    for(ccId = 0; ccId < xran_get_num_cc(pDevCtx); ccId++) {
        if(!xran_isactive_cc(pDevCtx, ccId))
            continue;
        if(0 == pDevCtx->enableSrsCp)
        {
            if(pDevCtx->pSrsCallback[ccId]){
                struct xran_cb_tag *pTag = pDevCtx->pSrsCallbackTag[ccId];
                if(pTag) {
                    //pTag->cellId = ccId;
                    pTag->slotiId = rx_tti;
                    pTag->symbol  = XRAN_FULL_CB_SYM; /* last 7 sym means full slot of Symb */
                    pDevCtx->pSrsCallback[ccId](pDevCtx->pSrsCallbackTag[ccId], status, mu);
                }
            }
        }
    }
    MLogXRANTask(PID_UP_STATIC_SRS_DEAD_LINE_CB, t1, MLogXRANTick());
}


void rx_ul_deadline_one_fourths_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogXRANTick();
    xran_device_per_mu_fields *p_MuPerDev = (xran_device_per_mu_fields *)arg;
    struct xran_device_ctx * pDevCtx = p_MuPerDev->p_dev_ctx;
    uint8_t mu = p_MuPerDev->mu;
    struct xran_timing_source_ctx *pTmCtx = xran_timingsource_get_ctx();

    xran_status_t status;
    /* half of RX for current TTI as measured against current OTA time */
    int32_t rx_tti;
    int32_t ccId;

    struct xran_timer_ctx* p_timer_ctx = NULL;

    if(pDevCtx->xran2phy_mem_ready == 0)
        return;

    p_timer_ctx = &pDevCtx->perMu[mu].cb_timer_ctx[pDevCtx->perMu[mu].timer_get % MAX_CB_TIMER_CTX];
    rx_tti = p_timer_ctx->tti_to_process;

    rx_tti = (rx_tti + xran_fs_get_max_slot(mu) - pDevCtx->perMu[mu].deadline_slot_advance[XRAN_SLOT_1_4_CB]) % xran_fs_get_max_slot(mu);

    for(ccId = 0; ccId < xran_get_num_cc(pDevCtx); ccId++)
    {
#if XRAN_CHECK_ACTIVECC_CB
        if(!xran_isactive_cc(pDevCtx, ccId))
            continue;
#endif
        if(pDevCtx->pCallback[ccId])
        {
            struct xran_cb_tag *pTag = pDevCtx->pCallbackTag[ccId];
            if(pTag)
            {
                //pTag->cellId = ccId;
                pTag->slotiId = rx_tti;
                pTag->symbol  = XRAN_ONE_FOURTHS_CB_SYM;
                status = XRAN_STATUS_SUCCESS;

                pDevCtx->pCallback[ccId](pDevCtx->pCallbackTag[ccId], status, mu);
            }
        }
    }

    if(xran_timingsource_get_numerology() == mu
        && pTmCtx->ttiCb[XRAN_CB_HALF_SLOT_RX])
    {
        if(pTmCtx->SkipTti[XRAN_CB_HALF_SLOT_RX] <= 0)
        {
            pTmCtx->ttiCb[XRAN_CB_HALF_SLOT_RX](pTmCtx->TtiCbParam[XRAN_CB_HALF_SLOT_RX], mu);
        }
        else
        {
            pTmCtx->SkipTti[XRAN_CB_HALF_SLOT_RX]--;
        }
    }

    MLogXRANTask(PID_UP_UL_ONE_FOURTHS_DEAD_LINE_CB, t1, MLogXRANTick());
}

void rx_ul_deadline_half_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogXRANTick();
    xran_device_per_mu_fields *p_MuPerDev = (xran_device_per_mu_fields *)arg;
    struct xran_device_ctx * pDevCtx = p_MuPerDev->p_dev_ctx;
    uint8_t mu = p_MuPerDev->mu;
    struct xran_timing_source_ctx *pTmCtx = xran_timingsource_get_ctx();

    xran_status_t status;
    /* half of RX for current TTI as measured against current OTA time */
    int32_t rx_tti;
    int32_t ccId;

    struct xran_timer_ctx* p_timer_ctx = NULL;

    if(pDevCtx->xran2phy_mem_ready == 0)
        return;

    p_timer_ctx = &pDevCtx->perMu[mu].cb_timer_ctx[pDevCtx->perMu[mu].timer_get % MAX_CB_TIMER_CTX];

    rx_tti = p_timer_ctx->tti_to_process;

    rx_tti = (rx_tti + xran_fs_get_max_slot(mu) - pDevCtx->perMu[mu].deadline_slot_advance[XRAN_SLOT_HALF_CB]) % xran_fs_get_max_slot(mu);

    for(ccId = 0; ccId < xran_get_num_cc(pDevCtx); ccId++)
    {
#if XRAN_CHECK_ACTIVECC_CB
        if(!xran_isactive_cc(pDevCtx, ccId))
            continue;
#endif
        if(pDevCtx->pCallback[ccId]) {
            struct xran_cb_tag *pTag = pDevCtx->pCallbackTag[ccId];
            if(pTag) {
                //pTag->cellId = ccId;
                pTag->slotiId = rx_tti;
                pTag->symbol  = XRAN_HALF_CB_SYM;
                status = XRAN_STATUS_SUCCESS;

                pDevCtx->pCallback[ccId](pDevCtx->pCallbackTag[ccId], status, mu);
            }
        }
    }

    if(xran_timingsource_get_numerology() == mu
        && pTmCtx->ttiCb[XRAN_CB_HALF_SLOT_RX])
    {
        if(pTmCtx->SkipTti[XRAN_CB_HALF_SLOT_RX] <= 0)
        {
            pTmCtx->ttiCb[XRAN_CB_HALF_SLOT_RX](pTmCtx->TtiCbParam[XRAN_CB_HALF_SLOT_RX], mu);
        }
        else
        {
            pTmCtx->SkipTti[XRAN_CB_HALF_SLOT_RX]--;
        }
    }

    MLogXRANTask(PID_UP_UL_HALF_DEAD_LINE_CB, t1, MLogXRANTick());
}

void rx_ul_deadline_three_fourths_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogXRANTick();
    xran_device_per_mu_fields *p_MuPerDev = (xran_device_per_mu_fields *)arg;
    struct xran_device_ctx * pDevCtx = p_MuPerDev->p_dev_ctx;
    uint8_t mu = p_MuPerDev->mu;
    struct xran_timing_source_ctx *pTmCtx = xran_timingsource_get_ctx();

    xran_status_t status;
    /* half of RX for current TTI as measured against current OTA time */
    int32_t rx_tti;
    int32_t ccId;

    struct xran_timer_ctx* p_timer_ctx = NULL;

    if(pDevCtx->xran2phy_mem_ready == 0)
        return;

    p_timer_ctx = &pDevCtx->perMu[mu].cb_timer_ctx[pDevCtx->perMu[mu].timer_get % MAX_CB_TIMER_CTX];

    rx_tti = p_timer_ctx->tti_to_process;

    rx_tti = (rx_tti + xran_fs_get_max_slot(mu) - pDevCtx->perMu[mu].deadline_slot_advance[XRAN_SLOT_3_4_CB]) % xran_fs_get_max_slot(mu);

    for(ccId = 0; ccId < xran_get_num_cc(pDevCtx); ccId++)
    {
#if XRAN_CHECK_ACTIVECC_CB
        if(!xran_isactive_cc(pDevCtx, ccId))
            continue;
#endif
        if(pDevCtx->pCallback[ccId])
        {
            struct xran_cb_tag *pTag = pDevCtx->pCallbackTag[ccId];
            if(pTag)
            {
                //pTag->cellId = ccId;
                pTag->slotiId = rx_tti;
                pTag->symbol  = XRAN_THREE_FOURTHS_CB_SYM;
                status = XRAN_STATUS_SUCCESS;

                pDevCtx->pCallback[ccId](pDevCtx->pCallbackTag[ccId], status, mu);
            }
        }
    }

    if(xran_timingsource_get_numerology() == mu
        && pTmCtx->ttiCb[XRAN_CB_HALF_SLOT_RX])
    {
        if(pTmCtx->SkipTti[XRAN_CB_HALF_SLOT_RX] <= 0)
        {
            pTmCtx->ttiCb[XRAN_CB_HALF_SLOT_RX](pTmCtx->TtiCbParam[XRAN_CB_HALF_SLOT_RX], mu);
        }
        else
        {
            pTmCtx->SkipTti[XRAN_CB_HALF_SLOT_RX]--;
        }
    }

    MLogXRANTask(PID_UP_UL_THREE_FOURTHS_DEAD_LINE_CB, t1, MLogXRANTick());
}

void rx_ul_deadline_full_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogXRANTick();
    xran_device_per_mu_fields *p_MuPerDev = (xran_device_per_mu_fields *)arg;
    struct xran_device_ctx * pDevCtx = p_MuPerDev->p_dev_ctx;
    uint8_t mu = p_MuPerDev->mu;
    struct xran_timing_source_ctx *pTmCtx = xran_timingsource_get_ctx();

    xran_status_t status = 0;
    int32_t rx_tti = 0;
    int32_t ccId = 0;

    struct xran_timer_ctx* p_timer_ctx = NULL;

    if(pDevCtx->xran2phy_mem_ready == 0)
        return;

    p_timer_ctx = &pDevCtx->perMu[mu].cb_timer_ctx[pDevCtx->perMu[mu].timer_get % MAX_CB_TIMER_CTX];
    rx_tti = p_timer_ctx->tti_to_process;

    rx_tti = (rx_tti + xran_fs_get_max_slot(mu) - 1 - pDevCtx->perMu[mu].deadline_slot_advance[XRAN_SLOT_FULL_CB]) % xran_fs_get_max_slot(mu);

    /* U-Plane */
    for(ccId = 0; ccId < xran_get_num_cc(pDevCtx); ccId++)
    {
#if XRAN_CHECK_ACTIVECC_CB
        if(!xran_isactive_cc(pDevCtx, ccId))
            continue;
#endif
        if(pDevCtx->pCallback[ccId]){
            struct xran_cb_tag *pTag = pDevCtx->pCallbackTag[ccId];
            if(pTag) {
                //pTag->cellId = ccId;
                pTag->slotiId = rx_tti;
                pTag->symbol  = XRAN_FULL_CB_SYM; /* last 7 sym means full slot of Symb */
                status = XRAN_STATUS_SUCCESS;
                pDevCtx->pCallback[ccId](pDevCtx->pCallbackTag[ccId], status, mu);
            }
        }

        if(pDevCtx->pPrachCallback[ccId]){
            struct xran_cb_tag *pTag = pDevCtx->pPrachCallbackTag[ccId];
            if(pTag) {
                //pTag->cellId = ccId;
                pTag->slotiId = rx_tti;
                pTag->symbol  = XRAN_FULL_CB_SYM; /* last 7 sym means full slot of Symb */
                pDevCtx->pPrachCallback[ccId](pDevCtx->pPrachCallbackTag[ccId], status, mu);
            }
        }

        if(pDevCtx->enableSrsCp)
        {
            if(pDevCtx->pSrsCallback[ccId]){
                struct xran_cb_tag *pTag = pDevCtx->pSrsCallbackTag[ccId];
                if(pTag) {
                    //pTag->cellId = ccId;
                    pTag->slotiId = rx_tti;
                    pTag->symbol  = XRAN_FULL_CB_SYM; /* last 7 sym means full slot of Symb */
                    pDevCtx->pSrsCallback[ccId](pDevCtx->pSrsCallbackTag[ccId], status, mu);
                }
            }
        }

        if(pDevCtx->pCsirsCallback[ccId]){
            struct xran_cb_tag *pTag = pDevCtx->pCsirsCallbackTag[ccId];
            if(pTag) {
                //pTag->cellId = ccId;
                pTag->slotiId = rx_tti;
                pTag->symbol  = XRAN_FULL_CB_SYM; /* last 7 sym means full slot of Symb */
                pDevCtx->pCsirsCallback[ccId](pDevCtx->pCsirsCallbackTag[ccId], status, mu);
            }
        }
    }

    /* user call backs if any */
    if(xran_timingsource_get_numerology() == mu
        && pTmCtx->ttiCb[XRAN_CB_FULL_SLOT_RX])
    {
        if(pTmCtx->SkipTti[XRAN_CB_FULL_SLOT_RX] <= 0)
        {
            pTmCtx->ttiCb[XRAN_CB_FULL_SLOT_RX](pTmCtx->TtiCbParam[XRAN_CB_FULL_SLOT_RX], mu);
        }
        else
        {
            pTmCtx->SkipTti[XRAN_CB_FULL_SLOT_RX]--;
        }
    }

    MLogXRANTask(PID_UP_UL_FULL_DEAD_LINE_CB, t1, MLogXRANTick());
}

void rx_ul_user_sym_cb(struct rte_timer *tim, void *arg)
{
    //TODO: Check the usage for mixed mu
    long t1 = MLogXRANTick();
    struct xran_device_ctx * p_dev_ctx = NULL;
    struct cb_user_per_sym_ctx *p_sym_cb_ctx = (struct cb_user_per_sym_ctx *)arg;
    int32_t rx_tti = 0;
    uint32_t interval, ota_sym_idx = 0;
    struct xran_timer_ctx* p_timer_ctx =  NULL;
    uint8_t mu=p_sym_cb_ctx->mu;

    if(p_sym_cb_ctx->p_dev)
        p_dev_ctx = (struct xran_device_ctx *)p_sym_cb_ctx->p_dev;
    else
        rte_panic("p_sym_cb_ctx->p_dev == NULL");

    if(p_dev_ctx->xran2phy_mem_ready == 0)
        return;

    interval = xran_fs_get_tti_interval(mu);

    p_timer_ctx = &p_sym_cb_ctx->user_cb_timer_ctx[p_sym_cb_ctx->user_timer_get++ % MAX_CB_TIMER_CTX];
    if (p_sym_cb_ctx->user_timer_get >= MAX_CB_TIMER_CTX)
        p_sym_cb_ctx->user_timer_get = 0;

    rx_tti = p_timer_ctx->tti_to_process;

    if( p_sym_cb_ctx->sym_diff > 0)
        /* + advacne TX Wind: at OTA Time we indicating event in future */
        ota_sym_idx = ((p_timer_ctx->ota_sym_idx + p_sym_cb_ctx->sym_diff) % xran_timingsource_get_max_ota_sym_idx(mu));
    else if (p_sym_cb_ctx->sym_diff < 0)
    {
        /* - dealy RX Win: at OTA Time we indicate event in the past */
        if(p_timer_ctx->ota_sym_idx >= abs(p_sym_cb_ctx->sym_diff))
        {
            ota_sym_idx = p_timer_ctx->ota_sym_idx + p_sym_cb_ctx->sym_diff;
        }
        else
        {
            ota_sym_idx = ((xran_timingsource_get_max_ota_sym_idx(mu) + p_timer_ctx->ota_sym_idx) + p_sym_cb_ctx->sym_diff) % xran_timingsource_get_max_ota_sym_idx(mu);
        }
    }
    else /* 0 - OTA exact time */
        ota_sym_idx = p_timer_ctx->ota_sym_idx;

    rx_tti = (int32_t)XranGetTtiNum(ota_sym_idx, XRAN_NUM_OF_SYMBOL_PER_SLOT);

    if(p_sym_cb_ctx->symCbTimeInfo)
    {
            struct xran_sense_of_time *p_sense_time = p_sym_cb_ctx->symCbTimeInfo;
            p_sense_time->type_of_event = p_sym_cb_ctx->cb_type_id;
            p_sense_time->nSymIdx       = p_sym_cb_ctx->symb_num_req;
            p_sense_time->tti_counter   = rx_tti;
            p_sense_time->nSlotIdx      = (uint32_t)XranGetSlotNum(rx_tti, SLOTNUM_PER_SUBFRAME(interval));
            p_sense_time->nSubframeIdx  = (uint32_t)XranGetSubFrameNum(rx_tti,SLOTNUM_PER_SUBFRAME(interval),  SUBFRAMES_PER_SYSTEMFRAME);
            p_sense_time->nFrameIdx     = (uint32_t)XranGetFrameNum(rx_tti, p_timer_ctx->xran_sfn_at_sec_start, SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval));
            p_sense_time->nSecond       = p_timer_ctx->current_second;
    }

    /* user call backs if any */
    if(p_sym_cb_ctx->symCb)
    {
        p_sym_cb_ctx->symCb(p_sym_cb_ctx->symCbParam, p_sym_cb_ctx->symCbTimeInfo);
    }

    MLogXRANTask(PID_UP_UL_USER_DEAD_LINE_CB, t1, MLogXRANTick());
}

int32_t
xran_prepare_cp_ul_slot(uint16_t xran_port_id, uint32_t nSlotIdx,  uint32_t nCcStart, uint32_t nCcNum, uint32_t nSymMask, uint32_t nAntStart,
                            uint32_t nAntNum, uint32_t nSymStart, uint32_t nSymNum, uint8_t mu)
{
    int32_t ret = XRAN_STATUS_SUCCESS;
    long t1 = MLogXRANTick();
    int tti, bufId;
    uint32_t slotId, sfId, frameId;
    int32_t ccId;
    int antId, portId;
    uint16_t occasionid = 0;
    uint8_t  occasionloop = 0;
    uint16_t beam_id;
    uint8_t num_eAxc, num_CCPorts, ruPortId;
    uint8_t ctxId;
    void *pHandle;
    uint32_t interval;
    uint8_t PortId;

    struct xran_buffer_list *pBufList;
    struct xran_device_ctx * pDevCtx = xran_dev_get_ctx_by_id(xran_port_id);
    struct xran_system_config *sysCfg = xran_get_systemcfg();
    if(unlikely(!pDevCtx))
    {
        print_err("Null xRAN context!!\n");
        return ret;
    }

    if(unlikely((pDevCtx->fh_cfg.ru_conf.xranCat != XRAN_CATEGORY_A) && (pDevCtx->fh_cfg.ru_conf.xranCat != XRAN_CATEGORY_B)))
    {
        print_err("Unsupported Category %d\n", pDevCtx->fh_cfg.ru_conf.xranCat);
        return -1;
    }

    if(mu == XRAN_DEFAULT_MU)
        mu = pDevCtx->fh_cfg.mu_number[0];

    if(first_call && pDevCtx->enableCP)
    {
        pHandle     = pDevCtx;
        interval    = xran_fs_get_tti_interval(mu);
        PortId      = pDevCtx->xran_port_id;

        tti = nSlotIdx;

        bufId      = tti % XRAN_N_FE_BUF_LEN;
        ctxId      = tti % XRAN_MAX_SECTIONDB_CTX;
        slotId     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME(interval));
        sfId       = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME(interval),  SUBFRAMES_PER_SYSTEMFRAME);
        frameId    = (nSlotIdx / SLOTS_PER_SYSTEMFRAME(interval)) & 0x3FF;

        // ORAN frameId, 8 bits, [0, 255]
        frameId = (frameId & 0xff);

        num_eAxc = xran_get_num_eAxcUl(pHandle);
        num_CCPorts = xran_get_num_cc(pHandle);

        print_dbg("[%d]SFN %d sf %d slot %d\n", tti, frameId, sfId, slotId);

        bool useSectType3 = xran_use_sect_type_3(pDevCtx, bufId, XRAN_DIR_UL, mu);

        /* General Uplink */
#if defined(__INTEL_COMPILER)
#pragma vector always
#endif
        for(antId = nAntStart; (antId < (nAntStart + nAntNum)  && antId < num_eAxc); ++antId) {
            ruPortId = antId + pDevCtx->perMu[mu].eaxcOffset;
            for(ccId = nCcStart; (ccId < (nCcStart + nCcNum) && ccId < num_CCPorts); ccId++)
            {
                if(!xran_isactive_cc(pDevCtx, ccId))
                    continue;

                /* start new section information list */
                xran_cp_reset_section_info(pHandle, XRAN_DIR_UL, ccId, ruPortId, ctxId, mu);
                if(xran_fs_get_slot_type(PortId, ccId, tti, XRAN_SLOT_TYPE_UL, mu) == 1)
                {

                    pBufList = &(pDevCtx->perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[bufId][ccId][antId].sBufferList); /* To shorten reference */
                    if(pBufList->pBuffers && pBufList->pBuffers->pData)
                    {
                        ret = xran_cp_create_and_send_section(xran_port_id, ruPortId, XRAN_DIR_UL, tti, ccId,
                                                              (struct xran_prb_map *)(pBufList->pBuffers->pData), NULL,
                                                              pDevCtx->fh_cfg.ru_conf.xranCat, ctxId, mu, useSectType3, 1);
                    }
                }
            }
        } /* for(antId = 0; antId < num_eAxc; ++antId) */

        /*PRACH*/
        if(pDevCtx->perMu[mu].enablePrach && mu !=XRAN_NBIOT_MU)
        {
            struct xran_prach_cp_config *pPrachCPConfig = NULL;
                //check for dss enable and fill based on technology select the pDevCtx->perMu[mu].PrachCPConfig NR/LTE.
                if(pDevCtx->dssEnable){
                    int i = tti % pDevCtx->dssPeriod;
                    if(pDevCtx->technology[i]==1) {
                        pPrachCPConfig = &(pDevCtx->perMu[mu].PrachCPConfig);
                    }
                    else{
                        pPrachCPConfig = &(pDevCtx->perMu[mu].PrachCPConfigLTE);
                    }
                }
                else{
                    pPrachCPConfig = &(pDevCtx->perMu[mu].PrachCPConfig);
                }

                uint32_t is_prach_slot = xran_is_prach_slot(PortId, sfId, slotId, mu);

                if(((frameId % pPrachCPConfig->x) == pPrachCPConfig->y[0])
                    && (is_prach_slot==1))
                {
                    for(antId = nAntStart; (antId < nAntStart + nAntNum) && antId < num_eAxc; antId++)
                    {
                        portId = antId + pPrachCPConfig->prachEaxcOffset;
                        for(ccId = nCcStart; (ccId < (nCcStart + nCcNum) && ccId < num_CCPorts); ccId++)
                        {
                            if(!xran_isactive_cc(pDevCtx, ccId))
                                continue;

                            /* start new section information list */
                            xran_cp_reset_section_info(pHandle, XRAN_DIR_UL, ccId, portId, ctxId, mu);

                            //If rru_workaround flag is enabled, only send C-P for first occasion
                            if(likely(sysCfg->rru_workaround == 0))
                            {
                                occasionloop = pPrachCPConfig->occassionsInPrachSlot;
                            }
                            else
                            {
                                occasionloop = 1;
                            }
                            for(occasionid = 0; occasionid < occasionloop; occasionid++)
                            {
                                struct xran_cp_gen_params params;
                                struct xran_section_gen_info sect_geninfo[8];
                                struct xran_section_info sectInfo[8];
                                for(int secId=0;secId<8;secId++)
                                    sect_geninfo[secId].info = &sectInfo[secId];

                                struct rte_mbuf *mbuf = xran_ethdi_mbuf_alloc();
                                uint8_t seqid = xran_get_cp_seqid(pHandle, XRAN_DIR_UL, ccId, portId);

                                beam_id = xran_get_beamid(pHandle, XRAN_DIR_UL, ccId, portId, slotId);
                                ret = generate_cpmsg_prach(pHandle, &params, sect_geninfo, mbuf, pDevCtx,
                                            frameId, sfId, slotId, tti,
                                            beam_id, ccId, portId, occasionid, seqid, mu, 0);
                                if(ret == XRAN_STATUS_SUCCESS)
                                {
                                    if(unlikely(xran_transmit_cpmsg_prach(pHandle, &params, sect_geninfo, mbuf, pDevCtx, tti, ccId, portId, seqid, mu) != XRAN_STATUS_SUCCESS)){
                                        print_err("Error: PRACH CP transmit failed! ru_portId %d ccId %d tti %d\n", portId, ccId, tti);
                                    }
                                }
                                else
                                    print_err("generate_cpmsg_prach failed! tti %d, ccId %d portId %d",tti, ccId, portId);
                            }
                        }
                    }
                }
        } /* if(pDevCtx->perMu[mu].prachEnable) */

        /* SRS */
        if(pDevCtx->enableSrsCp)
        {
            struct xran_srs_config *pSrsCfg = &(pDevCtx->srs_cfg);
            uint16_t stream2srs = xran_get_num_ant_elm(pHandle)/num_eAxc; /* do poportional part of SRS (e.g 64/16=4 per nAntNum equal to 1*/
            for(antId = nAntStart*stream2srs;
                (antId < ((nAntStart + nAntNum)*stream2srs)  && antId < xran_get_num_ant_elm(pHandle) && antId < XRAN_MAX_ANT_ARRAY_ELM_NR);
                ++antId)
            {
                portId = antId + pSrsCfg->srsEaxcOffset;
                for(ccId = nCcStart; (ccId < (nCcStart + nCcNum) && ccId < num_CCPorts); ccId++)
                {
                    if(!xran_isactive_cc(pDevCtx, ccId))
                        continue;

                    /* start new section information list */
                    xran_cp_reset_section_info(pHandle, XRAN_DIR_UL, ccId, portId, ctxId, mu);
                    if(xran_fs_get_slot_type(PortId, ccId, tti, XRAN_SLOT_TYPE_UL,mu) == 1)
                    {
                        pBufList = &(pDevCtx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[bufId][ccId][antId].sBufferList); /* To shorten reference */
                        if(pBufList->pBuffers && pBufList->pBuffers->pData)
                        {
                            ret = xran_cp_create_and_send_section(xran_port_id, portId, XRAN_DIR_UL, tti, ccId,
                                                                  (struct xran_prb_map *)(pBufList->pBuffers->pData), NULL,
                                                                  pDevCtx->fh_cfg.ru_conf.xranCat, ctxId, mu, useSectType3, 1);
                        }
                    }
                }
            }
        } /* if(pDevCtx->enableSrs) */

        MLogXRANTask(PID_CP_UL_CB, t1, MLogXRANTick());
	MLogSetTaskCoreMap(TASK_3501);
    } /* if(pDevCtx->enableCP) */

    return ret;
}


void tx_cp_ul_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogXRANTick();
    int tti, bufId;
    int ret;
    uint32_t slotId = 0, sfId = 0, frameId = 0;
    int32_t ccId;
    int antId, portId;
    uint16_t occasionid = 0;
    uint8_t  occasionloop = 0;
    uint16_t beam_id;
    uint8_t num_eAxc, num_CCPorts, ruPortId;
    uint8_t ctxId;

    void *pHandle;
    uint32_t interval;
    uint8_t PortId;

    struct xran_timer_ctx *pTCtx;
    struct xran_buffer_list *pBufList;
#ifdef POLL_EBBU_OFFLOAD
    PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();
#endif

    if(unlikely(!arg))
    {
        print_err("Null xRAN context!!\n");
        return;
    }

    xran_device_per_mu_fields *p_MuPerDev = (xran_device_per_mu_fields *)arg;
    struct xran_device_ctx * pDevCtx = p_MuPerDev->p_dev_ctx;
    struct xran_system_config *sysCfg = xran_get_systemcfg();
    uint8_t mu = p_MuPerDev->mu;

    if(xran_get_syscfg_bbuoffload())
        return;

#ifdef POLL_EBBU_OFFLOAD
    first_call = pCtx->first_call;
#endif

    if(unlikely((pDevCtx->fh_cfg.ru_conf.xranCat != XRAN_CATEGORY_A) && (pDevCtx->fh_cfg.ru_conf.xranCat != XRAN_CATEGORY_B)))
    {
        print_err("Unsupported Category %d\n", pDevCtx->fh_cfg.ru_conf.xranCat);
        return;
    }
    /* */
    if(first_call && pDevCtx->enableCP)
    {
        pHandle     = pDevCtx;
        pTCtx       = &pDevCtx->perMu[mu].timer_ctx[0];
        interval    = xran_fs_get_tti_interval(mu);
        PortId      = pDevCtx->xran_port_id;
#ifndef POLL_EBBU_OFFLOAD
        tti         = pTCtx[(xran_lib_ota_tti_mu[PortId][mu] & 1) ^ 1].tti_to_process;
#else
        tti         = pTCtx[(pCtx->ebbu_offload_ota_tti_cnt_mu[PortId][mu] & 1) ^ 1].tti_to_process;
#endif

        /* tti_to_process is {OTA tti + 1}, so, odd value of tti_to_process implies
         * even value of OTA tti. For special case of NB-IOT and 3.75KHz scs,
         * following additional logic is needed */
        if(mu == XRAN_NBIOT_MU && pDevCtx->fh_cfg.perMu[mu].nbIotUlScs==XRAN_NBIOT_UL_SCS_3_75)
        {
            if(tti%2)
            {//tti_to_process is even --> OTA tti is odd
                if(pDevCtx->perMu[mu].ulCpSlotOffset)
                    return;
                //else send ul cp packet
            }
            else
            {//tti_to_process is odd --> OTA tti is even
                if(pDevCtx->perMu[mu].ulCpSlotOffset) //slot offset is >0
                {
                    //send ul cp packet
                }
                else
                    return;
            }

            tti         += pDevCtx->perMu[mu].ulCpSlotOffset; /* ulCpSlotOffset currently used for nb-iot 3.75 only */
            if(tti >= xran_fs_get_max_slot(mu)){
                print_dbg("[%d]SFN %d sf %d slot %d\n",tti, frameId, sfId, slotId);
                tti=0;
            }

            slotId     = 0;
        }
        else
        {
            slotId     = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME(interval));
        }


        bufId      = tti % XRAN_N_FE_BUF_LEN;
        ctxId      = tti % XRAN_MAX_SECTIONDB_CTX;
        sfId = XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME(interval),  SUBFRAMES_PER_SYSTEMFRAME);
        frameId    = XranGetFrameNum(tti, xran_getSfnSecStart(), SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval));

        /* Wrap around to next second */
        if(tti == 0)
            frameId = (frameId + NUM_OF_FRAMES_PER_SECOND) & 0x3ff;

        num_eAxc = xran_get_num_eAxcUl(pHandle);
        num_CCPorts = xran_get_num_cc(pHandle);

        print_dbg("[%d]SFN %d sf %d slot %d\n", tti, frameId, sfId, slotId);

        bool useSectType3 = xran_use_sect_type_3(pDevCtx, bufId, XRAN_DIR_UL, mu);

        /* General Uplink */
        for(antId = 0; antId < num_eAxc; antId++)
        {
            ruPortId = antId + pDevCtx->perMu[mu].eaxcOffset;
            for(ccId = 0; ccId < num_CCPorts; ccId++)
            {
                if(!xran_isactive_cc(pDevCtx, ccId))
                    continue;

                /* start new section information list */
                xran_cp_reset_section_info(pHandle, XRAN_DIR_UL, ccId, ruPortId, ctxId, mu);
                if(xran_fs_get_slot_type(PortId, ccId, tti, XRAN_SLOT_TYPE_UL, mu) == 1)
                {
                    pBufList = &(pDevCtx->perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[bufId][ccId][antId].sBufferList); /* To shorten reference */
                    if(pBufList->pBuffers && pBufList->pBuffers->pData)
                    {
                        ret = xran_cp_create_and_send_section(PortId, ruPortId, XRAN_DIR_UL, tti, ccId,
                                                              (struct xran_prb_map *)(pBufList->pBuffers->pData), NULL,
                                                              pDevCtx->fh_cfg.ru_conf.xranCat, ctxId, mu, useSectType3, 0);
                    }
                }
            }
        } /* for(antId = 0; antId < num_eAxc; ++antId) */

        /* PRACH */
        if(pDevCtx->perMu[mu].enablePrach  && mu != XRAN_NBIOT_MU)
        {
            struct xran_prach_cp_config *pPrachCPConfig = NULL;

            //check for dss enable and fill based on technology select the pDevCtx->perMu[mu].PrachCPConfig NR/LTE.
            if(pDevCtx->dssEnable){
                int i = tti % pDevCtx->dssPeriod;
                if(pDevCtx->technology[i]==1) {
                    pPrachCPConfig = &(pDevCtx->perMu[mu].PrachCPConfig);
                }
                else{
                    pPrachCPConfig = &(pDevCtx->perMu[mu].PrachCPConfigLTE);
                }
            }
            else{
                pPrachCPConfig = &(pDevCtx->perMu[mu].PrachCPConfig);
            }

            uint32_t is_prach_slot = xran_is_prach_slot(PortId, sfId, slotId, mu);

            if(((frameId % pPrachCPConfig->x) == pPrachCPConfig->y[0])
                    && (is_prach_slot==1))
            {
                for(antId = 0; antId < num_eAxc; antId++)
                {
                    portId = antId + pPrachCPConfig->prachEaxcOffset;
                    for(ccId = 0; ccId < num_CCPorts; ccId++)
                    {
                        if(!xran_isactive_cc(pDevCtx, ccId))
                            continue;

                        /* start new section information list */
                        xran_cp_reset_section_info(pHandle, XRAN_DIR_UL, ccId, portId, ctxId, mu);

                        //If rru_workaround flag is enabled, only send C-P for first occasion
                        if(likely(sysCfg->rru_workaround == 0))
                        {
                            occasionloop = pPrachCPConfig->occassionsInPrachSlot;
                        }
                        else
                        {
                            occasionloop = 1;
                        }
                        for(occasionid = 0; occasionid < occasionloop; occasionid++)

                        {
                            struct xran_cp_gen_params params;
                            struct xran_section_gen_info sect_geninfo[8];
                            struct xran_section_info sectInfo[8];
                            for(int secId=0;secId<8;secId++)
                                sect_geninfo[secId].info = &sectInfo[secId];

                            struct rte_mbuf *mbuf = xran_ethdi_mbuf_alloc();
                            uint8_t seqid = xran_get_cp_seqid(pHandle, XRAN_DIR_UL, ccId, portId);

                            beam_id = xran_get_beamid(pHandle, XRAN_DIR_UL, ccId, portId, slotId);
                            ret = generate_cpmsg_prach(pHandle, &params, sect_geninfo, mbuf, pDevCtx,
                                    frameId, sfId, slotId, tti,
                                    beam_id, ccId, portId, occasionid, seqid, mu, 0);
                            if(ret == XRAN_STATUS_SUCCESS)
                            {
                                if(unlikely(xran_transmit_cpmsg_prach(pHandle, &params, sect_geninfo, mbuf, pDevCtx, tti, ccId, portId, seqid, mu) != XRAN_STATUS_SUCCESS)){
                                    print_err("Error: PRACH CP transmit failed! ru_portId %d ccId %d tti %d\n", portId, ccId, tti);
                                }
                            }
                            else
                                print_err("generate_cpmsg_prach failed! tti %d, ccId %d portId %d",tti, ccId, portId);
                        }
                    }
                }
            }
        } /* if(pDevCtx->perMu[mu].prachEnable) */

        /* SRS */
        if(pDevCtx->enableSrsCp)
        {
            struct xran_srs_config *pSrsCfg = &(pDevCtx->srs_cfg);

            for(antId = 0; antId < xran_get_num_ant_elm(pHandle); antId++)
            {
                portId = antId + pSrsCfg->srsEaxcOffset;
                for(ccId = 0; ccId < num_CCPorts; ccId++)
                {
                    if(!xran_isactive_cc(pDevCtx, ccId))
                        continue;

                    /* start new section information list */
                    xran_cp_reset_section_info(pHandle, XRAN_DIR_UL, ccId, portId, ctxId, mu);
                    if(xran_fs_get_slot_type(PortId, ccId, tti, XRAN_SLOT_TYPE_UL, mu) == 1)
                    {
                        pBufList = &(pDevCtx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[bufId][ccId][antId].sBufferList); /* To shorten reference */
                        if(pBufList->pBuffers && pBufList->pBuffers->pData)
                        {
                            ret = xran_cp_create_and_send_section(PortId, portId, XRAN_DIR_UL, tti, ccId,
                                                                  (struct xran_prb_map *)(pBufList->pBuffers->pData), NULL,
                                                                  pDevCtx->fh_cfg.ru_conf.xranCat, ctxId, mu, useSectType3, 0);
                        }
                    }
                }
            }
        } /* if(pDevCtx->enableSrs) */

    MLogXRANTask(PID_CP_UL_CB, t1, MLogXRANTick());
    } /* if(pDevCtx->enableCP) */
}

void tti_to_phy_cb(struct rte_timer *tim, void *arg)
{
    long t1 = MLogTick();
    uint8_t mu;
    uint32_t interval;
    struct xran_timing_source_ctx *pTmCtx;

    pTmCtx      = xran_timingsource_get_ctx();
    mu          = (uint8_t)((uint64_t)arg);
    interval    = xran_fs_get_tti_interval(mu);

#ifdef POLL_EBBU_OFFLOAD
    PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();
#endif

#ifdef POLL_EBBU_OFFLOAD
    if(pCtx->first_call)
#else
    if(first_call)
#endif
    {
        if(pTmCtx->ttiCb[XRAN_CB_TTI])
        {
            if(pTmCtx->SkipTti[XRAN_CB_TTI] <= 0)
            {
                pTmCtx->ttiCb[XRAN_CB_TTI](pTmCtx->TtiCbParam[XRAN_CB_TTI], mu);
            }
            else
            {
                pTmCtx->SkipTti[XRAN_CB_TTI]--;
            }
        }
    }
    else
    {
        if(pTmCtx->ttiCb[XRAN_CB_TTI])
        {
#ifdef POLL_EBBU_OFFLOAD
            int32_t tti = (int32_t)XranGetTtiNum(pCtx->ebbu_offload_ota_sym_cnt_mu[mu], XRAN_NUM_OF_SYMBOL_PER_SLOT);
#else
            int32_t tti = (int32_t)XranGetTtiNum(xran_lib_ota_sym_idx_mu[mu], XRAN_NUM_OF_SYMBOL_PER_SLOT);
#endif
            uint32_t slotId = XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME(interval));
            uint32_t sfId   = XranGetSubFrameNum(tti, SLOTNUM_PER_SUBFRAME(interval), SUBFRAMES_PER_SYSTEMFRAME);
            uint32_t frameId= XranGetFrameNum(tti, xran_getSfnSecStart(), SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval));

            if((frameId == xran_max_frame) && (sfId==9) && (slotId == SLOTNUM_PER_SUBFRAME(interval)-1))
            {
#ifdef POLL_EBBU_OFFLOAD
                pCtx->first_call = 1;
#else
                first_call = 1;
#endif
            }
        }
    }

    MLogTask(PID_TTI_CB_TO_PHY, t1, MLogTick());
}


#define MBUFS_CNT 16
int32_t xran_handle_rx_pkts(struct rte_mbuf* pkt_q[], uint16_t xport_id, struct xran_eaxc_info *p_cid, uint16_t num, uint16_t vf_id)
{
    struct rte_mbuf *pkt;
    uint16_t i;
    struct rte_ether_hdr* eth_hdr;
    struct xran_ecpri_hdr* ecpri_hdr;
    unsigned long t1;
    int32_t ret = MBUF_FREE;
    uint32_t ret_data[MBUFS_CNT] = { MBUFS_CNT * MBUF_FREE };
    struct xran_device_ctx* p_dev_ctx = xran_dev_get_ctx_by_id(xport_id);
    struct xran_system_config *sysCfg = xran_get_systemcfg();
    uint16_t num_data = 0, num_control = 0, num_meas = 0, num_cfm = 0;
    struct rte_mbuf* pkt_data[MBUFS_CNT], * pkt_control[MBUFS_CNT], * pkt_meas[MBUFS_CNT], *pkt_adj[MBUFS_CNT], *pkt_cfm[MBUFS_CNT];
    static uint32_t owdm_rx_first_pass = 1;
    uint32_t expected_ecpri_payload;

    if (unlikely(p_dev_ctx == NULL))
        return ret;

    for (i = 0; i < num; ++i)
    {
        pkt = pkt_q[i];

//        rte_prefetch0(rte_pktmbuf_mtod(pkt, void*));

        eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr*);

        if(eth_hdr->ether_type == rte_cpu_to_be_16(ETHER_TYPE_ECPRI))
        {
            rte_pktmbuf_adj(pkt, sizeof(*eth_hdr));
            ecpri_hdr = rte_pktmbuf_mtod(pkt, struct xran_ecpri_hdr*);
            expected_ecpri_payload = rte_be_to_cpu_16(ecpri_hdr->cmnhdr.bits.ecpri_payl_size);

            p_dev_ctx->fh_counters.rx_bytes_counter += rte_pktmbuf_pkt_len(pkt);

            struct radio_app_common_hdr *radio_hdr =
            rte_pktmbuf_mtod_offset(pkt, struct radio_app_common_hdr *, sizeof(*ecpri_hdr));
#if 0   /* Cov */
            if (radio_hdr == NULL)
            {
                rte_pktmbuf_free(pkt);
                continue;
            }
#endif
            if(unlikely(xran_get_syscfg_appmode() == O_DU
                && radio_hdr->data_feature.data_direction == XRAN_DIR_DL))
            {
                ++p_dev_ctx->fh_counters.rx_err_drop;
                ++p_dev_ctx->fh_counters.rx_counter;
                rte_pktmbuf_free(pkt);
                continue;
            }

            if(unlikely(xran_get_syscfg_appmode() == O_RU
                        && ecpri_hdr->cmnhdr.bits.ecpri_mesg_type==0
                        && radio_hdr->data_feature.data_direction == XRAN_DIR_UL))
            {
                ++p_dev_ctx->fh_counters.rx_err_drop;
                ++p_dev_ctx->fh_counters.rx_counter;
                rte_pktmbuf_free(pkt);
                continue;
            }
            pkt_adj[i] = pkt;
            switch (ecpri_hdr->cmnhdr.bits.ecpri_mesg_type)
            {
                case ECPRI_IQ_DATA:
                    // ECPRI payload validation
                    if(likely(sysCfg->rru_workaround == 0))
                    {
                        if(unlikely(expected_ecpri_payload != (rte_pktmbuf_pkt_len(pkt) - sizeof(union xran_ecpri_cmn_hdr))))
                        {
                            ++p_dev_ctx->fh_counters.rx_err_ecpri;
                            ++p_dev_ctx->fh_counters.rx_err_drop;
                            ++p_dev_ctx->fh_counters.rx_counter;
                            rte_pktmbuf_free(pkt);
                            continue;
                        }
                    }
                    pkt_data[num_data++] = pkt;
                    break;
                // For RU emulation
                case ECPRI_RT_CONTROL_DATA:
                    pkt_control[num_control++] = pkt;
                    break;
                case ECPRI_DELAY_MEASUREMENT:
                    // ECPRI payload validation
                    if(unlikely(expected_ecpri_payload != ((rte_pktmbuf_pkt_len(pkt) - sizeof(union xran_ecpri_cmn_hdr) - 4)))) //Subtracting 4 bytes for FCS
                    {
                        ++p_dev_ctx->fh_counters.rx_err_ecpri;
                        ++p_dev_ctx->fh_counters.rx_err_drop;
                        ++p_dev_ctx->fh_counters.rx_counter;
                        rte_pktmbuf_free(pkt);
                        continue;
                    }
                    if (owdm_rx_first_pass != 0)
                    {
                        // Initialize and verify that Payload Length is in range */
                        xran_initialize_and_verify_owd_pl_length((void*)p_dev_ctx);
                        owdm_rx_first_pass = 0;
                    }
                    pkt_meas[num_meas++] = pkt;
                    break;
                default:
                    if(xran_get_syscfg_appmode() == O_DU)
                    {
                        ++p_dev_ctx->fh_counters.rx_err_ecpri;
                        ++p_dev_ctx->fh_counters.rx_err_drop;
                        rte_pktmbuf_free(pkt);
                        print_dbg("Invalid eCPRI message type - %d", ecpri_hdr->cmnhdr.bits.ecpri_mesg_type);
                    }
                    break;
            }
        }
        else if(eth_hdr->ether_type == rte_cpu_to_be_16(ETHER_TYPE_CFM))
        {
            pkt_cfm[num_cfm++] = pkt;
        }
        else
        {
            rte_pktmbuf_free(pkt);
            continue;
        }
    }

    if(num_data == MBUFS_CNT && p_dev_ctx->fh_cfg.ru_conf.xranCat == XRAN_CATEGORY_B) /* w/a for Cat A issue */
    {
        for (i = 0; i < MBUFS_CNT; ++i)
        {
            ret_data[i] = MBUF_FREE;
        }
        if(p_dev_ctx->xran2phy_mem_ready != 0)
        {
            ret = process_mbuf_batch(pkt_data, (void*)p_dev_ctx, MBUFS_CNT, p_cid, ret_data);
        }
        for(i = 0; i < MBUFS_CNT; ++i)
        {
            if (ret_data[i] == MBUF_FREE)
                rte_pktmbuf_free(pkt_data[i]);
        }
    }
    else
    {
        // uint64_t tt1 = MLogXRANTick();
        for (i = 0; i < num_data; ++i)
        {
            ret = process_mbuf(pkt_data[i], (void*)p_dev_ctx, p_cid);
            if (ret == MBUF_FREE)
                rte_pktmbuf_free(pkt_data[i]);
        }
        // MLogXRANTask(PID_PROC_UP_PKT_PARSE, tt1, MLogXRANTick());
        if(xran_get_syscfg_appmode() == O_RU)
        {
            for (i = 0; i < num_control; ++i)
            {
                t1 = MLogXRANTick();
                ret = process_cplane(pkt_control[i], (void*)p_dev_ctx);
                ++p_dev_ctx->fh_counters.rx_counter;
                if (ret == MBUF_FREE)
                    rte_pktmbuf_free(pkt_control[i]);
                MLogXRANTask(PID_PROC_CP_PKT, t1, MLogXRANTick());
            }
        }
        else
        {
            p_dev_ctx->fh_counters.rx_err_ecpri += num_control;
            p_dev_ctx->fh_counters.rx_err_drop += num_control;
            print_dbg("O-DU recevied C-Plane message!");
        }

        for (i = 0; i < num_meas; ++i)
        {
            /*if(xran_get_syscfg_appmode() == O_RU)
                printf("Got delay_meas_pkt xport_id %d p_dev_ctx %08"PRIx64" %d\n", xport_id,(int64_t*)p_dev_ctx, num_meas) ;*/
            t1 = MLogXRANTick();
            if(xran_if_current_state != XRAN_RUNNING)
                ret = process_delay_meas(pkt_meas[i], (void*)p_dev_ctx, xport_id);
            else
                ret = MBUF_FREE;
            if (ret == MBUF_FREE)
                rte_pktmbuf_free(pkt_meas[i]);
            MLogXRANTask(PID_PROC_DELAY_MEAS_PKT, t1, MLogXRANTick());
        }

        for(i=0; i < num_cfm; ++i)
        {
            if(likely(xran_if_current_state == XRAN_RUNNING))
            {
                if(xran_ethdi_get_ctx()->lbm_port_info[vf_id].lbm_enable == 1)
                {
                    if(xran_process_cfm_message(pkt_cfm[i], vf_id) != XRAN_STATUS_SUCCESS){
                        print_err("xran_process_cfm_message failed");
                        p_dev_ctx->fh_counters.rx_err_drop += 1;
                    }
                }
                else
                {
                    xran_ethdi_get_ctx()->lbm_port_info[vf_id].stats.numRxLBRsIgnored += 1;
                }
            }
            rte_pktmbuf_free(pkt_cfm[i]);
        }
    }

    return MBUF_FREE;
}

void xran_initialize_ecpri_owd_meas_cmn(struct xran_device_ctx *ptr, int appMode)
{
//    ptr->eowd_cmn.initiator_en = 0; // Initiator 1, Recipient 0
//    ptr->eowd_cmn.filterType = 0;  // 0 Simple average based on number of measurements
    // Set default values if the Timeout and numberOfSamples are not set
    if(ptr->eowd_cmn[appMode].responseTo == 0)
        ptr->eowd_cmn[appMode].responseTo = 10E6; // 10 ms timeout expressed in ns
    if(ptr->eowd_cmn[appMode].numberOfSamples == 0)
        ptr->eowd_cmn[appMode].numberOfSamples = 8; // Number of samples to be averaged
}
void xran_initialize_ecpri_owd_meas_per_port (int i, struct xran_io_cfg *ptr )
{
   /* This function initializes one_way delay measurements on a per port basis,
      most variables default to zero    */
   ptr->eowd_port[ptr->id][i].portid = (uint8_t)i;
}

int32_t xran_init(int argc, char *argv[],
           struct xran_fh_init *p_xran_fh_init, char *appName, void **pXranLayerHandle)
{
    int32_t ret = XRAN_STATUS_SUCCESS;
    int32_t i,iMu;
    int32_t o_xu_id = 0;
    struct xran_io_cfg      *p_io_cfg       = NULL;
    struct xran_device_ctx * pDevCtx = NULL;
    int32_t  lcore_id = 0;
    const char *version = rte_version();
    struct xran_system_config *sysCfg;
    struct xran_timing_source_ctx *pTmCtx;

    if (version == NULL)
        rte_panic("version == NULL");

    printf("'%s' (XRAN_N_FE_BUF_LEN=%d)\n", version, XRAN_N_FE_BUF_LEN);

    if (p_xran_fh_init->xran_ports < 1 || p_xran_fh_init->xran_ports > XRAN_PORTS_NUM)
    {
        ret = XRAN_STATUS_INVALID_PARAM;
        print_err("fh_init xran_ports= %d is wrong [%d]\n", p_xran_fh_init->xran_ports, ret);
        return ret;
    }
    mlogxranenable = p_xran_fh_init->mlogxranenable;
    p_io_cfg = (struct xran_io_cfg *)&p_xran_fh_init->io_cfg;

    /* default values if not set */
    if(p_io_cfg->nEthLinePerPort == 0)
        p_io_cfg->nEthLinePerPort = 1;

    if(p_io_cfg->nEthLineSpeed == 0)
        p_io_cfg->nEthLineSpeed = 25;

    /** at least 1 RX Q */
    if(p_io_cfg->num_rxq == 0)
        p_io_cfg->num_rxq = 1;

#if (RTE_VER_YEAR < 21) /* eCPRI flow supported with DPDK 21.02 or later */
    if (p_io_cfg->num_rxq > 1)
    {
        p_io_cfg->num_rxq =  1;
        printf("%s does support eCPRI flows. Set rxq to %d\n", version, p_io_cfg->num_rxq);
    }
#endif

    printf("PF Eth line speed %dG\n",p_io_cfg->nEthLineSpeed);
    printf("PF Eth lines per O-xU port %d\n",p_io_cfg->nEthLinePerPort);
    printf("RX HW queues per O-xU Eth line %d \n",p_io_cfg->num_rxq);

    if(p_xran_fh_init->xran_ports * p_io_cfg->nEthLinePerPort *(2 - 1* p_io_cfg->one_vf_cu_plane)  != p_io_cfg->num_vfs)
    {
        print_err("Incorrect VFs configurations: For %d O-xUs with %d Ethernet ports expected number of VFs is %d. [provided %d]\n",
            p_xran_fh_init->xran_ports, p_io_cfg->nEthLinePerPort,
            p_xran_fh_init->xran_ports * p_io_cfg->nEthLinePerPort *(2 - 1* p_io_cfg->one_vf_cu_plane), p_io_cfg->num_vfs);
    }

    /* Basic EAL initialization */
    xran_ethdi_init_dpdk(p_xran_fh_init->filePrefix, p_xran_fh_init->dpdkVfioVfToken, p_io_cfg);

    sysCfg = xran_get_systemcfg();
    if(sysCfg)
    {
        memset(sysCfg, 0, sizeof(struct xran_system_config));

        memcpy(&(sysCfg->io_cfg), p_io_cfg, sizeof(struct xran_io_cfg));
        sysCfg->filePrefix  = p_xran_fh_init->filePrefix;
        sysCfg->numRUs  = p_xran_fh_init->xran_ports;
        rte_spinlock_init(&sysCfg->spinLock);
        sysCfg->active_nRU  = 0;
        sysCfg->active_RU   = 0;

        rte_atomic32_init(&sysCfg->nStart);
        rte_atomic32_clear(&sysCfg->nStart);

        sysCfg->xran2phy_mem_ready  = 0;

        sysCfg->ttiBaseMu   = p_xran_fh_init->ttiBaseMu;

        sysCfg->debugStop       = p_xran_fh_init->debugStop;
        sysCfg->debugStopCount  = p_xran_fh_init->debugStopCount;
        sysCfg->mlogEnable      = p_xran_fh_init->mlogEnable;
        sysCfg->logLevel        = p_xran_fh_init->logLevel;
        sysCfg->mlogEnable      = p_xran_fh_init->mlogxranenable;   // will be removed
        sysCfg->rru_workaround  = p_xran_fh_init->rru_workaround;
        sysCfg->bbdevEnc        = p_xran_fh_init->bbdevEnc;
        sysCfg->bbdevDec        = p_xran_fh_init->bbdevDec;
        sysCfg->bbdevSrsFft     = p_xran_fh_init->bbdevSrsFft;
        sysCfg->bbdevPrachIfft     = p_xran_fh_init->bbdevPrachIfft;
    }

    pTmCtx  = xran_timingsource_get_ctx();
    for(i = 0; i < MAX_TTI_TO_PHY_TIMER; i++ )
        for(iMu = 0; iMu < XRAN_MAX_NUM_MU; iMu++)
            rte_timer_init(&pTmCtx->tti_to_phy_timer[i][iMu]);

    if((ret = xran_dev_create_ctx(p_xran_fh_init->xran_ports)) < 0)
    {
        print_err("context allocation error [%d]\n", ret);
        return ret;
    }

    for(o_xu_id = 0; o_xu_id < p_xran_fh_init->xran_ports; o_xu_id++)
    {
        pDevCtx  = xran_dev_get_ctx_by_id(o_xu_id);
        memset(pDevCtx, 0, sizeof(struct xran_device_ctx));
        pDevCtx->xran_port_id  = o_xu_id;

        /* copy init */
        pDevCtx->mtu = p_xran_fh_init->mtu;
        printf("RU%d MTU %d\n", pDevCtx->xran_port_id, pDevCtx->mtu);

        pDevCtx->totalBfWeights = p_xran_fh_init->totalBfWeights;
        pDevCtx->dlCpProcBurst  = p_xran_fh_init->dlCpProcBurst;

        pDevCtx->numRxq         = p_io_cfg->num_rxq;

        rte_spinlock_init(&pDevCtx->spinLock);
        pDevCtx->active_CC  = 0;
        pDevCtx->active_nCC = 0;
        for(i=0; i < XRAN_MAX_SECTOR_NR; i++)
            pDevCtx->cfged_CCId[i]  = -1;

        memcpy(&(pDevCtx->eAxc_id_cfg), &(p_xran_fh_init->eAxCId_conf[o_xu_id]), sizeof(struct xran_eaxcid_config));

        /* To make sure to set default functions */
        pDevCtx->send_upmbuf2ring    = NULL;
        pDevCtx->send_cpmbuf2ring    = NULL;

        // Ecpri initialization for One Way delay measurements common variables to default values
        pDevCtx->p_o_du_addr    = p_xran_fh_init->p_o_du_addr;  /* TODO: need to revisit */
        pDevCtx->p_o_ru_addr    = p_xran_fh_init->p_o_ru_addr;  /* TODO: need to revisit */
        xran_initialize_ecpri_owd_meas_cmn(pDevCtx, p_io_cfg->id);

        for(i = 0; i < MAX_NUM_OF_DPDK_TIMERS; i++)
            rte_timer_init(&pDevCtx->dpdk_timer[i]);

        pDevCtx->direct_pool   = socket_direct_pool;
        pDevCtx->indirect_pool = socket_indirect_pool;

        *pXranLayerHandle = pDevCtx;
        pXranLayerHandle++;
    }

    xran_if_current_state = XRAN_INIT;

    /* Note - xran_handle_rx_pkts processes ETHER_TYPE_ECPRI & ETHER_TYPE_CFM packets */
    xran_register_ethertype_handler(ETHER_TYPE_ECPRI, xran_handle_rx_pkts);
    if(p_io_cfg->id == O_DU)
        xran_ethdi_init_dpdk_ports(p_io_cfg, &lcore_id,
                           (struct rte_ether_addr *)p_xran_fh_init->p_o_du_addr,
                           (struct rte_ether_addr *)p_xran_fh_init->p_o_ru_addr,
                           p_xran_fh_init->mtu);
    else
        xran_ethdi_init_dpdk_ports(p_io_cfg, &lcore_id,
                           (struct rte_ether_addr *)p_xran_fh_init->p_o_ru_addr,
                           (struct rte_ether_addr *)p_xran_fh_init->p_o_du_addr,
                           p_xran_fh_init->mtu);

    for(i=0; i<XRAN_PORTS_NUM; i++)
    {
        for(uint32_t nCellIdx = 0; nCellIdx < XRAN_MAX_SECTOR_NR; nCellIdx++)
        {
            xran_fs_clear_slot_type(i,nCellIdx);
        }
    }

    /* Init LBM info */
    if(p_xran_fh_init->lbmEnable)
    {
        struct xran_ethdi_ctx *eth_ctx = xran_ethdi_get_ctx();
        eth_ctx->lbmEnable                          = p_xran_fh_init->lbmEnable;
        eth_ctx->lbm_common_info.LBMPeriodicity     = p_xran_fh_init->lbm_common_info.LBMPeriodicity;
        eth_ctx->lbm_common_info.LBRTimeOut         = p_xran_fh_init->lbm_common_info.LBRTimeOut;
        eth_ctx->lbm_common_info.numRetransmissions = p_xran_fh_init->lbm_common_info.numRetransmissions;
        eth_ctx->lbm_common_info.nextLBMtime        = 0;

        /* Register OAM callback with L1 */
        p_xran_fh_init->oam_cb_func[XRAN_PHY_OAM_MANAGE_LBM]    =  xran_oam_phy2xran_manage_lbm;

        for(i=0; i < p_xran_fh_init->io_cfg.num_vfs; ++i)
        {
            xran_lbm_port_info *port_info_loc = &eth_ctx->lbm_port_info[i];

            port_info_loc->expectedLBRtransID          = 0;
            port_info_loc->LBRreceived                 = 0;
            port_info_loc->linkStatus                  = 0;
            port_info_loc->nextLBMtransID              = 0;
            port_info_loc->stats.rxValidInOrderLBRs    = 0;
            port_info_loc->stats.rxValidOutOfOrderLBRs = 0;
            port_info_loc->stats.numRxLBRsIgnored = 0;
            port_info_loc->lbm_state                   = XRAN_LBM_STATE_INIT;
            port_info_loc->lbm_enable = eth_ctx->lbmEnable;
        }
    }

    // The ecpri initialization loop needs to be done per pf and vf (Outer loop pf and inner loop vf)
    for (i=0;  i< p_io_cfg->num_vfs; i++)
    {
        /* Initialize ecpri one-way delay measurement info on a per vf port basis */
        xran_initialize_ecpri_owd_meas_per_port (i, p_io_cfg);
    }

    if(p_io_cfg->bbdev_mode != XRAN_BBDEV_NOT_USED)
    {
        struct xran_ethdi_ctx *eth_ctx = xran_ethdi_get_ctx();

        eth_ctx->bbdev_dec = p_xran_fh_init->bbdevDec;
        eth_ctx->bbdev_enc = p_xran_fh_init->bbdevEnc;
        eth_ctx->bbdev_srs_fft = p_xran_fh_init->bbdevSrsFft;
        eth_ctx->bbdev_prach_ifft = p_xran_fh_init->bbdevPrachIfft;

    }

    return ret;
}

void xran_cleanup(void)
{
    rte_timer_subsystem_finalize();

    xran_dev_destroy_ctx();

    xran_mem_mgr_leak_detector_display(2);

#ifdef RTE_LIB_PDUMP
    if (rte_eal_process_type() == RTE_PROC_PRIMARY)
    {
        /* uninitialize packet capture framework */
        rte_pdump_uninit();
    }
#endif
    rte_eal_cleanup();

    return;
}

int32_t
xran_sector_get_instances (uint32_t xran_port, void * pDevHandle, uint16_t nNumInstances,
               xran_cc_handle_t * pSectorInstanceHandles)
{
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

    for (i = 0; i < nNumInstances; i++)
    {
        if(pSectorInstanceHandles[i] == NULL)
        {
            /* Allocate Memory for CC handles */
            pCcHandle = (XranSectorHandleInfo *) xran_malloc("xran_cc_handles", sizeof (XranSectorHandleInfo), 64);
            if(pCcHandle == NULL)
                return XRAN_STATUS_RESOURCE;

            memset(pCcHandle, 0, (sizeof (XranSectorHandleInfo)));

            pCcHandle->nIndex    = i;
            pCcHandle->nXranPort = pDev->xran_port_id;

            pLibInstanceHandles[pDev->xran_port_id][i] = pSectorInstanceHandles[i] = pCcHandle;
            printf("RU%d CC%d: Instance allocated.\n", pDev->xran_port_id, i);
        }
        else
        {
            printf("RU%d CC%d: Instance allocated already\n", pDev->xran_port_id, i);
        }
        //printf("  RU%d CC%d: Sector handle: %p Instance: %p\n", pDev->xran_port_id, i, &pSectorInstanceHandles[i], pSectorInstanceHandles[i]);
    }

    return XRAN_STATUS_SUCCESS;
}

int32_t xran_sector_free_instance(xran_cc_handle_t *pSectorInstanceHandle)
{
    if(pSectorInstanceHandle)
        xran_free(pSectorInstanceHandle);
    else
        return(-1);

    return(0);
}

int32_t xran_sector_free_all_instances(void)
{
    int i, j;

    for(i = 0; i < XRAN_PORTS_NUM; i++)
    {
        for(j=0; j < XRAN_MAX_SECTOR_NR; j++)
        {
            if(pLibInstanceHandles[i][j])
            {
                printf("Free Port%d CC%d handle %p\n", i, j, pLibInstanceHandles[i][j]);
                rte_free(pLibInstanceHandles[i][j]);
            }
        }
    }

    return(0);
}

int32_t
xran_5g_fronthault_config (void * pHandle,
                struct xran_buffer_list *pSrcBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pSrcCpBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pDstBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pDstCpBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                xran_transport_callback_fn pCallback,
                void *pCallbackTag, uint8_t mu)
{
    int j, i = 0, z;
    XranSectorHandleInfo* pXranCc = NULL;
    struct xran_device_ctx * pDevCtx = NULL;

    if(NULL == pHandle) {
        printf("Handle is NULL!\n");
        return XRAN_STATUS_FAIL;
    }

    pXranCc = (XranSectorHandleInfo*) pHandle;
    pDevCtx = xran_dev_get_ctx_by_id(pXranCc->nXranPort);
    if (pDevCtx == NULL) {
        printf ("pDevCtx is NULL\n");
        return XRAN_STATUS_FAIL;
    }

    i = pXranCc->nIndex;
    for(j = 0; j < XRAN_N_FE_BUF_LEN; j++) {
        for(z = 0; z < XRAN_MAX_ANTENNA_NR; z++){
                /* U-plane TX */

                pDevCtx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][i][z].bValid = 0;
                pDevCtx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
                pDevCtx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
                pDevCtx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
                pDevCtx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;

                if(pSrcBuffer[z][j])
                    pDevCtx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList =   *pSrcBuffer[z][j];

                /* C-plane TX */
                pDevCtx->perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].bValid = 0;
                pDevCtx->perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
                pDevCtx->perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
                pDevCtx->perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
                pDevCtx->perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;

                if(pSrcCpBuffer[z][j])
                    pDevCtx->perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList =   *pSrcCpBuffer[z][j];
                /* U-plane RX */

                pDevCtx->perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][i][z].bValid = 0;
                pDevCtx->perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
                pDevCtx->perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
                pDevCtx->perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
                pDevCtx->perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;

                if(pDstBuffer[z][j])
                    pDevCtx->perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList =   *pDstBuffer[z][j];

                /* C-plane RX */
                pDevCtx->perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].bValid = 0;
                pDevCtx->perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
                pDevCtx->perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
                pDevCtx->perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
                pDevCtx->perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;

                if(pDstCpBuffer[z][j])
                    pDevCtx->perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList =   *pDstCpBuffer[z][j];
        } //z loop (antennas)
    } // j loop - XRAN_N_FE_BUF_LEN

    /* only single wakeup callback is allowed per RU */
    if(pDevCtx->pCallback[0] == NULL)
    {
        pDevCtx->pCallback[0]    = pCallback;
        pDevCtx->pCallbackTag[0] = pCallbackTag;

        printf("\n%s: [p %d CC  %d] Cb %p cb %p\n",__FUNCTION__,
            pDevCtx->xran_port_id, 0, pDevCtx->pCallback[0], pDevCtx->pCallbackTag[0]);
    }


    pDevCtx->xran2phy_mem_ready = 1;

    return XRAN_STATUS_SUCCESS;
}

int32_t xran_5g_bfw_config(void * pHandle,
                    struct xran_buffer_list *pSrcRxCpBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                    struct xran_buffer_list *pSrcTxCpBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                    xran_transport_callback_fn pCallback,
                    void *pCallbackTag, uint8_t mu){
    int j, i = 0, z;
    XranSectorHandleInfo* pXranCc = NULL;
    struct xran_device_ctx * pDevCtx = NULL;

    if(NULL == pHandle) {
        printf("Handle is NULL!\n");
        return XRAN_STATUS_FAIL;
    }
    pXranCc = (XranSectorHandleInfo*) pHandle;
    pDevCtx = xran_dev_get_ctx_by_id(pXranCc->nXranPort);
    if (pDevCtx == NULL) {
        printf ("pDevCtx is NULL\n");
        return XRAN_STATUS_FAIL;
    }

    i = pXranCc->nIndex;

    for(j = 0; j < XRAN_N_FE_BUF_LEN; j++) {
        for(z = 0; z < XRAN_MAX_ANTENNA_NR; z++){
            /* C-plane RX - RU */
            pDevCtx->perMu[mu].sFHCpRxPrbMapBbuIoBufCtrl[j][i][z].bValid = 0;
            pDevCtx->perMu[mu].sFHCpRxPrbMapBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
            pDevCtx->perMu[mu].sFHCpRxPrbMapBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
            pDevCtx->perMu[mu].sFHCpRxPrbMapBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
            pDevCtx->perMu[mu].sFHCpRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;

            if(pSrcRxCpBuffer[z][j])
                pDevCtx->perMu[mu].sFHCpRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList =   *pSrcRxCpBuffer[z][j];

            /* C-plane TX - RU */
            pDevCtx->perMu[mu].sFHCpTxPrbMapBbuIoBufCtrl[j][i][z].bValid = 0;
            pDevCtx->perMu[mu].sFHCpTxPrbMapBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
            pDevCtx->perMu[mu].sFHCpTxPrbMapBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
            pDevCtx->perMu[mu].sFHCpTxPrbMapBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
            pDevCtx->perMu[mu].sFHCpTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;

            if(pSrcTxCpBuffer[z][j])
                pDevCtx->perMu[mu].sFHCpTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList =   *pSrcTxCpBuffer[z][j];
        }
    }

    return XRAN_STATUS_SUCCESS;
} //xran_5g_bfw_config()

int32_t
xran_5g_prach_req (void *  pHandle,
                struct xran_buffer_list *pDstBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pDstBufferDecomp[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                xran_transport_callback_fn pCallback,
                void *pCallbackTag, uint8_t mu)
{
    int j, i = 0, z;
    XranSectorHandleInfo* pXranCc = NULL;
    struct xran_device_ctx * pDevCtx = NULL;

    if(NULL == pHandle) {
        printf("Handle is NULL!\n");
        return XRAN_STATUS_FAIL;
    }

    pXranCc = (XranSectorHandleInfo*) pHandle;
    pDevCtx = xran_dev_get_ctx_by_id(pXranCc->nXranPort);
    if (pDevCtx == NULL) {
        printf ("pDevCtx is NULL\n");
        return XRAN_STATUS_FAIL;
    }

    i = pXranCc->nIndex;

    for(j = 0; j < XRAN_N_FE_BUF_LEN; j++) {
        for(z = 0; z < XRAN_MAX_PRACH_ANT_NUM; z++){
            pDevCtx->perMu[mu].sFHPrachRxBbuIoBufCtrl[j][i][z].bValid = 0;
            pDevCtx->perMu[mu].sFHPrachRxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
            pDevCtx->perMu[mu].sFHPrachRxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
            pDevCtx->perMu[mu].sFHPrachRxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
            pDevCtx->perMu[mu].sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_MAX_PRACH_ANT_NUM; // ant number.
            if(pDstBuffer[z][j])
                pDevCtx->perMu[mu].sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList =   *pDstBuffer[z][j];

            if(pDstBufferDecomp[z][j])
                pDevCtx->perMu[mu].sFHPrachRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList =   *pDstBufferDecomp[z][j];
        }
    }


    /* only single wakeup callback is allowed per RU */
    if(pDevCtx->pPrachCallback[0] == NULL)
    {
        pDevCtx->pPrachCallback[0]    = pCallback;
        pDevCtx->pPrachCallbackTag[0] = pCallbackTag;

        printf("\n%s: [p %d CC  %d] Cb %p cb %p\n",__FUNCTION__,
            pDevCtx->xran_port_id, 0, pDevCtx->pPrachCallback[0], pDevCtx->pPrachCallbackTag[0]);
    }


    return XRAN_STATUS_SUCCESS;
}

int32_t
xran_5g_srs_req (void *  pHandle,
                struct xran_buffer_list *pDstBuffer[XRAN_MAX_ANT_ARRAY_ELM_NR][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pDstCpBuffer[XRAN_MAX_ANT_ARRAY_ELM_NR][XRAN_N_FE_BUF_LEN],
                xran_transport_callback_fn pCallback,
                void *pCallbackTag, uint8_t mu)
{
    int j, i = 0, z;
    XranSectorHandleInfo* pXranCc = NULL;
    struct xran_device_ctx * pDevCtx = NULL;

    if(NULL == pHandle) {
        printf("Handle is NULL!\n");
        return XRAN_STATUS_FAIL;
    }

    pXranCc = (XranSectorHandleInfo*) pHandle;
    pDevCtx = xran_dev_get_ctx_by_id(pXranCc->nXranPort);
    if (pDevCtx == NULL) {
        printf ("pDevCtx is NULL\n");
        return XRAN_STATUS_FAIL;
    }

    i = pXranCc->nIndex;

    for(j=0; j<XRAN_N_FE_BUF_LEN; j++) {
        for(z = 0; z < XRAN_MAX_ANT_ARRAY_ELM_NR; z++){
            pDevCtx->perMu[mu].sFHSrsRxBbuIoBufCtrl[j][i][z].bValid = 0;
            pDevCtx->perMu[mu].sFHSrsRxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
            pDevCtx->perMu[mu].sFHSrsRxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
            pDevCtx->perMu[mu].sFHSrsRxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
            pDevCtx->perMu[mu].sFHSrsRxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_MAX_ANT_ARRAY_ELM_NR; // ant number.
            if(pDstBuffer[z][j])
                pDevCtx->perMu[mu].sFHSrsRxBbuIoBufCtrl[j][i][z].sBufferList =   *pDstBuffer[z][j];

            /* C-plane SRS */
            pDevCtx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].bValid = 0;
            pDevCtx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
            pDevCtx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
            pDevCtx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
            pDevCtx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;

            if(pDstCpBuffer[z][j])
                pDevCtx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList =   *pDstCpBuffer[z][j];
        }
    }


    /* only single wakeup callback is allowed per RU */
    if(pDevCtx->pSrsCallback[0] == NULL)
    {
        pDevCtx->pSrsCallback[0]    = pCallback;
        pDevCtx->pSrsCallbackTag[0] = pCallbackTag;

        printf("\n%s: [p %d CC  %d] Cb %p cb %p\n",__FUNCTION__,
            pDevCtx->xran_port_id, 0, pDevCtx->pSrsCallback[0], pDevCtx->pSrsCallbackTag[0]);
    }

    return XRAN_STATUS_SUCCESS;
}

int32_t
xran_5g_csirs_config (void *  pHandle,
                struct xran_buffer_list *pSrcBuffer[XRAN_MAX_CSIRS_PORTS][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pSrcCpBuffer[XRAN_MAX_CSIRS_PORTS][XRAN_N_FE_BUF_LEN],
                xran_transport_callback_fn pCallback,
                void *pCallbackTag, uint8_t mu)
{
    int j, i = 0, z;
    XranSectorHandleInfo* pXranCc = NULL;
    struct xran_device_ctx * pDevCtx = NULL;

    if(NULL == pHandle) {
        printf("Handle is NULL!\n");
        return XRAN_STATUS_FAIL;
    }

    pXranCc = (XranSectorHandleInfo*) pHandle;
    pDevCtx = xran_dev_get_ctx_by_id(pXranCc->nXranPort);
    if (pDevCtx == NULL) {
        printf ("pDevCtx is NULL\n");
        return XRAN_STATUS_FAIL;
    }

    i = pXranCc->nIndex;

    for(j=0; j<XRAN_N_FE_BUF_LEN; j++) {
        for(z = 0; z < XRAN_MAX_CSIRS_PORTS; z++){
            pDevCtx->perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][i][z].bValid = 0;
            pDevCtx->perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
            pDevCtx->perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
            pDevCtx->perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
            pDevCtx->perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_MAX_CSIRS_PORTS; // port number.
            if(pSrcBuffer[z][j])
                pDevCtx->perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][i][z].sBufferList =   *pSrcBuffer[z][j];

            /* C-plane CSI-RS */
            pDevCtx->perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][i][z].bValid = 0;
            pDevCtx->perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
            pDevCtx->perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
            pDevCtx->perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
            pDevCtx->perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;

            if(pSrcCpBuffer[z][j])
                pDevCtx->perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList =   *pSrcCpBuffer[z][j];
        }
    }

    /* only single wakeup callback is allowed per RU */
    if(pDevCtx->pCsirsCallback[0] == NULL)
    {
        pDevCtx->pCsirsCallback[0]    = pCallback;
        pDevCtx->pCsirsCallbackTag[0] = pCallbackTag;

        printf("\n%s: [p %d CC  %d] Cb %p cb %p\n",__FUNCTION__,
            pDevCtx->xran_port_id, 0, pDevCtx->pCsirsCallback[0], pDevCtx->pCsirsCallbackTag[0]);
    }

    return XRAN_STATUS_SUCCESS;
}

int32_t xran_5g_ssb_config(uint8_t ssbMu,
                           uint8_t actualMu,
                           void* pHandle,
                           struct xran_buffer_list *pSsbTxBuf[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                           struct xran_buffer_list *pSsbTxPrbBuf[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN])
{
    uint32_t cc_id, ant_id, tti;
    xran_device_ctx_t* pDevCtx;
    XranSectorHandleInfo* pXranCc = (XranSectorHandleInfo*)pHandle;
    if(pXranCc == NULL)
    {
        print_err("pXranCc == NULL");
        return XRAN_STATUS_FAIL;
    }

    pDevCtx = xran_dev_get_ctx_by_id(pXranCc->nXranPort);
    if (pDevCtx == NULL)
    {
        printf ("pDevCtx == NULL\n");
        return XRAN_STATUS_FAIL;
    }

    cc_id = pXranCc->nIndex;
    pDevCtx->vMuInfo.ssbInfo.ssbMu = ssbMu;
    /**
     * Refer WG4 CUS : RB mapping and support of mixed numerologies - example SSB Figure 7.2.3.2-4
    */
    pDevCtx->vMuInfo.ssbInfo.freqOffset = 240;

    /* SSB uses same RU PortIds as PRACH */
    if(pDevCtx->perMu[actualMu].PrachCPConfig.prachEaxcOffset != 0){
        pDevCtx->vMuInfo.ssbInfo.ruPortId_offset = pDevCtx->perMu[actualMu].PrachCPConfig.prachEaxcOffset;
    }
    else{
        pDevCtx->vMuInfo.ssbInfo.ruPortId_offset = pDevCtx->fh_cfg.perMu[actualMu].eaxcOffset + xran_get_num_eAxc(pDevCtx);
    }

    for(tti = 0; tti < XRAN_N_FE_BUF_LEN; ++tti)
    {
        for(ant_id = 0; ant_id < XRAN_MAX_ANTENNA_NR; ++ant_id)
        {
            /* U-Plane */
            pDevCtx->vMuInfo.ssbInfo.sFHSsbTxBbuIoBufCtrl[tti][cc_id][ant_id].bValid = 0;
            pDevCtx->vMuInfo.ssbInfo.sFHSsbTxBbuIoBufCtrl[tti][cc_id][ant_id].nSegGenerated = -1;
            pDevCtx->vMuInfo.ssbInfo.sFHSsbTxBbuIoBufCtrl[tti][cc_id][ant_id].nSegToBeGen = -1;
            pDevCtx->vMuInfo.ssbInfo.sFHSsbTxBbuIoBufCtrl[tti][cc_id][ant_id].nSegTransferred = 0;
            pDevCtx->vMuInfo.ssbInfo.sFHSsbTxBbuIoBufCtrl[tti][cc_id][ant_id].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;

            if(pSsbTxBuf[ant_id][tti])
                pDevCtx->vMuInfo.ssbInfo.sFHSsbTxBbuIoBufCtrl[tti][cc_id][ant_id].sBufferList = *pSsbTxBuf[ant_id][tti];
            else{
                print_err("1 NULL ptr to buff passed by L1");
                return XRAN_STATUS_FAIL;
            }

            /* C-Plane */
            pDevCtx->vMuInfo.ssbInfo.sFhSsbTxPrbMapBbuIoBufCtrl[tti][cc_id][ant_id].bValid = 0;
            pDevCtx->vMuInfo.ssbInfo.sFhSsbTxPrbMapBbuIoBufCtrl[tti][cc_id][ant_id].nSegGenerated = -1;
            pDevCtx->vMuInfo.ssbInfo.sFhSsbTxPrbMapBbuIoBufCtrl[tti][cc_id][ant_id].nSegToBeGen = -1;
            pDevCtx->vMuInfo.ssbInfo.sFhSsbTxPrbMapBbuIoBufCtrl[tti][cc_id][ant_id].nSegTransferred = 0;
            pDevCtx->vMuInfo.ssbInfo.sFhSsbTxPrbMapBbuIoBufCtrl[tti][cc_id][ant_id].sBufferList.nNumBuffers = 1; //TBD

            if(pSsbTxPrbBuf[ant_id][tti])
                pDevCtx->vMuInfo.ssbInfo.sFhSsbTxPrbMapBbuIoBufCtrl[tti][cc_id][ant_id].sBufferList = *pSsbTxPrbBuf[ant_id][tti];
            else{
                print_err("2 NULL ptr to buff passed by L1");
                return XRAN_STATUS_FAIL;
            }
        }
    }

    return XRAN_STATUS_SUCCESS;
}
int32_t xran_lte_fronthault_config(void *pHandle,
                struct xran_buffer_list *pSrcBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pSrcCpBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pDstBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pDstCpBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                xran_transport_callback_fn pCallback, void *pCallbackTag)
{
    return(xran_5g_fronthault_config(pHandle, pSrcBuffer, pSrcCpBuffer, pDstBuffer, pDstCpBuffer, pCallback, pCallbackTag,0));
}

int32_t xran_lte_prach_req(void *pHandle,
            struct xran_buffer_list *pDstBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
            struct xran_buffer_list *pDstBufferDecomp[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
            xran_transport_callback_fn pCallback, void *pCallbackTag)
{
    return(xran_5g_prach_req(pHandle, pDstBuffer, pDstBufferDecomp, pCallback, pCallbackTag,0));
}

int32_t xran_lte_srs_req(void *pHandle,
            struct xran_buffer_list *pDstBuffer[XRAN_MAX_ANT_ARRAY_ELM_NR][XRAN_N_FE_BUF_LEN],
            struct xran_buffer_list *pDstCpBuffer[XRAN_MAX_ANT_ARRAY_ELM_NR][XRAN_N_FE_BUF_LEN],
            xran_transport_callback_fn pCallback, void *pCallbackTag)
{
    return(xran_5g_srs_req(pHandle, pDstBuffer, pDstCpBuffer, pCallback, pCallbackTag,0 /*mu = 0 for LTE*/));
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
xran_tx_sym_process_ring(struct rte_ring *r)
{
    assert(r);
    struct rte_mbuf *mbufs[16];
    int i;
    uint32_t remaining;
    uint64_t t1;
    const uint16_t dequeued = rte_ring_dequeue_burst(r, (void **)mbufs,
        RTE_DIM(mbufs), &remaining);


    if (!dequeued)
        return 0;

    t1 = MLogXRANTick();
    for (i = 0; i < dequeued; ++i) {
        struct tx_sym_desc * p_tx_desc =  (struct tx_sym_desc *)rte_pktmbuf_mtod(mbufs[i],  struct tx_sym_desc *);
        xran_process_tx_sym_oru(p_tx_desc->arg,p_tx_desc->mu);

        xran_tx_sym_gen_desc_free(p_tx_desc);
        if (XRAN_STOPPED == xran_if_current_state){
            MLogXRANTask(PID_PROCESS_TX_SYM, t1, MLogXRANTick());
            return -1;
        }
    }

    if(xran_get_syscfg_iosleep())
        nanosleep(&xran_timingsource_get_ctx()->sleeptime, NULL);

    MLogXRANTask(PID_PROCESS_TX_SYM, t1, MLogXRANTick());
    MLogSetTaskCoreMap(TASK_3416);

    return remaining;
}


int32_t
xran_pkt_gen_process_ring(struct rte_ring *r)
{
    assert(r);
    struct rte_mbuf *mbufs[16];
    int i;
    uint32_t remaining;
    uint64_t t1;

    const uint16_t dequeued = rte_ring_dequeue_burst(r, (void **)mbufs, RTE_DIM(mbufs), &remaining);
    if (!dequeued)
        return 0;

    t1 = MLogXRANTick();
    for (i = 0; i < dequeued; ++i) {
        struct cp_up_tx_desc * p_tx_desc =  (struct cp_up_tx_desc *)rte_pktmbuf_mtod(mbufs[i],  struct cp_up_tx_desc *);
        if(xran_get_syscfg_appmode() == O_DU)
        {
            xran_process_tx_sym_cp_on_opt(p_tx_desc->pHandle,
                                            p_tx_desc->ctx_id,
                                            p_tx_desc->tti,
                                            p_tx_desc->start_cc,
                                            p_tx_desc->cc_num,
                                            p_tx_desc->start_ant,
                                            p_tx_desc->ant_num,
                                            p_tx_desc->frame_id,
                                            p_tx_desc->subframe_id,
                                            p_tx_desc->slot_id,
                                            p_tx_desc->sym_id,
                                            (enum xran_comp_hdr_type)p_tx_desc->compType,
                                            (enum xran_pkt_dir) p_tx_desc->direction,
                                            p_tx_desc->xran_port_id,
                                            (PSECTION_DB_TYPE)p_tx_desc->p_sec_db,
                                            p_tx_desc->mu,
                                            p_tx_desc->tti_for_ring,
                                            p_tx_desc->sym_id_for_ring,
                                            0);

        }
        else if(xran_get_syscfg_appmode() == O_RU)
        {
            xran_process_tx_sym_cp_off(p_tx_desc->pHandle,
                                       p_tx_desc->ctx_id,
                                       p_tx_desc->tti,
                                       p_tx_desc->start_cc,
                                       p_tx_desc->start_ant,
                                       p_tx_desc->frame_id,
                                       p_tx_desc->subframe_id,
                                       p_tx_desc->slot_id,
                                       p_tx_desc->sym_id,
                                       0,
                                       p_tx_desc->mu,
                                       p_tx_desc->tti_for_ring,
                                       p_tx_desc->sym_id_for_ring);
        }

        xran_pkt_gen_desc_free(p_tx_desc);
        if (XRAN_STOPPED == xran_if_current_state){
            MLogXRANTask(PID_PROCESS_TX_SYM, t1, MLogXRANTick());
            return -1;
        }
    }

    if(xran_get_syscfg_iosleep())
        nanosleep(&xran_timingsource_get_ctx()->sleeptime, NULL);

    MLogXRANTask(PID_PROCESS_TX_SYM, t1, MLogXRANTick());

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

int32_t
xran_dequeue_timing_info_for_tx(void* args)
{
    struct xran_ethdi_ctx *const ctx = xran_ethdi_get_ctx();
    uint16_t xran_port_mask = (uint16_t)((uint64_t)args & 0xFFFF);
    uint16_t current_port;
    uint32_t remaining;
    int i;
    struct rte_mbuf *mbufs[16];

    rte_timer_manage();

    for (current_port = 0; current_port < XRAN_PORTS_NUM;  current_port++) {
        if (xran_port_mask & (1 << current_port))
        {
            assert(ctx->up_dl_pkt_gen_ring[current_port]);

            const uint16_t dequeued = rte_ring_dequeue_burst(ctx->up_dl_pkt_gen_ring[current_port],
                                                             (void **)mbufs, RTE_DIM(mbufs), &remaining);
            if (!dequeued)
                continue;

            //printf("dequeued=%u, remaining=%u\n", dequeued, remaining);
            for (i = 0; i < dequeued; ++i)
            {
                struct cp_up_tx_desc *p_tx_desc = (struct cp_up_tx_desc *)rte_pktmbuf_mtod(mbufs[i], struct cp_up_tx_desc *);

                xran_process_tx_sym_cp_on_ring(
                    p_tx_desc->pHandle,
                    p_tx_desc->ctx_id,
                    p_tx_desc->tti,
                    p_tx_desc->start_cc,
                    p_tx_desc->cc_num,
                    p_tx_desc->start_ant,
                    p_tx_desc->ant_num,
                    p_tx_desc->frame_id,
                    p_tx_desc->subframe_id,
                    p_tx_desc->slot_id,
                    p_tx_desc->sym_id,
                    (enum xran_comp_hdr_type)p_tx_desc->compType,
                    (enum xran_pkt_dir)p_tx_desc->direction,
                    p_tx_desc->xran_port_id,
                    (PSECTION_DB_TYPE)p_tx_desc->p_sec_db,
                    p_tx_desc->mu,
                    p_tx_desc->tti_for_ring,
                    p_tx_desc->sym_id_for_ring,
                    0);

                xran_pkt_gen_desc_free(p_tx_desc);
                if (XRAN_STOPPED == xran_if_current_state)
                {
                    return -1;
                }
            }

            if(xran_get_syscfg_iosleep())
                nanosleep(&xran_timingsource_get_ctx()->sleeptime, NULL);
        }
    }

    if (XRAN_STOPPED == xran_if_current_state)
        return -1;

    return 0;
}


int32_t
xran_pkt_gen_process_via_ring(struct rte_ring *r)
{
    assert(r);
    struct rte_mbuf *mbufs[16];
    int i;
    uint32_t remaining;
    uint64_t t1;
    const uint16_t dequeued = rte_ring_dequeue_burst(r, (void **)mbufs,
        RTE_DIM(mbufs), &remaining);


    if (!dequeued)
        return 0;

    t1 = MLogXRANTick();
    for (i = 0; i < dequeued; ++i) {
        struct cp_up_tx_desc * p_tx_desc =  (struct cp_up_tx_desc *)rte_pktmbuf_mtod(mbufs[i],  struct cp_up_tx_desc *);
        xran_process_tx_sym_cp_on_ring(p_tx_desc->pHandle,
                                        p_tx_desc->ctx_id,
                                        p_tx_desc->tti,
                                        p_tx_desc->start_cc,
                                        p_tx_desc->cc_num,
                                        p_tx_desc->start_ant,
                                        p_tx_desc->ant_num,
                                        p_tx_desc->frame_id,
                                        p_tx_desc->subframe_id,
                                        p_tx_desc->slot_id,
                                        p_tx_desc->sym_id,
                                        (enum xran_comp_hdr_type)p_tx_desc->compType,
                                        (enum xran_pkt_dir) p_tx_desc->direction,
                                        p_tx_desc->xran_port_id,
                                        (PSECTION_DB_TYPE)p_tx_desc->p_sec_db,
                                        p_tx_desc->mu,
                                        p_tx_desc->tti_for_ring,
                                        p_tx_desc->sym_id_for_ring,
                                        0);

        xran_pkt_gen_desc_free(p_tx_desc);
        rte_timer_manage();
        if (XRAN_STOPPED == xran_if_current_state){
            MLogXRANTask(PID_PROCESS_TX_SYM, t1, MLogXRANTick());
            return -1;
        }
    }

    if(xran_get_syscfg_iosleep())
        nanosleep(&xran_timingsource_get_ctx()->sleeptime, NULL);

    MLogXRANTask(PID_PROCESS_TX_SYM, t1, MLogXRANTick());
    MLogSetTaskCoreMap(TASK_3416);

    return remaining;
}

int32_t
xran_tx_symb_ring_processing_func(void* args)
{
    struct xran_ethdi_ctx *const ctx = xran_ethdi_get_ctx();
    uint16_t xran_port_mask = (uint16_t)((uint64_t)args & 0xFFFF);
    uint16_t current_port;


    rte_timer_manage();

    for (current_port = 0; current_port < XRAN_PORTS_NUM;  current_port++) {
        if( xran_port_mask & (1<<current_port)) {
            xran_tx_sym_process_ring(ctx->up_dl_pkt_gen_ring[current_port]);
        }
    }

    if (XRAN_STOPPED == xran_if_current_state)
        return -1;

    return 0;
}


int32_t xran_transmit_up_pkts(void *args)
{
    unsigned long t1 = MLogXRANTick();
    uint64_t port_id, starPort_id, endPort_id;
    struct xran_device_ctx *p_dev_ctx;
    uint8_t mu ;
    uint8_t tti=0;
    uint8_t sym_id = 0;
    uint8_t mu_idx;
    uint32_t otaSymIdx, otaTti, otaSym;

    starPort_id = (uint64_t)args;
    endPort_id  = starPort_id+1;
    if(likely(xran_if_current_state == XRAN_RUNNING))
    {
        for(port_id = starPort_id ; port_id < endPort_id ; ++port_id)
        {
            p_dev_ctx = xran_dev_get_ctx_by_id(port_id);
            if(p_dev_ctx)
            {
                for(mu_idx=0; mu_idx < p_dev_ctx->fh_cfg.numMUs; mu_idx++)
                {
                    mu = p_dev_ctx->fh_cfg.mu_number[mu_idx];

                    otaSymIdx = xran_lib_ota_sym_idx_mu[mu];
                    /*Check if it is intra symbol boundary of curr symbol and transmit the next sym packet (advance Tx)*/
                    if(unlikely(p_dev_ctx->perMu[mu].adv_tx_factor != 0)){
                        if(p_dev_ctx->perMu[mu].adv_tx_factor <= xran_intra_sym_div[mu])
                            otaSymIdx += 1;
                        else
                            return 0;
                    }

                    otaTti = XranGetTtiNum(otaSymIdx, XRAN_NUM_OF_SYMBOL_PER_SLOT) % XRAN_N_FE_BUF_LEN;
                    otaSym = XranGetSymNum(otaSymIdx, XRAN_NUM_OF_SYMBOL_PER_SLOT);

                    tti = otaTti;
                    sym_id = otaSym;

                    xran_process_tx_sym_cp_on_ring_opt(p_dev_ctx, 0, p_dev_ctx->fh_cfg.nCC, 0, p_dev_ctx->fh_cfg.neAxc, XRAN_DIR_DL, mu, tti,sym_id);
                }
            }
        }
    }

    MLogXRANTask(PID_RADIO_ETH_TX_BURST, t1, MLogXRANTick());

    return 0;
}

int32_t
xran_dl_pkt_via_ring_processing_func(void* args)
{
    struct xran_ethdi_ctx *const ctx = xran_ethdi_get_ctx();
    uint16_t xran_port_mask = (uint16_t)((uint64_t)args & 0xFFFF);
    uint16_t current_port;
    int32_t i;
    queueid_t qi;

    rte_timer_manage();

    if (XRAN_RUNNING == xran_if_current_state)
    {
        for (current_port = 0; current_port < XRAN_PORTS_NUM;  current_port++)
        {
            if( xran_port_mask & (1<<current_port))
            {
                if(ctx->up_dl_pkt_gen_ring[current_port])
                    xran_pkt_gen_process_via_ring(ctx->up_dl_pkt_gen_ring[current_port]);
            }
        }

        for (current_port = 0; current_port < XRAN_PORTS_NUM;  current_port++)
        {
            if( xran_port_mask & (1<<current_port))
            {
                rte_timer_manage();
                process_dpdk_io_port_id(current_port*2, 2);
            }
        }

        for (current_port = 0; current_port < XRAN_PORTS_NUM;  current_port++)
        {
            if(xran_port_mask & (1<<current_port))
            {
                for (i = 0; i < ctx->io_cfg.num_vfs && i < XRAN_VF_MAX; i = i+1) {
                    if (ctx->vf2xran_port[i] == current_port) {
                        for(qi = 0; qi < ctx->rxq_per_port[current_port]; qi++){
                            rte_timer_manage();
                            if (process_ring(ctx->rx_ring[i][qi], i, qi))
                                return 0;
                        }
                    }
                }
            }
        }
    }
    if (XRAN_STOPPED == xran_if_current_state)
        return -1;

    return 0;
}

int32_t xran_fh_rx_and_up_tx_processing(void *port_mask)
{
    int32_t ret_val=0;

    ret_val = ring_processing_func((void *)0);
    if(ret_val != 0)
       return ret_val;

    ret_val = xran_dl_pkt_ring_processing_func(port_mask);
    if(ret_val != 0)
       return ret_val;

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


/** Function to pefromrm TX and RX on ETH device */
int32_t
xran_eth_trx_tasks_ports(void* arg)
{
    //process_dpdk_io(arg);
    rte_timer_manage();
    process_dpdk_io_port_id(0, 2);
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
    int32_t i;
    uint16_t portId = (uint16_t)((uint64_t)args & 0xFFFF);
    queueid_t qi;

    for (i = 0; i < ctx->io_cfg.num_vfs && i < XRAN_VF_MAX; i = i+1) {
        if (ctx->vf2xran_port[i] == portId) {
            for(qi = 0; qi < ctx->rxq_per_port[portId]; qi++){
                if (process_ring(ctx->rx_ring[i][qi], i, qi))
                    return 0;
            }
        }
    }

    if (XRAN_STOPPED == xran_if_current_state)
        return -1;

    return 0;
}

xran_status_t xran_update_worker_info(struct xran_worker_info_s *p_worker_info,
                                      struct xran_ethdi_ctx *eth_ctx,
                                      int32_t *core_map, uint32_t xran_ports,
                                      int32_t (*arr_job2wrk_id)[XRAN_JOB_TYPE_MAX],
                                      uint8_t total_num_cores)
{
    int i;
    struct xran_worker_info_s *temp_info;
    struct xran_worker_th_ctx *pThCtx;
    struct xran_device_ctx *p_dev_ctx;

    /* Update timing core func */
    // p_worker_info[0] will always contain info for timing and it must be present always
    eth_ctx->time_wrk_cfg.f     = p_worker_info[0].TaskFunc;
    eth_ctx->time_wrk_cfg.arg   = p_worker_info[0].TaskArg;
    eth_ctx->time_wrk_cfg.state = p_worker_info[0].State;

    for(i=1 ; i < total_num_cores ; ++i)
    {
        uint32_t workerid;
        temp_info = &p_worker_info[i];
        workerid = temp_info->WorkerId;

        if(workerid>= XRAN_MAX_FH_CORES)
            continue;

        if(temp_info->TaskFunc == NULL)
        {
            eth_ctx->pkt_wrk_cfg[workerid].f     = NULL;
            eth_ctx->pkt_wrk_cfg[workerid].arg   = NULL;
            continue;
        }

        pThCtx = (struct xran_worker_th_ctx*) _mm_malloc(sizeof(struct xran_worker_th_ctx), 64);
        if(pThCtx == NULL)
        {
            print_err("pThCtx allocation error\n");
            return XRAN_STATUS_FAIL;
        }

        //printf("\nUpdating worker info: Worker ID: %d, \n",i);

        memset(pThCtx, 0, sizeof(struct xran_worker_th_ctx));
        pThCtx->worker_id      = workerid;
        pThCtx->worker_core_id = core_map[pThCtx->worker_id];
        snprintf(pThCtx->worker_name, RTE_DIM(pThCtx->worker_name), "%s-%d", temp_info->ThreadName, core_map[pThCtx->worker_id]);
        pThCtx->task_func = temp_info->TaskFunc;
        pThCtx->task_arg  = temp_info->TaskArg;
        eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].f     = xran_generic_worker_thread;
        eth_ctx->pkt_wrk_cfg[pThCtx->worker_id].arg   = pThCtx;
    }

    for(i = 0; i < xran_ports; i++)
    {
        p_dev_ctx =  xran_dev_get_ctx_by_id(i);
        if(p_dev_ctx == NULL)
        {
            print_err("p_dev_update\n");
            return XRAN_STATUS_FAIL;
        }
        p_dev_ctx->job2wrk_id[XRAN_JOB_TYPE_OTA_CB] = arr_job2wrk_id[i][XRAN_JOB_TYPE_OTA_CB];
        p_dev_ctx->job2wrk_id[XRAN_JOB_TYPE_CP_DL]  = arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_DL];
        p_dev_ctx->job2wrk_id[XRAN_JOB_TYPE_CP_UL]  = arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_UL];
        p_dev_ctx->job2wrk_id[XRAN_JOB_TYPE_SYM_CB] = arr_job2wrk_id[i][XRAN_JOB_TYPE_SYM_CB];
        printf("RU%d XRAN_JOB_TYPE_OTA_CB worker id %d\n", i,  p_dev_ctx->job2wrk_id[XRAN_JOB_TYPE_OTA_CB]);
        printf("RU%d XRAN_JOB_TYPE_CP_UL  worker id %d\n", i,  p_dev_ctx->job2wrk_id[XRAN_JOB_TYPE_CP_UL]);
        printf("RU%d XRAN_JOB_TYPE_CP_DL  worker id %d\n", i,  p_dev_ctx->job2wrk_id[XRAN_JOB_TYPE_CP_DL]);
        printf("RU%d XRAN_JOB_TYPE_SYM_CB worker id %d\n", i,  p_dev_ctx->job2wrk_id[XRAN_JOB_TYPE_SYM_CB]);
    }

    return XRAN_STATUS_SUCCESS;
}

#define XRAN_SET_WORKER_INFO(p_worker_info, Worker_Id, Thread_name, Task_func, Task_arg, state) \
                    p_worker_info = (struct xran_worker_info_s){.WorkerId = Worker_Id, .ThreadName = Thread_name, .TaskFunc = Task_func, .TaskArg = Task_arg, .State = state};

/** Fucntion generate configuration of worker threads and creates them base on sceanrio and used platform */
int32_t xran_spawn_workers(void)
{
    uint64_t nWorkerCore = 1LL;
    uint32_t coreNum     = sysconf(_SC_NPROCESSORS_CONF);
    int32_t  i = 0;
    int32_t numRUs;
    uint32_t total_num_cores  = 1; /*start with timing core */
    uint32_t worker_num_cores = 0;
    uint32_t icx_cpu = 0;
    int32_t core_map[2*sizeof(uint64_t)*8];
    uint64_t xran_port_mask = 0;
    enum xran_category ruCat_port0;
    bool is_mixed_cat;

    struct xran_ethdi_ctx  *eth_ctx   = xran_ethdi_get_ctx();
    struct xran_device_ctx *p_dev     = NULL;
    struct xran_fh_config  *fh_cfg    = NULL;
    void *worker_ports=NULL;

    struct xran_worker_info_s p_worker_info[XRAN_MAX_FH_CORES] = {0};

    int32_t arr_job2wrk_id[XRAN_PORTS_NUM][XRAN_JOB_TYPE_MAX];

    p_dev =  xran_dev_get_ctx_by_id(0);
    if(p_dev == NULL)
    {
        print_err("p_dev\n");
        return XRAN_STATUS_FAIL;
    }

    fh_cfg = &p_dev->fh_cfg;
    if(fh_cfg == NULL)
    {
        print_err("fh_cfg\n");
        return XRAN_STATUS_FAIL;
    }

    for(i = 0; i < coreNum && i < 64; i++)
    {
        if(nWorkerCore & (uint64_t)eth_ctx->io_cfg.pkt_proc_core)
        {
            core_map[worker_num_cores++] = i;
            total_num_cores++;
        }
        nWorkerCore = nWorkerCore << 1;
    }

    nWorkerCore = 1LL;
    for(i = 64; i < coreNum && i < 128; i++)
    {
        if (nWorkerCore & (uint64_t)eth_ctx->io_cfg.pkt_proc_core_64_127)
        {
            core_map[worker_num_cores++] = i;
            total_num_cores++;
        }
        nWorkerCore = nWorkerCore << 1;
    }
    eth_ctx->num_workers = 0;

    if(total_num_cores > XRAN_MAX_FH_CORES)
    {
        print_err("Unsupported configuration - exceeds the maximum number of available FH cores");
        return XRAN_STATUS_FAIL;
    }

    numRUs      = xran_get_syscfg_totalnumru();
    ruCat_port0 = fh_cfg->ru_conf.xranCat;
    is_mixed_cat= 0;

    /* Check if this is a mixed category case */
    for(i=1; (i < numRUs) && (numRUs > 1); ++i)
    {
        struct xran_device_ctx* dev_ctx_portn = xran_dev_get_ctx_by_id(i);
        if(dev_ctx_portn == NULL)
        {
            print_err("dev_ctx_portn\n");
            return XRAN_STATUS_FAIL;
        }

        if(ruCat_port0 != dev_ctx_portn->fh_cfg.ru_conf.xranCat)
        {
            is_mixed_cat = 1;
            break;
        }
    }

    icx_cpu = _may_i_use_cpu_feature(_FEATURE_AVX512IFMA52);

    printf("ORAN operating mode: O-%s\n", (xran_get_syscfg_appmode() == ID_O_DU) ?"DU":"RU");
    printf("  Num of configured cores: %d%s\n", total_num_cores, icx_cpu?" (Xeon SP Gen3 or later)":"");
    printf("  Num of configured RUs  : %d\n", numRUs);
    for(i=0; i < numRUs; ++i)
    {
        struct xran_device_ctx *tmpDevCtx;

        tmpDevCtx = xran_dev_get_ctx_by_id(i);
        if(tmpDevCtx != NULL)
        {
            printf("    O-RU[%d]: Cat-%c CC[%2d] eAxC[%2d]\n", i,
                (tmpDevCtx->fh_cfg.ru_conf.xranCat == XRAN_CATEGORY_A) ? 'A' : 'B',
                tmpDevCtx->fh_cfg.nCC, tmpDevCtx->fh_cfg.neAxc);
        }
        else
        {
            print_err("O-RU[%d]: Failed to get information!\n", i);
        }
    }

    for(i = 0; i < numRUs; i++)
        xran_port_mask |= 1L<<i;

    for(i = 0; i < numRUs; i++)
    {
        arr_job2wrk_id[i][XRAN_JOB_TYPE_OTA_CB] = 0;
        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_DL]  = 1;
        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_UL]  = 1;
        arr_job2wrk_id[i][XRAN_JOB_TYPE_SYM_CB] = 0;
    }

    /* For worker task allocation treat mixed cat case as cat-B */
    if(fh_cfg->ru_conf.xranCat == XRAN_CATEGORY_A && is_mixed_cat == 0)
    {
        switch(total_num_cores)
        {
            case 1: /** only timing core */
                XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", xran_all_tasks, NULL, 1);
                break;
            case 2:
                if(xran_get_syscfg_bbuoffload())
                    p_dev->tx_sym_gen_func = xran_process_tx_sym_cp_on_ring;
                else
                    p_dev->tx_sym_gen_func = xran_process_tx_sym_cp_on_opt;

                XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", xran_eth_trx_tasks, NULL, 1);
                XRAN_SET_WORKER_INFO(p_worker_info[1],  0, "fh_rx_bbdev", ring_processing_func, NULL, 1);
                break;
            case 3:
                /* timing core */
                XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", xran_eth_trx_tasks, NULL, 1);
                /* workers */
                /** 0 **/
                XRAN_SET_WORKER_INFO(p_worker_info[1],  0, "fh_rx_bbdev", ring_processing_func, NULL, 1);

                for (i = 0; i < numRUs; i++)
                {
                    arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_DL]=0;
                    arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_UL]=0;
                }
                /** 1 - CP GEN **/
                XRAN_SET_WORKER_INFO(p_worker_info[2], 1, "fh_cp_gen", xran_dl_pkt_ring_processing_func, (void*)xran_port_mask, 1);
                break;
            default:
                print_err("unsupported configuration Cat %d numports %d total_num_cores = %d\n", fh_cfg->ru_conf.xranCat, numRUs, total_num_cores);
                return XRAN_STATUS_FAIL;
        }
    }
    else if(((fh_cfg->ru_conf.xranCat == XRAN_CATEGORY_B || is_mixed_cat ) && numRUs == 1) || xran_get_syscfg_bbuoffload())
    {
        switch(total_num_cores)
        {
            case 1: /** only timing core */
                print_err("unsupported configuration Cat %d numports %d total_num_cores = %d\n", fh_cfg->ru_conf.xranCat, numRUs, total_num_cores);
                return XRAN_STATUS_FAIL;
                break;
            case 2:
                XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", xran_eth_trx_tasks, NULL, 1);

                if(xran_get_syscfg_bbuoffload())
                    p_dev->tx_sym_gen_func = xran_process_tx_sym_cp_on_ring;
                else
                    p_dev->tx_sym_gen_func = xran_process_tx_sym_cp_on_opt;

                XRAN_SET_WORKER_INFO(p_worker_info[1],  0, "fh_rx_bbdev", ring_processing_func, NULL, 1);
                break;
            case 3:
                /* timing core */
                XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", xran_eth_trx_tasks, NULL, 1);

                if(xran_get_syscfg_bbuoffload())
                {
                    eth_ctx->time_wrk_cfg.f = xran_eth_rx_tasks;
                    /* Assign 1 worker for Tx and Another for Rx */
                    uint8_t port_id=0;
                    struct xran_device_ctx *p_devCtx;
                    for(port_id = 0 ; port_id < XRAN_PORTS_NUM; ++port_id)
                    {
                        p_devCtx = xran_dev_get_ctx_by_id(port_id);
                        if(p_devCtx)
                        {
#ifndef POLL_EBBU_OFFLOAD
                            p_devCtx->tx_sym_gen_func = do_nothing;
#else
                            p_devCtx->tx_sym_gen_func = xran_process_tx_sym_cp_on_ring;
#endif
                            // p_devCtx->tx_sym_gen_func = xran_enqueue_timing_info_for_tx;
                        }
                        else
                            printf("\nWarning: p_devCtx is NULL for port_id %hhu\n",port_id);
                    }
                    XRAN_SET_WORKER_INFO(p_worker_info[1],  0, "fhRx", ring_processing_func, NULL, 1);
                    XRAN_SET_WORKER_INFO(p_worker_info[2], 1, "fhTx", xran_transmit_up_pkts, NULL, 1);
                }
                else
                {
                    /* workers */
                    /** 0 **/
                    XRAN_SET_WORKER_INFO(p_worker_info[1],0, "fh_rx_bbdev", ring_processing_func, NULL, 1);

                    for (i = 0; i < numRUs; i++)
                    {
                        struct xran_device_ctx *p_dev_update = xran_dev_get_ctx_by_id(i);
                        if (p_dev_update == NULL)
                        {
                            print_err("p_dev_update\n");
                            return XRAN_STATUS_FAIL;
                        }
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_DL]=0;
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_UL]=0;
                        if(xran_get_syscfg_bbuoffload() && i >= 1)
                        {
                            p_dev_update->tx_sym_gen_func = xran_process_tx_sym_cp_on_dispatch_opt;
                            printf("p [%d] xran_process_tx_sym_cp_on_dispatch_opt\n", i);
                        }
                    }
                    /** 1 - CP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[2], 1, "fh_cp_gen", \
                                (xran_get_syscfg_bbuoffload()? xran_dl_pkt_via_ring_processing_func:xran_dl_pkt_ring_processing_func),   \
                                (void*)(((1<<0) | (1<<1) | (1<<2)) & xran_port_mask), 1);
                }
                break;
            case 4:
                /* timing core */
                if(numRUs == 1)
                {
                    XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", xran_eth_trx_tasks, NULL, 1);
                }
                else
                {
                    XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", xran_eth_trx_tasks_ports, NULL, 1);
                }
                if(xran_get_syscfg_bbuoffload())
                {
                    uint8_t port_id=0;
                    struct xran_device_ctx *p_devCtx;

                    for(port_id = 0 ; port_id < XRAN_PORTS_NUM; ++port_id)
                    {
                        p_devCtx = xran_dev_get_ctx_by_id(port_id);
                        if(p_devCtx)
                        {
                            if(port_id == 0)
                            {
                                p_devCtx->tx_sym_gen_func =  xran_process_tx_sym_cp_on_ring;
                            }
                            else
                            {
                                p_devCtx->tx_sym_gen_func = xran_process_tx_sym_cp_on_dispatch_opt;
                                printf("p [%d] xran_process_tx_sym_cp_on_dispatch_opt\n", i);
                            }
                        }
                        else
                            printf("\nWarning: p_devCtx is NULL for port_id %hhu\n",port_id);
                    }

                    for (i = 0; i < numRUs; i++)
                    {
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_OTA_CB]=0;
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_UL]=0;
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_DL]=0;
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_SYM_CB]=0;
                    }

                    /** 0 **/
                    XRAN_SET_WORKER_INFO(p_worker_info[1], 0, "fh_rx_bbdev", ring_processing_func2, NULL, 1);
                    /** 1 - CP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[2], 1, "fh_cp_gen", xran_dl_pkt_via_ring_processing_func, (void*)((1L<<1)), 1);
                    /** 2 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[3],  2, "fh_tx_gen", xran_dl_pkt_via_ring_processing_func, (void*)((1L<<2)), 1);
                }   /* if(xran_get_syscfg_bbuoffload()) */
                else
                {
                    /** 0 **/
                    XRAN_SET_WORKER_INFO(p_worker_info[1], 0, "fh_rx_bbdev", ring_processing_func, NULL, 1);
                    /** 1 - CP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[2], 1, "fh_cp_gen", xran_dl_pkt_ring_processing_func, (void*)(((1L<<1) | (1L<<2) |(1L<<0)) & xran_port_mask), 1);
                    /** 2 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[3], 2, "fh_tx_gen", xran_dl_pkt_ring_processing_func, (void*)((1L<<0) & xran_port_mask), 1);

                    for (i = 1; i < numRUs; i++)
                    {
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_DL]=2;
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_UL]=2;
                    }
                }
                break;
            case 5:
                /* timing core */
                XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", xran_eth_trx_tasks, NULL, 1);
                /* workers */
                /** 0 **/
                XRAN_SET_WORKER_INFO(p_worker_info[1], 0, "fh_rx_bbdev", ring_processing_func, NULL, 1);
                if(xran_get_syscfg_bbuoffload())
                {
                    uint8_t port_id=0;
                    struct xran_device_ctx *p_devCtx;

                    for(port_id = 0 ; port_id < XRAN_PORTS_NUM; ++port_id)
                    {
                        p_devCtx = xran_dev_get_ctx_by_id(port_id);
                        if(p_devCtx)
                        {
                            p_devCtx->tx_sym_gen_func = do_nothing;
                            // p_devCtx->tx_sym_gen_func = xran_enqueue_timing_info_for_tx;
                        }
                        else
                            printf("\nWarning: p_devCtx is NULL for port_id %hhu\n",port_id);
                    }

                    XRAN_SET_WORKER_INFO(p_worker_info[2],  1, "fh_tx_p0", xran_transmit_up_pkts, ((void*)0), 1);
                    XRAN_SET_WORKER_INFO(p_worker_info[3],  2, "fh_tx_p1", xran_transmit_up_pkts, ((void*)1), 1);
                    XRAN_SET_WORKER_INFO(p_worker_info[4], 3, "fh_tx_p2", xran_transmit_up_pkts, ((void*)2), 1);
                }
                else
                {
                    /** 1 - CP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[2], 1, "fh_cp_gen", xran_dl_pkt_ring_processing_func, (void*)(((1L<<1) | (1L<<2) |(1L<<0)) & xran_port_mask), 1);
                    /** 2 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[3], 2, "fh_tx_gen", xran_dl_pkt_ring_processing_func, (void*)((1L<<0) & xran_port_mask), 1);
                    /** 3 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[4], 3, "fh_tx_gen", xran_dl_pkt_ring_processing_func, (void*)((1L<<0) & xran_port_mask), 1);
                    /* TODO: Would it be OK to process same port from different core? */

                    for (i = 1; i < numRUs; i++)
                    {
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_DL]=3;
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_UL]=3;
                    }
                }
                break;
            case 6:
                if(xran_get_syscfg_appmode() == O_DU)
                {
                    /* timing core */
                    XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", xran_eth_rx_tasks, NULL, 1);
                    /* workers */
                    /** 0 **/
                    XRAN_SET_WORKER_INFO(p_worker_info[1], 0, "fh_rx_bbdev", ring_processing_func, NULL, 1);
                    /** 1 Eth Tx **/
                    XRAN_SET_WORKER_INFO(p_worker_info[2], 1, "fh_eth_tx", process_dpdk_io_tx, (void *)2, 1);
                    /** 2 - CP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[3], 2, "fh_cp_gen", xran_dl_pkt_ring_processing_func,   \
                                                    (void*)(((1L<<1) | (1L<<2) |(1L<<0)) & xran_port_mask), 1);
                    /** 3 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[4], 3, "fh_tx_gen", xran_dl_pkt_ring_processing_func,   \
                                                    (void*)((1L<<0) & xran_port_mask), 1);
                    /** 4 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[5], 4, "fh_tx_gen", xran_dl_pkt_ring_processing_func,   \
                                                    (void*)((1L<<0) & xran_port_mask), 1);

                    for (i = 0; i < numRUs; i++)
                    {
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_DL]=4;
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_UL]=4;
                    }
                }

                else if(xran_get_syscfg_appmode() == O_RU)
                {
                    /*** O_RU specific config */
                    /* timing core */
                    XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", NULL, NULL, 1);
                    /* workers */
                    /** 0  Eth RX */
                    XRAN_SET_WORKER_INFO(p_worker_info[1], 0, "fh_eth_rx", process_dpdk_io_rx, NULL, 1);
                    /** 1  FH RX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[2], 1, "fh_rx_p0", ring_processing_func_per_port, (void *)0, 1);
                    /** 2  FH RX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[3], 2, "fh_rx_p1", ring_processing_func_per_port, (void *)1, 1);
                    /** 3  FH RX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[4], 3, "fh_rx_p2", ring_processing_func_per_port, (void *)2, 1);
                    /**  FH TX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[5], 4, "fh_eth_tx", process_dpdk_io_tx, (void *)2, 1);
                }

                else
                {
                    print_err("unsupported configuration Cat %d numports %d total_num_cores = %d\n", fh_cfg->ru_conf.xranCat, numRUs, total_num_cores);
                    return XRAN_STATUS_FAIL;
                }
                break;
            default:
                print_err("unsupported configuration\n");
                return XRAN_STATUS_FAIL;
        }
    }
    else if((fh_cfg->ru_conf.xranCat == XRAN_CATEGORY_B || is_mixed_cat ) && numRUs > 1)
    {
        switch(total_num_cores)
        {
            case 1:
                print_err("unsupported configuration Cat %d numports %d total_num_cores = %d\n", fh_cfg->ru_conf.xranCat, numRUs, total_num_cores);
                return XRAN_STATUS_FAIL;
                break;

            case 2:
                if(numRUs == 2)
                    worker_ports = (void *)((1L<<0 | 1L<<1) & xran_port_mask);
                else if(numRUs == 3)
                    worker_ports = (void *)((1L<<0 | 1L<<1 | 1L<<2) & xran_port_mask);
                else if(numRUs == 4)
                    worker_ports = (void *)((1L<<0 | 1L<<1 | 1L<<2 | 1L<<3) & xran_port_mask);
                else
                {
                    print_err("unsupported configuration Cat %d numports %d total_num_cores = %d\n", fh_cfg->ru_conf.xranCat, numRUs, total_num_cores);
                    return XRAN_STATUS_FAIL;
                }

                XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", xran_eth_trx_tasks, NULL, 1);
                XRAN_SET_WORKER_INFO(p_worker_info[1], 0, "fh_rx_bbdev", xran_fh_rx_and_up_tx_processing, worker_ports, 1);

                for (i = 1; i < numRUs; i++)
                    {
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_DL]=0;
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_UL]=0;
                    }
                break;
            case 3:
                if(icx_cpu)
                {
                    /* timing core */
                    XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", xran_eth_trx_tasks, NULL, 1);
                    /* workers */
                    /** 0 **/
                    XRAN_SET_WORKER_INFO(p_worker_info[1], 0, "fh_rx_bbdev", ring_processing_func, NULL, 1);
                    for (i = 1; i < numRUs; i++)
                    {
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_DL]=0;
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_UL]=0;
                    }

                    /** 1 - CP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[2], 1, "fh_cp_gen", xran_dl_pkt_ring_processing_func, (void *)xran_port_mask, 1);
                }

                else /* csx cpu */
                {
                    if(numRUs == 3)
                        worker_ports = (void *)(1L<<2 & xran_port_mask);
                    else if(numRUs == 4)
                        worker_ports = (void *)((1L<<2 | 1L<<3) & xran_port_mask);
                    else
                    {
                        print_err("unsupported configuration Cat %d numports %d total_num_cores = %d\n", fh_cfg->ru_conf.xranCat, numRUs, total_num_cores);
                        return XRAN_STATUS_FAIL;
                    }
                    /* timing core */
                    XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", xran_eth_trx_tasks, NULL, 1);
                    /* workers */
                    /** 0 **/
                    XRAN_SET_WORKER_INFO(p_worker_info[1], 0, "fh_rx_bbdev", xran_dl_pkt_ring_processing_func, (void *)((1L<<0|1L<<1) & xran_port_mask), 1);
                    for (i = 1; i < numRUs; i++)
                    {
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_DL]=0;
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_UL]=0;
                    }
                    /** 1 - CP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[2], 1, "fh_cp_gen", xran_fh_rx_and_up_tx_processing, worker_ports, 1);
                }
                break;

            case 4:
                /* timing core */
                XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", xran_eth_trx_tasks, NULL, 1);
                /* workers */
                /** 0 **/
                XRAN_SET_WORKER_INFO(p_worker_info[1], 0, "fh_rx_bbdev", ring_processing_func, NULL, 1);
                /** 1 - CP GEN **/
                XRAN_SET_WORKER_INFO(p_worker_info[2], 1, "fh_cp_gen", xran_dl_pkt_ring_processing_func, (void*)(((1L<<1) | (1L<<2)) & xran_port_mask), 1);
                /** 2 UP GEN **/
                XRAN_SET_WORKER_INFO(p_worker_info[3], 2, "fh_tx_gen", xran_dl_pkt_ring_processing_func, (void*)((1L<<0) & xran_port_mask), 1);

                for (i = 1; i < numRUs; i++)
                {
                    arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_DL]=2;
                    arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_UL]=2;
                }
                break;
            case 5:
                    /* timing core */
                    XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", xran_eth_trx_tasks, NULL, 1);
                    /* workers */
                    /** 0  FH RX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[1], 0, "fh_rx_bbdev", ring_processing_func, NULL, 1);
                    /** 1 - CP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[2], 1, "fh_cp_gen", xran_dl_pkt_ring_processing_func, (void*)((1<<0) & xran_port_mask), 1);
                    /** 2 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[3], 2, "fh_up_gen", xran_dl_pkt_ring_processing_func, (void*)((1<<1) & xran_port_mask), 1);
                    /** 3 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[4], 3, "fh_up_gen", xran_dl_pkt_ring_processing_func, (void*)((1<<2) & xran_port_mask), 1);

                    if(xran_get_syscfg_appmode()  == O_DU)
                    {
                        for(i = 1; i < numRUs; i++)
                        {
                            struct xran_device_ctx *p_dev_update =  xran_dev_get_ctx_by_id(i);
                            if(p_dev_update == NULL)
                            {
                                print_err("p_dev_update\n");
                                return XRAN_STATUS_FAIL;
                            }
                            if(p_dev_update->dlCpProcBurst == 0)
                            {
                                arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_DL]=i+1;
                            }
                        }
                    }

                break;
            case 6:
                if(xran_get_syscfg_appmode() == O_DU)
                {
                    /* timing core */
                    XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", xran_eth_trx_tasks, NULL, 1);
                    /* workers */
                    /** 0 **/
                    XRAN_SET_WORKER_INFO(p_worker_info[1], 0, "fh_rx_bbdev", ring_processing_func, NULL, 1);
                    /** 1 - CP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[2], 1, "fh_cp_gen", xran_processing_timer_only_func, NULL, 1);
                    /** 2 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[3], 2, "fh_tx_gen", xran_dl_pkt_ring_processing_func, (void*)((1<<0) & xran_port_mask), 1);
                    /** 3 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[4], 3, "fh_tx_gen", xran_dl_pkt_ring_processing_func, (void*)((1<<1) & xran_port_mask), 1);
                    /** 4 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[5], 4, "fh_tx_gen", xran_dl_pkt_ring_processing_func, (void*)((1<<2) & xran_port_mask), 1);
                }
                else
                {
                    /*** O_RU specific config */
                    /* timing core */
                    XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", NULL, NULL, 1);
                    /* workers */
                    /** 0  Eth RX */
                    XRAN_SET_WORKER_INFO(p_worker_info[1], 0, "fh_eth_rx", process_dpdk_io_rx, NULL, 1);
                    /** 1  FH RX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[2], 1, "fh_rx_p0", ring_processing_func_per_port, (void*)0, 1);
                    /** 2  FH RX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[3], 2, "fh_rx_p1", ring_processing_func_per_port, (void*)1, 1);
                    /** 3  FH RX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[4], 3, "fh_rx_p2", ring_processing_func_per_port, (void*)2, 1);
                    /** 4 FH TX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[5], 4, "fh_eth_tx", process_dpdk_io_tx, NULL, 1);
                }
                break;
            case 7:
                /*** O_RU specific config */
                if((numRUs == 4) && (xran_get_syscfg_appmode() == O_RU))
                {
                    /*** O_RU specific config */
                    XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", NULL, NULL, 1);
                    /* workers */
                    /** 0  Eth RX */
                    XRAN_SET_WORKER_INFO(p_worker_info[1], 0, "fh_eth_rx", process_dpdk_io_rx, NULL, 1);
                    /** 1  FH RX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[2], 1, "fh_rx_p0", ring_processing_func_per_port, (void*)0, 1);
                    /** 2  FH RX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[3], 2, "fh_rx_p1", ring_processing_func_per_port, (void*)1, 1);
                    /** 3  FH RX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[4], 3, "fh_rx_p2", ring_processing_func_per_port, (void*)2, 1);
                    /** 4  FH RX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[5], 4, "fh_rx_p3", ring_processing_func_per_port, (void*)3, 1);
                    /**  FH TX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[6], 5, "fh_eth_tx", process_dpdk_io_tx, NULL, 1);
                } /* -- if xran->ports == 4 -- */
                else if(xran_get_syscfg_appmode() == O_DU)
                {
                    if(numRUs == 3)
                        worker_ports = (void *)((1<<2) & xran_port_mask);
                    else if(numRUs == 4)
                        worker_ports = (void *)((1<<3) & xran_port_mask);
                    /* timing core */
                    XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", xran_eth_trx_tasks, NULL, 1);
                    /* workers */
                    /** 0 **/
                    XRAN_SET_WORKER_INFO(p_worker_info[1], 0, "fh_rx_bbdev", ring_processing_func, NULL, 1);

                    for (i = 2; i < numRUs; i++)
                    {
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_UL]=0;
                    }

                    /** 1 - CP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[2], 1, "fh_cp_gen", xran_processing_timer_only_func, NULL, 1);
                    /** 2 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[3], 2, "fh_tx_gen", xran_dl_pkt_ring_processing_func, (void*)((1<<0) & xran_port_mask), 1);

                    for (i = (numRUs-1); i < numRUs; i++)
                    {
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_DL]=2;
                    }
                    /** 3 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[4], 3, "fh_tx_gen", xran_dl_pkt_ring_processing_func, (void*)((1<<1) & xran_port_mask), 1);

                    for (i = (numRUs - 2); i < (numRUs - 1); i++)
                    {
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_DL]=3;
                    }

                    /** 4 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[5], 4, "fh_tx_gen", xran_dl_pkt_ring_processing_func, (void*)((1<<2) & xran_port_mask), 1);
                    /** 5 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[6], 5, "fh_tx_gen", xran_dl_pkt_ring_processing_func, worker_ports, 1);
                }
                else
                {
                    print_err("unsupported configuration Cat %d numports %d total_num_cores = %d\n", fh_cfg->ru_conf.xranCat, numRUs, total_num_cores);
                    return XRAN_STATUS_FAIL;
                }
                break;
            case 8:
                /*** O_RU specific config */
                if((numRUs >= 2) && (xran_get_syscfg_appmode() == O_RU))
                {
                    /*** O_RU specific config */
                    /* timing core */
                    XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", NULL, NULL, 1);
                    /* workers */
                    /** 0  Eth RX */
                    XRAN_SET_WORKER_INFO(p_worker_info[1], 0, "fh_eth_rx", process_dpdk_io_rx, NULL, 1);
                    /** 1  FH RX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[2], 1, "fh_rx_p0", ring_processing_func_per_port, (void *)0, 1);
                    /** 2  FH RX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[3], 2, "fh_rx_p1", ring_processing_func_per_port, (void *)1, 1);
                    /** 3  FH RX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[4], 3, "fh_rx_p2", ring_processing_func_per_port, (void *)2, 1);
                    /** 4  FH RX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[5], 4, "fh_rx_p3", ring_processing_func_per_port, (void *)3, 1);
                    /** 5 FH TX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[6], 5, "fh_eth_tx", process_dpdk_io_tx, NULL, 1);
                    /** 6 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[7], 6, "fh_up_gen", xran_tx_symb_ring_processing_func, (void*)(((1L<<1) | (1L<<2) |(1L<<0)) & xran_port_mask), 1);

                    for(i = 0; i < numRUs; i++)
                    {
                        struct xran_device_ctx * p_dev_update =  xran_dev_get_ctx_by_id(i);
                        if(p_dev_update == NULL)
                        {
                            print_err("p_dev_update\n");
                            return XRAN_STATUS_FAIL;
                        }
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_DL]=6;
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_UL]=6;
                        if (i >= 1)
                        {
                            //p_dev_update->tx_sym_gen_func = xran_process_tx_sym_cp_on_dispatch_opt;
                            p_dev_update->tx_sym_func = xran_tx_sym_cp_off;
                            printf("p [%d] xran_tx_sym_cp_off\n", i);
                        }
                    }

                } /* -- if xran->ports == 4 -- */
                else if(xran_get_syscfg_appmode() == O_DU)
                {
                    if(numRUs == 3)
                        worker_ports = (void *)((1<<2) & xran_port_mask);
                    else if(numRUs == 4)
                        worker_ports = (void *)((1<<3) & xran_port_mask);
                    /* timing core */
                    XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", xran_eth_trx_tasks, NULL, 1);
                    /* workers */
                    /** 0 **/
                    XRAN_SET_WORKER_INFO(p_worker_info[1], 0, "fh_rx_bbdev", ring_processing_func, NULL, 1);

                    for(i = 2; i < numRUs; i++)
                    {
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_UL]=0;
                    }
                    /** 1 - CP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[2], 1, "fh_cp_gen", xran_processing_timer_only_func, NULL, 1);
                    /** 2 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[3], 2, "fh_tx_gen", xran_dl_pkt_ring_processing_func, (void*)((1<<0) & xran_port_mask), 1);

                    for(i = (numRUs-1); i < numRUs; i++)
                    {
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_DL]=2;
                    }
                    /** 3 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[4], 3, "fh_tx_gen", xran_dl_pkt_ring_processing_func, (void*)((1<<1) & xran_port_mask), 1);

                    for(i = (numRUs - 2); i < (numRUs - 1); i++)
                    {
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_DL]=3;
                    }
                    /** 4 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[5], 4, "fh_tx_gen", xran_dl_pkt_ring_processing_func, (void*)((1<<2) & xran_port_mask), 1);
                    /** 5 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[6], 5, "fh_tx_gen", xran_dl_pkt_ring_processing_func, worker_ports, 1);
                }
                else
                {
                    print_err("unsupported configuration Cat %d numports %d total_num_cores = %d\n", fh_cfg->ru_conf.xranCat, numRUs, total_num_cores);
                    return XRAN_STATUS_FAIL;
                }
                break;
            case 9:
                /*** O_RU specific config */
                if((numRUs >= 2) && (xran_get_syscfg_appmode() == O_RU))
                {
                    /*** O_RU specific config */
                    /* timing core */
                    XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", NULL, NULL, 1);
                    /* workers */
                    /** 0  Eth RX */
                    XRAN_SET_WORKER_INFO(p_worker_info[1], 0, "fh_eth_rx", process_dpdk_io_rx, NULL, 1);
                    /** 1  FH RX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[2], 1, "fh_rx_p0", ring_processing_func_per_port, (void *)0, 1);
                    /** 2  FH RX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[3], 2, "fh_rx_p1", ring_processing_func_per_port, (void *)1, 1);
                    /** 3  FH RX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[4], 3, "fh_rx_p2", ring_processing_func_per_port, (void *)2, 1);
                    /** 4  FH RX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[5], 4, "fh_rx_p3", ring_processing_func_per_port, (void *)3, 1);
                    /** 5 FH TX and BBDEV */
                    XRAN_SET_WORKER_INFO(p_worker_info[6], 5, "fh_eth_tx", process_dpdk_io_tx, NULL, 1);
                    /** 6 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[7], 6, "fh_up_gen", xran_tx_symb_ring_processing_func, (void*)(((1L<<1)) & xran_port_mask), 1);
                    /** 7 UP GEN **/
                    XRAN_SET_WORKER_INFO(p_worker_info[8], 7, "fh_up_gen", xran_tx_symb_ring_processing_func, (void*)(((1L<<2)) & xran_port_mask), 1);

                    for(i = 0; i < numRUs; i++)
                    {
                        struct xran_device_ctx * p_dev_update =  xran_dev_get_ctx_by_id(i);
                        if(p_dev_update == NULL)
                        {
                            print_err("p_dev_update\n");
                            return XRAN_STATUS_FAIL;
                        }
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_UL]=7;
                        arr_job2wrk_id[i][XRAN_JOB_TYPE_CP_DL]=7;
                        if(i >= 1)
                        {
                            //p_dev_update->tx_sym_gen_func = xran_process_tx_sym_cp_on_dispatch_opt;
                            p_dev_update->tx_sym_func = xran_tx_sym_cp_off;
                            printf("p [%d] xran_tx_sym_cp_off\n", i);
                        }
                    }
                } /* -- if xran->ports == 4 -- */

                else
                {
                    print_err("unsupported configuration\n");
                    return XRAN_STATUS_FAIL;
                }
                break;
            case 10:
                /*** O_RU specific config */
                /* timing core */
                XRAN_SET_WORKER_INFO(p_worker_info[0], -1, "timing", NULL, NULL, 1);
                struct xran_device_ctx *pDevCtx;
                uint8_t port_id;

                for(port_id=0; port_id < 3; ++port_id)
                {
                    pDevCtx = xran_dev_get_ctx_by_id(port_id);
                    if(pDevCtx)
                    {
                        pDevCtx->tx_sym_gen_func = xran_process_tx_sym_cp_on_dispatch_opt;
                        pDevCtx->use_tx_sym_gen_func = 1;
                    }
                    else
                    {
                        print_err("Failed to get devCtx");
                        return XRAN_STATUS_FAIL;
                    }
                }

                /* workers */
                /** 0  Eth RX */
                XRAN_SET_WORKER_INFO(p_worker_info[1], 0, "fh_eth_rx", process_dpdk_io_rx, NULL, 1);
                /** 1  FH RX Port0 */
                XRAN_SET_WORKER_INFO(p_worker_info[2], 1, "fh_rx_p0", ring_processing_func_per_port, (void*)0, 1);
                /** 2  FH RX Port1 */
                XRAN_SET_WORKER_INFO(p_worker_info[3], 2, "fh_rx_p1", ring_processing_func_per_port, (void*)1, 1);
                /** 3  FH RX Port2 */
                XRAN_SET_WORKER_INFO(p_worker_info[4], 3, "fh_rx_p2", ring_processing_func_per_port, (void*)2, 1);
                /**  FH TX and BBDEV */
                XRAN_SET_WORKER_INFO(p_worker_info[5], 4, "fh_eth_tx", process_dpdk_io_tx, NULL, 1);
                /** UP Gen P0 */
                XRAN_SET_WORKER_INFO(p_worker_info[6], 5, "up_gen_p0", xran_dl_pkt_ring_processing_func, (void*)(1<<0), 1);
                /** UP Gen P1 */
                XRAN_SET_WORKER_INFO(p_worker_info[7], 6, "up_gen_p1", xran_dl_pkt_ring_processing_func, (void*)(1<<1), 1);
                /** UP Gen P2 */
                XRAN_SET_WORKER_INFO(p_worker_info[8], 7, "up_gen_p2", xran_dl_pkt_ring_processing_func, (void*)(1<<2), 1);
                /** not used  */
                XRAN_SET_WORKER_INFO(p_worker_info[9], 8, "not used", NULL, NULL, 1);
                break;

            default:
                print_err("unsupported configuration Cat %d numports %d total_num_cores = %d\n", fh_cfg->ru_conf.xranCat, numRUs, total_num_cores);
                return XRAN_STATUS_FAIL;
        } /* switch(total_num_cores) */
    } /* else if((fh_cfg->ru_conf.xranCat == XRAN_CATEGORY_B || is_mixed_cat ) && numRUs > 1) */
    else
    {
        print_err("unsupported configuration\n");
        return XRAN_STATUS_FAIL;
    }

    if(xran_update_worker_info(p_worker_info, eth_ctx, core_map, numRUs, arr_job2wrk_id, total_num_cores) != XRAN_STATUS_SUCCESS)
    {
        print_err("xran_update_worker_info failed!!");
        return XRAN_STATUS_FAIL;
    }

    nWorkerCore = 1LL;
    if(eth_ctx->io_cfg.pkt_proc_core)
    {
        for(i = 0; i < coreNum && i < 64; i++)
        {
            if(nWorkerCore & (uint64_t)eth_ctx->io_cfg.pkt_proc_core
                && eth_ctx->pkt_wrk_cfg[eth_ctx->num_workers].arg != NULL)
            {
#ifndef POLL_EBBU_OFFLOAD
                xran_dev_add_usedcore(i);
                if(rte_eal_remote_launch(eth_ctx->pkt_wrk_cfg[eth_ctx->num_workers].f, eth_ctx->pkt_wrk_cfg[eth_ctx->num_workers].arg, i))
                    rte_panic("eth_ctx->pkt_wrk_cfg[eth_ctx->num_workers].f() failed to start\n");

                eth_ctx->pkt_wrk_cfg[i].state = 1;

                if(eth_ctx->pkt_proc_core_id == 0)
                    eth_ctx->pkt_proc_core_id = i;

                //printf("spawn worker %d core %d\n",eth_ctx->num_workers, i);
                eth_ctx->worker_core[eth_ctx->num_workers++] = i;
#endif
            }
            nWorkerCore = nWorkerCore << 1;
        }
    }

    nWorkerCore = 1LL;
    if(eth_ctx->io_cfg.pkt_proc_core_64_127)
    {
        for (i = 64; i < coreNum && i < 128; i++)
        {
            if(nWorkerCore & (uint64_t)eth_ctx->io_cfg.pkt_proc_core_64_127
                && eth_ctx->pkt_wrk_cfg[eth_ctx->num_workers].arg != NULL)
            {
#ifndef POLL_EBBU_OFFLOAD
                xran_dev_add_usedcore(i);
                if (rte_eal_remote_launch(eth_ctx->pkt_wrk_cfg[eth_ctx->num_workers].f, eth_ctx->pkt_wrk_cfg[eth_ctx->num_workers].arg, i))
                    rte_panic("eth_ctx->pkt_wrk_cfg[eth_ctx->num_workers].f() failed to start\n");

                eth_ctx->pkt_wrk_cfg[i].state = 1;

                if(eth_ctx->pkt_proc_core_id == 0)
                    eth_ctx->pkt_proc_core_id = i;

                //printf("spawn worker %d core %d\n",eth_ctx->num_workers, i);
                eth_ctx->worker_core[eth_ctx->num_workers++] = i;
#endif
            }
            nWorkerCore = nWorkerCore << 1;
        }
    }

    return XRAN_STATUS_SUCCESS;
}


int32_t xran_start_worker_threads()
{
    int32_t ret = 0;
    struct xran_io_cfg *ioCfg;

    ioCfg = xran_get_sysiocfg();

    if((uint16_t)ioCfg->port[XRAN_UP_VF] != 0xFFFF)
    {
        printf("Start Workers !!\n");
        if((ret = xran_spawn_workers()) < 0)
        {
            return ret;
        }
        //printf("%s [CPU %2d] [PID: %6d]\n", __FUNCTION__,  sched_getcpu(), getpid());
    }

    return XRAN_STATUS_SUCCESS;
}

/**
 * xran_open:
 * This function is called per xRAN port and performs following -
 *   Intialize xran_device_ctx fields
 *   Initialize SRS, PRACH etc configs
 *   Create xran callbacks
 * */
int32_t xran_open(void *pHandle, struct xran_fh_config *pConf)
{
    int32_t ret = XRAN_STATUS_SUCCESS;
    int32_t i,j;
    int16_t nNumerology = 0;
    struct xran_device_ctx  *pDevCtx = NULL;
    struct xran_fh_config   *pFhCfg  = NULL;
    struct xran_ethdi_ctx   *eth_ctx = xran_ethdi_get_ctx();
    struct xran_system_config *sysCfg = xran_get_systemcfg();
    int64_t offset_sec, offset_nsec;
    uint8_t mu=0;
    struct xran_io_cfg *ioCfg;
    int appMode;

    ioCfg = xran_get_sysiocfg();
    appMode = xran_get_syscfg_appmode();

    if(pConf->dpdk_port < XRAN_PORTS_NUM)
    {
        pDevCtx  = xran_dev_get_ctx_by_id(pConf->dpdk_port);
    }
    else
    {
        print_err("@0x%p [ru %d ] pConf->dpdk_port > XRAN_PORTS_NUM\n", pConf,  pConf->dpdk_port);
        return XRAN_STATUS_FAIL;
    }

    if(pDevCtx == NULL)
    {
        print_err("[ru %d] pDevCtx == NULL ", pConf->dpdk_port);
        return XRAN_STATUS_FAIL;
    }

    if(NULL == pConf->activeMUs)
    {
        print_err("activeMUs=NULL. pointer to a xran_active_numerologies_per_tti structure must be provided\n");
        return XRAN_STATUS_FAIL;
    }

    pFhCfg = &pDevCtx->fh_cfg;
    memcpy(pFhCfg, pConf, sizeof(struct xran_fh_config));

    if(pConf->log_level)
    {
        printf("RU%d Opening...  %s Category %s\n", pConf->dpdk_port,
        (pFhCfg->ru_conf.xranTech == XRAN_RAN_5GNR) ? "5G NR" : "LTE",
        (pFhCfg->ru_conf.xranCat == XRAN_CATEGORY_A) ? "A" : "B");
    }

    if (pConf->RemoteMACvalid)
    {
        xran_update_eth_addr(&sysCfg->io_cfg, pConf->dpdk_port, pConf->numRemoteMAC, pConf->RemoteMAC);
    }

    pDevCtx->enableCP    = pConf->enableCP;
    pDevCtx->enableSrs   = pConf->srsEnable;
    pDevCtx->enableSrsCp   = pConf->srsEnableCp;
    pDevCtx->nSrsDelaySym   = pConf->SrsDelaySym;
    pDevCtx->puschMaskEnable = pConf->puschMaskEnable;
    pDevCtx->puschMaskSlot = pConf->puschMaskSlot;
    pDevCtx->DynamicSectionEna = pConf->DynamicSectionEna;
    pDevCtx->RunSlotPrbMapBySymbolEnable = pConf->RunSlotPrbMapBySymbolEnable;
    pDevCtx->dssEnable = pConf->dssEnable;
    pDevCtx->dssPeriod = pConf->dssPeriod;
    pDevCtx->csirsEnable = pConf->csirsEnable;

    pDevCtx->cfged_nCC = pConf->nCC;       /* if 'open per port' implemented, need to increase this by 1 */
    //pDevCtx->cfged_CCId[];
    // printf()
    nNumerology = xran_get_conf_highest_numerology(pDevCtx);
    pDevCtx->perMu = (xran_device_per_mu_fields*)xran_malloc("perMU fields",(nNumerology+1)*sizeof(xran_device_per_mu_fields),RTE_CACHE_LINE_SIZE);

    for(i = 0;i < pDevCtx->fh_cfg.numMUs; i++)
    {
        uint8_t mu = pDevCtx->fh_cfg.mu_number[i];
        pDevCtx->perMu[mu].p_dev_ctx = pDevCtx; /* Set the backreference to device context for easy reference when we pass the pointers to perMu[i] around */
        pDevCtx->perMu[mu].mu        = mu;//pDevCtx->fh_cfg.mu_number[i];
    }

    for(i = 0;i < pDevCtx->fh_cfg.numMUs; i++)
    {
        uint8_t mu = pDevCtx->fh_cfg.mu_number[i];
        for (j = 0; j< XRAN_NUM_OF_SYMBOL_PER_SLOT; j++)
        {
            LIST_INIT (&pDevCtx->perMu[mu].sym_cb_list_head[j]);
        }
    }

    for(i=0; i<pConf->dssPeriod; i++)
    {
        pDevCtx->technology[i] = pConf->technology[i];
    }

    for(i=0; i< pConf->numMUs; i++)
    {
        mu = pConf->mu_number[i];
        pDevCtx->perMu[mu].eaxcOffset = pConf->perMu[mu].eaxcOffset;
    }

    if(pConf->GPS_Alpha || pConf->GPS_Beta )
    {
        offset_sec = pConf->GPS_Beta / 100;    /* resolution of beta is 10ms */
        offset_nsec = (pConf->GPS_Beta - offset_sec * 100) * 1e7 + pConf->GPS_Alpha;
        sysCfg->offset_sec = offset_sec;
        sysCfg->offset_nsec = offset_nsec;
    }
    else
    {
        sysCfg->offset_sec  = 0;
        sysCfg->offset_nsec = 0;
    }

    if(pConf->nCC > XRAN_MAX_SECTOR_NR)
    {
        if(pConf->log_level)
            printf("Number of cells %d exceeds max number supported %d!\n", pConf->nCC, XRAN_MAX_SECTOR_NR);
        pConf->nCC = XRAN_MAX_SECTOR_NR;
    }

    if(pConf->ru_conf.iqOrder != XRAN_I_Q_ORDER  || pConf->ru_conf.byteOrder != XRAN_NE_BE_BYTE_ORDER )
    {
        print_err("Byte order and/or IQ order is not supported [IQ %d byte %d]\n", pConf->ru_conf.iqOrder, pConf->ru_conf.byteOrder);
        return XRAN_STATUS_FAIL;
    }

    if(pConf->ru_conf.xran_max_frame)
    {
       xran_max_frame = pConf->ru_conf.xran_max_frame;
       printf("xran_max_frame %d\n", xran_max_frame);
    }

    if(xran_get_syscfg_appmode() == O_RU)
    {
        if((ret = xran_ruemul_init(pDevCtx)) < 0)
            return ret;
    }

    for(i=0; i<pDevCtx->fh_cfg.numMUs; i++)
    {
        uint8_t mu = pDevCtx->fh_cfg.mu_number[i];
        /* setup PRACH configuration for C-Plane */
        if(pConf->dssEnable)
        {
            if((ret  = xran_init_prach(pConf, pDevCtx, XRAN_RAN_5GNR, mu))< 0)
                return ret;
            if((ret  =  xran_init_prach(pConf, pDevCtx, XRAN_RAN_LTE, mu))< 0)
                return ret;
        }
        else
        {
            if(pConf->ru_conf.xranTech == XRAN_RAN_5GNR)
            {
                if((ret  = xran_init_prach(pConf, pDevCtx, XRAN_RAN_5GNR, mu))< 0)
                {
                    return ret;
                }
            }
            else if (pConf->ru_conf.xranTech == XRAN_RAN_LTE)
            {
                if(mu == XRAN_NBIOT_MU)
                {   /*NBIOT PRACH handling is done inside xran_init_prach API*/
                    if((ret  = xran_init_prach(pConf, pDevCtx, XRAN_RAN_5GNR, mu))< 0)
                        return ret;
                }
                else if((ret  =  xran_init_prach_lte(pConf, pDevCtx))< 0)
                {
                    return ret;
                }
            }
        }
        pDevCtx->perMu[mu].enablePrach = pConf->perMu[mu].prachEnable; /*Make this per numerology*/
        xran_fs_slot_limit_init(mu);
    }

    if((ret  = xran_init_srs(pConf, pDevCtx))< 0)
        return ret;

    if((ret  = xran_cp_init_sectiondb(pDevCtx)) < 0)
        return ret;

    if((ret  = xran_init_sectionid(pDevCtx)) < 0)
        return ret;

    if((ret  = xran_init_seqid(pDevCtx)) < 0)
        return ret;

    (&(pDevCtx->csirs_cfg))->csirsEaxcOffset = pConf->csirs_conf.csirsEaxcOffset;

    if((uint16_t)eth_ctx->io_cfg.port[XRAN_UP_VF] != 0xFFFF)    /* TODO: REVIEW */
    {
        if((ret  = xran_init_vfs_mapping(pDevCtx)) < 0)
        {
            return ret;
        }

        if(pDevCtx->numRxq > 1)
        {
            if((ret  = xran_init_vf_rxq_to_pcid_mapping(pDevCtx)) < 0)
                return ret;
        }
    }

    if(interval_us > xran_fs_get_tti_interval(nNumerology))
    {
        interval_us = xran_fs_get_tti_interval(nNumerology); //only update interval_us based on maximum numerology
        printf("%s: interval_us=%ld, interval_us_local=%d\n", __FUNCTION__, interval_us, xran_fs_get_tti_interval(nNumerology));
    }

    if(nNumerology > xran_timingsource_get_numerology())
    {
        printf("RU%d has higher numerology(%d) than current(%d). Updating..\n",
            pConf->dpdk_port, nNumerology, xran_timingsource_get_numerology());
        if(nNumerology != XRAN_NBIOT_MU)
            xran_timingsource_set_numerology(nNumerology);
        else
            xran_timingsource_set_numerology(0); // NBIOT supported for LTE
    }

    for(i = 0 ; i <pConf->nCC; i++)
    {
        printf("Configuring RU%d CC%d\n", pConf->dpdk_port, i);
        xran_fs_set_slot_type(pConf->dpdk_port, i, pConf->frame_conf.nFrameDuplexType, pConf->frame_conf.nTddPeriod,
            pConf->frame_conf.sSlotConfig);
    }

    /* if send_xpmbuf2ring needs to be changed from default functions,
     * then those should be set between xran_init and xran_open */
    if(pDevCtx->send_cpmbuf2ring == NULL)
        pDevCtx->send_cpmbuf2ring    = xran_ethdi_mbuf_send_cp;
    if(pDevCtx->send_upmbuf2ring == NULL)
        pDevCtx->send_upmbuf2ring    = xran_ethdi_mbuf_send;

    if(pDevCtx->tx_sym_gen_func == NULL)
    {
        if(xran_get_syscfg_bbuoffload())
        {
            pDevCtx->tx_sym_gen_func = xran_process_tx_sym_cp_on_ring;
        }
        else
        {
            if(pFhCfg->ru_conf.xranCat == XRAN_CATEGORY_A)
            {
                pDevCtx->tx_sym_gen_func = xran_process_tx_sym_cp_on_opt;
            }
            else
            {
                pDevCtx->tx_sym_gen_func = xran_process_tx_sym_cp_on_dispatch_opt;
            }
        }
    }

    /* By default RU wont trigger tx_sym_gen_func on timing core.
       For cases using tx_sym_gen_func, it should be set in xran_spawn_workers */
    if(xran_get_syscfg_appmode() == O_RU)
        pDevCtx->use_tx_sym_gen_func = 0;

    printf("bbu_offload %d\n", xran_get_syscfg_bbuoffload());
#ifdef POLL_EBBU_OFFLOAD
    printf("Macro POLL_EBBU_OFFLOAD defined, eBBUPool framework will handle polling events\n");
#endif

    if(pConf->dpdk_port == 0)
    {
        if(!ioCfg->eowd_cmn[appMode].owdm_enable)
        {
            if((uint16_t)eth_ctx->io_cfg.port[XRAN_UP_VF] != 0xFFFF)
            {
                if ((ret = xran_timing_create_cbs(pDevCtx)) < 0)
                {
                    return ret;
                }
            }
            else if(pConf->log_level)
                printf("Eth port was not open. Processing thread was not started\n");
        }
    }
    else
    {
        if((uint16_t)eth_ctx->io_cfg.port[XRAN_UP_VF] != 0xFFFF)
        {
            if ((ret = xran_timing_create_cbs(pDevCtx)) < 0)
            {
                return ret;
            }
        }
    }

    xran_set_active_ru(pConf->dpdk_port);
    printf("XRAN Open RU%d [%d:%08X]\n", pConf->dpdk_port, xran_get_systemcfg()->active_nRU, xran_get_systemcfg()->active_RU);

    return ret;
}

int32_t xran_start(void *pHandle)
{
    uint32_t port_id;
    struct tm * ptm;
    struct timespec ts;
    char buff[100];
    struct xran_device_ctx *pDevCtx;

    if(pHandle == NULL)
    {
        print_err("Invalid RU handle! [%p]\n", pHandle);
        return(XRAN_STATUS_FAIL);
    }

    pDevCtx = (struct xran_device_ctx *)pHandle;
    port_id = pDevCtx->xran_port_id;
    if(port_id >= XRAN_PORTS_NUM)
    {
        print_err("Invalid RU port ID! [%d]\n", port_id);
        return(XRAN_STATUS_FAIL);
    }

    if(pDevCtx->fh_cfg.numMUs == 1 && pDevCtx->fh_cfg.mu_number[0] == 3 && pDevCtx->vMuInfo.ssbInfo.ssbMu == 4)
    {
        // Init SSB section DB
        if(xran_cp_init_vMu_sectiondb(pDevCtx, pDevCtx->vMuInfo.ssbInfo.ssbMu, pDevCtx->vMuInfo.ssbInfo.ruPortId_offset + xran_get_num_eAxc(pHandle)) != XRAN_STATUS_SUCCESS){
            print_err("xran_cp_init_vMu_sectiondb failed port_id %d ssbMu %hhu",port_id,pDevCtx->vMuInfo.ssbInfo.ssbMu);
            return XRAN_STATUS_FAIL;
        }
    }

    if(rte_atomic32_test_and_set(&(xran_get_systemcfg()->nStart)))
    {
        timespec_get(&ts, TIME_UTC);
        ptm = gmtime(&ts.tv_sec);
        if(ptm)
        {
            strftime(buff, sizeof(buff), "%D %T", ptm);
            printf("%s: XRAN start time: %s.%09ld UTC [%ld]\n",
                (xran_get_syscfg_appmode() == O_DU ? "O-DU": "O-RU"), buff, ts.tv_nsec, interval_us);
        }

        if(xran_get_syscfg_appmode() == O_RU)
        {
            uint8_t port_idx, i, j, mu;
            for(port_idx = 0; port_idx < xran_get_syscfg_totalnumru(); ++port_idx)
            {
                struct xran_device_ctx *pDevCtx_port = xran_dev_get_ctx_by_id(port_idx);
                if(unlikely(pDevCtx_port == NULL))
                    rte_panic("pDevCtx_port for port %hhu is NULL ",port_idx);

                for(j=0;j<pDevCtx_port->fh_cfg.numMUs;j++)
                {
                    mu = pDevCtx_port->fh_cfg.mu_number[j];
                    struct xran_prb_map * prbMap0 = (struct xran_prb_map *) pDevCtx_port->perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[0][0][0].sBufferList.pBuffers->pData;
                    for(i = 0; i < XRAN_MAX_SECTIONS_PER_SLOT && i < prbMap0->nPrbElm; i++)
                        pDevCtx_port->perMu[mu].numSetBFWs_arr[i] = prbMap0->prbMap[i].bf_weight.numSetBFWs;
                }
            }
        }

        pDevCtx = XRAN_GET_DEV_CTX;
        if(pDevCtx->eowd_cmn[xran_get_syscfg_appmode()].owdm_enable)
            xran_if_current_state = XRAN_OWDM;
        else
            xran_if_current_state = XRAN_RUNNING;
    }
    else
    {
        rte_atomic32_inc(&(xran_get_systemcfg()->nStart));
    }

    printf("XRAN Start! RU%d [%d]\n", port_id, rte_atomic32_read(&(xran_get_systemcfg()->nStart)));

    return 0;
}

int32_t xran_activate_cc(int32_t port_id, int32_t cc_id)
{
    struct xran_device_ctx *pDevCtx;

    if(cc_id >= XRAN_MAX_SECTOR_NR)
        return(-1);

    pDevCtx = xran_dev_get_ctx_by_id(port_id);
    if(pDevCtx)
    {
        rte_spinlock_lock(&pDevCtx->spinLock);
        if(xran_isactive_cc(pDevCtx, cc_id))
        {
            rte_spinlock_unlock(&pDevCtx->spinLock);
            return(-1);
        }
        pDevCtx->active_CC |= 1L << cc_id;
        pDevCtx->active_nCC++;
        rte_spinlock_unlock(&pDevCtx->spinLock);
    }
    else
    {
        printf("Invalid port(RU) index - %d\n", port_id);
        return(-1);
    }

    printf("Enable RU%d CC%d [%016lX:%d]\n", pDevCtx->xran_port_id, cc_id, pDevCtx->active_CC, pDevCtx->active_nCC);
    return(0);
}

int32_t xran_deactivate_cc(int32_t port_id, int32_t cc_id)
{
    struct xran_device_ctx *pDevCtx;

    if(cc_id >= XRAN_MAX_SECTOR_NR)
        return(-1);

    pDevCtx = xran_dev_get_ctx_by_id(port_id);
    if(pDevCtx)
    {
        rte_spinlock_lock(&pDevCtx->spinLock);
        if(!xran_isactive_cc(pDevCtx, cc_id))
        {
            rte_spinlock_unlock(&pDevCtx->spinLock);
            return(-1);
        }
        pDevCtx->active_CC &= ~(1L << cc_id);
        pDevCtx->active_nCC--;
        rte_spinlock_unlock(&pDevCtx->spinLock);
    }
    else
    {
        printf("Invalid port(RU) index - %d\n", port_id);
        return(-1);
    }

#if 0
    {
    int i, j, z;
    uint8_t mu;

    pDevCtx->pCallback[cc_id]           = NULL;
    pDevCtx->pCallbackTag[cc_id]        = NULL;
    pDevCtx->pPrachCallback[cc_id]      = NULL;
    pDevCtx->pPrachCallbackTag[cc_id]   = NULL;
    pDevCtx->pSrsCallback[cc_id]        = NULL;
    pDevCtx->pSrsCallbackTag[cc_id]     = NULL;
    pDevCtx->pCsirsCallback[cc_id]      = NULL;
    pDevCtx->pCsirsCallbackTag[cc_id]   = NULL;

    for(i=0; i < pDevCtx->fh_cfg.numMUs; i++)
    {
        mu = pDevCtx->fh_cfg.mu_number[i];
        for(j = 0; j < XRAN_N_FE_BUF_LEN; j++)
        {
            for(z = 0; z < XRAN_MAX_ANTENNA_NR; z++)
            {
                /* U-plane TX */
                pDevCtx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][cc_id][z].sBufferList.nNumBuffers = 0;
                pDevCtx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][cc_id][z].sBufferList.pBuffers = NULL;
                pDevCtx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][cc_id][z].sBufferList.pUserData = NULL;
                pDevCtx->perMu[mu].sFrontHaulTxBbuIoBufCtrl[j][cc_id][z].sBufferList.pPrivateMetaData = NULL;
                /* C-plane TX */
                pDevCtx->perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][cc_id][z].sBufferList.nNumBuffers = 0;
                pDevCtx->perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][cc_id][z].sBufferList.pBuffers = NULL;
                pDevCtx->perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][cc_id][z].sBufferList.pUserData = NULL;
                pDevCtx->perMu[mu].sFrontHaulTxPrbMapBbuIoBufCtrl[j][cc_id][z].sBufferList.pPrivateMetaData = NULL;
                /* U-plane RX */
                pDevCtx->perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][cc_id][z].sBufferList.nNumBuffers = 0;
                pDevCtx->perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][cc_id][z].sBufferList.pBuffers = NULL;
                pDevCtx->perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][cc_id][z].sBufferList.pUserData = NULL;
                pDevCtx->perMu[mu].sFrontHaulRxBbuIoBufCtrl[j][cc_id][z].sBufferList.pPrivateMetaData = NULL;
                /* C-plane RX */
                pDevCtx->perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][cc_id][z].sBufferList.nNumBuffers = 0;
                pDevCtx->perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][cc_id][z].sBufferList.pBuffers = NULL;
                pDevCtx->perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][cc_id][z].sBufferList.pUserData = NULL;
                pDevCtx->perMu[mu].sFrontHaulRxPrbMapBbuIoBufCtrl[j][cc_id][z].sBufferList.pPrivateMetaData = NULL;
                /* PRACH */
                pDevCtx->perMu[mu].sFHPrachRxBbuIoBufCtrl[j][cc_id][z].sBufferList.nNumBuffers = 0;
                pDevCtx->perMu[mu].sFHPrachRxBbuIoBufCtrl[j][cc_id][z].sBufferList.pBuffers = NULL;
                pDevCtx->perMu[mu].sFHPrachRxBbuIoBufCtrl[j][cc_id][z].sBufferList.pUserData = NULL;
                pDevCtx->perMu[mu].sFHPrachRxBbuIoBufCtrl[j][cc_id][z].sBufferList.pPrivateMetaData = NULL;
                /* SRS */
                pDevCtx->perMu[mu].sFHSrsRxBbuIoBufCtrl[j][cc_id][z].sBufferList.nNumBuffers = 0;
                pDevCtx->perMu[mu].sFHSrsRxBbuIoBufCtrl[j][cc_id][z].sBufferList.pBuffers = NULL;
                pDevCtx->perMu[mu].sFHSrsRxBbuIoBufCtrl[j][cc_id][z].sBufferList.pUserData = NULL;
                pDevCtx->perMu[mu].sFHSrsRxBbuIoBufCtrl[j][cc_id][z].sBufferList.pPrivateMetaData = NULL;
                pDevCtx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][cc_id][z].sBufferList.nNumBuffers = 0;
                pDevCtx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][cc_id][z].sBufferList.pBuffers = NULL;
                pDevCtx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][cc_id][z].sBufferList.pUserData = NULL;
                pDevCtx->perMu[mu].sFHSrsRxPrbMapBbuIoBufCtrl[j][cc_id][z].sBufferList.pPrivateMetaData = NULL;
                /* CSI-RS */
                pDevCtx->perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][cc_id][z].sBufferList.nNumBuffers = 0;
                pDevCtx->perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][cc_id][z].sBufferList.pBuffers = NULL;
                pDevCtx->perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][cc_id][z].sBufferList.pUserData = NULL;
                pDevCtx->perMu[mu].sFHCsirsTxBbuIoBufCtrl[j][cc_id][z].sBufferList.pPrivateMetaData = NULL;
                pDevCtx->perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][cc_id][z].sBufferList.nNumBuffers = 0;
                pDevCtx->perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][cc_id][z].sBufferList.pBuffers = NULL;
                pDevCtx->perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][cc_id][z].sBufferList.pUserData = NULL;
                pDevCtx->perMu[mu].sFHCsirsTxPrbMapBbuIoBufCtrl[j][cc_id][z].sBufferList.pPrivateMetaData = NULL;
            }
        }
    }
    }
#endif

    /* Reset statisitcs */
    if(xran_get_numactiveccs_ru(pDevCtx) == 0)
        memset(&pDevCtx->fh_counters, 0, sizeof(struct xran_common_counters));

    printf("Disable RU%d CC%d [%016lX:%d]\n", pDevCtx->xran_port_id, cc_id, pDevCtx->active_CC, pDevCtx->active_nCC);
    return(0);
}

int32_t xran_stop(void *pHandle)
{
    uint32_t port_id;
    struct xran_device_ctx *pDevCtx;

    if(pHandle == NULL)
    {
        print_err("Invalid RU handle! [%p]\n", pHandle);
        return(XRAN_STATUS_FAIL);
    }

    pDevCtx = (struct xran_device_ctx *)pHandle;
    port_id = pDevCtx->xran_port_id;
    if(port_id >= XRAN_PORTS_NUM)
    {
        print_err("Invalid RU port ID! [%d]\n", port_id);
        return(XRAN_STATUS_FAIL);
    }

    if(xran_get_if_state() == XRAN_STOPPED)
    {
        print_err("Already STOPPED!!");
        return (-1);
    }

    printf("XRAN Stop! RU%d [%d]\n", port_id, rte_atomic32_read(&(xran_get_systemcfg()->nStart)));

    if(xran_get_numactiveccs_ru(pDevCtx) == 0)
    {
        pDevCtx->pCallback[0]           = NULL;
        pDevCtx->pCallbackTag[0]        = NULL;
        pDevCtx->pPrachCallback[0]      = NULL;
        pDevCtx->pPrachCallbackTag[0]   = NULL;
        pDevCtx->pSrsCallback[0]        = NULL;
        pDevCtx->pSrsCallbackTag[0]     = NULL;
        pDevCtx->pCsirsCallback[0]      = NULL;
        pDevCtx->pCsirsCallbackTag[0]   = NULL;
        pDevCtx->xran2phy_mem_ready = 0;
        xran_close(pHandle);        /* TODO */
    }

    if(rte_atomic32_dec_and_test(&(xran_get_systemcfg()->nStart)))
    {
        xran_if_current_state = XRAN_STOPPED;
        printf("  Received total number of stops! Stopping......\n");
    }

    return 0;
}

int32_t xran_shutdown(void *pHandle)
{
    if(rte_atomic32_read(&(xran_get_systemcfg()->nStart)) == 0)
    {
        xran_if_current_state = XRAN_STOPPED;
        printf("shutting down xRAN......\n");
    }
    return 0;
}


int32_t xran_close(void *pHandle)
{
    int32_t ret = XRAN_STATUS_SUCCESS;
    uint32_t port_id;
    struct xran_device_ctx *pDevCtx;;

    if(pHandle == NULL)
    {
        print_err("Invalid RU handle! [%p]\n", pHandle);
        return(XRAN_STATUS_FAIL);
    }

    pDevCtx = (struct xran_device_ctx *)pHandle;
    port_id = pDevCtx->xran_port_id;
    if(port_id >= XRAN_PORTS_NUM)
    {
        print_err("Invalid RU port ID! [%d]\n", port_id);
        return(XRAN_STATUS_FAIL);
    }

    ret = xran_cp_free_sectiondb(pDevCtx);

    if(xran_get_syscfg_appmode() == O_RU)
        xran_ruemul_release(pDevCtx);

    xran_set_deactive_ru(pDevCtx->xran_port_id);

    printf("XRAN Close RU%d [%d:%08X]\n", pDevCtx->xran_port_id, xran_get_systemcfg()->active_nRU, xran_get_systemcfg()->active_RU);

    return ret;
}

int32_t xran_get_ru_isactive(__attribute__((unused)) void *pHandle)
{
    return(xran_isactive_ru(pHandle));
}

int32_t xran_get_cc_isactive(void *pHandle, uint32_t cc_id)
{
    if(pHandle)
        return(xran_isactive_cc((struct xran_device_ctx *)pHandle, cc_id));
    else
        return(0);
}

/* send_cpmbuf2ring and send_upmbuf2ring should be set between xran_init and xran_open
 * each cb will be set by default duing open if it is set by NULL */
int32_t
xran_register_cb_mbuf2ring(xran_ethdi_mbuf_send_fn mbuf_send_cp, xran_ethdi_mbuf_send_fn mbuf_send_up)
{
    struct xran_device_ctx *pDevCtx;

    if(xran_get_if_state() == XRAN_RUNNING) {
        print_err("Cannot register callback while running!!\n");
        return (-1);
        }

    pDevCtx = XRAN_GET_DEV_CTX;

    pDevCtx->send_cpmbuf2ring    = mbuf_send_cp;
    pDevCtx->send_upmbuf2ring    = mbuf_send_up;

    pDevCtx->tx_sym_gen_func = xran_process_tx_sym_cp_on_opt;

    return (0);
}

int32_t xran_timingsource_get_slotidx(uint32_t *nFrameIdx, uint32_t *nSubframeIdx, uint32_t *nSlotIdx, uint64_t *nSecond, uint8_t mu)
{
    int32_t tti = 0;
    uint16_t interval_mu;

    if(mu == XRAN_DEFAULT_MU)
        mu = xran_timingsource_get_numerology();

    interval_mu = xran_fs_get_tti_interval(mu);

#ifndef POLL_EBBU_OFFLOAD
    tti           = (int32_t)XranGetTtiNum(xran_lib_ota_sym_idx_mu[mu], XRAN_NUM_OF_SYMBOL_PER_SLOT);
#else
    PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();
    tti           = (int32_t)XranGetTtiNum(pCtx->ebbu_offload_ota_sym_cnt_mu[mu], XRAN_NUM_OF_SYMBOL_PER_SLOT);
#endif
    *nSlotIdx     = (uint32_t)XranGetSlotNum(tti, SLOTNUM_PER_SUBFRAME(interval_mu));
    *nSubframeIdx = (uint32_t)XranGetSubFrameNum(tti,SLOTNUM_PER_SUBFRAME(interval_mu), SUBFRAMES_PER_SYSTEMFRAME);
    *nFrameIdx    = (uint32_t)XranGetFrameNum(tti, xran_getSfnSecStart(), SUBFRAMES_PER_SYSTEMFRAME, SLOTNUM_PER_SUBFRAME(interval_mu));
    *nSecond      = xran_timingsource_get_current_second();

    return tti;
}
int32_t xran_get_slot_idx(__attribute__((unused)) uint32_t PortId, uint32_t *nFrameIdx, uint32_t *nSubframeIdx, uint32_t *nSlotIdx, uint64_t *nSecond, uint8_t mu)
{
    return(xran_timingsource_get_slotidx(nFrameIdx, nSubframeIdx, nSlotIdx, nSecond, mu));
}

int32_t xran_set_debug_stop(int32_t value, int32_t count)
{
    return timing_set_debug_stop(value, count);
}


int32_t xran_get_num_prb_elm(struct xran_prb_map* p_PrbMapIn, uint32_t mtu)
{
    int32_t i,j = 0;
    int16_t iqwidth, compMeth;
    struct xran_prb_elm *p_prb_elm_src;
    int32_t nRBremain;
    int32_t eth_xran_up_headers_sz;
    int32_t nmaxRB;
    uint32_t nRBSize=0;

    iqwidth = p_PrbMapIn->prbMap[0].iqWidth;
    compMeth = p_PrbMapIn->prbMap[0].compMethod;
    eth_xran_up_headers_sz = sizeof(struct eth_xran_up_pkt_hdr) - sizeof(struct data_section_hdr);
    nmaxRB  = (mtu - eth_xran_up_headers_sz - RTE_PKTMBUF_HEADROOM) /
                (xran_get_iqdata_len(1, iqwidth, compMeth) + sizeof(struct data_section_hdr));

    if (mtu==9600)
        nmaxRB--;   //for some reason when mtu is 9600, only 195 RB can be sent, not 196

    for (i = 0;i < p_PrbMapIn->nPrbElm; i++)
    {
        p_prb_elm_src = &p_PrbMapIn->prbMap[i];
        if (p_prb_elm_src->nRBSize <= nmaxRB)    //no fragmentation needed
        {
            j++;
        }
        else
        {
            nRBremain = p_prb_elm_src->nRBSize - nmaxRB;
            j++;
            while (nRBremain > 0)
            {
                nRBSize = RTE_MIN(nmaxRB, nRBremain);
                nRBremain -= nRBSize;
                j++;
            }
        }
    }

    return j;
}


int32_t xran_init_PrbMap_from_cfg(struct xran_prb_map* p_PrbMapIn, struct xran_prb_map* p_PrbMapOut, uint32_t mtu)
{
    int32_t i,j = 0;
    int16_t iqwidth, compMeth;
    struct xran_prb_elm *p_prb_elm_src, *p_prb_elm_dst, *p_prbelm_ref, *p_prbelm_to_comp;
    int32_t nRBStart_tmp, nRBremain;
    int32_t eth_xran_up_headers_sz;
    int32_t nmaxRB;

    iqwidth = p_PrbMapIn->prbMap[0].iqWidth;
    compMeth = p_PrbMapIn->prbMap[0].compMethod;
    eth_xran_up_headers_sz = sizeof(struct eth_xran_up_pkt_hdr) - sizeof(struct data_section_hdr);
    nmaxRB  = (mtu - eth_xran_up_headers_sz - RTE_PKTMBUF_HEADROOM) /
                (xran_get_iqdata_len(1, iqwidth, compMeth) + sizeof(struct data_section_hdr));

    if (mtu==9600)
        nmaxRB--;   //for some reason when mtu is 9600, only 195 RB can be sent, not 196

    memcpy(p_PrbMapOut, p_PrbMapIn, sizeof(struct xran_prb_map));
    for (i = 0;i < p_PrbMapIn->nPrbElm; i++)
    {
        p_prb_elm_src = &p_PrbMapIn->prbMap[i];
        p_prb_elm_dst = &p_PrbMapOut->prbMap[j];
        memcpy(p_prb_elm_dst, p_prb_elm_src, sizeof(struct xran_prb_elm));

        // int32_t nStartSymb, nEndSymb, numSymb, nRBStart, nRBEnd, nRBSize;
        // nStartSymb = p_prb_elm_src->nStartSymb;
        // nEndSymb = nStartSymb + p_prb_elm_src->numSymb;
        if (p_prb_elm_src->nRBSize <= nmaxRB)    //no fragmentation needed
        {
            p_prb_elm_dst->generateCpPkt = 1;
            p_prb_elm_dst->UP_nRBSize = p_prb_elm_src->nRBSize;
            p_prb_elm_dst->UP_nRBStart = p_prb_elm_src->nRBStart;
            p_prb_elm_dst->startSectId = i;
            j++;
        }
        else
        {
            nRBStart_tmp = p_prb_elm_src->nRBStart + nmaxRB;
            nRBremain = p_prb_elm_src->nRBSize - nmaxRB;
            p_prb_elm_dst->generateCpPkt = 0;
            p_prb_elm_dst->UP_nRBSize = nmaxRB;
            p_prb_elm_dst->UP_nRBStart = p_prb_elm_src->nRBStart;
            p_prb_elm_dst->startSectId = i;
            j++;
            while (nRBremain > 0)
            {
                p_prb_elm_dst = &p_PrbMapOut->prbMap[j];
                memcpy(p_prb_elm_dst, p_prb_elm_src, sizeof(struct xran_prb_elm));
                p_prb_elm_dst->generateCpPkt = 0;
                p_prb_elm_dst->UP_nRBSize = RTE_MIN(nmaxRB, nRBremain);
                p_prb_elm_dst->UP_nRBStart = nRBStart_tmp;
                nRBremain -= p_prb_elm_dst->UP_nRBSize;
                nRBStart_tmp += p_prb_elm_dst->UP_nRBSize;
                p_prb_elm_dst->startSectId = i;
                j++;
            }
            p_prb_elm_dst->generateCpPkt = 1;
        }
    }
    p_PrbMapOut->nPrbElm = j;

    /* Update/Keep the same sectionID if prbElm(s) are same except the reMask*/
    for (i = (p_PrbMapOut->nPrbElm -1) ;i >= 0; i--){
        p_prbelm_ref = &p_PrbMapOut->prbMap[i];
        if(p_prbelm_ref->reMask && p_prbelm_ref->reMask != 0xfff){
            for (j = i-1;j >= 0; j--){
                p_prbelm_to_comp = &p_PrbMapOut->prbMap[j];
                if(p_prbelm_ref->UP_nRBStart == p_prbelm_to_comp->UP_nRBStart && p_prbelm_ref->UP_nRBSize == p_prbelm_to_comp->UP_nRBSize && \
                            p_prbelm_ref->nStartSymb == p_prbelm_to_comp->nStartSymb && p_prbelm_ref->numSymb == p_prbelm_to_comp->numSymb)
                                p_prbelm_ref->startSectId = p_prbelm_to_comp->startSectId;
            }
        }
    }

    return 0;
}


int32_t xran_init_PrbMap_from_cfg_for_rx(struct xran_prb_map* p_PrbMapIn, struct xran_prb_map* p_PrbMapOut, uint32_t mtu)
{
    int32_t i,j = 0;
    struct xran_prb_elm *p_prb_elm_src, *p_prb_elm_dst, *p_prbelm_ref, *p_prbelm_to_comp;

    memcpy(p_PrbMapOut, p_PrbMapIn, sizeof(struct xran_prb_map));
    for (i = 0;i < p_PrbMapIn->nPrbElm; i++)
    {
        p_prb_elm_src = &p_PrbMapIn->prbMap[i];
        p_prb_elm_dst = &p_PrbMapOut->prbMap[j];
        memcpy(p_prb_elm_dst, p_prb_elm_src, sizeof(struct xran_prb_elm));

        p_prb_elm_dst->generateCpPkt = 1;
        p_prb_elm_dst->UP_nRBSize = p_prb_elm_src->nRBSize;
        p_prb_elm_dst->UP_nRBStart = p_prb_elm_src->nRBStart;
        p_prb_elm_dst->startSectId = j;
        j++;
    }

    p_PrbMapOut->nPrbElm = j;
    mtu = mtu + j;

    /* Update/Keep the same sectionID if prbElm(s) are same except the reMask*/
    for (i = (p_PrbMapOut->nPrbElm -1) ;i >= 0; i--){
        p_prbelm_ref = &p_PrbMapOut->prbMap[i];
        if(p_prbelm_ref->reMask && p_prbelm_ref->reMask != 0xfff){
            for (j = i-1;j >= 0; j--){
                p_prbelm_to_comp = &p_PrbMapOut->prbMap[j];
                if(p_prbelm_ref->UP_nRBStart == p_prbelm_to_comp->UP_nRBStart && p_prbelm_ref->UP_nRBSize == p_prbelm_to_comp->UP_nRBSize && \
                            p_prbelm_ref->nStartSymb == p_prbelm_to_comp->nStartSymb && p_prbelm_ref->numSymb == p_prbelm_to_comp->numSymb)
                                p_prbelm_ref->startSectId = p_prbelm_to_comp->startSectId;
            }
        }
    }

    return 0;
}


int32_t xran_init_PrbMap_by_symbol_from_cfg(struct xran_prb_map* p_PrbMapIn, struct xran_prb_map* p_PrbMapOut, uint32_t mtu, uint32_t xran_max_prb)
{
    int32_t i = 0, j = 0, nPrbElm = 0;
    int16_t iqwidth, compMeth;
    struct xran_prb_elm *p_prb_elm_src, *p_prb_elm_dst, *p_prbelm_ref, *p_prbelm_to_comp;
    struct xran_prb_elm prbMapTemp[XRAN_NUM_OF_SYMBOL_PER_SLOT];
    int32_t nRBStart_tmp, nRBremain, nStartSymb, nEndSymb, nRBStart, nRBEnd, nRBSize;
    int32_t eth_xran_up_headers_sz;
    int32_t nmaxRB;

    iqwidth = p_PrbMapIn->prbMap[0].iqWidth;
    compMeth = p_PrbMapIn->prbMap[0].compMethod;
    eth_xran_up_headers_sz = sizeof(struct eth_xran_up_pkt_hdr) - sizeof(struct data_section_hdr);
    nmaxRB  = (mtu - eth_xran_up_headers_sz - RTE_PKTMBUF_HEADROOM) /
                (xran_get_iqdata_len(1, iqwidth, compMeth) + sizeof(struct data_section_hdr));

    if (mtu==9600)
        nmaxRB--;   //for some reason when mtu is 9600, only 195 RB can be sent, not 196

    memcpy(p_PrbMapOut, p_PrbMapIn, sizeof(struct xran_prb_map));
    for(i = 0; i < XRAN_NUM_OF_SYMBOL_PER_SLOT; i++)
    {
        p_prb_elm_dst = &prbMapTemp[i];
        // nRBStart = 273;
        nRBStart = xran_max_prb;
        nRBEnd = 0;

        for(j = 0; j < p_PrbMapIn->nPrbElm; j++)
        {
            p_prb_elm_src = &(p_PrbMapIn->prbMap[j]);
            nStartSymb = p_prb_elm_src->nStartSymb;
            nEndSymb = nStartSymb + p_prb_elm_src->numSymb;

            if((i >=  nStartSymb) && (i < nEndSymb))
            {
                if(nRBStart > p_prb_elm_src->nRBStart)
                {
                    nRBStart = p_prb_elm_src->nRBStart;
                }
                if(nRBEnd < (p_prb_elm_src->nRBStart + p_prb_elm_src->nRBSize))
                {
                    nRBEnd = (p_prb_elm_src->nRBStart + p_prb_elm_src->nRBSize);
                }

                p_prb_elm_dst->nBeamIndex = p_prb_elm_src->nBeamIndex;
                p_prb_elm_dst->bf_weight_update = p_prb_elm_src->bf_weight_update;
                p_prb_elm_dst->compMethod = p_prb_elm_src->compMethod;
                p_prb_elm_dst->iqWidth = p_prb_elm_src->iqWidth;
                p_prb_elm_dst->ScaleFactor = p_prb_elm_src->ScaleFactor;
                p_prb_elm_dst->reMask = p_prb_elm_src->reMask;
                p_prb_elm_dst->BeamFormingType = p_prb_elm_src->BeamFormingType;
            }
        }

        if(nRBEnd < nRBStart)
        {
            p_prb_elm_dst->nRBStart = 0;
            p_prb_elm_dst->nRBSize = 0;
            p_prb_elm_dst->nStartSymb = i;
            p_prb_elm_dst->numSymb = 1;
        }
        else
        {
            p_prb_elm_dst->nRBStart = nRBStart;
            p_prb_elm_dst->nRBSize = nRBEnd - nRBStart;
            p_prb_elm_dst->nStartSymb = i;
            p_prb_elm_dst->numSymb = 1;
        }
    }

    for(i = 0; i < XRAN_NUM_OF_SYMBOL_PER_SLOT; i++)
    {
        if((prbMapTemp[i].nRBSize != 0))
        {
            nRBStart = prbMapTemp[i].nRBStart;
            nRBSize = prbMapTemp[i].nRBSize;
            prbMapTemp[nPrbElm].nRBStart = prbMapTemp[i].nRBStart;
            prbMapTemp[nPrbElm].nRBSize = prbMapTemp[i].nRBSize;
            prbMapTemp[nPrbElm].nStartSymb = prbMapTemp[i].nStartSymb;
            prbMapTemp[nPrbElm].nBeamIndex = prbMapTemp[i].nBeamIndex;
            prbMapTemp[nPrbElm].bf_weight_update = prbMapTemp[i].bf_weight_update;
            prbMapTemp[nPrbElm].compMethod = prbMapTemp[i].compMethod;
            prbMapTemp[nPrbElm].iqWidth = prbMapTemp[i].iqWidth;
            prbMapTemp[nPrbElm].ScaleFactor = prbMapTemp[i].ScaleFactor;
            prbMapTemp[nPrbElm].reMask = prbMapTemp[i].reMask;
            prbMapTemp[nPrbElm].BeamFormingType = prbMapTemp[i].BeamFormingType;
            i++;
            break;
        }
    }

    for(; i < XRAN_NUM_OF_SYMBOL_PER_SLOT; i++)
    {
        if((nRBStart == prbMapTemp[i].nRBStart) && (nRBSize == prbMapTemp[i].nRBSize))
        {
                prbMapTemp[nPrbElm].numSymb++;
        }
        else
        {
            nPrbElm++;
            prbMapTemp[nPrbElm].nStartSymb = prbMapTemp[i].nStartSymb;
            prbMapTemp[nPrbElm].nRBStart = prbMapTemp[i].nRBStart;
            prbMapTemp[nPrbElm].nRBSize = prbMapTemp[i].nRBSize;
            prbMapTemp[nPrbElm].nBeamIndex = prbMapTemp[i].nBeamIndex;
            prbMapTemp[nPrbElm].bf_weight_update = prbMapTemp[i].bf_weight_update;
            prbMapTemp[nPrbElm].compMethod = prbMapTemp[i].compMethod;
            prbMapTemp[nPrbElm].iqWidth = prbMapTemp[i].iqWidth;
            prbMapTemp[nPrbElm].ScaleFactor = prbMapTemp[i].ScaleFactor;
            prbMapTemp[nPrbElm].reMask = prbMapTemp[i].reMask;
            prbMapTemp[nPrbElm].BeamFormingType = prbMapTemp[i].BeamFormingType;

            nRBStart = prbMapTemp[i].nRBStart;
            nRBSize = prbMapTemp[i].nRBSize;
        }
    }

    for(i = 0; i < nPrbElm; i++)
    {
        if(prbMapTemp[i].nRBSize == 0)
            prbMapTemp[i].nRBSize = 1;
    }

    if(prbMapTemp[nPrbElm].nRBSize != 0)
        nPrbElm++;


    j = 0;

    for (i = 0;i < nPrbElm; i++)
    {
        p_prb_elm_src = &prbMapTemp[i];
        p_prb_elm_dst = &p_PrbMapOut->prbMap[j];
        memcpy(p_prb_elm_dst, p_prb_elm_src, sizeof(struct xran_prb_elm));
        if (p_prb_elm_src->nRBSize <= nmaxRB)    //no fragmentation needed
        {
            p_prb_elm_dst->generateCpPkt = 1;
            p_prb_elm_dst->UP_nRBSize = p_prb_elm_src->nRBSize;
            p_prb_elm_dst->UP_nRBStart = p_prb_elm_src->nRBStart;
            p_prb_elm_dst->startSectId = i;
            j++;
        }
        else
        {
            nRBStart_tmp = p_prb_elm_src->nRBStart + nmaxRB;
            nRBremain = p_prb_elm_src->nRBSize - nmaxRB;
            p_prb_elm_dst->generateCpPkt = 1;
            p_prb_elm_dst->UP_nRBSize = nmaxRB;
            p_prb_elm_dst->UP_nRBStart = p_prb_elm_src->nRBStart;
            p_prb_elm_dst->startSectId = i;
            j++;
            while (nRBremain > 0)
            {
                p_prb_elm_dst = &p_PrbMapOut->prbMap[j];
                memcpy(p_prb_elm_dst, p_prb_elm_src, sizeof(struct xran_prb_elm));
                p_prb_elm_dst->generateCpPkt = 0;
                p_prb_elm_dst->UP_nRBSize = RTE_MIN(nmaxRB, nRBremain);
                p_prb_elm_dst->UP_nRBStart = nRBStart_tmp;
                nRBremain -= p_prb_elm_dst->UP_nRBSize;
                nRBStart_tmp += p_prb_elm_dst->UP_nRBSize;
                p_prb_elm_dst->startSectId = i;
                j++;
            }
        }
    }

    p_PrbMapOut->nPrbElm = j;

    /* Update/Keep the same sectionID if prbElm(s) are same except the reMask*/
    for (i = (p_PrbMapOut->nPrbElm -1) ;i >= 0; i--){
        p_prbelm_ref = &p_PrbMapOut->prbMap[i];
        if(p_prbelm_ref->reMask && p_prbelm_ref->reMask != 0xfff){
            for (j = i-1;j >= 0; j--){
                p_prbelm_to_comp = &p_PrbMapOut->prbMap[j];
                if(p_prbelm_ref->UP_nRBStart == p_prbelm_to_comp->UP_nRBStart && p_prbelm_ref->UP_nRBSize == p_prbelm_to_comp->UP_nRBSize && \
                            p_prbelm_ref->nStartSymb == p_prbelm_to_comp->nStartSymb && p_prbelm_ref->numSymb == p_prbelm_to_comp->numSymb)
                                p_prbelm_ref->startSectId = p_prbelm_to_comp->startSectId;
            }
        }
    }

    return 0;
}

inline void MLogXRANTask(uint32_t taskid, uint64_t ticksstart, uint64_t ticksstop)
{
    if (mlogxranenable)
    {
        MLogTask(taskid, ticksstart, ticksstop);
    }
    return;
}

inline uint64_t MLogXRANTick(void)
{
    if (mlogxranenable)
        return MLogTick();
    else
        return 0;
}

void xran_print_error_stats(struct xran_common_counters* x_counters){
    printf("\nXRAN Error counters: (Non zero)");
    PRINT_NON_ZERO_CNTR(x_counters->rx_err_up,"rx_err_up","\n%12s: %u");
    PRINT_NON_ZERO_CNTR(x_counters->rx_err_drop,"rx_err_drop","\n%12s: %u");
    PRINT_NON_ZERO_CNTR(x_counters->rx_err_pusch,"rx_err_pusch","\n%12s: %u");
    PRINT_NON_ZERO_CNTR(x_counters->rx_err_srs,"rx_err_srs","\n%12s: %u");
    PRINT_NON_ZERO_CNTR(x_counters->rx_err_csirs,"rx_err_csirs","\n%12s: %u");
    PRINT_NON_ZERO_CNTR(x_counters->rx_err_prach,"rx_err_prach","\n%12s: %u");
    PRINT_NON_ZERO_CNTR(x_counters->rx_err_cp,"rx_err_cp","\n%12s: %u");
    PRINT_NON_ZERO_CNTR(x_counters->rx_err_ecpri,"rx_err_ecpri","\n%12s: %u");
    printf("\n\n");
}

//-------------------------------------------------------------------------------------------
/*
 *  @description
 *  This function calculates the UL and DL processing Budget
 */
//-------------------------------------------------------------------------------------------

void xran_l1budget_calc(uint8_t numerology, uint16_t t1a_max_up, uint16_t ta4_max, uint16_t *ul_budget,uint16_t *dl_budget, uint16_t *num_sym)
{
   uint8_t num_sym_ul,num_sym_dl, num_of_tti = 1;
   float slot_time;
   float symbol_time;

   slot_time = xran_fs_get_tti_interval(numerology);
   symbol_time  = (slot_time/N_SYM_PER_SLOT);
   num_sym_ul = ceil(ta4_max/symbol_time);
   num_sym_dl = (t1a_max_up/symbol_time);
   //Assuming 2 tti for L1 processing for mu 1 & 3
   if (numerology > 0)
       num_of_tti = 2;
   *ul_budget = (num_of_tti*slot_time) - (num_sym_ul*symbol_time);
   *dl_budget = (num_of_tti*slot_time) - (num_sym_dl*symbol_time);
   *num_sym = num_sym_ul;

   printf("dl_budget: %d\n",*dl_budget);
   printf("ul_budget: %d\n",*ul_budget);
   printf("symbol_time: %f\n",symbol_time);
   printf("slot_time: %f\n",slot_time);
}

/**
 * =============================================================
 * xran_fetch_and_print_lbm_stats
 * @param[in] print_xran_lbm_stats : Flag to print xRAN level LBM counters
 * @param[in] link_status          : Pointer to store the link state the desired VF
 * @param[in] vfId                 : Eth Port Id 
 *
 * @brief
 *
 * Function to fetch the link status and print the xRAN level LBM stats if
 * indicated in the input args. Application must call this function to
 * fetch the link state maintained using IEEE 802.1 Q CFM LBM/LBR protocol
 * in xRAN.
 * =============================================================
 */
int32_t xran_fetch_and_print_lbm_stats(bool print_xran_lbm_stats, uint8_t *link_status, uint8_t vfId)
{
    xran_lbm_port_info *lbm_port_info;
    struct xran_ethdi_ctx *eth_ctx = xran_ethdi_get_ctx();

    if(vfId >= XRAN_VF_MAX){
        return XRAN_STATUS_INVALID_PARAM;
    }

    lbm_port_info = &eth_ctx->lbm_port_info[vfId]; 

    if(lbm_port_info->lbm_enable)
    {
        *link_status = lbm_port_info->linkStatus;
    }
    else
    {
        return -1;
    }
    
    /* Fetch link status */

    if(print_xran_lbm_stats)
    {
        if(xran_get_syscfg_appmode() == ID_O_DU)
        {
            if(print_xran_lbm_stats)
            { /* Fetch link status and print lbm stats */
                
                if(vfId == 0)
                {
                    printf("\n%10s %10s %15s %15s %15s %15s\n","ETH PORT","LINK STATE","LBMTransmitted","InOrderLBR","OutOfOrderLBR","IgnoredRxLBRs");
                }
                
                printf("\n%10hhu %10hhu %15lu %15lu %15u %15u",vfId, lbm_port_info->linkStatus, lbm_port_info->stats.numTransmittedLBM, lbm_port_info->stats.rxValidInOrderLBRs, lbm_port_info->stats.rxValidOutOfOrderLBRs, lbm_port_info->stats.numRxLBRsIgnored);
                printf("\n");    
            }
        }
        else
        {
            if(vfId == 0)
            {
                printf("\n%10s %15s %15s %15s\n","ETH PORT","LBRTransmitted","InOrderLBM","OutOfOrderLBM");
            }

            printf("\n%10hhu %15lu %15lu %15u\n",vfId, lbm_port_info->stats.numTransmittedLBM, lbm_port_info->stats.rxValidInOrderLBRs, lbm_port_info->stats.rxValidOutOfOrderLBRs);

        }
    }

    return 0;
}

#ifdef POLL_EBBU_OFFLOAD
PXRAN_TIMER_CTX xran_timer_get_ctx_ebbu_offload(void)
{
    return &gXranTmrCtx;
}

long xran_timer_get_interval_ebbu_offload(void)
{
    return (interval_us*1000L/N_SYM_PER_SLOT);
}

inline int32_t timing_adjust_gps_second_ebbu_offload(struct timespec* p_time)
{
    struct xran_system_config *p_sysCfg = xran_get_systemcfg();

    if(unlikely(p_sysCfg->offset_sec || p_sysCfg->offset_nsec))
    {
        if (p_time->tv_nsec >= p_sysCfg->offset_nsec)
        {
            p_time->tv_nsec -= p_sysCfg->offset_nsec;
            p_time->tv_sec -= p_sysCfg->offset_sec;
        }
        else
        {
            p_time->tv_nsec += 1e9 - p_sysCfg->offset_nsec;
            p_time->tv_sec -= p_sysCfg->offset_sec + 1;
        }
    }
    return 0;
}

inline int32_t xran_sym_poll_callback_task_ebbu_offload(void)
{
    struct xran_device_ctx* p_dev_ctx_run = NULL;
    struct xran_common_counters* pCnt = NULL;
    int32_t xran_port_id, i, mu;
    uint64_t tUsed;
    PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();

    for(xran_port_id =  0; xran_port_id < XRAN_PORTS_NUM; xran_port_id++ )
    {
        if(!xran_isactive_ru_byid(xran_port_id))
            continue;
        p_dev_ctx_run = xran_dev_get_ctx_by_id(xran_port_id);
        if(p_dev_ctx_run) {
            if(p_dev_ctx_run->xran_port_id == xran_port_id) {
                for(i=0;i<p_dev_ctx_run->fh_cfg.numMUs;i++)
                {
                    mu = p_dev_ctx_run->fh_cfg.mu_number[i];
                    if(XranGetSymNum(pCtx->ebbu_offload_ota_sym_cnt_mu[mu], XRAN_NUM_OF_SYMBOL_PER_SLOT) == pCtx->ebbu_offload_ota_sym_idx_mu[mu])
                    {
                        sym_ota_cb_ebbu_offload(p_dev_ctx_run, &tUsed, mu);
                    }
                    pCnt = &p_dev_ctx_run->fh_counters;
                    if (pCnt->gps_second != pCtx->current_second)
                    {
                        if ((pCtx->current_second - pCnt->gps_second) != 1)
                            print_dbg("second c %ld p %ld\n", pCtx->current_second, pCnt->gps_second);

                        pCnt->gps_second = (uint64_t)pCtx->current_second;

                        pCnt->rx_counter_pps = pCnt->rx_counter - pCnt->old_rx_counter;
                        pCnt->old_rx_counter = pCnt->rx_counter;
                        pCnt->tx_counter_pps = pCnt->tx_counter - pCnt->old_tx_counter;
                        pCnt->old_tx_counter = pCnt->tx_counter;
                        pCnt->rx_bytes_per_sec = pCnt->rx_bytes_counter;
                        pCnt->tx_bytes_per_sec = pCnt->tx_bytes_counter;
                        pCnt->rx_bytes_counter = 0;
                        pCnt->tx_bytes_counter = 0;
                        pCnt->rx_bits_per_sec = pCnt->rx_bytes_per_sec * 8 / 1000L;
                        pCnt->tx_bits_per_sec = pCnt->tx_bytes_per_sec * 8 / 1000L;
                        print_dbg("current_second %d\n", pCtx->current_second);
                    }
                }
            }
            else  {
                rte_panic("p_dev_ctx_run == xran_port_id");
            }
        }
    }
    return 0;
}

inline int32_t xran_sym_poll_task_ebbu_offload(void)
{
    int32_t ret;
    ret = xran_eth_trx_tasks(NULL);
    return ret;
}

inline int32_t xran_pkt_proc_poll_task_ebbu_offload(void)
{
    int32_t ret;
    ret = ring_processing_func(NULL);
    return ret;
}

inline int32_t xran_task_dl_cp_ebbu_offload(void *arg)
{
    tx_cp_dl_cb(NULL, arg);
    return 0;
}

inline int32_t xran_task_ul_cp_ebbu_offload(void *arg)
{
    tx_cp_ul_cb(NULL, arg);
    return 0;
}

inline int32_t xran_task_tti_ebbu_offload(void *arg)
{
    tti_to_phy_cb(NULL, arg);
    return 0;
}
#endif
