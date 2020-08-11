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

#include <rte_config.h>
#include <rte_mbuf.h>
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
#include "xran_fh_o_du.h"

#define XRAN_THREAD_DEFAULT_PRIO (98)

/* If we're not receiving packets for more then this threshold... */
//#define SLEEP_THRESHOLD (rte_get_tsc_hz() / 30)    /* = 33.3(3)ms */
/* we go to sleep for this long (usleep). Undef SLEEP_TRESHOLD to disable. */
#define SLEEP_TIME 200      /* (us) */
#define BCAST {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}

#define TX_TIMER_INTERVAL ((rte_get_timer_hz() / 1000000000L)*interval_us*1000) /* nanosec */
#define TX_RX_LOOP_TIME rte_get_timer_hz() / 1

/* CAUTION: Keep in sync with the string table below. */
enum xran_entities_id
{
    ID_O_DU,
    ID_O_RU,
    ID_BROADCAST,
    ID_MAX
};

static char *const entity_names[] = {
    "ORAN O-DU sim app",
    "ORAN O-RU sim app",
};

typedef int (*PROCESS_CB)(void * arg);

/**
 * Structure storing internal configuration of workers
 */
struct xran_worker_config {
    lcore_function_t *f;
    void *arg;
    int32_t state;
};

struct xran_ethdi_ctx
{
    struct xran_io_cfg io_cfg;
    struct rte_ether_addr entities[XRAN_VF_MAX][ID_BROADCAST + 1];

    struct rte_ring *tx_ring[XRAN_VF_MAX];
    struct rte_ring *rx_ring[XRAN_VF_MAX];
    struct rte_ring *pkt_dump_ring[XRAN_VF_MAX];
    struct rte_timer timer_autodetect;
    struct rte_timer timer_ping;
    struct rte_timer timer_sync;
    struct rte_timer timer_tx;

    struct xran_worker_config pkt_wrk_cfg[RTE_MAX_LCORE];

    unsigned pkt_stats[PKT_LAST + 1];
};

enum {
    MBUF_KEEP,
    MBUF_FREE
};

extern enum xran_if_state xran_if_current_state;

static inline struct xran_ethdi_ctx *xran_ethdi_get_ctx(void)
{
    extern struct xran_ethdi_ctx g_ethdi_ctx;

    return &g_ethdi_ctx;
}
typedef int (*xran_ethdi_handler)(struct rte_mbuf *, int sender, uint64_t rx_time);

typedef int (*ethertype_handler)(struct rte_mbuf *, uint64_t rx_time);
typedef int (*xran_ethdi_handler)(struct rte_mbuf *, int sender, uint64_t rx_time);

int xran_register_ethertype_handler(uint16_t ethertype, ethertype_handler callback);

int32_t xran_ethdi_init_dpdk_io(char *name, const struct xran_io_cfg *io_cfg,
    int *lcore_id, struct rte_ether_addr *p_o_du_addr,
    struct rte_ether_addr *p_ru_addr);

struct rte_mbuf *xran_ethdi_mbuf_alloc(void);
int32_t xran_ethdi_mbuf_send(struct rte_mbuf *mb, uint16_t ethertype, uint16_t vf_id);
int32_t xran_ethdi_mbuf_send_cp(struct rte_mbuf *mb, uint16_t ethertype, uint16_t vf_id);
int32_t xran_ethdi_filter_packet(struct rte_mbuf *pkt, uint64_t rx_time);
int32_t process_dpdk_io(void);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _ETHDI_H_ */
