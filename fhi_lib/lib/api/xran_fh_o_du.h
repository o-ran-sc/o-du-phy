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
#include <stdbool.h>
#include "rte_ether.h"

#ifdef POLL_EBBU_OFFLOAD
#include "xran_timer.h"
#endif

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

#define XRAN_MAX_FH_CORES            (10)   /**< Maximum number of XRAN FH cores supported */
#define XRAN_PORTS_NUM               (8)    /**< number of XRAN ports (aka O-RU|O-DU devices) supported */
#define XRAN_ETH_PF_LINKS_NUM        (4)    /**< number of Physical Ethernet links per one O-RU|O-DU */
#define XRAN_MAX_PRACH_ANT_NUM       (8)    /**< number of XRAN Prach ports supported */
#define XRAN_MAX_NUM_MU              (6)    /**< Maximum number of numerologies defined in the standard. */
#define XRAN_MAX_NUM_PRACH_MU        (5)    /**< Maximum number of prach numerologies used in FlexRAN  */
#define XRAN_DEFAULT_MU              XRAN_MAX_NUM_MU    /**< primary numerology for given ru/xranport will be used when this value is provided by L1 */
#define XRAN_MAX_MTU                 (9600)
#define XRAN_NBIOT_MU                (5)    /**< numerology number to use for nb-iot */
#define XRAN_INTRA_SYM_MAX_DIV       (4)    /*Maximum number of divisions of a symbol*/
#define XRAN_MAX_RU_ETH_POINTS       (2)    /* the number of RU used eth points. 1 or 2*/
#define XRAN_MAX_AUX_BBDEV_NUM       (3)

#if (defined(XRAN_O_RU_BUILD) || defined(XRAN_SAMPLEAPP_BUILD))
    #define XRAN_N_FE_BUF_LEN        (20)   /**< Number of TTIs (slots) */
#else
    #define XRAN_N_FE_BUF_LEN        (10)    /**< Number of TTIs (slots) */
#endif

#define XRAN_MAX_SECTOR_NR           (40)   /**< Max sectors per XRAN port */
#define XRAN_MAX_ANTENNA_NR          (16)   /**< Max number of extended Antenna-Carriers:
                                               a data flow for a single antenna (or spatial stream) for a single carrier in a single sector */

/* see 10.2	Hierarchy of Radiation Structure in O-RU (assume TX and RX panel are the same dimensions)*/
#define XRAN_MAX_PANEL_NR            (1)   /**< Max number of Panels supported per O-RU */
#define XRAN_MAX_TRX_ANTENNA_ARRAY   (1)   /**< Max number of TX and RX arrays per panel in O-RU */
#define XRAN_MAX_ANT_ARRAY_ELM_NR    (64)  /**< Maximum number of Antenna Array Elemets in Antenna Array in the O-RU */
#define XRAN_MAX_CSIRS_PORTS         (32)  /**< Max number of CSI-RS Ports */

#define XRAN_NUM_OF_SYMBOL_PER_SLOT  (14) /**< Number of symbols per slot */
#define XRAN_MAX_NUM_OF_SRS_SYMBOL_PER_SLOT  XRAN_NUM_OF_SYMBOL_PER_SLOT /**< Max Number of SRS symbols per slot */
#define XRAN_MAX_TDD_PERIODICITY     (80) /**< Max TDD pattern period */
#define XRAN_MAX_CELLS_PER_PORT      (XRAN_MAX_SECTOR_NR) /**< Max cells mapped to XRAN port */
#define XRAN_COMPONENT_CARRIERS_MAX  (XRAN_MAX_SECTOR_NR) /**< number of CCs */
#define XRAN_NUM_OF_ANT_RADIO        (XRAN_MAX_SECTOR_NR*XRAN_MAX_ANTENNA_NR) /**< Max Number of Antennas supported for all CC on single XRAN port */
#define XRAN_MAX_PRBS                (275) /**< Max of PRBs per CC per antanna for 5G NR */
#define XRAN_NUM_OF_SC_PER_RB  (12) /**< Number of subcarriers per RB */

#define XRAN_MAX_DSS_PERIODICITY     (15) /**< Max DSS pattern period */

#define XRAN_MAX_SECTIONS_PER_SLOT   (32)  /**< Max number of different sections in single slot (section may be equal to RB allocation for UE) */
#define XRAN_MIN_SECTIONS_PER_SLOT   (6)   /**< Min number of different sections in single slot (section may be equal to RB allocation for UE) */
#define XRAN_MAX_SECTIONS_PER_SYM    (XRAN_MAX_SECTIONS_PER_SLOT)  /**< Max number of different sections in single slot (section may be equal to RB allocation for UE) */
#define XRAN_MIN_SECTIONS_PER_SYM    (XRAN_MIN_SECTIONS_PER_SLOT)  /**< Min number of different sections in single slot (section may be equal to RB allocation for UE) */
#define XRAN_SSB_MAX_NUM_SC          (240)  /**< 3GPP TS 38.211 - 7.4.3.1 Time-frequency structure of an SS/PBCH block */
#define XRAN_SSB_MAX_NUM_PRB         (XRAN_SSB_MAX_NUM_SC /  XRAN_NUM_OF_SC_PER_RB)

#define XRAN_MAX_FRAGMENT            (2)   /**< Max number of fragmentations in single symbol */
#define XRAN_MAX_RX_PKT_PER_SYM      (32)   /**< Max number of packets received in single symbol */
#define XRAN_MAX_SET_BFWS            (64)  /**< Assumed 64Ant, BFP 9bit with 9K jumbo frame */

#define XRAN_MAX_PKT_BURST (448+4) /**< 4x14x8 symbols per ms */
#define XRAN_N_MAX_BUFFER_SEGMENT XRAN_MAX_PKT_BURST /**< Max number of segments per ms */

#define NUM_MBUFS_SMALL (16383)  /** optimal is n = (2^q - 1) */
#define NUM_MBUFS_VF_SMALL (131071)
#define NUM_MBUFS 65535  /** optimal is n = (2^q - 1) */
#define NUM_MBUFS_VF (1048575)
#define NUM_MBUFS_RING (NUM_MBUFS+1) /** The size of the ring (must be a power of 2) */


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
#define MX_NUM_SAMPLES                       (16)/**< MAX Number of Samples for One Way delay Measurement */

#define XRAN_VF_QUEUE_MAX (XRAN_MAX_SECTOR_NR*XRAN_MAX_ANTENNA_NR*2+XRAN_MAX_ANT_ARRAY_ELM_NR) /**< MAX number of HW queues for given VF */

#define XRAN_HALF_CB_SYM            0   /**< Half of the Slot (offset +7) */
#define XRAN_THREE_FOURTHS_CB_SYM   3   /**< 2/4 of the Slot  (offset +7) */
#define XRAN_FULL_CB_SYM            7   /**< Full Slot  (offset +7) */
#define XRAN_ONE_FOURTHS_CB_SYM    12   /**< 1/4 of the Slot (offset +7) */

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

#define PRINT_NON_ZERO_CNTR(counter,str,fmt) \
    if(counter != 0){                        \
        printf(fmt,str,counter);             \
    }

enum XranFrameDuplexType
{
    XRAN_FDD = 0, XRAN_TDD
};

enum xran_if_state
{
    XRAN_INIT = 0,
    XRAN_RUNNING,
    XRAN_STOPPED,
    XRAN_OWDM
};

typedef enum xran_vMu_proc_type_e {
    XRAN_VMU_PROC_SSB,
    XRAN_VMU_PROC_MAX
} xran_vMu_proc_type_t;

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
 *       enum of callback function type ids for TTI
 *****************************************************************************/
enum callback_to_phy_id
{
    XRAN_CB_TTI = 0, /**< callback on TTI boundary */
    XRAN_CB_HALF_SLOT_RX =1, /**< callback on half slot (sym 7) packet arrival*/
    XRAN_CB_FULL_SLOT_RX =2, /**< callback on full slot (sym 14) packet arrival */
    XRAN_CB_MAX /**< max number of callbacks */
};

/**
 ******************************************************************************
 * @ingroup xran
 *
 * @description
 *       enum of callback function type ids for sumbol
 *****************************************************************************/
enum cb_per_sym_type_id
{
    XRAN_CB_SYM_OTA_TIME         = 0, /**< callback on exact SYM OTA time (+-SW jitter)  */
    XRAN_CB_SYM_RX_WIN_BEGIN     = 1, /**< callback on exact SYM RX window start time (+-SW jitter) */
    XRAN_CB_SYM_RX_WIN_END       = 2, /**< callback on exact SYM RX window stop time (+-SW jitter)  */
    XRAN_CB_SYM_TX_WIN_BEGIN     = 3, /**< callback on exact SYM TX window start time (+-SW jitter) */
    XRAN_CB_SYM_TX_WIN_END       = 4, /**< callback on exact SYM TX window stop time (+-SW jitter)  */
    XRAN_CB_SYM_CP_DL_WIN_BEGIN  = 5, /**< callback on exact SYM DL CP window start time (+-SW jitter)  */
    XRAN_CB_SYM_CP_DL_WIN_END    = 6, /**< callback on exact SYM DL CP window stop time (+-SW jitter)  */
    XRAN_CB_SYM_CP_UL_WIN_BEGIN  = 7, /**< callback on exact SYM UL CP window start time (+-SW jitter)  */
    XRAN_CB_SYM_CP_UL_WIN_END    = 8, /**< callback on exact SYM UL CP window stop time (+-SW jitter)  */
    XRAN_CB_SYM_MAX                   /**< max number of types of callbacks */
};

/**  Beamforming type, enumerated as "frequency", "time" or "hybrid"
     section 10.4.2	Weight-based dynamic beamforming */
enum xran_weight_based_beamforming_type {
    XRAN_BF_T_FREQUENCY = 0,
    XRAN_BF_T_TIME      = 1,
    XRAN_BF_T_HYBRID    = 2,
    XRAN_BF_T_MAX
};

typedef enum xran_lbm_state_e {
    XRAN_LBM_STATE_INIT           = 0,
    XRAN_LBM_STATE_IDLE           = 1,
    XRAN_LBM_STATE_TRANSMITTING   = 2,
    XRAN_LBM_STATE_WAITING        = 3,
    XRAN_LBM_STATE_MAX            = 4
} xran_lbm_state;

/** contains time related information according to type of event */
struct xran_sense_of_time {
    enum cb_per_sym_type_id type_of_event; /**< event type id */
    uint32_t tti_counter;  /**< TTI counter with in GPS second */
    uint32_t nSymIdx;      /**< Symbol Idx with in Slot [0-13] */
    uint32_t nFrameIdx;    /**< ORAN Frame */
    uint32_t nSubframeIdx; /**< ORAN Subframe */
    uint32_t nSlotIdx;     /**< Slot within subframe */
    uint64_t nSecond;      /**< GPS second of this symbol */
};

typedef int32_t xran_status_t; /**< Xran status return value */

/** callback function type for Symbol packet */
typedef int32_t (*xran_callback_sym_fn)(void*, struct xran_sense_of_time* p_sense_of_time);

/** Callback function type for TTI event */
typedef int32_t (*xran_fh_tti_callback_fn)(void*, uint8_t mu);

/** Callback function type packet arrival from transport layer (ETH or IP) */
typedef void (*xran_transport_callback_fn)(void*, xran_status_t, uint8_t mu);

/** Callback function type OAM cb */
typedef int32_t (*xran_callback_oam_notify_fn)(void*, uint8_t vfId, uint8_t lbmStatus);

/** Callback functions to poll BBdev encoder */
typedef int16_t (*phy_encoder_poll_fn)(void);

/** Callback functions to poll BBdev decoder */
typedef int16_t (*phy_decoder_poll_fn)(void);

/** Callback functions to poll BBdev SRS FFT */
typedef int16_t (*phy_srs_fft_poll_fn)(void);

/** Callback functions to poll BBdev PRACH IFFT */
typedef int16_t (*phy_prach_ifft_poll_fn)(void);

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
    XRAN_BEAM_ID_BASED  = 0, /**< beam index based */
    XRAN_BEAM_WEIGHT    = 1, /**< beam forming weights */
    XRAN_BEAM_ATTRIBUTE = 2, /**< beam index based */
    XRAN_BEAM_TYPE_MAX
};

/** state of bbdev with xran */
enum xran_bbdev_init
{
    XRAN_BBDEV_NOT_USED    = -1, /**< BBDEV is disabled */
    XRAN_BBDEV_MODE_HW_OFF =  0, /**< BBDEV is enabled for SW sim mode */
    XRAN_BBDEV_MODE_HW_ON  =  1, /**< BBDEV is enable for HW */
    XRAN_BBDEV_MODE_HW_SW  =  2, /**< BBDEV for SW and HW is enabled */
    XRAN_BBDEV_MODE_MAX
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

typedef enum
{
    XRAN_NBIOT_UL_SCS_15,
    XRAN_NBIOT_UL_SCS_3_75,
}nbiot_ul_scs;


enum xran_memstat_index
{
    XRAN_MEMSTAT_MAXNUM = 0,
    XRAN_MEMSTAT_AVAIL,
    XRAN_MEMSTAT_INUSE,
    XRAN_MEMSTAT_END
};

struct xran_memstat
{
    uint32_t socket_direct[XRAN_MEMSTAT_END];
    uint32_t socket_indirect[XRAN_MEMSTAT_END];
    uint32_t pktgen[XRAN_MEMSTAT_END];
    uint32_t vf_rx[16][RTE_MAX_QUEUES_PER_PORT][XRAN_MEMSTAT_END];
    uint32_t vf_small[16][XRAN_MEMSTAT_END];
};


/** callback return information */
struct xran_cb_tag {
    uint16_t cellId;
    uint16_t oXuId;
    uint32_t symbol;
    uint32_t slotiId;
};

/** Common Data for ecpri one-way delay measurements  */
struct xran_ecpri_del_meas_cmn {
    uint16_t initiator_en;      // Initiator 1, Recipient 0
    uint16_t numberOfSamples;   // Total number of samples to be collected and averaged
    uint32_t filterType;        // Average for number of samples collected 0
    uint64_t responseTo;        //  Response Timeout in ns
    uint16_t measVf;            //  Vf using the owd transmitter
    uint16_t measState;         //  The state of the owd Transmitter: OWDMTX_DIS,OWDMTX_INIT,OWDMTX_IDLE,OWDMTX_ACTIVE,OWDTX_DONE
    uint16_t measId;            //  Measurement Id to be used by the transmitter
    uint16_t measMethod;        //  Measurement Method i.e. REQUEST, REM_REQ, REQ_WFUP or REM_REQ_WFUP
    uint16_t owdm_enable;       //  1: Enabled  0:Disabled
    uint16_t owdm_PlLength;     //  Payload Length   44 <= PlLength <= 1400
};

/** Port specific data for ecpri one-way delay measurements */
struct xran_ecpri_del_meas_port {
    uint64_t t1;                            // ecpri ts1
    uint64_t t2;                            // ecpri ts2
    uint64_t tr;                            // ecpri tr
    int64_t delta;                          // stores differences based on the msState
    uint8_t portid;                         // portid for this owdm
    uint8_t runMeas;                        // run One Way Delay measurements for numberOfSamples
    uint16_t currentMeasID;                 // Last Measurement ID received, for originator use as base for the Measurement ID being send out
    uint16_t msState;                       // Measurement State for Initiator: Idle, Waiting_Response, Waiting_Request, Waiting_Request_with_fup, Waiting_fup, Done
                                            // Measurement State for Recipient: Idle, Waiting_Response, Waiting_Follow_up, Done
    uint16_t numMeas;                       // Number of Measurements completed (Running number up to common config numberOfSamples
    uint16_t txDone;                        // For originator clear after each change of state and set once the transmission is done
    uint64_t rspTimerIdx;                   // Timer Index for TimeOut Timer. On timeout abort current measurement and go back to idle state
    uint64_t delaySamples[MX_NUM_SAMPLES];  // Storage for collected delay samples i.e. td
    uint64_t delayAvg;                      // Contains the average based on the numberOfSamples for the delay, gets computed once we have
                                            // completed the collection for all the numberOfSamples prescribed
};

/** DPDK IO configuration for XRAN layer */
struct xran_io_cfg {
    uint8_t id;                   /**< should be (0) for O-DU or (1) O-RU (debug) */
    uint8_t num_vfs;              /**< number of VFs for C-plane and U-plane (should be even) */
    uint16_t num_rxq;             /**< number of RX queues per VF */
    char *dpdk_dev[XRAN_VF_MAX];  /**< VFs devices  */
    char *bbdev_dev[1];           /**< BBDev dev name */
    int32_t bbdev_mode;           /**< DPDK for BBDev */
    char *bbdev_devx[XRAN_MAX_AUX_BBDEV_NUM];           /**< BBDev other dev name */
    int32_t bbdevx_num;           /**< DPDK for other BBDev */
    int32_t dpdkProcessType;      /**< DPDK init Process type */
    uint32_t nDpdkProcessID;      /**< DPDK init Process Id tag, use for different L1app with the same file-prefix  */
    uint32_t dpdkIoVaMode;        /**< IOVA Mode */
    uint32_t dpdkMemorySize;      /**< DPDK max memory allocation */
    int32_t  core;                /**< reserved */
    int32_t  system_core;         /**< reserved */
    uint64_t pkt_proc_core;         /**< worker mask 0-63 */
    uint64_t pkt_proc_core_64_127;  /**< worker mask 64-127 */
    int32_t  pkt_aux_core;        /**< reserved */
    int32_t  timing_core;         /**< core used by xRAN */
    int32_t  port[XRAN_VF_MAX];   /**< VFs ports */
    int32_t  io_sleep;            /**< enable sleep on PMD cores */
    uint32_t nEthLinePerPort;     /**< 1, 2, 3 total number of links per O-RU (Fronthaul Ethernet link) */
    uint32_t nEthLineSpeed;       /**< 10G,25G,40G,100G speed of Physical connection on O-RU */
    uint32_t num_mbuf_alloc;      /**< number of mbuf allocated by DPDK */
    uint32_t num_mbuf_vf_alloc;   /**< number of mbuf allocated by DPDK */
    int32_t  one_vf_cu_plane;     /**< 1 - C-plane and U-plane use one VF */
    struct xran_ecpri_del_meas_cmn eowd_cmn[2];/**<ecpri owd measurements common settings for O-DU and O-RU */
    struct xran_ecpri_del_meas_port eowd_port[2][XRAN_VF_MAX];  /**< ecpri owd measurements per port variables for O-DU and O-RU */
    int32_t  bbu_offload;         /**< enable packet handling on BBU cores */
};

/** XRAN spec section 3.1.3.1.6 ecpriRtcid / ecpriPcid define */
struct xran_eaxcid_config {
    uint16_t mask_cuPortId;     /**< Mask CU PortId */
    uint16_t mask_bandSectorId; /**< Mask Band */
    uint16_t mask_ccId;         /**< Mask CC */
    uint16_t mask_ruPortId;     /**< Mask RU Port ID */

    uint8_t bit_cuPortId;       /**< bandsectorId + ccId + ru_portId */
    uint8_t bit_bandSectorId;   /**< ccId + ruPortId */
    uint8_t bit_ccId;           /**< ru_portId */
    uint8_t bit_ruPortId;       /**< 0 */
};

struct xran_ru_init
{
    uint32_t dpdk_port;         /**< DPDK port number used for FH */
    uint8_t  id;
    uint8_t  num_vfs;           /**< number of VFs for C-plane and U-plane (should be even) */
    uint16_t num_rxq;           /**< number of RX queues per VF */
    int32_t  port[XRAN_VF_MAX]; /**< the list of VFs */
    uint32_t nEthLinePerPort;   /**< 1, 2, 3 total number of links per O-RU (Fronthaul Ethernet link) */
    uint32_t nEthLineSpeed;     /**< 10G,25G,40G,100G speed of Physical connection on O-RU */
    int32_t  one_vf_cu_plane;   /**< 1 - C-plane and U-plane use one VF */
    uint32_t nCC;               /**< number of Component carriers supported on FH */

    uint32_t mtu;               /**< maximum transmission unit (MTU)
                                    is the size of the largest protocol data unit (PDU)
                                    that can be communicated in a single xRAN network layer transaction.
                                    supported 1500 bytes and 9600 bytes (Jumbo Frame) */
    int8_t  *pPeerMacAddr;     /**<  Ethernet Mac Address of destination(DU or RU) */

    enum xran_input_byte_order byteOrder;   /**< Order of bytes in int16_t in buffer. Big or little endian */
    enum xran_input_i_q_order  iqOrder;     /**< order of IQs in the buffer */

    struct xran_eaxcid_config eAxCId_conf;  /**< config of ecpriRtcid/ecpriPcid */

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

    uint8_t  eaxcOffset;    /* Starting value of Eaxc for PDSCH, PUSCH packets (Absolute value) of this numerology.
                               Should be unique across all numerologies for the RU */

    struct xran_ecpri_del_meas_cmn eowd_cmn[2];     /**<ecpri owd measurements common settings for O-DU and O-RU */
    struct xran_ecpri_del_meas_port eowd_port[2][XRAN_VF_MAX];  /**< ecpri owd measurements per port variables for O-DU and O-RU */
};

typedef int32_t (* xran_phy_oam_cb_func)(uint8_t param1, uint8_t param2, uint8_t param3);

typedef enum xran_phy_oam_cb_e {
    XRAN_PHY_OAM_MANAGE_LBM       = 0,
    XRAN_PHY_OAM_PM_STATS         = 1,
    XRAN_PHY_OAM_LBM_NOTIFICATION = 2,
    XRAN_PHY_OAM_MAX              = 3
} xran_phy_oam_cb;

typedef struct xran_lbm_stats_s {

    uint64_t rxValidInOrderLBRs;

    uint32_t rxValidOutOfOrderLBRs;

    /* O-DU : Number of transmitted LBM packets
       O-RU : Number of transmitted LBR packets */
    uint64_t numTransmittedLBM;

    /* Number of LBRs received but ignored */
    uint32_t numRxLBRsIgnored;

} xran_lbm_stats;

/**
* =============================================================
* @struct xran_lbm_port_info
*
* @brief
*
* This structure contains LBM info per ethernet port
* =============================================================
*/
typedef struct xran_lbm_port_info_s {

    xran_lbm_state lbm_state;

    /* Flag to allow transmission of LBM */
    bool xmitEnable;

    /* Flag to indicate that LBR has been received */
    bool LBRreceived;

    /* The value to place in the Loopback Transaction Identifier field of the next LBM */
    uint32_t nextLBMtransID;

    /* The value expected to be found in the Loopback Transaction Identifier field of the next LBR */
    uint32_t expectedLBRtransID;

    /* Link status of the eth port: DOWN (0)/ UP (1) */
    bool linkStatus;

    /* Number of re-transmissions attempted, if this value exceeds the maximum configured number of
       retransmissions link state is declared down */
    uint16_t numRetries;

    /* Time instance in nsec for keeping track of timeout, next LBM transmission */
    uint64_t nextPass;

    xran_lbm_stats stats;

    bool lbm_enable;

} xran_lbm_port_info;

/**
* =============================================================
* @struct xran_LBM_init_vars
*
* @brief
*
* This structure contains inputs for Connection Fault Management
* LBM/LBR procedure as per IEEE 802.1Q-2018
* =============================================================
*/
typedef struct xran_lbm_common_info_s {

    /* Time interval (in milli seconds) for LBR reception time out */
    uint16_t LBRTimeOut;

    /* Time interval (in milli seconds) between two LBM transmissions from O-DU */
    uint16_t LBMPeriodicity;

    /* Number of LBM retransmission attempted by O-DU if LBR is not received before declaring link status as down */
    uint16_t numRetransmissions;

    /* Time (in nano sec) when next LBM tranmission will be enabled */
    uint64_t nextLBMtime;

} xran_lbm_common_info;

/**
* XRAN Front haul interface initialization settings
*/
struct xran_fh_init {
    struct xran_io_cfg io_cfg;    /**< DPDK IO for XRAN */
    struct xran_eaxcid_config eAxCId_conf[XRAN_PORTS_NUM]; /**< config of ecpriRtcid/ecpriPcid */

    uint32_t xran_ports;        /**< Number of O-RU/O-DU connected to this instantiation of ORAN FH layer */
    uint32_t dpdkBasebandFecMode;/**< DPDK Baseband FEC device mode (0-SW, 1-HW) */
    char *dpdkBasebandDevice;   /**< DPDK Baseband device address */
    char *filePrefix;           /**< DPDK prefix */
    char *dpdkVfioVfToken;      /**< DPDK VFIO token */
    uint32_t mtu;               /**< maximum transmission unit (MTU) is the size of the largest protocol data unit (PDU)
                                    that can be communicated in a single xRAN network layer transaction.
                                    Supported 1500 bytes and 9600 bytes (Jumbo Frame) */
    int8_t *p_o_du_addr;    /**<  O-DU Ethernet Mac Address */
    int8_t *p_o_ru_addr;    /**<  O-RU Ethernet Mac Address */

    uint16_t totalBfWeights;        /**< The total number of beamforming weights on RU for extensions */
    int32_t  mlogxranenable;        /**< Enable mlog during runtime 0:disable 1:enable */
    uint32_t rru_workaround;        /* < rru adapt flag */
    uint8_t  dlCpProcBurst;         /**< When set to 1, dl cp processing will be done on single symbol.
                                        When set to 0, DL CP processing will be spread across all allowed symbols and
                                        multiple cores to reduce burstiness */
    phy_encoder_poll_fn bbdevEnc;   /**< call back to poll BBDev encoder */
    phy_decoder_poll_fn bbdevDec;   /**< call back to poll BBDev decoder */
    phy_srs_fft_poll_fn bbdevSrsFft;   /**< call back to poll BBDev SRS FFT */
    phy_prach_ifft_poll_fn bbdevPrachIfft;   /**< call back to poll BBDev PRACH IFFT */

#if 0   // not implemented
    xran_fh_tti_callback_fn ttiCb;  /**< call back for TTI event */
    void *ttiCbParam;               /**< parameters of call back function */
#endif

    uint32_t ttiBaseMu;         /**< Numerology for the period of TTI indication */
    int32_t  GPS_Alpha;         /**< refer to alpha as defined in section 9.7.2 of ORAN spec.
                                    this value should be alpha * (1/1.2288ns), range 0 - 1e7 (ns) */
    int32_t  GPS_Beta;          /**< beta value as defined in section 9.7.2 of ORAN spec. range -32767 ~ +32767 */
    uint16_t xran_max_frame;    /**< max frame number supported */

    struct xran_ru_init ruCfg[XRAN_PORTS_NUM];

    int32_t  debugStop;         /**< Enable auto stop */
    int32_t  debugStopCount;    /**< Enable auto stop after number of Tx packets */
    int32_t  mlogEnable;        /**< Enable mlog during runtime 0:disable 1:enable */
    uint32_t logLevel;          /**< configuration of log level */

    bool lbmEnable;     /* Enable IEEE 802.1Q LBM messages on the fronthaul interface */
    xran_lbm_common_info lbm_common_info;
    xran_phy_oam_cb_func oam_cb_func[XRAN_PHY_OAM_MAX];

};

struct xran_ext11_bfw_info {
        uint16_t    beamId;     /* 15bits, needs to strip MSB */
        uint8_t     *pBFWs;     /* external buffer pointer */
};
#define XRAN_CP_BF_WEIGHT_STRUCT_OPT
#ifdef XRAN_CP_BF_WEIGHT_STRUCT_OPT
    struct xran_ext11_bfw_set_info {
        uint16_t    beamId[XRAN_MAX_SET_BFWS];     /* 15bits, needs to strip MSB */
        uint8_t     *pBFWs[XRAN_MAX_SET_BFWS];     /* external buffer pointer */
};
#else
#endif

/** Beamforming waights for single stream for each PRBs  given number of Antenna elements */
struct xran_cp_bf_weight{
    int16_t nAntElmTRx;        /**< num TRX for this allocation */
    int16_t  ext_section_sz;   /**< extType section size */
    void*  p_ext_start;      /**< pointer to start of buffer for full C-plane packet */
    int8_t*  p_ext_section;    /**< pointer to form extType */

    uint8_t     extType;     /* This parameter determines whether to use extType-1 or 11, 1 - use ext1 and 0 - ext11 */
    /* For ext 11 */
    uint8_t     bfwCompMeth;    /* Compression Method for BFW */
    uint8_t     bfwIqWidth;     /* Bitwidth of BFW */
    uint8_t     numSetBFWs;     /* Total number of beam forming weights set (L) */
    uint8_t     numBundPrb;     /* This parameter is the number of bundled PRBs per beamforming weights, 0 means to use ext1 */
    uint8_t     RAD;
    uint8_t     disableBFWs;
    int16_t     maxExtBufSize;  /* Maximum space of external buffer */
#ifdef XRAN_CP_BF_WEIGHT_STRUCT_OPT
    struct xran_ext11_bfw_set_info bfw[1];
#else
    struct xran_ext11_bfw_info bfw[XRAN_MAX_SET_BFWS];
#endif
};
struct xran_cp_bf_attribute{
    int16_t weight[4];
};
struct xran_cp_bf_precoding{
    int16_t weight[4];
};

struct xran_rx_packet_ctl {
    uint16_t nRBStart[XRAN_MAX_RX_PKT_PER_SYM];    /**< start RB of RB allocation */
    uint16_t nRBSize[XRAN_MAX_RX_PKT_PER_SYM];     /**< number of RBs used */
    uint16_t nSectid[XRAN_MAX_RX_PKT_PER_SYM];     /**< number of RBs used */
    uint8_t *pData[XRAN_MAX_RX_PKT_PER_SYM];      /**< pointer to data buffer */
    void    *pCtrl[XRAN_MAX_RX_PKT_PER_SYM];      /**< pointer to mbuf */
    int32_t nRxPkt;    /**< number of Rx packets received */
    int32_t reserved;    /**< reserved */
};

/** section descriptor for given number of PRBs used on U-plane packet creation */
struct xran_section_desc {
    uint32_t section_id:9; /**< section id used for this element */
    uint32_t num_prbu:9;
    uint32_t start_prbu:9;
    uint32_t section_pos:2;
    uint32_t reserved:3;
    int16_t iq_buffer_offset;    /**< Offset in bytes for the content of IQs with in main symbol buffer */
    int16_t iq_buffer_len;       /**< Length in bytes for the content of IQs with in main symbol buffer */

    uint8_t *pData;      /**< optional pointer to data buffer */
    void    *pCtrl;      /**< optional pointer to mbuf */
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
    uint16_t ScaleFactor;  /**< scale factor for modulation compression */
    int16_t reMask;   /**< 12-bit RE Mask for modulation compression */
    int16_t BeamFormingType; /**< index based, weights based or attribute based beam forming*/
    int16_t startSectId;    /**< start section id for this prb element*/
    bool generateCpPkt;    /**< flag for new C-Plane section */
    int16_t UP_nRBStart;    /**< start RB of RB allocation for U-Plane */
    int16_t UP_nRBSize;    /**< start RB of RB allocation for U-Plane */
    int16_t PDU_RBStart;   /**< start RB of PDU */
    uint8_t sectType;   /**< section type for correlating UL uplane messages with numerology in case of mixed numerology */
    struct xran_section_desc   sec_desc[XRAN_NUM_OF_SYMBOL_PER_SLOT]; /**< section desctiptors to U-plane data given RBs */
    struct xran_cp_bf_weight   bf_weight; /**< beam forming information relevant for given RBs */

    union {
        struct xran_cp_bf_attribute bf_attribute;
        struct xran_cp_bf_precoding bf_precoding;
    };
};

/** PRB map structure */ /*Different PRB MAPs for different Numerologies: Mixed Numerology case*/
struct xran_prb_map {
    uint8_t   dir;              /**< DL or UL direction */
    uint8_t   xran_port;        /**< xran id of given RU [0-(XRAN_PORTS_NUM-1)] */
    uint16_t  band_id;          /**< xran band id */
    uint16_t  cc_id;            /**< component carrier id [0 - (XRAN_MAX_SECTOR_NR-1)] */
    uint16_t  ru_port_id;       /**< RU device antenna port id [0 - (XRAN_MAX_ANTENNA_NR-1) */
    uint16_t  tti_id;           /**< xRAN slot id [0 - (max tti-1)] */
    uint8_t   start_sym_id;     /**< start symbol Id [0-13] */
    uint32_t  nPrbElm;          /**< total number of PRB elements for given map [0- (XRAN_MAX_SECTIONS_PER_SLOT-1)] */
    struct xran_rx_packet_ctl sFrontHaulRxPacketCtrl[XRAN_NUM_OF_SYMBOL_PER_SLOT];
    struct xran_prb_elm prbMap[1];
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

    /* Return values after initialization */
    uint8_t    startSymId;
    uint8_t    lastSymId;
    uint16_t   startPrbc;
    uint8_t    numPrbc;
    uint16_t   timeOffset;
    int32_t    freqOffset;
    uint8_t    prachEaxcOffset; /* Starting value of Eaxc for PRACH packets for this numerology (Absolute value). Should be unique across all numerologies for the RU */
    uint8_t    nPrachConfIdxLTE;
    /*NPRACH Parameters to use only in case of NB-IOT: To be filled by L1*/
    uint8_t    nprachformat;
    uint16_t   periodicity;
    uint16_t   startTime;
    uint8_t    suboffset;
    uint8_t    numSubCarriers;
    uint8_t    nRep; /*Repetitions*/
};

/**< SRS configuration required for XRAN based FH */
struct xran_srs_config {
    uint16_t    symbMask;       /* deprecated */
    uint16_t    slot;           /**< SRS slot within TDD period (special slot), for O-RU emulation */
    uint8_t     ndm_offset;     /**< tti offset to delay the transmission of NDM SRS UP, for O-RU emulation */
    uint16_t    ndm_txduration; /**< symbol duration for NDM SRS UP transmisson, for O-RU emulation */
    uint8_t     srsEaxcOffset;    /**< starting value of eAxC for SRS packets for this numerology (Absolute value). Should be unique across all numerologies for the RU */
};

/**< CSI_RS configuration */
struct xran_csirs_config {
    int32_t     csirsEaxcOffset; /**<starting value of eAxC for CSi-RS packets */
};

/** XRAN slot configuration */
struct xran_slot_config {
    uint8_t nSymbolType[XRAN_NUM_OF_SYMBOL_PER_SLOT]; /**< Defines the Symbol type for all 14 symbols in a slot. 0: DL, 1: UL, 2: Guard */
    uint8_t reserved[2];
};

/** XRAN front haul frame config */
struct xran_frame_config {
    uint8_t      nFrameDuplexType; /**< Frame Duplex type:  0 -> FDD, 1 -> TDD */
    uint8_t      nTddPeriod;  /**< TDD period */
    struct xran_slot_config sSlotConfig[XRAN_MAX_TDD_PERIODICITY];
    /**< TDD Slot configuration - If nFrameDuplexType = TDD(1), then this config defines the slot config type for each slot.*/
    /* The number of slots need to be equal to nTddPeriod */
};

/** XRAN front haul O-RU settings */
struct xran_ru_config {
    enum xran_ran_tech      xranTech;      /**< 5GNR or LTE */
    enum xran_category      xranCat;       /**< mode: Catergory A or Category B */
    enum xran_comp_hdr_type xranCompHdrType;   /**< dynamic or static udCompHdr handling*/
    uint8_t                 iqWidth;           /**< IQ bit width */
    uint8_t                 compMeth;      /**< Compression method */
    uint8_t                 iqWidth_PRACH;           /**< IQ bit width for PRACH */
    uint8_t                 compMeth_PRACH;      /**< Compression method for PRACH */
    uint8_t                 fftSize[XRAN_MAX_NUM_MU];       /**< FFT Size */
    enum xran_input_byte_order byteOrder; /**< Order of bytes in int16_t in buffer. Big or little endian */
    enum xran_input_i_q_order  iqOrder;   /**< order of IQs in the buffer */
    uint16_t xran_max_frame;   /**< max frame number supported */
};

/* Application should allocate memory for this structure per port and update it per tti.
 * Xran will access cplane and uplane shared buffers for active numerologies and send the packets out for that tti.
 * Note: TTI of the given numerology will be considered.
 */
typedef struct xran_active_numerologies_per_tti
{
    bool numerology[XRAN_N_FE_BUF_LEN][XRAN_MAX_NUM_MU];
}xran_active_numerologies_per_tti;

typedef struct xran_fh_per_mu_cfg
{
    uint32_t nDLBandwidth;    /**< Carrier bandwidth for in MHz. Value: 5->400 */
    uint32_t nULBandwidth;    /**< Carrier bandwidth for in MHz. Value: 5->400 */
    struct   xran_prach_config     prach_conf;
    int32_t  freqOffset;

    uint16_t nDLFftSize;  /**< DL FFT size */
    uint16_t nULFftSize;  /**< UL FFT size */

    uint8_t eaxcOffset; /* Starting value of Eaxc for PDSCH, PUSCH packets (Absolute value) of this numerology. Should be unique across all numerologies for the RU */

    uint16_t nDLRBs;      /**< DL PRB  */
    uint16_t nULRBs;      /**< UL PRB  */

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

    uint8_t prachEnable;    /**<  enable PRACH   */
    uint8_t prachConfigIndex;/**< TS36.211 - Table 5.7.1-2 : PRACH Configuration Index */
    uint8_t prachConfigIndexLTE;/**< PRACH Configuration Index for LTE in dss case*/

    nbiot_ul_scs nbIotUlScs;     /**< Applicable only for NB-IOT (mu=4). NBIOT supports asymmetric SCS usage in
                                      downlink and uplink directions. xran library will use this parameter to derive
                                      slot-duration for UL NB-IOT:
                                      XRAN_NBIOT_UL_SCS_15: slot-duration=1ms
                                      XRAN_NBIOT_UL_SCS_3_75: slot-duration=2ms */
    uint16_t adv_tx_time;         /**Time by which the packet should be transmitted in advance (microseconds)*/

} xran_fh_per_mu_cfg;

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
    uint32_t            nDLAbsFrePointA; /**< Abs Freq Point A of the Carrier Center Frequency for in KHz Value: 450000->52600000 */
    uint32_t            nULAbsFrePointA; /**< Abs Freq Point A of the Carrier Center Frequency for in KHz Value: 450000->52600000 */
    uint32_t            nDLCenterFreqARFCN;   /**< center frequency for DL in MHz */
    uint32_t            nULCenterFreqARFCN;   /**< center frequency for UL in MHz */
    xran_fh_tti_callback_fn     ttiCb;        /**< call back for TTI event */
    void                *ttiCbParam;  /**< parameters of call back function */
    xran_fh_per_mu_cfg perMu[XRAN_MAX_NUM_MU];
    uint8_t mu_number[XRAN_MAX_NUM_MU];

    uint8_t enableCP;       /**<  enable C-plane */
    uint8_t dropPacketsUp;   /**<  enable droping the up channel packets if they miss timing window */
    uint8_t srsEnable;      /**<  enable SRS (Cat B specific) */
    uint8_t srsEnableCp;    /**<  enable SRS Cp(Cat B specific) */
    uint8_t SrsDelaySym;   /**<  enable SRS Cp(Cat B specific) */
    uint8_t puschMaskEnable;/**< enable pusch mask> */
    uint8_t puschMaskSlot;  /**< specific which slot pusch channel masked> */
    uint8_t csirsEnable;      /**<  enable CSI-RS (Cat B specific) */
    int32_t debugStop;      /**<  enable auto stop */
    int32_t debugStopCount;      /**<  enable auto stop after number of Tx packets */
    int32_t DynamicSectionEna; /**<  enable dynamic C-Plane section allocation */
    int32_t GPS_Alpha;  // refer to alpha as defined in section 9.7.2 of ORAN spec. this value should be alpha*(1/1.2288ns), range 0 - 1e7 (ns)
    int32_t GPS_Beta;   //beta value as defined in section 9.7.2 of ORAN spec. range -32767 ~ +32767
    uint8_t numMUs;     // Number of numerologies
    uint8_t nNumerology[XRAN_MAX_NUM_MU]; /**< Numerology, determine sub carrier spacing, Value: 0->4
                                       0: 15khz,  1: 30khz,  2: 60khz
                                       3: 120khz, 4: 240khz.
                                       At least 1 numerology must be configured. First specified numerology will be considered
                                       primary numerology and rest will be considered secondary numerologies. Secondary
                                       numerologies can be dynamically scheduled or completely turned off. */
    // int32_t freqOffset[XRAN_MAX_NUM_MU];
    uint16_t RemoteMACvalid;
    uint16_t numRemoteMAC;
    struct rte_ether_addr RemoteMAC[XRAN_MAX_RU_ETH_POINTS];
    struct xran_srs_config       srs_conf;     /**< SRS specific configurations for FH */
    struct xran_frame_config     frame_conf;   /**< frame config */
    struct xran_ru_config        ru_conf;      /**< config of RU as per XRAN spec */
    struct xran_csirs_config     csirs_conf;   /**< CSI-RS specific configurations for FH */

    phy_encoder_poll_fn bbdev_enc; /**< call back to poll BBDev encoder */
    phy_decoder_poll_fn bbdev_dec; /**< call back to poll BBDev decoder */
    phy_srs_fft_poll_fn bbdev_srs_fft; /**< call back to poll BBDev SRS FFT */
    phy_prach_ifft_poll_fn bbdev_prach_ifft; /**< call back to poll BBDev PRACH IFFT */

    uint16_t tx_cp_eAxC2Vf[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR*2 + XRAN_MAX_ANT_ARRAY_ELM_NR]; /**< mapping of C-Plane (ecpriRtcid) or U-Plane (ecpriPcid) to VF */
    uint16_t tx_up_eAxC2Vf[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR*2 + XRAN_MAX_ANT_ARRAY_ELM_NR]; /**< mapping of C-Plane (ecpriRtcid) or U-Plane (ecpriPcid) to VF */

    uint16_t rx_cp_eAxC2Vf[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR*2 + XRAN_MAX_ANT_ARRAY_ELM_NR]; /**< mapping of C-Plane (ecpriRtcid) or U-Plane (ecpriPcid) to VF */
    uint16_t rx_up_eAxC2Vf[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR*2 + XRAN_MAX_ANT_ARRAY_ELM_NR]; /**< mapping of C-Plane (ecpriRtcid) or U-Plane (ecpriPcid) to VF */

    uint32_t log_level; /**< configuration of log level */

    uint16_t max_sections_per_slot; /**< M-Plane settings for section */
    uint16_t max_sections_per_symbol; /**< M-Plane settings for section */
    int32_t RunSlotPrbMapBySymbolEnable; /**< enable prb mapping by symbol with multisection*/

    uint8_t dssEnable;  /**< enable DSS (extension-9) */
    uint8_t dssPeriod;  /**< DSS pattern period for LTE/NR */
    uint8_t technology[XRAN_MAX_DSS_PERIODICITY];   /**< technology array represents slot is LTE(0)/NR(1) */
    xran_active_numerologies_per_tti *activeMUs;    /**< Should be set per slot to true or false indicating whether
                                                         this numerology is active in this slot */
};


struct xran_cc_init
{
    int16_t  ccId;      /* Carrier component ID */

    enum xran_category ruCat;           /**< mode: Catergory A or Category B */ /* cannot be different within RU?? */
    enum xran_ran_tech ranTech;         /**< 5GNR or LTE */
    enum xran_comp_hdr_type cmpHdrType; /**< dynamic or static udCompHdr handling */

    uint8_t  iqWidth;           /**< IQ bit width */
    uint8_t  compMeth;          /**< Compression method */
    uint8_t  iqWidth_PRACH;     /**< IQ bit width for PRACH */
    uint8_t  compMeth_PRACH;    /**< Compression method for PRACH */

    uint32_t neAxc;             /**< number of eAxc supported on this CC */
    uint32_t neAxcUl;           /**< number of eAxc supported on this CC for UL direction */
    uint32_t nAntElmTRx;        /**< Number of antenna elements for TX and RX */
    uint32_t nDLAbsFrePointA;   /**< Abs Freq Point A of the Carrier Center Frequency for in KHz Value: 450000->52600000 */
    uint32_t nULAbsFrePointA;   /**< Abs Freq Point A of the Carrier Center Frequency for in KHz Value: 450000->52600000 */
    uint32_t nDLCenterFreqARFCN;/**< Center frequency for DL in MHz */
    uint32_t nULCenterFreqARFCN;/**< Center frequency for UL in MHz */

    uint32_t nDLBandwidth;      /**< Carrier bandwidth for in MHz. Value: 5->400 */
    uint32_t nULBandwidth;      /**< Carrier bandwidth for in MHz. Value: 5->400 */
    uint16_t nDLFftSize;        /**< DL FFT size */
    uint16_t nULFftSize;        /**< UL FFT size */
    uint16_t nDLRBs;            /**< DL PRB  */
    uint16_t nULRBs;            /**< UL PRB  */
    int32_t  freqOffset;

    uint8_t  nNumerology;       /**< Numerology, determine sub carrier spacing, Value: 0->4
                                        0: 15khz,  1: 30khz,  2: 60khz
                                        3: 120khz, 4: 240khz. */
    uint8_t  prachEnable;       /**<  enable PRACH   */
    //uint8_t prachConfigIndex;/**< TS36.211 - Table 5.7.1-2 : PRACH Configuration Index */
    //uint8_t prachConfigIndexLTE;/**< PRACH Configuration Index for LTE in dss case*/
    uint8_t  enableCP;          /**<  enable C-plane */
    uint8_t  srsEnable;         /**<  enable SRS (Cat B specific) */
    uint8_t  srsEnableCp;       /**<  enable SRS Cp(Cat B specific) */
    uint8_t  SrsDelaySym;       /**<  enable SRS Cp(Cat B specific) */
    uint8_t  puschMaskEnable;   /**< enable pusch mask> */
    uint8_t  puschMaskSlot;     /**< specific which slot pusch channel masked> */
    uint8_t  csirsEnable;       /**<  enable CSI-RS (Cat B specific) */
    int32_t  DynamicSectionEna; /**<  enable dynamic C-Plane section allocation */

    struct xran_prach_config prach_pri; /* PRACH configuration for primary (in case of DSS) */
    struct xran_prach_config prach_2nd; /* PRACH configuration for secondary (in case of DSS) */
    struct xran_frame_config frame_conf;/**< frame config */
    struct xran_srs_config srs_conf;    /**< SRS specific configurations for FH */
    struct xran_csirs_config csirs_conf;/**< CSI-RS specific configurations for FH */

    uint16_t maxSectionsPerSlot;        /**< M-Plane settings for section */
    uint16_t maxSectionsPerSymbol;      /**< M-Plane settings for section */
    int32_t  RunSlotPrbMapBySymbolEnable; /**< enable prb mapping by symbol with multisection*/

    uint8_t dssEnable;  /**< enable DSS (extension-9) */
    uint8_t dssPeriod;  /**< DSS pattern period for LTE/NR */
    uint8_t technology[XRAN_MAX_DSS_PERIODICITY];   /**< technology array represents slot is LTE(0)/NR(1) */
    xran_active_numerologies_per_tti *activeMUs;    /**< Should be set per slot to true or false indicating whether
                                                         this numerology is active in this slot */

    nbiot_ul_scs nbIotUlScs;     /**< Applicable only for NB-IOT (mu=4). NBIOT supports asymmetric SCS usage in
                                      downlink and uplink directions. xran library will use this parameter to derive
                                      slot-duration for UL NB-IOT:
                                      XRAN_NBIOT_UL_SCS_15: slot-duration=1ms
                                      XRAN_NBIOT_UL_SCS_3_75: slot-duration=2ms */
};

/**
 * @ingroup xran
 * XRAN front haul statistic counters according to Table 7 1 : Common Counters for both DL and UL */
struct xran_common_counters{
    uint64_t gps_second;
    uint64_t Rx_on_time;      /**< Data was received on time (applies to user data reception window) */
    uint64_t Rx_early;        /**< Data was received too early (applies to user data reception window) */
    uint64_t Rx_late;         /**< Data was received too late (applies to user data reception window) */
    uint64_t Rx_corrupt;      /**< Corrupt/Incorrect header packet */
    uint64_t Rx_pkt_dupl;     /**< Duplicated packet */
    uint64_t Total_msgs_rcvd; /**< Total messages received (on all links) */

    /* debug statistis */
    uint64_t rx_counter;
    uint64_t old_rx_counter;
    uint64_t rx_counter_pps;
    uint64_t tx_counter;
    uint64_t old_tx_counter;
    uint64_t tx_counter_pps;
    uint64_t tx_bytes_counter;
    uint64_t old_tx_bytes_counter;
    uint64_t rx_bytes_counter;
    uint64_t old_rx_bytes_counter;
    uint64_t tx_bytes_per_sec;
    uint64_t tx_bits_per_sec;
    uint64_t rx_bytes_per_sec;
    uint64_t rx_bits_per_sec;

    uint64_t rx_pusch_packets[XRAN_MAX_ANTENNA_NR];
    uint64_t rx_prach_packets[XRAN_MAX_ANTENNA_NR];
    uint64_t rx_srs_packets;
    uint64_t rx_csirs_packets;
    uint64_t rx_invalid_ext1_packets; /**< Counts the invalid extType-1 packets - valid for packets received from O-DU*/

    uint64_t timer_missed_sym;
    uint64_t timer_missed_slot;
#ifdef POLL_EBBU_OFFLOAD
    uint64_t timer_missed_sym_window;
#endif

    /* Error counters */
    uint32_t rx_err_drop; /**< Table 9.1-1 The total number of inbound messages which are discarded by the receiving O-RAN entity for any reason */

    uint32_t rx_err_up;      /** < (Internal counter) Number of packets dropped due to errors in UP data section header */
    uint32_t rx_err_pusch;   /** < (Internal counter) Number of PUSCH packets dropped due to errors in UP data section header */
    uint32_t rx_err_csirs;   /** < (Internal counter) Number of CSI-RS packets dropped due to errors in UP data section header */
    uint32_t rx_err_srs;     /** < (Internal counter) Number of SRS packets dropped due to errors in UP data section header */
    uint32_t rx_err_prach;   /** < (Internal counter) Number of PRACH packets dropped due to errors in UP data section header */
    uint32_t rx_err_cp;      /** < (Internal counter) Number of packets dropped due to errors in CP section header */
    uint32_t rx_err_ecpri;   /** < (Internal counter) Number of packets dropped due to error in eCPRI header */
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
    uint32_t nElementLenInBytes; /**< The Element length specified in bytes.
                                   * This parameter specifies the size of a single element in the buffer.
                                   * The total size of the buffer is described as
                                   * bufferSize = nElementLenInBytes * nNumberOfElements */
    uint32_t nNumberOfElements;  /**< The number of elements in the physical contiguous memory segment */
    uint32_t nOffsetInBytes;     /**< Offset in bytes to the start of the data in the physical contiguous
                                   * memory segment */
    uint8_t *pData;         /**< The data pointer is a virtual address */
    void *pCtrl;            /**< pointer to control section coresponding to data buffer */
    void *pRing;            /**< pointer to ring with prepared mbufs */
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
 *****************************************************************************/
struct xran_buffer_list
{
    uint32_t nNumBuffers; /**< Number of pointers */
    struct xran_flat_buffer *pBuffers;
    void *pUserData;
    void *pPrivateMetaData;
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
 * Clean XRAN Layer resources.
 *
 * @param
 *  NONE
 *
 * @return
 *  NONE
 */
void xran_cleanup(void);

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
int32_t xran_sector_get_instances (uint32_t xran_port, void * pDevHandle, uint16_t nNumInstances,
               xran_cc_handle_t * pSectorInstanceHandles);

/**
 * @ingroup xran
 *
 *   Function frees the allocated handle for the sectors.
 *
 * @param pSectorInstanceHandles
 *   Pointer to xran_cc_handle_t which has the handle pointer
 *
 * @return
 *   0 - on success
 */
int32_t xran_sector_free_instance(xran_cc_handle_t *pSectorInstanceHandle);

/**
 * @ingroup xran
 *
 *   Function initialize Memory Management leak detector in order to calculate allocated memory through DPDK
 *
 * @return
 *   0 - on success
 */
uint32_t xran_mem_mgr_leak_detector_init(void);

/**
 * @ingroup xran
 *
 *   Function destroy Memory Management leak detector
 *
 * @return
 *
 */
void xran_mem_mgr_leak_detector_destroy(void);

/**
 * @ingroup xran
 *
 *  @description
 *   Forms memory leak detector final output message
 *
 *  @param[in]   last - 32-bit flag indicating that function will be called last time
 *
 * @return
 *   0 - on success
 */
uint32_t xran_mem_mgr_leak_detector_display(uint32_t last);

typedef uint32_t(*xran_mem_mgr_leak_detector_add_cb_fn) (uint32_t nSize, char *pString, void* pMemBlk);
typedef uint32_t(*xran_mem_mgr_leak_detector_remove_cb_fn)(void* pMemBlk);

/**
 * @ingroup xran
 *
 *  @description
 *   Registers external leak detector functions
 *
 *  @param[in]   pxRANAddFn - Pointer to Add function
 *  @param[in]   pxRANRemoveFn - Pointer to Remove function
 *
 * @return
 *   0 - on success
 *   -1 - on failure
 */
int32_t xran_mem_mgr_leak_detector_register_cb_fn(xran_mem_mgr_leak_detector_add_cb_fn pxRANAddFn, xran_mem_mgr_leak_detector_remove_cb_fn pxRANRemoveFn);

/**
 * @ingroup xran
 *
 *   Function allocates memory of given size from DPDK
 *
 * @param buf_len
 *   buffer size
 *
 * @return
 *   buf_len - size of memory allocation
 */
void* xran_malloc(char *name, size_t buf_len, uint32_t align);
void* xran_zmalloc(char *name, size_t buf_len, uint32_t align);
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
void xran_free(void *addr);

struct rte_mempool * xran_pktmbuf_pool_create(const char *name, unsigned int n,
	unsigned int cache_size, uint16_t priv_size, uint16_t data_room_size, int socket_id);

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
 *   Function allocates buffer memory (bm) used between XRAN layer and PHY. In general case it's DPDK mempool.
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
 *   Function free buffer memory (bm) pool used between XRAN layer and PHY. In general case it's DPDK mempool.
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param nPoolIndex
 *   pointer to buffer pool identification to be returned
 *
 * @return
 *   0 - on success
 */
int32_t xran_bm_release(void *pHandle, uint32_t *pPoolIndex);

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
 *   Function allocates ring buffer used between XRAN layer and PHY.
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param rng_name_prefix
 *   prefix of ring name
 * @param cc_id
 *   Component Carrier ID
 * @param buff_id
 *   Buffer id for given ring
 * @param ant_id
 *   Antenna id for given ring
 * @param symb_id
 *   Symbol id for given ring
 * @param ppRing
 *   Pointer to pointer where to store address of internal DPDK ring
 *
 * @return
 *   0 - on success
 */
int32_t xran_bm_allocate_ring(void * pHandle, const char *rng_name_prefix, uint16_t cc_id, uint16_t buff_id, uint16_t ant_id, uint16_t symb_id, void **ppRing);

/**
 * @ingroup xran
 *
 *   Function free ring
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param pRing
 *   Pointer to ring structure
 *
 * @return
 *   0 - on success
 */
int32_t xran_bm_free_ring(void *pHandle, void *pRing);

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
 * @param mu
 *   Numerology for which to call this callback
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
                    void *pCallbackTag, uint8_t mu);

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
 * @param mu
 *   Numerology for which to call this callback
 *
 * @return
 *   0  - on success
 *   -1 - on error
 */
int32_t xran_5g_prach_req (void *  pHandle,
                struct xran_buffer_list *pDstBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pDstBufferDecomp[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                xran_transport_callback_fn pCallback,
                void *pCallbackTag, uint8_t mu);

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
 * @param mu
 *   Numerology for which to call this callback
 *
 * @return
 *   0  - on success
 *   -1 - on error
 */
int32_t xran_5g_srs_req (void *  pHandle,
                struct xran_buffer_list *pDstBuffer[XRAN_MAX_ANT_ARRAY_ELM_NR][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pDstCpBuffer[XRAN_MAX_ANT_ARRAY_ELM_NR][XRAN_N_FE_BUF_LEN],
                xran_transport_callback_fn pCallback,
                void *pCallbackTag, uint8_t mu);

/**
 * @ingroup xran
 *   Function configures CSI-RS input buffers to deliver CSIRS IQs from PHY layer to xran
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param pSrcBuffer
 *   list of memory buffers to use to deliver CSIRS IQs from PHY layer to xran
 * @param mu
 *   Numerology for which to call this callback
 * @return
 *   0  - on success
 *   -1 - on error
 */
int32_t xran_5g_csirs_config (void *  pHandle,
                struct xran_buffer_list *pSrcBuffer[XRAN_MAX_CSIRS_PORTS][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pSrcCpBuffer[XRAN_MAX_CSIRS_PORTS][XRAN_N_FE_BUF_LEN],
                xran_transport_callback_fn pCallback,
                void *pCallbackTag,uint8_t mu);


int32_t xran_5g_ssb_config(uint8_t ssbMu,
                           uint8_t actualMu,
                           void *pHandle,
                           struct xran_buffer_list *pSsbTxBuf[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                           struct xran_buffer_list *pSsbTxPrbBuf[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN]);

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
int32_t xran_lte_fronthault_config(void *pHandle,
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
int32_t xran_lte_prach_req (void *  pHandle,
                struct xran_buffer_list *pDstBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pDstBufferDecomp[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
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
int32_t xran_lte_srs_req (void *  pHandle,
                struct xran_buffer_list *pDstBuffer[XRAN_MAX_ANT_ARRAY_ELM_NR][XRAN_N_FE_BUF_LEN],
                struct xran_buffer_list *pDstCpBuffer[XRAN_MAX_ANT_ARRAY_ELM_NR][XRAN_N_FE_BUF_LEN],
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
uint32_t xran_get_time_stats(uint64_t *total_time, uint64_t *used_time, uint32_t *num_core_used, uint32_t *core_used, uint32_t clear);
uint32_t xran_timingsource_get_timestats(uint64_t *total_time, uint64_t *used_time, uint32_t *num_core_used, uint32_t *core_used, uint32_t clear);

/**
 * @ingroup xran
 *
 *   Function separates the start of timing thread from xran per port processing.
 *   It sets the timer thread state as RUNNING and starts the timer thread with xran_timing_source_thread().
 *   The per port initializations and XAN layer start will be handled post this.
 *
 * @return
 *   0  - on success
 *   -1 - on error
 */
int32_t xran_timingsource_start(void);

/**
 * @ingroup xran
 *
 *   Function starts the worker cores for XRAN after timing thread has been started.
 *
 * @return
 *   0  - on success
 *   -1 - on error
 */
int32_t xran_start_worker_threads();

/**
 * @ingroup xran
 *
 *   Function separates the stop of timing thread and XRAN layer. Handled post XRAN layer stop.
 *
 * @return
 *   0  - on success
 *   -1 - on error
 */
int32_t xran_timingsource_stop(void);

int32_t xran_timingsource_get_threadstate(void);


/**
 * @ingroup xran
 *
 *   Function opens XRAN layer with given handle
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param pConf
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
 *   Pointer to XRAN layer handle for RU
 *
 * @return
 *   0 - on success
 */
int32_t xran_start(void *pHandle);

/**
 * @ingroup xran
 *
 *   Function enable a Carrier Component in the RU.
 *   Given CC starts transmitting/receiving packets.
 *
 * @param port_id
 *   Port(RU) ID
 * @param cc_id
 *   Carrier Component ID in the Port
 *
 * @return
 *   0 - on success
 */
int32_t xran_activate_cc(int32_t port_id, int32_t cc_id);

/**
 * @ingroup xran
 *
 *   Function disable a Carrier Component in the RU.
 *   Given CC stops transmitting/receiving packets.
 *
 * @param port_id
 *   Port(RU) ID
 * @param cc_id
 *   Carrier Component ID in the Port
 *
 * @return
 *   0 - on success
 */
int32_t xran_deactivate_cc(int32_t port_id, int32_t cc_id);

/**
 * @ingroup xran
 *
 *   Function stops XRAN layer with given handle
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for RU
 *
 * @return
 *   0 - on success
 */
int32_t xran_stop(void *pHandle);
/**
 * @ingroup xran
 *
 *   Function shuts down XRAN layer
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for RU
 *
 * @return
 *   0 - on success
 */
int32_t xran_shutdown(void *pHandle);

/**
 * @ingroup xran
 *
 *   Function closes XRAN layer with given handle
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for RU
 *
 * @return
 *   0 - on success
 */
int32_t xran_close(void *pHandle);

/**
 * @ingroup xran
 *
 *   Function checks RU is active state
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 *
 * @return
 *   0 - on success
 */
int32_t xran_get_ru_isactive(void *pHandle);

/**
 * @ingroup xran
 *
 *   Function checks CC in specified RU is active
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param cc_id
 *   Carrier Component ID in the RU
 *
 * @return
 *   0 - on success
 */
int32_t xran_get_cc_isactive(void *pHandle, uint32_t cc_id);

/**
 * @ingroup xran
 *
 *   Function registers callback to XRAN layer. Function support callbacks aligned on packet arrival for all the
 *   numerologies configured on the xran port.
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param symCb
 *   pointer to callback function
 * @param symCbParam
 *   pointer to Callback Function parameters
 * @param symb
 *   symbol to be register for
 * @param cb_per_sym_type_id
 *   call back time identification (see enum cb_per_sym_type_id)
 * @param mu
 *   Numerology for which to call this callback
 *
 * @return
 *    0 - in case of success
 *   -1 - in case of failure
 */
int32_t xran_reg_sym_cb(void *pHandle, xran_callback_sym_fn symCb, void * symCbParam, struct xran_sense_of_time* symCbTime,
        uint8_t symb, enum cb_per_sym_type_id cb_sym_t_id, uint8_t mu);


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
int32_t xran_timingsource_reg_tticb(__attribute__((unused)) void *pHandle, xran_fh_tti_callback_fn Cb, void *cbParam, int skipTtiNum, enum callback_to_phy_id id);

/**
 * @ingroup xran
 *
 * Function registers callback to XRAN layer
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param Cb
 * @return
 *    0 - in case of success
 *   -1 - in case of failure
 */
int32_t xran_reg_physide_oam_cb(void *pHandle, xran_callback_oam_notify_fn Cb);


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
int32_t xran_get_slot_idx (uint32_t PortId, uint32_t *nFrameIdx, uint32_t *nSubframeIdx,  uint32_t *nSlotIdx, uint64_t *nSecond, uint8_t mu);
int32_t xran_timingsource_get_slotidx (uint32_t *nFrameIdx, uint32_t *nSubframeIdx,  uint32_t *nSlotIdx, uint64_t *nSecond, uint8_t mu);

/**
 * @ingroup xran
 *
 *   Function returns whether it is a prach slot or not based on given port and slot number
 *
 * @param PortId
 *    xRAN Port Id
 *
 * @param subframe_id
 *    Subframe number [0-9]
 *
 * @param slot_id
 *    Pointer to Slot number [0-7]
 *
 * @return
 *   whether it is a prach slot or not
 */
int32_t xran_is_prach_slot(uint8_t PortId, uint32_t subframe_id, uint32_t slot_id, uint8_t mu);

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
 * @brief Common function to print the XRAN Error Counters
 */
void xran_print_error_stats(struct xran_common_counters* x_counters);

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

/**
 * @ingroup xran
 *
 *   Debug function to trigger stop on 1pps (GPS second) boundary
 *
 * @param value
 *   1 - enable stop
 *   0 - disable stop
 * @param count
 *   enable auto stop after number of Tx packets
 * @return
 *    0 - on success
 */
int32_t xran_set_debug_stop(int32_t value, int32_t count);

/**
 * @ingroup xran
 *
 *   function initialize PRB map from config input
 *
 * @param p_PrbMapIn
 *   Input PRBmap from config
 * @param p_PrbMapOut
 *   Output PRBmap
 * @return
 *    0 - on success
 */
int32_t xran_init_PrbMap_from_cfg(struct xran_prb_map* p_PrbMapIn, struct xran_prb_map* p_PrbMapOut, uint32_t mtu);

/**
 * @ingroup xran
 *
 *   function initialize PRB map from config input
 *
 * @param p_PrbMapIn
 *   Input PRBmap from config for Rx
 * @param p_PrbMapOut
 *   Output PRBmap
 * @return
 *    0 - on success
 */

int32_t xran_init_PrbMap_from_cfg_for_rx(struct xran_prb_map* p_PrbMapIn, struct xran_prb_map* p_PrbMapOut, uint32_t mtu);

int32_t xran_get_num_prb_elm(struct xran_prb_map* p_PrbMapIn, uint32_t mtu);

/**
 * @ingroup xran
 *
 *   function initialize PRB map from config input by symbol
 *
 * @param p_PrbMapIn
 *   Input PRBmap from config
 * @param p_PrbMapOut
 *   Output PRBmap
 * @return
 *    0 - on success
 */
int32_t xran_init_PrbMap_by_symbol_from_cfg(struct xran_prb_map* p_PrbMapIn, struct xran_prb_map* p_PrbMapOut, uint32_t mtu, uint32_t xran_max_prb);

/**
 * @ingroup xran
 *
 *   Function prepares DL U-plane packets for symbol for O-RAN FH. Enques resulting packet to ring for TX at appropriate time
 *
 * @param pHandle
 *    pointer to O-RU port structure
 * @return
 *    0 - on success
 */
int32_t xran_prepare_up_dl_sym(uint16_t xran_port_id, uint32_t nSlotIdx,  uint32_t nCcStart, uint32_t nCcNum, uint32_t nSymMask, uint32_t nAntStart,
                            uint32_t nAntNum, uint32_t nSymStart, uint32_t nSymNum, uint8_t mu);
/**
 * @ingroup xran
 *
 *   Function prepares DL C-plane packets for slot for O-RAN FH. Enques resulting packet to ring for TX at appropriate time
 *
 * @param pHandle
 *    pointer to O-RU port structure
 * @return
 *    0 - on success
 */
int32_t xran_prepare_cp_dl_slot(uint16_t xran_port_id, uint32_t nSlotIdx,  uint32_t nCcStart, uint32_t nCcNum, uint32_t nSymMask, uint32_t nAntStart,
                            uint32_t nAntNum, uint32_t nSymStart, uint32_t nSymNum, uint8_t mu);

/**
 * @ingroup xran
 *
 *   Function prepares UL C-plane packets for slot for O-RAN FH. Enques resulting packet to ring for TX at appropriate time
 *
 * @param pHandle
 *    pointer to O-RU port structure
 * @return
 *    0 - on success
 */

int32_t xran_prepare_cp_ul_slot(uint16_t xran_port_id, uint32_t nSlotIdx,  uint32_t nCcStart, uint32_t nCcNum, uint32_t nSymMask, uint32_t nAntStart,
                            uint32_t nAntNum, uint32_t nSymStart, uint32_t nSymNum, uint8_t mu);


int32_t xran_get_numerology_status_buffer_pointer(uint16_t xran_port_id, xran_active_numerologies_per_tti **activeMuPerTti);

/**
 * @ingroup xran
 *
 *   Function prepares UL U-plane, PRACH, SRS packets for symbol for O-RAN FH. Enques resulting packet to ring for TX at appropriate time
 *
 * @param pHandle
 *    pointer to O-RU port structure
 * @return
 *    0 - on success
 */

int32_t xran_prepare_up_ul_tx_sym(uint16_t xran_port_id, uint32_t nSfIdx,  uint32_t nCcStart, uint32_t nCcNum, uint32_t nSymMask, uint32_t nAntStart,
                            uint32_t nAntNum, uint32_t nSymStart, uint32_t nSymNum, uint8_t mu);
void xran_l1budget_calc(uint8_t numerology, uint16_t t1a_max_up, uint16_t ta4_max,uint16_t *ul_budget,uint16_t *dl_budget, uint16_t *num_sym);


/** Function returns xran port id (RU) through local VF MAC address */
uint32_t xran_get_RUport_by_MACaddr(uint8_t nSrcMacAddress[6]);

int32_t xran_fetch_and_print_lbm_stats(bool print_xran_lbm_stats, uint8_t *link_status , uint8_t vfId);

void xran_config_dpdk_process_id_tag(int32_t Tag);

int xran_get_memstat(struct xran_memstat *stat);

#ifdef POLL_EBBU_OFFLOAD
/* All functions/variables defined below are to support the polling event processing feature of eBBUPOOL framework*/

/** Function removes all the timing related callback functions */
int32_t xran_timing_destroy_cbs(void *args);

/** Timer context structure when macro POLL_EBBU_OFFLOAD is enabled */
typedef struct _XRAN_TIMER_CTX_
{
    struct timespec ebbu_offload_last_time;
    // volatile struct timespec ebbu_offload_last_time;
    struct timespec ebbu_offload_cur_time;
    // volatile struct timespec ebbu_offload_cur_time;
    long current_second;
    // volatile unsigned long current_second;
    uint32_t ebbu_offload_ota_tti_cnt_mu[XRAN_PORTS_NUM][XRAN_MAX_NUM_MU];
    uint32_t ebbu_offload_ota_sym_cnt_mu[XRAN_MAX_NUM_MU];
    uint32_t ebbu_offload_ota_sym_idx_mu[XRAN_MAX_NUM_MU];
    int32_t first_call;
    int32_t sym_up_window;
    uint64_t used_tick;
    uint16_t xran_SFN_at_Sec_Start; /**< SFN at current second start */
    int32_t (*pFn)(uint32_t, uint32_t, uint32_t, void*);
} XRAN_TIMER_CTX, *PXRAN_TIMER_CTX;

 /** Function returns the context of XRAN timer structure when macro POLL_EBBU_OFFLOAD is enabled */
PXRAN_TIMER_CTX xran_timer_get_ctx_ebbu_offload(void);

 /** Function returns TTI interval when macro POLL_EBBU_OFFLOAD is enabled */
long xran_timer_get_interval_ebbu_offload(void);

 /** Function returns TTI interval when macro POLL_EBBU_OFFLOAD is enabled */
int32_t xran_sym_poll_task_ebbu_offload(void);

 /** Function to do time adjustment according to section 9.7.2 of ORAN spec when macro POLL_EBBU_OFFLOAD is enabled */
int32_t timing_adjust_gps_second_ebbu_offload(struct timespec* p_time);

 /** Function makes callbacks according to symbol timing when macro POLL_EBBU_OFFLOAD is enabled */
int32_t xran_sym_poll_callback_task_ebbu_offload(void);

 /** Function offloads FH packet polling and processsing when macro POLL_EBBU_OFFLOAD is enabled */
int32_t xran_pkt_proc_poll_task_ebbu_offload(void);

 /** Function offloads the DL CP processsing when macro POLL_EBBU_OFFLOAD is enabled */
int32_t xran_task_dl_cp_ebbu_offload(void *arg);

 /** Function offloads the UL CP processsing when macro POLL_EBBU_OFFLOAD is enabled */
int32_t xran_task_ul_cp_ebbu_offload(void *arg);

 /** Function offloads TTI processsing when macro POLL_EBBU_OFFLOAD is enabled */
int32_t xran_task_tti_ebbu_offload(void *arg);

 /** Function updates SFN when macro POLL_EBBU_OFFLOAD is enabled */
void xran_updateSfnSecStart(void);

 /** Function returns FH counter structure when macro POLL_EBBU_OFFLOAD is enabled */
struct xran_common_counters* xran_fh_counters_ebbu_offload(void);

 /** Function returns device context structure when macro POLL_EBBU_OFFLOAD is enabled */
struct xran_device_ctx *xran_dev_get_ctx_ebbu_offload(void);

#endif

#ifdef __cplusplus
}
#endif

#endif /* _XRAN_FH_O_DU_H_*/
