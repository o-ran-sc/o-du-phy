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
 * @file This file has Shared Memory interface functions between FAPI and MAC 
 *
 **/

#include "nr5g_fapi_std.h"
#include "nr5g_fapi_common_types.h"
#include "nr5g_fapi_wls.h"
#include "gnb_l1_l2_api.h"
#include "nr5g_fapi_fapi2mac_wls.h"
#include "nr5g_fapi_log.h"
#include "nr5g_fapi_urllc_thread.h"

static p_fapi_api_queue_elem_t p_fapi2mac_buffers;

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_fapi2mac_group
 *
 *  @param data Pointer to validate
 *
 *  @return  TRUE If pointer is within valid shared WLS memory region
 *           FALSE If pointer is out of valid shared WLS memory region
 *
 *  @description
 *  This function validates pointer's in WLS shared memory region
 *
**/
//------------------------------------------------------------------------------
uint8_t nr5g_fapi_fapi2mac_is_valid_wls_ptr(
    void *data)
{
    p_nr5g_fapi_wls_context_t p_wls_ctx = nr5g_fapi_wls_context();
    if ((unsigned long)data >= (unsigned long)p_wls_ctx->shmem &&
        (unsigned long)data < ((unsigned long)p_wls_ctx->shmem +
            p_wls_ctx->nPartitionMemSize)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_fapi2mac_group
 *
 *  @param void
 *
 *  @return  A pointer to WLS_HANDLE stucture
 *
 *  @description
 *  This function returns the WLS instance
 *
**/
//------------------------------------------------------------------------------
static inline WLS_HANDLE nr5g_fapi_fapi2mac_wls_instance(
    )
{
    p_nr5g_fapi_wls_context_t p_wls_ctx = nr5g_fapi_wls_context();

    return p_wls_ctx->h_wls[NR5G_FAPI2MAC_WLS_INST];
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_fapi2phy_group
 *
 *  @param   void
 *
 *  @return  Pointer to the memory block
 *
 *  @description
 *  This function allocates a block of memory from the pool
 *
**/
//------------------------------------------------------------------------------
void *nr5g_fapi_fapi2mac_wls_alloc_buffer(
    )
{
    uint64_t pa_block = 0;
    void *p_va_block = NULL;
    p_nr5g_fapi_wls_context_t p_wls_ctx = nr5g_fapi_wls_context();
    WLS_HANDLE h_wls = nr5g_fapi_fapi2mac_wls_instance();

    if (pthread_mutex_lock((pthread_mutex_t *) &
            p_wls_ctx->fapi2mac_lock_alloc)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("unable to lock alloc pthread mutex"));
        return NULL;
    }
    if (p_fapi2mac_buffers) {
        p_va_block = (void *)p_fapi2mac_buffers;
        p_fapi2mac_buffers = p_fapi2mac_buffers->p_next;
    } else {
        pa_block = (uint64_t) WLS_DequeueBlock((void *)h_wls);
        if (!pa_block) {
            if (pthread_mutex_unlock((pthread_mutex_t *) &
                    p_wls_ctx->fapi2mac_lock_alloc)) {
                NR5G_FAPI_LOG(ERROR_LOG,
                    ("unable to unlock alloc pthread mutex"));
                return NULL;
            }
            //NR5G_FAPI_LOG(ERROR_LOG, ("nr5g_fapi_fapi2phy_wls_alloc_buffer alloc error\n"));
            return NULL;
        }
        p_va_block = (void *)nr5g_fapi_wls_pa_to_va(h_wls, pa_block);
    }
    if (pthread_mutex_unlock((pthread_mutex_t *) &
            p_wls_ctx->fapi2mac_lock_alloc)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("unable to unlock alloc pthread mutex"));
        return NULL;
    }
    return p_va_block;
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_fapi2phy_group
 *
 *  @param   void
 *
 *  @return  Pointer to the memory block
 *
 *  @description
 *  This function allocates a block of memory from the pool
 *
**/
//------------------------------------------------------------------------------
void nr5g_fapi_fapi2mac_wls_free_buffer(
    void *buffers)
{
    p_nr5g_fapi_wls_context_t p_wls_ctx = nr5g_fapi_wls_context();

    if (pthread_mutex_lock((pthread_mutex_t *) &
            p_wls_ctx->fapi2mac_lock_alloc)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("unable to lock alloc pthread mutex"));
        return;
    }
    if (p_fapi2mac_buffers) {
        ((p_fapi_api_queue_elem_t) buffers)->p_next = p_fapi2mac_buffers;
        p_fapi2mac_buffers = (p_fapi_api_queue_elem_t) buffers;
    } else {
        p_fapi2mac_buffers = (p_fapi_api_queue_elem_t) buffers;
        p_fapi2mac_buffers->p_next = NULL;
    }

    if (pthread_mutex_unlock((pthread_mutex_t *) &
            p_wls_ctx->fapi2mac_lock_alloc)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("unable to unlock alloc pthread mutex"));
        return;
    }
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_fapi2phy_group
 *
 *  @param void
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function is called at WLS init and waits in an infinite for L1 to respond back with some information
 *  needed by the L2
 *
**/
//------------------------------------------------------------------------------
uint8_t nr5g_fapi_fapi2mac_wls_ready(
    )
{
    int ret = SUCCESS;
    ret = WLS_Ready1(nr5g_fapi_fapi2mac_wls_instance());
    return ret;
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_fapi2mac_group
 *
 *  @param   void
 *
 *  @return  Number of blocks of APIs received
 *
 *  @description
 *  This functions waits in a infinite loop for L1 to send a list of APIs to MAC. This is called
 *  during runtime when L2 sends a API to L1 and then waits for response back.
 *
**/
//------------------------------------------------------------------------------
uint32_t nr5g_fapi_fapi2mac_wls_wait(
    )
{
    int ret = SUCCESS;
//    NR5G_FAPI_LOG(TRACE_LOG, ("Waiting for MAC to respond in WLS Wait"));
    ret = WLS_Wait1(nr5g_fapi_fapi2mac_wls_instance());
    return ret;
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_fapi2phy_group
 *
 *  @param[out]   data Location where First API from L1 is stored
 *
 *  @return  Size of Message sent from L1
 *
 *  @description
 *  This function checks if this is the last message in this tti.
 *
**/
//------------------------------------------------------------------------------
static inline uint8_t is_msg_present(
    uint16_t flags)
{
    return (!((flags & WLS_TF_FIN) || (flags == 0)));
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_fapi2phy_group
 *
 *  @param[out]   data Location where First API from L1 is stored
 *
 *  @return  Size of Message sent from L1
 *
 *  @description
 *  This function queries the APIs sent from L1 to L2 and gets the first pointer
 *  to the linked list
 *
**/
//------------------------------------------------------------------------------
uint64_t *nr5g_fapi_fapi2mac_wls_get(
    uint32_t * msg_size,
    uint16_t * msg_type,
    uint16_t * flags)
{
    uint64_t *data = NULL;
    WLS_HANDLE h_wls;
    uint32_t ms = 0;
    uint16_t mt = 0, f = 0;

    h_wls = nr5g_fapi_fapi2mac_wls_instance();
    data = (uint64_t *) WLS_Get1(h_wls, &ms, &mt, &f);
    *msg_size = ms, *msg_type = mt, *flags = f;
    NR5G_FAPI_LOG(TRACE_LOG, ("[FAPI2MAC WLS][GET] %p size: %d "
            "type: %x flags: %x", data, *msg_size, *msg_type, *flags));

    return data;
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_fapi2mac_group
 *
 *  @param[in]   ptr Pointer to the block to send
 *  @param[in]   size Size of the block to send
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function sends a single block of API from PHY to MAC
 *
**/
//------------------------------------------------------------------------------
inline uint8_t nr5g_fapi_fapi2mac_wls_put(
    p_fapi_api_queue_elem_t p_msg,
    uint32_t msg_size,
    uint16_t msg_type,
    uint16_t flags)
{
    uint8_t ret = SUCCESS;

    WLS_HANDLE h_mac_wls = nr5g_fapi_fapi2mac_wls_instance();
    uint64_t pa = nr5g_fapi_wls_va_to_pa(h_mac_wls, (void *)p_msg);
    NR5G_FAPI_LOG(TRACE_LOG, ("[FAPI2MAC WLS][PUT] %ld size: %d "
            "type: %x flags: %x", pa, msg_size, msg_type, flags));

    ret = WLS_Put1(h_mac_wls, (uint64_t) pa, msg_size, msg_type, flags);

    return ret;
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_fapi2mac_group
 *
 *  @param[in]   p_list Pointer to the linked list head
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function sends a linked list of APIs from PHY to MAC.
 *
**/
//------------------------------------------------------------------------------
uint8_t nr5g_fapi_fapi2mac_wls_send(
    p_fapi_api_queue_elem_t p_list_elem,
    bool is_urllc)
{
    uint8_t ret = SUCCESS;
    p_fapi_api_queue_elem_t p_curr_msg = NULL;
    fapi_msg_t *p_msg_header = NULL;
    uint16_t flags = 0;
    uint16_t flags_urllc = (is_urllc ? WLS_TF_URLLC : 0);
    p_nr5g_fapi_wls_context_t p_wls_ctx = nr5g_fapi_wls_context();
    uint64_t start_tick = __rdtsc();

    p_curr_msg = p_list_elem;

    if (pthread_mutex_lock((pthread_mutex_t *) & p_wls_ctx->fapi2mac_lock_send)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("unable to lock send pthread mutex"));
        return FAILURE;
    }

    if (p_curr_msg && p_curr_msg->p_next) {
        flags = WLS_SG_FIRST | flags_urllc;
        if (p_curr_msg->msg_type == FAPI_VENDOR_MSG_HEADER_IND) {
            if (SUCCESS != nr5g_fapi_fapi2mac_wls_put(p_curr_msg,
                    p_curr_msg->msg_len + sizeof(fapi_api_queue_elem_t),
                    FAPI_VENDOR_MSG_HEADER_IND, flags)) {
                printf("Error\n");
                if (pthread_mutex_unlock((pthread_mutex_t *) &
                        p_wls_ctx->fapi2mac_lock_send)) {
                    NR5G_FAPI_LOG(ERROR_LOG,
                        ("unable to unlock send pthread mutex"));
                }
                return FAILURE;
            }
            p_curr_msg = p_curr_msg->p_next;
            flags = WLS_SG_NEXT | flags_urllc;
        }

        while (p_curr_msg) {
            // only batch mode
            p_msg_header = (fapi_msg_t *) (p_curr_msg + 1);
            if (p_curr_msg->p_next) {   // FIRST/NEXT
                if (SUCCESS != nr5g_fapi_fapi2mac_wls_put(p_curr_msg,
                        p_curr_msg->msg_len + sizeof(fapi_api_queue_elem_t),
                        p_msg_header->msg_id, flags)) {
                    printf("Error\n");
                    if (pthread_mutex_unlock((pthread_mutex_t *) &
                            p_wls_ctx->fapi2mac_lock_send)) {
                        NR5G_FAPI_LOG(ERROR_LOG,
                            ("unable to unlock send pthread mutex"));
                    }
                    return FAILURE;
                }
                p_curr_msg = p_curr_msg->p_next;
            } else {            // LAST
                flags = WLS_SG_LAST | flags_urllc;
                if (SUCCESS != nr5g_fapi_fapi2mac_wls_put(p_curr_msg,
                        p_curr_msg->msg_len + sizeof(fapi_api_queue_elem_t),
                        p_msg_header->msg_id, flags)) {
                    printf("Error\n");
                    if (pthread_mutex_unlock((pthread_mutex_t *) &
                            p_wls_ctx->fapi2mac_lock_send)) {
                        NR5G_FAPI_LOG(ERROR_LOG,
                            ("unable to unlock send pthread mutex"));
                    }
                    return FAILURE;
                }
                p_curr_msg = NULL;
            }
            flags = WLS_SG_NEXT | flags_urllc;
        }
    }

    if (pthread_mutex_unlock((pthread_mutex_t *) &
            p_wls_ctx->fapi2mac_lock_send)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("unable to unlock send pthread mutex"));
        return FAILURE;
    }
    tick_total_wls_send_per_tti_ul += __rdtsc() - start_tick;

    return ret;
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_wls_fapi2phy_group
 *
 *  @param[out]   data Location where First API from L1 is stored
 *
 *  @return  Size of Message sent from L1
 *
 *  @description
 *  This function queries the APIs sent from L1 to L2 and gets the first pointer
 *  to the linked list
 *
**/
//------------------------------------------------------------------------------
p_fapi_api_queue_elem_t nr5g_fapi_fapi2mac_wls_recv(
    )
{
    uint16_t msg_type = 0;
    uint16_t flags = 0;
    uint32_t msg_size = 0;
    uint32_t num_elms = 0;
    uint64_t *p_msg = NULL;
    p_fapi_api_queue_elem_t p_qelm_list = NULL, p_urllc_qelm_list = NULL;
    p_fapi_api_queue_elem_t p_qelm = NULL;
    p_fapi_api_queue_elem_t p_tail_qelm = NULL, p_urllc_tail_qelm = NULL;
    WLS_HANDLE h_wls = nr5g_fapi_fapi2mac_wls_instance();
    uint64_t start_tick = 0;

    num_elms = nr5g_fapi_fapi2mac_wls_wait();
    if (!num_elms)
        return p_qelm_list;

    start_tick = __rdtsc();
    do {
        p_msg = nr5g_fapi_fapi2mac_wls_get(&msg_size, &msg_type, &flags);
        if (p_msg) {
            p_qelm = (p_fapi_api_queue_elem_t) nr5g_fapi_wls_pa_to_va(h_wls,
                (uint64_t) p_msg);
            if (nr5g_fapi_fapi2mac_is_valid_wls_ptr(p_qelm) == FALSE) {
                printf("Error: Invalid Ptr\n");
                continue;
            }
            p_qelm->p_next = NULL;
            
            if (flags & WLS_TF_URLLC)
            {
                if (p_urllc_qelm_list) {
                    p_urllc_tail_qelm = p_urllc_qelm_list;
                    while (NULL != p_urllc_tail_qelm->p_next) {
                        p_urllc_tail_qelm = p_urllc_tail_qelm->p_next;
                    }
                    p_urllc_tail_qelm->p_next = p_qelm;
                } else {
                    p_urllc_qelm_list = p_qelm;
                }
            } else {
            if (p_qelm_list) {
                p_tail_qelm = p_qelm_list;
                while (NULL != p_tail_qelm->p_next) {
                    p_tail_qelm = p_tail_qelm->p_next;
                }
                p_tail_qelm->p_next = p_qelm;
            } else {
                p_qelm_list = p_qelm;
            }
        }
        }
        num_elms--;
    } while (num_elms && is_msg_present(flags));

    if (p_urllc_qelm_list) {
        nr5g_fapi_urllc_thread_callback(NR5G_FAPI_URLLC_MSG_DIR_MAC2PHY, (void *) p_urllc_qelm_list);
    }

    tick_total_wls_get_per_tti_dl += __rdtsc() - start_tick;

    return p_qelm_list;
}
