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
#include <limits.h>

#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/Tuple.h>
#include <aprinter/meta/TupleForEach.h>
#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/TypeListLength.h>
#include <aprinter/meta/TypeListFold.h>
#include <aprinter/meta/IndexElemTuple.h>
#include <aprinter/meta/TupleGet.h>
#include <aprinter/meta/GetMemberTypeFunc.h>
#include <aprinter/meta/MapElemTuple.h>
#include <aprinter/meta/PointerFunc.h>
#include <aprinter/meta/ComposeFunctions.h>
#include <aprinter/meta/TypeListFold.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/OffsetCallback.h>

#include <aprinter/BeginNamespace.h>

template <typename TSharer, typename TAbsVelFixedType, typename TAbsAccFixedType>
struct MotionPlannerAxisSpec {
    using Sharer = TSharer;
    using AbsVelFixedType = TAbsVelFixedType;
    using AbsAccFixedType = TAbsAccFixedType;
};

template <typename Context, typename AxesList, typename PullCmdHandler, typename BufferFullHandler, typename BufferEmptyHandler, int AxisIndex>
class MotionPlannerAxis;

template <typename Context, typename AxesList, typename PullCmdHandler, typename BufferFullHandler, typename BufferEmptyHandler>
class MotionPlanner
: private DebugObject<Context, void>
{
private:
    template <typename, typename, typename, typename, typename, int>
    friend class MotionPlannerAxis;
    
    using Loop = typename Context::EventLoop;
    
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetFunc_Sharer, Sharer)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_init, init)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_deinit, deinit)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_start, start)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_stop, stop)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_addTime, addTime)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_startStepping, startStepping)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_stopStepping, stopStepping)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_commandDone_assert, commandDone_assert)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_commandDone_compute_vel, commandDone_compute_vel)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_commandDone_compute_acc, commandDone_compute_acc)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_commandDone_compute, commandDone_compute)
    
    enum {NUM_AXES = TypeListLength<AxesList>::value};
    enum {ALL_CMD_END = 3 * NUM_AXES};
    
    static_assert(ALL_CMD_END <= UINT8_MAX, "");
    
    template <typename TheAxisSpec, typename AccumType>
    using MinTimeTypeHelper = FixedIntersectTypes<typename TheAxisSpec::Sharer::Axis::TimeFixedType, AccumType>;
    template <typename TheAxisSpec, typename AccumType>
    using MaxStepTypeHelper = FixedUnionTypes<typename TheAxisSpec::Sharer::Axis::StepFixedType, AccumType>;
    template <typename TheAxisSpec, typename AccumType>
    using RelSpeedTypeHelper = decltype(FixedMin(FixedDivide<true>(typename TheAxisSpec::AbsVelFixedType(), typename TheAxisSpec::Sharer::Axis::StepFixedType()), AccumType()));
    
    using MinTimeType = TypeListFold<AxesList, FixedIdentity, MinTimeTypeHelper>;
    using MaxStepType = TypeListFold<AxesList, FixedIdentity, MaxStepTypeHelper>;
    
public:
    using TimeType = typename Context::Clock::TimeType;
    using SharersTuple = MapElemTuple<AxesList, ComposeFunctions<PointerFunc, GetFunc_Sharer>>;
    using RelSpeedType = TypeListFold<AxesList, FixedIdentity, RelSpeedTypeHelper>;
    
    struct InputCommand;
    
    template <int AxisIndex>
    using Axis = MotionPlannerAxis<Context, AxesList, PullCmdHandler, BufferFullHandler, BufferEmptyHandler, AxisIndex>;
    
private:
    using AxesTuple = IndexElemTuple<AxesList, Axis>;
    
public:
    void init (Context c, SharersTuple sharers)
    {
        m_pull_event.init(c, AMBRO_OFFSET_CALLBACK_T(&MotionPlanner::m_pull_event, &MotionPlanner::pull_event_handler));
        m_full_event.init(c, AMBRO_OFFSET_CALLBACK_T(&MotionPlanner::m_full_event, &MotionPlanner::full_event_handler));
        m_empty_event.init(c, AMBRO_OFFSET_CALLBACK_T(&MotionPlanner::m_empty_event, &MotionPlanner::empty_event_handler));
        TupleForEachForward(&m_axes, Foreach_init(), c, sharers);
        m_running = false;
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        AMBRO_ASSERT(!m_running)
        
        TupleForEachReverse(&m_axes, Foreach_deinit(), c);
        m_empty_event.deinit(c);
        m_full_event.deinit(c);
        m_pull_event.deinit(c);
    }
    
    void start (Context c, TimeType start_time)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(!m_running)
        
        TupleForEachForward(&m_axes, Foreach_start(), c, start_time);
        m_running = true;
        m_stepping = false;
        m_pulling = false;
        m_all_command_state = ALL_CMD_END;
        m_all_full = 0;
        m_all_empty = 0;
    }
    
    void stop (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        
        TupleForEachReverse(&m_axes, Foreach_stop(), c);
        m_running = false;
        m_pull_event.unset(c);
        m_full_event.unset(c);
        m_empty_event.unset(c);
    }
    
    void addTime (Context c, TimeType time_add)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        AMBRO_ASSERT(!m_stepping)
        
        TupleForEachForward(&m_axes, Foreach_addTime(), c, time_add);
    }
    
    void startStepping (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        AMBRO_ASSERT(!m_stepping)
        AMBRO_ASSERT(m_all_empty == 0)
        
        TupleForEachForward(&m_axes, Foreach_startStepping(), c);
        m_stepping = true;
        m_all_full = 0;
        m_full_event.unset(c);
    }
    
    void stopStepping (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        AMBRO_ASSERT(m_stepping)
        AMBRO_ASSERT(m_all_full == 0)
        
        TupleForEachForward(&m_axes, Foreach_stopStepping(), c);
        m_stepping = false;
        m_all_empty = 0;
        m_empty_event.unset(c);
    }
    
    template <int AxisIndex>
    struct AxisInputCommand {
        using TheAxis = Axis<AxisIndex>;
        bool dir;
        typename TheAxis::StepFixedType x;
        typename TheAxis::AbsVelFixedType max_v;
        typename TheAxis::AbsAccFixedType max_a;
    };
    
    struct InputCommand {
        RelSpeedType rel_max_v;
        IndexElemTuple<AxesList, AxisInputCommand> axes;
    };
    
    void commandDone (Context c, InputCommand icmd)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        AMBRO_ASSERT(m_pulling)
        AMBRO_ASSERT(m_all_command_state == ALL_CMD_END)
        AMBRO_ASSERT(!m_stepping || m_all_full == 0)
        AMBRO_ASSERT(icmd.rel_max_v.bitsValue() > 0)
        TupleForEachForward(&m_axes, Foreach_commandDone_assert(), c, icmd);
        
        RelSpeedType norm_v = TupleForEachForwardAccRes(&m_axes, FixedIdentity(), Foreach_commandDone_compute_vel(), c, icmd);
        if (norm_v > icmd.rel_max_v) {
            norm_v = icmd.rel_max_v;
        }
        
        auto norm_a = TupleForEachForwardAccRes(&m_axes, FixedIdentity(), Foreach_commandDone_compute_acc(), c, icmd);
        
        auto norm_try_x = FixedResDivide<-MaxStepType::num_bits, MaxStepType::num_bits, false>(norm_v * norm_v, norm_a.template shift<1>());
        auto norm_half_x = decltype(norm_try_x)::importBits(decltype(norm_try_x)::maxValue().bitsValue() / 2);
        
        if (norm_try_x > norm_half_x) {
            norm_try_x = norm_half_x;
        }
        
        auto norm_rem_x = decltype(norm_try_x)::importBits(
            decltype(norm_try_x)::maxValue().bitsValue() - norm_try_x.bitsValue() - norm_try_x.bitsValue()
        );
        
        MinTimeType t02 = FixedSquareRoot(FixedResDivide<0, (2 * MinTimeType::num_bits), false>(norm_try_x.template shift<1>(), norm_a));
        MinTimeType t1 = FixedResDivide<0, MinTimeType::num_bits, false>(norm_rem_x, norm_v);
        
        m_pulling = false;
        m_all_command_state = 0;
        m_all_empty = 0;
        m_empty_event.unset(c);
        
        TupleForEachForward(&m_axes, Foreach_commandDone_compute(), c, icmd, norm_try_x, t02, t1);
        
        if (m_all_full == NUM_AXES) {
            m_full_event.prependNow(c);
        }
    }
    
    bool isRunning (Context c)
    {
        this->debugAccess(c);
        
        return m_running;
    }
    
    bool isStepping (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        
        return m_stepping;
    }
    
    bool isPulling (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_running)
        
        return m_pulling;
    }
    
private:
    void pull_event_handler (Context c)
    {
        AMBRO_ASSERT(m_running)
        AMBRO_ASSERT(!m_pulling)
        AMBRO_ASSERT(m_all_command_state == ALL_CMD_END)
        AMBRO_ASSERT(m_stepping || m_all_empty == 0)
        
        m_pulling = true;
        
        if (m_all_empty == NUM_AXES) {
            m_empty_event.prependNow(c);
        }
        
        return PullCmdHandler::call(this, c);
    }
    
    void full_event_handler (Context c)
    {
        AMBRO_ASSERT(m_running)
        AMBRO_ASSERT(!m_stepping)
        AMBRO_ASSERT(m_all_full == NUM_AXES)
        AMBRO_ASSERT(m_all_command_state != ALL_CMD_END)
        
        return BufferFullHandler::call(this, c);
    }
    
    void empty_event_handler (Context c)
    {
        AMBRO_ASSERT(m_running)
        AMBRO_ASSERT(m_stepping)
        AMBRO_ASSERT(m_all_empty == NUM_AXES)
        AMBRO_ASSERT(m_all_command_state == ALL_CMD_END)
        AMBRO_ASSERT(m_pulling)
        
        return BufferEmptyHandler::call(this, c);
    }
    
    typename Loop::QueuedEvent m_pull_event;
    typename Loop::QueuedEvent m_full_event;
    typename Loop::QueuedEvent m_empty_event;
    AxesTuple m_axes;
    bool m_running;
    bool m_stepping;
    bool m_pulling;
    uint8_t m_all_command_state;
    uint8_t m_all_full;
    uint8_t m_all_empty;
};

template <typename Context, typename AxesList, typename PullCmdHandler, typename BufferFullHandler, typename BufferEmptyHandler, int AxisIndex>
class MotionPlannerAxis {
public:
    using TheMotionPlanner = MotionPlanner<Context, AxesList, PullCmdHandler, BufferFullHandler, BufferEmptyHandler>;
    using TheAxisInputCommand = typename TheMotionPlanner::template AxisInputCommand<AxisIndex>;
    using TimeType = typename TheMotionPlanner::TimeType;
    using AxisSpec = TypeListGet<AxesList, AxisIndex>;
    using Sharer = typename AxisSpec::Sharer;
    using StepFixedType = typename Sharer::Axis::StepFixedType;
    using TimeFixedType = typename Sharer::Axis::TimeFixedType;
    using AccelFixedType = typename Sharer::Axis::AccelFixedType;
    using AbsVelFixedType = typename AxisSpec::AbsVelFixedType;
    using AbsAccFixedType = typename AxisSpec::AbsAccFixedType;
    
    static_assert(!AbsVelFixedType::is_signed, "");
    static_assert(!AbsAccFixedType::is_signed, "");
    
private:
    friend TheMotionPlanner;
    
    using AxesTuple = typename TheMotionPlanner::AxesTuple;
    using SharersTuple = typename TheMotionPlanner::SharersTuple;
    using InputCommand = typename TheMotionPlanner::InputCommand;
    using MinTimeType = typename TheMotionPlanner::MinTimeType;
    enum {NUM_AXES = TheMotionPlanner::NUM_AXES};
    enum {ALL_CMD_END = TheMotionPlanner::ALL_CMD_END};
    
    TheMotionPlanner * parent ()
    {
        return AMBRO_WMEMB_TD(&TheMotionPlanner::m_axes)::container(TupleGetTuple<AxisIndex, AxesTuple>(this));
    }
    
    void init (Context c, SharersTuple sharers)
    {
        m_user.init(c, *TupleGetElem<AxisIndex>(&sharers),
            AMBRO_OFFSET_CALLBACK_T(&MotionPlannerAxis::m_user, &MotionPlannerAxis::sharer_pull_cmd_handler),
            AMBRO_OFFSET_CALLBACK_T(&MotionPlannerAxis::m_user, &MotionPlannerAxis::sharer_buffer_full_handler),
            AMBRO_OFFSET_CALLBACK_T(&MotionPlannerAxis::m_user, &MotionPlannerAxis::sharer_buffer_empty_handler)
        );
    }
    
    void deinit (Context c)
    {
        m_user.deinit(c);
    }
    
    void start (Context c, TimeType start_time)
    {
        m_user.activate(c);
        m_user.getAxis(c)->start(c, start_time);
        m_user_pulling = false;
        m_command_state = CMD_END;
    }
    
    void stop (Context c)
    {
        m_user.getAxis(c)->stop(c);
        m_user.deactivate(c);
    }
    
    void addTime (Context c, TimeType time_add)
    {
        m_user.getAxis(c)->addTime(c, time_add);
    }
    
    void startStepping (Context c)
    {
        m_user.getAxis(c)->startStepping(c);
    }
    
    void stopStepping (Context c)
    {
        m_user.getAxis(c)->stopStepping(c);
    }
    
    void commandDone_assert (Context c, InputCommand icmd)
    {
        TheAxisInputCommand *axis_icmd = TupleGetElem<AxisIndex>(&icmd.axes);
        AMBRO_ASSERT(m_command_state == CMD_END)
        AMBRO_ASSERT(axis_icmd->max_v.bitsValue() > 0)
        AMBRO_ASSERT(axis_icmd->max_a.bitsValue() > 0)
    }
    
    template <typename AccumVel>
    auto commandDone_compute_vel (AccumVel accum_vel, Context c, InputCommand icmd)
    {
        TheAxisInputCommand *axis_icmd = TupleGetElem<AxisIndex>(&icmd.axes);
        auto norm_v = FixedDivide<true>(axis_icmd->max_v, axis_icmd->x);
        return FixedMin(norm_v, accum_vel);
    }
    
    template <typename AccumAcc>
    auto commandDone_compute_acc (AccumAcc accum_acc, Context c, InputCommand icmd)
    {
        TheAxisInputCommand *axis_icmd = TupleGetElem<AxisIndex>(&icmd.axes);
        auto norm_a = FixedDivide<true>(axis_icmd->max_a, axis_icmd->x);
        return FixedMin(norm_a, accum_acc);
    }
    
    template <typename NormTryX>
    void commandDone_compute (Context c, InputCommand icmd, NormTryX norm_try_x, MinTimeType t02, MinTimeType t1)
    {
        TheMotionPlanner *o = parent();
        TheAxisInputCommand *axis_icmd = TupleGetElem<AxisIndex>(&icmd.axes);
        
        StepFixedType x = FixedResMultiply(axis_icmd->x, norm_try_x);
        
        m_command.dir = axis_icmd->dir;
        m_command.x[0] = x;
        m_command.x[2] = x;
        m_command.x[1] = StepFixedType::importBits(axis_icmd->x.bitsValue() - x.bitsValue() - x.bitsValue());
        m_command.t[0] = t02;
        m_command.t[2] = t02;
        m_command.t[1] = t1;
        m_command.a[0] = m_command.x[0].toSigned();
        m_command.a[2] = -m_command.x[2];
        m_command.a[1] = AccelFixedType::importBits(0);
        
        m_command_state = 0;
        
        if (m_user_pulling) {
            m_user.getAxis(c)->commandDone(c, m_command.dir, m_command.x[m_command_state], m_command.t[m_command_state], m_command.a[m_command_state]);
            m_command_state++;
            o->m_all_command_state++;
            m_user_pulling = false;
        }
    }
    
    void sharer_pull_cmd_handler (Context c)
    {
        TheMotionPlanner *o = parent();
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_running)
        AMBRO_ASSERT(!m_user_pulling)
        AMBRO_ASSERT(o->m_all_full < NUM_AXES)
        
        bool returning_command = (m_command_state < CMD_END);
        if (returning_command) {
            o->m_all_command_state++;
        } else {
            m_user_pulling = true;
        }
        
        if (o->m_all_command_state == ALL_CMD_END) {
            if (!o->m_pulling) {
                o->m_pull_event.prependNow(c);
            }
        }
        
        if (returning_command) {
            m_user.getAxis(c)->commandDone(c, m_command.dir, m_command.x[m_command_state], m_command.t[m_command_state], m_command.a[m_command_state]);
            m_command_state++;
        }
    }
    
    void sharer_buffer_full_handler (Context c)
    {
        TheMotionPlanner *o = parent();
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_running)
        AMBRO_ASSERT(!o->m_stepping)
        AMBRO_ASSERT(!m_user_pulling)
        AMBRO_ASSERT(o->m_all_full < NUM_AXES)
        
        o->m_all_full++;
        
        if (o->m_all_full == NUM_AXES && o->m_all_command_state != ALL_CMD_END) {
            o->m_full_event.prependNow(c);
        }
    }
    
    void sharer_buffer_empty_handler (Context c)
    {
        TheMotionPlanner *o = parent();
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_running)
        AMBRO_ASSERT(o->m_stepping)
        AMBRO_ASSERT(m_user_pulling)
        AMBRO_ASSERT(o->m_all_empty < NUM_AXES)
        
        o->m_all_empty++;
        
        if (o->m_all_empty == NUM_AXES && o->m_pulling) {
            o->m_empty_event.prependNow(c);
        }
    }
    
    enum {CMD_END = 3};
    
    struct Command {
        bool dir;
        StepFixedType x[3];
        TimeFixedType t[3];
        AccelFixedType a[3];
    };
    
    typename Sharer::User m_user;
    bool m_user_pulling;
    uint8_t m_command_state;
    Command m_command;
};

#include <aprinter/EndNamespace.h>

#endif
