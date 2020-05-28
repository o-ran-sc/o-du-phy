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
 * @brief This file provides the definitions for Transport layer (eCPRI) API.
 *
 * @file xran_transport.h
 * @ingroup group_lte_source_xran
 * @author Intel Corporation
 *
 **/

#ifndef _XRAN_TRANSPORT_H_
#define _XRAN_TRANSPORT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <rte_mbuf.h>

#include "xran_pkt.h"

struct xran_eaxc_info {
    uint8_t cuPortId;
    uint8_t bandSectorId;
    uint8_t ccId;
    uint8_t ruPortId;
};

struct xran_recv_packet_info {
    int ecpri_version;
    enum ecpri_msg_type msg_type;
    int payload_len;
    struct xran_eaxc_info eaxc;
    int seq_id;
    int subseq_id;
    int ebit;
};


int xran_get_ecpri_hdr_size(void);
void xran_update_ecpri_payload_size(struct rte_mbuf *mbuf, int size);

uint16_t xran_compose_cid(uint8_t CU_Port_ID, uint8_t BandSector_ID, uint8_t CC_ID, uint8_t Ant_ID);
void xran_decompose_cid(uint16_t cid, struct xran_eaxc_info *result);

int xran_build_ecpri_hdr(struct rte_mbuf *mbuf,
                        uint8_t CC_ID, uint8_t Ant_ID,
                        uint8_t seq_id,
                        struct xran_ecpri_hdr **ecpri_hdr);

int xran_parse_ecpri_hdr(struct rte_mbuf *mbuf,
                        struct xran_ecpri_hdr **ecpri_hdr,
                        struct xran_recv_packet_info *pkt_info);

#ifdef __cplusplus
}
#endif

#endif

