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

#ifndef AMBROLIB_CHOOSE_INT_H
#define AMBROLIB_CHOOSE_INT_H

#include <stdint.h>

#include <aprinter/meta/If.h>

#include <aprinter/BeginNamespace.h>

template <int NumBits, bool Signed>
class ChooseIntHelper {
public:
    static_assert(NumBits > 0, "");
    static_assert((!Signed || NumBits < 64), "Too many bits (signed)");
    static_assert((!!Signed || NumBits <= 64), "Too many bits (unsigned).");
    
    using Result =
        If<(Signed && NumBits < 8), int8_t,
        If<(Signed && NumBits < 16), int16_t,
        If<(Signed && NumBits < 32), int32_t,
        If<(Signed && NumBits < 64), int64_t,
        If<(!Signed && NumBits <= 8), uint8_t,
        If<(!Signed && NumBits <= 16), uint16_t,
        If<(!Signed && NumBits <= 32), uint32_t,
        If<(!Signed && NumBits <= 64), uint64_t,
        void>>>>>>>>;
};

template <int NumBits, bool Signed = false>
using ChooseInt = typename ChooseIntHelper<NumBits, Signed>::Result;

#include <aprinter/EndNamespace.h>

#endif
