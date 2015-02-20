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

#ifndef AMBROLIB_TYPE_LIST_UTILS_H
#define AMBROLIB_TYPE_LIST_UTILS_H

#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/TypeDict.h>
#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/MapTypeList.h>

#include <aprinter/BeginNamespace.h>

namespace Private {
    template <typename List>
    struct TypeListLengthHelper;

    template <>
    struct TypeListLengthHelper<EmptyTypeList> {
        static const int Value = 0;
    };

    template <typename Head, typename Tail>
    struct TypeListLengthHelper<ConsTypeList<Head, Tail>> {
        static const int Value = 1 + TypeListLengthHelper<Tail>::Value;
    };
}

template <typename List>
using TypeListLength = Private::TypeListLengthHelper<List>;

namespace Private {
    template <int, typename>
    struct TypeDictElemToIndexMapHelper;
    
    template <int Offset, typename Head, typename Tail>
    struct TypeDictElemToIndexMapHelper<Offset, ConsTypeList<Head, Tail>> {
        using Result = ConsTypeList<
            TypeDictEntry<Head, WrapInt<Offset>>,
            typename TypeDictElemToIndexMapHelper<
                (Offset + 1),
                Tail
            >::Result
        >;
    };
    
    template <int Offset>
    struct TypeDictElemToIndexMapHelper<Offset, EmptyTypeList> {
        using Result = EmptyTypeList;
    };
    
    template <int, typename>
    struct TypeDictIndexToSublistMapHelper;
    
    template <int Offset, typename Head, typename Tail>
    struct TypeDictIndexToSublistMapHelper<Offset, ConsTypeList<Head, Tail>> {
        using Result = ConsTypeList<
            TypeDictEntry<WrapInt<Offset>, ConsTypeList<Head, Tail>>,
            typename TypeDictIndexToSublistMapHelper<
                (Offset + 1),
                Tail
            >::Result
        >;
    };
    
    template <int Offset>
    struct TypeDictIndexToSublistMapHelper<Offset, EmptyTypeList> {
        using Result = EmptyTypeList;
    };
}

template <typename List>
using TypeDictMakeElemToIndexMap = typename Private::TypeDictElemToIndexMapHelper<0, List>::Result;

template <typename List>
using TypeDictMakeIndexToSublistMap = typename Private::TypeDictIndexToSublistMapHelper<0, List>::Result;

namespace Private {
    template <typename List, int Index>
    struct ListIndexGetHelper {
        using FindRes = TypeDictFindNoDupl<TypeDictMakeIndexToSublistMap<List>, WrapInt<Index>>;
        static_assert(FindRes::Found, "Element index is outside the range of the list.");
        using Result = typename FindRes::Result::Head;
    };
}

template <typename List, int Index>
using TypeListGet = typename Private::ListIndexGetHelper<List, Index>::Result;

namespace Private {
    template <typename List>
    struct TypeListGetFuncHelper {
        template <typename Index>
        struct Call {
            using Type = TypeListGet<List, Index::Value>;
        };
    };
}

template <typename List>
using TypeListGetFunc = Private::TypeListGetFuncHelper<List>;

template <typename List, typename Value>
using TypeListFind = TypeDictFind<TypeDictMakeElemToIndexMap<List>, Value>;

template <typename List, typename Value>
using TypeListIndex = typename TypeListFind<List, Value>::Result;

template <typename List, typename Func, typename FuncValue>
using TypeListFindMapped = TypeListFind<MapTypeList<List, Func>, FuncValue>;

template <typename List, typename Func, typename FuncValue>
using TypeListIndexMapped = typename TypeListFindMapped<List, Func, FuncValue>::Result;

template <typename List, typename Func, typename FuncValue>
using TypeListGetMapped = TypeListGet<List, TypeListIndexMapped<List, Func, FuncValue>::Value>;

#include <aprinter/EndNamespace.h>

#endif
