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

#include "xran_compression.hpp"
#include "xran_compression.h"
#include <complex>
#include <algorithm>
#include <immintrin.h>
#include <limits.h>
#include <cstring>

static int16_t saturateAbs(int16_t inVal)
{
  int16_t result;
  if (inVal == std::numeric_limits<short>::min())
  {
    result = std::numeric_limits<short>::max();
  }
  else
  {
    result = (int16_t)std::abs(inVal);
  }
  return result;
}


/// Compute exponent value for a set of RB from the maximum absolute value
void
computeExponent(const BlockFloatCompander::ExpandedData& dataIn, int8_t* expStore)
{
  __m512i maxAbs = __m512i();

  /// Load data and find max(abs(RB))
  const __m512i* rawData = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
  constexpr int k_numRBPerLoop = 4;
  constexpr int k_numInputLoopIts = BlockFloatCompander::k_numRB / k_numRBPerLoop;

#pragma unroll(k_numInputLoopIts)
  for (int n = 0; n < k_numInputLoopIts; ++n)
  {
    /// Re-order the next 4RB in input data into 3 registers
    /// Input SIMD vectors are:
    ///   [A A A A A A A A A A A A B B B B]
    ///   [B B B B B B B B C C C C C C C C]
    ///   [C C C C D D D D D D D D D D D D]
    /// Re-ordered SIMD vectors are:
    ///   [A A A A B B B B C C C C D D D D]
    ///   [A A A A B B B B C C C C D D D D]
    ///   [A A A A B B B B C C C C D D D D]
    constexpr uint8_t k_msk1 = 0b11111100; // Copy first lane of src
    constexpr int k_shuff1 = 0x41;
    const auto z_w1 = _mm512_mask_shuffle_i64x2(rawData[3 * n + 0], k_msk1, rawData[3 * n + 1], rawData[3 * n + 2], k_shuff1);

    constexpr uint8_t k_msk2 = 0b11000011; // Copy middle two lanes of src
    constexpr int k_shuff2 = 0xB1;
    const auto z_w2 = _mm512_mask_shuffle_i64x2(rawData[3 * n + 1], k_msk2, rawData[3 * n + 0], rawData[3 * n + 2], k_shuff2);

    constexpr uint8_t k_msk3 = 0b00111111; // Copy last lane of src
    constexpr int k_shuff3 = 0xBE;
    const auto z_w3 = _mm512_mask_shuffle_i64x2(rawData[3 * n + 2], k_msk3, rawData[3 * n + 0], rawData[3 * n + 1], k_shuff3);

    /// Perform max abs on these 3 registers
    const auto abs16_1 = _mm512_abs_epi16(z_w1);
    const auto abs16_2 = _mm512_abs_epi16(z_w2);
    const auto abs16_3 = _mm512_abs_epi16(z_w3);
    const auto maxAbs_12 = _mm512_max_epi16(abs16_1, abs16_2);
    const auto maxAbs_123 = _mm512_max_epi16(maxAbs_12, abs16_3);

    /// Perform horizontal max over each lane
    /// Swap 64b in each lane and compute max
    const auto k_perm64b = _mm512_set_epi64(6, 7, 4, 5, 2, 3, 0, 1);
    auto maxAbsPerm = _mm512_permutexvar_epi64(k_perm64b, maxAbs_123);
    auto maxAbsHorz = _mm512_max_epi16(maxAbs_123, maxAbsPerm);

    /// Swap each pair of 32b in each lane and compute max
    const auto k_perm32b = _mm512_set_epi32(14, 15, 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1);
    maxAbsPerm = _mm512_permutexvar_epi32(k_perm32b, maxAbsHorz);
    maxAbsHorz = _mm512_max_epi16(maxAbsHorz, maxAbsPerm);

    /// Swap each IQ pair in each lane (via 32b rotation) and compute max
    maxAbsPerm = _mm512_rol_epi32(maxAbsHorz, BlockFloatCompander::k_numBitsIQ);
    maxAbsHorz = _mm512_max_epi16(maxAbsHorz, maxAbsPerm);

    /// Insert values into maxAbs
    /// Use sliding mask to insert wanted values into maxAbs
    /// Pairs of values will be inserted and corrected outside of loop
    const auto k_select4RB = _mm512_set_epi32(28, 24, 20, 16, 28, 24, 20, 16,
                                              28, 24, 20, 16, 28, 24, 20, 16);
    constexpr uint16_t k_expMsk[k_numInputLoopIts] = { 0x000F, 0x00F0, 0x0F00, 0xF000 };
    maxAbs = _mm512_mask_permutex2var_epi32(maxAbs, k_expMsk[n], k_select4RB, maxAbsHorz);
  }

  /// Convert to 32b by removing repeated values in maxAbs
  const auto k_upperWordMask = _mm512_set_epi64(0x0000FFFF0000FFFF, 0x0000FFFF0000FFFF,
                                                0x0000FFFF0000FFFF, 0x0000FFFF0000FFFF,
                                                0x0000FFFF0000FFFF, 0x0000FFFF0000FFFF,
                                                0x0000FFFF0000FFFF, 0x0000FFFF0000FFFF);
  maxAbs = _mm512_and_epi64(maxAbs, k_upperWordMask);

  /// Compute and store exponent
  const auto totShiftBits = _mm512_set1_epi32(32 - dataIn.iqWidth + 1);
  const auto lzCount = _mm512_lzcnt_epi32(maxAbs);
  const auto exponent = _mm512_sub_epi32(totShiftBits, lzCount);
  constexpr uint16_t k_expWriteMask = 0xFFFF;
  _mm512_mask_cvtepi32_storeu_epi8(expStore, k_expWriteMask, exponent);
}


/// Pack compressed 9 bit data in network byte order
/// See https://soco.intel.com/docs/DOC-2665619
__m512i
networkBytePack9b(const __m512i compData)
{
  /// Logical shift left to align network order byte parts
  const __m512i k_shiftLeft = _mm512_set_epi64(0x0000000100020003, 0x0004000500060007,
                                               0x0000000100020003, 0x0004000500060007,
                                               0x0000000100020003, 0x0004000500060007,
                                               0x0000000100020003, 0x0004000500060007);
  auto compDataPacked = _mm512_sllv_epi16(compData, k_shiftLeft);

  /// First epi8 shuffle of even indexed samples
  const __m512i k_byteShuffleMask1 = _mm512_set_epi64(0x0000000000000000, 0x0C0D080904050001,
                                                      0x0000000000000000, 0x0C0D080904050001,
                                                      0x0000000000000000, 0x0C0D080904050001,
                                                      0x0000000000000000, 0x0C0D080904050001);
  constexpr uint64_t k_byteMask1 = 0x000000FF00FF00FF;
  auto compDataShuff1 = _mm512_maskz_shuffle_epi8(k_byteMask1, compDataPacked, k_byteShuffleMask1);

  /// Second epi8 shuffle of odd indexed samples
  const __m512i k_byteShuffleMask2 = _mm512_set_epi64(0x000000000000000E, 0x0F0A0B0607020300,
                                                      0x000000000000000E, 0x0F0A0B0607020300,
                                                      0x000000000000000E, 0x0F0A0B0607020300,
                                                      0x000000000000000E, 0x0F0A0B0607020300);
  constexpr uint64_t k_byteMask2 = 0x000001FE01FE01FE;
  auto compDataShuff2 = _mm512_maskz_shuffle_epi8(k_byteMask2, compDataPacked, k_byteShuffleMask2);

  /// Ternary blend of the two shuffled results
  const __m512i k_ternLogSelect = _mm512_set_epi64(0x00000000000000FF, 0x01FC07F01FC07F00,
                                                   0x00000000000000FF, 0x01FC07F01FC07F00,
                                                   0x00000000000000FF, 0x01FC07F01FC07F00,
                                                   0x00000000000000FF, 0x01FC07F01FC07F00);
  return _mm512_ternarylogic_epi64(compDataShuff1, compDataShuff2, k_ternLogSelect, 0xd8);
}


/// Pack compressed 10 bit data in network byte order
/// See https://soco.intel.com/docs/DOC-2665619
__m512i
networkBytePack10b(const __m512i compData)
{
  /// Logical shift left to align network order byte parts
  const __m512i k_shiftLeft = _mm512_set_epi64(0x0000000200040006, 0x0000000200040006,
                                               0x0000000200040006, 0x0000000200040006,
                                               0x0000000200040006, 0x0000000200040006,
                                               0x0000000200040006, 0x0000000200040006);
  auto compDataPacked = _mm512_sllv_epi16(compData, k_shiftLeft);

  /// First epi8 shuffle of even indexed samples
  const __m512i k_byteShuffleMask1 = _mm512_set_epi64(0x000000000000000C, 0x0D08090004050001,
                                                      0x000000000000000C, 0x0D08090004050001,
                                                      0x000000000000000C, 0x0D08090004050001,
                                                      0x000000000000000C, 0x0D08090004050001);
  constexpr uint64_t k_byteMask1 = 0x000001EF01EF01EF;
  auto compDataShuff1 = _mm512_maskz_shuffle_epi8(k_byteMask1, compDataPacked, k_byteShuffleMask1);

  /// Second epi8 shuffle of odd indexed samples
  const __m512i k_byteShuffleMask2 = _mm512_set_epi64(0x0000000000000E0F, 0x0A0B000607020300,
                                                      0x0000000000000E0F, 0x0A0B000607020300,
                                                      0x0000000000000E0F, 0x0A0B000607020300,
                                                      0x0000000000000E0F, 0x0A0B000607020300);
  constexpr uint64_t k_byteMask2 = 0x000003DE03DE03DE;
  auto compDataShuff2 = _mm512_maskz_shuffle_epi8(k_byteMask2, compDataPacked, k_byteShuffleMask2);

  /// Ternary blend of the two shuffled results
  const __m512i k_ternLogSelect = _mm512_set_epi64(0x000000000000FF03, 0xF03F00FF03F03F00,
                                                   0x000000000000FF03, 0xF03F00FF03F03F00,
                                                   0x000000000000FF03, 0xF03F00FF03F03F00,
                                                   0x000000000000FF03, 0xF03F00FF03F03F00);
  return _mm512_ternarylogic_epi64(compDataShuff1, compDataShuff2, k_ternLogSelect, 0xd8);
}


/// Pack compressed 12 bit data in network byte order
/// See https://soco.intel.com/docs/DOC-2665619
__m512i
networkBytePack12b(const __m512i compData)
{
  /// Logical shift left to align network order byte parts
  const __m512i k_shiftLeft = _mm512_set_epi64(0x0000000400000004, 0x0000000400000004,
                                               0x0000000400000004, 0x0000000400000004,
                                               0x0000000400000004, 0x0000000400000004,
                                               0x0000000400000004, 0x0000000400000004);
  auto compDataPacked = _mm512_sllv_epi16(compData, k_shiftLeft);

  /// First epi8 shuffle of even indexed samples
  const __m512i k_byteShuffleMask1 = _mm512_set_epi64(0x00000000000C0D00, 0x0809000405000001,
                                                      0x00000000000C0D00, 0x0809000405000001,
                                                      0x00000000000C0D00, 0x0809000405000001,
                                                      0x00000000000C0D00, 0x0809000405000001);
  constexpr uint64_t k_byteMask1 = 0x000006DB06DB06DB;
  auto compDataShuff1 = _mm512_maskz_shuffle_epi8(k_byteMask1, compDataPacked, k_byteShuffleMask1);

  /// Second epi8 shuffle of odd indexed samples
  const __m512i k_byteShuffleMask2 = _mm512_set_epi64(0x000000000E0F000A, 0x0B00060700020300,
                                                      0x000000000E0F000A, 0x0B00060700020300,
                                                      0x000000000E0F000A, 0x0B00060700020300,
                                                      0x000000000E0F000A, 0x0B00060700020300);
  constexpr uint64_t k_byteMask2 = 0x00000DB60DB60DB6;
  auto compDataShuff2 = _mm512_maskz_shuffle_epi8(k_byteMask2, compDataPacked, k_byteShuffleMask2);

  /// Ternary blend of the two shuffled results
  const __m512i k_ternLogSelect = _mm512_set_epi64(0x00000000FF0F00FF, 0x0F00FF0F00FF0F00,
                                                   0x00000000FF0F00FF, 0x0F00FF0F00FF0F00,
                                                   0x00000000FF0F00FF, 0x0F00FF0F00FF0F00,
                                                   0x00000000FF0F00FF, 0x0F00FF0F00FF0F00);
  return _mm512_ternarylogic_epi64(compDataShuff1, compDataShuff2, k_ternLogSelect, 0xd8);
}


/// Unpack compressed 9 bit data in network byte order
/// See https://soco.intel.com/docs/DOC-2665619
__m512i
networkByteUnpack9b(const uint8_t* inData)
{
  /// Align chunks of compressed bytes into lanes to allow for expansion
  const __m512i* rawDataIn = reinterpret_cast<const __m512i*>(inData);
  const auto k_expPerm = _mm512_set_epi32(15, 14, 13, 12,  7,  6,  5,  4,
                                           5,  4,  3,  2,  3,  2,  1,  0);
  auto expData = _mm512_permutexvar_epi32(k_expPerm, *rawDataIn);

  /// Byte shuffle to get all bits for each sample into 16b chunks
  /// Due to previous permute to get chunks of bytes into each lane, there is
  /// a different shuffle offset in each lane
  const __m512i k_byteShuffleMask = _mm512_set_epi64(0x0F0E0D0C0B0A0908, 0x0706050403020100,
                                                     0x090A080907080607, 0x0506040503040203,
                                                     0x0809070806070506, 0x0405030402030102,
                                                     0x0708060705060405, 0x0304020301020001);
  expData = _mm512_shuffle_epi8(expData, k_byteShuffleMask);

  /// Logical shift left to set sign bit
  const __m512i k_slBits = _mm512_set_epi64(0x0007000600050004, 0x0003000200010000,
                                            0x0007000600050004, 0x0003000200010000,
                                            0x0007000600050004, 0x0003000200010000,
                                            0x0007000600050004, 0x0003000200010000);
  expData = _mm512_sllv_epi16(expData, k_slBits);

  /// Mask to zero unwanted bits
  const __m512i k_expMask = _mm512_set1_epi16(0xFF80);
  return _mm512_and_epi64(expData, k_expMask);
}


/// Unpack compressed 10 bit data in network byte order
/// See https://soco.intel.com/docs/DOC-2665619
__m512i
networkByteUnpack10b(const uint8_t* inData)
{
  /// Align chunks of compressed bytes into lanes to allow for expansion
  const __m512i* rawDataIn = reinterpret_cast<const __m512i*>(inData);
  const auto k_expPerm = _mm512_set_epi32(15, 14, 13, 12,  8,  7,  6,  5,
                                           5,  4,  3,  2,  3,  2,  1,  0);
  auto expData = _mm512_permutexvar_epi32(k_expPerm, *rawDataIn);

  /// Byte shuffle to get all bits for each sample into 16b chunks
  /// Due to previous permute to get chunks of bytes into each lane, lanes
  /// 0 and 2 happen to be aligned, but lane 1 is offset by 2 bytes
  const __m512i k_byteShuffleMask = _mm512_set_epi64(0x0809070806070506, 0x0304020301020001,
                                                     0x0809070806070506, 0x0304020301020001,
                                                     0x0A0B090A08090708, 0x0506040503040203,
                                                     0x0809070806070506, 0x0304020301020001);
  expData = _mm512_shuffle_epi8(expData, k_byteShuffleMask);

  /// Logical shift left to set sign bit
  const __m512i k_slBits = _mm512_set_epi64(0x0006000400020000, 0x0006000400020000,
                                            0x0006000400020000, 0x0006000400020000,
                                            0x0006000400020000, 0x0006000400020000,
                                            0x0006000400020000, 0x0006000400020000);
  expData = _mm512_sllv_epi16(expData, k_slBits);

  /// Mask to zero unwanted bits
  const __m512i k_expMask = _mm512_set1_epi16(0xFFC0);
  return _mm512_and_epi64(expData, k_expMask);
}


/// Unpack compressed 12 bit data in network byte order
/// See https://soco.intel.com/docs/DOC-2665619
__m512i
networkByteUnpack12b(const uint8_t* inData)
{
  /// Align chunks of compressed bytes into lanes to allow for expansion
  const __m512i* rawDataIn = reinterpret_cast<const __m512i*>(inData);
  const auto k_expPerm = _mm512_set_epi32(15, 14, 13, 12,  9,  8,  7,  6,
                                           6,  5,  4,  3,  3,  2,  1,  0);
  auto expData = _mm512_permutexvar_epi32(k_expPerm, *rawDataIn);

  /// Byte shuffle to get all bits for each sample into 16b chunks
  /// For 12b mantissa all lanes post-permute are aligned and require same shuffle offset
  const __m512i k_byteShuffleMask = _mm512_set_epi64(0x0A0B090A07080607, 0x0405030401020001,
                                                     0x0A0B090A07080607, 0x0405030401020001,
                                                     0x0A0B090A07080607, 0x0405030401020001,
                                                     0x0A0B090A07080607, 0x0405030401020001);
  expData = _mm512_shuffle_epi8(expData, k_byteShuffleMask);

  /// Logical shift left to set sign bit
  const __m512i k_slBits = _mm512_set_epi64(0x0004000000040000, 0x0004000000040000,
                                            0x0004000000040000, 0x0004000000040000,
                                            0x0004000000040000, 0x0004000000040000,
                                            0x0004000000040000, 0x0004000000040000);
  expData = _mm512_sllv_epi16(expData, k_slBits);

  /// Mask to zero unwanted bits
  const __m512i k_expMask = _mm512_set1_epi16(0xFFF0);
  return _mm512_and_epi64(expData, k_expMask);
}


/// 8 bit compression
void
BlockFloatCompander::BlockFloatCompress_8b_AVX512(const ExpandedData& dataIn, CompressedData* dataOut)
{
  /// Compute exponent and store for later use
  int8_t storedExp[BlockFloatCompander::k_numRB] = {};
  computeExponent(dataIn, storedExp);

  /// Shift 1RB by corresponding exponent and write exponent and data to output
#pragma unroll(BlockFloatCompander::k_numRB)
  for (int n = 0; n < BlockFloatCompander::k_numRB; ++n)
  {
    const __m512i* rawDataIn = reinterpret_cast<const __m512i*>(dataIn.dataExpanded + n * BlockFloatCompander::k_numREReal);
    auto compData = _mm512_srai_epi16(*rawDataIn, storedExp[n]);
    auto thisRBExpAddr = n * (BlockFloatCompander::k_numREReal + 1);
    /// Store exponent first
    dataOut->dataCompressed[thisRBExpAddr] = storedExp[n];
    /// Store compressed RB
    constexpr uint32_t k_rbMask = 0x00FFFFFF; // Write mask for 1RB (24 values)
    _mm256_mask_storeu_epi8(dataOut->dataCompressed + thisRBExpAddr + 1, k_rbMask, _mm512_cvtepi16_epi8(compData));
  }
}


/// 9 bit compression
void
BlockFloatCompander::BlockFloatCompress_9b_AVX512(const ExpandedData& dataIn, CompressedData* dataOut)
{
  /// Compute exponent and store for later use
  int8_t storedExp[BlockFloatCompander::k_numRB] = {};
  computeExponent(dataIn, storedExp);

  /// Shift 1RB by corresponding exponent and write exponent and data to output
  /// Output data is packed exponent first followed by corresponding compressed RB
#pragma unroll(BlockFloatCompander::k_numRB)
  for (int n = 0; n < BlockFloatCompander::k_numRB; ++n)
  {
    /// Apply exponent shift
    const __m512i* rawDataIn = reinterpret_cast<const __m512i*>(dataIn.dataExpanded + n * BlockFloatCompander::k_numREReal);
    auto compData = _mm512_srai_epi16(*rawDataIn, storedExp[n]);

    /// Pack compressed data network byte order
    auto compDataBytePacked = networkBytePack9b(compData);

    /// Store exponent first
    constexpr int k_totNumBytesPerRB = 28;
    auto thisRBExpAddr = n * k_totNumBytesPerRB;
    dataOut->dataCompressed[thisRBExpAddr] = storedExp[n];

    /// Now have 1 RB worth of bytes separated into 3 chunks (1 per lane)
    /// Use three offset stores to join
    constexpr uint16_t k_RbWriteMask = 0x01FF;
    constexpr int k_numDataBytesPerLane = 9;
    _mm_mask_storeu_epi8(dataOut->dataCompressed + thisRBExpAddr + 1, k_RbWriteMask, _mm512_extracti64x2_epi64(compDataBytePacked, 0));
    _mm_mask_storeu_epi8(dataOut->dataCompressed + thisRBExpAddr + 1 + k_numDataBytesPerLane, k_RbWriteMask, _mm512_extracti64x2_epi64(compDataBytePacked, 1));
    _mm_mask_storeu_epi8(dataOut->dataCompressed + thisRBExpAddr + 1 + (2 * k_numDataBytesPerLane), k_RbWriteMask, _mm512_extracti64x2_epi64(compDataBytePacked, 2));
  }
}


/// 10 bit compression
void
BlockFloatCompander::BlockFloatCompress_10b_AVX512(const ExpandedData& dataIn, CompressedData* dataOut)
{
  /// Compute exponent and store for later use
  int8_t storedExp[BlockFloatCompander::k_numRB] = {};
  computeExponent(dataIn, storedExp);

  /// Shift 1RB by corresponding exponent and write exponent and data to output
  /// Output data is packed exponent first followed by corresponding compressed RB
#pragma unroll(BlockFloatCompander::k_numRB)
  for (int n = 0; n < BlockFloatCompander::k_numRB; ++n)
  {
    /// Apply exponent shift
    const __m512i* rawDataIn = reinterpret_cast<const __m512i*>(dataIn.dataExpanded + n * BlockFloatCompander::k_numREReal);
    auto compData = _mm512_srai_epi16(*rawDataIn, storedExp[n]);

    /// Pack compressed data network byte order
    auto compDataBytePacked = networkBytePack10b(compData);

    /// Store exponent first
    constexpr int k_totNumBytesPerRB = 31;
    auto thisRBExpAddr = n * k_totNumBytesPerRB;
    dataOut->dataCompressed[thisRBExpAddr] = storedExp[n];

    /// Now have 1 RB worth of bytes separated into 3 chunks (1 per lane)
    /// Use three offset stores to join
    constexpr uint16_t k_RbWriteMask = 0x03FF;
    constexpr int k_numDataBytesPerLane = 10;
    _mm_mask_storeu_epi8(dataOut->dataCompressed + thisRBExpAddr + 1, k_RbWriteMask, _mm512_extracti64x2_epi64(compDataBytePacked, 0));
    _mm_mask_storeu_epi8(dataOut->dataCompressed + thisRBExpAddr + 1 + k_numDataBytesPerLane, k_RbWriteMask, _mm512_extracti64x2_epi64(compDataBytePacked, 1));
    _mm_mask_storeu_epi8(dataOut->dataCompressed + thisRBExpAddr + 1 + (2 * k_numDataBytesPerLane), k_RbWriteMask, _mm512_extracti64x2_epi64(compDataBytePacked, 2));
  }
}


/// 12 bit compression
void
BlockFloatCompander::BlockFloatCompress_12b_AVX512(const ExpandedData& dataIn, CompressedData* dataOut)
{
  /// Compute exponent and store for later use
  int8_t storedExp[BlockFloatCompander::k_numRB] = {};
  computeExponent(dataIn, storedExp);

  /// Shift 1RB by corresponding exponent and write exponent and data to output
  /// Output data is packed exponent first followed by corresponding compressed RB
#pragma unroll(BlockFloatCompander::k_numRB)
  for (int n = 0; n < BlockFloatCompander::k_numRB; ++n)
  {
    /// Apply exponent shift
    const __m512i* rawDataIn = reinterpret_cast<const __m512i*>(dataIn.dataExpanded + n * BlockFloatCompander::k_numREReal);
    auto compData = _mm512_srai_epi16(*rawDataIn, storedExp[n]);

    /// Pack compressed data network byte order
    auto compDataBytePacked = networkBytePack12b(compData);

    /// Store exponent first
    constexpr int k_totNumBytesPerRB = 37;
    auto thisRBExpAddr = n * k_totNumBytesPerRB;
    dataOut->dataCompressed[thisRBExpAddr] = storedExp[n];

    /// Now have 1 RB worth of bytes separated into 3 chunks (1 per lane)
    /// Use three offset stores to join
    constexpr uint16_t k_RbWriteMask = 0x0FFF;
    constexpr int k_numDataBytesPerLane = 12;
    _mm_mask_storeu_epi8(dataOut->dataCompressed + thisRBExpAddr + 1, k_RbWriteMask, _mm512_extracti64x2_epi64(compDataBytePacked, 0));
    _mm_mask_storeu_epi8(dataOut->dataCompressed + thisRBExpAddr + 1 + k_numDataBytesPerLane, k_RbWriteMask, _mm512_extracti64x2_epi64(compDataBytePacked, 1));
    _mm_mask_storeu_epi8(dataOut->dataCompressed + thisRBExpAddr + 1 + (2 * k_numDataBytesPerLane), k_RbWriteMask, _mm512_extracti64x2_epi64(compDataBytePacked, 2));
  }
}


/// 8 bit expansion
void
BlockFloatCompander::BlockFloatExpand_8b_AVX512(const CompressedData& dataIn, ExpandedData* dataOut)
{
#pragma unroll(BlockFloatCompander::k_numRB)
  for (int n = 0; n < BlockFloatCompander::k_numRB; ++n)
  {
    /// Expand 1RB of data
    auto expAddr = n * (BlockFloatCompander::k_numREReal + 1);
    const __m256i* rawDataIn = reinterpret_cast<const __m256i*>(dataIn.dataCompressed + expAddr + 1);
    const auto compData16 = _mm512_cvtepi8_epi16(*rawDataIn);
    const auto expData = _mm512_slli_epi16(compData16, *(dataIn.dataCompressed + expAddr));
    /// Write expanded data to output
    constexpr uint8_t k_rbMask64 = 0b00111111; // 64b write mask for 1RB (24 int16 values)
    _mm512_mask_storeu_epi64(dataOut->dataExpanded + n * BlockFloatCompander::k_numREReal, k_rbMask64, expData);
  }
}


/// 9 bit expansion
void
BlockFloatCompander::BlockFloatExpand_9b_AVX512(const CompressedData& dataIn, ExpandedData* dataOut)
{
#pragma unroll(BlockFloatCompander::k_numRB)
  for (int n = 0; n < BlockFloatCompander::k_numRB; ++n)
  {
    constexpr int k_totNumBytesPerRB = 28;
    auto expAddr = n * k_totNumBytesPerRB;

    /// Unpack network order packed data
    auto expData = networkByteUnpack9b(dataIn.dataCompressed + expAddr + 1);

    /// Apply exponent scaling (by appropriate arithmetic shift right)
    constexpr int k_maxExpShift = 7;
    expData = _mm512_srai_epi16(expData, k_maxExpShift - *(dataIn.dataCompressed + expAddr));

    /// Write expanded data to output
    static constexpr uint32_t k_WriteMask = 0x00FFFFFF;
    _mm512_mask_storeu_epi16(dataOut->dataExpanded + n * BlockFloatCompander::k_numREReal, k_WriteMask, expData);
  }
}


/// 10 bit expansion
void
BlockFloatCompander::BlockFloatExpand_10b_AVX512(const CompressedData& dataIn, ExpandedData* dataOut)
{
#pragma unroll(BlockFloatCompander::k_numRB)
  for (int n = 0; n < BlockFloatCompander::k_numRB; ++n)
  {
    constexpr int k_totNumBytesPerRB = 31;
    auto expAddr = n * k_totNumBytesPerRB;

    /// Unpack network order packed data
    auto expData = networkByteUnpack10b(dataIn.dataCompressed + expAddr + 1);

    /// Apply exponent scaling (by appropriate arithmetic shift right)
    constexpr int k_maxExpShift = 6;
    expData = _mm512_srai_epi16(expData, k_maxExpShift - *(dataIn.dataCompressed + expAddr));

    /// Write expanded data to output
    static constexpr uint32_t k_WriteMask = 0x00FFFFFF;
    _mm512_mask_storeu_epi16(dataOut->dataExpanded + n * BlockFloatCompander::k_numREReal, k_WriteMask, expData);
  }
}


/// 12 bit expansion
void
BlockFloatCompander::BlockFloatExpand_12b_AVX512(const CompressedData& dataIn, ExpandedData* dataOut)
{
#pragma unroll(BlockFloatCompander::k_numRB)
  for (int n = 0; n < BlockFloatCompander::k_numRB; ++n)
  {
    constexpr int k_totNumBytesPerRB = 37;
    auto expAddr = n * k_totNumBytesPerRB;

    /// Unpack network order packed data
    auto expData = networkByteUnpack12b(dataIn.dataCompressed + expAddr + 1);

    /// Apply exponent scaling (by appropriate arithmetic shift right)
    constexpr int k_maxExpShift = 4;
    expData = _mm512_srai_epi16(expData, k_maxExpShift - *(dataIn.dataCompressed + expAddr));

    /// Write expanded data to output
    static constexpr uint32_t k_WriteMask = 0x00FFFFFF;
    _mm512_mask_storeu_epi16(dataOut->dataExpanded + n * BlockFloatCompander::k_numREReal, k_WriteMask, expData);
  }
}


/// Reference compression
void
BlockFloatCompander::BlockFloatCompress_Basic(const ExpandedData& dataIn, CompressedData* dataOut)
{
  int dataOutIdx = 0;
  int16_t iqMask = (int16_t)((1 << dataIn.iqWidth) - 1);
  int byteShiftUnits = dataIn.iqWidth - 8;

  for (int rb = 0; rb < BlockFloatCompander::k_numRB; ++rb)
  {
    /// Find max abs value for this RB
    int16_t maxAbs = 0;
    for (int re = 0; re < BlockFloatCompander::k_numREReal; ++re)
    {
      auto dataIdx = rb * BlockFloatCompander::k_numREReal + re;
      auto dataAbs = saturateAbs(dataIn.dataExpanded[dataIdx]);
      maxAbs = std::max(maxAbs, dataAbs);
    }

    // Find exponent and insert into byte stream
    auto thisExp = (uint8_t)(std::max(0,(16 - dataIn.iqWidth + 1 - __lzcnt16(maxAbs))));
    dataOut->dataCompressed[dataOutIdx++] = thisExp;

    /// ARS data by exponent and pack bytes in Network order
    /// This uses a sliding buffer where one or more bytes are
    /// extracted after the insertion of each compressed sample
    static constexpr int k_byteMask = 0xFF;
    int byteShiftVal = -8;
    int byteBuffer = { 0 };
    for (int re = 0; re < BlockFloatCompander::k_numREReal; ++re)
    {
      auto dataIdxIn = rb * BlockFloatCompander::k_numREReal + re;
      auto thisRE = dataIn.dataExpanded[dataIdxIn] >> thisExp;
      byteBuffer = (byteBuffer << dataIn.iqWidth) + (int)(thisRE & iqMask);

      byteShiftVal += (8 + byteShiftUnits);
      while (byteShiftVal >= 0)
      {
        auto thisByte = (uint8_t)((byteBuffer >> byteShiftVal) & k_byteMask);
        dataOut->dataCompressed[dataOutIdx++] = thisByte;
        byteShiftVal -= 8;
      }
    }
  }
  dataOut->iqWidth = dataIn.iqWidth;
}

/// Reference expansion
void
BlockFloatCompander::BlockFloatExpand_Basic(const CompressedData& dataIn, ExpandedData* dataOut)
{
  uint32_t iqMask = (uint32_t)(UINT_MAX - ((1 << (32 - dataIn.iqWidth)) - 1));
  uint32_t byteBuffer = { 0 };
  int numBytesPerRB = (3 * dataIn.iqWidth) + 1;
  int bitPointer = 0;
  int dataIdxOut = 0;

  for (int rb = 0; rb < BlockFloatCompander::k_numRB; ++rb)
  {
    auto expIdx = rb * numBytesPerRB;
    auto signExtShift = 32 - dataIn.iqWidth - dataIn.dataCompressed[expIdx];

    for (int b = 0; b < numBytesPerRB - 1; ++b)
    {
      auto dataIdxIn = (expIdx + 1) + b;
      auto thisByte = (uint16_t)dataIn.dataCompressed[dataIdxIn];
      byteBuffer = (uint32_t)((byteBuffer << 8) + thisByte);
      bitPointer += 8;
      while (bitPointer >= dataIn.iqWidth)
      {
        /// byteBuffer currently has enough data in it to extract a sample
        /// Shift left first to set sign bit at MSB, then shift right to
        /// sign extend down to iqWidth. Finally recast to int16.
        int32_t thisSample32 = (int32_t)((byteBuffer << (32 - bitPointer)) & iqMask);
        int16_t thisSample = (int16_t)(thisSample32 >> signExtShift);
        bitPointer -= dataIn.iqWidth;
        dataOut->dataExpanded[dataIdxOut++] = thisSample;
      }
    }
  }
}

/// Reference compression
void
BlockFloatCompanderBFW::BlockFloatCompress_Basic(const BlockFloatCompanderBFW::ExpandedData& dataIn, BlockFloatCompanderBFW::CompressedData* dataOut)
{
  int dataOutIdx = 0;
  int16_t iqMask = (int16_t)((1 << dataIn.iqWidth) - 1);
  int byteShiftUnits = dataIn.iqWidth - 8;

  for (int rb = 0; rb < BlockFloatCompanderBFW::k_numRB; ++rb)
  {
    /// Find max abs value for this RB
    int16_t maxAbs = 0;
    for (int re = 0; re < BlockFloatCompanderBFW::k_numREReal; ++re)
    {
      auto dataIdx = rb * BlockFloatCompanderBFW::k_numREReal + re;
      auto dataAbs = saturateAbs(dataIn.dataExpanded[dataIdx]);
      maxAbs = std::max(maxAbs, dataAbs);
    }

    // Find exponent and insert into byte stream
    auto thisExp = (uint8_t)(std::max(0,(16 - dataIn.iqWidth + 1 - __lzcnt16(maxAbs))));
    dataOut->dataCompressed[dataOutIdx++] = thisExp;

    /// ARS data by exponent and pack bytes in Network order
    /// This uses a sliding buffer where one or more bytes are
    /// extracted after the insertion of each compressed sample
    static constexpr int k_byteMask = 0xFF;
    int byteShiftVal = -8;
    int byteBuffer = { 0 };
    for (int re = 0; re < BlockFloatCompanderBFW::k_numREReal; ++re)
    {
      auto dataIdxIn = rb * BlockFloatCompanderBFW::k_numREReal + re;
      auto thisRE = dataIn.dataExpanded[dataIdxIn] >> thisExp;
      byteBuffer = (byteBuffer << dataIn.iqWidth) + (int)(thisRE & iqMask);

      byteShiftVal += (8 + byteShiftUnits);
      while (byteShiftVal >= 0)
      {
        auto thisByte = (uint8_t)((byteBuffer >> byteShiftVal) & k_byteMask);
        dataOut->dataCompressed[dataOutIdx++] = thisByte;
        byteShiftVal -= 8;
      }
    }
  }
  dataOut->iqWidth = dataIn.iqWidth;
}

/// Reference expansion
void
BlockFloatCompanderBFW::BlockFloatExpand_Basic(const BlockFloatCompanderBFW::CompressedData& dataIn, BlockFloatCompanderBFW::ExpandedData* dataOut)
{
  uint32_t iqMask = (uint32_t)(UINT_MAX - ((1 << (32 - dataIn.iqWidth)) - 1));
  uint32_t byteBuffer = { 0 };
  int numBytesPerRB = (3 * dataIn.iqWidth) + 1;
  int bitPointer = 0;
  int dataIdxOut = 0;

  for (int rb = 0; rb < BlockFloatCompanderBFW::k_numRB; ++rb)
  {
    auto expIdx = rb * numBytesPerRB;
    auto signExtShift = 32 - dataIn.iqWidth - dataIn.dataCompressed[expIdx];

    for (int b = 0; b < numBytesPerRB - 1; ++b)
    {
      auto dataIdxIn = (expIdx + 1) + b;
      auto thisByte = (uint16_t)dataIn.dataCompressed[dataIdxIn];
      byteBuffer = (uint32_t)((byteBuffer << 8) + thisByte);
      bitPointer += 8;
      while (bitPointer >= dataIn.iqWidth)
      {
        /// byteBuffer currently has enough data in it to extract a sample
        /// Shift left first to set sign bit at MSB, then shift right to
        /// sign extend down to iqWidth. Finally recast to int16.
        int32_t thisSample32 = (int32_t)((byteBuffer << (32 - bitPointer)) & iqMask);
        int16_t thisSample = (int16_t)(thisSample32 >> signExtShift);
        bitPointer -= dataIn.iqWidth;
        dataOut->dataExpanded[dataIdxOut++] = thisSample;
      }
    }
  }
}

#define RB_NUM_ROUNDUP(rb) \
    (BlockFloatCompander::k_numRB * ((rb + BlockFloatCompander::k_numRB - 1) / BlockFloatCompander::k_numRB))


/** callback function type for Symbol packet */
typedef void (*xran_bfp_compress_fn)(const BlockFloatCompander::ExpandedData& dataIn,
                                     BlockFloatCompander::CompressedData* dataOut);

int32_t
xranlib_compress_avx512(const struct xranlib_compress_request *request,
                        struct xranlib_compress_response *response)
{
    BlockFloatCompander::ExpandedData expandedDataInput;
    BlockFloatCompander::CompressedData compressedDataOut;
    xran_bfp_compress_fn com_fn = NULL;
    int16_t numRBs = request->numRBs;
    int16_t len = 0;

    switch (request->iqWidth){
        case 8:
            expandedDataInput.iqWidth = 8;
            com_fn = BlockFloatCompander::BlockFloatCompress_8b_AVX512;
            break;
        case 9:
            expandedDataInput.iqWidth = 9;
            com_fn = BlockFloatCompander::BlockFloatCompress_9b_AVX512;
            break;
        case 10:
            expandedDataInput.iqWidth = 10;
            com_fn = BlockFloatCompander::BlockFloatCompress_10b_AVX512;
            break;
        case 12:
            expandedDataInput.iqWidth = 12;
            com_fn = BlockFloatCompander::BlockFloatCompress_12b_AVX512;
            break;
        default:
            expandedDataInput.iqWidth = request->iqWidth;
            com_fn = BlockFloatCompander::BlockFloatCompress_Basic;
            break;
    }

    for (int16_t block_idx = 0;
        block_idx < RB_NUM_ROUNDUP(numRBs)/BlockFloatCompander::k_numRB /*+ 1*/; /*  16 RBs at time */
        block_idx++) {

        expandedDataInput.dataExpanded =
            &request->data_in[block_idx*BlockFloatCompander::k_numSampsExpanded];
        compressedDataOut.dataCompressed =
            (uint8_t*)&response->data_out[len];

        com_fn(expandedDataInput, &compressedDataOut);
        len  += ((3 * expandedDataInput.iqWidth) + 1) * std::min((int16_t)BlockFloatCompander::k_numRB,(int16_t)numRBs);
    }

    response->len =  ((3 * expandedDataInput.iqWidth) + 1) * numRBs;

    return 0;
}

/** callback function type for Symbol packet */
typedef void (*xran_bfp_compress_bfw_fn)(const BlockFloatCompanderBFW::ExpandedData& dataIn, BlockFloatCompanderBFW::CompressedData* dataOut);

int32_t
xranlib_compress_avx512_bfw(const struct xranlib_compress_request *request,
                        struct xranlib_compress_response *response)
{
    BlockFloatCompanderBFW::ExpandedData expandedDataInput;
    BlockFloatCompanderBFW::CompressedData compressedDataKern;
    xran_bfp_compress_bfw_fn com_fn = NULL;

#if 0
    for (int m = 0; m < BlockFloatCompander::k_numRB; ++m){
        for (int n = 0; n < BlockFloatCompander::k_numREReal; ++n){
            expandedDataInput.dataExpanded[m*BlockFloatCompander::k_numREReal+n] =
                request->data_in[m*BlockFloatCompander::k_numREReal+n];
        }
    }
#endif

    expandedDataInput.dataExpanded = request->data_in;
    compressedDataKern.dataCompressed = (uint8_t*)response->data_out;

    com_fn = BlockFloatCompanderBFW::BlockFloatCompress_Basic;
    switch (request->iqWidth){
        case 8:
            expandedDataInput.iqWidth = 8;
            break;
        case 9:
            expandedDataInput.iqWidth = 9;
            //com_fn = BlockFloatCompanderBFW::BlockFloatExpand_9b_AVX512
            break;
        case 10:
            expandedDataInput.iqWidth = 10;
            break;
        case 12:
            expandedDataInput.iqWidth = 12;
            break;
        default:
            printf("bfwIqWidth is not supported %d\n", request->iqWidth);
            return -1;
            break;
    }

    com_fn(expandedDataInput, &compressedDataKern);
    response->len = ((BlockFloatCompanderBFW::k_numRE/16*4*expandedDataInput.iqWidth)+1)*BlockFloatCompanderBFW::k_numRB;

    return 0;
}

/** callback function type for Symbol packet */
typedef void (*xran_bfp_decompress_fn)(const BlockFloatCompander::CompressedData& dataIn, BlockFloatCompander::ExpandedData* dataOut);


int32_t
xranlib_decompress_avx512(const struct xranlib_decompress_request *request,
    struct xranlib_decompress_response *response)
{

    BlockFloatCompander::CompressedData compressedDataInput;
    BlockFloatCompander::ExpandedData expandedDataOut;

    xran_bfp_decompress_fn decom_fn = NULL;
    int16_t numRBs = request->numRBs;
    int16_t len = 0;

    switch (request->iqWidth){
        case 8:
            compressedDataInput.iqWidth = 8;
            decom_fn = BlockFloatCompander::BlockFloatExpand_8b_AVX512;
            break;
        case 9:
            compressedDataInput.iqWidth = 9;
            decom_fn = BlockFloatCompander::BlockFloatExpand_9b_AVX512;
            break;
        case 10:
            compressedDataInput.iqWidth = 10;
            decom_fn = BlockFloatCompander::BlockFloatExpand_10b_AVX512;
            break;
        case 12:
            compressedDataInput.iqWidth = 12;
            decom_fn = BlockFloatCompander::BlockFloatExpand_12b_AVX512;
            break;
        default:
            compressedDataInput.iqWidth = request->iqWidth;
            decom_fn = BlockFloatCompander::BlockFloatExpand_Basic;
            break;
    }

    for (int16_t block_idx = 0;
        block_idx < RB_NUM_ROUNDUP(numRBs)/BlockFloatCompander::k_numRB;
        block_idx++) {

        compressedDataInput.dataCompressed = (uint8_t*)&request->data_in[block_idx*(((3 * compressedDataInput.iqWidth ) + 1) * BlockFloatCompander::k_numRB)];
        expandedDataOut.dataExpanded = &response->data_out[len];

        decom_fn(compressedDataInput, &expandedDataOut);
        len  += std::min((int16_t)BlockFloatCompander::k_numSampsExpanded, (int16_t)(numRBs*BlockFloatCompander::k_numREReal));
    }

    response->len = numRBs * BlockFloatCompander::k_numREReal* sizeof(int16_t);

    return 0;
}
