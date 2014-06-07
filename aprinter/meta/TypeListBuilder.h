/*
 * Copyright (c) 2014 Ambroz Bizjak
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

#ifndef APRINTER_TYPE_LIST_BUILDER
#define APRINTER_TYPE_LIST_BUILDER

#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/TypeListReverse.h>

#include <aprinter/BeginNamespace.h>

// See: http://stackoverflow.com/questions/24088373/building-a-compile-time-list-incrementally-in-c

template <int Count>
struct TypeListBuilder__Counter : public TypeListBuilder__Counter<(Count - 1)> {};

template <>
struct TypeListBuilder__Counter<0> {};

#define APRINTER_START_LIST_INTERNAL(Name, Qualifier) \
Qualifier APrinter::EmptyTypeList Name##__Helper (APrinter::TypeListBuilder__Counter<__COUNTER__>);

#define APRINTER_ADD_TO_LIST_INTERNAL(Name, Type, Qualifier) \
Qualifier APrinter::ConsTypeList<Type, decltype(Name##__Helper(APrinter::TypeListBuilder__Counter<__COUNTER__>()))> Name##__Helper (APrinter::TypeListBuilder__Counter<__COUNTER__>);

#define APRINTER_END_LIST(Name) \
using Name = APrinter::TypeListReverse<decltype(Name##__Helper(APrinter::TypeListBuilder__Counter<__COUNTER__>()))>;

#define APRINTER_START_LIST(Name) APRINTER_START_LIST_INTERNAL(Name, static)
#define APRINTER_ADD_TO_LIST(Name, Type) APRINTER_ADD_TO_LIST_INTERNAL(Name, Type, static)

#define APRINTER_START_LIST_FUNC(Name) APRINTER_START_LIST_INTERNAL(Name,)
#define APRINTER_ADD_TO_LIST_FUNC(Name, Type) APRINTER_ADD_TO_LIST_INTERNAL(Name, Type,)

#include <aprinter/EndNamespace.h>

#endif
