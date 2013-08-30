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

static inline int32_t shift_s32_r7 (int32_t op)
{
    asm(
        "lsl %A[op]\n"
        "rol %B[op]\n"
        "rol %C[op]\n"
        "rol %D[op]\n"
        "mov %A[op],%B[op]\n"
        "mov %B[op],%C[op]\n"
        "mov %C[op],%D[op]\n"
        "clr %D[op]\n"
        "sbc %D[op],__zero_reg__\n"
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

#endif
