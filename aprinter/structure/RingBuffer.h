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

#ifndef AMBROLIB_RING_BUFFER_H
#define AMBROLIB_RING_BUFFER_H

#include <stddef.h>

#include <aprinter/meta/BoundedInt.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename Entry, int BufferBits>
class RingBuffer : private DebugObject<Context, RingBuffer<Context, Entry, BufferBits>> {
public:
    using SizeType = BoundedInt<BufferBits, false>;
    
    template <typename ThisContext>
    void init (ThisContext c)
    {
        m_start = SizeType::import(0);
        m_end = SizeType::import(0);
        
        this->debugInit(c);
    }
    
    template <typename ThisContext>
    void deinit (ThisContext c)
    {
        this->debugDeinit(c);
    }
    
    template <typename ThisContext>
    SizeType writerGetAvail (ThisContext c)
    {
        this->debugAccess(c);
        
        return calc_write_avail(m_start, m_end);
    }
    
    template <typename ThisContext>
    Entry * writerGetPtr (ThisContext c, SizeType index = SizeType::import(0))
    {
        this->debugAccess(c);
        AMBRO_ASSERT(index < calc_write_avail(m_start, m_end))
        
        return &m_entries[BoundedModuloAdd(m_end, index).value()];
    }
    
    template <typename ThisContext>
    void writerProvide (ThisContext c, SizeType amount = SizeType::import(1))
    {
        this->debugAccess(c);
        AMBRO_ASSERT(amount <= calc_write_avail(m_start, m_end))
        
        m_end = BoundedModuloAdd(m_end, amount);
    }
    
    template <typename ThisContext>
    Entry * writerGetPrevPtr (ThisContext c)
    {
        this->debugAccess(c);
        
        return &m_entries[BoundedModuloSubtract(m_end, 1).value()];
    }
    
    template <typename ThisContext>
    SizeType readerGetAvail (ThisContext c)
    {
        this->debugAccess(c);
        
        return calc_read_avail(m_start, m_end);
    }
    
    template <typename ThisContext>
    Entry * readerGetPtr (ThisContext c, SizeType index = SizeType::import(0))
    {
        this->debugAccess(c);
        AMBRO_ASSERT(index < calc_read_avail(m_start, m_end))
        
        return &m_entries[BoundedModuloAdd(m_start, index).value()];
    }
    
    template <typename ThisContext>
    void readerConsume (ThisContext c, SizeType amount = SizeType::import(1))
    {
        this->debugAccess(c);
        AMBRO_ASSERT(amount <= calc_read_avail(m_start, m_end))
        
        m_start = BoundedModuloAdd(m_start, amount);
    }
    
    template <typename ThisContext>
    void clear (ThisContext c)
    {
        this->debugAccess(c);
        
        m_start = SizeType::import(0);
        m_end = SizeType::import(0);
    }
    
private:
    static SizeType calc_write_avail (SizeType start, SizeType end)
    {
        return BoundedModuloSubtract(BoundedModuloSubtract(start, SizeType::import(1)), end);
    }
    
    static SizeType calc_read_avail (SizeType start, SizeType end)
    {
        return BoundedModuloSubtract(end, start);
    }
    
    SizeType m_start;
    SizeType m_end;
    Entry m_entries[(size_t)SizeType::maxValue() + 1];
};

#include <aprinter/EndNamespace.h>

#endif
