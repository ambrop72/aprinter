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

#ifndef AMBROLIB_INT_DIVIDE_H
#define AMBROLIB_INT_DIVIDE_H

#include <stdint.h>

#include <aprinter/meta/IntTypeInfo.h>
#include <aprinter/base/Assert.h>
#ifdef AMBROLIB_AVR
#include "../avr-asm-ops/div_32_32_large.h"
#include "../avr-asm-ops/div_32_16_large.h"
#endif

#include <aprinter/BeginNamespace.h>

template <typename T1, typename T2, typename Void = void>
struct IntDivide {
    static_assert(IntTypeInfo<T1>::is_signed == IntTypeInfo<T2>::is_signed,
                  "division of operands with different signedness not supported");
    
    static T1 call (T1 op1, T2 op2)
    {
        AMBRO_ASSERT(op2 != 0)
        
        return (op1 / op2);
    }
};

#ifdef AMBROLIB_AVR

template <>
struct IntDivide<uint32_t, uint32_t> {
    static uint32_t call (uint32_t n, uint32_t d)
    {
        return div_32_32_large(n, d);
    }
};

template <>
struct IntDivide<uint32_t, uint16_t> {
    static uint32_t call (uint32_t n, uint16_t d)
    {
        return div_32_16_large(n, d);
    }
};

#endif

#include <aprinter/EndNamespace.h>

#endif
