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
#include <aprinter/meta/PowerOfTwo.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/Union.h>
#include <aprinter/meta/UnionGet.h>
#include <aprinter/meta/IndexElemUnion.h>
#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/OffsetCallback.h>
#include <aprinter/base/Likely.h>
#include <aprinter/base/GetContainer.h>
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
    typename PullHandler, typename FinishedHandler, typename AbortedHandler, typename ChannelsList = EmptyTypeList
>
class MotionPlanner
: private DebugObject<Context, void>
{
private:
    template <int AxisIndex> struct AxisPosition;
    template <int ChannelIndex> struct ChannelPosition;
    
    static_assert(StepperSegmentBufferSize >= 6, "");
    static_assert(LookaheadBufferSize >= 2, "");
    using Loop = typename Context::EventLoop;
    using TimeType = typename Context::Clock::TimeType;
    static const int NumAxes = TypeListLength<AxesList>::value;
    template <typename AxisSpec, typename AccumType>
    using MinTimeTypeHelper = FixedIntersectTypes<typename AxisSpec::TheAxisStepper::TimeFixedType, AccumType>;
    using MinTimeType = TypeListFold<AxesList, FixedIdentity, MinTimeTypeHelper>;
    using SegmentBufferSizeType = typename ChooseInt<BitsInInt<LookaheadBufferSize>::value, false>::Type;
    static const size_t NumStepperCommands = 3 * (StepperSegmentBufferSize + 2 * LookaheadBufferSize);
    using StepperCommandSizeType = typename ChooseInt<BitsInInt<NumStepperCommands>::value, true>::Type;
    using UnsignedStepperCommandSizeType = typename ChooseInt<BitsInInt<NumStepperCommands>::value, false>::Type;
    using StepperFastEvent = typename Context::EventLoop::template FastEventSpec<MotionPlanner>;
    
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
    
    enum {STATE_BUFFERING, STATE_STEPPING, STATE_ABORTED};
    
public:
    template <int AxisIndex>
    class Axis {
    public:
        using AxisSpec = TypeListGet<AxesList, AxisIndex>;
        using TheAxisStepper = typename AxisSpec::TheAxisStepper;
        using StepFixedType = FixedPoint<AxisSpec::StepBits, false, 0>;
        using TheAxisInputCommand = AxisInputCommand<AxisIndex>;
        using StepperCommandCallbackContext = typename TheAxisStepper::CommandCallbackContext;
        
    public: // private, workaround gcc bug
        friend MotionPlanner;
        
        using StepperStepFixedType = typename TheAxisStepper::StepFixedType;
        using StepperTimeFixedType = typename TheAxisStepper::TimeFixedType;
        using StepperAccelFixedType = typename TheAxisStepper::AccelFixedType;
        using StepperCommand = typename TheAxisStepper::Command;
        using TheAxisSplitBuffer = AxisSplitBuffer<AxisIndex>;
        using TheAxisSegment = AxisSegment<AxisIndex>;
        using TheAxisStepperCommand = AxisStepperCommand<AxisIndex>;
        static const bool PrestepCallbackEnabled = !TypesAreEqual<typename AxisSpec::PrestepCallback, void>::value;
        
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
            o->m_first = -1;
            o->m_free_first = -1;
            o->m_new_first = -1;
            o->m_num_committed = 0;
            o->m_last_committed = 0;
            for (size_t i = 0; i < NumStepperCommands; i++) {
                index_command(c, i)->next = o->m_free_first;
                o->m_free_first = i;
            }
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
        
        static void commandDone_assert (InputCommand *icmd)
        {
            TheAxisInputCommand *axis_icmd = TupleGetElem<AxisIndex>(&icmd->axes);
            AMBRO_ASSERT(FloatIsPosOrPosZero(axis_icmd->max_v_rec))
            AMBRO_ASSERT(FloatIsPosOrPosZero(axis_icmd->max_a_rec))
        }
        
        static void write_splitbuf (Context c, InputCommand *icmd)
        {
            MotionPlanner *m = MotionPlanner::self(c);
            TheAxisInputCommand *axis_icmd = TupleGetElem<AxisIndex>(&icmd->axes);
            TheAxisSplitBuffer *axis_split = TupleGetElem<AxisIndex>(&m->m_split_buffer.axes);
            axis_split->dir = axis_icmd->dir;
            axis_split->x = axis_icmd->x;
            axis_split->max_v_rec = axis_icmd->max_v_rec;
            axis_split->max_a_rec = axis_icmd->max_a_rec;
            axis_split->x_pos = StepFixedType::importBits(0);
        }
        
        static bool splitbuf_fits (bool accum, Context c)
        {
            MotionPlanner *m = MotionPlanner::self(c);
            TheAxisSplitBuffer *axis_split = TupleGetElem<AxisIndex>(&m->m_split_buffer.axes);
            return (accum && axis_split->x <= StepperStepFixedType::maxValue());
        }
        
        static double compute_split_count (double accum, Context c)
        {
            MotionPlanner *m = MotionPlanner::self(c);
            TheAxisSplitBuffer *axis_split = TupleGetElem<AxisIndex>(&m->m_split_buffer.axes);
            return fmax(accum, axis_split->x.doubleValue() * (1.0001 / StepperStepFixedType::maxValue().doubleValue()));
        }
        
        static bool check_icmd_zero (bool accum, InputCommand *icmd)
        {
            TheAxisInputCommand *axis_icmd = TupleGetElem<AxisIndex>(&icmd->axes);
            return (accum && axis_icmd->x.bitsValue() == 0);
        }
        
        static void write_segment_buffer_entry (Context c, Segment *entry)
        {
            MotionPlanner *m = MotionPlanner::self(c);
            TheAxisSplitBuffer *axis_split = TupleGetElem<AxisIndex>(&m->m_split_buffer.axes);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            StepFixedType new_x;
            if (m->m_split_buffer.split_pos == m->m_split_buffer.split_count) {
                new_x = axis_split->x;
            } else {
                new_x = FixedMin(axis_split->x, StepFixedType::importDoubleSaturatedRound(m->m_split_buffer.split_pos * m->m_split_buffer.split_frac * axis_split->x.doubleValue()));
            }
            axis_entry->dir = axis_split->dir;
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
            MotionPlanner *m = MotionPlanner::self(c);
            TheAxisSplitBuffer *axis_split = TupleGetElem<AxisIndex>(&m->m_split_buffer.axes);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            return fmax(accum, axis_entry->x.doubleValue() * axis_split->max_v_rec);
        }
        
        static double compute_segment_buffer_entry_accel (double accum, Context c, Segment *entry)
        {
            MotionPlanner *m = MotionPlanner::self(c);
            TheAxisSplitBuffer *axis_split = TupleGetElem<AxisIndex>(&m->m_split_buffer.axes);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            return fmax(accum, axis_entry->x.doubleValue() * axis_split->max_a_rec);
        }
        
        static double write_segment_buffer_entry_extra (Segment *entry, double rel_max_accel)
        {
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            axis_entry->half_accel = 0.5 * rel_max_accel * axis_entry->x.doubleValue();
        }
        
        static double compute_segment_buffer_cornering_speed (double accum, Context c, Segment *entry, double entry_distance_rec, Segment *prev_entry)
        {
            MotionPlanner *m = MotionPlanner::self(c);
            TheAxisSplitBuffer *axis_split = TupleGetElem<AxisIndex>(&m->m_split_buffer.axes);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            TheAxisSegment *prev_axis_entry = TupleGetElem<AxisIndex>(&prev_entry->axes);
            double m1 = axis_entry->x.doubleValue() * entry_distance_rec;
            double m2 = prev_axis_entry->x.doubleValue() * m->m_last_distance_rec;
            bool dir_changed = axis_entry->dir != prev_axis_entry->dir;
            double dm = (dir_changed ? (m1 + m2) : fabs(m1 - m2));
            return fmin(accum, (AxisSpec::CorneringDistance::value() * AxisSpec::DistanceFactor::value()) / (dm * axis_split->max_a_rec));
        }
        
        static void gen_segment_stepper_commands (Context c, Segment *entry, double frac_x0, double frac_x2, MinTimeType t0, MinTimeType t2, MinTimeType t1, double t0_squared, double t2_squared, bool is_first)
        {
            Axis *o = self(c);
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
                o->m_first_segment_num_stepper_entries = num_stepper_entries;
                o->m_first_segment_last_stepper_entry = o->m_new_last;
            }
        }
        
        static void gen_stepper_command (Context c, TheAxisSegment *axis_entry, StepperStepFixedType x, StepperTimeFixedType t, StepperAccelFixedType a)
        {
            Axis *o = self(c);
            StepperCommandSizeType entry = o->m_free_first;
            o->m_free_first = index_command(c, o->m_free_first)->next;
            TheAxisStepper::generate_command(axis_entry->dir, x, t, a, &index_command(c, entry)->scmd);
            if (!(o->m_new_first >= 0)) {
                o->m_new_first = entry;
            }
            o->m_new_last = entry;
        }
        
        static void complete_new (Context c)
        {
            Axis *o = self(c);
            if (o->m_new_first >= 0) {
                index_command(c, o->m_new_last)->next = -1;
            }
        }
        
        static bool have_commit_space (bool accum, Context c)
        {
            Axis *o = self(c);
            return (accum && o->m_num_committed <= 3 * (StepperSegmentBufferSize - 1));
        }
        
        static void commit_segment_hot (Context c, Segment *entry)
        {
            Axis *o = self(c);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            AMBRO_ASSERT(o->m_num_committed <= 3 * StepperSegmentBufferSize - o->m_first_segment_num_stepper_entries)
            o->m_num_committed += o->m_first_segment_num_stepper_entries;
        }
        
        static void commit_segment_finish (Context c, Segment *entry)
        {
            Axis *o = self(c);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&entry->axes);
            o->m_last_committed = o->m_first_segment_last_stepper_entry;
        }
        
        static void dispose_new (Context c)
        {
            Axis *o = self(c);
            if (o->m_new_first >= 0) {
                index_command(c, o->m_new_last)->next = o->m_free_first;
                o->m_free_first = o->m_new_first;
                o->m_new_first = -1;
            }
        }
        
        static void swap_staging_cold (Context c)
        {
            Axis *o = self(c);
            if (!(o->m_new_first >= 0)) {
                return;
            }
            if (index_command(c, o->m_last_committed)->next >= 0) {
                index_command(c, o->m_last)->next = o->m_free_first;
                o->m_free_first = index_command(c, o->m_last_committed)->next;
            }
            index_command(c, o->m_last_committed)->next = o->m_new_first;
            if (o->m_num_committed == 0) {
                o->m_first = o->m_new_first;
            }
            o->m_last = o->m_new_last;
            o->m_new_first = -1;
        }
        
        static void swap_staging_prepare (Context c, StepperCommandSizeType *old_first)
        {
            Axis *o = self(c);
            
            old_first[AxisIndex] = index_command(c, o->m_last_committed)->next;
        }
        
        static void swap_staging_hot (Context c)
        {
            Axis *o = self(c);
            if (AMBRO_LIKELY(o->m_new_first >= 0)) {
                index_command(c, o->m_last_committed)->next = o->m_new_first;
            }
        }
        
        static void swap_staging_finish (Context c, StepperCommandSizeType *old_first)
        {
            Axis *o = self(c);
            if (o->m_new_first >= 0) {
                StepperCommandSizeType old_last = o->m_last;
                o->m_last = o->m_new_last;
                o->m_new_first = old_first[AxisIndex];
                o->m_new_last = old_last;
            }
        }
        
        static void start_stepping (Context c, TimeType start_time)
        {
            Axis *o = self(c);
            if (!(o->m_first >= 0)) {
                return;
            }
            stepper(c)->template start<TheAxisStepperConsumer<AxisIndex>>(c, start_time, &index_command(c, o->m_first)->scmd);
        }
        
        static bool is_empty (bool accum, Context c)
        {
            Axis *o = self(c);
            return (accum && !(o->m_first >= 0));
        }
        
        static bool is_underrun (bool accum, Context c)
        {
            Axis *o = self(c);
            return (accum || (o->m_num_committed <= 0));
        }
        
        static bool is_aborted (bool accum, Context c)
        {
            Axis *o = self(c);
            return (accum || (PrestepCallbackEnabled && o->m_first <= -2));
        }
        
        static void reset_aborted (Context c)
        {
            Axis *o = self(c);
            o->m_first = -1;
        }
        
        static void stopped_stepping (Context c)
        {
            Axis *o = self(c);
            o->m_num_committed = 0;
        }
        
        static bool stepper_command_callback (StepperCommandCallbackContext c, StepperCommand *prev_cmd, StepperCommand **out_cmd)
        {
            Axis *o = self(c);
            MotionPlanner *m = MotionPlanner::self(c);
            AMBRO_ASSERT(o->m_first >= 0)
            AMBRO_ASSERT(m->m_state == STATE_STEPPING)
            AMBRO_ASSERT(prev_cmd == &index_command(c, o->m_first)->scmd)
            
            TheAxisStepperCommand *cmd = GetContainer(prev_cmd, &TheAxisStepperCommand::scmd);
            c.eventLoop()->template triggerFastEvent<StepperFastEvent>(c);
            o->m_num_committed--;
            o->m_first = cmd->next;
            if (!(o->m_first >= 0)) {
                return false;
            }
            *out_cmd = &index_command(c, o->m_first)->scmd;
            return true;
        }
        
        static bool stepper_prestep_callback (StepperCommandCallbackContext c)
        {
            return PrestepCallbackHelper<PrestepCallbackEnabled>::call(c);
        }
        
        AMBRO_ALWAYS_INLINE static TheAxisStepperCommand * index_command (Context c, UnsignedStepperCommandSizeType i)
        {
            Axis *o = self(c);
            return (TheAxisStepperCommand *)((char *)o->m_stepper_entries + i * sizeof(TheAxisStepperCommand));
        }
        
        template <bool Enabled, typename Dummy = void>
        struct PrestepCallbackHelper {
            static bool call (StepperCommandCallbackContext c)
            {
                return false;
            }
        };
        
        template <typename Dummy>
        struct PrestepCallbackHelper<true, Dummy> {
            static bool call (StepperCommandCallbackContext c)
            {
                Axis *o = self(c);
                bool res = AxisSpec::PrestepCallback::call(c);
                if (AMBRO_UNLIKELY(res)) {
                    c.eventLoop()->template triggerFastEvent<StepperFastEvent>(c);
                    o->m_num_committed = 0;
                    o->m_first = -2 - o->m_first;
                }
                return res;
            }
        };
        
        template <typename StepsType>
        static StepsType axis_count_aborted_rem_steps (Context c)
        {
            Axis *o = self(c);
            MotionPlanner *m = MotionPlanner::self(c);
            
            StepsType steps = 0;
            if (o->m_first != -1) {
                bool dir;
                StepperStepFixedType cmd_steps = stepper(c)->getAbortedCmdSteps(c, &dir);
                if (dir) {
                    steps += (StepsType)cmd_steps.bitsValue();
                } else {
                    steps -= (StepsType)cmd_steps.bitsValue();
                }
                StepperCommandSizeType first = o->m_first;
                if (first < 0) {
                    first = -(o->m_first + 2);
                }
                StepperCommandSizeType i = index_command(c, first)->next;
                while (i != -1) {
                    TheAxisStepperCommand *cmd = index_command(c, i);
                    cmd_steps = stepper(c)->getPendingCmdSteps(c, &cmd->scmd, &dir);
                    if (dir) {
                        steps += (StepsType)cmd_steps.bitsValue();
                    } else {
                        steps -= (StepsType)cmd_steps.bitsValue();
                    }
                    i = cmd->next;
                }
            }
            for (SegmentBufferSizeType i = m->m_segments_staging_length; i < m->m_segments_length; i++) {
                Segment *seg = &m->m_segments[segments_add(m->m_segments_start, i)];
                TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(&seg->axes);
                if (axis_entry->dir) {
                    steps += (StepsType)axis_entry->x.bitsValue();
                } else {
                    steps -= (StepsType)axis_entry->x.bitsValue();
                }
            }
            if (m->m_split_buffer.type == 0) {
                TheAxisSplitBuffer *axis_split = TupleGetElem<AxisIndex>(&m->m_split_buffer.axes);
                StepFixedType x = StepFixedType::importBits(axis_split->x.bitsValue() - axis_split->x_pos.bitsValue());
                if (axis_split->dir) {
                    steps += (StepsType)x.bitsValue();
                } else {
                    steps -= (StepsType)x.bitsValue();
                }
            }
            return steps;
        }
        
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
        static_assert(ChannelSpec::BufferSize > 0, "");
        static const size_t NumChannelCommands = TheChannelCommand::NumChannelCommands;
        using ChannelCommandSizeType = typename TheChannelCommand::ChannelCommandSizeType;
        
        static Channel * self (Context c)
        {
            return PositionTraverse<typename Context::TheRootPosition, ChannelPosition<ChannelIndex>>(c.root());
        }
        
        static void init (Context c)
        {
            Channel *o = self(c);
            o->m_first = -1;
            o->m_free_first = -1;
            o->m_new_first = -1;
            o->m_num_committed = 0;
            o->m_last_committed = 0;
            for (size_t i = 0; i < NumChannelCommands; i++) {
                o->m_channel_commands[i].next = o->m_free_first;
                o->m_free_first = i;
            }
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
        
        static void gen_command (Context c, Segment *entry, TimeType time)
        {
            Channel *o = self(c);
            TheChannelSegment *channel_entry = UnionGetElem<ChannelIndex>(&entry->channels);
            
            ChannelCommandSizeType cmd = o->m_free_first;
            o->m_free_first = o->m_channel_commands[o->m_free_first].next;
            o->m_channel_commands[cmd].payload = channel_entry->payload;
            o->m_channel_commands[cmd].time = time;
            if (!(o->m_new_first >= 0)) {
                o->m_new_first = cmd;
            }
            o->m_new_last = cmd;
            channel_entry->command = cmd;
        }
        
        static void complete_new (Context c)
        {
            Channel *o = self(c);
            if (o->m_new_first >= 0) {
                o->m_channel_commands[o->m_new_last].next = -1;
            }
        }
        
        static void dispose_new (Context c)
        {
            Channel *o = self(c);
            if (o->m_new_first >= 0) {
                o->m_channel_commands[o->m_new_last].next = o->m_free_first;
                o->m_free_first = o->m_new_first;
                o->m_new_first = -1;
            }
        }
        
        static void swap_staging_cold (Context c)
        {
            Channel *o = self(c);
            if (!(o->m_new_first >= 0)) {
                return;
            }
            if (o->m_channel_commands[o->m_last_committed].next >= 0) {
                o->m_channel_commands[o->m_last].next = o->m_free_first;
                o->m_free_first = o->m_channel_commands[o->m_last_committed].next;
            }
            o->m_channel_commands[o->m_last_committed].next = o->m_new_first;
            if (o->m_num_committed == 0) {
                o->m_first = o->m_new_first;
            }
            o->m_last = o->m_new_last;
            o->m_new_first = -1;
        }
        
        static void swap_staging_prepare (Context c, ChannelCommandSizeTypeTuple *old_first_tuple)
        {
            Channel *o = self(c);
            
            *TupleGetElem<ChannelIndex>(old_first_tuple) = o->m_channel_commands[o->m_last_committed].next;
        }
        
        template <typename LockContext>
        static void swap_staging_hot (LockContext c)
        {
            Channel *o = self(c);
            if (AMBRO_LIKELY(o->m_new_first >= 0)) {
                o->m_channel_commands[o->m_last_committed].next = o->m_new_first;
                if (AMBRO_LIKELY(o->m_num_committed == 0)) {
                    o->m_first = o->m_new_first;
                    o->m_timer.unset(c);
                    o->m_timer.setFirst(c, o->m_channel_commands[o->m_first].time);
                }
            }
        }
        
        static void swap_staging_finish (Context c, ChannelCommandSizeTypeTuple *old_first_tuple)
        {
            Channel *o = self(c);
            if (o->m_new_first >= 0) {
                ChannelCommandSizeType old_last = o->m_last;
                o->m_last = o->m_new_last;
                o->m_new_first = *TupleGetElem<ChannelIndex>(old_first_tuple);
                o->m_new_last = old_last;
            }
        }
        
        static void start_stepping (Context c, TimeType start_time)
        {
            Channel *o = self(c);
            if (!(o->m_first >= 0)) {
                return;
            }
            for (ChannelCommandSizeType cmd = o->m_first; cmd >= 0; cmd = o->m_channel_commands[cmd].next) {
                o->m_channel_commands[cmd].time += start_time;
            }
            o->m_timer.setFirst(c, o->m_channel_commands[o->m_first].time);
        }
        
        static bool commit_segment (Context c, Segment *entry)
        {
            Channel *o = self(c);
            MotionPlanner *m = MotionPlanner::self(c);
            TheChannelSegment *channel_entry = UnionGetElem<ChannelIndex>(&entry->channels);
            
            if (m->m_state == STATE_BUFFERING) {
                if (o->m_num_committed == ChannelSpec::BufferSize) {
                    planner_start_stepping(c);
                    return false;
                }
                o->m_num_committed++;
            } else {
                bool cleared = false;
                AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                    m->m_underrun = planner_is_underrun(c);
                    if (!m->m_underrun && o->m_num_committed != ChannelSpec::BufferSize) {
                        o->m_num_committed++;
                        cleared = true;
                    }
                }
                if (!cleared) {
                    return false;
                }
            }
            o->m_last_committed = channel_entry->command;
            return true;
        }
        
        static bool is_empty (bool accum, Context c)
        {
            Channel *o = self(c);
            return (accum && !(o->m_first >= 0));
        }
        
        static bool is_underrun (bool accum, Context c)
        {
            Channel *o = self(c);
            return (accum || (o->m_num_committed < 0));
        }
        
        static void reset_aborted (Context c)
        {
            Channel *o = self(c);
            o->m_first = -1;
        }
        
        static void stopped_stepping (Context c)
        {
            Channel *o = self(c);
            o->m_num_committed = 0;
        }
        
        static bool timer_handler (TheTimer *, typename TheTimer::HandlerContext c)
        {
            Channel *o = self(c);
            MotionPlanner *m = MotionPlanner::self(c);
            AMBRO_ASSERT(o->m_first >= 0)
            AMBRO_ASSERT(m->m_state == STATE_STEPPING)
            
            ChannelSpec::Callback::call(c, &o->m_channel_commands[o->m_first].payload);
            
            c.eventLoop()->template triggerFastEvent<StepperFastEvent>(c);
            o->m_num_committed--;
            o->m_first = o->m_channel_commands[o->m_first].next;
            if (!(o->m_first >= 0)) {
                return false;
            }
            o->m_timer.setNext(c, o->m_channel_commands[o->m_first].time);
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
        o->m_underrun = true;
        o->m_waiting = false;
#ifdef AMBROLIB_ASSERTIONS
        o->m_pulling = false;
#endif
        TupleForEachForward(&o->m_axes, Foreach_init(), c, prestep_callback_enabled);
        TupleForEachForward(&o->m_channels, Foreach_init(), c);
        o->m_pull_finished_event.prependNowNotAlready(c);
#ifdef MOTIONPLANNER_BENCHMARK
        if (NumAxes > 1) {
            o->m_bench_time = 0;
        }
#endif
    }
    
    static void deinit (Context c)
    {
        MotionPlanner *o = self(c);
        
        TupleForEachForward(&o->m_channels, Foreach_deinit(), c);
        TupleForEachForward(&o->m_axes, Foreach_deinit(), c);
        c.eventLoop()->template resetFastEvent<StepperFastEvent>(c);
        o->m_pull_finished_event.deinit(c);
    }
    
    static void commandDone (Context c, InputCommand *icmd)
    {
        MotionPlanner *o = self(c);
        AMBRO_ASSERT(o->m_state != STATE_ABORTED)
        AMBRO_ASSERT(o->m_pulling)
        AMBRO_ASSERT(o->m_split_buffer.type == 0xFF)
        if (icmd->type == 0) {
            AMBRO_ASSERT(FloatIsPosOrPosZero(icmd->rel_max_v_rec))
            TupleForEachForward(&o->m_axes, Foreach_commandDone_assert(), icmd);
        }
        
        o->m_waiting = false;
        o->m_pull_finished_event.unset(c);
#ifdef AMBROLIB_ASSERTIONS
        o->m_pulling = false;
#endif
        
        o->m_split_buffer.type = icmd->type;
        if (o->m_split_buffer.type == 0) {
            if (TupleForEachForwardAccRes(&o->m_axes, true, Foreach_check_icmd_zero(), icmd)) {
                o->m_split_buffer.type = 0xFF;
                o->m_pull_finished_event.prependNowNotAlready(c);
                return;
            }
            TupleForEachForward(&o->m_axes, Foreach_write_splitbuf(), c, icmd);
            o->m_split_buffer.split_pos = 0;
            if (AMBRO_LIKELY(TupleForEachForwardAccRes(&o->m_axes, true, Foreach_splitbuf_fits(), c))) {
                o->m_split_buffer.base_max_v_rec = icmd->rel_max_v_rec;
                o->m_split_buffer.split_count = 1;
            } else {
                double split_count = ceil(TupleForEachForwardAccRes(&o->m_axes, 0.0, Foreach_compute_split_count(), c));
                o->m_split_buffer.split_frac = 1.0 / split_count;
                o->m_split_buffer.base_max_v_rec = icmd->rel_max_v_rec * o->m_split_buffer.split_frac;
                o->m_split_buffer.split_count = split_count;
            }
        } else {
            o->m_split_buffer.channel_payload = icmd->channel_payload;
        }
        
        work(c);
    }
    
    static void waitFinished (Context c)
    {
        MotionPlanner *o = self(c);
        AMBRO_ASSERT(o->m_state != STATE_ABORTED)
        AMBRO_ASSERT(o->m_pulling)
        AMBRO_ASSERT(o->m_split_buffer.type == 0xFF)
        
        if (!o->m_waiting) {
            o->m_waiting = true;
            if (o->m_state == STATE_BUFFERING) {
                continue_wait(c);
            }
        }
    }
    
    template <int AxisIndex, typename StepsType>
    static StepsType countAbortedRemSteps (Context c)
    {
        MotionPlanner *o = self(c);
        AMBRO_ASSERT(o->m_state == STATE_ABORTED)
        
        return Axis<AxisIndex>::template axis_count_aborted_rem_steps<StepsType>(c);
    }
    
    static void continueAfterAborted (Context c)
    {
        MotionPlanner *o = self(c);
        AMBRO_ASSERT(o->m_state == STATE_ABORTED)
        AMBRO_ASSERT(o->m_underrun)
        
        o->m_segments_start = 0;
        o->m_segments_staging_length = 0;
        o->m_segments_length = 0;
        o->m_staging_time = 0;
        o->m_staging_v_squared = 0.0;
        o->m_split_buffer.type = 0xFF;
        o->m_state = STATE_BUFFERING;
        o->m_waiting = false;
#ifdef AMBROLIB_ASSERTIONS
        o->m_pulling = false;
#endif
        TupleForEachForward(&o->m_axes, Foreach_reset_aborted(), c);
        TupleForEachForward(&o->m_channels, Foreach_reset_aborted(), c);
        TupleForEachForward(&o->m_axes, Foreach_stopped_stepping(), c);
        TupleForEachForward(&o->m_channels, Foreach_stopped_stepping(), c);
        o->m_pull_finished_event.prependNowNotAlready(c);
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
    
    static void work (Context c)
    {
        MotionPlanner *o = self(c);
        AMBRO_ASSERT(o->m_state != STATE_ABORTED)
        AMBRO_ASSERT(o->m_split_buffer.type != 0xFF)
        AMBRO_ASSERT(!o->m_pulling)
        AMBRO_ASSERT(o->m_split_buffer.type != 0 || o->m_split_buffer.split_pos < o->m_split_buffer.split_count)
        
        while (1) {
            do {
                if (o->m_segments_length == LookaheadBufferSize) {
                    o->m_underrun = o->m_underrun && (o->m_state == STATE_STEPPING);
                    break;
                }
                Segment *entry = &o->m_segments[segments_add(o->m_segments_start, o->m_segments_length)];
                entry->type = o->m_split_buffer.type;
                if (entry->type == 0) {
                    o->m_split_buffer.split_pos++;
                    TupleForEachForward(&o->m_axes, Foreach_write_segment_buffer_entry(), c, entry);
                    double distance_squared = TupleForEachForwardAccRes(&o->m_axes, 0.0, Foreach_compute_segment_buffer_entry_distance(), entry);
                    entry->rel_max_speed_rec = TupleForEachForwardAccRes(&o->m_axes, o->m_split_buffer.base_max_v_rec, Foreach_compute_segment_buffer_entry_speed(), c, entry);
                    double rel_max_accel_rec = TupleForEachForwardAccRes(&o->m_axes, 0.0, Foreach_compute_segment_buffer_entry_accel(), c, entry);
                    double distance = sqrt(distance_squared);
                    double rel_max_accel = 1.0 / rel_max_accel_rec;
                    entry->lp_seg.max_v = distance_squared / (entry->rel_max_speed_rec * entry->rel_max_speed_rec);
                    entry->lp_seg.max_start_v = entry->lp_seg.max_v;
                    entry->lp_seg.a_x = 2 * rel_max_accel * distance_squared;
                    entry->lp_seg.a_x_rec = 1.0 / entry->lp_seg.a_x;
                    entry->lp_seg.two_max_v_minus_a_x = 2 * entry->lp_seg.max_v - entry->lp_seg.a_x;
                    entry->max_accel_rec = rel_max_accel_rec / distance;
                    TupleForEachForward(&o->m_axes, Foreach_write_segment_buffer_entry_extra(), entry, rel_max_accel);
                    double distance_rec = 1.0 / distance;
                    for (SegmentBufferSizeType i = o->m_segments_length; i > 0; i--) {
                        Segment *prev_entry = &o->m_segments[segments_add(o->m_segments_start, i - 1)];
                        if (AMBRO_LIKELY(prev_entry->type == 0)) {
                            entry->lp_seg.max_start_v = TupleForEachForwardAccRes(&o->m_axes, entry->lp_seg.max_start_v, Foreach_compute_segment_buffer_cornering_speed(), c, entry, distance_rec, prev_entry);
                            break;
                        }
                    }
                    o->m_last_distance_rec = distance_rec;
                    if (!(o->m_split_buffer.split_pos < o->m_split_buffer.split_count)) {
                        o->m_split_buffer.type = 0xFF;
                    }
                } else {
                    entry->lp_seg.a_x = 0.0;
                    entry->lp_seg.max_v = INFINITY;
                    entry->lp_seg.max_start_v = INFINITY;
                    entry->lp_seg.a_x_rec = INFINITY;
                    entry->lp_seg.two_max_v_minus_a_x = INFINITY;
                    TupleForOneOffset<1>(entry->type, &o->m_channels, Foreach_write_segment(), c, entry);
                    o->m_split_buffer.type = 0xFF;
                }
                o->m_segments_length++;
            } while (o->m_split_buffer.type != 0xFF);
            
            if (AMBRO_LIKELY(!o->m_underrun && o->m_segments_staging_length != o->m_segments_length)) {
                plan(c);
            }
            
            if (o->m_split_buffer.type == 0xFF) {
                o->m_pull_finished_event.prependNowNotAlready(c);
                return;
            }
            if (AMBRO_UNLIKELY(o->m_underrun)) {
                return;
            }
            
            Segment *entry = &o->m_segments[o->m_segments_start];
            if (entry->type == 0) {
                if (AMBRO_UNLIKELY(o->m_state == STATE_BUFFERING)) {
                    if (!TupleForEachForwardAccRes(&o->m_axes, true, Foreach_have_commit_space(), c)) {
                        planner_start_stepping(c);
                        return;
                    }
                    TupleForEachForward(&o->m_axes, Foreach_commit_segment_hot(), c, entry);
                } else {
                    bool cleared = false;
                    AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                        o->m_underrun = planner_is_underrun(c);
                        if (!o->m_underrun && TupleForEachForwardAccRes(&o->m_axes, true, Foreach_have_commit_space(), c)) {
                            TupleForEachForward(&o->m_axes, Foreach_commit_segment_hot(), c, entry);
                            cleared = true;
                        }
                    }
                    if (!cleared) {
                        return;
                    }
                }
                TupleForEachForward(&o->m_axes, Foreach_commit_segment_finish(), c, entry);
            } else {
                if (!TupleForOneOffset<1, bool>(entry->type, &o->m_channels, Foreach_commit_segment(), c, entry)) {
                    return;
                }
            }
            o->m_segments_start = segments_inc(o->m_segments_start);
            o->m_segments_length--;
            o->m_segments_staging_length--;
            o->m_staging_time += o->m_first_segment_time_duration;
            o->m_staging_v_squared = o->m_first_segment_end_speed_squared;
        }
    }
    
    static void plan (Context c)
    {
        MotionPlanner *o = self(c);
        AMBRO_ASSERT(o->m_state != STATE_ABORTED)
        AMBRO_ASSERT(o->m_segments_staging_length != o->m_segments_length)
        
#ifdef MOTIONPLANNER_BENCHMARK
        TimeType bench_start;
        if (NumAxes > 1) {
            bench_start = c.clock()->getTime(c);
        }
#endif
        
        LinearPlannerSegmentState state[LookaheadBufferSize];
        
        SegmentBufferSizeType i = o->m_segments_length - 1;
        double v = 0.0;
        while (1) {
            Segment *entry = &o->m_segments[segments_add(o->m_segments_start, i)];
            v = LinearPlannerPush(&entry->lp_seg, &state[i], v);
            if (AMBRO_UNLIKELY(i == 0)) {
                break;
            }
            i--;
        }
        
        i = 0;
        v = o->m_staging_v_squared;
        double v_start = sqrt(o->m_staging_v_squared);
        TimeType time = o->m_staging_time;
        do {
            Segment *entry = &o->m_segments[segments_add(o->m_segments_start, i)];
            LinearPlannerSegmentResult result;
            v = LinearPlannerPull(&entry->lp_seg, &state[i], v, &result);
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
                TupleForEachForward(&o->m_axes, Foreach_gen_segment_stepper_commands(), c, entry,
                                    result.const_start, result.const_end, t0, t2, t1,
                                    t0_double * t0_double, t2_double * t2_double, i == 0);
                v_start = v_end;
            } else {
                time_duration = 0;
                TupleForOneOffset<1>(entry->type, &o->m_channels, Foreach_gen_command(), c, entry, time);
            }
            if (AMBRO_UNLIKELY(i == 0)) {
                o->m_first_segment_end_speed_squared = v;
                o->m_first_segment_time_duration = time_duration;
            }
            i++;
        } while (i != o->m_segments_length);
        
        TupleForEachForward(&o->m_axes, Foreach_complete_new(), c);
        TupleForEachForward(&o->m_channels, Foreach_complete_new(), c);
        if (AMBRO_UNLIKELY(o->m_state == STATE_BUFFERING)) {
            TupleForEachForward(&o->m_axes, Foreach_swap_staging_cold(), c);
            TupleForEachForward(&o->m_channels, Foreach_swap_staging_cold(), c);
            o->m_segments_staging_length = o->m_segments_length;
        } else {
            StepperCommandSizeType axes_old_first[NumAxes];
            ChannelCommandSizeTypeTuple channels_old_first;
            TupleForEachForward(&o->m_axes, Foreach_swap_staging_prepare(), c, axes_old_first);
            TupleForEachForward(&o->m_channels, Foreach_swap_staging_prepare(), c, &channels_old_first);
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                o->m_underrun = planner_is_underrun(c);
                if (AMBRO_LIKELY(!o->m_underrun)) {
                    TupleForEachForward(&o->m_axes, Foreach_swap_staging_hot(), c);
                    TupleForEachForward(&o->m_channels, Foreach_swap_staging_hot(), lock_c);
                }
            }
            if (AMBRO_LIKELY(!o->m_underrun)) {
                TupleForEachForward(&o->m_axes, Foreach_swap_staging_finish(), c, axes_old_first);
                TupleForEachForward(&o->m_channels, Foreach_swap_staging_finish(), c, &channels_old_first);
                o->m_segments_staging_length = o->m_segments_length;
            }
            TupleForEachForward(&o->m_axes, Foreach_dispose_new(), c);
            TupleForEachForward(&o->m_channels, Foreach_dispose_new(), c);
        }
        
#ifdef MOTIONPLANNER_BENCHMARK
        if (NumAxes > 1) {
            o->m_bench_time += (TimeType)(c.clock()->getTime(c) - bench_start);
        }
#endif
    }
    
    static void planner_start_stepping (Context c)
    {
        MotionPlanner *o = self(c);
        AMBRO_ASSERT(o->m_state == STATE_BUFFERING)
        AMBRO_ASSERT(!o->m_underrun)
        AMBRO_ASSERT(o->m_segments_staging_length == o->m_segments_length)
        
#ifdef MOTIONPLANNER_BENCHMARK
        if (NumAxes > 1) {
            printf("elapsed %" PRIu32 "\n", o->m_bench_time);
        }
#endif
        o->m_state = STATE_STEPPING;
        TimeType start_time = c.clock()->getTime(c) + (TimeType)(0.05 * Context::Clock::time_freq);
        o->m_staging_time += start_time;
        TupleForEachForward(&o->m_axes, Foreach_start_stepping(), c, start_time);
        TupleForEachForward(&o->m_channels, Foreach_start_stepping(), c, start_time);
    }
    
    static void continue_wait (Context c)
    {
        MotionPlanner *o = self(c);
        AMBRO_ASSERT(o->m_state == STATE_BUFFERING)
        AMBRO_ASSERT(o->m_waiting)
        
        if (o->m_segments_length == 0) {
            o->m_pull_finished_event.prependNowNotAlready(c);
        } else {
            o->m_underrun = false;
            if (o->m_segments_staging_length != o->m_segments_length) {
                plan(c);
            }
            planner_start_stepping(c);
        }
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
            AMBRO_ASSERT(planner_is_empty(c))
            
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
    
    AMBRO_ALWAYS_INLINE static bool planner_is_empty (Context c)
    {
        MotionPlanner *o = self(c);
        return
            TupleForEachForwardAccRes(&o->m_axes, true, Foreach_is_empty(), c) &&
            TupleForEachForwardAccRes(&o->m_channels, true, Foreach_is_empty(), c);
    }
    
    AMBRO_ALWAYS_INLINE static uint8_t planner_is_underrun (Context c)
    {
        MotionPlanner *o = self(c);
        return
            TupleForEachForwardAccRes(&o->m_axes, false, Foreach_is_underrun(), c) ||
            TupleForEachForwardAccRes(&o->m_channels, false, Foreach_is_underrun(), c);
    }
    
    static void stepper_event_handler (Context c)
    {
        MotionPlanner *o = PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
        AMBRO_ASSERT(o->m_state == STATE_STEPPING)
        
        bool empty;
        bool aborted;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            o->m_underrun = planner_is_underrun(c);
            empty = planner_is_empty(c);
            aborted = TupleForEachForwardAccRes(&o->m_axes, false, Foreach_is_aborted(), c);
        }
        
        if (AMBRO_UNLIKELY(aborted)) {
            AMBRO_ASSERT(o->m_underrun)
            TupleForEachForward(&o->m_axes, Foreach_abort(), c);
            TupleForEachForward(&o->m_channels, Foreach_abort(), c);
            c.eventLoop()->template resetFastEvent<StepperFastEvent>(c);
            o->m_state = STATE_ABORTED;
            o->m_pull_finished_event.unset(c);
            return AbortedHandler::call(c);
        }
        
        if (AMBRO_UNLIKELY(empty)) {
            AMBRO_ASSERT(o->m_underrun)
            o->m_state = STATE_BUFFERING;
            o->m_segments_start = segments_add(o->m_segments_start, o->m_segments_staging_length);
            o->m_segments_length -= o->m_segments_staging_length;
            o->m_segments_staging_length = 0;
            o->m_staging_time = 0;
            o->m_staging_v_squared = 0.0;
            c.eventLoop()->template resetFastEvent<StepperFastEvent>(c);
            TupleForEachForward(&o->m_axes, Foreach_stopped_stepping(), c);
            TupleForEachForward(&o->m_channels, Foreach_stopped_stepping(), c);
            if (o->m_waiting) {
                return continue_wait(c);
            }
        }
        
        if (o->m_split_buffer.type != 0xFF) {
            work(c);
        }
    }
    
    static SegmentBufferSizeType segments_inc (SegmentBufferSizeType i)
    {
        return (i == LookaheadBufferSize - 1) ? 0 : (i + 1);
    }
    
    static SegmentBufferSizeType segments_add (SegmentBufferSizeType i, SegmentBufferSizeType j)
    {
        return (j >= LookaheadBufferSize - i) ? (j - (LookaheadBufferSize - i)) : (i + j);
    }    
    
    typename Loop::QueuedEvent m_pull_finished_event;
    SegmentBufferSizeType m_segments_start;
    SegmentBufferSizeType m_segments_staging_length;
    SegmentBufferSizeType m_segments_length;
    TimeType m_staging_time;
    double m_staging_v_squared;
    double m_first_segment_end_speed_squared;
    TimeType m_first_segment_time_duration;
    double m_last_distance_rec;
    uint8_t m_state;
    bool m_underrun;
    bool m_waiting;
#ifdef AMBROLIB_ASSERTIONS
    bool m_pulling;
#endif
    SplitBuffer m_split_buffer;
    Segment m_segments[LookaheadBufferSize];
    AxesTuple m_axes;
    ChannelsTuple m_channels;
#ifdef MOTIONPLANNER_BENCHMARK
    TimeType m_bench_time;
#endif
    
    template <int AxisIndex> struct AxisPosition : public TuplePosition<Position, AxesTuple, &MotionPlanner::m_axes, AxisIndex> {};
    template <int ChannelIndex> struct ChannelPosition : public TuplePosition<Position, ChannelsTuple, &MotionPlanner::m_channels, ChannelIndex> {};
};

#include <aprinter/EndNamespace.h>

#endif
