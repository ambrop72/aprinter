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

#ifndef APRINTER_ENUM_UTILS_H
#define APRINTER_ENUM_UTILS_H

#include <type_traits>

#include <aprinter/meta/BasicMetaUtils.h>

namespace APrinter {

template <typename EnumType>
constexpr inline auto ToUnderlyingType (EnumType e)
{
    static_assert(std::is_enum<EnumType>::value, "EnumType must be an enum type");
    return std::underlying_type_t<EnumType>(e);
}

namespace Private {
    template <bool IsEnum, typename Type, typename BaseType>
    struct EnumWithBaseTypeHelper {
        static bool const IsEnumWithBaseType = false;
    };
    
    template <typename Type, typename BaseType>
    struct EnumWithBaseTypeHelper<true, Type, BaseType> {
        static bool const IsEnumWithBaseType =
            std::is_same<std::underlying_type_t<Type>, BaseType>::value;
    };
    
    template <bool IsEnum, typename Type>
    struct GetSameOrBaseTypeHelper {
        using ResultType = Type;
    };
    
    template <typename Type>
    struct GetSameOrBaseTypeHelper<true, Type> {
        using ResultType = std::underlying_type_t<Type>;
    };
};

template <typename Type, typename BaseType>
constexpr bool IsEnumWithBaseType ()
{
    return Private::EnumWithBaseTypeHelper<std::is_enum<Type>::value, Type, BaseType>::IsEnumWithBaseType;
}

template <typename Type, typename BaseType>
constexpr bool IsSameOrEnumWithBaseType ()
{
    return std::is_same<Type, BaseType>::value || IsEnumWithBaseType<Type, BaseType>();
}

template <typename Type>
using GetSameOrEnumBaseType = typename Private::GetSameOrBaseTypeHelper<std::is_enum<Type>::value, Type>::ResultType;

}

#endif
