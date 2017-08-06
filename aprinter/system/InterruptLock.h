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

#ifndef AMBROLIB_INTERRUPT_LOCK_H
#define AMBROLIB_INTERRUPT_LOCK_H

#include <aprinter/meta/MemberType.h>
#include <aprinter/meta/FuncUtils.h>
#include <aprinter/system/InterruptLockCommon.h>

namespace APrinter {

#if APRINTER_INTERRUPT_LOCK_MODE == APRINTER_INTERRUPT_LOCK_MODE_SIMPLE
#define APRINTER_HAVE_INTR_CONTEXT 0
#elif APRINTER_INTERRUPT_LOCK_MODE == APRINTER_INTERRUPT_LOCK_MODE_MULTILEVEL
#define APRINTER_HAVE_INTR_CONTEXT 1
#else
#error "Bad APRINTER_INTERRUPT_LOCK_MODE"
#endif

template <typename Context>
struct AtomicContext : public Context {
    explicit AtomicContext (Context c) : Context(c) {}
    using AtomicContextTag = void;
    using ParentContext = Context;
};

template <typename Context>
AtomicContext<Context> MakeAtomicContext (Context c)
{
    return AtomicContext<Context>(c);
}

#if APRINTER_HAVE_INTR_CONTEXT

template <typename Context>
struct InterruptContext : public Context {
    explicit InterruptContext (Context c) : Context(c) {}
    using InterruptContextTag = void;
    using ParentContext = Context;
};

template <typename Context>
InterruptContext<Context> MakeInterruptContext (Context c)
{
    return InterruptContext<Context>(c);
}

#endif

enum {
    CONTEXT_ATOMIC,
#if APRINTER_HAVE_INTR_CONTEXT
    CONTEXT_INTERRUPT,
#endif
    CONTEXT_NORMAL
};

template <typename ThisContext>
class GetContextType {
private:
    AMBRO_DECLARE_HAS_MEMBER_TYPE_FUNC(HasAtomicContextTag, AtomicContextTag)
#if APRINTER_HAVE_INTR_CONTEXT
    AMBRO_DECLARE_HAS_MEMBER_TYPE_FUNC(HasInterruptContextTag, InterruptContextTag)
#endif
    
public:
    static int const Value =
        FuncCall<HasAtomicContextTag, ThisContext>::Value ? CONTEXT_ATOMIC :
#if APRINTER_HAVE_INTR_CONTEXT
        FuncCall<HasInterruptContextTag, ThisContext>::Value ? CONTEXT_INTERRUPT :
#endif
        CONTEXT_NORMAL;
};

class InterruptLockImpl {
private:
    template <typename ThisContext, int ContextType, typename Dummy = void>
    struct LockHelper;
    
    template <typename ThisContext, typename Dummy>
    struct LockHelper<ThisContext, CONTEXT_ATOMIC, Dummy> {
        using EnterContext = ThisContext;
        inline static EnterContext makeContext (ThisContext c) { return c; }
        inline static void enterLock (ThisContext c) {}
        inline static void exitLock (ThisContext c) {}
    };
    
#if APRINTER_HAVE_INTR_CONTEXT
    template <typename ThisContext, typename Dummy>
    struct LockHelper<ThisContext, CONTEXT_INTERRUPT, Dummy> {
        using EnterContext = AtomicContext<typename ThisContext::ParentContext>;
        inline static EnterContext makeContext (ThisContext c) { return MakeAtomicContext<typename ThisContext::ParentContext>(c); }
        inline static void enterLock (ThisContext c) { cli(); }
        inline static void exitLock (ThisContext c) { sei(); }
    };
#endif
    
    template <typename ThisContext, typename Dummy>
    struct LockHelper<ThisContext, CONTEXT_NORMAL, Dummy> {
        using EnterContext = AtomicContext<ThisContext>;
        inline static EnterContext makeContext (ThisContext c) { return MakeAtomicContext(c); }
        inline static void enterLock (ThisContext c) { cli(); }
        inline static void exitLock (ThisContext c) { sei(); }
    };
    
public:
    template <typename ThisContext>
    using EnterContext = typename LockHelper<ThisContext, GetContextType<ThisContext>::Value>::EnterContext;
    
    template <typename ThisContext>
    inline static EnterContext<ThisContext> makeContext (ThisContext c)
    {
        return LockHelper<ThisContext, GetContextType<ThisContext>::Value>::makeContext(c);
    }
    
    template <typename ThisContext>
    inline static void enterLock (ThisContext c)
    {
        return LockHelper<ThisContext, GetContextType<ThisContext>::Value>::enterLock(c);
    }
    
    template <typename ThisContext>
    inline static void exitLock (ThisContext c)
    {
        return LockHelper<ThisContext, GetContextType<ThisContext>::Value>::exitLock(c);
    }
};

InterruptLockImpl InterruptTempLock ()
{
    return InterruptLockImpl();
}

}

#endif
