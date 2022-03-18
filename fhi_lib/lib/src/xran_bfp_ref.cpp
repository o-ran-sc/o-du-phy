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
 * @brief xRAN BFP compression/decompression [C-code version, no AVX512 optimization]
 *
 * @file xran_bfp_ref.cpp
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#include "xran_compression.hpp"
#include "xran_bfp_utils.hpp"
#include <complex>
#include <algorithm>
#include <limits.h>

static int16_t saturateAbs(int16_t inVal)
{
  int16_t result;
  if (inVal == std::numeric_limits<int16_t>::min())
  {
    result = std::numeric_limits<int16_t>::max();
  }
  else
  {
    result = (int16_t)std::abs(inVal);
  }
  return result;
}


/// Reference compression
void
BlockFloatCompander::BFPCompressRef(const ExpandedData& dataIn, CompressedData* dataOut)
{
  int dataOutIdx = 0;
  int16_t iqMask = (int16_t)((1 << dataIn.iqWidth) - 1);
  int byteShiftUnits = dataIn.iqWidth - 8;

  for (int rb = 0; rb < dataIn.numBlocks; ++rb)
  {
    /// Find max abs value for this RB
    int16_t maxAbs = 0;
    for (int re = 0; re < dataIn.numDataElements; ++re)
    {
      auto dataIdx = rb * dataIn.numDataElements + re;
      auto dataAbs = saturateAbs(dataIn.dataExpanded[dataIdx]);
      maxAbs = std::max(maxAbs, dataAbs);
    }

    /// Find exponent and insert into byte stream
    auto thisExp = (uint8_t)(std::max(0,(16 - dataIn.iqWidth + 1 - __lzcnt16(maxAbs))));
    dataOut->dataCompressed[dataOutIdx++] = thisExp;

    /// ARS data by exponent and pack bytes in Network order
    /// This uses a sliding buffer where one or more bytes are
    /// extracted after the insertion of each compressed sample
    static constexpr int k_byteMask = 0xFF;
    int byteShiftVal = -8;
    int byteBuffer = { 0 };
    for (int re = 0; re < dataIn.numDataElements; ++re)
    {
      auto dataIdxIn = rb * dataIn.numDataElements + re;
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
  dataOut->numBlocks = dataIn.numBlocks;
  dataOut->numDataElements = dataIn.numDataElements;
}


/// Reference expansion
void
BlockFloatCompander::BFPExpandRef(const CompressedData& dataIn, ExpandedData* dataOut)
{
  uint32_t iqMask = (uint32_t)(UINT_MAX - ((1 << (32 - dataIn.iqWidth)) - 1));
  uint32_t byteBuffer = { 0 };
  int numBytesPerRB = ((dataIn.numDataElements * dataIn.iqWidth) >> 3) + 1;
  int bitPointer = 0;
  int dataIdxOut = 0;

  for (int rb = 0; rb < dataIn.numBlocks; ++rb)
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
  dataOut->iqWidth = dataIn.iqWidth;
  dataOut->numBlocks = dataIn.numBlocks;
  dataOut->numDataElements = dataIn.numDataElements;
}
