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
 * to send APIs from FAPI to PHY
 *
 **/

#include <stdio.h>
#include "nr5g_fapi_internal.h"
#include "gnb_l1_l2_api.h"
#include "nr5g_fapi_wls.h"
#include "nr5g_fapi_fapi2phy_api.h"
#include "nr5g_fapi_fapi2phy_wls.h"
#include "nr5g_fapi_log.h"

nr5g_fapi_fapi2phy_queue_t fapi2phy_q;
nr5g_fapi_fapi2phy_queue_t fapi2phy_q_urllc;

//------------------------------------------------------------------------------
/** @ingroup     group_source_api_fapi2phy
 *
 *  @param[in]   p_list_elem Pointer to the ListElement
 *
 *  @return      void
 *
 *  @description This function adds a ListElement API to a Linked list which will
 *               be sent to L1 once all APIs for a TTI are added
 *
**/
//------------------------------------------------------------------------------
p_nr5g_fapi_fapi2phy_queue_t nr5g_fapi_fapi2phy_queue(
    )
{
    return &fapi2phy_q;
}

p_nr5g_fapi_fapi2phy_queue_t nr5g_fapi_fapi2phy_queue_urllc(
    )
{
    return &fapi2phy_q_urllc;
}

uint8_t nr5g_fapi_get_stats_location(
    uint8_t msg_type)
{
    uint8_t loc;
    switch (msg_type) {
        case MSG_TYPE_PHY_CONFIG_REQ:
            loc = MEM_STAT_CONFIG_REQ;
            break;

        case MSG_TYPE_PHY_START_REQ:
            loc = MEM_STAT_START_REQ;
            break;

        case MSG_TYPE_PHY_STOP_REQ:
            loc = MEM_STAT_STOP_REQ;
            break;

        case MSG_TYPE_PHY_SHUTDOWN_REQ:
            loc = MEM_STAT_SHUTDOWN_REQ;
            break;

        case MSG_TYPE_PHY_DL_CONFIG_REQ:
            loc = MEM_STAT_DL_CONFIG_REQ;
            break;

        case MSG_TYPE_PHY_UL_CONFIG_REQ:
            loc = MEM_STAT_UL_CONFIG_REQ;
            break;

        case MSG_TYPE_PHY_UL_DCI_REQ:
            loc = MEM_STAT_UL_DCI_REQ;
            break;

        case MSG_TYPE_PHY_TX_REQ:
            loc = MEM_STAT_TX_REQ;
            break;

        case MSG_TYPE_PHY_DL_IQ_SAMPLES:
            loc = MEM_STAT_DL_IQ_SAMPLES;
            break;

        case MSG_TYPE_PHY_UL_IQ_SAMPLES:
            loc = MEM_STAT_UL_IQ_SAMPLES;
            break;

        default:
            loc = MEM_STAT_DEFAULT;
    }

    return loc;
}

//------------------------------------------------------------------------------
/** @ingroup     group_source_api_fapi2phy
 *
 *  @param[in]   msg_type Message Type
 *  @param[in]   num_message_in_blockn Number of Messages in Block
 *  @param[in]   align_offset Align Offset
 *
 *  @return      Pointer to the List Element structure
 *
 *  @description This function allocates a buffer from shared memory WLS
 *               interface and creates a List Element structures. It then fills
 *               all the fields with data being passed in.
 *
**/
//------------------------------------------------------------------------------
PMAC2PHY_QUEUE_EL nr5g_fapi_fapi2phy_create_api_list_elem(
    uint32_t msg_type,
    uint16_t num_message_in_block,
    uint32_t align_offset)
{
    PMAC2PHY_QUEUE_EL p_list_elem = NULL;
    uint8_t loc;

    loc = nr5g_fapi_get_stats_location(msg_type);
    p_list_elem = (PMAC2PHY_QUEUE_EL) wls_fapi_alloc_buffer(0, loc);
    //Fill header for link list of API messages
    if (p_list_elem) {
        p_list_elem->nMessageType = (uint8_t) msg_type;
        p_list_elem->nNumMessageInBlock = num_message_in_block;
        p_list_elem->nAlignOffset = (uint16_t) align_offset;
        p_list_elem->nMessageLen = num_message_in_block * align_offset;
        p_list_elem->pNext = NULL;
    }

    return p_list_elem;
}

//------------------------------------------------------------------------------
/** @ingroup     group_source_api_fapi2phy
 *
 *  @param[in]   p_list_elem Pointer to the ListElement
 *
 *  @return      void
 *
 *  @description This function adds a ListElement API to a Linked list which will
 *               be sent to L1 once all APIs for a TTI are added
 *
**/
//------------------------------------------------------------------------------
void nr5g_fapi_fapi2phy_add_to_api_list(
    bool is_urllc,
    PMAC2PHY_QUEUE_EL p_list_elem)
{
    p_nr5g_fapi_fapi2phy_queue_t queue = NULL;

    if (!p_list_elem) {
        return;
    }

    queue = is_urllc ? nr5g_fapi_fapi2phy_queue_urllc()
                     : nr5g_fapi_fapi2phy_queue();

    if (queue->p_send_list_head && queue->p_send_list_tail) {
        queue->p_send_list_tail->pNext = p_list_elem;
        queue->p_send_list_tail = p_list_elem;
    } else {
        queue->p_send_list_head = queue->p_send_list_tail = p_list_elem;
    }
}

//------------------------------------------------------------------------------
/** @ingroup      group_source_api_fapi2phy
 *
 *  @param        A pointer to phy Instance
 *
 *  @return       FAPI status
 *
 *  @description  This function send API list to L1
 *
**/
//------------------------------------------------------------------------------
void nr5g_fapi_fapi2phy_send_api_list(
    bool is_urllc)
{
    uint8_t ret = FAILURE;
    p_nr5g_fapi_fapi2phy_queue_t queue = NULL;

    queue = is_urllc ? nr5g_fapi_fapi2phy_queue_urllc()
                     : nr5g_fapi_fapi2phy_queue();
    if (queue->p_send_list_head) {

        NR5G_FAPI_LOG(TRACE_LOG,
            ("[NR5G_FAPI][FAPI2PHY] Sending API's to PHY"));
        ret = nr5g_fapi_fapi2phy_wls_send(queue->p_send_list_head, is_urllc);
        if (FAILURE == ret) {
            NR5G_FAPI_LOG(ERROR_LOG,
                ("[NR5G_FAPI][FAPI2PHY] Error sending API's to PHY"));
        }
        queue->p_send_list_tail = queue->p_send_list_head = NULL;
    }
}

//------------------------------------------------------------------------------
/** @ingroup        group_source_api_fapi2phy
 *
 *  @param[in]      A pointer to phy instance
 *
 *  @return         void
 *
 *  @description    The function adds all memory elements to WLS free list
 *
 **/
//-------------------------------------------------------------------------------------------

void nr5g_fapi_fapi2phy_destroy_api_list_elem(
    PMAC2PHY_QUEUE_EL p_list_elem)
{
    if (p_list_elem) {
        uint8_t loc = nr5g_fapi_get_stats_location(p_list_elem->nMessageType);
        wls_fapi_free_buffer(p_list_elem, loc);
    }
}
