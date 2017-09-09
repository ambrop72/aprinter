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

#ifndef APRINTER_IPSTACK_CHKSUM_H
#define APRINTER_IPSTACK_CHKSUM_H

#include <stdint.h>
#include <stddef.h>

#include <limits>

#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Hints.h>

#include <aipstack/common/Buf.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/BinaryTools.h>

// NOTE: IpChksumInverted (and IpChksum) accept size_t len
// but the length must not exceed 65535. This is okay since
// checksums always apply to data within IP packets which
// cannot be larget than that.

#if AIPSTACK_EXTERNAL_CHKSUM
extern "C" uint16_t IpChksumInverted (char const *data, size_t len);
#else

APRINTER_NO_INLINE
inline uint16_t IpChksumInverted (char const *data, size_t len)
{
    using namespace AIpStack;
    
    char const *even_end = data + (len & (size_t)-2);
    uint32_t sum = 0;
    
    while (data < even_end) {
        sum += ReadBinaryInt<uint16_t, BinaryBigEndian>(data);
        data += 2;
    }
    
    if ((len & 1) != 0) {
        uint8_t byte = ReadBinaryInt<uint8_t, BinaryBigEndian>(data);
        sum += (uint16_t)byte << 8;
    }
    
    sum = (sum & UINT32_C(0xFFFF)) + (sum >> 16);
    sum = (sum & UINT32_C(0xFFFF)) + (sum >> 16);
    
    return sum;
}

#endif

namespace AIpStack {

inline uint16_t IpChksum (char const *data, size_t len)
{
    return ~IpChksumInverted(data, len);
}

class IpChksumAccumulator {
private:
    uint32_t m_sum;
    
public:
    enum State : uint32_t {};
    
    inline IpChksumAccumulator ()
    : m_sum(0)
    {
    }
    
    inline IpChksumAccumulator (State state)
    : m_sum(state)
    {
    }
    
    inline State getState () const
    {
        return State(m_sum);
    }
    
    inline void addWord (APrinter::WrapType<uint16_t>, uint16_t word)
    {
        m_sum += word;
    }
    
    inline void addWord (APrinter::WrapType<uint32_t>, uint32_t word)
    {
        addWord(APrinter::WrapType<uint16_t>(), (uint16_t)(word >> 16));
        addWord(APrinter::WrapType<uint16_t>(), (uint16_t)word);
    }
    
    template <typename WordType, int NumWords>
    inline void addWords (WordType const *words)
    {
        for (int i = 0; i < NumWords; i++) {
            addWord(APrinter::WrapType<WordType>(), words[i]);
        }
    }
    
    template <typename WordType, int NumWords>
    inline void addWords (WordType const (*words)[NumWords])
    {
        addWords<WordType, NumWords>(*words);
    }
    
    inline void addEvenBytes (char const *ptr, size_t num_bytes)
    {
        AMBRO_ASSERT(num_bytes % 2 == 0)
        
        char const *endptr = ptr + num_bytes;
        while (ptr < endptr) {
            uint16_t word = ReadBinaryInt<uint16_t, BinaryBigEndian>(ptr);
            ptr += 2;
            addWord(APrinter::WrapType<uint16_t>(), word);
        }
    }
    
    inline uint16_t getChksum ()
    {
        foldOnce();
        foldOnce();
        return ~m_sum;
    }
    
    inline uint16_t getChksum (IpBufRef buf)
    {
        if (buf.tot_len > 0) {
            addIpBuf(buf);
        }
        return getChksum();
    }
    
private:
    inline void foldOnce ()
    {
        m_sum = (m_sum & std::numeric_limits<uint16_t>::max()) + (m_sum >> 16);
    }
    
    inline static uint32_t swapBytes (uint32_t x)
    {
        return ((x >> 8) & UINT32_C(0x00FF00FF)) | ((x << 8) & UINT32_C(0xFF00FF00));
    }
    
    void addIpBuf (IpBufRef buf)
    {
        bool swapped = false;
        
        do {
            size_t len = buf.getChunkLength();
            
            // Calculate sum of buffer.
            uint16_t buf_sum = IpChksumInverted(buf.getChunkPtr(), len);
            
            // Add the buffer sum to our sum.
            uint32_t old_sum = m_sum;
            m_sum += buf_sum;
            
            // Fold back any overflow.
            if (AMBRO_UNLIKELY(m_sum < old_sum)) {
                m_sum++;
            }
            
            // If the buffer has an odd length, swap bytes in sum.
            if (len % 2 != 0) {
                m_sum = swapBytes(m_sum);
                swapped = !swapped;
            }
        } while (buf.nextChunk());
        
        // Swap bytes if we swapped an odd number of times.
        if (swapped) {
            m_sum = swapBytes(m_sum);
        }
    }
};

inline uint16_t IpChksum (IpBufRef buf)
{
    IpChksumAccumulator accum;
    return accum.getChksum(buf);
}

}

#endif
