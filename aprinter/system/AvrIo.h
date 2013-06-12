/*
 * Copyright (c) 2013 Ambroz Bizjak
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

#ifndef AMBROLIB_AVR_IO_H
#define AMBROLIB_AVR_IO_H

#include <avr/sfr_defs.h>

#include <stdint.h>

#include <aprinter/BeginNamespace.h>

template <uint32_t IoAddr>
void avrSetReg (uint8_t value)
{
    _SFR_IO8(IoAddr) = value;
}

template <uint32_t IoAddr>
void avrSetReg16 (uint16_t value)
{
    _SFR_IO16(IoAddr) = value;
}

template <uint32_t IoAddr>
uint8_t avrGetReg ()
{
    return _SFR_IO8(IoAddr);
}

template <uint32_t IoAddr>
uint16_t avrGetReg16 ()
{
    return _SFR_IO16(IoAddr);
}

template <uint32_t IoAddr>
void avrSetBitReg (uint8_t bit)
{
    asm("sbi %0,%1" :: "i" (IoAddr), "i" (bit));
}

template <uint32_t IoAddr>
void avrClearBitReg (uint8_t bit)
{
    asm("cbi %0,%1" :: "i" (IoAddr), "i" (bit));
}

template <uint32_t IoAddr>
void avrSoftSetBitReg (uint8_t bit)
{
    _SFR_IO8(IoAddr) |= (1 << bit);
}

template <uint32_t IoAddr>
void avrSoftClearBitReg (uint8_t bit)
{
    _SFR_IO8(IoAddr) &= ~(1 << bit);
}

#include <aprinter/EndNamespace.h>

#endif
