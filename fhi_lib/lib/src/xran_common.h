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
#include <rte_log.h>

#include "xran_fh_o_du.h"
#include "xran_pkt_up.h"
#include "xran_cp_api.h"
#include "xran_dev.h"
#include "xran_pkt.h"


extern uint64_t interval_us;
#define O_DU 0
#define O_RU 1

#define N_SC_PER_PRB 12
#define MAX_N_FULLBAND_SC 273
#define N_SYM_PER_SLOT 14
#define SUBFRAME_DURATION_US 1000
#define SLOTNUM_PER_SUBFRAME(interval)       (SUBFRAME_DURATION_US/(interval))
#define SUBFRAMES_PER_SYSTEMFRAME  10
#define SLOTS_PER_SYSTEMFRAME(interval) ((SLOTNUM_PER_SUBFRAME(interval))*SUBFRAMES_PER_SYSTEMFRAME)

/* PRACH data samples are 32 bits wide, 16bits for I and 16bits for Q. Each packet contains 839 samples for long sequence or 144 for short sequence. The payload length is 840*16*2/8 octets.*/
#ifdef FCN_1_2_6_EARLIER
#define PRACH_PLAYBACK_BUFFER_BYTES (144*4L)
#else
#define PRACH_PLAYBACK_BUFFER_BYTES (840*4L)
#endif
#define PRACH_SRS_BUFFER_BYTES (144*14*4L)

/**<  this is the configuration of M-plane */
#define XRAN_MAX_NUM_SECTIONS       (N_SYM_PER_SLOT* (XRAN_MAX_ANTENNA_NR*2) + XRAN_MAX_ANT_ARRAY_ELM_NR)

#define    XRAN_PAYLOAD_1_RB_SZ(iqWidth) (((iqWidth == 0) || (iqWidth == 16)) ? \
                (N_SC_PER_PRB*(MAX_IQ_BIT_WIDTH/8)*2) : (3 * iqWidth + 1))

#define XRAN_MAX_MBUF_LEN (13168 + XRAN_MAX_SECTIONS_PER_SYM* (RTE_PKTMBUF_HEADROOM + sizeof(struct rte_ether_hdr) + sizeof(struct xran_ecpri_hdr) + sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr)))
#define NSEC_PER_SEC 1000000000L
#define TIMER_RESOLUTION_CYCLES 1596*1 /* 1us */
#define XRAN_RING_SIZE  512 /*4*14*8 pow of 2 */
#define XRAN_NAME_MAX_LEN	(64)
#define XRAN_RING_NUM       (3)

#define XranDiffSymIdx(prevSymIdx, currSymIdx, numTotalSymIdx)  ((prevSymIdx > currSymIdx) ? ((currSymIdx + numTotalSymIdx) - prevSymIdx) : (currSymIdx - prevSymIdx))

#define XRAN_MLOG_VAR 0 /**< enable debug variables to mlog */


#define DIV_ROUND_OFFSET(X,Y) ( X/Y + ((X%Y)?1:0) )

#define MAX_NUM_OF_XRAN_CTX          (2)
#define XranIncrementCtx(ctx)                             ((ctx >= (MAX_NUM_OF_XRAN_CTX-1)) ? 0 : (ctx+1))
#define XranDecrementCtx(ctx)                             ((ctx == 0) ? (MAX_NUM_OF_XRAN_CTX-1) : (ctx-1))

#define MAX_NUM_OF_DPDK_TIMERS       (10)
#define DpdkTimerIncrementCtx(ctx)           ((ctx >= (MAX_NUM_OF_DPDK_TIMERS-1)) ? 0 : (ctx+1))
#define DpdkTimerDecrementCtx(ctx)           ((ctx == 0) ? (MAX_NUM_OF_DPDK_TIMERS-1) : (ctx-1))

/* Callback function to send mbuf to the ring */
typedef int (*xran_ethdi_mbuf_send_fn)(struct rte_mbuf *mb, uint16_t ethertype, uint16_t vf_id);


#define XranIncrementJob(i)                  ((i >= (XRAN_SYM_JOB_SIZE-1)) ? 0 : (i+1))

/** Worker task function type */
typedef int32_t (*worker_task_fn)(void*);

/** worker thread context structure */
struct xran_worker_th_ctx {
    pthread_t      *pThread;

    struct sched_param sched_param;
    char      worker_name[32];
    int32_t   worker_id;
    uint64_t  worker_core_id;
    int32_t   worker_policy;
    int32_t   worker_status;

    /* task */
    worker_task_fn task_func;
    void* task_arg;
};

struct xran_sectioninfo_db {
    uint32_t    cur_index;  /**< Current index to store for this eAXC */
    struct xran_section_info list[XRAN_MAX_NUM_SECTIONS]; /**< The array of section information */
};

int32_t xran_generic_worker_thread(void *args);

int process_mbuf(struct rte_mbuf *pkt, void* handle, struct xran_eaxc_info *p_cid);
int process_mbuf_batch(struct rte_mbuf* pkt[], void* handle, int16_t num, struct xran_eaxc_info *p_cid, uint32_t* ret);
int process_ring(struct rte_ring *r,  uint16_t ring_id, uint16_t q_id);
int ring_processing_thread(void *args);
int packets_dump_thread(void *args);
// Support for 1-way eCPRI delay measurement per section 3.2.4.6 of eCPRI Specification V2.0
int32_t xran_ecpri_port_update_required(struct xran_io_cfg * cfg, uint16_t port_id);
int xran_ecpri_one_way_delay_measurement_transmitter(uint16_t port_id, void* handle);
int xran_generate_delay_meas(uint16_t port_id, void* handle, uint8_t actionType, uint8_t MeasurementID );
int process_delay_meas(struct rte_mbuf *pkt,  void* handle, uint16_t port_id);
int xran_process_delmeas_request(struct rte_mbuf *pkt, void* handle, struct xran_ecpri_del_meas_pkt*, uint16_t port_id);
int xran_process_delmeas_request_w_fup(struct rte_mbuf *pkt, void* handle, struct xran_ecpri_del_meas_pkt*, uint16_t port_id);
int xran_process_delmeas_response(struct rte_mbuf *pkt, void* handle, struct xran_ecpri_del_meas_pkt*, uint16_t port_id);
int xran_process_delmeas_rem_request(struct rte_mbuf *pkt, void* handle, struct xran_ecpri_del_meas_pkt*, uint16_t port_id);
int xran_process_delmeas_rem_request_w_fup(struct rte_mbuf *pkt, void* handle, struct xran_ecpri_del_meas_pkt*, uint16_t port_id);
int xran_process_delmeas_follow_up(struct rte_mbuf *pkt, void* handle, struct xran_ecpri_del_meas_pkt*, uint16_t port_id);
void xran_initialize_ecpri_del_meas_port(struct xran_ecpri_del_meas_cmn* pCmn, struct xran_ecpri_del_meas_port* pPort,uint16_t full_init);

int send_symbol_ex(void* handle,
                enum xran_pkt_dir direction,
                uint16_t section_id,
                struct rte_mbuf *mb,
                uint8_t *data,
                uint8_t compMeth,
                uint8_t iqWidth,
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
                uint8_t *data,
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
                uint32_t do_copy,
                enum xran_comp_hdr_type staticEn);
inline int32_t prepare_sf_slot_sym (enum xran_pkt_dir direction,
                uint8_t frame_id,
                uint8_t subframe_id,
                uint8_t slot_id,
                uint8_t symbol_no,
                struct xran_up_pkt_gen_params *xp);

static inline int32_t prepare_symbol_opt(enum xran_pkt_dir direction,
                uint16_t section_id,
                struct rte_mbuf *mb,
                struct rb_map *data,
                uint8_t     compMeth,
                uint8_t     iqWidth,
                const enum xran_input_byte_order iq_buf_byte_order,
                int prb_start,
                int prb_num,
                uint8_t CC_ID,
                uint8_t RU_Port_ID,
                uint8_t seq_id,
                uint32_t do_copy,
                struct xran_up_pkt_gen_params *xp,
                enum xran_comp_hdr_type staticEn);


int send_cpmsg(void *pHandle, struct rte_mbuf *mbuf,struct xran_cp_gen_params *params,
                struct xran_section_gen_info *sect_geninfo, uint8_t cc_id, uint8_t ru_port_id, uint8_t seq_id);

int32_t generate_cpmsg_dlul(void *pHandle, struct xran_cp_gen_params *params, struct xran_section_gen_info *sect_geninfo, struct rte_mbuf *mbuf,
    enum xran_pkt_dir dir, uint8_t frame_id, uint8_t subframe_id, uint8_t slot_id,
    uint8_t startsym, uint8_t numsym, uint16_t prb_start, uint16_t prb_num,int16_t iq_buffer_offset, int16_t iq_buffer_len,
    uint16_t beam_id, uint8_t cc_id, uint8_t ru_port_id, uint8_t comp_method, uint8_t iqWidth,  uint8_t seq_id, uint8_t symInc);

int generate_cpmsg_prach(void *pHandle, struct xran_cp_gen_params *params, struct xran_section_gen_info *sect_geninfo, struct rte_mbuf *mbuf, struct xran_device_ctx *pxran_lib_ctx,
                uint8_t frame_id, uint8_t subframe_id, uint8_t slot_id,
                uint16_t beam_id, uint8_t cc_id, uint8_t prach_port_id, uint16_t occasionid, uint8_t seq_id);

struct xran_eaxcid_config *xran_get_conf_eAxC(void *pHandle);
int xran_register_cb_mbuf2ring(xran_ethdi_mbuf_send_fn mbuf_send_cp, xran_ethdi_mbuf_send_fn mbuf_send_up);

uint16_t xran_alloc_sectionid(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id);
uint8_t xran_get_seqid(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id);
int32_t ring_processing_func(void* arg);
int xran_init_prach(struct xran_fh_config* pConf, struct xran_device_ctx * p_xran_dev_ctx);
void xran_updateSfnSecStart(void);
uint32_t xran_slotid_convert(uint16_t slot_id, uint16_t dir);

uint16_t xran_map_ecpriRtcid_to_vf(struct xran_device_ctx *p_dev_ctx,  int32_t dir, int32_t cc_id, int32_t ru_port_id);
uint16_t xran_map_ecpriPcid_to_vf(struct xran_device_ctx *p_dev_ctx, int32_t dir, int32_t cc_id, int32_t ru_port_id);

#ifdef __cplusplus
}
#endif

#endif

