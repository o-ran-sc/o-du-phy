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
#include <stdio.h>
#include <immintrin.h>
#include "xran_mod_compression.h"

#ifdef C_Module_Used
void
mod_compression_qpsk_c(int16_t *pData,int8_t *pOut,int16_t unit, int32_t nSc)
{
    for (int32_t iSc = 0 ; iSc<nSc ; iSc ++)
    {
        uint8_t bit_pos= iSc &0x3;
        int8_t bit_i = pData[iSc*2] >=0 ? 0 :1;
        int8_t bit_q = pData[iSc*2+1] >=0 ? 0 :1;
        *pOut |= bit_i<<(7-(bit_pos*2))|bit_q<<(6-(bit_pos*2));
        if (3 == bit_pos)
            pOut++;
    }
}

void
mod_compression_16qam_c(int16_t *pData,int8_t *pOut,int16_t unit, int32_t nSc)
{
    int16_t bit_unit = unit>>1;
    for (int32_t iSc = 0 ; iSc<nSc ; iSc ++)
    {
        uint8_t bit_pos= iSc &0x1;
        int8_t bit_i = pData[iSc*2]/bit_unit;
        int8_t bit_q = pData[iSc*2+1]/bit_unit;
        if(pData[iSc*2]<0)
        {
            bit_i=3+bit_i;
        }
        if(pData[iSc*2+1]<0)
        {
            bit_q=3+bit_q;
        }

        *pOut |= bit_i<<(6-(bit_pos*4))|bit_q<<(4-(bit_pos*4));
        if (1 == bit_pos)
            pOut++;
    }
}

void
mod_compression_64qam_c(int16_t *pData,int8_t *pOut,int16_t unit, int32_t nSc)
{
    int16_t bit_unit = unit>>2;
    for (int32_t iSc = 0 ; iSc<nSc ; iSc ++)
    {
        int32_t bit_pos = iSc &0x3;
        int8_t bit_i = pData[iSc*2]/bit_unit;
        int8_t bit_q = pData[iSc*2+1]/bit_unit;
        if(pData[iSc*2]<0)
        {
            bit_i=7+bit_i;
        }
        if(pData[iSc*2+1]<0)
        {
            bit_q=7+bit_q;
        }
        if (0 == bit_pos)
        {
            *pOut |= bit_i<<5|bit_q<<2;
        }
        else if (1 == bit_pos)
        {
            *pOut |= bit_i>>1;
            pOut++;
            *pOut |= bit_i<<7|bit_q<<4;
        }
        else if (2 == bit_pos)
        {
            *pOut |= bit_i<<1|bit_q>>2;
            pOut++;
            *pOut |= bit_q<<6;
        }
        else if (3 == bit_pos)
        {
            *pOut |= bit_i<<3|bit_q;
            pOut++;
        }
    }
}

void
mod_compression_256qam_c(int16_t *pData,int8_t *pOut,int16_t unit,int32_t nSc)
{
    int16_t bit_unit = unit>>3;
    for (int32_t iSc = 0 ; iSc<nSc ; iSc ++)
    {
        int8_t bit_i = pData[iSc*2]/bit_unit;
        int8_t bit_q = pData[iSc*2+1]/bit_unit;
        if(pData[iSc*2]<0)
        {
            bit_i=15+bit_i;
        }
        if(pData[iSc*2+1]<0)
        {
            bit_q=15+bit_q;
        }
        *pOut = (bit_i<<4)|bit_q;
        pOut++;
    }
}
#endif
void mod_compression_qpsk_avx512(int16_t *pData,int8_t *pOut, int16_t unit, int32_t nSc)
{

    __m512i permute_index = _mm512_set_epi16(24,25,26,27,28,29,30,31,
                                             16,17,18,19,20,21,22,23,
                                             8,9,10,11,12,13,14,15,
                                             0,1,2,3,4,5,6,7);

     //calculate loop size
    const int32_t nSc0 = nSc&0xfffffff0;
    const int32_t nSc1 = nSc&0xf;
    int32_t bits = 0;
    __m512i symbol;
    __m512i *pDataOffset = (__m512i *) pData;

#pragma unroll
    for(int32_t iSc=0; iSc<nSc0; iSc=iSc+16)
    {
        symbol = _mm512_loadu_epi32 (pDataOffset);
        pDataOffset++;
        symbol = _mm512_permutexvar_epi16 (permute_index, symbol);
        bits = _mm512_movepi16_mask(symbol);
        *(int32_t *)pOut = bits;
        pOut = pOut+4;
    }
    if(nSc1!=0)
    {
        __mmask16 k1=0;
        k1 = ((__mmask16)1<<nSc1)-1;

        symbol = _mm512_mask_loadu_epi32 (_mm512_setzero_si512(), k1, pDataOffset);
        symbol = _mm512_permutexvar_epi16 (permute_index, symbol);
        bits = _mm512_movepi16_mask(symbol);
        for (uint8_t idx = 0;idx<(((nSc1-1)>>2)+1);idx++)
        {
            *pOut = *(((int8_t *)&bits)+idx);
            pOut++;
        }
    }
}

inline __m512i
byte_pack2b(const __m512i comp_data)
{
    const __m512i k_shift_left = _mm512_set_epi64(0x0000000200040006, 0x0000000200040006,
                                                 0x0000000200040006, 0x0000000200040006,
                                                 0x0000000200040006, 0x0000000200040006,
                                                 0x0000000200040006, 0x0000000200040006);
    const auto comp_data_packed = _mm512_sllv_epi16(comp_data, k_shift_left);

    const __m512i k_byte_shufflemask1 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000800,
                                                        0x0000000000000000, 0x0000000000000800,
                                                        0x0000000000000000, 0x0000000000000800,
                                                        0x0000000000000000, 0x0000000000000800);
    constexpr uint64_t k_bytemask1 = 0x0003000300030003;
    const auto comp_data_shuff1 = _mm512_maskz_shuffle_epi8(k_bytemask1, comp_data_packed, k_byte_shufflemask1);

    const __m512i k_byte_shufflemask2 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000A02,
                                                        0x0000000000000000, 0x0000000000000A02,
                                                        0x0000000000000000, 0x0000000000000A02,
                                                        0x0000000000000000, 0x0000000000000A02);
    const auto comp_data_shuff2 = _mm512_maskz_shuffle_epi8(k_bytemask1, comp_data_packed, k_byte_shufflemask2);

    const __m512i k_byte_shufflemask3 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000C04,
                                                        0x0000000000000000, 0x0000000000000C04,
                                                        0x0000000000000000, 0x0000000000000C04,
                                                        0x0000000000000000, 0x0000000000000C04);
    const auto comp_data_shuff3 = _mm512_maskz_shuffle_epi8(k_bytemask1, comp_data_packed, k_byte_shufflemask3);

    const __m512i k_byte_shufflemask4 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000E06,
                                                        0x0000000000000000, 0x0000000000000E06,
                                                        0x0000000000000000, 0x0000000000000E06,
                                                        0x0000000000000000, 0x0000000000000E06);
    const auto comp_data_shuff4 = _mm512_maskz_shuffle_epi8(k_bytemask1, comp_data_packed, k_byte_shufflemask4);

    /// Ternary blend of the two shuffled results
    const __m512i k_ternlog_select1 = _mm512_set_epi64(0x0000000000000000, 0x0000000000003030,
                                                     0x0000000000000000, 0x0000000000003030,
                                                     0x0000000000000000, 0x0000000000003030,
                                                     0x0000000000000000, 0x0000000000003030);

    const __m512i k_ternlog_select2 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000C0C,
                                                     0x0000000000000000, 0x0000000000000C0C,
                                                     0x0000000000000000, 0x0000000000000C0C,
                                                     0x0000000000000000, 0x0000000000000C0C);

    const __m512i k_ternlog_select3 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000303,
                                                     0x0000000000000000, 0x0000000000000303,
                                                     0x0000000000000000, 0x0000000000000303,
                                                     0x0000000000000000, 0x0000000000000303);

    auto comp_data_packed2 =  _mm512_ternarylogic_epi64(comp_data_shuff1, comp_data_shuff2, k_ternlog_select1, 0xd8);
    auto comp_data_packed3 =  _mm512_ternarylogic_epi64(comp_data_packed2, comp_data_shuff3, k_ternlog_select2, 0xd8);
    return _mm512_ternarylogic_epi64(comp_data_packed3, comp_data_shuff4, k_ternlog_select3, 0xd8);
}

inline __m512i
byte_pack2b_snc(const __m512i comp_data)
{
    const __m512i k_shift_left = _mm512_set_epi64(0x0000000200040006, 0x0000000200040006,
                                                 0x0000000200040006, 0x0000000200040006,
                                                 0x0000000200040006, 0x0000000200040006,
                                                 0x0000000200040006, 0x0000000200040006);
    const auto comp_data_packed = _mm512_sllv_epi16(comp_data, k_shift_left);

    const __m512i k_byte_shufflemask1 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000800,
                                                        0x0000000000000000, 0x0000000000000800,
                                                        0x0000000000000000, 0x0000000000000800,
                                                        0x0000000000000000, 0x0000000000000800);
    constexpr uint64_t k_bytemask1 = 0x0003000300030003;
    const auto comp_data_shuff1 = _mm512_maskz_shuffle_epi8(k_bytemask1, comp_data_packed, k_byte_shufflemask1);

    const __m512i k_byte_shufflemask2 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000A02,
                                                        0x0000000000000000, 0x0000000000000A02,
                                                        0x0000000000000000, 0x0000000000000A02,
                                                        0x0000000000000000, 0x0000000000000A02);
    const auto comp_data_shuff2 = _mm512_maskz_shuffle_epi8(k_bytemask1, comp_data_packed, k_byte_shufflemask2);

    const __m512i k_byte_shufflemask3 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000C04,
                                                        0x0000000000000000, 0x0000000000000C04,
                                                        0x0000000000000000, 0x0000000000000C04,
                                                        0x0000000000000000, 0x0000000000000C04);
    const auto comp_data_shuff3 = _mm512_maskz_shuffle_epi8(k_bytemask1, comp_data_packed, k_byte_shufflemask3);

    const __m512i k_byte_shufflemask4 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000E06,
                                                        0x0000000000000000, 0x0000000000000E06,
                                                        0x0000000000000000, 0x0000000000000E06,
                                                        0x0000000000000000, 0x0000000000000E06);
    const auto comp_data_shuff4 = _mm512_maskz_shuffle_epi8(k_bytemask1, comp_data_packed, k_byte_shufflemask4);

    /// Ternary blend of the two shuffled results
    const __m512i k_ternlog_select1 = _mm512_set_epi64(0x0000000000000000, 0x0000000000003030,
                                                     0x0000000000000000, 0x0000000000003030,
                                                     0x0000000000000000, 0x0000000000003030,
                                                     0x0000000000000000, 0x0000000000003030);

    const __m512i k_ternlog_select2 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000C0C,
                                                     0x0000000000000000, 0x0000000000000C0C,
                                                     0x0000000000000000, 0x0000000000000C0C,
                                                     0x0000000000000000, 0x0000000000000C0C);

    const __m512i k_ternlog_select3 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000303,
                                                     0x0000000000000000, 0x0000000000000303,
                                                     0x0000000000000000, 0x0000000000000303,
                                                     0x0000000000000000, 0x0000000000000303);

    auto comp_data_packed2 =  _mm512_ternarylogic_epi64(comp_data_shuff1, comp_data_shuff2, k_ternlog_select1, 0xd8);
    auto comp_data_packed3 =  _mm512_ternarylogic_epi64(comp_data_packed2, comp_data_shuff3, k_ternlog_select2, 0xd8);
    auto comp_data_packed4 =  _mm512_ternarylogic_epi64(comp_data_packed3, comp_data_shuff4, k_ternlog_select3, 0xd8);
    const auto k_byte_permute =
      _mm512_setr_epi32(
                      0x11100100, 0x31302120, 0xFFFFFFFF, 0xFFFFFFFF,
                      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
                      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
                      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);

    return _mm512_permutexvar_epi8(k_byte_permute,comp_data_packed4);
}

void mod_compression_16qam_avx512(int16_t *pData, int8_t *pOut, int16_t unit, int32_t nSc)
{
    int16_t bit_unit = unit>>1;
    if (0 == bit_unit)
    {
        printf("modulation compression unit is too low!\n ");
        bit_unit = 1;
    }
    __m512i symbol,symbol_unit ,bit_convert,byte_pack;
    __mmask32 mask32 ;
    __mmask16 mask_store = 0x3;
    int32_t nSc0,nSc1;
    nSc0 = nSc&0xfffffff0;
    nSc1 = nSc&0xf;
    symbol_unit = _mm512_set1_epi16(bit_unit);
    bit_convert = _mm512_set1_epi16(3);
    for (int32_t iSc = 0 ; iSc<nSc0 ; iSc =iSc+16 ,pOut = pOut+8)
    {
        symbol = _mm512_loadu_epi16(pData);
        mask32 = _mm512_movepi16_mask(symbol);
        pData+=32;
        symbol = _mm512_div_epi16(symbol ,symbol_unit);
        symbol =_mm512_mask_add_epi16(symbol,mask32 ,symbol,bit_convert);

        byte_pack = byte_pack2b(symbol);
        _mm_mask_storeu_epi8(pOut , mask_store ,_mm512_extracti64x2_epi64(byte_pack, 0));
        _mm_mask_storeu_epi8(pOut+2 , mask_store ,_mm512_extracti64x2_epi64(byte_pack, 1));
        _mm_mask_storeu_epi8(pOut+4 , mask_store ,_mm512_extracti64x2_epi64(byte_pack, 2));
        _mm_mask_storeu_epi8(pOut+6 , mask_store ,_mm512_extracti64x2_epi64(byte_pack, 3));
    }

    if(nSc1!=0)
    {
        __mmask16 k1 , left_mask;
        k1 = ((__mmask16)1<<nSc1)-1;
        symbol = _mm512_mask_loadu_epi32(_mm512_setzero_epi32() ,k1 ,pData);
        mask32 = _mm512_movepi16_mask(symbol);
        symbol = _mm512_div_epi16(symbol ,symbol_unit);
        symbol =_mm512_mask_add_epi16(symbol,mask32 ,symbol,bit_convert);
        byte_pack = byte_pack2b(symbol);
        left_mask = (k1&0x1)|(((k1>>2)&0x1)<<1);
        _mm_mask_storeu_epi8(pOut , left_mask ,_mm512_extracti64x2_epi64(byte_pack, 0));
        left_mask = ((k1>>4)&0x1)|(((k1>>6)&0x1)<<1);
        _mm_mask_storeu_epi8(pOut+2 , left_mask ,_mm512_extracti64x2_epi64(byte_pack, 1));
        left_mask = ((k1>>8)&0x1)|(((k1>>10)&0x1)<<1);
        _mm_mask_storeu_epi8(pOut+4 , left_mask ,_mm512_extracti64x2_epi64(byte_pack, 2));
        left_mask = ((k1>>12)&0x1)|(((k1>>14)&0x1)<<1);
        _mm_mask_storeu_epi8(pOut+6 , left_mask ,_mm512_extracti64x2_epi64(byte_pack, 3));
    }
}

void mod_compression_16qam_snc(int16_t *pData, int8_t *pOut, int16_t unit, int32_t nSc)
{
    int16_t bit_unit = unit>>1;
    if (0 == bit_unit)
    {
        printf("modulation compression unit is too low!\n ");
        bit_unit = 1;
    }
    __m512i symbol,symbol_unit ,bit_convert,byte_pack;
    __mmask32 mask32 ;
    __mmask16 mask_store = 0x3;
    int32_t nSc0,nSc1;
    nSc0 = nSc&0xfffffff0;
    nSc1 = nSc&0xf;
    symbol_unit = _mm512_set1_epi16(bit_unit);
    bit_convert = _mm512_set1_epi16(3);
    for (int32_t iSc = 0 ; iSc<nSc0 ; iSc =iSc+16 ,pOut = pOut+8)
    {
        symbol = _mm512_loadu_epi16(pData);
        mask32 = _mm512_movepi16_mask(symbol);
        pData+=32;
        symbol = _mm512_div_epi16(symbol ,symbol_unit);
        symbol =_mm512_mask_add_epi16(symbol,mask32 ,symbol,bit_convert);
        _mm512_mask_storeu_epi32(pOut , mask_store ,byte_pack2b_snc(symbol));
    }

    if(nSc1!=0)
    {
        __mmask16 k1 , left_mask;
        int8_t left_byte = 0;
        k1 = ((__mmask16)1<<nSc1)-1;
        symbol = _mm512_mask_loadu_epi32(_mm512_setzero_epi32() ,k1 ,pData);
        mask32 = _mm512_movepi16_mask(symbol);
        symbol = _mm512_div_epi16(symbol ,symbol_unit);
        symbol =_mm512_mask_add_epi16(symbol,mask32 ,symbol,bit_convert);
        byte_pack = byte_pack2b_snc(symbol);
        left_byte = (nSc+1)>>1;
        left_mask = ((__mmask16)1<<left_byte)-1;
        _mm_mask_storeu_epi8(pOut , left_mask ,_mm512_extracti64x2_epi64(byte_pack, 0));
    }
}

inline __m512i
byte_pack3b(const __m512i comp_data)
{
    const __m512i k_shift_left = _mm512_set_epi64(0x0000000300060001, 0x0004000700020005,
                                                 0x0000000300060001, 0x0004000700020005,
                                                 0x0000000300060001, 0x0004000700020005,
                                                 0x0000000300060001, 0x0004000700020005);
    const auto comp_data_packed = _mm512_sllv_epi16(comp_data, k_shift_left);


    const __m512i k_shift_right = _mm512_set_epi64(0x0000000000020000, 0x0000000100000000,
                                                   0x0000000000020000, 0x0000000100000000,
                                                   0x0000000000020000, 0x0000000100000000,
                                                   0x0000000000020000, 0x0000000100000000);
    const auto comp_data_packed2 = _mm512_srlv_epi16(comp_data, k_shift_right);

    const __m512i k_byte_shufflemask1 = _mm512_set_epi64(0x0000000000000000, 0x00000000000A0400,
                                                        0x0000000000000000, 0x00000000000A0400,
                                                        0x0000000000000000, 0x00000000000A0400,
                                                        0x0000000000000000, 0x00000000000A0400);
    constexpr uint64_t k_bytemask1 = 0x0007000700070007;
    const auto comp_data_shuff1 = _mm512_maskz_shuffle_epi8(k_bytemask1, comp_data_packed, k_byte_shufflemask1);

    const __m512i k_byte_shufflemask2 = _mm512_set_epi64(0x0000000000000000, 0x00000000000C0602,
                                                        0x0000000000000000, 0x00000000000C0602,
                                                        0x0000000000000000, 0x00000000000C0602,
                                                        0x0000000000000000, 0x00000000000C0602);
    const auto comp_data_shuff2 = _mm512_maskz_shuffle_epi8(k_bytemask1, comp_data_packed, k_byte_shufflemask2);

    const __m512i k_byte_shufflemask3 = _mm512_set_epi64(0x0000000000000000, 0x00000000000E0800,
                                                        0x0000000000000000, 0x00000000000E0800,
                                                        0x0000000000000000, 0x00000000000E0800,
                                                        0x0000000000000000, 0x00000000000E0800);
    constexpr uint64_t k_bytemask2 = 0x0006000600060006;
    const auto comp_data_shuff3 = _mm512_maskz_shuffle_epi8(k_bytemask2, comp_data_packed, k_byte_shufflemask3);

    const __m512i k_byte_shufflemask4 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000A04,
                                                        0x0000000000000000, 0x0000000000000A04,
                                                        0x0000000000000000, 0x0000000000000A04,
                                                        0x0000000000000000, 0x0000000000000A04);
    constexpr uint64_t k_bytemask3 = 0x0003000300030003;
    const auto comp_data_shuff4 = _mm512_maskz_shuffle_epi8(k_bytemask3, comp_data_packed2, k_byte_shufflemask4);

    /// Ternary blend of the two shuffled results
    const __m512i k_ternlog_select1 = _mm512_set_epi64(0x0000000000000000, 0x000000000038701C,
                                                     0x0000000000000000, 0x000000000038701C,
                                                     0x0000000000000000, 0x000000000038701C,
                                                     0x0000000000000000, 0x000000000038701C);

    const __m512i k_ternlog_select2 = _mm512_set_epi64(0x0000000000000000, 0x0000000000070E00,
                                                     0x0000000000000000, 0x0000000000070E00,
                                                     0x0000000000000000, 0x0000000000070E00,
                                                     0x0000000000000000, 0x0000000000070E00);

    const __m512i k_ternlog_select3 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000103,
                                                     0x0000000000000000, 0x0000000000000103,
                                                     0x0000000000000000, 0x0000000000000103,
                                                     0x0000000000000000, 0x0000000000000103);

    auto comp_data_packed3 =  _mm512_ternarylogic_epi64(comp_data_shuff1, comp_data_shuff2, k_ternlog_select1, 0xd8);
    auto comp_data_packed4 =  _mm512_ternarylogic_epi64(comp_data_packed3, comp_data_shuff3, k_ternlog_select2, 0xd8);
    return _mm512_ternarylogic_epi64(comp_data_packed4, comp_data_shuff4, k_ternlog_select3, 0xd8);
}

inline __m512i
byte_pack3b_snc(const __m512i comp_data)
{
    const __m512i k_shift_left = _mm512_set_epi64(0x0000000300060001, 0x0004000700020005,
                                                 0x0000000300060001, 0x0004000700020005,
                                                 0x0000000300060001, 0x0004000700020005,
                                                 0x0000000300060001, 0x0004000700020005);
    const auto comp_data_packed = _mm512_sllv_epi16(comp_data, k_shift_left);


    const __m512i k_shift_right = _mm512_set_epi64(0x0000000000020000, 0x0000000100000000,
                                                   0x0000000000020000, 0x0000000100000000,
                                                   0x0000000000020000, 0x0000000100000000,
                                                   0x0000000000020000, 0x0000000100000000);
    const auto comp_data_packed2 = _mm512_srlv_epi16(comp_data, k_shift_right);

    const __m512i k_byte_shufflemask1 = _mm512_set_epi64(0x0000000000000000, 0x00000000000A0400,
                                                        0x0000000000000000, 0x00000000000A0400,
                                                        0x0000000000000000, 0x00000000000A0400,
                                                        0x0000000000000000, 0x00000000000A0400);
    constexpr uint64_t k_bytemask1 = 0x0007000700070007;
    const auto comp_data_shuff1 = _mm512_maskz_shuffle_epi8(k_bytemask1, comp_data_packed, k_byte_shufflemask1);

    const __m512i k_byte_shufflemask2 = _mm512_set_epi64(0x0000000000000000, 0x00000000000C0602,
                                                        0x0000000000000000, 0x00000000000C0602,
                                                        0x0000000000000000, 0x00000000000C0602,
                                                        0x0000000000000000, 0x00000000000C0602);
    const auto comp_data_shuff2 = _mm512_maskz_shuffle_epi8(k_bytemask1, comp_data_packed, k_byte_shufflemask2);

    const __m512i k_byte_shufflemask3 = _mm512_set_epi64(0x0000000000000000, 0x00000000000E0800,
                                                        0x0000000000000000, 0x00000000000E0800,
                                                        0x0000000000000000, 0x00000000000E0800,
                                                        0x0000000000000000, 0x00000000000E0800);
    constexpr uint64_t k_bytemask2 = 0x0006000600060006;
    const auto comp_data_shuff3 = _mm512_maskz_shuffle_epi8(k_bytemask2, comp_data_packed, k_byte_shufflemask3);

    const __m512i k_byte_shufflemask4 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000A04,
                                                        0x0000000000000000, 0x0000000000000A04,
                                                        0x0000000000000000, 0x0000000000000A04,
                                                        0x0000000000000000, 0x0000000000000A04);
    constexpr uint64_t k_bytemask3 = 0x0003000300030003;
    const auto comp_data_shuff4 = _mm512_maskz_shuffle_epi8(k_bytemask3, comp_data_packed2, k_byte_shufflemask4);

    /// Ternary blend of the two shuffled results
    const __m512i k_ternlog_select1 = _mm512_set_epi64(0x0000000000000000, 0x000000000038701C,
                                                     0x0000000000000000, 0x000000000038701C,
                                                     0x0000000000000000, 0x000000000038701C,
                                                     0x0000000000000000, 0x000000000038701C);

    const __m512i k_ternlog_select2 = _mm512_set_epi64(0x0000000000000000, 0x0000000000070E00,
                                                     0x0000000000000000, 0x0000000000070E00,
                                                     0x0000000000000000, 0x0000000000070E00,
                                                     0x0000000000000000, 0x0000000000070E00);

    const __m512i k_ternlog_select3 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000103,
                                                     0x0000000000000000, 0x0000000000000103,
                                                     0x0000000000000000, 0x0000000000000103,
                                                     0x0000000000000000, 0x0000000000000103);

    auto comp_data_packed3 =  _mm512_ternarylogic_epi64(comp_data_shuff1, comp_data_shuff2, k_ternlog_select1, 0xd8);
    auto comp_data_packed4 =  _mm512_ternarylogic_epi64(comp_data_packed3, comp_data_shuff3, k_ternlog_select2, 0xd8);
    auto comp_data_packed5 =  _mm512_ternarylogic_epi64(comp_data_packed4, comp_data_shuff4, k_ternlog_select3, 0xd8);

    const auto k_byte_permute =
      _mm512_setr_epi32(
                      0x10020100, 0x21201211, 0x32313022, 0xFFFFFFFF,
                      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
                      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
                      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);

    return _mm512_permutexvar_epi8(k_byte_permute,comp_data_packed5);
}

void mod_compression_64qam_avx512(int16_t *pData, int8_t *pOut, int16_t unit, int32_t nSc)
{
    int16_t bit_unit = unit>>2;
    if (0 == bit_unit)
    {
        printf("modulation compression unit is too low!\n ");
        bit_unit = 1;
    }
    __m512i symbol,symbol_unit ,bit_convert,byte_pack;
    __mmask32 mask32 ;
    __mmask16 mask_store = 0x7;
    int32_t nSc0,nSc1;
    nSc0 = nSc&0xfffffff0;
    nSc1 = nSc&0xf;
    symbol_unit = _mm512_set1_epi16(bit_unit);
    bit_convert = _mm512_set1_epi16(7);
    for (int32_t iSc = 0 ; iSc<nSc0 ; iSc =iSc+16 ,pOut = pOut+12)
    {
        symbol = _mm512_loadu_epi16(pData);
        mask32 = _mm512_movepi16_mask(symbol);
        pData+=32;
        symbol = _mm512_div_epi16(symbol ,symbol_unit);
        symbol =_mm512_mask_add_epi16(symbol,mask32 ,symbol,bit_convert);

        byte_pack = byte_pack3b(symbol);
        _mm_mask_storeu_epi8(pOut , mask_store ,_mm512_extracti64x2_epi64(byte_pack, 0));
        _mm_mask_storeu_epi8(pOut+3 , mask_store ,_mm512_extracti64x2_epi64(byte_pack, 1));
        _mm_mask_storeu_epi8(pOut+6 , mask_store ,_mm512_extracti64x2_epi64(byte_pack, 2));
        _mm_mask_storeu_epi8(pOut+9 , mask_store ,_mm512_extracti64x2_epi64(byte_pack, 3));
    }

    if(nSc1!=0)
    {
        __mmask16 k1 , left_mask;
        k1 = ((__mmask16)1<<nSc1)-1;
        symbol = _mm512_mask_loadu_epi32(_mm512_setzero_epi32() ,k1 ,pData);
        mask32 = _mm512_movepi16_mask(symbol);
        symbol = _mm512_div_epi16(symbol ,symbol_unit);
        symbol =_mm512_mask_add_epi16(symbol,mask32 ,symbol,bit_convert);
        byte_pack = byte_pack3b(symbol);
        left_mask = k1&mask_store;
        _mm_mask_storeu_epi8(pOut , left_mask ,_mm512_extracti64x2_epi64(byte_pack, 0));
        left_mask = (k1>>4)&mask_store;
        _mm_mask_storeu_epi8(pOut+3 , left_mask ,_mm512_extracti64x2_epi64(byte_pack, 1));
        left_mask = (k1>>8)&mask_store;
        _mm_mask_storeu_epi8(pOut+6 , left_mask ,_mm512_extracti64x2_epi64(byte_pack, 2));
        left_mask = (k1>>12)&mask_store;
        _mm_mask_storeu_epi8(pOut+9 , left_mask ,_mm512_extracti64x2_epi64(byte_pack, 3));
    }
}

void mod_compression_64qam_snc(int16_t *pData, int8_t *pOut, int16_t unit, int32_t nSc)
{
    int16_t bit_unit = unit>>2;
    if (0 == bit_unit)
    {
        printf("modulation compression unit is too low!\n ");
        bit_unit = 1;
    }
    __m512i symbol,symbol_unit ,bit_convert,byte_pack;
    __mmask32 mask32 ;
    __mmask16 mask_store = 0x7;
    int32_t nSc0,nSc1;
    nSc0 = nSc&0xfffffff0;
    nSc1 = nSc&0xf;
    symbol_unit = _mm512_set1_epi16(bit_unit);
    bit_convert = _mm512_set1_epi16(7);
    for (int32_t iSc = 0 ; iSc<nSc0 ; iSc =iSc+16 ,pOut = pOut+12)
    {
        symbol = _mm512_loadu_epi16(pData);
        mask32 = _mm512_movepi16_mask(symbol);
        pData+=32;
        symbol = _mm512_div_epi16(symbol ,symbol_unit);
        symbol =_mm512_mask_add_epi16(symbol,mask32 ,symbol,bit_convert);
        _mm512_mask_storeu_epi32(pOut , mask_store ,byte_pack3b_snc(symbol));
    }

    if(nSc1!=0)
    {
        __mmask16 k1 , left_mask;
        int8_t left_byte = 0;
        k1 = ((__mmask16)1<<nSc1)-1;
        symbol = _mm512_mask_loadu_epi32(_mm512_setzero_epi32() ,k1 ,pData);
        mask32 = _mm512_movepi16_mask(symbol);
        symbol = _mm512_div_epi16(symbol ,symbol_unit);
        symbol =_mm512_mask_add_epi16(symbol,mask32 ,symbol,bit_convert);
        byte_pack = byte_pack3b_snc(symbol);
        left_byte = (nSc*3+3)>>2;
        left_mask = ((__mmask16)1<<left_byte)-1;
        _mm_mask_storeu_epi8(pOut , left_mask ,_mm512_extracti64x2_epi64(byte_pack, 0));
    }
}

inline __m512i
byte_pack4b(const __m512i comp_data)
{
    const __m512i k_shift_Left = _mm512_set_epi64(0x0000000400000004, 0x0000000400000004,
                                                 0x0000000400000004, 0x0000000400000004,
                                                 0x0000000400000004, 0x0000000400000004,
                                                 0x0000000400000004, 0x0000000400000004);
    const auto comp_data_packed = _mm512_sllv_epi16(comp_data, k_shift_Left);

    const __m512i k_byte_shufflemask1 = _mm512_set_epi64(0x0000000000000000, 0x000000000c080400,
                                                        0x0000000000000000, 0x000000000c080400,
                                                        0x0000000000000000, 0x000000000c080400,
                                                        0x0000000000000000, 0x000000000c080400);
    constexpr uint64_t k_bytemask1 = 0x000F000F000F000F;
    const auto comp_data_shuff1 = _mm512_maskz_shuffle_epi8(k_bytemask1, comp_data_packed, k_byte_shufflemask1);

    const __m512i k_byte_shufflemask2 = _mm512_set_epi64(0x0000000000000000, 0x000000000E0A0602,
                                                        0x0000000000000000, 0x000000000E0A0602,
                                                        0x0000000000000000, 0x000000000E0A0602,
                                                        0x0000000000000000, 0x000000000E0A0602);
    const auto comp_data_shuff2 = _mm512_maskz_shuffle_epi8(k_bytemask1, comp_data_packed, k_byte_shufflemask2);

    /// Ternary blend of the two shuffled results
    const __m512i k_ternlog_select = _mm512_set_epi64(0x0000000000000000, 0x000000000F0F0F0F,
                                                     0x0000000000000000, 0x000000000F0F0F0F,
                                                     0x0000000000000000, 0x000000000F0F0F0F,
                                                     0x0000000000000000, 0x000000000F0F0F0F);
    const auto comp_data_packed2 =  _mm512_ternarylogic_epi64(comp_data_shuff1, comp_data_shuff2, k_ternlog_select, 0xd8);

    const __m512i k_dwordmask = _mm512_set_epi64(0x0000000F0000000F, 0x0000000F0000000F,
                                                 0x0000000F0000000F, 0x0000000F0000000F,
                                                 0x0000000F0000000F, 0x0000000F0000000F,
                                                 0x0000000C00000008, 0x0000000400000000);
    return _mm512_permutevar_epi32 (k_dwordmask,comp_data_packed2);
}

void mod_compression_256qam_avx512(int16_t *pData, int8_t *pOut, int16_t unit, int32_t nSc)
{
    int16_t bit_unit = unit>>3;
    if (0 == bit_unit)
    {
        printf("modulation compression unit is too low!\n ");
        bit_unit = 1;
    }
    __m512i symbol,symbol_unit ,bit_convert;
    __mmask32 mask32 ;
    __mmask16 mask_store =0xF;
    int32_t nSc0,nSc1;
    nSc0 = nSc&0xfffffff0;
    nSc1 = nSc&0xf;
    symbol_unit = _mm512_set1_epi16(bit_unit);
    bit_convert = _mm512_set1_epi16(15);
    for (int32_t iSc = 0 ; iSc<nSc0 ; iSc =iSc+16)
    {
        symbol = _mm512_loadu_epi16(pData);
        mask32 = _mm512_movepi16_mask(symbol);
        pData+=32;
        symbol = _mm512_div_epi16(symbol ,symbol_unit);
        symbol =_mm512_mask_add_epi16(symbol,mask32 ,symbol,bit_convert);

        _mm512_mask_storeu_epi32 (pOut,mask_store, byte_pack4b(symbol));
        pOut+=16;
    }

    if(nSc1!=0)
    {
        __mmask16 k1;
        k1 = ((__mmask16)1<<nSc1)-1;
        symbol = _mm512_mask_loadu_epi32(_mm512_setzero_epi32() ,k1 ,pData);
        mask32 = _mm512_movepi16_mask(symbol);
        symbol = _mm512_div_epi16(symbol ,symbol_unit);
        symbol =_mm512_mask_add_epi16(symbol,mask32 ,symbol,bit_convert);
        _mm512_mask_storeu_epi8 (pOut ,(__mmask64)k1 ,byte_pack4b(symbol));
    }
}

void mod_compression_256qam_snc(int16_t *pData, int8_t *pOut, int16_t unit, int32_t nSc)
{
    int16_t bit_unit = unit>>3;
    if (0 == bit_unit)
    {
        printf("modulation compression unit is too low!\n ");
        bit_unit = 1;
    }
    __m512i symbol,symbol_unit ,bit_convert;
    __mmask32 mask32 ;
    __mmask16 mask_store =0xF;
    int32_t nSc0,nSc1;
    nSc0 = nSc&0xfffffff0;
    nSc1 = nSc&0xf;
    symbol_unit = _mm512_set1_epi16(bit_unit);
    bit_convert = _mm512_set1_epi16(15);
    for (int32_t iSc = 0 ; iSc<nSc0 ; iSc =iSc+16)
    {
        symbol = _mm512_loadu_epi16(pData);
        mask32 = _mm512_movepi16_mask(symbol);
        pData+=32;
        symbol = _mm512_div_epi16(symbol ,symbol_unit);
        symbol =_mm512_mask_add_epi16(symbol,mask32 ,symbol,bit_convert);

        _mm512_mask_storeu_epi32 (pOut,mask_store, byte_pack4b(symbol));
        pOut+=16;
    }

    if(nSc1!=0)
    {
        __mmask16 k1;
        k1 = ((__mmask16)1<<nSc1)-1;
        symbol = _mm512_mask_loadu_epi32(_mm512_setzero_epi32() ,k1 ,pData);
        mask32 = _mm512_movepi16_mask(symbol);
        symbol = _mm512_div_epi16(symbol ,symbol_unit);
        symbol =_mm512_mask_add_epi16(symbol,mask32 ,symbol,bit_convert);
        _mm512_mask_storeu_epi8 (pOut ,(__mmask64)k1 ,byte_pack4b(symbol));
    }
}

int xranlib_5gnr_mod_compression_snc(const struct xranlib_5gnr_mod_compression_request* request,
        struct xranlib_5gnr_mod_compression_response* response){

    switch(request->modulation)
    {
      case XRAN_QPSK:
          mod_compression_qpsk_avx512(request->data_in, response->data_out, request->unit, request->num_symbols);
      break;
      case XRAN_QAM16:
          mod_compression_16qam_snc(request->data_in, response->data_out, request->unit, request->num_symbols);
      break;
      case XRAN_QAM64:
          mod_compression_64qam_snc(request->data_in, response->data_out, request->unit, request->num_symbols);
      break;
       case XRAN_QAM256:
          mod_compression_256qam_snc(request->data_in, response->data_out, request->unit, request->num_symbols);
      break;
      default:
          printf("Error invalid modulation compression request\n");
          return -1;
    }
    return 0;
}

int xranlib_5gnr_mod_compression(const struct xranlib_5gnr_mod_compression_request* request,
        struct xranlib_5gnr_mod_compression_response* response){
#ifdef C_Module_Used
    return (xranlib_5gnr_mod_compression_c(request, response));
#else
    if(_may_i_use_cpu_feature(_FEATURE_AVX512IFMA52))
        return (xranlib_5gnr_mod_compression_snc(request, response));
    else
        return (xranlib_5gnr_mod_compression_avx512(request, response));
#endif
}

#ifdef C_Module_Used
int xranlib_5gnr_mod_compression_c(const struct xranlib_5gnr_mod_compression_request* request,
        struct xranlib_5gnr_mod_compression_response* response){

    switch(request->modulation)
    {
      case XRAN_QPSK:
          mod_compression_qpsk_c(request->data_in, response->data_out, request->unit, request->num_symbols);
      break;
      case XRAN_QAM16:
          mod_compression_16qam_c(request->data_in, response->data_out, request->unit,request->num_symbols);
      break;
      case XRAN_QAM64:
          mod_compression_64qam_c(request->data_in, response->data_out, request->unit, request->num_symbols);
      break;
       case XRAN_QAM256:
          mod_compression_256qam_c(request->data_in, response->data_out, request->unit, request->num_symbols);
      break;
      default:
          printf("Error invalid modulation compression request\n");
          return -1;
    }
    return 0;
}
#endif
int xranlib_5gnr_mod_compression_avx512(const struct xranlib_5gnr_mod_compression_request* request,
        struct xranlib_5gnr_mod_compression_response* response){

    switch(request->modulation)
    {
      case XRAN_QPSK:
          mod_compression_qpsk_avx512(request->data_in, response->data_out, request->unit, request->num_symbols);
      break;
      case XRAN_QAM16:
          mod_compression_16qam_avx512(request->data_in, response->data_out, request->unit, request->num_symbols);
      break;
      case XRAN_QAM64:
          mod_compression_64qam_avx512(request->data_in, response->data_out, request->unit, request->num_symbols);
      break;
       case XRAN_QAM256:
          mod_compression_256qam_avx512(request->data_in, response->data_out, request->unit, request->num_symbols);
      break;
      default:
          printf("Error invalid modulation compression request\n");
          return -1;
    }
    return 0;
}

void
mod_decompression_qpsk_c(int8_t *pData,int16_t *pOut,int16_t unit, int32_t nSc ,int16_t re_mask)
{
    int16_t symbol_unit[2] = {0};
    symbol_unit[0] = (unit>>1);
    symbol_unit[1] = (unit>>1)*-1;
    for (int32_t iSc = 0 ; iSc<nSc ; iSc ++)
    {
        uint8_t mask_pos= iSc %12;
        if (1 == ((re_mask >> mask_pos)&0x1))
        {
            uint8_t symbol_pos= iSc &0x3;
            uint32_t byte_pos= iSc >>2;
            uint8_t bit_i = (pData[byte_pos]>>(7-(symbol_pos*2)))&0x1;
            pOut[iSc*2] = symbol_unit[bit_i];
            uint8_t bit_q = (pData[byte_pos]>>(6-(symbol_pos*2)))&0x1;
            pOut[iSc*2+1] = symbol_unit[bit_q];
        }
    }
}

void
mod_decompression_16qam_c(int8_t *pData,int16_t *pOut,int16_t unit, int32_t nSc)
{
    int16_t symbol_unit[4] = {0};
    symbol_unit[0] = (unit>>2);
    symbol_unit[1] = (unit>>2)*3;
    symbol_unit[3] = (unit>>2)*-1;
    symbol_unit[2] = (unit>>2)*-3;
    for (int32_t iSc = 0 ; iSc<nSc ; iSc ++)
    {
        uint8_t symbol_pos= iSc &0x1;
        uint32_t byte_pos= iSc >>1;
        uint8_t bit_i = (pData[byte_pos]>>(6-(symbol_pos*4)))&0x3;
        pOut[iSc*2] = symbol_unit[bit_i];
        uint8_t bit_q = (pData[byte_pos]>>(4-(symbol_pos*4)))&0x3;
        pOut[iSc*2+1] = symbol_unit[bit_q];
    }
}

void
mod_decompression_64qam_c(int8_t *pData,int16_t *pOut,int16_t unit, int32_t nSc)
{
    int16_t symbol_unit[8] = {0};
    symbol_unit[0] = (unit>>3);
    symbol_unit[1] = (unit>>3)*3;
    symbol_unit[2] = (unit>>3)*5;
    symbol_unit[3] = (unit>>3)*7;
    symbol_unit[7] = (unit>>3)*-1;
    symbol_unit[6] = (unit>>3)*-3;
    symbol_unit[5] = (unit>>3)*-5;
    symbol_unit[4] = (unit>>3)*-7;
    uint8_t bit_i , bit_q ;
    for (int32_t iSc = 0 ; iSc<nSc ; iSc ++)
    {
        uint8_t symbol_pos= iSc %4;
        if (0 == symbol_pos)
        {
            bit_i = (pData[0]>>5)&0x7;
            bit_q = (pData[0]>>2)&0x7;
        }
        else if (1 == symbol_pos)
        {
            bit_i = ((pData[0]&0x3)<<1)|((pData[1]>>7)&0x1);
            bit_q = (pData[1]>>4)&0x7;
        }
        else if (2 == symbol_pos)
        {
            bit_q = ((pData[1]&0x1)<<2)|((pData[2]>>6)&0x3);
            bit_i = (pData[1]>>1)&0x7;
        }
        else if (3 == symbol_pos)
        {
            bit_i = (pData[2]>>3)&0x7;
            bit_q = pData[2]&0x7;
            pData +=3;
        }
        pOut[iSc*2] = symbol_unit[bit_i];
        pOut[iSc*2+1] = symbol_unit[bit_q];
    }
}

void
mod_decompression_256qam_c(int8_t *pData,int16_t *pOut,int16_t unit,int32_t nSc)
{
    int16_t symbol_unit[16] = {0};
    symbol_unit[0] = (unit>>4);
    symbol_unit[1] = (unit>>4)*3;
    symbol_unit[2] = (unit>>4)*5;
    symbol_unit[3] = (unit>>4)*7;
    symbol_unit[4] = (unit>>4)*9;
    symbol_unit[5] = (unit>>4)*11;
    symbol_unit[6] = (unit>>4)*13;
    symbol_unit[7] = (unit>>4)*15;
    symbol_unit[15] = (unit>>4)*-1;
    symbol_unit[14] = (unit>>4)*-3;
    symbol_unit[13] = (unit>>4)*-5;
    symbol_unit[12] = (unit>>4)*-7;
    symbol_unit[11] = (unit>>4)*-9;
    symbol_unit[10] = (unit>>4)*-11;
    symbol_unit[9] = (unit>>4)*-13;
    symbol_unit[8] = (unit>>4)*-15;
    for (int32_t iSc = 0 ; iSc<nSc ; iSc ++)
    {
        uint8_t bit_i = (pData[iSc]>>4)&0xF;
        uint8_t bit_q = pData[iSc]&0xF;
        pOut[iSc*2] = symbol_unit[bit_i];
        pOut[iSc*2+1] = symbol_unit[bit_q];
    }
}

int xranlib_5gnr_mod_decompression(const struct xranlib_5gnr_mod_decompression_request* request,
        struct xranlib_5gnr_mod_decompression_response* response){

    switch(request->modulation)
    {
      case XRAN_QPSK:
          mod_decompression_qpsk_c(request->data_in, response->data_out, request->unit, request->num_symbols, request->re_mask);
      break;
      case XRAN_QAM16:
          mod_decompression_16qam_c(request->data_in, response->data_out, request->unit,request->num_symbols);
      break;
      case XRAN_QAM64:
          mod_decompression_64qam_c(request->data_in, response->data_out, request->unit, request->num_symbols);
      break;
       case XRAN_QAM256:
          mod_decompression_256qam_c(request->data_in, response->data_out, request->unit, request->num_symbols);
      break;
      default:
          printf("Error invalid modulation compression request\n");
          return -1;
    }
    return 0;
}


