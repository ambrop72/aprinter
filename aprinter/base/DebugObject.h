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

#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

class DebugObjectGroup {
public:
    inline void init ();
    inline void deinit ();
    
private:
#ifdef AMBROLIB_ASSERTIONS
    uint32_t m_count;
#endif
    
    template <typename Context, typename Ident>
    friend class DebugObject;
};

template <typename Context, typename Ident>
class DebugObject {
public:
    void debugInit (Context c);
    void debugDeinit (Context c);
    void debugAccess (Context c);
    
private:
    static uint32_t getMagic ();
    
#ifdef AMBROLIB_ASSERTIONS
    uint32_t m_magic;
#endif
};

void DebugObjectGroup::init ()
{
#ifdef AMBROLIB_ASSERTIONS
    m_count = 0;
#endif
}

void DebugObjectGroup::deinit ()
{
#ifdef AMBROLIB_ASSERTIONS
    AMBRO_ASSERT(m_count == 0)
#endif
}

template <typename Context, typename Ident>
void DebugObject<Context, Ident>::debugInit (Context c)
{
#ifdef AMBROLIB_ASSERTIONS
    DebugObjectGroup *g = c.debugObjectGroup();
    m_magic = getMagic();
    g->m_count++;
#endif
}

template <typename Context, typename Ident>
void DebugObject<Context, Ident>::debugDeinit (Context c)
{
#ifdef AMBROLIB_ASSERTIONS
    DebugObjectGroup *g = c.debugObjectGroup();
    AMBRO_ASSERT(m_magic == getMagic())
    m_magic = 0;
    g->m_count--;
#endif
}

template <typename Context, typename Ident>
void DebugObject<Context, Ident>::debugAccess (Context c)
{
#ifdef AMBROLIB_ASSERTIONS
    AMBRO_ASSERT(m_magic == getMagic())
#endif
}

template <typename Context, typename Ident>
uint32_t DebugObject<Context, Ident>::getMagic ()
{
    return UINT32_C(0x1c5c0678);
}

#include <aprinter/EndNamespace.h>

#endif
