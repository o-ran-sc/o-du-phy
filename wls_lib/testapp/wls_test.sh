#!/bin/bash
################################################################
#
# <COPYRIGHT_TAG>
#
################################################################

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
