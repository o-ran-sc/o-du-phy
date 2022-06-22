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
 * @brief This file has all definitions for the Ethernet Data Interface Layer
 * @file ethdi.h
 * @ingroup group_lte_source_auxlib
 * @author Intel Corporation
 **/


#ifndef _ETHDI_H_
#define _ETHDI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>

#include <rte_config.h>
#include <rte_mbuf.h>
#include <rte_flow.h>
#include <rte_timer.h>

/*  comment this to enable PDUMP
 *  DPDK has to be compiled with
 *      CONFIG_RTE_LIBRTE_PMD_PCAP=y
 *      CONFIG_RTE_LIBRTE_PDUMP=y
 */
#undef RTE_LIBRTE_PDUMP

#ifdef RTE_LIBRTE_PDUMP
#include <rte_pdump.h>
#endif

#include "ethernet.h"
#include "xran_transport.h"
#include "xran_fh_o_du.h"

#define XRAN_THREAD_DEFAULT_PRIO (98)
#define XRAN_MAX_WORKERS 6 /**< max number of worker cores */

#define TX_TIMER_INTERVAL ((rte_get_timer_hz() / 1000000000L)*interval_us*1000) /* nanosec */
#define TX_RX_LOOP_TIME (rte_get_timer_hz() / 1)

typedef uint8_t  lcoreid_t;
typedef uint16_t portid_t;
typedef uint16_t queueid_t;
typedef uint16_t streamid_t;

/* CAUTION: Keep in sync with the string table below. */
enum xran_entities_id
{
    ID_O_DU,
    ID_O_RU,
    ID_MAX
};

static char *const entity_names[] = {
    (char *)"ORAN O-DU sim app",
    (char *)"ORAN O-RU sim app"
};

typedef int (*PROCESS_CB)(void * arg);

extern queueid_t nb_rxq; /**< Number of RX queues per port. */
extern queueid_t nb_txq; /**< Number of TX queues per port. */

/**
 * Structure storing internal configuration of workers
 */
struct xran_worker_config {
    lcore_function_t *f;
    void *arg;
    int32_t state;
};

struct xran_ethdi_ctx {
    struct xran_io_cfg io_cfg;
    struct rte_ether_addr entities[XRAN_VF_MAX][ID_MAX];
    uint16_t vf2xran_port[XRAN_VF_MAX];
    uint16_t vf_and_q2pc_id[XRAN_VF_MAX][XRAN_VF_QUEUE_MAX];
    struct xran_eaxc_info vf_and_q2cid[XRAN_VF_MAX][XRAN_VF_QUEUE_MAX];
    uint16_t rxq_per_port[XRAN_VF_MAX];

    struct rte_ring *tx_ring[XRAN_VF_MAX];
    struct rte_ring *rx_ring[XRAN_VF_MAX][XRAN_VF_QUEUE_MAX];

    struct rte_ring *up_dl_pkt_gen_ring[XRAN_PORTS_NUM];

    struct xran_worker_config time_wrk_cfg; /**< core doing polling of time */
    struct xran_worker_config pkt_wrk_cfg[RTE_MAX_LCORE]; /**< worker cores */

    phy_encoder_poll_fn bbdev_enc; /**< call back to poll BBDev encoder */
    phy_decoder_poll_fn bbdev_dec; /**< call back to poll BBDev decoder */

    uint32_t pkt_proc_core_id; /**< core used for processing DPDK timer cb */
    uint32_t num_workers; /**< number of workers */
    uint32_t worker_core[XRAN_MAX_WORKERS]; /**< id of core used as worker */

    uint64_t rx_vf_queue_cnt[XRAN_VF_MAX][XRAN_VF_QUEUE_MAX];
};

enum xran_mbuf_mem_op_id {
    MBUF_KEEP,
    MBUF_FREE
};

extern enum xran_if_state xran_if_current_state;

static inline struct xran_ethdi_ctx *xran_ethdi_get_ctx(void)
{
    extern struct xran_ethdi_ctx g_ethdi_ctx;
    return &g_ethdi_ctx;
}
typedef int (*xran_ethdi_handler)(struct rte_mbuf *, int sender, uint16_t vf_id);

typedef int (*ethertype_handler)(struct rte_mbuf* pkt_q[], uint16_t xport_id,  struct xran_eaxc_info *p_cid, uint16_t num);


int32_t xran_register_ethertype_handler(uint16_t ethertype, ethertype_handler callback);
int32_t xran_ethdi_init_dpdk_io(char *name, const struct xran_io_cfg *io_cfg,
                                int32_t *lcore_id, struct rte_ether_addr *p_o_du_addr,
                                struct rte_ether_addr *p_ru_addr, uint32_t mtu);

struct rte_mbuf *xran_ethdi_mbuf_alloc(void);
struct rte_mbuf *xran_ethdi_mbuf_indir_alloc(void);
int32_t xran_ethdi_mbuf_send(struct rte_mbuf *mb, uint16_t ethertype, uint16_t vf_id);
int32_t xran_ethdi_mbuf_send_cp(struct rte_mbuf *mb, uint16_t ethertype, uint16_t vf_id);
int32_t xran_ethdi_filter_packet(struct rte_mbuf *pkt[], uint16_t vf_id, uint16_t q_id, uint16_t num);
int32_t process_dpdk_io(void* args);
int32_t process_dpdk_io_tx(void* args);
int32_t process_dpdk_io_rx(void* args);

struct rte_flow * generate_ecpri_flow(uint16_t port_id, uint16_t rx_q, uint16_t pc_id_be, struct rte_flow_error *error);


#ifdef __cplusplus
}
#endif

#endif /* #ifndef _ETHDI_H_ */
