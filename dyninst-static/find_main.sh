#!/bin/bash

set -e

if [ "$1" == "" ]; then
  echo "Usage: $0 <binary>" >&2
  exit 1
fi
BIN=$1

# if there are symbols available, use them
addr=`readelf --symbols $BIN | egrep "\s+main$" | awk '{ print $2 }' | head -n1`
if [ "$addr" != "" ]; then
  echo "0x$addr"
  exit 0
fi

# else, assume the first argument to __libc_start_main is the address of main
addr=`objdump --wide -d $BIN | egrep -B1 "call.+__libc_start_main" | grep -v "DYNAMIC" | egrep -o "0x[0-9a-f]+"`
if [ "$addr" != "" ]; then
  echo "$addr"
  exit 0
fi

exit 1

