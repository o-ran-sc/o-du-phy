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
#include <complex>
#include <algorithm>
#include <immintrin.h>

void
BlockFloatCompander::BlockFloatCompress_AVX512(const ExpandedData& dataIn, CompressedData* dataOut)
{
  __m512i maxAbs = __m512i();

  /// Load data and find max(abs(RB))
  const __m512i* rawData = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
  static constexpr int k_numInputLoopIts = BlockFloatCompander::k_numRB / 4;

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
    static constexpr uint8_t k_msk1 = 0b11111100; // Copy first lane of src
    static constexpr int k_shuff1 = 0x41;
    const auto z_w1 = _mm512_mask_shuffle_i64x2(rawData[3 * n + 0], k_msk1, rawData[3 * n + 1], rawData[3 * n + 2], k_shuff1);

    static constexpr uint8_t k_msk2 = 0b11000011; // Copy middle two lanes of src
    static constexpr int k_shuff2 = 0xB1;
    const auto z_w2 = _mm512_mask_shuffle_i64x2(rawData[3 * n + 1], k_msk2, rawData[3 * n + 0], rawData[3 * n + 2], k_shuff2);

    static constexpr uint8_t k_msk3 = 0b00111111; // Copy last lane of src
    static constexpr int k_shuff3 = 0xBE;
    const auto z_w3 = _mm512_mask_shuffle_i64x2(rawData[3 * n + 2], k_msk3, rawData[3 * n + 0], rawData[3 * n + 1], k_shuff3);

    /// Perform max abs on these 3 registers
    const auto abs16_1 = _mm512_abs_epi16(z_w1);
    const auto abs16_2 = _mm512_abs_epi16(z_w2);
    const auto abs16_3 = _mm512_abs_epi16(z_w3);
    const auto maxAbs_12 = _mm512_max_epi16(abs16_1, abs16_2);
    const auto maxAbs_123 = _mm512_max_epi16(maxAbs_12, abs16_3);

    /// Perform horizontal max over each lane
    /// Swap 64b in each lane and compute max
    static const auto k_perm64b = _mm512_set_epi64(6, 7, 4, 5, 2, 3, 0, 1);
    auto maxAbsPerm = _mm512_permutexvar_epi64(k_perm64b, maxAbs_123);
    auto maxAbsHorz = _mm512_max_epi16(maxAbs_123, maxAbsPerm);

    /// Swap each pair of 32b in each lane and compute max
    static const auto k_perm32b = _mm512_set_epi32(14, 15, 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1);
    maxAbsPerm = _mm512_permutexvar_epi32(k_perm32b, maxAbsHorz);
    maxAbsHorz = _mm512_max_epi16(maxAbsHorz, maxAbsPerm);

    /// Swap each IQ pair in each lane (via 32b rotation) and compute max
    maxAbsPerm = _mm512_rol_epi32(maxAbsHorz, BlockFloatCompander::k_numBitsIQ);
    maxAbsHorz = _mm512_max_epi16(maxAbsHorz, maxAbsPerm);

    /// Insert values into maxAbs
    /// Use sliding mask to insert wanted values into maxAbs
    /// Pairs of values will be inserted and corrected outside of loop
    static const auto k_select4RB = _mm512_set_epi32(28, 24, 20, 16, 28, 24, 20, 16,
                                                     28, 24, 20, 16, 28, 24, 20, 16);
    static constexpr uint16_t k_expMsk[k_numInputLoopIts] = { 0x000F, 0x00F0, 0x0F00, 0xF000 };
    maxAbs = _mm512_mask_permutex2var_epi32(maxAbs, k_expMsk[n], k_select4RB, maxAbsHorz);
  }

  /// Convert to 32b by removing repeated values in maxAbs
  static const auto k_upperWordMask = _mm512_set_epi64(0x0000FFFF0000FFFF, 0x0000FFFF0000FFFF,
                                                       0x0000FFFF0000FFFF, 0x0000FFFF0000FFFF,
                                                       0x0000FFFF0000FFFF, 0x0000FFFF0000FFFF,
                                                       0x0000FFFF0000FFFF, 0x0000FFFF0000FFFF);
  maxAbs = _mm512_and_epi64(maxAbs, k_upperWordMask);

  /// Compute exponent and store for later use
  static constexpr int k_expTotShiftBits = 32 - BlockFloatCompander::k_iqWidth + 1;
  const auto totShiftBits = _mm512_set1_epi32(k_expTotShiftBits);
  const auto lzCount = _mm512_lzcnt_epi32(maxAbs);
  const auto exponent = _mm512_sub_epi32(totShiftBits, lzCount);
  int8_t storedExp[BlockFloatCompander::k_numRB] = {};
  static constexpr uint16_t k_expWriteMask = 0xFFFF;
  _mm512_mask_cvtepi32_storeu_epi8(storedExp, k_expWriteMask, exponent);

  /// Shift 1RB by corresponding exponent and write exponent and data to output
  /// Output data is packed exponent first followed by corresponding compressed RB
#pragma unroll(BlockFloatCompander::k_numRB)
  for (int n = 0; n < BlockFloatCompander::k_numRB; ++n)
  {
    const __m512i* rawDataIn = reinterpret_cast<const __m512i*>(dataIn.dataExpanded + n * BlockFloatCompander::k_numREReal);
    auto compData = _mm512_srai_epi16(*rawDataIn, storedExp[n]);

    dataOut->dataCompressed[n * (BlockFloatCompander::k_numREReal + 1)] = storedExp[n];
    static constexpr uint32_t k_rbMask = 0x00FFFFFF; // Write mask for 1RB (24 values)
    _mm512_mask_cvtepi16_storeu_epi8(dataOut->dataCompressed + n * (BlockFloatCompander::k_numREReal + 1) + 1, k_rbMask, compData);
  }
}


void
BlockFloatCompander::BlockFloatExpand_AVX512(const CompressedData& dataIn, ExpandedData* dataOut)
{
#pragma unroll(BlockFloatCompander::k_numRB)
  for (int n = 0; n < BlockFloatCompander::k_numRB; ++n)
  {
    /// Expand 1RB of data
    const __m256i* rawDataIn = reinterpret_cast<const __m256i*>(dataIn.dataCompressed + n * (BlockFloatCompander::k_numREReal + 1) + 1);
    const auto compData16 = _mm512_cvtepi8_epi16(*rawDataIn);
    const auto expData = _mm512_slli_epi16(compData16, *(dataIn.dataCompressed + n * (BlockFloatCompander::k_numREReal + 1)));

    /// Write expanded data to output
    static constexpr uint8_t k_rbMask64 = 0b00111111; // 64b write mask for 1RB (24 int16 values)
    _mm512_mask_storeu_epi64(dataOut->dataExpanded + n * BlockFloatCompander::k_numREReal, k_rbMask64, expData);
  }
}


void
BlockFloatCompander::BlockFloatCompress_Basic(const ExpandedData& dataIn, CompressedData* dataOut)
{
  int16_t maxAbs[BlockFloatCompander::k_numRB];
  for (int rb = 0; rb < BlockFloatCompander::k_numRB; ++rb)
  {
    // Find max abs value for this RB
    maxAbs[rb] = 0;
    for (int re = 0; re < BlockFloatCompander::k_numREReal; ++re)
    {
      auto dataIdx = rb * BlockFloatCompander::k_numREReal + re;
      int16_t dataAbs = (int16_t)std::abs(dataIn.dataExpanded[dataIdx]);
      maxAbs[rb] = std::max(maxAbs[rb], dataAbs);
    }

    // Find exponent
    static constexpr int k_expTotShiftBits16 = 16 - BlockFloatCompander::k_iqWidth + 1;
    auto thisExp = (int8_t)(k_expTotShiftBits16 - __lzcnt16(maxAbs[rb]));
    auto expIdx = rb * (BlockFloatCompander::k_numREReal + 1);
    dataOut->dataCompressed[expIdx] = thisExp;

    // ARS data by exponent
    for (int re = 0; re < BlockFloatCompander::k_numREReal; ++re)
    {
      auto dataIdxIn = rb * BlockFloatCompander::k_numREReal + re;
      auto dataIdxOut = (expIdx + 1) + re;
      dataOut->dataCompressed[dataIdxOut] = (int8_t)(dataIn.dataExpanded[dataIdxIn] >> thisExp);
    }
  }
}


void
BlockFloatCompander::BlockFloatExpand_Basic(const CompressedData& dataIn, ExpandedData* dataOut)
{
  // Expand data
  for (int rb = 0; rb < BlockFloatCompander::k_numRB; ++rb)
  {
    for (int re = 0; re < BlockFloatCompander::k_numREReal; ++re)
    {
      auto dataIdxOut = rb * BlockFloatCompander::k_numREReal + re;
      auto expIdx = rb * (BlockFloatCompander::k_numREReal + 1);
      auto dataIdxIn = (expIdx + 1) + re;
      auto thisData = (int16_t)dataIn.dataCompressed[dataIdxIn];
      auto thisExp = (int16_t)dataIn.dataCompressed[expIdx];
      dataOut->dataExpanded[dataIdxOut] = (int16_t)(thisData << thisExp);
    }
  }
}
