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

#ifndef AMBRO_AVR_ASM_MUL_32_16
#define AMBRO_AVR_ASM_MUL_32_16

#include <stdint.h>

static inline void mul_32_16 (uint32_t op1, uint16_t op2, uint32_t *low, uint16_t *high)
{
    uint8_t zero;
    
    asm(
        "clr %[zero]\n"
        
        "mul %A[op1],%A[op2]\n"
        "movw %A[resL],r0\n"
        
        "mul %B[op1],%B[op2]\n"
        "movw %C[resL],r0\n"
        
        "mul %D[op1],%B[op2]\n"
        "movw %A[resH],r0\n"
        
        "mul %A[op1],%B[op2]\n"
        "add %B[resL],r0\n"
        "adc %C[resL],r1\n"
        "adc %D[resL],%[zero]\n"
        "adc %A[resH],%[zero]\n"
        "adc %B[resH],%[zero]\n"
        
        "mul %B[op1],%A[op2]\n"
        "add %B[resL],r0\n"
        "adc %C[resL],r1\n"
        "adc %D[resL],%[zero]\n"
        "adc %A[resH],%[zero]\n"
        "adc %B[resH],%[zero]\n"
        
        "mul %C[op1],%A[op2]\n"
        "add %C[resL],r0\n"
        "adc %D[resL],r1\n"
        "adc %A[resH],%[zero]\n"
        "adc %B[resH],%[zero]\n"
        
        "mul %C[op1],%B[op2]\n"
        "add %D[resL],r0\n"
        "adc %A[resH],r1\n"
        "adc %B[resH],%[zero]\n"

        "mul %D[op1],%A[op2]\n"
        "add %D[resL],r0\n"
        "adc %A[resH],r1\n"
        "adc %B[resH],%[zero]\n"
        
        "clr __zero_reg__\n"
        
        : [resL] "=&r" (*low),
          [resH] "=&r" (*high),
          [zero] "=&r" (zero)
        : [op1] "r" (op1),
          [op2] "r" (op2)
    );
}

#endif
