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

#ifndef AMBROLIB_STM32F4DISCOVERY_PINS_H
#define AMBROLIB_STM32F4DISCOVERY_PINS_H

#include <aprinter/hal/stm32/Stm32f4Pins.h>

namespace APrinter {

using DiscoveryPinLedGreen = Stm32f4Pin<Stm32f4PortD, 12>;
using DiscoveryPinLedOrange = Stm32f4Pin<Stm32f4PortD, 13>;
using DiscoveryPinLedRed = Stm32f4Pin<Stm32f4PortD, 14>;
using DiscoveryPinLedBlue = Stm32f4Pin<Stm32f4PortD, 15>;
using DiscoveryPinButtonUser = Stm32f4Pin<Stm32f4PortA, 0>;

/*
List of internally connected pins or otherwise possibly used:

// MEMS
PE1 PE0 PA5 PA7 PA6 PE3

// LEDs
PD12 PD13 PD14 PD15

// User button
PA0

// USB
PA9 PA11 PA12 PA10 PC0 PD5

// Audio
PC3 PB10 PD4 PB9 PB6 PC7 PC10 PC12 PA4 PC4

// Oscillator
PH0 PH1

// ST-LINK
PB3 PA13 PA14

// SD card (if used)
PC8 PC9 PC10 PC11 PC12 PD2
Optional for card detection: PA8
*/

}

#endif
