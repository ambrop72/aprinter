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

#ifndef AMBROLIB_LIST_FOR_EACH_H
#define AMBROLIB_LIST_FOR_EACH_H

#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/base/Hints.h>
#include <aprinter/base/Preprocessor.h>

#include <aprinter/BeginNamespace.h>

template <typename TheList>
struct ListForEach;

template <typename Head, typename Tail>
struct ListForEach<ConsTypeList<Head, Tail>> {
    template <typename Func, typename... Args>
    AMBRO_ALWAYS_INLINE static void call_forward (Func func, Args... args)
    {
        func(WrapType<Head>(), args...);
        ListForEach<Tail>::call_forward(func, args...);
    }
    
    template <typename Func, typename... Args>
    AMBRO_ALWAYS_INLINE static void call_reverse (Func func, Args... args)
    {
        ListForEach<Tail>::call_reverse(func, args...);
        func(WrapType<Head>(), args...);
    }
    
    template <typename Func, typename... Args>
    AMBRO_ALWAYS_INLINE static bool call_forward_interruptible (Func func, Args... args)
    {
        if (!func(WrapType<Head>(), args...)) {
            return false;
        }
        return ListForEach<Tail>::call_forward_interruptible(func, args...);
    }
    
    template <typename AccRes, typename Func, typename... Args>
    AMBRO_ALWAYS_INLINE static auto call_forward_accres (AccRes acc_res, Func func, Args... args) -> decltype(ListForEach<Tail>::call_forward_accres(func(WrapType<Head>(), acc_res, args...), func, args...))
    {
        return ListForEach<Tail>::call_forward_accres(func(WrapType<Head>(), acc_res, args...), func, args...);
    }
};

template <>
struct ListForEach<EmptyTypeList> {
    template <typename Func, typename... Args>
    AMBRO_ALWAYS_INLINE static void call_forward (Func func, Args... args)
    {
    }
    
    template <typename Func, typename... Args>
    AMBRO_ALWAYS_INLINE static void call_reverse (Func func, Args... args)
    {
    }
    
    template <typename Func, typename... Args>
    AMBRO_ALWAYS_INLINE static bool call_forward_interruptible (Func func, Args... args)
    {
        return true;
    }
    
    template <typename AccRes, typename Func, typename... Args>
    AMBRO_ALWAYS_INLINE static auto call_forward_accres (AccRes acc_res, Func func, Args... args) -> decltype(acc_res)
    {
        return acc_res;
    }
};

template <typename List, int Offset, typename Ret, typename IndexType>
struct ListForOneHelper;

template <typename Head, typename Tail, int Offset, typename Ret, typename IndexType>
struct ListForOneHelper<ConsTypeList<Head, Tail>, Offset, Ret, IndexType> {
    template <typename Func, typename... Args>
    AMBRO_ALWAYS_INLINE static Ret call (IndexType index, Func func, Args... args)
    {
        if (index == Offset) {
            return func(WrapType<Head>(), args...);
        }
        return ListForOneHelper<Tail, Offset + 1, Ret, IndexType>::call(index, func, args...);
    }
    
    template <typename Func, typename... Args>
    AMBRO_ALWAYS_INLINE static bool call_bool (IndexType index, Func func, Args... args)
    {
        if (index == Offset) {
            func(WrapType<Head>(), args...);
            return true;
        }
        return ListForOneHelper<Tail, Offset + 1, Ret, IndexType>::call_bool(index, func, args...);
    }
};

template <int Offset, typename Ret, typename IndexType>
struct ListForOneHelper<EmptyTypeList, Offset, Ret, IndexType> {
    template <typename Func, typename... Args>
    AMBRO_ALWAYS_INLINE static Ret call (IndexType index, Func func, Args... args)
    {
        __builtin_unreachable();
    }
    
    template <typename Func, typename... Args>
    AMBRO_ALWAYS_INLINE static bool call_bool (IndexType index, Func func, Args... args)
    {
        return false;
    }
};

template <typename List, typename Func, typename... Args>
AMBRO_ALWAYS_INLINE void ListFor (Func func, Args... args)
{
    return ListForEach<List>::call_forward(func, args...);
}

template <typename List, typename Func, typename... Args>
AMBRO_ALWAYS_INLINE void ListForReverse (Func func, Args... args)
{
    return ListForEach<List>::call_reverse(func, args...);
}

template <typename List, typename Func, typename... Args>
AMBRO_ALWAYS_INLINE bool ListForBreak (Func func, Args... args)
{
    return ListForEach<List>::call_forward_interruptible(func, args...);
}

template <typename List, typename InitialAccRes, typename Func, typename... Args>
AMBRO_ALWAYS_INLINE auto ListForFold (InitialAccRes initial_acc_res, Func func, Args... args) -> decltype(ListForEach<List>::call_forward_accres(initial_acc_res, func, args...))
{
    return ListForEach<List>::call_forward_accres(initial_acc_res, func, args...);
}

template <typename List, int Offset = 0, typename Ret = void, typename IndexType, typename Func, typename... Args>
AMBRO_ALWAYS_INLINE Ret ListForOne (IndexType index, Func func, Args... args)
{
    return ListForOneHelper<List, Offset, Ret, IndexType>::call(index, func, args...);
}

template <typename List, int Offset = 0, typename IndexType, typename Func, typename... Args>
AMBRO_ALWAYS_INLINE bool ListForOneBool (IndexType index, Func func, Args... args)
{
    return ListForOneHelper<List, Offset, void, IndexType>::call_bool(index, func, args...);
}

#define APRINTER_TL(TypeAlias, code) (auto aprinter__type_lambda_arg) { using TypeAlias = typename decltype(aprinter__type_lambda_arg)::Type; code; }

#define APRINTER_TLA(TypeAlias, args, code) (auto aprinter__type_lambda_arg, APRINTER_REMOVE_PARENS args) { using TypeAlias = typename decltype(aprinter__type_lambda_arg)::Type; code; }

#include <aprinter/EndNamespace.h>

#endif
