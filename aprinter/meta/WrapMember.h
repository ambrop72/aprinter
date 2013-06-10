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

#ifndef AMBROLIB_WRAP_MEMBER_H
#define AMBROLIB_WRAP_MEMBER_H

#include <stddef.h>

#include <aprinter/BeginNamespace.h>

namespace WrapMemberPrivate {
    template <typename Obj, typename Type>
    struct Helper {
        template <Type Obj::*Member>
        struct Wrapper {
            static Type * access (Obj *o)
            {
                return &(o->*Member);
            }
            
            static Type const * access (Obj const *o)
            {
                return &(o->*Member);
            }
            
            static Obj * container (Type *member)
            {
                return reinterpret_cast<Obj *>(reinterpret_cast<char *>(member) - offset());
            }
            
            static Obj const * container (Type const *member)
            {
                return reinterpret_cast<Obj const *>(reinterpret_cast<char const *>(member) - offset());
            }
            
            static ptrdiff_t offset ()
            {
                union {
                    Obj obj;
                    int other;
                } dummy;
                return (reinterpret_cast<char *>(&(dummy.obj.*Member)) - reinterpret_cast<char *>(&(dummy.obj)));
            }
        };
    };

    template <typename Obj, typename Type>
    struct Helper<Obj, Type> MakeHelper (Type Obj::*member);
}

#define AMBRO_WMEMB(memb) decltype(APrinter::WrapMemberPrivate::MakeHelper(memb))::Wrapper<memb>
#define AMBRO_WMEMB_T(memb) typename decltype(APrinter::WrapMemberPrivate::MakeHelper(memb))::template Wrapper<memb>
#define AMBRO_WMEMB_TD(memb) decltype(APrinter::WrapMemberPrivate::MakeHelper(memb))::template Wrapper<memb>

#include <aprinter/EndNamespace.h>

#endif
