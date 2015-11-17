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

#ifndef AMBROLIB_INT_MULTIPLY_H
#define AMBROLIB_INT_MULTIPLY_H

#include <stdint.h>

#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/PowerOfTwo.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/math/IntShift.h>

#ifdef AMBROLIB_AVR
#include <aprinter/avr-asm-ops/mul.h>
#endif

#include <aprinter/BeginNamespace.h>

template <int NumBits1, bool Signed1, int NumBits2, bool Signed2, int RightShift>
class IntMultiply {
public:
    static_assert(RightShift >= 0, "RightShift must be non-negative");
    static_assert(RightShift < NumBits1 + NumBits2, "RightShift must be less than multiplication result width");
    
    typedef ChooseInt<NumBits1, Signed1> Op1Type;
    typedef ChooseInt<NumBits2, Signed2> Op2Type;
    typedef ChooseInt<(NumBits1 + NumBits2 - RightShift), (Signed1 || Signed2)> ResType;
    
    static const bool Signed = Signed1 || Signed2;
    static const int TempBits = NumBits1 + NumBits2;
    
    template <typename Option = int>
    __attribute__((always_inline)) inline static ResType call (Op1Type op1, Op2Type op2, Option opt = 0)
    {
        return
#ifdef AMBROLIB_AVR
            (RightShift >= 0 && !Signed1 && NumBits1 > 8 && NumBits1 <= 16 && !Signed2 && NumBits2 > 8 && NumBits2 <= 16) ?
                IntShiftRight<MaxValue(1, TempBits - 0), Signed, MaxValue(0, RightShift - 0)>::call(mul_16_16(op1, op2)) :
            (RightShift >= 15 && !Signed1 && NumBits1 > 16 && NumBits1 <= 24 && !Signed2 && NumBits2 > 8 && NumBits2 <= 16) ?
                IntShiftRight<MaxValue(1, TempBits - 15), Signed, MaxValue(0, RightShift - 15)>::call(mul_24_16_r15(op1, op2)) :
#endif
            default_multiply(op1, op2);
    }
    
private:
    typedef ChooseInt<(NumBits1 + NumBits2), (Signed1 || Signed2)> TempResType;
    
    static ResType default_multiply (Op1Type op1, Op2Type op2)
    {
        return IntShiftRight<(NumBits1 + NumBits2), (Signed1 || Signed2), RightShift>::call((TempResType)op1 * (TempResType)op2);
    }
};

#include <aprinter/EndNamespace.h>

#endif
