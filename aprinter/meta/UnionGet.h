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

#ifndef AMBROLIB_UNION_GET_H
#define AMBROLIB_UNION_GET_H

#include <stddef.h>

#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/Union.h>

namespace APrinter {

template <typename TheUnion, int Index>
struct UnionGet;

template <typename Head, typename Tail, int Index>
struct UnionGet<ConsTypeList<Head, Tail>, Index> {
    typedef Union<ConsTypeList<Head, Tail>> ThisUnionType;
    typedef UnionGet<Tail, Index - 1> TargetUnionGet;
    
    typedef typename TargetUnionGet::ElemType ElemType;
    typedef typename TargetUnionGet::UnionType UnionType;
    
    static ElemType * getElem (ThisUnionType *tuple)
    {
        return TargetUnionGet::getElem(tuple->getTail());
    }
    
    static ElemType const * getElem (ThisUnionType const *tuple)
    {
        return TargetUnionGet::getElem(tuple->getTail());
    }
};

template <typename Head, typename Tail>
struct UnionGet<ConsTypeList<Head, Tail>, 0> {
    typedef Union<ConsTypeList<Head, Tail>> ThisUnionType;
    typedef Head ElemType;
    typedef Union<ConsTypeList<Head, Tail>> UnionType;
    
    static ElemType * getElem (ThisUnionType *tuple)
    {
        return &tuple->elem;
    }
    
    static ElemType const * getElem (ThisUnionType const *tuple)
    {
        return &tuple->elem;
    }
};

template <int Index, typename UnionType>
auto UnionGetElem (UnionType *tuple) -> decltype(UnionGet<typename UnionType::ElemTypes, Index>::getElem(tuple))
{
    return UnionGet<typename UnionType::ElemTypes, Index>::getElem(tuple);
}

}

#endif
