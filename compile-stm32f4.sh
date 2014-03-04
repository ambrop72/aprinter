#!/bin/bash

set -e
set -x

# User settings.
MAIN=aprinter/printer/aprinter-stm32f4.cpp
STM32F4_DIR=/home/ambro/STM32F4xx_DSP_StdPeriph_Lib_V1.3.0
GCCARM_DIR=/home/ambro/gcc-arm-none-eabi-4_8-2013q4
CROSS="${GCCARM_DIR}/bin/arm-none-eabi-"

mkdir -p out

CC="${CROSS}gcc"
CMSIS_DIR=${STM32F4_DIR}/Libraries/CMSIS/Device/ST/STM32F4xx
TEMPLATES_DIR=${CMSIS_DIR}/Source/Templates
STDPERIPH=${STM32F4_DIR}/Libraries/STM32F4xx_StdPeriph_Driver
LINKER_SCRIPT=${STM32F4_DIR}/Project/STM32F4xx_StdPeriph_Templates/RIDE/stm32f4xx_flash.ld

FLAGS_C_CXX_LD=(
    -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -O2
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
    -DSTM32F40_41xxx -DHSE_VALUE=8000000 -D"assert_param(x)"
    -DNDEBUG
    -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS
    -I.
    -I aprinter/platform/stm32f4
    -I "${CMSIS_DIR}/Include"
    -I "${STM32F4_DIR}/Libraries/CMSIS/Include"
    -I "${STDPERIPH}/inc"
    -ffunction-sections -fdata-sections
)
FLAGS_LD=(
    -T"${LINKER_SCRIPT}" -nostartfiles -Wl,--gc-sections
    --specs=nano.specs
)

cp --no-preserve=all "${TEMPLATES_DIR}/system_stm32f4xx.c" out/system_stm32f4xx-hacked.c
sed 's/#define PLL_M      25/#define PLL_M      8/' -i out/system_stm32f4xx-hacked.c

C_SOURCES=(
    "out/system_stm32f4xx-hacked.c"
    "${STDPERIPH}/src/stm32f4xx_rcc.c"
)

CFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_C_CXX[@]}" "${FLAGS_C[@]}" ${CFLAGS})
CXXFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_CXX_LD[@]}" "${FLAGS_C_CXX[@]}" "${FLAGS_CXX[@]}" ${CXXFLAGS})
LDFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_CXX_LD[@]}" "${FLAGS_LD[@]}" ${LDFLAGS})

C_OBJS=()
for cfile in "${C_SOURCES[@]}"; do
    OBJ=out/$(basename -s .c "${cfile}").o
    C_OBJS=("${C_OBJS[@]}" "${OBJ}")
    "${CC}" -x c -c "${CFLAGS[@]}" "${cfile}" -o "${OBJ}"
done

"${CC}" -x assembler "${FLAGS_C_CXX_LD[@]}" -c "${TEMPLATES_DIR}/gcc_ride7/startup_stm32f40xx.s" -o out/startup_stm32f40xx.o
"${CC}" -x c++ -c "${CXXFLAGS[@]}" aprinter/platform/stm32f4/stm32f4_support.cpp -o out/stm32f4_support.o
"${CC}" -x c++ -c "${CXXFLAGS[@]}" "${MAIN}" -o out/main.o
"${CC}" "${LDFLAGS[@]}" "${C_OBJS[@]}" out/startup_stm32f40xx.o out/stm32f4_support.o out/main.o -o out/aprinter.elf -lm
${CROSS}objcopy --output-target=binary out/aprinter.elf out/aprinter.bin
