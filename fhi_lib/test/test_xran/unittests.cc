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

#include <limits.h>
#include "xran_compression.h"
#include "gtest/gtest.h"

#define MAX_IQ (273*12*2)// 273 RBs, 12 SC, 32bits IQ

int8_t iCompressionOutput_AVX_A[MAX_IQ + MAX_IQ/24]; /* 273 RB 8 bits IQ + exponent */
int16_t iDeCompressionOutput_AVX_A[MAX_IQ]; /* 273 RB 16bits IQ */
int16_t iCompressionInput[MAX_IQ]; /* 273 RB 16bits IQ */
int16_t iLength = 273*12*2; // total 16bits IQ

// Tests
/*
TEST(Compression, Zero) {
  EXPECT_EQ(0, xran_bfp_comp_avx512_fun_a(iCompressionInput,iCompressionOutput_AVX_A, iLength));
}



TEST(Decompression, Zero) {
    iLength= 24+1;
    EXPECT_EQ(0, xran_bfp_decomp_avx512_fun_a(iCompressionOutput_AVX_A,iDeCompressionOutput_AVX_A,iLength));
}*/


