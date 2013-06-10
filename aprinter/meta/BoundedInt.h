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

#ifndef AMBROLIB_BOUNDED_INT_H
#define AMBROLIB_BOUNDED_INT_H

#include <stdint.h>

#include <aprinter/meta/If.h>
#include <aprinter/meta/PowerOfTwo.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/IntTypeInfo.h>
#include <aprinter/meta/Modulo.h>
#include <aprinter/base/Assert.h>
#include <aprinter/math/IntSqrt.h>
#include <aprinter/math/IntMultiply.h>
#include <aprinter/math/IntDivide.h>

#include <aprinter/BeginNamespace.h>

template <int NumBits, bool Signed = true>
class BoundedInt {
public:
    static_assert(NumBits > 0, "");
    static_assert((!Signed || NumBits < 32) && (!!Signed || NumBits <= 32), "Too many bits, fix your operations.");
    //static_assert(NumBits < 64, "Too many bits, fix your operations.");
    
    static const int num_bits = NumBits;
    
    typedef
        typename If<(Signed && NumBits < 8), int8_t,
        typename If<(Signed && NumBits < 16), int16_t,
        typename If<(Signed && NumBits < 32), int32_t,
        typename If<(Signed && NumBits < 64), int64_t,
        typename If<(!Signed && NumBits <= 8), uint8_t,
        typename If<(!Signed && NumBits <= 16), uint16_t,
        typename If<(!Signed && NumBits <= 32), uint32_t,
        typename If<(!Signed && NumBits <= 64), uint64_t,
        void>::Type>::Type>::Type>::Type>::Type>::Type>::Type>::Type IntType;
    
    static IntType minValue ()
    {
        return Signed ? -PowerOfTwoMinusOne<IntType, NumBits>::value : 0;
    }
    
    static IntType maxValue ()
    {
        return PowerOfTwoMinusOne<IntType, NumBits>::value;
    }
    
    static BoundedInt import (IntType the_int)
    {
        AMBRO_ASSERT(the_int >= minValue())
        AMBRO_ASSERT(the_int <= maxValue())
        
        BoundedInt res = {the_int};
        return res;
    }
    
    IntType value () const
    {
        return m_int;
    }
    
    template <int NewNumBits, bool NewSigned>
    BoundedInt<NewNumBits, NewSigned> convert () const
    {
        static_assert(NewNumBits >= NumBits, "");
        static_assert(!Signed || NewSigned, "");
        
        return BoundedInt<NewNumBits, NewSigned>::import(m_int);
    }
    
    BoundedInt<NumBits, true> toSigned () const
    {
        return BoundedInt<NumBits, true>::import(m_int);
    }
    
    BoundedInt<NumBits, false> toUnsignedUnsafe () const
    {
        AMBRO_ASSERT(m_int >= 0)
        
        return BoundedInt<NumBits, false>::import(m_int);
    }
    
    template <int ShiftExp>
    BoundedInt<NumBits - ShiftExp, Signed> shiftRight () const
    {
        static_assert(ShiftExp >= 0, "");
        
        return BoundedInt<NumBits - ShiftExp, Signed>::import(m_int / PowerOfTwo<IntType, ShiftExp>::value);
    }
    
    template <int ShiftExp>
    BoundedInt<NumBits + ShiftExp, Signed> shiftLeft () const
    {
        static_assert(ShiftExp >= 0, "");
        
        return BoundedInt<NumBits + ShiftExp, Signed>::import(m_int * PowerOfTwo<typename BoundedInt<NumBits + ShiftExp, Signed>::IntType, ShiftExp>::value);
    }
    
    template <int ShiftExp>
    BoundedInt<NumBits - ShiftExp, Signed> shift () const
    {
        return ShiftHelper<ShiftExp, (ShiftExp < 0)>::call(*this);
    }
    
    BoundedInt operator- () const
    {
        // TODO remove for unsigned
        AMBRO_ASSERT(Signed)
        return import(-m_int);
    }
    
    template <int NumBits2>
    BoundedInt<max(NumBits, NumBits2) + 1, Signed> operator+ (BoundedInt<NumBits2, Signed> op2) const
    {
        return BoundedInt<max(NumBits, NumBits2) + 1, Signed>::import(
            (typename BoundedInt<max(NumBits, NumBits2) + 1, Signed>::IntType)m_int +
            (typename BoundedInt<max(NumBits, NumBits2) + 1, Signed>::IntType)op2.m_int
        );
    }
    
    template <int NumBits2>
    BoundedInt<max(NumBits, NumBits2) + 1, Signed> operator- (BoundedInt<NumBits2, Signed> op2) const
    {
        return BoundedInt<max(NumBits, NumBits2) + 1, Signed>::import(
            (typename BoundedInt<max(NumBits, NumBits2) + 1, Signed>::IntType)m_int -
            (typename BoundedInt<max(NumBits, NumBits2) + 1, Signed>::IntType)op2.m_int
        );
    }
    
    template <int NumBits2, bool Signed2>
    BoundedInt<NumBits + NumBits2, (Signed || Signed2)> operator* (BoundedInt<NumBits2, Signed2> op2) const
    {
        return BoundedInt<NumBits + NumBits2, (Signed || Signed2)>::import(
            IntMultiply<
                IntType,
                typename BoundedInt<NumBits2, Signed2>::IntType,
                typename BoundedInt<NumBits + NumBits2, (Signed || Signed2)>::IntType
            >::call(m_int, op2.m_int)
        );
    }
    
    template <int NumBits2, bool Signed2>
    BoundedInt<NumBits, (Signed || Signed2)> operator/ (BoundedInt<NumBits2, Signed2> op2) const
    {
        AMBRO_ASSERT(op2.m_int != 0)
        
        return BoundedInt<NumBits, (Signed || Signed2)>::import(
            IntDivide<
                typename BoundedInt<NumBits, Signed>::IntType,
                typename BoundedInt<NumBits2, Signed2>::IntType
            >::call(m_int, op2.m_int)
        );
    }
    
    BoundedInt<max(1, ((NumBits / 2) + Modulo(NumBits, 2))), false> squareRoot () const
    {
        AMBRO_ASSERT(m_int >= 0)
        
        return BoundedInt<max(1, ((NumBits / 2) + Modulo(NumBits, 2))), false>::import(IntSqrt<typename IntTypeInfo<IntType>::UnsignedType>::call(m_int));
    }
    
    template <int NumBits2>
    bool operator< (BoundedInt<NumBits2, Signed> op2) const
    {
        return (m_int < op2.m_int);
    }
    
    template <int ShiftExp, bool Left>
    struct ShiftHelper;
    
    template <int ShiftExp>
    struct ShiftHelper<ShiftExp, false> {
        static BoundedInt<NumBits - ShiftExp, Signed> call (BoundedInt op)
        {
            return op.shiftRight<ShiftExp>();
        }
    };
    
    template <int ShiftExp>
    struct ShiftHelper<ShiftExp, true> {
        static BoundedInt<NumBits - ShiftExp, Signed> call (BoundedInt op)
        {
            return op.shiftLeft<(-ShiftExp)>();
        }
    };
    
public:
    IntType m_int;
};

#include <aprinter/EndNamespace.h>

#endif
