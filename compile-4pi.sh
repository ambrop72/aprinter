#!/bin/bash

set -e
set -x

# User settings.
MAIN=aprinter/printer/aprinter-4pi.cpp
ASF_DIR=/home/ambro/asf-standalone-archive-3.14.0.86/xdk-asf-3.14.0
GCCARM_DIR=/home/ambro/gcc-arm-none-eabi-4_8-2013q4

mkdir -p out

CROSS="${GCCARM_DIR}/bin/arm-none-eabi-"
CC="${CROSS}gcc"
CMSIS_DIR=${ASF_DIR}/sam/utils/cmsis/sam3u
TEMPLATES_DIR=${CMSIS_DIR}/source/templates
LINKER_SCRIPT=${ASF_DIR}/sam/utils/linker_scripts/sam3u/sam3u4/gcc/flash.ld

FLAGS_C_CXX_LD=(
    -mcpu=cortex-m3 -mthumb -O2 -g
)
FLAGS_CXX_LD=(
    -fno-rtti -fno-exceptions
    -Wno-deprecated-register
)
FLAGS_C=(
    -std=c99
)
FLAGS_CXX=(
    -std=c++11
)
FLAGS_C_CXX=(
    -D__SAM3U4E__ -DHEAP_SIZE=16384 -DNDEBUG
    -DBOARD=33
    -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS
    -I. -I"${CMSIS_DIR}/include" -I"${TEMPLATES_DIR}" -I"${ASF_DIR}/thirdparty/CMSIS/Include"
    -I"${ASF_DIR}/sam/utils"
    -I"${ASF_DIR}/sam/utils/preprocessor"
    -I"${ASF_DIR}/sam/utils/header_files"
    -I"${ASF_DIR}/sam/boards"
    -I"${ASF_DIR}/sam/drivers/pmc"
    -I"${ASF_DIR}/sam/drivers/pio"
    -I"${ASF_DIR}/common/utils"
    -I"${ASF_DIR}/common/services/ioport"
    -I"${ASF_DIR}/common/services/usb"
    -I"${ASF_DIR}/common/services/usb/udc"
    -I"${ASF_DIR}/common/services/clock"
    -I"${ASF_DIR}/common/services/sleepmgr"
    -I"${ASF_DIR}/common/services/usb/class/cdc"
    -I"${ASF_DIR}/common/services/usb/class/cdc/device"
    -I"${ASF_DIR}/common/boards"
    -I"${ASF_DIR}"
    -I aprinter/platform/at91sam3u
    -ffunction-sections -fdata-sections
)
FLAGS_LD=(
    -T"${LINKER_SCRIPT}" -nostartfiles -Wl,--gc-sections
    --specs=nano.specs
)

cp "${ASF_DIR}/common/services/usb/class/cdc/device/udi_cdc.c" out/udi_cdc-hacked.c
patch -p0 out/udi_cdc-hacked.c < patches/asf-cdc-tx.patch

C_SOURCES=(
    "${TEMPLATES_DIR}/exceptions.c"
    "${TEMPLATES_DIR}/system_sam3u.c"
    "${TEMPLATES_DIR}/gcc/startup_sam3u.c"
    "${ASF_DIR}/sam/drivers/pmc/pmc.c"
    "${ASF_DIR}/sam/drivers/udphs/udphs_device.c"
    "${ASF_DIR}/sam/drivers/pmc/sleep.c"
    "${ASF_DIR}/common/utils/interrupt/interrupt_sam_nvic.c"
    "${ASF_DIR}/common/services/usb/udc/udc.c"
    out/udi_cdc-hacked.c
    "${ASF_DIR}/common/services/usb/class/cdc/device/udi_cdc_desc.c"
    "${ASF_DIR}/common/services/clock/sam3u/sysclk.c"
)

CFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_C_CXX[@]}" "${FLAGS_C[@]}" ${CFLAGS})
CXXFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_CXX_LD[@]}" "${FLAGS_C_CXX[@]}" "${FLAGS_CXX[@]}" ${CXXFLAGS})
LDFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_CXX_LD[@]}" "${FLAGS_LD[@]}" ${LDFLAGS})

C_OBJS=()
for cfile in "${C_SOURCES[@]}"; do
    OBJ=out/$(basename "${cfile}").o
    C_OBJS=("${C_OBJS[@]}" "${OBJ}")
    "${CC}" -x c -c "${CFLAGS[@]}" "${cfile}" -o "${OBJ}"
done

"${CC}" -x c++ -c "${CXXFLAGS[@]}" aprinter/platform/at91sam3u/at91sam3u_support.cpp -o out/at91sam3u_support.o
"${CC}" -x c++ -c "${CXXFLAGS[@]}" "${MAIN}" -o out/main.o
"${CC}" "${LDFLAGS[@]}" "${C_OBJS[@]}" out/at91sam3u_support.o out/main.o -o out/aprinter.elf -lm
"${CROSS}objcopy" --output-target=binary out/aprinter.elf out/aprinter.bin
