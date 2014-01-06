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

#ifndef AMBROLIB_IDENTITY_TRANSFORM_H
#define AMBROLIB_IDENTITY_TRANSFORM_H

#include <math.h>

#include <aprinter/BeginNamespace.h>

template <int TNumAxes, uint32_t TSplit>
struct IdentityTransformParams {
    static int const NumAxes = TNumAxes;
    static uint32_t const Split = TSplit;
};

template <typename Params>
class IdentityTransform {
public:
    static int const NumAxes = Params::NumAxes;
    
    static void virtToPhys (double const *virt, double *out_phys)
    {
        for (int i = 0; i < NumAxes; i++) {
            out_phys[i] = virt[i];
        }
    }
    
    static void physToVirt (double const *phys, double *out_virt)
    {
        for (int i = 0; i < NumAxes; i++) {
            out_virt[i] = phys[i];
        }
    }
    
    class Splitter {
    public:
        void start (double const *old_virt, double const *virt)
        {
            if (Params::Split != (uint32_t)-1) {
                double dist = 0.0;
                for (int i = 0; i < NumAxes; i++) {
                    dist = fmax(dist, fabs(old_virt[i] - virt[i]));
                }
                m_count = 1 + (uint32_t)(dist * (1.0 / (Params::Split / 1000.0)));
                m_pos = 1;
            }
        }
        
        bool pull (double *out_frac)
        {
            if (Params::Split == (uint32_t)-1) {
                return false;
            } else {
                if (m_pos == m_count) {
                    return false;
                }
                *out_frac = (double)m_pos / m_count;
                m_pos++;
                return true;
            }
        }
        
        uint32_t m_count;
        uint32_t m_pos;
    };
};

#include <aprinter/EndNamespace.h>

#endif
