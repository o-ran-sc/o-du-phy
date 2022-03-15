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


PTP Configuration
=================

PTP Synchronization
===================

Precision Time Protocol (PTP) provides an efficient way to synchronize
time on the network nodes. This protocol uses Master-Slave architecture.
Grandmaster Clock (Master) is a reference clock for the other nodes,
which adapt their clocks to the master.

Using Physical Hardware Clock (PHC) from the Grandmaster Clock, NIC port
precision timestamp packets can be served for other network nodes. Slave
nodes adjust their PHC to the master following the IEEE 1588
specification.

There are existing implementations of PTP protocol that are widely used
in the industry. One of them is PTP for Linux, which is a set of tools
providing necessary PTP functionality. There is no need to re-implement
the 1588 protocol because PTP for Linux is precise and efficient enough
to be used out of the box.

To meet O-RAN requirements, two tools from PTP for Linux package are
required: ptp4l and phc2sys.

PTP for Linux\* Requirements
============================

PTP for Linux\* introduces some software and hardware requirements. The
machine on which the tools will be run needs to use at least a 3.10
Kernel version (built-in PTP support). Several Kernel options need to be
enabled in Kernel configuration:

-  CONFIG_PPS

-  CONFIG_NETWORK_PHY_TIMESTAMPING

-  PTP_1588_CLOCK

Be sure that the Kernel is compiled with these options.

For the best precision, PTP uses hardware timestamping. NIC has its own
clock, called Physical Hardware Clock (PHC), to read current time just a
moment before the packet is sent to minimalize the delays added by the
Kernel processing the packet. Not every NIC supports that feature. To
confirm that currently attached NIC support Hardware Timestamps, use
ethtool with the command::

ethtool -T eth0

Where the eth0 is the potential PHC port. The output from the command
should say that there is Hardware Timestamps support.

To set up PTP for Linux*:

1.Download source code::

    git clone http://git.code.sf.net/p/linuxptp/code linuxptp
    git checkout v2.0
    
*Note* Apply patch (this is required to work around an issue with some of the GM PTP packet sizes.) ::

    diff --git a/msg.c b/msg.c
    old mode 100644
    new mode 100755
    index d1619d4..40d1538
    --- a/msg.c
    +++ b/msg.c
    @@ -399,9 +399,11 @@ int msg_post_recv(struct ptp_message *m, int cnt)
    port_id_post_recv(&m->pdelay_resp.requestingPortIdentity);
    break;
    case FOLLOW_UP:
    + cnt -= 4;
    timestamp_post_recv(m, &m->follow_up.preciseOriginTimestamp);
    break;
    case DELAY_RESP:
    + cnt -= 4;
    timestamp_post_recv(m, &m->delay_resp.receiveTimestamp);
    port_id_post_recv(&m->delay_resp.requestingPortIdentity);
    break;

2. Build and install ptp41. ::

   # make && make install

22. Modify configs/default.cfg to control frequency of Sync interval to 0.0625 s. ::

    logSyncInterval -4

ptp4l 
=====

This tool handles all PTP traffic on the provided NIC port and updated
PHC. It also determines the Grandmaster Clock and tracks synchronization
status. This tool can be run as a daemon or as a regular Linux\*
application. When the synchronization is reached, it gives output on the
screen for precision tracking. The configuration file of ptp4l contains
many options that can be set to get the best synchronization precision.
Although, even with default.cfg the synchronization quality is
excellent.

To start the synchronization process run::

    cd linuxptp
    ./ptp4l -f ./configs/default.cfg -2 -i <if_name> -m

The output below shows what the output on non-master node should look
like when synchronization is started. This means that PHC on this
machine is synchronized to the master PHC. ::

    ptp4l[1434165.358]: port 1: INITIALIZING to LISTENING on INIT_COMPLETE
    ptp4l[1434165.358]: port 0: INITIALIZING to LISTENING on INIT_COMPLETE
        ptp4l[1434166.384]: port 1: new foreign master fcaf6a.fffe.029708-1
        ptp4l[1434170.352]: selected best master clock fcaf6a.fffe.029708
    ptp4l[1434170.352]: updating UTC offset to 37
    ptp4l[1434170.352]: port 1: LISTENING to UNCALIBRATED on RS_SLAVE
        ptp4l[1434171.763]: master offset -5873 s0 freq -18397 path delay 2778
        ptp4l[1434172.763]: master offset -6088 s2 freq -18612 path delay 2778
        ptp4l[1434172.763]: port 1: UNCALIBRATED to SLAVE on MASTER_CLOCK_SELECTED
        ptp4l[1434173.763]: master offset -5886 s2 freq -24498 path delay 2732
        ptp4l[1434174.763]: master offset 221 s2 freq -20157 path delay 2728
        ptp4l[1434175.763]: master offset 1911 s2 freq -18401 path delay 2724
        ptp4l[1434176.763]: master offset 1774 s2 freq -17964 path delay 2728
        ptp4l[1434177.763]: master offset 1198 s2 freq -18008 path delay 2728
        ptp4l[1434178.763]: master offset 746 s2 freq -18101 path delay 2755
        ptp4l[1434179.763]: master offset 218 s2 freq -18405 path delay 2792
        ptp4l[1434180.763]: master offset 103 s2 freq -18454 path delay 2792
        ptp4l[1434181.763]: master offset -13 s2 freq -18540 path delay 2813
        ptp4l[1434182.763]: master offset 9 s2 freq -18521 path delay 2813
        ptp4l[1434183.763]: master offset 11 s2 freq -18517 path delay 2813
    
phc2sys
=======

The PHC clock is independent from the system clock. Synchronizing only
PHC does not make the system clock exactly the same as the master. The
xRAN library requires use of the system clock to determine a common
point in time on two machines (O-DU and RU) to start transmission at the
same moment and keep time frames defined by O-RAN Fronthaul
specification.

This application keeps the system clock updated to PHC. It makes it
possible to use POSIX timers as a time reference in xRAN application.

Run phc2sys with the command::

    cd linuxptp
    ./phc2sys -s enp25s0f0 -w -m -R 8

Command output will look like::

    ptp4l[1434165.342]: selected /dev/ptp4 as PTP
    phc2sys[1434344.651]: CLOCK_REALTIME phc offset       450 s2 freq  -39119 delay   1354
    phc2sys[1434344.776]: CLOCK_REALTIME phc offset       499 s2 freq  -38620 delay   1344
    phc2sys[1434344.902]: CLOCK_REALTIME phc offset       485 s2 freq  -38484 delay   1347
    phc2sys[1434345.027]: CLOCK_REALTIME phc offset       476 s2 freq  -38348 delay   1346
    phc2sys[1434345.153]: CLOCK_REALTIME phc offset       392 s2 freq  -38289 delay   1340
    phc2sys[1434345.278]: CLOCK_REALTIME phc offset       319 s2 freq  -38244 delay   1340
    phc2sys[1434345.404]: CLOCK_REALTIME phc offset       278 s2 freq  -38190 delay   1349
    phc2sys[1434345.529]: CLOCK_REALTIME phc offset       221 s2 freq  -38163 delay   1343
    phc2sys[1434345.654]: CLOCK_REALTIME phc offset        97 s2 freq  -38221 delay   1342
    phc2sys[1434345.780]: CLOCK_REALTIME phc offset        67 s2 freq  -38222 delay   1344
    phc2sys[1434345.905]: CLOCK_REALTIME phc offset        68 s2 freq  -38201 delay   1341
    phc2sys[1434346.031]: CLOCK_REALTIME phc offset       104 s2 freq  -38144 delay   1340
    phc2sys[1434346.156]: CLOCK_REALTIME phc offset        58 s2 freq  -38159 delay   1340
    phc2sys[1434346.281]: CLOCK_REALTIME phc offset        12 s2 freq  -38188 delay   1343
    phc2sys[1434346.407]: CLOCK_REALTIME phc offset       -36 s2 freq  -38232 delay   1342
    phc2sys[1434346.532]: CLOCK_REALTIME phc offset      -103 s2 freq  -38310 delay   1348

Configuration C3
================

Configuration C3 27 can be simulated for O-DU using a separate server
acting as Fronthaul Network and O-RU at the same time. O-RU server can
be configured to relay PTP and act as PTP master for O-DU. Settings
below can be used to instantiate this scenario. The difference is that
on the O-DU side, the Fronthaul port can be used as the source of PTP as
well as for U-plane and C-plane traffic.

1. Follow the steps in Appendix *B.1.1,* *PTP for Linux\* Requirements*
to install PTP on the O-RU server.

2.Copy configs/default.cfg to configs/default_slave.cfg and modify the
Copied file as below::

    diff --git a/configs/default.cfg b/configs/default.cfg
    old mode 100644
    new mode 100755
    index e23dfd7..f1ecaf1
    --- a/configs/default.cfg
    +++ b/configs/default.cfg
    @@ -3,26 +3,26 @@
    # Default Data Set
    #
    twoStepFlag 1
    -slaveOnly 0
    +slaveOnly 1
    priority1 128
    -priority2 128
    +priority2 255
    domainNumber 0
    #utc_offset 37
    -clockClass 248
    +clockClass 255
    clockAccuracy 0xFE
    offsetScaledLogVariance 0xFFFF
    free_running 0
    freq_est_interval 1
    dscp_event 0
    dscp_general 0
    -dataset_comparison ieee1588
    +dataset_comparison G.8275.x
    G.8275.defaultDS.localPriority 128
    maxStepsRemoved 255
    #
    # Port Data Set
    #
    logAnnounceInterval 1
    -logSyncInterval 0
    +logSyncInterval -4
    operLogSyncInterval 0
    logMinDelayReqInterval 0
    logMinPdelayReqInterval 0
    @@ -37,7 +37,7 @@ G.8275.portDS.localPriority 128
    asCapable auto
    BMCA ptp
    inhibit_announce 0
    -inhibit_pdelay_req 0
    +#inhibit_pdelay_req 0
    ignore_source_id 0
    #
    # Run time options


3. Start slave port toward PTP GM::

    ./ptp4l -f ./configs/default_slave.cfg -2 -i enp25s0f0 –m

Example of output::

    ./ptp4l -f ./configs/default_slave.cfg -2 -i enp25s0f0 -m
    ptp4l[3904470.256]: selected /dev/ptp6 as PTP clock
    ptp4l[3904470.274]: port 1: INITIALIZING to LISTENING on INIT_COMPLETE
    ptp4l[3904470.275]: port 0: INITIALIZING to LISTENING on INIT_COMPLETE
    ptp4l[3904471.085]: port 1: new foreign master fcaf6a.fffe.029708-1
    ptp4l[3904475.053]: selected best master clock fcaf6a.fffe.029708
    ptp4l[3904475.053]: updating UTC offset to 37
    ptp4l[3904475.053]: port 1: LISTENING to UNCALIBRATED on RS_SLAVE
    ptp4l[3904477.029]: master offset        196 s0 freq  -18570 path delay      1109
    ptp4l[3904478.029]: master offset        212 s2 freq  -18554 path delay      1109
    ptp4l[3904478.029]: port 1: UNCALIBRATED to SLAVE on MASTER_CLOCK_SELECTED
    ptp4l[3904479.029]: master offset         86 s2 freq  -18468 path delay      1109
    ptp4l[3904480.029]: master offset         23 s2 freq  -18505 path delay      1124
    ptp4l[3904481.029]: master offset          3 s2 freq  -18518 path delay      1132
    ptp4l[3904482.029]: master offset       -169 s2 freq  -18689 path delay      1141
    
4. Synchronize local timer clock on O-RU for sample application ::

   ./phc2sys -s enp25s0f0 -w -m -R 8

Example of output::

   ./phc2sys -s enp25s0f0 -w -m -R 8
    phc2sys[3904510.892]: CLOCK_REALTIME phc offset   343 s0 freq  -38967 delay   1530
    phc2sys[3904511.017]: CLOCK_REALTIME phc offset   368 s2 freq  -38767 delay   1537
    phc2sys[3904511.142]: CLOCK_REALTIME phc offset   339 s2 freq  -38428 delay   1534
    phc2sys[3904511.267]: CLOCK_REALTIME phc offset   298 s2 freq  -38368 delay   1532
    phc2sys[3904511.392]: CLOCK_REALTIME phc offset   239 s2 freq  -38337 delay   1534
    phc2sys[3904511.518]: CLOCK_REALTIME phc offset   145 s2 freq  -38360 delay   1530
    phc2sys[3904511.643]: CLOCK_REALTIME phc offset   106 s2 freq  -38355 delay   1527
    phc2sys[3904511.768]: CLOCK_REALTIME phc offset   -30 s2 freq  -38459 delay   1534
    phc2sys[3904511.893]: CLOCK_REALTIME phc offset   -92 s2 freq  -38530 delay   1530
    phc2sys[3904512.018]: CLOCK_REALTIME phc offset  -173 s2 freq  -38639 delay   1528
    phc2sys[3904512.143]: CLOCK_REALTIME phc offset  -246 s2 freq  -38764 delay   1530
    phc2sys[3904512.268]: CLOCK_REALTIME phc offset  -300 s2 freq  -38892 delay   1532
   
5. Modify configs/default.cfg as shown below to run PTP master on Fronthaul of O-RU. ::

    diff --git a/configs/default.cfg b/configs/default.cfg
    old mode 100644
    new mode 100755
    index e23dfd7..c9e9d4c
    --- a/configs/default.cfg
    +++ b/configs/default.cfg
    @@ -15,14 +15,14 @@ free_running 0
    freq_est_interval 1
    dscp_event 0
    dscp_general 0
    -dataset_comparison ieee1588
    +dataset_comparison G.8275.x
    G.8275.defaultDS.localPriority 128
    maxStepsRemoved 255
    #
    # Port Data Set
    #
    logAnnounceInterval 1
    -logSyncInterval 0
    +logSyncInterval -4
    operLogSyncInterval 0
    logMinDelayReqInterval 0
    logMinPdelayReqInterval 0
    @@ -37,7 +37,7 @@ G.8275.portDS.localPriority 128
    asCapable auto
    BMCA ptp
    inhibit_announce 0
    -inhibit_pdelay_req 0
    +#inhibit_pdelay_req 0
    ignore_source_id 0
    #
    # Run time options

6. Start PTP master toward O-DU::

   ./ptp4l -f ./configs/default.cfg -2 -i enp175s0f1 –m

Example of output::

   ./ptp4l -f ./configs/default.cfg -2 -i enp175s0f1 -m
   ptp4l[3903857.249]: selected /dev/ptp3 as PTP clock
   ptp4l[3903857.266]: port 1: INITIALIZING to LISTENING on INIT_COMPLETE
   ptp4l[3903857.267]: port 0: INITIALIZING to LISTENING on INIT_COMPLETE
    ptp4l[3903863.734]: port 1: LISTENING to MASTER on ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES
    ptp4l[3903863.734]: selected local clock 3cfdfe.fffe.bd005d as best master
    ptp4l[3903863.734]: assuming the grand master role
   
7. Synchronize local NIC PTP master clock to local NIC PTP slave clock. ::

   ./phc2sys -c enp175s0f1 -s enp25s0f0 -w -m -R 8

Example of output::

   ./phc2sys -c enp175s0f1 -s enp25s0f0 -w -m -R 8
    phc2sys[3904600.332]: enp175s0f1 phc offset      2042 s0 freq   -2445 delay   4525
    phc2sys[3904600.458]: enp175s0f1 phc offset      2070 s2 freq   -2223 delay   4506
   phc2sys[3904600.584]: enp175s0f1 phc offset 2125 s2 freq -98 delay 4505
   phc2sys[3904600.710]: enp175s0f1 phc offset 1847 s2 freq +262 delay 4518
   phc2sys[3904600.836]: enp175s0f1 phc offset 1500 s2 freq +469 delay 4515
   phc2sys[3904600.961]: enp175s0f1 phc offset 1146 s2 freq +565 delay 4547
   phc2sys[3904601.086]: enp175s0f1 phc offset 877 s2 freq +640 delay 4542
   phc2sys[3904601.212]: enp175s0f1 phc offset 517 s2 freq +543 delay 4517
   phc2sys[3904601.337]: enp175s0f1 phc offset 189 s2 freq +370 delay 4510
   phc2sys[3904601.462]: enp175s0f1 phc offset -125 s2 freq +113 delay 4554
   phc2sys[3904601.587]: enp175s0f1 phc offset -412 s2 freq -212 delay 4513
   phc2sys[3904601.712]: enp175s0f1 phc offset -693 s2 freq -617 delay 4519
    phc2sys[3904601.837]: enp175s0f1 phc offset      -878 s2 freq   -1009 delay   4515
    phc2sys[3904601.962]: enp175s0f1 phc offset      -965 s2 freq   -1360 delay   4518
    phc2sys[3904602.088]: enp175s0f1 phc offset     -1048 s2 freq   -1732 delay   4510
    phc2sys[3904602.213]: enp175s0f1 phc offset     -1087 s2 freq   -2086 delay   4531
    phc2sys[3904602.338]: enp175s0f1 phc offset     -1014 s2 freq   -2339 delay   4528
    phc2sys[3904602.463]: enp175s0f1 phc offset     -1009 s2 freq   -2638 delay   4531
   
8. On O-DU Install PTP for Linux tools from source code the same way as
on O-RU above but no need to apply the patch for msg.c

9. Start slave port toward PTP master from O-RU using the same
default_slave.cfg as on O-RU (see above)::

    ./ptp4l -f ./configs/default_slave.cfg -2 -i enp181s0f0 –m

Example of output::

    ./ptp4l -f ./configs/default_slave.cfg -2 -i enp181s0f0 -m
    ptp4l[809092.918]: selected /dev/ptp6 as PTP clock
    ptp4l[809092.934]: port 1: INITIALIZING to LISTENING on INIT_COMPLETE
    ptp4l[809092.934]: port 0: INITIALIZING to LISTENING on INIT_COMPLETE
    ptp4l[809092.949]: port 1: new foreign master 3cfdfe.fffe.bd005d-1
    ptp4l[809096.949]: selected best master clock 3cfdfe.fffe.bd005d
    ptp4l[809096.950]: port 1: LISTENING to UNCALIBRATED on RS_SLAVE
    ptp4l[809098.363]: port 1: UNCALIBRATED to SLAVE on MASTER_CLOCK_SELECTED
    ptp4l[809099.051]: rms 38643 max 77557 freq   +719 +/- 1326 delay  1905 +/-   0
    ptp4l[809100.051]: rms 1134 max 1935 freq -103 +/- 680 delay 1891 +/- 4
    ptp4l[809101.051]: rms 453 max 855 freq +341 +/- 642 delay 1888 +/- 0
    ptp4l[809102.052]: rms 491 max 772 freq +1120 +/- 752 delay 1702 +/- 0
    ptp4l[809103.052]: rms 423 max 654 freq +1352 +/- 653 delay 1888 +/- 0
    ptp4l[809104.052]: rms 412 max 579 freq +1001 +/- 672 delay 1702 +/- 0
    ptp4l[809105.053]: rms 441 max 672 freq +807 +/- 709 delay 1826 +/- 88
    ptp4l[809106.053]: rms 422 max 607 freq +1353 +/- 636 delay 1702 +/- 0
    ptp4l[809107.054]: rms 401 max 466 freq +946 +/- 646 delay 1702 +/- 0
    ptp4l[809108.055]: rms 401 max 502 freq +912 +/- 659

10. Synchronize local clock on O-DU for sample application or l1
Application. ::

    ./phc2sys -s enp181s0f0 -w -m -R 8

Example of output::

   ./phc2sys -s enp181s0f0 -w -m -R 8
    phc2sys[809127.123]: CLOCK_REALTIME phc offset    675 s0 freq  -37379 delay   1646
    phc2sys[809127.249]: CLOCK_REALTIME phc offset    696 s2 freq  -37212 delay   1654
    phc2sys[809127.374]: CLOCK_REALTIME phc offset    630 s2 freq  -36582 delay   1648
    phc2sys[809127.500]: CLOCK_REALTIME phc offset    461 s2 freq  -36562 delay   1642
    phc2sys[809127.625]: CLOCK_REALTIME phc offset    374 s2 freq  -36510 delay   1643
    phc2sys[809127.751]: CLOCK_REALTIME phc offset    122 s2 freq  -36650 delay   1649
    phc2sys[809127.876]: CLOCK_REALTIME phc offset     34 s2 freq  -36702 delay   1650
    phc2sys[809128.002]: CLOCK_REALTIME phc offset   -112 s2 freq  -36837 delay   1645
    phc2sys[809128.127]: CLOCK_REALTIME phc offset   -160 s2 freq  -36919 delay   1643
    phc2sys[809128.252]: CLOCK_REALTIME phc offset   -270 s2 freq  -37077 delay   1657
    phc2sys[809128.378]: CLOCK_REALTIME phc offset   -285 s2 freq  -37173 delay   1644
    phc2sys[809128.503]: CLOCK_REALTIME phc offset   -349 s2 freq  -37322 delay   1644
    phc2sys[809128.629]: CLOCK_REALTIME phc offset   -402 s2 freq  -37480 delay   1641
    phc2sys[809128.754]: CLOCK_REALTIME phc offset   -377 s2 freq  -37576 delay   1648
    phc2sys[809128.879]: CLOCK_REALTIME phc offset   -467 s2 freq  -37779 delay   1650
    phc2sys[809129.005]: CLOCK_REALTIME phc offset   -408 s2 freq  -37860 delay   1648
    phc2sys[809129.130]: CLOCK_REALTIME phc offset   -480 s2 freq  -38054 delay   1655
    phc2sys[809129.256]: CLOCK_REALTIME phc offset   -350 s2 freq  -38068 delay   1650

Support in xRAN Library
=======================

The xRAN library provides an API to check whether PTP for Linux is
running correctly. There is a function called xran_is_synchronized(). It
checks if ptp4l and phc2sys are running in the system by making PMC tool
requests for the current port state and comparing it with the expected
value. This verification should be done before initialization.

-  “SLAVE” is the only expected value in this release; only a non-master scenario is supported currently.

