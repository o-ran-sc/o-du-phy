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

#include <assert.h>
#include <err.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <immintrin.h>
#include "common.h"
#include "xran_fh_o_du.h"
#include "xran_pkt.h"
#include "xran_pkt_up.h"
#include "xran_cp_api.h"
#include "xran_up_api.h"

#include "xran_mlog_lnx.h"

extern enum app_state state;
struct o_xu_buffers* p_o_xu_buff[XRAN_PORTS_NUM] = {NULL, NULL, NULL, NULL};

// F1 Tables 38.101-1 Table 5.3.2-1. Maximum transmission bandwidth configuration NRB
uint16_t nLteNumRbsPerSymF1[1][4] =
{
    //  5MHz    10MHz   15MHz   20 MHz
        {25,    50,     75,     100},         // Numerology 0 (15KHz)
};

// F1 Tables 38.101-1 Table 5.3.2-1. Maximum transmission bandwidth configuration NRB
uint16_t nNumRbsPerSymF1[3][13] =
{
    //  5MHz    10MHz   15MHz   20 MHz  25 MHz  30 MHz  40 MHz  50MHz   60 MHz  70 MHz  80 MHz   90 MHz  100 MHz
        {25,    52,     79,     106,    133,    160,    216,    270,    0,         0,      0,      0,      0},         // Numerology 0 (15KHz)
        {11,    24,     38,     51,     65,     78,     106,    133,    162,       0,    217,    245,    273},         // Numerology 1 (30KHz)
        {0,     11,     18,     24,     31,     38,     51,     65,     79,        0,    107,    121,    135}          // Numerology 2 (60KHz)
};

// F2 Tables 38.101-2 Table 5.3.2-1. Maximum transmission bandwidth configuration NRB
uint16_t nNumRbsPerSymF2[2][4] =
{
    //  50Mhz  100MHz  200MHz   400MHz
        {66,    132,    264,     0},        // Numerology 2 (60KHz)
        {32,    66,     132,     264}       // Numerology 3 (120KHz)
};

// 38.211 - Table 4.2.1
uint16_t nSubCarrierSpacing[5] =
{
    15,     // mu = 0
    30,     // mu = 1
    60,     // mu = 2
    120,    // mu = 3
    240     // mu = 4
};

// TTI interval in us (slot duration)
uint16_t nTtiInterval[4] =
{
    1000,     // mu = 0
    500,     // mu = 1
    250,     // mu = 2
    125,     // mu = 3
};


// F1 Tables 38.101-1 Table F.5.3. Window length for normal CP
uint16_t nCpSizeF1[3][13][2] =
{
    //    5MHz      10MHz      15MHz       20 MHz      25 MHz     30 MHz      40 MHz       50MHz       60 MHz      70 MHz     80 MHz     90 MHz     100 MHz
        {{40, 36}, {80, 72}, {120, 108}, {160, 144}, {160, 144}, {240, 216}, {320, 288}, {320, 288},     {0, 0},     {0, 0},     {0, 0},     {0, 0},     {0, 0}},        // Numerology 0 (15KHz)
        {{22, 18}, {44, 36},   {66, 54},   {88, 72},   {88, 72}, {132, 108}, {176, 144}, {176, 144}, {264, 216}, {264, 216}, {352, 288}, {352, 288}, {352, 288}},       // Numerology 1 (30KHz)
        {  {0, 0}, {26, 18},   {39, 27},   {52, 36},   {52, 36},   {78, 54},  {104, 72},  {104, 72}, {156, 108}, {156, 108}, {208, 144}, {208, 144}, {208, 144}},       // Numerology 2 (60KHz)
};

// F2 Tables 38.101-2 Table F.5.3. Window length for normal CP
int16_t nCpSizeF2[2][4][2] =
{
    //    50Mhz    100MHz      200MHz     400MHz
        {  {0, 0}, {104, 72}, {208, 144}, {416, 288}}, // Numerology 2 (60KHz)
        {{68, 36}, {136, 72}, {272, 144}, {544, 288}}, // Numerology 3 (120KHz)
};

uint32_t gLocMaxSlotNum;

static uint16_t g_NumSlotTDDLoop[XRAN_MAX_SECTOR_NR] = { XRAN_NUM_OF_SLOT_IN_TDD_LOOP };
static uint16_t g_NumDLSymSp[XRAN_MAX_SECTOR_NR][XRAN_NUM_OF_SLOT_IN_TDD_LOOP] = {0};
static uint16_t g_NumULSymSp[XRAN_MAX_SECTOR_NR][XRAN_NUM_OF_SLOT_IN_TDD_LOOP] = {0};
static uint8_t g_SlotType[XRAN_MAX_SECTOR_NR][XRAN_NUM_OF_SLOT_IN_TDD_LOOP] = {{XRAN_SLOT_TYPE_INVALID}};
float g_UlRate[XRAN_MAX_SECTOR_NR] = {0.0};
float g_DlRate[XRAN_MAX_SECTOR_NR] = {0.0};

uint32_t app_xran_get_tti_interval(uint8_t nMu)
{
    if (nMu < 4)
    {
        return nTtiInterval[nMu];
    }
    else
    {
        printf("ERROR: %s Mu[%d] is not valid\n",__FUNCTION__, nMu);
    }

    return 0;
}

uint32_t app_xran_get_scs(uint8_t nMu)
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
uint16_t app_xran_get_num_rbs(uint8_t ranTech, uint32_t nNumerology, uint32_t nBandwidth, uint32_t nAbsFrePointA)
{
    uint32_t error = 1;
    uint16_t numRBs = 0;

    if (ranTech == XRAN_RAN_LTE) {
        switch(nBandwidth)
        {
            case PHY_BW_5_0_MHZ:
                numRBs = nLteNumRbsPerSymF1[nNumerology][0];
                error = 0;
            break;
            case PHY_BW_10_0_MHZ:
                numRBs = nLteNumRbsPerSymF1[nNumerology][1];
                error = 0;
            break;
            case PHY_BW_15_0_MHZ:
                numRBs = nLteNumRbsPerSymF1[nNumerology][2];
                error = 0;
            break;
            case PHY_BW_20_0_MHZ:
                numRBs = nLteNumRbsPerSymF1[nNumerology][3];
                error = 0;
            break;
            default:
                error = 1;
            break;
        }
    } else if (nAbsFrePointA <= 6000000) {
        // F1 Tables 38.101-1 Table 5.3.2-1. Maximum transmission bandwidth configuration NRB
        if (nNumerology < 3)
        {
            switch(nBandwidth)
            {
                case PHY_BW_5_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][0];
                    error = 0;
                break;
                case PHY_BW_10_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][1];
                    error = 0;
                break;
                case PHY_BW_15_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][2];
                    error = 0;
                break;
                case PHY_BW_20_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][3];
                    error = 0;
                break;
                case PHY_BW_25_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][4];
                    error = 0;
                break;
                case PHY_BW_30_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][5];
                    error = 0;
                break;
                case PHY_BW_40_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][6];
                    error = 0;
                break;
                case PHY_BW_50_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][7];
                    error = 0;
                break;
                case PHY_BW_60_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][8];
                    error = 0;
                break;
                case PHY_BW_70_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][9];
                    error = 0;
                break;
                case PHY_BW_80_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][10];
                    error = 0;
                break;
                case PHY_BW_90_0_MHZ:
                    numRBs = nNumRbsPerSymF1[nNumerology][11];
                    error = 0;
                break;
                case PHY_BW_100_0_MHZ:
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
                case PHY_BW_50_0_MHZ:
                    numRBs = nNumRbsPerSymF2[nNumerology-2][0];
                    error = 0;
                break;
                case PHY_BW_100_0_MHZ:
                    numRBs = nNumRbsPerSymF2[nNumerology-2][1];
                    error = 0;
                break;
                case PHY_BW_200_0_MHZ:
                    numRBs = nNumRbsPerSymF2[nNumerology-2][2];
                    error = 0;
                break;
                case PHY_BW_400_0_MHZ:
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
        printf("ERROR: %s: RAN[%s] nNumerology[%d] nBandwidth[%d] nAbsFrePointA[%d]\n",__FUNCTION__, (ranTech ? "LTE" : "5G NR"), nNumerology, nBandwidth, nAbsFrePointA);
    }
    else
    {
        printf("%s: RAN [%s] nNumerology[%d] nBandwidth[%d] nAbsFrePointA[%d] numRBs[%d]\n",__FUNCTION__, (ranTech ? "LTE" : "5G NR"), nNumerology, nBandwidth, nAbsFrePointA, numRBs);
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
uint32_t app_xran_cal_nrarfcn(uint32_t nCenterFreq)
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

int32_t app_xran_slot_limit(int32_t nSfIdx)
{
    while (nSfIdx < 0) {
        nSfIdx += gLocMaxSlotNum;
    }

    while (nSfIdx >= gLocMaxSlotNum) {
        nSfIdx -= gLocMaxSlotNum;
    }

    return nSfIdx;
}

void app_xran_clear_slot_type(uint32_t nPhyInstanceId)
{
    g_UlRate[nPhyInstanceId] = 0.0;
    g_DlRate[nPhyInstanceId] = 0.0;
    g_NumSlotTDDLoop[nPhyInstanceId] = 1;
}

int32_t app_xran_set_slot_type(uint32_t nPhyInstanceId, uint32_t nFrameDuplexType, uint32_t nTddPeriod, struct xran_slot_config *psSlotConfig)
{
    uint32_t nSlotNum, nSymNum, nVal, i;
    uint32_t numDlSym, numUlSym, numGuardSym;
    uint32_t numDlSlots = 0, numUlSlots = 0, numSpDlSlots = 0, numSpUlSlots = 0, numSpSlots = 0;
    char sSlotPattern[XRAN_SLOT_TYPE_LAST][10] = {"IN\0", "DL\0", "UL\0", "SP\0", "FD\0"};

    // nPhyInstanceId    Carrier ID
    // nFrameDuplexType  0 = FDD 1 = TDD
    // nTddPeriod        Tdd Periodicity
    // psSlotConfig[80]  Slot Config Structure for nTddPeriod Slots

    g_UlRate[nPhyInstanceId] = 0.0;
    g_DlRate[nPhyInstanceId] = 0.0;
    g_NumSlotTDDLoop[nPhyInstanceId] = nTddPeriod;

    for (i = 0; i < XRAN_NUM_OF_SLOT_IN_TDD_LOOP; i++)
    {
        g_SlotType[nPhyInstanceId][i] = XRAN_SLOT_TYPE_INVALID;
        g_NumDLSymSp[nPhyInstanceId][i] = 0;
        g_NumULSymSp[nPhyInstanceId][i] = 0;
    }

    if (nFrameDuplexType == XRAN_FDD)
    {
        for (i = 0; i < XRAN_NUM_OF_SLOT_IN_TDD_LOOP; i++)
        {
            g_SlotType[nPhyInstanceId][i] = XRAN_SLOT_TYPE_FDD;
        }
        g_NumSlotTDDLoop[nPhyInstanceId] = 1;
        g_DlRate[nPhyInstanceId] = 1.0;
        g_UlRate[nPhyInstanceId] = 1.0;
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
                    break;
                    case XRAN_SYMBOL_TYPE_GUARD:
                        numGuardSym++;
                    break;
                    default:
                        numUlSym++;
                    break;
                }
            }

            // printf("nSlotNum[%d] : numDlSym[%d] numGuardSym[%d] numUlSym[%d]\n", nSlotNum, numDlSym, numGuardSym, numUlSym);

            if ((numUlSym == 0) && (numGuardSym == 0))
            {
                g_SlotType[nPhyInstanceId][nSlotNum] = XRAN_SLOT_TYPE_DL;
                numDlSlots++;
            }
            else if ((numDlSym == 0) && (numGuardSym == 0))
            {
                g_SlotType[nPhyInstanceId][nSlotNum] = XRAN_SLOT_TYPE_UL;
                numUlSlots++;
            }
            else
            {
                g_SlotType[nPhyInstanceId][nSlotNum] = XRAN_SLOT_TYPE_SP;
                numSpSlots++;

                if (numDlSym)
                {
                    numSpDlSlots++;
                    g_NumDLSymSp[nPhyInstanceId][nSlotNum] = numDlSym;
                }
                if (numUlSym)
                {
                    numSpUlSlots++;
                    g_NumULSymSp[nPhyInstanceId][nSlotNum] = numUlSym;
                }
            }

            // printf("            numDlSlots[%d] numUlSlots[%d] numSpSlots[%d] numSpDlSlots[%d] numSpUlSlots[%d]\n", numDlSlots, numUlSlots, numSpSlots, numSpDlSlots, numSpUlSlots);
        }

        g_DlRate[nPhyInstanceId] = (float)(numDlSlots + numSpDlSlots) / (float)nTddPeriod;
        g_UlRate[nPhyInstanceId] = (float)(numUlSlots + numSpUlSlots) / (float)nTddPeriod;
    }

    printf("set_slot_type: nPhyInstanceId[%d] nFrameDuplexType[%d], nTddPeriod[%d]\n",
        nPhyInstanceId, nFrameDuplexType, nTddPeriod);

    printf("DLRate[%f] ULRate[%f]\n", g_DlRate[nPhyInstanceId], g_UlRate[nPhyInstanceId]);

    nVal = (g_NumSlotTDDLoop[nPhyInstanceId] < 10) ? g_NumSlotTDDLoop[nPhyInstanceId] : 10;

    printf("SlotPattern:\n");
    printf("Slot:   ");
    for (nSlotNum = 0; nSlotNum < nVal; nSlotNum++)
    {
        printf("%d    ", nSlotNum);
    }
    printf("\n");

    printf("  %3d   ", 0);
    for (nSlotNum = 0, i = 0; nSlotNum < g_NumSlotTDDLoop[nPhyInstanceId]; nSlotNum++)
    {
        printf("%s   ", sSlotPattern[g_SlotType[nPhyInstanceId][nSlotNum]]);
        i++;
        if ((i == 10) && ((nSlotNum+1) < g_NumSlotTDDLoop[nPhyInstanceId]))
        {
            printf("\n");
            printf("  %3d   ", nSlotNum);
            i = 0;
        }
    }
    printf("\n\n");

    return 0;
}

int32_t app_xran_get_slot_type(int32_t nCellIdx, int32_t nSlotdx, int32_t nType)
{
    int32_t nSfIdxMod, nSfType, ret = 0;

    nSfIdxMod = app_xran_slot_limit(nSlotdx) % ((g_NumSlotTDDLoop[nCellIdx] > 0) ? g_NumSlotTDDLoop[nCellIdx]: 1);
    nSfType = g_SlotType[nCellIdx][nSfIdxMod];

    if (nSfType == nType)
    {
        ret = 1;
    }
    else if (nSfType == XRAN_SLOT_TYPE_SP)
    {
        if ((nType == XRAN_SLOT_TYPE_DL) && g_NumDLSymSp[nCellIdx][nSfIdxMod])
        {
            ret = 1;
        }

        if ((nType == XRAN_SLOT_TYPE_UL) && g_NumULSymSp[nCellIdx][nSfIdxMod])
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



void sys_save_buf_to_file(char *filename, char *bufname, unsigned char *pBuffer, unsigned int size, unsigned int buffers_num)
{
    if (size)
    {
        if (filename && bufname)
        {
            FILE           *file;
            printf("Storing %s to file %s: ", bufname, filename);
            file = fopen(filename, "wb");
            if (file == NULL)
            {
                printf("can't open file %s!!!", filename);
            }
            else
            {
                uint32_t             num;
                num = fwrite(pBuffer, buffers_num, size, file);
                fflush(file);
                fclose(file);
                printf("from addr (0x%lx) size (%d) bytes num (%d)", (uint64_t)pBuffer, size, num);
            }
            printf(" \n");
        }
        else
        {
            printf(" the file name, buffer name are not set!!!");
        }
    }
    else
    {
        printf(" the %s is free: size = %d bytes!!!", bufname, size);
    }
}

int sys_load_file_to_buff(char *filename, char *bufname, unsigned char *pBuffer, unsigned int size, unsigned int buffers_num)
{
    unsigned int  file_size = 0;
    int  num= 0;

    if (size)
    {
        if (filename && bufname)
        {
            FILE           *file;
            printf("Loading file %s to  %s: ", filename, bufname);
            file = fopen(filename, "rb");


            if (file == NULL)
            {
                printf("can't open file %s!!!", filename);
                exit(-1);
            }
            else
            {
                fseek(file, 0, SEEK_END);
                file_size = ftell(file);
                fseek(file, 0, SEEK_SET);

                if ((file_size > size) || (file_size == 0))
                    file_size = size;

                printf("Reading IQ samples from file: File Size: %d [Buffer Size: %d]\n", file_size, size);

                num = fread(pBuffer, buffers_num, size, file);
                fflush(file);
                fclose(file);
                printf("from addr (0x%lx) size (%d) bytes num (%d)", (uint64_t)pBuffer, file_size, num);
            }
            printf(" \n");

        }
        else
        {
            printf(" the file name, buffer name are not set!!!");
        }
    }
    else
    {
        printf(" the %s is free: size = %d bytes!!!", bufname, size);
    }
    return num;
}


void sys_save_buf_to_file_txt(char *filename, char *bufname, unsigned char *pBuffer, unsigned int size, unsigned int buffers_num)
{
    unsigned int i;
    int ret = 0;
    if (pBuffer == NULL)
        return;

    if (size)
    {
        if (filename && bufname)
        {
            FILE           *file;
            printf("Storing %s to file %s: ", bufname, filename);
            file = fopen(filename, "w");
            if (file == NULL)
            {
                printf("can't open file %s!!!", filename);
                exit(-1);
            }
            else
            {
                uint32_t num = 0;

                signed short *ptr = (signed short*)pBuffer;
                for (i = 0; i < (size/((unsigned int)sizeof(signed short) /** 2 * 2 * 2*/)); i = i + 2)
                {
#ifndef CSCOPE_DEBUG
                    ret = fprintf(file,"%d %d\n", ptr[i], ptr[i + 1]);
#else
                    ret = fprintf(file,"%d %d ", ptr[i], ptr[i + 1]);
                    /*      I data => Ramp data, from 1 to 792.
                            Q data => Contains time information of the current symbol:
                            Bits [15:14] = Antenna-ID
                            Bits [13:12] = “00”
                            Bits [11:8]  = Subframe-ID
                            Bits [7:4]   = Slot-ID
                            Bits [3:0]   = Symbol-ID */
                            fprintf(file, "0x%04x: ant %d Subframe-ID %d Slot-ID %d Symbol-ID %d\n",
                                        ptr[i + 1], (ptr[i + 1]>>14) & 0x3,  (ptr[i + 1]>>8) & 0xF,  (ptr[i + 1]>>4) & 0xF, (ptr[i + 1]>>0) & 0xF);
#endif
                    if (ret < 0)
                    {
                        printf("fprintf %d\n", ret);
                        fclose(file);
                        break;
                    }
                    num++;
                }
                fflush(file);
                fclose(file);
                printf("from addr (0x%lx) size (%d) IQ num (%d)", (uint64_t)pBuffer, size, num);
            }
            printf(" \n");
        }
        else
        {
            printf(" the file name, buffer name are not set!!!");
        }
    }
    else
    {
        printf(" the %s is free: size = %d bytes!!!", bufname, size);
    }
}

