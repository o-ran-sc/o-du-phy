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
#include "xran_cp_api.h"
#include "xran_lib_wrap.hpp"
#include "xran_common.h"
#include "xran_ethdi.h"
#include "xran_timer.h"

#include <stdint.h>
#include <iostream>
#include <vector>
#include <string>



using namespace std;
const std::string module_name = "init_sys_functional";

extern enum xran_if_state xran_if_current_state;

int32_t physide_sym_call_back(void * param, struct xran_sense_of_time *time)
{
    rte_pause();
    return 0;
}

int physide_dl_tti_call_back(void * param)
{
    rte_pause();
    return 0;
}

int physide_ul_half_slot_call_back(void * param)
{
    rte_pause();
    return 0;
}

int physide_ul_full_slot_call_back(void * param)
{
    rte_pause();
    return 0;
}

void xran_fh_rx_callback(void *pCallbackTag, xran_status_t status, uint8_t mu)
{
    rte_pause();
    return;
}

void xran_fh_bfw_callback(void *pCallbackTag, xran_status_t status)
{
    rte_pause();
    return;
}

void xran_fh_srs_callback(void *pCallbackTag, xran_status_t status)
{
    rte_pause();
    return;
}


void xran_fh_rx_prach_callback(void *pCallbackTag, xran_status_t status)
{

    rte_pause();
}

#ifdef POLL_EBBU_OFFLOAD
int32_t xran_fh_poll_ebbu_offload(uint32_t, uint32_t, uint32_t, void*)
{
    rte_pause();
    return 0;
}
#endif

class Init_Sys_Check : public KernelTests
{
protected:

    void SetUp() override
    {
        xranlib->Init(0);
        xranlib->Open(0, nullptr, nullptr, (void *)xran_fh_rx_callback, (void *)xran_fh_bfw_callback, (void *)xran_fh_rx_prach_callback, (void *)xran_fh_srs_callback);
    }

    /* It's called after an execution of the each test case.*/
    void TearDown() override
    {
        xranlib->Close();
        xranlib->Cleanup();
    }

public:

    BbuIoBufCtrlStruct sFrontHaulTxBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    BbuIoBufCtrlStruct sFrontHaulTxPrbMapBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    BbuIoBufCtrlStruct sFrontHaulRxBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    BbuIoBufCtrlStruct sFrontHaulRxPrbMapBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    BbuIoBufCtrlStruct sFHPrachRxBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];

    BbuIoBufCtrlStruct sFHCpRxPrbMapBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    BbuIoBufCtrlStruct sFHCpTxPrbMapBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    /* buffers lists */
    struct xran_flat_buffer sFrontHaulTxBuffers[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_NUM_OF_SYMBOL_PER_SLOT];
    struct xran_flat_buffer sFrontHaulTxPrbMapBuffers[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    struct xran_flat_buffer sFrontHaulRxBuffers[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_NUM_OF_SYMBOL_PER_SLOT];
    struct xran_flat_buffer sFrontHaulRxPrbMapBuffers[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    struct xran_flat_buffer sFHPrachRxBuffers[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_NUM_OF_SYMBOL_PER_SLOT];

    void*    nInstanceHandle[XRAN_PORTS_NUM][XRAN_MAX_SECTOR_NR]; // instance per sector
    uint32_t nBufPoolIndex[XRAN_MAX_SECTOR_NR][xranLibWraper::MAX_SW_XRAN_INTERFACE_NUM];
    uint16_t nInstanceNum;
};

TEST_P(Init_Sys_Check, Test_Open_Close)
{
    struct xran_device_ctx * p_xran_dev_ctx = XRAN_GET_DEV_CTX;
    /* check stat of lib */
    ASSERT_EQ(1, p_xran_dev_ctx->enableCP);
    ASSERT_EQ(1, p_xran_dev_ctx->xran2phy_mem_ready);
}

TEST_P(Init_Sys_Check, Test_xran_mm_init)
{
    int16_t ret = 0;
    ret = xran_mm_init (xranlib->get_xranhandle(), (uint64_t) SW_FPGA_FH_TOTAL_BUFFER_LEN, SW_FPGA_SEGMENT_BUFFER_LEN);
    ASSERT_EQ(0, ret);
}

/* this case cannot be tested since memory cannot be initialized twice */
/* memory initialization is moved to the wrapper class */
#if 0
TEST_P(Init_Sys_Check, Test_xran_bm_init_alloc_free)
{
    int16_t ret = 0;
    void *ptr;
    void *mb;
    uint32_t nSW_ToFpga_FTH_TxBufferLen   = 13168; /* 273*12*4 + 64*/
    int16_t k = 0;


    struct xran_buffer_list *pFthTxBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
    struct xran_buffer_list *pFthTxPrbMapBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
    struct xran_buffer_list *pFthRxBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
    struct xran_buffer_list *pFthRxPrbMapBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
    struct xran_buffer_list *pFthRxRachBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
    struct xran_buffer_list *pFthRxRachBufferDecomp[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];

    Init_Sys_Check::nInstanceNum = xranlib->get_num_cc();

    for (k = 0; k < XRAN_PORTS_NUM; k++) {
        ret = xran_sector_get_instances (xranlib->get_xranhandle(), Init_Sys_Check::nInstanceNum, &(Init_Sys_Check::nInstanceHandle[k][0]));
        ASSERT_EQ(0, ret);
        ASSERT_EQ(1, Init_Sys_Check::nInstanceNum);
    }


    ret = xran_bm_init(Init_Sys_Check::nInstanceHandle[0][0],
                    &Init_Sys_Check::nBufPoolIndex[0][0],
                    XRAN_N_FE_BUF_LEN*XRAN_MAX_ANTENNA_NR*XRAN_NUM_OF_SYMBOL_PER_SLOT, nSW_ToFpga_FTH_TxBufferLen);
    ASSERT_EQ(0, ret);

    ret = xran_bm_allocate_buffer(Init_Sys_Check::nInstanceHandle[0][0], Init_Sys_Check::nBufPoolIndex[0][0],&ptr, &mb);
    ASSERT_EQ(0, ret);
    ASSERT_NE(ptr, nullptr);
    ASSERT_NE(mb, nullptr);

    ret = xran_bm_free_buffer(Init_Sys_Check::nInstanceHandle[0][0], ptr, mb);
    ASSERT_EQ(0, ret);



    for(int i=0; i< xranlib->get_num_cc(); i++)
    {
        for(int j=0; j<XRAN_N_FE_BUF_LEN; j++)
        {
            for(int z = 0; z < XRAN_MAX_ANTENNA_NR; z++){
                pFthTxBuffer[i][z][j]     = &(Init_Sys_Check::sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList);
                pFthTxPrbMapBuffer[i][z][j]     = &(Init_Sys_Check::sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList);
                pFthRxBuffer[i][z][j]     = &(Init_Sys_Check::sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList);
                pFthRxPrbMapBuffer[i][z][j]     = &(Init_Sys_Check::sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList);
                pFthRxRachBuffer[i][z][j] = &(Init_Sys_Check::sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList);
                pFthRxRachBufferDecomp[i][z][j] = &(Init_Sys_Check::sFHPrachRxBbuIoBufCtrlDecomp[j][i][z].sBufferList);
            }
        }
    }

    if(NULL != Init_Sys_Check::nInstanceHandle[0])
    {
        for (int i = 0; i < xranlib->get_num_cc(); i++)
        {
            ret = xran_5g_fronthault_config (Init_Sys_Check::nInstanceHandle[0][i],
                pFthTxBuffer[i],
                pFthTxPrbMapBuffer[i],
                pFthRxBuffer[i],
                pFthRxPrbMapBuffer[i],
                xran_fh_rx_callback,  &pFthRxBuffer[i][0]);

            ASSERT_EQ(0, ret);
        }

        // add prach callback here
        for (int i = 0; i < xranlib->get_num_cc(); i++)
        {
            ret = xran_5g_prach_req(Init_Sys_Check::nInstanceHandle[0][i], pFthRxRachBuffer[i], pFthRxRachBufferDecomp[i],
                xran_fh_rx_prach_callback,&pFthRxRachBuffer[i][0]);
            ASSERT_EQ(0, ret);
        }
    }


}
#endif

TEST_P(Init_Sys_Check, Test_xran_get_common_counters)
{
    int16_t ret = 0;
    struct xran_common_counters x_counters;

    ret = xran_get_common_counters(xranlib->get_xranhandle(), &x_counters);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(0, x_counters.Rx_on_time);
    ASSERT_EQ(0, x_counters.Rx_early);
    ASSERT_EQ(0, x_counters.Rx_late);
    ASSERT_EQ(0, x_counters.Rx_corrupt);
    ASSERT_EQ(0, x_counters.Rx_pkt_dupl);
    ASSERT_EQ(0, x_counters.Total_msgs_rcvd);
}

#if 0   /* This test case is not valid since SfIdx could note be zero by system time */
TEST_P(Init_Sys_Check, Test_xran_get_slot_idx)
{
#define NUM_OF_SUBFRAME_PER_FRAME 10
    int32_t nNrOfSlotInSf = 1;
    int32_t nSfIdx = -1;
    uint32_t nFrameIdx;
    uint32_t nSubframeIdx;
    uint32_t nSlotIdx;
    uint64_t nSecond;
    uint8_t mu = 1;

    uint32_t nXranTime  = xran_get_slot_idx(0, &nFrameIdx, &nSubframeIdx, &nSlotIdx, &nSecond, mu);
    nSfIdx = nFrameIdx*NUM_OF_SUBFRAME_PER_FRAME*nNrOfSlotInSf
        + nSubframeIdx*nNrOfSlotInSf
        + nSlotIdx;

    ASSERT_EQ(0, nSfIdx);
}
#endif

TEST_P(Init_Sys_Check, Test_xran_timingsource_reg_tticb)
{
    int16_t ret = 0;
    struct xran_timing_source_ctx *pTmCtx = xran_timingsource_get_ctx();

    ret = xran_timingsource_reg_tticb(xranlib->get_xranhandle(), (xran_fh_tti_callback_fn) physide_dl_tti_call_back, NULL, 10, XRAN_CB_TTI);
    ASSERT_EQ(0,ret);
    ASSERT_EQ((long long)physide_dl_tti_call_back, (long long)pTmCtx->ttiCb[XRAN_CB_TTI]);
    ASSERT_EQ(NULL, pTmCtx->TtiCbParam[XRAN_CB_TTI]);
    ASSERT_EQ(10, pTmCtx->SkipTti[XRAN_CB_TTI]);

    ret = xran_timingsource_reg_tticb(xranlib->get_xranhandle(), (xran_fh_tti_callback_fn)physide_ul_half_slot_call_back, NULL, 10, XRAN_CB_HALF_SLOT_RX);
    ASSERT_EQ(0,ret);
    ASSERT_EQ((long long)physide_ul_half_slot_call_back, (long long)pTmCtx->ttiCb[XRAN_CB_HALF_SLOT_RX]);
    ASSERT_EQ((long long)NULL, (long long)pTmCtx->TtiCbParam[XRAN_CB_HALF_SLOT_RX]);
    ASSERT_EQ(10, pTmCtx->SkipTti[XRAN_CB_HALF_SLOT_RX]);

    ret = xran_timingsource_reg_tticb(xranlib->get_xranhandle(), (xran_fh_tti_callback_fn)physide_ul_full_slot_call_back, NULL, 10, XRAN_CB_FULL_SLOT_RX);
    ASSERT_EQ(0,ret);
    ASSERT_EQ((long long)physide_ul_full_slot_call_back,(long long) pTmCtx->ttiCb[XRAN_CB_FULL_SLOT_RX]);
    ASSERT_EQ(NULL, pTmCtx->TtiCbParam[XRAN_CB_FULL_SLOT_RX]);
    ASSERT_EQ(10, pTmCtx->SkipTti[XRAN_CB_FULL_SLOT_RX]);
}

TEST_P(Init_Sys_Check, Test_xran_reg_sym_cb){
    int16_t ret = 0;
    uint8_t mu = xranlib->get_numerology();
    ret = xran_reg_sym_cb(xranlib->get_xranhandle(), physide_sym_call_back, NULL, NULL, 11, XRAN_CB_SYM_RX_WIN_END, XRAN_DEFAULT_MU);
    ASSERT_EQ(0,ret);
}

TEST_P(Init_Sys_Check, Test_xran_mm_destroy){
    int16_t ret = 0;
    ret = xran_mm_destroy(xranlib->get_xranhandle());
    ASSERT_EQ(0,ret);
}

TEST_P(Init_Sys_Check, Test_xran_start_stop){
    int16_t ret = 0;
    ASSERT_EQ(XRAN_INIT, xran_if_current_state);
    ret = xranlib->Start();
    ASSERT_EQ(0,ret);
    ASSERT_EQ(XRAN_RUNNING, xran_if_current_state);
    ret = xranlib->Stop();
    ASSERT_EQ(0,ret);
    ASSERT_EQ(XRAN_STOPPED, xran_if_current_state);
}

#ifdef POLL_EBBU_OFFLOAD
TEST_P(Init_Sys_Check, Test_xran_timing_destroy_cbs){
    int32_t ret = 0;

    ret = xran_timing_destroy_cbs(xranlib->get_timer_ctx());

    ASSERT_EQ(0,ret);
}

TEST_P(Init_Sys_Check, Test_xran_timer_get_ctx_ebbu_offload){
    PXRAN_TIMER_CTX ret = NULL;

    ret = xran_timer_get_ctx_ebbu_offload();

    ASSERT_TRUE(ret != NULL);
}

TEST_P(Init_Sys_Check, Test_xran_sym_poll_task_ebbu_offload){
    int32_t ret = 0;

    ret = xran_sym_poll_task_ebbu_offload();

    ASSERT_EQ(0,ret);
}

TEST_P(Init_Sys_Check, Test_timing_adjust_gps_second_ebbu_offload){
    int32_t ret = 0;

    ret = timing_adjust_gps_second_ebbu_offload(&(xran_timer_get_ctx_ebbu_offload()->ebbu_offload_last_time));

    ASSERT_EQ(0,ret);
}

TEST_P(Init_Sys_Check, Test_xran_sym_poll_callback_task_ebbu_offload){
    int16_t ret = 0;
    PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();

    pCtx->pFn = xran_fh_poll_ebbu_offload;
    ret = xran_sym_poll_callback_task_ebbu_offload();
    pCtx->pFn = NULL;

    ASSERT_EQ(0,ret);
}

TEST_P(Init_Sys_Check, Test_xran_pkt_proc_poll_task_ebbu_offload){
    int16_t ret = 0;
    int32_t i, qi;
    char ring_name[32] = "";
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();

    pCtx->pFn = xran_fh_poll_ebbu_offload;
    xran_if_current_state = XRAN_RUNNING;
    for (i = 0; i < XRAN_VF_MAX && i < ctx->io_cfg.num_vfs; i++)
    {
        ctx->vf2xran_port[i] = 0;
        if(ctx->tx_ring[i] == NULL)
        {
            snprintf(ring_name, RTE_DIM(ring_name), "%s_%d", "tx_ring_up", i);
            ctx->tx_ring[i] = rte_ring_create(ring_name, NUM_MBUFS_RING_TRX,
                                  0, RING_F_SC_DEQ);
        }
        for(qi = 0; qi < ctx->io_cfg.num_rxq; qi++) {
            if(ctx->rx_ring[i][qi] == NULL)
            {
                snprintf(ring_name, RTE_DIM(ring_name), "%s_%d_%d", "rx_ring_up", i, qi);
                ctx->rx_ring[i][qi] = rte_ring_create(ring_name, NUM_MBUFS_RING_TRX,
                                          0, RING_F_SP_ENQ);
            }
        }
    }
    ret = xran_pkt_proc_poll_task_ebbu_offload();
    for (i = 0; i < XRAN_VF_MAX && i < ctx->io_cfg.num_vfs; i++)
    {
        if(ctx->tx_ring[i] != NULL)
        {
            rte_ring_free(ctx->tx_ring[i]);
        }
        for(qi = 0; qi < ctx->io_cfg.num_rxq; qi++) {
            if(ctx->rx_ring[i][qi] != NULL)
            {
                rte_ring_free(ctx->rx_ring[i][qi]);
            }
        }
    }
    pCtx->pFn = NULL;
    xran_if_current_state = XRAN_STOPPED;

    ASSERT_EQ(0,ret);
}

TEST_P(Init_Sys_Check, Test_xran_task_dl_cp_ebbu_offload){
    int16_t ret = 0, mu;
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
    PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();

    pCtx->pFn = xran_fh_poll_ebbu_offload;
    /* This eBBUPOOL polling event processing feature only support numerology 0 and 1 now */
    for(mu = 0; mu < 2; mu++)
    {
        ret = xran_task_dl_cp_ebbu_offload(&(XRAN_GET_DEV_CTX->perMu[mu]));
    }
    pCtx->pFn = NULL;

    ASSERT_EQ(0,ret);
}

TEST_P(Init_Sys_Check, Test_xran_task_ul_cp_ebbu_offload){
    int16_t ret = 0, mu;
    PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();

    pCtx->pFn = xran_fh_poll_ebbu_offload;
    /* This eBBUPOOL polling event processing feature only support numerology 0 and 1 now */
    for(mu = 0; mu < 2; mu++)
    {
        ret = xran_task_ul_cp_ebbu_offload(&(XRAN_GET_DEV_CTX->perMu[mu]));
    }
    pCtx->pFn = NULL;

    ASSERT_EQ(0,ret);
}

TEST_P(Init_Sys_Check, Test_xran_task_tti_ebbu_offload){
    int16_t ret = 0, mu = 0;
    PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();

    pCtx->pFn = xran_fh_poll_ebbu_offload;
    /* This eBBUPOOL polling event processing feature only support numerology 0 and 1 now */
    for(mu = 0; mu < 2; mu++)
    {
        ret = xran_task_tti_ebbu_offload(&(XRAN_GET_DEV_CTX->perMu[mu]));
    }
    pCtx->pFn = NULL;

    ASSERT_EQ(0,ret);
}

TEST_P(Init_Sys_Check, Test_xran_updateSfnSecStart){
    int16_t ret = 0;
    PXRAN_TIMER_CTX pCtx = xran_timer_get_ctx_ebbu_offload();

    xran_updateSfnSecStart();

    ASSERT_GE(pCtx->xran_SFN_at_Sec_Start,0);
}

TEST_P(Init_Sys_Check, Test_xran_fh_counters_ebbu_offload){
    struct xran_common_counters* ret = NULL;

    ret = xran_fh_counters_ebbu_offload();

    ASSERT_TRUE(ret != NULL);
}

TEST_P(Init_Sys_Check, Test_xran_dev_get_ctx_ebbu_offload){
    struct xran_device_ctx* ret = NULL;

    ret = xran_dev_get_ctx_ebbu_offload();

    ASSERT_TRUE(ret != NULL);
}
#endif

INSTANTIATE_TEST_CASE_P(UnitTest, Init_Sys_Check,
                        testing::ValuesIn(get_sequence(Init_Sys_Check::get_number_of_cases("init_sys_functional"))));



