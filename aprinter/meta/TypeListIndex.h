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

#ifndef AMBROLIB_TYPE_LIST_INDEX_H
#define AMBROLIB_TYPE_LIST_INDEX_H

#include <aprinter/meta/TypeList.h>

#include <aprinter/BeginNamespace.h>

template <typename List, typename Predicate>
struct TypeListIndex;

template <typename Head, typename Tail, typename Predicate, bool Satisfies>
struct TypeListIndexHelper;

template <typename Head, typename Tail, typename Predicate>
struct TypeListIndexHelper<Head, Tail, Predicate, true> {
    static const int Value = 0;
};

template <typename Head, typename Tail, typename Predicate>
struct TypeListIndexHelper<Head, Tail, Predicate, false> {
    static const int tail_index = TypeListIndex<Tail, Predicate>::Value;
    static const int Value = (tail_index >= 0) + tail_index;
};

template <typename Head, typename Tail, typename Predicate>
struct TypeListIndex<ConsTypeList<Head, Tail>, Predicate> {
    static const int Value = TypeListIndexHelper<Head, Tail, Predicate, Predicate::template Call<Head>::Type::Value>::Value;
};

template <typename Predicate>
struct TypeListIndex<EmptyTypeList, Predicate> {
    static const int Value = -1;
};

#include <aprinter/EndNamespace.h>

#endif
