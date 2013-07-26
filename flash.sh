#!/bin/bash
avrdude -p atmega1284p -c stk500v2 -P /dev/ttyACM0 -U flash:w:aprinter.hex
