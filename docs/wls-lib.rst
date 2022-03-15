..    Copyright (c) 2012 Intel
..
..  Licensed under the Apache License, Version 2.0 (the "License");
..  you may not use this file except in compliance with the License.
..  You may obtain a copy of the License at
..
..      http://www.apache.org/licenses/LICENSE-2.0
..
..  Unless required by applicable law or agreed to in writing, software
..  distributed under the License is distributed on an "AS IS" BASIS,
..  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
..  See the License for the specific language governing permissions and
..  limitations under the License.

.. |br| raw:: html

   <br /> 
   
Wls Lib Overview
================

The Wls_lib is a Wireless Service library that supports shared memory and buffer management used by applications 
implementing a gNb or eNb. 
This library uses DPDK, libhugetlbfs and pthreads to provide memcpy less data exchange between an L2 application,
API Translator Module and a L1 application by sharing the same memory zone from the DPDK perspective.

Project Resources
-----------------

The source code is avalable from the Linux Foundation Gerrit server:
    `<https://gerrit.o-ran-sc.org/r/gitweb?p=o-du%2Fphy.git;a=summary>`_
    
The build (CI) jobs will be in the Linux Foundation Jenkins server:
    `<https://jenkins.o-ran-sc.org>`_

Issues are tracked in the Linux Foundation Jira server:
    `<https://jira.o-ran-sc.org/secure/Dashboard.jspa>`_

Project information is available in the Linux Foundation Wiki:
    `<https://wiki.o-ran-sc.org>`_
    

Library Functions
-----------------

* **WLS_Open() and WLS_Open_Dual()** that open a single or dual wls instance interface and registers the instance with the kernel space driver.
    
* **WLS_Close(), WLS_Close1()** closes the wls instance and deregisters it from the kernel space driver.

* **WLS_Ready(), WLS_Ready1()** checks state of remote peer of WLS interface and returns 1 if remote peer is available.
    
* **WLS_Alloc()** allocates a memory block for data exchange shared memory. This block uses hugepages.

* **WLS_Free()** frees memory block for data exchange shared memory.

* **WLS_Put(), WLS_Put1()** puts memory block (or group of blocks) allocated from WLS memory into the interface for transfer to remote peer.

* **WLS_Check(), WLS_Check1()** checks if there are memory blocks with data from remote peer and returns number of blocks available for "get" operation.

* **WLS_Get(), WLS_Get1()** gets memory block from interface received from remote peer. Function is a non-blocking operation and returns NULL if no blocks available.

* **WLS_Wait(), WLS_Wait1()** waits for new memory block from remote peer. This Function is a blocking call and returns number of blocks received.
    
* **WLS_WakeUp(), WLS_WakeUp1()** performs "wakeup" notification to remote peer to unblock "wait" operations pending.

* **WLS_Get(), WLS_Get1()** gets a memory block from the interface received from remote peer. This Function is blocking operation and waits till next memory block from remote peer.

* **WLS_VA2PA()** converts virtual address (VA) to physical address (PA).

* **WLS_PA2VA()** converts physical address (PA) to virtual address (VA).

* **WLS_EnqueueBlock(), WLS_EnqueueBlock1()** This function is used by a master or secondary master to provide memory blocks to a slave for next slave to master (sec master) transfer of data.

* **WLS_NumBlocks()** returns number of current available blocks provided by master for a new transfer of data from the slave.

The **_1()** functions are only needed when using the WLS_Open_Dual().

The source code and documentation will be updated in the next release to use inclusive engineering terms.