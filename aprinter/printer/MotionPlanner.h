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
#include <aprinter/meta/Position.h>
#include <aprinter/meta/TuplePosition.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/Union.h>
#include <aprinter/meta/UnionGet.h>
#include <aprinter/meta/IndexElemUnion.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Likely.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/printer/LinearPlanner.h>

#include <aprinter/BeginNamespace.h>

template <
    typename TTheAxisStepper,
    typename TGetAxisStepper,
    int TStepBits,
    typename TDistanceFactor,
    typename TCorneringDistance,
    typename TPrestepCallback
>
struct MotionPlannerAxisSpec {
    using TheAxisStepper = TTheAxisStepper;
    using GetAxisStepper = TGetAxisStepper;
    static const int StepBits = TStepBits;
    using DistanceFactor = TDistanceFactor;
    using CorneringDistance = TCorneringDistance;
    using PrestepCallback = TPrestepCallback;
};

template <
    typename TPayload,
    typename TCallback,
    int TBufferSize,
    template <typename, typename, typename> class TTimer
>
struct MotionPlannerChannelSpec {
    using Payload = TPayload;
    using Callback = TCallback;
    static const int BufferSize = TBufferSize;
    template<typename X, typename Y, typename Z> using Timer = TTimer<X, Y, Z>;
};

template <
    typename Position, typename Context, typename AxesList, int StepperSegmentBufferSize, int LookaheadBufferSize,
    int LookaheadCommitCount,
    typename PullHandler, typename FinishedHandler, typename AbortedHandler, typename UnderrunCallback,
    typename ChannelsList = EmptyTypeList
>
class MotionPlanner
: private DebugObject<Context, void>
{
private:
    template <int AxisIndex> struct AxisPosition;
    template <int ChannelIndex> struct ChannelPosition;
    
    static_assert(StepperSegmentBufferSize - LookaheadCommitCount >= 6, "");
    static_assert(LookaheadBufferSize >= 2, "");
    static_assert(LookaheadCommitCount >= 1, "");
    static_assert(LookaheadCommitCount < LookaheadBufferSize, "");
    using Loop = typename Context::EventLoop;
    using TimeType = typename Context::Clock::TimeType;
    static const int NumAxes = TypeListLength<AxesList>::value;
    static const int NumChannels = TypeListLength<ChannelsList>::value;
    template <typename AxisSpec, typename AccumType>
    using MinTimeTypeHelper = FixedIntersectTypes<typename AxisSpec::TheAxisStepper::TimeFixedType, AccumType>;
    using MinTimeType = TypeListFold<AxesList, FixedIdentity, MinTimeTypeHelper>;
    using SegmentBufferSizeType = typename ChooseInt<BitsInInt<2 * LookaheadBufferSize>::value, false>::Type; // twice for segments_add()
    static const size_t StepperCommitBufferSize = 3 * StepperSegmentBufferSize;
    static const size_t StepperBackupBufferSize = 3 * (LookaheadBufferSize - LookaheadCommitCount);
    using StepperCommitBufferSizeType = typename ChooseInt<BitsInInt<StepperCommitBufferSize>::value, false>::Type;
    using StepperBackupBufferSizeType = typename ChooseInt<BitsInInt<2 * StepperBackupBufferSize>::value, false>::Type;
    using StepperFastEvent = typename Context::EventLoop::template FastEventSpec<MotionPlanner>;
    static const int TypeBits = BitsInInt<NumChannels>::value;
    using AxisMaskType = typename ChooseInt<NumAxes + TypeBits, false>::Type;
    static const AxisMaskType TypeMask = ((AxisMaskType)1 << TypeBits) - 1;
    
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_init, init)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_deinit, deinit)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_abort, abort)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_commandDone_assert, commandDone_assert)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_write_splitbuf, write_splitbuf)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_splitbuf_fits, splitbuf_fits)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_compute_split_count, compute_split_count)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_check_icmd_zero, check_icmd_zero)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_write_segment_buffer_entry, write_segment_buffer_entry)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_compute_segment_buffer_entry_distance, compute_segment_buffer_entry_distance)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_compute_segment_buffer_entry_speed, compute_segment_buffer_entry_speed)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_compute_segment_buffer_entry_accel, compute_segment_buffer_entry_accel)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_write_segment_buffer_entry_extra, write_segment_buffer_entry_extra)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_compute_segment_buffer_cornering_speed, compute_segment_buffer_cornering_speed)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_have_commit_space, have_commit_space)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_start_commands, start_commands)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_gen_segment_stepper_commands, gen_segment_stepper_commands)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_do_commit, do_commit)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_do_commit_cold, do_commit_cold)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_do_commit_hot, do_commit_hot)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_start_stepping, start_stepping)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_is_busy, is_busy)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_reset_aborted, reset_aborted)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_stopped_stepping, stopped_stepping)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_write_segment, write_segment)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_gen_command, gen_command)
    
public:
    template <int ChannelIndex>
    using ChannelPayload = typename TypeListGet<ChannelsList, ChannelIndex>::Payload;
    
    using ChannelPayloadUnion = IndexElemUnion<ChannelsList, ChannelPayload>;
    
    template <int AxisIndex>
    struct AxisSplitBuffer {
        using AxisSpec = TypeListGet<AxesList, AxisIndex>;
        using StepFixedType = FixedPoint<AxisSpec::StepBits, false, 0>;
        
        bool dir;
        StepFixedType x;
        double max_v_rec;
        double max_a_rec;
        StepFixedType x_pos; // internal
    };
    
    struct SplitBuffer {
        uint8_t type; // internal
        union {
            struct {
                double rel_max_v_rec;
                double split_frac; // internal
                uint32_t split_count; // internal
                uint32_t split_pos; // internal
                IndexElemTuple<AxesList, AxisSplitBuffer> axes;
            };
            ChannelPayloadUnion channel_payload;
        };
    };
    
private:
    template <int ChannelIndex>
    struct ChannelCommand {
        using ChannelSpec = TypeListGet<ChannelsList, ChannelIndex>;
        using Payload = typename ChannelSpec::Payload;
        
        Payload payload;
        TimeType time;
    };
    
    template <int AxisIndex>
    struct AxisSegment {
        using AxisSpec = TypeListGet<AxesList, AxisIndex>;
        using TheAxisStepper = typename AxisSpec::TheAxisStepper;
        using StepperStepFixedType = typename TheAxisStepper::StepFixedType;
        
        StepperStepFixedType x;
    };
    
    template <int ChannelIndex>
    struct ChannelSegment {
        using ChannelSpec = TypeListGet<ChannelsList, ChannelIndex>;
        using Payload = typename ChannelSpec::Payload;
        using TheChannelCommand = ChannelCommand<ChannelIndex>;
        
        Payload payload;
    };
    
    struct Segment {
        AxisMaskType dir_and_type;
        LinearPlannerSegmentData lp_seg;
        union {
            struct {
                double max_accel_rec;
                double rel_max_speed_rec;
                double half_accel[NumAxes];
                IndexElemTuple<AxesList, AxisSegment> axes;
            };
            IndexElemUnion<ChannelsList, ChannelSegment> channels;
        };
    };
    
    enum {STATE_BUFFERING, STATE_STEPPING, STATE_ABORTED};
    
public:
    template <int AxisIndex>
    class Axis {
    public:
        using AxisSpec = TypeListGet<AxesList, AxisIndex>;
        using TheAxisStepper = typename AxisSpec::TheAxisStepper;
        using StepFixedType = FixedPoint<AxisSpec::StepBits, false, 0>;
        using StepperCommandCallbackContext = typename TheAxisStepper::CommandCallbackContext;
        
    public: // private, workaround gcc bug
        friend MotionPlanner;
        
        using StepperStepFixedType = typename TheAxisStepper::StepFixedType;
        using StepperTimeFixedType = typename TheAxisStepper::TimeFixedType;
        using StepperAccelFixedType = typename TheAxisStepper::AccelFixedType;
        using StepperCommand = typename TheAxisStepper::Command;
        using TheAxisSplitBuffer = AxisSplitBuffer<AxisIndex>;
        using TheAxisSegment = AxisSegment<AxisIndex>;
        static const AxisMaskType TheAxisMask = (AxisMaskType)1 << (AxisIndex + TypeBits);
        
        static Axis * self (Context c)
        {
            return PositionTraverse<typename Context::TheRootPosition, AxisPosition<AxisIndex>>(c.root());
        }
        
        static TheAxisStepper * stepper (Context c)
        {
            return AxisSpec::GetAxisStepper::call(c);
        }
        
        static void init (Context c, bool prestep_callback_enabled)
        {
            Axis *o = self(c);
            o->m_commit_start = 0;
            o->m_commit_end = 0;
            o->m_backup_start = 0;
            o->m_backup_end = 0;
            o->m_busy = false;
            stepper(c)->setPrestepCallbackEnabled(c, prestep_callback_enabled);
        }
        
        static void deinit (Context c)
        {
            stepper(c)->stop(c);
        }
        
        static void abort (Context c)
        {
            stepper(c)->stop(c);
        }
        
        static void commandDone_assert (Context c)
        {
            TheAxisSplitBuffer *axis_split = get_axis_split(c);
            AMBRO_ASSERT(FloatIsPosOrPosZero(axis_split->max_v_rec))
            AMBRO_ASSERT(FloatIsPosOrPosZero(axis_split->max_a_rec))
        }
        
        static void write_splitbuf (Context c)
        {
            TheAxisSplitBuffer *axis_split = get_axis_split(c);
            axis_split->x_pos = StepFixedType::importBits(0);
        }
        
        static bool splitbuf_fits (bool accum, Context c)
        {
            TheAxisSplitBuffer *axis_split = get_axis_split(c);
            return (accum && axis_split->x <= StepperStepFixedType::maxValue());
        }
        
        static double compute_split_count (double accum, Context c)
        {
            TheAxisSplitBuffer *axis_split = get_axis_split(c);
            return fmax(accum, axis_split->x.doubleValue() * (1.0001 / StepperStepFixedType::maxValue().doubleValue()));
        }
        
        static bool check_icmd_zero (bool accum, Context c)
        {
            TheAxisSplitBuffer *axis_split = get_axis_split(c);
            return (accum && axis_split->x.bitsValue() == 0);
        }
        
        static void write_segment_buffer_entry (Context c, Segment *entry)
        {
            MotionPlanner *m = MotionPlanner::self(c);
            TheAxisSplitBuffer *axis_split = get_axis_split(c);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            StepFixedType new_x;
            if (m->m_split_buffer.split_pos == m->m_split_buffer.split_count) {
                new_x = axis_split->x;
            } else {
                new_x = FixedMin(axis_split->x, StepFixedType::importDoubleSaturatedRound(m->m_split_buffer.split_pos * m->m_split_buffer.split_frac * axis_split->x.doubleValue()));
            }
            if (axis_split->dir) {
                entry->dir_and_type |= TheAxisMask;
            }
            axis_entry->x = StepperStepFixedType::importBits(new_x.bitsValue() - axis_split->x_pos.bitsValue());
            axis_split->x_pos = new_x;
        }
        
        static double compute_segment_buffer_entry_distance (double accum, Segment *entry)
        {
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            return (accum + (axis_entry->x.doubleValue() * axis_entry->x.doubleValue()) * (AxisSpec::DistanceFactor::value() * AxisSpec::DistanceFactor::value()));
        }
        
        static double compute_segment_buffer_entry_speed (double accum, Context c, Segment *entry)
        {
            TheAxisSplitBuffer *axis_split = get_axis_split(c);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            return fmax(accum, axis_entry->x.doubleValue() * axis_split->max_v_rec);
        }
        
        static double compute_segment_buffer_entry_accel (double accum, Context c, Segment *entry)
        {
            TheAxisSplitBuffer *axis_split = get_axis_split(c);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            return fmax(accum, axis_entry->x.doubleValue() * axis_split->max_a_rec);
        }
        
        static double write_segment_buffer_entry_extra (Segment *entry, double rel_max_accel)
        {
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            entry->half_accel[AxisIndex] = 0.5 * rel_max_accel * axis_entry->x.doubleValue();
        }
        
        static double compute_segment_buffer_cornering_speed (double accum, Context c, Segment *entry, double entry_distance_rec, Segment *prev_entry)
        {
            MotionPlanner *m = MotionPlanner::self(c);
            TheAxisSplitBuffer *axis_split = get_axis_split(c);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            TheAxisSegment *prev_axis_entry = TupleGetElem<AxisIndex>(&prev_entry->axes);
            double m1 = axis_entry->x.doubleValue() * entry_distance_rec;
            double m2 = prev_axis_entry->x.doubleValue() * m->m_last_distance_rec;
            bool dir_changed = (entry->dir_and_type ^ prev_entry->dir_and_type) & TheAxisMask;
            double dm = (dir_changed ? (m1 + m2) : fabs(m1 - m2));
            return fmin(accum, (AxisSpec::CorneringDistance::value() * AxisSpec::DistanceFactor::value()) / (dm * axis_split->max_a_rec));
        }
        
        static bool have_commit_space (bool accum, Context c)
        {
            Axis *o = self(c);
            return (accum && commit_avail(o->m_commit_start, o->m_commit_end) > 3 * LookaheadCommitCount);
        }
        
        static void start_commands (Context c)
        {
            Axis *o = self(c);
            MotionPlanner *m = MotionPlanner::self(c);
            o->m_new_commit_end = o->m_commit_end;
            o->m_new_backup_end = m->m_current_backup ? 0 : StepperBackupBufferSize;
        }
        
        static void gen_segment_stepper_commands (Context c, Segment *entry, double frac_x0, double frac_x2, MinTimeType t0, MinTimeType t2, MinTimeType t1, double t0_squared, double t2_squared)
        {
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            
            StepperStepFixedType x1 = axis_entry->x;
            StepperStepFixedType x0 = FixedMin(x1, StepperStepFixedType::importDoubleSaturatedRound(frac_x0 * axis_entry->x.doubleValue()));
            x1.m_bits.m_int -= x0.bitsValue();
            StepperStepFixedType x2 = FixedMin(x1, StepperStepFixedType::importDoubleSaturatedRound(frac_x2 * axis_entry->x.doubleValue()));
            x1.m_bits.m_int -= x2.bitsValue();
            
            if (x0.bitsValue() == 0) {
                t1.m_bits.m_int += t0.bitsValue();
            }
            if (x2.bitsValue() == 0) {
                t1.m_bits.m_int += t2.bitsValue();
            }
            
            bool gen1 = true;
            if (x1.bitsValue() == 0 && (x0.bitsValue() != 0 || x2.bitsValue() != 0)) {
                gen1 = false;
                if (x0.bitsValue() != 0) {
                    t0.m_bits.m_int += t1.bitsValue();
                } else {
                    t2.m_bits.m_int += t1.bitsValue();
                }
            }
            
            bool dir = entry->dir_and_type & TheAxisMask;
            if (x0.bitsValue() != 0) {
                gen_stepper_command(c, dir, x0, t0, FixedMin(x0, StepperAccelFixedType::importDoubleSaturatedRound(entry->half_accel[AxisIndex] * t0_squared)));
            }
            if (gen1) {
                gen_stepper_command(c, dir, x1, t1, StepperAccelFixedType::importBits(0));
            }
            if (x2.bitsValue() != 0) {
                gen_stepper_command(c, dir, x2, t2, -FixedMin(x2, StepperAccelFixedType::importDoubleSaturatedRound(entry->half_accel[AxisIndex] * t2_squared)));
            }
        }
        
        static void gen_stepper_command (Context c, bool dir, StepperStepFixedType x, StepperTimeFixedType t, StepperAccelFixedType a)
        {
            Axis *o = self(c);
            MotionPlanner *m = MotionPlanner::self(c);
            
            StepperCommand *cmd;
            if (!m->m_new_to_backup) {
                cmd = &o->m_commit_buffer[o->m_new_commit_end];
                o->m_new_commit_end = commit_inc(o->m_new_commit_end);
            } else {
                cmd = &o->m_backup_buffer[o->m_new_backup_end];
                o->m_new_backup_end++;
            }
            TheAxisStepper::generate_command(dir, x, t, a, cmd);
        }
        
        static void do_commit (Context c)
        {
            Axis *o = self(c);
            MotionPlanner *m = MotionPlanner::self(c);
            o->m_commit_end = o->m_new_commit_end;
            o->m_backup_start = m->m_current_backup ? 0 : StepperBackupBufferSize;
            o->m_backup_end = o->m_new_backup_end;
        }
        
        static void start_stepping (Context c, TimeType start_time)
        {
            Axis *o = self(c);
            MotionPlanner *m = MotionPlanner::self(c);
            AMBRO_ASSERT(!o->m_busy)
            
            if (o->m_commit_start != o->m_commit_end) {
                if (AxisIndex == 0) {
                    // Only first axis sets it so it doesn't get re-set after a fast underflow.
                    // Can't happen anyway due to the start time offset.
                    m->m_syncing = true;
                }
                o->m_busy = true;
                StepperCommand *cmd = &o->m_commit_buffer[o->m_commit_start];
                o->m_commit_start = commit_inc(o->m_commit_start);
                stepper(c)->template start<TheAxisStepperConsumer<AxisIndex>>(c, start_time, cmd);
            }
        }
        
        static bool is_busy (bool accum, Context c)
        {
            Axis *o = self(c);
            return (accum || o->m_busy);
        }
        
        static void reset_aborted (Context c)
        {
            Axis *o = self(c);
            o->m_commit_start = 0;
            o->m_commit_end = 0;
            o->m_busy = false;
        }
        
        static void stopped_stepping (Context c)
        {
            Axis *o = self(c);
            o->m_backup_start = 0;
            o->m_backup_end = 0;
        }
        
        static bool stepper_command_callback (StepperCommandCallbackContext c, StepperCommand **cmd)
        {
            Axis *o = self(c);
            MotionPlanner *m = MotionPlanner::self(c);
            AMBRO_ASSERT(o->m_busy)
            AMBRO_ASSERT(m->m_state == STATE_STEPPING)
            
            c.eventLoop()->template triggerFastEvent<StepperFastEvent>(c);
            if (AMBRO_UNLIKELY(o->m_commit_start != o->m_commit_end)) {
                *cmd = &o->m_commit_buffer[o->m_commit_start];
                o->m_commit_start = commit_inc(o->m_commit_start);
            } else {
                m->m_syncing = false;
                if (o->m_backup_start == o->m_backup_end) {
                    o->m_busy = false;
                    return false;
                }
                *cmd = &o->m_backup_buffer[o->m_backup_start];
                o->m_backup_start++;
            }
            return true;
        }
        
        static bool stepper_prestep_callback (StepperCommandCallbackContext c)
        {
            MotionPlanner *m = MotionPlanner::self(c);
            bool res = AxisSpec::PrestepCallback::call(c);
            if (AMBRO_UNLIKELY(res)) {
                c.eventLoop()->template triggerFastEvent<StepperFastEvent>(c);
                m->m_aborted = true;
            }
            return res;
        }
        
        template <typename StepsType>
        static void add_command_steps (Context c, StepsType *steps, StepperCommand *cmd)
        {
            bool dir;
            StepperStepFixedType cmd_steps = stepper(c)->getPendingCmdSteps(c, cmd, &dir);
            add_steps(steps, cmd_steps, dir);
        }
        
        template <typename StepsType, typename TheseStepsFixedType>
        static void add_steps (StepsType *steps, TheseStepsFixedType these_steps, bool dir)
        {
            if (dir) {
                *steps += (StepsType)these_steps.bitsValue();
            } else {
                *steps -= (StepsType)these_steps.bitsValue();
            }
        }
        
        template <typename StepsType>
        static StepsType axis_count_aborted_rem_steps (Context c)
        {
            Axis *o = self(c);
            MotionPlanner *m = MotionPlanner::self(c);
            
            StepsType steps = 0;
            if (o->m_busy) {
                bool dir;
                StepperStepFixedType cmd_steps = stepper(c)->getAbortedCmdSteps(c, &dir);
                add_steps(&steps, cmd_steps, dir);
            }
            for (StepperCommitBufferSizeType i = o->m_commit_start; i != o->m_commit_end; i = commit_inc(i)) {
                add_command_steps(c, &steps, &o->m_commit_buffer[i]);
            }
            for (StepperBackupBufferSizeType i = o->m_backup_start; i < o->m_backup_end; i++) {
                add_command_steps(c, &steps, &o->m_backup_buffer[i]);
            }
            for (SegmentBufferSizeType i = m->m_segments_staging_length; i < m->m_segments_length; i++) {
                Segment *seg = &m->m_segments[segments_add(m->m_segments_start, i)];
                TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&seg->axes);
                add_steps(&steps, axis_entry->x, (seg->dir_and_type & TheAxisMask));
            }
            if (m->m_split_buffer.type == 0) {
                TheAxisSplitBuffer *axis_split = get_axis_split(c);
                StepFixedType x = StepFixedType::importBits(axis_split->x.bitsValue() - axis_split->x_pos.bitsValue());
                add_steps(&steps, x, axis_split->dir);
            }
            return steps;
        }
        
        static StepperCommitBufferSizeType commit_inc (StepperCommitBufferSizeType a)
        {
            a++;
            if (AMBRO_LIKELY(a == StepperCommitBufferSize)) {
                a = 0;
            }
            return a;
        }
        
        static StepperCommitBufferSizeType commit_avail (StepperCommitBufferSizeType start, StepperCommitBufferSizeType end)
        {
            return (end >= start) ? ((StepperCommitBufferSize - 1) - (end - start)) : ((start - end) - 1);
        }
        
        static TheAxisSplitBuffer * get_axis_split (Context c)
        {
            MotionPlanner *m = MotionPlanner::self(c);
            return TupleGetElem<AxisIndex>(&m->m_split_buffer.axes);
        }
        
        StepperCommitBufferSizeType m_commit_start;
        StepperCommitBufferSizeType m_commit_end;
        StepperBackupBufferSizeType m_backup_start;
        StepperBackupBufferSizeType m_backup_end;
        StepperCommitBufferSizeType m_new_commit_end;
        StepperBackupBufferSizeType m_new_backup_end;
        bool m_busy;
        StepperCommand m_commit_buffer[StepperCommitBufferSize];
        StepperCommand m_backup_buffer[2 * StepperBackupBufferSize];
    };
    
    template <int ChannelIndex>
    class Channel {
    public: // private, workaround gcc bug
        friend MotionPlanner;
        struct TimerHandler;
        struct TimerPosition;
        
    public:
        using ChannelSpec = TypeListGet<ChannelsList, ChannelIndex>;
        using Payload = typename ChannelSpec::Payload;
        using TheChannelCommand = ChannelCommand<ChannelIndex>;
        using TheChannelSegment = ChannelSegment<ChannelIndex>;
        using TheTimer = typename ChannelSpec::template Timer<TimerPosition, Context, TimerHandler>;
        using CallbackContext = typename TheTimer::HandlerContext;
        
    public: // private, workaround gcc bug
        static_assert(ChannelSpec::BufferSize - LookaheadCommitCount > 1, "");
        static const size_t ChannelCommitBufferSize = ChannelSpec::BufferSize;
        static const size_t ChannelBackupBufferSize = LookaheadBufferSize - LookaheadCommitCount;
        using ChannelCommitBufferSizeType = typename ChooseInt<BitsInInt<ChannelCommitBufferSize>::value, false>::Type;
        using ChannelBackupBufferSizeType = typename ChooseInt<BitsInInt<2 * ChannelBackupBufferSize>::value, false>::Type;
        using LookaheadSizeType = typename ChooseInt<BitsInInt<LookaheadBufferSize>::value, false>::Type;
        
        static Channel * self (Context c)
        {
            return PositionTraverse<typename Context::TheRootPosition, ChannelPosition<ChannelIndex>>(c.root());
        }
        
        static void init (Context c)
        {
            Channel *o = self(c);
            o->m_commit_start = 0;
            o->m_commit_end = 0;
            o->m_backup_start = 0;
            o->m_backup_end = 0;
            o->m_busy = false;
            o->m_timer.init(c);
        }
        
        static void deinit (Context c)
        {
            Channel *o = self(c);
            o->m_timer.deinit(c);
        }
        
        static void abort (Context c)
        {
            Channel *o = self(c);
            o->m_timer.unset(c);
        }
        
        static void write_segment (Context c, Segment *entry)
        {
            MotionPlanner *m = MotionPlanner::self(c);
            TheChannelSegment *channel_entry = UnionGetElem<ChannelIndex>(&entry->channels);
            channel_entry->payload = *UnionGetElem<ChannelIndex>(&m->m_split_buffer.channel_payload);
        }
        
        static bool have_commit_space (bool accum, Context c)
        {
            Channel *o = self(c);
            return (accum && commit_avail(o->m_commit_start, o->m_commit_end) > LookaheadCommitCount);
        }
        
        static void start_commands (Context c)
        {
            Channel *o = self(c);
            MotionPlanner *m = MotionPlanner::self(c);
            o->m_new_commit_end = o->m_commit_end;
            o->m_new_backup_end = m->m_current_backup ? 0 : ChannelBackupBufferSize;
        }
        
        static void gen_command (Context c, Segment *entry, TimeType time)
        {
            Channel *o = self(c);
            MotionPlanner *m = MotionPlanner::self(c);
            TheChannelSegment *channel_entry = UnionGetElem<ChannelIndex>(&entry->channels);
            
            TheChannelCommand *cmd;
            if (!m->m_new_to_backup) {
                cmd = &o->m_commit_buffer[o->m_new_commit_end];
                o->m_new_commit_end = commit_inc(o->m_new_commit_end);
            } else {
                cmd = &o->m_backup_buffer[o->m_new_backup_end];
                o->m_new_backup_end++;
            }
            cmd->payload = channel_entry->payload;
            cmd->time = time;
        }
        
        static void do_commit_cold (Context c)
        {
            Channel *o = self(c);
            MotionPlanner *m = MotionPlanner::self(c);
            o->m_commit_end = o->m_new_commit_end;
            o->m_backup_start = m->m_current_backup ? 0 : ChannelBackupBufferSize;
            o->m_backup_end = o->m_new_backup_end;
        }
        
        template <typename LockContext>
        static void do_commit_hot (LockContext c)
        {
            Channel *o = self(c);
            MotionPlanner *m = MotionPlanner::self(c);
            o->m_commit_end = o->m_new_commit_end;
            o->m_backup_start = m->m_current_backup ? 0 : ChannelBackupBufferSize;
            o->m_backup_end = o->m_new_backup_end;
            if (AMBRO_LIKELY(o->m_commit_start != o->m_commit_end && !o->m_busy)) {
                o->m_busy = true;
                o->m_cmd = &o->m_commit_buffer[o->m_commit_start];
                o->m_commit_start = commit_inc(o->m_commit_start);
                o->m_timer.setFirst(c, o->m_cmd->time);
            }
        }
        
        static void start_stepping (Context c, TimeType start_time)
        {
            Channel *o = self(c);
            AMBRO_ASSERT(!o->m_busy)
            
            if (o->m_commit_start != o->m_commit_end) {
                for (ChannelCommitBufferSizeType i = o->m_commit_start; i != o->m_commit_end; i = commit_inc(i)) {
                    o->m_commit_buffer[i].time += start_time;
                }
                for (ChannelBackupBufferSizeType i = o->m_backup_start; i < o->m_backup_end; i++) {
                    o->m_backup_buffer[i].time += start_time;
                }
                o->m_busy = true;
                o->m_cmd = &o->m_commit_buffer[o->m_commit_start];
                o->m_commit_start = commit_inc(o->m_commit_start);
                o->m_timer.setFirst(c, o->m_cmd->time);
            }
        }
        
        static bool is_busy (bool accum, Context c)
        {
            Channel *o = self(c);
            return (accum || o->m_busy);
        }
        
        static void reset_aborted (Context c)
        {
            Channel *o = self(c);
            o->m_commit_start = 0;
            o->m_commit_end = 0;
            o->m_busy = false;
        }
        
        static void stopped_stepping (Context c)
        {
            Channel *o = self(c);
            o->m_backup_start = 0;
            o->m_backup_end = 0;
        }
        
        static bool timer_handler (TheTimer *, typename TheTimer::HandlerContext c)
        {
            Channel *o = self(c);
            MotionPlanner *m = MotionPlanner::self(c);
            AMBRO_ASSERT(o->m_busy)
            AMBRO_ASSERT(m->m_state == STATE_STEPPING)
            
            c.eventLoop()->template triggerFastEvent<StepperFastEvent>(c);
            ChannelSpec::Callback::call(c, &o->m_cmd->payload);
            if (o->m_commit_start != o->m_commit_end) {
                o->m_cmd = &o->m_commit_buffer[o->m_commit_start];
                o->m_commit_start = commit_inc(o->m_commit_start);
            } else {
                if (o->m_backup_start == o->m_backup_end) {
                    o->m_busy = false;
                    return false;
                }
                m->m_syncing = false;
                o->m_cmd = &o->m_backup_buffer[o->m_backup_start];
                o->m_backup_start++;
            }
            o->m_timer.setNext(c, o->m_cmd->time);
            return true;
        }
        
        static ChannelCommitBufferSizeType commit_inc (ChannelCommitBufferSizeType a)
        {
            a++;
            if (AMBRO_LIKELY(a == ChannelCommitBufferSize)) {
                a = 0;
            }
            return a;
        }
        
        static ChannelCommitBufferSizeType commit_avail (ChannelCommitBufferSizeType start, ChannelCommitBufferSizeType end)
        {
            return (end >= start) ? ((ChannelCommitBufferSize - 1) - (end - start)) : ((start - end) - 1);
        }
        
        ChannelCommitBufferSizeType m_commit_start;
        ChannelCommitBufferSizeType m_commit_end;
        ChannelBackupBufferSizeType m_backup_start;
        ChannelBackupBufferSizeType m_backup_end;
        ChannelCommitBufferSizeType m_new_commit_end;
        ChannelBackupBufferSizeType m_new_backup_end;
        bool m_busy;
        TheChannelCommand m_commit_buffer[ChannelCommitBufferSize];
        TheChannelCommand m_backup_buffer[(size_t)2 * ChannelBackupBufferSize];
        TheChannelCommand *m_cmd;
        TheTimer m_timer;
        
        struct TimerHandler : public AMBRO_WFUNC_TD(&Channel::timer_handler) {};
        struct TimerPosition : public MemberPosition<ChannelPosition<ChannelIndex>, TheTimer, &Channel::m_timer> {};
    };
    
private:
    using AxesTuple = IndexElemTuple<AxesList, Axis>;
    using ChannelsTuple = IndexElemTuple<ChannelsList, Channel>;
    
public:
    static void init (Context c, bool prestep_callback_enabled)
    {
        MotionPlanner *o = self(c);
        
        o->m_pull_finished_event.init(c, MotionPlanner::pull_finished_event_handler);
        c.eventLoop()->template initFastEvent<StepperFastEvent>(c, MotionPlanner::stepper_event_handler);
        o->m_segments_start = 0;
        o->m_segments_staging_length = 0;
        o->m_segments_length = 0;
        o->m_staging_time = 0;
        o->m_staging_v_squared = 0.0;
        o->m_split_buffer.type = 0xFF;
        o->m_state = STATE_BUFFERING;
        o->m_waiting = false;
        o->m_aborted = false;
        o->m_syncing = false;
        o->m_current_backup = false;
#ifdef AMBROLIB_ASSERTIONS
        o->m_pulling = false;
#endif
        TupleForEachForward(&o->m_axes, Foreach_init(), c, prestep_callback_enabled);
        TupleForEachForward(&o->m_channels, Foreach_init(), c);
        o->m_pull_finished_event.prependNowNotAlready(c);
    }
    
    static void deinit (Context c)
    {
        MotionPlanner *o = self(c);
        
        TupleForEachForward(&o->m_channels, Foreach_deinit(), c);
        TupleForEachForward(&o->m_axes, Foreach_deinit(), c);
        c.eventLoop()->template resetFastEvent<StepperFastEvent>(c);
        o->m_pull_finished_event.deinit(c);
    }
    
    static SplitBuffer * getBuffer (Context c)
    {
        MotionPlanner *o = self(c);
        AMBRO_ASSERT(o->m_state != STATE_ABORTED)
        AMBRO_ASSERT(o->m_pulling)
        AMBRO_ASSERT(o->m_split_buffer.type == 0xFF)
        
        return &o->m_split_buffer;
    }
    
    static void axesCommandDone (Context c)
    {
        MotionPlanner *o = self(c);
        AMBRO_ASSERT(o->m_state != STATE_ABORTED)
        AMBRO_ASSERT(o->m_pulling)
        AMBRO_ASSERT(o->m_split_buffer.type == 0xFF)
        AMBRO_ASSERT(FloatIsPosOrPosZero(o->m_split_buffer.rel_max_v_rec))
        TupleForEachForward(&o->m_axes, Foreach_commandDone_assert(), c);
        AMBRO_ASSERT(!TupleForEachForwardAccRes(&o->m_axes, true, Foreach_check_icmd_zero(), c))
        
        o->m_waiting = false;
        o->m_pull_finished_event.unset(c);
#ifdef AMBROLIB_ASSERTIONS
        o->m_pulling = false;
#endif
        
        o->m_split_buffer.type = 0;
        TupleForEachForward(&o->m_axes, Foreach_write_splitbuf(), c);
        o->m_split_buffer.split_pos = 0;
        if (AMBRO_LIKELY(TupleForEachForwardAccRes(&o->m_axes, true, Foreach_splitbuf_fits(), c))) {
            o->m_split_buffer.split_count = 1;
        } else {
            double split_count = ceil(TupleForEachForwardAccRes(&o->m_axes, 0.0, Foreach_compute_split_count(), c));
            o->m_split_buffer.split_frac = 1.0 / split_count;
            o->m_split_buffer.rel_max_v_rec *= o->m_split_buffer.split_frac;
            o->m_split_buffer.split_count = split_count;
        }
        
        c.eventLoop()->template triggerFastEvent<StepperFastEvent>(c);
    }
    
    static void channelCommandDone (Context c, uint8_t channel_index_plus_one)
    {
        MotionPlanner *o = self(c);
        AMBRO_ASSERT(o->m_state != STATE_ABORTED)
        AMBRO_ASSERT(o->m_pulling)
        AMBRO_ASSERT(o->m_split_buffer.type == 0xFF)
        AMBRO_ASSERT(channel_index_plus_one >= 1)
        AMBRO_ASSERT(channel_index_plus_one <= NumChannels)
        
        o->m_waiting = false;
        o->m_pull_finished_event.unset(c);
#ifdef AMBROLIB_ASSERTIONS
        o->m_pulling = false;
#endif
        
        o->m_split_buffer.type = channel_index_plus_one;
        
        c.eventLoop()->template triggerFastEvent<StepperFastEvent>(c);
    }
    
    static void emptyDone (Context c)
    {
        MotionPlanner *o = self(c);
        AMBRO_ASSERT(o->m_state != STATE_ABORTED)
        AMBRO_ASSERT(o->m_pulling)
        AMBRO_ASSERT(o->m_split_buffer.type == 0xFF)
        
        o->m_waiting = false;
        o->m_pull_finished_event.unset(c);
#ifdef AMBROLIB_ASSERTIONS
        o->m_pulling = false;
#endif
        
        o->m_pull_finished_event.prependNowNotAlready(c);
    }
    
    static void waitFinished (Context c)
    {
        MotionPlanner *o = self(c);
        AMBRO_ASSERT(o->m_state != STATE_ABORTED)
        AMBRO_ASSERT(o->m_pulling)
        AMBRO_ASSERT(o->m_split_buffer.type == 0xFF)
        
        if (!o->m_waiting) {
            o->m_waiting = true;
            c.eventLoop()->template triggerFastEvent<StepperFastEvent>(c);
        }
    }
    
    template <int AxisIndex, typename StepsType>
    static StepsType countAbortedRemSteps (Context c)
    {
        MotionPlanner *o = self(c);
        AMBRO_ASSERT(o->m_state == STATE_ABORTED)
        
        return Axis<AxisIndex>::template axis_count_aborted_rem_steps<StepsType>(c);
    }
    
    template <int ChannelIndex>
    typename Channel<ChannelIndex>::TheTimer * getChannelTimer ()
    {
        return &TupleGetElem<ChannelIndex>(&m_channels)->m_timer;
    }
    
    template <int AxisIndex>
    using TheAxisStepperConsumer = AxisStepperConsumer<
        AMBRO_WFUNC_T(&Axis<AxisIndex>::stepper_command_callback),
        AMBRO_WFUNC_T(&Axis<AxisIndex>::stepper_prestep_callback)
    >;
    
    using EventLoopFastEvents = MakeTypeList<StepperFastEvent>;
    
private:
    static MotionPlanner * self (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
    }
    
    static bool plan (Context c)
    {
        MotionPlanner *o = self(c);
        AMBRO_ASSERT(o->m_state != STATE_ABORTED)
        AMBRO_ASSERT(o->m_segments_staging_length != o->m_segments_length)
#ifdef AMBROLIB_ASSERTIONS
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) { AMBRO_ASSERT(planner_have_commit_space(c)) }
#endif
        
        LinearPlannerSegmentState state[LookaheadBufferSize];
        
        SegmentBufferSizeType i = o->m_segments_length;
        double v = 0.0;
        do {
            i--;
            Segment *entry = &o->m_segments[segments_add(o->m_segments_start, i)];
            v = LinearPlannerPush(&entry->lp_seg, &state[i], v);
        } while (i != 0);
        
        SegmentBufferSizeType commit_count = (o->m_segments_length < LookaheadCommitCount) ? o->m_segments_length : LookaheadCommitCount;
        
        o->m_new_to_backup = false;
        TupleForEachForward(&o->m_axes, Foreach_start_commands(), c);
        TupleForEachForward(&o->m_channels, Foreach_start_commands(), c);
        
        TimeType time = o->m_staging_time;
        v = o->m_staging_v_squared;
        double v_start = sqrt(v);
        
        do {
            Segment *entry = &o->m_segments[segments_add(o->m_segments_start, i)];
            LinearPlannerSegmentResult result;
            v = LinearPlannerPull(&entry->lp_seg, &state[i], v, &result);
            if (AMBRO_LIKELY((entry->dir_and_type & TypeMask) == 0)) {
                double v_end = sqrt(v);
                double v_const = sqrt(result.const_v);
                double t0_double = (v_const - v_start) * entry->max_accel_rec;
                double t2_double = (v_const - v_end) * entry->max_accel_rec;
                double t1_double = (1.0 - result.const_start - result.const_end) * entry->rel_max_speed_rec;
                MinTimeType t1 = MinTimeType::importDoubleSaturatedRound(t0_double + t2_double + t1_double);
                time += t1.bitsValue();
                MinTimeType t0 = FixedMin(t1, MinTimeType::importDoubleSaturatedRound(t0_double));
                t1.m_bits.m_int -= t0.bitsValue();
                MinTimeType t2 = FixedMin(t1, MinTimeType::importDoubleSaturatedRound(t2_double));
                t1.m_bits.m_int -= t2.bitsValue();
                TupleForEachForward(&o->m_axes, Foreach_gen_segment_stepper_commands(), c, entry,
                                    result.const_start, result.const_end, t0, t2, t1,
                                    t0_double * t0_double, t2_double * t2_double);
                v_start = v_end;
            } else {
                TupleForOneOffset<1>((entry->dir_and_type & TypeMask), &o->m_channels, Foreach_gen_command(), c, entry, time);
            }
            i++;
            if (AMBRO_UNLIKELY(i == commit_count)) {
                // It's safe to update these here before committing the new plan,
                // since in case of commit failure (loss of sync), plan() will
                // not be called until we're back to buffering state.
                o->m_new_to_backup = true;
                o->m_staging_time = time;
                o->m_staging_v_squared = v;
            }
        } while (i != o->m_segments_length);
        
        bool ok = true;
        
        if (AMBRO_UNLIKELY(o->m_state == STATE_BUFFERING)) {
            TupleForEachForward(&o->m_axes, Foreach_do_commit(), c);
            TupleForEachForward(&o->m_channels, Foreach_do_commit_cold(), c);
            o->m_current_backup = !o->m_current_backup;
        } else {
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                ok = o->m_syncing;
                if (AMBRO_LIKELY(ok)) {
                    TupleForEachForward(&o->m_axes, Foreach_do_commit(), c);
                    TupleForEachForward(&o->m_channels, Foreach_do_commit_hot(), lock_c);
                    o->m_current_backup = !o->m_current_backup;
                }
            }
        }
        
        if (AMBRO_LIKELY(ok)) {
            o->m_segments_start = segments_add(o->m_segments_start, commit_count);
            o->m_segments_length -= commit_count;
            o->m_segments_staging_length = o->m_segments_length;
        }
        return ok;
    }
    
    static void planner_start_stepping (Context c)
    {
        MotionPlanner *o = self(c);
        AMBRO_ASSERT(o->m_state == STATE_BUFFERING)
        AMBRO_ASSERT(!o->m_syncing)
        
        o->m_state = STATE_STEPPING;
        TimeType start_time = c.clock()->getTime(c) + (TimeType)(0.05 * Context::Clock::time_freq);
        o->m_staging_time += start_time;
        TupleForEachForward(&o->m_axes, Foreach_start_stepping(), c, start_time);
        TupleForEachForward(&o->m_channels, Foreach_start_stepping(), c, start_time);
    }
    
    static void pull_finished_event_handler (typename Loop::QueuedEvent *, Context c)
    {
        MotionPlanner *o = self(c);
        AMBRO_ASSERT(o->m_state != STATE_ABORTED)
        AMBRO_ASSERT(o->m_split_buffer.type == 0xFF)
        
        if (AMBRO_UNLIKELY(o->m_waiting)) {
            AMBRO_ASSERT(o->m_pulling)
            AMBRO_ASSERT(o->m_state == STATE_BUFFERING)
            AMBRO_ASSERT(o->m_segments_length == 0)
            AMBRO_ASSERT(!planner_is_busy(c))
            
            o->m_waiting = false;
            return FinishedHandler::call(c);
        } else {
            AMBRO_ASSERT(!o->m_pulling)
            
#ifdef AMBROLIB_ASSERTIONS
            o->m_pulling = true;
#endif
            return PullHandler::call(c);
        }
    }
    
    AMBRO_ALWAYS_INLINE static uint8_t planner_have_commit_space (Context c)
    {
        MotionPlanner *o = self(c);
        return
            TupleForEachForwardAccRes(&o->m_axes, true, Foreach_have_commit_space(), c) &&
            TupleForEachForwardAccRes(&o->m_channels, true, Foreach_have_commit_space(), c);
    }
    
    AMBRO_ALWAYS_INLINE static bool planner_is_busy (Context c)
    {
        MotionPlanner *o = self(c);
        return
            TupleForEachForwardAccRes(&o->m_axes, false, Foreach_is_busy(), c) ||
            TupleForEachForwardAccRes(&o->m_channels, false, Foreach_is_busy(), c);
    }
    
    static void stepper_event_handler (Context c)
    {
        MotionPlanner *o = PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
        AMBRO_ASSERT(o->m_state != STATE_ABORTED)
        
        if (AMBRO_LIKELY(o->m_state == STATE_STEPPING)) {
            bool busy;
            bool aborted;
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                busy = planner_is_busy(c);
                aborted = o->m_aborted;
            }
            
            if (AMBRO_UNLIKELY(aborted)) {
                TupleForEachForward(&o->m_axes, Foreach_abort(), c);
                TupleForEachForward(&o->m_channels, Foreach_abort(), c);
                c.eventLoop()->template resetFastEvent<StepperFastEvent>(c);
                o->m_state = STATE_ABORTED;
                o->m_pull_finished_event.unset(c);
                return AbortedHandler::call(c);
            }
            
            if (AMBRO_UNLIKELY(!busy)) {
                AMBRO_ASSERT(!o->m_syncing)
                o->m_state = STATE_BUFFERING;
                o->m_segments_start = segments_add(o->m_segments_start, o->m_segments_staging_length);
                o->m_segments_length -= o->m_segments_staging_length;
                o->m_segments_staging_length = 0;
                o->m_staging_time = 0;
                o->m_staging_v_squared = 0.0;
                o->m_current_backup = false;
                c.eventLoop()->template resetFastEvent<StepperFastEvent>(c);
                TupleForEachForward(&o->m_axes, Foreach_stopped_stepping(), c);
                TupleForEachForward(&o->m_channels, Foreach_stopped_stepping(), c);
                UnderrunCallback::call(c);
            }
        }
        
        if (AMBRO_UNLIKELY(o->m_waiting)) {
            if (AMBRO_UNLIKELY(o->m_state == STATE_BUFFERING)) {
                if (o->m_segments_length == 0) {
                    o->m_pull_finished_event.prependNowNotAlready(c);
                    return;
                }
                if (o->m_segments_staging_length != o->m_segments_length && planner_have_commit_space(c)) {
                    plan(c);
                }
                planner_start_stepping(c);
            } else if (o->m_segments_staging_length != o->m_segments_length) {
                bool cleared;
                AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                    cleared = o->m_syncing && planner_have_commit_space(c);
                }
                if (cleared) {
                    plan(c);
                }
            }
            return;
        }
        
        while (1) {
            if (AMBRO_LIKELY(o->m_segments_length == LookaheadBufferSize)) {
                if (AMBRO_UNLIKELY(o->m_state == STATE_BUFFERING)) {
                    if (AMBRO_UNLIKELY(!planner_have_commit_space(c))) {
                        planner_start_stepping(c);
                        return;
                    }
                } else {
                    bool cleared;
                    AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                        cleared = o->m_syncing && planner_have_commit_space(c);
                    }
                    if (AMBRO_UNLIKELY(!cleared)) {
                        return;
                    }
                }
                bool ok = plan(c);
                if (AMBRO_UNLIKELY(!ok)) {
                    return;
                }
            }
            
            if (AMBRO_LIKELY(o->m_split_buffer.type == 0xFF)) {
                return;
            }
            
            AMBRO_ASSERT(!o->m_pulling)
            AMBRO_ASSERT(o->m_split_buffer.type != 0 || o->m_split_buffer.split_pos < o->m_split_buffer.split_count)
            
            Segment *entry = &o->m_segments[segments_add(o->m_segments_start, o->m_segments_length)];
            entry->dir_and_type = o->m_split_buffer.type;
            if (AMBRO_LIKELY(o->m_split_buffer.type == 0)) {
                o->m_split_buffer.split_pos++;
                TupleForEachForward(&o->m_axes, Foreach_write_segment_buffer_entry(), c, entry);
                double distance_squared = TupleForEachForwardAccRes(&o->m_axes, 0.0, Foreach_compute_segment_buffer_entry_distance(), entry);
                entry->rel_max_speed_rec = TupleForEachForwardAccRes(&o->m_axes, o->m_split_buffer.rel_max_v_rec, Foreach_compute_segment_buffer_entry_speed(), c, entry);
                double rel_max_accel_rec = TupleForEachForwardAccRes(&o->m_axes, 0.0, Foreach_compute_segment_buffer_entry_accel(), c, entry);
                double distance = sqrt(distance_squared);
                double distance_rec = 1.0 / distance;
                double rel_max_accel = 1.0 / rel_max_accel_rec;
                entry->lp_seg.max_v = distance_squared / (entry->rel_max_speed_rec * entry->rel_max_speed_rec);
                entry->lp_seg.max_end_v = entry->lp_seg.max_v;
                entry->lp_seg.a_x = 2 * rel_max_accel * distance_squared;
                entry->lp_seg.a_x_rec = 1.0 / entry->lp_seg.a_x;
                entry->lp_seg.two_max_v_minus_a_x = 2 * entry->lp_seg.max_v - entry->lp_seg.a_x;
                entry->max_accel_rec = rel_max_accel_rec * distance_rec;
                TupleForEachForward(&o->m_axes, Foreach_write_segment_buffer_entry_extra(), entry, rel_max_accel);
                for (SegmentBufferSizeType i = o->m_segments_length; i > 0; i--) {
                    Segment *prev_entry = &o->m_segments[segments_add(o->m_segments_start, i - 1)];
                    if (AMBRO_LIKELY((prev_entry->dir_and_type & TypeMask) == 0)) {
                        prev_entry->lp_seg.max_end_v = TupleForEachForwardAccRes(&o->m_axes, entry->lp_seg.max_end_v, Foreach_compute_segment_buffer_cornering_speed(), c, entry, distance_rec, prev_entry);
                        break;
                    }
                }
                o->m_last_distance_rec = distance_rec;
                if (AMBRO_LIKELY(o->m_split_buffer.split_pos == o->m_split_buffer.split_count)) {
                    o->m_split_buffer.type = 0xFF;
                }
            } else {
                entry->lp_seg.a_x = 0.0;
                entry->lp_seg.max_v = INFINITY;
                entry->lp_seg.max_end_v = INFINITY;
                entry->lp_seg.a_x_rec = INFINITY;
                entry->lp_seg.two_max_v_minus_a_x = INFINITY;
                TupleForOneOffset<1>((entry->dir_and_type & TypeMask), &o->m_channels, Foreach_write_segment(), c, entry);
                o->m_split_buffer.type = 0xFF;
            }
            o->m_segments_length++;
            
            if (AMBRO_LIKELY(o->m_split_buffer.type == 0xFF)) {
                o->m_pull_finished_event.prependNowNotAlready(c);
            }
        }
    }
    
    static SegmentBufferSizeType segments_add (SegmentBufferSizeType i, SegmentBufferSizeType j)
    {
        SegmentBufferSizeType res = i + j;
        if (res >= LookaheadBufferSize) {
            res -= LookaheadBufferSize;
        }
        return res;
    }
    
    typename Loop::QueuedEvent m_pull_finished_event;
    SegmentBufferSizeType m_segments_start;
    SegmentBufferSizeType m_segments_staging_length;
    SegmentBufferSizeType m_segments_length;
    TimeType m_staging_time;
    double m_staging_v_squared;
    double m_last_distance_rec;
    uint8_t m_state;
    bool m_waiting;
    bool m_aborted;
    bool m_syncing;
    bool m_current_backup;
    bool m_new_to_backup;
#ifdef AMBROLIB_ASSERTIONS
    bool m_pulling;
#endif
    SplitBuffer m_split_buffer;
    Segment m_segments[LookaheadBufferSize];
    AxesTuple m_axes;
    ChannelsTuple m_channels;
    
    template <int AxisIndex> struct AxisPosition : public TuplePosition<Position, AxesTuple, &MotionPlanner::m_axes, AxisIndex> {};
    template <int ChannelIndex> struct ChannelPosition : public TuplePosition<Position, ChannelsTuple, &MotionPlanner::m_channels, ChannelIndex> {};
};

#include <aprinter/EndNamespace.h>

#endif
