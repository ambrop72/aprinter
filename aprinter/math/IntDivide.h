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
#ifdef AMBROLIB_AVR
#include <avr-asm-ops/div_32_32_large.h>
#include <avr-asm-ops/div_32_16_large.h>
#include <avr-asm-ops/div_29_16_large.h>
#endif

#include <aprinter/BeginNamespace.h>

template <int NumBits1, bool Signed1, int NumBits2, bool Signed2>
class IntDivide {
public:
    typedef typename ChooseInt<NumBits1, Signed1>::Type IntType1;
    typedef typename ChooseInt<NumBits2, Signed2>::Type IntType2;
    typedef typename ChooseInt<NumBits1, (Signed1 || Signed2)>::Type ResType;
    static_assert(Signed1 == Signed2, "division of operands with different signedness not supported");
    
    static ResType call (IntType1 op1, IntType2 op2)
    {
        return
#ifdef AMBROLIB_AVR
            (!Signed1 && NumBits1 <= 29 && !Signed2 && NumBits2 <= 16) ? div_29_16_large(op1, op2) :
            (!Signed1 && NumBits1 <= 32 && !Signed2 && NumBits2 <= 16) ? div_32_16_large(op1, op2) :
            (!Signed1 && NumBits1 <= 32 && !Signed2 && NumBits2 <= 32) ? div_32_32_large(op1, op2) :
#endif
            default_divide(op1, op2);
    }
    
private:
    static ResType default_divide (IntType1 op1, IntType2 op2)
    {
        return (op1 / op2);
    }
};

#include <aprinter/EndNamespace.h>

#endif
