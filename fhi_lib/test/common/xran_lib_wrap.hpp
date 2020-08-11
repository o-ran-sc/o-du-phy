/*******************************************************************************
 *
 * <COPYRIGHT_TAG>
 *
 *******************************************************************************/

#ifndef XRAN_LIB_WRAP_HPP
#define XRAN_LIB_WRAP_HPP

#include <exception>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include <malloc.h>
#include <stdint.h>

#include "common.hpp"
#include "xran_fh_o_du.h"
#include "xran_common.h"
#include "xran_frame_struct.h"


#define XRAN_UT_CFG_FILENAME            "conf.json"

#define XRAN_UT_KEY_GLOBALCFG           "GLOBAL"
#define XRAN_UT_KEY_GLOBALCFG_IO        "io_cfg"
#define XRAN_UT_KEY_GLOBALCFG_EAXCID    "eAxCId_cfg"
#define XRAN_UT_KEY_GLOBALCFG_PRACH     "prach_cfg"
#define XRAN_UT_KEY_GLOBALCFG_RU        "ru_cfg"
#define XRAN_UT_KEY_GLOBALCFG_SLOT      "slotcfg_"

#define MAX_NUM_OF_XRAN_CTX             (2)

#define SW_FPGA_TOTAL_BUFFER_LEN        (4*1024*1024*1024)
#define SW_FPGA_SEGMENT_BUFFER_LEN      (1*1024*1024*1024)
#define SW_FPGA_FH_TOTAL_BUFFER_LEN     (1*1024*1024*1024)
#define FPGA_TO_SW_PRACH_RX_BUFFER_LEN  (8192)

#define MAX_ANT_CARRIER_SUPPORTED (XRAN_MAX_SECTOR_NR*XRAN_MAX_ANTENNA_NR)

extern "C"
{
extern uint32_t xran_lib_ota_tti;
extern uint32_t xran_lib_ota_sym;
extern uint32_t xran_lib_ota_sym_idx;

void sym_ota_cb(struct rte_timer *tim, void *arg);
void tti_ota_cb(struct rte_timer *tim, void *arg);
}

class xranLibWraper
{
public:
    typedef enum
    {
        XRANFTHTX_OUT = 0,
        XRANFTHTX_PRB_MAP_OUT,
        XRANFTHTX_SEC_DESC_OUT,
        XRANFTHRX_IN,
        XRANFTHRX_PRB_MAP_IN,
        XRANFTHTX_SEC_DESC_IN,
        XRANFTHRACH_IN,
        MAX_SW_XRAN_INTERFACE_NUM
    } SWXRANInterfaceTypeEnum;

    enum nChBw
    {
        PHY_BW_5MHZ   =   5, PHY_BW_10MHZ  =  10, PHY_BW_15MHZ  =  15,
        PHY_BW_20MHZ  =  20, PHY_BW_25MHZ  =  25, PHY_BW_30MHZ  =  30,
        PHY_BW_40MHZ  =  40, PHY_BW_50MHZ  =  50, PHY_BW_60MHZ  =  60,
        PHY_BW_70MHZ  =  70, PHY_BW_80MHZ  =  80, PHY_BW_90MHZ  =  90,
        PHY_BW_100MHZ = 100, PHY_BW_200MHZ = 200, PHY_BW_400MHZ = 400
    };

    // F1 Tables 38.101-1 Table 5.3.2-1. Maximum transmission bandwidth configuration NRB
    const uint16_t nNumRbsPerSymF1[3][13] =
    {
    //      5MHz   10MHz   15MHz   20MHz   25MHz   30MHz   40MHz   50MHz   60MHz   70MHz   80MHz   90MHz  100MHz
        {    25,     52,     79,    106,    133,    160,    216,    270,      0,      0,      0,      0,      0 },  // Numerology 0 (15KHz)
        {    11,     24,     38,     51,     65,     78,    106,    133,    162,      0,    217,    245,    273 },  // Numerology 1 (30KHz)
        {     0,     11,     18,     24,     31,     38,     51,     65,     79,      0,    107,    121,    135 }   // Numerology 2 (60KHz)
    };

    // F2 Tables 38.101-2 Table 5.3.2-1. Maximum transmission bandwidth configuration NRB
    const uint16_t nNumRbsPerSymF2[2][4] =
    {
    //     50MHz  100MHz  200MHz  400MHz
        {    66,    132,    264,      0 },  // Numerology 2 (60KHz)
        {    32,     66,    132,    264 }   // Numerology 3 (120KHz)
    };


protected:
    char argv[25] = "unittest";

    std::string m_dpdk_dev_up, m_dpdk_dev_cp, m_dpdk_bbdev;

    void *m_xranhandle;

    uint8_t m_du_mac[6] = { 0x00,0x11, 0x22, 0x33, 0x44, 0x66 };
    uint8_t m_ru_mac[6] = { 0x00,0x11, 0x22, 0x33, 0x44, 0x55 };
    bool m_bSub6;
    uint32_t m_nSlots = 10;

    struct xran_fh_config   m_xranConf;
    struct xran_fh_init     m_xranInit;

    struct xran_timer_ctx {
        uint32_t    tti_to_process;
        } m_timer_ctx[MAX_NUM_OF_XRAN_CTX];

    /* io struct */
    BbuIoBufCtrlStruct m_sFrontHaulTxBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    BbuIoBufCtrlStruct m_sFrontHaulTxPrbMapBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    BbuIoBufCtrlStruct m_sFrontHaulRxBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    BbuIoBufCtrlStruct m_sFrontHaulRxPrbMapBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    BbuIoBufCtrlStruct m_sFHPrachRxBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];

    /* Cat B */
    BbuIoBufCtrlStruct m_sFHSrsRxBbuIoBufCtrl[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANT_ARRAY_ELM_NR];

    /* buffers lists */
    struct xran_flat_buffer m_sFrontHaulTxBuffers[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_NUM_OF_SYMBOL_PER_SLOT];
    struct xran_flat_buffer m_sFrontHaulTxPrbMapBuffers[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    struct xran_flat_buffer m_sFrontHaulRxBuffers[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_NUM_OF_SYMBOL_PER_SLOT];
    struct xran_flat_buffer m_sFrontHaulRxPrbMapBuffers[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR];
    struct xran_flat_buffer m_sFHPrachRxBuffers[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_NUM_OF_SYMBOL_PER_SLOT];

    /* Cat B SRS buffers */
    struct xran_flat_buffer m_sFHSrsRxBuffers[XRAN_N_FE_BUF_LEN][XRAN_MAX_SECTOR_NR][XRAN_MAX_ANT_ARRAY_ELM_NR][XRAN_MAX_NUM_OF_SRS_SYMBOL_PER_SLOT];

    void    *m_nInstanceHandle[XRAN_PORTS_NUM][XRAN_MAX_SECTOR_NR]; // instance per sector
    uint32_t m_nBufPoolIndex[XRAN_MAX_SECTOR_NR][MAX_SW_XRAN_INTERFACE_NUM];   // every api owns unique buffer pool

    uint32_t m_nSW_ToFpga_FTH_TxBufferLen;
    uint32_t m_nFpgaToSW_FTH_RxBufferLen;

    int32_t m_nSectorIndex[XRAN_MAX_SECTOR_NR];

    int iq_bfw_buffer_size_dl = 0;
    int iq_bfw_buffer_size_ul = 0;

    /* beamforming weights for UL (O-DU) */
    int16_t *p_tx_dl_bfw_buffer[MAX_ANT_CARRIER_SUPPORTED];
    int32_t tx_dl_bfw_buffer_size[MAX_ANT_CARRIER_SUPPORTED];
    int32_t tx_dl_bfw_buffer_position[MAX_ANT_CARRIER_SUPPORTED];

    /* beamforming weights for UL (O-DU) */
    int16_t *p_tx_ul_bfw_buffer[MAX_ANT_CARRIER_SUPPORTED];
    int32_t tx_ul_bfw_buffer_size[MAX_ANT_CARRIER_SUPPORTED];
    int32_t tx_ul_bfw_buffer_position[MAX_ANT_CARRIER_SUPPORTED];


private:
    json m_global_cfg;

    template<typename T>
    T get_globalcfg(const std::string &type, const std::string &parameter_name)
    {
        return m_global_cfg[XRAN_UT_KEY_GLOBALCFG][type][parameter_name];
    }

    template<typename T>
    std::vector<T> get_globalcfg_array(const std::string &type, const std::string &parameter_name)
    {
        auto array_size = m_global_cfg[XRAN_UT_KEY_GLOBALCFG][type][parameter_name].size();

        std::vector<T> result(array_size);

        for(unsigned number = 0; number < array_size; number++)
            result.at(number) = m_global_cfg[XRAN_UT_KEY_GLOBALCFG][type][parameter_name][number];

        return result;
    }

    uint16_t get_eaxcid_mask(int numbit, int shift)
    {
        uint16_t result = 0;

        for(int i=0; i < numbit; i++) {
            result = result << 1; result +=1;
            }
        return (result << shift);
    }

    int init_memory()
    {
        xran_status_t status;
        int32_t i, j, k, z;
        SWXRANInterfaceTypeEnum eInterfaceType;
        void *ptr;
        void *mb;
        uint32_t *u32dptr;
        uint16_t *u16dptr;
        uint8_t  *u8dptr;

        uint32_t xran_max_antenna_nr = RTE_MAX(get_num_eaxc(), get_num_eaxc_ul());
        uint32_t xran_max_ant_array_elm_nr = RTE_MAX(get_num_antelmtrx(), xran_max_antenna_nr);


        std::cout << "XRAN front haul xran_mm_init" << std::endl;
        status = xran_mm_init(m_xranhandle, (uint64_t) SW_FPGA_FH_TOTAL_BUFFER_LEN, SW_FPGA_SEGMENT_BUFFER_LEN);
        if(status != XRAN_STATUS_SUCCESS) {
            std::cout << "Failed at XRAN front haul xran_mm_init" << std::endl;
            return (-1);
            }

        /* initialize maximum instances to have flexibility for the tests */
        int nInstanceNum = XRAN_MAX_SECTOR_NR;
        /* initialize maximum supported CC to have flexibility on the test */
        int32_t nSectorNum = 6;//XRAN_MAX_SECTOR_NR;

        for(k = 0; k < XRAN_PORTS_NUM; k++) {
            status = xran_sector_get_instances(m_xranhandle, nInstanceNum, &m_nInstanceHandle[k][0]);
            if(status != XRAN_STATUS_SUCCESS) {
                std::cout  << "get sector instance failed " << k << " for XRAN nInstanceNum " << nInstanceNum << std::endl;
                return (-1);
                }
            for (i = 0; i < nInstanceNum; i++)
                std::cout << __func__ << " [" << k << "]: CC " << i << " handle " << m_nInstanceHandle[0][i] << std::endl;
            }
        std::cout << "Sucess xran_mm_init" << std::endl;

        /* Init Memory */
        for(i = 0; i<nSectorNum; i++) {
            eInterfaceType = XRANFTHTX_OUT;
            status = xran_bm_init(m_nInstanceHandle[0][i],
                            &m_nBufPoolIndex[m_nSectorIndex[i]][eInterfaceType],
                            XRAN_N_FE_BUF_LEN * xran_max_antenna_nr * XRAN_NUM_OF_SYMBOL_PER_SLOT,
                            m_nSW_ToFpga_FTH_TxBufferLen);
            if(status != XRAN_STATUS_SUCCESS) {
                std::cout << __LINE__ << " Failed at xran_bm_init, status " << status << std::endl;
                return (-1);
                }
            for(j = 0; j < XRAN_N_FE_BUF_LEN; j++) {
                for(z = 0; z < xran_max_antenna_nr; z++){
                    m_sFrontHaulTxBbuIoBufCtrl[j][i][z].bValid = 0;
                    m_sFrontHaulTxBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
                    m_sFrontHaulTxBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
                    m_sFrontHaulTxBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
                    m_sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
                    m_sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &m_sFrontHaulTxBuffers[j][i][z][0];

                    for(k = 0; k < XRAN_NUM_OF_SYMBOL_PER_SLOT; k++) {
                        m_sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nElementLenInBytes = m_nSW_ToFpga_FTH_TxBufferLen; // 14 symbols 3200bytes/symbol
                        m_sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nNumberOfElements = 1;
                        m_sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nOffsetInBytes = 0;
                        status = xran_bm_allocate_buffer(m_nInstanceHandle[0][i], m_nBufPoolIndex[m_nSectorIndex[i]][eInterfaceType], &ptr, &mb);
                        if(status != XRAN_STATUS_SUCCESS) {
                            std::cout << __LINE__ << " Failed at  xran_bm_allocate_buffer, status " << status << std::endl;
                            return (-1);
                            }
                        m_sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pData = (uint8_t *)ptr;
                        m_sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pCtrl = (void *)mb;

                        if(ptr) {
                            u32dptr = (uint32_t*)(ptr);
                            uint8_t *ptr_temp = (uint8_t *)ptr;
                            memset(u32dptr, 0x0, m_nSW_ToFpga_FTH_TxBufferLen);
                            }
                        }
                    }
                }

            /* C-plane DL */
            eInterfaceType = XRANFTHTX_SEC_DESC_OUT;
            status = xran_bm_init(m_nInstanceHandle[0][i],
                            &m_nBufPoolIndex[m_nSectorIndex[i]][eInterfaceType],
                            XRAN_N_FE_BUF_LEN * xran_max_antenna_nr * XRAN_NUM_OF_SYMBOL_PER_SLOT*XRAN_MAX_SECTIONS_PER_SYM, sizeof(struct xran_section_desc));
            if(XRAN_STATUS_SUCCESS != status) {
                std::cout << __LINE__ << " Failed at xran_bm_init, status " << status << std::endl;
                return (-1);
            }
            eInterfaceType = XRANFTHTX_PRB_MAP_OUT;
            status = xran_bm_init(m_nInstanceHandle[0][i],
                            &m_nBufPoolIndex[m_nSectorIndex[i]][eInterfaceType],
                            XRAN_N_FE_BUF_LEN * xran_max_antenna_nr * XRAN_NUM_OF_SYMBOL_PER_SLOT,
                            sizeof(struct xran_prb_map));
            if(status != XRAN_STATUS_SUCCESS) {
                std::cout << __LINE__ << " Failed at xran_bm_init, status " << status << std::endl;
                return (-1);
            }
            for(j = 0; j < XRAN_N_FE_BUF_LEN; j++) {
                for(z = 0; z < xran_max_antenna_nr; z++) {
                    m_sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].bValid = 0;
                    m_sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].nSegGenerated = -1;
                    m_sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].nSegToBeGen = -1;
                    m_sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].nSegTransferred = 0;
                    m_sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
                    m_sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers = &m_sFrontHaulTxPrbMapBuffers[j][i][z];

                    m_sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nElementLenInBytes = sizeof(struct xran_prb_map);
                    m_sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nNumberOfElements = 1;
                    m_sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nOffsetInBytes = 0;
                    status = xran_bm_allocate_buffer(m_nInstanceHandle[0][i], m_nBufPoolIndex[m_nSectorIndex[i]][eInterfaceType], &ptr, &mb);
                    if(status != XRAN_STATUS_SUCCESS) {
                        std::cout << __LINE__ << " Failed at xran_bm_allocate_buffer, status " << status << std::endl;
                        return (-1);
                        }
                    m_sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->pData = (uint8_t *)ptr;
                    m_sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->pCtrl = (void *)mb;
                    void *sd_ptr;
                    void *sd_mb;
                    int  elm_id;
                    struct xran_prb_map * p_rb_map = (struct xran_prb_map *)ptr;
                    //memcpy(ptr, &startupConfiguration.PrbMap, sizeof(struct xran_prb_map));
                    for (elm_id = 0; elm_id < XRAN_MAX_SECTIONS_PER_SYM; elm_id++){
                        struct xran_prb_elm *pPrbElem = &p_rb_map->prbMap[elm_id];
                        for(k = 0; k < XRAN_NUM_OF_SYMBOL_PER_SLOT; k++){
                            status = xran_bm_allocate_buffer(m_nInstanceHandle[0][i], m_nBufPoolIndex[m_nSectorIndex[i]][XRANFTHTX_SEC_DESC_OUT], &sd_ptr, &sd_mb);
                            if(XRAN_STATUS_SUCCESS != status){
                                std::cout << __LINE__ << "SD Failed at  xran_bm_allocate_buffer , status %d\n" << status << std::endl;
                                return (-1);
                            }
                            pPrbElem->p_sec_desc[k] = (struct xran_section_desc *)sd_ptr;
                        }
                    }
                 }
             }
        }

        for(i = 0; i<nSectorNum; i++) {
            eInterfaceType = XRANFTHRX_IN;
            status = xran_bm_init(m_nInstanceHandle[0][i],
                            &m_nBufPoolIndex[m_nSectorIndex[i]][eInterfaceType],
                            XRAN_N_FE_BUF_LEN * xran_max_antenna_nr * XRAN_NUM_OF_SYMBOL_PER_SLOT,
                            m_nSW_ToFpga_FTH_TxBufferLen);  /* ????, actual alloc size is m_nFpgaToSW_FTH_RxBUfferLen */
            if(status != XRAN_STATUS_SUCCESS) {
                std::cout << __LINE__ << " Failed at xran_bm_init, status " << status << std::endl;
                return (-1);
                }

            for(j = 0;j < XRAN_N_FE_BUF_LEN; j++) {
                for(z = 0; z < xran_max_antenna_nr; z++) {
                    m_sFrontHaulRxBbuIoBufCtrl[j][i][z].bValid                  = 0;
                    m_sFrontHaulRxBbuIoBufCtrl[j][i][z].nSegGenerated           = -1;
                    m_sFrontHaulRxBbuIoBufCtrl[j][i][z].nSegToBeGen             = -1;
                    m_sFrontHaulRxBbuIoBufCtrl[j][i][z].nSegTransferred         = 0;
                    m_sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers = XRAN_NUM_OF_SYMBOL_PER_SLOT;
                    m_sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers    = &m_sFrontHaulRxBuffers[j][i][z][0];
                    for(k = 0; k< XRAN_NUM_OF_SYMBOL_PER_SLOT; k++) {
                        m_sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nElementLenInBytes  = m_nFpgaToSW_FTH_RxBufferLen;
                        m_sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nNumberOfElements   = 1;
                        m_sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nOffsetInBytes      = 0;
                        status = xran_bm_allocate_buffer(m_nInstanceHandle[0][i], m_nBufPoolIndex[m_nSectorIndex[i]][eInterfaceType],&ptr, &mb);
                        if(status != XRAN_STATUS_SUCCESS) {
                            std::cout << __LINE__ << " Failed at  xran_bm_allocate_buffer, status " << status << std::endl;
                            return (-1);
                            }
                        m_sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pData   = (uint8_t *)ptr;
                        m_sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pCtrl   = (void *) mb;
                        if(ptr) {
                            u32dptr = (uint32_t*)(ptr);
                            uint8_t *ptr_temp = (uint8_t *)ptr;
                            memset(u32dptr, 0x0, m_nFpgaToSW_FTH_RxBufferLen);
                            }
                        }
                    }
                }

            eInterfaceType = XRANFTHTX_SEC_DESC_IN;
            status = xran_bm_init(m_nInstanceHandle[0][i],
                            &m_nBufPoolIndex[m_nSectorIndex[i]][eInterfaceType],
                            XRAN_N_FE_BUF_LEN * xran_max_antenna_nr * XRAN_NUM_OF_SYMBOL_PER_SLOT*XRAN_MAX_SECTIONS_PER_SYM, sizeof(struct xran_section_desc));
            if(XRAN_STATUS_SUCCESS != status) {
                std::cout << __LINE__ << " Failed at xran_bm_init, status " << status << std::endl;
                return (-1);
            }
            eInterfaceType = XRANFTHRX_PRB_MAP_IN;
            status = xran_bm_init(m_nInstanceHandle[0][i],
                                &m_nBufPoolIndex[m_nSectorIndex[i]][eInterfaceType],
                                XRAN_N_FE_BUF_LEN * xran_max_antenna_nr * XRAN_NUM_OF_SYMBOL_PER_SLOT,
                                sizeof(struct xran_prb_map));
            if(status != XRAN_STATUS_SUCCESS) {
                std::cout << __LINE__ << " Failed at xran_bm_init, status " << status << std::endl;
                return (-1);
            }

            for(j = 0;j < XRAN_N_FE_BUF_LEN; j++) {
                for(z = 0; z < xran_max_antenna_nr; z++) {
                    m_sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].bValid                    = 0;
                    m_sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].nSegGenerated             = -1;
                    m_sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].nSegToBeGen               = -1;
                    m_sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].nSegTransferred           = 0;
                    m_sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers   = XRAN_NUM_OF_SYMBOL_PER_SLOT;
                    m_sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers      = &m_sFrontHaulRxPrbMapBuffers[j][i][z];

                    m_sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nElementLenInBytes  = sizeof(struct xran_prb_map);
                    m_sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nNumberOfElements   = 1;
                    m_sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->nOffsetInBytes      = 0;
                    status = xran_bm_allocate_buffer(m_nInstanceHandle[0][i],m_nBufPoolIndex[m_nSectorIndex[i]][eInterfaceType], &ptr, &mb);
                    if(status != XRAN_STATUS_SUCCESS) {
                        std::cout << __LINE__ << " Failed at  xran_bm_allocate_buffer , status " << status << std::endl;
                        return (-1);
                        }
                    m_sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->pData   = (uint8_t *)ptr;
                    m_sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList.pBuffers->pCtrl   = (void *)mb;
                    void *sd_ptr;
                    void *sd_mb;
                    int  elm_id;
                    struct xran_prb_map * p_rb_map = (struct xran_prb_map *)ptr;
                    //memcpy(ptr, &startupConfiguration.PrbMap, sizeof(struct xran_prb_map));
                    for (elm_id = 0; elm_id < XRAN_MAX_SECTIONS_PER_SYM; elm_id++){
                        struct xran_prb_elm *pPrbElem = &p_rb_map->prbMap[elm_id];
                        for(k = 0; k < XRAN_NUM_OF_SYMBOL_PER_SLOT; k++){
                            status = xran_bm_allocate_buffer(m_nInstanceHandle[0][i], m_nBufPoolIndex[m_nSectorIndex[i]][XRANFTHTX_SEC_DESC_IN], &sd_ptr, &sd_mb);
                            if(XRAN_STATUS_SUCCESS != status){
                                std::cout << __LINE__ << "SD Failed at  xran_bm_allocate_buffer , status %d\n" << status << std::endl;
                                return (-1);
                            }
                            pPrbElem->p_sec_desc[k] = (struct xran_section_desc *)sd_ptr;
                        }
                    }
                }
            }
        }

        for(i = 0; i<nSectorNum; i++) {
            eInterfaceType = XRANFTHRACH_IN;
            status = xran_bm_init(m_nInstanceHandle[0][i],
                                &m_nBufPoolIndex[m_nSectorIndex[i]][eInterfaceType],
                                XRAN_N_FE_BUF_LEN * xran_max_antenna_nr * XRAN_NUM_OF_SYMBOL_PER_SLOT,
                                FPGA_TO_SW_PRACH_RX_BUFFER_LEN);
            if(status != XRAN_STATUS_SUCCESS) {
                std::cout << __LINE__ << " Failed at xran_bm_init, status " << status << std::endl;
                return (-1);
                }
            for(j = 0; j < XRAN_N_FE_BUF_LEN; j++) {
                for(z = 0; z < xran_max_antenna_nr; z++) {
                    m_sFHPrachRxBbuIoBufCtrl[j][i][z].bValid                    = 0;
                    m_sFHPrachRxBbuIoBufCtrl[j][i][z].nSegGenerated             = -1;
                    m_sFHPrachRxBbuIoBufCtrl[j][i][z].nSegToBeGen               = -1;
                    m_sFHPrachRxBbuIoBufCtrl[j][i][z].nSegTransferred           = 0;
                    m_sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.nNumBuffers   = xran_max_antenna_nr;
                    m_sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers      = &m_sFHPrachRxBuffers[j][i][z][0];
                    for(k = 0; k< XRAN_NUM_OF_SYMBOL_PER_SLOT; k++) {
                        m_sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nElementLenInBytes    = FPGA_TO_SW_PRACH_RX_BUFFER_LEN;
                        m_sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nNumberOfElements     = 1;
                        m_sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].nOffsetInBytes        = 0;
                        status = xran_bm_allocate_buffer(m_nInstanceHandle[0][i], m_nBufPoolIndex[m_nSectorIndex[i]][eInterfaceType], &ptr, &mb);
                        if(status != XRAN_STATUS_SUCCESS) {
                            std::cout << __LINE__ << " Failed at  xran_bm_allocate_buffer, status " << status << std::endl;
                            return (-1);
                            }
                        m_sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pData = (uint8_t *)ptr;
                        m_sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList.pBuffers[k].pCtrl = (void *)mb;
                        if(ptr) {
                            u32dptr = (uint32_t*)(ptr);
                            memset(u32dptr, 0x0, FPGA_TO_SW_PRACH_RX_BUFFER_LEN);
                            }
                        }
                    }
                }
            }

        return (0);
    }


public:
    xranLibWraper()
    {
        int i, temp;
        std::string tmpstr;
        unsigned int tmp_mac[6];

        m_global_cfg = read_json_from_file(XRAN_UT_CFG_FILENAME);

        memset(&m_xranInit, 0, sizeof(xran_fh_init));

        m_xranInit.io_cfg.id  = 0;

        /* DPDK configuration */
        m_dpdk_dev_up = get_globalcfg<std::string>(XRAN_UT_KEY_GLOBALCFG_IO, "dpdk_dev_up");
        m_dpdk_dev_cp = get_globalcfg<std::string>(XRAN_UT_KEY_GLOBALCFG_IO, "dpdk_dev_cp");
        m_xranInit.io_cfg.num_vfs = 2;
        m_xranInit.io_cfg.dpdk_dev[XRAN_UP_VF]  = (m_dpdk_dev_up == "") ? NULL : (char *)&m_dpdk_dev_up;
        m_xranInit.io_cfg.dpdk_dev[XRAN_CP_VF]  = (m_dpdk_dev_cp == "") ? NULL : (char *)&m_dpdk_dev_cp;

        m_xranInit.io_cfg.core              = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_IO, "core");
        m_xranInit.io_cfg.system_core       = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_IO, "system_core");
        m_xranInit.io_cfg.pkt_proc_core     = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_IO, "pkt_proc_core");
        m_xranInit.io_cfg.pkt_aux_core      = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_IO, "pkt_aux_core");
        m_xranInit.io_cfg.timing_core       = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_IO, "timing_core");

        std::string bbdev_mode = get_globalcfg<std::string>(XRAN_UT_KEY_GLOBALCFG_IO, "bbdev_mode");
        if(bbdev_mode == "sw")
            m_xranInit.io_cfg.bbdev_mode    = XRAN_BBDEV_MODE_HW_OFF;
        else if(bbdev_mode == "hw")
            m_xranInit.io_cfg.bbdev_mode    = XRAN_BBDEV_MODE_HW_ON;
        else if(bbdev_mode == "none")
            m_xranInit.io_cfg.bbdev_mode    = XRAN_BBDEV_NOT_USED;
        else {
            std::cout << "Invalid BBDev mode [" << bbdev_mode << "], bbdev won't be used." << std::endl;
            m_xranInit.io_cfg.bbdev_mode    = XRAN_BBDEV_NOT_USED;
            }

        m_xranInit.dpdkBasebandFecMode      = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_IO, "dpdkBasebandFecMode");

        m_dpdk_bbdev = get_globalcfg<std::string>(XRAN_UT_KEY_GLOBALCFG_IO, "dpdkBasebandDevice");
        m_xranInit.dpdkBasebandDevice       = (m_dpdk_bbdev == "") ? NULL : (char *)&m_dpdk_bbdev;

        /* Network configurations */
        m_xranInit.mtu          = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_IO, "mtu");

        std::string du_mac_str = get_globalcfg<std::string>(XRAN_UT_KEY_GLOBALCFG_IO, "o_du_macaddr");
        std::string ru_mac_str = get_globalcfg<std::string>(XRAN_UT_KEY_GLOBALCFG_IO, "o_ru_macaddr");
        /* using temp variables to resolve KW issue */
        std::sscanf(du_mac_str.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x",
                                           &tmp_mac[0], &tmp_mac[1], &tmp_mac[2],
                                           &tmp_mac[3], &tmp_mac[4], &tmp_mac[5]);
        for(i=0; i<6; i++)
            m_du_mac[i] = (uint8_t)tmp_mac[i];
        std::sscanf(du_mac_str.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x",
                                           &tmp_mac[0], &tmp_mac[1], &tmp_mac[2],
                                           &tmp_mac[3], &tmp_mac[4], &tmp_mac[5]);
        for(i=0; i<6; i++)
            m_ru_mac[i] = (uint8_t)tmp_mac[i];
        m_xranInit.p_o_du_addr  = (int8_t *)m_du_mac;
        m_xranInit.p_o_ru_addr  = (int8_t *)m_ru_mac;
        m_xranInit.cp_vlan_tag  = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_IO, "cp_vlan_tag");
        m_xranInit.up_vlan_tag  = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_IO, "up_vlan_tag");

        /* eAxCID configurations */
        int bitnum_cuport   = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_EAXCID, "bit_cuPortId");
        int bitnum_bandsec  = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_EAXCID, "bit_bandSectorId");
        int bitnum_ccid     = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_EAXCID, "bit_ccId");
        int bitnum_ruport   = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_EAXCID, "bit_ruPortId");

        m_xranInit.eAxCId_conf.bit_cuPortId       = bitnum_bandsec + bitnum_ccid + bitnum_ruport;
        m_xranInit.eAxCId_conf.bit_bandSectorId   = bitnum_ccid + bitnum_ruport;
        m_xranInit.eAxCId_conf.bit_ccId           = bitnum_ruport;
        m_xranInit.eAxCId_conf.bit_ruPortId       = 0;
        m_xranInit.eAxCId_conf.mask_cuPortId      = get_eaxcid_mask(bitnum_cuport, m_xranInit.eAxCId_conf.bit_cuPortId);
        m_xranInit.eAxCId_conf.mask_bandSectorId  = get_eaxcid_mask(bitnum_bandsec, m_xranInit.eAxCId_conf.bit_bandSectorId);
        m_xranInit.eAxCId_conf.mask_ccId          = get_eaxcid_mask(bitnum_ccid, m_xranInit.eAxCId_conf.bit_ccId);
        m_xranInit.eAxCId_conf.mask_ruPortId      = get_eaxcid_mask(bitnum_ruport, m_xranInit.eAxCId_conf.bit_ruPortId);

        m_xranInit.totalBfWeights   = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "totalBfWeights");

        m_xranInit.Tadv_cp_dl       = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "Tadv_cp_dl");
        m_xranInit.T2a_min_cp_dl    = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "T2a_min_cp_dl");
        m_xranInit.T2a_max_cp_dl    = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "T2a_max_cp_dl");
        m_xranInit.T2a_min_cp_ul    = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "T2a_min_cp_ul");
        m_xranInit.T2a_max_cp_ul    = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "T2a_max_cp_ul");
        m_xranInit.T2a_min_up       = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "T2a_min_up");
        m_xranInit.T2a_max_up       = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "T2a_max_up");
        m_xranInit.Ta3_min          = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "Ta3_min");
        m_xranInit.Ta3_max          = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "Ta3_max");
        m_xranInit.T1a_min_cp_dl    = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "T1a_min_cp_dl");
        m_xranInit.T1a_max_cp_dl    = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "T1a_max_cp_dl");
        m_xranInit.T1a_min_cp_ul    = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "T1a_min_cp_ul");
        m_xranInit.T1a_max_cp_ul    = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "T1a_max_cp_ul");
        m_xranInit.T1a_min_up       = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "T1a_min_up");
        m_xranInit.T1a_max_up       = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "T1a_max_up");
        m_xranInit.Ta4_min          = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "Ta4_min");
        m_xranInit.Ta4_max          = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "Ta4_max");

        m_xranInit.enableCP         = 1;
        m_xranInit.prachEnable      = 1;
        m_xranInit.debugStop        = 0;
        m_xranInit.debugStopCount   = 0;
        m_xranInit.DynamicSectionEna= 0;

        m_xranInit.filePrefix   = "wls";

        m_bSub6     = get_globalcfg<bool>(XRAN_UT_KEY_GLOBALCFG_RU, "sub6");

        memset(&m_xranConf, 0, sizeof(struct xran_fh_config));
        tmpstr = get_globalcfg<std::string>(XRAN_UT_KEY_GLOBALCFG_RU, "duplex");
        if(tmpstr == "FDD") {
            m_xranConf.frame_conf.nFrameDuplexType  = 0;
            }
        else if(tmpstr == "TDD") {
            m_xranConf.frame_conf.nFrameDuplexType  = 1;

            std::string slotcfg_key = get_globalcfg<std::string>(XRAN_UT_KEY_GLOBALCFG_RU, "slot_config");

            int numcfg = get_globalcfg<int>(slotcfg_key, "period");
            m_xranConf.frame_conf.nTddPeriod = numcfg;

            for(int i=0; i< numcfg; i++) {
                std::stringstream slotcfgname;
                slotcfgname << "slot" << i;
                std::vector<int> slotcfg = get_globalcfg_array<int>(slotcfg_key, slotcfgname.str());
                for(int j=0; j < slotcfg.size(); j++) {
                    m_xranConf.frame_conf.sSlotConfig[i].nSymbolType[j] = slotcfg[j];
                    }
                m_xranConf.frame_conf.sSlotConfig[i].reserved[0] = 0;
                m_xranConf.frame_conf.sSlotConfig[i].reserved[1] = 0;
                }
            }
        else {
            std::cout << "*** Invalid Duplex type [" << tmpstr << "] !!!" << std::endl;
            std::cout << "****** Set it to FDD... " << std::endl;
            m_xranConf.frame_conf.nFrameDuplexType  = 0;
            }

        m_xranConf.frame_conf.nNumerology = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "mu");
        if(m_xranConf.frame_conf.nNumerology > 3) {
            std::cout << "*** Invalid Numerology [" << m_xranConf.frame_conf.nNumerology << "] !!!" << std::endl;
            m_xranConf.frame_conf.nNumerology   = 0;
            std::cout << "****** Set it to " << m_xranConf.frame_conf.nNumerology << "..." << std::endl;
            }

        m_xranConf.nCC = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "num_cc");
        if(m_xranConf.nCC > XRAN_MAX_SECTOR_NR) {
            std::cout << "*** Exceeds maximum number of carriers supported [" << m_xranConf.nCC << "] !!!" << std::endl;
            m_xranConf.nCC = XRAN_MAX_SECTOR_NR;
            std::cout << "****** Adjusted to " << m_xranConf.nCC << "..." << std::endl;
            }
        m_xranConf.neAxc = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "num_eaxc");
        if(m_xranConf.neAxc > XRAN_MAX_ANTENNA_NR) {
            std::cout << "*** Exceeds maximum number of antenna supported [" << m_xranConf.neAxc << "] !!!" << std::endl;
            m_xranConf.neAxc = XRAN_MAX_ANTENNA_NR;
            std::cout << "****** Adjusted to " << m_xranConf.neAxc << "..." << std::endl;
            }

        m_bSub6     = get_globalcfg<bool>(XRAN_UT_KEY_GLOBALCFG_RU, "sub6");
        temp = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "chbw_dl");
        m_xranConf.nDLRBs = get_num_rbs(get_numerology(), temp, m_bSub6);
        temp = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "chbw_ul");
        m_xranConf.nULRBs = get_num_rbs(get_numerology(), temp, m_bSub6);

        m_xranConf.nAntElmTRx = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "ant_elm_trx");
        m_xranConf.nDLFftSize = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "fft_size");
        m_xranConf.nULFftSize = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "fft_size");

        m_xranConf.prach_conf.nPrachConfIdx     = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_PRACH, "config_id");
        m_xranConf.prach_conf.nPrachSubcSpacing = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_PRACH, "scs");
        m_xranConf.prach_conf.nPrachFreqStart   = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_PRACH, "freq_start");
        m_xranConf.prach_conf.nPrachFreqOffset  = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_PRACH, "freq_offset");
        m_xranConf.prach_conf.nPrachFilterIdx   = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_PRACH, "filter_id");
        m_xranConf.prach_conf.nPrachZeroCorrConf= 0;
        m_xranConf.prach_conf.nPrachRestrictSet = 0;
        m_xranConf.prach_conf.nPrachRootSeqIdx  = 0;

        tmpstr = get_globalcfg<std::string>(XRAN_UT_KEY_GLOBALCFG_RU, "category");
        if(tmpstr == "A")
            m_xranConf.ru_conf.xranCat = XRAN_CATEGORY_A;
        else if(tmpstr == "B")
            m_xranConf.ru_conf.xranCat = XRAN_CATEGORY_B;
        else {
            std::cout << "*** Invalid RU Category [" << tmpstr << "] !!!" << std::endl;
            std::cout << "****** Set it to Category A... " << std::endl;
            m_xranConf.ru_conf.xranCat = XRAN_CATEGORY_A;
            }

        m_xranConf.ru_conf.iqWidth  = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "iq_width");
        m_xranConf.ru_conf.compMeth = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "comp_meth");

        temp = get_globalcfg<int>(XRAN_UT_KEY_GLOBALCFG_RU, "fft_size");
        m_xranConf.ru_conf.fftSize  = 0;
        while (temp >>= 1)
            ++m_xranConf.ru_conf.fftSize;

        m_xranConf.ru_conf.byteOrder    =  XRAN_NE_BE_BYTE_ORDER;
        m_xranConf.ru_conf.iqOrder      =  XRAN_I_Q_ORDER;

        m_xranConf.log_level    = 0;
/*
        m_xranConf.bbdev_enc = nullptr;
        m_xranConf.bbdev_dec = nullptr;
        m_xranConf.ttiCb    = nullptr;
        m_xranConf.ttiCbParam   = nullptr;
*/
    }

    ~xranLibWraper()
    {
    }

    int SetUp()
    {
        int i;

        printf("O-DU MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
            m_xranInit.p_o_du_addr[0],
            m_xranInit.p_o_du_addr[1],
            m_xranInit.p_o_du_addr[2],
            m_xranInit.p_o_du_addr[3],
            m_xranInit.p_o_du_addr[4],
            m_xranInit.p_o_du_addr[5]);

        printf("O-RU MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
            m_xranInit.p_o_ru_addr[0],
            m_xranInit.p_o_ru_addr[1],
            m_xranInit.p_o_ru_addr[2],
            m_xranInit.p_o_ru_addr[3],
            m_xranInit.p_o_ru_addr[4],
            m_xranInit.p_o_ru_addr[5]);

        printf("eAxCID - %d:%d:%d:%d (%04x, %04x, %04x, %04x)\n",
            m_xranInit.eAxCId_conf.bit_cuPortId,
            m_xranInit.eAxCId_conf.bit_bandSectorId,
            m_xranInit.eAxCId_conf.bit_ccId,
            m_xranInit.eAxCId_conf.bit_ruPortId,
            m_xranInit.eAxCId_conf.mask_cuPortId,
            m_xranInit.eAxCId_conf.mask_bandSectorId,
            m_xranInit.eAxCId_conf.mask_ccId,
            m_xranInit.eAxCId_conf.mask_ruPortId);

        printf("Total BF Weights : %d\n", m_xranInit.totalBfWeights);

        xran_init(0, NULL, &m_xranInit, &argv[0], &m_xranhandle);

        for(i = 0; i < XRAN_MAX_SECTOR_NR; i++)
            m_nSectorIndex[i] = i;

        /* set to maximum length to support multiple cases */
        m_nFpgaToSW_FTH_RxBufferLen     = 13168; /* 273*12*4 + 64*/
        m_nSW_ToFpga_FTH_TxBufferLen    = 13168; /* 273*12*4 + 64*/

        if(init_memory() < 0) {
            std::cout << "Fatal Error on Initialization !!!" << std::endl;
            std::cout << "INIT FAILED" << std::endl;
            return (-1);
            }

        std::cout << "INIT DONE" << std::endl;
        return (0);
    }

    void TearDown()
    {
        if(m_xranhandle) {
            xran_close(m_xranhandle);
            m_xranhandle = nullptr;
            std::cout << "CLOSE DONE" << std::endl;
            }
        else
            std::cout << "ALREADY CLOSED" << std::endl;
    }

    int Init(struct xran_fh_config *pCfg = nullptr)
    {
        xran_status_t status;
        int32_t nSectorNum;
        int32_t i, j, k, z;
        void *ptr;
        void *mb;
        uint32_t *u32dptr;
        uint16_t *u16dptr;
        uint8_t  *u8dptr;
        SWXRANInterfaceTypeEnum eInterfaceType;
        int32_t cc_id, ant_id, sym_id, tti;
        int32_t flowId;
        char    *pos        = NULL;
        struct xran_prb_map *pRbMap = NULL;


        uint32_t xran_max_antenna_nr = RTE_MAX(get_num_eaxc(), get_num_eaxc_ul());
        uint32_t xran_max_ant_array_elm_nr = RTE_MAX(get_num_antelmtrx(), xran_max_antenna_nr);


        /* Update member variables */
        if(pCfg)
            memcpy(&m_xranConf, pCfg, sizeof(struct xran_fh_config));

        /* Init timer context */
        xran_lib_ota_tti        = 0;
        xran_lib_ota_sym        = 0;
        xran_lib_ota_sym_idx    = 0;
        for(i=0; i < MAX_NUM_OF_XRAN_CTX; i++)
            m_timer_ctx[i].tti_to_process = i;

        nSectorNum = get_num_cc();

        /* Cat B RU support */
        if(get_rucategory() == XRAN_CATEGORY_B) {
            /* 10 * [14*32*273*2*2] = 4892160 bytes */
            iq_bfw_buffer_size_dl = (m_nSlots * N_SYM_PER_SLOT * get_num_antelmtrx() * get_num_dlrbs() * 4L);
            iq_bfw_buffer_size_ul = (m_nSlots * N_SYM_PER_SLOT * get_num_antelmtrx() * get_num_ulrbs() * 4L);

            for(i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(get_num_cc() * get_num_eaxc()); i++) {
                p_tx_dl_bfw_buffer[i]   = (int16_t*)malloc(iq_bfw_buffer_size_dl);
                tx_dl_bfw_buffer_size[i] = (int32_t)iq_bfw_buffer_size_dl;
                if(p_tx_dl_bfw_buffer[i] == NULL)
                    return(-1);

                memset(p_tx_dl_bfw_buffer[i], 'D', iq_bfw_buffer_size_dl);
                tx_dl_bfw_buffer_position[i] = 0;

                p_tx_ul_bfw_buffer[i]    = (int16_t*)malloc(iq_bfw_buffer_size_ul);
                tx_ul_bfw_buffer_size[i] = (int32_t)iq_bfw_buffer_size_ul;
                if(p_tx_ul_bfw_buffer[i] == NULL)
                    return (-1);

                memset(p_tx_ul_bfw_buffer[i], 'U', iq_bfw_buffer_size_ul);
                tx_ul_bfw_buffer_position[i] = 0;
            }
        }

        /* Init RB map */
        for(cc_id = 0; cc_id <nSectorNum; cc_id++) {
            for(tti  = 0; tti  < XRAN_N_FE_BUF_LEN; tti ++) {
                for(ant_id = 0; ant_id < xran_max_antenna_nr; ant_id++) {
                    flowId = xran_max_antenna_nr*cc_id + ant_id;

                    /* C-plane DL */
                    pRbMap = (struct xran_prb_map *)m_sFrontHaulTxPrbMapBbuIoBufCtrl[tti][cc_id][ant_id].sBufferList.pBuffers->pData;
                    if(pRbMap) {
                        pRbMap->dir                     = XRAN_DIR_DL;
                        pRbMap->xran_port               = 0;
                        pRbMap->band_id                 = 0;
                        pRbMap->cc_id                   = cc_id;
                        pRbMap->ru_port_id              = ant_id;
                        pRbMap->tti_id                  = tti;
                        pRbMap->start_sym_id            = 0;

                        pRbMap->nPrbElm                 = 1;
                        pRbMap->prbMap[0].nRBStart      = 0;
                        pRbMap->prbMap[0].nRBSize       = get_num_dlrbs();
                        pRbMap->prbMap[0].nStartSymb    = 0;
                        pRbMap->prbMap[0].numSymb       = 14;
                        pRbMap->prbMap[0].nBeamIndex    = 0;
                        pRbMap->prbMap[0].compMethod    = XRAN_COMPMETHOD_NONE;

                        if(get_rucategory() == XRAN_CATEGORY_A) {
                            pRbMap->prbMap[0].BeamFormingType   = XRAN_BEAM_ID_BASED;
                            pRbMap->prbMap[0].bf_weight_update  = 0;
                            //pRbMap->prbMap[0].bf_attribute.weight[];
                            //pRbMap->prbMap[0].bf_precoding.weight[];
                            }
                        else if(get_rucategory() == XRAN_CATEGORY_B) {
                            int idxElm;
                            int iPrb;
                            char *dl_bfw_pos = ((char*)p_tx_dl_bfw_buffer[flowId]) + tx_dl_bfw_buffer_position[flowId];
                            struct xran_prb_elm* p_prbMap = NULL;
                            int num_antelm;

                            pRbMap->prbMap[0].BeamFormingType   = XRAN_BEAM_WEIGHT;
                            pRbMap->prbMap[0].bf_weight_update  = 1;

                            num_antelm = get_num_antelmtrx();
#if 0
                            /* populate beam weights to C-plane for each elm */
                            pRbMap->bf_weight.nAntElmTRx = num_antelm;
                            for(idxElm = 0;  idxElm < pRbMap->nPrbElm; idxElm++){
                                p_prbMap = &pRbMap->prbMap[idxElm];
                                for (iPrb = p_prbMap->nRBStart; iPrb < (p_prbMap->nRBStart + p_prbMap->nRBSize); iPrb++) {
                                    /* copy BF W IQs for 1 PRB of */
                                    rte_memcpy(&pRbMap->bf_weight.weight[iPrb][0], (dl_bfw_pos + (iPrb * num_antelm)*4), num_antelm*4);
                                    }
                                }
#endif
                            } /* else if(get_rucategory() == XRAN_CATEGORY_B) */
                        } /* if(pRbMap) */
                    else {
                        std::cout << "DL pRbMap ==NULL" << std::endl;
                        }

                    /* C-plane UL */
                    pRbMap = (struct xran_prb_map *)m_sFrontHaulRxPrbMapBbuIoBufCtrl[tti][cc_id][ant_id].sBufferList.pBuffers->pData;
                    if(pRbMap) {
                        pRbMap->dir                     = XRAN_DIR_UL;
                        pRbMap->xran_port               = 0;
                        pRbMap->band_id                 = 0;
                        pRbMap->cc_id                   = cc_id;
                        pRbMap->ru_port_id              = ant_id;
                        pRbMap->tti_id                  = tti;
                        pRbMap->start_sym_id            = 0;

                        pRbMap->nPrbElm                 = 1;
                        pRbMap->prbMap[0].nRBStart      = 0;
                        pRbMap->prbMap[0].nRBSize       = get_num_ulrbs();
                        pRbMap->prbMap[0].nStartSymb    = 0;
                        pRbMap->prbMap[0].numSymb       = 14;
                        pRbMap->prbMap[0].nBeamIndex    = 0;
                        pRbMap->prbMap[0].compMethod    = XRAN_COMPMETHOD_NONE;

                        if(get_rucategory() == XRAN_CATEGORY_A) {
                            pRbMap->prbMap[0].BeamFormingType   = XRAN_BEAM_ID_BASED;
                            pRbMap->prbMap[0].bf_weight_update  = 0;
                            //pRbMap->prbMap[0].bf_attribute.weight[];
                            //pRbMap->prbMap[0].bf_precoding.weight[];
                            }
                        else if(get_rucategory() == XRAN_CATEGORY_B) {
                            int idxElm;
                            int iPrb;
                            char *ul_bfw_pos =  ((char*)p_tx_ul_bfw_buffer[flowId]) + tx_ul_bfw_buffer_position[flowId];
                            struct xran_prb_elm* p_prbMap = NULL;
                            int num_antelm;

                            pRbMap->prbMap[0].BeamFormingType   = XRAN_BEAM_WEIGHT;
                            pRbMap->prbMap[0].bf_weight_update  = 1;

                            num_antelm = get_num_antelmtrx();
#if 0
                            /* populate beam weights to C-plane for each elm */
                            pRbMap->bf_weight.nAntElmTRx = num_antelm;
                            for (idxElm = 0;  idxElm < pRbMap->nPrbElm; idxElm++){
                                p_prbMap = &pRbMap->prbMap[idxElm];
                                for (iPrb = p_prbMap->nRBStart; iPrb < (p_prbMap->nRBStart + p_prbMap->nRBSize); iPrb++){
                                    /* copy BF W IQs for 1 PRB of */
                                    rte_memcpy(&pRbMap->bf_weight.weight[iPrb][0], (ul_bfw_pos + (iPrb*num_antelm)*4), num_antelm*4);
                                    }
                                }
#endif
                            } /* else if(get_rucategory() == XRAN_CATEGORY_B) */

                        } /* if(pRbMap) */
                    else {
                        std::cout << "UL: pRbMap ==NULL" << std::endl;
                        }
                    }
                }
            }

        return (0);
    }

    void Cleanup()
    {
        int i;

        if(get_rucategory() == XRAN_CATEGORY_B) {
            for(i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(get_num_cc() * get_num_eaxc()); i++) {
                if(p_tx_dl_bfw_buffer[i]) {
                    free(p_tx_dl_bfw_buffer[i]);
                    p_tx_dl_bfw_buffer[i] == NULL;
                    }

                if(p_tx_ul_bfw_buffer[i]) {
                    free(p_tx_ul_bfw_buffer[i]);
                    p_tx_ul_bfw_buffer[i] == NULL;
                    }
                }
            }

        return;
    }


    void Open(xran_ethdi_mbuf_send_fn send_cp, xran_ethdi_mbuf_send_fn send_up,
            void *fh_rx_callback, void *fh_rx_prach_callback, void *fh_srs_callback)
    {
        struct xran_fh_config *pXranConf;
        int32_t nSectorNum;
        int i, j, k, z;

        uint32_t xran_max_antenna_nr = RTE_MAX(get_num_eaxc(), get_num_eaxc_ul());
        uint32_t xran_max_ant_array_elm_nr = RTE_MAX(get_num_antelmtrx(), xran_max_antenna_nr);

        struct xran_buffer_list *pFthTxBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
        struct xran_buffer_list *pFthTxPrbMapBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
        struct xran_buffer_list *pFthRxBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
        struct xran_buffer_list *pFthRxPrbMapBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
        struct xran_buffer_list *pFthRxRachBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
        struct xran_buffer_list *pFthRxSrsBuffer[XRAN_MAX_SECTOR_NR][XRAN_MAX_ANT_ARRAY_ELM_NR][XRAN_N_FE_BUF_LEN];

#if 0
        xran_reg_physide_cb(xranHandle, physide_dl_tti_call_back, NULL, 10, XRAN_CB_TTI);
        xran_reg_physide_cb(xranHandle, physide_ul_half_slot_call_back, NULL, 10, XRAN_CB_HALF_SLOT_RX);
        xran_reg_physide_cb(xranHandle, physide_ul_full_slot_call_back, NULL, 10, XRAN_CB_FULL_SLOT_RX);
#endif
        nSectorNum = get_num_cc();

        for(i=0; i<nSectorNum; i++)
        {
            for(j=0; j<XRAN_N_FE_BUF_LEN; j++)
            {
                for(z = 0; z < xran_max_antenna_nr; z++){
                    pFthTxBuffer[i][z][j]       = NULL;
                    pFthTxPrbMapBuffer[i][z][j] = NULL;
                    pFthRxBuffer[i][z][j]       = NULL;
                    pFthRxPrbMapBuffer[i][z][j] = NULL;
                    pFthRxRachBuffer[i][z][j]   = NULL;
                }
                for(z = 0; z < xran_max_ant_array_elm_nr; z++){
                    pFthRxSrsBuffer[i][z][j] = NULL;
                }
            }
        }

        for(i=0; i<nSectorNum; i++) {
            for(j=0; j<XRAN_N_FE_BUF_LEN; j++) {
                for(z = 0; z < xran_max_antenna_nr; z++) {
                    pFthTxBuffer[i][z][j]       = &(m_sFrontHaulTxBbuIoBufCtrl[j][i][z].sBufferList);
                    pFthTxPrbMapBuffer[i][z][j] = &(m_sFrontHaulTxPrbMapBbuIoBufCtrl[j][i][z].sBufferList);
                    pFthRxBuffer[i][z][j]       = &(m_sFrontHaulRxBbuIoBufCtrl[j][i][z].sBufferList);
                    pFthRxPrbMapBuffer[i][z][j] = &(m_sFrontHaulRxPrbMapBbuIoBufCtrl[j][i][z].sBufferList);
                    pFthRxRachBuffer[i][z][j]   = &(m_sFHPrachRxBbuIoBufCtrl[j][i][z].sBufferList);
                    }
                    for(z = 0; z < xran_max_ant_array_elm_nr && xran_max_ant_array_elm_nr; z++){
                        pFthRxSrsBuffer[i][z][j] = &(m_sFHSrsRxBbuIoBufCtrl[j][i][z].sBufferList);
            }
                }
            }

        if(m_nInstanceHandle[0] != NULL) {
            for(i = 0; i<nSectorNum; i++) {
                xran_5g_fronthault_config(m_nInstanceHandle[0][i],
                        pFthTxBuffer[i], pFthTxPrbMapBuffer[i],
                        pFthRxBuffer[i], pFthRxPrbMapBuffer[i],
                        (void (*)(void *, xran_status_t))fh_rx_callback, &pFthRxBuffer[i][0]);

                xran_5g_prach_req(m_nInstanceHandle[0][i], pFthRxRachBuffer[i],
                        (void (*)(void *, xran_status_t))fh_rx_prach_callback, &pFthRxRachBuffer[i][0]);
                }

            /* add SRS callback here */
            for (i = 0; i<nSectorNum && xran_max_ant_array_elm_nr; i++) {
                xran_5g_srs_req(m_nInstanceHandle[0][i], pFthRxSrsBuffer[i],
                    (void (*)(void *, xran_status_t))fh_srs_callback,&pFthRxSrsBuffer[i][0]);
                }

            }



        xran_register_cb_mbuf2ring(send_cp, send_up);

        xran_open(m_xranhandle, &m_xranConf);
    }

    void Close()
    {
        if(m_xranhandle)
            xran_close(m_xranhandle);
    }

    int Start()
    {
        if(m_xranhandle)
            return(xran_start(m_xranhandle));
        else
            return (-1);
    }

    int Stop()
    {
        if(m_xranhandle)
            return(xran_stop(m_xranhandle));
        else
            return (-1);
    }

    /* emulation of timer */
    void update_tti()
    {
        tti_ota_cb(nullptr, get_timer_ctx());
    }

    void update_symbol_index()
    {
        xran_lib_ota_sym_idx++;
        if((xran_lib_ota_sym_idx % N_SYM_PER_SLOT) == 0) {
            update_tti();
            }

        xran_lib_ota_sym++;
        if(xran_lib_ota_sym >= N_SYM_PER_SLOT)
            xran_lib_ota_sym = 0;
    }

    int apply_cpenable(bool flag)
    {
        struct xran_device_ctx *pCtx = xran_dev_get_ctx();

        if(is_running())
            return (-1);

        if(pCtx == nullptr)
            return (-1);

        if(flag == true) {
            m_xranInit.enableCP = 1;
            pCtx->enableCP = 1;
            }
        else {
            m_xranInit.enableCP = 0;
            pCtx->enableCP = 0;
            }

        return (0);
    }


    int get_slot_config(const std::string &cfgname, struct xran_frame_config *pCfg)
    {
        int numcfg, i, j;
        std::vector<int> slotcfg;

        numcfg = get_globalcfg<int>(cfgname, "period");
        pCfg->nTddPeriod = numcfg;
        for(i=0; i < numcfg; i++) {
            std::stringstream slotcfgname;

            slotcfgname << "slot" << i;
            std::vector<int> slotcfg = get_globalcfg_array<int>(cfgname, slotcfgname.str());

            for(j=0; j < slotcfg.size(); j++)
                pCfg->sSlotConfig[i].nSymbolType[j] = slotcfg[j];
            pCfg->sSlotConfig[i].reserved[0] = 0; pCfg->sSlotConfig[i].reserved[1] = 0;
            }

        return (numcfg);
    }

    int get_num_rbs(uint32_t nNumerology, uint32_t nBandwidth, bool nSub6)
    {
        if(nNumerology > 3)
            return (-1);

        if(nSub6) {
            if (nNumerology < 3) {
                /* F1 Tables 38.101-1 Table 5.3.2-1. Maximum transmission bandwidth configuration NRB */
                switch(nBandwidth) {
                    case PHY_BW_5MHZ:   return(nNumRbsPerSymF1[nNumerology][0]);
                    case PHY_BW_10MHZ:  return(nNumRbsPerSymF1[nNumerology][1]);
                    case PHY_BW_15MHZ:  return(nNumRbsPerSymF1[nNumerology][2]);
                    case PHY_BW_20MHZ:  return(nNumRbsPerSymF1[nNumerology][3]);
                    case PHY_BW_25MHZ:  return(nNumRbsPerSymF1[nNumerology][4]);
                    case PHY_BW_30MHZ:  return(nNumRbsPerSymF1[nNumerology][5]);
                    case PHY_BW_40MHZ:  return(nNumRbsPerSymF1[nNumerology][6]);
                    case PHY_BW_50MHZ:  return(nNumRbsPerSymF1[nNumerology][7]);
                    case PHY_BW_60MHZ:  return(nNumRbsPerSymF1[nNumerology][8]);
                    case PHY_BW_70MHZ:  return(nNumRbsPerSymF1[nNumerology][9]);
                    case PHY_BW_80MHZ:  return(nNumRbsPerSymF1[nNumerology][10]);
                    case PHY_BW_90MHZ:  return(nNumRbsPerSymF1[nNumerology][11]);
                    case PHY_BW_100MHZ: return(nNumRbsPerSymF1[nNumerology][12]);
                }
            }
        }
        else { /* if(nSub6) */
            if((nNumerology >= 2) && (nNumerology <= 3)) {
                nNumerology -= 2;
                /* F2 Tables 38.101-2 Table 5.3.2-1. Maximum transmission bandwidth configuration NRB */
                switch(nBandwidth) {
                    case PHY_BW_50MHZ:  return(nNumRbsPerSymF2[nNumerology][0]); break;
                    case PHY_BW_100MHZ: return(nNumRbsPerSymF2[nNumerology][1]); break;
                    case PHY_BW_200MHZ: return(nNumRbsPerSymF2[nNumerology][2]); break;
                    case PHY_BW_400MHZ: return(nNumRbsPerSymF2[nNumerology][3]); break;
                }
            }
        }

        return(-1);
    }

    void *get_xranhandle()  { return(m_xranhandle); }
    void *get_timer_ctx()   { return((void *)&m_timer_ctx[0]); }

    int get_symbol_index()  { return (xran_lib_ota_sym); }

    bool is_running()       { return((xran_get_if_state() == XRAN_RUNNING)?true:false); }

    enum xran_category get_rucategory()    { return(m_xranConf.ru_conf.xranCat); }

    int get_numerology()    { return(m_xranConf.frame_conf.nNumerology); }
    int get_duplextype()    { return(m_xranConf.frame_conf.nFrameDuplexType); }
    int get_num_cc()        { return(m_xranConf.nCC); }
    int get_num_eaxc()      { return(m_xranConf.neAxc); }
    int get_num_eaxc_ul()   { return(m_xranConf.neAxcUl); }
    int get_num_dlrbs()     { return(m_xranConf.nDLRBs); }
    int get_num_ulrbs()     { return(m_xranConf.nULRBs); }
    int get_num_antelmtrx() { return(m_xranConf.nAntElmTRx); }

    bool is_cpenable()      { return(m_xranInit.enableCP); };
    bool is_prachenable()   { return(m_xranInit.prachEnable); };
    bool is_dynamicsection() { return(m_xranInit.DynamicSectionEna?true:false); }

    void get_cfg_prach(struct xran_prach_config *pCfg)
    {
        if(pCfg)
            memcpy(pCfg, &m_xranConf.prach_conf, sizeof(struct xran_prach_config));
    }

    void get_cfg_frame(struct xran_frame_config *pCfg)
    {
        if(pCfg)
            memcpy(pCfg, &m_xranConf.frame_conf, sizeof(struct xran_frame_config));
    }

    void get_cfg_ru(struct xran_ru_config *pCfg)
    {
        if(pCfg)
            memcpy(pCfg, &m_xranConf.ru_conf, sizeof(struct xran_ru_config));
    }

    void get_cfg_fh(struct xran_fh_config *pCfg)
    {
        if(pCfg)
            memcpy(pCfg, &m_xranConf, sizeof(struct xran_fh_config));
    }

};


/* external declaration for the instance */
extern xranLibWraper *xranlib;


#endif //XRAN_LIB_WRAP_HPP
