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
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/IndexElemTuple.h>
#include <aprinter/meta/TupleGet.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/Union.h>
#include <aprinter/meta/UnionGet.h>
#include <aprinter/meta/IndexElemUnion.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/MemberType.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Hints.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/printer/actuators/AxisDriverConsumer.h>
#include <aprinter/printer/planning/LinearPlanner.h>
#include <aprinter/printer/Configuration.h>

#include <aprinter/BeginNamespace.h>

APRINTER_ALIAS_STRUCT(MotionPlannerAxisSpec, (
    APRINTER_AS_TYPE(TheAxisDriver),
    APRINTER_AS_VALUE(int, StepBits),
    APRINTER_AS_TYPE(DistanceFactor),
    APRINTER_AS_TYPE(CorneringDistance),
    APRINTER_AS_TYPE(MaxSpeedRec),
    APRINTER_AS_TYPE(MaxAccelRec),
    APRINTER_AS_TYPE(PrestepCallback)
))

APRINTER_ALIAS_STRUCT(MotionPlannerChannelSpec, (
    APRINTER_AS_TYPE(Payload),
    APRINTER_AS_TYPE(Callback),
    APRINTER_AS_VALUE(int, BufferSize),
    APRINTER_AS_TYPE(TimerService)
))

APRINTER_ALIAS_STRUCT(MotionPlannerLaserSpec, (
    APRINTER_AS_TYPE(TheLaserDriverService),
    APRINTER_AS_TYPE(PowerInterface),
    APRINTER_AS_TYPE(MaxSpeedRec)
))

template <typename Context>
struct MotionPlannerConstants {
    // Allows dependant equal expressions to not be duplicated for different MotionPlanner instances.
    using FCpu = APRINTER_FP_CONST_EXPR(F_CPU);
    using TimeConversion = APRINTER_FP_CONST_EXPR(Context::Clock::time_freq);
};

template <typename Arg>
class MotionPlanner {
    using Context                             = typename Arg::Context;
    using ParentObject                        = typename Arg::ParentObject;
    using Config                              = typename Arg::Config;
    using ParamsAxesList                      = typename Arg::ParamsAxesList;
    static int const StepperSegmentBufferSize = Arg::StepperSegmentBufferSize;
    static int const LookaheadBufferSize      = Arg::LookaheadBufferSize;
    static int const LookaheadCommitCount     = Arg::LookaheadCommitCount;
    using FpType                              = typename Arg::FpType;
    using MaxStepsPerCycle                    = typename Arg::MaxStepsPerCycle;
    using PullHandler                         = typename Arg::PullHandler;
    using FinishedHandler                     = typename Arg::FinishedHandler;
    using AbortedHandler                      = typename Arg::AbortedHandler;
    using UnderrunCallback                    = typename Arg::UnderrunCallback;
    using ParamsChannelsList                  = typename Arg::ParamsChannelsList;
    using ParamsLasersList                    = typename Arg::ParamsLasersList;
    
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
    static const int NumAxes = TypeListLength<ParamsAxesList>::Value;
    static_assert(NumAxes > 0, "");
    static const int NumChannels = TypeListLength<ParamsChannelsList>::Value;
    using SegmentBufferSizeType = ChooseIntForMax<2 * LookaheadBufferSize, false>; // twice for segments_add()
    static const size_t StepperCommitBufferSize = 3 * StepperSegmentBufferSize;
    static const size_t StepperBackupBufferSize = 3 * (LookaheadBufferSize - LookaheadCommitCount);
    using StepperCommitBufferSizeType = ChooseIntForMax<StepperCommitBufferSize, false>;
    using StepperBackupBufferSizeType = ChooseIntForMax<2 * StepperBackupBufferSize, false>;
    using StepperFastEvent = typename Context::EventLoop::template FastEventSpec<MotionPlanner>;
    using CallbackFastEvent = typename Context::EventLoop::template FastEventSpec<StepperFastEvent>;
    static const int TypeBits = BitsInInt<NumChannels>::Value;
    using AxisMaskType = ChooseInt<NumAxes + TypeBits, false>;
    static const AxisMaskType TypeMask = ((AxisMaskType)1 << TypeBits) - 1;
    using TheLinearPlanner = LinearPlanner<FpType>;
    using Constants = MotionPlannerConstants<Context>;
    
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_TheCommon, TheCommon)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_ComputeState, ComputeState)
    
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
        StepFixedType x_pos; // internal
    };
    
    template <int LaserIndex>
    struct LaserSplitBuffer {
        FpType x;
    };
    
    using SplitBufferAxesTuple = IndexElemTuple<ParamsAxesList, AxisSplitBuffer>;
    using SplitBufferLasersTuple = IndexElemTuple<ParamsLasersList, LaserSplitBuffer>;
    
    struct SplitBufferAxesHelper : private SplitBufferAxesTuple {
        SplitBufferAxesTuple * axes () { return this; };
    };
    struct SplitBufferLasersHelper : private SplitBufferLasersTuple {
        SplitBufferLasersTuple * lasers () { return this; };
    };
    
    struct SplitBufferAxesPart : public SplitBufferAxesHelper, public SplitBufferLasersHelper {
        FpType rel_max_v_rec;
        FpType split_frac; // internal
        uint32_t split_count; // internal
        uint32_t split_pos; // internal
    };
    
    struct SplitBuffer {
        uint8_t type; // internal
        union {
            SplitBufferAxesPart axes;
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
        using TheAxisDriver = typename AxisSpec::TheAxisDriver;
        using StepperStepFixedType = typename TheAxisDriver::StepFixedType;
        
        StepperStepFixedType x;
    };
    
    template <int LaserIndex>
    struct LaserSegment {
        FpType x_by_distance;
    };
    
    template <int ChannelIndex>
    struct ChannelSegment {
        using ChannelSpec = TypeListGet<ParamsChannelsList, ChannelIndex>;
        using Payload = typename ChannelSpec::Payload;
        
        Payload payload;
    };
    
    using SegmentAxesTuple = IndexElemTuple<ParamsAxesList, AxisSegment>;
    using SegmentLasersTuple = IndexElemTuple<ParamsLasersList, LaserSegment>;
    
    struct SegmentAxesHelper : private SegmentAxesTuple {
        SegmentAxesTuple * axes () { return this; };
    };
    struct SegmentLasersHelper : private SegmentLasersTuple {
        SegmentLasersTuple * lasers () { return this; };
    };
    
    struct SegmentAxesPart : public SegmentAxesHelper, public SegmentLasersHelper {
        typename TheLinearPlanner::SegmentData lp_seg;
        FpType max_accel_rec;
        FpType rel_max_speed_rec;
    };
    
    struct Segment {
        AxisMaskType dir_and_type;
        union {
            SegmentAxesPart axes;
            IndexElemUnion<ParamsChannelsList, ChannelSegment> channels;
        };
    };
    
    enum {STATE_BUFFERING, STATE_STEPPING, STATE_ABORTED};
    
    template <typename TheAxis>
    struct AxisCommon {
        struct Object;
        using TheStepper = typename TheAxis::TheStepper;
        using StepperCommand = typename TheStepper::Command;
        using StepperCommandCallbackContext = typename TheStepper::CommandCallbackContext;
        using ComputeState = typename TheAxis::ComputeState;
        
        static void init (Context c, bool prestep_callback_enabled)
        {
            auto *o = Object::self(c);
            o->m_commit_start = 0;
            o->m_commit_end = 0;
            o->m_busy = false;
            TheAxis::init_impl(c, prestep_callback_enabled);
        }
        
        static void deinit (Context c)
        {
            TheAxis::abort_impl(c);
            TheAxis::deinit_impl(c);
        }
        
        static void abort (Context c)
        {
            TheAxis::abort_impl(c);
        }
        
        static void commandDone_assert (Context c)
        {
            TheAxis::commandDone_assert_impl(c);
        }
        
        static bool check_icmd_zero (bool accum, Context c)
        {
            return accum && TheAxis::check_icmd_zero_impl(c);
        }
        
        template <typename TheComputeStateTuple>
        static void compute_compute_state (Context c, Segment *entry, TheComputeStateTuple *cst)
        {
            TheAxis::compute_compute_state_impl(c, entry, cst);
        }
        
        template <typename TheComputeStateTuple>
        static void compute_steps_time (Context c, Segment *entry, TheComputeStateTuple *cst, FpType *sync_time, FpType *async_time)
        {
            FpType axis_sync_time;
            FpType axis_async_time;
            TheAxis::compute_steps_time_impl(c, entry, cst, &axis_sync_time, &axis_async_time);
            *sync_time = *sync_time + axis_sync_time;
            *async_time = FloatMax(*async_time, axis_async_time);
        }
        
        template <typename TheComputeStateTuple>
        static FpType compute_segment_buffer_entry_speed (FpType accum, Context c, Segment *entry, TheComputeStateTuple const *cst)
        {
            return FloatMax(accum, TheAxis::compute_segment_buffer_entry_speed_impl(c, entry, cst));
        }
        
        static bool have_commit_space (bool accum, Context c)
        {
            auto *o = Object::self(c);
            return (accum && commit_avail(o->m_commit_start, o->m_commit_end) >= 3 * LookaheadCommitCount);
        }
        
        static void start_commands (Context c)
        {
            auto *o = Object::self(c);
            auto *m = MotionPlanner::Object::self(c);
            o->m_new_commit_end = o->m_commit_end;
            o->m_new_backup_end = m->m_current_backup ? 0 : StepperBackupBufferSize;
        }
        
        template <typename... Args>
        static void gen_stepper_command (Context c, Args... args)
        {
            auto *o = Object::self(c);
            auto *m = MotionPlanner::Object::self(c);
            
            StepperCommand *cmd;
            if (AMBRO_UNLIKELY(!m->m_new_to_backup)) {
                cmd = &o->m_commit_buffer[o->m_new_commit_end];
                o->m_new_commit_end = commit_inc(o->m_new_commit_end);
            } else {
                cmd = &o->m_backup_buffer[o->m_new_backup_end];
                o->m_new_backup_end++;
            }
            TheStepper::generate_command(args..., cmd);
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
                if (TheAxis::IsFirst) {
                    // Only first axis sets it so it doesn't get re-set after a fast underflow.
                    // Can't happen anyway due to the start time offset.
                    m->m_syncing = true;
                }
                o->m_busy = true;
                StepperCommand *cmd = &o->m_commit_buffer[o->m_commit_start];
                o->m_commit_start = commit_inc(o->m_commit_start);
                TheAxis::start_stepping_impl(c, start_time, cmd);
            }
        }
        
        static bool is_busy (bool accum, Context c)
        {
            auto *o = Object::self(c);
            return (accum || o->m_busy);
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
                AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                    m->m_syncing = false;
                }
                if (o->m_backup_start == o->m_backup_end) {
                    o->m_busy = false;
                    return false;
                }
                *cmd = &o->m_backup_buffer[o->m_backup_start];
                o->m_backup_start++;
            }
            return true;
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
        
        struct Object : public ObjBase<AxisCommon, typename MotionPlanner::Object, MakeTypeList<
            TheAxis
        >> {
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
    
public:
    template <int AxisIndex>
    class Axis {
    public:
        using AxisSpec = TypeListGet<ParamsAxesList, AxisIndex>;
        using TheAxisSplitBuffer = AxisSplitBuffer<AxisIndex>;
        using TheAxisDriver = typename AxisSpec::TheAxisDriver;
        using StepFixedType = FixedPoint<AxisSpec::StepBits, false, 0>;
        using StepperCommandCallbackContext = typename TheAxisDriver::CommandCallbackContext;
        
    public: // private, workaround gcc bug
        friend MotionPlanner;
        
        struct Object;
        using TheCommon = AxisCommon<Axis>;
        using TheStepper = TheAxisDriver;
        static bool const IsFirst = (AxisIndex == 0);
        using StepperStepFixedType = typename TheAxisDriver::StepFixedType;
        using TheAxisSegment = AxisSegment<AxisIndex>;
        static const AxisMaskType TheAxisMask = (AxisMaskType)1 << (AxisIndex + TypeBits);
        
        struct ComputeState {
            FpType x;
        };
        
        using StepperCommand = typename TheCommon::StepperCommand;
        
        static void init_impl (Context c, bool prestep_callback_enabled)
        {
            auto *o = Object::self(c);
            TheAxisDriver::setPrestepCallbackEnabled(c, prestep_callback_enabled);
            o->last_x_by_distance = 0.0f;
        }
        
        static void deinit_impl (Context c)
        {
        }
        
        static void abort_impl (Context c)
        {
            TheAxisDriver::stop(c);
        }
        
        static void commandDone_assert_impl (Context c) {}
        
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
        
        template <typename AccumType>
        static FpType compute_split_count (AccumType accum, Context c)
        {
            TheAxisSplitBuffer *axis_split = get_axis_split(c);
            return FloatMax(accum, axis_split->x.template fpValue<FpType>() * (FpType)(1.0001 / StepperStepFixedType::maxValue().fpValueConstexpr()));
        }
        
        static bool check_icmd_zero_impl (Context c)
        {
            TheAxisSplitBuffer *axis_split = get_axis_split(c);
            return (axis_split->x.bitsValue() == 0);
        }
        
        static void write_segment_buffer_entry (Context c, Segment *entry)
        {
            auto *m = MotionPlanner::Object::self(c);
            TheAxisSplitBuffer *axis_split = get_axis_split(c);
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(entry->axes.axes());
            StepFixedType new_x;
            if (AMBRO_LIKELY(m->m_split_buffer.axes.split_pos == m->m_split_buffer.axes.split_count)) {
                new_x = axis_split->x;
            } else {
                new_x = FixedMin(axis_split->x, StepFixedType::importFpSaturatedRound(m->m_split_buffer.axes.split_pos * m->m_split_buffer.axes.split_frac * axis_split->x.template fpValue<FpType>()));
            }
            if (axis_split->dir) {
                entry->dir_and_type |= TheAxisMask;
            }
            axis_entry->x = StepperStepFixedType::importBits(new_x.bitsValue() - axis_split->x_pos.bitsValue());
            axis_split->x_pos = new_x;
        }
        
        template <typename TheComputeStateTuple>
        static void compute_compute_state_impl (Context c, Segment *entry, TheComputeStateTuple *cst)
        {
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(entry->axes.axes());
            ComputeState *cs = TupleFindElem<ComputeState>(cst);
            cs->x = axis_entry->x.template fpValue<FpType>();
        }
        
        template <typename TheComputeStateTuple>
        static void compute_steps_time_impl (Context c, Segment *entry, TheComputeStateTuple *cst, FpType *sync_time, FpType *async_time)
        {
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(entry->axes.axes());
            ComputeState *cs = TupleFindElem<ComputeState>(cst);
            FpType nonzero_x = (axis_entry->x.bitsValue() == 0) ? 1.0f : cs->x;
            *sync_time = nonzero_x * APRINTER_CFG(Config, CSyncMinStepTime, c);
            *async_time = nonzero_x * APRINTER_CFG(Config, CAsyncMinStepTime, c);
        }
        
        template <typename AccumType, typename TheComputeStateTuple>
        static FpType compute_segment_buffer_entry_distance (AccumType accum, Context c, TheComputeStateTuple const *cst)
        {
            ComputeState const *cs = TupleFindElem<ComputeState>(cst);
            return FloatMax(accum, cs->x * APRINTER_CFG(Config, CDistanceFactor, c));
        }
        
        template <typename TheComputeStateTuple>
        static FpType compute_segment_buffer_entry_speed_impl (Context c, Segment *entry, TheComputeStateTuple const *cst)
        {
            ComputeState const *cs = TupleFindElem<ComputeState>(cst);
            return cs->x * APRINTER_CFG(Config, CMaxSpeedRec, c);
        }
        
        template <typename AccumType, typename TheComputeStateTuple>
        static FpType compute_segment_buffer_entry_accel (AccumType accum, Context c, TheComputeStateTuple const *cst)
        {
            ComputeState const *cs = TupleFindElem<ComputeState>(cst);
            return FloatMax(accum, cs->x * APRINTER_CFG(Config, CMaxAccelRec, c));
        }
        
        template <typename AccumType, typename TheComputeStateTuple>
        static FpType do_junction_limit (AccumType accum, Context c, Segment const *entry, FpType distance_rec, TheComputeStateTuple const *cst)
        {
            auto *o = Object::self(c);
            auto *mo = MotionPlanner::Object::self(c);
            ComputeState const *cs = TupleFindElem<ComputeState>(cst);
            
            FpType m2 = o->last_x_by_distance;
            FpType m1 = cs->x * distance_rec;
            o->last_x_by_distance = m1;
            bool dir_changed = (entry->dir_and_type ^ mo->m_last_dir_and_type) & TheAxisMask;
            FpType dm = (dir_changed ? (m1 + m2) : FloatAbs(m1 - m2));
            return FloatMax(accum, dm * APRINTER_CFG(Config, CCorneringSpeedComputationFactor, c));
        }
        
        template <typename TheMinTimeType>
        static void gen_segment_stepper_commands (Context c, Segment *entry, FpType frac_x0, FpType frac_x2, TheMinTimeType t0, TheMinTimeType t2, TheMinTimeType t1, FpType vdiff0_squared, FpType vdiff2_squared)
        {
            TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(entry->axes.axes());
            
            FpType xfp = axis_entry->x.template fpValue<FpType>();
            StepperStepFixedType x1 = axis_entry->x;
            StepperStepFixedType x0 = FixedMin(x1, StepperStepFixedType::importFpSaturatedRound(frac_x0 * xfp));
            x1.m_bits.m_int -= x0.bitsValue();
            StepperStepFixedType x2 = FixedMin(x1, StepperStepFixedType::importFpSaturatedRound(frac_x2 * xfp));
            x1.m_bits.m_int -= x2.bitsValue();
            
            if (x0.bitsValue() == 0) {
                t1.m_bits.m_int += t0.bitsValue();
            }
            if (x2.bitsValue() == 0) {
                t1.m_bits.m_int += t2.bitsValue();
            }
            
            bool skip1 = (x1.bitsValue() == 0 && (x0.bitsValue() != 0 || x2.bitsValue() != 0));
            if (skip1) {
                if (x0.bitsValue() != 0) {
                    t0.m_bits.m_int += t1.bitsValue();
                } else {
                    t2.m_bits.m_int += t1.bitsValue();
                }
            }
            
            bool dir = entry->dir_and_type & TheAxisMask;
            FpType accel_conversion = entry->axes.lp_seg.a_x_rec * xfp;
            
            if (x0.bitsValue() != 0) {
                TheCommon::gen_stepper_command(c, dir, x0, t0, FixedMin(x0, StepperStepFixedType::importFpSaturatedRound(accel_conversion * vdiff0_squared)));
            }
            if (!skip1) {
                TheCommon::gen_stepper_command(c, dir, x1, t1, StepperStepFixedType::importBits(0));
            }
            if (x2.bitsValue() != 0) {
                TheCommon::gen_stepper_command(c, dir, x2, t2, -FixedMin(x2, StepperStepFixedType::importFpSaturatedRound(accel_conversion * vdiff2_squared)));
            }
        }
        
        static void start_stepping_impl (Context c, TimeType start_time, StepperCommand *cmd)
        {
            TheAxisDriver::template start<TheAxisDriverConsumer<AxisIndex>>(c, start_time, cmd);
        }
        
        AMBRO_ALWAYS_INLINE
        static bool stepper_prestep_callback (StepperCommandCallbackContext c)
        {
            auto *m = MotionPlanner::Object::self(c);
            bool res = AxisSpec::PrestepCallback::call(c);
            if (AMBRO_UNLIKELY(res)) {
                Context::EventLoop::template triggerFastEvent<StepperFastEvent>(c);
                AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                    m->m_aborted = true;
                }
            }
            return res;
        }
        
        template <typename StepsType>
        static void add_command_steps (Context c, StepsType *steps, StepperCommand *cmd)
        {
            bool dir;
            StepperStepFixedType cmd_steps = TheAxisDriver::getPendingCmdSteps(c, cmd, &dir);
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
            auto *co = TheCommon::Object::self(c);
            auto *m = MotionPlanner::Object::self(c);
            
            StepsType steps = 0;
            if (co->m_busy) {
                bool dir;
                StepperStepFixedType cmd_steps = TheAxisDriver::getAbortedCmdSteps(c, &dir);
                add_steps(&steps, cmd_steps, dir);
            }
            for (StepperCommitBufferSizeType i = co->m_commit_start; i != co->m_commit_end; i = TheCommon::commit_inc(i)) {
                add_command_steps(c, &steps, &co->m_commit_buffer[i]);
            }
            for (StepperBackupBufferSizeType i = co->m_backup_start; i < co->m_backup_end; i++) {
                add_command_steps(c, &steps, &co->m_backup_buffer[i]);
            }
            for (SegmentBufferSizeType i = m->m_segments_staging_length; i < m->m_segments_length; i++) {
                Segment *seg = &m->m_segments[segments_add(m->m_segments_start, i)];
                TheAxisSegment *axis_entry = TupleGetElem<AxisIndex>(seg->axes.axes());
                add_steps(&steps, axis_entry->x, (seg->dir_and_type & TheAxisMask));
            }
            if (m->m_split_buffer.type == 0) {
                TheAxisSplitBuffer *axis_split = get_axis_split(c);
                StepFixedType x = StepFixedType::importBits(axis_split->x.bitsValue() - axis_split->x_pos.bitsValue());
                add_steps(&steps, x, axis_split->dir);
            }
            return steps;
        }
        
        static TheAxisSplitBuffer * get_axis_split (Context c)
        {
            auto *m = MotionPlanner::Object::self(c);
            return TupleGetElem<AxisIndex>(m->m_split_buffer.axes.axes());
        }
        
#ifdef AXISDRIVER_DETECT_OVERLOAD
        static bool overload_occurred (bool accum, Context c)
        {
            return accum || TheAxisDriver::overloadOccurred(c);
        }
#endif
        
        using DriverSyncMinStepTime = APRINTER_FP_CONST_EXPR(TheAxisDriver::SyncMinStepTime());
        using DriverAsyncMinStepTime = APRINTER_FP_CONST_EXPR(TheAxisDriver::AsyncMinStepTime());
        
        using SyncMinStepTime = decltype(typename Constants::TimeConversion() * (ExprRec(MaxStepsPerCycle() * typename Constants::FCpu()) + DriverSyncMinStepTime()));
        
        using CDistanceFactor = decltype(ExprCast<FpType>(AxisSpec::DistanceFactor::e()));
        using CCorneringSpeedComputationFactor = decltype(ExprCast<FpType>(AxisSpec::MaxAccelRec::e() / (AxisSpec::CorneringDistance::e() * AxisSpec::DistanceFactor::e())));
        using CMaxSpeedRec = decltype(ExprCast<FpType>(AxisSpec::MaxSpeedRec::e()));
        using CMaxAccelRec = decltype(ExprCast<FpType>(AxisSpec::MaxAccelRec::e()));
        using CSyncMinStepTime = decltype(ExprCast<FpType>(SyncMinStepTime()));
        using CAsyncMinStepTime = decltype(ExprCast<FpType>(SyncMinStepTime() + typename Constants::TimeConversion() * DriverAsyncMinStepTime()));
        
        using ConfigExprs = MakeTypeList<CDistanceFactor, CCorneringSpeedComputationFactor, CMaxSpeedRec, CMaxAccelRec, CSyncMinStepTime, CAsyncMinStepTime>;
        
        struct Object : public ObjBase<Axis, typename TheCommon::Object, EmptyTypeList> {
            FpType last_x_by_distance;
        };
    };
    
    template <int LaserIndex>
    class Laser {
    public: // private, workaround gcc bug
        friend MotionPlanner;
        struct Object;
        struct StepperCommandCallback;
        
    public:
        using LaserSpec = TypeListGet<ParamsLasersList, LaserIndex>;
        using TheLaserSplitBuffer = LaserSplitBuffer<LaserIndex>;
        APRINTER_MAKE_INSTANCE(TheLaserDriver, (LaserSpec::TheLaserDriverService::template Driver<Context, Object, FpType, typename LaserSpec::PowerInterface, StepperCommandCallback>))
        
    public: // private, workaround gcc bug
        using TheCommon = AxisCommon<Laser>;
        using TheStepper = TheLaserDriver;
        static bool const IsFirst = false;
        using TheLaserSegment = LaserSegment<LaserIndex>;
        static TimeType const AdjustmentIntervalTicks = LaserSpec::TheLaserDriverService::AdjustmentInterval::value() / Clock::time_unit;
        
        struct ComputeState {};
        
        using StepperCommand = typename TheCommon::StepperCommand;
        
        static void init_impl (Context c, bool prestep_callback_enabled)
        {
            TheLaserDriver::init(c);
        }
        
        static void deinit_impl (Context c)
        {
            TheLaserDriver::deinit(c);
        }
        
        static void abort_impl (Context c)
        {
            TheLaserDriver::stop(c);
        }
        
        static void commandDone_assert_impl (Context c)
        {
            TheLaserSplitBuffer *laser_split = get_laser_split(c);
            AMBRO_ASSERT(FloatIsPosOrPosZero(laser_split->x))
        }
        
        static void fixup_split (Context c)
        {
            auto *m = MotionPlanner::Object::self(c);
            TheLaserSplitBuffer *laser_split = get_laser_split(c);
            
            laser_split->x *= m->m_split_buffer.axes.split_frac;
        }
        
        static bool check_icmd_zero_impl (Context c)
        {
            TheLaserSplitBuffer *laser_split = get_laser_split(c);
            return (laser_split->x == 0.0f);
        }
        
        template <typename TheComputeStateTuple>
        static void compute_compute_state_impl (Context c, Segment *entry, TheComputeStateTuple *cst)
        {
        }
        
        template <typename TheComputeStateTuple>
        static void compute_steps_time_impl (Context c, Segment *entry, TheComputeStateTuple *cst, FpType *sync_time, FpType *async_time)
        {
            *sync_time = 0.0f;
            *async_time = 0.0f;
        }
        
        template <typename TheComputeStateTuple>
        static FpType compute_segment_buffer_entry_speed_impl (Context c, Segment *entry, TheComputeStateTuple const *cst)
        {
            TheLaserSplitBuffer *laser_split = get_laser_split(c);
            
            return laser_split->x * APRINTER_CFG(Config, CMaxSpeedRec, c);
        }
        
        static void write_segment_buffer_entry_extra (Context c, Segment *entry, FpType distance_rec)
        {
            TheLaserSplitBuffer *laser_split = get_laser_split(c);
            TheLaserSegment *laser_segment = TupleGetElem<LaserIndex>(entry->axes.lasers());
            
            laser_segment->x_by_distance = laser_split->x * distance_rec * (FpType)Clock::time_freq;
        }
        
        template <typename TheTheMinTimeType>
        static void gen_segment_stepper_commands (Context c, Segment *entry, TheTheMinTimeType t0, TheTheMinTimeType t2, TheTheMinTimeType t1, FpType v_start, FpType v_end, FpType v_const)
        {
            TheLaserSegment *laser_segment = TupleGetElem<LaserIndex>(entry->axes.lasers());
            
            FpType xv_start = laser_segment->x_by_distance * v_start;
            FpType xv_end = laser_segment->x_by_distance * v_end;
            FpType xv_const = laser_segment->x_by_distance * v_const;
            
            bool skip0 = (t0.bitsValue() < AdjustmentIntervalTicks);
            if (skip0) {
                t1.m_bits.m_int += t0.bitsValue();
            }
            
            bool skip2 = (t2.bitsValue() < AdjustmentIntervalTicks);
            if (skip2) {
                t1.m_bits.m_int += t2.bitsValue();
            }
            
            bool skip1 = (t1.bitsValue() < AdjustmentIntervalTicks && (!skip0 || !skip2));
            if (skip1) {
                if (!skip0) {
                    t0.m_bits.m_int += t1.bitsValue();
                } else {
                    t2.m_bits.m_int += t1.bitsValue();
                }
            }
            
            if (!skip0) {
                TheCommon::gen_stepper_command(c, t0, xv_start, xv_const);
            }
            if (!skip1) {
                TheCommon::gen_stepper_command(c, t1, xv_const, xv_const);
            }
            if (!skip2) {
                TheCommon::gen_stepper_command(c, t2, xv_const, xv_end);
            }
        }
        
        static void start_stepping_impl (Context c, TimeType start_time, StepperCommand *cmd)
        {
            TheLaserDriver::start(c, start_time, cmd);
        }
        
        static TheLaserSplitBuffer * get_laser_split (Context c)
        {
            auto *m = MotionPlanner::Object::self(c);
            return TupleGetElem<LaserIndex>(m->m_split_buffer.axes.lasers());
        }
        
        using CMaxSpeedRec = decltype(ExprCast<FpType>(LaserSpec::MaxSpeedRec::e()));
        
        using ConfigExprs = MakeTypeList<CMaxSpeedRec>;
        
        struct StepperCommandCallback : public AMBRO_WFUNC_TD(&TheCommon::stepper_command_callback) {};
        
        struct Object : public ObjBase<Laser, typename TheCommon::Object, MakeTypeList<
            TheLaserDriver
        >> {};
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
        APRINTER_MAKE_INSTANCE(TheTimer, (ChannelSpec::TimerService::template InterruptTimer<Context, Object, TimerHandler>))
        using CallbackContext = typename TheTimer::HandlerContext;
        
    public: // private, workaround gcc bug
        static_assert(ChannelSpec::BufferSize - LookaheadCommitCount > 1, "");
        static const size_t ChannelCommitBufferSize = ChannelSpec::BufferSize;
        static const size_t ChannelBackupBufferSize = LookaheadBufferSize - LookaheadCommitCount;
        using ChannelCommitBufferSizeType = ChooseIntForMax<ChannelCommitBufferSize, false>;
        using ChannelBackupBufferSizeType = ChooseIntForMax<2 * ChannelBackupBufferSize, false>;
        using LookaheadSizeType = ChooseIntForMax<LookaheadBufferSize, false>;
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            o->m_commit_start = 0;
            o->m_commit_end = 0;
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
                AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                    m->m_syncing = false;
                }
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
    using LasersList = IndexElemList<ParamsLasersList, Laser>;
    using ChannelsList = IndexElemList<ParamsChannelsList, Channel>;
    using AxisCommonList = MapTypeList<JoinTypeLists<AxesList, LasersList>, GetMemberType_TheCommon>;
    
    template <typename TheAxisCommon, typename AccumType>
    using MinTimeTypeHelper = FixedIntersectTypes<typename TheAxisCommon::TheStepper::TimeFixedType, AccumType>;
    using MinTimeType = TypeListFold<AxisCommonList, FixedIdentity, MinTimeTypeHelper>;
    
    struct ComputeStateTuple : public Tuple<MapTypeList<AxisCommonList, GetMemberType_ComputeState>> {};
    
public:
    static void init (Context c, bool prestep_callback_enabled)
    {
        auto *o = Object::self(c);
        
        Context::EventLoop::template initFastEvent<StepperFastEvent>(c, MotionPlanner::stepper_event_handler);
        Context::EventLoop::template initFastEvent<CallbackFastEvent>(c, MotionPlanner::callback_event_handler);
        o->m_segments_start = 0;
        o->m_segments_staging_length = 0;
        o->m_segments_length = 0;
        o->m_staging_time = 0;
        o->m_staging_v_squared = 0.0f;
        o->m_staging_v = 0.0f;
        o->m_last_max_v = 0.0f;
        o->m_last_dir_and_type = 0;
        o->m_split_buffer.type = 0xFF;
        o->m_state = STATE_BUFFERING;
        o->m_waiting = false;
        o->m_aborted = false;
        o->m_syncing = false;
        o->m_current_backup = false;
#ifdef AMBROLIB_ASSERTIONS
        o->m_pulling = false;
        o->m_planned = false;
#endif
        ListFor<AxisCommonList>([&] APRINTER_TL(axis, axis::init(c, prestep_callback_enabled)));
        ListFor<ChannelsList>([&] APRINTER_TL(channel, channel::init(c)));
        Context::EventLoop::template triggerFastEvent<CallbackFastEvent>(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        
        ListForReverse<ChannelsList>([&] APRINTER_TL(channel, channel::deinit(c)));
        ListForReverse<AxisCommonList>([&] APRINTER_TL(axis, axis::deinit(c)));
        Context::EventLoop::template resetFastEvent<CallbackFastEvent>(c);
        Context::EventLoop::template resetFastEvent<StepperFastEvent>(c);
    }
    
    static SplitBuffer * getBuffer (Context c)
    {
        auto *o = Object::self(c);
        assert_pulling(c);
        
        return &o->m_split_buffer;
    }
    
    static void axesCommandDone (Context c)
    {
        auto *o = Object::self(c);
        assert_pulling(c);
        AMBRO_ASSERT(FloatIsPosOrPosZero(o->m_split_buffer.axes.rel_max_v_rec))
        ListFor<AxisCommonList>([&] APRINTER_TL(axis, axis::commandDone_assert(c)));
        
        stop_pulling(c);
        
        if (AMBRO_UNLIKELY(ListForFold<AxisCommonList>(true, [&] APRINTER_TLA(axis, (bool accum), return axis::check_icmd_zero(accum, c))) && o->m_split_buffer.axes.rel_max_v_rec == 0.0f)) {
            Context::EventLoop::template triggerFastEvent<CallbackFastEvent>(c);
            return;
        }
        
        o->m_split_buffer.type = 0;
        ListFor<AxesList>([&] APRINTER_TL(axis, axis::write_splitbuf(c)));
        o->m_split_buffer.axes.split_pos = 0;
        if (AMBRO_LIKELY(ListForFold<AxesList>(true, [&] APRINTER_TLA(axis, (bool accum), return axis::splitbuf_fits(accum, c))))) {
            o->m_split_buffer.axes.split_count = 1;
        } else {
            o->m_split_buffer.axes.split_count = FloatCeil(ListForFold<AxesList>(FloatIdentity(), [&] APRINTER_TLA(axis, (auto accum), return axis::compute_split_count(accum, c))));
            o->m_split_buffer.axes.split_frac = (FpType)1.0 / o->m_split_buffer.axes.split_count;
            o->m_split_buffer.axes.rel_max_v_rec *= o->m_split_buffer.axes.split_frac;
            ListFor<LasersList>([&] APRINTER_TL(laser, laser::fixup_split(c)));
        }
        
        Context::EventLoop::template triggerFastEvent<StepperFastEvent>(c);
    }
    
    static void channelCommandDone (Context c, uint8_t channel_index_plus_one)
    {
        auto *o = Object::self(c);
        assert_pulling(c);
        AMBRO_ASSERT(channel_index_plus_one >= 1)
        AMBRO_ASSERT(channel_index_plus_one <= NumChannels)
        
        stop_pulling(c);
        
        o->m_split_buffer.type = channel_index_plus_one;
        
        Context::EventLoop::template triggerFastEvent<StepperFastEvent>(c);
    }
    
    static void emptyDone (Context c)
    {
        auto *o = Object::self(c);
        assert_pulling(c);
        
        stop_pulling(c);
        
        Context::EventLoop::template triggerFastEvent<CallbackFastEvent>(c);
    }
    
    static void waitFinished (Context c)
    {
        auto *o = Object::self(c);
        assert_pulling(c);
        
        o->m_waiting = true;
        Context::EventLoop::template triggerFastEvent<StepperFastEvent>(c);
    }
    
    template <int AxisIndex, typename StepsType>
    static StepsType countAbortedRemSteps (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state == STATE_ABORTED)
        
        return Axis<AxisIndex>::template axis_count_aborted_rem_steps<StepsType>(c);
    }
    
    static char getStatusCode (Context c)
    {
        auto *o = Object::self(c);
        
        if (o->m_state == STATE_BUFFERING) {
            return (o->m_segments_length > 0) ? 'R' : 'I';
        }
        else if (o->m_state == STATE_STEPPING) {
            bool syncing;
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                syncing = o->m_syncing;
            }
            return syncing ? 'P' : 'D';
        }
        else {
            return 'D';
        }
    }
    
#ifdef AXISDRIVER_DETECT_OVERLOAD
    static bool axisOverloadOccurred (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state == STATE_BUFFERING)
        
        return ListForFold<AxesList>(false, [&] APRINTER_TLA(axis, (bool accum), return axis::overload_occurred(accum, c)));
    }
#endif
    
    template <int ChannelIndex>
    using GetChannelTimer = typename Channel<ChannelIndex>::TheTimer;
    
    template <int AxisIndex>
    using TheAxisDriverConsumer = AxisDriverConsumer<
        AMBRO_WFUNC_T(&Axis<AxisIndex>::TheCommon::stepper_command_callback),
        AMBRO_WFUNC_T(&Axis<AxisIndex>::stepper_prestep_callback)
    >;
    
    using EventLoopFastEvents = MakeTypeList<StepperFastEvent, CallbackFastEvent>;
    
private:
    static void assert_pulling (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state != STATE_ABORTED)
        AMBRO_ASSERT(o->m_pulling)
        AMBRO_ASSERT(o->m_split_buffer.type == 0xFF)
    }
    
    static void stop_pulling (Context c)
    {
        auto *o = Object::self(c);
        
        o->m_waiting = false;
        Context::EventLoop::template resetFastEvent<CallbackFastEvent>(c);
#ifdef AMBROLIB_ASSERTIONS
        o->m_pulling = false;
#endif
    }
    
    static bool plan (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state != STATE_ABORTED)
        AMBRO_ASSERT(o->m_segments_staging_length != o->m_segments_length)
#ifdef AMBROLIB_ASSERTIONS
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) { AMBRO_ASSERT(planner_have_commit_space(c)) }
#endif
        
        SegmentBufferSizeType i = o->m_segments_length;
        FpType v = 0.0f;
        do {
            i--;
            Segment *entry = &o->m_segments[segments_add(o->m_segments_start, i)];
            if (AMBRO_LIKELY((entry->dir_and_type & TypeMask) == 0)) {
                v = TheLinearPlanner::push(&entry->axes.lp_seg, &o->m_segment_state[i], v);
            }
        } while (i != 0);
        
        SegmentBufferSizeType commit_count = MinValue(o->m_segments_length, (SegmentBufferSizeType)LookaheadCommitCount);
        
        o->m_new_to_backup = false;
        ListFor<AxisCommonList>([&] APRINTER_TL(axis, axis::start_commands(c)));
        ListFor<ChannelsList>([&] APRINTER_TL(channel, channel::start_commands(c)));
        
        TimeType time = o->m_staging_time;
        v = o->m_staging_v_squared;
        FpType v_start = o->m_staging_v;
        
        do {
            Segment *entry = &o->m_segments[segments_add(o->m_segments_start, i)];
            if (AMBRO_LIKELY((entry->dir_and_type & TypeMask) == 0)) {
                typename TheLinearPlanner::SegmentResult result;
                v = TheLinearPlanner::pull(&entry->axes.lp_seg, &o->m_segment_state[i], v, &result);
                FpType v_end = FloatSqrt(v);
                FpType v_const = FloatSqrt(result.const_v);
                FpType vdiff0 = v_const - v_start;
                FpType vdiff2 = v_const - v_end;
                FpType t0_double = vdiff0 * entry->axes.max_accel_rec;
                MinTimeType t0 = MinTimeType::importFpSaturatedRound(t0_double);
                FpType t2_double = vdiff2 * entry->axes.max_accel_rec;
                MinTimeType t2 = MinTimeType::importFpSaturatedRound(t2_double);
                FpType t1_double = (1.0f - result.const_start - result.const_end) * entry->axes.rel_max_speed_rec;
                MinTimeType t1 = MinTimeType::importFpSaturatedRound(t1_double);
                auto t_sum = t0 + t2 + t1;
                if (AMBRO_UNLIKELY(t_sum > MinTimeType::maxValue())) {
                    t1 = MinTimeType::maxValue();
                    t_sum = t1;
                    t0 = FixedMin(t1, t0);
                    t1.m_bits.m_int -= t0.bitsValue();
                    t2 = FixedMin(t1, t2);
                    t1.m_bits.m_int -= t2.bitsValue();
                }
                time += t_sum.bitsValue();
                ListFor<AxesList>([&] APRINTER_TL(axis, axis::gen_segment_stepper_commands(c, entry,
                                    result.const_start, result.const_end, t0, t2, t1,
                                    vdiff0 * vdiff0, vdiff2 * vdiff2)));
                ListFor<LasersList>([&] APRINTER_TL(laser, laser::gen_segment_stepper_commands(c, entry,
                    t0, t2, t1, v_start, v_end, v_const)));
                v_start = v_end;
            } else {
                ListForOne<ChannelsList, 1>((entry->dir_and_type & TypeMask), [&] APRINTER_TL(channel, channel::gen_command(c, entry, time)));
            }
            i++;
            if (AMBRO_UNLIKELY(i == commit_count)) {
                // It's safe to update these here before committing the new plan,
                // since in case of commit failure (loss of sync), plan() will
                // not be called until we're back to buffering state.
                o->m_new_to_backup = true;
                o->m_staging_time = time;
                o->m_staging_v_squared = v;
                o->m_staging_v = v_start;
            }
        } while (i != o->m_segments_length);
        
        bool ok;
        if (AMBRO_UNLIKELY(o->m_state == STATE_BUFFERING)) {
            ok = true;
            ListFor<AxisCommonList>([&] APRINTER_TL(axis, axis::do_commit(c)));
            ListFor<ChannelsList>([&] APRINTER_TL(channel, channel::do_commit_cold(c)));
            o->m_current_backup = !o->m_current_backup;
        } else {
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                ok = o->m_syncing;
                if (AMBRO_LIKELY(ok)) {
                    ListFor<AxisCommonList>([&] APRINTER_TL(axis, axis::do_commit(c)));
                    ListFor<ChannelsList>([&] APRINTER_TL(channel, channel::do_commit_hot(lock_c)));
                    o->m_current_backup = !o->m_current_backup;
                }
            }
        }
        
        if (AMBRO_LIKELY(ok)) {
            o->m_segments_start = segments_add(o->m_segments_start, commit_count);
            o->m_segments_length -= commit_count;
            o->m_segments_staging_length = o->m_segments_length;
#ifdef AMBROLIB_ASSERTIONS
            o->m_planned = true;
#endif
        }
        return ok;
    }
    
    static void planner_start_stepping (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state == STATE_BUFFERING)
        AMBRO_ASSERT(!o->m_syncing)
        AMBRO_ASSERT(o->m_planned)
        
        o->m_state = STATE_STEPPING;
        TimeType start_time = Clock::getTime(c) + (TimeType)(0.05 * Context::Clock::time_freq);
        o->m_staging_time += start_time;
        ListFor<AxisCommonList>([&] APRINTER_TL(axis, axis::start_stepping(c, start_time)));
        ListFor<ChannelsList>([&] APRINTER_TL(channel, channel::start_stepping(c, start_time)));
    }
    
    static void callback_event_handler (Context c)
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
    
    AMBRO_ALWAYS_INLINE static bool planner_have_commit_space (Context c)
    {
        return
            ListForFold<AxisCommonList>(true, [&] APRINTER_TLA(axis, (bool accum), return axis::have_commit_space(accum, c))) &&
            ListForFold<ChannelsList>(true, [&] APRINTER_TLA(channel, (bool accum), return channel::have_commit_space(accum, c)));
    }
    
    AMBRO_ALWAYS_INLINE static bool planner_is_busy (Context c)
    {
        return
            ListForFold<AxisCommonList>(false, [&] APRINTER_TLA(axis, (bool accum), return axis::is_busy(accum, c))) ||
            ListForFold<ChannelsList>(false, [&] APRINTER_TLA(channel, (bool accum), return channel::is_busy(accum, c)));
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
                return finish_after_aborted(c);
            }
            
            if (AMBRO_UNLIKELY(!busy)) {
                recover_from_underrun(c);
            }
        }
        
        if (AMBRO_UNLIKELY(o->m_waiting)) {
            if (AMBRO_UNLIKELY(o->m_state == STATE_BUFFERING)) {
                if (o->m_segments_length == 0) {
                    Context::EventLoop::template triggerFastEvent<CallbackFastEvent>(c);
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
            
            emit_segment(c);
        }
    }
    
    static void finish_after_aborted (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state == STATE_STEPPING)
        
        ListFor<AxisCommonList>([&] APRINTER_TL(axis, axis::abort(c)));
        ListFor<ChannelsList>([&] APRINTER_TL(channel, channel::abort(c)));
        Context::EventLoop::template resetFastEvent<StepperFastEvent>(c);
        o->m_state = STATE_ABORTED;
        Context::EventLoop::template resetFastEvent<CallbackFastEvent>(c);
        return AbortedHandler::call(c);
    }
    
    static void recover_from_underrun (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_state == STATE_STEPPING)
        AMBRO_ASSERT(!o->m_syncing)
        
        o->m_state = STATE_BUFFERING;
        o->m_segments_start = segments_add(o->m_segments_start, o->m_segments_staging_length);
        o->m_segments_length -= o->m_segments_staging_length;
        o->m_segments_staging_length = 0;
        o->m_staging_time = 0;
        o->m_staging_v_squared = 0.0f;
        o->m_staging_v = 0.0f;
#ifdef AMBROLIB_ASSERTIONS
        o->m_planned = false;
#endif
        UnderrunCallback::call(c);
    }
    
    static void emit_segment (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(!o->m_pulling)
        AMBRO_ASSERT(o->m_split_buffer.type != 0xFF)
        AMBRO_ASSERT(o->m_split_buffer.type != 0 || o->m_split_buffer.axes.split_pos < o->m_split_buffer.axes.split_count)
        
        Segment *entry = &o->m_segments[segments_add(o->m_segments_start, o->m_segments_length)];
        entry->dir_and_type = o->m_split_buffer.type;
        
        if (AMBRO_LIKELY(o->m_split_buffer.type == 0)) {
            o->m_split_buffer.axes.split_pos++;
            ListFor<AxesList>([&] APRINTER_TL(axis, axis::write_segment_buffer_entry(c, entry)));
            
            ComputeStateTuple cst;
            ListFor<AxisCommonList>([&] APRINTER_TL(axis, axis::compute_compute_state(c, entry, &cst)));
            
            FpType sync_steps_time = 0.0f;
            FpType async_steps_time = 0.0f;
            ListFor<AxisCommonList>([&] APRINTER_TL(axis, axis::compute_steps_time(c, entry, &cst, &sync_steps_time, &async_steps_time)));
            FpType base_rel_max_speed = FloatMax(o->m_split_buffer.axes.rel_max_v_rec, FloatMax(sync_steps_time, async_steps_time));
            entry->axes.rel_max_speed_rec = ListForFold<AxisCommonList>(base_rel_max_speed, [&] APRINTER_TLA(axis, (FpType accum), return axis::compute_segment_buffer_entry_speed(accum, c, entry, &cst)));
            
            FpType distance = ListForFold<AxesList>(FloatIdentity(), [&] APRINTER_TLA(axis, (auto accum), return axis::compute_segment_buffer_entry_distance(accum, c, &cst)));
            bool degenerate = (distance == 0.0f);
            if (degenerate) {
                distance = 1.0f;
            }
            FpType distance_rec = 1.0f / distance;
            
            ListFor<LasersList>([&] APRINTER_TL(laser, laser::write_segment_buffer_entry_extra(c, entry, distance_rec)));
            
            FpType rel_max_accel_rec = ListForFold<AxesList>(FloatIdentity(), [&] APRINTER_TLA(axis, (auto accum), return axis::compute_segment_buffer_entry_accel(accum, c, &cst)));
            entry->axes.max_accel_rec = rel_max_accel_rec * distance_rec;
            FpType half_rel_max_accel = 0.5f / rel_max_accel_rec;
            
            FpType distance_rec_for_junction = AMBRO_UNLIKELY(degenerate) ? NAN : distance_rec;
            FpType junction_max_v_rec = ListForFold<AxesList>(FloatIdentity(), [&] APRINTER_TLA(axis, (auto accum), return axis::do_junction_limit(accum, c, entry, distance_rec_for_junction, &cst)));
            FpType junction_max_start_v = AMBRO_UNLIKELY(FloatIsNan(junction_max_v_rec)) ? 0.0f : (1.0f / junction_max_v_rec);
            o->m_last_dir_and_type = entry->dir_and_type;
            
            FpType distance_squared = distance * distance;
            FpType max_v = distance_squared / (entry->axes.rel_max_speed_rec * entry->axes.rel_max_speed_rec);
            FpType a_x = FloatLdexp(half_rel_max_accel * distance_squared, 2);
            TheLinearPlanner::initSegment(&entry->axes.lp_seg, o->m_last_max_v, junction_max_start_v, max_v, a_x);
            o->m_last_max_v = max_v;
            
            if (AMBRO_LIKELY(o->m_split_buffer.axes.split_pos == o->m_split_buffer.axes.split_count)) {
                o->m_split_buffer.type = 0xFF;
            }
        } else {
            ListForOne<ChannelsList, 1>((entry->dir_and_type & TypeMask), [&] APRINTER_TL(channel, channel::write_segment(c, entry)));
            o->m_split_buffer.type = 0xFF;
        }
        
        o->m_segments_length++;
        
        if (AMBRO_LIKELY(o->m_split_buffer.type == 0xFF)) {
            Context::EventLoop::template triggerFastEvent<CallbackFastEvent>(c);
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
        AxisCommonList,
        ChannelsList
    >> {
        SegmentBufferSizeType m_segments_start;
        SegmentBufferSizeType m_segments_staging_length;
        SegmentBufferSizeType m_segments_length;
        TimeType m_staging_time;
        FpType m_staging_v_squared;
        FpType m_staging_v;
        FpType m_last_max_v;
        AxisMaskType m_last_dir_and_type;
        uint8_t m_state;
        bool m_waiting;
        bool m_aborted;
        bool m_syncing;
        bool m_current_backup;
        bool m_new_to_backup;
#ifdef AMBROLIB_ASSERTIONS
        bool m_pulling;
        bool m_planned;
#endif
        SplitBuffer m_split_buffer;
        Segment m_segments[LookaheadBufferSize];
        typename TheLinearPlanner::SegmentState m_segment_state[LookaheadBufferSize];
    };
};

APRINTER_ALIAS_STRUCT_EXT(MotionPlannerArg, (
    APRINTER_AS_TYPE(Context),
    APRINTER_AS_TYPE(ParentObject),
    APRINTER_AS_TYPE(Config),
    APRINTER_AS_TYPE(ParamsAxesList),
    APRINTER_AS_VALUE(int, StepperSegmentBufferSize),
    APRINTER_AS_VALUE(int, LookaheadBufferSize),
    APRINTER_AS_VALUE(int, LookaheadCommitCount),
    APRINTER_AS_TYPE(FpType),
    APRINTER_AS_TYPE(MaxStepsPerCycle),
    APRINTER_AS_TYPE(PullHandler),
    APRINTER_AS_TYPE(FinishedHandler),
    APRINTER_AS_TYPE(AbortedHandler),
    APRINTER_AS_TYPE(UnderrunCallback),
    APRINTER_AS_TYPE(ParamsChannelsList),
    APRINTER_AS_TYPE(ParamsLasersList)
), (
    APRINTER_DEF_INSTANCE(MotionPlannerArg, MotionPlanner)
))

#include <aprinter/EndNamespace.h>

#endif
