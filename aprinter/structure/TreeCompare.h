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

#ifndef APRINTER_TREE_COMPARE_H
#define APRINTER_TREE_COMPARE_H

namespace APrinter {

template <
    typename LinkModel,
    typename KeyFuncs
>
class TreeCompare {
    using State = typename LinkModel::State;
    using Ref = typename LinkModel::Ref;
    
public:
    inline static int compareEntries (State, Ref ref1, Ref ref2)
    {
        decltype(auto) key1 = KeyFuncs::GetKeyOfEntry(*ref1);
        decltype(auto) key2 = KeyFuncs::GetKeyOfEntry(*ref2);
        
        return KeyFuncs::CompareKeys(key1, key2);
    }
    
    template <typename Key>
    inline static int compareKeyEntry (State, Key const &key1, Ref ref2)
    {
        decltype(auto) key2 = KeyFuncs::GetKeyOfEntry(*ref2);
        
        return KeyFuncs::CompareKeys(key1, key2);
    }
};

}

#endif
