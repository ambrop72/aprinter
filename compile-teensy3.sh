#!/bin/bash

set -e
set -x

CROSS=/home/ambro/gcc-arm-none-eabi-4_8-2013q4/bin/arm-none-eabi-
TEENSY_CORES=/home/ambro/cores
MAIN=aprinter/printer/aprinter-teensy3.cpp
TEENSY_VERSION=3.0
F_CPU=96000000

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

cp "${TEENSY3}/mk20dx128.c" mk20dx128-hacked.c
sed -i 's/WDOG_STCTRLH = WDOG_STCTRLH_ALLOWUPDATE;//' mk20dx128-hacked.c

"${CC}" -x c -c "${CFLAGS[@]}" -Dasm=__asm__ "mk20dx128-hacked.c"
"${CC}" -x c -c "${CFLAGS[@]}" -Dasm=__asm__ "${TEENSY3}/nonstd.c"
"${CC}" -x c -c "${CFLAGS[@]}" -Dasm=__asm__ "${TEENSY3}/yield.c"
"${CC}" -x c -c "${CFLAGS[@]}" -Dasm=__asm__ "${TEENSY3}/usb_dev.c"
"${CC}" -x c -c "${CFLAGS[@]}" -Dasm=__asm__ "${TEENSY3}/usb_desc.c"
"${CC}" -x c -c "${CFLAGS[@]}" -Dasm=__asm__ "${TEENSY3}/usb_mem.c"
"${CC}" -x c -c "${CFLAGS[@]}" -Dasm=__asm__ "${TEENSY3}/usb_serial.c"
"${CC}" -x c++ -c "${CXXFLAGS[@]}" aprinter/platform/teensy3/teensy3_support.cpp
"${CC}" -x c++ -c "${CXXFLAGS[@]}" "${MAIN}" -o main.o
"${CC}" "${LDFLAGS[@]}" mk20dx128-hacked.o nonstd.o yield.o usb_dev.o usb_desc.o usb_mem.o usb_serial.o teensy3_support.o main.o -o aprinter.elf -lm
${CROSS}objcopy -O ihex -R .eeprom aprinter.elf aprinter.hex
