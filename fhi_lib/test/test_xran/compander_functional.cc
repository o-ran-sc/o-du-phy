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

#include "common.hpp"
#include "xran_fh_o_du.h"
#include "xran_compression.h"
#include "xran_compression.hpp"

#include <stdint.h>
#include <random>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <cstring>

const std::string module_name = "bfp";

template <typename T>
int checkData(T* inVec1, T* inVec2, int numVals)
{
  int checkSum = 0;
  for (int n = 0; n < numVals; ++n)
  {
    checkSum += std::abs(inVec1[n] - inVec2[n]);
  }
  if (checkSum == 0)
  {
    //std::cout << "Test Passed\n";
    return 0;
  }
  else
  {
    //std::cout << "Test Failed\n";
    return 1;
  }
}
template int checkData(int8_t*, int8_t*, int);
template int checkData(int16_t*, int16_t*, int);

int checkDataApprox(int16_t *inVec1, int16_t *inVec2, int numVals)
{
  int checkSum = 0;
  for (int n = 0; n < numVals; ++n)
  {
    if (std::abs(inVec1[n] & 0xFF00)   - std::abs(inVec2[n] & 0xFF00)){;
        printf("%d %d\n", inVec1[n] & 0xFF00, inVec2[n] & 0xFF00);
        checkSum += 1;
    }
  }
  if (checkSum == 0)
  {
    //std::cout << "Test Passed\n";
    return 0;
  }
  else
  {
    //std::cout << "Test Failed\n";
    return 1;
  }
}


class BfpCheck : public KernelTests
{
protected:
    void SetUp() override {
        init_test("bfp_functional");
    }

    /* It's called after an execution of the each test case.*/
    void TearDown() override {
    }
};

class BfpPerf : public KernelTests
{
protected:
    void SetUp() override {
        init_test("bfp_performace");
    }

    /* It's called after an execution of the each test case.*/
    void TearDown() override {
    }
};
CACHE_ALIGNED int16_t loc_dataExpandedIn[288*BlockFloatCompander::k_numREReal];
CACHE_ALIGNED int16_t loc_dataExpandedRes[288*BlockFloatCompander::k_numREReal];
CACHE_ALIGNED uint8_t loc_dataCompressedDataOut[2*288*BlockFloatCompander::k_numREReal];

class BfpPerfEx : public KernelTests
{
protected:
    struct xranlib_decompress_request  bfp_decom_req;
    struct xranlib_decompress_response bfp_decom_rsp;
    struct xranlib_compress_request  bfp_com_req;
    struct xranlib_compress_response bfp_com_rsp;

    void SetUp() override {
        init_test("bfp_performace_ex");
        int32_t resSum  = 0;
        int16_t len = 0;
        int16_t compMethod = XRAN_COMPMETHOD_BLKFLOAT;
        int16_t iqWidth    = get_input_parameter<int16_t>("iqWidth");
        int16_t numRBs = get_input_parameter<int16_t>("nRBsize");
        // Create random number generator
        std::random_device rd;
        std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
        std::uniform_int_distribution<int16_t> randInt16(-32768, 32767);
        std::uniform_int_distribution<int> randExpShift(0, 4);

        BlockFloatCompander::ExpandedData expandedData;
        expandedData.dataExpanded = &loc_dataExpandedIn[0];
        BlockFloatCompander::ExpandedData expandedDataRes;
        expandedDataRes.dataExpanded = &loc_dataExpandedRes[0];

        //printf("iqWidth %d numRBs %d\n", iqWidth, numRBs);

        for (int m = 0; m < 18*BlockFloatCompander::k_numRB; ++m) {
            auto shiftVal = randExpShift(gen);
            for (int n = 0; n < BlockFloatCompander::k_numREReal; ++n) {
                expandedData.dataExpanded[m*BlockFloatCompander::k_numREReal+n] = int16_t(randInt16(gen) >> shiftVal);
            }
        }

        BlockFloatCompander::CompressedData compressedData;
        compressedData.dataCompressed = &loc_dataCompressedDataOut[0];

        std::memset(&loc_dataCompressedDataOut[0], 0, 288*BlockFloatCompander::k_numREReal);
        std::memset(&loc_dataExpandedRes[0], 0, 288*BlockFloatCompander::k_numREReal);

        std::memset(&bfp_com_req, 0, sizeof(struct xranlib_compress_request));
        std::memset(&bfp_com_rsp, 0, sizeof(struct xranlib_compress_response));
        std::memset(&bfp_decom_req, 0, sizeof(struct xranlib_decompress_request));
        std::memset(&bfp_decom_rsp, 0, sizeof(struct xranlib_decompress_response));

        bfp_com_req.data_in    = (int16_t *)expandedData.dataExpanded;
        bfp_com_req.numRBs     = numRBs;
        bfp_com_req.len        = numRBs*12*2*2;
        bfp_com_req.compMethod = compMethod;
        bfp_com_req.iqWidth    = iqWidth;

        bfp_com_rsp.data_out   = (int8_t *)(compressedData.dataCompressed);
        bfp_com_rsp.len        = 0;

        bfp_decom_req.data_in    = (int8_t *)(compressedData.dataCompressed);
        bfp_decom_req.numRBs     = numRBs;
        bfp_decom_req.len        = ((3 * iqWidth) + 1) * numRBs;
        bfp_decom_req.compMethod = compMethod;
        bfp_decom_req.iqWidth    = iqWidth;

        bfp_decom_rsp.data_out   = (int16_t *)expandedDataRes.dataExpanded;
        bfp_decom_rsp.len        = 0;
    }

    /* It's called after an execution of the each test case.*/
    void TearDown() override {

    }
};

TEST_P(BfpCheck, AVX512_12bit)
{
  int resSum = 0;

  // Create random number generator
  std::random_device rd;
  std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<int16_t> randInt16(-32768, 32767);
  std::uniform_int_distribution<int> randExpShift(0, 4);

  // Generate random test data for compression kernel
  BlockFloatCompander::ExpandedData expandedDataInput;
  expandedDataInput.dataExpanded = &expandedDataInput.dataExpandedIn[0];
  for (int m = 0; m < BlockFloatCompander::k_numRB; ++m)
    {
        auto shiftVal = randExpShift(gen);
        for (int n = 0; n < BlockFloatCompander::k_numREReal; ++n)
        {
            expandedDataInput.dataExpanded[m*BlockFloatCompander::k_numREReal+n] = int16_t(randInt16(gen) >> shiftVal);
        }
    }

    BlockFloatCompander::CompressedData compressedDataRef;
    compressedDataRef.dataCompressed = &compressedDataRef.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataRef;
    expandedDataRef.dataExpanded = &expandedDataRef.dataExpandedIn[0];
    BlockFloatCompander::CompressedData compressedDataKern;
    compressedDataKern.dataCompressed = &compressedDataKern.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataKern;
    expandedDataKern.dataExpanded = &expandedDataKern.dataExpandedIn[0];

    //std::cout << "Verifying AVX512 12b iqWidth Kernel\n";
    expandedDataInput.iqWidth = 12;
    // Generate reference
    BlockFloatCompander::BlockFloatCompress_Basic(expandedDataInput, &compressedDataRef);
    BlockFloatCompander::BlockFloatExpand_Basic(compressedDataRef, &expandedDataRef);
    // Generate kernel output
    BlockFloatCompander::BlockFloatCompress_12b_AVX512(expandedDataInput, &compressedDataKern);
    BlockFloatCompander::BlockFloatExpand_12b_AVX512(compressedDataRef, &expandedDataKern);
    // Verify
    auto totNumBytes = ((3 * compressedDataRef.iqWidth) + 1) * BlockFloatCompander::k_numRB;
    //std::cout << "Compression: ";
    resSum += checkData(compressedDataRef.dataCompressed, compressedDataKern.dataCompressed, totNumBytes);
    //std::cout << "Expansion: ";
    resSum += checkData(expandedDataRef.dataExpanded, expandedDataKern.dataExpanded, BlockFloatCompander::k_numSampsExpanded);

    ASSERT_EQ(0, resSum);
}

TEST_P(BfpCheck, AVX512_10bit)
{
  int resSum = 0;

  // Create random number generator
  std::random_device rd;
  std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<int16_t> randInt16(-32768, 32767);
  std::uniform_int_distribution<int> randExpShift(0, 4);

  // Generate random test data for compression kernel
  BlockFloatCompander::ExpandedData expandedDataInput;
  expandedDataInput.dataExpanded = &expandedDataInput.dataExpandedIn[0];
  for (int m = 0; m < BlockFloatCompander::k_numRB; ++m)
    {
        auto shiftVal = randExpShift(gen);
        for (int n = 0; n < BlockFloatCompander::k_numREReal; ++n)
        {
            expandedDataInput.dataExpanded[m*BlockFloatCompander::k_numREReal+n] = int16_t(randInt16(gen) >> shiftVal);
        }
    }

    BlockFloatCompander::CompressedData compressedDataRef;
    compressedDataRef.dataCompressed = &compressedDataRef.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataRef;
    expandedDataRef.dataExpanded = &expandedDataRef.dataExpandedIn[0];
    BlockFloatCompander::CompressedData compressedDataKern;
    compressedDataKern.dataCompressed = &compressedDataKern.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataKern;
    expandedDataKern.dataExpanded = &expandedDataKern.dataExpandedIn[0];

    //std::cout << "Verifying AVX512 10b iqWidth Kernel\n";
    expandedDataInput.iqWidth = 10;
    // Generate reference
    BlockFloatCompander::BlockFloatCompress_Basic(expandedDataInput, &compressedDataRef);
    BlockFloatCompander::BlockFloatExpand_Basic(compressedDataRef, &expandedDataRef);
    // Generate kernel output
    BlockFloatCompander::BlockFloatCompress_10b_AVX512(expandedDataInput, &compressedDataKern);
    BlockFloatCompander::BlockFloatExpand_10b_AVX512(compressedDataRef, &expandedDataKern);
    // Verify
    auto totNumBytes = ((3 * compressedDataRef.iqWidth) + 1) * BlockFloatCompander::k_numRB;
    //std::cout << "Compression: ";
    resSum += checkData(compressedDataRef.dataCompressed, compressedDataKern.dataCompressed, totNumBytes);
    //std::cout << "Expansion: ";
    resSum += checkData(expandedDataRef.dataExpanded, expandedDataKern.dataExpanded, BlockFloatCompander::k_numSampsExpanded);

    ASSERT_EQ(0, resSum);

//    performance("AVX512", module_name, BlockFloatCompander::BlockFloatCompress_10b_AVX512, expandedDataInput, &compressedDataKern);
}

TEST_P(BfpCheck, AVX512_9bit)
{
  int resSum = 0;

  // Create random number generator
  std::random_device rd;
  std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<int16_t> randInt16(-32768, 32767);
  std::uniform_int_distribution<int> randExpShift(0, 4);

  // Generate random test data for compression kernel
  BlockFloatCompander::ExpandedData expandedDataInput;
  expandedDataInput.dataExpanded = &expandedDataInput.dataExpandedIn[0];
  for (int m = 0; m < BlockFloatCompander::k_numRB; ++m)
    {
        auto shiftVal = randExpShift(gen);
        for (int n = 0; n < BlockFloatCompander::k_numREReal; ++n)
        {
            expandedDataInput.dataExpanded[m*BlockFloatCompander::k_numREReal+n] = int16_t(randInt16(gen) >> shiftVal);
        }
    }

    BlockFloatCompander::CompressedData compressedDataRef;
    compressedDataRef.dataCompressed = &compressedDataRef.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataRef;
    expandedDataRef.dataExpanded = &expandedDataRef.dataExpandedIn[0];
    BlockFloatCompander::CompressedData compressedDataKern;
    compressedDataKern.dataCompressed = &compressedDataKern.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataKern;
    expandedDataKern.dataExpanded = &expandedDataKern.dataExpandedIn[0];

    //std::cout << "Verifying AVX512 9b iqWidth Kernel\n";
    expandedDataInput.iqWidth = 9;
    // Generate reference
    BlockFloatCompander::BlockFloatCompress_Basic(expandedDataInput, &compressedDataRef);
    BlockFloatCompander::BlockFloatExpand_Basic(compressedDataRef, &expandedDataRef);
    // Generate kernel output
    BlockFloatCompander::BlockFloatCompress_9b_AVX512(expandedDataInput, &compressedDataKern);
    BlockFloatCompander::BlockFloatExpand_9b_AVX512(compressedDataRef, &expandedDataKern);
    // Verify
    auto totNumBytes = ((3 * compressedDataRef.iqWidth) + 1) * BlockFloatCompander::k_numRB;
    //std::cout << "Compression: ";
    resSum += checkData(compressedDataRef.dataCompressed, compressedDataKern.dataCompressed, totNumBytes);
    //std::cout << "Expansion: ";
    resSum += checkData(expandedDataRef.dataExpanded, expandedDataKern.dataExpanded, BlockFloatCompander::k_numSampsExpanded);

    ASSERT_EQ(0, resSum);
}


TEST_P(BfpCheck, AVX512_8bit)
{
  int resSum = 0;

  // Create random number generator
  std::random_device rd;
  std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<int16_t> randInt16(-32768, 32767);
  std::uniform_int_distribution<int> randExpShift(0, 4);

  // Generate random test data for compression kernel
  BlockFloatCompander::ExpandedData expandedDataInput;
  expandedDataInput.dataExpanded = &expandedDataInput.dataExpandedIn[0];
  for (int m = 0; m < BlockFloatCompander::k_numRB; ++m)
    {
        auto shiftVal = randExpShift(gen);
        for (int n = 0; n < BlockFloatCompander::k_numREReal; ++n)
        {
            expandedDataInput.dataExpanded[m*BlockFloatCompander::k_numREReal+n] = int16_t(randInt16(gen) >> shiftVal);
        }
    }

    BlockFloatCompander::CompressedData compressedDataRef;
    compressedDataRef.dataCompressed = &compressedDataRef.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataRef;
    expandedDataRef.dataExpanded = &expandedDataRef.dataExpandedIn[0];
    BlockFloatCompander::CompressedData compressedDataKern;
    compressedDataKern.dataCompressed = &compressedDataKern.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataKern;
    expandedDataKern.dataExpanded = &expandedDataKern.dataExpandedIn[0];

    //std::cout << "Verifying AVX512 8bit Kernel\n";
    expandedDataInput.iqWidth = 8;
    // Generate reference
    BlockFloatCompander::BlockFloatCompress_Basic(expandedDataInput, &compressedDataRef);
    BlockFloatCompander::BlockFloatExpand_Basic(compressedDataRef, &expandedDataRef);
    // Generate kernel output
    BlockFloatCompander::BlockFloatCompress_8b_AVX512(expandedDataInput, &compressedDataKern);
    BlockFloatCompander::BlockFloatExpand_8b_AVX512(compressedDataRef, &expandedDataKern);
    // Verify
    auto totNumBytes = ((3 * compressedDataRef.iqWidth) + 1) * BlockFloatCompander::k_numRB;
    //std::cout << "Compression: ";
    resSum += checkData(compressedDataRef.dataCompressed, compressedDataKern.dataCompressed, totNumBytes);
    //std::cout << "Expansion: ";
    resSum += checkData(expandedDataRef.dataExpanded, expandedDataKern.dataExpanded, BlockFloatCompander::k_numSampsExpanded);

    ASSERT_EQ(0, resSum);
}

TEST_P(BfpPerf, AVX512_8bit_compression)
{
  int resSum = 0;

  // Create random number generator
  std::random_device rd;
  std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<int16_t> randInt16(-32768, 32767);
  std::uniform_int_distribution<int> randExpShift(0, 4);

  // Generate random test data for compression kernel
  BlockFloatCompander::ExpandedData expandedDataInput;
  expandedDataInput.dataExpanded = &expandedDataInput.dataExpandedIn[0];
  for (int m = 0; m < BlockFloatCompander::k_numRB; ++m)
    {
        auto shiftVal = randExpShift(gen);
        for (int n = 0; n < BlockFloatCompander::k_numREReal; ++n)
        {
            expandedDataInput.dataExpanded[m*BlockFloatCompander::k_numREReal+n] = int16_t(randInt16(gen) >> shiftVal);
        }
    }

    BlockFloatCompander::CompressedData compressedDataRef;
    compressedDataRef.dataCompressed = &compressedDataRef.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataRef;
    expandedDataRef.dataExpanded = &expandedDataRef.dataExpandedIn[0];
    BlockFloatCompander::CompressedData compressedDataKern;
    compressedDataKern.dataCompressed = &compressedDataKern.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataKern;
    expandedDataKern.dataExpanded = &expandedDataKern.dataExpandedIn[0];

    //std::cout << "Verifying AVX512 8bit Kernel\n";
    expandedDataInput.iqWidth = 8;
    // Generate reference
    BlockFloatCompander::BlockFloatCompress_Basic(expandedDataInput, &compressedDataRef);
    BlockFloatCompander::BlockFloatExpand_Basic(compressedDataRef, &expandedDataRef);
    // Generate kernel output
    BlockFloatCompander::BlockFloatCompress_8b_AVX512(expandedDataInput, &compressedDataKern);
    BlockFloatCompander::BlockFloatExpand_8b_AVX512(compressedDataRef, &expandedDataKern);
    // Verify
    auto totNumBytes = ((3 * compressedDataRef.iqWidth) + 1) * BlockFloatCompander::k_numRB;
    //std::cout << "Compression: ";
    resSum += checkData(compressedDataRef.dataCompressed, compressedDataKern.dataCompressed, totNumBytes);
    //std::cout << "Expansion: ";
    resSum += checkData(expandedDataRef.dataExpanded, expandedDataKern.dataExpanded, BlockFloatCompander::k_numSampsExpanded);

    ASSERT_EQ(0, resSum);

    performance("AVX512", module_name, BlockFloatCompander::BlockFloatCompress_8b_AVX512, expandedDataInput, &compressedDataKern);
}

TEST_P(BfpPerf, AVX512_8bit_decompression)
{
  int resSum = 0;

  // Create random number generator
  std::random_device rd;
  std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<int16_t> randInt16(-32768, 32767);
  std::uniform_int_distribution<int> randExpShift(0, 4);

  // Generate random test data for compression kernel
  BlockFloatCompander::ExpandedData expandedDataInput;
  expandedDataInput.dataExpanded = &expandedDataInput.dataExpandedIn[0];
  for (int m = 0; m < BlockFloatCompander::k_numRB; ++m)
    {
        auto shiftVal = randExpShift(gen);
        for (int n = 0; n < BlockFloatCompander::k_numREReal; ++n)
        {
            expandedDataInput.dataExpanded[m*BlockFloatCompander::k_numREReal+n] = int16_t(randInt16(gen) >> shiftVal);
        }
    }

    BlockFloatCompander::CompressedData compressedDataRef;
    compressedDataRef.dataCompressed = &compressedDataRef.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataRef;
    expandedDataRef.dataExpanded = &expandedDataRef.dataExpandedIn[0];
    BlockFloatCompander::CompressedData compressedDataKern;
    compressedDataKern.dataCompressed = &compressedDataKern.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataKern;
    expandedDataKern.dataExpanded = &expandedDataKern.dataExpandedIn[0];

    //std::cout << "Verifying AVX512 8bit Kernel\n";
    expandedDataInput.iqWidth = 8;
    // Generate reference
    BlockFloatCompander::BlockFloatCompress_Basic(expandedDataInput, &compressedDataRef);
    BlockFloatCompander::BlockFloatExpand_Basic(compressedDataRef, &expandedDataRef);
    // Generate kernel output
    BlockFloatCompander::BlockFloatCompress_8b_AVX512(expandedDataInput, &compressedDataKern);
    BlockFloatCompander::BlockFloatExpand_8b_AVX512(compressedDataRef, &expandedDataKern);
    // Verify
    auto totNumBytes = ((3 * compressedDataRef.iqWidth) + 1) * BlockFloatCompander::k_numRB;
    //std::cout << "Compression: ";
    resSum += checkData(compressedDataRef.dataCompressed, compressedDataKern.dataCompressed, totNumBytes);
    //std::cout << "Expansion: ";
    resSum += checkData(expandedDataRef.dataExpanded, expandedDataKern.dataExpanded, BlockFloatCompander::k_numSampsExpanded);

    ASSERT_EQ(0, resSum);

    performance("AVX512", module_name, BlockFloatCompander::BlockFloatExpand_8b_AVX512, compressedDataRef, &expandedDataKern);
}



TEST_P(BfpPerf, AVX512_9bit_compression)
{
  int resSum = 0;

  // Create random number generator
  std::random_device rd;
  std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<int16_t> randInt16(-32768, 32767);
  std::uniform_int_distribution<int> randExpShift(0, 4);

  // Generate random test data for compression kernel
  BlockFloatCompander::ExpandedData expandedDataInput;
  expandedDataInput.dataExpanded = &expandedDataInput.dataExpandedIn[0];
  for (int m = 0; m < BlockFloatCompander::k_numRB; ++m)
    {
        auto shiftVal = randExpShift(gen);
        for (int n = 0; n < BlockFloatCompander::k_numREReal; ++n)
        {
            expandedDataInput.dataExpanded[m*BlockFloatCompander::k_numREReal+n] = int16_t(randInt16(gen) >> shiftVal);
        }
    }

    BlockFloatCompander::CompressedData compressedDataRef;
    compressedDataRef.dataCompressed = &compressedDataRef.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataRef;
    expandedDataRef.dataExpanded = &expandedDataRef.dataExpandedIn[0];
    BlockFloatCompander::CompressedData compressedDataKern;
    compressedDataKern.dataCompressed = &compressedDataKern.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataKern;
    expandedDataKern.dataExpanded = &expandedDataKern.dataExpandedIn[0];

    //std::cout << "Verifying AVX512 8bit Kernel\n";
    expandedDataInput.iqWidth = 9;
    // Generate reference
    BlockFloatCompander::BlockFloatCompress_Basic(expandedDataInput, &compressedDataRef);
    BlockFloatCompander::BlockFloatExpand_Basic(compressedDataRef, &expandedDataRef);
    // Generate kernel output
    BlockFloatCompander::BlockFloatCompress_9b_AVX512(expandedDataInput, &compressedDataKern);
    BlockFloatCompander::BlockFloatExpand_9b_AVX512(compressedDataRef, &expandedDataKern);
    // Verify
    auto totNumBytes = ((3 * compressedDataRef.iqWidth) + 1) * BlockFloatCompander::k_numRB;
    //std::cout << "Compression: ";
    resSum += checkData(compressedDataRef.dataCompressed, compressedDataKern.dataCompressed, totNumBytes);
    //std::cout << "Expansion: ";
    resSum += checkData(expandedDataRef.dataExpanded, expandedDataKern.dataExpanded, BlockFloatCompander::k_numSampsExpanded);

    ASSERT_EQ(0, resSum);

    performance("AVX512", module_name, BlockFloatCompander::BlockFloatCompress_9b_AVX512, expandedDataInput, &compressedDataKern);
}


TEST_P(BfpPerf, AVX512_9bit_decompression)
{
  int resSum = 0;

  // Create random number generator
  std::random_device rd;
  std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<int16_t> randInt16(-32768, 32767);
  std::uniform_int_distribution<int> randExpShift(0, 4);

  // Generate random test data for compression kernel
  BlockFloatCompander::ExpandedData expandedDataInput;
  expandedDataInput.dataExpanded = &expandedDataInput.dataExpandedIn[0];
  for (int m = 0; m < BlockFloatCompander::k_numRB; ++m)
    {
        auto shiftVal = randExpShift(gen);
        for (int n = 0; n < BlockFloatCompander::k_numREReal; ++n)
        {
            expandedDataInput.dataExpanded[m*BlockFloatCompander::k_numREReal+n] = int16_t(randInt16(gen) >> shiftVal);
        }
    }

    BlockFloatCompander::CompressedData compressedDataRef;
    compressedDataRef.dataCompressed = &compressedDataRef.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataRef;
    expandedDataRef.dataExpanded = &expandedDataRef.dataExpandedIn[0];
    BlockFloatCompander::CompressedData compressedDataKern;
    compressedDataKern.dataCompressed = &compressedDataKern.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataKern;
    expandedDataKern.dataExpanded = &expandedDataKern.dataExpandedIn[0];

    //std::cout << "Verifying AVX512 8bit Kernel\n";
    expandedDataInput.iqWidth = 9;
    // Generate reference
    BlockFloatCompander::BlockFloatCompress_Basic(expandedDataInput, &compressedDataRef);
    BlockFloatCompander::BlockFloatExpand_Basic(compressedDataRef, &expandedDataRef);
    // Generate kernel output
    BlockFloatCompander::BlockFloatCompress_9b_AVX512(expandedDataInput, &compressedDataKern);
    BlockFloatCompander::BlockFloatExpand_9b_AVX512(compressedDataRef, &expandedDataKern);
    // Verify
    auto totNumBytes = ((3 * compressedDataRef.iqWidth) + 1) * BlockFloatCompander::k_numRB;
    //std::cout << "Compression: ";
    resSum += checkData(compressedDataRef.dataCompressed, compressedDataKern.dataCompressed, totNumBytes);
    //std::cout << "Expansion: ";
    resSum += checkData(expandedDataRef.dataExpanded, expandedDataKern.dataExpanded, BlockFloatCompander::k_numSampsExpanded);

    ASSERT_EQ(0, resSum);

    performance("AVX512", module_name, BlockFloatCompander::BlockFloatExpand_9b_AVX512, compressedDataRef, &expandedDataKern);
}


TEST_P(BfpPerf, AVX512_10bit_compression)
{
  int resSum = 0;

  // Create random number generator
  std::random_device rd;
  std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<int16_t> randInt16(-32768, 32767);
  std::uniform_int_distribution<int> randExpShift(0, 4);

  // Generate random test data for compression kernel
  BlockFloatCompander::ExpandedData expandedDataInput;
  expandedDataInput.dataExpanded = &expandedDataInput.dataExpandedIn[0];
  for (int m = 0; m < BlockFloatCompander::k_numRB; ++m)
    {
        auto shiftVal = randExpShift(gen);
        for (int n = 0; n < BlockFloatCompander::k_numREReal; ++n)
        {
            expandedDataInput.dataExpanded[m*BlockFloatCompander::k_numREReal+n] = int16_t(randInt16(gen) >> shiftVal);
        }
    }

    BlockFloatCompander::CompressedData compressedDataRef;
    compressedDataRef.dataCompressed = &compressedDataRef.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataRef;
    expandedDataRef.dataExpanded = &expandedDataRef.dataExpandedIn[0];
    BlockFloatCompander::CompressedData compressedDataKern;
    compressedDataKern.dataCompressed = &compressedDataKern.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataKern;
    expandedDataKern.dataExpanded = &expandedDataKern.dataExpandedIn[0];

    //std::cout << "Verifying AVX512 8bit Kernel\n";
    expandedDataInput.iqWidth = 10;
    // Generate reference
    BlockFloatCompander::BlockFloatCompress_Basic(expandedDataInput, &compressedDataRef);
    BlockFloatCompander::BlockFloatExpand_Basic(compressedDataRef, &expandedDataRef);
    // Generate kernel output
    BlockFloatCompander::BlockFloatCompress_10b_AVX512(expandedDataInput, &compressedDataKern);
    BlockFloatCompander::BlockFloatExpand_10b_AVX512(compressedDataRef, &expandedDataKern);
    // Verify
    auto totNumBytes = ((3 * compressedDataRef.iqWidth) + 1) * BlockFloatCompander::k_numRB;
    //std::cout << "Compression: ";
    resSum += checkData(compressedDataRef.dataCompressed, compressedDataKern.dataCompressed, totNumBytes);
    //std::cout << "Expansion: ";
    resSum += checkData(expandedDataRef.dataExpanded, expandedDataKern.dataExpanded, BlockFloatCompander::k_numSampsExpanded);

    ASSERT_EQ(0, resSum);

    performance("AVX512", module_name, BlockFloatCompander::BlockFloatCompress_10b_AVX512, expandedDataInput, &compressedDataKern);
}

TEST_P(BfpPerf, AVX512_10bit_decompression)
{
  int resSum = 0;

  // Create random number generator
  std::random_device rd;
  std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<int16_t> randInt16(-32768, 32767);
  std::uniform_int_distribution<int> randExpShift(0, 4);

  // Generate random test data for compression kernel
  BlockFloatCompander::ExpandedData expandedDataInput;
  expandedDataInput.dataExpanded = &expandedDataInput.dataExpandedIn[0];
  for (int m = 0; m < BlockFloatCompander::k_numRB; ++m)
    {
        auto shiftVal = randExpShift(gen);
        for (int n = 0; n < BlockFloatCompander::k_numREReal; ++n)
        {
            expandedDataInput.dataExpanded[m*BlockFloatCompander::k_numREReal+n] = int16_t(randInt16(gen) >> shiftVal);
        }
    }

    BlockFloatCompander::CompressedData compressedDataRef;
    compressedDataRef.dataCompressed = &compressedDataRef.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataRef;
    expandedDataRef.dataExpanded = &expandedDataRef.dataExpandedIn[0];
    BlockFloatCompander::CompressedData compressedDataKern;
    compressedDataKern.dataCompressed = &compressedDataKern.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataKern;
    expandedDataKern.dataExpanded = &expandedDataKern.dataExpandedIn[0];

    //std::cout << "Verifying AVX512 8bit Kernel\n";
    expandedDataInput.iqWidth = 10;
    // Generate reference
    BlockFloatCompander::BlockFloatCompress_Basic(expandedDataInput, &compressedDataRef);
    BlockFloatCompander::BlockFloatExpand_Basic(compressedDataRef, &expandedDataRef);
    // Generate kernel output
    BlockFloatCompander::BlockFloatCompress_10b_AVX512(expandedDataInput, &compressedDataKern);
    BlockFloatCompander::BlockFloatExpand_10b_AVX512(compressedDataRef, &expandedDataKern);
    // Verify
    auto totNumBytes = ((3 * compressedDataRef.iqWidth) + 1) * BlockFloatCompander::k_numRB;
    //std::cout << "Compression: ";
    resSum += checkData(compressedDataRef.dataCompressed, compressedDataKern.dataCompressed, totNumBytes);
    //std::cout << "Expansion: ";
    resSum += checkData(expandedDataRef.dataExpanded, expandedDataKern.dataExpanded, BlockFloatCompander::k_numSampsExpanded);

    ASSERT_EQ(0, resSum);

    performance("AVX512", module_name, BlockFloatCompander::BlockFloatExpand_10b_AVX512, compressedDataRef, &expandedDataKern);
}

TEST_P(BfpPerf, AVX512_12bit_compression)
{
  int resSum = 0;

  // Create random number generator
  std::random_device rd;
  std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<int16_t> randInt16(-32768, 32767);
  std::uniform_int_distribution<int> randExpShift(0, 4);

  // Generate random test data for compression kernel
  BlockFloatCompander::ExpandedData expandedDataInput;
  expandedDataInput.dataExpanded = &expandedDataInput.dataExpandedIn[0];
  for (int m = 0; m < BlockFloatCompander::k_numRB; ++m)
    {
        auto shiftVal = randExpShift(gen);
        for (int n = 0; n < BlockFloatCompander::k_numREReal; ++n)
        {
            expandedDataInput.dataExpanded[m*BlockFloatCompander::k_numREReal+n] = int16_t(randInt16(gen) >> shiftVal);
        }
    }

    BlockFloatCompander::CompressedData compressedDataRef;
    compressedDataRef.dataCompressed = &compressedDataRef.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataRef;
    expandedDataRef.dataExpanded = &expandedDataRef.dataExpandedIn[0];
    BlockFloatCompander::CompressedData compressedDataKern;
    compressedDataKern.dataCompressed = &compressedDataKern.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataKern;
    expandedDataKern.dataExpanded = &expandedDataKern.dataExpandedIn[0];

    //std::cout << "Verifying AVX512 8bit Kernel\n";
    expandedDataInput.iqWidth = 12;
    // Generate reference
    BlockFloatCompander::BlockFloatCompress_Basic(expandedDataInput, &compressedDataRef);
    BlockFloatCompander::BlockFloatExpand_Basic(compressedDataRef, &expandedDataRef);
    // Generate kernel output
    BlockFloatCompander::BlockFloatCompress_12b_AVX512(expandedDataInput, &compressedDataKern);
    BlockFloatCompander::BlockFloatExpand_12b_AVX512(compressedDataRef, &expandedDataKern);
    // Verify
    auto totNumBytes = ((3 * compressedDataRef.iqWidth) + 1) * BlockFloatCompander::k_numRB;
    //std::cout << "Compression: ";
    resSum += checkData(compressedDataRef.dataCompressed, compressedDataKern.dataCompressed, totNumBytes);
    //std::cout << "Expansion: ";
    resSum += checkData(expandedDataRef.dataExpanded, expandedDataKern.dataExpanded, BlockFloatCompander::k_numSampsExpanded);

    ASSERT_EQ(0, resSum);

    performance("AVX512", module_name, BlockFloatCompander::BlockFloatCompress_12b_AVX512, expandedDataInput, &compressedDataKern);
}


TEST_P(BfpPerf, AVX512_12bit_decompression)
{
  int resSum = 0;

  // Create random number generator
  std::random_device rd;
  std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<int16_t> randInt16(-32768, 32767);
  std::uniform_int_distribution<int> randExpShift(0, 4);

  // Generate random test data for compression kernel
  BlockFloatCompander::ExpandedData expandedDataInput;
  expandedDataInput.dataExpanded = &expandedDataInput.dataExpandedIn[0];
  for (int m = 0; m < BlockFloatCompander::k_numRB; ++m)
    {
        auto shiftVal = randExpShift(gen);
        for (int n = 0; n < BlockFloatCompander::k_numREReal; ++n)
        {
            expandedDataInput.dataExpanded[m*BlockFloatCompander::k_numREReal+n] = int16_t(randInt16(gen) >> shiftVal);
        }
    }

    BlockFloatCompander::CompressedData compressedDataRef;
    compressedDataRef.dataCompressed = &compressedDataRef.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataRef;
    expandedDataRef.dataExpanded = &expandedDataRef.dataExpandedIn[0];
    BlockFloatCompander::CompressedData compressedDataKern;
    compressedDataKern.dataCompressed = &compressedDataKern.dataCompressedDataOut[0];
    BlockFloatCompander::ExpandedData expandedDataKern;
    expandedDataKern.dataExpanded = &expandedDataKern.dataExpandedIn[0];

    //std::cout << "Verifying AVX512 8bit Kernel\n";
    expandedDataInput.iqWidth = 12;
    // Generate reference
    BlockFloatCompander::BlockFloatCompress_Basic(expandedDataInput, &compressedDataRef);
    BlockFloatCompander::BlockFloatExpand_Basic(compressedDataRef, &expandedDataRef);
    // Generate kernel output
    BlockFloatCompander::BlockFloatCompress_12b_AVX512(expandedDataInput, &compressedDataKern);
    BlockFloatCompander::BlockFloatExpand_12b_AVX512(compressedDataRef, &expandedDataKern);
    // Verify
    auto totNumBytes = ((3 * compressedDataRef.iqWidth) + 1) * BlockFloatCompander::k_numRB;
    //std::cout << "Compression: ";
    resSum += checkData(compressedDataRef.dataCompressed, compressedDataKern.dataCompressed, totNumBytes);
    //std::cout << "Expansion: ";
    resSum += checkData(expandedDataRef.dataExpanded, expandedDataKern.dataExpanded, BlockFloatCompander::k_numSampsExpanded);

    ASSERT_EQ(0, resSum);

    performance("AVX512", module_name, BlockFloatCompander::BlockFloatExpand_12b_AVX512, compressedDataRef, &expandedDataKern);
}

TEST_P(BfpCheck, AVX512_sweep_xranlib)
{
    int32_t resSum  = 0;
    int16_t len = 0;

    int16_t compMethod = XRAN_COMPMETHOD_BLKFLOAT;
    int16_t iqWidth[]    = {8, 9, 10, 12};

    int16_t numRBs[] = {16, 18, 32, 36, 48, 70, 113, 273};
    struct xranlib_decompress_request  bfp_decom_req;
    struct xranlib_decompress_response bfp_decom_rsp;

    struct xranlib_compress_request  bfp_com_req;
    struct xranlib_compress_response bfp_com_rsp;

    // Create random number generator
    std::random_device rd;
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<int16_t> randInt16(-32768, 32767);
    std::uniform_int_distribution<int> randExpShift(0, 4);

    BlockFloatCompander::ExpandedData expandedData;
    expandedData.dataExpanded = &loc_dataExpandedIn[0];
    BlockFloatCompander::ExpandedData expandedDataRes;
    expandedDataRes.dataExpanded = &loc_dataExpandedRes[0];
    for (int iq_w_id = 0; iq_w_id < sizeof(iqWidth)/sizeof(iqWidth[0]); iq_w_id ++){
        for (int tc = 0; tc < sizeof(numRBs)/sizeof(numRBs[0]); tc ++){

            //printf("[%d]numRBs %d [%d] iqWidth %d\n",tc, numRBs[tc], iq_w_id, iqWidth[iq_w_id]);
            // Generate random test data for compression kernel

            for (int m = 0; m < 18*BlockFloatCompander::k_numRB; ++m) {
                auto shiftVal = randExpShift(gen);
                for (int n = 0; n < BlockFloatCompander::k_numREReal; ++n) {
                    expandedData.dataExpanded[m*BlockFloatCompander::k_numREReal+n] = int16_t(randInt16(gen) >> shiftVal);
                }
            }

            BlockFloatCompander::CompressedData compressedData;
            compressedData.dataCompressed = &loc_dataCompressedDataOut[0];

            std::memset(&loc_dataCompressedDataOut[0], 0, 288*BlockFloatCompander::k_numREReal);
            std::memset(&loc_dataExpandedRes[0], 0, 288*BlockFloatCompander::k_numREReal);

            std::memset(&bfp_com_req, 0, sizeof(struct xranlib_compress_request));
            std::memset(&bfp_com_rsp, 0, sizeof(struct xranlib_compress_response));
            std::memset(&bfp_decom_req, 0, sizeof(struct xranlib_decompress_request));
            std::memset(&bfp_decom_rsp, 0, sizeof(struct xranlib_decompress_response));

            bfp_com_req.data_in    = (int16_t *)expandedData.dataExpanded;
            bfp_com_req.numRBs     = numRBs[tc];
            bfp_com_req.len        = numRBs[tc]*12*2*2;
            bfp_com_req.compMethod = compMethod;
            bfp_com_req.iqWidth    = iqWidth[iq_w_id];

            bfp_com_rsp.data_out   = (int8_t *)(compressedData.dataCompressed);
            bfp_com_rsp.len        = 0;

            xranlib_compress_avx512(&bfp_com_req, &bfp_com_rsp);

            bfp_decom_req.data_in    = (int8_t *)(compressedData.dataCompressed);
            bfp_decom_req.numRBs     = numRBs[tc];
            bfp_decom_req.len        = bfp_com_rsp.len;
            bfp_decom_req.compMethod = compMethod;
            bfp_decom_req.iqWidth    = iqWidth[iq_w_id];

            bfp_decom_rsp.data_out   = (int16_t *)expandedDataRes.dataExpanded;
            bfp_decom_rsp.len        = 0;

            xranlib_decompress_avx512(&bfp_decom_req, &bfp_decom_rsp);

            resSum += checkDataApprox(expandedData.dataExpanded, expandedDataRes.dataExpanded, numRBs[tc]*BlockFloatCompander::k_numREReal);

            ASSERT_EQ(numRBs[tc]*12*2*2, bfp_decom_rsp.len);
            ASSERT_EQ(0, resSum);
         }
    }
}

TEST_P(BfpPerfEx, AVX512_Comp)
{
     performance("AVX512", module_name, xranlib_compress_avx512, &bfp_com_req, &bfp_com_rsp);
}

TEST_P(BfpPerfEx, AVX512_DeComp)
{
     performance("AVX512", module_name, xranlib_decompress_avx512, &bfp_decom_req, &bfp_decom_rsp);
}

INSTANTIATE_TEST_CASE_P(UnitTest, BfpCheck,
                        testing::ValuesIn(get_sequence(BfpCheck::get_number_of_cases("bfp_functional"))));

INSTANTIATE_TEST_CASE_P(UnitTest, BfpPerf,
                        testing::ValuesIn(get_sequence(BfpPerf::get_number_of_cases("bfp_performace"))));


INSTANTIATE_TEST_CASE_P(UnitTest, BfpPerfEx,
                        testing::ValuesIn(get_sequence(BfpPerf::get_number_of_cases("bfp_performace_ex"))));


