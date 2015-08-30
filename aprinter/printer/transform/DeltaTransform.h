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

#include <aprinter/meta/WrapDouble.h>
#include <aprinter/base/Object.h>
#include <aprinter/math/Vector3.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/printer/Configuration.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename Config, typename FpType, typename Params>
class DeltaTransform {
private:
    using MyVector = Vector3<FpType>;
    
public:
    static int const NumAxes = 3;
    
    template <typename Src, typename Dst>
    static void virtToPhys (Context c, Src virt, Dst out_phys)
    {
        out_phys.template set<0>(FloatSqrt(APRINTER_CFG(Config, CDiagonalRod2, c) - FloatSquare(APRINTER_CFG(Config, CTower1X, c) - virt.template get<0>()) - FloatSquare(APRINTER_CFG(Config, CTower1Y, c) - virt.template get<1>())) + virt.template get<2>());
        out_phys.template set<1>(FloatSqrt(APRINTER_CFG(Config, CDiagonalRod2, c) - FloatSquare(APRINTER_CFG(Config, CTower2X, c) - virt.template get<0>()) - FloatSquare(APRINTER_CFG(Config, CTower2Y, c) - virt.template get<1>())) + virt.template get<2>());
        out_phys.template set<2>(FloatSqrt(APRINTER_CFG(Config, CDiagonalRod2, c) - FloatSquare(APRINTER_CFG(Config, CTower3X, c) - virt.template get<0>()) - FloatSquare(APRINTER_CFG(Config, CTower3Y, c) - virt.template get<1>())) + virt.template get<2>());
    }
    
    template <typename Src, typename Dst>
    static void physToVirt (Context c, Src phys, Dst out_virt)
    {
        MyVector p1 = MyVector::make(APRINTER_CFG(Config, CTower1X, c), APRINTER_CFG(Config, CTower1Y, c), phys.template get<0>());
        MyVector p2 = MyVector::make(APRINTER_CFG(Config, CTower2X, c), APRINTER_CFG(Config, CTower2Y, c), phys.template get<1>());
        MyVector p3 = MyVector::make(APRINTER_CFG(Config, CTower3X, c), APRINTER_CFG(Config, CTower3Y, c), phys.template get<2>());
        MyVector normal = (p1 - p2).cross(p2 - p3);
        FpType k = 1.0f / normal.norm();
        FpType q = 0.5f * k;
        FpType a = q * (p2 - p3).norm() * (p1 - p2).dot(p1 - p3);
        FpType b = q * (p1 - p3).norm() * (p2 - p1).dot(p2 - p3);
        FpType cc = q * (p1 - p2).norm() * (p3 - p1).dot(p3 - p2);
        MyVector pc = (p1 * a) + (p2 * b) + (p3 * cc);
        FpType r2 = 0.25f * k * (p1 - p2).norm() * (p2 - p3).norm() * (p3 - p1).norm();
        FpType d = FloatSqrt(k * (APRINTER_CFG(Config, CDiagonalRod2, c) - r2));
        MyVector ps = pc - (normal * d);
        out_virt.template set<0>(ps.m_v[0]);
        out_virt.template set<1>(ps.m_v[1]);
        out_virt.template set<2>(ps.m_v[2]);
    }
    
private:
    using DiagonalRod = decltype(Config::e(Params::DiagonalRod::i()));
    using Radius = decltype(Config::e(Params::SmoothRodOffset::i()) - Config::e(Params::EffectorOffset::i()) - Config::e(Params::CarriageOffset::i()));
    
    using Value1 = APRINTER_FP_CONST_EXPR(-0.8660254037844386);
    using Value2 = APRINTER_FP_CONST_EXPR(-0.5);
    using Value3 = APRINTER_FP_CONST_EXPR(0.8660254037844386);
    using Value4 = APRINTER_FP_CONST_EXPR(0.0);
    using Value5 = APRINTER_FP_CONST_EXPR(1.0);
    
    using CDiagonalRod2 = decltype(ExprCast<FpType>(DiagonalRod() * DiagonalRod()));
    using CTower1X = decltype(ExprCast<FpType>(Radius() * Value1()));
    using CTower1Y = decltype(ExprCast<FpType>(Radius() * Value2()));
    using CTower2X = decltype(ExprCast<FpType>(Radius() * Value3()));
    using CTower2Y = decltype(ExprCast<FpType>(Radius() * Value2()));
    using CTower3X = decltype(ExprCast<FpType>(Radius() * Value4()));
    using CTower3Y = decltype(ExprCast<FpType>(Radius() * Value5()));
    
public:
    using ConfigExprs = MakeTypeList<CDiagonalRod2, CTower1X, CTower1Y, CTower2X, CTower2Y, CTower3X, CTower3Y>;
    
    struct Object : public ObjBase<DeltaTransform, ParentObject, EmptyTypeList> {};
};

template <
    typename TDiagonalRod,
    typename TSmoothRodOffset,
    typename TEffectorOffset,
    typename TCarriageOffset
>
struct DeltaTransformService {
    using DiagonalRod = TDiagonalRod;
    using SmoothRodOffset = TSmoothRodOffset;
    using EffectorOffset = TEffectorOffset;
    using CarriageOffset = TCarriageOffset;
    
    template <typename Context, typename ParentObject, typename Config, typename FpType>
    using Transform = DeltaTransform<Context, ParentObject, Config, FpType, DeltaTransformService>;
};

#include <aprinter/EndNamespace.h>

#endif
