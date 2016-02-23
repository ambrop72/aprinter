/*
 * Copyright (c) 2015 Ambroz Bizjak
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

#ifndef AMBROLIB_WRAP_BUFFER_H
#define AMBROLIB_WRAP_BUFFER_H

#include <stddef.h>
#include <string.h>

#include <aprinter/meta/MinMax.h>
#include <aprinter/base/MemRef.h>

#include <aprinter/BeginNamespace.h>

struct WrapBuffer {
    WrapBuffer () = default;
    
    inline WrapBuffer (size_t wrap_arg, char *ptr1_arg, char *ptr2_arg)
    : wrap(wrap_arg), ptr1(ptr1_arg), ptr2(ptr2_arg)
    {}
    
    inline WrapBuffer (char *ptr)
    : wrap((size_t)-1), ptr1(ptr), ptr2(nullptr)
    {}
    
    inline void copyIn (MemRef data) const
    {
        size_t first_length = MinValue(data.len, wrap);
        memcpy(ptr1, data.ptr, first_length);
        if (first_length < data.len) {
            memcpy(ptr2, data.ptr + first_length, data.len - first_length);
        }
    }
    
    inline void copyOut (MemRef data) const
    {
        size_t first_length = MinValue(data.len, wrap);
        memcpy((char *)data.ptr, ptr1, first_length);
        if (first_length < data.len) {
            memcpy((char *)data.ptr + first_length, ptr2, data.len - first_length);
        }
    }
    
    inline WrapBuffer subFrom (size_t offset) const
    {
        if (offset < wrap) {
            return WrapBuffer(wrap - offset, ptr1 + offset, ptr2);
        } else {
            return WrapBuffer(ptr2 + (offset - wrap));
        }
    }
    
    size_t wrap;
    char *ptr1;
    char *ptr2;
};

#include <aprinter/EndNamespace.h>

#endif
