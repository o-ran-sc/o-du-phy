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

#include <rte_config.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
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

/* Our mbuf pools. */
struct rte_mempool *_eth_mbuf_pool          = NULL;
struct rte_mempool *_eth_mbuf_pool_inderect = NULL;
struct rte_mempool *_eth_mbuf_pool_rx     = NULL;
struct rte_mempool *_eth_mbuf_pool_small  = NULL;
struct rte_mempool *_eth_mbuf_pool_big    = NULL;

struct rte_mempool *socket_direct_pool    = NULL;
struct rte_mempool *socket_indirect_pool  = NULL;


/*
 * Make sure the ring indexes are big enough to cover buf space x2
 * This ring-buffer maintains the property head - tail <= RINGSIZE.
 * head == tail:  ring buffer empty
 * head - tail == RINGSIZE: ring buffer full
 */
typedef uint16_t ring_idx;
static struct {
    ring_idx head;
    ring_idx read_head;
    ring_idx tail;
    char buf[1024];      /* needs power of 2! */
} io_ring = { {0}, 0, 0};

#define RINGSIZE sizeof(io_ring.buf)
#define RINGMASK (RINGSIZE - 1)

int __xran_delayed_msg(const char *fmt, ...)
{
#if 0
    va_list ap;
    int msg_len;
    char localbuf[RINGSIZE];
    ring_idx old_head, new_head;
    ring_idx copy_len;

    /* first prep a copy of the message on the local stack */
    va_start(ap, fmt);
    msg_len = vsnprintf(localbuf, RINGSIZE, fmt, ap);
    va_end(ap);

    /* atomically reserve space in the ring */
    for (;;) {
        old_head = io_ring.head;        /* snapshot head */
        /* free always within range of [0, RINGSIZE] - proof by induction */
        const ring_idx free = RINGSIZE - (old_head - io_ring.tail);

        copy_len = RTE_MIN(msg_len, free);
        if (copy_len <= 0)
            return 0;   /* vsnprintf error or ringbuff full. Drop log. */

        new_head = old_head + copy_len;
        RTE_ASSERT((ring_idx)(new_head - io_ring.tail) <= RINGSIZE);

        if (likely(__atomic_compare_exchange_n(&io_ring.head, &old_head,
                        new_head, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)))
            break;
    }

    /* Now copy data in at ease. */
    const int copy_start = (old_head & RINGMASK);
    if (copy_start < (new_head & RINGMASK))     /* no wrap */
        memcpy(io_ring.buf + copy_start, localbuf, copy_len);
    else {                                      /* wrap-around */
        const int chunk_len = RINGSIZE - copy_start;

        memcpy(io_ring.buf + copy_start, localbuf, chunk_len);
        memcpy(io_ring.buf, localbuf + chunk_len, copy_len - chunk_len);
    }

    /* wait for previous writes to complete before updating read_head. */
    while (io_ring.read_head != old_head)
        rte_pause();
    io_ring.read_head = new_head;


    return copy_len;
 #endif
    return 0;
}

/*
 * Display part of the message stored in the ring buffer.
 * Might require multiple calls to print the full message.
 * Will return 0 when nothing left to print.
 */
#if 0
int xran_show_delayed_message(void)
{
    ring_idx tail = io_ring.tail;
    ring_idx wlen = io_ring.read_head - tail; /* always within [0, RINGSIZE] */

    if (wlen <= 0)
        return 0;

    tail &= RINGMASK;   /* modulo the range down now that we have wlen */

    /* Make sure we're not going over buffer end. Next call will wrap. */
    if (tail + wlen > RINGSIZE)
        wlen = RINGSIZE - tail;

    RTE_ASSERT(tail + wlen <= RINGSIZE);

    /* We use write() here to avoid recaculating string length in fwrite(). */
    const ssize_t written = write(STDOUT_FILENO, io_ring.buf + tail, wlen);
    if (written <= 0)
        return 0;   /* To avoid moving tail the wrong way on error. */

    /* Move tail up. Only we touch it. And we only print from one core. */
    io_ring.tail += written;

    return written;     /* next invocation will print the rest if any */
}
#endif

void xran_init_mbuf_pool(void)
{
    /* Init the buffer pool */
    if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
        _eth_mbuf_pool = rte_pktmbuf_pool_create("mempool", NUM_MBUFS,
                MBUF_CACHE, 0, MBUF_POOL_ELEMENT, rte_socket_id());
#ifdef XRAN_ATTACH_MBUF
        _eth_mbuf_pool_inderect = rte_pktmbuf_pool_create("mempool_indirect", NUM_MBUFS,
                MBUF_CACHE, 0, MBUF_POOL_ELEMENT, rte_socket_id());*/
#endif
        _eth_mbuf_pool_rx = rte_pktmbuf_pool_create("mempool_rx", NUM_MBUFS,
                MBUF_CACHE, 0, MBUF_POOL_ELEMENT, rte_socket_id());
        _eth_mbuf_pool_small = rte_pktmbuf_pool_create("mempool_small",
                NUM_MBUFS, MBUF_CACHE, 0, MBUF_POOL_ELM_SMALL, rte_socket_id());
        _eth_mbuf_pool_big = rte_pktmbuf_pool_create("mempool_big",
                NUM_MBUFS_BIG, 0, 0, MBUF_POOL_ELM_BIG, rte_socket_id());
    } else {
        _eth_mbuf_pool = rte_mempool_lookup("mempool");
        _eth_mbuf_pool_inderect = rte_mempool_lookup("mempool_indirect");
        _eth_mbuf_pool_rx = rte_mempool_lookup("mempool_rx");
        _eth_mbuf_pool_small = rte_mempool_lookup("mempool_small");
        _eth_mbuf_pool_big = rte_mempool_lookup("mempool_big");
    }
    if (_eth_mbuf_pool == NULL)
        rte_panic("Cannot create mbuf pool: %s\n", rte_strerror(rte_errno));
#ifdef XRAN_ATTACH_MBUF
    if (_eth_mbuf_pool_inderect == NULL)
        rte_panic("Cannot create mbuf pool: %s\n", rte_strerror(rte_errno));
#endif
    if (_eth_mbuf_pool_rx == NULL)
        rte_panic("Cannot create mbuf pool: %s\n", rte_strerror(rte_errno));
    if (_eth_mbuf_pool_small == NULL)
        rte_panic("Cannot create small mbuf pool: %s\n", rte_strerror(rte_errno));
    if (_eth_mbuf_pool_big == NULL)
        rte_panic("Cannot create big mbuf pool: %s\n", rte_strerror(rte_errno));

    if (socket_direct_pool == NULL)
        socket_direct_pool = _eth_mbuf_pool;

    if (socket_indirect_pool == NULL)
        socket_indirect_pool = _eth_mbuf_pool_inderect;
}

/* Init NIC port, then start the port */
void xran_init_port(int p_id)
{
    static uint16_t nb_rxd = BURST_SIZE;
    static uint16_t nb_txd = BURST_SIZE;
    struct rte_ether_addr addr;
    struct rte_eth_rxmode rxmode =
            { .split_hdr_size = 0,
              .max_rx_pkt_len = MAX_RX_LEN,
              .offloads=(DEV_RX_OFFLOAD_JUMBO_FRAME /*|DEV_RX_OFFLOAD_CRC_STRIP*/)
            };
    struct rte_eth_txmode txmode = {
                .mq_mode = ETH_MQ_TX_NONE
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

    rte_eth_dev_info_get(p_id, &dev_info);
    if (dev_info.driver_name)
        drv_name = dev_info.driver_name;
    printf("initializing port %d for TX, drv=%s\n", p_id, drv_name);

    rte_eth_macaddr_get(p_id, &addr);

    printf("Port %u MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
        " %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
        (unsigned)p_id,
        addr.addr_bytes[0], addr.addr_bytes[1], addr.addr_bytes[2],
        addr.addr_bytes[3], addr.addr_bytes[4], addr.addr_bytes[5]);

    /* Init port */
    ret = rte_eth_dev_configure(p_id, 1, 1, &port_conf);
    if (ret < 0)
        rte_panic("Cannot configure port %u (%d)\n", p_id, ret);

    ret = rte_eth_dev_adjust_nb_rx_tx_desc(p_id, &nb_rxd,&nb_txd);

    if (ret < 0) {
        printf("\n");
        rte_exit(EXIT_FAILURE, "Cannot adjust number of "
            "descriptors: err=%d, port=%d\n", ret, p_id);
    }
    printf("Port %u: nb_rxd %d nb_txd %d\n", p_id, nb_rxd, nb_txd);

    /* Init RX queues */
    rxq_conf = dev_info.default_rxconf;
    ret = rte_eth_rx_queue_setup(p_id, 0, nb_rxd,
        sock_id, &rxq_conf, _eth_mbuf_pool_rx);
    if (ret < 0)
        rte_panic("Cannot init RX for port %u (%d)\n",
            p_id, ret);

    /* Init TX queues */
    txq_conf = dev_info.default_txconf;
    ret = rte_eth_tx_queue_setup(p_id, 0, nb_txd, sock_id, &txq_conf);
    if (ret < 0)
        rte_panic("Cannot init TX for port %u (%d)\n",
                p_id, ret);

    /* Start port */
    ret = rte_eth_dev_start(p_id);
    if (ret < 0)
        rte_panic("Cannot start port %u (%d)\n", p_id, ret);

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


