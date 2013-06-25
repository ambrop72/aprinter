#!/bin/bash

set -e
set -x

CROSS=${CROSS:-avr-}
CXX=${CXX:-${CROSS}g++}
MCU=${MCU:-atmega1284p}
F_CPU=${F_CPU:-20000000}
CXXFLAGS="-std=c++11 -mmcu=$MCU -DF_CPU=$F_CPU -DNDEBUG -O4 -g3 -gdwarf-2 \
-ffunction-sections -fdata-sections -Wl,--gc-sections \
-D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS \
-DAMBROLIB_AVR -I. $CXXFLAGS"

${CXX} $CXXFLAGS examples/test4.cpp -o test4.elf -Wl,-u,vfprintf -lprintf_flt -lm
${CROSS}objcopy -j .text -j .data -O ihex test4.elf test4.hex
