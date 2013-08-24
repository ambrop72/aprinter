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

#include <aprinter/meta/BoundedInt.h>
#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/Tuple.h>
#include <aprinter/meta/TupleForEach.h>
#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/TypeListLength.h>
#include <aprinter/meta/TypeListFold.h>
#include <aprinter/meta/IndexElemTuple.h>
#include <aprinter/meta/TupleGet.h>
#include <aprinter/meta/WrapMethod.h>
#include <aprinter/meta/Position.h>
#include <aprinter/meta/TuplePosition.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/PowerOfTwo.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/OffsetCallback.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/printer/LinearPlanner.h>

#include <aprinter/BeginNamespace.h>

template <
    typename TTheAxisStepper,
    typename TGetAxisStepper,
    int TStepBits,
    typename TDistanceFactor,
    typename TCorneringDistance
>
struct MotionPlannerAxisSpec {
    using TheAxisStepper = TTheAxisStepper;
    using GetAxisStepper = TGetAxisStepper;
    static const int StepBits = TStepBits;
    using DistanceFactor = TDistanceFactor;
    using CorneringDistance = TCorneringDistance;
};

template <typename Position, typename Context, typename AxesList, int StepperSegmentBufferSize, int LookaheadBufferSizeExp, typename PullHandler, typename FinishedHandler>
class MotionPlanner
: private DebugObject<Context, void>
{
private:
    template <int AxisIndex> struct AxisPosition;
    
    static_assert(StepperSegmentBufferSize >= 6, "");
    static_assert(LookaheadBufferSizeExp >= 1, "");
    using Loop = typename Context::EventLoop;
    using TimeType = typename Context::Clock::TimeType;
    static const int NumAxes = TypeListLength<AxesList>::value;
    template <typename AxisSpec, typename AccumType>
    using MinTimeTypeHelper = FixedIntersectTypes<typename AxisSpec::TheAxisStepper::TimeFixedType, AccumType>;
    using MinTimeType = TypeListFold<AxesList, FixedIdentity, MinTimeTypeHelper>;
    using SegmentBufferSizeType = BoundedInt<LookaheadBufferSizeExp, false>;
    static const size_t SegmentBufferSize = PowerOfTwo<size_t, LookaheadBufferSizeExp>::value;
    
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_init, init)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_deinit, deinit)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_commandDone_assert, commandDone_assert)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_write_splitbuf, write_splitbuf)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_compute_split_count, compute_split_count)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_check_split_finished, check_split_finished)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_write_segment_buffer_entry, write_segment_buffer_entry)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_compute_segment_buffer_entry_distance, compute_segment_buffer_entry_distance)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_compute_segment_buffer_entry_speed, compute_segment_buffer_entry_speed)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_compute_segment_buffer_entry_accel, compute_segment_buffer_entry_accel)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_write_segment_buffer_entry_accel, write_segment_buffer_entry_accel)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_compute_segment_buffer_cornering_speed, compute_segment_buffer_cornering_speed)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_gen_segment_stepper_commands, gen_segment_stepper_commands)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_have_commit_space, have_commit_space)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_commit_segment_hot, commit_segment_hot)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_commit_segment_finish, commit_segment_finish)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_dispose_new, dispose_new)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_swap_staging_cold, swap_staging_cold)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_swap_staging_hot, swap_staging_hot)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_start_stepping, start_stepping)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_num_empty, num_empty)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_is_underrun, is_underrun)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_stopped_stepping, stopped_stepping)
    
public:
    template <int AxisIndex>
    struct AxisInputCommand {
        using AxisSpec = TypeListGet<AxesList, AxisIndex>;
        using StepFixedType = FixedPoint<AxisSpec::StepBits, false, 0>;
        
        bool dir;
        StepFixedType x;
        double max_v;
        double max_a;
    };
    
    struct InputCommand {
        double rel_max_v;
        IndexElemTuple<AxesList, AxisInputCommand> axes;
    };
    
private:
    template <int AxisIndex>
    struct AxisSplitBuffer {
        using AxisSpec = TypeListGet<AxesList, AxisIndex>;
        using StepFixedType = FixedPoint<AxisSpec::StepBits, false, 0>;
        
        bool dir;
        StepFixedType x;
        double max_v;
        double max_a;
        StepFixedType x_pos;
    };
    
    struct SplitBuffer {
        double base_max_v;
        double split_frac;
        uint32_t split_count;
        uint32_t split_pos;
        IndexElemTuple<AxesList, AxisSplitBuffer> axes;
    };
    
    template <int AxisIndex>
    struct AxisStepperCommand {
        using AxisSpec = TypeListGet<AxesList, AxisIndex>;
        using TheAxisStepper = typename AxisSpec::TheAxisStepper;
        using StepperCommand = typename TheAxisStepper::Command;
        
        StepperCommand scmd;
        AxisStepperCommand *next;
    };
    
    template <int AxisIndex>
    struct AxisSegment {
        using AxisSpec = TypeListGet<AxesList, AxisIndex>;
        using TheAxisStepper = typename AxisSpec::TheAxisStepper;
        using StepperStepFixedType = typename TheAxisStepper::StepFixedType;
        using TheAxisStepperCommand = AxisStepperCommand<AxisIndex>;
        
        bool dir;
        uint8_t num_stepper_entries;
        StepperStepFixedType x;
        double half_accel;
        TheAxisStepperCommand *last_stepper_entry;
    };
    
    struct Segment {
        double distance;
        LinearPlannerSegmentData lp_seg;
        double max_accel_rec;
        double max_v_rec;
        double end_speed_squared;
        IndexElemTuple<AxesList, AxisSegment> axes;
    };
    
public:
    template <int AxisIndex>
    class Axis {
    public:
        using AxisSpec = TypeListGet<AxesList, AxisIndex>;
        using TheAxisStepper = typename AxisSpec::TheAxisStepper;
        using StepFixedType = FixedPoint<AxisSpec::StepBits, false, 0>;
        using TheAxisInputCommand = AxisInputCommand<AxisIndex>;
        
    private:
        friend MotionPlanner;
        
        using StepperStepFixedType = typename TheAxisStepper::StepFixedType;
        using StepperTimeFixedType = typename TheAxisStepper::TimeFixedType;
        using StepperAccelFixedType = typename TheAxisStepper::AccelFixedType;
        using StepperCommand = typename TheAxisStepper::Command;
        using TheAxisSplitBuffer = AxisSplitBuffer<AxisIndex>;
        using TheAxisSegment = AxisSegment<AxisIndex>;
        using TheAxisStepperCommand = AxisStepperCommand<AxisIndex>;
        using StepperCommandCallbackContext = typename TheAxisStepper::CommandCallbackContext;
        static const size_t NumStepperCommands = 3 * (StepperSegmentBufferSize + 2 * SegmentBufferSize);
        using StepperCommandSizeType = typename ChooseInt<BitsInInt<NumStepperCommands>::value, true>::Type;
        
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
            m_first = NULL;
            m_free_first = NULL;
            m_new_first = NULL;
            m_num_committed = 0;
            for (size_t i = 0; i < NumStepperCommands; i++) {
                m_stepper_entries[i].next = m_free_first;
                m_free_first = &m_stepper_entries[i];
            }
        }
        
        void deinit (Context c)
        {
            stepper()->stop(c);
            m_stepper_event.deinit(c);
        }
        
        void commandDone_assert (InputCommand icmd)
        {
            TheAxisInputCommand *axis_icmd = TupleGetElem<AxisIndex>(&icmd.axes);
            AMBRO_ASSERT(FloatIsPosOrPosZero(axis_icmd->max_v))
            AMBRO_ASSERT(FloatIsPosOrPosZero(axis_icmd->max_a))
        }
        
        void write_splitbuf (InputCommand icmd)
        {
            MotionPlanner *o = parent();
            TheAxisInputCommand *axis_icmd = TupleGetElem<AxisIndex>(&icmd.axes);
            TheAxisSplitBuffer *axis_split = TupleGetElem<AxisIndex>(&o->m_split_buffer.axes);
            axis_split->dir = axis_icmd->dir;
            axis_split->x = axis_icmd->x;
            axis_split->max_v = axis_icmd->max_v;
            axis_split->max_a = axis_icmd->max_a;
            axis_split->x_pos = StepFixedType::importBits(0);
        }
        
        double compute_split_count (double accum)
        {
            MotionPlanner *o = parent();
            TheAxisSplitBuffer *axis_split = TupleGetElem<AxisIndex>(&o->m_split_buffer.axes);
            return fmax(accum, axis_split->x.doubleValue() / StepperStepFixedType::maxValue().doubleValue());
        }
        
        bool check_split_finished (bool accum)
        {
            MotionPlanner *o = parent();
            TheAxisSplitBuffer *axis_split = TupleGetElem<AxisIndex>(&o->m_split_buffer.axes);
            return (accum && axis_split->x_pos == axis_split->x);
        }
        
        void write_segment_buffer_entry (Segment *entry)
        {
            MotionPlanner *o = parent();
            TheAxisSplitBuffer *axis_split = TupleGetElem<AxisIndex>(&o->m_split_buffer.axes);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            StepFixedType new_x;
            if (o->m_split_buffer.split_pos == o->m_split_buffer.split_count) {
                new_x = axis_split->x;
            } else {
                new_x = FixedMin(axis_split->x, StepFixedType::importDoubleSaturated(o->m_split_buffer.split_pos * o->m_split_buffer.split_frac * axis_split->x.doubleValue()));
            }
            axis_entry->dir = axis_split->dir;
            axis_entry->x = StepperStepFixedType::importBits(new_x.bitsValue() - axis_split->x_pos.bitsValue());
            axis_split->x_pos = new_x;
        }
        
        double compute_segment_buffer_entry_distance (double accum, Segment *entry)
        {
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            return (accum + (axis_entry->x.doubleValue() * axis_entry->x.doubleValue()) * (AxisSpec::DistanceFactor::value() * AxisSpec::DistanceFactor::value()));
        }
        
        double compute_segment_buffer_entry_speed (double accum, Segment *entry)
        {
            MotionPlanner *o = parent();
            TheAxisSplitBuffer *axis_split = TupleGetElem<AxisIndex>(&o->m_split_buffer.axes);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            return fmin(accum, axis_split->max_v / axis_entry->x.doubleValue());
        }
        
        double compute_segment_buffer_entry_accel (double accum, Segment *entry)
        {
            MotionPlanner *o = parent();
            TheAxisSplitBuffer *axis_split = TupleGetElem<AxisIndex>(&o->m_split_buffer.axes);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            return fmin(accum, axis_split->max_a / axis_entry->x.doubleValue());
        }
        
        double write_segment_buffer_entry_accel (Segment *entry, double rel_max_accel)
        {
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            axis_entry->half_accel = rel_max_accel * axis_entry->x.doubleValue() / 2;
        }
        
        double compute_segment_buffer_cornering_speed (double accum, Segment *entry, Segment *prev_entry)
        {
            MotionPlanner *o = parent();
            TheAxisSplitBuffer *axis_split = TupleGetElem<AxisIndex>(&o->m_split_buffer.axes);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            TheAxisSegment *prev_axis_entry = TupleGetElem<AxisIndex>(&prev_entry->axes);
            double m1 = axis_entry->x.doubleValue() / entry->distance;
            double m2 = prev_axis_entry->x.doubleValue() / prev_entry->distance;
            bool dir_changed = axis_entry->dir != prev_axis_entry->dir;
            double dm = (dir_changed ? (m1 + m2) : fabs(m1 - m2));
            return fmin(accum, axis_split->max_a * (AxisSpec::CorneringDistance::value() * AxisSpec::DistanceFactor::value()) / dm);
        }
        
        template <typename SegmentResult>
        void gen_segment_stepper_commands (Context c, Segment *entry, SegmentResult result, double frac_x0, double frac_x2, MinTimeType t0, MinTimeType t2, MinTimeType t1)
        {
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            StepperStepFixedType x1 = axis_entry->x;
            StepperStepFixedType x0 = FixedMin(x1, StepperStepFixedType::importDoubleSaturated(frac_x0 * axis_entry->x.doubleValue()));
            x1.m_bits.m_int -= x0.bitsValue();
            StepperStepFixedType x2 = FixedMin(x1, StepperStepFixedType::importDoubleSaturated(frac_x2 * axis_entry->x.doubleValue()));
            x1.m_bits.m_int -= x2.bitsValue();
            StepperAccelFixedType a0 = StepperAccelFixedType::importDoubleSaturated(axis_entry->half_accel * (t0.doubleValue() * t0.doubleValue()));
            if (a0.bitsValue() > x0.bitsValue()) {
                a0.m_bits.m_int = x0.bitsValue();
            }
            StepperAccelFixedType a2 = StepperAccelFixedType::importDoubleSaturated(axis_entry->half_accel * (t2.doubleValue() * t2.doubleValue()));
            if (a2.bitsValue() > x2.bitsValue()) {
                a2.m_bits.m_int = x2.bitsValue();
            }
            axis_entry->num_stepper_entries = 0;
            gen_stepper_command(c, axis_entry, x0, t0, a0);
            gen_stepper_command(c, axis_entry, x1, t1, StepperAccelFixedType::importBits(0));
            gen_stepper_command(c, axis_entry, x2, t2, -a2);
        }
        
        void gen_stepper_command (Context c, TheAxisSegment *axis_entry, StepperStepFixedType x, StepperTimeFixedType t, StepperAccelFixedType a)
        {
            MotionPlanner *o = parent();
            
            TheAxisStepperCommand *entry;
            AMBRO_LOCK_T(o->m_lock, c, lock_c, {
                entry = m_free_first;
                m_free_first = m_free_first->next;
            });
            TheAxisStepper::generate_command(axis_entry->dir, x, t, a, &entry->scmd);
            entry->next = NULL;
            if (m_new_first) {
                m_new_last->next = entry;
            } else {
                m_new_first = entry;
            }
            m_new_last = entry;
            axis_entry->last_stepper_entry = entry;
            axis_entry->num_stepper_entries++;
        }
        
        bool have_commit_space (bool accum)
        {
            return (accum && m_num_committed <= 3 * (StepperSegmentBufferSize - 1));
        }
        
        void commit_segment_hot (Segment *entry)
        {
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            AMBRO_ASSERT(m_num_committed <= 3 * StepperSegmentBufferSize - axis_entry->num_stepper_entries)
            m_num_committed += axis_entry->num_stepper_entries;
        }
        
        void commit_segment_finish (Segment *entry)
        {
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            m_last_committed = axis_entry->last_stepper_entry;
        }
        
        void dispose_new (Context c)
        {
            MotionPlanner *o = parent();
            if (m_new_first) {
                AMBRO_LOCK_T(o->m_lock, c, lock_c, {
                    m_new_last->next = m_free_first;
                    m_free_first = m_new_first;
                });
                m_new_first = NULL;
            }
        }
        
        void swap_staging_cold ()
        {
            AMBRO_ASSERT(m_new_first)
            
            if (m_num_committed == 0) {
                if (m_first) {
                    m_last->next = m_free_first;
                    m_free_first = m_first;
                }
                m_first = m_new_first;
            } else {
                if (m_last_committed->next) {
                    m_last->next = m_free_first;
                    m_free_first = m_last_committed->next;
                }
                m_last_committed->next = m_new_first;
            }
            m_last = m_new_last;
            m_new_first = NULL;
        }
        
        void swap_staging_hot ()
        {
            AMBRO_ASSERT(m_new_first)
            AMBRO_ASSERT(m_num_committed > 0)
            
            TheAxisStepperCommand *old_first = m_last_committed->next;
            TheAxisStepperCommand *old_last = m_last;
            m_last_committed->next = m_new_first;
            m_last = m_new_last;
            m_new_first = old_first;
            m_new_last = old_last;
        }
        
        void start_stepping (Context c, TimeType start_time)
        {
            stepper()->template start<TheAxisStepperConsumer<AxisIndex>>(c, start_time, &m_first->scmd);
        }
        
        uint8_t num_empty (uint8_t accum)
        {
            return (accum + (m_first == NULL));
        }
        
        bool is_underrun (bool accum)
        {
            return (accum || (m_num_committed <= 0));
        }
        
        void stopped_stepping (Context c)
        {
            m_num_committed = 0;
            m_stepper_event.unset(c);
        }
        
        void stepper_event_handler (Context c)
        {
            MotionPlanner *o = parent();
            o->stepper_event_handler(c);
        }
        
        StepperCommand * stepper_command_callback (StepperCommandCallbackContext c)
        {
            MotionPlanner *o = parent();
            AMBRO_ASSERT(m_first)
            AMBRO_ASSERT(o->m_stepping)
            
            TheAxisStepperCommand *old = m_first;
            m_first = m_first->next;
            old->next = m_free_first;
            m_free_first = old;
            m_num_committed--;
            m_stepper_event.appendNowIfNotAlready(c);
            return (m_first ? &m_first->scmd : NULL);
        }
        
        typename Loop::QueuedEvent m_stepper_event;
        TheAxisStepperCommand *m_first;
        TheAxisStepperCommand *m_last_committed;
        TheAxisStepperCommand *m_last;
        TheAxisStepperCommand *m_new_first;
        TheAxisStepperCommand *m_new_last;
        TheAxisStepperCommand *m_free_first;
        StepperCommandSizeType m_num_committed;
        TheAxisStepperCommand m_stepper_entries[NumStepperCommands];
    };
    
private:
    using AxesTuple = IndexElemTuple<AxesList, Axis>;
    
public:
    void init (Context c)
    {
        m_lock.init(c);
        m_pull_finished_event.init(c, AMBRO_OFFSET_CALLBACK_T(&MotionPlanner::m_pull_finished_event, &MotionPlanner::pull_finished_event_handler));
        m_segments_start = SegmentBufferSizeType::import(0);
        m_segments_staging_end = SegmentBufferSizeType::import(0);
        m_segments_end = SegmentBufferSizeType::import(0);
        m_segments_start_v_squared = 0.0;
        m_have_split_buffer = false;
        m_stepping = false;
        m_underrun = false;
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
        AMBRO_ASSERT(!m_have_split_buffer)
        AMBRO_ASSERT(FloatIsPosOrPosZero(icmd.rel_max_v))
        TupleForEachForward(&m_axes, Foreach_commandDone_assert(), icmd);
        
        TupleForEachForward(&m_axes, Foreach_write_splitbuf(), icmd);
        double split_count_comp = TupleForEachForwardAccRes(&m_axes, 0.0, Foreach_compute_split_count());
        double split_count = ceil(split_count_comp + 0.1);
        m_split_buffer.base_max_v = icmd.rel_max_v * split_count;
        m_split_buffer.split_frac = 1.0 / split_count;
        m_split_buffer.split_count = split_count;
        m_split_buffer.split_pos = 1;
        
        m_pull_finished_event.unset(c);
        m_waiting = false;
        m_have_split_buffer = true;
#ifdef AMBROLIB_ASSERTIONS
        m_pulling = false;
#endif
        
        work(c);
    }
    
    void waitFinished (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_pulling)
        AMBRO_ASSERT(!m_waiting)
        AMBRO_ASSERT(!m_have_split_buffer)
        
        m_waiting = true;
        if (!m_stepping) {
            if (m_segments_start == m_segments_end) {
                m_pull_finished_event.prependNowNotAlready(c);
            } else {
                if (m_segments_staging_end != m_segments_end) {
                    plan(c);
                }
                start_stepping(c);
            }
        }
    }
    
    template <int AxisIndex>
    using TheAxisStepperConsumer = AxisStepperConsumer<
        AxisPosition<AxisIndex>,
        AMBRO_WMETHOD_T(&Axis<AxisIndex>::stepper_command_callback)
    >;
    
private:
    void work (Context c)
    {
        AMBRO_ASSERT(m_have_split_buffer)
        AMBRO_ASSERT(!m_pulling)
        
        while (1) {
            while (1) {
                if (TupleForEachForwardAccRes(&m_axes, true, Foreach_check_split_finished())) {
                    m_have_split_buffer = false;
                    break;
                }
                if (BoundedModuloSubtract(m_segments_start, m_segments_end).value() == 1) {
                    break;
                }
                Segment *entry = &m_segments[m_segments_end.value()];
                TupleForEachForward(&m_axes, Foreach_write_segment_buffer_entry(), entry);
                double distance_squared = TupleForEachForwardAccRes(&m_axes, 0.0, Foreach_compute_segment_buffer_entry_distance(), entry);
                double rel_max_speed = TupleForEachForwardAccRes(&m_axes, m_split_buffer.base_max_v, Foreach_compute_segment_buffer_entry_speed(), entry);
                double rel_max_accel = TupleForEachForwardAccRes(&m_axes, INFINITY, Foreach_compute_segment_buffer_entry_accel(), entry);
                entry->distance = sqrt(distance_squared);
                entry->lp_seg.max_v = (rel_max_speed * rel_max_speed) * distance_squared;
                entry->lp_seg.max_start_v = entry->lp_seg.max_v;
                double max_accel = rel_max_accel * entry->distance;
                entry->lp_seg.a_x = 2 * rel_max_accel * distance_squared;
                entry->lp_seg.a_x_rec = 1.0 / entry->lp_seg.a_x;
                entry->lp_seg.two_max_v_minus_a_x = 2 * entry->lp_seg.max_v - entry->lp_seg.a_x;
                entry->max_accel_rec = 1.0 / max_accel;
                entry->max_v_rec = 1.0 / (rel_max_speed * entry->distance);
                TupleForEachForward(&m_axes, Foreach_write_segment_buffer_entry_accel(), entry, rel_max_accel);
                if (m_split_buffer.split_pos == 1 && m_segments_start != m_segments_end) {
                    Segment *prev_entry = &m_segments[BoundedModuloDec(m_segments_end).value()];
                    entry->lp_seg.max_start_v = TupleForEachForwardAccRes(&m_axes, entry->lp_seg.max_start_v, Foreach_compute_segment_buffer_cornering_speed(), entry, prev_entry);
                }
                m_split_buffer.split_pos++;
                m_segments_end = BoundedModuloInc(m_segments_end);
            }
            
            if (!m_underrun && m_segments_staging_end != m_segments_end) {
                plan(c);
            }
            
            if (m_underrun || !m_have_split_buffer) {
                if (!m_have_split_buffer) {
                    m_pull_finished_event.prependNowNotAlready(c);
                }
                return;
            }
            
            Segment *entry = &m_segments[m_segments_start.value()];
            if (!m_stepping) {
                if (!TupleForEachForwardAccRes(&m_axes, true, Foreach_have_commit_space())) {
                    start_stepping(c);
                    return;
                }
                TupleForEachForward(&m_axes, Foreach_commit_segment_hot(), entry);
            } else {
                bool cleared = false;
                AMBRO_LOCK_T(m_lock, c, lock_c, {
                    m_underrun = is_underrun();
                    if (!m_underrun && TupleForEachForwardAccRes(&m_axes, true, Foreach_have_commit_space())) {
                        TupleForEachForward(&m_axes, Foreach_commit_segment_hot(), entry);
                        cleared = true;
                    }
                });
                if (!cleared) {
                    return;
                }
            }
            TupleForEachForward(&m_axes, Foreach_commit_segment_finish(), entry);
            m_segments_start = BoundedModuloInc(m_segments_start);
            m_segments_start_v_squared = entry->end_speed_squared;
        }
    }
    
    LinearPlannerSegmentData * lp_get (Context c, SegmentBufferSizeType *i, double *start_v)
    {
        if (*i == m_segments_start) {
            *start_v = m_segments_start_v_squared;
            return NULL;
        }
        *i = BoundedModuloDec(*i);
        Segment *entry = &m_segments[(*i).value()];
        return &entry->lp_seg;
    }
    
    void lp_result (Context c, SegmentBufferSizeType *i, LinearPlannerSegmentResult *result)
    {
        Segment *entry = &m_segments[(*i).value()];
        double v_start = sqrt(result->start_v);
        double v_end = sqrt(result->end_v);
        double v_const = sqrt(result->const_v);
        MinTimeType t0 = MinTimeType::importDoubleSaturated((v_const - v_start) * entry->max_accel_rec);
        MinTimeType t2 = MinTimeType::importDoubleSaturated((v_const - v_end) * entry->max_accel_rec);
        MinTimeType t1 = MinTimeType::importDoubleSaturated(((1.0 - result->const_start - result->const_end) * entry->distance) * entry->max_v_rec);
        TupleForEachForward(&m_axes, Foreach_gen_segment_stepper_commands(), c, entry, result, result->const_start, result->const_end, t0, t2, t1);
        entry->end_speed_squared = result->end_v;
        *i = BoundedModuloInc(*i);
    }
    
    using TheLinearPlanner = LinearPlanner<
        AMBRO_WMETHOD_T(&MotionPlanner::lp_get),
        AMBRO_WMETHOD_T(&MotionPlanner::lp_result)
    >;
    
    void plan (Context c)
    {
        AMBRO_ASSERT(m_segments_staging_end != m_segments_end)
        AMBRO_ASSERT(m_stepping || !m_underrun)
        
        SegmentBufferSizeType i = BoundedModuloDec(m_segments_end);
        LinearPlannerSegmentData *last_segment = &m_segments[i.value()].lp_seg;
        TheLinearPlanner::plan(last_segment, 0.0, this, c, &i);
        
        if (!m_stepping) {
            TupleForEachForward(&m_axes, Foreach_swap_staging_cold());
        } else {
            AMBRO_LOCK_T(m_lock, c, lock_c, {
                m_underrun = is_underrun();
                if (!m_underrun) {
                    TupleForEachForward(&m_axes, Foreach_swap_staging_hot());
                }
            });
            TupleForEachForward(&m_axes, Foreach_dispose_new(), c);
        }
        if (!m_underrun) {
            m_segments_staging_end = m_segments_end;
        }
    }
    
    void start_stepping (Context c)
    {
        AMBRO_ASSERT(!m_stepping)
        AMBRO_ASSERT(num_empty() == 0)
        AMBRO_ASSERT(m_segments_staging_end == m_segments_end)
        
        m_stepping = true;
        TimeType start_time = c.clock()->getTime(c);
        TupleForEachForward(&m_axes, Foreach_start_stepping(), c, start_time);
    }
    
    void pull_finished_event_handler (Context c)
    {
        this->debugAccess(c);
        
        if (m_waiting) {
            AMBRO_ASSERT(m_pulling)
            AMBRO_ASSERT(!m_stepping)
            AMBRO_ASSERT(m_segments_start == m_segments_end)
            AMBRO_ASSERT(num_empty() == NumAxes)
            
            m_waiting = false;
            return FinishedHandler::call(this, c);
        } else {
            AMBRO_ASSERT(!m_pulling)
            AMBRO_ASSERT(!m_have_split_buffer)
            
#ifdef AMBROLIB_ASSERTIONS
            m_pulling = true;
#endif
            return PullHandler::call(this, c);
        }
    }
    
    uint8_t num_empty ()
    {
        return TupleForEachForwardAccRes(&m_axes, 0, Foreach_num_empty());
    }
    
    uint8_t is_underrun ()
    {
        return TupleForEachForwardAccRes(&m_axes, false, Foreach_is_underrun());
    }
    
    void stepper_event_handler (Context c)
    {
        AMBRO_ASSERT(m_stepping)
        
        uint8_t the_num_empty;
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            m_underrun = is_underrun();
            the_num_empty = num_empty();
        });
        
        if (the_num_empty == NumAxes) {
            m_stepping = false;
            m_underrun = false;
            m_segments_start = m_segments_staging_end;
            TupleForEachForward(&m_axes, Foreach_stopped_stepping(), c);
            
            if (m_waiting) {
                if (m_segments_start == m_segments_end) {
                    m_pull_finished_event.prependNowNotAlready(c);
                } else {
                    if (m_segments_staging_end != m_segments_end) {
                        plan(c);
                    }
                    start_stepping(c);
                }
            }
        }
        
        if (m_have_split_buffer) {
            work(c);
        }
    }

    typename Context::Lock m_lock;
    typename Loop::QueuedEvent m_pull_finished_event;
    SegmentBufferSizeType m_segments_start;
    SegmentBufferSizeType m_segments_staging_end;
    SegmentBufferSizeType m_segments_end;
    double m_segments_start_v_squared;
    bool m_have_split_buffer;
    bool m_stepping;
    bool m_underrun;
    bool m_waiting;
#ifdef AMBROLIB_ASSERTIONS
    bool m_pulling;
#endif
    SplitBuffer m_split_buffer;
    Segment m_segments[SegmentBufferSize];
    AxesTuple m_axes;
    
    template <int AxisIndex> struct AxisPosition : public TuplePosition<Position, AxesTuple, &MotionPlanner::m_axes, AxisIndex> {};
};

#include <aprinter/EndNamespace.h>

#endif
