#!/bin/bash
###############################################################################
#
#   Copyright (c) 2021 Intel.
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
###############################################################################

COREMASK=2
SECONDARY=1
FPREFIX="wls_test"
WLS_TEST_HELP=0

while getopts ":mhpa:w:" opt; do
  case ${opt} in
    m )
      SECONDARY=0
      ;;
    a )
      COREMASK=$((1 << $OPTARG))
      ;;
    : )
      echo "Invalid option: $OPTARG requires a core number"
      exit 1
      ;;
    w )
      #replace / with _ for dpdk file prefix
      FPREFIX=${OPTARG////_}
      ;;
    : )
      echo "Invalid option: $OPTARG requires dev wls path"
      exit 1
      ;;
    h )
      echo "invoking help"
      WLS_TEST_HELP=1
      ;;
  esac
done

wlsTestBinary="wls_test"
if [ $WLS_TEST_HELP -eq 0 ]; then
    if [ $SECONDARY -eq 0 ]; then
      wlsTestBinary="wls_test -c $COREMASK -n 4 "
        wlsTestBinary+="--file-prefix=$FPREFIX --socket-mem=3072 --"
    else
      wlsTestBinary="wls_test -c $COREMASK -n 4 "
        wlsTestBinary+="--proc-type=secondary --file-prefix=$FPREFIX --"
    fi
else
  wlsTestBinary+=" --"
fi

ulimit -c unlimited

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/..

wlsCmd="./${wlsTestBinary} $*"
echo "Running... ${wlsCmd}"

eval $wlsCmd

exit 0
