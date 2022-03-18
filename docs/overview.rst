..    Copyright (c) 2022 Intel
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

Front Haul Interface Library Overview
=====================================

The O-RAN FHI Lib is built on top of DPDK to perform U-plane and C-plane functions according to the 
ORAN Fronthaul Interface specification between O-DU and O-RU.
The S-Plane support requires PTP for Linux version 2.0 or later.
The Management plane is outside of the scope of this library implementation.


Project Resources
-----------------

The source code is available from the Linux Foundation Gerrit server:
    `<https://gerrit.o-ran-sc.org/r/gitweb?p=o-du%2Fphy.git;a=summary>`_
 
The build (CI) jobs will be in the Linux Foundation Jenkins server:
    `<https://jenkins.o-ran-sc.org>`_

Issues are tracked in the Linux Foundation Jira server:
    `<https://jira.o-ran-sc.org/secure/Dashboard.jspa>`_

Project information is available in the Linux Foundation Wiki:
    `<https://wiki.o-ran-sc.org>`_


ODULOW
------

The ODULOW uses the FHI library to access the C-Plane and U-Plane interfaces to the O-RU. 
The FHI Lib is defined to communicate TTI event, symbol time, C-plane information as well as IQ sample data.

DPDK
----

DPDK is used by the FHI Library to interface to an Ethernet Port
The FHI Library is built on top of DPDK to perform U-plane and C-plane functions per the ORAN Front Haul specifications

Linux PTP
---------
Linux PTP is used to synchronize the system timer to the GPS time
