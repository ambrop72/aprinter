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
#include <aprinter/math/PrintInt.h>

#include <aprinter/BeginNamespace.h>

template <typename T>
struct IsFpType {
    static bool const value = false;
};

template <>
struct IsFpType<float> {
    static bool const value = true;
};

template <>
struct IsFpType<double> {
    static bool const value = true;
};

template <typename T>
struct IsFloat {
    static bool const value = TypesAreEqual<T, float>::value;
};

template <typename T>
bool FloatIsPosOrPosZero (T x)
{
    static_assert(IsFpType<T>::value, "");
    
    return (signbit(x) == 0 && !isnan(x));
}

template <typename T>
T FloatMakePosOrPosZero (T x)
{
    static_assert(IsFpType<T>::value, "");
    
    if (!(x > 0.0f)) {
        x = 0.0f;
    }
    return x;
}

template <typename T>
bool FloatIsNan (T x)
{
    static_assert(IsFpType<T>::value, "");
    
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
    static_assert(IsFpType<T>::value, "");
    
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
    static_assert(IsFpType<T>::value, "");
    
    return IsFloat<T>::value ? sqrtf(x) : sqrt(x);
}

template <typename T>
T StrToFloat (char const *nptr, char **endptr)
{
    static_assert(IsFpType<T>::value, "");
    
#ifdef AMBROLIB_AVR
    return strtod(nptr, endptr);
#else
    return IsFloat<T>::value ? strtof(nptr, endptr) : strtod(nptr, endptr);
#endif
}

template <typename T>
T FloatLdexp (T x, int exp)
{
    static_assert(IsFpType<T>::value, "");
    
    return IsFloat<T>::value ? ldexpf(x, exp) : ldexp(x, exp);
}

template <typename T>
T FloatRound (T x)
{
    static_assert(IsFpType<T>::value, "");
    
    return IsFloat<T>::value ? roundf(x) : round(x);
}

template <typename T>
T FloatCeil (T x)
{
    static_assert(IsFpType<T>::value, "");
    
    return IsFloat<T>::value ? ceilf(x) : ceil(x);
}

template <typename T>
T FloatAbs (T x)
{
    static_assert(IsFpType<T>::value, "");
    
    return IsFloat<T>::value ? fabsf(x) : fabs(x);
}

template <typename T>
T FloatLog (T x)
{
    static_assert(IsFpType<T>::value, "");
    
    return IsFloat<T>::value ? logf(x) : log(x);
}

template <typename T>
T FloatExp (T x)
{
    static_assert(IsFpType<T>::value, "");
    
    return IsFloat<T>::value ? expf(x) : exp(x);
}

template <typename T1, typename T2>
using FloatPromote = If<(IsFloat<T1>::value && IsFloat<T2>::value), float, double>;

template <typename T1, typename T2>
FloatPromote<T1, T2> FloatMin (T1 x, T2 y)
{
    static_assert(IsFpType<T1>::value, "");
    static_assert(IsFpType<T2>::value, "");
    
    return IsFloat<FloatPromote<T1, T2>>::value ? fminf(x, y) : fmin(x, y);
}

template <typename T1, typename T2>
FloatPromote<T1, T2> FloatMax (T1 x, T2 y)
{
    static_assert(IsFpType<T1>::value, "");
    static_assert(IsFpType<T2>::value, "");
    
    return IsFloat<FloatPromote<T1, T2>>::value ? fmaxf(x, y) : fmax(x, y);
}

struct FloatIdentity {};

template <typename T2>
T2 FloatMin (FloatIdentity, T2 y)
{
    static_assert(IsFpType<T2>::value, "");
    return y;
}

template <typename T2>
T2 FloatMax (FloatIdentity, T2 y)
{
    static_assert(IsFpType<T2>::value, "");
    return y;
}

template <typename T>
T FloatPositiveIntegerRange ()
{
    static_assert(IsFpType<T>::value, "");
    
    return FloatLdexp<T>(1.0f, (IsFloat<T>::value ? FLT_MANT_DIG : DBL_MANT_DIG));
}

template <typename T>
T FloatSignedIntegerRange ()
{
    static_assert(IsFpType<T>::value, "");
    
    return FloatLdexp<T>(1.0f, (IsFloat<T>::value ? FLT_MANT_DIG : DBL_MANT_DIG) - 1);
}

#include <aprinter/EndNamespace.h>

#endif
