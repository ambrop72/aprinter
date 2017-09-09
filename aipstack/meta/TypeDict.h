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

#ifndef AIPSTACK_TYPE_DICT_H
#define AIPSTACK_TYPE_DICT_H

#include <aipstack/meta/TypeList.h>
#include <aipstack/meta/BasicMetaUtils.h>

namespace AIpStack {

template <typename TKey, typename TValue>
struct TypeDictEntry {
    using Key = TKey;
    using Value = TValue;
};

template <typename TResult>
struct TypeDictFound {
    static bool const Found = true;
    using Result = TResult;
};

struct TypeDictNotFound {
    static bool const Found = false;
};

namespace Private {
    template <typename EntriesList>
    struct TypeDictHelper;

    template <typename Key, typename Value, typename Tail>
    struct TypeDictHelper<ConsTypeList<TypeDictEntry<Key, Value>, Tail>>
    : public TypeDictHelper<Tail>
    {
    };

    template <>
    struct TypeDictHelper<EmptyTypeList> {
    };

    template <typename Key, typename Value, typename Tail>
    TypeDictFound<Value> TypeDictGetHelper(TypeDictHelper<ConsTypeList<TypeDictEntry<Key, Value>, Tail>>);

    template <typename Key>
    TypeDictNotFound TypeDictGetHelper(TypeDictHelper<EmptyTypeList>);
    
    template <typename, typename>
    struct TypeDictRemoveDuplicatesHelper;
    
    template <typename Current, typename Key, typename Value, typename Tail>
    struct TypeDictRemoveDuplicatesHelper<Current, ConsTypeList<TypeDictEntry<Key, Value>, Tail>> {
        using Result = typename TypeDictRemoveDuplicatesHelper<
            If<
                decltype(TypeDictGetHelper<Key>(TypeDictHelper<Current>()))::Found,
                Current,
                ConsTypeList<TypeDictEntry<Key, Value>, Current>
            >,
            Tail
        >::Result;
    };
    
    template <typename Current>
    struct TypeDictRemoveDuplicatesHelper<Current, EmptyTypeList> {
        using Result = Current;
    };
    
    template <typename Default, typename FindResult>
    struct TypeDictDefaultHelper {
        using Result = typename FindResult::Result;
    };
    
    template <typename Default>
    struct TypeDictDefaultHelper<Default, TypeDictNotFound> {
        using Result = Default;
    };
}

template <typename EntriesList, typename Key>
using TypeDictFindNoDupl = decltype(Private::TypeDictGetHelper<Key>(Private::TypeDictHelper<EntriesList>()));

template <typename EntriesList>
using TypeDictRemoveDuplicatesAndReverse = typename Private::TypeDictRemoveDuplicatesHelper<EmptyTypeList, EntriesList>::Result;

template <typename EntriesList, typename Key>
using TypeDictFind = TypeDictFindNoDupl<TypeDictRemoveDuplicatesAndReverse<EntriesList>, Key>;

template <typename EntriesList, typename Key, typename Default>
using TypeDictGetOrDefault = typename Private::template TypeDictDefaultHelper<Default, TypeDictFind<EntriesList, Key>>::Result;

}

#endif
