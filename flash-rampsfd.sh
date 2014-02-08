#!/bin/bash

set -e
set -x

PORT=ttyACM0
BOSSAC=/home/ambro/arduino-1.5.4/hardware/tools/bossac

stty -F /dev/${PORT} 1200
sleep 0.5
"$BOSSAC" -p ${PORT} -U false -i -e -w -v -b out/aprinter.bin -R
