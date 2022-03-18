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

#include <assert.h>
#include <err.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <immintrin.h>
#include <libgen.h>
#include "common.h"
#include "xran_fh_o_du.h"
#include "xran_pkt.h"
#include "xran_pkt_up.h"
#include "xran_cp_api.h"
#include "xran_up_api.h"

#include "xran_mlog_lnx.h"
#include "app_profile_xran.h"
#include "xran_timer.h"
#include "xran_lib_mlog_tasks_id.h"
#include "xran_mlog_task_id.h"

#define XRAN_REPORT_FILE    "xran_mlog_stats"

int32_t xran_init_mlog_stats(char *file, uint64_t nTscFreq);
int32_t xran_get_mlog_stats(char *, UsecaseConfig *, RuntimeConfig *[], struct xran_mlog_times *);

struct xran_mlog_times mlog_times = {0};
struct xran_mlog_stats tmp;
uint64_t xran_total_ticks = 0, xran_mlog_time;
uint64_t tWake, tWakePrev = 0;

extern UsecaseConfig* p_usecaseConfiguration;
extern RuntimeConfig* p_startupConfiguration[XRAN_PORTS_NUM];

#ifdef MLOG_ENABLED
/*
 * Covert a test case path into a test case name
 *   with the last two basenames in the path.
 */
int32_t
test_path_to_name(char *path, char *name)
{
    if (path == NULL || name == NULL)
    {
        print_err("Null path(%#p) or name(%#p)", path, name);
        return -1;
    }

    char *dir, *base, *np = strdup(path);
    int num=0;

    if (np)
    {
        base = basename(np);
        if (isdigit(*base))
        {
            num = atoi(base);
            *--base = '\0';     /* trim the last basename */
            base = basename(np);
        }

        dir = dirname(np);
        sprintf(name, "%s-%s-%d", basename(dir), base, num);
        free(np);
        return 0;
    }

    return -1;
}




//-------------------------------------------------------------------------------------------
/** @ingroup group_source_flexran_xran
 *
 *  @param[in]   nTscFreq Frequency of the Time Stamp Counter (TSC) that the CPU currently is
 *                        programmed with
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function dumps current CPU information onto the XRAN_REPORT_FILE file which is used
 *  for automation of report generation
 *
**/
//-------------------------------------------------------------------------------------------
int32_t
xran_init_mlog_stats(char *file, uint64_t nTscFreq)
{
    char command[1024];
    FILE *pFile= NULL;

    pFile = fopen(file, "w");
    if (pFile == NULL)
    {
        printf("1: Cannot open %s to write in phydi_init_mlog_stats\n", file);
        return -1;
    }
    fprintf(pFile, "------------------------------------------------------------------------------------------------------------\n");
    fprintf(pFile, "SYSTEM_PARAMS:\n");
    fprintf(pFile, "TSC_FREQ: %ld\n", nTscFreq);

#ifdef BBDEV_FEC_ACCL_NR5G
    PPHYCFG_VARS pPhyCfgVars = phycfg_get_ctx();

    if (pPhyCfgVars->dpdkBasebandFecMode == 0)
    {
        fprintf(pFile, "FEC_OFFLOAD: SOFT_LDPC\n");
    }
    else
    {
        uint32_t nRet = phy_gnb_check_bbdev_hw_type();

        if (nRet == BBDEV_DEV_NAME_MOUNT_BRYCE)
            fprintf(pFile, "FEC_OFFLOAD: MOUNT_BRYCE\n");
        else if (nRet == BBDEV_DEV_NAME_VISTA_CREEK)
            fprintf(pFile, "FEC_OFFLOAD: VISTA_CREEK\n");
        else if (nRet == BBDEV_DEV_NAME_SW_LDPC)
            fprintf(pFile, "FEC_OFFLOAD: SOFT_LDPC\n");
        else
            fprintf(pFile, "FEC_OFFLOAD: UNKNOWN\n");
    }
#else
    fprintf(pFile, "FEC_OFFLOAD: TERASIC\n");
#endif

    fclose(pFile);
    pFile = NULL;
    usleep(100000);
    sprintf(command, "lscpu >> %s", file);
    system(command);
    usleep(100000);

    pFile = fopen(file, "a");
    if (pFile == NULL)
    {
        printf("2: Cannot open %s to write in %s\n", file, __FUNCTION__);
        return -1;
    }
    fprintf(pFile, "------------------------------------------------------------------------------------------------------------\n");
    fprintf(pFile, "COMMAND_LINE:\n");
    fclose(pFile);

    usleep(100000);
    sprintf(command, "cat /proc/cmdline >> %s", file);
    system(command);
    usleep(100000);

    pFile = fopen(file, "a");
    if (pFile == NULL)
    {
        printf("3: Cannot open %s to write in %s\n", file, __FUNCTION__);
        return -1;
    }
    fprintf(pFile, "------------------------------------------------------------------------------------------------------------\n");
    fprintf(pFile, "MEMORY_INFO:\n");
    fclose(pFile);
    pFile = NULL;

    usleep(100000);
    sprintf(command, "dmidecode -t memory >> %s", file);
    system(command);
    usleep(100000);

    pFile = fopen(file, "a");
    if (pFile == NULL)
    {
        printf("4: Cannot open %s to write in %s\n", file, __FUNCTION__);
        return -1;
    }
    fprintf(pFile, "------------------------------------------------------------------------------------------------------------\n");
    fprintf(pFile, "TURBOSTAT_INFO:\n");
    fclose(pFile);
    pFile = NULL;
    usleep(100000);
    sprintf(command, "turbostat --num_iterations 1 --interval 1 -q >> %s", file);

    system(command);
    usleep(100000);

    pFile = fopen(file, "a");
    if (pFile == NULL)
    {
        printf("5: Cannot open %s to write in %s\n", file, __FUNCTION__);
        return -1;
    }
    fprintf(pFile, "---------------------------------------------------------------------------\n");
    fflush(pFile);
    fclose(pFile);

    return 0;
}

int32_t
xran_get_mlog_stats(char *usecase, UsecaseConfig *puConf, RuntimeConfig *psConf[], struct xran_mlog_times *mlog_times_p)
{
    int i, ret=0;
    FILE *pFile= NULL;
    char stats_file[512]={0};
    struct xran_mlog_stats tti, tmp;
    uint32_t ttiDuration = 1000;

    printf("%s: Usecase: %s\n", __FUNCTION__, usecase);
    if (puConf == NULL || psConf == NULL || mlog_times_p == NULL) {
        print_err("Null puConf(%p), psConf(%p), or mlog_times(%p)!",
                puConf, psConf, mlog_times_p);
        ret = -1;
        goto exit;
    }

    MLogPrint((char *)MLogGetFileName());

    MLogGetStats(PID_TTI_TIMER, &tti.cnt, &tti.max, &tti.min, &tti.avg);
    if (tti.cnt != 0) {
        sprintf(stats_file, "%s-%s-%s\0", XRAN_REPORT_FILE, (puConf->appMode == APP_O_DU)? "o-du" : "o-ru", usecase);
        printf("xran report file: %s\n", stats_file);
        ret = xran_init_mlog_stats(stats_file, mlog_times_p->ticks_per_usec);
        if (ret != 0)
        {
            print_err("xran_init_mlog_stats(%s) returned %d!!", stats_file, ret);
            ret = -2;
            goto exit;
        }

        pFile = fopen(stats_file, "a");
        if (pFile == NULL)
        {
            print_err("Cannot create %s!!", stats_file);
            ret = -2;
            goto exit;
        }

        for (i = 0; i < psConf[0]->mu_number; i++)
            ttiDuration = ttiDuration >> 1;

        fprintf(pFile, "All data in this sheet are presented in usecs\n");
        fprintf(pFile, "ORANTest: %s-%s (Num Cells: %d) (Num TTI: %d) (nNumerology: %d) (ttiDuration: %d usecs) (testStats: %d %ld %ld)\n",
            (puConf->appMode == APP_O_DU)? "O-DU" : "O-RU", usecase, puConf->oXuNum * psConf[0]->numCC, tti.cnt, psConf[0]->mu_number, ttiDuration, puConf->appMode, mlog_times_p->xran_total_time, mlog_times_p->mlog_total_time);

        double xran_task_type_sum[XRAN_TASK_TYPE_MAX] = {0, 0, 0, 0, 0, 0};
        char * xran_task_type_name[XRAN_TASK_TYPE_MAX] =
            { "GNB", "BBDEV", "Timer", "Radio", "CP", "UP" };
#define NUM_GNB_TASKS           (5)
#define NUM_BBDEV_TASKS         (4)
#define NUM_TIMER_TASKS         (7)
#define NUM_RADIO_TASKS         (2)
#define NUM_CP_TASKS            (7)
#define NUM_UP_TASKS            (5)
#define NUM_ALL_TASKS (NUM_GNB_TASKS+NUM_BBDEV_TASKS+NUM_TIMER_TASKS+NUM_RADIO_TASKS+NUM_CP_TASKS+NUM_UP_TASKS)
        struct xran_mlog_taskid xranTasks[NUM_ALL_TASKS] = {
            {PID_GNB_PROC_TIMING, XRAN_TASK_TYPE_GNB,              "GNB_PROCC_TIMING                    \0"},
            {PID_GNB_PROC_TIMING_TIMEOUT, XRAN_TASK_TYPE_GNB,      "GNB_PROCC_TIMING_TIMEOUT            \0"},
            {PID_GNB_SYM_CB, XRAN_TASK_TYPE_GNB,                   "GNB_SYM_CB                          \0"},
            {PID_GNB_PRACH_CB, XRAN_TASK_TYPE_GNB,                 "GNB_PRACH_CB                        \0"},
            {PID_GNB_SRS_CB, XRAN_TASK_TYPE_GNB,                   "GNB_SRS_CB                          \0"},

            {PID_XRAN_BBDEV_DL_POLL, XRAN_TASK_TYPE_BBDEV,         "BBDEV_DL_POLL                       \0"},
            {PID_XRAN_BBDEV_DL_POLL_DISPATCH, XRAN_TASK_TYPE_BBDEV,"BBDEV_DL_POLL_DISPATCH              \0"},
            {PID_XRAN_BBDEV_UL_POLL, XRAN_TASK_TYPE_BBDEV,         "BBDEV_UL_POLL                       \0"},
            {PID_XRAN_BBDEV_UL_POLL_DISPATCH, XRAN_TASK_TYPE_BBDEV,"BBDEV_UL_POLL_DISPATCH              \0"},

            {PID_TTI_TIMER, XRAN_TASK_TYPE_TIMER,                  "TTI_TIMER                           \0"},
            {PID_TTI_CB, XRAN_TASK_TYPE_TIMER,                     "TTI_CB                              \0"},
            {PID_TIME_SYSTIME_POLL, XRAN_TASK_TYPE_TIMER,          "TIME_SYSTIME_POLL                   \0"},
            {PID_TIME_SYSTIME_STOP, XRAN_TASK_TYPE_TIMER,          "TIME_SYSTIME_STOP                   \0"},
            {PID_TIME_ARM_TIMER, XRAN_TASK_TYPE_TIMER,             "TIME_ARM_TIMER                      \0"},
            {PID_TIME_ARM_TIMER_DEADLINE, XRAN_TASK_TYPE_TIMER,    "TIME_ARM_TIMER_DEADLINE             \0"},
            {PID_TIME_ARM_USER_TIMER_DEADLINE, XRAN_TASK_TYPE_TIMER,"TIME_ARM_USER_TIMER_DEADLINE        \0"},

            {PID_RADIO_ETH_TX_BURST, XRAN_TASK_TYPE_RADIO,         "RADIO_ETH_TX_BURST                  \0"},
            {PID_RADIO_RX_VALIDATE, XRAN_TASK_TYPE_RADIO,          "RADIO_RX_VALIDATE                   \0"},

            {PID_PROCESS_TX_SYM, XRAN_TASK_TYPE_CP,                "PROCESS_TX_SYM                      \0"},
            {PID_DISPATCH_TX_SYM, XRAN_TASK_TYPE_CP,               "PID_DISPATCH_TX_SYM                 \0"},
            {PID_CP_DL_CB, XRAN_TASK_TYPE_CP,                      "PID_CP_DL_CB                        \0"},
            {PID_CP_UL_CB, XRAN_TASK_TYPE_CP,                      "PID_CP_UL_CB                        \0"},
            {PID_SYM_OTA_CB, XRAN_TASK_TYPE_CP,                    "SYM_OTA_CB                          \0"},
            {PID_TTI_CB_TO_PHY, XRAN_TASK_TYPE_CP,                 "TTI_CB_TO_PHY                       \0"},
            {PID_PROCESS_CP_PKT, XRAN_TASK_TYPE_CP,                "PROCESS_CP_PKT                      \0"},

            {PID_UP_UL_HALF_DEAD_LINE_CB, XRAN_TASK_TYPE_UP,       "UP_UL_HALF_DEAD_LINE_CB             \0"},
            {PID_UP_UL_FULL_DEAD_LINE_CB, XRAN_TASK_TYPE_UP,       "UP_UL_FULL_DEAD_LINE_CB             \0"},
            {PID_UP_UL_USER_DEAD_LINE_CB, XRAN_TASK_TYPE_UP,       "UP_UL_USER_DEAD_LINE_CB             \0"},
            {PID_PROCESS_UP_PKT, XRAN_TASK_TYPE_UP,                "PROCESS_UP_PKT                      \0"},
            {PID_PROCESS_UP_PKT_SRS, XRAN_TASK_TYPE_UP,            "PROCESS_UP_PKT_SRS                  \0"},
        };

#if 1
        fprintf(pFile, "mlog_times: core used/total %lu/%lu, xran %lu(us)\n",
                mlog_times_p->core_used_time, mlog_times_p->core_total_time,
                mlog_times_p->xran_total_time);
#endif

        fprintf(pFile, "---------------------------------------------------------------------------\n");
        fprintf(pFile, "All task breakdown\n");
        for (i=0; i < NUM_ALL_TASKS; i++) {
            struct xran_mlog_taskid *p;

            p = &xranTasks[i];
            MLogGetStats(p->taskId, &tmp.cnt, &tmp.max, &tmp.min, &tmp.avg);
            fprintf(pFile, "%4u:%s\t\t:\t%5.2f\n",
                    p->taskId, p->taskName, tmp.avg);
            if (p->taskId != PID_TIME_SYSTIME_POLL) /* Skip TIME_SYSTIME_POLL */
                xran_task_type_sum[p->taskType] += tmp.avg * tmp.cnt;
        }
        fprintf(pFile, "---------------------------------------------------------------------------\n");
        fprintf(pFile, "Task type breakdown:\t\ttotal time\t(busy %%)\n");
        for (i=0; i < XRAN_TASK_TYPE_MAX; i++) {
            char name[32] ={' '};

            sprintf(name,"%5s tasks", xran_task_type_name[i]);
            name[31]='\0';
            fprintf(pFile, "%s:\t\t\t\t\t%7.2f\t(%5.2f%%)\n",
                    name, xran_task_type_sum[i] / tti.cnt,
                    xran_task_type_sum[i] * 100 / mlog_times_p->xran_total_time);
        }
        fprintf(pFile, "---------------------------------------------------------------------------\n\n");
        fprintf(pFile, "====~~~~====~~~~====~~~~====~~~~====~~~~====~~~~====~~~~====~~~~====~~~~====~~~~====~~~~====~~~~====~~~~====~~~~~~~~====~~~~====~~~~");
    }

exit:
    if (pFile)
    {
        fflush(pFile);
        printf("Closing [%s] ...\n", stats_file);
        fclose(pFile);
        pFile = NULL;
    }
    printf("%s: exit: %d\n", __FUNCTION__, ret);
    return ret;
}

int32_t
app_profile_xran_print_mlog_stats(char *usecase_file)
{
    int32_t ret = 0;
    char filename[512];

    printf("core_total_time\t\t%lu,\tcore_used_time\t\t%lu,\t%5.2f%% busy\n",
            mlog_times.core_total_time, mlog_times.core_used_time,
            ((float)mlog_times.core_used_time * 100.0) / (float)mlog_times.core_total_time);
    mlog_times.xran_total_time = xran_total_ticks / MLogGetFreq();
    printf("xran_total_ticks %lu (%lu usec)\n", xran_total_ticks, mlog_times.xran_total_time);

    MLogGetStats(PID_XRAN_MAIN, &tmp.cnt, &tmp.max, &tmp.min, &tmp.avg);
    mlog_times.mlog_total_time = tmp.cnt * (uint64_t)tmp.avg;
    printf("xran_mlog_time: %lu usec\n", mlog_times.mlog_total_time);

    MLogSetMask(0); /* Turned off MLOG */
    test_path_to_name(usecase_file, filename);
    printf("test cases: %s\n", filename);
    ret = xran_get_mlog_stats(filename, p_usecaseConfiguration, p_startupConfiguration, &mlog_times);

    return ret;
}

#endif  /* MLOG_ENABLED */
