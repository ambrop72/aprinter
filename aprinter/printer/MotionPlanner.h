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
#include <stddef.h>
#include <limits.h>
#include <math.h>

#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/Tuple.h>
#include <aprinter/meta/TupleForEach.h>
#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/TypeListLength.h>
#include <aprinter/meta/TypeListFold.h>
#include <aprinter/meta/IndexElemTuple.h>
#include <aprinter/meta/TupleGet.h>
#include <aprinter/meta/TypeListFold.h>
#include <aprinter/meta/BoundedInt.h>
#include <aprinter/meta/CopyUnrolled.h>
#include <aprinter/meta/WrapMethod.h>
#include <aprinter/meta/Position.h>
#include <aprinter/meta/TuplePosition.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/OffsetCallback.h>
#include <aprinter/math/FloatTools.h>

#include <aprinter/BeginNamespace.h>

template <
    typename TTheAxisStepper,
    typename TGetAxisStepper,
    template<typename> class TSplitter,
    int TStepperBufferSizeExp
>
struct MotionPlannerAxisSpec {
    using TheAxisStepper = TTheAxisStepper;
    using GetAxisStepper = TGetAxisStepper;
    template <typename X> using Splitter = TSplitter<X>;
    static const int StepperBufferSizeExp = TStepperBufferSizeExp;
};

template <typename Position, typename Context, typename AxesList, typename PullHandler, typename FinishedHandler>
class MotionPlanner
: private DebugObject<Context, void>
{
private:
    template <int AxisIndex> struct AxisPosition;
    
    using Loop = typename Context::EventLoop;
    using TimeType = typename Context::Clock::TimeType;
    static const int NumAxes = TypeListLength<AxesList>::value;
    enum {ALL_CMD_END = 3 * NumAxes};
    static_assert(ALL_CMD_END <= UINT8_MAX, "");
    template <typename AxisSpec, typename AccumType>
    using MinTimeTypeHelper = FixedIntersectTypes<typename AxisSpec::template Splitter<typename AxisSpec::TheAxisStepper>::TimeFixedType, AccumType>;
    using MinTimeType = TypeListFold<AxesList, FixedIdentity, MinTimeTypeHelper>;
    
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_init, init)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_deinit, deinit)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_commandDone_assert, commandDone_assert)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_commandDone_compute_vel, commandDone_compute_vel)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_commandDone_compute_acc, commandDone_compute_acc)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_commandDone_compute, commandDone_compute)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_work_command, work_command)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_set_generated_ready, set_generated_ready)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_set_stepper_ready, set_stepper_ready)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_start_stepping, start_stepping)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_is_finished, is_finished)
    
public:
    template <int AxisIndex>
    struct AxisInputCommand {
        using AxisSpec = TypeListGet<AxesList, AxisIndex>;
        using TheAxisStepper = typename AxisSpec::TheAxisStepper;
        using TheSplitter = typename AxisSpec::template Splitter<TheAxisStepper>;
        using StepFixedType = typename TheSplitter::StepFixedType;
        
        bool dir;
        StepFixedType x;
        double max_v;
        double max_a;
    };
    
    struct InputCommand {
        double rel_max_v;
        IndexElemTuple<AxesList, AxisInputCommand> axes;
    };
    
public:
    template <int AxisIndex>
    class Axis {
    public:
        using AxisSpec = TypeListGet<AxesList, AxisIndex>;
        using TheAxisStepper = typename AxisSpec::TheAxisStepper;
        using TheSplitter = typename AxisSpec::template Splitter<TheAxisStepper>;
        using StepFixedType = typename TheSplitter::StepFixedType;
        using TimeFixedType = typename TheSplitter::TimeFixedType;
        using AccelFixedType = typename TheSplitter::AccelFixedType;
        using TheAxisInputCommand = AxisInputCommand<AxisIndex>;
        
    private:
        friend MotionPlanner;
        
        using StepperBufferSizeType = BoundedInt<AxisSpec::StepperBufferSizeExp, false>;
        using StepperCommand = typename TheAxisStepper::Command;
        using StepperCommandCallbackContext = typename TheAxisStepper::CommandCallbackContext;
        enum {CMD_END = 3};
        
        MotionPlanner * parent ()
        {
            return AMBRO_WMEMB_TD(&MotionPlanner::m_axes)::container(TupleGetTuple<AxisIndex, AxesTuple>(this));
        }
        
        TheAxisStepper * stepper ()
        {
            return AxisSpec::GetAxisStepper::call(parent());
        }
        
        void init (Context c)
        {
            m_stepper_event.init(c, AMBRO_OFFSET_CALLBACK_T(&Axis::m_stepper_event, &Axis::stepper_event_handler));
            m_sbuf_start = StepperBufferSizeType::import(0);
            m_sbuf_stepper_ready_end = StepperBufferSizeType::import(0);
            m_sbuf_generated_ready_end = StepperBufferSizeType::import(0);
            m_sbuf_end = StepperBufferSizeType::import(0);
            m_command_pos = CMD_END;
#ifdef AMBROLIB_ASSERTIONS
            m_stepping = false;
#endif
        }
        
        void deinit (Context c)
        {
            stepper()->stop(c);
            m_stepper_event.deinit(c);
        }
        
        void commandDone_assert (Context c, InputCommand icmd)
        {
            TheAxisInputCommand *axis_icmd = TupleGetElem<AxisIndex>(&icmd.axes);
            AMBRO_ASSERT(m_command_pos == CMD_END)
            AMBRO_ASSERT(FloatIsPosOrPosZero(axis_icmd->max_v))
            AMBRO_ASSERT(FloatIsPosOrPosZero(axis_icmd->max_a))
        }
        
        double commandDone_compute_vel (double accum_vel, Context c, InputCommand icmd)
        {
            TheAxisInputCommand *axis_icmd = TupleGetElem<AxisIndex>(&icmd.axes);
            return fmin(accum_vel, axis_icmd->max_v / axis_icmd->x.doubleValue());
        }
        
        double commandDone_compute_acc (double accum_acc, Context c, InputCommand icmd)
        {
            TheAxisInputCommand *axis_icmd = TupleGetElem<AxisIndex>(&icmd.axes);
            return fmin(accum_acc, axis_icmd->max_a / axis_icmd->x.doubleValue());
        }
        
        void commandDone_compute (Context c, InputCommand icmd, double norm_acc_x, MinTimeType t02, MinTimeType t1)
        {
            TheAxisInputCommand *axis_icmd = TupleGetElem<AxisIndex>(&icmd.axes);
            
            StepFixedType x = StepFixedType::importDoubleSaturated(axis_icmd->x.doubleValue() * norm_acc_x);
            if (x.m_bits.m_int > axis_icmd->x.bitsValue() / 2) {
                x.m_bits.m_int = axis_icmd->x.bitsValue() / 2;
            }
            
            m_x[1] = x;
            m_x[0] = StepFixedType::importBits(axis_icmd->x.bitsValue() - x.bitsValue() - x.bitsValue());
            m_t[1] = t02;
            m_t[0] = t1;
            m_a[1] = -x;
            m_a[0] = AccelFixedType::importBits(0);
            
            m_command_pos = 0;
            m_splitter.setInput(axis_icmd->dir, x, t02, x.toSigned());
        }
        
        void work_command (Context c)
        {
            MotionPlanner *o = parent();
            AMBRO_ASSERT(m_command_pos < CMD_END)
            AMBRO_ASSERT(o->m_all_command_pos < ALL_CMD_END)
            
            while (1) {
                StepperBufferSizeType start;
                AMBRO_LOCK_T(o->m_lock, c, lock_c, {
                    start = m_sbuf_start;
                });
                if (BoundedModuloSubtract(start, m_sbuf_end).value() == 1) {
                    return;
                }
                bool part_finished;
                m_splitter.getOutput(&m_sbuf[m_sbuf_end.value()], &part_finished);
                m_sbuf_end = BoundedModuloInc(m_sbuf_end);
                if (part_finished) {
                    m_command_pos++;
                    o->m_all_command_pos++;
                    if (m_command_pos == CMD_END) {
                        return;
                    }
                    m_splitter.setInput(m_splitter.m_command.dir, m_x[m_command_pos - 1], m_t[m_command_pos - 1], m_a[m_command_pos - 1]);
                }
            }
        }
        
        void set_generated_ready ()
        {
            m_sbuf_generated_ready_end = m_sbuf_end;
        }
        
        void set_stepper_ready ()
        {
            m_sbuf_stepper_ready_end = m_sbuf_generated_ready_end;
        }
        
        void is_finished (bool *out)
        {
            *out &= (m_sbuf_start == m_sbuf_end);
        }
        
        void start_stepping (Context c, TimeType start_time)
        {
            MotionPlanner *o = parent();
            AMBRO_ASSERT(!m_stepping)
            AMBRO_ASSERT(m_sbuf_start != m_sbuf_generated_ready_end)
            AMBRO_ASSERT(o->m_num_stepping == NumAxes)
            AMBRO_ASSERT(!o->m_continue)
            
            m_sbuf_stepper_ready_end = m_sbuf_generated_ready_end;
#ifdef AMBROLIB_ASSERTIONS
            m_stepping = true;
#endif
            stepper()->template start<TheAxisStepperConsumer<AxisIndex>>(c, start_time, &m_sbuf[m_sbuf_start.value()]);
        }
        
        void stepper_event_handler (Context c)
        {
            MotionPlanner *o = parent();
            
            if (m_command_pos < CMD_END) {
                work_command(c);
                if (o->m_all_command_pos == ALL_CMD_END) {
                    return o->command_completed(c);
                }
            }
            
            uint8_t num_stepping;
            AMBRO_LOCK_T(o->m_lock, c, lock_c, {
                num_stepping = o->m_num_stepping;
            });
            if (num_stepping == 0) {
                if (o->m_continue) {
                    o->start_stepping(c);
                } else if (o->m_waiting) {
                    o->m_pull_finished_event.appendNow(c);
                }
            }
        }
        
        StepperCommand * stepper_command_callback (StepperCommandCallbackContext c)
        {
            MotionPlanner *o = parent();
            AMBRO_ASSERT(m_stepping)
            
            StepperCommand *result;
            
            AMBRO_LOCK_T(o->m_lock, c, lock_c, {
                AMBRO_ASSERT(m_sbuf_start != m_sbuf_stepper_ready_end)
                
                m_sbuf_start = BoundedModuloInc(m_sbuf_start);
                if (m_sbuf_start == m_sbuf_stepper_ready_end) {
                    result = NULL;
#ifdef AMBROLIB_ASSERTIONS
                    m_stepping = false;
#endif
                    o->m_num_stepping--;
                } else {
                    result = &m_sbuf[m_sbuf_start.value()];
                }
                
                if (!m_stepper_event.isSet(lock_c)) {
                    m_stepper_event.appendNow(lock_c);
                }
            });
            
            return result;
        }
        
        typename Loop::QueuedEvent m_stepper_event;
        StepperBufferSizeType m_sbuf_start;
        StepperBufferSizeType m_sbuf_stepper_ready_end;
        StepperBufferSizeType m_sbuf_generated_ready_end;
        StepperBufferSizeType m_sbuf_end;
        uint8_t m_command_pos;
        StepFixedType m_x[2];
        TimeFixedType m_t[2];
        AccelFixedType m_a[2];
        TheSplitter m_splitter;
        StepperCommand m_sbuf[(size_t)StepperBufferSizeType::maxIntValue() + 1];
#ifdef AMBROLIB_ASSERTIONS
        bool m_stepping;
#endif
    };
    
private:
    using AxesTuple = IndexElemTuple<AxesList, Axis>;
    
public:
    void init (Context c)
    {
        m_lock.init(c);
        m_pull_finished_event.init(c, AMBRO_OFFSET_CALLBACK_T(&MotionPlanner::m_pull_finished_event, &MotionPlanner::pull_finished_event_handler));
        m_all_command_pos = ALL_CMD_END;
        m_num_stepping = 0;
        m_waiting = false;
#ifdef AMBROLIB_ASSERTIONS
        m_pulling = false;
#endif
        TupleForEachForward(&m_axes, Foreach_init(), c);
        m_pull_finished_event.prependNowNotAlready(c);
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        
        TupleForEachForward(&m_axes, Foreach_deinit(), c);
        m_pull_finished_event.deinit(c);
        m_lock.deinit(c);
    }
    
    void commandDone (Context c, InputCommand icmd)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_pulling)
        AMBRO_ASSERT(m_all_command_pos == ALL_CMD_END)
        AMBRO_ASSERT(FloatIsPosOrPosZero(icmd.rel_max_v))
        TupleForEachForward(&m_axes, Foreach_commandDone_assert(), c, icmd);
        
        double norm_v = TupleForEachForwardAccRes(&m_axes, icmd.rel_max_v, Foreach_commandDone_compute_vel(), c, icmd);
        double norm_a = TupleForEachForwardAccRes(&m_axes, INFINITY, Foreach_commandDone_compute_acc(), c, icmd);
        double norm_acc_x = fmin(0.5, (norm_v * norm_v) / (2 * norm_a));
        double norm_con_x = 1.0 - (2 * norm_acc_x);
        MinTimeType t02 = MinTimeType::importDoubleSaturated(sqrt((2 * norm_acc_x) / norm_a));
        MinTimeType t1 = MinTimeType::importDoubleSaturated(norm_con_x / norm_v);
        TupleForEachForward(&m_axes, Foreach_commandDone_compute(), c, icmd, norm_acc_x, t02, t1);
        
        m_pull_finished_event.unset(c);
        m_waiting = false;
#ifdef AMBROLIB_ASSERTIONS
        m_pulling = false;
#endif
        m_all_command_pos = 0;
        TupleForEachForward(&m_axes, Foreach_work_command(), c);
        if (m_all_command_pos == ALL_CMD_END) {
            command_completed(c);
        }
    }
    
    void waitFinished (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_pulling)
        AMBRO_ASSERT(!m_waiting)
        
        m_waiting = true;
        if (is_finished(c)) {
            m_pull_finished_event.appendNowNotAlready(c);
        }
    }
    
    template <int AxisIndex>
    using TheAxisStepperConsumer = AxisStepperConsumer<
        AxisPosition<AxisIndex>,
        AMBRO_WMETHOD_T(&Axis<AxisIndex>::stepper_command_callback)
    >;
    
private:
    void command_completed (Context c)
    {
        AMBRO_ASSERT(m_all_command_pos == ALL_CMD_END)
        
        TupleForEachForward(&m_axes, Foreach_set_generated_ready());
        uint8_t num_stepping;
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            num_stepping = m_num_stepping;
            if (num_stepping == NumAxes) {
                TupleForEachForward(&m_axes, Foreach_set_stepper_ready());
            }
        });
        if (num_stepping != NumAxes) {
            m_continue = true;
            if (num_stepping == 0) {
                start_stepping(c);
            }
        }
        m_pull_finished_event.prependNowNotAlready(c);
    }
    
    void start_stepping (Context c)
    {
        AMBRO_ASSERT(m_num_stepping == 0)
        AMBRO_ASSERT(m_continue)
        
        m_num_stepping = NumAxes;
        m_continue = false;
        TimeType start_time = c.clock()->getTime(c);
        TupleForEachForward(&m_axes, Foreach_start_stepping(), c, start_time);
    }
    
    void pull_finished_event_handler (Context c)
    {
        this->debugAccess(c);
        
        if (m_waiting) {
            AMBRO_ASSERT(m_pulling)
            AMBRO_ASSERT(is_finished(c))
            
            m_waiting = false;
            return FinishedHandler::call(this, c);
        } else {
            AMBRO_ASSERT(!m_pulling)
            AMBRO_ASSERT(m_all_command_pos == ALL_CMD_END)
            
#ifdef AMBROLIB_ASSERTIONS
            m_pulling = true;
#endif
            return PullHandler::call(this, c);
        }
    }
    
    bool is_finished (Context c)
    {
        AMBRO_ASSERT(m_waiting)
        AMBRO_ASSERT(m_pulling)
        AMBRO_ASSERT(m_all_command_pos == ALL_CMD_END)
        
        bool finished = true;
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            TupleForEachForward(&m_axes, Foreach_is_finished(), &finished);
        });
        
        return finished;
    }
    
    typename Context::Lock m_lock;
    typename Loop::QueuedEvent m_pull_finished_event;
    uint8_t m_all_command_pos;
    uint8_t m_num_stepping;
    bool m_continue;
    bool m_waiting;
#ifdef AMBROLIB_ASSERTIONS
    bool m_pulling;
#endif
    AxesTuple m_axes;
    
    template <int AxisIndex> struct AxisPosition : public TuplePosition<Position, AxesTuple, &MotionPlanner::m_axes, AxisIndex> {};
};

#include <aprinter/EndNamespace.h>

#endif
