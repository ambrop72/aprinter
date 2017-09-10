/*
 * Copyright (c) 2016 Ambroz Bizjak
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

#ifndef AIPSTACK_BASIC_META_UTILS_H
#define AIPSTACK_BASIC_META_UTILS_H

namespace AIpStack {

// WrapType

template <typename TType>
struct WrapType {
    using Type = TType;
};

// WrapValue

template <typename TType, TType TValue>
struct WrapValue {
    typedef TType Type;
    static constexpr Type Value = TValue;
    static constexpr Type value () { return TValue; }
};

template <bool Value>
using WrapBool = WrapValue<bool, Value>;

template <int Value>
using WrapInt = WrapValue<int, Value>;

// GetReturnType

template <typename Func>
struct GetReturnTypeHelper;

template <typename Ret, typename... Args>
struct GetReturnTypeHelper<Ret(Args...)> {
    using Result = Ret;
};

template <typename Ret, typename... Args>
struct GetReturnTypeHelper<Ret(Args...)const> {
    using Result = Ret;
};

template <typename Func>
using GetReturnType = typename GetReturnTypeHelper<Func>::Result;

}

#endif
