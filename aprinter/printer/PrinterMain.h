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
#include <aprinter/stepper/AxisSplitter.h>
#include <aprinter/printer/AxisHomer.h>
#include <aprinter/printer/GcodeParser.h>
#include <aprinter/printer/MotionPlanner.h>
#include <aprinter/printer/TemperatureObserver.h>

#include <aprinter/BeginNamespace.h>

template <
    typename TSerial, typename TLedPin, typename TLedBlinkInterval, typename TDefaultInactiveTime,
    typename TSpeedLimitMultiply, typename TMaxStepsPerCycle, typename TAxesList, typename THeatersList
>
struct PrinterMainParams {
    using Serial = TSerial;
    using LedPin = TLedPin;
    using LedBlinkInterval = TLedBlinkInterval;
    using DefaultInactiveTime = TDefaultInactiveTime;
    using SpeedLimitMultiply = TSpeedLimitMultiply;
    using MaxStepsPerCycle = TMaxStepsPerCycle;
    using AxesList = TAxesList;
    using HeatersList = THeatersList;
};

template <uint32_t tbaud, typename TTheGcodeParserParams>
struct PrinterMainSerialParams {
    static const uint32_t baud = tbaud;
    using TheGcodeParserParams = TTheGcodeParserParams;
};

template <
    char tname,
    typename TDirPin, typename TStepPin, typename TEnablePin, bool TInvertDir,
    int TBufferSizeExp, typename TTheAxisStepperParams,
    typename TDefaultStepsPerUnit, typename TDefaultMaxSpeed, typename TDefaultMaxAccel,
    typename TDefaultMin, typename TDefaultMax, bool tenable_cartesian_speed_limit,
    typename THoming
>
struct PrinterMainAxisParams {
    static const char name = tname;
    using DirPin = TDirPin;
    using StepPin = TStepPin;
    using EnablePin = TEnablePin;
    static const bool InvertDir = TInvertDir;
    static const int BufferSizeExp = TBufferSizeExp;
    using TheAxisStepperParams = TTheAxisStepperParams;
    using DefaultStepsPerUnit = TDefaultStepsPerUnit;
    using DefaultMaxSpeed = TDefaultMaxSpeed;
    using DefaultMaxAccel = TDefaultMaxAccel;
    using DefaultMin = TDefaultMin;
    using DefaultMax = TDefaultMax;
    static const bool enable_cartesian_speed_limit = tenable_cartesian_speed_limit;
    using Homing = THoming;
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
    typename TAdcPin, typename TFormula,
    typename TOutputPin, typename TPulseInterval,
    template<typename, typename> class TControl,
    typename TControlParams,
    template<typename, typename> class TTimerTemplate,
    typename TTheTemperatureObserverParams
>
struct PrinterMainHeaterParams {
    static const char Name = TName;
    static const int SetMCommand = TSetMCommand;
    static const int WaitMCommand = TWaitMCommand;
    using AdcPin = TAdcPin;
    using Formula = TFormula;
    using OutputPin = TOutputPin;
    using PulseInterval = TPulseInterval;
    template <typename X, typename Y> using Control = TControl<X, Y>;
    using ControlParams = TControlParams;
    template <typename X, typename Y> using TimerTemplate = TTimerTemplate<X, Y>;
    using TheTemperatureObserverParams = TTheTemperatureObserverParams;
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
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_update_req_pos, update_req_pos)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_write_planner_command, write_planner_command)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_append_position, append_position)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_set_relative_positioning, set_relative_positioning)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_set_position, set_position)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_append_value, append_value)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_check_command, check_command)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_emergency, emergency)
    
    template <int AxisIndex> struct AxisPosition;
    template <int AxisIndex> struct HomingFeaturePosition;
    template <int AxisIndex> struct HomingStatePosition;
    struct PlannerPosition;
    template <int HeaterIndex> struct HeaterPosition;
    
    struct SerialRecvHandler;
    struct SerialSendHandler;
    template <int AxisIndex> struct PlannerGetAxisStepper;
    struct PlannerPullHandler;
    struct PlannerFinishedHandler;
    template <int AxisIndex> struct AxisStepperConsumersList;
    
    static const int serial_recv_buffer_bits = 6;
    static const int serial_send_buffer_bits = 8;
    static const int reply_buffer_size = 128;
    
    using Loop = typename Context::EventLoop;
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using AxesList = typename Params::AxesList;
    using HeatersList = typename Params::HeatersList;
    static const int num_axes = TypeListLength<AxesList>::value;
    using AxisMaskType = typename ChooseInt<num_axes, false>::Type;
    using AxisCountType = typename ChooseInt<BitsInInt<num_axes>::value, false>::Type;
    using ReplyBufferSizeType = typename ChooseInt<BitsInInt<reply_buffer_size>::value, false>::Type;
    
    template <typename TheAxis>
    using MakeStepperDef = StepperDef<
        typename TheAxis::DirPin,
        typename TheAxis::StepPin,
        typename TheAxis::EnablePin,
        TheAxis::InvertDir
    >;
    
    using TheBlinker = Blinker<Context, typename Params::LedPin>;
    using StepperDefsList = MapTypeList<AxesList, TemplateFunc<MakeStepperDef>>;
    using TheSteppers = Steppers<Context, StepperDefsList>;
    using TheSerial = AvrSerial<Context, serial_recv_buffer_bits, serial_send_buffer_bits, SerialRecvHandler, SerialSendHandler>;
    using RecvSizeType = typename TheSerial::RecvSizeType;
    using SendSizeType = typename TheSerial::SendSizeType;
    using TheGcodeParser = GcodeParser<Context, typename Params::Serial::TheGcodeParserParams, typename RecvSizeType::IntType>;
    using GcodeParserCommand = typename TheGcodeParser::Command;
    using GcodeParserCommandPart = typename TheGcodeParser::CommandPart;
    using GcodePartsSizeType = typename TheGcodeParser::PartsSizeType;
    
    template <int TAxisIndex>
    struct Axis {
        static const int AxisIndex = TAxisIndex;
        friend PrinterMain;
        
        struct AxisStepperPosition;
        struct AxisStepperGetStepperHandler;
        
        using AxisSpec = TypeListGet<AxesList, AxisIndex>;
        using Stepper = SteppersStepper<Context, StepperDefsList, AxisIndex>;
        using TheAxisStepper = AxisStepper<AxisStepperPosition, Context, typename AxisSpec::TheAxisStepperParams, Stepper, AxisStepperGetStepperHandler, AxisStepperConsumersList<AxisIndex>>;
        template <typename X> using SplitterTemplate = AxisSplitter<X>;
        using TheSplitter = SplitterTemplate<TheAxisStepper>;
        using StepFixedType = typename TheSplitter::StepFixedType;
        static const char axis_name = AxisSpec::name;
        
        AMBRO_STRUCT_IF(HomingFeature, AxisSpec::Homing::enabled) {
            struct HomingState {
                struct HomerPosition;
                struct HomerGetAxisStepper;
                struct HomerFinishedHandler;
                
                using Homer = AxisHomer<
                    HomerPosition, Context, TheAxisStepper, SplitterTemplate, AxisSpec::BufferSizeExp, typename AxisSpec::Homing::EndPin,
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
                    axis->m_end_pos = axis->m_min;
                    axis->m_req_pos = axis->m_min;
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
                Axis *axis = parent();
                
                m_fast_max_dist = axis->dist_from_real(AxisSpec::Homing::DefaultFastMaxDist::value());
                m_retract_dist = axis->dist_from_real(AxisSpec::Homing::DefaultRetractDist::value());
                m_slow_max_dist = axis->dist_from_real(AxisSpec::Homing::DefaultSlowMaxDist::value());
                m_fast_speed = axis->speed_from_real(AxisSpec::Homing::DefaultFastSpeed::value());
                m_retract_speed = axis->speed_from_real(AxisSpec::Homing::DefaultRetractSpeed::value());
                m_slow_speed = axis->speed_from_real(AxisSpec::Homing::DefaultSlowSpeed::value());
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
                params.fast_max_dist = StepFixedType::importDoubleSaturated(m_fast_max_dist);
                params.retract_dist = StepFixedType::importDoubleSaturated(m_retract_dist);
                params.slow_max_dist = StepFixedType::importDoubleSaturated(m_slow_max_dist);
                params.fast_speed = m_fast_speed;
                params.retract_speed = m_retract_speed;
                params.slow_speed = m_slow_speed;
                params.max_accel = axis->m_max_accel;
                
                axis->stepper()->enable(c, true);
                homing_state()->m_homer.init(c, params);
                axis->m_state = AXIS_STATE_HOMING;
                o->m_homing_rem_axes++;
            }
            
            double m_fast_max_dist; // steps
            double m_retract_dist; // steps
            double m_slow_max_dist; // steps
            double m_fast_speed; // steps/tick
            double m_retract_speed; // steps/tick
            double m_slow_speed; // steps/tick
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
            return (x * m_steps_per_unit);
        }
        
        double speed_from_real (double v)
        {
            return (v * (m_steps_per_unit / Clock::time_freq));
        }
        
        double accel_from_real (double a)
        {
            return (a * (m_steps_per_unit / (Clock::time_freq * Clock::time_freq)));
        }
        
        static double clamp_limit (double x)
        {
            double bound = FloatSignedIntegerRange<double>();
            return fmax(-bound, fmin(bound, round(x)));
        }
        
        double clamp_pos (double pos)
        {
            return fmax(m_min, fmin(m_max, pos));
        }
        
        void init (Context c)
        {
            m_axis_stepper.init(c);
            m_state = AXIS_STATE_OTHER;
            m_steps_per_unit = AxisSpec::DefaultStepsPerUnit::value();
            m_max_speed = speed_from_real(AxisSpec::DefaultMaxSpeed::value());
            m_max_accel = accel_from_real(AxisSpec::DefaultMaxAccel::value());
            m_min = clamp_limit(dist_from_real(AxisSpec::DefaultMin::value()));
            m_max = clamp_limit(dist_from_real(AxisSpec::DefaultMax::value()));
            m_homing_feature.init(c);
            m_end_pos = clamp_pos(0.0);
            m_req_pos = clamp_pos(0.0);
            m_move = 0.0;
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
        
        void update_req_pos (Context c, GcodeParserCommandPart *part, bool *changed)
        {
            if (part->code == axis_name) {
                double req = strtod(part->data, NULL);
                if (isnan(req)) {
                    req = 0.0;
                }
                req *= m_steps_per_unit;
                if (m_relative_positioning) {
                    req += m_req_pos;
                }
                m_req_pos = clamp_pos(req);
                m_move = round(m_req_pos) - m_end_pos;
                if (m_move != 0.0) {
                    if (fabs(m_move) > StepFixedType::maxValue().doubleValue()) {
                        m_move = ((m_move < 0.0) ? -1.0 : 1.0) * StepFixedType::maxValue().doubleValue();
                        m_req_pos = m_end_pos + m_move;
                    }
                    *changed = true;
                    if (AxisSpec::enable_cartesian_speed_limit) {
                        double delta = m_move / m_steps_per_unit;
                        parent()->m_planning_distance += delta * delta;
                    }
                    enable_stepper(c, true);
                }
            }
        }
        
        template <typename PlannerCmd>
        void write_planner_command (PlannerCmd *cmd, double *total_steps)
        {
            auto *mycmd = TupleGetElem<AxisIndex>(&cmd->axes);
            if (m_move >= 0.0) {
                mycmd->dir = true;
                mycmd->x = StepFixedType::importBits(m_move);
                *total_steps += m_move;
            } else {
                mycmd->dir = false;
                mycmd->x = StepFixedType::importBits(-m_move);
                *total_steps -= m_move;
            }
            mycmd->max_v = m_max_speed;
            mycmd->max_a = m_max_accel;
            m_end_pos += m_move;
            m_move = 0.0;
        }
        
        void append_position (Context c)
        {
            parent()->reply_append_fmt(c, "%c:%f", axis_name, m_req_pos / m_steps_per_unit);
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
                m_end_pos = round(clamp_pos(req * m_steps_per_unit));
                m_req_pos = m_end_pos;
                m_move = 0.0;
            }
        }
        
        static void emergency ()
        {
            Stepper::emergency();
        }
        
        TheAxisStepper m_axis_stepper;
        uint8_t m_state;
        double m_steps_per_unit;
        double m_max_speed; // steps/tick
        double m_max_accel; // steps/tick^2
        double m_min; // steps, integer
        double m_max; // steps, integer
        HomingFeature m_homing_feature;
        double m_end_pos; // steps, integer
        double m_req_pos; // steps
        double m_move; // steps, integer, =round(m_req_pos)-m_end_pos
        bool m_relative_positioning;
        
        struct AxisStepperPosition : public MemberPosition<AxisPosition<AxisIndex>, TheAxisStepper, &Axis::m_axis_stepper> {};
        struct AxisStepperGetStepperHandler : public AMBRO_WCALLBACK_TD(&Axis::stepper, &Axis::m_axis_stepper) {};
    };
    
    template <typename TheAxis>
    using MakePlannerAxisSpec = MotionPlannerAxisSpec<
        typename TheAxis::TheAxisStepper,
        PlannerGetAxisStepper<TheAxis::AxisIndex>,
        TheAxis::template SplitterTemplate,
        TheAxis::AxisSpec::BufferSizeExp
    >;
    
    using MotionPlannerAxes = MapTypeList<IndexElemList<AxesList, Axis>, TemplateFunc<MakePlannerAxisSpec>>;
    using ThePlanner = MotionPlanner<PlannerPosition, Context, MotionPlannerAxes, PlannerPullHandler, PlannerFinishedHandler>;
    using PlannerInputCommand = typename ThePlanner::InputCommand;
    using AxesTuple = IndexElemTuple<AxesList, Axis>;
    
    template <int AxisIndex>
    using HomingStateTupleHelper = typename Axis<AxisIndex>::HomingFeature::HomingState;
    using HomingStateTuple = IndexElemTuple<AxesList, HomingStateTupleHelper>;
    
    template <int HeaterIndex>
    struct Heater {
        struct SoftPwmTimerHandler;
        struct ObserverGetValueCallback;
        struct ObserverHandler;
        
        using HeaterSpec = TypeListGet<HeatersList, HeaterIndex>;
        using TheControl = typename HeaterSpec::template Control<typename HeaterSpec::ControlParams, typename HeaterSpec::PulseInterval>;
        using TheSoftPwm = SoftPwm<Context, typename HeaterSpec::OutputPin, typename HeaterSpec::PulseInterval, SoftPwmTimerHandler, HeaterSpec::template TimerTemplate>;
        using TheObserver = TemperatureObserver<Context, typename HeaterSpec::TheTemperatureObserverParams, ObserverGetValueCallback, ObserverHandler>;
        
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
        double get_value (ThisContext c)
        {
            return HeaterSpec::Formula::call(c.adc()->template getValue<typename HeaterSpec::AdcPin>(c));
        }
        
        void append_value (PrinterMain *o, Context c)
        {
            double value = get_value(c);
            o->reply_append_fmt(c, " %c:%f", HeaterSpec::Name, value);
        }
        
        bool check_command (Context c, int cmd_num, int *out_result)
        {
            PrinterMain *o = parent();
            
            if (cmd_num != HeaterSpec::SetMCommand && cmd_num != HeaterSpec::WaitMCommand) {
                return true;
            }
            if (o->m_state == STATE_PLANNING) {
                *out_result = CMD_WAIT_PLANNER;
                return false;
            }
            double target = o->get_command_param_double(o->m_cmd, 'S', 0.0);
            if (target > 0.0 && target <= 300.0) {
                if (!m_enabled) {
                    m_control.init(target);
                    AMBRO_LOCK_T(m_lock, c, lock_c, {
                        m_enabled = true;
                    });
                } else {
                    AMBRO_LOCK_T(m_lock, c, lock_c, {
                        m_control.setTarget(target);
                    });
                }
            } else {
                if (m_enabled) {
                    AMBRO_LOCK_T(m_lock, c, lock_c, {
                        m_enabled = false;
                    });
                }
            }
            if (cmd_num == HeaterSpec::WaitMCommand) {
                AMBRO_ASSERT(!m_observing)
                m_observer.init(c, target);
                m_observing = true;
                o->m_state = STATE_WAITING_TEMP;
                o->now_active(c);
                *out_result = CMD_DELAY;
            } else {
                *out_result = CMD_REPLY;
            }
            return false;
        }
        
        double softpwm_timer_handler (typename TheSoftPwm::TimerInstance::HandlerContext c)
        {
            if (AMBRO_UNLIKELY(!m_enabled)) {
                return 0.0;
            }
            double sensor_value = get_value(c);
            double control_value = m_control.addMeasurement(sensor_value);
            return control_value;
        }
        
        double observer_get_value_callback (Context c)
        {
            return get_value(c);
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
    
    using HeatersTuple = IndexElemTuple<HeatersList, Heater>;
    
public:
    void init (Context c)
    {
        m_blinker.init(c, Params::LedBlinkInterval::value() * Clock::time_freq);
        m_steppers.init(c);
        TupleForEachForward(&m_axes, Foreach_init(), c);
        m_serial.init(c, Params::Serial::baud);
        m_gcode_parser.init(c);
        m_disable_timer.init(c, AMBRO_OFFSET_CALLBACK_T(&PrinterMain::m_disable_timer, &PrinterMain::disable_timer_handler));
        TupleForEachForward(&m_heaters, Foreach_init(), c);
        m_inactive_time = Params::DefaultInactiveTime::value() * Clock::time_freq;
        m_recv_next_error = 0;
        m_line_number = 0;
        m_cmd = NULL;
        m_reply_length = 0;
        m_max_cart_speed = INFINITY;
        m_state = STATE_IDLE;
        
        reply_append_str(c, "APrinter\n");
        reply_send(c);
        
        this->debugInit(c);
    }

    void deinit (Context c)
    {
        this->debugDeinit(c);
        
        TupleForEachReverse(&m_heaters, Foreach_deinit(), c);
        if (m_state == STATE_PLANNING) {
            m_planner.deinit(c);
        }
        m_disable_timer.deinit(c);
        m_gcode_parser.deinit(c);
        m_serial.deinit(c);
        TupleForEachReverse(&m_axes, Foreach_deinit(), c);
        m_steppers.deinit(c);
        m_blinker.deinit(c);
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
    
    static void emergency ()
    {
        AxesTuple dummy_axes;
        TupleForEachForward(&dummy_axes, Foreach_emergency());
        HeatersTuple dummy_heaters;
        TupleForEachForward(&dummy_heaters, Foreach_emergency());
    }
    
private:
    enum {STATE_IDLE, STATE_HOMING, STATE_PLANNING, STATE_WAITING_TEMP};
    enum {CMD_REPLY, CMD_WAIT_PLANNER, CMD_DELAY};
    
    static TimeType time_from_real (double t)
    {
        return (FixedPoint<30, false, 0>::importDoubleSaturated(t * Clock::time_freq)).bitsValue();
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
        AMBRO_ASSERT(m_state != STATE_PLANNING || !m_planning_req_pending)
        
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
            reply_append_fmt(c, "Error:%s\n", err);
            goto reply;
        }
        
        cmd_code = m_cmd->parts[0].code;
        cmd_num = atoi(m_cmd->parts[0].data);
        
        if (!already_seen) {
            bool is_m110 = (cmd_code == 'M' && cmd_num == 110);
            if (is_m110) {
                m_line_number = get_command_param_uint32(m_cmd, 'L', -1);
            }
            if (m_cmd->have_line_number) {
                if (m_cmd->line_number != m_line_number) {
                    reply_append_fmt(c, "Error:Line Number is not Last Line Number+1, Last Line:%" PRIu32 "\n", (uint32_t)(m_line_number - 1));
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
                    if (!TupleForEachForwardInterruptible(&m_heaters, Foreach_check_command(), c, cmd_num, &result)) {
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
                    reply_append_str(c, "\n");
                    no_ok = true;
                } break;
                
                case 114: {
                    TupleForEachForward(&m_axes, Foreach_append_position(), c);
                    reply_append_str(c, "\n");
                } break;
            } break;
            
            case 'G': switch (cmd_num) {
                default:
                    goto unknown_command;
                
                case 0:
                case 1: { // buffered move
                    bool changed = false;
                    m_planning_distance = 0.0;
                    for (GcodePartsSizeType i = 1; i < m_cmd->num_parts; i++) {
                        GcodeParserCommandPart *part = &m_cmd->parts[i];
                        TupleForEachForward(&m_axes, Foreach_update_req_pos(), c, part, &changed);
                        if (part->code == 'F') {
                            m_max_cart_speed = strtod(part->data, NULL) * Params::SpeedLimitMultiply::value();
                        }
                    }
                    if (changed) {
                        if (m_state != STATE_PLANNING) {
                            m_planner.init(c);
                            m_state = STATE_PLANNING;
                            m_planning_pull_pending = false;
                            now_active(c);
                        }
                        m_planning_req_pending = true;
                        m_planning_distance = sqrt(m_planning_distance);
                        if (m_planning_pull_pending) {
                            send_req_to_planner(c);
                        }
                        return;
                    }
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
                reply_append_fmt(c, "Error:Unknown command %s\n", (m_cmd->parts[0].data - 1));
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
        reply_send(c);
        
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
    
    template <typename... Args>
    void reply_append_fmt (Context c, char const *fmt, Args... args)
    {
        int len = snprintf(m_reply_buf + m_reply_length, sizeof(m_reply_buf) - m_reply_length, fmt, args...);
        if (len > sizeof(m_reply_buf) - 1 - m_reply_length) {
            len = sizeof(m_reply_buf) - 1 - m_reply_length;
        }
        m_reply_length += len;
    }
    
    void reply_append_str (Context c, char const *str)
    {
        size_t len = strlen(str);
        if (len > sizeof(m_reply_buf) - 1 - m_reply_length) {
            len = sizeof(m_reply_buf) - 1 - m_reply_length;
        }
        memcpy(m_reply_buf + m_reply_length, str, len);
        m_reply_length += len;
    }
    
    void reply_send (Context c)
    {
        char *src = m_reply_buf;
        ReplyBufferSizeType length = m_reply_length;
        SendSizeType avail = m_serial.sendQuery(c);
        if (length > avail.value()) {
            length = avail.value();
        }
        while (length > 0) {
            char *chunk_data = m_serial.sendGetChunkPtr(c);
            SendSizeType chunk_length = m_serial.sendGetChunkLen(c, SendSizeType::import(length));
            memcpy(chunk_data, src, chunk_length.value());
            m_serial.sendProvide(c, chunk_length);
            src += chunk_length.value();
            length -= chunk_length.value();
        }
        m_reply_length = 0;
    }
    
    void send_req_to_planner (Context c)
    {
        AMBRO_ASSERT(m_state == STATE_PLANNING)
        AMBRO_ASSERT(m_planning_req_pending)
        AMBRO_ASSERT(m_cmd)
        AMBRO_ASSERT(m_planning_pull_pending)
        
        PlannerInputCommand cmd;
        cmd.rel_max_v = FloatMakePosOrPosZero((m_max_cart_speed / m_planning_distance) / Clock::time_freq);
        double total_steps = 0.0;
        TupleForEachForward(&m_axes, Foreach_write_planner_command(), &cmd, &total_steps);
        cmd.rel_max_v = fmin(cmd.rel_max_v, (Params::MaxStepsPerCycle::value() * (F_CPU / Clock::time_freq)) / total_steps);
        m_planner.commandDone(c, cmd);
        m_planning_req_pending = false;
        m_planning_pull_pending = false;
        finish_command(c, false);
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
    
    void planner_pull_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_PLANNING)
        AMBRO_ASSERT(!m_planning_pull_pending)
        
        m_planning_pull_pending = true;
        if (m_planning_req_pending) {
            send_req_to_planner(c);
        } else if (m_cmd) {
            m_planner.waitFinished(c);
        }
    }
    
    void planner_finished_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_PLANNING)
        AMBRO_ASSERT(m_planning_pull_pending)
        AMBRO_ASSERT(!m_planning_req_pending)
        AMBRO_ASSERT(m_cmd)
        
        m_planner.deinit(c);
        m_state = STATE_IDLE;
        now_inactive(c);
        
        if (m_cmd) {
            process_received_command(c, true);
        }
    }
    
    TheBlinker m_blinker;
    TheSteppers m_steppers;
    AxesTuple m_axes;
    TheSerial m_serial;
    TheGcodeParser m_gcode_parser;
    typename Loop::QueuedEvent m_disable_timer;
    HeatersTuple m_heaters;
    TimeType m_inactive_time;
    TimeType m_last_active_time;
    int8_t m_recv_next_error;
    uint32_t m_line_number;
    GcodeParserCommand *m_cmd;
    char m_reply_buf[reply_buffer_size];
    ReplyBufferSizeType m_reply_length;
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
            double m_planning_distance;
        };
    };
    
    template <int AxisIndex> struct AxisPosition : public TuplePosition<Position, AxesTuple, &PrinterMain::m_axes, AxisIndex> {};
    template <int AxisIndex> struct HomingFeaturePosition : public MemberPosition<AxisPosition<AxisIndex>, typename Axis<AxisIndex>::HomingFeature, &Axis<AxisIndex>::m_homing_feature> {};
    template <int AxisIndex> struct HomingStatePosition : public TuplePosition<Position, HomingStateTuple, &PrinterMain::m_homers, AxisIndex> {};
    struct PlannerPosition : public MemberPosition<Position, ThePlanner, &PrinterMain::m_planner> {};
    template <int HeaterIndex> struct HeaterPosition : public TuplePosition<Position, HeatersTuple, &PrinterMain::m_heaters, HeaterIndex> {};
    
    struct SerialRecvHandler : public AMBRO_WCALLBACK_TD(&PrinterMain::serial_recv_handler, &PrinterMain::m_serial) {};
    struct SerialSendHandler : public AMBRO_WCALLBACK_TD(&PrinterMain::serial_send_handler, &PrinterMain::m_serial) {};
    template <int AxisIndex> struct PlannerGetAxisStepper : public AMBRO_WCALLBACK_TD(&PrinterMain::template getAxisStepper<AxisIndex>, &PrinterMain::m_planner) {};
    struct PlannerPullHandler : public AMBRO_WCALLBACK_TD(&PrinterMain::planner_pull_handler, &PrinterMain::m_planner) {};
    struct PlannerFinishedHandler : public AMBRO_WCALLBACK_TD(&PrinterMain::planner_finished_handler, &PrinterMain::m_planner) {};
    template <int AxisIndex> struct AxisStepperConsumersList {
        using List = JoinTypeLists<
            MakeTypeList<typename ThePlanner::template TheAxisStepperConsumer<AxisIndex>>,
            typename Axis<AxisIndex>::HomingFeature::template MakeAxisStepperConsumersList<typename Axis<AxisIndex>::HomingFeature>
        >;
    };
};

#include <aprinter/EndNamespace.h>

#endif
