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

#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#include <aprinter/base/Assert.h>
#include <aprinter/printer/LinearPlanner.h>

using namespace APrinter;

using FpType = double;

static constexpr FpType SpeedEpsilon = 0.00001;

struct Segment {
    FpType distance;
    FpType max_speed_squared;
    FpType two_max_accel;
};

struct Path {
    Segment const *segs;
    size_t num_segs;
};

#include "linearplanner_paths.cpp"

using TheLinearPlanner = LinearPlanner<FpType>;

TheLinearPlanner::SegmentData lp_sd[max_path_len];
TheLinearPlanner::SegmentState lp_ss[max_path_len];

static void test_path (Path path)
{
    AMBRO_ASSERT_FORCE(path.num_segs <= max_path_len)
    
    for (size_t i = 0; i < path.num_segs; i++) {
        Segment const *seg = &path.segs[i];
        FpType max_v = seg->max_speed_squared;
        FpType a_x = seg->two_max_accel * seg->distance;
        FpType a_x_rec = 1.0f / a_x;
        TheLinearPlanner::initSegment(&lp_sd[i], max_v, a_x, a_x_rec);
        if (i > 0) {
            TheLinearPlanner::applySegmentJunction(&lp_sd[i - 1], &lp_sd[i], INFINITY);
        }
    }
    
    FpType v = 0.0;
    
    for (size_t j = path.num_segs; j > 0; j--) {
        size_t i = j - 1;
        v = TheLinearPlanner::push(&lp_sd[i], &lp_ss[i], v);
    }
    
    v = 0.0;
    
    for (size_t i = 0; i < path.num_segs; i++) {
        FpType start_v = v;
        TheLinearPlanner::SegmentResult result;
        v = TheLinearPlanner::pull(&lp_sd[i], &lp_ss[i], v, &result);
        
        FpType speed_limit = path.segs[i].max_speed_squared + SpeedEpsilon;
        AMBRO_ASSERT_FORCE(start_v <= speed_limit)
        AMBRO_ASSERT_FORCE(result.const_v <= speed_limit)
        AMBRO_ASSERT_FORCE(v <= speed_limit)
    }
}

int main ()
{
    for (size_t i = 0; i < num_paths; i++) {
        test_path(paths[i]);
    }
}
