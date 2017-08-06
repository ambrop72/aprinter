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

#ifndef AMBROLIB_MIN_MAX_H
#define AMBROLIB_MIN_MAX_H

#include <aprinter/meta/IntTypeInfo.h>
#include <aprinter/meta/BasicMetaUtils.h>

namespace APrinter {

template <typename T>
constexpr T MinValue (T op1, T op2)
{
    return (op1 < op2) ? op1 : op2;
}

template <typename T>
constexpr T MaxValue (T op1, T op2)
{
    return (op1 > op2) ? op1 : op2;
}

template <typename T>
constexpr T AbsoluteValue (T op)
{
    return (op > 0) ? op : -op;
}

template <typename T>
constexpr T AbsoluteDiff (T op1, T op2)
{
    return (op1 > op2) ? (op1 - op2) : (op2 - op1);
}

template <typename T1, typename T2>
using MinValueURetType = If<(IntTypeInfo<T1>::NumBits < IntTypeInfo<T2>::NumBits), T1, T2>;

template <typename T1, typename T2>
constexpr MinValueURetType<T1, T2> MinValueU (T1 op1, T2 op2)
{
    static_assert(!IntTypeInfo<T1>::Signed, "Only unsigned allowed");
    static_assert(!IntTypeInfo<T2>::Signed, "Only unsigned allowed");
    
    if (op1 < op2) {
        return op1;
    } else {
        return op2;
    }
}

}

#endif
