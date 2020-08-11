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

/**
 * @brief This file has the command line interface for the testmac application
 * @file nr5g_fapi_cmd.c
 * @ingroup group_source_utils
 * @author Intel Corporation
 **/

#include "nr5g_fapi_std.h"
#include "nr5g_fapi_framework.h"
#include "nr5g_fapi_wls.h"

#define NUM_CMDS 5
#define CMD_SIZE 32
#define ORAN_5G_FAPI_LOGO "ORAN_5G_FAPI>"

enum {
    NR5G_FAPI_CMGR_HELP = 0,
    NR5G_FAPI_CMGR_EXIT,
    NR5G_FAPI_CMGR_VERS,
    NR5G_FAPI_CMGR_WLS_STATS,
    NR5G_FAPI_CMGR_NULL,
};

static char nr5g_fapi_cmd_registry[NUM_CMDS][CMD_SIZE] = {
    {"help"}, {"exit"}, {"version"}, {"show wls stats"}
};

static char *nr5g_fapi_cmgr_char_get(
    void)
{
    static char str[CMD_SIZE + 2];
    int ch, n = 0;

    while ((ch = getchar()) != EOF && n < CMD_SIZE && ch != '\n') {
        str[n] = ch;
        ++n;
    }

    if (n > 0 && n < CMD_SIZE) {
        str[n] = (char)0;
        return str;
    } else {
        return NULL;
    }
}

static int nr5g_fapi_cmgr_type(
    char *cmd)
{
    int len = 0, i;
    for (i = 0; i < NUM_CMDS; i++) {
        nr5g_fapi_cmd_registry[i][sizeof(nr5g_fapi_cmd_registry[i]) - 1] = 0;
        len = strlen(nr5g_fapi_cmd_registry[i]);
        if (len < CMD_SIZE) {
            if (!strncmp(cmd, nr5g_fapi_cmd_registry[i], len)) {
                return i;
            }
        }
    }

    return NR5G_FAPI_CMGR_NULL;
}

//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]   paramc Number of command line parameters
 *  @param[in]   params Pointer to structure that has the command line parameter
 *  @param[in]   *cmd_param Pointer to additional parameters
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function prints the oran_5g_fapi version to the oran_5g_fapi console
 *
**/
//-------------------------------------------------------------------------------------------
static void nr5g_fapi_cmd_print_version(
    )
{
    //printf("\n ORAN_5G_FAPI Version: \n");
}

//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]   paramc Number of command line parameters
 *  @param[in]   params Pointer to structure that has the command line parameter
 *  @param[in]   *cmd_param Pointer to additional parameters
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function exits the testmac application
 *
**/
//-------------------------------------------------------------------------------------------
static void nr5g_fapi_cmd_exit(
    p_nr5g_fapi_phy_ctx_t p_phy_ctx)
{
    p_phy_ctx->process_exit = 1;
    printf("Exitting App...\n");
    exit(0);
}

//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]   paramc Number of command line parameters
 *  @param[in]   params Pointer to structure that has the command line parameter
 *  @param[in]   *cmd_param Pointer to additional parameters
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function lists all the pre-registered function and associated commands on screen.
 *  The commands are sorted alphabetically.
 *
**/
//-------------------------------------------------------------------------------------------
static void nr5g_fapi_cmd_help(
    )
{

}

//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]   paramc Number of command line parameters
 *  @param[in]   params Pointer to structure that has the command line parameter
 *  @param[in]   *cmd_param Pointer to additional parameters
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function lists all the pre-registered function and associated commands on screen.
 *  The commands are sorted alphabetically.
 *
**/
//-------------------------------------------------------------------------------------------
static void nr5g_fapi_cmd_wls_stats(
    )
{
    nr5g_fapi_wls_print_stats();
}

//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param   void
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function initializes all the pre-defined commands with associated functions for the testmac
 *  application. It iscalled on system bootup
 *
**/
//-------------------------------------------------------------------------------------------
void nr5g_fapi_cmgr(
    void *config)
{
    static int process_exit = 1;
    char *cmd;
    int cmd_type;
    UNUSED(config);
    //p_nr5g_fapi_cfg_t cfg = (p_nr5g_fapi_cfg_t) config; // start up config
    p_nr5g_fapi_phy_ctx_t p_phy_ctx = nr5g_fapi_get_nr5g_fapi_phy_ctx();

    fflush(stdout);
    printf("\n");
    while (process_exit) {
        printf("%s", ORAN_5G_FAPI_LOGO);

        cmd = nr5g_fapi_cmgr_char_get();
        if (!cmd)
            continue;

        cmd_type = nr5g_fapi_cmgr_type(cmd);
        switch (cmd_type) {
            case NR5G_FAPI_CMGR_VERS:
                nr5g_fapi_cmd_print_version();

                break;
            case NR5G_FAPI_CMGR_HELP:
                nr5g_fapi_cmd_help();
                break;

            case NR5G_FAPI_CMGR_EXIT:
                nr5g_fapi_cmd_exit(p_phy_ctx);
                break;

            case NR5G_FAPI_CMGR_WLS_STATS:
                nr5g_fapi_cmd_wls_stats();
                break;

            case NR5G_FAPI_CMGR_NULL:
            default:
                printf("Warning: command (%s) not present\n", cmd);
                break;
        }
    }
}
