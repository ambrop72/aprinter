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

#ifndef AMBROLIB_INT_TYPE_INFO
#define AMBROLIB_INT_TYPE_INFO

#include <stdint.h>

#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/meta/If.h>

#include <aprinter/BeginNamespace.h>

template <typename T>
struct IntTypeInfo {
    static_assert(
        TypesAreEqual<T, int8_t>::value || 
        TypesAreEqual<T, int16_t>::value || 
        TypesAreEqual<T, int32_t>::value || 
        TypesAreEqual<T, int64_t>::value ||
        TypesAreEqual<T, uint8_t>::value ||
        TypesAreEqual<T, uint16_t>::value ||
        TypesAreEqual<T, uint32_t>::value ||
        TypesAreEqual<T, uint64_t>::value, "");
    
    static const bool is_signed =
        TypesAreEqual<T, int8_t>::value ? true :
        TypesAreEqual<T, int16_t>::value ? true :
        TypesAreEqual<T, int32_t>::value ? true :
        TypesAreEqual<T, int64_t>::value ? true :
        TypesAreEqual<T, uint8_t>::value ? false :
        TypesAreEqual<T, uint16_t>::value ? false :
        TypesAreEqual<T, uint32_t>::value ? false :
        TypesAreEqual<T, uint64_t>::value ? false : false;
    
    static const int nonsign_bits =
        TypesAreEqual<T, int8_t>::value ? 7 :
        TypesAreEqual<T, int16_t>::value ? 15 :
        TypesAreEqual<T, int32_t>::value ? 31 :
        TypesAreEqual<T, int64_t>::value ? 63 :
        TypesAreEqual<T, uint8_t>::value ? 8 :
        TypesAreEqual<T, uint16_t>::value ? 16 :
        TypesAreEqual<T, uint32_t>::value ? 32 :
        TypesAreEqual<T, uint64_t>::value ? 64 : 0;
    
    typedef
        If<TypesAreEqual<T, int8_t>::value, int16_t,
        If<TypesAreEqual<T, int16_t>::value, int32_t,
        If<TypesAreEqual<T, int32_t>::value, int64_t,
        If<TypesAreEqual<T, int64_t>::value, void,
        If<TypesAreEqual<T, uint8_t>::value, uint16_t,
        If<TypesAreEqual<T, uint16_t>::value, uint32_t,
        If<TypesAreEqual<T, uint32_t>::value, uint64_t,
        If<TypesAreEqual<T, uint64_t>::value, void,
        void>>>>>>>> NextType;
    
    typedef
        If<TypesAreEqual<T, int8_t>::value, void,
        If<TypesAreEqual<T, int16_t>::value, int8_t,
        If<TypesAreEqual<T, int32_t>::value, int16_t,
        If<TypesAreEqual<T, int64_t>::value, int32_t,
        If<TypesAreEqual<T, uint8_t>::value, void,
        If<TypesAreEqual<T, uint16_t>::value, uint8_t,
        If<TypesAreEqual<T, uint32_t>::value, uint16_t,
        If<TypesAreEqual<T, uint64_t>::value, uint32_t,
        void>>>>>>>> PrevType;
    
    typedef
        If<TypesAreEqual<T, int8_t>::value, uint8_t,
        If<TypesAreEqual<T, int16_t>::value, uint16_t,
        If<TypesAreEqual<T, int32_t>::value, uint32_t,
        If<TypesAreEqual<T, int64_t>::value, uint64_t,
        If<TypesAreEqual<T, uint8_t>::value, uint8_t,
        If<TypesAreEqual<T, uint16_t>::value, uint16_t,
        If<TypesAreEqual<T, uint32_t>::value, uint32_t,
        If<TypesAreEqual<T, uint64_t>::value, uint64_t,
        void>>>>>>>> UnsignedType;
};

#include <aprinter/EndNamespace.h>

#endif
