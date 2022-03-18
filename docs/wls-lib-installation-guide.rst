..    Copyright (c) 2019-2022 Intel
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
   
Wls Lib Installation Guide
==========================

The wls library uses DPDK as the basis for the shared memory operations and requires that DPDK 
be installed in the system since in the makefile it uses the RTE_SDK environment variable when
building the library. |br|
The current release was tested using DPDK version 20.11 but it doesn't preclude the 
use of newer releases. |br|
Also the library uses the Intel Compiler that is defined as part of the ODULOW documentation.

Contents
--------

- Overview
- Building and Installation
- Command Line Parameters
- Known Issues/Troubleshooting
- License


================================================================================

Overview
--------

This document describes the wls DPDK base library for ODULOW to ODUHIGH
communication as part of the |br|
ORAN Reference Architecture where an intermediate
shin layer can be present between these components.


================================================================================


Building and Installation
-------------------------

Retrieve the source files from the Linux Foundation Gerrit server:
    `<https://gerrit.o-ran-sc.org/r/gitweb?p=o-du%2Fphy.git;a=summary>`_

1. cd wls_lib
2. wls_lib$ ./build.sh xclean
3. wls_lib$ ./build.sh

The shared library is available at wls_lib/lib

This library is used by the ODUHIGH, shin layer implementing a 5G FAPI to IAPI translator and the 
ODULOW components.

Please define an environment variable DIR_WIRELESS_WLS with the path to the root folder of
the wls_lib as it is needed for the fapi_5g build process.

Unit Test building and validation
---------------------------------

In order to build the unit test do the following steps:

1. cd test
2. ./build.sh xclean
3. ./build.sh
4. Create an SSH session into the target an change directory to wls_lib/bin/phy
5. issue ./phy.sh
6. Create a second SSH session into the target and change directory to wls_lib/bin/fapi
7. issue ./fapi.sh
8. Create a third SSH session into the target and change directory to wls_lib/bin/mac
9. issue ./mac.sh

After the test run you should see that each module sent and receive 16 messages from
the display status messages.

================================================================================



Known Issues/Troubleshooting
----------------------------
No known issues.
For troubleshooting use unit test application.

================================================================================

License
-------

Please see License.txt at the root of the phy repository for license information details


