#!/bin/bash

set -e
set -x

openocd -f aprinter/platform/at91sam7s/openocd-flash.cfg -c "myflash aprinter.bin" -c "reset run" -c exit
