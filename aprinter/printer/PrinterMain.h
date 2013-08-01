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
#include <aprinter/meta/WrapMember.h>
#include <aprinter/meta/TupleGet.h>
#include <aprinter/meta/TupleForEach.h>
#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/ForwardHandler.h>
#include <aprinter/meta/WrapDouble.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/TypeListLength.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/IndexElemList.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/OffsetCallback.h>
#include <aprinter/base/Lock.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/system/AvrSerial.h>
#include <aprinter/devices/Blinker.h>
#include <aprinter/devices/SoftPwm.h>
#include <aprinter/stepper/Steppers.h>
#include <aprinter/stepper/AxisSharer.h>
#include <aprinter/printer/AxisHomer.h>
#include <aprinter/printer/GcodeParser.h>
#include <aprinter/printer/MotionPlanner.h>

#include <aprinter/BeginNamespace.h>

template <
    typename TSerial, typename TLedPin, typename TLedBlinkInterval, typename TDefaultInactiveTime,
    typename TSpeedLimitMultiply, typename TAxesList, typename THeatersList
>
struct PrinterMainParams {
    using Serial = TSerial;
    using LedPin = TLedPin;
    using LedBlinkInterval = TLedBlinkInterval;
    using DefaultInactiveTime = TDefaultInactiveTime;
    using SpeedLimitMultiply = TSpeedLimitMultiply;
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
    typename TTheAxisStepperParams,
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
    char TName, int TSetMCommand,
    typename TAdcPin, typename TFormula,
    typename TOutputPin, typename TPulseInterval,
    template<typename, typename> class TControl,
    typename TControlParams,
    template<typename, typename> class TTimerTemplate
>
struct PrinterMainHeaterParams {
    static const char Name = TName;
    static const int SetMCommand = TSetMCommand;
    using AdcPin = TAdcPin;
    using Formula = TFormula;
    using OutputPin = TOutputPin;
    using PulseInterval = TPulseInterval;
    template <typename X, typename Y> using Control = TControl<X, Y>;
    using ControlParams = TControlParams;
    template <typename X, typename Y> using TimerTemplate = TTimerTemplate<X, Y>;
};

template <typename Context, typename Params>
class PrinterMain
: private DebugObject<Context, void>
{
private:
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_init, init)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_deinit, deinit)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_start_homing, start_homing)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_update_homing_mask, update_homing_mask)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_enable_stepper, enable_stepper)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_init_sharers_tuple, init_sharers_tuple)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_update_req_pos, update_req_pos)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_write_planner_command, write_planner_command)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_append_position, append_position)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_set_relative_positioning, set_relative_positioning)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_set_position, set_position)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_append_value, append_value)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_check_command, check_command)
    
    struct SerialRecvHandler;
    struct SerialSendHandler;
    struct PlannerPullCmdHandler;
    struct PlannerBufferFullHandler;
    struct PlannerBufferEmptyHandler;
    
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
    
    template <int AxisIndex>
    struct Axis {
        friend PrinterMain;
        
        struct SharerGetStepperHandler;
        
        using AxisSpec = TypeListGet<AxesList, AxisIndex>;
        using Stepper = SteppersStepper<Context, StepperDefsList, AxisIndex>;
        using Sharer = AxisSharer<Context, typename AxisSpec::TheAxisStepperParams, Stepper, SharerGetStepperHandler>;
        using StepFixedType = typename Sharer::Axis::StepFixedType;
        static const char axis_name = AxisSpec::name;
        
        AMBRO_STRUCT_IF(HomingFeature, AxisSpec::Homing::enabled) {
            struct HomerFinishedHandler;
            using Homer = AxisHomer<Context, Sharer, typename AxisSpec::Homing::EndPin, AxisSpec::Homing::end_invert, AxisSpec::Homing::home_dir, HomerFinishedHandler>;
            
            Axis * parent ()
            {
                return AMBRO_WMEMB_TD(&Axis::m_homing_feature)::container(this);
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
                    m_homer.stop(c);
                    m_homer.deinit(c);
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
                
                typename Homer::HomingParams params;
                params.fast_max_dist = StepFixedType::importDoubleSaturated(m_fast_max_dist);
                params.retract_dist = StepFixedType::importDoubleSaturated(m_retract_dist);
                params.slow_max_dist = StepFixedType::importDoubleSaturated(m_slow_max_dist);
                params.fast_speed = m_fast_speed;
                params.retract_speed = m_retract_speed;
                params.slow_speed = m_slow_speed;
                params.max_accel = axis->m_max_accel;
                
                axis->stepper()->enable(c, true);
                m_homer.init(c, &axis->m_sharer);
                m_homer.start(c, params);
                axis->m_state = AXIS_STATE_HOMING;
                o->m_homing.rem_axes++;
            }
            
            void homer_finished_handler (Context c, bool success)
            {
                Axis *axis = parent();
                PrinterMain *o = axis->parent();
                AMBRO_ASSERT(axis->m_state == AXIS_STATE_HOMING)
                AMBRO_ASSERT(o->m_state == STATE_HOMING)
                AMBRO_ASSERT(o->m_homing.rem_axes > 0)
                
                m_homer.deinit(c);
                axis->m_end_pos = axis->m_min;
                axis->m_req_pos = axis->m_min;
                axis->m_state = AXIS_STATE_OTHER;
                o->m_homing.rem_axes--;
                if (!success) {
                    o->m_homing.failed = true;
                }
                if (o->m_homing.rem_axes == 0) {
                    o->homing_finished(c);
                }
            }
            
            Homer m_homer;
            double m_fast_max_dist; // steps
            double m_retract_dist; // steps
            double m_slow_max_dist; // steps
            double m_fast_speed; // steps/tick
            double m_retract_speed; // steps/tick
            double m_slow_speed; // steps/tick
            
            struct HomerFinishedHandler : public AMBRO_WCALLBACK_TD(&HomingFeature::homer_finished_handler, &HomingFeature::m_homer) {};
        } AMBRO_STRUCT_ELSE(HomingFeature) {
            void init (Context c) {}
            void deinit (Context c) {}
            void start_homing (Context c, AxisMaskType mask) {}
        };
        
        enum {AXIS_STATE_OTHER, AXIS_STATE_HOMING};
        
        PrinterMain * parent ()
        {
            return AMBRO_WMEMB_TD(&PrinterMain::m_axes)::container(TupleGetTuple<AxisIndex, AxesTuple>(this));
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
            double bound = fmin(FloatSignedIntegerRange<double>(), StepFixedType::maxValue().doubleValue() / 2);
            return fmax(-bound, fmin(bound, round(x)));
        }
        
        double clamp_pos (double pos)
        {
            return fmax(m_min, fmin(m_max, pos));
        }
        
        void init (Context c)
        {
            m_sharer.init(c);
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
            m_sharer.deinit(c);
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
        
        template <typename SharersTuple>
        void init_sharers_tuple (SharersTuple *sharers)
        {
            *TupleGetElem<AxisIndex>(sharers) = &m_sharer;
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
                    *changed = true;
                    if (AxisSpec::enable_cartesian_speed_limit) {
                        double delta = m_move / m_steps_per_unit;
                        parent()->m_planning.distance += delta * delta;
                    }
                    enable_stepper(c, true);
                }
            }
        }
        
        template <typename PlannerCmd>
        void write_planner_command (PlannerCmd *cmd)
        {
            auto *mycmd = TupleGetElem<AxisIndex>(&cmd->axes);
            if (m_move >= 0.0) {
                mycmd->dir = true;
                mycmd->x = StepFixedType::importBits(m_move);
            } else {
                mycmd->dir = false;
                mycmd->x = StepFixedType::importBits(-m_move);
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
        
        Sharer m_sharer;
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
        
        struct SharerGetStepperHandler : public AMBRO_WCALLBACK_TD(&Axis::stepper, &Axis::m_sharer) {};
    };
    
    template <typename TheAxis>
    using MakePlannerAxisSpec = MotionPlannerAxisSpec<typename TheAxis::Sharer>;
    
    using MotionPlannerAxes = MapTypeList<IndexElemList<AxesList, Axis>, TemplateFunc<MakePlannerAxisSpec>>;
    using ThePlanner = MotionPlanner<Context, MotionPlannerAxes, PlannerPullCmdHandler, PlannerBufferFullHandler, PlannerBufferEmptyHandler>;
    using PlannerSharersTuple = typename ThePlanner::SharersTuple;
    using PlannerInputCommand = typename ThePlanner::InputCommand;
    using AxesTuple = IndexElemTuple<AxesList, Axis>;
    
    template <int HeaterIndex>
    struct Heater {
        using HeaterSpec = TypeListGet<HeatersList, HeaterIndex>;
        using TheControl = typename HeaterSpec::template Control<typename HeaterSpec::ControlParams, typename HeaterSpec::PulseInterval>;
        struct SoftPwmTimerHandler;
        using TheSoftPwm = SoftPwm<Context, typename HeaterSpec::OutputPin, typename HeaterSpec::PulseInterval, SoftPwmTimerHandler, HeaterSpec::template TimerTemplate>;
        
        PrinterMain * parent ()
        {
            return AMBRO_WMEMB_TD(&PrinterMain::m_heaters)::container(TupleGetTuple<HeaterIndex, HeatersTuple>(this));
        }
        
        void init (Context c)
        {
            m_lock.init(c);
            m_enabled = false;
            m_softpwm.init(c, c.clock()->getTime(c));
        }
        
        void deinit (Context c)
        {
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
        
        bool check_command (Context c, int cmd_num)
        {
            PrinterMain *o = parent();
            
            if (cmd_num == HeaterSpec::SetMCommand) {
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
                return false;
            }
            return true;
        }
        
        double softpwm_timer_handler (typename TheSoftPwm::TimerInstance::HandlerContext c)
        {
            if (!m_enabled) {
                return 0.0;
            }
            double sensor_value = get_value(c);
            double control_value = m_control.addMeasurement(sensor_value);
            return fmax(0.0, fmin(1.0, control_value));
        }
        
        typename Context::Lock m_lock;
        bool m_enabled;
        TheControl m_control;
        TheSoftPwm m_softpwm;
        
        struct SoftPwmTimerHandler : public AMBRO_WCALLBACK_TD(&Heater::softpwm_timer_handler, &Heater::m_softpwm) {};
    };
    
    using HeatersTuple = IndexElemTuple<HeatersList, Heater>;
    
public:
    void init (Context c)
    {
        PlannerSharersTuple sharers;
        TupleForEachForward(&m_axes, Foreach_init_sharers_tuple(), &sharers);
        
        m_blinker.init(c, Params::LedBlinkInterval::value() * Clock::time_freq);
        m_steppers.init(c);
        TupleForEachForward(&m_axes, Foreach_init(), c);
        m_serial.init(c, Params::Serial::baud);
        m_gcode_parser.init(c);
        m_disable_timer.init(c, AMBRO_OFFSET_CALLBACK_T(&PrinterMain::m_disable_timer, &PrinterMain::disable_timer_handler));
        m_planner.init(c, sharers);
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
            m_planner.stop(c);
        }
        m_planner.deinit(c);
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
    typename Axis<AxisIndex>::Sharer * getSharer ()
    {
        return &TupleGetElem<AxisIndex>(&m_axes)->m_sharer;
    }
    
    template <int HeaterIndex>
    typename Heater<HeaterIndex>::TheSoftPwm::TimerInstance * getHeaterTimer ()
    {
        return TupleGetElem<HeaterIndex>(&m_heaters)->m_softpwm.getTimer();
    }
    
private:
    enum {
        STATE_IDLE,
        STATE_HOMING,
        STATE_PLANNING
    };
    
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
        AMBRO_ASSERT(m_state != STATE_PLANNING || !m_planning.req_pending)
        
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
                    if (!TupleForEachForwardInterruptible(&m_heaters, Foreach_check_command(), c, cmd_num)) {
                        goto reply;
                    }
                    goto unknown_command;
                
                case 110: // set line number
                    break;
                
                case 17: {
                    if (m_state == STATE_PLANNING) {
                        return;
                    }
                    TupleForEachForward(&m_axes, Foreach_enable_stepper(), c, true);
                    now_inactive(c);
                } break;
                
                case 18: // disable steppers or set timeout
                case 84: {
                    if (m_state == STATE_PLANNING) {
                        return;
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
                    m_planning.distance = 0.0;
                    for (GcodePartsSizeType i = 1; i < m_cmd->num_parts; i++) {
                        GcodeParserCommandPart *part = &m_cmd->parts[i];
                        TupleForEachForward(&m_axes, Foreach_update_req_pos(), c, part, &changed);
                        if (part->code == 'F') {
                            m_max_cart_speed = strtod(part->data, NULL) * Params::SpeedLimitMultiply::value();
                        }
                    }
                    if (changed) {
                        if (m_state != STATE_PLANNING) {
                            m_planner.start(c, c.clock()->getTime(c));
                            m_planner.startStepping(c);
                            m_state = STATE_PLANNING;
                            m_planning.pull_pending = false;
                            now_active(c);
                        }
                        m_planning.req_pending = true;
                        m_planning.distance = sqrt(m_planning.distance);
                        if (m_planning.pull_pending) {
                            send_req_to_planner(c);
                        }
                        return;
                    }
                } break;
                
                case 21: // set units to millimeters
                    break;
                
                case 28: { // home axes
                    if (m_state == STATE_PLANNING) {
                        return;
                    }
                    AxisMaskType mask = 0;
                    for (GcodePartsSizeType i = 1; i < m_cmd->num_parts; i++) {
                        TupleForEachForward(&m_axes, Foreach_update_homing_mask(), &mask, &m_cmd->parts[i]);
                    }
                    if (mask == 0) {
                        mask = -1;
                    }
                    m_homing.rem_axes = 0;
                    m_homing.failed = false;
                    TupleForEachForward(&m_axes, Foreach_start_homing(), c, mask);
                    if (m_homing.rem_axes > 0) {
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
        AMBRO_ASSERT(m_homing.rem_axes == 0)
        
        if (m_homing.failed) {
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
        AMBRO_ASSERT(m_planning.req_pending)
        AMBRO_ASSERT(m_cmd)
        AMBRO_ASSERT(m_planning.pull_pending)
        
        PlannerInputCommand cmd;
        cmd.rel_max_v = FloatMakePosOrPosZero((m_max_cart_speed / m_planning.distance) / Clock::time_freq);
        TupleForEachForward(&m_axes, Foreach_write_planner_command(), &cmd);
        m_planner.commandDone(c, cmd);
        m_planning.req_pending = false;
        m_planning.pull_pending = false;
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
    
    void planner_pull_cmd_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_PLANNING)
        AMBRO_ASSERT(!m_planning.pull_pending)
        
        m_planning.pull_pending = true;
        if (m_planning.req_pending) {
            send_req_to_planner(c);
        }
    }
    
    void planner_buffer_full_handler (Context c)
    {
        this->debugAccess(c);
    }
    
    void planner_buffer_empty_handler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_PLANNING)
        AMBRO_ASSERT(m_planning.pull_pending)
        AMBRO_ASSERT(!m_planning.req_pending)
        
        m_planner.stop(c);
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
    ThePlanner m_planner;
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
            AxisCountType rem_axes;
            bool failed;
        } m_homing;
        struct {
            bool req_pending;
            bool pull_pending;
            double distance;
        } m_planning;
    };
    
    struct SerialRecvHandler : public AMBRO_WCALLBACK_TD(&PrinterMain::serial_recv_handler, &PrinterMain::m_serial) {};
    struct SerialSendHandler : public AMBRO_WCALLBACK_TD(&PrinterMain::serial_send_handler, &PrinterMain::m_serial) {};
    struct PlannerPullCmdHandler : public AMBRO_WCALLBACK_TD(&PrinterMain::planner_pull_cmd_handler, &PrinterMain::m_planner) {};
    struct PlannerBufferFullHandler : public AMBRO_WCALLBACK_TD(&PrinterMain::planner_buffer_full_handler, &PrinterMain::m_planner) {};
    struct PlannerBufferEmptyHandler : public AMBRO_WCALLBACK_TD(&PrinterMain::planner_buffer_empty_handler, &PrinterMain::m_planner) {};
};

#include <aprinter/EndNamespace.h>

#endif

