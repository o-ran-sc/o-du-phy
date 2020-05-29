#######################################################################
#
# <COPYRIGHT_TAG>
#
#######################################################################
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

echo start WLS PHY Process
if [ "$1" = "-g" ]; then
    shift
    taskset -c 16 gdb-ia --args ./fapi_app $@
else
    taskset -c 16 ./fapi_app $@
fi
