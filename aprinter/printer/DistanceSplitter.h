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

#ifndef AMBROLIB_DISTANCE_SPLITTER_H
#define AMBROLIB_DISTANCE_SPLITTER_H

#include <stdint.h>

#include <aprinter/BeginNamespace.h>

template <typename SplitLength>
class DistanceSplitter {
public:
    void start (double distance, double time_freq_by_max_speed)
    {
        m_count = 1 + (uint32_t)(distance * (1.0 / SplitLength::value()));
        m_pos = 1;
        m_max_v_rec = (distance * time_freq_by_max_speed) / m_count;
    }
    
    bool pull (double *out_rel_max_v_rec, double *out_frac)
    {
        *out_rel_max_v_rec = m_max_v_rec;
        if (m_pos == m_count) {
            return false;
        }
        *out_frac = (double)m_pos / m_count;
        m_pos++;
        return true;
    }
    
private:
    uint32_t m_count;
    uint32_t m_pos;
    double m_max_v_rec;
};

#include <aprinter/EndNamespace.h>

#endif
