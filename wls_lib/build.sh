#!/bin/sh
################################################################
#
# <COPYRIGHT_TAG>
#
#################################################################
#
#  File: build.sh
#        Build script to comapile kernel module, library and test application.
#

echo "Building dpdk based wls library" 
CODE_COVERAGE=0

echo Number of commandline: $#
while [[ $# -ne 0 ]]
do
key="$1"

#echo Parsing: $key
case $key in
    clean)
    COMMAND_LINE+=$key
    COMMAND_LINE+=" "
    ;;
    --cov)
    CODE_COVERAGE=1
    ;;
    *)
    echo $key is unknown command        # unknown option
    ;;
esac
shift # past argument or value
done

echo "CODE_COVERAGE=${CODE_COVERAGE}"

make $COMMAND_LINE CODE_COVERAGE=${CODE_COVERAGE}
cd testapp
make $COMMAND_LINE CODE_COVERAGE=${CODE_COVERAGE}
