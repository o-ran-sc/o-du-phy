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
 * @file This file defines all the functions used for statistics. 
 *
 **/
#include "nr5g_fapi_std.h"
#include "nr5g_fapi_stats.h"
#include "nr5g_fapi_log.h"

#define NR5G_FAPI_STATS_FNAME_LEN   64

void nr5g_fapi_print_phy_instance_stats(
    p_nr5g_fapi_phy_instance_t p_phy_instance)
{
    int fd = 0, ret_val;
    FILE *fp = NULL;
    enum { FILE_MODE = 0666 };
    nr5g_fapi_stats_t *p_stats;
    char *fname = NULL;
#ifdef DEBUG_MODE
    char stats_fname[NR5G_FAPI_STATS_FNAME_LEN];
    uint32_t test_num;
    uint32_t test_type;
    char test_type_str[][8] = { {"DL"}, {"UL"}, {"FD"} };
#endif

    if (NULL == p_phy_instance) {
        NR5G_FAPI_LOG(ERROR_LOG, ("[NR5G_FAPI][STATS] Invalid "
                "phy instance"));
        return;
    }
#ifdef DEBUG_MODE
    if (p_phy_instance->shutdown_test_type) {
        test_num = p_phy_instance->shutdown_test_type & 0xFFFFFFF;
        test_type = p_phy_instance->shutdown_test_type >> 28;
        snprintf(stats_fname, NR5G_FAPI_STATS_FNAME_LEN,
            "FapiStats_%s_%d.txt", test_type_str[test_type], test_num);
        fname = stats_fname;
    }
#endif

    if (!fname)
        fname = NR5G_FAPI_STATS_FNAME;

    ret_val = remove(fname);
    if ((-1 == ret_val) && (ENOENT != errno)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("File %s delete not successful errno %d",
                fname, errno));
        perror("[Error] ");
        return;
    }

    fd = open(fname, O_RDWR | O_CREAT | O_EXCL);
    if (-1 == fd) {
        NR5G_FAPI_LOG(ERROR_LOG, ("Failed to open the file %s\n", fname));
        perror("[Error] ");
        return;
    }

    fp = fdopen(fd, "w");
    if (NULL == fp) {
        NR5G_FAPI_LOG(ERROR_LOG,
            ("Failed to open the file %s from file descriptor\n", fname));
        perror("[Error] ");

        if (-1 == close(fd)) {
            NR5G_FAPI_LOG(ERROR_LOG,
                ("Failed to close the descriptor for file %s\n", fname));
            perror("[Error] ");
        }
        return;
    }

    p_stats = &p_phy_instance->stats;
    fprintf(fp, "5GNR FAPI instance statistics PhyId: %u\n",
        p_phy_instance->phy_id);
    fprintf(fp, "%*s: %u\t\t\t%*s: %u\n", 14, "ParamReq",
        p_stats->fapi_stats.fapi_param_req, 14, "ConfigReq",
        p_stats->fapi_stats.fapi_config_req);
    fprintf(fp, "%*s: %u\t\t\t%*s: %u\n", 14, "StartReq",
        p_stats->fapi_stats.fapi_start_req, 14, "StopReq",
        p_stats->fapi_stats.fapi_stop_req);
    fprintf(fp, "%*s: %u\t\t\t%*s: %u\n", 14, "ParamResp",
        p_stats->fapi_stats.fapi_param_res, 14, "ConfigResp",
        p_stats->fapi_stats.fapi_config_res);
    fprintf(fp, "%*s: %u\t\t\t%*s: %u\n", 14, "stopInd",
        p_stats->fapi_stats.fapi_stop_ind, 14, "vendorMsg",
        p_stats->fapi_stats.fapi_vendor_msg);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "DLTTIReq",
        p_stats->fapi_stats.fapi_dl_tti_req, 14, "ULTTIReq",
        p_stats->fapi_stats.fapi_ul_tti_req);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "TxDataReq",
        p_stats->fapi_stats.fapi_tx_data_req, 14, "ErrorInd",
        p_stats->fapi_stats.fapi_error_ind);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "ULDciReq",
        p_stats->fapi_stats.fapi_ul_dci_req, 14, "SlotInd",
        p_stats->fapi_stats.fapi_slot_ind);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "CrcInd",
        p_stats->fapi_stats.fapi_crc_ind, 14, "RxDataInd",
        p_stats->fapi_stats.fapi_rx_data_ind);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "UciInd",
        p_stats->fapi_stats.fapi_uci_ind, 14, "SrsInd",
        p_stats->fapi_stats.fapi_srs_ind);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %u\n", 14, "RachInd",
        p_stats->fapi_stats.fapi_rach_ind, 14, "ShutdownReq",
        p_stats->fapi_stats.fapi_vext_shutdown_req);
    fprintf(fp, "%*s: %u\t\t\t%*s: %lu\n", 14, "ShutdownRes",
        p_stats->fapi_stats.fapi_vext_shutdown_res, 14, "ULTTIReqPdus",
        p_stats->fapi_stats.fapi_ul_tti_pdus);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "ULTTIReqPrach",
        p_stats->fapi_stats.fapi_ul_tti_prach_pdus, 14, "ULTTIReqPusch",
        p_stats->fapi_stats.fapi_ul_tti_pusch_pdus);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "ULTTIReqPucch",
        p_stats->fapi_stats.fapi_ul_tti_pucch_pdus, 14, "ULTTIReqSrs",
        p_stats->fapi_stats.fapi_ul_tti_srs_pdus);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "DLTTIReqPdus",
        p_stats->fapi_stats.fapi_dl_tti_pdus, 14, "DLTTIReqPdsch",
        p_stats->fapi_stats.fapi_dl_tti_pdsch_pdus);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "DLTTIReqPdcch",
        p_stats->fapi_stats.fapi_dl_tti_pdcch_pdus, 14, "DLTTIReqSsb",
        p_stats->fapi_stats.fapi_dl_tti_ssb_pdus);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "DLTTIReqCsiRs",
        p_stats->fapi_stats.fapi_dl_tti_csi_rs_pdus, 14, "ULDCIReqPdus",
        p_stats->fapi_stats.fapi_ul_dci_pdus);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "CrcIndPdus",
        p_stats->fapi_stats.fapi_crc_ind_pdus, 14, "RxDataPdus",
        p_stats->fapi_stats.fapi_rx_data_ind_pdus);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "UciIndPdus",
        p_stats->fapi_stats.fapi_uci_ind_pdus, 14, "SrsIndPdus",
        p_stats->fapi_stats.fapi_srs_ind_pdus);
    fprintf(fp, "%*s: %lu\t\t\t\n", 14, "RachPdus",
        p_stats->fapi_stats.fapi_rach_ind_pdus);
    fprintf(fp, "\n\n");

    fprintf(fp, "5GNR L1 instance statistics PhyId: %u\n",
        p_phy_instance->phy_id);
    fprintf(fp, "%*s: %u\t\t\t%*s: %u\n", 14, "ParamReq",
        p_stats->iapi_stats.iapi_param_req, 14, "ConfigReq",
        p_stats->iapi_stats.iapi_config_req);
    fprintf(fp, "%*s: %u\t\t\t%*s: %u\n", 14, "StartReq",
        p_stats->iapi_stats.iapi_start_req, 14, "StopReq",
        p_stats->iapi_stats.iapi_stop_req);
    fprintf(fp, "%*s: %u\t\t\t%*s: %u\n", 14, "StartRes",
        p_stats->iapi_stats.iapi_start_res, 14, "StopInd",
        p_stats->iapi_stats.iapi_stop_ind);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "DlConfigReq",
        p_stats->iapi_stats.iapi_dl_config_req, 14, "UlConfigReq",
        p_stats->iapi_stats.iapi_ul_config_req);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "TxReq",
        p_stats->iapi_stats.iapi_tx_req, 14, "ErrorInd",
        p_stats->iapi_stats.iapi_error_ind);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "UlDciReq",
        p_stats->iapi_stats.iapi_ul_dci_req, 14, "SlotInd",
        p_stats->iapi_stats.iapi_slot_ind);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "CrcInd",
        p_stats->iapi_stats.iapi_crc_ind, 14, "RxDataInd",
        p_stats->iapi_stats.iapi_rx_data_ind);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "UciInd",
        p_stats->iapi_stats.iapi_uci_ind, 14, "SrsInd",
        p_stats->iapi_stats.iapi_srs_ind);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %u\n", 14, "RACHInd",
        p_stats->iapi_stats.iapi_rach_ind, 14, "ShutdownReq",
        p_stats->iapi_stats.iapi_shutdown_req);
    fprintf(fp, "%*s: %u\t\t\t%*s: %lu\n", 14, "ShutdownRes",
        p_stats->iapi_stats.iapi_shutdown_res, 14, "ULTTIReqPdus",
        p_stats->iapi_stats.iapi_ul_tti_pdus);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "ULTTIReqPrach",
        p_stats->iapi_stats.iapi_ul_tti_prach_pdus, 14, "ULTTIReqPusch",
        p_stats->iapi_stats.iapi_ul_tti_pusch_pdus);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "ULTTIReqPucch",
        p_stats->iapi_stats.iapi_ul_tti_pucch_pdus, 14, "ULTTIReqSrs",
        p_stats->iapi_stats.iapi_ul_tti_srs_pdus);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "DLTTIReqPdus",
        p_stats->iapi_stats.iapi_dl_tti_pdus, 14, "DLTTIReqPdsch",
        p_stats->iapi_stats.iapi_dl_tti_pdsch_pdus);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "DLTTIReqPdcch",
        p_stats->iapi_stats.iapi_dl_tti_pdcch_pdus, 14, "DLTTIReqSsb",
        p_stats->iapi_stats.iapi_dl_tti_ssb_pdus);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "DLTTIReqCsiRs",
        p_stats->iapi_stats.iapi_dl_tti_csi_rs_pdus, 14, "ULDCIReqPdus",
        p_stats->iapi_stats.iapi_ul_dci_pdus);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "CrcIndPdus",
        p_stats->iapi_stats.iapi_crc_ind_pdus, 14, "RxDataPdus",
        p_stats->iapi_stats.iapi_rx_data_ind_pdus);
    fprintf(fp, "%*s: %lu\t\t\t%*s: %lu\n", 14, "UciIndPdus",
        p_stats->iapi_stats.iapi_uci_ind_pdus, 14, "SrsIndPdus",
        p_stats->iapi_stats.iapi_srs_ind_pdus);
    fprintf(fp, "%*s: %lu\t\t\t\n", 14, "RachPreambles",
        p_stats->iapi_stats.iapi_rach_preambles);
    fprintf(fp, "\n");
    if (EOF == fclose(fp)) {
        NR5G_FAPI_LOG(ERROR_LOG, ("Unable to close the file\n"));
    }
    close(fd);
}
