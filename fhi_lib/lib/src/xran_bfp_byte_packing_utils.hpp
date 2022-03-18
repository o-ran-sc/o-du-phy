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
 * @brief xRAN BFP byte packing utilities functions
 *
 * @file xran_bfp_byte_packing_utils.hpp
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#pragma once
#include <immintrin.h>

namespace BlockFloatCompander
{
  /// Define function signatures for byte packing functions
  typedef __m512i(*PackFunction)(const __m512i);
  typedef __m512i(*UnpackFunction)(const uint8_t*);
  typedef __m256i(*UnpackFunction256)(const uint8_t*);

  /// Pack compressed 9 bit data in network byte order
  inline __m512i
  networkBytePack9b(const __m512i compData)
  {
    /// Logical shift left to align network order byte parts
    const __m512i k_shiftLeft = _mm512_set_epi64(0x0000000100020003, 0x0004000500060007,
                                                 0x0000000100020003, 0x0004000500060007,
                                                 0x0000000100020003, 0x0004000500060007,
                                                 0x0000000100020003, 0x0004000500060007);
    const auto compDataPacked = _mm512_sllv_epi16(compData, k_shiftLeft);

    /// First epi8 shuffle of even indexed samples
    const __m512i k_byteShuffleMask1 = _mm512_set_epi64(0x0000000000000000, 0x0C0D080904050001,
                                                        0x0000000000000000, 0x0C0D080904050001,
                                                        0x0000000000000000, 0x0C0D080904050001,
                                                        0x0000000000000000, 0x0C0D080904050001);
    constexpr uint64_t k_byteMask1 = 0x00FF00FF00FF00FF;
    const auto compDataShuff1 = _mm512_maskz_shuffle_epi8(k_byteMask1, compDataPacked, k_byteShuffleMask1);

    /// Second epi8 shuffle of odd indexed samples
    const __m512i k_byteShuffleMask2 = _mm512_set_epi64(0x000000000000000E, 0x0F0A0B0607020300,
                                                        0x000000000000000E, 0x0F0A0B0607020300,
                                                        0x000000000000000E, 0x0F0A0B0607020300,
                                                        0x000000000000000E, 0x0F0A0B0607020300);
    constexpr uint64_t k_byteMask2 = 0x01FE01FE01FE01FE;
    const auto compDataShuff2 = _mm512_maskz_shuffle_epi8(k_byteMask2, compDataPacked, k_byteShuffleMask2);

    /// Ternary blend of the two shuffled results
    const __m512i k_ternLogSelect = _mm512_set_epi64(0x00000000000000FF, 0x01FC07F01FC07F00,
                                                     0x00000000000000FF, 0x01FC07F01FC07F00,
                                                     0x00000000000000FF, 0x01FC07F01FC07F00,
                                                     0x00000000000000FF, 0x01FC07F01FC07F00);
    return _mm512_ternarylogic_epi64(compDataShuff1, compDataShuff2, k_ternLogSelect, 0xd8);
  }


  /// Pack compressed 10 bit data in network byte order
  inline __m512i
  networkBytePack10b(const __m512i compData)
  {
    /// Logical shift left to align network order byte parts
    const __m512i k_shiftLeft = _mm512_set_epi64(0x0000000200040006, 0x0000000200040006,
                                                 0x0000000200040006, 0x0000000200040006,
                                                 0x0000000200040006, 0x0000000200040006,
                                                 0x0000000200040006, 0x0000000200040006);
    const auto compDataPacked = _mm512_sllv_epi16(compData, k_shiftLeft);

    /// First epi8 shuffle of even indexed samples
    const __m512i k_byteShuffleMask1 = _mm512_set_epi64(0x000000000000000C, 0x0D08090004050001,
                                                        0x000000000000000C, 0x0D08090004050001,
                                                        0x000000000000000C, 0x0D08090004050001,
                                                        0x000000000000000C, 0x0D08090004050001);
    constexpr uint64_t k_byteMask1 = 0x01EF01EF01EF01EF;
    const auto compDataShuff1 = _mm512_maskz_shuffle_epi8(k_byteMask1, compDataPacked, k_byteShuffleMask1);

    /// Second epi8 shuffle of odd indexed samples
    const __m512i k_byteShuffleMask2 = _mm512_set_epi64(0x0000000000000E0F, 0x0A0B000607020300,
                                                        0x0000000000000E0F, 0x0A0B000607020300,
                                                        0x0000000000000E0F, 0x0A0B000607020300,
                                                        0x0000000000000E0F, 0x0A0B000607020300);
    constexpr uint64_t k_byteMask2 = 0x03DE03DE03DE03DE;
    const auto compDataShuff2 = _mm512_maskz_shuffle_epi8(k_byteMask2, compDataPacked, k_byteShuffleMask2);

    /// Ternary blend of the two shuffled results
    const __m512i k_ternLogSelect = _mm512_set_epi64(0x000000000000FF03, 0xF03F00FF03F03F00,
                                                     0x000000000000FF03, 0xF03F00FF03F03F00,
                                                     0x000000000000FF03, 0xF03F00FF03F03F00,
                                                     0x000000000000FF03, 0xF03F00FF03F03F00);
    return _mm512_ternarylogic_epi64(compDataShuff1, compDataShuff2, k_ternLogSelect, 0xd8);
  }


  /// Pack compressed 12 bit data in network byte order
  inline __m512i
  networkBytePack12b(const __m512i compData)
  {
    /// Logical shift left to align network order byte parts
    const __m512i k_shiftLeft = _mm512_set_epi64(0x0000000400000004, 0x0000000400000004,
                                                 0x0000000400000004, 0x0000000400000004,
                                                 0x0000000400000004, 0x0000000400000004,
                                                 0x0000000400000004, 0x0000000400000004);
    const auto compDataPacked = _mm512_sllv_epi16(compData, k_shiftLeft);

    /// First epi8 shuffle of even indexed samples
    const __m512i k_byteShuffleMask1 = _mm512_set_epi64(0x00000000000C0D00, 0x0809000405000001,
                                                        0x00000000000C0D00, 0x0809000405000001,
                                                        0x00000000000C0D00, 0x0809000405000001,
                                                        0x00000000000C0D00, 0x0809000405000001);
    constexpr uint64_t k_byteMask1 = 0x06DB06DB06DB06DB;
    const auto compDataShuff1 = _mm512_maskz_shuffle_epi8(k_byteMask1, compDataPacked, k_byteShuffleMask1);

    /// Second epi8 shuffle of odd indexed samples
    const __m512i k_byteShuffleMask2 = _mm512_set_epi64(0x000000000E0F000A, 0x0B00060700020300,
                                                        0x000000000E0F000A, 0x0B00060700020300,
                                                        0x000000000E0F000A, 0x0B00060700020300,
                                                        0x000000000E0F000A, 0x0B00060700020300);
    constexpr uint64_t k_byteMask2 = 0x0DB60DB60DB60DB6;
    const auto compDataShuff2 = _mm512_maskz_shuffle_epi8(k_byteMask2, compDataPacked, k_byteShuffleMask2);

    /// Ternary blend of the two shuffled results
    const __m512i k_ternLogSelect = _mm512_set_epi64(0x00000000FF0F00FF, 0x0F00FF0F00FF0F00,
                                                     0x00000000FF0F00FF, 0x0F00FF0F00FF0F00,
                                                     0x00000000FF0F00FF, 0x0F00FF0F00FF0F00,
                                                     0x00000000FF0F00FF, 0x0F00FF0F00FF0F00);
    return _mm512_ternarylogic_epi64(compDataShuff1, compDataShuff2, k_ternLogSelect, 0xd8);
  }


  /// Unpack compressed 9 bit data in network byte order
  inline __m512i
  networkByteUnpack9b(const uint8_t* inData)
  {
    /// Align chunks of compressed bytes into lanes to allow for expansion
    const __m512i* rawDataIn = reinterpret_cast<const __m512i*>(inData);
    const auto k_expPerm = _mm512_set_epi32(9, 8, 7, 6, 7, 6, 5, 4,
                                            5, 4, 3, 2, 3, 2, 1, 0);
    const auto inLaneAlign = _mm512_permutexvar_epi32(k_expPerm, *rawDataIn);

    /// Byte shuffle to get all bits for each sample into 16b chunks
    /// Due to previous permute to get chunks of bytes into each lane, there is
    /// a different shuffle offset in each lane
    const __m512i k_byteShuffleMask = _mm512_set_epi64(0x0A0B090A08090708, 0x0607050604050304,
                                                       0x090A080907080607, 0x0506040503040203,
                                                       0x0809070806070506, 0x0405030402030102,
                                                       0x0708060705060405, 0x0304020301020001);
    const auto inDatContig = _mm512_shuffle_epi8(inLaneAlign, k_byteShuffleMask);

    /// Logical shift left to set sign bit
    const __m512i k_slBits = _mm512_set_epi64(0x0007000600050004, 0x0003000200010000,
                                              0x0007000600050004, 0x0003000200010000,
                                              0x0007000600050004, 0x0003000200010000,
                                              0x0007000600050004, 0x0003000200010000);
    const auto inSetSign = _mm512_sllv_epi16(inDatContig, k_slBits);

    /// Mask to zero unwanted bits
    const __m512i k_expMask = _mm512_set1_epi16(0xFF80);
    return _mm512_and_epi64(inSetSign, k_expMask);
  }


  /// Unpack compressed 10 bit data in network byte order
  inline __m512i
  networkByteUnpack10b(const uint8_t* inData)
  {
    /// Align chunks of compressed bytes into lanes to allow for expansion
    const __m512i* rawDataIn = reinterpret_cast<const __m512i*>(inData);
    const auto k_expPerm = _mm512_set_epi32(10, 9, 8, 7, 8, 7, 6, 5,
                                             5, 4, 3, 2, 3, 2, 1, 0);
    const auto inLaneAlign = _mm512_permutexvar_epi32(k_expPerm, *rawDataIn);

    /// Byte shuffle to get all bits for each sample into 16b chunks
    /// Due to previous permute to get chunks of bytes into each lane, lanes
    /// 0 and 2 happen to be aligned, but lane 1 is offset by 2 bytes
    const __m512i k_byteShuffleMask = _mm512_set_epi64(0x0A0B090A08090708, 0x0506040503040203,
                                                       0x0809070806070506, 0x0304020301020001,
                                                       0x0A0B090A08090708, 0x0506040503040203,
                                                       0x0809070806070506, 0x0304020301020001);
    const auto inDatContig = _mm512_shuffle_epi8(inLaneAlign, k_byteShuffleMask);

    /// Logical shift left to set sign bit
    const __m512i k_slBits = _mm512_set_epi64(0x0006000400020000, 0x0006000400020000,
                                              0x0006000400020000, 0x0006000400020000,
                                              0x0006000400020000, 0x0006000400020000,
                                              0x0006000400020000, 0x0006000400020000);
    const auto inSetSign = _mm512_sllv_epi16(inDatContig, k_slBits);

    /// Mask to zero unwanted bits
    const __m512i k_expMask = _mm512_set1_epi16(0xFFC0);
    return _mm512_and_epi64(inSetSign, k_expMask);
  }


  /// Unpack compressed 12 bit data in network byte order
  inline __m512i
  networkByteUnpack12b(const uint8_t* inData)
  {
    /// Align chunks of compressed bytes into lanes to allow for expansion
    const __m512i* rawDataIn = reinterpret_cast<const __m512i*>(inData);
    const auto k_expPerm = _mm512_set_epi32(12, 11, 10, 9, 9, 8, 7, 6,
                                             6, 5, 4, 3, 3, 2, 1, 0);
    const auto inLaneAlign = _mm512_permutexvar_epi32(k_expPerm, *rawDataIn);

    /// Byte shuffle to get all bits for each sample into 16b chunks
    /// For 12b mantissa all lanes post-permute are aligned and require same shuffle offset
    const __m512i k_byteShuffleMask = _mm512_set_epi64(0x0A0B090A07080607, 0x0405030401020001,
                                                       0x0A0B090A07080607, 0x0405030401020001,
                                                       0x0A0B090A07080607, 0x0405030401020001,
                                                       0x0A0B090A07080607, 0x0405030401020001);
    const auto inDatContig = _mm512_shuffle_epi8(inLaneAlign, k_byteShuffleMask);

    /// Logical shift left to set sign bit
    const __m512i k_slBits = _mm512_set_epi64(0x0004000000040000, 0x0004000000040000,
                                              0x0004000000040000, 0x0004000000040000,
                                              0x0004000000040000, 0x0004000000040000,
                                              0x0004000000040000, 0x0004000000040000);
    const auto inSetSign = _mm512_sllv_epi16(inDatContig, k_slBits);

    /// Mask to zero unwanted bits
    const __m512i k_expMask = _mm512_set1_epi16(0xFFF0);
    return _mm512_and_epi64(inSetSign, k_expMask);
  }


  /// Unpack compressed 9 bit data in network byte order
  /// This unpacking function is for 256b registers
  inline __m256i
  networkByteUnpack9b256(const uint8_t* inData)
  {
    /// Align chunks of compressed bytes into lanes to allow for expansion
    const __m256i* rawDataIn = reinterpret_cast<const __m256i*>(inData);
    const auto k_expPerm = _mm256_set_epi32(5, 4, 3, 2, 3, 2, 1, 0);
    const auto inLaneAlign = _mm256_permutexvar_epi32(k_expPerm, *rawDataIn);

    /// Byte shuffle to get all bits for each sample into 16b chunks
    /// Due to previous permute to get chunks of bytes into each lane, there is
    /// a different shuffle offset in each lane
    const __m256i k_byteShuffleMask = _mm256_set_epi64x(0x0809070806070506, 0x0405030402030102,
                                                        0x0708060705060405, 0x0304020301020001);
    const auto inDatContig = _mm256_shuffle_epi8(inLaneAlign, k_byteShuffleMask);

    /// Logical shift left to set sign bit
    const __m256i k_slBits = _mm256_set_epi64x(0x0007000600050004, 0x0003000200010000,
                                               0x0007000600050004, 0x0003000200010000);
    const auto inSetSign = _mm256_sllv_epi16(inDatContig, k_slBits);

    /// Mask to zero unwanted bits
    const __m256i k_expMask = _mm256_set1_epi16(0xFF80);
    return _mm256_and_si256(inSetSign, k_expMask);
  }


  /// Unpack compressed 10 bit data in network byte order
  /// This unpacking function is for 256b registers
  inline __m256i
  networkByteUnpack10b256(const uint8_t* inData)
  {
    /// Align chunks of compressed bytes into lanes to allow for expansion
    const __m256i* rawDataIn = reinterpret_cast<const __m256i*>(inData);
    const auto k_expPerm = _mm256_set_epi32(5, 4, 3, 2, 3, 2, 1, 0);
    const auto inLaneAlign = _mm256_permutexvar_epi32(k_expPerm, *rawDataIn);

    /// Byte shuffle to get all bits for each sample into 16b chunks
    /// Due to previous permute to get chunks of bytes into each lane, lanes
    /// 0 and 2 happen to be aligned, but lane 1 is offset by 2 bytes
    const __m256i k_byteShuffleMask = _mm256_set_epi64x(0x0A0B090A08090708, 0x0506040503040203,
                                                        0x0809070806070506, 0x0304020301020001);
    const auto inDatContig = _mm256_shuffle_epi8(inLaneAlign, k_byteShuffleMask);

    /// Logical shift left to set sign bit
    const __m256i k_slBits = _mm256_set_epi64x(0x0006000400020000, 0x0006000400020000,
                                               0x0006000400020000, 0x0006000400020000);
    const auto inSetSign = _mm256_sllv_epi16(inDatContig, k_slBits);

    /// Mask to zero unwanted bits
    const __m256i k_expMask = _mm256_set1_epi16(0xFFC0);
    return _mm256_and_si256(inSetSign, k_expMask);
  }


  /// Unpack compressed 12 bit data in network byte order
  /// This unpacking function is for 256b registers
  inline __m256i
  networkByteUnpack12b256(const uint8_t* inData)
  {
    /// Align chunks of compressed bytes into lanes to allow for expansion
    const __m256i* rawDataIn = reinterpret_cast<const __m256i*>(inData);
    const auto k_expPerm = _mm256_set_epi32(6, 5, 4, 3, 3, 2, 1, 0);
    const auto inLaneAlign = _mm256_permutexvar_epi32(k_expPerm, *rawDataIn);

    /// Byte shuffle to get all bits for each sample into 16b chunks
    /// For 12b mantissa all lanes post-permute are aligned and require same shuffle offset
    const __m256i k_byteShuffleMask = _mm256_set_epi64x(0x0A0B090A07080607, 0x0405030401020001,
                                                        0x0A0B090A07080607, 0x0405030401020001);
    const auto inDatContig = _mm256_shuffle_epi8(inLaneAlign, k_byteShuffleMask);

    /// Logical shift left to set sign bit
    const __m256i k_slBits = _mm256_set_epi64x(0x0004000000040000, 0x0004000000040000,
                                               0x0004000000040000, 0x0004000000040000);
    const auto inSetSign = _mm256_sllv_epi16(inDatContig, k_slBits);

    /// Mask to zero unwanted bits
    const __m256i k_expMask = _mm256_set1_epi16(0xFFF0);
    return _mm256_and_si256(inSetSign, k_expMask);
  }



  /// Pack compressed 9 bit data in network byte order
  inline __m512i
  networkBytePack9bSnc(const __m512i compData)
  {
    /// Logical shift left to align network order byte parts
    const __m512i k_shiftLeft = _mm512_set_epi64(0x0000000100020003, 0x0004000500060007,
                                                 0x0000000100020003, 0x0004000500060007,
                                                 0x0000000100020003, 0x0004000500060007,
                                                 0x0000000100020003, 0x0004000500060007);
    const auto compDataPacked = _mm512_sllv_epi16(compData, k_shiftLeft);

    /// First epi8 permute of even indexed samples
    const __m512i k_byteShuffleMask1 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000000,
                                                        0x0000000000000000, 0x00000000003C3D38,
                                                        0x3934353031002C2D, 0x282924252021001C,
                                                        0x1D18191415101100, 0x0C0D080904050001);
    constexpr uint64_t k_byteMask1 = 0x00000007FBFDFEFF;
    const auto compDataShuff1 = _mm512_maskz_permutexvar_epi8(k_byteMask1, k_byteShuffleMask1, compDataPacked);

    /// Second epi8 permute of odd indexed samples
    const __m512i k_byteShuffleMask2 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000000,
                                                        0x0000000000000000, 0x000000003E3F3A3B,
                                                        0x36373233002E2F2A, 0x2B26272223001E1F,
                                                        0x1A1B16171213000E, 0x0F0A0B0607020300);
    constexpr uint64_t k_byteMask2 = 0x0000000FF7FBFDFE;
    auto compDataShuff2 = _mm512_maskz_permutexvar_epi8(k_byteMask2, k_byteShuffleMask2, compDataPacked);

    /// Ternary blend of the two shuffled results
    const __m512i k_ternLogSelect = _mm512_set_epi64(0x0000000000000000, 0x0000000000000000,
                                                     0x0000000000000000, 0x00000000FF01FC07,
                                                     0xF01FC07F00FF01FC, 0x07F01FC07F00FF01,
                                                     0xFC07F01FC07F00FF, 0x01FC07F01FC07F00);
    return _mm512_ternarylogic_epi64(compDataShuff1, compDataShuff2, k_ternLogSelect, 0xd8);
  }


  /// Pack compressed 10 bit data in network byte order
  inline __m512i
  networkBytePack10bSnc(const __m512i compData)
  {
    /// Logical shift left to align network order byte parts
    const __m512i k_shiftLeft = _mm512_set_epi64(0x0000000200040006, 0x0000000200040006,
                                                 0x0000000200040006, 0x0000000200040006,
                                                 0x0000000200040006, 0x0000000200040006,
                                                 0x0000000200040006, 0x0000000200040006);
    const auto compDataPacked = _mm512_sllv_epi16(compData, k_shiftLeft);

    /// First epi8 shuffle of even indexed samples
    const __m512i k_byteShuffleMask1 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000000,
                                                        0x0000000000000000, 0x003C3D3839003435,
                                                        0x3031002C2D282900, 0x24252021001C1D18,
                                                        0x190014151011000C, 0x0D08090004050001);
    constexpr uint64_t k_byteMask1 = 0x0000007BDEF7BDEF;
    const auto compDataShuff1 = _mm512_maskz_permutexvar_epi8(k_byteMask1, k_byteShuffleMask1, compDataPacked);

    /// Second epi8 shuffle of odd indexed samples
    const __m512i k_byteShuffleMask2 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000000,
                                                        0x0000000000000000, 0x3E3F3A3B00363732,
                                                        0x33002E2F2A2B0026, 0x272223001E1F1A1B,
                                                        0x0016171213000E0F, 0x0A0B000607020300);
    constexpr uint64_t k_byteMask2 = 0x000000F7BDEF7BDE;
    auto compDataShuff2 = _mm512_maskz_permutexvar_epi8(k_byteMask2, k_byteShuffleMask2, compDataPacked);

    /// Ternary blend of the two shuffled results
    const __m512i k_ternLogSelect = _mm512_set_epi64(0x0000000000000000, 0x0000000000000000,
                                                     0x0000000000000000, 0xFF03F03F00FF03F0,
                                                     0x3F00FF03F03F00FF, 0x03F03F00FF03F03F,
                                                     0x00FF03F03F00FF03, 0xF03F00FF03F03F00);
    return _mm512_ternarylogic_epi64(compDataShuff1, compDataShuff2, k_ternLogSelect, 0xd8);
  }


  inline __m512i
  networkBytePack12bSnc(const __m512i compData)
  {
    /// Logical shift left to align network order byte parts
    const __m512i k_shiftLeft = _mm512_set_epi64(0x0000000400000004, 0x0000000400000004,
                                                 0x0000000400000004, 0x0000000400000004,
                                                 0x0000000400000004, 0x0000000400000004,
                                                 0x0000000400000004, 0x0000000400000004);
    const auto compDataPacked = _mm512_sllv_epi16(compData, k_shiftLeft);

    /// First epi8 shuffle of even indexed samples
    const __m512i k_byteShuffleMask1 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000000,
                                                        0x003C3D0038390034, 0x35003031002C2D00,
                                                        0x2829002425002021, 0x001C1D0018190014,
                                                        0x15001011000C0D00, 0x0809000405000001);
    constexpr uint64_t k_byteMask1 = 0x00006DB6DB6DB6DB;
    const auto compDataShuff1 = _mm512_maskz_permutexvar_epi8(k_byteMask1, k_byteShuffleMask1, compDataPacked);

    /// Second epi8 shuffle of odd indexed samples
    const __m512i k_byteShuffleMask2 = _mm512_set_epi64(0x0000000000000000, 0x0000000000000000,
                                                        0x3E3F003A3B003637, 0x003233002E2F002A,
                                                        0x2B00262700222300, 0x1E1F001A1B001617,
                                                        0x001213000E0F000A, 0x0B00060700020300);
    constexpr uint64_t k_byteMask2 = 0x0000DB6DB6DB6DB6;
    auto compDataShuff2 = _mm512_maskz_permutexvar_epi8(k_byteMask2, k_byteShuffleMask2, compDataPacked);

    /// Ternary blend of the two shuffled results
    const __m512i k_ternLogSelect = _mm512_set_epi64(0x0000000000000000, 0x0000000000000000,
                                                     0xFF0F00FF0F00FF0F, 0x00FF0F00FF0F00FF,
                                                     0x0F00FF0F00FF0F00, 0xFF0F00FF0F00FF0F,
                                                     0x00FF0F00FF0F00FF, 0x0F00FF0F00FF0F00);
    return _mm512_ternarylogic_epi64(compDataShuff1, compDataShuff2, k_ternLogSelect, 0xd8);
  }


  /// Pack compressed 9 bit data in network byte order
  /// This version is specific to the c-plane 8 antenna case, where 2 compression blocks
  /// are handled in one register.
  inline __m512i
  networkBytePack9bSncB(const __m512i compData)
  {
    /// Logical shift left to align network order byte parts
    const __m512i k_shiftLeft = _mm512_set_epi64(0x0000000100020003, 0x0004000500060007,
                                                 0x0000000100020003, 0x0004000500060007,
                                                 0x0000000100020003, 0x0004000500060007,
                                                 0x0000000100020003, 0x0004000500060007);
    const auto compDataPacked = _mm512_sllv_epi16(compData, k_shiftLeft);

    /// First epi8 permute of even indexed samples
    const __m512i k_byteShuffleMask1 = _mm512_set_epi64(0x0000000000000000, 0x000000000000003C,
                                                        0x3D38393435303100, 0x2C2D282924252021,
                                                        0x0000000000000000, 0x000000000000001C,
                                                        0x1D18191415101100, 0x0C0D080904050001);
    constexpr uint64_t k_byteMask1 = 0x0001FEFF0001FEFF;
    const auto compDataShuff1 = _mm512_maskz_permutexvar_epi8(k_byteMask1, k_byteShuffleMask1, compDataPacked);

    /// Second epi8 permute of odd indexed samples
    const __m512i k_byteShuffleMask2 = _mm512_set_epi64(0x0000000000000000, 0x0000000000003E3F,
                                                        0x3A3B36373233002E, 0x2F2A2B2627222300,
                                                        0x0000000000000000, 0x0000000000001E1F,
                                                        0x1A1B16171213000E, 0x0F0A0B0607020300);
    constexpr uint64_t k_byteMask2 = 0x0003FDFE0003FDFE;
    auto compDataShuff2 = _mm512_maskz_permutexvar_epi8(k_byteMask2, k_byteShuffleMask2, compDataPacked);

    /// Ternary blend of the two shuffled results
    const __m512i k_ternLogSelect = _mm512_set_epi64(0x0000000000000000, 0x000000000000FF01,
                                                     0xFC07F01FC07F00FF, 0x01FC07F01FC07F00,
                                                     0x0000000000000000, 0x000000000000FF01,
                                                     0xFC07F01FC07F00FF, 0x01FC07F01FC07F00);
    return _mm512_ternarylogic_epi64(compDataShuff1, compDataShuff2, k_ternLogSelect, 0xd8);
  }


  /// Pack compressed 10 bit data in network byte order
  /// This version is specific to the c-plane 8 antenna case, where 2 compression blocks
  /// are handled in one register.
  inline __m512i
  networkBytePack10bSncB(const __m512i compData)
  {
    /// Logical shift left to align network order byte parts
    const __m512i k_shiftLeft = _mm512_set_epi64(0x0000000200040006, 0x0000000200040006,
                                                 0x0000000200040006, 0x0000000200040006,
                                                 0x0000000200040006, 0x0000000200040006,
                                                 0x0000000200040006, 0x0000000200040006);
    const auto compDataPacked = _mm512_sllv_epi16(compData, k_shiftLeft);

    /// First epi8 shuffle of even indexed samples
    const __m512i k_byteShuffleMask1 = _mm512_set_epi64(0x0000000000000000, 0x00000000003C3D38,
                                                        0x390034353031002C, 0x2D28290024252021,
                                                        0x0000000000000000, 0x00000000001C1D18,
                                                        0x190014151011000C, 0x0D08090004050001);
    constexpr uint64_t k_byteMask1 = 0x0007BDEF0007BDEF;
    const auto compDataShuff1 = _mm512_maskz_permutexvar_epi8(k_byteMask1, k_byteShuffleMask1, compDataPacked);

    /// Second epi8 shuffle of odd indexed samples
    const __m512i k_byteShuffleMask2 = _mm512_set_epi64(0x0000000000000000, 0x000000003E3F3A3B,
                                                        0x0036373233002E2F, 0x2A2B002627222300,
                                                        0x0000000000000000, 0x000000001E1F1A1B,
                                                        0x0016171213000E0F, 0x0A0B000607020300);
    constexpr uint64_t k_byteMask2 = 0x000F7BDE000F7BDE;
    auto compDataShuff2 = _mm512_maskz_permutexvar_epi8(k_byteMask2, k_byteShuffleMask2, compDataPacked);

    /// Ternary blend of the two shuffled results
    const __m512i k_ternLogSelect = _mm512_set_epi64(0x0000000000000000, 0x00000000FF03F03F,
                                                     0x00FF03F03F00FF03, 0xF03F00FF03F03F00,
                                                     0x0000000000000000, 0x00000000FF03F03F,
                                                     0x00FF03F03F00FF03, 0xF03F00FF03F03F00);
    return _mm512_ternarylogic_epi64(compDataShuff1, compDataShuff2, k_ternLogSelect, 0xd8);
  }


  /// Pack compressed 12 bit data in network byte order
  /// This version is specific to the c-plane 8 antenna case, where 2 compression blocks
  /// are handled in one register.
  inline __m512i
  networkBytePack12bSncB(const __m512i compData)
  {
    /// Logical shift left to align network order byte parts
    const __m512i k_shiftLeft = _mm512_set_epi64(0x0000000400000004, 0x0000000400000004,
                                                 0x0000000400000004, 0x0000000400000004,
                                                 0x0000000400000004, 0x0000000400000004,
                                                 0x0000000400000004, 0x0000000400000004);
    const auto compDataPacked = _mm512_sllv_epi16(compData, k_shiftLeft);

    /// First epi8 shuffle of even indexed samples
    const __m512i k_byteShuffleMask1 = _mm512_set_epi64(0x0000000000000000, 0x003C3D0038390034,
                                                        0x35003031002C2D00, 0x2829002425002021,
                                                        0x0000000000000000, 0x001C1D0018190014,
                                                        0x15001011000C0D00, 0x0809000405000001);
    constexpr uint64_t k_byteMask1 = 0x006DB6DB006DB6DB;
    const auto compDataShuff1 = _mm512_maskz_permutexvar_epi8(k_byteMask1, k_byteShuffleMask1, compDataPacked);

    /// Second epi8 shuffle of odd indexed samples
    const __m512i k_byteShuffleMask2 = _mm512_set_epi64(0x0000000000000000, 0x3E3F003A3B003637,
                                                        0x003233002E2F002A, 0x2B00262700222300,
                                                        0x0000000000000000, 0x1E1F001A1B001617,
                                                        0x001213000E0F000A, 0x0B00060700020300);
    constexpr uint64_t k_byteMask2 = 0x00DB6DB600DB6DB6;
    auto compDataShuff2 = _mm512_maskz_permutexvar_epi8(k_byteMask2, k_byteShuffleMask2, compDataPacked);

    /// Ternary blend of the two shuffled results
    const __m512i k_ternLogSelect = _mm512_set_epi64(0x0000000000000000, 0xFF0F00FF0F00FF0F,
                                                     0x00FF0F00FF0F00FF, 0x0F00FF0F00FF0F00,
                                                     0x0000000000000000, 0xFF0F00FF0F00FF0F,
                                                     0x00FF0F00FF0F00FF, 0x0F00FF0F00FF0F00);
    return _mm512_ternarylogic_epi64(compDataShuff1, compDataShuff2, k_ternLogSelect, 0xd8);
  }



  /// Unpack compressed 9 bit data in network byte order
  inline __m512i
  networkByteUnpack9bSnc(const uint8_t* inData)
  {
    /// Align chunks of compressed bytes into lanes to allow for expansion
    const __m512i* rawDataIn = reinterpret_cast<const __m512i*>(inData);

    /// Byte shuffle to get all bits for each sample into 16b chunks
    /// Due to previous permute to get chunks of bytes into each lane, there is
    /// a different shuffle offset in each lane
    const __m512i k_byteShuffleMask = _mm512_set_epi64(0x2223212220211F20, 0x1E1F1D1E1C1D1B1C,
                                                       0x191A181917181617, 0x1516141513141213,
                                                       0x10110F100E0F0D0E, 0x0C0D0B0C0A0B090A,
                                                       0x0708060705060405, 0x0304020301020001);
    constexpr uint64_t k_byteMask = 0xFFFFFFFFFFFFFFFF;
    const auto inDataContig = _mm512_maskz_permutexvar_epi8(k_byteMask, k_byteShuffleMask, *rawDataIn);

    /// Logical shift left to set sign bit
    const __m512i k_slBits = _mm512_set_epi64(0x0007000600050004, 0x0003000200010000,
                                              0x0007000600050004, 0x0003000200010000,
                                              0x0007000600050004, 0x0003000200010000,
                                              0x0007000600050004, 0x0003000200010000);
    const auto inSetSign = _mm512_sllv_epi16(inDataContig, k_slBits);

    /// Mask to zero unwanted bits
    const __m512i k_expMask = _mm512_set1_epi16(0xFF80);
    return _mm512_and_epi64(inSetSign, k_expMask);
  }


  /// Unpack compressed 10 bit data in network byte order
  inline __m512i
  networkByteUnpack10bSnc(const uint8_t* inData)
  {
    /// Align chunks of compressed bytes into lanes to allow for expansion
    const __m512i* rawDataIn = reinterpret_cast<const __m512i*>(inData);

    /// Byte shuffle to get all bits for each sample into 16b chunks
    /// Due to previous permute to get chunks of bytes into each lane, lanes
    /// 0 and 2 happen to be aligned, but lane 1 is offset by 2 bytes
    const __m512i k_byteShuffleMask = _mm512_set_epi64(0x2627252624252324, 0x212220211F201E1F,
                                                       0x1C1D1B1C1A1B191A, 0x1718161715161415,
                                                       0x1213111210110F10, 0x0D0E0C0D0B0C0A0B,
                                                       0x0809070806070506, 0x0304020301020001);
    constexpr uint64_t k_byteMask = 0xFFFFFFFFFFFFFFFF;
    const auto inDataContig = _mm512_maskz_permutexvar_epi8(k_byteMask, k_byteShuffleMask, *rawDataIn);

    /// Logical shift left to set sign bit
    const __m512i k_slBits = _mm512_set_epi64(0x0006000400020000, 0x0006000400020000,
                                              0x0006000400020000, 0x0006000400020000,
                                              0x0006000400020000, 0x0006000400020000,
                                              0x0006000400020000, 0x0006000400020000);
    const auto inSetSign = _mm512_sllv_epi16(inDataContig, k_slBits);

    /// Mask to zero unwanted bits
    const __m512i k_expMask = _mm512_set1_epi16(0xFFC0);
    return _mm512_and_epi64(inSetSign, k_expMask);
  }


  /// Unpack compressed 12 bit data in network byte order
  inline __m512i
  networkByteUnpack12bSnc(const uint8_t* inData)
  {
    /// Align chunks of compressed bytes into lanes to allow for expansion
    const __m512i* rawDataIn = reinterpret_cast<const __m512i*>(inData);

    /// Byte shuffle to get all bits for each sample into 16b chunks
    /// For 12b mantissa all lanes post-permute are aligned and require same shuffle offset
    const __m512i k_byteShuffleMask = _mm512_set_epi64(0x2E2F2D2E2B2C2A2B, 0x2829272825262425,
                                                       0x222321221F201E1F, 0x1C1D1B1C191A1819,
                                                       0x1617151613141213, 0x10110F100D0E0C0D,
                                                       0x0A0B090A07080607, 0x0405030401020001);
    constexpr uint64_t k_byteMask = 0xFFFFFFFFFFFFFFFF;
    const auto inDataContig = _mm512_maskz_permutexvar_epi8(k_byteMask, k_byteShuffleMask, *rawDataIn);

    /// Logical shift left to set sign bit
    const __m512i k_slBits = _mm512_set_epi64(0x0004000000040000, 0x0004000000040000,
                                              0x0004000000040000, 0x0004000000040000,
                                              0x0004000000040000, 0x0004000000040000,
                                              0x0004000000040000, 0x0004000000040000);
    const auto inSetSign = _mm512_sllv_epi16(inDataContig, k_slBits);

    /// Mask to zero unwanted bits
    const __m512i k_expMask = _mm512_set1_epi16(0xFFF0);
    return _mm512_and_epi64(inSetSign, k_expMask);
  }


  /// Unpack compressed 9 bit data in network byte order
  /// This unpacking function is for 256b registers
  inline __m256i
  networkByteUnpack9b256Snc(const uint8_t* inData)
  {
    /// Align chunks of compressed bytes into lanes to allow for expansion
    const __m256i* rawDataIn = reinterpret_cast<const __m256i*>(inData);

    /// Byte shuffle to get all bits for each sample into 16b chunks
    /// Due to previous permute to get chunks of bytes into each lane, there is
    /// a different shuffle offset in each lane
    const __m256i k_byteShuffleMask = _mm256_set_epi64x(0x10110F100E0F0D0E, 0x0C0D0B0C0A0B090A,
                                                        0x0708060705060405, 0x0304020301020001);
    constexpr uint32_t k_byteMask = 0xFFFFFFFF;
    const auto inDataContig = _mm256_maskz_permutexvar_epi8(k_byteMask, k_byteShuffleMask, *rawDataIn);

    /// Logical shift left to set sign bit
    const __m256i k_slBits = _mm256_set_epi64x(0x0007000600050004, 0x0003000200010000,
                                               0x0007000600050004, 0x0003000200010000);
    const auto inSetSign = _mm256_sllv_epi16(inDataContig, k_slBits);

    /// Mask to zero unwanted bits
    const __m256i k_expMask = _mm256_set1_epi16(0xFF80);
    return _mm256_and_si256(inSetSign, k_expMask);
  }


  /// Unpack compressed 10 bit data in network byte order
  /// This unpacking function is for 256b registers
  inline __m256i
  networkByteUnpack10b256Snc(const uint8_t* inData)
  {
    /// Align chunks of compressed bytes into lanes to allow for expansion
    const __m256i* rawDataIn = reinterpret_cast<const __m256i*>(inData);

    /// Byte shuffle to get all bits for each sample into 16b chunks
    /// Due to previous permute to get chunks of bytes into each lane, lanes
    /// 0 and 2 happen to be aligned, but lane 1 is offset by 2 bytes
    const __m256i k_byteShuffleMask = _mm256_set_epi64x(0x1213111210110F10, 0x0D0E0C0D0B0C0A0B,
                                                        0x0809070806070506, 0x0304020301020001);
    constexpr uint32_t k_byteMask = 0xFFFFFFFF;
    const auto inDataContig = _mm256_maskz_permutexvar_epi8(k_byteMask, k_byteShuffleMask, *rawDataIn);

    /// Logical shift left to set sign bit
    const __m256i k_slBits = _mm256_set_epi64x(0x0006000400020000, 0x0006000400020000,
                                               0x0006000400020000, 0x0006000400020000);
    const auto inSetSign = _mm256_sllv_epi16(inDataContig, k_slBits);

    /// Mask to zero unwanted bits
    const __m256i k_expMask = _mm256_set1_epi16(0xFFC0);
    return _mm256_and_si256(inSetSign, k_expMask);
  }


  /// Unpack compressed 12 bit data in network byte order
  /// This unpacking function is for 256b registers
  inline __m256i
  networkByteUnpack12b256Snc(const uint8_t* inData)
  {
    /// Align chunks of compressed bytes into lanes to allow for expansion
    const __m256i* rawDataIn = reinterpret_cast<const __m256i*>(inData);

    /// Byte shuffle to get all bits for each sample into 16b chunks
    /// For 12b mantissa all lanes post-permute are aligned and require same shuffle offset
    const __m256i k_byteShuffleMask = _mm256_set_epi64x(0x1617151613141213, 0x10110F100D0E0C0D,
                                                        0x0A0B090A07080607, 0x0405030401020001);
    constexpr uint32_t k_byteMask = 0xFFFFFFFF;
    const auto inDataContig = _mm256_maskz_permutexvar_epi8(k_byteMask, k_byteShuffleMask, *rawDataIn);

    /// Logical shift left to set sign bit
    const __m256i k_slBits = _mm256_set_epi64x(0x0004000000040000, 0x0004000000040000,
                                               0x0004000000040000, 0x0004000000040000);
    const auto inSetSign = _mm256_sllv_epi16(inDataContig, k_slBits);

    /// Mask to zero unwanted bits
    const __m256i k_expMask = _mm256_set1_epi16(0xFFF0);
    return _mm256_and_si256(inSetSign, k_expMask);
  }

}
