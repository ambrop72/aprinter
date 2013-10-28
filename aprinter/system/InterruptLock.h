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

#include <aprinter/meta/HasMemberTypeFunc.h>

#include <aprinter/BeginNamespace.h>

template <typename Context>
struct InterruptContext : public Context {
    explicit InterruptContext (Context c) : Context(c) {}
    typedef void InterruptContextTag;
};

template <typename Context>
InterruptContext<Context> MakeInterruptContext (Context c)
{
    return InterruptContext<Context>(c);
}

template <typename Context>
class IsInterruptContext {
private:
    AMBRO_DECLARE_HAS_MEMBER_TYPE_FUNC(HasInterruptContextTag, InterruptContextTag)
    
public:
    static const bool value = HasInterruptContextTag::Call<Context>::Type::value;
};

class InterruptLockImpl {
private:
    template <typename ThisContext, bool InInterruptContext, typename Dummy = void>
    struct LockHelper;
    
public:
    template <typename ThisContext>
    using EnterContext = typename LockHelper<ThisContext, IsInterruptContext<ThisContext>::value>::EnterContext;
    
    template <typename ThisContext>
    inline static EnterContext<ThisContext> makeContext (ThisContext c)
    {
        return LockHelper<ThisContext, IsInterruptContext<ThisContext>::value>::makeContext(c);
    }
    
    template <typename ThisContext>
    inline static void enterLock (ThisContext c)
    {
        return LockHelper<ThisContext, IsInterruptContext<ThisContext>::value>::enterLock(c);
    }
    
    template <typename ThisContext>
    inline static void exitLock (ThisContext c)
    {
        return LockHelper<ThisContext, IsInterruptContext<ThisContext>::value>::exitLock(c);
    }
    
private:
    template <typename ThisContext, typename Dummy>
    struct LockHelper<ThisContext, false, Dummy> {
        typedef InterruptContext<ThisContext> EnterContext;
        
        inline static EnterContext makeContext (ThisContext c)
        {
            return MakeInterruptContext(c);
        }
        
        inline static void enterLock (ThisContext c)
        {
            cli();
        }
        
        inline static void exitLock (ThisContext c)
        {
            sei();
        }
    };
    
    template <typename ThisContext, typename Dummy>
    struct LockHelper<ThisContext, true, Dummy> {
        typedef ThisContext EnterContext;
        
        inline static EnterContext makeContext (ThisContext c)
        {
            return c;
        }
        
        inline static void enterLock (ThisContext c)
        {
        }
        
        inline static void exitLock (ThisContext c)
        {
        }
    };
};

template <typename Context>
class InterruptLock {
private:
public:
    template <typename ThisContext>
    void init (ThisContext c)
    {
    }
    
    template <typename ThisContext>
    void deinit (ThisContext c)
    {
    }
    
    template <typename ThisContext>
    using EnterContext = typename InterruptLockImpl::template EnterContext<ThisContext>;
    
    template <typename ThisContext>
    inline static EnterContext<ThisContext> makeContext (ThisContext c)
    {
        return InterruptLockImpl::makeContext(c);
    }
    
    template <typename ThisContext>
    inline static void enterLock (ThisContext c)
    {
        return InterruptLockImpl::enterLock(c);
    }
    
    template <typename ThisContext>
    inline static void exitLock (ThisContext c)
    {
        return InterruptLockImpl::exitLock(c);
    }
};

InterruptLockImpl InterruptTempLock ()
{
    return InterruptLockImpl();
}

#include <aprinter/EndNamespace.h>

#endif
