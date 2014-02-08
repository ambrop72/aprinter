#!/bin/bash

set -e
set -x

# User settings.
MAIN=aprinter/printer/aprinter-rampsfd.cpp
ASF_DIR=/home/ambro/asf-standalone-archive-3.14.0.86/xdk-asf-3.14.0
GCCARM_DIR=/home/ambro/gcc-arm-none-eabi-4_8-2013q4
CROSS="${GCCARM_DIR}/bin/arm-none-eabi-"
USE_USB_SERIAL=0

# Clang support.
# Yes, it works, if you compile clang right, but runs slower.
# Configure llvm/clang with:
# ../llvm-3.4/configure --enable-targets=arm --target=arm-none-eabi --enable-cxx11 \
#   --prefix=/home/ambro/clang-arm-install \
#   --with-gcc-toolchain=/home/ambro/gcc-arm-none-eabi-4_8-2013q4/lib/gcc/arm-none-eabi/4.8.3 \
#   --with-c-include-dirs="/home/ambro/gcc-arm-none-eabi-4_8-2013q4/arm-none-eabi/include"
# Also remove -g from FLAGS_C_CXX_LD below or segmentation fault.
USE_CLANG=0
CLANG_DIR=/home/ambro/clang-arm-install

mkdir -p out

# clang compiler with gcc-arm-embedded libs
if [[ $USE_CLANG = 1 ]]; then
    CC="${CLANG_DIR}/bin/arm-none-eabi-clang"
    CC_STARTUP="${CROSS}gcc"
    export PATH="${GCCARM_DIR}/bin":$PATH
else
    CC="${CROSS}gcc"
    CC_STARTUP="${CC}"
fi

CMSIS_DIR=${ASF_DIR}/sam/utils/cmsis/sam3x
TEMPLATES_DIR=${CMSIS_DIR}/source/templates
LINKER_SCRIPT=${ASF_DIR}/sam/utils/linker_scripts/sam3x/sam3x8/gcc/flash.ld

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
    -D__SAM3X8E__ -DHEAP_SIZE=16384 -DNDEBUG
    -DBOARD=43
    -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS
    -I. -I"${CMSIS_DIR}/include" -I"${TEMPLATES_DIR}" -I"${ASF_DIR}/thirdparty/CMSIS/Include"
    -I"${ASF_DIR}/sam/utils"
    -I"${ASF_DIR}/sam/utils/preprocessor"
    -I"${ASF_DIR}/sam/utils/header_files"
    -I"${ASF_DIR}/sam/boards"
    -I"${ASF_DIR}/sam/drivers/pmc"
    -I"${ASF_DIR}/common/utils"
    -I"${ASF_DIR}/common/services/usb"
    -I"${ASF_DIR}/common/services/usb/udc"
    -I"${ASF_DIR}/common/services/clock"
    -I"${ASF_DIR}/common/services/sleepmgr"
    -I"${ASF_DIR}/common/services/usb/class/cdc"
    -I"${ASF_DIR}/common/services/usb/class/cdc/device"
    -I"${ASF_DIR}/common/boards"
    -I"${ASF_DIR}"
    -I aprinter/platform/at91sam3x
    -ffunction-sections -fdata-sections
)
FLAGS_LD=(
    -T"${LINKER_SCRIPT}" -nostartfiles -Wl,--gc-sections
    --specs=nano.specs
)

C_SOURCES=(
    "${TEMPLATES_DIR}/exceptions.c"
    "${TEMPLATES_DIR}/system_sam3x.c"
    "${TEMPLATES_DIR}/gcc/startup_sam3x.c"
    "${ASF_DIR}/sam/drivers/pmc/pmc.c"
)

if [[ $USE_USB_SERIAL = 1 ]]; then
    cp "${ASF_DIR}/common/services/usb/class/cdc/device/udi_cdc.c" out/udi_cdc-hacked.c
    patch -p0 out/udi_cdc-hacked.c < asf-cdc-tx.patch

    FLAGS_C_CXX=("${FLAGS_C_CXX[@]}" -DUSB_SERIAL)
    C_SOURCES=("${C_SOURCES[@]}"
        "${ASF_DIR}/sam/drivers/uotghs/uotghs_device.c"
        "${ASF_DIR}/sam/drivers/pmc/sleep.c"
        "${ASF_DIR}/common/utils/interrupt/interrupt_sam_nvic.c"
        "${ASF_DIR}/common/services/usb/udc/udc.c"
        out/udi_cdc-hacked.c
        "${ASF_DIR}/common/services/usb/class/cdc/device/udi_cdc_desc.c"
        "${ASF_DIR}/common/services/clock/sam3x/sysclk.c"
    )
fi

CFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_C_CXX[@]}" "${FLAGS_C[@]}" ${CFLAGS})
CXXFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_CXX_LD[@]}" "${FLAGS_C_CXX[@]}" "${FLAGS_CXX[@]}" ${CXXFLAGS})
LDFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_CXX_LD[@]}" "${FLAGS_LD[@]}" ${LDFLAGS})

C_OBJS=()
for cfile in "${C_SOURCES[@]}"; do
    OBJ=out/$(basename -s .c "${cfile}").o
    C_OBJS=("${C_OBJS[@]}" "${OBJ}")
    "${CC}" -x c -c "${CFLAGS[@]}" "${cfile}" -o "${OBJ}"
done

"${CC}" -x c -c "${CFLAGS[@]}" -fno-builtin "aprinter/platform/clang_missing.c" -o out/clang_missing.o
"${CC}" -x c++ -c "${CXXFLAGS[@]}" aprinter/platform/at91sam3x/at91sam3x_support.cpp -o out/at91sam3x_support.o
"${CC}" -x c++ -c "${CXXFLAGS[@]}" "${MAIN}" -o out/main.o
"${CC}" "${LDFLAGS[@]}" "${C_OBJS[@]}" out/clang_missing.o out/at91sam3x_support.o out/main.o -o out/aprinter.elf -lm
${CROSS}objcopy --output-target=binary out/aprinter.elf out/aprinter.bin
