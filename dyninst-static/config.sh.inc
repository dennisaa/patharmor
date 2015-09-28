#!/bin/sh

if [ ! -f config.inc ]; then
    echo "Run configure first!"
    exit 1
fi

cat config.inc | sed "s/=\(.*\)/=\"\1\"/g" > /tmp/__config.inc
. /tmp/__config.inc
