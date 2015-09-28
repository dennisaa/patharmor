#!/bin/bash

set -e 

cut -d' ' -f4,5 libsyms.rel | uniq > libsyms.index

