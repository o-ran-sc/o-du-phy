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
#
#  File: wls_start.sh
#        Test script to load wls module.
#

export RTE_WLS=`pwd`

#
# Unloads wls.ko.
#
remove_wls_module()
{
    echo "Unloading WLS module"
    /sbin/lsmod | grep -s wls > /dev/null
    if [ $? -eq 0 ] ; then
        sudo /sbin/rmmod wls
    fi
}

#
# Loads new wls.ko
#
load_wls_module()
{
    if [ ! -f $RTE_WLS/wls.ko ];then
        echo "## ERROR: Folder does not have the WLS Kernel Module."
        echo "       To fix, please try to rebuild WLS"
        return
    fi

    remove_wls_module

    /sbin/lsmod | grep -s wls > /dev/null
    if [ $? -eq	1 ] ; then
        if [ -f /lib/modules/$(uname -r)/updates/drivers/intel/wls/wls.ko ] ; then
            echo "Loading WLS module"
            sudo /sbin/modprobe wls wlsMaxClients=4
	else
	    echo "No module. WLS is not istalled? do 'make install'"
        fi
    fi
}

load_wls_module



