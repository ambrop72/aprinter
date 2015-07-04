/*
 * Copyright (c) 2014 Ambroz Bizjak
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

#ifndef AMBROLIB_CONSTEXPR_HASH_H
#define AMBROLIB_CONSTEXPR_HASH_H

#include <stdint.h>

#include <aprinter/BeginNamespace.h>

template <typename TheHash>
class ConstexprHash {
public:
    using Type = typename TheHash::Type;
    
    constexpr ConstexprHash ()
    : m_accum(0) {}
    
    constexpr Type end () const
    {
        return m_accum;
    }
    
    constexpr ConstexprHash addString (char const *buf, size_t len) const
    {
        return (len == 0) ? *this : ConstexprHash(TheHash::hash(m_accum, (uint8_t)*buf)).addString(buf + 1, len - 1);
    }
    
    constexpr ConstexprHash addUint8 (uint8_t x) const
    {
        return ConstexprHash(TheHash::hash(m_accum, (uint8_t)x));
    }
    
    constexpr ConstexprHash addUint16 (uint16_t x) const
    {
        return ConstexprHash(TheHash::hash(TheHash::hash(m_accum, (uint8_t)(x >> 0)), (uint8_t)(x >> 8)));
    }
    
    constexpr ConstexprHash addUint32 (uint32_t x) const
    {
        return ConstexprHash(TheHash::hash(TheHash::hash(TheHash::hash(TheHash::hash(m_accum, (uint8_t)(x >> 0)), (uint8_t)(x >> 8)), (uint8_t)(x >> 16)), (uint8_t)(x >> 24)));
    }
    
private:
    constexpr ConstexprHash (Type accum)
    : m_accum(accum) {}
    
    Type m_accum;
};

#include <aprinter/EndNamespace.h>

#endif
