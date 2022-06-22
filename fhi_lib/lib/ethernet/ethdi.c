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
#include <immintrin.h>
#include <numa.h>
#include <rte_config.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_malloc.h>
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
#include "xran_common.h"

#include "xran_lib_mlog_tasks_id.h"

#define BURST_RX_IO_SIZE 48

//#define ORAN_OWD_DEBUG_TX_LOOP

struct xran_ethdi_ctx g_ethdi_ctx = { 0 };
enum xran_if_state xran_if_current_state = XRAN_STOPPED;

struct rte_mbuf *xran_ethdi_mbuf_alloc(void)
{
    return rte_pktmbuf_alloc(_eth_mbuf_pool);
}

struct rte_mbuf *xran_ethdi_mbuf_indir_alloc(void)
{
    return rte_pktmbuf_alloc(socket_indirect_pool);
}

int32_t xran_ethdi_mbuf_send(struct rte_mbuf *mb, uint16_t ethertype, uint16_t vf_id)
{
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    int res = 0;

    mb->port = ctx->io_cfg.port[vf_id];
    xran_add_eth_hdr_vlan(&ctx->entities[vf_id][ID_O_RU], ethertype, mb);

    res = xran_enqueue_mbuf(mb, ctx->tx_ring[vf_id]);
    return res;
}

int32_t xran_ethdi_mbuf_send_cp(struct rte_mbuf *mb, uint16_t ethertype, uint16_t vf_id)
{
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    int res = 0;

    mb->port = ctx->io_cfg.port[vf_id];
    xran_add_eth_hdr_vlan(&ctx->entities[vf_id][ID_O_RU], ethertype, mb);

    res = xran_enqueue_mbuf(mb, ctx->tx_ring[vf_id]);
    return res;
}

struct {
    uint16_t ethertype;
    ethertype_handler fn;
} xran_ethertype_handlers[] = {
    { ETHER_TYPE_ECPRI, NULL },
};

int32_t xran_register_ethertype_handler(uint16_t ethertype, ethertype_handler callback)
{
    int i;

    for (i = 0; i < RTE_DIM(xran_ethertype_handlers); ++i)
        if (xran_ethertype_handlers[i].ethertype == ethertype) {
            xran_ethertype_handlers[i].fn = callback;

            return 1;
        }

    print_err("support for ethertype %u not found", ethertype);

    return 0;
}

int xran_handle_ether(uint16_t ethertype, struct rte_mbuf* pkt_q[], uint16_t xport_id, struct xran_eaxc_info *p_cid,  uint16_t num)
{
    int i;

    for (i = 0; i < RTE_DIM(xran_ethertype_handlers); ++i)
        if (xran_ethertype_handlers[i].ethertype == ethertype)
            if (xran_ethertype_handlers[i].fn){
//                rte_prefetch0(rte_pktmbuf_mtod(pkt, void *));
                return xran_ethertype_handlers[i].fn(pkt_q, xport_id, p_cid, num);
            }

    print_err("Packet with unrecognized ethertype '%.4X' dropped", ethertype);

    return MBUF_FREE;
};


/* Process vlan tag. Cut the ethernet header. Call the etherype handlers. */
int xran_ethdi_filter_packet(struct rte_mbuf *pkt_q[], uint16_t vf_id, uint16_t q_id, uint16_t num)
{
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    uint16_t port_id = ctx->vf2xran_port[vf_id];
    struct xran_eaxc_info *p_cid = &ctx->vf_and_q2cid[vf_id][q_id];

    xran_handle_ether(ETHER_TYPE_ECPRI, pkt_q, port_id, p_cid, num);

    return MBUF_FREE;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void check_port_link_status(uint8_t portid)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
    uint8_t count, all_ports_up, print_flag = 0;
    struct rte_eth_link link;

    printf("\nChecking link status portid [%d]  ",  portid);
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

/**
 * create a flow rule that sends packets with matching pc_id
 * to selected queue.
 *
 * @param port_id
 *   The selected port.
 * @param rx_q
 *   The selected target queue.
 * @param pc_id_be
 *   The value to apply to the pc_id.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   A flow if the rule could be created else return NULL.
 */
struct rte_flow *
generate_ecpri_flow(uint16_t port_id, uint16_t rx_q, uint16_t pc_id_be, struct rte_flow_error *error)
{
    struct rte_flow *flow = NULL;
#if (RTE_VER_YEAR >= 21)
#define MAX_PATTERN_NUM		3
#define MAX_ACTION_NUM		2
    struct rte_flow_attr attr;
    struct rte_flow_item pattern[MAX_PATTERN_NUM];
    struct rte_flow_action action[MAX_ACTION_NUM];

    struct rte_flow_action_queue queue = { .index = rx_q };
    struct rte_flow_item_ecpri ecpri_spec;
    struct rte_flow_item_ecpri ecpri_mask;

    int res;
    print_dbg("%s\n", __FUNCTION__);
    memset(pattern, 0, sizeof(pattern));
    memset(action, 0, sizeof(action));

    /*
     * set the rule attribute.
     * in this case only ingress packets will be checked.
     */
    memset(&attr, 0, sizeof(struct rte_flow_attr));
    attr.ingress = 1;

    /*
     * create the action sequence.
     * one action only,  move packet to queue
     */
    action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
    action[0].conf = &queue;
    action[1].type = RTE_FLOW_ACTION_TYPE_END;

    /*
     * set the first level of the pattern (ETH).
     * since in this example we just want to get the
     * eCPRI we set this level to allow all.
     */
    pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;

    memset(&ecpri_spec, 0, sizeof(struct rte_flow_item_ecpri));
    memset(&ecpri_mask, 0, sizeof(struct rte_flow_item_ecpri));

    ecpri_spec.hdr.common.type = RTE_ECPRI_MSG_TYPE_IQ_DATA;
    ecpri_spec.hdr.type0.pc_id = pc_id_be;

    ecpri_mask.hdr.common.type  = 0xff;
    ecpri_mask.hdr.type0.pc_id  = 0xffff;

    ecpri_spec.hdr.common.u32   = rte_cpu_to_be_32(ecpri_spec.hdr.common.u32);

    pattern[1].type = RTE_FLOW_ITEM_TYPE_ECPRI;
    pattern[1].spec = &ecpri_spec;
    pattern[1].mask = &ecpri_mask;

    struct rte_flow_item_ecpri *pecpri_spec = (struct rte_flow_item_ecpri *)pattern[1].spec;
    struct rte_flow_item_ecpri *pecpri_mask = (struct rte_flow_item_ecpri *)pattern[1].mask;
    print_dbg("RTE_FLOW_ITEM_TYPE_ECPRI\n");
    print_dbg("spec type %x pc_id %x\n", pecpri_spec->hdr.common.type, pecpri_spec->hdr.type0.pc_id);
    print_dbg("mask type %x pc_id %x\n", pecpri_mask->hdr.common.type, pecpri_mask->hdr.type0.pc_id);

    /* the final level must be always type end */
    pattern[2].type = RTE_FLOW_ITEM_TYPE_END;

    res = rte_flow_validate(port_id, &attr, pattern, action, error);
    if (!res)
        flow = rte_flow_create(port_id, &attr, pattern, action, error);
    else {
        rte_panic("Flow can't be created %d message: %s\n",
                    error->type,
                    error->message ? error->message : "(no stated reason)");
    }
#endif
    return flow;
}


int32_t
xran_ethdi_init_dpdk_io(char *name, const struct xran_io_cfg *io_cfg,
    int *lcore_id, struct rte_ether_addr *p_o_du_addr,
    struct rte_ether_addr *p_ru_addr,  uint32_t mtu)
{
    uint16_t port[XRAN_VF_MAX];
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    int i,ivf;
    char core_mask[64];
    uint64_t c_mask         = 0L;
    uint64_t c_mask_64_127  = 0L;
    uint64_t nWorkerCore = 1;
    uint32_t coreNum = sysconf(_SC_NPROCESSORS_CONF);
    char bbdev_wdev[32]   = "";
    char bbdev_vdev[32]   = "";
    char iova_mode[32]    = "--iova-mode=pa";
    char socket_mem[32]   = "--socket-mem=8192";
    char socket_limit[32] = "--socket-limit=8192";
    char ring_name[32]    = "";
    int32_t xran_port = -1;
    queueid_t qi = 0;
    uint32_t cpu = 0;
    uint32_t node = 0;

    cpu = sched_getcpu();
    node = numa_node_of_cpu(cpu);

    char *argv[] = { name, core_mask, "-n2", iova_mode, socket_mem, socket_limit, "--proc-type=auto",
        "--file-prefix", name, "-a0000:00:00.0", bbdev_wdev, bbdev_vdev};

    if (io_cfg == NULL)
        return 0;
    if(io_cfg->bbdev_mode != XRAN_BBDEV_NOT_USED){
        printf("BBDEV_FEC_ACCL_NR5G\n");
        if (io_cfg->bbdev_mode == XRAN_BBDEV_MODE_HW_ON){
            // hw-accelerated bbdev
            printf("hw-accelerated bbdev %s\n", io_cfg->bbdev_dev[0]);
            snprintf(bbdev_wdev, RTE_DIM(bbdev_wdev), "-a%s", io_cfg->bbdev_dev[0]);
        } else if (io_cfg->bbdev_mode == XRAN_BBDEV_MODE_HW_OFF){
            snprintf(bbdev_wdev, RTE_DIM(bbdev_wdev), "%s", "--vdev=baseband_turbo_sw");
        } else if (io_cfg->bbdev_mode == XRAN_BBDEV_MODE_HW_SW){
            printf("software and hw-accelerated bbdev %s\n", io_cfg->bbdev_dev[0]);
            snprintf(bbdev_wdev, RTE_DIM(bbdev_wdev), "-a%s", io_cfg->bbdev_dev[0]);
            snprintf(bbdev_vdev, RTE_DIM(bbdev_vdev), "%s", "--vdev=baseband_turbo_sw");
        } else {
            rte_panic("Cannot init DPDK incorrect [bbdev_mode %d]\n", io_cfg->bbdev_mode);
        }
    }

    if (io_cfg->dpdkIoVaMode == 1){
        snprintf(iova_mode, RTE_DIM(iova_mode), "%s", "--iova-mode=va");
    }

    if (io_cfg->dpdkMemorySize){
        printf("node %d\n", node);
        if (node == 1){
            snprintf(socket_mem, RTE_DIM(socket_mem), "--socket-mem=0,%d", io_cfg->dpdkMemorySize);
            snprintf(socket_limit, RTE_DIM(socket_limit), "--socket-limit=0,%d", io_cfg->dpdkMemorySize);
        } else {
            snprintf(socket_mem, RTE_DIM(socket_mem), "--socket-mem=%d,0", io_cfg->dpdkMemorySize);
            snprintf(socket_limit, RTE_DIM(socket_limit), "--socket-limit=%d,0", io_cfg->dpdkMemorySize);
        }
    }

    if (io_cfg->core < 64)
        c_mask |= (long)(1L << io_cfg->core);
    else
        c_mask_64_127 |= (long)(1L << (io_cfg->core - 64));

    if (io_cfg->system_core < 64)
        c_mask |= (long)(1L << io_cfg->system_core);
    else
        c_mask_64_127 |= (long)(1L << (io_cfg->system_core - 64));

    if (io_cfg->timing_core < 64)
       c_mask |=  (long)(1L << io_cfg->timing_core);
    else
       c_mask_64_127 |= (long)(1L << (io_cfg->timing_core - 64));

    nWorkerCore = 1L;
    for (i = 0; i < coreNum && i < 64; i++) {
        if (nWorkerCore & (uint64_t)io_cfg->pkt_proc_core) {
            c_mask |= nWorkerCore;
        }
        nWorkerCore = nWorkerCore << 1;
    }

    nWorkerCore = 1L;
    for (i = 64; i < coreNum && i < 128; i++) {
        if (nWorkerCore & (uint64_t)io_cfg->pkt_proc_core_64_127) {
            c_mask_64_127 |= nWorkerCore;
        }
        nWorkerCore = nWorkerCore << 1;
    }

    printf("total cores %d c_mask 0x%lx%016lx core %d [id] system_core %d [id] pkt_proc_core 0x%lx%016lx [mask] pkt_aux_core %d [id] timing_core %d [id]\n",
        coreNum, c_mask_64_127, c_mask, io_cfg->core, io_cfg->system_core, io_cfg->pkt_proc_core_64_127, io_cfg->pkt_proc_core, io_cfg->pkt_aux_core, io_cfg->timing_core);

    snprintf(core_mask, sizeof(core_mask), "-c 0x%lx%016lx",c_mask_64_127,c_mask);

    ctx->io_cfg = *io_cfg;

    for (ivf = 0; ivf < XRAN_VF_MAX; ivf++){
        for (i = 0; i < ID_MAX; i++)     /* Initialize all as broadcast */
            memset(&ctx->entities[ivf][i], 0xFF, sizeof(ctx->entities[0][0]));
    }

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

    if (rte_eal_process_type() == RTE_PROC_SECONDARY)
        rte_exit(EXIT_FAILURE,
                "Secondary process type not supported.\n");

    xran_init_mbuf_pool(mtu);

#ifdef RTE_LIBRTE_PDUMP
    /* initialize packet capture framework */
    rte_pdump_init();
#endif

    /* Timers. */
    rte_timer_subsystem_init();

    *lcore_id = rte_get_next_lcore(rte_lcore_id(), 0, 0);

    PANIC_ON(*lcore_id == RTE_MAX_LCORE, "out of lcores for io_loop()");

    for (i = 0; i < XRAN_VF_MAX; i++)
        port[i] = 0xffff;

    if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
        for (i = 0; i < XRAN_VF_MAX && i < io_cfg->num_vfs; i++){
            if(io_cfg->dpdk_dev[i]){
                struct rte_dev_iterator iterator;
                uint16_t port_id;

                if (rte_dev_probe(io_cfg->dpdk_dev[i]) != 0 ||
                    rte_eth_dev_count_avail() == 0) {
                        errx(1, "Network port doesn't exist\n");
                }

                RTE_ETH_FOREACH_MATCHING_DEV(port_id, io_cfg->dpdk_dev[i], &iterator){
                        port[i] = port_id;
                        xran_init_port(port[i], io_cfg->num_rxq, mtu);
            }

                if(!(i & 1) || io_cfg->one_vf_cu_plane){
                snprintf(ring_name, RTE_DIM(ring_name), "%s_%d", "tx_ring_up", i);
                    ctx->tx_ring[i] = rte_ring_create(ring_name, NUM_MBUFS_RING_TRX,
                    rte_lcore_to_socket_id(*lcore_id), RING_F_SC_DEQ);
                    PANIC_ON(ctx->tx_ring[i] == NULL, "failed to allocate tx ring");
                    for(qi = 0; qi < io_cfg->num_rxq; qi++) {
                        snprintf(ring_name, RTE_DIM(ring_name), "%s_%d_%d", "rx_ring_up", i, qi);
                        ctx->rx_ring[i][qi] = rte_ring_create(ring_name, NUM_MBUFS_RING_TRX,
                            rte_lcore_to_socket_id(*lcore_id), RING_F_SP_ENQ);
                        PANIC_ON(ctx->rx_ring[i][qi] == NULL, "failed to allocate rx ring");
                    }
            }else {
                snprintf(ring_name, RTE_DIM(ring_name), "%s_%d", "tx_ring_cp", i);
                    ctx->tx_ring[i] = rte_ring_create(ring_name, NUM_MBUFS_RING_TRX,
                    rte_lcore_to_socket_id(*lcore_id), RING_F_SC_DEQ);
                    PANIC_ON(ctx->tx_ring[i] == NULL, "failed to allocate rx ring");
                    for(qi = 0; qi < io_cfg->num_rxq; qi++) {
                        snprintf(ring_name, RTE_DIM(ring_name), "%s_%d_%d", "rx_ring_cp", i, qi);
                        ctx->rx_ring[i][qi] = rte_ring_create(ring_name, NUM_MBUFS_RING_TRX,
                            rte_lcore_to_socket_id(*lcore_id), RING_F_SP_ENQ);
                        PANIC_ON(ctx->rx_ring[i][qi] == NULL, "failed to allocate rx ring");
                    }
                }
            } else {
                printf("no DPDK port provided\n");
                xran_init_port_mempool(i, mtu);
            }

            if(io_cfg->dpdk_dev[i]){
                check_port_link_status(port[i]);
            }
        }
    } else {
        rte_panic("ethdi_dpdk_io_loop() failed to start  with RTE_PROC_SECONDARY\n");
    }

    for (i = 0; i < XRAN_VF_MAX && i < io_cfg->num_vfs; i++){
        ctx->io_cfg.port[i] = port[i];
        print_dbg("port_id 0x%04x\n", ctx->io_cfg.port[i]);
    }

    for (i = 0; i < XRAN_VF_MAX; i++){
        ctx->vf2xran_port[i] = 0xFFFF;
        ctx->rxq_per_port[i] = 1;
        for (qi = 0; qi < XRAN_VF_QUEUE_MAX; qi++){
            ctx->vf_and_q2pc_id[i][qi] = 0xFFFF;

            ctx->vf_and_q2cid[i][qi].cuPortId     = 0xFF;
            ctx->vf_and_q2cid[i][qi].bandSectorId = 0xFF;
            ctx->vf_and_q2cid[i][qi].ccId         = 0xFF;
            ctx->vf_and_q2cid[i][qi].ruPortId     = 0xFF;
        }
    }

    for (i = 0; i < XRAN_VF_MAX && i < io_cfg->num_vfs; i++){
        if(io_cfg->dpdk_dev[i]){
            struct rte_ether_addr *p_addr;

            if(i % (io_cfg->nEthLinePerPort * (2 - 1*ctx->io_cfg.one_vf_cu_plane)) == 0) /* C-p and U-p VFs per line */
                xran_port +=1;

            rte_eth_macaddr_get(port[i], &ctx->entities[i][io_cfg->id]);

            p_addr = &ctx->entities[i][io_cfg->id];
            printf("[%2d] vf %2u local  SRC MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
                   " %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
                   (unsigned)xran_port,
                   (unsigned)i,
                   p_addr->addr_bytes[0], p_addr->addr_bytes[1], p_addr->addr_bytes[2],
                   p_addr->addr_bytes[3], p_addr->addr_bytes[4], p_addr->addr_bytes[5]);

            p_addr = &p_ru_addr[i];
            printf("[%2d] vf %2u remote DST MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
                   " %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
                   (unsigned)xran_port,
                   (unsigned)i,
                   p_addr->addr_bytes[0], p_addr->addr_bytes[1], p_addr->addr_bytes[2],
                   p_addr->addr_bytes[3], p_addr->addr_bytes[4], p_addr->addr_bytes[5]);

            rte_ether_addr_copy(&p_ru_addr[i],  &ctx->entities[i][ID_O_RU]);
            ctx->vf2xran_port[i] = xran_port;
            ctx->rxq_per_port[i] = io_cfg->num_rxq;
        }
    }

    for(i = 0; i < xran_port + 1 && i < XRAN_PORTS_NUM; i++) {
        snprintf(ring_name, RTE_DIM(ring_name), "%s_%d", "dl_gen_ring_up", i);
        ctx->up_dl_pkt_gen_ring[i] = rte_ring_create(ring_name, NUM_MBUFS_RING,
        rte_lcore_to_socket_id(*lcore_id), /*RING_F_SC_DEQ*/0);
        PANIC_ON(ctx->up_dl_pkt_gen_ring[i] == NULL, "failed to allocate dl gen ring");
        printf("created %s\n", ring_name);
    }

    return 1;
}

static inline uint16_t xran_tx_from_ring(int port, struct rte_ring *r)
{
    struct rte_mbuf *mbufs[BURST_SIZE];
    uint16_t dequeued, sent = 0;
    uint32_t remaining;
    long t1 = MLogXRANTick();

    dequeued = rte_ring_dequeue_burst(r, (void **)mbufs, BURST_SIZE,
            &remaining);
    if (!dequeued)
        return 0;   /* Nothing to send. */

    while (1) {     /* When tx queue is full it is trying again till succeed */
        sent += rte_eth_tx_burst(port, 0, &mbufs[sent], dequeued - sent);
        if (sent == dequeued){
            MLogXRANTask(PID_RADIO_ETH_TX_BURST, t1, MLogXRANTick());
            return remaining;
    }
}
}

int32_t process_dpdk_io(void* args)
{
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    struct xran_io_cfg * cfg = &(xran_ethdi_get_ctx()->io_cfg);
    int32_t* port = &cfg->port[0];
    int port_id = 0;
    int qi      = 0;

    rte_timer_manage();

    for (port_id = 0; port_id < XRAN_VF_MAX && port_id < ctx->io_cfg.num_vfs; port_id++){
        struct rte_mbuf *mbufs[BURST_RX_IO_SIZE];
        if(port[port_id] == 0xFF)
            return 0;

        /* RX */
        for(qi = 0; qi < ctx->rxq_per_port[port_id]; qi++) {
            const uint16_t rxed = rte_eth_rx_burst(port[port_id], qi, mbufs, BURST_RX_IO_SIZE);
        if (rxed != 0){
            unsigned enq_n = 0;
                long t1 = MLogXRANTick();
                ctx->rx_vf_queue_cnt[port[port_id]][qi] += rxed;
                enq_n =  rte_ring_enqueue_burst(ctx->rx_ring[port_id][qi], (void*)mbufs, rxed, NULL);
            if(rxed - enq_n)
                rte_panic("error enq\n");
                MLogXRANTask(PID_RADIO_RX_VALIDATE, t1, MLogXRANTick());
        }
        }

        /* TX */

        xran_tx_from_ring(port[port_id], ctx->tx_ring[port_id]);
        /* One way Delay Measurements */
        if ((cfg->eowd_cmn[cfg->id].owdm_enable != 0) && (cfg->eowd_cmn[cfg->id].measVf == port_id))
        {
          if (!xran_ecpri_port_update_required(cfg, (uint16_t)port_id))
            {
#ifdef ORAN_OWD_DEBUG_TX_LOOP
                printf("going to owd tx for port %d\n", port_id);
#endif
                if (xran_ecpri_one_way_delay_measurement_transmitter((uint16_t) port_id, (void*)xran_dev_get_ctx()) != OK)
                {
                    errx(1,"Exit pdio port_id %d", port_id);
                }
            }
        }

        if (XRAN_STOPPED == xran_if_current_state)
            return -1;
    }

    if (XRAN_STOPPED == xran_if_current_state)
            return -1;

    return 0;
}

int32_t process_dpdk_io_tx(void* args)
{
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    struct xran_io_cfg * cfg = &(xran_ethdi_get_ctx()->io_cfg);
    int32_t* port = &cfg->port[0];
    int port_id = 0;

    //rte_timer_manage();

    for (port_id = 0; port_id < XRAN_VF_MAX && port_id < ctx->io_cfg.num_vfs; port_id++){
        if(port[port_id] == 0xFF)
            return 0;
        /* TX */
        xran_tx_from_ring(port[port_id], ctx->tx_ring[port_id]);

        if (XRAN_STOPPED == xran_if_current_state)
            return -1;
    }

    if (XRAN_STOPPED == xran_if_current_state)
            return -1;

    return 0;
}

int32_t process_dpdk_io_rx(void* args)
{
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    struct xran_io_cfg * cfg = &(xran_ethdi_get_ctx()->io_cfg);
    int32_t* port = &cfg->port[0];
    int port_id = 0;
    int qi     = 0;

    rte_timer_manage();

    if (XRAN_RUNNING != xran_if_current_state)
            return 0;

    for (port_id = 0; port_id < XRAN_VF_MAX && port_id < ctx->io_cfg.num_vfs; port_id++){
        struct rte_mbuf *mbufs[BURST_RX_IO_SIZE];
        if(port[port_id] == 0xFF)
            return 0;

        /* RX */
        for(qi = 0; qi < ctx->rxq_per_port[port_id]; qi++){
            const uint16_t rxed = rte_eth_rx_burst(port[port_id], qi, mbufs, BURST_RX_IO_SIZE);
            if (rxed != 0){
                unsigned enq_n = 0;
                long t1 = MLogXRANTick();
                ctx->rx_vf_queue_cnt[port[port_id]][qi] += rxed;
                enq_n =  rte_ring_enqueue_burst(ctx->rx_ring[port_id][qi], (void*)mbufs, rxed, NULL);
                if(rxed - enq_n)
                    rte_panic("error enq\n");
                MLogXRANTask(PID_RADIO_RX_VALIDATE, t1, MLogXRANTick());
            }
        }
        if (XRAN_STOPPED == xran_if_current_state)
            return -1;
    }

    if (XRAN_STOPPED == xran_if_current_state)
            return -1;

    return 0;
}

