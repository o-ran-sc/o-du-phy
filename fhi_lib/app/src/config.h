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

#ifndef _SAMPLEAPP__CONFIG_H_
#define _SAMPLEAPP__CONFIG_H_

#include <stdint.h>
#include <rte_ether.h>

/** Run time configuration of application */
typedef struct _RuntimeConfig
{
    uint8_t appMode;      /**< Application mode: lls-CU or RU  */
    uint8_t xranMode;     /**< xran mode: Categoty A | Category B */
    uint8_t numCC;        /**< Number of CC per ports supported by RU */
    uint8_t mu_number;    /**< Mu numner as per 3GPP */
    uint8_t numAxc;       /**< Number of Antenna Carriers per CC */
    uint32_t ttiPeriod;   /**< TTI period */
    uint32_t testVect;    /**< Test Signal to send */
    struct ether_addr lls_cu_addr; /**<  lls-CU Ethernet Mac Address */
    struct ether_addr ru_addr; /**<  RU Ethernet Mac Address */
    char ant_file[16][512] /**<  file to use for test vector */ ;

    /* prach config */
    uint8_t enablePrach; /**<  enable PRACH */
    uint8_t prachOffset; /**< Sets the PRACH position in frequency / subcarrier position, n_PRBoffset^RA and is expressed as a physical resource block number.
                              Set by SIB2, prach-FreqOffset in E-UTRA. */

    uint8_t prachConfigIndex; /**< TS36.211 - Table 5.7.1-2 : PRACH Configuration Index */
    uint8_t iqswap; /**< do swap of IQ before send to ETH */
    uint8_t nebyteorderswap; /**< do swap of byte order from host byte order to network byte order. ETH */

    uint16_t Tadv_cp_dl;
    uint16_t T2a_min_cp_dl;
    uint16_t T2a_max_cp_dl;
    uint16_t T2a_min_cp_ul;
    uint16_t T2a_max_cp_ul;
    uint16_t T2a_min_up;
    uint16_t T2a_max_up;
    uint16_t Ta3_min;
    uint16_t Ta3_max;
    uint16_t T1a_min_cp_dl;
    uint16_t T1a_max_cp_dl;
    uint16_t T1a_min_cp_ul;
    uint16_t T1a_max_cp_ul;
    uint16_t T1a_min_up;
    uint16_t T1a_max_up;
    uint16_t Ta4_min;
    uint16_t Ta4_max;

    uint8_t enableCP;    /**<  enable C-plane */
    uint8_t cp_vlan_tag; /**<  C-plane vlan tag */
    uint8_t up_vlan_tag; /**<  U-plane vlan tag */

    int32_t debugStop;


} RuntimeConfig;

/**
 * Parse application configuration file.
 *
 * @param filename The name of the configuration file to be parsed.
 * @param config The configuration structure to be filled with parsed data. */
int parseConfigFile(char *filename, RuntimeConfig *config);

#endif /* _SAMPLEAPP__CONFIG_H_ */
