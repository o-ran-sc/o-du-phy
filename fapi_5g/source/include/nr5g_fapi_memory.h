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
 * @file This file consist of fapi memory macro.
 *
 **/

#ifndef NR5G_FAPI_MEM_H_
#define NR5G_FAPI_MEM_H_

#define NR5G_FAPI_MEMCPY(d, x, s, n) nr5g_fapi_memcpy_bound_check(d, x, s, n)
#define NR5G_FAPI_MEMSET(s, x, c, n) nr5g_fapi_memset_bound_check(s, x, c, n)
#define NR5G_FAPI_STRCPY(d, x, s, n) nr5g_fapi_strcpy_bound_check(d, x, s, n)

inline uint8_t nr5g_fapi_memcpy_bound_check(
    void *d,
    size_t x,
    const void *s,
    size_t n);
inline uint8_t nr5g_fapi_memset_bound_check(
    void *s,
    size_t x,
    const int32_t c,
    size_t n);
inline uint8_t nr5g_fapi_strcpy_bound_check(
    char *d,
    size_t x,
    const char *s,
    size_t n);

#endif                          // NR5G_FAPI_MEM_H_
