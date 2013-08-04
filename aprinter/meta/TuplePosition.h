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

#ifndef AMBROLIB_TUPLE_POSITION_H
#define AMBROLIB_TUPLE_POSITION_H

#include <aprinter/meta/TupleGet.h>
#include <aprinter/meta/Position.h>

#include <aprinter/BeginNamespace.h>

template <typename TParent, typename TupleType, TupleType TParent::ObjectType::*TTupleMemberPtr, int TupleIndex>
struct TuplePosition {
    using Parent = TParent;
    using ObjectType = typename TupleGet<TupleType, TupleIndex>::ElemType;
    using ParentType = typename Parent::ObjectType;
    
    static ObjectType * down (ParentType *x)
    {
        return TupleGetElem<TupleIndex>(&(x->*TTupleMemberPtr));
    }
    
    static ParentType * up (ObjectType *x)
    {
        TupleType *tuple = TupleGetTuple<TupleIndex, TupleType>(x);
        union Dummy {
            ParentType p;
            int d;
        } dummy;
        ptrdiff_t tuple_offest = reinterpret_cast<char *>(&((dummy.p).*TTupleMemberPtr)) - reinterpret_cast<char *>(&dummy.p);
        return reinterpret_cast<ParentType *>(reinterpret_cast<char *>(tuple) - tuple_offest);
    }
};

#include <aprinter/EndNamespace.h>

#endif
