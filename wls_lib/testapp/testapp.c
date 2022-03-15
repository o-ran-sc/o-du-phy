/******************************************************************************
*
*   Copyright (c) 2021 Intel.
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
 * WLS interface test application
 * (contains functional unit tests and diagnostics to test wls
 *  supported by WLS interface)
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>   // for printf
#include <string.h>  // for memset
#include <signal.h>  // for SIGINT
#include <unistd.h>  // for usleep
#include <stdlib.h>  // for rand
#include <getopt.h>  // for getopt
#include <sys/time.h>
#include <pthread.h>
#include <sched.h>
#include "ttypes.h"
#include "wls_lib.h"
#include "pool.h"
#include <rte_config.h>
#include <rte_common.h>
#include <rte_eal.h>


#define HANDLE	PVOID


#define K 			1024
#define M			(K*K)

#define   DEFAULT_TEST_MEMORY_SIZE      256*M
#define   DEFAUTL_TEST_BLOCK_SIZE       16*K

#define   DEFAULT_MESSAGE_COUNT_PER_MS  10
#define   DEFAULT_MAX_MESSAGE_SIZE      2000
#define   DEFUALT_MIN_MESSAGE_SIZE      100

#define   APP_QUEUE_SIZE   255   /* number of elements each queue of the WLS  being registered will have */
#define   MAX_MESSAGES  1000   /* per ms */

U32 nCRC_Fail = 0;
U32 nCRC_Pass = 0;

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

typedef enum {
    APP_TC_SANITY_TEST = 0,
} APP_TEST_CASES;

typedef struct tagAPP_PARAMS {
    char *wls_dev_name;
    int aff_core;
    int test_id;
    int rx_id;
    int tx_id;
    int n_messages;
    int max_size;
    int min_size;
    int interface_count;
    U8 master;
    U8 debug;
    U8 crc;
    U8 trusted;
} APP_PARAMS, *PAPP_PARAMS;

typedef struct tagAPP_MESSAGE {
    U32 id;
} APP_MESSAGE, *PAPP_MESSAGE;

typedef struct tagAPP_CONTEXT {
    V32 ExitStatus;
    HANDLE hWls;

    U32 master;

    PVOID shm_memory;

    POOL Pool; // The pool descriptor
    void* PoolStrPtr; // The pool storage pointer to keep indexes

    U16 RxID;
    U16 TxID;

    U16 nInterfaces; // number of RX identifiers used by the APP
    //
    U16 InitQueueSize; // for invalid messages test (to trigger WLS  blocking)

    //
    U32 MsgPerMs;
    U32 MaxMsgSize;
    U32 MinMsgSize;

    U32 TxCnt;
    U32 RxCnt;

    U64 nTxMsgs; // Messages transmitted
    U64 nTxOctets; // Octets transmitted
    U64 nRxMsgs; // Messages received
    U64 nRxOcters; // Octets received
    U64 Cycles; // number of 1ms cycles

    int AppSanityMsgSize; // 4 or 8 depending on CRC feature
    U8 Debug; // when TRUE app cycle is 1 sec, otherwise 1ms
    U8 TrustedDataSource; // for trusted data sources ICC service removes msg validity checking.

    void (*Receive)(HANDLE h);
    void (*Transmit)(HANDLE h);

    void (*ThreadReceive)(HANDLE h);
    void (*ThreadTransmit)(HANDLE h);

    int (*wls_put)(void* h, unsigned long long pMsg, unsigned int MsgSize, unsigned short MsgTypeID, unsigned short Flags);
    unsigned long long (*wls_get)(void* h, unsigned int *MsgSize, unsigned short *MsgTypeID, unsigned short *Flags);
    unsigned long long (*wls_wget)(void* h, unsigned int *MsgSize, unsigned short *MsgTypeID, unsigned short *Flags);

    U8 *pLastRx; // used for scatter-gather test
    U32 LastRxSize; // used for scatter-gather test

    U32 *pServiceBuffer;

    U32 TxMsgCnt;
    PVOID TxMessages[MAX_MESSAGES];
    U32 TxMessageSizes[MAX_MESSAGES]; // <-- required for Ping-Pong test to store received sizes
    int core;

} APP_CONTEXT, *PAPP_CONTEXT;

APP_CONTEXT AppContext;

static int pool_alloc = 0;
static int pool_free = 0;

static void ShowData(void* ptr, unsigned int size)
{
    U8 *d = ptr;
    unsigned int i;

    for (i = 0; i < size; i++) {
        if (!(i & 0xf))
            printf("\n");
        printf("%02x ", d[i]);
    }
    printf("\n");
}

static void App_SigExitCallback(int signum)
{
    (void) signum;
    AppContext.ExitStatus = TRUE;
}

static void* WlsVaToPa(void * ptr)
{
    PAPP_CONTEXT pCtx = &AppContext;
    return (void*) WLS_VA2PA(pCtx->hWls, ptr);
}

static void* WlsPaToVa(void * ptr)
{
    PAPP_CONTEXT pCtx = &AppContext;
    return (void*) WLS_PA2VA(pCtx->hWls, (U64) ptr);
}

static void* App_Alloc(void* h, unsigned long size)
{
    (void) h;
    (void) size;
    void * retval = NULL;
    if (AppContext.master) {
        retval = PoolAlloc(&(AppContext.Pool));
        //printf("pPool->FreeGet  %d == pPool->FreePut %d\n", AppContext.Pool.FreeGet, AppContext.Pool.FreePut);
    } else {
        retval = (void*) WLS_DequeueBlock(AppContext.hWls);
        if (retval)
            retval = (void*) WlsPaToVa(retval);
        else
            printf("WLS_DequeueBlock returned null\n");
    }

    if (retval == NULL) {
        printf("no memory %d %d\n", pool_alloc, pool_free);
        exit(-1);
    } else
        pool_alloc++;

    return retval;
}

static int App_Free(void* h, void* pMsg)
{
    (void) h;
    if (AppContext.master)
        if (pMsg) {
            pool_free++;
            return (PoolFree(&(AppContext.Pool), pMsg) == 1 ? 0 : -1);
        } else {
            printf("Free Null pointer\n");
            exit(-1);
        } else
        return 0;
}

static int App_MemoryInit(void* h, unsigned long size, U32 BlockSize)
{
    int ret = 0;
    unsigned long long* pUsed;
    unsigned long long* pFree;
    PAPP_CONTEXT pCtx = &AppContext;
    U32 nBlocksSlave = 0;

    U32 nElmNum = size / BlockSize - 1;

    // We need to allocate the memory for indexes and to initialize the
    // pool descriptor, (x+1) is used to prevent queues overflow

    pCtx->PoolStrPtr = malloc((nElmNum + 1) * 4 * sizeof (unsigned long long));

    if (pCtx->PoolStrPtr == NULL)
        return -1;

    pFree = (unsigned long long*) pCtx->PoolStrPtr;
    pUsed = pFree + (nElmNum + 1);

    ret = PoolInit(&pCtx->Pool, h, nElmNum, BlockSize, pFree, pUsed);

    if (ret == 0) {

        if (AppContext.master) {
            int res = TRUE;
            /* allocate blocks for Slave to Master transmittion */
            while (res) {
                void* pBlock = App_Alloc(AppContext.hWls, DEFAUTL_TEST_BLOCK_SIZE);
                if (pBlock) {
                    res = WLS_EnqueueBlock(AppContext.hWls, (U64) WlsVaToPa(pBlock));
                    if (res)
                        nBlocksSlave++;
                    else
                        App_Free(AppContext.hWls, pBlock);
                } else
                    res = FALSE;
            }
            printf("Slave has %d free blocks\n", nBlocksSlave);
        }
    }

    return ret;
}

/********************************/

const U8 mb_table_level1[] = {
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
};

const U8 mb_table_level2[] = {
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2,
    0xC6, 0x06, 0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04,
    0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E,
    0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09, 0x08, 0xC8,
    0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A,
    0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC,
    0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6,
    0xD2, 0x12, 0x13, 0xD3, 0x11, 0xD1, 0xD0, 0x10,
    0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32,
    0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,
    0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE,
    0xFA, 0x3A, 0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38,
    0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA,
    0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED, 0xEC, 0x2C,
    0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
    0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0,
    0xA0, 0x60, 0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62,
    0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4,
    0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F, 0x6E, 0xAE,
    0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,
    0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA,
    0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C,
    0xB4, 0x74, 0x75, 0xB5, 0x77, 0xB7, 0xB6, 0x76,
    0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0,
    0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92,
    0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54,
    0x9C, 0x5C, 0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E,
    0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98,
    0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A, 0x4A,
    0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
    0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86,
    0x82, 0x42, 0x43, 0x83, 0x41, 0x81, 0x80, 0x40
};

#define CRC32_INIT_VAL  0xFFFFFFFF
#define CRC32_DIVISOR   0xA0000001

static U32 ICC_CRC32(U8 *pData, U32 size)
{
    U32 retval = CRC32_INIT_VAL;
    U8 i, tmp;

    if (!size)
        return CRC32_INIT_VAL; // mean CRC error
    do {
        retval ^= *pData++;
        for (i = 8; i > 0; --i) {
            tmp = retval & 0x01;
            retval >>= 1;
            if (tmp) {
                retval ^= CRC32_DIVISOR;
            }
        }
    } while (--size);
    return retval;
}


static int app_PutMessageCRC(void* h, unsigned long long pMsg, unsigned int MsgSize, unsigned short MsgTypeID, unsigned short Flags)
{
    U8 *p;
    U64 pMsgVa = (U64) WlsPaToVa((void*) pMsg);

    if (pMsgVa == 0)
    {
        return 0;
    }
    p = (U8 *) pMsgVa;

    U32 crc = ICC_CRC32((U8 *) pMsgVa, MsgSize - sizeof (crc));

    // CRC32
    p[MsgSize - 4] = (crc >> 0) & 0xff;
    p[MsgSize - 3] = (crc >> 8) & 0xff;
    p[MsgSize - 2] = (crc >> 16) & 0xff;
    p[MsgSize - 1] = (crc >> 24) & 0xff;

    return WLS_Put(h, (unsigned long long) pMsg, MsgSize, MsgTypeID, Flags);
}

static unsigned long long app_GetMessageCRC(void* h, unsigned int *MsgSize, unsigned short *MsgTypeID, unsigned short *Flags)
{
    U64 pMsgPa = WLS_Get(h, MsgSize, MsgTypeID, Flags);

    if (pMsgPa) {
    U64 pMsg = (U64) WlsPaToVa((void*) pMsgPa);

    if (pMsg) {
        U32 size = *MsgSize;
        U32 crc = ICC_CRC32((U8*) pMsg, size);

        if (crc != 0) {
                nCRC_Fail++;
            printf("CRC error detected for message %p, size_%lu\n", (void*) pMsg, (long) size);
            ShowData((U8*) pMsg, size);
        }
            else {
                if (nCRC_Pass == 0) {
                    printf("Example of Msg Size and Content being sent: %d\n", size);
                    ShowData((U8*) pMsg, size);
                }
                nCRC_Pass++;
            }
        }
    }
    return pMsgPa;
}

static unsigned long long app_WGetMessageCRC(void* h, unsigned int *MsgSize, unsigned short *MsgTypeID, unsigned short *Flags)
{
    U64 pMsgPa = WLS_WGet(h, MsgSize, MsgTypeID, Flags);

    if (pMsgPa) {
    U64 pMsg = (U64) WlsPaToVa((void*) pMsgPa);

    if (pMsg) {
        U32 size = *MsgSize;
        U32 crc = ICC_CRC32((U8*) pMsg, size);

        if (crc != 0) {
                nCRC_Fail++;
            printf("CRC error detected for message %p, size_%lu\n", (void*) pMsg, (long) size);
            ShowData((U8*) pMsg, size);
        }
            else {
                if (nCRC_Pass == 0) {
                    printf("Example of Msg Size and Content being sent: %d\n", size);
                    ShowData((U8*) pMsg, size);
                }
                nCRC_Pass++;
            }
        }
    }
    return pMsgPa;
}

static void CreateMessage(PAPP_MESSAGE p, U32 size)
{
    (void) size;
    p->id = AppContext.TxCnt++;
}

static void CheckMessage(PAPP_MESSAGE p, U32 size)
{
    if (AppContext.RxCnt && p->id != AppContext.RxCnt) {
        //		char buf[8*K];
        printf("rx message(id_%llu)_%lx error expected_%lu, received_%lu\n", (long long) AppContext.nRxMsgs, (U64) p, (long) AppContext.RxCnt, (long) p->id);
        ShowData(p, size);
        //		if (TL_GetStatistics(AppContext.hWls, buf, sizeof(buf)))
        //		printf("%s", buf);
    }

    AppContext.RxCnt = p->id;
    AppContext.RxCnt += 1;
}

/**
 *******************************************************************************
 *
 * @fn    app_AllocMultiple
 * @brief used to allocate multiple blocks of the same size from the WLS
 *
 * @param[h]  hWls - app thread WLS  handle
 * @param[o]  pMsgs - ptr to beginning of array of points to allocated blocks
 * @param[o]  pMsgSizes - array to write size for each allocated blocks
 * @param[i]  nMsgs - number of blocks to allocate
 * @return    U32 - number of allocated blocks
 *
 * @description
 *    The routine is used allocate multiple blocks from the ICC service,
 * the blocks are supposed to be same size blocks, satisfying
 * appContext.MaxMsgSize parameter.
 *    In case the service is unable to provide requested number of blocks,
 * smaller count is allocated. The routine returns actual number of allocated
 * blocks
 *
 * @references
 * MS-111070-SP
 *
 * @ingroup icc_service_unit_test
 *
 ******************************************************************************/
static U32 app_AllocMultiple(HANDLE hWls, PVOID *pMsgs, U32 *pMsgSizes, U32 nMsgs)
{
    unsigned n = 0;
    unsigned i, j;

    memset(pMsgs, 0x00, sizeof (PVOID) * nMsgs);

    while (nMsgs--) {
        pMsgs[n] = App_Alloc(hWls, AppContext.MaxMsgSize);
        pMsgSizes[n] = AppContext.MaxMsgSize;
        if (!pMsgs[n]) {
            printf("empty pool allocated_%u out of %lu\n", n, (long) AppContext.MsgPerMs);
            break;
        }
        n += 1;
    }

    // check for duplicated pointers
    for (i = 0; i < n; i++) {
        for (j = i + 1; j < n; j++) {
            if (pMsgs[i] == pMsgs[j]) {
                printf("duplicated pointer %p (msg_id1_%u, msg_id2_%u)\n", pMsgs[i], i, j);
                break;
            }
        }
    }

    return n;
    //ShowData(TxMessages, sizeof(TxMessages));
}

/**
 *******************************************************************************
 *
 * @fn    app_SanityTestTransmitter
 * @brief transmitter of default test case (0).
 *
 * @param[h]  hWls - app thread WLS  handle
 * @return    void
 *
 * @description
 *    The routine is used in test case 0 (non-blocking sanity unit test)
 * The transmitter does allocate multiple blocks of the same size from the ICC
 * service. Then it fills each block with incremental counter and transfers
 * to other application specified by parameter TxID.
 *
 * @references
 * MS-111070-SP
 *
 * @ingroup icc_service_unit_test
 *
 ******************************************************************************/
static void app_SanityTestTransmitter(HANDLE hWls)
{
    U8 *pMsg;
    unsigned n = app_AllocMultiple(hWls, AppContext.TxMessages, AppContext.TxMessageSizes, AppContext.MsgPerMs);
    unsigned fn = n;
    unsigned cnt = 0;
    unsigned k = 0;
    unsigned alloc = n;

    // lets transmit some message for test
    while (n--) {
        pMsg = AppContext.TxMessages[cnt++];
        if (pMsg) {
            U32 size = (rand() % AppContext.MaxMsgSize);

            if (size < AppContext.MinMsgSize)
                size = AppContext.MinMsgSize;

            memset(pMsg, cnt, size);
            CreateMessage((PAPP_MESSAGE) pMsg, size);
            if ((AppContext.wls_put(hWls, (U64) WlsVaToPa(pMsg), size, AppContext.TxID, 0) != 0)) {
                printf("could not send the message_%p\n", pMsg);
                break;
            } else {
                k++;
            }
            AppContext.nTxOctets += size;
            AppContext.nTxMsgs += 1;
        }
    }

    if (alloc != k)
        printf("inorrect sent %d alloc %d \n", k, alloc);

    cnt = 0;
    while (fn--) {
        pMsg = AppContext.TxMessages[cnt++];
        if (pMsg) {
            if (App_Free(hWls, pMsg) != 0)
                printf("could not release the message_%p\n", pMsg);
        } else
            printf("pMsg is NULL [%d]\n", cnt);
    }
    if (cnt != k) {
        printf("inorrect free sent %d free %d \nQuiting...\n", k, cnt);
        AppContext.ExitStatus = 1;
    }
}


/**
 *******************************************************************************
 *
 * @fn    app_SanityTestReceiver
 * @brief default sanity checking receiver used in multiple tests.
 *
 * @param[h]  hWls - app thread WLS  handle
 * @return    void
 *
 * @description
 *    The routine takes received messages and checks the sanity incremental
 * counter to confirm the order. In case the counter does not correspond to
 * expected counter (misordered message or incorrect message) an error is
 * printed to STDOUT.
 *
 * @references
 * MS-111070-SP
 *
 * @ingroup icc_service_unit_test
 *
 ******************************************************************************/
static void app_SanityTestReceiver(HANDLE hWls)
{
    (void) hWls;
    U32 MsgSize;
    U8 *pMsg;
    U8 *pMsgPa;
    U8 *pMsgVa;
    U8 TempBuf[16 * K];
    unsigned short MsgTypeID;
    unsigned short Flags;
    U32 nBlocksSlave = 0;

    // handle RX receiver
    while (((pMsgPa = (U8 *) AppContext.wls_get(AppContext.hWls, &MsgSize, &MsgTypeID, &Flags)) != NULL)) {
        pMsgVa = (U8 *) WlsPaToVa(pMsgPa);

        if (pMsgVa == NULL) {
            printf("va: %lx pa: %lx\n", (long) pMsgVa, (long) pMsgPa);
            continue;
        }

        pMsg = pMsgVa;

        if (((U64) pMsg & 0x3) == 0) {
            // aligned message
            CheckMessage((PAPP_MESSAGE) pMsg, MsgSize);
        } else {
            // misaligned message
            printf("Unaligned message\n");
            MsgSize = (MsgSize > sizeof (TempBuf)) ? sizeof (TempBuf) : MsgSize;
            memcpy(TempBuf, pMsg, MsgSize);
            // handle received message
            CheckMessage((PAPP_MESSAGE) TempBuf, MsgSize);
        }
        App_Free(AppContext.hWls, pMsg);
        AppContext.nRxOcters += MsgSize;
        AppContext.nRxMsgs += 1;

        if (AppContext.master) {
            int res = TRUE;
            /* allocate blocks for Slave to Master transmittion */
            while (res) {
                void* pBlock = App_Alloc(AppContext.hWls, DEFAUTL_TEST_BLOCK_SIZE);
                if (pBlock) {
                    res = WLS_EnqueueBlock(AppContext.hWls, (U64) WlsVaToPa(pBlock));
                    if (res)
                        nBlocksSlave++;
                    else
                        App_Free(AppContext.hWls, pBlock);
                } else
                    res = FALSE;
            }
        }

    }
}


/******************************************************************************
 *                                                                             *
 *                       Application common routines                           *
 *                                                                             *
 ******************************************************************************/

/**
 *******************************************************************************
 *
 * @fn    app_UpdateStatistics
 * @brief is used to update RX and TX statistics
 *
 * @param[n]  void
 * @return    void
 *
 * @description
 *    The routine prints out the statistics of received and transmitted
 * messages.
 *
 * @references
 * MS-111070-SP
 *
 * @ingroup icc_service_unit_test
 *
 ******************************************************************************/

static void app_UpdateStatistics(void)
{
    AppContext.Cycles += 1;

    if (AppContext.Debug || AppContext.Cycles % 1000 == 0) {
        printf("Rx(id_%u) (%llu) - (%llu KiB)\n", AppContext.RxID, (long long) AppContext.nRxMsgs, (long long) AppContext.nRxOcters >> 10);
        printf("Tx(id_%u) (%llu) - (%llu KiB)\n", AppContext.TxID, (long long) AppContext.nTxMsgs, (long long) AppContext.nTxOctets >> 10);
    }

}

/**
 *******************************************************************************
 *
 * @fn    app_Help
 * @brief prints app help content
 *
 * @param[n]  void
 * @return    void
 *
 * @description
 *    The routine is used to print help content to stdout.
 *
 * @references
 * MS-111070-SP
 *
 * @ingroup icc_service_unit_test
 *
 ******************************************************************************/
static void app_Help(void)
{
    char help_content[] =  \
			"WLS test application\n\n"\
			"Usage: testapp [-c <test>] [-r <rxid>] [-t <txid>] [-n <msgcount>]\n\n"\
			"supports the following parameters:\n\n"
                        "-c | --testcase <test number>     0 - default sanity test\n"\
			"                                  1 - misaligned pointers test\n"\
			"                                  2 - aligned 4 pointers test\n"\
			"                                  3 - random pools test\n"\
			"                                  4 - ping-pong (ZBC test)\n"\
			"                                  5 - invalid messages test\n\n"\
			"--trusted                    switches WLS  to trusted mode\n"\
			"-r | --rxid <id>             used to specify RxTypeID\n"\
			"-t | --txid <id>             used to specify TxTypeID\n"\
			"-n | --msgcount <count>      used to specify number of messages per timeframe\n"\
			"-l | --minsize  <size>       specifies MIN message size in bytes\n"\
			"-s | --maxsize  <size>       specifies MAX message size in bytes\n"\
			"--crc                        enables CRC generation and checking\n"\
			"--debug                      increases sleep interval to 1 second\n"\
			"-m | --master                set predefined rxid and txid\n";

    printf("%s", help_content);
}

/**
 *******************************************************************************
 *
 * @fn    app_ParseArgs
 * @brief is used to parse incoming app args
 *
 * @param[i]  argc - app arg count
 * @param[i]  argv - array of args
 * @param[o]  params - app startup params filled basing on args parse
 * @return    number of parsed args
 *
 * @description
 *    The routine is parse input args and convert them into app startup params
 *
 * @references
 * MS-111070-SP
 *
 * @ingroup icc_service_unit_test
 *
 ******************************************************************************/
static int app_ParseArgs(int argc, char ** argv, PAPP_PARAMS params)
{
    int c;
    int *pInt;
    int cnt = 0;

    struct option long_options[] = {
        {"wlsdev", required_argument, 0, 'w'},
        {"affinity", required_argument, 0, 'a'},
        {"testcase", required_argument, 0, 'c'},
        {"rxid", required_argument, 0, 'r'},
        {"txid", required_argument, 0, 't'},
        {"msgcount", required_argument, 0, 'n'},
        {"maxsize", required_argument, 0, 's'},
        {"minsize", required_argument, 0, 'l'},
        {"master", no_argument, 0, 'm'},
        {"debug", no_argument, 0, 'd'}, /* slow down the app cycle from 1ms to 1s*/
        {"icount", required_argument, 0, 'i'},
        {"crc", no_argument, 0, 1},
        {"trusted", no_argument, 0, 2},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    memset(params, 0, sizeof (*params));

    // set default values here
    params->interface_count = 1;

    while (1) {
        //int this_option_optind = optind ? optind : 1;
        int option_index = 0;

        c = getopt_long(argc, argv, "a:w:c:r:t:n:s:l:mdi:h", long_options, &option_index);

        if (c == -1)
            break;

        cnt += 1;
        pInt = NULL;

        switch (c) {
            case 'a': // test Case selection
                pInt = &params->aff_core;
                break;
            case 'c': // test Case selection
                pInt = &params->test_id;
                break;
            case 'r': // rx id selection
                pInt = &params->rx_id;
                break;
            case 't': // tx id selection
                pInt = &params->tx_id;
                break;
            case 's': // select message size
                pInt = &params->max_size;
                break;
            case 'l': // select message size
                pInt = &params->min_size;
                break;
            case 'n': // select number of messages
                pInt = &params->n_messages;
                break;
            case 'i': // select number of interfaces to register
                pInt = &params->interface_count;
                break;
            case 'm':
                params->master = TRUE;
                break;
            case 'd':
                params->debug = TRUE;
                break;
            case 'w':
                params->wls_dev_name = optarg;
                break;
            case 'h':
                app_Help();
                exit(0);
            case 2:
                params->trusted = TRUE;
                break;
            case 1: // crc checking enabled
                params->crc = TRUE;
                break;
        }

        if (pInt && optarg) {
            // get int arg
            if (optarg[0] == '0' && (optarg[1] == 'x' || optarg[1] == 'X')) {
                sscanf(optarg, "%x", (unsigned *) pInt);
            } else {
                *pInt = atoi(optarg);
            }
        }
    }
    return cnt;
}

static int app_set_affinity(int coreNum)
{
    cpu_set_t cpuset;
    int i, rc;

    /* set main thread affinity mask to CPU7 */

    CPU_ZERO(&cpuset);
    CPU_SET(coreNum, &cpuset);

    rc = pthread_setaffinity_np(pthread_self(), sizeof (cpu_set_t), &cpuset);
    if (rc) {
        perror("pthread_setaffinity_np failed");
        printf("pthread_setaffinity_np failed: %d", rc);
    }

    /* check the actual affinity mask assigned to the thread */

    CPU_ZERO(&cpuset);

    rc = pthread_getaffinity_np(pthread_self(), sizeof (cpu_set_t), &cpuset);

    if (rc) {
        perror("pthread_getaffinity_np failed");
        printf("pthread_getaffinity_np failed: %d", rc);
    }

    printf("set affinity: ");
    for (i = 0; i < CPU_SETSIZE; i++)
        if (CPU_ISSET(i, &cpuset))
            printf("    CPU %d\n", i);

    if (!CPU_ISSET(coreNum, &cpuset)) {
        printf("affinity failed");
    }

    /**
       A new thread created by pthread_create(3) inherits a copy of its
       creator's CPU affinity mask. */

    return rc;
}

/**
 *******************************************************************************
 *
 * @fn    app_ApplyParams
 * @brief is used to apply application startup parameters
 *
 * @param[i]  params - app startup params
 * @return    void
 *
 * @description
 *    The applies startup parameters
 *
 * @references
 * MS-111070-SP
 *
 * @ingroup icc_service_unit_test
 *
 ******************************************************************************/
static void app_ApplyParams(PAPP_PARAMS params)
{
    // apply parameters
    printf("selected test case %d - ", params->test_id);
    switch (params->test_id) {
        case APP_TC_SANITY_TEST:
        default:
            printf("NON-BLOCKING SANITY TEST\n");
            AppContext.Receive = app_SanityTestReceiver;
            AppContext.Transmit = app_SanityTestTransmitter;
            break;
    }

    AppContext.wls_put = WLS_Put;
    AppContext.wls_get = WLS_Get;
    AppContext.wls_wget = WLS_WGet;

    AppContext.MsgPerMs = DEFAULT_MESSAGE_COUNT_PER_MS;
    AppContext.MaxMsgSize = DEFAULT_MAX_MESSAGE_SIZE;
    AppContext.MinMsgSize = DEFUALT_MIN_MESSAGE_SIZE;
    AppContext.AppSanityMsgSize = sizeof (APP_MESSAGE);

    if (params->master) {
        printf("WLS test app (supposed to run as MEMORY MASTER)\n");
        AppContext.master = TRUE;
        AppContext.RxID = 1;
        AppContext.TxID = 2;
    } else {
        AppContext.master = FALSE;
        AppContext.RxID = 2;
        AppContext.TxID = 1;
    }

    if (params->rx_id)
        AppContext.RxID = params->rx_id;

    if (params->tx_id)
        AppContext.TxID = params->tx_id;

    if (params->n_messages && params->n_messages < MAX_MESSAGES)
        AppContext.MsgPerMs = params->n_messages;

    if (params->min_size && params->min_size >= 4)
        AppContext.MinMsgSize = params->min_size;

    // default is 1 RX interface
    printf("if count = %u\n", params->interface_count);
    AppContext.nInterfaces = 1;
    if (params->interface_count == 0) {
        printf("WLS test app started as simple data source, no RX ID will be specified\n");
        AppContext.nInterfaces = 0;
        AppContext.RxID = 0; // override RxID
    } else if (params->interface_count <= 7) {
        AppContext.nInterfaces = params->interface_count;
    }


    AppContext.TrustedDataSource = params->trusted;

    if (params->crc) {
        if (AppContext.MinMsgSize < 8)
            AppContext.MinMsgSize = 8;

        AppContext.wls_put = app_PutMessageCRC;
        AppContext.wls_get = app_GetMessageCRC;
        AppContext.wls_wget = app_WGetMessageCRC;

        AppContext.AppSanityMsgSize += 4; // + sizeof CRC
    }

    if (params->max_size && params->max_size <= 16 * K)
        AppContext.MaxMsgSize = params->max_size;

    if (params->max_size < params->min_size)
        params->max_size = params->min_size;

    AppContext.Debug = params->debug;

    if (params->aff_core) {
        AppContext.core = params->aff_core;
        app_set_affinity(AppContext.core);
    }

    printf("The application started with:\n");
    printf("Core ................ %d\n", AppContext.core);
    printf("Rx interface count .. %d\n", AppContext.nInterfaces);
    printf("RxID ................ %d\n", AppContext.RxID);
    printf("TxID ................ %d\n", AppContext.TxID);
    if (AppContext.Debug)
        printf("Generating .......... %lu Messages per second (DEBUG MODE)\n", (long) AppContext.MsgPerMs);
    else
        printf("Generating .......... %lu Messages per ms\n", (long) AppContext.MsgPerMs);
    printf("Max Message Size .... %lu bytes\n", (long) AppContext.MaxMsgSize);
    printf("Min Message Size .... %lu bytes\n", (long) AppContext.MinMsgSize);
    printf("Number of threads ... 1\n");
    printf("CRC checking ........ ");
    if (params->crc)
        printf("ENABLED\n");
    else
        printf("DISABLED\n");
}

/**
 *******************************************************************************
 *
 * @fn    app_ReleaseAllocatedBuffers
 * @brief releases ICC buffers allocated by the application
 *
 * @param[n]  void
 * @return    void
 *
 * @description
 *    In process of making some tests when signal to close the application
 * happens the app may keep some allocated buffers from the ICC pools. This
 * routine does release these buffers back to ICC.
 *
 * @references
 * MS-111070-SP
 *
 * @ingroup icc_service_unit_test
 *
 ******************************************************************************/
static void app_ReleaseAllocatedBuffers(void)
{
    if (AppContext.TxMsgCnt && AppContext.master)
        do {
            AppContext.TxMsgCnt -= 1;
            App_Free(AppContext.hWls, AppContext.TxMessages[ AppContext.TxMsgCnt ]);
        } while (AppContext.TxMsgCnt != 0);
}

/**
 *******************************************************************************
 *
 * @fn    main
 * @brief ICC test application main routine
 *
 * @param[n]  void
 * @return    void
 *
 * @description
 *    Contains logic of the test (one RX/TX thread)
 *
 * @references
 * MS-111070-SP
 *
 * @ingroup icc_service_unit_test
 *
 ****************************************************************************/
int main(int argc, char* argv[])
{
    int retval = 0;
    APP_PARAMS params;
    uint64_t nWlsMacMemorySize = DEFAULT_TEST_MEMORY_SIZE, nWlsPhyMemorySize = 0;
    uint32_t nLoop = 30000;
    uint32_t nNumBlks = 0;

    signal(SIGINT, App_SigExitCallback);

    memset(&AppContext, 0, sizeof (AppContext));
    memset(&params, 0, sizeof (params));

    nCRC_Fail = 0;
    nCRC_Pass = 0;

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

    argc -= ret;
    argv += ret;

    app_ParseArgs(argc, argv, &params);
    app_ApplyParams(&params);

    AppContext.InitQueueSize = APP_QUEUE_SIZE;

    AppContext.hWls = WLS_Open(params.wls_dev_name, !AppContext.master, &nWlsMacMemorySize, &nWlsPhyMemorySize);

    if (!AppContext.hWls) {
        printf("could not register WLS client\n");
        return 1;
    } else {
        printf("WLS has been registered\n");
    }

    WLS_SetMode(AppContext.hWls, AppContext.master);
    AppContext.shm_memory = WLS_Alloc(AppContext.hWls, DEFAULT_TEST_MEMORY_SIZE);

    if (AppContext.shm_memory == NULL) {
        if (AppContext.master)
            printf("could not create WLS shared memory\n");
        else
            printf("could not attach WLS shared memory\n");

        return -1;
    }

    if (AppContext.master) {
        if (App_MemoryInit(AppContext.shm_memory, DEFAULT_TEST_MEMORY_SIZE, DEFAUTL_TEST_BLOCK_SIZE) != 0) {
            WLS_Free(AppContext.hWls, AppContext.shm_memory);
            WLS_Close(AppContext.hWls);
            exit(1);
        }

    }

    ret = WLS_Ready(AppContext.hWls);
    if (ret) {
        printf("wls not ready\n");
        return -1;
    }

    nNumBlks = WLS_Check(AppContext.hWls);
    printf("There are %d blocks in queue from WLS_Check\n", nNumBlks);

    nNumBlks = WLS_NumBlocks(AppContext.hWls);
    printf("There are %d blocks in queue from WLS_NumBlocks\n", nNumBlks);

    // APPLICATION MAIN LOOP
    while (!AppContext.ExitStatus && (AppContext.Receive || AppContext.Transmit) && nLoop) {
        if (AppContext.Receive)
            AppContext.Receive(AppContext.hWls);

        if (AppContext.Debug)
            //usleep(10000); // 1 sec delay
            sleep(1); // 1 sec delay
        else
            usleep(1000); // 1 ms delay

        if (AppContext.Transmit)
            AppContext.Transmit(AppContext.hWls);

        app_UpdateStatistics();
        nLoop--;
    }

    app_ReleaseAllocatedBuffers();
    printf("deregistering WLS  (TxTotal_%lld, RxTotal_%lld)\n", (long long) AppContext.nTxMsgs, (long long) AppContext.nRxMsgs);
    if (params.crc)
    {
        printf("Number of CRC Pass %d\n", nCRC_Pass);
        printf("Number of CRC Fail %d\n", nCRC_Fail);
        printf("Total Message sent: %d\n", (nCRC_Pass + nCRC_Fail));
    }
    WLS_Free(AppContext.hWls, AppContext.shm_memory);
    WLS_Close(AppContext.hWls);
    return retval;
}
