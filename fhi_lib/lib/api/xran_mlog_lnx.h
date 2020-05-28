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

#ifndef _XRAN_MLOG_LNX_H_
#define _XRAN_MLOG_LNX_H_

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef MLOG_ENABLED
#include <mlog_lnx.h>
#else

/* stubs for MLOG functions */
#define MLOG_FALSE                  ( 0 )

#define MLogOpen(a, b, c, d, e)     MLOG_FALSE
#define MLogRestart(a)              MLOG_FALSE
#define MLogPrint(a)                MLOG_FALSE
#define MLogGetFileLocation()       NULL
#define MLogGetFileSize()           0
#define MLogSetMask(a)              MLOG_FALSE
#define MLogGetMask()
#define MLogRegisterTick()
#define MLogTick()                  0
#define MLogIncrementCounter()      0
#define MLogTask(w,x,y)             0
#define MLogTaskCore(w,x,y,z)       0
#define MLogMark(x,y)
#define MLogDevInfo(x)
#define MLogRegisterFrameSubframe(x,y)
#define MLogAddVariables(x,y,z)
#define MLogGetStats(a, b, c, d, e) MLOG_FALSE
#define MLogGetAvgStats(a, b, c, d) MLOG_FALSE
#define MLogAddTestCase(a, b)       MLOG_FALSE
#define MLogAddPowerStats(a, b, c, d, e) MLOG_FALSE

#endif /* MLOG_ENABLED */

#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */

#endif  /* #ifndef _XRAN_MLOG_LNX_H_ */

