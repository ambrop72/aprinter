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
    STM32CUBEF4_DIR=${CUSTOM_STM32CUBEF4}
    
    CMSIS_DIR=${STM32CUBEF4_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx
    TEMPLATES_DIR=${CMSIS_DIR}/Source/Templates
    HAL_DIR=${STM32CUBEF4_DIR}/Drivers/STM32F4xx_HAL_Driver
    # TBD: This linker script has an inappropriate license.
    LINKER_SCRIPT=${STM32CUBEF4_DIR}/Projects/STM32F429I-Discovery/Templates/TrueSTUDIO/STM32F429I_DISCO/STM32F429ZI_FLASH.ld

    ARM_CPU=cortex-m4

    configure_arm

    FLAGS_C_CXX_LD+=(
        -mfpu=fpv4-sp-d16 -mfloat-abi=hard
    )
    FLAGS_C_CXX+=(
        -DSTM32F429xx -DUSE_HAL_DRIVER
        -DHSE_VALUE=${HSE_VALUE} -DPLL_N_VALUE=${PLL_N_VALUE} -DPLL_M_VALUE=${PLL_M_VALUE} -DPLL_P_DIV_VALUE=${PLL_P_DIV_VALUE}
        -DAPB1_PRESC_DIV=${APB1_PRESC_DIV} -DAPB2_PRESC_DIV=${APB2_PRESC_DIV}
        -I aprinter/platform/stm32f4
        -I "${CMSIS_DIR}/Include"
        -I "${STM32CUBEF4_DIR}/Drivers/CMSIS/Include"
        -I "${HAL_DIR}/Inc"
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
    )
    ASM_SOURCES+=(
        "${TEMPLATES_DIR}/gcc/startup_stm32f429xx.s"
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
