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

#ifndef AMBRO_AVR_ASM_SHIFT_H
#define AMBRO_AVR_ASM_SHIFT_H

#include <stdint.h>

static inline __int24 shift_s24_r1 (__int24 op)
{
    asm(
        "tst %C[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],-1\n"
        "sbci %B[op],-1\n"
        "sbci %C[op],-1\n"
        "not_negative_%=:\n"
        "asr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline __int24 shift_s24_r2 (__int24 op)
{
    asm(
        "tst %C[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],-3\n"
        "sbci %B[op],-1\n"
        "sbci %C[op],-1\n"
        "not_negative_%=:\n"
        "asr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "asr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline __int24 shift_s24_r3 (__int24 op)
{
    asm(
        "tst %C[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],-7\n"
        "sbci %B[op],-1\n"
        "sbci %C[op],-1\n"
        "not_negative_%=:\n"
        "asr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "asr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "asr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline __int24 shift_s24_r4 (__int24 op)
{
    asm(
        "tst %C[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],-15\n"
        "sbci %B[op],-1\n"
        "sbci %C[op],-1\n"
        "not_negative_%=:\n"
        "asr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "asr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "asr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "asr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline __int24 shift_s24_r5 (__int24 op)
{
    asm(
        "tst %C[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],-31\n"
        "sbci %B[op],-1\n"
        "sbci %C[op],-1\n"
        "not_negative_%=:\n"
        "asr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "asr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "asr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "asr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "asr %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline __int24 shift_s24_r6 (__int24 op)
{
    asm(
        "tst %C[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],-63\n"
        "sbci %B[op],-1\n"
        "sbci %C[op],-1\n"
        "not_negative_%=:\n"
        "clr __tmp_reg__\n"
        "sbrc %C[op],7\n"
        "com __tmp_reg__\n"
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "rol __tmp_reg__\n"
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "rol __tmp_reg__\n"
        "mov %A[op],%B[op]\n"
        "mov %B[op],%C[op]\n"
        "mov %C[op],__tmp_reg__\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline __int24 shift_s24_r7 (__int24 op)
{
    asm(
        "tst %C[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],-127\n"
        "sbci %B[op],-1\n"
        "sbci %C[op],-1\n"
        "not_negative_%=:\n"
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "mov %A[op],%B[op]\n"
        "mov %B[op],%C[op]\n"
        "clr %C[op]\n"
        "sbc %C[op],__zero_reg__\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_r1 (int32_t op)
{
    asm(
        "tst %D[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],-1\n"
        "sbci %B[op],-1\n"
        "sbci %C[op],-1\n"
        "sbci %D[op],-1\n"
        "not_negative_%=:\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_r2 (int32_t op)
{
    asm(
        "tst %D[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],-3\n"
        "sbci %B[op],-1\n"
        "sbci %C[op],-1\n"
        "sbci %D[op],-1\n"
        "not_negative_%=:\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_r3 (int32_t op)
{
    asm(
        "tst %D[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],-7\n"
        "sbci %B[op],-1\n"
        "sbci %C[op],-1\n"
        "sbci %D[op],-1\n"
        "not_negative_%=:\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_r4 (int32_t op)
{
    asm(
        "tst %D[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],-15\n"
        "sbci %B[op],-1\n"
        "sbci %C[op],-1\n"
        "sbci %D[op],-1\n"
        "not_negative_%=:\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_r5 (int32_t op)
{
    asm(
        "tst %D[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],-31\n"
        "sbci %B[op],-1\n"
        "sbci %C[op],-1\n"
        "sbci %D[op],-1\n"
        "not_negative_%=:\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_r6 (int32_t op)
{
    asm(
        "tst %D[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],-63\n"
        "sbci %B[op],-1\n"
        "sbci %C[op],-1\n"
        "sbci %D[op],-1\n"
        "not_negative_%=:\n"
        "clr __tmp_reg__\n"
        "sbrc %D[op],7\n"
        "com __tmp_reg__\n"
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "rol %D[op]\n"
        "rol __tmp_reg__\n"
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "rol %D[op]\n"
        "rol __tmp_reg__\n"
        "mov %A[op],%B[op]\n"
        "mov %B[op],%C[op]\n"
        "mov %C[op],%D[op]\n"
        "mov %D[op],__tmp_reg__\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_r7 (int32_t op)
{
    asm(
        "tst %D[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],-127\n"
        "sbci %B[op],-1\n"
        "sbci %C[op],-1\n"
        "sbci %D[op],-1\n"
        "not_negative_%=:\n"
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "rol %D[op]\n"
        "mov %A[op],%B[op]\n"
        "mov %B[op],%C[op]\n"
        "mov %C[op],%D[op]\n"
        "clr %D[op]\n"
        "sbc %D[op],__zero_reg__\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_r9 (int32_t op)
{
    asm(
        "tst %D[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],1\n"
        "sbci %B[op],-2\n"
        "sbci %C[op],-1\n"
        "sbci %D[op],-1\n"
        "not_negative_%=:\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "mov %A[op],%B[op]\n"
        "mov %B[op],%C[op]\n"
        "mov %C[op],%D[op]\n"
        "clr %D[op]\n"
        "sbrc %C[op],7\n"
        "com %D[op]\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_r10 (int32_t op)
{
    asm(
        "tst %D[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],1\n"
        "sbci %B[op],-4\n"
        "sbci %C[op],-1\n"
        "sbci %D[op],-1\n"
        "not_negative_%=:\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "mov %A[op],%B[op]\n"
        "mov %B[op],%C[op]\n"
        "mov %C[op],%D[op]\n"
        "clr %D[op]\n"
        "sbrc %C[op],7\n"
        "com %D[op]\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_r11 (int32_t op)
{
    asm(
        "tst %D[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],1\n"
        "sbci %B[op],-8\n"
        "sbci %C[op],-1\n"
        "sbci %D[op],-1\n"
        "not_negative_%=:\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "mov %A[op],%B[op]\n"
        "mov %B[op],%C[op]\n"
        "mov %C[op],%D[op]\n"
        "clr %D[op]\n"
        "sbrc %C[op],7\n"
        "com %D[op]\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_r12 (int32_t op)
{
    asm(
        "tst %D[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],1\n"
        "sbci %B[op],-16\n"
        "sbci %C[op],-1\n"
        "sbci %D[op],-1\n"
        "not_negative_%=:\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "asr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "mov %A[op],%B[op]\n"
        "mov %B[op],%C[op]\n"
        "mov %C[op],%D[op]\n"
        "clr %D[op]\n"
        "sbrc %C[op],7\n"
        "com %D[op]\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_r13 (int32_t op)
{
    asm(
        "tst %D[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],1\n"
        "sbci %B[op],-32\n"
        "sbci %C[op],-1\n"
        "sbci %D[op],-1\n"
        "not_negative_%=:\n"
        "clr __tmp_reg__\n"
        "sbrc %D[op],7\n"
        "com __tmp_reg__\n"
        "lsl %B[op]\n"
        "rol %C[op]\n"
        "rol %D[op]\n"
        "rol __tmp_reg__\n"
        "lsl %B[op]\n"
        "rol %C[op]\n"
        "rol %D[op]\n"
        "rol __tmp_reg__\n"
        "lsl %B[op]\n"
        "rol %C[op]\n"
        "rol %D[op]\n"
        "rol __tmp_reg__\n"
        "movw %A[op],%C[op]\n"
        "mov %C[op],__tmp_reg__\n"
        "clr %D[op]\n"
        "sbrc %C[op],7\n"
        "com %D[op]\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_r14 (int32_t op)
{
    asm(
        "tst %D[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],1\n"
        "sbci %B[op],-64\n"
        "sbci %C[op],-1\n"
        "sbci %D[op],-1\n"
        "not_negative_%=:\n"
        "clr __tmp_reg__\n"
        "sbrc %D[op],7\n"
        "com __tmp_reg__\n"
        "lsl %B[op]\n"
        "rol %C[op]\n"
        "rol %D[op]\n"
        "rol __tmp_reg__\n"
        "lsl %B[op]\n"
        "rol %C[op]\n"
        "rol %D[op]\n"
        "rol __tmp_reg__\n"
        "movw %A[op],%C[op]\n"
        "mov %C[op],__tmp_reg__\n"
        "clr %D[op]\n"
        "sbrc %C[op],7\n"
        "com %D[op]\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_r15 (int32_t op)
{
    asm(
        "tst %D[op]\n"
        "brpl not_negative_%=\n"
        "subi %A[op],1\n"
        "sbci %B[op],-128\n"
        "sbci %C[op],-1\n"
        "sbci %D[op],-1\n"
        "not_negative_%=:\n"
        "lsl %B[op]\n"
        "rol %C[op]\n"
        "rol %D[op]\n"
        "movw %A[op],%C[op]\n"
        "clr %C[op]\n"
        "clr %D[op]\n"
        "sbc %C[op],__zero_reg__\n"
        "sbc %D[op],__zero_reg__\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_l1 (int32_t op)
{
    asm(
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "rol %D[op]\n"
        : [op] "=&r" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_l2 (int32_t op)
{
    asm(
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "rol %D[op]\n"
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "rol %D[op]\n"
        : [op] "=&r" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_l3 (int32_t op)
{
    asm(
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "rol %D[op]\n"
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "rol %D[op]\n"
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "rol %D[op]\n"
        : [op] "=&r" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_l4 (int32_t op)
{
    asm(
        "swap %A[op]\n"
        "swap %B[op]\n"
        "swap %C[op]\n"
        "swap %D[op]\n"
        "andi %D[op],0xF0\n"
        "add %D[op],%C[op]\n"
        "andi %C[op],0xF0\n"
        "sub %D[op],%C[op]\n"
        "add %C[op],%B[op]\n"
        "andi %B[op],0xF0\n"
        "sub %C[op],%B[op]\n"
        "add %B[op],%A[op]\n"
        "andi %A[op],0xF0\n"
        "sub %B[op],%A[op]\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_l5 (int32_t op)
{
    asm(
        "swap %A[op]\n"
        "swap %B[op]\n"
        "swap %C[op]\n"
        "swap %D[op]\n"
        "andi %D[op],0xF0\n"
        "add %D[op],%C[op]\n"
        "andi %C[op],0xF0\n"
        "sub %D[op],%C[op]\n"
        "add %C[op],%B[op]\n"
        "andi %B[op],0xF0\n"
        "sub %C[op],%B[op]\n"
        "add %B[op],%A[op]\n"
        "andi %A[op],0xF0\n"
        "sub %B[op],%A[op]\n"
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "rol %D[op]\n"
        : [op] "=&d" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_l6 (int32_t op)
{
    asm(
        "clr __tmp_reg__\n"
        "lsr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "ror __tmp_reg__\n"
        "lsr %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        "ror __tmp_reg__\n"
        "mov %D[op],%C[op]\n"
        "mov %C[op],%B[op]\n"
        "mov %B[op],%A[op]\n"
        "mov %A[op],__tmp_reg__\n"
        : [op] "=&r" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_l7 (int32_t op)
{
    asm(
        "lsr %D[op]\n"
        "mov %D[op],%C[op]\n"
        "mov %C[op],%B[op]\n"
        "mov %B[op],%A[op]\n"
        "clr %A[op]\n"
        "ror %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "ror %A[op]\n"
        : [op] "=&r" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_l9 (int32_t op)
{
    asm(
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "mov %D[op],%C[op]\n"
        "mov %C[op],%B[op]\n"
        "mov %B[op],%A[op]\n"
        "clr %A[op]\n"
        : [op] "=&r" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_l10 (int32_t op)
{
    asm(
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "mov %D[op],%C[op]\n"
        "mov %C[op],%B[op]\n"
        "mov %B[op],%A[op]\n"
        "clr %A[op]\n"
        : [op] "=&r" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_l11 (int32_t op)
{
    asm(
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "mov %D[op],%C[op]\n"
        "mov %C[op],%B[op]\n"
        "mov %B[op],%A[op]\n"
        "clr %A[op]\n"
        : [op] "=&r" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_l12 (int32_t op)
{
    asm(
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "mov %D[op],%C[op]\n"
        "mov %C[op],%B[op]\n"
        "mov %B[op],%A[op]\n"
        "clr %A[op]\n"
        : [op] "=&r" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_l13 (int32_t op)
{
    asm(
        "mov __tmp_reg__,%C[op]\n"
        "movw %C[op],%A[op]\n"
        "clr %A[op]\n"
        "clr %B[op]\n"
        "lsr __tmp_reg__\n"
        "ror %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "lsr __tmp_reg__\n"
        "ror %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "lsr __tmp_reg__\n"
        "ror %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        : [op] "=&r" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_l14 (int32_t op)
{
    asm(
        "mov __tmp_reg__,%C[op]\n"
        "movw %C[op],%A[op]\n"
        "clr %A[op]\n"
        "clr %B[op]\n"
        "lsr __tmp_reg__\n"
        "ror %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        "lsr __tmp_reg__\n"
        "ror %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        : [op] "=&r" (op)
        : "[op]" (op)
    );
    return op;
}

static inline int32_t shift_s32_l15 (int32_t op)
{
    asm(
        "mov __tmp_reg__,%C[op]\n"
        "movw %C[op],%A[op]\n"
        "clr %A[op]\n"
        "clr %B[op]\n"
        "lsr __tmp_reg__\n"
        "ror %D[op]\n"
        "ror %C[op]\n"
        "ror %B[op]\n"
        : [op] "=&r" (op)
        : "[op]" (op)
    );
    return op;
}

#endif
