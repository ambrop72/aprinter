/*
 * Copyright (c) 2018 Ambroz Bizjak
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

#ifndef APRINTER_VECTOR2_H
#define APRINTER_VECTOR2_H

#include <aprinter/math/FloatTools.h>

namespace APrinter {

template <typename FpType>
class Vector2 {
public:
    static Vector2 make (FpType x, FpType y)
    {
        Vector2 res;
        res.m_v[0] = x;
        res.m_v[1] = y;
        return res;
    }
    
    FpType squaredLength () const
    {
        return (m_v[0] * m_v[0]) + (m_v[1] * m_v[1]);
    }
    
    FpType length () const
    {
        return FloatSqrt(squaredLength());
    }
    
    Vector2 operator* (FpType s) const
    {
        return make(m_v[0] * s, m_v[1] * s);
    }
    
    Vector2 operator+ (Vector2 other) const
    {
        return make(m_v[0] + other.m_v[0], m_v[1] + other.m_v[1]);
    }
    
    Vector2 operator- (Vector2 other) const
    {
        return make(m_v[0] - other.m_v[0], m_v[1] - other.m_v[1]);
    }

    Vector2 rotate90DegCCW () const
    {
        return make(-m_v[1], m_v[0]);
    }
    
public:
    FpType m_v[2];
};

}

#endif
