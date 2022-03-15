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
 * @brief This file has Shared Memory interface functions between MAC and PHY
 * @file testmac_wls.h
 * @ingroup group_testmac
 * @author Intel Corporation
 **/

#ifndef _TESTMAC_WLS_H_
#define _TESTMAC_WLS_H_

#ifdef __cplusplus
extern "C" {
#endif

//#include "common_typedef.h"
#include "fapi_interface.h"
#include "fapi_vendor_extension.h"

#define SUCCESS 0
#define FAILURE 1

#define MAX_NUM_LOCATIONS           (50)

#define MIN_DL_BUF_LOCATIONS        (0)                                             /* Used for stats collection 0-49 */
#define MIN_UL_BUF_LOCATIONS        (MIN_DL_BUF_LOCATIONS + MAX_NUM_LOCATIONS)      /* Used for stats collection 50-99 */

#define MAX_DL_BUF_LOCATIONS        (MIN_DL_BUF_LOCATIONS + MAX_NUM_LOCATIONS)          /* Used for stats collection 0-49 */
#define MAX_UL_BUF_LOCATIONS        (MIN_UL_BUF_LOCATIONS + MAX_NUM_LOCATIONS)          /* Used for stats collection 50-99 */

#define WLS_MSG_BLOCK_SIZE          ( 16384 * 16 )

typedef struct tagZBC_LIST_ITEM
{
    uint64_t pMsg;
    uint32_t MsgSize;
} ZBC_LIST_ITEM, *PZBC_LIST_ITEM;


uint32_t wls_mac_init(char * wls_device_name, uint64_t nBlockSize);
void wls_mac_print_thread_info(void);
uint32_t wls_mac_destroy(void);
void *wls_mac_alloc_buffer(uint32_t size, uint32_t loc);
uint32_t wls_mac_send_msg_to_phy(void *data);
uint64_t wls_mac_va_to_pa(void *ptr);
void *wls_mac_pa_to_va(uint64_t ptr);
void wls_mac_free_buffer(void *pMsg, uint32_t loc);
void wls_mac_get_time_stats(uint64_t *pTotal, uint64_t *pUsed, uint32_t nClear);
void wls_mac_free_list_all(void);
int wls_mac_free_list(uint32_t idx);
p_fapi_api_queue_elem_t wls_mac_create_elem(uint16_t num_msg, uint32_t align_offset, uint32_t msg_type, uint32_t n_loc);
void wls_mac_print_recv_list(p_fapi_api_queue_elem_t list, uint32_t i);
uint8_t mac_dpdk_init();
void *wls_mac_rx_task();

#ifdef __cplusplus
}
#endif


#endif /* #ifndef _TESTMAC_WLS_H_ */


