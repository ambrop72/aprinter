/*
 * Copyright (c) 2014 Ambroz Bizjak
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

#ifndef AMBROLIB_STATIC_ARRAY_H
#define AMBROLIB_STATIC_ARRAY_H

#include <stddef.h>

#include <aprinter/meta/TypeSequence.h>
#include <aprinter/meta/TypeSequenceMakeInt.h>
#include <aprinter/base/ProgramMemory.h>

#include <aprinter/BeginNamespace.h>

template <typename, template<int> class, typename>
struct StaticArrayHelper;

template <typename ElemType, template<int> class ElemValue, typename... Indices>
class StaticArrayHelper<ElemType, ElemValue, TypeSequence<Indices...>>
{
public:
    static size_t const Length = sizeof...(Indices);
    
    static ElemType readAt (size_t index)
    {
#if AMBRO_HAS_NONTRANSPARENT_PROGMEM
        ElemType elem;
        AMBRO_PGM_MEMCPY(&elem, &data[index], sizeof(elem));
        return elem;
#else
        return data[index];
#endif
    }
    
private:
    static ElemType AMBRO_PROGMEM const data[Length];
};

template <typename ElemType, template<int> class ElemValue, typename... Indices>
ElemType AMBRO_PROGMEM const StaticArrayHelper<ElemType, ElemValue, TypeSequence<Indices...>>::data[] = {ElemValue<Indices::Value>::value()...};

template <typename ElemType, int Size, template<int> class ElemValue>
using StaticArray = StaticArrayHelper<ElemType, ElemValue, TypeSequenceMakeInt<Size>>;

#include <aprinter/EndNamespace.h>

#endif
