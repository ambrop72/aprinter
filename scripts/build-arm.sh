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

if [ "$(uname)" == "Linux" ]; then
    SYSARCH=linux
elif [ "${uname}" == "Darwin" ]; then
    SYSARCH=mac
else
    SYSARCH=""
fi

GCCARM_CURRENT=4_8-2013q4
GCCARM_RELEASE=20131204

ARM_GCC_PATH="${ROOT}/depends/gcc-arm-none-eabi-${GCCARM_CURRENT}"

if [ -n "${CUSTOM_ARM_GCC}" ]; then
    ARM_GCC_PREFIX=${CUSTOM_ARM_GCC}
else
    ARM_GCC_PREFIX=${ARM_GCC_PATH}/bin/arm-none-eabi-
fi

ARM_CC=${ARM_GCC_PREFIX}gcc
ARM_OBJCOPY=${ARM_GCC_PREFIX}objcopy

GCCARM_URL=(
    "https://launchpad.net/gcc-arm-embedded/4.8/4.8-2013-q4-major/+download/gcc-arm-none-eabi-${GCCARM_CURRENT}-${GCCARM_RELEASE}-${SYSARCH}.tar.bz2" 
)

GCCARM_CHECKSUM=(
    "fd090320ab9d4b6cf8cdf29bf5b046db816da9e6738eb282b9cf2321ecf6356a  gcc-arm-none-eabi-4_8-2013q4-20131204-linux.tar.bz2"
)

install_arm() {
    if [ -z "${CUSTOM_ARM_GCC}" ]; then
        echo "  Installing ARM toolchain"
        [ -f "${ARM_CC}" ] && \
        [ -f "${ARM_OBJCOPY}" ] && echo "   [!] ARM toolchain already installed" && return 0

        create_depends_dir
        retr_and_extract GCCARM_URL[@] GCCARM_CHECKSUM[@]
    fi
}

flush_arm() {
    clean
    echo "  Deleting GCC-ARM install. Are you sure? (C-c to abort)"
    read 
    rm -rf "${ARM_GCC_PATH}"
}

check_depends_arm() {
    echo "   Checking depends"
    check_build_tool "${ARM_CC}" "ARM compiler"
    check_build_tool "${ARM_OBJCOPY}" "ARM objcopy"
}

clean_arm() {
    clean
    for file in "${C_SOURCES[@]}"; do
        OBJ=${BUILD}/$(basename "${file}" .c).o
        ($V; rm -f $OBJ)
    done

    for file in "${CXX_SOURCES[@]}"; do
        OBJ=${BUILD}/$(basename "${file}" .cpp).o
        ($V; rm -f $OBJ)
    done

    for file in "${ASM_SOURCES[@]}"; do
        OBJ=${BUILD}/$(basename "${file}" .s).o
        ($V; rm -f $OBJ)
    done
}

configure_arm() {
    echo "  Configuring ARM build"
    FLAGS_C_CXX_LD=(
        -mcpu=${ARM_CPU} -mthumb ${ARM_EXTRA_CPU_FLAGS} -O2
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
        -I.
        -ffunction-sections -fdata-sections
    )
    FLAGS_LD=(
        -T"${LINKER_SCRIPT}" -nostartfiles -Wl,--gc-sections
        --specs=nano.specs
    )

    C_SOURCES=()
    CXX_SOURCES=( "${SOURCE}" )
    ASM_SOURCES=()

    OBJS=()

    FLUSH=flush_arm
    CHECK=check_depends_arm
    CLEAN=clean_arm
}

build_arm() {
    echo "  Compiling for ARM"
    ${CHECK}
    CFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_C_CXX[@]}" "${FLAGS_C[@]}" ${CFLAGS})
    CXXFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_CXX_LD[@]}" "${FLAGS_C_CXX[@]}" "${FLAGS_CXX[@]}" ${CXXFLAGS})
    ASMFLAGS=("${FLAGS_C_CXX_LD[@]}")
    LDFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_CXX_LD[@]}" "${FLAGS_LD[@]}" ${LDFLAGS})

    create_build_dir

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
        ( $V ; "${ARM_CC}" -x assembler -c "${ASMFLAGS[@]}" "${file}" -o "${OBJ}" )
    done
    
    echo "   Linking objects"
    ( $V ;
    "${ARM_CC}" "${LDFLAGS[@]}" "${OBJS[@]}" -o "${TARGET}.elf" -lm

    echo "   Building images"
    "${ARM_OBJCOPY}" --output-target=binary "${TARGET}.elf" "${TARGET}.bin"
    "${ARM_OBJCOPY}" --output-target=ihex "${TARGET}.elf" "${TARGET}.hex"
    )
}
