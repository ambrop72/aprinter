/*
 * Copyright (c) 2014 Ambroz Bizjak
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

#ifndef AMBRO_AVR_ASM_FPFROMINT_H
#define AMBRO_AVR_ASM_FPFROMINT_H

#include <stdint.h>

#include <aprinter/BeginNamespace.h>

static float fpfromint_u32 (uint32_t op)
{
    float res;
    uint8_t exp;
    asm(
        "tst %D[op]\n"
        "brne right_shift_%=\n"
        "tst %C[op]\n"
        "brne left_shift0_%=\n"        
        "tst %B[op]\n"
        "brne left_shift1_%=\n"
        "tst %A[op]\n"
        "brne left_shift2_%=\n"
        "clr %[exp]\n"
        "rjmp done_%=\n"
        
        "left_shift0_%=:\n"
        "ldi %[exp],150\n"
        "tst %C[op]\n"
        "brmi left_shift0_done_%=\n"
        "left_shift0_again_%=:\n"
        "dec %[exp]\n"
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "brpl left_shift0_again_%=\n"
        "left_shift0_done_%=:\n"
        "rjmp done_%=\n"
        
        "left_shift1_%=:\n"
        "ldi %[exp],150-8\n"
        "tst %B[op]\n"
        "brmi left_shift1_done_%=\n"
        "left_shift1_again_%=:\n"
        "dec %[exp]\n"
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "brpl left_shift1_again_%=\n"
        "left_shift1_done_%=:\n"
        "mov %C[op],%B[op]\n"
        "mov %B[op],%A[op]\n"
        "clr %A[op]\n"
        "rjmp done_%=\n"
        
        "left_shift2_%=:\n"
        "ldi %[exp],150-16\n"
        "tst %A[op]\n"
        "brmi left_shift2_done_%=\n"
        "left_shift2_again_%=:\n"
        "dec %[exp]\n"
        "lsl %A[op]\n"
        "brpl left_shift2_again_%=\n"
        "left_shift2_done_%=:\n"
        "mov %C[op],%A[op]\n"
        "clr %B[op]\n"
        "clr %A[op]\n"
        "rjmp done_%=\n"
        
        "right_shift_%=:\n"
        "ldi %[exp],150\n"
        "clr __tmp_reg__\n"
        "rjmp right_shift_entry_%=\n"
        "right_shift_again_%=:\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "inc %[exp]\n"
        "ror __tmp_reg__\n"
        "right_shift_entry_%=:\n"
        "lsr %D[op]\n"
        "brne right_shift_again_%=\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "inc %[exp]\n"
        "tst __tmp_reg__\n"
        "brne right_shift_inc_%=\n"
        "sbrs %A[op],0\n"
        "clc\n"
        "right_shift_inc_%=:\n"
        "adc %A[op],__zero_reg__\n"
        "adc %B[op],__zero_reg__\n"
        "adc %C[op],__zero_reg__\n"
        "adc %[exp],__zero_reg__\n"
        
        "done_%=:\n"
        "lsl %C[op]\n"
        "mov %D[op],%[exp]\n"
        "lsr %D[op]\n"
        "ror %C[op]\n"
        
        : [op] "=&r" (res),
          [exp] "=&d" (exp)
        : "[op]" (op)
    );
    return res;
}

#include <aprinter/EndNamespace.h>

#endif
