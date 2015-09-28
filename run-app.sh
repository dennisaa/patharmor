# start me from within an app directory, e.g. nginx-0.8.54/

set -e

if [ $# -lt 1 ]
then	echo "Usage: $0 <executable> [args]"
	exit 1
fi

exe=$1

DI=`pwd`/../Dyninst-8.2.1/install-dir
DI_OPT=../bin/di-opt
if [ ! -x $DI_OPT ]
then	echo "$DI_OPT not found. Please build and install it first."
	echo "And invoke this script from an app directory."
	exit 1
fi

shift

set -x
sudo LD_BIND_NOW=y LD_PRELOAD=`pwd`/../bin/libwrappers.so DYNINSTAPI_RT_LIB=$DI/lib/libdyninstAPI_RT.so LD_LIBRARY_PATH=$DI/lib:`pwd`/../dyninst-mainline/install-dir/lib:$LD_LIBRARY_PATH $DI_OPT -load=`pwd`/../bin/padyn.di -padyn -args `pwd`/$exe $*

