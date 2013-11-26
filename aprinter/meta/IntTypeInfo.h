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

#include <aprinter/BeginNamespace.h>

template <int TNumBits, bool TSigned>
struct IntTypeInfoHelper {
    static const int NumBits = TNumBits;
    static const bool Signed = TSigned;
};

template <typename IntType>
struct IntTypeInfo;

template <>
struct IntTypeInfo<uint8_t> : public IntTypeInfoHelper<8, false> {};
template <>
struct IntTypeInfo<uint16_t> : public IntTypeInfoHelper<16, false> {};
template <>
struct IntTypeInfo<uint32_t> : public IntTypeInfoHelper<32, false> {};
template <>
struct IntTypeInfo<uint64_t> : public IntTypeInfoHelper<64, false> {};
template <>
struct IntTypeInfo<int8_t> : public IntTypeInfoHelper<8, true> {};
template <>
struct IntTypeInfo<int16_t> : public IntTypeInfoHelper<16, true> {};
template <>
struct IntTypeInfo<int32_t> : public IntTypeInfoHelper<32, true> {};
template <>
struct IntTypeInfo<int64_t> : public IntTypeInfoHelper<64, true> {};

#ifdef AMBROLIB_AVR
template <>
struct IntTypeInfo<__uint24> : public IntTypeInfoHelper<24, false> {};
template <>
struct IntTypeInfo<__int24> : public IntTypeInfoHelper<24, true> {};
#endif

#include <aprinter/EndNamespace.h>

#endif
