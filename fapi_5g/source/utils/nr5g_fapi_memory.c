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
 * @file This file defines all the functions used for 
 * memory operations. 
 *
 **/

#include <rte_memcpy.h>
#include <string.h>
#include "nr5g_fapi_wls.h"

inline uint8_t nr5g_fapi_memcpy_bound_check(
    void *d,
    size_t x,
    const void *s,
    size_t n)
{
    // Memory block size and destination/source boundary check
    if ((n >= (MSG_MAXSIZE)) || (n > x)) {
        printf
            ("[MEMCPY FAILED] %s : %d : max block size: %d, memcpy dst size: %ld, memcpy src size: %ld\n",
            __func__, __LINE__, MSG_MAXSIZE, x, n);
        return FAILURE;
    }
    // Overlap check: source pointer overruns destination pointer
    if ((char *)s < (char *)d) {
        if ((((char *)s + x) >= (char *)d) || (((char *)s + n) >= (char *)d)) {
            printf("[MEMCPY FAILED] %s : %d : max block size: %d, \
                    memcpy dst size: %ld, strcpy src size: %ld, \
                    Source pointer %p overlaps into destination pointer %p\n", __func__, __LINE__, MSG_MAXSIZE, x, n, s, d);
            return FAILURE;
        }
    }
    // Overlap check: destination pointer overruns source pointer
    if ((char *)d < (char *)s) {
        if ((((char *)d + x) >= (char *)s) || (((char *)d + n) >= (char *)s)) {
            printf("[MEMCPY FAILED] %s : %d : max block size: %d, \
                    memcpy dst size: %ld, strcpy src size: %ld, \
                    Destination pointer %p overlaps into source pointer %p\n", __func__, __LINE__, MSG_MAXSIZE, x, n, d, s);
            return FAILURE;
        }
    }
    rte_memcpy(d, s, n);
    return SUCCESS;
}

inline uint8_t nr5g_fapi_memset_bound_check(
    void *s,
    size_t x,
    int32_t c,
    size_t n)
{
    // Memory block size and destination/source boundary check
    if ((n >= MSG_MAXSIZE) || (n > x)) {
        printf
            ("[MEMSET FAILED] %s : %d : max block size: %d, memset dst size: %ld, memset src size: %ld\n",
            __func__, __LINE__, MSG_MAXSIZE, x, n);
        return FAILURE;
    }
    memset(s, c, n);
    return SUCCESS;
}

inline uint8_t nr5g_fapi_strcpy_bound_check(
    char *d,
    size_t x,
    const char *s,
    size_t n)
{
    // Memory block size and destination/source boundary check
    if ((n >= MSG_MAXSIZE) || (n > x)) {
        printf("[STRNCPY FAILED] %s : %d : max block size: %d, \
            strcpy dst size: %ld, strcpy src size: %ld\n", __func__, __LINE__, MSG_MAXSIZE, x, n);
        return FAILURE;
    }
    // Overlap check: source pointer overruns destination pointer
    if ((char *)s < (char *)d) {
        if ((((char *)s + x) >= (char *)d) || (((char *)s + n) >= (char *)d)) {
            printf("[STRNCPY FAILED] %s : %d : max block size: %d, \
                    strcpy dst size: %ld, strcpy src size: %ld, \
                    Source pointer %p overlaps into destination pointer %p\n", __func__, __LINE__, MSG_MAXSIZE, x, n, s, d);
            return FAILURE;
        }
    }
    // Overlap check: destination pointer overruns source pointer
    if ((char *)d < (char *)s) {
        if ((((char *)d + x) >= (char *)s) || (((char *)d + n) >= (char *)s)) {
            printf("[STRNCPY FAILED] %s : %d : max block size: %d, \
                    strcpy dst size: %ld, strcpy src size: %ld, \
                    Destination pointer %p overlaps into source pointer %p\n", __func__, __LINE__, MSG_MAXSIZE, x, n, d, s);
            return FAILURE;
        }
    }
    strncpy(d, s, n);
    return SUCCESS;
}
