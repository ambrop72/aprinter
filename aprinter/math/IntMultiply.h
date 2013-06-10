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

#ifndef AMBROLIB_INT_MULTIPLY_H
#define AMBROLIB_INT_MULTIPLY_H

#include <stdint.h>

#include <aprinter/meta/IntTypeInfo.h>

#include <aprinter/BeginNamespace.h>

template <typename T1, typename T2, typename TR, typename Void = void>
struct IntMultiply {
    static_assert(IntTypeInfo<T1>::is_signed == IntTypeInfo<T2>::is_signed,
                  "signedness of multiplication operands must match");
    static_assert(IntTypeInfo<TR>::is_signed == IntTypeInfo<T1>::is_signed,
                  "signedness of multiplication result must match operands");
    
    static TR call (T1 op1, T2 op2)
    {
        return ((TR)op1 * (TR)op2);
    }
};

#if defined(AMBROLIB_AVR) && 0

template <>
struct IntMultiply<int16_t, int16_t, int32_t> {
    static int32_t call (int16_t op1, int16_t op2)
    {
        int32_t res;
        asm(
            "clr r26 \n\t"
            "mul %A1, %A2 \n\t"
            "movw %A0, r0 \n\t"
            "muls %B1, %B2 \n\t"
            "movw %C0, r0 \n\t"
            "mulsu %B2, %A1 \n\t"
            "sbc %D0, r26 \n\t"
            "add %B0, r0 \n\t"
            "adc %C0, r1 \n\t"
            "adc %D0, r26 \n\t"
            "mulsu %B1, %A2 \n\t"
            "sbc %D0, r26 \n\t"
            "add %B0, r0 \n\t"
            "adc %C0, r1 \n\t"
            "adc %D0, r26 \n\t"
            "clr r1 \n\t"
            :
            "=&r" (res)
            :
            "a" (op1),
            "a" (op2)
            :
            "r26"
        );
        return res;
    }
};

template <>
struct IntMultiply<int16_t, uint16_t, int32_t> {
    static int32_t call (int16_t op1, uint16_t op2)
    {
        int32_t res;
        asm(
            "clr r26 \n\t"
            "mul %A1, %A2 \n\t"
            "movw %A0, r0 \n\t"
            "mulsu %B1, %B2 \n\t"
            "movw %C0, r0 \n\t"
            "mul %B2, %A1 \n\t"
            "add %B0, r0 \n\t"
            "adc %C0, r1 \n\t"
            "adc %D0, r26 \n\t"
            "mulsu %B1, %A2 \n\t"
            "sbc %D0, r26 \n\t"
            "add %B0, r0 \n\t"
            "adc %C0, r1 \n\t"
            "adc %D0, r26 \n\t"
            "clr r1 \n\t"
            :
            "=&r" (res)
            :
            "a" (op1),
            "a" (op2)
            :
            "r26"
        );
        return res;
    }
};

template <>
struct IntMultiply<uint16_t, uint16_t, uint32_t> {
    static uint32_t call (uint16_t op1, uint16_t op2)
    {
        uint32_t res;
        asm(
            "clr r26 \n\t"
            "mul %A1, %A2 \n\t"
            "movw %A0, r0 \n\t"
            "mul %B1, %B2 \n\t"
            "movw %C0, r0 \n\t"
            "mul %B2, %A1 \n\t"
            "add %B0, r0 \n\t"
            "adc %C0, r1 \n\t"
            "adc %D0, r26 \n\t"
            "mul %B1, %A2 \n\t"
            "add %B0, r0 \n\t"
            "adc %C0, r1 \n\t"
            "adc %D0, r26 \n\t"
            "clr r1 \n\t"
            :
            "=&r" (res)
            :
            "a" (op1),
            "a" (op2)
            :
            "r26"
        );
        return res;
    }
};

#endif

#include <aprinter/EndNamespace.h>

#endif
