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


class PrachPerf : public KernelTests
{

	private:
    struct xran_section_gen_info *m_pSectResult = NULL;

	protected:
		struct xran_fh_config m_xranConf;
		struct xran_device_ctx m_xran_dev_ctx;
		struct xran_prach_config *m_pPRACHConfig;
		struct xran_prach_cp_config  *m_pPrachCPConfig;

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
		uint8_t    	m_filterIdx;
		uint16_t   	m_startPrbc;
		uint8_t    	m_numPrbc;
		uint8_t    	m_numSymbol;
		uint16_t   	m_timeOffset;
		int32_t    	m_freqOffset;
		uint8_t   	m_nrofPrachInSlot;
		uint8_t    	m_occassionsInPrachSlot;
		uint8_t    	m_y[XRAN_PRACH_CANDIDATE_Y];
		uint8_t     m_isPRACHslot[XRAN_PRACH_CANDIDATE_SLOT];
		int 		m_prach_start_symbol;
		int			m_prach_last_symbol;
		uint8_t     m_SlotNrNum;


	void SetUp() override
  {
    	init_test("prach_performance");
		memset(&m_xranConf, 0, sizeof(struct xran_fh_config));
		memset(&m_xran_dev_ctx, 0, sizeof(struct xran_device_ctx));
		m_pPRACHConfig = &m_xranConf.prach_conf;
		m_pPrachCPConfig = &m_xran_dev_ctx.PrachCPConfig;

		//initialize input parameters
        m_xranConf.frame_conf.nNumerology = get_input_parameter<uint8_t>("Numerology");
        m_xranConf.frame_conf.nFrameDuplexType = get_input_parameter<uint8_t>("FrameDuplexType");
        m_xranConf.log_level = get_input_parameter<uint32_t>("loglevel");

        m_pPRACHConfig->nPrachConfIdx = get_input_parameter<uint8_t>("PrachConfIdx");
        m_pPRACHConfig->nPrachFreqStart = get_input_parameter<uint16_t>("PrachFreqStart");
        m_pPRACHConfig->nPrachFreqOffset = get_input_parameter<int32_t>("PrachFreqOffset");
        m_pPRACHConfig->nPrachSubcSpacing = get_input_parameter<uint8_t>("PrachSubcSpacing");


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

		//get the values from a vector
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

void performance_cp(void *pHandle,struct xran_cp_gen_params *params, struct xran_section_gen_info *sect_geninfo, struct xran_device_ctx *pxran_lib_ctx,
                uint8_t frame_id, uint8_t subframe_id, uint8_t slot_id,
                uint16_t beam_id, uint8_t cc_id, uint8_t prach_port_id, uint8_t seq_id)
{
  	struct rte_mbuf *mbuf;

    mbuf = (struct rte_mbuf*)rte_pktmbuf_alloc(_eth_mbuf_pool);

    generate_cpmsg_prach(pxran_lib_ctx, params, sect_geninfo, mbuf, pxran_lib_ctx,
        frame_id, subframe_id, slot_id,
        beam_id, cc_id, prach_port_id, 0, seq_id);

    seq_id++;

    rte_pktmbuf_free(mbuf);
}

TEST_P(PrachPerf, PrachPerfPacketGen)//TestCaseName   TestName
{
    int ret;
    void *pHandle = NULL;

    /* Preparing input data for prach config */
	ret = xran_init_prach(&m_xranConf, &m_xran_dev_ctx);
    ASSERT_TRUE(ret == XRAN_STATUS_SUCCESS);


    ret = generate_cpmsg_prach(&m_xran_dev_ctx, &m_params, m_pSectGenInfo, m_pTestBuffer, &m_xran_dev_ctx,
        m_frameId, m_subframeId, m_slotId,
        m_beamId, m_ccId, m_antId, 0, 0);
    ASSERT_TRUE(ret == XRAN_STATUS_SUCCESS);


	performance("C", module_name,
            &performance_cp, pHandle, &m_params, m_pSectGenInfo, &m_xran_dev_ctx,
        m_frameId, m_subframeId, m_slotId,
        m_beamId, m_ccId, m_antId, 0);
}


INSTANTIATE_TEST_CASE_P(UnitTest, PrachPerf,
        testing::ValuesIn(get_sequence(PrachPerf::get_number_of_cases("prach_performance"))));

