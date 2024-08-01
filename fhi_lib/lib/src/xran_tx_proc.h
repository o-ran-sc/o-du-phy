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
 * @brief XRAN TX header file
 * @file xran_tx_proc.h
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#ifndef _XRAN_TX_PROC_H_
#define _XRAN_TX_PROC_H_

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
#include "xran_prach_cfg.h"
#include "xran_up_api.h"
#include "xran_cp_api.h"

struct tx_sym_desc {
    struct rte_mbuf * mb;
    void *arg;
    uint8_t mu;
    uint8_t res1;
    uint8_t res2;
    uint8_t res3;
};

struct cp_up_tx_desc {
    struct rte_mbuf * mb;
    void *pHandle;
    uint8_t ctx_id;
    uint32_t tti;
    int32_t start_cc;
    int32_t cc_num;
    int32_t start_ant;
    int32_t ant_num;
    uint32_t frame_id;
    uint32_t subframe_id;
    uint32_t slot_id;
    uint32_t sym_id;
    uint32_t compType;
    uint32_t direction;
    uint16_t xran_port_id;
    void     *p_sec_db;
    uint8_t mu;
    uint32_t tti_for_ring;
    uint32_t sym_id_for_ring;
};

int32_t xran_process_tx_sym(void *arg, uint8_t mu);
int32_t xran_process_tx_sym_oru(void *arg, uint8_t mu);

struct cp_up_tx_desc * xran_pkt_gen_desc_alloc(void);
int32_t xran_pkt_gen_desc_free(struct cp_up_tx_desc *p_desc);

int32_t xran_tx_sym_cp_off(void* arg, uint8_t mu);
struct tx_sym_desc * xran_tx_sym_gen_desc_alloc(void);
int32_t xran_tx_sym_gen_desc_free(struct tx_sym_desc *p_desc);

uint16_t xran_getSfnSecStart(void);

int32_t xran_process_tx_sym_cp_on_dispatch_opt(void* pHandle, uint8_t ctx_id, uint32_t tti, int32_t start_cc, int32_t num_cc, int32_t start_ant, int32_t num_ant, uint32_t frame_id,
    uint32_t subframe_id, uint32_t slot_id, uint32_t sym_id, enum xran_comp_hdr_type compType, enum xran_pkt_dir direction,
    uint16_t xran_port_id, PSECTION_DB_TYPE p_sec_db, uint8_t mu, uint32_t tti_for_ring, uint32_t sym_id_for_ring, bool isVmu);

int32_t xran_enqueue_timing_info_for_tx(void* pHandle, uint8_t ctx_id, uint32_t tti, int32_t start_cc, int32_t num_cc, int32_t start_ant, int32_t num_ant, uint32_t frame_id,
    uint32_t subframe_id, uint32_t slot_id, uint32_t sym_id, enum xran_comp_hdr_type compType, enum xran_pkt_dir direction,
    uint16_t xran_port_id, PSECTION_DB_TYPE p_sec_db, uint8_t mu, uint32_t tti_for_ring, uint32_t sym_id_for_ring);

int32_t xran_process_tx_sym_cp_on_opt(void* pHandle, uint8_t ctx_id, uint32_t tti, int32_t start_cc, int32_t num_cc, int32_t start_ant,  int32_t num_ant, uint32_t frame_id,
    uint32_t subframe_id, uint32_t slot_id, uint32_t sym_id, enum xran_comp_hdr_type compType, enum xran_pkt_dir direction,
    uint16_t xran_port_id, PSECTION_DB_TYPE p_sec_db, uint8_t mu, uint32_t tti_for_ring, uint32_t sym_id_for_ring, bool isVmu);

int32_t xran_process_tx_sym_cp_on_ring(void* pHandle, uint8_t ctx_id, uint32_t tti, int32_t start_cc, int32_t num_cc, int32_t start_ant,  int32_t num_ant, uint32_t frame_id,
    uint32_t subframe_id, uint32_t slot_id, uint32_t sym_id, enum xran_comp_hdr_type compType, enum xran_pkt_dir direction,
    uint16_t xran_port_id, PSECTION_DB_TYPE p_sec_db, uint8_t mu, uint32_t tti_for_ring, uint32_t sym_id_for_ring, bool isVmu);

int32_t do_nothing(void* pHandle, uint8_t ctx_id, uint32_t tti, int32_t start_cc, int32_t num_cc, int32_t start_ant,  int32_t num_ant, uint32_t frame_id,
    uint32_t subframe_id, uint32_t slot_id, uint32_t sym_id, enum xran_comp_hdr_type compType, enum xran_pkt_dir direction,
    uint16_t xran_port_id, PSECTION_DB_TYPE p_sec_db, uint8_t mu, uint32_t tti_for_ring, uint32_t sym_id_for_ring, bool isVmu);

int32_t xran_process_tx_sym_cp_on_ring_opt(void* pHandle, int32_t start_cc, int32_t num_cc, int32_t start_ant,  int32_t num_ant, 
                                   enum xran_pkt_dir direction, uint8_t mu, uint32_t tti_for_ring, uint8_t sym_id);

int32_t xran_process_tx_sym_cp_off(void *pHandle, uint8_t ctx_id, uint32_t tti, int32_t cc_id,
    int32_t ant_id, uint32_t frame_id, uint32_t subframe_id, uint32_t slot_id, uint32_t sym_id,
    int32_t do_srs, uint8_t mu, uint32_t tti_for_ring, uint32_t sym_id_for_ring);

int32_t xran_process_tx_csirs_cp_on(void* pHandle, uint8_t ctx_id, uint32_t tti, int32_t start_cc, int32_t num_cc, int32_t start_ant,  int32_t num_ant, uint32_t frame_id,
    uint32_t subframe_id, uint32_t slot_id, uint32_t sym_id, enum xran_comp_hdr_type compType, enum xran_pkt_dir direction,
    uint16_t xran_port_id, PSECTION_DB_TYPE p_sec_db, uint8_t mu, uint32_t tti_for_ring, uint32_t sym_id_for_ring);

extern int rte_eth_macaddr_get(uint16_t port_id, struct rte_ether_addr* mac_addr);

extern PSECTION_DB_TYPE p_sectiondb[XRAN_PORTS_NUM][XRAN_MAX_NUM_MU];

#ifdef __cplusplus
}
#endif

#endif /* _XRAN_TX_PROC_H_ */
