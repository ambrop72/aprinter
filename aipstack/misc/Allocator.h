/*
 * Copyright (c) 2017 Ambroz Bizjak
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

#ifndef AIPSTACK_ALLOCATOR_H
#define AIPSTACK_ALLOCATOR_H

#include <stddef.h>

#include <aprinter/base/Assert.h>

#include <aipstack/misc/Buf.h>

#include <aipstack/BeginNamespace.h>

class StackBufAllocator {
public:
    template <size_t MaxSize_>
    class Allocation {
    public:
        static size_t const MaxSize = MaxSize_;
        
        inline char * getPtr ()
        {
            return m_data;
        }
        
    private:
        char m_data[MaxSize];
    };
};

template <typename Allocator, size_t MaxSize, size_t HeaderBefore>
class TxAllocHelper {
    static size_t const TotalMaxSize = HeaderBefore + MaxSize;
    using Allocation = typename Allocator::template Allocation<TotalMaxSize>;
    
public:
    inline TxAllocHelper (size_t size)
    {
        AMBRO_ASSERT(size <= MaxSize)
        
        m_node = IpBufNode{m_alloc.getPtr(), size_t(HeaderBefore + size), nullptr};
        m_tot_len = size;
    }
    
    inline void reset (size_t size)
    {
        AMBRO_ASSERT(size <= MaxSize)
        AMBRO_ASSERT(m_node.ptr == m_alloc.getPtr())
        
        m_node.len = HeaderBefore + size;
        m_node.next = nullptr;
        m_tot_len = size;
    }
    
    inline char * getPtr ()
    {
        return m_alloc.getPtr() + HeaderBefore;
    }
    
    inline void changeSize (size_t size)
    {
        AMBRO_ASSERT(m_node.next == nullptr)
        AMBRO_ASSERT(size <= MaxSize)
        
        m_node.len = HeaderBefore + size;
        m_tot_len = size;
    }
    
    inline void setNext (IpBufNode const *next_node, size_t next_len)
    {
        AMBRO_ASSERT(m_node.next == nullptr)
        AMBRO_ASSERT(m_node.len == HeaderBefore + m_tot_len)
        AMBRO_ASSERT(next_node != nullptr)
        
        m_node.next = next_node;
        m_tot_len += next_len;
    }
    
    inline IpBufRef getBufRef ()
    {
        return IpBufRef{&m_node, HeaderBefore, m_tot_len};
    }
    
private:
    IpBufNode m_node;
    size_t m_tot_len;
    typename Allocator::template Allocation<HeaderBefore + MaxSize> m_alloc;
};

#include <aipstack/EndNamespace.h>

#endif
