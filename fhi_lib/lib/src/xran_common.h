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
 * @brief XRAN layer common functionality for both lls-CU and RU as well as C-plane and
 *    U-plane
 * @file xran_common.h
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#ifndef _XRAN_COMMON_H_
#define _XRAN_COMMON_H_

#include <stdio.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_timer.h>

#include "xran_fh_lls_cu.h"
#include "xran_pkt_up.h"

#define APP_LLS_CU 0
#define APP_RU     1
#define NUM_OF_PRB_IN_FULL_BAND (66)
#define N_SC_PER_PRB 12
#define N_SYM_PER_SLOT 14
#define N_FULLBAND_SC (NUM_OF_PRB_IN_FULL_BAND*N_SC_PER_PRB)
#define MAX_ANT_CARRIER_SUPPORTED 16
/* 0.125, just for testing */
#define SLOTNUM_PER_SUBFRAME      8
#define SUBFRAMES_PER_SYSTEMFRAME  10
#define SLOTS_PER_SYSTEMFRAME (SLOTNUM_PER_SUBFRAME*SUBFRAMES_PER_SYSTEMFRAME)
#define PDSCH_PAYLOAD_SIZE (N_FULLBAND_SC*4)
#define NUM_OF_SLOT_IN_TDD_LOOP         (80)
#define IQ_PLAYBACK_BUFFER_BYTES (NUM_OF_SLOT_IN_TDD_LOOP*N_SYM_PER_SLOT*N_FULLBAND_SC*4L)

/* PRACH data samples are 32 bits wide, 16bits for I and 16bits for Q. Each packet contains 839 samples. The payload length is 3356 octets.*/
#define PRACH_PLAYBACK_BUFFER_BYTES (10*839*4L)

#define XRAN_MAX_NUM_SECTIONS       (NUM_OF_PRB_IN_FULL_BAND)     // TODO: need to decide proper value

#define XRAN_MAX_MBUF_LEN 9600 /**< jummbo frame */
#define NSEC_PER_SEC 1000000000
#define TIMER_RESOLUTION_CYCLES 1596*1 /* 1us */
#define XRAN_RING_SIZE  512 /*4*14*8 pow of 2 */
#define XRAN_NAME_MAX_LEN	(64)
#define XRAN_RING_NUM       (3)

#define MAX_NUM_OF_XRAN_CTX       (2)
#define XranIncrementCtx(ctx)                             ((ctx >= (MAX_NUM_OF_XRAN_CTX-1)) ? 0 : (ctx+1))
#define XranDecrementCtx(ctx)                             ((ctx == 0) ? (MAX_NUM_OF_XRAN_CTX-1) : (ctx-1))

#define XranDiffSymIdx(prevSymIdx, currSymIdx, numTotalSymIdx)  ((prevSymIdx > currSymIdx) ? ((currSymIdx + numTotalSymIdx) - prevSymIdx) : (currSymIdx - prevSymIdx))

#define XRAN_SYM_JOB_SIZE 512

struct send_symbol_cb_args
{
    struct rb_map *samp_buf;
    uint8_t *symb_id;
};

struct pkt_dump
{
    int num_samp;
    int num_bytes;
    uint8_t symb;
    struct ecpri_seq_id seq;
} __rte_packed;

/* PRACH configuration table defines */
#define XRAN_PRACH_CANDIDATE_PREAMBLE    (2)
#define XRAN_PRACH_CANDIDATE_Y           (2)
#define XRAN_PRACH_CANDIDATE_SLOT        (40)
#define XRAN_PRACH_CONFIG_TABLE_SIZE     (256)
#define XRAN_PRACH_PREAMBLE_FORMAT_OF_ABC (9)
typedef enum
{
    FORMAT_0 = 0,
    FORMAT_1,
    FORMAT_2,
    FORMAT_3,
    FORMAT_A1,
    FORMAT_A2,
    FORMAT_A3,
    FORMAT_B1,
    FORMAT_B2,
    FORMAT_B3,
    FORMAT_B4,
    FORMAT_C0,
    FORMAT_C2,
    FORMAT_LAST
}PreambleFormatEnum;

/* add PRACH used config table, same structure as used in refPHY */
typedef struct
{
    uint8_t     prachConfigIdx;
    uint8_t     preambleFmrt[XRAN_PRACH_CANDIDATE_PREAMBLE];
    uint8_t     x;
    uint8_t     y[XRAN_PRACH_CANDIDATE_Y];
    uint8_t     slotNr[XRAN_PRACH_CANDIDATE_SLOT];
    uint8_t     slotNrNum;
    uint8_t     startingSym;
    uint8_t     nrofPrachInSlot;
    uint8_t     occassionsInPrachSlot;
    uint8_t     duration;
} xRANPrachConfigTableStruct;

typedef struct
{
    uint8_t    preambleFmrt;
    uint16_t   lRALen;
    uint8_t    fRA;
    uint32_t    nu;
    uint16_t   nRaCp;
}xRANPrachPreambleLRAStruct;

typedef struct
{
    uint8_t    filterIdx;
    uint8_t    startSymId;
    uint16_t   startPrbc;
    uint8_t    numPrbc;
    uint8_t    numSymbol;
    uint16_t   timeOffset;
    int32_t    freqOffset;
    uint8_t    occassionsInPrachSlot;
    uint8_t    x;
    uint8_t    y[XRAN_PRACH_CANDIDATE_Y];
    uint8_t    isPRACHslot[XRAN_PRACH_CANDIDATE_SLOT];
}xRANPrachCPConfigStruct;


typedef struct DeviceHandleInfo
{
    /**< Structure that contains the information to describe the
     * instance i.e service type, virtual function, package Id etc..*/
    uint16_t nIndex;
    /* Unique ID of an handle shared between phy layer and library */
    /**< number of antennas supported per link*/
    uint32_t nBufferPoolIndex;
    /**< Buffer poolIndex*/
    struct rte_mempool * p_bufferPool[XRAN_MAX_SECTOR_NR];
    uint32_t bufferPoolElmSz[XRAN_MAX_SECTOR_NR];
    uint32_t bufferPoolNumElm[XRAN_MAX_SECTOR_NR];

}XranLibHandleInfoStruct;

typedef void (*XranSymCallbackFn)(struct rte_timer *tim, void* arg);

/*
 * manage one cell's all Ethernet frames for one DL or UL LTE subframe
 */
typedef struct {
    /* -1-this subframe is not used in current frame format
         0-this subframe can be transmitted, i.e., data is ready
          1-this subframe is waiting transmission, i.e., data is not ready
         10 - DL transmission missing deadline. When FE needs this subframe data but bValid is still 1,
        set bValid to 10.
    */
    int32_t bValid ; // when UL rx, it is subframe index.
    int32_t nSegToBeGen;
    int32_t nSegGenerated; // how many date segment are generated by DL LTE processing or received from FE
                       // -1 means that DL packet to be transmitted is not ready in BS
    int32_t nSegTransferred; // number of data segments has been transmitted or received
    struct rte_mbuf *pData[XRAN_N_MAX_BUFFER_SEGMENT]; // point to DPDK allocated memory pool
    XRANBufferListStruct sBufferList;
} BbuIoBufCtrlStruct;

struct xran_sym_job {
    uint32_t sym_idx;
	uint32_t status;
}__rte_cache_aligned;

#define XranIncrementJob(i)                  ((i >= (XRAN_SYM_JOB_SIZE-1)) ? 0 : (i+1))

struct xran_lib_ctx
{
    uint8_t llscu_id;
    uint8_t sector_id;
    XRANEAXCIDCONFIG eAxc_id_cfg;
    XRANFHINIT   xran_init_cfg;
    XRANFHCONFIG xran_fh_cfg;
    XranLibHandleInfoStruct* pDevHandle;
    xRANPrachCPConfigStruct PrachCPConfig;
    uint32_t enableCP;
    char ring_name[XRAN_RING_NUM][XRAN_MAX_SECTOR_NR][RTE_RING_NAMESIZE];
    struct rte_ring *dl_sym_idx_ring[XRAN_MAX_SECTOR_NR];
    struct rte_ring *xran2phy_ring[XRAN_MAX_SECTOR_NR];
    struct rte_ring *xran2prach_ring[XRAN_MAX_SECTOR_NR];

    struct xran_sym_job sym_job[XRAN_SYM_JOB_SIZE];
    uint32_t sym_job_idx;

    BbuIoBufCtrlStruct sFrontHaulTxBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    BbuIoBufCtrlStruct sFrontHaulRxBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    BbuIoBufCtrlStruct sFHPrachRxBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];

    /* buffers lists */
    XRANFlatBufferStruct sFrontHaulTxBuffers[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_NUM_OF_SYMBOL_PER_SLOT];
    XRANFlatBufferStruct sFrontHaulRxBuffers[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_NUM_OF_SYMBOL_PER_SLOT];
    XRANFlatBufferStruct sFHPrachRxBuffers[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_NUM_OF_SYMBOL_PER_SLOT];

    XranTransportBlockCallbackFn pCallback[XRAN_MAX_SECTOR_NR];
    void *pCallbackTag[XRAN_MAX_SECTOR_NR];

    XranTransportBlockCallbackFn pPrachCallback[XRAN_MAX_SECTOR_NR];
    void *pPrachCallbackTag[XRAN_MAX_SECTOR_NR];

    XranSymCallbackFn pSymCallback[XRAN_MAX_SECTOR_NR][XRAN_NUM_OF_SYMBOL_PER_SLOT];
    void *pSymCallbackTag[XRAN_MAX_SECTOR_NR][XRAN_NUM_OF_SYMBOL_PER_SLOT];

    int32_t sym_up; /**< when we start sym 0 of up with respect to OTA time as measured in symbols */
    int32_t sym_up_ul;

    XRANFHTTIPROCCB ttiCb[XRAN_CB_MAX];
    void *TtiCbParam[XRAN_CB_MAX];
    uint32_t SkipTti[XRAN_CB_MAX];

    int xran2phy_mem_ready;

    int rx_packet_symb_tracker[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_NUM_OF_SYMBOL_PER_SLOT];
    int rx_packet_callback_tracker[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR];
    int phy_tti_cb_done;
};

extern const xRANPrachConfigTableStruct gxranPrachDataTable_sub6_fdd[XRAN_PRACH_CONFIG_TABLE_SIZE];
extern const xRANPrachConfigTableStruct gxranPrachDataTable_sub6_tdd[XRAN_PRACH_CONFIG_TABLE_SIZE];
extern const xRANPrachConfigTableStruct gxranPrachDataTable_mmw[XRAN_PRACH_CONFIG_TABLE_SIZE];
extern const xRANPrachPreambleLRAStruct gxranPreambleforLRA[XRAN_PRACH_PREAMBLE_FORMAT_OF_ABC];

int process_mbuf(struct rte_mbuf *pkt);
int process_ring(struct rte_ring *r);
int ring_processing_thread(void *args);
int packets_dump_thread(void *args);

int send_symbol_ex(enum xran_pkt_dir direction,
                uint16_t section_id,
                struct rb_map *data,
                uint8_t frame_id,
                uint8_t subframe_id,
                uint8_t slot_id,
                uint8_t symbol_no,
                int prb_start,
                int prb_num,
                uint8_t CC_ID,
                uint8_t RU_Port_ID,
                uint8_t seq_id);

int send_cpmsg_dlul(void *pHandle, enum xran_pkt_dir dir,
                uint8_t frame_id, uint8_t subframe_id, uint8_t slot_id,
                uint8_t startsym, uint8_t numsym, int prb_num,
                uint16_t beam_id, uint8_t cc_id, uint8_t ru_port_id,
                uint8_t seq_id);

int send_cpmsg_prach(void *pHandle,
                uint8_t frame_id, uint8_t subframe_id, uint8_t slot_id,
                uint16_t beam_id, uint8_t cc_id, uint8_t prach_port_id,
                uint8_t seq_id);

uint8_t xran_get_max_sections(void *pHandle);

XRANEAXCIDCONFIG *xran_get_conf_eAxC(void *pHandle);
uint8_t xran_get_conf_prach_scs(void *pHandle);
uint8_t xran_get_conf_fftsize(void *pHandle);
uint8_t xran_get_conf_numerology(void *pHandle);
uint8_t xran_get_conf_iqwidth(void *pHandle);
uint8_t xran_get_conf_compmethod(void *pHandle);

uint8_t xran_get_num_cc(void *pHandle);
uint8_t xran_get_num_eAxc(void *pHandle);
uint8_t xran_get_llscuid(void *pHandle);
uint8_t xran_get_sectorid(void *pHandle);
struct xran_lib_ctx *xran_lib_get_ctx(void);

uint16_t xran_alloc_sectionid(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id);
uint8_t xran_get_seqid(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id);

#endif

