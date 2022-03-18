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


.. |br| raw:: html

   <br />
   
ORAN 5G FAPI TM Release Notes
=============================

Version FAPI TM oran_e_maintenance_release_v1.0, Mar 2022
---------------------------------------------------------

* Increased test coverage. Now DL, UL, FD, URLLC and Massive MIMO use cases are supported.
* Support for features not properly defined in the SCF 5G FAPI 2.0 specs has been added by
  means of vendor specific fields.
  
Version FAPI TM oran_release_bronze_v1.1, Aug 2020
------------------------------------------------------

* Increased test coverage. All supported DL, UL and FD standard MIMO cases are validated
* Support for carrier aggregation
* Support for API ordering
* Support for handling Intel proprietary PHY shutdown message in radio mode
* FAPI TM latency measurement
* Bug fixes
* Feedback provided to SCF on parameter gaps identified in SCF 5G FAPI specification dated March 2020
* This version of the 5G FAPI TM incorporates the changes that were provided to the SCF.


Version FAPI TM oran_release_bronze_v1.0, May 2020
------------------------------------------------------
* First release of the 5G FAPI TM to ORAN in support of the Bronze Release
* This version supports 5G only
* PARAM.config and PARAM.resp are not supported
* ERROR.ind relies on the L1 support for error detection as the 5G FAPI TM \
  only enforces security checks and |br|
  integrity checks to avoid DOS attacks but \
  it doesn't perform full validation of the input parameters for compliance to
  the standard
* Deviations from the March version of the SCF 5G FAPI document have been \
  implemented in order to deal with |br|
  limitations and ommisions found in the
  current SCF document, these differences are being provided to the SCF for
  the next document update. The 5G FAPI implementation is defined in the file
  fapi_interface.h
* Multi-user MIMO, Beamforming, Precoding and URLLC are not supported in the
  current implementation as they |br|
  require additional alignment between the SCF
  P19 and the ORAN
* The option for the MAC layer doing the full generation of the PBCH payload is not supported in this release and it will be added in the maintainance release cycle.