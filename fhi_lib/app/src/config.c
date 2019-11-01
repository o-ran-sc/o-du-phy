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
 * @brief
 * @file
 * @ingroup
 * @author Intel Corporation
 **/

#include "rte_common.h"
#include "config.h"
#include "common.h"
#include "debug.h"

#include <rte_ethdev.h>

#define MAX_LINE_SIZE 512
/* Configuration file maximum supported line length */

#define KEY_APP_MODE        "appMode"
#define KEY_XRAN_MODE       "xranMode"
#define KEY_MU_NUMBER       "mu"
#define KEY_NDLABSFREPOINTA "nDLAbsFrePointA"
#define KEY_NULABSFREPOINTA "nULAbsFrePointA"
#define KEY_NDLBANDWIDTH    "nDLBandwidth"
#define KEY_NULBANDWIDTH    "nULBandwidth"
#define KEY_NDLFFTSIZE      "nDLFftSize"
#define KEY_NULFFTSIZE      "nULFftSize"

#define KEY_NFRAMEDUPLEXTYPE "nFrameDuplexType"
#define KEY_NTDDPERIOD       "nTddPeriod"

#define KEY_SSLOTCONFIG     "sSlotConfig"

#define KEY_CC_PER_PORT_NUM "ccNum"
#define KEY_ANT_NUM         "antNum"
#define KEY_TTI_PERIOD      "ttiPeriod"

#define KEY_MTU_SIZE        "MTUSize"
#define KEY_LLS_CU_MAC      "llsCUMac"
#define KEY_RU_MAC          "ruMac"

#define KEY_FILE_NUMSLOTS   "numSlots"
#define KEY_FILE_AxC        "antC"
#define KEY_FILE_PRACH_AxC  "antPrachC"


#define KEY_PRACH_ENABLE   "rachEanble"
#define KEY_PRACH_CFGIDX   "prachConfigIndex"

#define KEY_IQ_SWAP        "iqswap"
#define KEY_HTONS_SWAP     "nebyteorderswap"

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


#define KEY_CP_ENABLE      "CPenable"
#define KEY_CP_VTAG        "c_plane_vlan_tag"
#define KEY_UP_VTAG        "u_plane_vlan_tag"
#define KEY_DEBUG_STOP     "debugStop"
#define KEY_DEBUG_STOP_CNT     "debugStopCount"
#define KEY_BBDEV_MODE     "bbdevMode"
#define KEY_DYNA_SEC_ENA     "DynamicSectionEna"


/**
 * Set runtime configuration parameters to their defaults.
 *
 * @todo Initialize missing parameters.
 */
static void init_config(RuntimeConfig* config)
{
    memset(config , 0, sizeof(RuntimeConfig));
}

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
    int32_t parse_res = 0;

    if (strcmp(key, KEY_APP_MODE) == 0){
        config->appMode = atoi(value);
    } else if (strcmp(key, KEY_XRAN_MODE) == 0) {
        config->xranMode = atoi(value);
    } else if (strcmp(key, KEY_CC_PER_PORT_NUM) == 0) {
        config->numCC= atoi(value);
    } else if (strcmp(key, KEY_MU_NUMBER) == 0) {
        config->mu_number= atoi(value);
        printf("mu_number: %d\n",config->mu_number);
    } else if (strcmp(key, KEY_NDLABSFREPOINTA) == 0) {
        config->nDLAbsFrePointA = atoi(value);
        printf("nDLAbsFrePointA: %d\n",config->nDLAbsFrePointA);
    } else if (strcmp(key, KEY_NULABSFREPOINTA) == 0) {
        config->nULAbsFrePointA = atoi(value);
        printf("nULAbsFrePointA: %d\n",config->nULAbsFrePointA);
    } else if (strcmp(key, KEY_NDLBANDWIDTH) == 0) {
        config->nDLBandwidth = atoi(value);
        printf("nDLBandwidth: %d\n",config->nDLBandwidth);
    } else if (strcmp(key, KEY_NULBANDWIDTH) == 0) {
        config->nULBandwidth = atoi(value);
        printf("nULBandwidth: %d\n",config->nULBandwidth);
    } else if (strcmp(key, KEY_NDLFFTSIZE) == 0) {
        config->nDLFftSize = atoi(value);
        printf("nULFftSize: %d\n",config->nDLFftSize);
    } else if (strcmp(key, KEY_NULFFTSIZE) == 0) {
        config->nULFftSize = atoi(value);
        printf("nULFftSize: %d\n",config->nULFftSize);
    } else if (strcmp(key, KEY_NFRAMEDUPLEXTYPE) == 0) {
        config->nFrameDuplexType = atoi(value);
        printf("nFrameDuplexType: %d\n",config->nFrameDuplexType);
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
            sscanf(value, "%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x",
                                           (uint32_t*)&config->sSlotConfig[slot_num].nSymbolType[0],
                                           (uint32_t*)&config->sSlotConfig[slot_num].nSymbolType[1],
                                           (uint32_t*)&config->sSlotConfig[slot_num].nSymbolType[2],
                                           (uint32_t*)&config->sSlotConfig[slot_num].nSymbolType[3],
                                           (uint32_t*)&config->sSlotConfig[slot_num].nSymbolType[4],
                                           (uint32_t*)&config->sSlotConfig[slot_num].nSymbolType[5],
                                           (uint32_t*)&config->sSlotConfig[slot_num].nSymbolType[6],
                                           (uint32_t*)&config->sSlotConfig[slot_num].nSymbolType[7],
                                           (uint32_t*)&config->sSlotConfig[slot_num].nSymbolType[8],
                                           (uint32_t*)&config->sSlotConfig[slot_num].nSymbolType[9],
                                           (uint32_t*)&config->sSlotConfig[slot_num].nSymbolType[10],
                                           (uint32_t*)&config->sSlotConfig[slot_num].nSymbolType[11],
                                           (uint32_t*)&config->sSlotConfig[slot_num].nSymbolType[12],
                                           (uint32_t*)&config->sSlotConfig[slot_num].nSymbolType[13]);
            printf("sSlotConfig%d: ",slot_num);
            for (i = 0; i< 14; i++){
                printf("%d ",config->sSlotConfig[slot_num].nSymbolType[i]);
            }
            printf("\n");
        }
    } else if (strcmp(key, KEY_ANT_NUM) == 0) {
        config->numAxc = atoi(value);
    } else if (strcmp(key, KEY_TTI_PERIOD) == 0) {
        config->ttiPeriod = atoi(value);
    } else if (strcmp(key, KEY_IQ_SWAP) == 0) {
        config->iqswap = atoi(value);
    } else if (strcmp(key, KEY_HTONS_SWAP) == 0) {
        config->nebyteorderswap = atoi(value);
    } else if (strcmp(key, KEY_MTU_SIZE) == 0) {
        config->mtu = atoi(value);
        printf("mtu %d\n", config->mtu);
    } else if (strcmp(key, KEY_LLS_CU_MAC) == 0) {
        sscanf(value, "%02x:%02x:%02x:%02x:%02x:%02x", (uint32_t*)&config->o_du_addr.addr_bytes[0],
                                           (uint32_t*)&config->o_du_addr.addr_bytes[1],
                                           (uint32_t*)&config->o_du_addr.addr_bytes[2],
                                           (uint32_t*)&config->o_du_addr.addr_bytes[3],
                                           (uint32_t*)&config->o_du_addr.addr_bytes[4],
                                           (uint32_t*)&config->o_du_addr.addr_bytes[5]);

        printf("lls-CU MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
            config->o_du_addr.addr_bytes[0],
            config->o_du_addr.addr_bytes[1],
            config->o_du_addr.addr_bytes[2],
            config->o_du_addr.addr_bytes[3],
            config->o_du_addr.addr_bytes[4],
            config->o_du_addr.addr_bytes[5]);

    } else if (strcmp(key, KEY_RU_MAC) == 0) {
        sscanf(value, "%02x:%02x:%02x:%02x:%02x:%02x", (uint32_t*)&config->o_ru_addr.addr_bytes[0],
                                           (uint32_t*)&config->o_ru_addr.addr_bytes[1],
                                           (uint32_t*)&config->o_ru_addr.addr_bytes[2],
                                           (uint32_t*)&config->o_ru_addr.addr_bytes[3],
                                           (uint32_t*)&config->o_ru_addr.addr_bytes[4],
                                           (uint32_t*)&config->o_ru_addr.addr_bytes[5]);

        printf("RU MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
            config->o_ru_addr.addr_bytes[0],
            config->o_ru_addr.addr_bytes[1],
            config->o_ru_addr.addr_bytes[2],
            config->o_ru_addr.addr_bytes[3],
            config->o_ru_addr.addr_bytes[4],
            config->o_ru_addr.addr_bytes[5]);
    } else if (strcmp(key, KEY_FILE_NUMSLOTS) == 0) {
        config->numSlots = atoi(value);
        printf("numSlots: %d\n",config->numSlots);
    }else if (strncmp(key, KEY_FILE_AxC, strlen(KEY_FILE_AxC)) == 0) {
        unsigned int ant_num = 0;
        sscanf(key,"antC%02u",&ant_num);
        if (ant_num >= MAX_ANT_CARRIER_SUPPORTED)
        {
            printf("antC%d exceeds max antenna supported\n",ant_num);
        }
        else{
            strncpy(&config->ant_file[ant_num][0], value, strlen(value));
            printf("antC%d: %s\n",ant_num, config->ant_file[ant_num]);
        }
    } else if (strcmp(key, KEY_PRACH_ENABLE) == 0) {
        config->enablePrach = atoi(value);
        printf("Prach enable: %d\n",config->enablePrach);
    } else if (strcmp(key, KEY_PRACH_CFGIDX) == 0) {
        config->prachConfigIndex = atoi(value);
        printf("Prach config index: %d\n",config->prachConfigIndex);
    } else if (strncmp(key, KEY_FILE_PRACH_AxC, strlen(KEY_FILE_PRACH_AxC)) == 0) {
        unsigned int ant_num = 0;
        sscanf(key,"antPrachC%02u",&ant_num);
        if (ant_num >= MAX_ANT_CARRIER_SUPPORTED)
        {
            printf("antC%d exceeds max antenna supported\n",ant_num);
        }
        else{
            strncpy(&config->prach_file[ant_num][0], value, strlen(value));
            printf("antPrachC%d: %s\n",ant_num, config->prach_file[ant_num]);
        }
        /* timing */
    } else if (strcmp(key, KEY_TADV_CP_DL ) == 0) {
        config->Tadv_cp_dl = atoi(value);
        printf("Tadv_cp_dl: %d\n",config->Tadv_cp_dl);
    } else if (strcmp(key, KEY_T2A_MIN_CP_DL ) == 0) {
        config->T2a_min_cp_dl = atoi(value);
        printf("T2a_min_cp_dl: %d\n",config->T2a_min_cp_dl);
    } else if (strcmp(key, KEY_T2A_MAX_CP_DL ) == 0) {
        config->T2a_max_cp_dl = atoi(value);
        printf("T2a_max_cp_dl: %d\n",config->T2a_max_cp_dl);
    } else if (strcmp(key, KEY_T2A_MIN_CP_UL ) == 0) {
        config->T2a_min_cp_ul = atoi(value);
        printf("T2a_min_cp_ul: %d\n",config->T2a_min_cp_ul);
    } else if (strcmp(key, KEY_T2A_MAX_CP_UL ) == 0) {
        config->T2a_max_cp_ul = atoi(value);
        printf("T2a_max_cp_ul: %d\n",config->T2a_max_cp_ul);
    } else if (strcmp(key, KEY_T2A_MIN_UP ) == 0) {
        config->T2a_min_up = atoi(value);
        printf("T2a_min_up: %d\n",config->T2a_min_up);
    } else if (strcmp(key, KEY_T2A_MAX_UP ) == 0) {
        config->T2a_max_up = atoi(value);
        printf("T2a_max_up: %d\n",config->T2a_max_up);
    } else if (strcmp(key, KEY_TA3_MIN ) == 0) {
        config->Ta3_min = atoi(value);
        printf("Ta3_min: %d\n",config->Ta3_min);
    } else if (strcmp(key, KEY_TA3_MAX ) == 0) {
        config->Ta3_max = atoi(value);
        printf("Ta3_max: %d\n",config->Ta3_max);
    } else if (strcmp(key, KEY_T1A_MIN_CP_DL ) == 0) {
        config->T1a_min_cp_dl = atoi(value);
        printf("T1a_min_cp_dl: %d\n",config->T1a_min_cp_dl);
    } else if (strcmp(key, KEY_T1A_MAX_CP_DL ) == 0) {
        config->T1a_max_cp_dl = atoi(value);
        printf("T1a_max_cp_dl: %d\n",config->T1a_max_cp_dl);
    } else if (strcmp(key, KEY_T1A_MIN_CP_UL ) == 0) {
        config->T1a_min_cp_ul = atoi(value);
        printf("T1a_min_cp_ul: %d\n",config->T1a_min_cp_ul);
    } else if (strcmp(key, KEY_T1A_MAX_CP_UL ) == 0) {
        config->T1a_max_cp_ul = atoi(value);
        printf("T1a_max_cp_ul: %d\n",config->T1a_max_cp_ul);
    } else if (strcmp(key, KEY_T1A_MIN_UP ) == 0) {
        config->T1a_min_up = atoi(value);
        printf("T1a_min_up: %d\n",config->T1a_min_up);
    } else if (strcmp(key, KEY_T1A_MAX_UP ) == 0) {
        config->T1a_max_up = atoi(value);
        printf("T1a_max_up: %d\n",config->T1a_max_up);
    } else if (strcmp(key, KEY_TA4_MIN ) == 0) {
        config->Ta4_min = atoi(value);
        printf("Ta4_min: %d\n",config->Ta4_min);
    } else if (strcmp(key, KEY_TA4_MAX ) == 0) {
        config->Ta4_max = atoi(value);
        printf("Ta4_max: %d\n",config->Ta4_max);
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
        printf("bbdevMode: %d\n",config->debugStopCount);
    } else if (strcmp(key, KEY_DYNA_SEC_ENA) == 0) {
        config->DynamicSectionEna = atoi(value);
        printf("DynamicSectionEna: %d\n",config->DynamicSectionEna);
    } else if (strcmp(key, KEY_CP_VTAG ) == 0) {
        config->cp_vlan_tag = atoi(value);
        printf("cp_vlan_tag: %d\n",config->cp_vlan_tag);
    } else if (strcmp(key, KEY_UP_VTAG ) == 0) {
        config->up_vlan_tag = atoi(value);
        printf("up_vlan_tag: %d\n",config->up_vlan_tag);
    } else {
        printf("Unsupported configuration key [%s]\n", key);
        return -1;
    }

    return 0;
}

int parseConfigFile(char *filename, RuntimeConfig *config)
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

        if (inputLine[strlen(inputLine)-1] == '\n')
            inputLine[strlen(inputLine)-1] == '\0';

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

                if (fillConfigStruct(config, key, value) != 0) {
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
