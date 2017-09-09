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

#ifndef AIPSTACK_MRU_LIST_INDEX_H
#define AIPSTACK_MRU_LIST_INDEX_H

#include <aipstack/meta/Instance.h>
#include <aipstack/misc/Accessor.h>
#include <aipstack/misc/Preprocessor.h>
#include <aipstack/structure/LinkedList.h>

namespace AIpStack {

template <typename Arg>
class MruListIndex {
    AIPSTACK_USE_TYPES1(Arg, (HookAccessor, LookupKeyArg, KeyFuncs, LinkModel))
    
    AIPSTACK_USE_TYPES1(LinkModel, (State, Ref))
    
    using ListNode = LinkedListNode<LinkModel>;
    
public:
    class Node {
        friend MruListIndex;
        
        ListNode list_node;
    };
    
    class Index {
        using ListNodeAccessor = ComposedAccessor<
            HookAccessor,
            AIPSTACK_MEMBER_ACCESSOR_TN(&Node::list_node)
        >;
        using EntryList = LinkedList<ListNodeAccessor, LinkModel, false>;
        
    public:
        inline void init ()
        {
            m_list.init();
        }
        
        inline void addEntry (Ref e, State st = State())
        {
            m_list.prepend(e, st);
        }
        
        inline void removeEntry (Ref e, State st = State())
        {
            m_list.remove(e, st);
        }
        
        Ref findEntry (LookupKeyArg key, State st = State())
        {
            for (Ref e = m_list.first(st); !e.isNull(); e = m_list.next(e, st)) {
                if (KeyFuncs::KeysAreEqual(KeyFuncs::GetKeyOfEntry(*e), key)) {
                    if (!(e == m_list.first(st))) {
                        m_list.remove(e, st);
                        m_list.prepend(e, st);
                    }
                    return e;
                }
            }
            return Ref::null();
        }
        
        inline Ref first (State st = State())
        {
            return m_list.first(st);
        }
        
        inline Ref next (Ref node, State st = State())
        {
            return m_list.next(node, st);
        }
        
    private:
        EntryList m_list;
    };
};

struct MruListIndexService {
    template <typename HookAccessor_, typename LookupKeyArg_,
              typename KeyFuncs_, typename LinkModel_>
    struct Index {
        using HookAccessor = HookAccessor_;
        using LookupKeyArg = LookupKeyArg_;
        using KeyFuncs = KeyFuncs_;
        using LinkModel = LinkModel_;
        AIPSTACK_DEF_INSTANCE(Index, MruListIndex)
    };
};

}

#endif
