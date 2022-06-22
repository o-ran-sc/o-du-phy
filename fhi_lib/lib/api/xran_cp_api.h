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
 * @brief This file provides the definitions for Control Plane Messages APIs.
 *
 * @file xran_cp_api.h
 * @ingroup group_lte_source_xran
 * @author Intel Corporation
 *
 **/

#ifndef _XRAN_CP_API_H_
#define _XRAN_CP_API_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "xran_fh_o_du.h"
#include "xran_pkt_cp.h"
#include "xran_transport.h"

#define XRAN_MAX_SECTIONDB_CTX              4

#define XRAN_MAX_NUM_EXTENSIONS     10//XRAN_MAX_PRBS /* Maximum number of extensions in a section [up to 1 ext section per RB]*/
#define XRAN_MAX_NUM_UE             16      /* Maximum number of UEs/Lyaers */
#define XRAN_MAX_NUM_ANT_BF         64      /* Maximum number of beamforming antenna,
                                             * could be defined as XRAN_MAX_ANTENNA_NR */
/* Maximum total number of beamforming weights (5.4.7.1.2) */
#define XRAN_MAX_BFW_N              (XRAN_MAX_NUM_ANT_BF*XRAN_MAX_NUM_UE)
#define XRAN_MAX_MODCOMP_ADDPARMS   6       /* max should be even number */

#define XRAN_SECTIONEXT_ALIGN       4       /* alignment size in byte for section extension */


/** Control Plane section types, defined in 5.4 Table 5.1 */
enum xran_cp_sectiontype {
    XRAN_CP_SECTIONTYPE_0   = 0,    /**< Unused RB or Symbols in DL or UL, not supported */
    XRAN_CP_SECTIONTYPE_1   = 1,    /**< Most DL/UL Radio Channels */
    XRAN_CP_SECTIONTYPE_3   = 3,    /**< PRACH and Mixed-numerology Channels */
    XRAN_CP_SECTIONTYPE_5   = 5,    /**< UE scheduling information, not supported  */
    XRAN_CP_SECTIONTYPE_6   = 6,    /**< Channel Information, not supported */
    XRAN_CP_SECTIONTYPE_7   = 7,    /**< LAA, not supported */
    XRAN_CP_SECTIONTYPE_MAX
    };

/** Filter index, defined in 5.4.4.3 */
enum xran_cp_filterindex {
    XRAN_FILTERINDEX_STANDARD   = 0,    /**< UL filter for standard channel */
    XRAN_FILTERINDEX_PRACH_012  = 1,    /**< UL filter for PRACH preamble format 0, 1, 2 */
    XRAN_FILTERINDEX_PRACH_3    = 2,    /**< UL filter for PRACH preamble format 3 */
    XRAN_FILTERINDEX_PRACH_ABC  = 3,    /**< UL filter for PRACH preamble format A1~3, B1~4, C0, C2 */
    XRAN_FILTERINDEX_NPRACH     = 4,    /**< UL filter for NPRACH */
    XRAN_FILTERINDEX_LTE4       = 5,    /**< UL filter for PRACH preamble format LTE-4 */
    XRAN_FILTERINDEX_MAX
    };

/** Maximum Slot Index, defined in 5.4.4.6 */
#define XRAN_SLOTID_MAX                     16

/** FFT size in frame structure, defined in 5.4.4.13 Table 5.9 */
enum xran_cp_fftsize {
    XRAN_FFTSIZE_128    = 7,       /* 128 */
    XRAN_FFTSIZE_256    = 8,       /* 256 */
    XRAN_FFTSIZE_512    = 9,       /* 512 */
    XRAN_FFTSIZE_1024   = 10,      /* 1024 */
    XRAN_FFTSIZE_2048   = 11,      /* 2048 */
    XRAN_FFTSIZE_4096   = 12,      /* 4096 */
    XRAN_FFTSIZE_1536   = 13,      /* 1536 */
    XRAN_FFTSIZE_MAX
    };

/** Sub-carrier spacing, defined in 5.4.4.13 Table 5.10 */
enum xran_cp_subcarrierspacing {   /*3GPP u,  SCS, Nslot, Slot len */
    XRAN_SCS_15KHZ      = 0,       /*  0,   15kHz,  1, 1ms */
    XRAN_SCS_30KHZ      = 1,       /*  1,   30kHz,  2, 500us */
    XRAN_SCS_60KHZ      = 2,       /*  2,   60kHz,  4, 250us */
    XRAN_SCS_120KHZ     = 3,       /*  3,  120kHz,  8, 125us */
    XRAN_SCS_240KHZ     = 4,       /*  4,  240kHz, 16, 62.5us */
    XRAN_SCS_1P25KHZ    = 12,      /* NA, 1.25kHz,  1, 1ms */
    XRAN_SCS_3P75KHZ    = 13,      /* NA, 3.75kHz,  1, 1ms */
    XRAN_SCS_5KHZ       = 14,      /* NA,    5kHz,  1, 1ms */
    XRAN_SCS_7P5KHZ     = 15,      /* NA,  7.5kHz,  1, 1ms */
    XRAN_SCS_MAX
    };

/** Resource block indicator, defined in 5.4.5.2 */
enum xran_cp_rbindicator {
    XRAN_RBIND_EVERY        = 0,    /**< every RB used */
    XRAN_RBIND_EVERYOTHER   = 1,    /**< every other RB used */
    XRAN_RBIND_MAX
    };

/** Symbol number increment command, defined in 5.4.5.3 */
enum xran_cp_symbolnuminc {
    XRAN_SYMBOLNUMBER_NOTINC    = 0,      /**< do not increment the current symbol number */
    XRAN_SYMBOLNUMBER_INC       = 1,      /**< increment the current symbol number and use that */
    XRAN_SYMBOLNUMBER_INC_MAX
    };

/** Macro to convert the number of PRBs as defined in 5.4.5.6 */
#define XRAN_CONVERT_NUMPRBC(x)             ((x) > 255 ? 0 : (x))

#define XRAN_CONVERT_IQWIDTH(x)             ((x) > 15 ? 0 : (x))

/** Minimum number of symbols, defined in 5.4.5.7 */
#define XRAN_SYMBOLNUMBER_MIN               1
/** Maximum number of symbols, defined in  5.4.5.7 */
#define XRAN_SYMBOLNUMBER_MAX               14

/* LAA message type 5.4.5.14 Table 5.11, not supported */
#define XRAN_LAAMSGTYPE_LBT_PDSCH_REQ       0
#define XRAN_LAAMSGTYPE_LBT_DRS_REQ         1
#define XRAN_LAAMSGTYPE_LBT_PDSCH_RSP       2
#define XRAN_LAAMSGTYPE_LBT_DRS_RSP         3
#define XRAN_LAAMSGTYPE_LBT_BUFFER_ERROR    4
#define XRAN_LAAMSGTYPE_LBT_CWCONFIG_REQ    5
#define XRAN_LAAMSGTYPE_LBT_CWCONFIG_RSP    6

#define XRAN_LBTMODE_FULL                   0
#define XRAN_LBTMODE_PARTIAL25              1
#define XRAN_LBTMODE_PARTIAL34              2
#define XRAN_LBTMODE_FULLSTOP               3


#define XRAN_EF_F_LAST                      0
#define XRAN_EF_F_ANOTHER_ONE               1

/** Control Plane section extension commands, defined in 5.4.6 Table 5.13 */
enum xran_cp_sectionextcmd {
    XRAN_CP_SECTIONEXTCMD_0 = 0,    /**< Reserved, for future use */
    XRAN_CP_SECTIONEXTCMD_1 = 1,    /**< Beamforming weights */
    XRAN_CP_SECTIONEXTCMD_2 = 2,    /**< Beamforming attributes */
    XRAN_CP_SECTIONEXTCMD_3 = 3,    /**< DL Precoding configuration parameters and indications, not supported */
    XRAN_CP_SECTIONEXTCMD_4 = 4,    /**< Modulation compression parameter */
    XRAN_CP_SECTIONEXTCMD_5 = 5,    /**< Modulation compression additional scaling parameters */
    XRAN_CP_SECTIONEXTCMD_6 = 6,    /**< Non-contiguous PRB allocation */
    XRAN_CP_SECTIONEXTCMD_7 = 7,    /**< Multiple-eAxC designation */
    XRAN_CP_SECTIONEXTCMD_8 = 8,    /**< MMSE parameters */
    XRAN_CP_SECTIONEXTCMD_9 = 9,    /**< Dynamic Spectrum Sharing parameters */
    XRAN_CP_SECTIONEXTCMD_10 = 10,  /**< Multiple ports grouping */
    XRAN_CP_SECTIONEXTCMD_11 = 11,  /**< Flexible BF weights */
    XRAN_CP_SECTIONEXTCMD_MAX       /* 12~127 reserved for future use */
    };

/** Macro to convert bfwIqWidth defined in 5.4.7.1.1, Table 5-15 */
#define XRAN_CONVERT_BFWIQWIDTH(x)          ((x) > 15 ? 0 : (x))

/** Beamforming Weights Compression Method 5.4.7.1.1, Table 5-16 */
enum xran_cp_bfw_compression_method {
    XRAN_BFWCOMPMETHOD_NONE         = 0,    /**< Uncopressed I/Q value */
    XRAN_BFWCOMPMETHOD_BLKFLOAT     = 1,    /**< I/Q mantissa value */
    XRAN_BFWCOMPMETHOD_BLKSCALE     = 2,    /**< I/Q scaled value */
    XRAN_BFWCOMPMETHOD_ULAW         = 3,    /**< compressed I/Q value */
    XRAN_BFWCOMPMETHOD_BEAMSPACE    = 4,    /**< beamspace I/Q coefficient */
    XRAN_BFWCOMPMETHOD_MAX                  /* reserved for future methods */
    };

/** Beamforming Attributes Bitwidth 5.4.7.2.1 */
enum xran_cp_bfa_bitwidth {
    XRAN_BFABITWIDTH_NO             = 0,    /**< the filed is no applicable or the default value shall be used */
    XRAN_BFABITWIDTH_2BIT           = 1,    /**< the filed is 2-bit bitwidth */
    XRAN_BFABITWIDTH_3BIT           = 2,    /**< the filed is 3-bit bitwidth */
    XRAN_BFABITWIDTH_4BIT           = 3,    /**< the filed is 4-bit bitwidth */
    XRAN_BFABITWIDTH_5BIT           = 4,    /**< the filed is 5-bit bitwidth */
    XRAN_BFABITWIDTH_6BIT           = 5,    /**< the filed is 6-bit bitwidth */
    XRAN_BFABITWIDTH_7BIT           = 6,    /**< the filed is 7-bit bitwidth */
    XRAN_BFABITWIDTH_8BIT           = 7,    /**< the filed is 8-bit bitwidth */
    };

/** Layer ID for DL transmission in TM1-TM4 5.4.7.3.2 */
#define XRAN_LAYERID_0              0       /**< Layer 0 */
#define XRAN_LAYERID_1              1       /**< Layer 1 */
#define XRAN_LAYERID_2              2       /**< Layer 2 */
#define XRAN_LAYERID_3              3       /**< Layer 3 */
#define XRAN_LAYERID_TXD            0xf     /**< TxD */

/** LTE Transmission Scheme for section extension type 3 5.4.7.3.3 */
#define XRAN_TXS_SMUXCDD            0       /**< Spatial Multiplexing (CDD) */
#define XRAN_TXS_SMUXNOCDD          1       /**< Spatial Multiplexing (no CDD) */
#define XRAN_TXS_TXDIV              2       /**< Transmit diversity */

/** Resource Block Group Size 5.4.7.6.1 */
enum xran_cp_rbgsize {
    XRAN_RBGSIZE_1RB                = 1,    /**< 1 RB */
    XRAN_RBGSIZE_2RB                = 2,    /**< 2 RBs */
    XRAN_RBGSIZE_3RB                = 3,    /**< 3 RBs */
    XRAN_RBGSIZE_4RB                = 4,    /**< 4 RBs */
    XRAN_RBGSIZE_6RB                = 5,    /**< 6 RBs */
    XRAN_RBGSIZE_8RB                = 6,    /**< 8 RBs */
    XRAN_RBGSIZE_16RB               = 7,    /**< 16 RBs */
};

/** Technology for Dynamic Spectrum Sharing operation 5,4,7.9.1 */
#define XRAN_DSSTECH_LTE            0       /**< LTE support */
#define XRAN_DSSTECH_NR             1       /**< NR support */

/** The type of beam grouping 5.4.7.10.1 */
#define XRAN_BEAMGT_COMMON          0       /** common beam */
#define XRAN_BEAMGT_MATRIXIND       1       /** beam matrix indication */
#define XRAN_BEAMGT_VECTORLIST      2       /** beam vector listing */

#define XRAN_MAX_NUMPORTC_EXT10     64      /* defined in 5.4.7.10.2 */

/**
 * This structure contains the information to generate the section body of C-Plane message */
struct xran_section_info {
    /** for U-plane */
    struct xran_section_desc sec_desc[XRAN_NUM_OF_SYMBOL_PER_SLOT];
    int32_t     freqOffset; /*      X         24bits */
    uint32_t    startPrbc:9;  /*  X X X X X     9bits */
    uint32_t    numPrbc:9;    /*  X X X X X      8bits */ /* will be converted to zero if >255 */
    uint32_t    type:4;       /* type of this section  */
                            /* section type   bit-    */
                            /*  0 1 3 5 6 7    length */
    uint32_t     startSymId:4; /*  X X X X X X    4bits */
    uint32_t     numSymbol:4;  /*  X X X X        4bits */
    uint32_t    res:2;
    uint16_t    beamId;     /*    X X         15bits */
    uint16_t    ueId;       /*        X X     15bits */
    uint16_t    regFactor;  /*          X     16bits */
    uint16_t    id;         /*  X X X X X     12bits */
    uint16_t    reMask;     /*  X X X X       12bits */

    uint8_t     symInc:1;     /*  X X X X X      1bit  */
    uint8_t     rb:1;         /*  X X X X X      1bit  */
    uint8_t     ef:1;         /*    X X X X      1bit  */
    uint8_t     prbElemBegin:1;      /* Flag to indicate beginning of a PRB element */
    uint8_t     prbElemEnd:1;        /* Flag to indicate end of a PRB element */
    uint8_t     reserved:3;
    uint8_t     compMeth:4;   /*    X X X        4bits */
    uint8_t     iqWidth:4;    /*    X X X        4bits */
};


struct xran_sectionext1_info {
    uint16_t    rbNumber;                   /**< number RBs to ext1 chain */
    uint16_t    bfwNumber;                  /**< number of bf weights in this section */
    uint8_t     bfwIqWidth;
    uint8_t     bfwCompMeth;
    int8_t     *p_bfwIQ;                   /**< pointer to formed section extention */
    int16_t     bfwIQ_sz;                   /**< size of buffer with section extention information */
    union {
        uint8_t     exponent;
        uint8_t     blockScaler;
        uint8_t     compBitWidthShift;
        uint8_t     activeBeamspaceCoeffMask[XRAN_MAX_BFW_N];   /* ceil(N/8)*8, should be multiple of 8 */
        } bfwCompParam;
    };

struct xran_sectionext2_info {
    uint8_t     bfAzPtWidth;    /* beamforming zenith beamwidth parameter */
    uint8_t     bfAzPt;
    uint8_t     bfZePtWidth;    /* beamforming azimuth beamwidth parameter */
    uint8_t     bfZePt;
    uint8_t     bfAz3ddWidth;   /* beamforming zenith pointing parameter */
    uint8_t     bfAz3dd;
    uint8_t     bfZe3ddWidth;   /* beamforming azimuth pointing parameter */
    uint8_t     bfZe3dd;

    uint8_t     bfAzSI;
    uint8_t     bfZeSI;
    };

struct xran_sectionext3_info {
    uint8_t     codebookIdx;
    uint8_t     layerId;
    uint8_t     numLayers;
    uint8_t     txScheme;
    uint16_t    crsReMask;
    uint8_t     crsShift;
    uint8_t     crsSymNum;
    uint16_t    numAntPort;     /* number of antenna port - 2 or 4 */
    uint16_t    beamIdAP1;
    uint16_t    beamIdAP2;
    uint16_t    beamIdAP3;
    };

struct xran_sectionext4_info {
    uint8_t     csf;
    uint8_t     pad0;
    uint16_t    modCompScaler;
    };

struct xran_sectionext5_info {
    uint8_t     num_sets;
    struct {
        uint16_t    csf;
        uint16_t    mcScaleReMask;
        uint16_t    mcScaleOffset;
        } mc[XRAN_MAX_MODCOMP_ADDPARMS];
    };

struct xran_sectionext6_info {
    uint8_t     rbgSize;
    uint8_t     pad;
    uint16_t    symbolMask;
    uint32_t    rbgMask;
};

struct xran_sectionext7_info {
    uint16_t    eAxCmask;
};

struct xran_sectionext8_info {
    uint16_t    regularizationFactor;
};

struct xran_sectionext9_info {
    uint8_t     technology;
    uint8_t     reserved;
};

struct xran_sectionext10_info {
    uint8_t     numPortc;
    uint8_t     beamGrpType;
    uint16_t    beamID[XRAN_MAX_NUMPORTC_EXT10];
};

struct xran_sectionext11_info {
    uint8_t     RAD;
    uint8_t     disableBFWs;

    uint8_t     numBundPrb;
    uint8_t     numSetBFWs;     /* Total number of beam forming weights set (L) */

    uint8_t     bfwCompMeth;
    uint8_t     bfwIqWidth;

    int32_t         totalBfwIQLen;
    int32_t         maxExtBufSize;  /* Maximum space of external buffer */
    uint8_t     *pExtBuf;       /* pointer to start of external buffer */
    void        *pExtBufShinfo; /* Pointer to rte_mbuf_ext_shared_info */
};

union xran_ext_bfwcompparam_info {
    uint8_t     exponent;
//    uint8_t     blockScaler;                /*  Not supported */
//    uint8_t     compBitWidthShift;          /*  Not supported */
//    uint8_t     *pActBeamspaceCoeffMask;    /*  Not supported */
    };
struct xran_ext11_prbbundle_info {
    union xran_ext_bfwcompparam_info bfwCompParam;
    uint16_t    beamId;     /*  15bits, needs to strip MSB */
    uint16_t    BFWSize;    /*  actual size of bfws in bytes */
    uint8_t     *pBFWs;     /*  external buffer pointer */
};
struct xran_sectionext11_recv_info {
    uint8_t     RAD;
    uint8_t     disableBFWs;

    uint8_t     numBundPrb;
    uint8_t     numSetBFWs;     /* Total number of beam forming weights set (L) */

    uint8_t     bfwCompMeth;
    uint8_t     bfwIqWidth;

    int32_t         totalBfwIQLen;
    int32_t         maxExtBufSize;  /* Maximum space of external buffer */
    uint8_t     *pExtBuf;       /* pointer to start of external buffer */
    void        *pExtBufShinfo; /* Pointer to rte_mbuf_ext_shared_info */

    /* For parsing */
    struct xran_ext11_prbbundle_info bundInfo[XRAN_MAX_SET_BFWS];
};

struct xran_sectionext_info {
    uint16_t    type;
    uint16_t    len;
    void        *data;
    };

/**
 * This structure contains the information to generate the section header of C-Plane message */
struct xran_cp_header_params {
    // common parameters
    uint8_t     filterIdx;
    uint8_t     frameId;
    uint8_t     subframeId;
    uint8_t     slotId;
    uint8_t     startSymId;
                            /* section type   bit-    */
                            /*  0 1 3 5 6 7    length */
    uint8_t     fftSize;    /*  X   X          4bits */
    uint8_t     scs;        /*  X   X          4bits */
    uint8_t     iqWidth;    /*    X X X        4bits */
    uint8_t     compMeth;   /*    X X X        4bits */
    uint8_t     numUEs;     /*          X      8bits */
    uint16_t    timeOffset; /*  X   X         16bits */
    uint16_t    cpLength;   /*  X   X         16bits */
    };

/** The structure for the generation of section extension */
struct xran_section_ext_gen_info {
    uint16_t    type;           /**< the type of section extension */
    uint16_t    len;            /**< length of extension data */
    void        *data;          /**< pointer to extension data */
    };

/**
 * This structure to hold the information to generate the sections of C-Plane message */
struct xran_section_gen_info {
    struct xran_section_info *info;  /**< The information for section */

    /** the array to store section extension */
    struct xran_section_ext_gen_info exData[XRAN_MAX_NUM_EXTENSIONS];
    uint32_t    exDataSize;         /**< The number of Extensions or type 6/7 data */
};


/**
 * This structure to hold the information to generate a C-Plane message */
struct xran_cp_gen_params {
    uint8_t     dir;            /**< UL or DL */
    uint8_t     sectionType;    /**< each section must have same type with this */
    uint16_t    numSections;    /**< the number of sections to generate */

    struct xran_cp_header_params hdr;
    /**< The information for C-Plane message header */
    struct xran_section_gen_info *sections;
    /**< Array of the section information */
    };

/** The structure to store received section extension */
struct xran_section_ext_recv_info {
    uint16_t    type;           /**< the type of section extension */
    uint16_t    size;
    union {
        struct xran_sectionext1_info    ext1;
        struct xran_sectionext2_info    ext2;
        struct xran_sectionext3_info    ext3;
        struct xran_sectionext4_info    ext4;
        struct xran_sectionext5_info    ext5;
        struct xran_sectionext6_info    ext6;
        struct xran_sectionext9_info    ext9;
        struct xran_sectionext10_info   ext10;
        struct xran_sectionext11_recv_info  ext11;
    } u;
};

/**
 * This structure to hold the information of received sections of C-Plane message */
struct xran_section_recv_info {
    struct xran_section_info info;  /**< The information for received section */

    uint32_t numExts;
    /** the array to store section extension */
    struct xran_section_ext_recv_info exts[XRAN_MAX_NUM_EXTENSIONS];
};

/**
 * This structure to store received C-Plane message */
struct xran_cp_recv_params {
    uint8_t     dir;            /**< UL or DL */
    uint8_t     sectionType;    /**< each section must have same type with this */
    uint16_t    numSections;    /**< the number of sections received */
    uint8_t     numSetBFW;      /**<Set of BFWs */
    uint8_t     ext1count;      /**<Count set of extension type-1 BFWs*/
    uint32_t    tti;            /**<micro-second*/
    uint8_t     dssPeriod;      /**< DSS pattern period for LTE/NR */
    uint8_t     technology_arr[XRAN_MAX_DSS_PERIODICITY];   /**< technology array represents slot is LTE(0)/NR(1) */
    

    struct xran_cp_header_params hdr;
    /**< The information for C-Plane message header */
    struct xran_section_recv_info *sections;
    /**< Array of the section information */
};

/**
 * This structure to hold the information of RB allocation from PHY
 * to send data for allocated RBs only. */
struct xran_cp_rbmap_list {
    uint16_t    grp_id;     /**< group id for this entry, reserved for future use */

    uint8_t     sym_start;  /**< Start symbol ID */
    uint8_t     sym_num;    /**< Number of symbols */

    uint16_t    rb_start;   /**< Start RB position */
    uint16_t    rb_num;     /**< Number of RBs */

    int16_t     iq_buff_offset; /**< Offset within Sym for start of IQs */
    int16_t     iq_buff_len;    /**< length IQs */

    uint16_t    beam_id;    /**< Bean Index */
    uint8_t     iqWidth;    /**< I and Q width in bits */
    uint8_t     comp_meth;  /**< Compression method */
    uint8_t     pad0;
    };

typedef struct tagSECTION_DB_TYPE {
    struct xran_sectioninfo_db* p_sectiondb_elm[XRAN_MAX_SECTIONDB_CTX][XRAN_DIR_MAX][XRAN_COMPONENT_CARRIERS_MAX][XRAN_MAX_ANTENNA_NR * 2 + XRAN_MAX_ANT_ARRAY_ELM_NR];
} SECTION_DB_TYPE, * PSECTION_DB_TYPE;

uint16_t xran_get_cplength(int32_t cpLength);
int32_t xran_get_freqoffset(int32_t freqOffset, int32_t scs);

int32_t xran_prepare_ctrl_pkt(struct rte_mbuf *mbuf,
                        struct xran_cp_gen_params *params,
                        uint8_t CC_ID, uint8_t Ant_ID,
                        uint8_t seq_id,
                        uint16_t start_sect_id);

int32_t xran_parse_cp_pkt(struct rte_mbuf *mbuf,
                    struct xran_cp_recv_params *result,
                    struct xran_recv_packet_info *pkt_info, void* handle, uint32_t *mb_free);

int32_t xran_cp_init_sectiondb(void *pHandle);
int32_t xran_cp_free_sectiondb(void *pHandle);
int32_t xran_cp_add_section_info(void *pHandle,
        uint8_t dir, uint8_t cc_id, uint8_t ruport_id,
        uint8_t ctx_id, struct xran_section_info *info);

struct xran_section_info *
xran_cp_get_section_info_ptr(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ruport_id, uint8_t ctx_id);

int32_t xran_cp_add_multisection_info(void *pHandle,
        uint8_t cc_id, uint8_t ruport_id, uint8_t ctx_id,
        struct xran_cp_gen_params *gen_info);
struct xran_section_info *xran_cp_find_section_info(void *pHandle,
        uint8_t dir, uint8_t cc_id, uint8_t ruport_id,
        uint8_t ctx_id, uint16_t section_id);
struct xran_section_info *xran_cp_iterate_section_info(void *pHandle,
        uint8_t dir, uint8_t cc_id, uint8_t ruport_id,
        uint8_t ctx_id, uint32_t *next);

int32_t xran_cp_getsize_section_info(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ruport_id, uint8_t ctx_id);
int32_t xran_cp_reset_section_info(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ruport_id, uint8_t ctx_id);
int32_t xran_cp_populate_section_ext_1(int8_t  *p_ext1_dst,    /**< destination buffer */
                                       uint16_t  ext1_dst_len, /**< dest buffer size */
                                       int16_t  *p_bfw_iq_src, /**< source buffer of IQs */
                                       struct xran_prb_elm *p_pRbMapElm);
struct rte_mbuf *xran_attach_cp_ext_buf(uint16_t vf_id, int8_t* p_ext_buff_start, int8_t* p_ext_buff, uint16_t ext_buff_len,
                struct rte_mbuf_ext_shared_info * p_share_data);
int32_t xran_cp_attach_ext_buf(struct rte_mbuf *mbuf, uint8_t *extbuf_start, uint16_t extbuf_len,
                struct rte_mbuf_ext_shared_info *shinfo);
int32_t xran_cp_prepare_ext11_bfws(uint8_t numSetBFW, uint8_t numBFW,
                        uint8_t iqWidth, uint8_t compMeth,
                        uint8_t *dst, int16_t dst_maxlen,
                        struct xran_ext11_bfw_info bfwInfo[]);
int32_t xran_cp_estimate_max_set_bfws(uint8_t numBFWs, uint8_t iqWidth,
                        uint8_t compMeth, uint16_t mtu);

#ifdef __cplusplus
}
#endif

#endif /* _XRAN_CP_API_H_ */
