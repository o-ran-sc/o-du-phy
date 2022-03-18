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

#ifndef _WLS_LIB_H_
#define _WLS_LIB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** WLS driver client operates as slave in terms of management of shared memory */
#define WLS_SLAVE_CLIENT   0
/** WLS driver client operates as master in terms of management of shared memory */
#define WLS_MASTER_CLIENT  1
/** WLS Open Dual Options */
#define WLS_SEC_NA     0
#define WLS_SEC_MASTER 1
/** WLS Open Dual enabled */
#define WLS_SINGLE_MODE 0
#define WLS_DUAL_MODE   1


/* definitions PUT/GET Flags */
#define WLS_TF_SCATTER_GATHER  (1 << 15)
#define WLS_TF_URLLC           (1 << 10)
#define WLS_TF_SYN             (1 << 9)
#define WLS_TF_FIN             (1 << 8)
#define WLS_FLAGS_MASK         (0xFF00)

/** First block in Scatter/Gather sequence of blocks */
#define WLS_SG_FIRST               (WLS_TF_SCATTER_GATHER | WLS_TF_SYN)
/** Next block in Scatter/Gather sequence of blocks */
#define WLS_SG_NEXT                (WLS_TF_SCATTER_GATHER)
/** Last block in Scatter/Gather sequence of blocks */
#define WLS_SG_LAST                (WLS_TF_SCATTER_GATHER | WLS_TF_FIN)

//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
 *
 *  @param[in]   ifacename - pointer to string with device driver name (/dev/wls)
 *  @param[in]   mode         - mode of operation (Master or Slave)
 *  @param[in]   nWlsMacMemorySize - Pointer with size of Memory blocks managed by MAC
 *  @param[in]   nWlsPhyMemorySize - Pointer with size of Memory blocks managed by L1 (SRS Channel Estimates)
 *
 *  @return  pointer to WLS handle
 *
 *  @description
 *  Function opens the WLS interface and registers as instance in the kernel space driver.
 *  Control section of shared memory is mapped to application memory.
 *  pointer (handle) of WLS interface is returned for future use by WLS functions
 *
**/
//-------------------------------------------------------------------------------------------
void* WLS_Open(const char *ifacename, unsigned int mode, uint64_t *nWlsMacMemorySize, uint64_t *nWlsPhyMemorySize);

uint32_t WLS_SetMode(void* h, unsigned int mode);
//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
 *
 *  @param[in]   ifacename - pointer to string with device driver name (/dev/wls)
 *  @param[in]   modef     - mode of operation (Master or Slave)
 *
 *  @return  pointer to second WLS handle while first WLS_handle is returned to the argument handle1 location
 *
 *  @description
 *  Function opens the WLS interface and registers two instances in the kernel space driver.
 *  Control section of shared memory is mapped to application memory.
 *  pointer (handle) of WLS interface is returned for future use by WLS functions
 *
**/
//-------------------------------------------------------------------------------------------
void* WLS_Open_Dual(const char *ifacename, unsigned int mode, uint64_t *nWlsMacMemorySize, uint64_t *nWlsPhyMemorySize, void** handle1);

//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
 *
 *  @param[in]   h - handle of WLS interface to close
 *
 *  @return  0 - in case of success
 *
 *  @description
 *  Function closes the WLS interface and deregisters as instance in the kernel space driver.
 *  Control section of shared memory is unmapped form user space application
 *
**/
//-------------------------------------------------------------------------------------------
int WLS_Close(void* h);

//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
 *
 *  @param[in]   h - handle of second WLS interface within same app to close
 *
 *  @return  0 - in case of success
 *
 *  @description
 *  Function closes a second WLS interface open from a same process and deregisters as instance in the kernel space driver.
 *  Control section of shared memory is unmapped form user space application
 *
**/
//-------------------------------------------------------------------------------------------
int WLS_Close1(void* h);
//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
 *
 *  @param[in]   h - handle of WLS interface to check status
 *
 *  @return  1 - in case of success
 *
 *  @description
 *  Function checks state of remote peer of WLS interface and returns 1 if remote peer is available
 *  (one to one connection is established)
 *
**/
//-------------------------------------------------------------------------------------------
int WLS_Ready(void* h);

//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
 *
 *  @param[in]   h - handle of second WLS interface within the same app to check status
 *
 *  @return  1 - in case of success
 *
 *  @description
 *  Function checks state of remote peer of WLS interface and returns 1 if remote peer is available
 *  (one to one connection is established)
 *
**/
//-------------------------------------------------------------------------------------------
int WLS_Ready1(void* h);
//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
 *
 *  @param[in]   h    - handle of WLS interface
 *  @param[in]   size - size of memory block to allocate
 *
 *  @return  void*    - pointer to allocated memory block or NULL if no memory available
 *
 *  @description
 *  Function allocates memory block for data exchange shared memory. Memory block is backed
 *  by huge pages.
 *
**/
//-------------------------------------------------------------------------------------------
void* WLS_Alloc(void* h, uint64_t size);

//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
*
*  @param[in]   h    - handle of WLS interface
*  @param[in]   pMsg - pointer to WLS memory
*
*  @return  0 - if operation is successful
*
*  @description
*  Function frees memory block for data exchange shared memory. Memory block is backed
*  by huge pages
*
**/
//-------------------------------------------------------------------------------------------
int WLS_Free(void* h, void* pMsg);

//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
 *
 *  @param[in]   h    - handle of WLS interface
 *  @param[in]   pMsg - pointer to memory block (physical address) with data to be transfered to remote peer.
 *                      pointer should belong to WLS memory allocated via WLS_Alloc()
 *  @param[in]   MsgSize - size of memory block to send (should be less than 2 MB)
 *  @param[in]   MsgTypeID - application specific identifier of message type
 *  @param[in]   Flags - Scatter/Gather flag if memory block has multiple chunks
 *
 *  @return  0 - if successful
 *          -1 - if error
 *
 *  @description
 *  Function puts memory block (or group of blocks) allocated from WLS memory into interface
 *  for transfer to remote peer.
 *
**/
//-------------------------------------------------------------------------------------------
int WLS_Put(void* h, unsigned long long pMsg, unsigned int MsgSize, unsigned short MsgTypeID, unsigned short Flags);

//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
 *
 *  @param[in]   h    - handle of second WLS interface within same app
 *  @param[in]   pMsg - pointer to memory block (physical address) with data to be transfered to remote peer.
 *                      pointer should belong to WLS memory allocated via WLS_Alloc()
 *  @param[in]   MsgSize - size of memory block to send (should be less than 2 MB)
 *  @param[in]   MsgTypeID - application specific identifier of message type
 *  @param[in]   Flags - Scatter/Gather flag if memory block has multiple chunks
 *
 *  @return  0 - if successful
 *          -1 - if error
 *
 *  @description
 *  Function puts memory block (or group of blocks) allocated from WLS memory into interface
 *  for transfer to remote peer.
 *
**/
//-------------------------------------------------------------------------------------------
int WLS_Put1(void* h, unsigned long long pMsg, unsigned int MsgSize, unsigned short MsgTypeID, unsigned short Flags);
//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
 *
 *  @param[in]   h    - handle of WLS interface
 *
 *  @return  number of blocks available
 *
 *  @description
 *  Function checks if there are memory blocks with data from remote peer and returns number of blocks
 *  available for "get" operation
 *
**/
//-------------------------------------------------------------------------------------------
int WLS_Check(void* h);

//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
 *
 *  @param[in]   h    - handle of second WLS interface within same app
 *
 *  @return  number of blocks available
 *
 *  @description
 *  Function checks if there are memory blocks with data from remote peer and returns number of blocks
 *  available for "get" operation
 *
**/
//-------------------------------------------------------------------------------------------
int WLS_Check1(void* h);

//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
*
*  @param[in]   h    - handle of WLS interface
*  @param[in]   *MsgSize - pointer to set size of memory block
*  @param[in]   *MsgTypeID - pointer to application specific identifier of message type
*  @param[in]   *Flags - pointer to Scatter/Gather flag if memory block has multiple chunks
*
*  @return  pointer to memory block (physical address) with data received from remote peer
*           NULL -  if error
*
*  @description
*  Function gets memory block from interface received from remote peer. Function is non-blocking
*  operation and returns NULL if no blocks available
*
**/
//-------------------------------------------------------------------------------------------
unsigned long long WLS_Get(void* h, unsigned int *MsgSize, unsigned short *MsgTypeID, unsigned short *Flags);


//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
*
*  @param[in]   h    - handle of second WLS interface within same app
*  @param[in]   *MsgSize - pointer to set size of memory block
*  @param[in]   *MsgTypeID - pointer to application specific identifier of message type
*  @param[in]   *Flags - pointer to Scatter/Gather flag if memory block has multiple chunks
*
*  @return  pointer to memory block (physical address) with data received from remote peer
*           NULL -  if error
*
*  @description
*  Function gets memory block from interface received from remote peer. Function is non-blocking
*  operation and returns NULL if no blocks available
*
**/
//-------------------------------------------------------------------------------------------
unsigned long long WLS_Get1(void* h, unsigned int *MsgSize, unsigned short *MsgTypeID, unsigned short *Flags);

//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
*
*  @param[in]   h    - handle of WLS interface
*
*  @return  number of blocks available for get
*
*  @description
*  Function waits for new memory block from remote peer. Function is blocking call and returns number
*  of blocks received.
*
**/
//-------------------------------------------------------------------------------------------
int WLS_Wait(void* h);

//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
*
*  @param[in]   h    - handle second of WLS interface within same app
*
*  @return  number of blocks available for get
*
*  @description
*  Function waits for new memory block from remote peer. Function is blocking call and returns number
*  of blocks received.
*
**/
//-------------------------------------------------------------------------------------------
int WLS_Wait1(void* h);

//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
*
*  @param[in]   h    - handle of WLS interface
*
*  @return  0 - if successful
*
*  @description
*  Function performs "wakeup" notification to remote peer to unblock "wait" operations pending
*
**/
//-------------------------------------------------------------------------------------------
int WLS_WakeUp(void* h);

//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
*
*  @param[in]   h    - handle of second  WLS interface within same app
*
*  @return  0 - if successful
*
*  @description
*  Function performs "wakeup" notification to remote peer to unblock "wait" operations pending
*
**/
//-------------------------------------------------------------------------------------------
int WLS_WakeUp1(void* h);
//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
*
*  @param[in]   h    - handle of WLS interface
*  @param[in]   *MsgSize - pointer to set size of memory block
*  @param[in]   *MsgTypeID - pointer to application specific identifier of message type
*  @param[in]   *Flags - pointer to Scatter/Gather flag if memory block has multiple chunks
*
*  @return  pointer to memory block (physical address) with data received from remote peer
*           NULL -  if error
*
*  @description
*  Function gets memory block from interface received from remote peer. Function is blocking
*  operation and waits till next memory block from remote peer.
*
**/
//-------------------------------------------------------------------------------------------
unsigned long long WLS_WGet(void* h, unsigned int *MsgSize, unsigned short *MsgTypeID, unsigned short *Flags);

//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
*
*  @param[in]   h    - handle of second WLS interface within the same app
*  @param[in]   *MsgSize - pointer to set size of memory block
*  @param[in]   *MsgTypeID - pointer to application specific identifier of message type
*  @param[in]   *Flags - pointer to Scatter/Gather flag if memory block has multiple chunks
*
*  @return  pointer to memory block (physical address) with data received from remote peer
*           NULL -  if error
*
*  @description
*  Function gets memory block from interface received from remote peer. Function is blocking
*  operation and waits till next memory block from remote peer.
*
**/
//-------------------------------------------------------------------------------------------
unsigned long long WLS_WGet1(void* h, unsigned int *MsgSize, unsigned short *MsgTypeID, unsigned short *Flags);

//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
*
*  @param[in]   h    - handle of WLS interface
*  @param[in]   pMsg - virtual address of WLS memory block.
*
*  @return  physical address of WLS memory block
*           NULL - if error
*
*  @description
*  Function converts virtual address (VA) to physical address (PA)
*
**/
//-------------------------------------------------------------------------------------------
unsigned long long WLS_VA2PA(void* h, void* pMsg);

//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
*
*  @param[in]   h    - handle of WLS interface
*  @param[in]   pMsg - physical address of WLS memory block.
*
*  @return  virtual address of WLS memory block
*           NULL - if error
*
*  @description
*  Function converts physical address (PA) to virtual address (VA)
*
**/
//-------------------------------------------------------------------------------------------
void* WLS_PA2VA(void* h, unsigned long long pMsg);

//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
*
*  @param[in]   h    - handle of WLS interface
*  @param[in]   pMsg - physical address of WLS memory block.
*
*  @return  0 - if successful
*          -1 - if error
*
*  @description
*  Function is used by master to provide memory blocks to slave for next slave to master transfer
*  of data.
*
**/
//-------------------------------------------------------------------------------------------
int WLS_EnqueueBlock(void* h, unsigned long long pMsg);

//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
*
*  @param[in]   h    -- handle of second WLS interface within the same app
*  @param[in]   pMsg - physical address of WLS memory block.
*
*  @return  0 - if successful
*          -1 - if error
*
*  @description
*  Function is used by master to provide memory blocks to slave for next slave to master transfer
*  of data.
*
**/
//-------------------------------------------------------------------------------------------
int WLS_EnqueueBlock1(void* h, unsigned long long pMsg);


//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
*
*  @param[in]   h    - handle of WLS interface
*
*  @return  0   - pointer (physical address) of WLS memory block
*          NULL - if error
*
*  @description
*  Function is used by master and slave to get block from master to slave queue of available memory
*  blocks.
*
**/
//-------------------------------------------------------------------------------------------
unsigned long long WLS_DequeueBlock(void* h);

//-------------------------------------------------------------------------------------------
/** @ingroup wls_mod
*
*  @param[in]   h    - handle of WLS interface
*
*  @return  number of blocks in slave to master queue
*
*  @description
*  Function returns number of current available block provided by master for new transfer
*  of data from slave.
*
**/
//-------------------------------------------------------------------------------------------
int WLS_NumBlocks(void* h);

#ifdef __cplusplus
}
#endif
#endif //_WLS_LIB_H_
