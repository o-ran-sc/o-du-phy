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
 * @brief This file provides the implementation of User Plane Messages APIs.
 *
 * @file xran_up_api.c
 * @ingroup group_lte_source_xran
 * @author Intel Corporation
 *
 **/
#include <inttypes.h>
#include <immintrin.h>
#include <rte_mbuf.h>

#include "xran_fh_o_du.h"
#include "xran_transport.h"
#include "xran_up_api.h"
#include "xran_printf.h"
#include "xran_mlog_lnx.h"
#include "xran_common.h"


#if 0
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

    ecpri_hdr->cmnhdr.data.data_num_1 = 0x0;
    ecpri_hdr->cmnhdr.bits.ecpri_ver = XRAN_ECPRI_VER;
    //ecpri_hdr->cmnhdr.bits.ecpri_resv = 0;
    //ecpri_hdr->cmnhdr.bits.ecpri_concat = 0;
    ecpri_hdr->cmnhdr.bits.ecpri_mesg_type = ECPRI_IQ_DATA;

    if (iq_data_offset + iq_samples_bytes_in_mbuf > iq_data_num_bytes) {
        ecpri_hdr->cmnhdr.bits.ecpri_payl_size =
            rte_cpu_to_be_16(sizeof(struct radio_app_common_hdr) +
                sizeof(struct data_section_hdr) +
                (iq_data_num_bytes - iq_data_offset) +
                XRAN_ECPRI_HDR_SZ); //xran_get_ecpri_hdr_size());
        ecpri_hdr->ecpri_seq_id.bits.e_bit = 1;  /* last segment */
    } else {
        ecpri_hdr->cmnhdr.bits.ecpri_payl_size =
            rte_cpu_to_be_16(sizeof(struct radio_app_common_hdr) +
                sizeof(struct data_section_hdr) +
                iq_samples_bytes_in_mbuf +
                XRAN_ECPRI_HDR_SZ); //xran_get_ecpri_hdr_size());
        ecpri_hdr->ecpri_seq_id.bits.e_bit = 0;
    }

    ecpri_hdr->ecpri_xtc_id = 0;    /* currently not used */
    ecpri_hdr->ecpri_seq_id.bits.sub_seq_id = iq_data_offset /
        iq_samples_bytes_in_mbuf;

    return 0;
}

#endif
/**
 * @brief Builds eCPRI header in xRAN packet
 *
 * @param mbuf Initialized rte_mbuf packet
 * @param ecpri_mesg_type eCPRI message type
 * @param payl_size the size in bytes of the payload part of eCPRI message
 * @param CC_ID Component Carrier ID for ecpriRtcid/ecpriPcid
 * @param Ant_ID Antenna ID for ecpriRtcid/ecpriPcid
 * @param seq_id Message identifier for eCPRI message
 * @param comp_meth Compression method
 * @return int int 0 on success, non zero on failure
 */
static inline int xran_build_ecpri_hdr_ex(struct rte_mbuf *mbuf,
                              uint8_t ecpri_mesg_type,
                              int payl_size,
                              uint8_t CC_ID,
                              uint8_t Ant_ID,
                              uint8_t seq_id,
                              uint8_t comp_meth,
                              enum xran_comp_hdr_type staticEn)
{
    char *pChar = rte_pktmbuf_mtod(mbuf, char*);
    struct xran_ecpri_hdr *ecpri_hdr = (struct xran_ecpri_hdr *)(pChar + sizeof(struct rte_ether_hdr));
    
    uint16_t    ecpri_payl_size = payl_size
                                + sizeof(struct radio_app_common_hdr)
                                + XRAN_ECPRI_HDR_SZ; //xran_get_ecpri_hdr_size();
    if (NULL == ecpri_hdr)
        return 1;

    ecpri_hdr->cmnhdr.data.data_num_1 = 0x0;
    ecpri_hdr->cmnhdr.bits.ecpri_ver       = XRAN_ECPRI_VER;
    //ecpri_hdr->cmnhdr.bits.ecpri_resv      = 0;     // should be zero
    //ecpri_hdr->cmnhdr.bits.ecpri_concat    = 0;
    ecpri_hdr->cmnhdr.bits.ecpri_mesg_type = ecpri_mesg_type;
    ecpri_hdr->cmnhdr.bits.ecpri_payl_size = rte_cpu_to_be_16(ecpri_payl_size);

    /* one to one lls-CU to RU only and band sector is the same */
    ecpri_hdr->ecpri_xtc_id = xran_compose_cid(0, 0, CC_ID, Ant_ID);

    /* no transport layer fragmentation supported */
    ecpri_hdr->ecpri_seq_id.data.data_num_1 = 0x8000;
    ecpri_hdr->ecpri_seq_id.bits.seq_id = seq_id;

    /* no transport layer fragmentation supported */
    //ecpri_hdr->ecpri_seq_id.sub_seq_id  = 0;
    //ecpri_hdr->ecpri_seq_id.e_bit       = 1;

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
static inline int build_application_layer(
    struct rte_mbuf *mbuf,
    const struct radio_app_common_hdr *app_hdr_input)
{
    char *pChar = rte_pktmbuf_mtod(mbuf, char*);
    struct radio_app_common_hdr *app_hdr = (struct radio_app_common_hdr *)(pChar + sizeof(struct rte_ether_hdr)
        + sizeof (struct xran_ecpri_hdr));

    if (NULL == app_hdr)
        return 1;

    memcpy(app_hdr, app_hdr_input, sizeof(struct radio_app_common_hdr));

    return 0;
}

/**
 * @brief Builds section header in xRAN packet
 *
 * @param mbuf Initialized rte_mbuf packet
 * @param sec_hdr Section header structure to be set in mbuf packet
 * @param offset Offset to create the section header
 * @return int 0 on success, non zero on failure
 */
static inline int build_section_hdr(
    struct rte_mbuf *mbuf,
    const struct data_section_hdr *sec_hdr,
    uint32_t offset)
{
    char *pChar = rte_pktmbuf_mtod(mbuf, char*);
    struct data_section_hdr *section_hdr = (struct data_section_hdr *)(pChar + offset);

    if (NULL == section_hdr)
        return 1;

    memcpy(section_hdr, &sec_hdr->fields.all_bits, sizeof(struct data_section_hdr));

    return 0;
}

#if 0
/**
 * @brief Function for appending IQ samples data to the mbuf.
 *
 * @param mbuf Initialized rte_mbuf packet.
 * @param iq_data_start Address of the first element in IQ data array.
 * @param iq_data_num_bytes Size of the IQ data array.
 * @param iq_data_offset IQ data btyes already sent.
 * @return uint16_t Bytes that have been appended to the packet.
 */
static inline uint16_t append_iq_samples_ex(
    struct rte_mbuf *mbuf,
    int iq_sam_offset,
    const void *iq_data_start,
    const uint32_t iq_data_num_bytes,
    enum xran_input_byte_order iq_buf_byte_order,
    uint32_t do_copy)
{
    char *pChar = rte_pktmbuf_mtod(mbuf, char*);
    void *iq_sam_buf;

    iq_sam_buf = (pChar + iq_sam_offset);
    if (iq_sam_buf == NULL){
        print_err("iq_sam_buf == NULL\n");
        return 0;
    }
    if(iq_buf_byte_order == XRAN_CPU_LE_BYTE_ORDER){
        int idx = 0;
        uint16_t *psrc = (uint16_t *)iq_data_start;
        uint16_t *pdst = (uint16_t *)iq_sam_buf;
        /* CPU byte order (le) of IQ to network byte order (be) */
        for (idx = 0; idx < iq_data_num_bytes/sizeof(int16_t); idx++){
            pdst[idx]  =  (psrc[idx]>>8) | (psrc[idx]<<8); //rte_cpu_to_be_16(psrc[idx]);
        }
    }

    else if(iq_buf_byte_order == XRAN_NE_BE_BYTE_ORDER){
        if(do_copy) {
           memcpy(iq_sam_buf, (uint8_t *)iq_data_start,  iq_data_num_bytes);
        }
    }

    return iq_data_num_bytes;
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

    memcpy(iq_sam_buf, (uint8_t *)iq_data_start + iq_data_offset,
            iq_bytes_to_send);

    return iq_bytes_to_send;
}
#endif

/**
 * @brief Builds compression header in xRAN packet
 *
 * @param mbuf Initialized rte_mbuf packet
 * @param compression_hdr Section compression header structure
 *                to be set in mbuf packet
 * @param offset mbuf data offset to create compression header
 * @return int 0 on success, non zero on failure
 */
static inline int build_compression_hdr(
    struct rte_mbuf *mbuf,
    const struct data_section_compression_hdr *compr_hdr,
    uint32_t offset)
{
    char *pChar = rte_pktmbuf_mtod(mbuf, char*);
    struct data_section_compression_hdr *compression_hdr = 
                    (struct data_section_compression_hdr *)(pChar + offset);

    if (NULL == compression_hdr)
        return 1;

    memcpy(compression_hdr, compr_hdr, sizeof(*compression_hdr));

    return 0;
}

#if 0
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

    memcpy(compr_param, ud_comp_param, sizeof(union compression_params));

    return 0;
}
#endif
/**
 * @brief Function for extracting all IQ samples from xRAN packet
 *        holding a single data section
 * @param iq_data_start Address of the first element in IQ data array.
 * @param symb_id Symbol ID to be extracted from ecpri header
 * @param seq_id  Sequence ID to be extracted from radio header
 * @return int Size of remaining mbuf filled with IQ samples
               zero on failure
 */
int32_t xran_extract_iq_samples(struct rte_mbuf *mbuf,
    void **iq_data_start,
    uint8_t *CC_ID,
    uint8_t *Ant_ID,
    uint8_t *frame_id,
    uint8_t *subframe_id,
    uint8_t *slot_id,
    uint8_t *symb_id,
    union ecpri_seq_id *seq_id,
    uint16_t *num_prbu,
    uint16_t *start_prbu,
    uint16_t *sym_inc,
    uint16_t *rb,
    uint16_t *sect_id,
    int8_t   expect_comp,
    enum xran_comp_hdr_type staticComp,
    uint8_t *compMeth,
    uint8_t *iqWidth)
{
#if XRAN_MLOG_VAR
    uint32_t mlogVar[10];
    uint32_t mlogVarCnt = 0;
#endif
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

    if(*CC_ID == 0xFF && *Ant_ID == 0xFF) {
        /* if not classified vi HW Queue parse packet  */
    xran_decompose_cid((uint16_t)ecpri_hdr->ecpri_xtc_id, &result);

    *CC_ID  = result.ccId;
    *Ant_ID = result.ruPortId;
    }

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
        *slot_id     = xran_slotid_convert(radio_hdr->sf_slot_sym.slot_id, 1);

    if (symb_id)
        *symb_id = radio_hdr->sf_slot_sym.symb_id;

    /* Process data section hdr */
    struct data_section_hdr *data_hdr =
        (void *)rte_pktmbuf_adj(mbuf, sizeof(*radio_hdr));
    if (data_hdr == NULL)
        return 0;       /* packet too short */

    /* cpu byte order */
    data_hdr->fields.all_bits  = rte_be_to_cpu_32(data_hdr->fields.all_bits);

    *num_prbu   = data_hdr->fields.num_prbu;
    *start_prbu = data_hdr->fields.start_prbu;
    *sym_inc    = data_hdr->fields.sym_inc;
    *rb         = data_hdr->fields.rb;
    *sect_id    = data_hdr->fields.sect_id;

    if(expect_comp) {
            const struct data_section_compression_hdr *data_compr_hdr;
        if (staticComp != XRAN_COMP_HDR_TYPE_STATIC)
        {
            data_compr_hdr =
            (void *) rte_pktmbuf_adj(mbuf, sizeof(*data_hdr));

        if (data_compr_hdr == NULL)
            return 0;

        *compMeth = data_compr_hdr->ud_comp_hdr.ud_comp_meth;
        *iqWidth =  data_compr_hdr->ud_comp_hdr.ud_iq_width;
        const uint8_t *compr_param =
            (void *)rte_pktmbuf_adj(mbuf, sizeof(*data_compr_hdr));

            *iq_data_start = (void *)compr_param; /*rte_pktmbuf_adj(mbuf, sizeof(*compr_param))*/;
        }
        else
        {
            *iq_data_start = rte_pktmbuf_adj(mbuf, sizeof(*data_hdr));
        }


    } else {
        *iq_data_start = rte_pktmbuf_adj(mbuf, sizeof(*data_hdr));
    }

    if (*iq_data_start == NULL)
        return 0;

#if XRAN_MLOG_VAR
    mlogVar[mlogVarCnt++] = 0xBBBBBBBB;
    mlogVar[mlogVarCnt++] = radio_hdr->frame_id;
    mlogVar[mlogVarCnt++] = radio_hdr->sf_slot_sym.subframe_id;
    mlogVar[mlogVarCnt++] = radio_hdr->sf_slot_sym.slot_id;
    mlogVar[mlogVarCnt++] = radio_hdr->sf_slot_sym.symb_id;
    mlogVar[mlogVarCnt++] = data_hdr->fields.sect_id;
    mlogVar[mlogVarCnt++] = data_hdr->fields.start_prbu;
    mlogVar[mlogVarCnt++] = data_hdr->fields.num_prbu;
    mlogVar[mlogVarCnt++] = rte_pktmbuf_pkt_len(mbuf);
    MLogAddVariables(mlogVarCnt, mlogVar, MLogTick());
#endif

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
 * @param num_sections Number of data sections to be created
 * @return int Number of bytes that have been appended
               to the packet within all appended sections.
 */
int32_t xran_prepare_iq_symbol_portion(
                        struct rte_mbuf *mbuf,
                        const void *iq_data_start,
                        const enum xran_input_byte_order iq_buf_byte_order,
                        const uint32_t iq_data_num_bytes,
                        struct xran_up_pkt_gen_params *params,
                        uint8_t CC_ID,
                        uint8_t Ant_ID,
                        uint8_t seq_id,
                        enum xran_comp_hdr_type staticEn,
                        uint32_t do_copy,
                        uint16_t num_sections,
                        uint16_t section_id_start,
                        uint16_t iq_offset)
{
    uint32_t offset=0 , ret_val=0;
    uint16_t idx , iq_len=0;
    const void *iq_data;
    uint16_t iq_n_section_size; //All data_section + compression hdrs + iq

    iq_n_section_size = iq_data_num_bytes + num_sections*sizeof(struct data_section_hdr);
    
    if ((params[0].compr_hdr_param.ud_comp_hdr.ud_comp_meth != XRAN_COMPMETHOD_NONE)&&(staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC))
{
        iq_n_section_size += num_sections*sizeof(struct data_section_compression_hdr);
    }

    if(xran_build_ecpri_hdr_ex(mbuf,
                           ECPRI_IQ_DATA,
                           (int)iq_n_section_size,
                           CC_ID,
                           Ant_ID,
                           seq_id,
                           params[0].compr_hdr_param.ud_comp_hdr.ud_comp_meth,
                           staticEn)){
        print_err("xran_build_ecpri_hdr_ex return 0\n");
        return 0;
    }

    if (build_application_layer(mbuf, &(params[0].app_params)) != 0){
        print_err("build_application_layer return != 0\n");
        return 0;
    }

    offset = sizeof(struct rte_ether_hdr)
                + sizeof(struct xran_ecpri_hdr)
                + sizeof(struct radio_app_common_hdr);
    for(idx=0 ; idx < num_sections ; idx++)
    {
        if (build_section_hdr(mbuf, &(params[idx].sec_hdr),offset) != 0){
        print_err("build_section_hdr return != 0\n");
        return 0;
    }
        offset += sizeof(struct data_section_hdr);
        if ((params[idx].compr_hdr_param.ud_comp_hdr.ud_comp_meth != XRAN_COMPMETHOD_NONE)&&(staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC)) {
            if (build_compression_hdr(mbuf, &(params[idx].compr_hdr_param),offset) !=0)
            return 0;
            
        offset += sizeof(struct data_section_compression_hdr);
    }

        /** IQ buffer contains space for data section/compression hdr in case of multiple sections.*/
        iq_data = (const void *)((uint8_t *)iq_data_start 
                                + idx*(sizeof(struct data_section_hdr) + iq_data_num_bytes/num_sections));
        
        if ((params[idx].compr_hdr_param.ud_comp_hdr.ud_comp_meth != XRAN_COMPMETHOD_NONE)&&(staticEn == XRAN_COMP_HDR_TYPE_DYNAMIC))
            iq_data = (const void *)((uint8_t *)iq_data + idx*sizeof(struct data_section_compression_hdr));

        //ret_val = (do_copy ? append_iq_samples_ex(mbuf, offset, iq_data_start, iq_data_num_bytes/num_sections, iq_buf_byte_order, do_copy) : iq_data_num_bytes/num_sections);
        ret_val = iq_data_num_bytes/num_sections;
        
        if(!ret_val)
            return ret_val;
        
        iq_len += ret_val;
        offset += ret_val;
    }
    return iq_len;
}

