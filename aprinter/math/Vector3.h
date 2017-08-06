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

#ifndef AMBROLIB_VECTOR3_H
#define AMBROLIB_VECTOR3_H

#include <aprinter/math/FloatTools.h>

namespace APrinter {

template <typename FpType>
class Vector3 {
public:
    static Vector3 make (FpType x, FpType y, FpType z)
    {
        Vector3 res;
        res.m_v[0] = x;
        res.m_v[1] = y;
        res.m_v[2] = z;
        return res;
    }
    
    FpType norm () const
    {
        return (m_v[0] * m_v[0]) + (m_v[1] * m_v[1]) + (m_v[2] * m_v[2]);
    }
    
    FpType length () const
    {
        return FloatSqrt(norm());
    }
    
    FpType dot (Vector3 other) const
    {
        return (m_v[0] * other.m_v[0]) + (m_v[1] * other.m_v[1]) + (m_v[2] * other.m_v[2]);
    }
    
    Vector3 operator* (FpType s) const
    {
        return Vector3::make(m_v[0] * s, m_v[1] * s, m_v[2] * s);
    }
    
    Vector3 operator+ (Vector3 other) const
    {
        return Vector3::make(m_v[0] + other.m_v[0], m_v[1] + other.m_v[1], m_v[2] + other.m_v[2]);
    }
    
    Vector3 operator- (Vector3 other) const
    {
        return Vector3::make(m_v[0] - other.m_v[0], m_v[1] - other.m_v[1], m_v[2] - other.m_v[2]);
    }
    
    Vector3 cross (Vector3 other) const
    {
        return Vector3::make(
            (m_v[1] * other.m_v[2]) - (m_v[2] * other.m_v[1]),
            (m_v[2] * other.m_v[0]) - (m_v[0] * other.m_v[2]),
            (m_v[0] * other.m_v[1]) - (m_v[1] * other.m_v[0])
        );
    }
    
public:
    FpType m_v[3];
};

}

#endif
