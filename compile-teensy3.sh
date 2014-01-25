#!/bin/bash

set -e
set -x

CROSS=/home/ambro/gcc-arm-none-eabi-4_7-2013q3/bin/arm-none-eabi-
TEENSY_CORES=/home/ambro/cores
MAIN=aprinter/printer/aprinter-teensy3.cpp

CC=${CC:-${CROSS}gcc}
TEENSY3="${TEENSY_CORES}/teensy3"

FLAGS_C_CXX_LD=(
    -mcpu=cortex-m4 -mthumb -O2 -g
)
FLAGS_CXX_LD=(
    -fno-rtti -fno-exceptions
)
FLAGS_C=(
    -std=c99
)
FLAGS_CXX=(
    -std=c++11
)
FLAGS_C_CXX=(
    -DNDEBUG
    -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS
    -ffunction-sections -fdata-sections
    -I.
    -I"${TEENSY3}" -D__MK20DX128__ -DF_CPU=48000000
)
FLAGS_LD=(
    -T"${TEENSY3}/mk20dx128.ld" -nostartfiles -Wl,--gc-sections
)

CFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_C_CXX[@]}" "${FLAGS_C[@]}" ${CFLAGS})
CXXFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_CXX_LD[@]}" "${FLAGS_C_CXX[@]}" "${FLAGS_CXX[@]}" ${CXXFLAGS})
LDFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_CXX_LD[@]}" "${FLAGS_LD[@]}" ${LDFLAGS})

"${CC}" -x c -c "${CFLAGS[@]}" -Dasm=__asm__ "${TEENSY3}/mk20dx128.c"
"${CC}" -x c++ -c "${CXXFLAGS[@]}" aprinter/platform/teensy3/teensy3_support.cpp
"${CC}" -x c++ -c "${CXXFLAGS[@]}" "${MAIN}" -o main.o
"${CC}" "${LDFLAGS[@]}" mk20dx128.o teensy3_support.o main.o -o aprinter.elf -lm
#"${CROSS}objcopy" --output-target=binary aprinter.elf aprinter.bin
${CROSS}objcopy -O ihex aprinter.elf aprinter.hex
