#!/bin/bash

function callsite2targets
{
    local callsite=$1
    local map=$2
    targets=$( grep "$callsite " $map | awk '{ print $2; }' )
    if [ "$targets" == "" ]; then
        callsite="*" #falls back to functions with address taken when mapping not found
        targets=$( grep "$callsite " $map | awk '{ print $2; }' )
    fi
    echo $targets
}

MAP_FILE="settings.llvm/icall-info/callee-edges-by-current.map"
if [ ! -f $MAP_FILE ]; then
    #
    # No map file available, assume no targets!
    #
    echo "WARNING: no map file. returning no targets." >&2
    exit 0
fi

callsite2targets $1 $MAP_FILE
