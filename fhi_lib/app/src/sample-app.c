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
 * @brief Main module of sample application. Demonstration of usage of xRAN library for ORAN
 *        WG4 Front haul
 * @file sample-app.c
 * @ingroup xran
 * @author Intel Corporation
 *
 **/

#define _GNU_SOURCE
#include <unistd.h>
#include <immintrin.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <sched.h>
#include <assert.h>
#include <err.h>
#include <libgen.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#include "common.h"
#include "config.h"
#include "xran_mlog_lnx.h"

#include "xran_fh_o_du.h"
#include "xran_sync_api.h"
#include "xran_mlog_task_id.h"
#include "app_io_fh_xran.h"
#include "app_profile_xran.h"
#include "xran_ecpri_owd_measurements.h"

#define MAX_BBU_POOL_CORE_MASK  (4)
#ifndef NS_PER_SEC
#define NS_PER_SEC 1E9
#endif
#define MAIN_PRIORITY 98
#define CPU_HZ ticks_per_usec /* us */

struct sample_app_params {
    int num_vfs;
    int num_o_xu;
    char *cfg_file;
    char *usecase_file;
    char vf_pcie_addr[XRAN_PORTS_NUM][XRAN_VF_MAX][32];
};

struct app_sym_cb_ctx {
    int32_t cb_param;
    struct  xran_sense_of_time sense_of_time;
};

static enum app_state state;
static uint64_t  ticks_per_usec;
static volatile uint64_t timer_last_irq_tick = 0;
static uint64_t tsc_resolution_hz = 0;

UsecaseConfig* p_usecaseConfiguration = {NULL};
RuntimeConfig* p_startupConfiguration[XRAN_PORTS_NUM] = {NULL,NULL,NULL,NULL};

struct app_sym_cb_ctx cb_sym_ctx[XRAN_CB_SYM_MAX];

long old_rx_counter[XRAN_PORTS_NUM] = {0,0,0,0};
long old_tx_counter[XRAN_PORTS_NUM] = {0,0,0,0};

static void
app_print_menu()
{
    puts("+---------------------------------------+");
    puts("| Press 1 to start 5G NR XRAN traffic   |");
    puts("| Press 2 reserved for future use       |");
    puts("| Press 3 to quit                       |");
    puts("+---------------------------------------+");
}

uint64_t
app_timer_get_ticks(void)
{
    uint64_t ret;
    union
    {
        uint64_t tsc_64;
        struct
        {
            uint32_t lo_32;
            uint32_t hi_32;
        };
    } tsc;

    __asm volatile("rdtsc" :
             "=a" (tsc.lo_32),
             "=d" (tsc.hi_32));

     ret = ((uint64_t)tsc.tsc_64);
     return ret;
}

//-------------------------------------------------------------------------------------------
/** @ingroup xran
 *
 *  @param   void
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function gets the clock speed of the core and figures out number of ticks per usec.
 *  It is used by l1app and testmac applications to initialize the mlog utility
 *
**/
//-------------------------------------------------------------------------------------------
int32_t
app_timer_set_tsc_freq_from_clock(void)
{
    struct timespec sleeptime = {.tv_nsec = 5E8 }; /* 1/2 second */
    struct timespec t_start, t_end;
    uint64_t tsc_resolution_hz = 0;

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &t_start) == 0) {
        unsigned long ns, end, start = app_timer_get_ticks();
        nanosleep(&sleeptime,NULL);
        clock_gettime(CLOCK_MONOTONIC_RAW, &t_end);
        end = app_timer_get_ticks();
        ns = ((t_end.tv_sec - t_start.tv_sec) * NS_PER_SEC);
        ns += (t_end.tv_nsec - t_start.tv_nsec);

        double secs = (double)ns/NS_PER_SEC;
        tsc_resolution_hz = (unsigned long)((end - start)/secs);

        ticks_per_usec = (tsc_resolution_hz / 1000000);
        printf("System clock (rdtsc) resolution %lu [Hz]\n", tsc_resolution_hz);
        printf("Ticks per us %lu\n", ticks_per_usec);
        return 0;
    }

    return -1;
}

void
app_version_print(void)
{
    char            sysversion[100];
    char           *compilation_date = __DATE__;
    char           *compilation_time = __TIME__;

    uint32_t          nLen;

    snprintf(sysversion, 99, "Version: %s", VERSIONX);
    nLen = strlen(sysversion);

    printf("\n\n");
    printf("===========================================================================================================\n");
    printf("SAMPLE-APP VERSION\n");
    printf("===========================================================================================================\n");

    printf("%s\n", sysversion);
    printf("build-date: %s\n", compilation_date);
    printf("build-time: %s\n", compilation_time);
}

static void
app_help(void)
{
    char help_content[] =  \
            "sample application\n\n"\
            "Usage: sample-app --usecasefile ./usecase_du.cfg --num_eth_vfs 12"\
                                "--vf_addr_o_xu_a \"0000:51:01.0,0000:51:01.1,0000:51:01.2,0000:51:01.3\""\
                                "--vf_addr_o_xu_b \"0000:51:01.4,0000:51:01.5,0000:51:01.6,0000:51:01.7\""\
                                "--vf_addr_o_xu_c \"0000:51:02.0,0000:51:02.1,0000:51:02.2,0000:51:02.3\"\n\n"\
            "or     sample-app --usecasefile ./usecase_du.cfg --num_eth_vfs 2"\
                                "--vf_addr_o_xu_a \"0000:51:01.0,0000:51:01.1\""\
	        "supports the following options:\n\n"\
            "-p | --num_eth_pfs <number of ETH ports to connect to O-RU|O-DU>     2 - default\n"
            "-a | --vf_addr_o_xu_a <list of PCIe Bus Address separated by comma for VFs of O-xU0 >"
            "-b | --vf_addr_o_xu_b <list of PCIe Bus Address separated by comma for VFs of O-xU1 >"
            "-c | --vf_addr_o_xu_c <list of PCIe Bus Address separated by comma for VFs of O-xU2 >"
            "-d | --vf_addr_o_xu_d <list of PCIe Bus Address separated by comma for VFs of O-xU3 >"
            "-u | --usecasefile <name of use case file for multiple O-DU|O-RUs>\n"\
            "-h | --help         print usage\n";

    printf("%s", help_content);
}

/**
 *******************************************************************************
 *
 * @fn    app_parse_args
 * @brief is used to parse incoming app args
 *
 * @description
 *    The routine is parse input args and convert them into app startup params
 *
 * @references
 *
 * @ingroup xran_lib
 *
 ******************************************************************************/
static int32_t
app_parse_cmdline_args(int argc, char ** argv, struct sample_app_params* params)
{
    int32_t ret = 0;
    int32_t c = 0;
    int32_t vf_cnt = 0;
    int32_t *pInt;
    int32_t cnt = 0;
    size_t optlen = 0;
    char *saveptr = NULL;
    char *token = NULL;
    int32_t port = 4;

    static struct option long_options[] = {
        {"cfgfile", required_argument, 0, 'z'},
        {"usecasefile", required_argument, 0, 'u'},
        {"num_eth_vfs", required_argument, 0, 'p'},
        {"vf_addr_o_xu_a", required_argument, 0, 'a'},
        {"vf_addr_o_xu_b", required_argument, 0, 'b'},
        {"vf_addr_o_xu_c", required_argument, 0, 'c'},
        {"vf_addr_o_xu_d", required_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    memset(params, 0, sizeof (*params));

    while (1) {
        //int this_option_optind = optind ? optind : 1;
        int option_index = 0;

        c = getopt_long(argc, argv, "a:b:c:d:f:h:p:u:v", long_options, &option_index);

        if (c == -1)
            break;

        cnt += 1;
        pInt = NULL;
        port = 4;

        switch (c) {
            case 'f':
                params->cfg_file = optarg;
                optlen = strlen(optarg) + 1;
                printf("%s:%d: %s [len %ld]\n",__FUNCTION__, __LINE__, params->cfg_file, optlen);
                break;
            case 'p':
                params->num_vfs = atoi(optarg);
                printf("%s:%d: %d\n",__FUNCTION__, __LINE__, params->num_vfs);
                break;
            case 'u':
                params->usecase_file = optarg;
                optlen = strlen(optarg) + 1;
                printf("%s:%d: %s [len %ld]\n",__FUNCTION__, __LINE__, params->usecase_file, optlen);
                break;
            case 'a':
                port -= 1;
            case 'b':
                port -= 1;
            case 'c':
                port -= 1;
            case 'd':
                port -= 1;
                vf_cnt = 0;
                optlen = strlen(optarg) + 1;
                printf("%s:%d: port %d %s [len %ld]\n",__FUNCTION__, __LINE__, port, optarg, optlen);
                token = strtok_r(optarg, ",", &saveptr);
                while (token != NULL) {
                    optlen = strlen(token) + 1;
                    snprintf(&params->vf_pcie_addr[port][vf_cnt][0], optlen, "%s", token);
                    printf("%s:%d: port %d %s [len %ld]\n",__FUNCTION__, __LINE__, port, &params->vf_pcie_addr[port][vf_cnt][0], optlen);
                    token = strtok_r(NULL, ",", &saveptr);
                    vf_cnt +=1;
                }
                break;
            case 'h':
                app_help();
                exit(0);
        }
    }
    return cnt;
}

int32_t
app_apply_slot_cfg(RuntimeConfig *config)
{
    int32_t ret = 0;
    int32_t slot_idx = 0;
    int32_t cc_idx = 0;
    int32_t ant_idx = 0;
    int32_t section_idx = 0;
    int32_t direction = 0;

    int32_t enable = 0;

    for (slot_idx = 0; slot_idx  < config->numSlots; slot_idx++) {
        for (direction = 0; direction < XRAN_DIR_MAX; direction++) {
            for (cc_idx = 0; cc_idx < config->numCC; cc_idx++) {
                for (ant_idx = 0; ant_idx < ((direction == XRAN_DIR_UL) ? config->numUlAxc :config->numAxc); ant_idx++) {
                    for (section_idx = 0; section_idx < config->p_SlotPrbMap[direction][slot_idx]->nPrbElm && section_idx < XRAN_MAX_SECTIONS_PER_SLOT; section_idx++) {
                        if (config->SlotPrbCCmask[direction][slot_idx][section_idx] & (1L << cc_idx)) {
                            if (config->SlotPrbAntCMask[direction][slot_idx][section_idx] & (1L << ant_idx)) {
                                struct xran_prb_map  *pRbMap = config->p_RunSlotPrbMap[direction][slot_idx][cc_idx][ant_idx];
                                pRbMap->dir = direction;
                                pRbMap->xran_port = config->o_xu_id;
                                pRbMap->band_id = 0;
                                pRbMap->cc_id = cc_idx;
                                pRbMap->ru_port_id = ant_idx;
                                pRbMap->tti_id = slot_idx;
                                pRbMap->start_sym_id = 0;
                                if (pRbMap->nPrbElm < XRAN_MAX_SECTIONS_PER_SLOT && section_idx < XRAN_MAX_SECTIONS_PER_SLOT) {
                                    struct xran_prb_elm *pMapElmRun = &pRbMap->prbMap[pRbMap->nPrbElm];
                                    struct xran_prb_elm *pMapElmCfg = &config->p_SlotPrbMap[direction][slot_idx]->prbMap[section_idx];
                                    memcpy(pMapElmRun, pMapElmCfg, sizeof(struct xran_prb_elm));
            } else {
                                    rte_panic("Incorrect slot cfg\n");
            }
                                pRbMap->nPrbElm++;
                                enable = 1;
        }
    }
}
            }
            }
        }
    }

    config->RunSlotPrbMapEnabled = enable;
    printf("[%d]config->RunSlotPrbMapEnabled %d\n",config->o_xu_id, config->RunSlotPrbMapEnabled);

    return ret;
}

int32_t
app_parse_all_cfgs(struct sample_app_params* p_args, UsecaseConfig* p_use_cfg,  RuntimeConfig* p_o_xu_cfg)
{
    int32_t ret     = 0;
    int32_t vf_num  = 0;
    int32_t o_xu_id = 0;
    char filename[512];
    char *dir;
    size_t len;

    if (p_use_cfg) {
        memset(p_use_cfg, 0, sizeof(UsecaseConfig));
    } else {
        printf("p_use_cfg error.\n");
        exit(-1);
    }

    if (p_o_xu_cfg) {
        int32_t i;
        RuntimeConfig* p_o_xu_cfg_loc = p_o_xu_cfg;
        for (i = 0; i < XRAN_PORTS_NUM; i++) {
            config_init(p_o_xu_cfg_loc);
            p_o_xu_cfg_loc++;
    }
    } else {
        printf("p_o_xu_cfg error.\n");
        exit(-1);
    }

    if (p_args) {
        if (p_args->usecase_file) { /* use case for multiple O-RUs */
            printf("p_args->usecase_file (%s)\n", p_args->usecase_file);
            len = strlen(p_args->usecase_file) + 1;
            if (len > 511){
                printf("app_parse_all_cfgs: Name of p_args->usecase_file, %s is too long.  Maximum is 511 characters!!\n", p_args->usecase_file);
                return -1;
            } else {
                strncpy(filename, p_args->usecase_file, len);
            }
            if (parseUsecaseFile(filename, p_use_cfg) != 0) {
                printf("Use case config file error.\n");
                return -1;
            }
            if (p_use_cfg->oXuNum > XRAN_PORTS_NUM) {
                printf("Use case config file error.\n");
                return -1;
            }

            /* use cmdline pcie address */
            for (o_xu_id = 0; o_xu_id < p_use_cfg->oXuNum && o_xu_id < XRAN_PORTS_NUM; o_xu_id++) {
                for (vf_num = 0; vf_num <  XRAN_VF_MAX && p_args->num_vfs ; vf_num++) {
                    strncpy(&p_use_cfg->o_xu_pcie_bus_addr[o_xu_id][vf_num][0], &p_args->vf_pcie_addr[o_xu_id][vf_num][0], strlen(&p_args->vf_pcie_addr[o_xu_id][vf_num][0]));
                }
            }
            dir = dirname(p_args->usecase_file);
            for (o_xu_id = 0; o_xu_id < p_use_cfg->oXuNum && o_xu_id < XRAN_PORTS_NUM; o_xu_id++) {
                memset(filename, 0, sizeof(filename));
                printf("dir (%s)\n",dir);
                len = strlen(dir) + 1;
                if (len > 511){
                    printf("app_parse_all_cfgs: Name of directory, %s, xu_id = %d is too long.  Maximum is 511 characters!!\n", dir, o_xu_id);
        return -1;
                } else {
                    strncpy(filename, dir, len);
    }
                strncat(filename, "/", 1);
                len +=1;
                len = (sizeof(filename)) - len;

                if (len > strlen(p_use_cfg->o_xu_cfg_file[o_xu_id])) {
                    strncat(filename, p_use_cfg->o_xu_cfg_file[o_xu_id], RTE_MIN (len, strlen(p_use_cfg->o_xu_cfg_file[o_xu_id])));
                } else {
                    printf("File name error\n");
                    return -1;
                }
                printf("cfg_file (%s)\n",filename);
                printf("\n=================== O-XU %d===================\n", o_xu_id);
                if (parseConfigFile(filename, p_o_xu_cfg) != 0) {
                    printf("Configuration file error\n");
                    return -1;
                }
                p_o_xu_cfg->o_xu_id = o_xu_id;
                if (p_o_xu_cfg->SlotNum_fileEnabled) {
                    if (parseSlotConfigFile(dir, p_o_xu_cfg) != 0) {
                        printf("parseSlotConfigFiles\n");
                        return -1;
                    }
                    if (app_apply_slot_cfg(p_o_xu_cfg)!= 0) {
                        printf("app_apply_slot_cfg\n");
                        return -1;
                    }
                }
                p_o_xu_cfg++;
            }
        } else {
            printf("p_args error\n");
            app_help();
        exit(-1);
    }
    } else {
        printf("p_args error\n");
        exit(-1);
    }

    return ret;
}

int32_t
app_setup_o_xu_buffers(UsecaseConfig* p_use_cfg,  RuntimeConfig* p_o_xu_cfg, struct xran_fh_init* p_xran_fh_init)
{
    int32_t ret  = 0;
    int32_t i    = 0;
    int32_t j    = 0;
    char filename[256];
    struct o_xu_buffers *p_iq = NULL;

    if (p_o_xu_cfg->p_buff) {
        p_iq = p_o_xu_cfg->p_buff;
        printf("IQ files size is %d slots\n", p_o_xu_cfg->numSlots);

        p_iq->iq_playback_buffer_size_dl = (p_o_xu_cfg->numSlots * N_SYM_PER_SLOT * N_SC_PER_PRB *
                                    app_xran_get_num_rbs(p_o_xu_cfg->xranTech, p_o_xu_cfg->mu_number,
                                    p_o_xu_cfg->nDLBandwidth, p_o_xu_cfg->nDLAbsFrePointA) *4L);

        p_iq->iq_playback_buffer_size_ul = (p_o_xu_cfg->numSlots * N_SYM_PER_SLOT * N_SC_PER_PRB *
                                    app_xran_get_num_rbs(p_o_xu_cfg->xranTech, p_o_xu_cfg->mu_number,
                                    p_o_xu_cfg->nULBandwidth, p_o_xu_cfg->nULAbsFrePointA) *4L);


    /* 10 * [14*32*273*2*2] = 4892160 bytes */
        p_iq->iq_bfw_buffer_size_dl = (p_o_xu_cfg->numSlots * N_SYM_PER_SLOT *  p_o_xu_cfg->antElmTRx *
                                    app_xran_get_num_rbs(p_o_xu_cfg->xranTech, p_o_xu_cfg->mu_number,
                                    p_o_xu_cfg->nDLBandwidth, p_o_xu_cfg->nDLAbsFrePointA) *4L);

    /* 10 * [14*32*273*2*2] = 4892160 bytes */
        p_iq->iq_bfw_buffer_size_ul = (p_o_xu_cfg->numSlots * N_SYM_PER_SLOT *
                                    app_xran_get_num_rbs(p_o_xu_cfg->xranTech, p_o_xu_cfg->mu_number,
                                    p_o_xu_cfg->nULBandwidth, p_o_xu_cfg->nULAbsFrePointA) *4L);

    /* 10 * [1*273*2*2] = 349440 bytes */
        p_iq->iq_srs_buffer_size_ul = (p_o_xu_cfg->numSlots * N_SYM_PER_SLOT * N_SC_PER_PRB *
                                    app_xran_get_num_rbs(p_o_xu_cfg->xranTech, p_o_xu_cfg->mu_number,
                                    p_o_xu_cfg->nULBandwidth, p_o_xu_cfg->nULAbsFrePointA)*4L);

        for (i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->numAxc); i++) {
            p_iq->p_tx_play_buffer[i]    = (int16_t*)malloc(p_iq->iq_playback_buffer_size_dl);
            p_iq->tx_play_buffer_size[i] = (int32_t)p_iq->iq_playback_buffer_size_dl;

            if (p_iq->p_tx_play_buffer[i] == NULL)
            exit(-1);

            p_iq->tx_play_buffer_size[i] = sys_load_file_to_buff(p_o_xu_cfg->ant_file[i],
                            "DL IFFT IN IQ Samples in binary format",
                                (uint8_t*)p_iq->p_tx_play_buffer[i],
                                p_iq->tx_play_buffer_size[i],
                            1);
            p_iq->tx_play_buffer_position[i] = 0;
    }

        if (p_o_xu_cfg->appMode == APP_O_DU && p_o_xu_cfg->xranCat == XRAN_CATEGORY_B) {
            for (i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->numAxc); i++) {

                p_iq->p_tx_dl_bfw_buffer[i]    = (int16_t*)malloc(p_iq->iq_bfw_buffer_size_dl);
                p_iq->tx_dl_bfw_buffer_size[i] = (int32_t)p_iq->iq_bfw_buffer_size_dl;

                if (p_iq->p_tx_dl_bfw_buffer[i] == NULL)
                exit(-1);

                p_iq->tx_dl_bfw_buffer_size[i] = sys_load_file_to_buff(p_o_xu_cfg->dl_bfw_file[i],
                                "DL BF weights IQ Samples in binary format",
                                    (uint8_t*) p_iq->p_tx_dl_bfw_buffer[i],
                                    p_iq->tx_dl_bfw_buffer_size[i],
                                1);
                p_iq->tx_dl_bfw_buffer_position[i] = 0;
        }
    }

        if (p_o_xu_cfg->appMode == APP_O_DU && p_o_xu_cfg->xranCat == XRAN_CATEGORY_B) {

            for (i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->numAxc); i++) {
                p_iq->p_tx_ul_bfw_buffer[i]    = (int16_t*)malloc(p_iq->iq_bfw_buffer_size_ul);
                p_iq->tx_ul_bfw_buffer_size[i] = (int32_t)p_iq->iq_bfw_buffer_size_ul;

                if (p_iq->p_tx_ul_bfw_buffer[i] == NULL)
                exit(-1);

                p_iq->tx_ul_bfw_buffer_size[i] = sys_load_file_to_buff(p_o_xu_cfg->ul_bfw_file[i],
                                "UL BF weights IQ Samples in binary format",
                                    (uint8_t*) p_iq->p_tx_ul_bfw_buffer[i],
                                    p_iq->tx_ul_bfw_buffer_size[i],
                                1);
                p_iq->tx_ul_bfw_buffer_position[i] = 0;
        }
    }

        if (p_o_xu_cfg->appMode == APP_O_RU && p_o_xu_cfg->enablePrach) {
            for (i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->numAxc); i++) {
                p_iq->p_tx_prach_play_buffer[i]    = (int16_t*)malloc(PRACH_PLAYBACK_BUFFER_BYTES);
                p_iq->tx_prach_play_buffer_size[i] = (int32_t)PRACH_PLAYBACK_BUFFER_BYTES;

                if (p_iq->p_tx_prach_play_buffer[i] == NULL)
                 exit(-1);

                memset(p_iq->p_tx_prach_play_buffer[i], 0, PRACH_PLAYBACK_BUFFER_BYTES);

                p_iq->tx_prach_play_buffer_size[i] = sys_load_file_to_buff(p_o_xu_cfg->prach_file[i],
                                 "PRACH IQ Samples in binary format",
                                    (uint8_t*) p_iq->p_tx_prach_play_buffer[i],
                                    p_iq->tx_prach_play_buffer_size[i],
                                 1);
                p_iq->tx_prach_play_buffer_position[i] = 0;
         }
    }

        if (p_o_xu_cfg->appMode == APP_O_RU && p_o_xu_cfg->enableSrs) {
         for(i = 0;
                i < MAX_ANT_CARRIER_SUPPORTED_CAT_B && i < (uint32_t)(p_o_xu_cfg->numCC  * p_o_xu_cfg->antElmTRx);
             i++) {

                p_iq->p_tx_srs_play_buffer[i]    = (int16_t*)malloc(p_iq->iq_srs_buffer_size_ul);
                p_iq->tx_srs_play_buffer_size[i] = (int32_t)p_iq->iq_srs_buffer_size_ul;

                if (p_iq->p_tx_srs_play_buffer[i] == NULL)
                 exit(-1);

                memset(p_iq->p_tx_srs_play_buffer[i], 0, p_iq->iq_srs_buffer_size_ul);
                p_iq->tx_srs_play_buffer_size[i] = sys_load_file_to_buff(p_o_xu_cfg->ul_srs_file[i],
                                 "SRS IQ Samples in binary format",
                                    (uint8_t*) p_iq->p_tx_srs_play_buffer[i],
                                    p_iq->tx_srs_play_buffer_size[i],
                                 1);

                p_iq->tx_srs_play_buffer_position[i] = 0;
         }
    }

    /* log of ul */
        for (i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->numAxc); i++) {

            p_iq->p_rx_log_buffer[i]    = (int16_t*)malloc(p_iq->iq_playback_buffer_size_ul);
            p_iq->rx_log_buffer_size[i] = (int32_t)p_iq->iq_playback_buffer_size_ul;

            if (p_iq->p_rx_log_buffer[i] == NULL)
            exit(-1);

            p_iq->rx_log_buffer_position[i] = 0;

            memset(p_iq->p_rx_log_buffer[i], 0, p_iq->rx_log_buffer_size[i]);
    }

    /* log of prach */
        for (i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->numAxc); i++) {

            p_iq->p_prach_log_buffer[i]    = (int16_t*)malloc(p_o_xu_cfg->numSlots*XRAN_NUM_OF_SYMBOL_PER_SLOT*PRACH_PLAYBACK_BUFFER_BYTES);
            p_iq->prach_log_buffer_size[i] = (int32_t)p_o_xu_cfg->numSlots*XRAN_NUM_OF_SYMBOL_PER_SLOT*PRACH_PLAYBACK_BUFFER_BYTES;

            if (p_iq->p_prach_log_buffer[i] == NULL)
            exit(-1);

            memset(p_iq->p_prach_log_buffer[i], 0, p_iq->prach_log_buffer_size[i]);
            p_iq->prach_log_buffer_position[i] = 0;
    }

    /* log of SRS */
        if (p_o_xu_cfg->appMode == APP_O_DU && p_o_xu_cfg->enableSrs) {
        for(i = 0;
                i < MAX_ANT_CARRIER_SUPPORTED_CAT_B && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->antElmTRx);
            i++) {

                p_iq->p_srs_log_buffer[i]    = (int16_t*)malloc(p_iq->iq_srs_buffer_size_ul);
                p_iq->srs_log_buffer_size[i] = (int32_t)p_iq->iq_srs_buffer_size_ul;

                if (p_iq->p_srs_log_buffer[i] == NULL)
                 exit(-1);

                memset(p_iq->p_srs_log_buffer[i], 0, p_iq->iq_srs_buffer_size_ul);
                p_iq->srs_log_buffer_position[i] = 0;
    }
    }

        for (i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->numAxc); i++) {

            snprintf(filename, sizeof(filename), "./logs/%s%d-play_ant%d.txt",((p_o_xu_cfg->appMode == APP_O_DU) ? "o-du" : "o-ru"), p_o_xu_cfg->o_xu_id,  i);
        sys_save_buf_to_file_txt(filename,
                            "DL IFFT IN IQ Samples in human readable format",
                                (uint8_t*) p_iq->p_tx_play_buffer[i],
                                p_iq->tx_play_buffer_size[i],
                            1);

            snprintf(filename, sizeof(filename),"./logs/%s%d-play_ant%d.bin",((p_o_xu_cfg->appMode == APP_O_DU) ? "o-du" : "o-ru"), p_o_xu_cfg->o_xu_id, i);
        sys_save_buf_to_file(filename,
                            "DL IFFT IN IQ Samples in binary format",
                                (uint8_t*) p_iq->p_tx_play_buffer[i],
                                p_iq->tx_play_buffer_size[i]/sizeof(short),
                            sizeof(short));


            if (p_o_xu_cfg->appMode == APP_O_DU && p_o_xu_cfg->xranCat == XRAN_CATEGORY_B) {
                snprintf(filename, sizeof(filename),"./logs/%s%d-dl_bfw_ue%d.txt",((p_o_xu_cfg->appMode == APP_O_DU) ? "o-du" : "o-ru"), p_o_xu_cfg->o_xu_id,  i);
            sys_save_buf_to_file_txt(filename,
                                "DL Beamformig weights IQ Samples in human readable format",
                                    (uint8_t*) p_iq->p_tx_dl_bfw_buffer[i],
                                    p_iq->tx_dl_bfw_buffer_size[i],
                                1);

                snprintf(filename, sizeof(filename),"./logs/%s%d-dl_bfw_ue%d.bin",((p_o_xu_cfg->appMode == APP_O_DU) ? "o-du" : "o-ru"),p_o_xu_cfg->o_xu_id, i);
            sys_save_buf_to_file(filename,
                                "DL Beamformig weightsIQ Samples in binary format",
                                    (uint8_t*) p_iq->p_tx_dl_bfw_buffer[i],
                                    p_iq->tx_dl_bfw_buffer_size[i]/sizeof(short),
                                sizeof(short));


                snprintf(filename, sizeof(filename), "./logs/%s%d-ul_bfw_ue%d.txt",((p_o_xu_cfg->appMode == APP_O_DU) ? "o-du" : "o-ru"), p_o_xu_cfg->o_xu_id, i);
            sys_save_buf_to_file_txt(filename,
                                "UL Beamformig weights IQ Samples in human readable format",
                                    (uint8_t*) p_iq->p_tx_ul_bfw_buffer[i],
                                    p_iq->tx_ul_bfw_buffer_size[i],
                                1);

                snprintf(filename, sizeof(filename),"./logs/%s%d-ul_bfw_ue%d.bin",((p_o_xu_cfg->appMode == APP_O_DU) ? "o-du" : "o-ru"), p_o_xu_cfg->o_xu_id, i);
            sys_save_buf_to_file(filename,
                                "UL Beamformig weightsIQ Samples in binary format",
                                    (uint8_t*) p_iq->p_tx_ul_bfw_buffer[i],
                                    p_iq->tx_ul_bfw_buffer_size[i]/sizeof(short),
                                sizeof(short));

        }
    }

        if (p_o_xu_cfg->appMode == APP_O_RU && p_o_xu_cfg->enableSrs && p_o_xu_cfg->xranCat == XRAN_CATEGORY_B) {
       for(i = 0;
            i < MAX_ANT_CARRIER_SUPPORTED_CAT_B && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->antElmTRx);
           i++) {

                snprintf(filename, sizeof(filename), "./logs/%s%d-play_srs_ant%d.txt",((p_o_xu_cfg->appMode == APP_O_DU) ? "o-du" : "o-ru"), p_o_xu_cfg->o_xu_id, i);
            sys_save_buf_to_file_txt(filename,
                            "SRS IQ Samples in human readable format",
                                (uint8_t*)p_iq->p_tx_srs_play_buffer[i],
                                p_iq->tx_srs_play_buffer_size[i],
                            1);

                snprintf(filename,sizeof(filename), "./logs/%s%d-play_srs_ant%d.bin",((p_o_xu_cfg->appMode == APP_O_DU) ? "o-du" : "o-ru"), p_o_xu_cfg->o_xu_id, i);
            sys_save_buf_to_file(filename,
                                "SRS IQ Samples in binary format",
                                    (uint8_t*) p_iq->p_tx_srs_play_buffer[i],
                                    p_iq->tx_srs_play_buffer_size[i]/sizeof(short),
                                sizeof(short));
        }
    }

        if (p_o_xu_cfg->iqswap == 1) {
            for (i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->numAxc); i++) {
            printf("TX: Swap I and Q to match RU format: [%d]\n",i);
            {
                /* swap I and Q */
                int32_t j;
                    signed short *ptr = (signed short *) p_iq->p_tx_play_buffer[i];
                signed short temp;

                    for (j = 0; j < (int32_t)(p_iq->tx_play_buffer_size[i]/sizeof(short)) ; j = j + 2) {
                   temp    = ptr[j];
                   ptr[j]  = ptr[j + 1];
                   ptr[j + 1] = temp;
                }
            }
                if (p_o_xu_cfg->appMode == APP_O_DU && p_o_xu_cfg->xranCat == XRAN_CATEGORY_B) {
                printf("DL BFW: Swap I and Q to match RU format: [%d]\n",i);
                {
                    /* swap I and Q */
                    int32_t j;
                        signed short *ptr = (signed short *) p_iq->p_tx_dl_bfw_buffer[i];
                    signed short temp;

                        for (j = 0; j < (int32_t)(p_iq->tx_dl_bfw_buffer_size[i]/sizeof(short)) ; j = j + 2) {
                       temp    = ptr[j];
                       ptr[j]  = ptr[j + 1];
                       ptr[j + 1] = temp;
                    }
                }
                printf("UL BFW: Swap I and Q to match RU format: [%d]\n",i);
                {
                    /* swap I and Q */
                    int32_t j;
                        signed short *ptr = (signed short *)  p_iq->p_tx_ul_bfw_buffer[i];
                    signed short temp;

                        for (j = 0; j < (int32_t)(p_iq->tx_ul_bfw_buffer_size[i]/sizeof(short)) ; j = j + 2) {
                       temp    = ptr[j];
                       ptr[j]  = ptr[j + 1];
                       ptr[j + 1] = temp;
                    }
                }
            }
        }

            if (p_o_xu_cfg->appMode == APP_O_RU) {
                for (i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->numAxc); i++) {
                printf("PRACH: Swap I and Q to match RU format: [%d]\n",i);
                {
                    /* swap I and Q */
                    int32_t j;
                        signed short *ptr = (signed short *) p_iq-> p_tx_prach_play_buffer[i];
                    signed short temp;

                        for (j = 0; j < (int32_t)(p_iq->tx_prach_play_buffer_size[i]/sizeof(short)) ; j = j + 2) {
                       temp    = ptr[j];
                       ptr[j]  = ptr[j + 1];
                       ptr[j + 1] = temp;
                    }
                }
            }
        }

            if (p_o_xu_cfg->appMode == APP_O_RU) {
            for(i = 0;
                i < MAX_ANT_CARRIER_SUPPORTED_CAT_B && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->antElmTRx);
               i++) {
                 printf("SRS: Swap I and Q to match RU format: [%d]\n",i);
                {
                    /* swap I and Q */
                    int32_t j;
                        signed short *ptr = (signed short *) p_iq->p_tx_srs_play_buffer[i];
                    signed short temp;

                        for (j = 0; j < (int32_t)(p_iq->tx_srs_play_buffer_size[i]/sizeof(short)) ; j = j + 2) {
                       temp    = ptr[j];
                       ptr[j]  = ptr[j + 1];
                       ptr[j + 1] = temp;
                    }
                }
            }
        }
    }

#if 0
        for (i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->numAxc); i++) {

        sprintf(filename, "./logs/swap_IQ_play_ant%d.txt", i);
        sys_save_buf_to_file_txt(filename,
                            "DL IFFT IN IQ Samples in human readable format",
                                (uint8_t*) p_iq->p_tx_play_buffer[i],
                                p_iq->tx_play_buffer_size[i],
                            1);
    }
#endif
        if (p_o_xu_cfg->nebyteorderswap == 1 && p_o_xu_cfg->compression == 0) {
            for (i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->numAxc); i++) {
            printf("TX: Convert S16 I and S16 Q to network byte order for XRAN Ant: [%d]\n",i);
                for (j = 0; j < p_iq->tx_play_buffer_size[i]/sizeof(short); j++) {
                    p_iq->p_tx_play_buffer[i][j]  = rte_cpu_to_be_16(p_iq->p_tx_play_buffer[i][j]);
            }

                if (p_o_xu_cfg->appMode == APP_O_DU && p_o_xu_cfg->xranCat == XRAN_CATEGORY_B) {
                printf("DL BFW: Convert S16 I and S16 Q to network byte order for XRAN Ant: [%d]\n",i);
                    for (j = 0; j < p_iq->tx_dl_bfw_buffer_size[i]/sizeof(short); j++) {
                        p_iq->p_tx_dl_bfw_buffer[i][j]  = rte_cpu_to_be_16(p_iq->p_tx_dl_bfw_buffer[i][j]);
                }
                printf("UL BFW: Convert S16 I and S16 Q to network byte order for XRAN Ant: [%d]\n",i);
                    for (j = 0; j < p_iq->tx_ul_bfw_buffer_size[i]/sizeof(short); j++) {
                        p_iq->p_tx_ul_bfw_buffer[i][j]  = rte_cpu_to_be_16(p_iq->p_tx_ul_bfw_buffer[i][j]);
                }
            }
        }

            if (p_o_xu_cfg->appMode == APP_O_RU && p_o_xu_cfg->enablePrach) {
                for (i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->numAxc); i++) {
                printf("PRACH: Convert S16 I and S16 Q to network byte order for XRAN Ant: [%d]\n",i);
                    for (j = 0; j < p_iq->tx_prach_play_buffer_size[i]/sizeof(short); j++) {
                        p_iq->p_tx_prach_play_buffer[i][j]  = rte_cpu_to_be_16(p_iq->p_tx_prach_play_buffer[i][j]);
                }
            }
        }

            if (p_o_xu_cfg->appMode == APP_O_RU && p_o_xu_cfg->enableSrs) {
               for(i = 0;
                i < MAX_ANT_CARRIER_SUPPORTED_CAT_B && i < (uint32_t)(p_o_xu_cfg->numCC  * p_o_xu_cfg->antElmTRx);
               i++)  {
                printf("SRS: Convert S16 I and S16 Q to network byte order for XRAN Ant: [%d]\n",i);
                    for (j = 0; j < p_iq->tx_srs_play_buffer_size[i]/sizeof(short); j++) {
                        p_iq->p_tx_srs_play_buffer[i][j]  = rte_cpu_to_be_16(p_iq->p_tx_srs_play_buffer[i][j]);
                }
            }
        }

    }

#if 0
        for (i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->numAxc); i++) {

        sprintf(filename, "./logs/swap_be_play_ant%d.txt", i);
        sys_save_buf_to_file_txt(filename,
                            "DL IFFT IN IQ Samples in human readable format",
                                (uint8_t*) p_iq->p_tx_play_buffer[i],
                                p_iq->tx_play_buffer_size[i],
                            1);
    }
#endif
    }

    return ret;
}

int32_t
app_dump_o_xu_buffers(UsecaseConfig* p_use_cfg,  RuntimeConfig* p_o_xu_cfg)
{
    int32_t ret  = 0;
    int32_t i    = 0;
    int32_t j    = 0;
    char filename[256];
    struct o_xu_buffers* p_iq = NULL;

    if (p_o_xu_cfg->p_buff) {
        p_iq = p_o_xu_cfg->p_buff;
    } else {
        printf("Error p_o_xu_cfg->p_buff\n");
        exit(-1);
    }

    if (p_o_xu_cfg->iqswap == 1) {
        for (i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->numAxc); i++) {
            printf("RX: Swap I and Q to match CPU format: [%d]\n",i);
            {
                /* swap I and Q */
                int32_t j;
                signed short *ptr = (signed short *)  p_iq->p_rx_log_buffer[i];
                signed short temp;

                for (j = 0; j < (int32_t)(p_iq->rx_log_buffer_size[i]/sizeof(short)) ; j = j + 2) {
                   temp    = ptr[j];
                   ptr[j]  = ptr[j + 1];
                   ptr[j + 1] = temp;
                }
            }
        }

        if (p_o_xu_cfg->appMode == APP_O_DU && p_o_xu_cfg->enableSrs) {
            for (i = 0;
            i < MAX_ANT_CARRIER_SUPPORTED_CAT_B && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->antElmTRx);
            i++)  {
                printf("SRS: Swap I and Q to match CPU format: [%d]\n",i);
                {
                    /* swap I and Q */
                    int32_t j;
                    signed short *ptr = (signed short *)  p_iq->p_srs_log_buffer[i];
                    signed short temp;

                    for (j = 0; j < (int32_t)(p_iq->srs_log_buffer_size[i]/sizeof(short)) ; j = j + 2) {
                       temp    = ptr[j];
                       ptr[j]  = ptr[j + 1];
                       ptr[j + 1] = temp;
                    }
                }
            }
        }
    }

    if (p_o_xu_cfg->nebyteorderswap == 1 && p_o_xu_cfg->compression == 0) {

        for (i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->numAxc); i++) {
            printf("RX: Convert S16 I and S16 Q to cpu byte order from XRAN Ant: [%d]\n",i);
            for (j = 0; j < p_iq->rx_log_buffer_size[i]/sizeof(short); j++) {
                p_iq->p_rx_log_buffer[i][j]  = rte_be_to_cpu_16(p_iq->p_rx_log_buffer[i][j]);
            }
    }

        if (p_o_xu_cfg->appMode == APP_O_DU && p_o_xu_cfg->enableSrs) {
            for (i = 0;
            i < MAX_ANT_CARRIER_SUPPORTED_CAT_B && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->antElmTRx);
            i++)  {
                printf("SRS: Convert S16 I and S16 Q to cpu byte order from XRAN Ant: [%d]\n",i);
                for (j = 0; j < p_iq->srs_log_buffer_size[i]/sizeof(short); j++) {
                    p_iq->p_srs_log_buffer[i][j]  = rte_be_to_cpu_16(p_iq->p_srs_log_buffer[i][j]);
                }
            }
        }
    }

    for (i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->numAxc); i++) {

        snprintf(filename, sizeof(filename), "./logs/%s%d-rx_log_ant%d.txt",((p_o_xu_cfg->appMode == APP_O_DU) ? "o-du" : "o-ru"), p_o_xu_cfg->o_xu_id, i);
        sys_save_buf_to_file_txt(filename,
                            "UL FFT OUT IQ Samples in human readable format",
                            (uint8_t*) p_iq->p_rx_log_buffer[i],
                            p_iq->rx_log_buffer_size[i],
                            1);

        snprintf(filename, sizeof(filename), "./logs/%s%d-rx_log_ant%d.bin",((p_o_xu_cfg->appMode == APP_O_DU) ? "o-du" : "o-ru"), p_o_xu_cfg->o_xu_id, i);
        sys_save_buf_to_file(filename,
                            "UL FFT OUT IQ Samples in binary format",
                            (uint8_t*) p_iq->p_rx_log_buffer[i],
                            p_iq->rx_log_buffer_size[i]/sizeof(short),
                            sizeof(short));
    }

    if (p_o_xu_cfg->appMode == APP_O_DU && p_o_xu_cfg->enableSrs) {
        for (i = 0;
        i < MAX_ANT_CARRIER_SUPPORTED_CAT_B && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->antElmTRx);
        i++) {
            snprintf(filename, sizeof(filename), "./logs/%s%d-srs_log_ant%d.txt",((p_o_xu_cfg->appMode == APP_O_DU) ? "o-du" : "o-ru"), p_o_xu_cfg->o_xu_id, i);
            sys_save_buf_to_file_txt(filename,
                                "SRS UL FFT OUT IQ Samples in human readable format",
                                (uint8_t*)p_iq-> p_srs_log_buffer[i],
                                p_iq->srs_log_buffer_size[i],
                                1);

            snprintf(filename, sizeof(filename),  "./logs/%s%d-srs_log_ant%d.bin",((p_o_xu_cfg->appMode == APP_O_DU) ? "o-du" : "o-ru"), p_o_xu_cfg->o_xu_id, i);
            sys_save_buf_to_file(filename,
                                "SRS UL FFT OUT IQ Samples in binary format",
                                (uint8_t*) p_iq->p_srs_log_buffer[i],
                                p_iq->srs_log_buffer_size[i]/sizeof(short),
                                sizeof(short));
        }
    }

    if (p_o_xu_cfg->enablePrach) {
        if (p_o_xu_cfg->iqswap == 1) {
            for (i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->numAxc); i++) {
                printf("PRACH: Swap I and Q to match CPU format: [%d]\n",i);
                {
                    /* swap I and Q */
                    int32_t j;
                    signed short *ptr = (signed short *)  p_iq->p_prach_log_buffer[i];
                    signed short temp;

                    for (j = 0; j < (int32_t)(p_iq->prach_log_buffer_size[i]/sizeof(short)) ; j = j + 2) {
                       temp    = ptr[j];
                       ptr[j]  = ptr[j + 1];
                       ptr[j + 1] = temp;
                    }
                }
            }
        }

        if (p_o_xu_cfg->nebyteorderswap == 1 && p_o_xu_cfg->compression == 0) {
            for (i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->numAxc); i++) {
                printf("PRACH: Convert S16 I and S16 Q to cpu byte order from XRAN Ant: [%d]\n",i);
                for (j = 0; j < p_iq->prach_log_buffer_size[i]/sizeof(short); j++) {
                    p_iq->p_prach_log_buffer[i][j]  = rte_be_to_cpu_16(p_iq->p_prach_log_buffer[i][j]);
                }
            }
        }

        for (i = 0; i < MAX_ANT_CARRIER_SUPPORTED && i < (uint32_t)(p_o_xu_cfg->numCC * p_o_xu_cfg->numAxc); i++) {

            if (p_o_xu_cfg->appMode == APP_O_DU)
                snprintf(filename, sizeof(filename), "./logs/%s%d%s_ant%d.txt","o-du",p_o_xu_cfg->o_xu_id,"-prach_log", i);
    else
                snprintf(filename, sizeof(filename), "./logs/%s%d%s_ant%d.txt","o-ru",p_o_xu_cfg->o_xu_id,"-play_prach", i);
            sys_save_buf_to_file_txt(filename,
                                "PRACH IQ Samples in human readable format",
                                (uint8_t*) p_iq->p_prach_log_buffer[i],
                                p_iq->prach_log_buffer_size[i],
                                1);

            if (p_o_xu_cfg->appMode == APP_O_DU)
                snprintf(filename, sizeof(filename), "./logs/%s%d%s_ant%d.bin","o-du",p_o_xu_cfg->o_xu_id,"-prach_log", i);
            else
                snprintf(filename, sizeof(filename), "./logs/%s%d%s_ant%d.bin","o-ru",p_o_xu_cfg->o_xu_id,"-play_prach", i);
            sys_save_buf_to_file(filename,
                                "PRACH IQ Samples in binary format",
                                (uint8_t*) p_iq->p_prach_log_buffer[i],
                                p_iq->prach_log_buffer_size[i]/sizeof(short),
                                sizeof(short));
        }
    }
    return ret;
}

int32_t
app_set_main_core(UsecaseConfig* p_usecase)
{
    struct sched_param sched_param;
    cpu_set_t cpuset;
    int32_t   result = 0;
    memset(&sched_param, 0, sizeof(struct sched_param));
    /* set main thread affinity mask to CPU2 */
    sched_param.sched_priority = 99;
    CPU_ZERO(&cpuset);

    printf("This system has %d processors configured and %d processors available.\n",  get_nprocs_conf(), get_nprocs());

    if (p_usecase->main_core < get_nprocs_conf())
        CPU_SET(p_usecase->main_core, &cpuset);
    else
        return -1;

    if (result = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset))
    {
        printf("pthread_setaffinity_np failed: coreId = 2, result = %d\n",result);
    }
    printf("%s [CPU %2d] [PID: %6d]\n", __FUNCTION__,  sched_getcpu(), getpid());
#if 0
    if ((result = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched_param)))
    {
        printf("priority is not changed: coreId = 2, result = %d\n",result);
    }
#endif
    return result;
}

int32_t
app_alloc_all_cfgs(void)
{
    void * ptr =  NULL;
    RuntimeConfig* p_rt_cfg = NULL;
    int32_t i = 0;

    ptr = _mm_malloc(sizeof(UsecaseConfig), 256);
    if (ptr == NULL) {
        rte_panic("_mm_malloc: Can't allocate %lu bytes\n", sizeof(UsecaseConfig));
    }

    p_usecaseConfiguration = (UsecaseConfig*)ptr;

    ptr = _mm_malloc(sizeof(RuntimeConfig)*XRAN_PORTS_NUM, 256);
    if (ptr == NULL) {
        rte_panic("_mm_malloc: Can't allocate %lu bytes\n", sizeof(RuntimeConfig)*XRAN_PORTS_NUM);
    }
    p_rt_cfg = (RuntimeConfig*)ptr;

    for (i = 0; i < XRAN_PORTS_NUM; i++) {
        p_startupConfiguration[i] = p_rt_cfg++;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int i;
    int j, len;
    int32_t o_xu_id = 0;
    int  lcore_id = 0;
    char filename[256];
    int32_t xret = 0;
    struct stat st = {0};
    uint32_t filenameLength = strlen(argv[1]);
    enum xran_if_state xran_curr_if_state = XRAN_INIT;
    struct sample_app_params arg_params;

    uint64_t nTotalTime;
    uint64_t nUsedTime;
    uint32_t nCoresUsed;
    uint32_t nCoreUsedNum[64];
    float nUsedPercent;

    app_version_print();
    app_timer_set_tsc_freq_from_clock();

    if (xran_is_synchronized() != 0)
        printf("Machine is not synchronized using PTP!\n");
    else
        printf("Machine is synchronized using PTP!\n");

    if (filenameLength >= 256) {
        printf("Config file name input is too long, exiting!\n");
        exit(-1);
    }

    if ((xret = app_alloc_all_cfgs()) < 0) {
        printf("app_alloc_all_cfgs failed %d\n", xret);
        exit(-1);
    }

    if ((xret = app_parse_cmdline_args(argc, argv, &arg_params)) < 0) {
        printf("app_parse_args failed %d\n", xret);
        exit(-1);
    }

    if ((xret = app_parse_all_cfgs(&arg_params, p_usecaseConfiguration, p_startupConfiguration[0])) < 0) {
        printf("app_parse_all_cfgs failed %d\n", xret);
        exit(-1);
    }

    if ((xret = app_set_main_core(p_usecaseConfiguration)) < 0) {
        printf("app_set_main_core failed %d\n", xret);
        exit(-1);
    }

    app_io_xran_if_alloc();

    /* one init for all O-XU */
    app_io_xran_fh_init_init(p_usecaseConfiguration, p_startupConfiguration[0], &app_io_xran_fh_init);

    xret =  xran_init(argc, argv, &app_io_xran_fh_init, argv[0], &app_io_xran_handle);
    if (xret != XRAN_STATUS_SUCCESS) {
        printf("xran_init failed %d\n", xret);
        exit(-1);
    }

    if (app_io_xran_handle == NULL)
        exit(1);

    if (stat("./logs", &st) == -1) {
        mkdir("./logs", 0777);
    }

    /** process all the O-RU|O-DU for use case */
    for (o_xu_id = 0; o_xu_id <  p_usecaseConfiguration->oXuNum;  o_xu_id++) {
        RuntimeConfig* p_o_xu_cfg = p_startupConfiguration[o_xu_id];
        if (o_xu_id == 0)
            app_io_xran_buffers_max_sz_set(p_o_xu_cfg);

        if (p_o_xu_cfg->ant_file[0] == NULL) {
            printf("it looks like test vector for antennas were not provided\n");
            exit(-1);
        }
        if (p_o_xu_cfg->numCC > XRAN_MAX_SECTOR_NR) {
            printf("Number of cells %d exceeds max number supported %d!\n", p_o_xu_cfg->numCC, XRAN_MAX_SECTOR_NR);
            p_o_xu_cfg->numCC = XRAN_MAX_SECTOR_NR;

        }
        if (p_o_xu_cfg->antElmTRx > XRAN_MAX_ANT_ARRAY_ELM_NR) {
            printf("Number of Antenna elements %d exceeds max number supported %d!\n", p_o_xu_cfg->antElmTRx, XRAN_MAX_ANT_ARRAY_ELM_NR);
            p_o_xu_cfg->antElmTRx = XRAN_MAX_ANT_ARRAY_ELM_NR;
        }

        printf("Numm CC %d numAxc %d numUlAxc %d\n", p_o_xu_cfg->numCC, p_o_xu_cfg->numAxc, p_o_xu_cfg->numUlAxc);

        app_setup_o_xu_buffers(p_usecaseConfiguration, p_o_xu_cfg, &app_io_xran_fh_init);

        app_io_xran_fh_config_init(p_usecaseConfiguration, p_o_xu_cfg, &app_io_xran_fh_init, &app_io_xran_fh_config[o_xu_id]);

        xret = xran_open(app_io_xran_handle, &app_io_xran_fh_config[o_xu_id]);
    if(xret != XRAN_STATUS_SUCCESS){
        printf("xran_open failed %d\n", xret);
        exit(-1);
    }

        if (app_io_xran_interface(o_xu_id, p_startupConfiguration[o_xu_id], p_usecaseConfiguration) != 0)
            exit(-1);

        app_io_xran_iq_content_init(o_xu_id, p_startupConfiguration[o_xu_id]);

        if ((xret = xran_reg_physide_cb(app_io_xran_handle, app_io_xran_dl_tti_call_back, NULL, 10, XRAN_CB_TTI)) != XRAN_STATUS_SUCCESS) {
            printf("xran_reg_physide_cb failed %d\n", xret);
            exit(-1);
        }
        if ((xret = xran_reg_physide_cb(app_io_xran_handle, app_io_xran_ul_half_slot_call_back, NULL, 10, XRAN_CB_HALF_SLOT_RX)) != XRAN_STATUS_SUCCESS) {
            printf("xran_reg_physide_cb failed %d\n", xret);
            exit(-1);
        }
        if ((xret = xran_reg_physide_cb(app_io_xran_handle, app_io_xran_ul_full_slot_call_back, NULL, 10, XRAN_CB_FULL_SLOT_RX)) != XRAN_STATUS_SUCCESS) {
            printf("xran_reg_physide_cb failed %d\n", xret);
            exit(-1);
        }
#ifdef TEST_SYM_CBS
        if ((xret = xran_reg_sym_cb(app_io_xran_handle, app_io_xran_ul_custom_sym_call_back,
                                    (void*)&cb_sym_ctx[0].cb_param,
                                    &cb_sym_ctx[0].sense_of_time,
                                    3, XRAN_CB_SYM_RX_WIN_BEGIN)) != XRAN_STATUS_SUCCESS) {
            printf("xran_reg_sym_cb failed %d\n", xret);
            exit(-1);
        }

        if ((xret = xran_reg_sym_cb(app_io_xran_handle, app_io_xran_ul_custom_sym_call_back,
                                    (void*)&cb_sym_ctx[1].cb_param,
                                    &cb_sym_ctx[1].sense_of_time,
                                    3, XRAN_CB_SYM_RX_WIN_END)) != XRAN_STATUS_SUCCESS) {
            printf("xran_reg_sym_cb failed %d\n", xret);
            exit(-1);
        }

        if ((xret = xran_reg_sym_cb(app_io_xran_handle, app_io_xran_ul_custom_sym_call_back,
                                    (void*)&cb_sym_ctx[2].cb_param,
                                    &cb_sym_ctx[2].sense_of_time,
                                    3, XRAN_CB_SYM_TX_WIN_BEGIN)) != XRAN_STATUS_SUCCESS) {
            printf("xran_reg_sym_cb failed %d\n", xret);
            exit(-1);
        }

        if ((xret = xran_reg_sym_cb(app_io_xran_handle, app_io_xran_ul_custom_sym_call_back,
                                    (void*)&cb_sym_ctx[3].cb_param,
                                    &cb_sym_ctx[3].sense_of_time,
                                    3, XRAN_CB_SYM_TX_WIN_END)) != XRAN_STATUS_SUCCESS) {
            printf("xran_reg_sym_cb failed %d\n", xret);
            exit(-1);
        }
#endif
    }

    snprintf(filename, sizeof(filename),"mlog-%s", p_usecaseConfiguration->appMode == 0 ? "o-du" : "o-ru");

    /* MLogOpen(0, 32, 0, 0xFFFFFFFF, filename);*/

    MLogOpen(128, 7, 20000, 0, filename);
    MLogSetMask(0);

    puts("----------------------------------------");
    printf("MLog Info: virt=0x%016lx size=%d\n", MLogGetFileLocation(), MLogGetFileSize());
    puts("----------------------------------------");

    uint64_t nActiveCoreMask[MAX_BBU_POOL_CORE_MASK] = {0};
    uint32_t totalCC =  0;
    nActiveCoreMask[0] = ((1 << app_io_xran_fh_init.io_cfg.timing_core) | app_io_xran_fh_init.io_cfg.pkt_proc_core);
    nActiveCoreMask[1] = app_io_xran_fh_init.io_cfg.pkt_proc_core_64_127;

    for (o_xu_id = 0; o_xu_id <  p_usecaseConfiguration->oXuNum;  o_xu_id++) {
        RuntimeConfig* p_o_xu_cfg = p_startupConfiguration[o_xu_id];
        totalCC += p_o_xu_cfg->numCC;
    }
    MLogAddTestCase(nActiveCoreMask, totalCC);

    fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);

    state = APP_RUNNING;
    printf("Start XRAN traffic\n");
    xran_start(app_io_xran_handle);
    app_print_menu();

    struct xran_common_counters x_counters[XRAN_PORTS_NUM];
    int is_mlog_on = 0;
    for (;;) {
        char input[10];
        sleep(1);
        xran_curr_if_state = xran_get_if_state();

        if (xran_get_common_counters(app_io_xran_handle, &x_counters[0]) == XRAN_STATUS_SUCCESS) {
            for (o_xu_id = 0; o_xu_id <  p_usecaseConfiguration->oXuNum;  o_xu_id++) {
                if (o_xu_id == 0) {
                    xran_get_time_stats(&nTotalTime, &nUsedTime, &nCoresUsed, nCoreUsedNum, 1);
                    nUsedPercent = 0.0;
                    if (nTotalTime) {
            nUsedPercent = ((float)nUsedTime * 100.0) / (float)nTotalTime;
                    }
                    mlog_times.core_total_time += nTotalTime;
                    mlog_times.core_used_time += nUsedTime;

#if 0
                    printf("[nCoresUsed: %d] [MainCore: %d - Util: %5.2f %%]", nCoresUsed, nCoreUsedNum[0], nUsedPercent);
                    if (nCoresUsed > 1) {
                        printf("[Additional Cores: ");
                        for (int nCore = 1; nCore < nCoresUsed; nCore++) {
                            printf("%d ", nCoreUsedNum[nCore]);
                        }
                        printf("]");
                    }
                    printf("\n");
#endif
                }
                printf("[%s%d][rx %7ld pps %7ld kbps %7ld][tx %7ld pps %7ld kbps %7ld] [on_time %ld early %ld late %ld corrupt %ld pkt_dupl %ld Total %ld]\n",
                    ((p_usecaseConfiguration->appMode == APP_O_DU) ? "o-du" : "o-ru"),
                    o_xu_id,
                    x_counters[o_xu_id].rx_counter,
                    x_counters[o_xu_id].rx_counter-old_rx_counter[o_xu_id],
                    x_counters[o_xu_id].rx_bytes_per_sec*8/1000L,
                    x_counters[o_xu_id].tx_counter,
                    x_counters[o_xu_id].tx_counter-old_tx_counter[o_xu_id],
                    x_counters[o_xu_id].tx_bytes_per_sec*8/1000L,
                    x_counters[o_xu_id].Rx_on_time,
                    x_counters[o_xu_id].Rx_early,
                    x_counters[o_xu_id].Rx_late,
                    x_counters[o_xu_id].Rx_corrupt,
                    x_counters[o_xu_id].Rx_pkt_dupl,
                    x_counters[o_xu_id].Total_msgs_rcvd);

                if (x_counters[o_xu_id].rx_counter > old_rx_counter[o_xu_id])
                    old_rx_counter[o_xu_id] = x_counters[o_xu_id].rx_counter;
                if (x_counters[o_xu_id].tx_counter > old_tx_counter[o_xu_id])
                    old_tx_counter[o_xu_id] = x_counters[o_xu_id].tx_counter;

                if(o_xu_id == 0){
                    if(is_mlog_on == 0 && x_counters[o_xu_id].rx_counter > 0 && x_counters[o_xu_id].tx_counter > 0) {
                        xran_set_debug_stop(p_startupConfiguration[0]->debugStop, p_startupConfiguration[0]->debugStopCount);
                MLogSetMask(0xFFFFFFFF);
                        is_mlog_on =  1;
                    }
                }
            }
        } else {
            printf("error xran_get_common_counters\n");
        }

        if (xran_curr_if_state == XRAN_STOPPED){
            break;
        }
        if (NULL == fgets(input, 10, stdin)) {
            continue;
        }

        const int sel_opt = atoi(input);
        switch (sel_opt) {
            case 1:
                xran_start(app_io_xran_handle);
                printf("Start XRAN traffic\n");
                break;
            case 2:
                break;
            case 3:
                xran_stop(app_io_xran_handle);
                printf("Stop XRAN traffic\n");
                state = APP_STOPPED;
                break;
            default:
                puts("Wrong option passed!");
                break;
        }
        if (APP_STOPPED == state)
            break;
    }

    /** process all the O-RU|O-DU for use case */
    for (o_xu_id = 0; o_xu_id <  p_usecaseConfiguration->oXuNum;  o_xu_id++) {
        app_io_xran_iq_content_get(o_xu_id, p_startupConfiguration[o_xu_id]);
        /* Check for owd results */
        if (p_usecaseConfiguration->owdmEnable)
            {

            FILE *file= NULL;
            uint64_t avgDelay =0;
            snprintf(filename, sizeof(filename), "./logs/%s%d-owd_results.txt", ((p_startupConfiguration[o_xu_id]->appMode == APP_O_DU)?"o-du":"o-ru"),o_xu_id);
            file = fopen(filename, "w");
            if (file == NULL) {
                printf("can't open file %s\n",filename);
                exit (-1);
        }
            if (xran_get_delay_measurements_results (app_io_xran_handle, (uint16_t) p_startupConfiguration[o_xu_id]->o_xu_id,  p_usecaseConfiguration->appMode, &avgDelay))
                {
                fprintf(file,"OWD Measurements failed for port %d and appMode %d \n", p_startupConfiguration[o_xu_id]->o_xu_id,p_usecaseConfiguration->appMode);
        }
            else
                {
                fprintf(file,"OWD Measurements passed for port %d and appMode %d with AverageDelay %lu [ns]\n", p_startupConfiguration[o_xu_id]->o_xu_id,p_usecaseConfiguration->appMode, avgDelay);
                }
            fflush(file);
            fclose(file);
            }
        }


    puts("Closing l1 app... Ending all threads...");

    xran_close(app_io_xran_handle);
    if(is_mlog_on) {
        app_profile_xran_print_mlog_stats(arg_params.usecase_file);
        rte_pause();
        }
    app_io_xran_if_stop();

    puts("Dump IQs...");
    for (o_xu_id = 0; o_xu_id <  p_usecaseConfiguration->oXuNum;  o_xu_id++) {
        app_dump_o_xu_buffers(p_usecaseConfiguration,  p_startupConfiguration[o_xu_id]);
    }

    app_io_xran_if_free();
    return 0;
}
