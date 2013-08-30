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

#ifndef AMBRO_AVR_ASM_MUL_24_16_R16_H
#define AMBRO_AVR_ASM_MUL_24_16_R16_H

#include <stdint.h>

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

#endif
