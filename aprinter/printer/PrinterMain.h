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

#ifndef AMBROLIB_PRINTER_MAIN_H
#define AMBROLIB_PRINTER_MAIN_H

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stddef.h>

#include <aprinter/meta/MapTypeList.h>
#include <aprinter/meta/TemplateFunc.h>
#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/WrapCallback.h>
#include <aprinter/meta/IndexElemTuple.h>
#include <aprinter/meta/TupleGet.h>
#include <aprinter/meta/TupleForEach.h>
#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/WrapDouble.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/TypeListLength.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/IndexElemList.h>
#include <aprinter/meta/Position.h>
#include <aprinter/meta/TuplePosition.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/JoinTypeLists.h>
#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/Union.h>
#include <aprinter/meta/UnionGet.h>
#include <aprinter/meta/GetMemberTypeFunc.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/OffsetCallback.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Likely.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/system/AvrSerial.h>
#include <aprinter/devices/Blinker.h>
#include <aprinter/devices/SoftPwm.h>
#include <aprinter/stepper/Steppers.h>
#include <aprinter/stepper/AxisStepper.h>
#include <aprinter/printer/AxisHomer.h>
#include <aprinter/printer/GcodeParser.h>
#include <aprinter/printer/MotionPlanner.h>
#include <aprinter/printer/TemperatureObserver.h>

#include <aprinter/BeginNamespace.h>

template <
    typename TSerial, typename TLedPin, typename TLedBlinkInterval, typename TDefaultInactiveTime,
    typename TSpeedLimitMultiply, typename TMaxStepsPerCycle,
    int TStepperSegmentBufferSize, int TEventChannelBufferSize, int TLookaheadBufferSizeExp,
    typename TForceTimeout, template <typename, typename> class TEventChannelTimer,
    template <typename, typename> class TWatchdogTemplate, typename TWatchdogParams,
    typename TAxesList, typename THeatersList, typename TFansList
>
struct PrinterMainParams {
    using Serial = TSerial;
    using LedPin = TLedPin;
    using LedBlinkInterval = TLedBlinkInterval;
    using DefaultInactiveTime = TDefaultInactiveTime;
    using SpeedLimitMultiply = TSpeedLimitMultiply;
    using MaxStepsPerCycle = TMaxStepsPerCycle;
    static const int StepperSegmentBufferSize = TStepperSegmentBufferSize;
    static const int EventChannelBufferSize = TEventChannelBufferSize;
    static const int LookaheadBufferSizeExp = TLookaheadBufferSizeExp;
    using ForceTimeout = TForceTimeout;
    template <typename X, typename Y> using EventChannelTimer = TEventChannelTimer<X, Y>;
    template <typename X, typename Y> using WatchdogTemplate = TWatchdogTemplate<X, Y>;
    using WatchdogParams = TWatchdogParams;
    using AxesList = TAxesList;
    using HeatersList = THeatersList;
    using FansList = TFansList;
};

template <uint32_t tbaud, typename TTheGcodeParserParams>
struct PrinterMainSerialParams {
    static const uint32_t baud = tbaud;
    using TheGcodeParserParams = TTheGcodeParserParams;
};

template <
    char tname,
    typename TDirPin, typename TStepPin, typename TEnablePin, bool TInvertDir,
    typename TDefaultStepsPerUnit, typename TDefaultMin, typename TDefaultMax,
    typename TDefaultMaxSpeed, typename TDefaultMaxAccel,
    typename TDefaultDistanceFactor, typename TDefaultCorneringDistance,
    typename THoming, bool tenable_cartesian_speed_limit, int TStepBits,
    typename TTheAxisStepperParams
>
struct PrinterMainAxisParams {
    static const char name = tname;
    using DirPin = TDirPin;
    using StepPin = TStepPin;
    using EnablePin = TEnablePin;
    static const bool InvertDir = TInvertDir;
    using DefaultStepsPerUnit = TDefaultStepsPerUnit;
    using DefaultMin = TDefaultMin;
    using DefaultMax = TDefaultMax;
    using DefaultMaxSpeed = TDefaultMaxSpeed;
    using DefaultMaxAccel = TDefaultMaxAccel;
    using DefaultDistanceFactor = TDefaultDistanceFactor;
    using DefaultCorneringDistance = TDefaultCorneringDistance;
    using Homing = THoming;
    static const bool enable_cartesian_speed_limit = tenable_cartesian_speed_limit;
    static const int StepBits = TStepBits;
    using TheAxisStepperParams = TTheAxisStepperParams;
};

struct PrinterMainNoHomingParams {
    static const bool enabled = false;
};

template <
    typename TEndPin, bool tend_invert, bool thome_dir,
    typename TDefaultFastMaxDist, typename TDefaultRetractDist, typename TDefaultSlowMaxDist,
    typename TDefaultFastSpeed, typename TDefaultRetractSpeed, typename TDefaultSlowSpeed
>
struct PrinterMainHomingParams {
    static const bool enabled = true;
    using EndPin = TEndPin;
    static const bool end_invert = tend_invert;
    static const bool home_dir = thome_dir;
    using DefaultFastMaxDist = TDefaultFastMaxDist;
    using DefaultRetractDist = TDefaultRetractDist;
    using DefaultSlowMaxDist = TDefaultSlowMaxDist;
    using DefaultFastSpeed = TDefaultFastSpeed;
    using DefaultRetractSpeed = TDefaultRetractSpeed;
    using DefaultSlowSpeed = TDefaultSlowSpeed;
};

template <
    char TName, int TSetMCommand, int TWaitMCommand,
    typename TAdcPin, typename TOutputPin,
    typename TFormula,
    typename TMinSafeTemp, typename TMaxSafeTemp,
    typename TPulseInterval,
    template<typename, typename, typename> class TControl,
    typename TControlParams,
    typename TTheTemperatureObserverParams,
    template<typename, typename> class TTimerTemplate
>
struct PrinterMainHeaterParams {
    static const char Name = TName;
    static const int SetMCommand = TSetMCommand;
    static const int WaitMCommand = TWaitMCommand;
    using AdcPin = TAdcPin;
    using OutputPin = TOutputPin;
    using Formula = TFormula;
    using MinSafeTemp = TMinSafeTemp;
    using MaxSafeTemp = TMaxSafeTemp;
    using PulseInterval = TPulseInterval;
    template <typename X, typename Y, typename Z> using Control = TControl<X, Y, Z>;
    using ControlParams = TControlParams;
    using TheTemperatureObserverParams = TTheTemperatureObserverParams;
    template <typename X, typename Y> using TimerTemplate = TTimerTemplate<X, Y>;
};

template <
    int TSetMCommand, int TOffMCommand,
    typename TOutputPin, typename TPulseInterval, typename TSpeedMultiply,
    template<typename, typename> class TTimerTemplate
>
struct PrinterMainFanParams {
    static const int SetMCommand = TSetMCommand;
    static const int OffMCommand = TOffMCommand;
    using OutputPin = TOutputPin;
    using PulseInterval = TPulseInterval;
    using SpeedMultiply = TSpeedMultiply;
    template <typename X, typename Y> using TimerTemplate = TTimerTemplate<X, Y>;
};

template <typename Position, typename Context, typename Params>
class PrinterMain
: private DebugObject<Context, void>
{
private:
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_init, init)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_deinit, deinit)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_start_homing, start_homing)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_update_homing_mask, update_homing_mask)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_enable_stepper, enable_stepper)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_init_new_pos, init_new_pos)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_collect_new_pos, collect_new_pos)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_process_new_pos, process_new_pos)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_append_position, append_position)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_set_relative_positioning, set_relative_positioning)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_set_position, set_position)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_append_value, append_value)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_check_command, check_command)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_emergency, emergency)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_channel_callback, channel_callback)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_ChannelPayload, ChannelPayload)
    
    template <int AxisIndex> struct AxisPosition;
    template <int AxisIndex> struct HomingFeaturePosition;
    template <int AxisIndex> struct HomingStatePosition;
    struct PlannerPosition;
    template <int HeaterIndex> struct HeaterPosition;
    template <int FanIndex> struct FanPosition;
    
    struct BlinkerHandler;
    struct SerialRecvHandler;
    struct SerialSendHandler;
    template <int AxisIndex> struct PlannerGetAxisStepper;
    struct PlannerPullHandler;
    struct PlannerFinishedHandler;
    struct PlannerChannelCallback;
    template <int AxisIndex> struct AxisStepperConsumersList;
    
    static const int serial_recv_buffer_bits = 6;
    static const int serial_send_buffer_bits = 8;
    
    using Loop = typename Context::EventLoop;
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using AxesList = typename Params::AxesList;
    using HeatersList = typename Params::HeatersList;
    using FansList = typename Params::FansList;
    static const int num_axes = TypeListLength<AxesList>::value;
    using AxisMaskType = typename ChooseInt<num_axes, false>::Type;
    using AxisCountType = typename ChooseInt<BitsInInt<num_axes>::value, false>::Type;
    
    template <typename TheAxis>
    using MakeStepperDef = StepperDef<
        typename TheAxis::DirPin,
        typename TheAxis::StepPin,
        typename TheAxis::EnablePin,
        TheAxis::InvertDir
    >;
    
    using TheWatchdog = typename Params::template WatchdogTemplate<Context, typename Params::WatchdogParams>;
    using TheBlinker = Blinker<Context, typename Params::LedPin, BlinkerHandler>;
    using StepperDefsList = MapTypeList<AxesList, TemplateFunc<MakeStepperDef>>;
    using TheSteppers = Steppers<Context, StepperDefsList>;
    using TheSerial = AvrSerial<Context, serial_recv_buffer_bits, serial_send_buffer_bits, SerialRecvHandler, SerialSendHandler>;
    using RecvSizeType = typename TheSerial::RecvSizeType;
    using SendSizeType = typename TheSerial::SendSizeType;
    using TheGcodeParser = GcodeParser<Context, typename Params::Serial::TheGcodeParserParams, typename RecvSizeType::IntType>;
    using GcodeParserCommand = typename TheGcodeParser::Command;
    using GcodeParserCommandPart = typename TheGcodeParser::CommandPart;
    using GcodePartsSizeType = typename TheGcodeParser::PartsSizeType;
    
    static_assert(Params::LedBlinkInterval::value() < TheWatchdog::WatchdogTime / 2.0, "");
    
    template <int TAxisIndex>
    struct Axis {
        static const int AxisIndex = TAxisIndex;
        friend PrinterMain;
        
        struct AxisStepperPosition;
        struct AxisStepperGetStepperHandler;
        
        using AxisSpec = TypeListGet<AxesList, AxisIndex>;
        using Stepper = SteppersStepper<Context, StepperDefsList, AxisIndex>;
        using TheAxisStepper = AxisStepper<AxisStepperPosition, Context, typename AxisSpec::TheAxisStepperParams, Stepper, AxisStepperGetStepperHandler, AxisStepperConsumersList<AxisIndex>>;
        using StepFixedType = FixedPoint<AxisSpec::StepBits, false, 0>;
        using AbsStepFixedType = FixedPoint<AxisSpec::StepBits - 1, true, 0>;
        static const char axis_name = AxisSpec::name;
        
        AMBRO_STRUCT_IF(HomingFeature, AxisSpec::Homing::enabled) {
            struct HomingState {
                struct HomerPosition;
                struct HomerGetAxisStepper;
                struct HomerFinishedHandler;
                
                using Homer = AxisHomer<
                    HomerPosition, Context, TheAxisStepper, AxisSpec::StepBits,
                    typename AxisSpec::DefaultDistanceFactor, typename AxisSpec::DefaultCorneringDistance,
                    Params::StepperSegmentBufferSize, Params::LookaheadBufferSizeExp, typename AxisSpec::Homing::EndPin,
                    AxisSpec::Homing::end_invert, AxisSpec::Homing::home_dir, HomerGetAxisStepper, HomerFinishedHandler
                >;
                
                Axis * get_axis ()
                {
                    return PositionTraverse<HomingStatePosition<AxisIndex>, AxisPosition<AxisIndex>>(this);
                }
                
                TheAxisStepper * homer_get_axis_stepper ()
                {
                    return &get_axis()->m_axis_stepper;
                }
                
                void homer_finished_handler (Context c, bool success)
                {
                    Axis *axis = get_axis();
                    PrinterMain *o = PositionTraverse<HomingStatePosition<AxisIndex>, Position>(this);
                    AMBRO_ASSERT(axis->m_state == AXIS_STATE_HOMING)
                    AMBRO_ASSERT(o->m_state == STATE_HOMING)
                    AMBRO_ASSERT(o->m_homing_rem_axes > 0)
                    
                    m_homer.deinit(c);
                    axis->m_req_pos = (AxisSpec::Homing::home_dir ? axis->max_req_pos() : axis->min_req_pos());
                    axis->m_end_pos = AbsStepFixedType::importDoubleSaturatedRound(axis->dist_from_real(axis->m_req_pos));
                    axis->m_state = AXIS_STATE_OTHER;
                    o->m_homing_rem_axes--;
                    if (!success) {
                        o->m_homing_failed = true;
                    }
                    if (o->m_homing_rem_axes == 0) {
                        o->homing_finished(c);
                    }
                }
                
                Homer m_homer;
                
                struct HomerPosition : public MemberPosition<HomingStatePosition<AxisIndex>, Homer, &HomingState::m_homer> {};
                struct HomerGetAxisStepper : public AMBRO_WCALLBACK_TD(&HomingState::homer_get_axis_stepper, &HomingState::m_homer) {};
                struct HomerFinishedHandler : public AMBRO_WCALLBACK_TD(&HomingState::homer_finished_handler, &HomingState::m_homer) {};
            };
            
            template <typename TheHomingFeature>
            using MakeAxisStepperConsumersList = MakeTypeList<typename TheHomingFeature::HomingState::Homer::TheAxisStepperConsumer>;
            
            Axis * parent ()
            {
                return PositionTraverse<HomingFeaturePosition<AxisIndex>, AxisPosition<AxisIndex>>(this);
            }
            
            HomingState * homing_state ()
            {
                return PositionTraverse<HomingFeaturePosition<AxisIndex>, HomingStatePosition<AxisIndex>>(this);
            }
            
            void init (Context c)
            {
            }
            
            void deinit (Context c)
            {
                Axis *axis = parent();
                if (axis->m_state == AXIS_STATE_HOMING) {
                    homing_state()->m_homer.deinit(c);
                }
            }
            
            void start_homing (Context c, AxisMaskType mask)
            {
                Axis *axis = parent();
                PrinterMain *o = axis->parent();
                AMBRO_ASSERT(axis->m_state == AXIS_STATE_OTHER)
                
                if (!(mask & ((AxisMaskType)1 << AxisIndex))) {
                    return;
                }
                
                typename HomingState::Homer::HomingParams params;
                params.fast_max_dist = StepFixedType::importDoubleSaturated(axis->dist_from_real(AxisSpec::Homing::DefaultFastMaxDist::value()));
                params.retract_dist = StepFixedType::importDoubleSaturated(axis->dist_from_real(AxisSpec::Homing::DefaultRetractDist::value()));
                params.slow_max_dist = StepFixedType::importDoubleSaturated(axis->dist_from_real(AxisSpec::Homing::DefaultSlowMaxDist::value()));
                params.fast_speed = axis->speed_from_real(AxisSpec::Homing::DefaultFastSpeed::value());
                params.retract_speed = axis->speed_from_real(AxisSpec::Homing::DefaultRetractSpeed::value());;
                params.slow_speed = axis->speed_from_real(AxisSpec::Homing::DefaultSlowSpeed::value());
                params.max_accel = axis->accel_from_real(AxisSpec::DefaultMaxAccel::value());
                
                axis->stepper()->enable(c, true);
                homing_state()->m_homer.init(c, params);
                axis->m_state = AXIS_STATE_HOMING;
                o->m_homing_rem_axes++;
            }
        } AMBRO_STRUCT_ELSE(HomingFeature) {
            struct HomingState {};
            template <typename TheHomingFeature>
            using MakeAxisStepperConsumersList = MakeTypeList<>;
            void init (Context c) {}
            void deinit (Context c) {}
            void start_homing (Context c, AxisMaskType mask) {}
        };
        
        enum {AXIS_STATE_OTHER, AXIS_STATE_HOMING};
        
        PrinterMain * parent ()
        {
            return PositionTraverse<AxisPosition<AxisIndex>, Position>(this);
        }
        
        double dist_from_real (double x)
        {
            return (x * AxisSpec::DefaultStepsPerUnit::value());
        }
        
        double dist_to_real (double x)
        {
            return (x * (1.0 / AxisSpec::DefaultStepsPerUnit::value()));
        }
        
        double speed_from_real (double v)
        {
            return (v * (AxisSpec::DefaultStepsPerUnit::value() / Clock::time_freq));
        }
        
        double accel_from_real (double a)
        {
            return (a * (AxisSpec::DefaultStepsPerUnit::value() / (Clock::time_freq * Clock::time_freq)));
        }
        
        double clamp_req_pos (double req)
        {
            return fmax(min_req_pos(), fmin(max_req_pos(), req));
        }
        
        double min_req_pos ()
        {
            return fmax(AxisSpec::DefaultMin::value(), dist_to_real(AbsStepFixedType::minValue().doubleValue()));
        }
        
        double max_req_pos ()
        {
            return fmin(AxisSpec::DefaultMax::value(), dist_to_real(AbsStepFixedType::maxValue().doubleValue()));
        }
        
        void init (Context c)
        {
            m_axis_stepper.init(c);
            m_state = AXIS_STATE_OTHER;
            m_homing_feature.init(c);
            m_req_pos = clamp_req_pos(0.0);
            m_end_pos = AbsStepFixedType::importDoubleSaturatedRound(dist_from_real(m_req_pos));
            m_relative_positioning = false;
        }
        
        void deinit (Context c)
        {
            m_homing_feature.deinit(c);
            m_axis_stepper.deinit(c);
        }
        
        void start_homing (Context c, AxisMaskType mask)
        {
            return m_homing_feature.start_homing(c, mask);
        }
        
        void update_homing_mask (AxisMaskType *mask, GcodeParserCommandPart *part)
        {
            if (AxisSpec::Homing::enabled && part->code == axis_name) {
                *mask |= (AxisMaskType)1 << AxisIndex;
            }
        }
        
        void enable_stepper (Context c, bool enable)
        {
            stepper()->enable(c, enable);
        }
        
        Stepper * stepper ()
        {
            return parent()->m_steppers.template getStepper<AxisIndex>();
        }
        
        void init_new_pos (double *new_pos)
        {
            new_pos[AxisIndex] = m_req_pos;
        }
        
        void collect_new_pos (double *new_pos, GcodeParserCommandPart *part)
        {
            if (AMBRO_UNLIKELY(part->code == axis_name)) {
                double req = strtod(part->data, NULL);
                if (isnan(req)) {
                    req = 0.0;
                }
                if (m_relative_positioning) {
                    req += m_req_pos;
                }
                req = clamp_req_pos(req);
                new_pos[AxisIndex] = req;
            }
        }
        
        template <typename PlannerCmd>
        void process_new_pos (Context c, double *new_pos, double *distance_squared, double *total_steps, PlannerCmd *cmd)
        {
            AbsStepFixedType new_end_pos = AbsStepFixedType::importDoubleSaturatedRound(dist_from_real(new_pos[AxisIndex]));
            bool dir = (new_end_pos >= m_end_pos);
            StepFixedType move = StepFixedType::importBits(dir ? 
                ((typename StepFixedType::IntType)new_end_pos.bitsValue() - (typename StepFixedType::IntType)m_end_pos.bitsValue()) :
                ((typename StepFixedType::IntType)m_end_pos.bitsValue() - (typename StepFixedType::IntType)new_end_pos.bitsValue())
            );
            if (AMBRO_UNLIKELY(move.bitsValue() != 0)) {
                if (AxisSpec::enable_cartesian_speed_limit) {
                    double delta = dist_to_real(move.doubleValue());
                    *distance_squared += delta * delta;
                }
                *total_steps += move.doubleValue();
                enable_stepper(c, true);
            }
            auto *mycmd = TupleGetElem<AxisIndex>(&cmd->axes);
            mycmd->dir = dir;
            mycmd->x = move;
            mycmd->max_v_rec = 1.0 / speed_from_real(AxisSpec::DefaultMaxSpeed::value());
            mycmd->max_a_rec = 1.0 / accel_from_real(AxisSpec::DefaultMaxAccel::value());
            m_end_pos = new_end_pos;
            m_req_pos = new_pos[AxisIndex];
        }
        
        void append_position (Context c)
        {
            PrinterMain *o = parent();
            o->reply_append_ch(c, axis_name);
            o->reply_append_ch(c, ':');
            o->reply_append_double(c, m_req_pos);
        }
        
        void set_relative_positioning (bool relative)
        {
            m_relative_positioning = relative;
        }
        
        void set_position (Context c, GcodeParserCommandPart *part, bool *found_axes)
        {
            if (part->code == axis_name) {
                *found_axes = true;
                if (AxisSpec::Homing::enabled) {
                    parent()->reply_append_str(c, "Error:G92 on homable axis\n");
                    return;
                }
                double req = strtod(part->data, NULL);
                if (isnan(req)) {
                    req = 0.0;
                }
                m_req_pos = clamp_req_pos(req);
                m_end_pos = AbsStepFixedType::importDoubleSaturatedRound(dist_from_real(m_req_pos));
            }
        }
        
        static void emergency ()
        {
            Stepper::emergency();
        }
        
        TheAxisStepper m_axis_stepper;
        uint8_t m_state;
        HomingFeature m_homing_feature;
        AbsStepFixedType m_end_pos;
        double m_req_pos;
        bool m_relative_positioning;
        
        struct AxisStepperPosition : public MemberPosition<AxisPosition<AxisIndex>, TheAxisStepper, &Axis::m_axis_stepper> {};
        struct AxisStepperGetStepperHandler : public AMBRO_WCALLBACK_TD(&Axis::stepper, &Axis::m_axis_stepper) {};
    };
    
    template <typename TheAxis>
    using MakePlannerAxisSpec = MotionPlannerAxisSpec<
        typename TheAxis::TheAxisStepper,
        PlannerGetAxisStepper<TheAxis::AxisIndex>,
        TheAxis::AxisSpec::StepBits,
        typename TheAxis::AxisSpec::DefaultDistanceFactor,
        typename TheAxis::AxisSpec::DefaultCorneringDistance,
        void
    >;
    
    template <int HeaterIndex>
    struct Heater {
        struct SoftPwmTimerHandler;
        struct ObserverGetValueCallback;
        struct ObserverHandler;
        
        using HeaterSpec = TypeListGet<HeatersList, HeaterIndex>;
        using ValueFixedType = typename HeaterSpec::Formula::ValueFixedType;
        using TheControl = typename HeaterSpec::template Control<typename HeaterSpec::ControlParams, typename HeaterSpec::PulseInterval, ValueFixedType>;
        using TheSoftPwm = SoftPwm<Context, typename HeaterSpec::OutputPin, typename HeaterSpec::PulseInterval, SoftPwmTimerHandler, HeaterSpec::template TimerTemplate>;
        using TheObserver = TemperatureObserver<Context, typename HeaterSpec::TheTemperatureObserverParams, ObserverGetValueCallback, ObserverHandler>;
        using OutputFixedType = typename TheControl::OutputFixedType;
        
        struct ChannelPayload {
            ValueFixedType target;
        };
        
        static ValueFixedType min_safe_temp ()
        {
            return ValueFixedType::importDoubleSaturatedInline(HeaterSpec::MinSafeTemp::value());
        }
        
        static ValueFixedType max_safe_temp ()
        {
            return ValueFixedType::importDoubleSaturatedInline(HeaterSpec::MaxSafeTemp::value());
        }
        
        PrinterMain * parent ()
        {
            return PositionTraverse<HeaterPosition<HeaterIndex>, Position>(this);
        }
        
        void init (Context c)
        {
            m_lock.init(c);
            m_enabled = false;
            m_softpwm.init(c, c.clock()->getTime(c));
            m_observing = false;
        }
        
        void deinit (Context c)
        {
            if (m_observing) {
                m_observer.deinit(c);
            }
            m_softpwm.deinit(c);
            m_lock.deinit(c);
        }
        
        template <typename ThisContext>
        ValueFixedType get_value (ThisContext c)
        {
            return HeaterSpec::Formula::call(c.adc()->template getValue<typename HeaterSpec::AdcPin>(c));
        }
        
        void append_value (PrinterMain *o, Context c)
        {
            double value = get_value(c).doubleValue();
            o->reply_append_ch(c, ' ');
            o->reply_append_ch(c, HeaterSpec::Name);
            o->reply_append_ch(c, ':');
            o->reply_append_double(c, value);
        }
        
        template <typename ThisContext>
        void set (ThisContext c, ValueFixedType target)
        {
            AMBRO_ASSERT(target > min_safe_temp())
            AMBRO_ASSERT(target < max_safe_temp())
            
            AMBRO_LOCK_T(m_lock, c, lock_c, {
                if (!m_enabled) {
                    m_control.init(target);
                    m_enabled = true;
                } else {
                    m_control.setTarget(target);
                }
            });
        }
        
        template <typename ThisContext>
        void unset (ThisContext c)
        {
            AMBRO_LOCK_T(m_lock, c, lock_c, {
                m_enabled = false;
            });
        }
        
        bool check_command (Context c, int cmd_num, int *out_result)
        {
            PrinterMain *o = parent();
            
            if (cmd_num == HeaterSpec::WaitMCommand) {
                if (o->m_state == STATE_PLANNING) {
                    *out_result = CMD_WAIT_PLANNER;
                    return false;
                }
                double target = o->get_command_param_double(o->m_cmd, 'S', 0.0);
                ValueFixedType fixed_target = ValueFixedType::importDoubleSaturated(target);
                if (fixed_target > min_safe_temp() && fixed_target < max_safe_temp()) {
                    set(c, fixed_target);
                } else {
                    unset(c);
                }
                AMBRO_ASSERT(!m_observing)
                m_observer.init(c, target);
                m_observing = true;
                o->m_state = STATE_WAITING_TEMP;
                o->now_active(c);
                *out_result = CMD_DELAY;
                return false;
            }
            if (cmd_num == HeaterSpec::SetMCommand) {
                if (!o->try_buffered_command(c)) {
                    *out_result = CMD_DELAY;
                    return false;
                }
                double target = o->get_command_param_double(o->m_cmd, 'S', 0.0);
                ValueFixedType fixed_target = ValueFixedType::importDoubleSaturated(target);
                if (!(fixed_target > min_safe_temp() && fixed_target < max_safe_temp())) {
                    fixed_target = ValueFixedType::minValue();
                }
                PlannerInputCommand cmd;
                cmd.type = 1;
                PlannerChannelPayload *payload = UnionGetElem<0>(&cmd.channel_payload);
                payload->type = HeaterIndex;
                UnionGetElem<HeaterIndex>(&payload->heaters)->target = fixed_target;
                o->finish_command(c, false);
                o->finish_buffered_command(c, &cmd);
                *out_result = CMD_DELAY;
                return false;
            }
            return true;
        }
        
        OutputFixedType softpwm_timer_handler (typename TheSoftPwm::TimerInstance::HandlerContext c)
        {
            OutputFixedType control_value = OutputFixedType::importBits(0);
            AMBRO_LOCK_T(m_lock, c, lock_c, {
                ValueFixedType sensor_value = get_value(lock_c);
                if (AMBRO_UNLIKELY(sensor_value <= min_safe_temp() || sensor_value >= max_safe_temp())) {
                    m_enabled = false;
                }
                if (AMBRO_LIKELY(m_enabled)) {
                    control_value = m_control.addMeasurement(sensor_value);
                }
            });
            return control_value;
        }
        
        double observer_get_value_callback (Context c)
        {
            return get_value(c).doubleValue();
        }
        
        void observer_handler (Context c, bool state)
        {
            PrinterMain *o = parent();
            AMBRO_ASSERT(m_observing)
            AMBRO_ASSERT(o->m_state == STATE_WAITING_TEMP)
            
            if (!state) {
                return;
            }
            m_observer.deinit(c);
            m_observing = false;
            o->m_state = STATE_IDLE;
            o->finish_command(c, false);
            o->now_inactive(c);
        }
        
        static void emergency ()
        {
            Context::Pins::template emergencySet<typename HeaterSpec::OutputPin>(false);
        }
        
        template <typename ThisContext, typename TheChannelPayloadUnion>
        void channel_callback (ThisContext c, TheChannelPayloadUnion *payload_union)
        {
            ChannelPayload *payload = UnionGetElem<HeaterIndex>(payload_union);
            if (AMBRO_LIKELY(payload->target != ValueFixedType::minValue())) {
                set(c, payload->target);
            } else {
                unset(c);
            }
        }
        
        typename Context::Lock m_lock;
        bool m_enabled;
        TheControl m_control;
        TheSoftPwm m_softpwm;
        TheObserver m_observer;
        bool m_observing;
        
        struct SoftPwmTimerHandler : public AMBRO_WCALLBACK_TD(&Heater::softpwm_timer_handler, &Heater::m_softpwm) {};
        struct ObserverGetValueCallback : public AMBRO_WCALLBACK_TD(&Heater::observer_get_value_callback, &Heater::m_observer) {};
        struct ObserverHandler : public AMBRO_WCALLBACK_TD(&Heater::observer_handler, &Heater::m_observer) {};
    };
    
    template <int FanIndex>
    struct Fan {
        struct SoftPwmTimerHandler;
        
        using FanSpec = TypeListGet<FansList, FanIndex>;
        using TheSoftPwm = SoftPwm<Context, typename FanSpec::OutputPin, typename FanSpec::PulseInterval, SoftPwmTimerHandler, FanSpec::template TimerTemplate>;
        using OutputFixedType = FixedPoint<16, false, -16>;
        
        struct ChannelPayload {
            OutputFixedType target;
        };
        
        PrinterMain * parent ()
        {
            return PositionTraverse<FanPosition<FanIndex>, Position>(this);
        }
        
        void init (Context c)
        {
            m_lock.init(c);
            m_target = OutputFixedType::importBits(0);
            m_softpwm.init(c, c.clock()->getTime(c));
        }
        
        void deinit (Context c)
        {
            m_softpwm.deinit(c);
            m_lock.deinit(c);
        }
        
        bool check_command (Context c, int cmd_num, int *out_result)
        {
            PrinterMain *o = parent();
            
            if (cmd_num == FanSpec::SetMCommand || cmd_num == FanSpec::OffMCommand) {
                if (!o->try_buffered_command(c)) {
                    *out_result = CMD_DELAY;
                    return false;
                }
                double target = 0.0;
                if (cmd_num == FanSpec::SetMCommand) {
                    target = 1.0;
                    if (o->find_command_param_double(o->m_cmd, 'S', &target)) {
                        target *= FanSpec::SpeedMultiply::value();
                    }
                }
                PlannerInputCommand cmd;
                cmd.type = 1;
                PlannerChannelPayload *payload = UnionGetElem<0>(&cmd.channel_payload);
                payload->type = TypeListLength<HeatersList>::value + FanIndex;
                UnionGetElem<FanIndex>(&payload->fans)->target = OutputFixedType::importDoubleSaturated(target);
                o->finish_command(c, false);
                o->finish_buffered_command(c, &cmd);
                *out_result = CMD_DELAY;
                return false;
            }
            return true;
        }
        
        OutputFixedType softpwm_timer_handler (typename TheSoftPwm::TimerInstance::HandlerContext c)
        {
            OutputFixedType control_value;
            AMBRO_LOCK_T(m_lock, c, lock_c, {
                control_value = m_target;
            });
            return control_value;
        }
        
        static void emergency ()
        {
            Context::Pins::template emergencySet<typename FanSpec::OutputPin>(false);
        }
        
        template <typename ThisContext, typename TheChannelPayloadUnion>
        void channel_callback (ThisContext c, TheChannelPayloadUnion *payload_union)
        {
            ChannelPayload *payload = UnionGetElem<FanIndex>(payload_union);
            AMBRO_LOCK_T(m_lock, c, lock_c, {
                m_target = payload->target;
            });
        }
        
        typename Context::Lock m_lock;
        OutputFixedType m_target;
        TheSoftPwm m_softpwm;
        
        struct SoftPwmTimerHandler : public AMBRO_WCALLBACK_TD(&Fan::softpwm_timer_handler, &Fan::m_softpwm) {};
    };
    
    using HeatersTuple = IndexElemTuple<HeatersList, Heater>;
    using FansTuple = IndexElemTuple<FansList, Fan>;
    
    using HeatersChannelPayloadUnion = Union<MapTypeList<typename HeatersTuple::ElemTypes, GetMemberType_ChannelPayload>>;
    using FansChannelPayloadUnion = Union<MapTypeList<typename FansTuple::ElemTypes, GetMemberType_ChannelPayload>>;
    
    struct PlannerChannelPayload {
        uint8_t type;
        union {
            HeatersChannelPayloadUnion heaters;
            FansChannelPayloadUnion fans;
        };
    };
    
    using MotionPlannerChannels = MakeTypeList<MotionPlannerChannelSpec<PlannerChannelPayload, PlannerChannelCallback, Params::EventChannelBufferSize, Params::template EventChannelTimer>>;
    using MotionPlannerAxes = MapTypeList<IndexElemList<AxesList, Axis>, TemplateFunc<MakePlannerAxisSpec>>;
    using ThePlanner = MotionPlanner<PlannerPosition, Context, MotionPlannerAxes, Params::StepperSegmentBufferSize, Params::LookaheadBufferSizeExp, PlannerPullHandler, PlannerFinishedHandler, MotionPlannerChannels>;
    using PlannerInputCommand = typename ThePlanner::InputCommand;
    using AxesTuple = IndexElemTuple<AxesList, Axis>;
    
    template <int AxisIndex>
    using HomingStateTupleHelper = typename Axis<AxisIndex>::HomingFeature::HomingState;
    using HomingStateTuple = IndexElemTuple<AxesList, HomingStateTupleHelper>;
    
public:
    void init (Context c)
    {
        m_watchdog.init(c);
        m_blinker.init(c, Params::LedBlinkInterval::value() * Clock::time_freq);
        m_steppers.init(c);
        TupleForEachForward(&m_axes, Foreach_init(), c);
        m_serial.init(c, Params::Serial::baud);
        m_gcode_parser.init(c);
        m_disable_timer.init(c, AMBRO_OFFSET_CALLBACK_T(&PrinterMain::m_disable_timer, &PrinterMain::disable_timer_handler));
        m_force_timer.init(c, AMBRO_OFFSET_CALLBACK_T(&PrinterMain::m_force_timer, &PrinterMain::force_timer_handler));
        TupleForEachForward(&m_heaters, Foreach_init(), c);
        TupleForEachForward(&m_fans, Foreach_init(), c);
        m_inactive_time = Params::DefaultInactiveTime::value() * Clock::time_freq;
        m_recv_next_error = 0;
        m_line_number = 0;
        m_cmd = NULL;
        m_max_cart_speed = INFINITY;
        m_state = STATE_IDLE;
        
        reply_append_str(c, "APrinter\n");
        
        this->debugInit(c);
    }

    void deinit (Context c)
    {
        this->debugDeinit(c);
        
        if (m_state == STATE_PLANNING) {
            m_planner.deinit(c);
        }
        TupleForEachReverse(&m_fans, Foreach_deinit(), c);
        TupleForEachReverse(&m_heaters, Foreach_deinit(), c);
        m_force_timer.deinit(c);
        m_disable_timer.deinit(c);
        m_gcode_parser.deinit(c);
        m_serial.deinit(c);
        TupleForEachReverse(&m_axes, Foreach_deinit(), c);
        m_steppers.deinit(c);
        m_blinker.deinit(c);
        m_watchdog.deinit(c);
    }
    
    TheSerial * getSerial ()
    {
        return &m_serial;
    }
    
    template <int AxisIndex>
    typename Axis<AxisIndex>::TheAxisStepper * getAxisStepper ()
    {
        return &PositionTraverse<Position, AxisPosition<AxisIndex>>(this)->m_axis_stepper;
    }
    
    template <int HeaterIndex>
    typename Heater<HeaterIndex>::TheSoftPwm::TimerInstance * getHeaterTimer ()
    {
        return PositionTraverse<Position, HeaterPosition<HeaterIndex>>(this)->m_softpwm.getTimer();
    }
    
    template <int FanIndex>
    typename Fan<FanIndex>::TheSoftPwm::TimerInstance * getFanTimer ()
    {
        return PositionTraverse<Position, FanPosition<FanIndex>>(this)->m_softpwm.getTimer();
    }
    
    typename ThePlanner::template Channel<0>::TheTimer * getEventChannelTimer ()
    {
        return m_planner.template getChannelTimer<0>();
    }
    
    static void emergency ()
    {
        AxesTuple dummy_axes;
        TupleForEachForward(&dummy_axes, Foreach_emergency());
        HeatersTuple dummy_heaters;
        TupleForEachForward(&dummy_heaters, Foreach_emergency());
        FansTuple dummy_fans;
        TupleForEachForward(&dummy_fans, Foreach_emergency());
    }
    
private:
    enum {STATE_IDLE, STATE_HOMING, STATE_PLANNING, STATE_WAITING_TEMP};
    enum {CMD_REPLY, CMD_WAIT_PLANNER, CMD_DELAY};
    
    static TimeType time_from_real (double t)
    {
        return (FixedPoint<30, false, 0>::importDoubleSaturated(t * Clock::time_freq)).bitsValue();
    }
    
    void blinker_handler (Context c)
    {
        this->debugAccess(c);
        
        m_watchdog.reset(c);
    }
    
    void serial_recv_handler (Context c)
    {
        this->debugAccess(c);
        
        if (m_cmd) {
            return;
        }
        if (!m_gcode_parser.haveCommand(c)) {
            m_gcode_parser.startCommand(c, m_serial.recvGetChunkPtr(c), m_recv_next_error);
            m_recv_next_error = 0;
        }
        bool overrun;
        RecvSizeType avail = m_serial.recvQuery(c, &overrun);
        m_cmd = m_gcode_parser.extendCommand(c, avail.value());
        if (m_cmd) {
            return process_received_command(c, false);
        }
        if (overrun) {
            m_serial.recvConsume(c, avail);
            m_serial.recvClearOverrun(c);
            m_gcode_parser.resetCommand(c);
            m_recv_next_error = TheGcodeParser::ERROR_RECV_OVERRUN;
        }
    }
    
    void process_received_command (Context c, bool already_seen)
    {
        AMBRO_ASSERT(m_cmd)
        AMBRO_ASSERT(m_state == STATE_IDLE || m_state == STATE_PLANNING)
        
        bool no_ok = false;
        char cmd_code;
        int cmd_num;
        
        if (m_cmd->num_parts <= 0) {
            char const *err = "unknown error";
            switch (m_cmd->num_parts) {
                case 0: err = "empty command"; break;
                case TheGcodeParser::ERROR_TOO_MANY_PARTS: err = "too many parts"; break;
                case TheGcodeParser::ERROR_INVALID_PART: err = "invalid part"; break;
                case TheGcodeParser::ERROR_CHECKSUM: err = "incorrect checksum"; break;
                case TheGcodeParser::ERROR_RECV_OVERRUN: err = "receive buffer overrun"; break;
            }
            reply_append_str(c, "Error:");
            reply_append_str(c, err);
            reply_append_ch(c, '\n');
            goto reply;
        }
        
        cmd_code = m_cmd->parts[0].code;
        cmd_num = atoi(m_cmd->parts[0].data);
        
        if (!already_seen) {
            bool is_m110 = (cmd_code == 'M' && cmd_num == 110);
            if (is_m110) {
                m_line_number = get_command_param_uint32(m_cmd, 'L', (m_cmd->have_line_number ? m_cmd->line_number : -1));
            }
            if (m_cmd->have_line_number) {
                if (m_cmd->line_number != m_line_number) {
                    reply_append_str(c, "Error:Line Number is not Last Line Number+1, Last Line:");
                    reply_append_uint32(c, (uint32_t)(m_line_number - 1));
                    reply_append_ch(c, '\n');
                    goto reply;
                }
            }
            if (m_cmd->have_line_number || is_m110) {
                m_line_number++;
            }
        }
        
        switch (cmd_code) {
            case 'M': switch (cmd_num) {
                default:
                    int result;
                    if (!TupleForEachForwardInterruptible(&m_heaters, Foreach_check_command(), c, cmd_num, &result) ||
                        !TupleForEachForwardInterruptible(&m_fans, Foreach_check_command(), c, cmd_num, &result)
                    ) {
                        if (result == CMD_REPLY) {
                            goto reply;
                        } else if (result == CMD_WAIT_PLANNER) {
                            goto wait_planner;
                        } else if (result == CMD_DELAY) {
                            return;
                        }
                        AMBRO_ASSERT(0);
                    }
                    goto unknown_command;
                
                case 110: // set line number
                    break;
                
                case 17: {
                    if (m_state == STATE_PLANNING) {
                        goto wait_planner;
                    }
                    TupleForEachForward(&m_axes, Foreach_enable_stepper(), c, true);
                    now_inactive(c);
                } break;
                
                case 18: // disable steppers or set timeout
                case 84: {
                    if (m_state == STATE_PLANNING) {
                        goto wait_planner;
                    }
                    double inactive_time;
                    if (find_command_param_double(m_cmd, 'S', &inactive_time)) {
                        m_inactive_time = time_from_real(inactive_time);
                        if (m_disable_timer.isSet(c)) {
                            m_disable_timer.appendAt(c, m_last_active_time + m_inactive_time);
                        }
                    } else {
                        TupleForEachForward(&m_axes, Foreach_enable_stepper(), c, false);
                        m_disable_timer.unset(c);
                    }
                } break;
                
                case 105: {
                    reply_append_str(c, "ok");
                    TupleForEachForward(&m_heaters, Foreach_append_value(), this, c);
                    reply_append_ch(c, '\n');
                    no_ok = true;
                } break;
                
                case 114: {
                    TupleForEachForward(&m_axes, Foreach_append_position(), c);
                    reply_append_ch(c, '\n');
                } break;
            } break;
            
            case 'G': switch (cmd_num) {
                default:
                    goto unknown_command;
                
                case 0:
                case 1: { // buffered move
                    if (!try_buffered_command(c)) {
                        return;
                    }
                    double new_pos[num_axes];
                    TupleForEachForward(&m_axes, Foreach_init_new_pos(), new_pos);
                    for (GcodePartsSizeType i = 1; i < m_cmd->num_parts; i++) {
                        GcodeParserCommandPart *part = &m_cmd->parts[i];
                        TupleForEachForward(&m_axes, Foreach_collect_new_pos(), new_pos, part);
                        if (part->code == 'F') {
                            m_max_cart_speed = strtod(part->data, NULL) * Params::SpeedLimitMultiply::value();
                        }
                    }
                    finish_command(c, false);
                    PlannerInputCommand cmd;
                    double distance = 0.0;
                    double total_steps = 0.0;
                    TupleForEachForward(&m_axes, Foreach_process_new_pos(), c, new_pos, &distance, &total_steps, &cmd);
                    distance = sqrt(distance);
                    cmd.type = 0;
                    cmd.rel_max_v_rec = FloatMakePosOrPosZero(distance / (m_max_cart_speed * Clock::time_unit));
                    cmd.rel_max_v_rec = fmax(cmd.rel_max_v_rec, total_steps * (1.0 / (Params::MaxStepsPerCycle::value() * F_CPU * Clock::time_unit)));
                    finish_buffered_command(c, &cmd);
                    return;
                } break;
                
                case 21: // set units to millimeters
                    break;
                
                case 28: { // home axes
                    if (m_state == STATE_PLANNING) {
                        goto wait_planner;
                    }
                    AxisMaskType mask = 0;
                    for (GcodePartsSizeType i = 1; i < m_cmd->num_parts; i++) {
                        TupleForEachForward(&m_axes, Foreach_update_homing_mask(), &mask, &m_cmd->parts[i]);
                    }
                    if (mask == 0) {
                        mask = -1;
                    }
                    m_homing_rem_axes = 0;
                    m_homing_failed = false;
                    TupleForEachForward(&m_axes, Foreach_start_homing(), c, mask);
                    if (m_homing_rem_axes > 0) {
                        m_state = STATE_HOMING;
                        now_active(c);
                        return;
                    }
                } break;
                
                case 90: { // absolute positioning
                    TupleForEachForward(&m_axes, Foreach_set_relative_positioning(), false);
                } break;
                
                case 91: { // relative positioning
                    TupleForEachForward(&m_axes, Foreach_set_relative_positioning(), true);
                } break;
                
                case 92: { // set position
                    bool found_axes = false;
                    for (GcodePartsSizeType i = 1; i < m_cmd->num_parts; i++) {
                        TupleForEachForward(&m_axes, Foreach_set_position(), c, &m_cmd->parts[i], &found_axes);
                    }
                    if (!found_axes) {
                        reply_append_str(c, "Error:not supported\n");
                    }
                } break;
            } break;
            
            unknown_command:
            default: {
                reply_append_str(c, "Error:Unknown command ");
                reply_append_str(c, (m_cmd->parts[0].data - 1));
                reply_append_ch(c, '\n');
            } break;
        }
        
    reply:
        finish_command(c, no_ok);
        return;
        
    wait_planner:
        AMBRO_ASSERT(m_state == STATE_PLANNING)
        AMBRO_ASSERT(!m_planning_req_pending)
        if (m_planning_pull_pending) {
            m_planner.waitFinished(c);
        }
    }
    
    void finish_command (Context c, bool no_ok)
    {
        AMBRO_ASSERT(m_cmd)
        
        if (!no_ok) {
            reply_append_str(c, "ok\n");
        }
        
        m_serial.recvConsume(c, RecvSizeType::import(m_cmd->length));
        m_cmd = 0;
        m_serial.recvForceEvent(c);
    }
    
    void homing_finished (Context c)
    {
        AMBRO_ASSERT(m_state == STATE_HOMING)
        AMBRO_ASSERT(m_homing_rem_axes == 0)
        
        if (m_homing_failed) {
            reply_append_str(c, "Error:Homing failed\n");
        }
        m_state = STATE_IDLE;
        finish_command(c, false);
        now_inactive(c);
    }
    
    void now_inactive (Context c)
    {
        AMBRO_ASSERT(m_state == STATE_IDLE)
        
        m_last_active_time = c.clock()->getTime(c);
        m_disable_timer.appendAt(c, m_last_active_time + m_inactive_time);
    }
    
    void now_active (Context c)
    {
        m_disable_timer.unset(c);
    }
    
    static GcodeParserCommandPart * find_command_param (GcodeParserCommand *cmd, char code)
    {
        AMBRO_ASSERT(code >= 'A')
        AMBRO_ASSERT(code <= 'Z')
        
        for (GcodePartsSizeType i = 1; i < cmd->num_parts; i++) {
            if (cmd->parts[i].code == code) {
                return &cmd->parts[i];
            }
        }
        return NULL;
    }
    
    static uint32_t get_command_param_uint32 (GcodeParserCommand *cmd, char code, uint32_t default_value)
    {
        GcodeParserCommandPart *part = find_command_param(cmd, code);
        if (!part) {
            return default_value;
        }
        return strtoul(part->data, NULL, 10);
    }
    
    static double get_command_param_double (GcodeParserCommand *cmd, char code, double default_value)
    {
        GcodeParserCommandPart *part = find_command_param(cmd, code);
        if (!part) {
            return default_value;
        }
        return strtod(part->data, NULL);
    }
    
    static bool find_command_param_double (GcodeParserCommand *cmd, char code, double *out)
    {
        GcodeParserCommandPart *part = find_command_param(cmd, code);
        if (!part) {
            return false;
        }
        *out = strtod(part->data, NULL);
        return true;
    }
    
    void reply_append (Context c, char const *str, uint8_t length)
    {
        SendSizeType avail = m_serial.sendQuery(c);
        if (length > avail.value()) {
            length = avail.value();
        }
        while (length > 0) {
            char *chunk_data = m_serial.sendGetChunkPtr(c);
            uint8_t chunk_length = m_serial.sendGetChunkLen(c, SendSizeType::import(length)).value();
            memcpy(chunk_data, str, chunk_length);
            m_serial.sendProvide(c, SendSizeType::import(chunk_length));
            str += chunk_length;
            length -= chunk_length;
        }
    }
    
    void reply_append_str (Context c, char const *str)
    {
        reply_append(c, str, strlen(str));
    }
    
    void reply_append_ch (Context c, char ch)
    {
        if (m_serial.sendQuery(c).value() > 0) {
            *m_serial.sendGetChunkPtr(c) = ch;
            m_serial.sendProvide(c, SendSizeType::import(1));
        }
    }
    
    void reply_append_double (Context c, double x)
    {
        char buf[30];
        uint8_t len = sprintf(buf, "%g", x);
        reply_append(c, buf, len);
    }
    
    void reply_append_uint32 (Context c, uint32_t x)
    {
        char buf[11];
        uint8_t len = sprintf(buf, "%" PRIu32, x);
        reply_append(c, buf, len);
    }
    
    bool try_buffered_command (Context c)
    {
        AMBRO_ASSERT(m_cmd)
        AMBRO_ASSERT(m_state == STATE_IDLE || m_state == STATE_PLANNING)
        
        if (m_state != STATE_PLANNING) {
            m_planner.init(c, false);
            m_state = STATE_PLANNING;
            m_planning_pull_pending = false;
            now_active(c);
        }
        m_planning_req_pending = true;
        return m_planning_pull_pending;
    }
    
    void finish_buffered_command (Context c, PlannerInputCommand *cmd)
    {
        AMBRO_ASSERT(m_state == STATE_PLANNING)
        AMBRO_ASSERT(m_planning_req_pending)
        AMBRO_ASSERT(!m_cmd)
        AMBRO_ASSERT(m_planning_pull_pending)
        
        m_planner.commandDone(c, cmd);
        m_planning_req_pending = false;
        m_planning_pull_pending = false;
        m_force_timer.unset(c);
    }
    
    void serial_send_handler (Context c)
    {
        this->debugAccess(c);
    }
    
    void disable_timer_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_IDLE)
        
        TupleForEachForward(&m_axes, Foreach_enable_stepper(), c, false);
    }
    
    void force_timer_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_PLANNING)
        AMBRO_ASSERT(m_planning_pull_pending)
        AMBRO_ASSERT(!m_planning_req_pending)
        
        m_planner.waitFinished(c);
    }
    
    void planner_pull_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_PLANNING)
        AMBRO_ASSERT(!m_planning_pull_pending)
        AMBRO_ASSERT(!m_planning_req_pending || m_cmd)
        
        m_planning_pull_pending = true;
        if (m_planning_req_pending) {
            process_received_command(c, true);
        } else if (m_cmd) {
            m_planner.waitFinished(c);
        } else {
            TimeType force_time = c.clock()->getTime(c) + (TimeType)(Params::ForceTimeout::value() * Clock::time_freq);
            m_force_timer.appendAt(c, force_time);
        }
    }
    
    void planner_finished_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_PLANNING)
        AMBRO_ASSERT(m_planning_pull_pending)
        AMBRO_ASSERT(!m_planning_req_pending)
        
        if (!m_cmd) {
            return;
        }
        
        m_planner.deinit(c);
        m_force_timer.unset(c);
        m_state = STATE_IDLE;
        now_inactive(c);
        
        if (m_cmd) {
            process_received_command(c, true);
        }
    }
    
    void planner_channel_callback (typename ThePlanner::template Channel<0>::CallbackContext c, PlannerChannelPayload *payload)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_PLANNING)
        
        TupleForOneOffset<0>(payload->type, &m_heaters, Foreach_channel_callback(), c, &payload->heaters);
        TupleForOneOffset<TypeListLength<HeatersList>::value>(payload->type, &m_fans, Foreach_channel_callback(), c, &payload->fans);
    }
    
    TheWatchdog m_watchdog;
    TheBlinker m_blinker;
    TheSteppers m_steppers;
    AxesTuple m_axes;
    TheSerial m_serial;
    TheGcodeParser m_gcode_parser;
    typename Loop::QueuedEvent m_disable_timer;
    typename Loop::QueuedEvent m_force_timer;
    HeatersTuple m_heaters;
    FansTuple m_fans;
    TimeType m_inactive_time;
    TimeType m_last_active_time;
    int8_t m_recv_next_error;
    uint32_t m_line_number;
    GcodeParserCommand *m_cmd;
    double m_max_cart_speed;
    uint8_t m_state;
    union {
        struct {
            HomingStateTuple m_homers;
            AxisCountType m_homing_rem_axes;
            bool m_homing_failed;
        };
        struct {
            ThePlanner m_planner;
            bool m_planning_req_pending;
            bool m_planning_pull_pending;
        };
    };
    
    template <int AxisIndex> struct AxisPosition : public TuplePosition<Position, AxesTuple, &PrinterMain::m_axes, AxisIndex> {};
    template <int AxisIndex> struct HomingFeaturePosition : public MemberPosition<AxisPosition<AxisIndex>, typename Axis<AxisIndex>::HomingFeature, &Axis<AxisIndex>::m_homing_feature> {};
    template <int AxisIndex> struct HomingStatePosition : public TuplePosition<Position, HomingStateTuple, &PrinterMain::m_homers, AxisIndex> {};
    struct PlannerPosition : public MemberPosition<Position, ThePlanner, &PrinterMain::m_planner> {};
    template <int HeaterIndex> struct HeaterPosition : public TuplePosition<Position, HeatersTuple, &PrinterMain::m_heaters, HeaterIndex> {};
    template <int FanIndex> struct FanPosition : public TuplePosition<Position, FansTuple, &PrinterMain::m_fans, FanIndex> {};
    
    struct BlinkerHandler : public AMBRO_WCALLBACK_TD(&PrinterMain::blinker_handler, &PrinterMain::m_blinker) {};
    struct SerialRecvHandler : public AMBRO_WCALLBACK_TD(&PrinterMain::serial_recv_handler, &PrinterMain::m_serial) {};
    struct SerialSendHandler : public AMBRO_WCALLBACK_TD(&PrinterMain::serial_send_handler, &PrinterMain::m_serial) {};
    template <int AxisIndex> struct PlannerGetAxisStepper : public AMBRO_WCALLBACK_TD(&PrinterMain::template getAxisStepper<AxisIndex>, &PrinterMain::m_planner) {};
    struct PlannerPullHandler : public AMBRO_WCALLBACK_TD(&PrinterMain::planner_pull_handler, &PrinterMain::m_planner) {};
    struct PlannerFinishedHandler : public AMBRO_WCALLBACK_TD(&PrinterMain::planner_finished_handler, &PrinterMain::m_planner) {};
    struct PlannerChannelCallback : public AMBRO_WCALLBACK_TD(&PrinterMain::planner_channel_callback, &PrinterMain::m_planner) {};
    template <int AxisIndex> struct AxisStepperConsumersList {
        using List = JoinTypeLists<
            MakeTypeList<typename ThePlanner::template TheAxisStepperConsumer<AxisIndex>>,
            typename Axis<AxisIndex>::HomingFeature::template MakeAxisStepperConsumersList<typename Axis<AxisIndex>::HomingFeature>
        >;
    };
};

#include <aprinter/EndNamespace.h>

#endif
