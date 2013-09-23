#!/bin/bash

set -e
set -x

CROSS=armv4t-none-eabi-
CXX=${CROSS}g++

MAIN=aprinter/printer/aprinter-at91sam7s.cpp
AT91SAM7S_PLAT=AT91SAM7S512
HEAP_SIZE=2048
PANIC_LED=AT91C_PIO_PA27
F_MUL=77
F_DIV=13
F_CRYSTAL=18432000

CXXFLAGS="-std=c++11 -mcpu=arm7tdmi -DNDEBUG -O2 -fwhole-program -g \
-ffunction-sections -fdata-sections -Wl,--gc-sections \
-D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS \
-I. -fno-rtti -fno-exceptions -nostartfiles \
-Taprinter/platform/at91sam7s/flash.lds -D${AT91SAM7S_PLAT} \
-DHEAP_SIZE=${HEAP_SIZE} -DPANIC_LED=${PANIC_LED} \
-DF_MUL=${F_MUL} -DF_DIV=${F_DIV} -DF_CRYSTAL=${F_CRYSTAL} $CXXFLAGS"

${CXX} $CXXFLAGS aprinter/platform/at91sam7s/crt.S aprinter/platform/at91sam7s/at91sam7s_support.cpp ${MAIN} -o aprinter.elf
${CROSS}objcopy --output-target=binary aprinter.elf aprinter.bin
