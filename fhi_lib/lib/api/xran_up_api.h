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
 * @brief This file provides the definitions for User Plane Messages APIs.
 *
 * @file xran_up_api.h
 * @ingroup group_lte_source_xran
 * @author Intel Corporation
 *
 **/

#ifndef _XRAN_UP_API_H_
#define _XRAN_UP_API_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <rte_mbuf.h>

#include "xran_pkt.h"
#include "xran_pkt_up.h"

/*
 * structure used for storing packet parameters needed for generating
 * a data packet
 */
struct xran_up_pkt_gen_params
{
    struct radio_app_common_hdr app_params;
    struct data_section_hdr sec_hdr;
    struct data_section_compression_hdr compr_hdr_param;
    union compression_params compr_param;
};

/*
 * structure used for storing packet parameters needed for generating
 * a data packet without compression
 *   Next fields are omitted:
 *        udCompHdr (not always present)
 *        reserved (not always present)
 *        udCompParam (not always present)
 */
struct xran_up_pkt_gen_no_compression_params
{
    struct radio_app_common_hdr app_params;
    struct data_section_hdr sec_hdr;
};


/**
 * @brief Function that is preparing an mbuf with portion of IQ samples related
 *        to the single symbol.
 *
 * @param mbuf Initialized rte_mbuf packet
 * @param iq_data_start Address of the first element in IQ data array.
 * @param iq_data_num_elements Size of the IQ data array.
 * @param iq_data_offset IQ data array's elements already sent.
 * @param alignment Align data to this many bytes.
 * @param params Structure containing Radio App Header and Data Section Header
 *               structures.
 * @return int Bytes of IQ samples that have been appended to the packet.
 */
int xran_prepare_iq_symbol_portion(
    struct rte_mbuf *mbuf,
    const void *iq_data_start,
    const uint32_t iq_data_num_bytes,
    uint32_t *iq_data_offset,
    const uint8_t alignment,
    struct xran_up_pkt_gen_params *params,
    int sub_seq_id);

/**
 * @brief Function extracts IQ samples from received mbuf packet.
 *
 * @param mbuf Packet with received data.
 * @param iq_data_start Address of the first IQ sample in mbuf will be returned
 *                      here
 * @return int Bytes of IQ samples that have been extracted from mbuf.
 */
int xran_extract_iq_samples(struct rte_mbuf *mbuf,
    void **iq_data_start,
    uint8_t *CC_ID,
    uint8_t *Ant_ID,
    uint8_t *frame_id,
    uint8_t *subframe_id,
    uint8_t *slot_id,
    uint8_t *symb_id,
    struct ecpri_seq_id *seq_id,
    uint16_t *num_prbu,
    uint16_t *start_prbu,
    uint16_t *sym_inc,
    uint16_t *rb,
    uint16_t *sect_id);

int xran_prepare_iq_symbol_portion_no_comp(
                        struct rte_mbuf *mbuf,
                        const void *iq_data_start,
                        const enum xran_input_byte_order iq_buf_byte_order,
                        const uint32_t iq_data_num_bytes,
                        struct xran_up_pkt_gen_no_compression_params *params,
                        uint8_t CC_ID,
                        uint8_t Ant_ID,
                        uint8_t seq_id,
                        uint32_t do_copy);

#ifdef __cplusplus
}
#endif

#endif /* _XRAN_UP_API_H_ */
