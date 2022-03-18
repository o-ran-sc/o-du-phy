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
 * @file ethernet.c
 * @ingroup group_lte_source_auxlib
 * @author Intel Corporation
 **/


#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/queue.h>
#include <err.h>
#include <assert.h>

#include <linux/limits.h>
#include <sys/types.h>
#include <stdlib.h>
#include <math.h>
#include <immintrin.h>
#include <rte_config.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_errno.h>

#include "ethernet.h"
#include "ethdi.h"

/* mbuf pools */
struct rte_mempool *_eth_mbuf_pool          = NULL;
struct rte_mempool *_eth_mbuf_pool_indirect = NULL;
struct rte_mempool *_eth_mbuf_pool_rx     = NULL;
struct rte_mempool *_eth_mbuf_pkt_gen       = NULL;

struct rte_mempool *socket_direct_pool    = NULL;
struct rte_mempool *socket_indirect_pool  = NULL;

struct rte_mempool *_eth_mbuf_pool_vf_rx[16][RTE_MAX_QUEUES_PER_PORT] = {NULL};
struct rte_mempool *_eth_mbuf_pool_vf_small[16]    = {NULL};

void
xran_init_mbuf_pool(uint32_t mtu)
{
    uint16_t data_room_size = MBUF_POOL_ELEMENT;
    printf("%s: socket %d\n",__FUNCTION__, rte_socket_id());

    if (mtu <= 1500) {
        data_room_size = MBUF_POOL_ELM_SMALL;
}

    /* Init the buffer pool */
    if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
        _eth_mbuf_pool = rte_pktmbuf_pool_create("mempool", NUM_MBUFS,
                MBUF_CACHE, 0, data_room_size, rte_socket_id());
        _eth_mbuf_pool_indirect = rte_pktmbuf_pool_create("mempool_indirect", NUM_MBUFS_VF,
                MBUF_CACHE, 0, 0, rte_socket_id());
        _eth_mbuf_pkt_gen = rte_pktmbuf_pool_create("mempool_pkt_gen",
                NUM_MBUFS, MBUF_CACHE, 0, MBUF_POOL_PKT_GEN_ELM, rte_socket_id());
    } else {
        _eth_mbuf_pool = rte_mempool_lookup("mempool");
        _eth_mbuf_pool_indirect = rte_mempool_lookup("mempool_indirect");
        _eth_mbuf_pkt_gen = rte_mempool_lookup("mempool_pkt_gen");
    }

    if (_eth_mbuf_pool == NULL)
        rte_panic("Cannot create mbuf pool: %s\n", rte_strerror(rte_errno));
    if (_eth_mbuf_pool_indirect == NULL)
        rte_panic("Cannot create mbuf pool: %s\n", rte_strerror(rte_errno));
    if (_eth_mbuf_pkt_gen == NULL)
        rte_panic("Cannot create packet gen pool: %s\n", rte_strerror(rte_errno));

    if (socket_direct_pool == NULL)
        socket_direct_pool = _eth_mbuf_pool;

    if (socket_indirect_pool == NULL)
        socket_indirect_pool = _eth_mbuf_pool_indirect;
}

/* Configure the Rx with optional split. */
int
rx_queue_setup(uint16_t port_id, uint16_t rx_queue_id,
           uint16_t nb_rx_desc, unsigned int socket_id,
           struct rte_eth_rxconf *rx_conf, struct rte_mempool *mp)
{
    unsigned int i, mp_n;
    int ret;
#ifndef RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT
#define RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT 0x00100000
#endif
    if ((rx_conf->offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT) == 0) {
#if (RTE_VER_YEAR >= 21)
        rx_conf->rx_seg = NULL;
        rx_conf->rx_nseg = 0;
#endif
        ret = rte_eth_rx_queue_setup(port_id, rx_queue_id,
                         nb_rx_desc, socket_id,
                         rx_conf, mp);
        return ret;

    } else {
        printf("rx_queue_setup error\n");
        ret = -EINVAL;
        return ret;
    }
}

/* Init NIC port, then start the port */
void xran_init_port(int p_id, uint16_t num_rxq, uint32_t mtu)
{
    static uint16_t nb_rxd = BURST_SIZE;
    static uint16_t nb_txd = BURST_SIZE;
    struct rte_ether_addr addr;
    struct rte_eth_rxmode rxmode = {
            .split_hdr_size = 0,
              .max_rx_pkt_len = MAX_RX_LEN,
            .offloads       = DEV_RX_OFFLOAD_JUMBO_FRAME
            };
    struct rte_eth_txmode txmode = {
            .mq_mode        = ETH_MQ_TX_NONE,
            .offloads       = DEV_TX_OFFLOAD_MULTI_SEGS
            };
    struct rte_eth_conf port_conf = {
            .rxmode = rxmode,
            .txmode = txmode
            };
    struct rte_eth_rxconf rxq_conf;
    struct rte_eth_txconf txq_conf;

    int ret;
    struct rte_eth_dev_info dev_info;
    const char *drv_name = "";
    int sock_id = rte_eth_dev_socket_id(p_id);
    char rx_pool_name[32]    = "";
    uint16_t data_room_size = MBUF_POOL_ELEMENT;
    uint16_t qi = 0;
    uint32_t num_mbufs = 0;

    if (mtu <= 1500) {
        rxmode.offloads &= ~DEV_RX_OFFLOAD_JUMBO_FRAME;
        rxmode.max_rx_pkt_len = RTE_ETHER_MAX_LEN;
        data_room_size = MBUF_POOL_ELM_SMALL;
    }

    rte_eth_dev_info_get(p_id, &dev_info);
    if (dev_info.driver_name)
        drv_name = dev_info.driver_name;
    printf("initializing port %d for TX, drv=%s\n", p_id, drv_name);

    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE){
        printf("set DEV_TX_OFFLOAD_MBUF_FAST_FREE\n");
        port_conf.txmode.offloads |=
            DEV_TX_OFFLOAD_MBUF_FAST_FREE;
    }

    rte_eth_macaddr_get(p_id, &addr);

    printf("Port %u MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
        " %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
        (unsigned)p_id,
        addr.addr_bytes[0], addr.addr_bytes[1], addr.addr_bytes[2],
        addr.addr_bytes[3], addr.addr_bytes[4], addr.addr_bytes[5]);

    if(num_rxq > 1) {
        nb_rxd    = 2048;
        num_mbufs = 2*nb_rxd-1;
    } else {
        nb_rxd    = BURST_SIZE;
        num_mbufs = NUM_MBUFS;
    }

    /* Init port */
    ret = rte_eth_dev_configure(p_id, num_rxq, 1, &port_conf);
    if (ret < 0)
        rte_panic("Cannot configure port %u (%d)\n", p_id, ret);

    ret = rte_eth_dev_adjust_nb_rx_tx_desc(p_id, &nb_rxd,&nb_txd);

    if (ret < 0) {
        printf("\n");
        rte_exit(EXIT_FAILURE, "Cannot adjust number of "
            "descriptors: err=%d, port=%d\n", ret, p_id);
    }
    printf("Port %u: nb_rxd %d nb_txd %d\n", p_id, nb_rxd, nb_txd);

    for (qi = 0; qi < num_rxq; qi++) {
        snprintf(rx_pool_name, RTE_DIM(rx_pool_name), "%s_p_%d_q_%d", "mp_rx_", p_id, qi);
        printf("[%d] %s num blocks %d\n", p_id, rx_pool_name, num_mbufs);
        _eth_mbuf_pool_vf_rx[p_id][qi] = rte_pktmbuf_pool_create(rx_pool_name, num_mbufs,
                    MBUF_CACHE, 0, data_room_size, rte_socket_id());

        if (_eth_mbuf_pool_vf_rx[p_id][qi] == NULL)
            rte_panic("Cannot create mbuf pool: %s\n", rte_strerror(rte_errno));
    }

    snprintf(rx_pool_name, RTE_DIM(rx_pool_name), "%s_%d", "mempool_small_", p_id);
    printf("[%d] %s\n", p_id, rx_pool_name);
    _eth_mbuf_pool_vf_small[p_id] = rte_pktmbuf_pool_create(rx_pool_name, NUM_MBUFS_VF,
                MBUF_CACHE, 0, MBUF_POOL_ELM_SMALL_INDIRECT, rte_socket_id());

    if (_eth_mbuf_pool_vf_small[p_id] == NULL)
        rte_panic("Cannot create mbuf pool: %s\n", rte_strerror(rte_errno));

    /* Init RX queues */
    fflush(stdout);
    rxq_conf = dev_info.default_rxconf;

    for (qi = 0; qi < num_rxq; qi++) {
        ret = rx_queue_setup(p_id, qi, nb_rxd,
                sock_id, &rxq_conf, _eth_mbuf_pool_vf_rx[p_id][qi]);
    }

    if (ret < 0)
        rte_panic("Cannot init RX for port %u (%d)\n",
            p_id, ret);

    /* Init TX queues */
    fflush(stdout);
    txq_conf = dev_info.default_txconf;

    ret = rte_eth_tx_queue_setup(p_id, 0, nb_txd, sock_id, &txq_conf);
    if (ret < 0)
        rte_panic("Cannot init TX for port %u (%d)\n",
                p_id, ret);

    ret = rte_eth_dev_set_ptypes(p_id, RTE_PTYPE_UNKNOWN, NULL, 0);
    if (ret < 0)
        rte_panic("Port %d: Failed to disable Ptype parsing\n", p_id);

    /* Start port */
    ret = rte_eth_dev_start(p_id);
    if (ret < 0)
        rte_panic("Cannot start port %u (%d)\n", p_id, ret);
}

void xran_init_port_mempool(int p_id, uint32_t mtu)
{
    int ret;
    int sock_id = rte_eth_dev_socket_id(p_id);
    char rx_pool_name[32]    = "";
    uint16_t data_room_size = MBUF_POOL_ELEMENT;

    if (mtu <= 1500) {
        data_room_size = MBUF_POOL_ELM_SMALL;
}

    snprintf(rx_pool_name, RTE_DIM(rx_pool_name), "%s_%d", "mempool_small_", p_id);
    printf("[%d] %s\n", p_id, rx_pool_name);
    _eth_mbuf_pool_vf_small[p_id] = rte_pktmbuf_pool_create(rx_pool_name, NUM_MBUFS_VF,
                MBUF_CACHE, 0, MBUF_POOL_ELM_SMALL, rte_socket_id());

    if (_eth_mbuf_pool_vf_small[p_id] == NULL)
        rte_panic("Cannot create mbuf pool: %s\n", rte_strerror(rte_errno));


}

/* Prepend ethernet header, possibly vlan tag. */
void xran_add_eth_hdr_vlan(struct rte_ether_addr *dst, uint16_t ethertype, struct rte_mbuf *mb)
{
    /* add in the ethernet header */
    struct rte_ether_hdr *h = (struct rte_ether_hdr *)rte_pktmbuf_mtod(mb, struct rte_ether_hdr*);

    PANIC_ON(h == NULL, "mbuf prepend of ether_hdr failed");

    /* Fill in the ethernet header. */
    rte_eth_macaddr_get(mb->port, &h->s_addr);          /* set source addr */
    h->d_addr = *dst;                                   /* set dst addr */
    h->ether_type = rte_cpu_to_be_16(ethertype);        /* ethertype too */

#if defined(DPDKIO_DEBUG) && DPDKIO_DEBUG > 1
    {
        char dst[RTE_ETHER_ADDR_FMT_SIZE] = "(empty)";
        char src[RTE_ETHER_ADDR_FMT_SIZE] = "(empty)";

        printf("*** packet for TX below (len %d) ***", rte_pktmbuf_pkt_len(mb));
        rte_ether_format_addr(src, sizeof(src), &h->s_addr);
        rte_ether_format_addr(dst, sizeof(dst), &h->d_addr);
        printf("src: %s dst: %s ethertype: %.4X", src, dst, ethertype);
    }
#endif
}


