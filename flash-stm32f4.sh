#!/usr/bin/env bash

set -e
set -x

STLINK=/run/current-system/sw/bin

#"${STLINK}/st-flash" erase
"${STLINK}/st-flash" write out/aprinter.bin 0x08000000
