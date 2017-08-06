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

#ifndef AMBROLIB_LOCK_H
#define AMBROLIB_LOCK_H

#include <aprinter/meta/BasicMetaUtils.h>

namespace APrinter {

/**
 * This is a utility class used for RAII-based locking.
 * 
 * It is normally used implicitly in the APRINTER_LOCK macro.
 * It relies on an abstract Mutex type which implements the
 * actual locking and unlocking.
 * 
 * \tparam ThisContext context type outside the lock
 * \tparam Mutex mutex type to use (class or class reference)
 */
template <typename ThisContext, typename Mutex>
class Lock {
    using MutexClass = RemoveReference<Mutex>;
    
public:
    /**
     * Context type inside the lock, and return type of lockContext.
     */
    using LockContext = typename MutexClass::template EnterContext<ThisContext>;
    
    /**
     * Constructor which locks the mutex.
     * 
     * The Lock stores the passed context and mutex.
     * The stored type for the mutex is Mutex, so if Mutex is a reference
     * then the lock stores the reference, otherwise it stores a copy.
     */
    inline Lock (ThisContext c, Mutex mutex)
    : m_c(c), m_mutex(mutex)
    {
        m_mutex.enterLock(m_c);
    }
    
    /**
     * Destructor which unlocks the mutex.
     */
    inline ~Lock ()
    {
        m_mutex.exitLock(m_c);
    }
    
    /**
     * Returns the lock context.
     */
    inline LockContext lockContext ()
    {
        return m_mutex.makeContext(m_c);
    }
    
    /**
     * Returns true.
     * 
     * This allows use with if, e.g.:
     *   if (Lock<...> lock{...}) { .. }
     * 
     * It is important to give the lock variable a name, or the lock
     * would be destructed before the body starts! Do NOT do this:
     *   if (Lock<...>{...}) { ... } // WRONG
     */
    inline operator bool ()
    {
        return true;
    }
    
    Lock (Lock const &) = delete;
    Lock & operator= (Lock const &) = delete;
    
private:
    ThisContext m_c;
    Mutex m_mutex;
};

/**
 * Macro for easily locking a mutex.
 * 
 * This creates a Lock object with ThisContext=decltype(this_context)
 * but with Lock=decltype((this_context)). As a result it can be used
 * both with lvalue mutexes and temporary dummy mutex objects:
 * 
 * 1. MyMutexType mutex; APRINTER_LOCK(mutex, ...) { ...}
 *    In this case the Lock's Mutex tupe will be MyMutexType&
 *    and the lock will store a reference to the mutex.
 * 
 * 2. APRINTER_LOCK(InterruptLock(), ...) { ... }
 *    In this case the Lock's Mutex type will be InterruptLock
 *    and the lock will store an InterruptLock object by value
 *    (presumably an empty class).
 * 
 * WARNING: Currently the user-specified body after the macro becomes the
 * body of a for-loop. Therefore break or continue must not be used within
 * the lock body. This might be fixed in the future with C++17 if-initializers.
 * 
 * \param mutex The mutex to lock.
 * \param this_context The context outside the lock.
 * \param lock_context Variable name for the context inside the lock.
 */
#define APRINTER_LOCK(mutex, this_context, lock_context) \
if (APrinter::Lock<decltype(this_context), decltype((mutex))> aprinter__lock_var{(this_context), (mutex)}) \
for (bool aprinter__lock_run = true; aprinter__lock_run;) \
for (auto lock_context = aprinter__lock_var.lockContext(); aprinter__lock_run; aprinter__lock_run = false)

// Compatibility macros.
#define AMBRO_LOCK(mutex, this_context, lock_context) APRINTER_LOCK(mutex, this_context, lock_context)
#define AMBRO_LOCK_T(mutex, this_context, lock_context) APRINTER_LOCK(mutex, this_context, lock_context)

}

#endif
