#!/bin/bash

set -e
set -x

CROSS=/home/ambro/gcc-arm-none-eabi-4_8-2013q4/bin/arm-none-eabi-
TEENSY_CORES=/home/ambro/cores
MAIN=aprinter/printer/aprinter-teensy3.cpp
TEENSY_VERSION=3.0
F_CPU=96000000

mkdir -p out

CC=${CC:-${CROSS}gcc}
TEENSY3="${TEENSY_CORES}/teensy3"

if [[ $TEENSY_VERSION = 3.1 ]]; then
    CPU_DEF=__MK20DX256__
    LINKER_SCRIPT=${TEENSY3}/mk20dx256.ld
elif [[ $TEENSY_VERSION = 3.0 ]]; then
    CPU_DEF=__MK20DX128__
    LINKER_SCRIPT=${TEENSY3}/mk20dx128.ld
else
    echo "Unknown TEENSY_VERSION"
    exit 1
fi

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
    -I"${TEENSY3}" -D${CPU_DEF} -DF_CPU=${F_CPU} -DUSB_SERIAL
)
FLAGS_LD=(
    -T "${LINKER_SCRIPT}" -nostartfiles -Wl,--gc-sections
    --specs=nano.specs
)

CFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_C_CXX[@]}" "${FLAGS_C[@]}" ${CFLAGS})
CXXFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_CXX_LD[@]}" "${FLAGS_C_CXX[@]}" "${FLAGS_CXX[@]}" ${CXXFLAGS})
LDFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_CXX_LD[@]}" "${FLAGS_LD[@]}" ${LDFLAGS})

cp "${TEENSY3}/mk20dx128.c" out/mk20dx128-hacked.c
patch -p2 out/mk20dx128-hacked.c < teensy-startup-watchdog.patch

"${CC}" -x c -c "${CFLAGS[@]}" -Dasm=__asm__ out/mk20dx128-hacked.c -o out/mk20dx128-hacked.o
"${CC}" -x c -c "${CFLAGS[@]}" -Dasm=__asm__ "${TEENSY3}/nonstd.c" -o out/nonstd.o
"${CC}" -x c -c "${CFLAGS[@]}" -Dasm=__asm__ "${TEENSY3}/yield.c" -o out/yield.o
"${CC}" -x c -c "${CFLAGS[@]}" -Dasm=__asm__ "${TEENSY3}/usb_dev.c" -o out/usb_dev.o
"${CC}" -x c -c "${CFLAGS[@]}" -Dasm=__asm__ "${TEENSY3}/usb_desc.c" -o out/usb_desc.o
"${CC}" -x c -c "${CFLAGS[@]}" -Dasm=__asm__ "${TEENSY3}/usb_mem.c" -o out/usb_mem.o
"${CC}" -x c -c "${CFLAGS[@]}" -Dasm=__asm__ "${TEENSY3}/usb_serial.c" -o out/usb_serial.o
"${CC}" -x c++ -c "${CXXFLAGS[@]}" aprinter/platform/teensy3/teensy3_support.cpp -o out/teensy3_support.o
"${CC}" -x c++ -c "${CXXFLAGS[@]}" "${MAIN}" -o out/main.o
"${CC}" "${LDFLAGS[@]}" out/mk20dx128-hacked.o out/nonstd.o out/yield.o out/usb_dev.o out/usb_desc.o out/usb_mem.o out/usb_serial.o out/teensy3_support.o out/main.o -o out/aprinter.elf -lm
${CROSS}objcopy -O ihex -R .eeprom out/aprinter.elf out/aprinter.hex
