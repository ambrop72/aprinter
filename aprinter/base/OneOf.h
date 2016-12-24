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

#ifndef APRINTER_ONE_OF_H
#define APRINTER_ONE_OF_H

#include <aprinter/base/Hints.h>

#include <aprinter/BeginNamespace.h>

template <typename...>
struct OneOfStruct;

template <typename OptRefType, typename... TailOptRefType>
struct OneOfStruct<OptRefType, TailOptRefType...> {
    AMBRO_ALWAYS_INLINE
    constexpr OneOfStruct (OptRefType const &opt_ref_arg, TailOptRefType const & ... tail_opt_ref_arg)
    : opt_ref(opt_ref_arg), tail_opt_ref(tail_opt_ref_arg...) {}
    
    template <typename SelType>
    AMBRO_ALWAYS_INLINE
    constexpr bool one_of (SelType const &sel) const
    {
        return sel == opt_ref || tail_opt_ref.one_of(sel);
    }
    
    OptRefType opt_ref;
    OneOfStruct<TailOptRefType...> tail_opt_ref;
};

template <>
struct OneOfStruct<> {
    AMBRO_ALWAYS_INLINE
    constexpr OneOfStruct () {}
    
    template <typename SelType>
    AMBRO_ALWAYS_INLINE
    constexpr bool one_of (SelType const &sel) const
    {
        return false;
    }
};

template <typename SelType, typename... OptRefType>
AMBRO_ALWAYS_INLINE
bool operator== (SelType const &sel, OneOfStruct<OptRefType...> opt_struct)
{
    return opt_struct.one_of(sel);
}

template <typename SelType, typename... OptRefType>
AMBRO_ALWAYS_INLINE
bool operator!= (SelType const &sel, OneOfStruct<OptRefType...> opt_struct)
{
    return !opt_struct.one_of(sel);
}

/**
 * Use to check if a value is equal to or not equal to any of the choices,
 * according to the following recipe:
 *   X == OneOf(C1, ..., CN)
 *   X != OneOf(C1, ..., CN)
 * 
 * NOTE: This stores the arguments by value in the intermediate struct.
 * Since the intention is to be used with integers/enums, and inlining
 * is forced, it should regardless have no special overhead.
 */
template <typename... OptType>
AMBRO_ALWAYS_INLINE
OneOfStruct<OptType...> OneOf (OptType ... opt)
{
    return OneOfStruct<OptType...>(opt...);
}

/**
 * Defines a OneOf function serving as an alias for OneOf.
 * This is useful so one does not need to write APrinter::OneOf
 * when using from another namespace.
 */
#define APRINTER_USE_ONEOF \
template <typename... APrinter_OneOf_OptType> \
AMBRO_ALWAYS_INLINE \
static auto OneOf(APrinter_OneOf_OptType... APrinter_OneOf_opt) \
{ return APrinter::OneOf(APrinter_OneOf_opt...); }

#include <aprinter/EndNamespace.h>

#endif
