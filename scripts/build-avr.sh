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
# AVR SPECIFIC STUFF

: ${AVR_GCC_PREFIX:="avr-"}
: ${AVR_GCC_PATH="/usr/local/"}

AVR_TOOLCHAIN_URL=(
    "http://ftp.gnu.org/gnu/binutils/binutils-2.23.2.tar.bz2"
    "ftp://ftp.gnu.org/gnu/gcc/gcc-4.8.2/gcc-4.8.2.tar.bz2"
)
AVR_TOOLCHAIN_CHECKSUMS=(
    ""
    ""
)

install_avr() {
    echo "  Installing AVR toolchain"
    echo "   [X] Not implemented at the moment (use system installs)"
    return 0
    create_depends_dir
    retr_and_extract AVR_TOOLCHAIN_URL[@] AVR_TOOLCHAIN_CHECKSUMS[@]

    (
        cd ${DEPS}
        (
            cd binutils-2.23.2
            ./configure && make # TODO
        )
        (
            cd gcc-4.8.2
            ./configure && make # TODO
        )
        # TODO install avrdude
    )
}

flush_avr() {
    echo "  Flushing AVR toolchain. Are you sure? (C-c to abort)"
    read

    (V;
    rm -rf ${DEPS}/binutils-2.23.2
    rm -rf ${DEPS}/gcc-4.8.2
    )
}

check_depends_avr() {
    echo -n "   Checking depends: "
    [ -f ${AVR_GXX} ] || fail "Missing AVR compiler" 
    [ -f ${AVR_OBJCOPY} ] || fail "Missing AVR objcopy"
    [ -f ${AVR_SIZE} ] || fail "Missing AVR size calculator"
    [ -f ${AVRDUDE} ] || fail "Missing AVR uploader 'avrdude'"
    echo "ok"
}

configure_avr() {
    echo "  Configuring AVR build"

    AVR_GXX=${AVR_GCC_PATH}/bin/${AVR_GCC_PREFIX}g++
    AVR_OBJCPY=${AVR_GCC_PATH}/bin/avr-objcopy
    AVR_SIZE=${AVR_GCC_PATH}/bin/avr-size

    AVRDUDE=${AVR_GCC_PATH}/bin/avrdude
    AVRDUDE_PORT=/dev/ttyUSB0
    AVRDUDE_BAUDRATE=57600
    AVRDUDE_PROGRAMMER=stk500v1

    CXX=${AVR_GXX}
    OBJCPY=${AVR_OBJCPY}
    AVRDUDE_FLAGS="-p $MCU -D -P $AVRDUDE_PORT -b $AVRDUDE_BAUDRATE -c $AVRDUDE_PROGRAMMER"
    CXXFLAGS="-std=c++11 -mmcu=${MCU} -DF_CPU=${F_CPU} -DNDEBUG -O2 -fwhole-program \
    -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS \
    -DAMBROLIB_AVR -I. $CXXFLAGS_EXTRA"

    # define target functions
    INSTALL=install_avr
    RUNBUILD=build_avr
    UPLOAD=upload_avr
}

build_avr() {
    echo "  Compiling for AVR"
    check_depends_avr
    echo "   Compiling and linking"
    ($V; $CXX $CXXFLAGS $SOURCE -o $TARGET.elf -Wl,-u,vfprintf -lprintf_flt || exit 2)
    echo "   Building images"
    ($V; $OBJCPY -j .text -j .data -O ihex $TARGET.elf $TARGET.hex || exit 2)
    echo -n "   Size of build: "
    $AVR_SIZE --format=avr --mcu=${MCU} ${TARGET}.elf | grep bytes | sed 's/\(.*\):\s*\(.* bytes.* Full\)/\1: \2/g' | sed 'N;s/\n/\; /' | sed 's/\s\s*/ /g'
}

upload_avr() {
    echo "  Uploading to AVR"
    ($V; $AVRDUDE $AVRDUDE_FLAGS -U flash:w:$TARGET.hex:i || exit 3)
}

