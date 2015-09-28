# start me from within an app directory, e.g. nginx-0.8.54/

set -e

if [ $# -lt 1 ]
then	echo "Usage: $0 <executable> [args]"
	exit 1
fi

exe=$1

DI=`pwd`/../Dyninst-8.2.1/install-dir
DRLIB=`pwd`/../DynamoRIO-Linux-5.0.0-9/lib64/release

python `pwd`/../at/obj/get_objdump_func.py `pwd`/$exe
python `pwd`/../at/obj/read_section.py `pwd`/$exe > `pwd`/$exe.raw
echo -n "* " > `pwd`/settings.llvm/icall-info/callee-edges-by-at.map
DYNINSTAPI_RT_LIB=$DI/lib/libdyninstAPI_RT.so LD_LIBRARY_PATH=$DRLIB:$DI/lib:`pwd`/../dyninst-mainline/install-dir/lib:$LD_LIBRARY_PATH `pwd`/../at/obj/ATAnalysis `pwd`/$exe `pwd`/$exe.raw | cut -d':' -f2 | tr '\n' , >> `pwd`/settings.llvm/icall-info/callee-edges-by-at.map
cp `pwd`/settings.llvm/icall-info/callee-edges-by-at.map `pwd`/settings.llvm/icall-info/addr-taken-funcs.map
cp `pwd`/settings.llvm/icall-info/callee-edges-by-at.map `pwd`/settings.llvm/icall-info/callee-edges-by-current.map

