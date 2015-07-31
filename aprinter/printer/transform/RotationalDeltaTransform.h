/*
 * Copyright (c) 2015,2013 Armin van der Togt, Ambroz Bizjak
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

#include <aprinter/meta/WrapDouble.h>
#include <aprinter/base/Object.h>
#include <aprinter/math/Vector3.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/printer/DistanceSplitter.h>
#include <aprinter/printer/Configuration.h>

#include <aprinter/BeginNamespace.h>

// trigonometric constants
#define Sin120 0.866025404f
#define Cos120 -0.5f
#define Tan60 1.732050808f
#define Sin30 0.5f
#define DegreesToRadians 0.017453293f

// Calculations from http://forums.trossenrobotics.com/tutorials/introduction-129/delta-robot-kinematics-3276/
template <typename Context, typename ParentObject, typename Config, typename FpType, typename Params>
class RotationalDeltaTransform {
private:
	using MyVector = Vector3<FpType>;

	// helper function, calculates angle theta1 (for YZ-pane)
	static int delta_calcAngleYZ(Context c, FpType x0, FpType y0, FpType z0, FpType &theta) {
		y0 -= APRINTER_CFG(Config, ValueY0Diff, c);    // shift center to edge
		FpType a = (x0 * x0 + y0 * y0 + z0 * z0
				+ APRINTER_CFG(Config, ArmLength, c) * APRINTER_CFG(Config, ArmLength, c)
				- APRINTER_CFG(Config, RodLength, c) * APRINTER_CFG(Config, RodLength, c) - APRINTER_CFG(Config, ValueY1, c) * APRINTER_CFG(Config, ValueY1, c))
				/ (2 * z0);
		FpType b = (APRINTER_CFG(Config, ValueY1, c) - y0) / z0;
		// discriminant
		FpType d = -(a + b * APRINTER_CFG(Config, ValueY1, c)) * (a + b * APRINTER_CFG(Config, ValueY1, c))
				+ APRINTER_CFG(Config, ArmLength, c)
						* (b * b * APRINTER_CFG(Config, ArmLength, c) + APRINTER_CFG(Config, ArmLength, c));
		if (d < 0)
			return -1; // non-existing point
		FpType yj = (APRINTER_CFG(Config, ValueY1, c) - a * b - FloatSqrt(d)) / (b * b + 1); // choosing outer point
		FpType zj = a + b * yj;
		theta = atan(-zj / (APRINTER_CFG(Config, ValueY1, c) - yj)) / DegreesToRadians + ((yj > APRINTER_CFG(Config, ValueY1, c)) ? 180.0f : 0.0f);
		return 0;
	}

public:
    static int const NumAxes = 3;
    
    template <typename Src, typename Dst>
    static void virtToPhys (Context c, Src virt, Dst out_phys)
			{
		// Cartesian to delta = inverse kinematics
		FpType z = virt.template get<2>() - APRINTER_CFG(Config, ZOffset, c);
		FpType theta1;
		FpType theta2;
		FpType theta3;
		int status = delta_calcAngleYZ(c, virt.template get<0>(), virt.template get<1>(), z, theta1);
		if (status == 0)
			status = delta_calcAngleYZ(c, virt.template get<0>() * Cos120 + virt.template get<1>() * Sin120,
					virt.template get<1>() * Cos120 - virt.template get<0>() * Sin120, z, theta2); // rotate coords to +120 deg
		if (status == 0)
			status = delta_calcAngleYZ(c, virt.template get<0>() * Cos120 - virt.template get<1>() * Sin120,
					virt.template get<1>() * Cos120 + virt.template get<0>() * Sin120, z, theta3); // rotate coords to -120 deg

		// TODO What to do when status < 0???
		out_phys.template set<0>(theta1);
		out_phys.template set<1>(theta2);
		out_phys.template set<2>(theta3);
	}
    
    template <typename Src, typename Dst>
    static void physToVirt (Context c, Src phys, Dst out_virt)
    {
    	// Delta to cartesian = forward kinematics

		MyVector theta = MyVector::make(phys.template get<0>(), phys.template get<1>(), phys.template get<2>());
		theta = theta * DegreesToRadians; // Degrees to radians

		FpType y1 = -(APRINTER_CFG(Config, ValueT, c) + APRINTER_CFG(Config, ArmLength, c) * cos(theta.m_v[0]));
		FpType z1 = -APRINTER_CFG(Config, ArmLength, c) * sin(theta.m_v[0]);

		FpType y2 = (APRINTER_CFG(Config, ValueT, c) + APRINTER_CFG(Config, ArmLength, c) * cos(theta.m_v[1])) * Sin30;
		FpType x2 = y2 * Tan60;
		FpType z2 = -APRINTER_CFG(Config, ArmLength, c) * sin(theta.m_v[1]);

		FpType y3 = (APRINTER_CFG(Config, ValueT, c) + APRINTER_CFG(Config, ArmLength, c) * cos(theta.m_v[2])) * Sin30;
		FpType x3 = -y3 * Tan60;
		FpType z3 = -APRINTER_CFG(Config, ArmLength, c) * sin(theta.m_v[2]);

		FpType dnm = (y2 - y1) * x3 - (y3 - y1) * x2;

		FpType w1 = y1 * y1 + z1 * z1;
		FpType w2 = x2 * x2 + y2 * y2 + z2 * z2;
		FpType w3 = x3 * x3 + y3 * y3 + z3 * z3;

		// x = (a1*z + b1)/dnm
		FpType a1 = (z2 - z1) * (y3 - y1) - (z3 - z1) * (y2 - y1);
		FpType b1 = -((w2 - w1) * (y3 - y1) - (w3 - w1) * (y2 - y1)) / 2.0f;

		// y = (a2*z + b2)/dnm;
		FpType a2 = -(z2 - z1) * x3 + (z3 - z1) * x2;
		FpType b2 = ((w2 - w1) * x3 - (w3 - w1) * x2) / 2.0f;

		// a*z^2 + b*z + c = 0
		FpType a = a1 * a1 + a2 * a2 + dnm * dnm;
		FpType b = 2 * (a1 * b1 + a2 * (b2 - y1 * dnm) - z1 * dnm * dnm);
		FpType cee = (b2 - y1 * dnm) * (b2 - y1 * dnm) + b1 * b1
				+ dnm * dnm * (z1 * z1 - APRINTER_CFG(Config, RodLength, c) * APRINTER_CFG(Config, RodLength, c));

		// discriminant
		FpType d = b * b - 4.0f * a * cee;
		// TODO What to do when d < 0???
//        if (d < 0) return -1; // non-existing point

		FpType z0 = -0.5f * (b + FloatSqrt(d)) / a;
		FpType x0 = (a1 * z0 + b1) / dnm;
		FpType y0 = (a2 * z0 + b2) / dnm;
		out_virt.template set<0>(x0);
		out_virt.template set<1>(y0);
		out_virt.template set<2>(z0 + APRINTER_CFG(Config, ZOffset, c));
    }
    
    using Splitter = DistanceSplitter<typename Params::SplitterParams, FpType>;
    
private:
    // trigonometric constant
    using HalfTan30 = APRINTER_FP_CONST_EXPR(0.288675135f);

    using EndEffectorLength = decltype(Config::e(Params::EndEffectorLength::i)); // e in trossen tutorial
    using BaseLength = decltype(Config::e(Params::BaseLength::i)); // f in trossen tutorial
    using RodLength = decltype(Config::e(Params::RodLength::i));// re in trossen tutorial
    using ArmLength = decltype(Config::e(Params::ArmLength::i));// rf in trossen tutorial
    using ZOffset = decltype(Config::e(Params::ZOffset::i));// Z- axis offset to put the print bed at 0 Z coordinate

    using ValueY1 = decltype(ExprCast<FpType>(Config::e(Params::BaseLength::i) * -HalfTan30()));
    using ValueY0Diff = decltype(ExprCast<FpType>(Config::e(Params::EndEffectorLength::i) * HalfTan30()));

    using ValueT =  decltype(ExprCast<FpType>((Config::e(Params::BaseLength::i) - Config::e(Params::EndEffectorLength::i))*HalfTan30()));
    
public:
    using ConfigExprs = MakeTypeList<ValueT, ValueY1, ValueY0Diff, RodLength, ArmLength, ZOffset>;
    
    struct Object : public ObjBase<RotationalDeltaTransform, ParentObject, EmptyTypeList> {};
};

template <
    typename TEndEffectorLength,
    typename TBaseLength,
    typename TRodLength,
    typename TArmLength,
    typename TZOffset,
    typename TSplitterParams
>
struct RotationalDeltaTransformService {
    using EndEffectorLength = TEndEffectorLength;
    using BaseLength = TBaseLength;
    using RodLength = TRodLength;
    using ArmLength = TArmLength;
    using ZOffset = TZOffset;
    using SplitterParams = TSplitterParams;
    
    template <typename Context, typename ParentObject, typename Config, typename FpType>
    using Transform = RotationalDeltaTransform<Context, ParentObject, Config, FpType, RotationalDeltaTransformService>;
};

#include <aprinter/EndNamespace.h>

#endif
