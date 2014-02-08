#!/bin/bash
avrdude -p atmega2560 -c stk500v2 -b 115200 -P /dev/ttyACM0 -D -U flash:w:out/aprinter.hex
