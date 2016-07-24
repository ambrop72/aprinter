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

#ifndef APRINTER_IPSTACK_BUF_H
#define APRINTER_IPSTACK_BUF_H

#include <stddef.h>
#include <string.h>

#include <aprinter/meta/MinMax.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Hints.h>

#include <aprinter/BeginNamespace.h>

struct IpBufNode {
    char *ptr;
    size_t len;
    IpBufNode const *next;
};

struct IpBufRef {
    IpBufNode const *node;
    size_t offset;
    size_t tot_len;
    
    inline size_t getTotalLength () const
    {
        return tot_len;
    }
    
    inline char * getChunkPtr () const
    {
        AMBRO_ASSERT(node != nullptr)
        AMBRO_ASSERT(offset <= node->len)
        
        return node->ptr + offset;
    }
    
    inline size_t getChunkLength () const
    {
        AMBRO_ASSERT(node != nullptr)
        AMBRO_ASSERT(offset <= node->len)
        
        return MinValue(tot_len, (size_t)(node->len - offset));
    }
    
    inline bool nextChunk ()
    {
        AMBRO_ASSERT(node != nullptr)
        AMBRO_ASSERT(offset <= node->len)
        
        tot_len -= MinValue(tot_len, (size_t)(node->len - offset));
        node = node->next;
        offset = 0;
        
        bool more = (tot_len > 0);
        AMBRO_ASSERT(!more || node != nullptr)
        
        return more;
    }
    
    inline bool revealHeader (size_t amount, IpBufRef *new_ref) const
    {
        if (amount > offset) {
            return false;
        }
        
        *new_ref = IpBufRef {
            node,
            (size_t)(offset  - amount),
            (size_t)(tot_len + amount)
        };
        return true;
    }
    
    inline bool hasHeader (size_t amount) const
    {
        return getChunkLength() >= amount;
    }
    
    inline IpBufRef hideHeader (size_t amount) const
    {
        AMBRO_ASSERT(node != nullptr)
        AMBRO_ASSERT(offset <= node->len)
        AMBRO_ASSERT(amount <= node->len - offset)
        AMBRO_ASSERT(amount <= tot_len)
        
        return IpBufRef {
            node,
            (size_t)(offset  + amount),
            (size_t)(tot_len - amount)
        };
    }
    
    inline IpBufNode toNode () const
    {
        AMBRO_ASSERT(node != nullptr)
        AMBRO_ASSERT(offset <= node->len)
        
        return IpBufNode {
            node->ptr + offset,
            (size_t)(node->len - offset),
            node->next
        };
    }
    
    inline void skipBytes (size_t amount)
    {
        processBytes(amount, [](char *, size_t) {});
    }
    
    inline void takeBytes (size_t amount, char *dst)
    {
        processBytes(amount, [&](char *data, size_t len) {
            memcpy(dst, data, len);
            dst += len;
        });
    }
    
    inline size_t advanceToData ()
    {
        AMBRO_ASSERT(tot_len > 0)
        
        while (true) {
            size_t chunk_len = getChunkLength();
            if (AMBRO_LIKELY(chunk_len > 0)) {
                return chunk_len;
            }
            bool ok = nextChunk();
            AMBRO_ASSERT(ok)
        }
    }
    
    inline char takeByte ()
    {
        AMBRO_ASSERT(tot_len > 0)
        
        advanceToData();
        char ch = *getChunkPtr();
        offset++;
        tot_len--;
        return ch;
    }
    
    template <typename Func>
    inline void processBytes (size_t amount, Func func)
    {
        AMBRO_ASSERT(amount <= tot_len)
        
        while (amount > 0) {
            size_t chunk_len = advanceToData();
            size_t chunk_amount = MinValue(chunk_len, amount);
            
            func(getChunkPtr(), chunk_amount);
            
            offset += chunk_amount;
            tot_len -= chunk_amount;
            amount -= chunk_amount;
        }
    }
    
    inline IpBufRef subTo (size_t new_tot_len) const
    {
        AMBRO_ASSERT(new_tot_len <= tot_len)
        
        return IpBufRef {
            node,
            offset,
            new_tot_len
        };
    }
    
    static constexpr inline IpBufRef NullRef ()
    {
        return IpBufRef {nullptr, 0, 0};
    }
};

class StackBufAllocator {
public:
    template <size_t MaxSize>
    class Allocation {
    public:
        inline Allocation(size_t size)
        : m_size(size)
        {
            AMBRO_ASSERT(size <= MaxSize)
        }
        
        inline char * getPtr ()
        {
            return m_data;
        }
        
        inline size_t getSize ()
        {
            return m_size;
        }
        
    private:
        size_t m_size;
        char m_data[MaxSize];
    };
};

template <typename Allocator, size_t MaxSize, size_t HeaderBefore>
class TxAllocHelper {
public:
    inline TxAllocHelper (size_t size)
    : m_alloc(HeaderBefore + size)
    {
        m_node = IpBufNode {
            m_alloc.getPtr(),
            m_alloc.getSize(),
            nullptr
        };
        m_tot_len = size;
    }
    
    inline char * getPtr ()
    {
        return m_alloc.getPtr() + HeaderBefore;
    }
    
    inline void setNext (IpBufNode const *next_node, size_t next_len)
    {
        AMBRO_ASSERT(m_node.next == nullptr)
        AMBRO_ASSERT(m_tot_len == m_alloc.getSize() - HeaderBefore)
        AMBRO_ASSERT(next_node != nullptr)
        
        m_node.next = next_node;
        m_tot_len += next_len;
    }
    
    inline IpBufRef getBufRef ()
    {
        return IpBufRef {
            &m_node,
            HeaderBefore,
            m_tot_len
        };
    }
    
private:
    IpBufNode m_node;
    size_t m_tot_len;
    typename Allocator::template Allocation<HeaderBefore + MaxSize> m_alloc;
};

#include <aprinter/EndNamespace.h>

#endif
