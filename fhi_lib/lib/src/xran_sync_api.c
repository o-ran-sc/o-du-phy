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
 * @brief This file provides implementation of synchronization related APIs (PTP/1588)
 *        for XRAN.
 *
 * @file xran_sync_api.c
 * @ingroup group_lte_source_xran
 * @author Intel Corporation
 *
 **/

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xran_sync_api.h"
#include "xran_printf.h"

#define BUF_LEN 256
#define PROC_DIR "/proc"
#define COMM_FILE "comm"
#define PMC_CMD "pmc -u -b 0 'GET PORT_DATA_SET'"
#define PTP4L_PROC_NAME "ptp4l"
#define PHC2SYS_PROC_NAME "phc2sys"

static int find_substr(const char *str, const unsigned int str_len,
    const char *substr, const unsigned int substr_len)
{
    assert(str);
    assert(substr);
    unsigned int ind = 0;

    while (ind + substr_len <= str_len) {
        if (0 == strncmp(&str[ind], substr, substr_len))
            return 0;
        ++ind;
    }
    return 1;
}

static int is_process_running(char *pname)
{
    char full_path[BUF_LEN] = {0};
    char read_proc_name[BUF_LEN] = {0};
    int res = 1;
    DIR *dir = opendir(PROC_DIR);
    if (NULL == dir) {
        return 1;
    }

    struct dirent *entry = NULL;
    while ((entry = readdir(dir))) {
        long pid = atol(entry->d_name);
        if (0 == pid)
            continue;
        snprintf(full_path, sizeof(full_path), "%s/%ld/%s", PROC_DIR, pid, COMM_FILE);
        FILE *proc_name_file = fopen(full_path, "r");
        if (NULL == proc_name_file)
            continue;
        fgets( read_proc_name, BUF_LEN, proc_name_file);
        if (0 == strncmp(read_proc_name, pname, strlen(pname))) {
            res = 0;
            fclose(proc_name_file);
            break;
        }
        fclose(proc_name_file);
    }
    closedir(dir);
    return res;
}

static int check_ptp_status()
{
    char pmc_out_line[BUF_LEN];
    const char *keywords[2] = {"portState", "SLAVE"};
    int res = 1;
    FILE *pmc_pipe = popen(PMC_CMD, "r");
    if (NULL == pmc_pipe)
        return 1;

    while(fgets(pmc_out_line, BUF_LEN, pmc_pipe)) {
        if (0 == find_substr(pmc_out_line, strlen(pmc_out_line), keywords[0],
            strlen(keywords[0]))) {
            if (0 == find_substr(pmc_out_line, strlen(pmc_out_line),
                keywords[1], strlen(keywords[1]))) {
                res = 0;
                break;
            }
        }
    }
    fclose(pmc_pipe);
    return res;
}

int xran_is_synchronized()
{
    int res = 0;
    res |= is_process_running(PTP4L_PROC_NAME);
    print_dbg("PTP4L_PROC_NAME %d\n", res);
    res |= is_process_running(PHC2SYS_PROC_NAME);
    print_dbg("PHC2SYS_PROC_NAME %d\n", res);
    res |= check_ptp_status();
    print_dbg("check_ptp_status %d\n", res);
    return res;
}
