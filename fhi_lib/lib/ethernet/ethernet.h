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
 * @file ethernet.h
 * @ingroup group_lte_source_auxlib
 * @author Intel Corporation
 **/

#ifndef _XRANLIB_ETHERNET_H_
#define _XRANLIB_ETHERNET_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_config.h>
#include <rte_ether.h>
#include <rte_mbuf.h>


#define BURST_SIZE 4096 /** IAVF_MAX_RING_DESC        4096  */

#define ETHER_TYPE_ETHDI RTE_ETHER_TYPE_IPV4    /* hack needed for jumbo frames */
#define ETHER_TYPE_ECPRI 0xAEFE

#define NUM_MBUFS 65535/*16383*/ /*65535*/ /** optimal is n = (2^q - 1) */
#define NUM_MBUFS_RING NUM_MBUFS+1 /** The size of the ring (must be a power of 2) */

#define NUM_MBUFS_VF 1048575

#define NUM_MBUFS_RING_TRX 2097152

#define MBUF_CACHE 256

#define MBUF_POOL_ELM_SMALL_INDIRECT (128 + RTE_PKTMBUF_HEADROOM ) /* indirect */

#define MBUF_POOL_ELM_SMALL (1500 + RTE_PKTMBUF_HEADROOM )/* regular ethernet MTU, most compatible */
#define MBUF_POOL_ELEMENT (MAX_RX_LEN + RTE_PKTMBUF_HEADROOM)

#define MBUF_POOL_PKT_GEN_ELM (256 + RTE_PKTMBUF_HEADROOM )

#define MAX_RX_LEN 9600
#define MAX_TX_LEN (MAX_RX_LEN - 14) /* headroom for rx driver */
#define MAX_DATA_SIZE (MAX_TX_LEN - sizeof(struct ether_hdr) - \
    sizeof(struct ethdi_hdr) - sizeof(struct burst_hdr))

extern struct rte_mempool *_eth_mbuf_pool;

extern struct rte_mempool *_eth_mbuf_pkt_gen;
extern struct rte_mempool *_eth_mbuf_pool_big;
extern struct rte_mempool *socket_direct_pool;
extern struct rte_mempool *socket_indirect_pool;

extern struct rte_mempool *_eth_mbuf_pool_vf_rx[16][RTE_MAX_QUEUES_PER_PORT];
extern struct rte_mempool *_eth_mbuf_pool_vf_small[16];



struct ethdi_hdr {
    uint8_t pkt_type;
    uint8_t source_id;
    uint8_t dest_id;
    int8_t data[];    /* original raw data starts here! */
};


void xran_init_mbuf_pool(uint32_t mtu);
void xran_init_port(int port, uint16_t num_rxq, uint32_t mtu);
void xran_init_port_mempool(int p_id, uint32_t mtu);
void xran_add_eth_hdr_vlan(struct rte_ether_addr *dst, uint16_t ethertype, struct rte_mbuf *mb);

#define PANIC_ON(x, m, ...) do { if (unlikely(x)) \
    rte_panic("%s: " m "\n", #x, ##__VA_ARGS__); } while (0)

/* Add mbuf to the TX ring. */
static inline int xran_enqueue_mbuf(struct rte_mbuf *mb, struct rte_ring *r)
{
    if (rte_ring_enqueue(r, mb) == 0) {
        return 1;   /* success */
    }

    rte_pktmbuf_free(mb);
    //print_err("failed to enqueue packet on port %d (ring full)", mb->port);

    return 0;   /* fail */
}

#ifdef __cplusplus
}
#endif

#endif /* _XRANLIB_ETHERNET_H_ */
