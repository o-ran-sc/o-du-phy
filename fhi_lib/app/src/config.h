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

#ifndef _SAMPLEAPP__CONFIG_H_
#define _SAMPLEAPP__CONFIG_H_

#include <stdint.h>
#include <rte_ether.h>
#include "xran_fh_o_du.h"
#include "xran_pkt.h"


typedef struct ant_files_perMu
{
    char ant_file[XRAN_MAX_SECTOR_NR*XRAN_MAX_ANTENNA_NR][512]; /**<  file to use for test vector */
    char prach_file[XRAN_MAX_SECTOR_NR*XRAN_MAX_ANTENNA_NR][512]; /**<  file to use for test vector */

    char dl_bfw_file [XRAN_MAX_SECTOR_NR*XRAN_MAX_ANTENNA_NR][512]; /**< file with beamforming weights for DL streams */
    char ul_bfw_file [XRAN_MAX_SECTOR_NR*XRAN_MAX_ANTENNA_NR][512]; /**< file with beamforming weights for UL streams */

    char ul_srs_file [XRAN_MAX_SECTOR_NR*XRAN_MAX_ANT_ARRAY_ELM_NR][512]; /**< file with SRS content for UL antenna elements */
    char dl_csirs_file [XRAN_MAX_SECTOR_NR*XRAN_MAX_CSIRS_PORTS][512]; /**< file with CSIRS content for CSI-RS ports */

} ant_files_perMu;

/** Run time configuration of application */
typedef struct _RuntimeConfig
{
    uint8_t appMode;      /**< Application mode: lls-CU or RU  */
    uint8_t xranTech;     /**< Radio Access Technology (NR or LTE) */
    uint8_t xranCat;      /**< xran mode: NR Categoty A, NR Category B, LTE Cat A, LTE Cat B */
    uint8_t numCC;        /**< Number of CC per ports supported by RU */
    uint8_t numAxc;       /**< Number of Antenna Carriers per CC */
    uint8_t numUlAxc;     /**< Number of Antenna Carriers per CC for UL (Cat B) */
    uint32_t antElmTRx;   /**< Number of antenna elements for TX and RX */
    uint32_t muMimoUEs;   /**< Number of UEs (with 1 RX ant)/beams */

    uint32_t o_xu_id;     /**< id of O-DU|O-RU with in use case scenario */

    uint32_t DlLayersPerUe; /**< Number of DL layer per UE */
    uint32_t UlLayersPerUe; /**< Number of UL layer per UE */

    uint32_t ttiPeriod;   /**< TTI period */
    uint32_t testVect;    /**< Test Signal to send */
    struct rte_ether_addr o_du_addr[XRAN_VF_MAX]; /**<  O-DU Ethernet Mac Address */
    struct rte_ether_addr o_ru_addr[XRAN_VF_MAX]; /**<  O-RU Ethernet Mac Address */
    struct rte_ether_addr tmp_addr; /**<  Temp Ethernet Mac Address */

    uint32_t mtu; /**< maximum transmission unit (MTU) is the size of the largest protocol data unit (PDU) that can be communicated in a single
                       xRAN network layer transaction. supported 1500 bytes and 9600 bytes (Jumbo Frame) */
    uint32_t nPuschUpMultiSectonSend; /**< multi-section pusch in one up message. Test pusch/pucch multi-section receiver used*/
    int numSlots;  /**< number of slots in IQ vector */
    // char ant_file[XRAN_MAX_SECTOR_NR*XRAN_MAX_ANTENNA_NR][512]; /**<  file to use for test vector */
    // char prach_file[XRAN_MAX_SECTOR_NR*XRAN_MAX_ANTENNA_NR][512]; /**<  file to use for test vector */
    ant_files_perMu ant_perMu[XRAN_MAX_NUM_MU];
    // char dl_bfw_file [XRAN_MAX_SECTOR_NR*XRAN_MAX_ANTENNA_NR][512]; /**< file with beamforming weights for DL streams */
    // char ul_bfw_file [XRAN_MAX_SECTOR_NR*XRAN_MAX_ANTENNA_NR][512]; /**< file with beamforming weights for UL streams */

    // char ul_srs_file [XRAN_MAX_SECTOR_NR*XRAN_MAX_ANT_ARRAY_ELM_NR][512]; /**< file with SRS content for UL antenna elements */

    /* prach config */
    uint8_t prachOffset; /**< Sets the PRACH position in frequency / subcarrier position, n_PRBoffset^RA and is expressed as a physical resource block number.
                              Set by SIB2, prach-FreqOffset in E-UTRA. */

    uint8_t iqswap;          /**< do swap of IQ before send to ETH */
    uint8_t nebyteorderswap; /**< do swap of byte order from host byte order to network byte order. ETH */
    uint8_t compression;     /**< enable use case with compression */
    uint8_t CompHdrType;     /**< dynamic or static compression header */
    uint8_t prachCompMethod;     /**< compression enable for PRACH */
    uint8_t prachiqWidth;     /**< IQ width for PRACH */

    uint16_t totalBfWeights; /**< The total number of beamforming weights on RU */
    uint32_t DropPacketsUp; /**< enable droping the up channel packets if they miss timing window */
    uint8_t  enableSrs;     /**< enable SRS (valid for Cat B only) */
    uint16_t srsSymMask;    /* deprecated */
    uint16_t srsSlot;       /**< SRS slot within TDD period (special slot), for O-RU emulation */
    uint8_t  srsNdmOffset;  /**< tti offset to delay the transmission of NDM SRS UP, for O-RU emulation */
    uint16_t srsNdmTxDuration; /**< symbol duration for NDM SRS UP transmisson, for O-RU emulation */

    uint8_t puschMaskEnable; /**< enable PUSCH mask, which means not tranfer PUSCH in some UL slot */
    uint8_t puschMaskSlot; /**< PUSCH channel will not tranfer in slot module Frame */
    uint8_t extType;

    uint16_t maxFrameId; /**< max value of frame id */
    xran_fh_per_mu_cfg perMu[XRAN_MAX_NUM_MU];

    uint8_t enableCP;    /**<  enable C-plane */

    int32_t debugStop;
    int32_t debugStopCount;
    int32_t bbdevMode;
    int32_t DynamicSectionEna;
    int32_t GPS_Alpha;
    int32_t GPS_Beta;

    uint8_t  numMu;
    uint8_t  mu_number[XRAN_MAX_NUM_MU];       /**< Mu number as per 3GPP */
    uint32_t nDLAbsFrePointA; /**< Abs Freq Point A of the Carrier Center Frequency for in KHz Value: 450000->52600000 */
    uint32_t nULAbsFrePointA; /**< Abs Freq Point A of the Carrier Center Frequency for in KHz Value: 450000->52600000 */
    // uint32_t nDLBandwidth;    /**< Carrier bandwidth for in MHz. Value: 5->400 */
    // uint32_t nULBandwidth;    /**< Carrier bandwidth for in MHz. Value: 5->400 */


    uint8_t nFrameDuplexType;
    uint8_t nTddPeriod;
    struct xran_slot_config sSlotConfig[XRAN_MAX_TDD_PERIODICITY];

    struct xran_prb_map* p_PrbMapDl[XRAN_MAX_NUM_MU];
    struct xran_prb_map* p_PrbMapUl[XRAN_MAX_NUM_MU];
    struct xran_prb_map* p_PrbMapSrs;
    struct xran_prb_map* p_PrbMapCsiRs;

    uint8_t dssEnable;      /**< enable DSS (extension-9) */
    uint8_t dssPeriod;      /**< DSS pattern period for LTE/NR */
    uint8_t technology[XRAN_MAX_DSS_PERIODICITY];   /**< technology array represents slot is LTE(0)/NR(1) */

    uint16_t SlotPrbCCmask[XRAN_DIR_MAX][XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTIONS_PER_SLOT];
    uint64_t SlotPrbAntCMask[XRAN_DIR_MAX][XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTIONS_PER_SLOT];

    uint16_t SlotSrsPrbCCmask[XRAN_DIR_MAX][XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTIONS_PER_SLOT];
    uint64_t SlotSrsPrbAntCMask[XRAN_DIR_MAX][XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTIONS_PER_SLOT];

    struct xran_prb_map* p_SlotPrbMap[XRAN_DIR_MAX][XRAN_N_FE_BUF_LEN];
    struct xran_prb_map* p_SlotSrsPrbMap[XRAN_DIR_MAX][XRAN_N_FE_BUF_LEN];

    int32_t RunSlotPrbMapEnabled;
    struct xran_prb_map* p_RunSlotPrbMap[XRAN_DIR_MAX][XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    struct xran_prb_map* p_RunSrsSlotPrbMap[XRAN_DIR_MAX][XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANT_ARRAY_ELM_NR];

    int32_t DU_Port_ID_bitwidth;
    int32_t BandSector_ID_bitwidth;
    int32_t CC_ID_bitwidth;
    int32_t RU_Port_ID_bitwidth;
    struct o_xu_buffers *p_buff;

    int32_t SlotNum_fileEnabled;
    char SlotNum_file[XRAN_DIR_MAX][XRAN_N_FE_BUF_LEN][512]; /**<  file to use for test vector */

    uint16_t max_sections_per_slot;
    uint16_t max_sections_per_symbol;
    int32_t RunSlotPrbMapBySymbolEnable;

    /*NPRACH Parameters to use only in case of NB-IOT*/
    uint8_t    nprachformat;
    uint16_t   periodicity;
    uint16_t   startTime;
    uint8_t    suboffset;
    uint8_t    numSubCarriers;
    uint8_t    nRep; /*Repetitions*/

    uint8_t  csirsEnable;     /**< enable CSI-RS (valid for Cat B only) */
    uint8_t  nCsiPorts;

} RuntimeConfig;

/** use case configuration  */
typedef struct _UsecaseConfig
{
    uint8_t  oXuNum;        /**< Number of O-RU/O-DU connected to this instance */
    uint8_t  numSecMu[XRAN_PORTS_NUM];         /**< Number of numerologies in mixedNumerology case */
    uint8_t  mixedMu[XRAN_MAX_NUM_MU];
    uint8_t  appMode;       /**< Application mode: O-DU or O-RU  */

    uint32_t instance_id;  /**< Instance ID of application */
    uint32_t main_core;    /**< Core used for main() */
    uint32_t io_core;      /**< Core used for IO */
    uint64_t io_worker;           /**< Mask for worker cores 0-63 */
    uint64_t io_worker_64_127;    /**< Mask for worker cores 64-127 */
    int32_t  io_sleep;     /**< Enable sleep on PMD cores */
    uint32_t system_core;  /**< System core */
    int32_t  iova_mode;    /**< DPDK IOVA Mode */
    int32_t  dpdk_mem_sz;  /**< Total DPDK memory size */

    int32_t EthLinkSpeed;    /**< Ethernet Physical Link speed per O-RU: 10,25,40,100 >*/
    int32_t EthLinesNumber;  /**< 1, 2, 3 total number of links per O-RU (Fronthaul Ethernet link) */
    int32_t one_vf_cu_plane;  /**< 1 - C-plane and U-plane use one VF */
    uint16_t owdmInitEn;     /**< One Way Delay Measurement Initiator if set, Recipient if clear */
    uint16_t owdmMeasMeth;   /**< One Way Delay Measurement Method:0  REQUEST, 1 REM_REQ, 2 REQ_WFUP, 3 REM_REQ_WFUP */
    uint16_t owdmNumSamps;   /**< One Way Delay Measurement number of samples per test */
    uint16_t owdmFltType;    /**< One Way Delay Measurement Filter Type 0: Simple Average */
    uint64_t owdmRspTo;      /**< One Way Delay Measurement Response Time Out in ns */
    uint16_t owdmMeasState;  /**< One Way Delay Measurement State 0:INIT, 1:IDLE, 2:ACTIVE, 3:DONE */
    uint16_t owdmMeasId;     /**< One Way Delay Measurement Id, Seed for the measurementId to be used */
    uint16_t owdmEnable;     /**< One Way Delay Measurement master enable when set performs measurements on all vfs */
    uint16_t owdmPlLength;   /**< One Way Delay Measurement Payload length   44<= PiLength <= 1400 bytes */

    int num_vfs;  /**< Total numbers of VFs accrose all O-RU|O-DU */
    int num_rxq;  /**< Total numbers of HW RX queues for each VF O-RU|O-DU */

    struct rte_ether_addr remote_o_xu_addr[XRAN_PORTS_NUM][XRAN_VF_MAX]; /**<  O-DU Ethernet Mac Address */
    struct rte_ether_addr remote_o_xu_addr_copy[XRAN_VF_MAX];            /**<  Temp Ethernet Mac Address */

    char o_xu_cfg_file [XRAN_PORTS_NUM][512]; /**< file with config for each O-XU */
    char o_xu_mixed_num_file[XRAN_PORTS_NUM][XRAN_MAX_NUM_MU][512]; /**< file for secondary numerologies*/
    char o_xu_pcie_bus_addr[XRAN_PORTS_NUM][XRAN_VF_MAX][512]; /**<  VFs used for each O-RU|O-DU */


    char o_xu_bbu_cfg_file[512]; /**< file with config for each O-XU */

    char prefix_name[256];
    uint8_t     dlCpProcBurst; /**< When set to 1, dl cp processing will be done on single symbol. When set to 0, DL CP processing
                                     will be spread across all allowed symbols and multiple cores to reduce burstiness */
    int32_t  bbu_offload;     /**< enable packet handling on BBU cores */
    int32_t  mlogxrandisable;  /**< set to 1 to disable mlog 0 - default mlog enabled */
    
    /* Config for IEEE 802.1Q Connectivity and fault management - LBM/LBR */
    bool lbmEnable;
    uint16_t numRetransmissions;
    uint16_t LBRTimeOut;
    uint16_t LBMPeriodicity;

} UsecaseConfig;

/**
 * Parse application configuration file.
 *
 * @param filename The name of the configuration file to be parsed.
 * @param config The configuration structure to be filled with parsed data. */
int parseConfigFile(const char *filename, RuntimeConfig *config);

/**
 * Parse application use case  file.
 *
 * @param filename The name of the use case file to be parsed.
 * @param config The configuration structure to be filled with parsed data. */
int parseUsecaseFile(const char *filename, UsecaseConfig *config);

/**
 * Parse slot config file.
 *
 * @param dir folder name.
 * @param config The configuration structure to be filled with parsed data. */
int32_t parseSlotConfigFile(const char *dir, RuntimeConfig *config);
int32_t config_init(RuntimeConfig *p_o_xu_cfg);
int32_t config_init2(RuntimeConfig *p_o_xu_cfg);
struct xran_prb_map* config_malloc_prb_map(void);

#endif /* _SAMPLEAPP__CONFIG_H_ */
