/*
 * Copyright (c) 2013 Ambroz Bizjak
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

#ifndef APRINTER_LINK_MODEL_H
#define APRINTER_LINK_MODEL_H

#include <aprinter/BeginNamespace.h>

template <typename Entry>
class PointerLinkModel {
public:
    using State = nullptr_t;
    
    class Ref;
    
    class Link {
        friend PointerLinkModel;
        
        inline Link(Entry *ptr)
        : m_ptr(ptr) {}
        
    public:
        Link() = default;
        
        inline static Link null ()
        {
            return Link(nullptr);
        }
        
        inline bool isNull () const
        {
            return m_ptr == nullptr;
        }
        
        inline Ref ref (State) const
        {
            return Ref(*this);
        }
        
        inline bool operator== (Link const &other) const
        {
            return m_ptr == other.m_ptr;
        }
        
    private:
        Entry *m_ptr;
    };
    
    class Ref {
        friend PointerLinkModel;
        
        inline Ref(Link link)
        : m_link(link) {}
        
    public:
        inline static Ref null ()
        {
            return Ref(Link::null());
        }
        
        Ref() = default;
        
        inline Ref(Entry &entry)
        : m_link(Link(&entry))
        {}
        
        inline bool isNull () const
        {
            return m_link.isNull();
        }
        
        inline Link link () const
        {
            return m_link;
        }
        
        inline Entry & operator* () const
        {
            return *m_link.m_ptr;
        }
        
        inline Entry * pointer () const
        {
            return m_link.m_ptr;
        }
        
    private:
        Link m_link;
    };
};

template <
    typename Entry,
    typename StateType,
    typename ArrayAccessor,
    typename IndexType,
    IndexType NullIndex
>
class ArrayLinkModel {
public:
    using State = StateType;
    
    class Ref;
    
    class Link {
        friend ArrayLinkModel;
        
        inline Link(IndexType index)
        : m_index(index) {}
        
    public:
        Link() = default;
        
        inline static Link null ()
        {
            return Link(NullIndex);
        }
        
        inline bool isNull () const
        {
            return m_index == NullIndex;
        }
        
        inline Ref ref (State state) const
        {
            if (isNull()) {
                return Ref(this, nullptr);
            } else {
                auto array = ArrayAccessor::access(state);
                return Ref(this, &array[m_index]);
            }
        }
        
        inline bool operator== (Link const &other) const
        {
            return m_index == other.m_index;
        }
        
    private:
        IndexType m_index;
    };
    
    class Ref {
        friend ArrayLinkModel;
        
        inline Ref (Link link, Entry *ptr)
        : m_link(link), m_ptr(ptr) {}
        
    public:
        inline static Ref null ()
        {
            return Ref(Link::null(), nullptr);
        }
        
        Ref() = default;
        
        inline Ref(Entry &entry, IndexType index)
        : m_link(Link(index)), m_ptr(&entry)
        {}
        
        inline bool isNull () const
        {
            return m_link.isNull();
        }
        
        inline Link link () const
        {
            return m_link;
        }
        
        inline Entry & operator* () const
        {
            return *m_ptr;
        }
        
    private:
        Link m_link;
        Entry *m_ptr;
    };
};

#include <aprinter/EndNamespace.h>

#endif
