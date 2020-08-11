#!/bin/bash
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

set -e
trap 'l_c=$current_command; current_command=$BASH_COMMAND' DEBUG
trap 'echo "\"${l_c}\" command exited with code $?."' EXIT

XRAN_FH_LIB_DIR=$XRAN_DIR/lib
XRAN_FH_APP_DIR=$XRAN_DIR/app
XRAN_FH_TEST_DIR=$XRAN_DIR/test/test_xran
LIBXRANSO=0
MLOG=0
COMMAND_LINE=

echo Number of commandline arguments: $#
while [[ $# -ne 0 ]]
do
key="$1"

#echo Parsing: $key
case $key in
    LIBXRANSO)
    LIBXRANSO=1
    ;;
    MLOG)
    MLOG=1
    ;;
    xclean)
    COMMAND_LINE+=$key
    COMMAND_LINE+=" "
    ;;
    clean)
    COMMAND_LINE+=$key
    COMMAND_LINE+=" "
    ;;
    *)
    echo $key is unknown command        # unknown option
    ;;
esac
shift # past argument or value
done

if [ -z "$MLOG_DIR" ]
then
	echo 'MLOG folder is not set. Disable MLOG (MLOG_DIR='$MLOG_DIR')'
	MLOG=0
else
	echo 'MLOG folder is set. Enable MLOG (MLOG_DIR='$MLOG_DIR')'
	MLOG=1
fi

echo 'Building xRAN Library'
echo "LIBXRANSO = ${LIBXRANSO}"
echo "MLOG      = ${MLOG}"

cd $XRAN_FH_LIB_DIR
make $COMMAND_LINE MLOG=${MLOG} LIBXRANSO=${LIBXRANSO} #DEBUG=1 VERBOSE=1

echo 'Building xRAN Test Application'
cd $XRAN_FH_APP_DIR
make $COMMAND_LINE MLOG=${MLOG} #DEBUG=1 VERBOSE=1

if [ -z ${GTEST_ROOT+x} ];
then
    echo "GTEST_ROOT is not set. Unit tests are not compiled";
else
	echo 'Building xRAN Test Application ('$GTEST_ROOT')'
	cd $XRAN_FH_TEST_DIR
	make $COMMAND_LINE;
fi

