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

#ifndef APRINTER_IPSTACK_MRU_LIST_INDEX_H
#define APRINTER_IPSTACK_MRU_LIST_INDEX_H

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Accessor.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/structure/LinkModel.h>
#include <aprinter/structure/DoubleEndedList.h>

#include <aipstack/BeginNamespace.h>

template <typename Arg>
class MruListIndex {
    APRINTER_USE_TYPES1(Arg, (Entry, HookAccessor, LookupKeyArg, KeyFuncs,
                              LinkModel))
    
    // TODO: This really only works with PointerLinkModel, need to improve
    // DoubleEndedList to work with a LinkModel.
    APRINTER_USE_TYPES1(LinkModel, (State, Ref))
    
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
        
        inline void addEntry (State st, Ref e)
        {
            m_list.prepend(&*e);
        }
        
        inline void removeEntry (State st, Ref e)
        {
            m_list.remove(&*e);
        }
        
        Entry * findEntry (State st, LookupKeyArg key)
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

struct MruListIndexService {
    APRINTER_ALIAS_STRUCT_EXT(Index, (
        APRINTER_AS_TYPE(Entry),
        APRINTER_AS_TYPE(HookAccessor),
        APRINTER_AS_TYPE(LookupKeyArg),
        APRINTER_AS_TYPE(KeyFuncs),
        APRINTER_AS_TYPE(LinkModel)
    ), (
        APRINTER_DEF_INSTANCE(Index, MruListIndex)
    ))
};

#include <aipstack/EndNamespace.h>

#endif
