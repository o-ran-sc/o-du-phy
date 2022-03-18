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
#include "xran_lib_wrap.hpp"
#include "xran_common.h"
#include "xran_fh_o_du.h"
#include "ethdi.h"
#include "ethernet.h"
#include "xran_transport.h"
#include "xran_cp_api.h"

#include <stdint.h>

#define DELETE_ARRAY(x)         { if(x) { delete[] x; x = nullptr; } }

#define XRAN_MAX_BUFLEN_EXT11   (MAX_RX_LEN -                                           \
                                    (RTE_PKTMBUF_HEADROOM                               \
                                     + sizeof(struct xran_ecpri_hdr)                    \
                                     + sizeof(struct xran_cp_radioapp_common_header)    \
                                     + sizeof(struct xran_cp_radioapp_section1)         \
                                     + sizeof(union xran_cp_radioapp_section_ext6)     \
                                     + sizeof(union xran_cp_radioapp_section_ext10)    \
                                     ))

const std::string module_name = "C-Plane";

const uint8_t m_bitmask[] = { 0x00, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff };

extern "C"
{

/* wrapper function for performace tests to reset mbuf */
int xran_ut_prepare_cp(struct xran_cp_gen_params *params,
                        uint8_t cc_id, uint8_t ant_id, uint8_t seq_id)
{
    register int ret;
    register struct rte_mbuf *mbuf;

    mbuf = xran_ethdi_mbuf_alloc();
    if(mbuf == NULL) {
        printf("Failed to allocate buffer!\n");
        return (-1);
        }

    ret = xran_prepare_ctrl_pkt(mbuf, params, cc_id, ant_id, seq_id);
    rte_pktmbuf_free(mbuf);

    return (ret);
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
    struct xran_section_recv_info *m_pSectResult = NULL;

    struct sectinfo {
        uint16_t    sectionId;
        uint8_t     rb;
        uint8_t     symInc;
        uint16_t    startPrbc;
        uint16_t    numPrbc;
        uint16_t    reMask;
        uint8_t     numSymbol;
        uint16_t    beamId;
        int         freqOffset;
        std::vector<uint8_t> exts;
        };

    struct extcfginfo {
        int         type;
        std::string name;
        union {
            struct xran_sectionext1_info ext1;
            struct xran_sectionext2_info ext2;
            struct xran_sectionext3_info ext3;
            struct xran_sectionext4_info ext4;
            struct xran_sectionext5_info ext5;
            struct xran_sectionext6_info ext6;
            struct xran_sectionext7_info ext7;
            struct xran_sectionext8_info ext8;
            struct xran_sectionext9_info ext9;
            struct xran_sectionext10_info ext10;
            struct xran_sectionext11_info ext11;
            } u;
        };

protected:
    int m_maxSections = 8;  /*  not used */
    int m_numSections;

    struct rte_mbuf *m_pTestBuffer = nullptr;

    struct xran_cp_gen_params m_params;
    struct xran_recv_packet_info m_pktInfo;
    struct xran_cp_recv_params m_result;

    struct xran_sectionext1_info m_temp_ext1[XRAN_MAX_PRBS];

    uint8_t     m_dir;
    std::string m_dirStr;
    uint8_t     m_sectionType;

    uint8_t     m_ccId, m_antId;
    uint8_t     m_seqId;
    uint8_t     m_frameId, m_subframeId, m_slotId;
    uint8_t     m_symStart;

    uint8_t     m_iqWidth, m_compMethod;
    uint8_t     m_filterIndex;
    uint16_t    m_timeOffset;
    uint8_t     m_fftSize;
    uint8_t     m_scs;
    uint16_t    m_cpLength;

    struct sectinfo     *m_sections;
    struct extcfginfo   *m_extcfgs;
    int                 m_nextcfgs;

    uint16_t  m_ext1_dst_len = 0;
    int8_t   *m_p_ext1_dst   = nullptr;
    int16_t  *m_p_bfw_iq_src = nullptr;

    struct xran_sectionext1_info m_ext1;

    int         m_antElmTRx;
    struct rte_mbuf_ext_shared_info m_extSharedInfo;
    uint8_t     *m_pBfwIQ_ext = nullptr;
    int16_t     *m_pBfw_src[XRAN_MAX_SET_BFWS];
    int16_t     m_pBfw_rx[XRAN_MAX_SET_BFWS][MAX_RX_LEN];
    struct xran_ext11_bfw_info m_bfwInfo[XRAN_MAX_SET_BFWS];

    void SetUp() override
    {
        int i, j;
        bool flag_skip;
        int ext_type;
        std::string ext_name;

        init_test("C_Plane");

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
                break;

            default:
                FAIL() << "Invalid Section Type - " << m_sectionType << std::endl;
            }

        m_numSections   = get_input_subsection_size("sections");
        ASSERT_FALSE(m_numSections == 0);

        m_sections = new struct sectinfo [m_numSections];
        for(i=0; i<m_numSections; i++) {
            m_sections[i].sectionId = get_input_parameter<uint16_t>("sections", i, "sectionId");
            m_sections[i].rb        = get_input_parameter<uint16_t>("sections", i, "rb");
            m_sections[i].symInc    = get_input_parameter<uint16_t>("sections", i, "symInc");
            m_sections[i].startPrbc = get_input_parameter<uint16_t>("sections", i, "startPrbc");
            m_sections[i].numPrbc   = get_input_parameter<uint16_t>("sections", i, "numPrbc");
            m_sections[i].reMask    = get_input_parameter<uint16_t>("sections", i, "reMask");
            m_sections[i].numSymbol = get_input_parameter<uint16_t>("sections", i, "numSymbol");
            m_sections[i].beamId    = get_input_parameter<uint16_t>("sections", i, "beamId");

            switch(m_sectionType) {
                case XRAN_CP_SECTIONTYPE_3:
                    m_sections[i].freqOffset    = get_input_parameter<uint16_t>("sections", i, "freqOffset");
                    break;
                }

            m_sections[i].exts      = get_input_parameter<std::vector<uint8_t>>("sections", i, "exts");
            }

        /* allocate and prepare required data storage */
        m_pSectGenInfo      = new struct xran_section_gen_info [m_numSections];
        ASSERT_NE(m_pSectGenInfo, nullptr);
        m_params.sections   = m_pSectGenInfo;

        m_pSectResult       = new struct xran_section_recv_info [m_numSections];
        ASSERT_NE(m_pSectResult, nullptr);
        m_result.sections   = m_pSectResult;

        /* reading configurations of section extension */
        m_nextcfgs = get_input_subsection_size("extensions");
        if(m_nextcfgs) {
            m_extcfgs = new struct extcfginfo [m_nextcfgs];

            flag_skip = false;
            for(i=0; i < m_nextcfgs; i++) {
                std::vector<uint16_t> beamIDs;

                ext_type    = get_input_parameter<int>("extensions", i, "type");
                switch(ext_type) {
                    case XRAN_CP_SECTIONEXTCMD_1:
                        /* if section extension type 1 is present, then ignore other extensions */
                        if(i != 0 && m_nextcfgs != 1) {
                            std::cout << "### Extension 1 configuration, ignore other extensions !!\n" << std::endl;
                            }
                        flag_skip = true;
                        m_nextcfgs = 1;
                        i = 0;
                        m_extcfgs[i].u.ext1.bfwCompMeth = get_input_parameter<uint8_t> ("extensions", i, "bfwCompMeth");
                        m_extcfgs[i].u.ext1.bfwIqWidth  = get_input_parameter<uint8_t> ("extensions", i, "bfwIqWidth");
                        m_antElmTRx                     = get_input_parameter<uint8_t> ("extensions", i, "antelm_trx");
                        break;

                    case XRAN_CP_SECTIONEXTCMD_2:
                        m_extcfgs[i].u.ext2.bfAzPtWidth  = get_input_parameter<uint8_t>("extensions", i, "bfAzPtWidth") & 0x7;
                        m_extcfgs[i].u.ext2.bfAzPt       = get_input_parameter<uint8_t>("extensions", i, "bfAzPt") & 0xf;
                        m_extcfgs[i].u.ext2.bfZePtWidth  = get_input_parameter<uint8_t>("extensions", i, "bfZePtWidth") & 0x7;
                        m_extcfgs[i].u.ext2.bfZePt       = get_input_parameter<uint8_t>("extensions", i, "bfZePt") & 0xf;
                        m_extcfgs[i].u.ext2.bfAz3ddWidth = get_input_parameter<uint8_t>("extensions", i, "bfAz3ddWidth") & 0x7;
                        m_extcfgs[i].u.ext2.bfAz3dd      = get_input_parameter<uint8_t>("extensions", i, "bfAz3dd") & 0xf;
                        m_extcfgs[i].u.ext2.bfZe3ddWidth = get_input_parameter<uint8_t>("extensions", i, "bfZe3ddWidth") & 0x7;
                        m_extcfgs[i].u.ext2.bfZe3dd      = get_input_parameter<uint8_t>("extensions", i, "bfZe3dd") & 0xf;
                        m_extcfgs[i].u.ext2.bfAzSI       = get_input_parameter<uint8_t>("extensions", i, "bfAzSI") & 0x7;
                        m_extcfgs[i].u.ext2.bfZeSI       = get_input_parameter<uint8_t>("extensions", i, "bfZeSI") & 0x7;
                        break;

                    case XRAN_CP_SECTIONEXTCMD_3:
                        m_extcfgs[i].u.ext3.codebookIdx  = get_input_parameter<uint8_t> ("extensions", i, "codebookIdx");
                        m_extcfgs[i].u.ext3.layerId      = get_input_parameter<uint8_t> ("extensions", i, "layerId") & 0xf;
                        m_extcfgs[i].u.ext3.numLayers    = get_input_parameter<uint8_t> ("extensions", i, "numLayers") & 0xf;
                        m_extcfgs[i].u.ext3.txScheme     = get_input_parameter<uint8_t> ("extensions", i, "txScheme") & 0xf;
                        m_extcfgs[i].u.ext3.crsReMask    = get_input_parameter<uint16_t>("extensions", i, "crsReMask") & 0xfff;
                        m_extcfgs[i].u.ext3.crsShift     = get_input_parameter<uint8_t> ("extensions", i, "crsShift") & 0x1;
                        m_extcfgs[i].u.ext3.crsSymNum    = get_input_parameter<uint8_t> ("extensions", i, "crsSymNum") & 0xf;
                        m_extcfgs[i].u.ext3.numAntPort   = get_input_parameter<uint16_t>("extensions", i, "numAntPort");
                        m_extcfgs[i].u.ext3.beamIdAP1    = get_input_parameter<uint16_t>("extensions", i, "beamIdAP1");
                        m_extcfgs[i].u.ext3.beamIdAP2    = get_input_parameter<uint16_t>("extensions", i, "beamIdAP2");
                        m_extcfgs[i].u.ext3.beamIdAP3    = get_input_parameter<uint16_t>("extensions", i, "beamIdAP3");
                        break;

                    case XRAN_CP_SECTIONEXTCMD_4:
                        m_extcfgs[i].u.ext4.csf          = get_input_parameter<uint8_t> ("extensions", i, "csf") & 0xf;
                        m_extcfgs[i].u.ext4.modCompScaler= get_input_parameter<uint16_t>("extensions", i, "modCompScaler") & 0x7fff;
                        break;

                    case XRAN_CP_SECTIONEXTCMD_5:
                        {
                        std::vector<uint16_t> csf;
                        std::vector<uint16_t> mcScaleReMask;
                        std::vector<uint16_t> mcScaleOffset;

                        m_extcfgs[i].u.ext5.num_sets     = get_input_parameter<uint8_t>("extensions", i, "num_sets");
                        if(m_extcfgs[i].u.ext5.num_sets > XRAN_MAX_MODCOMP_ADDPARMS)
                            FAIL() << "Invalid number of sets in extension 5!";

                        csf = get_input_parameter<std::vector<uint16_t>>("extensions", i, "csf");
                        mcScaleReMask = get_input_parameter<std::vector<uint16_t>>("extensions", i, "mcScaleReMask");
                        mcScaleOffset = get_input_parameter<std::vector<uint16_t>>("extensions", i, "mcScaleOffset");

                        if(csf.size() != m_extcfgs[i].u.ext5.num_sets
                                || mcScaleReMask.size() != m_extcfgs[i].u.ext5.num_sets
                                || mcScaleOffset.size() != m_extcfgs[i].u.ext5.num_sets)
                            FAIL() << "Invalid configuration in extension 5 - different size!";

                        for(j=0; j < m_extcfgs[i].u.ext5.num_sets; j++) {
                            m_extcfgs[i].u.ext5.mc[j].csf           = csf[j];
                            m_extcfgs[i].u.ext5.mc[j].mcScaleReMask = mcScaleReMask[j];
                            m_extcfgs[i].u.ext5.mc[j].mcScaleOffset = mcScaleOffset[j];
                            }
                        }
                        break;

                    case XRAN_CP_SECTIONEXTCMD_6:
                        m_extcfgs[i].u.ext6.rbgSize     = get_input_parameter<uint8_t> ("extensions", i, "rbgSize");
                        m_extcfgs[i].u.ext6.rbgMask     = get_input_parameter<uint32_t>("extensions", i, "rbgMask");
                        m_extcfgs[i].u.ext6.symbolMask  = get_input_parameter<uint16_t>("extensions", i, "symbolMask");
                        break;

                    case XRAN_CP_SECTIONEXTCMD_10:
                        m_extcfgs[i].u.ext10.numPortc   = get_input_parameter<uint8_t> ("extensions", i, "numPortc");
                        m_extcfgs[i].u.ext10.beamGrpType= get_input_parameter<uint8_t> ("extensions", i, "beamGrpType");
                        switch(m_extcfgs[i].u.ext10.beamGrpType) {
                            case XRAN_BEAMGT_COMMON:
                            case XRAN_BEAMGT_MATRIXIND:
                                break;
                            case XRAN_BEAMGT_VECTORLIST:
                                beamIDs = get_input_parameter<std::vector<uint16_t>>("extensions", i, "beamID");
                                for(j=0; j < m_extcfgs[i].u.ext10.numPortc; j++)
                                    m_extcfgs[i].u.ext10.beamID[j] = beamIDs[j];
                                break;
                            default:
                                FAIL() << "Invalid Beam Group Type - " << m_extcfgs[i].u.ext10.beamGrpType << std::endl;
                            }
                        break;

                    case XRAN_CP_SECTIONEXTCMD_11:
                        {
                        int temp;
                        /* if section extension type 11 is present, then ignore other extensions */
                        if(i != 0 && m_nextcfgs != 1) {
                            std::cout << "### Extension 11 configuration, ignore other extensions !!\n" << std::endl;
                            }
                        flag_skip = true;
                        m_nextcfgs = 1;
                        i = 0;

                        m_extcfgs[i].u.ext11.RAD            = get_input_parameter<uint8_t> ("extensions", i, "RAD");
                        m_extcfgs[i].u.ext11.disableBFWs    = get_input_parameter<uint8_t> ("extensions", i, "disableBFWs");
                        m_extcfgs[i].u.ext11.numBundPrb     = get_input_parameter<uint8_t> ("extensions", i, "numBundPrb");
                        m_extcfgs[i].u.ext11.bfwCompMeth    = get_input_parameter<uint8_t> ("extensions", i, "bfwCompMeth");
                        m_extcfgs[i].u.ext11.bfwIqWidth     = get_input_parameter<uint8_t> ("extensions", i, "bfwIqWidth");
                        m_extcfgs[i].u.ext11.numSetBFWs     = get_input_parameter<uint8_t> ("extensions", i, "numSetBFWs");
                        m_antElmTRx                         = get_input_parameter<uint8_t> ("extensions", i, "antelm_trx");
                        beamIDs = get_input_parameter<std::vector<uint16_t>>("extensions", i, "beamID");

                        /* Allocate buffers */
                        m_extcfgs[i].u.ext11.maxExtBufSize  = MAX_RX_LEN;
                        m_pBfwIQ_ext = (uint8_t *)xran_malloc(m_extcfgs[i].u.ext11.maxExtBufSize);
                        m_extcfgs[i].u.ext11.pExtBuf = m_pBfwIQ_ext;

                        for(j = 0; j < XRAN_MAX_SET_BFWS; j++) {
                            m_pBfw_src[j]    = new int16_t [XRAN_MAX_BFW_N];
                            memset(m_pBfw_src[j], j+1, XRAN_MAX_BFW_N);
                            }

                        for(j=0; j < m_extcfgs[i].u.ext11.numSetBFWs; j++) {
                            m_bfwInfo[j].pBFWs  = (uint8_t *)(m_pBfw_src[j]);
                            m_bfwInfo[j].beamId = beamIDs[j];
                            }

                        /* Initialize Shared information for external buffer */
                        m_extSharedInfo.free_cb     = NULL;
                        m_extSharedInfo.fcb_opaque  = NULL;
                        rte_mbuf_ext_refcnt_update(&m_extSharedInfo, 0);
                        m_extcfgs[i].u.ext11.pExtBufShinfo  = &m_extSharedInfo;

                        /* Check all BFWs can be fit with given buffer */
                        temp = xran_cp_estimate_max_set_bfws(m_antElmTRx, m_extcfgs[i].u.ext11.bfwIqWidth,
                                            m_extcfgs[i].u.ext11.bfwCompMeth, m_extcfgs[i].u.ext11.maxExtBufSize);
                        if(m_extcfgs[i].u.ext11.numSetBFWs > temp) {
                            FAIL() << "Too many sets of BFWs - " << m_extcfgs[i].u.ext11.numSetBFWs
                                    << "  (max " << temp << " for " << m_extcfgs[i].u.ext11.maxExtBufSize << std::endl;
                            }

                        temp = xran_cp_prepare_ext11_bfws(m_extcfgs[i].u.ext11.numSetBFWs, m_antElmTRx,
                                            m_extcfgs[i].u.ext11.bfwIqWidth, m_extcfgs[i].u.ext11.bfwCompMeth,
                                            m_pBfwIQ_ext, XRAN_MAX_BUFLEN_EXT11, m_bfwInfo);
                        if(temp < 0) {
                            FAIL() << "Fail to prepare BFWs!" << std::endl;
                            };
                        m_extcfgs[i].u.ext11.totalBfwIQLen = temp;
                        }
                        break;

                    default:
                       FAIL() << "Invalid Section Type Extension - " << ext_type << std::endl;
                       continue;
                    } /* switch(ext_type) */

                m_extcfgs[i].type   = ext_type;
                m_extcfgs[i].name   = get_input_parameter<std::string>("extensions", i, "name");

                if(flag_skip)
                    break;
                } /* for(i=0; i < m_nextcfgs; i++) */
            }
        else {
            m_extcfgs = nullptr;
            }

        m_ext1_dst_len      = 9600;
        m_p_ext1_dst        = new int8_t [m_ext1_dst_len];
        m_p_bfw_iq_src      = new int16_t [9600/2];
    }

    void TearDown() override
    {
        if(m_pTestBuffer != nullptr) {
            rte_pktmbuf_free(m_pTestBuffer);
            m_pTestBuffer = nullptr;
            }

        if(m_pBfwIQ_ext){
            xran_free(m_pBfwIQ_ext);
            m_pBfwIQ_ext = nullptr;
            }
        for(int i = 0; i < XRAN_MAX_SET_BFWS; i++) {
            DELETE_ARRAY(m_pBfw_src[i]);
            }

        DELETE_ARRAY(m_extcfgs);
        DELETE_ARRAY(m_sections);
        DELETE_ARRAY(m_p_bfw_iq_src);
        DELETE_ARRAY(m_p_ext1_dst);
        DELETE_ARRAY(m_pSectGenInfo);
        DELETE_ARRAY(m_pSectResult);
    }

    int prepare_sections(void);
    int prepare_extensions(void);
    void verify_sections(void);

    void test_ext1(void);
};



int C_plane::prepare_extensions()
{
    int i, j, numext, sect_num;
    int ext_id;

    for(sect_num=0; sect_num < m_numSections; sect_num++) {
        numext = 0;

        for(i=0; i < m_sections[sect_num].exts.size(); i++) {

            ext_id = m_sections[sect_num].exts[i];
            if(ext_id >= m_nextcfgs) {
                std::cout << "Invalid section extension configuration index - " << ext_id << " [max " << m_nextcfgs-1 << "]" << std::endl;
                return (-1);
                }

            switch(m_extcfgs[ext_id].type) {
                case XRAN_CP_SECTIONEXTCMD_1:
//                    std::cout << "Skip Extension 1 !!" << std::endl;
                    continue;
                case XRAN_CP_SECTIONEXTCMD_2:
                    m_params.sections[sect_num].exData[numext].len  = sizeof(m_extcfgs[ext_id].u.ext2);
                    m_params.sections[sect_num].exData[numext].data = &m_extcfgs[ext_id].u.ext2;
                    break;
                case XRAN_CP_SECTIONEXTCMD_3:
                    m_params.sections[sect_num].exData[numext].len  = sizeof(m_extcfgs[ext_id].u.ext3);
                    m_params.sections[sect_num].exData[numext].data = &m_extcfgs[ext_id].u.ext3;
                    break;
                case XRAN_CP_SECTIONEXTCMD_4:
                    m_params.sections[sect_num].exData[numext].len  = sizeof(m_extcfgs[ext_id].u.ext4);
                    m_params.sections[sect_num].exData[numext].data = &m_extcfgs[ext_id].u.ext4;
                    break;
                case XRAN_CP_SECTIONEXTCMD_5:
                    m_params.sections[sect_num].exData[numext].len  = sizeof(m_extcfgs[ext_id].u.ext5);
                    m_params.sections[sect_num].exData[numext].data = &m_extcfgs[ext_id].u.ext5;
                    break;
                case XRAN_CP_SECTIONEXTCMD_6:
                    m_params.sections[sect_num].exData[numext].len  = sizeof(m_extcfgs[ext_id].u.ext6);
                    m_params.sections[sect_num].exData[numext].data = &m_extcfgs[ext_id].u.ext6;
                    break;
                case XRAN_CP_SECTIONEXTCMD_10:
                    m_params.sections[sect_num].exData[numext].len  = sizeof(m_extcfgs[ext_id].u.ext10);
                    m_params.sections[sect_num].exData[numext].data = &m_extcfgs[ext_id].u.ext10;
                    break;
                case XRAN_CP_SECTIONEXTCMD_11:
                    m_params.sections[sect_num].exData[numext].len  = sizeof(m_extcfgs[ext_id].u.ext11);
                    m_params.sections[sect_num].exData[numext].data = &m_extcfgs[ext_id].u.ext11;

                    m_result.sections[sect_num].exts[numext].type   = m_extcfgs[ext_id].type;
                    for(j=0 ; j < XRAN_MAX_SET_BFWS; j++)
                        m_result.sections[sect_num].exts[numext].u.ext11.bundInfo[j].pBFWs = (uint8_t *)m_pBfw_rx[j];

                    break;
                default:
                    std::cout << "Invalid Section Extension Type - " << (int)m_extcfgs[ext_id].type << std::endl;
                    return (-1);
                } /* switch(m_extcfgs[ext_id].type) */

            m_params.sections[sect_num].exData[numext].type = m_extcfgs[ext_id].type;
            numext++;
            } /* for(i=0; i < m_sections[sect_num].exts.size(); i++) */

        if(numext) {
            m_params.sections[sect_num].exDataSize  = numext;
            m_params.sections[sect_num].info.ef     = 1;
            }
        else {
            m_params.sections[sect_num].exDataSize  = 0;
            m_params.sections[sect_num].info.ef     = 0;
            }
        } /* for(sect_num=0; sect_num < m_numSections; sect_num++) */

    return (0);
}


int C_plane::prepare_sections(void)
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
        m_params.sections[numsec].info.type         = m_params.sectionType;
        m_params.sections[numsec].info.startSymId   = m_params.hdr.startSymId;
        m_params.sections[numsec].info.iqWidth      = m_params.hdr.iqWidth;
        m_params.sections[numsec].info.compMeth     = m_params.hdr.compMeth;
        m_params.sections[numsec].info.id           = m_sections[numsec].sectionId;
        m_params.sections[numsec].info.rb           = m_sections[numsec].rb;
        m_params.sections[numsec].info.symInc       = m_sections[numsec].symInc;
        m_params.sections[numsec].info.startPrbc    = m_sections[numsec].startPrbc;
        m_params.sections[numsec].info.numPrbc      = m_sections[numsec].numPrbc;
        m_params.sections[numsec].info.reMask       = m_sections[numsec].reMask;
        m_params.sections[numsec].info.numSymbol    = m_sections[numsec].numSymbol;
        m_params.sections[numsec].info.beamId       = m_sections[numsec].beamId;

        switch(m_sectionType) {
            case XRAN_CP_SECTIONTYPE_1:
                break;

            case XRAN_CP_SECTIONTYPE_3:
                m_params.sections[numsec].info.freqOffset   = m_sections[numsec].freqOffset;
                break;

            default:
                return (-1);
            }
    }

    m_params.numSections        = numsec;

    return (0);
}


void C_plane::verify_sections(void)
{
  int i,j,k;

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
        EXPECT_TRUE(m_result.sections[i].info.rb        == m_params.sections[i].info.rb);
        EXPECT_TRUE(m_result.sections[i].info.symInc    == m_params.sections[i].info.symInc);
        EXPECT_TRUE(m_result.sections[i].info.startPrbc == m_params.sections[i].info.startPrbc);
        if(m_params.sections[i].info.numPrbc > 255)
            EXPECT_TRUE(m_result.sections[i].info.numPrbc == 0);
        else
            EXPECT_TRUE(m_result.sections[i].info.numPrbc == m_params.sections[i].info.numPrbc);
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
            //printf("[%d] %d ==  %d\n",i,  m_result.sections[i].exDataSize, m_params.sections[i].exDataSize);
            EXPECT_TRUE(m_result.sections[i].numExts == m_params.sections[i].exDataSize);

            for(j=0; j < m_params.sections[i].exDataSize; j++) {
                EXPECT_TRUE(m_result.sections[i].exts[j].type == m_params.sections[i].exData[j].type);

                switch(m_params.sections[i].exData[j].type) {
                    case XRAN_CP_SECTIONEXTCMD_1:
                        {
                        struct xran_sectionext1_info *ext1_params, *ext1_result;
                        int iq_size, parm_size, N;

                        ext1_params = (struct xran_sectionext1_info *)m_params.sections[i].exData[j].data;
                        ext1_result = &m_result.sections[i].exts[j].u.ext1;

                        EXPECT_TRUE(ext1_result->bfwIqWidth  == ext1_params->bfwIqWidth);
                        EXPECT_TRUE(ext1_result->bfwCompMeth == ext1_params->bfwCompMeth);

                        N = ext1_params->bfwNumber;
                        switch(ext1_params->bfwCompMeth) {
                            case XRAN_BFWCOMPMETHOD_BLKFLOAT:
                                //printf("[%d, %d] %d ==  %d\n",i, j, ext1_result->bfwCompParam.exponent, ext1_params->bfwCompParam.exponent);
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
                        iq_size = N*ext1_params->bfwIqWidth*2;  // total in bits
                        parm_size = iq_size>>3;                 // total in bytes (/8)
                        if(iq_size%8) parm_size++;              // round up
                        EXPECT_FALSE(std::memcmp(ext1_result->p_bfwIQ, ext1_params->p_bfwIQ, parm_size));
                        }
                        break;

                    case XRAN_CP_SECTIONEXTCMD_2:
                        {
                        struct xran_sectionext2_info *ext2_params, *ext2_result;

                        ext2_params = (struct xran_sectionext2_info *)m_params.sections[i].exData[j].data;
                        ext2_result = &m_result.sections[i].exts[j].u.ext2;

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

                    case XRAN_CP_SECTIONEXTCMD_3:
                        {
                        struct xran_sectionext3_info *ext3_params, *ext3_result;

                        ext3_params = (struct xran_sectionext3_info *)m_params.sections[i].exData[j].data;
                        ext3_result = &m_result.sections[i].exts[j].u.ext3;

                        EXPECT_TRUE(ext3_result->layerId    == ext3_params->layerId);
                        EXPECT_TRUE(ext3_result->codebookIdx== ext3_params->codebookIdx);
                        EXPECT_TRUE(ext3_result->numLayers  == ext3_params->numLayers);

                        if(ext3_params->layerId == XRAN_LAYERID_0
                            || ext3_params->layerId == XRAN_LAYERID_TXD) {   /* first data layer */
                            EXPECT_TRUE(ext3_result->txScheme   == ext3_params->txScheme);
                            EXPECT_TRUE(ext3_result->crsReMask  == ext3_params->crsReMask);
                            EXPECT_TRUE(ext3_result->crsShift   == ext3_params->crsShift);
                            EXPECT_TRUE(ext3_result->crsSymNum  == ext3_params->crsSymNum);

                            EXPECT_TRUE(ext3_result->numAntPort == ext3_params->numAntPort);

                            EXPECT_TRUE(ext3_result->beamIdAP1  == ext3_params->beamIdAP1);

                            if(ext3_params->numAntPort == 4) {
                                EXPECT_TRUE(ext3_result->beamIdAP2  == ext3_params->beamIdAP2);
                                EXPECT_TRUE(ext3_result->beamIdAP3  == ext3_params->beamIdAP3);
                                }
                            }
                        }
                        break;

                    case XRAN_CP_SECTIONEXTCMD_4:
                        {
                        struct xran_sectionext4_info *ext4_params, *ext4_result;

                        ext4_params = (struct xran_sectionext4_info *)m_params.sections[i].exData[j].data;
                        ext4_result = &m_result.sections[i].exts[j].u.ext4;

                        EXPECT_TRUE(ext4_result->csf            == ext4_params->csf);
                        EXPECT_TRUE(ext4_result->modCompScaler  == ext4_params->modCompScaler);
                        }
                        break;

                    case XRAN_CP_SECTIONEXTCMD_5:
                        {
                        struct xran_sectionext5_info *ext5_params, *ext5_result;
                        int idx;

                        ext5_params = (struct xran_sectionext5_info *)m_params.sections[i].exData[j].data;
                        ext5_result = &m_result.sections[i].exts[j].u.ext5;

                        EXPECT_TRUE(ext5_result->num_sets == ext5_params->num_sets);
                        for(idx=0; idx < ext5_params->num_sets; idx++) {
                            EXPECT_TRUE(ext5_result->mc[idx].csf == ext5_params->mc[idx].csf);
                            EXPECT_TRUE(ext5_result->mc[idx].mcScaleReMask == ext5_params->mc[idx].mcScaleReMask);
                            EXPECT_TRUE(ext5_result->mc[idx].mcScaleOffset == ext5_params->mc[idx].mcScaleOffset);
                            }
                        }
                        break;
                    case XRAN_CP_SECTIONEXTCMD_6:
                        {
                        struct xran_sectionext6_info *ext6_params, *ext6_result;

                        ext6_params = (struct xran_sectionext6_info *)m_params.sections[i].exData[j].data;
                        ext6_result = &m_result.sections[i].exts[j].u.ext6;

                        EXPECT_TRUE(ext6_result->rbgSize    == ext6_params->rbgSize);
                        EXPECT_TRUE(ext6_result->rbgMask    == ext6_params->rbgMask);
                        EXPECT_TRUE(ext6_result->symbolMask == ext6_params->symbolMask);
                        }
                        break;
                    case XRAN_CP_SECTIONEXTCMD_10:
                        {
                        struct xran_sectionext10_info *ext10_params, *ext10_result;
                        int idx;

                        ext10_params = (struct xran_sectionext10_info *)m_params.sections[i].exData[j].data;
                        ext10_result = &m_result.sections[i].exts[j].u.ext10;

                        EXPECT_TRUE(ext10_result->numPortc      == ext10_params->numPortc);
                        EXPECT_TRUE(ext10_result->beamGrpType   == ext10_params->beamGrpType);
                        if(ext10_result->beamGrpType == XRAN_BEAMGT_VECTORLIST) {
                            for(idx=0; idx < ext10_result->numPortc; idx++)
                                EXPECT_TRUE(ext10_result->beamID[idx] == ext10_params->beamID[idx]);
                            }
                        }
                        break;
                    case XRAN_CP_SECTIONEXTCMD_11:
                        {
                        struct xran_sectionext11_info *ext11_params;
                        struct xran_sectionext11_recv_info *ext11_result;

                        ext11_params = (struct xran_sectionext11_info *)m_params.sections[i].exData[j].data;
                        ext11_result = &m_result.sections[i].exts[j].u.ext11;

                        EXPECT_TRUE(ext11_result->RAD           == ext11_params->RAD);
                        EXPECT_TRUE(ext11_result->disableBFWs   == ext11_params->disableBFWs);
                        EXPECT_TRUE(ext11_result->numBundPrb    == ext11_params->numBundPrb);
                        EXPECT_TRUE(ext11_result->bfwCompMeth   == ext11_params->bfwCompMeth);
                        EXPECT_TRUE(ext11_result->bfwIqWidth    == ext11_params->bfwIqWidth);


                        EXPECT_TRUE(ext11_result->numSetBFWs    == ext11_params->numSetBFWs);
                        for(k=0; k < ext11_result->numSetBFWs; k++) {
                            EXPECT_TRUE(ext11_result->bundInfo[k].beamId    == m_bfwInfo[k].beamId);
#if 0
                            EXPECT_TRUE(ext11_result->bundInfo[k].BFWSize   == ext11_params->BFWSize);

                            if(ext11_result->bundInfo[k].pBFWs)
                                EXPECT_TRUE(memcmp(ext11_result->bundInfo[k].pBFWs,
                                                ext11_params->bundInfo[k].pBFWs + ext11_params->bundInfo[k].offset + 4,
                                                ext11_result->bundInfo[k].BFWSize) == 0);
                            switch(ext11_result->bfwCompMeth) {
                                case XRAN_BFWCOMPMETHOD_NONE:
                                    break;

                                case XRAN_BFWCOMPMETHOD_BLKFLOAT:
                                    EXPECT_TRUE(ext11_result->bundInfo[k].bfwCompParam.exponent == ext11_params->bundInfo[k].bfwCompParam.exponent);
                                    break;

                                default:
                                    FAIL() << "Invalid BfComp method - %d" << ext11_result->bfwCompMeth << std::endl;
                                }
#endif
                            }
                        }
                        break;
                    }
                }
            }
        }

    return;
}

void C_plane::test_ext1(void)
{
    int i = 0, idRb;
    int16_t *ptr = NULL;
    int32_t nRbs = 36;
    int32_t nAntElm = 64;
    int8_t  iqWidth = 9;
    int8_t  compMethod = XRAN_COMPMETHOD_BLKFLOAT;
    int8_t  *p_ext1_dst  = NULL;
    int8_t  *bfw_payload = NULL;
    int32_t expected_len = ((nAntElm/16*4*iqWidth)+1)*nRbs + /* bfwCompParam + IQ = */
                            sizeof(struct xran_cp_radioapp_section_ext1)*nRbs; /* ext1 Headers */

    int16_t  ext_len       = 9600;
    int16_t  ext_sec_total = 0;
    int8_t * ext_buf       = nullptr;
    int8_t * ext_buf_init  = nullptr;

    struct xran_section_gen_info* loc_pSectGenInfo = m_params.sections;
    struct xran_sectionext1_info m_prep_ext1;
    struct xran_cp_radioapp_section_ext1 *p_ext1;
    struct rte_mbuf_ext_shared_info  share_data;
    struct rte_mbuf *mbuf = NULL;


    nAntElm     = m_antElmTRx;
    nRbs        = m_params.sections[0].info.numPrbc;
    iqWidth     = m_extcfgs[0].u.ext1.bfwIqWidth;
    compMethod  = m_extcfgs[0].u.ext1.bfwCompMeth;

    switch(compMethod) {
        case XRAN_BFWCOMPMETHOD_NONE:
            expected_len = (3+1)*nRbs + nAntElm*nRbs*4;
            break;
        case XRAN_BFWCOMPMETHOD_BLKFLOAT:
            expected_len = ((nAntElm/16*4*iqWidth)+1)*nRbs + /* bfwCompParam + IQ = */
                            sizeof(struct xran_cp_radioapp_section_ext1)*nRbs; /* ext1 Headers */
            break;
        default:
            FAIL() << "Unsupported Compression Method - " << compMethod << std::endl;
        }

    if(loc_pSectGenInfo->info.type == XRAN_CP_SECTIONTYPE_1) {
        /* extType 1 only with Section 1 for now */

        ext_buf  = ext_buf_init = (int8_t*) xran_malloc(ext_len);
        if (ext_buf) {
            ptr = m_p_bfw_iq_src;

            for (idRb =0; idRb < nRbs*nAntElm*2; idRb++){
                ptr[idRb] = i;
                i++;
                }

            ext_buf += (RTE_PKTMBUF_HEADROOM +
                       sizeof (struct xran_ecpri_hdr) +
                       sizeof(struct xran_cp_radioapp_common_header) +
                       sizeof(struct xran_cp_radioapp_section1));

            ext_len -= (RTE_PKTMBUF_HEADROOM +
                        sizeof(struct xran_ecpri_hdr) +
                        sizeof(struct xran_cp_radioapp_common_header) +
                        sizeof(struct xran_cp_radioapp_section1));

            ext_sec_total = xran_cp_populate_section_ext_1((int8_t *)ext_buf,
                                                 ext_len,
                                                 m_p_bfw_iq_src,
                                                 nRbs,
                                                 nAntElm,
                                                 iqWidth,
                                                 compMethod);

            ASSERT_TRUE(ext_sec_total == expected_len);
            p_ext1_dst = ext_buf;

            memset(&m_temp_ext1[0], 0, sizeof (struct xran_sectionext1_info)*XRAN_MAX_PRBS);

            idRb = 0;
            do {
                p_ext1 = (struct xran_cp_radioapp_section_ext1 *)p_ext1_dst;
                bfw_payload = (int8_t*)(p_ext1+1);
                p_ext1_dst += p_ext1->extLen*XRAN_SECTIONEXT_ALIGN;

                m_temp_ext1[idRb].bfwNumber      = nAntElm;
                m_temp_ext1[idRb].bfwIqWidth     = iqWidth;
                m_temp_ext1[idRb].bfwCompMeth    = compMethod;

                if(compMethod == XRAN_BFWCOMPMETHOD_BLKFLOAT) {
                    m_temp_ext1[idRb].bfwCompParam.exponent = *bfw_payload++ & 0xF;
                    }

                m_temp_ext1[idRb].p_bfwIQ               = (int16_t*)bfw_payload;
                m_temp_ext1[idRb].bfwIQ_sz              = p_ext1->extLen*XRAN_SECTIONEXT_ALIGN;

                loc_pSectGenInfo->exData[idRb].type = XRAN_CP_SECTIONEXTCMD_1;
                loc_pSectGenInfo->exData[idRb].len  = sizeof(m_temp_ext1[idRb]);
                loc_pSectGenInfo->exData[idRb].data = &m_temp_ext1[idRb];

                idRb++;
            } while(p_ext1->ef != XRAN_EF_F_LAST);

            ASSERT_TRUE(idRb == nRbs);

            mbuf = xran_attach_cp_ext_buf(0, ext_buf_init, ext_buf, ext_sec_total, &share_data);

            /* Update section information */
            memset(&m_prep_ext1, 0, sizeof (struct xran_sectionext1_info));
            m_prep_ext1.bfwNumber      = nAntElm;
            m_prep_ext1.bfwIqWidth     = iqWidth;
            m_prep_ext1.bfwCompMeth    = compMethod;
            m_prep_ext1.p_bfwIQ        = (int16_t*)ext_buf;
            m_prep_ext1.bfwIQ_sz       = ext_sec_total;


            loc_pSectGenInfo->exData[0].type = XRAN_CP_SECTIONEXTCMD_1;
            loc_pSectGenInfo->exData[0].len  = sizeof(m_prep_ext1);
            loc_pSectGenInfo->exData[0].data = &m_prep_ext1;

            loc_pSectGenInfo->info.ef       = 1;
            loc_pSectGenInfo->exDataSize    = 1; /* append all extType1 as one shot
                                                    (as generated via xran_cp_populate_section_ext_1)*/

            m_params.numSections    = 1;

            /* Generating C-Plane packet */
            ASSERT_TRUE(xran_prepare_ctrl_pkt(/*m_pTestBuffer*/mbuf, &m_params, m_ccId, m_antId, m_seqId) == XRAN_STATUS_SUCCESS);

            /** to match O-RU parsing */
            loc_pSectGenInfo->exDataSize = nRbs;
            loc_pSectGenInfo->exData[0].len  = sizeof(m_temp_ext1[0]);
            loc_pSectGenInfo->exData[0].data = &m_temp_ext1[0];

            /* Parsing generated packet */
            EXPECT_TRUE(xran_parse_cp_pkt(/*m_pTestBuffer*/mbuf, &m_result, &m_pktInfo) == XRAN_STATUS_SUCCESS);
            }
        else {
            FAIL() << "xran_malloc failed\n";
            }

        /* Verify the result */
        verify_sections();

        if(ext_buf_init)
            xran_free(ext_buf_init);
        }
}

/***************************************************************************
 * Functional Test cases
 ***************************************************************************/

TEST_P(C_plane, CPacketGen)
{
    /* Configure section information */
    if(prepare_sections() < 0) {
        FAIL() << "Invalid Section configuration\n";
    }
    if(prepare_extensions() < 0) {
        FAIL() << "Invalid Section extension configuration\n";
    }
    if(m_nextcfgs) {
        if(m_extcfgs[0].type == XRAN_CP_SECTIONEXTCMD_1) {
            test_ext1();
            return;
        }
        else if(m_extcfgs[0].type == XRAN_CP_SECTIONEXTCMD_11) {
            /* use large buffer to linearize external buffer for parsing */
            m_pTestBuffer = xran_ethdi_mbuf_alloc();
            ASSERT_FALSE(m_pTestBuffer == nullptr);

            if(xran_cp_attach_ext_buf(m_pTestBuffer,
                                m_pBfwIQ_ext,
                                m_extcfgs[0].u.ext11.maxExtBufSize,
                                &m_extSharedInfo) < 0) {
                rte_pktmbuf_free(m_pTestBuffer);
                m_pTestBuffer = nullptr;
                FAIL() << "Failed to attach external buffer !!\n";
            }
            rte_mbuf_ext_refcnt_update(&m_extSharedInfo, 0);
        }
    }

    if(m_pTestBuffer == nullptr) {
        m_pTestBuffer = xran_ethdi_mbuf_alloc();
        ASSERT_FALSE(m_pTestBuffer == nullptr);
    }

    /* Generating C-Plane packet */
    ASSERT_TRUE(xran_prepare_ctrl_pkt(m_pTestBuffer, &m_params, m_ccId, m_antId, m_seqId) == XRAN_STATUS_SUCCESS);

    /* Linearize data in the chain of mbufs to parse generated packet*/
    ASSERT_TRUE(rte_pktmbuf_linearize(m_pTestBuffer) == 0);

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
    if(prepare_sections() < 0) {
        FAIL() << "Invalid Section configuration\n";
        }
    if(prepare_extensions() < 0) {
        FAIL() << "Invalid Section extension configuration\n";
        }

    /* using wrapper function to reset mbuf */
    performance("C", module_name,
            &xran_ut_prepare_cp, &m_params, m_ccId, m_antId, m_seqId);
}



INSTANTIATE_TEST_CASE_P(UnitTest, C_plane,
        testing::ValuesIn(get_sequence(C_plane::get_number_of_cases("C_Plane"))));

