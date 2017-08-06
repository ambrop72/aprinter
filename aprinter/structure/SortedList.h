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

#ifndef APRINTER_SORTED_LIST_H
#define APRINTER_SORTED_LIST_H

#include <aprinter/base/Assert.h>
#include <aprinter/base/Accessor.h>
#include <aprinter/structure/LinkedList.h>

namespace APrinter {

//#define APRINTER_SORTED_LIST_VERIFY 1

template <typename, typename, typename>
class SortedList;

template <typename LinkModel>
class SortedListNode {
    template <typename, typename, typename>
    friend class SortedList;
    
private:
    LinkedListNode<LinkModel> list_node;
};

template <
    typename Accessor,
    typename Compare,
    typename LinkModel
>
class SortedList
{
    using Node = SortedListNode<LinkModel>;
    
    using ListNodeAccessor = ComposedAccessor<
        Accessor,
        APRINTER_MEMBER_ACCESSOR_TN(&Node::list_node)
    >;
    
    using List = LinkedList<ListNodeAccessor, LinkModel, true>;
    
private:
    List m_list;
    
public:
    using State = typename LinkModel::State;
    using Ref = typename LinkModel::Ref;
    
    inline void init ()
    {
        m_list.init();
    }
    
    inline bool isEmpty () const
    {
        return m_list.isEmpty();
    }
    
    inline Ref first (State st = State()) const
    {
        return m_list.first(st);
    }
    
    void insert (Ref node, State st = State())
    {
        Ref after_node;
        
        if (m_list.isEmpty()) {
            after_node = Ref::null();
        } else {
            after_node = m_list.lastNotEmpty(st);
            while (Compare::compareEntries(st, after_node, node) > 0) {
                if (after_node == m_list.first(st)) {
                    after_node = Ref::null();
                    break;
                }
                after_node = m_list.prevNotFirst(after_node, st);
            }
        }
        
        if (after_node.isNull()) {
            m_list.prepend(node, st);
        } else {
            m_list.insertAfter(node, after_node, st);
        }
        
        assertValidHeap(st);
    }
    
    void remove (Ref node, State st = State())
    {
        AMBRO_ASSERT(!m_list.isEmpty())
        
        m_list.remove(node, st);
        
        assertValidHeap(st);
    }
    
    void fixup (Ref node, State st = State())
    {
        AMBRO_ASSERT(!m_list.isEmpty())
        
        bool need_fixup = false;
        Ref after_node;
        
        if (!(node == m_list.first(st))) {
            after_node = m_list.prevNotFirst(node, st);
            while (Compare::compareEntries(st, after_node, node) > 0) {
                need_fixup = true;
                if (after_node == m_list.first(st)) {
                    after_node = Ref::null();
                    break;
                }
                after_node = m_list.prevNotFirst(after_node, st);
            }
        }
        
        if (!need_fixup) {
            Ref next_node = m_list.next(node, st);
            while (!next_node.isNull() &&
                   Compare::compareEntries(st, node, next_node) > 0)
            {
                need_fixup = true;
                after_node = next_node;
                next_node = m_list.next(next_node, st);
            }
        }
        
        if (need_fixup) {
            m_list.remove(node, st);
            
            if (after_node.isNull()) {
                m_list.prepend(node, st);
            } else {
                m_list.insertAfter(node, after_node, st);
            }
        }
        
        assertValidHeap(st);
    }
    
    template <typename KeyType, typename Func>
    inline void findAllLesserOrEqual (KeyType key, Func func, State st = State())
    {
        Ref node = m_list.first(st);
        while (!node.isNull() && Compare::compareKeyEntry(st, key, node) >= 0) {
            func(static_cast<Ref>(node));
            node = m_list.next(node, st);
        }
    }
    
    template <typename KeyType>
    Ref findFirstLesserOrEqual (KeyType key, State st = State())
    {
        return next_lesser_or_equal(st, key, m_list.first(st));
    }
    
    template <typename KeyType>
    Ref findNextLesserOrEqual (KeyType key, Ref node, State st = State())
    {
        AMBRO_ASSERT(!node.isNull())
        
        return next_lesser_or_equal(st, key, m_list.next(node, st));
    }
    
    inline void assertValidHeap (State st = State())
    {
#if APRINTER_SORTED_LIST_VERIFY
        verifyHeap(st);
#endif
    }
    
    void verifyHeap (State st = State())
    {
        Ref node = m_list.first(st);
        if (node.isNull()) {
            return;
        }
        
        Ref next_node = m_list.next(node, st);
        while (!next_node.isNull()) {
            AMBRO_ASSERT_FORCE(Compare::compareEntries(st, node, next_node) <= 0)
            node = next_node;
            next_node = m_list.next(node, st);
        }
    }
    
private:
    template <typename KeyType>
    Ref next_lesser_or_equal (State st, KeyType key, Ref node)
    {
        if (!node.isNull() && Compare::compareKeyEntry(st, key, node) >= 0) {
            return node;
        }
        
        return Ref::null();
    }
};

struct SortedListService {
    template <typename LinkModel>
    using Node = SortedListNode<LinkModel>;
    
    template <typename Accessor, typename Compare, typename LinkModel>
    using Structure = SortedList<Accessor, Compare, LinkModel>;
};

}

#endif
