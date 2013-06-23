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

#include <stddef.h>

#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

template <class Entry, class Accessor>
class DoubleEndedListWithAccessor;

template <class Entry>
class DoubleEndedListNode {
    template <class, class>
    friend class DoubleEndedListWithAccessor;
    Entry *next;
    Entry *prev;
};

template <class Entry, class Accessor>
class DoubleEndedListWithAccessor {
public:
    void init ();
    bool isEmpty () const;
    Entry * first () const;
    Entry * next (Entry *e) const;
    void prepend (Entry *e);
    void append (Entry *e);
    void remove (Entry *e);
    void removeFirst ();
    static void markRemoved (Entry *e);
    static bool isRemoved (Entry *e);
    
private:
    static DoubleEndedListNode<Entry> * ac (Entry *e);
    
public:
    Entry *m_first;
    Entry *m_last;
};

template <class Entry, DoubleEndedListNode<Entry> Entry::*NodeMember>
struct DoubleEndedListAccessor {
    static DoubleEndedListNode<Entry> * access (Entry *e)
    {
        return &(e->*NodeMember);
    }
};

template <class Entry, DoubleEndedListNode<Entry> Entry::*NodeMember>
class DoubleEndedList : public DoubleEndedListWithAccessor<Entry, DoubleEndedListAccessor<Entry, NodeMember> > {};

template <class Entry, class Accessor>
DoubleEndedListNode<Entry> * DoubleEndedListWithAccessor<Entry, Accessor>::ac (Entry *e)
{
    return Accessor::access(e);
}

template <class Entry, class Accessor>
void DoubleEndedListWithAccessor<Entry, Accessor>::init ()
{
    m_first = NULL;
}

template <class Entry, class Accessor>
bool DoubleEndedListWithAccessor<Entry, Accessor>::isEmpty () const
{
    return (m_first == NULL);
}

template <class Entry, class Accessor>
Entry * DoubleEndedListWithAccessor<Entry, Accessor>::first () const
{
    return m_first;
}

template <class Entry, class Accessor>
Entry * DoubleEndedListWithAccessor<Entry, Accessor>::next (Entry *e) const
{
    return ac(e)->next;
}

template <class Entry, class Accessor>
void DoubleEndedListWithAccessor<Entry, Accessor>::prepend (Entry *e)
{
    ac(e)->next = m_first;
    if (m_first) {
        ac(m_first)->prev = e;
    } else {
        m_last = e;
    }
    m_first = e;
}

template <class Entry, class Accessor>
void DoubleEndedListWithAccessor<Entry, Accessor>::append (Entry *e)
{
    ac(e)->next = NULL;
    if (m_first) {
        ac(e)->prev = m_last;
        ac(m_last)->next = e;
    } else {
        m_first = e;
    }
    m_last = e;
}

template <class Entry, class Accessor>
void DoubleEndedListWithAccessor<Entry, Accessor>::remove (Entry *e)
{
    if (e != m_first) {
        ac(ac(e)->prev)->next = ac(e)->next;
        if (ac(e)->next) {
            ac(ac(e)->next)->prev = ac(e)->prev;
        } else {
            m_last = ac(e)->prev;
        }
    } else {
        m_first = ac(e)->next;
    }
}

template <class Entry, class Accessor>
void DoubleEndedListWithAccessor<Entry, Accessor>::removeFirst ()
{
    AMBRO_ASSERT(m_first)
    
    m_first = ac(m_first)->next;
}

template <class Entry, class Accessor>
void DoubleEndedListWithAccessor<Entry, Accessor>::markRemoved (Entry *e)
{
    ac(e)->next = e;
}

template <class Entry, class Accessor>
bool DoubleEndedListWithAccessor<Entry, Accessor>::isRemoved (Entry *e)
{
    return (ac(e)->next == e);
}

#include <aprinter/EndNamespace.h>

#endif
