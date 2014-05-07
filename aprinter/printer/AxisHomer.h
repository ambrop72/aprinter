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

#ifndef AMBROLIB_AXIS_HOMER_H
#define AMBROLIB_AXIS_HOMER_H

#include <stdint.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/printer/MotionPlanner.h>

#include <aprinter/BeginNamespace.h>

template <
    typename Context, typename ParentObject, typename TheAxisStepper,
    int PlannerStepBits,
    typename PlannerDistanceFactor, typename PlannerCorneringDistance,
    int StepperSegmentBufferSize, int MaxLookaheadBufferSize, typename FpType,
    typename SwitchPin, bool SwitchInvert, bool HomeDir,
    typename FinishedHandler
>
class AxisHomer {
public:
    struct Object;
    
private:
    struct PlannerPullHandler;
    struct PlannerFinishedHandler;
    struct PlannerAbortedHandler;
    struct PlannerUnderrunCallback;
    struct PlannerPrestepCallback;
    
    static int const LookaheadBufferSize = min(MaxLookaheadBufferSize, 3);
    static int const LookaheadCommitCount = 1;
    
    using PlannerAxes = MakeTypeList<MotionPlannerAxisSpec<TheAxisStepper, PlannerStepBits, PlannerDistanceFactor, PlannerCorneringDistance, PlannerPrestepCallback>>;
    using Planner = MotionPlanner<Context, Object, PlannerAxes, StepperSegmentBufferSize, LookaheadBufferSize, LookaheadCommitCount, FpType, PlannerPullHandler, PlannerFinishedHandler, PlannerAbortedHandler, PlannerUnderrunCallback>;
    using PlannerCommand = typename Planner::SplitBuffer;
    enum {STATE_FAST, STATE_RETRACT, STATE_SLOW, STATE_END};
    
public:
    using StepFixedType = typename Planner::template Axis<0>::StepFixedType;
    
    struct HomingParams {
        StepFixedType fast_max_dist;
        StepFixedType retract_dist;
        StepFixedType slow_max_dist;
        FpType fast_speed;
        FpType retract_speed;
        FpType slow_speed;
        FpType max_accel;
    };
    
    static void init (Context c, HomingParams params)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(params.fast_max_dist.bitsValue() > 0)
        AMBRO_ASSERT(params.retract_dist.bitsValue() > 0)
        AMBRO_ASSERT(params.slow_max_dist.bitsValue() > 0)
        AMBRO_ASSERT(FloatIsPosOrPosZero(params.fast_speed))
        AMBRO_ASSERT(FloatIsPosOrPosZero(params.retract_speed))
        AMBRO_ASSERT(FloatIsPosOrPosZero(params.slow_speed))
        AMBRO_ASSERT(FloatIsPosOrPosZero(params.max_accel))
        
        Planner::init(c, true);
        o->m_state = STATE_FAST;
        o->m_command_sent = false;
        o->m_params = params;
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->debugDeinit(c);
        
        if (o->m_state != STATE_END) {
            Planner::deinit(c);
        }
    }
    
    using TheAxisStepperConsumer = typename Planner::template TheAxisStepperConsumer<0>;
    using EventLoopFastEvents = typename Planner::EventLoopFastEvents;
    
private:
    static void planner_pull_handler (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state != STATE_END)
        
        if (o->m_command_sent) {
            Planner::waitFinished(c);
            return;
        }
        
        PlannerCommand *cmd = Planner::getBuffer(c);
        cmd->axes.rel_max_v_rec = 0.0f;
        switch (o->m_state) {
            case STATE_FAST: {
                cmd->axes.axes()->elem.dir = HomeDir;
                cmd->axes.axes()->elem.x = o->m_params.fast_max_dist;
                cmd->axes.axes()->elem.max_v_rec = 1.0f / o->m_params.fast_speed;
                cmd->axes.axes()->elem.max_a_rec = 1.0f / o->m_params.max_accel;
            } break;
            case STATE_RETRACT: {
                cmd->axes.axes()->elem.dir = !HomeDir;
                cmd->axes.axes()->elem.x = o->m_params.retract_dist;
                cmd->axes.axes()->elem.max_v_rec = 1.0f / o->m_params.retract_speed;
                cmd->axes.axes()->elem.max_a_rec = 1.0f / o->m_params.max_accel;
            } break;
            case STATE_SLOW: {
                cmd->axes.axes()->elem.dir = HomeDir;
                cmd->axes.axes()->elem.x = o->m_params.slow_max_dist;
                cmd->axes.axes()->elem.max_v_rec = 1.0f / o->m_params.slow_speed;
                cmd->axes.axes()->elem.max_a_rec = 1.0f / o->m_params.max_accel;
            } break;
        }
        
        if (cmd->axes.axes()->elem.x.bitsValue() != 0) {
            Planner::axesCommandDone(c);
        } else {
            Planner::emptyDone(c);
        }
        o->m_command_sent = true;
    }
    
    static void planner_finished_handler (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state != STATE_END)
        AMBRO_ASSERT(o->m_command_sent)
        
        Planner::deinit(c);
        
        if (o->m_state != STATE_RETRACT) {
            o->m_state = STATE_END;
            return FinishedHandler::call(c, false);
        }
        
        o->m_state++;
        Planner::init(c, true);
        o->m_command_sent = false;
    }
    
    static void planner_aborted_handler (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state == STATE_FAST || o->m_state == STATE_SLOW)
        
        Planner::deinit(c);
        o->m_state++;
        
        if (o->m_state == STATE_END) {
            return FinishedHandler::call(c, true);
        }
        
        Planner::init(c, o->m_state != STATE_RETRACT);
        o->m_command_sent = false;
    }
    
    static void planner_underrun_callback (Context c)
    {
    }
    
    static bool planner_prestep_callback (typename Planner::template Axis<0>::StepperCommandCallbackContext c)
    {
        return (Context::Pins::template get<SwitchPin>(c) != SwitchInvert);
    }
    
    struct PlannerPullHandler : public AMBRO_WFUNC_TD(&AxisHomer::planner_pull_handler) {};
    struct PlannerFinishedHandler : public AMBRO_WFUNC_TD(&AxisHomer::planner_finished_handler) {};
    struct PlannerAbortedHandler : public AMBRO_WFUNC_TD(&AxisHomer::planner_aborted_handler) {};
    struct PlannerUnderrunCallback : public AMBRO_WFUNC_TD(&AxisHomer::planner_underrun_callback) {};
    struct PlannerPrestepCallback : public AMBRO_WFUNC_TD(&AxisHomer::planner_prestep_callback) {};
    
public:
    struct Object : public ObjBase<AxisHomer, ParentObject, MakeTypeList<
        Planner
    >>,
        public DebugObject<Context, void>
    {
        uint8_t m_state;
        bool m_command_sent;
        HomingParams m_params;
    };
};

#include <aprinter/EndNamespace.h>

#endif
