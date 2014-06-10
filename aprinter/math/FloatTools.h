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

#ifndef AMBROLIB_FLOAT_TOOLS_H
#define AMBROLIB_FLOAT_TOOLS_H

#include <math.h>
#include <float.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/meta/If.h>
#include <aprinter/meta/PowerOfTwo.h>
#include <aprinter/meta/IntTypeInfo.h>
#include <aprinter/math/PrintInt.h>

#include <aprinter/BeginNamespace.h>

template <typename T>
struct IsFpType {
    static bool const Value = false;
};

template <>
struct IsFpType<float> {
    static bool const Value = true;
};

template <>
struct IsFpType<double> {
    static bool const Value = true;
};

template <typename T>
struct IsFloat {
    static bool const Value = TypesAreEqual<T, float>::Value;
};

template <typename T>
bool FloatIsPosOrPosZero (T x)
{
    static_assert(IsFpType<T>::Value, "");
    
    return (signbit(x) == 0 && !isnan(x));
}

template <typename T>
T FloatMakePosOrPosZero (T x)
{
    static_assert(IsFpType<T>::Value, "");
    
    if (!(x > 0.0f)) {
        x = 0.0f;
    }
    return x;
}

template <typename T>
bool FloatIsNan (T x)
{
    static_assert(IsFpType<T>::Value, "");
    
    return isnan(x);
}

static void FloatToStrSoft (double x, char *s, int prec_approx = 6, bool pretty = true)
{
    if (isnan(x)) {
        strcpy(s, "nan");
        return;
    }
    if (signbit(x)) {
        *s++ = '-';
        x = -x;
    }
    if (x == INFINITY) {
        strcpy(s, "inf");
        return;
    }
    int e;
    double m = frexp(x, &e);
    double f = 0.3010299956639811952137388947244930267681898814621085 * e;
    double fi;
    double ff = modf(f, &fi);
    if (f < 0.0) {
        fi -= 1.0;
        ff += 1.0;
    }
    int ep = 1 - prec_approx;
    double n = m * pow(10.0, ff - ep);
    if (n < pow(10.0, prec_approx - 1)) {
        ep--;
        n = m * pow(10.0, ff - ep);
    }
    uint64_t n_int = n;
    int n_len = PrintNonnegativeIntDecimal<uint64_t>(n_int, s);
    s += n_len;
    int e10 = fi + ep;
    if (pretty) {
        char *dot = s;
        while (e10 != 0 && n_len > 1) {
            *dot = *(dot - 1);
            dot--;
            e10++;
            n_len--;
        }
        if (dot != s) {
            *dot = '.';
            s++;
        }
    }
    if (!pretty || (e10 != 0 && n_int != 0)) {
        *s++ = 'e';
        if (e10 < 0) {
            e10 = -e10;
            *s++ = '-';
        }
        s += PrintNonnegativeIntDecimal<int>(e10, s);
    }
    *s = '\0';
}

template <typename T>
bool FloatSignBit (T x)
{
    static_assert(IsFpType<T>::Value, "");
    
#ifdef AMBROLIB_AVR
    static_assert(sizeof(x) == 4, "");
    union { float f; uint8_t b[4]; } u;
    u.f = x;
    return (u.b[3] & 0x80);
#else
    return signbit(x);
#endif
}

template <typename T>
T FloatSqrt (T x)
{
    static_assert(IsFpType<T>::Value, "");
    
    return IsFloat<T>::Value ? sqrtf(x) : sqrt(x);
}

template <typename T>
T StrToFloat (char const *nptr, char **endptr)
{
    static_assert(IsFpType<T>::Value, "");
    
#ifdef AMBROLIB_AVR
    return strtod(nptr, endptr);
#else
    return IsFloat<T>::Value ? strtof(nptr, endptr) : strtod(nptr, endptr);
#endif
}

template <typename T>
T FloatLdexp (T x, int exp)
{
    static_assert(IsFpType<T>::Value, "");
    
    return IsFloat<T>::Value ? ldexpf(x, exp) : ldexp(x, exp);
}

template <typename T>
T FloatRound (T x)
{
    static_assert(IsFpType<T>::Value, "");
    
    return IsFloat<T>::Value ? roundf(x) : round(x);
}

template <typename T>
T FloatCeil (T x)
{
    static_assert(IsFpType<T>::Value, "");
    
    return IsFloat<T>::Value ? ceilf(x) : ceil(x);
}

template <typename T>
T FloatAbs (T x)
{
    static_assert(IsFpType<T>::Value, "");
    
    return IsFloat<T>::Value ? fabsf(x) : fabs(x);
}

template <typename T>
T FloatLog (T x)
{
    static_assert(IsFpType<T>::Value, "");
    
    return IsFloat<T>::Value ? logf(x) : log(x);
}

template <typename T>
T FloatExp (T x)
{
    static_assert(IsFpType<T>::Value, "");
    
    return IsFloat<T>::Value ? expf(x) : exp(x);
}

template <typename T1, typename T2>
using FloatPromote = If<(IsFloat<T1>::Value && IsFloat<T2>::Value), float, double>;

template <typename T1, typename T2>
FloatPromote<T1, T2> FloatMin (T1 x, T2 y)
{
    static_assert(IsFpType<T1>::Value, "");
    static_assert(IsFpType<T2>::Value, "");
    
    return IsFloat<FloatPromote<T1, T2>>::Value ? fminf(x, y) : fmin(x, y);
}

template <typename T1, typename T2>
FloatPromote<T1, T2> FloatMax (T1 x, T2 y)
{
    static_assert(IsFpType<T1>::Value, "");
    static_assert(IsFpType<T2>::Value, "");
    
    return IsFloat<FloatPromote<T1, T2>>::Value ? fmaxf(x, y) : fmax(x, y);
}

struct FloatIdentity {};

template <typename T2>
T2 FloatMin (FloatIdentity, T2 y)
{
    static_assert(IsFpType<T2>::Value, "");
    return y;
}

template <typename T2>
T2 FloatMax (FloatIdentity, T2 y)
{
    static_assert(IsFpType<T2>::Value, "");
    return y;
}

template <typename T>
T FloatPositiveIntegerRange ()
{
    static_assert(IsFpType<T>::Value, "");
    
    return FloatLdexp<T>(1.0f, (IsFloat<T>::Value ? FLT_MANT_DIG : DBL_MANT_DIG));
}

template <typename T>
T FloatSignedIntegerRange ()
{
    static_assert(IsFpType<T>::Value, "");
    
    return FloatLdexp<T>(1.0f, (IsFloat<T>::Value ? FLT_MANT_DIG : DBL_MANT_DIG) - 1);
}

#define APRINTER_DEFINE_INT_ROUND_HELPER_START \
template <typename FpType, typename IntType, typename Dummy2 = void> \
struct FloatIntRoundHelper;

#define APRINTER_DEFINE_INT_ROUND_HELPER(FpType, IntType, func) \
template <typename Dummy2> \
struct FloatIntRoundHelper<FpType, IntType, Dummy2> { \
    inline static IntType call (FpType x) \
    { \
        return func(x); \
    } \
};

template <int LongIntSize, typename Dummy = void>
struct FloatRoundImpl;

template <typename Dummy>
struct FloatRoundImpl<4, Dummy> {
    static_assert(TypesAreEqual<Dummy, void>::Value && TypesAreEqual<decltype(lroundf(0)), int32_t>::Value, "");
    static_assert(TypesAreEqual<Dummy, void>::Value && TypesAreEqual<decltype(lround(0)), int32_t>::Value, "");
#if !defined(AMBROLIB_AVR)
    static_assert(TypesAreEqual<Dummy, void>::Value && TypesAreEqual<decltype(llroundf(0)), int64_t>::Value, "");
    static_assert(TypesAreEqual<Dummy, void>::Value && TypesAreEqual<decltype(llround(0)), int64_t>::Value, "");
#endif
    
    APRINTER_DEFINE_INT_ROUND_HELPER_START
    APRINTER_DEFINE_INT_ROUND_HELPER(float, int8_t, lroundf)
    APRINTER_DEFINE_INT_ROUND_HELPER(float, int16_t, lroundf)
    APRINTER_DEFINE_INT_ROUND_HELPER(float, int32_t, lroundf)
    APRINTER_DEFINE_INT_ROUND_HELPER(double, int8_t, lround)
    APRINTER_DEFINE_INT_ROUND_HELPER(double, int16_t, lround)
    APRINTER_DEFINE_INT_ROUND_HELPER(double, int32_t, lround)
#if !defined(AMBROLIB_AVR)
    APRINTER_DEFINE_INT_ROUND_HELPER(float, int64_t, llroundf)
    APRINTER_DEFINE_INT_ROUND_HELPER(double, int64_t, llround)
#endif
};

template <typename Dummy>
struct FloatRoundImpl<8, Dummy> {
    static_assert(TypesAreEqual<Dummy, void>::Value && TypesAreEqual<decltype(lroundf(0)), int64_t>::Value, "");
    static_assert(TypesAreEqual<Dummy, void>::Value && TypesAreEqual<decltype(lround(0)), int64_t>::Value, "");
    
    APRINTER_DEFINE_INT_ROUND_HELPER_START
    APRINTER_DEFINE_INT_ROUND_HELPER(float, int8_t, lroundf)
    APRINTER_DEFINE_INT_ROUND_HELPER(float, int16_t, lroundf)
    APRINTER_DEFINE_INT_ROUND_HELPER(float, int32_t, lroundf)
    APRINTER_DEFINE_INT_ROUND_HELPER(float, int64_t, lroundf)
    APRINTER_DEFINE_INT_ROUND_HELPER(double, int8_t, lround)
    APRINTER_DEFINE_INT_ROUND_HELPER(double, int16_t, lround)
    APRINTER_DEFINE_INT_ROUND_HELPER(double, int32_t, lround)
    APRINTER_DEFINE_INT_ROUND_HELPER(double, int64_t, lround)
};

template <typename IntType, typename FpType>
IntType FloatIntRound (FpType x)
{
    static_assert(IsFpType<FpType>::Value, "");
    static_assert(IntTypeInfo<IntType>::Signed, "");
    
    return FloatRoundImpl<sizeof(long int)>::template FloatIntRoundHelper<FpType, IntType>::call(x);
}

template <typename FpType, typename IntType, int Bits>
class FloatIntRoundLimit {
    static constexpr IntType MaxInt = PowerOfTwoMinusOne<IntType, Bits>::Value;
    static constexpr FpType FpPower = __builtin_ldexp(1.0, Bits);
    
    static constexpr FpType helper (IntType x)
    {
        return ((FpType)x != FpPower) ? (FpType)x : helper(x - 1);
    }
    
public:
    static constexpr FpType Value = helper(MaxInt);
};

#include <aprinter/EndNamespace.h>

#endif
