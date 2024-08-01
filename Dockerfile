################################################################################
#   Copyright (c) 2019 AT&T Intellectual Property.                             #
#                                                                              #
#   Licensed under the Apache License, Version 2.0 (the "License");            #
#   you may not use this file except in compliance with the License.           #
#   You may obtain a copy of the License at                                    #
#                                                                              #
#       http://www.apache.org/licenses/LICENSE-2.0                             #
#                                                                              #
#   Unless required by applicable law or agreed to in writing, software        #
#   distributed under the License is distributed on an "AS IS" BASIS,          #
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   #
#   See the License for the specific language governing permissions and        #
#   limitations under the License.                                             #
################################################################################
FROM ubuntu:22.04 AS builder

ENV https_proxy $https_proxy
ENV http_proxy $http_proxy
ENV no_proxy $no_proxy

WORKDIR /opt/o-du/phy
ENV BUILD_DIR /opt/o-du/phy
ENV XRAN_DIR $BUILD_DIR/fhi_lib

RUN apt update
RUN apt upgrade -y

###Install required libs
RUN apt install -y build-essential libhugetlbfs-bin libhugetlbfs-dev libtool-bin cmake python3-pip tcpdump net-tools bc cmake wget  pciutils vim tcpdump ntpdate driverctl lshw numactl linux-tools-common libnuma-dev xterm pkg-config  rt-tests git libtool libcrypto++-dev linux-tools-generic vim python3 pciutils


###googletest is required for XRAN unittests build and run
RUN wget https://github.com/google/googletest/archive/release-1.7.0.tar.gz && \
    cd /opt && tar -zxvf $BUILD_DIR/release-1.7.0.tar.gz
ENV GTEST_ROOT /opt/googletest-release-1.7.0
ENV GTEST_DIR /opt/googletest-release-1.7.0
RUN cd $GTEST_DIR && \
    g++ -isystem ${GTEST_DIR}/include -I${GTEST_DIR} -pthread -c ${GTEST_DIR}/src/gtest-all.cc && \
    ar -rv libgtest.a gtest-all.o && cd build-aux/ && cmake $GTEST_DIR && make && cd .. && ln -s build-aux/libgtest_main.a libgtest_main.a


RUN wget https://registrationcenter-download.intel.com/akdlm/irc_nas/19079/l_BaseKit_p_2023.0.0.25537_offline.sh
RUN chmod +x l_BaseKit_p_2023.0.0.25537_offline.sh 
RUN ./l_BaseKit_p_2023.0.0.25537_offline.sh -a -s --eula accept --install-dir /opt/backups/intel/oneapi

RUN ln -sf /opt/intel/oneapi/compiler/2023.0.0/ /opt/intel/oneapi/compiler/latest
RUN source /opt/intel/oneapi/setvars.sh --force
ENV PATH /opt/intel/oneapi/compiler/2023.0.0/linux/bin-llvm:$PATH

####Download and build DPDK
ENV RTE_TARGET x86_64-native-linuxapp-icc
ENV RTE_SDK /opt/o-du/dpdk-22.11
RUN wget http://static.dpdk.org/rel/dpdk-22.11.1.tar.xz && \
    tar -xf dpdk-22.11.tar.xz && \
    mv dpdk-22.11 $RTE_SDK && \
    cd $RTE_SDK && \
    make config T=$RTE_TARGET O=$RTE_TARGET && \
    cd $RTE_SDK/$RTE_TARGET && \
    sed -i "s/CONFIG_RTE_EAL_IGB_UIO=y/CONFIG_RTE_EAL_IGB_UIO=n/"  .config && \
    sed -i "s/CONFIG_RTE_KNI_KMOD=y/CONFIG_RTE_KNI_KMOD=n/"  .config && \
    make 

####Install octave. Issue when downloading octave. And the octave will take 500MB space.
RUN apt install octave -y
####Install expect
RUN apt install -y expect

###Copy XRAN source code into docker image
COPY fhi_lib $BUILD_DIR/fhi_lib
COPY misc $BUILD_DIR/misc

ENV WIRELESS_SDK_TARGET_ISA avx512

####Download and Build FlexRAN FEC SDK, not needed for run L1
#RUN cd $BUILD_DIR/ && wget https://software.intel.com/sites/default/files/managed/23/b8/FlexRAN-FEC-SDK-19-04.tar.gz && tar -zxvf #FlexRAN-FEC-SDK-19-04.tar.gz && misc/extract-flexran-fec-sdk.ex && cd FlexRAN-FEC-SDK-19-04/sdk && source #/opt/intel/system_studio_2019/bin/iccvars.sh intel64 && ./create-makefiles-linux.sh && cd build-avx512-icc && #make && make install

####Build XRAN lib, unittests, sample app
RUN cd $BUILD_DIR/fhi_lib/ && export XRAN_LIB_SO=1 && ./build.sh xclean && ./build.sh && cd app && octave gen_test.m

####Build wls lib
COPY wls_lib $BUILD_DIR/wls_lib
RUN cd $BUILD_DIR/wls_lib && ./build.sh xclean && ./build.sh
ENV DIR_WIRELESS_WLS $BUILD_DIR/wls_lib

####Clone FlexRAN repo from Github
RUN cd /opt/o-du && git clone https://github.com/intel/FlexRAN.git
ENV DIR_WIRELESS=/opt/o-du/FlexRAN/l1/

####Build 5g fapi
COPY fapi_5g $BUILD_DIR/fapi_5g
ENV DIR_WIRELESS_ORAN_5G_FAPI $BUILD_DIR/fapi_5g
ENV DEBUG_MODE true
RUN cd $BUILD_DIR/fapi_5g/build && ./build.sh xclean && ./build.sh

####Copy other folders/files
COPY setupenv.sh $BUILD_DIR

####Unset network proxy
RUN unset http_proxy https_prox ftp_proxy no_proxy

####Copy built binaries/libraries into docker images
FROM ubuntu:22.04
ENV TARGET_DIR /opt/o-du/
COPY --from=builder $TARGET_DIR/phy $TARGET_DIR/phy
COPY --from=builder $TARGET_DIR/FlexRAN $TARGET_DIR/FlexRAN

RUN apt update
RUN apt upgrade -y

###Install required libs
RUN apt install -y build-essential libhugetlbfs-bin libhugetlbfs-dev libtool-bin cmake python3-pip tcpdump net-tools bc cmake wget  pciutils vim tcpdump ntpdate driverctl lshw numactl linux-tools-common libnuma-dev xterm pkg-config  rt-tests git libtool libcrypto++-dev linux-tools-generic vim python3 pciutils

###googletest is required for XRAN unittests build and run
RUN wget https://github.com/google/googletest/archive/release-1.7.0.tar.gz && \
    cd /opt && tar -zxvf $BUILD_DIR/release-1.7.0.tar.gz
ENV GTEST_ROOT /opt/googletest-release-1.7.0
ENV GTEST_DIR /opt/googletest-release-1.7.0
RUN cd $GTEST_DIR && \
    g++ -isystem ${GTEST_DIR}/include -I${GTEST_DIR} -pthread -c ${GTEST_DIR}/src/gtest-all.cc && \
    ar -rv libgtest.a gtest-all.o && cd build-aux/ && cmake $GTEST_DIR && make && cd .. && ln -s build-aux/libgtest_main.a libgtest_main.a

RUN apt install -y expect

WORKDIR $TARGET_DIR

LABEL description="ORAN O-DU PHY Applications"

