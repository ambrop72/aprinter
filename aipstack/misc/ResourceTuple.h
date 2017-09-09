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

#ifndef AIPSTACK_RESOURCE_TUPLE_H
#define AIPSTACK_RESOURCE_TUPLE_H

#include <stddef.h>

#include <type_traits>
#include <utility>
#include <tuple>

namespace AIpStack {

struct ResourceTupleInitSame {};

namespace ResourceTuplePrivate {
    template <typename Elem, size_t Index>
    class InheritElemHelper
    {
    public:
        Elem m_elem;
        
    public:
        InheritElemHelper () = default;
        
        template <typename... Args>
        inline InheritElemHelper (ResourceTupleInitSame, Args && ... args) :
            m_elem(std::forward<Args>(args)...)
        {
        }
    };
    
    template <typename ElemsSequence, typename IndicesSequence>
    class InheritAllHelper;
    
    template <typename... Elems, size_t... Indices>
    class InheritAllHelper<std::tuple<Elems...>, std::index_sequence<Indices...>> :
        public InheritElemHelper<Elems, Indices>...
    {
    public:
        InheritAllHelper () = default;
        
        template <typename... Args>
        inline InheritAllHelper (ResourceTupleInitSame, Args const & ... args) :
            InheritElemHelper<Elems, Indices>{ResourceTupleInitSame(), args...}...
        {
        }
    };
    
    template <typename... Elems>
    using InheritAllAlias = InheritAllHelper<
        std::tuple<Elems...>, std::make_index_sequence<sizeof...(Elems)>>;
}

template <typename... Elems>
class ResourceTuple :
    private ResourceTuplePrivate::InheritAllAlias<Elems...>
{
    using InheritAll = ResourceTuplePrivate::InheritAllAlias<Elems...>;
    
public:
    template <size_t Index>
    using ElemType = std::tuple_element_t<Index, std::tuple<Elems...>>;
    
private:
    template <size_t Index>
    using ElemHelperType = ResourceTuplePrivate::InheritElemHelper<ElemType<Index>, Index>;
    
public:
    ResourceTuple () = default;
    
    template <typename... Args>
    ResourceTuple (ResourceTupleInitSame, Args const & ... args) :
        InheritAll(ResourceTupleInitSame(), args...)
    {
    }
    
    template <size_t Index>
    ElemType<Index> & get ()
    {
        return static_cast<ElemHelperType<Index> &>(*this).m_elem;
    }
    
    template <size_t Index>
    ElemType<Index> const & get () const
    {
        return static_cast<ElemHelperType<Index> const &>(*this).m_elem;
    }
};

}

#endif
