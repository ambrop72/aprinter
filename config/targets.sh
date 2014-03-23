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
# Targets configuration

TARGETS+=( "melzi" )
target_melzi() {
    PLATFORM=avr
    F_CPU=16000000 
    MCU=atmega1284p
    AVRDUDE_PORT=/dev/ttyUSB0
    AVRDUDE_BAUDRATE=57600
    AVRDUDE_PROGRAMMER=stk500v1
}

#####################################################################################

TARGETS+=( "ramps13" )
target_ramps13() {
    PLATFORM=avr
    F_CPU=16000000
    MCU=atmega2560
    AVRDUDE_PORT=/dev/ttyACM0
    AVRDUDE_BAUDRATE=115200
    AVRDUDE_PROGRAMMER=stk500v2
}

#####################################################################################

TARGETS+=( "rampsfd" )
target_rampsfd() {
    PLATFORM=sam3x
    ASF_BOARD=43
    ARCH=sam3x
    SUBARCH=8
    USE_USB_SERIAL=0
    BOSSA_PORT=/dev/ttyACM0
    BOSSA_USE_USB=0
    BOSSA_IS_ARDUINO_DUE=1
}

#####################################################################################

TARGETS+=( "4pi" )
target_4pi() {
    PLATFORM=sam3x
    ASF_BOARD=33
    ARCH=sam3u
    SUBARCH=4
    USE_USB_SERIAL=1
    BOSSA_PORT=/dev/ttyACM0
    BOSSA_USE_USB=1
}

#####################################################################################

TARGETS+=( "teensy3" )
target_teensy3() {
    PLATFORM=teensy
    F_CPU=96000000
    TEENSY_VERSION=3.1
}

#####################################################################################

TARGETS+=( "stm32f4" )
target_stm32f4() {
    PLATFORM=stm
}

#####################################################################################
