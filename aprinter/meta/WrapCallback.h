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

#ifndef AMBROLIB_WRAP_CALLBACK_H
#define AMBROLIB_WRAP_CALLBACK_H

#include <aprinter/meta/WrapMember.h>

#include <aprinter/BeginNamespace.h>

namespace WrapCallbackPrivate {
    template <typename Obj, typename Obj2, typename MemberType, typename R, typename... Args>
    struct Helper {
        template <R (Obj::*Method) (Args...), MemberType Obj2::*Member>
        struct Wrapper {
            static R call (MemberType *member, Args... args)
            {
                Obj *o = static_cast<Obj *>(AMBRO_WMEMB_TD(Member)::container(member));
                return (o->*Method)(args...);
            }
        };
    };

    template <typename Obj, typename Obj2, typename MemberType, typename R, typename... Args>
    struct Helper<Obj, Obj2, MemberType, R, Args...> MakeHelper (R (Obj::*method) (Args...), MemberType Obj2::*member);
}

#define AMBRO_WCALLBACK(method, member) decltype(APrinter::WrapCallbackPrivate::MakeHelper(method, member))::Wrapper<method, member>
#define AMBRO_WCALLBACK_T(method, member) typename decltype(APrinter::WrapCallbackPrivate::MakeHelper(method, member))::template Wrapper<method, member>
#define AMBRO_WCALLBACK_TD(method, member) decltype(APrinter::WrapCallbackPrivate::MakeHelper(method, member))::template Wrapper<method, member>

#include <aprinter/EndNamespace.h>

#endif
