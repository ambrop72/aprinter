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

#include <aprinter/BeginNamespace.h>

template <int NumBits, bool Signed, int Exp>
class FixedPoint {
public:
    static const int num_bits = NumBits;
    static const bool is_signed = Signed;
    static const int exp = Exp;
    using BoundedIntType = BoundedInt<NumBits, Signed>;
    using IntType = typename BoundedIntType::IntType;
    
    inline static FixedPoint importBoundedBits (BoundedIntType op)
    {
        FixedPoint res;
        res.m_bits = op;
        return res;
    }
    
    inline static FixedPoint importBits (IntType op)
    {
        return importBoundedBits(BoundedIntType::import(op));
    }
    
    inline BoundedIntType bitsBoundedValue () const
    {
        return m_bits;
    }
    
    inline IntType bitsValue () const
    {
        return m_bits.value();
    }
    
    inline static FixedPoint importDouble (double op)
    {
        AMBRO_ASSERT(ldexp(op, -Exp) >= BoundedIntType::minIntValue())
        AMBRO_ASSERT(ldexp(op, -Exp) <= BoundedIntType::maxIntValue())
        
        return importBits(BoundedIntType::import(ldexp(op, -Exp)));
    }
    
    inline double doubleValue () const
    {
        return ldexp(bitsValue(), Exp);
    }
    
    inline FixedPoint<NumBits, true, Exp> toSigned () const
    {
        return FixedPoint<NumBits, true, Exp>::importBoundedBits(m_bits.toSigned());
    }
    
    inline FixedPoint<NumBits, false, Exp> toUnsignedUnsafe () const
    {
        return FixedPoint<NumBits, false, Exp>::importBoundedBits(m_bits.toUnsignedUnsafe());
    }
    
    template <int ShiftExp>
    inline FixedPoint<NumBits - ShiftExp, Signed, Exp + ShiftExp> shiftBits () const
    {
        return FixedPoint<NumBits - ShiftExp, Signed, Exp + ShiftExp>::importBoundedBits(bitsBoundedValue().template shift<ShiftExp>());
    }
    
    template <int NewBits>
    inline FixedPoint<NewBits, Signed, Exp - (NewBits - NumBits)> bitsTo () const
    {
        return FixedPoint<NewBits, Signed, Exp - (NewBits - NumBits)>::importBoundedBits(bitsBoundedValue().template shift<(NumBits - NewBits)>());
    }
    
    template <int MaxBits>
    inline FixedPoint<min(NumBits, MaxBits), Signed, Exp - (min(NumBits, MaxBits) - NumBits)> bitsDown () const
    {
        return FixedPoint<min(NumBits, MaxBits), Signed, Exp - (min(NumBits, MaxBits) - NumBits)>::importBoundedBits(bitsBoundedValue().template shift<(NumBits - min(NumBits, MaxBits))>());
    }
    
    template <int MinBits>
    inline FixedPoint<max(NumBits, MinBits), Signed, Exp - (max(NumBits, MinBits) - NumBits)> bitsUp () const
    {
        return FixedPoint<max(NumBits, MinBits), Signed, Exp - (max(NumBits, MinBits) - NumBits)>::importBoundedBits(bitsBoundedValue().template shift<(NumBits - max(NumBits, MinBits))>());
    }
    
    template <int ShiftExp>
    inline FixedPoint<NumBits, Signed, Exp + ShiftExp> shift () const
    {
        return FixedPoint<NumBits, Signed, Exp + ShiftExp>::importBoundedBits(bitsBoundedValue());
    }
    
    template <int NewBits>
    inline FixedPoint<NewBits, Signed, Exp> dropBitsUnsafe () const
    {
        AMBRO_ASSERT((m_bits.value() >= BoundedInt<NewBits, Signed>::minIntValue()))
        AMBRO_ASSERT((m_bits.value() <= BoundedInt<NewBits, Signed>::maxIntValue()))
        
        return FixedPoint<NewBits, Signed, Exp>::importBits(bitsValue());
    }
    
    template <int NewBits>
    inline FixedPoint<NewBits, Signed, Exp> dropBitsSaturated () const
    {
        IntType bits_value = m_bits.value();
        if (bits_value < BoundedInt<NewBits, Signed>::minIntValue()) {
            bits_value = BoundedInt<NewBits, Signed>::minIntValue();
        }
        else if (bits_value > BoundedInt<NewBits, Signed>::maxIntValue()) {
            bits_value = BoundedInt<NewBits, Signed>::maxIntValue();
        }
        
        return FixedPoint<NewBits, Signed, Exp>::importBits(bits_value);
    }
    
    template <typename Dummy = void>
    inline static FixedPoint one (Dummy)
    {
        static_assert(Exp <= 0, "");
        static_assert(NumBits + Exp > 0, "");
        
        return FixedPoint::importBits(PowerOfTwo<IntType, (-Exp)>::value);
    }
    
public:
    BoundedIntType m_bits;
};

template <int NumBits, bool Signed, int Exp>
inline FixedPoint<NumBits, true, Exp> operator- (FixedPoint<NumBits, Signed, Exp> op)
{
    return FixedPoint<NumBits, true, Exp>::importBoundedBits(-op.bitsBoundedValue());
}

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2, int RightShiftBits>
struct FixedPointMultiply {
    using ResultType = FixedPoint<(NumBits1 + NumBits2 - RightShiftBits), (Signed1 || Signed2), (Exp1 + Exp2 + RightShiftBits)>;
    
    static ResultType call (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
    {
        return ResultType::importBoundedBits(BoundedMultiply<RightShiftBits>(op1.bitsBoundedValue(), op2.bitsBoundedValue()));
    }
};

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
inline typename FixedPointMultiply<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, 0>::ResultType operator* (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointMultiply<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, 0>::call(op1, op2);
}

template <int RightShiftBits, int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
inline typename FixedPointMultiply<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, RightShiftBits>::ResultType FixedMultiply (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointMultiply<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, RightShiftBits>::call(op1, op2);
}

template <int NumBits1, int Exp1, int NumBits2, int Exp2, bool Signed>
struct FixedPointAdd {
    static const int shift_op1 = min(0, Exp2 - Exp1);
    static const int shift_op2 = min(0, Exp1 - Exp2);
    static_assert(Exp1 + shift_op1 == Exp2 + shift_op2, "");
    static const int numbits_shift_op1 = NumBits1 - shift_op1;
    static const int numbits_shift_op2 = NumBits2 - shift_op2;
    static const int numbits_result = max(numbits_shift_op1, numbits_shift_op2) + 1;
    static const int exp_result = Exp1 + shift_op1;
    
    using ResultType = FixedPoint<numbits_result, Signed, exp_result>;
    
    inline static ResultType call (FixedPoint<NumBits1, Signed, Exp1> op1, FixedPoint<NumBits2, Signed, Exp2> op2)
    {
        return ResultType::importBoundedBits(op1.bitsBoundedValue().template shift<shift_op1>() + op2.bitsBoundedValue().template shift<shift_op2>());
    }
};

template <int NumBits1, int Exp1, int NumBits2, int Exp2, bool Signed>
inline typename FixedPointAdd<NumBits1, Exp1, NumBits2, Exp2, Signed>::ResultType operator+ (FixedPoint<NumBits1, Signed, Exp1> op1, FixedPoint<NumBits2, Signed, Exp2> op2)
{
    return FixedPointAdd<NumBits1, Exp1, NumBits2, Exp2, Signed>::call(op1, op2);
}

template <int NumBits1, int Exp1, int NumBits2, int Exp2, bool Signed>
inline typename FixedPointAdd<NumBits1, Exp1, NumBits2, Exp2, Signed>::ResultType operator- (FixedPoint<NumBits1, Signed, Exp1> op1, FixedPoint<NumBits2, Signed, Exp2> op2)
{
    return FixedPointAdd<NumBits1, Exp1, NumBits2, Exp2, Signed>::call(op1, -op2);
}

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2, int LeftShiftBits, int ResSatBits>
struct FixedPointDivide {
    using ResultType = FixedPoint<ResSatBits, (Signed1 || Signed2), (Exp1 - Exp2 - LeftShiftBits)>;
    
    inline static ResultType call (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
    {
        return ResultType::importBoundedBits(BoundedDivide<LeftShiftBits, ResSatBits>(op1.bitsBoundedValue(), op2.bitsBoundedValue()));
    }
};

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
inline typename FixedPointDivide<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, 0, NumBits1>::ResultType operator/ (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointDivide<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, 0, NumBits1>::call(op1, op2);
}

template <int LeftShiftBits, int ResSatBits, int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
inline typename FixedPointDivide<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, LeftShiftBits, ResSatBits>::ResultType FixedDivide (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointDivide<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2, LeftShiftBits, ResSatBits>::call(op1, op2);
}

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
struct FixedPointCompare {
    static const int shift_op1 = min(0, Exp2 - Exp1);
    static const int shift_op2 = min(0, Exp1 - Exp2);
    static_assert(Exp1 + shift_op1 == Exp2 + shift_op2, "");
    
    inline static bool eq (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
    {
        return (op1.bitsBoundedValue().template shift<shift_op1>() == op2.bitsBoundedValue().template shift<shift_op2>());
    }
    
    inline static bool ne (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
    {
        return (op1.bitsBoundedValue().template shift<shift_op1>() != op2.bitsBoundedValue().template shift<shift_op2>());
    }
    
    inline static bool lt (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
    {
        return (op1.bitsBoundedValue().template shift<shift_op1>() < op2.bitsBoundedValue().template shift<shift_op2>());
    }
    
    inline static bool gt (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
    {
        return (op1.bitsBoundedValue().template shift<shift_op1>() > op2.bitsBoundedValue().template shift<shift_op2>());
    }
    
    inline static bool le (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
    {
        return (op1.bitsBoundedValue().template shift<shift_op1>() <= op2.bitsBoundedValue().template shift<shift_op2>());
    }
    
    inline static bool ge (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
    {
        return (op1.bitsBoundedValue().template shift<shift_op1>() >= op2.bitsBoundedValue().template shift<shift_op2>());
    }
};

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
inline bool operator== (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointCompare<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2>::eq(op1, op2);
}

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
inline bool operator!= (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointCompare<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2>::ne(op1, op2);
}

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
inline bool operator< (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointCompare<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2>::lt(op1, op2);
}

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
inline bool operator> (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointCompare<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2>::gt(op1, op2);
}

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
inline bool operator<= (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointCompare<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2>::le(op1, op2);
}

template <int NumBits1, bool Signed1, int Exp1, int NumBits2, bool Signed2, int Exp2>
inline bool operator>= (FixedPoint<NumBits1, Signed1, Exp1> op1, FixedPoint<NumBits2, Signed2, Exp2> op2)
{
    return FixedPointCompare<NumBits1, Signed1, Exp1, NumBits2, Signed2, Exp2>::ge(op1, op2);
}

template <int NumBits, bool Signed, int Exp>
inline FixedPoint<((NumBits + Modulo(Exp, 2) + 1) / 2), false, ((Exp - Modulo(Exp, 2)) / 2)> FixedSquareRoot (FixedPoint<NumBits, Signed, Exp> op)
{
    return FixedPoint<((NumBits + Modulo(Exp, 2) + 1) / 2), false, ((Exp - Modulo(Exp, 2)) / 2)>::importBoundedBits(BoundedSquareRoot(op.bitsBoundedValue().template shiftLeft<Modulo(Exp, 2)>()));
}

#include <aprinter/EndNamespace.h>

#endif
