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
 *
 * @file This file has definitions of Shared Memory interface functions between
 * FAPI and PHY.
 *
 **/

#ifndef _NR5G_FAPI_WLS_H_
#define _NR5G_FAPI_WLS_H_

#include "nr5g_fapi_std.h"
#include "nr5g_fapi_common_types.h"
#include "wls_lib.h"
#include "gnb_l1_l2_api.h"

typedef void *WLS_HANDLE;

#define NUM_WLS_INSTANCES 2
#define NR5G_FAPI2PHY_WLS_INST  0
#define NR5G_FAPI2MAC_WLS_INST  1

#define MAX_NUM_LOCATIONS          (16)

#define MIN_DL_BUF_LOCATIONS        (0) /* Used for stats collection 0-10 */
#define MAX_DL_BUF_LOCATIONS        (MIN_DL_BUF_LOCATIONS + MAX_NUM_LOCATIONS)  /* Used for stats collection 0-10 */
#define MIN_UL_BUF_LOCATIONS        (MAX_DL_BUF_LOCATIONS)  /* Used for stats collection 0-10 */
#define MAX_UL_BUF_LOCATIONS        (MIN_UL_BUF_LOCATIONS + MAX_NUM_LOCATIONS)

#define TO_FREE_SIZE                        ( 5 )
#define TO_FREE_SIZE_URLLC                  ( MAX_NUM_OF_SYMBOL_PER_SLOT * TO_FREE_SIZE ) // TR 38.912 8.1 mini-slot may be 1 symbol long
#define TOTAL_FREE_BLOCKS                   ( 100 * FAPI_MAX_PHY_INSTANCES)  /* To hold both send and recv blocks on PHY side wls */
#define ALLOC_TRACK_SIZE                    ( 16384 )
#define MSG_MAXSIZE                         (16*16384 )

#define MEMORY_CORRUPTION_DETECT
#define MEMORY_CORRUPTION_DETECT_FLAG       (0xAB)

typedef enum wls_fapi_free_list_e {
    WLS_FAPI_FREE_SEND_LIST = 0,
    WLS_FAPI_FREE_RECV_LIST
} wls_fapi_free_list_t;

typedef struct wls_fapi_mem_array {
    void **ppFreeBlock;
    void *pStorage;
    void *pEndOfStorage;
    uint32_t nBlockSize;
    uint32_t nBlockCount;
} WLS_FAPI_MEM_STRUCT,
*PWLS_FAPI_MEM_STRUCT;

// WLS context structure
typedef struct _nr5g_fapi_wls_context {
    void *shmem;                // shared  memory region.
    uint64_t shmem_size;        // shared  memory region size.
    WLS_FAPI_MEM_STRUCT sWlsStruct;
    WLS_HANDLE h_wls[NUM_WLS_INSTANCES];    // WLS context handle
    void *pWlsMemBase;
    uint32_t nTotalMemorySize;
    uint32_t nTotalBlocks;
    uint32_t nAllocBlocks;
    uint32_t nTotalAllocCnt;
    uint32_t nTotalFreeCnt;
    uint32_t nTotalUlBufAllocCnt;
    uint32_t nTotalUlBufFreeCnt;
    uint32_t nTotalDlBufAllocCnt[MAX_DL_BUF_LOCATIONS];
    uint32_t nTotalDlBufFreeCnt[MAX_DL_BUF_LOCATIONS];
    uint32_t nPartitionMemSize;
    void *pPartitionMemBase;
    volatile pthread_mutex_t fapi2phy_lock_send;
    volatile pthread_mutex_t fapi2phy_lock_alloc;
    volatile pthread_mutex_t fapi2mac_lock_send;
    volatile pthread_mutex_t fapi2mac_lock_alloc;
} nr5g_fapi_wls_context_t,
*p_nr5g_fapi_wls_context_t;

extern nr5g_fapi_wls_context_t g_wls_ctx;

inline p_nr5g_fapi_wls_context_t nr5g_fapi_wls_context(
    );
inline uint8_t nr5g_fapi_fapi2phy_wls_ready(
    );
inline uint8_t nr5g_fapi_fapi2mac_wls_ready(
    );
uint8_t nr5g_fapi_wls_init(
    );
uint8_t nr5g_fapi_wls_memory_init(
    );
uint8_t nr5g_fapi_wls_destroy(
    WLS_HANDLE h_wls);
void *wls_fapi_alloc_buffer(
    uint32_t size,
    uint32_t loc);
void wls_fapi_free_buffer(
    void *pMsg,
    uint32_t loc);
uint32_t wls_fapi_create_mem_array(
    PWLS_FAPI_MEM_STRUCT pMemArray,
    void *pMemArrayMemory,
    uint32_t totalSize,
    uint32_t nBlockSize);
uint32_t wls_fapi_alloc_mem_array(
    PWLS_FAPI_MEM_STRUCT pMemArray,
    void **ppBlock);
uint32_t wls_fapi_free_mem_array(
    PWLS_FAPI_MEM_STRUCT pMemArray,
    void *pBlock);
uint64_t nr5g_fapi_wls_va_to_pa(
    WLS_HANDLE h_wls,
    void *ptr);
void *nr5g_fapi_wls_pa_to_va(
    WLS_HANDLE h_wls,
    uint64_t ptr);
uint32_t wls_fapi_add_blocks_to_ul(
    void);
void nr5g_fapi_wls_show_data(
    void *ptr,
    uint32_t size);
void wls_fapi_free_list_all(
    void);
void nr5g_fapi_wls_print_stats(
    void);

#endif /*_NR5G_FAPI_WLS_H_*/
