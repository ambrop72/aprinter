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

#ifndef AMBRO_AVR_ASM_DIV_13_16_L16_S15_H
#define AMBRO_AVR_ASM_DIV_13_16_L16_S15_H

#include <stdint.h>

#define DIVIDE_13_16_L16_S15_ITER_17_23(i) \
"    lsl %A[n]\n" \
"    rol %B[n]\n" \
"    rol __tmp_reg__\n" \
"    cp %A[n],%A[d]\n" \
"    cpc %B[n],%B[d]\n" \
"    cpc __tmp_reg__,__zero_reg__\n" \
"    brcs zero_bit_" #i "__%=\n" \
"    sub %A[n],%A[d]\n" \
"    sbc %B[n],%B[d]\n" \
"    sbc __tmp_reg__,__zero_reg__\n" \
"    ori %B[q],1<<(23-" #i ")\n" \
"zero_bit_" #i "__%=:\n"

#define DIVIDE_13_16_L16_S15_ITER_24_30(i) \
"    lsl %A[n]\n" \
"    rol %B[n]\n" \
"    rol __tmp_reg__\n" \
"    rol %[t]\n" \
"    cp %A[n],%A[d]\n" \
"    cpc %B[n],%B[d]\n" \
"    cpc __tmp_reg__,__zero_reg__\n" \
"    cpc %[t],__zero_reg__\n" \
"    brcs zero_bit_" #i "__%=\n" \
"    sub %A[n],%A[d]\n" \
"    sbc %B[n],%B[d]\n" \
"    sbc __tmp_reg__,__zero_reg__\n" \
"    sbc %[t],__zero_reg__\n" \
"    ori %A[q],1<<(31-" #i ")\n" \
"zero_bit_" #i "__%=:\n"

/**
 * Division 2^16*(13bit/16bit), saturated to 15 bits.
 * 
 * WRONG
 * Cycles in worst case: 308
 * = 4 + 9 + (4 * 7) + (8 * 9) + (8 * 11) + (7 * 14) + 9
 */
static inline uint16_t div_13_16_l16_s15 (uint16_t n, uint16_t d)
{
    uint16_t q;
    uint8_t t;
    
    asm(
        "    clr __tmp_reg__\n"
        "    movw %A[q],__tmp_reg__\n"
        "    clr %[t]\n"
        "    lsl %A[n]\n"
        "    rol %B[n]\n"
        DIVIDE_13_16_L16_S15_ITER_17_23(17)
        DIVIDE_13_16_L16_S15_ITER_17_23(18)
        DIVIDE_13_16_L16_S15_ITER_17_23(19)
        DIVIDE_13_16_L16_S15_ITER_17_23(20)
        DIVIDE_13_16_L16_S15_ITER_17_23(21)
        DIVIDE_13_16_L16_S15_ITER_17_23(22)
        DIVIDE_13_16_L16_S15_ITER_17_23(23)
        DIVIDE_13_16_L16_S15_ITER_24_30(24)
        DIVIDE_13_16_L16_S15_ITER_24_30(25)
        DIVIDE_13_16_L16_S15_ITER_24_30(26)
        DIVIDE_13_16_L16_S15_ITER_24_30(27)
        DIVIDE_13_16_L16_S15_ITER_24_30(28)
        DIVIDE_13_16_L16_S15_ITER_24_30(29)
        DIVIDE_13_16_L16_S15_ITER_24_30(30)
        "    lsl %A[n]\n"
        "    rol %B[n]\n"
        "    rol __tmp_reg__\n"
        "    rol %[t]\n"
        "    cp %A[n],%A[d]\n"
        "    cpc %B[n],%B[d]\n"
        "    cpc __tmp_reg__,__zero_reg__\n"
        "    cpc %[t],__zero_reg__\n"
        "    sbci %A[q],-1\n"
        
        : [q] "=&a" (q),
          [n] "=&r" (n),
          [t] "=&r" (t)
        : "[n]" (n),
          [d] "r" (d)
    );
    
    return q;
}

#endif
