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

#ifndef AMBROLIB_OFFSET_CALLBACK_H
#define AMBROLIB_OFFSET_CALLBACK_H

#include <aprinter/base/GetContainer.h>

#include <aprinter/BeginNamespace.h>

namespace OffsetCallbackPrivate {
    template <typename ObjType, typename MemberType, typename R, typename... Args>
    struct HelperStruct {
        template <MemberType ObjType::*Member, R (ObjType::*Callback) (Args...)>
        struct Wrapper {
            static R wrapper_func (MemberType *member_ptr, Args... args)
            {
                ObjType *obj = GetContainer(member_ptr, Member);
                return (obj->*Callback)(args...);
            }
        };
    };
    
    template <typename ObjType, typename MemberType, typename R, typename... Args>
    HelperStruct<ObjType, MemberType, R, Args...> HelperFunc (MemberType ObjType::*member, R (ObjType::*callback) (Args...));
    
    template <typename ObjType, typename MemberType, typename MemberType2, typename R, typename... Args>
    struct HelperStruct2 {
        template <MemberType ObjType::*Member, MemberType2 MemberType::*Member2, R (ObjType::*Callback) (Args...)>
        struct Wrapper {
            static R wrapper_func (MemberType2 *member2_ptr, Args... args)
            {
                MemberType *member_ptr = GetContainer(member2_ptr, Member2);
                ObjType *obj = GetContainer(member_ptr, Member);
                return (obj->*Callback)(args...);
            }
        };
    };
    
    template <typename ObjType, typename MemberType, typename MemberType2, typename R, typename... Args>
    HelperStruct2<ObjType, MemberType, MemberType2, R, Args...> HelperFunc2 (MemberType ObjType::*member, MemberType2 MemberType::*member2, R (ObjType::*callback) (Args...));
}

#define AMBRO_OFFSET_CALLBACK(member, callback) decltype(APrinter::OffsetCallbackPrivate::HelperFunc(member, callback))::Wrapper<member, callback>::wrapper_func
#define AMBRO_OFFSET_CALLBACK_T(member, callback) decltype(APrinter::OffsetCallbackPrivate::HelperFunc(member, callback))::template Wrapper<member, callback>::wrapper_func

#define AMBRO_OFFSET_CALLBACK2(member, member2, callback) decltype(APrinter::OffsetCallbackPrivate::HelperFunc2(member, member2, callback))::Wrapper<member, member2, callback>::wrapper_func
#define AMBRO_OFFSET_CALLBACK2_T(member, member2, callback) decltype(APrinter::OffsetCallbackPrivate::HelperFunc2(member, member2, callback))::template Wrapper<member, member2, callback>::wrapper_func

#include <aprinter/EndNamespace.h>

#endif
