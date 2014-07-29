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

#ifndef AMBRO_AVR_ASM_FPROUND_H
#define AMBRO_AVR_ASM_FPROUND_H

#include <stdint.h>

static uint32_t fpround_u32 (float op)
{
    uint32_t res;
    uint8_t exp;
    asm(
        "sbrc %D[op],7\n"
        "rjmp underflow_%=\n"
        
        "lsl %C[op]\n"
        "mov %[exp],%D[op]\n"
        "rol %[exp]\n"
        "clr %D[op]\n"
        
        "cpi %[exp],126\n"
        "brcs underflow_%=\n"
        
        "cpi %[exp],134\n"
        "brcc right_shift_zero_or_one_byte_or_left_shift_%=\n"
        "ror %C[op]\n"
        
        "mov %A[op],%C[op]\n"
        "clr %B[op]\n"
        "clr %C[op]\n"
        "subi %[exp],133\n"
        "breq right_shift_two_bytes_round_%=\n"
        "lsr %A[op]\n"
        "subi %[exp],-1\n"
        "breq right_shift_two_bytes_round_%=\n"
        "lsr %A[op]\n"
        "subi %[exp],-1\n"
        "breq right_shift_two_bytes_round_%=\n"
        "lsr %A[op]\n"
        "subi %[exp],-1\n"
        "breq right_shift_two_bytes_round_%=\n"
        "lsr %A[op]\n"
        "subi %[exp],-1\n"
        "breq right_shift_two_bytes_round_%=\n"
        "lsr %A[op]\n"
        "subi %[exp],-1\n"
        "breq right_shift_two_bytes_round_%=\n"
        "lsr %A[op]\n"
        "subi %[exp],-1\n"
        "breq right_shift_two_bytes_round_%=\n"
        "lsr %A[op]\n"
        "right_shift_two_bytes_round_%=:\n"
        "lsr %A[op]\n"
        "adc %A[op],__zero_reg__\n"
        "jmp end_%=\n"
        
        "underflow_%=:\n"
        "clr %A[op]\n"
        "clr %B[op]\n"
        "movw %C[op],%A[op]\n"
        "jmp end_%=\n"
        
        "right_shift_zero_or_one_byte_or_left_shift_%=:\n"
        "cpi %[exp],150\n"
        "brcc left_shift_%=\n"
        "ror %C[op]\n"
        "cpi %[exp],142\n"
        "brcs right_shift_one_byte_%=\n"
        
        "subi %[exp],149\n"
        "breq right_shift_zero_bytes_round_%=\n"
        "lsr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "subi %[exp],-1\n"
        "breq right_shift_zero_bytes_round_%=\n"
        "lsr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "subi %[exp],-1\n"
        "breq right_shift_zero_bytes_round_%=\n"
        "lsr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "subi %[exp],-1\n"
        "breq right_shift_zero_bytes_round_%=\n"
        "lsr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "subi %[exp],-1\n"
        "breq right_shift_zero_bytes_round_%=\n"
        "lsr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "subi %[exp],-1\n"
        "breq right_shift_zero_bytes_round_%=\n"
        "lsr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "subi %[exp],-1\n"
        "breq right_shift_zero_bytes_round_%=\n"
        "lsr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "right_shift_zero_bytes_round_%=:\n"
        "lsr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "adc %A[op],__zero_reg__\n"
        "adc %B[op],__zero_reg__\n"
        "adc %C[op],__zero_reg__\n"
        "jmp end_%=\n"
        
        "overflow_%=:\n"
        "ldi %A[op],0xFF\n"
        "ldi %B[op],0xFF\n"
        "movw %C[op],%A[op]\n"
        "jmp end_%=\n"
        
        "left_shift_%=:\n"
        "cpi %[exp],32+127\n"
        "brcc overflow_%=\n"
        "ror %C[op]\n"
        "subi %[exp],150\n"
        "breq end_%=\n"
        "left_shift_again_%=:\n"
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "rol %D[op]\n"
        "subi %[exp],1\n"
        "brne left_shift_again_%=\n"
        "jmp end_%=\n"
        
        "right_shift_one_byte_%=:\n"
        "mov %A[op],%B[op]\n"
        "mov %B[op],%C[op]\n"
        "clr %C[op]\n"
        "subi %[exp],141\n"
        "breq right_shift_one_byte_round_%=\n"
        "lsr %B[op]\n"
        "ror %A[op]\n"
        "subi %[exp],-1\n"
        "breq right_shift_one_byte_round_%=\n"
        "lsr %B[op]\n"
        "ror %A[op]\n"
        "subi %[exp],-1\n"
        "breq right_shift_one_byte_round_%=\n"
        "lsr %B[op]\n"
        "ror %A[op]\n"
        "subi %[exp],-1\n"
        "breq right_shift_one_byte_round_%=\n"
        "lsr %B[op]\n"
        "ror %A[op]\n"
        "subi %[exp],-1\n"
        "breq right_shift_one_byte_round_%=\n"
        "lsr %B[op]\n"
        "ror %A[op]\n"
        "subi %[exp],-1\n"
        "breq right_shift_one_byte_round_%=\n"
        "lsr %B[op]\n"
        "ror %A[op]\n"
        "subi %[exp],-1\n"
        "breq right_shift_one_byte_round_%=\n"
        "lsr %B[op]\n"
        "ror %A[op]\n"
        "right_shift_one_byte_round_%=:\n"
        "lsr %B[op]\n"
        "ror %A[op]\n"
        "adc %A[op],__zero_reg__\n"
        "adc %B[op],__zero_reg__\n"
        
        "end_%=:\n"
        
        : [op] "=&d" (res),
          [exp] "=&d" (exp)
        : "[op]" (op)
    );
    return res;
}

#endif
