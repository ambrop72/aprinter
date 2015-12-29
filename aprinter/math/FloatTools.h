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
#include <aprinter/base/Inline.h>
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
AMBRO_ALWAYS_INLINE
bool FloatIsNan (T x)
{
    static_assert(IsFpType<T>::Value, "");
    
#ifdef AMBROLIB_AVR
    static_assert(sizeof(x) == 4, "");
    union { float f; uint8_t b[4]; } u;
    u.f = x;
    return (u.b[3] & 0x7F) == 0x7F && (u.b[2] & 0x80) == 0x80 && ((u.b[2] & 0x7F) | u.b[1] | u.b[0]) != 0;
#else
    return isnan(x);
#endif
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

double FloatSqrt (double x)
{
    return sqrt(x);
}

float FloatSqrt (float x)
{
    return sqrtf(x);
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

double FloatLdexp (double x, int exp)
{
    return ldexp(x, exp);
}

float FloatLdexp (float x, int exp)
{
    return ldexpf(x, exp);
}

double FloatRound (double x)
{
    return round(x);
}

float FloatRound (float x)
{
    return roundf(x);
}

double FloatCeil (double x)
{
    return ceil(x);
}

float FloatCeil (float x)
{
    return ceilf(x);
}

double FloatAbs (double x)
{
#ifdef APRINTER_BROKEN_FABS
    return signbit(x) ? -x : x;
#else
    return fabs(x);
#endif
}

float FloatAbs (float x)
{
    return fabsf(x);
}

double FloatLog (double x)
{
    return log(x);
}

float FloatLog (float x)
{
    return logf(x);
}

double FloatExp (double x)
{
    return exp(x);
}

float FloatExp (float x)
{
    return expf(x);
}

double FloatSin (double x)
{
    return sin(x);
}

float FloatSin (float x)
{
    return sinf(x);
}

double FloatCos (double x)
{
    return cos(x);
}

float FloatCos (float x)
{
    return cosf(x);
}

double FloatAcos (double x)
{
    return acos(x);
}

float FloatAcos (float x)
{
    return acosf(x);
}

double FloatAtan2 (double y, double x)
{
    return atan2(y, x);
}

float FloatAtan2 (float y, float x)
{
    return atan2f(y, x);
}

double FloatMin (double x, double y)
{
    return fmin(x, y);
}

float FloatMin (float x, float y)
{
    return fminf(x, y);
}

double FloatMax (double x, double y)
{
    return fmax(x, y);
}

float FloatMax (float x, float y)
{
    return fmaxf(x, y);
}

double FloatSquare (double x)
{
    return x * x;
}

float FloatSquare (float x)
{
    return x * x;
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
    
    return FloatLdexp(T(1.0f), (IsFloat<T>::Value ? FLT_MANT_DIG : DBL_MANT_DIG));
}

template <typename T>
T FloatSignedIntegerRange ()
{
    static_assert(IsFpType<T>::Value, "");
    
    return FloatLdexp(T(1.0f), (IsFloat<T>::Value ? FLT_MANT_DIG : DBL_MANT_DIG) - 1);
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
    static_assert(TypesAreEqual<Dummy, void>::Value && sizeof(decltype(lroundf(0))) == 4, "");
    static_assert(TypesAreEqual<Dummy, void>::Value && sizeof(decltype(lround(0))) == 4, "");
#if !defined(AMBROLIB_AVR)
    static_assert(TypesAreEqual<Dummy, void>::Value && sizeof(decltype(llroundf(0))) == 8, "");
    static_assert(TypesAreEqual<Dummy, void>::Value && sizeof(decltype(llround(0))) == 8, "");
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
    static_assert(TypesAreEqual<Dummy, void>::Value && sizeof(decltype(lroundf(0))) == 8, "");
    static_assert(TypesAreEqual<Dummy, void>::Value && sizeof(decltype(lround(0))) == 8, "");
    
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
