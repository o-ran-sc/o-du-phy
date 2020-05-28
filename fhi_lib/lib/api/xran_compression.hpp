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
#define ASSUME_CACHE_ALIGNED(data) __assume_aligned(data, k_cacheByteAlignment);

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
  static constexpr int k_numRB = 16;
  static constexpr int k_numRE = 12;
  static constexpr int k_numREReal = k_numRE * 2;
  static constexpr int k_numSampsExpanded = k_numRB * k_numREReal;
  static constexpr int k_numSampsCompressed = (k_numSampsExpanded * 2) + k_numRB;

  struct CompressedData
  {
    /// Compressed data
    CACHE_ALIGNED uint8_t dataCompressedDataOut[k_numSampsCompressed];
    CACHE_ALIGNED uint8_t *dataCompressed;
    /// Size of mantissa including sign bit
    int iqWidth;
  };

  struct ExpandedData
  {
    /// Expanded data or input data to compressor
    CACHE_ALIGNED int16_t dataExpandedIn[k_numSampsExpanded];
    CACHE_ALIGNED int16_t *dataExpanded;

    /// Size of mantissa including sign bit
    int iqWidth;
  };

  void BlockFloatCompress_Basic(const ExpandedData& dataIn, CompressedData* dataOut);
  void BlockFloatCompress_8b_AVX512(const ExpandedData& dataIn, CompressedData* dataOut);
  void BlockFloatCompress_9b_AVX512(const ExpandedData& dataIn, CompressedData* dataOut);
  void BlockFloatCompress_10b_AVX512(const ExpandedData& dataIn, CompressedData* dataOut);
  void BlockFloatCompress_12b_AVX512(const ExpandedData& dataIn, CompressedData* dataOut);

  void BlockFloatExpand_Basic(const CompressedData& dataIn, ExpandedData* dataOut);
  void BlockFloatExpand_8b_AVX512(const CompressedData& dataIn, ExpandedData* dataOut);
  void BlockFloatExpand_9b_AVX512(const CompressedData& dataIn, ExpandedData* dataOut);
  void BlockFloatExpand_10b_AVX512(const CompressedData& dataIn, ExpandedData* dataOut);
  void BlockFloatExpand_12b_AVX512(const CompressedData& dataIn, ExpandedData* dataOut);
}

namespace BlockFloatCompanderBFW
{
  /// Compute 32 RB at a time
  static constexpr int k_numBitsIQ = 16;
  static constexpr int k_numRB = 1;
  static constexpr int k_numRE = 32;
  static constexpr int k_numREReal = k_numRE * 2;
  static constexpr int k_numSampsExpanded = k_numRB * k_numREReal;
  static constexpr int k_numSampsCompressed = (k_numSampsExpanded * 2) + k_numRB;

  struct CompressedData
  {
    /// Compressed data
    CACHE_ALIGNED uint8_t dataCompressedDataOut[k_numSampsCompressed];
    CACHE_ALIGNED uint8_t *dataCompressed;
    /// Size of mantissa including sign bit
    int iqWidth;
  };

  struct ExpandedData
  {
    /// Expanded data or input data to compressor
    CACHE_ALIGNED int16_t dataExpandedIn[k_numSampsExpanded];
    CACHE_ALIGNED int16_t *dataExpanded;

    /// Size of mantissa including sign bit
    int iqWidth;
  };

  void BlockFloatCompress_Basic(const ExpandedData& dataIn, CompressedData* dataOut);
/*  void BlockFloatCompress_8b_AVX512(const ExpandedData& dataIn, CompressedData* dataOut);
  void BlockFloatCompress_9b_AVX512(const ExpandedData& dataIn, CompressedData* dataOut);
  void BlockFloatCompress_10b_AVX512(const ExpandedData& dataIn, CompressedData* dataOut);
  void BlockFloatCompress_12b_AVX512(const ExpandedData& dataIn, CompressedData* dataOut); */

  void BlockFloatExpand_Basic(const CompressedData& dataIn, ExpandedData* dataOut);
/*  void BlockFloatExpand_8b_AVX512(const CompressedData& dataIn, ExpandedData* dataOut);
  void BlockFloatExpand_9b_AVX512(const CompressedData& dataIn, ExpandedData* dataOut);
  void BlockFloatExpand_10b_AVX512(const CompressedData& dataIn, ExpandedData* dataOut);
  void BlockFloatExpand_12b_AVX512(const CompressedData& dataIn, ExpandedData* dataOut);*/
}

