#/******************************************************************************
#*
#*   Copyright (c) 2019 Intel.
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
1. Introduction
xRAN Lib performs communication between the low-layer split central unit (lls-CU) and RU, it is highly-optimized software implementation based on Intel Architecture to provide the standard interface implementation based on O-RAN front haul interface specification. 

2. Supported features
please refer PRD in the table <<xRAN release track table.xlsx>>, only ICC compiler was supported for this version.

3. Fixed Issues
It's first version of seed code for feature development, future fixed issues will be tracked here.

4. Known Issues
From current unit testing coverage, no Know issues was founded yet.

5. Prerequisites for install

5.1 Intel Compiler version
icc -v
icc version 18.0.1 (gcc version 4.8.5 compatibility)

Link to ICC (community free version):
https://software.intel.com/en-us/system-studio/choose-download#technical


5.2  DPDK version
dpdk_18.08

5.3   compile DPDK with command
[dpdk]# ./usertools/dpdk-setup.sh

select [16] x86_64-native-linuxapp-icc
select [18] Insert IGB UIO module
exit   [35] Exit Script

5.4 Find PCIe device of Fortville port 

lspci |grep Eth
19:00.0 Ethernet controller: Intel Corporation 82599ES 10-Gigabit SFI/SFP+ Network Connection (rev 01)
19:00.1 Ethernet controller: Intel Corporation 82599ES 10-Gigabit SFI/SFP+ Network Connection (rev 01)
41:00.0 Ethernet controller: Intel Corporation Ethernet Connection X722 for 10GBASE-T (rev 04)
41:00.1 Ethernet controller: Intel Corporation Ethernet Connection X722 for 10GBASE-T (rev 04)
d8:00.0 << Ethernet controller: Intel Corporation Ethernet Controller XL710 for 40GbE QSFP+ (rev 02) <<<< this one
d8:00.1 Ethernet controller: Intel Corporation Ethernet Controller XL710 for 40GbE QSFP+ (rev 02)

5.5 Corresponding Eth device via

ifconfig -a

find port Eth with correct PCIe Bus address as per list above

ethtool -i enp216s0f0
driver: i40e
version: 2.4.10  << i40e driver
firmware-version: 6.01 0x800034a4 1.1747.0
expansion-rom-version:
bus-info: 0000:d8:00.0 <<< this one 
supports-statistics: yes
supports-test: yes
supports-eeprom-access: yes
supports-register-dump: yes
supports-priv-flags: yes

5.6 install correct 2.4.10 i40e version if different (https://downloadcenter.intel.com/download/28306/Intel-Network-Adapter-Driver-for-PCIe-40-Gigabit-Ethernet-Network-Connections-Under-Linux-)

make sure firmare version is 

firmware-version: 6.01 

5.7 make sure that linux boot arguments are correct 

cat /proc/cmdline
BOOT_IMAGE=/vmlinuz-3.10.0-rt56 root=/dev/mapper/centos_5gnr--skx--sp-root ro crashkernel=auto rd.lvm.lv=centos_5gnr-skx-sp/root rd.lvm.lv=centos_5gnr-skx-sp/swap intel_iommu=off usbcore.autosuspend=-1 selinux=0 enforcing=0 nmi_watchdog=0 softlockup_panic=0 audit=0 intel_pstate=disable cgroup_disable=memory mce=off idle=poll hugepagesz=1G hugepages=20 hugepagesz=2M hugepages=0 default_hugepagesz=1G isolcpus=1-35 rcu_nocbs=1-35 kthread_cpus=0 irqaffinity=0 nohz_full=1-35
 
5.8 enable SRIOV VF port for XRAN 

echo 2 > /sys/class/net/enp216s0f0/device/sriov_numvfs

see https://doc.dpdk.org/guides/nics/intel_vf.html

5.9 Check Virtual Function was created 

lspci |grep Eth
19:00.0 Ethernet controller: Intel Corporation 82599ES 10-Gigabit SFI/SFP+ Network Connection (rev 01)
19:00.1 Ethernet controller: Intel Corporation 82599ES 10-Gigabit SFI/SFP+ Network Connection (rev 01)
41:00.0 Ethernet controller: Intel Corporation Ethernet Connection X722 for 10GBASE-T (rev 04)
41:00.1 Ethernet controller: Intel Corporation Ethernet Connection X722 for 10GBASE-T (rev 04)
d8:00.0 Ethernet controller: Intel Corporation Ethernet Controller XL710 for 40GbE QSFP+ (rev 02)
d8:00.1 Ethernet controller: Intel Corporation Ethernet Controller XL710 for 40GbE QSFP+ (rev 02)
d8:02.0 Ethernet controller: Intel Corporation XL710/X710 Virtual Function (rev 02) <<<< this is XRAN port (u-plane)
d8:02.1 Ethernet controller: Intel Corporation XL710/X710 Virtual Function (rev 02) <<<< this is XRAN port (c-plane)

5.10 Configure VFs
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
 
6. Install xRAN Lib 
 
6.1 start matlab and run gen_test.m
copy ant_*.bin  to /xran/app

6.2 build xran sample application
export XRAN_DIR=xRAN folder
export RTE_SDK=dpdk folder
[xRAN root folder]$ ./build.sh

6.3 update Eth port used for XRAN
in ./app/run_lls-cu.sh
ports have to match VF function from step 1.11 (0000:d8:02.0 - U-plane  0000:d8:02.1 C-plane)

6.4 Run dpdk.sh to assign port to PMD 

[xran root folder]# ./app/dpdk.sh

Network devices using DPDK-compatible driver
============================================
0000:d8:02.0 'XL710/X710 Virtual Function 154c' drv=igb_uio unused=i40evf
0000:d8:02.1 'XL710/X710 Virtual Function 154c' drv=igb_uio unused=i40evf


6.5 Run XRAN lls-CU sample app 
setup RU mac address in config_file_lls_cu.dat
[xran root folder]# ./app/run_lls-cu.sh

