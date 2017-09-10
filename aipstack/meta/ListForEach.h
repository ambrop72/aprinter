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

#ifndef AIPSTACK_LIST_FOR_EACH_H
#define AIPSTACK_LIST_FOR_EACH_H

#include <aipstack/meta/TypeList.h>
#include <aipstack/meta/BasicMetaUtils.h>
#include <aipstack/misc/Hints.h>
#include <aipstack/misc/Preprocessor.h>

namespace AIpStack {

template <typename TheList>
struct ListForEach;

template <typename Head, typename Tail>
struct ListForEach<ConsTypeList<Head, Tail>> {
    template <typename Func, typename... Args>
    AIPSTACK_ALWAYS_INLINE static void call_forward (Func func, Args... args)
    {
        func(WrapType<Head>(), args...);
        ListForEach<Tail>::call_forward(func, args...);
    }
    
    template <typename Func, typename... Args>
    AIPSTACK_ALWAYS_INLINE static bool call_forward_interruptible (Func func, Args... args)
    {
        if (!func(WrapType<Head>(), args...)) {
            return false;
        }
        return ListForEach<Tail>::call_forward_interruptible(func, args...);
    }
};

template <>
struct ListForEach<EmptyTypeList> {
    template <typename Func, typename... Args>
    AIPSTACK_ALWAYS_INLINE static void call_forward (Func func, Args... args)
    {
    }
    
    template <typename Func, typename... Args>
    AIPSTACK_ALWAYS_INLINE static bool call_forward_interruptible (Func func, Args... args)
    {
        return true;
    }
};

template <typename List, typename Func, typename... Args>
AIPSTACK_ALWAYS_INLINE void ListFor (Func func, Args... args)
{
    return ListForEach<List>::call_forward(func, args...);
}

template <typename List, typename Func, typename... Args>
AIPSTACK_ALWAYS_INLINE bool ListForBreak (Func func, Args... args)
{
    return ListForEach<List>::call_forward_interruptible(func, args...);
}

#define AIPSTACK_TL(TypeAlias, code) (auto aipstack__type_lambda_arg) { using TypeAlias = typename decltype(aipstack__type_lambda_arg)::Type; code; }

#define AIPSTACK_TLA(TypeAlias, args, code) (auto aipstack__type_lambda_arg, AIPSTACK_REMOVE_PARENS args) { using TypeAlias = typename decltype(aipstack__type_lambda_arg)::Type; code; }

}

#endif
