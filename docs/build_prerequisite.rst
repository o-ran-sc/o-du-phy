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

Build Prerequisite
====================

.. contents::
    :depth: 3
    :local:
    
This section describes how to install and build the required components needed to build the FHI Library, WLS Library and the 5G FAPI TM modules.

Install ICC
------------
Intel® C++ Compiler v19.0.3 is used for the test application and system integration with L1, 
The Intel® C++ Compiler can be obtained using the follwoing link https://software.intel.com/en-us/system-studio/choose-download with community |br|
license::

         COPY $icc_license_file $BUILD_DIR/license.lic
    
*Note: The version available at this link is always the latest ICC version, the verification for that version may not have been 
performed yet, so please provide feedback through O-DU Low project WIKI page if you face any issues.*


Download Intel System Studio from Intel website and install ICC ::

         #wget https://registrationcenter-download.intel.com/akdlm/irc_nas/16242/system_studio_2020_ultimate_edition_offline.tar.gz 
         #cd /opt && mkdir intel && cp $BUILD_DIR/license.lic intel/license.lic
         #tar -zxvf $BUILD_DIR/system_studio_2020_ultimate_edition_offline.tar.gz

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


Build DPDK
-----------
   - download FEC SDK lib::
   
         #wget https://software.intel.com/sites/default/files/managed/23/b8/FlexRAN-FEC-SDK-19-04.tar.gz
         #tar -xzvf FlexRAN-FEC-SDK-19-04.tar.gz

   - build FEC SDK lib::
        
         #./FlexRAN-FEC-SDK-19-04.sh
         #cd FlexRAN-FEC-SDK-19-04/sdk
         #./create-makefiles-linux.sh
         #cd build-avx512-icc
         #make;make install

   *note: you need to accept the Intel OBL commercial use license during the installation*

   - download DPDK::
     
         #wget http://fast.dpdk.org/rel/dpdk-18.08.tar.xz
         #tar -xf dpdk-18.08.tar.xz
         #export RTE_TARGET=x86_64-native-linuxapp-icc
         #export RTE_SDK=Intallation_DIR/dpdk-18.08


   - patch DPDK for O-RAN FHI lib, this patch is specific for O-RAN FHI to reduce the data transmission latency of Intel NIC. This may not be needed for some NICs, please refer to O-RAN FHI Lib Introduction -> setup configuration -> A.2 prerequisites


   - change DPDK config to enable SW FEC support

     check the configuration as below in dpdk-18.08/config/common_base::
     
         CONFIG_RTE_LIBRTE_BBDEV=y
         CONFIG_RTE_LIBRTE_BBDEV_DEBUG=n
         CONFIG_RTE_BBDEV_MAX_DEVS=128
         CONFIG_RTE_BBDEV_OFFLOAD_COST=y
         CONFIG_RTE_BBDEV_SDK_AVX2=y
         CONFIG_RTE_BBDEV_SDK_AVX512=y

   - enable FEC accelerator card, DPDK BBDev framework support any kind of accelerator card to plug in the associated driver while using the same interface. Right now there is no open source driver available yet, so you need to get the associated vendor's driver to enable the capability to use an accelerator card. 

   - enable IGB UIO for FEC card and NIC configure::
   
         CONFIG_RTE_EAL_IGB_UIO=y
         CONFIG_RTE_KNI_KMOD=y

   - build DPDK
      setup Env for BBDev SW::

        #export FLEXRAN_SDK=$Intallation_DIR/FlexRAN-FEC-SDK-19-04/sdk/build-avx512-icc/install

      build DPDK::

        #./usertools/dpdk-setup.sh
        select [16] x86_64-native-linuxapp-icc
        select [19] Insert VFIO module
        exit   [35] Exit Script
   - set DPDK path
     this path should be always there during you build and run lib/app::

        #export RTE_SDK="your DPDK path"


Install google test
-------------------
Download google test from https://github.com/google/googletest/releases 
   - Example build and installation commands::

        #tar -xvf googletest-release-1.7.0.tar.gz
        #mv googletest-release-1.7.0 gtest-1.7.0
        #export GTEST_DIR=YOUR_DIR/gtest-1.7.0
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
For the Bronze Release only as SW FEC is available so this step is not needed, for later releases the required information will be added to the document.






