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

#pragma once
#include <stdint.h>
#ifndef RTE_ARCH_ARM64
#include <immintrin.h>
#elif defined(__arm__) || defined(__aarch64__)

#define SIMDE_ENABLE_NATIVE_ALIASES
#include <simde/x86/sse2.h>
#include <simde/x86/avx2.h>
#include <simde/x86/avx512.h>

#define _mm512_mask_shuffle_i64x2 simde_mm512_mask_shuffle_i64x2

#define _mm512_extracti64x2_epi64 simde_mm512_extracti64x2_epi64
static simde__m128i
simde_mm512_extracti64x2_epi64 (simde__m512i a, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 3) {
  simde__m512i_private a_ = simde__m512i_to_private(a);

  return a_.m128i[imm8 & 3];
}

#define  _mm_mask_storeu_epi8 simde_mm_mask_storeu_epi8
static void
simde_mm_mask_storeu_epi8(uint8_t mem_addr[HEDLEY_ARRAY_PARAM(16)], simde__mmask16 mask, simde__m128i a) {
    simde__m128i_private a_ = simde__m128i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.u8) / sizeof(a_.u8[0])) ; i++) {
      if ((mask >> i) & 1)
        mem_addr[i] = a_.u8[i];
    }
}

#define _mm256_mask_storeu_epi8(mem_addr, k, a) simde_mm256_mask_storeu_epi8((uint8_t *)mem_addr, k, a)
static void
simde_mm256_mask_storeu_epi8(uint8_t mem_addr[HEDLEY_ARRAY_PARAM(32)], simde__mmask32 mask, simde__m256i a) {
    simde__m256i_private a_ = simde__m256i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.u8) / sizeof(a_.u8[0])) ; i++) {
      if ((mask >> i) & 1)
        mem_addr[i] = a_.u8[i];
    }
}

#define _mm512_mask_storeu_epi16 simde_mm512_mask_storeu_epi16
static void
simde_mm512_mask_storeu_epi16(int16_t mem_addr[HEDLEY_ARRAY_PARAM(16)], simde__mmask32 mask, simde__m512i a) {
    simde__m512i_private a_ = simde__m512i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i16) / sizeof(a_.i16[0])) ; i++) {
      if ((mask >> i) & 1)
        mem_addr[i] = a_.i16[i];
    }
}

#define _mm512_mask_storeu_epi64(mem_addr, k, a) simde_mm512_mask_storeu_epi64((uint64_t *)mem_addr, k, a)
 static void
simde_mm512_mask_storeu_epi64(uint64_t mem_addr[HEDLEY_ARRAY_PARAM(8)], simde__mmask8 mask, simde__m512i a) {
    simde__m512i_private a_ = simde__m512i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.u64) / sizeof(a_.u64[0])) ; i++) {
      if ((mask >> i) & 1)
        mem_addr[i] = a_.u64[i];
    }
}

#define _mm512_mask_reduce_max_epi32 simde_mm512_mask_reduce_max_epi32
static int simde_mm512_mask_reduce_max_epi32(simde__mmask16 k, simde__m512i a) {
    simde__m512i_private
      a_ = simde__m512i_to_private(a);
    int _max = 0;

    SIMDE_VECTORIZE
      for (size_t i = 0; i < (sizeof(a_.i32) / sizeof(a_.i32[0])); i++) {
        _max = ((k >> i) & 1) ? ((a_.i32[i] > _max) ? a_.i32[i] : _max) : _max;
      }

    return _max;
}

#endif // x86_64 || i386

// This configuration file sets global constants and macros which are
// of general use throughout the project.

// All current IA processors of interest align their cache lines on
// this boundary. If the cache alignment for future processors changes
// then the most restrictive alignment should be set.
constexpr unsigned k_cacheByteAlignment = 64;

// Force the data to which this macro is applied to be aligned on a cache line.
// For example:
//
// CACHE_ALIGNED float data[64];
#define CACHE_ALIGNED alignas(k_cacheByteAlignment)

// Hint to the compiler that the data to which this macro is applied
// can be assumed to be aligned to a cache line. This allows the
// compiler to generate improved code by using aligned reads and
// writes.
#define ASSUME_CACHE_ALIGNED(data)
// __assume_aligned(data, k_cacheByteAlignment);

/// Intel compiler frequently complains about templates not being declared in an external
/// header. Templates are used throughout this project's source files to define local type-specific
/// versions of functions. Defining every one of these in a header is unnecessary, so the warnings
/// about this are turned off globally.
#pragma warning(disable:1418)
#pragma warning(disable:1419)


namespace BlockFloatCompander
{
  /// Compute 32 RB at a time
  static constexpr int k_numBitsIQ = 16;
  static constexpr int k_numBitsIQPair = 2 * k_numBitsIQ;
  static constexpr int k_maxNumBlocks = 16;
  static constexpr int k_maxNumElements = 128;
  static constexpr int k_numSampsExpanded = k_maxNumBlocks * k_maxNumElements;
  static constexpr int k_numSampsCompressed = (k_numSampsExpanded * 2) + k_maxNumBlocks;

  struct CompressedData
  {
    /// Compressed data
    CACHE_ALIGNED uint8_t dataCompressedDataOut[k_numSampsCompressed];
    CACHE_ALIGNED uint8_t *dataCompressed;
    /// Size of mantissa including sign bit
    int iqWidth;

    /// Number of BFP blocks in message
    int numBlocks;

    /// Number of data elements per compression block (only required for reference function)
    int numDataElements;
  };

  struct ExpandedData
  {
    /// Expanded data or input data to compressor
    CACHE_ALIGNED int16_t dataExpandedIn[k_numSampsExpanded];
    CACHE_ALIGNED int16_t *dataExpanded;

    /// Size of mantissa including sign bit
    int iqWidth;

    /// Number of BFP blocks in message
    int numBlocks;

    /// Number of data elements per compression block (only required for reference function)
    int numDataElements;
  };

  /// Reference compression and expansion functions
  void BFPCompressRef(const ExpandedData& dataIn, CompressedData* dataOut);
  void BFPExpandRef(const CompressedData& dataIn, ExpandedData* dataOut);

  /// User-Plane specific compression and expansion functions
  void BFPCompressUserPlaneAvx512(const ExpandedData& dataIn, CompressedData* dataOut);
  void BFPExpandUserPlaneAvx512(const CompressedData& dataIn, ExpandedData* dataOut);

  /// Control-Plane specific compression and expansion functions for 8 antennas
  void BFPCompressCtrlPlane8Avx512(const ExpandedData& dataIn, CompressedData* dataOut);
  void BFPExpandCtrlPlane8Avx512(const CompressedData& dataIn, ExpandedData* dataOut);

  /// Control-Plane specific compression and expansion functions for 16 antennas
  void BFPCompressCtrlPlane16Avx512(const ExpandedData& dataIn, CompressedData* dataOut);
  void BFPExpandCtrlPlane16Avx512(const CompressedData& dataIn, ExpandedData* dataOut);

  /// Control-Plane specific compression and expansion functions for 32 antennas
  void BFPCompressCtrlPlane32Avx512(const ExpandedData& dataIn, CompressedData* dataOut);
  void BFPExpandCtrlPlane32Avx512(const CompressedData& dataIn, ExpandedData* dataOut);

  /// Control-Plane specific compression and expansion functions for 64 antennas
  void BFPCompressCtrlPlane64Avx512(const ExpandedData& dataIn, CompressedData* dataOut);
  void BFPExpandCtrlPlane64Avx512(const CompressedData& dataIn, ExpandedData* dataOut);
}

