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
 * @brief Header file for functions to perform application level fragmentation
 *
 * @file xran_app_frag.h
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#ifndef _XRAN_APP_FRAG_
#define _XRAN_APP_FRAG_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

#include <rte_config.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_byteorder.h>

#include "xran_fh_o_du.h"
#include "xran_cp_api.h"

int32_t xran_app_fragment_packet(struct rte_mbuf *pkt_in, /* eth hdr is prepended */
                                    struct rte_mbuf **pkts_out,
                                    uint16_t nb_pkts_out,
                                    uint16_t mtu_size,
                                    struct rte_mempool *pool_direct,
                                    struct rte_mempool *pool_indirect,
                                    struct xran_section_info *sectinfo,
                                    uint8_t *seqid);

#ifdef __cplusplus
}
#endif

#endif /* _XRAN_APP_FRAG_ */

