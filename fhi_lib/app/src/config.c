/******************************************************************************
*
*   Copyright (c) 2020 Intel.
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
 * @brief
 * @file
 * @ingroup
 * @author Intel Corporation
 **/

#include <inttypes.h>
#include <immintrin.h>
#include "rte_common.h"
#include "config.h"
#include "common.h"
#include "debug.h"

#include <rte_ethdev.h>

#define MAX_LINE_SIZE 512
/* Configuration file maximum supported line length */

#define KEY_APP_MODE        "appMode"
#define KEY_XRAN_MODE       "xranMode"
#define KEY_XRAN_TECH       "xranRanTech"
#define KEY_MU_NUMBER       "mu"
#define KEY_UL_SCS          "nbIotUlScs"
#define KEY_FREQ_OFFSET     "freqOffset"
#define KEY_EAXC_OFFSET     "eAxcOffset"
#define KEY_MIXED_NUM_ENABLE "mixedNumEnable"
#define KEY_NDLABSFREPOINTA "nDLAbsFrePointA"
#define KEY_NULABSFREPOINTA "nULAbsFrePointA"
#define KEY_NDLBANDWIDTH    "nDLBandwidth"
#define KEY_NULBANDWIDTH    "nULBandwidth"
#define KEY_NDLFFTSIZE      "nDLFftSize"
#define KEY_NULFFTSIZE      "nULFftSize"

#define KEY_DU_PORT_ID_BITWIDTH    "DU_Port_ID_bitwidth"
#define KEY_BANDSECTOR_ID_BITWIDTH "BandSector_ID_bitwidth"
#define KEY_CC_ID_BITWIDTH         "CC_ID_bitwidth"
#define KEY_RU_PORT_ID_BITWIDTH    "RU_Port_ID_bitwidth"

#define KEY_NFRAMEDUPLEXTYPE "nFrameDuplexType"
#define KEY_NTDDPERIOD       "nTddPeriod"

#define KEY_SSLOTCONFIG     "sSlotConfig"

#define KEY_XU_NUM          "oXuNum"
#define KEY_XU_ETH_LINK_SPD "oXuEthLinkSpeed"
#define KEY_XU_ETH_LINE_NUM "oXuLinesNumber"
#define KEY_XU_CP_ON_ONE_VF "oXuCPon1Vf"
#define KEY_XU_RXQ_VF       "oXuRxqNumber"
#define KEY_OWDM_INIT_EN    "oXuOwdmInitEn"
#define KEY_OWDM_MEAS_METH  "oXuOwdmMeasMeth"
#define KEY_OWDM_NUM_SAMPS  "oXuOwdmNumSamps"
#define KEY_OWDM_FLTR_TYPE  "oXuOwdmFltrType"
#define KEY_OWDM_RSP_TO     "oXuOwdmRespTimeOut"
#define KEY_OWDM_MEAS_ST    "oXuOwdmMeasState"
#define KEY_OWDM_MEAS_ID    "oXuOwdmMeasId"
#define KEY_OWDM_EN         "oXuOwdmEnabled"
#define KEY_OWDM_PL_LENGTH  "oXuOwdmPlLength"

/* Config for IEEE 802.1Q CFM LBM/LBR */
#define KEY_LBM_ENABLE              "lbmEnable"
#define KEY_LBM_NUM_RETRANSMISSIONS "NumRetransmissions"
#define KEY_LBR_TIMEOUT             "lbrTimeout"
#define KEY_LBM_PERIODICITY         "lbmPeriodicity"

#define KEY_CC_PER_PORT_NUM "ccNum"
#define KEY_ANT_NUM         "antNum"
#define KEY_UL_ANT_NUM      "antNumUL"

#define KEY_ANT_ELM_TRX_NUM "antElmTRx"

#define KEY_MU_MIMO_UES_NUM "muMimoUEs"
#define KEY_DLLAYERS_PER_UE "DlLayersPerUe"
#define KEY_ULLAYERS_PER_UE "UlLayersPerUe"
#define KEY_FILE_DLBFWUE    "DlBfwUe"
#define KEY_FILE_ULBFWUE    "UlBfwUe"

#define KEY_FILE_O_XU_CFG   "oXuCfgFile"
#define KEY_NUM_SECMU       "numSecMu"
#define KEY_FILE_O_XU_BBU_CFG   "oXuBbuCfgFile"
#define KEY_FILE_SEC_NUM   "oXuSecNumFile"
#define KEY_O_XU_PCIE_BUS   "PciBusAddoXu"
#define KEY_O_XU_REM_MAC    "oXuRem"
#define KEY_DL_CP_BURST     "dlCpProcBurst"
#define KEY_XRAN_MLOG_DIS   "xranMlogDisable"

#define KEY_FILE_ULSRS      "antSrsC"

#define KEY_FILE_DLCSIRS    "csirsAntC"

#define KEY_TTI_PERIOD      "ttiPeriod"

#define KEY_MTU_SIZE         "MTUSize"
#define KEY_MAIN_CORE        "mainCore"
#define KEY_IO_CORE          "ioCore"
#define KEY_IO_WORKER        "ioWorker"
#define KEY_IO_WORKER_64_127 "ioWorker_64_127"
#define KEY_IO_SLEEP         "ioSleep"
#define KEY_SYSTEM_CORE      "systemCore"
#define KEY_IOVA_MODE        "iovaMode"
#define KEY_DPDK_MEM_SZ      "dpdkMemorySize"

#define KEY_INSTANCE_ID     "instanceId"

#define KEY_DU_MAC          "duMac"
#define KEY_RU_MAC          "ruMac"

#define KEY_FILE_NUMSLOTS   "numSlots"
#define KEY_FILE_AxC        "antC"
#define KEY_FILE_PRACH_AxC  "antPrachC"

#define KEY_FILE_SLOT_TX     "SlotNumTx"
#define KEY_FILE_SLOT_RX     "SlotNumRx"

#define KEY_PRACH_ENABLE     "rachEnable"

/*NPRACH parameters*/
#define KEY_NPRACH_FORMAT     "nprachformat"
#define KEY_NPRACH_PERIOD     "periodicity"
#define KEY_NPRACH_STARTTIME  "startTime"
#define KEY_NPRACH_SUB_OFFSET "suboffset"
#define KEY_NUM_SUBCARRIERS   "numSubCarriers"
#define KEY_NUM_REP           "nRep"

#define KEY_DROP_PACKETS_UP  "dropPacketsUp"
#define KEY_SRS_ENABLE     "srsEnable"
#define KEY_CSIRS_ENABLE   "csirsEnable"
#define KEY_CSI_PORTS      "nCSIports"

#define KEY_PUSCH_MASK_ENABLE "puschMaskEnable"
#define KEY_PUSCH_MASK_SLOT "puschMaskSlot"

#define KEY_PRACH_CFGIDX   "prachConfigIndex"
#define KEY_PRACH_CFGIDX_LTE   "prachConfigIndexLTE"
#define KEY_SRS_SYM_IDX     "srsSym"
#define KEY_SRS_SLOT        "srsSlot"
#define KEY_SRS_NDM_OFFSET  "srsNdmOffset"
#define KEY_SRS_NDM_TXDUR   "srsNdmTxDuration"

#define KEY_MAX_FRAME_ID   "maxFrameId"


#define KEY_IQ_SWAP        "iqswap"
#define KEY_HTONS_SWAP     "nebyteorderswap"
#define KEY_COMPRESSION    "compression"
#define KEY_COMP_TYPE      "compType"
#define KEY_PRACH_COMPMETH    "prachCompMethod"
#define KEY_PRACH_IQ    "prachiqWidth"

#define KEY_PUSCH_UP_MULTI_SECTION "PuschUpMultiSec"

#define KEY_BFW_NUM        "totalBFWeights"

#define KEY_TADV_CP_DL     "Tadv_cp_dl"
#define KEY_T2A_MIN_CP_DL  "T2a_min_cp_dl"
#define KEY_T2A_MAX_CP_DL  "T2a_max_cp_dl"
#define KEY_T2A_MIN_CP_UL  "T2a_min_cp_ul"
#define KEY_T2A_MAX_CP_UL  "T2a_max_cp_ul"
#define KEY_T2A_MIN_UP     "T2a_min_up"
#define KEY_T2A_MAX_UP     "T2a_max_up"
#define KEY_TA3_MIN        "Ta3_min"
#define KEY_TA3_MAX        "Ta3_max"
#define KEY_T1A_MIN_CP_DL  "T1a_min_cp_dl"
#define KEY_T1A_MAX_CP_DL  "T1a_max_cp_dl"
#define KEY_T1A_MIN_CP_UL  "T1a_min_cp_ul"
#define KEY_T1A_MAX_CP_UL  "T1a_max_cp_ul"
#define KEY_T1A_MIN_UP     "T1a_min_up"
#define KEY_T1A_MAX_UP     "T1a_max_up"
#define KEY_TA4_MIN        "Ta4_min"
#define KEY_TA4_MAX        "Ta4_max"

/*Time by which symbol should be transmitted in advanced in microseconds*/
#define KEY_TX_ADVANCED    "adv_tx_time" 

#define KEY_CP_ENABLE      "CPenable"
#define KEY_DEBUG_STOP     "debugStop"
#define KEY_DEBUG_STOP_CNT "debugStopCount"
#define KEY_BBDEV_MODE     "bbdevMode"
#define KEY_DYNA_SEC_ENA   "DynamicSectionEna"
#define EXT_TYPE           "extType"
#define KEY_ALPHA          "Gps_Alpha"
#define KEY_BETA           "Gps_Beta"

#define KEY_NPRBELEM_DL       "nPrbElemDl"
#define KEY_PRBELEM_DL        "PrbElemDl"
#define KEY_PRBELEM_DL_CC_M   "PrbElemDlCCMask"
#define KEY_PRBELEM_DL_ANT_M  "PrbElemDlAntCMask"
#define KEY_EXTBFW_DL         "ExtBfwDl"

#define KEY_NPRBELEM_UL       "nPrbElemUl"
#define KEY_PRBELEM_UL        "PrbElemUl"
#define KEY_PRBELEM_UL_CC_M   "PrbElemUlCCMask"
#define KEY_PRBELEM_UL_ANT_M  "PrbElemUlAntCMask"
#define KEY_EXTBFW_UL         "ExtBfwUl"
#define KEY_NPRBELEM_SRS       "nPrbElemSrs"
#define KEY_NPRBELEM_CSIRS     "nPrbElemCsirs"
#define KEY_PRBELEM_SRS        "PrbElemSrs"
#define KEY_PRBELEM_CSIRS      "PrbElemCsirs"
#define KEY_PRBELEM_SRS_CC_M   "PrbElemSrsCCMask"
#define KEY_PRBELEM_SRS_ANT_M  "PrbElemSrsAntCMask"
#define KEY_MAX_SEC_SYM        "max_sections_per_symbol"
#define KEY_MAX_SEC_SLOT       "max_sections_per_slot"
#define KEY_PRBMAP_BY_SYMB     "RunSlotPrbMapBySymbolEna"

#define KEY_DSS_ENABLE       "dssEnable"
#define KEY_DSS_PERIOD       "dssPeriod"
#define KEY_TECHNOLOGY       "technology"
typedef int (*fillConfigStruct_fn)(void* cbPram, const char *key, const char *value);

struct slot_cfg_to_pars {
    RuntimeConfig *config;
    int32_t direction;
    int32_t slot_idx;
};

/**
 * Set runtime configuration parameters to their defaults.
 *
 * @todo Initialize missing parameters.
 */
//static void init_config(RuntimeConfig* config)
//{
//    memset(config , 0, sizeof(RuntimeConfig));
//}

static int32_t
parseFileViaCb (char *filename, fillConfigStruct_fn cbFn, void* cbParm);

/** - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - **/

static void trim(char* input)
{
    uint32_t i;
    for (i = 0; i<strlen(input); i++)
        if (input[i] == ' ' || input[i] == '\n' || input[i] == '\t')
            input[i] = '\0';
}


static int fillConfigStruct(RuntimeConfig *config, const char *key, const char *value)
{
    static uint32_t section_idx_dl    = 0;
    static uint32_t section_idx_ul    = 0;
    static uint8_t  section_idx_srs   = 0;
    static uint8_t  section_idx_csirs = 0;
    static int8_t   configForMu       =-1;
    // static uint8_t  mu_idx          = 0;

    if (strcmp(key, KEY_APP_MODE) == 0){
        config->appMode = atoi(value);
    } else if (strcmp(key, KEY_XRAN_TECH) == 0) {
        config->xranTech = atoi(value);
    } else if (strcmp(key, KEY_XRAN_MODE) == 0) {
        config->xranCat = atoi(value);
    } else if (strcmp(key, KEY_CC_PER_PORT_NUM) == 0) {
        config->numCC= atoi(value);
    } else if (strcmp(key, KEY_MU_NUMBER) == 0) {
        configForMu = atoi(value);
        config->mu_number[config->numMu++] = configForMu;
    } else if (strcmp(key, KEY_UL_SCS) == 0) {
        config->perMu[configForMu].nbIotUlScs = atoi(value);
    } else if (strcmp(key, KEY_FREQ_OFFSET) == 0) {
        config->perMu[configForMu].freqOffset = atoi(value);
    } else if (strcmp(key, KEY_EAXC_OFFSET) == 0) {
        config->perMu[configForMu].eaxcOffset = atoi(value);
        printf("eAxcOffset = %d\n",config->perMu[configForMu].eaxcOffset);
    } else if (strcmp(key, KEY_NDLABSFREPOINTA) == 0) {
        config->nDLAbsFrePointA = atoi(value);
        printf("nDLAbsFrePointA: %d\n",config->nDLAbsFrePointA);
    } else if (strcmp(key, KEY_NULABSFREPOINTA) == 0) {
        config->nULAbsFrePointA = atoi(value);
        printf("nULAbsFrePointA: %d\n",config->nULAbsFrePointA);
    } else if (strcmp(key, KEY_NDLBANDWIDTH) == 0) {
        config->perMu[configForMu].nDLBandwidth = atoi(value);
        printf("nDLBandwidth: %d\n",config->perMu[configForMu].nDLBandwidth);
    } else if (strcmp(key, KEY_NULBANDWIDTH) == 0) {
        config->perMu[configForMu].nULBandwidth = atoi(value);
        printf("nULBandwidth: %d\n",config->perMu[configForMu].nULBandwidth);
    } else if (strcmp(key, KEY_NDLFFTSIZE) == 0) {
        config->perMu[configForMu].nDLFftSize = atoi(value);
        printf("nULFftSize: %d\n",config->perMu[configForMu].nULFftSize);
    } else if (strcmp(key, KEY_NULFFTSIZE) == 0) {
        config->perMu[configForMu].nULFftSize = atoi(value);
        printf("nULFftSize: %d\n",config->perMu[configForMu].nULFftSize);
    } else if (strcmp(key, KEY_NFRAMEDUPLEXTYPE) == 0) {
        config->nFrameDuplexType = atoi(value);
        printf("nFrameDuplexType: %d\n",config->nFrameDuplexType);
    } else if (strcmp(key, KEY_DU_PORT_ID_BITWIDTH) == 0) {
        config->DU_Port_ID_bitwidth = atoi(value);
        printf("DU_Port_ID_bitwidth: %d\n",config->DU_Port_ID_bitwidth);
    } else if (strcmp(key, KEY_BANDSECTOR_ID_BITWIDTH) == 0) {
        config->BandSector_ID_bitwidth = atoi(value);
        printf("BandSector_ID_bitwidth: %d\n",config->BandSector_ID_bitwidth);
    } else if (strcmp(key, KEY_CC_ID_BITWIDTH) == 0) {
        config->CC_ID_bitwidth = atoi(value);
        printf("CC_ID_bitwidth: %d\n",config->CC_ID_bitwidth);
    } else if (strcmp(key, KEY_RU_PORT_ID_BITWIDTH) == 0) {
        config->RU_Port_ID_bitwidth = atoi(value);
        printf("RU_Port_ID_bitwidth: %d\n",config->RU_Port_ID_bitwidth);
    } else if (strcmp(key, KEY_NTDDPERIOD) == 0) {
        config->nTddPeriod = atoi(value);
        printf("nTddPeriod: %d\n",config->nTddPeriod);
        if (config->nTddPeriod > XRAN_MAX_TDD_PERIODICITY)
        {
            printf("nTddPeriod is larger than max allowed, invalid!\n");
            config->nTddPeriod = XRAN_MAX_TDD_PERIODICITY;
        }
    } else if (strncmp(key, KEY_SSLOTCONFIG, strlen(KEY_SSLOTCONFIG)) == 0) {
        unsigned int slot_num = 0;
        int i = 0;
        sscanf(key,"sSlotConfig%u",&slot_num);
        if (slot_num >= config->nTddPeriod){
            printf("slot_num %d exceeds TddPeriod\n",slot_num);
        }
        else{
            sscanf(value, "%02hhx,%02hhx,%02hhx,%02hhx,%02hhx,%02hhx,%02hhx,%02hhx,%02hhx,%02hhx,%02hhx,%02hhx,%02hhx,%02hhx",
                                           (uint8_t *)&config->sSlotConfig[slot_num].nSymbolType[0],
                                           (uint8_t *)&config->sSlotConfig[slot_num].nSymbolType[1],
                                           (uint8_t *)&config->sSlotConfig[slot_num].nSymbolType[2],
                                           (uint8_t *)&config->sSlotConfig[slot_num].nSymbolType[3],
                                           (uint8_t *)&config->sSlotConfig[slot_num].nSymbolType[4],
                                           (uint8_t *)&config->sSlotConfig[slot_num].nSymbolType[5],
                                           (uint8_t *)&config->sSlotConfig[slot_num].nSymbolType[6],
                                           (uint8_t *)&config->sSlotConfig[slot_num].nSymbolType[7],
                                           (uint8_t *)&config->sSlotConfig[slot_num].nSymbolType[8],
                                           (uint8_t *)&config->sSlotConfig[slot_num].nSymbolType[9],
                                           (uint8_t *)&config->sSlotConfig[slot_num].nSymbolType[10],
                                           (uint8_t *)&config->sSlotConfig[slot_num].nSymbolType[11],
                                           (uint8_t *)&config->sSlotConfig[slot_num].nSymbolType[12],
                                           (uint8_t *)&config->sSlotConfig[slot_num].nSymbolType[13]);
            printf("sSlotConfig%d: ",slot_num);
            for (i = 0; i< 14; i++){
                printf("%d ",config->sSlotConfig[slot_num].nSymbolType[i]);
            }
            printf("\n");
        }
    } else if (strcmp(key, KEY_ANT_NUM) == 0) {
        config->numAxc = atoi(value);
    } else if (strcmp(key, KEY_UL_ANT_NUM) == 0) {
        config->numUlAxc = atoi(value);
    }else if (strcmp(key, KEY_ANT_ELM_TRX_NUM) == 0) {
        config->antElmTRx = atoi(value);
        printf("antElmTRx %d\n", config->antElmTRx);
    } else if (strcmp(key, KEY_MU_MIMO_UES_NUM) == 0) {
        config->muMimoUEs = atoi(value);
    } else if (strcmp(key, KEY_DLLAYERS_PER_UE) == 0) {
        config->DlLayersPerUe = atoi(value);
    } else if (strcmp(key, KEY_ULLAYERS_PER_UE) == 0) {
        config->UlLayersPerUe = atoi(value);
    } else if (strcmp(key, KEY_TTI_PERIOD) == 0) {
        config->ttiPeriod = atoi(value);
    } else if (strcmp(key, KEY_IQ_SWAP) == 0) {
        config->iqswap = atoi(value);
    } else if (strcmp(key, KEY_HTONS_SWAP) == 0) {
        config->nebyteorderswap = atoi(value);
    } else if (strcmp(key, KEY_COMPRESSION) == 0) {
        config->compression = atoi(value);
    } else if (strcmp(key, KEY_COMP_TYPE) == 0) {
        config->CompHdrType = atoi(value);
    } else if (strcmp(key, KEY_PRACH_COMPMETH) == 0) {
        config->prachCompMethod = atoi(value);
    } else if (strcmp(key, KEY_PRACH_IQ) == 0) {
        config->prachiqWidth = atoi(value);
    } else if (strcmp(key, KEY_MTU_SIZE) == 0) {
        config->mtu = atoi(value);
        printf("mtu %d\n", config->mtu);
    } else if (strcmp(key, KEY_PUSCH_UP_MULTI_SECTION) == 0) {
        config->nPuschUpMultiSectonSend= atoi(value);
        printf("PuschUpMultiSec %d\n", config->nPuschUpMultiSectonSend);
    } else if (strncmp(key, KEY_DU_MAC, strlen(KEY_DU_MAC)) == 0) {
        unsigned int vf_num = 0;
        sscanf(key,"duMac%02u",&vf_num);
        if (vf_num >= XRAN_VF_MAX) {
            printf("duMac%d exceeds max antenna supported\n",vf_num);
        } else {
            printf("duMac%d: %s\n",vf_num, value);
            sscanf(value, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", (uint8_t*)&config->o_du_addr[vf_num].addr_bytes[0],
                                               (uint8_t*)&config->o_du_addr[vf_num].addr_bytes[1],
                                               (uint8_t*)&config->o_du_addr[vf_num].addr_bytes[2],
                                               (uint8_t*)&config->o_du_addr[vf_num].addr_bytes[3],
                                               (uint8_t*)&config->o_du_addr[vf_num].addr_bytes[4],
                                               (uint8_t*)&config->o_du_addr[vf_num].addr_bytes[5]);

            printf("[vf %d]O-DU MAC address: %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
                vf_num,
                config->o_du_addr[vf_num].addr_bytes[0],
                config->o_du_addr[vf_num].addr_bytes[1],
                config->o_du_addr[vf_num].addr_bytes[2],
                config->o_du_addr[vf_num].addr_bytes[3],
                config->o_du_addr[vf_num].addr_bytes[4],
                config->o_du_addr[vf_num].addr_bytes[5]);
        }
    } else if (strncmp(key, KEY_RU_MAC, strlen(KEY_RU_MAC)) == 0) {
        unsigned int vf_num = 0;
        sscanf(key,"ruMac%02u",&vf_num);
        if (vf_num >= XRAN_VF_MAX) {
            printf("ruMac%d exceeds max antenna supported\n",vf_num);
        } else {
            printf("ruMac%d: %s\n",vf_num, value);

            sscanf(value, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", (uint8_t*)&config->o_ru_addr[vf_num].addr_bytes[0],
                                               (uint8_t*)&config->o_ru_addr[vf_num].addr_bytes[1],
                                               (uint8_t*)&config->o_ru_addr[vf_num].addr_bytes[2],
                                               (uint8_t*)&config->o_ru_addr[vf_num].addr_bytes[3],
                                               (uint8_t*)&config->o_ru_addr[vf_num].addr_bytes[4],
                                               (uint8_t*)&config->o_ru_addr[vf_num].addr_bytes[5]);

            printf("[vf %d]RU MAC address: %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
                vf_num,
                config->o_ru_addr[vf_num].addr_bytes[0],
                config->o_ru_addr[vf_num].addr_bytes[1],
                config->o_ru_addr[vf_num].addr_bytes[2],
                config->o_ru_addr[vf_num].addr_bytes[3],
                config->o_ru_addr[vf_num].addr_bytes[4],
                config->o_ru_addr[vf_num].addr_bytes[5]);
        }
    } else if (strcmp(key, KEY_FILE_NUMSLOTS) == 0) {
        config->numSlots = atoi(value);
        printf("numSlots: %d\n",config->numSlots);
    }else if (strncmp(key, KEY_FILE_AxC, strlen(KEY_FILE_AxC)) == 0) {
        unsigned int ant_num = 0;
        sscanf(key,"antC%02u",&ant_num);
        if (ant_num >= MAX_ANT_CARRIER_SUPPORTED) {
            printf("antC%d exceeds max antenna supported\n",ant_num);
        } else {
            int len = strlen(value);
            if(len > 512) len = 511;
            strncpy(&config->ant_perMu[configForMu].ant_file[ant_num][0], value, len);
            printf("[%d] antC%d: %s\n", configForMu, ant_num, config->ant_perMu[configForMu].ant_file[ant_num]);
        }
    } else if (strncmp(key, KEY_FILE_DLBFWUE, strlen(KEY_FILE_DLBFWUE)) == 0) {
        unsigned int ue_num = 0;
        sscanf(key,"DlBfwUe%02u",&ue_num);
        if (ue_num >= MAX_ANT_CARRIER_SUPPORTED) {
            printf("DlBfwUe%d exceeds max streams supported\n",ue_num);
        } else {
            int len = strlen(value);
            if(len > 512) len = 511;
            strncpy(&config->ant_perMu[configForMu].dl_bfw_file[ue_num][0], value, len);
            printf("DlBfwUe%d: %s\n",ue_num, config->ant_perMu[configForMu].dl_bfw_file[ue_num]);
        }
    }else if (strncmp(key, KEY_FILE_ULBFWUE, strlen(KEY_FILE_ULBFWUE)) == 0) {
        unsigned int ue_num = 0;
        sscanf(key,"UlBfwUe%02u",&ue_num);
        if (ue_num >= MAX_ANT_CARRIER_SUPPORTED) {
            printf("UlBfwUe%d exceeds max streams supported\n",ue_num);
        } else {
            int len = strlen(value);
            if(len > 512) len = 511;
            strncpy(&config->ant_perMu[configForMu].ul_bfw_file[ue_num][0], value, len);
            printf("UlBfwUe%d: %s\n",ue_num, config->ant_perMu[configForMu].ul_bfw_file[ue_num]);
        }
    } else if (strncmp(key, KEY_FILE_ULSRS, strlen(KEY_FILE_ULSRS)) == 0) {
        unsigned int srs_ant = 0;
        sscanf(key,"antSrsC%03u",&srs_ant);
        if (srs_ant >= MAX_ANT_CARRIER_SUPPORTED_CAT_B) {
            printf("antSrsC%d exceeds max ant elemnets supported [%d]\n", srs_ant, MAX_ANT_CARRIER_SUPPORTED_CAT_B);
        } else {
            int len = strlen(value);
            if(len > 512) len = 511;
            strncpy(&config->ant_perMu[configForMu].ul_srs_file[srs_ant][0], value, len);
            printf("antSrsC%d: %s\n",srs_ant, config->ant_perMu[configForMu].ul_srs_file[srs_ant]);
        }
    } else if (strncmp(key, KEY_FILE_DLCSIRS, strlen(KEY_FILE_DLCSIRS)) == 0) {
        unsigned int csirs_ant = 0;
        sscanf(key,"csirsAntC%02u",&csirs_ant);
        if (csirs_ant >= MAX_CSIRS_PORTS_SUPPORTED) {
            printf("csirsAntC%d exceeds max ports supported [%d]\n", csirs_ant, MAX_CSIRS_PORTS_SUPPORTED);
        } else {
            int len = strlen(value);
            if(len > 512) len = 511;
            strncpy(&config->ant_perMu[configForMu].dl_csirs_file[csirs_ant][0], value, len);
            printf("csirsAntC%d: %s\n",csirs_ant, config->ant_perMu[configForMu].dl_csirs_file[csirs_ant]);
        }
    } else if (strncmp(key, KEY_FILE_SLOT_TX, strlen(KEY_FILE_SLOT_TX)) == 0) {
        unsigned int slot_num = 0;
        unsigned int direction = XRAN_DIR_DL;
        sscanf(key,"SlotNumTx%02u",&slot_num);
        if (slot_num >= XRAN_N_FE_BUF_LEN) {
            printf("SlotNumTx%d exceeds max slots supported\n",slot_num);
        } else {
            int len = strlen(value);
            if(len > 512) len = 511;
            config->SlotNum_fileEnabled = 1;
            strncpy(&config->SlotNum_file[direction][slot_num][0], value, len);
            printf("SlotNumTx%d: %s\n",slot_num, config->SlotNum_file[direction][slot_num]);
        }
    }else if (strncmp(key, KEY_FILE_SLOT_RX, strlen(KEY_FILE_SLOT_RX)) == 0) {
        unsigned int slot_num = 0;
        unsigned int direction = XRAN_DIR_UL;
        sscanf(key,"SlotNumRx%02u",&slot_num);
        if (slot_num >= XRAN_N_FE_BUF_LEN) {
            printf("SlotNumRx%d exceeds max slots supported\n",slot_num);
        } else {
            int len = strlen(value);
            if(len > 512) len = 511;
            config->SlotNum_fileEnabled = 1;
            strncpy(&config->SlotNum_file[direction][slot_num][0], value, len);
            printf("SlotNumRx%d: %s\n",slot_num, config->SlotNum_file[direction][slot_num]);
        }
    } else if (strcmp(key, KEY_PRACH_ENABLE) == 0) {
        config->perMu[configForMu].prachEnable = atoi(value);
        printf("Prach enable: %d\n",config->perMu[configForMu].prachEnable);
    } else if (strcmp(key, KEY_NPRACH_FORMAT) == 0) {
        config->nprachformat = atoi(value);
        printf("config->nprachformat: %d\n",config->nprachformat);
    } else if (strcmp(key, KEY_NPRACH_PERIOD) == 0) {
        config->periodicity = atoi(value);
        printf("config->periodicity: %d\n",config->periodicity);
    } else if (strcmp(key, KEY_NPRACH_STARTTIME) == 0) {
        config->startTime = atoi(value);
        printf("config->startTime : %d\n",config->startTime);
    } else if (strcmp(key, KEY_NPRACH_SUB_OFFSET) == 0) {
        config->suboffset = atoi(value);
        printf("config->suboffset : %d\n",config->suboffset);
    } else if (strcmp(key, KEY_NUM_SUBCARRIERS) == 0) {
        config->numSubCarriers = atoi(value);
        printf("config->numSubCarriers : %d\n",config->numSubCarriers);
    } else if (strcmp(key, KEY_NUM_REP) == 0) {
        config->nRep = atoi(value);
        printf("config->nRep : %d\n",config->nRep);
    } else if (strcmp(key, KEY_MAX_FRAME_ID) == 0) {
        config->maxFrameId = atoi(value);
        printf("maxFrameId: %d\n",config->maxFrameId);
    } else if (strcmp(key, KEY_DROP_PACKETS_UP) == 0) {
        config->DropPacketsUp = atoi(value);
        printf("dropPacketsUp enable: %d\n",config->DropPacketsUp);
    } else if (strcmp(key, KEY_SRS_ENABLE) == 0) {
        config->enableSrs = atoi(value);
        printf("Srs enable: %d\n",config->enableSrs);
    } else if (strcmp(key, KEY_CSIRS_ENABLE) == 0) {
        config->csirsEnable = atoi(value);
        printf("CSI-RS enable: %d\n",config->csirsEnable);
    } else if (strcmp(key, KEY_CSI_PORTS) == 0) {
        config->nCsiPorts = atoi(value);
        printf("CSI-RS Ports: %d\n",config->nCsiPorts);
    } else if (strcmp(key, KEY_SRS_SYM_IDX) == 0) {
        config->srsSymMask = atoi(value);
        printf("Srs symbol [0-13]: %d\n",config->srsSymMask);
    } else if (strcmp(key, KEY_SRS_SLOT) == 0) {
        config->srsSlot = atoi(value);
        printf("Srs slot: %d\n",config->srsSlot);
    } else if (strcmp(key, KEY_SRS_NDM_OFFSET) == 0) {
        config->srsNdmOffset = atoi(value);
        printf("Srs NDM Offset: %d\n",config->srsNdmOffset);
    } else if (strcmp(key, KEY_SRS_NDM_TXDUR) == 0) {
        config->srsNdmTxDuration = atoi(value);
        printf("Srs NDM TX duration: %d\n",config->srsNdmTxDuration);
    } else if (strcmp(key, KEY_PUSCH_MASK_ENABLE) == 0) {
        config->puschMaskEnable = atoi(value);
        printf("PUSCH mask enable: %d\n",config->puschMaskEnable);
    } else if (strcmp(key, KEY_PUSCH_MASK_SLOT) == 0) {
        config->puschMaskSlot = atoi(value);
        printf("PUSCH mask enable: %d\n",config->puschMaskSlot);
    } else if (strcmp(key, KEY_PRACH_CFGIDX) == 0) {
        if(config->perMu[configForMu].prachEnable == 1)
            config->perMu[configForMu].prachConfigIndex = atoi(value);
        printf("Prach config index: %d\n",config->perMu[configForMu].prachConfigIndex);
    } else if (strcmp(key, KEY_PRACH_CFGIDX_LTE) == 0) {
        if(config->perMu[configForMu].prachEnable == 1)
            config->perMu[configForMu].prachConfigIndexLTE = atoi(value);
        printf("Prach config index LTE for DSS: %d\n",config->perMu[configForMu].prachConfigIndexLTE);
    } else if (strncmp(key, KEY_FILE_PRACH_AxC, strlen(KEY_FILE_PRACH_AxC)) == 0) {
        unsigned int ant_num = 0;
        sscanf(key,"antPrachC%02u",&ant_num);
        if (ant_num >= MAX_ANT_CARRIER_SUPPORTED)
        {
            printf("antC%d exceeds max antenna supported\n",ant_num);
        }
        else{
            int len = strlen(value);
            if(len > 512) len = 511;
            strncpy(&config->ant_perMu[configForMu].prach_file[ant_num][0], value, len);
            printf("antPrachC%d: %s\n",ant_num, config->ant_perMu[configForMu].prach_file[ant_num]);
        }
    } else if (strcmp(key, KEY_BFW_NUM) == 0) {
        config->totalBfWeights = atoi(value);
        printf("%s : %d\n",KEY_BFW_NUM, config->totalBfWeights);
        /* timing */
    } else if (strcmp(key, KEY_TADV_CP_DL ) == 0) {
        config->perMu[configForMu].Tadv_cp_dl = atoi(value);
        printf("Tadv_cp_dl[%d]: %d\n",configForMu, config->perMu[configForMu].Tadv_cp_dl);
    } else if (strcmp(key, KEY_T2A_MIN_CP_DL ) == 0) {
        config->perMu[configForMu].T2a_min_cp_dl = atoi(value);
        printf("T2a_min_cp_dl: %d\n",config->perMu[configForMu].T2a_min_cp_dl);
    } else if (strcmp(key, KEY_T2A_MAX_CP_DL ) == 0) {
        config->perMu[configForMu].T2a_max_cp_dl = atoi(value);
        printf("T2a_max_cp_dl: %d\n",config->perMu[configForMu].T2a_max_cp_dl);
    } else if (strcmp(key, KEY_T2A_MIN_CP_UL ) == 0) {
        config->perMu[configForMu].T2a_min_cp_ul = atoi(value);
        printf("T2a_min_cp_ul: %d\n",config->perMu[configForMu].T2a_min_cp_ul);
    } else if (strcmp(key, KEY_T2A_MAX_CP_UL ) == 0) {
        config->perMu[configForMu].T2a_max_cp_ul = atoi(value);
        printf("T2a_max_cp_ul: %d\n",config->perMu[configForMu].T2a_max_cp_ul);
    } else if (strcmp(key, KEY_T2A_MIN_UP ) == 0) {
        config->perMu[configForMu].T2a_min_up = atoi(value);
        printf("T2a_min_up: %d\n",config->perMu[configForMu].T2a_min_up);
    } else if (strcmp(key, KEY_T2A_MAX_UP ) == 0) {
        config->perMu[configForMu].T2a_max_up = atoi(value);
        printf("T2a_max_up: %d\n",config->perMu[configForMu].T2a_max_up);
    } else if (strcmp(key, KEY_TA3_MIN ) == 0) {
        config->perMu[configForMu].Ta3_min = atoi(value);
        printf("Ta3_min: %d\n",config->perMu[configForMu].Ta3_min);
    } else if (strcmp(key, KEY_TA3_MAX ) == 0) {
        config->perMu[configForMu].Ta3_max = atoi(value);
        printf("Ta3_max: %d\n",config->perMu[configForMu].Ta3_max);
    } else if (strcmp(key, KEY_T1A_MIN_CP_DL ) == 0) {
        config->perMu[configForMu].T1a_min_cp_dl = atoi(value);
        printf("T1a_min_cp_dl: %d\n",config->perMu[configForMu].T1a_min_cp_dl);
    } else if (strcmp(key, KEY_T1A_MAX_CP_DL ) == 0) {
        config->perMu[configForMu].T1a_max_cp_dl = atoi(value);
        printf("T1a_max_cp_dl: %d\n",config->perMu[configForMu].T1a_max_cp_dl);
    } else if (strcmp(key, KEY_T1A_MIN_CP_UL ) == 0) {
        config->perMu[configForMu].T1a_min_cp_ul = atoi(value);
        printf("T1a_min_cp_ul: %d\n",config->perMu[configForMu].T1a_min_cp_ul);
    } else if (strcmp(key, KEY_T1A_MAX_CP_UL ) == 0) {
        config->perMu[configForMu].T1a_max_cp_ul = atoi(value);
        printf("T1a_max_cp_ul: %d\n",config->perMu[configForMu].T1a_max_cp_ul);
    } else if (strcmp(key, KEY_T1A_MIN_UP ) == 0) {
        config->perMu[configForMu].T1a_min_up = atoi(value);
        printf("T1a_min_up: %d\n",config->perMu[configForMu].T1a_min_up);
    } else if (strcmp(key, KEY_T1A_MAX_UP ) == 0) {
        config->perMu[configForMu].T1a_max_up = atoi(value);
        printf("T1a_max_up: %d\n",config->perMu[configForMu].T1a_max_up);
    } else if (strcmp(key, KEY_TA4_MIN ) == 0) {
        config->perMu[configForMu].Ta4_min = atoi(value);
        printf("Ta4_min: %d\n",config->perMu[configForMu].Ta4_min);
    } else if (strcmp(key, KEY_TA4_MAX ) == 0) {
        config->perMu[configForMu].Ta4_max = atoi(value);
        printf("Ta4_max: %d\n",config->perMu[configForMu].Ta4_max);
        /* end of timing */
    }  else if (strcmp(key, KEY_TX_ADVANCED) == 0) {
        config->perMu[configForMu].adv_tx_time = atoi(value);
        printf("adv_tx_time: %d\n",config->perMu[configForMu].adv_tx_time);
        /* end of timing */
    } else if (strcmp(key, KEY_CP_ENABLE ) == 0) {
        config->enableCP = atoi(value);
        printf("CPenable: %d\n",config->enableCP);
    } else if (strcmp(key, KEY_DEBUG_STOP ) == 0) {
        config->debugStop = atoi(value);
        printf("debugStop: %d\n",config->debugStop);
    } else if (strcmp(key, KEY_DEBUG_STOP_CNT) == 0) {
        config->debugStopCount = atoi(value);
        printf("debugStopCount: %d\n",config->debugStopCount);
    } else if (strcmp(key, KEY_BBDEV_MODE) == 0) {
        config->bbdevMode = atoi(value);
        printf("bbdevMode: %d\n",config->bbdevMode);
    } else if (strcmp(key, KEY_DYNA_SEC_ENA) == 0) {
        config->DynamicSectionEna = atoi(value);
        printf("DynamicSectionEna: %d\n",config->DynamicSectionEna);
    } else if (strcmp(key, EXT_TYPE) == 0) {
        config->extType = atoi(value);
        printf("ExtType: %d\n",config->extType);
    } else if (strcmp(key, KEY_ALPHA) == 0) {
        config->GPS_Alpha = atoi(value);
        printf("GPS_Alpha: %d\n",config->GPS_Alpha);
    } else if (strcmp(key, KEY_BETA) == 0) {
        config->GPS_Beta = atoi(value);
        printf("GPS_Beta: %d\n",config->GPS_Beta);
    } else if (strcmp(key, KEY_MAX_SEC_SYM ) == 0) {
        config->max_sections_per_symbol = atoi(value);
        printf("max_sections_per_symbol: %d\n",config->max_sections_per_symbol);
    } else if (strcmp(key, KEY_MAX_SEC_SLOT ) == 0) {
        config->max_sections_per_slot = atoi(value);
        printf("max_sections_per_slot: %d\n",config->max_sections_per_slot);
    } else if (strcmp(key, KEY_NPRBELEM_UL ) == 0) {
        config->p_PrbMapUl[configForMu]->nPrbElm = atoi(value);
        if (config->p_PrbMapUl[configForMu]->nPrbElm > XRAN_MAX_SECTIONS_PER_SLOT)
        {
            printf("nTddPeriod is larger than max allowed, invalid!\n");
            config->p_PrbMapUl[configForMu]->nPrbElm = XRAN_MAX_SECTIONS_PER_SLOT;
        }
        printf("nPrbElemUl: %d\n",config->p_PrbMapUl[configForMu]->nPrbElm);
    } else if (strncmp(key, KEY_PRBELEM_UL, strlen(KEY_PRBELEM_UL)) == 0) {
        sscanf(key,"PrbElemUl%u",&section_idx_ul);
        if (section_idx_ul >= config->p_PrbMapUl[configForMu]->nPrbElm){
            printf("section_idx %d exceeds nPrbElemUl\n",section_idx_ul);
        }
        else{
            struct xran_prb_elm *pPrbElem = &config->p_PrbMapUl[configForMu]->prbMap[section_idx_ul];
            sscanf(value, "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd",
                (int16_t*)&pPrbElem->nRBStart,
                (int16_t*)&pPrbElem->nRBSize,
                (int16_t*)&pPrbElem->nStartSymb,
                (int16_t*)&pPrbElem->numSymb,
                (int16_t*)&pPrbElem->nBeamIndex,
                (int16_t*)&pPrbElem->bf_weight_update,
                (int16_t*)&pPrbElem->compMethod,
                (int16_t*)&pPrbElem->iqWidth,
                (int16_t*)&pPrbElem->BeamFormingType);
            printf("nPrbElemUl%d: ",section_idx_ul);
            printf("nRBStart %d,nRBSize %d,nStartSymb %d,numSymb %d,nBeamIndex %d, bf_weight_update %d compMethod %d, iqWidth %d BeamFormingType %d\n",
                pPrbElem->nRBStart,pPrbElem->nRBSize,pPrbElem->nStartSymb,pPrbElem->numSymb,pPrbElem->nBeamIndex, pPrbElem->bf_weight_update, pPrbElem->compMethod, pPrbElem->iqWidth, pPrbElem->BeamFormingType);
        }
    } else if(strncmp(key, KEY_EXTBFW_UL, strlen(KEY_EXTBFW_UL)) == 0) {
        sscanf(key, "ExtBfwUl%u", &section_idx_ul);
        if(section_idx_ul >= config->p_PrbMapUl[configForMu]->nPrbElm) {
            printf("section_idx %d of bfw exceeds nPrbElemUl\n",section_idx_ul);
        }
        else{
            struct xran_prb_elm *pPrbElem = &config->p_PrbMapUl[configForMu]->prbMap[section_idx_ul];
            sscanf(value, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
                (uint8_t*)&pPrbElem->bf_weight.numBundPrb,
                (uint8_t*)&pPrbElem->bf_weight.numSetBFWs,
                (uint8_t*)&pPrbElem->bf_weight.RAD,
                (uint8_t*)&pPrbElem->bf_weight.disableBFWs,
                (uint8_t*)&pPrbElem->bf_weight.bfwIqWidth,
                (uint8_t*)&pPrbElem->bf_weight.bfwCompMeth,
                (uint8_t*)&pPrbElem->bf_weight.extType);
            printf(KEY_EXTBFW_UL"%d: ", section_idx_ul);
            printf("numBundPrb %d, numSetBFW %d, RAD %d, disableBFW %d, bfwIqWidth %d, bfwCompMeth %d, extType %d\n",
                pPrbElem->bf_weight.numBundPrb, pPrbElem->bf_weight.numSetBFWs, pPrbElem->bf_weight.RAD, pPrbElem->bf_weight.disableBFWs, pPrbElem->bf_weight.bfwIqWidth, pPrbElem->bf_weight.bfwCompMeth, pPrbElem->bf_weight.extType);
        }
    } else if (strcmp(key, KEY_NPRBELEM_DL ) == 0) {
        config->p_PrbMapDl[configForMu]->nPrbElm = atoi(value);
        if (config->p_PrbMapDl[configForMu]->nPrbElm > XRAN_MAX_SECTIONS_PER_SLOT)
        {
            printf("nPrbElm is larger than max allowed, invalid!\n");
            config->p_PrbMapDl[configForMu]->nPrbElm = XRAN_MAX_SECTIONS_PER_SLOT;
        }
        printf("nPrbElemDl: %d\n",config->p_PrbMapDl[configForMu]->nPrbElm);
    } else if (strncmp(key, KEY_PRBELEM_DL, strlen(KEY_PRBELEM_DL)) == 0) {
        sscanf(key,"PrbElemDl%u",&section_idx_dl);
        if (section_idx_dl >= config->p_PrbMapDl[configForMu]->nPrbElm){
            printf("section_idx %d exceeds nPrbElemDl\n",section_idx_dl);
        }
        else{
            struct xran_prb_elm *pPrbElem = &config->p_PrbMapDl[configForMu]->prbMap[section_idx_dl];
            sscanf(value, "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hx",
                (int16_t*)&pPrbElem->nRBStart,
                (int16_t*)&pPrbElem->nRBSize,
                (int16_t*)&pPrbElem->nStartSymb,
                (int16_t*)&pPrbElem->numSymb,
                (int16_t*)&pPrbElem->nBeamIndex,
                (int16_t*)&pPrbElem->bf_weight_update,
                (int16_t*)&pPrbElem->compMethod,
                (int16_t*)&pPrbElem->iqWidth,
                (int16_t*)&pPrbElem->BeamFormingType,
                (int16_t*)&pPrbElem->ScaleFactor,
                (uint16_t*)&pPrbElem->reMask);
            printf("nPrbElemDl%d: ",section_idx_dl);
            printf("nRBStart %d,nRBSize %d,nStartSymb %d,numSymb %d,nBeamIndex %d, bf_weight_update %d compMethod %d, iqWidth %d BeamFormingType %d ScaleFactor %d reMask %d\n",
                pPrbElem->nRBStart,pPrbElem->nRBSize,pPrbElem->nStartSymb,pPrbElem->numSymb,pPrbElem->nBeamIndex, pPrbElem->bf_weight_update, pPrbElem->compMethod, pPrbElem->iqWidth, pPrbElem->BeamFormingType, pPrbElem->ScaleFactor, pPrbElem->reMask);
        }
    } else if(strncmp(key, KEY_EXTBFW_DL, strlen(KEY_EXTBFW_DL)) == 0) {
        sscanf(key, "ExtBfwDl%u", &section_idx_dl);
        if(section_idx_dl >= config->p_PrbMapDl[configForMu]->nPrbElm) {
            printf("section_idx %d of bfw exceeds nPrbElemUl\n",section_idx_dl);
        }
        else{
            struct xran_prb_elm *pPrbElem = &config->p_PrbMapDl[configForMu]->prbMap[section_idx_dl];
            sscanf(value, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
                (uint8_t*)&pPrbElem->bf_weight.numBundPrb,
                (uint8_t*)&pPrbElem->bf_weight.numSetBFWs,
                (uint8_t*)&pPrbElem->bf_weight.RAD,
                (uint8_t*)&pPrbElem->bf_weight.disableBFWs,
                (uint8_t*)&pPrbElem->bf_weight.bfwIqWidth,
                (uint8_t*)&pPrbElem->bf_weight.bfwCompMeth,
                (uint8_t*)&pPrbElem->bf_weight.extType);
            printf(KEY_EXTBFW_DL"%d: ", section_idx_dl);
            printf("numBundPrb %d, numSetBFW %d, RAD %d, disableBFW %d, bfwIqWidth %d, bfwCompMeth %d, extType %d\n",
                pPrbElem->bf_weight.numBundPrb, pPrbElem->bf_weight.numSetBFWs, pPrbElem->bf_weight.RAD, pPrbElem->bf_weight.disableBFWs, pPrbElem->bf_weight.bfwIqWidth, pPrbElem->bf_weight.bfwCompMeth, pPrbElem->bf_weight.extType);
        }
    } else if (strcmp(key, KEY_NPRBELEM_SRS ) == 0) {
        config->p_PrbMapSrs->nPrbElm = atoi(value);
        if (config->p_PrbMapSrs->nPrbElm > XRAN_MAX_SECTIONS_PER_SLOT)
        {
            printf("nPrbElm is larger than max allowed, invalid!\n");
            config->p_PrbMapSrs->nPrbElm = XRAN_MAX_SECTIONS_PER_SLOT;
        }
        printf("nPrbElemSrs: %d\n",config->p_PrbMapSrs->nPrbElm);
    } else if (strncmp(key, KEY_PRBELEM_SRS, strlen(KEY_PRBELEM_SRS)) == 0) {
        sscanf(key,"PrbElemSrs%hhu",&section_idx_srs);
        if (section_idx_srs >= config->p_PrbMapSrs->nPrbElm) {
            printf("section_idx %d exceeds nPrbElemSrs\n",section_idx_srs);
        }else {
            struct xran_prb_elm *pPrbElem = &config->p_PrbMapSrs->prbMap[section_idx_srs];
            sscanf(value, "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd",
                (int16_t*)&pPrbElem->nRBStart,
                (int16_t*)&pPrbElem->nRBSize,
                (int16_t*)&pPrbElem->nStartSymb,
                (int16_t*)&pPrbElem->numSymb,
                (int16_t*)&pPrbElem->nBeamIndex,
                (int16_t*)&pPrbElem->bf_weight_update,
                (int16_t*)&pPrbElem->compMethod,
                (int16_t*)&pPrbElem->iqWidth,
                (int16_t*)&pPrbElem->BeamFormingType);
            printf("nPrbElemSrs%d: ",section_idx_srs);
            printf("nRBStart %d,nRBSize %d,nStartSymb %d,numSymb %d,nBeamIndex %d, bf_weight_update %d compMethod %d, iqWidth %d BeamFormingType %d\n",
                pPrbElem->nRBStart,pPrbElem->nRBSize,pPrbElem->nStartSymb,pPrbElem->numSymb,pPrbElem->nBeamIndex, pPrbElem->bf_weight_update, pPrbElem->compMethod, pPrbElem->iqWidth, pPrbElem->BeamFormingType);
        }
    } else if (strcmp(key, KEY_NPRBELEM_CSIRS ) == 0) {
        config->p_PrbMapCsiRs->nPrbElm = atoi(value);
        if (config->p_PrbMapCsiRs->nPrbElm > XRAN_MAX_SECTIONS_PER_SLOT)
        {
            printf("nPrbElm is larger than max allowed, invalid!\n");
            config->p_PrbMapCsiRs->nPrbElm = XRAN_MAX_SECTIONS_PER_SLOT;
        }
        printf("nPrbElemCsiRs: %d\n",config->p_PrbMapCsiRs->nPrbElm);
    } else if (strncmp(key, KEY_PRBELEM_CSIRS, strlen(KEY_PRBELEM_CSIRS)) == 0) {
        sscanf(key,"PrbElemCsirs%hhu",&section_idx_csirs);
        if (section_idx_csirs >= config->p_PrbMapCsiRs->nPrbElm) {
            printf("section_idx %d exceeds nPrbElemCsirs\n",section_idx_csirs);
        }else {
            struct xran_prb_elm *pPrbElem = &config->p_PrbMapCsiRs->prbMap[section_idx_csirs];
            sscanf(value, "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd",
                (int16_t*)&pPrbElem->nRBStart,
                (int16_t*)&pPrbElem->nRBSize,
                (int16_t*)&pPrbElem->nStartSymb,
                (int16_t*)&pPrbElem->numSymb,
                (int16_t*)&pPrbElem->nBeamIndex,
                (int16_t*)&pPrbElem->bf_weight_update,
                (int16_t*)&pPrbElem->compMethod,
                (int16_t*)&pPrbElem->iqWidth,
                (int16_t*)&pPrbElem->BeamFormingType);
            printf("nPrbElemCsirs%d: ",section_idx_csirs);
            printf("nRBStart %d,nRBSize %d,nStartSymb %d,numSymb %d,nBeamIndex %d, bf_weight_update %d compMethod %d, iqWidth %d BeamFormingType %d\n",
                pPrbElem->nRBStart,pPrbElem->nRBSize,pPrbElem->nStartSymb,pPrbElem->numSymb,pPrbElem->nBeamIndex, pPrbElem->bf_weight_update, pPrbElem->compMethod, pPrbElem->iqWidth, pPrbElem->BeamFormingType);
        }
    } else if (strcmp(key, KEY_PRBMAP_BY_SYMB ) == 0) {
        config->RunSlotPrbMapBySymbolEnable = atoi(value);
        printf("RunSlotPrbMapBySymbolEnable: %d\n",config->RunSlotPrbMapBySymbolEnable);
    } else if (strcmp(key, KEY_DSS_ENABLE ) == 0) {
        config->dssEnable =  atoi(value);
        printf("dssEnable: %d\n",config->dssEnable);
    } else if (strcmp(key, KEY_DSS_PERIOD ) == 0) {
        config->dssPeriod =  atoi(value);
        printf("dssPeriod: %d\n",config->dssPeriod);
    } else if (strcmp(key, KEY_TECHNOLOGY ) == 0) {
        int i = 0;
        sscanf(value, "%02hhx,%02hhx,%02hhx,%02hhx,%02hhx,%02hhx,%02hhx,%02hhx,%02hhx,%02hhx,%02hhx,%02hhx,%02hhx,%02hhx,%02hhx",
                                                    (uint8_t *)&config->technology[0],
                                                    (uint8_t *)&config->technology[1],
                                                    (uint8_t *)&config->technology[2],
                                                    (uint8_t *)&config->technology[3],
                                                    (uint8_t *)&config->technology[4],
                                                    (uint8_t *)&config->technology[5],
                                                    (uint8_t *)&config->technology[6],
                                                    (uint8_t *)&config->technology[7],
                                                    (uint8_t *)&config->technology[8],
                                                    (uint8_t *)&config->technology[9],
                                                    (uint8_t *)&config->technology[10],
                                                    (uint8_t *)&config->technology[11],
                                                    (uint8_t *)&config->technology[12],
                                                    (uint8_t *)&config->technology[13],
                                                    (uint8_t *)&config->technology[14]);
        printf("technology:");
        for( i=0; i<config->dssPeriod; i++) {
           printf("%d ",config->technology[i]);
        }
        printf("\n");
    } else {
        printf("Unsupported configuration key [%s]\n", key);
        return -1;
    }
    return 0;
}

static int
fillUsecaseStruct(UsecaseConfig *config, const char *key, const char *value)
{
    static int curr_oxu=0;
    if (strcmp(key, KEY_APP_MODE) == 0){
        config->appMode = atoi(value);
        printf("appMode %d \n", config->appMode);
    } else if (strcmp(key, KEY_XU_NUM) == 0){
        config->oXuNum = atoi(value);
        printf("oXuNum %d \n", config->oXuNum);
    } else if (strcmp(key, KEY_XU_ETH_LINK_SPD) == 0){
        config->EthLinkSpeed = atoi(value);
        printf("EthLinkSpeed %d \n", config->EthLinkSpeed);
    } else if (strcmp(key, KEY_XU_ETH_LINE_NUM) == 0){
        config->EthLinesNumber = atoi(value);
        printf("EthLinesNumber %d \n", config->EthLinesNumber);
    } else if (strcmp(key, KEY_XU_RXQ_VF) == 0){
        config->num_rxq = atoi(value);
        printf("oXuRxqNumber %d \n", config->num_rxq);
    } else if (strcmp(key, KEY_XU_CP_ON_ONE_VF) == 0) {
        config->one_vf_cu_plane = atoi(value);
        printf("oXuCPon1Vf %d \n", config->one_vf_cu_plane);
        /*O-RAN.WG4.MP.0-v07.00.00 Section - 4.3
         one_vf_cu_plane = 0 is not supported. */
        if(config->one_vf_cu_plane == 0){
            printf("\n%s = 0 is not supported\n",KEY_XU_CP_ON_ONE_VF);
            return -1;
        }
    } else if (strcmp(key, KEY_IO_SLEEP) == 0) {
        config->io_sleep = atoi(value);
        printf("io_sleep %d \n", config->io_sleep);
    } else if (strcmp(key, KEY_IO_CORE) == 0) {
        config->io_core = atoi(value);
        printf("io_core %d [core id]\n", config->io_core);
    } else if (strcmp(key, KEY_MAIN_CORE) == 0) {
        config->main_core = atoi(value);
        printf("main_core %d [core id]\n", config->main_core);
    } else if (strcmp(key, KEY_IO_WORKER) == 0) {
        config->io_worker = strtoull(value, NULL, 0);
        printf("io_worker 0x%lx [mask]\n", config->io_worker);
    } else if (strcmp(key, KEY_IO_WORKER_64_127) == 0) {
        config->io_worker_64_127 = strtoull(value, NULL, 0);
        printf("io_worker_64_127 0x%lx [mask]\n", config->io_worker_64_127);
    } else if (strcmp(key, KEY_SYSTEM_CORE) == 0) {
        config->system_core = atoi(value);
        printf("system core %d [core id]\n", config->system_core);
    } else if (strcmp(key, KEY_IOVA_MODE) == 0) {
        config->iova_mode = atoi(value);
        printf("iova_mode %d\n", config->iova_mode);
    } else if (strcmp(key, KEY_DPDK_MEM_SZ) == 0) {
        config->dpdk_mem_sz = atoi(value);
        printf("dpdk_mem_sz %d\n", config->dpdk_mem_sz);
    } else if (strcmp(key, KEY_INSTANCE_ID) == 0) {
        config->instance_id = atoi(value);
        printf("instance_id %d\n", config->instance_id);
    } else if (strncmp(key, KEY_FILE_O_XU_CFG, strlen(KEY_FILE_O_XU_CFG)) == 0) {
        unsigned int o_xu_id = 0;
        sscanf(key,"oXuCfgFile%02u",&o_xu_id);
        if (o_xu_id >= XRAN_PORTS_NUM) {
            printf("oXuCfgFile%d exceeds max O-XU supported\n",o_xu_id);
        } else {
            int len = strlen(value);
            if(len > 512) len = 511;
            strncpy(&config->o_xu_cfg_file[o_xu_id][0], value, len);
            printf("oXuCfgFile%d: %s\n",o_xu_id, config->o_xu_cfg_file[o_xu_id]);
        }
        curr_oxu = o_xu_id;
    } else if (strcmp(key, KEY_NUM_SECMU) == 0){
        config->numSecMu[curr_oxu] = atoi(value);
        printf("numSecMu %d \n", config->numSecMu[curr_oxu]);
    } else if (strncmp(key, KEY_FILE_SEC_NUM, strlen(KEY_FILE_SEC_NUM)) == 0) {
        unsigned int idx = 0;
        sscanf(key,"oXuSecNumFile%02u",&idx);
        if (idx >= XRAN_MAX_NUM_MU) {
            printf("oXuSecNumFile%d exceeds max Numerology supported\n",idx);
        } else {
            int len = strlen(value);
            if(len > 512) len = 511;
            strncpy(&config->o_xu_mixed_num_file[curr_oxu][idx][0], value, len);
            printf("oXuSecNumFile: %s\n", config->o_xu_mixed_num_file[curr_oxu][idx]);
        }
    } else if (strncmp(key, KEY_FILE_O_XU_BBU_CFG, strlen(KEY_FILE_O_XU_BBU_CFG)) == 0) {
        int len = strlen(value);
        if(len > 512) len = 511;
        strncpy(&config->o_xu_bbu_cfg_file[0], value, len);
        printf("oXuBbuCfgFile: %s\n", config->o_xu_bbu_cfg_file);
    } else if (strncmp(key, KEY_OWDM_INIT_EN, strlen(KEY_OWDM_INIT_EN)) == 0) {
        config->owdmInitEn = atoi(value);
        printf("owdmInitEn %d\n", config->owdmInitEn);
    } else if (strncmp(key, KEY_OWDM_MEAS_METH, strlen(KEY_OWDM_MEAS_METH)) == 0) {
        config->owdmMeasMeth =  atoi(value);
        printf("owdmMeasMeth %d\n", config->owdmMeasMeth);
    } else if (strncmp(key, KEY_OWDM_NUM_SAMPS, strlen(KEY_OWDM_NUM_SAMPS)) == 0) {
        config->owdmNumSamps =  atoi(value);
        printf("owdmNumSamps %d\n", config->owdmNumSamps);
    } else if (strncmp(key, KEY_OWDM_FLTR_TYPE, strlen(KEY_OWDM_FLTR_TYPE)) == 0) {
        config->owdmFltType =  atoi(value);
        printf("owdmFltType %d\n", config->owdmFltType);
    } else if (strncmp(key, KEY_OWDM_RSP_TO, strlen(KEY_OWDM_RSP_TO)) == 0) {
        config->owdmRspTo =  atol(value);
        printf("owdmRspTo %lu\n", config->owdmRspTo);
    } else if (strncmp(key, KEY_OWDM_MEAS_ID, strlen(KEY_OWDM_MEAS_ID)) == 0) {
        config->owdmMeasId =  atoi(value);
        printf("owdmMeasId %d\n", config->owdmMeasId);
    } else if (strncmp(key, KEY_OWDM_EN, strlen(KEY_OWDM_EN)) == 0) {
        config->owdmEnable =  atoi(value);
        printf("owdmEnable %d\n", config->owdmEnable);
    } else if (strncmp(key, KEY_OWDM_PL_LENGTH, strlen(KEY_OWDM_PL_LENGTH)) == 0) {
        config->owdmPlLength = atoi(value);
        printf("owdmPlLength %d\n", config->owdmPlLength);
    } else if (strncmp(key, KEY_OWDM_MEAS_ST, strlen(KEY_OWDM_MEAS_ST)) == 0) {
        config->owdmMeasState =  atoi(value);
        printf("owdmMeasState %d\n", config->owdmMeasState);
    } else if (strncmp(key, KEY_O_XU_PCIE_BUS, strlen(KEY_O_XU_PCIE_BUS)) == 0) {
        unsigned int o_xu_id = 0;
        unsigned int vf_num = 0;
        sscanf(key,"PciBusAddoXu%02uVf%02u",&o_xu_id, &vf_num);
        if (o_xu_id >= XRAN_PORTS_NUM || vf_num >= XRAN_VF_MAX){
            printf("PciBusAddoXu%dVf%d exceeds max O-XU supported\n",o_xu_id, vf_num);
        } else {
            int len = strlen(value);
            if(len > 512) len = 511;
            strncpy(&config->o_xu_pcie_bus_addr[o_xu_id][vf_num][0], value, len);
            printf("PciBusAddoXu%dVf%d: %s\n",o_xu_id, vf_num, &config->o_xu_pcie_bus_addr[o_xu_id][vf_num][0]);
        }
    } else if (strncmp(key, KEY_O_XU_REM_MAC, strlen(KEY_O_XU_REM_MAC)) == 0) {
        unsigned int xu_num = 0;
        unsigned int vf_num = 0;

        sscanf(key,"oXuRem%02uMac%02u",&xu_num, &vf_num);

        if (xu_num >= XRAN_PORTS_NUM || vf_num >= XRAN_VF_MAX) {
            printf("oXuRem%02uMac%02u exceeds max supported\n",xu_num, vf_num);
        } else {
            printf("oXuRem%02uMac%02u: %s\n",xu_num, vf_num, value);
            sscanf(value, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", (uint8_t*)&config->remote_o_xu_addr[xu_num][vf_num].addr_bytes[0],
                                               (uint8_t*)&config->remote_o_xu_addr[xu_num][vf_num].addr_bytes[1],
                                               (uint8_t*)&config->remote_o_xu_addr[xu_num][vf_num].addr_bytes[2],
                                               (uint8_t*)&config->remote_o_xu_addr[xu_num][vf_num].addr_bytes[3],
                                               (uint8_t*)&config->remote_o_xu_addr[xu_num][vf_num].addr_bytes[4],
                                               (uint8_t*)&config->remote_o_xu_addr[xu_num][vf_num].addr_bytes[5]);

            printf("[xu %d vf %d]RU MAC address: %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
                xu_num,
                vf_num,
                config->remote_o_xu_addr[xu_num][vf_num].addr_bytes[0],
                config->remote_o_xu_addr[xu_num][vf_num].addr_bytes[1],
                config->remote_o_xu_addr[xu_num][vf_num].addr_bytes[2],
                config->remote_o_xu_addr[xu_num][vf_num].addr_bytes[3],
                config->remote_o_xu_addr[xu_num][vf_num].addr_bytes[4],
                config->remote_o_xu_addr[xu_num][vf_num].addr_bytes[5]);
        }
    }
    else if (strncmp(key, KEY_DL_CP_BURST, strlen(KEY_DL_CP_BURST)) == 0) {
        config->dlCpProcBurst = atoi(value);
        printf("dlCpProcBurst %d\n", config->dlCpProcBurst);
    } else if (strncmp(key, KEY_XRAN_MLOG_DIS, strlen(KEY_XRAN_MLOG_DIS)) == 0) {
        config->mlogxrandisable = atoi(value);
        printf("xranMlogDisable %d\n", config->mlogxrandisable);
    } else if (strncmp(key, KEY_LBM_ENABLE, strlen(KEY_LBM_ENABLE)) == 0) {
        config->lbmEnable = atoi(value);
        printf("%s %hhu\n",KEY_LBM_ENABLE, config->lbmEnable);
    } else if (strncmp(key, KEY_LBM_NUM_RETRANSMISSIONS, strlen(KEY_LBM_NUM_RETRANSMISSIONS)) == 0) {
        config->numRetransmissions = atoi(value);
        printf("%s %hu\n",KEY_LBM_NUM_RETRANSMISSIONS, config->numRetransmissions);
    } else if (strncmp(key, KEY_LBM_PERIODICITY, strlen(KEY_LBM_PERIODICITY)) == 0) {
        config->LBMPeriodicity = atoi(value);
        printf("%s %u\n",KEY_LBM_PERIODICITY,config->LBMPeriodicity);
    } else if (strncmp(key, KEY_LBR_TIMEOUT, strlen(KEY_LBR_TIMEOUT)) == 0) {
        config->LBRTimeOut = atoi(value);
        printf("%s %u\n",KEY_LBR_TIMEOUT,config->LBRTimeOut);
    } else {
        printf("Unsupported configuration key [%s]\n", key);
        return -1;
    }

    return 0;
}

static int
fillSlotStructAsCb(void* cbParam, const char *key, const char *value)
{
    struct slot_cfg_to_pars *p_slot_cfg = (struct slot_cfg_to_pars*) cbParam;
    RuntimeConfig *config  = p_slot_cfg->config;
    int32_t  direction     = p_slot_cfg->direction;
    int32_t  slot_idx      = p_slot_cfg->slot_idx;
    uint32_t section_idx   = 0;

    //printf("Dir %d slot %d\n", direction, slot_idx);

    if (strcmp(key, KEY_NPRBELEM_UL ) == 0) {
        config->p_SlotPrbMap[direction][slot_idx]->nPrbElm = atoi(value);
        if (config->p_SlotPrbMap[direction][slot_idx]->nPrbElm > XRAN_MAX_SECTIONS_PER_SLOT)
        {
            printf("nTddPeriod is larger than max allowed, invalid!\n");
            config->p_SlotPrbMap[direction][slot_idx]->nPrbElm  = XRAN_MAX_SECTIONS_PER_SLOT;
        }
        printf("nPrbElemUl: %d\n",config->p_SlotPrbMap[direction][slot_idx]->nPrbElm );
    } else if (strcmp(key, KEY_NPRBELEM_SRS ) == 0) {
        config->p_SlotSrsPrbMap[direction][slot_idx]->nPrbElm = atoi(value);
        if (config->p_SlotSrsPrbMap[direction][slot_idx]->nPrbElm > XRAN_MAX_SECTIONS_PER_SLOT)
        {
            printf("nTddPeriod is larger than max allowed, invalid!\n");
            config->p_SlotSrsPrbMap[direction][slot_idx]->nPrbElm  = XRAN_MAX_SECTIONS_PER_SLOT;
        }
        printf("nPrbElemSrs: %d\n",config->p_SlotSrsPrbMap[direction][slot_idx]->nPrbElm );
    } else if (strncmp(key, KEY_PRBELEM_UL_ANT_M, strlen(KEY_PRBELEM_UL_ANT_M)) == 0) {
        sscanf(key,"PrbElemUlAntCMask%u",&section_idx);
        if (section_idx >= config->p_SlotPrbMap[direction][slot_idx]->nPrbElm){
            printf("section_idx %d exceeds nPrbElemul\n",section_idx);
        }
        else{
            sscanf(value, "%lx",(uint64_t*)&config->SlotPrbAntCMask[direction][slot_idx][section_idx]);
            printf("%s%u 0x%lx\n",KEY_PRBELEM_UL_ANT_M, section_idx, config->SlotPrbAntCMask[direction][slot_idx][section_idx]);
        }
    } else if (strncmp(key, KEY_PRBELEM_SRS_ANT_M, strlen(KEY_PRBELEM_SRS_ANT_M)) == 0) {
        sscanf(key,"PrbElemSrsAntCMask%u",&section_idx);
        if (section_idx >= config->p_SlotSrsPrbMap[direction][slot_idx]->nPrbElm){
            printf("section_idx %d exceeds nPrbElemul\n",section_idx);
        }
        else{
            sscanf(value, "%lx",(uint64_t*)&config->SlotSrsPrbAntCMask[direction][slot_idx][section_idx]);
            printf("%s%u 0x%lx\n",KEY_PRBELEM_SRS_ANT_M, section_idx, config->SlotSrsPrbAntCMask[direction][slot_idx][section_idx]);
        }
    } else if (strncmp(key, KEY_PRBELEM_UL_CC_M, strlen(KEY_PRBELEM_UL_CC_M)) == 0) {
        sscanf(key,"PrbElemUlCCMask%u",&section_idx);
        if (section_idx >= config->p_SlotPrbMap[direction][slot_idx]->nPrbElm){
            printf("section_idx %d exceeds nPrbElemUL\n",section_idx);
        }
        else{
            sscanf(value, "%02hx",(uint16_t*)&config->SlotPrbCCmask[direction][slot_idx][section_idx]);
            printf("%s%u 0x%02x\n",KEY_PRBELEM_UL_CC_M, section_idx, config->SlotPrbCCmask[direction][slot_idx][section_idx]);
        }
    } else if (strncmp(key, KEY_PRBELEM_SRS_CC_M, strlen(KEY_PRBELEM_SRS_CC_M)) == 0) {
        sscanf(key,"PrbElemSrsCCMask%u",&section_idx);
        if (section_idx >= config->p_SlotSrsPrbMap[direction][slot_idx]->nPrbElm){
            printf("section_idx %d exceeds nPrbElemUL\n",section_idx);
        }
        else{
            sscanf(value, "%02hx",(uint16_t*)&config->SlotSrsPrbCCmask[direction][slot_idx][section_idx]);
            printf("%s%u 0x%02x\n",KEY_PRBELEM_SRS_CC_M, section_idx, config->SlotSrsPrbCCmask[direction][slot_idx][section_idx]);
        }
    } else if (strncmp(key, KEY_PRBELEM_UL, strlen(KEY_PRBELEM_UL)) == 0) {
        sscanf(key,"PrbElemUl%u",&section_idx);
        if (section_idx >=  config->p_SlotPrbMap[direction][slot_idx]->nPrbElm) {
            printf("section_idx %d exceeds nPrbElemUl\n",section_idx);
        }
        else {
            struct xran_prb_elm *pPrbElem = &config->p_SlotPrbMap[direction][slot_idx]->prbMap[section_idx];
            sscanf(value, "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd",
                (int16_t*)&pPrbElem->nRBStart,
                (int16_t*)&pPrbElem->nRBSize,
                (int16_t*)&pPrbElem->nStartSymb,
                (int16_t*)&pPrbElem->numSymb,
                (int16_t*)&pPrbElem->nBeamIndex,
                (int16_t*)&pPrbElem->bf_weight_update,
                (int16_t*)&pPrbElem->compMethod,
                (int16_t*)&pPrbElem->iqWidth,
                (int16_t*)&pPrbElem->BeamFormingType);
            printf("nPrbElemUl%d: ",section_idx);
            printf("nRBStart %d,nRBSize %d,nStartSymb %d,numSymb %d,nBeamIndex %d, bf_weight_update %d compMethod %d, iqWidth %d BeamFormingType %d\n",
                pPrbElem->nRBStart,pPrbElem->nRBSize,pPrbElem->nStartSymb,pPrbElem->numSymb,pPrbElem->nBeamIndex, pPrbElem->bf_weight_update, pPrbElem->compMethod, pPrbElem->iqWidth, pPrbElem->BeamFormingType);
        }
    } else if (strncmp(key, KEY_PRBELEM_SRS, strlen(KEY_PRBELEM_SRS)) == 0) {
        sscanf(key,"PrbElemSrs%u",&section_idx);
        if (section_idx >=  config->p_SlotSrsPrbMap[direction][slot_idx]->nPrbElm) {
            printf("section_idx %d exceeds nPrbElemUl\n",section_idx);
        }
        else {
            struct xran_prb_elm *pPrbElem = &config->p_SlotSrsPrbMap[direction][slot_idx]->prbMap[section_idx];
            sscanf(value, "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd",
                (int16_t*)&pPrbElem->nRBStart,
                (int16_t*)&pPrbElem->nRBSize,
                (int16_t*)&pPrbElem->nStartSymb,
                (int16_t*)&pPrbElem->numSymb,
                (int16_t*)&pPrbElem->nBeamIndex,
                (int16_t*)&pPrbElem->bf_weight_update,
                (int16_t*)&pPrbElem->compMethod,
                (int16_t*)&pPrbElem->iqWidth,
                (int16_t*)&pPrbElem->BeamFormingType);
            printf("nPrbElemSrs%d: ",section_idx);
            printf("nRBStart %d,nRBSize %d,nStartSymb %d,numSymb %d,nBeamIndex %d, bf_weight_update %d compMethod %d, iqWidth %d BeamFormingType %d\n",
                pPrbElem->nRBStart,pPrbElem->nRBSize,pPrbElem->nStartSymb,pPrbElem->numSymb,pPrbElem->nBeamIndex, pPrbElem->bf_weight_update, pPrbElem->compMethod, pPrbElem->iqWidth, pPrbElem->BeamFormingType);
        }
    } else if(strncmp(key, KEY_EXTBFW_UL, strlen(KEY_EXTBFW_UL)) == 0) {
        sscanf(key, "ExtBfwUl%u", &section_idx);
        if(section_idx >= config->p_SlotPrbMap[direction][slot_idx]->nPrbElm) {
            printf("section_idx %d of bfw exceeds nPrbElemUl\n",section_idx);
        }else{
            struct xran_prb_elm *pPrbElem = &config->p_SlotPrbMap[direction][slot_idx]->prbMap[section_idx];
            sscanf(value, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
                (uint8_t*)&pPrbElem->bf_weight.numBundPrb,
                (uint8_t*)&pPrbElem->bf_weight.numSetBFWs,
                (uint8_t*)&pPrbElem->bf_weight.RAD,
                (uint8_t*)&pPrbElem->bf_weight.disableBFWs,
                (uint8_t*)&pPrbElem->bf_weight.bfwIqWidth,
                (uint8_t*)&pPrbElem->bf_weight.bfwCompMeth,
                (uint8_t*)&pPrbElem->bf_weight.extType);
            printf(KEY_EXTBFW_UL"%d: ", section_idx);
            printf("numBundPrb %d, numSetBFW %d, RAD %d, disableBFW %d, bfwIqWidth %d, bfwCompMeth %d, extType %d\n",
                pPrbElem->bf_weight.numBundPrb, pPrbElem->bf_weight.numSetBFWs, pPrbElem->bf_weight.RAD, pPrbElem->bf_weight.disableBFWs, pPrbElem->bf_weight.bfwIqWidth, pPrbElem->bf_weight.bfwCompMeth, pPrbElem->bf_weight.extType);
        }
    }else if (strcmp(key, KEY_NPRBELEM_DL ) == 0) {
        config->p_SlotPrbMap[direction][slot_idx]->nPrbElm = atoi(value);
        if (config->p_SlotPrbMap[direction][slot_idx]->nPrbElm > XRAN_MAX_SECTIONS_PER_SLOT)
        {
            printf("nTddPeriod is larger than max allowed, invalid!\n");
            config->p_SlotPrbMap[direction][slot_idx]->nPrbElm = XRAN_MAX_SECTIONS_PER_SLOT;
        }
        printf("nPrbElemDl: %d\n",config->p_SlotPrbMap[direction][slot_idx]->nPrbElm);
    } else if (strncmp(key, KEY_PRBELEM_DL_ANT_M, strlen(KEY_PRBELEM_DL_ANT_M)) == 0) {
        sscanf(key,"PrbElemDlAntCMask%u",&section_idx);
        if (section_idx >= config->p_SlotPrbMap[direction][slot_idx]->nPrbElm){
            printf("section_idx %d exceeds nPrbElemDl\n",section_idx);
        }
        else{
            sscanf(value, "%lx",(uint64_t*)&config->SlotPrbAntCMask[direction][slot_idx][section_idx]);
            printf("%s%u 0x%lx\n",KEY_PRBELEM_DL_ANT_M, section_idx, config->SlotPrbAntCMask[direction][slot_idx][section_idx]);
        }
    }  else if (strncmp(key, KEY_PRBELEM_DL_CC_M, strlen(KEY_PRBELEM_DL_CC_M)) == 0) {
        sscanf(key,"PrbElemDlCCMask%u",&section_idx);
        if (section_idx >= config->p_SlotPrbMap[direction][slot_idx]->nPrbElm){
            printf("section_idx %d exceeds nPrbElemDl\n",section_idx);
        }
        else{
            sscanf(value, "%02hx",(uint16_t*)&config->SlotPrbCCmask[direction][slot_idx][section_idx]);
            printf("%s%u 0x%02x\n",KEY_PRBELEM_DL_CC_M, section_idx, config->SlotPrbCCmask[direction][slot_idx][section_idx]);
        }
    } else if (strncmp(key, KEY_PRBELEM_DL, strlen(KEY_PRBELEM_DL)) == 0) {
        sscanf(key,"PrbElemDl%u",&section_idx);
        if (section_idx >= config->p_SlotPrbMap[direction][slot_idx]->nPrbElm){
            printf("section_idx %d exceeds nPrbElemDl\n",section_idx);
        }
        else{
            struct xran_prb_elm *pPrbElem = &config->p_SlotPrbMap[direction][slot_idx]->prbMap[section_idx];
            sscanf(value, "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd",
                (int16_t*)&pPrbElem->nRBStart,
                (int16_t*)&pPrbElem->nRBSize,
                (int16_t*)&pPrbElem->nStartSymb,
                (int16_t*)&pPrbElem->numSymb,
                (int16_t*)&pPrbElem->nBeamIndex,
                (int16_t*)&pPrbElem->bf_weight_update,
                (int16_t*)&pPrbElem->compMethod,
                (int16_t*)&pPrbElem->iqWidth,
                (int16_t*)&pPrbElem->BeamFormingType);
            printf("nPrbElemDl%d: ",section_idx);
            printf("nRBStart %d,nRBSize %d,nStartSymb %d,numSymb %d,nBeamIndex %d, bf_weight_update %d compMethod %d, iqWidth %d BeamFormingType %d\n",
                pPrbElem->nRBStart,pPrbElem->nRBSize,pPrbElem->nStartSymb,pPrbElem->numSymb,pPrbElem->nBeamIndex, pPrbElem->bf_weight_update, pPrbElem->compMethod, pPrbElem->iqWidth, pPrbElem->BeamFormingType);
        }
    } else if(strncmp(key, KEY_EXTBFW_DL, strlen(KEY_EXTBFW_DL)) == 0) {
        sscanf(key, "ExtBfwDl%u", &section_idx);
        if(section_idx >= config->p_SlotPrbMap[direction][slot_idx]->nPrbElm) {
            printf("section_idx %d of bfw exceeds nPrbElemUl\n",section_idx);
        }
        else{
            struct xran_prb_elm *pPrbElem = &config->p_SlotPrbMap[direction][slot_idx]->prbMap[section_idx];
            sscanf(value, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
                (uint8_t*)&pPrbElem->bf_weight.numBundPrb,
                (uint8_t*)&pPrbElem->bf_weight.numSetBFWs,
                (uint8_t*)&pPrbElem->bf_weight.RAD,
                (uint8_t*)&pPrbElem->bf_weight.disableBFWs,
                (uint8_t*)&pPrbElem->bf_weight.bfwIqWidth,
                (uint8_t*)&pPrbElem->bf_weight.bfwCompMeth,
                (uint8_t*)&pPrbElem->bf_weight.extType);
            printf(KEY_EXTBFW_DL"%d: ",section_idx);
            printf("numBundPrb %d, numSetBFW %d, RAD %d, disableBFW %d, bfwIqWidth %d, bfwCompMeth %d, extType %d\n",
                pPrbElem->bf_weight.numBundPrb, pPrbElem->bf_weight.numSetBFWs, pPrbElem->bf_weight.RAD, pPrbElem->bf_weight.disableBFWs, pPrbElem->bf_weight.bfwIqWidth, pPrbElem->bf_weight.bfwCompMeth, pPrbElem->bf_weight.extType);
        }
    } else {
        printf("Unsupported configuration key [%s]\n", key);
        return -1;
    }

    return 0;
}

struct xran_prb_map*
config_malloc_prb_map(void)
{
    uint32_t size = sizeof(struct xran_prb_map)  + (XRAN_MAX_SECTIONS_PER_SLOT) * sizeof(struct xran_prb_elm);
    void *ret = NULL;

    ret = malloc(size);

    if(ret) {
        memset(ret, 0, size);
        return (struct xran_prb_map*)ret;
    } else {
        rte_panic("xran_prb_map alloc failed");
    }
}

int32_t
config_init(RuntimeConfig *p_o_xu_cfg)
{
    int32_t i, j;
    memset(p_o_xu_cfg, 0, sizeof(RuntimeConfig));

    for(i= 0; i < XRAN_MAX_NUM_MU; i++){
        p_o_xu_cfg->p_PrbMapDl[i]  = config_malloc_prb_map();
        p_o_xu_cfg->p_PrbMapUl[i]  = config_malloc_prb_map();
    }
    p_o_xu_cfg->p_PrbMapSrs = config_malloc_prb_map();
    p_o_xu_cfg->p_PrbMapCsiRs = config_malloc_prb_map();

    for (i= 0; i < XRAN_DIR_MAX; i++){
        for (j= 0; j < XRAN_N_FE_BUF_LEN; j++){
           p_o_xu_cfg->p_SlotPrbMap[i][j] = config_malloc_prb_map();
           p_o_xu_cfg->p_SlotSrsPrbMap[i][j] = config_malloc_prb_map();
        }
    }

#if 0
    for (i = 0; i < XRAN_DIR_MAX; i++) {
        for (j = 0; j < XRAN_N_FE_BUF_LEN; j++) {
            for (k = 0; k < XRAN_MAX_SECTOR_NR; k++) {
                for (z = 0; z < XRAN_MAX_ANTENNA_NR; z++) {
                    p_o_xu_cfg->p_SlotSrsPrbMap[i][j][k][z] = config_malloc_prb_map();
                    p_o_xu_cfg->p_SlotPrbMap[i][j][k][z] = config_malloc_prb_map();
                }
            }
        }
    }
#endif

    return 0;
}


int32_t
config_init2(RuntimeConfig *p_o_xu_cfg)
{
    int32_t i, j, k, z;
    for (i = 0; i < XRAN_DIR_MAX; i++) {
        for (j = 0; j < XRAN_N_FE_BUF_LEN; j++) {
            for (k = 0; k < p_o_xu_cfg->numCC; k++) {
                for (z = 0; z < p_o_xu_cfg->numAxc; z++) {
                    p_o_xu_cfg->p_RunSlotPrbMap[i][j][k][z] = config_malloc_prb_map();
                    p_o_xu_cfg->p_RunSrsSlotPrbMap[i][j][k][z] = config_malloc_prb_map();
                }

                for (z = 0; z < p_o_xu_cfg->antElmTRx; z++) {
                    p_o_xu_cfg->p_RunSrsSlotPrbMap[i][j][k][z] = config_malloc_prb_map();
                }
            }
        }
    }

    return 0;
}

int
parseConfigFile(const char *filename, RuntimeConfig *config)
{
    char inputLine[MAX_LINE_SIZE] = {0};
    int inputLen = 0;
    int i;
    int lineNum = 0;
    char key[MAX_LINE_SIZE] = {0};
    char value[MAX_LINE_SIZE] = {0};
    FILE *file = fopen(filename, "r");

    if (NULL == file) {
        log_err("Error while opening config file from: %s", filename);
        return -1;
    }

//    init_config(config);

    for (;;) {
        if (fgets(inputLine, MAX_LINE_SIZE, file) == NULL) {
            if (lineNum > 0) {
                printf("%d lines of config file has been read.\n", lineNum);
                break;
            } else {
                printf("Configuration file reading error has occurred.\n");
                fclose(file);
                return -1;
            }
        }


        lineNum++;
        inputLen = strlen(inputLine);

        for (i=0; i<inputLen; i++)
            if (inputLine[i] == '#') {
                inputLine[i] = '\0';
                inputLen = i + 1;
                break;
            }

        for (i=0; i<inputLen; i++)
            if (inputLine[i] == '=') {
                strncpy(key, inputLine, i);
                key[i] = '\0';
                trim(key);
                if ((i + 1 > inputLen - 1) || (i - 2 > inputLen)) {
                    printf("1:Parsing config file error at line %d\n", lineNum);
                    fclose(file);
                    return -1;
                }
                strncpy(value, &inputLine[i+1], (sizeof(value) - 1));
                value[inputLen-i-2] = '\0';
                trim(value);

                if (strlen(key) == 0 || strlen(value) == 0) {
                    printf("2:Parsing config file error at line %d\n", lineNum);
                    fclose(file);
                    return -1;
                }

                if (fillConfigStruct(config, key, value) != 0) {
                    printf("3:Parsing config file error at line %d\n", lineNum);
                }

                break;
            }

        memset(&inputLine[0], 0, sizeof(MAX_LINE_SIZE));
        memset(&key[0], 0, sizeof(MAX_LINE_SIZE));
        memset(&value[0], 0, sizeof(MAX_LINE_SIZE));
    }
    fclose(file);

    return 0;
}

int32_t
parseSlotConfigFile(const char *dir, RuntimeConfig *config)
{
    int32_t ret     = 0;
    char filename[512];
    size_t len;
    int32_t slot_idx = 0;
    int32_t direction = 0;
    struct slot_cfg_to_pars slot_cfg_param;

    for (direction = 0; direction < XRAN_DIR_MAX; direction++) {
        for (slot_idx = 0; slot_idx  < config->numSlots; slot_idx++){
            memset(filename, 0, sizeof(filename));
            printf("dir (%s)\n",dir);
            len = strlen(dir) + 1;
            if (len > 511) {
                printf("parseSlotConfigFile: Name of directory, %s is too long.  Maximum is 511 characters!!\n", dir);
                return -1;
            } else {
                strncpy(filename, dir, len);
            }
            strncat(filename, "/", 1);
            len +=1;
            len = (sizeof(filename)) - len;

            if(len > strlen(config->SlotNum_file[direction][slot_idx])){
                strncat(filename, config->SlotNum_file[direction][slot_idx], RTE_MIN (len, strlen(config->SlotNum_file[direction][slot_idx])));
            } else {
                printf("File name error\n");
                return -1;
            }
            printf("slot_file[%d][%d] (%s)\n",direction, slot_idx, filename);
            printf("\n=================== Slot%s %d===================\n", ((direction == XRAN_DIR_UL) ? "RX" : "TX"), slot_idx);

            slot_cfg_param.direction = direction;
            slot_cfg_param.slot_idx  = slot_idx;
            slot_cfg_param.config    = config;

            if (parseFileViaCb(filename, fillSlotStructAsCb,  (void*)&slot_cfg_param)) {
                printf("Configuration file error\n");
            }
        }
    }

    return ret;
}

int32_t
parseFileViaCb (char *filename, fillConfigStruct_fn cbFn, void* cbParm)
{
    char inputLine[MAX_LINE_SIZE] = {0};
    int inputLen = 0;
    int i;
    int lineNum = 0;
    char key[MAX_LINE_SIZE] = {0};
    char value[MAX_LINE_SIZE] = {0};
    FILE *file = fopen(filename, "r");

    if (NULL == file) {
        log_err("Error while opening config file from: %s", filename);
        return -1;
    }

    for (;;) {
        if (fgets(inputLine, MAX_LINE_SIZE, file) == NULL) {
            if (lineNum > 0) {
                printf("%d lines of config file has been read.\n", lineNum);
                break;
            } else {
                printf("Configuration file reading error has occurred.\n");
                fclose(file);
                return -1;
            }
        }


        lineNum++;
        inputLen = strlen(inputLine);

        for (i=0; i<inputLen; i++)
            if (inputLine[i] == '#') {
                inputLine[i] = '\0';
                inputLen = i + 1;
                break;
            }

        for (i=0; i<inputLen; i++)
            if (inputLine[i] == '=') {
                strncpy(key, inputLine, i);
                key[i] = '\0';
                trim(key);
                if ((i + 1 > inputLen - 1) || (i - 2 > inputLen)) {
                    log_err("Parsing config file error at line %d", lineNum);
                    fclose(file);
                    return -1;
                }
                strncpy(value, &inputLine[i+1], (sizeof(value) - 1));
                value[inputLen-i-2] = '\0';
                trim(value);

                if (strlen(key) == 0 || strlen(value) == 0) {
                    printf("Parsing config file error at line %d", lineNum);
                    fclose(file);
                    return -1;
                }

                if(cbFn){
                    if (cbFn(cbParm, key, value) != 0) {
                        fclose(file);
                        return -1;
                    }
                } else {
                    printf("cbFn==NULL\n");
                    fclose(file);
                    return -1;
                }

                break;
            }

        memset(&inputLine[0], 0, sizeof(MAX_LINE_SIZE));
        memset(&key[0], 0, sizeof(MAX_LINE_SIZE));
        memset(&value[0], 0, sizeof(MAX_LINE_SIZE));
    }
    fclose(file);

    return 0;
}

int parseUsecaseFile(const char *filename, UsecaseConfig *usecase_cfg)
{
    char inputLine[MAX_LINE_SIZE] = {0};
    int inputLen = 0;
    int i;
    int lineNum = 0;
    char key[MAX_LINE_SIZE] = {0};
    char value[MAX_LINE_SIZE] = {0};
    FILE *file = fopen(filename, "r");

    if (NULL == file){
        log_err("Error while opening config file from: %s", filename);
        return -1;
    }

    for (;;) {
        if (fgets(inputLine, MAX_LINE_SIZE, file) == NULL) {
            if (lineNum > 0) {
                printf("%d lines of usecase config file has been read.\n", lineNum);
                break;
            } else {
                printf("Usecase configuration file reading error has occurred.\n");
                fclose(file);
                return -1;
            }
        }


        lineNum++;
        inputLen = strlen(inputLine);

        for (i=0; i<inputLen; i++)
            if (inputLine[i] == '#') {
                inputLine[i] = '\0';
                inputLen = i + 1;
                break;
            }

        for (i=0; i<inputLen; i++)
            if (inputLine[i] == '=') {
                strncpy(key, inputLine, i);
                key[i] = '\0';
                trim(key);
                if ((i + 1 > inputLen - 1) || (i - 2 > inputLen)) {
                    printf("1:Parsing usecase config file error at line %d\n", lineNum);
                    fclose(file);
                    return -1;
                }
                strncpy(value, &inputLine[i+1], (sizeof(value) - 1));
                value[inputLen-i-2] = '\0';
                trim(value);

                if (strlen(key) == 0 || strlen(value) == 0) {
                    printf("2:Parsing usecase config file error at line %d\n", lineNum);
                    fclose(file);
                    return -1;
                }

                if (fillUsecaseStruct(usecase_cfg, key, value) != 0) {
                    printf("3:Parsing usecase config file error at line %d\n", lineNum);
                }

                break;
            }

        memset(&inputLine[0], 0, sizeof(MAX_LINE_SIZE));
        memset(&key[0], 0, sizeof(MAX_LINE_SIZE));
        memset(&value[0], 0, sizeof(MAX_LINE_SIZE));
    }
    fclose(file);

    return 0;
}
