..    Copyright (c) 2019 Intel
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
   
Test Cases
============

.. contents::
    :depth: 3
    :local:

This section describes the downlink, uplink and full duplex bit exact test cases that are present as part of the Bronze |br|
release. All the test config files, IQ samples and reference Inputs are placed under the FlexRAN/testcase folder. These test config files are used for testmac.

There are 3 kinds of tests: dl, ul, and fd. The following test cases are part of the Bronze Release and reside in the github repo mentioned earlier in this document.

Downlink Tx Sub6 Test Cases [mu = 0 (15khz) and 5Mhz]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Test case 1001 1 PDSCH and 1 Control symbol**

**Test case 1002 1 PUCCH Format 2 channel**

Uplink Rx Sub6 Test Cases [mu = 0 (15khz) and 5Mhz]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Test case 1001 1 PUSCH**

**Test case 1002 1 PUCCH Format 2**

Uplink Rx Sub6 Test Cases [mu = 0 (15khz) and 20Mhz]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Test case 1002 1 PRACH**

**Test case 1003 1 PRACH**

PDSCH {QAM256, mcs28, 272rbs, 12symbols, 4Layers, 16UE/TTI}, PUSCH {QAM64, mcs28, 248rbs, 14symbols, 2Layers, 16UE/TTI}, 189 PUCCH and PRACH
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**TEST_FD, 1300, 1, fd/mu1_100mhz/300/fd_testconfig_tst300.cfg**

PDSCH {QAM64, mcs16, 272rbs, 12symbols, 4Layers, 16UE/TTI}, PUSCH {QAM16, mcs16, 248rbs, 14symbols, 2Layers, 16UE/TTI}, 189 PUCCH and PRACH
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**TEST_FD, 1301, 1, fd/mu1_100mhz/301/fd_testconfig_tst301.cfg**

PDSCH {QAM16, mcs9, 272rbs, 12symbols, 4Layers, 16UE/TTI}, PUSCH {QPSK, mcs9, 248rbs, 14symbols, 2Layers, 16UE/TTI}, 189 PUCCH and PRACH
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**TEST_FD, 1302, 1, fd/mu1_100mhz/302/fd_testconfig_tst302.cfg**

PDSCH {QAM256, mcs28, 190rbs, 12symbols, 4Layers, 16UE/TTI}, PUSCH {QAM64, mcs28, 190rbs, 14symbols, 2Layers, 16UE/TTI}, 189 PUCCH and PRACH
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**TEST_FD, 1303, 1, fd/mu1_100mhz/303/fd_testconfig_tst303.cfg**

PDSCH {QAM64, mcs16, 190rbs, 12symbols, 4Layers, 16UE/TTI}, PUSCH {QAM16, mcs16, 190rbs, 14symbols, 2Layers, 16UE/TTI}, 189 PUCCH and PRACH
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**TEST_FD, 1304, 1, fd/mu1_100mhz/304/fd_testconfig_tst304.cfg**

PDSCH {QAM16, mcs9, 190rbs, 12symbols, 4Layers, 16UE/TTI}, PUSCH {QPSK, mcs9, 190rbs, 14symbols, 2Layers, 16UE/TTI}, 189 PUCCH and PRACH
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**TEST_FD, 1305, 1, fd/mu1_100mhz/305/fd_testconfig_tst305.cfg**

PDSCH {QAM256, mcs28, 96rbs, 12symbols, 4Layers, 16UE/TTI}, PUSCH {QAM64, mcs28, 96rbs, 14symbols, 2Layers, 16UE/TTI}, 94 PUCCH and PRACH
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**TEST_FD, 1306, 1, fd/mu1_100mhz/306/fd_testconfig_tst306.cfg**

PDSCH {QAM64, mcs16, 96rbs, 12symbols, 4Layers, 16UE/TTI}, PUSCH {QAM16, mcs16, 96rbs, 14symbols, 2Layers, 16UE/TTI}, 94 PUCCH and PRACH
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**TEST_FD, 1307, 1, fd/mu1_100mhz/307/fd_testconfig_tst307.cfg**

PDSCH {QAM16, mcs9, 96rbs, 12symbols, 4Layers, 16UE/TTI}, PUSCH {QPSK, mcs9, 96rbs, 14symbols, 2Layers, 16UE/TTI}, 94 PUCCH and PRACH
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**TEST_FD, 1308, 1, fd/mu1_100mhz/308/fd_testconfig_tst308.cfg**






