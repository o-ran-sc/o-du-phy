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
 * @brief
 * @file
 * @ingroup
 * @author Intel Corporation
 **/

#ifndef _SAMPLEAPP__DEBUG_H_
#define _SAMPLEAPP__DEBUG_H_

#include <stdio.h>

#include "config.h"

#define MAX_FILE_NAME_LEN (512)
#define MAX_PATH_NAME_LEN (1024)

#ifdef _DEBUG
    #define log_dbg(fmt, ...)                       \
        fprintf(stderr,                     \
            "DEBUG: %s(%d): " fmt "\n",             \
            __FILE__,                       \
            __LINE__, ##__VA_ARGS__)
#else
    #define log_dbg(fmt, ...)
#endif

#if defined(_DEBUG) || defined(_VERBOSE)
    #define log_wrn(fmt, ...)                               \
        fprintf(                                            \
            stderr,                                     \
            "WARNING: %s(%d): " fmt "\n",                   \
            __FILE__,                                       \
            __LINE__, ##__VA_ARGS__)
#else
    #define log_dbg(fmt, ...)
    #define log_wrn(fmt, ...)
#endif


#define log_err(fmt, ...)                       \
    fprintf(stderr,                     \
        "ERROR: %s(%d): " fmt "\n",             \
        __FILE__,                       \
        __LINE__, ##__VA_ARGS__)


inline void ShowData(void* ptr, unsigned int size)
{
    uint8_t *d =  (uint8_t *)ptr;
    unsigned int i;

    for(i = 0; i < size; i++)
    {
        if ( !(i & 0xf) )
            printf("\n");
        printf("%02x ", d[i]);
    }
    printf("\n");
}


#endif /* _SAMPLEAPP__DEBUG_H_ */
