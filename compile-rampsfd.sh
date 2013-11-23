#!/bin/bash

set -e
set -x

CROSS=/home/ambro/gcc-arm-none-eabi-4_7-2013q3/bin/arm-none-eabi-
#CROSS=armv7m-softfloat-eabi-
CC=${CC:-${CROSS}gcc}
ASF_DIR=/home/ambro/asf-3.12.1
MAIN=aprinter/printer/aprinter-rampsfd.cpp

CMSIS_DIR=${ASF_DIR}/sam/utils/cmsis/sam3x
TEMPLATES_DIR=${CMSIS_DIR}/source/templates
LINKER_SCRIPT=${ASF_DIR}/sam/utils/linker_scripts/sam3x/sam3x8/gcc/flash.ld

FLAGS_C_CXX_LD=(
    -mcpu=cortex-m3 -mthumb -O2 -g
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
    -D__SAM3X8E__ -DHEAP_SIZE=16384 -DNDEBUG
    -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS
    -I. -I"${CMSIS_DIR}/include" -I"${TEMPLATES_DIR}" -I"${ASF_DIR}/thirdparty/CMSIS/Include"
    -I"${ASF_DIR}/sam/utils" -I"${ASF_DIR}/sam/utils/preprocessor"
    -I"${ASF_DIR}/sam/utils/header_files" -I"${ASF_DIR}/common/utils"
    -I"${ASF_DIR}"
    -ffunction-sections -fdata-sections
)
FLAGS_LD=(
    -T"${LINKER_SCRIPT}" -nostartfiles -Wl,--gc-sections
)

CFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_C_CXX[@]}" "${FLAGS_C[@]}" ${CFLAGS})
CXXFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_CXX_LD[@]}" "${FLAGS_C_CXX[@]}" "${FLAGS_CXX[@]}" ${CXXFLAGS})
LDFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_CXX_LD[@]}" "${FLAGS_LD[@]}" ${LDFLAGS})

"${CC}" -x c -c "${CFLAGS[@]}" "${TEMPLATES_DIR}/exceptions.c"
"${CC}" -x c -c "${CFLAGS[@]}" "${TEMPLATES_DIR}/system_sam3x.c"
"${CC}" -x c -c "${CFLAGS[@]}" "${TEMPLATES_DIR}/gcc/startup_sam3x.c"
"${CC}" -x c -c "${CFLAGS[@]}" "${ASF_DIR}/sam/drivers/pmc/pmc.c"
"${CC}" -x c++ -c "${CXXFLAGS[@]}" aprinter/platform/at91sam3x/at91sam3x_support.cpp
"${CC}" -x c++ -c "${CXXFLAGS[@]}" "${MAIN}" -o main.o
"${CC}" "${LDFLAGS[@]}" exceptions.o system_sam3x.o startup_sam3x.o pmc.o at91sam3x_support.o main.o -o aprinter.elf -lm
${CROSS}objcopy --output-target=binary aprinter.elf aprinter.bin
