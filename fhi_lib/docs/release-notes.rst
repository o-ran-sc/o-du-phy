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

Front Haul Interface Library Release Notes
==========================================

Version ORAN-seedcode_v1.2, 1 Nov 2019
--------------------------------------
* Second version released to ORAN in support of Release A
* Incorporates support for 5G NR sub 6 and mmWave
* Support for Application fragementation under Transport features was added
* This release has been designed and implemented to support the following numerologies defined in the 3GPP specification 
*	Numerology 0 with bandwidth 5/10/20 MHz with up to 12 cells
*	Numerology 1 with bandwidth 100MHz with up to 1 cell
*	Numerology 3 with bandwidth 100MHz with up to 1 cell
* The feature set of xRAN protocol should be aligned with Radio Unit (O-RU) implementation
* Inter-operability testing (IOT) is required to confirm correctness of functionality on both sides
* The following mandatory features of the ORAN FH interface are not yet supported in this release
* RU Category  Support of CAT-B RU (i.e. precoding in RU)
* Beamforming Beam Index Based and Real Time BF weights
* Transport Features QoS over FrontHaul


Version ORAN-seedcode_v1.1, 25 Jul 2019
---------------------------------------
* This first version supports only mmWave per 5G NR and it is not yet
* optimized
* It is a first draft prior to the November 2019 Release A
* The following mandatory features of the ORAN FH interface are not yet
* supported in this initial release
* RU Category  Support of CAT-B RU (i.e. precoding in RU)
* Beamforming Beam Index Based and Real Time BF weights
* Transport Features QoS over FrontHaul and Application Fragmentation




