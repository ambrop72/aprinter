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

#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/math/PrintInt.h>

#include <aprinter/BeginNamespace.h>

template <typename T>
bool FloatIsPosOrPosZero (T x)
{
    return (signbit(x) == 0 && !isnan(x));
}

template <typename T>
T FloatMakePosOrPosZero (T x)
{
    if (!(x > 0.0)) {
        x = 0.0;
    }
    return x;
}

template <typename T>
T FloatPositiveIntegerRange ()
{
    static_assert(TypesAreEqual<T, float>::value || TypesAreEqual<T, double>::value, "");
    
    return ldexp(1.0, (TypesAreEqual<T, float>::value ? FLT_MANT_DIG : DBL_MANT_DIG));
}

template <typename T>
T FloatSignedIntegerRange ()
{
    static_assert(TypesAreEqual<T, float>::value || TypesAreEqual<T, double>::value, "");
    
    return ldexp(1.0, (TypesAreEqual<T, float>::value ? FLT_MANT_DIG : DBL_MANT_DIG) - 1);
}

static void FloatToStrSoft (double x, char *s, int prec_approx)
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
        n *= 10;
        ep--;
    }
    s += PrintNonnegativeIntDecimal<uint64_t>(n, s);
    *s++ = 'e';
    int e10 = fi + ep;
    if (e10 < 0) {
        e10 = -e10;
        *s++ = '-';
    }
    s += PrintNonnegativeIntDecimal<int>(e10, s);
    *s = '\0';
}

#include <aprinter/EndNamespace.h>

#endif
