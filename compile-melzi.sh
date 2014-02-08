#!/bin/bash

set -e
set -x

CROSS=avr-
CXX=${CROSS}g++

MCU=atmega1284p
F_CPU=16000000
MAIN=aprinter/printer/aprinter-melzi.cpp

mkdir -p out

CXXFLAGS="-std=c++11 -mmcu=$MCU -DF_CPU=$F_CPU -DNDEBUG -O2 -fwhole-program -g \
-ffunction-sections -fdata-sections -Wl,--gc-sections \
-D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS \
-DAMBROLIB_AVR -I. $CXXFLAGS"

${CXX} $CXXFLAGS ${MAIN} -o out/aprinter.elf -Wl,-u,vfprintf -lprintf_flt
${CROSS}objcopy -j .text -j .data -O ihex out/aprinter.elf out/aprinter.hex
