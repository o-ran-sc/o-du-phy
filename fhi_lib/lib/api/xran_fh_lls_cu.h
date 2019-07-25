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
 * @brief This file provides public interface to XRAN Front Haul layer implementation as defined in the
 *      XRAN-FH.CUS.0-v02.00 spec. Implementation is specific to lls-CU node
 *      for 5G NR Radio Access technology
 *
 * @file xran_fh_lls_cu.h
 * @ingroup group_lte_source_xran
 * @author Intel Corporation
 *
 **/

#ifndef _XRAN_FH_LLS_CU_H_
#define _XRAN_FH_LLS_CU_H_

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

/** Macro to calculate TTI number [0:7999] from symbol index [0: 112000-1] used by timing thread */
#define XranGetTtiNum(symIdx, numSymPerTti) (((uint32_t)symIdx / (uint32_t)numSymPerTti))
/** Macro to calculate Symbol number [0:7] for given slot from symbol index [0: 112000-1] */
#define XranGetSymNum(symIdx, numSymPerTti) (((uint32_t)symIdx % (uint32_t)numSymPerTti))
/** Macro to calculate Frame number [0:99] for given tti [0: 7999] */
#define XranGetFrameNum(tti,numSubFramePerSystemFrame, numSlotPerSubFrame)  ((uint32_t)tti / ((uint32_t)numSubFramePerSystemFrame * (uint32_t)numSlotPerSubFrame))
/** Macro to calculate Subframe number [0:9] for given tti [0: 7999] */
#define XranGetSubFrameNum(tti, numSlotPerSubFrame, numSubFramePerSystemFrame) (((uint32_t)tti/(uint32_t)numSlotPerSubFrame) % (uint32_t)numSubFramePerSystemFrame)
/** Macro to calculate Slot number [0:7] for given tti [0: 7999] */
#define XranGetSlotNum(tti, numSlotPerSfn) ((uint32_t)tti % ((uint32_t)numSlotPerSfn))

#define XRAN_PORTS_NUM      (1) /**< number of XRAN ports supported */
#define XRAN_N_FE_BUF_LEN   (80)/** Number of TTIs (slots) */
#define XRAN_MAX_SECTOR_NR  (4) /**< Max sectors per XRAN port */
#define XRAN_MAX_ANTENNA_NR (4) /**< Max antenna per port */
#define XRAN_NUM_OF_SYMBOL_PER_SLOT          ( 14 ) /**< Number of symbols per slot */

#define XRAN_MAX_CELLS_PER_PORT      (4) /**< Max cells mapped to XRAN port */
#define XRAN_COMPONENT_CARRIERS_MAX  XRAN_MAX_SECTOR_NR /**< number of CCs */
#define XRAN_NUM_OF_ANT_RADIO        16  /**< Max Number of Antennas supported for all CC on single XRAN port */

#define XRAN_MAX_PKT_BURST (448+4) /**< 4x14x8 symbols per ms */
#define XRAN_N_MAX_BUFFER_SEGMENT XRAN_MAX_PKT_BURST /**< Max number of segments per ms */

#define XRAN_STRICT_PARM_CHECK      (1) /**< enable parameter check for C-plane */

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

typedef int32_t XranStatusInt32; /**< Xran status return value */

/** callback function type for Symbol packet */
typedef void (*XRANFHSYMPROCCB)(void*);

/** Callback function type for TTI event */
typedef int (*XRANFHTTIPROCCB)(void* );

/** Callback function type packet arrival from transport layer (ETH or IP) */
typedef void (*XranTransportBlockCallbackFn)(void*, int32_t);

/**
* Component Carrier Initialization
*/
typedef struct tagXRANCCINIT
{
    uint32_t RadioMode; /**< XRAN mode Cat A or Cat B on given CC */
    uint32_t nTxAnt;    /**< Number of TX antennas */
    uint32_t nRxAnt;    /**< Number of RX antennas */
    uint32_t radiobw;   /**< bandwidth id */
    uint32_t dpdk_port; /**< networking layer port id */
    char *dpdk_pcie_eth_dev; /**< pcie device for this cc */
    char *ru_mac_str;       /**< mac address of RU */
    uint32_t ulAgc;     /**< state of UL AGC (ON/OFF) */
    uint32_t numCell;   /**< Number of Cells per port per CC */
    uint32_t phyInsId[XRAN_MAX_CELLS_PER_PORT]; /**< Mapping of Cell ID to CC */
    uint32_t dpdkRxCore; /**< DPDK RX Core */
    uint32_t dpdkTxCore; /**< DPDK TX Core */
}XRANCCINIT, *PXRANCCINIT;

/** XRAN port enum */
enum xran_vf_ports
{
    XRAN_UP_VF = 0, /**< port type for U-plane */
    XRAN_CP_VF,     /**< port type for C-plane */
    XRAN_VF_MAX
};

/** DPDK IO configuration for XRAN layer */
typedef struct tagXRAN_IO_LOOP_CFG
{
    uint8_t id;
    char *dpdk_dev[XRAN_VF_MAX];
    int core;
    int system_core;    /* Needed as DPDK will change your starting core. */
    int pkt_proc_core;  /* Needed for packet processing thread. */
    int pkt_aux_core;   /* Needed for debug purposes. */
    int timing_core;    /* Needed for getting precise time */
    int port[XRAN_VF_MAX];  /* This is auto-detected, no need to set. */
}XRAN_IO_LOOP_CFG, *PXRAN_IO_LOOP_CFG;

/** XRAN spec section 3.1.3.1.6 ecpriRtcid / ecpriPcid define */
typedef struct tagXRANEAXCIDCONFIG
{
    uint16_t mask_cuPortId;     /**< Mask CU PortId */
    uint16_t mask_bandSectorId; /**< Mask Band */
    uint16_t mask_ccId;         /**< Mask CC */
    uint16_t mask_ruPortId;     /**< Mask RU Port ID */

    uint8_t bit_cuPortId;       /**< bandsectorId + ccId + ruportId */
    uint8_t bit_bandSectorId;   /**< ccId + ruPortId */
    uint8_t bit_ccId;           /**<  ruportId */
    uint8_t bit_ruPortId;       /**<  0 */
}XRANEAXCIDCONFIG, *PXRANEAXCIDCONFIG;

/**
* XRAN Front haul interface initialization settings
*/
typedef struct tagXRANFHINIT
{
    uint32_t llscuId;    /**< lls-cu ID */
    uint32_t nSec;       /**< number of sectors, shall be 1 */
    XRANCCINIT ccCfg[XRAN_COMPONENT_CARRIERS_MAX]; /**< configuration of each CCs */
    XRANEAXCIDCONFIG eAxCId_conf; /**< config of ecpriRtcid/ecpriPcid */
    uint32_t radio_iface;         /**< enable/disable radio */
    uint32_t dpdkMasterCore;      /**< master core of DPDK */
    uint32_t dpdkMemorySize;      /**< huge pages allocation for DPDK */
    uint32_t dpdkIrqMode;         /**< DPDK IRQ or PMD mode */
    uint32_t dpdkBasebandFecMode; /**< DPDK Baseband FEC device mode (0-SW, 1-HW) */
    char *dpdkBasebandDevice;     /**< DPDK Baseband device address */
    uint32_t singleThreadTxRx;
    uint32_t bbuPoolCores;  /**< DPDK cores for BBU pool */
    uint32_t radioEnabled;  /**< reserved */
    uint32_t powerSaveEn;   /**< reserved */
    char *filePrefix;       /**< DPDK prefix */
    XRAN_IO_LOOP_CFG io_cfg;/**< DPDK IO for XRAN */
    uint8_t xranMode;       /**< mode: lls-CU or RU */
    int8_t *p_lls_cu_addr;  /**<  lls-CU Ethernet Mac Address */
    int8_t *p_ru_addr;      /**<  RU Ethernet Mac Address */
    uint32_t ttiPeriod;     /**< TTI period */

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

    uint8_t enableCP;    /**<  enable C-plane */
    uint8_t cp_vlan_tag; /**<  C-plane vlan tag */
    uint8_t up_vlan_tag; /**<  U-plane vlan tag */
    int32_t debugStop;   /**<  enable auto stop */
} XRANFHINIT, *PXRANFHINIT;

/** XRAN Playback format */
typedef enum {
    XRAN_RADIO_PLAYBACK_TIME_DOMAIN = 0,
    XRAN_RADIO_PLAYBACK_FREQ_DOMAIN = 1
} XranPlaybackFormatEnum;

/* PRACH config required for XRAN based FH */
typedef struct tagXRANPRACHCONFIG
{
    /**** word 5 *****/
    /* PRACH config*/
    /** PRACH Configuration Index*/
    uint8_t      nPrachConfIdx;
    /** PRACH Sub-carrier spacing
    Value:0->1
    For below 6GHz the values indicate 15kHz or 30kHz
    For above 6GHz the values indicate 60kHz or 120kHz*/
    /*PRACH zeroCorrelationZoneConfig */
    uint8_t      nPrachSubcSpacing;
    /** PRACH zeroCorrelationZoneConfig */
    uint8_t      nPrachZeroCorrConf;
    /** PRACH restrictedSetConfig */
    uint8_t      nPrachRestrictSet;

    /**** word 6 *****/
    /** PRACH Root Sequence Index */
    uint16_t     nPrachRootSeqIdx;
    /** PRACH prach-frequency-start  */
    uint16_t     nPrachFreqStart;

    /** PRACH prach-frequency-offset */
    int32_t     nPrachFreqOffset;
    /** PRACH Filter index */
    uint8_t     nPrachFilterIdx;
}XRANPRACHCONFIG, *PXRANPRACHCONFIG;

/** XRAN front haul playback configuration (not supported in 19.03) */
typedef struct tagXRANFHPLAYBACK
{
    XranPlaybackFormatEnum TxPlayFormatType; /**< type of play back files [Time|Freq] */

    unsigned long TxPlayBufAddr[XRAN_NUM_OF_ANT_RADIO]; /**< pointer to buffers to play */
    uint32_t TxPlayBufSize; /**< Buffer size */

    char*      TxPlayFileName[XRAN_NUM_OF_ANT_RADIO]; /**< files to play */
    uint32_t   TxPlayFileSize; /**< expected the same size for all Ant */

}XRANPLAYBACKCONFIG,*PXRANPLAYBACKCONFIG;

/** XRAN front haul logging configuration (not supported in 19.03) */
typedef struct tagXRANFHLOGCONF
{
    /* logging */
    unsigned long TxLogBufAddr;
    uint32_t TxLogBufSize;

    unsigned long TxLogIfftInAddr;
    uint32_t  TxLogIfftInSize;

    unsigned long TxLogIfft1200InAddr;
    uint32_t  TxLogIfft1200InSize;

    unsigned long RxLogFftOutAddr;
    uint32_t  RxLogFftOutSize;

    unsigned long RxLogFftOutExpAddr;
    uint32_t  RxLogFftOutExpSize;

    unsigned long RxLogFftOutGainAddr;
    uint32_t  RxLogFftOutGainSize;

    unsigned long RxLogBufAddr;
    uint32_t  RxLogBufSize;

    unsigned long RxLogAlawBufAddr;
    uint32_t  RxLogAlawBufSize;

    unsigned long RxLogPrachBufAddr;
    uint32_t  RxLogPrachBufSize;

    uint32_t  cfg_dl_iq_buf_enabled;
    uint32_t  cfg_ul_iq_buf_enabled;

}XRANFHLOGCONF, *PXRANFHLOGCONF;

/** XRAN front haul frame config */
typedef struct tagXRANFRAMECONFIG
{
    /** Frame Duplex type:  0 -> FDD, 1 -> TDD */
    uint8_t      nFrameDuplexType;
    /** Numerology, determine sub carrier spacing, Value: 0->4
       0: 15khz,  1: 30khz,  2: 60khz
       3: 120khz, 4: 240khz */
    uint8_t      nNumerology;
    /** TDD period */
    uint8_t      nTddPeriod;
}XRANFRAMECONFIG, *PXRANFRAMECONFIG;

/** XRAN front haul BBU pooling config */
typedef struct tagXRANBBUPOOLCONFIG
{
    uint32_t isBbuPool; /**< FH running with BBU pool */
}XRANBBUPOOLCONFIG, *PXRANBBUPOOLCONFIG;

/** XRAN front haul IQ compression settings */
typedef struct tagXRANRUCONFIG
{
    uint8_t iqWidth;        /**< IQ bit width */
    uint8_t compMeth;       /**< Compression method */
    uint8_t fftSize;        /**< FFT Size */
}XRANRUCONFIG, *PXRANRUCONFIG;

/** XRAN front haul Phase compensation settings */
typedef struct
{
    uint32_t nSecNum;
    uint32_t nPhaseCompFlag;
    uint32_t nDlArfcn[XRAN_MAX_SECTOR_NR];
    uint32_t nUlArfcn[XRAN_MAX_SECTOR_NR];
}XRANPHASECompConfig;

/**
 * @ingroup xran
 *XRAN front haul general configuration */
typedef struct tagXRANFHCONFIG
{
    uint32_t            dpdk_port; /**< DPDK port number used for FH */
    uint32_t            sector_id; /**< Band sector ID for FH */
    uint32_t            nCC;       /**< number of Component carriers supported on FH */
    uint32_t            neAxc;     /**< number of eAxc supported on FH */
    XRANPLAYBACKCONFIG  playback_conf;/**< configuration of playback of IQs supported with FH */
    XRANFHLOGCONF       log_conf;     /**< config of logging functionality supported by FH */
    XRANFHTTIPROCCB     ttiCb;        /**< call back for TTI event */
    void                *ttiCbParam;  /**< parameters of call back function */
    XRANPRACHCONFIG     prach_conf;   /**< PRACH specific configurations for FH */
    XRANFRAMECONFIG     frame_conf;   /**< frame config */
    XRANBBUPOOLCONFIG   bbu_conf;     /**< BBU pool config */
    XRANRUCONFIG        ru_conf;      /**< config of RU as per XRAN spec */
    XRANPHASECompConfig phase_compensation; /**< phase compensation settings */
}XRANFHCONFIG, *PXRANFHCONFIG;


/**
 * @ingroup xran
 * CC instance handle pointer type */
typedef void * XranCcInstanceHandleVoidP;

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
typedef struct XRANFlatBuffer
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
} XRANFlatBufferStruct;

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
typedef struct XRANBufferList
{
    uint32_t nNumBuffers;
    /**< Number of pointers */
    XRANFlatBufferStruct *pBuffers;
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
} XRANBufferListStruct;

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
int32_t xran_init(int argc, char *argv[], PXRANFHINIT p_xran_fh_init, char *appName, void ** pHandle);

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
 *   Pointer to XranCcInstanceHandleVoidP where to store Handle pointer
 *
 * @return
 *   0 - on success
 */
int32_t xran_sector_get_instances (void * pHandle, uint16_t nNumInstances,
               XranCcInstanceHandleVoidP * pSectorInstanceHandles);

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
 * @param ppVirtAddr
 *   Pointer to pointer where to store address of new buffer
 *
 * @return
 *   0 - on success
 */
int32_t xran_bm_allocate_buffer(void * pHandle, uint32_t nPoolIndex, void **ppVirtAddr);

/**
 * @ingroup xran
 *
 *   Function frees buffer used between XRAN layer and PHY. In general case it's DPDK mbuf
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param pVirtAddr
 *   Pointer to buffer
 *
 * @return
 *   0 - on success
 */
int32_t xran_bm_free_buffer(void * pHandle, void *pVirtAddr);

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
 * @param pDstBuffer
 *   list of memory buffers to use to deliver IQs from XRAN layer to PHY (UL)
 * @param XranTransportBlockCallbackFn pCallback
 *   Callback function to call with arrival of all packets for given CC for given symbol
 * @param pCallbackTag
 *   Parameters of Callback function
 *
 * @return
 *   0  - on success
 *   -1 - on error
 */
int32_t xran_5g_fronthault_config (void * pHandle,
                XRANBufferListStruct *pSrcBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                XRANBufferListStruct *pDstBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                XranTransportBlockCallbackFn pCallback,
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
 * @param XranTransportBlockCallbackFn pCallback
 *   Callback function to call with arrival of PRACH packets for given CC
 * @param pCallbackTag
 *   Parameters of Callback function
 *
 * @return
 *   0  - on success
 *   -1 - on error
 */
int32_t xran_5g_prach_req (void *  pHandle,
                XRANBufferListStruct *pDstBuffer[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                XranTransportBlockCallbackFn pCallback,
                void *pCallbackTag);
/**
 * @ingroup xran
 *
 *   Function configures phase compensation for RU via XRAN layer with given handle
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param nTxPhaseCps
 *   TX(DL) phase compensation settings
 * @param nTxPhaseCps
 *   RX(UL) phase compensation settings
 * @param nSectorId
 *   Sector id to use with given settings
 *
 * @return
 *   0 - on success
 */
int32_t xran_5g_pre_compenstor_cfg(void* pHandle,
                uint32_t nTxPhaseCps,
                uint32_t nRxPhaseCps,
                uint8_t nSectorId);

/**
 * @ingroup xran
 *
 *   Function opens XRAN layer with given handle
 *
 * @param pHandle
 *   Pointer to XRAN layer handle for given CC
 * @param PXRANFHCONFIG pConf
 *   Pointer to XRAN configuration structure with specific settings to use
 *
 * @return
 *   0 - on success
 */
int32_t xran_open(void *pHandle, PXRANFHCONFIG pConf);

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
int32_t xran_reg_sym_cb(void *pHandle, XRANFHSYMPROCCB symCb, void * symCbParam, uint8_t symb, uint8_t ant);

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
int32_t xran_reg_physide_cb(void *pHandle, XRANFHTTIPROCCB Cb, void *cbParam, int skipTtiNum, enum callback_to_phy_id);

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

#ifdef __cplusplus
}
#endif

#endif /* _XRAN_FH_LLS_CU_H_*/
