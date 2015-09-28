
if [ "$1" == "" ]; then
  echo "Usage: $0 <binary>"
  echo "Produces a libsyms file for a given binary"
  exit 1
fi

i=1
while read -r line
do
  lib=`echo $line | cut -d' ' -f1`
  loc=`echo $line | cut -d' ' -f3`
  
  if [ "$lib" == "linux-vdso.so.1" ]; then
    continue
  fi
  
  if [ "$loc" == "" ]; then
      loc=$lib
  fi

  if [[ ! -f $loc ]]; then
      echo "ERROR. NO LIBRARY FOUND ON LINE: $line"
      exit
  fi

  ./get_lib_rel_syms.sh $loc $i
  i=$((i+1))
done < <(ldd $1)

