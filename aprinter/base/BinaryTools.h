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
#include <aprinter/meta/WrapValue.h>

#include <aprinter/BeginNamespace.h>

namespace Private {
    template <bool IsSigned>
    struct ReadHelper {
        template <typename Type, bool IsBigEndian>
        static Type call (char const *src)
        {
            using TypeInfo = IntTypeInfo<Type>;
            static_assert(!TypeInfo::Signed, "");
            
            Type val = 0;
            for (int i = 0; i < sizeof(Type); i++) {
                int j = IsBigEndian ? (sizeof(Type) - 1 - i) : i;
                val |= (Type)(uint8_t)src[i] << (8 * j);
            }
            return val;
        }
    };
    
    template <>
    struct ReadHelper<true> {
        template <typename Type, bool IsBigEndian>
        static Type call (char const *src)
        {
            using TypeInfo = IntTypeInfo<Type>;
            static_assert(TypeInfo::Signed, "");
            
            using UType = ChooseInt<TypeInfo::NumBits, false>;
            UType uval = ReadHelper<false>::template call<UType, IsBigEndian>(src);
            return reinterpret_cast<Type const &>(uval);
        }
    };
}

template <bool TIsBigEndian>
struct BinaryEndian {
    static bool const IsBigEndian = TIsBigEndian;
};

using BinaryLittleEndian = BinaryEndian<false>;
using BinaryBigEndian = BinaryEndian<true>;

template <typename Type, typename Endian>
Type ReadBinaryInt (char const *src)
{
    using TypeInfo = IntTypeInfo<Type>;
    return Private::ReadHelper<TypeInfo::Signed>::template call<Type, Endian::IsBigEndian>(src);
}

#include <aprinter/EndNamespace.h>

#endif
