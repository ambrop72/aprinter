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
#####################################################################################
# Teensy

configure_teensy() {
    TEENSY3=${TEENSY_CORES}/teensy3

    if [[ "$TEENSY_VERSION" = 3.1 ]]; then
        CPU_DEF=__MK20DX256__
        LINKER_SCRIPT=${TEENSY3}/mk20dx256.ld
    elif [[ "$TEENSY_VERSION" = 3.0 ]]; then
        CPU_DEF=__MK20DX128__
        LINKER_SCRIPT=${TEENSY3}/mk20dx128.ld
    else
        echo "Unknown TEENSY_VERSION"
        exit 1
    fi

    ARM_CPU=cortex-m4

    configure_arm
    
    if [ "$BUILD_WITH_CLANG" = 1 ]; then
        FLAGS_C_CXX_LD+=(
            -msoft-float
        )
    fi

    FLAGS_C+=(
        -Dasm=__asm__
    )

    FLAGS_C_CXX+=(
        -I"${TEENSY3}" -D${CPU_DEF} -DF_CPU=${F_CPU} -DUSB_SERIAL -DAPRINTER_NO_SBRK
    )

    C_SOURCES+=(
        "${TEENSY3}/mk20dx128.c"
        "${TEENSY3}/nonstd.c"
        "${TEENSY3}/usb_dev.c"
        "${TEENSY3}/usb_desc.c"
        "${TEENSY3}/usb_mem.c"
        "${TEENSY3}/usb_serial.c"
        "aprinter/platform/teensy3/aprinter_teensy_eeprom.c"
        "aprinter/platform/newlib_common.c"
    )

    CXX_SOURCES+=(
        "aprinter/platform/teensy3/teensy3_support.cpp"
    )

    # define target functions
    RUNBUILD=build_arm
    CHECK=check_depends_teensy
}

check_depends_teensy() {
    check_depends_arm
    [ -d "${TEENSY3}" ] || fail "Teensy3 Framework missing in dependences"
}
