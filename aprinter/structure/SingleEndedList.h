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

#ifndef AMBROLIB_SINGLE_ENDED_LIST_H
#define AMBROLIB_SINGLE_ENDED_LIST_H

#include <stddef.h>

#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

template <class Entry>
struct SingleEndedListNode {
    Entry *next;
    Entry *prev;
};

template <class Entry, class Accessor>
class SingleEndedListWithAccessor {
public:
    void init ();
    bool isEmpty () const;
    Entry * first () const;
    Entry * next (Entry *e) const;
    void prepend (Entry *e);
    void insertBefore (Entry *e, Entry *target);
    void insertAfter (Entry *e, Entry *target);
    void remove (Entry *e);
    void removeFirst ();
    static void markRemoved (Entry *e);
    static bool isRemoved (Entry *e);
    
private:
    static SingleEndedListNode<Entry> * ac (Entry *e);
    
public:
    Entry *m_first;
};

template <class Entry, SingleEndedListNode<Entry> Entry::*NodeMember>
struct SingleEndedListAccessor {
    static SingleEndedListNode<Entry> * access (Entry *e)
    {
        return &(e->*NodeMember);
    }
};

template <class Entry, SingleEndedListNode<Entry> Entry::*NodeMember>
class SingleEndedList : public SingleEndedListWithAccessor<Entry, SingleEndedListAccessor<Entry, NodeMember> > {};

template <class Entry, class Accessor>
SingleEndedListNode<Entry> * SingleEndedListWithAccessor<Entry, Accessor>::ac (Entry *e)
{
    return Accessor::access(e);
}

template <class Entry, class Accessor>
void SingleEndedListWithAccessor<Entry, Accessor>::init ()
{
    m_first = NULL;
}

template <class Entry, class Accessor>
bool SingleEndedListWithAccessor<Entry, Accessor>::isEmpty () const
{
    return (m_first == NULL);
}

template <class Entry, class Accessor>
Entry * SingleEndedListWithAccessor<Entry, Accessor>::first () const
{
    return m_first;
}

template <class Entry, class Accessor>
Entry * SingleEndedListWithAccessor<Entry, Accessor>::next (Entry *e) const
{
    return ac(e)->next;
}

template <class Entry, class Accessor>
void SingleEndedListWithAccessor<Entry, Accessor>::prepend (Entry *e)
{
    ac(e)->next = m_first;
    if (m_first) {
        ac(m_first)->prev = e;
    }
    m_first = e;
}

template <class Entry, class Accessor>
void SingleEndedListWithAccessor<Entry, Accessor>::insertBefore (Entry *e, Entry *target)
{
    ac(e)->next = target;
    if (target != m_first) {
        ac(e)->prev = ac(target)->prev;
        ac(ac(target)->prev)->next = e;
    } else {
        m_first = e;
    }
    ac(target)->prev = e;
}

template <class Entry, class Accessor>
void SingleEndedListWithAccessor<Entry, Accessor>::insertAfter (Entry *e, Entry *target)
{
    ac(e)->next = ac(target)->next;
    ac(e)->prev = target;
    if (ac(target)->next) {
        ac(ac(target)->next)->prev = e;
    }
    ac(target)->next = e;
}

template <class Entry, class Accessor>
void SingleEndedListWithAccessor<Entry, Accessor>::remove (Entry *e)
{
    if (e != m_first) {
        ac(ac(e)->prev)->next = ac(e)->next;
        if (ac(e)->next) {
            ac(ac(e)->next)->prev = ac(e)->prev;
        }
    } else {
        m_first = ac(e)->next;
    }
}

template <class Entry, class Accessor>
void SingleEndedListWithAccessor<Entry, Accessor>::removeFirst ()
{
    AMBRO_ASSERT(m_first)
    
    m_first = ac(m_first)->next;
}

template <class Entry, class Accessor>
void SingleEndedListWithAccessor<Entry, Accessor>::markRemoved (Entry *e)
{
    ac(e)->next = e;
}

template <class Entry, class Accessor>
bool SingleEndedListWithAccessor<Entry, Accessor>::isRemoved (Entry *e)
{
    return (ac(e)->next == e);
}

#include <aprinter/EndNamespace.h>

#endif
