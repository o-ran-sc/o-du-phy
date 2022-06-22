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


#include "ebbu_pool_cfg.h"

#include <sys/param.h>
#include <stdio.h>
#include <string.h>

static eBbuPoolCfgVarsStruct geBbuPoolCfgVars;
char eBbuPoolCfgFileName[512];

uint32_t nD2USwitch[EBBU_POOL_MAX_FRAME_FORMAT][EBBU_POOL_TDD_PERIOD] =
{
    {0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3}, //FDD
    {0x1, 0x1, 0x1, 0x3, 0x2, 0x1, 0x1, 0x1, 0x3, 0x2}, //DDDSU
    {0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x2, 0x2}  //DDDDDDDSUU
};

//-------------------------------------------------------------------------------------------
/** @ingroup group_test_ebbu_pool
 *
 *  @param   void
 *
 *  @return  eBBUPool Config Local Context Structure Pointer
 *
 *  @description
 *  Returns the eBBUPool Config Local Context Structure Pointer
 *
**/
//-------------------------------------------------------------------------------------------
peBbuPoolCfgVarsStruct ebbu_pool_cfg_get_ctx(void)
{
    return &geBbuPoolCfgVars;
}



//-------------------------------------------------------------------------------------------
/** @ingroup group_test_ebbu_pool
 *
 *  @param   void
 *
 *  @return  void
 *
 *  @description
 *  Initialize the geBbuPoolCfgVars. This is called at application bootup
 *
**/
//-------------------------------------------------------------------------------------------
void ebbu_pool_cfg_init_vars(void)
{
    memset(&geBbuPoolCfgVars, 0, sizeof(eBbuPoolCfgVarsStruct));
}



//-------------------------------------------------------------------------------------------
/** @ingroup group_test_ebbu_pool
 *
 *  @param[in]   *pCfgFile Pointer to FILE descriptor to read from
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function parses the XML file that was opened and reads all the tags and associated
 *  values and stores them
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t ebbu_pool_cfg_parse_xml(FILE *pCfgFile)
{
    // peBbuPoolCfgVarsStruct geBbuPoolCfgVars = ebbu_pool_cfg_get_ctx();
    uint32_t lineNum = 0, retCode;
    char string[1024];

    while(!feof(pCfgFile))
    {
        fgets(string, 1024, pCfgFile);
        lineNum++;
        retCode = cline_parse_line(string);

        if (retCode != EBBU_POOL_CFG_ERRORCODE__SUCCESS)
        {
            printf("Something wrong in file %s ErrorCode[%d] at line[%d]: %s\n", eBbuPoolCfgFileName, (int) retCode, (int)lineNum, string);
            return EBBU_POOL_CFG_ERRORCODE__FAIL;
        }
    }

    return EBBU_POOL_CFG_ERRORCODE__SUCCESS;
}


//-------------------------------------------------------------------------------------------
/** @ingroup group_test_ebbu_pool
 *
 *  @param   void
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function searches for tags from xml and applies the values to the eBBUPool test
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t ebbu_pool_cfg_apply(void)
{
    printf("ebbu_pool_cfg_apply\n");
    peBbuPoolCfgVarsStruct geBbuPoolCfgVars = ebbu_pool_cfg_get_ctx();
    int rc = EBBU_POOL_CFG_ERRORCODE__SUCCESS;
    int32_t coreNum, cellNum, iCell;
    uint32_t cellFrameCfg[EBBU_POOL_MAX_TEST_CELL], cellTtiCfg[EBBU_POOL_MAX_TEST_CELL], cellEventCfg[EBBU_POOL_MAX_TEST_CELL];

    cline_print_info();

    //----------------------------------------------------
    // eBbuPool general config
    //----------------------------------------------------
    rc |= cline_set_int((const char *) "eBbuPoolMainThreadCore", (int *) &geBbuPoolCfgVars->mainThreadCoreId, 0);
    rc |= cline_set_int((const char *) "eBbuPoolConsumerSleep",  (int *) &geBbuPoolCfgVars->sleepFlag, 1);

    //----------------------------------------------------
    // Queue config
    //----------------------------------------------------
    rc |= cline_set_int((const char *) "QueueDepth",  (int *) &geBbuPoolCfgVars->queueDepth, 1024);
    rc |= cline_set_int((const char *) "QueueNum",    (int *) &geBbuPoolCfgVars->queueNum, 3);
    rc |= cline_set_int((const char *) "QueuCtxNum",  (int *) &geBbuPoolCfgVars->ququeCtxNum, 1);

    //----------------------------------------------------
    // Test Config
    //----------------------------------------------------
    rc |= cline_set_int((const char *) "TimerThreadCore",   (int *) &geBbuPoolCfgVars->timerCoreId, 0);
    rc |= cline_set_int((const char *) "CtrlThreadNum",     (int *) &geBbuPoolCfgVars->ctrlThreadNum, 0);
    rc |= cline_set_int_array((const char *) "CtrlThreadCoreList",  EBBU_POOL_MAX_CTRL_THREAD,(int *) &geBbuPoolCfgVars->ctrlThreadCoreId[0], &coreNum);
    rc |= cline_set_int((const char *) "TestCellNum",       (int *) &geBbuPoolCfgVars->testCellNum, 0);
    rc |= cline_set_int((const char *) "TestCoreNum",       (int *) &geBbuPoolCfgVars->testCoreNum, 0);
    rc |= cline_set_int_array((const char *) "TestCoreList",        EBBU_POOL_MAX_TEST_CORE,  (int *) &geBbuPoolCfgVars->testCoreList[0], &coreNum);
    rc |= cline_set_int_array((const char *) "TestCellFrameFormat", EBBU_POOL_MAX_TEST_CELL,  (int *) &cellFrameCfg[0], &cellNum);
    rc |= cline_set_int_array((const char *) "TestCellTti",         EBBU_POOL_MAX_TEST_CELL,  (int *) &cellTtiCfg[0], &cellNum);
    rc |= cline_set_int_array((const char *) "TestCellEventNum",    EBBU_POOL_MAX_TEST_CELL,  (int *) &cellEventCfg[0], &cellNum);
    rc |= cline_set_int((const char *) "MlogEnable",        (int *) &geBbuPoolCfgVars->mlogEnable, 0);

    if(cellNum > geBbuPoolCfgVars->testCellNum)
        cellNum = geBbuPoolCfgVars->testCellNum;

    for(iCell = 0; iCell < cellNum; iCell ++)
    {
        geBbuPoolCfgVars->sTestCell[iCell].frameFormat = cellFrameCfg[iCell];
        geBbuPoolCfgVars->sTestCell[iCell].tti = cellTtiCfg[iCell];
        geBbuPoolCfgVars->sTestCell[iCell].eventPerTti = cellEventCfg[iCell];
    }


    printf("eBbuPool config completely read: %x\n", rc);

    printf("\n");

    return rc;
}




//-------------------------------------------------------------------------------------------
/** @ingroup group_test_ebbu_pool
 *
 *  @param   void
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function is called from main() and initializes the EBBU_POOL_CFG layer. It reads the xml
 *  file and configures the application based on xml tags and values from file.
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t ebbu_pool_cfg_init_from_xml(void)
{
    // peBbuPoolCfgVarsStruct geBbuPoolCfgVars = ebbu_pool_cfg_get_ctx();
    FILE *pCfgFile;
    pCfgFile = fopen(eBbuPoolCfgFileName, "r");

    ebbu_pool_cfg_init_vars();

    cline_init();

    if (pCfgFile == NULL)
    {
        printf("ERROR: %s file is not found in directory\n", eBbuPoolCfgFileName);
        printf("       Please contact Intel to get the correct config file\n");
        return EBBU_POOL_CFG_ERRORCODE__FAIL;
    }

    if (ebbu_pool_cfg_parse_xml(pCfgFile) != EBBU_POOL_CFG_ERRORCODE__SUCCESS)
    {
        printf("Could not Parse the XML File.\n");
        fclose(pCfgFile);
        return EBBU_POOL_CFG_ERRORCODE__FAIL;
    }
    printf("PhyCfg XML file parsed\n");

    if (ebbu_pool_cfg_apply() != EBBU_POOL_CFG_ERRORCODE__SUCCESS)
    {
        printf("Could not Apply the settings from XML File.\n");
        fclose(pCfgFile);
        return EBBU_POOL_CFG_ERRORCODE__FAIL;
    }

    fclose(pCfgFile);
    return EBBU_POOL_CFG_ERRORCODE__SUCCESS;
}


//-------------------------------------------------------------------------------------------
/** @ingroup group_test_ebbu_pool
 *
 *  @param[in]   argc Number of command line arguments
 *  @param[in]   *argv[] Array of command line arguments
 *
 *  @return  void
 *
 *  @description
 *  This function parses the command line parameters entered while running the eBbuPool test application
 *  and searches for string "cfgfile" and takes the immediate next field and uses it as the
 *  xml file name. If it is not found, it uses default file name "bbdev_cfg.xml" and tries to open
 *  this.
 *
**/
//-------------------------------------------------------------------------------------------
void ebbu_pool_cfg_set_cfg_filename(int argc, char *argv[], char filename[512])
{
    // peBbuPoolCfgVarsStruct geBbuPoolCfgVars = ebbu_pool_cfg_get_ctx();
#if 0
    uint32_t ret;
    ret = cline_get_string(argc, argv, "cfgfile", eBbuPoolCfgFileName);

    if (ret != AUX_SUCCESS)
    {
        printf("ebbu_pool_cfg_set_cfg_filename: Coult not find string 'cfgfile' in command line. Using default File: %s\n", EBBU_POOL_FILE_NAME);
        strcpy(eBbuPoolCfgFileName, EBBU_POOL_FILE_NAME);
    }
    strcpy(filename, eBbuPoolCfgFileName);
#else
    strncpy(eBbuPoolCfgFileName, filename, MIN(strnlen(filename, 511),511) );
    eBbuPoolCfgFileName[511] = '\0';
    printf("eBbuPoolCfgFileName %s\n", eBbuPoolCfgFileName);
#endif
    return;
}

