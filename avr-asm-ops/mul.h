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

#ifndef AMBRO_AVR_ASM_MUL_H
#define AMBRO_AVR_ASM_MUL_H

#include <stdint.h>

static inline int32_t mul_s24_16_trunc32 (__int24 op1, uint16_t op2)
{
    int32_t res;
    uint8_t zero;
    
    asm(
        "clr %[zero]\n"
        
        "mul %A[op1],%A[op2]\n"
        "movw %A[res],r0\n"
        
        "mulsu %C[op1],%B[op2]\n"
        "mov %D[res],r0\n"
        
        "clr %C[res]\n"
        
        "mul %A[op1],%B[op2]\n"
        "add %B[res],r0\n"
        "adc %C[res],r1\n"
        "adc %D[res],%[zero]\n"
        
        "mul %B[op1],%A[op2]\n"
        "add %B[res],r0\n"
        "adc %C[res],r1\n"
        "adc %D[res],%[zero]\n"
        
        "mulsu %C[op1],%A[op2]\n"
        "add %C[res],r0\n"
        "adc %D[res],r1\n"
        
        "mul %B[op1],%B[op2]\n"
        "add %C[res],r0\n"
        "adc %D[res],r1\n"
        
        "clr __zero_reg__\n"
        
        : [res] "=&r" (res),
          [zero] "=&r" (zero)
        : [op1] "a" (op1),
          [op2] "a" (op2)
    );
    
    return res;
}

static inline int32_t mul_s24_16_r8 (__int24 op1, uint16_t op2)
{
    uint8_t low;
    int32_t res;
    uint8_t zero;
    
    asm(
        "clr %[zero]\n"
        
        "mul %A[op1],%A[op2]\n"
        "mov %A[low],r0\n"
        "mov %A[res],r1\n"
        
        "mulsu %C[op1],%B[op2]\n"
        "movw %C[res],r0\n"
        
        "clr %B[res]\n"
        
        "mul %A[op1],%B[op2]\n"
        "add %A[res],r0\n"
        "adc %B[res],r1\n"
        "adc %C[res],%[zero]\n"
        "adc %D[res],%[zero]\n"
        
        "mul %B[op1],%A[op2]\n"
        "add %A[res],r0\n"
        "adc %B[res],r1\n"
        "adc %C[res],%[zero]\n"
        "adc %D[res],%[zero]\n"
        
        "mulsu %C[op1],%A[op2]\n"
        "sbc %D[res],%[zero]\n"
        "add %B[res],r0\n"
        "adc %C[res],r1\n"
        "adc %D[res],%[zero]\n"
        
        "mul %B[op1],%B[op2]\n"
        "add %B[res],r0\n"
        "adc %C[res],r1\n"
        "adc %D[res],%[zero]\n"

        "clr __zero_reg__\n"
        
        "tst %D[res]\n"
        "brpl not_negative_%=\n"
        "clr __tmp_reg__\n"
        "dec __tmp_reg__\n"
        "subi %A[low],1\n"
        "sbc %A[res],__tmp_reg__\n"
        "sbc %B[res],__tmp_reg__\n"
        "sbc %C[res],__tmp_reg__\n"
        "sbc %D[res],__tmp_reg__\n"
        "not_negative_%=:\n"
        
        : [res] "=&r" (res),
          [zero] "=&r" (zero),
          [low] "=&d" (low)
        : [op1] "a" (op1),
          [op2] "a" (op2)
    );
    
    return res;
}

static inline __int24 mul_s24_16_r16 (__int24 op1, uint16_t op2)
{
    uint16_t low;
    __int24 res;
    uint8_t zero;
    
    asm(
        "clr %[zero]\n"
        
        "mul %A[op1],%A[op2]\n"
        "movw %A[low],r0\n"
        
        "mulsu %C[op1],%B[op2]\n"
        "mov %B[res],r0\n"
        "mov %C[res],r1\n"
        
        "clr %A[res]\n"
        
        "mul %A[op1],%B[op2]\n"
        "add %B[low],r0\n"
        "adc %A[res],r1\n"
        "adc %B[res],%[zero]\n"
        "adc %C[res],%[zero]\n"
        
        "mul %B[op1],%A[op2]\n"
        "add %B[low],r0\n"
        "adc %A[res],r1\n"
        "adc %B[res],%[zero]\n"
        "adc %C[res],%[zero]\n"
        
        "mulsu %C[op1],%A[op2]\n"
        "sbc %C[res],%[zero]\n"
        "add %A[res],r0\n"
        "adc %B[res],r1\n"
        "adc %C[res],%[zero]\n"
        
        "mul %B[op1],%B[op2]\n"
        "add %A[res],r0\n"
        "adc %B[res],r1\n"
        "adc %C[res],%[zero]\n"

        "clr __zero_reg__\n"
        
        "tst %C[res]\n"
        "brpl not_negative_%=\n"
        "clr __tmp_reg__\n"
        "dec __tmp_reg__\n"
        "subi %A[low],1\n"
        "sbc %B[low],__zero_reg__\n"
        "sbc %A[res],__tmp_reg__\n"
        "sbc %B[res],__tmp_reg__\n"
        "sbc %C[res],__tmp_reg__\n"
        "not_negative_%=:\n"
        
        : [res] "=&r" (res),
          [zero] "=&r" (zero),
          [low] "=&d" (low)
        : [op1] "a" (op1),
          [op2] "a" (op2)
    );
    
    return res;
}

static inline int16_t mul_s16_16_r16 (int16_t op1, uint16_t op2)
{
    uint16_t low;
    int16_t res;
    uint8_t zero;
    
    asm(
        "clr %[zero]\n"
        
        "mul %A[op1],%A[op2]\n"
        "movw %A[low],r0\n"
        
        "mulsu %B[op1],%B[op2]\n"
        "movw %A[res],r0\n"
        
        "mul %A[op1],%B[op2]\n"
        "add %B[low],r0\n"
        "adc %A[res],r1\n"
        "adc %B[res],%[zero]\n"
        
        "mulsu %B[op1],%A[op2]\n"
        "sbc %B[res],%[zero]\n"
        "add %B[low],r0\n"
        "adc %A[res],r1\n"
        "adc %B[res],%[zero]\n"
        
        "clr __zero_reg__\n"
        
        "tst %B[res]\n"
        "brpl not_negative_%=\n"
        "subi %A[low],1\n"
        "sbc %B[low],__zero_reg__\n"
        "sbci %A[res],-1\n"
        "sbci %B[res],-1\n"
        "not_negative_%=:\n"
        
        : [res] "=&d" (res),
          [zero] "=&r" (zero),
          [low] "=&d" (low)
        : [op1] "a" (op1),
          [op2] "a" (op2)
    );
    
    return res;
}

static inline __uint24 mul_24_16_r16 (__uint24 op1, uint16_t op2)
{
    uint8_t low;
    __uint24 res;
    uint8_t zero;
    
    asm(
        "clr %[zero]\n"
        
        "mul %A[op1],%A[op2]\n"
        "mov %A[low],r1\n"
        
        "mul %B[op1],%B[op2]\n"
        "movw %A[res],r0\n"
        
        "clr %C[res]\n"
        
        "mul %A[op1],%B[op2]\n"
        "add %A[low],r0\n"
        "adc %A[res],r1\n"
        "adc %B[res],%[zero]\n"
        "adc %C[res],%[zero]\n"
        
        "mul %B[op1],%A[op2]\n"
        "add %A[low],r0\n"
        "adc %A[res],r1\n"
        "adc %B[res],%[zero]\n"
        "adc %C[res],%[zero]\n"
        
        "mul %C[op1],%A[op2]\n"
        "add %A[res],r0\n"
        "adc %B[res],r1\n"
        "adc %C[res],%[zero]\n"
        
        "mul %C[op1],%B[op2]\n"
        "add %B[res],r0\n"
        "adc %C[res],r1\n"

        "clr __zero_reg__\n"
        
        : [res] "=&d" (res),
          [zero] "=&r" (zero),
          [low] "=&r" (low)
        : [op1] "r" (op1),
          [op2] "r" (op2)
    );
    
    return res;
}

static inline uint32_t mul_32_16_r16 (uint32_t op1, uint16_t op2)
{
    uint8_t low;
    uint32_t res;
    uint8_t zero;
    
    asm(
        "clr %[zero]\n"
        
        "mul %A[op1],%A[op2]\n"
        "mov %A[low],r1\n"
        
        "mul %B[op1],%B[op2]\n"
        "movw %A[res],r0\n"
        
        "mul %D[op1],%B[op2]\n"
        "movw %C[res],r0\n"
        
        "mul %A[op1],%B[op2]\n"
        "add %A[low],r0\n"
        "adc %A[res],r1\n"
        "adc %B[res],%[zero]\n"
        "adc %C[res],%[zero]\n"
        "adc %D[res],%[zero]\n"
        
        "mul %B[op1],%A[op2]\n"
        "add %A[low],r0\n"
        "adc %A[res],r1\n"
        "adc %B[res],%[zero]\n"
        "adc %C[res],%[zero]\n"
        "adc %D[res],%[zero]\n"
        
        "mul %C[op1],%A[op2]\n"
        "add %A[res],r0\n"
        "adc %B[res],r1\n"
        "adc %C[res],%[zero]\n"
        "adc %D[res],%[zero]\n"
        
        "mul %C[op1],%B[op2]\n"
        "add %B[res],r0\n"
        "adc %C[res],r1\n"
        "adc %D[res],%[zero]\n"

        "mul %D[op1],%A[op2]\n"
        "add %B[res],r0\n"
        "adc %C[res],r1\n"
        "adc %D[res],%[zero]\n"
        
        "clr __zero_reg__\n"
        
        : [res] "=&d" (res),
          [zero] "=&r" (zero),
          [low] "=&r" (low)
        : [op1] "r" (op1),
          [op2] "r" (op2)
    );
    
    return res;
}

static inline uint32_t mul_32_8_r8 (uint32_t op1, uint8_t op2)
{
    uint32_t res;
    uint8_t zero;
    
    asm(
        "clr %[zero]\n"
        
        "mul %B[op1],%A[op2]\n"
        "movw %A[res],r0\n"
        
        "mul %D[op1],%A[op2]\n"
        "movw %C[res],r0\n"
        
        "mul %A[op1],%A[op2]\n"
        "add %A[res],r1\n"
        "adc %B[res],%[zero]\n"
        "adc %C[res],%[zero]\n"
        "adc %D[res],%[zero]\n"
        
        "mul %C[op1],%A[op2]\n"
        "add %B[res],r0\n"
        "adc %C[res],r1\n"
        "adc %D[res],%[zero]\n"
        
        "clr __zero_reg__\n"
        
        : [res] "=&r" (res),
          [zero] "=&r" (zero)
        : [op1] "r" (op1),
          [op2] "r" (op2)
    );
    
    return res;
}

static inline int32_t mul_s16_16 (int16_t op1, uint16_t op2)
{
    int32_t res;
    uint8_t zero;
    
    asm(
        "clr %[zero]\n"
        "mul %A[op1], %A[op2]\n"
        "movw %A[res], r0\n"
        "mulsu %B[op1], %B[op2]\n"
        "movw %C[res], r0\n"
        "mul %B[op2], %A[op1]\n"
        "add %B[res], r0\n"
        "adc %C[res], r1\n"
        "adc %D[res], %[zero]\n"
        "mulsu %B[op1], %A[op2]\n"
        "sbc %D[res], %[zero]\n"
        "add %B[res], r0\n"
        "adc %C[res], r1\n"
        "adc %D[res], %[zero]\n"
        "clr __zero_reg__\n"
        
        : [res] "=&r" (res),
          [zero] "=&r" (zero)
        : [op1] "a" (op1),
          [op2] "a" (op2)
    );
    
    return res;
}

static inline __int24 mul_s16_16_r8 (int16_t op1, uint16_t op2)
{
    uint8_t low;
    __int24 res;
    uint8_t zero;
    
    asm(
        "clr %[zero]\n"
        
        "mul %A[op1],%A[op2]\n"
        "mov %A[low],r0\n"
        "mov %A[res],r1\n"
        
        "mulsu %B[op1],%B[op2]\n"
        "mov %B[res],r0\n"
        "mov %C[res],r1\n"
        
        "mul %A[op1],%B[op2]\n"
        "add %A[res],r0\n"
        "adc %B[res],r1\n"
        "adc %C[res],%[zero]\n"
        
        "mulsu %B[op1],%A[op2]\n"
        "sbc %C[res],%[zero]\n"
        "add %A[res],r0\n"
        "adc %B[res],r1\n"
        "adc %C[res],%[zero]\n"
        
        "clr __zero_reg__\n"
        
        "tst %C[res]\n"
        "brpl not_negative_%=\n"
        "subi %A[low],1\n"
        "sbci %A[res],-1\n"
        "sbci %B[res],-1\n"
        "sbci %C[res],-1\n"
        "not_negative_%=:\n"
        
        : [res] "=&d" (res),
          [zero] "=&r" (zero),
          [low] "=&d" (low)
        : [op1] "a" (op1),
          [op2] "a" (op2)
    );
    
    return res;
}

#endif
