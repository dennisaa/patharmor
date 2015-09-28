#!/bin/bash

set -a 

if [ "$1" == "" ] || [ "$2" == "" ]; then
  echo "Usage: $0 <library> <bitshift>"
  echo "Produces a list of relative exported function addresses from a library"
  exit 1
fi

lib=`readlink -e $1`
bit=$2
while read -r line
do
  rel=`echo $line | awk '{ print $1 }'`
  typ=`echo $line | awk '{ print $2 }'`
  sym=`echo $line | awk '{ print $3 }' | egrep -o '^[a-zA-Z0-9_-]+'`
  if [ "$typ" != "T" ] && [ "$typ" != "W" ] && [ "$typ" != "i" ]; then
    continue
  fi
  echo "0x$rel $sym $typ $bit $lib"
done < <(nm -D $1)

