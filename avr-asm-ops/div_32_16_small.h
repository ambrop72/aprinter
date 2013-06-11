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

#ifndef AMBRO_AVR_ASM_DIV_32_16_SMALL
#define AMBRO_AVR_ASM_DIV_32_16_SMALL

#include <stdint.h>

/**
 * Division uint32_t/uint16_t.
 * 
 * Cycles in worst case: 470
 * = 2 + (8 * 11 - 1) + (8 * 13 - 1) + (8 * 16 - 1) + (8 * 19 - 1)
 */
static inline uint32_t div_32_16_small (uint32_t n, uint16_t d)
{
    uint32_t q;
    uint8_t ctr;
    
    asm(
        "    clr __tmp_reg__\n"
        "    clr %[ctr]\n"
        
        "loop0_%=:\n"
        "    lsl %D[n]\n"
        "    rol __tmp_reg__\n"
        "    lsl %D[q]\n"
        "    cp __tmp_reg__,%A[d]\n"
        "    cpc __zero_reg__,%B[d]\n"
        "    brcs zero_bit0_%=\n"
        "    sub __tmp_reg__,%A[d]\n"
        "    inc %D[q]\n"
        "zero_bit0_%=:\n"
        "    subi %[ctr],-32\n"
        "    brne loop0_%=\n"
        
        "loop1_%=:\n"
        "    lsl %C[n]\n"
        "    rol __tmp_reg__\n"
        "    rol %D[n]\n"
        "    lsl %C[q]\n"
        "    cp __tmp_reg__,%A[d]\n"
        "    cpc %D[n],%B[d]\n"
        "    brcs zero_bit1_%=\n"
        "    sub __tmp_reg__,%A[d]\n"
        "    sbc %D[n],%B[d]\n"
        "    inc %C[q]\n"
        "zero_bit1_%=:\n"
        "    subi %[ctr],-32\n"
        "    brne loop1_%=\n"
        
        "loop2_%=:\n"
        "    lsl %B[n]\n"
        "    rol __tmp_reg__\n"
        "    rol %D[n]\n"
        "    rol %C[n]\n"
        "    lsl %B[q]\n"
        "    cp __tmp_reg__,%A[d]\n"
        "    cpc %D[n],%B[d]\n"
        "    cpc %C[n],__zero_reg__\n"
        "    brcs zero_bit2_%=\n"
        "    sub __tmp_reg__,%A[d]\n"
        "    sbc %D[n],%B[d]\n"
        "    sbc %C[n],__zero_reg__\n"
        "    inc %B[q]\n"
        "zero_bit2_%=:\n"
        "    subi %[ctr],-32\n"
        "    brne loop2_%=\n"
        
        "loop3_%=:\n"
        "    lsl %A[n]\n"
        "    rol __tmp_reg__\n"
        "    rol %D[n]\n"
        "    rol %C[n]\n"
        "    rol %B[n]\n"
        "    lsl %A[q]\n"
        "    cp __tmp_reg__,%A[d]\n"
        "    cpc %D[n],%B[d]\n"
        "    cpc %C[n],__zero_reg__\n"
        "    cpc %B[n],__zero_reg__\n"
        "    brcs zero_bit3_%=\n"
        "    sub __tmp_reg__,%A[d]\n"
        "    sbc %D[n],%B[d]\n"
        "    sbc %C[n],__zero_reg__\n"
        "    sbc %B[n],__zero_reg__\n"
        "    inc %A[q]\n"
        "zero_bit3_%=:\n"
        "    subi %[ctr],-32\n"
        "    brne loop3_%=\n"
        
        : [q] "=&r" (q),
          [n] "=&r" (n),
          [ctr] "=&a" (ctr)
        : "[n]" (n),
          [d] "r" (d)
    );
    
    return q;
}

#endif
