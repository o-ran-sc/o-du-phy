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
#include "xran_mod_compression.h"

#include <stdint.h>
#include <random>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <cstring>

const std::string module_name = "mod_compression";

extern int _may_i_use_cpu_feature(unsigned __int64);

int16_t loc_ModCompIn[273*12*14*2*16*2];
int8_t loc_ModCompOut[273*12*14*2*16];

class Mod_CompressionPerf : public KernelTests
{
protected:
    struct xranlib_5gnr_mod_compression_request  mod_com_req;
    struct xranlib_5gnr_mod_compression_response mod_com_rsp;

    void SetUp() override {
        init_test("mod_compression_performace");
        // Create random number generator
        std::random_device rd;
        std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
        std::uniform_int_distribution<int16_t> randInt16(-32768, 32767);
        std::uniform_int_distribution<int> randExpShift(0, 4);
        std::memset(&loc_ModCompOut[0], 0, 273*12);

        std::memset(&mod_com_req, 0, sizeof(struct xranlib_5gnr_mod_compression_request));
        std::memset(&mod_com_rsp, 0, sizeof(struct xranlib_5gnr_mod_compression_response));
        mod_com_req.unit = get_input_parameter<int16_t>("unit");
        mod_com_req.modulation = get_input_parameter<xran_modulation_order>("modulation");
        mod_com_req.num_symbols = get_input_parameter<int32_t>("num_symbols");

        for (int m = 0; m < 2*mod_com_req.num_symbols; ++m) {
            loc_ModCompIn[m] = int16_t(randInt16(gen));
        }

        mod_com_req.data_in    = (int16_t *)loc_ModCompIn;
        mod_com_rsp.data_out   = (int8_t *)(loc_ModCompOut);
    }

    /* It's called after an execution of the each test case.*/
    void TearDown() override {

    }
};

TEST_P(Mod_CompressionPerf, AVX512_Mod_Comp)
{
     performance("AVX512", module_name, xranlib_5gnr_mod_compression_avx512, &mod_com_req, &mod_com_rsp);
}


TEST_P(Mod_CompressionPerf,  AVXSNC_Mod_Comp)
{
    if(_may_i_use_cpu_feature(_FEATURE_AVX512IFMA52))
         performance("AVXSNC", module_name, xranlib_5gnr_mod_compression_snc, &mod_com_req, &mod_com_rsp);
}

INSTANTIATE_TEST_CASE_P(UnitTest, Mod_CompressionPerf,
                        testing::ValuesIn(get_sequence(Mod_CompressionPerf::get_number_of_cases("mod_compression_performace"))));
