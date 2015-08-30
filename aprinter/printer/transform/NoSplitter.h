/*
 * Copyright (c) 2013 Ambroz Bizjak
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

#ifndef AMBROLIB_NO_SPLITTER_H
#define AMBROLIB_NO_SPLITTER_H

#include <aprinter/BeginNamespace.h>

template <typename Context, typename FpType>
class NoSplitter {
public:
    class Splitter {
    public:
        void start (Context c, FpType distance, FpType base_max_v_rec, FpType time_freq_by_max_speed)
        {
            m_max_v_rec = base_max_v_rec;
        }
        
        bool pull (Context c, FpType *out_rel_max_v_rec, FpType *out_frac)
        {
            *out_rel_max_v_rec = m_max_v_rec;
            return false;
        }
        
    private:
        FpType m_max_v_rec;
    };
    
public:
    struct Object {};
};

struct NoSplitterService {
    template <typename Context, typename ParentObject, typename Config, typename FpType>
    using Splitter = NoSplitter<Context, FpType>;
};

#include <aprinter/EndNamespace.h>

#endif
