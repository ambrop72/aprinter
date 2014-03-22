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
# STM SPECIFIC STUFF


STM32F4_URL=(
    "http://www.st.com/st-web-ui/static/active/en/st_prod_software_internet/resource/technical/software/firmware/stm32f4_dsp_stdperiph_lib.zip"
    "http://www.st.com/st-web-ui/static/active/en/st_prod_software_internet/resource/technical/software/firmware/stm32_f105-07_f2_f4_usb-host-device_lib.zip"
)
STM32F4_CHECKSUMS=(
    "7fd6a73b022535a9d7fd0d2dd67127ce7abf42d737c0b8da358ba81f3deb02b4  stm32_f105-07_f2_f4_usb-host-device_lib.zip"
    "e50106475b15657913b7eac98e294f9428c26eef5993b1a203f12bebfd5a869a  stm32f4_dsp_stdperiph_lib.zip"
)

configure_stm() {
    STM32F4_DIR=${DEPS}/STM32F4xx_DSP_StdPeriph_Lib_V1.3.0
    STM_USB_DIR=${DEPS}/STM32_USB-Host-Device_Lib_V2.1.0

    CMSIS_DIR=${STM32F4_DIR}/Libraries/CMSIS/Device/ST/STM32F4xx
    TEMPLATES_DIR=${CMSIS_DIR}/Source/Templates
    STDPERIPH=${STM32F4_DIR}/Libraries/STM32F4xx_StdPeriph_Driver
    LINKER_SCRIPT=${STM32F4_DIR}/Project/STM32F4xx_StdPeriph_Templates/RIDE/stm32f4xx_flash.ld

    configure_arm

    FLAGS_C_CXX_LD+=(
        -mfpu=fpv4-sp-d16
    )
    FLAGS_C_CXX+=(
        -DSTM32F40_41xxx -DHSE_VALUE=8000000 -D"assert_param(x)"
        -I aprinter/platform/stm32f4
        -I "${CMSIS_DIR}/Include"
        -I "${STM32F4_DIR}/Libraries/CMSIS/Include"
        -I "${STDPERIPH}/inc"
        -I "${STM_USB_DIR}/Libraries/STM32_USB_OTG_Driver/inc"
    )

    CXX_SOURCES+=(
        "aprinter/platform/stm32f4/stm32f4_support.cpp"
    )
    C_SOURCES+=(
        "${BUILD}/system_stm32f4xx-hacked.c"
        "${STDPERIPH}/src/stm32f4xx_rcc.c"
    )
    ASM_SOURCES+=(
        "${TEMPLATES_DIR}/gcc_ride7/startup_stm32f40xx.s"
    )

    # define target functions
    INSTALL=install_stm
    RUNBUILD=build_stm
    UPLOAD=upload_stm
    CLEAN=clean_stm
    FLUSH=flush_stm
    CHECK=check_depends_stm
}

build_stm() {
    cp -f "${TEMPLATES_DIR}/system_stm32f4xx.c" ${BUILD}/system_stm32f4xx-hacked.c
    sed 's/#define PLL_M      25/#define PLL_M      8/' -i ${BUILD}/system_stm32f4xx-hacked.c
    build_arm
}

check_depends_stm() {
    check_depends_arm
    [ -d ${STM32F4_DIR} ] || fail "STM32F4 framework missing in dependences"
    [ -d ${STM_USB_DIR} ] || fail "STM USB framework missing in dependences"
    [ -d ${DEPS}/stlink ] || fail "STM upload tool 'stlink' missing"
}

clean_stm() {
    clean_arm
    ($V; rm -f ${BUILD}/system_stm32f4xx-hacked.c)
}

flush_stm() {
    flush_arm
    echo "  Flushing STM32F4 toolchain"
    (V; 
    rm -rf ${DEPS}/${STM32F4_DIR}
    rm -rf ${DEPS}/${STM_USB_DIR}
    rm -rf ${DEPS}/stlink)
}

install_stm() {
    install_arm

    [ -d ${STM32F4_DIR} ] && \
    [ -d ${STM_USB_DIR} ] && \
    [ -d ${DEPS}/stlink ] && \
        echo "   [!] STM32F4 toolchain already installed" && return 0

    retr_and_extract STM32F4_URL[@] STM32F4_CHECKSUMS[@]

    (
        cd ${DEPS}

        git clone https://github.com/texane/stlink 
        cd stlink
        ./autogen.sh && ./configure && make
    )
    echo "  Installation of STM32F4 Toolchain and STLink: success"
}

upload_stm() {
    echo 
}

