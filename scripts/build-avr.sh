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

ATMEL_AVR_GCC_PATH="${ROOT}/depends/avr8-gnu-toolchain-linux_x86"

if [ -n "${CUSTOM_AVR_GCC}" ]; then
    AVR_GCC_PREFIX=${CUSTOM_AVR_GCC}
else
    AVR_GCC_PREFIX=${ATMEL_AVR_GCC_PATH}/bin/avr-
fi

AVR_CC=${AVR_GCC_PREFIX}gcc
AVR_OBJCOPY=${AVR_GCC_PREFIX}objcopy
AVR_SIZE=${AVR_GCC_PREFIX}size

if [ -n "${CUSTOM_AVRDUDE}" ]; then
    AVRDUDE=${CUSTOM_AVRDUDE}
else
    AVRDUDE=avrdude
fi

ATMEL_AVR_GCC_URL=(
    "http://www.atmel.com/images/avr8-gnu-toolchain-3.4.3.1072-linux.any.x86.tar.gz"
)
ATMEL_AVR_GCC_CHECKSUM=(
    "fa815c9e966b67353a16fb37b78e4b7d3e4eec72e8416f2d933a89262a46cbfb  avr8-gnu-toolchain-3.4.3.1072-linux.any.x86.tar.gz"
)

install_avr() {
    if [ -z "${CUSTOM_AVR_GCC}" ]; then
        echo "  Installing Atmel AVR toolchain"
        [ -f "${AVR_CC}" ] && \
        [ -f "${AVR_OBJCOPY}" ] && echo "   [!] Atmel AVR toolchain already installed" && return 0

        create_depends_dir
        retr_and_extract ATMEL_AVR_GCC_URL[@] ATMEL_AVR_GCC_CHECKSUM[@]
    fi
}

flush_avr() {
    clean
    echo "  Deleting Atmel AVR toolchain install. Are you sure? (C-c to abort)"
    read 
    rm -rf "${ATMEL_AVR_GCC_PATH}"
}

check_depends_avr() {
    echo "   Checking depends"
    check_build_tool "${AVR_CC}" "AVR compiler"
    check_build_tool "${AVR_OBJCOPY}" "AVR objcopy"
    check_build_tool "${AVR_SIZE}" "AVR size calculator"
    check_build_tool "${AVRDUDE}" "AVR uploader 'avrdude'"
}

configure_avr() {
    echo "  Configuring AVR build"

    CXXFLAGS=(
        -std=c++11 -mmcu=${MCU} -DF_CPU=${F_CPU} -DNDEBUG -O2 -fwhole-program \
        -fno-math-errno -fno-trapping-math
        -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS \
        -DAMBROLIB_AVR -I.
        ${CXXFLAGS}
    )
    
    AVRDUDE_FLAGS=(
        -p $MCU -D -P $AVRDUDE_PORT -b $AVRDUDE_BAUDRATE -c $AVRDUDE_PROGRAMMER
    )
    
    INSTALL=install_avr
    RUNBUILD=build_avr
    UPLOAD=upload_avr
    FLUSH=flush_avr
    CHECK=check_depends_avr
}

build_avr() {
    echo "  Compiling for AVR"
    ${CHECK}
    
    echo "   Compiling and linking"
    ($V; "$AVR_CC" -x c++ "${CXXFLAGS[@]}" "$SOURCE" -o "$TARGET.elf" -Wl,-u,vfprintf -lprintf_flt || exit 2)
    
    echo "   Building images"
    ($V; "$AVR_OBJCOPY" -j .text -j .data -O ihex "$TARGET.elf" "$TARGET.hex" || exit 2)
    
    echo -n "   Size of build: "
    "$AVR_SIZE" --format=avr --mcu=${MCU} "${TARGET}.elf" | grep bytes | sed 's/\(.*\):\s*\(.* bytes.* Full\)/\1: \2/g' | sed 'N;s/\n/\; /' | sed 's/\s\s*/ /g'
}

upload_avr() {
    echo "  Uploading to AVR"
    
    ($V; "$AVRDUDE" "${AVRDUDE_FLAGS[@]}" -U "flash:w:$TARGET.hex:i" || exit 3)
}
