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
 * @brief This file provides public interface to xRAN Front Haul layer implementation as defined in the
 *      ORAN-WG4.CUS.0-v01.00 spec. Implementation specific to
 *      (O-DU): a logical node that includes the eNB/gNB functions as
 *      listed in section 2.1 split option 7-2x.
 *  
 *
 * @file xran_fh_o_ru.h
 * @ingroup group_lte_source_xran
 * @author Intel Corporation
 *
 **/

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
#include "xran_fh_o_du.h"

/**
 * @ingroup
 *
 *   Function configures TX and RX output buffers
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param pSrcRxCpBuffer
 *   list of memory buffers to use to deliver BFWs from XRAN layer to the application for Validation
 * @param pSrcTxCpBuffer
 *   list of memory buffers to use to deliver BFWs from XRAN layer to the application for Validation
 * @param xran_transport_callback_fn pCallback
 *   Callback function to call with arrival of C-Plane packets for given CC
 * @param pCallbackTag
 *   Parameters of Callback function
 * 
 * @return
 *   0  - on success
 *   -1 - on error
 */

int32_t xran_5g_bfw_config(void * pHandle, struct xran_buffer_list *pSrcRxCpBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                    struct xran_buffer_list *pSrcTxCpBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                    xran_transport_callback_fn pCallback,
                    void *pCallbackTag);


#ifdef __cplusplus
}
#endif