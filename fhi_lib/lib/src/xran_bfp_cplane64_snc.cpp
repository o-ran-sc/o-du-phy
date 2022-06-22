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
 * @brief xRAN BFP compression/decompression for C-plane with 64T64R
 *
 * @file xran_bfp_cplane64.cpp
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#include "xran_compression.hpp"
#include "xran_bfp_utils.hpp"
#include "xran_bfp_byte_packing_utils.hpp"
#include <complex>
#include <algorithm>
#include <immintrin.h>


namespace BFP_CPlane_64_SNC
{
  /// Namespace constants
  const int k_numDataElements = 128; /// 16 IQ pairs
  const int k_numRegsPerBlock = 4; /// Number of AVX512 registers per compression block (input)

  inline int
  maxAbsOneBlock(const __m512i* inData)
  {
    /// Vertical maxAbs on all registers
    __m512i maxAbsReg = __m512i();
#pragma unroll(k_numRegsPerBlock)
    for (int n = 0; n < k_numRegsPerBlock; ++n)
    {
      const auto thisRegAbs = _mm512_abs_epi16(inData[n]);
      maxAbsReg = _mm512_max_epi16(thisRegAbs, maxAbsReg);
    }
    /// Horizontal max across remaining register
    return BlockFloatCompander::horizontalMax1x32(maxAbsReg);
  }

  /// Compute exponent value for a set of 16 RB from the maximum absolute value
  inline __m512i
  computeExponent_16RB(const BlockFloatCompander::ExpandedData& dataIn, const __m512i totShiftBits)
  {
    __m512i maxAbs = __m512i();
    const __m512i* dataInAddr = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
#pragma unroll(16)
    for (int n = 0; n < 16; ++n)
    {
      ((uint32_t*)&maxAbs)[n] = maxAbsOneBlock(dataInAddr + n * k_numRegsPerBlock);
    }
    /// Calculate exponent
    return BlockFloatCompander::expLzCnt(maxAbs, totShiftBits);
  }

  /// Compute exponent value for a set of 4 RB from the maximum absolute value
  inline __m512i
  computeExponent_4RB(const BlockFloatCompander::ExpandedData& dataIn, const __m512i totShiftBits)
  {
    __m512i maxAbs = __m512i();
    const __m512i* dataInAddr = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
#pragma unroll(4)
    for (int n = 0; n < 4; ++n)
    {
      ((uint32_t*)&maxAbs)[n] = maxAbsOneBlock(dataInAddr + n * k_numRegsPerBlock);
    }
    /// Calculate exponent
    return BlockFloatCompander::expLzCnt(maxAbs, totShiftBits);
  }

  /// Compute exponent value for 1 RB from the maximum absolute value
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
                        const int iqWidth, const uint8_t thisExp, const int totNumBytesPerReg, const uint64_t rbWriteMask)
  {
    /// Store exponent first
    *outBlockAddr = thisExp;
#pragma unroll(k_numRegsPerBlock)
    for (int n = 0; n < k_numRegsPerBlock; ++n)
    {
      /// Apply the exponent shift
      const auto compData = _mm512_srai_epi16(dataIn[n], thisExp);
      /// Pack compressed data network byte order
      const auto compDataBytePacked = networkBytePack(compData);
      /// Store compressed data
      _mm512_mask_storeu_epi8(outBlockAddr + 1 + n * totNumBytesPerReg, rbWriteMask, compDataBytePacked);
    }
  }

  /// Derive and apply 9, 10, or 12bit compression to 16 compression blocks
  template<BlockFloatCompander::PackFunction networkBytePack>
  inline void
  compressN_16RB(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut,
                 const __m512i totShiftBits, const int totNumBytesPerBlock, const int totNumBytesPerReg, const uint64_t rbWriteMask)
  {
    const auto exponents = computeExponent_16RB(dataIn, totShiftBits);
    const __m512i* dataInAddr = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
#pragma unroll(16)
    for (int n = 0; n < 16; ++n)
    {
      applyCompressionN_1RB<networkBytePack>(dataInAddr + n * k_numRegsPerBlock, dataOut->dataCompressed + n * totNumBytesPerBlock, dataIn.iqWidth, ((uint8_t*)&exponents)[n * 4], totNumBytesPerReg, rbWriteMask);
    }
  }

  /// Derive and apply 9, 10, or 12bit compression to 4 compression blocks
  template<BlockFloatCompander::PackFunction networkBytePack>
  inline void
  compressN_4RB(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut,
                const __m512i totShiftBits, const int totNumBytesPerBlock, const int totNumBytesPerReg, const uint64_t rbWriteMask)
  {
    const auto exponents = computeExponent_4RB(dataIn, totShiftBits);
    const __m512i* dataInAddr = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
#pragma unroll(4)
    for (int n = 0; n < 4; ++n)
    {
      applyCompressionN_1RB<networkBytePack>(dataInAddr + n * k_numRegsPerBlock, dataOut->dataCompressed + n * totNumBytesPerBlock, dataIn.iqWidth, ((uint8_t*)&exponents)[n * 4], totNumBytesPerReg, rbWriteMask);
    }
  }

  /// Derive and apply 9, 10, or 12bit compression to 1 RB
  template<BlockFloatCompander::PackFunction networkBytePack>
  inline void
  compressN_1RB(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut,
                const __m512i totShiftBits, const int totNumBytesPerBlock, const int totNumBytesPerReg, const uint64_t rbWriteMask)
  {
    const auto thisExponent = computeExponent_1RB(dataIn, totShiftBits);
    const __m512i* dataInAddr = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
    applyCompressionN_1RB<networkBytePack>(dataInAddr, dataOut->dataCompressed, dataIn.iqWidth, thisExponent, totNumBytesPerReg, rbWriteMask);
  }

  /// Calls compression function specific to the number of blocks to be executed. For 9, 10, or 12bit iqWidth.
  template<BlockFloatCompander::PackFunction networkBytePack>
  inline void
  compressByAllocN(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut,
                   const __m512i totShiftBits, const int totNumBytesPerBlock, const int totNumBytesPerReg, const uint64_t rbWriteMask)
  {
    switch (dataIn.numBlocks)
    {
    case 16:
      compressN_16RB<networkBytePack>(dataIn, dataOut, totShiftBits, totNumBytesPerBlock, totNumBytesPerReg, rbWriteMask);
      break;

    case 4:
      compressN_4RB<networkBytePack>(dataIn, dataOut, totShiftBits, totNumBytesPerBlock, totNumBytesPerReg, rbWriteMask);
      break;

    case 1:
      compressN_1RB<networkBytePack>(dataIn, dataOut, totShiftBits, totNumBytesPerBlock, totNumBytesPerReg, rbWriteMask);
      break;
    }
  }



  /// Apply 8b compression to 1 compression block.
  inline void
  applyCompression8_1RB(const __m512i* dataIn, uint8_t* outBlockAddr, const uint8_t thisExp)
  {
    /// Store exponent first
    *outBlockAddr = thisExp;
    constexpr uint32_t k_writeMask = 0xFFFFFFFF;
    __m256i* regOutAddr = reinterpret_cast<__m256i*>(outBlockAddr + 1);
#pragma unroll(k_numRegsPerBlock)
    for (int n = 0; n < k_numRegsPerBlock; ++n)
    {
      /// Apply the exponent shift
      const auto compData = _mm512_srai_epi16(dataIn[n], thisExp);
      /// Truncate to 8bit and store
      _mm256_mask_storeu_epi8(regOutAddr + n, k_writeMask, _mm512_cvtepi16_epi8(compData));
    }
  }

  /// Derive and apply 8b compression to 16 compression blocks
  inline void
  compress8_16RB(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut, const __m512i totShiftBits)
  {
    const __m512i exponents = computeExponent_16RB(dataIn, totShiftBits);
    const __m512i* dataInAddr = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
#pragma unroll(16)
    for (int n = 0; n < 16; ++n)
    {
      applyCompression8_1RB(dataInAddr + n * k_numRegsPerBlock, dataOut->dataCompressed + n * (k_numDataElements + 1), ((uint8_t*)&exponents)[n * 4]);
    }
  }

  /// Derive and apply 8b compression to 4 compression blocks
  inline void
  compress8_4RB(const BlockFloatCompander::ExpandedData& dataIn, BlockFloatCompander::CompressedData* dataOut, const __m512i totShiftBits)
  {
    const __m512i exponents = computeExponent_4RB(dataIn, totShiftBits);
    const __m512i* dataInAddr = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
#pragma unroll(4)
    for (int n = 0; n < 4; ++n)
    {
      applyCompression8_1RB(dataInAddr + n * k_numRegsPerBlock, dataOut->dataCompressed + n * (k_numDataElements + 1), ((uint8_t*)&exponents)[n * 4]);
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
  applyExpansionN_1RB(const uint8_t* expAddr, __m512i* dataOutAddr, const int maxExpShift, const int totNumBytesPerReg)
  {
    static constexpr uint8_t k_WriteMask = 0xFF;
    const auto thisExpShift = maxExpShift - *expAddr;
#pragma unroll(k_numRegsPerBlock)
    for (int n = 0; n < k_numRegsPerBlock; ++n)
    {
      const auto thisInRegAddr = expAddr + 1 + n * totNumBytesPerReg;
      /// Unpack network order packed data
      const auto inDataUnpacked = networkByteUnpack(thisInRegAddr);
      /// Apply exponent scaling (by appropriate arithmetic shift right)
      const auto expandedData = _mm512_srai_epi16(inDataUnpacked, thisExpShift);
      /// Write expanded data to output
      _mm512_mask_storeu_epi64(dataOutAddr + n, k_WriteMask, expandedData);
    }
  }

  /// Calls expansion function specific to the number of blocks to be executed. For 9, 10, or 12bit iqWidth.
  template<BlockFloatCompander::UnpackFunction networkByteUnpack>
  void expandByAllocN(const BlockFloatCompander::CompressedData& dataIn, BlockFloatCompander::ExpandedData* dataOut,
                      const int totNumBytesPerBlock, const int totNumBytesPerReg, const int maxExpShift)
  {
    __m512i* dataOutAddr = reinterpret_cast<__m512i*>(dataOut->dataExpanded);
    switch (dataIn.numBlocks)
    {
    case 16:
#pragma unroll(16)
      for (int n = 0; n < 16; ++n)
      {
        applyExpansionN_1RB<networkByteUnpack>(dataIn.dataCompressed + n * totNumBytesPerBlock, dataOutAddr + n * k_numRegsPerBlock, maxExpShift, totNumBytesPerReg);
      }
      break;

    case 4:
#pragma unroll(4)
      for (int n = 0; n < 4; ++n)
      {
        applyExpansionN_1RB<networkByteUnpack>(dataIn.dataCompressed + n * totNumBytesPerBlock, dataOutAddr + n * k_numRegsPerBlock, maxExpShift, totNumBytesPerReg);
      }
      break;

    case 1:
      applyExpansionN_1RB<networkByteUnpack>(dataIn.dataCompressed, dataOutAddr, maxExpShift, totNumBytesPerReg);
      break;
    }
  }


  /// Apply expansion to 1 compression block
  inline void
  applyExpansion8_1RB(const uint8_t* expAddr, __m512i* dataOutAddr)
  {
    const __m256i* rawDataIn = reinterpret_cast<const __m256i*>(expAddr + 1);
    static constexpr uint8_t k_WriteMask = 0xFF;
#pragma unroll(k_numRegsPerBlock)
    for (int n = 0; n < k_numRegsPerBlock; ++n)
    {
      const auto compData16 = _mm512_cvtepi8_epi16(rawDataIn[n]);
      const auto expData = _mm512_slli_epi16(compData16, *expAddr);
      _mm512_mask_storeu_epi64(dataOutAddr + n, k_WriteMask, expData);
    }
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
        applyExpansion8_1RB(dataIn.dataCompressed + n * (k_numDataElements + 1), dataOutAddr + n * k_numRegsPerBlock);
      }
      break;

    case 4:
#pragma unroll(4)
      for (int n = 0; n < 4; ++n)
      {
        applyExpansion8_1RB(dataIn.dataCompressed + n * (k_numDataElements + 1), dataOutAddr + n * k_numRegsPerBlock);
      }
      break;

    case 1:
      applyExpansion8_1RB(dataIn.dataCompressed, dataOutAddr);
      break;
    }
  }
}


/// Main kernel function for 64 antenna C-plane compression.
/// Starts by determining iqWidth specific parameters and functions.
void
BlockFloatCompander::BFPCompressCtrlPlane64AvxSnc(const ExpandedData& dataIn, CompressedData* dataOut)
{
  /// Compensation for extra zeros in 32b leading zero count when computing exponent
  const auto totShiftBits8 = _mm512_set1_epi32(25);
  const auto totShiftBits9 = _mm512_set1_epi32(24);
  const auto totShiftBits10 = _mm512_set1_epi32(23);
  const auto totShiftBits12 = _mm512_set1_epi32(21);

  /// Total number of data bytes per compression block is (iqWidth * numElements / 8) + 1
  const auto totNumBytesPerBlock = ((BFP_CPlane_64_SNC::k_numDataElements * dataIn.iqWidth) >> 3) + 1;
  /// Total number of compressed bytes to handle per register is 32 * iqWidth / 8
  const auto totNumBytesPerReg = dataIn.iqWidth << 2;

  /// Compressed data write mask for each iqWidth option
  constexpr uint64_t rbWriteMask9 = 0x0000000FFFFFFFFF;
  constexpr uint64_t rbWriteMask10 = 0x000000FFFFFFFFFF;
  constexpr uint64_t rbWriteMask12 = 0x0000FFFFFFFFFFFF;

  switch (dataIn.iqWidth)
  {
  case 8:
    BFP_CPlane_64_SNC::compressByAlloc8(dataIn, dataOut, totShiftBits8);
    break;

  case 9:
    BFP_CPlane_64_SNC::compressByAllocN<BlockFloatCompander::networkBytePack9bSnc>(dataIn, dataOut, totShiftBits9, totNumBytesPerBlock, totNumBytesPerReg, rbWriteMask9);
    break;

  case 10:
    BFP_CPlane_64_SNC::compressByAllocN<BlockFloatCompander::networkBytePack10bSnc>(dataIn, dataOut, totShiftBits10, totNumBytesPerBlock, totNumBytesPerReg, rbWriteMask10);
    break;

  case 12:
    BFP_CPlane_64_SNC::compressByAllocN<BlockFloatCompander::networkBytePack12bSnc>(dataIn, dataOut, totShiftBits12, totNumBytesPerBlock, totNumBytesPerReg, rbWriteMask12);
    break;
  }
}


/// Main kernel function for 64 antenna C-plane expansion.
/// Starts by determining iqWidth specific parameters and functions.
void
BlockFloatCompander::BFPExpandCtrlPlane64AvxSnc(const CompressedData& dataIn, ExpandedData* dataOut)
{
  constexpr int k_maxExpShift9 = 7;
  constexpr int k_maxExpShift10 = 6;
  constexpr int k_maxExpShift12 = 4;

  /// Total number of data bytes per compression block is (iqWidth * numElements / 8) + 1
  const auto totNumBytesPerBlock = ((BFP_CPlane_64_SNC::k_numDataElements * dataIn.iqWidth) >> 3) + 1;
  /// Total number of compressed bytes to handle per register is 32 * iqWidth / 8
  const auto totNumBytesPerReg = dataIn.iqWidth << 2;

  switch (dataIn.iqWidth)
  {
  case 8:
    BFP_CPlane_64_SNC::expandByAlloc8(dataIn, dataOut);
    break;

  case 9:
    BFP_CPlane_64_SNC::expandByAllocN<BlockFloatCompander::networkByteUnpack9bSnc>(dataIn, dataOut, totNumBytesPerBlock, totNumBytesPerReg, k_maxExpShift9);
    break;

  case 10:
    BFP_CPlane_64_SNC::expandByAllocN<BlockFloatCompander::networkByteUnpack10bSnc>(dataIn, dataOut, totNumBytesPerBlock, totNumBytesPerReg, k_maxExpShift10);
    break;

  case 12:
    BFP_CPlane_64_SNC::expandByAllocN<BlockFloatCompander::networkByteUnpack12bSnc>(dataIn, dataOut, totNumBytesPerBlock, totNumBytesPerReg, k_maxExpShift12);
    break;
  }
}
