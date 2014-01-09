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

#ifndef AMBROLIB_HALF_DELTA_TRANSFORM_H
#define AMBROLIB_HALF_DELTA_TRANSFORM_H

#include <stdint.h>
#include <math.h>

#include <aprinter/meta/WrapDouble.h>
#include <aprinter/math/Vector3.h>
#include <aprinter/printer/DistanceSplitter.h>

#include <aprinter/BeginNamespace.h>

template <
    typename TDiagonalRod,
    typename TTower1X,
    typename TTower2X,
    typename TSplitLength
>
struct HalfDeltaTransformParams {
    using DiagonalRod = TDiagonalRod;
    using Tower1X = TTower1X;
    using Tower2X = TTower2X;
    using SplitLength = TSplitLength;
};

template <typename Params>
class HalfDeltaTransform {
private:
    static constexpr double square (double x)
    {
        return x * x;
    }
    
    using DiagonalRod2 = AMBRO_WRAP_DOUBLE(square(Params::DiagonalRod::value()));
    
public:
    static int const NumAxes = 2;
    
    template <typename Src, typename Dst>
    static void virtToPhys (Src virt, Dst out_phys)
    {
        out_phys.template set<0>(sqrt(DiagonalRod2::value() - square(Params::Tower1X::value() - virt.template get<0>())) + virt.template get<1>());
        out_phys.template set<1>(sqrt(DiagonalRod2::value() - square(Params::Tower2X::value() - virt.template get<0>())) + virt.template get<1>());
    }
    
    template <typename Src, typename Dst>
    static void physToVirt (Src phys, Dst out_virt)
    {
        Vector3 p1 = Vector3::make(Params::Tower1X::value(), phys.template get<0>(), 0);
        Vector3 p2 = Vector3::make(Params::Tower2X::value(), phys.template get<1>(), 0);
        Vector3 pc = (p1 * 0.5) + (p2 * 0.5);
        Vector3 v12 = p2 - p1;
        Vector3 normal = Vector3::make(v12.m_v[1], -v12.m_v[0], 0);
        double d = sqrt((DiagonalRod2::value() / v12.norm()) - 0.25);
        Vector3 ps = pc + (normal * d);
        out_virt.template set<0>(ps.m_v[0]);
        out_virt.template set<1>(ps.m_v[1]);
    }
    
    using Splitter = DistanceSplitter<typename Params::SplitLength>;
};

#include <aprinter/EndNamespace.h>

#endif
