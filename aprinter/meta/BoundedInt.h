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

#include <aprinter/meta/PowerOfTwo.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/Modulo.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/base/Assert.h>
#include <aprinter/math/IntSqrt.h>
#include <aprinter/math/IntMultiply.h>
#include <aprinter/math/IntDivide.h>
#include <aprinter/math/IntShift.h>

#include <aprinter/BeginNamespace.h>

template <int NumBits, bool Signed = true>
class BoundedInt {
public:
    static_assert(NumBits > 0, "");
    
    static const int num_bits = NumBits;
    
    using IntType = typename ChooseInt<NumBits, Signed>::Type;
    
    static constexpr IntType minIntValue ()
    {
        return Signed ? -PowerOfTwoMinusOne<IntType, NumBits>::value : 0;
    }
    
    static constexpr IntType maxIntValue ()
    {
        return PowerOfTwoMinusOne<IntType, NumBits>::value;
    }
    
    static BoundedInt minValue ()
    {
        return BoundedInt::import(minIntValue());
    }
    
    static BoundedInt maxValue ()
    {
        return BoundedInt::import(maxIntValue());
    }
    
    static BoundedInt import (IntType the_int)
    {
        AMBRO_ASSERT(the_int >= minIntValue())
        AMBRO_ASSERT(the_int <= maxIntValue())
        
#if 0
        BoundedInt res;
        res.m_int = the_int;
        return res;
#else
        return BoundedInt{the_int};
#endif
    }
    
    IntType value () const
    {
        return m_int;
    }
    
    template <int NewNumBits, bool NewSigned>
    operator BoundedInt<NewNumBits, NewSigned> () const
    {
        static_assert(NewNumBits >= NumBits, "");
        static_assert(!Signed || NewSigned, "");
        
        return BoundedInt<NewNumBits, NewSigned>::import(value());
    }
    
    BoundedInt<NumBits, true> toSigned () const
    {
        return BoundedInt<NumBits, true>::import(value());
    }
    
    BoundedInt<NumBits, false> toUnsignedUnsafe () const
    {
        AMBRO_ASSERT(value() >= 0)
        
        return BoundedInt<NumBits, false>::import(value());
    }
    
    template <int ShiftExp>
    BoundedInt<NumBits - ShiftExp, Signed> shiftRight () const
    {
        static_assert(ShiftExp >= 0, "");
        
        return BoundedInt<NumBits - ShiftExp, Signed>::import(IntShiftRight<NumBits, Signed, ShiftExp>::call(value()));
    }
    
    template <int ShiftExp>
    BoundedInt<NumBits + ShiftExp, Signed> shiftLeft () const
    {
        static_assert(ShiftExp >= 0, "");
        
        return BoundedInt<NumBits + ShiftExp, Signed>::import(IntShiftLeft<NumBits, Signed, ShiftExp>::call(value()));
    }
    
    template <int ShiftExp>
    BoundedInt<NumBits - ShiftExp, Signed> shift () const
    {
        return ShiftHelper<ShiftExp, (ShiftExp < 0)>::call(*this);
    }
    
    BoundedInt<NumBits, false> absVal () const
    {
        return BoundedInt<NumBits, false>::import((value() >= 0) ? value() : -value());
    }
    
private:
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

template <int NumBits1, bool Signed1, int NumBits2, bool Signed2>
bool operator== (BoundedInt<NumBits1, Signed1> op1, BoundedInt<NumBits2, Signed2> op2)
{
    using TempType = typename ChooseInt<max(NumBits1, NumBits2), (Signed1 || Signed2)>::Type;
    return ((TempType)op1.value() == (TempType)op2.value());
}

template <int NumBits1, bool Signed1, int NumBits2, bool Signed2>
bool operator!= (BoundedInt<NumBits1, Signed1> op1, BoundedInt<NumBits2, Signed2> op2)
{
    using TempType = typename ChooseInt<max(NumBits1, NumBits2), (Signed1 || Signed2)>::Type;
    return ((TempType)op1.value() != (TempType)op2.value());
}

template <int NumBits1, bool Signed1, int NumBits2, bool Signed2>
bool operator< (BoundedInt<NumBits1, Signed1> op1, BoundedInt<NumBits2, Signed2> op2)
{
    using TempType = typename ChooseInt<max(NumBits1, NumBits2), (Signed1 || Signed2)>::Type;
    return ((TempType)op1.value() < (TempType)op2.value());
}

template <int NumBits1, bool Signed1, int NumBits2, bool Signed2>
bool operator> (BoundedInt<NumBits1, Signed1> op1, BoundedInt<NumBits2, Signed2> op2)
{
    using TempType = typename ChooseInt<max(NumBits1, NumBits2), (Signed1 || Signed2)>::Type;
    return ((TempType)op1.value() > (TempType)op2.value());
}

template <int NumBits1, bool Signed1, int NumBits2, bool Signed2>
bool operator<= (BoundedInt<NumBits1, Signed1> op1, BoundedInt<NumBits2, Signed2> op2)
{
    using TempType = typename ChooseInt<max(NumBits1, NumBits2), (Signed1 || Signed2)>::Type;
    return ((TempType)op1.value() <= (TempType)op2.value());
}

template <int NumBits1, bool Signed1, int NumBits2, bool Signed2>
bool operator>= (BoundedInt<NumBits1, Signed1> op1, BoundedInt<NumBits2, Signed2> op2)
{
    using TempType = typename ChooseInt<max(NumBits1, NumBits2), (Signed1 || Signed2)>::Type;
    return ((TempType)op1.value() >= (TempType)op2.value());
}

template <int NumBits, bool Signed>
BoundedInt<NumBits, true> operator- (BoundedInt<NumBits, Signed> op)
{
    using TempType = typename ChooseInt<NumBits, true>::Type;
    return BoundedInt<NumBits, true>::import(-(TempType)op.value());
}

template <int NumBits1, bool Signed1, int NumBits2, bool Signed2>
BoundedInt<(max(NumBits1, NumBits2) + 1), (Signed1 || Signed2)> operator+ (BoundedInt<NumBits1, Signed1> op1, BoundedInt<NumBits2, Signed2> op2)
{
    using TempType = typename ChooseInt<(max(NumBits1, NumBits2) + 1), (Signed1 || Signed2)>::Type;
    return BoundedInt<(max(NumBits1, NumBits2) + 1), (Signed1 || Signed2)>::import((TempType)op1.value() + (TempType)op2.value());
}

template <int NumBits1, bool Signed1, int NumBits2, bool Signed2>
BoundedInt<(max(NumBits1, NumBits2) + 1), true> operator- (BoundedInt<NumBits1, Signed1> op1, BoundedInt<NumBits2, Signed2> op2)
{
    using TempType = typename ChooseInt<(max(NumBits1, NumBits2) + 1), true>::Type;
    return BoundedInt<(max(NumBits1, NumBits2) + 1), true>::import((TempType)op1.value() - (TempType)op2.value());
}

template <int RightShift, int NumBits1, bool Signed1, int NumBits2, bool Signed2>
BoundedInt<(NumBits1 + NumBits2 - RightShift), (Signed1 || Signed2)> BoundedMultiply (BoundedInt<NumBits1, Signed1> op1, BoundedInt<NumBits2, Signed2> op2)
{
    return BoundedInt<(NumBits1 + NumBits2 - RightShift), (Signed1 || Signed2)>::import(
        IntMultiply<NumBits1, Signed1, NumBits2, Signed2, RightShift>::call(op1.value(), op2.value())
    );
}

template <int NumBits1, bool Signed1, int NumBits2, bool Signed2>
BoundedInt<(NumBits1 + NumBits2), (Signed1 || Signed2)> operator* (BoundedInt<NumBits1, Signed1> op1, BoundedInt<NumBits2, Signed2> op2)
{
    return BoundedMultiply<0>(op1, op2);
}

template <int LeftShift, int ResSatBits, bool SupportZero, int NumBits1, bool Signed1, int NumBits2, bool Signed2, typename Option = int>
__attribute__((always_inline)) inline BoundedInt<ResSatBits, (Signed1 || Signed2)> BoundedDivide (BoundedInt<NumBits1, Signed1> op1, BoundedInt<NumBits2, Signed2> op2, Option opt = 0)
{
    AMBRO_ASSERT(!!SupportZero || op2.value() != 0)
    
    return BoundedInt<ResSatBits, (Signed1 || Signed2)>::import(
        IntDivide<NumBits1, Signed1, NumBits2, Signed2, LeftShift, ResSatBits, SupportZero>::call(op1.value(), op2.value(), opt)
    );
}

template <int NumBits1, bool Signed1, int NumBits2, bool Signed2>
BoundedInt<NumBits1, (Signed1 || Signed2)> operator/ (BoundedInt<NumBits1, Signed1> op1, BoundedInt<NumBits2, Signed2> op2)
{
    return BoundedDivide<0, NumBits1, false>(op1, op2);
}

template <int NumBits, bool Signed, typename Option = int>
__attribute__((always_inline)) inline BoundedInt<((NumBits + 1) / 2), false> BoundedSquareRoot (BoundedInt<NumBits, Signed> op1, Option opt = 0)
{
    AMBRO_ASSERT(op1.value() >= 0)
    
    return BoundedInt<((NumBits + 1) / 2), false>::import(IntSqrt<NumBits>::call(op1.value(), opt));
}

template <int NumBits>
BoundedInt<NumBits, false> BoundedModuloAdd (BoundedInt<NumBits, false> op1, BoundedInt<NumBits, false> op2)
{
    return BoundedInt<NumBits, false>::import((typename BoundedInt<NumBits, false>::IntType)(op1.value() + op2.value()) & BoundedInt<NumBits, false>::maxIntValue());
}

template <int NumBits>
BoundedInt<NumBits, false> BoundedModuloSubtract (BoundedInt<NumBits, false> op1, BoundedInt<NumBits, false> op2)
{
    return BoundedInt<NumBits, false>::import((typename BoundedInt<NumBits, false>::IntType)(op1.value() - op2.value()) & BoundedInt<NumBits, false>::maxIntValue());
}

template <int NumBits>
BoundedInt<NumBits, false> BoundedModuloNegative (BoundedInt<NumBits, false> op)
{
    return BoundedInt<NumBits, false>::import((typename BoundedInt<NumBits, false>::IntType)(-op.value()) & BoundedInt<NumBits, false>::maxIntValue());
}

template <int NumBits>
BoundedInt<NumBits, false> BoundedModuloInc (BoundedInt<NumBits, false> op)
{
    return BoundedInt<NumBits, false>::import((typename BoundedInt<NumBits, false>::IntType)(op.value() + 1) & BoundedInt<NumBits, false>::maxIntValue());
}

template <int NumBits>
BoundedInt<NumBits, false> BoundedModuloDec (BoundedInt<NumBits, false> op)
{
    return BoundedInt<NumBits, false>::import((typename BoundedInt<NumBits, false>::IntType)(op.value() - 1) & BoundedInt<NumBits, false>::maxIntValue());
}

template <int NumBits, bool Signed>
BoundedInt<NumBits, Signed> BoundedUnsafeAdd (BoundedInt<NumBits, Signed> op1, BoundedInt<NumBits, Signed> op2)
{
    return BoundedInt<NumBits, Signed>::import((typename BoundedInt<NumBits, Signed>::IntType)(op1.value() + op2.value()));
}

template <int NumBits, bool Signed>
BoundedInt<NumBits, Signed> BoundedUnsafeSubtract (BoundedInt<NumBits, Signed> op1, BoundedInt<NumBits, Signed> op2)
{
    return BoundedInt<NumBits, Signed>::import((typename BoundedInt<NumBits, Signed>::IntType)(op1.value() - op2.value()));
}

template <int NumBits>
BoundedInt<NumBits, true> BoundedUnsafeNegative (BoundedInt<NumBits, true> op)
{
    return BoundedInt<NumBits, true>::import((typename BoundedInt<NumBits, true>::IntType)(-op.value()));
}

template <int NumBits, bool Signed>
BoundedInt<NumBits, Signed> BoundedUnsafeInc (BoundedInt<NumBits, Signed> op)
{
    return BoundedInt<NumBits, Signed>::import((typename BoundedInt<NumBits, Signed>::IntType)(op.value() + 1));
}

template <int NumBits, bool Signed>
BoundedInt<NumBits, Signed> BoundedUnsafeDec (BoundedInt<NumBits, Signed> op)
{
    return BoundedInt<NumBits, Signed>::import((typename BoundedInt<NumBits, Signed>::IntType)(op.value() - 1));
}

#include <aprinter/EndNamespace.h>

#endif
