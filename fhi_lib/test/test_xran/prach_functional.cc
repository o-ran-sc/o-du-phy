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
#include "xran_fh_o_du.h"
#include "ethernet.h"
#include "xran_transport.h"
#include "xran_cp_api.h"

#include <stdint.h>


const std::string module_name = "Prach_test";


class PrachCheck : public KernelTests
{
private:
    struct xran_section_gen_info *m_pSectResult = NULL;

protected:
    struct xran_fh_config *m_xranConf;
    struct xran_device_ctx m_xran_dev_ctx;
    struct xran_prach_config *m_pPRACHConfig;
    struct xran_ru_config *m_pRUConfig;
	struct xran_prach_cp_config *m_pPrachCPConfig;

    struct xran_section_gen_info *m_pSectGenInfo = NULL;
    int m_maxSections = 8;  /*  not used */
    int lastsymbol;

    struct rte_mbuf *m_pTestBuffer;

    struct xran_cp_gen_params m_params;
    struct xran_recv_packet_info m_pktInfo;
    struct xran_cp_gen_params m_result;

    uint8_t     m_dir;
    std::string m_dirStr;
    uint8_t     m_sectionType;

    uint8_t     m_ccId, m_antId;
    uint8_t     m_seqId;
    uint8_t     m_frameId, m_subframeId, m_slotId;
    uint8_t     m_symStart, m_symNum;
    uint16_t    *m_prbStart = NULL, *m_prbNum = NULL;

    uint8_t     m_iqWidth, m_compMethod;
    uint16_t    m_beamId;
    uint16_t    m_reMask = 0xfff;
    uint16_t    m_sectionId;
    uint8_t     m_filterIndex;
    //uint16_t    m_timeOffset;
    uint8_t     m_fftSize;

    //define reference values
    uint8_t     m_startSymId;
    uint8_t     m_x;
    uint8_t        m_filterIdx;
    uint16_t       m_startPrbc;
    uint8_t        m_numPrbc;
    uint8_t        m_numSymbol;
    uint16_t       m_timeOffset;
    int32_t        m_freqOffset;
    uint8_t       m_nrofPrachInSlot;
    uint8_t        m_occassionsInPrachSlot;
    uint8_t        m_y[XRAN_PRACH_CANDIDATE_Y];
    uint8_t     m_isPRACHslot[XRAN_PRACH_CANDIDATE_SLOT];
    int         m_prach_start_symbol;
    int            m_prach_last_symbol;
    uint8_t     m_SlotNrNum;
    uint16_t    m_m_params_timeOffset;
    uint16_t    m_id;

    void SetUp() override
    {
        init_test("prach_functional");
        memset(&m_xran_dev_ctx, 0, sizeof(struct xran_device_ctx));
        //modify
        m_xranConf = &m_xran_dev_ctx.fh_cfg;

        m_pPRACHConfig = &m_xranConf->prach_conf;
        m_pRUConfig = &m_xranConf->ru_conf;
        m_pPrachCPConfig = &m_xran_dev_ctx.PrachCPConfig;
        //initialize input parameters
        m_xranConf->frame_conf.nNumerology = get_input_parameter<uint8_t>("Numerology");
        m_xranConf->frame_conf.nFrameDuplexType = get_input_parameter<uint8_t>("FrameDuplexType");
        m_xranConf->log_level = get_input_parameter<uint32_t>("loglevel");
        m_pPRACHConfig->nPrachConfIdx = get_input_parameter<uint8_t>("PrachConfIdx");
        m_pPRACHConfig->nPrachFreqStart = get_input_parameter<uint16_t>("PrachFreqStart");
        m_pPRACHConfig->nPrachFreqOffset = get_input_parameter<int32_t>("PrachFreqOffset");
        m_pPRACHConfig->nPrachSubcSpacing = get_input_parameter<uint8_t>("PrachSubcSpacing");

        m_pRUConfig->iqWidth = get_input_parameter<uint8_t>("iqWidth");
        m_pRUConfig->compMeth = get_input_parameter<uint8_t>("compMeth");
        m_pRUConfig->fftSize = get_input_parameter<uint8_t>("fftSize");

        m_frameId = get_input_parameter<uint8_t>("frameId");
        m_subframeId = get_input_parameter<uint8_t>("subframeId");
        m_slotId = get_input_parameter<uint8_t>("slotId");
        m_beamId = get_input_parameter<uint16_t>("beamId");
        m_ccId = get_input_parameter<uint8_t>("ccId");
        m_antId = get_input_parameter<uint8_t>("antId");

        //initialize reference output
        m_startSymId = get_reference_parameter<uint8_t>("startSymId");
        m_x = get_reference_parameter<uint8_t>("x_value");

        m_filterIdx = get_reference_parameter<uint8_t>("filterIdx");
        m_startPrbc = get_reference_parameter<uint16_t>("startPrbc");
        m_numPrbc = get_reference_parameter<uint8_t>("numPrbc");
        m_timeOffset = get_reference_parameter<uint16_t>("timeOffset");
        m_freqOffset = get_reference_parameter<uint32_t>("freqOffset");
        m_nrofPrachInSlot = get_reference_parameter<uint8_t>("nrofPrachInSlot");
        m_m_params_timeOffset = get_reference_parameter<uint16_t>("m_params_timeOffset");
        m_id = get_reference_parameter<uint16_t>("id");
        std::vector<uint8_t> y_vec = get_reference_parameter<std::vector<uint8_t>>("y_value");
        for(int i=0; i < XRAN_PRACH_CANDIDATE_Y; i++) {
            m_y[i] = y_vec[i];
            }

        m_numSymbol = get_reference_parameter<uint8_t>("numSymbol");
        m_occassionsInPrachSlot = get_reference_parameter<uint8_t>("occassionsInPrachSlot");

        std::vector<uint8_t> index_vec = get_reference_parameter<std::vector<uint8_t>>("isPRACHslot");
        m_SlotNrNum = get_reference_parameter<uint8_t>("SlotNrNum");
        for(int i = 0; i < XRAN_PRACH_CANDIDATE_SLOT; i++){
            m_isPRACHslot[i]=0;
            }
        for(int i=0; i<m_SlotNrNum;i++){
            m_isPRACHslot[index_vec[i]]=1;
            }


        m_prach_start_symbol = get_reference_parameter<int>("prach_start_symbol");
        m_prach_last_symbol = get_reference_parameter<int>("prach_last_symbol");

        /* allocate and prepare required data storage */
        m_pSectGenInfo = new struct xran_section_gen_info[8];
        ASSERT_NE(m_pSectGenInfo, nullptr);
        m_params.sections = m_pSectGenInfo;

        /* allocating an mbuf for packet generatrion */
        m_pTestBuffer = (struct rte_mbuf*)rte_pktmbuf_alloc(_eth_mbuf_pool);
        ASSERT_FALSE(m_pTestBuffer == NULL);

    }

    void TearDown() override
    {
        if(m_pTestBuffer != NULL)
            rte_pktmbuf_free(m_pTestBuffer);
        if(m_pSectGenInfo)
            delete[] m_pSectGenInfo;
        return;
    }
};


TEST_P(PrachCheck, PrachPacketGen)//TestCaseName   TestName
{
    int ret;
    int32_t i;
    void *pHandle = NULL;

    /* Preparing input data for prach config */
    ret = xran_init_prach(m_xranConf, &m_xran_dev_ctx);
    ASSERT_TRUE(ret == XRAN_STATUS_SUCCESS);

    /* Verify the result */
    EXPECT_EQ(m_pPrachCPConfig->filterIdx, m_filterIdx);
    EXPECT_EQ(m_pPrachCPConfig->startSymId, m_startSymId);
    EXPECT_EQ(m_pPrachCPConfig->startPrbc, m_startPrbc);
    EXPECT_EQ(m_pPrachCPConfig->numPrbc, m_numPrbc);
    EXPECT_EQ(m_pPrachCPConfig->timeOffset, m_timeOffset);
    EXPECT_EQ(m_pPrachCPConfig->freqOffset, m_freqOffset);
    EXPECT_EQ(m_pPrachCPConfig->x, m_x);
    EXPECT_EQ(m_pPrachCPConfig->nrofPrachInSlot, m_nrofPrachInSlot);
    EXPECT_EQ(m_pPrachCPConfig->y[0], m_y[0]);
    EXPECT_EQ(m_pPrachCPConfig->y[1], m_y[1]);
    EXPECT_EQ(m_pPrachCPConfig->numSymbol, m_numSymbol);
    EXPECT_EQ(m_pPrachCPConfig->occassionsInPrachSlot, m_occassionsInPrachSlot);
    for (i = 0; i < XRAN_PRACH_CANDIDATE_SLOT; i++){
        EXPECT_EQ(m_pPrachCPConfig->isPRACHslot[i], m_isPRACHslot[i]);
    }
    for (i = 0; i < XRAN_MAX_SECTOR_NR; i++){
        EXPECT_EQ(m_xran_dev_ctx.prach_start_symbol[i], m_prach_start_symbol);
        EXPECT_EQ(m_xran_dev_ctx.prach_last_symbol[i], m_prach_last_symbol);
    }

    ret = xran_open(pHandle, m_xranConf);
    ASSERT_TRUE(ret == XRAN_STATUS_SUCCESS);

    ret = generate_cpmsg_prach(&m_xran_dev_ctx, &m_params, m_pSectGenInfo, m_pTestBuffer, &m_xran_dev_ctx,
        m_frameId, m_subframeId, m_slotId,
        m_beamId, m_ccId, m_antId, 0, 0);
    ASSERT_TRUE(ret == XRAN_STATUS_SUCCESS);
    /* Verify the result */
    EXPECT_EQ(m_params.sectionType, XRAN_CP_SECTIONTYPE_3);
    EXPECT_EQ(m_params.dir, XRAN_DIR_UL);
    EXPECT_EQ(m_params.hdr.filterIdx, m_filterIdx);
    EXPECT_EQ(m_params.hdr.frameId, m_frameId);
    EXPECT_EQ(m_params.hdr.subframeId, m_subframeId);
    EXPECT_EQ(m_params.hdr.slotId, m_slotId);
    EXPECT_EQ(m_params.hdr.startSymId, m_startSymId);
    EXPECT_EQ(m_params.hdr.iqWidth, (m_pRUConfig->iqWidth==16)?0:m_pRUConfig->iqWidth);
    EXPECT_EQ(m_params.hdr.compMeth,m_pRUConfig->compMeth );
    EXPECT_EQ(m_params.hdr.timeOffset, m_m_params_timeOffset);
    EXPECT_EQ(m_params.hdr.fftSize,m_pRUConfig->fftSize);
    EXPECT_EQ(m_params.hdr.scs, m_pPRACHConfig->nPrachSubcSpacing);
    EXPECT_EQ(m_params.hdr.cpLength, 0);
    EXPECT_EQ(m_params.numSections, 1);

    EXPECT_EQ(m_params.sections[0].info.type, XRAN_CP_SECTIONTYPE_3);
    EXPECT_EQ(m_params.sections[0].info.startSymId, m_startSymId);
    EXPECT_EQ(m_params.sections[0].info.iqWidth, (m_pRUConfig->iqWidth==16)?0:m_pRUConfig->iqWidth);
    EXPECT_EQ(m_params.sections[0].info.compMeth, m_pRUConfig->compMeth);

    EXPECT_EQ(m_params.sections[0].info.id, m_id);
    EXPECT_EQ(m_params.sections[0].info.rb, XRAN_RBIND_EVERY);
    EXPECT_EQ(m_params.sections[0].info.symInc, XRAN_SYMBOLNUMBER_NOTINC);
    EXPECT_EQ(m_params.sections[0].info.startPrbc, m_startPrbc);
    EXPECT_EQ(m_params.sections[0].info.numPrbc, m_numPrbc);
    EXPECT_EQ(m_params.sections[0].info.numSymbol, m_numSymbol);
    EXPECT_EQ(m_params.sections[0].info.reMask, 0xfff);
    EXPECT_EQ(m_params.sections[0].info.beamId, m_beamId);
    EXPECT_EQ(m_params.sections[0].info.freqOffset, m_freqOffset);
    EXPECT_EQ(m_xran_dev_ctx.prach_last_symbol[m_ccId], m_prach_last_symbol);
    EXPECT_EQ(m_params.sections[0].info.ef, 0);
    EXPECT_EQ(m_params.sections[0].exDataSize, 0);

}


INSTANTIATE_TEST_CASE_P(UnitTest, PrachCheck,
        testing::ValuesIn(get_sequence(PrachCheck::get_number_of_cases("prach_functional"))));

