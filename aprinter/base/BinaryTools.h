/*
 * Copyright (c) 2015 Ambroz Bizjak
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

#ifndef AMBROLIB_BINARY_TOOLS_H
#define AMBROLIB_BINARY_TOOLS_H

#include <stdint.h>

#include <aprinter/meta/IntTypeInfo.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/base/Hints.h>

#include <aprinter/BeginNamespace.h>

namespace Private {
    template <int Bits>
    using UnsignedType = ChooseInt<Bits, false>;
    
    template <int Bits, bool BigEndian>
    struct ReadUnsigned {
        using IntType = UnsignedType<Bits>;
        static_assert(Bits % 8 == 0, "");
        static int const Bytes = Bits / 8;
        
        AMBRO_ALWAYS_INLINE APRINTER_UNROLL_LOOPS
        static IntType readInt (char const *src)
        {
            IntType val = 0;
            for (int i = 0; i < Bytes; i++) {
                int j = BigEndian ? (Bytes - 1 - i) : i;
                val |= (IntType)((unsigned char)src[i] & 0xFF) << (8 * j);
            }
            return val;
        }
    };
    
    template <int Bits, bool BigEndian>
    struct WriteUnsigned {
        using IntType = UnsignedType<Bits>;
        static_assert(Bits % 8 == 0, "");
        static int const Bytes = Bits / 8;
        
        AMBRO_ALWAYS_INLINE APRINTER_UNROLL_LOOPS
        static void writeInt (IntType value, char *dst)
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
#define APRINTER_BINARYTOOLS_BIG_ENDIAN 0
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define APRINTER_BINARYTOOLS_BIG_ENDIAN 1
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
    struct ReadUnsigned<32, BigEndian> {
        AMBRO_ALWAYS_INLINE
        static uint32_t readInt (char const *src)
        {
            uint32_t w;
            __builtin_memcpy(&w, src, sizeof(w));
            return BigEndian != APRINTER_BINARYTOOLS_BIG_ENDIAN ? __builtin_bswap32(w) : w;
        }
    };
    
    template <bool BigEndian>
    struct WriteUnsigned<32, BigEndian> {
        AMBRO_ALWAYS_INLINE
        static void writeInt (uint32_t value, char *dst)
        {
            uint32_t w = BigEndian != APRINTER_BINARYTOOLS_BIG_ENDIAN ? __builtin_bswap32(value) : value;
            __builtin_memcpy(dst, &w, sizeof(w));
        }
    };
    
    template <bool BigEndian>
    struct ReadUnsigned<16, BigEndian> {
        AMBRO_ALWAYS_INLINE
        static uint16_t readInt (char const *src)
        {
            uint16_t w;
            __builtin_memcpy(&w, src, sizeof(w));
            return BigEndian != APRINTER_BINARYTOOLS_BIG_ENDIAN ? __builtin_bswap16(w) : w;
        }
    };
    
    template <bool BigEndian>
    struct WriteUnsigned<16, BigEndian> {
        AMBRO_ALWAYS_INLINE
        static void writeInt (uint16_t value, char *dst)
        {
            uint16_t w = BigEndian != APRINTER_BINARYTOOLS_BIG_ENDIAN ? __builtin_bswap16(value) : value;
            __builtin_memcpy(dst, &w, sizeof(w));
        }
    };
    
#endif
    
    template <bool IsSigned>
    struct BinaryToolsHelper {
        template <typename Type, bool BigEndian>
        inline static Type read_it (char const *src)
        {
            static_assert(!IntTypeInfo<Type>::Signed, "");
            
            return ReadUnsigned<IntTypeInfo<Type>::NumBits, BigEndian>::readInt(src);
        }
        
        template <typename Type, bool BigEndian>
        inline static void write_it (Type value, char *dst)
        {
            static_assert(!IntTypeInfo<Type>::Signed, "");
            
            return WriteUnsigned<IntTypeInfo<Type>::NumBits, BigEndian>::writeInt(value, dst);
        }
    };
    
    template <>
    struct BinaryToolsHelper<true> {
        template <typename Type, bool BigEndian>
        inline static Type read_it (char const *src)
        {
            static_assert(IntTypeInfo<Type>::Signed, "");
            using UType = ChooseInt<IntTypeInfo<Type>::NumBits, false>;
            
            UType uval = BinaryToolsHelper<false>::template read_it<UType, BigEndian>(src);
            return reinterpret_cast<Type const &>(uval);
        }
        
        template <typename Type, bool BigEndian>
        inline static void write_it (Type value, char *dst)
        {
            static_assert(IntTypeInfo<Type>::Signed, "");
            using UType = ChooseInt<IntTypeInfo<Type>::NumBits, false>;
            
            UType uval = value;
            BinaryToolsHelper<false>::template write_it<UType, BigEndian>(uval, dst);
        }
    };
}

template <bool TBigEndian>
struct BinaryEndian {
    static bool const BigEndian = TBigEndian;
};

using BinaryLittleEndian = BinaryEndian<false>;
using BinaryBigEndian = BinaryEndian<true>;

template <typename Type, typename Endian>
inline Type ReadBinaryInt (char const *src)
{
    using TypeInfo = IntTypeInfo<Type>;
    return Private::BinaryToolsHelper<TypeInfo::Signed>::template read_it<Type, Endian::BigEndian>(src);
}

template <typename Type, typename Endian>
inline void WriteBinaryInt (Type value, char *dst)
{
    using TypeInfo = IntTypeInfo<Type>;
    return Private::BinaryToolsHelper<TypeInfo::Signed>::template write_it<Type, Endian::BigEndian>(value, dst);
}

#include <aprinter/EndNamespace.h>

#endif
