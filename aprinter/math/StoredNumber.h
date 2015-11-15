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

#ifndef AMBROLIB_STORED_NUMBER_H
#define AMBROLIB_STORED_NUMBER_H

#include <stdint.h>

#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/EnableIf.h>
#include <aprinter/base/Inline.h>

#include <aprinter/BeginNamespace.h>

template <int NumBits, bool Signed, typename Dummy = void>
class StoredNumber {
public:
    using IntType = ChooseInt<NumBits, Signed>;
    
    AMBRO_ALWAYS_INLINE
    static StoredNumber store (IntType value)
    {
        StoredNumber res;
        res.m_value = value;
        return res;
    }
    
    AMBRO_ALWAYS_INLINE
    static IntType retrieve (StoredNumber inp)
    {
        return inp.m_value;
    }
    
    IntType m_value;
};

#if defined(AMBROLIB_AVR)

template <int NumBits>
class StoredNumber<NumBits, false, EnableIf<(NumBits >= 24 && NumBits < 32), void>> {
public:
    using ThisClass = StoredNumber<NumBits, false, EnableIf<(NumBits >= 24 && NumBits < 32), void>>;
    using IntType = ChooseInt<NumBits, false>;
    
    AMBRO_ALWAYS_INLINE
    static ThisClass store (IntType value)
    {
        ThisClass res;
        res.m_value[0] = value >> 0;
        res.m_value[1] = value >> 8;
        res.m_value[2] = value >> 16;
        return res;
    }
    
    AMBRO_ALWAYS_INLINE
    static IntType retrieve (ThisClass inp)
    {
        union {
            IntType x;
            uint8_t bytes[4];
        } u;
        u.bytes[0] = inp.m_value[0];
        u.bytes[1] = inp.m_value[1];
        u.bytes[2] = inp.m_value[2];
        u.bytes[3] = 0;
        return u.x;
    }
    
    uint8_t m_value[3];
};

#endif

#include <aprinter/EndNamespace.h>

#endif
