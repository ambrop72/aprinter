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

#ifndef AMBROLIB_DELTA_TRANSFORM_H
#define AMBROLIB_DELTA_TRANSFORM_H

#include <stdint.h>
#include <math.h>

#include <aprinter/meta/WrapDouble.h>
#include <aprinter/math/Vector3.h>

#include <aprinter/BeginNamespace.h>

template <
    typename TDiagonalRod,
    typename TTower1X,
    typename TTower1Y,
    typename TTower2X,
    typename TTower2Y,
    typename TTower3X,
    typename TTower3Y,
    typename TSplitLength
>
struct DeltaTransformParams {
    using DiagonalRod = TDiagonalRod;
    using Tower1X = TTower1X;
    using Tower1Y = TTower1Y;
    using Tower2X = TTower2X;
    using Tower2Y = TTower2Y;
    using Tower3X = TTower3X;
    using Tower3Y = TTower3Y;
    using SplitLength = TSplitLength;
};

template <typename Params>
class DeltaTransform {
private:
    static constexpr double square (double x)
    {
        return x * x;
    }
    
    using DiagonalRod2 = AMBRO_WRAP_DOUBLE(square(Params::DiagonalRod::value()));
    
public:
    static int const NumAxes = 3;
    
    static void virtToPhys (double const *virt, double *out_phys)
    {
        out_phys[0] = sqrt(DiagonalRod2::value() - square(Params::Tower1X::value() - virt[0]) - square(Params::Tower1Y::value() - virt[1])) + virt[2];
        out_phys[1] = sqrt(DiagonalRod2::value() - square(Params::Tower2X::value() - virt[0]) - square(Params::Tower2Y::value() - virt[1])) + virt[2];
        out_phys[2] = sqrt(DiagonalRod2::value() - square(Params::Tower3X::value() - virt[0]) - square(Params::Tower3Y::value() - virt[1])) + virt[2];
    }
    
    static void physToVirt (double const *phys, double *out_virt)
    {
        Vector3 p1 = Vector3::make(Params::Tower1X::value(), Params::Tower1Y::value(), phys[0]);
        Vector3 p2 = Vector3::make(Params::Tower2X::value(), Params::Tower2Y::value(), phys[1]);
        Vector3 p3 = Vector3::make(Params::Tower3X::value(), Params::Tower3Y::value(), phys[2]);
        Vector3 normal = (p1 - p2).cross(p2 - p3);
        double k = 1.0 / normal.norm();
        double q = 0.5 * k;
        double a = q * (p2 - p3).norm() * (p1 - p2).dot(p1 - p3);
        double b = q * (p1 - p3).norm() * (p2 - p1).dot(p2 - p3);
        double c = q * (p1 - p2).norm() * (p3 - p1).dot(p3 - p2);
        Vector3 pc = (p1 * a) + (p2 * b) + (p3 * c);
        double r2 = 0.25 * k * (p1 - p2).norm() * (p2 - p3).norm() * (p3 - p1).norm();
        double d = sqrt(k * (DiagonalRod2::value() - r2));
        Vector3 ps = pc - (normal * d);
        out_virt[0] = ps.m_v[0];
        out_virt[1] = ps.m_v[1];
        out_virt[2] = ps.m_v[2];
    }
    
    class Splitter {
    public:
        void start (double const *old_virt, double const *virt)
        {
            double dist = sqrt(square(old_virt[0] - virt[0]) + square(old_virt[1] - virt[1]) + square(old_virt[2] - virt[2]));
            m_count = 1 + (uint32_t)(dist * (1.0 / Params::SplitLength::value()));
            m_pos = 1;
        }
        
        bool pull (double *out_frac)
        {
            if (m_pos == m_count) {
                return false;
            }
            *out_frac = (double)m_pos / m_count;
            m_pos++;
            return true;
        }
        
        uint32_t m_count;
        uint32_t m_pos;
    };
};

#include <aprinter/EndNamespace.h>

#endif
