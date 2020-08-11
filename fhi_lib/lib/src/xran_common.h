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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_timer.h>

#include "xran_fh_o_du.h"
#include "xran_pkt_up.h"
#include "xran_cp_api.h"

#define O_DU 0
#define O_RU 1

#define N_SC_PER_PRB 12
#define MAX_N_FULLBAND_SC 273
#define N_SYM_PER_SLOT 14
#define SUBFRAME_DURATION_US 1000
#define SLOTNUM_PER_SUBFRAME       (SUBFRAME_DURATION_US/interval_us)
#define SUBFRAMES_PER_SYSTEMFRAME  10
#define SLOTS_PER_SYSTEMFRAME (SLOTNUM_PER_SUBFRAME*SUBFRAMES_PER_SYSTEMFRAME)

/* PRACH data samples are 32 bits wide, 16bits for I and 16bits for Q. Each packet contains 839 samples for long sequence or 144*14 (max) for short sequence. The payload length is 3356 octets.*/
#define PRACH_PLAYBACK_BUFFER_BYTES (144*14*4L)

#define PRACH_SRS_BUFFER_BYTES (144*14*4L)

/**<  this is the configuration of M-plane */
#define XRAN_MAX_NUM_SECTIONS       (N_SYM_PER_SLOT* (XRAN_MAX_ANTENNA_NR*2) + XRAN_MAX_ANT_ARRAY_ELM_NR)

#define XRAN_MAX_MBUF_LEN 9600 /**< jumbo frame */
#define NSEC_PER_SEC 1000000000L
#define TIMER_RESOLUTION_CYCLES 1596*1 /* 1us */
#define XRAN_RING_SIZE  512 /*4*14*8 pow of 2 */
#define XRAN_NAME_MAX_LEN	(64)
#define XRAN_RING_NUM       (3)

#define XranDiffSymIdx(prevSymIdx, currSymIdx, numTotalSymIdx)  ((prevSymIdx > currSymIdx) ? ((currSymIdx + numTotalSymIdx) - prevSymIdx) : (currSymIdx - prevSymIdx))

#define XRAN_MLOG_VAR 0 /**< enable debug variables to mlog */

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

struct xran_prach_cp_config
{
    uint8_t    filterIdx;
    uint8_t    startSymId;
    uint16_t   startPrbc;
    uint8_t    numPrbc;
    uint8_t    numSymbol;
    uint16_t   timeOffset;
    int32_t    freqOffset;
    uint8_t    nrofPrachInSlot;
    uint8_t    occassionsInPrachSlot;
    uint8_t    x;
    uint8_t    y[XRAN_PRACH_CANDIDATE_Y];
    uint8_t    isPRACHslot[XRAN_PRACH_CANDIDATE_SLOT];
    uint8_t    eAxC_offset;  /**< starting eAxC for PRACH stream */
};

#define XRAN_MAX_POOLS_PER_SECTOR_NR 8 /**< 2x(TX_OUT, RX_IN, PRACH_IN, SRS_IN) with C-plane */

typedef struct sectorHandleInfo
{
    /**< Structure that contains the information to describe the
     * instance i.e service type, virtual function, package Id etc..*/
    uint16_t nIndex;
    uint16_t nXranPort;
    /* Unique ID of an handle shared between phy layer and library */
    /**< number of antennas supported per link*/
    uint32_t nBufferPoolIndex;
    /**< Buffer poolIndex*/
    struct rte_mempool * p_bufferPool[XRAN_MAX_POOLS_PER_SECTOR_NR];
    uint32_t bufferPoolElmSz[XRAN_MAX_POOLS_PER_SECTOR_NR];
    uint32_t bufferPoolNumElm[XRAN_MAX_POOLS_PER_SECTOR_NR];

}XranSectorHandleInfo, *PXranSectorHandleInfo;

typedef void (*XranSymCallbackFn)(struct rte_timer *tim, void* arg);

struct cb_elem_entry{
    XranSymCallbackFn pSymCallback;
    void *pSymCallbackTag;
    LIST_ENTRY(cb_elem_entry) pointers;
};

/* Callback function to send mbuf to the ring */
typedef int (*xran_ethdi_mbuf_send_fn)(struct rte_mbuf *mb, uint16_t ethertype, uint16_t vf_id);

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
    struct xran_buffer_list sBufferList;
} BbuIoBufCtrlStruct;


#define XranIncrementJob(i)                  ((i >= (XRAN_SYM_JOB_SIZE-1)) ? 0 : (i+1))

#define XRAN_MAX_PKT_BURST_PER_SYM 32
#define XRAN_MAX_PACKET_FRAG 9

#define MBUF_TABLE_SIZE  (2 * MAX(XRAN_MAX_PKT_BURST_PER_SYM, XRAN_MAX_PACKET_FRAG))

struct mbuf_table {
	uint16_t len;
	struct rte_mbuf *m_table[MBUF_TABLE_SIZE];
};

struct xran_device_ctx
{
    uint8_t sector_id;
    uint8_t xran_port_id;
    struct xran_eaxcid_config    eAxc_id_cfg;
    struct xran_fh_init          fh_init;
    struct xran_fh_config        fh_cfg;
    struct xran_prach_cp_config  PrachCPConfig;

    uint32_t enablePrach;
    uint32_t enableCP;

    int32_t DynamicSectionEna;
    int64_t offset_sec;
    int64_t offset_nsec;    //offset to GPS time calcuated based on alpha and beta

    uint32_t enableSrs;
    struct xran_srs_config srs_cfg; /** configuration of SRS */

    BbuIoBufCtrlStruct sFrontHaulTxBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    BbuIoBufCtrlStruct sFrontHaulTxPrbMapBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    BbuIoBufCtrlStruct sFrontHaulRxBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    BbuIoBufCtrlStruct sFrontHaulRxPrbMapBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    BbuIoBufCtrlStruct sFHPrachRxBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    BbuIoBufCtrlStruct sFHSrsRxBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANT_ARRAY_ELM_NR];

    /* buffers lists */
    struct xran_flat_buffer sFrontHaulTxBuffers[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_NUM_OF_SYMBOL_PER_SLOT];
    struct xran_flat_buffer sFrontHaulTxPrbMapBuffers[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_NUM_OF_SYMBOL_PER_SLOT];
    struct xran_flat_buffer sFrontHaulRxBuffers[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_NUM_OF_SYMBOL_PER_SLOT];
    struct xran_flat_buffer sFrontHaulRxPrbMapBuffers[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_NUM_OF_SYMBOL_PER_SLOT];
    struct xran_flat_buffer sFHPrachRxBuffers[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_NUM_OF_SYMBOL_PER_SLOT];

    struct xran_flat_buffer sFHSrsRxBuffers[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANT_ARRAY_ELM_NR][XRAN_MAX_NUM_OF_SRS_SYMBOL_PER_SLOT];

    xran_transport_callback_fn pCallback[XRAN_MAX_SECTOR_NR];
    void *pCallbackTag[XRAN_MAX_SECTOR_NR];

    xran_transport_callback_fn pPrachCallback[XRAN_MAX_SECTOR_NR];
    void *pPrachCallbackTag[XRAN_MAX_SECTOR_NR];

    xran_transport_callback_fn pSrsCallback[XRAN_MAX_SECTOR_NR];
    void *pSrsCallbackTag[XRAN_MAX_SECTOR_NR];

    LIST_HEAD(sym_cb_elem_list, cb_elem_entry) sym_cb_list_head[XRAN_MAX_SECTOR_NR][XRAN_NUM_OF_SYMBOL_PER_SLOT];

    int32_t sym_up; /**< when we start sym 0 of up with respect to OTA time as measured in symbols */
    int32_t sym_up_ul;

    xran_fh_tti_callback_fn ttiCb[XRAN_CB_MAX];
    void *TtiCbParam[XRAN_CB_MAX];
    uint32_t SkipTti[XRAN_CB_MAX];

    int xran2phy_mem_ready;

    int rx_packet_symb_tracker[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_NUM_OF_SYMBOL_PER_SLOT];
    int rx_packet_prach_tracker[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_NUM_OF_SYMBOL_PER_SLOT];
    int rx_packet_callback_tracker[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR];
    int rx_packet_prach_callback_tracker[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR];
    int prach_start_symbol[XRAN_MAX_SECTOR_NR];
    int prach_last_symbol[XRAN_MAX_SECTOR_NR];

    int phy_tti_cb_done;

    struct rte_mempool *direct_pool;
    struct rte_mempool *indirect_pool;
    struct mbuf_table  tx_mbufs[RTE_MAX_ETHPORTS];

    struct xran_common_counters fh_counters;

    phy_encoder_poll_fn bbdev_enc; /**< call back to poll BBDev encoder */
    phy_decoder_poll_fn bbdev_dec; /**< call back to poll BBDev decoder */

    xran_ethdi_mbuf_send_fn send_cpmbuf2ring;   /**< callback to send mbufs of C-Plane packets to the ring */
    xran_ethdi_mbuf_send_fn send_upmbuf2ring;   /**< callback to send mbufs of U-Plane packets to the ring */
    uint32_t pkt_proc_core_id; /**< core used for processing DPDK timer cb */
};

extern const xRANPrachConfigTableStruct gxranPrachDataTable_sub6_fdd[XRAN_PRACH_CONFIG_TABLE_SIZE];
extern const xRANPrachConfigTableStruct gxranPrachDataTable_sub6_tdd[XRAN_PRACH_CONFIG_TABLE_SIZE];
extern const xRANPrachConfigTableStruct gxranPrachDataTable_mmw[XRAN_PRACH_CONFIG_TABLE_SIZE];
extern const xRANPrachPreambleLRAStruct gxranPreambleforLRA[13];

int process_mbuf(struct rte_mbuf *pkt);
int process_ring(struct rte_ring *r);
int ring_processing_thread(void *args);
int packets_dump_thread(void *args);

int send_symbol_ex(enum xran_pkt_dir direction,
                uint16_t section_id,
                struct rte_mbuf *mb,
                struct rb_map *data,
                const enum xran_input_byte_order iq_buf_byte_order,
                uint8_t frame_id,
                uint8_t subframe_id,
                uint8_t slot_id,
                uint8_t symbol_no,
                int prb_start,
                int prb_num,
                uint8_t CC_ID,
                uint8_t RU_Port_ID,
                uint8_t seq_id);

int32_t prepare_symbol_ex(enum xran_pkt_dir direction,
                uint16_t section_id,
                struct rte_mbuf *mb,
                struct rb_map *data,
                uint8_t     compMeth,
                uint8_t     iqWidth,
                const enum xran_input_byte_order iq_buf_byte_order,
                uint8_t frame_id,
                uint8_t subframe_id,
                uint8_t slot_id,
                uint8_t symbol_no,
                int prb_start,
                int prb_num,
                uint8_t CC_ID,
                uint8_t RU_Port_ID,
                uint8_t seq_id,
                uint32_t do_copy);

int send_cpmsg(void *pHandle, struct rte_mbuf *mbuf,struct xran_cp_gen_params *params,
                struct xran_section_gen_info *sect_geninfo, uint8_t cc_id, uint8_t ru_port_id, uint8_t seq_id);

int32_t generate_cpmsg_dlul(void *pHandle, struct xran_cp_gen_params *params, struct xran_section_gen_info *sect_geninfo, struct rte_mbuf *mbuf,
    enum xran_pkt_dir dir, uint8_t frame_id, uint8_t subframe_id, uint8_t slot_id,
    uint8_t startsym, uint8_t numsym, uint16_t prb_start, uint16_t prb_num,int16_t iq_buffer_offset, int16_t iq_buffer_len,
    uint16_t beam_id, uint8_t cc_id, uint8_t ru_port_id, uint8_t comp_method, uint8_t iqWidth,  uint8_t seq_id, uint8_t symInc);

int generate_cpmsg_prach(void *pHandle, struct xran_cp_gen_params *params, struct xran_section_gen_info *sect_geninfo, struct rte_mbuf *mbuf, struct xran_device_ctx *pxran_lib_ctx,
                uint8_t frame_id, uint8_t subframe_id, uint8_t slot_id,
                uint16_t beam_id, uint8_t cc_id, uint8_t prach_port_id, uint8_t seq_id);

struct xran_eaxcid_config *xran_get_conf_eAxC(void *pHandle);
uint8_t xran_get_conf_prach_scs(void *pHandle);
uint8_t xran_get_conf_fftsize(void *pHandle);
uint8_t xran_get_conf_numerology(void *pHandle);
uint8_t xran_get_conf_iqwidth(void *pHandle);
uint8_t xran_get_conf_compmethod(void *pHandle);
uint8_t xran_get_conf_num_bfweights(void *pHandle);

uint8_t xran_get_num_cc(void *pHandle);
uint8_t xran_get_num_eAxc(void *pHandle);
uint8_t xran_get_num_eAxcUl(void *pHandle);
uint8_t xran_get_num_ant_elm(void *pHandle);
enum xran_category xran_get_ru_category(void *pHandle);

struct xran_device_ctx *xran_dev_get_ctx(void);

int xran_register_cb_mbuf2ring(xran_ethdi_mbuf_send_fn mbuf_send_cp, xran_ethdi_mbuf_send_fn mbuf_send_up);

uint16_t xran_alloc_sectionid(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id);
uint8_t xran_get_seqid(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id);
int32_t ring_processing_func(void);
int xran_init_prach(struct xran_fh_config* pConf, struct xran_device_ctx * p_xran_dev_ctx);
void xran_updateSfnSecStart(void);
uint32_t xran_slotid_convert(uint16_t slot_id, uint16_t dir);
struct cb_elem_entry *xran_create_cb(XranSymCallbackFn cb_fn, void *cb_data);
int xran_destroy_cb(struct cb_elem_entry * cb_elm);

uint16_t xran_map_ecpriRtcid_to_vf(int32_t dir, int32_t cc_id, int32_t ru_port_id);
uint16_t xran_map_ecpriPcid_to_vf(int32_t dir, int32_t cc_id, int32_t ru_port_id);

#ifdef __cplusplus
}
#endif

#endif

