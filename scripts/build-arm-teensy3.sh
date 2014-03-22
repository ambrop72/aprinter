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
    TEENSY_CORES=${DEPS}/teensy3-cores
    TEENSY3="${TEENSY_CORES}/teensy3"

    if [[ $TEENSY_VERSION = 3.1 ]]; then
        CPU_DEF=__MK20DX256__
        MPU=mk20dx256
        LINKER_SCRIPT=${TEENSY3}/mk20dx256.ld
    elif [[ $TEENSY_VERSION = 3.0 ]]; then
        CPU_DEF=__MK20DX128__
        MPU=mk20dx128
        LINKER_SCRIPT=${TEENSY3}/mk20dx128.ld
    else
        echo "Unknown TEENSY_VERSION"
        exit 1
    fi

    configure_arm

    FLAGS_C+=(
        -Dasm=__asm__
    )

    FLAGS_C_CXX+=(
        -I"${TEENSY3}" -D${CPU_DEF} -DF_CPU=${F_CPU} -DUSB_SERIAL
    )

    C_SOURCES+=(
        "${TEENSY3}/mk20dx128.c"
        "${TEENSY3}/nonstd.c"
        "${TEENSY3}/yield.c"
        "${TEENSY3}/usb_dev.c"
        "${TEENSY3}/usb_desc.c"
        "${TEENSY3}/usb_mem.c"
        "${TEENSY3}/usb_serial.c"
    )

    CXX_SOURCES+=(
        "aprinter/platform/teensy3/teensy3_support.cpp"
    )

    # define target functions
    INSTALL=install_teensy
    RUNBUILD=build_arm
    UPLOAD=upload_teensy
    FLUSH=flush_teensy
    CHECK=check_depends_teensy
}

check_depends_teensy() {
    check_depends_arm
    [ -d ${TEENSY3_CORES} ] || fail "Teensy3 Framework missing in dependences"
    [ -d ${DEPS}/teensy_loader_cli ] || fail "Teensy3 upload tool missing"
}

flush_teensy() {
    flush_arm
    echo "  Flushing Teensy3 toolchain"
    (V;
    rm -rf ${DEPS}/teensy3-cores
    rm -rf ${DEPS}/teensy_loader_cli
    )
}

install_teensy() {
    install_arm
    (
        [ -d ${TEENSY3_CORES} ] && \
        [ -d ${DEPS}/teensy_loader_cli ] && \
        echo "   [!] Teensy3 toolchain already installed" && return 0

        cd ${DEPS}
        git clone https://github.com/PaulStoffregen/cores teensy3-cores
        git clone https://github.com/astraw/teensy_loader_cli
        cd teensy_loader_cli
        make
    )
    echo "  Installation of Teensy3 Framework and Teensy CLI Loader: success"
}

upload_teensy() {
    ${DEPS}/teensy_loader_cli/teensy_loader_cli -mmcu=${MMCU} ${TARGET}.hex
}
