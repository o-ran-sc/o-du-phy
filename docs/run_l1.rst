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

Run L1 and Testmac
===================

.. contents::
    :depth: 3
    :local:

Before you run L1, please make sure all the FH, WLS, and FAPI TM lib was built according to above relative chapters for each lib, or you can refer below quick build command to create these libs.

Build FH
------------
     under folder phy/fhi_lib::

     #./build.sh

Build WLS
-------------
     under folder phy/wls_lib::
     #./build.sh

Build FAPI TM
--------------
     under folder phy/fapi_5g/build::
     #./build.sh

For the current O-RAN release, the L1 only has a binary image as well as the testmac which is an L2 test application, details of the L1 and testmac application are in https://github.com/intel/FlexRAN

Download L1 and testmac
------------------------

Download L1 and testmac through https://github.com/intel/FlexRAN

CheckList Before Running the code
---------------------------------
Before running the L1 and Testmac code make sure that you have built the wls_lib, FHI_lib and 5G_FAPI_TM using the instructions provided earlier in this 
document and in the order specified in this documentation.

Run L1 with testmac
--------------------
Three console windows are needed, one for L1 app, one for FAPI TM, one for testmac. They need to run in the following order L1-> FAPI TM-> testmac.
In each console window, the environment needs to be set using a shell script under folder phy/  example::

     #source ./setupenv.sh

* Run L1 under folder FlexRAN/l1/bin/nr5g/gnb/l1 in timer mode using::

     #l1.sh -e

**Note** that the markups dpdkBasebandFecMode and dpdkBasebandDevice needs to be adjusted in the relevant phycfg.xml under folder
 FlexRAN/l1/bin/nr5g/gnb/l1 before starting L1. |br|
 dpdkBasebandFecMode = 0 for LDPC Encoder/Decoder in software. |br|
 dpdkBasebandFecMode = 1 for LDPC Encoder/Decoder in FPGA. |br|

* Run FAPI TM under folder phy/fapi_5g/bin::

     #./oran_5g_fapi.sh --cfg=oran_5g_fapi.cfg

* Run testmac under folder FlexRAN/l1/bin/nr5g/gnb/testmac::

     #./l2.sh

Once the application comes up, you will see a *<TESTMAC>* prompt. The same Unit tests can be run using the command:

- **run   testtype   numerology   bandwidth   testnum** where

- **testtype** is 0 (DL), 1 (UL) or 2 (FD)

- **numerology** [0 -> 4], 0=15khz, 1=30khz, 2=60khz, 3=120khz, 4=240khz

- **bandwidth** 5, 10, 15, 20, 30, 40, 50, 60, 70, 80, 90, 100, 200, 400 (in Mhz)

- **testnum** is the Bit Exact TestNum. [1001 -> above]If this is left blank, then all tests under type testtype are run

testnum is always a 4 digit number. First digit represents the number of carriers to run.
For example, to run Test Case 5 for Uplink Rx mu=3, 100Mhz for 1 carrier, the command would be:
run 1 3 100 1005
All the pre-defined test cases for the current O-RAN Release are defined in the Test Cases section in https://github.com/intel/FlexRAN and also in the Test 
Cases section of this document.
If the user wants to run more slots (than specified in test config file) or change the mode or change the TTI interval of the test, then the command phystart can be used as follows:

- **phystart   mode   interval   num_tti**

- **mode** is 4 (ORAN compatible Radio) or 1 (Timer)

- **interval** is the TTI duration scaled as per Numerology (only used in timer mode).

    - So if Numerology is 3 and this parameter is set to 1, then the interval will be programmed to 0.125 ms.
    
    - If this is set to 10, then interval is programmed to 1.25ms
    
- **num_tti** is the total number of TTIs to run the test.

    - If 0, then the test config file defines how many slots to run.
    
    - If a non zero number, then test is run for these many slots.
    
    - If the num_tti is more than the number of slots in config file, then the configuration is repeated till end of test.
    
    - So if num_tti=200 and num_slot from config file is 10, then the 10 slot configs are repeated 20 times in a cyclic fashion.
    
- The default mode set at start of testmac is (phystart 1 10 0). So it is timer mode at 10ms TTI intervals running for duration specified in each test config file

- Once user manually types the phystart command on the l2 console, then all subsequent tests will use this phystart config till user changes it or testmac is restarted.

- If user wants to run a set of tests which are programmed in a cfg file (for example tests_customer.cfg):
     ./l2.sh –testfile=tests_customer.cfg

   example::

      #./l2.sh --testfile=oran_bronze_rel_fec_sw.cfg

- This will run all the tests that are listed in the config file. Please see the tests_customer.cfg present in the release for example of how to program the tests 





