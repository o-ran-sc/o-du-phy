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

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/kthread.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <asm/uaccess.h>

#include "wls.h"
#include "wls_drv.h"
#include "wls_debug.h"

#if defined(_MLOG_TRACE_)
#include "mlog.h"
#endif

#define WLS_VERSION_X           0
#define WLS_VERSION_Y           0
#define WLS_VERSION_Z           3
#define WLS_VERSION_RESERVED    0
#define WLS_DRV_VERSION        ((WLS_VERSION_X << 24) | (WLS_VERSION_Y << 16) | (WLS_VERSION_Z << 8) | WLS_VERSION_RESERVED)

#define WLS_DRV_VERSION_FORMAT "%d.%d.%d"
#define WLS_DEV_DEVICE_FORMAT "wls%d"


#define WLS_SEMA_COUNT 32
#define WLS_MAX_CLIENTS 8


typedef struct wls_us_priv_s
{
    wls_sema_priv_t   sema;

    U8                NeedToWakeUp;
    U8                isWait;

    U32               pid;
} wls_us_priv_t;

char wls_driver_name[] = "wls";
char wls_driver_version[10];
char wls_dev_device_name[10];

static long wls_ioctl(struct file * filp, unsigned int cmd, unsigned long arg);
static int  wls_open(struct inode * inode, struct file * filp);
static int  wls_release(struct inode * inode, struct file * filp);
static int  wls_mmap(struct file * filp, struct vm_area_struct * vma);
static int  wls_wait(wls_sema_priv_t *priv, unsigned long arg);
static int  wls_wake_up_user_thread(char *buf, wls_sema_priv_t *semap);

static struct file_operations wls_fops = {
    .owner = THIS_MODULE,
    .open = wls_open,
    .release = wls_release,
    .unlocked_ioctl = wls_ioctl,
    .compat_ioctl = wls_ioctl,
    .mmap = wls_mmap,
};

static struct wls_dev_t*  wls_dev[WLS_MAX_CLIENTS];
static wls_drv_ctx_t    wls_drv_ctx[WLS_MAX_CLIENTS];

static struct class * wls_class;

/**********************************************************************
*                      Module Parameters                              *
**********************************************************************/
int     wlsMaxClients               = 1;


/**********************************************************************/
module_param(wlsMaxClients,             int, S_IRUSR);

/**********************************************************************/

static wls_drv_ctx_t * wls_get_ctx(unsigned int id)
{
    if(id < WLS_MAX_CLIENTS)
        return &wls_drv_ctx[id];
    else
        return NULL;
}

int wls_wake_up_user_thread(char *buf, wls_sema_priv_t *semap)
{
    if (likely(atomic_read(&semap->is_irq) < FIFO_LEN)) {
        unsigned int put = semap->drv_block_put + 1;
        if (put >= FIFO_LEN)
            put = 0;
        //copy data from user
        memcpy(&semap->drv_block[put], buf, sizeof(wls_wait_req_t));

        semap->drv_block_put = put;
        atomic_inc(&semap->is_irq);
#ifdef DEBUG
        printk(KERN_INFO "[wls]:PUT: put=%d get=%d T=%lu is_irq=%d\n",
               semap->drv_block_put, semap->drv_block_get,
               semap->drv_block[put].start_time , atomic_read(&semap->is_irq));
#endif /* DEBUG */
        wake_up_interruptible(&semap->queue);
    }

    return 0;
}

void wls_show_data(void* ptr, unsigned int size)
{
    unsigned char *d = ptr;
    int i;

    for(i = 0; i < size; i++)
    {
        if ( !(i & 0xf) )
            printk("\n");
        printk("%02x ", d[i]);
    }
    printk("\n");
}

static int wls_open(struct inode * inode, struct file * filp)
{
    if((MINOR(inode->i_rdev ) >= 0) && (MINOR(inode->i_rdev) < wlsMaxClients) && (MINOR(inode->i_rdev ) < WLS_MAX_CLIENTS)){
        filp->private_data = (void *)wls_dev[MINOR(inode->i_rdev)];
        WLS_DEBUG("wls_open [%d] priv: 0x%p",MINOR(inode->i_rdev), filp->private_data);
        WLS_DEBUG("wls_open PID [%d] ", current->pid);
    } else {
        WLS_ERROR("wls_open PID [%d] incorrect  inode->i_rdev %d", current->pid, MINOR(inode->i_rdev));
    }

    return 0;
}

static int wls_release(struct inode * inode, struct file * filp)
{
    struct wls_dev_t* wls_loc = NULL;
    wls_us_ctx_t*     pUsCtx   = NULL;
    wls_us_priv_t*    pUs_priv = NULL;
    wls_drv_ctx_t*    pDrv_ctx = NULL;
    int i = 0;

    WLS_DEBUG("priv: 0x%p", filp->private_data);
    WLS_DEBUG("wls_release PID [%d] ",current->pid);

    if((MINOR(inode->i_rdev ) >= 0) && (MINOR(inode->i_rdev) < wlsMaxClients) && (MINOR(inode->i_rdev ) < WLS_MAX_CLIENTS)){
        if (filp->private_data != NULL) {
            wls_loc = (struct wls_dev_t*)filp->private_data;
            if((void *)wls_dev[MINOR(inode->i_rdev)] == (void *)wls_loc){
                pDrv_ctx = (wls_drv_ctx_t*)wls_loc->pWlsDrvCtx;
                if(pDrv_ctx){
                    for(i = 0; i < 2; i++ ){
                        pUsCtx =  (wls_us_ctx_t*)pDrv_ctx->p_wls_us_ctx[i];
                        if(pUsCtx){
                            wls_us_ctx_t* dst = (wls_us_ctx_t*)pUsCtx->dst_kernel_va;
                            wls_wait_req_t drv_block;
                            if(dst){
                                wls_us_priv_t* pDstPriv = (wls_us_priv_t*)dst->wls_us_private;
                                if(pDstPriv){
                                    drv_block.start_time = wls_rdtsc();
                                    pDstPriv->NeedToWakeUp = 1;
                                    wls_wake_up_user_thread((char *)&drv_block, &pDstPriv->sema);
                                }
                            }
                            //un-link ctx
                            pDrv_ctx->p_wls_us_ctx[i]->dst_kernel_va = (uint64_t)0;
                            pDrv_ctx->p_wls_us_ctx[i]->dst_user_va = (uint64_t)0;
                            pDrv_ctx->p_wls_us_ctx[i]->dst_pa = (uint64_t)0;
                        }
                    }

                    for(i = 0; i < 2; i++ ){
                        pUsCtx =  (wls_us_ctx_t*)pDrv_ctx->p_wls_us_ctx[i];

                        if(pUsCtx){
                            pUs_priv = (wls_us_priv_t*)pUsCtx->wls_us_private;
                            if(pUs_priv){
                                if(pUs_priv->pid == current->pid){
                                    if (pUs_priv->isWait == 0){
                                            pUsCtx->wls_us_private = NULL;
                                            kfree(pUs_priv);
                                            pDrv_ctx->p_wls_us_ctx[i]    = NULL;
                                            dma_free_coherent(wls_loc->device, sizeof(wls_us_ctx_t),(void*)pUsCtx, (long)pDrv_ctx->p_wls_us_pa_ctx[i]);
                                            pDrv_ctx->p_wls_us_pa_ctx[i] = NULL;
                                            pDrv_ctx->nWlsClients--;
                                    } else {
                                        WLS_PRINT("Wait is in process\n");
                                    }
                                }
                            }
                        }
                    }
                }
            }
            filp->private_data = NULL;
        }
    } else {
        WLS_ERROR("wls_release PID [%d] incorrect  inode->i_rdev %d", current->pid, MINOR(inode->i_rdev));
    }

    return 0;
}

static int wls_wait(wls_sema_priv_t *priv, unsigned long arg)
{
    char __user *buf = (char __user *)arg;

    if (!likely(atomic_read(&priv->is_irq))) {
        if (unlikely(wait_event_interruptible(priv->queue, atomic_read(&priv->is_irq)))) {
            return -ERESTARTSYS;
        }
    }

    atomic_dec(&priv->is_irq);

    if (priv->drv_block_put != priv->drv_block_get) {
        unsigned int get = priv->drv_block_get + 1;

        if (get >= FIFO_LEN)
            get = 0;

        if (copy_to_user(buf, &priv->drv_block[get], sizeof(wls_wait_req_t))) {
            return -EFAULT;
        }

        priv->drv_block_get = get;

#ifdef DEBUG
        printk(KERN_INFO "[wls]:GET: put=%d get=%d T=%lu is_irq=%d\n",
               priv->drv_block_put, priv->drv_block_get,
               priv->drv_block[get].start_time, atomic_read(&priv->is_irq));
#endif /* DEBUG */

    } else {
#ifdef DEBUG
        printk(KERN_ERR "[wls]: wrong computation of queueing\n");
#endif /* DEBUG */
    }

    return 0;
}

static unsigned wls_is_us_opened(wls_open_req_t *param)
{
    // TODO: add check

        return 0;
}

wls_us_ctx_t *wls_create_us_ctx(wls_open_req_t *param, struct wls_dev_t * wls_loc)
{
    wls_us_ctx_t*     pUsCtx = NULL;
    wls_drv_ctx_t*    pDrv_ctx =  wls_loc->pWlsDrvCtx;

    // check if instance already registered
    if(wls_is_us_opened(param))
        goto err0;

    // allocate memory for shared portion
    pUsCtx = (wls_us_ctx_t*)dma_alloc_coherent(NULL, param->size, (dma_addr_t *)&param->ctx_pa, GFP_KERNEL);
    WLS_DEBUG("wls_create_us_ctx: pUsCtx  0x%016lx\n", (unsigned long)pUsCtx);
    if (pUsCtx){
        // allocate memory for private
        wls_us_priv_t *pUs_priv = kmalloc(sizeof(wls_us_priv_t), GFP_KERNEL);

        if(pUs_priv == NULL)
            goto err1;
        // init shared
        memset (pUsCtx, 0, sizeof(wls_us_ctx_t));

        SFL_DefQueue(&pUsCtx->ul_free_block_pq, pUsCtx->ul_free_block_storage, UL_FREE_BLOCK_QUEUE_SIZE * sizeof(void*));
        WLS_PRINT("ul free: off %lx\n",((U64) &pUsCtx->ul_free_block_pq -(U64)pUsCtx));

        WLS_MsgDefineQueue(&pUsCtx->get_queue, pUsCtx->get_storage, WLS_GET_QUEUE_N_ELEMENTS, 0);
        WLS_PRINT("get_queue: off %lx\n",((U64) &pUsCtx->get_queue -(U64)pUsCtx));

        WLS_MsgDefineQueue(&pUsCtx->put_queue, pUsCtx->put_storage, WLS_PUT_QUEUE_N_ELEMENTS, 0);
        WLS_PRINT("put_queue: off %lx\n",((U64) &pUsCtx->put_queue -(U64)pUsCtx));

        // init private
        memset (pUs_priv, 0, sizeof(wls_us_priv_t));
        init_waitqueue_head(&pUs_priv->sema.queue);
        atomic_set(&pUs_priv->sema.is_irq, 0);

        pUs_priv->pid = current->pid;

        pUsCtx->wls_us_private = pUs_priv;
        WLS_DEBUG("wls_create_us_ctx: pUsCtx->wls_us_private 0x%016lx\n", (unsigned long)pUsCtx->wls_us_private);
    } else
        goto err0;

    pDrv_ctx->p_wls_us_ctx[pDrv_ctx->nWlsClients]      = pUsCtx;
    pDrv_ctx->p_wls_us_pa_ctx[pDrv_ctx->nWlsClients++] = (wls_us_ctx_t*)param->ctx_pa;

    if(pDrv_ctx->p_wls_us_ctx[0] && pDrv_ctx->p_wls_us_ctx[1])
    {
        //link ctx
        pDrv_ctx->p_wls_us_ctx[0]->dst_kernel_va = (uint64_t)pDrv_ctx->p_wls_us_ctx[1];
        pDrv_ctx->p_wls_us_ctx[0]->dst_pa        = (uint64_t) pDrv_ctx->p_wls_us_pa_ctx[1];

        pDrv_ctx->p_wls_us_ctx[1]->dst_kernel_va = (uint64_t)pDrv_ctx->p_wls_us_ctx[0];
        pDrv_ctx->p_wls_us_ctx[1]->dst_pa        = (uint64_t) pDrv_ctx->p_wls_us_pa_ctx[0];

        pDrv_ctx->p_wls_us_ctx[0]->dst_kernel_va = (uint64_t)pDrv_ctx->p_wls_us_ctx[1];
        pDrv_ctx->p_wls_us_ctx[0]->dst_pa        = (uint64_t) pDrv_ctx->p_wls_us_pa_ctx[1];
        pDrv_ctx->p_wls_us_ctx[1]->dst_kernel_va = (uint64_t)pDrv_ctx->p_wls_us_ctx[0];
        pDrv_ctx->p_wls_us_ctx[1]->dst_pa        = (uint64_t) pDrv_ctx->p_wls_us_pa_ctx[0];

        WLS_DEBUG("link: 0 <-> 1: 0: 0x%016lx 1: 0x%016lx\n", (long unsigned int)pDrv_ctx->p_wls_us_ctx[0]->dst_kernel_va,
                                                              (long unsigned int)pDrv_ctx->p_wls_us_ctx[1]->dst_kernel_va);
    }

    return pUsCtx;

//err2:
    kfree(pUsCtx->wls_us_private);
err1:
    dma_free_coherent(wls_loc->device, param->size, pUsCtx, param->ctx_pa);
err0:
    return NULL;
}


int wls_destroy_us_ctx(wls_close_req_t *param,     struct wls_dev_t * wls_prv)
{
    wls_us_ctx_t*  pUsCtx   = NULL;
    wls_us_priv_t* pUs_priv = NULL;

    wls_drv_ctx_t*    pDrv_ctx =  wls_prv->pWlsDrvCtx;

    if(pDrv_ctx->p_wls_us_ctx[0] && pDrv_ctx->p_wls_us_ctx[1])
    {
        //link ctx
        pDrv_ctx->p_wls_us_ctx[0]->dst_kernel_va = (uint64_t)0;
        pDrv_ctx->p_wls_us_ctx[1]->dst_kernel_va = (uint64_t)0;
        pDrv_ctx->p_wls_us_ctx[0]->dst_user_va = (uint64_t)0;
        pDrv_ctx->p_wls_us_ctx[1]->dst_user_va = (uint64_t)0;
        pDrv_ctx->p_wls_us_ctx[0]->dst_pa = (uint64_t)0;
        pDrv_ctx->p_wls_us_ctx[1]->dst_pa = (uint64_t)0;

        WLS_DEBUG("un-link: 0 <-> 1: 0: 0x%016lx 1: 0x%016lx\n", (long unsigned int)pDrv_ctx->p_wls_us_ctx[0]->dst_kernel_va,
                                                                 (long unsigned int)pDrv_ctx->p_wls_us_ctx[1]->dst_kernel_va);
    }

    pUsCtx =  (wls_us_ctx_t*)param->ctx;

    if(pUsCtx){
        pUs_priv = (wls_us_priv_t*)pUsCtx->wls_us_private;
        if(pUs_priv){
            if (pUs_priv->isWait == 0){

                pUsCtx->wls_us_private = NULL;
                kfree(pUs_priv);
                if(param->ctx_pa){
                    if( pDrv_ctx->p_wls_us_ctx[0] == pUsCtx){
                        pDrv_ctx->p_wls_us_ctx[0]    = NULL;
                        pDrv_ctx->p_wls_us_pa_ctx[0] = NULL;
                    } else {
                        pDrv_ctx->p_wls_us_ctx[1]    = NULL;
                        pDrv_ctx->p_wls_us_pa_ctx[1] = NULL;
                    }
                    pDrv_ctx->nWlsClients--;
                    dma_free_coherent(wls_prv->device, param->size, pUsCtx, param->ctx_pa);
                }else{
                    WLS_ERROR("param->ctx_pa is NULL\n");
                }
            } else
                WLS_PRINT("Wait is in process\n");
        }
    }

    return 0;
}

static int wls_process_wait(wls_us_ctx_t* pUsCtx)
{
    int n = WLS_GetNumItemsInTheQueue(&pUsCtx->get_queue);

    return n;
}

static int wls_process_put(wls_us_ctx_t *src, wls_us_ctx_t *dst)
{
    int ret = 0;
    WLS_MSG_HANDLE hMsg;
    int n = 0;

    wls_us_priv_t* pDstPriv    =  NULL;
    wls_wait_req_t drv_block;

    WLS_DEBUG("offset get_queue %lx\n",(U64)&src->get_queue - (U64)src);

    n = WLS_GetNumItemsInTheQueue(&src->put_queue);

    while(n--)
    {
        if (WLS_MsgDequeue(&src->put_queue, &hMsg, NULL, (void*)src))
        {
           WLS_DEBUG("WLS_Get %lx %d type %d\n",(U64) hMsg.pIaPaMsg, hMsg.MsgSize, hMsg.TypeID);
           if(WLS_MsgEnqueue(&dst->get_queue, hMsg.pIaPaMsg, hMsg.MsgSize, hMsg.TypeID, hMsg.flags, NULL,  (void*)dst) == FALSE){ // try to send
               if(WLS_MsgEnqueue(&src->put_queue, hMsg.pIaPaMsg, hMsg.MsgSize, hMsg.TypeID, hMsg.flags, NULL, (void*)src) == FALSE){ // return back
                   WLS_ERROR("wls_process_put: Cannot return block to back to queue \n");
                   ret = -1;
               }
               break;
           }
        }
        else{
            ret = -1;
            break;
        }

    }

    if(dst->wls_us_private){
        pDstPriv = (wls_us_priv_t*)dst->wls_us_private;

        drv_block.start_time = wls_rdtsc();
        pDstPriv->NeedToWakeUp = 1;
        wls_wake_up_user_thread((char *)&drv_block, &pDstPriv->sema);
    }
    else
        ret = -1;

    return ret;
}

static long wls_ioctl(struct file * filp, unsigned int cmd, unsigned long arg)
{
    struct wls_dev_t * wls_prv = (struct wls_dev_t *)filp->private_data;
    void __user * to = (void __user *)arg;
    const void __user * from = (const void __user *)arg;
    long ret = 0;

    WLS_DEBUG("wls_ioctl PID [%d] ", current->pid);

    if (_IOC_TYPE(cmd) != WLS_IOC_MAGIC) {
        return -ENOTTY;
    }

    if (_IOC_NR(cmd) >= WLS_IOC_COUNT) {
        return -ENOTTY;
    }

    switch (cmd) {
        case WLS_IOC_OPEN: {
            wls_open_req_t param;

            WLS_DEBUG("WLS_IOC_OPEN wls_us_ctx_t %ld\n", sizeof(wls_us_ctx_t));
            ret = copy_from_user((void *)&param, from, sizeof(param));
            if (ret != 0) {
                WLS_ERROR("could not copy %lu bytes from user 0x%08lx",
                            (unsigned long)ret, (unsigned long)from);
                break;
            }

            if (sizeof(wls_drv_ctx_t) >= param.size){
                WLS_ERROR("incorrect size %lu > %u\n", sizeof(wls_drv_ctx_t), param.size);
                ret = -1;
                break;
            }

            param.ctx = (uint64_t)wls_create_us_ctx(&param, wls_prv);
            if (param.ctx == 0) {
                WLS_ERROR("could not copy %lu bytes to  user 0x%08lx",
                            (unsigned long)ret, (unsigned long)from);
                break;
            }

            WLS_DEBUG("WLS_IOC_OPEN: kva %lx pa %lx sz [%d]\n", (long unsigned int)param.ctx, (long unsigned int)param.ctx_pa, param.size);

            ret = copy_to_user(to, (const void *)&param, sizeof(wls_open_req_t));
            if (ret != 0) {
                WLS_ERROR("could not copy %lu bytes to  user 0x%08lx",
                            (unsigned long)ret, (unsigned long)from);
                break;
            }
        } break;
        case WLS_IOC_CLOSE: {
            wls_close_req_t param;

            ret = copy_from_user((void *)&param, from, sizeof(param));
            if (ret != 0) {
                WLS_ERROR("could not copy %lu bytes from user 0x%08lx",
                            (unsigned long)ret, (unsigned long)from);
                break;
            }
            WLS_DEBUG("WLS_IOC_CLOSE: kva %lx pa %lx sz [%d]\n", (long unsigned int)param.ctx,  (long unsigned int)param.ctx_pa, param.size);

            ret = wls_destroy_us_ctx(&param, wls_prv);

            if (ret != 0) {
                WLS_ERROR("could not copy %lu bytes from user 0x%08lx",
                            (unsigned long)ret, (unsigned long)from);
                break;
            }
        } break;
        case WLS_IOC_PUT: {
            wls_put_req_t param;
            wls_us_ctx_t*  pUsCtx   = NULL;

#if defined(_MLOG_TRACE_)
            unsigned long t = MLOG_GETTICK();
#endif
            ret = copy_from_user((void *)&param, from, sizeof(param));
            if (ret != 0) {
                WLS_ERROR("could not copy %lu bytes from user 0x%08lx",
                            (unsigned long)ret, (unsigned long)from);
                break;
            }

            pUsCtx = (wls_us_ctx_t*)param.wls_us_kernel_va;
            if (pUsCtx == NULL) {
                WLS_ERROR("Transaction failed %ld\n", (unsigned long)ret);
                break;
            }

            if(pUsCtx->dst_kernel_va)
                ret = wls_process_put(pUsCtx, (wls_us_ctx_t*)pUsCtx->dst_kernel_va);

            if (ret != 0) {
                WLS_ERROR("Transaction failed %ld\n", (unsigned long)ret);
                break;
            }

            /* clean up for next time */
#if defined(_MLOG_TRACE_)
            MLogTask(PID_WLS_DRV_IOC_PUT, t, MLOG_GETTICK());
#endif
        } break;
        case WLS_IOC_EVENT: {
            wls_event_req_t param;

            ret = copy_from_user((void *)&param, from, sizeof(param));

            if (ret != 0) {
                WLS_ERROR("Event %ld failed %ld\n", (unsigned long)param.event_to_wls,
                            (unsigned long)ret);
                break;
            }
        }break;
        case WLS_IOC_WAIT: {
            wls_wait_req_t param;
            wls_us_ctx_t*  pUsCtx  =  NULL;
            wls_us_priv_t* pUsPriv =  NULL;
#if defined(_MLOG_TRACE_)
            unsigned long t = MLOG_GETTICK();
            MLogTask(PID_WLS_DRV_IOC_WAIT_WAKE_ENTRY, t, 1250+t);
#endif
            ret = copy_from_user((void *)&param, from, sizeof(param));
            if (ret != 0) {
                WLS_ERROR("Wait failed %ld\n", (unsigned long)ret);
                break;
            }

            WLS_DEBUG("Wait pUsCtx 0x%016lx\n", (unsigned long)param.wls_us_kernel_va);
            pUsCtx  = (wls_us_ctx_t*)  param.wls_us_kernel_va;
            if(pUsCtx == NULL) {
                ret = -EINVAL;
                WLS_ERROR("Wait failed on User context %ld\n", (unsigned long)ret);
                break;
            }

            pUsPriv = (wls_us_priv_t*) pUsCtx->wls_us_private;
            WLS_DEBUG("Wait pUsPriv 0x%016lx\n", (unsigned long)pUsPriv);

            if(pUsPriv == NULL) {
                ret = -EINVAL;
                WLS_ERROR("Wait failed %ld\n", (unsigned long)ret);
                break;
            }
            pUsPriv->isWait = 1;
            wls_wait(&pUsPriv->sema, (unsigned long)from);
            pUsPriv->isWait = 0;
            memset(&param, 0, sizeof(wls_wait_req_t));
            param.nMsg = wls_process_wait(pUsCtx);

#if defined(_MLOG_TRACE_)
            t = MLOG_GETTICK();
#endif
            ret = copy_to_user(to, (const void *)&param, sizeof(wls_wait_req_t));
            if (ret != 0) {
                WLS_ERROR("could not copy %lu bytes to  user 0x%08lx",
                            (unsigned long)ret, (unsigned long)from);
                break;
            }
#if defined(_MLOG_TRACE_)
            MLogTask(PID_WLS_DRV_IOC_WAIT_WAKE_UP, t, MLOG_GETTICK());
#endif
        } break;
        case WLS_IOC_WAKE_UP: {
            wls_wait_req_t param;
            wls_us_ctx_t*  pUsCtx  =  NULL;
            wls_us_priv_t* pUsPriv =  NULL;
            wls_wait_req_t drv_block;

            ret = copy_from_user((void *)&param, from, sizeof(param));
            if (ret != 0) {
              WLS_ERROR("WLS_IOC_WAKE_UP failed %ld\n",
                          (unsigned long)ret);
              break;
            }

            WLS_DEBUG("Wait pUsCtx 0x%016lx\n", (unsigned long)param.wls_us_kernel_va);
            pUsCtx  = (wls_us_ctx_t*)  param.wls_us_kernel_va;
            if(pUsCtx == NULL) {
                ret = -EINVAL;
                WLS_ERROR("Wait failed on User context %ld\n", (unsigned long)ret);
                break;
            }

            pUsPriv = (wls_us_priv_t*) pUsCtx->wls_us_private;
            WLS_DEBUG("Wait pUsPriv 0x%016lx\n", (unsigned long)pUsPriv);

            if(pUsPriv == NULL) {
                ret = -EINVAL;
                WLS_ERROR("Wait failed %ld\n", (unsigned long)ret);
                break;
            }

            drv_block.start_time = wls_rdtsc();
            wls_wake_up_user_thread((char *)&drv_block, &pUsPriv->sema);
        } break;
        case WLS_IOC_CONNECT: {
          wls_connect_req_t param;
          wls_us_priv_t* pUsPriv    =  NULL;
          wls_wait_req_t drv_block;

          ret = copy_from_user((void *)&param, from, sizeof(param));
          if (ret != 0) {
              WLS_ERROR("WLS_IOC_WAKE_UP failed %ld\n",
                          (unsigned long)ret);
              break;
          }

          pUsPriv = (wls_us_priv_t*)param.wls_us_kernel_va;
          drv_block.start_time = wls_rdtsc();
          wls_wake_up_user_thread((char *)&drv_block, &pUsPriv->sema);
        } break;
        default:{
            WLS_ERROR("unknown ioctl cmd: '0x%08x'", cmd);
            BUG();
        } break;
    }

    if (ret != 0) {
        WLS_ERROR("cmd_%x failed: %ld", cmd, ret);
    }

    return ret;
}

static int wls_mmap(struct file * filp, struct vm_area_struct * vma)
{
    struct wls_dev_t * wls = (struct wls_dev_t *)filp->private_data;

    WLS_DEBUG("priv: 0x%p", filp->private_data);
    WLS_DEBUG("WLS_mmap : mmap function called \n");
    WLS_DEBUG("vma->start =%lx\n",vma->vm_start);
    WLS_DEBUG("vma->end =%lx\n",vma->vm_end);

    // non cached
//    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

    WLS_DEBUG("vma->pgoff =%lx\n",vma->vm_pgoff);

    if (wls == NULL) {
        WLS_ERROR("WLS is NULL");
        return -EIO;
    }

    return  remap_pfn_range(vma,vma->vm_start,vma->vm_pgoff,vma->vm_end-vma->vm_start,\
                            vma->vm_page_prot);
}

static int __init wls_init(void)
{
    struct wls_dev_t* wls_dev_loc = NULL;
    int res = 0;
    dev_t dev_no = 0;
    int minor_no = 0;
    int dev_cnt = 0;

    memset(&wls_dev[0], 0, sizeof(struct wls_dev_t*) * WLS_MAX_CLIENTS);

    snprintf(wls_driver_version, 10, WLS_DRV_VERSION_FORMAT, WLS_VERSION_X, WLS_VERSION_Y, WLS_VERSION_Z);

    WLS_PRINT("Intel(R) Wireless Subsystem Communication interface - %s\n", wls_driver_version);
    WLS_PRINT("Copyright(c) 2014 Intel Corporation.\n");
    //WLS_PRINT("Build: Date: %s Time: %s\n", __DATE__, __TIME__);

    if ((wlsMaxClients > WLS_MAX_CLIENTS) || (wlsMaxClients < 1))
    {
        WLS_ERROR("Invalid wlsMaxClients %d\n", wlsMaxClients);
        wlsMaxClients = 1;
    }
    for (dev_cnt = 0; dev_cnt < wlsMaxClients; dev_cnt++){
        wls_dev_loc  = (struct wls_dev_t *)kzalloc(sizeof(struct wls_dev_t), GFP_KERNEL);
        WLS_DEBUG("wls_dev_loc %d %p", dev_cnt, wls_dev_loc);

        if (wls_dev_loc == NULL) {
            WLS_ERROR("no free memory (wanted %ld bytes)", sizeof(struct wls_dev_t));
            res = -ENOMEM;
            goto err0;
        }
        wls_dev[dev_cnt] = wls_dev_loc;
        WLS_DEBUG("wls_init [%d]: 0x%p",dev_cnt, wls_dev[dev_cnt]);
    }

    res = alloc_chrdev_region(&dev_no, minor_no, wlsMaxClients, MODNAME);
    if (res < 0) {
        WLS_ERROR("failed alloc char dev region: %d", res);
        goto err0;
    }

    wls_class = class_create(THIS_MODULE, wls_driver_name);

    wls_dev_loc =  wls_dev[0];
    wls_dev_loc->dev_no = dev_no;

    cdev_init(&wls_dev_loc->cdev, &wls_fops);
    wls_dev_loc->cdev.owner = THIS_MODULE;
    wls_dev_loc->cdev.ops = &wls_fops;
    res = cdev_add(&wls_dev_loc->cdev, dev_no, wlsMaxClients);

    if (res) {
        WLS_ERROR("failed add char dev: %d", res);
        res = -1;
        goto err2;
    }

    if (IS_ERR((void *)wls_class)) {
        WLS_ERROR("failed create class");
        res = -EIO;
        goto err1;
    }

    for (dev_cnt = 0; dev_cnt < wlsMaxClients; dev_cnt++){
        wls_dev_loc  = wls_dev[dev_cnt];
        if (wls_dev_loc == NULL ) {
            WLS_ERROR("wls_dev_loc is NULL");
            goto err2;
        }

        if(wlsMaxClients > 1){
            snprintf(wls_dev_device_name, 10, WLS_DEV_DEVICE_FORMAT, dev_cnt);
        } else {
            snprintf(wls_dev_device_name, 10, "%s", MODNAME);
        }

        wls_dev_loc->dev_no = MKDEV(MAJOR(dev_no), dev_cnt);
        wls_dev_loc->device = device_create(wls_class, NULL, wls_dev_loc->dev_no, NULL, wls_dev_device_name);

        if (IS_ERR((void *)wls_dev_loc->device)) {
            WLS_ERROR("failed create / device");
            res = -2;
            goto err2;
        }

        dev_info(wls_dev_loc->device, "Device: %s\n", wls_dev_device_name);
        mutex_init(&wls_dev_loc->lock);

        wls_dev_loc->pWlsDrvCtx = wls_get_ctx(dev_cnt);

        if (wls_dev_loc->pWlsDrvCtx ==  NULL) {
            WLS_ERROR("failed wls_get_ctx(%d)", dev_cnt);
            res = -3;
            goto err2;
        }

        //return res;
//        dev_no++;
        continue;
    }
    WLS_PRINT("init %d /dev/wlsX communication devices [0-%d]\n", dev_cnt, dev_cnt-1);
    return res;

    err2:
         for (dev_cnt = 0; dev_cnt < wlsMaxClients; dev_cnt++){
            wls_dev_loc  = wls_dev[dev_cnt];
            if(wls_dev_loc){
                device_destroy(wls_class, wls_dev_loc->dev_no);
                cdev_del(&wls_dev_loc->cdev);
            }
         }
        class_destroy(wls_class);
    err1:
        unregister_chrdev_region(wls_dev[0]->dev_no, wlsMaxClients);
    err0:
        for (dev_cnt = 0; dev_cnt < wlsMaxClients; dev_cnt++){
            if(wls_dev[dev_cnt]){
                kfree(wls_dev[dev_cnt]);
                wls_dev[dev_cnt] = NULL;
            }
        }

        WLS_ERROR("init failed");
        return -1;
}

static void __exit wls_exit(void)
{
    struct wls_dev_t* wls_dev_loc = NULL;
    int dev_cnt = 0;

    if (wls_dev[0]) {
        for (dev_cnt = 0; dev_cnt < wlsMaxClients; dev_cnt++){
            wls_dev_loc  = wls_dev[dev_cnt];
            device_destroy(wls_class, wls_dev_loc->dev_no);
        }
        wls_dev_loc =  wls_dev[0];

        cdev_del(&wls_dev_loc->cdev);
        class_destroy(wls_class);

        unregister_chrdev_region(wls_dev[0]->dev_no, wlsMaxClients);

        for (dev_cnt = 0; dev_cnt < wlsMaxClients; dev_cnt++){
            if(wls_dev[dev_cnt]){
                kfree(wls_dev[dev_cnt]);
                wls_dev[dev_cnt] = NULL;
            }
        }
    }

    WLS_PRINT("Intel(R) Wireless Subsystem Communication interface - %s was removed\n", wls_driver_version);
}

MODULE_DESCRIPTION("Wirelsess Sybsytem Communication interface");
MODULE_AUTHOR("Intel Corporation");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("WLS_DRV_VERSION_FORMAT");

module_init(wls_init);
module_exit(wls_exit);
