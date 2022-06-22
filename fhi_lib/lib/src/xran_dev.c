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
 * @brief XRAN library device (O-RU or O-DU) specific context and coresponding methods
 * @file xran_dev.c
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#define _GNU_SOURCE
#include <sched.h>
#include <assert.h>
#include <err.h>
#include <libgen.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <malloc.h>
#include <immintrin.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_version.h>

#include "xran_fh_o_du.h"
#include "xran_dev.h"
#include "ethdi.h"
#include "xran_printf.h"

static struct xran_device_ctx *g_xran_dev_ctx[XRAN_PORTS_NUM] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

int32_t
xran_dev_create_ctx(uint32_t xran_ports_num)
{
    int32_t i = 0;
    struct xran_device_ctx * pCtx = NULL;

    if (xran_ports_num > XRAN_PORTS_NUM)
        return -1;

    pCtx = (struct xran_device_ctx *) _mm_malloc(sizeof(struct xran_device_ctx)*xran_ports_num, 64);
    if(pCtx){
        for(i = 0; i < xran_ports_num; i++){
            g_xran_dev_ctx[i] = pCtx;
            pCtx++;
        }
    } else {
        return -1;
    }
    return 0;
}

int32_t
xran_dev_destroy_ctx(void)
{
    if (g_xran_dev_ctx[0])
        free(g_xran_dev_ctx[0]);

    return 0;
}
struct xran_device_ctx *xran_dev_get_ctx(void)
{
    return g_xran_dev_ctx[0];
}

struct xran_device_ctx **xran_dev_get_ctx_addr(void)
{
    return &g_xran_dev_ctx[0];
}

struct xran_device_ctx *xran_dev_get_ctx_by_id(uint32_t xran_port_id)
{
    if (xran_port_id >= XRAN_PORTS_NUM)
        return NULL;
    else
        return g_xran_dev_ctx[xran_port_id];
}

static inline struct xran_fh_config *xran_lib_get_ctx_fhcfg(void *pHandle)
{
    struct xran_device_ctx * p_dev_ctx = (struct xran_device_ctx*)pHandle;
    return (&(p_dev_ctx->fh_cfg));
}


/**
 * @brief Get the configuration of eAxC ID
 *
 * @return the pointer of configuration
 */
struct xran_eaxcid_config *xran_get_conf_eAxC(void *pHandle)
{
    struct xran_device_ctx * p_dev_ctx = pHandle;
    if(p_dev_ctx == NULL)
        p_dev_ctx = xran_dev_get_ctx();

    if(p_dev_ctx == NULL)
        return NULL;
    return (&(p_dev_ctx->eAxc_id_cfg));
}

/**
 * @brief Get the configuration of the total number of beamforming weights on RU
 *
 * @return Configured the number of beamforming weights
 */
uint8_t xran_get_conf_num_bfweights(void *pHandle)
{
    struct xran_device_ctx * p_dev_ctx = pHandle;
    if(p_dev_ctx == NULL)
        p_dev_ctx = xran_dev_get_ctx();

    if(p_dev_ctx == NULL)
        return 0;

    return (p_dev_ctx->fh_init.totalBfWeights);
}

/**
 * @brief Get the configuration of subcarrier spacing for PRACH
 *
 * @return subcarrier spacing value for PRACH
 */
uint8_t xran_get_conf_prach_scs(void *pHandle)
{
    return (xran_lib_get_ctx_fhcfg(pHandle)->prach_conf.nPrachSubcSpacing);
}

/**
 * @brief Get the configuration of FFT size for RU
 *
 * @return FFT size value for RU
 */
uint8_t xran_get_conf_fftsize(void *pHandle)
{
    return (xran_lib_get_ctx_fhcfg(pHandle)->ru_conf.fftSize);
}

/**
 * @brief Get the configuration of nummerology
 *
 * @return Configured numerology
 */
uint8_t xran_get_conf_numerology(void *pHandle)
{
    return (xran_lib_get_ctx_fhcfg(pHandle)->frame_conf.nNumerology);
}

/**
 * @brief Get the configuration of IQ bit width for RU
 *
 * @return IQ bit width for RU
 */
uint8_t xran_get_conf_iqwidth_prach(void *pHandle)
{
    struct xran_fh_config *pFhCfg;

    pFhCfg = xran_lib_get_ctx_fhcfg(pHandle);
    return ((pFhCfg->ru_conf.iqWidth_PRACH==16)?0:pFhCfg->ru_conf.iqWidth_PRACH);
}

/**
 * @brief Get the configuration of compression method for RU
 *
 * @return Compression method for RU
 */
uint8_t xran_get_conf_compmethod_prach(void *pHandle)
{
    return (xran_lib_get_ctx_fhcfg(pHandle)->ru_conf.compMeth_PRACH);
}


/**
 * @brief Get the configuration of the number of component carriers
 *
 * @return Configured the number of component carriers
 */
uint8_t xran_get_num_cc(void *pHandle)
{
    return (xran_lib_get_ctx_fhcfg(pHandle)->nCC);
}

/**
 * @brief Get the configuration of the number of antenna for UL
 *
 * @return Configured the number of antenna
 */
uint8_t xran_get_num_eAxc(void *pHandle)
{
    return (xran_lib_get_ctx_fhcfg(pHandle)->neAxc);
}

/**
 * @brief Get configuration of O-RU (Cat A or Cat B)
 *
 * @return Configured the number of antenna
 */
enum xran_category xran_get_ru_category(void *pHandle)
{
    return (xran_lib_get_ctx_fhcfg(pHandle)->ru_conf.xranCat);
}

/**
 * @brief Get the configuration of the number of antenna
 *
 * @return Configured the number of antenna
 */
uint8_t xran_get_num_eAxcUl(void *pHandle)
{
    return (xran_lib_get_ctx_fhcfg(pHandle)->neAxcUl);
}

/**
 * @brief Get the configuration of the number of antenna elements
 *
 * @return Configured the number of antenna
 */
uint8_t xran_get_num_ant_elm(void *pHandle)
{
    return (xran_lib_get_ctx_fhcfg(pHandle)->nAntElmTRx);
}

int32_t xran_get_common_counters(void *pXranLayerHandle, struct xran_common_counters *pStats)
{
    int32_t  o_xu_id        = 0;
    int32_t  xran_port_num  = 0;
    struct xran_device_ctx* pDev = (struct xran_device_ctx*)pXranLayerHandle;
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    uint16_t port, qi;

    if(pStats && pDev) {
        xran_port_num  = pDev->fh_init.xran_ports;
        for(o_xu_id = 0; o_xu_id < XRAN_PORTS_NUM;o_xu_id++ ){
            if(o_xu_id < xran_port_num ){
                pStats[o_xu_id] =  pDev->fh_counters;
            }
            pDev++;
        }
        if (ctx->io_cfg.id == 0 && ctx->io_cfg.num_rxq > 1) {
            for (port = 0; port < ctx->io_cfg.num_vfs; port++) {
                printf("vf %d: ", port);
                for (qi = 0; qi < ctx->rxq_per_port[port]; qi++){
                    printf("%6ld ", ctx->rx_vf_queue_cnt[port][qi]);
                }
                printf("\n");
            }
        }

        return XRAN_STATUS_SUCCESS;
    } else {
        return XRAN_STATUS_INVALID_PARAM;
    }
}

uint16_t xran_get_beamid(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id)
{
    return (0);     // NO BEAMFORMING
}

struct cb_elem_entry *xran_create_cb(XranSymCallbackFn cb_fn, void *cb_data, void* p_dev_ctx)
{
        struct cb_elem_entry * cb_elm = (struct cb_elem_entry *)malloc(sizeof(struct cb_elem_entry));
        if(cb_elm){
            cb_elm->pSymCallback    = cb_fn;
            cb_elm->pSymCallbackTag = cb_data;
            cb_elm->p_dev_ctx = p_dev_ctx;
        }

        return cb_elm;
}

int32_t
xran_destroy_cb(struct cb_elem_entry * cb_elm)
{
    if(cb_elm)
        free(cb_elm);
    return 0;
}

uint16_t
xran_map_ecpriRtcid_to_vf(struct xran_device_ctx *p_dev_ctx, int32_t dir, int32_t cc_id, int32_t ru_port_id)
{
    return (p_dev_ctx->map2vf[dir][cc_id][ru_port_id][XRAN_CP_VF]);
}

uint16_t
xran_map_ecpriPcid_to_vf(struct xran_device_ctx *p_dev_ctx,  int32_t dir, int32_t cc_id, int32_t ru_port_id)
{
    return (p_dev_ctx->map2vf[dir][cc_id][ru_port_id][XRAN_UP_VF]);
}

uint16_t
xran_set_map_ecpriRtcid_to_vf(struct xran_device_ctx *p_dev_ctx, int32_t dir, int32_t cc_id, int32_t ru_port_id, uint16_t vf_id)
{
    p_dev_ctx->map2vf[dir][cc_id][ru_port_id][XRAN_CP_VF] = vf_id;
    return XRAN_STATUS_SUCCESS;
}

uint16_t
xran_set_map_ecpriPcid_to_vf(struct xran_device_ctx *p_dev_ctx,  int32_t dir, int32_t cc_id, int32_t ru_port_id, uint16_t vf_id)
{
    p_dev_ctx->map2vf[dir][cc_id][ru_port_id][XRAN_UP_VF] = vf_id;
    return XRAN_STATUS_SUCCESS;
}

const char *
xran_pcid_str_type(struct xran_device_ctx* p_dev, int ant)
{
    if(ant < xran_get_num_eAxcUl(p_dev))
        return "PUSCH";
    else if (ant >= xran_get_num_eAxcUl(p_dev) && ant < 2*xran_get_num_eAxcUl(p_dev))
        return "PRACH";
    else if ( ant >= xran_get_num_eAxcUl(p_dev) * 2 && ant < 2*xran_get_num_eAxcUl(p_dev) + xran_get_num_ant_elm(p_dev))
        return " SRS ";
    else
        return " N/A ";
}

int32_t
xran_init_vf_rxq_to_pcid_mapping(void *pHandle)
{
/* eCPRI flow supported with DPDK 21.02 or later */
#if (RTE_VER_YEAR >= 21) /* eCPRI flow supported with DPDK 21.02 or later */
    struct xran_ethdi_ctx *eth_ctx = xran_ethdi_get_ctx();
    uint8_t xran_port_id = 0;
    struct xran_device_ctx* p_dev = NULL;
    struct rte_flow_error error;
    int32_t vf_id = 0;
    int32_t ant = 0;
    int32_t cc = 0;
    uint16_t pc_id_be = 0;
    uint16_t rx_q[XRAN_VF_MAX] = { 0 };

    int32_t dir = XRAN_DIR_UL;
    uint8_t num_eAxc = 0;
    uint8_t num_cc   = 0;

    if(pHandle) {
        p_dev = (struct xran_device_ctx* )pHandle;
        xran_port_id = p_dev->xran_port_id;
    } else {
        print_err("Invalid pHandle - %p", pHandle);
        return (XRAN_STATUS_FAIL);
    }

    num_cc = xran_get_num_cc(p_dev);

    if(xran_get_ru_category(pHandle) == XRAN_CATEGORY_A)
        num_eAxc = xran_get_num_eAxc(p_dev);
    else
        num_eAxc = xran_get_num_eAxcUl(p_dev);

    num_eAxc *= 2; /* +PRACH */
    num_eAxc += xran_get_num_ant_elm(p_dev); /* +SRS */

    for(cc = 0; cc < num_cc; cc++) {
        for(ant = 0; ant < num_eAxc; ant++) {
            pc_id_be = xran_compose_cid(0, 0, cc, ant);
            vf_id    = xran_map_ecpriPcid_to_vf(p_dev, dir, cc, ant);

            /* don't use queue 0 for eCpri Flows */
            if(rx_q[vf_id] == 0)
                rx_q[vf_id]++;

            p_dev->p_iq_flow[p_dev->iq_flow_cnt] = generate_ecpri_flow(vf_id, rx_q[vf_id], pc_id_be, &error);
            eth_ctx->vf_and_q2pc_id[vf_id][rx_q[vf_id]] = rte_be_to_cpu_16(pc_id_be);

            xran_decompose_cid((uint16_t)pc_id_be, &eth_ctx->vf_and_q2cid[vf_id][rx_q[vf_id]]);

            eth_ctx->vf_and_q2cid[vf_id][rx_q[vf_id]].bandSectorId = vf_id;
            eth_ctx->vf_and_q2cid[vf_id][rx_q[vf_id]].cuPortId     = rx_q[vf_id];

            printf("%s: p %d vf %d qi %d 0x%016p UP: dir %d cc %d (%d) ant %d (%d) type %s", __FUNCTION__, xran_port_id, vf_id, rx_q[vf_id], &eth_ctx->vf_and_q2cid[vf_id][rx_q[vf_id]], dir,
                cc, eth_ctx->vf_and_q2cid[vf_id][rx_q[vf_id]].ccId,  ant, eth_ctx->vf_and_q2cid[vf_id][rx_q[vf_id]].ruPortId, xran_pcid_str_type(p_dev, ant));

            printf("     queue_id %d flow_id %d pc_id 0x%04x\n",rx_q[vf_id], p_dev->iq_flow_cnt, pc_id_be);
            p_dev->iq_flow_cnt++;
            rx_q[vf_id]++;

            if(rx_q[vf_id] > eth_ctx->io_cfg.num_rxq)
                rte_panic("Not enough RX Queues\n");
            eth_ctx->rxq_per_port[vf_id] = rx_q[vf_id];
        }
    }
#endif
    return XRAN_STATUS_SUCCESS;
}

int32_t
xran_init_vfs_mapping(void *pHandle)
{
    int dir, cc, ant, i;
    struct xran_device_ctx* p_dev = NULL;
    uint8_t xran_port_id = 0;
    uint16_t vf_id    = 0;
    uint16_t vf_id_cp = 0;
    struct xran_ethdi_ctx *eth_ctx = xran_ethdi_get_ctx();
    uint16_t vf_id_all[XRAN_VF_MAX];
    uint16_t total_vf_cnt = 0;

    if(pHandle) {
        p_dev = (struct xran_device_ctx* )pHandle;
        xran_port_id = p_dev->xran_port_id;
    } else {
        print_err("Invalid pHandle - %p", pHandle);
        return (XRAN_STATUS_FAIL);
    }

    memset(vf_id_all, 0, sizeof(vf_id_all));

    for(i =  0; i < XRAN_VF_MAX; i++){
        if(eth_ctx->vf2xran_port[i] == xran_port_id){
            vf_id_all[total_vf_cnt++] = i;
            printf("%s: p %d vf %d\n", __FUNCTION__, xran_port_id, i);
        }
    }

    print_dbg("total_vf_cnt %d\n", total_vf_cnt);

    if(eth_ctx->io_cfg.nEthLinePerPort != (total_vf_cnt >> (1 - eth_ctx->io_cfg.one_vf_cu_plane))) {
        print_err("Invalid total_vf_cnt - %d [expected %d]", total_vf_cnt,
                eth_ctx->io_cfg.nEthLinePerPort << (1 - eth_ctx->io_cfg.one_vf_cu_plane));
        return (XRAN_STATUS_FAIL);
    }

    for(dir=0; dir < 2; dir++){
        for(cc=0; cc < xran_get_num_cc(p_dev); cc++){
            for(ant=0; ant < xran_get_num_eAxc(p_dev)*2 + xran_get_num_ant_elm(p_dev); ant++){
                if((total_vf_cnt == 2) && eth_ctx->io_cfg.one_vf_cu_plane){
                    if(ant & 1) { /* split ant half and half on VFs */
                        vf_id  = vf_id_all[XRAN_UP_VF+1];
                        xran_set_map_ecpriPcid_to_vf(p_dev, dir, cc, ant, vf_id);
                        vf_id_cp  = vf_id_all[XRAN_UP_VF+1];
                        xran_set_map_ecpriRtcid_to_vf(p_dev, dir, cc, ant, vf_id_cp);
                    } else {
                        vf_id  = vf_id_all[XRAN_UP_VF];
                        xran_set_map_ecpriPcid_to_vf(p_dev, dir, cc, ant, vf_id);
                        vf_id_cp  = vf_id_all[XRAN_UP_VF];
                        xran_set_map_ecpriRtcid_to_vf(p_dev, dir, cc, ant, vf_id_cp);
                    }
                } else {
                    vf_id  = vf_id_all[XRAN_UP_VF];
                    xran_set_map_ecpriPcid_to_vf(p_dev, dir, cc, ant, vf_id);
                    vf_id_cp  = vf_id_all[(eth_ctx->io_cfg.one_vf_cu_plane ? XRAN_UP_VF : XRAN_CP_VF)];
                    xran_set_map_ecpriRtcid_to_vf(p_dev, dir, cc, ant, vf_id_cp);
                }
                print_dbg("%s: p %d vf %d UP: dir %d cc %d ant %d\n", __FUNCTION__, xran_port_id, vf_id, dir, cc, ant);
                print_dbg("%s: p %d vf %d CP: dir %d cc %d ant %d\n", __FUNCTION__, xran_port_id, vf_id_cp, dir, cc, ant);
            }
        }
    }

    return (XRAN_STATUS_SUCCESS);
}
