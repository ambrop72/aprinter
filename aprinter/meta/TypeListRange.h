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

#ifndef AMBROLIB_TYPE_LIST_RANGE_H
#define AMBROLIB_TYPE_LIST_RANGE_H

#include <aprinter/meta/TypeList.h>

#include <aprinter/BeginNamespace.h>

template <typename List, int IndexFrom>
struct TypeListRangeFromHelper;

template <>
struct TypeListRangeFromHelper<EmptyTypeList, 0> {
    using Result = EmptyTypeList;
};

template <typename Head, typename Tail>
struct TypeListRangeFromHelper<ConsTypeList<Head, Tail>, 0> {
    using Result = ConsTypeList<Head, Tail>;
};

template <typename Head, typename Tail, int IndexFrom>
struct TypeListRangeFromHelper<ConsTypeList<Head, Tail>, IndexFrom> {
    static_assert(IndexFrom > 0, "");
    using Result = typename TypeListRangeFromHelper<Tail, (IndexFrom - 1)>::Result;
};

template <typename List, int IndexTo>
struct TypeListRangeToHelper;

template <>
struct TypeListRangeToHelper<EmptyTypeList, 0> {
    using Result = EmptyTypeList;
};

template <typename Head, typename Tail>
struct TypeListRangeToHelper<ConsTypeList<Head, Tail>, 0> {
    using Result = EmptyTypeList;
};

template <typename Head, typename Tail, int IndexTo>
struct TypeListRangeToHelper<ConsTypeList<Head, Tail>, IndexTo> {
    static_assert(IndexTo > 0, "");
    using Result = ConsTypeList<Head, typename TypeListRangeToHelper<Tail, (IndexTo - 1)>::Result>;
};

template <typename List, int IndexFrom>
using TypeListRangeFrom = typename TypeListRangeFromHelper<List, IndexFrom>::Result;

template <typename List, int IndexTo>
using TypeListRangeTo = typename TypeListRangeToHelper<List, IndexTo>::Result;

template <typename List, int IndexFrom, int Count>
using TypeListRange = TypeListRangeTo<TypeListRangeFrom<List, IndexFrom>, Count>;

#include <aprinter/EndNamespace.h>

#endif
