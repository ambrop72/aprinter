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

#ifndef AMBROLIB_TYPE_DICT_LIST_H
#define AMBROLIB_TYPE_DICT_LIST_H

#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/TypeDict.h>
#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/MapTypeList.h>
#include <aprinter/meta/TypeListGet.h>

#include <aprinter/BeginNamespace.h>

namespace Private {
    template <int, typename>
    struct TypeDictListMapHelper;
    
    template <int Offset, typename Head, typename Tail>
    struct TypeDictListMapHelper<Offset, ConsTypeList<Head, Tail>> {
        using Result = ConsTypeList<
            TypeDictEntry<Head, WrapInt<Offset>>,
            typename TypeDictListMapHelper<
                (Offset + 1),
                Tail
            >::Result
        >;
    };
    
    template <int Offset>
    struct TypeDictListMapHelper<Offset, EmptyTypeList> {
        using Result = EmptyTypeList;
    };
}

template <typename List, typename Value>
using TypeDictListFind = TypeDictFind<typename Private::TypeDictListMapHelper<0, List>::Result, Value>;

template <typename List, typename Value>
using TypeDictListIndex = typename TypeDictListFind<List, Value>::Result;

template <typename List, typename Func, typename FuncValue>
using TypeDictListFindMapped = TypeDictListFind<MapTypeList<List, Func>, FuncValue>;

template <typename List, typename Func, typename FuncValue>
using TypeDictListIndexMapped = typename TypeDictListFindMapped<List, Func, FuncValue>::Result;

template <typename List, typename Func, typename FuncValue>
using TypeDictListGetMapped = TypeListGet<List, TypeDictListIndexMapped<List, Func, FuncValue>::Value>;

#include <aprinter/EndNamespace.h>

#endif
