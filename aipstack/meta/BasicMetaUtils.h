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

// DedummyIndexTemplate

template <template<int, typename> class Template>
struct DedummyIndexTemplate {
    template <int Index>
    using Result = Template<Index, void>;
};

// EnableIf

template <bool Cond, typename T>
struct EnableIfHelper {};

template <typename T>
struct EnableIfHelper<true, T> {
    using Type = T;
};

template <bool Cond, typename T>
using EnableIf = typename EnableIfHelper<Cond, T>::Type;

// If

template <bool Cond, typename T1, typename T2>
struct IfHelper;

template <typename T1, typename T2>
struct IfHelper<true, T1, T2> {
    typedef T1 Type;
};

template <typename T1, typename T2>
struct IfHelper<false, T1, T2> {
    typedef T2 Type;
};

template <bool Cond, typename T1, typename T2>
using If = typename IfHelper<Cond, T1, T2>::Type;

// RemoveConst

template <typename T>
struct RemoveConstHelper {
    using Type = T;
};

template <typename T>
struct RemoveConstHelper<T const> {
    using Type = T;
};

template <typename T>
using RemoveConst = typename RemoveConstHelper<T>::Type;

// InheritConst

template <typename InheritFromType, typename TargetType>
struct InheritConstHelper {
    using Result = RemoveConst<TargetType>;
};

template <typename InheritFromType, typename TargetType>
struct InheritConstHelper<InheritFromType const, TargetType> {
    using Result = TargetType const;
};

template <typename InheritFromType, typename TargetType>
using InheritConst = typename InheritConstHelper<InheritFromType, TargetType>::Result;

// RemoveReference

template <typename T>
struct RemoveReferenceHelper {
    using Type = T;
};

template <typename T>
struct RemoveReferenceHelper<T &> {
    using Type = T;
};

template <typename T>
struct RemoveReferenceHelper<T &&> {
    using Type = T;
};

template <typename T>
using RemoveReference = typename RemoveReferenceHelper<T>::Type;

// TypesAreEqual

template <typename T1, typename T2>
struct TypesAreEqual {
    static const bool Value = false;
};

template <typename T>
struct TypesAreEqual<T, T> {
    static const bool Value = true;
};

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

#define AIPSTACK_WRAP_COMPLEX_VALUE(Type, Value) struct { static constexpr Type value () { return (Value); } }

#define AIPSTACK_WRAP_DOUBLE(Value) AIPSTACK_WRAP_COMPLEX_VALUE(double, (Value))

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
