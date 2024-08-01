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
 * @file xran_ethdi.c
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

#include "xran_ethernet.h"
#include "xran_ethdi.h"
#include "xran_fh_o_du.h"
#include "xran_mlog_lnx.h"
#include "xran_printf.h"
#include "xran_common.h"

#include "xran_lib_mlog_tasks_id.h"

#define BURST_RX_IO_SIZE 48

//#define ORAN_OWD_DEBUG_TX_LOOP

struct xran_ethdi_ctx g_ethdi_ctx = { 0 };
enum xran_if_state xran_if_current_state = XRAN_STOPPED;

extern inline struct xran_ethdi_ctx *xran_ethdi_get_ctx(void);

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

int xran_handle_ether(uint16_t ethertype, struct rte_mbuf* pkt_q[], uint16_t xport_id, struct xran_eaxc_info *p_cid,  uint16_t num, uint16_t vf_id)
{
    int i;

    for (i = 0; i < RTE_DIM(xran_ethertype_handlers); ++i)
    {
        if (xran_ethertype_handlers[i].ethertype == ethertype &&
            xran_ethertype_handlers[i].fn)
        {
                return xran_ethertype_handlers[i].fn(pkt_q, xport_id, p_cid, num, vf_id);
        }
    }

    for(i=0;i<num;i++)
    {
        rte_pktmbuf_free(pkt_q[i]);
    }

    struct xran_device_ctx* p_dev_ctx = xran_dev_get_ctx_by_id(xport_id);
    if (likely(p_dev_ctx != NULL)){
        p_dev_ctx->fh_counters.rx_err_drop += num;
    }
    print_dbg("Packet with unrecognized ethertype '%.4X' dropped", ethertype);

    return MBUF_FREE;
};


/* Process vlan tag. Cut the ethernet header. Call the etherype handlers. */
int xran_ethdi_filter_packet(struct rte_mbuf *pkt_q[], uint16_t vf_id, uint16_t q_id, uint16_t num)
{
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    uint16_t port_id = ctx->vf2xran_port[vf_id];
    struct xran_eaxc_info *p_cid = &ctx->vf_and_q2cid[vf_id][q_id];

    xran_handle_ether(ETHER_TYPE_ECPRI, pkt_q, port_id, p_cid, num, vf_id);

    return MBUF_FREE;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void check_port_link_status(uint8_t portid)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
    uint8_t count, print_flag = 0;
    struct rte_eth_link link;

    printf("\nChecking link status portid [%d]  ",  portid);
    fflush(stdout);


    for (count = 0; count <= MAX_CHECK_TIME; count++) {
        memset(&link, 0, sizeof(link));
        rte_eth_link_get_nowait(portid, &link);

        /* print link status if flag set */
        if (print_flag == 1) {
            if (link.link_status)
                printf("Port %d Link Up - speed %u "
                        "Mbps - %s\n", (uint8_t)portid,
                        (unsigned)link.link_speed,
                        (link.link_duplex == RTE_ETH_LINK_FULL_DUPLEX) ?
                        ("full-duplex") : ("half-duplex\n"));
            else
                printf("Port %d Link Down\n",
                        (uint8_t)portid);
        }

        if (link.link_status == RTE_ETH_LINK_DOWN) {
            printf(".");
            fflush(stdout);
            rte_delay_ms(CHECK_INTERVAL);
            break;
        }

        /* after finally printing all link status, get out */
        if (print_flag == 1)
            break;

        /* set the print_flag if all ports up or timeout */
        if (count == (MAX_CHECK_TIME - 1)) {
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

    int32_t res;
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
#ifdef PRINTF_DBG_OK
    struct rte_flow_item_ecpri *pecpri_spec = (struct rte_flow_item_ecpri *)pattern[1].spec;
    struct rte_flow_item_ecpri *pecpri_mask = (struct rte_flow_item_ecpri *)pattern[1].mask;
#endif
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

int32_t xran_get_dpdk_process_tag(struct xran_io_cfg *io_cfg)
{
    if (io_cfg->dpdkProcessType)
    {
        char mz_name[] = "dpdk_process_tag\0";
        struct rte_memzone *mng_memzone;

        if (rte_eal_process_type() == RTE_PROC_PRIMARY)
        {
            mng_memzone = (struct rte_memzone *)rte_memzone_reserve(mz_name, sizeof(uint32_t), rte_socket_id(), 0);
            if (mng_memzone == NULL)
            {
                rte_panic("Cannot reserve memory zone[%s]: %s\n", mz_name, rte_strerror(rte_errno));
                return -1;
            }
            *((uint32_t *)(mng_memzone->addr)) = 0;
            io_cfg->nDpdkProcessID = *((uint32_t *)(mng_memzone->addr));
        }
        else
        {
            mng_memzone = (struct rte_memzone *)rte_memzone_lookup(mz_name);
            if (mng_memzone == NULL)
            {
                rte_panic("Cannot find memory zone[%s]: %s\n", mz_name, rte_strerror(rte_errno));
                return -1;
            }
            *((uint32_t *)(mng_memzone->addr)) = *((uint32_t *)(mng_memzone->addr)) + 1;
        }
        io_cfg->nDpdkProcessID = *((uint32_t *)(mng_memzone->addr));
    }

    return 0;
}



int32_t xran_ethdi_init_dpdk(char *name, char *vfio_name, struct xran_io_cfg *io_cfg)
{
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    int i,ivf;
    char core_mask[64];
    char main_core[32];
    uint64_t c_mask         = 0L;
    uint64_t c_mask_64_127  = 0L;
    uint64_t nWorkerCore = 1;
    uint32_t coreNum = sysconf(_SC_NPROCESSORS_CONF);
    char bbdev_wdev[32]   = "";
    char bbdev_vdev[32]   = "";
    char bbdev_vdev_aux[XRAN_MAX_AUX_BBDEV_NUM][32];
    char vfio_token[64]   = "";
    char iova_mode[32]    = "--iova-mode=pa";
    char socket_mem[32]   = "--socket-mem=8192";
    char socket_limit[32] = "--socket-limit=8192";
    uint32_t cpu = 0;
    uint32_t node = 0;

    cpu = sched_getcpu();
    node = numa_node_of_cpu(cpu);

    char *argv[14 + XRAN_MAX_AUX_BBDEV_NUM] = { name, core_mask, main_core, "-n2", iova_mode, socket_mem, socket_limit, "--proc-type=auto",
        "--file-prefix", name, "-a0000:00:00.0", bbdev_wdev, bbdev_vdev, vfio_token};

    for (i = 0; i < XRAN_MAX_AUX_BBDEV_NUM; i++)
    {
        int32_t j = RTE_DIM(argv)-XRAN_MAX_AUX_BBDEV_NUM + i;
        bbdev_vdev_aux[i][0] = '\0';
        argv[j] = bbdev_vdev_aux[i];
    }

    if((vfio_name)&&(strlen(vfio_name)))
    {
        snprintf(vfio_token, RTE_DIM(vfio_token), "--vfio-vf-token=%s", vfio_name);
    }

    if (io_cfg == NULL)
        return 0;
    if(io_cfg->bbdev_mode != XRAN_BBDEV_NOT_USED){
        printf("BBDEV_FEC_ACCL_NR5G\n");
        if (io_cfg->bbdev_mode == XRAN_BBDEV_MODE_HW_ON){
            // hw-accelerated bbdev
            printf("hw-accelerated bbdev %s\n", io_cfg->bbdev_dev[0]);
            snprintf(bbdev_wdev, RTE_DIM(bbdev_wdev), "-a%s", io_cfg->bbdev_dev[0]);

            if (io_cfg->bbdevx_num)
            {
                for (i = 0; i < io_cfg->bbdevx_num; i++)
                {
                    printf("hw-accelerated aux bbdev %s\n", io_cfg->bbdev_devx[i]);
                    snprintf(bbdev_vdev_aux[i], RTE_DIM(bbdev_vdev_aux[i]), "-a%s", io_cfg->bbdev_devx[i]);
                }
            }
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

    printf("total cores %d c_mask 0x%lx%016lx core %d [id] system_core %d [id] pkt_proc_core 0x%lx%016lx [mask] pkt_aux_core %d [id] timing_core %u [id]\n",
        coreNum, c_mask_64_127, c_mask, io_cfg->core, io_cfg->system_core, io_cfg->pkt_proc_core_64_127, io_cfg->pkt_proc_core, io_cfg->pkt_aux_core, io_cfg->timing_core);

    snprintf(core_mask, sizeof(core_mask), "-c 0x%lx%016lx",c_mask_64_127,c_mask);
    snprintf(main_core, sizeof(main_core), "--main-lcore=%d", io_cfg->system_core);

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

    if(rte_vect_set_max_simd_bitwidth(RTE_VECT_SIMD_512) < 0)
        rte_panic("Cannot init RTE_VECT_SIMD_512: %s\n", rte_strerror(rte_errno));

    /* This will return on system_core, which is not necessarily the
     * one we're on right now. */
    if (rte_eal_init(RTE_DIM(argv), argv) < 0)
        rte_panic("Cannot init EAL: %s\n", rte_strerror(rte_errno));

    if (0 == io_cfg->dpdkProcessType && rte_eal_process_type() == RTE_PROC_SECONDARY)
        rte_exit(EXIT_FAILURE,
                "Secondary process type not supported.\n");

    xran_get_dpdk_process_tag(io_cfg);

#ifdef RTE_LIB_PDUMP
    if (rte_eal_process_type() == RTE_PROC_PRIMARY)
    {
        /* initialize packet capture framework */
        rte_pdump_init();
    }
#endif

    /* Timers. */
    rte_timer_subsystem_init();

    return 1;
}

int32_t xran_ethdi_init_dpdk_ports(struct xran_io_cfg *io_cfg,
    int *lcore_id, struct rte_ether_addr *p_o_du_addr,
    struct rte_ether_addr *p_ru_addr,  uint32_t mtu)
{
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    int i;
    uint16_t port[XRAN_VF_MAX];
    char ring_name[32]    = "";
    int32_t xran_port = -1;
    queueid_t qi = 0;

    xran_init_mbuf_pool(io_cfg, mtu);

    *lcore_id = rte_get_next_lcore(rte_lcore_id(), 0, 1);
    PANIC_ON(*lcore_id == RTE_MAX_LCORE, "out of lcores for io_loop()");

    for (i = 0; i < XRAN_VF_MAX; i++)
        port[i] = 0xffff;

//    if (rte_eal_process_type() == RTE_PROC_PRIMARY) {     // xran_ethdi_init_dpdk() checked this already
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
                    xran_init_port(port[i], io_cfg, mtu);
            }

            if(!(i & 1) || io_cfg->one_vf_cu_plane){
                snprintf(ring_name, RTE_DIM(ring_name), "%s_%d_%d", "tx_ring_up", i, port[i]);
                ctx->tx_ring[i] = rte_ring_create(ring_name, NUM_MBUFS_RING_TRX,
                    rte_lcore_to_socket_id(*lcore_id), RING_F_SC_DEQ);
                PANIC_ON(ctx->tx_ring[i] == NULL, "failed to allocate tx ring");
                for(qi = 0; qi < io_cfg->num_rxq; qi++) {
                    snprintf(ring_name, RTE_DIM(ring_name), "%s_%d_%d_%d", "rx_ring_up", i, qi, port[i]);
                    ctx->rx_ring[i][qi] = rte_ring_create(ring_name, NUM_MBUFS_RING_TRX,
                        rte_lcore_to_socket_id(*lcore_id), RING_F_SP_ENQ);
                    PANIC_ON(ctx->rx_ring[i][qi] == NULL, "failed to allocate rx ring");
                }
            }else {
                snprintf(ring_name, RTE_DIM(ring_name), "%s_%d_%d", "tx_ring_cp", i, port[i]);
                ctx->tx_ring[i] = rte_ring_create(ring_name, NUM_MBUFS_RING_TRX,
                    rte_lcore_to_socket_id(*lcore_id), RING_F_SC_DEQ);
                PANIC_ON(ctx->tx_ring[i] == NULL, "failed to allocate rx ring");
                for(qi = 0; qi < io_cfg->num_rxq; qi++) {
                    snprintf(ring_name, RTE_DIM(ring_name), "%s_%d_%d_%d", "rx_ring_cp", i, qi, port[i]);
                    ctx->rx_ring[i][qi] = rte_ring_create(ring_name, NUM_MBUFS_RING_TRX,
                        rte_lcore_to_socket_id(*lcore_id), RING_F_SP_ENQ);
                    PANIC_ON(ctx->rx_ring[i][qi] == NULL, "failed to allocate rx ring");
                }
            }
        } else {
            printf("no DPDK port provided\n");
            xran_init_port_mempool(i, io_cfg);
        }

        if(io_cfg->dpdk_dev[i]){
            check_port_link_status(port[i]);
        }
    }
//    } else {
//        rte_panic("ethdi_dpdk_io_loop() failed to start  with RTE_PROC_SECONDARY\n");
//    }

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
        snprintf(ring_name, RTE_DIM(ring_name), "%s_%d_%d", "dl_gen_ring_up", i,io_cfg->nDpdkProcessID);
        ctx->up_dl_pkt_gen_ring[i] = rte_ring_create(ring_name, NUM_MBUFS_RING, rte_lcore_to_socket_id(*lcore_id), /*RING_F_SC_DEQ*/0);
        PANIC_ON(ctx->up_dl_pkt_gen_ring[i] == NULL, "failed to allocate dl gen ring");
        printf("created %s\n", ring_name);
    }

    return 1;
}

void xran_update_eth_addr(struct xran_io_cfg *io_cfg, uint32_t RU_port, uint32_t num_vf_port, struct rte_ether_addr *p_o_ru_addr)
{
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    struct rte_ether_addr *p_addr;
    uint32_t i, j = 0;
    for (i = 0; i < XRAN_VF_MAX && i < io_cfg->num_vfs; i++)
    {
        if (ctx->vf2xran_port[i] == RU_port)
        {
            p_addr = &p_o_ru_addr[j];
            printf("updating [%2d] vf %2u remote DST MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
                   " %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
                   RU_port, j,
                   p_addr->addr_bytes[0], p_addr->addr_bytes[1], p_addr->addr_bytes[2],
                   p_addr->addr_bytes[3], p_addr->addr_bytes[4], p_addr->addr_bytes[5]);

            rte_ether_addr_copy(&p_o_ru_addr[j],  &ctx->entities[i][ID_O_RU]);
            j++;
        }
        if (j == num_vf_port)
            return;
    }
    printf("Error: Not found VF ports for RUport %d\n", RU_port);
    return;
}

uint32_t xran_get_RUport_by_MACaddr(uint8_t nSrcMacAddress[6])
{
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    struct rte_ether_addr *p_addr;
    struct xran_system_config *sysCfg = xran_get_systemcfg();
    struct xran_io_cfg *p_io_cfg;
    p_io_cfg = &sysCfg->io_cfg;
    uint32_t i, j, isequal;
    for (i = 0; i < XRAN_VF_MAX && i < p_io_cfg->num_vfs; i++)
    {
        p_addr = &ctx->entities[i][0];
        isequal = 1;
        for (j = 0; j < RTE_ETHER_ADDR_LEN; j++)
        {
            if (p_addr->addr_bytes[j] != nSrcMacAddress[j])
            {
                isequal = 0;
                break;
            }
        }
        if (isequal)
        {
            return ctx->vf2xran_port[i];
        }
    }
    return 0xFFFF;
}

void xran_reset_lbm_ctx(xran_lbm_port_info *lbmPortInfo)
{
    /* Reset stats */
    lbmPortInfo->linkStatus                  = 0;
    lbmPortInfo->LBRreceived                 = 0;
    lbmPortInfo->stats.numTransmittedLBM     = 0;
    lbmPortInfo->stats.rxValidInOrderLBRs    = 0;
    lbmPortInfo->stats.rxValidOutOfOrderLBRs = 0;
    lbmPortInfo->stats.numRxLBRsIgnored      = 0;
    lbmPortInfo->lbm_state                   = XRAN_LBM_STATE_IDLE;
    lbmPortInfo->numRetries                  = 0;
    lbmPortInfo->xmitEnable                  = 0;

    return;
}

/**
 * =============================================================
 * xran_oam_phy2xran_manage_lbm
 * @param[in] lbmEnable: Flag 0/1 disable/enable LBM in xRAN
 *
 * @brief
 *
 * Function to update LBM enable in xRAN via L1 from OAM interface.
 * 
 * =============================================================
 */
int32_t xran_oam_phy2xran_manage_lbm(uint8_t lbmEnable, __attribute__((unused))uint8_t vfId,  __attribute__((unused))uint8_t param3)
{

    struct xran_ethdi_ctx *ethCtx = xran_ethdi_get_ctx();

    if(unlikely(vfId >= XRAN_VF_MAX)){
        print_err("Invalid vfId %hhu", vfId);
        return XRAN_STATUS_INVALID_PARAM;
    }

    if(unlikely(!ethCtx)){
        print_err("ethCtx is NULL");
        return XRAN_STATUS_INVALID_PARAM;
    }

    if(ethCtx->lbm_port_info[vfId].lbm_enable == 1 && lbmEnable == 0) // OAM disables active LBM
    {
        /* First disable then reset ctx */

        ethCtx->lbm_port_info[vfId].lbm_enable = lbmEnable;
        /* Reset LBM ctx for vfId*/
        xran_reset_lbm_ctx(&ethCtx->lbm_port_info[vfId]);
    }
    else if (ethCtx->lbm_port_info[vfId].lbm_enable == 0 && lbmEnable == 1) // OAM turns on inactive LBM
    {
        /* First reset ctx then enable LBM*/
        /* Reset stats for vfId*/
        xran_reset_lbm_ctx(&ethCtx->lbm_port_info[vfId]);

        /*Where can we add this flag for it to be per VF?*/
        ethCtx->lbm_port_info[vfId].lbm_enable = lbmEnable;
    }
    else //State is unchanged no need to reset the context here
    {
        ethCtx->lbm_port_info[vfId].lbm_enable = lbmEnable;
    }

    return XRAN_STATUS_SUCCESS;
}


/**
 * =============================================================
 * xran_lbm_state_machine
 * @param[in] eth_port  : Process LBM for this Ethernet port/VF ID
 *
 * @brief
 *
 * State machine for processing IEEE 802.1Q Connectivity Fault Management
 * LBM (Loop Back Message) transmission
 * 
 * =============================================================
 */
xran_status_t xran_lbm_state_machine(uint8_t eth_port)
{
    struct xran_ethdi_ctx *eth_ctx = xran_ethdi_get_ctx();
    struct xran_system_config *p_syscfg = xran_get_systemcfg();
    struct timespec currTime;

    xran_lbm_port_info *lbm_port_info = &eth_ctx->lbm_port_info[eth_port];

    switch(lbm_port_info->lbm_state){
        case XRAN_LBM_STATE_IDLE:
            /* LBM wake up*/
            if(lbm_port_info->xmitEnable == 1)
            {
                lbm_port_info->lbm_state = XRAN_LBM_STATE_TRANSMITTING;
                lbm_port_info->numRetries = 0;
                lbm_port_info->LBRreceived = 0;
                if(xran_create_and_send_lbm_packet(eth_port, eth_ctx, lbm_port_info)){
                    rte_panic("xran_create_and_send_lbm_packet failed");
                }
                clock_gettime(CLOCK_REALTIME, &currTime);
                lbm_port_info->nextPass = TIMESPEC_TO_NSEC(currTime) + (eth_ctx->lbm_common_info.LBRTimeOut*NSEC_PER_MILLI_SEC);
                lbm_port_info->lbm_state = XRAN_LBM_STATE_WAITING;
                print_dbg("LBM transmitted waiting for LBR: port %hhu: transId ",eth_port, eth_ctx->lbm_port_info[eth_port].nextLBMtransID - 1);
            }

        break;

        case XRAN_LBM_STATE_WAITING:
            clock_gettime(CLOCK_REALTIME, &currTime);

            if((lbm_port_info->LBRreceived) && 
                (TIMESPEC_TO_NSEC(currTime) <= lbm_port_info->nextPass) && 
                (lbm_port_info->numRetries <= eth_ctx->lbm_common_info.numRetransmissions))
            {

                if(lbm_port_info->linkStatus == 0){ /*Checking if previous Link State is 0*/
                    /*Notify that link has gone up*/
                    if(p_syscfg->oam_notify_cb){
                        p_syscfg->oam_notify_cb(NULL, eth_port,1 /*link State changes to UP/1*/);
                    }
                    else{
                        print_dbg("LBM notification callback not registered");
                    }
                }

                /* LBR received - link up */
                lbm_port_info->linkStatus = 1;
                lbm_port_info->xmitEnable = 0;
                lbm_port_info->lbm_state = XRAN_LBM_STATE_IDLE;
                print_dbg("LBR received for port %hhu",eth_port);
            }
            else /* Not received LBR*/ 
            {
                /* LBR timed out */
                if(TIMESPEC_TO_NSEC(currTime) > lbm_port_info->nextPass)
                {
                    /* Attempt re-transmission*/
                    if(lbm_port_info->numRetries < eth_ctx->lbm_common_info.numRetransmissions)
                    {
                        lbm_port_info->lbm_state = XRAN_LBM_STATE_TRANSMITTING;
                        lbm_port_info->numRetries += 1;
                        if(xran_create_and_send_lbm_packet(eth_port, eth_ctx, lbm_port_info)){
                            rte_panic("xran_create_and_send_lbm_packets failed");
                        }
                        struct timespec currTime1;
                        clock_gettime(CLOCK_REALTIME, &currTime1);
                        lbm_port_info->nextPass = TIMESPEC_TO_NSEC(currTime1) + (eth_ctx->lbm_common_info.LBRTimeOut*NSEC_PER_MILLI_SEC);
                        lbm_port_info->lbm_state = XRAN_LBM_STATE_WAITING;
                        print_dbg("LBR timeout for port %hhu numRetries %hu",eth_port,lbm_port_info->numRetries);
                    }
                    /* LBR timed out for last re-transmission - link down */
                    else if (lbm_port_info->numRetries >= eth_ctx->lbm_common_info.numRetransmissions)
                    {
                        if(lbm_port_info->linkStatus == 1){ /*Checking if previous Link State is UP/1*/
                            /*Notify that link has gone up*/
                            if(p_syscfg->oam_notify_cb){
                                p_syscfg->oam_notify_cb(NULL,eth_port,0 /*link State changed to DOWN/1*/);
                            }
                            else{
                                print_dbg("LBM notification callback not registered");
                            }
                        }
                        lbm_port_info->linkStatus = 0;
                        lbm_port_info->xmitEnable = 0;
                        lbm_port_info->lbm_state = XRAN_LBM_STATE_IDLE;
                        print_dbg("LBR max retries reached for port %hhu: port down",eth_port);
                    }
                    /* else state machine is waiting */
                }
            }
        break;

        default:
                print_err("LBM state machine error port %hhu : %u", eth_port, lbm_port_info->lbm_state);
        break;
    }
    return XRAN_STATUS_SUCCESS;
}
/**
 * =============================================================
 * xran_process_lbm
 * @param[in] ctx           : Pointer to the ethernet context 
 * @param[in] eth_port_start: Ethernet port/VF ID to start processing LBM for
 * @param[in] eth_port_end  : Process LBM upto this Ethernet port/VF ID 
 *
 * @brief
 *
 * Function to kick start LBM tranmission and update the 
 * next LBM transmission time.
 * 
 * =============================================================
 */
xran_status_t xran_process_lbm(struct xran_ethdi_ctx *ctx, uint8_t eth_port_start, uint8_t eth_port_end)
{
    uint8_t eth_port;
    static uint8_t first_call = 1;
    struct timespec ts;
    uint64_t currTime;

    // Kick start LBM
    if(unlikely(first_call == 1)){
        for(eth_port = eth_port_start; eth_port < eth_port_end; ++eth_port)
        {
            if(ctx->lbm_port_info[eth_port].lbm_state == XRAN_LBM_STATE_INIT)
            {
                ctx->lbm_port_info[eth_port].xmitEnable = 0;
                ctx->lbm_port_info[eth_port].lbm_state = XRAN_LBM_STATE_IDLE;
            }
        }

        clock_gettime(CLOCK_REALTIME, &ts); //get clock time
        currTime = TIMESPEC_TO_NSEC(ts);
        ctx->lbm_common_info.nextLBMtime = currTime + (ctx->lbm_common_info.LBMPeriodicity*NSEC_PER_MILLI_SEC);

        first_call = 0;
    }
    else{
        clock_gettime(CLOCK_REALTIME, &ts); //get clock time
        currTime = TIMESPEC_TO_NSEC(ts);

        if(currTime >= (ctx->lbm_common_info.nextLBMtime))
        {
            for(eth_port = eth_port_start; eth_port < eth_port_end; ++eth_port)
            {
                if(ctx->lbm_port_info[eth_port].lbm_enable && ctx->lbm_port_info[eth_port].lbm_state == XRAN_LBM_STATE_IDLE)
                {
                    ctx->lbm_port_info[eth_port].xmitEnable = 1;
                }
                // else state machine waiting for LBRs/ re-transmitting
            }

            ctx->lbm_common_info.nextLBMtime = currTime + (ctx->lbm_common_info.LBMPeriodicity*NSEC_PER_MILLI_SEC);
            // printf("\n1. currTime %lu LBMPeriodicity*NSEC_PER_MILLI_SEC %lu nextLBMtime %lu",currTime, ctx->lbm_common_info.LBMPeriodicity*NSEC_PER_MILLI_SEC, ctx->lbm_common_info.nextLBMtime);

        }

        /*Make this per VF call and if that VF/port is enabled*/
        for(eth_port = eth_port_start; eth_port < eth_port_end; ++eth_port)
        {
            if(ctx->lbm_port_info[eth_port].lbm_enable == 1){
                if(xran_lbm_state_machine(eth_port) != XRAN_STATUS_SUCCESS)
                {
                    print_err("xran_lbm_state_machine!");
                    return XRAN_STATUS_FAIL;
                }
            }
        }
    }
    return XRAN_STATUS_SUCCESS;
}
xran_status_t xran_create_and_send_lbr_pkt(uint32_t lbm_transId, uint16_t port_id, struct xran_ethdi_ctx *eth_ctx)
{
    xran_lbm_port_info *lbm_port_info = &eth_ctx->lbm_port_info[port_id];
    struct rte_mbuf *pkt = xran_ethdi_mbuf_alloc();
    uint8_t *buff;
    xran_lbr_header *lbr_hdr;
    xran_device_ctx_t *xran_dev_ctx;

    /* Allocate mbuf */
    if(unlikely(pkt == NULL)){
        print_err("mbuf alloc failure!");
        return XRAN_STATUS_FAIL;
    }

    // Append bytes for lbr header
    buff = (uint8_t *)rte_pktmbuf_append(pkt,sizeof(xran_lbr_header));

    if(unlikely(!buff)){
        print_err("\nFailed to allocate LBR memory1");
        return XRAN_STATUS_RESOURCE;
    }

    // Prepend bytes for eth_hdr
    buff = (uint8_t *)rte_pktmbuf_prepend(pkt, sizeof(struct rte_ether_hdr));

    if(unlikely(!buff)){
        print_err("\nFailed to allocate LBR memory2");
        return XRAN_STATUS_RESOURCE;
    }

    /* Create CFM common header */
    lbr_hdr = rte_pktmbuf_mtod_offset(pkt, xran_lbr_header *, sizeof(struct rte_ether_hdr));
    lbr_hdr->cfm_common_header.version = 0;
    lbr_hdr->cfm_common_header.md_level = 0;
    lbr_hdr->cfm_common_header.opcode   = CFM_OPCODE_LOOPBACK_REPLY;
    lbr_hdr->cfm_common_header.flags = 0;
    lbr_hdr->cfm_common_header.first_tlv_offset = 4;
    lbr_hdr->loopBackTransactionIdentifier = lbm_transId; /* Copying directly from LBM msg hence don't need CPU to BE */

    lbr_hdr->end_tlv = END_TLV;

    /* Send the packet */
    if(xran_ethdi_mbuf_send(pkt, ETHER_TYPE_CFM, port_id) != 1)
    {
        print_err("Failed to transmit LBR : vf_id %hu transId %u", port_id, rte_be_to_cpu_32(lbm_transId));
        return XRAN_STATUS_FAIL;
    }

    /* Increment stats */
    lbm_port_info->stats.numTransmittedLBM += 1;

    /* Increment xRAN port stats */
    xran_dev_ctx = xran_dev_get_ctx_by_id(eth_ctx->vf2xran_port[port_id]);
    if(unlikely(!xran_dev_ctx))
    {
        print_err("dev_ctx is NULL : vf id %hu", port_id);
        return XRAN_STATUS_FAIL;
    }

    ++xran_dev_ctx->fh_counters.tx_counter;
    xran_dev_ctx->fh_counters.tx_bytes_counter += rte_pktmbuf_pkt_len(pkt);

    return XRAN_STATUS_SUCCESS;
}

/**
 * =============================================================
 * xran_parse_lbm
 * @param[in]  lbm_pkt : Pointer to the LBM packet mbuf
 * @param[in]  vf_id   : VF ID to process the LBM message for
 * @param[in]  lbm_hdr : Pointer to the LBM header in the segment buffer of lbm_pkt
 *
 * @brief
 *
 * Function for processesing the received IEEE 802.1Q
 * Connectivity Fault Management LBM (Loop Back Message) on O-RU
 * =============================================================
 */
xran_status_t xran_parse_lbm(struct rte_mbuf *lbm_pkt, uint16_t vf_id, xran_lbm_header *lbm_hdr)
{
    struct xran_ethdi_ctx *eth_ctx = xran_ethdi_get_ctx();
    uint16_t expected_tlv_length = sizeof(lbm_hdr->tlv.chassis_id_length) + sizeof(lbm_hdr->tlv.chassis_id_subtype) + sizeof(struct rte_ether_addr);

    /* Validate packet length */
    if(rte_pktmbuf_pkt_len(lbm_pkt) - sizeof(struct rte_ether_hdr) < sizeof(xran_lbm_header))
    {
        print_dbg("Shorter LBM packet");
        return XRAN_STATUS_FAIL;

    }

    /* Validate TLV contents*/
    if((lbm_hdr->tlv.type != SENDER_ID_TLV) ||
       (lbm_hdr->tlv.chassis_id_subtype != CHASSIS_ID_SUBTYPE_MAC_ADDR) ||
       (rte_be_to_cpu_16(lbm_hdr->tlv.length) != expected_tlv_length) ||
       (lbm_hdr->tlv.chassis_id_length != sizeof(struct rte_ether_addr)))
    {
        print_err("TLV validation failed");
        return XRAN_STATUS_FAIL;
    }

    /* Check transaction-id */
    if(rte_be_to_cpu_32(lbm_hdr->loopBackTransactionIdentifier) != eth_ctx->lbm_port_info[vf_id].expectedLBRtransID){
        eth_ctx->lbm_port_info[vf_id].stats.rxValidOutOfOrderLBRs += 1;
        eth_ctx->lbm_port_info[vf_id].expectedLBRtransID = rte_be_to_cpu_32(lbm_hdr->loopBackTransactionIdentifier) + 1; //Next pkt will have + 1 transId
        print_err("OOO LBR - expected %u receieved %u", eth_ctx->lbm_port_info[vf_id].expectedLBRtransID, rte_be_to_cpu_32(lbm_hdr->loopBackTransactionIdentifier));
    }
    else{
        eth_ctx->lbm_port_info[vf_id].stats.rxValidInOrderLBRs += 1;
        eth_ctx->lbm_port_info[vf_id].expectedLBRtransID += 1;
    }

    /* LBM received, now send reply to O-DU */
    if(xran_create_and_send_lbr_pkt(lbm_hdr->loopBackTransactionIdentifier, vf_id, xran_ethdi_get_ctx()) )
    {
        print_dbg("xran_create_and_send_lbr_pkt failed!");
        return XRAN_STATUS_FAIL;
    }

    return XRAN_STATUS_SUCCESS;
}

/**
 * =============================================================
 * xran_parse_lbr
 * @param[in]  mbuf_cfm : Pointer to the CFM packet mbuf
 * @param[in]  vf_id   : VF ID to process the LBM message for
 * @param[in]  lbr_hdr : Pointer to the LBR header in the segment buffer of mbuf_cfm
 *
 * @brief
 *
 * Function for processesing the received IEEE 802.1Q
 * Connectivity Fault Management LBR (Loop Back Response) on O-DU
 * =============================================================
 */
xran_status_t xran_parse_lbr(struct rte_mbuf *mbuf_cfm, uint16_t vf_id, xran_lbr_header *lbr_hdr)
{
    struct xran_ethdi_ctx *eth_ctx = xran_ethdi_get_ctx();
    xran_lbm_port_info *lbm_port_info = &eth_ctx->lbm_port_info[vf_id];

    /* Validate the length of the packet */
    if(rte_pktmbuf_pkt_len(mbuf_cfm) - sizeof(struct rte_ether_hdr) < sizeof(xran_lbr_header))
    {
        print_err("Shorter LBR packet");
        return XRAN_STATUS_FAIL;
    }

    /* Check the transaction-id */
    if(rte_be_to_cpu_32(lbr_hdr->loopBackTransactionIdentifier) == lbm_port_info->expectedLBRtransID)
        lbm_port_info->stats.rxValidInOrderLBRs += 1;
    else
    {
        lbm_port_info->stats.rxValidOutOfOrderLBRs += 1;
    }

    /* O-DU is expecting LBR */
    if(lbm_port_info->lbm_state == XRAN_LBM_STATE_WAITING)
    {
        /* LBR received in time link is UP */
        lbm_port_info->LBRreceived = 1;
    }

    return XRAN_STATUS_SUCCESS;
}

/**
 * =============================================================
 * xran_receive_lbr
 * @param[in]  mbuf_cfm: Pointer to the CFM packet mbuf
 * @param[in]  vf_id   : VF ID to process the LBM message for
 *
 * @brief
 *
 * Function for processesing the received IEEE 802.1Q
 * Connectivity Fault Management messages
 * =============================================================
 */
xran_status_t xran_process_cfm_message(struct rte_mbuf *mbuf_cfm, uint16_t vf_id)
{
    xran_cfm_common_header *cfm_cmn_hdr = rte_pktmbuf_mtod_offset(mbuf_cfm, xran_cfm_common_header *, sizeof(struct rte_ether_hdr));

   if(cfm_cmn_hdr->version != 0 || cfm_cmn_hdr->md_level != 0 || cfm_cmn_hdr->first_tlv_offset != 4 || cfm_cmn_hdr->flags != 0){
        print_err("CFM common header validation failed");
        return XRAN_STATUS_FAIL;
   }

    switch(cfm_cmn_hdr->opcode){

        case CFM_OPCODE_LOOPBACK_REPLY:

            if(xran_get_syscfg_appmode() == ID_O_DU){
                if(xran_parse_lbr(mbuf_cfm, vf_id, (xran_lbr_header *)cfm_cmn_hdr)){
                    print_err("xran_parse_lbr failed");
                    return XRAN_STATUS_FAIL;
                }
            }
            else{
                print_err("O-RU received LBR - not expected");
            }
        break;

        case CFM_OPCODE_LOOPBACK_MESSAGE:
            if(xran_get_syscfg_appmode() == ID_O_RU)
            {
                if(xran_parse_lbm(mbuf_cfm, vf_id, (xran_lbm_header *)cfm_cmn_hdr)){
                    print_err("xran_parse_lbm failed");
                    return XRAN_STATUS_FAIL;
                }
            }
            else{
                print_err("O-DU received LBM - not expected");
            }
        break;

        default:
            print_err("Unsupported opcode");
            return XRAN_STATUS_FAIL;
        break;
    }

    return XRAN_STATUS_SUCCESS;
}

xran_status_t xran_create_and_send_lbm_packet(uint8_t port_id, struct xran_ethdi_ctx *eth_ctx, xran_lbm_port_info *lbm_port_info)
{
    struct rte_mbuf *pkt = xran_ethdi_mbuf_alloc();
    uint8_t *buff;
    // struct rte_ether_hdr *eth_hdr;
    xran_lbm_header *lbm_hdr;
    struct rte_ether_addr smac;
    xran_device_ctx_t *xran_dev_ctx;

    if(unlikely(pkt == NULL)){
        print_err("mbuf alloc failure!");
        return XRAN_STATUS_FAIL;
    }

    // 1. Append pkt size bytes to mbuf
    buff = (uint8_t *)rte_pktmbuf_append(pkt,sizeof(xran_lbm_header));

    if(unlikely(!buff)){
        print_err("\nFailed to allocate LBM memory1");
        return XRAN_STATUS_RESOURCE;
    }

    buff = (uint8_t *)rte_pktmbuf_prepend(pkt, sizeof(struct rte_ether_hdr));

    if(unlikely(!buff)){
        print_err("\nFailed to allocate LBM memory2");
        return XRAN_STATUS_RESOURCE;
    }

    // 2. Prepare eth header -> will be done in xran_ethdi_mbuf_send
    #if 0
    eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
    eth_hdr->dst_addr = eth_ctx->entities[port_id][O_DU];

    rte_eth_macaddr_get(port_id, &eth_hdr->src_addr);
    eth_hdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_CFM);
    #endif

    // 3. Prepare CFM common headr
    lbm_hdr = rte_pktmbuf_mtod_offset(pkt, xran_lbm_header *, sizeof(struct rte_ether_hdr));

    lbm_hdr->cfm_common_header.md_level = 0;
    lbm_hdr->cfm_common_header.version = 0;
    lbm_hdr->cfm_common_header.opcode = CFM_OPCODE_LOOPBACK_MESSAGE;
    lbm_hdr->cfm_common_header.flags = 0;
    lbm_hdr->cfm_common_header.first_tlv_offset = 4;

    // 4. Prepare LBM header
    lbm_port_info->expectedLBRtransID = lbm_port_info->nextLBMtransID;
    lbm_hdr->loopBackTransactionIdentifier = rte_cpu_to_be_32(lbm_port_info->nextLBMtransID);
    lbm_port_info->nextLBMtransID += 1;

    // 5. Prepare TLV header
    lbm_hdr->tlv.type = SENDER_ID_TLV;
    lbm_hdr->tlv.length = rte_cpu_to_be_16(sizeof(lbm_hdr->tlv.chassis_id_length) + sizeof(lbm_hdr->tlv.chassis_id_subtype) + sizeof(struct rte_ether_addr)); /*Size of value field */
    lbm_hdr->tlv.chassis_id_subtype = CHASSIS_ID_SUBTYPE_MAC_ADDR;
    lbm_hdr->tlv.chassis_id_length = sizeof(struct rte_ether_addr);

    /* CPU to BE */
    rte_eth_macaddr_get(port_id, &smac);
    lbm_hdr->tlv.chassis_id_smac[0] = smac.addr_bytes[5];
    lbm_hdr->tlv.chassis_id_smac[1] = smac.addr_bytes[4];
    lbm_hdr->tlv.chassis_id_smac[2] = smac.addr_bytes[3];
    lbm_hdr->tlv.chassis_id_smac[3] = smac.addr_bytes[2];
    lbm_hdr->tlv.chassis_id_smac[4] = smac.addr_bytes[1];
    lbm_hdr->tlv.chassis_id_smac[5] = smac.addr_bytes[0];

    // 6. Type end
    lbm_hdr->tlv.end_type = END_TLV;

    // if(rte_ring_enqueue(eth_ctx->tx_ring[eth_ctx->io_cfg.port[port_id]],pkt) != 0)
    // {
    //     print_err("Failed to enqueue LBM pkt port: %d",eth_ctx->io_cfg.port[port_id]);
    //     rte_pktmbuf_free(pkt);
    //     return XRAN_STATUS_FAIL;
    // }

    if(xran_ethdi_mbuf_send(pkt, ETHER_TYPE_CFM, port_id) != 1)
    {
        print_err("\nFailed to transmit LBM pkt: port_id %hhu, currTransId %u", port_id, lbm_port_info->expectedLBRtransID);
        return XRAN_STATUS_FAIL;
    }

    // if(rte_eth_tx_burst(eth_ctx->io_cfg.port[port_id], 0, &pkt, 1) != 1)
    // {
    //     print_err("\nFailed to transmit LBM pkt: port_id %hhu, currTransId %u", port_id, lbm_port_info->expectedLBRtransID);
    //     rte_pktmbuf_free(pkt);
    //     return XRAN_STATUS_FAIL;
    // }

    ++eth_ctx->lbm_port_info[port_id].stats.numTransmittedLBM;

    /* Increment xRAN port stats */
    xran_dev_ctx = xran_dev_get_ctx_by_id(eth_ctx->vf2xran_port[port_id]);
    if(unlikely(!xran_dev_ctx))
    {
        print_err("dev_ctx is NULL : vf id %hhu", port_id);
        return XRAN_STATUS_FAIL;
    }

    ++xran_dev_ctx->fh_counters.tx_counter;
    xran_dev_ctx->fh_counters.tx_bytes_counter += rte_pktmbuf_pkt_len(pkt);


    return XRAN_STATUS_SUCCESS;
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

#ifndef POLL_EBBU_OFFLOAD
    rte_timer_manage();
#endif

    if(ctx->lbmEnable == 1 && xran_if_current_state == XRAN_RUNNING && (xran_get_syscfg_appmode() == ID_O_DU))
    {
        if(xran_process_lbm(ctx, 0, ctx->io_cfg.num_vfs) != XRAN_STATUS_SUCCESS)
        {
            print_err("xran_process_lbm failed");
        }

    }

    for (port_id = 0; port_id < XRAN_VF_MAX && port_id < ctx->io_cfg.num_vfs; port_id++){
        struct rte_mbuf *mbufs[BURST_RX_IO_SIZE];
#ifndef POLL_EBBU_OFFLOAD
        if(port[port_id] == 0xFF)
            return 0;
#else
        if(port[port_id] == 0xFFFF)
            return 0;
#endif

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
                if (xran_ecpri_one_way_delay_measurement_transmitter((uint16_t) port_id, (void*)XRAN_GET_DEV_CTX) != OK)
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

int32_t process_dpdk_io_port_id(int32_t port_start, int32_t port_num)
{
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    struct xran_io_cfg * cfg = &(xran_ethdi_get_ctx()->io_cfg);
    int32_t* port = &cfg->port[0];
    int32_t qi      = 0;
    int32_t port_id = 0;
    uint16_t rxed_total = 0;
    long t1 = MLogXRANTick();

    if(ctx->lbmEnable && xran_if_current_state == XRAN_RUNNING && (xran_get_syscfg_appmode() == ID_O_DU))
    {
        if(xran_process_lbm(ctx, port_start, port_start + port_num) != XRAN_STATUS_SUCCESS)
        {
            print_err("xran_process_lbm failed!");
        }
    }

    for (port_id = port_start; port_id < XRAN_VF_MAX && port_id < ctx->io_cfg.num_vfs && port_id < (port_start + port_num); port_id++){
        struct rte_mbuf *mbufs[BURST_RX_IO_SIZE];
        if(port[port_id] == 0xFF)
            return 0;

        /* RX */
        for(qi = 0; qi < ctx->rxq_per_port[port_id]; qi++) {

            const uint16_t rxed = rte_eth_rx_burst(port[port_id], qi, mbufs, BURST_RX_IO_SIZE);
            if (rxed != 0){
                unsigned enq_n = 0;
                rxed_total += rxed;
                ctx->rx_vf_queue_cnt[port[port_id]][qi] += rxed;
                enq_n =  rte_ring_enqueue_burst(ctx->rx_ring[port_id][qi], (void*)mbufs, rxed, NULL);
                if(rxed - enq_n)
                    rte_panic("error enq\n");
            }
        }

        // /* TX */
        xran_tx_from_ring(port[port_id], ctx->tx_ring[port_id]);

        if (XRAN_STOPPED == xran_if_current_state)
            return -1;
    }
    if(rxed_total)
        MLogXRANTask(PID_RADIO_RX_VALIDATE, t1, MLogXRANTick());

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
     if(ctx->lbmEnable == 1 && xran_if_current_state == XRAN_RUNNING && (xran_get_syscfg_appmode() == ID_O_DU))
    {
        if(xran_process_lbm(ctx, 0, ctx->io_cfg.num_vfs) != XRAN_STATUS_SUCCESS)
        {
            print_err("xran_process_lbm failed");
        }
    }

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

