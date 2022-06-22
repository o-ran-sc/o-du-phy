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
// Ecpri One-way delay measurement support definitions

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>

enum xran_owdm_state
{
    XRAN_OWDM_IDLE = 0,
    XRAN_OWDM_WAITRESP,
    XRAN_OWDM_WAITREQ,
    XRAN_OWDM_WAITFUP,
    XRAN_OWDM_GENFUP,
    XRAN_OWDM_WAITREQWFUP,
    XRAN_OWDM_WAITREMREQ,
    XRAN_OWDM_WAITREMREQWFUP,
    XRAN_OWDM_DONE  
};

enum xran_owd_meas_method
{
    XRAN_REQUEST = 0,
    XRAN_REM_REQ,
    XRAN_REQ_WFUP,
    XRAN_REM_REQ_WFUP
};

enum xran_owdm_tx_state
{
    OWDMTX_INIT = 0,
    OWDMTX_IDLE,
    OWDMTX_ACTIVE,
    OWDTX_DONE
};

#define DELAY_THRESHOLD  60000 /* in ns */
#define ADJUSTMENT 60 /* in us */
#define MIN_OWDM_PL_LENGTH 40 /* Minimum owdm Payload length in bytes */
#define MAX_OWDM_PL_LENGTH 1400 /* Maximum owdm Payload length in bytes */

int xran_get_delay_measurements_results (void* Handle,  uint16_t port_id, uint8_t id, uint64_t* pdelay_avg);

void xran_adjust_timing_parameters(void* Handle);

void xran_initialize_and_verify_owd_pl_length(void* Handle);

int process_delay_meas(struct rte_mbuf *pkt,  void* handle, uint16_t port_id);