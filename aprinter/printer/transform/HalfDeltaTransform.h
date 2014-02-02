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

#include <aprinter/meta/WrapDouble.h>
#include <aprinter/math/Vector3.h>
#include <aprinter/printer/DistanceSplitter.h>

#include <aprinter/BeginNamespace.h>

template <
    typename TDiagonalRod,
    typename TTower1X,
    typename TTower2X,
    typename TSplitterParams
>
struct HalfDeltaTransformParams {
    using DiagonalRod = TDiagonalRod;
    using Tower1X = TTower1X;
    using Tower2X = TTower2X;
    using SplitterParams = TSplitterParams;
};

template <typename Params, typename FpType>
class HalfDeltaTransform {
private:
    static constexpr FpType square (FpType x)
    {
        return x * x;
    }
    
    using DiagonalRod2 = AMBRO_WRAP_DOUBLE(square(Params::DiagonalRod::value()));
    using MyVector = Vector3<FpType>;
    
public:
    static int const NumAxes = 2;
    
    template <typename Src, typename Dst>
    static void virtToPhys (Src virt, Dst out_phys)
    {
        out_phys.template set<0>(FloatSqrt((FpType)DiagonalRod2::value() - square((FpType)Params::Tower1X::value() - virt.template get<0>())) + virt.template get<1>());
        out_phys.template set<1>(FloatSqrt((FpType)DiagonalRod2::value() - square((FpType)Params::Tower2X::value() - virt.template get<0>())) + virt.template get<1>());
    }
    
    template <typename Src, typename Dst>
    static void physToVirt (Src phys, Dst out_virt)
    {
        MyVector p1 = MyVector::make((FpType)Params::Tower1X::value(), phys.template get<0>(), 0);
        MyVector p2 = MyVector::make((FpType)Params::Tower2X::value(), phys.template get<1>(), 0);
        MyVector pc = (p1 * 0.5f) + (p2 * 0.5f);
        MyVector v12 = p2 - p1;
        MyVector normal = MyVector::make(v12.m_v[1], -v12.m_v[0], 0);
        FpType d = FloatSqrt(((FpType)DiagonalRod2::value() / v12.norm()) - 0.25f);
        MyVector ps = pc + (normal * d);
        out_virt.template set<0>(ps.m_v[0]);
        out_virt.template set<1>(ps.m_v[1]);
    }
    
    using Splitter = DistanceSplitter<typename Params::SplitterParams, FpType>;
};

#include <aprinter/EndNamespace.h>

#endif
