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
#include "xran_lib_wrap.hpp"
#include "xran_common.h"
#include "xran_fh_o_du.h"
#include "ethdi.h"
#include "ethernet.h"
#include "xran_transport.h"
#include "xran_cp_api.h"

#include <stdint.h>


const std::string module_name = "C-Plane";

const uint8_t m_bitmask[] = { 0x00, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff };

extern "C"
{

/* wrapper function for performace tests to reset mbuf */
int xran_ut_prepare_cp(struct rte_mbuf *mbuf, struct xran_cp_gen_params *params,
                        uint8_t cc_id, uint8_t ant_id, uint8_t seq_id)
{
    rte_pktmbuf_reset(mbuf);
    return(xran_prepare_ctrl_pkt(mbuf, params, cc_id, ant_id, seq_id));
}


void cput_fh_rx_callback(void *pCallbackTag, xran_status_t status)
{
    return;
}

void cput_fh_rx_prach_callback(void *pCallbackTag, xran_status_t status)
{
    rte_pause();
}

} /* extern "C" */



class C_plane: public KernelTests
{
private:
    struct xran_section_gen_info *m_pSectGenInfo = NULL;
    struct xran_section_gen_info *m_pSectResult = NULL;


protected:
    int m_maxSections = 8;  /*  not used */
    int m_numSections;

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
    uint16_t    m_timeOffset;
    uint8_t     m_fftSize;
    uint8_t     m_scs;
    uint16_t    m_cpLength;
    int         m_freqOffset;

    uint16_t  m_ext1_dst_len = 0;
    int8_t   *m_p_ext1_dst   = NULL;
    int16_t  *m_p_bfw_iq_src = NULL;

    struct xran_sectionext1_info m_ext1;
    struct xran_sectionext2_info m_ext2;
    struct xran_sectionext4_info m_ext4;
    struct xran_sectionext5_info m_ext5;

    int16_t m_bfwIQ[XRAN_MAX_BFW_N*2];


    void SetUp() override
    {
        int i, j;

        init_test("C_Plane");

        m_numSections   = get_input_parameter<int>("num_sections");
        ASSERT_FALSE(m_numSections == 0);

        m_dirStr        = get_input_parameter<std::string>("direction");

        if(!m_dirStr.compare("DL")) m_dir = XRAN_DIR_DL;
        else if(!m_dirStr.compare("UL")) m_dir = XRAN_DIR_UL;
        else FAIL() << "Invalid direction!";

        m_sectionType   = get_input_parameter<uint8_t>("section_type");
        m_ccId          = get_input_parameter<uint8_t>("cc_id");
        m_antId         = get_input_parameter<uint8_t>("ant_id");
        m_seqId         = get_input_parameter<uint16_t>("seq_id");

        m_frameId       = get_input_parameter<uint8_t>("frame_id");
        m_subframeId    = get_input_parameter<uint8_t>("subframe_id");
        m_slotId        = get_input_parameter<uint8_t>("slot_id");
        m_symStart      = get_input_parameter<uint8_t>("symbol_start");
        m_compMethod    = get_input_parameter<uint8_t>("comp_method");
        m_iqWidth       = get_input_parameter<uint8_t>("iq_width");

        m_sectionId     = get_input_parameter<uint8_t>("section_id");
        m_symNum        = get_input_parameter<uint8_t>("symbol_num");
        m_beamId        = get_input_parameter<uint16_t>("beam_id");

        /* reading configurations of start prb and the number of prbs  */
        std::vector<int> prbstart = get_input_parameter<std::vector<int>>("prb_start");
        std::vector<int> prbnum = get_input_parameter<std::vector<int>>("prb_num");
        /* number of sections and  the pair of start/number of prb shall be matched */
        ASSERT_TRUE((m_numSections == prbstart.size())
                    && (m_numSections == prbnum.size())
                    && (prbstart.size() == prbnum.size()));

        m_prbStart  = new uint16_t [m_numSections];
        m_prbNum    = new uint16_t [m_numSections];
        for(i=0; i < m_numSections; i++) {
            m_prbStart[i] = prbstart[i];
            m_prbNum[i] = prbnum[i];
            }

        switch(m_sectionType) {
            case XRAN_CP_SECTIONTYPE_1:
                m_filterIndex = XRAN_FILTERINDEX_STANDARD;
                break;

            case XRAN_CP_SECTIONTYPE_3:
                m_filterIndex   = get_input_parameter<uint8_t>("filter_index");
                m_timeOffset    = get_input_parameter<uint16_t>("time_offset");
                m_fftSize       = get_input_parameter<uint8_t>("fft_size");
                m_scs           = get_input_parameter<uint8_t>("scs");
                m_cpLength      = get_input_parameter<uint16_t>("cp_length");
                m_freqOffset    = get_input_parameter<int>("freq_offset");
                break;

            default:
                FAIL() << "Invalid Section Type - " << m_sectionType << "\n";
            }

        /* allocate and prepare required data storage */
        m_pSectGenInfo = new struct xran_section_gen_info [m_numSections];
        ASSERT_NE(m_pSectGenInfo, nullptr);
        m_params.sections = m_pSectGenInfo;

        m_pSectResult = new struct xran_section_gen_info [m_numSections];
        ASSERT_NE(m_pSectResult, nullptr);
        m_result.sections = m_pSectResult;

        m_ext1_dst_len   = 9600;
        m_p_ext1_dst   = new int8_t [m_ext1_dst_len];
        m_p_bfw_iq_src = new int16_t [9600/2];

        /* allocating an mbuf for packet generatrion */
        m_pTestBuffer = xran_ethdi_mbuf_alloc();

        ASSERT_FALSE(m_pTestBuffer == NULL);
    }

    void TearDown() override
    {
        int i, j;

        if(m_pTestBuffer != NULL)
            rte_pktmbuf_free(m_pTestBuffer);

        if(m_prbStart)
            delete[] m_prbStart;
        if(m_prbNum)
            delete[] m_prbNum;

        if(m_p_bfw_iq_src)
            delete[] m_p_bfw_iq_src;

        if(m_p_ext1_dst)
            delete[] m_p_ext1_dst;

        if(m_pSectGenInfo)
            delete[] m_pSectGenInfo;

        if(m_pSectResult) {
            delete[] m_pSectResult;
            }

    }

    int prepare_sections(bool extflag);
    int prepare_extensions(int sect_num);
    void verify_sections(void);

};



int C_plane::prepare_extensions(int sect_num)
{
    int i, numext;
    int N;


    N = 8;

    // extension 1
    m_ext1.bfwNumber  = 4*N; // 4 ant, 8 UEs
    m_ext1.bfwiqWidth = 16;
    m_ext1.bfwCompMeth    = XRAN_BFWCOMPMETHOD_NONE;
                            /* XRAN_BFWCOMPMETHOD_BLKFLOAT
                             * XRAN_BFWCOMPMETHOD_BLKSCALE
                             * XRAN_BFWCOMPMETHOD_ULAW
                             * XRAN_BFWCOMPMETHOD_BEAMSPACE
                             */
    m_ext1.p_bfwIQ = m_bfwIQ;

    switch (m_ext1.bfwCompMeth) {
        case XRAN_BFWCOMPMETHOD_BLKFLOAT:
            m_ext1.bfwCompParam.exponent = 0xa;
            break;
        case XRAN_BFWCOMPMETHOD_BLKSCALE:
            m_ext1.bfwCompParam.blockScaler = 0xa5;
            break;
        case XRAN_BFWCOMPMETHOD_ULAW:
            m_ext1.bfwCompParam.compBitWidthShift = 0x55;
        case XRAN_BFWCOMPMETHOD_BEAMSPACE:
            for(i=0; i<N; i++)
                m_ext1.bfwCompParam.activeBeamspaceCoeffMask[i] = 0xa0 + i;
            break;
        }

    for(i=0; i<N*4; i++) {
        m_ext1.p_bfwIQ[i*2]     = 0xcafe;
        m_ext1.p_bfwIQ[i*2+1]   = 0xbeef;
        }

    // extension 2
    m_ext2.bfAzPtWidth        = 7;
    m_ext2.bfAzPt             = 0x55 & m_bitmask[m_ext2.bfAzPtWidth];
    m_ext2.bfZePtWidth        = 7;
    m_ext2.bfZePt             = 0xaa & m_bitmask[m_ext2.bfAzPtWidth];
    m_ext2.bfAz3ddWidth       = 7;
    m_ext2.bfAz3dd            = 0x5a & m_bitmask[m_ext2.bfAzPtWidth];
    m_ext2.bfZe3ddWidth       = 7;
    m_ext2.bfZe3dd            = 0xa5 & m_bitmask[m_ext2.bfAzPtWidth];
    m_ext2.bfAzSI             = 0x2 & m_bitmask[3];
    m_ext2.bfZeSI             = 0x5 & m_bitmask[3];

    // extension 4
    m_ext4.csf                = 1;
    m_ext4.modCompScaler      = 0x5aa5;

    // extension 5
    m_ext5.num_sets = 2;
    for(i=0; i<m_ext5.num_sets; i++) {
        m_ext5.mc[i].csf              = i%2;
        m_ext5.mc[i].mcScaleReMask    = 0xa5a + i;
        m_ext5.mc[i].mcScaleOffset    = 0x5a5a + i;
        }

    numext = 0;

    m_params.sections[sect_num].exData[numext].type = XRAN_CP_SECTIONEXTCMD_1;
    m_params.sections[sect_num].exData[numext].len  = sizeof(m_ext1);
    m_params.sections[sect_num].exData[numext].data = &m_ext1;
    numext++;

    m_params.sections[sect_num].exData[numext].type = XRAN_CP_SECTIONEXTCMD_2;
    m_params.sections[sect_num].exData[numext].len  = sizeof(m_ext2);
    m_params.sections[sect_num].exData[numext].data = &m_ext2;
    numext++;

    m_params.sections[sect_num].exData[numext].type = XRAN_CP_SECTIONEXTCMD_4;
    m_params.sections[sect_num].exData[numext].len  = sizeof(m_ext4);
    m_params.sections[sect_num].exData[numext].data = &m_ext4;
    numext++;

    m_params.sections[sect_num].exData[numext].type = XRAN_CP_SECTIONEXTCMD_5;
    m_params.sections[sect_num].exData[numext].len  = sizeof(m_ext5);
    m_params.sections[sect_num].exData[numext].data = &m_ext5;
    numext++;

    m_params.sections[sect_num].exDataSize = numext;

    return (0);
}

int C_plane::prepare_sections(bool extflag)
{
  int numsec;


    /* Preparing input data for packet generation */
    m_params.dir                  = m_dir;
    m_params.sectionType          = m_sectionType;

    m_params.hdr.filterIdx        = m_filterIndex;
    m_params.hdr.frameId          = m_frameId;
    m_params.hdr.subframeId       = m_subframeId;
    m_params.hdr.slotId           = m_slotId;
    m_params.hdr.startSymId       = m_symStart;
    m_params.hdr.iqWidth          = XRAN_CONVERT_IQWIDTH(m_iqWidth);
    m_params.hdr.compMeth         = m_compMethod;

    switch(m_sectionType) {
        case XRAN_CP_SECTIONTYPE_1:
            break;

        case XRAN_CP_SECTIONTYPE_3:
            m_params.hdr.timeOffset   = m_timeOffset;
            m_params.hdr.fftSize      = m_fftSize;
            m_params.hdr.scs          = m_scs;
            m_params.hdr.cpLength     = m_cpLength;
            break;

        default:
            return (-1);
        }

    for(numsec=0; numsec < m_numSections; numsec++) {
        m_params.sections[numsec].info.type         = m_params.sectionType;       // for database
        m_params.sections[numsec].info.startSymId   = m_params.hdr.startSymId;    // for database
        m_params.sections[numsec].info.iqWidth      = m_params.hdr.iqWidth;       // for database
        m_params.sections[numsec].info.compMeth     = m_params.hdr.compMeth;      // for database
        m_params.sections[numsec].info.id           = m_sectionId++;
        m_params.sections[numsec].info.rb           = XRAN_RBIND_EVERY;
        m_params.sections[numsec].info.symInc       = XRAN_SYMBOLNUMBER_NOTINC;
        m_params.sections[numsec].info.startPrbc    = m_prbStart[numsec];
        m_params.sections[numsec].info.numPrbc      = m_prbNum[numsec];
        m_params.sections[numsec].info.numSymbol    = m_symNum;
        m_params.sections[numsec].info.reMask       = m_reMask;
        m_params.sections[numsec].info.beamId       = m_beamId;
        switch(m_sectionType) {
            case XRAN_CP_SECTIONTYPE_1:
                break;

            case XRAN_CP_SECTIONTYPE_3:
                m_params.sections[numsec].info.freqOffset   = m_freqOffset;
                break;

            default:
                return (-1);
            }

        /* section extension */
        if(/*extflag == true*/0) {
            m_params.sections[numsec].info.ef       = 1;
            prepare_extensions(numsec);
            }
        else {
            m_params.sections[numsec].info.ef       = 0;
            m_params.sections[numsec].exDataSize    = 0;
            }
        }

    m_params.numSections        = numsec;

    return (0);
}


void C_plane::verify_sections(void)
{
  int i,j;

    /* Verify the result */
    EXPECT_TRUE(m_result.dir            == m_params.dir);
    EXPECT_TRUE(m_result.sectionType    == m_params.sectionType);

    EXPECT_TRUE(m_result.hdr.filterIdx  == m_params.hdr.filterIdx);
    EXPECT_TRUE(m_result.hdr.frameId    == m_params.hdr.frameId);
    EXPECT_TRUE(m_result.hdr.subframeId == m_params.hdr.subframeId);
    EXPECT_TRUE(m_result.hdr.slotId     == m_params.hdr.slotId);
    EXPECT_TRUE(m_result.hdr.startSymId == m_params.hdr.startSymId);
    EXPECT_TRUE(m_result.hdr.iqWidth    == m_params.hdr.iqWidth);
    EXPECT_TRUE(m_result.hdr.compMeth   == m_params.hdr.compMeth);

    switch(m_sectionType) {
        case XRAN_CP_SECTIONTYPE_1:
            break;

        case XRAN_CP_SECTIONTYPE_3:
            EXPECT_TRUE(m_result.hdr.fftSize    == m_params.hdr.fftSize);
            EXPECT_TRUE(m_result.hdr.scs        == m_params.hdr.scs);
            EXPECT_TRUE(m_result.hdr.cpLength   == m_params.hdr.cpLength);
            break;

        default:
            FAIL() << "Invalid Section Type - " << m_sectionType << "\n";
        }

    ASSERT_TRUE(m_result.numSections    == m_params.numSections);
    for(i=0; i < m_result.numSections; i++) {
        EXPECT_TRUE(m_result.sections[i].info.id        == m_params.sections[i].info.id);
        EXPECT_TRUE(m_result.sections[i].info.rb        == XRAN_RBIND_EVERY);
        EXPECT_TRUE(m_result.sections[i].info.symInc    == XRAN_SYMBOLNUMBER_NOTINC);
        EXPECT_TRUE(m_result.sections[i].info.startPrbc == m_params.sections[i].info.startPrbc);
        EXPECT_TRUE(m_result.sections[i].info.numPrbc   == m_params.sections[i].info.numPrbc);
        EXPECT_TRUE(m_result.sections[i].info.numSymbol == m_params.sections[i].info.numSymbol);
        EXPECT_TRUE(m_result.sections[i].info.reMask    == m_params.sections[i].info.reMask);
        EXPECT_TRUE(m_result.sections[i].info.beamId    == m_params.sections[i].info.beamId);
        EXPECT_TRUE(m_result.sections[i].info.ef        == m_params.sections[i].info.ef);

        switch(m_sectionType) {
            case XRAN_CP_SECTIONTYPE_1:
                break;

            case XRAN_CP_SECTIONTYPE_3:
                EXPECT_TRUE(m_result.sections[i].info.freqOffset  == m_params.sections[i].info.freqOffset);
                break;

            default:
                FAIL() << "Invalid Section Type - " << m_sectionType << "\n";
            }

        if(m_params.sections[i].info.ef) {
     //       printf("[%d] %d ==  %d\n",i,  m_result.sections[i].exDataSize, m_params.sections[i].exDataSize);
            EXPECT_TRUE(m_result.sections[i].exDataSize == m_params.sections[i].exDataSize);

            for(j=0; j < m_params.sections[i].exDataSize; j++) {
                EXPECT_TRUE(m_result.sections[i].exData[j].type == m_params.sections[i].exData[j].type);

                switch(m_params.sections[i].exData[j].type) {
                    case XRAN_CP_SECTIONEXTCMD_1:
                        {
                        struct xran_sectionext1_info *ext1_params, *ext1_result;
                        int iq_size, parm_size, N;

                        ext1_params = (struct xran_sectionext1_info *)m_params.sections[i].exData[j].data;
                        ext1_result = (struct xran_sectionext1_info *)m_result.sections[i].exData[j].data;

                        EXPECT_TRUE(ext1_result->bfwiqWidth == ext1_params->bfwiqWidth);
                        EXPECT_TRUE(ext1_result->bfwCompMeth    == ext1_params->bfwCompMeth);

                        N = ext1_params->bfwNumber;
                        switch(ext1_params->bfwCompMeth) {
                            case XRAN_BFWCOMPMETHOD_BLKFLOAT:
                                EXPECT_TRUE(ext1_result->bfwCompParam.exponent == ext1_params->bfwCompParam.exponent);
                                break;

                            case XRAN_BFWCOMPMETHOD_BLKSCALE:
                                EXPECT_TRUE(ext1_result->bfwCompParam.blockScaler == ext1_params->bfwCompParam.blockScaler);
                                break;

                            case XRAN_BFWCOMPMETHOD_ULAW:
                                EXPECT_TRUE(ext1_result->bfwCompParam.compBitWidthShift == ext1_params->bfwCompParam.compBitWidthShift);
                                break;

                            case XRAN_BFWCOMPMETHOD_BEAMSPACE:
                                parm_size = N>>3; if(N%8) parm_size++; parm_size *= 8;
                                EXPECT_TRUE(std::memcmp(ext1_result->bfwCompParam.activeBeamspaceCoeffMask, ext1_params->bfwCompParam.activeBeamspaceCoeffMask, parm_size));
                                break;
                            }

                        /* Get the number of BF weights */
                        iq_size = N*ext1_params->bfwiqWidth*2;  // total in bits
                        parm_size = iq_size>>3;                 // total in bytes (/8)
                        if(iq_size%8) parm_size++;              // round up
                        EXPECT_TRUE(std::memcmp(ext1_result->p_bfwIQ, ext1_params->p_bfwIQ, parm_size));

                        }
                        break;

                    case XRAN_CP_SECTIONEXTCMD_2:
                        {
                        struct xran_sectionext2_info *ext2_params, *ext2_result;

                        ext2_params = (struct xran_sectionext2_info *)m_params.sections[i].exData[j].data;
                        ext2_result = (struct xran_sectionext2_info *)m_result.sections[i].exData[j].data;

                        if(ext2_params->bfAzPtWidth) {
                            EXPECT_TRUE(ext2_result->bfAzPtWidth    == ext2_params->bfAzPtWidth);
                            EXPECT_TRUE(ext2_result->bfAzPt         == ext2_params->bfAzPt);
                            }

                        if(ext2_params->bfZePtWidth) {
                            EXPECT_TRUE(ext2_result->bfZePtWidth    == ext2_params->bfZePtWidth);
                            EXPECT_TRUE(ext2_result->bfZePt         == ext2_params->bfZePt);
                            }
                        if(ext2_params->bfAz3ddWidth) {
                            EXPECT_TRUE(ext2_result->bfAz3ddWidth   == ext2_params->bfAz3ddWidth);
                            EXPECT_TRUE(ext2_result->bfAz3dd        == ext2_params->bfAz3dd);
                            }
                        if(ext2_params->bfZe3ddWidth) {
                            EXPECT_TRUE(ext2_result->bfZe3ddWidth   == ext2_params->bfZe3ddWidth);
                            EXPECT_TRUE(ext2_result->bfZe3dd        == ext2_params->bfZe3dd);
                            }

                        EXPECT_TRUE(ext2_result->bfAzSI == ext2_params->bfAzSI);
                        EXPECT_TRUE(ext2_result->bfZeSI == ext2_params->bfZeSI);
                        }
                        break;

                    case XRAN_CP_SECTIONEXTCMD_4:
                        {
                        struct xran_sectionext4_info *ext4_params, *ext4_result;

                        ext4_params = (struct xran_sectionext4_info *)m_params.sections[i].exData[j].data;
                        ext4_result = (struct xran_sectionext4_info *)m_result.sections[i].exData[j].data;

                        EXPECT_TRUE(ext4_result->csf            == ext4_params->csf);
                        EXPECT_TRUE(ext4_result->modCompScaler  == ext4_params->modCompScaler);
                        }
                        break;
                    case XRAN_CP_SECTIONEXTCMD_5:
                        {
                        struct xran_sectionext5_info *ext5_params, *ext5_result;
                        int idx;

                        ext5_params = (struct xran_sectionext5_info *)m_params.sections[i].exData[j].data;
                        ext5_result = (struct xran_sectionext5_info *)m_result.sections[i].exData[j].data;

                        EXPECT_TRUE(ext5_result->num_sets == ext5_params->num_sets);
                        for(idx=0; idx < ext5_params->num_sets; idx++) {
                            EXPECT_TRUE(ext5_result->mc[idx].csf == ext5_params->mc[idx].csf);
                            EXPECT_TRUE(ext5_result->mc[idx].mcScaleReMask == ext5_params->mc[idx].mcScaleReMask);
                            EXPECT_TRUE(ext5_result->mc[idx].mcScaleOffset == ext5_params->mc[idx].mcScaleOffset);
                            }
                        }
                        break;
                    }
                }
            }
        }

    return;
}


/***************************************************************************
 * Functional Test cases
 ***************************************************************************/

TEST_P(C_plane, Section_Ext1)
{
    int i = 0, idRb;
    int32_t len = 0;
    int16_t *ptr = NULL;
    int32_t nRbs = 36;
    int32_t nAntElm = 32;
    int8_t  iqWidth = 16;
    int8_t  compMethod = XRAN_COMPMETHOD_NONE;
    int8_t  *p_ext1_dst  = NULL;
    int16_t *bfw_payload = NULL;
    int32_t expected_len = (3+1)*nRbs + nAntElm*nRbs*4;

    struct xran_section_gen_info* loc_pSectGenInfo = m_params.sections;
    struct xran_sectionext1_info m_ext1;
    struct xran_cp_radioapp_section_ext1 *p_ext1;

    /* Configure section information */
    if(prepare_sections(false) < 0) {
        FAIL() << "Invalid Section configuration\n";
    }
    ptr = m_p_bfw_iq_src;

    for (idRb =0; idRb < nRbs*nAntElm*2; idRb++){
        ptr[idRb] = i;
        i++;
    }

    len = xran_cp_populate_section_ext_1(m_p_ext1_dst,
                                         m_ext1_dst_len,
                                         m_p_bfw_iq_src,
                                         nRbs,
                                         nAntElm,
                                         iqWidth,
                                         compMethod);

    ASSERT_TRUE(len == expected_len);

    p_ext1_dst = m_p_ext1_dst;
    idRb = 0;
    do {
        p_ext1 = (struct xran_cp_radioapp_section_ext1 *)p_ext1_dst;
        bfw_payload = (int16_t*)(p_ext1+1);
        p_ext1_dst += p_ext1->extLen*XRAN_SECTIONEXT_ALIGN;
        idRb++;
    }while(p_ext1->ef != XRAN_EF_F_LAST);

    ASSERT_TRUE(idRb == nRbs);

    /* Update section information */
    memset(&m_ext1, 0, sizeof (struct xran_sectionext1_info));
    m_ext1.bfwNumber      = nAntElm;
    m_ext1.bfwiqWidth     = iqWidth;
    m_ext1.bfwCompMeth    = compMethod;
    m_ext1.p_bfwIQ        = (int16_t*)m_p_ext1_dst;
    m_ext1.bfwIQ_sz       = len;

    loc_pSectGenInfo->exData[0].type = XRAN_CP_SECTIONEXTCMD_1;
    loc_pSectGenInfo->exData[0].len  = sizeof(m_ext1);
    loc_pSectGenInfo->exData[0].data = &m_ext1;

    loc_pSectGenInfo->info.ef       = 1;
    loc_pSectGenInfo->exDataSize    = 1;

    m_params.numSections    = 1;

    /* Generating C-Plane packet */
    ASSERT_TRUE(xran_prepare_ctrl_pkt(m_pTestBuffer, &m_params, m_ccId, m_antId, m_seqId) == XRAN_STATUS_SUCCESS);

    /* Parsing generated packet */
    EXPECT_TRUE(xran_parse_cp_pkt(m_pTestBuffer, &m_result, &m_pktInfo) == XRAN_STATUS_SUCCESS);

    /* Verify the result */
    //verify_sections();
}

TEST_P(C_plane, Section_Ext1_9bit)
{
    int i = 0, idRb;
    int32_t len = 0;
    int16_t *ptr = NULL;
    int32_t nRbs = 36;
    int32_t nAntElm = 32;
    int8_t  iqWidth = 9;
    int8_t  compMethod = XRAN_COMPMETHOD_BLKFLOAT;
    int8_t  *p_ext1_dst  = NULL;
    int16_t *bfw_payload = NULL;
    int32_t expected_len = ((nAntElm/16*4*iqWidth)+1)*nRbs + /* bfwCompParam + IQ = */
                            sizeof(struct xran_cp_radioapp_section_ext1)*nRbs; /* ext1 Headers */

    struct xran_section_gen_info* loc_pSectGenInfo = m_params.sections;
    struct xran_sectionext1_info m_ext1;
    struct xran_cp_radioapp_section_ext1 *p_ext1;

    /* Configure section information */
    if(prepare_sections(false) < 0) {
        FAIL() << "Invalid Section configuration\n";
    }
    ptr = m_p_bfw_iq_src;

    for (idRb =0; idRb < nRbs*nAntElm*2; idRb++){
        ptr[idRb] = i;
        i++;
    }

    len = xran_cp_populate_section_ext_1(m_p_ext1_dst,
                                         m_ext1_dst_len,
                                         m_p_bfw_iq_src,
                                         nRbs,
                                         nAntElm,
                                         iqWidth,
                                         compMethod);

    ASSERT_TRUE(len == expected_len);

    p_ext1_dst = m_p_ext1_dst;
    idRb = 0;
    do {
        p_ext1 = (struct xran_cp_radioapp_section_ext1 *)p_ext1_dst;
        bfw_payload = (int16_t*)(p_ext1+1);
        p_ext1_dst += p_ext1->extLen*XRAN_SECTIONEXT_ALIGN;
        idRb++;
    }while(p_ext1->ef != XRAN_EF_F_LAST);

    ASSERT_TRUE(idRb == nRbs);

    /* Update section information */
    memset(&m_ext1, 0, sizeof (struct xran_sectionext1_info));
    m_ext1.bfwNumber      = nAntElm;
    m_ext1.bfwiqWidth     = iqWidth;
    m_ext1.bfwCompMeth    = compMethod;
    m_ext1.p_bfwIQ        = (int16_t*)m_p_ext1_dst;
    m_ext1.bfwIQ_sz       = len;

    loc_pSectGenInfo->exData[0].type = XRAN_CP_SECTIONEXTCMD_1;
    loc_pSectGenInfo->exData[0].len  = sizeof(m_ext1);
    loc_pSectGenInfo->exData[0].data = &m_ext1;

    loc_pSectGenInfo->info.ef       = 1;
    loc_pSectGenInfo->exDataSize    = 1;

    m_params.numSections    = 1;

    /* Generating C-Plane packet */
    ASSERT_TRUE(xran_prepare_ctrl_pkt(m_pTestBuffer, &m_params, m_ccId, m_antId, m_seqId) == XRAN_STATUS_SUCCESS);

    /* Parsing generated packet */
    EXPECT_TRUE(xran_parse_cp_pkt(m_pTestBuffer, &m_result, &m_pktInfo) == XRAN_STATUS_SUCCESS);

    /* Verify the result */
    //verify_sections();
}



TEST_P(C_plane, PacketGen)
{
  int i;


    /* Configure section information */
    if(prepare_sections(false) < 0) {
        FAIL() << "Invalid Section configuration\n";
        }

    /* Generating C-Plane packet */
    ASSERT_TRUE(xran_prepare_ctrl_pkt(m_pTestBuffer, &m_params, m_ccId, m_antId, m_seqId) == XRAN_STATUS_SUCCESS);

    /* Parsing generated packet */
    EXPECT_TRUE(xran_parse_cp_pkt(m_pTestBuffer, &m_result, &m_pktInfo) == XRAN_STATUS_SUCCESS);

    /* Verify the result */
    verify_sections();
}


TEST_P(C_plane, PacketGen_Ext)
{
  int i;


    /* Configure section information */
    if(prepare_sections(true) < 0) {
        FAIL() << "Invalid Section configuration\n";
        }

    /* Generating C-Plane packet */
    ASSERT_TRUE(xran_prepare_ctrl_pkt(m_pTestBuffer, &m_params, m_ccId, m_antId, m_seqId) == XRAN_STATUS_SUCCESS);

    /* Parsing generated packet */
    EXPECT_TRUE(xran_parse_cp_pkt(m_pTestBuffer, &m_result, &m_pktInfo) == XRAN_STATUS_SUCCESS);

    /* Verify the result */
    verify_sections();
}


/***************************************************************************
 * Performance Test cases
 ***************************************************************************/
TEST_P(C_plane, Perf)
{
    /* Configure section information */
    if(prepare_sections(false) < 0) {
        FAIL() << "Invalid Section configuration\n";
        }

    /* using wrapper function to reset mbuf */
    performance("C", module_name,
            &xran_ut_prepare_cp, m_pTestBuffer, &m_params, m_ccId, m_antId, m_seqId);
}

TEST_P(C_plane, Perf_Ext)
{
    /* Configure section information */
    if(prepare_sections(true) < 0) {
        FAIL() << "Invalid Section configuration\n";
        }

    /* using wrapper function to reset mbuf */
    performance("C", module_name,
            &xran_ut_prepare_cp, m_pTestBuffer, &m_params, m_ccId, m_antId, m_seqId);
}


INSTANTIATE_TEST_CASE_P(UnitTest, C_plane,
        testing::ValuesIn(get_sequence(C_plane::get_number_of_cases("C_Plane"))));

