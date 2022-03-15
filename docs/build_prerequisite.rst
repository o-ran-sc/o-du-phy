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

Build Prerequisite
====================

.. contents::
    :depth: 3
    :local:
    
This section describes how to install and build the required components needed to build the FHI Library, WLS Library and the 5G FAPI TM modules.

Install ICC
------------
Intel® C++ Compiler v19.0.3 is used for the test application and system integration with L1, 
The Intel® C++ Compiler can be obtained using the following link https://software.intel.com/en-us/system-studio/choose-download with community |br|
license::

         COPY $icc_license_file $BUILD_DIR/license.lic
    
*Note: The version available at this link is always the latest ICC version, the verification for that version may not have been 
performed yet, so please provide feedback through O-DU Low project WIKI page if you face any issues.*


You can follow the installation guide from above website to download Intel System Studio and install. Intel® Math Kernel Library, Intel® Integrated Performance Primitives and Intel® C++ Compiler are mandatory components.
Here we are using the Linux* Host,Linux* Target and standalone installer as one example, below link might need update based on the website ::

         #wget https://registrationcenter-download.intel.com/akdlm/irc_nas/16789/system_studio_2020_u2_ultimate_edition_offline.tar.gz
         #cd /opt && mkdir intel && cp $BUILD_DIR/license.lic intel/license.lic
         #tar -zxvf $BUILD_DIR/system_studio_2020_u2_ultimate_edition_offline.tar.gz

Edit system_studio_2020_ultimate_edition_offline/silent.cfg to accept the EULA file as below example::
  
         ACCEPT_EULA=accept
         PSET_INSTALL_DIR=opt/intel
         ACTIVATION_LICENSE_FILE=/opt/intel/license.lic
         ACTIVATION_TYPE=license_file
    
Silent installation::

         #./install.sh -s silent.cfg

Set env for ICC::
 
         #source /opt/intel/system_studio_2020/bin/iccvars.sh intel64
         #export PATH=/opt/intel/system_studio_2020/bin/:$PATH


Download and Build DPDK
-----------------------
   - download DPDK::
     
         #wget http://static.dpdk.org/rel/dpdk-20.11.tar.x
         #tar -xf dpdk-20.11.tar.xz
         #export RTE_TARGET=x86_64-native-linuxapp-icc
         #export RTE_SDK=Intallation_DIR/dpdk-20.11

   - patch DPDK for O-RAN FHI lib, this patch is specific for O-RAN FHI to reduce the data transmission latency of Intel NIC. This may not be needed for some NICs, please refer to |br| O-RAN FHI Lib Introduction -> setup configuration -> A.2 prerequisites

   - SW FEC was enabled by default, to enable HW FEC with specific accelerator card, you need to get the associated driver and build steps from the accelerator card vendors.


   - build DPDK
      build DPDK::
        Set your ICC installation path to /usertools/dpdk-setup.sh, DEFAULT_PATH="your ICC installation path"/bin/iccvars.sh, then run it to build DPDK with below option. 
        #./usertools/dpdk-setup.sh
        select [39] x86_64-native-linuxapp-icc
        exit   [62] Exit Script
        please pay attention, the number 39 might change in your setup, you need choose accordingly to option 'x86_64-native-linuxapp-icc'   
  
    - set DPDK path
       DPDK path is needed during build and run lib/app::

        #export RTE_SDK=Installation_DIR/dpdk-20.11
        #export DESTDIR=Installation_DIR/dpdk-20.11


Install google test
-------------------
Download google test from https://github.com/google/googletest/releases 
   - Example build and installation commands::

        #tar -xvf googletest-release-1.7.0.tar.gz
        #mv googletest-release-1.7.0 gtest-1.7.0
        #export GTEST_DIR=YOUR_DIR/gtest-1.7.0
        #export GTEST_ROOT= $GTEST_DIR
        #cd ${GTEST_DIR}
        #g++ -isystem ${GTEST_DIR}/include -I${GTEST_DIR} -pthread -c ${GTEST_DIR}/src/gtest-all.cc
        #ar -rv libgtest.a gtest-all.o
        #cd ${GTEST_DIR}/build-aux
        #cmake ${GTEST_DIR}
        #make
        #cd ${GTEST_DIR}
        #ln -s build-aux/libgtest_main.a libgtest_main.a

- Set the google test Path
   this path should be always here when you build and run O-RAN FH lib unit test::

        #export DIR_ROOT_GTEST="your google test path"


Configure FEC card
--------------------
For the E Maintenance Release either a SW FEC, or an FPGA FEC (Vista Creek N3000) or an ASIC FEC (Mount Bryce ACC100) can be used.
The procedure to configure the HW based FECs is explained below.

Customize a setup environment shell script
------------------------------------------
Using as an example the provided in the folder phy\\setupenv.sh as the starting point
customize this script to provide the paths to the tools and libraries that
are used building and running the code.
You can add for example the following entries based on your particular installation and the
following illustration is just an example::
                                                                           
- export DIR_ROOT=/home/                                                           
- #set the L1 binary root DIR                                                      
- export DIR_ROOT_L1_BIN=$DIR_ROOT/FlexRAN                                         
- #set the phy root DIR                                                            
- export DIR_ROOT_PHY=$DIR_ROOT/phy                                                
- #set the DPDK root DIR                                                           
- #export DIR_ROOT_DPDK=/home/dpdk-19.11                                           
- #set the GTEST root DIR                                                          
- #export DIR_ROOT_GTEST=/home/gtest/gtest-1.7.0                                                                                                                   
- export DIR_WIRELESS_TEST_5G=$DIR_ROOT_L1_BIN/testcase                            
- export DIR_WIRELESS_SDK=$DIR_ROOT_L1_BIN/sdk/build-avx512-icc                    
- export DIR_WIRELESS_TABLE_5G=$DIR_ROOT_L1_BIN/l1/bin/nr5g/gnb/l1/table           
- #source /opt/intel/system_studio_2019/bin/iccvars.sh intel64 -platform linux     
- export XRAN_DIR=$DIR_ROOT_PHY/fhi_lib                                            
- export XRAN_LIB_SO=true                                                          
- export RTE_TARGET=x86_64-native-linuxapp-icc                                     
- #export RTE_SDK=$DIR_ROOT_DPDK                                                   
- #export DESTDIR=""                                                                                                                                              
- #export GTEST_ROOT=$DIR_ROOT_GTEST                                                                                                                             
- export ORAN_5G_FAPI=true                                                         
- export DIR_WIRELESS_WLS=$DIR_ROOT_PHY/wls_lib                                    
- export DEBUG_MODE=true                                                           
- export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$DIR_WIRELESS_WLS:$XRAN_DIR/lib/build    
- export DIR_WIRELESS=$DIR_ROOT_L1_BIN/l1                                          
- export DIR_WIRELESS_ORAN_5G_FAPI=$DIR_ROOT_PHY/fapi_5g                           
- export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$DIR_ROOT_L1_BIN/libs/cpa/bin        

Then issue::

- source ./setupenv.sh

This sets up the correct environment to build the code

Then build the wls_lib, FHI_Lib, 5G FAPI TM prior to running the code with the steps described in the Run L1 section
                                                                                 







