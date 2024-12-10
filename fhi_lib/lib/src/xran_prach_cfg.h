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
 * @brief Header file to PRACH specific config structures
 * @file xran_prach_cfg.h
 * @author Intel Corporation
 **/

#ifndef _XRAN_PRACH_CFG_H_
#define _XRAN_PRACH_CFG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* PRACH configuration table defines */
#define XRAN_PRACH_CANDIDATE_PREAMBLE    (2)
#define XRAN_PRACH_CANDIDATE_Y           (2)
#define XRAN_PRACH_CANDIDATE_SLOT        (40)
#define XRAN_PRACH_CONFIG_TABLE_SIZE     (256)
#define XRAN_PRACH_PREAMBLE_FORMAT_OF_ABC (9)
#define XRAN_LTE_PRACH_CONFIG_TABLE_SIZE    (64)
#define XRAN_LTE_MAX_PRACH_PREAMBLE_FORMAT  (4)

#if !(defined(SUBFRAMES_PER_SYSTEMFRAME))
#define  SUBFRAMES_PER_SYSTEMFRAME 10
#endif

typedef enum
{
    FORMAT_0 = 0,
    FORMAT_1,
    FORMAT_2,
    FORMAT_3,
    FORMAT_A1,
    FORMAT_A2,
    FORMAT_A3,
    FORMAT_B1,
    FORMAT_B2,
    FORMAT_B3,
    FORMAT_B4,
    FORMAT_C0,
    FORMAT_C2,
    FORMAT_4,
    FORMAT_LAST
}PreambleFormatEnum;

enum
{
    PRACH_ANY_FRAME     = 1,
    PRACH_EVEN_FRAME    = 2,
};

/* add PRACH used config table, same structure as used in refPHY */
typedef struct
{
    uint8_t     prachConfigIdx;
    uint8_t     preambleFmrt[XRAN_PRACH_CANDIDATE_PREAMBLE];
    uint8_t     x;
    uint8_t     y[XRAN_PRACH_CANDIDATE_Y];
    uint8_t     slotNr[XRAN_PRACH_CANDIDATE_SLOT];
    uint8_t     slotNrNum;
    uint8_t     startingSym;
    uint8_t     nrofPrachInSlot;
    uint8_t     occassionsInPrachSlot;
    uint8_t     duration;
} xRANPrachConfigTableStruct;

typedef struct
{
    uint8_t    preambleFmrt;
    uint16_t   lRALen;
    uint8_t    fRA;
    uint32_t    nu;
    uint16_t   nRaCp;
}xRANPrachPreambleLRAStruct;

struct xran_lte_prach_config_table
{
    int16_t     prachConfigIdx; /* 0 ~ 63, -1 means N/A */
    uint8_t     preambleFmrt;   /* FORMAT_X */
    uint8_t     frameNum;       /* PRACH_XXX_FRAME */

    uint8_t     sfnNum[SUBFRAMES_PER_SYSTEMFRAME]; /* 0 or 1 */
};

struct xran_lte_prach_preambleformat_table
{
    uint16_t    preambleFmrt;
    uint16_t    Tcp;
    uint32_t    Tseq;
    uint16_t    Nzc;
    uint16_t    fRA;
};

struct xran_prach_cp_config
{
    uint8_t    filterIdx;
    uint8_t    startSymId;
    uint16_t   startPrbc;
    uint8_t    numPrbc;
    uint8_t    numSymbol;
    uint16_t   timeOffset;
    int32_t    freqOffset;
    uint8_t    nrofPrachInSlot;
    uint8_t    occassionsInPrachSlot;
    uint8_t    x;
    uint8_t    y[XRAN_PRACH_CANDIDATE_Y];
    uint8_t    isPRACHslot[XRAN_PRACH_CANDIDATE_SLOT];
	uint8_t    duration;
    uint16_t   prachEaxcOffset;  /**< starting eAxC for PRACH stream */
    /*Parameters for NB-IOT*/
    uint8_t    nprachformat;
    uint16_t   periodicity;
    uint16_t   startTime;
    uint8_t    suboffset;
    uint8_t    numSubCarriers;
    uint8_t    nRep; /*Repetitions*/
    /* -- end of Parameters fo NB-IOT */
};

extern const xRANPrachConfigTableStruct gxranPrachDataTable_sub6_fdd[XRAN_PRACH_CONFIG_TABLE_SIZE];
extern const xRANPrachConfigTableStruct gxranPrachDataTable_sub6_tdd[XRAN_PRACH_CONFIG_TABLE_SIZE];
extern const xRANPrachConfigTableStruct gxranPrachDataTable_mmw[XRAN_PRACH_CONFIG_TABLE_SIZE];
extern const xRANPrachPreambleLRAStruct gxranPreambleforLRA[13];

extern const struct xran_lte_prach_config_table gxranPrachDataTable_lte_fs1[XRAN_LTE_PRACH_CONFIG_TABLE_SIZE];
extern const struct xran_lte_prach_preambleformat_table gxranLtePreambleFormat[5];

#ifdef __cplusplus
}
#endif

#endif
