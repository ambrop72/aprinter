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
#include <aprinter/printer/DistanceSplitter.h>

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
    
    template <typename Src, typename Dst>
    static void virtToPhys (Src virt, Dst out_phys)
    {
        out_phys.template set<0>(sqrt(DiagonalRod2::value() - square(Params::Tower1X::value() - virt.template get<0>()) - square(Params::Tower1Y::value() - virt.template get<1>())) + virt.template get<2>());
        out_phys.template set<1>(sqrt(DiagonalRod2::value() - square(Params::Tower2X::value() - virt.template get<0>()) - square(Params::Tower2Y::value() - virt.template get<1>())) + virt.template get<2>());
        out_phys.template set<2>(sqrt(DiagonalRod2::value() - square(Params::Tower3X::value() - virt.template get<0>()) - square(Params::Tower3Y::value() - virt.template get<1>())) + virt.template get<2>());
    }
    
    template <typename Src, typename Dst>
    static void physToVirt (Src phys, Dst out_virt)
    {
        Vector3 p1 = Vector3::make(Params::Tower1X::value(), Params::Tower1Y::value(), phys.template get<0>());
        Vector3 p2 = Vector3::make(Params::Tower2X::value(), Params::Tower2Y::value(), phys.template get<1>());
        Vector3 p3 = Vector3::make(Params::Tower3X::value(), Params::Tower3Y::value(), phys.template get<2>());
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
        out_virt.template set<0>(ps.m_v[0]);
        out_virt.template set<1>(ps.m_v[1]);
        out_virt.template set<2>(ps.m_v[2]);
    }
    
    using Splitter = DistanceSplitter<typename Params::SplitLength>;
};

#include <aprinter/EndNamespace.h>

#endif
