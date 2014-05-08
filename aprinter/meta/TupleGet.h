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

#ifndef AMBROLIB_TUPLE_GET_H
#define AMBROLIB_TUPLE_GET_H

#include <stddef.h>

#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/Tuple.h>

#include <aprinter/BeginNamespace.h>

template <typename TheTuple, int Index>
struct TupleGet;

template <typename Head, typename Tail, int Index>
struct TupleGet<Tuple<ConsTypeList<Head, Tail>>, Index> {
    typedef Tuple<ConsTypeList<Head, Tail>> ThisTupleType;
    typedef TupleGet<Tuple<Tail>, Index - 1> TargetTupleGet;
    
    typedef typename TargetTupleGet::ElemType ElemType;
    typedef typename TargetTupleGet::TupleType TupleType;
    
    static ElemType * getElem (ThisTupleType *tuple)
    {
        return TargetTupleGet::getElem(tuple->getTail());
    }
    
    static ElemType const * getElem (ThisTupleType const *tuple)
    {
        return TargetTupleGet::getElem(tuple->getTail());
    }
};

template <typename Head, typename Tail>
struct TupleGet<Tuple<ConsTypeList<Head, Tail>>, 0> {
    typedef Tuple<ConsTypeList<Head, Tail>> ThisTupleType;
    typedef Head ElemType;
    typedef Tuple<ConsTypeList<Head, Tail>> TupleType;
    
    static ElemType * getElem (ThisTupleType *tuple)
    {
        return tuple->getHead();
    }
    
    static ElemType const * getElem (ThisTupleType const *tuple)
    {
        return tuple->getHead();
    }
};

template <int Index, typename TupleType>
auto TupleGetElem (TupleType *tuple) -> decltype(TupleGet<TupleType, Index>::getElem(tuple))
{
    return TupleGet<TupleType, Index>::getElem(tuple);
}

#include <aprinter/EndNamespace.h>

#endif
