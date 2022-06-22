###############################################################################
#
#   Copyright (c) 2021 Intel.
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

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$DIR_WIRELESS_WLS

MACHINE_TYPE=`uname -m`

if [ ${MACHINE_TYPE} == 'x86_64' ]; then

	ulimit -c unlimited
	echo 1 > /proc/sys/kernel/core_uses_pid

	sysctl -w kernel.sched_rt_runtime_us=-1
	sysctl -w kernel.shmmax=2147483648
	sysctl -w kernel.shmall=2147483648
	chkconfig --level 12345 irqbalance off
	echo 0 > /proc/sys/kernel/nmi_watchdog
	echo 1 > /sys/module/rcupdate/parameters/rcu_cpu_stall_suppress

fi

echo start ORAN 5G FAPI
if [ "$1" = "-g" ]; then
    shift
    if [ "$RTE_TARGET" == "x86_64-native-linuxapp-icx"]; then
        /opt/intel/oneapi/debugger/10.2.4/gdb/intel64/bin/gdb-oneapi --args ./oran_5g_fapi $@
    else
        /home/opt/intel/system_studio_2019/bin/gdb-ia --args ./oran_5g_fapi $@
    fi
else
    ./oran_5g_fapi $@
fi
