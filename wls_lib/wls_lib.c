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

#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include "ttypes.h"
#include "wls_lib.h"
#include "wls.h"
#include "syslib.h"

#define WLS_MAP_SHM 1

#define WLS_PHY_SHM_FILE_NAME "/tmp/phyappshm"

#define HUGE_PAGE_FILE_NAME "/mnt/huge/page"

#define DIV_ROUND_OFFSET(X,Y) ( X/Y + ((X%Y)?1:0) )

#define WLS_LIB_USER_SPACE_CTX_SIZE DMA_MAP_MAX_BLOCK_SIZE

#define PLIB_ERR(x, args...)   printf("wls_lib: "x, ## args);
#define PLIB_INFO(x, args...)  printf("wls_lib: "x, ## args);

#ifdef _DEBUG_
#define PLIB_DEBUG(x, args...)  printf("wls_lib debug: "x, ## args);
#else
#define PLIB_DEBUG(x, args...)  do { } while(0)
#endif

#ifdef __x86_64__
#define WLS_LIB_MMAP mmap
#else
#define WLS_LIB_MMAP mmap64
#endif

extern int gethugepagesizes(long pagesizes[], int n_elem);
extern int hugetlbfs_unlinked_fd(void);


static pthread_mutex_t wls_put_lock;
static pthread_mutex_t wls_get_lock;

static int           wls_dev_fd =  0;
static wls_us_ctx_t* wls_us_ctx =  NULL;

static uint64_t wls_kernel_va_to_user_va(void *pWls_us, uint64_t ptr);

int ipc_file = 0;

static int wls_VirtToPhys(void* virtAddr, uint64_t* physAddr)
{
    int          mapFd;
    uint64_t     page;
    unsigned int pageSize;
    unsigned long virtualPageNumber;

    mapFd = open ("/proc/self/pagemap" , O_RDONLY );
    if (mapFd < 0 )
    {
        PLIB_ERR("Could't open pagemap file\n");
        return -1;
    }

    /*get standard page size*/
    pageSize = getpagesize();

    virtualPageNumber = (unsigned long) virtAddr / pageSize ;

    lseek(mapFd , virtualPageNumber * sizeof(uint64_t) , SEEK_SET );

    if(read(mapFd ,&page , sizeof(uint64_t)) < 0 )
    {
        close(mapFd);
        PLIB_ERR("Could't read pagemap file\n");
        return -1;
    }

    *physAddr = (( page & 0x007fffffffffffffULL ) * pageSize );

    close(mapFd);

    return 0;
}

static void wls_mutex_destroy(pthread_mutex_t* pMutex)
{
    pthread_mutex_destroy(pMutex);
}

static void wls_mutex_init(pthread_mutex_t* pMutex)
{
   pthread_mutexattr_t prior;
   pthread_mutexattr_init(&prior);
   pthread_mutexattr_setprotocol(&prior, PTHREAD_PRIO_INHERIT);
   pthread_mutex_init(pMutex, &prior);
   pthread_mutexattr_destroy(&prior);
}

static void wls_mutex_lock(pthread_mutex_t* pMutex)
{
    pthread_mutex_lock(pMutex);
}

static void wls_mutex_unlock(pthread_mutex_t* pMutex)
{
    pthread_mutex_unlock(pMutex);
}

static uint64_t wls_kernel_va_to_user_va(void *pWls_us, uint64_t ptr)
{
    unsigned long ret = 0;
    wls_us_ctx_t* pUs = (wls_us_ctx_t*)pWls_us;

    uint64_t kva = (uint64_t) pUs->wls_us_kernel_va;
    uint64_t uva = (uint64_t) pUs->wls_us_user_space_va;

    ret = (uva + (ptr - kva));

    PLIB_DEBUG("kva %lx to uva %lx [offset %d]\n",kva, ret, (kva - ret));
    return ret;
}

static uint64_t wls_kernel_va_to_user_va_dest(void *pWls_us, uint64_t ptr)
{
    unsigned long ret = 0;
    wls_us_ctx_t* pUs = (wls_us_ctx_t*)pWls_us;

    uint64_t kva = (uint64_t) pUs->dst_kernel_va;
    uint64_t uva = (uint64_t) pUs->dst_user_va;

    ret = (uva + (ptr - kva));

    PLIB_DEBUG("kva %lx to uva %lx [offset %d]\n",kva, ret, (kva - ret));
    return ret;
}


void* WLS_Open(const char *ifacename, unsigned int mode, unsigned long long nWlsMemorySize)
{
    wls_us_ctx_t*  pWls_us = NULL;
    unsigned int   ret = 0;
    wls_open_req_t params;
    int i, len;
    char temp[WLS_DEV_SHM_NAME_LEN];

#ifdef __x86_64__
    params.ctx  = 64L;
#else
    params.ctx  = 32L;
#endif

    params.ctx_pa = 0;
    params.size   = WLS_LIB_USER_SPACE_CTX_SIZE;

    if(sizeof(wls_us_ctx_t) >= 64*1024){
        PLIB_ERR("WLS_Open %ld \n", sizeof(wls_us_ctx_t));
        return NULL;
    }

    if (!wls_us_ctx) {
        PLIB_INFO("Open %s 0x%08lx\n", ifacename, WLS_IOC_OPEN);

        if ((wls_dev_fd = open(ifacename, O_RDWR | O_SYNC)) < 0){
            PLIB_ERR("Open filed [%d]\n", wls_dev_fd);
            return NULL;
        }
        /* allocate block in shared space */
        if((ret = ioctl(wls_dev_fd, WLS_IOC_OPEN, &params)) < 0) {
            PLIB_ERR("Open filed [%d]\n", ret);
            return NULL;
        }

        PLIB_DEBUG("params: kernel va 0x%016llx pa 0x%016llx size %ld\n",
           params.ctx, params.ctx_pa, params.size);

        if (params.ctx_pa) {
            /* remap to user space the same block */
            pWls_us = (wls_us_ctx_t*) WLS_LIB_MMAP(NULL,
                                      params.size,
                                      PROT_READ|PROT_WRITE ,
                                      MAP_SHARED,
                                      wls_dev_fd,
                                      params.ctx_pa);

            if( pWls_us == MAP_FAILED ){
                PLIB_ERR("mmap has failed (%d:%s) 0x%016lx [size %d]\n", errno, strerror(errno),params.ctx_pa, params.size);
                return NULL;
            }

            PLIB_DEBUG("Local: pWls_us 0x%016p\n", pWls_us);

            PLIB_DEBUG("size wls_us_ctx_t %d\n", sizeof(wls_us_ctx_t));
            PLIB_DEBUG("    ul free  : off 0x%016lx\n",((unsigned long) &pWls_us->ul_free_block_pq -(unsigned long)pWls_us));
            PLIB_DEBUG("    get_queue: off 0x%016lx\n",((unsigned long) &pWls_us->get_queue -(unsigned long)pWls_us));
            PLIB_DEBUG("    put_queue: off 0x%016lx\n",((unsigned long) &pWls_us->put_queue -(unsigned long)pWls_us));

            //memset(pWls_us, 0, params.size);

            pWls_us->padding_wls_us_user_space_va = 0LL;

            pWls_us->wls_us_user_space_va = pWls_us;

            pWls_us->wls_us_kernel_va = (uint64_t) params.ctx;
            pWls_us->wls_us_pa        = (uint64_t) params.ctx_pa;
            pWls_us->wls_us_ctx_size  =  params.size;

            PLIB_INFO("User Space Lib Context: us va 0x%016lx kernel va 0x%016lx pa 0x%016lx size %d \n",
                (uintptr_t)pWls_us->wls_us_user_space_va,
                pWls_us->wls_us_kernel_va,
                pWls_us->wls_us_pa,
                pWls_us->wls_us_ctx_size);

            wls_mutex_init(&wls_put_lock);
            wls_mutex_init(&wls_get_lock);

            pWls_us->mode = mode;
            PLIB_INFO("\nMode %d\n", pWls_us->mode);

            PLIB_INFO("\nWLS device %s [%d]\n", ifacename, (int)strlen(ifacename));
            strncpy(temp, ifacename, WLS_DEV_SHM_NAME_LEN - 1);
            len = strlen(ifacename);
            if (len < WLS_DEV_SHM_NAME_LEN - 1)
                strncpy(pWls_us->wls_dev_name, temp, len);
            else
                strncpy(pWls_us->wls_dev_name, temp, WLS_DEV_SHM_NAME_LEN - 1);
            for(i = 0; i < MIN(strlen(pWls_us->wls_dev_name),WLS_DEV_SHM_NAME_LEN); i++)
                if(pWls_us->wls_dev_name[i] != '/')
                    pWls_us->wls_shm_name[i] = pWls_us->wls_dev_name[i];
                else
                    pWls_us->wls_shm_name[i] = '_';

            wls_us_ctx = pWls_us;
        }
        else {
            PLIB_ERR("Open filed: incorrect allocation \n");
            return NULL;
        }
    }

    return wls_us_ctx;
}

int WLS_Ready(void* h)
{
    int ret = 0;
    wls_event_req_t params;

    if (!wls_us_ctx || !wls_dev_fd){
        PLIB_ERR("Library was not opened\n");
        return -1;
    }

    params.event_to_wls =  WLS_EVENT_IA_READY;
    params.event_param = 0;

    /* free block in shared space */
    if((ret = ioctl(wls_dev_fd, WLS_IOC_EVENT, &params)) < 0) {
        PLIB_ERR("Event filed [%d]\n", ret);
        return ret;
    }

    return 0;
}

int WLS_Close(void* h)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t*)h;
    wls_close_req_t params;
    int ret = 0;

    if (!wls_us_ctx || !wls_dev_fd){
        PLIB_ERR("Library was not opened\n");
        return -1;
    }

    if ((unsigned long)pWls_us !=  (unsigned long )wls_us_ctx){
        PLIB_ERR("Incorret handle %lx [expected %lx]\n", (unsigned long)pWls_us, (unsigned long )wls_us_ctx);
        return -1;
    }

    params.ctx    = pWls_us->wls_us_kernel_va;
    params.ctx_pa = pWls_us->wls_us_pa;
    params.size   = pWls_us->wls_us_ctx_size;

    /* free block in shared space */
    if((ret = ioctl(wls_dev_fd, WLS_IOC_CLOSE, &params)) < 0) {
        PLIB_ERR("Close filed [%d]\n", ret);
        return 0;
    }

    /* unmap to user space */
    munmap(pWls_us, pWls_us->wls_us_ctx_size);

    wls_mutex_destroy(&wls_put_lock);
    wls_mutex_destroy(&wls_get_lock);

    close(wls_dev_fd);

    wls_us_ctx = NULL;
    wls_dev_fd = 0;

    return 0;
}


void* WLS_Alloc(void* h, unsigned int size)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t* )h;

    long          pageSize[1];
    long          hugePageSize;
    long          nHugePage;

    hugepage_tabl_t*  pHugePageTlb = &pWls_us->hugepageTbl[0];

    void*  pvirtAddr = NULL;
    int    count;
    int    fd;

    char shm_file_name[256];

    fd = hugetlbfs_unlinked_fd();

    if (fd < 0)
        PLIB_ERR("Unable to open temp file in hugetlbfs (%s)", strerror(errno));

    gethugepagesizes(pageSize,1);
    hugePageSize = pageSize[0];

    PLIB_INFO("hugePageSize on the system is %ld\n", hugePageSize);

    /* calculate total number of hugepages */
    nHugePage =  DIV_ROUND_OFFSET(size, hugePageSize);

    if (nHugePage >= MAX_N_HUGE_PAGES){
        PLIB_INFO("not enough hugepages: need %ld  system has %d\n", nHugePage,  MAX_N_HUGE_PAGES);
        return NULL;
    }

    if(pHugePageTlb == NULL )
    {
        PLIB_INFO("Table memory allocation failed\n");
        return NULL;
    }

#if WLS_MAP_SHM
{
    snprintf(shm_file_name, WLS_DEV_SHM_NAME_LEN, "%s_%s", WLS_PHY_SHM_FILE_NAME, pWls_us->wls_shm_name);
    PLIB_INFO("shm open %s\n", shm_file_name);
    ipc_file = open(shm_file_name, O_CREAT); // | O_EXCL  maybe sometimes in future.. ;-)
    if(ipc_file == -1){
        PLIB_ERR("open  failed (%s)\n", strerror(errno) );
        return NULL;
    }

    key_t  key = ftok(shm_file_name, '4');
    int shm_handle = shmget(key, size, SHM_HUGETLB|SHM_R|SHM_W);
    if(shm_handle == -1){
         PLIB_INFO("Create shared memory\n");
         shm_handle = shmget(key, size, SHM_HUGETLB | IPC_CREAT | SHM_R | SHM_W);
    }
    else
        PLIB_INFO("Attach to shared memory\n");

    if(shm_handle == -1){
        PLIB_ERR("shmget has failed (%s) [size %ld]\n", strerror(errno), nHugePage * hugePageSize);
        return NULL;
    }

    pvirtAddr = shmat(shm_handle, 0, /*SHM_RND*/0);
}
#else
    /* Allocate required number of pages */
    pvirtAddr = mmap(0,(nHugePage * hugePageSize), (PROT_READ|PROT_WRITE), MAP_SHARED, fd,0);
#endif
    if(pvirtAddr == MAP_FAILED )
    {
        PLIB_ERR("mmap has failed (%s) [size %ld]\n", strerror(errno), nHugePage * hugePageSize);
        return NULL;
    }

    PLIB_INFO("pvirtAddr  0x%016lx\n", (unsigned long)pvirtAddr);

    for(count = 0 ; count < nHugePage ; count++ )
    {
        /*Incremented virtual address to next hugepage to create table*/
        pHugePageTlb[count].pageVa =  ((unsigned char*)pvirtAddr + \
                                                ( count * hugePageSize ));
        /*Creating dummy page fault in process for each page
                                                inorder to get pagemap*/
        *(unsigned char*)pHugePageTlb[count].pageVa = 1;

        if(wls_VirtToPhys((uint64_t*) pHugePageTlb[count].pageVa,
                    &pHugePageTlb[count].pagePa ) == -1)
        {
            munmap(pvirtAddr, (nHugePage * hugePageSize));
            PLIB_ERR("Virtual to physical conversion failed\n");
            return NULL;
        }

        //PLIB_INFO("id %d va 0x%016p pa 0x%016llx [%ld]\n", count, (uintptr_t)pHugePageTlb[count].pageVa, (uint64_t) pHugePageTlb[count].pagePa, hugePageSize);
    }

    PLIB_INFO("WLS_Alloc: 0x%016lx [%d]\n", (unsigned long)pvirtAddr, size);

    close(fd);

    pWls_us->HugePageSize = (uint32_t)hugePageSize;
    pWls_us->alloc_buffer = pvirtAddr;
    pWls_us->alloc_size   = (uint32_t)(nHugePage * hugePageSize);

    if (pWls_us->mode == WLS_MASTER_CLIENT){
        wls_us_ctx_t* pWls_usRem = NULL;
        PLIB_INFO("Connecting to remote peer ...\n");
        while (pWls_us->dst_pa == 0) // wait for slave
                       ;

        /* remap to user space the same block */
        pWls_usRem = (wls_us_ctx_t*) WLS_LIB_MMAP(NULL,
                                  sizeof(wls_us_ctx_t),
                                  PROT_READ|PROT_WRITE ,
                                  MAP_SHARED,
                                  wls_dev_fd,
                                  pWls_us->dst_pa);

        if( pWls_us == MAP_FAILED ){
            PLIB_ERR("mmap has failed (%d:%s) 0x%016lx \n", errno, strerror(errno),pWls_us->dst_pa);
            return NULL;
        }

        PLIB_INFO("Remote: pWls_us 0x%p\n", pWls_usRem);

        PLIB_INFO("size wls_us_ctx_t %ld\n", sizeof(wls_us_ctx_t));
        PLIB_INFO("    ul free  : off 0x%016lx\n",((unsigned long) &pWls_usRem->ul_free_block_pq -(unsigned long)pWls_usRem));
        PLIB_INFO("    get_queue: off 0x%016lx\n",((unsigned long) &pWls_usRem->get_queue -(unsigned long)pWls_usRem));
        PLIB_INFO("    put_queue: off 0x%016lx\n",((unsigned long) &pWls_usRem->put_queue -(unsigned long)pWls_usRem));

        pWls_us->dst_user_va = (uint64_t) pWls_usRem  ;
    }


    return pvirtAddr;
}

int WLS_Free(void* h, PVOID pMsg)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t* )h;

    if ((unsigned long)pMsg != (unsigned long)pWls_us->alloc_buffer) {
        PLIB_ERR("incorrect pMsg %lx [expected %lx]\n", (unsigned long)pMsg ,(unsigned long)pWls_us->alloc_buffer);
        return -1;
    }

    if (pWls_us->mode == WLS_MASTER_CLIENT){
        if(pWls_us->dst_user_va){
            munmap((void*)pWls_us->dst_user_va, sizeof(wls_us_ctx_t));
            pWls_us->dst_user_va = 0;
        }
    }

    PLIB_DEBUG("WLS_Free 0x%016lx", (unsigned long)pMsg);
#if WLS_MAP_SHM
    shmdt(pMsg);
    close (ipc_file);
#else
    munmap(pMsg, pWls_us->alloc_size);
#endif



    return 0;
}

int WLS_Put(void* h, unsigned long long  pMsg, unsigned int MsgSize, unsigned short MsgTypeID, unsigned short Flags)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t* )h;
    int ret = 0;

    if ((unsigned long)h != (unsigned long)wls_us_ctx) {
        PLIB_ERR("Incorrect user space context %lx [%lx]\n", (unsigned long)h, (unsigned long)wls_us_ctx);
        return -1;
    }

    if(!WLS_IS_ONE_HUGE_PAGE(pMsg, MsgSize, WLS_HUGE_DEF_PAGE_SIZE)) {
        PLIB_ERR("WLS_Put input error: buffer is crossing 2MB page boundary 0x%016llx size %ld\n", pMsg, (unsigned long)MsgSize);
    }

    wls_mutex_lock(&wls_put_lock);

    if ((WLS_FLAGS_MASK & Flags)){ // multi block transaction
        if (Flags & WLS_TF_SYN){
            PLIB_DEBUG("WLS_SG_FIRST\n");
            if (WLS_MsgEnqueue(&pWls_us->put_queue, pMsg, MsgSize, MsgTypeID, Flags,  wls_kernel_va_to_user_va, (void*)pWls_us))
            {
               PLIB_DEBUG("WLS_Get %lx %d type %d\n",(U64) pMsg, MsgSize, MsgTypeID);
            }
        } else if ((Flags & WLS_TF_SCATTER_GATHER) && !(Flags & WLS_TF_SYN) && !(Flags & WLS_TF_FIN)){
            PLIB_DEBUG("WLS_SG_NEXT\n");
            if (WLS_MsgEnqueue(&pWls_us->put_queue, pMsg, MsgSize, MsgTypeID, Flags,  wls_kernel_va_to_user_va, (void*)pWls_us))
            {
               PLIB_DEBUG("WLS_Put %lx %d type %d\n",(U64) pMsg, MsgSize, MsgTypeID);
            }
        } else if (Flags & WLS_TF_FIN) {
            wls_put_req_t params;
            PLIB_DEBUG("WLS_SG_LAST\n");
            params.wls_us_kernel_va = pWls_us->wls_us_kernel_va;
            if (WLS_MsgEnqueue(&pWls_us->put_queue, pMsg, MsgSize, MsgTypeID, Flags,  wls_kernel_va_to_user_va, (void*)pWls_us))
            {
               PLIB_DEBUG("WLS_Put %lx %d type %d\n",(U64) pMsg, MsgSize, MsgTypeID);
            }

            PLIB_DEBUG("List: call WLS_IOC_PUT\n");
            if((ret = ioctl(wls_dev_fd, WLS_IOC_PUT, &params)) < 0) {
                PLIB_ERR("Put filed [%d]\n", ret);
                wls_mutex_unlock(&wls_put_lock);
                return -1;
            }
        } else
            PLIB_ERR("unsaported flags %x\n", WLS_FLAGS_MASK & Flags);
    } else {  // one block transaction
        wls_put_req_t params;
        params.wls_us_kernel_va = pWls_us->wls_us_kernel_va;
        if (WLS_MsgEnqueue(&pWls_us->put_queue, pMsg, MsgSize, MsgTypeID, Flags,  wls_kernel_va_to_user_va, (void*)pWls_us))
        {
           PLIB_DEBUG("WLS_Put %lx %d type %d\n",(U64) pMsg, MsgSize, MsgTypeID);
        }

        PLIB_DEBUG("One block: call WLS_IOC_PUT\n");
        if((ret = ioctl(wls_dev_fd, WLS_IOC_PUT, &params)) < 0) {
            PLIB_ERR("Put filed [%d]\n", ret);
            wls_mutex_unlock(&wls_put_lock);
            return -1;
        }
    }
    wls_mutex_unlock(&wls_put_lock);

    return 0;
}

int WLS_Check(void* h)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t* )h;

    if ((unsigned long)h != (unsigned long)wls_us_ctx) {
        PLIB_ERR("Incorrect user space context %lx [%lx]\n", (unsigned long)h, (unsigned long)wls_us_ctx);
        return 0;
    }

    PLIB_DEBUG("offset get_queue %lx\n",(U64)&pWls_us->get_queue - (U64)pWls_us);

    return WLS_GetNumItemsInTheQueue(&pWls_us->get_queue);
}


unsigned long long WLS_Get(void* h, unsigned int *MsgSize, unsigned short *MsgTypeID, unsigned short *Flags)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t* )h;
    WLS_MSG_HANDLE hMsg;
    uint64_t pMsg = NULL;

    if ((unsigned long)h != (unsigned long)wls_us_ctx) {
        PLIB_ERR("Incorrect user space context %lx [%lx]\n", (unsigned long)h, (unsigned long)wls_us_ctx);
        return 0;
    }

    PLIB_DEBUG("offset get_queue %lx\n",(U64)&pWls_us->get_queue - (U64)pWls_us);
    wls_mutex_lock(&wls_get_lock);

    if (WLS_MsgDequeue(&pWls_us->get_queue, &hMsg, wls_kernel_va_to_user_va, (void*)pWls_us))
    {
       PLIB_DEBUG("WLS_Get %lx %d type %d\n",(U64) hMsg.pIaPaMsg, hMsg.MsgSize, hMsg.TypeID);
       pMsg         = hMsg.pIaPaMsg;
       *MsgSize     = hMsg.MsgSize;
       *MsgTypeID   = hMsg.TypeID;
       *Flags       = hMsg.flags;
    }

    wls_mutex_unlock(&wls_get_lock);

    return pMsg;
}

int   WLS_WakeUp(void* h)
{
    int ret;
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t* )h;
    wls_wake_up_req_t params;

    if (!wls_us_ctx || !wls_dev_fd){
        PLIB_ERR("Library was not opened\n");
        return -1;
    }

    params.ctx              = (uint64_t)pWls_us;
    params.wls_us_kernel_va = (uint64_t)pWls_us->wls_us_kernel_va;

    PLIB_DEBUG("WLS_WakeUp\n");

    if((ret = ioctl(wls_dev_fd, WLS_IOC_WAKE_UP, &params)) < 0) {
        PLIB_ERR("Wake Up filed [%d]\n", ret);
        return ret;
    }

    return 0;
}

int   WLS_Wait(void* h)
{
    int ret;
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t* )h;
    wls_wait_req_t params;

    if (!wls_us_ctx || !wls_dev_fd){
        PLIB_ERR("Library was not opened\n");
        return -1;
    }

    params.ctx              = (uint64_t)pWls_us;
    params.wls_us_kernel_va = (uint64_t)pWls_us->wls_us_kernel_va;
    params.action           =  0;
    params.nMsg             =  0;

    PLIB_DEBUG("WLS_Wait\n");

    if((ret = ioctl(wls_dev_fd, WLS_IOC_WAIT, &params)) < 0) {
        PLIB_ERR("Wait filed [%d]\n", ret);
        return ret;
    }

    return params.nMsg;
}

unsigned long long  WLS_WGet(void* h, unsigned int *MsgSize, unsigned short *MsgTypeID, unsigned short *Flags)
{
    uint64_t pRxMsg = WLS_Get(h, MsgSize, MsgTypeID, Flags);

    if (pRxMsg)
        return pRxMsg;

    WLS_Wait(h);
    return WLS_Get(h, MsgSize, MsgTypeID, Flags);
}

unsigned long long  WLS_VA2PA(void* h, PVOID pMsg)
{
    uint64_t      ret = 0;
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t* )h;

    unsigned long    alloc_base;
    hugepage_tabl_t* pHugePageTlb;
    uint64_t    hugePageBase;
    uint64_t    hugePageOffet;
    unsigned int     count = 0;

    uint64_t    HugePageMask = ((unsigned long)pWls_us->HugePageSize - 1);

    if(pWls_us->alloc_buffer == NULL){
        PLIB_ERR("WLS_VA2PA: nothing was allocated [%ld]\n", ret);
        return  (uint64_t)ret;
    }

    alloc_base     = (unsigned long)pWls_us->alloc_buffer;

    pHugePageTlb   = &pWls_us->hugepageTbl[0];

    hugePageBase   = (uint64_t)pMsg & ~HugePageMask;
    hugePageOffet  = (uint64_t)pMsg & HugePageMask;

    count          = (hugePageBase - alloc_base) / pWls_us->HugePageSize;

    PLIB_DEBUG("WLS_VA2PA %lx base %llx off %llx  count %d\n", (unsigned long)pMsg,
        (uint64_t)hugePageBase, (uint64_t)hugePageOffet, count);

    ret = pHugePageTlb[count].pagePa + hugePageOffet;

    return (uint64_t) ret;
}

void* WLS_PA2VA(void* h, unsigned long long  pMsg)
{
    unsigned long    ret = NULL;
    wls_us_ctx_t*    pWls_us = (wls_us_ctx_t* )h;

    hugepage_tabl_t* pHugePageTlb;
    uint64_t         hugePageBase;
    uint64_t         hugePageOffet;
    unsigned int     count;
    int              i;
    uint64_t         HugePageMask = ((uint64_t)pWls_us->HugePageSize - 1);

    if(pWls_us->alloc_buffer == NULL){
        PLIB_ERR("WLS_PA2VA: nothing was allocated [%ld]\n", ret);
        return  (void*)ret;
    }

    pHugePageTlb   = &pWls_us->hugepageTbl[0];

    hugePageBase   = (uint64_t)pMsg & ~HugePageMask;
    hugePageOffet  = (uint64_t)pMsg &  HugePageMask;

    count          = pWls_us->alloc_size / pWls_us->HugePageSize;

    PLIB_DEBUG("WLS_PA2VA %llx base %llx off %llx  count %d\n", (uint64_t)pMsg,
        (uint64_t)hugePageBase, (uint64_t)hugePageOffet, count);

    for (i = 0; i < count; i++) {
        if (pHugePageTlb[i].pagePa == hugePageBase)
        {
            ret = (unsigned long)pHugePageTlb[i].pageVa;
            ret += hugePageOffet;
            return  (void*)ret;
        }
    }

    return (void*) (ret);
}

int WLS_EnqueueBlock(void* h, unsigned long long pMsg)
{
    int ret = 0;
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t* )h;

    if (!wls_us_ctx || !wls_dev_fd){
        PLIB_ERR("Library was not opened\n");
        return -1;
    }

    if(pWls_us->mode == WLS_SLAVE_CLIENT){
        PLIB_ERR("Slave doesn't support memory allocation\n");
        return -1;
    }

    if(pMsg == 0){
        PLIB_ERR("WLS_EnqueueBlock: Null\n");
        return -1;
    }

    if(pWls_us->dst_kernel_va){
        if (pWls_us->dst_user_va)
        {
            wls_us_ctx_t* pDstWls_us = (wls_us_ctx_t* )pWls_us->dst_user_va;
            ret = SFL_WlsEnqueue(&pDstWls_us->ul_free_block_pq, pMsg, wls_kernel_va_to_user_va_dest, pWls_us);
            if(ret == 1){
                unsigned long* ptr = (unsigned long*)WLS_PA2VA(pWls_us, pMsg);
                if(ptr){
                    *ptr = 0xFFFFFFFFFFFFFFFF;
                }
            }
        }
        else
            ret = -1;
    }
    else
        ret = -1;

    PLIB_DEBUG("SFL_WlsEnqueue %d\n", ret);
    return ret;
}

unsigned long long WLS_DequeueBlock(void* h)
{
    unsigned long long retval = NULL;
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t* )h;

    if(pWls_us->mode == WLS_SLAVE_CLIENT){
        // local
        retval = SFL_WlsDequeue(&pWls_us->ul_free_block_pq, wls_kernel_va_to_user_va, h );
    } else if(pWls_us->dst_kernel_va) {
        // remote
        if (pWls_us->dst_user_va)
        {
            wls_us_ctx_t* pDstWls_us = (wls_us_ctx_t* )pWls_us->dst_user_va;
            retval = SFL_WlsDequeue(&pDstWls_us->ul_free_block_pq, wls_kernel_va_to_user_va_dest, pWls_us);
            if(retval){
                unsigned long* ptr = (unsigned long*)WLS_PA2VA(pWls_us, retval);
                if(ptr){
                    if(*ptr != 0xFFFFFFFFFFFFFFFF){
                        PLIB_ERR("WLS_EnqueueBlock: incorrect content pa: 0x%016lx: 0x%016lx\n", (unsigned long)retval, *ptr);
                    }
                }
            }
        }
    }

    return retval;
}

int WLS_NumBlocks(void* h)
{
    wls_us_ctx_t* pWls_us = (wls_us_ctx_t* )h;
    int n = 0;

    if(pWls_us->mode == WLS_SLAVE_CLIENT){
        // local
        n = SFL_GetNumItemsInTheQueue(&pWls_us->ul_free_block_pq);
    } else if(pWls_us->dst_kernel_va) {
        // remote
        if (pWls_us->dst_user_va)
        {
            wls_us_ctx_t* pDstWls_us = (wls_us_ctx_t* )pWls_us->dst_user_va;
            n = SFL_GetNumItemsInTheQueue(&pDstWls_us->ul_free_block_pq);
        }
    }

    return n;
}


