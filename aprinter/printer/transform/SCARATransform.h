/*
 * Copyright (c) 2015 Armin van der Togt
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

#ifndef AMBROLIB_SCARA_TRANSFORM_H
#define AMBROLIB_SCARA_TRANSFORM_H

#include <stdint.h>
#include <stdio.h>

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/math/Vector3.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/printer/Configuration.h>
#include <aprinter/printer/Console.h>

namespace APrinter {

// Calculations from https://roboted.wordpress.com/fundamentals/ 
template <typename Arg>
class SCARATransform {
    using Context      = typename Arg::Context;
    using ParentObject = typename Arg::ParentObject;
    using Config       = typename Arg::Config;
    using FpType       = typename Arg::FpType;
    using Params       = typename Arg::Params;
    
private:
    // constants
    using DegreesToRadians = APRINTER_FP_CONST_EXPR(0.017453292519943295);
    using RadiansToDegrees = APRINTER_FP_CONST_EXPR(57.29577951308232);
    using Two = APRINTER_FP_CONST_EXPR(2.0);

public:
    static int const NumAxes = 2;

    template <typename Src, typename Dst>
    static bool virtToPhys (Context c, Src virt, Dst out_phys)
    {
        // Cartesian to SCARA = inverse kinematics

        // Apply some corrections
        FpType x = virt.template get<0>() - APRINTER_CFG(Config, CXOffset, c);
        FpType y = virt.template get<1>() - APRINTER_CFG(Config, CYOffset, c);

        // We'll need this a few times.
        FpType d2 = x * x + y * y;
        
        // First, we obtain an expression for the elbow angle by applying the cos rule in the triangle (0,0)(x’,y’)(x,y).
        // E = cos^-1 ( (x^2+y^2-L_1^2-L_2^2)/(2L_1L_2) )
        FpType cosE = (d2 - APRINTER_CFG(Config, CSSQArms, c)) / APRINTER_CFG(Config, C2PArms, c);

        // SCARA position is undefined if abs(cosE) >= 1
        if (!(cosE >= -1.0f && cosE <= 1.0f)) {
            return false;
        }

        FpType e = FloatAcos(cosE) * (FpType)RadiansToDegrees::value();

        // The angle (S+Q) = tan^-1( y/x )
        FpType sPlusQ = FloatAtan2(y, x);

        // The angle Q = cos^-1((x^2+y^2+L_1^2-L_2^2)/(2L_1sqrt(x^2+y^2)))
        FpType q = FloatAcos((d2 + APRINTER_CFG(Config, CDSQArms, c)) / (APRINTER_CFG(Config, C2Arm1Length, c) * FloatSqrt(d2)));

        // So, the shoulder angle S=tan^-1(y/x)-cos^-1((x^2+y^2+L_1^2-L_2^2)/(2L_1sqrt(x^2+y^2)))
        FpType s = (sPlusQ - q) * (FpType)RadiansToDegrees::value();

        // When the motor that drives arm2 is not built into the arm (e.g. RepRap Morgan), the elbow angle is not independent of the shoulder angle
        if (APRINTER_CFG(Config, CExternalArm2Motor, c)) {
            e += s;
        }
        
        out_phys.template set<0>(s);
        out_phys.template set<1>(e);
        return true;
    }

    template <typename Src, typename Dst>
    static void physToVirt (Context c, Src phys, Dst out_virt)
    {
        // SCARA forward kinematic equations

        FpType s = phys.template get<0>() * (FpType)DegreesToRadians::value();
        FpType sPlusE = phys.template get<1>() * (FpType)DegreesToRadians::value();
        
        if (!APRINTER_CFG(Config, CExternalArm2Motor, c)) {
            sPlusE += s;
        }
        
        // x = L_1cos(S) + L_2cos(S+E)
        FpType x = APRINTER_CFG(Config, CArm1Length, c) * FloatCos(s)
                 + APRINTER_CFG(Config, CArm2Length, c) * FloatCos(sPlusE)
                 + APRINTER_CFG(Config, CXOffset, c);
        
        // y = L_1sin(S) + L_2sin(S+E)
        FpType y = APRINTER_CFG(Config, CArm1Length, c) * FloatSin(s)
                 + APRINTER_CFG(Config, CArm2Length, c) * FloatSin(sPlusE)
                 + APRINTER_CFG(Config, CYOffset, c);

        out_virt.template set<0>(x);
        out_virt.template set<1>(y);
    }

private:
    // expressions for configuration parameters
    using Arm1Length = decltype(Config::e(Params::Arm1Length::i()));
    using Arm2Length = decltype(Config::e(Params::Arm2Length::i()));
    using ExternalArm2Motor = decltype(Config::e(Params::ExternalArm2Motor::i()));
    using XOffset = decltype(Config::e(Params::XOffset::i()));
    using YOffset = decltype(Config::e(Params::YOffset::i()));

    // cached values derived from configuration parameters (all cast to proper types)
    using CArm1Length = decltype(ExprCast<FpType>(Arm1Length()));
    using CArm2Length = decltype(ExprCast<FpType>(Arm2Length()));
    using C2Arm1Length = decltype(ExprCast<FpType>(Two() * Arm1Length()));
    using CExternalArm2Motor = decltype(ExprCast<bool>(ExternalArm2Motor()));
    using C2PArms = decltype(ExprCast<FpType>(Two() * Arm1Length() * Arm2Length()));
    using CSSQArms = decltype(ExprCast<FpType>(Arm1Length() * Arm1Length() + Arm2Length() * Arm2Length()));
    using CDSQArms = decltype(ExprCast<FpType>(Arm1Length() * Arm1Length() - Arm2Length() * Arm2Length()));
    using CXOffset = decltype(ExprCast<FpType>(XOffset()));
    using CYOffset = decltype(ExprCast<FpType>(YOffset()));

public:
    using ConfigExprs = MakeTypeList<CArm1Length, CArm2Length, C2Arm1Length, CExternalArm2Motor, C2PArms, CSSQArms, CDSQArms, CXOffset, CYOffset>;

    struct Object : public ObjBase<SCARATransform, ParentObject, EmptyTypeList> {};
};

APRINTER_ALIAS_STRUCT_EXT(SCARATransformService, (
    APRINTER_AS_TYPE(Arm1Length),
    APRINTER_AS_TYPE(Arm2Length),
    APRINTER_AS_TYPE(ExternalArm2Motor),
    APRINTER_AS_TYPE(XOffset),
    APRINTER_AS_TYPE(YOffset)
), (
    APRINTER_ALIAS_STRUCT_EXT(Transform, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(Config),
        APRINTER_AS_TYPE(FpType)
    ), (
        using Params = SCARATransformService;
        APRINTER_DEF_INSTANCE(Transform, SCARATransform)
    ))
))

}

#endif
