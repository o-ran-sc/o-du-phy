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

#include "xran_common.h"
#include "xran_up_api.h"
#include "xran_fh_o_du.h"
#include "ethernet.h"

#include <stdint.h>

const std::string module_name = "U-Plane";

    /*union xran_test {
		struct {
            struct xran_cp_radioapp_common_header cmnhdr;
            struct xran_radioapp_udComp_header udComp;
            uint8_t     reserved;
		}field;
		struct {
			uint32_t     data_one;
			uint32_t     data_two;
		}all_data;
    };*/
    void fucntional_dl(struct rte_mbuf *test_buffer, char * iq_offset)
    {
        enum xran_pkt_dir direction =  XRAN_DIR_DL;
        uint16_t section_id = 7;
        enum xran_input_byte_order iq_buf_byte_order = XRAN_CPU_LE_BYTE_ORDER;
        uint8_t frame_id = 99;
        uint8_t subframe_id  = 9;
        uint8_t slot_id = 10;
        uint8_t symbol_no = 7;
        int prb_start = 0;
        int prb_num = 66;
        uint8_t CC_ID = 0;
        uint8_t RU_Port_ID = 0;
        uint8_t seq_id =0;
        uint32_t do_copy = 0;
        uint8_t compMeth = 0;
        enum xran_comp_hdr_type staticEn = XRAN_COMP_HDR_TYPE_DYNAMIC;
        uint8_t iqWidth =  16;

        int32_t prep_bytes;
		
        prep_bytes = prepare_symbol_ex(direction,
                                section_id,
                                test_buffer,
                                (uint8_t *)iq_offset,
                                compMeth,
                                iqWidth,
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
                                do_copy,
                                staticEn);
		
		/*union xran_cp_radioapp_section_ext11 *ext11 = NULL;
		struct xran_sectionext11_info *params = NULL;
		int i;
		ext11 = (union xran_cp_radioapp_section_ext11 *)(iq_offset);
		params = (struct xran_sectionext11_info *)(iq_offset+100);
		
		params->RAD = 1;
		params->disableBFWs = 1;
		params->numBundPrb = 1;
		params->bfwCompMeth = 2;
		
        for(i = 0; i< 10000; i++)
		{
			ext11->data_field.data_field1 = (XRAN_CP_SECTIONEXTCMD_11 << 24) | (params->RAD << 7) | (params->disableBFWs << 8);
			//ext11->data_field.data_field1 = (XRAN_CP_SECTIONEXTCMD_11 << 24)'
            //ext11->all_bits.RAD          = params->RAD;
            //ext11->all_bits.disableBFWs  = params->disableBFWs;
			ext11->data_field.data_field2 = (params->bfwCompMeth << 4) | params->numBundPrb;
            //ext11->all_bits.numBundPrb   = params->numBundPrb;
            //ext11->all_bits.bfwCompMeth  = params->bfwCompMeth;
			
		}*/
		//printf("ext11->data_field.data_field1 is %d\n", ext11->data_field.data_field1);
        //ASSERT_EQ(prep_bytes, 3168);
    }    
class U_planePerf : public KernelTests
{

protected:
    int32_t request;
    int32_t response;

    struct rte_mbuf *test_buffer;
    char * iq_offset;

    void SetUp() override
    {
        /* Parameters stored in the functional section will be used. GTest will call
           TEST_P (including SetUp and TearDown) for each case in the section. */
        init_test("u_plane_performace");
        test_buffer = (struct rte_mbuf*)rte_pktmbuf_alloc(_eth_mbuf_pool);

        /* buffer size defined as the maximum size of all inputs/outputs in BYTE */
        if(test_buffer == NULL) {
            std::cout << __func__ << ":" << __LINE__ << " Failed to allocatte a packet buffer!" << std::endl;
            return;
        }
        iq_offset = rte_pktmbuf_mtod(test_buffer, char * );
        iq_offset = iq_offset + sizeof(struct rte_ether_hdr) +
                                    sizeof (struct xran_ecpri_hdr) +
                                    sizeof (struct radio_app_common_hdr) +
                                    sizeof(struct data_section_hdr);
    }

    /* It's called after an execution of the each test case.*/
    void TearDown() override
    {
        rte_pktmbuf_free(test_buffer);
    }


};

TEST_P(U_planePerf, Perf)
{
    performance("C", module_name, &fucntional_dl, test_buffer, iq_offset);
}

INSTANTIATE_TEST_CASE_P(UnitTest, U_planePerf,
                        testing::ValuesIn(get_sequence(U_planePerf::get_number_of_cases("u_plane_performance"))));


