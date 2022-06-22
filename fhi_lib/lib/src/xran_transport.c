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
 * @brief This file provides the implementation for Transport lyaer (eCPRI) API.
 *
 * @file xran_transport.c
 * @ingroup group_lte_source_xran
 * @author Intel Corporation
 *
 **/

#include <stdint.h>
#include <endian.h>
#include <immintrin.h>
#include <rte_common.h>
#include <rte_config.h>

#include "xran_fh_o_du.h"
#include "xran_common.h"
#include "xran_transport.h"
#include "xran_pkt_cp.h"
#include "xran_cp_api.h"
#include "xran_up_api.h"
#include "xran_printf.h"


/**
 * @brief return eCPRI header size without eCPRI common header
 *
 * @ingroup xran
 *
 * @return the size of eCPRI header without common header
 */
int xran_get_ecpri_hdr_size(void)
{
    return(sizeof(struct xran_ecpri_hdr) - sizeof(union xran_ecpri_cmn_hdr));
}

/**
 * @brief Compose ecpriRtcid/ecpriPcid
 *
 * @ingroup xran
 *
 * @param CU_Port_ID CU Port ID
 * @param BanbSector_ID Band Sector ID
 * @param CC_ID Component Carrier ID
 * @param Ant_ID RU Port ID (antenna ID)
 * @return uint16_t composed ecpriRtcid/ecpriPcid (network byte order)
 */
uint16_t xran_compose_cid(uint8_t CU_Port_ID, uint8_t BandSector_ID, uint8_t CC_ID, uint8_t Ant_ID)
{
  uint16_t cid;
  struct xran_eaxcid_config *conf;

    conf = xran_get_conf_eAxC(NULL);

    if(conf == NULL)
      rte_panic("conf == NULL");

    cid = ((CU_Port_ID      << conf->bit_cuPortId)      & conf->mask_cuPortId)
        | ((BandSector_ID   << conf->bit_bandSectorId)  & conf->mask_bandSectorId)
        | ((CC_ID           << conf->bit_ccId)          & conf->mask_ccId)
        | ((Ant_ID          << conf->bit_ruPortId)      & conf->mask_ruPortId);

    return (rte_cpu_to_be_16(cid));
}

/**
 * @brief Decompose ecpriRtcid/ecpriPcid
 *
 * @ingroup xran
 *
 * @param cid composed ecpriRtcid/ecpriPcid (network byte order)
 * @param result the pointer of the structure to store decomposed values
 * @return none
 */
void xran_decompose_cid(uint16_t cid, struct xran_eaxc_info *result)
{
  struct xran_eaxcid_config *conf;

    conf = xran_get_conf_eAxC(NULL);
    cid = rte_be_to_cpu_16(cid);

    if(conf == NULL)
      rte_panic("conf == NULL");

    result->cuPortId        = (cid&conf->mask_cuPortId)     >> conf->bit_cuPortId;
    result->bandSectorId    = (cid&conf->mask_bandSectorId) >> conf->bit_bandSectorId;
    result->ccId            = (cid&conf->mask_ccId)         >> conf->bit_ccId;
    result->ruPortId        = (cid&conf->mask_ruPortId)     >> conf->bit_ruPortId;

    return;
}

/**
 * @brief modify the payload size of eCPRI header in xRAN packet
 *
 * @ingroup xran
 *
 * @param mbuf Initialized rte_mbuf packet which has eCPRI header already
 * @param size payload size to be updated
 * @return none
 */
inline void xran_update_ecpri_payload_size(struct rte_mbuf *mbuf, int size)
{
  struct xran_ecpri_hdr *ecpri_hdr;

    ecpri_hdr = rte_pktmbuf_mtod(mbuf, struct xran_ecpri_hdr *);

    ecpri_hdr->cmnhdr.bits.ecpri_payl_size = rte_cpu_to_be_16(size);
}


/**
 * @brief Build ECPRI header and returns added length
 *
 * @ingroup xran
 *
 * @param mbuf
 *  The pointer of the packet buffer to be parsed
 * @param CC_ID
 *  Component Carrier ID for this C-Plane message
 * @param Ant_ID
 *  Antenna ID(RU Port ID) for this C-Plane message
 * @param seq_id
 *  Sequence ID for this C-Plane message
 * @param ecpri_hdr
 *  The pointer to ECPRI header
 * @return
 *  added payload size on success
 *  XRAN_STATUS_RESOURCE if failed to allocate the space to packet buffer
 */
int xran_build_ecpri_hdr(struct rte_mbuf *mbuf,
                        uint8_t CC_ID, uint8_t Ant_ID,
                        uint8_t seq_id,
                        struct xran_ecpri_hdr **ecpri_hdr)
{
  uint32_t payloadlen;
  struct xran_ecpri_hdr *tmp;

    tmp = (struct xran_ecpri_hdr *)rte_pktmbuf_append(mbuf, sizeof(struct xran_ecpri_hdr));
    if(unlikely(tmp == NULL)) {
        print_err("Fail to allocate the space for eCPRI hedaer!");
        return (XRAN_STATUS_RESOURCE);
        }

    /* Fill common header */
    /*tmp->cmnhdr.bits.ecpri_ver           = XRAN_ECPRI_VER;
    //tmp->cmnhdr.bits.ecpri_resv          = 0;     // should be zero
    //tmp->cmnhdr.bits.ecpri_concat        = 0;
    //tmp->cmnhdr.bits.ecpri_mesg_type     = ECPRI_RT_CONTROL_DATA;*/

    tmp->cmnhdr.data.data_num_1 = (XRAN_ECPRI_VER << xran_ecpri_cmn_hdr_bitfield_EcpriVer)
                                | (ECPRI_RT_CONTROL_DATA << xran_ecpri_cmn_hdr_bitfield_EcpriMsgType);
    tmp->ecpri_xtc_id               = xran_compose_cid(0, 0, CC_ID, Ant_ID);

    /* TODO: Transport layer fragmentation is not supported */
    //tmp->ecpri_seq_id.bits.seq_id        = seq_id;
    //tmp->ecpri_seq_id.bits.sub_seq_id    = 0;
    //tmp->ecpri_seq_id.bits.e_bit         = 1;
    tmp->ecpri_seq_id.data.data_num_1 = (seq_id << ecpri_seq_id_bitfield_seq_id)
                                      | (1 << ecpri_seq_id_bitfield_e_bit);

    /* Starts with eCPRI header size */
    payloadlen = XRAN_ECPRI_HDR_SZ; //xran_get_ecpri_hdr_size();

    *ecpri_hdr = tmp;

    return (payloadlen);
}

/**
 * @brief Parse ECPRI header
 *
 * @ingroup xran
 *
 * @param mbuf
 *  The pointer of the packet buffer to be parsed
 * @param ecpri_hdr
 *  The pointer to ECPRI header
 * @param pkt_info
 *  The pointer of sturcture to store the information from header
 * @return
 *  XRAN_STATUS_SUCCESS on success
 *  XRAN_STATUS_INVALID_PACKET if failed to parse the packet
 */
int xran_parse_ecpri_hdr(struct rte_mbuf *mbuf,
                    struct xran_ecpri_hdr **ecpri_hdr,
                    struct xran_recv_packet_info *pkt_info)
{
    int ret = XRAN_STATUS_SUCCESS;

    *ecpri_hdr = rte_pktmbuf_mtod(mbuf, void *);
    if(*ecpri_hdr == NULL) {
        print_err("Invalid packet - eCPRI hedaer!");
        return (XRAN_STATUS_INVALID_PACKET);
        }

    if(((*ecpri_hdr)->cmnhdr.bits.ecpri_ver != XRAN_ECPRI_VER) || ((*ecpri_hdr)->cmnhdr.bits.ecpri_resv != 0)){
        print_err("Invalid eCPRI version - %d", (*ecpri_hdr)->cmnhdr.bits.ecpri_ver);
        print_err("Invalid reserved field - %d", (*ecpri_hdr)->cmnhdr.bits.ecpri_resv);
        return (XRAN_STATUS_INVALID_PACKET);
    }

    /* Process eCPRI header */
    /*if((*ecpri_hdr)->cmnhdr.ecpri_ver != XRAN_ECPRI_VER) {
        print_err("Invalid eCPRI version - %d", (*ecpri_hdr)->cmnhdr.ecpri_ver);
        ret = XRAN_STATUS_INVALID_PACKET;
        }*/
    /*if((*ecpri_hdr)->cmnhdr.ecpri_resv != 0) {
        print_err("Invalid reserved field - %d", (*ecpri_hdr)->cmnhdr.ecpri_resv);
        ret = XRAN_STATUS_INVALID_PACKET;
        }*/


    if(pkt_info != NULL) {
        /* store the information from header */
        pkt_info->ecpri_version = (*ecpri_hdr)->cmnhdr.bits.ecpri_ver;
        pkt_info->msg_type      = (enum ecpri_msg_type)(*ecpri_hdr)->cmnhdr.bits.ecpri_mesg_type;
        pkt_info->payload_len   = rte_be_to_cpu_16((*ecpri_hdr)->cmnhdr.bits.ecpri_payl_size);

        pkt_info->seq_id        = (*ecpri_hdr)->ecpri_seq_id.bits.seq_id;
        pkt_info->subseq_id     = (*ecpri_hdr)->ecpri_seq_id.bits.sub_seq_id;
        pkt_info->ebit          = (*ecpri_hdr)->ecpri_seq_id.bits.e_bit;
        xran_decompose_cid((*ecpri_hdr)->ecpri_xtc_id, &(pkt_info->eaxc));
        }

    return (ret);
}

