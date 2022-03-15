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

extern int _may_i_use_cpu_feature(unsigned __int64);

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
        printf("[%d]: %d %d\n",n, inVec1[n] & 0xFF00, inVec2[n] & 0xFF00);
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

CACHE_ALIGNED int16_t loc_dataExpandedIn[288*128];
CACHE_ALIGNED int16_t loc_dataExpandedRes[288*128];
CACHE_ALIGNED uint8_t loc_dataCompressedDataOut[2*288*128];

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

        for (int m = 0; m < 18*BlockFloatCompander::k_maxNumBlocks; ++m) {
            auto shiftVal = randExpShift(gen);
            for (int n = 0; n < 24; ++n) {
                expandedData.dataExpanded[m*24+n] = int16_t(randInt16(gen) >> shiftVal);
            }
        }

        BlockFloatCompander::CompressedData compressedData;
        compressedData.dataCompressed = &loc_dataCompressedDataOut[0];

        std::memset(&loc_dataCompressedDataOut[0], 0, 288*24);
        std::memset(&loc_dataExpandedRes[0], 0, 288*24);

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


class BfpPerfCp : public KernelTests
{
protected:
    struct xranlib_decompress_request  bfp_decom_req;
    struct xranlib_decompress_response bfp_decom_rsp;
    struct xranlib_compress_request  bfp_com_req;
    struct xranlib_compress_response bfp_com_rsp;

    void SetUp() override {
        init_test("bfp_performace_cp");
        int32_t resSum  = 0;
        int16_t len = 0;
        int16_t compMethod = XRAN_COMPMETHOD_BLKFLOAT;
        int16_t iqWidth    = get_input_parameter<int16_t>("iqWidth");
        int16_t AntElm     = get_input_parameter<int16_t>("AntElm");
        int16_t numDataElements = 0;
        int16_t numRBs = 1;
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
        numDataElements = 2*AntElm;

        // Generate input data
        for (int m = 0; m < numRBs; ++m)
        {
          auto shiftVal = randExpShift(gen);
          for (int n = 0; n < numDataElements; ++n)
          {
            expandedData.dataExpanded[m * numDataElements + n] = int16_t(randInt16(gen) >> shiftVal);
          }
        }

        BlockFloatCompander::CompressedData compressedData;
        compressedData.dataCompressed = &loc_dataCompressedDataOut[0];

        std::memset(&loc_dataCompressedDataOut[0], 0, 288*128);
        std::memset(&loc_dataExpandedRes[0], 0, 288*128);

        std::memset(&bfp_com_req, 0, sizeof(struct xranlib_compress_request));
        std::memset(&bfp_com_rsp, 0, sizeof(struct xranlib_compress_response));
        std::memset(&bfp_decom_req, 0, sizeof(struct xranlib_decompress_request));
        std::memset(&bfp_decom_rsp, 0, sizeof(struct xranlib_decompress_response));

        bfp_com_req.data_in    = (int16_t *)expandedData.dataExpanded;
        bfp_com_req.numRBs     = numRBs;
        bfp_com_req.numDataElements = numDataElements;
        bfp_com_req.len        = AntElm*4;
        bfp_com_req.compMethod = compMethod;
        bfp_com_req.iqWidth    = iqWidth;

        bfp_com_rsp.data_out   = (int8_t *)(compressedData.dataCompressed);
        bfp_com_rsp.len        = 0;

        bfp_decom_req.data_in    = (int8_t *)(compressedData.dataCompressed);
        bfp_decom_req.numRBs     = numRBs;
        bfp_decom_req.numDataElements = numDataElements;
        bfp_decom_req.len        = (((numDataElements  * iqWidth) >> 3) + 1) * numRBs;
        bfp_decom_req.compMethod = compMethod;
        bfp_decom_req.iqWidth    = iqWidth;

        bfp_decom_rsp.data_out   = (int16_t *)expandedDataRes.dataExpanded;
        bfp_decom_rsp.len        = 0;
    }

    /* It's called after an execution of the each test case.*/
    void TearDown() override {

    }
};

struct ErrorData
{
  int checkSum;
  float errorAccum;
  int errorCount;
};

template <typename T>
void compareData(T* inVecRef, T* inVecTest, ErrorData& err, int numVals)
{
  for (int n = 0; n < numVals; ++n)
  {
    auto valDiff = std::abs(inVecRef[n] - inVecTest[n]);
    err.checkSum += valDiff;
    if (inVecRef[n] != 0)
    {
      err.errorAccum += (float)valDiff / std::abs((float)inVecRef[n]);
      err.errorCount++;
    }
  }
}
template void compareData(int8_t*, int8_t*, ErrorData&, int);
template void compareData(int16_t*, int16_t*, ErrorData&, int);

int checkPass(ErrorData& err, int testType)
{
  if (testType == 0)
  {
    if (err.checkSum == 0)
    {
      /*std::cout << "PASS "; */
      return 0;
    }
    else
    {
      std::cout << "FAIL ";
      return 1;
    }
  }
  else
  {
    //std::cout << err.errorAccum / err.errorCount;
    if (err.errorAccum / err.errorCount < 0.1)
    {
      /*std::cout << " PASS ";*/
      return 0;
    }
    else
    {
      std::cout << " FAIL ";
      return 1;
    }
  }
}
template <typename KERN_TYPE, typename T1, typename T2>
void timeThis(KERN_TYPE kernel, T1& inData, T2* outData)
{
  uint64_t startTime;
  uint64_t finishTime;
  uint64_t thisDuration;
  uint64_t meanTime = 0;
  uint64_t minTime;
  int numRuns = 1000000;
  for (int cnt = 0; cnt < numRuns; ++cnt)
  {
    startTime = __rdtsc();
    kernel(inData, outData);
    kernel(inData, outData);
    kernel(inData, outData);
    kernel(inData, outData);
    kernel(inData, outData);
    kernel(inData, outData);
    kernel(inData, outData);
    kernel(inData, outData);
    kernel(inData, outData);
    kernel(inData, outData);
    finishTime = __rdtsc();
    thisDuration = (finishTime - startTime);
    meanTime += thisDuration;
    if (cnt == 0)
    {
      minTime = thisDuration;
    }
    else if (thisDuration < minTime)
    {
      minTime = thisDuration;
    }
  }
  meanTime = meanTime / numRuns;
  //std::cout << "10 Executions: Mean Time = " << meanTime << ", Min Time = " << minTime << "\n";
  printf("10 Executions: Mean Time = %5ld Min Time = %5ld\n", meanTime, minTime);
}


int runTest(const int runMode, const int iqWidth, const int numRB, const int numDataElements, const int totNumBlocks)
{
  BlockFloatCompander::ExpandedData expandedDataInput;
  BlockFloatCompander::CompressedData compressedDataRef;
  BlockFloatCompander::CompressedData compressedDataKern;
  BlockFloatCompander::ExpandedData expandedDataRef;
  BlockFloatCompander::ExpandedData expandedDataKern;

  ErrorData errRef = ErrorData();
  ErrorData errComp = ErrorData();
  ErrorData errExp = ErrorData();

  // Create random number generator
  std::random_device rd;
  std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<int16_t> randInt16(-32767, 32767);
  std::uniform_int_distribution<int> randExpShift(0, 4);

 // expandedDataInput.dataExpanded    = &expandedDataInput.dataExpandedIn[0];
 // compressedDataRef.dataCompressed  = &compressedDataRef.dataCompressedDataOut[0];
 // compressedDataKern.dataCompressed = &compressedDataKern.dataCompressedDataOut[0];
 // expandedDataRef.dataExpanded      = &expandedDataRef.dataExpandedIn[0];
 // expandedDataKern.dataExpanded     = &expandedDataKern.dataExpandedIn[0];

  expandedDataInput.iqWidth = iqWidth;
  expandedDataInput.numBlocks = numRB;
  expandedDataInput.numDataElements = numDataElements;
  int totExpValsPerCall = numRB * numDataElements;
  int totCompValsPerCall = (((numDataElements * iqWidth) >> 3) + 1) * numRB;

  // Assign pointers to input/output data arrays
  CACHE_ALIGNED int16_t DATAEXPANDED_IN[BlockFloatCompander::k_numSampsExpanded] = { 0 };
  CACHE_ALIGNED uint8_t DATACOMPRESSED_REF[BlockFloatCompander::k_numSampsCompressed] = { 0 };
  CACHE_ALIGNED uint8_t DATACOMPRESSED_KERN[BlockFloatCompander::k_numSampsCompressed] = { 0 };
  CACHE_ALIGNED int16_t DATAEXPANDED_REF[BlockFloatCompander::k_numSampsExpanded] = { 0 };
  CACHE_ALIGNED int16_t DATAEXPANDED_KERN[BlockFloatCompander::k_numSampsExpanded] = { 0 };
  expandedDataInput.dataExpanded = DATAEXPANDED_IN;
  compressedDataRef.dataCompressed = DATACOMPRESSED_REF;
  expandedDataRef.dataExpanded = DATAEXPANDED_REF;
  compressedDataKern.dataCompressed = DATACOMPRESSED_KERN;
  expandedDataKern.dataExpanded = DATAEXPANDED_KERN;

  //-------------------------------------------------------------------------
  // KERNEL VERIFICATION LOOP
  //-------------------------------------------------------------------------
  for (int blk = 0; blk < totNumBlocks; ++blk)
  {
    // Generate input data
    for (int m = 0; m < numRB; ++m)
    {
      auto shiftVal = randExpShift(gen);
      for (int n = 0; n < numDataElements; ++n)
      {
        DATAEXPANDED_IN[m * numDataElements + n] = int16_t(randInt16(gen) >> shiftVal);
      }
    }
    // Generate reference
    BlockFloatCompander::BFPCompressRef(expandedDataInput, &compressedDataRef);
    BlockFloatCompander::BFPExpandRef(compressedDataRef, &expandedDataRef);
    // Generate kernel output
    if (runMode == 1)
    {
      // Run Sunny Cove version
      switch (numDataElements)
      {
      case 16:
        BlockFloatCompander::BFPCompressCtrlPlane8AvxSnc(expandedDataInput, &compressedDataKern);
        BlockFloatCompander::BFPExpandCtrlPlane8AvxSnc(compressedDataRef, &expandedDataKern);
        break;
      case 24:
        BlockFloatCompander::BFPCompressUserPlaneAvxSnc(expandedDataInput, &compressedDataKern);
        BlockFloatCompander::BFPExpandUserPlaneAvxSnc(compressedDataRef, &expandedDataKern);
        break;
      case 32:
        BlockFloatCompander::BFPCompressCtrlPlane16AvxSnc(expandedDataInput, &compressedDataKern);
        BlockFloatCompander::BFPExpandCtrlPlane16AvxSnc(compressedDataRef, &expandedDataKern);
        break;
      case 64:
        BlockFloatCompander::BFPCompressCtrlPlane32AvxSnc(expandedDataInput, &compressedDataKern);
        BlockFloatCompander::BFPExpandCtrlPlane32AvxSnc(compressedDataRef, &expandedDataKern);
        break;
      case 128:
        BlockFloatCompander::BFPCompressCtrlPlane64AvxSnc(expandedDataInput, &compressedDataKern);
        BlockFloatCompander::BFPExpandCtrlPlane64AvxSnc(compressedDataRef, &expandedDataKern);
        break;
      }
    }
    else
    {
        // Default Skylake/Palm Cove AVX512 version
          switch (numDataElements)
        {
        case 16:
          BlockFloatCompander::BFPCompressCtrlPlane8Avx512(expandedDataInput, &compressedDataKern);
          BlockFloatCompander::BFPExpandCtrlPlane8Avx512(compressedDataRef, &expandedDataKern);
          break;
        case 24:
          if ((iqWidth == 9) && (numRB == 16))
          {
            BlockFloatCompander::BFPCompressUserPlaneAvx512_9b16RB(expandedDataInput, &compressedDataKern);
            BlockFloatCompander::BFPExpandUserPlaneAvx512_9b16RB(compressedDataRef, &expandedDataKern);
          }
          else
          {
            BlockFloatCompander::BFPCompressUserPlaneAvx512(expandedDataInput, &compressedDataKern);
            BlockFloatCompander::BFPExpandUserPlaneAvx512(compressedDataRef, &expandedDataKern);
          }
          break;
        case 32:
          BlockFloatCompander::BFPCompressCtrlPlane16Avx512(expandedDataInput, &compressedDataKern);
          BlockFloatCompander::BFPExpandCtrlPlane16Avx512(compressedDataRef, &expandedDataKern);
          break;
        case 64:
          BlockFloatCompander::BFPCompressCtrlPlane32Avx512(expandedDataInput, &compressedDataKern);
          BlockFloatCompander::BFPExpandCtrlPlane32Avx512(compressedDataRef, &expandedDataKern);
          break;
        case 128:
          BlockFloatCompander::BFPCompressCtrlPlane64Avx512(expandedDataInput, &compressedDataKern);
          BlockFloatCompander::BFPExpandCtrlPlane64Avx512(compressedDataRef, &expandedDataKern);
          break;
    }
}
    // Check data
    compareData(expandedDataInput.dataExpanded, expandedDataRef.dataExpanded, errRef, totExpValsPerCall);
    compareData(compressedDataRef.dataCompressed, compressedDataKern.dataCompressed, errComp, totCompValsPerCall);
    compareData(expandedDataRef.dataExpanded, expandedDataKern.dataExpanded, errRef, totExpValsPerCall);
  }
  // Verify Reference
  int resSum = 0;
  /*std::cout << "Valid Reference: ";*/
  resSum += checkPass(errRef, 1);
  // Verify Kernel
  /*std::cout << "Compression: ";*/
  resSum += checkPass(errComp, 0);
  /*std::cout << "Expansion: ";*/
  resSum += checkPass(errExp, 0);
  /*std::cout << "\n";*/
  //-------------------------------------------------------------------------
  // KERNEL TIMING LOOP
  //-------------------------------------------------------------------------
  // Generate input data

  for (int m = 0; m < numRB; ++m)
  {
    auto shiftVal = randExpShift(gen);
    for (int n = 0; n < numDataElements; ++n)
    {
      DATAEXPANDED_IN[m * numDataElements + n] = int16_t(randInt16(gen) >> shiftVal);
    }
  }
  // Generate reference
  BlockFloatCompander::BFPCompressRef(expandedDataInput, &compressedDataRef);

  if (runMode == 1)
  {
    // Run Sunny Cove version
    switch (numDataElements)
    {
    case 16:
      //std::cout << "Timing Control Plane 8 Antennas (SNC)...\n";
      //std::cout << "Compression: ";
      printf("BFPCompressCtrlPlane8AvxSnc         iqWidth %2d numRB %2d numDataElements %3d ",iqWidth, numRB, numDataElements);
      timeThis(BlockFloatCompander::BFPCompressCtrlPlane8AvxSnc, expandedDataInput, &compressedDataKern);
      //std::cout << "Expansion  : ";
      printf("BFPExpandCtrlPlane8AvxSnc           iqWidth %2d numRB %2d numDataElements %3d ",iqWidth, numRB, numDataElements);
      timeThis(BlockFloatCompander::BFPExpandCtrlPlane8AvxSnc, compressedDataRef, &expandedDataKern);
      break;
    case 24:
      //std::cout << "Timing User Plane (SNC)...\n";
      //std::cout << "Compression: ";
      printf("BFPCompressUserPlaneAvxSnc          iqWidth %2d numRB %2d numDataElements %3d ",iqWidth, numRB, numDataElements);
      timeThis(BlockFloatCompander::BFPCompressUserPlaneAvxSnc, expandedDataInput, &compressedDataKern);
      //std::cout << "Expansion  : ";
      printf("BFPExpandUserPlaneAvxSnc            iqWidth %2d numRB %2d numDataElements %3d ",iqWidth, numRB, numDataElements);
      timeThis(BlockFloatCompander::BFPExpandUserPlaneAvxSnc, compressedDataRef, &expandedDataKern);
      break;
    case 32:
      //std::cout << "Timing Control Plane 16 Antennas (SNC)...\n";
      //std::cout << "Compression: ";
      printf("BFPCompressCtrlPlane16AvxSnc        iqWidth %2d numRB %2d numDataElements %3d ",iqWidth, numRB, numDataElements);
      timeThis(BlockFloatCompander::BFPCompressCtrlPlane16AvxSnc, expandedDataInput, &compressedDataKern);
      //std::cout << "Expansion  : ";
      printf("BFPExpandCtrlPlane16AvxSnc          iqWidth %2d numRB %2d numDataElements %3d ", iqWidth, numRB, numDataElements);
      timeThis(BlockFloatCompander::BFPExpandCtrlPlane16AvxSnc, compressedDataRef, &expandedDataKern);
      break;
    case 64:
      //std::cout << "Timing Control Plane 32 Antennas (SNC)...\n";
      //std::cout << "Compression: ";
      printf("BFPCompressCtrlPlane32AvxSnc        iqWidth %2d numRB %2d numDataElements %3d ", iqWidth, numRB, numDataElements);
      timeThis(BlockFloatCompander::BFPCompressCtrlPlane32AvxSnc, expandedDataInput, &compressedDataKern);
      //std::cout << "Expansion  : ";
      printf("BFPExpandCtrlPlane32AvxSnc          iqWidth %2d numRB %2d numDataElements %3d ", iqWidth, numRB, numDataElements);
      timeThis(BlockFloatCompander::BFPExpandCtrlPlane32AvxSnc, compressedDataRef, &expandedDataKern);
      break;
    case 128:
      //std::cout << "Timing Control Plane 64 Antennas (SNC)...\n";
      //std::cout << "Compression: ";
      printf("BFPCompressCtrlPlane64AvxSnc        iqWidth %2d numRB %2d numDataElements %3d ", iqWidth, numRB, numDataElements);
      timeThis(BlockFloatCompander::BFPCompressCtrlPlane64AvxSnc, expandedDataInput, &compressedDataKern);
      //std::cout << "Expansion  : ";
      printf("BFPExpandCtrlPlane64AvxSnc          iqWidth %2d numRB %2d numDataElements %3d ", iqWidth, numRB, numDataElements);
      timeThis(BlockFloatCompander::BFPExpandCtrlPlane64AvxSnc, compressedDataRef, &expandedDataKern);
      break;
    }
  }
  else
  {
    // Default Skylake/Palm Cove AVX512 version
    switch (numDataElements)
    {
    case 16:
      //std::cout << "Timing Control Plane 8 Antennas (AVX512)...\n";
      //std::cout << "Compression: ";
      printf("BFPCompressCtrlPlane8Avx512         iqWidth %2d numRB %2d numDataElements %3d ",iqWidth, numRB, numDataElements);
      timeThis(BlockFloatCompander::BFPCompressCtrlPlane8Avx512, expandedDataInput, &compressedDataKern);
      //std::cout << "Expansion  : ";
      printf("BFPExpandCtrlPlane8Avx512           iqWidth %2d numRB %2d numDataElements %3d ",iqWidth, numRB, numDataElements);
      timeThis(BlockFloatCompander::BFPExpandCtrlPlane8Avx512, compressedDataRef, &expandedDataKern);
      break;
    case 24:
      if ((iqWidth == 9) && (numRB == 16))
      {
          //std::cout << "Timing User Plane (AVX512)...\n";
          //std::cout << "Compression: ";
          printf("BFPCompressUserPlaneAvx512_9b16RB   iqWidth %2d numRB %2d numDataElements %3d ",iqWidth, numRB, numDataElements);
          timeThis(BlockFloatCompander::BFPCompressUserPlaneAvx512_9b16RB, expandedDataInput, &compressedDataKern);
          //std::cout << "Expansion  : ";
          printf("BFPExpandUserPlaneAvx512_9b16RB     iqWidth %2d numRB %2d numDataElements %3d ",iqWidth, numRB, numDataElements);
          timeThis(BlockFloatCompander::BFPExpandUserPlaneAvx512_9b16RB, compressedDataRef, &expandedDataKern);
      }
      else
      {
          //std::cout << "Timing User Plane (AVX512)...\n";
          //std::cout << "Compression: ";
          printf("BFPCompressUserPlaneAvx512          iqWidth %2d numRB %2d numDataElements %3d ",iqWidth, numRB, numDataElements);
          timeThis(BlockFloatCompander::BFPCompressUserPlaneAvx512, expandedDataInput, &compressedDataKern);
          //std::cout << "Expansion  : ";
          printf("BFPExpandUserPlaneAvx512            iqWidth %2d numRB %2d numDataElements %3d ",iqWidth, numRB, numDataElements);
          timeThis(BlockFloatCompander::BFPExpandUserPlaneAvx512, compressedDataRef, &expandedDataKern);
      }
      break;
    case 32:
      //std::cout << "Timing Control Plane 16 Antennas (AVX512)...\n";
      //std::cout << "Compression: ";
      printf("BFPCompressCtrlPlane16Avx512        iqWidth %2d numRB %2d numDataElements %3d ",iqWidth, numRB, numDataElements);
      timeThis(BlockFloatCompander::BFPCompressCtrlPlane16Avx512, expandedDataInput, &compressedDataKern);
      //std::cout << "Expansion  : ";
      printf("BFPExpandCtrlPlane16Avx512          iqWidth %2d numRB %2d numDataElements %3d ",iqWidth, numRB, numDataElements);
      timeThis(BlockFloatCompander::BFPExpandCtrlPlane16Avx512, compressedDataRef, &expandedDataKern);
      break;
    case 64:
      //std::cout << "Timing Control Plane 32 Antennas (AVX512)...\n";
      //std::cout << "Compression: ";
      printf("BFPCompressCtrlPlane32Avx512        iqWidth %2d numRB %2d numDataElements %3d ",iqWidth, numRB, numDataElements);
      timeThis(BlockFloatCompander::BFPCompressCtrlPlane32Avx512, expandedDataInput, &compressedDataKern);
      //std::cout << "Expansion  : ";
      printf("BFPExpandCtrlPlane32Avx512          iqWidth %2d numRB %2d numDataElements %3d ",iqWidth, numRB, numDataElements);
      timeThis(BlockFloatCompander::BFPExpandCtrlPlane32Avx512, compressedDataRef, &expandedDataKern);
      break;
    case 128:
      //std::cout << "Timing Control Plane 64 Antennas (AVX512)...\n";
      //std::cout << "Compression: ";
      printf("BFPCompressCtrlPlane64Avx512        iqWidth %2d numRB %2d numDataElements %3d ",iqWidth, numRB, numDataElements);
      timeThis(BlockFloatCompander::BFPCompressCtrlPlane64Avx512, expandedDataInput, &compressedDataKern);
      //std::cout << "Expansion  : ";
      printf("BFPExpandCtrlPlane64Avx512          iqWidth %2d numRB %2d numDataElements %3d ",iqWidth, numRB, numDataElements);
      timeThis(BlockFloatCompander::BFPExpandCtrlPlane64Avx512, compressedDataRef, &expandedDataKern);
      break;
    }
  }

  return resSum;
}

TEST_P(BfpCheck, AVX512_bfp_main)
{
  int resSum = 0;
  int iqWidth[4] = { 8, 9, 10, 12 };
  int numRB[3] = { 1, 4, 16 };
  int numDataElementsUPlane = 24;
  int numDataElementsCPlane8 = 16;
  int numDataElementsCPlane16 = 32;
  int numDataElementsCPlane32 = 64;
  int numDataElementsCPlane64 = 128;
  int totNumBlocks = 100;

  ASSERT_EQ(0, bind_to_cpu(BenchmarkParameters::cpu_id)) << "Failed to bind to cpu!";

  for (int iqw = 0; iqw < 4; ++iqw)
  {
    for (int nrb = 0; nrb < 3; ++nrb)
    {
      //std::cout << "\n";

      // USER PLANE TESTS
      //std::cout << "U-Plane: Testing iqWidth = " << iqWidth[iqw] << ", numRB = " << numRB[nrb] << ", numElements = " << numDataElementsUPlane << ": ";
      resSum += runTest(0, iqWidth[iqw], numRB[nrb], numDataElementsUPlane, totNumBlocks);

      // CONTROL PLANE TESTS : 8 Antennas
      //std::cout << "C-Plane: Testing iqWidth = " << iqWidth[iqw] << ", numRB = " << numRB[nrb] << ", numElements = " << numDataElementsCPlane8 << ": ";
      resSum += runTest(0, iqWidth[iqw], numRB[nrb], numDataElementsCPlane8, totNumBlocks);

      // CONTROL PLANE TESTS : 16 Antennas
      //std::cout << "C-Plane: Testing iqWidth = " << iqWidth[iqw] << ", numRB = " << numRB[nrb] << ", numElements = " << numDataElementsCPlane16 << ": ";
      resSum += runTest(0, iqWidth[iqw], numRB[nrb], numDataElementsCPlane16, totNumBlocks);

      // CONTROL PLANE TESTS : 32 Antennas
      //std::cout << "C-Plane: Testing iqWidth = " << iqWidth[iqw] << ", numRB = " << numRB[nrb] << ", numElements = " << numDataElementsCPlane32 << ": ";
      resSum += runTest(0, iqWidth[iqw], numRB[nrb], numDataElementsCPlane32, totNumBlocks);

      // CONTROL PLANE TESTS : 64 Antennas
      //std::cout << "C-Plane: Testing iqWidth = " << iqWidth[iqw] << ", numRB = " << numRB[nrb] << ", numElements = " << numDataElementsCPlane64 << ": ";
      resSum += runTest(0, iqWidth[iqw], numRB[nrb], numDataElementsCPlane64, totNumBlocks);
    }
  }

  ASSERT_EQ(0, resSum);
}

TEST_P(BfpCheck, AVXSNC_bfp_main)
{
  int resSum = 0;
  int iqWidth[4] = { 8, 9, 10, 12 };
  int numRB[3] = { 1, 4, 16 };
  int numDataElementsUPlane = 24;
  int numDataElementsCPlane8 = 16;
  int numDataElementsCPlane16 = 32;
  int numDataElementsCPlane32 = 64;
  int numDataElementsCPlane64 = 128;
  int totNumBlocks = 100;

  ASSERT_EQ(0, bind_to_cpu(BenchmarkParameters::cpu_id)) << "Failed to bind to cpu!";

  if(_may_i_use_cpu_feature(_FEATURE_AVX512IFMA52) == 0)
     return;

  for (int iqw = 0; iqw < 4; ++iqw)
  {
    for (int nrb = 0; nrb < 3; ++nrb)
    {
      //std::cout << "\n";

      // USER PLANE TESTS
      //std::cout << "U-Plane: Testing iqWidth = " << iqWidth[iqw] << ", numRB = " << numRB[nrb] << ", numElements = " << numDataElementsUPlane << ": ";
      resSum += runTest(1, iqWidth[iqw], numRB[nrb], numDataElementsUPlane, totNumBlocks);

      // CONTROL PLANE TESTS : 8 Antennas
      //std::cout << "C-Plane: Testing iqWidth = " << iqWidth[iqw] << ", numRB = " << numRB[nrb] << ", numElements = " << numDataElementsCPlane8 << ": ";
      resSum += runTest(1, iqWidth[iqw], numRB[nrb], numDataElementsCPlane8, totNumBlocks);

      // CONTROL PLANE TESTS : 16 Antennas
      //std::cout << "C-Plane: Testing iqWidth = " << iqWidth[iqw] << ", numRB = " << numRB[nrb] << ", numElements = " << numDataElementsCPlane16 << ": ";
      resSum += runTest(1, iqWidth[iqw], numRB[nrb], numDataElementsCPlane16, totNumBlocks);

      // CONTROL PLANE TESTS : 32 Antennas
      //std::cout << "C-Plane: Testing iqWidth = " << iqWidth[iqw] << ", numRB = " << numRB[nrb] << ", numElements = " << numDataElementsCPlane32 << ": ";
      resSum += runTest(1, iqWidth[iqw], numRB[nrb], numDataElementsCPlane32, totNumBlocks);

      // CONTROL PLANE TESTS : 64 Antennas
      //std::cout << "C-Plane: Testing iqWidth = " << iqWidth[iqw] << ", numRB = " << numRB[nrb] << ", numElements = " << numDataElementsCPlane64 << ": ";
      resSum += runTest(1, iqWidth[iqw], numRB[nrb], numDataElementsCPlane64, totNumBlocks);
    }
  }

  ASSERT_EQ(0, resSum);
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

    int numDataElements = 24;

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

            for (int m = 0; m < 18*BlockFloatCompander::k_maxNumBlocks; ++m) {
                auto shiftVal = randExpShift(gen);
                for (int n = 0; n < numDataElements; ++n) {
                    expandedData.dataExpanded[m*numDataElements+n] = int16_t(randInt16(gen) >> shiftVal);
                }
            }

            BlockFloatCompander::CompressedData compressedData;
            compressedData.dataCompressed = &loc_dataCompressedDataOut[0];

            std::memset(&loc_dataCompressedDataOut[0], 0, 288*numDataElements);
            std::memset(&loc_dataExpandedRes[0], 0, 288*numDataElements);

            std::memset(&bfp_com_req, 0, sizeof(struct xranlib_compress_request));
            std::memset(&bfp_com_rsp, 0, sizeof(struct xranlib_compress_response));
            std::memset(&bfp_decom_req, 0, sizeof(struct xranlib_decompress_request));
            std::memset(&bfp_decom_rsp, 0, sizeof(struct xranlib_decompress_response));

            bfp_com_req.data_in    = (int16_t *)expandedData.dataExpanded;
            bfp_com_req.numRBs     = numRBs[tc];
            bfp_com_req.numDataElements = 24;
            bfp_com_req.len        = numRBs[tc]*12*2*2;
            bfp_com_req.compMethod = compMethod;
            bfp_com_req.iqWidth    = iqWidth[iq_w_id];

            bfp_com_rsp.data_out   = (int8_t *)(compressedData.dataCompressed);
            bfp_com_rsp.len        = 0;

            xranlib_compress_avx512(&bfp_com_req, &bfp_com_rsp);

            bfp_decom_req.data_in    = (int8_t *)(compressedData.dataCompressed);
            bfp_decom_req.numRBs     = numRBs[tc];
            bfp_decom_req.len        = bfp_com_rsp.len;
            bfp_decom_req.numDataElements = 24;
            bfp_decom_req.compMethod = compMethod;
            bfp_decom_req.iqWidth    = iqWidth[iq_w_id];

            bfp_decom_rsp.data_out   = (int16_t *)expandedDataRes.dataExpanded;
            bfp_decom_rsp.len        = 0;

            xranlib_decompress_avx512(&bfp_decom_req, &bfp_decom_rsp);

            resSum += checkDataApprox(expandedData.dataExpanded, expandedDataRes.dataExpanded, numRBs[tc]*numDataElements);

            ASSERT_EQ(numRBs[tc]*12*2*2, bfp_decom_rsp.len);
            ASSERT_EQ(0, resSum);
         }
    }
}

TEST_P(BfpCheck, AVXSNC_sweep_xranlib)
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

    int numDataElements = 24;


    if(_may_i_use_cpu_feature(_FEATURE_AVX512IFMA52) == 0)
        return;

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

            for (int m = 0; m < 18*BlockFloatCompander::k_maxNumBlocks; ++m) {
                auto shiftVal = randExpShift(gen);
                for (int n = 0; n < numDataElements; ++n) {
                    expandedData.dataExpanded[m*numDataElements+n] = int16_t(randInt16(gen) >> shiftVal);
                }
            }

            BlockFloatCompander::CompressedData compressedData;
            compressedData.dataCompressed = &loc_dataCompressedDataOut[0];

            std::memset(&loc_dataCompressedDataOut[0], 0, 288*numDataElements);
            std::memset(&loc_dataExpandedRes[0], 0, 288*numDataElements);

            std::memset(&bfp_com_req, 0, sizeof(struct xranlib_compress_request));
            std::memset(&bfp_com_rsp, 0, sizeof(struct xranlib_compress_response));
            std::memset(&bfp_decom_req, 0, sizeof(struct xranlib_decompress_request));
            std::memset(&bfp_decom_rsp, 0, sizeof(struct xranlib_decompress_response));

            bfp_com_req.data_in    = (int16_t *)expandedData.dataExpanded;
            bfp_com_req.numRBs     = numRBs[tc];
            bfp_com_req.numDataElements = 24;
            bfp_com_req.len        = numRBs[tc]*12*2*2;
            bfp_com_req.compMethod = compMethod;
            bfp_com_req.iqWidth    = iqWidth[iq_w_id];

            bfp_com_rsp.data_out   = (int8_t *)(compressedData.dataCompressed);
            bfp_com_rsp.len        = 0;

            xranlib_compress_avxsnc(&bfp_com_req, &bfp_com_rsp);

            bfp_decom_req.data_in    = (int8_t *)(compressedData.dataCompressed);
            bfp_decom_req.numRBs     = numRBs[tc];
            bfp_decom_req.len        = bfp_com_rsp.len;
            bfp_decom_req.numDataElements = 24;
            bfp_decom_req.compMethod = compMethod;
            bfp_decom_req.iqWidth    = iqWidth[iq_w_id];

            bfp_decom_rsp.data_out   = (int16_t *)expandedDataRes.dataExpanded;
            bfp_decom_rsp.len        = 0;

            xranlib_decompress_avxsnc(&bfp_decom_req, &bfp_decom_rsp);

            resSum += checkDataApprox(expandedData.dataExpanded, expandedDataRes.dataExpanded, numRBs[tc]*numDataElements);

            ASSERT_EQ(numRBs[tc]*12*2*2, bfp_decom_rsp.len);
            ASSERT_EQ(0, resSum);
         }
    }
}

TEST_P(BfpCheck, AVX512_cp_sweep_xranlib)
{
    int32_t resSum  = 0;
    int16_t len = 0;

    int16_t compMethod = XRAN_COMPMETHOD_BLKFLOAT;
    int16_t iqWidth[]    = {8, 9, 10, 12};
    int16_t numRB = 1;
    int16_t antElm[] = {8, 16, 32, 64};

    struct xranlib_decompress_request  bfp_decom_req;
    struct xranlib_decompress_response bfp_decom_rsp;

    struct xranlib_compress_request  bfp_com_req;
    struct xranlib_compress_response bfp_com_rsp;
    int32_t numDataElements;

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
        for (int tc = 0; tc < sizeof(antElm)/sizeof(antElm[0]); tc ++){

            numDataElements = 2*antElm[tc];

            // Generate input data
            for (int m = 0; m < numRB; ++m)
            {
              auto shiftVal = randExpShift(gen);
              for (int n = 0; n < numDataElements; ++n)
              {
                expandedData.dataExpanded[m * numDataElements + n] = int16_t(randInt16(gen) >> shiftVal);
              }
            }

            BlockFloatCompander::CompressedData compressedData;
            compressedData.dataCompressed = &loc_dataCompressedDataOut[0];

            std::memset(&loc_dataCompressedDataOut[0], 0, 288*numDataElements);
            std::memset(&loc_dataExpandedRes[0], 0, 288*numDataElements);

            std::memset(&bfp_com_req, 0, sizeof(struct xranlib_compress_request));
            std::memset(&bfp_com_rsp, 0, sizeof(struct xranlib_compress_response));
            std::memset(&bfp_decom_req, 0, sizeof(struct xranlib_decompress_request));
            std::memset(&bfp_decom_rsp, 0, sizeof(struct xranlib_decompress_response));

            bfp_com_req.data_in    = (int16_t *)expandedData.dataExpanded;
            bfp_com_req.numRBs     = numRB;
            bfp_com_req.numDataElements = numDataElements;
            bfp_com_req.len        = antElm[tc]*4;
            bfp_com_req.compMethod = compMethod;
            bfp_com_req.iqWidth    = iqWidth[iq_w_id];

            bfp_com_rsp.data_out   = (int8_t *)(compressedData.dataCompressed);
            bfp_com_rsp.len        = 0;

            xranlib_compress_avx512_bfw(&bfp_com_req, &bfp_com_rsp);

            bfp_decom_req.data_in         = (int8_t *)(compressedData.dataCompressed);
            bfp_decom_req.numRBs          = numRB;
            bfp_decom_req.numDataElements = numDataElements;
            bfp_decom_req.len             = bfp_com_rsp.len;
            bfp_decom_req.compMethod      = compMethod;
            bfp_decom_req.iqWidth         = iqWidth[iq_w_id];

            bfp_decom_rsp.data_out   = (int16_t *)expandedDataRes.dataExpanded;
            bfp_decom_rsp.len        = 0;

            xranlib_decompress_avx512_bfw(&bfp_decom_req, &bfp_decom_rsp);

            resSum += checkDataApprox(expandedData.dataExpanded, expandedDataRes.dataExpanded, numRB*numDataElements);

            ASSERT_EQ(antElm[tc]*4, bfp_decom_rsp.len);
            ASSERT_EQ(0, resSum);
         }
    }
}

TEST_P(BfpCheck, AVXSNC_cp_sweep_xranlib)
{
    int32_t resSum  = 0;
    int16_t len = 0;

    int16_t compMethod = XRAN_COMPMETHOD_BLKFLOAT;
    int16_t iqWidth[]    = {8, 9, 10, 12};
    int16_t numRB = 1;
    int16_t antElm[] = {8, 16, 32, 64};

    struct xranlib_decompress_request  bfp_decom_req;
    struct xranlib_decompress_response bfp_decom_rsp;

    struct xranlib_compress_request  bfp_com_req;
    struct xranlib_compress_response bfp_com_rsp;
    int32_t numDataElements;

    // Create random number generator
    std::random_device rd;
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<int16_t> randInt16(-32768, 32767);
    std::uniform_int_distribution<int> randExpShift(0, 4);

    BlockFloatCompander::ExpandedData expandedData;
    expandedData.dataExpanded = &loc_dataExpandedIn[0];
    BlockFloatCompander::ExpandedData expandedDataRes;
    expandedDataRes.dataExpanded = &loc_dataExpandedRes[0];

    if(_may_i_use_cpu_feature(_FEATURE_AVX512IFMA52) == 0)
        return;

    for (int iq_w_id = 0; iq_w_id < sizeof(iqWidth)/sizeof(iqWidth[0]); iq_w_id ++){
        for (int tc = 0; tc < sizeof(antElm)/sizeof(antElm[0]); tc ++){

            numDataElements = 2*antElm[tc];

            // Generate input data
            for (int m = 0; m < numRB; ++m)
            {
              auto shiftVal = randExpShift(gen);
              for (int n = 0; n < numDataElements; ++n)
              {
                expandedData.dataExpanded[m * numDataElements + n] = int16_t(randInt16(gen) >> shiftVal);
              }
            }

            BlockFloatCompander::CompressedData compressedData;
            compressedData.dataCompressed = &loc_dataCompressedDataOut[0];

            std::memset(&loc_dataCompressedDataOut[0], 0, 288*numDataElements);
            std::memset(&loc_dataExpandedRes[0], 0, 288*numDataElements);

            std::memset(&bfp_com_req, 0, sizeof(struct xranlib_compress_request));
            std::memset(&bfp_com_rsp, 0, sizeof(struct xranlib_compress_response));
            std::memset(&bfp_decom_req, 0, sizeof(struct xranlib_decompress_request));
            std::memset(&bfp_decom_rsp, 0, sizeof(struct xranlib_decompress_response));

            bfp_com_req.data_in    = (int16_t *)expandedData.dataExpanded;
            bfp_com_req.numRBs     = numRB;
            bfp_com_req.numDataElements = numDataElements;
            bfp_com_req.len        = antElm[tc]*4;
            bfp_com_req.compMethod = compMethod;
            bfp_com_req.iqWidth    = iqWidth[iq_w_id];

            bfp_com_rsp.data_out   = (int8_t *)(compressedData.dataCompressed);
            bfp_com_rsp.len        = 0;

            xranlib_compress_avxsnc_bfw(&bfp_com_req, &bfp_com_rsp);

            bfp_decom_req.data_in         = (int8_t *)(compressedData.dataCompressed);
            bfp_decom_req.numRBs          = numRB;
            bfp_decom_req.numDataElements = numDataElements;
            bfp_decom_req.len             = bfp_com_rsp.len;
            bfp_decom_req.compMethod      = compMethod;
            bfp_decom_req.iqWidth         = iqWidth[iq_w_id];

            bfp_decom_rsp.data_out   = (int16_t *)expandedDataRes.dataExpanded;
            bfp_decom_rsp.len        = 0;

            xranlib_decompress_avxsnc_bfw(&bfp_decom_req, &bfp_decom_rsp);

            resSum += checkDataApprox(expandedData.dataExpanded, expandedDataRes.dataExpanded, numRB*numDataElements);

            ASSERT_EQ(antElm[tc]*4, bfp_decom_rsp.len);
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

TEST_P(BfpPerfCp, AVX512_CpComp)
{
     performance("AVX512", module_name, xranlib_compress_avx512_bfw, &bfp_com_req, &bfp_com_rsp);
}

TEST_P(BfpPerfCp, AVX512_CpDeComp)
{
     performance("AVX512", module_name, xranlib_decompress_avx512_bfw, &bfp_decom_req, &bfp_decom_rsp);
}

TEST_P(BfpPerfEx,  AVXSNC_Comp)
{
    if(_may_i_use_cpu_feature(_FEATURE_AVX512IFMA52))
         performance("AVXSNC", module_name, xranlib_compress_avxsnc, &bfp_com_req, &bfp_com_rsp);
}

TEST_P(BfpPerfEx, AVXSNC_DeComp)
{
    if(_may_i_use_cpu_feature(_FEATURE_AVX512IFMA52))
        performance("AVXSNC", module_name, xranlib_decompress_avxsnc, &bfp_decom_req, &bfp_decom_rsp);
}

TEST_P(BfpPerfCp, AVXSNC_CpComp)
{
    if(_may_i_use_cpu_feature(_FEATURE_AVX512IFMA52))
        performance("AVXSNC", module_name, xranlib_compress_avxsnc_bfw, &bfp_com_req, &bfp_com_rsp);
}

TEST_P(BfpPerfCp, AVXSNC_CpDeComp)
{
    if(_may_i_use_cpu_feature(_FEATURE_AVX512IFMA52))
        performance("AVXSNC", module_name, xranlib_decompress_avxsnc_bfw, &bfp_decom_req, &bfp_decom_rsp);
}

INSTANTIATE_TEST_CASE_P(UnitTest, BfpCheck,
                        testing::ValuesIn(get_sequence(BfpCheck::get_number_of_cases("bfp_functional"))));

INSTANTIATE_TEST_CASE_P(UnitTest, BfpPerfEx,
                        testing::ValuesIn(get_sequence(BfpPerfEx::get_number_of_cases("bfp_performace_ex"))));

INSTANTIATE_TEST_CASE_P(UnitTest, BfpPerfCp,
                        testing::ValuesIn(get_sequence(BfpPerfCp::get_number_of_cases("bfp_performace_cp"))));

