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

#ifndef APRINTER_AVL_TREE_H
#define APRINTER_AVL_TREE_H

#include <stdint.h>

#include <algorithm>

#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

//#define APRINTER_AVL_TREE_VERIFY 1

template<typename, typename, typename>
class AvlTree;

template <typename LinkModel>
class AvlTreeNode {
    template<typename, typename, typename>
    friend class AvlTree;
    
    using Link = typename LinkModel::Link;
    
private:
    Link parent;
    Link child[2];
    int8_t balance;
};

template <
    typename Accessor,
    typename Compare,
    typename LinkModel
>
class AvlTree {
    using Link = typename LinkModel::Link;
    
private:
    Link m_root;
    
public:
    using State = typename LinkModel::State;
    using Ref = typename LinkModel::Ref;
    
    inline void init ()
    {
        m_root = Link::null();
    }
    
    bool insert (Ref node, Ref *out_ref, State st = State())
    {
        AMBRO_ASSERT(!node.isNull())
        
        if (m_root.isNull()) {
            m_root = node.link(st);
            
            ac(node).parent = Link::null();
            ac(node).child[0] = Link::null();
            ac(node).child[1] = Link::null();
            ac(node).balance = 0;
            
            assert_tree(st);
            
            if (out_ref != nullptr) {
                *out_ref = Ref::null();
            }
            return true;
        }
        
        Ref c = m_root.ref(st);
        bool side;
        
        while (true) {
            int comp = Compare::compareEntries(st, node, c);
            
            if (comp == 0) {
                if (out_ref != nullptr) {
                    *out_ref = c;
                }
                return false;
            }
            
            side = (comp == 1);
            
            if (ac(c).child[side].isNull()) {
                break;
            }
            
            c = ac(c).child[side].ref(st);
        }
        
        ac(c).child[side] = node.link(st);
        
        ac(node).parent = c.link(st);
        ac(node).child[0] = Link::null();
        ac(node).child[1] = Link::null();
        ac(node).balance = 0;
        
        rebalance(st, c, side, 1);
        
        assert_tree(st);
        
        if (out_ref != nullptr) {
            *out_ref = c;
        }
        return true;
    }
    
    void remove (Ref node, State st = State())
    {
        AMBRO_ASSERT(!node.isNull())
        AMBRO_ASSERT(!m_root.isNull())
        
        if (!ac(node).child[0].isNull() && !ac(node).child[1].isNull()) {
            Ref max_node = subtree_max(st, ac(node).child[0].ref(st));
            swap_for_remove(st, node, max_node, ac(node).parent.ref(st), ac(max_node).parent.ref(st));
        }
        
        AMBRO_ASSERT(ac(node).child[0].isNull() || ac(node).child[1].isNull())
        
        Ref paren = ac(node).parent.ref(st);
        Ref child = !ac(node).child[0].isNull() ? ac(node).child[0].ref(st) : ac(node).child[1].ref(st);
        
        if (!paren.isNull()) {
            bool side = node.link(st) == ac(paren).child[1];
            replace_subtree(st, node, child, paren);
            rebalance(st, paren, side, -1);
        } else {
            replace_subtree(st, node, child, paren);
        }
        
        assert_tree(st);
    }
    
    template <typename KeyType, typename LookupCompare = Compare>
    Ref lookup (KeyType key, State st = State())
    {
        if (m_root.isNull()) {
            return Ref::null();
        }
        
        Ref c = m_root.ref(st);
        
        while (true) {
            // compare
            int comp = LookupCompare::compareKeyEntry(st, key, c);
            
            // have we found a node that compares equal?
            if (comp == 0) {
                return c;
            }
            
            bool side = (comp == 1);
            
            // have we reached a leaf?
            if (ac(c).child[side].isNull()) {
                return Ref::null();
            }
            
            c = ac(c).child[side].ref(st);
        }
    }
    
private:
    inline static AvlTreeNode<LinkModel> & ac (Ref ref)
    {
        return Accessor::access(*ref);
    }
    
    inline static int8_t optneg (int8_t x, bool neg)
    {
        return neg ? -x : x;
    }
    
    void rebalance (State st, Ref node, bool side, int8_t deltac)
    {
        AMBRO_ASSERT(deltac >= -1 && deltac <= 1)
        AMBRO_ASSERT(ac(node).balance >= -1 && ac(node).balance <= 1)
        
        // if no subtree changed its height, no more rebalancing is needed
        if (deltac == 0) {
            return;
        }
        
        // calculate how much our height changed
        int8_t rel_balance = optneg(ac(node).balance, side);
        int8_t delta = std::max<int8_t>(deltac, rel_balance) - std::max<int8_t>(0, rel_balance);
        AMBRO_ASSERT(delta >= -1 && delta <= 1)
        
        // update our balance factor
        ac(node).balance -= optneg(deltac, side);
        
        Ref child;
        Ref gchild;
        
        // perform transformations if the balance factor is wrong
        if (ac(node).balance == 2 || ac(node).balance == -2) {
            bool bside;
            int8_t bsidef;
            if (ac(node).balance == 2) {
                bside = true;
                bsidef = 1;
            } else {
                bside = false;
                bsidef = -1;
            }
            
            AMBRO_ASSERT(!ac(node).child[bside].isNull())
            child = ac(node).child[bside].ref(st);
            
            switch (ac(child).balance * bsidef) {
                case 1:
                    rotate(st, node, !bside, ac(node).parent.ref(st));
                    ac(node).balance = 0;
                    ac(child).balance = 0;
                    node = child;
                    delta -= 1;
                    break;
                case 0:
                    rotate(st, node, !bside, ac(node).parent.ref(st));
                    ac(node).balance = bsidef;
                    ac(child).balance = -bsidef;
                    node = child;
                    break;
                case -1:
                    AMBRO_ASSERT(!ac(child).child[!bside].isNull())
                    gchild = ac(child).child[!bside].ref(st);
                    rotate(st, child, bside, node);
                    rotate(st, node, !bside, ac(node).parent.ref(st));
                    ac(node).balance = -std::max<int8_t>(0, ac(gchild).balance * bsidef) * bsidef;
                    ac(child).balance = std::max<int8_t>(0, -ac(gchild).balance * bsidef) * bsidef;
                    ac(gchild).balance = 0;
                    node = gchild;
                    delta -= 1;
                    break;
                default:
                    AMBRO_ASSERT(false);
            }
        }
        
        AMBRO_ASSERT(delta >= -1 && delta <= 1)
        // Transformations above preserve this. Proof:
        //     - if a child subtree gained 1 height and rebalancing was needed,
        //       it was the heavier subtree. Then delta was was originally 1, because
        //       the heaviest subtree gained one height. If the transformation reduces
        //       delta by one, it becomes 0.
        //     - if a child subtree lost 1 height and rebalancing was needed, it
        //       was the lighter subtree. Then delta was originally 0, because
        //       the height of the heaviest subtree was unchanged. If the transformation
        //       reduces delta by one, it becomes -1.
        
        if (!ac(node).parent.isNull()) {
            Ref node_parent = ac(node).parent.ref(st);
            rebalance(st, node_parent, node.link(st) == ac(node_parent).child[1], delta);
        }
    }
    
    void rotate (State st, Ref r, bool dir, Ref r_parent)
    {
        AMBRO_ASSERT(check_parent(st, r_parent, r))
        Ref nr = ac(r).child[!dir].ref(st);
        
        ac(r).child[!dir] = ac(nr).child[dir];
        if (!ac(r).child[!dir].isNull()) {
            ac(ac(r).child[!dir].ref(st)).parent = r.link(st);
        }
        ac(nr).child[dir] = r.link(st);
        ac(nr).parent = r_parent.link(st);
        if (!r_parent.isNull()) {
            ac(r_parent).child[r.link(st) == ac(r_parent).child[1]] = nr.link(st);
        } else {
            m_root = nr.link(st);
        }
        ac(r).parent = nr.link(st);
    }
    
    static Ref subtree_max (State st, Ref n)
    {
        AMBRO_ASSERT(!n.isNull())
        
        while (!ac(n).child[1].isNull()) {
            n = ac(n).child[1].ref(st);
        }
        return n;
    }
    
    void swap_for_remove (State st, Ref node, Ref enode, Ref node_parent, Ref enode_parent)
    {
        AMBRO_ASSERT(check_parent(st, node_parent, node))
        AMBRO_ASSERT(check_parent(st, enode_parent, enode))
        
        if (enode_parent.link(st) == node.link(st)) {
            // when the nodes are directly connected we need special handling
            
            bool side = enode.link(st) == ac(node).child[1];
            Ref c = ac(node).child[!side].ref(st);
            
            if (!(ac(node).child[0] = ac(enode).child[0]).isNull()) {
                ac(ac(node).child[0].ref(st)).parent = node.link(st);
            }
            if (!(ac(node).child[1] = ac(enode).child[1]).isNull()) {
                ac(ac(node).child[1].ref(st)).parent = node.link(st);
            }
            
            ac(enode).parent = ac(node).parent;
            if (!node_parent.isNull()) {
                ac(node_parent).child[node.link(st) == ac(node_parent).child[1]] = enode.link(st);
            } else {
                m_root = enode.link(st);
            }
            
            ac(enode).child[side] = node.link(st);
            ac(node).parent = enode.link(st);
            if (!(ac(enode).child[!side] = c.link(st)).isNull()) {
                ac(c).parent = enode.link(st);
            }
        } else {
            Ref temp;
            
            // swap parents
            temp = node_parent;
            ac(node).parent = ac(enode).parent;
            if (!enode_parent.isNull()) {
                ac(enode_parent).child[enode.link(st) == ac(enode_parent).child[1]] = node.link(st);
            } else {
                m_root = node.link(st);
            }
            ac(enode).parent = temp.link(st);
            if (!temp.isNull()) {
                ac(temp).child[node.link(st) == ac(temp).child[1]] = enode.link(st);
            } else {
                m_root = enode.link(st);
            }
            
            // swap left children
            temp = ac(node).child[0].ref(st);
            if (!(ac(node).child[0] = ac(enode).child[0]).isNull()) {
                ac(ac(node).child[0].ref(st)).parent = node.link(st);
            }
            if (!(ac(enode).child[0] = temp.link(st)).isNull()) {
                ac(ac(enode).child[0].ref(st)).parent = enode.link(st);
            }
            
            // swap right children
            temp = ac(node).child[1].ref(st);
            if (!(ac(node).child[1] = ac(enode).child[1]).isNull()) {
                ac(ac(node).child[1].ref(st)).parent = node.link(st);
            }
            if (!(ac(enode).child[1] = temp.link(st)).isNull()) {
                ac(ac(enode).child[1].ref(st)).parent = enode.link(st);
            }
        }
        
        // swap balance factors
        int8_t b = ac(node).balance;
        ac(node).balance = ac(enode).balance;
        ac(enode).balance = b;
    }
    
    void replace_subtree (State st, Ref dest, Ref n, Ref dest_parent)
    {
        AMBRO_ASSERT(!dest.isNull())
        AMBRO_ASSERT(check_parent(st, dest_parent, dest))
        
        if (!dest_parent.isNull()) {
            ac(dest_parent).child[dest.link(st) == ac(dest_parent).child[1]] = n.link(st);
        } else {
            m_root = n.link(st);
        }
        
        if (!n.isNull()) {
            ac(n).parent = ac(dest).parent;
        }
    }
    
    static bool check_parent (State st, Ref p, Ref c)
    {
        return (p.link(st) == ac(c).parent) &&
               (p.isNull() || c.link(st) == ac(p).child[0] || c.link(st) == ac(p).child[1]);
    }
    
    void assert_tree (State st)
    {
#if APRINTER_AVL_TREE_VERIFY
        verify_tree(st);
#endif
    }
    
#if APRINTER_AVL_TREE_VERIFY
    void verify_tree (State st)
    {
        if (!m_root.isNull()) {
            Ref root = m_root.ref(st);
            AMBRO_ASSERT_FORCE(ac(root).parent.isNull())
            verify_recurser(st, root);
        }
    }
    
    int verify_recurser (State st, Ref n)
    {
        AMBRO_ASSERT_FORCE(ac(n).balance >= -1)
        AMBRO_ASSERT_FORCE(ac(n).balance <= 1)
        
        int height_left = 0;
        int height_right = 0;
        
        // check left subtree
        if (!ac(n).child[0].isNull()) {
            // check parent link
            AMBRO_ASSERT_FORCE(ac(ac(n).child[0].ref(st)).parent == n.link(st))
            // check binary search tree
            AMBRO_ASSERT_FORCE(Compare::compareEntries(st, ac(n).child[0].ref(st), n) == -1)
            // recursively calculate height
            height_left = verify_recurser(st, ac(n).child[0].ref(st));
        }
        
        // check right subtree
        if (!ac(n).child[1].isNull()) {
            // check parent link
            AMBRO_ASSERT_FORCE(ac(ac(n).child[1].ref(st)).parent == n.link(st))
            // check binary search tree
            AMBRO_ASSERT_FORCE(Compare::compareEntries(st, ac(n).child[1].ref(st), n) == 1)
            // recursively calculate height
            height_right = verify_recurser(st, ac(n).child[1].ref(st));
        }
        
        // check balance factor
        AMBRO_ASSERT_FORCE(ac(n).balance == height_right - height_left)
        
        return std::max(height_left, height_right) + 1;
    }
#endif
};

#include <aprinter/EndNamespace.h>

#endif
