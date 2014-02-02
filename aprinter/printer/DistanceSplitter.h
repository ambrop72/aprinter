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

#include <aprinter/math/FloatTools.h>
#include <aprinter/meta/PowerOfTwo.h>

#include <aprinter/BeginNamespace.h>

template <typename TMinSplitLength, typename TMaxSplitLength>
struct DistanceSplitterParams {
    using MinSplitLength = TMinSplitLength;
    using MaxSplitLength = TMaxSplitLength;
};

template <typename Params, typename FpType>
class DistanceSplitter {
public:
    void start (FpType distance, FpType base_max_v_rec, FpType num_segments_by_distance)
    {
        FpType fpcount = distance * FloatMin((FpType)(1.0 / Params::MinSplitLength::value()), FloatMax((FpType)(1.0 / Params::MaxSplitLength::value()), num_segments_by_distance));
        if (fpcount >= FloatLdexp<FpType>(1.0f, 31)) {
            m_count = PowerOfTwo<uint32_t, 31>::value;
        } else {
            m_count = 1 + (uint32_t)fpcount;
        }
        m_pos = 1;
        m_max_v_rec = base_max_v_rec / m_count;
    }
    
    bool pull (FpType *out_rel_max_v_rec, FpType *out_frac)
    {
        *out_rel_max_v_rec = m_max_v_rec;
        if (m_pos == m_count) {
            return false;
        }
        *out_frac = (FpType)m_pos / m_count;
        m_pos++;
        return true;
    }
    
private:
    uint32_t m_count;
    uint32_t m_pos;
    FpType m_max_v_rec;
};

#include <aprinter/EndNamespace.h>

#endif
