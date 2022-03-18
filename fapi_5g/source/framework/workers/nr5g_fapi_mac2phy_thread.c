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
#include "nr5g_fapi_std.h"
#include "nr5g_fapi_mac2phy_thread.h"
#include "nr5g_fapi_fapi2mac_wls.h"
#include "nr5g_fapi_fapi2phy_api.h"
#include "nr5g_fapi_fapi2phy_p5_proc.h"
#include "nr5g_fapi_fapi2phy_p7_proc.h"
#include "nr5g_fapi_log.h"

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_workers_mac2phy_group
 *
 *  @param[in,out]   void
 *
 *  @return none
 *
 *  @description
 *  DOXYGEN_TO_DO
 *
**/
//------------------------------------------------------------------------------
void *nr5g_fapi_mac2phy_thread_func(
    void *config)
{
    cpu_set_t cpuset;
    pthread_t thread;
    p_fapi_api_queue_elem_t p_msg_list = NULL;
    p_nr5g_fapi_phy_ctx_t p_phy_ctx = (p_nr5g_fapi_phy_ctx_t) config;
    uint64_t start_tick;

    NR5G_FAPI_LOG(INFO_LOG, ("[MAC2PHY] Thread %s launched LWP:%ld on "
            "Core: %d\n", __func__, pthread_self(),
            p_phy_ctx->mac2phy_worker_core_id));

    thread = p_phy_ctx->mac2phy_tid = pthread_self();
    CPU_ZERO(&cpuset);
    CPU_SET(p_phy_ctx->mac2phy_worker_core_id, &cpuset);
    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

    usleep(1000);
    while (!p_phy_ctx->process_exit) {
        p_msg_list = nr5g_fapi_fapi2mac_wls_recv();
        if (p_msg_list)
            nr5g_fapi_mac2phy_api_recv_handler(false, config, p_msg_list);

        start_tick = __rdtsc();
        NR5G_FAPI_LOG(TRACE_LOG, ("[MAC2PHY] Send to PHY.."));
        nr5g_fapi_fapi2phy_send_api_list(0);
        tick_total_wls_send_per_tti_dl += __rdtsc() - start_tick;
    }
    pthread_exit(NULL);
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_workers_mac2phy_group
 *
 *  @param[in,out]   void
 *
 *  @return none
 *
 *  @description
 *  DOXYGEN_TO_DO
 *
**/
//------------------------------------------------------------------------------
void nr5g_fapi_mac2phy_api_recv_handler(
    bool is_urllc,
    void *config,
    p_fapi_api_queue_elem_t p_msg_list)
{
    p_fapi_api_queue_elem_t p_per_carr_api_list = NULL;
    p_fapi_api_queue_elem_t p_prev_elm = NULL;
    fapi_msg_t *p_fapi_msg = NULL;
    p_fapi_msg_header_t p_fapi_msg_header = NULL;

    p_nr5g_fapi_phy_ctx_t p_phy_ctx = NULL;
    p_nr5g_fapi_phy_instance_t p_phy_instance = NULL;
    uint8_t num_apis = 0;
    uint8_t phy_id = 0;
    uint64_t start_tick = __rdtsc();

    NR5G_FAPI_LOG(TRACE_LOG, ("[MAC2PHY] %s:", __func__));
    p_phy_ctx = (p_nr5g_fapi_phy_ctx_t) config;
    while (p_msg_list) {
        p_per_carr_api_list = p_msg_list;
        p_fapi_msg_header = (fapi_msg_header_t *) (p_per_carr_api_list + 1);
        num_apis = p_fapi_msg_header->num_msg;
        phy_id = p_fapi_msg_header->handle;

        if (num_apis > 0 && p_msg_list->p_next) {   // likely
            p_per_carr_api_list = p_per_carr_api_list->p_next;
            p_msg_list = p_per_carr_api_list;
            NR5G_FAPI_LOG(TRACE_LOG, ("\n[MAC2PHY] PHY_ID: %d NUM APIs: %d\n",
                    phy_id, num_apis));
        } else {                // unlikely
            // skip to next carrier list. since current fapi message hearder
            // has no apis
            if (p_msg_list->p_next) {
                NR5G_FAPI_LOG(TRACE_LOG, ("\n[MAC2PHY] No APIs for PHY_ID: %d."
                        " Skip...\n", phy_id));
                p_msg_list = p_msg_list->p_next;
                continue;
            } else {
                NR5G_FAPI_LOG(ERROR_LOG, ("\n[MAC2PHY] PHY_ID: %d NUM APIs: %d\n",
                        phy_id, num_apis));
                return;
            }
        }

        // walk through the list and disconnet per carrier apis
        if (p_msg_list->p_next) {
            p_prev_elm = p_msg_list;
            while (p_msg_list) {
                if (FAPI_VENDOR_MSG_HEADER_IND == p_msg_list->msg_type) {
                    p_prev_elm->p_next = NULL;
                    break;
                }
                p_prev_elm = p_msg_list;
                p_msg_list = p_msg_list->p_next;
            }
        } else {
            p_msg_list = NULL;
        }

        if (phy_id > FAPI_MAX_PHY_INSTANCES) {
            NR5G_FAPI_LOG(ERROR_LOG, ("[MAC2PHY]: Invalid Phy Id: %d\n",
                    phy_id));
            continue;
        }

        if (p_per_carr_api_list) {
            p_fapi_msg = (fapi_msg_t *) (p_per_carr_api_list + 1);
#ifdef DEBUG_MODE
            if ((p_fapi_msg->msg_id != FAPI_VENDOR_EXT_UL_IQ_SAMPLES) &&
                (p_fapi_msg->msg_id != FAPI_VENDOR_EXT_ADD_REMOVE_CORE)) {
#endif
                p_phy_instance = &p_phy_ctx->phy_instance[phy_id];
                if (FAPI_STATE_IDLE == p_phy_instance->state) {
                    if (p_fapi_msg->msg_id != FAPI_CONFIG_REQUEST) {
                        NR5G_FAPI_LOG(ERROR_LOG,
                            ("CONFIG.request is not received "
                                "for %d PHY Instance\n", phy_id));
                        continue;
                    }
                    p_phy_instance->phy_id = phy_id;
                }
#ifdef DEBUG_MODE
            }
#endif

            nr5g_fapi_mac2phy_api_processing_handler(is_urllc, p_phy_instance,
                p_per_carr_api_list);
            p_per_carr_api_list = NULL;
        }
    }
    tick_total_parse_per_tti_dl += __rdtsc() - start_tick;
}

//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_workers_mac2phy_group
 *
 *  @param[in,out]   void
 *
 *  @return none
 *
 *  @description
 *  DOXYGEN_TO_DO
 *
**/
//------------------------------------------------------------------------------
void nr5g_fapi_mac2phy_api_processing_handler(
    bool is_urllc,
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    p_fapi_api_queue_elem_t p_msg_list)
{
    uint16_t msg_type;
    p_fapi_api_queue_elem_t p_vendor_elm = NULL;
    p_fapi_api_queue_elem_t p_prev_elm = NULL;
    p_fapi_api_queue_elem_t p_curr_elm = NULL;
    p_fapi_api_queue_elem_t p_tx_data_elm = NULL;
    p_fapi_api_queue_elem_t p_tx_data_pdu_list = NULL;
    p_fapi_api_queue_elem_t p_tx_data_pdu_list_tail = NULL;
    fapi_msg_t *p_fapi_msg = NULL;
    fapi_vendor_msg_t *p_vendor_msg = NULL;

    // Get vendor body if present
    p_prev_elm = p_vendor_elm = p_msg_list;
    while (p_vendor_elm) {
        p_fapi_msg = (fapi_msg_t *) (p_vendor_elm + 1);
        if (p_fapi_msg->msg_id == FAPI_VENDOR_MESSAGE) {
            if (p_prev_elm == p_vendor_elm) {
                NR5G_FAPI_LOG(ERROR_LOG,
                    ("[MAC2PHY] Received only Vendor Message"));
                return;
            }
            p_vendor_msg = (fapi_vendor_msg_t *) p_fapi_msg;
            NR5G_FAPI_LOG(DEBUG_LOG, ("[MAC2PHY] P7 Vendor Msg: %p",
                    p_vendor_msg));
            // disconnect the vendor element from the api list
            p_prev_elm->p_next = NULL;
            break;
        }
        p_prev_elm = p_vendor_elm;
        p_vendor_elm = p_vendor_elm->p_next;
    }

    // split the tx_data_request pdus
    p_curr_elm = p_msg_list;
    while (p_curr_elm) {
        msg_type = p_curr_elm->msg_type;
        if (msg_type == FAPI_TX_DATA_REQUEST) {
            p_tx_data_elm = p_curr_elm;
        } else if (msg_type == FAPI_VENDOR_MSG_PHY_ZBC_BLOCK_REQ) {
            if (p_tx_data_pdu_list) {
                p_tx_data_pdu_list_tail->p_next = p_curr_elm;
                p_tx_data_pdu_list_tail = p_tx_data_pdu_list_tail->p_next;
            } else {
                p_tx_data_pdu_list_tail = p_tx_data_pdu_list = p_curr_elm;
            }
        } else {
        }
        p_curr_elm = p_curr_elm->p_next;
    }

    if (p_tx_data_pdu_list && p_tx_data_elm) {
        p_tx_data_elm->p_tx_data_elm_list = p_tx_data_pdu_list;
        p_tx_data_elm->p_next = NULL;
    }

    if (FAILURE == nr5g_fapi_check_api_ordering(p_phy_instance, p_msg_list)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("API Ordering is wrong."));
        return;
    }
    // Walk through the API list
    while (p_msg_list) {
        p_fapi_msg = (fapi_msg_t *) (p_msg_list + 1);
        switch (p_fapi_msg->msg_id) {
                /*  P5 Vendor Message Processing */
#ifdef DEBUG_MODE
            case FAPI_VENDOR_EXT_ADD_REMOVE_CORE:
                nr5g_fapi_add_remove_core_message(is_urllc,
                    (fapi_vendor_ext_add_remove_core_msg_t *) p_fapi_msg);
                break;
            case FAPI_VENDOR_EXT_UL_IQ_SAMPLES:
                nr5g_fapi_ul_iq_samples_request(is_urllc,
                    (fapi_vendor_ext_iq_samples_req_t *) p_fapi_msg);
                break;

            case FAPI_VENDOR_EXT_DL_IQ_SAMPLES:
                nr5g_fapi_dl_iq_samples_request(is_urllc,
                    (fapi_vendor_ext_iq_samples_req_t *) p_fapi_msg);
                break;

#endif
            case FAPI_VENDOR_EXT_SHUTDOWN_REQUEST:
                {
                    nr5g_fapi_shutdown_request(is_urllc, p_phy_instance,
                        (fapi_vendor_ext_shutdown_req_t *) p_fapi_msg);
                    nr5g_fapi_statistic_info_print();
                    if (g_statistic_start_flag == 1)
                        g_statistic_start_flag = 0;
                }
                break;

                /*  P5 Message Processing */
            case FAPI_CONFIG_REQUEST:
                {
                    nr5g_fapi_config_request(is_urllc, p_phy_instance,
                        (fapi_config_req_t *)
                        p_fapi_msg, p_vendor_msg);
                    nr5g_fapi_statistic_info_init();
                }

                break;

            case FAPI_START_REQUEST:
                nr5g_fapi_start_request(is_urllc, p_phy_instance, (fapi_start_req_t *)
                    p_fapi_msg, p_vendor_msg);
                break;

            case FAPI_STOP_REQUEST:
                {
                    nr5g_fapi_stop_request(is_urllc, p_phy_instance, (fapi_stop_req_t *)
                        p_fapi_msg, p_vendor_msg);
                    nr5g_fapi_statistic_info_print();
                    if (g_statistic_start_flag == 1)
                        g_statistic_start_flag = 0;
                }

                break;
                /*  P7 Message Processing */
            case FAPI_DL_TTI_REQUEST:
                {
                    nr5g_fapi_dl_tti_request(is_urllc, p_phy_instance,
                        (fapi_dl_tti_req_t *)
                        p_fapi_msg, p_vendor_msg);
                    if (g_statistic_start_flag == 0)
                        g_statistic_start_flag = 1;
                }
                break;

            case FAPI_UL_TTI_REQUEST:
                nr5g_fapi_ul_tti_request(is_urllc, p_phy_instance, (fapi_ul_tti_req_t *)
                    p_fapi_msg, p_vendor_msg);
                break;

            case FAPI_UL_DCI_REQUEST:
                nr5g_fapi_ul_dci_request(is_urllc, p_phy_instance, (fapi_ul_dci_req_t *)
                    p_fapi_msg, p_vendor_msg);
                break;

            case FAPI_TX_DATA_REQUEST:
                nr5g_fapi_tx_data_request(is_urllc, p_phy_instance, (fapi_tx_data_req_t *)
                    p_fapi_msg, p_vendor_msg);
                p_msg_list->p_tx_data_elm_list = NULL;
                break;

            default:
                NR5G_FAPI_LOG(ERROR_LOG, ("[MAC2PHY THREAD] Received Unknown Message: [msg_id = 0x%x]",
                    p_fapi_msg->msg_id));
                break;
        }
        p_msg_list = p_msg_list->p_next;
    }
}

uint8_t nr5g_fapi_check_api_ordering(
    p_nr5g_fapi_phy_instance_t p_phy_instance,
    p_fapi_api_queue_elem_t p_msg_list)
{
    uint16_t msg_id, api_order_check = FAPI_CONFIG_REQUEST;
    p_fapi_api_queue_elem_t p_msg = p_msg_list;
    fapi_msg_t *p_fapi_msg = NULL;

    if (p_phy_instance && p_phy_instance->state == FAPI_STATE_RUNNING) {
        p_fapi_msg = (fapi_msg_t *) (p_msg + 1);
        msg_id = p_fapi_msg->msg_id;
        // check if first msg is CONFIG.req
        if (msg_id == FAPI_CONFIG_REQUEST) {
            p_msg = p_msg->p_next;
        }
        api_order_check = FAPI_DL_TTI_REQUEST;
        // Continue checking remaining APIs
        while (p_msg) {
            p_fapi_msg = (fapi_msg_t *) (p_msg + 1);
            msg_id = p_fapi_msg->msg_id;
            if ((msg_id == FAPI_DL_TTI_REQUEST) && (msg_id == api_order_check)) {
                api_order_check = FAPI_UL_TTI_REQUEST;
            } else if ((msg_id == FAPI_UL_TTI_REQUEST) &&
                (msg_id == api_order_check)) {
                api_order_check = FAPI_UL_DCI_REQUEST;
            } else {
                break;
            }
            p_msg = p_msg->p_next;
        }
        if (api_order_check != FAPI_UL_DCI_REQUEST) {
            return FAILURE;
        }
    }

    return SUCCESS;
}
