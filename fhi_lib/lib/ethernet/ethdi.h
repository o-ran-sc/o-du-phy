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

/* How often to ping? */
#define PING_INTERVAL 300   /* (us) */
#define PING_BUSY_POLL 50   /* (us) how long to actively wait for response */

/* If we're not receiving packets for more then this threshold... */
//#define SLEEP_THRESHOLD (rte_get_tsc_hz() / 30)    /* = 33.3(3)ms */
/* we go to sleep for this long (usleep). Undef SLEEP_TRESHOLD to disable. */
#define SLEEP_TIME 200      /* (us) */
#define BCAST {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}

#define TX_TIMER_INTERVAL ((rte_get_timer_hz() / 1000000000L)*interval_us*1000) /* nanosec */
#define TX_RX_LOOP_TIME rte_get_timer_hz() / 1

enum xran_ping_states
{
    PING_IDLE,
    PING_NEEDED,
    AWAITING_PONG
};

enum xran_ethdi_vf_ports
{
    ETHDI_UP_VF = 0,
    ETHDI_CP_VF,
    ETHDI_VF_MAX
};

struct xran_io_loop_cfg
{
    uint8_t id;
    char *dpdk_dev[ETHDI_VF_MAX];
    char *bbdev_dev[1];
    int bbdev_mode;
    int core;
    int system_core;    /* Needed as DPDK will change your starting core. */
    int pkt_proc_core;  /* Needed for packet processing thread. */
    int pkt_aux_core;   /* Needed for packet dumping for debug purposes. */
    int timing_core;    /* Needed for getting precise time */
    int port[ETHDI_VF_MAX];           /* This is auto-detected, no need to set. */
};

/* CAUTION: Keep in sync with the string table below. */
enum xran_entities_id
{
    ID_LLS_CU,
    ID_RU,
    ID_BROADCAST,
    ID_MAX
};

static char *const entity_names[] = {
    "xRAN lls-CU sim app",
    "xRAN RU sim app",
};

typedef int (*PROCESS_CB)(void * arg);

struct xran_ethdi_ctx
{
    struct xran_io_loop_cfg io_cfg;
    struct ether_addr entities[ID_BROADCAST + 1];
    uint8_t ping_state;
    int ping_times;
    int known_peers;

    struct rte_ring *tx_ring[ETHDI_VF_MAX];
    struct rte_ring *rx_ring[ETHDI_VF_MAX];
    struct rte_ring *pkt_dump_ring[ETHDI_VF_MAX];
    struct rte_timer timer_autodetect;
    struct rte_timer timer_ping;
    struct rte_timer timer_sync;
    struct rte_timer timer_tx;

    uint64_t busy_poll_till;

    unsigned pkt_stats[PKT_LAST + 1];

    uint16_t cp_vtag;
    uint16_t up_vtag;
};

enum {
    MBUF_KEEP,
    MBUF_FREE
};

extern enum xran_if_state xran_if_current_state;
extern uint8_t ping_dst_id;
extern struct ether_addr entities_addrs[];

static inline struct xran_ethdi_ctx *xran_ethdi_get_ctx(void)
{
    extern struct xran_ethdi_ctx g_ethdi_ctx;

    return &g_ethdi_ctx;
}
typedef int (*xran_ethdi_handler)(struct rte_mbuf *, int sender, uint64_t rx_time);

typedef int (*ethertype_handler)(struct rte_mbuf *, uint64_t rx_time);
typedef int (*xran_ethdi_handler)(struct rte_mbuf *, int sender, uint64_t rx_time);
typedef void (xran_ethdi_tx_callback)(struct rte_timer *tim, void *arg);


int xran_register_ethertype_handler(uint16_t ethertype, ethertype_handler callback);


int xran_ethdi_init_dpdk_io(char *name, const struct xran_io_loop_cfg *io_cfg,
    int *lcore_id, struct ether_addr *p_lls_cu_addr, struct ether_addr *p_ru_addr,
    uint16_t cp_vlan, uint16_t up_vlan);
struct rte_mbuf *xran_ethdi_mbuf_alloc(void);
int xran_ethdi_mbuf_send(struct rte_mbuf *mb, uint16_t ethertype);
int xran_ethdi_mbuf_send_cp(struct rte_mbuf *mb, uint16_t ethertype);
#if 0
void xran_ethdi_stop_tx(void);
void xran_ethdi_ports_stats(void);
int xran_ethdi_dpdk_io_loop(void *);
#endif
int xran_ethdi_filter_packet(struct rte_mbuf *pkt, uint64_t rx_time);
int32_t process_dpdk_io(void);


#ifdef __cplusplus
}
#endif

#endif /* #ifndef _ETHDI_H_ */
