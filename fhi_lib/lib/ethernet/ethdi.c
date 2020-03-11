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

#define _GNU_SOURCE
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
#include "xran_fh_o_du.h"
#include "xran_mlog_lnx.h"
#include "xran_printf.h"

#include "../src/xran_lib_mlog_tasks_id.h"

#define BURST_RX_IO_SIZE 48

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
#if 0
void xran_ethdi_stop_tx()
{
    struct xran_ethdi_ctx *const ctx = xran_ethdi_get_ctx();
    rte_timer_stop_sync(&ctx->timer_tx);
}
#endif

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

#if 0
//-------------------------------------------------------------------------------------------
/** @ingroup xran
 *
 *  @param[in]  port - DPDK ETH port id
 *
 *  @return  void
 *
 *  @description
 *  Prints statistics of usage of DPDK port
 *
**/
//-------------------------------------------------------------------------------------------
void xran_ethdi_ports_stats(void)
{
    struct rte_eth_stats stats;
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    int32_t i = 0;

    for(i = 0; i < ETHDI_VF_MAX; i++){
        /* Get stats (extended stats includes common stats) */
        rte_eth_stats_get(ctx->io_cfg.port[i], &stats);
        printf("DPDK stats:\n");
        printf("** Port %hhu **\n", ctx->io_cfg.port[i]);
        printf("ierrors:\t%lu\n",   stats.ierrors);
        printf("oerrors:\t%lu\n",   stats.oerrors);
        printf("ipackets:\t%lu\n",  stats.ipackets);
        printf("opackets:\t%lu\n",  stats.opackets);
        printf("imissed:\t%lu\n",   stats.imissed);
        printf("rx_nombuf:\t%lu\n", stats.rx_nombuf);
    }
    return ;
}
#endif
/* Check the link status of all ports in up to 9s, and print them finally */
static void check_port_link_status(uint8_t portid)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
    uint8_t count, all_ports_up, print_flag = 0;
    struct rte_eth_link link;

    printf("\nChecking link status");
    fflush(stdout);
    for (count = 0; count <= MAX_CHECK_TIME; count++) {
        all_ports_up = 1;
        memset(&link, 0, sizeof(link));
        rte_eth_link_get_nowait(portid, &link);

        /* print link status if flag set */
        if (print_flag == 1) {
            if (link.link_status)
                printf("Port %d Link Up - speed %u "
                        "Mbps - %s\n", (uint8_t)portid,
                        (unsigned)link.link_speed,
                        (link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
                        ("full-duplex") : ("half-duplex\n"));
            else
                printf("Port %d Link Down\n",
                        (uint8_t)portid);
        }
        /* clear all_ports_up flag if any link down */
        if (link.link_status == ETH_LINK_DOWN) {
            all_ports_up = 0;
            break;
        }
        /* after finally printing all link status, get out */
        if (print_flag == 1)
            break;

        if (all_ports_up == 0) {
            printf(".");
            fflush(stdout);
            rte_delay_ms(CHECK_INTERVAL);
        }

        /* set the print_flag if all ports up or timeout */
        if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
            print_flag = 1;
            printf(" ... done\n");
        }
    }
}


int xran_ethdi_init_dpdk_io(char *name, const struct xran_io_loop_cfg *io_cfg,
    int *lcore_id, struct ether_addr *p_lls_cu_addr, struct ether_addr *p_ru_addr,
    uint16_t cp_vlan, uint16_t up_vlan)
{
    uint16_t port[2] = {0xffff, 0xffff};
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    int i;
    char core_mask[64];
    long c_mask=0;
    char bbdev_wdev[32] = "";
    char bbdev_vdev[32] = "";

    char *argv[] = { name, /*"-c 0xFFFFF00000FFFFF"*/core_mask, "-n2", "--socket-mem=8192", "--proc-type=auto",
        "--file-prefix", name, "-w", "0000:00:00.0", bbdev_wdev, bbdev_vdev};

    if (io_cfg == NULL)
        return 0;
    if(io_cfg->bbdev_mode != XRAN_BBDEV_NOT_USED){
        printf("BBDEV_FEC_ACCL_NR5G\n");
        if (io_cfg->bbdev_mode == XRAN_BBDEV_MODE_HW_ON){
            // hw-accelerated bbdev
            printf("hw-accelerated bbdev %s\n", io_cfg->bbdev_dev[0]);
            sprintf(bbdev_wdev, "-w %s", io_cfg->bbdev_dev[0]);
        } else if (io_cfg->bbdev_mode == XRAN_BBDEV_MODE_HW_OFF){
            // hw-accelerated bbdev disable
            if(io_cfg->bbdev_dev[0]){
                printf("hw-accelerated bbdev disable %s\n", io_cfg->bbdev_dev[0]);
                sprintf(bbdev_wdev, "-b %s", io_cfg->bbdev_dev[0]);
            }
            sprintf(bbdev_wdev, "%s", "--vdev=baseband_turbo_sw");
        } else {
            rte_panic("Cannot init DPDK incorrect [bbdev_mode %d]\n", io_cfg->bbdev_mode);
        }
    }

    c_mask = (long)(1L << io_cfg->core) |
            (long)(1L << io_cfg->system_core) |
            (long)(1L << io_cfg->pkt_proc_core) |
            (long)(1L << io_cfg->pkt_aux_core) |
            (long)(1L << io_cfg->timing_core);

    printf("c_mask 0x%lx core %d system_core %d pkt_proc_core %d pkt_aux_core %d timing_core %d\n",
        c_mask, io_cfg->core, io_cfg->system_core, io_cfg->pkt_proc_core, io_cfg->pkt_aux_core, io_cfg->timing_core);

    snprintf(core_mask, sizeof(core_mask), "-c 0x%lx", c_mask);

    ctx->io_cfg = *io_cfg;
    ctx->ping_state           = PING_IDLE;
    ctx->known_peers          = 1;
    ctx->busy_poll_till = rte_rdtsc();
    ctx->cp_vtag = cp_vlan;
    ctx->up_vtag = up_vlan;

    for (i = 0; i <= ID_BROADCAST; i++)     /* Initialize all as broadcast */
        memset(&ctx->entities[i], 0xFF, sizeof(ctx->entities[0]));

    printf("%s: Calling rte_eal_init:", __FUNCTION__);
    for (i = 0; i < RTE_DIM(argv); i++)
    {
        printf("%s ", argv[i]);
    }
    printf("\n");


    /* This will return on system_core, which is not necessarily the
     * one we're on right now. */
    if (rte_eal_init(RTE_DIM(argv), argv) < 0)
        rte_panic("Cannot init EAL: %s\n", rte_strerror(rte_errno));

    xran_init_mbuf_pool();

#ifdef RTE_LIBRTE_PDUMP
    /* initialize packet capture framework */
    rte_pdump_init(NULL);
#endif

    /* Timers. */
    rte_timer_subsystem_init();
    rte_timer_init(&ctx->timer_ping);
    rte_timer_init(&ctx->timer_sync);
    rte_timer_init(&ctx->timer_tx);

    *lcore_id = rte_get_next_lcore(rte_lcore_id(), 0, 0);

    PANIC_ON(*lcore_id == RTE_MAX_LCORE, "out of lcores for io_loop()");

    if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
        for (i = 0; i < ETHDI_VF_MAX; i ++){
            if(io_cfg->dpdk_dev[i]){
                if (rte_eth_dev_attach(io_cfg->dpdk_dev[i], &port[i]) != 0 ||
                    rte_eth_dev_count_avail() == 0)
                    errx(1, "Network port doesn't exist.");
                xran_init_port(port[i], p_lls_cu_addr);
            } else {
                printf("no DPDK port provided\n");
            }
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
            if(io_cfg->dpdk_dev[i])
                check_port_link_status(port[i]);
        }
    } else {
        rte_panic("ethdi_dpdk_io_loop() failed to start  with RTE_PROC_SECONDARY\n");
    }
    PANIC_ON(ctx->tx_ring == NULL, "failed to allocate tx ring");
    PANIC_ON(ctx->rx_ring == NULL, "failed to allocate rx ring");
    PANIC_ON(ctx->pkt_dump_ring == NULL, "failed to allocate pkt dumping ring");
    for (i = 0; i < ETHDI_VF_MAX; i++){
        ctx->io_cfg.port[i] = port[i];
        print_dbg("port_id 0x%04x\n", ctx->io_cfg.port[i]);
    }

    if(io_cfg->dpdk_dev[ETHDI_UP_VF]){
        rte_eth_macaddr_get(port[ETHDI_UP_VF], &ctx->entities[io_cfg->id]);
        ether_addr_copy(p_ru_addr,  &ctx->entities[ID_RU]);
    }

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

int32_t process_dpdk_io(void)
{
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    const struct xran_io_loop_cfg *const cfg = &(xran_ethdi_get_ctx()->io_cfg);
    const int port[ETHDI_VF_MAX] = {cfg->port[ETHDI_UP_VF], cfg->port[ETHDI_CP_VF]};
    int port_id = 0;

    for (port_id = 0; port_id < ETHDI_VF_MAX; port_id++){
        struct rte_mbuf *mbufs[BURST_RX_IO_SIZE];
        if(port[port_id] == 0xFF)
            return 0;
        /* RX */
        const uint16_t rxed = rte_eth_rx_burst(port[port_id], 0, mbufs, BURST_RX_IO_SIZE);
        if (rxed != 0){
            unsigned enq_n = 0;
            long t1 = MLogTick();
            enq_n =  rte_ring_enqueue_burst(ctx->rx_ring[port_id], (void*)mbufs, rxed, NULL);
            if(rxed - enq_n)
                rte_panic("error enq\n");
            MLogTask(PID_RADIO_RX_VALIDATE, t1, MLogTick());
        }

        /* TX */
        const uint16_t sent = xran_tx_from_ring(port[port_id], ctx->tx_ring[port_id]);

        if (XRAN_STOPPED == xran_if_current_state)
            return -1;
    }

    if (XRAN_STOPPED == xran_if_current_state)
            return -1;

    return 0;
}

#if 0
static inline void xran_process_rx_burst(struct rte_mbuf *mbufs[], uint16_t n_mbufs,
        uint64_t rx_time)
{
        int i;

        if (!n_mbufs)
            return;

        for (i = 0; i < n_mbufs; ++i)
        {
            if (xran_ethdi_filter_packet(mbufs[i], rx_time) == MBUF_FREE)
                rte_pktmbuf_free(mbufs[i]);
        }

#ifdef DPDKIO_LATENCY_DEBUG
       struct timeval tv_now, tv_diff;

       gettimeofday(&tv_now, NULL);
       if (n_mbufs > 1)
           nlog("Warning - received %d mbufs in a row", n_mbufs);

       timersub(&tv_now, &rx_time, &tv_diff);
       nlog("rx processing took %d usec", tv_diff.tv_usec);
#endif
}

/*
 * This is the main DPDK-IO loop.
 * This will sleep if there's no packets incoming and there's
 * no work enqueued, sleep lenth is defined in IDLE_SLEEP_MICROSECS
 */
int xran_ethdi_dpdk_io_loop(void *io_loop_cfg)
{
    struct sched_param sched_param;
    int res = 0;
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    const struct xran_io_loop_cfg *const cfg = &(xran_ethdi_get_ctx()->io_cfg);
    const int port[ETHDI_VF_MAX] = {cfg->port[ETHDI_UP_VF], cfg->port[ETHDI_CP_VF]};

    printf("%s [PORT: %d %d] [CPU %2d] [PID: %6d]\n", __FUNCTION__, port[ETHDI_UP_VF], port[ETHDI_CP_VF] , rte_lcore_id(), getpid());

    printf("%s [CPU %2d] [PID: %6d]\n", __FUNCTION__,  rte_lcore_id(), getpid());
    sched_param.sched_priority = XRAN_THREAD_DEFAULT_PRIO;
    if ((res = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched_param))) {
        printf("priority is not changed: coreId = %d, result1 = %d\n",rte_lcore_id(), res);
    }

    for (;;){
        if(process_dpdk_io()!=0)
            break;
    }

    fflush(stderr);
    fflush(stdout);
    puts("IO loop finished");

    return 0;
}
#endif

