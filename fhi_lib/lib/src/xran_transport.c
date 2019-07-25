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
 * @brief This file provides the implementation for Transport lyaer (eCPRI) API.
 *
 * @file xran_transport.c
 * @ingroup group_lte_source_xran
 * @author Intel Corporation
 *
 **/

#include <stdint.h>
#include <endian.h>
#include <rte_common.h>
#include <rte_config.h>

#include "xran_fh_lls_cu.h"
#include "xran_common.h"
#include "xran_transport.h"
#include "xran_up_api.h"


/**
 * @brief Compose ecpriRtcid/ecpriPcid
 *
 * @param CU_Port_ID CU Port ID
 * @param BanbSector_ID Band Sector ID
 * @param CC_ID Component Carrier ID
 * @param Ant_ID RU Port ID (antenna ID)
 * @return uint16_t composed ecpriRtcid/ecpriPcid (network byte order)
 */
inline uint16_t xran_compose_cid(uint8_t CU_Port_ID, uint8_t BandSector_ID, uint8_t CC_ID, uint8_t Ant_ID)
{
  uint16_t cid;
  XRANEAXCIDCONFIG *conf;

    conf = xran_get_conf_eAxC(NULL);

    cid = ((CU_Port_ID      << conf->bit_cuPortId)      & conf->mask_cuPortId)
        | ((BandSector_ID   << conf->bit_bandSectorId)  & conf->mask_bandSectorId)
        | ((CC_ID           << conf->bit_ccId)          & conf->mask_ccId)
        | ((Ant_ID          << conf->bit_ruPortId)      & conf->mask_ruPortId);

    return (rte_cpu_to_be_16(cid));
}

/**
 * @brief Decompose ecpriRtcid/ecpriPcid
 *
 * @param cid composed ecpriRtcid/ecpriPcid (network byte order)
 * @param result the pointer of the structure to store decomposed values
 * @return none
 */
inline void xran_decompose_cid(uint16_t cid, struct xran_eaxc_info *result)
{
  XRANEAXCIDCONFIG *conf;

    conf = xran_get_conf_eAxC(NULL);
    cid = rte_be_to_cpu_16(cid);

    result->cuPortId        = (cid&conf->mask_cuPortId)     >> conf->bit_cuPortId;
    result->bandSectorId    = (cid&conf->mask_bandSectorId) >> conf->bit_bandSectorId;
    result->ccId            = (cid&conf->mask_ccId)         >> conf->bit_ccId;
    result->ruPortId        = (cid&conf->mask_ruPortId)     >> conf->bit_ruPortId;

    return;
}

/**
 * @brief modify the payload size of eCPRI header in xRAN packet
 *
 * @param mbuf Initialized rte_mbuf packet which has eCPRI header already
 * @param size payload size to be updated
 * @return none
 */
inline void xran_update_ecpri_payload_size(struct rte_mbuf *mbuf, int size)
{
  struct xran_ecpri_hdr *ecpri_hdr;

    ecpri_hdr = rte_pktmbuf_mtod(mbuf, struct xran_ecpri_hdr *);

    ecpri_hdr->ecpri_payl_size = rte_cpu_to_be_16(size);
}

