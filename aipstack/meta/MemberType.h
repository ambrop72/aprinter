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

#ifndef AIPSTACK_MEMBER_TYPE_H
#define AIPSTACK_MEMBER_TYPE_H

#include <aipstack/meta/BasicMetaUtils.h>

#define AIPSTACK_DECLARE_HAS_MEMBER_TYPE_FUNC(ClassName, TypeMemberName) \
struct ClassName { \
    template <typename T> \
    class Call { \
    private: \
        typedef char Yes[1]; \
        typedef char No[2]; \
        \
        template <typename U> \
        static Yes & test (typename U::TypeMemberName *arg); \
        \
        template <typename U> \
        static No & test (...); \
        \
    public: \
        typedef AIpStack::WrapBool<(sizeof(test<T>(0)) == sizeof(Yes))> Type; \
    }; \
};

#define AIPSTACK_DECLARE_GET_MEMBER_TYPE_FUNC(ClassName, TypeMemberName) \
struct ClassName { \
    template <typename T> \
    struct Call { \
        typedef typename T::TypeMemberName Type; \
    }; \
};

#define AIPSTACK_DEFINE_MEMBER_TYPE(ClassName, MemberName) \
struct ClassName { \
    AIPSTACK_DECLARE_HAS_MEMBER_TYPE_FUNC(Has, MemberName) \
    AIPSTACK_DECLARE_GET_MEMBER_TYPE_FUNC(Get, MemberName) \
};

#endif
