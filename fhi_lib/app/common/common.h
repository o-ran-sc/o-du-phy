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

#include <stdio.h>
#include <unistd.h>

#include "xran_pkt_up.h"

#include <rte_common.h>
#include <rte_mbuf.h>

#define APP_LLS_CU 0
#define APP_RU     1

enum app_state
{
    APP_RUNNING,
    APP_STOPPED
};

#define NUM_OF_PRB_IN_FULL_BAND (66)
#define N_SC_PER_PRB 12
#define N_SYM_PER_SLOT 14
#define N_FULLBAND_SC (NUM_OF_PRB_IN_FULL_BAND*N_SC_PER_PRB)
#define MAX_ANT_CARRIER_SUPPORTED 16
// 0.125, just for testing
#define SLOTNUM_PER_SUBFRAME      8
#define SUBFRAMES_PER_SYSTEMFRAME  10
#define PDSCH_PAYLOAD_SIZE (N_FULLBAND_SC*4)
#define NUM_OF_SLOT_IN_TDD_LOOP         (80)
#define IQ_PLAYBACK_BUFFER_BYTES (NUM_OF_SLOT_IN_TDD_LOOP*N_SYM_PER_SLOT*N_FULLBAND_SC*4L)
/* PRACH data samples are 32 bits wide, 16bits for I and 16bits for Q. Each packet contains 839 samples. The payload length is 3356 octets.*/
#define PRACH_PLAYBACK_BUFFER_BYTES (10*839*4L)

#ifdef _DEBUG
#define iAssert(p) if(!(p)){fprintf(stderr,\
    "Assertion failed: %s, file %s, line %d, val %d\n",\
    #p, __FILE__, __LINE__, p);exit(-1);}
#else /* _DEBUG */
#define iAssert(p)
#endif /* _DEBUG */

struct send_symbol_cb_args
{
    struct rb_map *samp_buf;
    uint8_t *symb_id;
};

struct pkt_dump
{
    int num_samp;
    int num_bytes;
    uint8_t symb;
    struct ecpri_seq_id seq;
} __rte_packed;

extern uint8_t numCCPorts;
/* Number of antennas supported by front-end */

extern uint8_t num_eAxc;
/* Number of antennas supported by front-end */
extern int16_t *p_tx_play_buffer[MAX_ANT_CARRIER_SUPPORTED];
extern int32_t tx_play_buffer_size[MAX_ANT_CARRIER_SUPPORTED];
extern int32_t tx_play_buffer_position[MAX_ANT_CARRIER_SUPPORTED];

/* Number of antennas supported by front-end */
extern int16_t *p_rx_log_buffer[MAX_ANT_CARRIER_SUPPORTED];
extern int32_t rx_log_buffer_size[MAX_ANT_CARRIER_SUPPORTED];
extern int32_t rx_log_buffer_position[MAX_ANT_CARRIER_SUPPORTED];

extern int16_t *p_prach_log_buffer[MAX_ANT_CARRIER_SUPPORTED];
extern int32_t prach_log_buffer_size[MAX_ANT_CARRIER_SUPPORTED];
extern int32_t prach_log_buffer_position[MAX_ANT_CARRIER_SUPPORTED];

extern int16_t *p_tx_buffer[MAX_ANT_CARRIER_SUPPORTED];
extern int32_t tx_buffer_size[MAX_ANT_CARRIER_SUPPORTED];

extern int16_t *p_rx_buffer[MAX_ANT_CARRIER_SUPPORTED];
extern int32_t rx_buffer_size[MAX_ANT_CARRIER_SUPPORTED];

void sys_save_buf_to_file_txt(char *filename, char *bufname, unsigned char *pBuffer, unsigned int size, unsigned int buffers_num);
void sys_save_buf_to_file(char *filename, char *bufname, unsigned char *pBuffer, unsigned int size, unsigned int buffers_num);
int  sys_load_file_to_buff(char *filename, char *bufname, unsigned char *pBuffer, unsigned int size, unsigned int buffers_num);

