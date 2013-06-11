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

#ifndef AMBROLIB_INT_SQRT_H
#define AMBROLIB_INT_SQRT_H

#include <stdint.h>

#include <aprinter/meta/IntTypeInfo.h>
#include <aprinter/meta/TypesAreEqual.h>
#ifdef AMBROLIB_AVR
#include <avr-asm-ops/sqrt_32_large.h>
#endif

#include <aprinter/BeginNamespace.h>

template <typename T>
struct IntSqrt {
    static_assert(!IntTypeInfo<T>::is_signed, "square root of signed not supported");
    static_assert(!TypesAreEqual<typename IntTypeInfo<T>::PrevType, void>::value, "square root of smallest integer type not supported");
    
    static typename IntTypeInfo<T>::PrevType call (T op)
    {
        T res = 0;
        T one = ((T)1) << (IntTypeInfo<T>::nonsign_bits - 2);
        
        while (one > op) {
            one >>= 2;
        }
        
        while (one != 0) {
            if (op >= res + one) {
                op = op - (res + one);
                res = res +  2 * one;
            }
            res >>= 1;
            one >>= 2;
        }
        
        return res;
    }
};

#ifdef AMBROLIB_AVR
template <>
struct IntSqrt<uint32_t> {
    static uint16_t call (uint32_t op)
    {
        return sqrt_32_large(op);
    }
};
#endif

#include <aprinter/EndNamespace.h>

#endif
