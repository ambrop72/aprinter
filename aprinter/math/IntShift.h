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

#ifndef AMBROLIB_INT_SHIFT_H
#define AMBROLIB_INT_SHIFT_H

#include <stdint.h>

#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/PowerOfTwo.h>

#ifdef AMBROLIB_AVR
#include <avr-asm-ops/shift.h>
#endif

#include <aprinter/BeginNamespace.h>

template <int NumBits, bool Signed, int ShiftCount>
class IntShiftRight {
public:
    static_assert(ShiftCount >= 0, "");
    typedef typename ChooseInt<NumBits, Signed>::Type OpType;
    typedef typename ChooseInt<NumBits - ShiftCount, Signed>::Type ResType;
    
    template <typename Option = int>
    __attribute__((always_inline)) inline static ResType call (OpType op)
    {
        return
#ifdef AMBROLIB_AVR
            (Signed && NumBits > 15 && NumBits <= 23 && ShiftCount == 7) ? shift_s24_r7(op) :
            (Signed && NumBits > 15 && NumBits <= 23 && ShiftCount == 6) ? shift_s24_r6(op) :
            (Signed && NumBits > 15 && NumBits <= 23 && ShiftCount == 5) ? shift_s24_r5(op) :
            (Signed && NumBits > 15 && NumBits <= 23 && ShiftCount == 4) ? shift_s24_r4(op) :
            (Signed && NumBits > 15 && NumBits <= 23 && ShiftCount == 3) ? shift_s24_r3(op) :
            (Signed && NumBits > 15 && NumBits <= 23 && ShiftCount == 2) ? shift_s24_r2(op) :
            (Signed && NumBits > 15 && NumBits <= 23 && ShiftCount == 1) ? shift_s24_r1(op) :
            (Signed && NumBits > 23 && NumBits <= 31 && ShiftCount == 15) ? shift_s32_r15(op) :
            (Signed && NumBits > 23 && NumBits <= 31 && ShiftCount == 14) ? shift_s32_r14(op) :
            (Signed && NumBits > 23 && NumBits <= 31 && ShiftCount == 13) ? shift_s32_r13(op) :
            (Signed && NumBits > 23 && NumBits <= 31 && ShiftCount == 12) ? shift_s32_r12(op) :
            (Signed && NumBits > 23 && NumBits <= 31 && ShiftCount == 11) ? shift_s32_r11(op) :
            (Signed && NumBits > 23 && NumBits <= 31 && ShiftCount == 10) ? shift_s32_r10(op) :
            (Signed && NumBits > 23 && NumBits <= 31 && ShiftCount == 9) ? shift_s32_r9(op) :
            (Signed && NumBits > 23 && NumBits <= 31 && ShiftCount == 7) ? shift_s32_r7(op) :
            (Signed && NumBits > 23 && NumBits <= 31 && ShiftCount == 6) ? shift_s32_r6(op) :
            (Signed && NumBits > 23 && NumBits <= 31 && ShiftCount == 5) ? shift_s32_r5(op) :
            (Signed && NumBits > 23 && NumBits <= 31 && ShiftCount == 4) ? shift_s32_r4(op) :
            (Signed && NumBits > 23 && NumBits <= 31 && ShiftCount == 3) ? shift_s32_r3(op) :
            (Signed && NumBits > 23 && NumBits <= 31 && ShiftCount == 2) ? shift_s32_r2(op) :
            (Signed && NumBits > 23 && NumBits <= 31 && ShiftCount == 1) ? shift_s32_r1(op) :
#endif
            (op / PowerOfTwo<OpType, ShiftCount>::value);
    }
};

template <int NumBits, bool Signed, int ShiftCount>
class IntShiftLeft {
public:
    static_assert(ShiftCount >= 0, "");
    typedef typename ChooseInt<NumBits, Signed>::Type OpType;
    static const int ResBits = NumBits + ShiftCount;
    typedef typename ChooseInt<ResBits, Signed>::Type ResType;
    
    template <typename Option = int>
    __attribute__((always_inline)) inline static ResType call (OpType op)
    {
        return
#ifdef AMBROLIB_AVR
            (Signed && ResBits > 23 && ResBits <= 31 && ShiftCount == 7) ? shift_s32_l7(op) :
            (Signed && ResBits > 23 && ResBits <= 31 && ShiftCount == 9) ? shift_s32_l9(op) :
            (Signed && ResBits > 23 && ResBits <= 31 && ShiftCount == 10) ? shift_s32_l10(op) :
            (Signed && ResBits > 23 && ResBits <= 31 && ShiftCount == 11) ? shift_s32_l11(op) :
            (Signed && ResBits > 23 && ResBits <= 31 && ShiftCount == 12) ? shift_s32_l12(op) :
            (Signed && ResBits > 23 && ResBits <= 31 && ShiftCount == 13) ? shift_s32_l13(op) :
            (Signed && ResBits > 23 && ResBits <= 31 && ShiftCount == 14) ? shift_s32_l14(op) :
            (Signed && ResBits > 23 && ResBits <= 31 && ShiftCount == 15) ? shift_s32_l15(op) :
#endif
            (op * PowerOfTwo<ResType, ShiftCount>::value);
    }
};

#include <aprinter/EndNamespace.h>

#endif
