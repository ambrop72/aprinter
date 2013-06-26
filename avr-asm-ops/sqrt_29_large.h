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

#ifndef AMBRO_AVR_ASM_SQRT_29_LARGE_H
#define AMBRO_AVR_ASM_SQRT_29_LARGE_H

#include <stdint.h>

#define SQRT_29_ITER_1_4(i) \
"    cp %D[x],%B[goo]\n" \
"    brcs zero_bit_" #i "_%=\n" \
"    sub %D[x],%B[goo]\n" \
"    ori %B[goo],1<<(6-" #i ")\n" \
"zero_bit_" #i "_%=:\n" \
"    subi %B[goo],1<<(4-" #i ")\n" \
"    lsl %B[x]\n" \
"    rol %C[x]\n" \
"    rol %D[x]\n"

#define SQRT_29_ITER_5_5(i) \
"    cp %D[x],%B[goo]\n" \
"    brcs zero_bit_" #i "_%=\n" \
"    sub %D[x],%B[goo]\n" \
"    ori %B[goo],1<<(6-" #i ")\n" \
"zero_bit_" #i "_%=:\n" \
"    ldi %A[goo],0x80\n" \
"    dec %B[goo]\n" \
"    lsl %B[x]\n" \
"    rol %C[x]\n" \
"    rol %D[x]\n"

#define SQRT_29_ITER_6_6(i) \
"    cp %C[x],%A[goo]\n" \
"    cpc %D[x],%B[goo]\n" \
"    brcs zero_bit_" #i "_%=\n" \
"    sub %C[x],%A[goo]\n" \
"    sbc %D[x],%B[goo]\n" \
"    ori %B[goo],1<<(6-" #i ")\n" \
"zero_bit_" #i "_%=:\n" \
"    subi %A[goo],1<<(12-" #i ")\n" \
"    lsl %B[x]\n" \
"    rol %C[x]\n" \
"    rol %D[x]\n"

#define SQRT_29_ITER_7_8(i) \
"    cp %C[x],%A[goo]\n" \
"    cpc %D[x],%B[goo]\n" \
"    brcs zero_bit_" #i "_%=\n" \
"    sub %C[x],%A[goo]\n" \
"    sbc %D[x],%B[goo]\n" \
"    ori %A[goo],1<<(14-" #i ")\n" \
"zero_bit_" #i "_%=:\n" \
"    subi %A[goo],1<<(12-" #i ")\n" \
"    lsl %B[x]\n" \
"    rol %C[x]\n" \
"    rol %D[x]\n"

#define SQRT_29_ITER_9_12(i) \
"    cp %C[x],%A[goo]\n" \
"    cpc %D[x],%B[goo]\n" \
"    brcs zero_bit_" #i "_%=\n" \
"    sub %C[x],%A[goo]\n" \
"    sbc %D[x],%B[goo]\n" \
"    ori %A[goo],1<<(14-" #i ")\n" \
"zero_bit_" #i "_%=:\n" \
"    subi %A[goo],1<<(12-" #i ")\n" \
"    lsl %A[x]\n" \
"    rol %C[x]\n" \
"    rol %D[x]\n"

#define SQRT_29_ITER_13_13(i) \
"    cp %C[x],%A[goo]\n" \
"    cpc %D[x],%B[goo]\n" \
"    brcs zero_bit_" #i "_%=\n" \
"    sub %C[x],%A[goo]\n" \
"    sbc %D[x],%B[goo]\n" \
"    ori %A[goo],1<<(14-" #i ")\n" \
"zero_bit_" #i "_%=:\n" \
"    lsl %A[goo]\n" \
"    rol %B[goo]\n" \
"    subi %A[goo],1<<(13-" #i ")\n" \
"    lsl %A[x]\n" \
"    rol %C[x]\n" \
"    rol %D[x]\n" \
"    lsl %A[x]\n" \
"    rol %C[x]\n" \
"    rol %D[x]\n"

#define SQRT_29_ITER_14_14(i) \
"    brcs one_bit_" #i "_%=\n" \
"    cp %C[x],%A[goo]\n" \
"    cpc %D[x],%B[goo]\n" \
"    brcs zero_bit_" #i "_%=\n" \
"one_bit_" #i "_%=:\n" \
"    sub %C[x],%A[goo]\n" \
"    sbc %D[x],%B[goo]\n" \
"    ori %A[goo],1<<(15-" #i ")\n" \
"zero_bit_" #i "_%=:\n" \
"    dec %A[goo]\n" \
"    lsl %A[x]\n" \
"    rol %C[x]\n" \
"    rol %D[x]\n"

/*
 * Square root 29-bit.
 * 
 * Cycles in worst case: 142
 * = 4 * 8 + 9 + 10 + 2 * 10 + 4 * 10 + 15 + 11 + 5
 */
static inline uint16_t sqrt_29_large (uint32_t x)
{
    uint16_t goo = UINT16_C(0x1030);
    
    asm(
        SQRT_29_ITER_1_4(1)
        SQRT_29_ITER_1_4(2)
        SQRT_29_ITER_1_4(3)
        SQRT_29_ITER_1_4(4)
        SQRT_29_ITER_5_5(5)
        SQRT_29_ITER_6_6(6)
        SQRT_29_ITER_7_8(7)
        SQRT_29_ITER_7_8(8)
        SQRT_29_ITER_9_12(9)
        SQRT_29_ITER_9_12(10)
        SQRT_29_ITER_9_12(11)
        SQRT_29_ITER_9_12(12)
        SQRT_29_ITER_13_13(13)
        SQRT_29_ITER_14_14(14)
        "    brcs end_inc%=\n"
        "    lsl %A[x]\n"
        "    cpc %A[goo],%C[x]\n"
        "    cpc %B[goo],%D[x]\n"
        "end_inc%=:\n"
        "    adc %A[goo],__zero_reg__\n"
        
        : [goo] "=&d" (goo),
          [x] "=&r" (x)
        : "[x]" (x),
          "[goo]" (goo)
    );
    
    return goo;
}

#endif
