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

#ifndef AMBRO_AVR_ASM_SQRT_25_LARGE_H
#define AMBRO_AVR_ASM_SQRT_25_LARGE_H

#include <stdint.h>

#include <aprinter/meta/Options.h>

#include <aprinter/BeginNamespace.h>

#define SQRT_25_ITER_3_4(i) \
"    cp %D[x],%B[goo]\n" \
"    brcs zero_bit_" #i "_%=\n" \
"    sub %D[x],%B[goo]\n" \
"    ori %B[goo],1<<(6-" #i ")\n" \
"zero_bit_" #i "_%=:\n" \
"    subi %B[goo],1<<(4-" #i ")\n" \
"    lsl %B[x]\n" \
"    rol %C[x]\n" \
"    rol %D[x]\n"

#define SQRT_25_ITER_5_5(i) \
"    cp %D[x],%B[goo]\n" \
"    brcs zero_bit_" #i "_%=\n" \
"    sub %D[x],%B[goo]\n" \
"    ori %B[goo],1<<(6-" #i ")\n" \
"zero_bit_" #i "_%=:\n" \
"    dec %B[goo]\n" \
"    lsl %B[x]\n" \
"    rol %C[x]\n" \
"    rol %D[x]\n"

#define SQRT_25_ITER_6_6(i) \
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

#define SQRT_25_ITER_7_8(i) \
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

#define SQRT_25_ITER_9_12(i) \
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

#define SQRT_25_ITER_13_13(i) \
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

#define SQRT_25_ITER_14_14(i) \
"    cp %C[x],%A[goo]\n" \
"    cpc %D[x],%B[goo]\n" \
"    brcs zero_bit_" #i "_%=\n" \
"    sub %C[x],%A[goo]\n" \
"    sbc %D[x],%B[goo]\n" \
"    ori %A[goo],1<<(15-" #i ")\n" \
"zero_bit_" #i "_%=:\n" \
"    dec %A[goo]\n" \
"    lsl %A[x]\n" \
"    rol %C[x]\n" \
"    rol %D[x]\n"

/*
 * Square root 25-bit.
 * 
 * Cycles in worst case: 131
 * = 8 + 2 * 8 + 8 + 10 + 2 * 10 + 4 * 10 + 15 + 10 + 4
 */
__attribute__((always_inline)) inline static uint16_t sqrt_25_large (uint32_t x, OptionForceInline opt)
{
    uint16_t goo;
    
    asm(
        "    ldi %A[goo],0x80\n"
        "    ldi %B[goo],0x04\n"
        "    lsl %B[x]\n"
        "    rol %C[x]\n"
        "    rol %D[x]\n"
        "    lsl %B[x]\n" \
        "    rol %C[x]\n" \
        "    rol %D[x]\n"
        SQRT_25_ITER_3_4(3)
        SQRT_25_ITER_3_4(4)
        SQRT_25_ITER_5_5(5)
        SQRT_25_ITER_6_6(6)
        SQRT_25_ITER_7_8(7)
        SQRT_25_ITER_7_8(8)
        SQRT_25_ITER_9_12(9)
        SQRT_25_ITER_9_12(10)
        SQRT_25_ITER_9_12(11)
        SQRT_25_ITER_9_12(12)
        SQRT_25_ITER_13_13(13)
        SQRT_25_ITER_14_14(14)
        "    lsl %A[x]\n"
        "    cpc %A[goo],%C[x]\n"
        "    cpc %B[goo],%D[x]\n"
        "    adc %A[goo],__zero_reg__\n"
        
        : [goo] "=&d" (goo),
          [x] "=&r" (x)
        : "[x]" (x)
    );
    
    return goo;
}

/*
 * Square root 25-bit with rounding.
 * 
 * Cycles in worst case: 142
 * = 8 + 2 * 8 + 8 + 10 + 2 * 10 + 4 * 10 + 15 + 10 + 15
 */
__attribute__((always_inline)) inline static uint16_t sqrt_25_large_round (uint32_t x, OptionForceInline opt)
{
    uint16_t goo;
    
    asm(
        "    ldi %A[goo],0x80\n"
        "    ldi %B[goo],0x04\n"
        "    lsl %B[x]\n"
        "    rol %C[x]\n"
        "    rol %D[x]\n"
        "    lsl %B[x]\n" \
        "    rol %C[x]\n" \
        "    rol %D[x]\n"
        SQRT_25_ITER_3_4(3)
        SQRT_25_ITER_3_4(4)
        SQRT_25_ITER_5_5(5)
        SQRT_25_ITER_6_6(6)
        SQRT_25_ITER_7_8(7)
        SQRT_25_ITER_7_8(8)
        SQRT_25_ITER_9_12(9)
        SQRT_25_ITER_9_12(10)
        SQRT_25_ITER_9_12(11)
        SQRT_25_ITER_9_12(12)
        SQRT_25_ITER_13_13(13)
        SQRT_25_ITER_14_14(14)
        "    cpi %A[x],0x80\n"
        "    cpc %C[x],%A[goo]\n"
        "    cpc %D[x],%B[goo]\n"
        "    brcs zero_bit_end_%=\n"
        "    subi %A[x],0x80\n"
        "    sbc %C[x],%A[goo]\n"
        "    sbc %D[x],%B[goo]\n"
        "    inc %A[goo]\n"
        "zero_bit_end_%=:"
        "    lsl %A[x]\n"
        "    rol %C[x]\n"
        "    rol %D[x]\n"
        "    cp %A[goo],%C[x]\n"
        "    cpc %B[goo],%D[x]\n"
        "    adc %A[goo],__zero_reg__\n"
        "    adc %B[goo],__zero_reg__\n"
        
        : [goo] "=&d" (goo),
          [x] "=&d" (x)
        : "[x]" (x)
    );
    
    return goo;
}

template <typename Option = int>
static uint16_t sqrt_25_large (uint32_t x, Option opt = 0)
{
    return sqrt_25_large(x, OptionForceInline());
}

template <typename Option = int>
static uint16_t sqrt_25_large_round (uint32_t x, Option opt = 0)
{
    return sqrt_25_large_round(x, OptionForceInline());
}


#include <aprinter/EndNamespace.h>

#endif
