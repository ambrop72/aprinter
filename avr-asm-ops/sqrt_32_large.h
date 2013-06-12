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

#ifndef AMBRO_AVR_ASM_SQRT_32_LARGE
#define AMBRO_AVR_ASM_SQRT_32_LARGE

#include <stdint.h>

#define SQRT_32_ITER_0_0(i) \
"    cp %D[x],%B[goo]\n" \
"    brcs zero_bit_" #i "_%=\n" \
"    sub %D[x],%B[goo]\n" \
"    or %B[goo],__tmp_reg__\n" \
"zero_bit_" #i "_%=:\n" \
"    lsr __tmp_reg__\n" \
"    eor %B[goo],__tmp_reg__\n" \
"    lsr __tmp_reg__\n" \
"    lsr %B[goo]\n"

#define SQRT_32_ITER_1_4(i) \
"    cp %D[x],%B[goo]\n" \
"    brcs zero_bit_" #i "_%=\n" \
"    sub %D[x],%B[goo]\n" \
"    or %B[goo],__tmp_reg__\n" \
"zero_bit_" #i "_%=:\n" \
"    lsr __tmp_reg__\n" \
"    eor %B[goo],__tmp_reg__\n" \
"    lsl %B[x]\n" \
"    rol %C[x]\n" \
"    rol %D[x]\n"

#define SQRT_32_ITER_5_5(i) \
"    cp %D[x],%B[goo]\n" \
"    brcs zero_bit_" #i "_%=\n" \
"    sub %D[x],%B[goo]\n" \
"    or %B[goo],__tmp_reg__\n" \
"zero_bit_" #i "_%=:\n" \
"    lsr __tmp_reg__\n" \
"    ror __tmp_reg__\n" \
"    mov %A[goo],__tmp_reg__\n" \
"    dec %B[goo]\n" \
"    lsl %B[x]\n" \
"    rol %C[x]\n" \
"    rol %D[x]\n"

#define SQRT_32_ITER_6_6(i) \
"    cp %C[x],%A[goo]\n" \
"    cpc %D[x],%B[goo]\n" \
"    brcs zero_bit_" #i "_%=\n" \
"    sub %C[x],%A[goo]\n" \
"    sbc %D[x],%B[goo]\n" \
"    inc %B[goo]\n" \
"zero_bit_" #i "_%=:\n" \
"    asr __tmp_reg__\n" \
"    eor %A[goo],__tmp_reg__\n" \
"    lsl %B[x]\n" \
"    rol %C[x]\n" \
"    rol %D[x]\n"

#define SQRT_32_ITER_7_8(i) \
"    cp %C[x],%A[goo]\n" \
"    cpc %D[x],%B[goo]\n" \
"    brcs zero_bit_" #i "_%=\n" \
"    sub %C[x],%A[goo]\n" \
"    sbc %D[x],%B[goo]\n" \
"    or %A[goo],__tmp_reg__\n" \
"zero_bit_" #i "_%=:\n" \
"    lsr __tmp_reg__\n" \
"    eor %A[goo],__tmp_reg__\n" \
"    lsl %B[x]\n" \
"    rol %C[x]\n" \
"    rol %D[x]\n"

#define SQRT_32_ITER_9_12(i) \
"    cp %C[x],%A[goo]\n" \
"    cpc %D[x],%B[goo]\n" \
"    brcs zero_bit_" #i "_%=\n" \
"    sub %C[x],%A[goo]\n" \
"    sbc %D[x],%B[goo]\n" \
"    or %A[goo],__tmp_reg__\n" \
"zero_bit_" #i "_%=:\n" \
"    lsr __tmp_reg__\n" \
"    eor %A[goo],__tmp_reg__\n" \
"    lsl %A[x]\n" \
"    rol %C[x]\n" \
"    rol %D[x]\n"

#define SQRT_32_ITER_13_13(i) \
"    cp %C[x],%A[goo]\n" \
"    cpc %D[x],%B[goo]\n" \
"    brcs zero_bit_" #i "_%=\n" \
"    sub %C[x],%A[goo]\n" \
"    sbc %D[x],%B[goo]\n" \
"    or %A[goo],__tmp_reg__\n" \
"zero_bit_" #i "_%=:\n" \
"    lsl %A[goo]\n" \
"    rol %B[goo]\n" \
"    eor %A[goo],__tmp_reg__\n" \
"    lsl %A[x]\n" \
"    rol %C[x]\n" \
"    rol %D[x]\n" \
"    lsl %A[x]\n" \
"    rol %C[x]\n" \
"    rol %D[x]\n"

#define SQRT_32_ITER_14_14(i) \
"    brcs one_bit_" #i "_%=\n" \
"    cp %C[x],%A[goo]\n" \
"    cpc %D[x],%B[goo]\n" \
"    brcs zero_bit_" #i "_%=\n" \
"one_bit_" #i "_%=:\n" \
"    sub %C[x],%A[goo]\n" \
"    sbc %D[x],%B[goo]\n" \
"    or %A[goo],__tmp_reg__\n" \
"zero_bit_" #i "_%=:\n" \
"    dec %A[goo]\n" \
"    lsl %A[x]\n" \
"    rol %C[x]\n" \
"    rol %D[x]\n"

static inline uint16_t sqrt_32_large (uint32_t x)
{
    uint16_t goo = UINT16_C(0x40C0);
    
    asm(
        "    mov __tmp_reg__,%A[goo]\n"
        SQRT_32_ITER_0_0(0)
        SQRT_32_ITER_1_4(1)
        SQRT_32_ITER_1_4(2)
        SQRT_32_ITER_1_4(3)
        SQRT_32_ITER_1_4(4)
        SQRT_32_ITER_5_5(5)
        SQRT_32_ITER_6_6(6)
        SQRT_32_ITER_7_8(7)
        SQRT_32_ITER_7_8(8)
        SQRT_32_ITER_9_12(9)
        SQRT_32_ITER_9_12(10)
        SQRT_32_ITER_9_12(11)
        SQRT_32_ITER_9_12(12)
        SQRT_32_ITER_13_13(13)
        SQRT_32_ITER_14_14(14)
        "    brcs end_inc%=\n"
        "    lsl %A[x]\n"
        "    cpc %A[goo],%C[x]\n"
        "    cpc %B[goo],%D[x]\n"
        "end_inc%=:\n"
        "    adc %A[goo],__zero_reg__\n"
        
        : [goo] "=&r" (goo),
          [x] "=&r" (x)
        : "[x]" (x),
          "[goo]" (goo)
    );
    
    return goo;
}

#endif
