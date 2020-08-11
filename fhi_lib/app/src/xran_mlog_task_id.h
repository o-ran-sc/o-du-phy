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
 * @brief This file has the System Debug Trace Logger (Mlog) Task IDs used by PHY
 * @file mlog_task_id.h
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#ifndef _XRAN_TASK_ID_H_
#define _XRAN_TASK_ID_H_

#ifdef __cplusplus
extern "C" {
#endif

#define RESOURCE_CORE_0                             0
#define RESOURCE_CORE_1                             1
#define RESOURCE_CORE_2                             2
#define RESOURCE_CORE_3                             3
#define RESOURCE_CORE_4                             4
#define RESOURCE_CORE_5                             5
#define RESOURCE_CORE_6                             6
#define RESOURCE_CORE_7                             7
#define RESOURCE_CORE_8                             8
#define RESOURCE_CORE_9                             9
#define RESOURCE_CORE_10                            10
#define RESOURCE_CORE_11                            11
#define RESOURCE_CORE_12                            12
#define RESOURCE_CORE_13                            13
#define RESOURCE_CORE_14                            14
#define RESOURCE_CORE_15                            15
#define RESOURCE_CORE_16                            16

#define RESOURCE_IA_CORE                            100

//--------------------------------------------------------------------
// XRAN APP
//--------------------------------------------------------------------

#define PID_GNB_PROC_TIMING                             70
#define PID_GNB_PROC_TIMING_TIMEOUT                     71
#define PID_GNB_SYM_CB                                  72
#define PID_GNB_PRACH_CB                                73
#define PID_GNB_SRS_CB                                  74


#ifdef __cplusplus
}
#endif

#endif /* _XRAN_TASK_ID_H_ */

