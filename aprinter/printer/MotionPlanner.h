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

#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/Tuple.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/TypeListLength.h>
#include <aprinter/meta/TypeListFold.h>
#include <aprinter/meta/IndexElemTuple.h>
#include <aprinter/meta/IndexElemList.h>
#include <aprinter/meta/TupleGet.h>
#include <aprinter/meta/Object.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/Union.h>
#include <aprinter/meta/UnionGet.h>
#include <aprinter/meta/IndexElemUnion.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/JoinTypeLists.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Likely.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/printer/LinearPlanner.h>

#include <aprinter/BeginNamespace.h>

template <
    typename TTheAxisStepper,
    int TStepBits,
    typename TDistanceFactor,
    typename TCorneringDistance,
    typename TPrestepCallback
>
struct MotionPlannerAxisSpec {
    using TheAxisStepper = TTheAxisStepper;
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
    typename Context, typename ParentObject, typename ParamsAxesList, int StepperSegmentBufferSize, int LookaheadBufferSize,
    int LookaheadCommitCount, typename FpType,
    typename PullHandler, typename FinishedHandler, typename AbortedHandler, typename UnderrunCallback,
    typename ParamsChannelsList = EmptyTypeList
>
class MotionPlanner {
public:
    struct Object;
    
private:
    static_assert(StepperSegmentBufferSize - LookaheadCommitCount >= 6, "");
    static_assert(LookaheadBufferSize >= 2, "");
    static_assert(LookaheadCommitCount >= 1, "");
    static_assert(LookaheadCommitCount < LookaheadBufferSize, "");
    using Loop = typename Context::EventLoop;
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    static const int NumAxes = TypeListLength<ParamsAxesList>::value;
    static const int NumChannels = TypeListLength<ParamsChannelsList>::value;
    template <typename AxisSpec, typename AccumType>
    using MinTimeTypeHelper = FixedIntersectTypes<typename AxisSpec::TheAxisStepper::TimeFixedType, AccumType>;
    using MinTimeType = TypeListFold<ParamsAxesList, FixedIdentity, MinTimeTypeHelper>;
    using SegmentBufferSizeType = typename ChooseInt<BitsInInt<2 * LookaheadBufferSize>::value, false>::Type; // twice for segments_add()
    static const size_t StepperCommitBufferSize = 3 * StepperSegmentBufferSize;
    static const size_t StepperBackupBufferSize = 3 * (LookaheadBufferSize - LookaheadCommitCount);
    using StepperCommitBufferSizeType = typename ChooseInt<BitsInInt<StepperCommitBufferSize>::value, false>::Type;
    using StepperBackupBufferSizeType = typename ChooseInt<BitsInInt<2 * StepperBackupBufferSize>::value, false>::Type;
    using StepperFastEvent = typename Context::EventLoop::template FastEventSpec<MotionPlanner>;
    static const int TypeBits = BitsInInt<NumChannels>::value;
    using AxisMaskType = typename ChooseInt<NumAxes + TypeBits, false>::Type;
    static const AxisMaskType TypeMask = ((AxisMaskType)1 << TypeBits) - 1;
    using TheLinearPlanner = LinearPlanner<FpType>;
    
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_init, init)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_deinit, deinit)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_abort, abort)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_commandDone_assert, commandDone_assert)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_write_splitbuf, write_splitbuf)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_splitbuf_fits, splitbuf_fits)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_compute_split_count, compute_split_count)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_check_icmd_zero, check_icmd_zero)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_write_segment_buffer_entry, write_segment_buffer_entry)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_compute_segment_buffer_entry_distance, compute_segment_buffer_entry_distance)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_compute_segment_buffer_entry_speed, compute_segment_buffer_entry_speed)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_compute_segment_buffer_entry_accel, compute_segment_buffer_entry_accel)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_write_segment_buffer_entry_extra, write_segment_buffer_entry_extra)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_compute_segment_buffer_cornering_speed, compute_segment_buffer_cornering_speed)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_have_commit_space, have_commit_space)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_start_commands, start_commands)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_gen_segment_stepper_commands, gen_segment_stepper_commands)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_do_commit, do_commit)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_do_commit_cold, do_commit_cold)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_do_commit_hot, do_commit_hot)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_start_stepping, start_stepping)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_is_busy, is_busy)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_reset_aborted, reset_aborted)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_stopped_stepping, stopped_stepping)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_write_segment, write_segment)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_gen_command, gen_command)
    
public:
    template <int ChannelIndex>
    using ChannelPayload = typename TypeListGet<ParamsChannelsList, ChannelIndex>::Payload;
    
    using ChannelPayloadUnion = IndexElemUnion<ParamsChannelsList, ChannelPayload>;
    
    template <int AxisIndex>
    struct AxisSplitBuffer {
        using AxisSpec = TypeListGet<ParamsAxesList, AxisIndex>;
        using StepFixedType = FixedPoint<AxisSpec::StepBits, false, 0>;
        
        bool dir;
        StepFixedType x;
        FpType max_v_rec;
        FpType max_a_rec;
        StepFixedType x_pos; // internal
    };
    
    struct SplitBuffer {
        uint8_t type; // internal
        union {
            struct {
                FpType rel_max_v_rec;
                FpType split_frac; // internal
                uint32_t split_count; // internal
                uint32_t split_pos; // internal
                IndexElemTuple<ParamsAxesList, AxisSplitBuffer> axes;
            };
            ChannelPayloadUnion channel_payload;
        };
    };
    
private:
    template <int ChannelIndex>
    struct ChannelCommand {
        using ChannelSpec = TypeListGet<ParamsChannelsList, ChannelIndex>;
        using Payload = typename ChannelSpec::Payload;
        
        Payload payload;
        TimeType time;
    };
    
    template <int AxisIndex>
    struct AxisSegment {
        using AxisSpec = TypeListGet<ParamsAxesList, AxisIndex>;
        using TheAxisStepper = typename AxisSpec::TheAxisStepper;
        using StepperStepFixedType = typename TheAxisStepper::StepFixedType;
        
        StepperStepFixedType x;
    };
    
    template <int ChannelIndex>
    struct ChannelSegment {
        using ChannelSpec = TypeListGet<ParamsChannelsList, ChannelIndex>;
        using Payload = typename ChannelSpec::Payload;
        using TheChannelCommand = ChannelCommand<ChannelIndex>;
        
        Payload payload;
    };
    
    struct Segment {
        AxisMaskType dir_and_type;
        typename TheLinearPlanner::SegmentData lp_seg;
        union {
            struct {
                FpType max_accel_rec;
                FpType rel_max_speed_rec;
                FpType half_accel[NumAxes];
                IndexElemTuple<ParamsAxesList, AxisSegment> axes;
            };
            IndexElemUnion<ParamsChannelsList, ChannelSegment> channels;
        };
    };
    
    enum {STATE_BUFFERING, STATE_STEPPING, STATE_ABORTED};
    
public:
    template <int AxisIndex>
    class Axis {
    public:
        using AxisSpec = TypeListGet<ParamsAxesList, AxisIndex>;
        using TheAxisStepper = typename AxisSpec::TheAxisStepper;
        using StepFixedType = FixedPoint<AxisSpec::StepBits, false, 0>;
        using StepperCommandCallbackContext = typename TheAxisStepper::CommandCallbackContext;
        
    public: // private, workaround gcc bug
        friend MotionPlanner;
        
        struct Object;
        using StepperStepFixedType = typename TheAxisStepper::StepFixedType;
        using StepperTimeFixedType = typename TheAxisStepper::TimeFixedType;
        using StepperAccelFixedType = typename TheAxisStepper::AccelFixedType;
        using StepperCommand = typename TheAxisStepper::Command;
        using TheAxisSplitBuffer = AxisSplitBuffer<AxisIndex>;
        using TheAxisSegment = AxisSegment<AxisIndex>;
        static const AxisMaskType TheAxisMask = (AxisMaskType)1 << (AxisIndex + TypeBits);
        
        static void init (Context c, bool prestep_callback_enabled)
        {
            auto *o = Object::self(c);
            o->m_commit_start = 0;
            o->m_commit_end = 0;
            o->m_backup_start = 0;
            o->m_backup_end = 0;
            o->m_busy = false;
            TheAxisStepper::setPrestepCallbackEnabled(c, prestep_callback_enabled);
        }
        
        static void deinit (Context c)
        {
            TheAxisStepper::stop(c);
        }
        
        static void abort (Context c)
        {
            TheAxisStepper::stop(c);
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
        
        static FpType compute_split_count (FpType accum, Context c)
        {
            TheAxisSplitBuffer *axis_split = get_axis_split(c);
            return FloatMax(accum, axis_split->x.template fpValue<FpType>() * (1.0001f / StepperStepFixedType::maxValue().template fpValue<FpType>()));
        }
        
        static bool check_icmd_zero (bool accum, Context c)
        {
            TheAxisSplitBuffer *axis_split = get_axis_split(c);
            return (accum && axis_split->x.bitsValue() == 0);
        }
        
        static void write_segment_buffer_entry (Context c, Segment *entry)
        {
            auto *m = MotionPlanner::Object::self(c);
            TheAxisSplitBuffer *axis_split = get_axis_split(c);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            StepFixedType new_x;
            if (m->m_split_buffer.split_pos == m->m_split_buffer.split_count) {
                new_x = axis_split->x;
            } else {
                new_x = FixedMin(axis_split->x, StepFixedType::template importFpSaturatedRound<FpType>(m->m_split_buffer.split_pos * m->m_split_buffer.split_frac * axis_split->x.template fpValue<FpType>()));
            }
            if (axis_split->dir) {
                entry->dir_and_type |= TheAxisMask;
            }
            axis_entry->x = StepperStepFixedType::importBits(new_x.bitsValue() - axis_split->x_pos.bitsValue());
            axis_split->x_pos = new_x;
        }
        
        static FpType compute_segment_buffer_entry_distance (FpType accum, Segment *entry)
        {
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            return (accum + (axis_entry->x.template fpValue<FpType>() * axis_entry->x.template fpValue<FpType>()) * (FpType)(AxisSpec::DistanceFactor::value() * AxisSpec::DistanceFactor::value()));
        }
        
        static FpType compute_segment_buffer_entry_speed (FpType accum, Context c, Segment *entry)
        {
            TheAxisSplitBuffer *axis_split = get_axis_split(c);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            return FloatMax(accum, axis_entry->x.template fpValue<FpType>() * axis_split->max_v_rec);
        }
        
        static FpType compute_segment_buffer_entry_accel (FpType accum, Context c, Segment *entry)
        {
            TheAxisSplitBuffer *axis_split = get_axis_split(c);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            return FloatMax(accum, axis_entry->x.template fpValue<FpType>() * axis_split->max_a_rec);
        }
        
        static void write_segment_buffer_entry_extra (Segment *entry, FpType rel_max_accel)
        {
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            entry->half_accel[AxisIndex] = 0.5f * rel_max_accel * axis_entry->x.template fpValue<FpType>();
        }
        
        static FpType compute_segment_buffer_cornering_speed (FpType accum, Context c, Segment *entry, FpType entry_distance_rec, Segment *prev_entry)
        {
            auto *m = MotionPlanner::Object::self(c);
            TheAxisSplitBuffer *axis_split = get_axis_split(c);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            TheAxisSegment *prev_axis_entry = TupleGetElem<AxisIndex>(&prev_entry->axes);
            FpType m1 = axis_entry->x.template fpValue<FpType>() * entry_distance_rec;
            FpType m2 = prev_axis_entry->x.template fpValue<FpType>() * m->m_last_distance_rec;
            bool dir_changed = (entry->dir_and_type ^ prev_entry->dir_and_type) & TheAxisMask;
            FpType dm = (dir_changed ? (m1 + m2) : FloatAbs(m1 - m2));
            return FloatMin(accum, (FpType)(AxisSpec::CorneringDistance::value() * AxisSpec::DistanceFactor::value()) / (dm * axis_split->max_a_rec));
        }
        
        static bool have_commit_space (bool accum, Context c)
        {
            auto *o = Object::self(c);
            return (accum && commit_avail(o->m_commit_start, o->m_commit_end) > 3 * LookaheadCommitCount);
        }
        
        static void start_commands (Context c)
        {
            auto *o = Object::self(c);
            auto *m = MotionPlanner::Object::self(c);
            o->m_new_commit_end = o->m_commit_end;
            o->m_new_backup_end = m->m_current_backup ? 0 : StepperBackupBufferSize;
        }
        
        static void gen_segment_stepper_commands (Context c, Segment *entry, FpType frac_x0, FpType frac_x2, MinTimeType t0, MinTimeType t2, MinTimeType t1, FpType t0_squared, FpType t2_squared)
        {
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            
            StepperStepFixedType x1 = axis_entry->x;
            StepperStepFixedType x0 = FixedMin(x1, StepperStepFixedType::template importFpSaturatedRound<FpType>(frac_x0 * axis_entry->x.template fpValue<FpType>()));
            x1.m_bits.m_int -= x0.bitsValue();
            StepperStepFixedType x2 = FixedMin(x1, StepperStepFixedType::template importFpSaturatedRound<FpType>(frac_x2 * axis_entry->x.template fpValue<FpType>()));
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
                gen_stepper_command(c, dir, x0, t0, FixedMin(x0, StepperAccelFixedType::template importFpSaturatedRound<FpType>(entry->half_accel[AxisIndex] * t0_squared)));
            }
            if (gen1) {
                gen_stepper_command(c, dir, x1, t1, StepperAccelFixedType::importBits(0));
            }
            if (x2.bitsValue() != 0) {
                gen_stepper_command(c, dir, x2, t2, -FixedMin(x2, StepperAccelFixedType::template importFpSaturatedRound<FpType>(entry->half_accel[AxisIndex] * t2_squared)));
            }
        }
        
        static void gen_stepper_command (Context c, bool dir, StepperStepFixedType x, StepperTimeFixedType t, StepperAccelFixedType a)
        {
            auto *o = Object::self(c);
            auto *m = MotionPlanner::Object::self(c);
            
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
            auto *o = Object::self(c);
            auto *m = MotionPlanner::Object::self(c);
            o->m_commit_end = o->m_new_commit_end;
            o->m_backup_start = m->m_current_backup ? 0 : StepperBackupBufferSize;
            o->m_backup_end = o->m_new_backup_end;
        }
        
        static void start_stepping (Context c, TimeType start_time)
        {
            auto *o = Object::self(c);
            auto *m = MotionPlanner::Object::self(c);
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
                TheAxisStepper::template start<TheAxisStepperConsumer<AxisIndex>>(c, start_time, cmd);
            }
        }
        
        static bool is_busy (bool accum, Context c)
        {
            auto *o = Object::self(c);
            return (accum || o->m_busy);
        }
        
        static void reset_aborted (Context c)
        {
            auto *o = Object::self(c);
            o->m_commit_start = 0;
            o->m_commit_end = 0;
            o->m_busy = false;
        }
        
        static void stopped_stepping (Context c)
        {
            auto *o = Object::self(c);
            o->m_backup_start = 0;
            o->m_backup_end = 0;
        }
        
        static bool stepper_command_callback (StepperCommandCallbackContext c, StepperCommand **cmd)
        {
            auto *o = Object::self(c);
            auto *m = MotionPlanner::Object::self(c);
            AMBRO_ASSERT(o->m_busy)
            AMBRO_ASSERT(m->m_state == STATE_STEPPING)
            
            Context::EventLoop::template triggerFastEvent<StepperFastEvent>(c);
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
            auto *m = MotionPlanner::Object::self(c);
            bool res = AxisSpec::PrestepCallback::call(c);
            if (AMBRO_UNLIKELY(res)) {
                Context::EventLoop::template triggerFastEvent<StepperFastEvent>(c);
                m->m_aborted = true;
            }
            return res;
        }
        
        template <typename StepsType>
        static void add_command_steps (Context c, StepsType *steps, StepperCommand *cmd)
        {
            bool dir;
            StepperStepFixedType cmd_steps = TheAxisStepper::getPendingCmdSteps(c, cmd, &dir);
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
            auto *o = Object::self(c);
            auto *m = MotionPlanner::Object::self(c);
            
            StepsType steps = 0;
            if (o->m_busy) {
                bool dir;
                StepperStepFixedType cmd_steps = TheAxisStepper::getAbortedCmdSteps(c, &dir);
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
            auto *m = MotionPlanner::Object::self(c);
            return TupleGetElem<AxisIndex>(&m->m_split_buffer.axes);
        }
        
        struct Object : public ObjBase<Axis, typename MotionPlanner::Object, EmptyTypeList> {
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
    };
    
    template <int ChannelIndex>
    class Channel {
    public: // private, workaround gcc bug
        friend MotionPlanner;
        struct Object;
        struct TimerHandler;
        
    public:
        using ChannelSpec = TypeListGet<ParamsChannelsList, ChannelIndex>;
        using Payload = typename ChannelSpec::Payload;
        using TheChannelCommand = ChannelCommand<ChannelIndex>;
        using TheChannelSegment = ChannelSegment<ChannelIndex>;
        using TheTimer = typename ChannelSpec::template Timer<Context, Object, TimerHandler>;
        using CallbackContext = typename TheTimer::HandlerContext;
        
    public: // private, workaround gcc bug
        static_assert(ChannelSpec::BufferSize - LookaheadCommitCount > 1, "");
        static const size_t ChannelCommitBufferSize = ChannelSpec::BufferSize;
        static const size_t ChannelBackupBufferSize = LookaheadBufferSize - LookaheadCommitCount;
        using ChannelCommitBufferSizeType = typename ChooseInt<BitsInInt<ChannelCommitBufferSize>::value, false>::Type;
        using ChannelBackupBufferSizeType = typename ChooseInt<BitsInInt<2 * ChannelBackupBufferSize>::value, false>::Type;
        using LookaheadSizeType = typename ChooseInt<BitsInInt<LookaheadBufferSize>::value, false>::Type;
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            o->m_commit_start = 0;
            o->m_commit_end = 0;
            o->m_backup_start = 0;
            o->m_backup_end = 0;
            o->m_busy = false;
            TheTimer::init(c);
        }
        
        static void deinit (Context c)
        {
            TheTimer::deinit(c);
        }
        
        static void abort (Context c)
        {
            TheTimer::unset(c);
        }
        
        static void write_segment (Context c, Segment *entry)
        {
            auto *m = MotionPlanner::Object::self(c);
            TheChannelSegment *channel_entry = UnionGetElem<ChannelIndex>(&entry->channels);
            channel_entry->payload = *UnionGetElem<ChannelIndex>(&m->m_split_buffer.channel_payload);
        }
        
        static bool have_commit_space (bool accum, Context c)
        {
            auto *o = Object::self(c);
            return (accum && commit_avail(o->m_commit_start, o->m_commit_end) >= LookaheadCommitCount);
        }
        
        static void start_commands (Context c)
        {
            auto *o = Object::self(c);
            auto *m = MotionPlanner::Object::self(c);
            o->m_new_commit_end = o->m_commit_end;
            o->m_new_backup_end = m->m_current_backup ? 0 : ChannelBackupBufferSize;
        }
        
        static void gen_command (Context c, Segment *entry, TimeType time)
        {
            auto *o = Object::self(c);
            auto *m = MotionPlanner::Object::self(c);
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
            auto *o = Object::self(c);
            auto *m = MotionPlanner::Object::self(c);
            o->m_commit_end = o->m_new_commit_end;
            o->m_backup_start = m->m_current_backup ? 0 : ChannelBackupBufferSize;
            o->m_backup_end = o->m_new_backup_end;
        }
        
        template <typename LockContext>
        static void do_commit_hot (LockContext c)
        {
            auto *o = Object::self(c);
            auto *m = MotionPlanner::Object::self(c);
            o->m_commit_end = o->m_new_commit_end;
            o->m_backup_start = m->m_current_backup ? 0 : ChannelBackupBufferSize;
            o->m_backup_end = o->m_new_backup_end;
            if (AMBRO_LIKELY(o->m_commit_start != o->m_commit_end || o->m_backup_start != o->m_backup_end)) {
                o->m_busy = true;
                o->m_cmd = (o->m_commit_start != o->m_commit_end) ? &o->m_commit_buffer[o->m_commit_start] : &o->m_backup_buffer[o->m_backup_start];
                TheTimer::unset(c);
                TheTimer::setFirst(c, o->m_cmd->time);
            }
        }
        
        static void start_stepping (Context c, TimeType start_time)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(!o->m_busy)
            
            for (ChannelCommitBufferSizeType i = o->m_commit_start; i != o->m_commit_end; i = commit_inc(i)) {
                o->m_commit_buffer[i].time += start_time;
            }
            for (ChannelBackupBufferSizeType i = o->m_backup_start; i < o->m_backup_end; i++) {
                o->m_backup_buffer[i].time += start_time;
            }
            if (o->m_commit_start != o->m_commit_end || o->m_backup_start != o->m_backup_end) {
                o->m_busy = true;
                o->m_cmd = (o->m_commit_start != o->m_commit_end) ? &o->m_commit_buffer[o->m_commit_start] : &o->m_backup_buffer[o->m_backup_start];
                TheTimer::setFirst(c, o->m_cmd->time);
            }
        }
        
        static bool is_busy (bool accum, Context c)
        {
            auto *o = Object::self(c);
            return (accum || o->m_busy);
        }
        
        static void reset_aborted (Context c)
        {
            auto *o = Object::self(c);
            o->m_commit_start = 0;
            o->m_commit_end = 0;
            o->m_busy = false;
        }
        
        static void stopped_stepping (Context c)
        {
            auto *o = Object::self(c);
            o->m_backup_start = 0;
            o->m_backup_end = 0;
        }
        
        static bool timer_handler (typename TheTimer::HandlerContext c)
        {
            auto *o = Object::self(c);
            auto *m = MotionPlanner::Object::self(c);
            AMBRO_ASSERT(o->m_busy)
            AMBRO_ASSERT(m->m_state == STATE_STEPPING)
            AMBRO_ASSERT(o->m_commit_start != o->m_commit_end || o->m_backup_start != o->m_backup_end)
            
            Context::EventLoop::template triggerFastEvent<StepperFastEvent>(c);
            ChannelSpec::Callback::call(c, &o->m_cmd->payload);
            if (o->m_commit_start != o->m_commit_end) {
                o->m_commit_start = commit_inc(o->m_commit_start);
            } else {
                o->m_backup_start++;
                m->m_syncing = false;
            }
            if (o->m_commit_start != o->m_commit_end) {
                o->m_cmd = &o->m_commit_buffer[o->m_commit_start];
            } else {
                if (o->m_backup_start == o->m_backup_end) {
                    o->m_busy = false;
                    return false;
                }
                o->m_cmd = &o->m_backup_buffer[o->m_backup_start];
            }
            TheTimer::setNext(c, o->m_cmd->time);
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
        
        struct TimerHandler : public AMBRO_WFUNC_TD(&Channel::timer_handler) {};
        
        struct Object : public ObjBase<Channel, typename MotionPlanner::Object, MakeTypeList<
            TheTimer
        >> {
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
        };
    };
    
private:
    using AxesList = IndexElemList<ParamsAxesList, Axis>;
    using ChannelsList = IndexElemList<ParamsChannelsList, Channel>;
    
public:
    static void init (Context c, bool prestep_callback_enabled)
    {
        auto *o = Object::self(c);
        
        o->m_pull_finished_event.init(c, MotionPlanner::pull_finished_event_handler);
        Context::EventLoop::template initFastEvent<StepperFastEvent>(c, MotionPlanner::stepper_event_handler);
        o->m_segments_start = 0;
        o->m_segments_staging_length = 0;
        o->m_segments_length = 0;
        o->m_staging_time = 0;
        o->m_staging_v_squared = 0.0f;
        o->m_split_buffer.type = 0xFF;
        o->m_state = STATE_BUFFERING;
        o->m_waiting = false;
        o->m_aborted = false;
        o->m_syncing = false;
        o->m_current_backup = false;
#ifdef AMBROLIB_ASSERTIONS
        o->m_pulling = false;
#endif
        ListForEachForward<AxesList>(LForeach_init(), c, prestep_callback_enabled);
        ListForEachForward<ChannelsList>(LForeach_init(), c);
        o->m_pull_finished_event.prependNowNotAlready(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        
        ListForEachForward<ChannelsList>(LForeach_deinit(), c);
        ListForEachForward<AxesList>(LForeach_deinit(), c);
        Context::EventLoop::template resetFastEvent<StepperFastEvent>(c);
        o->m_pull_finished_event.deinit(c);
    }
    
    static SplitBuffer * getBuffer (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state != STATE_ABORTED)
        AMBRO_ASSERT(o->m_pulling)
        AMBRO_ASSERT(o->m_split_buffer.type == 0xFF)
        
        return &o->m_split_buffer;
    }
    
    static void axesCommandDone (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state != STATE_ABORTED)
        AMBRO_ASSERT(o->m_pulling)
        AMBRO_ASSERT(o->m_split_buffer.type == 0xFF)
        AMBRO_ASSERT(FloatIsPosOrPosZero(o->m_split_buffer.rel_max_v_rec))
        ListForEachForward<AxesList>(LForeach_commandDone_assert(), c);
        AMBRO_ASSERT(!ListForEachForwardAccRes<AxesList>(true, LForeach_check_icmd_zero(), c))
        
        o->m_waiting = false;
        o->m_pull_finished_event.unset(c);
#ifdef AMBROLIB_ASSERTIONS
        o->m_pulling = false;
#endif
        
        o->m_split_buffer.type = 0;
        ListForEachForward<AxesList>(LForeach_write_splitbuf(), c);
        o->m_split_buffer.split_pos = 0;
        if (AMBRO_LIKELY(ListForEachForwardAccRes<AxesList>(true, LForeach_splitbuf_fits(), c))) {
            o->m_split_buffer.split_count = 1;
        } else {
            FpType split_count = FloatCeil(ListForEachForwardAccRes<AxesList>(0.0f, LForeach_compute_split_count(), c));
            o->m_split_buffer.split_frac = 1.0f / split_count;
            o->m_split_buffer.rel_max_v_rec *= o->m_split_buffer.split_frac;
            o->m_split_buffer.split_count = split_count;
        }
        
        Context::EventLoop::template triggerFastEvent<StepperFastEvent>(c);
    }
    
    static void channelCommandDone (Context c, uint8_t channel_index_plus_one)
    {
        auto *o = Object::self(c);
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
        
        Context::EventLoop::template triggerFastEvent<StepperFastEvent>(c);
    }
    
    static void emptyDone (Context c)
    {
        auto *o = Object::self(c);
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
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state != STATE_ABORTED)
        AMBRO_ASSERT(o->m_pulling)
        AMBRO_ASSERT(o->m_split_buffer.type == 0xFF)
        
        if (!o->m_waiting) {
            o->m_waiting = true;
            Context::EventLoop::template triggerFastEvent<StepperFastEvent>(c);
        }
    }
    
    template <int AxisIndex, typename StepsType>
    static StepsType countAbortedRemSteps (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state == STATE_ABORTED)
        
        return Axis<AxisIndex>::template axis_count_aborted_rem_steps<StepsType>(c);
    }
    
    template <int ChannelIndex>
    using GetChannelTimer = typename Channel<ChannelIndex>::TheTimer;
    
    template <int AxisIndex>
    using TheAxisStepperConsumer = AxisStepperConsumer<
        AMBRO_WFUNC_T(&Axis<AxisIndex>::stepper_command_callback),
        AMBRO_WFUNC_T(&Axis<AxisIndex>::stepper_prestep_callback)
    >;
    
    using EventLoopFastEvents = MakeTypeList<StepperFastEvent>;
    
private:
    static bool plan (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state != STATE_ABORTED)
        AMBRO_ASSERT(o->m_segments_staging_length != o->m_segments_length)
#ifdef AMBROLIB_ASSERTIONS
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) { AMBRO_ASSERT(planner_have_commit_space(c)) }
#endif
        
        typename TheLinearPlanner::SegmentState state[LookaheadBufferSize];
        
        SegmentBufferSizeType i = o->m_segments_length;
        FpType v = 0.0f;
        do {
            i--;
            Segment *entry = &o->m_segments[segments_add(o->m_segments_start, i)];
            v = TheLinearPlanner::push(&entry->lp_seg, &state[i], v);
        } while (i != 0);
        
        SegmentBufferSizeType commit_count = (o->m_segments_length < LookaheadCommitCount) ? o->m_segments_length : LookaheadCommitCount;
        
        o->m_new_to_backup = false;
        ListForEachForward<AxesList>(LForeach_start_commands(), c);
        ListForEachForward<ChannelsList>(LForeach_start_commands(), c);
        
        TimeType time = o->m_staging_time;
        v = o->m_staging_v_squared;
        FpType v_start = FloatSqrt(v);
        
        do {
            Segment *entry = &o->m_segments[segments_add(o->m_segments_start, i)];
            typename TheLinearPlanner::SegmentResult result;
            v = TheLinearPlanner::pull(&entry->lp_seg, &state[i], v, &result);
            if (AMBRO_LIKELY((entry->dir_and_type & TypeMask) == 0)) {
                FpType v_end = FloatSqrt(v);
                FpType v_const = FloatSqrt(result.const_v);
                FpType t0_double = (v_const - v_start) * entry->max_accel_rec;
                FpType t2_double = (v_const - v_end) * entry->max_accel_rec;
                FpType t1_double = (1.0f - result.const_start - result.const_end) * entry->rel_max_speed_rec;
                MinTimeType t1 = MinTimeType::template importFpSaturatedRound<FpType>(t0_double + t2_double + t1_double);
                time += t1.bitsValue();
                MinTimeType t0 = FixedMin(t1, MinTimeType::template importFpSaturatedRound<FpType>(t0_double));
                t1.m_bits.m_int -= t0.bitsValue();
                MinTimeType t2 = FixedMin(t1, MinTimeType::template importFpSaturatedRound<FpType>(t2_double));
                t1.m_bits.m_int -= t2.bitsValue();
                ListForEachForward<AxesList>(LForeach_gen_segment_stepper_commands(), c, entry,
                                    result.const_start, result.const_end, t0, t2, t1,
                                    t0_double * t0_double, t2_double * t2_double);
                v_start = v_end;
            } else {
                ListForOneOffset<ChannelsList, 1>((entry->dir_and_type & TypeMask), LForeach_gen_command(), c, entry, time);
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
            ListForEachForward<AxesList>(LForeach_do_commit(), c);
            ListForEachForward<ChannelsList>(LForeach_do_commit_cold(), c);
            o->m_current_backup = !o->m_current_backup;
        } else {
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                ok = o->m_syncing;
                if (AMBRO_LIKELY(ok)) {
                    ListForEachForward<AxesList>(LForeach_do_commit(), c);
                    ListForEachForward<ChannelsList>(LForeach_do_commit_hot(), lock_c);
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
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state == STATE_BUFFERING)
        AMBRO_ASSERT(!o->m_syncing)
        
        o->m_state = STATE_STEPPING;
        TimeType start_time = Clock::getTime(c) + (TimeType)(0.05 * Context::Clock::time_freq);
        o->m_staging_time += start_time;
        ListForEachForward<AxesList>(LForeach_start_stepping(), c, start_time);
        ListForEachForward<ChannelsList>(LForeach_start_stepping(), c, start_time);
    }
    
    static void pull_finished_event_handler (typename Loop::QueuedEvent *, Context c)
    {
        auto *o = Object::self(c);
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
        return
            ListForEachForwardAccRes<AxesList>(true, LForeach_have_commit_space(), c) &&
            ListForEachForwardAccRes<ChannelsList>(true, LForeach_have_commit_space(), c);
    }
    
    AMBRO_ALWAYS_INLINE static bool planner_is_busy (Context c)
    {
        return
            ListForEachForwardAccRes<AxesList>(false, LForeach_is_busy(), c) ||
            ListForEachForwardAccRes<ChannelsList>(false, LForeach_is_busy(), c);
    }
    
    static void stepper_event_handler (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state != STATE_ABORTED)
        
        if (AMBRO_LIKELY(o->m_state == STATE_STEPPING)) {
            bool busy;
            bool aborted;
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                busy = planner_is_busy(c);
                aborted = o->m_aborted;
            }
            
            if (AMBRO_UNLIKELY(aborted)) {
                ListForEachForward<AxesList>(LForeach_abort(), c);
                ListForEachForward<ChannelsList>(LForeach_abort(), c);
                Context::EventLoop::template resetFastEvent<StepperFastEvent>(c);
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
                o->m_staging_v_squared = 0.0f;
                o->m_current_backup = false;
                Context::EventLoop::template resetFastEvent<StepperFastEvent>(c);
                ListForEachForward<AxesList>(LForeach_stopped_stepping(), c);
                ListForEachForward<ChannelsList>(LForeach_stopped_stepping(), c);
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
                ListForEachForward<AxesList>(LForeach_write_segment_buffer_entry(), c, entry);
                FpType distance_squared = ListForEachForwardAccRes<AxesList>(0.0f, LForeach_compute_segment_buffer_entry_distance(), entry);
                entry->rel_max_speed_rec = ListForEachForwardAccRes<AxesList>(o->m_split_buffer.rel_max_v_rec, LForeach_compute_segment_buffer_entry_speed(), c, entry);
                FpType rel_max_accel_rec = ListForEachForwardAccRes<AxesList>(0.0f, LForeach_compute_segment_buffer_entry_accel(), c, entry);
                FpType distance = FloatSqrt(distance_squared);
                FpType distance_rec = 1.0f / distance;
                FpType rel_max_accel = 1.0f / rel_max_accel_rec;
                entry->lp_seg.max_v = distance_squared / (entry->rel_max_speed_rec * entry->rel_max_speed_rec);
                entry->lp_seg.max_end_v = entry->lp_seg.max_v;
                entry->lp_seg.a_x = 2 * rel_max_accel * distance_squared;
                entry->lp_seg.a_x_rec = 1.0f / entry->lp_seg.a_x;
                entry->lp_seg.two_max_v_minus_a_x = 2 * entry->lp_seg.max_v - entry->lp_seg.a_x;
                entry->max_accel_rec = rel_max_accel_rec * distance_rec;
                ListForEachForward<AxesList>(LForeach_write_segment_buffer_entry_extra(), entry, rel_max_accel);
                for (SegmentBufferSizeType i = o->m_segments_length; i > 0; i--) {
                    Segment *prev_entry = &o->m_segments[segments_add(o->m_segments_start, i - 1)];
                    if (AMBRO_LIKELY((prev_entry->dir_and_type & TypeMask) == 0)) {
                        prev_entry->lp_seg.max_end_v = ListForEachForwardAccRes<AxesList>(prev_entry->lp_seg.max_end_v, LForeach_compute_segment_buffer_cornering_speed(), c, entry, distance_rec, prev_entry);
                        break;
                    }
                }
                o->m_last_distance_rec = distance_rec;
                if (AMBRO_LIKELY(o->m_split_buffer.split_pos == o->m_split_buffer.split_count)) {
                    o->m_split_buffer.type = 0xFF;
                }
            } else {
                entry->lp_seg.a_x = 0.0f;
                entry->lp_seg.max_v = INFINITY;
                entry->lp_seg.max_end_v = INFINITY;
                entry->lp_seg.a_x_rec = INFINITY;
                entry->lp_seg.two_max_v_minus_a_x = INFINITY;
                ListForOneOffset<ChannelsList, 1>((entry->dir_and_type & TypeMask), LForeach_write_segment(), c, entry);
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
    
public:
    struct Object : public ObjBase<MotionPlanner, ParentObject, JoinTypeLists<
        AxesList,
        ChannelsList
    >> {
        typename Loop::QueuedEvent m_pull_finished_event;
        SegmentBufferSizeType m_segments_start;
        SegmentBufferSizeType m_segments_staging_length;
        SegmentBufferSizeType m_segments_length;
        TimeType m_staging_time;
        FpType m_staging_v_squared;
        FpType m_last_distance_rec;
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
    };
};

#include <aprinter/EndNamespace.h>

#endif
