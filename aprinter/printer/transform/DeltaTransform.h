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

#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/math/Vector3.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/printer/Configuration.h>

namespace APrinter {

template <typename Arg>
class DeltaTransform {
    using Context      = typename Arg::Context;
    using ParentObject = typename Arg::ParentObject;
    using Config       = typename Arg::Config;
    using FpType       = typename Arg::FpType;
    using Params       = typename Arg::Params;
    
private:
    using MyVector = Vector3<FpType>;
    
public:
    static int const NumAxes = 3;
    
    template <typename Src, typename Dst>
    static bool virtToPhys (Context c, Src virt, Dst out_phys)
    {
        FpType x = virt.template get<0>();
        FpType y = virt.template get<1>();
        FpType z = virt.template get<2>();
        
        if (!(x*x + y*y <= APRINTER_CFG(Config, CLimitRadius2, c))) {
            return false;
        }
        
        out_phys.template set<0>(FloatSqrt(APRINTER_CFG(Config, CDiagonalRod1_2, c) - FloatSquare(APRINTER_CFG(Config, CTower1X, c) - x) - FloatSquare(APRINTER_CFG(Config, CTower1Y, c) - y)) + z);
        out_phys.template set<1>(FloatSqrt(APRINTER_CFG(Config, CDiagonalRod2_2, c) - FloatSquare(APRINTER_CFG(Config, CTower2X, c) - x) - FloatSquare(APRINTER_CFG(Config, CTower2Y, c) - y)) + z);
        out_phys.template set<2>(FloatSqrt(APRINTER_CFG(Config, CDiagonalRod3_2, c) - FloatSquare(APRINTER_CFG(Config, CTower3X, c) - x) - FloatSquare(APRINTER_CFG(Config, CTower3Y, c) - y)) + z);

        return true;
    }
    
    template <typename Src, typename Dst>
    static void physToVirt (Context c, Src phys, Dst out_virt)
    {
        MyVector p1 = MyVector::make(APRINTER_CFG(Config, CTower1X, c), APRINTER_CFG(Config, CTower1Y, c), phys.template get<0>());
        MyVector p2 = MyVector::make(APRINTER_CFG(Config, CTower2X, c), APRINTER_CFG(Config, CTower2Y, c), phys.template get<1>());
        MyVector p3 = MyVector::make(APRINTER_CFG(Config, CTower3X, c), APRINTER_CFG(Config, CTower3Y, c), phys.template get<2>());
        
        // We need to find the point p which is exactly (r1, r2, r3) away from the points
        // (p1, p2, p3) respectively, where ri are the diagonal rod lengths. This code
        // is based on the solution on Wikipedia:
        // https://en.wikipedia.org/wiki/True_range_multilateration#Three_Cartesian_dimensions,_three_measured_slant_ranges
        //
        // The idea is to work in a local (X, Y, Z) coordinate system defined by p1, p2, p3
        // such that:
        // - p1 is at (0, 0, 0),
        // - p2 is at (u, 0, 0),
        // - p3 is at (vx, vy, 0),
        // - p (the solution) is at (x, y, z).

        // Calculate u as the length of the vector p2 - p1.
        MyVector d12 = p2 - p1;
        FpType u_2 = d12.squaredLength();
        FpType u = FloatSqrt(u_2);

        // Calculate ex, the unit vector in the direction of p2 - p1.
        MyVector ex = d12 / u;

        // Calculate vx by projecting the vector p3 - p1 to ex (note that ex is a unit
        // vector so there is no need to normalize after the dot product).
        MyVector d13 = p3 - p1;
        FpType vx = d13.dot(ex);

        // Calculate vy_v, the rejection of p3 - p1 after projection to ex.
        MyVector vy_v = d13 - ex * vx;

        // Calculate vy as the length of vy_v.
        FpType vy = vy_v.length();

        // Calculate ey, the unit vector in the direction of the Y axis.
        MyVector ey = vy_v / vy;

        // Get the squares of ri into variables.
        FpType r1_2 = APRINTER_CFG(Config, CDiagonalRod1_2, c);
        FpType r2_2 = APRINTER_CFG(Config, CDiagonalRod2_2, c);
        FpType r3_2 = APRINTER_CFG(Config, CDiagonalRod3_2, c);

        // Calculate v_2. This is equal to the squared length of p3 - p1, but it
        // is more efficient to calculate it from vx and vy.
        FpType v_2 = vx * vx + vy * vy;

        // Calculate (x, y, z) using the formulas. Note that we need the negative z
        // because we are interested in solution below (p1, p2, p3), not the one above.
        FpType x = (r1_2 - r2_2 + u_2) / (2.0f * u);
        FpType y = (r1_2 - r3_2 + v_2 - 2.0f * vx * x) / (2.0f * vy);
        FpType z = -FloatSqrt(r1_2 - x * x - y * y);

        // Calculate ez, the unit vector in the direction of the Z axis.
        MyVector ez = ex.cross(ey);

        // Calculate the result p by transforming (x, y, z) into the global coordinate
        // system.
        MyVector p = p1 + ex * x + ey * y + ez * z;

        out_virt.template set<0>(p.m_v[0]);
        out_virt.template set<1>(p.m_v[1]);
        out_virt.template set<2>(p.m_v[2]);
    }
    
private:
    using DiagonalRod = decltype(Config::e(Params::DiagonalRod::i()));
    using DiagonalRodCorr1 = decltype(Config::e(Params::DiagonalRodCorr1::i()));
    using DiagonalRodCorr2 = decltype(Config::e(Params::DiagonalRodCorr2::i()));
    using DiagonalRodCorr3 = decltype(Config::e(Params::DiagonalRodCorr3::i()));
    using Radius = decltype(Config::e(Params::SmoothRodOffset::i()) - Config::e(Params::EffectorOffset::i()) - Config::e(Params::CarriageOffset::i()));
    using LimitRadius = decltype(Config::e(Params::LimitRadius::i()));
    
    using Value1 = APRINTER_FP_CONST_EXPR(-0.8660254037844386);
    using Value2 = APRINTER_FP_CONST_EXPR(-0.5);
    using Value3 = APRINTER_FP_CONST_EXPR(0.8660254037844386);
    using Value4 = APRINTER_FP_CONST_EXPR(0.0);
    using Value5 = APRINTER_FP_CONST_EXPR(1.0);
    
    using CDiagonalRod1_2 = decltype(ExprCast<FpType>(ExprSquare(DiagonalRod() + DiagonalRodCorr1())));
    using CDiagonalRod2_2 = decltype(ExprCast<FpType>(ExprSquare(DiagonalRod() + DiagonalRodCorr2())));
    using CDiagonalRod3_2 = decltype(ExprCast<FpType>(ExprSquare(DiagonalRod() + DiagonalRodCorr3())));
    using CTower1X = decltype(ExprCast<FpType>(Radius() * Value1()));
    using CTower1Y = decltype(ExprCast<FpType>(Radius() * Value2()));
    using CTower2X = decltype(ExprCast<FpType>(Radius() * Value3()));
    using CTower2Y = decltype(ExprCast<FpType>(Radius() * Value2()));
    using CTower3X = decltype(ExprCast<FpType>(Radius() * Value4()));
    using CTower3Y = decltype(ExprCast<FpType>(Radius() * Value5()));
    using CLimitRadius2 = decltype(ExprCast<FpType>(LimitRadius() * LimitRadius()));
    
public:
    using ConfigExprs = MakeTypeList<CDiagonalRod1_2, CDiagonalRod2_2, CDiagonalRod3_2,
        CTower1X, CTower1Y, CTower2X, CTower2Y, CTower3X, CTower3Y, CLimitRadius2>;
    
    struct Object : public ObjBase<DeltaTransform, ParentObject, EmptyTypeList> {};
};

APRINTER_ALIAS_STRUCT_EXT(DeltaTransformService, (
    APRINTER_AS_TYPE(DiagonalRod),
    APRINTER_AS_TYPE(DiagonalRodCorr1),
    APRINTER_AS_TYPE(DiagonalRodCorr2),
    APRINTER_AS_TYPE(DiagonalRodCorr3),
    APRINTER_AS_TYPE(SmoothRodOffset),
    APRINTER_AS_TYPE(EffectorOffset),
    APRINTER_AS_TYPE(CarriageOffset),
    APRINTER_AS_TYPE(LimitRadius)
), (
    APRINTER_ALIAS_STRUCT_EXT(Transform, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(Config),
        APRINTER_AS_TYPE(FpType)
    ), (
        using Params = DeltaTransformService;
        APRINTER_DEF_INSTANCE(Transform, DeltaTransform)
    ))
))

}

#endif
