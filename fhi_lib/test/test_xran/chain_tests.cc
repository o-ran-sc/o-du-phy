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



const std::string module_name = "C-Plane";

const uint8_t m_bitmask[] = { 0x00, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff };


extern "C"
{

/* external functions in xRAN library */
void tx_cp_dl_cb(struct rte_timer *tim, void *arg);
void tx_cp_ul_cb(struct rte_timer *tim, void *arg);
int xran_process_tx_sym(void *arg);
int process_mbuf(struct rte_mbuf *pkt, void *arg, struct xran_eaxc_info *p_cid);


/* wrapper functions for performace tests */
void xran_ut_tx_cp_dl()
{
    xranlib->update_tti();
    tx_cp_dl_cb(nullptr, xranlib->get_timer_ctx());
}

void xran_ut_tx_cp_ul()
{
    xranlib->update_tti();
    tx_cp_ul_cb(nullptr, xranlib->get_timer_ctx());
}

void xran_ut_tx_up_dl()
{
    xranlib->update_symbol_index();
    xran_process_tx_sym(xranlib->get_timer_ctx());
}

void xran_ut_tx_cpup_dl()
{
    xranlib->update_symbol_index();

    if(xranlib->get_symbol_index() == 3)
        tx_cp_dl_cb(nullptr, xranlib->get_timer_ctx());

    xran_process_tx_sym(xranlib->get_timer_ctx());
}

#if 0   /* TBD */
void xran_ut_rx_up_ul()
{
    process_mbf(mbuf);
}
#endif


/* call back functions */
int send_mbuf_up(struct rte_mbuf *mbuf, uint16_t type, uint16_t vf_id)
{
    rte_pktmbuf_free(mbuf);

    return (1);
}

int send_mbuf_cp_perf(struct rte_mbuf *mbuf, uint16_t type, uint16_t vf_id)
{
    rte_pktmbuf_free(mbuf);
    /*  TODO: need to free chained mbufs */
    return (1);
}

#if 0   /* TBD */
int send_mbuf_cp(struct rte_mbuf *mbuf, uint16_t type)
{
#if 0
    xran_parse_cp_pkt(m_pTestBuffer, &m_result, &m_pktInfo);

    /* Verify the result */
    verify_sections();
#else
    printf("cp\n");
#endif
    return (1);
}
#endif

void utcp_fh_rx_callback(void *pCallbackTag, xran_status_t status)
{
    return;
}

void utcp_fh_srs_callback(void *pCallbackTag, xran_status_t status)
{
    return;
}

void utcp_fh_rx_prach_callback(void *pCallbackTag, xran_status_t status)
{
    rte_pause();
}

} /* extern "C" */


class TestChain: public KernelTests
{
protected:
    struct xran_fh_config   m_xranConf;
    struct xran_fh_init     m_xranInit;

    bool m_bSub6;


    void SetUp() override
    {
        int temp;
        std::string tmpstr;


        init_test("TestChain");

        xranlib->get_cfg_fh(&m_xranConf);

        tmpstr = get_input_parameter<std::string>("category");
        if(tmpstr == "A")
            m_xranConf.ru_conf.xranCat = XRAN_CATEGORY_A;
        else if(tmpstr == "B")
            m_xranConf.ru_conf.xranCat = XRAN_CATEGORY_B;
        else {
            std::cout << "*** Invalid RU Category [" << tmpstr << "] !!!" << std::endl;
            std::cout << "Set it to Category A... " << std::endl;
            m_xranConf.ru_conf.xranCat = XRAN_CATEGORY_A;
            }

        m_xranConf.frame_conf.nNumerology = get_input_parameter<int>("mu");
        if(m_xranConf.frame_conf.nNumerology > 3) {
            std::cout << "*** Invalid Numerology [" << m_xranConf.frame_conf.nNumerology << "] !!!" << std::endl;
            m_xranConf.frame_conf.nNumerology   = 0;
            std::cout << "Set it to " << m_xranConf.frame_conf.nNumerology << "..." << std::endl;
            }

        tmpstr = get_input_parameter<std::string>("duplex");
        if(tmpstr == "FDD")
            m_xranConf.frame_conf.nFrameDuplexType  = 0;
        else if(tmpstr == "TDD") {
            m_xranConf.frame_conf.nFrameDuplexType  = 1;

            tmpstr = get_input_parameter<std::string>("slot_config");
            temp = xranlib->get_slot_config(tmpstr, &m_xranConf.frame_conf);
            }
        else {
            std::cout << "*** Invalid Duplex type [" << tmpstr << "] !!!" << std::endl;
            std::cout << "Set it to FDD... " << std::endl;
            m_xranConf.frame_conf.nFrameDuplexType  = 0;
            }

        m_xranConf.nCC = get_input_parameter<int>("num_cc");
        if(m_xranConf.nCC > XRAN_MAX_SECTOR_NR) {
            std::cout << "*** Exceeds maximum number of carriers supported [" << m_xranConf.nCC << "] !!!" << std::endl;
            m_xranConf.nCC = XRAN_MAX_SECTOR_NR;
            std::cout << "Set it to " << m_xranConf.nCC << "..." << std::endl;
            }
        m_xranConf.neAxc = get_input_parameter<int>("num_eaxc");
        if(m_xranConf.neAxc > XRAN_MAX_ANTENNA_NR) {
            std::cout << "*** Exceeds maximum number of antenna supported [" << m_xranConf.neAxc << "] !!!" << std::endl;
            m_xranConf.neAxc = XRAN_MAX_ANTENNA_NR;
            std::cout << "Set it to " << m_xranConf.neAxc << "..." << std::endl;
            }

        m_bSub6     = get_input_parameter<bool>("sub6");
        temp = get_input_parameter<int>("chbw_dl");
        m_xranConf.nDLRBs = xranlib->get_num_rbs(m_xranConf.frame_conf.nNumerology, temp, m_bSub6);
        temp = get_input_parameter<int>("chbw_ul");
        m_xranConf.nULRBs = xranlib->get_num_rbs(m_xranConf.frame_conf.nNumerology, temp, m_bSub6);

        m_xranConf.nAntElmTRx = get_input_parameter<int>("antelm_trx");
        m_xranConf.nDLFftSize = get_input_parameter<int>("fftsize_dl");
        m_xranConf.nULFftSize = get_input_parameter<int>("fftsize_ul");


        m_xranConf.ru_conf.iqWidth  = get_input_parameter<int>("iq_width");
        m_xranConf.ru_conf.compMeth = get_input_parameter<int>("comp_meth");

#if 0
        temp = get_input_parameter<int>("fft_size");
        m_xranConf.ru_conf.fftSize  = 0;
        while (temp >>= 1)
            ++m_xranConf.ru_conf.fftSize;
#endif

    }

    void TearDown() override
    {
    }
};




/***************************************************************************
 * Performance Test cases
 ***************************************************************************/
/* C-Plane DL chain (tx_cp_dl_cb) only */
TEST_P(TestChain, CPlaneDLPerf)
{
    xranlib->Init(0, &m_xranConf);
    xranlib->Open(0, send_mbuf_cp_perf, send_mbuf_up,
            (void *)utcp_fh_rx_callback, (void *)utcp_fh_rx_prach_callback, (void *)utcp_fh_srs_callback);

    performance("C", module_name, xran_ut_tx_cp_dl);

    xranlib->Close();
    xranlib->Cleanup();
}

/* C-Plane UL chain (tx_cp_ul_cb) only */
TEST_P(TestChain, CPlaneULPerf)
{
    xranlib->Init(0, &m_xranConf);
    xranlib->Open(0, send_mbuf_cp_perf, send_mbuf_up,
            (void *)utcp_fh_rx_callback, (void *)utcp_fh_rx_prach_callback, (void *)utcp_fh_srs_callback);

    performance("C", module_name, xran_ut_tx_cp_ul);

    xranlib->Close();
    xranlib->Cleanup();
}

/* U-Plane UL chain (process_tx_sym with disable CP) */
TEST_P(TestChain, UPlaneDLPerf)
{
    bool flag_cpen;

    xranlib->Init(0, &m_xranConf);

    /* save current CP enable flag */
    flag_cpen = xranlib->is_cpenable()?true:false;

    /* need to disable CP to make U-Plane work without CP */
    xranlib->apply_cpenable(false);
    xranlib->Open(0, send_mbuf_cp_perf, send_mbuf_up,
            (void *)utcp_fh_rx_callback, (void *)utcp_fh_rx_prach_callback, (void *)utcp_fh_srs_callback);

    performance("C", module_name, xran_ut_tx_up_dl);

    xranlib->Close();
    xranlib->Cleanup();

    /* restore previous CP enable flag */
    xranlib->apply_cpenable(flag_cpen);
}

/* C-Plane and U-Plane DL chain, U-Plane will be generated by C-Plane config */
TEST_P(TestChain, APlaneDLPerf)
{
    bool flag_cpen;

    xranlib->Init(0, &m_xranConf);

    /* save current CP enable flag */
    flag_cpen = xranlib->is_cpenable()?true:false;

    /* Enable CP by force to make UP work by CP's section information */
    xranlib->apply_cpenable(true);
    xranlib->Open(0, send_mbuf_cp_perf, send_mbuf_up,
            (void *)utcp_fh_rx_callback, (void *)utcp_fh_rx_prach_callback, (void *)utcp_fh_srs_callback);

    performance("C", module_name, xran_ut_tx_cpup_dl);

    xranlib->Close();
    xranlib->Cleanup();

    /* restore previous CP enable flag */
    xranlib->apply_cpenable(flag_cpen);
}

#if 0       /* TBD */
TEST_P(TestChain, UPlaneULPerf)
{
}
#endif


INSTANTIATE_TEST_CASE_P(UnitTest, TestChain,
        testing::ValuesIn(get_sequence(TestChain::get_number_of_cases("TestChain"))));

