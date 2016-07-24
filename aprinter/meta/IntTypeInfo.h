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

#ifndef AMBROLIB_INT_TYPE_INFO_H
#define AMBROLIB_INT_TYPE_INFO_H

#include <stdint.h>

#include <aprinter/meta/BasicMetaUtils.h>

#include <aprinter/BeginNamespace.h>

template <int TNumBits, bool TSigned>
struct IntTypeInfoHelper {
    static const int NumBits = TNumBits;
    static const bool Signed = TSigned;
};

template <typename T>
static constexpr int IntTypeInfo_NumBits (T op)
{
    return (op == 0) ? 0 : (1 + IntTypeInfo_NumBits(op / 2));
}

#define DEFINE_UNSIGNED_TYPEINFO(type)\
auto IntTypeInfoFunc(WrapType<type>) -> IntTypeInfoHelper<IntTypeInfo_NumBits((type)-1), false>;

#define DEFINE_TYPEINFO(type, utype)\
auto IntTypeInfoFunc(WrapType<type>) -> IntTypeInfoHelper<IntTypeInfo_NumBits((utype)-1), ((type)-1 < 0)>;

DEFINE_UNSIGNED_TYPEINFO(uint8_t)
DEFINE_UNSIGNED_TYPEINFO(uint16_t)
DEFINE_UNSIGNED_TYPEINFO(uint32_t)
DEFINE_UNSIGNED_TYPEINFO(uint64_t)
DEFINE_UNSIGNED_TYPEINFO(unsigned char)
DEFINE_UNSIGNED_TYPEINFO(unsigned short int)
DEFINE_UNSIGNED_TYPEINFO(unsigned int)
DEFINE_UNSIGNED_TYPEINFO(unsigned long int)
DEFINE_UNSIGNED_TYPEINFO(unsigned long long int)
DEFINE_UNSIGNED_TYPEINFO(size_t)

DEFINE_TYPEINFO(int8_t, uint8_t)
DEFINE_TYPEINFO(int16_t, uint16_t)
DEFINE_TYPEINFO(int32_t, uint32_t)
DEFINE_TYPEINFO(int64_t, uint64_t)
DEFINE_TYPEINFO(signed char, unsigned char)
DEFINE_TYPEINFO(short int, unsigned short int)
DEFINE_TYPEINFO(int, unsigned int)
DEFINE_TYPEINFO(long int, unsigned long int)
DEFINE_TYPEINFO(long long int, unsigned long long int)

#ifdef AMBROLIB_AVR
DEFINE_UNSIGNED_TYPEINFO(__uint24)
DEFINE_TYPEINFO(__int24, __uint24)
#endif

#undef DEFINE_UNSIGNED_TYPEINFO
#undef DEFINE_TYPEINFO

template <typename T>
using IntTypeInfo = decltype(IntTypeInfoFunc(WrapType<T>()));

#include <aprinter/EndNamespace.h>

#endif
