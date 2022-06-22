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
 * @brief This file has utilities to parse the parameters passed in through the command
 *        line and configure the application based on these parameters
 * @file aux_cline.h
 * @ingroup xran
 * @author Intel Corporation
 **/

#ifndef _AUX_CLINE_H_
#define _AUX_CLINE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define AUX_SUCCESS 0
#define AUX_FAILURE 1

int         cline_init (void);
uint32_t    cline_set_int(const char *name, int *value, int deflt);
uint32_t    cline_set_uint64(const char *name, uint64_t *value, uint64_t deflt);
uint32_t    cline_covert_hex_2_dec(char *pStr, uint64_t *pDst);
uint32_t    cline_set_thread_info(const char *name, uint64_t *core, int *priority, int *sched);
uint32_t    cline_set_int_array(const char *name, int maxLen, int *dataOut, int *outLen);
uint32_t    cline_set_str(const char *name, char *value, const char *deflt);
int         cline_parse_line(char *pString);
void        cline_print_info(void);
uint32_t    cline_get_string(int argc, char *argv[], char* pString, char *pDest);

#ifdef __cplusplus
}
#endif

#endif /*_AUX_CLINE_H_*/

