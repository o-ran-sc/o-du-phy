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
#include "nr5g_fapi_framework.h"
#include "nr5g_fapi_fapi2phy_wls.h"
#include "nr5g_fapi_phy2mac_thread.h"
#include "nr5g_fapi_fapi2mac_api.h"
#include "nr5g_fapi_fapi2mac_p5_proc.h"
#include "nr5g_fapi_fapi2mac_p7_proc.h"
#include "nr5g_fapi_log.h"
//------------------------------------------------------------------------------
/** @ingroup nr5g_fapi_source_framework_workers_phy2mac_group
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
void *nr5g_fapi_phy2mac_thread_func(
    void *config)
{
    cpu_set_t cpuset;
    pthread_t thread;
    PMAC2PHY_QUEUE_EL p_msg_list = NULL;
    p_nr5g_fapi_phy_ctx_t p_phy_ctx = (p_nr5g_fapi_phy_ctx_t) config;

    NR5G_FAPI_LOG(INFO_LOG, ("[PHY2MAC] Thread %s launched LWP:%ld on "
            "Core: %d\n", __func__, pthread_self(),
            p_phy_ctx->phy2mac_worker_core_id));

    thread = p_phy_ctx->phy2mac_tid = pthread_self();
    CPU_ZERO(&cpuset);
    CPU_SET(p_phy_ctx->phy2mac_worker_core_id, &cpuset);
    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

    nr5g_fapi_fapi2mac_init_api_list();

    usleep(1000);
    while (!p_phy_ctx->process_exit) {
        p_msg_list = nr5g_fapi_fapi2phy_wls_recv();
        if (p_msg_list)
            nr5g_fapi_phy2mac_api_recv_handler(false, config, p_msg_list);

        nr5g_fapi_fapi2mac_send_api_list(false);
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
void nr5g_fapi_phy2mac_api_recv_handler(
    bool is_urllc,
    void *config,
    PMAC2PHY_QUEUE_EL p_msg_list)
{
    PMAC2PHY_QUEUE_EL p_curr_msg;
    PL1L2MessageHdr p_msg_header = NULL;
    uint64_t start_tick = __rdtsc();
    fapi_api_stored_vendor_queue_elems vendor_extension_elems;
    NR5G_FAPI_LOG(TRACE_LOG, ("[PHY2MAC] %s:", __func__));

    memset(&vendor_extension_elems, 0, sizeof(vendor_extension_elems));
    nr5g_fapi_message_header((p_nr5g_fapi_phy_ctx_t) config, is_urllc);

    p_curr_msg = (PMAC2PHY_QUEUE_EL) p_msg_list;
    while (p_curr_msg) {
        p_msg_header = (PL1L2MessageHdr) (p_curr_msg + 1);
        switch (p_msg_header->nMessageType) {
                /*  P5 Vendor Message Processing */
#ifdef DEBUG_MODE
            case MSG_TYPE_PHY_DL_IQ_SAMPLES:
                {
                    nr5g_fapi_dl_iq_samples_response((p_nr5g_fapi_phy_ctx_t)
                        config, (PADD_REMOVE_BBU_CORES) p_msg_header);
                }
                break;

            case MSG_TYPE_PHY_UL_IQ_SAMPLES:
                {
                    nr5g_fapi_ul_iq_samples_response((p_nr5g_fapi_phy_ctx_t)
                        config, (PADD_REMOVE_BBU_CORES) p_msg_header);
                }
                break;
#endif
            case MSG_TYPE_PHY_SHUTDOWN_RESP:
                {
                    nr5g_fapi_shutdown_response((p_nr5g_fapi_phy_ctx_t) config,
                        (PSHUTDOWNRESPONSEStruct) p_msg_header);
                }
                break;

                /*  P5 Message Processing */
            case MSG_TYPE_PHY_CONFIG_RESP:
                {
                    nr5g_fapi_config_response((p_nr5g_fapi_phy_ctx_t) config,
                        (PCONFIGRESPONSEStruct) p_msg_header);
                }
                break;

            case MSG_TYPE_PHY_START_RESP:
                {
                    nr5g_fapi_start_resp((p_nr5g_fapi_phy_ctx_t) config,
                        (PSTARTRESPONSEStruct) p_msg_header);
                }
                break;

            case MSG_TYPE_PHY_STOP_RESP:
                {
                    nr5g_fapi_stop_indication((p_nr5g_fapi_phy_ctx_t) config,
                        (PSTOPRESPONSEStruct) p_msg_header);
                }
                break;

                /*  P7 Message Processing */
            case MSG_TYPE_PHY_RX_ULSCH_IND:
                {
                    nr5g_fapi_rx_data_indication(is_urllc,
                        (p_nr5g_fapi_phy_ctx_t) config,
                        &vendor_extension_elems,
                        (PRXULSCHIndicationStruct) p_msg_header);
                }
                break;

            case MSG_TYPE_PHY_RX_ULSCH_UCI_IND:
                {
                    nr5g_fapi_rx_data_uci_indication(is_urllc,
                        (p_nr5g_fapi_phy_ctx_t) config,
                        (PRXULSCHUCIIndicationStruct) p_msg_header);
                }
                break;

            case MSG_TYPE_PHY_CRC_IND:
                {
                    nr5g_fapi_crc_indication(is_urllc,
                        (p_nr5g_fapi_phy_ctx_t) config,
                        &vendor_extension_elems,
                        (PCRCIndicationStruct) p_msg_header);
                }
                break;

            case MSG_TYPE_PHY_UCI_IND:
                {
                    nr5g_fapi_uci_indication(is_urllc,
                        (p_nr5g_fapi_phy_ctx_t) config,
                        &vendor_extension_elems,
                        (PRXUCIIndicationStruct) p_msg_header);
                }
                break;

            case MSG_TYPE_PHY_RX_RACH_IND:
                {
                    nr5g_fapi_rach_indication(is_urllc,
                        (p_nr5g_fapi_phy_ctx_t) config,
                        (PRXRACHIndicationStruct) p_msg_header);
                }
                break;

            case MSG_TYPE_PHY_RX_SRS_IND:
                {
                    nr5g_fapi_srs_indication(is_urllc,
                        (p_nr5g_fapi_phy_ctx_t) config,
                        &vendor_extension_elems,
                        (PRXSRSIndicationStruct) p_msg_header);
                }
                break;

            case MSG_TYPE_PHY_SLOT_IND:
                {
                    nr5g_fapi_slot_indication(is_urllc,
                        (p_nr5g_fapi_phy_ctx_t) config,
                        &vendor_extension_elems,
                        (PSlotIndicationStruct) p_msg_header);
                    nr5g_fapi_statistic_info_set_all();
                }
                break;

            case MSG_TYPE_PHY_ERR_IND:
                {
                }
                break;

            default:
                NR5G_FAPI_LOG(ERROR_LOG, ("[PHY2MAC THREAD] Received Unknown Message: [nMessageType = 0x%x]",
                    p_msg_header->nMessageType));
                break;
        }
        p_curr_msg = p_curr_msg->pNext;
    }
    nr5g_fapi_proc_vendor_p7_msgs_move_to_api_list(is_urllc, &vendor_extension_elems);

    tick_total_parse_per_tti_ul += __rdtsc() - start_tick;

}
