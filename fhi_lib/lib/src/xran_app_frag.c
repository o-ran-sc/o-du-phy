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
 * @brief xRAN application fragmentation for U-plane packets
 *
 * @file xran_app_frag.c
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <immintrin.h>
#include <rte_mbuf.h>
#include <rte_memcpy.h>
#include <rte_mempool.h>
#include <rte_debug.h>

#include "xran_app_frag.h"
#include "xran_cp_api.h"
#include "xran_pkt_up.h"
#include "xran_printf.h"
#include "xran_common.h"

static inline void __fill_xranhdr_frag(struct xran_up_pkt_hdr *dst,
        const struct xran_up_pkt_hdr *src, uint16_t rblen_bytes,
        uint16_t rboff_bytes, uint16_t  startPrbc,  uint16_t numPrbc, uint32_t mf, uint8_t *seqid, uint8_t iqWidth)
{
    struct data_section_hdr loc_data_sec_hdr;
    struct xran_ecpri_hdr loc_ecpri_hdr;

    rte_memcpy(dst, src, sizeof(*dst));

    dst->ecpri_hdr.ecpri_seq_id.bits.seq_id = (*seqid)++;

    print_dbg("sec [%d %d] sec %d mf %d g_sec %d\n",startPrbc, numPrbc, dst->ecpri_hdr.ecpri_seq_id.seq_id, mf, *seqid);

    loc_data_sec_hdr.fields.all_bits = rte_be_to_cpu_32(dst->data_sec_hdr.fields.all_bits);

    /* update RBs */
    loc_data_sec_hdr.fields.start_prbu = startPrbc + rboff_bytes/XRAN_PAYLOAD_1_RB_SZ(iqWidth);
    loc_data_sec_hdr.fields.num_prbu   = rblen_bytes/XRAN_PAYLOAD_1_RB_SZ(iqWidth);

    print_dbg("sec [%d %d] pkt [%d %d] rboff_bytes %d rblen_bytes %d\n",startPrbc, numPrbc, loc_data_sec_hdr.fields.start_prbu, loc_data_sec_hdr.fields.num_prbu,
        rboff_bytes, rblen_bytes);

    dst->data_sec_hdr.fields.all_bits = rte_cpu_to_be_32(loc_data_sec_hdr.fields.all_bits);

    dst->ecpri_hdr.cmnhdr.bits.ecpri_payl_size = rte_cpu_to_be_16(sizeof(struct radio_app_common_hdr) +
                sizeof(struct data_section_hdr) + rblen_bytes + xran_get_ecpri_hdr_size());
}

static inline void __fill_xranhdr_frag_comp(struct xran_up_pkt_hdr_comp *dst,
        const struct xran_up_pkt_hdr_comp *src, uint16_t rblen_bytes,
        uint16_t rboff_bytes, uint16_t  startPrbc,  uint16_t numPrbc,  uint32_t mf, uint8_t *seqid, uint8_t iqWidth)
{
    struct data_section_hdr loc_data_sec_hdr;
    struct xran_ecpri_hdr loc_ecpri_hdr;

    rte_memcpy(dst, src, sizeof(*dst));

    dst->ecpri_hdr.ecpri_seq_id.bits.seq_id = (*seqid)++;

    print_dbg("sec [%d %d] sec %d mf %d g_sec %d\n", startPrbc, numPrbc, dst->ecpri_hdr.ecpri_seq_id.seq_id, mf, *seqid);

    loc_data_sec_hdr.fields.all_bits = rte_be_to_cpu_32(dst->data_sec_hdr.fields.all_bits);

    /* update RBs */
    loc_data_sec_hdr.fields.start_prbu = startPrbc + rboff_bytes/XRAN_PAYLOAD_1_RB_SZ(iqWidth);
    loc_data_sec_hdr.fields.num_prbu   = rblen_bytes/XRAN_PAYLOAD_1_RB_SZ(iqWidth);

    print_dbg("sec [%d %d] pkt [%d %d] rboff_bytes %d rblen_bytes %d\n",startPrbc, numPrbc, loc_data_sec_hdr.fields.start_prbu, loc_data_sec_hdr.fields.num_prbu,
        rboff_bytes, rblen_bytes);

    dst->data_sec_hdr.fields.all_bits = rte_cpu_to_be_32(loc_data_sec_hdr.fields.all_bits);

    dst->ecpri_hdr.cmnhdr.bits.ecpri_payl_size = rte_cpu_to_be_16(sizeof(struct radio_app_common_hdr) +
            sizeof(struct data_section_hdr) + sizeof(struct data_section_compression_hdr) + rblen_bytes + xran_get_ecpri_hdr_size());
}



static inline void __free_fragments(struct rte_mbuf *mb[], uint32_t num)
{
    uint32_t i;
    for (i = 0; i != num; i++)
        rte_pktmbuf_free(mb[i]);
}

/**
 * XRAN fragmentation.
 *
 * This function implements the application fragmentation of XRAN packets.
 *
 * @param pkt_in
 *   The input packet.
 * @param pkts_out
 *   Array storing the output fragments.
 * @param mtu_size
 *   Size in bytes of the Maximum Transfer Unit (MTU) for the outgoing XRAN
 *   datagrams. This value includes the size of the XRAN headers.
 * @param pool_direct
 *   MBUF pool used for allocating direct buffers for the output fragments.
 * @param pool_indirect
 *   MBUF pool used for allocating indirect buffers for the output fragments.
 * @return
 *   Upon successful completion - number of output fragments placed
 *   in the pkts_out array.
 *   Otherwise - (-1) * <errno>.
 */
int32_t
xran_app_fragment_packet(struct rte_mbuf *pkt_in, /* eth hdr is prepended */
    struct rte_mbuf **pkts_out,
    uint16_t nb_pkts_out,
    uint16_t mtu_size,
    struct rte_mempool *pool_direct,
    struct rte_mempool *pool_indirect,
    int16_t nRBStart,  /**< start RB of RB allocation */
    int16_t nRBSize,  /**< number of RBs used */
    uint8_t *seqid,
    uint8_t iqWidth,
    uint8_t isUdCompHdr)
{
    struct rte_mbuf *in_seg = NULL;
    uint32_t out_pkt_pos =  0, in_seg_data_pos = 0;
    uint32_t more_in_segs;
    uint16_t fragment_offset, frag_size;
    uint16_t frag_bytes_remaining;
    struct eth_xran_up_pkt_hdr *in_hdr;
    struct xran_up_pkt_hdr *in_hdr_xran;

    struct eth_xran_up_pkt_hdr_comp *in_hdr_comp = NULL;
    struct xran_up_pkt_hdr_comp *in_hdr_xran_comp = NULL;

    int32_t eth_xran_up_headers_sz =  0;
    eth_xran_up_headers_sz = sizeof(struct eth_xran_up_pkt_hdr);

    if(isUdCompHdr)
        eth_xran_up_headers_sz += sizeof(struct data_section_compression_hdr);

    /*
     * Ensure the XRAN payload length of all fragments is aligned to a
     * multiple of 48 bytes (1 RB with IQ of 16 bits each)
     */
    frag_size = ((mtu_size - eth_xran_up_headers_sz - RTE_PKTMBUF_HEADROOM)/XRAN_PAYLOAD_1_RB_SZ(iqWidth))*XRAN_PAYLOAD_1_RB_SZ(iqWidth);

    print_dbg("frag_size %d\n",frag_size);

    if(isUdCompHdr){
        in_hdr_comp = rte_pktmbuf_mtod(pkt_in, struct eth_xran_up_pkt_hdr_comp*);
        in_hdr_xran_comp = &in_hdr_comp->xran_hdr;
        if (unlikely(frag_size * nb_pkts_out <
            (uint16_t)(pkt_in->pkt_len - sizeof (struct xran_up_pkt_hdr_comp)))){
            print_err("-EINVAL\n");
            return -EINVAL;
        }
    }else {
    in_hdr = rte_pktmbuf_mtod(pkt_in, struct eth_xran_up_pkt_hdr *);
    in_hdr_xran = &in_hdr->xran_hdr;
    /* Check that pkts_out is big enough to hold all fragments */
    if (unlikely(frag_size * nb_pkts_out <
        (uint16_t)(pkt_in->pkt_len - sizeof (struct xran_up_pkt_hdr)))){
        print_err("-EINVAL\n");
        return -EINVAL;
    }
    }

    in_seg = pkt_in;
    if(isUdCompHdr){
        in_seg_data_pos = sizeof(struct eth_xran_up_pkt_hdr_comp);
    }else{
    in_seg_data_pos = sizeof(struct eth_xran_up_pkt_hdr);
    }
    out_pkt_pos = 0;
    fragment_offset = 0;

    more_in_segs = 1;
    while (likely(more_in_segs)) {
        struct rte_mbuf *out_pkt = NULL, *out_seg_prev = NULL;
        uint32_t more_out_segs;
        struct xran_up_pkt_hdr *out_hdr;
        struct xran_up_pkt_hdr_comp *out_hdr_comp;

        /* Allocate direct buffer */
        out_pkt = rte_pktmbuf_alloc(pool_direct);
        if (unlikely(out_pkt == NULL)) {
            print_err("pool_direct -ENOMEM\n");
            __free_fragments(pkts_out, out_pkt_pos);
            return -ENOMEM;
        }

        print_dbg("[%d] out_pkt %p\n",more_in_segs, out_pkt);

        /* Reserve space for the XRAN header that will be built later */
        //out_pkt->data_len = sizeof(struct xran_up_pkt_hdr);
         //out_pkt->pkt_len = sizeof(struct xran_up_pkt_hdr);
        if(isUdCompHdr){
            if(rte_pktmbuf_append(out_pkt, sizeof(struct xran_up_pkt_hdr_comp)) ==NULL){
                rte_panic("sizeof(struct xran_up_pkt_hdr)");
            }
        }else{
        if(rte_pktmbuf_append(out_pkt, sizeof(struct xran_up_pkt_hdr)) ==NULL){
            rte_panic("sizeof(struct xran_up_pkt_hdr)");
        }
        }

        frag_bytes_remaining = frag_size;

        out_seg_prev = out_pkt;
        more_out_segs = 1;
        while (likely(more_out_segs && more_in_segs)) {
            uint32_t len;
#ifdef XRAN_ATTACH_MBUF
            struct rte_mbuf *out_seg = NULL;

            /* Allocate indirect buffer */
            print_dbg("Allocate indirect buffer \n");
            out_seg = rte_pktmbuf_alloc(pool_indirect);
            if (unlikely(out_seg == NULL)) {
                print_err("pool_indirect -ENOMEM\n");
                rte_pktmbuf_free(out_pkt);
                __free_fragments(pkts_out, out_pkt_pos);
                return -ENOMEM;
            }

            print_dbg("[%d %d] out_seg %p\n",more_out_segs, more_in_segs, out_seg);
            out_seg_prev->next = out_seg;
            out_seg_prev = out_seg;

            /* Prepare indirect buffer */
            rte_pktmbuf_attach(out_seg, in_seg);
#endif
            len = frag_bytes_remaining;
            if (len > (in_seg->data_len - in_seg_data_pos)) {
                len = in_seg->data_len - in_seg_data_pos;
            }
#ifdef XRAN_ATTACH_MBUF
            out_seg->data_off = in_seg->data_off + in_seg_data_pos;
            out_seg->data_len = (uint16_t)len;
            out_pkt->pkt_len = (uint16_t)(len +
                out_pkt->pkt_len);
            out_pkt->nb_segs += 1;
#else
{
            char* pChar   = rte_pktmbuf_mtod(in_seg, char*);
            void *iq_src  = (pChar + in_seg_data_pos);
            void *iq_dst  = rte_pktmbuf_append(out_pkt, len);

            print_dbg("rte_pktmbuf_attach\n");
            if(iq_src && iq_dst)
                rte_memcpy(iq_dst, iq_src, len);
            else
                print_err("iq_src %p iq_dst %p\n len %d room %d\n", iq_src, iq_dst, len, rte_pktmbuf_tailroom(out_pkt));
}
#endif
            in_seg_data_pos += len;
            frag_bytes_remaining -= len;

            /* Current output packet (i.e. fragment) done ? */
            if (unlikely(frag_bytes_remaining == 0))
                more_out_segs = 0;

            /* Current input segment done ? */
            if (unlikely(in_seg_data_pos == in_seg->data_len)) {
                in_seg = in_seg->next;
                in_seg_data_pos = 0;

                if (unlikely(in_seg == NULL))
                    more_in_segs = 0;
            }
        }

        /* Build the XRAN header */
        print_dbg("Build the XRAN header\n");


        if(isUdCompHdr){
            out_hdr_comp = rte_pktmbuf_mtod(out_pkt, struct xran_up_pkt_hdr_comp*);
            __fill_xranhdr_frag_comp(out_hdr_comp, in_hdr_xran_comp,
                (uint16_t)out_pkt->pkt_len - sizeof(struct xran_up_pkt_hdr_comp),
                fragment_offset, nRBStart, nRBSize,  more_in_segs, seqid, iqWidth);

            fragment_offset = (uint16_t)(fragment_offset +
                out_pkt->pkt_len - sizeof(struct xran_up_pkt_hdr_comp));
        } else {
            out_hdr = rte_pktmbuf_mtod(out_pkt, struct xran_up_pkt_hdr *);
        __fill_xranhdr_frag(out_hdr, in_hdr_xran,
            (uint16_t)out_pkt->pkt_len - sizeof(struct xran_up_pkt_hdr),
                fragment_offset, nRBStart, nRBSize, more_in_segs, seqid, iqWidth);

        fragment_offset = (uint16_t)(fragment_offset +
            out_pkt->pkt_len - sizeof(struct xran_up_pkt_hdr));
        }

        //out_pkt->l3_len = sizeof(struct xran_up_pkt_hdr);

        /* Write the fragment to the output list */
        pkts_out[out_pkt_pos] = out_pkt;
        print_dbg("out_pkt_pos %d data_len %d pkt_len %d\n", out_pkt_pos, out_pkt->data_len, out_pkt->pkt_len);
        out_pkt_pos ++;
        //rte_pktmbuf_dump(stdout, out_pkt, 96);
    }

    return out_pkt_pos;
}


