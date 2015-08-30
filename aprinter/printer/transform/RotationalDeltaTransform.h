/*
 * Copyright (c) 2015, 2013 Armin van der Togt, Ambroz Bizjak
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

#ifndef AMBROLIB_ROTATIONALDELTA_TRANSFORM_H
#define AMBROLIB_ROTATIONALDELTA_TRANSFORM_H

#include <stdint.h>

#include <aprinter/base/Object.h>
#include <aprinter/math/Vector3.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/printer/Configuration.h>

#include <aprinter/BeginNamespace.h>

// Calculations from http://forums.trossenrobotics.com/tutorials/introduction-129/delta-robot-kinematics-3276/
template <typename Context, typename ParentObject, typename Config, typename FpType, typename Params>
class RotationalDeltaTransform {
private:
    using MyVector = Vector3<FpType>;
    
    // trigonometric constants
    using HalfTan30 = APRINTER_FP_CONST_EXPR(0.28867513459481287);
    using Sin120 = APRINTER_FP_CONST_EXPR(0.8660254037844387);
    using Cos120 = APRINTER_FP_CONST_EXPR(-0.5f);
    using Tan60 = APRINTER_FP_CONST_EXPR(1.7320508075688767);
    using Sin30 = APRINTER_FP_CONST_EXPR(0.5f);
    using DegreesToRadians = APRINTER_FP_CONST_EXPR(0.017453292519943295);
    using RadiansToDegrees = APRINTER_FP_CONST_EXPR(57.29577951308232);
    
    // helper function, calculates angle theta1 (for YZ-pane)
    static bool delta_calcAngleYZ (Context c, FpType x0, FpType y0, FpType z0, FpType &out_theta)
    {
        FpType arm_length = APRINTER_CFG(Config, CArmLength, c);
        FpType rod_length = APRINTER_CFG(Config, CRodLength, c);
        FpType value_y1 = APRINTER_CFG(Config, CValueY1, c);
        
        y0 -= APRINTER_CFG(Config, CValueY0Diff, c); // shift center to edge
        FpType a = (FloatSquare(x0) + FloatSquare(y0) + FloatSquare(z0)
                    + FloatSquare(arm_length) - FloatSquare(rod_length) - FloatSquare(value_y1)) / (2 * z0);
        FpType b = (value_y1 - y0) / z0;
        
        // discriminant
        FpType d = -FloatSquare(a + b * value_y1) + arm_length * (FloatSquare(b) * arm_length + arm_length);
        if (d < 0) {
            return false; // non-existing point
        }
        
        FpType yj = (value_y1 - a * b - FloatSqrt(d)) / (FloatSquare(b) + 1); // choosing outer point
        FpType zj = a + b * yj;
        FpType value_y1_minus_yj = value_y1 - yj;
        out_theta = FloatAtan2(-zj, value_y1_minus_yj) * (FpType)RadiansToDegrees::value();
        return true;
    }

public:
    static int const NumAxes = 3;
    
    template <typename Src, typename Dst>
    static void virtToPhys (Context c, Src virt, Dst out_phys)
    {
        // Cartesian to delta = inverse kinematics
        
        FpType x = virt.template get<0>();
        FpType y = virt.template get<1>();
        FpType z = virt.template get<2>() - APRINTER_CFG(Config, CZOffset, c);
        
        FpType theta1;
        FpType theta2;
        FpType theta3;
        bool status = true;
        status &= delta_calcAngleYZ(c, x, y, z, theta1);
        status &= delta_calcAngleYZ(c, x * (FpType)Cos120::value() + y * (FpType)Sin120::value(), y * (FpType)Cos120::value() - x * (FpType)Sin120::value(), z, theta2); // rotate coords to +120 deg
        status &= delta_calcAngleYZ(c, x * (FpType)Cos120::value() - y * (FpType)Sin120::value(), y * (FpType)Cos120::value() + x * (FpType)Sin120::value(), z, theta3); // rotate coords to -120 deg
        // NOTE: We don't handle errors (status==false).
        
        out_phys.template set<0>(theta1);
        out_phys.template set<1>(theta2);
        out_phys.template set<2>(theta3);
    }
    
    template <typename Src, typename Dst>
    static void physToVirt (Context c, Src phys, Dst out_virt)
    {
        // Delta to cartesian = forward kinematics
        
        FpType arm_length = APRINTER_CFG(Config, CArmLength, c);
        FpType rod_length = APRINTER_CFG(Config, CRodLength, c);
        FpType value_t = APRINTER_CFG(Config, CValueT, c);

        MyVector theta = MyVector::make(phys.template get<0>(), phys.template get<1>(), phys.template get<2>());
        theta = theta * (FpType)DegreesToRadians::value(); // Degrees to radians

        FpType y1 = -(value_t + arm_length * FloatCos(theta.m_v[0]));
        FpType z1 = -arm_length * FloatSin(theta.m_v[0]);

        FpType y2 = (value_t + arm_length * FloatCos(theta.m_v[1])) * (FpType)Sin30::value();
        FpType x2 = y2 * (FpType)Tan60::value();
        FpType z2 = -arm_length * FloatSin(theta.m_v[1]);

        FpType y3 = (value_t + arm_length * FloatCos(theta.m_v[2])) * (FpType)Sin30::value();
        FpType x3 = -y3 * (FpType)Tan60::value();
        FpType z3 = -arm_length * FloatSin(theta.m_v[2]);

        FpType dnm = (y2 - y1) * x3 - (y3 - y1) * x2;

        FpType w1 = y1 * y1 + z1 * z1;
        FpType w2 = x2 * x2 + y2 * y2 + z2 * z2;
        FpType w3 = x3 * x3 + y3 * y3 + z3 * z3;

        // x = (a1*z + b1)/dnm
        FpType a1 = (z2 - z1) * (y3 - y1) - (z3 - z1) * (y2 - y1);
        FpType b1 = -((w2 - w1) * (y3 - y1) - (w3 - w1) * (y2 - y1)) * 0.5f;

        // y = (a2*z + b2)/dnm;
        FpType a2 = -(z2 - z1) * x3 + (z3 - z1) * x2;
        FpType b2 = ((w2 - w1) * x3 - (w3 - w1) * x2) * 0.5f;

        // a*z^2 + b*z + c = 0
        FpType a = a1 * a1 + a2 * a2 + dnm * dnm;
        FpType b = 2 * (a1 * b1 + a2 * (b2 - y1 * dnm) - z1 * dnm * dnm);
        FpType cee = (b2 - y1 * dnm) * (b2 - y1 * dnm) + b1 * b1
                        + dnm * dnm * (z1 * z1 - FloatSquare(rod_length));

        // discriminant
        FpType d = FloatSquare(b) - 4.0f * a * cee;
        // NOTE: We don't handle errors (d<0).

        FpType z0 = -0.5f * (b + FloatSqrt(d)) / a;
        FpType x0 = (a1 * z0 + b1) / dnm;
        FpType y0 = (a2 * z0 + b2) / dnm;
        
        out_virt.template set<0>(x0);
        out_virt.template set<1>(y0);
        out_virt.template set<2>(z0 + APRINTER_CFG(Config, CZOffset, c));
    }
    
private:
    // expressions for configuration parameters
    using EndEffectorLength = decltype(Config::e(Params::EndEffectorLength::i())); // e in trossen tutorial
    using BaseLength = decltype(Config::e(Params::BaseLength::i())); // f in trossen tutorial
    using RodLength = decltype(Config::e(Params::RodLength::i()));// re in trossen tutorial
    using ArmLength = decltype(Config::e(Params::ArmLength::i()));// rf in trossen tutorial
    using ZOffset = decltype(Config::e(Params::ZOffset::i()));// Z- axis offset to put the print bed at 0 Z coordinate

    // cached values derived from configuration parameters (all cast to FpType)
    using CRodLength = decltype(ExprCast<FpType>(RodLength()));
    using CArmLength = decltype(ExprCast<FpType>(ArmLength()));
    using CZOffset = decltype(ExprCast<FpType>(ZOffset()));
    using CValueY1 = decltype(ExprCast<FpType>(BaseLength() * -HalfTan30()));
    using CValueY0Diff = decltype(ExprCast<FpType>(EndEffectorLength() * HalfTan30()));
    using CValueT = decltype(ExprCast<FpType>((BaseLength() - EndEffectorLength()) * HalfTan30()));
    
public:
    using ConfigExprs = MakeTypeList<CRodLength, CArmLength, CZOffset, CValueY1, CValueY0Diff, CValueT>;
    
    struct Object : public ObjBase<RotationalDeltaTransform, ParentObject, EmptyTypeList> {};
};

template <
    typename TEndEffectorLength,
    typename TBaseLength,
    typename TRodLength,
    typename TArmLength,
    typename TZOffset
>
struct RotationalDeltaTransformService {
    using EndEffectorLength = TEndEffectorLength;
    using BaseLength = TBaseLength;
    using RodLength = TRodLength;
    using ArmLength = TArmLength;
    using ZOffset = TZOffset;
    
    template <typename Context, typename ParentObject, typename Config, typename FpType>
    using Transform = RotationalDeltaTransform<Context, ParentObject, Config, FpType, RotationalDeltaTransformService>;
};

#include <aprinter/EndNamespace.h>

#endif
