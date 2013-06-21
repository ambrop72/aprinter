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

#ifndef AMBROLIB_RUNTIME_LIST_FOLD_H
#define AMBROLIB_RUNTIME_LIST_FOLD_H

#include <aprinter/meta/TypeList.h>

#include <aprinter/BeginNamespace.h>

template <typename List>
struct RuntimeListFold;

template <typename Head, typename Tail>
struct RuntimeListFold<ConsTypeList<Head, Tail>> {
    template <typename Oper, typename... Args>
    static auto call (Args... args) -> decltype(Oper::template combine<Head>(RuntimeListFold<Tail>::template call<Oper>(args...), args...))
    {
        return Oper::template combine<Head>(RuntimeListFold<Tail>::template call<Oper>(args...), args...);
    }
};

template <>
struct RuntimeListFold<EmptyTypeList> {
    template <typename Oper, typename... Args>
    static auto call (Args... args) -> decltype(Oper::zero(args...))
    {
        return Oper::zero(args...);
    }
};

#include <aprinter/EndNamespace.h>

#endif
