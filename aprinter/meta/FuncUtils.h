/*
 * Copyright (c) 2015 Ambroz Bizjak
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

#ifndef APRINTER_FUNC_UTILS_H
#define APRINTER_FUNC_UTILS_H

#include <aprinter/meta/BasicMetaUtils.h>

namespace APrinter {

template <typename Func, typename Arg>
using FuncCall = typename Func::template Call<Arg>::Type;

template <typename CondFunc, typename TrueFunc, typename FalseFunc>
struct IfFunc {
    template <typename X>
    struct Call {
        using Type = FuncCall<If<FuncCall<CondFunc, X>::Value, TrueFunc, FalseFunc>, X>;
    };
};

template <typename T>
struct IsEqualFunc {
    template <typename U>
    struct Call {
        typedef WrapBool<TypesAreEqual<U, T>::Value> Type;
    };
};

struct NotFunc {
    template <typename X>
    struct Call {
        typedef WrapBool<(!X::Value)> Type;
    };
};

template <template<typename> class Template>
struct TemplateFunc {
    template <typename U>
    struct Call {
        typedef Template<U> Type;
    };
};

template <typename ValueType, template<ValueType> class Template>
struct ValueTemplateFunc {
    template <typename U>
    struct Call {
        typedef Template<U::Value> Type;
    };
};

template <typename Value>
struct ConstantFunc {
    template <typename X>
    struct Call {
        using Type = Value;
    };
};

template <typename Func1, typename Func2>
struct ComposeFunctions {
    template <typename X>
    struct Call {
        using Type = FuncCall<Func1, FuncCall<Func2, X>>;
    };
};

}

#endif
