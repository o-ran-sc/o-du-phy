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

.. |br| raw:: html

   <br />

Setup Configuration
===================

A.1 Setup Configuration
-----------------------
The configuration shown in Figure 26 shows how to set up a test
environment to execute xRAN scenarios where O-DU and 0-RU are simulated
using the sample application. This setup allows development and
prototyping as well as testing of xRAN specific functionality. The O-DU
side can be instantiated with a full 5G NR L1 reference as well. The
configuration differences of the 5G NR l1app configuration are provided
below. Steps for running the sample application on the O-DU side and
0-RU side are the same, except configuration file options may be
different.

.. image:: images/Setup-for-xRAN-Testing.jpg
  :width: 400
  :alt: Figure 26. Setup for xRAN Testing

Figure 26. Setup for xRAN Testing

.. image:: images/Setup-for-xRAN-Testing-with-PHY-and-Configuration-C3.jpg
  :width: 400
  :alt: Figure 27. Setup for xRAN Testing with PHY and Configuration C3

Figure 27. Setup for xRAN Testing with PHY and Configuration C3

A.2 Prerequisites
-----------------
Each server in Figure 26 requires the following:

-  Wolfpass server according to recommended BOM for FlexRAN such as
   Intel® Xeon® Skylake Gold 6148 FC-LGA3647 2.4 GHz 27.5 MB 150W 20
   cores (two sockets)

-  BIOS settings:

-  Intel(R) Virtualization Technology Enabled

-  Intel(R) VT for Directed I/O - Enabled

-  ACS Control - Enabled

-  Coherency Support - Disabled

-  Front Haul networking cards:

-  Intel® Ethernet Converged Network Adapter XL710-QDA2

-  Intel® Ethernet Converged Network Adapter XXV710-DA2

-  Intel® FPGA Programmable Acceleration Card (Intel® FPGA PAC) N3000

**The Front Haul NIC requires support for PTP HW timestamping.**

The recommended configuration for NICs is::


    ethtool -i enp33s0f0
    driver: i40e
    version: 2.10.19.82
    firmware-version: 7.20 0x80007949 1.2585.0
    expansion-rom-version:
    bus-info: 0000:21:00.0
    supports-statistics: yes
    supports-test: yes
    supports-eeprom-access: yes
    supports-register-dump: yes
    supports-priv-flags: yes
    ethtool -T enp33s0f0
    Time stamping parameters for enp33s0f0:
    Capabilities:
        hardware-transmit     (SOF_TIMESTAMPING_TX_HARDWARE)
        software-transmit     (SOF_TIMESTAMPING_TX_SOFTWARE)
        hardware-receive      (SOF_TIMESTAMPING_RX_HARDWARE)
        software-receive      (SOF_TIMESTAMPING_RX_SOFTWARE)
        software-system-clock (SOF_TIMESTAMPING_SOFTWARE)
        hardware-raw-clock    (SOF_TIMESTAMPING_RAW_HARDWARE)
    PTP Hardware Clock: 4
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


A PTP Main Reference Clock is required to be available in the network to provide
synchronization of both O-DU and RU to GPS time.

1.Installing Intel® C++ Compiler v19.0.3 is preferred. or you could get
Intel® C++ Compiler through below link with community license,
however the version you could get is always latest version, the
verification for that version might not be performed yet, please
feedback through O-DU Low project WIKI page if you meet an issue. |br|
`https://software.intel.com/en-us/system-studio/choose-download <https://software.intel.com/en-us/system-studio/choose-download%20>`__

2.Download DPDK 19.11.

3.Change DPDK files according to below diff information which relevant to O-RAN FH::

    diff --git a/drivers/net/i40e/i40e_ethdev.c
    b/drivers/net/i40e/i40e_ethdev.c
    
    index 85a6a86..236fbe0 100644
    
    --- a/drivers/net/i40e/i40e_ethdev.c
    
    +++ b/drivers/net/i40e/i40e_ethdev.c
    
    @@ -2207,7 +2207,7 @@ void i40e_flex_payload_reg_set_default(struct
    i40e_hw \*hw)
    
    /\* Map queues with MSIX interrupt \*/
    
    main_vsi->nb_used_qps = dev->data->nb_rx_queues -
    
    pf->nb_cfg_vmdq_vsi \* RTE_LIBRTE_I40E_QUEUE_NUM_PER_VM;
    
    - i40e_vsi_queues_bind_intr(main_vsi, I40E_ITR_INDEX_DEFAULT);
    
    + i40e_vsi_queues_bind_intr(main_vsi, I40E_ITR_INDEX_NONE);
    
    i40e_vsi_enable_queues_intr(main_vsi);
    
    /\* Map VMDQ VSI queues with MSIX interrupt \*/
    
    @@ -2218,6 +2218,10 @@ void i40e_flex_payload_reg_set_default(struct
    i40e_hw \*hw)
    
    i40e_vsi_enable_queues_intr(pf->vmdq[i].vsi);
    
    }
    
    + i40e_aq_debug_write_global_register(hw,
    
    + 0x0012A504,
    
    + 0, NULL);
    
    +
    
    /\* enable FDIR MSIX interrupt \*/
    
    if (pf->fdir.fdir_vsi) {
    
    i40e_vsi_queues_bind_intr(pf->fdir.fdir_vsi,
    
    diff --git a/drivers/net/i40e/i40e_ethdev_vf.c
    b/drivers/net/i40e/i40e_ethdev_vf.c
    
    index 001c301..6f9ffdb 100644
    
    --- a/drivers/net/i40e/i40e_ethdev_vf.c
    
    +++ b/drivers/net/i40e/i40e_ethdev_vf.c
    
    @@ -640,7 +640,7 @@ struct rte_i40evf_xstats_name_off {
    
    map_info = (struct virtchnl_irq_map_info \*)cmd_buffer;
    
    map_info->num_vectors = 1;
    
    - map_info->vecmap[0].rxitr_idx = I40E_ITR_INDEX_DEFAULT;
    
    + map_info->vecmap[0].rxitr_idx = I40E_ITR_INDEX_NONE;
    
    map_info->vecmap[0].vsi_id = vf->vsi_res->vsi_id;
    
    /\* Alway use default dynamic MSIX interrupt \*/
    
    map_info->vecmap[0].vector_id = vector_id;
    
    diff --git a/drivers/net/ixgbe/ixgbe_ethdev.c
    b/drivers/net/ixgbe/ixgbe_ethdev.c
    
    index 26b1927..018eb8f 100644
    
    --- a/drivers/net/ixgbe/ixgbe_ethdev.c
    
    +++ b/drivers/net/ixgbe/ixgbe_ethdev.c
    
    @@ -3705,7 +3705,7 @@ static int
    ixgbevf_dev_xstats_get_names(__rte_unused struct rte_eth_dev \*dev,
    
    \* except for 82598EB, which remains constant.
    
    \*/
    
    if (dev_conf->txmode.mq_mode == ETH_MQ_TX_NONE &&
    
    - hw->mac.type != ixgbe_mac_82598EB)
    
    + hw->mac.type != ixgbe_mac_82598EB && hw->mac.type !=
    ixgbe_mac_82599EB)
    
    dev_info->max_tx_queues = IXGBE_NONE_MODE_TX_NB_QUEUES;
    
    }
    
    dev_info->min_rx_bufsize = 1024; /\* cf BSIZEPACKET in SRRCTL register
    \*/
    
    diff --git a/lib/librte_eal/common/include/rte_dev.h
    b/lib/librte_eal/common/include/rte_dev.h
    
    old mode 100644
    
    new mode 100755

5.Build and install DPDK::

   [root@xran dpdk]# ./usertools/dpdk-setup.sh
   
   select [39] x86_64-native-linuxapp-icc
   
   select [46] Insert VFIO module
   
   exit [62] Exit Script

6.Make below file changes in dpdk that assure i40e to get best
latency of packet processing::

    --- i40e.h 2018-11-30 11:27:00.000000000 +0000
    
    +++ i40e_patched.h 2019-03-06 15:49:06.877522427 +0000
    
    @@ -451,7 +451,7 @@
    
    #define I40E_QINT_RQCTL_VAL(qp, vector, nextq_type) \\
    
    (I40E_QINT_RQCTL_CAUSE_ENA_MASK \| \\
    
    - (I40E_RX_ITR << I40E_QINT_RQCTL_ITR_INDX_SHIFT) \| \\
    
    + (I40E_ITR_NONE << I40E_QINT_RQCTL_ITR_INDX_SHIFT) \| \\
    
    ((vector) << I40E_QINT_RQCTL_MSIX_INDX_SHIFT) \| \\
    
    ((qp) << I40E_QINT_RQCTL_NEXTQ_INDX_SHIFT) \| \\
    
    (I40E_QUEUE_TYPE_##nextq_type << I40E_QINT_RQCTL_NEXTQ_TYPE_SHIFT))
    
    --- i40e_main.c 2018-11-30 11:27:00.000000000 +0000
    
    +++ i40e_main_patched.c 2019-03-06 15:46:13.521518062 +0000
    
    @@ -15296,6 +15296,9 @@
    
    pf->hw_features \|= I40E_HW_HAVE_CRT_RETIMER;
    
    /\* print a string summarizing features \*/
    
    i40e_print_features(pf);
    
    +
    
    + /\* write to this register to clear rx descriptor \*/
    
    + i40e_aq_debug_write_register(hw, 0x0012A504, 0, NULL);
    
    return 0;
    
A.3 Configuration of System
---------------------------
1.Boot Linux with the following arguments::

    cat /proc/cmdline
    
    BOOT_IMAGE=/vmlinuz-3.10.0-1062.12.1.rt56.1042.el7.x86_64 root=/dev/mapper/centos-root ro
    crashkernel=auto rd.lvm.lv=centos/root rd.lvm.lv=centos/swap intel_iommu=on iommu=pt
    usbcore.autosuspend=-1 selinux=0 enforcing=0 nmi_watchdog=0 softlockup_panic=0 audit=0
    intel_pstate=disable cgroup_memory=1 cgroup_enable=memory mce=off idle=poll
    hugepagesz=1G hugepages=16 hugepagesz=2M hugepages=0 default_hugepagesz=1G
    isolcpus=1-19,21-39 rcu_nocbs=1-19,21-39 kthread_cpus=0,20 irqaffinity=0,20
    nohz_full=1-19,21-39
    
2.Download from Intel Website and install updated version of i40e
driver if needed. The current recommended version of i40e is 2.10.19.82. 
However, any latest version of i40e after  x2.9.21 expected to be functional for ORAN FH.

3.Identify PCIe Bus address of the Front Haul NIC::

    lspci |grep Eth
    19:00.0 Ethernet controller: Intel Corporation Ethernet Controller XXV710 Intel(R) FPGA Programmable Acceleration Card N3000 for Networking (rev 02)
    19:00.1 Ethernet controller: Intel Corporation Ethernet Controller XXV710 Intel(R) FPGA Programmable Acceleration Card N3000 for Networking (rev 02)
    1d:00.0 Ethernet controller: Intel Corporation Ethernet Controller XXV710 Intel(R) FPGA Programmable Acceleration Card N3000 for Networking (rev 02)
    1d:00.1 Ethernet controller: Intel Corporation Ethernet Controller XXV710 Intel(R) FPGA Programmable Acceleration Card N3000 for Networking (rev 02)
    21:00.0 Ethernet controller: Intel Corporation Ethernet Controller XXV710 for 25GbE SFP28 (rev 02)
    21:00.1 Ethernet controller: Intel Corporation Ethernet Controller XXV710 for 25GbE SFP28 (rev 02)
    67:00.0 Ethernet controller: Intel Corporation Ethernet Connection X722 for 10GBASE-T (rev 09)

    
4.Identify the Ethernet device name::

    ethtool -i enp33s0f0
    driver: i40e
    version: 2.10.19.82
    firmware-version: 7.20 0x80007949 1.2585.0
    expansion-rom-version:
    bus-info: 0000:21:00.0
    supports-statistics: yes
    supports-test: yes
    supports-eeprom-access: yes
    supports-register-dump: yes
    supports-priv-flags: yes
    Enable


5.Enable two virtual functions (VF) on the device::

    echo 2 > /sys/class/net/enp33s0f0/device/sriov_numvfs

More information about VFs supported by Intel NICs can be found at
https://doc.dpdk.org/guides/nics/intel_vf.html.

The resulting configuration can look like the listing below, where two
new VFs were added::

    lspci|grep Eth
    
    21:00.0 Ethernet controller: Intel Corporation Ethernet Controller XXV710 for 25GbE SFP28 (rev 02)
    21:00.1 Ethernet controller: Intel Corporation Ethernet Controller XXV710 for 25GbE SFP28 (rev 02)
    21:02.0 Ethernet controller: Intel Corporation Ethernet Virtual Function 700 Series (rev 02)
    21:02.1 Ethernet controller: Intel Corporation Ethernet Virtual Function 700 Series (rev 02)


6.Configure MAC address and VLAN settings for VFs for XRAN, based on
requirements for xRAN scenario and assignment of VLAN ID using IP
tool perform configuration of VF.
    
    Example where O-DU and O-RU simulation run on the same sytem::

    #!/bin/bash
    
    echo 2 > /sys/bus/pci/devices/0000\:21\:00.0/sriov_numvfs
    ip link set enp33s0f0 vf 1 mac 00:11:22:33:44:66 vlan 1
    ip link set enp33s0f0 vf 0 mac 00:11:22:33:44:66 vlan 2
    echo 2 > /sys/bus/pci/devices/0000\:21\:00.1/sriov_numvfs
    ip link set enp33s0f1 vf 1 mac 00:11:22:33:44:55 vlan 1
    ip link set enp33s0f1 vf 0 mac 00:11:22:33:44:55 vlan 2
    
    where output is next::
    
    [root@xran app]# ip link show
    
    1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN mode DEFAULT group default qlen 1000
    
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    
    2: enp25s0f0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP mode DEFAULT group default qlen 1000
    
    link/ether 64:4c:36:10:1f:30 brd ff:ff:ff:ff:ff:ff
    
    3: enp25s0f1: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP mode DEFAULT group default qlen 1000
    
    link/ether 64:4c:36:10:1f:31 brd ff:ff:ff:ff:ff:ff
    
    4: enp29s0f0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP mode DEFAULT group default qlen 1000
    
    link/ether 64:4c:36:10:1f:34 brd ff:ff:ff:ff:ff:ff
    
    5: enp29s0f1: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP mode DEFAULT group default qlen 1000
    
    link/ether 64:4c:36:10:1f:35 brd ff:ff:ff:ff:ff:ff
    
    6: enp33s0f0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP mode DEFAULT group default qlen 1000
    
    link/ether 3c:fd:fe:b9:f8:b4 brd ff:ff:ff:ff:ff:ff
    
    vf 0 MAC 00:11:22:33:44:66, vlan 2, spoof checking on, link-state auto, trust off
    
    vf 1 MAC 00:11:22:33:44:66, vlan 1, spoof checking on, link-state auto, trust off
    
    7: enp33s0f1: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP mode DEFAULT group default qlen 1000
    
    link/ether 3c:fd:fe:b9:f8:b5 brd ff:ff:ff:ff:ff:ff
    
    vf 0 MAC 00:11:22:33:44:55, vlan 2, spoof checking on, link-state auto, trust off
    
    vf 1 MAC 00:11:22:33:44:55, vlan 1, spoof checking on, link-state auto, trust off
    
    8: eno1: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP mode DEFAULT group default qlen 1000
    
    link/ether a4:bf:01:3e:1f:be brd ff:ff:ff:ff:ff:ff
    
    9: eno2: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP mode DEFAULT group default qlen 1000
    
    link/ether a4:bf:01:3e:1f:bf brd ff:ff:ff:ff:ff:ff
    
    10: npacf0g0l0: <LOWER_UP> mtu 9600 qdisc noop state UNKNOWN mode DEFAULT group default qlen 1000
    
    link/generic
    
    11: npacf0g0l1: <LOWER_UP> mtu 9600 qdisc noop state UNKNOWN mode DEFAULT group default qlen 1000
    
    link/generic
    
    12: npacf0g0l2: <LOWER_UP> mtu 9600 qdisc noop state UNKNOWN mode DEFAULT group default qlen 1000
    
    link/generic
    
    13: npacf0g0l3: <LOWER_UP> mtu 9600 qdisc noop state UNKNOWN mode DEFAULT group default qlen 1000
    
    link/generic

After this step FH NIC is configured.

O-DU
 
VF for C-plane is VF1 on PFH enp33s0f0enp216s0f0, it has ETH mac address 00:11:22:33:44:66 and VLAN tag 1. PCIe Bus address is VF1 is 21d8:02.1

VF for U-plane is VF0 on PFH enp33s0f0enp216s0f0, it has ETH mac address 00:11:22:33:44:66 and VLAN tag 2. PCIe Bus address is VF1 is 21d8:02.0

O-RU

VF for C-plane is VF1 on PF enp33s0f1, it has ETH mac address 00:11:22:33:44:55 and VLAN tag 1. PCIe Bus address is VF1 is 21:0a.1

VF for U-plane is VF0 on PF enp33s0f1, it has ETH mac address 00:11:22:33:44:55 and VLAN tag 2. PCIe Bus address is VF1 is 21:0a.0


A.4 Install and Configure Sample Application
--------------------------------------------
To install and configure the sample application:

1. Set up the environment:

   export GTEST_ROOT=`pwd`/gtest-1.7.0
   
   export RTE_SDK=`pwd`/dpdk-19.11
   
   export RTE_TARGET=x86_64-native-linuxapp-icc
   
   export MLOG_DIR=`pwd`/flexran_l1_sw/libs/mlog
   
   export XRAN_DIR=`pwd`/flexran_xran

2. Compile xRAN library and test the application:

   [turner@xran home]$ cd $XRAN_DIR
   
   [turner@xran xran]$ ./build.sh
   
3. Configure the sample app.

IQ samples can be generated using Octave\* and script
libs/xran/app/gen_test.m. (CentOS\* has octave-3.8.2-20.el7.x86_64
compatible with get_test.m)

Other IQ sample test vectors can be used as well. The format of IQ
samples is binary int16_t I and Q for N slots of the OTA RF signal. For
example, for mmWave, it corresponds to 792RE*2*14symbol*8slots*10 ms =
3548160 bytes per antenna. Refer to comments in gen_test.m to correctly
specify the configuration for IQ test vector generation.

Update config_file_o_du.dat (or config_file_o_ru.dat) with a suitable
configuration for your scenario.

Update run_o_du.sh (run_o_ru.sh) with PCIe bus address of VF0 and VF1
used for U-plane and C-plane correspondingly::

    ./build/sample-app -c ./usecase/mu0_10mhz/config_file_o_du.dat -p 2 0000:21d8:02.0 0000:21d8:02.1

4. Run application using run_o_du.sh (run_o_ru.sh).



