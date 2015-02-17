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
#include <aprinter/meta/TypeListLength.h>
#include <aprinter/meta/TypeDict.h>
#include <aprinter/meta/WrapValue.h>

#include <aprinter/BeginNamespace.h>

namespace Private {
    template <int, typename>
    struct TypeListGetMap;
    
    template <int Offset, typename Head, typename Tail>
    struct TypeListGetMap<Offset, ConsTypeList<Head, Tail>> {
        using Result = ConsTypeList<
            TypeDictEntry<WrapInt<Offset>, Head>,
            typename TypeListGetMap<
                (Offset + 1),
                Tail
            >::Result
        >;
    };
    
    template <int Offset>
    struct TypeListGetMap<Offset, EmptyTypeList> {
        using Result = EmptyTypeList;
    };
    
    template <typename List, int Index>
    struct ListIndexGetHelper {
        static int const Length = TypeListLength<List>::Value;
        static_assert(Index >= 0, "Element index is too small.");
        static_assert(Index < Length, "Element index is too large.");
        using Result = typename TypeDictFindNoDupl<typename TypeListGetMap<0, List>::Result, WrapInt<Index>>::Result;
    };
}

template <typename List, int Index>
using TypeListGet = typename Private::ListIndexGetHelper<List, Index>::Result;

template <typename List>
struct TypeListGetFunc {
    template <typename Index>
    struct Call {
        using Type = TypeListGet<List, Index::Value>;
    };
};

#include <aprinter/EndNamespace.h>

#endif
