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

#ifndef AMBROLIB_CONDITIONAL_FUNC_H
#define AMBROLIB_CONDITIONAL_FUNC_H

#include <aprinter/BeginNamespace.h>

namespace ConditionalFuncPrivate {
    template <bool Condition, typename TrueFunc, typename FalseFunc>
    struct Helper;
    
    template <typename TrueFunc, typename FalseFunc>
    struct Helper<true, TrueFunc, FalseFunc> {
        template <typename X>
        struct Call {
            typedef typename TrueFunc::template Call<X>::Type Type;
        };
    };

    template <typename TrueFunc, typename FalseFunc>
    struct Helper<false, TrueFunc, FalseFunc> {
        template <typename X>
        struct Call {
            typedef typename FalseFunc::template Call<X>::Type Type;
        };
    };
}

template <typename ConditionFunc, typename TrueFunc, typename FalseFunc>
struct ConditionalFunc {
    template <typename X>
    struct Call {
        typedef typename ConditionalFuncPrivate::Helper<
            ConditionFunc::template Call<X>::Type::value,
            TrueFunc,
            FalseFunc
        >::template Call<X>::Type Type;
    };
};

#include <aprinter/EndNamespace.h>

#endif
