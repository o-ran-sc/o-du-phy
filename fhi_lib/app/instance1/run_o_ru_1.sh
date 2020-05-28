#! /bin/bash

#******************************************************************************
#
#   Copyright (c) 2019 Intel.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
#******************************************************************************/

ulimit -c unlimited
echo 1 > /proc/sys/kernel/core_uses_pid


#40G
#./build/sample-app ./usecase/mu3_100mhz/config_file_o_du.dat  0000:d8:02.0 0000:d8:02.1

#25G
../build/sample-app ../usecase/mu0_20mhz/12/config_file_o_ru_1.dat  0000:af:0a.2 0000:af:0a.3

