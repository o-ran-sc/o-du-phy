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


Assumptions and Dependencies
===============================

.. contents::
    :depth: 3
    :local:

This chapter contains the assumptions, requirements and dependencies for the O-DU Low current implementation.

Assumptions
-----------

An L1 with a proprietary interface and a testmac supporting the FAPI interface are available through the Open Source Community(OSC) github in binary blob form and with the reference
files that support the tests required for the ORAN Bronze Release. The required header files that are needed to build the 5G FAPI TM and to run validation tests and to integrate with the O-DU
High to check network functionality are available from the same site.
The L1 App and Testmac repository is at https://github.com/intel/FlexRAN/

Requirements
------------
* Only Xeon® series Processor with Intel Architecture are supported and the platform should be |br|
  either Intel® Xeon® SkyLake or CascadeLake with at least 2.0 GHz core frequency.
* FPGA/ASIC card for FEC acceleration that is compliant with the BBDev framework and interface. Only needed to run high throughput cases with the HW FEC card assistance.
* Bios setting steps and options may have differences, however at least you should have the same Bios setting as decribed in the README.md file available at https://github.com/intel/FlexRAN Bios settings section.
* Running with FH requires PTP for Linux\* version 2.0 (or later) to be installed to provide IEEE 1588 synchronization.

Dependencies
------------

* Centos OS 7 (7.5+) (7.7 was used for the L1 and testmac binaries).

* RT Kernel kernel-rt-3.10.0-1062.12.1.rt56.1042

* Data Plane Development Kit (DPDK v19.11) with corresponding DPDK patch according to |br| O-RAN FH setup configuration section.

* FEC SDK lib which is needed when you run the FEC in SW mode, download through: https://software.intel.com/en-us/articles/flexran-lte-and-5g-nr-fec-software-development-kit-modules

* Intel® C++ Compiler v19.0.3 is used for test application and system integration. Free Intel® C++ Compiler can be gotten through below link with community license, however the version you get is always the latest ICC version, the verification for that version might not have been performed yet, please feedback through O-DU Low WIKI page if you meet any issues.
  https://software.intel.com/en-us/system-studio/choose-download 

* Optionally Octave v3.8.2 can be used to generate reference IQ samples (octave-3.8.2-20.el7.x86_64) for O-RAN FH Sample App. Only needed to run the sample APP for O-RAN FHI.




