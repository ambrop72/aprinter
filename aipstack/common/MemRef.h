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

#ifndef AIPSTACK_MEMREF_H
#define AIPSTACK_MEMREF_H

#include <stddef.h>
#include <string.h>

#include <aipstack/misc/Assert.h>

namespace AIpStack {

struct MemRef {
    char const *ptr;
    size_t len;
    
    MemRef () = default;
    
    inline MemRef (char const *ptr_arg, size_t len_arg)
    : ptr(ptr_arg), len(len_arg)
    {
    }
    
    inline MemRef (char const *cstr)
    : ptr(cstr), len(strlen(cstr))
    {
    }
    
    inline static MemRef Null ()
    {
        return MemRef(nullptr, 0);
    }
    
    inline char at (size_t pos) const
    {
        AIPSTACK_ASSERT(ptr)
        AIPSTACK_ASSERT(pos < len)
        
        return ptr[pos];
    }
    
    inline MemRef subFrom (size_t offset) const
    {
        AIPSTACK_ASSERT(ptr)
        AIPSTACK_ASSERT(offset <= len)
        
        return MemRef(ptr + offset, len - offset);
    }
    
    inline MemRef subTo (size_t offset) const
    {
        AIPSTACK_ASSERT(ptr)
        AIPSTACK_ASSERT(offset <= len)
        
        return MemRef(ptr, offset);
    }
    
    inline bool equalTo (MemRef other) const
    {
        AIPSTACK_ASSERT(ptr)
        AIPSTACK_ASSERT(other.ptr)
        
        return len == other.len && !memcmp(ptr, other.ptr, len);
    }
    
    bool removePrefix (char const *prefix)
    {
        size_t pos = 0;
        while (prefix[pos] != '\0') {
            if (pos == len || ptr[pos] != prefix[pos]) {
                return false;
            }
            pos++;
        }
        *this = subFrom(pos);
        return true;
    }
};

}

#endif
