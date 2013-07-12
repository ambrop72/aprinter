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

#ifndef AMBROLIB_MOTION_PLANNER_H
#define AMBROLIB_MOTION_PLANNER_H

#include <stdint.h>

#include <aprinter/meta/FixedPoint.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/OffsetCallback.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename Sharer, typename GetSharerHandler, typename PullCmdHandler, typename BufferFullHandler, typename BufferEmptyHandler>
class MotionPlanner
: private DebugObject<Context, void>
{
public:
    static const int velocity_bits = 15;
    static const int velocity_range_exp = -4;
    static const int accel_bits = 15;
    static const int accel_range_exp = -24;
    
    using TimeType = typename Context::Clock::TimeType;
    using StepFixedType = typename Sharer::Axis::StepFixedType;
    using AbsVelFixedType = FixedPoint<velocity_bits, false, (-velocity_bits + velocity_range_exp)>;
    using AbsAccFixedType = FixedPoint<accel_bits, false, (-accel_bits + accel_range_exp)>;
    
    void init (Context c, Sharer *sharer)
    {
        m_user.init(c, sharer,
            AMBRO_OFFSET_CALLBACK_T(&MotionPlanner::m_user, &MotionPlanner::sharer_pull_cmd_handler),
            AMBRO_OFFSET_CALLBACK_T(&MotionPlanner::m_user, &MotionPlanner::sharer_buffer_full_handler),
            AMBRO_OFFSET_CALLBACK_T(&MotionPlanner::m_user, &MotionPlanner::sharer_buffer_empty_handler)
        );
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        AMBRO_ASSERT(!m_user.isActive(c))
        
        m_user.deinit(c);
    }
    
    void start (Context c, TimeType start_time)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(!m_user.isActive(c))
        
        m_user.activate(c);
        m_user.getAxis(c)->start(c, start_time);
        m_pulling = false;
        m_command_state = CMD_END;
    }
    
    void stop (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_user.isActive(c))
        
        m_user.getAxis(c)->stop(c);
        m_user.deactivate(c);
    }
    
    void addTime (Context c, TimeType time_add)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_user.isActive(c))
        AMBRO_ASSERT(!m_user.getAxis(c)->isStepping(c))
        
        m_user.getAxis(c)->addTime(c, time_add);
    }
    
    void startStepping (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_user.isActive(c))
        AMBRO_ASSERT(!m_user.getAxis(c)->isStepping(c))
        
        m_user.getAxis(c)->startStepping(c);
    }
    
    void stopStepping (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_user.isActive(c))
        AMBRO_ASSERT(m_user.getAxis(c)->isStepping(c))
        
        m_user.getAxis(c)->stopStepping(c);
    }
    
    struct InputCommand {
        bool dir;
        StepFixedType x;
        AbsVelFixedType max_v;
        AbsAccFixedType max_a;
    };
    
    void commandDone (Context c, InputCommand icmd)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_user.isActive(c))
        AMBRO_ASSERT(m_pulling)
        AMBRO_ASSERT(m_command_state == CMD_END)
        AMBRO_ASSERT(icmd.x.bitsValue() > 0)
        AMBRO_ASSERT(icmd.max_v.bitsValue() > 0)
        AMBRO_ASSERT(icmd.max_a.bitsValue() > 0)
        
        auto norm_v = FixedDivide<false>(icmd.max_v, icmd.x);
        auto norm_a = FixedDivide<false>(icmd.max_a, icmd.x);
        auto norm_try_x = FixedResDivide<-StepFixedType::num_bits, StepFixedType::num_bits, false>(norm_v * norm_v, norm_a.template shift<1>());
        auto norm_half_x = decltype(norm_try_x)::template powerOfTwo<-1>();
        
        if (norm_try_x > norm_half_x) {
            norm_try_x = norm_half_x;
        }
        
        m_command.dir = icmd.dir;
        
        m_command.x[0] = FixedResMultiply(icmd.x, norm_try_x);
        m_command.x[2] = FixedResMultiply(icmd.x, norm_try_x);
        m_command.x[1] = StepFixedType::importBits(icmd.x.bitsValue() - m_command.x[0].bitsValue() - m_command.x[2].bitsValue());
        
        m_command.t[0] = FixedSquareRoot(FixedResDivide<0, (2 * TimeFixedType::num_bits), false>(m_command.x[0].template shift<1>(), icmd.max_a));
        m_command.t[2] = FixedSquareRoot(FixedResDivide<0, (2 * TimeFixedType::num_bits), false>(m_command.x[2].template shift<1>(), icmd.max_a));
        m_command.t[1] = FixedResDivide<0, TimeFixedType::num_bits, false>(m_command.x[1], icmd.max_v);
        
        m_command.a[0] = m_command.x[0].toSigned();
        m_command.a[2] = -m_command.x[2];
        m_command.a[1] = AccelFixedType::importBits(0);
        
        m_pulling = false;
        
        m_command_state = 0;
        while (m_command.x[m_command_state].bitsValue() == 0) {
            m_command_state++;
            AMBRO_ASSERT(m_command_state < CMD_END)
        }
        
        return_command(c);
    }
    
    bool isRunning (Context c)
    {
        this->debugAccess(c);
        
        return m_user.isActive(c);
    }
    
    bool isStepping (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_user.isActive(c))
        
        return m_user.getAxis(c)->isStepping(c);
    }
    
    bool isPulling (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_user.isActive(c))
        
        return m_pulling;
    }
    
private:
    using TimeFixedType = typename Sharer::Axis::TimeFixedType;
    using AccelFixedType = typename Sharer::Axis::AccelFixedType;
    
    enum {CMD_END = 3};
    
    struct Command {
        bool dir;
        StepFixedType x[3];
        TimeFixedType t[3];
        AccelFixedType a[3];
    };
    
    void return_command (Context c)
    {
        AMBRO_ASSERT(m_user.isActive(c))
        AMBRO_ASSERT(m_command_state < CMD_END)
        AMBRO_ASSERT(m_command.x[m_command_state].bitsValue() != 0)
        
        m_user.getAxis(c)->commandDone(c, m_command.dir, m_command.x[m_command_state], m_command.t[m_command_state], m_command.a[m_command_state]);
        
        do {
            m_command_state++;
        } while (m_command_state < CMD_END && m_command.x[m_command_state].bitsValue() == 0);
    }
    
    Sharer * getSharer ()
    {
        return GetSharerHandler::call(this);
    }
    
    void sharer_pull_cmd_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_user.isActive(c))
        AMBRO_ASSERT(!m_pulling)
        
        if (m_command_state == CMD_END) {
            m_pulling = true;
            return PullCmdHandler::call(this, c);
        }
        
        return_command(c);
    }
    
    void sharer_buffer_full_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_user.isActive(c))
        
        return BufferFullHandler::call(this, c);
    }
    
    void sharer_buffer_empty_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_user.isActive(c))
        
        return BufferEmptyHandler::call(this, c);
    }
    
    typename Sharer::User m_user;
    bool m_pulling;
    uint8_t m_command_state;
    Command m_command;
};

#include <aprinter/EndNamespace.h>

#endif
