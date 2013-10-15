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
#include <aprinter/meta/Union.h>
#include <aprinter/meta/UnionGet.h>
#include <aprinter/meta/IndexElemUnion.h>
#include <aprinter/meta/WrapCallback.h>
#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/OffsetCallback.h>
#include <aprinter/base/Likely.h>
#include <aprinter/math/FloatTools.h>
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
    template <typename, typename> class TTimer
>
struct MotionPlannerChannelSpec {
    using Payload = TPayload;
    using Callback = TCallback;
    static const int BufferSize = TBufferSize;
    template<typename X, typename Y> using Timer = TTimer<X, Y>;
};

template <
    typename Position, typename Context, typename AxesList, int StepperSegmentBufferSize, int LookaheadBufferSize,
    typename PullHandler, typename FinishedHandler, typename ChannelsList = EmptyTypeList
>
class MotionPlanner
: private DebugObject<Context, void>
{
private:
    template <int AxisIndex> struct AxisPosition;
    
    static_assert(StepperSegmentBufferSize >= 6, "");
    static_assert(LookaheadBufferSize >= 2, "");
    using Loop = typename Context::EventLoop;
    using TimeType = typename Context::Clock::TimeType;
    static const int NumAxes = TypeListLength<AxesList>::value;
    template <typename AxisSpec, typename AccumType>
    using MinTimeTypeHelper = FixedIntersectTypes<typename AxisSpec::TheAxisStepper::TimeFixedType, AccumType>;
    using MinTimeType = TypeListFold<AxesList, FixedIdentity, MinTimeTypeHelper>;
    using SegmentBufferSizeType = BoundedInt<BitsInInt<LookaheadBufferSize>::value, false>;
    static const size_t NumStepperCommands = 3 * (StepperSegmentBufferSize + 2 * LookaheadBufferSize);
    using StepperCommandSizeType = typename ChooseInt<BitsInInt<NumStepperCommands>::value, true>::Type;
    
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_init, init)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_deinit, deinit)
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
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_gen_segment_stepper_commands, gen_segment_stepper_commands)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_complete_new, complete_new)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_have_commit_space, have_commit_space)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_commit_segment_hot, commit_segment_hot)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_commit_segment_finish, commit_segment_finish)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_dispose_new, dispose_new)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_swap_staging_cold, swap_staging_cold)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_swap_staging_prepare, swap_staging_prepare)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_swap_staging_hot, swap_staging_hot)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_swap_staging_finish, swap_staging_finish)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_start_stepping, start_stepping)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_is_empty, is_empty)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_is_underrun, is_underrun)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_is_aborted, is_aborted)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_reset_aborted, reset_aborted)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_stopped_stepping, stopped_stepping)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_write_segment, write_segment)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_gen_command, gen_command)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_commit_segment, commit_segment)
    
public:
    template <int AxisIndex>
    struct AxisInputCommand {
        using AxisSpec = TypeListGet<AxesList, AxisIndex>;
        using StepFixedType = FixedPoint<AxisSpec::StepBits, false, 0>;
        
        bool dir;
        StepFixedType x;
        double max_v_rec;
        double max_a_rec;
    };
    
    template <int ChannelIndex>
    using ChannelPayload = typename TypeListGet<ChannelsList, ChannelIndex>::Payload;
    
    using ChannelPayloadUnion = IndexElemUnion<ChannelsList, ChannelPayload>;
    
    struct InputCommand {
        uint8_t type;
        union {
            struct {
                double rel_max_v_rec;
                IndexElemTuple<AxesList, AxisInputCommand> axes;
            };
            ChannelPayloadUnion channel_payload;
        };
    };
    
private:
    template <int AxisIndex>
    struct AxisSplitBuffer {
        using AxisSpec = TypeListGet<AxesList, AxisIndex>;
        using StepFixedType = FixedPoint<AxisSpec::StepBits, false, 0>;
        
        bool dir;
        StepFixedType x;
        double max_v_rec;
        double max_a_rec;
        StepFixedType x_pos;
    };
    
    struct SplitBuffer {
        uint8_t type;
        union {
            struct {
                double base_max_v_rec;
                double split_frac;
                uint32_t split_count;
                uint32_t split_pos;
                IndexElemTuple<AxesList, AxisSplitBuffer> axes;
            };
            ChannelPayloadUnion channel_payload;
        };
    };
    
    template <int AxisIndex>
    struct AxisStepperCommand {
        using AxisSpec = TypeListGet<AxesList, AxisIndex>;
        using TheAxisStepper = typename AxisSpec::TheAxisStepper;
        using StepperCommand = typename TheAxisStepper::Command;
        
        StepperCommand scmd;
        StepperCommandSizeType next;
    };
    
    template <int ChannelIndex>
    struct ChannelCommand {
        using ChannelSpec = TypeListGet<ChannelsList, ChannelIndex>;
        using Payload = typename ChannelSpec::Payload;
        static const size_t NumChannelCommands = ChannelSpec::BufferSize + 2 * LookaheadBufferSize;
        using ChannelCommandSizeType = typename ChooseInt<BitsInInt<NumChannelCommands>::value, true>::Type;
        
        Payload payload;
        TimeType time;
        ChannelCommandSizeType next;
    };
    
    template <int AxisIndex>
    struct AxisSegment {
        using AxisSpec = TypeListGet<AxesList, AxisIndex>;
        using TheAxisStepper = typename AxisSpec::TheAxisStepper;
        using StepperStepFixedType = typename TheAxisStepper::StepFixedType;
        
        bool dir;
        StepperStepFixedType x;
        double half_accel;
    };
    
    template <int ChannelIndex>
    struct ChannelSegment {
        using ChannelSpec = TypeListGet<ChannelsList, ChannelIndex>;
        using Payload = typename ChannelSpec::Payload;
        using TheChannelCommand = ChannelCommand<ChannelIndex>;
        
        Payload payload;
        typename TheChannelCommand::ChannelCommandSizeType command;
    };
    
    struct Segment {
        uint8_t type;
        LinearPlannerSegmentData lp_seg;
        union {
            struct {
                double max_accel_rec;
                double rel_max_speed_rec;
                IndexElemTuple<AxesList, AxisSegment> axes;
            };
            IndexElemUnion<ChannelsList, ChannelSegment> channels;
        };
    };
    
    template <int ChannelIndex>
    using ChannelCommandSizeTypeAlias = typename ChannelCommand<ChannelIndex>::ChannelCommandSizeType;
    
    using ChannelCommandSizeTypeTuple = IndexElemTuple<ChannelsList, ChannelCommandSizeTypeAlias>;
    
public:
    template <int AxisIndex>
    class Axis {
    public:
        using AxisSpec = TypeListGet<AxesList, AxisIndex>;
        using TheAxisStepper = typename AxisSpec::TheAxisStepper;
        using StepFixedType = FixedPoint<AxisSpec::StepBits, false, 0>;
        using TheAxisInputCommand = AxisInputCommand<AxisIndex>;
        using StepperCommandCallbackContext = typename TheAxisStepper::CommandCallbackContext;
        
    private:
        friend MotionPlanner;
        
        using StepperStepFixedType = typename TheAxisStepper::StepFixedType;
        using StepperTimeFixedType = typename TheAxisStepper::TimeFixedType;
        using StepperAccelFixedType = typename TheAxisStepper::AccelFixedType;
        using StepperCommand = typename TheAxisStepper::Command;
        using TheAxisSplitBuffer = AxisSplitBuffer<AxisIndex>;
        using TheAxisSegment = AxisSegment<AxisIndex>;
        using TheAxisStepperCommand = AxisStepperCommand<AxisIndex>;
        static const bool PrestepCallbackEnabled = !TypesAreEqual<typename AxisSpec::PrestepCallback, void>::value;
        
        MotionPlanner * parent ()
        {
            return AMBRO_WMEMB_TD(&MotionPlanner::m_axes)::container(TupleGetTuple<AxisIndex, AxesTuple>(this));
        }
        
        TheAxisStepper * stepper ()
        {
            return AxisSpec::GetAxisStepper::call(parent());
        }
        
        void init (Context c, bool prestep_callback_enabled)
        {
            m_first = -1;
            m_free_first = -1;
            m_new_first = -1;
            m_num_committed = 0;
            m_last_committed = 0;
            for (size_t i = 0; i < NumStepperCommands; i++) {
                m_stepper_entries[i].next = m_free_first;
                m_free_first = i;
            }
            stepper()->setPrestepCallbackEnabled(c, prestep_callback_enabled);
        }
        
        void deinit (Context c)
        {
            stepper()->stop(c);
        }
        
        void commandDone_assert (InputCommand *icmd)
        {
            TheAxisInputCommand *axis_icmd = TupleGetElem<AxisIndex>(&icmd->axes);
            AMBRO_ASSERT(FloatIsPosOrPosZero(axis_icmd->max_v_rec))
            AMBRO_ASSERT(FloatIsPosOrPosZero(axis_icmd->max_a_rec))
        }
        
        void write_splitbuf (InputCommand *icmd)
        {
            MotionPlanner *o = parent();
            TheAxisInputCommand *axis_icmd = TupleGetElem<AxisIndex>(&icmd->axes);
            TheAxisSplitBuffer *axis_split = TupleGetElem<AxisIndex>(&o->m_split_buffer.axes);
            axis_split->dir = axis_icmd->dir;
            axis_split->x = axis_icmd->x;
            axis_split->max_v_rec = axis_icmd->max_v_rec;
            axis_split->max_a_rec = axis_icmd->max_a_rec;
            axis_split->x_pos = StepFixedType::importBits(0);
        }
        
        bool splitbuf_fits (bool accum)
        {
            MotionPlanner *o = parent();
            TheAxisSplitBuffer *axis_split = TupleGetElem<AxisIndex>(&o->m_split_buffer.axes);
            return (accum && axis_split->x <= StepperStepFixedType::maxValue());
        }
        
        double compute_split_count (double accum)
        {
            MotionPlanner *o = parent();
            TheAxisSplitBuffer *axis_split = TupleGetElem<AxisIndex>(&o->m_split_buffer.axes);
            return fmax(accum, axis_split->x.doubleValue() * (1.0 / StepperStepFixedType::maxValue().doubleValue()));
        }
        
        bool check_icmd_zero (bool accum, InputCommand *icmd)
        {
            TheAxisInputCommand *axis_icmd = TupleGetElem<AxisIndex>(&icmd->axes);
            return (accum && axis_icmd->x.bitsValue() == 0);
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
                new_x = FixedMin(axis_split->x, StepFixedType::importDoubleSaturatedRound(o->m_split_buffer.split_pos * o->m_split_buffer.split_frac * axis_split->x.doubleValue()));
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
            return fmax(accum, axis_entry->x.doubleValue() * axis_split->max_v_rec);
        }
        
        double compute_segment_buffer_entry_accel (double accum, Segment *entry)
        {
            MotionPlanner *o = parent();
            TheAxisSplitBuffer *axis_split = TupleGetElem<AxisIndex>(&o->m_split_buffer.axes);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            return fmax(accum, axis_entry->x.doubleValue() * axis_split->max_a_rec);
        }
        
        double write_segment_buffer_entry_extra (Segment *entry, double rel_max_accel)
        {
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            axis_entry->half_accel = 0.5 * rel_max_accel * axis_entry->x.doubleValue();
        }
        
        double compute_segment_buffer_cornering_speed (double accum, Segment *entry, double entry_distance_rec, Segment *prev_entry)
        {
            MotionPlanner *o = parent();
            TheAxisSplitBuffer *axis_split = TupleGetElem<AxisIndex>(&o->m_split_buffer.axes);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            TheAxisSegment *prev_axis_entry = TupleGetElem<AxisIndex>(&prev_entry->axes);
            double m1 = axis_entry->x.doubleValue() * entry_distance_rec;
            double m2 = prev_axis_entry->x.doubleValue() * o->m_last_distance_rec;
            bool dir_changed = axis_entry->dir != prev_axis_entry->dir;
            double dm = (dir_changed ? (m1 + m2) : fabs(m1 - m2));
            return fmin(accum, (AxisSpec::CorneringDistance::value() * AxisSpec::DistanceFactor::value()) / (dm * axis_split->max_a_rec));
        }
        
        void gen_segment_stepper_commands (Context c, Segment *entry, double frac_x0, double frac_x2, MinTimeType t0, MinTimeType t2, MinTimeType t1, double t0_squared, double t2_squared, bool is_first)
        {
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            
            StepperStepFixedType x1 = axis_entry->x;
            StepperStepFixedType x0 = FixedMin(x1, StepperStepFixedType::importDoubleSaturatedRound(frac_x0 * axis_entry->x.doubleValue()));
            x1.m_bits.m_int -= x0.bitsValue();
            StepperStepFixedType x2 = FixedMin(x1, StepperStepFixedType::importDoubleSaturatedRound(frac_x2 * axis_entry->x.doubleValue()));
            x1.m_bits.m_int -= x2.bitsValue();
            
            StepperAccelFixedType a0;
            StepperAccelFixedType a2;
            
            if (x0.bitsValue() != 0) {
                a0 = StepperAccelFixedType::importDoubleSaturatedRound(axis_entry->half_accel * t0_squared);
                if (a0.bitsValue() > x0.bitsValue()) {
                    a0.m_bits.m_int = x0.bitsValue();
                }
            } else {
                t1.m_bits.m_int += t0.bitsValue();
            }
            if (x2.bitsValue() != 0) {
                a2 = StepperAccelFixedType::importDoubleSaturatedRound(axis_entry->half_accel * t2_squared);
                if (a2.bitsValue() > x2.bitsValue()) {
                    a2.m_bits.m_int = x2.bitsValue();
                }
            } else {
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
            
            uint8_t num_stepper_entries = 0;
            if (x0.bitsValue() != 0) {
                num_stepper_entries++;
                gen_stepper_command(c, axis_entry, x0, t0, a0);
            }
            if (gen1) {
                num_stepper_entries++;
                gen_stepper_command(c, axis_entry, x1, t1, StepperAccelFixedType::importBits(0));
            }
            if (x2.bitsValue() != 0) {
                num_stepper_entries++;
                gen_stepper_command(c, axis_entry, x2, t2, -a2);
            }
            
            if (AMBRO_UNLIKELY(is_first)) {
                m_first_segment_num_stepper_entries = num_stepper_entries;
                m_first_segment_last_stepper_entry = m_new_last;
            }
        }
        
        void gen_stepper_command (Context c, TheAxisSegment *axis_entry, StepperStepFixedType x, StepperTimeFixedType t, StepperAccelFixedType a)
        {
            MotionPlanner *o = parent();
            
            StepperCommandSizeType entry = m_free_first;
            m_free_first = m_stepper_entries[m_free_first].next;
            TheAxisStepper::generate_command(axis_entry->dir, x, t, a, &m_stepper_entries[entry].scmd);
            if (!(m_new_first >= 0)) {
                m_new_first = entry;
            }
            m_new_last = entry;
        }
        
        void complete_new ()
        {
            if (m_new_first >= 0) {
                m_stepper_entries[m_new_last].next = -1;
            }
        }
        
        bool have_commit_space (bool accum)
        {
            return (accum && m_num_committed <= 3 * (StepperSegmentBufferSize - 1));
        }
        
        void commit_segment_hot (Segment *entry)
        {
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            AMBRO_ASSERT(m_num_committed <= 3 * StepperSegmentBufferSize - m_first_segment_num_stepper_entries)
            m_num_committed += m_first_segment_num_stepper_entries;
        }
        
        void commit_segment_finish (Segment *entry)
        {
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            m_last_committed = m_first_segment_last_stepper_entry;
        }
        
        void dispose_new (Context c)
        {
            MotionPlanner *o = parent();
            if (m_new_first >= 0) {
                m_stepper_entries[m_new_last].next = m_free_first;
                m_free_first = m_new_first;
                m_new_first = -1;
            }
        }
        
        void swap_staging_cold ()
        {
            if (!(m_new_first >= 0)) {
                return;
            }
            if (m_stepper_entries[m_last_committed].next >= 0) {
                m_stepper_entries[m_last].next = m_free_first;
                m_free_first = m_stepper_entries[m_last_committed].next;
            }
            m_stepper_entries[m_last_committed].next = m_new_first;
            if (m_num_committed == 0) {
                m_first = m_new_first;
            }
            m_last = m_new_last;
            m_new_first = -1;
        }
        
        void swap_staging_prepare (StepperCommandSizeType *old_first)
        {
            AMBRO_ASSERT(m_num_committed > 0)
            
            old_first[AxisIndex] = m_stepper_entries[m_last_committed].next;
        }
        
        void swap_staging_hot ()
        {
            if (AMBRO_LIKELY(m_new_first >= 0)) {
                m_stepper_entries[m_last_committed].next = m_new_first;
            }
        }
        
        void swap_staging_finish (StepperCommandSizeType *old_first)
        {
            if (m_new_first >= 0) {
                StepperCommandSizeType old_last = m_last;
                m_last = m_new_last;
                m_new_first = old_first[AxisIndex];
                m_new_last = old_last;
            }
        }
        
        void start_stepping (Context c, TimeType start_time)
        {
            if (!(m_first >= 0)) {
                return;
            }
            stepper()->template start<TheAxisStepperConsumer<AxisIndex>>(c, start_time, &m_stepper_entries[m_first].scmd);
        }
        
        bool is_empty (bool accum)
        {
            return (accum && !(m_first >= 0));
        }
        
        bool is_underrun (bool accum)
        {
            return (accum || (m_num_committed <= 0));
        }
        
        bool is_aborted (bool accum)
        {
            return (accum || (PrestepCallbackEnabled && m_first == -2));
        }
        
        void reset_aborted ()
        {
            m_first = -1;
        }
        
        void stopped_stepping (Context c)
        {
            m_num_committed = 0;
        }
        
        StepperCommand * stepper_command_callback (StepperCommandCallbackContext c)
        {
            MotionPlanner *o = parent();
            AMBRO_ASSERT(m_first >= 0)
            AMBRO_ASSERT(o->m_stepping)
            
            o->m_stepper_event.appendNowIfNotAlready(c);
            m_num_committed--;
            m_first = m_stepper_entries[m_first].next;
            return (m_first >= 0 ? &m_stepper_entries[m_first].scmd : NULL);
        }
        
        bool stepper_prestep_callback (StepperCommandCallbackContext c)
        {
            return PrestepCallbackHelper<PrestepCallbackEnabled>::call(this, c);
        }
        
        template <bool Enabled, typename Dummy = void>
        struct PrestepCallbackHelper {
            static bool call (Axis *axis, StepperCommandCallbackContext c)
            {
                return false;
            }
        };
        
        template <typename Dummy>
        struct PrestepCallbackHelper<true, Dummy> {
            static bool call (Axis *axis, StepperCommandCallbackContext c)
            {
                MotionPlanner *o = axis->parent();
                
                bool res = AxisSpec::PrestepCallback::call(o, c);
                if (AMBRO_UNLIKELY(res)) {
                    o->m_stepper_event.appendNowIfNotAlready(c);
                    axis->m_num_committed = 0;
                    axis->m_first = -2;
                }
                return res;
            }
        };
        
        StepperCommandSizeType m_first;
        StepperCommandSizeType m_last_committed;
        StepperCommandSizeType m_last;
        StepperCommandSizeType m_new_first;
        StepperCommandSizeType m_new_last;
        StepperCommandSizeType m_free_first;
        StepperCommandSizeType m_num_committed;
        uint8_t m_first_segment_num_stepper_entries;
        StepperCommandSizeType m_first_segment_last_stepper_entry;
        TheAxisStepperCommand m_stepper_entries[NumStepperCommands];
    };
    
    template <int ChannelIndex>
    class Channel {
    private:
        friend MotionPlanner;
        struct TimerHandler;
        
    public:
        using ChannelSpec = TypeListGet<ChannelsList, ChannelIndex>;
        using Payload = typename ChannelSpec::Payload;
        using TheChannelCommand = ChannelCommand<ChannelIndex>;
        using TheChannelSegment = ChannelSegment<ChannelIndex>;
        using TheTimer = typename ChannelSpec::template Timer<Context, TimerHandler>;
        using CallbackContext = typename TheTimer::HandlerContext;
        
    private:
        static_assert(ChannelSpec::BufferSize > 0, "");
        static const size_t NumChannelCommands = TheChannelCommand::NumChannelCommands;
        using ChannelCommandSizeType = typename TheChannelCommand::ChannelCommandSizeType;
        
        MotionPlanner * parent ()
        {
            return AMBRO_WMEMB_TD(&MotionPlanner::m_channels)::container(TupleGetTuple<ChannelIndex, ChannelsTuple>(this));
        }
        
        void init (Context c)
        {
            m_first = -1;
            m_free_first = -1;
            m_new_first = -1;
            m_num_committed = 0;
            m_last_committed = 0;
            for (size_t i = 0; i < NumChannelCommands; i++) {
                m_channel_commands[i].next = m_free_first;
                m_free_first = i;
            }
            m_timer.init(c);
        }
        
        void deinit (Context c)
        {
            m_timer.deinit(c);
        }
        
        void write_segment (Segment *entry)
        {
            MotionPlanner *o = parent();
            TheChannelSegment *channel_entry = UnionGetElem<ChannelIndex>(&entry->channels);
            channel_entry->payload = *UnionGetElem<ChannelIndex>(&o->m_split_buffer.channel_payload);
        }
        
        void gen_command (Context c, Segment *entry, TimeType time)
        {
            MotionPlanner *o = parent();
            TheChannelSegment *channel_entry = UnionGetElem<ChannelIndex>(&entry->channels);
            
            ChannelCommandSizeType cmd = m_free_first;
            m_free_first = m_channel_commands[m_free_first].next;
            m_channel_commands[cmd].payload = channel_entry->payload;
            m_channel_commands[cmd].time = time;
            if (!(m_new_first >= 0)) {
                m_new_first = cmd;
            }
            m_new_last = cmd;
            channel_entry->command = cmd;
        }
        
        void complete_new ()
        {
            if (m_new_first >= 0) {
                m_channel_commands[m_new_last].next = -1;
            }
        }
        
        void dispose_new (Context c)
        {
            MotionPlanner *o = parent();
            if (m_new_first >= 0) {
                m_channel_commands[m_new_last].next = m_free_first;
                m_free_first = m_new_first;
                m_new_first = -1;
            }
        }
        
        void swap_staging_cold ()
        {
            if (!(m_new_first >= 0)) {
                return;
            }
            if (m_channel_commands[m_last_committed].next >= 0) {
                m_channel_commands[m_last].next = m_free_first;
                m_free_first = m_channel_commands[m_last_committed].next;
            }
            m_channel_commands[m_last_committed].next = m_new_first;
            if (m_num_committed == 0) {
                m_first = m_new_first;
            }
            m_last = m_new_last;
            m_new_first = -1;
        }
        
        void swap_staging_prepare (ChannelCommandSizeTypeTuple *old_first_tuple)
        {
            AMBRO_ASSERT(m_num_committed >= 0)
            
            *TupleGetElem<ChannelIndex>(old_first_tuple) = m_channel_commands[m_last_committed].next;
        }
        
        template <typename LockContext>
        void swap_staging_hot (LockContext c)
        {
            if (AMBRO_LIKELY(m_new_first >= 0)) {
                m_channel_commands[m_last_committed].next = m_new_first;
                if (AMBRO_LIKELY(m_num_committed == 0)) {
                    m_first = m_new_first;
                    m_timer.unset(c);
                    m_timer.set(c, m_channel_commands[m_first].time);
                }
            }
        }
        
        void swap_staging_finish (ChannelCommandSizeTypeTuple *old_first_tuple)
        {
            if (m_new_first >= 0) {
                ChannelCommandSizeType old_last = m_last;
                m_last = m_new_last;
                m_new_first = *TupleGetElem<ChannelIndex>(old_first_tuple);
                m_new_last = old_last;
            }
        }
        
        void start_stepping (Context c, TimeType start_time)
        {
            if (!(m_first >= 0)) {
                return;
            }
            for (ChannelCommandSizeType cmd = m_first; cmd >= 0; cmd = m_channel_commands[cmd].next) {
                m_channel_commands[cmd].time += start_time;
            }
            m_timer.set(c, m_channel_commands[m_first].time);
        }
        
        bool commit_segment (Context c, Segment *entry)
        {
            MotionPlanner *o = parent();
            TheChannelSegment *channel_entry = UnionGetElem<ChannelIndex>(&entry->channels);
            
            if (!o->m_stepping) {
                if (m_num_committed == ChannelSpec::BufferSize) {
                    o->start_stepping(c);
                    return false;
                }
                m_num_committed++;
            } else {
                bool cleared = false;
                AMBRO_LOCK_T(o->m_lock, c, lock_c, {
                    o->m_underrun = o->is_underrun();
                    if (!o->m_underrun && m_num_committed != ChannelSpec::BufferSize) {
                        m_num_committed++;
                        cleared = true;
                    }
                });
                if (!cleared) {
                    return false;
                }
            }
            m_last_committed = channel_entry->command;
            return true;
        }
        
        bool is_empty (bool accum)
        {
            return (accum && !(m_first >= 0));
        }
        
        bool is_underrun (bool accum)
        {
            return (accum || (m_num_committed < 0));
        }
        
        void stopped_stepping (Context c)
        {
            m_num_committed = 0;
        }
        
        bool timer_handler (typename TheTimer::HandlerContext c)
        {
            MotionPlanner *o = parent();
            AMBRO_ASSERT(m_first >= 0)
            AMBRO_ASSERT(o->m_stepping)
            
            ChannelSpec::Callback::call(o, c, &m_channel_commands[m_first].payload);
            
            o->m_stepper_event.appendNowIfNotAlready(c);
            m_num_committed--;
            m_first = m_channel_commands[m_first].next;
            if (!(m_first >= 0)) {
                return false;
            }
            m_timer.set(c, m_channel_commands[m_first].time);
            return true;
        }
        
        ChannelCommandSizeType m_first;
        ChannelCommandSizeType m_last_committed;
        ChannelCommandSizeType m_last;
        ChannelCommandSizeType m_new_first;
        ChannelCommandSizeType m_new_last;
        ChannelCommandSizeType m_free_first;
        ChannelCommandSizeType m_num_committed;
        TheTimer m_timer;
        TheChannelCommand m_channel_commands[NumChannelCommands];
        
        struct TimerHandler : public AMBRO_WCALLBACK_TD(&Channel::timer_handler, &Channel::m_timer) {};
    };
    
private:
    using AxesTuple = IndexElemTuple<AxesList, Axis>;
    using ChannelsTuple = IndexElemTuple<ChannelsList, Channel>;
    
public:
    void init (Context c, bool prestep_callback_enabled)
    {
        m_lock.init(c);
        m_pull_finished_event.init(c, AMBRO_OFFSET_CALLBACK_T(&MotionPlanner::m_pull_finished_event, &MotionPlanner::pull_finished_event_handler));
        m_stepper_event.init(c, AMBRO_OFFSET_CALLBACK_T(&MotionPlanner::m_stepper_event, &MotionPlanner::stepper_event_handler));
        m_segments_start = SegmentBufferSizeType::import(0);
        m_segments_staging_end = SegmentBufferSizeType::import(0);
        m_segments_end = SegmentBufferSizeType::import(0);
        m_staging_time = 0;
        m_staging_v_squared = 0.0;
        m_split_buffer.type = 0xFF;
        m_stepping = false;
        m_underrun = true;
        m_waiting = false;
#ifdef AMBROLIB_ASSERTIONS
        m_pulling = false;
#endif
        TupleForEachForward(&m_axes, Foreach_init(), c, prestep_callback_enabled);
        TupleForEachForward(&m_channels, Foreach_init(), c);
        m_pull_finished_event.prependNowNotAlready(c);
#ifdef MOTIONPLANNER_BENCHMARK
        if (NumAxes > 1) {
            m_bench_time = 0;
        }
#endif
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        
        TupleForEachForward(&m_channels, Foreach_deinit(), c);
        TupleForEachForward(&m_axes, Foreach_deinit(), c);
        m_stepper_event.deinit(c);
        m_pull_finished_event.deinit(c);
        m_lock.deinit(c);
    }
    
    void commandDone (Context c, InputCommand *icmd)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_pulling)
        AMBRO_ASSERT(m_split_buffer.type == 0xFF)
        if (icmd->type == 0) {
            AMBRO_ASSERT(FloatIsPosOrPosZero(icmd->rel_max_v_rec))
            TupleForEachForward(&m_axes, Foreach_commandDone_assert(), icmd);
        }
        
        m_waiting = false;
        m_pull_finished_event.unset(c);
#ifdef AMBROLIB_ASSERTIONS
        m_pulling = false;
#endif
        
        m_split_buffer.type = icmd->type;
        if (m_split_buffer.type == 0) {
            if (TupleForEachForwardAccRes(&m_axes, true, Foreach_check_icmd_zero(), icmd)) {
                m_split_buffer.type = 0xFF;
                m_pull_finished_event.prependNowNotAlready(c);
                return;
            }
            TupleForEachForward(&m_axes, Foreach_write_splitbuf(), icmd);
            m_split_buffer.split_pos = 0;
            if (AMBRO_LIKELY(TupleForEachForwardAccRes(&m_axes, true, Foreach_splitbuf_fits()))) {
                m_split_buffer.base_max_v_rec = icmd->rel_max_v_rec;
                m_split_buffer.split_count = 1;
            } else {
                double split_count_comp = TupleForEachForwardAccRes(&m_axes, 0.0, Foreach_compute_split_count());
                double split_count = ceil(split_count_comp + 0.5);
                m_split_buffer.split_frac = 1.0 / split_count;
                m_split_buffer.base_max_v_rec = icmd->rel_max_v_rec * m_split_buffer.split_frac;
                m_split_buffer.split_count = split_count;
            }
        } else {
            m_split_buffer.channel_payload = icmd->channel_payload;
        }
        
        work(c);
    }
    
    void waitFinished (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_pulling)
        AMBRO_ASSERT(m_split_buffer.type == 0xFF)
        
        if (!m_waiting) {
            m_waiting = true;
            if (!m_stepping) {
                continue_wait(c);
            }
        }
    }
    
    template <int ChannelIndex>
    typename Channel<ChannelIndex>::TheTimer * getChannelTimer ()
    {
        return &TupleGetElem<ChannelIndex>(&m_channels)->m_timer;
    }
    
    template <int AxisIndex>
    using TheAxisStepperConsumer = AxisStepperConsumer<
        AxisPosition<AxisIndex>,
        AMBRO_WMETHOD_T(&Axis<AxisIndex>::stepper_command_callback),
        AMBRO_WMETHOD_T(&Axis<AxisIndex>::stepper_prestep_callback)
    >;
    
private:
    void work (Context c)
    {
        AMBRO_ASSERT(m_split_buffer.type != 0xFF)
        AMBRO_ASSERT(!m_pulling)
        AMBRO_ASSERT(m_split_buffer.type != 0 || m_split_buffer.split_pos < m_split_buffer.split_count)
        
        while (1) {
            do {
                if (BoundedModuloSubtract(m_segments_end, m_segments_start).value() == LookaheadBufferSize) {
                    m_underrun = m_underrun && m_stepping;
                    break;
                }
                Segment *entry = &m_segments[m_segments_end.value()];
                entry->type = m_split_buffer.type;
                if (entry->type == 0) {
                    m_split_buffer.split_pos++;
                    TupleForEachForward(&m_axes, Foreach_write_segment_buffer_entry(), entry);
                    double distance_squared = TupleForEachForwardAccRes(&m_axes, 0.0, Foreach_compute_segment_buffer_entry_distance(), entry);
                    entry->rel_max_speed_rec = TupleForEachForwardAccRes(&m_axes, m_split_buffer.base_max_v_rec, Foreach_compute_segment_buffer_entry_speed(), entry);
                    double rel_max_accel_rec = TupleForEachForwardAccRes(&m_axes, 0.0, Foreach_compute_segment_buffer_entry_accel(), entry);
                    double distance = sqrt(distance_squared);
                    double rel_max_accel = 1.0 / rel_max_accel_rec;
                    entry->lp_seg.max_v = distance_squared / (entry->rel_max_speed_rec * entry->rel_max_speed_rec);
                    entry->lp_seg.max_start_v = entry->lp_seg.max_v;
                    entry->lp_seg.a_x = 2 * rel_max_accel * distance_squared;
                    entry->lp_seg.a_x_rec = 1.0 / entry->lp_seg.a_x;
                    entry->lp_seg.two_max_v_minus_a_x = 2 * entry->lp_seg.max_v - entry->lp_seg.a_x;
                    entry->max_accel_rec = rel_max_accel_rec / distance;
                    TupleForEachForward(&m_axes, Foreach_write_segment_buffer_entry_extra(), entry, rel_max_accel);
                    double distance_rec = 1.0 / distance;
                    for (SegmentBufferSizeType i = m_segments_end; i != m_segments_start; i = BoundedModuloDec(i)) {
                        Segment *prev_entry = &m_segments[BoundedModuloDec(i).value()];
                        if (AMBRO_LIKELY(prev_entry->type == 0)) {
                            entry->lp_seg.max_start_v = TupleForEachForwardAccRes(&m_axes, entry->lp_seg.max_start_v, Foreach_compute_segment_buffer_cornering_speed(), entry, distance_rec, prev_entry);
                            break;
                        }
                    }
                    m_last_distance_rec = distance_rec;
                    if (!(m_split_buffer.split_pos < m_split_buffer.split_count)) {
                        m_split_buffer.type = 0xFF;
                    }
                } else {
                    entry->lp_seg.a_x = 0.0;
                    entry->lp_seg.max_v = INFINITY;
                    entry->lp_seg.max_start_v = INFINITY;
                    entry->lp_seg.a_x_rec = INFINITY;
                    entry->lp_seg.two_max_v_minus_a_x = INFINITY;
                    TupleForOneOffset<1>(entry->type, &m_channels, Foreach_write_segment(), entry);
                    m_split_buffer.type = 0xFF;
                }
                m_segments_end = BoundedModuloInc(m_segments_end);
            } while (m_split_buffer.type != 0xFF);
            
            if (AMBRO_LIKELY(!m_underrun && m_segments_staging_end != m_segments_end)) {
                plan(c);
            }
            
            if (m_split_buffer.type == 0xFF) {
                m_pull_finished_event.prependNowNotAlready(c);
                return;
            }
            if (AMBRO_UNLIKELY(m_underrun)) {
                return;
            }
            
            Segment *entry = &m_segments[m_segments_start.value()];
            if (entry->type == 0) {
                if (AMBRO_UNLIKELY(!m_stepping)) {
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
            } else {
                if (!TupleForOneOffset<1, bool>(entry->type, &m_channels, Foreach_commit_segment(), c, entry)) {
                    return;
                }
            }
            m_segments_start = BoundedModuloInc(m_segments_start);
            m_staging_time += m_first_segment_time_duration;
            m_staging_v_squared = m_first_segment_end_speed_squared;
        }
    }
    
    void plan (Context c)
    {
        AMBRO_ASSERT(m_segments_staging_end != m_segments_end)
        
#ifdef MOTIONPLANNER_BENCHMARK
        TimeType bench_start;
        if (NumAxes > 1) {
            bench_start = c.clock()->getTime(c);
        }
#endif
        
        SegmentBufferSizeType count = BoundedModuloSubtract(m_segments_end, m_segments_start);
        LinearPlannerSegmentState state[LookaheadBufferSize];
        
        SegmentBufferSizeType i = BoundedUnsafeDec(count);
        double v = 0.0;
        while (1) {
            Segment *entry = &m_segments[BoundedModuloAdd(m_segments_start, i).value()];
            v = LinearPlannerPush(&entry->lp_seg, &state[i.value()], v);
            if (AMBRO_UNLIKELY(i.value() == 0)) {
                break;
            }
            i = BoundedUnsafeDec(i);
        }
        
        i = SegmentBufferSizeType::import(0);
        v = m_staging_v_squared;
        double v_start = sqrt(m_staging_v_squared);
        TimeType time = m_staging_time;
        do {
            Segment *entry = &m_segments[BoundedModuloAdd(m_segments_start, i).value()];
            LinearPlannerSegmentResult result;
            v = LinearPlannerPull(&entry->lp_seg, &state[i.value()], v, &result);
            TimeType time_duration;
            if (entry->type == 0) {
                double v_end = sqrt(v);
                double v_const = sqrt(result.const_v);
                double t0_double = (v_const - v_start) * entry->max_accel_rec;
                double t2_double = (v_const - v_end) * entry->max_accel_rec;
                double t1_double = (1.0 - result.const_start - result.const_end) * entry->rel_max_speed_rec;
                MinTimeType t1 = MinTimeType::importDoubleSaturatedRound(t0_double + t2_double + t1_double);
                time_duration = t1.bitsValue();
                time += t1.bitsValue();
                MinTimeType t0 = FixedMin(t1, MinTimeType::importDoubleSaturatedRound(t0_double));
                t1.m_bits.m_int -= t0.bitsValue();
                MinTimeType t2 = FixedMin(t1, MinTimeType::importDoubleSaturatedRound(t2_double));
                t1.m_bits.m_int -= t2.bitsValue();
                TupleForEachForward(&m_axes, Foreach_gen_segment_stepper_commands(), c, entry,
                                    result.const_start, result.const_end, t0, t2, t1,
                                    t0_double * t0_double, t2_double * t2_double, i == SegmentBufferSizeType::import(0));
                v_start = v_end;
            } else {
                time_duration = 0;
                TupleForOneOffset<1>(entry->type, &m_channels, Foreach_gen_command(), c, entry, time);
            }
            if (AMBRO_UNLIKELY(i == SegmentBufferSizeType::import(0))) {
                m_first_segment_end_speed_squared = v;
                m_first_segment_time_duration = time_duration;
            }
            i = BoundedUnsafeInc(i);
        } while (i != count);
        
        TupleForEachForward(&m_axes, Foreach_complete_new());
        TupleForEachForward(&m_channels, Foreach_complete_new());
        if (AMBRO_UNLIKELY(!m_stepping)) {
            TupleForEachForward(&m_axes, Foreach_swap_staging_cold());
            TupleForEachForward(&m_channels, Foreach_swap_staging_cold());
            m_segments_staging_end = m_segments_end;
        } else {
            StepperCommandSizeType axes_old_first[NumAxes];
            ChannelCommandSizeTypeTuple channels_old_first;
            TupleForEachForward(&m_axes, Foreach_swap_staging_prepare(), axes_old_first);
            TupleForEachForward(&m_channels, Foreach_swap_staging_prepare(), &channels_old_first);
            AMBRO_LOCK_T(m_lock, c, lock_c, {
                m_underrun = is_underrun();
                if (AMBRO_LIKELY(!m_underrun)) {
                    TupleForEachForward(&m_axes, Foreach_swap_staging_hot());
                    TupleForEachForward(&m_channels, Foreach_swap_staging_hot(), lock_c);
                }
            });
            if (AMBRO_LIKELY(!m_underrun)) {
                TupleForEachForward(&m_axes, Foreach_swap_staging_finish(), axes_old_first);
                TupleForEachForward(&m_channels, Foreach_swap_staging_finish(), &channels_old_first);
                m_segments_staging_end = m_segments_end;
            }
            TupleForEachForward(&m_axes, Foreach_dispose_new(), c);
            TupleForEachForward(&m_channels, Foreach_dispose_new(), c);
        }
        
#ifdef MOTIONPLANNER_BENCHMARK
        if (NumAxes > 1) {
            m_bench_time += (TimeType)(c.clock()->getTime(c) - bench_start);
        }
#endif
    }
    
    void start_stepping (Context c)
    {
        AMBRO_ASSERT(!m_stepping)
        AMBRO_ASSERT(!m_underrun)
        AMBRO_ASSERT(m_segments_staging_end == m_segments_end)
        
#ifdef MOTIONPLANNER_BENCHMARK
        if (NumAxes > 1) {
            printf("elapsed %" PRIu32 "\n", m_bench_time);
        }
#endif
        
        m_stepping = true;
        TimeType start_time = c.clock()->getTime(c);
        m_staging_time += start_time;
        TupleForEachForward(&m_axes, Foreach_start_stepping(), c, start_time);
        TupleForEachForward(&m_channels, Foreach_start_stepping(), c, start_time);
    }
    
    void continue_wait (Context c)
    {
        AMBRO_ASSERT(!m_stepping)
        AMBRO_ASSERT(m_waiting)
        
        if (m_segments_start == m_segments_end) {
            m_pull_finished_event.prependNowNotAlready(c);
        } else {
            m_underrun = false;
            if (m_segments_staging_end != m_segments_end) {
                plan(c);
            }
            start_stepping(c);
        }
    }
    
    void pull_finished_event_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_split_buffer.type == 0xFF)
        
        if (AMBRO_UNLIKELY(m_waiting)) {
            AMBRO_ASSERT(m_pulling)
            AMBRO_ASSERT(!m_stepping)
            AMBRO_ASSERT(m_segments_start == m_segments_end)
            AMBRO_ASSERT(is_empty())
            
            m_waiting = false;
            return FinishedHandler::call(this, c);
        } else {
            AMBRO_ASSERT(!m_pulling)
            
#ifdef AMBROLIB_ASSERTIONS
            m_pulling = true;
#endif
            return PullHandler::call(this, c);
        }
    }
    
    bool is_empty ()
    {
        return
            TupleForEachForwardAccRes(&m_axes, true, Foreach_is_empty()) &&
            TupleForEachForwardAccRes(&m_channels, true, Foreach_is_empty());
    }
    
    __attribute__((always_inline)) inline uint8_t is_underrun ()
    {
        return
            TupleForEachForwardAccRes(&m_axes, false, Foreach_is_underrun()) ||
            TupleForEachForwardAccRes(&m_channels, false, Foreach_is_underrun());
    }
    
    void stepper_event_handler (Context c)
    {
        AMBRO_ASSERT(m_stepping)
        
        bool empty;
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            m_underrun = is_underrun();
            empty = is_empty();
        });
        
        if (AMBRO_UNLIKELY(empty)) {
            AMBRO_ASSERT(m_underrun)
            if (TupleForEachForwardAccRes(&m_axes, false, Foreach_is_aborted())) {
                TupleForEachForward(&m_axes, Foreach_reset_aborted());
                m_segments_staging_end = m_segments_end;
                if (AMBRO_LIKELY(m_split_buffer.type != 0xFF)) {
                    m_split_buffer.type = 0xFF;
                    m_pull_finished_event.prependNowNotAlready(c);
                }
            }
            m_stepping = false;
            m_segments_start = m_segments_staging_end;
            m_staging_time = 0;
            m_staging_v_squared = 0.0;
            m_stepper_event.unset(c);
            TupleForEachForward(&m_axes, Foreach_stopped_stepping(), c);
            TupleForEachForward(&m_channels, Foreach_stopped_stepping(), c);
            if (m_waiting) {
                return continue_wait(c);
            }
        }
        
        if (m_split_buffer.type != 0xFF) {
            work(c);
        }
    }
    
    typename Context::Lock m_lock;
    typename Loop::QueuedEvent m_pull_finished_event;
    typename Loop::QueuedEvent m_stepper_event;
    SegmentBufferSizeType m_segments_start;
    SegmentBufferSizeType m_segments_staging_end;
    SegmentBufferSizeType m_segments_end;
    TimeType m_staging_time;
    double m_staging_v_squared;
    double m_first_segment_end_speed_squared;
    TimeType m_first_segment_time_duration;
    double m_last_distance_rec;
    bool m_stepping;
    bool m_underrun;
    bool m_waiting;
#ifdef AMBROLIB_ASSERTIONS
    bool m_pulling;
#endif
    SplitBuffer m_split_buffer;
    Segment m_segments[(size_t)SegmentBufferSizeType::maxIntValue() + 1];
    AxesTuple m_axes;
    ChannelsTuple m_channels;
#ifdef MOTIONPLANNER_BENCHMARK
    TimeType m_bench_time;
#endif
    
    template <int AxisIndex> struct AxisPosition : public TuplePosition<Position, AxesTuple, &MotionPlanner::m_axes, AxisIndex> {};
};

#include <aprinter/EndNamespace.h>

#endif
