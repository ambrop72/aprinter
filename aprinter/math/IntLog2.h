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

#ifndef AMBROLIB_INT_LOG2_H
#define AMBROLIB_INT_LOG2_H

#include <stdint.h>

#include <aprinter/meta/IntTypeInfo.h>

#include <aprinter/BeginNamespace.h>

template <typename T, typename Void = void>
struct IntLog2 {
    static_assert(!IntTypeInfo<T>::is_signed, "only unsigned integers are allowed for IntLog2");
    
    static uint8_t call (T op)
    {
        uint8_t res = 0;
        while (op) {
            res++;
            op >>= 1;
        }
        return res;
    }
};

#if defined(AMBROLIB_AVR)

template <>
struct IntLog2<uint32_t> {
    static uint8_t call (uint32_t op)
    {
        uint8_t res;
        __asm__(
            "    cpse %D[op],__zero_reg__\n"
            "    rjmp foundD%=\n"
            "    cpse %C[op],__zero_reg__\n"
            "    rjmp foundC%=\n"
            "    cpse %B[op],__zero_reg__\n"
            "    rjmp foundB%=\n"
            "    cpse %A[op],__zero_reg__\n"
            "    rjmp foundA%=\n"
            "    clr %[res]\n"
            "    rjmp end%=\n"
            "foundD%=:\n"
            "    ldi %[res],0x18\n"
            "    mov r17,%D[op]\n"
            "    rjmp examine%=\n"
            "foundC%=:\n"
            "    ldi %[res],0x10\n"
            "    mov r17,%C[op]\n"
            "    rjmp examine%=\n"
            "foundB%=:\n"
            "    ldi %[res],0x08\n"
            "    mov r17,%B[op]\n"
            "    rjmp examine%=\n"
            "foundA%=:\n"
            "    ldi %[res],0x00\n"
            "    mov r17,%A[op]\n"
            "examine%=:\n"
            "    bst r17,7\n"
            "    brts bit7%=\n"
            "    bst r17,6\n"
            "    brts bit6%=\n"
            "    bst r17,5\n"
            "    brts bit5%=\n"
            "    bst r17,4\n"
            "    brts bit4%=\n"
            "    bst r17,3\n"
            "    brts bit3%=\n"
            "    bst r17,2\n"
            "    brts bit2%=\n"
            "    bst r17,1\n"
            "    brts bit1%=\n"
            "    subi %[res],0xFF\n"
            "    rjmp end%=\n"
            "bit7%=:\n"
            "    subi %[res],0xF8\n"
            "    rjmp end%=\n"
            "bit6%=:\n"
            "    subi %[res],0xF9\n"
            "    rjmp end%=\n"
            "bit5%=:\n"
            "    subi %[res],0xFA\n"
            "    rjmp end%=\n"
            "bit4%=:\n"
            "    subi %[res],0xFB\n"
            "    rjmp end%=\n"
            "bit3%=:\n"
            "    subi %[res],0xFC\n"
            "    rjmp end%=\n"
            "bit2%=:\n"
            "    subi %[res],0xFD\n"
            "    rjmp end%=\n"
            "bit1%=:\n"
            "    subi %[res],0xFE\n"
            "end%=:\n"
            : [res] "=&a" (res)
            : [op] "r" (op)
            : "r17"
        );
        return res;
    }
};

#endif

#include <aprinter/EndNamespace.h>

#endif
