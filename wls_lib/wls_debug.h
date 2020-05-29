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

#ifndef __WLS_DEBUG_H__
#define __WLS_DEBUG_H__

#include <linux/types.h>

/* mlog specific defenitions */

#define PID_WLS_DRV_IOC_WAIT_WAKE_UP           77000
#define PID_WLS_DRV_IOC_WAIT_WAKE_ENTRY        77001
#define PID_WLS_DRV_IOC_PUT                    77002
#define PID_WLS_DRV_ISR                        77003
#define PID_WLS_DRV_IOC_FILL                   77004

#define MLOG_VAR_MSG_BLOCK                     0xDEAD7000
#define MLOG_VAR_MSG_TIME_SYNC                 0xDEAD7001
#define MLOG_VAR_MSG_COPY_THREAD                   0xDEAD7002


#endif /* __WLS_DEBUG_H__*/

