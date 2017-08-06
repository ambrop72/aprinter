/*
 * Copyright (c) 2016 Ambroz Bizjak
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

#ifndef APRINTER_LOOP_UTILS_H
#define APRINTER_LOOP_UTILS_H

namespace APrinter {

template <typename IntType>
class LoopRangeIter;

template <typename IntType>
class LoopRange {
public:
    inline LoopRange (IntType start, IntType end)
    : m_start(start), m_end(end)
    {}
    
    inline LoopRange (IntType end)
    : m_start(0), m_end(end)
    {}
    
    inline LoopRangeIter<IntType> begin () const
    {
        return LoopRangeIter<IntType>(m_start);
    }
    
    inline LoopRangeIter<IntType> end () const
    {
        return LoopRangeIter<IntType>(m_end);
    }
    
private:
    IntType m_start;
    IntType m_end;
};

template <typename IntType>
class LoopRangeIter {
public:
    inline LoopRangeIter (IntType value)
    : m_value(value)
    {}
    
    inline IntType operator* () const
    {
        return m_value;
    }
    
    inline bool operator!= (LoopRangeIter const &other) const
    {
        return m_value != other.m_value;
    }
    
    inline LoopRangeIter & operator++ ()
    {
        m_value++;
        return *this;
    }
    
private:
    IntType m_value;
};

template <typename IntType>
LoopRange<IntType> LoopRangeAuto (IntType start, IntType end)
{
    return LoopRange<IntType>(start, end);
}

template <typename IntType>
LoopRange<IntType> LoopRangeAuto (IntType end)
{
    return LoopRange<IntType>(end);
}

}

#endif
