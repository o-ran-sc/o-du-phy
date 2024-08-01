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
#define SLOTNUM_PER_SUBFRAME(interval)       (SUBFRAME_DURATION_US/(interval))
#define SLOTNUM_PER_SUBFRAME_MU(mu) (1 << mu)
#define SUBFRAMES_PER_SYSTEMFRAME  10
#define SLOTS_PER_SYSTEMFRAME(interval) ((SLOTNUM_PER_SUBFRAME(interval))*SUBFRAMES_PER_SYSTEMFRAME)

/* PRACH data samples are 32 bits wide, 16bits for I and 16bits for Q. Each packet contains 839 samples for long sequence or 144 for short sequence. The payload length is 840*16*2/8 octets.*/
#define PRACH_PLAYBACK_BUFFER_BYTES (840*4L)
#define PRACH_SRS_BUFFER_BYTES (144*14*4L)

/**<  this is the configuration of M-plane */
#define XRAN_MAX_NUM_SECTIONS       (32) //(N_SYM_PER_SLOT* (XRAN_MAX_ANTENNA_NR*2) + XRAN_MAX_ANT_ARRAY_ELM_NR)

#define XRAN_MAX_MBUF_LEN (13168 + XRAN_MAX_SECTIONS_PER_SYM* (RTE_PKTMBUF_HEADROOM + sizeof(struct rte_ether_hdr) + sizeof(struct xran_ecpri_hdr) + sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr)))
#define NSEC_PER_SEC 1000000000L
#define XRAN_RING_SIZE  512 /*4*14*8 pow of 2 */
#define XRAN_NAME_MAX_LEN	(64)
#define XRAN_RING_NUM       (3)
#define SUBCARRIERS_PER_PRB (12)

#define XranDiffSymIdx(prevSymIdx, currSymIdx, numTotalSymIdx)  ((prevSymIdx > currSymIdx) ? ((currSymIdx + numTotalSymIdx) - prevSymIdx) : (currSymIdx - prevSymIdx))

#define XRAN_MLOG_VAR 0 /**< enable debug variables to mlog */

#define DIV_ROUND_OFFSET(X,Y) ( X/Y + ((X%Y)?1:0) )

/* We are reserving a range of section-ids for each numerology for
 * sect-id to mu mapping in case of mixed numerology mode.
 *
 * mu0: 0-272
 * mu1: 273-545
 * mu2: 546-818
 * mu3: 819-1091
 * mu4: 1091-1364
 */
#define XRAN_BASE_SECT_ID_TO_MU_SECT_ID(base, mu) (base + (XRAN_MAX_SECTIONS_PER_SLOT * mu))
#define XRAN_MU_SECT_ID_TO_BASE_SECT_ID(sectId) (sectId % XRAN_MAX_SECTIONS_PER_SLOT)
#define XRAN_GET_MU_FROM_SECT_ID(sectId) (sectId/XRAN_MAX_SECTIONS_PER_SLOT)
#define XRAN_USEC_TO_NUM_SYM(slot_interval, duration) ((duration*1000/(slot_interval*1000/XRAN_NUM_OF_SYMBOL_PER_SLOT)) + 1)

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

struct xran_worker_info_s {
    lcore_function_t *TaskFunc;
    void *TaskArg;
    uint32_t WorkerId;
    char ThreadName[32];
    int32_t State;
};

struct xran_sectioninfo_db {
    uint32_t    cur_index;  /**< Current index to store for this eAXC */
    struct xran_section_info list[XRAN_MAX_NUM_SECTIONS]; /**< The array of section information */
};

int32_t xran_generic_worker_thread(void *args);
int32_t xran_validate_sectionId(void *arg, uint16_t mu);
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

int send_symbol_mult_section_ex(void* handle,
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
                void *pRing,
                uint16_t vf_id,
                uint8_t CC_ID,
                uint8_t RU_Port_ID,
                uint8_t seq_id, uint8_t mu);

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
                void *pRing,
                uint16_t vf_id,
                uint8_t CC_ID,
                uint8_t RU_Port_ID,
                uint8_t seq_id,
                uint8_t mu);

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
                enum xran_comp_hdr_type staticEn,
                uint16_t num_sections,
                uint16_t iq_buffer_offset,
                uint8_t mu,
                bool isNb375,
                uint8_t oxu_port_id);

int32_t prepare_symbol_ex_appand(enum xran_pkt_dir direction,
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
                enum xran_comp_hdr_type staticEn,
                uint16_t num_sections,
                uint16_t iq_buffer_offset,
                uint8_t mu,
                bool isNb375,
                uint8_t oxu_port_id);


int32_t prepare_sf_slot_sym (enum xran_pkt_dir direction,
                uint8_t frame_id,
                uint8_t subframe_id,
                uint8_t slot_id,
                uint8_t symbol_no,
                struct xran_up_pkt_gen_params *xp);

int send_cpmsg(void *pHandle, struct rte_mbuf *mbuf,struct xran_cp_gen_params *params,
                struct xran_section_gen_info *sect_geninfo, uint8_t cc_id, uint8_t ru_port_id, uint8_t seq_id, uint8_t mu);

int xran_register_cb_mbuf2ring(xran_ethdi_mbuf_send_fn mbuf_send_cp, xran_ethdi_mbuf_send_fn mbuf_send_up);

uint8_t xran_get_seqid(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id);
int32_t ring_processing_func(void* arg);
int32_t ring_processing_func2(void* arg);
int xran_init_prach(struct xran_fh_config* pConf, struct xran_device_ctx * p_xran_dev_ctx, enum xran_ran_tech xran_tech, uint8_t mu);
#ifndef POLL_EBBU_OFFLOAD
void xran_updateSfnSecStart(void);
#endif
uint32_t xran_slotid_convert(uint16_t slot_id, uint16_t dir);

uint16_t xran_map_ecpriRtcid_to_vf(struct xran_device_ctx *p_dev_ctx,  int32_t dir, int32_t cc_id, int32_t ru_port_id);
uint16_t xran_map_ecpriPcid_to_vf(struct xran_device_ctx *p_dev_ctx, int32_t dir, int32_t cc_id, int32_t ru_port_id);

bool xran_check_if_late_transmission(uint32_t ttiForRing, uint32_t symIdForRing, uint8_t mu);

int32_t xran_validate_sectionId(void *arg, uint16_t mu);
int32_t xran_validate_numprbs(uint8_t mu, uint16_t start_prbu, uint16_t num_prbs, struct xran_device_ctx *p_dev_ctx);
int32_t xran_validate_numprbs_prach(uint8_t mu, uint16_t start_prbu, uint16_t num_prbs, struct xran_device_ctx *p_dev_ctx);


inline int32_t xran_get_iqdata_len(uint16_t numRbs, uint8_t iqWidth, uint8_t compMeth)
{
    int32_t compParamLen;

    switch(compMeth)
    {
        case XRAN_COMPMETHOD_BLKFLOAT:
//        case XRAN_COMPMETHOD_BLKSCALE:
//        case XRAN_COMPMETHOD_ULAW:
            compParamLen = 1;
            break;

        case XRAN_COMPMETHOD_NONE:
        case XRAN_COMPMETHOD_MODULATION:
        default:
            compParamLen = 0;
    }

    return (numRbs * (3*iqWidth + compParamLen));   /* 3 = 12(the number of REs) * 2(I and Q pair) / 8(bitwidth) */
}

#ifdef __cplusplus
}
#endif

#endif

