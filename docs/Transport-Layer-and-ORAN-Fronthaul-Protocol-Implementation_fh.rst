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

Transport Layer and ORAN Fronthaul Protocol Implementation
==========================================================

.. contents::
    :depth: 3
    :local:

This chapter describes how the transport layer and ORAN Fronthaul
protocol are implemented.

.. _introduction-2:

Introduction
------------

Figure 8 presents an overview of the ORAN Fronthaul process.

.. image:: images/ORAN-Fronthaul-Process.jpg
  :width: 600
  :alt: Figure 8. ORAN Fronthaul Process

Figure 8. ORAN Fronthaul Process

The XRAN library provides support for transporting In-band and
Quadrature (IQ) samples between the O-DU and O-RU within the xRAN
architecture based on functional split 7.2x. The library defines the
xRAN packet formats to be used to transport radio samples within Front
Haul according to the ORAN Fronthaul specification. It provides
functionality for generating xRAN packets, appending IQ samples in the
packet payload, and extracting IQ samples from xRAN packets. 

Note: The Bronze release version of the library supports U-plane and C-plane only. It is ready to be used in the PTP synchronized environment.

Note: Regarding the clock model and synchronization topology, configurations
C1 and C3 of the connection between O-DU and O-RU are the only
configurations supported in this release of the xRAN implementation.

Note: Quality of PTP synchronization with respect to S-plane of ORAN 
Fronthaul requirements as defined for O-RU is out of the scope of this
document. PTP primary and PTP secondary configuration are expected to satisfy
only the O-DU side of requirements and provide the “best-effort” PTP primary for
O-RU. This may or may not be sufficient for achieving the end to end
system requirements of S-plane. Specialized dedicated NIC card with
additional HW functionality might be required to achieve PTP primary
functionality to satisfy O-RU precision requirements for RAN deployments
scenarios.

.. image:: images/Configuration-C1.jpg
  :width: 600
  :alt: Figure 9. Configuration C1

Figure 9. Configuration C1


.. image:: images/Configuration-C3.jpg
  :width: 600
  :alt: Figure 10. Configuration C3

Figure 10. Configuration C3

Supported Feature Set
---------------------

The ORAN Fronthaul specification defines a list of mandatory
functionality. Not all features defined as Mandatory for O-DU are
currently supported to fully extended. The following tables contain
information on what is available and the level of validation performed
for this release.

Note. Cells with a red background are listed as mandatory in the
specification but not supported in this implementation of xRAN.

Table 7. ORAN Mandatory and Optional Feature Support

+-----------------+-----------------+-----------+----------------+
| Category        | Feature         | O-DU      | Support        |
|                 |                 | Support   |                |
+=================+=================+===========+================+
| RU Category     | Support for     | Mandatory | Y              |
|                 | CAT-A RU (up to |           |                |
|                 | 8 spatial       |           |                |
|                 | streams)        |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | Support for     |           | Y              |
|                 | CAT-A RU (> 8   |           |                |
|                 | spatial         |           |                |
|                 | streams)        |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | Support for     | Mandatory | Y              |
|                 | CAT-B RU        |           |                |
|                 | (precoding in   |           |                |
|                 | RU)             |           |                |
+-----------------+-----------------+-----------+----------------+
| Beamforming     | Beam Index      | Mandatory | Y              |
|                 | based           |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | Real-time BF    | Mandatory | Y              |
|                 | Weights         |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | Real-Time       |           | N              |
|                 | Beamforming     |           |                |
|                 | Attributes      |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | UE Channel Info |           | N              |
+-----------------+-----------------+-----------+----------------+
| Bandwidth       | Programmable    | Mandatory | Y              |
| Saving          | static-bit-width|           |                |
|                 | Fixed Point IQ  |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | Real-time       |           | Y              |
|                 | variable-bit    |           |                |
|                 | -width          |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | Compressed IQ   |           | Y              |
+-----------------+-----------------+-----------+----------------+
|                 | Block floating  |           | Y              |
|                 | point           |           |                |
|                 | compression     |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | Block scaling   |           | N              |
|                 | compression     |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | u-law           |           | N              |
|                 | compression     |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | modulation      |           | N              |
|                 | compression     |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | beamspace       |           | N              |
|                 | compression     |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | Variable Bit    |           | Y              |
|                 | Width per       |           |                |
|                 | Channel (per    |           |                |
|                 | data section)   |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | Static          |           | N              |
|                 | configuration   |           |                |
|                 | of U-Plane IQ   |           |                |
|                 | format and      |           |                |
|                 | compression     |           |                |
|                 | header          |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | Use of “symInc” |           | N              |
|                 | flag to allow   |           |                |
|                 | multiple        |           |                |
|                 | symbols in a    |           |                |
|                 | C-Plane section |           |                |
+-----------------+-----------------+-----------+----------------+
| Energy Saving   | Transmission    |           | N              |
|                 | blanking        |           |                |
+-----------------+-----------------+-----------+----------------+
| O-DU - RU       | Pre-configured  | Mandatory | Y              |
| Timing          | Transport Delay |           |                |
|                 | Method          |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | Measured        |           | N              |
|                 | Transport       |           |                |
|                 | Method (eCPRI   |           |                |
|                 | Msg 5)          |           |                |
+-----------------+-----------------+-----------+----------------+
| Synchronization | G.8275.1        | Mandatory | Y     (C3 only)|
|                 |                 |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | G.8275.2        |           | N              |
+-----------------+-----------------+-----------+----------------+
|                 | GNSS based sync |           | N              |
+-----------------+-----------------+-----------+----------------+
|                 | SyncE           |           | N              |
+-----------------+-----------------+-----------+----------------+
| Transport       | L2 : Ethernet   | Mandatory | Y              |
| Features        |                 |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | L3 : IPv4, IPv6 |           | N              |
|                 | (CUS Plane)     |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | QoS over        | Mandatory | N              |
|                 | Fronthaul       |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | Prioritization  |           | N              |
|                 | of different    |           |                |
|                 | U-plane traffic |           |                |
|                 | types           |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | Support of      |           | N              |
|                 | Jumbo Ethernet  |           |                |
|                 | frames          |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | eCPRI           | Mandatory | Y              |
+-----------------+-----------------+-----------+----------------+
|                 | support of      |           | N              |
|                 | eCPRI           |           |                |
|                 | concatenation   |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | IEEE 1914.3     |           | N              |
+-----------------+-----------------+-----------+----------------+
|                 | Application     | Mandatory | Y              |
|                 | fragmentation   |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | Transport       |           | N              |
|                 | fragmentation   |           |                |
+-----------------+-----------------+-----------+----------------+
| Other           | LAA LBT O-DU    |           | N              |
|                 | Congestion      |           |                |
|                 | Window mgmt     |           |                |
+-----------------+-----------------+-----------+----------------+
|                 | LAA LBT RU      |           | N              |
|                 | Congestion      |           |                |
|                 | Window mgmt     |           |                |
+-----------------+-----------------+-----------+----------------+

Details on the subset of xRAN functionality implemented are shown in
Table 8.

Level of Validation Specified as:


-  C: Completed code implementation for xRAN Library

-  I: Integrated into Intel FlexRAN PHY

-  T: Tested end to end with O-RU

Table 8. Levels of Validation

+------------+------------+------------+------------+-----+-----+---+
| Category   | Item       | Q4 (20.04) |            |     |     |   |
+============+============+============+============+=====+=====+===+
|            |            | Status     | C          | I   | T   |   |
+------------+------------+------------+------------+-----+-----+---+
| General    | Radio      | NR         | N/A        | N/A | N/A |   |
|            | access     |            |            |     |     |   |
|            | technology |            |            |     |     |   |
|            | (LTE / NR) |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Nominal    | 15         | Y          | Y   | N   |   |
|            | s\         | /30/120KHz |            |     |     |   |
|            | ub-carrier |            |            |     |     |   |
|            | spacing    |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | FFT size   | 512/1024   | Y          | Y   | N   |   |
|            |            | /2048/4096 |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Channel    | 5/10       | Y          | Y   | N   |   |
|            | bandwidth  | /20/100Mhz |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Number of  | 12         | Y          | Y   | N   |   |
|            | the        |            |            |     |     |   |
|            | channel    |            |            |     |     |   |
|            | (Component |            |            |     |     |   |
|            | Carrier)   |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | RU         | A          | Y          | Y   | N   |   |
|            | category   |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | TDD Config | Supporte\  | Y          | Y   | N   |   |
|            |            | d/Flexible |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | FDD        | Supported  | Y          | Y   | N   |   |
|            | Support    |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Tx/Rx      | Supported  | Y          | Y   | N   |   |
|            | switching  |            |            |     |     |   |
|            | based on   |            |            |     |     |   |
|            | 'data      |            |            |     |     |   |
|            | Direction' |            |            |     |     |   |
|            | field of   |            |            |     |     |   |
|            | C-plane    |            |            |     |     |   |
|            | message    |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | IP version | N/A        | N/A        | N/A | N/A |   |
|            | for        |            |            |     |     |   |
|            | Management |            |            |     |     |   |
|            | traffic at |            |            |     |     |   |
|            | fronthaul  |            |            |     |     |   |
|            | network    |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
| PRACH      | One Type 3 | Supported  | Y          | Y   | N   |   |
|            | message    |            |            |     |     |   |
|            | for all    |            |            |     |     |   |
|            | repeated   |            |            |     |     |   |
|            | PRACH      |            |            |     |     |   |
|            | preambles  |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Type 3     | 1          | Y          | Y   | N   |   |
|            | message    |            |            |     |     |   |
|            | per        |            |            |     |     |   |
|            | repeated   |            |            |     |     |   |
|            | PRACH      |            |            |     |     |   |
|            | preambles  |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | timeOffset | Supported  | Y          | Y   | N   |   |
|            | including  |            |            |     |     |   |
|            | cpLength   |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Supported  | Supported  | Y          | Y   | N   |   |
+------------+------------+------------+------------+-----+-----+---+
|            | PRACH      | Supported  | Y          | Y   | N   |   |
|            | preamble   |            |            |     |     |   |
|            | format /   |            |            |     |     |   |
|            | index      |            |            |     |     |   |
|            | number     |            |            |     |     |   |
|            | (number of |            |            |     |     |   |
|            | the        |            |            |     |     |   |
|            | occasion)  |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
| Delay      | Network    | Supported  | Y          | Y   | N   |   |
| management | delay      |            |            |     |     |   |
|            | det\       |            |            |     |     |   |
|            | ermination |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | lls-CU     | Supported  | Y          | Y   | N   |   |
|            | timing     |            |            |     |     |   |
|            | advance    |            |            |     |     |   |
|            | type       |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Non-delay  | Not        | N          | N   | N   |   |
|            | managed    | supported  |            |     |     |   |
|            | U-plane    |            |            |     |     |   |
|            | traffic    |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
| C/U-plane  | Transport  | Ethernet   | Y          | Y   | N   |   |
| Transport  | enc\       |            |            |     |     |   |
|            | apsulation |            |            |     |     |   |
|            | (Ethernet  |            |            |     |     |   |
|            | / IP)      |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Jumbo      | Supported  | Y          | Y   | N   |   |
|            | frames     |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Transport  | eCPRI      | Y          | Y   | N   |   |
|            | header     |            |            |     |     |   |
|            | (eCPRI /   |            |            |     |     |   |
|            | RoE)       |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | IP version | N/A        | N/A        | N/A | N/A |   |
|            | when       |            |            |     |     |   |
|            | Transport  |            |            |     |     |   |
|            | header is  |            |            |     |     |   |
|            | IP/UDP     |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | eCPRI      | Not        | N          | N   | N   |   |
|            | Con\       | supported  |            |     |     |   |
|            | catenation |            |            |     |     |   |
|            | when       |            |            |     |     |   |
|            | Transport  |            |            |     |     |   |
|            | header is  |            |            |     |     |   |
|            | eCPRI      |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | eAxC ID    | 4 \*       | Y          | Y   | N   |   |
|            | CU_Port_ID |            |            |     |     |   |
|            | bitwidth   |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | eAxC ID    | 4 \*       | Y          | Y   | N   |   |
|            | Ban\       |            |            |     |     |   |
|            | dSector_ID |            |            |     |     |   |
|            | bitwidth   |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | eAxC ID    | 4 \*       | Y          | Y   | N   |   |
|            | CC_ID      |            |            |     |     |   |
|            | bitwidth   |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | eAxC ID    | 4 \*       | Y          | Y   | N   |   |
|            | RU_Port_ID |            |            |     |     |   |
|            | bitwidth   |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Fra\       | Supported  | Y          | Y   | N   |   |
|            | gmentation |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Transport  | N/A        | N          | N   | N   |   |
|            | prio\      |            |            |     |     |   |
|            | ritization |            |            |     |     |   |
|            | within     |            |            |     |     |   |
|            | U-plane    |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Separation | Supported  | Y          | Y   | N   |   |
|            | of         |            |            |     |     |   |
|            | C/U-plane  |            |            |     |     |   |
|            | and        |            |            |     |     |   |
|            | M-plane    |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Separation | VLAN ID    | Y          | Y   | N   |   |
|            | of C-plane |            |            |     |     |   |
|            | and        |            |            |     |     |   |
|            | U-plane    |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Max Number | 16         | Y          | Y   | N   |   |
|            | of VLAN    |            |            |     |     |   |
|            | per        |            |            |     |     |   |
|            | physical   |            |            |     |     |   |
|            | port       |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
| Reception  | Rx_on_time | Supported  | Y          | Y   | N   |   |
| Window     |            |            |            |     |     |   |
| Monitoring |            |            |            |     |     |   |
| (Counters) |            |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Rx_early   | Supported  | N          | N   | N   |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Rx_late    | Supported  | N          | N   | N   |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Rx_corrupt | Supported  | N          | N   | N   |   |
+------------+------------+------------+------------+-----+-----+---+
|            | R\         | Supported  | N          | N   | N   |   |
|            | x_pkt_dupl |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Total      | Supported  | Y          | N   | N   |   |
|            | _msgs_rcvd |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
| B\         | RU         | Index and  | Y          | Y   | N   |   |
| eamforming | b\         | weights    |            |     |     |   |
|            | eamforming |            |            |     |     |   |
|            | type       |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | B\         | C-plane    | Y          | N   | N   |   |
|            | eamforming |            |            |     |     |   |
|            | control    |            |            |     |     |   |
|            | method     |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Number of  | No-re      | Y          | Y   | N   |   |
|            | beams      | strictions |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
| IQ         | U-plane    | Supported  | Y          | Y   | Y   |   |
| c\         | data       |            |            |     |     |   |
| ompression | c\         |            |            |     |     |   |
|            | ompression |            |            |     |     |   |
|            | method     |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | U-plane    | BFP:       | Y          | Y   | Y   |   |
|            | data IQ    | 8,9,12,14  |            |     |     |   |
|            | bitwidth   | bits       |            |     |     |   |
|            | (Before /  |            |            |     |     |   |
|            | After      |            |            |     |     |   |
|            | co         |            |            |     |     |   |
|            | mpression) |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Static     | Supported  | N          | N   | N   |   |
|            | con\       |            |            |     |     |   |
|            | figuration |            |            |     |     |   |
|            | of U-plane |            |            |     |     |   |
|            | IQ format  |            |            |     |     |   |
|            | and        |            |            |     |     |   |
|            | c\         |            |            |     |     |   |
|            | ompression |            |            |     |     |   |
|            | header     |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
| eCPRI      | ec\        | 001b       | Y          | Y   | Y   |   |
| Header     | priVersion |            |            |     |     |   |
| Format     |            |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | ecp\       | Supported  | Y          | Y   | Y   |   |
|            | riReserved |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | ecpriCon\  | Not        | N          | N   | N   |   |
|            | catenation | supported  |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | ec\        | U-plane    | Supported  | Y   | Y   | Y |
|            | priMessage |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | C-plane    | Supported  | Y   | Y   | Y |
+------------+------------+------------+------------+-----+-----+---+
|            |            | Delay      | Not        | N   | N   | N |
|            |            | m\         | supported  |     |     |   |
|            |            | easurement |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | ec\        | Supported  | Y          | Y   | Y   |   |
|            | priPayload |            |            |     |     |   |
|            | (payload   |            |            |     |     |   |
|            | size in    |            |            |     |     |   |
|            | bytes)     |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | ecpriRtcid | Supported  | Y          | Y   | Y   |   |
|            | /ecpriPcid |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | e\         | Supported  | Y          | Y   | Y   |   |
|            | cpriSeqid: |            |            |     |     |   |
|            | Sequence   |            |            |     |     |   |
|            | ID         |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | e\         | Supported  | Y          | Y   | Y   |   |
|            | cpriSeqid: |            |            |     |     |   |
|            | E bit      |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | e\         | Not        | N          | N   | N   |   |
|            | cpriSeqid: | supported  |            |     |     |   |
|            | S\         |            |            |     |     |   |
|            | ubsequence |            |            |     |     |   |
|            | ID         |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
| C-plane    | Section    | Not        | N          | N   | N   |   |
| Type       | Type 0     | supported  |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Section    | Supported  | Y          | Y   | Y   |   |
|            | Type 1     |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Section    | Supported  | Y          | Y   | Y   |   |
|            | Type 3     |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Section    | Not        | N          | N   | N   |   |
|            | Type 5     | supported  |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Section    | Not        | N          | N   | N   |   |
|            | Type 6     | supported  |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Section    | Not        | N          | N   | N   |   |
|            | Type 7     | supported  |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
| C-plane    | *Coding of | dat\       | Supported  | Y   | Y   | N |
| Packet     | I\         | aDirection |            |     |     |   |
| Format     | nformation | (data      |            |     |     |   |
|            | Elements – | direction  |            |     |     |   |
|            | A\         | (gNB       |            |     |     |   |
|            | pplication | Tx/Rx))    |            |     |     |   |
|            | Layer,     |            |            |     |     |   |
|            | Common*    |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | payl\      | 001b       | Y   | Y   | N |
|            |            | oadVersion |            |     |     |   |
|            |            | (payload   |            |     |     |   |
|            |            | version)   |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | f\         | Supported  | Y   | Y   | N |
|            |            | ilterIndex |            |     |     |   |
|            |            | (filter    |            |     |     |   |
|            |            | index)     |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | frameId    | Supported  | Y   | Y   | N |
|            |            | (frame     |            |     |     |   |
|            |            | i\         |            |     |     |   |
|            |            | dentifier) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | subframeId | Supported  | Y   | Y   | N |
|            |            | (subframe  |            |     |     |   |
|            |            | i\         |            |     |     |   |
|            |            | dentifier) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | slotId     | Supported  | Y   | Y   | N |
|            |            | (slot      |            |     |     |   |
|            |            | i\         |            |     |     |   |
|            |            | dentifier) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | sta\       | Supported  | Y   | Y   | N |
|            |            | rtSymbolid |            |     |     |   |
|            |            | (start     |            |     |     |   |
|            |            | symbol     |            |     |     |   |
|            |            | i\         |            |     |     |   |
|            |            | dentifier) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | number     | up to the  | Y   | Y   | N |
|            |            | Ofsections | maximum    |     |     |   |
|            |            | (number of | number of  |     |     |   |
|            |            | sections)  | PRBs       |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | s\         | 1 and 3    | Y   | Y   | N |
|            |            | ectionType |            |     |     |   |
|            |            | (section   |            |     |     |   |
|            |            | type)      |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | udCompHdr  | Supported  | Y   | Y   | N |
|            |            | (user data |            |     |     |   |
|            |            | c\         |            |     |     |   |
|            |            | ompression |            |     |     |   |
|            |            | header)    |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | n\         | Not        | N   | N   | N |
|            |            | umberOfUEs | supported  |     |     |   |
|            |            | (number Of |            |     |     |   |
|            |            | UEs)       |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | timeOffset | Supported  | Y   | Y   | N |
|            |            | (time      |            |     |     |   |
|            |            | offset)    |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | fram\      | mu=0,1,3   | Y   | Y   | N |
|            |            | eStructure |            |     |     |   |
|            |            | (frame     |            |     |     |   |
|            |            | structure) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | cpLength   | Supported  | Y   | Y   | N |
|            |            | (cyclic    |            |     |     |   |
|            |            | prefix     |            |     |     |   |
|            |            | length)    |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | *Coding of | sectionId  | Supported  | Y   | Y   | N |
|            | I\         | (section   |            |     |     |   |
|            | nformation | i\         |            |     |     |   |
|            | Elements – | dentifier) |            |     |     |   |
|            | A\         |            |            |     |     |   |
|            | pplication |            |            |     |     |   |
|            | Layer,     |            |            |     |     |   |
|            | Sections*  |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | rb         | 0          | Y   | Y   | N |
|            |            | (resource  |            |     |     |   |
|            |            | block      |            |     |     |   |
|            |            | indicator) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | symInc     | 0 or 1     | Y   | Y   | N |
|            |            | (symbol    |            |     |     |   |
|            |            | number     |            |     |     |   |
|            |            | increment  |            |     |     |   |
|            |            | command)   |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | startPrbc  | Supported  | Y   | Y   | N |
|            |            | (starting  |            |     |     |   |
|            |            | PRB of     |            |     |     |   |
|            |            | control    |            |     |     |   |
|            |            | section)   |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | reMask     | Supported  | Y   | Y   | N |
|            |            | (resource  |            |     |     |   |
|            |            | element    |            |     |     |   |
|            |            | mask)      |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | numPrbc    | Supported  | Y   | Y   | N |
|            |            | (number of |            |     |     |   |
|            |            | contiguous |            |     |     |   |
|            |            | PRBs per   |            |     |     |   |
|            |            | control    |            |     |     |   |
|            |            | section)   |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | numSymbol  | Supported  | Y   | Y   | N |
|            |            | (number of |            |     |     |   |
|            |            | symbols)   |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | ef         | Supported  | Y   | Y   | N |
|            |            | (extension |            |     |     |   |
|            |            | flag)      |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | beamId     | Support    | Y   | Y   | N |
|            |            | (beam      |            |     |     |   |
|            |            | i\         |            |     |     |   |
|            |            | dentifier) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | ueId (UE   | Not        | N   | N   | N |
|            |            | i\         | supported  |     |     |   |
|            |            | dentifier) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | freqOffset | Supported  | Y   | Y   | N |
|            |            | (frequency |            |     |     |   |
|            |            | offset)    |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | regulariza\| Not        | N   | N   | N |
|            |            | tionFactor | supported  |     |     |   |
|            |            | (regu\     |            |     |     |   |
|            |            | larization |            |     |     |   |
|            |            | Factor)    |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | ciIsample, | Not        | N   | N   | N |
|            |            | ciQsample  | supported  |     |     |   |
|            |            | (channel   |            |     |     |   |
|            |            | i\         |            |     |     |   |
|            |            | nformation |            |     |     |   |
|            |            | I and Q    |            |     |     |   |
|            |            | values)    |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | laaMsgType | Not        | N   | N   | N |
|            |            | (LAA       | supported  |     |     |   |
|            |            | message    |            |     |     |   |
|            |            | type)      |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | laaMsgLen  | Not        | N   | N   | N |
|            |            | (LAA       | supported  |     |     |   |
|            |            | message    |            |     |     |   |
|            |            | length)    |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | lbtHandle  | Not        | N   | N   | N |
|            |            |            | supported  |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | lbtD\      | Not        | N   | N   | N |
|            |            | eferFactor | supported  |     |     |   |
|            |            | (listen-b  |            |     |     |   |
|            |            | efore-talk |            |     |     |   |
|            |            | defer      |            |     |     |   |
|            |            | factor)    |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | lbtBack    | Not        | N   | N   | N |
|            |            | offCounter | supported  |     |     |   |
|            |            | (listen-b\ |            |     |     |   |
|            |            | efore-talk |            |     |     |   |
|            |            | backoff    |            |     |     |   |
|            |            | counter)   |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | lbtOffset  | Not        | N   | N   | N |
|            |            | (listen-b\ | supported  |     |     |   |
|            |            | efore-talk |            |     |     |   |
|            |            | offset)    |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | MCOT       | Not        | N   | N   | N |
|            |            | (maximum   | supported  |     |     |   |
|            |            | channel    |            |     |     |   |
|            |            | occupancy  |            |     |     |   |
|            |            | time)      |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | lbtMode    | Not        | N   | N   | N |
|            |            | (LBT Mode) | supported  |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | l\         | Not        | N   | N   | N |
|            |            | btPdschRes | supported  |     |     |   |
|            |            | (LBT PDSCH |            |     |     |   |
|            |            | Result)    |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | sfStatus   | Not        | N   | N   | N |
|            |            | (subframe  | supported  |     |     |   |
|            |            | status)    |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | lbtDrsRes  | Not        | N   | N   | N |
|            |            | (LBT DRS   | supported  |     |     |   |
|            |            | Result)    |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | initia\    | Not        | N   | N   | N |
|            |            | lPartialSF | supported  |     |     |   |
|            |            | (Initial   |            |     |     |   |
|            |            | partial    |            |     |     |   |
|            |            | SF)        |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | lbtBufErr  | Not        | N   | N   | N |
|            |            | (LBT       | supported  |     |     |   |
|            |            | Buffer     |            |     |     |   |
|            |            | Error)     |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | sfnSf      | Not        | N   | N   | N |
|            |            | (SFN/SF    | supported  |     |     |   |
|            |            | End)       |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | lbt        | Not        | N   | N   | N |
|            |            | CWConfig_H | supported  |     |     |   |
|            |            | (HARQ      |            |     |     |   |
|            |            | Parameters |            |     |     |   |
|            |            | for        |            |     |     |   |
|            |            | Congestion |            |     |     |   |
|            |            | Window     |            |     |     |   |
|            |            | m          |            |     |     |   |
|            |            | anagement) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | lbt        | Not        | N   | N   | N |
|            |            | CWConfig_T | supported  |     |     |   |
|            |            | (TB        |            |     |     |   |
|            |            | Parameters |            |     |     |   |
|            |            | for        |            |     |     |   |
|            |            | Congestion |            |     |     |   |
|            |            | Window     |            |     |     |   |
|            |            | m          |            |     |     |   |
|            |            | anagement) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | lbtTr\     | Not        | N   | N   | N |
|            |            | afficClass | supported  |     |     |   |
|            |            | (Traffic   |            |     |     |   |
|            |            | class      |            |     |     |   |
|            |            | priority   |            |     |     |   |
|            |            | for        |            |     |     |   |
|            |            | Congestion |            |     |     |   |
|            |            | Window     |            |     |     |   |
|            |            | m          |            |     |     |   |
|            |            | anagement) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | lbtCWR_Rst | Not        | N   | N   | N |
|            |            | (No        | supported  |     |     |   |
|            |            | tification |            |     |     |   |
|            |            | about      |            |     |     |   |
|            |            | packet     |            |     |     |   |
|            |            | reception  |            |     |     |   |
|            |            | successful |            |     |     |   |
|            |            | or not)    |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | reserved   | 0          | N   | N   | N |
|            |            | (reserved  |            |     |     |   |
|            |            | for future |            |     |     |   |
|            |            | use)       |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | *Section   |            |     |     |   |
|            |            | Extension  |            |     |     |   |
|            |            | Commands*  |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | extType    | Supported  | Y   | Y   | N |
|            |            | (extension |            |     |     |   |
|            |            | type)      |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | ef         | Supported  | Y   | Y   | N |
|            |            | (extension |            |     |     |   |
|            |            | flag)      |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | extLen     | Supported  | Y   | Y   | N |
|            |            | (extension |            |     |     |   |
|            |            | length)    |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Coding of  |            |            |     |     |   |
|            | I\         |            |            |     |     |   |
|            | nformation |            |            |     |     |   |
|            | Elements – |            |            |     |     |   |
|            | A\         |            |            |     |     |   |
|            | pplication |            |            |     |     |   |
|            | Layer,     |            |            |     |     |   |
|            | Section    |            |            |     |     |   |
|            | E\         |            |            |     |     |   |
|            | xtensions  |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | *ExtType=1:| bfwCompHdr | Supported  | Y   | Y   | N |
|            | B\         | (beam\     |            |     |     |   |
|            | eamforming | forming    |            |     |     |   |
|            | Weights    | weight     |            |     |     |   |
|            | Extension  | c\         |            |     |     |   |
|            | Type*      | ompression |            |     |     |   |
|            |            | header)    |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | bf         | Supported  | Y   | Y   | N |
|            |            | wCompParam |            |     |     |   |
|            |            | (b\        |            |     |     |   |
|            |            | eamforming |            |     |     |   |
|            |            | weight     |            |     |     |   |
|            |            | c\         |            |     |     |   |
|            |            | ompression |            |     |     |   |
|            |            | parameter) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | bfwl       | Supported  | Y   | Y   | N |
|            |            | (b\        |            |     |     |   |
|            |            | eamforming |            |     |     |   |
|            |            | weight     |            |     |     |   |
|            |            | in-phase   |            |     |     |   |
|            |            | value)     |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | bfwQ       | Supported  | Y   | Y   | N |
|            |            | (b\        |            |     |     |   |
|            |            | eamforming |            |     |     |   |
|            |            | weight     |            |     |     |   |
|            |            | quadrature |            |     |     |   |
|            |            | value)     |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | bfaCompHdr | Not        | Y   | N   | N |
|            | *ExtType=2:| (b\        | supported  |     |     |   |
|            | B\         | eamforming |            |     |     |   |
|            | eamforming | attributes |            |     |     |   |
|            | Attributes | c\         |            |     |     |   |
|            | Extension  | ompression |            |     |     |   |
|            | Type*      | header)    |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | bfAzPt     | Not        | Y   | N   | N |
|            |            | (b\        | supported  |     |     |   |
|            |            | eamforming |            |     |     |   |
|            |            | azimuth    |            |     |     |   |
|            |            | pointing   |            |     |     |   |
|            |            | parameter) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | bfZePt     | Not        | Y   | N   | N |
|            |            | (b\        | supported  |     |     |   |
|            |            | eamforming |            |     |     |   |
|            |            | zenith     |            |     |     |   |
|            |            | pointing   |            |     |     |   |
|            |            | parameter) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | bfAz3dd    | Not        | Y   | N   | N |
|            |            | (b         | supported  |     |     |   |
|            |            | eamforming |            |     |     |   |
|            |            | azimuth    |            |     |     |   |
|            |            | beamwidth  |            |     |     |   |
|            |            | parameter) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | bfZe3dd    | Not        | Y   | N   | N |
|            |            | (b\        | supported  |     |     |   |
|            |            | eamforming |            |     |     |   |
|            |            | zenith     |            |     |     |   |
|            |            | beamwidth  |            |     |     |   |
|            |            | parameter) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | bfAzSl     | Not        | Y   | N   | N |
|            |            | (b\        | supported  |     |     |   |
|            |            | eamforming |            |     |     |   |
|            |            | azimuth    |            |     |     |   |
|            |            | sidelobe   |            |     |     |   |
|            |            | parameter) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | bfZeSl     | Not        | Y   | N   | N |
|            |            | (b\        | supported  |     |     |   |
|            |            | eamforming |            |     |     |   |
|            |            | zenith     |            |     |     |   |
|            |            | sidelobe   |            |     |     |   |
|            |            | parameter) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | ze\        | Not        | Y   | N   | N |
|            |            | ro-padding | supported  |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | cod        | Not        | N   | N   | N |
|            | *ExtType=3:| ebookIndex | supported  |     |     |   |
|            | DL         | (precoder  |            |     |     |   |
|            | Precoding  | codebook   |            |     |     |   |
|            | Extension  | used for   |            |     |     |   |
|            | Type*      | tra        |            |     |     |   |
|            |            | nsmission) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | layerID    | Not        | N   | N   | N |
|            |            | (Layer ID  | supported  |     |     |   |
|            |            | for DL     |            |     |     |   |
|            |            | tra\       |            |     |     |   |
|            |            | nsmission) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | txScheme   | Not        | N   | N   | N |
|            |            | (tr        | supported  |     |     |   |
|            |            | ansmission |            |     |     |   |
|            |            | scheme)    |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | numLayers  | Not        | N   | N   | N |
|            |            | (number of | supported  |     |     |   |
|            |            | layers     |            |     |     |   |
|            |            | used for   |            |     |     |   |
|            |            | DL         |            |     |     |   |
|            |            | tra\       |            |     |     |   |
|            |            | nsmission) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | crsReMask  | Not        | N   | N   | N |
|            |            | (CRS       | supported  |     |     |   |
|            |            | resource   |            |     |     |   |
|            |            | element    |            |     |     |   |
|            |            | mask)      |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | c          | Not        | N   | N   | N |
|            |            | rsSyumINum | supported  |     |     |   |
|            |            | (CRS       |            |     |     |   |
|            |            | symbol     |            |     |     |   |
|            |            | number     |            |     |     |   |
|            |            | i\         |            |     |     |   |
|            |            | ndication) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | crsShift   | Not        | N   | N   | N |
|            |            | (crsShift  | supported  |     |     |   |
|            |            | used for   |            |     |     |   |
|            |            | DL         |            |     |     |   |
|            |            | tra\       |            |     |     |   |
|            |            | nsmission) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | beamIdAP1  | Not        | N   | N   | N |
|            |            | (beam id   | supported  |     |     |   |
|            |            | to be used |            |     |     |   |
|            |            | for        |            |     |     |   |
|            |            | antenna    |            |     |     |   |
|            |            | port 1)    |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | beamIdAP2  | Not        | N   | N   | N |
|            |            | (beam id   | supported  |     |     |   |
|            |            | to be used |            |     |     |   |
|            |            | for        |            |     |     |   |
|            |            | antenna    |            |     |     |   |
|            |            | port 2)    |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | beamIdAP3  | Not        | N   | N   | N |
|            |            | (beam id   | supported  |     |     |   |
|            |            | to be used |            |     |     |   |
|            |            | for        |            |     |     |   |
|            |            | antenna    |            |     |     |   |
|            |            | port 3)    |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | csf        | Not        | Y   | N   | N |
|            | *ExtType=4:| (con\      | supported  |     |     |   |
|            | Modulation | stellation |            |     |     |   |
|            | C\         | shift      |            |     |     |   |
|            | ompression | flag)      |            |     |     |   |
|            | Parameters |            |            |     |     |   |
|            | Extension  |            |            |     |     |   |
|            | Type*      |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | mod        | Not        | Y   | N   | N |
|            |            | CompScaler | supported  |     |     |   |
|            |            | (          |            |     |     |   |
|            |            | modulation |            |     |     |   |
|            |            | c\         |            |     |     |   |
|            |            | ompression |            |     |     |   |
|            |            | scaler     |            |     |     |   |
|            |            | value)     |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | mcS\       | Not        | Y   | N   | N |
|            | *ExtType=5:| caleReMask | supported  |     |     |   |
|            | Modulation | (          |            |     |     |   |
|            | C\         | modulation |            |     |     |   |
|            | ompression | c\         |            |     |     |   |
|            | Additional | ompression |            |     |     |   |
|            | Parameters | power      |            |     |     |   |
|            | Extension  | scale RE   |            |     |     |   |
|            | Type*      | mask)      |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | csf        | Not        | Y   | N   | N |
|            |            | (con\      | supported  |     |     |   |
|            |            | stellation |            |     |     |   |
|            |            | shift      |            |     |     |   |
|            |            | flag)      |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            |            | mcS        | Not        | Y   | N   | N |
|            |            | caleOffset | supported  |     |     |   |
|            |            | (scaling   |            |     |     |   |
|            |            | value for  |            |     |     |   |
|            |            | modulation |            |     |     |   |
|            |            | co\        |            |     |     |   |
|            |            | mpression) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
| U-plane    | dat        | Supported  | Y          | Y   | Y   |   |
| Packet     | aDirection |            |            |     |     |   |
| Format     | (data      |            |            |     |     |   |
|            | direction  |            |            |     |     |   |
|            | (gNB       |            |            |     |     |   |
|            | Tx/Rx))    |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | payl\      | 001b       | Y          | Y   | Y   |   |
|            | oadVersion |            |            |     |     |   |
|            | (payload   |            |            |     |     |   |
|            | version)   |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | f\         | Supported  | Y          | Y   | Y   |   |
|            | ilterIndex |            |            |     |     |   |
|            | (filter    |            |            |     |     |   |
|            | index)     |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | frameId    | Supported  | Y          | Y   | Y   |   |
|            | (frame     |            |            |     |     |   |
|            | i\         |            |            |     |     |   |
|            | dentifier) |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | subframeId | Supported  | Y          | Y   | Y   |   |
|            | (subframe  |            |            |     |     |   |
|            | i\         |            |            |     |     |   |
|            | dentifier) |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | slotId     | Supported  | Y          | Y   | Y   |   |
|            | (slot      |            |            |     |     |   |
|            | i          |            |            |     |     |   |
|            | dentifier) |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | symbolId   | Supported  | Y          | Y   | Y   |   |
|            | (symbol    |            |            |     |     |   |
|            | i\         |            |            |     |     |   |
|            | dentifier) |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | sectionId  | Supported  | Y          | Y   | Y   |   |
|            | (section   |            |            |     |     |   |
|            | i\         |            |            |     |     |   |
|            | dentifier) |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | rb         | 0          | Y          | Y   | Y   |   |
|            | (resource  |            |            |     |     |   |
|            | block      |            |            |     |     |   |
|            | indicator) |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | symInc     | 0          | Y          | Y   | Y   |   |
|            | (symbol    |            |            |     |     |   |
|            | number     |            |            |     |     |   |
|            | increment  |            |            |     |     |   |
|            | command)   |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | startPrbu  | Supported  | Y          | Y   | Y   |   |
|            | (s\        |            |            |     |     |   |
|            | tartingPRB |            |            |     |     |   |
|            | of user    |            |            |     |     |   |
|            | plane      |            |            |     |     |   |
|            | section)   |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | numPrbu    | Supported  | Y          | Y   | Y   |   |
|            | (number of |            |            |     |     |   |
|            | PRBs per   |            |            |     |     |   |
|            | user plane |            |            |     |     |   |
|            | section)   |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | udCompHdr  | Supported  | Y          | Y   | N   |   |
|            | (user data |            |            |     |     |   |
|            | c\         |            |            |     |     |   |
|            | ompression |            |            |     |     |   |
|            | header)    |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | reserved   | 0          | Y          | Y   | Y   |   |
|            | (reserved  |            |            |     |     |   |
|            | for future |            |            |     |     |   |
|            | use)       |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | u\         | Supported  | Y          | Y   | N   |   |
|            | dCompParam |            |            |     |     |   |
|            | (user data |            |            |     |     |   |
|            | c\         |            |            |     |     |   |
|            | ompression |            |            |     |     |   |
|            | parameter) |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | iSample    | 16         | Y          | Y   | Y   |   |
|            | (in-phase  |            |            |     |     |   |
|            | sample)    |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | qSample    | 16         | Y          | Y   | Y   |   |
|            | (          |            |            |     |     |   |
|            | quadrature |            |            |     |     |   |
|            | sample)    |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
| S-plane    | Topology   | Supported  | N          | N   | N   |   |
|            | conf\      |            |            |     |     |   |
|            | iguration: |            |            |     |     |   |
|            | C1         |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Topology   | Supported  | N          | N   | N   |   |
|            | conf\      |            |            |     |     |   |
|            | iguration: |            |            |     |     |   |
|            | C2         |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Topology   | Supported  | Y          | Y   | Y   |   |
|            | conf\      |            |            |     |     |   |
|            | iguration: |            |            |     |     |   |
|            | C3         |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | Topology   | Supported  | N          | N   | N   |   |
|            | conf\      |            |            |     |     |   |
|            | iguration: |            |            |     |     |   |
|            | C4         |            |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
|            | PTP        | Full       | Supported  | Y   | Y   | N |
|            |            | Timing     |            |     |     |   |
|            |            | Support    |            |     |     |   |
|            |            | (G.8275.1) |            |     |     |   |
+------------+------------+------------+------------+-----+-----+---+
| M-plane    |            |            | Not        | N   | N   | N |
|            |            |            | supported  |     |     |   |
+------------+------------+------------+------------+-----+-----+---+

\* The bit width of each component in eAxC ID can be configurable.

Transport Layer
---------------

ORAN Fronthaul data can be transported over Ethernet or IPv4/IPv6. In
the current implementation, the xRAN library supports only Ethernet with
VLAN.

.. image:: images/Native-Ethernet-Frame-with-VLAN.jpg
  :width: 600
  :alt: Figure 11. Native Ethernet Frame with VLAN

Figure 11. Native Ethernet Frame with VLAN


Standard DPDK routines are used to perform Transport Layer
functionality.

VLAN tag functionality is offloaded to NIC as per the configuration of
VF (refer to Appendix Appendix 1).

The transport header is defined in the ORAN Fronthaul specification
based on the eCPRI specification.

.. image:: images/eCPRI-Header-Field-Definitions.jpg
  :width: 600
  :alt: Figure 12. eCPRI Header Field Definitions

Figure 12. eCPRI Header Field Definitions

Only ECPRI_IQ_DATA = 0x00 and ECPRI_RT_CONTROL_DATA= 0x02 message types
are supported.

Handling of ecpriRtcid/ecpriPcid Bit field size is configurable and can
be defined on the initialization stage of the xRAN library.

.. image:: images/Bit-Allocations-of-ecpriRtcid-ecpriPcid.jpg
  :width: 600
  :alt: Figure 13. Bit Allocations of ecpriRtcid/ecpriPcid

Figure 13. Bit Allocations of ecpriRtcid/ecpriPcid

For ecpriSeqid only, the support for a sequence number is implemented.
The subsequent number is not supported.

U-plane
-------

The following diagrams show xRAN packet protocols’ headers and data
arrangement with and without compression support.

XRAN packet meant for traffic with compression enabled has the
Compression Header added after each Application Header. According to
ORAN Fronthaul's specification, the Compression Header is part of a
repeated Section Application Header. In the xRAN library implementation,
the header is implemented as a separate structure, following the
Application Section Header. As a result, the Compression Header is not
included in the xRAN packet, if compression is not used.

Figure 14 shows the components of an xRAN packet.

.. image:: images/xRAN-Packet-Components.jpg
  :width: 600
  :alt: Figure 14. xRAN Packet Components

Figure 14. xRAN Packet Components

Radio Application Header
~~~~~~~~~~~~~~~~~~~~~~~~

The next header is a common header used for time reference.

.. image:: images/Radio-Application-Header.jpg
  :width: 600
  :alt: Figure 15. Radio Application Header

Figure 15. Radio Application Header

The radio application header specific field values are implemented as
follows:

-  filterIndex = 0

-  frameId = [0:99]

-  subframeId = [0:9]

-  slotId = [0:7]

-  symbolId = [0:13]

Data Section Application Data Header
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Common Radio Application Header is followed by the Application
Header that is repeated for each Data Section within the eCPRI message.
The relevant section of xRAN packet is shown in color.

.. image:: images/Data-Section-Application-Data-Header.jpg
  :width: 600
  :alt: Figure 16. Data Section Application Data Header

Figure 16. Data Section Application Data Header


A single section is used per one Ethernet packet with IQ samples
startPrbu is equal to 0 and numPrbu is wqual to the number of RBs used:

-  rb field is not used (value 0).

-  symInc is not used (value 0)

Data Payload
~~~~~~~~~~~~

An xRAN packet data payload contains a number of PRBs. Each PRB is built
of 12 IQ samples. Flexible IQ bit width is supported. If compression is enabled udCompParam is included in the data payload. The data section is shown in colour. 

.. image:: images/Data-Payload.jpg
  :width: 600
  :alt: Figure 17. Data Payload

Figure 17. Data Payload

C-plane
-------

C-Plane messages are encapsulated using a two-layered header approach.
The first layer consists of an eCPRI standard header, including
corresponding fields used to indicate the message type, while the second
layer is an application layer including necessary fields for control and
synchronization. Within the application layer, a “section” defines the characteristics of U-plane data to be transferred or received from a
beam with one pattern id. In general, the transport header,application
header, and sections are all intended to be aligned on 4-byte boundaries
and are transmitted in “network byte order” meaning the most significant
byte of a multi-byte parameter is transmitted first.

Table 9 is a list of sections currently supported.

Table 9. Section Types

+--------------+--------------------------+--------------------------+
| Section Type | Target Scenario          | Remarks                  |
+--------------+--------------------------+--------------------------+
| 0            | Unused Resource Blocks   | Not supported            |
|              | or symbols in Downlink   |                          |
|              | or Uplink                |                          |
+--------------+--------------------------+--------------------------+
| 1            | Most DL/UL radio         | Supported                |
|              | channels                 |                          |
+--------------+--------------------------+--------------------------+
| 2            | reserved for future use  | N/A                      |
+--------------+--------------------------+--------------------------+
| 3            | PRACH and                | Only PRACH is supported. |
|              | mixed-numerology         | Mixed numerology is not  |
|              | channels                 | supported.               |
+--------------+--------------------------+--------------------------+
| 4            | Reserved for future use  | Not supported            |
+--------------+--------------------------+--------------------------+
| 5            | UE scheduling            | Not supported            |
|              | information (UE-ID       |                          |
|              | assignment to section)   |                          |
+--------------+--------------------------+--------------------------+
| 6            | Channel information      | Not supported            |
+--------------+--------------------------+--------------------------+
| 7            | LAA                      | Not supported            |
+--------------+--------------------------+--------------------------+
| 8-255        | Reserved for future use  | N/A                      |
+--------------+--------------------------+--------------------------+

Section extensions are not supported in this release.

The definition of the C-Plane packet can be found lib/api/xran_pkt_cp.h
and the fields are appropriately re-ordered in order to apply the
conversion of network byte order after setting values.
The comments in source code of xRAN lib can be used to see more information on 
implementation specifics of handling sections as well as particular fields. 
Additional changes may be needed on C-plane to perform IOT with O-RU depending on the scenario.

Ethernet Header
~~~~~~~~~~~~~~~

Refer to Figure 11.

eCPRI Header
~~~~~~~~~~~~

Refer to Figure 12.

This header is defined as the structure of xran_ecpri_hdr in
lib/api/xran_pkt.h.

Radio Application Common Header
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Radio Application Common Header is used for time reference. Its
structure is shown in Figure 18.

.. image:: images/Radio-Application-Common-Header.jpg
  :width: 600
  :alt: Figure 18. Radio Application Common Header

Figure 18. Radio Application Common Header

This header is defined as the structure of
xran_cp_radioapp_common_header in lib/api/xran_pkt_cp.h.

Please note that the payload version in this header is fixed to
XRAN_PAYLOAD_VER (defined as 1) in this release.

Section Type 0 Structure
~~~~~~~~~~~~~~~~~~~~~~~~

Figure 19 describes the structure of Section Type 0.

.. image:: images/Section-Type-0-Structure.jpg
  :width: 600
  :alt: Figure 19. Section Type 0 Structure

Figure 19. Section Type 0 Structure

In Figure 18 through Figure 22, the color yellow means it is a transport
header; the color pink is the radio application header; others are
repeated sections.

Section Type 1 Structure
~~~~~~~~~~~~~~~~~~~~~~~~

Figure 20 describes the structure of Section Type 1.

.. image:: images/Section-Type-1-Structure.jpg
  :width: 600
  :alt: Figure 20. Section Type 1 Structure

Figure 20. Section Type 1 Structure

Section Type 1 message has two additional parameters in addition to
radio application common header:

-  udCompHdr : defined as the structure of xran_radioapp_udComp_header

-  reserved : fixed by zero

Section type 1 is defined as the structure of xran_cp_radioapp_section1,
and this part can be repeated to have multiple sections.

Whole section type 1 message can be described in this summary:

+----------------------------------+
| xran_cp_radioapp_common_header   |
+==================================+
| xran_cp_radioapp_section1_header |
+----------------------------------+
| xran_cp_radioapp_section1        |
+----------------------------------+
| ……                               |
+----------------------------------+
| xran_cp_radioapp_section1        |
+----------------------------------+

Section Type 3 Structure
~~~~~~~~~~~~~~~~~~~~~~~~

Figure 21 describes the structure of Section Type 3.

.. image:: images/Section-Type-3-Structure.jpg
  :width: 600
  :alt: Figure 21. Section Type 3 Structure

Figure 21. Section Type 3 Structure

Section Type 3 message has below four additional parameters in addition
to radio application common header.

-  timeOffset

-  frameStructure: defined as the structure of
   xran_cp_radioapp_frameStructure

-  cpLength

-  udCompHdr: defined as the structure of xran_radioapp_udComp_header

Section Type 3 is defined as the structure of xran_cp_radioapp_section3
and this part can be repeated to have multiple sections.

Whole section type 3 message can be described in this summary:

+----------------------------------+
| xran_cp_radioapp_common_header   |
+==================================+
| xran_cp_radioapp_section3_header |
+----------------------------------+
| xran_cp_radioapp_section3        |
+----------------------------------+
| ……                               |
+----------------------------------+
| xran_cp_radioapp_section3        |
+----------------------------------+

Section Type 5 Structure
~~~~~~~~~~~~~~~~~~~~~~~~

Figure 22 describes the structure of Section Type 5.

.. image:: images/Section-Type-5-Structure.jpg
  :width: 600
  :alt: Figure 22.   Section Type 5 Structure

Figure 22.   Section Type 5 Structure


Section Type 6 Structure
~~~~~~~~~~~~~~~~~~~~~~~~

Figure 23 describes the structure of Section Type 6.

.. image:: images/Section-Type-6-Structure.jpg
  :width: 600
  :alt: Figure 23. Section Type 6 Structure

Figure 23. Section Type 6 Structure

