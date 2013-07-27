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
#include <aprinter/system/AvrSerial.h>
#include <aprinter/devices/Blinker.h>
#include <aprinter/stepper/Steppers.h>
#include <aprinter/stepper/AxisSharer.h>
#include <aprinter/stepper/AxisHomer.h>
#include <aprinter/stepper/GcodeParser.h>
#include <aprinter/stepper/MotionPlanner.h>

#include <aprinter/BeginNamespace.h>

template <
    typename TSerial, typename TLedPin, typename TLedBlinkInterval, typename TDefaultInactiveTime,
    typename TSpeedLimitMultiply, typename TAxesList
>
struct PrinterMainParams {
    using Serial = TSerial;
    using LedPin = TLedPin;
    using LedBlinkInterval = TLedBlinkInterval;
    using DefaultInactiveTime = TDefaultInactiveTime;
    using SpeedLimitMultiply = TSpeedLimitMultiply;
    using AxesList = TAxesList;
};

template <uint32_t tbaud, typename TTheGcodeParserParams>
struct PrinterMainSerialParams {
    static const uint32_t baud = tbaud;
    using TheGcodeParserParams = TTheGcodeParserParams;
};

template <
    char tname,
    typename TDirPin, typename TStepPin, typename TEnablePin, bool TInvertDir,
    typename TTheAxisStepperParams, typename TAbsVelFixedType, typename TAbsAccFixedType,
    typename TDefaultStepsPerUnit, typename TDefaultMaxSpeed, typename TDefaultMaxAccel,
    typename TDefaultOffset, typename TDefaultLimit, bool tenable_cartesian_speed_limit,
    typename THoming
>
struct PrinterMainAxisParams {
    static const char name = tname;
    using DirPin = TDirPin;
    using StepPin = TStepPin;
    using EnablePin = TEnablePin;
    static const bool InvertDir = TInvertDir;
    using TheAxisStepperParams = TTheAxisStepperParams;
    using AbsVelFixedType = TAbsVelFixedType;
    using AbsAccFixedType = TAbsAccFixedType;
    using DefaultStepsPerUnit = TDefaultStepsPerUnit;
    using DefaultMaxSpeed = TDefaultMaxSpeed;
    using DefaultMaxAccel = TDefaultMaxAccel;
    using DefaultOffset = TDefaultOffset;
    using DefaultLimit = TDefaultLimit;
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
        using AbsVelFixedType = typename AxisSpec::AbsVelFixedType;
        using AbsAccFixedType = typename AxisSpec::AbsAccFixedType;
        static const char axis_name = AxisSpec::name;
        
        AMBRO_STRUCT_IF(HomingFeature, AxisSpec::Homing::enabled) {
            struct HomerFinishedHandler;
            using Homer = AxisHomer<Context, Sharer, AbsVelFixedType, AbsAccFixedType, typename AxisSpec::Homing::EndPin, AxisSpec::Homing::end_invert, AxisSpec::Homing::home_dir, HomerFinishedHandler>;
            
            Axis * parent ()
            {
                return AMBRO_WMEMB_TD(&Axis::m_homing_feature)::container(this);
            }
            
            void init (Context c)
            {
                m_fast_max_dist = AxisSpec::Homing::DefaultFastMaxDist::value();
                m_retract_dist = AxisSpec::Homing::DefaultRetractDist::value();
                m_slow_max_dist = AxisSpec::Homing::DefaultSlowMaxDist::value();
                m_fast_speed = AxisSpec::Homing::DefaultFastSpeed::value();
                m_retract_speed = AxisSpec::Homing::DefaultRetractSpeed::value();
                m_slow_speed = AxisSpec::Homing::DefaultSlowSpeed::value();
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
                AMBRO_ASSERT(axis->m_state == AXIS_STATE_IDLE)
                
                if (!(mask & ((AxisMaskType)1 << AxisIndex))) {
                    return;
                }
                
                typename Homer::HomingParams params;
                params.fast_max_dist = axis->dist_from_real(m_fast_max_dist);
                params.retract_dist = axis->dist_from_real(m_retract_dist);
                params.slow_max_dist = axis->dist_from_real(m_slow_max_dist);
                params.fast_speed = axis->speed_from_real(m_fast_speed);
                params.retract_speed = axis->speed_from_real(m_retract_speed);
                params.slow_speed = axis->speed_from_real(m_slow_speed);
                params.max_accel = axis->accel_from_real(axis->m_max_accel);
                
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
                axis->m_end_pos = StepFixedType::importBits(0);
                axis->m_req_pos = -axis->m_offset;
                axis->m_req_step_pos = StepFixedType::importBits(0);
                axis->m_state = AXIS_STATE_IDLE;
                o->m_homing.rem_axes--;
                if (!success) {
                    o->m_homing.failed = true;
                }
                if (o->m_homing.rem_axes == 0) {
                    o->homing_finished(c);
                }
            }
            
            Homer m_homer;
            float m_fast_max_dist;
            float m_retract_dist;
            float m_slow_max_dist;
            float m_fast_speed;
            float m_retract_speed;
            float m_slow_speed;
            
            struct HomerFinishedHandler : public AMBRO_WCALLBACK_TD(&HomingFeature::homer_finished_handler, &HomingFeature::m_homer) {};
        } AMBRO_STRUCT_ELSE(HomingFeature) {
            void init (Context c) {}
            void deinit (Context c) {}
            void start_homing (Context c, AxisMaskType mask) {}
        };
        
        enum {AXIS_STATE_IDLE, AXIS_STATE_HOMING, AXIS_SPACE_RUNNING};
        
        PrinterMain * parent ()
        {
            return AMBRO_WMEMB_TD(&PrinterMain::m_axes)::container(TupleGetTuple<AxisIndex, AxesTuple>(this));
        }
        
        StepFixedType dist_from_real (float x)
        {
            return StepFixedType::importDoubleSaturated(x * m_steps_per_unit);
        }
        
        AbsVelFixedType speed_from_real (float v)
        {
            return AbsVelFixedType::importDoubleSaturated(v * (m_steps_per_unit / Clock::time_freq));
        }
        
        AbsAccFixedType accel_from_real (float a)
        {
            return AbsAccFixedType::importDoubleSaturated(a * (m_steps_per_unit / (Clock::time_freq * Clock::time_freq)));
        }
        
        void init (Context c)
        {
            m_sharer.init(c);
            m_state = AXIS_STATE_IDLE;
            m_steps_per_unit = AxisSpec::DefaultStepsPerUnit::value();
            m_max_speed = AxisSpec::DefaultMaxSpeed::value();
            m_max_accel = AxisSpec::DefaultMaxAccel::value();
            m_offset = AxisSpec::DefaultOffset::value();
            m_limit = limit_limit(AxisSpec::DefaultLimit::value());
            m_homing_feature.init(c);
            m_end_pos = StepFixedType::importBits(0);
            m_req_pos = -m_offset;
            m_req_step_pos = StepFixedType::importBits(0);
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
                double req_pos = strtod(part->data, NULL);
                compute_req(req_pos);
                if (m_req_step_pos != m_end_pos) {
                    *changed = true;
                    if (AxisSpec::enable_cartesian_speed_limit) {
                        double delta = ((double)m_req_step_pos.bitsValue() - (double)m_end_pos.bitsValue()) / m_steps_per_unit;
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
            if (m_req_step_pos >= m_end_pos) {
                mycmd->dir = true;
                mycmd->x = StepFixedType::importBits(m_req_step_pos.bitsValue() - m_end_pos.bitsValue());
            } else {
                mycmd->dir = false;
                mycmd->x = StepFixedType::importBits(m_end_pos.bitsValue() - m_req_step_pos.bitsValue());
            }
            mycmd->max_v = speed_from_real(m_max_speed);
            mycmd->max_a = accel_from_real(m_max_accel);
            m_end_pos = m_req_step_pos;
        }
        
        void append_position (Context c)
        {
            parent()->reply_append_fmt(c, "%c:%f", axis_name, m_req_pos);
        }
        
        void compute_req (float req_pos)
        {
            if (m_relative_positioning) {
                req_pos += m_req_pos;
            }
            if (req_pos < -m_offset) {
                req_pos = -m_offset;
            } else if (req_pos > m_limit) {
                req_pos = m_limit;
            }
            m_req_pos = req_pos;
            m_req_step_pos = dist_from_real(m_offset + req_pos);
        }
        
        void set_relative_positioning (bool relative)
        {
            m_relative_positioning = relative;
        }
        
        void set_position (Context c, GcodeParserCommandPart *part, bool *found_axes)
        {
            AMBRO_ASSERT(m_end_pos == m_req_step_pos)
            
            if (part->code == axis_name) {
                *found_axes = true;
                if (AxisSpec::Homing::enabled) {
                    parent()->reply_append_str(c, "Error:G92 on homable axis\n");
                    return;
                }
                double req_pos = strtod(part->data, NULL);
                if (req_pos < -m_offset) {
                    req_pos = -m_offset;
                } else if (req_pos > m_limit) {
                    req_pos = m_limit;
                }
                double delta_steps = (req_pos - m_req_pos) * m_steps_per_unit;
                double new_steps = (double)m_end_pos.bitsValue() + delta_steps;
                m_end_pos = StepFixedType::importDoubleSaturated(new_steps);
                m_req_step_pos = m_end_pos;
                m_req_pos = req_pos;
            }
        }
        
        double limit_limit (double limit)
        {
            if (limit < -m_offset) {
                limit = -m_offset;
            } else {
                double max = (StepFixedType::maxValue().doubleValue() / m_steps_per_unit) - m_offset;
                if (limit > max) {
                    limit = max;
                }
            }
            return limit;
        }
        
        Sharer m_sharer;
        uint8_t m_state;
        float m_steps_per_unit;
        float m_max_speed;
        float m_max_accel;
        float m_offset;
        float m_limit;
        HomingFeature m_homing_feature;
        StepFixedType m_end_pos;
        float m_req_pos;
        StepFixedType m_req_step_pos;
        bool m_relative_positioning;
        
        struct SharerGetStepperHandler : public AMBRO_WCALLBACK_TD(&Axis::stepper, &Axis::m_sharer) {};
    };
    
    template <typename TheAxis>
    using MakePlannerAxisSpec = MotionPlannerAxisSpec<
        typename TheAxis::Sharer,
        typename TheAxis::AbsVelFixedType,
        typename TheAxis::AbsAccFixedType
    >;
    
    using MotionPlannerAxes = MapTypeList<IndexElemList<AxesList, Axis>, TemplateFunc<MakePlannerAxisSpec>>;
    using ThePlanner = MotionPlanner<Context, MotionPlannerAxes, PlannerPullCmdHandler, PlannerBufferFullHandler, PlannerBufferEmptyHandler>;
    using PlannerSharersTuple = typename ThePlanner::SharersTuple;
    using PlannerInputCommand = typename ThePlanner::InputCommand;
    using AxesTuple = IndexElemTuple<AxesList, Axis>;
    
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
        m_inactive_time = Params::DefaultInactiveTime::value() * Clock::time_freq;
        m_recv_next_error = 0;
        m_line_number = 0;
        m_cmd = NULL;
        m_reply_length = 0;
        m_state = STATE_IDLE;
        
        reply_append_str(c, "APrinter\n");
        reply_send(c);
        
        this->debugInit(c);
    }

    void deinit (Context c)
    {
        this->debugDeinit(c);
        
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
            return process_received_command(c);
        }
        if (overrun) {
            m_serial.recvConsume(c, avail);
            m_serial.recvClearOverrun(c);
            m_gcode_parser.resetCommand(c);
            m_recv_next_error = TheGcodeParser::ERROR_RECV_OVERRUN;
        }
    }
    
    void process_received_command (Context c)
    {
        AMBRO_ASSERT(m_cmd)
        AMBRO_ASSERT(m_state == STATE_IDLE || m_state == STATE_PLANNING)
        AMBRO_ASSERT(m_state != STATE_PLANNING || !m_planning.req_pending)
        
        char cmd_code;
        int cmd_num;
        bool is_m110;
        
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
        
        is_m110 = (cmd_code == 'M' && cmd_num == 110);
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
        
        switch (cmd_code) {
            case 'M': switch (cmd_num) {
                default: 
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
                    m_planning.max_cart_speed = INFINITY;
                    for (GcodePartsSizeType i = 1; i < m_cmd->num_parts; i++) {
                        GcodeParserCommandPart *part = &m_cmd->parts[i];
                        TupleForEachForward(&m_axes, Foreach_update_req_pos(), c, part, &changed);
                        if (part->code == 'F') {
                            m_planning.max_cart_speed = strtod(part->data, NULL) * Params::SpeedLimitMultiply::value();
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
        finish_command(c);
    }
    
    void finish_command (Context c)
    {
        AMBRO_ASSERT(m_cmd)
        
        reply_append_str(c, "ok\n");
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
        finish_command(c);
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
        cmd.rel_max_v = ThePlanner::RelSpeedType::importDoubleSaturated((m_planning.max_cart_speed / m_planning.distance) / Clock::time_freq);
        if (cmd.rel_max_v.bitsValue() == 0) {
            cmd.rel_max_v = ThePlanner::RelSpeedType::importBits(1);
        }
        TupleForEachForward(&m_axes, Foreach_write_planner_command(), &cmd);
        m_planner.commandDone(c, cmd);
        m_planning.req_pending = false;
        m_planning.pull_pending = false;
        finish_command(c);
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
            process_received_command(c);
        }
    }
    
    TheBlinker m_blinker;
    TheSteppers m_steppers;
    AxesTuple m_axes;
    TheSerial m_serial;
    TheGcodeParser m_gcode_parser;
    typename Loop::QueuedEvent m_disable_timer;
    ThePlanner m_planner;
    TimeType m_inactive_time;
    TimeType m_last_active_time;
    int8_t m_recv_next_error;
    uint32_t m_line_number;
    GcodeParserCommand *m_cmd;
    char m_reply_buf[reply_buffer_size];
    ReplyBufferSizeType m_reply_length;
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
            double max_cart_speed;
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

