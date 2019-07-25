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
struct rte_mempool *_eth_mbuf_pool;
struct rte_mempool *_eth_mbuf_pool_rx;
struct rte_mempool *_eth_mbuf_pool_small;
struct rte_mempool *_eth_mbuf_pool_big;

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


/*
 * Display part of the message stored in the ring buffer.
 * Might require multiple calls to print the full message.
 * Will return 0 when nothing left to print.
 */
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


void xran_init_mbuf_pool(void)
{
    /* Init the buffer pool */
    if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
        _eth_mbuf_pool = rte_pktmbuf_pool_create("mempool", NUM_MBUFS,
                MBUF_CACHE, 0, MBUF_POOL_ELEMENT, rte_socket_id());
        _eth_mbuf_pool_rx = rte_pktmbuf_pool_create("mempool_rx", NUM_MBUFS,
                MBUF_CACHE, 0, MBUF_POOL_ELEMENT, rte_socket_id());
        _eth_mbuf_pool_small = rte_pktmbuf_pool_create("mempool_small",
                NUM_MBUFS, MBUF_CACHE, 0, MBUF_POOL_ELM_SMALL, rte_socket_id());
        _eth_mbuf_pool_big = rte_pktmbuf_pool_create("mempool_big",
                NUM_MBUFS_BIG, 0, 0, MBUF_POOL_ELM_BIG, rte_socket_id());
    } else {
        _eth_mbuf_pool = rte_mempool_lookup("mempool");
        _eth_mbuf_pool_rx = rte_mempool_lookup("mempool_rx");
        _eth_mbuf_pool_small = rte_mempool_lookup("mempool_small");
        _eth_mbuf_pool_big = rte_mempool_lookup("mempool_big");
    }
    if (_eth_mbuf_pool == NULL)
        rte_panic("Cannot create mbuf pool: %s\n", rte_strerror(rte_errno));
    if (_eth_mbuf_pool_rx == NULL)
        rte_panic("Cannot create mbuf pool: %s\n", rte_strerror(rte_errno));
    if (_eth_mbuf_pool_small == NULL)
        rte_panic("Cannot create small mbuf pool: %s\n", rte_strerror(rte_errno));
    if (_eth_mbuf_pool_big == NULL)
        rte_panic("Cannot create big mbuf pool: %s\n", rte_strerror(rte_errno));
}

/* Init NIC port, then start the port */
void xran_init_port(int p_id,  struct ether_addr *p_lls_cu_addr)
{
    char buf[ETHER_ADDR_FMT_SIZE];
    struct ether_addr eth_addr;
    struct rte_eth_rxmode rxmode =
            { .split_hdr_size = 0,
              .max_rx_pkt_len = MAX_RX_LEN,
              .offloads=(DEV_RX_OFFLOAD_JUMBO_FRAME|DEV_RX_OFFLOAD_CRC_STRIP)
            };
    struct rte_eth_txmode txmode = {
                .mq_mode = ETH_MQ_TX_NONE
            };
    struct rte_eth_conf port_conf = {
            .rxmode = rxmode,
            .txmode = txmode
            };
    struct ether_addr pDstEthAddr;

    struct rte_eth_rxconf rxq_conf;
    struct rte_eth_txconf txq_conf;

    int ret;
    struct rte_eth_dev_info dev_info;
    const char *drv_name = "";
    int sock_id = rte_eth_dev_socket_id(p_id);

   // ether_format_addr(buf, sizeof(buf), p_lls_cu_addr);
   // printf("port %d set mac address %s\n", p_id, buf);
   // rte_eth_dev_default_mac_addr_set(p_id, p_lls_cu_addr);

    rte_eth_dev_info_get(p_id, &dev_info);
    if (dev_info.driver_name)
        drv_name = dev_info.driver_name;
    printf("initializing port %d for TX, drv=%s\n", p_id, drv_name);

    /* In order to receive packets from any server need to add broad case address
    * for the port*/
    pDstEthAddr.addr_bytes[0] = 0xFF;
    pDstEthAddr.addr_bytes[1] = 0xFF;
    pDstEthAddr.addr_bytes[2] = 0xFF;

    pDstEthAddr.addr_bytes[3] = 0xFF;
    pDstEthAddr.addr_bytes[4] = 0xFF;
    pDstEthAddr.addr_bytes[5] = 0xFF;

    rte_eth_macaddr_get(p_id, &eth_addr);
    ether_format_addr(buf, sizeof(buf), &eth_addr);
    printf("port %d mac address %s\n", p_id, buf);

    struct ether_addr addr;
    rte_eth_macaddr_get(p_id, &addr);

//    rte_eth_dev_mac_addr_add(p_id, &pDstEthAddr,1);
   // rte_eth_dev_mac_addr_add(p_id, &addr, 1);

    printf("Port %u MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
        " %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
        (unsigned)p_id,
        addr.addr_bytes[0], addr.addr_bytes[1], addr.addr_bytes[2],
        addr.addr_bytes[3], addr.addr_bytes[4], addr.addr_bytes[5]);

    /* Init port */
    ret = rte_eth_dev_configure(p_id, 1, 1, &port_conf);
    if (ret < 0)
        rte_panic("Cannot configure port %u (%d)\n", p_id, ret);

    /* Init RX queues */
    rxq_conf = dev_info.default_rxconf;
    ret = rte_eth_rx_queue_setup(p_id, 0, BURST_SIZE,
        sock_id, &rxq_conf, _eth_mbuf_pool_rx);
    if (ret < 0)
        rte_panic("Cannot init RX for port %u (%d)\n",
            p_id, ret);

    /* Init TX queues */
    txq_conf = dev_info.default_txconf;
    ret = rte_eth_tx_queue_setup(p_id, 0, BURST_SIZE, sock_id, &txq_conf);
    if (ret < 0)
        rte_panic("Cannot init TX for port %u (%d)\n",
                p_id, ret);

    /* Start port */
    ret = rte_eth_dev_start(p_id);
    if (ret < 0)
        rte_panic("Cannot start port %u (%d)\n", p_id, ret);

    rte_eth_promiscuous_enable(p_id);
}

void xran_memdump(void *addr, int len)
{
    int i;
    char tmp_buf[len * 2 + len / 16 + 1];
    char *p = tmp_buf;

    return;
#if 0
    for (i = 0; i < len; ++i) {
        sprintf(p, "%.2X ", ((uint8_t *)addr)[i]);
        if (i % 16 == 15)
            *p++ = '\n';
    }
    *p = 0;
    nlog("%s", tmp_buf);
#endif
}


/* Prepend ethernet header, possibly vlan tag. */
void xran_add_eth_hdr_vlan(struct ether_addr *dst, uint16_t ethertype, struct rte_mbuf *mb, uint16_t vlan_tci)
{
    /* add in the ethernet header */
    struct ether_hdr *const h = (void *)rte_pktmbuf_prepend(mb, sizeof(*h));

    PANIC_ON(h == NULL, "mbuf prepend of ether_hdr failed");

    /* Fill in the ethernet header. */
    rte_eth_macaddr_get(mb->port, &h->s_addr);          /* set source addr */
    h->d_addr = *dst;                                   /* set dst addr */
    h->ether_type = rte_cpu_to_be_16(ethertype);        /* ethertype too */

#if defined(DPDKIO_DEBUG) && DPDKIO_DEBUG > 1
    {
        char dst[ETHER_ADDR_FMT_SIZE] = "(empty)";
        char src[ETHER_ADDR_FMT_SIZE] = "(empty)";

        nlog("*** packet for TX below (len %d) ***", rte_pktmbuf_pkt_len(mb));
        ether_format_addr(src, sizeof(src), &h->s_addr);
        ether_format_addr(dst, sizeof(dst), &h->d_addr);
        nlog("src: %s dst: %s ethertype: %.4X", src, dst, ethertype);
    }
#endif
#ifdef VLAN_SUPPORT
    mb->vlan_tci = vlan_tci;
    dlog("Inserting vlan tag of %d", vlan_tci);
    rte_vlan_insert(&mb);
#endif
}

int xran_send_message_burst(int dst_id, int pkt_type, void *body, int len)
{
    struct rte_mbuf *mbufs[BURST_SIZE];
    int i;
    uint8_t *src = body;
    const struct xran_ethdi_ctx *const ctx = xran_ethdi_get_ctx();

    /* We're limited by maximum mbuf size on the receive size.
     * We can change this but this would be a bigger rework. */
    RTE_ASSERT(len < MBUF_POOL_ELM_BIG);

    /* Allocate the required number of mbufs. */
    const uint8_t count = ceilf((float)len / MAX_DATA_SIZE);
    if (rte_pktmbuf_alloc_bulk(_eth_mbuf_pool, mbufs, count) != 0)
        rte_panic("Failed to allocate %d mbufs\n", count);

    nlog("burst transfer with data size %lu", MAX_DATA_SIZE);
    for (i = 0; len > 0; ++i) {
        char *p;
        struct burst_hdr *bhdr;
        struct ethdi_hdr *edi_hdr;

        /* Setup the ethdi_hdr. */
        edi_hdr = (void *)rte_pktmbuf_append(mbufs[i], sizeof(*edi_hdr));
        if (edi_hdr == NULL)
            rte_panic("append of ethdi_hdr failed\n");
        edi_hdr->pkt_type = PKT_BURST;
        /* edi_hdr->source_id setup in tx_from_ring */
        edi_hdr->dest_id = dst_id;

        /* Setup the burst header */
        bhdr = (void *)rte_pktmbuf_append(mbufs[i], sizeof(*bhdr));
        if (bhdr == NULL)        /* append failed. */
            rte_panic("mbuf prepend of burst_hdr failed\n");
        bhdr->original_type = pkt_type;
        bhdr->pkt_idx = i;       /* save the index of the burst chunk. */
        bhdr->total_pkts = count;

        /* now copy in the actual data */
        const int curr_data_len = RTE_MIN(len, MAX_TX_LEN -
                rte_pktmbuf_pkt_len(mbufs[i]) - sizeof(struct ether_hdr));
        p = (void *)rte_pktmbuf_append(mbufs[i], curr_data_len);
        if (p == NULL)
            rte_panic("mbuf append of %d data bytes failed\n", curr_data_len);
        /* This copy is unavoidable, as we're splitting one big buffer
         * into multiple mbufs. */
        rte_memcpy(p, src, curr_data_len);

        dlog("curr_data_len[%d] = %d", i, curr_data_len);
        dlog("packet %d size %d", i, rte_pktmbuf_pkt_len(mbufs[i]));

        /* Update our source data pointer and remaining length. */
        len -= curr_data_len;
        src += curr_data_len;
    }

    /* Now enqueue the full prepared burst. */
    i = rte_ring_enqueue_bulk(ctx->tx_ring[0], (void **)mbufs, count, NULL);
    PANIC_ON(i != count, "failed to enqueue all mbufs: %d/%d", i, count);
    dlog("%d packets enqueued on port %d.", count, ctx->io_cfg.port);

    return 1;
}
