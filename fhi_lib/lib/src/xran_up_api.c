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
 * @brief This file provides the implementation of User Plane Messages APIs.
 *
 * @file xran_up_api.c
 * @ingroup group_lte_source_xran
 * @author Intel Corporation
 *
 **/

#include <rte_memcpy.h>
#include <inttypes.h>
#include "xran_fh_lls_cu.h"
#include "xran_transport.h"
#include "xran_up_api.h"
#ifndef MLOG_ENABLED
#include "mlog_lnx_xRAN.h"
#else
#include "mlog_lnx.h"
#endif

extern uint32_t xran_lib_ota_tti;


/**
 * @brief Builds eCPRI header in xRAN packet
 *
 * @param mbuf Initialized rte_mbuf packet
 * @param iq_data_num_bytes Number of bytes in IQ data buffer
 * @param iq_data_offset Number of elements already sent
 * @return int int 0 on success, non zero on failure
 */
static int build_ecpri_hdr(struct rte_mbuf *mbuf,
    const uint32_t iq_data_num_bytes,
    const uint32_t iq_data_offset,
    uint8_t alignment)
{
    struct xran_ecpri_hdr *ecpri_hdr = (struct xran_ecpri_hdr *)
        rte_pktmbuf_append(mbuf, sizeof(struct xran_ecpri_hdr));

    uint16_t iq_samples_bytes_in_mbuf = rte_pktmbuf_tailroom(mbuf) -
        sizeof(struct radio_app_common_hdr) - sizeof(struct data_section_hdr);

    iq_samples_bytes_in_mbuf -= (iq_samples_bytes_in_mbuf % alignment);

    if (NULL == ecpri_hdr)
        return 1;

    ecpri_hdr->ecpri_ver = XRAN_ECPRI_VER;
    ecpri_hdr->ecpri_resv = 0;
    ecpri_hdr->ecpri_concat = 0;
    ecpri_hdr->ecpri_mesg_type = ECPRI_IQ_DATA;

    if (iq_data_offset + iq_samples_bytes_in_mbuf > iq_data_num_bytes) {
        ecpri_hdr->ecpri_payl_size =
            rte_cpu_to_be_16(sizeof(struct radio_app_common_hdr) +
                sizeof(struct data_section_hdr) +
                (iq_data_num_bytes - iq_data_offset));
        ecpri_hdr->ecpri_seq_id.e_bit = 1;  /* last segment */
    } else {
        ecpri_hdr->ecpri_payl_size =
            rte_cpu_to_be_16(sizeof(struct radio_app_common_hdr) +
                sizeof(struct data_section_hdr) +
                iq_samples_bytes_in_mbuf);
        ecpri_hdr->ecpri_seq_id.e_bit = 0;
    }

//    ecpri_hdr->ecpri_xtc_id = 0;    /* currently not used */
    ecpri_hdr->ecpri_seq_id.seq_id = 0;
    ecpri_hdr->ecpri_seq_id.sub_seq_id = iq_data_offset /
        iq_samples_bytes_in_mbuf;

    return 0;
}

/**
 * @brief Builds eCPRI header in xRAN packet
 *
 * @param mbuf Initialized rte_mbuf packet
 * @param ecpri_mesg_type eCPRI message type
 * @param payl_size the size in bytes of the payload part of eCPRI message
 * @param CC_ID Component Carrier ID for ecpriRtcid/ecpriPcid
 * @param Ant_ID Antenna ID for ecpriRtcid/ecpriPcid
 * @param seq_id Message identifier for eCPRI message
 * @return int int 0 on success, non zero on failure
 */
static int xran_build_ecpri_hdr_ex(struct rte_mbuf *mbuf,
                              uint8_t ecpri_mesg_type,
                              int payl_size,
                              uint8_t CC_ID,
                              uint8_t Ant_ID,
                              uint8_t seq_id)
{
    struct xran_ecpri_hdr *ecpri_hdr = (struct xran_ecpri_hdr *)
        rte_pktmbuf_append(mbuf, sizeof(struct xran_ecpri_hdr));

    if (NULL == ecpri_hdr)
        return 1;

    ecpri_hdr->ecpri_ver       = XRAN_ECPRI_VER;
    ecpri_hdr->ecpri_resv      = 0;     // should be zero
    ecpri_hdr->ecpri_concat    = 0;
    ecpri_hdr->ecpri_mesg_type = ecpri_mesg_type;
    ecpri_hdr->ecpri_payl_size = rte_cpu_to_be_16(payl_size
                            + sizeof(struct data_section_hdr)+sizeof(struct radio_app_common_hdr));

    /* one to one lls-CU to RU only and band sector is the same */
    ecpri_hdr->ecpri_xtc_id = xran_compose_cid(0, 0, CC_ID, Ant_ID);

    ecpri_hdr->ecpri_seq_id.seq_id = seq_id;

    /* no transport layer fragmentation supported */
    ecpri_hdr->ecpri_seq_id.sub_seq_id  = 0;
    ecpri_hdr->ecpri_seq_id.e_bit       = 1;

    return 0;
}


/**
 * @brief Builds application layer of xRAN packet
 *
 * @param mbuf Initialized rte_mbuf packet
 * @param app_hdr_input Radio App common header structure to be set in mbuf
 *                      packet.
 * @return int 0 on success, non zero on failure
 */
static int build_application_layer(
    struct rte_mbuf *mbuf,
    const struct radio_app_common_hdr *app_hdr_input)
{
    struct radio_app_common_hdr *app_hdr = (struct radio_app_common_hdr *)
        rte_pktmbuf_append(mbuf, sizeof(struct radio_app_common_hdr));

    if (NULL == app_hdr)
        return 1;

    rte_memcpy(app_hdr, app_hdr_input, sizeof(struct radio_app_common_hdr));

    return 0;
}

/**
 * @brief Builds section header in xRAN packet
 *
 * @param mbuf Initialized rte_mbuf packet
 * @param sec_hdr Section header structure to be set in mbuf packet
 * @return int 0 on success, non zero on failure
 */
static int build_section_hdr(
    struct rte_mbuf *mbuf,
    const struct data_section_hdr *sec_hdr)
{
    struct data_section_hdr *section_hdr = (struct data_section_hdr *)
        rte_pktmbuf_append(mbuf, sizeof(struct data_section_hdr));

    if (NULL == section_hdr)
        return 1;

    rte_memcpy(section_hdr, sec_hdr, sizeof(struct data_section_hdr));

    return 0;
}
/**
 * @brief Function for appending IQ samples data to the mbuf.
 *
 * @param mbuf Initialized rte_mbuf packet.
 * @param iq_data_start Address of the first element in IQ data array.
 * @param iq_data_num_bytes Size of the IQ data array.
 * @param iq_data_offset IQ data btyes already sent.
 * @return uint16_t Bytes that have been appended to the packet.
 */
static uint16_t append_iq_samples_ex(
    struct rte_mbuf *mbuf,
    const void *iq_data_start,
    const uint32_t iq_data_num_bytes)
{
    uint16_t free_space_in_pkt = rte_pktmbuf_tailroom(mbuf);

    if(free_space_in_pkt >= iq_data_num_bytes){

        void *iq_sam_buf = (void *)rte_pktmbuf_append(mbuf, iq_data_num_bytes);
        if (iq_sam_buf == NULL)
            return 0;
#ifdef XRAN_BYTE_ORDER_SWAP
        int idx = 0;
        uint16_t *restrict psrc = (uint16_t *)iq_data_start;
        uint16_t *restrict pdst = (uint16_t *)iq_sam_buf;
        /* CPU byte order (le) of IQ to network byte order (be) */
        for (idx = 0; idx < iq_data_num_bytes/sizeof(int16_t); idx++){
            pdst[idx]  =  (psrc[idx]>>8) | (psrc[idx]<<8); //rte_cpu_to_be_16(psrc[idx]);
        }
#else
#error xran spec is network byte order
        /* for debug */
        rte_memcpy(iq_sam_buf, (uint8_t *)iq_data_start,  iq_data_num_bytes);

#endif

        return iq_data_num_bytes;
    }

    return 0;
}


/**
 * @brief Function for appending IQ samples data to the mbuf.
 *
 * @param mbuf Initialized rte_mbuf packet.
 * @param iq_data_start Address of the first element in IQ data array.
 * @param iq_data_num_bytes Size of the IQ data array.
 * @param iq_data_offset IQ data btyes already sent.
 * @return uint16_t Bytes that have been appended to the packet.
 */
static uint16_t append_iq_samples(
    struct rte_mbuf *mbuf,
    const void *iq_data_start,
    const uint32_t iq_data_num_bytes,
    const uint32_t iq_data_offset,
    const uint8_t alignment)
{
    uint16_t iq_bytes_to_send = 0;
    uint16_t free_space_in_pkt = rte_pktmbuf_tailroom(mbuf);

    if (free_space_in_pkt > iq_data_num_bytes - iq_data_offset)
        iq_bytes_to_send = iq_data_num_bytes - iq_data_offset;
    else
        iq_bytes_to_send = free_space_in_pkt;

    /* don't cut off an iq in half */
    iq_bytes_to_send -= iq_bytes_to_send % alignment;

    void *iq_sam_buf = (void *)rte_pktmbuf_append(mbuf, iq_bytes_to_send);

    rte_memcpy(iq_sam_buf, (uint8_t *)iq_data_start + iq_data_offset,
            iq_bytes_to_send);

    return iq_bytes_to_send;
}

/**
 * @brief Builds compression header in xRAN packet
 *
 * @param mbuf Initialized rte_mbuf packet
 * @param compression_hdr Section compression header structure
 *                to be set in mbuf packet
 * @return int 0 on success, non zero on failure
 */
static int build_compression_hdr(
    struct rte_mbuf *mbuf,
    const struct data_section_compression_hdr *compr_hdr)
{
    struct data_section_compression_hdr *compression_hdr =
        (struct data_section_compression_hdr *)
        rte_pktmbuf_append(mbuf, sizeof(*compression_hdr));

    if (NULL == compression_hdr)
        return 1;

    rte_memcpy(compression_hdr, compr_hdr, sizeof(*compression_hdr));

    return 0;
}

/**
 * @brief Appends compression parameter in xRAN packet
 *
 * @param mbuf Initialized rte_mbuf packet
 * @param ud_comp_paramr Compression param to be set in mbuf packet
 * @return int 0 on success, non zero on failure
 */
static int append_comp_param(struct rte_mbuf *mbuf, union compression_params *ud_comp_param)
{
    union compression_params *compr_param =
        (union compression_params *)rte_pktmbuf_append(mbuf, sizeof(union compression_params));

    if (NULL == compr_param)
        return 1;

    rte_memcpy(compr_param, ud_comp_param, sizeof(union compression_params));

    return 0;
}

/**
 * @brief Function for starting preparion of IQ samples portions
 *        to be sent in xRAN packet
 *
 * @param mbuf Initialized rte_mbuf packet.
 * @param iq_data_start Address of the first element in IQ data array.
 * @param iq_data_num_bytes Size of the IQ data array.
 * @param iq_data_offset IQ data bytes already sent.
 * @param alignment Size of IQ data alignment.
 * @param pkt_gen_params Struct with parameters used for building packet
 * @return int Number of bytes that have been appended
               to the packet within a single data section appended.
 */
int xran_prepare_iq_symbol_portion(
    struct rte_mbuf *mbuf,
    const void *iq_data_start,
    const uint32_t iq_data_num_bytes,
    uint32_t *iq_data_offset,
    uint8_t alignment,
    struct xran_up_pkt_gen_params *params,
    int sub_seq_id)
{
    uint8_t i = 0;
    uint16_t iq_sam_bytes_sent = 0;

    if (build_ecpri_hdr(mbuf, iq_data_num_bytes, *iq_data_offset, alignment))
        return 0;

    if (build_application_layer(mbuf, &(params->app_params)) != 0)
        return 0;

    if (build_section_hdr(mbuf, &(params->sec_hdr)) != 0)
        return 0;

    if(params->compr_hdr_param.ud_comp_hdr.ud_comp_meth != XRAN_COMPMETHOD_NONE) {
        if (build_compression_hdr(mbuf, &(params->compr_hdr_param)) !=0)
            return 0;

        if(append_comp_param(mbuf, &(params->compr_param)) !=0)
            return 0;
        }

    return append_iq_samples(mbuf, iq_data_start, iq_data_num_bytes,
           (*iq_data_offset), alignment);
}

/**
 * @brief Function for extracting all IQ samples from xRAN packet
 *        holding a single data section
 * @param iq_data_start Address of the first element in IQ data array.
 * @param symb_id Symbol ID to be extracted from ecpri header
 * @param seq_id  Sequence ID to be extracted from radio header
 * @return int Size of remaining mbuf filled with IQ samples
               zero on failure
 */
int xran_extract_iq_samples(struct rte_mbuf *mbuf,
    void **iq_data_start,
    uint8_t *CC_ID,
    uint8_t *Ant_ID,
    uint8_t *frame_id,
    uint8_t *subframe_id,
    uint8_t *slot_id,
    uint8_t *symb_id,
    struct ecpri_seq_id *seq_id)
{
    uint32_t mlogVar[10];
    uint32_t mlogVarCnt = 0;
    struct xran_eaxc_info result;

    if (NULL == mbuf)
        return 0;
    if (NULL == iq_data_start)
        return 0;

    /* Process eCPRI header. */
    const struct xran_ecpri_hdr *ecpri_hdr = rte_pktmbuf_mtod(mbuf, void *);
    if (ecpri_hdr == NULL)
        return 0;

    if (seq_id)
        *seq_id = ecpri_hdr->ecpri_seq_id;

    xran_decompose_cid((uint16_t)ecpri_hdr->ecpri_xtc_id, &result);

    *CC_ID  = result.ccId;
    *Ant_ID = result.ruPortId;

    /* Process radio header. */
    struct radio_app_common_hdr *radio_hdr =
        (void *)rte_pktmbuf_adj(mbuf, sizeof(*ecpri_hdr));
    if (radio_hdr == NULL)
        return 0;       /* packet too short */

    radio_hdr->sf_slot_sym.value = rte_be_to_cpu_16(radio_hdr->sf_slot_sym.value);

    if (frame_id)
        *frame_id    = radio_hdr->frame_id;

    if (subframe_id)
        *subframe_id = radio_hdr->sf_slot_sym.subframe_id;

    if (slot_id)
        *slot_id     = radio_hdr->sf_slot_sym.slot_id;

    if (symb_id)
        *symb_id = radio_hdr->sf_slot_sym.symb_id;

    /* Process data section hdr */
    const struct data_section_hdr *data_hdr =
        (void *)rte_pktmbuf_adj(mbuf, sizeof(*radio_hdr));
    if (data_hdr == NULL)
        return 0;       /* packet too short */

#ifdef COMPRESSION
    const struct data_section_compression_hdr *data_compr_hdr =
        (void *) rte_pktmbuf_adj(mbuf, sizeof(*data_hdr));

    const uint8_t *compr_param =
        (void *)rte_pktmbuf_adj(mbuf, sizeof(*data_compr_hdr));

    *iq_data_start = rte_pktmbuf_adj(mbuf, sizeof(*compr_param));

#else
    *iq_data_start = rte_pktmbuf_adj(mbuf, sizeof(*data_hdr));
#endif
    if (*iq_data_start == NULL)
        return 0;

    mlogVar[mlogVarCnt++] = 0xBBBBBBB;
    mlogVar[mlogVarCnt++] = xran_lib_ota_tti;
    mlogVar[mlogVarCnt++] = radio_hdr->frame_id;
    mlogVar[mlogVarCnt++] = radio_hdr->sf_slot_sym.subframe_id;
    mlogVar[mlogVarCnt++] = radio_hdr->sf_slot_sym.slot_id;
    mlogVar[mlogVarCnt++] = radio_hdr->sf_slot_sym.symb_id;
    mlogVar[mlogVarCnt++] = rte_pktmbuf_pkt_len(mbuf);
    MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());

    return rte_pktmbuf_pkt_len(mbuf);
}

/**
 * @brief Function for starting preparion of IQ samples portions
 *        to be sent in xRAN packet
 *
 * @param mbuf Initialized rte_mbuf packet.
 * @param iq_data_start Address of the first element in IQ data array.
 * @param iq_data_num_bytes Size of the IQ data array.
 * @param iq_data_offset IQ data bytes already sent.
 * @param alignment Size of IQ data alignment.
 * @param pkt_gen_params Struct with parameters used for building packet
 * @return int Number of bytes that have been appended
               to the packet within all appended sections.
 */
int xran_prepare_iq_symbol_portion_no_comp(
                        struct rte_mbuf *mbuf,
                        const void *iq_data_start,
                        const uint32_t iq_data_num_bytes,
                        struct xran_up_pkt_gen_no_compression_params *params,
                        uint8_t CC_ID,
                        uint8_t Ant_ID,
                        uint8_t seq_id)
{
    if(xran_build_ecpri_hdr_ex(mbuf,
                           ECPRI_IQ_DATA,
                           iq_data_num_bytes,
                           CC_ID,
                           Ant_ID,
                           seq_id))
        return 0;

    if (build_application_layer(mbuf, &(params->app_params)) != 0)
        return 0;

    if (build_section_hdr(mbuf, &(params->sec_hdr)) != 0)
        return 0;

    return append_iq_samples_ex(mbuf, iq_data_start, iq_data_num_bytes);
}

