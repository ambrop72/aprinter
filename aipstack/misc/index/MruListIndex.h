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

#ifndef APRINTER_IPSTACK_MRU_INDEX_H
#define APRINTER_IPSTACK_MRU_INDEX_H

#include <aprinter/base/Accessor.h>
#include <aprinter/structure/DoubleEndedList.h>

#include <aipstack/BeginNamespace.h>

template <
    typename Entry,
    typename HookAccessor,
    typename LookupKey,
    typename KeyFuncs
>
class MruListIndex {
    using ListNode = APrinter::DoubleEndedListNode<Entry>;
    
public:
    class Node {
        friend MruListIndex;
        
        ListNode list_node;
    };
    
    class Index {
        using ListNodeAccessor = APrinter::ComposedAccessor<
            HookAccessor,
            APRINTER_MEMBER_ACCESSOR_TN(&Node::list_node)
        >;
        using EntryList = APrinter::DoubleEndedListWithAccessor<Entry, ListNodeAccessor, false>;
        
    public:
        inline void init ()
        {
            m_list.init();
        }
        
        inline void addEntry (Entry &e)
        {
            m_list.prepend(&e);
        }
        
        inline void removeEntry (Entry &e)
        {
            m_list.remove(&e);
        }
        
        Entry * findEntry (LookupKey key)
        {
            for (Entry *ep = m_list.first(); ep != nullptr; ep = m_list.next(ep)) {
                Entry &e = *ep;
                if (KeyFuncs::GetKeyOfEntry(e) == key) {
                    if (&e != m_list.first()) {
                        m_list.remove(&e);
                        m_list.prepend(&e);
                    }
                    return &e;
                }
            }
            return nullptr;
        }
        
    private:
        EntryList m_list;
    };
};

#include <aipstack/EndNamespace.h>

#endif
