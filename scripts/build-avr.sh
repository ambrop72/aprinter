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

AVR_CC=${AVR_GCC_PREFIX}gcc
AVR_OBJCOPY=${AVR_GCC_PREFIX}objcopy
AVR_SIZE=${AVR_GCC_PREFIX}size

check_depends_avr() {
    echo "   Checking depends"
    check_build_tool "${AVR_CC}" "AVR compiler"
    check_build_tool "${AVR_OBJCOPY}" "AVR objcopy"
    check_build_tool "${AVR_SIZE}" "AVR size calculator"
}

configure_avr() {
    echo "  Configuring AVR build"

    FLAGS_OPT=( -O$( [[ $OPTIMIZE_FOR_SIZE = "1" ]] && echo s || echo 2 ) )
    CXXFLAGS=(
        -std=c++14 -mmcu=${MCU} -DF_CPU=${F_CPU} -DNDEBUG "${FLAGS_OPT[@]}" -fwhole-program \
        -fno-math-errno -fno-trapping-math
        -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -fno-access-control \
        -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS \
        -DAMBROLIB_AVR -I. -I"$BUILD" -Wfatal-errors
        ${CXXFLAGS} ${CCXXLDFLAGS}
    )
    
    RUNBUILD=build_avr
    CHECK=check_depends_avr
    
    REG_ADDR_PREPROCESS=aprinter/platform/avr/avr_reg_addr_preprocess.h
    REG_ADDR_PREPROCESS_OUT=$BUILD/avr_reg_addr_preprocess.i
    REG_ADDR_GEN=aprinter/platform/avr/avr_reg_addr_gen.sh
    REG_ADDR_GEN_OUT=$BUILD/aprinter_avr_reg_addrs.h
}

build_avr() {
    echo "  Compiling for AVR"
    ${CHECK}
    
    extract_register_addresses
    
    echo "   Compiling and linking"
    ($V; "$AVR_CC" -x c++ "${CXXFLAGS[@]}" "$SOURCE" -o "$TARGET.elf" -Wl,-u,vfprintf -lprintf_flt || exit 2)
    
    echo "   Building images"
    ($V; "$AVR_OBJCOPY" -j .text -j .data -O ihex "$TARGET.elf" "$TARGET.hex" || exit 2)
    
    if "$AVR_SIZE" --help | grep 'format.*avr'; then
        echo -n "   Size of build: "
        "$AVR_SIZE" --format=avr --mcu=${MCU} "${TARGET}.elf" | grep bytes | sed 's/\(.*\):\s*\(.* bytes.* Full\)/\1: \2/g' | sed 'N;s/\n/\; /' | sed 's/\s\s*/ /g'
    else
        echo "   Size of build: "
        "$AVR_SIZE" "${TARGET}.elf" | sed 's/^/    /'
    fi
}

extract_register_addresses() {
    echo "   Extracting register addresses"
    
    # Preprocess avr_reg_addr_helper.h (-P to get no line number comments).
    ($V; "$AVR_CC" -E -P -x c++ "${CXXFLAGS[@]}" "$REG_ADDR_PREPROCESS" -o "$REG_ADDR_PREPROCESS_OUT" || exit 2)
    
    # Generate the header using another script.
    ($V; "$BASH" "$REG_ADDR_GEN" "$REG_ADDR_PREPROCESS_OUT" "$REG_ADDR_GEN_OUT" || exit 2)
}
