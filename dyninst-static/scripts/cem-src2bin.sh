#!/bin/bash

KEEP_TEMPS=${KEEP_TEMPS:-0}
HT=${HT:-1}

declare -A FUNC_SRC2BIN_AA
declare -A CS_SRC2BIN_AA

function gen_src2bin_ht_map
{
    local map_file=$1
    local aa=$2
    local map=$3
    while read line
    do
        src=$( echo $line | awk '{ print $1; }' | sed "s/^.*\///g" )
        bin=$( $map $src )
        if [ "$bin" != "" ]; then
            bin=0
        else
            bin=$( echo $line | awk '{ print $2; }' )
        fi
        eval $aa["$src"]="$bin"
    done < $map_file
}

function src2bin_ht_cs
{
    echo ${CS_SRC2BIN_AA["$1"]}
}

function src2bin_ht_func
{
    echo ${FUNC_SRC2BIN_AA["$1"]}
}

function src2bin_ht
{
    local src=$1
    local map=$3
    src=$( echo $src | sed "s/^.*\///g" )
    bin=$( $map $src )
    if [ "$bin" == "" ]; then
        echo " * Warning: $map[$src] failed to map to a binary value!" >&2
        bin="0"
    elif [ "$bin" == "0" ]; then
        echo " * Warning: $map[$src] mapped to multiple binary values!" >&2
        bin="0"
    else
        bin=$( printf %012lx 0x$bin )
    fi
    echo $bin
}

function src2bin
{
    local src=$1
    local map=$2
    bin=$( grep "$src " $map | awk '{ print $2; }' )
    count=$( echo "$bin" | wc -l )
    if [ "$bin" == "" ]; then
        echo " * Warning:" grep \"$src \" $map \| awk '{ print $2; }' " failed to map to a binary value!" >&2
        bin="0"
    elif [ "$count" != "1" ]; then
        echo " * Warning:" grep \"$src \" $map \| awk '{ print $2; }' " mapped to multiple binary values!" >&2
        bin="0"
    else
        bin=$( printf %012lx 0x$bin )
    fi
    echo $bin
}

function gen_function_bin_map
{
    objdump -l -d $1 | grep -A 1 -B 1 -e \(\): | sed "s/ <.*>:/():/g" | perl -p -e 's/\(\):\n/ /' | grep \.[a-zA-Z][a-zA-Z]*:[0-9] | sed "s/\.\([a-zA-Z][a-zA-Z]*\):[0-9]*/.\1/g" | awk '{ printf("%s:%s %s\n", $3, $2, $1); }'
    objdump -l -d $1 | grep "@plt>:" | sed "s/<\(.*\)@plt>:/\1/g" | awk '{ printf("__EXT__:%s %s\n", $2, $1); }'
}

function gen_callsite_bin_map
{
    objdump -l -d $1 | grep -e "\scall[a-z]*\s\s*\*" -e "\.[a-zA-Z][a-zA-Z]*:[0-9]" | grep -B 1 "\scall[a-z]*\s" | sed "s/\([0-9]*\) (.*)/\1/g" | perl -p -e 's/(\.[a-zA-Z][a-zA-Z]*:[0-9]*)\n/\1/' | sed "s/\.\([a-zA-Z][a-zA-Z]*\):\([^:]*\).*/.\1:\2/g" | grep "\.[a-zA-Z][a-zA-Z]*:[0-9]"
}

function gen_cem_src_input
{
    grep " = " $1 | awk -F "= " '{ print $2; }' | grep ":I\(,.*\)\?\$" | sed "s/:I,\?/ /g"| sed "s/^\([^:]*\):[^:]*:\([0-9]*\)/\1:\2/g"
}

if [ $# -ne 3 ]; then
    echo "Usage: $0 <binary> <cem-src.dat> <cem-bin.dat>"
    exit 1
fi

BIN=$1
IF=$2
OF=$3

FUNC_BIN_MAP=$( mktemp -t cem-func_bin_map.XXXXXX )
CS_BIN_MAP=$( mktemp -t cem-cs_bin_map.XXXXXX )
CEM_SRC_INPUT=$( mktemp -t cem-src_input.XXXXXX )

echo " * Generating temps: $FUNC_BIN_MAP $CS_BIN_MAP $CEM_SRC_INPUT ..." >&2

gen_function_bin_map $BIN > $FUNC_BIN_MAP
gen_callsite_bin_map $BIN > $CS_BIN_MAP
gen_cem_src_input $IF > $CEM_SRC_INPUT

if [ $HT -eq 1 ]; then
    gen_src2bin_ht_map $CS_BIN_MAP CS_SRC2BIN_AA src2bin_ht_cs
    gen_src2bin_ht_map $FUNC_BIN_MAP FUNC_SRC2BIN_AA src2bin_ht_func

    SRC2BIN=src2bin_ht
else
    SRC2BIN=src2bin
fi

echo " * Generating output file: $OF ..." >&2

while read line
do
    src_callsite=$( echo $line | awk '{ print $1; }' )
    src_targets=$( echo $line | awk '{ print $2; }' | sed "s/,/ /g" )
    bin_callsite=$( $SRC2BIN $src_callsite $CS_BIN_MAP src2bin_ht_cs )
    bin_targets=""
    for src_target in $src_targets
    do
        bin_target=$( $SRC2BIN $src_target $FUNC_BIN_MAP src2bin_ht_func )
        [ "$bin_targets" == "" ] || bin_targets+=","
        bin_targets+="$bin_target"
    done
    echo "$bin_callsite $bin_targets"
done < $CEM_SRC_INPUT > $OF

if [ $KEEP_TEMPS -eq 0 ]; then
    echo " * Removing temps..." >&2
    rm -f $( dirname $FUNC_BIN_MAP )/cem-*
fi

echo " * Done" >&2

