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

#ifndef APRINTER_TRIANGLE_UTILS_H
#define APRINTER_TRIANGLE_UTILS_H

#include <aprinter/math/FloatTools.h>
#include <aprinter/math/Vector2.h>

namespace APrinter {

/**
 * Calculate triangle height given the lengths of sides.
 * 
 * @tparam FpType Floating-point type.
 * @param a Length of first side (with respect to which height will
 *        be calculated).
 * @param b Length of second side.
 * @param c Length of third side.
 * @return Height with respect to the first side.
 */
template <typename FpType>
FpType triangleHeight (FpType a, FpType b, FpType c)
{
    FpType s = (a + b + c) / 2.0f;
    return 2.0f * FloatSqrt(s * (s - a) * (s - b) * (s - c)) / a;
}

/**
 * Calculate an intersection of two circles given center coordinates
 * and radii.
 * 
 * This calculates the interesction on the left side if looking from
 * the first center toward the second center. Note that the
 * intersection on the other side can be calculated by reversing the
 * centers and radii.
 * 
 * @tparam FpType Floating-point type.
 * @param c1 Center of first circle.
 * @param c2 Center of second circle.
 * @param r1 Radius of first circle.
 * @param r2 Radius of second circle.
 * @param result On success, will be set to the left-side intersection point.
 * @return Whether the calculation succeeded.
 */
template <typename FpType>
bool leftIntersectionOfCircles (
    Vector2<FpType> c1, Vector2<FpType> c2, FpType r1, FpType r2, Vector2<FpType> &result)
{
    // Calculate the vector (d) from c1 to v2.
    Vector2<FpType> d = c2 - c1;

    // Calculate the squared length and length of d (distance between centers).
    FpType dLenSq = d.norm();
    FpType dLen = FloatSqrt(dLenSq);

    // Calculate the distance (h) of the seeked center point (p) from
    // the line going through the centers.
    FpType h = triangleHeight(dLen, r1, r2);
    
    // Sanity check.
    if (!(h >= 0.0f && h < r1 + r2)) {
        return false;
    }

    // Calculate the point (q) that is the orthogonal projection of the
    // seeked intersection point (p) to the line going through the centers.
    FpType f = (FloatSquare(r1) - FloatSquare(r2)) / (2.0f * dLenSq);
    Vector2<FpType> q = (c1 * (0.5f - f)) + (c2 * (0.5f + f));

    // Calculate the intersection point (p) by starting at q and going
    // h distance in the direction toward p (orthogonal to the line
    // betwen the centers).
    Vector2<FpType> dDir = d * (1.0f / dLen);
    Vector2<FpType> qpDir = dDir.rotate90DegCCW();
    Vector2<FpType> p = q + (qpDir * h);

    result = p;
    return true;
}

/**
 * Calculate a number which explains the position of a third point with respect
 * to two points.
 * 
 * @param a First point.
 * @param b Second point.
 * @param c Third point.
 * @return A number whose sign indicates the position of `c` with respect to `a`
 *         and `b`; positive means that `c` is on the left side of a directed
 *         line from `a` to `b`.
 */
template <typename FpType>
FpType triangleWindingOrder (Vector2<FpType> a, Vector2<FpType> b, Vector2<FpType> c)
{
    return (b.m_v[0] - a.m_v[0]) * (c.m_v[1] - a.m_v[1]) -
           (b.m_v[1] - a.m_v[1]) * (c.m_v[0] - a.m_v[0]);
}

}

#endif
