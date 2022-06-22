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
 * @brief XRAN C-plane processing module header file
 * @file xran_cp_proc.h
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#ifndef _XRAN_CP_PROC_H_
#define _XRAN_CP_PROC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <unistd.h>

#include <rte_common.h>

#include "xran_fh_o_du.h"
#include "xran_printf.h"

extern uint8_t xran_cp_seq_id_num[XRAN_PORTS_NUM][XRAN_MAX_CELLS_PER_PORT][XRAN_DIR_MAX][XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR];
extern uint8_t xran_updl_seq_id_num[XRAN_PORTS_NUM][XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR];
extern uint8_t xran_upul_seq_id_num[XRAN_PORTS_NUM][XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR];
extern uint8_t xran_section_id_curslot[XRAN_PORTS_NUM][XRAN_DIR_MAX][XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR * 2+ XRAN_MAX_ANT_ARRAY_ELM_NR];
extern uint16_t xran_section_id[XRAN_PORTS_NUM][XRAN_DIR_MAX][XRAN_MAX_CELLS_PER_PORT][XRAN_MAX_ANTENNA_NR * 2+ XRAN_MAX_ANT_ARRAY_ELM_NR];

int32_t xran_init_sectionid(void *pHandle);
int32_t xran_init_seqid(void *pHandle);

int32_t process_cplane(struct rte_mbuf *pkt, void* handle);
int32_t xran_cp_create_and_send_section(void *pHandle, uint8_t ru_port_id, int dir, int tti, int cc_id, struct xran_prb_map *prbMap,
                                        struct xran_prb_elm_proc_info_t *prbElmProcInfo,
                                        enum xran_category category,  uint8_t ctx_id);

int32_t xran_ruemul_init(void *pHandle);
int32_t xran_ruemul_release(void *pHandle);

#define ONE_EXT_LEN(prbMap) (prbMap->bf_weight.ext_section_sz / prbMap->bf_weight.numSetBFWs) - sizeof(struct xran_cp_radioapp_section1)
#define ONE_CPSEC_EXT_LEN(prbMap) (prbMap->bf_weight.ext_section_sz / prbMap->bf_weight.numSetBFWs)

static __rte_always_inline uint16_t
xran_alloc_sectionid(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ant_id, uint8_t subframe_id, uint8_t slot_id)
{
    int8_t xran_port = 0;
    if((xran_port =  xran_dev_ctx_get_port_id(pHandle)) < 0 ){
        print_err("Invalid pHandle - %p", pHandle);
        return (0);
    }

    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (0);
    }
    if(ant_id >= XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR) {  //for PRACH, ant_id starts from num_ant
        print_err("Invalid antenna ID - %d", ant_id);
        return (0);
    }

    /* if new slot has been started,
     * then initializes section id again for new start */
    if(xran_section_id_curslot[xran_port][dir][cc_id][ant_id] != (subframe_id * 2 + slot_id)) {
        xran_section_id[xran_port][dir][cc_id][ant_id] = 0;
        xran_section_id_curslot[xran_port][dir][cc_id][ant_id] = (subframe_id * 2 + slot_id);
    }

    return(xran_section_id[xran_port][dir][cc_id][ant_id]++);
}

static __rte_always_inline uint8_t
xran_get_upul_seqid(void *pHandle, uint8_t cc_id, uint8_t ant_id)
{
    int8_t xran_port = 0;
    if((xran_port =  xran_dev_ctx_get_port_id(pHandle)) < 0 ){
        print_err("Invalid pHandle - %p", pHandle);
        return (0);
    }

    if(xran_port >= XRAN_PORTS_NUM) {
        print_err("Invalid port - %d", xran_port);
        return (0);
    }

    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (0);
    }
    if(ant_id >= XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR) {
        print_err("Invalid antenna ID - %d", ant_id);
        return (0);
    }

    return(xran_upul_seq_id_num[xran_port][cc_id][ant_id]++);
}

static __rte_always_inline uint8_t*
xran_get_upul_seqid_addr(void *pHandle, uint8_t cc_id, uint8_t ant_id)
{
    int8_t xran_port = 0;
    if((xran_port =  xran_dev_ctx_get_port_id(pHandle)) < 0 ){
        print_err("Invalid pHandle - %p", pHandle);
        return (0);
    }

    if(xran_port >= XRAN_PORTS_NUM) {
        print_err("Invalid port - %d", xran_port);
        return (0);
    }

    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (0);
    }
    if(ant_id >= XRAN_MAX_ANTENNA_NR * 2) {
        print_err("Invalid antenna ID - %d", ant_id);
        return (0);
    }

    return(&xran_upul_seq_id_num[xran_port][cc_id][ant_id]);
}

static __rte_always_inline int8_t
xran_check_cp_seqid(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ant_id, uint8_t seq_id)
{
    int8_t xran_port = 0;
    if((xran_port =  xran_dev_ctx_get_port_id(pHandle)) < 0 ){
        print_err("Invalid pHandle - %p", pHandle);
        return (0);
    }

    if(xran_port >= XRAN_PORTS_NUM) {
        print_err("Invalid port - %d", xran_port);
        return (0);
    }

    if(dir >= XRAN_DIR_MAX) {
        print_err("Invalid direction - %d", dir);
        return (-1);
        }
    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (-1);
    }
    if(ant_id >= XRAN_MAX_ANTENNA_NR * 2) {
        print_err("Invalid antenna ID - %d", ant_id);
        return (-1);
    }

    xran_cp_seq_id_num[xran_port][cc_id][dir][ant_id]++;
    if(xran_cp_seq_id_num[xran_port][cc_id][dir][ant_id] == seq_id) { /* expected sequence */
        return (0);
    }
    else {
        xran_cp_seq_id_num[xran_port][cc_id][dir][ant_id] = seq_id;
        return (-1);
    }
}

static __rte_always_inline int8_t
xran_check_updl_seqid(void *pHandle, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id, uint8_t seq_id)
{
    int8_t xran_port = 0;
    if((xran_port =  xran_dev_ctx_get_port_id(pHandle)) < 0 ){
        print_err("Invalid pHandle - %p", pHandle);
        return (0);
    }

    if(xran_port >= XRAN_PORTS_NUM) {
        print_err("Invalid port - %d", xran_port);
        return (0);
    }

    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (-1);
    }

    if(ant_id >= XRAN_MAX_ANTENNA_NR) {
        print_err("Invalid antenna ID - %d", ant_id);
        return (-1);
    }

    /* O-RU needs to check the sequence ID of U-Plane DL from O-DU */
    xran_updl_seq_id_num[xran_port][cc_id][ant_id]++;
    if(xran_updl_seq_id_num[xran_port][cc_id][ant_id] == seq_id) {
        /* expected sequence */
        /*print_dbg("ant %u  cc_id %u : slot_id %u : seq_id %u : expected seq_id %u\n",
            ant_id, cc_id, slot_id, seq_id, xran_updl_seq_id_num[cc_id][ant_id]);*/
        return (0);
    } else {
       /*print_err("[%d] ant %u  cc_id %u : slot_id %u : seq_id %u : expected seq_id %u\n",
            xran_port, ant_id, cc_id, slot_id, seq_id, xran_updl_seq_id_num[xran_port][cc_id][ant_id]);*/

        xran_updl_seq_id_num[xran_port][cc_id][ant_id] = seq_id;

        return (-1);
    }
}

static __rte_always_inline int8_t
xran_check_upul_seqid(void *pHandle, uint8_t cc_id, uint8_t ant_id, uint8_t slot_id, uint8_t seq_id)
{
    int8_t xran_port = 0;
    if((xran_port =  xran_dev_ctx_get_port_id(pHandle)) < 0 ){
        print_err("Invalid pHandle - %p", pHandle);
        return (0);
    }

    if(xran_port >= XRAN_PORTS_NUM) {
        print_err("Invalid port - %d", xran_port);
        return (0);
    }

    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (-1);
    }

    if(ant_id >= XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR) {
        print_err("Invalid antenna ID - %d", ant_id);
        return (-1);
    }

    /* O-DU needs to check the sequence ID of U-Plane UL from O-RU */
    xran_upul_seq_id_num[xran_port][cc_id][ant_id]++;
    if(xran_upul_seq_id_num[xran_port][cc_id][ant_id] == seq_id) { /* expected sequence */
        return (XRAN_STATUS_SUCCESS);
    } else {
        print_dbg("[%d]expected seqid %u received %u, slot %u, ant %u cc %u", xran_port,  xran_upul_seq_id_num[xran_port][cc_id][ant_id], seq_id, slot_id, ant_id, cc_id);
        xran_upul_seq_id_num[xran_port][cc_id][ant_id] = seq_id; // for next
        return (-1);
    }
}

static __rte_always_inline uint8_t*
xran_get_updl_seqid_addr(void *pHandle, uint8_t cc_id, uint8_t ant_id)
{
    int8_t xran_port = 0;
    if((xran_port =  xran_dev_ctx_get_port_id(pHandle)) < 0 ){
        print_err("Invalid pHandle - %p", pHandle);
        return (0);
    }

    if(xran_port >= XRAN_PORTS_NUM) {
        print_err("Invalid port - %d", xran_port);
        return (0);
    }

    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (NULL);
    }
    if(ant_id >= XRAN_MAX_ANTENNA_NR) {
        print_err("Invalid antenna ID - %d", ant_id);
        return (NULL);
    }

    /* Only U-Plane DL needs to get sequence ID in O-DU */
    return(&xran_updl_seq_id_num[xran_port][cc_id][ant_id]);
}

static __rte_always_inline uint8_t
xran_get_cp_seqid(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ant_id)
{
    int8_t xran_port = 0;
    if((xran_port =  xran_dev_ctx_get_port_id(pHandle)) < 0 ){
        print_err("Invalid pHandle - %p", pHandle);
        return (0);
    }

    if(xran_port >= XRAN_PORTS_NUM) {
        print_err("Invalid port - %d", xran_port);
        return (0);
    }

    if(dir >= XRAN_DIR_MAX) {
        print_err("Invalid direction - %d", dir);
        return (0);
        }
    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (0);
        }
    if(ant_id >= XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR) {
        print_err("Invalid antenna ID - %d", ant_id);
        return (0);
        }

    return(xran_cp_seq_id_num[xran_port][cc_id][dir][ant_id]++);
}

static __rte_always_inline uint8_t
xran_get_updl_seqid(void *pHandle, uint8_t cc_id, uint8_t ant_id)
{
    int8_t xran_port = 0;
    if((xran_port =  xran_dev_ctx_get_port_id(pHandle)) < 0 ){
        print_err("Invalid pHandle - %p", pHandle);
        return (0);
    }

    if(xran_port >= XRAN_PORTS_NUM) {
        print_err("Invalid port - %d", xran_port);
        return (0);
    }
    if(cc_id >= XRAN_MAX_CELLS_PER_PORT) {
        print_err("Invalid CC ID - %d", cc_id);
        return (0);
        }
    if(ant_id >= XRAN_MAX_ANTENNA_NR) {
        print_err("Invalid antenna ID - %d", ant_id);
        return (0);
        }

    /* Only U-Plane DL needs to get sequence ID in O-DU */
    return(xran_updl_seq_id_num[xran_port][cc_id][ant_id]++);
}

#ifdef __cplusplus
}
#endif

#endif /*  _XRAN_CP_PROC_H_ */
