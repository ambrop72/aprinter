#!/bin/bash
avrdude -p atmega1284p -c stk500v1 -b 57600 -P /dev/ttyUSB0 -D -U flash:w:aprinter.hex
