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

#ifndef AIPSTACK_AVL_TREE_INDEX_H
#define AIPSTACK_AVL_TREE_INDEX_H

#include <aprinter/meta/Instance.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Accessor.h>
#include <aprinter/base/Assert.h>
#include <aprinter/structure/AvlTree.h>
#include <aprinter/structure/TreeCompare.h>

namespace AIpStack {

template <typename Arg>
class AvlTreeIndex {
    APRINTER_USE_TYPES1(Arg, (HookAccessor, LookupKeyArg, KeyFuncs, LinkModel))
    
    APRINTER_USE_TYPES1(LinkModel, (State, Ref))
    
    using TreeNode = APrinter::AvlTreeNode<LinkModel>;
    
public:
    class Node {
        friend AvlTreeIndex;
        
        TreeNode tree_node;
    };
    
    class Index {
        using TreeNodeAccessor = APrinter::ComposedAccessor<
            HookAccessor,
            APRINTER_MEMBER_ACCESSOR_TN(&Node::tree_node)
        >;
        
        struct TheTreeCompare : public APrinter::TreeCompare<LinkModel, KeyFuncs> {};
        
        using EntryTree = APrinter::AvlTree<TreeNodeAccessor, TheTreeCompare, LinkModel>;
        
    public:
        inline void init ()
        {
            m_tree.init();
        }
        
        inline void addEntry (Ref e, State st = State())
        {
            bool inserted = m_tree.insert(e, nullptr, st);
            AMBRO_ASSERT(inserted)
        }
        
        inline void removeEntry (Ref e, State st = State())
        {
            m_tree.remove(e, st);
        }
        
        inline Ref findEntry (LookupKeyArg key, State st = State())
        {
            Ref entry = m_tree.template lookup<LookupKeyArg>(key, st);
            AMBRO_ASSERT(entry.isNull() ||
                         KeyFuncs::KeysAreEqual(KeyFuncs::GetKeyOfEntry(*entry), key))
            return entry;
        }
        
        inline Ref first (State st = State())
        {
            return m_tree.first(st);
        }
        
        inline Ref next (Ref node, State st = State())
        {
            return m_tree.next(node, st);
        }
        
    private:
        EntryTree m_tree;
    };
};

struct AvlTreeIndexService {
    template <typename HookAccessor_, typename LookupKeyArg_,
              typename KeyFuncs_, typename LinkModel_>
    struct Index {
        using HookAccessor = HookAccessor_;
        using LookupKeyArg = LookupKeyArg_;
        using KeyFuncs = KeyFuncs_;
        using LinkModel = LinkModel_;
        APRINTER_DEF_INSTANCE(Index, AvlTreeIndex)
    };
};

}

#endif
