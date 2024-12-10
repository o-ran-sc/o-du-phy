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

#ifndef _XRAN_APP_COMMON_H_
#define _XRAN_APP_COMMON_H_

#include <stdio.h>
#include <unistd.h>

#include "xran_fh_o_du.h"
#include "xran_pkt_up.h"

#include <rte_common.h>
#include <rte_mbuf.h>

#define VERSIONX                "#DIRTY#"

#define APP_O_DU  0
#define APP_O_RU  1

enum app_state
{
    APP_RUNNING,
    APP_STOPPED
};

enum nRChBwOptions
{
    PHY_BW_1_4_0_MHZ = 1, PHY_BW_3_0_MHZ = 3, PHY_BW_5_0_MHZ = 5,   PHY_BW_10_0_MHZ = 10, PHY_BW_15_0_MHZ = 15, PHY_BW_20_0_MHZ = 20, PHY_BW_25_0_MHZ = 25,
    PHY_BW_30_0_MHZ = 30, PHY_BW_40_0_MHZ = 40, PHY_BW_50_0_MHZ = 50, PHY_BW_60_0_MHZ = 60, PHY_BW_70_0_MHZ = 70,
    PHY_BW_80_0_MHZ = 80, PHY_BW_90_0_MHZ = 90, PHY_BW_100_0_MHZ = 100, PHY_BW_200_0_MHZ = 200, PHY_BW_400_0_MHZ = 400
};

#define N_SC_PER_PRB(isNb375) (isNb375 ? 48 : 12)
#define N_SYM_PER_SLOT 14
#define MAX_ANT_CARRIER_SUPPORTED (XRAN_MAX_SECTOR_NR*XRAN_MAX_ANTENNA_NR)
#define MAX_ANT_CARRIER_SUPPORTED_CAT_B (XRAN_MAX_SECTOR_NR*XRAN_MAX_ANT_ARRAY_ELM_NR)
#define MAX_CSIRS_PORTS_SUPPORTED (XRAN_MAX_SECTOR_NR*XRAN_MAX_CSIRS_PORTS)

#define SUBFRAMES_PER_SYSTEMFRAME  10
#define IQ_PLAYBACK_BUFFER_BYTES(isNb375) (XRAN_NUM_OF_SLOT_IN_TDD_LOOP*N_SYM_PER_SLOT*XRAN_MAX_PRBS*N_SC_PER_PRB(isNb375)*4L)
/* PRACH data samples are 32 bits wide, 16bits for I and 16bits for Q. Each packet contains 840 samples for long sequence or 144 for short sequence. The payload length is 840*16*2/8 octets.*/
#define PRACH_PLAYBACK_BUFFER_BYTES (840*4L)

#ifdef _DEBUG
#define iAssert(p) if(!(p)){fprintf(stderr,\
    "Assertion failed: %s, file %s, line %d, val %d\n",\
    #p, __FILE__, __LINE__, p);exit(-1);}
#else /* _DEBUG */
#define iAssert(p)
#endif /* _DEBUG */


typedef struct
{
    int16_t *p_tx_play_buffer[MAX_ANT_CARRIER_SUPPORTED];
    int32_t tx_play_buffer_size[MAX_ANT_CARRIER_SUPPORTED];

    int16_t *p_tx_prach_play_buffer[MAX_ANT_CARRIER_SUPPORTED];
    int32_t tx_prach_play_buffer_size[MAX_ANT_CARRIER_SUPPORTED];
    int32_t tx_prach_play_buffer_position[MAX_ANT_CARRIER_SUPPORTED];

    int16_t *p_tx_srs_play_buffer[MAX_ANT_CARRIER_SUPPORTED_CAT_B];
    int32_t tx_srs_play_buffer_size[MAX_ANT_CARRIER_SUPPORTED_CAT_B];
    int32_t tx_srs_play_buffer_position[MAX_ANT_CARRIER_SUPPORTED_CAT_B];

    int16_t *p_tx_csirs_play_buffer[MAX_CSIRS_PORTS_SUPPORTED];
    int32_t tx_csirs_play_buffer_size[MAX_CSIRS_PORTS_SUPPORTED];

    int16_t *p_rx_log_buffer[MAX_ANT_CARRIER_SUPPORTED];
    int32_t rx_log_buffer_size[MAX_ANT_CARRIER_SUPPORTED];

    int16_t *p_prach_log_buffer[MAX_ANT_CARRIER_SUPPORTED];
    int32_t prach_log_buffer_size[MAX_ANT_CARRIER_SUPPORTED];

    int16_t *p_srs_log_buffer[MAX_ANT_CARRIER_SUPPORTED_CAT_B];
    int32_t srs_log_buffer_size[MAX_ANT_CARRIER_SUPPORTED_CAT_B];

    int16_t *p_csirs_log_buffer[MAX_ANT_CARRIER_SUPPORTED_CAT_B];
    int32_t csirs_log_buffer_size[MAX_ANT_CARRIER_SUPPORTED_CAT_B];

    int16_t *p_tx_buffer[MAX_ANT_CARRIER_SUPPORTED];
    int32_t tx_buffer_size[MAX_ANT_CARRIER_SUPPORTED];
    int16_t *p_rx_buffer[MAX_ANT_CARRIER_SUPPORTED];
    int32_t rx_buffer_size[MAX_ANT_CARRIER_SUPPORTED];

    /* beamforming weights for UL (O-DU) */
    int16_t *p_tx_dl_bfw_buffer[MAX_ANT_CARRIER_SUPPORTED];
    int32_t tx_dl_bfw_buffer_size[MAX_ANT_CARRIER_SUPPORTED];

    int16_t *p_tx_dl_bfw_log_buffer[MAX_ANT_CARRIER_SUPPORTED];
    int32_t tx_dl_bfw_log_buffer_size[MAX_ANT_CARRIER_SUPPORTED];

    /* beamforming weights for UL (O-DU) */
    int16_t *p_tx_ul_bfw_buffer[MAX_ANT_CARRIER_SUPPORTED];
    int32_t tx_ul_bfw_buffer_size[MAX_ANT_CARRIER_SUPPORTED];

    int16_t *p_tx_ul_bfw_log_buffer[MAX_ANT_CARRIER_SUPPORTED];
    int32_t tx_ul_bfw_log_buffer_size[MAX_ANT_CARRIER_SUPPORTED];

    /* beamforming weights for UL (O-RU) */
    int16_t *p_rx_dl_bfw_buffer[MAX_ANT_CARRIER_SUPPORTED];
    int32_t rx_dl_bfw_buffer_size[MAX_ANT_CARRIER_SUPPORTED];
    int32_t rx_dl_bfw_buffer_position[MAX_ANT_CARRIER_SUPPORTED];

    /* beamforming weights for UL (O-RU) */
    int16_t *p_rx_ul_bfw_buffer[MAX_ANT_CARRIER_SUPPORTED];
    int32_t rx_ul_bfw_buffer_size[MAX_ANT_CARRIER_SUPPORTED];
    int32_t rx_ul_bfw_buffer_position[MAX_ANT_CARRIER_SUPPORTED];

} buffers_per_mu;

/**< all the buffers allocated for O-XU */
struct o_xu_buffers {
    int iq_playback_buffer_size_dl;
    int iq_playback_buffer_size_ul;

    int iq_bfw_buffer_size_dl;
    int iq_bfw_buffer_size_ul;

    int iq_srs_buffer_size_ul;
    int iq_csirs_buffer_size_dl;

    int iq_dl_bit;
    int iq_ul_bit;
    int iq_bfw_dl_bit;
    int iq_bfw_ul_bit;
    int iq_srs_bit;
    int iq_csirs_bit;
    int iq_prach_bit;

    int numSlots;  /**< number of slots in IQ vector */
    buffers_per_mu buff_perMu[XRAN_MAX_NUM_MU];
};

extern struct o_xu_buffers* p_o_xu_buff[XRAN_PORTS_NUM];

void sys_save_buf_to_file_txt(char *filename, char *bufname, unsigned char *pBuffer, unsigned int size, unsigned int buffers_num, unsigned int bitwidth);
void sys_save_buf_to_file(char *filename, char *bufname, unsigned char *pBuffer, unsigned int size, unsigned int buffers_num);
int  sys_load_file_to_buff(char *filename, char *bufname, unsigned char *pBuffer, unsigned int size, unsigned int buffers_num);

uint32_t app_xran_get_scs(uint8_t nMu);
uint16_t app_xran_get_num_rbs(uint8_t ranTech, uint32_t nNumerology, uint32_t nBandwidth, uint32_t nAbsFrePointA);
uint32_t app_xran_cal_nrarfcn(uint32_t nCenterFreq);
int32_t  app_xran_set_slot_type(uint32_t nPhyInstanceId, uint32_t nFrameDuplexType,
                uint32_t nTddPeriod, struct xran_slot_config *psSlotConfig);
uint32_t app_xran_get_tti_interval(uint8_t nMu);
void app_print_xran_antenna_stats(uint8_t app_mode, int o_xu_id, struct xran_common_counters *x_counters);
#endif /*_XRAN_APP_COMMON_H_*/
