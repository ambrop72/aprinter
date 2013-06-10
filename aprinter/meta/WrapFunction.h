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

#ifndef AMBROLIB_WRAP_FUNCTION_H
#define AMBROLIB_WRAP_FUNCTION_H

#include <aprinter/BeginNamespace.h>

namespace WrapFunctionPrivate {
    template <typename R, typename... Args>
    struct Helper {
        template <R (*Func) (Args...)>
        struct Wrapper {
            static R call (Args... args)
            {
                return Func(args...);
            }
        };
    };

    template <typename R, typename... Args>
    struct Helper<R, Args...> MakeHelper (R (*func) (Args...));
}

#define AMBRO_WFUNC(func) decltype(APrinter::WrapFunctionPrivate::MakeHelper(func))::Wrapper<func>
#define AMBRO_WFUNC_T(func) typename decltype(APrinter::WrapFunctionPrivate::MakeHelper(func))::template Wrapper<func>
#define AMBRO_WFUNC_TD(func) decltype(APrinter::WrapFunctionPrivate::MakeHelper(func))::template Wrapper<func>

#include <aprinter/EndNamespace.h>

#endif
