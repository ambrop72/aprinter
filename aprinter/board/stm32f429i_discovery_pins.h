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

#ifndef AMBROLIB_STM32F429I_DISCOVERY_PINS_H
#define AMBROLIB_STM32F4DISCOVERY_PINS_H

#include <aprinter/system/Stm32f4Pins.h>

#include <aprinter/BeginNamespace.h>

using DiscoveryPinLedGreen = Stm32f4Pin<Stm32f4PortG, 13>;
using DiscoveryPinLedRed = Stm32f4Pin<Stm32f4PortG, 14>;
using DiscoveryPinButtonUser = Stm32f4Pin<Stm32f4PortA, 0>;

/*
List of internally connected pins:

// MEMS
PF7 PF8 PF9 PC1 PA2 PA1

// LEDs
PG13 PG14

// User button
PA0

// USB
PC5 PC4 PB15 PB14 PB13 PB12

// LCD
PA4 PC2 PC6 PD11 PD12 PD13 PF7 PF9 PF10 PG7
PC10 PB0 PA11 PA12 PB1 PG6
PA6 PG10 PB10 PB11 PC7 PD3
PD6 PG11 PG12 PA3 PB8 PB9

// I2C/LCD
PA8 PC9 PA15 PA7

// SDRAM
PB5 PB6 PC0 PE0 PE1 PF11 PG4 PG5 PG8 PG15
PF0 PF1 PF2 PF3 PF4 PF5 PF12 PF13 PF14 PF15 PG0 PG1
PD14 PD15 PD0 PD1 PE7 PE8 PE9 PE10 PE11 PE12 PE13 PE14 PE15 PD8 PD9 PD10

// Oscillator
PH0 PH1

// ST-LINK
PB3 PA13 PA14

*/

#include <aprinter/EndNamespace.h>

#endif
