#!/bin/bash
set -e
make -C lkm clean all

# hedge against crashing when loading the module
sync 
sleep 2

echo removing currently loaded armor module if present
sudo rmmod armor_module >/dev/null 2>&1 || true

echo loading armor module
sudo insmod lkm/armor-module.ko
