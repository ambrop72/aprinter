/*
 * Copyright (c) 2015 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

rec {
    melzi = {
        platform = "avr";
        targetVars = {
            F_CPU = "16000000";
            MCU = "atmega1284p";
        };
    };
    
    ramps13 = {
        platform = "avr";
        targetVars = {
            F_CPU = "16000000";
            MCU = "atmega2560";
        };
    };
    
    megatronics3 = {
        platform = "avr";
        targetVars = {
            F_CPU = "16000000";
            MCU = "atmega2560";
        };
    };
    
    arduino_due = {
        platform = "sam3x";
        targetVars = {
            ASF_BOARD = "43";
            ARCH = "sam3x";
            SUBARCH = "8";
            SUBSUBARCH = "e";
        };
    };
    
    rampsfd = arduino_due;
    
    radds = arduino_due;
    
    "4pi" = {
        platform = "sam3x";
        targetVars = {
            ASF_BOARD = "33";
            ARCH = "sam3u";
            SUBARCH = "4";
            SUBSUBARCH = "e";
        };
    };
    
    mlab = {
        platform = "sam3x";
        targetVars = {
            ASF_BOARD = "32";
            ARCH = "sam3s";
            SUBARCH = "2";
            SUBSUBARCH = "a";
            AT91SAM_ADC_TRIGGER_ERRATUM = "1";
        };
    };
    
    teensy3 = {
        platform = "teensy";
        targetVars = {
            F_CPU = "96000000";
            TEENSY_VERSION = "3.1";
        };
    };
    
    stm32f429 = {
        platform = "stm32f4";
        targetVars = {
            HSE_VALUE = "8000000";
            PLL_N_VALUE = "270";
            PLL_M_VALUE = "6";
            PLL_P_DIV_VALUE = "2";
            APB1_PRESC_DIV = "4";
            APB2_PRESC_DIV = "2";
        };
    };
}
