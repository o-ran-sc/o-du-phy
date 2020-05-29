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

#ifndef __WLS_DRV_H__
#define __WLS_DRV_H__

#include <linux/types.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include "ttypes.h"

struct wls_dev_t {
    struct mutex lock;
    dev_t dev_no;
    struct class * class;
    struct device * device;
    struct cdev cdev;
    wait_queue_head_t queue;
    atomic_t qwake;
    struct task_struct * thread;
    wls_drv_ctx_t* pWlsDrvCtx;
};

#endif /* __WLS_DRC_H__*/

