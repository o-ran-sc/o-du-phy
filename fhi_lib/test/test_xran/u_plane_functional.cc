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

const std::string module_name = "u-plane";

class U_planeCheck : public KernelTests
{
protected:
    struct rte_mbuf *test_buffer;
    char * iq_offset;
    struct rte_mempool *test_eth_mbuf_pool;

    void SetUp() override
    {
        init_test("u_plane_functional");
        test_buffer = (struct rte_mbuf*)rte_pktmbuf_alloc(_eth_mbuf_pool);

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

/* simple test of DL and UL functionality */
TEST_P(U_planeCheck, Test_DLUL)
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
    struct xran_ecpri_hdr *ecpri_hdr = NULL;
    struct radio_app_common_hdr *app_hdr = NULL;
    struct data_section_hdr *section_hdr = NULL;

    char *pChar = NULL;
    uint16_t payl_size = 0;
    struct data_section_hdr res_sect;

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

    ASSERT_EQ(prep_bytes, 3168);

    pChar = rte_pktmbuf_mtod(test_buffer, char*);

    ecpri_hdr = (struct xran_ecpri_hdr *)(pChar + sizeof(struct rte_ether_hdr));
    app_hdr = (struct radio_app_common_hdr *)(pChar + sizeof(struct rte_ether_hdr)
                                              + sizeof (struct xran_ecpri_hdr));
    section_hdr = (struct data_section_hdr *)(pChar + sizeof(struct rte_ether_hdr) +
                                            sizeof (struct xran_ecpri_hdr) +
                                            sizeof(struct radio_app_common_hdr));

    ASSERT_EQ (ecpri_hdr->cmnhdr.bits.ecpri_mesg_type,  ECPRI_IQ_DATA);
    payl_size =  rte_be_to_cpu_16(ecpri_hdr->cmnhdr.bits.ecpri_payl_size);
    ASSERT_EQ (payl_size,  3180);

    ASSERT_EQ(app_hdr->data_feature.data_direction, direction);
    ASSERT_EQ(app_hdr->frame_id, frame_id);

    res_sect.fields.all_bits = rte_be_to_cpu_32(section_hdr->fields.all_bits);
    ASSERT_EQ(res_sect.fields.num_prbu, prb_num);
    ASSERT_EQ(res_sect.fields.sect_id, section_id);

    {
        /* UL direction */
        void *iq_samp_buf;
        union ecpri_seq_id seq;
        int num_bytes = 0;

        uint8_t CC_ID = 0;
        uint8_t Ant_ID = 0;
        uint8_t frame_id = 0;
        uint8_t subframe_id = 0;
        uint8_t slot_id = 0;
        uint8_t symb_id = 0;

        uint8_t compMeth = 0;
        uint8_t iqWidth = 0;


        uint16_t num_prbu;
        uint16_t start_prbu;
        uint16_t sym_inc;
        uint16_t rb;
        uint16_t sect_id;

        int32_t prep_bytes;

        char *pChar = NULL;


        num_bytes = xran_extract_iq_samples(test_buffer,
                                &iq_samp_buf,
                                &CC_ID,
                                &Ant_ID,
                                &frame_id,
                                &subframe_id,
                                &slot_id,
                                &symb_id,
                                &seq,
                                &num_prbu,
                                &start_prbu,
                                &sym_inc,
                                &rb,
                                &sect_id,
                                0,
                                XRAN_COMP_HDR_TYPE_DYNAMIC,
                                &compMeth,
                                &iqWidth);

        ASSERT_EQ(num_bytes, 3182);
        ASSERT_EQ(prb_num, 66);
        ASSERT_EQ(CC_ID, 0);
    }
}

INSTANTIATE_TEST_CASE_P(UnitTest, U_planeCheck,
                        testing::ValuesIn(get_sequence(U_planeCheck::get_number_of_cases("u_plane_functional"))));


