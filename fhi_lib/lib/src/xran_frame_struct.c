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
 * @brief XRAN layer common functionality for both lls-CU and RU as well as C-plane and
 *    U-plane
 * @file xran_common.c
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#include <assert.h>
#include <err.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>

#include "xran_frame_struct.h"
#include "xran_printf.h"

enum nXranChBwOptions
{
    XRAN_BW_5_0_MHZ  = 5,  XRAN_BW_10_0_MHZ = 10, XRAN_BW_15_0_MHZ = 15, XRAN_BW_20_0_MHZ = 20, XRAN_BW_25_0_MHZ = 25,
    XRAN_BW_30_0_MHZ = 30, XRAN_BW_40_0_MHZ = 40, XRAN_BW_50_0_MHZ = 50, XRAN_BW_60_0_MHZ = 60, XRAN_BW_70_0_MHZ = 70,
    XRAN_BW_80_0_MHZ = 80, XRAN_BW_90_0_MHZ = 90, XRAN_BW_100_0_MHZ = 100, XRAN_BW_200_0_MHZ = 200, XRAN_BW_400_0_MHZ = 400
};

// F1 Tables 38.101-1 Table 5.3.2-1. Maximum transmission bandwidth configuration NRB
static uint16_t nNumRbsPerSymF1[3][13] =
{
    //  5MHz    10MHz   15MHz   20 MHz  25 MHz  30 MHz  40 MHz  50MHz   60 MHz  70 MHz  80 MHz   90 MHz  100 MHz
        {25,    52,     79,     106,    133,    160,    216,    270,    0,         0,      0,      0,      0},         // Numerology 0 (15KHz)
        {11,    24,     38,     51,     65,     78,     106,    133,    162,       0,    217,    245,    273},         // Numerology 1 (30KHz)
        {0,     11,     18,     24,     31,     38,     51,     65,     79,        0,    107,    121,    135}          // Numerology 2 (60KHz)
};

// F2 Tables 38.101-2 Table 5.3.2-1. Maximum transmission bandwidth configuration NRB
static uint16_t nNumRbsPerSymF2[2][4] =
{
    //  50Mhz  100MHz  200MHz   400MHz
        {66,    132,    264,     0},        // Numerology 2 (60KHz)
        {32,    66,     132,     264}       // Numerology 3 (120KHz)
};

// 38.211 - Table 4.2.1
static uint16_t nSubCarrierSpacing[5] =
{
    15,     // mu = 0
    30,     // mu = 1
    60,     // mu = 2
    120,    // mu = 3
    240     // mu = 4
};

// TTI interval in us (slot duration)
static uint16_t nTtiInterval[4] =
{
    1000,    // mu = 0
    500,     // mu = 1
    250,     // mu = 2
    125     // mu = 3
};

#if 0
// F1 Tables 38.101-1 Table F.5.3. Window length for normal CP
static uint16_t nCpSizeF1[3][13][2] =
{
    //    5MHz      10MHz      15MHz       20 MHz      25 MHz     30 MHz      40 MHz       50MHz       60 MHz      70 MHz     80 MHz     90 MHz     100 MHz
        {{40, 36}, {80, 72}, {120, 108}, {160, 144}, {160, 144}, {240, 216}, {320, 288}, {320, 288},     {0, 0},     {0, 0},     {0, 0},     {0, 0},     {0, 0}},        // Numerology 0 (15KHz)
        {{22, 18}, {44, 36},   {66, 54},   {88, 72},   {88, 72}, {132, 108}, {176, 144}, {176, 144}, {264, 216}, {264, 216}, {352, 288}, {352, 288}, {352, 288}},       // Numerology 1 (30KHz)
        {  {0, 0}, {26, 18},   {39, 27},   {52, 36},   {52, 36},   {78, 54},  {104, 72},  {104, 72}, {156, 108}, {156, 108}, {208, 144}, {208, 144}, {208, 144}},       // Numerology 2 (60KHz)
};

// F2 Tables 38.101-2 Table F.5.3. Window length for normal CP
static int16_t nCpSizeF2[2][4][2] =
{
    //    50Mhz    100MHz      200MHz     400MHz
        {  {0, 0}, {104, 72}, {208, 144}, {416, 288}}, // Numerology 2 (60KHz)
        {{68, 36}, {136, 72}, {272, 144}, {544, 288}}, // Numerology 3 (120KHz)
};
#endif


static uint32_t xran_fs_max_slot_num[XRAN_PORTS_NUM] = {8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000};
static uint32_t xran_fs_max_slot_num_SFN[XRAN_PORTS_NUM] = {20480,20480,20480,20480,20480,20480,20480,20480}; /* max slot number counted as SFN is 0-1023 */
static uint16_t xran_fs_num_slot_tdd_loop[XRAN_PORTS_NUM][XRAN_MAX_SECTOR_NR] = {{ XRAN_NUM_OF_SLOT_IN_TDD_LOOP }};
static uint16_t xran_fs_num_dl_sym_sp[XRAN_PORTS_NUM][XRAN_MAX_SECTOR_NR][XRAN_NUM_OF_SLOT_IN_TDD_LOOP] = {{{0}}};
static uint16_t xran_fs_num_ul_sym_sp[XRAN_PORTS_NUM][XRAN_MAX_SECTOR_NR][XRAN_NUM_OF_SLOT_IN_TDD_LOOP] = {{{0}}};
static uint8_t xran_fs_slot_type[XRAN_PORTS_NUM][XRAN_MAX_SECTOR_NR][XRAN_NUM_OF_SLOT_IN_TDD_LOOP] = {{{XRAN_SLOT_TYPE_INVALID}}};
static uint8_t xran_fs_slot_symb_type[XRAN_PORTS_NUM][XRAN_MAX_SECTOR_NR][XRAN_NUM_OF_SLOT_IN_TDD_LOOP][XRAN_NUM_OF_SYMBOL_PER_SLOT] = {{{{XRAN_SLOT_TYPE_INVALID}}}};
static float xran_fs_ul_rate[XRAN_PORTS_NUM][XRAN_MAX_SECTOR_NR] = {{0.0}};
static float xran_fs_dl_rate[XRAN_PORTS_NUM][XRAN_MAX_SECTOR_NR] = {{0.0}};

extern uint16_t xran_max_frame;

uint32_t xran_fs_get_tti_interval(uint8_t nMu)
{
    if (nMu < 4)
    {
        return nTtiInterval[nMu];
    }
    else
    {
        printf("ERROR: %s Mu[%d] is not valid, setting to 0\n",__FUNCTION__, nMu);
        return nTtiInterval[0];
    }
}

uint32_t xran_fs_get_scs(uint8_t nMu)
{
    if (nMu <= 3)
    {
        return nSubCarrierSpacing[nMu];
    }
    else
    {
        printf("ERROR: %s Mu[%d] is not valid\n",__FUNCTION__, nMu);
    }

    return 0;
}

//-------------------------------------------------------------------------------------------
/** @ingroup group_nr5g_source_phy_common
 *
 *  @param[in]   nNumerology - Numerology determine sub carrier spacing, Value: 0->4 0: 15khz,  1: 30khz,  2: 60khz 3: 120khz, 4: 240khz
 *  @param[in]   nBandwidth - Carrier bandwidth for in MHz. Value: 5->400
 *  @param[in]   nAbsFrePointA - Abs Freq Point A of the Carrier Center Frequency for in KHz Value: 450000->52600000
 *
 *  @return  Number of RBs in cell
 *
 *  @description
 *  Returns number of RBs based on 38.101-1 and 38.101-2 for the cell
 *
**/
//-------------------------------------------------------------------------------------------
uint16_t xran_fs_get_num_rbs(uint32_t nNumerology, uint32_t nBandwidth, uint32_t nAbsFrePointA)
{
    uint32_t error = 1;
    uint16_t numRBs = 0;

    if (nAbsFrePointA <= 6000000)
    {
        // F1 Tables 38.101-1 Table 5.3.2-1. Maximum transmission bandwidth configuration NRB
        if (nNumerology < 3)
        {
            switch(nBandwidth)
            {
                case XRAN_BW_5_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][0];
                    error = 0;
                break;
                case XRAN_BW_10_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][1];
                    error = 0;
                break;
                case XRAN_BW_15_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][2];
                    error = 0;
                break;
                case XRAN_BW_20_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][3];
                    error = 0;
                break;
                case XRAN_BW_25_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][4];
                    error = 0;
                break;
                case XRAN_BW_30_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][5];
                    error = 0;
                break;
                case XRAN_BW_40_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][6];
                    error = 0;
                break;
                case XRAN_BW_50_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][7];
                    error = 0;
                break;
                case XRAN_BW_60_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][8];
                    error = 0;
                break;
                case XRAN_BW_70_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][9];
                    error = 0;
                break;
                case XRAN_BW_80_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][10];
                    error = 0;
                break;
                case XRAN_BW_90_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][11];
                    error = 0;
                break;
                case XRAN_BW_100_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][12];
                    error = 0;
                break;
                default:
                    error = 1;
                break;
            }
        }
    }
    else
    {
        if ((nNumerology >= 2) && (nNumerology <= 3))
        {
            // F2 Tables 38.101-2 Table 5.3.2-1. Maximum transmission bandwidth configuration NRB
            switch(nBandwidth)
            {
                case XRAN_BW_50_0_MHZ:
                    numRBs = nNumRbsPerSymF2[nNumerology-2][0];
                    error = 0;
                break;
                case XRAN_BW_100_0_MHZ:
                    numRBs = nNumRbsPerSymF2[nNumerology-2][1];
                    error = 0;
                break;
                case XRAN_BW_200_0_MHZ:
                    numRBs = nNumRbsPerSymF2[nNumerology-2][2];
                    error = 0;
                break;
                case XRAN_BW_400_0_MHZ:
                    numRBs = nNumRbsPerSymF2[nNumerology-2][3];
                    error = 0;
                break;
                default:
                    error = 1;
                break;
            }
        }
    }


    if (error)
    {
        printf("ERROR: %s: nNumerology[%d] nBandwidth[%d] nAbsFrePointA[%d]\n",__FUNCTION__, nNumerology, nBandwidth, nAbsFrePointA);
    }
    else
    {
        printf("%s: nNumerology[%d] nBandwidth[%d] nAbsFrePointA[%d] numRBs[%d]\n",__FUNCTION__, nNumerology, nBandwidth, nAbsFrePointA, numRBs);
    }

    return numRBs;
}

//-------------------------------------------------------------------------------------------
/** @ingroup phy_cal_nrarfcn
 *
 *  @param[in]   center frequency
 *
 *  @return  NR-ARFCN
 *
 *  @description
 *  This calculates NR-ARFCN value according to center frequency
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t xran_fs_cal_nrarfcn(uint32_t nCenterFreq)
{
    uint32_t nDeltaFglobal,nFoffs,nNoffs;
    uint32_t nNRARFCN = 0;

    if(nCenterFreq > 0 && nCenterFreq < 3000*1000)
    {
        nDeltaFglobal = 5;
        nFoffs = 0;
        nNoffs = 0;
    }
    else if(nCenterFreq >= 3000*1000 && nCenterFreq < 24250*1000)
    {
        nDeltaFglobal = 15;
        nFoffs = 3000*1000;
        nNoffs = 600000;
    }
    else if(nCenterFreq >= 24250*1000 && nCenterFreq <= 100000*1000)
    {
        nDeltaFglobal = 60;
        nFoffs = 24250080;
        nNoffs = 2016667;
    }
    else
    {
         printf("@@@@ incorrect center frerquency %d\n",nCenterFreq);
         return (0);
    }

    nNRARFCN = ((nCenterFreq - nFoffs)/nDeltaFglobal) + nNoffs;

    printf("%s: nCenterFreq[%d] nDeltaFglobal[%d] nFoffs[%d] nNoffs[%d] nNRARFCN[%d]\n", __FUNCTION__, nCenterFreq, nDeltaFglobal, nFoffs, nNoffs, nNRARFCN);
    return (nNRARFCN);
}

uint32_t  xran_fs_slot_limit_init(uint32_t PortId, int32_t tti_interval_us)
{
    xran_fs_max_slot_num[PortId] = (1000/tti_interval_us)*1000;
    xran_fs_max_slot_num_SFN[PortId] = (1000/tti_interval_us)*(xran_max_frame+1)*10;
    return xran_fs_max_slot_num[PortId];
}

uint32_t xran_fs_get_max_slot(uint32_t PortId)
{
    return xran_fs_max_slot_num[PortId];
}

uint32_t xran_fs_get_max_slot_SFN(uint32_t PortId)
{
    return xran_fs_max_slot_num_SFN[PortId];
}

int32_t xran_fs_slot_limit(uint32_t PortId, int32_t nSfIdx)
{
    while (nSfIdx < 0) {
        nSfIdx += xran_fs_max_slot_num[PortId];
    }

    while (nSfIdx >= xran_fs_max_slot_num[PortId]) {
        nSfIdx -= xran_fs_max_slot_num[PortId];
    }

    return nSfIdx;
}

void xran_fs_clear_slot_type(uint32_t PortId, uint32_t nPhyInstanceId)
{
    xran_fs_ul_rate[PortId][nPhyInstanceId] = 0.0;
    xran_fs_dl_rate[PortId][nPhyInstanceId] = 0.0;
    xran_fs_num_slot_tdd_loop[PortId][nPhyInstanceId] = 1;
}

int32_t xran_fs_set_slot_type(uint32_t PortId, uint32_t nPhyInstanceId, uint32_t nFrameDuplexType, uint32_t nTddPeriod, struct xran_slot_config* psSlotConfig)
{
    uint32_t nSlotNum, nSymNum, nVal, i, j;
    uint32_t numDlSym, numUlSym, numGuardSym;
    uint32_t numDlSlots = 0, numUlSlots = 0, numSpDlSlots = 0, numSpUlSlots = 0, numSpSlots = 0;
#ifdef PRINTF_DBG_OK
    char sSlotPattern[XRAN_SLOT_TYPE_LAST][10] = {"IN\0", "DL\0", "UL\0", "SP\0", "FD\0"};
#endif

    // nPhyInstanceId    Carrier ID
    // nFrameDuplexType  0 = FDD 1 = TDD
    // nTddPeriod        Tdd Periodicity
    // psSlotConfig[80]  Slot Config Structure for nTddPeriod Slots

    xran_fs_ul_rate[PortId][nPhyInstanceId] = 0.0;
    xran_fs_dl_rate[PortId][nPhyInstanceId] = 0.0;
    xran_fs_num_slot_tdd_loop[PortId][nPhyInstanceId] = nTddPeriod;

    for (i = 0; i < XRAN_NUM_OF_SLOT_IN_TDD_LOOP; i++)
    {
        xran_fs_slot_type[PortId][nPhyInstanceId][i] = XRAN_SLOT_TYPE_INVALID;
        xran_fs_num_dl_sym_sp[PortId][nPhyInstanceId][i] = 0;
        xran_fs_num_ul_sym_sp[PortId][nPhyInstanceId][i] = 0;
    }

    if (nFrameDuplexType == XRAN_FDD)
    {
        for (i = 0; i < XRAN_NUM_OF_SLOT_IN_TDD_LOOP; i++)
        {
            xran_fs_slot_type[PortId][nPhyInstanceId][i] = XRAN_SLOT_TYPE_FDD;
            for(j = 0; j < XRAN_NUM_OF_SYMBOL_PER_SLOT; j++)
              xran_fs_slot_symb_type[PortId][nPhyInstanceId][i][j] = XRAN_SYMBOL_TYPE_FDD;
        }
        xran_fs_num_slot_tdd_loop[PortId][nPhyInstanceId] = 1;
        xran_fs_dl_rate[PortId][nPhyInstanceId] = 1.0;
        xran_fs_ul_rate[PortId][nPhyInstanceId] = 1.0;
    }
    else
    {
        for (nSlotNum = 0; nSlotNum < nTddPeriod; nSlotNum++)
        {
            numDlSym = 0;
            numUlSym = 0;
            numGuardSym = 0;
            for (nSymNum = 0; nSymNum < XRAN_NUM_OF_SYMBOL_PER_SLOT; nSymNum++)
            {
                switch(psSlotConfig[nSlotNum].nSymbolType[nSymNum])
                {
                    case XRAN_SYMBOL_TYPE_DL:
                        numDlSym++;
                        xran_fs_slot_symb_type[PortId][nPhyInstanceId][nSlotNum][nSymNum] = XRAN_SYMBOL_TYPE_DL;
                    break;
                    case XRAN_SYMBOL_TYPE_GUARD:
                        xran_fs_slot_symb_type[PortId][nPhyInstanceId][nSlotNum][nSymNum] = XRAN_SYMBOL_TYPE_GUARD;
                        numGuardSym++;
                    break;
                    default:
                        xran_fs_slot_symb_type[PortId][nPhyInstanceId][nSlotNum][nSymNum] = XRAN_SYMBOL_TYPE_UL;
                        numUlSym++;
                    break;
                }
            }

            print_dbg("nSlotNum[%d] : numDlSym[%d] numGuardSym[%d] numUlSym[%d] ", nSlotNum, numDlSym, numGuardSym, numUlSym);

            if ((numUlSym == 0) && (numGuardSym == 0))
            {
                xran_fs_slot_type[PortId][nPhyInstanceId][nSlotNum] = XRAN_SLOT_TYPE_DL;
                numDlSlots++;
                print_dbg("XRAN_SLOT_TYPE_DL\n");
            }
            else if ((numDlSym == 0) && (numGuardSym == 0))
            {
                xran_fs_slot_type[PortId][nPhyInstanceId][nSlotNum] = XRAN_SLOT_TYPE_UL;
                numUlSlots++;
                print_dbg("XRAN_SLOT_TYPE_UL\n");
            }
            else
            {
                xran_fs_slot_type[PortId][nPhyInstanceId][nSlotNum] = XRAN_SLOT_TYPE_SP;
                numSpSlots++;
                print_dbg("XRAN_SLOT_TYPE_SP\n");

                if (numDlSym)
                {
                    numSpDlSlots++;
                    xran_fs_num_dl_sym_sp[PortId][nPhyInstanceId][nSlotNum] = numDlSym;
                }
                if (numUlSym)
                {
                    numSpUlSlots++;
                    xran_fs_num_ul_sym_sp[PortId][nPhyInstanceId][nSlotNum] = numUlSym;
                }
            }
            print_dbg("            numDlSlots[%d] numUlSlots[%d] numSpSlots[%d] numSpDlSlots[%d] numSpUlSlots[%d]\n", numDlSlots, numUlSlots, numSpSlots, numSpDlSlots, numSpUlSlots);
        }

        xran_fs_dl_rate[PortId][nPhyInstanceId] = (float)(numDlSlots + numSpDlSlots) / (float)nTddPeriod;
        xran_fs_ul_rate[PortId][nPhyInstanceId] = (float)(numUlSlots + numSpUlSlots) / (float)nTddPeriod;
    }

    print_dbg("%s: nPhyInstanceId[%d] nFrameDuplexType[%d], nTddPeriod[%d]\n",
        __FUNCTION__, nPhyInstanceId, nFrameDuplexType, nTddPeriod);

    print_dbg("DLRate[%f] ULRate[%f]\n", xran_fs_dl_rate[PortId][nPhyInstanceId], xran_fs_ul_rate[PortId][nPhyInstanceId]);

    nVal = (xran_fs_num_slot_tdd_loop[PortId][nPhyInstanceId] < 10) ? xran_fs_num_slot_tdd_loop[PortId][nPhyInstanceId] : 10;

    print_dbg("SlotPattern:\n");
    print_dbg("Slot:   ");
    for (nSlotNum = 0; nSlotNum < nVal; nSlotNum++)
    {
        print_dbg("%d    ", nSlotNum);
    }
    print_dbg("\n");

    print_dbg("  %3d   ", 0);
    for (nSlotNum = 0, i = 0; nSlotNum < xran_fs_num_slot_tdd_loop[PortId][nPhyInstanceId]; nSlotNum++)
    {
        print_dbg("%s   ", sSlotPattern[xran_fs_slot_type[PortId][nPhyInstanceId][nSlotNum]]);
        i++;
        if ((i == 10) && ((nSlotNum+1) < xran_fs_num_slot_tdd_loop[PortId][nPhyInstanceId]))
        {
            print_dbg("\n");
            print_dbg("  %3d   ", nSlotNum);
            i = 0;
        }
    }
    print_dbg("\n\n");

    return 0;
}

int32_t xran_fs_get_slot_type(uint32_t PortId, int32_t nCellIdx, int32_t nSlotdx, int32_t nType)
{
    int32_t nSfIdxMod, nSfType, ret = 0;

    nSfIdxMod = xran_fs_slot_limit(PortId, nSlotdx) % ((xran_fs_num_slot_tdd_loop[PortId][nCellIdx] > 0) ? xran_fs_num_slot_tdd_loop[PortId][nCellIdx]: 1);
    nSfType = xran_fs_slot_type[PortId][nCellIdx][nSfIdxMod];

    if (nSfType == nType)
    {
        ret = 1;
    }
    else if (nSfType == XRAN_SLOT_TYPE_SP)
    {
        if ((nType == XRAN_SLOT_TYPE_DL) && xran_fs_num_dl_sym_sp[PortId][nCellIdx][nSfIdxMod])
        {
            ret = 1;
        }

        if ((nType == XRAN_SLOT_TYPE_UL) && xran_fs_num_ul_sym_sp[PortId][nCellIdx][nSfIdxMod])
        {
            ret = 1;
        }
    }
    else if (nSfType == XRAN_SLOT_TYPE_FDD)
    {
        ret = 1;
    }

    return ret;
}

int32_t xran_fs_get_symbol_type(uint32_t PortId, int32_t nCellIdx, int32_t nSlotdx,  int32_t nSymbIdx)
{
    int32_t nSfIdxMod;

    nSfIdxMod = xran_fs_slot_limit(PortId, nSlotdx) % ((xran_fs_num_slot_tdd_loop[PortId][nCellIdx] > 0) ? xran_fs_num_slot_tdd_loop[PortId][nCellIdx]: 1);

    return xran_fs_slot_symb_type[PortId][nCellIdx][nSfIdxMod][nSymbIdx];
}


