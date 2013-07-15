#!/bin/bash

set -e
set -x

CROSS=${CROSS:-avr-}
CXX=${CXX:-${CROSS}g++}
MCU=${MCU:-atmega1284p}
F_CPU=${F_CPU:-20000000}
CXXFLAGS="-std=c++1y -mmcu=$MCU -DF_CPU=$F_CPU -DNDEBUG -O2 -fwhole-program -g \
-ffunction-sections -fdata-sections -Wl,--gc-sections \
-D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS \
-DAMBROLIB_AVR -I. $CXXFLAGS"

${CXX} $CXXFLAGS aprinter/printer/aprinter.cpp -o aprinter.elf -Wl,-u,vfprintf -lprintf_flt
${CROSS}objcopy -j .text -j .data -O ihex aprinter.elf aprinter.hex
