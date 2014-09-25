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

# Some defaults used for several targets.

if [ "${SYSARCH}" == "mac" ]; then
    DEFAULT_BOSSA_PORT=`find /dev/tty.usbmodem* | sort -n | tail -1`
else
    DEFAULT_BOSSA_PORT=`find /dev/ttyACM* | sort -n | tail -1`
fi

#####################################################################################

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

TARGETS+=( "megatronics3" )
target_megatronics3() {
    PLATFORM=avr
    F_CPU=16000000
    MCU=atmega2560
    AVRDUDE_PORT=/dev/ttyACM0
    AVRDUDE_BAUDRATE=115200
    AVRDUDE_PROGRAMMER=stk500v2
}

#####################################################################################

TARGETS+=( "rampsfd_udoo" )
target_rampsfd_udoo() {
    if [ -e /dev/ttymxc3 ]; then
        DEFAULT_BOSSA_PORT="/dev/ttymxc3"
    fi
    SOURCE_NAME=rampsfd
    PLATFORM=sam3x
    ASF_BOARD=43
    ARCH=sam3x
    SUBARCH=8
    SUBSUBARCH=e
    USE_USB_SERIAL=0
    BOSSA_PORT=${DEFAULT_BOSSA_PORT}
    BOSSA_USE_USB=0
    BOSSA_IS_ARDUINO_DUE=1
}

#####################################################################################

TARGETS+=( "radds_udoo" )
target_radds_udoo() {
    if [ -e /dev/ttymxc3 ]; then
        DEFAULT_BOSSA_PORT="/dev/ttymxc3"
    fi
    SOURCE_NAME=radds
    PLATFORM=sam3x
    ASF_BOARD=43
    ARCH=sam3x
    SUBARCH=8
    SUBSUBARCH=e
    USE_USB_SERIAL=0
    BOSSA_PORT=${DEFAULT_BOSSA_PORT}
    BOSSA_USE_USB=0
    BOSSA_IS_ARDUINO_DUE=1
}

#####################################################################################

TARGETS+=( "rampsfd" )
target_rampsfd() {
    PLATFORM=sam3x
    ASF_BOARD=43
    ARCH=sam3x
    SUBARCH=8
    SUBSUBARCH=e
    USE_USB_SERIAL=1
    BOSSA_PORT=${DEFAULT_BOSSA_PORT}
    BOSSA_USE_USB=0
    BOSSA_IS_ARDUINO_DUE=1
}

#####################################################################################

TARGETS+=( "radds" )
target_radds() {
    PLATFORM=sam3x
    ASF_BOARD=43
    ARCH=sam3x
    SUBARCH=8
    SUBSUBARCH=e
    USE_USB_SERIAL=1
    BOSSA_PORT=${DEFAULT_BOSSA_PORT}
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
    SUBSUBARCH=e
    USE_USB_SERIAL=1
    BOSSA_PORT=${DEFAULT_BOSSA_PORT}
    BOSSA_USE_USB=1
}

#####################################################################################

TARGETS+=( "mlab" )
target_mlab() {
    PLATFORM=sam3x
    ASF_BOARD=32
    ARCH=sam3s
    SUBARCH=2
    SUBSUBARCH=a
    AT91SAM_ADC_TRIGGER_ERRATUM=1
    USE_USB_SERIAL=1
    BOSSA_PORT=${DEFAULT_BOSSA_PORT}
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

TARGETS+=( "teensy3-corexy-laser" )
target_teensy3-corexy-laser() {
    SOURCE_NAME=teensy3-corexy-laser
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
