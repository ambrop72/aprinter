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

#ifndef AMBRO_AVR_ASM_MUL_S16_16_H
#define AMBRO_AVR_ASM_MUL_S16_16_H

#include <stdint.h>

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

#endif
