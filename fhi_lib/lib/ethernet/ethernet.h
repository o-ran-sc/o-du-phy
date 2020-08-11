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
 * @file ethernet.h
 * @ingroup group_lte_source_auxlib
 * @author Intel Corporation
 **/

#ifndef AUXLIB_ETHERNET_H
#define AUXLIB_ETHERNET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_config.h>
#include <rte_ether.h>
#include <rte_mbuf.h>

#define BURST_SIZE 4096

#define ETHER_TYPE_ETHDI RTE_ETHER_TYPE_IPV4    /* hack needed for jumbo frames */
#define ETHER_TYPE_ECPRI 0xAEFE
#define ETHER_TYPE_SYNC 0xBEFE
#define ETHER_TYPE_START_TX 0xCEFE

#define NUM_MBUFS 65535/*16383*/ /*65535*/ /** optimal is n = (2^q - 1) */
#define NUM_MBUFS_RING NUM_MBUFS+1 /** The size of the ring (must be a power of 2) */

#define MBUF_CACHE 256

#define MBUF_POOL_ELM_SMALL (1500 + RTE_PKTMBUF_HEADROOM )/* regular ethernet MTU, most compatible */
#define MBUF_POOL_ELEMENT (MAX_RX_LEN + RTE_PKTMBUF_HEADROOM)

#define MAX_RX_LEN 9600
#define MAX_TX_LEN (MAX_RX_LEN - 14) /* headroom for rx driver */
#define MAX_DATA_SIZE (MAX_TX_LEN - sizeof(struct ether_hdr) - \
    sizeof(struct ethdi_hdr) - sizeof(struct burst_hdr))

/* Looks like mbuf size is limited to 16 bits - see the buf_len field. */
#define MBUF_POOL_ELM_BIG USHRT_MAX
#define NUM_MBUFS_BIG 64

#define DEFAULT_DUMP_LENGTH 96

extern struct rte_mempool *_eth_mbuf_pool;
extern struct rte_mempool *_eth_mbuf_pool_small;
extern struct rte_mempool *_eth_mbuf_pool_big;
extern struct rte_mempool *socket_direct_pool;
extern struct rte_mempool *socket_indirect_pool;

/* Do NOT change the order of this enum and below
 * - need to be in sync with the table of handlers in testue.c */
enum pkt_type
{
    PKT_ZERO,
    PKT_EMPTY,
    PKT_DISCOVER_REQUEST,
    PKT_PING,
    PKT_PONG,
    PKT_DISCOVER_REPLY,
    PKT_LTE_DATA,
    PKT_LTE_CONTROL,
    PKT_BURST,
    PKT_DATATEST,
    PKT_ADD_ETHDEV,
    PKT_SYNC_START,
    PKT_LAST,
};

/* Do NOT change the order. */
static char * const xran_pkt_descriptions[PKT_LAST + 1] = {
    "ZERO",
    "empty packet",
    "discovery request packet",
    "ping packet",
    "pong packet",
    "discovery reply packet",
    "LTE data packet",
    "LTE control packet",
    "BURST packet",
    "DATATEST packet",
    "Add ethernet port command packet",
    "SYNC-START packet",
    "LAST packet",
};

struct burst_hdr {
    int8_t pkt_idx;
    int8_t total_pkts;
    int8_t original_type;
    int8_t data[];
};

struct ethdi_hdr {
    uint8_t pkt_type;
    uint8_t source_id;
    uint8_t dest_id;
    int8_t data[];    /* original raw data starts here! */
};


void xran_init_mbuf_pool(void);

void xran_init_port(int port);

void xran_add_eth_hdr_vlan(struct rte_ether_addr *dst, uint16_t ethertype, struct rte_mbuf *mb);

#if 0
void xran_memdump(void *addr, int len);
void xran_add_eth_hdr(struct ether_addr *dst, uint16_t ethertype, struct rte_mbuf *);
int xran_send_mbuf(struct ether_addr *dst, struct rte_mbuf *mb);
int xran_send_message_burst(int dst_id, int pkt_type, void *body, int len);
int xran_show_delayed_message(void);
#endif
/*
 * Print a message after all critical processing done.
 * Mt-safe. 4 variants - normal, warning, error and debug log.
 */
int __xran_delayed_msg(const char *fmt, ...);
#define nlog(m, ...) __xran_delayed_msg("%s(): " m "\n", __FUNCTION__, ##__VA_ARGS__)
#define delayed_message nlog    /* this is the old alias for this function */
#define wlog(m, ...) nlog("WARNING: " m, ##__VA_ARGS__)
#define elog(m, ...) nlog("ERROR: " m, ##__VA_ARGS__)
#ifdef DEBUG
# define dlog(m, ...) nlog("DEBUG: " m, ##__VA_ARGS__)
#else
# define dlog(m, ...)
#endif

#define PANIC_ON(x, m, ...) do { if (unlikely(x)) \
    rte_panic("%s: " m "\n", #x, ##__VA_ARGS__); } while (0)

/* Add mbuf to the TX ring. */
static inline int xran_enqueue_mbuf(struct rte_mbuf *mb, struct rte_ring *r)
{
    if (rte_ring_enqueue(r, mb) == 0) {
        return 1;   /* success */
    }

    rte_pktmbuf_free(mb);
    wlog("failed to enqueue packet on port %d (ring full)", mb->port);

    return 0;   /* fail */
}

#ifdef __cplusplus
}
#endif

#endif /* AUXLIB_ETHERNET_H */
