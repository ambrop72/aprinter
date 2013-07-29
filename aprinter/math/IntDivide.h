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

#ifndef AMBROLIB_INT_DIVIDE_H
#define AMBROLIB_INT_DIVIDE_H

#include <stdint.h>

#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/PowerOfTwo.h>

#ifdef AMBROLIB_AVR
#include <avr-asm-ops/div_13_16_l16_s15.h>
#endif

#include <aprinter/BeginNamespace.h>

template <int NumBits1, bool Signed1, int NumBits2, bool Signed2, int LeftShift, int ResSatBits, bool SupportZero>
class IntDivide {
public:
    static_assert(LeftShift >= 0, "LeftShift must be non-negative");
    
    typedef typename ChooseInt<NumBits1, Signed1>::Type Op1Type;
    typedef typename ChooseInt<NumBits2, Signed2>::Type Op2Type;
    typedef typename ChooseInt<ResSatBits, (Signed1 || Signed2)>::Type ResType;
    
    template <typename Option = int>
    __attribute__((always_inline)) inline static ResType call (Op1Type op1, Op2Type op2, Option opt = 0)
    {
        return
#ifdef AMBROLIB_AVR
            (LeftShift == 16 && ResSatBits == 15 && !Signed1 && NumBits1 > 8 && NumBits1 <= 13 && !Signed2 && NumBits2 <= 16) ? div_13_16_l16_s15(op1, op2, opt) :
#endif
            default_divide(op1, op2);
    }
    
private:
    typedef typename ChooseInt<(NumBits1 + LeftShift), (Signed1 || Signed2)>::Type TempResType;
    typedef typename ChooseInt<NumBits2, (Signed1 || Signed2)>::Type TempType2;
    
    static ResType default_divide (Op1Type op1, Op2Type op2)
    {
        if (SupportZero && op2 == 0) {
            return (op1 < 0) ? -PowerOfTwoMinusOne<ResType, ResSatBits>::value :
                   (op1 == 0) ? 0 :
                   PowerOfTwoMinusOne<ResType, ResSatBits>::value;
        }
        TempResType res = (((TempResType)op1 * PowerOfTwo<TempResType, LeftShift>::value) / (TempType2)op2);
        if (ResSatBits < NumBits1 + LeftShift) {
            if (res > PowerOfTwoMinusOne<ResType, ResSatBits>::value) {
                res = PowerOfTwoMinusOne<ResType, ResSatBits>::value;
            } else if (Signed1 || Signed2) {
                if (res < -PowerOfTwoMinusOne<ResType, ResSatBits>::value) {
                    res = -PowerOfTwoMinusOne<ResType, ResSatBits>::value;
                }
            }
        }
        return res;
    }
};

#include <aprinter/EndNamespace.h>

#endif
