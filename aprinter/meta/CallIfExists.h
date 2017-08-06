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

#ifndef APRINTER_CALL_IF_EXISTS_H
#define APRINTER_CALL_IF_EXISTS_H

#include <aprinter/base/Hints.h>

namespace APrinter {

template <typename X, typename Y>
using CallIfExists__TypeHelper = X;

#define APRINTER_DEFINE_CALL_IF_EXISTS(ClassName, func_name) \
class ClassName { \
public: \
    template <typename CallIfExists__Class, typename... CallIfExists__Args> \
    AMBRO_ALWAYS_INLINE \
    static void call_void (CallIfExists__Args... args) \
    { \
        return decltype(CallIfExists__void_helper_func<CallIfExists__Class>(0, args...))::template CallIfExists__call<CallIfExists__Class>(args...); \
    } \
    \
    template <typename CallIfExists__Class, typename CallIfExists__ReturnType=void, CallIfExists__ReturnType CallIfExists__DefaultValue=CallIfExists__ReturnType(), typename... CallIfExists__Args> \
    AMBRO_ALWAYS_INLINE \
    static CallIfExists__ReturnType call_ret (CallIfExists__Args... args) \
    { \
        return decltype(CallIfExists__ret_helper_func<CallIfExists__Class, CallIfExists__ReturnType, CallIfExists__DefaultValue>(0, args...))::template CallIfExists__call<CallIfExists__Class, CallIfExists__ReturnType, CallIfExists__DefaultValue>(args...); \
    } \
    \
private: \
    struct CallIfExists__VoidHelperCall { \
        template <typename CallIfExists__Class, typename... CallIfExists__Args> \
        AMBRO_ALWAYS_INLINE \
        static void CallIfExists__call (CallIfExists__Args... args) \
        { \
            return CallIfExists__Class::func_name(args...); \
        } \
    }; \
    \
    struct CallIfExists__VoidHelperNoCall { \
        template <typename CallIfExists__Class, typename... CallIfExists__Args> \
        AMBRO_ALWAYS_INLINE \
        static void CallIfExists__call (CallIfExists__Args... args) \
        { \
        } \
    }; \
    \
    template <typename CallIfExists__Class, typename... CallIfExists__Args> \
    static auto CallIfExists__void_helper_func (int, CallIfExists__Args... args) -> APrinter::CallIfExists__TypeHelper<CallIfExists__VoidHelperCall, decltype(CallIfExists__Class::func_name(args...))>; \
    \
    template <typename CallIfExists__Class> \
    static auto CallIfExists__void_helper_func (...) -> CallIfExists__VoidHelperNoCall; \
    \
    struct CallIfExists__RetHelperCall { \
        template <typename CallIfExists__Class, typename CallIfExists__ReturnType, CallIfExists__ReturnType CallIfExists__DefaultValue, typename... CallIfExists__Args> \
        AMBRO_ALWAYS_INLINE \
        static CallIfExists__ReturnType CallIfExists__call (CallIfExists__Args... args) \
        { \
            return CallIfExists__Class::func_name(args...); \
        } \
    }; \
    \
    struct CallIfExists__RetHelperNoCall { \
        template <typename CallIfExists__Class, typename CallIfExists__ReturnType, CallIfExists__ReturnType CallIfExists__DefaultValue, typename... CallIfExists__Args> \
        AMBRO_ALWAYS_INLINE \
        static CallIfExists__ReturnType CallIfExists__call (CallIfExists__Args... args) \
        { \
            return CallIfExists__DefaultValue; \
        } \
    }; \
    \
    template <typename CallIfExists__Class, typename CallIfExists__ReturnType, CallIfExists__ReturnType CallIfExists__DefaultValue, typename... CallIfExists__Args> \
    static auto CallIfExists__ret_helper_func (int, CallIfExists__Args... args) -> APrinter::CallIfExists__TypeHelper<CallIfExists__RetHelperCall, decltype(CallIfExists__Class::func_name(args...))>;  \
    \
    template <typename CallIfExists__Class, typename CallIfExists__ReturnType, CallIfExists__ReturnType CallIfExists__DefaultValue> \
    static auto CallIfExists__ret_helper_func (...) -> CallIfExists__RetHelperNoCall; \
};

}

#endif
