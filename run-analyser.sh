# start me from within an app directory, e.g. nginx-0.8.54/

set -e

if [ $# -lt 1 ]
then	echo "Usage: $0 <executable> [args]"
	exit 1
fi

exe=$1
cp ${exe} ${exe}.1

# Parse some additional info on the binary to be processed, and store
# it in a database for use by analysis.c
export LBR_BININFO=`pwd`/../dyninst-static/bin.info
echo "Using bininfo file $LBR_BININFO"
rm -f $LBR_BININFO
addr=`../dyninst-static/find_main.sh $exe`
if [ "$addr" != "" ]; then
    echo "addr.func.main=$addr" >> $LBR_BININFO
fi

DI=`pwd`/../Dyninst-8.2.1/install-dir
DI_OPT=../bin/di-opt
if [ ! -x $DI_OPT ]
then	echo "$DI_OPT not found. Please build and install it first."
	echo "And invoke this script from an app directory."
	exit 1
fi

sudo DYNINSTAPI_RT_LIB=$DI/lib/libdyninstAPI_RT.so LD_LIBRARY_PATH=$DI/lib:$LD_LIBRARY_PATH $DI_OPT -load=../bin/lbr_analysis_pass.di -lbr_analysis_pass -icall-map-type=at -window-size=16 -daemon -o $exe ${exe}.1
