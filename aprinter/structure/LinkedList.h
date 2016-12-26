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

#ifndef APRINTER_LINKED_LIST_H
#define APRINTER_LINKED_LIST_H

#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/FunctionIf.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Accessor.h>

#include <aprinter/BeginNamespace.h>

template <typename, typename, typename, bool>
class LinkedList;

template <typename LinkModel>
class LinkedListNode {
    template <typename, typename, typename, bool>
    friend class LinkedList;
    
    using Link = typename LinkModel::Link;
    
private:
    Link next;
    Link prev;
};

template <typename LinkModel>
struct LinkedList__Extra {
    APRINTER_STRUCT_IF_TEMPLATE(ExtraForWithLast) {
        typename LinkModel::Link m_last;
    };
};

template <
    typename Entry,
    typename Accessor,
    typename LinkModel,
    bool WithLast_
>
class LinkedList :
    private LinkedList__Extra<LinkModel>::template ExtraForWithLast<WithLast_>
{
    using Link = typename LinkModel::Link;
    
private:
    Link m_first;
    
public:
    static bool const WithLast = WithLast_;
    
    using State = typename LinkModel::State;
    using Ref = typename LinkModel::Ref;
    
    inline void init ()
    {
        m_first = Link::null();
    }
    
    inline bool isEmpty () const
    {
        return m_first.isNull();
    }
    
    inline Ref first (State st = State()) const
    {
        return m_first.ref(st);
    }
    
    APRINTER_FUNCTION_IF_EXT(WithLast, inline, Ref, lastNotEmpty (State st = State()) const)
    {
        AMBRO_ASSERT(!m_first.isNull())
        
        return this->m_last.ref(st);
    }
    
    inline static Ref next (Ref e, State st = State())
    {
        return ac(e).next.ref(st);
    }
    
    void prepend (Ref e, State st = State())
    {
        ac(e).next = m_first;
        if (!m_first.isNull()) {
            ac(m_first.ref(st)).prev = e.link();
        } else {
            set_last(e.link());
        }
        m_first = e.link();
    }
    
    APRINTER_FUNCTION_IF(WithLast, void, append (Ref e, State st = State()))
    {
        ac(e).next = Link::null();
        if (!m_first.isNull()) {
            ac(e).prev = this->m_last;
            ac(this->m_last.ref(st)).next = e.link();
        } else {
            m_first = e.link();
        }
        this->m_last = e.link();
    }
    
    void remove (Ref e, State st = State())
    {
        if (!(e.link() == m_first)) {
            ac(ac(e).prev.ref(st)).next = ac(e).next;
            if (!ac(e).next.isNull()) {
                ac(ac(e).next.ref(st)).prev = ac(e).prev;
            } else {
                set_last(ac(e).prev);
            }
        } else {
            m_first = ac(e).next;
        }
    }
    
    inline void removeFirst (State st = State())
    {
        AMBRO_ASSERT(!m_first.isNull())
        
        m_first = ac(m_first.ref(st)).next;
    }
    
    inline static void markRemoved (Ref e)
    {
        ac(e).next = e.link();
    }
    
    inline static bool isRemoved (Ref e)
    {
        return ac(e).next == e.link();
    }
    
private:
    inline static LinkedListNode<LinkModel> & ac (Ref ref)
    {
        return Accessor::access(*ref);
    }
    
    APRINTER_FUNCTION_IF_OR_EMPTY_EXT(WithLast, inline, void, set_last (Link last))
    {
        this->m_last = last;
    }
};

#include <aprinter/EndNamespace.h>

#endif
