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

#ifndef _XRAN_COMMON_
#define _XRAN_COMMON_

/**
 * @brief XRAN layer common functionality for both lls-CU and RU as well as C-plane and
 *    U-plane
 * @file xran_common.c
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#include <assert.h>
#include <err.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>

#include "xran_common.h"
#include "ethdi.h"
#include "xran_pkt.h"
#include "xran_pkt_up.h"
#include "xran_cp_api.h"
#include "xran_up_api.h"
#include "../src/xran_printf.h"

#ifndef MLOG_ENABLED
#include "mlog_lnx_xRAN.h"
#else
#include "mlog_lnx.h"
#endif

#define MBUFS_CNT 256

extern int xran_process_rx_sym(void *arg,
                        void *iq_data_start,
                        uint16_t size,
                        uint8_t CC_ID,
                        uint8_t Ant_ID,
                        uint8_t frame_id,
                        uint8_t subframe_id,
                        uint8_t slot_id,
                        uint8_t symb_id);


int process_mbuf(struct rte_mbuf *pkt)
{
    void *iq_samp_buf;
    struct ecpri_seq_id seq;
    static int symbol_total_bytes = 0;
    int num_bytes = 0;
    struct xran_ethdi_ctx *const ctx = xran_ethdi_get_ctx();
    uint8_t CC_ID = 0;
    uint8_t Ant_ID = 0;
    uint8_t frame_id = 0;
    uint8_t subframe_id = 0;
    uint8_t slot_id = 0;
    uint8_t symb_id = 0;

    num_bytes = xran_extract_iq_samples(pkt,
                                        &iq_samp_buf,
                                        &CC_ID,
                                        &Ant_ID,
                                        &frame_id,
                                        &subframe_id,
                                        &slot_id,
                                        &symb_id,
                                        &seq);
    if (num_bytes <= 0)
        return -1;

    symbol_total_bytes += num_bytes;

    if (seq.e_bit == 1) {
        print_dbg("Completed receiving symbol %d, size=%d bytes\n",
            symb_id, symbol_total_bytes);

        if (symbol_total_bytes)
            xran_process_rx_sym(NULL,
                            iq_samp_buf,
                            symbol_total_bytes,
                            CC_ID,
                            Ant_ID,
                            frame_id,
                            subframe_id,
                            slot_id,
                            symb_id);
        symbol_total_bytes = 0;
    }

    return 0;
}

static int set_iq_bit_width(uint8_t iq_bit_width, struct data_section_compression_hdr *compr_hdr)
{
    if (iq_bit_width == MAX_IQ_BIT_WIDTH)
        compr_hdr->ud_comp_hdr.ud_iq_width = (uint8_t) 0;
    else
        compr_hdr->ud_comp_hdr.ud_iq_width = iq_bit_width;

    return  0;

}

/* Send a single 5G symbol over multiple packets */
int send_symbol_ex(enum xran_pkt_dir direction,
                uint16_t section_id,
                struct rb_map *data,
                uint8_t frame_id,
                uint8_t subframe_id,
                uint8_t slot_id,
                uint8_t symbol_no,
                int prb_start,
                int prb_num,
                uint8_t CC_ID,
                uint8_t RU_Port_ID,
                uint8_t seq_id)
{
    const int n_bytes = prb_num * N_SC_PER_PRB * sizeof(struct rb_map);
    int sent;
    uint32_t off;
    struct xran_up_pkt_gen_no_compression_params xp = { 0 };

    /* radio app header */
    xp.app_params.data_direction = direction;
    xp.app_params.payl_ver       = 1;
    xp.app_params.filter_id      = 0;
    xp.app_params.frame_id       = frame_id;
    xp.app_params.sf_slot_sym.subframe_id    = subframe_id;
    xp.app_params.sf_slot_sym.slot_id        = slot_id;
    xp.app_params.sf_slot_sym.symb_id        = symbol_no;

    /* convert to network byte order */
    xp.app_params.sf_slot_sym.value = rte_cpu_to_be_16(xp.app_params.sf_slot_sym.value);

    xp.sec_hdr.fields.sect_id    = section_id;
    xp.sec_hdr.fields.num_prbu   = (uint8_t)prb_num;
    xp.sec_hdr.fields.start_prbu = (uint8_t)prb_start;
    xp.sec_hdr.fields.sym_inc    = 0;
    xp.sec_hdr.fields.rb         = 0;

    /* network byte order */
    xp.sec_hdr.fields.all_bits  = rte_cpu_to_be_32(xp.sec_hdr.fields.all_bits);

    struct rte_mbuf *mb = xran_ethdi_mbuf_alloc();

    if (mb == NULL){
        MLogPrint(NULL);
        errx(1, "out of mbufs after %d packets", 1);
    }

    sent = xran_prepare_iq_symbol_portion_no_comp(mb,
                                                  data,
                                                  n_bytes,
                                                  &xp,
                                                  CC_ID,
                                                  RU_Port_ID,
                                                  seq_id);
    if (sent <= 0)
        errx(1, "failed preparing symbol");

    xran_ethdi_mbuf_send(mb, ETHER_TYPE_ECPRI);

#ifdef DEBUG
    printf("Symbol %2d sent (%d packets, %d bytes)\n", symbol_no, i, n_bytes);
#endif

    return sent;
}

int send_cpmsg_dlul(void *pHandle, enum xran_pkt_dir dir,
                uint8_t frame_id, uint8_t subframe_id, uint8_t slot_id,
                uint8_t startsym, uint8_t numsym, int prb_num,
                uint16_t beam_id,
                uint8_t cc_id, uint8_t ru_port_id,
                uint8_t seq_id)
{
  struct xran_cp_gen_params params;
  struct xran_section_gen_info sect_geninfo[XRAN_MAX_NUM_SECTIONS];
  struct rte_mbuf *mbuf;
  int ret, nsection, i;


    params.dir                  = dir;
    params.sectionType          = XRAN_CP_SECTIONTYPE_1;     // Most DL/UL Radio Channels
    params.hdr.filterIdx        = XRAN_FILTERINDEX_STANDARD;
    params.hdr.frameId          = frame_id;
    params.hdr.subframeId       = subframe_id;
    params.hdr.slotId           = slot_id;
    params.hdr.startSymId       = startsym;                 // start Symbol ID
    params.hdr.iqWidth          = xran_get_conf_iqwidth(pHandle);
    params.hdr.compMeth         = xran_get_conf_compmethod(pHandle);

    nsection = 0;
    sect_geninfo[nsection].info.type        = params.sectionType;
    sect_geninfo[nsection].info.id          = xran_alloc_sectionid(pHandle, dir, cc_id, ru_port_id, slot_id);
    sect_geninfo[nsection].info.rb          = XRAN_RBIND_EVERY;
    sect_geninfo[nsection].info.symInc      = XRAN_SYMBOLNUMBER_NOTINC;
    sect_geninfo[nsection].info.startPrbc   = 0;
    sect_geninfo[nsection].info.numPrbc     = NUM_OF_PRB_IN_FULL_BAND,
    sect_geninfo[nsection].info.numSymbol   = numsym;
    sect_geninfo[nsection].info.reMask      = 0xfff;
    sect_geninfo[nsection].info.beamId      = beam_id;

    sect_geninfo[nsection].info.ef          = 0;      // no extension
    sect_geninfo[nsection].exDataSize       = 0;
    sect_geninfo[nsection].exData           = NULL;
    nsection++;

    params.numSections          = nsection;
    params.sections             = sect_geninfo;

    mbuf = xran_ethdi_mbuf_alloc();
    if(unlikely(mbuf == NULL)) {
        print_err("Alloc fail!\n");
        return (-1);
        }

    ret = xran_prepare_ctrl_pkt(mbuf, &params, cc_id, ru_port_id, seq_id);
    if(ret < 0) {
        print_err("Fail to build control plane packet - [%d:%d:%d] dir=%d\n",
                    frame_id, subframe_id, slot_id, dir);
        }
    else {
        xran_ethdi_mbuf_send_cp(mbuf, ETHER_TYPE_ECPRI);
        for(i=0; i<nsection; i++)
            xran_cp_add_section_info(pHandle,
                    dir, cc_id, ru_port_id, subframe_id, slot_id,
                    &sect_geninfo[i].info);
        }

    return (ret);
}

int send_cpmsg_prach(void *pHandle,
                uint8_t frame_id, uint8_t subframe_id, uint8_t slot_id,
                uint16_t beam_id, uint8_t cc_id, uint8_t prach_port_id,
                uint8_t seq_id)
{
    struct xran_cp_gen_params params;
    struct xran_section_gen_info sect_geninfo[8];
    struct rte_mbuf *mbuf;
    int i, nsection, ret;
    struct xran_lib_ctx *pxran_lib_ctx = xran_lib_get_ctx();
    xRANPrachCPConfigStruct *pPrachCPConfig = &(pxran_lib_ctx->PrachCPConfig);

#if 0
    printf("%d:%d:%d:%d - filter=%d, startSym=%d[%d:%d], numSym=%d, occasions=%d, freqOff=%d\n",
                frame_id, subframe_id, slot_id, prach_port_id,
                pPrachCPConfig->filterIdx,
                pPrachCPConfig->startSymId,
                pPrachCPConfig->startPrbc,
                pPrachCPConfig->numPrbc,
                pPrachCPConfig->numSymbol,
                pPrachCPConfig->occassionsInPrachSlot,
                pPrachCPConfig->freqOffset);
#endif

    params.dir                  = XRAN_DIR_UL;
    params.sectionType          = XRAN_CP_SECTIONTYPE_3;
    params.hdr.filterIdx        = pPrachCPConfig->filterIdx;
    params.hdr.frameId          = frame_id;
    params.hdr.subframeId       = subframe_id;
    params.hdr.slotId           = slot_id;
    params.hdr.startSymId       = pPrachCPConfig->startSymId;
    params.hdr.iqWidth          = xran_get_conf_iqwidth(pHandle);
    params.hdr.compMeth         = xran_get_conf_compmethod(pHandle);
        /* use timeOffset field for the CP length value for prach sequence */
    params.hdr.timeOffset       = pPrachCPConfig->timeOffset;
    params.hdr.fftSize          = xran_get_conf_fftsize(pHandle);
    params.hdr.scs              = xran_get_conf_prach_scs(pHandle);
    params.hdr.cpLength         = 0;

    nsection = 0;
    sect_geninfo[nsection].info.type      = params.sectionType;
    sect_geninfo[nsection].info.id        = xran_alloc_sectionid(pHandle, XRAN_DIR_UL, cc_id, prach_port_id, slot_id);
    sect_geninfo[nsection].info.rb        = XRAN_RBIND_EVERY;
    sect_geninfo[nsection].info.symInc    = XRAN_SYMBOLNUMBER_NOTINC;
    sect_geninfo[nsection].info.startPrbc = pPrachCPConfig->startPrbc;
    sect_geninfo[nsection].info.numPrbc   = pPrachCPConfig->numPrbc,
    sect_geninfo[nsection].info.numSymbol = pPrachCPConfig->numSymbol*pPrachCPConfig->occassionsInPrachSlot;
    sect_geninfo[nsection].info.reMask    = 0xfff;
    sect_geninfo[nsection].info.beamId    = beam_id;
    sect_geninfo[nsection].info.freqOffset= pPrachCPConfig->freqOffset;

    sect_geninfo[nsection].info.ef        = 0;      // no extension
    sect_geninfo[nsection].exDataSize     = 0;
    sect_geninfo[nsection].exData         = NULL;
    nsection++;

    params.numSections          = nsection;
    params.sections             = sect_geninfo;

    mbuf = xran_ethdi_mbuf_alloc();
    if(unlikely(mbuf == NULL)) {
        print_err("Alloc fail!\n");
        return (-1);
        }

    ret = xran_prepare_ctrl_pkt(mbuf, &params, cc_id, prach_port_id, seq_id);
    if(ret < 0) {
        print_err("Fail to build prach control packet - [%d:%d:%d]\n", frame_id, subframe_id, slot_id);
        return (ret);
        }
    else {
        xran_ethdi_mbuf_send_cp(mbuf, ETHER_TYPE_ECPRI);
        for(i=0; i < nsection; i++)
            xran_cp_add_section_info(pHandle,
                    XRAN_DIR_UL, cc_id, prach_port_id, subframe_id, slot_id,
                    &sect_geninfo[i].info);
        }

    return (ret);
}


int process_ring(struct rte_ring *r)
{
    assert(r);

    struct rte_mbuf *mbufs[MBUFS_CNT];
    int i;
    uint32_t remaining;
    const uint16_t dequeued = rte_ring_dequeue_burst(r, (void **)mbufs,
        RTE_DIM(mbufs), &remaining);

    if (!dequeued)
        return 0;
    for (i = 0; i < dequeued; ++i) {
        if (xran_ethdi_filter_packet(mbufs[i], 0) == MBUF_FREE)
            rte_pktmbuf_free(mbufs[i]);
    }

    return remaining;
}

int ring_processing_thread(void *args)
{
    struct timespec tv = {0};
    int64_t prev_nsec = 0;
    uint8_t is_timer_set = 0;
    struct xran_ethdi_ctx *const ctx = xran_ethdi_get_ctx();
    struct sched_param sched_param;
    int res = 0;

    printf("%s [CPU %2d] [PID: %6d]\n", __FUNCTION__,  rte_lcore_id(), getpid());
    sched_param.sched_priority = XRAN_THREAD_DEFAULT_PRIO;
    if ((res = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched_param)))
    {
        printf("priority is not changed: coreId = %d, result1 = %d\n",rte_lcore_id(), res);
    }
    for (;;) {
        if (!is_timer_set) {
            if (clock_gettime(CLOCK_REALTIME, &tv) != 0)
                err(1, "gettimeofday() failed");
            if (tv.tv_nsec % 125000 < prev_nsec % 125000) { /* crossed an 125ms boundary */
                rte_timer_manage();     /* timers only run on IO core */
                is_timer_set = 1;
            }
            prev_nsec = tv.tv_nsec;
        } else {
            rte_timer_manage();
        }

        /* UP first */
        if (process_ring(ctx->rx_ring[ETHDI_UP_VF]))
            continue;
        /* CP next */
        if (process_ring(ctx->rx_ring[ETHDI_CP_VF]))
            continue;

        if (XRAN_STOPPED == xran_if_current_state)
            break;
    }

    puts("Pkt processing thread finished.");
    return 0;
}

#endif /* _XRAN_COMMON_ */
