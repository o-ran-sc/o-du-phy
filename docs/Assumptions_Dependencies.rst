..    Copyright (c) 2019-2022 Intel
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


Assumptions, Dependencies, and Constraints
==========================================

.. contents::
    :depth: 3
    :local:

This chapter contains limitations on the scope of the document.

Assumptions
-----------

An L1 with a proprietary interface and a testmac supporting the FAPI interface are available through the Open Source Community(OSC) github in binary blob form and with the reference
files that support the tests required for the current O-RAN Release. The required header files that are needed to build the 5G FAPI TM and to run validation tests and to integrate with the O-DU
High to check network functionality are available from the same site.
The L1 App and Testmac repository is at https://github.com/intel/FlexRAN/


Requirements
------------
* Only Xeon® series Processor with Intel Architecture are supported and the platform should be either Intel® Xeon® SkyLake or CascadeLake with at least 2.0 GHz core frequency.
* FPGA/ASIC card for FEC acceleration that is compliant with the BBDev framework and interface. Only needed to run high throughput cases with the HW FEC card assistance.
* Bios setting steps and options may have differences, however at least you should have the same Bios setting as decribed in the README.md file available at https://github.com/intel/FlexRAN Bios settings section.
* Running with FH requires PTP for Linux\* version 2.0 (or later) to be installed to provide IEEE 1588 synchronization.


Dependencies
------------

O-RAN library implementation depends on the Data Plane Development Kit
(DPDK v20.11).

DPDK v20.11 should be patched with corresponding DPDK patch provided
with FlexRAN release (see *Table 1*, FlexRAN Reference Solution Software
Release Notes)

Intel® C++ Compiler v19.0.3 is used.

-  Optionally Octave v3.8.2 can be used to generate reference IQ samples (octave-3.8.2-20.el7.x86_64).

Constraints
-----------

This release has been designed and implemented to support the following
numerologies defined in the 3GPP specifications for LTE and 5GNR (refer
to *Table 2*):

5G NR
~~~~~

Category A support:

-  Numerology 0 with bandwidth 5/10/20 MHz with up to 12 cells in 2x2 antenna configuration
-  Numerology 0 with bandwidth 40 MHz with 1 cell.

-  Numerology 1 with bandwidth 20/40 MHz with 1 cell and URLLC use cases for 40 MHz
-  Numerology 1 with bandwidth 100 MHz with up to 16 cells

-  Numerology 3 with bandwidth 100 MHz with up to 3 cells

Category B support:

Numerology 1 with bandwidth 100 MHz where the antenna panel is up to
64T64R with up to 3 cells.

LTE
~~~

Category A support:

Bandwidth 5/10/20 MHz with up to 12 cells

Category B support:

Bandwidth 5/10/20 MHz for 1 cell

The feature set of O-RAN protocol should be aligned with Radio Unit
(O-RU) implementation. Inter-operability testing (IoT) is required to
confirm the correctness of functionality on both sides. The exact
feature set supported is described in Chapter *4.0* *Transport Layer and
O-RAN Fronthaul Protocol Implementation* of this document.

-

