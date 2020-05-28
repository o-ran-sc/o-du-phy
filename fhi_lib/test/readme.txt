/******************************************************************************
*
*   Copyright (c) 2019 Intel.
*
*   Licensed under the Apache License, Version 2.0 (the "License");
*   you may not use this file except in compliance with the License.
*   You may obtain a copy of the License at
*
*       http://www.apache.org/licenses/LICENSE-2.0
*
*   Unless required by applicable law or agreed to in writing, software
*   distributed under the License is distributed on an "AS IS" BASIS,
*   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*   See the License for the specific language governing permissions and
*   limitations under the License.
*
*******************************************************************************/


Download googletest from https://github.com/google/googletest/releases 
Untar google test. 
The recommended installation folder is /opt/gtest/

Example build and installation commands:
sudo tar -xvf googletest-release-1.7.0.tar.gz
sudo mv googletest-release-1.7.0 gtest-1.7.0
export GTEST_DIR=/opt/gtest/gtest-1.7.0
cd ${GTEST_DIR}
sudo g++ -isystem ${GTEST_DIR}/include -I${GTEST_DIR} -pthread -c ${GTEST_DIR}/src/gtest-all.cc
sudo ar -rv libgtest.a gtest-all.o
cd ${GTEST_DIR}/build-aux
sudo cmake ${GTEST_DIR}
sudo make
cd ${GTEST_DIR}
sudo ln -s build-aux/libgtest_main.a libgtest_main.a

setup GTEST_ROOT 

export GTEST_ROOT=/opt/gtest/gtest-1.7.0