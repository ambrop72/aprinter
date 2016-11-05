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

#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/base/Assert.h>
#include <aprinter/ipstack/misc/Buf.h>

// Must be implemented externally.
extern "C" uint16_t lwip_standard_chksum (void const *data, int mlen);

#include <aprinter/BeginNamespace.h>

static inline uint16_t IpChksumInverted (char const *data, size_t len)
{
    uint16_t chksum = lwip_standard_chksum(data, len);
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    chksum = __builtin_bswap16(chksum);
#endif
    return chksum;
}

static inline uint16_t IpChksum (char const *data, size_t len)
{
    return ~IpChksumInverted(data, len);
}

class IpChksumAccumulator {
public:
    inline IpChksumAccumulator()
    : m_sum(0),
      m_swapped(false)
    {}
    
    inline void addChksum (uint16_t chksum_inverted, size_t len)
    {
        // Add the current 16-bit sum and the 16-bit sum of this data.
        uint32_t sum32 = (uint32_t)m_sum + chksum_inverted;
        
        // Fold once to bring this back to 16 bits.
        // At most one fold is needed - worst case if 0xFFFF+0xFFFF
        // which sums to 0x1FFFE which is folded to 0xFFFF.
        uint16_t sum = foldOnce(sum32);
        
        // If this is an odd-sized chunk, swap the sum and note
        // that the swapping took place.
        if (len % 2 != 0) {
            sum = swapBytes(sum);
            m_swapped = !m_swapped;
        }
        
        // Update sum.
        m_sum = sum;
    }
    
    inline void addData (char const *data, size_t len)
    {
        uint16_t chksum_inverted = IpChksumInverted(data, len);
        addChksum(chksum_inverted, len);
    }
    
    inline void addWord (WrapType<uint16_t>, uint16_t word)
    {
        uint32_t sum32 = (uint32_t)m_sum + word;
        m_sum = foldOnce(sum32);
    }
    
    inline void addWord (WrapType<uint32_t>, uint32_t word)
    {
        addWord(WrapType<uint16_t>(), (uint16_t)(word >> 16));
        addWord(WrapType<uint16_t>(), (uint16_t)word);
    }
    
    template <typename WordType, int NumWords>
    inline void addWords (WordType const *words)
    {
        for (int i = 0; i < NumWords; i++) {
            addWord(WrapType<WordType>(), words[i]);
        }
    }
    
    template <typename WordType, int NumWords>
    inline void addWords (WordType const (*words)[NumWords])
    {
        addWords<WordType, NumWords>(*words);
    }
    
    inline uint16_t getChksum ()
    {
        uint16_t sum = m_sum;
        
        // Swap if needed.
        if (m_swapped) {
            sum = swapBytes(sum);
        }
        
        return ~sum;
    }
    
    inline void addIpBuf (IpBufRef buf)
    {
        do {
            addData(buf.getChunkPtr(), buf.getChunkLength());
        } while (buf.nextChunk());
    }
    
private:
    inline static uint32_t foldOnce (uint32_t x)
    {
        return (x & UINT16_MAX) + (x >> 16);
    }
    
    inline static uint16_t swapBytes (uint16_t x)
    {
        return (x >> 8) | ((x & UINT16_C(0xFF)) << 8);
    }
    
    uint16_t m_sum;
    bool m_swapped;
};

static uint16_t IpChksum (IpBufRef buf)
{
    IpChksumAccumulator accum;
    accum.addIpBuf(buf);
    return accum.getChksum();
}

#include <aprinter/EndNamespace.h>

#endif
