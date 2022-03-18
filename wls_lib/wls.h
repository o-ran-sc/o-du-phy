/******************************************************************************
*
*   Copyright (c) 2021 Intel.
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

#ifndef __WLS_H__
#define __WLS_H__

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/sched.h>
#define MODNAME (KBUILD_MODNAME)
#else /* __KERNEL__ */
#include <sys/ioctl.h>
#include <stdint.h>
#include <semaphore.h>
#include <rte_common.h>
#include <rte_atomic.h>
#include <rte_memzone.h>

#endif
#include "ttypes.h"
#include "syslib.h"

#define WLS_PRINT(format, args...) printk(format, ##args)
#define WLS_ERROR(format, args...) printk(KERN_ERR "wls err: " format,##args)

#ifdef _DEBUG_
#define WLS_DEBUG(format, args...)            \
do                                                  \
{                                                   \
    printk(KERN_INFO "wls debug: " format,##args); \
}while(0)
#else /*_DEBUG_*/
#define WLS_DEBUG(format, args...) do { } while(0)
#endif /*_DEBUG_*/

/******************************************************************************
*                        Module error codes                                   *
******************************************************************************/
#define WLS_RC_MDMA_ID_ERROR                (-1)
#define WLS_RC_MDMA_TASK_ERROR              (-2)
#define WLS_RC_ALLOC_DELAY_MEM_ERROR        (-3)
#define WLS_RC_ALLOC_BAR_MEM_ERROR          (-4)
#define WLS_RC_ALLOC_TAR_MEM_ERROR          (-5)
#define WLS_RC_PARAM_SIZE_ERROR             (-6)
#define WLS_RC_WLS_HEAP_ALLOC_ERROR         (-7)
#define WLS_RC_IRQ_ALLOC_ERROR              (-8)
#define WLS_RC_DMA_ALLOC_ERROR              (-9)
#define WLS_RC_TRANSACTION_ERROR            (-10)
#define WLS_RC_PHY_CTX_ERROR                (-11)
#define WLS_RC_KERNEL_HEAP_ALLOC_ERROR      (-12)
#define WLS_RC_CONFIGURATION_ERROR          (-13)
#define WLS_RC_THREAD_CREATION_ERROR        (-14)

#define WLS_IOC_MAGIC 'W'
#define WLS_IOC_OPEN         _IOWR(WLS_IOC_MAGIC, WLS_IOC_OPEN_NO,         uint64_t)
#define WLS_IOC_CLOSE        _IOWR(WLS_IOC_MAGIC, WLS_IOC_CLOSE_NO,        uint64_t)
#define WLS_IOC_PUT          _IOWR(WLS_IOC_MAGIC, WLS_IOC_PUT_NO,          uint64_t)
#define WLS_IOC_EVENT        _IOWR(WLS_IOC_MAGIC, WLS_IOC_EVENT_NO,        uint64_t)
#define WLS_IOC_WAIT         _IOWR(WLS_IOC_MAGIC, WLS_IOC_WAIT_NO,         uint64_t)
#define WLS_IOC_WAKE_UP      _IOWR(WLS_IOC_MAGIC, WLS_IOC_WAKE_UP_NO,      uint64_t)
#define WLS_IOC_CONNECT      _IOWR(WLS_IOC_MAGIC, WLS_IOC_CONNECT_NO,      uint64_t)
#define WLS_IOC_FILL         _IOWR(WLS_IOC_MAGIC, WLS_IOC_FILL_NO,         uint64_t)

enum {
    WLS_IOC_OPEN_NO = 1,
    WLS_IOC_CLOSE_NO,
    WLS_IOC_PUT_NO,
    WLS_IOC_EVENT_NO,
    WLS_IOC_WAIT_NO,
    WLS_IOC_WAKE_UP_NO,
    WLS_IOC_CONNECT_NO,
    WLS_IOC_FILL_NO,
    WLS_IOC_COUNT,
};

enum {
    WLS_FILL_PUSH = 1,
    WLS_FILL_PULL,
    WLS_FILL_MAX,
};


#define WLS_RUP512B(x) (((x)+511)&(~511))
#define WLS_RUP256B(x) (((x)+255)&(~255))
#define WLS_RUP128B(x) (((x)+127)&(~127))
#define WLS_RUP64B(x) (((x)+63)&(~63))
#define WLS_RUP32B(x) (((x)+31)&(~31))
#define WLS_RUP16B(x) (((x)+15)&(~15))
#define WLS_RUP8B(x)  (((x)+7)&(~7))
#define WLS_RUP4B(x)  (((x)+3)&(~3))
#define WLS_RUP2B(x)  (((x)+1)&(~1))

#define WLS_US_CLIENTS_MAX 4

#define CACHE_LINE_SIZE 64                  /**< Cache line size. */
#define CACHE_LINE_MASK (CACHE_LINE_SIZE-1) /**< Cache line mask. */

#define CACHE_LINE_ROUNDUP(size) \
    (CACHE_LINE_SIZE * ((size + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE))

#define DMA_ALIGNMENT_SIZE     256L

// To make DMA we make sure that block starts on 256 bytes boundary
#define DMA_ALIGNMENT_ROUNDUP(size) \
    (DMA_ALIGNMENT_SIZE * ((size + DMA_ALIGNMENT_SIZE - 1) / DMA_ALIGNMENT_SIZE))

/**< Return the first cache-aligned value greater or equal to size. */

/**
 * Force alignment to cache line.
 */
#define __wls_cache_aligned __attribute__((__aligned__(CACHE_LINE_SIZE)))

#define WLS_HUGE_DEF_PAGE_SIZE                0x40000000LL
#define WLS_IS_ONE_HUGE_PAGE(ptr, size, hp_size)  ((((unsigned long long)ptr & (~(hp_size - 1)))\
        == (((unsigned long long)ptr + size - 1) & (~(hp_size - 1)))) ? 1 : 0)

typedef struct hugepage_tabl_s
{
    uint64_t pageVa;
    uint64_t pagePa;
}hugepage_tabl_t;

#define DMA_MAP_MAX_BLOCK_SIZE 64*1024
#define MAX_N_HUGE_PAGES            32
#define UL_FREE_BLOCK_QUEUE_SIZE    1200

#define WLS_GET_QUEUE_N_ELEMENTS    1024
#define WLS_PUT_QUEUE_N_ELEMENTS    1024

#define WLS_DEV_SHM_NAME_LEN      RTE_MEMZONE_NAMESIZE

#define FIFO_LEN 1024

typedef struct wls_wait_req_s {
    uint64_t wls_us_kernel_va;
    uint64_t start_time;
    uint64_t ctx;
    uint64_t action;
    uint64_t nMsg;
}wls_wait_req_t;

typedef struct wls_sema_priv_s
{
    sem_t                     sem;
    rte_atomic16_t            is_irq;
    wls_wait_req_t            drv_block[FIFO_LEN];
    volatile unsigned int     drv_block_put;
    volatile unsigned int     drv_block_get;
} wls_sema_priv_t;

typedef struct wls_us_priv_s
{
    wls_sema_priv_t   sema;
    U8                NeedToWakeUp;
    U8                isWait;
    volatile V32      pid;
} wls_us_priv_t;

typedef struct wls_us_ctx_s
{
    union {
       void *      wls_us_user_space_va;
       uint64_t    padding_wls_us_user_space_va;
    };

    uint64_t       wls_us_kernel_va;

    uint64_t       wls_us_pa;

    uint32_t       wls_us_ctx_size;
    uint32_t       HugePageSize;

    union {
        void*      alloc_buffer;
        uint64_t   padding_alloc_buffer;
    };

    hugepage_tabl_t    hugepageTbl [MAX_N_HUGE_PAGES];

    FASTQUEUE          ul_free_block_pq;
    uint64_t           ul_free_block_storage[UL_FREE_BLOCK_QUEUE_SIZE * sizeof(uint64_t)];

    WLS_MSG_QUEUE  get_queue;
    WLS_MSG_HANDLE get_storage[WLS_GET_QUEUE_N_ELEMENTS];

    WLS_MSG_QUEUE  put_queue;
    WLS_MSG_HANDLE put_storage[WLS_PUT_QUEUE_N_ELEMENTS];

    uint64_t           freePtrList[UL_FREE_BLOCK_QUEUE_SIZE * sizeof(uint64_t)];
    uint32_t	       freeListIndex;
    uint32_t         dualMode;

    // dst userspace context address (kernel va)
    uint64_t    dst_kernel_va;
    // dst userspace context address (local user sapce va)

    volatile uint64_t    dst_user_va;
    // dst userspace context address (local user sapce va)
    volatile uint64_t    dst_pa;

    uint32_t nHugePage;
    wls_us_priv_t wls_us_private;
    uint32_t  mode;
    uint32_t  secmode;
    char wls_dev_name[WLS_DEV_SHM_NAME_LEN];
    char wls_shm_name[WLS_DEV_SHM_NAME_LEN];
}wls_us_ctx_t;



typedef struct wls_fill_req_s {
    uint64_t wls_us_kernel_va;
    uint64_t ctx;
    uint64_t action;
    uint64_t nMsg;
}wls_fill_req_t;

typedef struct wls_connect_req_s {
    uint64_t wls_us_kernel_va;
}wls_connect_req_t;

typedef struct wls_drv_ctx_s
{
    uint32_t            init_mask;
    uint32_t            us_ctx_cout;
    wls_us_ctx_t        p_wls_us_ctx[WLS_US_CLIENTS_MAX];
    wls_us_ctx_t        p_wls_us_pa_ctx[WLS_US_CLIENTS_MAX];
    uint32_t            nWlsClients;
    uint32_t            nMacBufferSize;
    uint32_t            nPhyBufferSize;
    pthread_mutex_t mng_mutex;
}wls_drv_ctx_t;

typedef struct wls_open_req_s {
    uint64_t  ctx;
    uint64_t  ctx_pa;
    uint32_t  size;
}wls_open_req_t;

typedef struct wls_close_req_s {
    uint64_t  ctx;
    uint64_t  ctx_pa;
    uint32_t  size;
}wls_close_req_t;

typedef enum wls_events_num_s {
    WLS_EVENT_IA_READY = 0,
    WLS_EVENT_IA_STOP,
    WLS_EVENT_IA_ERROR,
    WLS_EVENT_MAX
}wls_events_num_t;

typedef struct wls_event_req_s {
    uint64_t wls_us_kernel_va;
    uint64_t event_to_wls;
    uint64_t event_param;
}wls_event_req_t;

typedef struct wls_put_req_s {
    uint64_t wls_us_kernel_va;
}wls_put_req_t;

typedef struct wls_wake_up_req_s {
    uint64_t wls_us_kernel_va;
    uint32_t id;
    uint64_t ctx;
}wls_wake_up_req_t;


static inline uint64_t wls_rdtsc(void)
{
    union {
        uint64_t tsc_64;
        struct {
            uint32_t lo_32;
            uint32_t hi_32;
        };
    } tsc;

    asm volatile("rdtsc" :
             "=a" (tsc.lo_32),
             "=d" (tsc.hi_32));
    return tsc.tsc_64;
}

#endif /* __WLS_H__*/

