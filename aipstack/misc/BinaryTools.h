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

#ifndef AIPSTACK_BINARY_TOOLS_H
#define AIPSTACK_BINARY_TOOLS_H

#include <stdint.h>

#include <type_traits>
#include <limits>

#include <aprinter/base/Hints.h>

namespace AIpStack {

namespace BinaryToolsPrivate {
    
    template <typename T>
    constexpr bool IsValidType ()
    {
        return std::is_integral<T>::value && std::numeric_limits<T>::radix == 2;
    }
    
    template <int Bits>
    struct RepresentativeImpl;

    #define AIPSTACK_DEFINE_REPRESENTATIVE(bits, repr_type) \
    template <> \
    struct RepresentativeImpl<bits> { \
        using Type = repr_type; \
    };

    AIPSTACK_DEFINE_REPRESENTATIVE(8,  uint8_t)
    AIPSTACK_DEFINE_REPRESENTATIVE(16, uint16_t)
    AIPSTACK_DEFINE_REPRESENTATIVE(32, uint32_t)
    AIPSTACK_DEFINE_REPRESENTATIVE(64, uint64_t)

    #undef AIPSTACK_DEFINE_REPRESENTATIVE
    
    template <typename T>
    struct RepresentativeCheck {
        static_assert(IsValidType<T>(), "");
        static_assert(std::is_unsigned<T>::value, "");
        
        using Type = typename RepresentativeImpl<std::numeric_limits<T>::digits>::Type;
    };
    
    template <typename T>
    using Representative = typename RepresentativeCheck<T>::Type;
    
    template <typename T, bool BigEndian>
    struct ReadUnsigned {
        static int const Bits = std::numeric_limits<T>::digits;
        static_assert(Bits % 8 == 0, "");
        static int const Bytes = Bits / 8;
        
        AMBRO_ALWAYS_INLINE APRINTER_UNROLL_LOOPS
        static T readInt (char const *src)
        {
            T val = 0;
            for (int i = 0; i < Bytes; i++) {
                int j = BigEndian ? (Bytes - 1 - i) : i;
                val |= (T)((unsigned char)src[i] & 0xFF) << (8 * j);
            }
            return val;
        }
    };
    
    template <typename T, bool BigEndian>
    struct WriteUnsigned {
        static int const Bits = std::numeric_limits<T>::digits;
        static_assert(Bits % 8 == 0, "");
        static int const Bytes = Bits / 8;
        
        AMBRO_ALWAYS_INLINE APRINTER_UNROLL_LOOPS
        static void writeInt (T value, char *dst)
        {
            for (int i = 0; i < Bytes; i++) {
                int j = BigEndian ? (Bytes - 1 - i) : i;
                ((unsigned char *)dst)[i] = (value >> (8 * j)) & 0xFF;
            }
        }
    };
    
#if defined(__GNUC__) && defined(__BYTE_ORDER__) && \
    (__ARM_ARCH >= 7 && __ARM_FEATURE_UNALIGNED)
    
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define AIPSTACK_BINARYTOOLS_BIG_ENDIAN 0
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define AIPSTACK_BINARYTOOLS_BIG_ENDIAN 1
#else
#error "Unknown endian"
#endif
    
    /*
     * These implementations are for architectures which support
     * unaligned memory access, with the intention that the memcpy
     * is compiled to a single load/store instruction.
     * 
     * The code should work generally with GCC however it is not enabled
     * by default since it may result in much worse code than default
     * implementations above. For example on ARM cortex-m3 with forced
     * -mno-unaligned-access, actual memcpy calls have been seen.
     * So, specific configurations should be added in the test above
     * only after it is confirmed the result is good.
     */
    
    template <bool BigEndian>
    struct ReadUnsigned<uint32_t, BigEndian> {
        AMBRO_ALWAYS_INLINE
        static uint32_t readInt (char const *src)
        {
            uint32_t w;
            __builtin_memcpy(&w, src, sizeof(w));
            return BigEndian != AIPSTACK_BINARYTOOLS_BIG_ENDIAN ? __builtin_bswap32(w) : w;
        }
    };
    
    template <bool BigEndian>
    struct WriteUnsigned<uint32_t, BigEndian> {
        AMBRO_ALWAYS_INLINE
        static void writeInt (uint32_t value, char *dst)
        {
            uint32_t w = BigEndian != AIPSTACK_BINARYTOOLS_BIG_ENDIAN ? __builtin_bswap32(value) : value;
            __builtin_memcpy(dst, &w, sizeof(w));
        }
    };
    
    template <bool BigEndian>
    struct ReadUnsigned<uint16_t, BigEndian> {
        AMBRO_ALWAYS_INLINE
        static uint16_t readInt (char const *src)
        {
            uint16_t w;
            __builtin_memcpy(&w, src, sizeof(w));
            return BigEndian != AIPSTACK_BINARYTOOLS_BIG_ENDIAN ? __builtin_bswap16(w) : w;
        }
    };
    
    template <bool BigEndian>
    struct WriteUnsigned<uint16_t, BigEndian> {
        AMBRO_ALWAYS_INLINE
        static void writeInt (uint16_t value, char *dst)
        {
            uint16_t w = BigEndian != AIPSTACK_BINARYTOOLS_BIG_ENDIAN ? __builtin_bswap16(value) : value;
            __builtin_memcpy(dst, &w, sizeof(w));
        }
    };
    
#endif
    
    template <bool IsSigned>
    struct SignHelper {
        template <typename T, bool BigEndian>
        inline static T read_it (char const *src)
        {
            static_assert(std::is_unsigned<T>::value, "");
            
            return ReadUnsigned<Representative<T>, BigEndian>::readInt(src);
        }
        
        template <typename T, bool BigEndian>
        inline static void write_it (T value, char *dst)
        {
            static_assert(std::is_unsigned<T>::value, "");
            
            return WriteUnsigned<Representative<T>, BigEndian>::writeInt(value, dst);
        }
    };
    
    template <>
    struct SignHelper<true> {
        template <typename T, bool BigEndian>
        inline static T read_it (char const *src)
        {
            static_assert(std::is_signed<T>::value, "");
            using UT = std::make_unsigned_t<T>;
            
            UT uval = SignHelper<false>::template read_it<UT, BigEndian>(src);
            return reinterpret_cast<T const &>(uval);
        }
        
        template <typename T, bool BigEndian>
        inline static void write_it (T value, char *dst)
        {
            static_assert(std::is_signed<T>::value, "");
            using UT = std::make_unsigned_t<T>;
            
            UT uval = value;
            return SignHelper<false>::template write_it<UT, BigEndian>(uval, dst);
        }
    };
}

template <bool BigEndian_>
struct BinaryEndian {
    static bool const BigEndian = BigEndian_;
};

using BinaryLittleEndian = BinaryEndian<false>;
using BinaryBigEndian = BinaryEndian<true>;

template <typename T, typename Endian>
inline T ReadBinaryInt (char const *src)
{
    static_assert(BinaryToolsPrivate::IsValidType<T>(), "");
    
    return BinaryToolsPrivate::SignHelper<std::is_signed<T>::value>::
        template read_it<T, Endian::BigEndian>(src);
}

template <typename T, typename Endian>
inline void WriteBinaryInt (T value, char *dst)
{
    static_assert(BinaryToolsPrivate::IsValidType<T>(), "");
    
    return BinaryToolsPrivate::SignHelper<std::is_signed<T>::value>::
        template write_it<T, Endian::BigEndian>(value, dst);
}

}

#endif
