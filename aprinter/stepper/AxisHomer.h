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

#include <aprinter/meta/WrapCallback.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/ForwardHandler.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/stepper/MotionPlanner.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename Sharer, typename AbsVelFixedType, typename AbsAccFixedType, typename SwitchPin, bool SwitchInvert, bool HomeDir, typename FinishedHandler>
class AxisHomer
: private DebugObject<Context, void>
{
private:
    struct PlannerPullCmdHandler;
    struct PlannerBufferFullHandler;
    struct PlannerBufferEmptyHandler;
    struct PinWatcherHandler;
    
    using PlannerAxes = MakeTypeList<
        MotionPlannerAxisSpec<Sharer, AbsVelFixedType, AbsAccFixedType>
    >;
    
    using Planner = MotionPlanner<Context, PlannerAxes, PlannerPullCmdHandler, PlannerBufferFullHandler, PlannerBufferEmptyHandler>;
    
public:
    using StepFixedType = typename Planner::template Axis<0>::StepFixedType;
    
    struct HomingParams {
        StepFixedType fast_max_dist;
        StepFixedType retract_dist;
        StepFixedType slow_max_dist;
        AbsVelFixedType fast_speed;
        AbsVelFixedType retract_speed;
        AbsVelFixedType slow_speed;
        AbsAccFixedType max_accel;
    };
    
    void init (Context c, Sharer *sharer)
    {
        typename Planner::SharersTuple sharers;
        sharers.elem = sharer;
        
        m_planner.init(c, sharers);
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        AMBRO_ASSERT(!m_planner.isRunning(c))
        
        m_planner.deinit(c);
    }
    
    void start (Context c, HomingParams params)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(!m_planner.isRunning(c))
        AMBRO_ASSERT(params.fast_max_dist.bitsValue() > 0)
        AMBRO_ASSERT(params.retract_dist.bitsValue() > 0)
        AMBRO_ASSERT(params.slow_max_dist.bitsValue() > 0)
        AMBRO_ASSERT(params.fast_speed.bitsValue() > 0)
        AMBRO_ASSERT(params.retract_speed.bitsValue() > 0)
        AMBRO_ASSERT(params.slow_speed.bitsValue() > 0)
        AMBRO_ASSERT(params.max_accel.bitsValue() > 0)
        
        m_planner.start(c, 0);
        m_state = STATE_FAST;
        m_command_sent = false;
        m_params = params;
    }
    
    void stop (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_planner.isRunning(c))
        
        if (m_state != STATE_RETRACT && m_planner.isStepping(c)) {
            m_pinwatcher.deinit(c);
        }
        m_planner.stop(c);
    }
    
    bool isRunning (Context c)
    {
        this->debugAccess(c);
        
        return m_planner.isRunning(c);
    }
    
private:
    enum {STATE_FAST, STATE_RETRACT, STATE_SLOW};
    
    using PinWatcherService = typename Context::PinWatcherService;
    using PlannerCommand = typename Planner::InputCommand;
    
    void planner_pull_cmd_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_planner.isRunning(c))
        
        if (m_command_sent) {
            if (!m_planner.isStepping(c)) {
                planner_buffer_full_handler(c);
            }
            return;
        }
        
        PlannerCommand cmd;
        switch (m_state) {
            case STATE_FAST: {
                cmd.axes.elem.dir = HomeDir;
                cmd.axes.elem.x = m_params.fast_max_dist;
                cmd.axes.elem.max_v = m_params.fast_speed;
                cmd.axes.elem.max_a = m_params.max_accel;
            } break;
            case STATE_RETRACT: {
                cmd.axes.elem.dir = !HomeDir;
                cmd.axes.elem.x = m_params.retract_dist;
                cmd.axes.elem.max_v = m_params.retract_speed;
                cmd.axes.elem.max_a = m_params.max_accel;
            } break;
            case STATE_SLOW: {
                cmd.axes.elem.dir = HomeDir;
                cmd.axes.elem.x = m_params.slow_max_dist;
                cmd.axes.elem.max_v = m_params.slow_speed;
                cmd.axes.elem.max_a = m_params.max_accel;
            } break;
        }
        
        m_planner.commandDone(c, cmd);
        m_command_sent = true;
    }
    
    void planner_buffer_full_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_planner.isRunning(c))
        AMBRO_ASSERT(!m_planner.isStepping(c))
        AMBRO_ASSERT(m_command_sent)
        
        if (m_state != STATE_RETRACT) {
            m_pinwatcher.init(c);
        }
        m_planner.addTime(c, c.clock()->getTime(c));
        m_planner.startStepping(c);
    }
    
    void planner_buffer_empty_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_planner.isRunning(c))
        AMBRO_ASSERT(m_planner.isStepping(c))
        
        if (m_state != STATE_RETRACT) {
            m_pinwatcher.deinit(c);
            m_planner.stop(c);
            return FinishedHandler::call(this, c, false);
        }
        
        m_planner.stop(c);
        m_planner.start(c, 0);
        m_state = STATE_SLOW;
        m_command_sent = false;
    }
    
    void pin_watcher_handler (Context c, bool state)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_planner.isRunning(c))
        AMBRO_ASSERT(m_planner.isStepping(c))
        AMBRO_ASSERT(m_state == STATE_FAST || m_state == STATE_SLOW)
        
        if (state == SwitchInvert) {
            return;
        }
        
        m_planner.stop(c);
        m_pinwatcher.deinit(c);
        
        if (m_state == STATE_SLOW) {
            return FinishedHandler::call(this, c, true);
        }
        
        m_planner.start(c, 0);
        m_state++;
        m_command_sent = false;
    }
    
    Planner m_planner;
    typename PinWatcherService::template PinWatcher<SwitchPin, PinWatcherHandler> m_pinwatcher;
    uint8_t m_state;
    bool m_command_sent;
    HomingParams m_params;
    
    struct PlannerPullCmdHandler : public AMBRO_WCALLBACK_TD(&AxisHomer::planner_pull_cmd_handler, &AxisHomer::m_planner) {};
    struct PlannerBufferFullHandler : public AMBRO_WCALLBACK_TD(&AxisHomer::planner_buffer_full_handler, &AxisHomer::m_planner) {};
    struct PlannerBufferEmptyHandler : public AMBRO_WCALLBACK_TD(&AxisHomer::planner_buffer_empty_handler, &AxisHomer::m_planner) {};
    struct PinWatcherHandler : public AMBRO_WCALLBACK_TD(&AxisHomer::pin_watcher_handler, &AxisHomer::m_pinwatcher) {};
};

#include <aprinter/EndNamespace.h>

#endif
