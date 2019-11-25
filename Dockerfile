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
FROM nexus3.o-ran-sc.org:10004/bldr-ubuntu16-c-go:2-u16.04-nng as ubuntu

WORKDIR /opt/o-du/phy
COPY . .

ENV XRAN_DIR /opt/o-du/phy/fhi_lib
ENV BUILD_GCC 1

RUN apt-get update &&\
    apt-get install -y wget libnuma-dev linux-headers-generic libgtest-dev
# because we run in VM which may be linuxkit kernel.  Faking for "regular" kernel
RUN cp -r /lib/modules/$(ls -1 /lib/modules | head -1) /lib/modules/$(uname -r)

RUN wget https://github.com/google/googletest/archive/v1.8.x.zip && \
    unzip v1.8.x.zip
ENV GTEST_ROOT /opt/o-du/phy/googletest-1.8.x

ENV RTE_TARGET x86_64-native-linuxapp-gcc
ENV RTE_SDK /opt/o-du/dpdk-18.08
RUN wget http://fast.dpdk.org/rel/dpdk-18.08.1.tar.xz && \
    tar -xf dpdk-18.08.1.tar.xz && \
    mv dpdk-stable-18.08.1 $RTE_SDK && \
    cd $RTE_SDK && \
    make install T="$RTE_TARGET"

RUN cd fhi_lib/lib && \
    bash ./build_ci.sh
