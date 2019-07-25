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
 * @brief This file provides the definitions for Control Plane Messages APIs.
 *
 * @file xran_cp_api.h
 * @ingroup group_lte_source_xran
 * @author Intel Corporation
 *
 **/

#ifndef _XRAN_CP_API_H_
#define _XRAN_CP_API_H_

#include "xran_fh_lls_cu.h"
#include "xran_pkt_cp.h"

/* Error Codes
 *  For errors and exceptions, all values will be negative */
enum xran_errcodes {
    XRAN_ERRCODE_OK             = 0,
    XRAN_ERRCODE_INVALIDPARAM,
    XRAN_ERRCODE_OUTOFMEMORY,
    XRAN_ERRCODE_FAILTOSEND,
    XRAN_ERRCODE_INVALIDPACKET,
    XRAN_ERRCODE_MAX
    };

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

/**
 * This structure contains the information to generate the section body of C-Plane message */
struct xran_section_info {
                            /* section type   bit-    */
                            /*  0 1 3 5 6 7    length */
    uint16_t    id;         /*  X X X X X     12bits */
    uint8_t     rb;         /*  X X X X X      1bit  */
    uint8_t     symInc;     /*  X X X X X      1bit  */
    uint16_t    startPrbc;  /*  X X X X X     10bits */
    uint8_t     numPrbc;    /*  X X X X X      8bits */
    uint8_t     numSymbol;  /*  X X X X        4bits */
    uint16_t    reMask;     /*  X X X X       12bits */
    uint16_t    beamId;     /*    X X         15bits */
    uint16_t    ueId;       /*        X X     15bits */
    uint16_t    regFactor;  /*          X     16bits */
    int32_t     freqOffset; /*      X         24bits */
    uint8_t     ef;         /*    X X X X      1bit  */

    uint8_t     type;       /* type of this section  */
    uint16_t    pad0;
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

/**
 * This structure to hold the information to generate the sections of C-Plane message */
struct xran_section_gen_info {
    struct xran_section_info info; /**< The information for section */

    uint32_t    exDataSize;
    /**< Extension or type 6/7 data size, not supported */
    void        *exData;
    /*(< The pointer to the extension or type 6/7 data, not supported */
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


uint16_t xran_get_cplength(int cpLength, int uval);
int32_t xran_get_freqoffset(int freqOffset, int scs);

int xran_prepare_ctrl_pkt(struct rte_mbuf *mbuf,
                        struct xran_cp_gen_params *params,
                        uint8_t CC_ID, uint8_t Ant_ID,
                        uint8_t seq_id);

int xran_cp_init_sectiondb(void *pHandle);
int xran_cp_free_sectiondb(void *pHandle);
int xran_cp_add_section_info(void *pHandle,
        uint8_t dir, uint8_t cc_id, uint8_t ruport_id,
        uint8_t subframe_id, uint8_t slot_id,
        struct xran_section_info *info);
struct xran_section_info *xran_cp_find_section_info(void *pHandle,
        uint8_t dir, uint8_t cc_id, uint8_t ruport_id,
        uint8_t subframe_id, uint8_t slot_id,
        uint16_t section_id);
struct xran_section_info *xran_cp_iterate_section_info(void *pHandle,
        uint8_t dir, uint8_t cc_id, uint8_t ruport_id,
        uint8_t subframe_id, uint8_t slot_id, uint32_t *next);
int xran_cp_getsize_section_info(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ruport_id);
int xran_cp_reset_section_info(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ruport_id);

#endif /* _XRAN_CP_API_H_ */
