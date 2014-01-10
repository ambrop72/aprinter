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

#ifndef AMBROLIB_FIXED_POINT_H
#define AMBROLIB_FIXED_POINT_H

#include <math.h>

#include <aprinter/meta/BoundedInt.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/Modulo.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Inline.h>

#include <aprinter/BeginNamespace.h>

struct FixedIdentity {};

template <int NumBits, bool Signed, int Exp>
class FixedPoint {
public:
    static const int num_bits = NumBits;
    static const bool is_signed = Signed;
    static const int exp = Exp;
    using BoundedIntType = BoundedInt<NumBits, Signed>;
    using IntType = typename BoundedIntType::IntType;
    
    static FixedPoint importBoundedBits (BoundedIntType op)
    {
        return FixedPoint{op};
    }
    
    static FixedPoint importBits (IntType op)
    {
        return importBoundedBits(BoundedIntType::import(op));
    }
    
    static FixedPoint minValue ()
    {
        return importBoundedBits(BoundedIntType::minValue());
    }
    
    static FixedPoint maxValue ()
    {
        return importBoundedBits(BoundedIntType::maxValue());
    }
    
    BoundedIntType bitsBoundedValue () const
    {
        return m_bits;
    }
    
    IntType bitsValue () const
    {
        return m_bits.value();
    }
    
    static FixedPoint importDouble (double op)
    {
#ifdef AMBROLIB_ASSERTIONS
        double a = trunc(ldexp(op, -Exp));
        AMBRO_ASSERT(!Signed || a > -ldexp(1.0, NumBits))
        AMBRO_ASSERT(Signed || a >= 0.0)
        AMBRO_ASSERT(a < ldexp(1.0, NumBits))
#endif
        
        if (Exp == 0) {
            return importBits(trunc(op));
        } else {
            return importBits(trunc(ldexp(op, -Exp)));
        }
    }
    
    static FixedPoint importDoubleSaturatedRound (double op)
    {
        return importDoubleSaturatedRoundInline(op);
    }
    
    __attribute__((always_inline)) inline static FixedPoint importDoubleSaturatedRoundInline (double op)
    {
        double a = round(ldexp(op, -Exp));
        if (a <= (Signed ? -ldexp(1.0, NumBits) : 0.0)) {
            return minValue();
        }
        if (a >= ldexp(1.0, NumBits)) {
            return maxValue();
        }
        return importBits(a);
    }
    
    double doubleValue () const
    {
        if (Exp == 0) {
            return bitsValue();
        } else {
            return ldexp(bitsValue(), Exp);
        }
    }
    
    FixedPoint<NumBits, true, Exp> toSigned () const
    {
        return FixedPoint<NumBits, true, Exp>::importBoundedBits(m_bits.toSigned());
    }
    
    FixedPoint<NumBits, false, Exp> toUnsignedUnsafe () const
    {
        return FixedPoint<NumBits, false, Exp>::importBoundedBits(m_bits.toUnsignedUnsafe());
    }
    
    template <int ShiftExp>
    FixedPoint<NumBits - ShiftExp, Signed, Exp + ShiftExp> shiftBits () const
    {
        return FixedPoint<NumBits - ShiftExp, Signed, Exp + ShiftExp>::importBoundedBits(bitsBoundedValue().template shift<ShiftExp>());
    }
    
    template <int ShiftExp>
    FixedPoint<NumBits - ShiftExp, Signed, Exp + ShiftExp> undoShiftBitsLeft () const
    {
        static_assert(ShiftExp >= 0, "");
        
        return FixedPoint<NumBits - ShiftExp, Signed, Exp + ShiftExp>::importBoundedBits(bitsBoundedValue().template undoShiftLeft<ShiftExp>());
    }
    
    template <int NewBits>
    FixedPoint<NewBits, Signed, Exp - (NewBits - NumBits)> bitsTo () const
    {
        return FixedPoint<NewBits, Signed, Exp - (NewBits - NumBits)>::importBoundedBits(bitsBoundedValue().template shift<(NumBits - NewBits)>());
    }
    
    template <int MaxBits>
    FixedPoint<min(NumBits, MaxBits), Signed, Exp - (min(NumBits, MaxBits) - NumBits)> bitsDown () const
    {
        return FixedPoint<min(NumBits, MaxBits), Signed, Exp - (min(NumBits, MaxBits) - NumBits)>::importBoundedBits(bitsBoundedValue().template shift<(NumBits - min(NumBits, MaxBits))>());
    }
    
    template <int MinBits>
    FixedPoint<max(NumBits, MinBits), Signed, Exp - (max(NumBits, MinBits) - NumBits)> bitsUp () const
    {
        return FixedPoint<max(NumBits, MinBits), Signed, Exp - (max(NumBits, MinBits) - NumBits)>::importBoundedBits(bitsBoundedValue().template shift<(NumBits - max(NumBits, MinBits))>());
    }
    
    template <int ShiftExp>
    FixedPoint<NumBits, Signed, Exp + ShiftExp> shift () const
    {
        return FixedPoint<NumBits, Signed, Exp + ShiftExp>::importBoundedBits(bitsBoundedValue());
    }
    
    template <int NewBits>
    FixedPoint<NewBits, Signed, Exp> dropBitsUnsafe () const
    {
        AMBRO_ASSERT((m_bits.value() >= BoundedInt<NewBits, Signed>::minIntValue()))
        AMBRO_ASSERT((m_bits.value() <= BoundedInt<NewBits, Signed>::maxIntValue()))
        
        return FixedPoint<NewBits, Signed, Exp>::importBits(bitsValue());
    }
    
    template <int NewBits, bool NewSigned = Signed>
    FixedPoint<NewBits, NewSigned, Exp> dropBitsSaturated () const
    {
        FixedPoint<NewBits, NewSigned, Exp> res;
        if (*this < FixedPoint<NewBits, NewSigned, Exp>::minValue()) {
            res = FixedPoint<NewBits, NewSigned, Exp>::minValue();
        } else if (*this > FixedPoint<NewBits, NewSigned, Exp>::maxValue()) {
            res = FixedPoint<NewBits, NewSigned, Exp>::maxValue();
        } else {
            res = FixedPoint<NewBits, NewSigned, Exp>::importBits(m_bits.value());
        }
        return res;
    }
    
    template <int PowerExp>
    static FixedPoint powerOfTwo ()
    {
        static_assert(PowerExp - Exp >= 0, "");
        static_assert(PowerExp - Exp < NumBits, "");
        
        return FixedPoint::importBits(PowerOfTwo<IntType, PowerExp - Exp>::value);
    }
    
    FixedPoint<NumBits, false, Exp> absVal () const
    {
        return FixedPoint<NumBits, false, Exp>::importBoundedBits(m_bits.absVal());
    }
    
    template <int NumBits2, bool Signed2, int Exp2>
    operator FixedPoint<NumBits2, Signed2, Exp2> () const
    {
        static_assert(NumBits2 + Exp2 >= NumBits + Exp, "");
        static_assert(Exp2 <= Exp, "");
        static_assert(!Signed || Signed2, "");
        
        return FixedPoint<NumBits2, Signed2, Exp2>::importBoundedBits(m_bits.template shiftLeft<(Exp - Exp2)>());
    }
    
public:
    BoundedIntType m_bits;
};

template <int NumBits, bool Signed, int Exp>
FixedPoint<NumBits, true, Exp> operator- (FixedPoint<NumBits, Signed, Exp> op)
{
    return FixedPoint<NumBits, true, Exp>::importBoundedBits(-op.bitsBoundedValue());
}

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2, int RightShiftBits>
struct FixedPointMultiply {
    using ResultType = FixedPoint<(NumBits1 + NumBits2 - RightShiftBits), (Signed1 || Signed2), (Exp1 + Exp2 + RightShiftBits)>;
    
    AMBRO_ALWAYS_INLINE static ResultType call (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
    {
        return ResultType::importBoundedBits(BoundedMultiply<RightShiftBits>(op1.bitsBoundedValue(), op2.bitsBoundedValue()));
    }
};

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
AMBRO_ALWAYS_INLINE typename FixedPointMultiply<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, 0>::ResultType operator* (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointMultiply<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, 0>::call(op1, op2);
}

template <int RightShiftBits, int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
AMBRO_ALWAYS_INLINE typename FixedPointMultiply<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, RightShiftBits>::ResultType FixedMultiply (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointMultiply<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, RightShiftBits>::call(op1, op2);
}

template <int ResExp = 0, int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
AMBRO_ALWAYS_INLINE typename FixedPointMultiply<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, (ResExp - (Exp1 + Exp2))>::ResultType FixedResMultiply (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointMultiply<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, (ResExp - (Exp1 + Exp2))>::call(op1, op2);
}

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
struct FixedPointAdd {
    static const int shift_op1 = min(0, Exp2 - Exp1);
    static const int shift_op2 = min(0, Exp1 - Exp2);
    static_assert(Exp1 + shift_op1 == Exp2 + shift_op2, "");
    static const int numbits_shift_op1 = NumBits1 - shift_op1;
    static const int numbits_shift_op2 = NumBits2 - shift_op2;
    static const int numbits_result = max(numbits_shift_op1, numbits_shift_op2) + 1;
    static const int exp_result = Exp1 + shift_op1;
    
    using ResultType = FixedPoint<numbits_result, (Signed1 || Signed2), exp_result>;
    
    AMBRO_ALWAYS_INLINE static ResultType call (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
    {
        return ResultType::importBoundedBits(op1.bitsBoundedValue().template shift<shift_op1>() + op2.bitsBoundedValue().template shift<shift_op2>());
    }
};

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
AMBRO_ALWAYS_INLINE typename FixedPointAdd<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2>::ResultType operator+ (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointAdd<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2>::call(op1, op2);
}

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
AMBRO_ALWAYS_INLINE typename FixedPointAdd<NumBits1, Signed1, Exp1, NumBits2, true, Exp2>::ResultType operator- (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointAdd<NumBits1, Signed1, Exp1, NumBits2, true, Exp2>::call(op1, -op2);
}

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2, int LeftShiftBits, int ResSatBits, bool SupportZero>
struct FixedPointDivide {
    using ResultType = FixedPoint<ResSatBits, (Signed1 || Signed2), (Exp1 - Exp2 - LeftShiftBits)>;
    
    template <typename Option = int>
    __attribute__((always_inline)) inline static ResultType call (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2, Option opt = 0)
    {
        return ResultType::importBoundedBits(BoundedDivide<LeftShiftBits, ResSatBits, SupportZero>(op1.bitsBoundedValue(), op2.bitsBoundedValue(), opt));
    }
};

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
typename FixedPointDivide<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, 0, NumBits1, false>::ResultType operator/ (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointDivide<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, 0, NumBits1, false>::call(op1, op2);
}

template <bool SupportZero = true, int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
typename FixedPointDivide<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, NumBits2, NumBits1 + NumBits2, SupportZero>::ResultType FixedDivide (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointDivide<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, NumBits2, NumBits1 + NumBits2, SupportZero>::call(op1, op2);
}

template <bool SupportZero = true, int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2, typename Option = int>
__attribute__((always_inline)) inline typename FixedPointDivide<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, NumBits2, NumBits2 + Exp2 - Exp1, SupportZero>::ResultType FixedFracDivide (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2, Option opt = 0)
{
    return FixedPointDivide<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, NumBits2, NumBits2 + Exp2 - Exp1, SupportZero>::call(op1, op2, opt);
}

template <int ResExp, int ResSatBits, bool SupportZero, int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
typename FixedPointDivide<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, Exp1 - Exp2 - ResExp, ResSatBits, SupportZero>::ResultType FixedResDivide (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointDivide<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, Exp1 - Exp2 - ResExp, ResSatBits, SupportZero>::call(op1, op2);
}

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
struct FixedPointCompare {
    static const int shift_op1 = min(0, Exp2 - Exp1);
    static const int shift_op2 = min(0, Exp1 - Exp2);
    static_assert(Exp1 + shift_op1 == Exp2 + shift_op2, "");
    
    static bool eq (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
    {
        return (op1.bitsBoundedValue().template shift<shift_op1>() == op2.bitsBoundedValue().template shift<shift_op2>());
    }
    
    static bool ne (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
    {
        return (op1.bitsBoundedValue().template shift<shift_op1>() != op2.bitsBoundedValue().template shift<shift_op2>());
    }
    
    static bool lt (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
    {
        return (op1.bitsBoundedValue().template shift<shift_op1>() < op2.bitsBoundedValue().template shift<shift_op2>());
    }
    
    static bool gt (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
    {
        return (op1.bitsBoundedValue().template shift<shift_op1>() > op2.bitsBoundedValue().template shift<shift_op2>());
    }
    
    static bool le (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
    {
        return (op1.bitsBoundedValue().template shift<shift_op1>() <= op2.bitsBoundedValue().template shift<shift_op2>());
    }
    
    static bool ge (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
    {
        return (op1.bitsBoundedValue().template shift<shift_op1>() >= op2.bitsBoundedValue().template shift<shift_op2>());
    }
};

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
bool operator== (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointCompare<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2>::eq(op1, op2);
}

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
bool operator!= (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointCompare<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2>::ne(op1, op2);
}

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
bool operator< (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointCompare<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2>::lt(op1, op2);
}

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
bool operator> (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointCompare<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2>::gt(op1, op2);
}

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
bool operator<= (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointCompare<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2>::le(op1, op2);
}

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
bool operator>= (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointCompare<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2>::ge(op1, op2);
}

template <bool Round, int NumBits, bool Signed, int Exp, typename Option = int>
__attribute__((always_inline)) inline FixedPoint<((NumBits + Modulo(Exp, 2) + 1 + Round) / 2), false, ((Exp - Modulo(Exp, 2)) / 2)> FixedSquareRoot (FixedPoint<NumBits, Signed, Exp> op, Option opt = 0)
{
    return FixedPoint<((NumBits + Modulo(Exp, 2) + 1 + Round) / 2), false, ((Exp - Modulo(Exp, 2)) / 2)>::importBoundedBits(BoundedSquareRoot<Round>(op.bitsBoundedValue().template shiftLeft<Modulo(Exp, 2)>(), opt));
}

template <typename T1, typename T2>
struct FixedIntersectTypesHelper;

template <int NumBits1, bool Signed1, int Exp1>
struct FixedIntersectTypesHelper<FixedPoint<NumBits1, Signed1, Exp1>, FixedIdentity> {
    using Type = FixedPoint<NumBits1, Signed1, Exp1>;
};

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
struct FixedIntersectTypesHelper<FixedPoint<NumBits1, Signed1, Exp1>, FixedPoint<NumBits2, Signed2, Exp2>> {
    static const int exp_result = max(Exp1, Exp2);
    static const bool signed_result = (Signed1 && Signed2);
    static const int numbits_result = min(NumBits1 + Exp1, NumBits2 + Exp2) - exp_result;
    using Type = FixedPoint<numbits_result, signed_result, exp_result>;
};

template <typename T1, typename T2>
using FixedIntersectTypes = typename FixedIntersectTypesHelper<T1, T2>::Type;

template <typename T1, typename T2>
struct FixedUnionTypesHelper;

template <int NumBits1, bool Signed1, int Exp1>
struct FixedUnionTypesHelper<FixedPoint<NumBits1, Signed1, Exp1>, FixedIdentity> {
    using Type = FixedPoint<NumBits1, Signed1, Exp1>;
};

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
struct FixedUnionTypesHelper<FixedPoint<NumBits1, Signed1, Exp1>, FixedPoint<NumBits2, Signed2, Exp2>> {
    static const int exp_result = min(Exp1, Exp2);
    static const bool signed_result = (Signed1 || Signed2);
    static const int numbits_result = max(NumBits1 + Exp1, NumBits2 + Exp2) - exp_result;
    using Type = FixedPoint<numbits_result, signed_result, exp_result>;
};

template <typename T1, typename T2>
using FixedUnionTypes = typename FixedUnionTypesHelper<T1, T2>::Type;

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
struct FixedMaxHelper {
    static const int exp_result = min(Exp1, Exp2);
    static const bool signed_result = (Signed1 && Signed2);
    static const int numbits_result = max(NumBits1 + Exp1, NumBits2 + Exp2) - exp_result;
    static const int shift1 = Exp1 - exp_result;
    static const int shift2 = Exp2 - exp_result;
    
    using ResultType = FixedPoint<numbits_result, signed_result, exp_result>;
    
    static ResultType call (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
    {
        if (op1 > op2) {
            return ResultType::importBits(op1.bitsBoundedValue().template shiftLeft<shift1>().value());
        } else {
            return ResultType::importBits(op2.bitsBoundedValue().template shiftLeft<shift2>().value());
        }
    }
};

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
auto FixedMax (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2) -> typename FixedMaxHelper<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2>::ResultType
{
    return FixedMaxHelper<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2>::call(op1, op2);
}

template <int NumBits, bool Signed, int Exp>
auto FixedMax (FixedPoint<NumBits, Signed, Exp> op1, FixedIdentity op2) -> decltype(op1)
{
    return op1;
}

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
struct FixedMinHelper {
    static const int exp_result = min(Exp1, Exp2);
    static const bool signed_result = (Signed1 || Signed2);
    static const int numbits_result =
        (
            (!Signed1 && NumBits1 + Exp1 >= NumBits2 + Exp2) ||
            (!Signed2 && NumBits2 + Exp2 >= NumBits1 + Exp1)
        ) ?
        (min(NumBits1 + Exp1, NumBits2 + Exp2) - exp_result) :
        (max(NumBits1 + Exp1, NumBits2 + Exp2) - exp_result);
    static const int shift1 = Exp1 - exp_result;
    static const int shift2 = Exp2 - exp_result;
    
    using TempType1 = FixedPoint<numbits_result - shift1, Signed1, Exp1>;
    using TempType2 = FixedPoint<numbits_result - shift2, Signed2, Exp2>;
    using ResultType = FixedPoint<numbits_result, signed_result, exp_result>;
    
    static ResultType call (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
    {
        if (op1 < op2) {
            return TempType1::importBits(op1.bitsValue());
        } else {
            return TempType2::importBits(op2.bitsValue());
        }
    }
};

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
auto FixedMin (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2) -> typename FixedMinHelper<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2>::ResultType
{
    return FixedMinHelper<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2>::call(op1, op2);
}

template <int NumBits, bool Signed, int Exp>
auto FixedMin (FixedPoint<NumBits, Signed, Exp> op1, FixedIdentity op2) -> decltype(op1)
{
    return op1;
}

#include <aprinter/EndNamespace.h>

#endif
