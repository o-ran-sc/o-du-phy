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
 * @file This file contains implementation of all the functions used 
 * to send APIs from FAPI to MAC
 *
 **/

#include <stdio.h>
#include "nr5g_fapi_internal.h"
#include "gnb_l1_l2_api.h"
#include "nr5g_fapi_fapi2mac_api.h"
#include "nr5g_fapi_fapi2mac_wls.h"
#include "nr5g_fapi_log.h"

static nr5g_fapi_fapi2mac_queue_t fapi2mac_q[FAPI_MAX_PHY_INSTANCES];
static nr5g_fapi_fapi2mac_queue_t fapi2mac_urllc_q[FAPI_MAX_PHY_INSTANCES];


//------------------------------------------------------------------------------
/** @ingroup     group_source_api_fapi2phy
 *
 *  @param[in]   phy_id Value of phy_id.
 *  @param[in]   is_urllc True for urllc, false otherwise.
 *
 *  @return      Pointer to fapi2mac api queue.
 *
 *  @description This function access proper instance of fapi2mac queue.
 *
**/
//------------------------------------------------------------------------------
static inline p_nr5g_fapi_fapi2mac_queue_t nr5g_fapi_fapi2mac_queue(
    uint8_t phy_id,
    bool is_urllc)
{
    return is_urllc ? &fapi2mac_urllc_q[phy_id] : &fapi2mac_q[phy_id];
}

//------------------------------------------------------------------------------
/** @ingroup     group_lte_source_phy_fapi
 *
 *  @param[in]   pListElem Pointer to the ListElement
 *
 *  @return      void
 *
 *  @description This function adds a ListElement API to a Linked list which will
 *               be sent to L1 once all APIs for a TTI are added
 *
**/
//------------------------------------------------------------------------------
void nr5g_fapi_fapi2mac_add_api_to_list(
    uint8_t phy_id,
    p_fapi_api_queue_elem_t p_list_elem,
    bool is_urllc)
{
    p_nr5g_fapi_fapi2mac_queue_t queue = NULL;
    p_fapi_msg_header_t p_fapi_msg_hdr = NULL;

    if (!p_list_elem) {
        return;
    }

    queue = nr5g_fapi_fapi2mac_queue(phy_id, is_urllc);
    if (pthread_mutex_lock((pthread_mutex_t *) & queue->lock)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("unable to lock fapi2mac aggregate list"
                "pthread mutex"));
        return;
    }
    if (queue->p_send_list_head && queue->p_send_list_tail) {
        p_fapi_msg_hdr = (p_fapi_msg_header_t) (queue->p_send_list_head + 1);
        p_fapi_msg_hdr->num_msg += 1;
        queue->p_send_list_tail->p_next = p_list_elem;
        queue->p_send_list_tail = p_list_elem;
    } else {
        queue->p_send_list_head = queue->p_send_list_tail = p_list_elem;
    }
    if (pthread_mutex_unlock((pthread_mutex_t *) & queue->lock)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("unable to unlock fapi2mac aggregate list"
                "pthread mutex"));
        return;
    }
}

//------------------------------------------------------------------------------
/** @ingroup      group_lte_source_phy_fapi
 *
 *  @param        A pointer to phy Instance
 *
 *  @return       FAPI status
 *
 *  @description  This function send API list to L1
 *
**/
//------------------------------------------------------------------------------
void nr5g_fapi_fapi2mac_send_api_list(
    bool is_urllc)
{
    uint8_t phy_id = 0;
    p_fapi_msg_header_t p_fapi_msg_hdr = NULL;
    p_fapi_api_queue_elem_t p_commit_list_head = NULL;
    p_fapi_api_queue_elem_t p_commit_list_tail = NULL;
    p_nr5g_fapi_fapi2mac_queue_t queue = NULL;

    for (phy_id = 0; phy_id < FAPI_MAX_PHY_INSTANCES; phy_id++) {
        queue = nr5g_fapi_fapi2mac_queue(phy_id, is_urllc);
        if (pthread_mutex_lock((pthread_mutex_t *) & queue->lock)) {
            NR5G_FAPI_LOG(ERROR_LOG, ("unable to lock fapi2mac aggregate list"
                    "pthread mutex"));
            return;
        }
        if (queue->p_send_list_head && queue->p_send_list_tail) {
            p_fapi_msg_hdr =
                (p_fapi_msg_header_t) (queue->p_send_list_head + 1);
            if (p_fapi_msg_hdr->num_msg) {
                if (p_commit_list_head && p_commit_list_tail) {
                    p_commit_list_tail->p_next = queue->p_send_list_head;
                    p_commit_list_tail = queue->p_send_list_tail;
                } else {
                    p_commit_list_head = queue->p_send_list_head;
                    p_commit_list_tail = queue->p_send_list_tail;
                }
            } else {
                nr5g_fapi_fapi2mac_wls_free_buffer((void *)
                    queue->p_send_list_head);
            }
            queue->p_send_list_head = queue->p_send_list_tail = NULL;
        }
        if (pthread_mutex_unlock((pthread_mutex_t *) & queue->lock)) {
            NR5G_FAPI_LOG(ERROR_LOG, ("unable to unlock fapi2mac aggregate list"
                    "pthread mutex"));
            return;
        }
    }

    if (p_commit_list_head)
        nr5g_fapi_fapi2mac_wls_send(p_commit_list_head, is_urllc);
}

//------------------------------------------------------------------------------
/** @ingroup     group_lte_source_phy_fapi
 *
 *  @param[in]   phyInstance PhyInstance Id
 *  @param[in]   NumMessageInBlock Number of Messages in Block
 *  @param[in]   AlignOffset Align Offset
 *  @param[in]   MsgType Message Type
 *  @param[in]   FrameNum Frame Number
 *  @param[in]   subFrameNum subframe Number
 *
 *  @return      Pointer to the List Element structure
 *
 *  @description This function allocates a buffer from shared memory WLS
 *               interface and creates a List Element structures. It then fills
 *               all the fields with data being passed in.
 *
**/
//------------------------------------------------------------------------------
p_fapi_api_queue_elem_t nr5g_fapi_fapi2mac_create_api_list_elem(
    uint32_t msg_type,
    uint16_t num_message_in_block,
    uint32_t align_offset)
{
    p_fapi_api_queue_elem_t p_list_elem = NULL;

    p_list_elem = (p_fapi_api_queue_elem_t)
        nr5g_fapi_fapi2mac_wls_alloc_buffer();
    //Fill header for link list of API messages
    if (p_list_elem) {
        p_list_elem->msg_type = (uint8_t) msg_type;
        p_list_elem->num_message_in_block = num_message_in_block;
        p_list_elem->align_offset = (uint16_t) align_offset;
        p_list_elem->msg_len = num_message_in_block * align_offset;
        p_list_elem->p_next = NULL;
        p_list_elem->p_tx_data_elm_list = NULL;
        p_list_elem->time_stamp = 0;
    }

    return p_list_elem;
}

//------------------------------------------------------------------------------
/** @ingroup     group_lte_source_phy_fapi
 *
 *  @return      void
 *
 *  @description This function initializes the pthead_mutext_lock.
 */
void nr5g_fapi_fapi2mac_init_api_list(
    )
{
    uint8_t phy_id = 0;
    p_nr5g_fapi_fapi2mac_queue_t queue = NULL;

    for (phy_id = 0; phy_id < FAPI_MAX_PHY_INSTANCES; phy_id++) {
        queue = nr5g_fapi_fapi2mac_queue(phy_id, false);
        pthread_mutex_init((pthread_mutex_t *) & queue->lock, NULL);
        queue = nr5g_fapi_fapi2mac_queue(phy_id, true);
        pthread_mutex_init((pthread_mutex_t *) & queue->lock, NULL);
    }
}
