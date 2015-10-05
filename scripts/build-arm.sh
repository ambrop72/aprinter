#!/usr/bin/env bash
# 
# Simple build script crafted for the APrinter project to support multiple 
# architecture targets and build actions using an elegant commandline.
# 
# Copyright (c) 2014 Bernard `Guyzmo` Pratz
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
#########################################################################################
# ARM stuff

GCCARM_CURRENT=4_8-2014q1

if [ "$(uname)" == "Linux" ]; then
    GCCARM_RELEASE=20140314
    GCCARM_CHECKSUM="ce92859550819d4a3d1a6e2672ea64882b30afa2c08cf67fa8e1d93788c2c577  gcc-arm-none-eabi-4_8-2014q1-20140314-linux.tar.bz2"
elif [ "$(uname)" == "Darwin" ]; then
    GCCARM_RELEASE=20140314
    GCCARM_CHECKSUM="d8d037d56e37c513f13f3b8864265489dca9ffaca616f679d45dff6e500c47af  gcc-arm-none-eabi-4_8-2014q1-20140314-mac.tar.bz2"
else
    GCCARM_RELEASE=""
    GCCARM_CHECKSUM=""
fi

GCCARM_URL=(
    "https://launchpad.net/gcc-arm-embedded/4.8/4.8-2014-q1-update/+download/gcc-arm-none-eabi-${GCCARM_CURRENT}-${GCCARM_RELEASE}-${SYSARCH}.tar.bz2" 
)

ARM_GCC_PATH="${ROOT}/depends/gcc-arm-none-eabi-${GCCARM_CURRENT}"

if [ -n "${CUSTOM_ARM_GCC}" ]; then
    ARM_GCC_PREFIX=${CUSTOM_ARM_GCC}
else
    ARM_GCC_PREFIX=${ARM_GCC_PATH}/bin/arm-none-eabi-
fi

ARM_GCC=${ARM_GCC_PREFIX}gcc

if [ "$BUILD_WITH_CLANG" = 1 ]; then
    ARM_CC=${CLANG_ARM_EMBEDDED}clang
else
    ARM_CC=${ARM_GCC}
fi

ARM_OBJCOPY=${ARM_GCC_PREFIX}objcopy
ARM_SIZE=${ARM_GCC_PREFIX}size

install_arm() {
    if [ -z "${CUSTOM_ARM_GCC}" ]; then
        echo "  Installing ARM toolchain"
        [ -f "${ARM_CC}" ] && \
        [ -f "${ARM_OBJCOPY}" ] && echo "   [!] ARM toolchain already installed" && return 0

        create_depends_dir
        retr_and_extract GCCARM_URL[@] GCCARM_CHECKSUM[@]
    fi
}

check_depends_arm() {
    echo "   Checking depends"
    check_build_tool "${ARM_CC}" "ARM compiler"
    check_build_tool "${ARM_OBJCOPY}" "ARM objcopy"
    check_build_tool "${ARM_SIZE}" "ARM size calculator"
}

configure_arm() {
    echo "  Configuring ARM build"
    
    FLAGS_OPT=( -O$( [[ $OPTIMIZE_FOR_SIZE = "1" ]] && echo s || echo 2 ) )
    FLAGS_C_CXX_LD=(
        -mcpu=${ARM_CPU} -mthumb ${ARM_EXTRA_CPU_FLAGS} "${FLAGS_OPT[@]}"
        -fno-math-errno -fno-trapping-math
    )
    FLAGS_CXX_LD=(
        -fno-rtti -fno-exceptions
    )
    FLAGS_C=(
        -std=c99
    )
    FLAGS_CXX=(
        -std=c++11 -fno-access-control -ftemplate-depth=1024
    )
    FLAGS_C_CXX=(
        -DNDEBUG
        -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS
        -I.
        -Wfatal-errors
        -Wno-absolute-value -Wno-undefined-internal -Wno-deprecated-register
        -ffunction-sections -fdata-sections
        $EXTRA_COMPILE_FLAGS
    )
    FLAGS_LD=(
        -T"${LINKER_SCRIPT}" -nostartfiles -Wl,--gc-sections
        --specs=nano.specs -u _printf_float
    )

    if [ "$BUILD_WITH_CLANG" = 1 ]; then
        FLAGS_C_CXX_LD+=( -fshort-enums )
        FLAGS_C_CXX+=( -DAPRINTER_BROKEN_FABS )
    fi
    
    C_SOURCES=( $(eval echo "$EXTRA_C_SOURCES") )
    CXX_SOURCES=( $(eval echo "$EXTRA_CXX_SOURCES") "${SOURCE}" )
    ASM_SOURCES=()

    OBJS=()

    CHECK=check_depends_arm
}

build_arm() {
    echo "  Compiling for ARM"
    ${CHECK}
    CFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_C_CXX[@]}" "${FLAGS_C[@]}" ${CFLAGS} ${CCXXLDFLAGS})
    CXXFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_CXX_LD[@]}" "${FLAGS_C_CXX[@]}" "${FLAGS_CXX[@]}" ${CXXFLAGS} ${CCXXLDFLAGS})
    ASMFLAGS=("${FLAGS_C_CXX_LD[@]}")
    LDFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_CXX_LD[@]}" "${FLAGS_LD[@]}" ${LDFLAGS} ${CCXXLDFLAGS})

    echo "   Compiling C files"
    for file in "${C_SOURCES[@]}"; do
        OBJ=${BUILD}/$(basename "${file}" .c).o
        OBJS+=( "${OBJ}" )
        ( $V ; "${ARM_CC}" -x c -c "${CFLAGS[@]}" "${file}" -o "${OBJ}" )
    done

    echo "   Compiling C++ files"
    for file in "${CXX_SOURCES[@]}"; do
        OBJ=${BUILD}/$(basename "${file}" .cpp).o
        OBJS+=( "${OBJ}" )
        ( $V ; "${ARM_CC}" -x c++ -c "${CXXFLAGS[@]}" "${file}" -o "${OBJ}" )
    done

    echo "   Compiling Assembly files"
    for file in "${ASM_SOURCES[@]}"; do
        OBJ=${BUILD}/$(basename "${file}" .s).o
        OBJS+=( "${OBJ}" )
        ( $V ; "${ARM_GCC}" -x assembler -c "${ASMFLAGS[@]}" "${file}" -o "${OBJ}" )
    done
    
    echo "   Linking objects"
    ( $V ;
    "${ARM_CC}" "${LDFLAGS[@]}" "${OBJS[@]}" -o "${TARGET}.elf" -lm

    echo "   Size of build: "
    "$ARM_SIZE" "${TARGET}.elf" | sed 's/^/    /'
    
    echo "   Building images"
    "${ARM_OBJCOPY}" --output-target=binary "${TARGET}.elf" "${TARGET}.bin"
    "${ARM_OBJCOPY}" --output-target=ihex "${TARGET}.elf" "${TARGET}.hex"
    )
}
