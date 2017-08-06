/*
 * Copyright (c) 2016 Ambroz Bizjak
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

#ifndef APRINTER_ACCESSOR_H
#define APRINTER_ACCESSOR_H

namespace APrinter {

template <typename Object, typename Member, Member Object::*MemberPtr>
struct MemberAccessor {
    using ObjectType = Object;
    using MemberType = Member;
    
    inline static MemberType & access (ObjectType &e)
    {
        return e.*MemberPtr;
    }
};

template <typename Object, typename Member>
struct MakeMemberAccessorHelper {
    template <Member Object::*MemberPtr>
    using Make = MemberAccessor<Object, Member, MemberPtr>;
};

template <typename Object, typename Member>
inline MakeMemberAccessorHelper<Object, Member> MakeMemberAccessorHelperFunc(Member Object::*memberPtr);

#define APRINTER_MEMBER_ACCESSOR(member) \
decltype(APrinter::MakeMemberAccessorHelperFunc((member)))::template Make<(member)>

#define APRINTER_MEMBER_ACCESSOR_TN(member) typename APRINTER_MEMBER_ACCESSOR(member)

template <typename Object, typename Member, typename Base, Member Base::*MemberPtr>
struct MemberAccessorWithBase {
    using ObjectType = Object;
    using MemberType = Member;
    
    inline static MemberType & access (ObjectType &e)
    {
        return e.*MemberPtr;
    }
};

template <typename Accessor1, typename Accessor2>
struct ComposedAccessor {
    using ObjectType = typename Accessor1::ObjectType;
    using MemberType = typename Accessor2::MemberType;
    
    inline static MemberType & access (ObjectType &e)
    {
        return Accessor2::access(Accessor1::access(e));
    }
};

template <typename Object, typename Member>
struct BaseClassAccessor {
    using ObjectType = Object;
    using MemberType = Member;
    
    inline static MemberType & access (ObjectType &e)
    {
        return e;
    }
};

}

#endif
