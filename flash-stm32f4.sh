#!/bin/bash

set -e
set -x

STLINK=/home/ambro/stlink

#"${STLINK}/st-flash" erase
"${STLINK}/st-flash" --reset write out/aprinter.bin 0x08000000
