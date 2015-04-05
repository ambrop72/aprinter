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

#ifndef APRINTER_CALLBACK_H
#define APRINTER_CALLBACK_H

#include <aprinter/BeginNamespace.h>

template <typename>
class Callback;

template <typename R, typename... Args>
class Callback<R(Args...)> {
public:
    R operator() (Args... args) const
    {
        return m_func(m_arg, args...);
    }
    
    operator bool () const
    {
        return m_func;
    }
    
public:
    R (*m_func) (void *, Args... args);
    void *m_arg;
};

namespace CallbackPrivate {
    template <typename Obj, typename R, typename... Args>
    struct MakeObj {
        template <R (Obj::*Func) (Args...)>
        struct WithFunc {
            static Callback<R(Args...)> MakeCallback (Obj *obj)
            {
                return Callback<R(Args...)>{WithFunc::_callback_func, obj};
            }
            
            static R _callback_func (void *obj, Args... args)
            {
                Obj *o = static_cast<Obj *>(obj);
                return (o->*Func)(args...);
            }
        };
    };

    template <typename Obj, typename R, typename... Args>
    MakeObj<Obj, R, Args...> MakeObjHelper (R (Obj::*func) (Args...));
    
    template <typename R, typename... Args>
    struct MakeStat {
        template <R (*Func) (Args...)>
        struct WithFunc {
            static Callback<R(Args...)> MakeCallback ()
            {
                return Callback<R(Args...)>{WithFunc::_callback_func, nullptr};
            }
            
            static R _callback_func (void *, Args... args)
            {
                return (*Func)(args...);
            }
        };
    };
    
    template <typename R, typename... Args>
    MakeStat<R, Args...> MakeStatHelper (R (*func) (Args...));
}

#define APRINTER_CB_OBJFUNC(func, obj) (decltype(APrinter::CallbackPrivate::MakeObjHelper(func))::WithFunc<func>::MakeCallback(obj))
#define APRINTER_CB_OBJFUNC_T(func, obj) (decltype(APrinter::CallbackPrivate::MakeObjHelper(func))::template WithFunc<func>::MakeCallback(obj))

#define APRINTER_CB_STATFUNC(func) (decltype(APrinter::CallbackPrivate::MakeStatHelper(func))::WithFunc<func>::MakeCallback())
#define APRINTER_CB_STATFUNC_T(func) (decltype(APrinter::CallbackPrivate::MakeStatHelper(func))::template WithFunc<func>::MakeCallback())

#include <aprinter/EndNamespace.h>

#endif
