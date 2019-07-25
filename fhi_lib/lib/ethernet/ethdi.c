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
 * @file ethdi.c
 * @ingroup group_lte_source_auxlib
 * @author Intel Corporation
 **/

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <err.h>
#include <assert.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

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
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mbuf.h>
#include <rte_timer.h>

#include "ethernet.h"
#include "ethdi.h"
#ifndef MLOG_ENABLED
#include "../src/mlog_lnx_xRAN.h"
#else
#include "mlog_lnx.h"
#endif

#include "../src/xran_lib_mlog_tasks_id.h"

struct xran_ethdi_ctx g_ethdi_ctx = { 0 };
enum xran_if_state xran_if_current_state = XRAN_STOPPED;

struct rte_mbuf *xran_ethdi_mbuf_alloc(void)
{
    return rte_pktmbuf_alloc(_eth_mbuf_pool);
}

int xran_ethdi_mbuf_send(struct rte_mbuf *mb, uint16_t ethertype)
{
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    int res = 0;

    mb->port = ctx->io_cfg.port[ETHDI_UP_VF];
    xran_add_eth_hdr_vlan(&ctx->entities[ID_RU], ethertype, mb, ctx->up_vtag);

    res = xran_enqueue_mbuf(mb, ctx->tx_ring[ETHDI_UP_VF]);
    return res;
}

int xran_ethdi_mbuf_send_cp(struct rte_mbuf *mb, uint16_t ethertype)
{
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    int res = 0;

    mb->port = ctx->io_cfg.port[ETHDI_CP_VF];
    xran_add_eth_hdr_vlan(&ctx->entities[ID_RU], ethertype, mb, ctx->cp_vtag);

    res = xran_enqueue_mbuf(mb, ctx->tx_ring[ETHDI_CP_VF]);
    return res;
}

void xran_ethdi_stop_tx()
{
    struct xran_ethdi_ctx *const ctx = xran_ethdi_get_ctx();
    rte_timer_stop_sync(&ctx->timer_tx);
}


struct {
    uint16_t ethertype;
    ethertype_handler fn;
} xran_ethertype_handlers[] = {
    { ETHER_TYPE_ETHDI, NULL },
    { ETHER_TYPE_ECPRI, NULL },
    { ETHER_TYPE_START_TX, NULL }
};



int xran_register_ethertype_handler(uint16_t ethertype, ethertype_handler callback)
{
    int i;

    for (i = 0; i < RTE_DIM(xran_ethertype_handlers); ++i)
        if (xran_ethertype_handlers[i].ethertype == ethertype) {
            xran_ethertype_handlers[i].fn = callback;

            return 1;
        }

    elog("support for ethertype %u not found", ethertype);

    return 0;
}

int xran_handle_ether(uint16_t ethertype, struct rte_mbuf *pkt, uint64_t rx_time)
{
    int i;

    for (i = 0; i < RTE_DIM(xran_ethertype_handlers); ++i)
        if (xran_ethertype_handlers[i].ethertype == ethertype)
            if (xran_ethertype_handlers[i].fn)
                return xran_ethertype_handlers[i].fn(pkt, rx_time);

    wlog("Packet with unrecognized ethertype '%.4X' dropped", ethertype);

    return 0;
};


/* Process vlan tag. Cut the ethernet header. Call the etherype handlers. */
int xran_ethdi_filter_packet(struct rte_mbuf *pkt, uint64_t rx_time)
{
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();

#ifdef VLAN_SUPPORT
    if (rte_vlan_strip(pkt) == 0) {
        if (pkt->vlan_tci == ctx->cp_vtag) {
            dlog("VLAN tci matches %d", pkt->vlan_tci);
        } else {
            wlog("packet with wrong VLAN tag %d, dropping",
                    pkt->vlan_tci);
            return 0;
        }
    } else
        dlog("Packet not vlan tagged");
#endif

    const struct ether_hdr *eth_hdr = rte_pktmbuf_mtod(pkt, void *);

#if defined(DPDKIO_DEBUG) && DPDKIO_DEBUG > 1
    nlog("*** processing RX'ed packet of size %d ***",
            rte_pktmbuf_data_len(pkt));
    /* TODO: just dump ethernet header in readable format? */
#endif

#if defined(DPDKIO_DEBUG) && DPDKIO_DEBUG > 1
    {
        char dst[ETHER_ADDR_FMT_SIZE] = "(empty)";
        char src[ETHER_ADDR_FMT_SIZE] = "(empty)";

        ether_format_addr(dst, sizeof(dst), &eth_hdr->d_addr);
        ether_format_addr(src, sizeof(src), &eth_hdr->s_addr);
        nlog("src: %s dst: %s ethertype: %.4X", dst, src,
                rte_be_to_cpu_16(eth_hdr->ether_type));
    }
#endif

    /* Cut out the ethernet header. It's not needed anymore. */
    if (rte_pktmbuf_adj(pkt, sizeof(*eth_hdr)) == NULL) {
        wlog("Packet too short, dropping");
        return 0;
    }


    return xran_handle_ether(rte_be_to_cpu_16(eth_hdr->ether_type), pkt, rx_time);
}




int xran_ethdi_init_dpdk_io(char *name, const struct xran_io_loop_cfg *io_cfg,
    int *lcore_id, struct ether_addr *p_lls_cu_addr, struct ether_addr *p_ru_addr,
    uint16_t cp_vlan, uint16_t up_vlan)
{
    uint16_t port[2] = {0, 0};
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    int i;
    char core_mask[20];
    char *argv[] = { name, core_mask, "-m3072", "--proc-type=auto",
        "--file-prefix", name, "-w", "0000:00:00.0" };

    if (io_cfg == NULL)
        return 0;

    snprintf(core_mask, sizeof(core_mask), "-c%x",
            (1 << io_cfg->core) |
            (1 << io_cfg->system_core) |
            (1 << io_cfg->pkt_proc_core) |
            (1 << io_cfg->pkt_aux_core) |
            (1 << io_cfg->timing_core));

    ctx->io_cfg = *io_cfg;
    ctx->ping_state           = PING_IDLE;
    ctx->known_peers          = 1;
    ctx->busy_poll_till = rte_rdtsc();
    ctx->cp_vtag = cp_vlan;
    ctx->up_vtag = up_vlan;

    for (i = 0; i <= ID_BROADCAST; i++)     /* Initialize all as broadcast */
        memset(&ctx->entities[i], 0xFF, sizeof(ctx->entities[0]));

    /* This will return on system_core, which is not necessarily the
     * one we're on right now. */
    if (rte_eal_init(RTE_DIM(argv), argv) < 0)
        rte_panic("Cannot init EAL: %s\n", rte_strerror(rte_errno));

    xran_init_mbuf_pool();

    /* Timers. */
    rte_timer_subsystem_init();
    rte_timer_init(&ctx->timer_ping);
    rte_timer_init(&ctx->timer_sync);
    rte_timer_init(&ctx->timer_tx);

    *lcore_id = rte_get_next_lcore(rte_lcore_id(), 0, 0);

    PANIC_ON(*lcore_id == RTE_MAX_LCORE, "out of lcores for io_loop()");

    if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
        for (i = 0; i < ETHDI_VF_MAX; i ++){
            if (rte_eth_dev_attach(io_cfg->dpdk_dev[i], &port[i]) != 0 ||
                rte_eth_dev_count_avail() == 0)
                errx(1, "Network port doesn't exist.");
            xran_init_port(port[i], p_lls_cu_addr);   /* we only have 1 port at this stage */
            if(i==0){
                ctx->tx_ring[i] = rte_ring_create("tx_ring_up", NUM_MBUFS,
                    rte_lcore_to_socket_id(*lcore_id), RING_F_SC_DEQ);
                ctx->rx_ring[i] = rte_ring_create("rx_ring_up", NUM_MBUFS,
                    rte_lcore_to_socket_id(*lcore_id), RING_F_SC_DEQ);
                ctx->pkt_dump_ring[i] = rte_ring_create("pkt_dump_ring_up", NUM_MBUFS,
                    rte_lcore_to_socket_id(*lcore_id), RING_F_SC_DEQ);
            }else {
                ctx->tx_ring[i] = rte_ring_create("tx_ring_cp", NUM_MBUFS,
                    rte_lcore_to_socket_id(*lcore_id), RING_F_SC_DEQ);
                ctx->rx_ring[i] = rte_ring_create("rx_ring_cp", NUM_MBUFS,
                    rte_lcore_to_socket_id(*lcore_id), RING_F_SC_DEQ);
                ctx->pkt_dump_ring[i] = rte_ring_create("pkt_dump_ring_cp", NUM_MBUFS,
                    rte_lcore_to_socket_id(*lcore_id), RING_F_SC_DEQ);
            }
        }
    } else {
        rte_panic("ethdi_dpdk_io_loop() failed to start  with RTE_PROC_SECONDARY\n");
    }
    PANIC_ON(ctx->tx_ring == NULL, "failed to allocate tx ring");
    PANIC_ON(ctx->rx_ring == NULL, "failed to allocate rx ring");
    PANIC_ON(ctx->pkt_dump_ring == NULL, "failed to allocate pkt dumping ring");
    for (i = 0; i < ETHDI_VF_MAX; i++)
        ctx->io_cfg.port[i] = port[i];

    rte_eth_macaddr_get(port[ETHDI_UP_VF], &ctx->entities[io_cfg->id]);
    ether_addr_copy(p_ru_addr,  &ctx->entities[ID_RU]);

    /* Start the actual IO thread */
    if (rte_eal_remote_launch(xran_ethdi_dpdk_io_loop, &ctx->io_cfg, *lcore_id))
        rte_panic("ethdi_dpdk_io_loop() failed to start\n");

    return 1;
}

static inline uint16_t xran_tx_from_ring(int port, struct rte_ring *r)
{
    struct rte_mbuf *mbufs[BURST_SIZE];
    uint16_t dequeued, sent = 0;
    uint32_t remaining;
    int i;
    long t1 = MLogTick();

    dequeued = rte_ring_dequeue_burst(r, (void **)mbufs, BURST_SIZE,
            &remaining);
    if (!dequeued)
        return 0;   /* Nothing to send. */

    while (1) {     /* When tx queue is full it is trying again till succeed */
        t1 = MLogTick();
        sent += rte_eth_tx_burst(port, 0, &mbufs[sent], dequeued - sent);
        MLogTask(PID_RADIO_ETH_TX_BURST, t1, MLogTick());

        if (sent == dequeued)
            return remaining;
    }
}



/*
 * This is the main DPDK-IO loop.
 * This will sleep if there's no packets incoming and there's
 * no work enqueued, sleep lenth is defined in IDLE_SLEEP_MICROSECS
 */
int xran_ethdi_dpdk_io_loop(void *io_loop_cfg)
{
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    const struct xran_io_loop_cfg *const cfg = io_loop_cfg;
    const int port[ETHDI_VF_MAX] = {cfg->port[ETHDI_UP_VF], cfg->port[ETHDI_CP_VF]};
    int port_id = 0;
    struct sched_param sched_param;
    int res = 0;

    printf("%s [PORT: %d %d] [CPU %2d] [PID: %6d]\n", __FUNCTION__, port[ETHDI_UP_VF], port[ETHDI_CP_VF] , rte_lcore_id(), getpid());

    printf("%s [CPU %2d] [PID: %6d]\n", __FUNCTION__,  rte_lcore_id(), getpid());
    sched_param.sched_priority = XRAN_THREAD_DEFAULT_PRIO;
    if ((res = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched_param)))
    {
        printf("priority is not changed: coreId = %d, result1 = %d\n",rte_lcore_id(), res);
    }

    for (;;) {
        for (port_id = 0; port_id < ETHDI_VF_MAX; port_id++){
            struct rte_mbuf *mbufs[BURST_SIZE];
            /* RX */
            const uint16_t rxed = rte_eth_rx_burst(port[port_id], 0, mbufs, BURST_SIZE);
            if (rxed != 0){
                long t1 = MLogTick();
                rte_ring_enqueue_burst(ctx->rx_ring[port_id], (void*)mbufs, rxed, NULL);
                MLogTask(PID_RADIO_RX_VALIDATE, t1, MLogTick());
            }

            /* TX */
            const uint16_t sent = xran_tx_from_ring(port[port_id], ctx->tx_ring[port_id]);
            if (rxed | sent)
                continue;   /* more packets might be waiting in queues */

            rte_pause();    /* short pause, optimize memory access */
            if (XRAN_STOPPED == xran_if_current_state)
                break;
        }

        if (XRAN_STOPPED == xran_if_current_state)
                break;
    }

    fflush(stderr);
    fflush(stdout);
    puts("IO loop finished");

    //for (port_id = 0; port_id < ETHDI_VF_MAX; port_id++)
      //  xran_ethdi_port_stats(port[port_id]);

    return 0;
}
