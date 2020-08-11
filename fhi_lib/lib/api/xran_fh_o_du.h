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
 * @brief This file provides public interface to xRAN Front Haul layer implementation as defined in the
 *      ORAN-WG4.CUS.0-v01.00 spec. Implementation specific to
 *      Lower Layer Split Central Unit (O-DU): a logical node that includes the eNB/gNB functions as
 *      listed in section 2.1 split option 7-2x, excepting those functions allocated exclusively to the O-RU.
 *      The O-DU controls the operation of O-RUs for 5G NR Radio Access technology
 *
 * @file xran_fh_o_du.h
 * @ingroup group_lte_source_xran
 * @author Intel Corporation
 *
 **/

#ifndef _XRAN_FH_O_DU_H_
#define _XRAN_FH_O_DU_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>

#define XRAN_STATUS_SUCCESS (0)
/**<
 *  @ingroup xran
 *   Success status value. */
#define XRAN_STATUS_FAIL (-1)
/**<
 *  @ingroup xran
 *   Fail status value. */

#define XRAN_STATUS_RETRY (-2)
/**<
 *  @ingroup xran
 *  Retry status value. */

#define XRAN_STATUS_RESOURCE (-3)
/**<
 *  @ingroup xran
 *  The resource that has been requested is unavailable. Refer
 *  to relevant sections of the API for specifics on what the suggested
 *  course of action is. */

#define XRAN_STATUS_INVALID_PARAM (-4)
/**<
 *  @ingroup xran
 *  Invalid parameter has been passed in. */
#define XRAN_STATUS_FATAL (-5)
/**<
 *  @ingroup xran
 *  A serious error has occurred. Recommended course of action
 *  is to shutdown and restart the component. */

#define XRAN_STATUS_UNSUPPORTED (-6)
/**<
 *  @ingroup xran
 *  The function is not supported, at least not with the specific
 *  parameters supplied.  This may be because a particular
 *  capability is not supported by the current implementation. */

#define XRAN_STATUS_INVALID_PACKET (-7)
/**<
 *  @ingroup xran
 *  Recevied packet does not have correct format. */

/** Macro to calculate TTI number from symbol index used by timing thread */
#define XranGetTtiNum(symIdx, numSymPerTti) (((uint32_t)symIdx / (uint32_t)numSymPerTti))
/** Macro to calculate Symbol number for given slot from symbol index  */
#define XranGetSymNum(symIdx, numSymPerTti) (((uint32_t)symIdx % (uint32_t)numSymPerTti))
/** Macro to calculate Frame number for given tti */
#define XranGetFrameNum(tti,SFNatSecStart,numSubFramePerSystemFrame, numSlotPerSubFrame)  ((((uint32_t)tti / ((uint32_t)numSubFramePerSystemFrame * (uint32_t)numSlotPerSubFrame)) + SFNatSecStart) & 0x3FF)
/** Macro to calculate Subframe number for given tti */
#define XranGetSubFrameNum(tti, numSlotPerSubFrame, numSubFramePerSystemFrame) (((uint32_t)tti/(uint32_t)numSlotPerSubFrame) % (uint32_t)numSubFramePerSystemFrame)
/** Macro to calculate Slot number */
#define XranGetSlotNum(tti, numSlotPerSfn) ((uint32_t)tti % ((uint32_t)numSlotPerSfn))

#define XRAN_PORTS_NUM               (1)    /**< number of XRAN ports (aka O-RU devices) supported */
#define XRAN_N_FE_BUF_LEN            (40)   /**< Number of TTIs (slots) */
#define XRAN_MAX_SECTOR_NR           (12)    /**< Max sectors per XRAN port */
#define XRAN_MAX_ANTENNA_NR          (16)   /**< Max number of extended Antenna-Carriers:
                                                a data flow for a single antenna (or spatial stream) for a single carrier in a single sector */

/* see 10.2	Hierarchy of Radiation Structure in O-RU (assume TX and RX pannel are the same dimensions)*/
#define XRAN_MAX_PANEL_NR            (1)   /**< Max number of Panels supported per O-RU */
#define XRAN_MAX_TRX_ANTENNA_ARRAY   (1)   /**< Max number of TX and RX arrays per panel in O-RU */
#define XRAN_MAX_ANT_ARRAY_ELM_NR    (64)  /**< Maximum number of Antenna Array Elemets in Antenna Array in the O-RU */



#define XRAN_NUM_OF_SYMBOL_PER_SLOT  (14) /**< Number of symbols per slot */
#define XRAN_MAX_NUM_OF_SRS_SYMBOL_PER_SLOT  XRAN_NUM_OF_SYMBOL_PER_SLOT /**< Max Number of SRS symbols per slot */
#define XRAN_MAX_TDD_PERIODICITY     (80) /**< Max TDD pattern period */
#define XRAN_MAX_CELLS_PER_PORT      (XRAN_MAX_SECTOR_NR) /**< Max cells mapped to XRAN port */
#define XRAN_COMPONENT_CARRIERS_MAX  (XRAN_MAX_SECTOR_NR) /**< number of CCs */
#define XRAN_NUM_OF_ANT_RADIO        (XRAN_MAX_SECTOR_NR*XRAN_MAX_ANTENNA_NR) /**< Max Number of Antennas supported for all CC on single XRAN port */
#define XRAN_MAX_PRBS                (275) /**< Max of PRBs per CC per antanna for 5G NR */

#define XRAN_MAX_SECTIONS_PER_SYM    (16)  /**< Max number of different sections in single symbol (section is equal to RB allocation for UE) */

#define XRAN_MAX_PKT_BURST (448+4) /**< 4x14x8 symbols per ms */
#define XRAN_N_MAX_BUFFER_SEGMENT XRAN_MAX_PKT_BURST /**< Max number of segments per ms */

#define XRAN_STRICT_PARM_CHECK               (1) /**< enable parameter check for C-plane */

/* Slot type definition */
#define XRAN_SLOT_TYPE_INVALID               (0) /**< invalid slot type */
#define XRAN_SLOT_TYPE_DL                    (1) /**< DL slot */
#define XRAN_SLOT_TYPE_UL                    (2) /**< UL slot */
#define XRAN_SLOT_TYPE_SP                    (3) /**< Special slot */
#define XRAN_SLOT_TYPE_FDD                   (4) /**< FDD slot */
#define XRAN_SLOT_TYPE_LAST                  (5) /**< MAX slot */

/* symbol type definition */
#define XRAN_SYMBOL_TYPE_DL                  (0) /**< DL symbol  */
#define XRAN_SYMBOL_TYPE_UL                  (1) /**< UL symbol  */
#define XRAN_SYMBOL_TYPE_GUARD               (2) /**< GUARD symbol */
#define XRAN_SYMBOL_TYPE_FDD                 (3) /**< FDD symbol */

#define XRAN_NUM_OF_SLOT_IN_TDD_LOOP         (80)/**< MAX number of slot for TDD repetition */

//#define _XRAN_DEBUG   /**< Enable debug log */
//#define _XRAN_VERBOSE /**< Enable verbose log */

#ifdef _XRAN_DEBUG
    #define xran_log_dbg(fmt, ...)          \
        fprintf(stderr,                     \
            "DEBUG: %s(%d): " fmt "\n",     \
            __FILE__,                       \
            __LINE__, ##__VA_ARGS__)
#else
    #define xran_log_dbg(fmt, ...)
#endif

#if defined(_XRAN_DEBUG) || defined(_XRAN_VERBOSE)
    #define xran_log_wrn(fmt, ...)          \
        fprintf(                            \
            stderr,                         \
            "WARNING: %s(%d): " fmt "\n",   \
            __FILE__,                       \
            __LINE__, ##__VA_ARGS__)
#else
    #define xran_log_dbg(fmt, ...)
    #define xran_log_wrn(fmt, ...)
#endif

#define xran_log_err(fmt, ...)          \
    fprintf(stderr,                     \
        "ERROR: %s(%d): " fmt "\n",     \
        __FILE__,                       \
        __LINE__, ##__VA_ARGS__)

enum XranFrameDuplexType
{
    XRAN_FDD = 0, XRAN_TDD
};

enum xran_if_state
{
    XRAN_INIT = 0,
    XRAN_RUNNING,
    XRAN_STOPPED
};

/**
 ******************************************************************************
 * @ingroup xran
 *
 * @description
 *      Compression Method 6.3.3.13, Table 6-43
 *****************************************************************************/
enum xran_compression_method {
    XRAN_COMPMETHOD_NONE        = 0,
    XRAN_COMPMETHOD_BLKFLOAT    = 1,
    XRAN_COMPMETHOD_BLKSCALE    = 2,
    XRAN_COMPMETHOD_ULAW        = 3,
    XRAN_COMPMETHOD_MODULATION  = 4,
    XRAN_COMPMETHOD_MAX
};

/**
 ******************************************************************************
 * @ingroup xran
 *
 * @description
 *      Callback function type for symbol packet enum
 *****************************************************************************/
enum callback_to_phy_id
{
    XRAN_CB_TTI = 0, /**< callback on TTI boundary */
    XRAN_CB_HALF_SLOT_RX =1, /**< callback on half slot (sym 7) packet arrival*/
    XRAN_CB_FULL_SLOT_RX =2, /**< callback on full slot (sym 14) packet arrival */
    XRAN_CB_MAX /**< max number of callbacks */
};

/**  Beamforming type, enumerated as "frequency", "time" or "hybrid"
     section 10.4.2	Weight-based dynamic beamforming */
enum xran_weight_based_beamforming_type {
    XRAN_BF_T_FREQUENCY = 0,
    XRAN_BF_T_TIME      = 1,
    XRAN_BF_T_HYBRID    = 2,
    XRAN_BF_T_MAX
};

typedef int32_t xran_status_t; /**< Xran status return value */

/** callback function type for Symbol packet */
typedef void (*xran_callback_sym_fn)(void*);

/** Callback function type for TTI event */
typedef int (*xran_fh_tti_callback_fn)(void*);

/** Callback function type packet arrival from transport layer (ETH or IP) */
typedef void (*xran_transport_callback_fn)(void*, xran_status_t);

/** Callback functions to poll BBdev encoder */
typedef int16_t (*phy_encoder_poll_fn)(void);

/** Callback functions to poll BBdev decoder */
typedef int16_t (*phy_decoder_poll_fn)(void);

/** XRAN port enum */
enum xran_vf_ports
{
    XRAN_UP_VF = 0, /**< port type for U-plane */
    XRAN_CP_VF,     /**< port type for C-plane */
    XRAN_UP_VF1,    /**< port type for U-plane */
    XRAN_CP_VF1,    /**< port type for C-plane */
    XRAN_UP_VF2,    /**< port type for U-plane */
    XRAN_CP_VF2,    /**< port type for C-plane */
    XRAN_UP_VF3,    /**< port type for U-plane */
    XRAN_CP_VF3,    /**< port type for C-plane */
    XRAN_UP_VF4,    /**< port type for U-plane */
    XRAN_CP_VF4,    /**< port type for C-plane */
    XRAN_UP_VF5,    /**< port type for U-plane */
    XRAN_CP_VF5,    /**< port type for C-plane */
    XRAN_UP_VF6,    /**< port type for U-plane */
    XRAN_CP_VF6,    /**< port type for C-plane */
    XRAN_UP_VF7,    /**< port type for U-plane */
    XRAN_CP_VF7,    /**< port type for C-plane */
    XRAN_VF_MAX
};

/** XRAN Radio Access technology enum */
enum xran_ran_tech
{
    XRAN_RAN_5GNR     = 0, /**< 5G NR */
    XRAN_RAN_LTE      = 1, /**< LTE   */
    XRAN_RAN_MAX
};

/** XRAN user data compression header handling types */
enum xran_comp_hdr_type
{
    XRAN_COMP_HDR_TYPE_DYNAMIC   = 0, /**< dynamic data format where U-plane udCompHdr controls compression parameters */
    XRAN_COMP_HDR_TYPE_STATIC    = 1, /**< static data format where M-plane defines compression parameters */
    XRAN_COMP_HDR_TYPE_MAX
};

/** XRAN category enum */
enum xran_category
{
    XRAN_CATEGORY_A     = 0, /**< 5G NR Category A */
    XRAN_CATEGORY_B     = 1, /**< 5G NR Category B */
    XRAN_CATEGORY_MAX
};

/** type of beamforming */
enum xran_beamforming_type
{
    XRAN_BEAM_ID_BASED = 0, /**< beam index based */
    XRAN_BEAM_WEIGHT,       /**< beam forming weights */
    XRAN_BEAM_ATTRIBUTE,    /**< beam index based */
};

/** state of bbdev with xran */
enum xran_bbdev_init
{
    XRAN_BBDEV_NOT_USED    = -1, /**< BBDEV is disabled */
    XRAN_BBDEV_MODE_HW_OFF = 0,  /**< BBDEV is enabled for SW sim mode */
    XRAN_BBDEV_MODE_HW_ON  = 1,  /**< BBDEV is enable for HW */
    XRAN_BBDEV_MODE_MAX
};

/** callback return information */
struct xran_cb_tag {
    uint16_t cellId;
    uint32_t symbol;
    uint32_t slotiId;
};

/** DPDK IO configuration for XRAN layer */
struct xran_io_cfg {
    uint8_t id; /**< should be (0) for O-DU or (1) O-RU (debug) */
    uint8_t num_vfs; /**< number of VFs for C-plane and U-plane (should be even) */
    char *dpdk_dev[XRAN_VF_MAX]; /**< VFs devices  */
    char *bbdev_dev[1];      /**< BBDev dev name */
    int32_t bbdev_mode;      /**< DPDK for BBDev */
    uint32_t dpdkIoVaMode;   /**< IOVA Mode */
    uint32_t dpdkMemorySize; /**< DPDK max memory allocation */
    int32_t  core;            /**< reservd */
    int32_t  system_core;     /**< reservd */
    uint64_t pkt_proc_core;  /**< worker mask */
    int32_t  pkt_aux_core;    /**< reservd */
    int32_t  timing_core;     /**< core used by xRAN */
    int32_t  port[XRAN_VF_MAX];  /**< VFs ports */
    int32_t  io_sleep;        /**< enable sleep on PMD cores */
};

/** XRAN spec section 3.1.3.1.6 ecpriRtcid / ecpriPcid define */
struct xran_eaxcid_config {
    uint16_t mask_cuPortId;     /**< Mask CU PortId */
    uint16_t mask_bandSectorId; /**< Mask Band */
    uint16_t mask_ccId;         /**< Mask CC */
    uint16_t mask_ruPortId;     /**< Mask RU Port ID */

    uint8_t bit_cuPortId;       /**< bandsectorId + ccId + ruportId */
    uint8_t bit_bandSectorId;   /**< ccId + ruPortId */
    uint8_t bit_ccId;           /**< ruportId */
    uint8_t bit_ruPortId;       /**< 0 */
};

/**
* XRAN Front haul interface initialization settings
*/
struct xran_fh_init {
    struct xran_io_cfg io_cfg;/**< DPDK IO for XRAN */
    struct xran_eaxcid_config eAxCId_conf; /**< config of ecpriRtcid/ecpriPcid */

    uint32_t dpdkBasebandFecMode; /**< DPDK Baseband FEC device mode (0-SW, 1-HW) */
    char *dpdkBasebandDevice;     /**< DPDK Baseband device address */
    char *filePrefix;             /**< DPDK prefix */

    uint32_t mtu; /**< maximum transmission unit (MTU) is the size of the largest protocol data unit (PDU) that can be communicated in a single
                       xRAN network layer transaction. supported 1500 bytes and 9600 bytes (Jumbo Frame) */
    int8_t *p_o_du_addr;  /**<  O-DU Ethernet Mac Address */
    int8_t *p_o_ru_addr;  /**<  O-RU Ethernet Mac Address */

    uint16_t totalBfWeights;/**< The total number of beamforming weights on RU for extensions */

    uint16_t Tadv_cp_dl;    /**< Table 2 7 : xRAN Delay Management Model Parameters */
    uint16_t T2a_min_cp_dl; /**< Table 2 7 : xRAN Delay Management Model Parameters */
    uint16_t T2a_max_cp_dl; /**< Table 2 7 : xRAN Delay Management Model Parameters */
    uint16_t T2a_min_cp_ul; /**< Table 2 7 : xRAN Delay Management Model Parameters */
    uint16_t T2a_max_cp_ul; /**< Table 2 7 : xRAN Delay Management Model Parameters */
    uint16_t T2a_min_up;    /**< Table 2 7 : xRAN Delay Management Model Parameters */
    uint16_t T2a_max_up;    /**< Table 2 7 : xRAN Delay Management Model Parameters */
    uint16_t Ta3_min;       /**< Table 2 7 : xRAN Delay Management Model Parameters */
    uint16_t Ta3_max;       /**< Table 2 7 : xRAN Delay Management Model Parameters */
    uint16_t T1a_min_cp_dl; /**< Table 2 7 : xRAN Delay Management Model Parameters */
    uint16_t T1a_max_cp_dl; /**< Table 2 7 : xRAN Delay Management Model Parameters */
    uint16_t T1a_min_cp_ul; /**< Table 2 7 : xRAN Delay Management Model Parameters */
    uint16_t T1a_max_cp_ul; /**< Table 2 7 : xRAN Delay Management Model Parameters */
    uint16_t T1a_min_up;    /**< Table 2 7 : xRAN Delay Management Model Parameters */
    uint16_t T1a_max_up;    /**< Table 2 7 : xRAN Delay Management Model Parameters */
    uint16_t Ta4_min;       /**< Table 2 7 : xRAN Delay Management Model Parameters */
    uint16_t Ta4_max;       /**< Table 2 7 : xRAN Delay Management Model Parameters */

    uint8_t enableCP;       /**<  enable C-plane */
    uint8_t prachEnable;    /**<  enable PRACH   */
    uint8_t srsEnable;      /**<  enable SRS (Cat B specific) */
    uint8_t cp_vlan_tag;    /**<  C-plane vlan tag */
    uint8_t up_vlan_tag;    /**<  U-plane vlan tag */
    int32_t debugStop;      /**<  enable auto stop */
    int32_t debugStopCount;      /**<  enable auto stop after number of Tx packets */
    int32_t DynamicSectionEna; /**<  enable dynamic C-Plane section allocation */
    int32_t GPS_Alpha;  // refer to alpha as defined in section 9.7.2 of ORAN spec. this value should be alpha*(1/1.2288ns), range 0 - 1e7 (ns)
    int32_t GPS_Beta;   //beta value as defined in section 9.7.2 of ORAN spec. range -32767 ~ +32767
};

/** Beamforming waights for single stream for each PRBs  given number of Antenna elements */
struct xran_cp_bf_weight{
    int16_t nAntElmTRx;        /**< num TRX for this allocation */
    int8_t*  p_ext_start;      /**< pointer to start of buffer for full C-plane packet */
    int8_t*  p_ext_section;    /**< pointer to form extType */
    int16_t  ext_section_sz;   /**< extType section size */
};
struct xran_cp_bf_attribute{
    int16_t weight[4];
};
struct xran_cp_bf_precoding{
    int16_t weight[4];
};

/** section descriptor for given number of PRBs used on U-plane packet creation */
struct xran_section_desc {
    uint16_t section_id; /**< section id used for this element */

    int16_t iq_buffer_offset;    /**< Offset in bytes for the content of IQs with in main symb buffer */
    int16_t iq_buffer_len;       /**< Length in bytes for the content of IQs with in main symb buffer */

    uint8_t *pData;      /**< optional pointer to data buffer */
    void    *pCtrl;      /**< optional poitner to mbuf */
};

/** PRB element structure */
struct xran_prb_elm {
    int16_t nRBStart;    /**< start RB of RB allocation */
    int16_t nRBSize;     /**< number of RBs used */
    int16_t nStartSymb;  /**< start symbol ID */
    int16_t numSymb;     /**< number of symbols */
    int16_t nBeamIndex;  /**< beam index for given PRB */
    int16_t bf_weight_update; /** need to update beam weights or not */
    int16_t compMethod;  /**< compression index for given PRB */
    int16_t iqWidth;     /**< compression bit width for given PRB */
    int16_t BeamFormingType; /**< index based, weights based or attribute based beam forming*/

    struct xran_section_desc * p_sec_desc[XRAN_NUM_OF_SYMBOL_PER_SLOT]; /**< section desctiptors to U-plane data given RBs */
    struct xran_cp_bf_weight   bf_weight; /**< beam forming information relevant for given RBs */

    union {
        struct xran_cp_bf_attribute bf_attribute;
        struct xran_cp_bf_precoding bf_precoding;
    };
};

/** PRB map structure */
struct xran_prb_map {
    uint8_t   dir;        /**< DL or UL direction */
    uint8_t   xran_port;  /**< xran id of given RU [0-(XRAN_PORTS_NUM-1)] */
    uint16_t  band_id;    /**< xran band id */
    uint16_t  cc_id;      /**< componnent carrier id [0 - (XRAN_MAX_SECTOR_NR-1)] */
    uint16_t  ru_port_id; /**< RU device antenna port id [0 - (XRAN_MAX_ANTENNA_NR-1) */
    uint16_t  tti_id;     /**< xRAN slot id [0 - (max tti-1)] */
    uint8_t   start_sym_id;     /**< start symbol Id [0-13] */
    uint32_t  nPrbElm;    /**< total number of PRB elements for given map [0- (XRAN_MAX_PRBS-1)] */
    struct xran_prb_elm prbMap[XRAN_MAX_PRBS];


};

/* PRACH config required for XRAN based FH */
struct xran_prach_config
{
    /* PRACH config*/
    uint8_t      nPrachConfIdx;  /**< PRACH Configuration Index*/
    uint8_t      nPrachSubcSpacing;
    /**< PRACH Sub-carrier spacing
    Value:0->1
    For below 6GHz the values indicate 15kHz or 30kHz
    For above 6GHz the values indicate 60kHz or 120kHz*/
    uint8_t      nPrachZeroCorrConf; /**< PRACH zeroCorrelationZoneConfig */
    uint8_t      nPrachRestrictSet;  /**< PRACH restrictedSetConfig */
    uint16_t     nPrachRootSeqIdx; /**< PRACH Root Sequence Index */
    uint16_t     nPrachFreqStart;  /**< PRACH prach-frequency-start  */
    int32_t      nPrachFreqOffset; /**< PRACH prach-frequency-offset */
    uint8_t      nPrachFilterIdx;  /**< PRACH Filter index */
};

/**< SRS configuration required for XRAN based FH */
struct xran_srs_config {
    uint16_t   symbMask;    /**< symbols used for SRS with in U/S slot [bits 0-13] */
    uint8_t    eAxC_offset; /**< starting value of eAxC for SRS packets */
};

/** XRAN slot configuration */
struct xran_slot_config {
    uint8_t nSymbolType[XRAN_NUM_OF_SYMBOL_PER_SLOT]; /**< Defines the Symbol type for all 14 symbols in a slot. 0: DL, 1: UL, 2: Guard */
    uint8_t reserved[2];
};

/** XRAN front haul frame config */
struct xran_frame_config {
    uint8_t      nFrameDuplexType; /**< Frame Duplex type:  0 -> FDD, 1 -> TDD */
    uint8_t      nNumerology; /**< Numerology, determine sub carrier spacing, Value: 0->4
                                   0: 15khz,  1: 30khz,  2: 60khz
                                   3: 120khz, 4: 240khz */
    uint8_t      nTddPeriod;  /**< TDD period */
    struct xran_slot_config sSlotConfig[XRAN_MAX_TDD_PERIODICITY];
    /**< TDD Slot configuration - If nFrameDuplexType = TDD(1), then this config defines the slot config type for each slot.*/
    /* The number of slots need to be equal to nTddPeriod */
};

/** XRAN-PHY interface byte order */
enum xran_input_byte_order {
    XRAN_NE_BE_BYTE_ORDER = 0, /**< Network byte order (Big endian), xRAN lib doesn't do swap */
    XRAN_CPU_LE_BYTE_ORDER     /**< CPU byte order (Little endian), xRAN lib does do swap */
};

/** XRAN-PHY interface I and Q order */
enum xran_input_i_q_order {
    XRAN_I_Q_ORDER = 0,  /**< I , Q */
    XRAN_Q_I_ORDER       /**< Q , I */
};

/** XRAN front haul O-RU settings */
struct xran_ru_config {
    enum xran_ran_tech      xranTech;      /**< 5GNR or LTE */
    enum xran_category      xranCat;       /**< mode: Catergory A or Category B */
    enum xran_comp_hdr_type xranCompHdrType;   /**< dynamic or static udCompHdr handling*/
    uint8_t                 iqWidth;           /**< IQ bit width */
    uint8_t                 compMeth;      /**< Compression method */
    uint8_t                 fftSize;       /**< FFT Size */
    enum xran_input_byte_order byteOrder; /**< Order of bytes in int16_t in buffer. Big or little endian */
    enum xran_input_i_q_order  iqOrder;   /**< order of IQs in the buffer */
    uint16_t xran_max_frame;   /**< max frame number supported */
};

/**
 * @ingroup xran
 * XRAN front haul general configuration */
struct xran_fh_config {
    uint32_t            dpdk_port; /**< DPDK port number used for FH */
    uint32_t            sector_id; /**< Band sector ID for FH */
    uint32_t            nCC;       /**< number of Component carriers supported on FH */
    uint32_t            neAxc;     /**< number of eAxc supported on one CC*/
    uint32_t            neAxcUl;     /**< number of eAxc supported on one CC for UL direction */
    uint32_t            nAntElmTRx;  /**< Number of antenna elements for TX and RX */
    uint16_t            nDLFftSize;  /**< DL FFT size */
    uint16_t            nULFftSize;  /**< UL FFT size */
    uint16_t            nDLRBs;      /**< DL PRB  */
    uint16_t            nULRBs;      /**< UL PRB  */
    uint32_t            nDLAbsFrePointA; /**< Abs Freq Point A of the Carrier Center Frequency for in KHz Value: 450000->52600000 */
    uint32_t            nULAbsFrePointA; /**< Abs Freq Point A of the Carrier Center Frequency for in KHz Value: 450000->52600000 */
    uint32_t            nDLCenterFreqARFCN;   /**< center frerquency for DL in MHz */
    uint32_t            nULCenterFreqARFCN;   /**< center frerquency for UL in MHz */
    xran_fh_tti_callback_fn     ttiCb;        /**< call back for TTI event */
    void                *ttiCbParam;  /**< parameters of call back function */

    struct xran_prach_config     prach_conf;   /**< PRACH specific configurations for FH */
    struct xran_srs_config       srs_conf;     /**< SRS specific configurations for FH */
    struct xran_frame_config     frame_conf;   /**< frame config */
    struct xran_ru_config        ru_conf;      /**< config of RU as per XRAN spec */

    phy_encoder_poll_fn bbdev_enc; /**< call back to poll BBDev encoder */
    phy_decoder_poll_fn bbdev_dec; /**< call back to poll BBDev decoder */

    uint16_t tx_cp_eAxC2Vf[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR*2 + XRAN_MAX_ANT_ARRAY_ELM_NR]; /**< mapping of C-Plane (ecpriRtcid) or U-Plane (ecpriPcid) to VF */
    uint16_t tx_up_eAxC2Vf[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR*2 + XRAN_MAX_ANT_ARRAY_ELM_NR]; /**< mapping of C-Plane (ecpriRtcid) or U-Plane (ecpriPcid) to VF */

    uint16_t rx_cp_eAxC2Vf[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR*2 + XRAN_MAX_ANT_ARRAY_ELM_NR]; /**< mapping of C-Plane (ecpriRtcid) or U-Plane (ecpriPcid) to VF */
    uint16_t rx_up_eAxC2Vf[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR*2 + XRAN_MAX_ANT_ARRAY_ELM_NR]; /**< mapping of C-Plane (ecpriRtcid) or U-Plane (ecpriPcid) to VF */

    uint32_t log_level; /**< configuration of log level */
};

/**
 * @ingroup xran
 * XRAN front haul statistic counters according to Table 7 1 : Common Counters for both DL and UL */
struct xran_common_counters{
    uint64_t Rx_on_time;      /**< Data was received on time (applies to user data reception window) */
    uint64_t Rx_early;        /**< Data was received too early (applies to user data reception window) */
    uint64_t Rx_late;         /**< Data was received too late (applies to user data reception window) */
    uint64_t Rx_corrupt;      /**< Corrupt/Incorrect header packet */
    uint64_t Rx_pkt_dupl;     /**< Duplicated packet */
    uint64_t Total_msgs_rcvd; /**< Total messages received (on all links) */

    /* debug statistis */
    uint64_t rx_counter;
    uint64_t tx_counter;
    uint64_t tx_bytes_counter;
    uint64_t rx_bytes_counter;
    uint64_t tx_bytes_per_sec;
    uint64_t rx_bytes_per_sec;

    uint64_t rx_pusch_packets[XRAN_MAX_ANTENNA_NR];
    uint64_t rx_prach_packets[XRAN_MAX_ANTENNA_NR];
    uint64_t rx_srs_packets;
};

/**
 * @ingroup xran
 * CC instance handle pointer type */
typedef void * xran_cc_handle_t;

/**
 *****************************************************************************
 * @ingroup xran
 *
 * @description
 *      A flat buffer structure. The data pointer, pData, is a virtual address.
 *      The API requires the memory to by physically contiguous. Each flat
 *      buffer segment may contain several equally sized elements.
 *
 *****************************************************************************/
struct xran_flat_buffer
{
    uint32_t nElementLenInBytes;
    /**< The Element length specified in bytes.
     * This parameter specifies the size of a single element in the buffer.
     * The total size of the buffer is described as
     * bufferSize = nElementLenInBytes * nNumberOfElements */
    uint32_t nNumberOfElements;
    /**< The number of elements in the physical contiguous memory segment */
    uint32_t nOffsetInBytes;
    /**< Offset in bytes to the start of the data in the physical contiguous
     * memory segment */
    uint32_t nIsPhyAddr;
    uint8_t *pData;
    /**< The data pointer is a virtual address, however the actual data pointed
     * to is required to be in contiguous physical memory unless the field
     requiresPhysicallyContiguousMemory in CpaInstanceInfo is false. */
    void *pCtrl;
    /**< pointer to control section coresponding to data buffer */
};

/**
 *****************************************************************************
 * @ingroup xran
 *      Scatter/Gather buffer list containing an array of Simple buffers.
 *
 * @description
 *      A Scatter/Gather buffer list structure. It is expected that this buffer
 *      structure will be used where more than one flat buffer can be provided
 *      on a particular API.
 *
 *      IMPORTANT - The memory for the pPrivateMetaData member must be allocated
 *      by the client as contiguous memory.  When allocating memory for
 *      pPrivateMetaData a call to cpaCyBufferListGetMetaSize MUST be made to
 *      determine the size of the Meta Data Buffer.  The returned size
 *      (in bytes) may then be passed in a memory allocation routine to allocate
 *      the pPrivateMetaData memory.
 *
 *****************************************************************************/
struct xran_buffer_list
{
    uint32_t nNumBuffers;
    /**< Number of pointers */
    struct xran_flat_buffer *pBuffers;
    /**< Pointer to an unbounded array containing the number of CpaFlatBuffers
     * defined by nNumBuffers */
    void *pUserData;
    /**< This is an opaque field that is not read or modified internally. */
    void *pPrivateMetaData;
    /**< Private Meta representation of this buffer List - the memory for this
     * buffer needs to be allocated by the client as contiguous data.
     * The amount of memory required is returned with a call to
     * cpaCyBufferListGetMetaSize. If cpaCyBufferListGetMetaSize returns a size
     * of zero no memory needs to be allocated, and this parameter can be NULL.
     */
};

/**
 * @ingroup xran
 * Initialize the XRAN Layer via DPDK.
 *
 * @param argc
 *   A non-negative value.  If it is greater than 0, the array members
 *   for argv[0] through argv[argc] (non-inclusive) shall contain pointers
 *   to strings.
 * @param argv
 *   An array of strings.  The contents of the array, as well as the strings
 *   which are pointed to by the array, may be modified by this function.
 *
 * @return
 *   0 - on success
 *   Error codes returned via rte_errno
 */
int32_t xran_init(int argc, char *argv[], struct xran_fh_init *p_xran_fh_init, char *appName, void ** pHandle);

/**
 * @ingroup xran
 *
 *   Function returns handles for number of sectors supported by XRAN layer. Currently function
 *   supports one handle XRAN layer where it supports only one CC
 *
 * @param pHandle
 *   Pointer to XRAN layer handle
 * @param nNumInstances
 *   total number of instances of CC
 * @param pSectorInstanceHandles
 *   Pointer to xran_cc_handle_t where to store Handle pointer
 *
 * @return
 *   0 - on success
 */
int32_t xran_sector_get_instances (void * pHandle, uint16_t nNumInstances,
               xran_cc_handle_t * pSectorInstanceHandles);

/**
 * @ingroup xran
 *
 *   Function initialize Memory Management subsystem (mm) in order to handle memory buffers between XRAN layer
 *   and PHY.
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param nMemorySize
 *   memory size of all segments
 * @param nMemorySegmentSize
 *   size of memory per segment
 *
 * @return
 *   0 - on success
 */
int32_t xran_mm_init (void * pHandle, uint64_t nMemorySize, uint32_t nMemorySegmentSize);

/**
 * @ingroup xran
 *
 *   Function allocates buffer memory (bm) used between XRAN layer and PHY. In general case it's DPDK mbuf.
 *   it uses Memory Management system to get memory chunk and define memory pool on top of it.
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param nPoolIndex
 *   pointer to buffer pool identification to be returned
 * @param nNumberOfBuffers
 *   number of buffer to allocate in the pool
 * @param nBufferSize
 *   buffer size to allocate
 *
 * @return
 *   0 - on success
 */
int32_t xran_bm_init (void * pHandle, uint32_t * pPoolIndex, uint32_t nNumberOfBuffers, uint32_t nBufferSize);

/**
 * @ingroup xran
 *
 *   Function allocates buffer used between XRAN layer and PHY. In general case it's DPDK mbuf.
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param nPoolIndex
 *   buffer pool identification
 * @param ppData
 *   Pointer to pointer where to store address of new buffer
 * @param ppCtrl
 *   Pointer to pointer where to store address of internal private control information
 *
 *
 * @return
 *   0 - on success
 */
int32_t xran_bm_allocate_buffer(void * pHandle, uint32_t nPoolIndex, void **ppData,  void **ppCtrl);

/**
 * @ingroup xran
 *
 *   Function frees buffer used between XRAN layer and PHY. In general case it's DPDK mbuf
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param pData
 *   Pointer to buffer
 * @param pData
 *   Pointer to internal private control information
 *
 * @return
 *   0 - on success
 */
int32_t xran_bm_free_buffer(void * pHandle, void *pData, void *pCtrl);

/**
 * @ingroup xran
 *
 *   Function destroys Memory Management (MM) layer of XRAN library
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 *
 * @return
 *   0 - on success
 */
int32_t xran_mm_destroy (void * pHandle);

/**
 * @ingroup xran
 *
 *   Function configures TX(DL) and RX(UL) output buffers and callback (UL only) for XRAN layer with
 *   given handle
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param pSrcBuffer
 *   list of memory buffers to use to fetch IQs from PHY to XRAN layer (DL)
 * @param pSrcCpBuffer
 *   list of memory buffers to use to configure C-plane (DL)
 * @param pDstBuffer
 *   list of memory buffers to use to deliver IQs from XRAN layer to PHY (UL)
 * @param pDstCpBuffer
 *   list of memory buffers to use to configure C-plane (UL)
 * @param xran_transport_callback_fn pCallback
 *   Callback function to call with arrival of all packets for given CC for given symbol
 * @param pCallbackTag
 *   Parameters of Callback function
 *
 * @return
 *   0  - on success
 *   -1 - on error
 */
 int32_t xran_5g_fronthault_config (void * pHandle,
                    struct xran_buffer_list *pSrcBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                    struct xran_buffer_list *pSrcCpBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                    struct xran_buffer_list *pDstBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                    struct xran_buffer_list *pDstCpBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                    xran_transport_callback_fn pCallback,
                    void *pCallbackTag);

/**
 * @ingroup xran
 *
 *   Function configures PRACH output buffers and callback for XRAN layer with given handle
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param pDstBuffer
 *   list of memory buffers to use to deliver PRACH IQs from xran layer to PHY
 * @param xran_transport_callback_fn pCallback
 *   Callback function to call with arrival of PRACH packets for given CC
 * @param pCallbackTag
 *   Parameters of Callback function
 *
 * @return
 *   0  - on success
 *   -1 - on error
 */
int32_t xran_5g_prach_req (void *  pHandle,
                struct xran_buffer_list *pDstBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                xran_transport_callback_fn pCallback,
                void *pCallbackTag);

/**
 * @ingroup xran
 *
 *   Function configures SRS output buffers and callback for XRAN layer with given handle
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param pDstBuffer
 *   list of memory buffers to use to deliver SRS IQs from xran layer to PHY
 * @param xran_transport_callback_fn pCallback
 *   Callback function to call with arrival of SRS packets for given CC
 * @param pCallbackTag
 *   Parameters of Callback function
 *
 * @return
 *   0  - on success
 *   -1 - on error
 */
int32_t xran_5g_srs_req (void *  pHandle,
                struct xran_buffer_list *pDstBuffer[XRAN_MAX_ANT_ARRAY_ELM_NR][XRAN_N_FE_BUF_LEN],
                xran_transport_callback_fn pCallback,
                void *pCallbackTag);


/**
 * @ingroup xran
 *
 *   Function returns XRAN core utilization stats
 *
 * @param total_time (out)
 *   Pointer to variable to store Total time thread has been running
 * @param used_time (out)
 *   Pointer to variable to store Total time essential tasks have been running on the thread
 * @param core_used (out)
 *   Pointer to variable to store Core on which the XRAN thread is running
 * @param clear (in)
 *   If set to 1, then internal variables total_time and used_time are cleared
 *
 * @return
 *   0 - on success
 */
uint32_t xran_get_time_stats(uint64_t *total_time, uint64_t *used_time, uint32_t *core_used, uint32_t clear);

/**
 * @ingroup xran
 *
 *   Function opens XRAN layer with given handle
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param pointer to struct xran_fh_config pConf
 *   Pointer to XRAN configuration structure with specific settings to use
 *
 * @return
 *   0 - on success
 */
int32_t xran_open(void *pHandle, struct xran_fh_config* pConf);

/**
 * @ingroup xran
 *
 *   Function starts XRAN layer with given handle
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 *
 * @return
 *   0 - on success
 */
int32_t xran_start(void *pHandle);

/**
 * @ingroup xran
 *
 *   Function stops XRAN layer with given handle
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 *
 * @return
 *   0 - on success
 */
int32_t xran_stop(void *pHandle);

/**
 * @ingroup xran
 *
 *   Function closes XRAN layer with given handle
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 *
 * @return
 *   0 - on success
 */
int32_t xran_close(void *pHandle);

/**
 * @ingroup xran
 *
 *   Function registers callback to XRAN layer. Function support callbacks aligned on packet arrival.
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param symCb
 *   pointer to callback function
 * @param symCb
 *   pointer to Callback Function parameters
 * @param symb
 *   symbol to be register for
 * @param ant
 *   Antenna number to trigger callback for packet arrival
 *
 * @return
 *    0 - in case of success
 *   -1 - in case of failure
 */
int32_t xran_reg_sym_cb(void *pHandle, xran_callback_sym_fn symCb, void * symCbParam, uint8_t symb, uint8_t ant);

/**
 * @ingroup xran
 *
 *   Function registers callback to XRAN layer. Function support callbacks align to OTA time. TTI even, half of slot,
 *   full slot with respect to PTP time.
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param Cb
 *   pointer to callback function
 * @param cbParam
 *   pointer to Callback Function parameters
 * @param skipTtiNum
 *   number of calls to be skipped before first call
 * @param callback_to_phy_id
 *   call back time identification (see enum callback_to_phy_id)
 *
 * @return
 *    0 - in case of success
 *   -1 - in case of failure
 */
int32_t xran_reg_physide_cb(void *pHandle, xran_fh_tti_callback_fn Cb, void *cbParam, int skipTtiNum, enum callback_to_phy_id);

/**
 * @ingroup xran
 *
 *   Function returns current TTI, Frame, Subframe, Slot Number as seen "Over air" base on PTP time
 *
 * @param nFrameIdx
 *    Pointer to Frame number [0-99]
 *
 * @param nSubframeIdx
 *    Pointer to Subframe number [0-10]
 *
 * @param nSlotIdx
 *    Pointer to Slot number [0-7]
 *
 * @param nSecond
 *    Pointer to current UTC second
 *
 * @return
 *   current TTI number [0-7999]
 */
int32_t xran_get_slot_idx (uint32_t *nFrameIdx, uint32_t *nSubframeIdx,  uint32_t *nSlotIdx, uint64_t *nSecond);

/**
 * @ingroup xran
 *
 *   Function retrun XRAN layer common counters for given handle
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 *
 * @param pStats
 *   Pointer to pointer of common counter structure
 *
 * @return
 *   0 - on success
 */
int32_t xran_get_common_counters(void *pXranLayerHandle, struct xran_common_counters *pStats);


/**
 * @ingroup xran
 *
 *   Function returns status of operation of FH layer
 *
 * @return
 *  XRAN_INIT    - init state
 *  XRAN_RUNNING - running
 *  XRAN_STOPPED - stopped
 */
enum xran_if_state xran_get_if_state(void);


/**
 * @ingroup xran
 *
 *   Function allocates memory of given size from heap
 *
 * @param buf_len
 *   buffer size
 *
 * @return
 *   buf_len - size of memory allocation
 */
void*    xran_malloc(size_t buf_len);

/**
 * @ingroup xran
 *
 *   Function frees memory of given size from heap
 *
 * @param buf_len
 *   addr - pointer to buffer
 *
 * @return
 *   void
 */
void  xran_free(void *addr);

/**
 * @ingroup xran
 *
 *   Function calculates offset for ptr according to ORAN headers requared
 *
 * @param dst
 *   pointer to be addjusted
 * @compMethod
 *   compression method according to enum xran_compression_method
 *
 * @return
 *   ptr - pointer to payload given header requared
 */
uint8_t* xran_add_hdr_offset(uint8_t  *dst, int16_t compMethod);

/**
 * @ingroup xran
 *
 *   Function calculates offset for ptr according to ORAN C-plane headers requared
 *
 * @param dst
 *   pointer to be addjusted
 *
 * @return
 *   ptr - pointer to payload given header requared
 */
uint8_t  *xran_add_cp_hdr_offset(uint8_t  *dst);

#ifdef __cplusplus
}
#endif

#endif /* _XRAN_FH_O_DU_H_*/
