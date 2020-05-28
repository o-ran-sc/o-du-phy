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

#include "xran_common.h"
#include "xran_fh_o_du.h"
#include "ethernet.h"

#include <stdint.h>

const std::string module_name = "u-plane";

class U_planePerf : public KernelTests
{

protected:
    int32_t request;
    int32_t response;

    struct rte_mbuf *test_buffer;
    char * iq_offset;
    struct rte_mempool *test_eth_mbuf_pool;

    void SetUp() override
    {
        /* Parameters stored in the functional section will be used. GTest will call
           TEST_P (including SetUp and TearDown) for each case in the section. */
        init_test("u_plane_performace");
        test_eth_mbuf_pool = rte_pktmbuf_pool_create("mempool", NUM_MBUFS,
                MBUF_CACHE, 0, MBUF_POOL_ELEMENT, rte_socket_id());

        /* buffer size defined as the maximum size of all inputs/outputs in BYTE */
        const int buffer_size = 9600;
        test_buffer = (struct rte_mbuf*)rte_pktmbuf_alloc(test_eth_mbuf_pool);

        iq_offset = rte_pktmbuf_mtod(test_buffer, char * );
        iq_offset = iq_offset + sizeof(struct ether_hdr) +
                                    sizeof (struct xran_ecpri_hdr) +
                                    sizeof (struct radio_app_common_hdr) +
                                    sizeof(struct data_section_hdr);
    }

    /* It's called after an execution of the each test case.*/
    void TearDown() override
    {
        rte_pktmbuf_free(test_buffer);
    }

    void fucntional_dl(F function, int32_t* request, int32_t* response)
    {
        enum xran_pkt_dir direction =  XRAN_DIR_DL;
        uint16_t section_id = 0;
        enum xran_input_byte_order iq_buf_byte_order = XRAN_CPU_LE_BYTE_ORDER;
        uint8_t frame_id = 0;
        uint8_t subframe_id  = 0;
        uint8_t slot_id = 0;
        uint8_t symbol_no = 0;
        int prb_start = 0;
        int prb_num = 66;
        uint8_t CC_ID = 0;
        uint8_t RU_Port_ID = 0;
        uint8_t seq_id =0;
        uint32_t do_copy = 0;

        int32_t prep_bytes;

        prep_bytes = prepare_symbol_ex(direction,
                                    section_id,
                                    test_buffer,
                                    (struct rb_map *)iq_offset,
                                    iq_buf_byte_order,
                                    frame_id,
                                    subframe_id,
                                    slot_id,
                                    symbol_no,
                                    prb_start,
                                    prb_num,
                                    CC_ID,
                                    RU_Port_ID,
                                    seq_id,
                                    do_copy);

        //ASSERT_EQ(prep_bytes, 3168);
    }
};

TEST_P(U_planePerf, Test_DL)
{
    performance("C", module_name, fucntional_dl, &request, &response);
}

INSTANTIATE_TEST_CASE_P(UnitTest, U_planePerf,
                        testing::ValuesIn(get_sequence(U_planePerf::get_number_of_cases("u_plane_performance"))));


