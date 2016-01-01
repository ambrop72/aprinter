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
# STM32F4 SPECIFIC STUFF

configure_stm32f4() {
    CMSIS_DIR=${STM32CUBEF4_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx
    TEMPLATES_DIR=${CMSIS_DIR}/Source/Templates
    HAL_DIR=${STM32CUBEF4_DIR}/Drivers/STM32F4xx_HAL_Driver
    USB_DIR=${STM32CUBEF4_DIR}/Middlewares/ST/STM32_USB_Device_Library

    ARM_CPU=cortex-m4

    if [[ $STM_CHIP = "stm32f429" ]]; then
        CHIP_FLAGS=( -DSTM32F429xx )
        STARTUP_ASM_FILE=startup_stm32f429xx.s
    elif [[ $STM_CHIP = "stm32f407" ]]; then
        CHIP_FLAGS=( -DSTM32F407xx )
        STARTUP_ASM_FILE=startup_stm32f407xx.s
    else
        fail "Unsupported STM_CHIP"
    fi

    LINKER_SCRIPT=aprinter/platform/stm32f4/${STM_CHIP}.ld
    
    configure_arm
    
    USB_FLAGS=()
    USB_C_SOURCES=()
    if [[ -n $USB_MODE ]]; then
        USB_FLAGS=( -DAPRINTER_ENABLE_USB )
        USB_C_SOURCES=(
            "${HAL_DIR}/Src/stm32f4xx_hal_pcd.c"
            "${HAL_DIR}/Src/stm32f4xx_hal_pcd_ex.c"
            "${HAL_DIR}/Src/stm32f4xx_ll_usb.c"
            "${USB_DIR}/Core/Src/usbd_core.c"
            "${USB_DIR}/Core/Src/usbd_ctlreq.c"
            "${USB_DIR}/Core/Src/usbd_ioreq.c"
            "${USB_DIR}/Class/CDC/Src/usbd_cdc.c"
            "aprinter/platform/stm32f4/usbd_conf.c"
            "aprinter/platform/stm32f4/usbd_desc.c"
        )
        
        if [[ $USB_MODE = "FS" ]]; then
            USB_FLAGS=( "${USB_FLAGS[@]}" -DUSE_USB_FS )
        elif [[ $USB_MODE = "HS" ]]; then
            USB_FLAGS=( "${USB_FLAGS[@]}" -DUSE_USB_HS )
        elif [[ $USB_MODE = "HS-in-FS" ]]; then
            USB_FLAGS=( "${USB_FLAGS[@]}" -DUSE_USB_HS -DUSE_USB_HS_IN_FS )
        else
            fail "Invalid USB_MODE"
        fi
    fi
    
    SDCARD_C_SOURCES=()
    if [[ -n $ENABLE_SDCARD ]]; then
        SDCARD_C_SOURCES=(
            "${HAL_DIR}/Src/stm32f4xx_hal_sd.c"
            "${HAL_DIR}/Src/stm32f4xx_ll_sdmmc.c"
        )
    fi
    
    FLAGS_C_CXX_LD+=(
        -mfpu=fpv4-sp-d16 -mfloat-abi=hard
    )
    FLAGS_C_CXX+=(
        "${CHIP_FLAGS[@]}"
        -DUSE_HAL_DRIVER -DHEAP_SIZE=16384
        -DHSE_VALUE=${HSE_VALUE} -DPLL_N_VALUE=${PLL_N_VALUE} -DPLL_M_VALUE=${PLL_M_VALUE}
        -DPLL_P_DIV_VALUE=${PLL_P_DIV_VALUE} -DPLL_Q_DIV_VALUE=${PLL_Q_DIV_VALUE}
        -DAPB1_PRESC_DIV=${APB1_PRESC_DIV} -DAPB2_PRESC_DIV=${APB2_PRESC_DIV}
        "${USB_FLAGS[@]}"
        -I aprinter/platform/stm32f4
        -I "${CMSIS_DIR}/Include"
        -I "${STM32CUBEF4_DIR}/Drivers/CMSIS/Include"
        -I "${HAL_DIR}/Inc"
        -I "${USB_DIR}/Core/Inc"
        -I "${USB_DIR}/Class/CDC/Inc"
    )
    
    CXX_SOURCES+=(
        "aprinter/platform/stm32f4/stm32f4_support.cpp"
    )
    C_SOURCES+=(
        "${TEMPLATES_DIR}/system_stm32f4xx.c"
        "${HAL_DIR}/Src/stm32f4xx_hal.c"
        "${HAL_DIR}/Src/stm32f4xx_hal_cortex.c"
        "${HAL_DIR}/Src/stm32f4xx_hal_rcc.c"
        "${HAL_DIR}/Src/stm32f4xx_hal_iwdg.c"
        "${HAL_DIR}/Src/stm32f4xx_hal_gpio.c"
        "${HAL_DIR}/Src/stm32f4xx_hal_dma.c"
        "aprinter/platform/newlib_common.c"
        "${USB_C_SOURCES[@]}" "${SDCARD_C_SOURCES[@]}"
    )
    ASM_SOURCES+=(
        "${TEMPLATES_DIR}/gcc/${STARTUP_ASM_FILE}"
    )

    # define target functions
    RUNBUILD=build_stm32f4
    CHECK=check_depends_stm32f4
}

build_stm32f4() {
    build_arm
}

check_depends_stm32f4() {
    check_depends_arm
    [ -d "${STM32CUBEF4_DIR}" ] || fail "STM32F4 framework missing in dependences"
}
