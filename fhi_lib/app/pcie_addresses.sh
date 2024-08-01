#! /bin/bash

#******************************************************************************
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
#******************************************************************************/

get_pcie_addresses_du()
{
    #$1 is hostname
    echo "O-DU hostname is $1"
    if [ $1 == "icelake-scs1-1" ]
    then
        pcie_addr1="0000:51:01.0,0000:51:09.0"
        pcie_addr2="0000:51:11.0,0000:51:19.0"
        pcie_addr3="0000:54:01.0,0000:54:11.0"
        pcie_addr4="0000:54:01.1,0000:54:11.1"

    elif [ $1 == "spr-npg-quanta-b4" ]
    then
        pcie_addr1="0000:43:01.0,0000:43:09.0"
        pcie_addr2="0000:43:11.0,0000:43:19.0"
        pcie_addr3="0000:45:01.0,0000:45:11.0"
        pcie_addr4="0000:45:01.1,0000:45:11.1"

    elif [ $1 == "skx-5gnr-sd6" ]
    then
        pcie_addr1="0000:af:01.0,0000:af:09.0"
        pcie_addr2="0000:af:11.0,0000:af:19.0"
        pcie_addr3="0000:af:11.1,0000:af:19.1"
        pcie_addr4="0000:af:01.1,0000:af:09.1"

    elif [ $1 == "npg-cp-srv02" ] 
    then
        pcie_addr1="0000:31:01.0,0000:31:09.0"
        pcie_addr2="0000:31:11.0,0000:31:19.0"
        pcie_addr3="0000:31:11.1,0000:31:19.1"
        pcie_addr4="0000:31:01.1,0000:31:09.1"

    elif [ $1 == "npg-cp-srv01" ]
    then
        pcie_addr1="0000:4b:01.0,0000:4b:09.0"
        pcie_addr2="0000:4b:11.0,0000:4b:19.0"
        pcie_addr3="0000:4b:11.1,0000:4b:19.1"
        pcie_addr4="0000:4b:01.1,0000:4b:09.1"

    elif [ $1 == "npg-cp-srv15" ]
    then
        pcie_addr1="0000:4b:01.0,0000:4b:09.0"
        pcie_addr2="0000:4b:11.0,0000:4b:19.0"
        pcie_addr3="0000:4b:11.1,0000:4b:19.1"
        pcie_addr4="0000:4b:01.1,0000:4b:09.1"

    else
        echo "## ERROR!! Unknown O-DU hostname $1"
        echo "## Kindly add the VF PCIe address for this host in ${XRAN_DIR}/app/pcie_addresses.sh ##"
        exit 1
    fi
}

get_pcie_addresses_ru()
{
    #$1 is hostname
    echo "O-RU hostname is $1"
    if [ $1 == "icx-nclg-scs1-14" ] #RU connected to icelake-scs1-1
    then
        #TBD
        pcie_addr1="0000:8a:01.0,0000:8a:09.0"
        pcie_addr2="0000:8a:11.0,0000:8a:19.0"
        pcie_addr3="0000:51:01.0,0000:51:09.0"
        pcie_addr4="0000:51:01.1,0000:51:09.1"

    elif [ $1 == "icx-npg-bds1-coyote28" ]  #RU spr-npg-quanta-b4
    then
        pcie_addr1="0000:8a:01.0,0000:8a:09.0"
        pcie_addr2="0000:8a:11.0,0000:8a:19.0"
        pcie_addr3="0000:51:11.0,0000:51:19.0"
        pcie_addr4="0000:51:11.1,0000:51:19.1"

    elif [ $1 == "skx-5gnr-sd6" ] #RU connected to skx-5gnr-sd6
    then
        pcie_addr1="0000:18:01.0,0000:18:09.0"
        pcie_addr2="0000:18:11.0,0000:18:19.0"
        pcie_addr3="0000:18:11.1,0000:18:19.1"
        pcie_addr4="0000:18:01.1,0000:18:09.1"
    
    elif [ $1 == "npg-cp-srv02" ] || [ $1 == "npg-cp-srv01" ] #RU connected to npg-cp-srv01 and npg-cp-srv-2 respectively (Loop back mode)
    then
        pcie_addr1="0000:b1:01.0,0000:b1:09.0"
        pcie_addr2="0000:b1:11.0,0000:b1:19.0"
        pcie_addr3="0000:b1:11.1,0000:b1:19.1"
        pcie_addr4="0000:b1:01.1,0000:b1:09.1"

elif [ $1 == "npg-cp-srv15" ] #RU connected to npg-cp-srv15(Loop back mode)
    then
        pcie_addr1="0000:ca:01.0,0000:ca:09.0"
        pcie_addr2="0000:ca:11.0,0000:ca:19.0"
        pcie_addr3="0000:ca:11.1,0000:ca:19.1"
        pcie_addr4="0000:ca:01.1,0000:ca:09.1"

    else
        echo "## ERROR!! Unknown O-RU hostname $1"
        echo "## Kindly add the VF PCIe address for this host in ${XRAN_DIR}/app/pcie_addresses.sh ##"
        exit 1
    fi
}
