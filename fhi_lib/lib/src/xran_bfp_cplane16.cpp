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
 * @brief xRAN BFP compression/decompression for C-plane with 16T16R
 *
 * @file xran_bfp_cplane16.cpp
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#include "xran_compression.hpp"
#include "xran_bfp_utils.hpp"
#include "xran_bfp_byte_packing_utils.hpp"
#include <complex>
#include <algorithm>
#include <immintrin.h>


namespace BFP_CPlane_16
{
  /// Namespace constants
  const int k_numDataElements = 32; /// 16 IQ pairs

  inline int
  maxAbsOneBlock(const __m512i* inData)
  {
    /// Compute abs of input data
    const auto thisRegAbs = _mm512_abs_epi16(*inData);
    /// Horizontal max across register
    return BlockFloatCompander::horizontalMax1x32(thisRegAbs);
  }

  /// Compute exponent value for a set of 16 RB from the maximum absolute value.
  inline __m512i
  computeExponent_16RB(const BlockFloatCompander::ExpandedData& dataIn, const __m512i totShiftBits)
  {
    __m512i maxAbs = __m512i();
    const __m512i* dataInAddr = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
#pragma unroll(16)
    for (int n = 0; n < 16; ++n)
    {
      ((uint32_t*)&maxAbs)[n] = maxAbsOneBlock(dataInAddr + n);
    }
    /// Calculate exponent
    return BlockFloatCompander::expLzCnt(maxAbs, totShiftBits);
  }

  /// Compute exponent value for a set of 4 RB from the maximum absolute value.
  inline __m512i
  computeExponent_4RB(const BlockFloatCompander::ExpandedData& dataIn, const __m512i totShiftBits)
  {
    __m512i maxAbs = __m512i();
    const __m512i* dataInAddr = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
#pragma unroll(4)
    for (int n = 0; n < 4; ++n)
    {
      ((uint32_t*)&maxAbs)[n] = maxAbsOneBlock(dataInAddr + n);
    }
    /// Calculate exponent
    return BlockFloatCompander::expLzCnt(maxAbs, totShiftBits);
  }

  /// Compute exponent value for 1 RB from the maximum absolute value.
  inline uint8_t
  computeExponent_1RB(const BlockFloatCompander::ExpandedData& dataIn, const __m512i totShiftBits)
  {
    __m512i maxAbs = __m512i();
    const __m512i* dataInAddr = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
    ((uint32_t*)&maxAbs)[0] = maxAbsOneBlock(dataInAddr);
    /// Calculate exponent
    const auto exps = BlockFloatCompander::expLzCnt(maxAbs, totShiftBits);
    return ((uint8_t*)&exps)[0];
  }



  /// Apply compression to one compression block
  template<BlockFloatCompander::PackFunction networkBytePack>
  inline void
  applyCompressionN_1RB(const __m512i* dataIn, uint8_t* outBlockAddr,
                        const int iqWidth, const uint8_t thisExp, const uint16_t rbWriteMask)
  {
    /// Store exponent first
    *outBlockAddr = thisExp;
    /// Apply the exponent shift
    const auto compData = _mm512_srai_epi16(*dataIn, thisExp);
    /// Pack compressed data network byte order
    const auto compDataBytePacked = networkBytePack(compData);
    /// Now have 1 register worth of bytes separated into 4 chunks (1 per lane)
    /// Use four offset stores to join
    const auto thisOutRegAddr = outBlockAddr + 1;
    _mm_mask_storeu_epi8(thisOutRegAddr, rbWriteMask, _mm512_extracti64x2_epi64(compDataBytePacked, 0));
    _mm_mask_storeu_epi8(thisOutRegAddr + iqWidth, rbWriteMask, _mm512_extracti64x2_epi64(compDataBytePacked, 1));
    _mm_mask_storeu_epi8(thisOutRegAddr + (2 * iqWidth), rbWriteMask, _mm512_extracti64x2_epi64(compDataBytePacked, 2));
    _mm_mask_storeu_epi8(thisOutRegAddr + (3 * iqWidth), rbWriteMask, _mm512_extracti64x2_epi64(compDataBytePacked, 3));
  }

  /// Derive and apply 9, 10, or 12bit compression to 16 compression blocks
  template<BlockFloatCompander::PackFunction networkBytePack>
  inline void
  compressN_16RB(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut,
                 const __m512i totShiftBits, const int totNumBytesPerBlock, const uint16_t rbWriteMask)
  {
    const auto exponents = computeExponent_16RB(dataIn, totShiftBits);
    const __m512i* dataInAddr = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
#pragma unroll(16)
    for (int n = 0; n < 16; ++n)
    {
      applyCompressionN_1RB<networkBytePack>(dataInAddr + n, dataOut->dataCompressed + n * totNumBytesPerBlock, dataIn.iqWidth, ((uint8_t*)&exponents)[n * 4], rbWriteMask);
    }
  }

  /// Derive and apply 9, 10, or 12bit compression to 4 compression blocks
  template<BlockFloatCompander::PackFunction networkBytePack>
  inline void
  compressN_4RB(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut,
                const __m512i totShiftBits, const int totNumBytesPerBlock, const uint16_t rbWriteMask)
  {
    const auto exponents = computeExponent_4RB(dataIn, totShiftBits);
    const __m512i* dataInAddr = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
#pragma unroll(4)
    for (int n = 0; n < 4; ++n)
    {
      applyCompressionN_1RB<networkBytePack>(dataInAddr + n, dataOut->dataCompressed + n * totNumBytesPerBlock, dataIn.iqWidth, ((uint8_t*)&exponents)[n * 4], rbWriteMask);
    }
  }

  /// Derive and apply 9, 10, or 12bit compression to 1 RB
  template<BlockFloatCompander::PackFunction networkBytePack>
  inline void
  compressN_1RB(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut,
                const __m512i totShiftBits, const int totNumBytesPerBlock, const uint16_t rbWriteMask)
  {
    const auto thisExponent = computeExponent_1RB(dataIn, totShiftBits);
    const __m512i* dataInAddr = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
    applyCompressionN_1RB<networkBytePack>(dataInAddr, dataOut->dataCompressed, dataIn.iqWidth, thisExponent, rbWriteMask);
  }

  /// Calls compression function specific to the number of blocks to be executed. For 9, 10, or 12bit iqWidth.
  template<BlockFloatCompander::PackFunction networkBytePack>
  inline void
  compressByAllocN(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut,
                   const __m512i totShiftBits, const int totNumBytesPerBlock, const uint16_t rbWriteMask)
  {
    switch (dataIn.numBlocks)
    {
    case 16:
      compressN_16RB<networkBytePack>(dataIn, dataOut, totShiftBits, totNumBytesPerBlock, rbWriteMask);
      break;

    case 4:
      compressN_4RB<networkBytePack>(dataIn, dataOut, totShiftBits, totNumBytesPerBlock, rbWriteMask);
      break;

    case 1:
      compressN_1RB<networkBytePack>(dataIn, dataOut, totShiftBits, totNumBytesPerBlock, rbWriteMask);
      break;
    }
  }



  /// Apply 8b compression to 1 compression block.
  inline void
  applyCompression8_1RB(const __m512i* dataIn, uint8_t* outBlockAddr, const uint8_t thisExp)
  {
    /// Store exponent first
    *outBlockAddr = thisExp;
    /// Apply the exponent shift
    const auto compData = _mm512_srai_epi16(*dataIn, thisExp);
    /// Truncate to 8bit and store
    constexpr uint32_t k_writeMask = 0xFFFFFFFF;
    _mm256_mask_storeu_epi8(outBlockAddr + 1, k_writeMask, _mm512_cvtepi16_epi8(compData));
  }

  /// Derive and apply 8b compression to 16 compression blocks
  inline void
  compress8_16RB(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut, const __m512i totShiftBits)
  {
    const auto exponents = computeExponent_16RB(dataIn, totShiftBits);
    const __m512i* dataInAddr = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
#pragma unroll(16)
    for (int n = 0; n < 16; ++n)
    {
      applyCompression8_1RB(dataInAddr + n, dataOut->dataCompressed + n * (k_numDataElements + 1), ((uint8_t*)&exponents)[n * 4]);
    }
  }

  /// Derive and apply 8b compression to 4 compression blocks
  inline void
  compress8_4RB(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut, const __m512i totShiftBits)
  {
    const auto exponents = computeExponent_4RB(dataIn, totShiftBits);
    const __m512i* dataInAddr = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
#pragma unroll(4)
    for (int n = 0; n < 4; ++n)
    {
      applyCompression8_1RB(dataInAddr + n, dataOut->dataCompressed + n * (k_numDataElements + 1), ((uint8_t*)&exponents)[n * 4]);
    }
  }

  /// Derive and apply 8b compression to 1 compression block
  inline void
  compress8_1RB(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut, const __m512i totShiftBits)
  {
    const auto thisExponent = computeExponent_1RB(dataIn, totShiftBits);
    const __m512i* dataInAddr = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
    applyCompression8_1RB(dataInAddr, dataOut->dataCompressed, thisExponent);
  }

  /// Calls compression function specific to the number of RB to be executed. For 8 bit iqWidth.
  inline void
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



  /// Expand 1 compression block
  template<BlockFloatCompander::UnpackFunction networkByteUnpack>
  inline void
  applyExpansionN_1RB(const uint8_t* expAddr, __m512i* dataOutAddr, const int maxExpShift)
  {
    const auto thisExpShift = maxExpShift - *expAddr;
    /// Unpack network order packed data
    const auto inDataUnpacked = networkByteUnpack(expAddr + 1);
    /// Apply exponent scaling (by appropriate arithmetic shift right)
    const auto expandedData = _mm512_srai_epi16(inDataUnpacked, thisExpShift);
    /// Write expanded data to output
    static constexpr uint8_t k_WriteMask = 0xFF;
    _mm512_mask_storeu_epi64(dataOutAddr, k_WriteMask, expandedData);
  }

  /// Calls expansion function specific to the number of blocks to be executed. For 9, 10, or 12bit iqWidth.
  template<BlockFloatCompander::UnpackFunction networkByteUnpack>
  void
  expandByAllocN(const BlockFloatCompander::CompressedData& dataIn, BlockFloatCompander::ExpandedData* dataOut,
                 const int totNumBytesPerBlock, const int maxExpShift)
  {
    __m512i* dataOutAddr = reinterpret_cast<__m512i*>(dataOut->dataExpanded);
    switch (dataIn.numBlocks)
    {
    case 16:
#pragma unroll(16)
      for (int n = 0; n < 16; ++n)
      {
        applyExpansionN_1RB<networkByteUnpack>(dataIn.dataCompressed + n * totNumBytesPerBlock, dataOutAddr + n, maxExpShift);
      }
      break;

    case 4:
#pragma unroll(4)
      for (int n = 0; n < 4; ++n)
      {
        applyExpansionN_1RB<networkByteUnpack>(dataIn.dataCompressed + n * totNumBytesPerBlock, dataOutAddr + n, maxExpShift);
      }
      break;

    case 1:
      applyExpansionN_1RB<networkByteUnpack>(dataIn.dataCompressed, dataOutAddr, maxExpShift);
      break;
    }
  }


  /// Apply expansion to 1 compression block
  inline void
  applyExpansion8_1RB(const uint8_t* expAddr, __m512i* dataOutAddr)
  {
    const __m256i* rawDataIn = reinterpret_cast<const __m256i*>(expAddr + 1);
    const auto compData16 = _mm512_cvtepi8_epi16(_mm256_loadu_si256(rawDataIn));
    const auto expData = _mm512_slli_epi16(compData16, *expAddr);
    static constexpr uint8_t k_WriteMask = 0xFF;
    _mm512_mask_storeu_epi64(dataOutAddr, k_WriteMask, expData);
  }

  /// Calls expansion function specific to the number of RB to be executed. For 8 bit iqWidth.
  void
  expandByAlloc8(const BlockFloatCompander::CompressedData& dataIn, BlockFloatCompander::ExpandedData* dataOut)
  {
    __m512i* dataOutAddr = reinterpret_cast<__m512i*>(dataOut->dataExpanded);
    switch (dataIn.numBlocks)
    {
    case 16:
#pragma unroll(16)
      for (int n = 0; n < 16; ++n)
      {
        applyExpansion8_1RB(dataIn.dataCompressed + n * (k_numDataElements + 1), dataOutAddr + n);
      }
      break;

    case 4:
#pragma unroll(4)
      for (int n = 0; n < 4; ++n)
      {
        applyExpansion8_1RB(dataIn.dataCompressed + n * (k_numDataElements + 1), dataOutAddr + n);
      }
      break;

    case 1:
      applyExpansion8_1RB(dataIn.dataCompressed, dataOutAddr);
      break;
    }
  }
}


/// Main kernel function for 16 antenna C-plane compression.
/// Starts by determining iqWidth specific parameters and functions.
void
BlockFloatCompander::BFPCompressCtrlPlane16Avx512(const ExpandedData& dataIn, CompressedData* dataOut)
{
  /// Compensation for extra zeros in 32b leading zero count when computing exponent
  const auto totShiftBits8 = _mm512_set1_epi32(25);
  const auto totShiftBits9 = _mm512_set1_epi32(24);
  const auto totShiftBits10 = _mm512_set1_epi32(23);
  const auto totShiftBits12 = _mm512_set1_epi32(21);

  /// Total number of data bytes per compression block is (iqWidth * numElements / 8) + 1
  const auto totNumBytesPerBlock = ((BFP_CPlane_16::k_numDataElements * dataIn.iqWidth) >> 3) + 1;

  /// Compressed data write mask for each iqWidth option
  constexpr uint16_t rbWriteMask9 = 0x01FF;
  constexpr uint16_t rbWriteMask10 = 0x03FF;
  constexpr uint16_t rbWriteMask12 = 0x0FFF;

  switch (dataIn.iqWidth)
  {
  case 8:
    BFP_CPlane_16::compressByAlloc8(dataIn, dataOut, totShiftBits8);
    break;

  case 9:
    BFP_CPlane_16::compressByAllocN<BlockFloatCompander::networkBytePack9b>(dataIn, dataOut, totShiftBits9, totNumBytesPerBlock, rbWriteMask9);
    break;

  case 10:
    BFP_CPlane_16::compressByAllocN<BlockFloatCompander::networkBytePack10b>(dataIn, dataOut, totShiftBits10, totNumBytesPerBlock, rbWriteMask10);
    break;

  case 12:
    BFP_CPlane_16::compressByAllocN<BlockFloatCompander::networkBytePack12b>(dataIn, dataOut, totShiftBits12, totNumBytesPerBlock, rbWriteMask12);
    break;
  }
}


/// Main kernel function for 16 antenna C-plane  expansion.
/// Starts by determining iqWidth specific parameters and functions.
void
BlockFloatCompander::BFPExpandCtrlPlane16Avx512(const CompressedData& dataIn, ExpandedData* dataOut)
{
  constexpr int k_maxExpShift9 = 7;
  constexpr int k_maxExpShift10 = 6;
  constexpr int k_maxExpShift12 = 4;

  /// Total number of data bytes per compression block is (iqWidth * numElements / 8) + 1
  const auto totNumBytesPerBlock = ((BFP_CPlane_16::k_numDataElements * dataIn.iqWidth) >> 3) + 1;

  switch (dataIn.iqWidth)
  {
  case 8:
    BFP_CPlane_16::expandByAlloc8(dataIn, dataOut);
    break;

  case 9:
    BFP_CPlane_16::expandByAllocN<BlockFloatCompander::networkByteUnpack9b>(dataIn, dataOut, totNumBytesPerBlock, k_maxExpShift9);
    break;

  case 10:
    BFP_CPlane_16::expandByAllocN<BlockFloatCompander::networkByteUnpack10b>(dataIn, dataOut, totNumBytesPerBlock, k_maxExpShift10);
    break;

  case 12:
    BFP_CPlane_16::expandByAllocN<BlockFloatCompander::networkByteUnpack12b>(dataIn, dataOut, totNumBytesPerBlock, k_maxExpShift12);
    break;
  }
}
