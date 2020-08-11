..    Copyright (c) 2019-2020 Intel
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
   
ORAN 5G FAPI TM Installation Guide
==================================

The 5G FAPI TM uses the wls library which uses DPDK as the basis for the shared memory operations 
and requires that DPDK 
be installed in the system since in the makefile it uses the RTE_SDK environment variable when
building the library. |br|
The current release was tested using DPDK version 19.11 but it doesn't preclude the 
use of newer releases. |br|
Also the 5G FAPI TM currently uses the Intel Compiler that is defined as part of the ODULOW documentation.

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

This document describes how to install and build the 5G FAPI TM for ODULOW to ODUHIGH
communication as part of the |br|
ORAN Reference Architecture.


================================================================================


Building and Installation
-------------------------

Retrieve the source files from the Linux Foundation Gerrit server:
    `<https://gerrit.o-ran-sc.org/r/gitweb?p=o-du%2Fphy.git;a=summary>`_

1. Make sure that the follwoing environment variables are defined
   DIR_WIRELESS_WLS for the wls_lib and RTE_SDK for the DPDK |br|
2. cd fapi_5g/build |br|
3. $ ./build.sh xclean  // Force full rebuild |br|
4. $ ./build.sh         // Build the 5G FAPI TM |br|

The executable is available at fapi_5g/bin and it is called oran_5g_fapi

Unit Test and validation
---------------------------------

The unit test for the ORAN 5G FAPI TM requires the testmac and L1 binaries that are described
in a later section and that for the Bronze Release consists of 15 basic tests in timer mode
where the DL, UL and FD paths are exercised for different channel types and numerology 0 and 1.

1.Open SSH session and cd l1\bin\nr5g\gnb\l1 |br|
2.Issue l1.sh |br|
3.Open a second SSH session and cd fapi_5g\bin |br|
4.Issue ./oran_5g_fapi.sh --cfg oran_5g_fapi.cfg |br|
5.Open a third SSH session and cd l1\bin\nr5g\gnb\testmac |br|
6.Issue ./l2.sh |br|
7.From the testmac command prompt (i.e. the l2 executable) issue::
run Direction Numerology Bandwidth TestCase
where Direction is 0 DL, 1 UL and 2 FD
Numerology 0 15 Khz, 1 30 Khz, 2 60 KHz, etc
Bandwidth is 5, 10 , 20, 100 
Testcase is defined from the set supported in this release
In general issue only the cases provided with this release that have the full set
of supporting files required. |br|
8.Observe in the SSH associated with the testmac the PASS/FAIL status. All of the reference cases
pass.


Testmac cases used for 5g FAPI TM
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following DL, UL and PRACH test cases are used for validation.

Downlink Tx Sub6 Test Cases [mu = 0 (15khz) and 5Mhz]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1.Test case 1001 1 PDSCH and 1 Control symbol

2.Test case 1002 1 PUCCH Format 2 channel

Uplink Rx Sub6 Test Cases [mu = 0 (15khz) and 5Mhz]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

3.Test case 1001 1 PUSCH

4.Test case 1002 1 PUCCH Format 2

Uplink Rx Sub6 Test Cases [mu = 0 (15khz) and 20Mhz]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

5.Test case 1002 1 PRACH

6.Test case 1003 1 PRACH


PDSCH {QAM256, mcs28, 272rbs, 12symbols, 4Layers, 16UE/TTI}, PUSCH {QAM64, mcs28, 248rbs, 14symbols, 2Layers, 16UE/TTI}, 189 PUCCH and PRACH
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

7.TEST_FD, 1300, 1, fd/mu1_100mhz/300/fd_testconfig_tst300.cfg

PDSCH {QAM64, mcs16, 272rbs, 12symbols, 4Layers, 16UE/TTI}, PUSCH {QAM16, mcs16, 248rbs, 14symbols, 2Layers, 16UE/TTI}, 189 PUCCH and PRACH
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

8.TEST_FD, 1301, 1, fd/mu1_100mhz/301/fd_testconfig_tst301.cfg

PDSCH {QAM16, mcs9, 272rbs, 12symbols, 4Layers, 16UE/TTI}, PUSCH {QPSK, mcs9, 248rbs, 14symbols, 2Layers, 16UE/TTI}, 189 PUCCH and PRACH
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

9.TEST_FD, 1302, 1, fd/mu1_100mhz/302/fd_testconfig_tst302.cfg

PDSCH {QAM256, mcs28, 190rbs, 12symbols, 4Layers, 16UE/TTI}, PUSCH {QAM64, mcs28, 190rbs, 14symbols, 2Layers, 16UE/TTI}, 189 PUCCH and PRACH
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

10.TEST_FD, 1303, 1, fd/mu1_100mhz/303/fd_testconfig_tst303.cfg

PDSCH {QAM64, mcs16, 190rbs, 12symbols, 4Layers, 16UE/TTI}, PUSCH {QAM16, mcs16, 190rbs, 14symbols, 2Layers, 16UE/TTI}, 189 PUCCH and PRACH
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

11.TEST_FD, 1304, 1, fd/mu1_100mhz/304/fd_testconfig_tst304.cfg

PDSCH {QAM16, mcs9, 190rbs, 12symbols, 4Layers, 16UE/TTI}, PUSCH {QPSK, mcs9, 190rbs, 14symbols, 2Layers, 16UE/TTI}, 189 PUCCH and PRACH
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

12.TEST_FD, 1305, 1, fd/mu1_100mhz/305/fd_testconfig_tst305.cfg

PDSCH {QAM256, mcs28, 96rbs, 12symbols, 4Layers, 16UE/TTI}, PUSCH {QAM64, mcs28, 96rbs, 14symbols, 2Layers, 16UE/TTI}, 94 PUCCH and PRACH
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

13.TEST_FD, 1306, 1, fd/mu1_100mhz/306/fd_testconfig_tst306.cfg

PDSCH {QAM64, mcs16, 96rbs, 12symbols, 4Layers, 16UE/TTI}, PUSCH {QAM16, mcs16, 96rbs, 14symbols, 2Layers, 16UE/TTI}, 94 PUCCH and PRACH
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

14.TEST_FD, 1307, 1, fd/mu1_100mhz/307/fd_testconfig_tst307.cfg

PDSCH {QAM16, mcs9, 96rbs, 12symbols, 4Layers, 16UE/TTI}, PUSCH {QPSK, mcs9, 96rbs, 14symbols, 2Layers, 16UE/TTI}, 94 PUCCH and PRACH
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

15.TEST_FD, 1308, 1, fd/mu1_100mhz/308/fd_testconfig_tst308.cfg