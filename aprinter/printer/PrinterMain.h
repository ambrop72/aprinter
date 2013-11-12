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

#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/MapTypeList.h>
#include <aprinter/meta/TemplateFunc.h>
#include <aprinter/meta/TypeListGet.h>
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
#include <aprinter/meta/TypeListFold.h>
#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/If.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Likely.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/math/FloatTools.h>
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
    typename TForceTimeout, template <typename, typename, typename> class TEventChannelTimer,
    template <typename, typename, typename> class TWatchdogTemplate, typename TWatchdogParams,
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
    template <typename X, typename Y, typename Z> using EventChannelTimer = TEventChannelTimer<X, Y, Z>;
    template <typename X, typename Y, typename Z> using WatchdogTemplate = TWatchdogTemplate<X, Y, Z>;
    using WatchdogParams = TWatchdogParams;
    using AxesList = TAxesList;
    using HeatersList = THeatersList;
    using FansList = TFansList;
};

template <
    uint32_t tbaud, typename TTheGcodeParserParams,
    template <typename, typename, int, int, typename, typename, typename> class TSerialTemplate,
    typename TSerialParams
>
struct PrinterMainSerialParams {
    static const uint32_t baud = tbaud;
    using TheGcodeParserParams = TTheGcodeParserParams;
    template <typename S, typename X, int Y, int Z, typename W, typename Q, typename R> using SerialTemplate = TSerialTemplate<S, X, Y, Z, W, Q, R>;
    using SerialParams = TSerialParams;
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
    char TName, int TSetMCommand, int TWaitMCommand, int TSetConfigMCommand,
    typename TAdcPin, typename TOutputPin,
    typename TFormula,
    typename TMinSafeTemp, typename TMaxSafeTemp,
    typename TPulseInterval,
    typename TControlInterval,
    template<typename, typename, typename> class TControl,
    typename TControlParams,
    typename TTheTemperatureObserverParams,
    template<typename, typename, typename> class TTimerTemplate
>
struct PrinterMainHeaterParams {
    static const char Name = TName;
    static const int SetMCommand = TSetMCommand;
    static const int WaitMCommand = TWaitMCommand;
    static const int SetConfigMCommand = TSetConfigMCommand;
    using AdcPin = TAdcPin;
    using OutputPin = TOutputPin;
    using Formula = TFormula;
    using MinSafeTemp = TMinSafeTemp;
    using MaxSafeTemp = TMaxSafeTemp;
    using PulseInterval = TPulseInterval;
    using ControlInterval = TControlInterval;
    template <typename X, typename Y, typename Z> using Control = TControl<X, Y, Z>;
    using ControlParams = TControlParams;
    using TheTemperatureObserverParams = TTheTemperatureObserverParams;
    template <typename X, typename Y, typename Z> using TimerTemplate = TTimerTemplate<X, Y, Z>;
};

template <
    int TSetMCommand, int TOffMCommand,
    typename TOutputPin, typename TPulseInterval, typename TSpeedMultiply,
    template<typename, typename, typename> class TTimerTemplate
>
struct PrinterMainFanParams {
    static const int SetMCommand = TSetMCommand;
    static const int OffMCommand = TOffMCommand;
    using OutputPin = TOutputPin;
    using PulseInterval = TPulseInterval;
    using SpeedMultiply = TSpeedMultiply;
    template <typename X, typename Y, typename Z> using TimerTemplate = TTimerTemplate<X, Y, Z>;
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
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_print_config, print_config)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_continue_locking_helper, continue_locking_helper)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_continue_planned_helper, continue_planned_helper)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_continue_unplanned_helper, continue_unplanned_helper)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_finish_locked_helper, finish_locked_helper)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_run_for_state_command, run_for_state_command)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_finish_init, finish_init)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_ChannelPayload, ChannelPayload)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_EventLoopFastEvents, EventLoopFastEvents)
    
    struct WatchdogPosition;
    struct BlinkerPosition;
    struct SteppersPosition;
    template <int AxisIndex> struct AxisPosition;
    template <int AxisIndex> struct HomingFeaturePosition;
    template <int AxisIndex> struct HomingStatePosition;
    struct SerialFeaturePosition;
    struct PlannerPosition;
    template <int HeaterIndex> struct HeaterPosition;
    template <int HeaterIndex> struct MainControlPosition;
    template <int FanIndex> struct FanPosition;
    
    struct BlinkerHandler;
    template <int AxisIndex> struct PlannerGetAxisStepper;
    struct PlannerPullHandler;
    struct PlannerFinishedHandler;
    struct PlannerChannelCallback;
    template <int AxisIndex> struct AxisStepperConsumersList;
    
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
    
    using TheWatchdog = typename Params::template WatchdogTemplate<WatchdogPosition, Context, typename Params::WatchdogParams>;
    using TheBlinker = Blinker<BlinkerPosition, Context, typename Params::LedPin, BlinkerHandler>;
    using StepperDefsList = MapTypeList<AxesList, TemplateFunc<MakeStepperDef>>;
    using TheSteppers = Steppers<SteppersPosition, Context, StepperDefsList>;
    
    static_assert(Params::LedBlinkInterval::value() < TheWatchdog::WatchdogTime / 2.0, "");
    
    enum {COMMAND_IDLE, COMMAND_LOCKING, COMMAND_LOCKED};
    
    template <typename ChannelCommonPosition, typename Channel>
    struct ChannelCommon {
        using TheGcodeParser = typename Channel::TheGcodeParser;
        using GcodeParserCommand = typename TheGcodeParser::Command;
        using GcodeParserCommandPart = typename TheGcodeParser::CommandPart;
        using GcodePartsSizeType = typename TheGcodeParser::PartsSizeType;
        
        static ChannelCommon * self (Context c)
        {
            return PositionTraverse<typename Context::TheRootPosition, ChannelCommonPosition>(c.root());
        }
        
        // channel interface
        
        static void init (Context c)
        {
            ChannelCommon *o = self(c);
            o->m_state = COMMAND_IDLE;
            o->m_line_number = 0;
            o->m_cmd = NULL;
        }
        
        static void startCommand (Context c, GcodeParserCommand *cmd)
        {
            ChannelCommon *o = self(c);
            AMBRO_ASSERT(o->m_state == COMMAND_IDLE)
            AMBRO_ASSERT(!o->m_cmd)
            AMBRO_ASSERT(cmd)
            
            o->m_cmd = cmd;
            if (o->m_cmd->num_parts <= 0) {
                char const *err = "unknown error";
                switch (o->m_cmd->num_parts) {
                    case 0: err = "empty command"; break;
                    case TheGcodeParser::ERROR_TOO_MANY_PARTS: err = "too many parts"; break;
                    case TheGcodeParser::ERROR_INVALID_PART: err = "invalid part"; break;
                    case TheGcodeParser::ERROR_CHECKSUM: err = "incorrect checksum"; break;
                    case TheGcodeParser::ERROR_RECV_OVERRUN: err = "receive buffer overrun"; break;
                }
                reply_append_str(c, "Error:");
                reply_append_str(c, err);
                reply_append_ch(c, '\n');
                return finishCommand(c);
            }
            o->m_cmd_code = o->m_cmd->parts[0].code;
            o->m_cmd_num = atoi(o->m_cmd->parts[0].data);
            bool is_m110 = (o->m_cmd_code == 'M' && o->m_cmd_num == 110);
            if (is_m110) {
                o->m_line_number = get_command_param_uint32(c, 'L', (o->m_cmd->have_line_number ? o->m_cmd->line_number : -1));
            }
            if (o->m_cmd->have_line_number) {
                if (o->m_cmd->line_number != o->m_line_number) {
                    reply_append_str(c, "Error:Line Number is not Last Line Number+1, Last Line:");
                    reply_append_uint32(c, (uint32_t)(o->m_line_number - 1));
                    reply_append_ch(c, '\n');
                    return finishCommand(c);
                }
            }
            if (o->m_cmd->have_line_number || is_m110) {
                o->m_line_number++;
            }
            work_command<ChannelCommon>(c);
        }
        
        static void maybePauseLockingCommand (Context c)
        {
            ChannelCommon *o = self(c);
            AMBRO_ASSERT(!o->m_cmd || o->m_state == COMMAND_LOCKING)
            AMBRO_ASSERT(o->m_cmd || o->m_state == COMMAND_IDLE)
            
            o->m_state = COMMAND_IDLE;
        }
        
        static bool maybeResumeLockingCommand (Context c)
        {
            ChannelCommon *o = self(c);
            PrinterMain *m = PrinterMain::self(c);
            AMBRO_ASSERT(o->m_state == COMMAND_IDLE)
            
            if (!o->m_cmd) {
                return false;
            }
            o->m_state = COMMAND_LOCKING;
            if (!m->m_unlocked_timer.isSet(c)) {
                m->m_unlocked_timer.prependNowNotAlready(c);
            }
            return true;
        }
        
        static void finishCommand (Context c, bool no_ok = false)
        {
            ChannelCommon *o = self(c);
            PrinterMain *m = PrinterMain::self(c);
            AMBRO_ASSERT(o->m_cmd)
            AMBRO_ASSERT(o->m_state == COMMAND_IDLE || o->m_state == COMMAND_LOCKED)
            
            Channel::finish_command_impl(c, no_ok);
            o->m_cmd = NULL;
            if (o->m_state == COMMAND_LOCKED) {
                AMBRO_ASSERT(m->m_locked)
                o->m_state = COMMAND_IDLE;
                m->m_locked = false;
                if (!m->m_unlocked_timer.isSet(c)) {
                    m->m_unlocked_timer.prependNowNotAlready(c);
                }
            }
        }
        
        // command interface
        
        static bool tryUnplannedCommand (Context c)
        {
            ChannelCommon *o = self(c);
            PrinterMain *m = PrinterMain::self(c);
            AMBRO_ASSERT(o->m_state == COMMAND_IDLE || o->m_state == COMMAND_LOCKED)
            AMBRO_ASSERT(o->m_cmd)
            
            if (o->m_state == COMMAND_LOCKED) {
                AMBRO_ASSERT(!m->m_planning)
                return true;
            }
            if (m->m_locked) {
                o->m_state = COMMAND_LOCKING;
                return false;
            }
            o->m_state = COMMAND_LOCKED;
            m->m_locked = true;
            if (m->m_planning) {
                AMBRO_ASSERT(!m->m_planning_req_pending)
                if (m->m_planning_pull_pending) {
                    m->m_planner.waitFinished(c);
                }
                return false;
            }
            return true;
        }
        
        static bool tryPlannedCommand (Context c)
        {
            ChannelCommon *o = self(c);
            PrinterMain *m = PrinterMain::self(c);
            AMBRO_ASSERT(o->m_state == COMMAND_IDLE || o->m_state == COMMAND_LOCKED)
            AMBRO_ASSERT(o->m_cmd)
            
            if (o->m_state == COMMAND_LOCKED) {
                AMBRO_ASSERT(m->m_planning)
                AMBRO_ASSERT(m->m_planning_req_pending)
                AMBRO_ASSERT(m->m_planning_pull_pending)
                return true;
            }
            if (m->m_locked) {
                o->m_state = COMMAND_LOCKING;
                return false;
            }
            o->m_state = COMMAND_LOCKED;
            m->m_locked = true;
            if (!m->m_planning) {
                m->m_planner.init(c, false);
                m->m_planning = true;
                m->m_planning_pull_pending = false;
                now_active(c);
            }
            m->m_planning_req_pending = true;
            return m->m_planning_pull_pending;
        }
        
        template <typename ThePlannerInputCommand>
        static void submitPlannedCommand (Context c, ThePlannerInputCommand *cmd)
        {
            ChannelCommon *o = self(c);
            PrinterMain *m = PrinterMain::self(c);
            AMBRO_ASSERT(o->m_state == COMMAND_IDLE)
            AMBRO_ASSERT(!o->m_cmd)
            AMBRO_ASSERT(!m->m_locked)
            AMBRO_ASSERT(m->m_planning)
            AMBRO_ASSERT(m->m_planning_req_pending)
            AMBRO_ASSERT(m->m_planning_pull_pending)
            
            m->m_planner.commandDone(c, cmd);
            m->m_planning_req_pending = false;
            m->m_planning_pull_pending = false;
            m->m_force_timer.unset(c);
        }
        
        static GcodeParserCommandPart * find_command_param (Context c, char code)
        {
            ChannelCommon *o = self(c);
            AMBRO_ASSERT(o->m_cmd)
            AMBRO_ASSERT(code >= 'A')
            AMBRO_ASSERT(code <= 'Z')
            
            for (GcodePartsSizeType i = 1; i < o->m_cmd->num_parts; i++) {
                if (o->m_cmd->parts[i].code == code) {
                    return &o->m_cmd->parts[i];
                }
            }
            return NULL;
        }
        
        static uint32_t get_command_param_uint32 (Context c, char code, uint32_t default_value)
        {
            GcodeParserCommandPart *part = find_command_param(c, code);
            if (!part) {
                return default_value;
            }
            return strtoul(part->data, NULL, 10);
        }
        
        static double get_command_param_double (Context c, char code, double default_value)
        {
            GcodeParserCommandPart *part = find_command_param(c, code);
            if (!part) {
                return default_value;
            }
            return strtod(part->data, NULL);
        }
        
        static bool find_command_param_double (Context c, char code, double *out)
        {
            GcodeParserCommandPart *part = find_command_param(c, code);
            if (!part) {
                return false;
            }
            *out = strtod(part->data, NULL);
            return true;
        }
        
        static void reply_append_str (Context c, char const *str)
        {
            Channel::reply_append_buffer_impl(c, str, strlen(str));
        }
        
        static void reply_append_ch (Context c, char ch)
        {
            Channel::reply_append_ch_impl(c, ch);
        }
        
        static void reply_append_double (Context c, double x)
        {
            char buf[30];
#if defined(AMBROLIB_AVR)
            uint8_t len = sprintf(buf, "%g", x);
            Channel::reply_append_buffer_impl(c, buf, len);
#else        
            FloatToStrSoft(x, buf);
            Channel::reply_append_buffer_impl(c, buf, strlen(buf));
#endif
        }
        
        static void reply_append_uint32 (Context c, uint32_t x)
        {
            char buf[11];
#if defined(AMBROLIB_AVR)
            uint8_t len = sprintf(buf, "%" PRIu32, x);
#else
            uint8_t len = PrintNonnegativeIntDecimal<uint32_t>(x, buf);
#endif
            Channel::reply_append_buffer_impl(c, buf, len);
        }
        
        // helper function to do something for the first channel in the given state
        
        template <typename Obj, typename Func, typename... Args>
        static bool run_for_state_command (Context c, uint8_t state, Obj *obj, Func func, Args... args)
        {
            ChannelCommon *o = self(c);
            PrinterMain *m = PrinterMain::self(c);
            
            if (o->m_state == state) {
                func(obj, c, o, args...);
                return false;
            }
            return true;
        }
        
        uint8_t m_state;
        uint32_t m_line_number;
        GcodeParserCommand *m_cmd;
        char m_cmd_code;
        int m_cmd_num;
    };
    
    struct SerialFeature {
        struct SerialPosition;
        struct GcodeParserPosition;
        struct ChannelCommonPosition;
        struct SerialRecvHandler;
        struct SerialSendHandler;
        
        static const int serial_recv_buffer_bits = 6;
        static const int serial_send_buffer_bits = 8;
        using TheSerial = typename Params::Serial::template SerialTemplate<SerialPosition, Context, serial_recv_buffer_bits, serial_send_buffer_bits, typename Params::Serial::SerialParams, SerialRecvHandler, SerialSendHandler>;
        using RecvSizeType = typename TheSerial::RecvSizeType;
        using SendSizeType = typename TheSerial::SendSizeType;
        using TheGcodeParser = GcodeParser<GcodeParserPosition, Context, typename Params::Serial::TheGcodeParserParams, typename RecvSizeType::IntType>;
        using TheChannelCommon = ChannelCommon<ChannelCommonPosition, SerialFeature>;
        
        static SerialFeature * self (Context c)
        {
            return PositionTraverse<typename Context::TheRootPosition, SerialFeaturePosition>(c.root());
        }
        
        static void init (Context c)
        {
            SerialFeature *o = self(c);
            o->m_serial.init(c, Params::Serial::baud);
            o->m_gcode_parser.init(c);
            o->m_channel_common.init(c);
            o->m_recv_next_error = 0;
        }
        
        static void deinit (Context c)
        {
            SerialFeature *o = self(c);
            o->m_gcode_parser.deinit(c);
            o->m_serial.deinit(c);
        }
        
        static void serial_recv_handler (TheSerial *, Context c)
        {
            SerialFeature *o = self(c);
            
            if (o->m_channel_common.m_cmd) {
                return;
            }
            if (!o->m_gcode_parser.haveCommand(c)) {
                o->m_gcode_parser.startCommand(c, o->m_serial.recvGetChunkPtr(c), o->m_recv_next_error);
                o->m_recv_next_error = 0;
            }
            bool overrun;
            RecvSizeType avail = o->m_serial.recvQuery(c, &overrun);
            typename TheGcodeParser::Command *cmd = o->m_gcode_parser.extendCommand(c, avail.value());
            if (cmd) {
                return o->m_channel_common.startCommand(c, cmd);
            }
            if (overrun) {
                o->m_serial.recvConsume(c, avail);
                o->m_serial.recvClearOverrun(c);
                o->m_gcode_parser.resetCommand(c);
                o->m_recv_next_error = TheGcodeParser::ERROR_RECV_OVERRUN;
            }
        }
        
        static void serial_send_handler (TheSerial *, Context c)
        {
        }
        
        static void finish_command_impl (Context c, bool no_ok)
        {
            SerialFeature *o = self(c);
            AMBRO_ASSERT(o->m_channel_common.m_cmd)
            
            if (!no_ok) {
                o->m_channel_common.reply_append_str(c, "ok\n");
            }
            o->m_serial.recvConsume(c, RecvSizeType::import(o->m_channel_common.m_cmd->length));
            o->m_serial.recvForceEvent(c);
        }
        
        static void reply_append_buffer_impl (Context c, char const *str, uint8_t length)
        {
            SerialFeature *o = self(c);
            SendSizeType avail = o->m_serial.sendQuery(c);
            if (length > avail.value()) {
                length = avail.value();
            }
            while (length > 0) {
                char *chunk_data = o->m_serial.sendGetChunkPtr(c);
                uint8_t chunk_length = o->m_serial.sendGetChunkLen(c, SendSizeType::import(length)).value();
                memcpy(chunk_data, str, chunk_length);
                o->m_serial.sendProvide(c, SendSizeType::import(chunk_length));
                str += chunk_length;
                length -= chunk_length;
            }
        }
        
        static void reply_append_ch_impl (Context c, char ch)
        {
            SerialFeature *o = self(c);
            if (o->m_serial.sendQuery(c).value() > 0) {
                *o->m_serial.sendGetChunkPtr(c) = ch;
                o->m_serial.sendProvide(c, SendSizeType::import(1));
            }
        }
        
        TheSerial m_serial;
        TheGcodeParser m_gcode_parser;
        TheChannelCommon m_channel_common;
        int8_t m_recv_next_error;
        
        struct SerialPosition : public MemberPosition<SerialFeaturePosition, TheSerial, &SerialFeature::m_serial> {};
        struct GcodeParserPosition : public MemberPosition<SerialFeaturePosition, TheGcodeParser, &SerialFeature::m_gcode_parser> {};
        struct ChannelCommonPosition : public MemberPosition<SerialFeaturePosition, TheChannelCommon, &SerialFeature::m_channel_common> {};
        struct SerialRecvHandler : public AMBRO_WFUNC_TD(&SerialFeature::serial_recv_handler) {};
        struct SerialSendHandler : public AMBRO_WFUNC_TD(&SerialFeature::serial_send_handler) {};
    };
    
    using ChannelCommonList = MakeTypeList<typename SerialFeature::TheChannelCommon>;
    using ChannelCommonTuple = Tuple<ChannelCommonList>;
    
    template <int TAxisIndex>
    struct Axis {
        static const int AxisIndex = TAxisIndex;
        friend PrinterMain;
        
        struct AxisStepperPosition;
        struct AxisStepperGetStepper;
        
        using AxisSpec = TypeListGet<AxesList, AxisIndex>;
        using Stepper = typename TheSteppers::template Stepper<AxisIndex>;
        using TheAxisStepper = AxisStepper<AxisStepperPosition, Context, typename AxisSpec::TheAxisStepperParams, Stepper, AxisStepperGetStepper, AxisStepperConsumersList<AxisIndex>>;
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
                
                static HomingState * self (Context c)
                {
                    return PositionTraverse<typename Context::TheRootPosition, HomingStatePosition<AxisIndex>>(c.root());
                }
                
                static TheAxisStepper * homer_get_axis_stepper (Context c)
                {
                    return &Axis::self(c)->m_axis_stepper;
                }
                
                static void homer_finished_handler (Context c, bool success)
                {
                    HomingState *o = self(c);
                    Axis *axis = Axis::self(c);
                    PrinterMain *m = PrinterMain::self(c);
                    AMBRO_ASSERT(axis->m_state == AXIS_STATE_HOMING)
                    AMBRO_ASSERT(m->m_locked)
                    AMBRO_ASSERT(m->m_homing_rem_axes > 0)
                    
                    o->m_homer.deinit(c);
                    axis->m_req_pos = (AxisSpec::Homing::home_dir ? axis->max_req_pos() : axis->min_req_pos());
                    axis->m_end_pos = AbsStepFixedType::importDoubleSaturatedRound(axis->dist_from_real(axis->m_req_pos));
                    axis->m_state = AXIS_STATE_OTHER;
                    m->m_homing_rem_axes--;
                    if (m->m_homing_rem_axes == 0) {
                        homing_finished(c);
                    }
                }
                
                Homer m_homer;
                
                struct HomerPosition : public MemberPosition<HomingStatePosition<AxisIndex>, Homer, &HomingState::m_homer> {};
                struct HomerGetAxisStepper : public AMBRO_WFUNC_TD(&HomingState::homer_get_axis_stepper) {};
                struct HomerFinishedHandler : public AMBRO_WFUNC_TD(&HomingState::homer_finished_handler) {};
            };
            
            template <typename TheHomingFeature>
            using MakeAxisStepperConsumersList = MakeTypeList<typename TheHomingFeature::HomingState::Homer::TheAxisStepperConsumer>;
            
            using EventLoopFastEvents = typename HomingState::Homer::EventLoopFastEvents;
            
            static void init (Context c)
            {
            }
            
            static void deinit (Context c)
            {
                Axis *axis = Axis::self(c);
                HomingState *hs = HomingState::self(c);
                if (axis->m_state == AXIS_STATE_HOMING) {
                    hs->m_homer.deinit(c);
                }
            }
            
            static void start_homing (Context c, AxisMaskType mask)
            {
                Axis *axis = Axis::self(c);
                PrinterMain *m = PrinterMain::self(c);
                HomingState *hs = HomingState::self(c);
                AMBRO_ASSERT(axis->m_state == AXIS_STATE_OTHER)
                
                if (!(mask & ((AxisMaskType)1 << AxisIndex))) {
                    return;
                }
                
                typename HomingState::Homer::HomingParams params;
                params.fast_max_dist = StepFixedType::importDoubleSaturated(dist_from_real(AxisSpec::Homing::DefaultFastMaxDist::value()));
                params.retract_dist = StepFixedType::importDoubleSaturated(dist_from_real(AxisSpec::Homing::DefaultRetractDist::value()));
                params.slow_max_dist = StepFixedType::importDoubleSaturated(dist_from_real(AxisSpec::Homing::DefaultSlowMaxDist::value()));
                params.fast_speed = speed_from_real(AxisSpec::Homing::DefaultFastSpeed::value());
                params.retract_speed = speed_from_real(AxisSpec::Homing::DefaultRetractSpeed::value());;
                params.slow_speed = speed_from_real(AxisSpec::Homing::DefaultSlowSpeed::value());
                params.max_accel = accel_from_real(AxisSpec::DefaultMaxAccel::value());
                
                stepper(c)->enable(c, true);
                hs->m_homer.init(c, params);
                axis->m_state = AXIS_STATE_HOMING;
                m->m_homing_rem_axes++;
            }
        } AMBRO_STRUCT_ELSE(HomingFeature) {
            struct HomingState {};
            template <typename TheHomingFeature>
            using MakeAxisStepperConsumersList = MakeTypeList<>;
            using EventLoopFastEvents = EmptyTypeList;
            static void init (Context c) {}
            static void deinit (Context c) {}
            static void start_homing (Context c, AxisMaskType mask) {}
        };
        
        enum {AXIS_STATE_OTHER, AXIS_STATE_HOMING};
        
        static Axis * self (Context c)
        {
            return PositionTraverse<typename Context::TheRootPosition, AxisPosition<AxisIndex>>(c.root());
        }
        
        static double dist_from_real (double x)
        {
            return (x * AxisSpec::DefaultStepsPerUnit::value());
        }
        
        static double dist_to_real (double x)
        {
            return (x * (1.0 / AxisSpec::DefaultStepsPerUnit::value()));
        }
        
        static double speed_from_real (double v)
        {
            return (v * (AxisSpec::DefaultStepsPerUnit::value() / Clock::time_freq));
        }
        
        static double accel_from_real (double a)
        {
            return (a * (AxisSpec::DefaultStepsPerUnit::value() / (Clock::time_freq * Clock::time_freq)));
        }
        
        static double clamp_req_pos (double req)
        {
            return fmax(min_req_pos(), fmin(max_req_pos(), req));
        }
        
        static double min_req_pos ()
        {
            return fmax(AxisSpec::DefaultMin::value(), dist_to_real(AbsStepFixedType::minValue().doubleValue()));
        }
        
        static double max_req_pos ()
        {
            return fmin(AxisSpec::DefaultMax::value(), dist_to_real(AbsStepFixedType::maxValue().doubleValue()));
        }
        
        static void init (Context c)
        {
            Axis *o = self(c);
            o->m_axis_stepper.init(c);
            o->m_state = AXIS_STATE_OTHER;
            o->m_homing_feature.init(c);
            o->m_req_pos = clamp_req_pos(0.0);
            o->m_end_pos = AbsStepFixedType::importDoubleSaturatedRound(dist_from_real(o->m_req_pos));
            o->m_relative_positioning = false;
        }
        
        static void deinit (Context c)
        {
            Axis *o = self(c);
            o->m_homing_feature.deinit(c);
            o->m_axis_stepper.deinit(c);
        }
        
        static void start_homing (Context c, AxisMaskType mask)
        {
            Axis *o = self(c);
            return o->m_homing_feature.start_homing(c, mask);
        }
        
        template <typename TheChannelCommon>
        static void update_homing_mask (TheChannelCommon *ch, AxisMaskType *mask, typename TheChannelCommon::GcodeParserCommandPart *part)
        {
            if (AxisSpec::Homing::enabled && part->code == axis_name) {
                *mask |= (AxisMaskType)1 << AxisIndex;
            }
        }
        
        static void enable_stepper (Context c, bool enable)
        {
            Axis *o = self(c);
            stepper(c)->enable(c, enable);
        }
        
        static Stepper * stepper (Context c)
        {
            return PrinterMain::self(c)->m_steppers.template getStepper<AxisIndex>(c);
        }
        
        static void init_new_pos (Context c, double *new_pos)
        {
            Axis *o = self(c);
            new_pos[AxisIndex] = o->m_req_pos;
        }
        
        template <typename TheChannelCommon>
        static void collect_new_pos (Context c, TheChannelCommon *cc, double *new_pos, typename TheChannelCommon::GcodeParserCommandPart *part)
        {
            Axis *o = self(c);
            if (AMBRO_UNLIKELY(part->code == axis_name)) {
                double req = strtod(part->data, NULL);
                if (isnan(req)) {
                    req = 0.0;
                }
                if (o->m_relative_positioning) {
                    req += o->m_req_pos;
                }
                req = clamp_req_pos(req);
                new_pos[AxisIndex] = req;
            }
        }
        
        template <typename PlannerCmd>
        static void process_new_pos (Context c, double *new_pos, double *distance_squared, double *total_steps, PlannerCmd *cmd)
        {
            Axis *o = self(c);
            AbsStepFixedType new_end_pos = AbsStepFixedType::importDoubleSaturatedRound(dist_from_real(new_pos[AxisIndex]));
            bool dir = (new_end_pos >= o->m_end_pos);
            StepFixedType move = StepFixedType::importBits(dir ? 
                ((typename StepFixedType::IntType)new_end_pos.bitsValue() - (typename StepFixedType::IntType)o->m_end_pos.bitsValue()) :
                ((typename StepFixedType::IntType)o->m_end_pos.bitsValue() - (typename StepFixedType::IntType)new_end_pos.bitsValue())
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
            o->m_end_pos = new_end_pos;
            o->m_req_pos = new_pos[AxisIndex];
        }
        
        template <typename TheChannelCommon>
        static void append_position (Context c, TheChannelCommon *cc)
        {
            Axis *o = self(c);
            cc->reply_append_ch(c, axis_name);
            cc->reply_append_ch(c, ':');
            cc->reply_append_double(c, o->m_req_pos);
        }
        
        static void set_relative_positioning (Context c, bool relative)
        {
            Axis *o = self(c);
            o->m_relative_positioning = relative;
        }
        
        template <typename TheChannelCommon>
        static void set_position (Context c, TheChannelCommon *cc, typename TheChannelCommon::GcodeParserCommandPart *part, bool *found_axes)
        {
            Axis *o = self(c);
            if (part->code == axis_name) {
                *found_axes = true;
                if (AxisSpec::Homing::enabled) {
                    cc->reply_append_str(c, "Error:G92 on homable axis\n");
                    return;
                }
                double req = strtod(part->data, NULL);
                if (isnan(req)) {
                    req = 0.0;
                }
                o->m_req_pos = clamp_req_pos(req);
                o->m_end_pos = AbsStepFixedType::importDoubleSaturatedRound(dist_from_real(o->m_req_pos));
            }
        }
        
        static void emergency ()
        {
            Stepper::emergency();
        }
        
        static Stepper * axis_get_stepper (Context c)
        {
            return stepper(c);
        }
        
        using EventLoopFastEvents = typename HomingFeature::EventLoopFastEvents;
        
        TheAxisStepper m_axis_stepper;
        uint8_t m_state;
        HomingFeature m_homing_feature;
        AbsStepFixedType m_end_pos;
        double m_req_pos;
        bool m_relative_positioning;
        
        struct AxisStepperPosition : public MemberPosition<AxisPosition<AxisIndex>, TheAxisStepper, &Axis::m_axis_stepper> {};
        struct AxisStepperGetStepper : public AMBRO_WFUNC_TD(&Axis::axis_get_stepper) {};
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
        struct SoftPwmPosition;
        struct ObserverPosition;
        
        using HeaterSpec = TypeListGet<HeatersList, HeaterIndex>;
        static const bool MainControlEnabled = (HeaterSpec::ControlInterval::value() != 0.0);
        using ValueFixedType = typename HeaterSpec::Formula::ValueFixedType;
        using MeasurementInterval = If<MainControlEnabled, typename HeaterSpec::ControlInterval, typename HeaterSpec::PulseInterval>;
        using TheControl = typename HeaterSpec::template Control<typename HeaterSpec::ControlParams, MeasurementInterval, ValueFixedType>;
        using ControlConfig = typename TheControl::Config;
        using TheSoftPwm = SoftPwm<SoftPwmPosition, Context, typename HeaterSpec::OutputPin, typename HeaterSpec::PulseInterval, SoftPwmTimerHandler, HeaterSpec::template TimerTemplate>;
        using TheObserver = TemperatureObserver<ObserverPosition, Context, typename HeaterSpec::TheTemperatureObserverParams, ObserverGetValueCallback, ObserverHandler>;
        using OutputFixedType = typename TheControl::OutputFixedType;
        
        static_assert(MainControlEnabled || TheControl::InterruptContextAllowed, "Chosen heater control algorithm is not allowed in interrupt context.");
        static_assert(!TheControl::SupportsConfig || MainControlEnabled, "Configurable heater control algorithms not allowed in interrupt context.");
        
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
        
        static Heater * self (Context c)
        {
            return PositionTraverse<typename Context::TheRootPosition, HeaterPosition<HeaterIndex>>(c.root());
        }
        
        static void init (Context c)
        {
            Heater *o = self(c);
            o->m_enabled = false;
            o->m_control_config = TheControl::makeDefaultConfig();
            TimeType time = c.clock()->getTime(c);
            o->m_main_control.init(c, time);
            o->m_softpwm.init(c, time);
            o->m_observing = false;
        }
        
        static void deinit (Context c)
        {
            Heater *o = self(c);
            if (o->m_observing) {
                o->m_observer.deinit(c);
            }
            o->m_softpwm.deinit(c);
            o->m_main_control.deinit(c);
        }
        
        template <typename ThisContext>
        static ValueFixedType get_value (ThisContext c)
        {
            return HeaterSpec::Formula::call(c.adc()->template getValue<typename HeaterSpec::AdcPin>(c));
        }
        
        template <typename TheChannelCommon>
        static void append_value (Context c, TheChannelCommon *cc)
        {
            double value = get_value(c).doubleValue();
            cc->reply_append_ch(c, ' ');
            cc->reply_append_ch(c, HeaterSpec::Name);
            cc->reply_append_ch(c, ':');
            cc->reply_append_double(c, value);
        }
        
        template <typename ThisContext>
        static void set (ThisContext c, ValueFixedType target)
        {
            Heater *o = self(c);
            AMBRO_ASSERT(target > min_safe_temp())
            AMBRO_ASSERT(target < max_safe_temp())
            
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                o->m_target = target;
                o->m_main_control.set(lock_c);
                o->m_enabled = true;
            }
        }
        
        template <typename ThisContext>
        static void unset (ThisContext c)
        {
            Heater *o = self(c);
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                o->m_enabled = false;
                o->m_main_control.unset(lock_c);
            }
        }
        
        template <typename TheChannelCommon>
        static bool check_command (Context c, TheChannelCommon *cc)
        {
            Heater *o = self(c);
            
            if (cc->m_cmd_num == HeaterSpec::WaitMCommand) {
                if (!cc->tryUnplannedCommand(c)) {
                    return false;
                }
                double target = cc->get_command_param_double(c, 'S', 0.0);
                ValueFixedType fixed_target = ValueFixedType::importDoubleSaturated(target);
                if (fixed_target > min_safe_temp() && fixed_target < max_safe_temp()) {
                    set(c, fixed_target);
                } else {
                    unset(c);
                }
                AMBRO_ASSERT(!o->m_observing)
                o->m_observer.init(c, target);
                o->m_observing = true;
                now_active(c);
                return false;
            }
            if (cc->m_cmd_num == HeaterSpec::SetMCommand) {
                if (!cc->tryPlannedCommand(c)) {
                    return false;
                }
                double target = cc->get_command_param_double(c, 'S', 0.0);
                cc->finishCommand(c);
                ValueFixedType fixed_target = ValueFixedType::importDoubleSaturated(target);
                if (!(fixed_target > min_safe_temp() && fixed_target < max_safe_temp())) {
                    fixed_target = ValueFixedType::minValue();
                }
                PlannerInputCommand cmd;
                cmd.type = 1;
                PlannerChannelPayload *payload = UnionGetElem<0>(&cmd.channel_payload);
                payload->type = HeaterIndex;
                UnionGetElem<HeaterIndex>(&payload->heaters)->target = fixed_target;
                cc->submitPlannedCommand(c, &cmd);
                return false;
            }
            if (cc->m_cmd_num == HeaterSpec::SetConfigMCommand && TheControl::SupportsConfig) {
                if (!cc->tryUnplannedCommand(c)) {
                    return false;
                }
                TheControl::setConfigCommand(c, cc, &o->m_control_config);
                cc->finishCommand(c);
                return false;
            }
            return true;
        }
        
        template <typename TheChannelCommon>
        static void print_config (Context c, TheChannelCommon *cc)
        {
            Heater *o = self(c);
            
            if (TheControl::SupportsConfig) {
                cc->reply_append_ch(c, HeaterSpec::Name);
                cc->reply_append_str(c, ": M" );
                cc->reply_append_uint32(c, HeaterSpec::SetConfigMCommand);
                TheControl::printConfig(c, cc, &o->m_control_config);
                cc->reply_append_ch(c, '\n');
            }
        }
        
        static OutputFixedType softpwm_timer_handler (typename TheSoftPwm::TimerInstance::HandlerContext c)
        {
            Heater *o = self(c);
            return o->m_main_control.get_output_for_pwm(c);
        }
        
        static double observer_get_value_callback (Context c)
        {
            Heater *o = self(c);
            return o->get_value(c).doubleValue();
        }
        
        static void observer_handler (Context c, bool state)
        {
            Heater *o = self(c);
            PrinterMain *m = PrinterMain::self(c);
            AMBRO_ASSERT(o->m_observing)
            AMBRO_ASSERT(m->m_locked)
            
            if (!state) {
                return;
            }
            o->m_observer.deinit(c);
            o->m_observing = false;
            now_inactive(c);
            finish_locked(c);
        }
        
        static void emergency ()
        {
            Context::Pins::template emergencySet<typename HeaterSpec::OutputPin>(false);
        }
        
        template <typename ThisContext, typename TheChannelPayloadUnion>
        static void channel_callback (ThisContext c, TheChannelPayloadUnion *payload_union)
        {
            ChannelPayload *payload = UnionGetElem<HeaterIndex>(payload_union);
            if (AMBRO_LIKELY(payload->target != ValueFixedType::minValue())) {
                set(c, payload->target);
            } else {
                unset(c);
            }
        }
        
        AMBRO_STRUCT_IF(MainControl, MainControlEnabled) {
            static const TimeType ControlIntervalTicks = HeaterSpec::ControlInterval::value() / Clock::time_unit;
            
            static MainControl * self (Context c)
            {
                return PositionTraverse<typename Context::TheRootPosition, MainControlPosition<HeaterIndex>>(c.root());
            }
            
            static void set (Context c) {}
            
            static void init (Context c, TimeType time)
            {
                MainControl *o = self(c);
                o->m_output = OutputFixedType::importBits(0);
                o->m_control_event.init(c, MainControl::control_event_handler);
                o->m_control_event.appendAt(c, time + (TimeType)(0.6 * ControlIntervalTicks));
                o->m_was_not_unset = false;
            }
            
            static void deinit (Context c)
            {
                MainControl *o = self(c);
                o->m_control_event.deinit(c);
            }
            
            AMBRO_ALWAYS_INLINE static void unset (Context c)
            {
                MainControl *o = self(c);
                o->m_was_not_unset = false;
                o->m_output = OutputFixedType::importBits(0);
            }
            
            static OutputFixedType get_output_for_pwm (typename TheSoftPwm::TimerInstance::HandlerContext c)
            {
                MainControl *o = self(c);
                Heater *h = Heater::self(c);
                ValueFixedType sensor_value = h->get_value(c);
                if (AMBRO_LIKELY(sensor_value <= min_safe_temp() || sensor_value >= max_safe_temp())) {
                    AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                        h->m_enabled = false;
                        unset(c);
                    }
                }
                OutputFixedType output;
                AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                    output = o->m_output;
                }
                return output;
            }
            
            static void control_event_handler (typename Loop::QueuedEvent *, Context c)
            {
                MainControl *o = self(c);
                Heater *h = Heater::self(c);
                
                o->m_control_event.appendAfterPrevious(c, ControlIntervalTicks);
                bool enabled;
                ValueFixedType target;
                bool was_not_unset;
                AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                    enabled = h->m_enabled;
                    target = h->m_target;
                    was_not_unset = o->m_was_not_unset;
                    o->m_was_not_unset = enabled;
                }
                if (AMBRO_LIKELY(enabled)) {
                    if (!was_not_unset) {
                        h->m_control.init();
                    }
                    ValueFixedType sensor_value = h->get_value(c);
                    OutputFixedType output = h->m_control.addMeasurement(sensor_value, target, &h->m_control_config);
                    AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                        if (o->m_was_not_unset) {
                            o->m_output = output;
                        }
                    }
                }
            }
            
            OutputFixedType m_output;
            typename Loop::QueuedEvent m_control_event;
            bool m_was_not_unset;
        } AMBRO_STRUCT_ELSE(MainControl) {
            static void init (Context c, TimeType time) {}
            static void deinit (Context c) {}
            static void unset (Context c) {}
            
            AMBRO_ALWAYS_INLINE static void set (Context c)
            {
                Heater *h = Heater::self(c);
                if (!h->m_enabled) {
                    h->m_control.init();
                }
            }
            
            static OutputFixedType get_output_for_pwm (typename TheSoftPwm::TimerInstance::HandlerContext c)
            {
                Heater *h = Heater::self(c);
                OutputFixedType control_value = OutputFixedType::importBits(0);
                AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                    ValueFixedType sensor_value = get_value(lock_c);
                    if (AMBRO_UNLIKELY(sensor_value <= min_safe_temp() || sensor_value >= max_safe_temp())) {
                        h->m_enabled = false;
                    }
                    if (AMBRO_LIKELY(h->m_enabled)) {
                        control_value = h->m_control.addMeasurement(sensor_value, h->m_target, &h->m_control_config);
                    }
                }
                return control_value;
            }
        };
        
        bool m_enabled;
        TheControl m_control;
        ControlConfig m_control_config;
        ValueFixedType m_target;
        TheSoftPwm m_softpwm;
        TheObserver m_observer;
        bool m_observing;
        MainControl m_main_control;
        
        struct SoftPwmTimerHandler : public AMBRO_WFUNC_TD(&Heater::softpwm_timer_handler) {};
        struct ObserverGetValueCallback : public AMBRO_WFUNC_TD(&Heater::observer_get_value_callback) {};
        struct ObserverHandler : public AMBRO_WFUNC_TD(&Heater::observer_handler) {};
        struct SoftPwmPosition : public MemberPosition<HeaterPosition<HeaterIndex>, TheSoftPwm, &Heater::m_softpwm> {};
        struct ObserverPosition : public MemberPosition<HeaterPosition<HeaterIndex>, TheObserver, &Heater::m_observer> {};
    };
    
    template <int FanIndex>
    struct Fan {
        struct SoftPwmTimerHandler;
        struct SoftPwmPosition;
        
        using FanSpec = TypeListGet<FansList, FanIndex>;
        using TheSoftPwm = SoftPwm<SoftPwmPosition, Context, typename FanSpec::OutputPin, typename FanSpec::PulseInterval, SoftPwmTimerHandler, FanSpec::template TimerTemplate>;
        using OutputFixedType = FixedPoint<8, false, -8>;
        
        struct ChannelPayload {
            OutputFixedType target;
        };
        
        static Fan * self (Context c)
        {
            return PositionTraverse<typename Context::TheRootPosition, FanPosition<FanIndex>>(c.root());
        }
        
        static void init (Context c)
        {
            Fan *o = self(c);
            o->m_target = OutputFixedType::importBits(0);
            o->m_softpwm.init(c, c.clock()->getTime(c));
        }
        
        static void deinit (Context c)
        {
            Fan *o = self(c);
            o->m_softpwm.deinit(c);
        }
        
        template <typename TheChannelCommon>
        static bool check_command (Context c, TheChannelCommon *cc)
        {
            if (cc->m_cmd_num == FanSpec::SetMCommand || cc->m_cmd_num == FanSpec::OffMCommand) {
                if (!cc->tryPlannedCommand(c)) {
                    return false;
                }
                double target = 0.0;
                if (cc->m_cmd_num == FanSpec::SetMCommand) {
                    target = 1.0;
                    if (cc->find_command_param_double(c, 'S', &target)) {
                        target *= FanSpec::SpeedMultiply::value();
                    }
                }
                cc->finishCommand(c);
                PlannerInputCommand cmd;
                cmd.type = 1;
                PlannerChannelPayload *payload = UnionGetElem<0>(&cmd.channel_payload);
                payload->type = TypeListLength<HeatersList>::value + FanIndex;
                UnionGetElem<FanIndex>(&payload->fans)->target = OutputFixedType::importDoubleSaturated(target);
                cc->submitPlannedCommand(c, &cmd);
                return false;
            }
            return true;
        }
        
        static OutputFixedType softpwm_timer_handler (typename TheSoftPwm::TimerInstance::HandlerContext c)
        {
            Fan *o = self(c);
            OutputFixedType control_value;
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                control_value = o->m_target;
            }
            return control_value;
        }
        
        static void emergency ()
        {
            Context::Pins::template emergencySet<typename FanSpec::OutputPin>(false);
        }
        
        template <typename ThisContext, typename TheChannelPayloadUnion>
        static void channel_callback (ThisContext c, TheChannelPayloadUnion *payload_union)
        {
            Fan *o = self(c);
            ChannelPayload *payload = UnionGetElem<FanIndex>(payload_union);
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                o->m_target = payload->target;
            }
        }
        
        OutputFixedType m_target;
        TheSoftPwm m_softpwm;
        
        struct SoftPwmTimerHandler : public AMBRO_WFUNC_TD(&Fan::softpwm_timer_handler) {};
        struct SoftPwmPosition : public MemberPosition<FanPosition<FanIndex>, TheSoftPwm, &Fan::m_softpwm> {};
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
    static void init (Context c)
    {
        PrinterMain *o = self(c);
        
        o->m_watchdog.init(c);
        o->m_blinker.init(c, Params::LedBlinkInterval::value() * Clock::time_freq);
        o->m_steppers.init(c);
        TupleForEachForward(&o->m_axes, Foreach_init(), c);
        o->m_serial_feature.init(c);
        o->m_unlocked_timer.init(c, PrinterMain::unlocked_timer_handler);
        o->m_disable_timer.init(c, PrinterMain::disable_timer_handler);
        o->m_force_timer.init(c, PrinterMain::force_timer_handler);
        TupleForEachForward(&o->m_heaters, Foreach_init(), c);
        TupleForEachForward(&o->m_fans, Foreach_init(), c);
        o->m_inactive_time = Params::DefaultInactiveTime::value() * Clock::time_freq;
        o->m_max_cart_speed = INFINITY;
        o->m_locked = false;
        o->m_planning = false;
        
        o->m_serial_feature.m_channel_common.reply_append_str(c, "APrinter\n");
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        PrinterMain *o = self(c);
        o->debugDeinit(c);
        
        if (o->m_planning) {
            o->m_planner.deinit(c);
        }
        TupleForEachReverse(&o->m_fans, Foreach_deinit(), c);
        TupleForEachReverse(&o->m_heaters, Foreach_deinit(), c);
        o->m_force_timer.deinit(c);
        o->m_disable_timer.deinit(c);
        o->m_unlocked_timer.deinit(c);
        o->m_serial_feature.deinit(c);
        TupleForEachReverse(&o->m_axes, Foreach_deinit(), c);
        o->m_steppers.deinit(c);
        o->m_blinker.deinit(c);
        o->m_watchdog.deinit(c);
    }
    
    typename SerialFeature::TheSerial * getSerial ()
    {
        return &m_serial_feature.m_serial;
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
    
    using EventLoopFastEvents = JoinTypeLists<
        typename SerialFeature::TheSerial::EventLoopFastEvents,
        JoinTypeLists<
            typename ThePlanner::EventLoopFastEvents,
            TypeListFold<
                MapTypeList<typename AxesTuple::ElemTypes, GetMemberType_EventLoopFastEvents>,
                EmptyTypeList,
                JoinTypeLists
            >
        >
    >;
    
private:
    static PrinterMain * self (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
    }
    
    static TimeType time_from_real (double t)
    {
        return (FixedPoint<30, false, 0>::importDoubleSaturated(t * Clock::time_freq)).bitsValue();
    }
    
    static void blinker_handler (Context c)
    {
        PrinterMain *o = self(c);
        o->debugAccess(c);
        
        o->m_watchdog.reset(c);
    }
    
    template <typename TheChannelCommon>
    static void work_command (Context c)
    {
        PrinterMain *o = self(c);
        TheChannelCommon *cc = TheChannelCommon::self(c);
        AMBRO_ASSERT(cc->m_cmd)
        
        switch (cc->m_cmd_code) {
            case 'M': switch (cc->m_cmd_num) {
                default:
                    if (
                        TupleForEachForwardInterruptible(&o->m_heaters, Foreach_check_command(), c, cc) &&
                        TupleForEachForwardInterruptible(&o->m_fans, Foreach_check_command(), c, cc)
                    ) {
                        goto unknown_command;
                    }
                    return;
                
                case 110: // set line number
                    return cc->finishCommand(c);
                
                case 17: {
                    if (!cc->tryUnplannedCommand(c)) {
                        return;
                    }
                    TupleForEachForward(&o->m_axes, Foreach_enable_stepper(), c, true);
                    now_inactive(c);
                    return cc->finishCommand(c);
                } break;
                
                case 18: // disable steppers or set timeout
                case 84: {
                    if (!cc->tryUnplannedCommand(c)) {
                        return;
                    }
                    double inactive_time;
                    if (cc->find_command_param_double(c, 'S', &inactive_time)) {
                        o->m_inactive_time = time_from_real(inactive_time);
                        if (o->m_disable_timer.isSet(c)) {
                            o->m_disable_timer.appendAt(c, o->m_last_active_time + o->m_inactive_time);
                        }
                    } else {
                        TupleForEachForward(&o->m_axes, Foreach_enable_stepper(), c, false);
                        o->m_disable_timer.unset(c);
                    }
                    return cc->finishCommand(c);
                } break;
                
                case 105: {
                    cc->reply_append_str(c, "ok");
                    TupleForEachForward(&o->m_heaters, Foreach_append_value(), c, cc);
                    cc->reply_append_ch(c, '\n');
                    return cc->finishCommand(c, true);
                } break;
                
                case 114: {
                    TupleForEachForward(&o->m_axes, Foreach_append_position(), c, cc);
                    cc->reply_append_ch(c, '\n');
                    return cc->finishCommand(c);
                } break;
                
                case 136: { // print heater config
                    TupleForEachForward(&o->m_heaters, Foreach_print_config(), c, cc);
                    return cc->finishCommand(c);
                } break;
            } break;
            
            case 'G': switch (cc->m_cmd_num) {
                default:
                    goto unknown_command;
                
                case 0:
                case 1: { // buffered move
                    if (!cc->tryPlannedCommand(c)) {
                        return;
                    }
                    double new_pos[num_axes];
                    TupleForEachForward(&o->m_axes, Foreach_init_new_pos(), c, new_pos);
                    for (typename TheChannelCommon::GcodePartsSizeType i = 1; i < cc->m_cmd->num_parts; i++) {
                        typename TheChannelCommon::GcodeParserCommandPart *part = &cc->m_cmd->parts[i];
                        TupleForEachForward(&o->m_axes, Foreach_collect_new_pos(), c, cc, new_pos, part);
                        if (part->code == 'F') {
                            o->m_max_cart_speed = strtod(part->data, NULL) * Params::SpeedLimitMultiply::value();
                        }
                    }
                    cc->finishCommand(c);
                    PlannerInputCommand cmd;
                    double distance = 0.0;
                    double total_steps = 0.0;
                    TupleForEachForward(&o->m_axes, Foreach_process_new_pos(), c, new_pos, &distance, &total_steps, &cmd);
                    distance = sqrt(distance);
                    cmd.type = 0;
                    cmd.rel_max_v_rec = FloatMakePosOrPosZero(distance / (o->m_max_cart_speed * Clock::time_unit));
                    cmd.rel_max_v_rec = fmax(cmd.rel_max_v_rec, total_steps * (1.0 / (Params::MaxStepsPerCycle::value() * F_CPU * Clock::time_unit)));
                    return cc->submitPlannedCommand(c, &cmd);
                } break;
                
                case 21: // set units to millimeters
                    return cc->finishCommand(c);
                
                case 28: { // home axes
                    if (!cc->tryUnplannedCommand(c)) {
                        return;
                    }
                    AxisMaskType mask = 0;
                    for (typename TheChannelCommon::GcodePartsSizeType i = 1; i < cc->m_cmd->num_parts; i++) {
                        TupleForEachForward(&o->m_axes, Foreach_update_homing_mask(), cc, &mask, &cc->m_cmd->parts[i]);
                    }
                    if (mask == 0) {
                        mask = -1;
                    }
                    o->m_homing_rem_axes = 0;
                    TupleForEachForward(&o->m_axes, Foreach_start_homing(), c, mask);
                    if (o->m_homing_rem_axes == 0) {
                        return cc->finishCommand(c);
                    }
                    now_active(c);
                } break;
                
                case 90: { // absolute positioning
                    TupleForEachForward(&o->m_axes, Foreach_set_relative_positioning(), c, false);
                    return cc->finishCommand(c);
                } break;
                
                case 91: { // relative positioning
                    TupleForEachForward(&o->m_axes, Foreach_set_relative_positioning(), c, true);
                    return cc->finishCommand(c);
                } break;
                
                case 92: { // set position
                    bool found_axes = false;
                    for (typename TheChannelCommon::GcodePartsSizeType i = 1; i < cc->m_cmd->num_parts; i++) {
                        TupleForEachForward(&o->m_axes, Foreach_set_position(), c, cc, &cc->m_cmd->parts[i], &found_axes);
                    }
                    if (!found_axes) {
                        cc->reply_append_str(c, "Error:not supported\n");
                    }
                    return cc->finishCommand(c);
                } break;
            } break;
            
            unknown_command:
            default: {
                cc->reply_append_str(c, "Error:Unknown command ");
                cc->reply_append_str(c, (cc->m_cmd->parts[0].data - 1));
                cc->reply_append_ch(c, '\n');
                return cc->finishCommand(c);
            } break;
        }
    }
    
    template <typename TheChannelCommon>
    static void finish_locked_helper (Context c, TheChannelCommon *cc)
    {
        cc->finishCommand(c);
    }
    
    static void finish_locked (Context c)
    {
        PrinterMain *o = self(c);
        AMBRO_ASSERT(o->m_locked)
        
        ChannelCommonTuple dummy;
        TupleForEachForwardInterruptible(&dummy, Foreach_run_for_state_command(), c, COMMAND_LOCKED, o, Foreach_finish_locked_helper());
    }
    
    static void homing_finished (Context c)
    {
        PrinterMain *o = self(c);
        AMBRO_ASSERT(o->m_locked)
        AMBRO_ASSERT(o->m_homing_rem_axes == 0)
        
        now_inactive(c);
        finish_locked(c);
    }
    
    static void now_inactive (Context c)
    {
        PrinterMain *o = self(c);
        
        o->m_last_active_time = c.clock()->getTime(c);
        o->m_disable_timer.appendAt(c, o->m_last_active_time + o->m_inactive_time);
    }
    
    static void now_active (Context c)
    {
        PrinterMain *o = self(c);
        o->m_disable_timer.unset(c);
    }
    
    template <typename TheChannelCommon>
    static void continue_locking_helper (Context c, TheChannelCommon *cc)
    {
        PrinterMain *o = self(c);
        AMBRO_ASSERT(!o->m_locked)
        AMBRO_ASSERT(cc->m_cmd)
        AMBRO_ASSERT(cc->m_state == COMMAND_LOCKING)
        
        cc->m_state = COMMAND_IDLE;
        work_command<TheChannelCommon>(c);
    }
    
    static void unlocked_timer_handler (typename Loop::QueuedEvent *, Context c)
    {
        PrinterMain *o = self(c);
        o->debugAccess(c);
        
        if (!o->m_locked) {
            ChannelCommonTuple dummy;
            TupleForEachForwardInterruptible(&dummy, Foreach_run_for_state_command(), c, COMMAND_LOCKING, o, Foreach_continue_locking_helper());
        }
    }
    
    static void disable_timer_handler (typename Loop::QueuedEvent *, Context c)
    {
        PrinterMain *o = self(c);
        o->debugAccess(c);
        
        TupleForEachForward(&o->m_axes, Foreach_enable_stepper(), c, false);
    }
    
    static void force_timer_handler (typename Loop::QueuedEvent *, Context c)
    {
        PrinterMain *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_planning)
        AMBRO_ASSERT(o->m_planning_pull_pending)
        AMBRO_ASSERT(!o->m_planning_req_pending)
        
        o->m_planner.waitFinished(c);
    }
    
    template <int AxisIndex>
    static typename Axis<AxisIndex>::TheAxisStepper * planner_get_axis_stepper (Context c)
    {
        return &Axis<AxisIndex>::self(c)->m_axis_stepper;
    }
    
    template <typename TheChannelCommon>
    static void continue_planned_helper (Context c, TheChannelCommon *cc)
    {
        PrinterMain *m = PrinterMain::self(c);
        AMBRO_ASSERT(m->m_locked)
        AMBRO_ASSERT(m->m_planning)
        AMBRO_ASSERT(m->m_planning_req_pending)
        AMBRO_ASSERT(m->m_planning_pull_pending)
        AMBRO_ASSERT(cc->m_state == COMMAND_LOCKED)
        AMBRO_ASSERT(cc->m_cmd)
        
        work_command<TheChannelCommon>(c);
    }
    
    static void planner_pull_handler (Context c)
    {
        PrinterMain *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_planning)
        AMBRO_ASSERT(!o->m_planning_pull_pending)
        
        o->m_planning_pull_pending = true;
        if (o->m_planning_req_pending) {
            ChannelCommonTuple dummy;
            TupleForEachForwardInterruptible(&dummy, Foreach_run_for_state_command(), c, COMMAND_LOCKED, o, Foreach_continue_planned_helper());
        } else if (o->m_locked) {
            o->m_planner.waitFinished(c);
        } else {
            TimeType force_time = c.clock()->getTime(c) + (TimeType)(Params::ForceTimeout::value() * Clock::time_freq);
            o->m_force_timer.appendAt(c, force_time);
        }
    }
    
    template <typename TheChannelCommon>
    static void continue_unplanned_helper (Context c, TheChannelCommon *cc)
    {
        PrinterMain *m = PrinterMain::self(c);
        AMBRO_ASSERT(m->m_locked)
        AMBRO_ASSERT(!m->m_planning)
        AMBRO_ASSERT(cc->m_state == COMMAND_LOCKED)
        AMBRO_ASSERT(cc->m_cmd)
        
        work_command<TheChannelCommon>(c);
    }
    
    static void planner_finished_handler (Context c)
    {
        PrinterMain *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_planning)
        AMBRO_ASSERT(o->m_planning_pull_pending)
        AMBRO_ASSERT(!o->m_planning_req_pending)
        
        o->m_planner.deinit(c);
        o->m_force_timer.unset(c);
        o->m_planning = false;
        now_inactive(c);
        
        if (o->m_locked) {
            ChannelCommonTuple dummy;
            TupleForEachForwardInterruptible(&dummy, Foreach_run_for_state_command(), c, COMMAND_LOCKED, o, Foreach_continue_unplanned_helper());
        }
    }
    
    static void planner_channel_callback (typename ThePlanner::template Channel<0>::CallbackContext c, PlannerChannelPayload *payload)
    {
        PrinterMain *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_planning)
        
        TupleForOneBoolOffset<0>(payload->type, &o->m_heaters, Foreach_channel_callback(), c, &payload->heaters) ||
        TupleForOneBoolOffset<TypeListLength<HeatersList>::value>(payload->type, &o->m_fans, Foreach_channel_callback(), c, &payload->fans);
    }
    
    TheWatchdog m_watchdog;
    TheBlinker m_blinker;
    TheSteppers m_steppers;
    AxesTuple m_axes;
    typename Loop::QueuedEvent m_unlocked_timer;
    typename Loop::QueuedEvent m_disable_timer;
    typename Loop::QueuedEvent m_force_timer;
    SerialFeature m_serial_feature;
    HeatersTuple m_heaters;
    FansTuple m_fans;
    TimeType m_inactive_time;
    TimeType m_last_active_time;
    double m_max_cart_speed;
    bool m_locked;
    bool m_planning;
    union {
        struct {
            HomingStateTuple m_homers;
            AxisCountType m_homing_rem_axes;
        };
        struct {
            ThePlanner m_planner;
            bool m_planning_req_pending;
            bool m_planning_pull_pending;
        };
    };
    
    struct WatchdogPosition : public MemberPosition<Position, TheWatchdog, &PrinterMain::m_watchdog> {};
    struct BlinkerPosition : public MemberPosition<Position, TheBlinker, &PrinterMain::m_blinker> {};
    struct SteppersPosition : public MemberPosition<Position, TheSteppers, &PrinterMain::m_steppers> {};
    template <int AxisIndex> struct AxisPosition : public TuplePosition<Position, AxesTuple, &PrinterMain::m_axes, AxisIndex> {};
    template <int AxisIndex> struct HomingFeaturePosition : public MemberPosition<AxisPosition<AxisIndex>, typename Axis<AxisIndex>::HomingFeature, &Axis<AxisIndex>::m_homing_feature> {};
    template <int AxisIndex> struct HomingStatePosition : public TuplePosition<Position, HomingStateTuple, &PrinterMain::m_homers, AxisIndex> {};
    struct SerialFeaturePosition : public MemberPosition<Position, SerialFeature, &PrinterMain::m_serial_feature> {};
    struct PlannerPosition : public MemberPosition<Position, ThePlanner, &PrinterMain::m_planner> {};
    template <int HeaterIndex> struct HeaterPosition : public TuplePosition<Position, HeatersTuple, &PrinterMain::m_heaters, HeaterIndex> {};
    template <int HeaterIndex> struct MainControlPosition : public MemberPosition<HeaterPosition<HeaterIndex>, typename Heater<HeaterIndex>::MainControl, &Heater<HeaterIndex>::m_main_control> {};
    template <int FanIndex> struct FanPosition : public TuplePosition<Position, FansTuple, &PrinterMain::m_fans, FanIndex> {};
    
    struct BlinkerHandler : public AMBRO_WFUNC_TD(&PrinterMain::blinker_handler) {};
    template <int AxisIndex> struct PlannerGetAxisStepper : public AMBRO_WFUNC_TD(&PrinterMain::template planner_get_axis_stepper<AxisIndex>) {};
    struct PlannerPullHandler : public AMBRO_WFUNC_TD(&PrinterMain::planner_pull_handler) {};
    struct PlannerFinishedHandler : public AMBRO_WFUNC_TD(&PrinterMain::planner_finished_handler) {};
    struct PlannerChannelCallback : public AMBRO_WFUNC_TD(&PrinterMain::planner_channel_callback) {};
    template <int AxisIndex> struct AxisStepperConsumersList {
        using List = JoinTypeLists<
            MakeTypeList<typename ThePlanner::template TheAxisStepperConsumer<AxisIndex>>,
            typename Axis<AxisIndex>::HomingFeature::template MakeAxisStepperConsumersList<typename Axis<AxisIndex>::HomingFeature>
        >;
    };
};

#include <aprinter/EndNamespace.h>

#endif
