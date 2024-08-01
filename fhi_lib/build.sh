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
MLOG=1
COMMAND_LINE="-j$(nproc) "
SAMPLEAPP=0
MPLANE=0
POLL_EBBU_OFFLOAD=0
CODE_COVERAGE=0

CLEANFLAG=0
REBUILD=0
buildlog_file=$PWD/build_xranlib.log

function log_print ()
{
    echo "$@"
    echo "$@" >> $buildlog_file
}

echo Number of commandline arguments: $#
echo $1
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
    FWK)
    FWK=1
    ;;
    SAMPLEAPP)
    SAMPLEAPP=1
    ;;
    MPLANE)
    MPLANE=1
    ;;
    xclean)
    COMMAND_LINE+=$key
    COMMAND_LINE+=" "
    SAMPLEAPP=1
    CLEANFLAG=1
    ;;
    POLL_EBBU_OFFLOAD)
    POLL_EBBU_OFFLOAD=1
    ;;
    clean)
    COMMAND_LINE+=$key
    COMMAND_LINE+=" "
    CLEANFLAG=1
    ;;
    cov)
    CODE_COVERAGE=1
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

if [ -z "$DIR_WIRELESS_FW" ]
then
	echo 'DIR_WIRELESS_FW folder is not set. Disable FWK (DIR_WIRELESS_FW='$DIR_WIRELESS_FW')'
	FWK=0
else
	echo 'DIR_WIRELESS_FW folder is set. Enable FWK (DIR_WIRELESS_FW='$DIR_WIRELESS_FW')'
	FWK=1
fi

ORU=1
echo 'Building xRAN Library for O-RU'
echo "LIBXRANSO    = ${LIBXRANSO}"
echo "MLOG         = ${MLOG}"
echo "FWK          = ${FWK}"
echo "ORU          = ${ORU}"
echo "CODE_COVERAGE= ${CODE_COVERAGE}"
echo "BUILD_APP    = ${SAMPLEAPP}"

cd $XRAN_FH_LIB_DIR
make $COMMAND_LINE MLOG=${MLOG} LIBXRANSO=${LIBXRANSO} ORU=${ORU} CODE_COVERAGE=${CODE_COVERAGE}

if [ "$SAMPLEAPP" -eq "1" ]
then
		echo 'Building xRAN O-RU Test Application'
		cd $XRAN_FH_APP_DIR
		make $COMMAND_LINE MLOG=${MLOG} FWK=${FWK} ORU=${ORU} CODE_COVERAGE=${CODE_COVERAGE}
else
		echo 'Not building xRAN Test Application...'
fi

ORU=0
if [ -f ${buildlog_file} ]; then
    PREVMODE=$(grep "BUILD_MODE_APP" ${buildlog_file} | awk -F= '{ print $2 }')
    if [ -z $PREVMODE ]; then
        REBUILD=1
    else
        if [ "$SAMPLEAPP" -eq "$PREVMODE" ]; then
            REBUILD=0
        else
            REBUILD=1
        fi
    fi
else
    REBUILD=1
fi

echo "" > ${buildlog_file}
log_print 'Building xRAN Library for O-DU'
log_print "LIBXRANSO            = ${LIBXRANSO}"
log_print "MLOG                 = ${MLOG}"
log_print "FWK                  = ${FWK}"
log_print "ORU                  = ${ORU}"
log_print "POLL_EBBU_OFFLOAD    = ${POLL_EBBU_OFFLOAD}"
log_print "CODE_COVERAGE        = ${CODE_COVERAGE}"
log_print "BUILD_MODE_APP       = ${SAMPLEAPP}"
log_print "REBUILD              = ${REBUILD}"

if [ "$CLEANFLAG" -eq "1" ]; then
    rm -f ${buildlog_file}
    REBUILD=0
fi

if [ "$REBUILD" -eq "1" ]; then
    echo
    echo "*** New build or previous build has different configuration! Rebuilding...."
    cd $XRAN_FH_LIB_DIR
    make "xclean" MLOG=${MLOG} LIBXRANSO=${LIBXRANSO} ORU=${ORU} SAMPLEAPP=${SAMPLEAPP} POLL_EBBU_OFFLOAD=${POLL_EBBU_OFFLOAD} CODE_COVERAGE=${CODE_COVERAGE}
fi

cd $XRAN_FH_LIB_DIR
make $COMMAND_LINE MLOG=${MLOG} LIBXRANSO=${LIBXRANSO} ORU=${ORU} SAMPLEAPP=${SAMPLEAPP} POLL_EBBU_OFFLOAD=${POLL_EBBU_OFFLOAD} CODE_COVERAGE=${CODE_COVERAGE}

if [ "$SAMPLEAPP" -eq "1" ]
then
		echo 'Building xRAN O-DU Test Application'
		cd $XRAN_FH_APP_DIR
		make $COMMAND_LINE MLOG=${MLOG} FWK=${FWK} ORU=${ORU} POLL_EBBU_OFFLOAD=${POLL_EBBU_OFFLOAD} CODE_COVERAGE=${CODE_COVERAGE}
else
		echo 'Not building xRAN Test Application...'
fi

if [ -z ${GTEST_ROOT+x} ];
then
    echo "GTEST_ROOT is not set. Unit tests are not compiled";
else
	echo 'Building xRAN Test Application ('$GTEST_ROOT')'
	cd $XRAN_FH_TEST_DIR
	make $COMMAND_LINE POLL_EBBU_OFFLOAD=${POLL_EBBU_OFFLOAD} CODE_COVERAGE=${CODE_COVERAGE}
fi

