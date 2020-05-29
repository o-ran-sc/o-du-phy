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

Sample Application
==================

.. contents::
    :depth: 3
    :local:

Figure 25 illustrates a sample xRAN application.

.. image:: images/Sample-Application.jpg
  :width: 600
  :alt: Figure 25. Sample Application

Figure 25. Sample Application

The sample application was created to execute test scenarios with
features of the xRAN library and test external API as well as timing.
The sample application is named sample-app, and depending on
configuration file settings can act as O-DU or simplified simulation of
O-RU. The first O-DU should be run on the machine that acts as O-DU and
the second as O-RU. Both machines are connected via ETH. The sample
application on both sides executes using a constant configuration |br|
according to settings in corresponding config files
(./app/usecase/mu0_10mhz/config_file_o_du.dat and |br|
./app/usecase/mu0_10mhz/config_file_o_ru.dat) and uses binary files
(ant.bin) with IQ samples as input. Multiple-use |br| 
cases for different
numerologies and different BW are available as examples. Configuration
files provide descriptions of each |br|
parameter and in general, those are
related to M-plane level settings as per the ORAN Fronthaul
specification.

From the start of the process, the application (O-DU) sends DL packets
for the U-plane and C-plane and receives U-plane UL packets.
Synchronization of O-DU and O-RU sides is achieved via IEEE 1588.

U-plane packets for UL and DL direction are constructed the same way
except for the direction field.

The several default configurations used with the sample app for v20.02
release are:

1 Cell mmWave 100MHz TDD DDDS:


-  Numerology 3 (mmWave)

-  TTI period 125 µs

-  100 Mhz Bandwidth: 792 subcarriers (all 66 RB utilized at all times)

-  4x4 MIMO

-  No beamforming

-  1 Component carrier

-  Jumbo Frame for Ethernet (up to 9728 bytes)

-  Front haul throughput ~11.5 Gbps.

12 Cells Sub6 10MHz FDD:


-  Numerology 0 (Sub-6)

-  TTI period 1000 µs

-  10Mhz Bandwidth: 624 subcarriers (all 52 RB utilized at all times)

-  4x4 MIMO

-  No beamforming

-  12 Component carrier

-  Jumbo Frame for Ethernet (up to 9728 bytes)

-  Front haul throughput ~13.7Gbps.

1 Cell Sub6 100MHz TDD:


-  Numerology 1 (Sub-6)

-  TTI period 500us

-  100Mhz Bandwidth: 3276 subcarriers (all 273RB utilized at all times)

-  4x4 MIMO

-  No beamforming

-  1 Component carrier

-  Jumbo Frame for Ethernet (up to 9728 bytes)

-  Front haul throughput ~11.7 Gbps.

.. _cell-sub6-100mhz-tdd-1:

1 Cell Sub6 100MHz TDD:


-  Numerology 1 (Sub-6)

-  TTI period 500 µs

-  100 Mhz Bandwidth: 3276 subcarriers (all 273RB utilized at all
   times). 8 UEs per TTI per layer

-  8DL /4UL MIMO Layers

-  Digital beamforming with 32T32R

-  1 Component carrier

-  Jumbo Frame for Ethernet (up to 9728 bytes)

-  Front haul throughput ~23.5 Gbps.

Other configurations can be constructed by modifying config files
(please see app/usecase/)



