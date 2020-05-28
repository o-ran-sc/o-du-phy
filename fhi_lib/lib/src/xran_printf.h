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
 * @brief Modules provide debug prints and utility functions
 * @file xran_printf.h
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#ifndef XRAN_PRINTF_H
#define XRAN_PRINTF_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <stdlib.h>

#define PRINTF_LOG_OK
#define PRINTF_INF_OK
#define PRINTF_ERR_OK
//#define PRINTF_DBG_OK

#ifndef WIN32
#ifdef PRINTF_LOG_OK
#define print_log(fmt, args...) printf("%s:" fmt "\n", __FUNCTION__, ## args)
#else  /* PRINTF_LOG_OK */
#define print_log(fmt, args...)
#endif  /* PRINTF_LOG_OK */
#else
#define print_log(fmt, ...) printf("%s:" fmt "\n", __FUNCTION__, __VA_ARGS__)
#endif

#ifndef WIN32
#ifdef PRINTF_DBG_OK
#define print_dbg(fmt, args...) printf("%s:[dbg] " fmt "\n", __FUNCTION__, ## args)
#else  /* PRINTF_LOG_OK */
#define print_dbg(fmt, args...)
#endif  /* PRINTF_LOG_OK */
#else
#define print_dbg(fmt, ...) printf("%s:[dbg] " fmt "\n", __FUNCTION__, __VA_ARGS__)
#endif

#ifndef WIN32
#ifdef PRINTF_ERR_OK
#define print_err(fmt, args...) printf("%s:[err] " fmt "\n", __FUNCTION__, ## args)
#else  /* PRINTF_LOG_OK */
#define print_err(fmt, args...)
#endif  /* PRINTF_LOG_OK */
#else
#define print_err(fmt, ...) printf("%s:[err] " fmt "\n", __FUNCTION__, __VA_ARGS__)
#endif

#ifndef WIN32
#ifdef PRINTF_INF_OK
#define print_inf               printf
#else  /* PRINTF_LOG_OK */
#define print_inf
#endif  /* PRINTF_LOG_OK */
#else
#define print_inf               printf
#endif

#ifdef __cplusplus
}
#endif

#ifndef _IASSERT_
#define _IASSERT_

#ifdef _DEBUG
#define iAssert(p) if(!(p)){fprintf(stderr,\
    "Assertion failed: %s, file %s, line %d, val %d\n",\
    #p, __FILE__, __LINE__, p);exit(-1);}
#else /* _DEBUG */
#define iAssert(p)
#endif /* _DEBUG */

#ifndef PHY_APP
#ifndef _assert
#define _assert(x)
#endif
#endif

#endif /* _IASSERT_*/

#ifdef CHECK_PARAMS
#define CHECK_NOT_NULL(param, returnValue)      \
if (param == NULL)                          \
{                                           \
    print_err("%s is NULL!\n", #param);   \
    return returnValue;                     \
}
#else
#define CHECK_NOT_NULL(param, returnValue)
#endif

#ifdef __cplusplus
}
#endif

#endif // PHY_PRINTF_H
