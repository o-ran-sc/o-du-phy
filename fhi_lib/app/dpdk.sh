#! /bin/bash

#/******************************************************************************
#*
#*   Copyright (c) 2020 Intel.
#*
#*   Licensed under the Apache License, Version 2.0 (the "License");
#*   you may not use this file except in compliance with the License.
#*   You may obtain a copy of the License at
#*
#*       http://www.apache.org/licenses/LICENSE-2.0
#*
#*   Unless required by applicable law or agreed to in writing, software
#*   distributed under the License is distributed on an "AS IS" BASIS,
#*   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#*   See the License for the specific language governing permissions and
#*   limitations under the License.
#*
#*******************************************************************************/

if [ -z "$RTE_SDK" ]
then
    echo "## ERROR: Please make sure environment variable RTE_SDK is set to valid DPDK path."
    echo "       To fix, please do: export RTE_SDK=path_to_dpdk_folder    before running this script"
    exit 1
fi

if [ -z "$RTE_TARGET" ]
then
    echo "## ERROR: Please make sure environment variable RTE_TARGET is set to valid DPDK path."
    exit 1
fi

#
# Unloads igb_uio.ko.
#
remove_igb_uio_module()
{
    echo "Unloading any existing DPDK UIO module"
    /sbin/lsmod | grep -s igb_uio > /dev/null
    if [ $? -eq 0 ] ; then
        sudo /sbin/rmmod igb_uio
    fi
}

#
# Loads new igb_uio.ko (and uio module if needed).
#
load_igb_uio_module()
{
    if [ ! -f $RTE_SDK/$RTE_TARGET/kmod/igb_uio.ko ];then
        echo "## ERROR: Target does not have the DPDK UIO Kernel Module."
        echo "       To fix, please try to rebuild target."
        return
    fi

    remove_igb_uio_module

    /sbin/lsmod | grep -s uio > /dev/null
    if [ $? -ne 0 ] ; then
        if [ -f /lib/modules/$(uname -r)/kernel/drivers/uio/uio.ko ] ; then
            echo "Loading uio module"
            sudo /sbin/modprobe uio
        fi
    fi

    # UIO may be compiled into kernel, so it may not be an error if it can't
    # be loaded.

    echo "Loading DPDK UIO module"
    sudo /sbin/insmod $RTE_SDK/$RTE_TARGET/kmod/igb_uio.ko
    if [ $? -ne 0 ] ; then
        echo "## ERROR: Could not load kmod/igb_uio.ko."
        quit
    fi
}

#
# Unloads VFIO modules.
#
remove_vfio_module()
{
    echo "Unloading any existing VFIO module"
    /sbin/lsmod | grep -s vfio > /dev/null
    if [ $? -eq 0 ] ; then
        sudo /sbin/rmmod vfio-pci
        sudo /sbin/rmmod vfio_iommu_type1
        sudo /sbin/rmmod vfio
    fi
}

#
# Loads new vfio-pci (and vfio module if needed).
#
load_vfio_module()
{
    remove_vfio_module


    echo "Loading VFIO module"
    /sbin/lsmod | grep -s vfio_pci > /dev/null
    if [ $? -ne 0 ] ; then
        sudo /sbin/modprobe -v vfio-pci
    fi

    # make sure regular users can read /dev/vfio
    echo "chmod /dev/vfio"
    sudo chmod a+x /dev/vfio
    if [ $? -ne 0 ] ; then
        echo "FAIL"
        quit
    fi
    echo "OK"

    # check if /dev/vfio/vfio exists - that way we
    # know we either loaded the module, or it was
    # compiled into the kernel
    if [ ! -e /dev/vfio/vfio ] ; then
        echo "## ERROR: VFIO not found!"
    fi
}


load_igb_uio_module
load_vfio_module

CPU_FEATURES_DETECT=`cat /proc/cpuinfo |grep hypervisor | wc -l`

if [ "$CPU_FEATURES_DETECT" -eq "0" ]
then
VM_DETECT='HOST'
echo ${VM_DETECT}
else
VM_DETECT='VM'
echo ${VM_DETECT}
fi

$RTE_SDK/usertools/dpdk-devbind.py --status
if [ ${VM_DETECT} == 'HOST' ]; then
    #HOST
    $RTE_SDK/usertools/dpdk-devbind.py --bind=vfio-pci 0000:19:02.0
    $RTE_SDK/usertools/dpdk-devbind.py --bind=vfio-pci 0000:19:02.1
    $RTE_SDK/usertools/dpdk-devbind.py --bind=vfio-pci 0000:19:02.2
    $RTE_SDK/usertools/dpdk-devbind.py --bind=vfio-pci 0000:19:0a.0
    $RTE_SDK/usertools/dpdk-devbind.py --bind=vfio-pci 0000:19:0a.1
    $RTE_SDK/usertools/dpdk-devbind.py --bind=vfio-pci 0000:19:0a.2

    $RTE_SDK/usertools/dpdk-devbind.py --bind=vfio-pci 0000:af:02.0
    $RTE_SDK/usertools/dpdk-devbind.py --bind=vfio-pci 0000:af:02.1
    $RTE_SDK/usertools/dpdk-devbind.py --bind=vfio-pci 0000:af:02.2
    $RTE_SDK/usertools/dpdk-devbind.py --bind=vfio-pci 0000:af:0a.0
    $RTE_SDK/usertools/dpdk-devbind.py --bind=vfio-pci 0000:af:0a.1
    $RTE_SDK/usertools/dpdk-devbind.py --bind=vfio-pci 0000:af:0a.2

else
    #VM
    $RTE_SDK/usertools/dpdk-devbind.py --bind=igb_uio 0000:00:04.0
    $RTE_SDK/usertools/dpdk-devbind.py --bind=igb_uio 0000:00:05.0
    $RTE_SDK/usertools/dpdk-devbind.py --bind=igb_uio 0000:00:06.0
    $RTE_SDK/usertools/dpdk-devbind.py --bind=igb_uio 0000:00:07.0
fi

$RTE_SDK/usertools/dpdk-devbind.py --status

