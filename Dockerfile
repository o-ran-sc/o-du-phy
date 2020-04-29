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
RUN yum update -y && \
    yum install -y libhugetlbfs-utils libhugetlbfs-devel libhugetlbfs numactl-devel ethtool gcc make module-init-tools kmod patch xz iproute pciutils python vim cmake unzip nano mc iputils-ping libaio libaio-devel net-tools  wget zip

###Install some libs to compile DPDK
RUN yum groupinstall -y "Development Tools"
RUN yum install -y  ncurses-devel hmaccalc zlib-devel binutils-devel elfutils-libelf-devel bc libstdc++-4.8.5-28.el7_5.1.x86_64 gcc-c++ libstdc++-devel-4.8.5-28.el7_5.1.x86_64 autoconf-2.69-11.el7.noarch

###googletest is required for XRAN unittests build and run
RUN wget https://github.com/google/googletest/archive/v1.8.x.zip && \
    cd /opt && unzip $BUILD_DIR/v1.8.x.zip
ENV GTEST_ROOT /opt/googletest-1.8.x/googletest
ENV GTEST_DIR /opt/googletest-1.8.x/googletest
RUN cd $GTEST_DIR && \
    g++ -isystem ${GTEST_DIR}/include -I${GTEST_DIR} -pthread -c ${GTEST_DIR}/src/gtest-all.cc && \
    ar -rv libgtest.a gtest-all.o


###Before install ISS, please go to Intel site to get a free license: https://software.intel.com/en-us/system-studio/choose-download
COPY $icc_license_file $BUILD_DIR/license.lic

####Download Intel System Studio from Intel website and install ICC
RUN wget https://registrationcenter-download.intel.com/akdlm/irc_nas/16242/system_studio_2020_ultimate_edition_offline.tar.gz && \
    cd /opt && mkdir intel && cp $BUILD_DIR/license.lic intel/license.lic && \
    tar -zxvf $BUILD_DIR/system_studio_2020_ultimate_edition_offline.tar.gz && \
    cd system_studio_2020_ultimate_edition_offline/ && \
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
ENV RTE_SDK /opt/o-du/dpdk-18.08
RUN wget http://fast.dpdk.org/rel/dpdk-18.08.tar.xz && \
    tar -xf dpdk-18.08.tar.xz && \
    mv dpdk-18.08 $RTE_SDK && \
    cd $RTE_SDK && \
    make config T=$RTE_TARGET O=$RTE_TARGET && \
    cd $RTE_SDK/$RTE_TARGET && \
    sed -i "s/CONFIG_RTE_EAL_IGB_UIO=y/CONFIG_RTE_EAL_IGB_UIO=n/"  .config && \
    sed -i "s/CONFIG_RTE_KNI_KMOD=y/CONFIG_RTE_KNI_KMOD=n/"  .config && \
    make 

####Install octave. Issue when downloading octave. And the octave will take 500MB space.
RUN yum install https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm -y && \
    yum install octave -y

###Copy XRAN source code into docker image
COPY fhi_lib $BUILD_DIR/fhi_lib
COPY misc $BUILD_DIR/misc

####Build XRAN lib, unittests, sample app
RUN cd $BUILD_DIR/fhi_lib/ && ./build.sh xclean && ./build.sh && cd app && octave gen_test.m

####Unset network proxy
RUN unset http_proxy https_prox ftp_proxy no_proxy

FROM centos:centos7.7.1908
ENV TARGET_DIR /opt/o-du/phy
COPY --from=builder /opt/o-du/phy/fhi_lib $TARGET_DIR/fhi_lib
COPY --from=builder /opt/o-du/phy/misc $TARGET_DIR/misc
COPY --from=builder  /usr/lib64/libnuma* /usr/lib64/
ENV XRAN_DIR $TARGET_DIR/fhi_lib
WORKDIR $TARGET_DIR/fhi_lib

LABEL description="ORAN Fronthaul Sample Application"

