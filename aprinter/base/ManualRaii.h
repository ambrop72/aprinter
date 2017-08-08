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

#ifndef APRINTER_MANUAL_RAII_H
#define APRINTER_MANUAL_RAII_H

#include <utility>
#include <new>

namespace APrinter {

template <typename ObjectType>
class ManualRaii
{
    union ObjectUnion {
        ObjectUnion () {}
        ~ObjectUnion () {}
        
        ObjectType object;
    };
    
private:
    ObjectUnion m_union;
    
public:
    template <typename... Args>
    inline void construct (Args && ... args)
    {
        new(&m_union.object) ObjectType(std::forward<Args>(args)...);
    }
    
    inline void destruct ()
    {
        m_union.object.~ObjectType();
    }
    
    inline ObjectType & operator* ()
    {
        return m_union.object;
    }
    
    inline ObjectType const & operator* () const
    {
        return m_union.object;
    }
    
    inline ObjectType * operator-> ()
    {
        return &m_union.object;
    }
    
    inline ObjectType const * operator-> () const
    {
        return &m_union.object;
    }
};

}

#endif
