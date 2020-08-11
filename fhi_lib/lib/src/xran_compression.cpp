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
 * @brief xRAN BFP compression/decompression U-plane implementation and interface functions
 *
 * @file xran_compression.cpp
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#include "xran_compression.hpp"
#include "xran_bfp_utils.hpp"
#include "xran_compression.h"
#include <complex>
#include <algorithm>
#include <immintrin.h>
#include <limits.h>
#include <cstring>

namespace BFP_UPlane
{
  /// Namespace constants
  const int k_numREReal = 24; /// 12 IQ pairs

  /// Perform horizontal max of 16 bit values across each lane
  __m512i
  horizontalMax4x16(const __m512i maxAbsIn)
  {
    /// Swap 64b in each lane and compute max
    const auto k_perm64b = _mm512_set_epi64(6, 7, 4, 5, 2, 3, 0, 1);
    auto maxAbsPerm = _mm512_permutexvar_epi64(k_perm64b, maxAbsIn);
    auto maxAbsHorz = _mm512_max_epi16(maxAbsIn, maxAbsPerm);

    /// Swap each pair of 32b in each lane and compute max
    const auto k_perm32b = _mm512_set_epi32(14, 15, 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1);
    maxAbsPerm = _mm512_permutexvar_epi32(k_perm32b, maxAbsHorz);
    maxAbsHorz = _mm512_max_epi16(maxAbsHorz, maxAbsPerm);

    /// Swap each IQ pair in each lane (via 32b rotation) and compute max
    maxAbsPerm = _mm512_rol_epi32(maxAbsHorz, BlockFloatCompander::k_numBitsIQ);
    return _mm512_max_epi16(maxAbsHorz, maxAbsPerm);
  }


  /// Perform U-plane input data re-ordering and vertical max abs of 16b values
  /// Works on 4 RB at a time
  __m512i
  maxAbsVertical4RB(const __m512i inA, const __m512i inB, const __m512i inC)
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
    const auto z_w1 = _mm512_mask_shuffle_i64x2(inA, k_msk1, inB, inC, k_shuff1);

    constexpr uint8_t k_msk2 = 0b11000011; // Copy middle two lanes of src
    constexpr int k_shuff2 = 0xB1;
    const auto z_w2 = _mm512_mask_shuffle_i64x2(inB, k_msk2, inA, inC, k_shuff2);

    constexpr uint8_t k_msk3 = 0b00111111; // Copy last lane of src
    constexpr int k_shuff3 = 0xBE;
    const auto z_w3 = _mm512_mask_shuffle_i64x2(inC, k_msk3, inA, inB, k_shuff3);

    /// Perform max abs on these 3 registers
    const auto abs16_1 = _mm512_abs_epi16(z_w1);
    const auto abs16_2 = _mm512_abs_epi16(z_w2);
    const auto abs16_3 = _mm512_abs_epi16(z_w3);
    return _mm512_max_epi16(_mm512_max_epi16(abs16_1, abs16_2), abs16_3);
  }


  /// Selects first 32 bit value in each src lane and packs into laneNum of dest
  __m512i
  slidePermute(const __m512i src, const __m512i dest, const int laneNum)
  {
    const auto k_selectVals = _mm512_set_epi32(28, 24, 20, 16, 28, 24, 20, 16,
                                               28, 24, 20, 16, 28, 24, 20, 16);
    constexpr uint16_t k_laneMsk[4] = { 0x000F, 0x00F0, 0x0F00, 0xF000 };
    return _mm512_mask_permutex2var_epi32(dest, k_laneMsk[laneNum], k_selectVals, src);
  }


  /// Compute exponent value for a set of 16 RB from the maximum absolute value.
  /// Max Abs operates in a loop, executing 4 RB per iteration. The results are
  /// packed into the final output register.
  __m512i
  computeExponent_16RB(const BlockFloatCompander::ExpandedData& dataIn, const __m512i totShiftBits)
  {
    __m512i maxAbs = __m512i();
    const __m512i* rawData = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
    /// Max Abs loop operates on 4RB at a time
#pragma unroll(4)
    for (int n = 0; n < 4; ++n)
    {
      /// Re-order and vertical max abs
      auto maxAbsVert = maxAbsVertical4RB(rawData[3 * n + 0], rawData[3 * n + 1], rawData[3 * n + 2]);
      /// Horizontal max abs
      auto maxAbsHorz = horizontalMax4x16(maxAbsVert);
      /// Pack these 4 values into maxAbs
      maxAbs = slidePermute(maxAbsHorz, maxAbs, n);
    }
    /// Calculate exponent
    const auto maxAbs32 = BlockFloatCompander::maskUpperWord(maxAbs);
    return BlockFloatCompander::expLzCnt(maxAbs32, totShiftBits);
  }


  /// Compute exponent value for a set of 4 RB from the maximum absolute value.
  /// Note that we do not need to perform any packing of result as we are only
  /// computing 4 RB. The appropriate offset is taken later when extracting the
  /// exponent.
  __m512i
  computeExponent_4RB(const BlockFloatCompander::ExpandedData& dataIn, const __m512i totShiftBits)
  {
    const __m512i* rawData = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
    /// Re-order and vertical max abs
    const auto maxAbsVert = maxAbsVertical4RB(rawData[0], rawData[1], rawData[2]);
    /// Horizontal max abs
    const auto maxAbsHorz = horizontalMax4x16(maxAbsVert);
    /// Calculate exponent
    const auto maxAbs = BlockFloatCompander::maskUpperWord(maxAbsHorz);
    return BlockFloatCompander::expLzCnt(maxAbs, totShiftBits);
  }


  /// Compute exponent value for 1 RB from the maximum absolute value.
  /// This works with horizontal max abs only, and needs to include a
  /// step to select the final exponent from the 4 lanes.
  uint8_t
  computeExponent_1RB(const BlockFloatCompander::ExpandedData& dataIn, const __m512i totShiftBits)
  {
    const __m512i* rawData = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
    /// Abs
    const auto rawDataAbs = _mm512_abs_epi16(rawData[0]);
    /// No need to do a full horizontal max operation here, just do a max IQ step,
    /// compute the exponents and then use a reduce max over all exponent values. This
    /// is the fastest way to handle a single RB.
    const auto rawAbsIQSwap = _mm512_rol_epi32(rawDataAbs, BlockFloatCompander::k_numBitsIQ);
    const auto maxAbsIQ = _mm512_max_epi16(rawDataAbs, rawAbsIQSwap);
    /// Calculate exponent
    const auto maxAbsIQ32 = BlockFloatCompander::maskUpperWord(maxAbsIQ);
    const auto exps = BlockFloatCompander::expLzCnt(maxAbsIQ32, totShiftBits);
    /// At this point we have exponent values for the maximum of each IQ pair.
    /// Run a reduce max step to compute the maximum exponent value in the first
    /// three lanes - this will give the desired exponent for this RB.
    constexpr uint16_t k_expMsk = 0x0FFF;
    return (uint8_t)_mm512_mask_reduce_max_epi32(k_expMsk, exps);
  }


  /// Apply compression to 1 RB
  template<BlockFloatCompander::PackFunction networkBytePack>
  void
  applyCompressionN_1RB(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut,
                        const int numREOffset, const uint8_t thisExp, const int thisRBExpAddr, const uint16_t rbWriteMask)
  {
    /// Get AVX512 pointer aligned to desired RB
    const __m512i* rawDataIn = reinterpret_cast<const __m512i*>(dataIn.dataExpanded + numREOffset);
    /// Apply the exponent shift
    const auto compData = _mm512_srai_epi16(*rawDataIn, thisExp);
    /// Pack compressed data network byte order
    const auto compDataBytePacked = networkBytePack(compData);
    /// Store exponent first
    dataOut->dataCompressed[thisRBExpAddr] = thisExp;
    /// Now have 1 RB worth of bytes separated into 3 chunks (1 per lane)
    /// Use three offset stores to join
    _mm_mask_storeu_epi8(dataOut->dataCompressed + thisRBExpAddr + 1, rbWriteMask, _mm512_extracti64x2_epi64(compDataBytePacked, 0));
    _mm_mask_storeu_epi8(dataOut->dataCompressed + thisRBExpAddr + 1 + dataIn.iqWidth, rbWriteMask, _mm512_extracti64x2_epi64(compDataBytePacked, 1));
    _mm_mask_storeu_epi8(dataOut->dataCompressed + thisRBExpAddr + 1 + (2 * dataIn.iqWidth), rbWriteMask, _mm512_extracti64x2_epi64(compDataBytePacked, 2));
  }


  /// Apply 9, 10, or 12bit compression to 16 RB
  template<BlockFloatCompander::PackFunction networkBytePack>
  void
  compressN_16RB(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut,
                 const __m512i totShiftBits, const int totNumBytesPerRB, const uint16_t rbWriteMask)
  {
    const auto exponents = computeExponent_16RB(dataIn, totShiftBits);
#pragma unroll(16)
    for (int n = 0; n < 16; ++n)
    {
      applyCompressionN_1RB<networkBytePack>(dataIn, dataOut, n * k_numREReal, ((uint8_t*)&exponents)[n * 4], n * totNumBytesPerRB, rbWriteMask);
    }
  }


  /// Apply 9, 10, or 12bit compression to 4 RB
  template<BlockFloatCompander::PackFunction networkBytePack>
  void
  compressN_4RB(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut,
                const __m512i totShiftBits, const int totNumBytesPerRB, const uint16_t rbWriteMask)
  {
    const auto exponents = computeExponent_4RB(dataIn, totShiftBits);
#pragma unroll(4)
    for (int n = 0; n < 4; ++n)
    {
      applyCompressionN_1RB<networkBytePack>(dataIn, dataOut, n * k_numREReal, ((uint8_t*)&exponents)[n * 16], n * totNumBytesPerRB, rbWriteMask);
    }
  }


  /// Apply 9, 10, or 12bit compression to 1 RB
  template<BlockFloatCompander::PackFunction networkBytePack>
  void
  compressN_1RB(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut,
                const __m512i totShiftBits, const int totNumBytesPerRB, const uint16_t rbWriteMask)
  {
    const auto thisExponent = computeExponent_1RB(dataIn, totShiftBits);
    applyCompressionN_1RB<networkBytePack>(dataIn, dataOut, 0, thisExponent, 0, rbWriteMask);
  }


  /// Calls compression function specific to the number of RB to be executed. For 9, 10, or 12bit iqWidth.
  template<BlockFloatCompander::PackFunction networkBytePack>
  void
  compressByAllocN(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut,
                   const __m512i totShiftBits, const int totNumBytesPerRB, const uint16_t rbWriteMask)
  {
    switch (dataIn.numBlocks)
    {
    case 16:
      compressN_16RB<networkBytePack>(dataIn, dataOut, totShiftBits, totNumBytesPerRB, rbWriteMask);
      break;

    case 4:
      compressN_4RB<networkBytePack>(dataIn, dataOut, totShiftBits, totNumBytesPerRB, rbWriteMask);
      break;

    case 1:
      compressN_1RB<networkBytePack>(dataIn, dataOut, totShiftBits, totNumBytesPerRB, rbWriteMask);
      break;
    }
  }


  /// Apply compression to 1 RB
  void
  applyCompression8_1RB(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut,
                        const int numREOffset, const uint8_t thisExp, const int thisRBExpAddr)
  {
    /// Get AVX512 pointer aligned to desired RB
    const __m512i* rawDataIn = reinterpret_cast<const __m512i*>(dataIn.dataExpanded + numREOffset);
    /// Apply the exponent shift
    const auto compData = _mm512_srai_epi16(*rawDataIn, thisExp);
    /// Store exponent first
    dataOut->dataCompressed[thisRBExpAddr] = thisExp;
    /// Now have 1 RB worth of bytes separated into 3 chunks (1 per lane)
    /// Use three offset stores to join
    constexpr uint32_t k_rbMask = 0x00FFFFFF; // Write mask for 1RB (24 values)
    _mm256_mask_storeu_epi8(dataOut->dataCompressed + thisRBExpAddr + 1, k_rbMask, _mm512_cvtepi16_epi8(compData));
  }


  /// 8bit RB compression loop for 16 RB
  void
  compress8_16RB(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut, const __m512i totShiftBits)
  {
    const auto exponents = computeExponent_16RB(dataIn, totShiftBits);
#pragma unroll(16)
    for (int n = 0; n < 16; ++n)
    {
      applyCompression8_1RB(dataIn, dataOut, n * k_numREReal, ((uint8_t*)&exponents)[n * 4], n * (k_numREReal + 1));
    }
  }


  /// 8bit RB compression loop for 4 RB
  void
  compress8_4RB(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut, const __m512i totShiftBits)
  {
    const auto exponents = computeExponent_4RB(dataIn, totShiftBits);
#pragma unroll(4)
    for (int n = 0; n < 4; ++n)
    {
      applyCompression8_1RB(dataIn, dataOut, n * k_numREReal, ((uint8_t*)&exponents)[n * 16], n * (k_numREReal + 1));
    }
  }


  /// 8bit RB compression loop for 4 RB
  void
  compress8_1RB(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut, const __m512i totShiftBits)
  {
    const auto thisExponent = computeExponent_1RB(dataIn, totShiftBits);
    applyCompression8_1RB(dataIn, dataOut, 0, thisExponent, 0);
  }


  /// Calls compression function specific to the number of RB to be executed. For 8 bit iqWidth.
  void
  compressByAlloc8(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut, const __m512i totShiftBits)
  {
    switch (dataIn.numBlocks)
    {
    case 16:
      compress8_16RB(dataIn, dataOut, totShiftBits);
      break;

    case 4:
      compress8_4RB(dataIn, dataOut, totShiftBits);
      break;

    case 1:
      compress8_1RB(dataIn, dataOut, totShiftBits);
      break;
    }
  }


  /// Apply compression to 1 RB
  template<BlockFloatCompander::UnpackFunction networkByteUnpack>
  void
  applyExpansionN_1RB(const BlockFloatCompander::CompressedData& dataIn, BlockFloatCompander::ExpandedData* dataOut,
                      const int expAddr, const int thisRBAddr, const int maxExpShift)
  {
    /// Unpack network order packed data
    const auto dataUnpacked = networkByteUnpack(dataIn.dataCompressed + expAddr + 1);
    /// Apply exponent scaling (by appropriate arithmetic shift right)
    const auto dataExpanded = _mm512_srai_epi16(dataUnpacked, maxExpShift - *(dataIn.dataCompressed + expAddr));
    /// Write expanded data to output
    static constexpr uint32_t k_WriteMask = 0x00FFFFFF;
    _mm512_mask_storeu_epi16(dataOut->dataExpanded + thisRBAddr, k_WriteMask, dataExpanded);
  }


  /// Calls compression function specific to the number of RB to be executed. For 9, 10, or 12bit iqWidth.
  template<BlockFloatCompander::UnpackFunction networkByteUnpack>
  void
  expandByAllocN(const BlockFloatCompander::CompressedData& dataIn, BlockFloatCompander::ExpandedData* dataOut,
                 const int totNumBytesPerRB, const int maxExpShift)
  {
    switch (dataIn.numBlocks)
    {
    case 16:
#pragma unroll(16)
      for (int n = 0; n < 16; ++n)
      {
        applyExpansionN_1RB<networkByteUnpack>(dataIn, dataOut, n * totNumBytesPerRB, n * k_numREReal, maxExpShift);
      }
      break;

    case 4:
#pragma unroll(4)
      for (int n = 0; n < 4; ++n)
      {
        applyExpansionN_1RB<networkByteUnpack>(dataIn, dataOut, n * totNumBytesPerRB, n * k_numREReal, maxExpShift);
      }
      break;

    case 1:
      applyExpansionN_1RB<networkByteUnpack>(dataIn, dataOut, 0, 0, maxExpShift);
      break;
    }
  }


  /// Apply expansion to 1 RB and store
  void
  applyExpansion8_1RB(const BlockFloatCompander::CompressedData& dataIn, BlockFloatCompander::ExpandedData* dataOut,
                      const int expAddr, const int thisRBAddr)
  {
    const __m256i* rawDataIn = reinterpret_cast<const __m256i*>(dataIn.dataCompressed + expAddr + 1);
    const auto compData16 = _mm512_cvtepi8_epi16(*rawDataIn);
    const auto expData = _mm512_slli_epi16(compData16, *(dataIn.dataCompressed + expAddr));
    constexpr uint8_t k_rbMask64 = 0b00111111; // 64b write mask for 1RB (24 int16 values)
    _mm512_mask_storeu_epi64(dataOut->dataExpanded + thisRBAddr, k_rbMask64, expData);
  }


  /// Calls expansion function specific to the number of RB to be executed. For 8 bit iqWidth.
  void
  expandByAlloc8(const BlockFloatCompander::CompressedData& dataIn, BlockFloatCompander::ExpandedData* dataOut)
  {
    switch (dataIn.numBlocks)
    {
    case 16:
#pragma unroll(16)
      for (int n = 0; n < 16; ++n)
      {
        applyExpansion8_1RB(dataIn, dataOut, n * (k_numREReal + 1), n * k_numREReal);
      }
      break;

    case 4:
#pragma unroll(4)
      for (int n = 0; n < 4; ++n)
      {
        applyExpansion8_1RB(dataIn, dataOut, n * (k_numREReal + 1), n * k_numREReal);
      }
      break;

    case 1:
      applyExpansion8_1RB(dataIn, dataOut, 0, 0);
      break;
    }
  }
}



/// Main kernel function for compression.
/// Starts by determining iqWidth specific parameters and functions.
void
BlockFloatCompander::BFPCompressUserPlaneAvx512(const ExpandedData& dataIn, CompressedData* dataOut)
{
  /// Compensation for extra zeros in 32b leading zero count when computing exponent
  const auto totShiftBits8 = _mm512_set1_epi32(25);
  const auto totShiftBits9 = _mm512_set1_epi32(24);
  const auto totShiftBits10 = _mm512_set1_epi32(23);
  const auto totShiftBits12 = _mm512_set1_epi32(21);

  /// Total number of compressed bytes per RB for each iqWidth option
  constexpr int totNumBytesPerRB9 = 28;
  constexpr int totNumBytesPerRB10 = 31;
  constexpr int totNumBytesPerRB12 = 37;

  /// Compressed data write mask for each iqWidth option
  constexpr uint16_t rbWriteMask9 = 0x01FF;
  constexpr uint16_t rbWriteMask10 = 0x03FF;
  constexpr uint16_t rbWriteMask12 = 0x0FFF;

  switch (dataIn.iqWidth)
  {
  case 8:
    BFP_UPlane::compressByAlloc8(dataIn, dataOut, totShiftBits8);
    break;

  case 9:
    BFP_UPlane::compressByAllocN<BlockFloatCompander::networkBytePack9b>(dataIn, dataOut, totShiftBits9, totNumBytesPerRB9, rbWriteMask9);
    break;

  case 10:
    BFP_UPlane::compressByAllocN<BlockFloatCompander::networkBytePack10b>(dataIn, dataOut, totShiftBits10, totNumBytesPerRB10, rbWriteMask10);
    break;

  case 12:
    BFP_UPlane::compressByAllocN<BlockFloatCompander::networkBytePack12b>(dataIn, dataOut, totShiftBits12, totNumBytesPerRB12, rbWriteMask12);
    break;
  }
}



/// Main kernel function for expansion.
/// Starts by determining iqWidth specific parameters and functions.
void
BlockFloatCompander::BFPExpandUserPlaneAvx512(const CompressedData& dataIn, ExpandedData* dataOut)
{
  constexpr int k_totNumBytesPerRB9 = 28;
  constexpr int k_totNumBytesPerRB10 = 31;
  constexpr int k_totNumBytesPerRB12 = 37;

  constexpr int k_maxExpShift9 = 7;
  constexpr int k_maxExpShift10 = 6;
  constexpr int k_maxExpShift12 = 4;

  switch (dataIn.iqWidth)
  {
  case 8:
    BFP_UPlane::expandByAlloc8(dataIn, dataOut);
    break;

  case 9:
    BFP_UPlane::expandByAllocN<BlockFloatCompander::networkByteUnpack9b>(dataIn, dataOut, k_totNumBytesPerRB9, k_maxExpShift9);
    break;

  case 10:
    BFP_UPlane::expandByAllocN<BlockFloatCompander::networkByteUnpack10b>(dataIn, dataOut, k_totNumBytesPerRB10, k_maxExpShift10);
    break;

  case 12:
    BFP_UPlane::expandByAllocN<BlockFloatCompander::networkByteUnpack12b>(dataIn, dataOut, k_totNumBytesPerRB12, k_maxExpShift12);
    break;
  }
}

/** callback function type for Symbol packet */
typedef void (*xran_bfp_compress_fn)(const BlockFloatCompander::ExpandedData& dataIn,
                                     BlockFloatCompander::CompressedData* dataOut);

/** callback function type for Symbol packet */
typedef void (*xran_bfp_decompress_fn)(const BlockFloatCompander::CompressedData& dataIn, BlockFloatCompander::ExpandedData* dataOut);

int32_t
xranlib_compress_avx512(const struct xranlib_compress_request *request,
                        struct xranlib_compress_response *response)
{
    BlockFloatCompander::ExpandedData expandedDataInput;
    BlockFloatCompander::CompressedData compressedDataOut;
    xran_bfp_compress_fn com_fn = NULL;
    uint16_t totalRBs = request->numRBs;
    uint16_t remRBs   = totalRBs;
    int16_t len = 0;
    int16_t block_idx_bytes = 0;

    switch (request->iqWidth) {
        case 8:
        case 9:
        case 10:
        case 12:
            com_fn = BlockFloatCompander::BFPCompressUserPlaneAvx512;
            break;
        default:
            com_fn = BlockFloatCompander::BFPCompressRef;
            break;
    }

    expandedDataInput.iqWidth = request->iqWidth;
    expandedDataInput.numDataElements =  24;

    while (remRBs){
        expandedDataInput.dataExpanded   = &request->data_in[block_idx_bytes];
        compressedDataOut.dataCompressed = (uint8_t*)&response->data_out[len];
        if(remRBs >= 16){
            expandedDataInput.numBlocks = 16;
            com_fn(expandedDataInput, &compressedDataOut);
            len  += ((3 * expandedDataInput.iqWidth) + 1) * std::min((int16_t)BlockFloatCompander::k_maxNumBlocks,(int16_t)16);
            block_idx_bytes += 16*expandedDataInput.numDataElements;
            remRBs -= 16;
        }else if(remRBs >= 4){
            expandedDataInput.numBlocks = 4;
            com_fn(expandedDataInput, &compressedDataOut);
            len  += ((3 * expandedDataInput.iqWidth) + 1) * std::min((int16_t)BlockFloatCompander::k_maxNumBlocks,(int16_t)4);
            block_idx_bytes +=4*expandedDataInput.numDataElements;
            remRBs -=4;
        }else if (remRBs >= 1){
            expandedDataInput.numBlocks = 1;
            com_fn(expandedDataInput, &compressedDataOut);
            len  += ((3 * expandedDataInput.iqWidth) + 1) * std::min((int16_t)BlockFloatCompander::k_maxNumBlocks,(int16_t)1);
            block_idx_bytes +=1*expandedDataInput.numDataElements;
            remRBs = remRBs - 1;
        }
    }

    response->len =  ((3 * expandedDataInput.iqWidth) + 1) * totalRBs;

    return 0;
}

int32_t
xranlib_decompress_avx512(const struct xranlib_decompress_request *request,
    struct xranlib_decompress_response *response)
{
    BlockFloatCompander::CompressedData compressedDataInput;
    BlockFloatCompander::ExpandedData expandedDataOut;

    xran_bfp_decompress_fn decom_fn = NULL;
    uint16_t totalRBs = request->numRBs;
    uint16_t remRBs   = totalRBs;
    int16_t len = 0;
    int16_t block_idx_bytes = 0;

    switch (request->iqWidth) {
    case 8:
    case 9:
    case 10:
    case 12:
        decom_fn = BlockFloatCompander::BFPExpandUserPlaneAvx512;
        break;
    default:
        decom_fn = BlockFloatCompander::BFPExpandRef;
        break;
    }

    compressedDataInput.iqWidth         =  request->iqWidth;
    compressedDataInput.numDataElements =  24;

    while(remRBs) {
        compressedDataInput.dataCompressed = (uint8_t*)&request->data_in[block_idx_bytes];
        expandedDataOut.dataExpanded       = &response->data_out[len];
        if(remRBs >= 16){
            compressedDataInput.numBlocks = 16;
            decom_fn(compressedDataInput, &expandedDataOut);
            len  += 16*compressedDataInput.numDataElements;
            block_idx_bytes  += ((3 * compressedDataInput.iqWidth) + 1) * std::min((int16_t)BlockFloatCompander::k_maxNumBlocks,(int16_t)16);
            remRBs -= 16;
        }else if(remRBs >= 4){
            compressedDataInput.numBlocks = 4;
            decom_fn(compressedDataInput, &expandedDataOut);
            len  += 4*compressedDataInput.numDataElements;
            block_idx_bytes  += ((3 * compressedDataInput.iqWidth) + 1) * std::min((int16_t)BlockFloatCompander::k_maxNumBlocks,(int16_t)4);
            remRBs -=4;
        }else if (remRBs >= 1){
            compressedDataInput.numBlocks = 1;
            decom_fn(compressedDataInput, &expandedDataOut);
            len  += 1*compressedDataInput.numDataElements;
            block_idx_bytes  += ((3 * compressedDataInput.iqWidth) + 1) * std::min((int16_t)BlockFloatCompander::k_maxNumBlocks,(int16_t)1);
            remRBs = remRBs - 1;
        }
    }

    response->len = totalRBs * compressedDataInput.numDataElements * sizeof(int16_t);

    return 0;
}

int32_t
xranlib_compress_avx512_bfw(const struct xranlib_compress_request *request,
                        struct xranlib_compress_response *response)
{
    BlockFloatCompander::ExpandedData expandedDataInput;
    BlockFloatCompander::CompressedData compressedDataOut;
    xran_bfp_compress_fn com_fn = NULL;

    if (request->numRBs != 1){
        printf("Unsupported numRBs %d\n", request->numRBs);
        return -1;
    }

    switch (request->iqWidth) {
        case 8:
        case 9:
        case 10:
        case 12:
        switch (request->numDataElements) {
            case 16:
                com_fn = BlockFloatCompander::BFPCompressCtrlPlane8Avx512;
                break;
            case 32:
                com_fn = BlockFloatCompander::BFPCompressCtrlPlane16Avx512;
                break;
            case 64:
                com_fn = BlockFloatCompander::BFPCompressCtrlPlane32Avx512;
                break;
            case 128:
                com_fn = BlockFloatCompander::BFPCompressCtrlPlane64Avx512;
                break;
            case 24:
            default:
                printf("Unsupported numDataElements %d\n", request->numDataElements);
                return -1;
                break;
        }
        break;
    default:
        printf("Unsupported iqWidth %d\n", request->iqWidth);
        return -1;
        break;
    }

    expandedDataInput.iqWidth         = request->iqWidth;
    expandedDataInput.numDataElements = request->numDataElements;
    expandedDataInput.numBlocks       = 1;
    expandedDataInput.dataExpanded    = &request->data_in[0];
    compressedDataOut.dataCompressed  = (uint8_t*)&response->data_out[0];

    com_fn(expandedDataInput, &compressedDataOut);

    response->len =  (((expandedDataInput.numDataElements  * expandedDataInput.iqWidth) >> 3) + 1)
                            * request->numRBs;

    return 0;
}

int32_t
xranlib_decompress_avx512_bfw(const struct xranlib_decompress_request *request,
                        struct xranlib_decompress_response *response)
{
    BlockFloatCompander::CompressedData compressedDataInput;
    BlockFloatCompander::ExpandedData expandedDataOut;
    xran_bfp_decompress_fn decom_fn = NULL;

    if (request->numRBs != 1){
        printf("Unsupported numRBs %d\n", request->numRBs);
        return -1;
    }

    switch (request->iqWidth) {
        case 8:
        case 9:
        case 10:
        case 12:
        switch (request->numDataElements) {
            case 16:
                decom_fn = BlockFloatCompander::BFPExpandCtrlPlane8Avx512;
                break;
            case 32:
                decom_fn = BlockFloatCompander::BFPExpandCtrlPlane16Avx512;
                break;
            case 64:
                decom_fn = BlockFloatCompander::BFPExpandCtrlPlane32Avx512;
                break;
            case 128:
                decom_fn = BlockFloatCompander::BFPExpandCtrlPlane64Avx512;
                break;
            case 24:
            default:
                printf("Unsupported numDataElements %d\n", request->numDataElements);
                return -1;
                break;
        }
        break;
    default:
        printf("Unsupported iqWidth %d\n", request->iqWidth);
        return -1;
        break;
    }

    compressedDataInput.iqWidth         = request->iqWidth;
    compressedDataInput.numDataElements = request->numDataElements;
    compressedDataInput.numBlocks       = 1;
    compressedDataInput.dataCompressed  = (uint8_t*)&request->data_in[0];
    expandedDataOut.dataExpanded        = &response->data_out[0];

    decom_fn(compressedDataInput, &expandedDataOut);

    response->len = request->numRBs * compressedDataInput.numDataElements * sizeof(int16_t);

    return 0;
}

