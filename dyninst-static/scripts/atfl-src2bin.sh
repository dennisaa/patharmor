#!/bin/bash

KEEP_TEMPS=${KEEP_TEMPS:-0}

function gen_cem_if
{
    sed "s/ = / = \*:\*:\*:I,/g" $1     
}

if [ $# -ne 3 ]; then
    echo "Usage: $0 <binary> <atfl-src.dat> <atfl-bin.dat>"
    exit 1
fi

BIN=$1
IF=$2
OF=$3

CEM_IF=$( mktemp -t atfl-cem-if.XXXXXX )

echo " * Generating temps: $CEM_IF ..." >&2

gen_cem_if $IF > $CEM_IF

echo " * Running: $( dirname $0 )/cem-src2bin.sh $BIN $CEM_IF $OF ..." >&2
$( dirname $0 )/cem-src2bin.sh $BIN $CEM_IF $OF 2>&1 | grep -v 'Warning:.*\*:\*'
sed -i "s/^0/*/g" $OF

if [ $KEEP_TEMPS -eq 0 ]; then
    echo " * Removing temps..." >&2
    rm -f $( dirname $CEM_IF )/atfl-*
fi

echo " * Done" >&2

