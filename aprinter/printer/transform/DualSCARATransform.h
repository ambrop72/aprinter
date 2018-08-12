/*
 * Copyright (c) 2018 Ambroz Bizjak
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

#ifndef APRINTER_DUAL_SCARA_TRANSFORM_H
#define APRINTER_DUAL_SCARA_TRANSFORM_H

#include <stdint.h>
#include <math.h>

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/math/Vector2.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/math/TriangleUtils.h>
#include <aprinter/printer/Configuration.h>

namespace APrinter {

template <typename Arg>
class DualSCARATransform {
    using Context      = typename Arg::Context;
    using ParentObject = typename Arg::ParentObject;
    using Config       = typename Arg::Config;
    using FpType       = typename Arg::FpType;
    using Params       = typename Arg::Params;
    
private:
    // Constants.
    using DegreesToRadians = APRINTER_FP_CONST_EXPR(0.017453292519943295);
    using RadiansToDegrees = APRINTER_FP_CONST_EXPR(57.29577951308232);

public:
    static int const NumAxes = 2;

    // Cartesian to arm angles (inverse kinematics).
    template <typename Src, typename Dst>
    static bool virtToPhys (Context c, Src virt, Dst out_phys)
    {
        // The point p represents the input Cartesian coordinates with XOffset
        // and YOffset applied.
        Vector2<FpType> p = Vector2<FpType>::make(
            virt.template get<0>() - APRINTER_CFG(Config, CXOffset, c),
            virt.template get<1>() - APRINTER_CFG(Config, CYOffset, c));

        // The points sh1 and sh2 represent the arm shoulder points.
        Vector2<FpType> sh1 = Vector2<FpType>::make(
            APRINTER_CFG(Config, CArm1ShoulderXCoord, c), 0.0f);
        Vector2<FpType> sh2 = Vector2<FpType>::make(
            APRINTER_CFG(Config, CArm2ShoulderXCoord, c), 0.0f);
        
        // Get the proximal (pr1, pr2) and distal (di1, di2) arm lengths.
        FpType pr1 = APRINTER_CFG(Config, CArm1ProximalSideLength, c);
        FpType pr2 = APRINTER_CFG(Config, CArm2ProximalSideLength, c);
        FpType di1 = APRINTER_CFG(Config, CArm1DistalSideLength, c);
        FpType di2 = APRINTER_CFG(Config, CArm2DistalSideLength, c);

        // Calculate the elbow locations (elb1, elb2). Each of these is
        // calculated as an intersection point of two circles: one circle is
        // centered at the arm shoulder point and its radius is the proximal
        // side length, and the other circle is centered at p and its radius
        // is the distal side length. The arguments are given in such an order
        // to obtain the "outside" intersection point.
        Vector2<FpType> elb1;
        Vector2<FpType> elb2;
        bool ok =
            leftIntersectionOfCircles(sh1, p, pr1, di1, elb1) &&
            leftIntersectionOfCircles(p, sh2, di2, pr2, elb2);

        // Check if these calculation succeeded.
        if (!ok) {
            return false;
        }

        // Check that the distal sides form a convex rather than concave
        // angle on the outside. We do not allow motion into the area where
        // these sides would form a concave angle. We check this by
        // determining which side of a directed line from e1 to e2 p lies on.
        if (!(triangleWindingOrder(elb1, elb2, p) >= 0.0f)) {
            return false;
        }

        // Subtract the arm shoulder points from the respective elbow locations,
        // obtaining vectors that will be used to calculate the arm angles.
        Vector2<FpType> v1 = elb1 - sh1;
        Vector2<FpType> v2 = elb2 - sh2;

        // Calculate the arm angles from v1 and v2.
        FpType angle1 = FloatAtan2(v1.m_v[0], v1.m_v[1]);
        FpType angle2 = FloatAtan2(v2.m_v[0], v2.m_v[1]);

        // Return the arm angles in degrees.
        out_phys.template set<0>(angle1 * (FpType)RadiansToDegrees::value());
        out_phys.template set<1>(angle2 * (FpType)RadiansToDegrees::value());
        return true;
    }

    // Arm angles to Cartesian (forward kinematics).
    template <typename Src, typename Dst>
    static void physToVirt (Context c, Src phys, Dst out_virt)
    {
        // Get the arm angles in radians.
        FpType angle1 = phys.template get<0>() * (FpType)DegreesToRadians::value();
        FpType angle2 = phys.template get<1>() * (FpType)DegreesToRadians::value();

        // Get the proximal (pr1, pr2) and distal (di1, di2) arm lengths.
        FpType pr1 = APRINTER_CFG(Config, CArm1ProximalSideLength, c);
        FpType pr2 = APRINTER_CFG(Config, CArm2ProximalSideLength, c);
        FpType di1 = APRINTER_CFG(Config, CArm1DistalSideLength, c);
        FpType di2 = APRINTER_CFG(Config, CArm2DistalSideLength, c);

        // Calculate the elbow locations (elb1, elb2).
        Vector2<FpType> elb1 = Vector2<FpType>::make(
            APRINTER_CFG(Config, CArm1ShoulderXCoord, c) + pr1 * FloatSin(angle1),
            pr1 * FloatCos(angle1));
        Vector2<FpType> elb2 = Vector2<FpType>::make(
            APRINTER_CFG(Config, CArm2ShoulderXCoord, c) + pr2 * FloatSin(angle2),
            pr2 * FloatCos(angle2));
        
        // Calculate the Cartesian position (p) as the intersection of two circles
        // centered at the elbow locations and with radii equal to the respective
        // distal side lengths.
        Vector2<FpType> p;
        bool ok = leftIntersectionOfCircles(elb1, elb2, di1, di2, p);
        
        if (!ok) {
            // NOTE: Cannot report error currently.
            out_virt.template set<0>(NAN);
            out_virt.template set<1>(NAN);
            return;
        }

        // Returns the resulting point with XOffset and YOffset applied.
        out_virt.template set<0>(p.m_v[0] + APRINTER_CFG(Config, CXOffset, c));
        out_virt.template set<1>(p.m_v[1] + APRINTER_CFG(Config, CYOffset, c));
    }

private:
    // Expressions for configuration parameters.
    using Arm1ShoulderXCoord = decltype(Config::e(Params::Arm1ShoulderXCoord::i()));
    using Arm2ShoulderXCoord = decltype(Config::e(Params::Arm2ShoulderXCoord::i()));
    using Arm1ProximalSideLength = decltype(Config::e(Params::Arm1ProximalSideLength::i()));
    using Arm2ProximalSideLength = decltype(Config::e(Params::Arm2ProximalSideLength::i()));
    using Arm1DistalSideLength = decltype(Config::e(Params::Arm1DistalSideLength::i()));
    using Arm2DistalSideLength = decltype(Config::e(Params::Arm2DistalSideLength::i()));
    using XOffset = decltype(Config::e(Params::XOffset::i()));
    using YOffset = decltype(Config::e(Params::YOffset::i()));

    // Cached values derived from configuration parameters (all cast to proper types).
    using CArm1ShoulderXCoord = decltype(ExprCast<FpType>(Arm1ShoulderXCoord()));
    using CArm2ShoulderXCoord = decltype(ExprCast<FpType>(Arm2ShoulderXCoord()));
    using CArm1ProximalSideLength = decltype(ExprCast<FpType>(Arm1ProximalSideLength()));
    using CArm2ProximalSideLength = decltype(ExprCast<FpType>(Arm2ProximalSideLength()));
    using CArm1DistalSideLength = decltype(ExprCast<FpType>(Arm1DistalSideLength()));
    using CArm2DistalSideLength = decltype(ExprCast<FpType>(Arm2DistalSideLength()));
    using CXOffset = decltype(ExprCast<FpType>(XOffset()));
    using CYOffset = decltype(ExprCast<FpType>(YOffset()));

public:
    using ConfigExprs = MakeTypeList<
        CArm1ShoulderXCoord,
        CArm2ShoulderXCoord,
        CArm1ProximalSideLength,
        CArm2ProximalSideLength,
        CArm1DistalSideLength,
        CArm2DistalSideLength,
        CXOffset,
        CYOffset
    >;

    struct Object : public ObjBase<DualSCARATransform, ParentObject, EmptyTypeList> {};
};

APRINTER_ALIAS_STRUCT_EXT(DualSCARATransformService, (
    APRINTER_AS_TYPE(Arm1ShoulderXCoord),
    APRINTER_AS_TYPE(Arm2ShoulderXCoord),
    APRINTER_AS_TYPE(Arm1ProximalSideLength),
    APRINTER_AS_TYPE(Arm2ProximalSideLength),
    APRINTER_AS_TYPE(Arm1DistalSideLength),
    APRINTER_AS_TYPE(Arm2DistalSideLength),
    APRINTER_AS_TYPE(XOffset),
    APRINTER_AS_TYPE(YOffset)
), (
    APRINTER_ALIAS_STRUCT_EXT(Transform, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(Config),
        APRINTER_AS_TYPE(FpType)
    ), (
        using Params = DualSCARATransformService;
        APRINTER_DEF_INSTANCE(Transform, DualSCARATransform)
    ))
))

}

#endif
