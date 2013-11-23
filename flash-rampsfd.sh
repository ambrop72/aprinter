#!/bin/bash

set -e
set -x

/home/ambro/arduino-1.5.4/hardware/tools/bossac -p ttyACM0 -U false -i -e -w -v -b aprinter.bin -R
