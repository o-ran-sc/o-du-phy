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

#include <rte_common.h>
#include <rte_mbuf.h>

#include "xran_pkt.h"

struct xran_eaxc_info {
    uint8_t cuPortId;
    uint8_t bandSectorId;
    uint8_t ccId;
    uint8_t ruPortId;
    };

/**
 * @brief Compose ecpriRtcid/ecpriPcid
 *
 * @param CU_Port_ID CU Port ID
 * @param BanbSector_ID Band Sector ID
 * @param CC_ID Component Carrier ID
 * @param Ant_ID RU Port ID (antenna ID)
 * @return uint16_t composed ecpriRtcid/ecpriPcid
 */
uint16_t xran_compose_cid(uint8_t CU_Port_ID, uint8_t BandSector_ID, uint8_t CC_ID, uint8_t Ant_ID);

/**
 * @brief Decompose ecpriRtcid/ecpriPcid
 *
 * @param cid composed ecpriRtcid/ecpriPcid (network byte order)
 * @param result the pointer of the structure to store decomposed values
 * @return none
 */
void xran_decompose_cid(uint16_t cid, struct xran_eaxc_info *result);

/**
 * @brief modify the payload size of eCPRI header in xRAN packet
 *
 * @param mbuf Initialized rte_mbuf packet which has eCPRI header already
 * @param size payload size to be updated
 * @return none
 */
void xran_update_ecpri_payload_size(struct rte_mbuf *mbuf, int size);

#endif

