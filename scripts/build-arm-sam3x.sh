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
# SAM3X stuff

sam3x_to_upper() {
    echo "$1"| tr 'a-z' 'A-Z'
}

configure_sam3x() {
    CMSIS_DIR=${ASF_DIR}/sam/utils/cmsis/${ARCH}
    TEMPLATES_DIR=${CMSIS_DIR}/source/templates
    LINKER_SCRIPT=${ASF_DIR}/sam/utils/linker_scripts/${ARCH}/${ARCH}${SUBARCH}/gcc/flash.ld

    ARM_CPU=cortex-m3

    configure_arm

    FLAGS_C_CXX+=(
        -D__$(sam3x_to_upper "$ARCH")$(sam3x_to_upper "$SUBARCH")$(sam3x_to_upper "$SUBSUBARCH")__ -DHEAP_SIZE=16384
        -DBOARD=${ASF_BOARD}
        -I"${CMSIS_DIR}/include"
        -I"${TEMPLATES_DIR}"
        -I"${ASF_DIR}/sam/utils"
        -I"${ASF_DIR}/sam/utils/preprocessor"
        -I"${ASF_DIR}/sam/utils/header_files"
        -I"${ASF_DIR}/sam/boards"
        -I"${ASF_DIR}/sam/drivers/pmc"
        -I"${ASF_DIR}/sam/drivers/pio"
        -I"${ASF_DIR}/sam/drivers/dmac"
        -I"${ASF_DIR}/sam/drivers/emac"
        -I"${ASF_DIR}/sam/drivers/rstc"
        -I"${ASF_DIR}/common/utils"
        -I"${ASF_DIR}/common/services/usb"
        -I"${ASF_DIR}/common/services/usb/udc"
        -I"${ASF_DIR}/common/services/clock"
        -I"${ASF_DIR}/common/services/sleepmgr"
        -I"${ASF_DIR}/common/services/ioport"
        -I"${ASF_DIR}/common/services/usb/class/cdc"
        -I"${ASF_DIR}/common/services/usb/class/cdc/device"
        -I"${ASF_DIR}/common/boards"
        -I"${ASF_DIR}/thirdparty/CMSIS/Include"
        -I"${ASF_DIR}"
        -I aprinter/platform/at91${ARCH}
    )
    
    if [ "$AT91SAM_ADC_TRIGGER_ERRATUM" = "1" ]; then
        FLAGS_C_CXX+=(-DAT91SAMADC_TRIGGER_ERRATUM)
    fi

    C_SOURCES+=(
        "${TEMPLATES_DIR}/exceptions.c"
        "${TEMPLATES_DIR}/system_${ARCH}.c"
        "${TEMPLATES_DIR}/gcc/startup_${ARCH}.c"
        "${ASF_DIR}/sam/drivers/pmc/pmc.c"
        "${ASF_DIR}/sam/drivers/pmc/sleep.c"
        "${ASF_DIR}/sam/drivers/dmac/dmac.c"
        "${ASF_DIR}/sam/drivers/rstc/rstc.c"
        "${ASF_DIR}/common/services/clock/${ARCH}/sysclk.c"
        "${ASF_DIR}/common/utils/interrupt/interrupt_sam_nvic.c"
        "aprinter/platform/newlib_common.c"
    )
    CXX_SOURCES+=(
        "aprinter/platform/at91${ARCH}/at91${ARCH}_support.cpp"
    )

    if [ $USE_USB_SERIAL -gt 0 ]; then
        if [ "$ARCH" = "sam3x" ]; then
            FLAGS_C_CXX+=( -DUSB_SERIAL )
            C_SOURCES+=(
                "${ASF_DIR}/sam/drivers/uotghs/uotghs_device.c"
            )
        elif [ "$ARCH" = "sam3u" ]; then
            C_SOURCES+=(
                "${ASF_DIR}/sam/drivers/udphs/udphs_device.c"
            )
        elif [ "$ARCH" = "sam3s" ]; then
            C_SOURCES+=(
                "${ASF_DIR}/sam/drivers/udp/udp_device.c"
            )
        fi

        C_SOURCES+=(
            "${ASF_DIR}/common/services/usb/udc/udc.c"
            "${ASF_DIR}/common/services/usb/class/cdc/device/udi_cdc.c"
            "${ASF_DIR}/common/services/usb/class/cdc/device/udi_cdc_desc.c"
        )
    fi
    
    # define target functions
    RUNBUILD=build_sam3x
    CHECK=check_depends_sam3x
}

check_depends_sam3x() {
    check_depends_arm
    [ -d "${ASF_DIR}" ] || fail "Atmel Software Framework missing"
}

build_sam3x() {
    build_arm
}
