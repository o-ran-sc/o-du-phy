..    Copyright (c) 2019 Intel
..
..  Licensed under the Apache License, Version 2.0 (the "License");
..  you may not use this file except in compliance with the License.
..  You may obtain a copy of the License at
..
..      http://www.apache.org/licenses/LICENSE-2.0
..
..  Unless required by applicable law or agreed to in writing, software
..  distributed under the License is distributed on an "AS IS" BASIS,
..  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
..  See the License for the specific language governing permissions and
..  limitations under the License.



Installation Guide
==================

.. contents::
   :depth: 3
   :local:

Abstract
--------

This document describes how to install the Front Haul Interface Library, it's dependencies and required system resources.


Version history

+--------------------+--------------------+--------------------+--------------------+
| **Date**           | **Ver.**           | **Author**         | **Comment**        |
|                    |                    |                    |                    |
+--------------------+--------------------+--------------------+--------------------+
| 2019-11-01         | 1.1                | 	Intel	       | Rel A Seed Cont    |
|                    |                    |                    |                    |
+--------------------+--------------------+--------------------+--------------------+



Introduction
------------

This document describes the Prerequirements and Installation Information for the Front Haul Interface Library.
The audience of this document is assumed to have good knowledge in RAN network and Linux system.

Preface
-------


Before starting the installation of the Front Haul Interface Library, please ensure that the following pre-requisites are met

System configuration
~~~~~~~~~~~~~~~~~~~~

VFIO requires:
 *  linux:
                IOMMU=ON
 *  BIOS:
                Intel(R) Virtualization Technology Enabled
                Intel(R) VT for Directed I/O - Enabled
                ACS Control - Enabled
                Coherency Support - Disabled
 *  Compiler

    icc -v
    icc version 19.0.3.206 (gcc version 4.8.5 compatibility)

    Link to ICC (community free version):
    https://software.intel.com/en-us/system-studio/choose-download#technical

  * DPDK 18.08

Compile DPDK with
[root@5gnr-sc12-xran dpdk]# ./usertools/dpdk-setup.sh    // Where the root@5gnr-sc12-xran dpdk corresponds to the location in the server for the dpdk installation folder

select [16] x86_64-native-linuxapp-icc
select [19] Insert VFIO module
exit   [35] Exit Script

 * Find PCIe device of Fortville port

lspci|grep Eth
19:00.0 Ethernet controller: Intel Corporation 82599ES 10-Gigabit SFI/SFP+ Network Connection (rev 01)
19:00.1 Ethernet controller: Intel Corporation 82599ES 10-Gigabit SFI/SFP+ Network Connection (rev 01)
41:00.0 Ethernet controller: Intel Corporation Ethernet Connection X722 for 10GBASE-T (rev 04)
41:00.1 Ethernet controller: Intel Corporation Ethernet Connection X722 for 10GBASE-T (rev 04)
d8:00.0 << Ethernet controller: Intel Corporation Ethernet Controller XL710 for 40GbE QSFP+ (rev 02) <<<< this one
d8:00.1 Ethernet controller: Intel Corporation Ethernet Controller XL710 for 40GbE QSFP+ (rev 02)

 * Corresponding Eth device via

ifconfig -a

find port Eth with correct PCIe Bus address as per list above

ethtool -i enp218s0f0
driver: i40e
version: 2.4.10 << driver
firmware-version: 6.80 0x80003cfd 1.2007.0
expansion-rom-version:
bus-info: 0000:da:00.0 << this one
supports-statistics: yes
supports-test: yes
supports-eeprom-access: yes
supports-register-dump: yes
supports-priv-flags: yes

* install correct 2.4.10 i40e version if different (https://downloadcenter.intel.com/download/28306/Intel-Network-Adapter-Driver-for-PCIe-40-Gigabit-Ethernet-Network-Connections-Under-Linux-)

make sure firmare version is at least this version or higher

firmware-version: 6.01

* make sure that linux boot arguments are correct

cat /proc/cmdline
BOOT_IMAGE=/vmlinuz-3.10.0-rt56 root=/dev/mapper/centos-root ro crashkernel=auto rd.lvm.lv=centos/root rd.lvm.lv=centos/swap intel_iommu=on iommu=pt usbcore.autosuspend=-1 selinux=0 enforcing=0 nmi_watchdog=0 softlockup_panic=0 audit=0 intel_pstate=disable cgroup_disable=memory mce=off idle=poll hugepagesz=1G hugepages=20 hugepagesz=2M hugepages=0 default_hugepagesz=1G isolcpus=1-39 rcu_nocbs=1-39 kthread_cpus=0 irqaffinity=0 nohz_full=1-39
* enable SRIOV VF port for XRAN

echo 2 > /sys/class/net/enp216s0f0/device/sriov_numvfs

see https://doc.dpdk.org/guides/nics/intel_vf.html

* Check that Virtual Function was created

lspci |grep Eth
19:00.0 Ethernet controller: Intel Corporation 82599ES 10-Gigabit SFI/SFP+ Network Connection (rev 01)
19:00.1 Ethernet controller: Intel Corporation 82599ES 10-Gigabit SFI/SFP+ Network Connection (rev 01)
41:00.0 Ethernet controller: Intel Corporation Ethernet Connection X722 for 10GBASE-T (rev 04)
41:00.1 Ethernet controller: Intel Corporation Ethernet Connection X722 for 10GBASE-T (rev 04)
d8:00.0 Ethernet controller: Intel Corporation Ethernet Controller XL710 for 40GbE QSFP+ (rev 02)
d8:00.1 Ethernet controller: Intel Corporation Ethernet Controller XL710 for 40GbE QSFP+ (rev 02)
d8:02.0 Ethernet controller: Intel Corporation XL710/X710 Virtual Function (rev 02) <<<< this is XRAN port (u-plane)
d8:02.1 Ethernet controller: Intel Corporation XL710/X710 Virtual Function (rev 02) <<<< this is XRAN port (c-plane)

* Configure VFs
- set mac to 00:11:22:33:44:66
- set Vlan tag to 2 (U-plane) for VF0
- set Vlan tag to 1 (C-plane) for VF1

[root@5gnr-sc12-xran app]# ip link set enp216s0f0 vf 0 mac 00:11:22:33:44:66 vlan 2
[root@5gnr-sc12-xran app]# ip link set enp216s0f0 vf 1 mac 00:11:22:33:44:66 vlan 1
[root@5gnr-sc12-xran app]# ip link show
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN mode DEFAULT qlen 1
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
2: enp65s0f0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP mode DEFAULT qlen 1000
    link/ether a4:bf:01:3e:6b:79 brd ff:ff:ff:ff:ff:ff
3: eno2: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP mode DEFAULT qlen 1000
    link/ether a4:bf:01:3e:6b:7a brd ff:ff:ff:ff:ff:ff
4: enp25s0f0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP mode DEFAULT qlen 1000
    link/ether 90:e2:ba:d3:b2:ec brd ff:ff:ff:ff:ff:ff
5: enp129s0f0: <NO-CARRIER,BROADCAST,MULTICAST,UP> mtu 1500 qdisc mq state DOWN mode DEFAULT qlen 1000
    link/ether 3c:fd:fe:a8:e0:70 brd ff:ff:ff:ff:ff:ff
6: enp129s0f1: <NO-CARRIER,BROADCAST,MULTICAST,UP> mtu 1500 qdisc mq state DOWN mode DEFAULT qlen 1000
    link/ether 3c:fd:fe:a8:e0:71 brd ff:ff:ff:ff:ff:ff
7: enp216s0f0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP mode DEFAULT qlen 1000
    link/ether 3c:fd:fe:9e:93:68 brd ff:ff:ff:ff:ff:ff
    vf 0 MAC 00:11:22:33:44:66, vlan 2, spoof checking on, link-state auto, trust off
    vf 1 MAC 00:11:22:33:44:66, vlan 1, spoof checking on, link-state auto, trust off
8: enp25s0f1: <NO-CARRIER,BROADCAST,MULTICAST,UP> mtu 1500 qdisc mq state DOWN mode DEFAULT qlen 1000
    link/ether 90:e2:ba:d3:b2:ed brd ff:ff:ff:ff:ff:ff
9: enp216s0f1: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP mode DEFAULT qlen 1000
    link/ether 3c:fd:fe:9e:93:69 brd ff:ff:ff:ff:ff:ff
12: enp216s2: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP mode DEFAULT qlen 1000
    link/ether 96:fa:4d:04:4d:87 brd ff:ff:ff:ff:ff:ff
13: enp216s2f1: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP mode DEFAULT qlen 1000
    link/ether a6:67:49:bb:bd:5e brd ff:ff:ff:ff:ff:ff


Hardware Requirements
---------------------

The following minimum hardware requirements must be met for installation of the FHI Library:
*	Wolfpass server according to recommended BOM for FlexRAN such as Intel® Xeon® Skylake Gold 6148 FC-LGA3647 2.4GHz 27.5MB 150W 20 cores (2 sockets) 
*	BIOS settings: 
    	Intel(R) Virtualization Technology Enabled
    	Intel(R) VT for Directed I/O - Enabled
        ACS Control - Enabled
    	Coherency Support - Disabled
*   Front Haul networking cards:
        Intel® Ethernet Converged Network Adapter XL710-QDA2
        Intel® Ethernet Converged Network Adapter XXV710-DA2
        Intel® FPGA Programmable Acceleration Card (Intel® FPGA PAC) N3000
*	Back (Mid) Haul networking card can be either 
    	Intel® Ethernet Connection X722 for 10GBASE-T  
        Intel® 82599ES 10-Gigabit SFI/SFP+ Network Connection
*   Other networking cards capable of HW timestamping for PTP synchronization.
Note:	Both Back (mid) Haul and Front Haul NIC require support for PTP HW timestamping.

The recommended configuration for NICs is: 
    ethtool -i enp216s0f0
    driver: i40e
    version: 2.8.43
    firmware-version: 6.80 0x80003cfb 1.2007.0
    expansion-rom-version:
    bus-info: 0000:d8:00.0
    supports-statistics: yes
    supports-test: yes
    supports-eeprom-access: yes
    supports-register-dump: yes
    supports-priv-flags: yes
    [root@5gnr-sc12-xran testmac]# ethtool -T enp216s0f0
    Time stamping parameters for enp216s0f0:
    Capabilities:
        hardware-transmit     (SOF_TIMESTAMPING_TX_HARDWARE)
        software-transmit     (SOF_TIMESTAMPING_TX_SOFTWARE)
        hardware-receive      (SOF_TIMESTAMPING_RX_HARDWARE)
        software-receive      (SOF_TIMESTAMPING_RX_SOFTWARE)
        software-system-clock (SOF_TIMESTAMPING_SOFTWARE)
        hardware-raw-clock    (SOF_TIMESTAMPING_RAW_HARDWARE)
    PTP Hardware Clock: 2
    Hardware Transmit Timestamp Modes:
        off                   (HWTSTAMP_TX_OFF)
        on                    (HWTSTAMP_TX_ON)
    Hardware Receive Filter Modes:
        none                  (HWTSTAMP_FILTER_NONE)
        ptpv1-l4-sync         (HWTSTAMP_FILTER_PTP_V1_L4_SYNC)
        ptpv1-l4-delay-req    (HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ)
        ptpv2-l4-event        (HWTSTAMP_FILTER_PTP_V2_L4_EVENT)
        ptpv2-l4-sync         (HWTSTAMP_FILTER_PTP_V2_L4_SYNC)
        ptpv2-l4-delay-req    (HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ)
        ptpv2-l2-event        (HWTSTAMP_FILTER_PTP_V2_L2_EVENT)
        ptpv2-l2-sync         (HWTSTAMP_FILTER_PTP_V2_L2_SYNC)
        ptpv2-l2-delay-req    (HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ)
        ptpv2-event           (HWTSTAMP_FILTER_PTP_V2_EVENT)
        ptpv2-sync            (HWTSTAMP_FILTER_PTP_V2_SYNC)
        ptpv2-delay-req       (HWTSTAMP_FILTER_PTP_V2_DELAY_REQ)
PTP Grand Master is required to be available in the network to provide synchronization of both O-DU and RU to GPS time.

Software Installation and Deployment
------------------------------------

This section describes the installation of the Front Haul Interface Library on the reference hardware and the validation.

* start matlab and run gen_test.m with correct Numerology, Bandwidth and number of slots
  copy ant_*.bin  to /xran/app/usecase/mu{X}_{Y}MHz
	where X is numerology: 0,1,3
	      Y is 5,10,20,100 MHz bandwidth

* compile xran sample application (Please make sure that the export match your install directories for SDK, ORAN_FH_lib (i.e. XRAN_DIR), google test
    export RTE_SDK=/opt/dpdk-18.08
    export RTE_TARGET=x86_64-native-linuxapp-icc
    export XRAN_DIR= /home/npg_wireless-flexran_xran/
    export export GTEST_ROOT=/opt/gtest/gtest-1.7.0 

* ./build.sh
    Number of commandline arguments: 0
    Building xRAN Library
  LIBXRANSO=0
  CC ../lib/ethernet/ethdi.o
  CC ../lib/ethernet/ethernet.o
  CC ../lib/src/xran_up_api.o
  CC ../lib/src/xran_sync_api.o
  CC ../lib/src/xran_timer.o
  CC ../lib/src/xran_cp_api.o
  CC ../lib/src/xran_transport.o
  CC ../lib/src/xran_common.o
  CC ../lib/src/xran_ul_tables.o
  CC ../lib/src/xran_frame_struct.o
  CC ../lib/src/xran_compression.o
  CC ../lib/src/xran_app_frag.o
  CC ../lib/src/xran_main.o
  AR libxran.a
  INSTALL-LIB libxran.a
  Building xRAN Test Application
  CC ../app/src/common.o
  CC ../app/src/sample-app.o
remark #11074: Inlining inhibited by limit max-size
remark #11076: To get full report use -qopt-report=4 -qopt-report-phase ipo
  CC ../app/src/config.o
  LD sample-app
  INSTALL-APP sample-app
  INSTALL-MAP sample-app.map


* update Eth port used for XRAN


cat ./run_o_du.sh
#! /bin/bash

ulimit -c unlimited
echo 1 > /proc/sys/kernel/core_uses_pid

grep Huge /proc/meminfo
huge_folder="/mnt/huge_bbu"
[ -d "$huge_folder" ] || mkdir -p $huge_folder
if ! mount | grep $huge_folder; then
 mount none $huge_folder -t hugetlbfs -o rw,mode=0777
fi

#40G
./build/sample-app ./usecase/mu3_100mhz/config_file_o_du.dat  0000:da:02.0 0000:da:02.1
                                                              ^^^^^ ports have to match VF function from step 1.11 (0000:da:02.0 - U-plane  0000:da:02.1 C-plane)

umount $huge_folder
rmdir $huge_folder


cat ./dpdk.sh
...
$RTE_SDK/usertools/dpdk-devbind.py --status
if [ ${VM_DETECT} == 'HOST' ]; then
    #HOST

    $RTE_SDK/usertools/dpdk-devbind.py --bind=vfio-pci 0000:da:02.0 <<< port has to match VF function from step 1.11
    $RTE_SDK/usertools/dpdk-devbind.py --bind=vfio-pci 0000:da:02.1 <<< port has to match VF function from step 1.11

	1.
Run

* Run dpdk.sh to assign port to PMD

[root@5gnr-sc12-xran app]# ./dpdk.sh

Network devices using DPDK-compatible driver
============================================
0000:da:02.0 'XL710/X710 Virtual Function 154c' drv=vfio-pci unused=i40evf,igb_uio
0000:da:02.1 'XL710/X710 Virtual Function 154c' drv=vfio-pci unused=i40evf,igb_uio


* Run XRAN sample app
setup RU mac address in config_file_o_du.dat for corespondig usecase

e.g.
./build/sample-app ./usecase/mu3_100mhz/config_file_o_du.dat  0000:da:02.0 0000:da:02.1

ruMac=00:11:22:33:44:55  #RU VF for RU

execute O-DU sample app

[root@sc12-xran-ru-1 app]# ./run_o_du.sh
HugePages_Total:      20
HugePages_Free:       11
HugePages_Rsvd:        0
HugePages_Surp:        0
Hugepagesize:    1048576 kB
Machine is synchronized using PTP!
mu_number: 3
nDLAbsFrePointA: 27968160
nULAbsFrePointA: 27968160
nDLBandwidth: 100
nULBandwidth: 100
nULFftSize: 1024
nULFftSize: 1024
nFrameDuplexType: 1
nTddPeriod: 4
sSlotConfig0: 0 0 0 0 0 0 0 0 0 0 0 0 0 0
sSlotConfig1: 0 0 0 0 0 0 0 0 0 0 0 0 0 0
sSlotConfig2: 0 0 0 0 0 0 0 0 0 0 0 0 0 0
sSlotConfig3: 0 2 2 1 1 1 1 1 1 1 1 1 1 1
mtu 9600
lls-CU MAC address: 00:11:22:33:44:66
RU MAC address: 00:11:22:33:44:55
numSlots: 40
antC0: ./usecase/mu3_100mhz/ant_0.bin
antC1: ./usecase/mu3_100mhz/ant_1.bin
antC2: ./usecase/mu3_100mhz/ant_2.bin
antC3: ./usecase/mu3_100mhz/ant_3.bin
antC4: ./usecase/mu3_100mhz/ant_4.bin
antC5: ./usecase/mu3_100mhz/ant_5.bin
antC6: ./usecase/mu3_100mhz/ant_6.bin
antC7: ./usecase/mu3_100mhz/ant_7.bin
antC8: ./usecase/mu3_100mhz/ant_8.bin
antC9: ./usecase/mu3_100mhz/ant_9.bin
antC10: ./usecase/mu3_100mhz/ant_10.bin
antC11: ./usecase/mu3_100mhz/ant_11.bin
antC12: ./usecase/mu3_100mhz/ant_12.bin
antC13: ./usecase/mu3_100mhz/ant_13.bin
antC14: ./usecase/mu3_100mhz/ant_14.bin
antC15: ./usecase/mu3_100mhz/ant_15.bin
Prach enable: 0
Prach config index: 81
debugStop: 1
CPenable: 1
cp_vlan_tag: 1
up_vlan_tag: 2
Tadv_cp_dl: 25
T2a_min_cp_dl: 50
T2a_max_cp_dl: 140
T2a_min_cp_ul: 50
T2a_max_cp_ul: 140
T2a_min_up: 25
T2a_max_up: 140
Ta3_min: 20
Ta3_max: 32
T1a_min_cp_dl: 70
T1a_max_cp_dl: 100
T1a_min_cp_ul: 60
T1a_max_cp_ul: 70
T1a_min_up: 35
T1a_max_up: 50
Ta4_min: 0
Ta4_max: 45
115 lines of config file has been read.
numCCPorts 1 num_eAxc4
set O-DU
IQ files size is 40 slots
app_xran_get_num_rbs: nNumerology[3] nBandwidth[100] nAbsFrePointA[27968160] numRBs[66]
app_xran_get_num_rbs: nNumerology[3] nBandwidth[100] nAbsFrePointA[27968160] numRBs[66]
Loading file ./usecase/mu3_100mhz/ant_0.bin to  DL IFFT IN IQ Samples in binary format: Reading IQ samples from file: File Size: 1774080 [Buffer Size: 1774080]
from addr (0x7f62ad088010) size (1774080) bytes num (1774080)
Loading file ./usecase/mu3_100mhz/ant_1.bin to  DL IFFT IN IQ Samples in binary format: Reading IQ samples from file: File Size: 1774080 [Buffer Size: 1774080]
from addr (0x7f62aced6010) size (1774080) bytes num (1774080)
Loading file ./usecase/mu3_100mhz/ant_2.bin to  DL IFFT IN IQ Samples in binary format: Reading IQ samples from file: File Size: 1774080 [Buffer Size: 1774080]
from addr (0x7f62acd24010) size (1774080) bytes num (1774080)
Loading file ./usecase/mu3_100mhz/ant_3.bin to  DL IFFT IN IQ Samples in binary format: Reading IQ samples from file: File Size: 1774080 [Buffer Size: 1774080]
from addr (0x7f62acb72010) size (1774080) bytes num (1774080)
Storing DL IFFT IN IQ Samples in human readable format to file ./logs/o-du-play_ant0.txt: from addr (0x7f62ad088010) size (1774080) IQ num (443520)
Storing DL IFFT IN IQ Samples in binary format to file ./logs/o-du-play_ant0.bin: from addr (0x7f62ad088010) size (887040) bytes num (887040)
Storing DL IFFT IN IQ Samples in human readable format to file ./logs/o-du-play_ant1.txt: from addr (0x7f62aced6010) size (1774080) IQ num (443520)
Storing DL IFFT IN IQ Samples in binary format to file ./logs/o-du-play_ant1.bin: from addr (0x7f62aced6010) size (887040) bytes num (887040)
Storing DL IFFT IN IQ Samples in human readable format to file ./logs/o-du-play_ant2.txt: from addr (0x7f62acd24010) size (1774080) IQ num (443520)
Storing DL IFFT IN IQ Samples in binary format to file ./logs/o-du-play_ant2.bin: from addr (0x7f62acd24010) size (887040) bytes num (887040)
Storing DL IFFT IN IQ Samples in human readable format to file ./logs/o-du-play_ant3.txt: from addr (0x7f62acb72010) size (1774080) IQ num (443520)
Storing DL IFFT IN IQ Samples in binary format to file ./logs/o-du-play_ant3.bin: from addr (0x7f62acb72010) size (887040) bytes num (887040)
TX: Convert S16 I and S16 Q to network byte order for XRAN Ant: [0]
TX: Convert S16 I and S16 Q to network byte order for XRAN Ant: [1]
TX: Convert S16 I and S16 Q to network byte order for XRAN Ant: [2]
TX: Convert S16 I and S16 Q to network byte order for XRAN Ant: [3]
System clock (rdtsc) resolution 1596250371 [Hz]
Ticks per us 1596
 xran_init: MTU 9600
xran_ethdi_init_dpdk_io: Calling rte_eal_init:wls -c ffffffff -m5120 --proc-type=auto --file-prefix wls -w 0000:00:00.0
EAL: Detected 40 lcore(s)
EAL: Detected 2 NUMA nodes
EAL: Auto-detected process type: PRIMARY
EAL: Multi-process socket /var/run/dpdk/wls/mp_socket
EAL: No free hugepages reported in hugepages-2048kB
EAL: Probing VFIO support...
EAL: VFIO support initialized
EAL: PCI device 0000:da:02.0 on NUMA socket 1
EAL:   probe driver: 8086:154c net_i40e_vf
EAL:   using IOMMU type 1 (Type 1)
initializing port 0 for TX, drv=net_i40e_vf
Port 0 MAC: 00 11 22 33 44 66

Checking link status ... done
Port 0 Link Up - speed 40000 Mbps - full-duplex
EAL: PCI device 0000:da:02.1 on NUMA socket 1
EAL:   probe driver: 8086:154c net_i40e_vf
initializing port 1 for TX, drv=net_i40e_vf
Port 1 MAC: 00 11 22 33 44 66

Checking link status ... done
Port 1 Link Up - speed 40000 Mbps - full-duplex
Set debug stop 1
FFT Order 10
app_xran_get_num_rbs: nNumerology[3] nBandwidth[100] nAbsFrePointA[27968160] numRBs[66]
app_xran_get_num_rbs: nNumerology[3] nBandwidth[100] nAbsFrePointA[27968160] numRBs[66]
app_xran_cal_nrarfcn: nCenterFreq[28015680] nDeltaFglobal[60] nFoffs[24250080] nNoffs[2016667] nNRARFCN[2079427]
DL center freq 28015680 DL NR-ARFCN  2079427
app_xran_cal_nrarfcn: nCenterFreq[28015680] nDeltaFglobal[60] nFoffs[24250080] nNoffs[2016667] nNRARFCN[2079427]
UL center freq 28015680 UL NR-ARFCN  2079427
XRAN front haul xran_mm_init
xran_sector_get_instances [0]: CC 0 handle 0xd013380
Handle: 0x5a07cb8 Instance: 0xd013380
init_xran [0]: CC 0 handle 0xd013380
Sucess xran_mm_init
nSectorNum 1
nSectorIndex[0] = 0
[ handle 0xd013380 0 0 ] [nPoolIndex 0] nNumberOfBuffers 4480 nBufferSize 3328
CC:[ handle 0xd013380 ru 0 cc_idx 0 ] [nPoolIndex 0] mb pool 0x24a7ad440
nSectorIndex[0] = 0
[ handle 0xd013380 0 0 ] [nPoolIndex 1] nNumberOfBuffers 4480 nBufferSize 2216
CC:[ handle 0xd013380 ru 0 cc_idx 0 ] [nPoolIndex 1] mb pool 0x24956d100
[ handle 0xd013380 0 0 ] [nPoolIndex 2] nNumberOfBuffers 4480 nBufferSize 3328
CC:[ handle 0xd013380 ru 0 oolIndex 2] mb pool 0x248818dc0
[ handle 0xd013380 0 0 ] [nPoolIndex 3] nNumberOfBuffers 4480 nBufferSize 2216
CC:[ handle 0xd013380 ru 0 cc_idx 0 ] [nPoolIndex 3] mb pool 0x2475d8a80
[ handle 0xd013380 0 0 ] [nPoolIndex 4] nNumberOfBuffers 4480 nBufferSize 8192
CC:[ handle 0xd013380 ru 0 cc_idx 0 ] [nPoolIndex 4] mb pool 0x246884740
@@@ NB cell 0 DL NR-ARFCN  0,DL phase comp flag 0 UL NR-ARFCN  0,UL phase comp flag 0
init_xran_iq_content
xRAN open PRACH config: Numerology 3 ConfIdx 81, preambleFmrt 6 startsymb 7, numSymbol 6, occassionsInPrachSlot 1
PRACH: x 1 y[0] 0, y[1] 0 prach slot: 3.. 5 .... 7 .... 9 .... 11 .... 13 ..

PRACH start symbol 7 lastsymbol 12
xran_cp_init_sectiondb:Allocation Size for Section DB : 128 (1x8x16)
xran_cp_init_sectiondb:Allocation Size for list : 1848 (28x66)
xran_cp_init_sectiondb:Allocation Size for list : 1848 (28x66)
xran_cp_init_sectiondb:Allocation Size for list : 1848 (28x66)
xran_cp_init_sectiondb:Allocation Size for list : 1848 (28x66)
xran_cp_init_sectiondb:Allocation Size for list : 1848 (28x66)
xran_cp_init_sectiondb:Allocation Size for list : 1848 (28x66)
xran_cp_init_sectiondb:Allocation Size for list : 1848 (28x66)
xran_cp_init_sectiondb:Allocation Size for list : 1848 (28x66)
xran_cp_init_sectiondb:Allocation Size for Section DB : 128 (1x8x16)
xran_cp_init_sectiondb:Allocation Size for list : 1848 (28x66)
xran_cp_init_sectiondb:Allocation Size for list : 1848 (28x66)
xran_cp_init_sectiondb:Allocation Size for list : 1848 (28x66)
xran_cp_init_sectiondb:Allocation Size for list : 1848 (28x66)
xran_cp_init_sectiondb:Allocation Size for list : 1848 (28x66)
xran_cp_init_sectiondb:Allocation Size for list : 1848 (28x66)
xran_cp_init_sectiondb:Allocation Size for list : 1848 (28x66)
xran_cp_init_sectiondb:Allocation Size for list : 1848 (28x66)
xran_open: interval_us=125
nSlotNum[0] : numDlSym[14] numGuardSym[0] numUlSym[0] XRAN_SLOT_TYPE_DL
            numDlSlots[1] numUlSlots[0] numSpSlots[0] numSpDlSlots[0] numSpUlSlots[0]
nSlotNum[1] : numDlSym[14] numGuardSym[0] numUlSym[0] XRAN_SLOT_TYPE_DL
            numDlSlots[2] numUlSlots[0] numSpSlots[0] numSpDlSlots[0] numSpUlSlots[0]
nSlotNum[2] : numDlSym[14] numGuardSym[0] numUlSym[0] XRAN_SLOT_TYPE_DL
            numDlSlots[3] numUlSlots[0] numSpSlots[0] numSpDlSlots[0] numSpUlSlots[0]
nSlotNum[3] : numDlSym[1] numGuardSym[2] numUlSym[11] XRAN_SLOT_TYPE_SP
            numDlSlots[3] numUlSlots[0] numSpSlots[1] numSpDlSlots[1] numSpUlSlots[1]
xran_fs_set_slot_type: nPhyInstanceId[0] nFrameDuplexType[1], nTddPeriod[4]
DLRate[1.000000] ULRate[0.250000]
SlotPattern:
Slot:   0    1    2    3
    0   DL   DL   DL   SP

xran_timing_source_thread [CPU  7] [PID: 292331]
MLogOpen: filename(mlog-o-du.bin) mlogSubframes (0), mlogCores(32), mlogSize(0) mlog_mask (-1)
    mlogSubframes (256), mlogCores(32), mlogSize(7168)
    localMLogTimerInit
lls-CU: thread_run start time: 06/10/19 21:09:37.000000028 UTC [125]
Start C-plane DL 25 us after TTI  [trigger on sym 3]
Start C-plane UL 55 us after TTI  [trigger on sym 7]
Start U-plane DL 50 us before OTA [offset  in sym -6]
Start U-plane UL 45 us OTA        [offset  in sym 6]
C-plane to U-plane delay 25 us after TTI
Start Sym timer 8928 ns
interval_us 125
        System clock (CLOCK_REALTIME)  resolution 1000037471 [Hz]
        Ticks per us 1000
    MLog Storage: 0x7f6298487100 -> 0x7f629bc88d20 [ 58727456 bytes ]
    localMLogFreqReg: 1000. Storing: 1000
    Mlog Open successful

----------------------------------------
MLog Info: virt=0x00007f6298487100 size=58727456
----------------------------------------
Start XRAN traffic
+---------------------------------------+
| Press 1 to start 5G NR XRAN traffic   |
| Press 2 reserved for future use       |
| Press 3 to quit                       |
+---------------------------------------+
rx_counter 0 tx_counter 1376072
rx_counter 0 tx_counter 1720112
rx_counter 0 tx_counter 2064161
rx_counter 0 tx_counter 2408212
rx_counter 0 tx_counter 2752232

type 3 to stop
3
rx_counter 0 tx_counter 3096264
Stop XRAN traffic
get_xran_iq_content
Closing timing source thread...
Closing l1 app... Ending all threads...
MLogPrint: ext_filename((null).bin)
    Opening MLog File: mlog-o-du-c0.bin
    MLog file mlog-o-du-c0.bin closed
    Mlog Print successful

Failed at  xran_mm_destroy, status -2
Dump IQs...
RX: Convert S16 I and S16 Q to cpu byte order from XRAN Ant: [0]
RX: Convert S16 I and S16 Q to cpu byte order from XRAN Ant: [1]
RX: Convert S16 I and S16 Q to cpu byte order from XRAN Ant: [2]
RX: Convert S16 I and S16 Q to cpu byte order from XRAN Ant: [3]
Storing UL FFT OUT IQ Samples in human readable format to file ./logs/o-du-rx_log_ant0.txt: from addr (0x7f62ac9c0010) size (1774080) IQ num (443520)
Storing UL FFT OUT IQ Samples in binary format to file ./logs/o-du-rx_log_ant0.bin: from addr (0x7f62ac9c0010) size (887040) bytes num (887040)
Storing UL FFT OUT IQ Samples in human readable format to file ./logs/o-du-rx_log_ant1.txt: from addr (0x7f62ac80e010) size (1774080) IQ num (443520)
Storing UL FFT OUT IQ Samples in binary format to file ./logs/o-du-rx_log_ant1.bin: from addr (0x7f62ac80e010) size (887040) bytes num (887040)
Storing UL FFT OUT IQ Samples in human readable format to file ./logs/o-du-rx_log_ant2.txt: from addr (0x7f62ac65c010) size (1774080) IQ num (443520)
Storing UL FFT OUT IQ Samples in binary format to file ./logs/o-du-rx_log_ant2.bin: from addr (0x7f62ac65c010) size (887040) bytes num (887040)
Storing UL FFT OUT IQ Samples in human readable format to file ./logs/o-du-rx_log_ant3.txt: from addr (0x7f62ac4aa010) size (1774080) IQ num (443520)
Storing UL FFT OUT IQ Samples in binary format to file ./logs/o-du-rx_log_ant3.bin: from addr (0x7f62ac4aa010) size (887040) bytes num (887040)


References
----------
* Front Haul Library Readme file




