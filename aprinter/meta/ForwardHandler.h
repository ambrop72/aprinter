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

#ifndef AMBROLIB_FORWARD_HANDLER_H
#define AMBROLIB_FORWARD_HANDLER_H

#include <aprinter/meta/WrapMember.h>

#include <aprinter/BeginNamespace.h>

template <typename ObjectType, typename MemberType, MemberType ObjectType::*MemberPtr, typename BaseHandler>
struct ForwardHandler {
    template <typename... Args>
    static auto call (MemberType *member, Args... args)
    {
        return BaseHandler::call(AMBRO_WMEMB_TD(MemberPtr)::container(member), args...);
    }
};

template <typename ObjectType, typename MemberType>
struct ForwardHandlerMatchStruct {
    template <MemberType ObjectType::*MemberPtr, typename BaseHandler>
    using Result = ForwardHandler<ObjectType, MemberType, MemberPtr, BaseHandler>;
};

template <typename ObjectType, typename MemberType>
ForwardHandlerMatchStruct<ObjectType, MemberType> ForwardHandlerMatchFunc (MemberType ObjectType::*);

#define AMBRO_FHANDLER_TD(member, handler) decltype(APrinter::ForwardHandlerMatchFunc(member))::template Result<member, handler>

#include <aprinter/EndNamespace.h>

#endif

