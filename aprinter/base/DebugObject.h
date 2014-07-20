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

#ifndef AMBROLIB_DEBUG_OBJECT_H
#define AMBROLIB_DEBUG_OBJECT_H

#include <stdint.h>

#include <aprinter/base/Object.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject>
class DebugObjectGroup {
public:
    struct Object;
    
    static void init (Context c)
    {
#ifdef AMBROLIB_ASSERTIONS
        auto *o = Object::self(c);
        o->m_count = 0;
#endif
    }
    
    static void deinit (Context c)
    {
#ifdef AMBROLIB_ASSERTIONS
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_count == 0)
#endif
    }
    
public:
    struct Object : public ObjBase<DebugObjectGroup, ParentObject, EmptyTypeList> {
#ifdef AMBROLIB_ASSERTIONS
        uint32_t m_count;
#endif
    };
};

template <typename Context, typename Ident>
class DebugObject {
public:
    template <typename ThisContext>
    void debugInit (ThisContext c)
    {
#ifdef AMBROLIB_ASSERTIONS
        auto *go = Context::DebugGroup::Object::self(c);
        m_magic = getMagic();
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            go->m_count++;
        }
#endif
    }
    
    template <typename ThisContext>
    void debugDeinit (ThisContext c)
    {
#ifdef AMBROLIB_ASSERTIONS
        auto *go = Context::DebugGroup::Object::self(c);
        AMBRO_ASSERT(m_magic == getMagic())
        m_magic = 0;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            go->m_count--;
        }
#endif
    }
    
    template <typename ThisContext>
    void debugAccess (ThisContext c)
    {
#ifdef AMBROLIB_ASSERTIONS
        AMBRO_ASSERT(m_magic == getMagic())
#endif
    }
    
private:
    static uint32_t getMagic ()
    {
        return UINT32_C(0x1c5c0678);
    }
    
#ifdef AMBROLIB_ASSERTIONS
    uint32_t m_magic;
#endif
};

#include <aprinter/EndNamespace.h>

#endif
