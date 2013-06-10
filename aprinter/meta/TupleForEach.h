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

#ifndef AMBROLIB_TUPLE_FOR_EACH_H
#define AMBROLIB_TUPLE_FOR_EACH_H

#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/Tuple.h>

#include <aprinter/BeginNamespace.h>

template <typename TheTuple>
struct TupleForEach;

template <typename Head, typename Tail>
struct TupleForEach<Tuple<ConsTypeList<Head, Tail>>> {
    typedef Tuple<ConsTypeList<Head, Tail>> TupleType;
    typedef typename TupleType::TailTupleType TailTupleType;
    
    template <typename Func, typename... Args>
    static void call_forward (TupleType *tuple, Func func, Args... args)
    {
        func(&tuple->elem, args...);
        TupleForEach<TailTupleType>::call_forward(tuple->getTail(), func, args...);
    }
    
    template <typename Func, typename... Args>
    static void call_reverse (TupleType *tuple, Func func, Args... args)
    {
        TupleForEach<TailTupleType>::call_reverse(tuple->getTail(), func, args...);
        func(&tuple->elem, args...);
    }
    
    template <typename Func, typename... Args>
    static bool call_forward_interruptible (TupleType *tuple, Func func, Args... args)
    {
        if (!func(&tuple->elem, args...)) {
            return false;
        }
        return TupleForEach<TailTupleType>::call_forward_interruptible(tuple->getTail(), func, args...);
    }
};

template <>
struct TupleForEach<Tuple<EmptyTypeList>> {
    typedef Tuple<EmptyTypeList> TupleType;
    
    template <typename Func, typename... Args>
    static void call_forward (TupleType *tuple, Func func, Args... args)
    {
    }
    
    template <typename Func, typename... Args>
    static void call_reverse (TupleType *tuple, Func func, Args... args)
    {
    }
    
    template <typename Func, typename... Args>
    static bool call_forward_interruptible (TupleType *tuple, Func func, Args... args)
    {
        return true;
    }
};

#include <aprinter/EndNamespace.h>

#endif
