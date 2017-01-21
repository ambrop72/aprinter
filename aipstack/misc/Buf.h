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

#include <aipstack/BeginNamespace.h>

/**
 * Node in a chain of memory buffers.
 * 
 * It contains the pointer to and length of a buffer,
 * and a pointer to the next node, if any.
 */
struct IpBufNode {
    /**
     * Pointer to the buffer data.
     */
    char *ptr;
    
    /**
     * Length of the buffer.
     */
    size_t len;
    
    /**
     * Pointer to the next buffer node (or NULL).
     */
    IpBufNode const *next;
};

/**
 * Reference to a possibly discontiguous range of memory in
 * a chain of memory buffers.
 * 
 * It contains the pointer to the first buffer node,
 * the byte offset within that first buffer, and the total
 * length of the memory range.
 * 
 * Except where noted otherwise, all functions in IpBufRef
 * require the reference to be *valid*. This means that
 * node is not NULL, the offset points to a valid location
 * within this first buffer (pointing to the end is permitted),
 * and there is at least tot_len remaining data in the first
 * buffer and subsequent buffers together.
 * 
 * Operations with these memory ranges never modify the
 * buffer nodes (IpBufNode). Only the IpBufRef objects are
 * changed or created to refer to different ranges of a
 * buffer chain.
 */
struct IpBufRef {
    /**
     * Pointer to the first buffer node.
     */
    IpBufNode const *node;
    
    /**
     * Byte offset in the first buffer.
     */
    size_t offset;
    
    /**
     * The total length of the data range.
     */
    size_t tot_len;
    
    /**
     * Returns tot_len.
     * 
     * A valid reference is not needed, this simply returns tot_len
     * without any requirements.
     */
    inline size_t getTotalLength () const
    {
        return tot_len;
    }
    
    /**
     * Returns the pointer to the first chunk of the memory range.
     * 
     * This returns node->ptr + offset.
     */
    inline char * getChunkPtr () const
    {
        AMBRO_ASSERT(node != nullptr)
        AMBRO_ASSERT(offset <= node->len)
        
        return node->ptr + offset;
    }
    
    /**
     * Returns the length of the first chunk of the memory range.
     * 
     * This returns min(tot_len, node->len - offset).
     */
    inline size_t getChunkLength () const
    {
        AMBRO_ASSERT(node != nullptr)
        AMBRO_ASSERT(offset <= node->len)
        
        return APrinter::MinValue(tot_len, (size_t)(node->len - offset));
    }
    
    /**
     * Move to the next buffer in the memory range.
     * 
     * This decrements tot_len by getChunkLength(), sets node to
     * node->next and sets offset to 0. After that it returns
     * whether there is any more data in the (now modified) memory
     * range, that is (tot_len > 0).
     */
    inline bool nextChunk ()
    {
        AMBRO_ASSERT(node != nullptr)
        AMBRO_ASSERT(offset <= node->len)
        
        tot_len -= APrinter::MinValue(tot_len, (size_t)(node->len - offset));
        node = node->next;
        offset = 0;
        
        bool more = (tot_len > 0);
        AMBRO_ASSERT(!more || node != nullptr)
        
        return more;
    }
    
    /**
     * Attempts to extend the memory range backward in the
     * first buffer.
     * 
     * If amount is greater than offset, returns false since
     * insufficient memory is available in the first buffer.
     * Otherwise, sets *new_ref to the memory region extended
     * to the left by amount and returns true. The *new_ref
     * will have the same node, offset decremented by amount
     * and tot_len incremented by amount.
     */
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
    
    /**
     * Checks if there is at least amount bytes available
     * in the first chunk of the memory range.
     * 
     * It returns (getChunkLength() >= amount).
     */
    inline bool hasHeader (size_t amount) const
    {
        return getChunkLength() >= amount;
    }
    
    /**
     * Returns a memory range with without an initial portion
     * of this memory range.
     * 
     * The amount must be lesser than or equal to getChunkLength().
     * It returns a reference with the same node, offset
     * incremented by amount and tot_len decremented by amount.
     */
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
    
    /**
     * Returns an IpBufNode corresponding for the first buffer
     * of the memory range with the offset applied.
     * 
     * It returns an IpBufNode with ptr equal to node->ptr
     * + offset, len equal to node->len - offset and next
     * equal to node->next.
     */
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
    
    /**
     * Creates a memory range consisting of an initial portion
     * of the first chunk of this memory range continued by
     * data in a specified buffer chain.
     * 
     * The header_len must be less than or equal to node->len
     * - offset and total_len must be greater than or equal to
     * header_len.
     * 
     * It sets *out_node to a new IpBufNode referencing the
     * initial portion and continuing into the given buffer
     * chain (ptr = node->ptr, len = offset + header_len,
     * next = cont), and returns an IpBufRef using out_node
     * as its first buffer (node = out_node, offset = offset,
     * tot_len = total_len).
     * 
     * Note that this does not "apply" the offset to the node
     * as toNode does. This is to allow revealHeader.
     *
     * It is important to understand that this works by creating
     * a new IpBufNode, because the buffer chain model cannot
     * support this operation otherwise. The returned IpBufRef
     * will be valid only so long as out_node remains valid.
     */
    inline IpBufRef subHeaderToContinuedBy (size_t header_len, IpBufNode const *cont,
                                            size_t total_len, IpBufNode *out_node) const
    {
        AMBRO_ASSERT(node != nullptr)
        AMBRO_ASSERT(offset <= node->len)
        AMBRO_ASSERT(header_len <= node->len - offset)
        AMBRO_ASSERT(total_len >= header_len)
        
        *out_node = IpBufNode{node->ptr, (size_t)(offset + header_len), cont};
        return IpBufRef{out_node, offset, total_len};
    }
    
    /**
     * Consume a number of bytes from the front of the memory
     * range.
     * 
     * The amount must not exceed tot_len.
     * 
     * This moves to subsequent buffers eagerly (see processBytes).
     */
    inline void skipBytes (size_t amount)
    {
        processBytes(amount, [](char *, size_t) {});
    }
    
    /**
     * Consume a number of bytes from the front of the memory
     * memory range while copying them to the given memory
     * location.
     * 
     * The amount must not exceed tot_len.
     * 
     * This moves to subsequent buffers eagerly (see processBytes).
     */
    inline void takeBytes (size_t amount, char *dst)
    {
        processBytes(amount, [&](char *data, size_t len) {
            ::memcpy(dst, data, len);
            dst += len;
        });
    }
    
    /**
     * Consume a number of bytes from the front of the memory
     * range while copying bytes from the given memory location
     * into the consumed part of the range.
     * 
     * The amount must not exceed tot_len.
     * 
     * This moves to subsequent buffers eagerly (see processBytes).
     */
    inline void giveBytes (size_t amount, char const *src)
    {
        processBytes(amount, [&](char *data, size_t len) {
            ::memcpy(data, src, len);
            src += len;
        });
    }
    
    /**
     * Consume a number of bytes from the front of the memory
     * range while copying bytes from another memory range
     * into the consumed part of the range.
     * 
     * The number of bytes consumed and copied is equal to
     * the length of the other memory range (src), and must
     * not exceed the length of this memory range.
     * 
     * This moves to subsequent buffers eagerly (see processBytes).
     */
    inline void giveBuf (IpBufRef src)
    {
        processBytes(src.tot_len, [&](char *data, size_t len) {
            src.takeBytes(len, data);
        });
    }
    
    /**
     * Consume and return a single byte from the front of the
     * memory range.
     * 
     * The tot_len must be positive.
     * 
     * This moves to subsequent buffers eagerly (see processBytes).
     */
    inline char takeByte ()
    {
        AMBRO_ASSERT(tot_len > 0)
        
        char ch;
        processBytes(1, [&](char *data, size_t len) {
            ch = *data;
        });
        return ch;
    }
    
    /**
     * Consume a number of bytes from the front of the memory
     * range while processing them with a user-specified function.
     * 
     * The function will be called on the subsequent contiguous
     * chunks of the consumed part of this memory range. It will
     * be passed two arguments: a char pointer to the start of
     * the chunk, and a size_t length of the chunk. The function
     * will not be called on zero-sized chunks.
     * 
     * This function moves forward to subsequent buffers eagerly.
     * This means that when there are no more bytes to be
     * processed, it will move to the next buffer as long as
     * it is currently at the end of the current buffer and there
     * is a next buffer.
     * 
     * This eager moving across buffer is useful when the buffer
     * chain is a ring buffer, so that the offset into the buffer
     * will remain always less than the buffer size, never becoming
     * equal.
     */
    template <typename Func>
    inline void processBytes (size_t amount, Func func)
    {
        AMBRO_ASSERT(node != nullptr)
        AMBRO_ASSERT(amount <= tot_len)
        
        while (true) {
            AMBRO_ASSERT(offset <= node->len)
            size_t rem_in_buf = node->len - offset;
            
            if (rem_in_buf > 0) {
                if (amount == 0) {
                    return;
                }
                
                size_t take = APrinter::MinValue(rem_in_buf, amount);
                func(getChunkPtr(), take);
                
                tot_len -= take;
                
                if (take < rem_in_buf || node->next == nullptr) {
                    offset += take;
                    AMBRO_ASSERT(amount == take)
                    return;
                }
                
                amount -= take;
            } else {
                if (node->next == nullptr) {
                    AMBRO_ASSERT(amount == 0)
                    return;
                }
            }
            
            node = node->next;
            offset = 0;
        }
    }
    
    /**
     * Return a memory range that is an initial part of
     * this memory range.
     * 
     * It returns an IpBufRef with the same node and offset
     * but with tot_len equal to new_tot_len. The new_tot_len
     * must not exceed tot_len.
     */
    inline IpBufRef subTo (size_t new_tot_len) const
    {
        AMBRO_ASSERT(new_tot_len <= tot_len)
        
        return IpBufRef {
            node,
            offset,
            new_tot_len
        };
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

#include <aipstack/EndNamespace.h>

#endif
