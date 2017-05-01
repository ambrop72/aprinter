/*
 * Copyright (c) 2017 Ambroz Bizjak
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

#include <avr/io.h>

/*
 * This file is preprocessed and then converted to definitions of
 * numeric register addresses.
 * 
 * The APRINTER_AVR_REG macro expands to values such as this:
 * 
 *   APRINTER_AVR_REG_PORTA:(*(volatile uint8_t *)((0X02) + 0x20));
 * 
 * The content between : and ; is the expansion of the register's macro
 * according to definitions in AVR libc. The script avr_reg_addr_gen.sh
 * will find these, extract the numeric addresses within and produce
 * definitions such as:
 * 
 *   static uint16_t const APrinter_AVR_PORTA_ADDR = (0X02) + 0x20;
 * 
 * The reason that this is needed is that simply using AVR libc's
 * _SFR_IO_ADDR/_SFT_MEM_ADDR macros may not work as a constant
 * expression, for cases where the address needs to be passed to
 * template parameters.
 */

#define APRINTER_AVR_REG(regname) \
APRINTER_AVR_REG_##regname:regname;

APRINTER_AVR_REG(PORTA)
APRINTER_AVR_REG(PORTB)
APRINTER_AVR_REG(PORTC)
APRINTER_AVR_REG(PORTD)
APRINTER_AVR_REG(PORTE)
APRINTER_AVR_REG(PORTF)
APRINTER_AVR_REG(PORTG)
APRINTER_AVR_REG(PORTH)
APRINTER_AVR_REG(PORTI)
APRINTER_AVR_REG(PORTJ)
APRINTER_AVR_REG(PORTK)
APRINTER_AVR_REG(PORTL)

APRINTER_AVR_REG(DDRA)
APRINTER_AVR_REG(DDRB)
APRINTER_AVR_REG(DDRC)
APRINTER_AVR_REG(DDRD)
APRINTER_AVR_REG(DDRE)
APRINTER_AVR_REG(DDRF)
APRINTER_AVR_REG(DDRG)
APRINTER_AVR_REG(DDRH)
APRINTER_AVR_REG(DDRI)
APRINTER_AVR_REG(DDRJ)
APRINTER_AVR_REG(DDRK)
APRINTER_AVR_REG(DDRL)

