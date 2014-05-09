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
    typedef ChooseInt<NumBits, Signed> OpType;
    typedef ChooseInt<NumBits - ShiftCount, Signed> ResType;
    
    template <typename Option = int>
    __attribute__((always_inline)) inline static ResType call (OpType op)
    {
        return
#ifdef AMBROLIB_AVR
#if 0
            (Signed && NumBits > 15 && NumBits <= 23 && ShiftCount == 7) ? shift_s24_r7(op) :
            (Signed && NumBits > 15 && NumBits <= 23 && ShiftCount == 6) ? shift_s24_r6(op) :
            (Signed && NumBits > 15 && NumBits <= 23 && ShiftCount == 5) ? shift_s24_r5(op) :
            (Signed && NumBits > 15 && NumBits <= 23 && ShiftCount == 4) ? shift_s24_r4(op) :
            (Signed && NumBits > 15 && NumBits <= 23 && ShiftCount == 3) ? shift_s24_r3(op) :
            (Signed && NumBits > 15 && NumBits <= 23 && ShiftCount == 2) ? shift_s24_r2(op) :
            (Signed && NumBits > 15 && NumBits <= 23 && ShiftCount == 1) ? shift_s24_r1(op) :
#endif
            (!Signed && NumBits > 16 && NumBits <= 32 && ShiftCount == 3) ? shift_32_r3(op) :
            (Signed && NumBits > 15 && NumBits <= 31 && ShiftCount == 15) ? shift_s32_r15(op) :
            (Signed && NumBits > 15 && NumBits <= 31 && ShiftCount == 14) ? shift_s32_r14(op) :
            (Signed && NumBits > 15 && NumBits <= 31 && ShiftCount == 13) ? shift_s32_r13(op) :
            (Signed && NumBits > 15 && NumBits <= 31 && ShiftCount == 12) ? shift_s32_r12(op) :
            (Signed && NumBits > 15 && NumBits <= 31 && ShiftCount == 11) ? shift_s32_r11(op) :
            (Signed && NumBits > 15 && NumBits <= 31 && ShiftCount == 10) ? shift_s32_r10(op) :
            (Signed && NumBits > 15 && NumBits <= 31 && ShiftCount == 9) ? shift_s32_r9(op) :
            (Signed && NumBits > 15 && NumBits <= 31 && ShiftCount == 7) ? shift_s32_r7(op) :
            (Signed && NumBits > 15 && NumBits <= 31 && ShiftCount == 6) ? shift_s32_r6(op) :
            (Signed && NumBits > 15 && NumBits <= 31 && ShiftCount == 5) ? shift_s32_r5(op) :
            (Signed && NumBits > 15 && NumBits <= 31 && ShiftCount == 4) ? shift_s32_r4(op) :
            (Signed && NumBits > 15 && NumBits <= 31 && ShiftCount == 3) ? shift_s32_r3(op) :
            (Signed && NumBits > 15 && NumBits <= 31 && ShiftCount == 2) ? shift_s32_r2(op) :
            (Signed && NumBits > 15 && NumBits <= 31 && ShiftCount == 1) ? shift_s32_r1(op) :
#endif
            (op / PowerOfTwo<OpType, ShiftCount>::value);
    }
};

template <int NumBits, bool Signed, int ShiftCount>
class IntShiftLeft {
public:
    static_assert(ShiftCount >= 0, "");
    typedef ChooseInt<NumBits, Signed> OpType;
    static const int ResBits = NumBits + ShiftCount;
    typedef ChooseInt<ResBits, Signed> ResType;
    
    template <typename Option = int>
    __attribute__((always_inline)) inline static ResType call (OpType op)
    {
        return
#ifdef AMBROLIB_AVR
            (Signed && ResBits > 15 && ResBits <= 31 && ShiftCount == 1) ? shift_s32_l1(op) :
            (Signed && ResBits > 15 && ResBits <= 31 && ShiftCount == 2) ? shift_s32_l2(op) :
            (Signed && ResBits > 15 && ResBits <= 31 && ShiftCount == 3) ? shift_s32_l3(op) :
            (Signed && ResBits > 15 && ResBits <= 31 && ShiftCount == 4) ? shift_s32_l4(op) :
            (Signed && ResBits > 15 && ResBits <= 31 && ShiftCount == 5) ? shift_s32_l5(op) :
            (Signed && ResBits > 15 && ResBits <= 31 && ShiftCount == 6) ? shift_s32_l6(op) :
            (Signed && ResBits > 15 && ResBits <= 31 && ShiftCount == 7) ? shift_s32_l7(op) :
            (Signed && ResBits > 15 && ResBits <= 31 && ShiftCount == 9) ? shift_s32_l9(op) :
            (Signed && ResBits > 15 && ResBits <= 31 && ShiftCount == 10) ? shift_s32_l10(op) :
            (Signed && ResBits > 15 && ResBits <= 31 && ShiftCount == 11) ? shift_s32_l11(op) :
            (Signed && ResBits > 15 && ResBits <= 31 && ShiftCount == 12) ? shift_s32_l12(op) :
            (Signed && ResBits > 15 && ResBits <= 31 && ShiftCount == 13) ? shift_s32_l13(op) :
            (Signed && ResBits > 15 && ResBits <= 31 && ShiftCount == 14) ? shift_s32_l14(op) :
            (Signed && ResBits > 15 && ResBits <= 31 && ShiftCount == 15) ? shift_s32_l15(op) :
#endif
            (op * PowerOfTwo<ResType, ShiftCount>::value);
    }
};

template <int NumBits, bool Signed, int ShiftCount>
class IntUndoShiftLeft {
public:
    static_assert(ShiftCount >= 0, "");
    typedef ChooseInt<NumBits, Signed> OpType;
    typedef ChooseInt<NumBits - ShiftCount, Signed> ResType;
    
    template <typename Option = int>
    __attribute__((always_inline)) inline static ResType call (OpType op)
    {
        // WARNING relying on implementation defined semantics
        // of right-shifting negative integers
        return (op >> ShiftCount);
    }
};

#include <aprinter/EndNamespace.h>

#endif
