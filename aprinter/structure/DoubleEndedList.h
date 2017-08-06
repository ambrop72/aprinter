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

#ifndef AMBROLIB_DOUBLE_ENDED_LIST
#define AMBROLIB_DOUBLE_ENDED_LIST

#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/FunctionIf.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Accessor.h>

namespace APrinter {

template <class, class, bool>
class DoubleEndedListWithAccessor;

template <class Entry>
class DoubleEndedListNode {
    template <class, class, bool>
    friend class DoubleEndedListWithAccessor;
    
    Entry *next;
    Entry *prev;
};

template <class Entry>
struct DoubleEndedList__Extra {
    APRINTER_STRUCT_IF_TEMPLATE(Extra) {
        Entry *m_last;
    };
};

template <class Entry, class Accessor, bool TWithLast=true>
class DoubleEndedListWithAccessor : private DoubleEndedList__Extra<Entry>::template Extra<TWithLast> {
public:
    static bool const WithLast = TWithLast;
    
    void init ()
    {
        m_first = nullptr;
    }
    
    bool isEmpty () const
    {
        return (m_first == nullptr);
    }
    
    Entry * first () const
    {
        return m_first;
    }
    
    APRINTER_FUNCTION_IF(WithLast, Entry *, lastNotEmpty () const)
    {
        AMBRO_ASSERT(m_first != nullptr)
        
        return this->m_last;
    }
    
    Entry * next (Entry *e) const
    {
        return ac(*e).next;
    }
    
    void prepend (Entry *e)
    {
        ac(*e).next = m_first;
        if (m_first) {
            ac(*m_first).prev = e;
        } else {
            set_last(e);
        }
        m_first = e;
    }
    
    APRINTER_FUNCTION_IF(WithLast, void, append (Entry *e))
    {
        ac(*e).next = nullptr;
        if (m_first) {
            ac(*e).prev = this->m_last;
            ac(*this->m_last).next = e;
        } else {
            m_first = e;
        }
        this->m_last = e;
    }
    
    void remove (Entry *e)
    {
        if (e != m_first) {
            ac(*ac(*e).prev).next = ac(*e).next;
            if (ac(*e).next) {
                ac(*ac(*e).next).prev = ac(*e).prev;
            } else {
                set_last(ac(*e).prev);
            }
        } else {
            m_first = ac(*e).next;
        }
    }
    
    void removeFirst ()
    {
        AMBRO_ASSERT(m_first)
        
        m_first = ac(*m_first).next;
    }
    
    static void markRemoved (Entry *e)
    {
        ac(*e).next = e;
    }
    
    static bool isRemoved (Entry *e)
    {
        return (ac(*e).next == e);
    }
    
private:
    static DoubleEndedListNode<Entry> & ac (Entry &e)
    {
        return Accessor::access(e);
    }
    
    APRINTER_FUNCTION_IF_OR_EMPTY(WithLast, void, set_last (Entry *last))
    {
        this->m_last = last;
    }
    
    Entry *m_first;
};

template <class Entry, DoubleEndedListNode<Entry> Entry::*NodeMember, bool WithLast=true>
class DoubleEndedList : public DoubleEndedListWithAccessor<
    Entry,
    APRINTER_MEMBER_ACCESSOR_TN(NodeMember),
    WithLast
> {};

template <class Entry, class Base, DoubleEndedListNode<Entry> Base::*NodeMember, bool WithLast=true>
class DoubleEndedListForBase : public DoubleEndedListWithAccessor<
    Entry,
    MemberAccessorWithBase<Entry, DoubleEndedListNode<Entry>, Base, NodeMember>,
    WithLast
> {};

}

#endif
