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

#ifndef AMBROLIB_TYPE_LIST_GET_H
#define AMBROLIB_TYPE_LIST_GET_H

#include <aprinter/meta/TypeList.h>

#include <aprinter/BeginNamespace.h>

template <typename List, int Index>
struct TypeListGetHelper;

template <typename Head, typename Tail>
struct TypeListGetHelper<ConsTypeList<Head, Tail>, 0> {
    typedef Head Type;
};

template <typename Head, typename Tail, int Index>
struct TypeListGetHelper<ConsTypeList<Head, Tail>, Index> {
    typedef typename TypeListGetHelper<Tail, (Index - 1)>::Type Type;
};

template <typename List, int Index>
using TypeListGet = typename TypeListGetHelper<List, Index>::Type;

template <typename List>
struct TypeListGetFunc {
    template <typename Index>
    struct Call {
        using Type = TypeListGet<List, Index::value>;
    };
};

#include <aprinter/EndNamespace.h>

#endif
