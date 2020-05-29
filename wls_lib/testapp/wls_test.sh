#!/bin/bash
###############################################################################
#
#   Copyright (c) 2019 Intel.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
###############################################################################

COREMASK=2
SECONDARY=1
FPREFIX="wls"
DPDK_WLS=0

while getopts ":mpa:w:" opt; do
  case ${opt} in
    m )
      SECONDARY=0
      ;;
    a )
      COREMASK=$((1 << $OPTARG))
      ;;
    : )
      echo "Invalid option: $OPTARG requires a core number"
      exit 1
      ;;
    w )
      #replace / with _ for dpdk file prefix
      FPREFIX=${OPTARG////_}
      ;;
    : )
      echo "Invalid option: $OPTARG requires dev wls path"
      exit 1
      ;;
    p )
      DPDK_WLS=1
      ;;
  esac
done

wlsTestBinary="wls_test"
if [ $DPDK_WLS -eq 1 ]; then
    if [ $SECONDARY -eq 0 ]; then
        wlsTestBinary="build/wls_test -c $COREMASK -n 4 "
        wlsTestBinary+="--file-prefix=$FPREFIX --socket-mem=3072 --"
    else
        wlsTestBinary="build/wls_test -c $COREMASK -n 4 "
        wlsTestBinary+="--proc-type=secondary --file-prefix=$FPREFIX --"
    fi
fi

ulimit -c unlimited

export RTE_WLS=$PWD/..

MACHINE_TYPE=`uname -m`

if [ ${MACHINE_TYPE} == 'x86_64' ]; then
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$RTE_WLS

    grep Huge /proc/meminfo

    ulimit -c unlimited
    echo 1 > /proc/sys/kernel/core_uses_pid
    sysctl -w kernel.sched_rt_runtime_us=-1
    for c in $(ls -d /sys/devices/system/cpu/cpu[0-9]*); do echo performance >$c/cpufreq/scaling_governor; done
    sysctl -w kernel.shmmax=2147483648
    sysctl -w kernel.shmall=2147483648
fi

wlsCmd="./${wlsTestBinary} $*"
echo "Running... ${wlsCmd}"

eval $wlsCmd

exit 0
