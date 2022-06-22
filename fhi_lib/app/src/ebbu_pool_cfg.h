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

/**
 * @brief This file consists of parameters that are to be read from ebbu_pool_cfg.xml
 * to configure the application at system initialization
 * @file ebbu_pool_cfg.h
 * @ingroup xran
 * @author Intel Corporation
**/

#ifndef _EBBUPOOLCFG_H_
#define _EBBUPOOLCFG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ebbu_pool_api.h"
#include "aux_cline.h"

#define EBBU_POOL_FILE_NAME                    "config_file/ebbu_pool_cfg_basic.xml"

#define EBBU_POOL_CFG_ERRORCODE__SUCCESS       ( 0 )
#define EBBU_POOL_CFG_ERRORCODE__FAIL          ( 1 )
#define EBBU_POOL_CFG_ERRORCODE__VER_MISMATCH  ( 2 )

#define EBBU_POOL_MAX_TEST_CELL 40
#define EBBU_POOL_MAX_TEST_CORE 256
#define EBBU_POOL_MAX_CTRL_THREAD 8


#define EBBU_POOL_MAX_FRAME_FORMAT 3
#define EBBU_POOL_TDD_PERIOD 10
#define EBBU_POOL_TEST_DL 1
#define EBBU_POOL_TEST_UL 2

extern uint32_t nD2USwitch[EBBU_POOL_MAX_FRAME_FORMAT][EBBU_POOL_TDD_PERIOD];

typedef struct
{
    uint32_t frameFormat; //FDD or TDD:DDDSU, DDDDDDDSUU
    uint32_t tti; //micro-second
    uint32_t eventPerTti;
}eBbuPoolTestCellStruc;

typedef struct
{
    //eBbuPool general config
    uint32_t mainThreadCoreId;
    uint32_t sleepFlag;

    //Queus config
    uint32_t queueDepth;
    uint32_t queueNum;
    uint32_t ququeCtxNum;

    //Test config
    uint32_t timerCoreId;
    uint32_t ctrlThreadNum;
    uint32_t ctrlThreadCoreId[EBBU_POOL_MAX_CTRL_THREAD];
    uint32_t testCellNum;
    eBbuPoolTestCellStruc sTestCell[EBBU_POOL_MAX_TEST_CELL];
    uint32_t testCoreNum;
    uint32_t testCoreList[EBBU_POOL_MAX_TEST_CORE];

    //Misc
    uint32_t mlogEnable;
} eBbuPoolCfgVarsStruct, *peBbuPoolCfgVarsStruct;

peBbuPoolCfgVarsStruct ebbu_pool_cfg_get_ctx(void);
uint32_t ebbu_pool_cfg_init_from_xml(void);
void ebbu_pool_cfg_set_cfg_filename(int argc, char *argv[], char filename[512]);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _EBBUPOOLCFG_H_ */

