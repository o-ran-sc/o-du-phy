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
FROM centos:centos7.7.1908 AS builder

ENV https_proxy $https_proxy
ENV http_proxy $http_proxy
ENV no_proxy $no_proxy

ARG icc_license_file

WORKDIR /opt/o-du/phy
ENV BUILD_DIR /opt/o-du/phy
ENV XRAN_DIR $BUILD_DIR/fhi_lib

###Install required libs
RUN yum install -y libhugetlbfs-utils libhugetlbfs-devel libhugetlbfs numactl-devel ethtool gcc make module-init-tools kmod patch xz iproute pciutils python vim cmake unzip nano mc iputils-ping libaio libaio-devel net-tools  wget zip

###Install some libs to compile DPDK
RUN yum groupinstall -y "Development Tools"
RUN yum install -y  ncurses-devel hmaccalc zlib-devel binutils-devel elfutils-libelf-devel bc libstdc++-4.8.5-28.el7_5.1.x86_64 gcc-c++ libstdc++-devel-4.8.5-28.el7_5.1.x86_64 autoconf-2.69-11.el7.noarch

###googletest is required for XRAN unittests build and run
RUN wget https://github.com/google/googletest/archive/release-1.7.0.tar.gz && \
    cd /opt && tar -zxvf $BUILD_DIR/release-1.7.0.tar.gz
ENV GTEST_ROOT /opt/googletest-release-1.7.0
ENV GTEST_DIR /opt/googletest-release-1.7.0
RUN cd $GTEST_DIR && \
    g++ -isystem ${GTEST_DIR}/include -I${GTEST_DIR} -pthread -c ${GTEST_DIR}/src/gtest-all.cc && \
    ar -rv libgtest.a gtest-all.o && cd build-aux/ && cmake $GTEST_DIR && make && cd .. && ln -s build-aux/libgtest_main.a libgtest_main.a

###Before install ISS, please go to Intel site to get a free license: https://software.intel.com/en-us/system-studio/choose-download
COPY $icc_license_file $BUILD_DIR/license.lic

####Download Intel System Studio from Intel website and install ICC
RUN wget https://registrationcenter-download.intel.com/akdlm/IRC_NAS/16789/system_studio_2020_u2_ultimate_edition_offline.tar.gz && \
    cd /opt && mkdir intel && cp $BUILD_DIR/license.lic intel/license.lic && \
    tar -zxvf $BUILD_DIR/system_studio_2020_u2_ultimate_edition_offline.tar.gz && \
    cd system_studio_2020_u2_ultimate_edition_offline/ && \
    sed -i "s/ACCEPT_EULA.*/ACCEPT_EULA=accept/" silent.cfg && \
    sed -i "s/PSET_INSTALL_DIR.*/PSET_INSTALL_DIR=\/opt\/intel/" silent.cfg && \
    sed -i "s/.*ACTIVATION_LICENSE_FILE.*/ACTIVATION_LICENSE_FILE=\/opt\/intel\/license.lic/" silent.cfg && \
    sed -i "s/ACTIVATION_TYPE.*/ACTIVATION_TYPE=license_file/" silent.cfg && \
    ./install.sh -s silent.cfg

###Set env for ICC
RUN source /opt/intel/system_studio_2020/bin/iccvars.sh intel64
ENV PATH /opt/intel/system_studio_2020/bin/:$PATH

####Download and build DPDK
ENV RTE_TARGET x86_64-native-linuxapp-icc
ENV RTE_SDK /opt/o-du/dpdk-19.11
RUN wget http://fast.dpdk.org/rel/dpdk-19.11.tar.xz && \
    tar -xf dpdk-19.11.tar.xz && \
    mv dpdk-19.11 $RTE_SDK && \
    cd $RTE_SDK && \
    make config T=$RTE_TARGET O=$RTE_TARGET && \
    cd $RTE_SDK/$RTE_TARGET && \
    sed -i "s/CONFIG_RTE_EAL_IGB_UIO=y/CONFIG_RTE_EAL_IGB_UIO=n/"  .config && \
    sed -i "s/CONFIG_RTE_KNI_KMOD=y/CONFIG_RTE_KNI_KMOD=n/"  .config && \
    make 

####Install octave. Issue when downloading octave. And the octave will take 500MB space.
RUN yum install https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm -y && \
    yum install octave -y
####Install expect
RUN yum install -y expect

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
FROM centos:centos7.7.1908
ENV TARGET_DIR /opt/o-du/
COPY --from=builder $TARGET_DIR/phy $TARGET_DIR/phy
COPY --from=builder $TARGET_DIR/FlexRAN $TARGET_DIR/FlexRAN

###Install required libs
RUN yum install -y libhugetlbfs-utils libhugetlbfs-devel libhugetlbfs numactl-devel ethtool gcc make module-init-tools kmod patch xz iproute pciutils python vim cmake unzip nano mc iputils-ping libaio libaio-devel net-tools  wget zip

###Install some libs to compile DPDK
RUN yum groupinstall -y "Development Tools"
RUN yum install -y  ncurses-devel hmaccalc zlib-devel binutils-devel elfutils-libelf-devel bc libstdc++-4.8.5-28.el7_5.1.x86_64 gcc-c++ libstdc++-devel-4.8.5-28.el7_5.1.x86_64 autoconf-2.69-11.el7.noarch

###googletest is required for XRAN unittests build and run
RUN wget https://github.com/google/googletest/archive/release-1.7.0.tar.gz && \
    cd /opt && tar -zxvf $BUILD_DIR/release-1.7.0.tar.gz
ENV GTEST_ROOT /opt/googletest-release-1.7.0
ENV GTEST_DIR /opt/googletest-release-1.7.0
RUN cd $GTEST_DIR && \
    g++ -isystem ${GTEST_DIR}/include -I${GTEST_DIR} -pthread -c ${GTEST_DIR}/src/gtest-all.cc && \
    ar -rv libgtest.a gtest-all.o && cd build-aux/ && cmake $GTEST_DIR && make && cd .. && ln -s build-aux/libgtest_main.a libgtest_main.a

RUN yum install -y expect

WORKDIR $TARGET_DIR

LABEL description="ORAN O-DU PHY Applications"

