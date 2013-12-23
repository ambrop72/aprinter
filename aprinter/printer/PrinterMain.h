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
#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/TypeListIndex.h>
#include <aprinter/meta/ComposeFunctions.h>
#include <aprinter/meta/IsEqualFunc.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Likely.h>
#include <aprinter/base/ProgramMemory.h>
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
    typename TSdCardParams, typename TProbeParams,
    typename TAxesList, typename TTransformParams, typename THeatersList, typename TFansList
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
    using SdCardParams = TSdCardParams;
    using ProbeParams = TProbeParams;
    using AxesList = TAxesList;
    using TransformParams = TTransformParams;
    using HeatersList = THeatersList;
    using FansList = TFansList;
};

template <
    uint32_t tbaud,
    int TRecvBufferSizeExp, int TSendBufferSizeExp,
    typename TTheGcodeParserParams,
    template <typename, typename, int, int, typename, typename, typename> class TSerialTemplate,
    typename TSerialParams
>
struct PrinterMainSerialParams {
    static const uint32_t baud = tbaud;
    static const int RecvBufferSizeExp = TRecvBufferSizeExp;
    static const int SendBufferSizeExp = TSendBufferSizeExp;
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

struct PrinterMainNoTransformParams {
    static const bool Enabled = false;
};

template <
    typename TVirtAxesList, typename TPhysAxesList,
    template<typename> class TTransformAlg, typename TTransformAlgParams
>
struct PrinterMainTransformParams {
    static bool const Enabled = true;
    using VirtAxesList = TVirtAxesList;
    using PhysAxesList = TPhysAxesList;
    template <typename X> using TransformAlg = TTransformAlg<X>;
    using TransformAlgParams = TTransformAlgParams;
};

template <
    char TName, int TSetMCommand, int TWaitMCommand, int TSetConfigMCommand,
    typename TAdcPin, typename TOutputPin, bool TOutputInvert,
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
    static const bool OutputInvert = TOutputInvert;
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
    typename TOutputPin, bool TOutputInvert, typename TPulseInterval, typename TSpeedMultiply,
    template<typename, typename, typename> class TTimerTemplate
>
struct PrinterMainFanParams {
    static const int SetMCommand = TSetMCommand;
    static const int OffMCommand = TOffMCommand;
    using OutputPin = TOutputPin;
    static const bool OutputInvert = TOutputInvert;
    using PulseInterval = TPulseInterval;
    using SpeedMultiply = TSpeedMultiply;
    template <typename X, typename Y, typename Z> using TimerTemplate = TTimerTemplate<X, Y, Z>;
};

struct PrinterMainNoSdCardParams {
    static const bool enabled = false;
};

template <
    template<typename, typename, typename, int, typename, typename> class TSdCard,
    typename TSdCardParams, typename TTheGcodeParserParams, int TReadBufferBlocks,
    int TMaxCommandSize
>
struct PrinterMainSdCardParams {
    static const bool enabled = true;
    template <typename X, typename Y, typename Z, int R, typename W, typename Q> using SdCard = TSdCard<X, Y, Z, R, W, Q>;
    using SdCardParams = TSdCardParams;
    using TheGcodeParserParams = TTheGcodeParserParams;
    static const int ReadBufferBlocks = TReadBufferBlocks;
    static const int MaxCommandSize = TMaxCommandSize;
};

struct PrinterMainNoProbeParams {
    static const bool enabled = false;
};

template <
    typename TPlatformAxesList,
    int TProbeAxis,
    typename TProbePin,
    bool TProbeInvert,
    typename TProbePlatformOffset,
    typename TProbeStartHeight,
    typename TProbeLowHeight,
    typename TProbeRetractDist,
    typename TProbeMoveSpeed,
    typename TProbeFastSpeed,
    typename TProbeRetractSpeed,
    typename TProbeSlowSpeed,
    typename TProbePoints
>
struct PrinterMainProbeParams {
    static const bool enabled = true;
    using PlatformAxesList = TPlatformAxesList;
    static const int ProbeAxis = TProbeAxis;
    using ProbePin = TProbePin;
    static const bool ProbeInvert = TProbeInvert;
    using ProbePlatformOffset = TProbePlatformOffset;
    using ProbeStartHeight = TProbeStartHeight;
    using ProbeLowHeight = TProbeLowHeight;
    using ProbeRetractDist = TProbeRetractDist;
    using ProbeMoveSpeed = TProbeMoveSpeed;
    using ProbeFastSpeed = TProbeFastSpeed;
    using ProbeRetractSpeed = TProbeRetractSpeed;
    using ProbeSlowSpeed = TProbeSlowSpeed;
    using ProbePoints = TProbePoints;
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
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_fix_aborted_pos, fix_aborted_pos)
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
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_add_axis, add_axis)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_get_coord, get_coord)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_report_height, report_height)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_read_phys_pos, read_phys_pos)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_write_virt_pos, write_virt_pos)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_move_phys, move_phys)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_add_virt_distance, add_virt_distance)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_ChannelPayload, ChannelPayload)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_EventLoopFastEvents, EventLoopFastEvents)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_WrappedAxisName, WrappedAxisName)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_WrappedPhysAxisIndex, WrappedPhysAxisIndex)
    
    struct WatchdogPosition;
    struct BlinkerPosition;
    struct SteppersPosition;
    template <int AxisIndex> struct AxisPosition;
    template <int AxisIndex> struct HomingFeaturePosition;
    template <int AxisIndex> struct HomingStatePosition;
    struct TransformFeaturePosition;
    struct SerialFeaturePosition;
    struct SdCardFeaturePosition;
    struct PlannerPosition;
    template <int HeaterIndex> struct HeaterPosition;
    template <int HeaterIndex> struct MainControlPosition;
    template <int FanIndex> struct FanPosition;
    struct ProbeFeaturePosition;
    
    struct BlinkerHandler;
    template <int AxisIndex> struct PlannerGetAxisStepper;
    struct PlannerPullHandler;
    struct PlannerFinishedHandler;
    struct PlannerAbortedHandler;
    struct PlannerChannelCallback;
    template <int AxisIndex> struct PlannerPrestepCallback;
    template <int AxisIndex> struct AxisStepperConsumersList;
    
    using Loop = typename Context::EventLoop;
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using AxesList = typename Params::AxesList;
    using TransformParams = typename Params::TransformParams;
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
    enum {PLANNER_NONE, PLANNER_RUNNING, PLANNER_STOPPING, PLANNER_WAITING, PLANNER_PROBE};
    
    struct MoveBuildState;
    
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
                AMBRO_PGM_P err = AMBRO_PSTR("unknown error");
                switch (o->m_cmd->num_parts) {
                    case 0: err = AMBRO_PSTR("empty command"); break;
                    case TheGcodeParser::ERROR_TOO_MANY_PARTS: err = AMBRO_PSTR("too many parts"); break;
                    case TheGcodeParser::ERROR_INVALID_PART: err = AMBRO_PSTR("invalid part"); break;
                    case TheGcodeParser::ERROR_CHECKSUM: err = AMBRO_PSTR("incorrect checksum"); break;
                    case TheGcodeParser::ERROR_RECV_OVERRUN: err = AMBRO_PSTR("receive buffer overrun"); break;
                }
                reply_append_pstr(c, AMBRO_PSTR("Error:"));
                reply_append_pstr(c, err);
                reply_append_ch(c, '\n');
                return finishCommand(c);
            }
            o->m_cmd_code = o->m_cmd->parts[0].code;
            o->m_cmd_num = atoi(o->m_cmd->parts[0].data);
            if (!Channel::start_command_impl(c)) {
                return finishCommand(c);
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
        
        static void maybeCancelLockingCommand (Context c)
        {
            ChannelCommon *o = self(c);
            AMBRO_ASSERT(o->m_state != COMMAND_LOCKED)
            
            o->m_state = COMMAND_IDLE;
            o->m_cmd = NULL;
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
        
        static bool tryLockedCommand (Context c)
        {
            ChannelCommon *o = self(c);
            PrinterMain *m = PrinterMain::self(c);
            AMBRO_ASSERT(o->m_state != COMMAND_LOCKING || !m->m_locked)
            AMBRO_ASSERT(o->m_state != COMMAND_LOCKED || m->m_locked)
            AMBRO_ASSERT(o->m_cmd)
            
            if (o->m_state == COMMAND_LOCKED) {
                return true;
            }
            if (m->m_locked) {
                o->m_state = COMMAND_LOCKING;
                return false;
            }
            o->m_state = COMMAND_LOCKED;
            m->m_locked = true;
            return true;
        }
        
        static bool tryUnplannedCommand (Context c)
        {
            PrinterMain *m = PrinterMain::self(c);
            
            if (!tryLockedCommand(c)) {
                return false;
            }
            AMBRO_ASSERT(m->m_planner_state == PLANNER_NONE || m->m_planner_state == PLANNER_RUNNING)
            if (m->m_planner_state == PLANNER_NONE) {
                return true;
            }
            m->m_planner_state = PLANNER_STOPPING;
            if (m->m_planning_pull_pending) {
                m->m_planner.waitFinished(c);
                m->m_force_timer.unset(c);
            }
            return false;
        }
        
        static bool tryPlannedCommand (Context c)
        {
            PrinterMain *m = PrinterMain::self(c);
            
            if (!tryLockedCommand(c)) {
                return false;
            }
            AMBRO_ASSERT(m->m_planner_state == PLANNER_NONE || m->m_planner_state == PLANNER_RUNNING)
            if (m->m_planner_state == PLANNER_NONE) {
                m->m_planner.init(c, false);
                m->m_planner_state = PLANNER_RUNNING;
                m->m_planning_pull_pending = false;
                now_active(c);
            }
            if (m->m_planning_pull_pending) {
                return true;
            }
            m->m_planner_state = PLANNER_WAITING;
            return false;
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
        
        static void reply_append_pstr (Context c, AMBRO_PGM_P pstr)
        {
            Channel::reply_append_pbuffer_impl(c, pstr, AMBRO_PGM_STRLEN(pstr));
        }
        
        static void reply_append_ch (Context c, char ch)
        {
            Channel::reply_append_ch_impl(c, ch);
        }
        
        static void reply_append_double (Context c, double x)
        {
            char buf[30];
#if defined(AMBROLIB_AVR)
            uint8_t len = AMBRO_PGM_SPRINTF(buf, AMBRO_PSTR("%g"), x);
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
            uint8_t len = AMBRO_PGM_SPRINTF(buf, AMBRO_PSTR("%" PRIu32), x);
#else
            uint8_t len = PrintNonnegativeIntDecimal<uint32_t>(x, buf);
#endif
            Channel::reply_append_buffer_impl(c, buf, len);
        }
        
        static void reply_append_uint8 (Context c, uint8_t x)
        {
            char buf[4];
#if defined(AMBROLIB_AVR)
            uint8_t len = AMBRO_PGM_SPRINTF(buf, AMBRO_PSTR("%" PRIu8), x);
#else
            uint8_t len = PrintNonnegativeIntDecimal<uint8_t>(x, buf);
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
        
        using TheSerial = typename Params::Serial::template SerialTemplate<SerialPosition, Context, Params::Serial::RecvBufferSizeExp, Params::Serial::SendBufferSizeExp, typename Params::Serial::SerialParams, SerialRecvHandler, SerialSendHandler>;
        using RecvSizeType = typename TheSerial::RecvSizeType;
        using SendSizeType = typename TheSerial::SendSizeType;
        using TheGcodeParser = GcodeParser<GcodeParserPosition, Context, typename Params::Serial::TheGcodeParserParams, typename RecvSizeType::IntType, GcodeParserTypeSerial>;
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
            o->m_line_number = 1;
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
        
        static bool start_command_impl (Context c)
        {
            SerialFeature *o = self(c);
            AMBRO_ASSERT(o->m_channel_common.m_cmd)
            
            bool is_m110 = (o->m_channel_common.m_cmd_code == 'M' && o->m_channel_common.m_cmd_num == 110);
            if (is_m110) {
                o->m_line_number = o->m_channel_common.get_command_param_uint32(c, 'L', (o->m_channel_common.m_cmd->have_line_number ? o->m_channel_common.m_cmd->line_number : -1));
            }
            if (o->m_channel_common.m_cmd->have_line_number) {
                if (o->m_channel_common.m_cmd->line_number != o->m_line_number) {
                    o->m_channel_common.reply_append_pstr(c, AMBRO_PSTR("Error:Line Number is not Last Line Number+1, Last Line:"));
                    o->m_channel_common.reply_append_uint32(c, (uint32_t)(o->m_line_number - 1));
                    o->m_channel_common.reply_append_ch(c, '\n');
                    return false;
                }
            }
            if (o->m_channel_common.m_cmd->have_line_number || is_m110) {
                o->m_line_number++;
            }
            return true;
        }
        
        static void finish_command_impl (Context c, bool no_ok)
        {
            SerialFeature *o = self(c);
            AMBRO_ASSERT(o->m_channel_common.m_cmd)
            
            if (!no_ok) {
                o->m_channel_common.reply_append_pstr(c, AMBRO_PSTR("ok\n"));
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
        
        static void reply_append_pbuffer_impl (Context c, AMBRO_PGM_P pstr, uint8_t length)
        {
            SerialFeature *o = self(c);
            SendSizeType avail = o->m_serial.sendQuery(c);
            if (length > avail.value()) {
                length = avail.value();
            }
            while (length > 0) {
                char *chunk_data = o->m_serial.sendGetChunkPtr(c);
                uint8_t chunk_length = o->m_serial.sendGetChunkLen(c, SendSizeType::import(length)).value();
                AMBRO_PGM_MEMCPY(chunk_data, pstr, chunk_length);
                o->m_serial.sendProvide(c, SendSizeType::import(chunk_length));
                pstr += chunk_length;
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
        uint32_t m_line_number;
        
        struct SerialPosition : public MemberPosition<SerialFeaturePosition, TheSerial, &SerialFeature::m_serial> {};
        struct GcodeParserPosition : public MemberPosition<SerialFeaturePosition, TheGcodeParser, &SerialFeature::m_gcode_parser> {};
        struct ChannelCommonPosition : public MemberPosition<SerialFeaturePosition, TheChannelCommon, &SerialFeature::m_channel_common> {};
        struct SerialRecvHandler : public AMBRO_WFUNC_TD(&SerialFeature::serial_recv_handler) {};
        struct SerialSendHandler : public AMBRO_WFUNC_TD(&SerialFeature::serial_send_handler) {};
    };
    
    AMBRO_STRUCT_IF(SdCardFeature, Params::SdCardParams::enabled) {
        struct SdCardPosition;
        struct GcodeParserPosition;
        struct ChannelCommonPosition;
        struct SdCardInitHandler;
        struct SdCardCommandHandler;
        
        static const int ReadBufferBlocks = Params::SdCardParams::ReadBufferBlocks;
        static const int MaxCommandSize = Params::SdCardParams::MaxCommandSize;
        static const size_t BlockSize = 512;
        static_assert(ReadBufferBlocks >= 2, "");
        static_assert(MaxCommandSize >= 64, "");
        static_assert(MaxCommandSize < BlockSize, "");
        static const size_t BufferBaseSize = ReadBufferBlocks * BlockSize;
        using ParserSizeType = typename ChooseInt<BitsInInt<MaxCommandSize>::value, false>::Type;
        using TheSdCard = typename Params::SdCardParams::template SdCard<SdCardPosition, Context, typename Params::SdCardParams::SdCardParams, 1, SdCardInitHandler, SdCardCommandHandler>;
        using TheGcodeParser = GcodeParser<GcodeParserPosition, Context, typename Params::SdCardParams::TheGcodeParserParams, ParserSizeType, GcodeParserTypeFile>;
        using SdCardReadState = typename TheSdCard::ReadState;
        using SdCardChannelCommon = ChannelCommon<ChannelCommonPosition, SdCardFeature>;
        enum {SDCARD_NONE, SDCARD_INITING, SDCARD_INITED, SDCARD_RUNNING, SDCARD_PAUSING};
        
        static SdCardFeature * self (Context c)
        {
            return PositionTraverse<typename Context::TheRootPosition, SdCardFeaturePosition>(c.root());
        }
        
        static void init (Context c)
        {
            SdCardFeature *o = self(c);
            o->m_sdcard.init(c);
            o->m_channel_common.init(c);
            o->m_next_event.init(c, SdCardFeature::next_event_handler);
            o->m_state = SDCARD_NONE;
        }
        
        static void deinit (Context c)
        {
            SdCardFeature *o = self(c);
            if (o->m_state != SDCARD_NONE && o->m_state != SDCARD_INITING) {
                o->m_gcode_parser.init(c);
            }
            o->m_next_event.deinit(c);
            o->m_gcode_parser.deinit(c);
            o->m_sdcard.deinit(c);
        }
        
        template <typename TheChannelCommon>
        static void finish_init (Context c, TheChannelCommon *cc, uint8_t error_code)
        {
            SdCardFeature *o = self(c);
            
            if (error_code) {
                cc->reply_append_pstr(c, AMBRO_PSTR("SD error "));
                cc->reply_append_uint8(c, error_code);
            } else {
                cc->reply_append_pstr(c, AMBRO_PSTR("SD blocks "));
                cc->reply_append_uint32(c, o->m_sdcard.getCapacityBlocks(c));
            }
            cc->reply_append_ch(c, '\n');
            cc->finishCommand(c);
        }
        
        static void sd_card_init_handler (Context c, uint8_t error_code)
        {
            SdCardFeature *o = self(c);
            AMBRO_ASSERT(o->m_state == SDCARD_INITING)
            
            if (error_code) {
                o->m_state = SDCARD_NONE;
            } else {
                o->m_state = SDCARD_INITED;
                o->m_gcode_parser.init(c);
                o->m_start = 0;
                o->m_length = 0;
                o->m_cmd_offset = 0;
                o->m_sd_block = 0;
            }
            Tuple<ChannelCommonList> dummy;
            TupleForEachForwardInterruptible(&dummy, Foreach_run_for_state_command(), c, COMMAND_LOCKED, o, Foreach_finish_init(), error_code);
        }
        
        static void sd_card_command_handler (Context c)
        {
            SdCardFeature *o = self(c);
            AMBRO_ASSERT(o->m_state == SDCARD_RUNNING || o->m_state == SDCARD_PAUSING)
            AMBRO_ASSERT(o->m_length < BufferBaseSize)
            AMBRO_ASSERT(o->m_sd_block < o->m_sdcard.getCapacityBlocks(c))
            
            bool error;
            if (!o->m_sdcard.checkReadBlock(c, &o->m_read_state, &error)) {
                return;
            }
            o->m_sdcard.unsetEvent(c);
            if (o->m_state == SDCARD_PAUSING) {
                o->m_state = SDCARD_INITED;
                return finish_locked(c);
            }
            if (error) {
                SerialFeature::TheChannelCommon::self(c)->reply_append_pstr(c, AMBRO_PSTR("//SdRdEr\n"));
                return start_read(c);
            }
            o->m_sd_block++;
            if (o->m_length == BufferBaseSize - o->m_start) {
                memcpy(o->m_buffer + BufferBaseSize, o->m_buffer, MaxCommandSize - 1);
            }
            o->m_length += BlockSize;
            if (o->m_length < BufferBaseSize && o->m_sd_block < o->m_sdcard.getCapacityBlocks(c)) {
                start_read(c);
            }
            if (!o->m_channel_common.m_cmd && !o->m_eof) {
                o->m_next_event.prependNowNotAlready(c);
            }
        }
        
        static void next_event_handler (typename Loop::QueuedEvent *, Context c)
        {
            SdCardFeature *o = self(c);
            AMBRO_ASSERT(o->m_state == SDCARD_RUNNING)
            AMBRO_ASSERT(!o->m_channel_common.m_cmd)
            AMBRO_ASSERT(!o->m_eof)
            
            AMBRO_PGM_P eof_str;
            typename TheGcodeParser::Command *cmd;
            if (!o->m_gcode_parser.haveCommand(c)) {
                o->m_gcode_parser.startCommand(c, (char *)buf_get(c, o->m_start, o->m_cmd_offset), 0);
            }
            ParserSizeType avail = (o->m_length - o->m_cmd_offset > MaxCommandSize) ? MaxCommandSize : (o->m_length - o->m_cmd_offset);
            if (avail >= 1 && *o->m_gcode_parser.getBuffer(c) == 'E') {
                eof_str = AMBRO_PSTR("//SdEof\n");
                goto eof;
            }
            cmd = o->m_gcode_parser.extendCommand(c, avail);
            if (cmd) {
                return o->m_channel_common.startCommand(c, cmd);
            }
            if (avail == MaxCommandSize) {
                eof_str = AMBRO_PSTR("//SdLnEr\n");
                goto eof;
            }
            if (o->m_sd_block == o->m_sdcard.getCapacityBlocks(c)) {
                eof_str = AMBRO_PSTR("//SdEnd\n");
                goto eof;
            }
            return;
        eof:
            SerialFeature::TheChannelCommon::self(c)->reply_append_pstr(c, eof_str);
            o->m_eof = true;
        }
        
        template <typename TheChannelCommon>
        static bool check_command (Context c, TheChannelCommon *cc)
        {
            SdCardFeature *o = self(c);
            PrinterMain *m = PrinterMain::self(c);
            
            if (TypesAreEqual<TheChannelCommon, SdCardChannelCommon>::value) {
                return true;
            }
            if (cc->m_cmd_num == 21) {
                if (!cc->tryUnplannedCommand(c)) {
                    return false;
                }
                if (o->m_state != SDCARD_NONE) {
                    cc->finishCommand(c);
                    return false;
                }
                o->m_sdcard.activate(c);
                o->m_state = SDCARD_INITING;
                return false;
            }
            if (cc->m_cmd_num == 22) {
                if (!cc->tryUnplannedCommand(c)) {
                    return false;
                }
                cc->finishCommand(c);
                AMBRO_ASSERT(o->m_state != SDCARD_INITING)
                AMBRO_ASSERT(o->m_state != SDCARD_PAUSING)
                if (o->m_state == SDCARD_NONE) {
                    return false;
                }
                o->m_gcode_parser.deinit(c);
                o->m_state = SDCARD_NONE;
                o->m_next_event.unset(c);
                o->m_channel_common.maybeCancelLockingCommand(c);
                o->m_sdcard.deactivate(c);
                return false;
            }
            if (cc->m_cmd_num == 24) {
                if (!cc->tryUnplannedCommand(c)) {
                    return false;
                }
                cc->finishCommand(c);
                if (o->m_state != SDCARD_INITED) {
                    return false;
                }
                o->m_state = SDCARD_RUNNING;
                o->m_eof = false;
                if (o->m_length < BufferBaseSize && o->m_sd_block < o->m_sdcard.getCapacityBlocks(c)) {
                    start_read(c);
                }
                if (!o->m_channel_common.maybeResumeLockingCommand(c)) {
                    o->m_next_event.prependNowNotAlready(c);
                }
                return false;
            }
            if (cc->m_cmd_num == 25) {
                if (!cc->tryUnplannedCommand(c)) {
                    return false;
                }
                if (o->m_state != SDCARD_RUNNING) {
                    cc->finishCommand(c);
                    return false;
                }
                o->m_next_event.unset(c);
                o->m_channel_common.maybePauseLockingCommand(c);
                if (o->m_length < BufferBaseSize && o->m_sd_block < o->m_sdcard.getCapacityBlocks(c)) {
                    o->m_state = SDCARD_PAUSING;
                } else {
                    o->m_state = SDCARD_INITED;
                    cc->finishCommand(c);
                }
                return false;
            }
            return true;
        }
        
        static bool start_command_impl (Context c)
        {
            return true;
        }
        
        static void finish_command_impl (Context c, bool no_ok)
        {
            SdCardFeature *o = self(c);
            AMBRO_ASSERT(o->m_channel_common.m_cmd)
            AMBRO_ASSERT(o->m_state == SDCARD_RUNNING)
            AMBRO_ASSERT(!o->m_eof)
            AMBRO_ASSERT(o->m_channel_common.m_cmd->length <= o->m_length - o->m_cmd_offset)
            
            o->m_next_event.prependNowNotAlready(c);
            o->m_cmd_offset += o->m_channel_common.m_cmd->length;
            if (o->m_cmd_offset >= BlockSize) {
                o->m_start += BlockSize;
                if (o->m_start == BufferBaseSize) {
                    o->m_start = 0;
                }
                o->m_length -= BlockSize;
                o->m_cmd_offset -= BlockSize;
                if (o->m_length == BufferBaseSize - BlockSize && o->m_sd_block < o->m_sdcard.getCapacityBlocks(c)) {
                    start_read(c);
                }
            }
        }
        
        static void reply_append_buffer_impl (Context c, char const *str, uint8_t length)
        {
        }
        
        static void reply_append_pbuffer_impl (Context c, AMBRO_PGM_P pstr, uint8_t length)
        {
        }
        
        static void reply_append_ch_impl (Context c, char ch)
        {
        }
        
        static uint8_t * buf_get (Context c, size_t start, size_t count)
        {
            SdCardFeature *o = self(c);
            
            static_assert(BufferBaseSize <= SIZE_MAX / 2, "");
            size_t x = start + count;
            if (x >= BufferBaseSize) {
                x -= BufferBaseSize;
            }
            return o->m_buffer + x;
        }
        
        static void start_read (Context c)
        {
            SdCardFeature *o = self(c);
            AMBRO_ASSERT(o->m_length < BufferBaseSize)
            AMBRO_ASSERT(o->m_sd_block < o->m_sdcard.getCapacityBlocks(c))
            
            o->m_sdcard.queueReadBlock(c, o->m_sd_block, buf_get(c, o->m_start, o->m_length), &o->m_read_state);
        }
        
        TheSdCard m_sdcard;
        SdCardChannelCommon m_channel_common;
        typename Loop::QueuedEvent m_next_event;
        uint8_t m_state;
        TheGcodeParser m_gcode_parser;
        SdCardReadState m_read_state;
        size_t m_start;
        size_t m_length;
        size_t m_cmd_offset;
        bool m_eof;
        uint32_t m_sd_block;
        uint8_t m_buffer[BufferBaseSize + (MaxCommandSize - 1)];
        
        struct SdCardPosition : public MemberPosition<SdCardFeaturePosition, TheSdCard, &SdCardFeature::m_sdcard> {};
        struct GcodeParserPosition : public MemberPosition<SdCardFeaturePosition, TheGcodeParser, &SdCardFeature::m_gcode_parser> {};
        struct ChannelCommonPosition : public MemberPosition<SdCardFeaturePosition, SdCardChannelCommon, &SdCardFeature::m_channel_common> {};
        struct SdCardInitHandler : public AMBRO_WFUNC_TD(&SdCardFeature::sd_card_init_handler) {};
        struct SdCardCommandHandler : public AMBRO_WFUNC_TD(&SdCardFeature::sd_card_command_handler) {};
        
        using EventLoopFastEvents = typename TheSdCard::EventLoopFastEvents;
        using SdChannelCommonList = MakeTypeList<SdCardChannelCommon>;
    } AMBRO_STRUCT_ELSE(SdCardFeature) {
        static void init (Context c) {}
        static void deinit (Context c) {}
        template <typename TheChannelCommon>
        static bool check_command (Context c, TheChannelCommon *cc) { return true; }
        using EventLoopFastEvents = EmptyTypeList;
        using SdChannelCommonList = EmptyTypeList;
    };
    
    using ChannelCommonList = JoinTypeLists<
        MakeTypeList<typename SerialFeature::TheChannelCommon>,
        typename SdCardFeature::SdChannelCommonList
    >;
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
        using WrappedAxisName = WrapInt<axis_name>;
        
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
                    m->m_transform_feature.template mark_phys_moved<AxisIndex>(c);
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
        
        static void update_new_pos (Context c, MoveBuildState *s, double req)
        {
            PrinterMain *m = PrinterMain::self(c);
            s->new_pos[AxisIndex] = clamp_req_pos(req);
            if (AxisSpec::enable_cartesian_speed_limit) {
                s->seen_cartesian = true;
            }
            m->m_transform_feature.template mark_phys_moved<AxisIndex>(c);
        }
        
        template <typename PlannerCmd>
        static void process_new_pos (Context c, MoveBuildState *s, double max_speed, double *distance_squared, double *total_steps, PlannerCmd *cmd)
        {
            Axis *o = self(c);
            AbsStepFixedType new_end_pos = AbsStepFixedType::importDoubleSaturatedRound(dist_from_real(s->new_pos[AxisIndex]));
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
            if (!s->seen_cartesian) {
                mycmd->max_v_rec = fmax(mycmd->max_v_rec, FloatMakePosOrPosZero(1.0 / speed_from_real(max_speed)));
            }
            mycmd->max_a_rec = 1.0 / accel_from_real(AxisSpec::DefaultMaxAccel::value());
            o->m_end_pos = new_end_pos;
            o->m_req_pos = s->new_pos[AxisIndex];
        }
        
        static void fix_aborted_pos (Context c)
        {
            Axis *o = self(c);
            PrinterMain *m = PrinterMain::self(c);
            using RemStepsType = typename ChooseInt<AxisSpec::StepBits, true>::Type;
            RemStepsType rem_steps = m->m_planner.template countAbortedRemSteps<AxisIndex, RemStepsType>(c);
            if (rem_steps != 0) {
                o->m_end_pos.m_bits.m_int -= rem_steps;
                o->m_req_pos = dist_to_real(o->m_end_pos.doubleValue());
                m->m_transform_feature.template mark_phys_moved<AxisIndex>(c);
            }
        }
        
        template <typename TheChannelCommon>
        static void set_position (Context c, TheChannelCommon *cc, typename TheChannelCommon::GcodeParserCommandPart *part, bool *found_axes)
        {
            Axis *o = self(c);
            if (part->code == axis_name) {
                *found_axes = true;
                if (AxisSpec::Homing::enabled) {
                    cc->reply_append_pstr(c, AMBRO_PSTR("Error:G92 on homable axis\n"));
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
    
    using AxesTuple = IndexElemTuple<AxesList, Axis>;
    
    template <int AxisName>
    using FindAxis = TypeListIndex<
        typename AxesTuple::ElemTypes,
        ComposeFunctions<
            IsEqualFunc<WrapInt<AxisName>>,
            GetMemberType_WrappedAxisName
        >
    >;
    
    template <typename TheAxis>
    using MakePlannerAxisSpec = MotionPlannerAxisSpec<
        typename TheAxis::TheAxisStepper,
        PlannerGetAxisStepper<TheAxis::AxisIndex>,
        TheAxis::AxisSpec::StepBits,
        typename TheAxis::AxisSpec::DefaultDistanceFactor,
        typename TheAxis::AxisSpec::DefaultCorneringDistance,
        PlannerPrestepCallback<TheAxis::AxisIndex>
    >;
    
    AMBRO_STRUCT_IF(TransformFeature, TransformParams::Enabled) {
        template <int VirtAxisIndex> struct VirtAxisPosition;
        
        using VirtAxesList = typename TransformParams::VirtAxesList;
        using PhysAxesList = typename TransformParams::PhysAxesList;
        using TheTransformAlg = typename TransformParams::template TransformAlg<typename TransformParams::TransformAlgParams>;
        static int const NumVirtAxes = TheTransformAlg::NumAxes;
        static_assert(TypeListLength<VirtAxesList>::value == NumVirtAxes, "");
        static_assert(TypeListLength<PhysAxesList>::value == NumVirtAxes, "");
        
        struct MoveStateExtra {
            bool seen_virt;
            double virt_old_pos[NumVirtAxes];
        };
        
        static TransformFeature * self (Context c)
        {
            return PositionTraverse<typename Context::TheRootPosition, TransformFeaturePosition>(c.root());
        }
        
        static void init (Context c)
        {
            TransformFeature *o = self(c);
            TupleForEachForward(&o->m_virt_axes, Foreach_init(), c);
            update_virt_from_phys(c);
            o->m_virt_update_pending = false;
        }
        
        static void update_virt_from_phys (Context c)
        {
            TransformFeature *o = self(c);
            double phys_pos[NumVirtAxes];
            TupleForEachForward(&o->m_virt_axes, Foreach_read_phys_pos(), c, phys_pos);
            double virt_pos[NumVirtAxes];
            TheTransformAlg::physToVirt(phys_pos, virt_pos);
            TupleForEachForward(&o->m_virt_axes, Foreach_write_virt_pos(), c, virt_pos);
        }
        
        static void init_move_state_extra (Context c, MoveBuildState *s)
        {
            s->seen_virt = false;
        }
        
        static void resolve_virt (Context c, MoveBuildState *s)
        {
            TransformFeature *o = self(c);
            if (s->seen_virt) {
                double phys_pos[NumVirtAxes];
                TheTransformAlg::virtToPhys(s->new_pos + num_axes, phys_pos);
                o->m_virt_update_pending = false;
                TupleForEachForward(&o->m_virt_axes, Foreach_move_phys(), c, s, phys_pos);
            }
        }
        
        static void resolve_virt_post (Context c, MoveBuildState *s, double *distance_squared)
        {
            TransformFeature *o = self(c);
            if (s->seen_virt) {
                TupleForEachForward(&o->m_virt_axes, Foreach_add_virt_distance(), c, s, distance_squared);
            }
        }
        
        template <int PhysAxisIndex>
        static void mark_phys_moved (Context c)
        {
            TransformFeature *o = self(c);
            if (IsPhysAxisTransformPhys<PhysAxisIndex>::value) {
                o->m_virt_update_pending = true;
            }
        }
        
        static void do_pending_virt_update (Context c)
        {
            TransformFeature *o = self(c);
            if (o->m_virt_update_pending) {
                update_virt_from_phys(c);
                o->m_virt_update_pending = false;
            }
        }
        
        template <int VirtAxisIndex>
        struct VirtAxis {
            static int const axis_name = TypeListGet<VirtAxesList, VirtAxisIndex>::value;
            static int const PhysAxisIndex = FindAxis<TypeListGet<PhysAxesList, VirtAxisIndex>::value>::value;
            using ThePhysAxis = Axis<PhysAxisIndex>;
            static_assert(!ThePhysAxis::AxisSpec::enable_cartesian_speed_limit, "");
            using WrappedPhysAxisIndex = WrapInt<PhysAxisIndex>;
            
            static VirtAxis * self (Context c)
            {
                return PositionTraverse<typename Context::TheRootPosition, VirtAxisPosition<VirtAxisIndex>>(c.root());
            }
            
            static void init (Context c)
            {
                VirtAxis *o = self(c);
                o->m_relative_positioning = false;
            }
            
            static void update_new_pos (Context c, MoveBuildState *s, double req)
            {
                s->new_pos[num_axes + VirtAxisIndex] = req;
                s->seen_cartesian = true;
                s->seen_virt = true;
            }
            
            static void read_phys_pos (Context c, double *out)
            {
                ThePhysAxis *axis = ThePhysAxis::self(c);
                out[VirtAxisIndex] = axis->m_req_pos;
            }
            
            static void write_virt_pos (Context c, double const *in)
            {
                VirtAxis *o = self(c);
                o->m_req_pos = in[VirtAxisIndex];
            }
            
            static void move_phys (Context c, MoveBuildState *s, double const *phys_pos)
            {
                VirtAxis *o = self(c);
                s->virt_old_pos[VirtAxisIndex] = o->m_req_pos;
                o->m_req_pos = s->new_pos[num_axes + VirtAxisIndex];
                s->new_pos[PhysAxisIndex] = Axis<PhysAxisIndex>::clamp_req_pos(phys_pos[VirtAxisIndex]);
                if (s->new_pos[PhysAxisIndex] != phys_pos[VirtAxisIndex]) {
                    TransformFeature::self(c)->m_virt_update_pending = true;
                }
            }
            
            static void add_virt_distance (Context c, MoveBuildState *s, double *distance_squared)
            {
                VirtAxis *o = self(c);
                double axis_distance = o->m_req_pos - s->virt_old_pos[VirtAxisIndex];
                *distance_squared += axis_distance * axis_distance;
            }
            
            double m_req_pos;
            bool m_relative_positioning;
        };
        
        using VirtAxesTuple = IndexElemTuple<VirtAxesList, VirtAxis>;
        
        template <int PhysAxisIndex>
        using IsPhysAxisTransformPhys = WrapBool<(TypeListIndex<
            typename VirtAxesTuple::ElemTypes,
            ComposeFunctions<
                IsEqualFunc<WrapInt<PhysAxisIndex>>,
                GetMemberType_WrappedPhysAxisIndex
            >
        >::value >= 0)>;
        
        VirtAxesTuple m_virt_axes;
        bool m_virt_update_pending;
        
        template <int VirtAxisIndex> struct VirtAxisPosition : public TuplePosition<TransformFeaturePosition, VirtAxesTuple, &TransformFeature::m_virt_axes, VirtAxisIndex> {};
    } AMBRO_STRUCT_ELSE(TransformFeature) {
        static int const NumVirtAxes = 0;
        struct MoveStateExtra {};
        static void init (Context c) {}
        static void update_virt_from_phys (Context c) {}
        static void init_move_state_extra (Context c, MoveBuildState *s) {}
        static void resolve_virt (Context c, MoveBuildState *s) {}
        static void resolve_virt_post (Context c, MoveBuildState *s, double *distance_squared) {}
        template <int PhysAxisIndex>
        static void mark_phys_moved (Context c) {}
        static void do_pending_virt_update (Context c) {}
    };
    
    static int const NumPhysVirtAxes = num_axes + TransformFeature::NumVirtAxes;
    
    template <bool IsVirt, int PhysVirtAxisIndex>
    struct GetPhysVirtAxisHelper {
        using Type = Axis<PhysVirtAxisIndex>;
    };
    
    template <int PhysVirtAxisIndex>
    struct GetPhysVirtAxisHelper<true, PhysVirtAxisIndex> {
        using Type = typename TransformFeature::template VirtAxis<(PhysVirtAxisIndex - num_axes)>;
    };
    
    template <int PhysVirtAxisIndex>
    using GetPhysVirtAxis = typename GetPhysVirtAxisHelper<(PhysVirtAxisIndex >= num_axes), PhysVirtAxisIndex>::Type;
    
    template <int PhysVirtAxisIndex>
    struct PhysVirtAxisHelper {
        using TheAxis = GetPhysVirtAxis<PhysVirtAxisIndex>;
        using WrappedAxisName = WrapInt<TheAxis::axis_name>;
        
        static void init_new_pos (Context c, MoveBuildState *s)
        {
            TheAxis *axis = TheAxis::self(c);
            s->new_pos[PhysVirtAxisIndex] = axis->m_req_pos;
        }
        
        static void update_new_pos (Context c, MoveBuildState *s, double req)
        {
            TheAxis::update_new_pos(c, s, req);
        }
        
        template <typename TheChannelCommon>
        static void collect_new_pos (Context c, TheChannelCommon *cc, MoveBuildState *s, typename TheChannelCommon::GcodeParserCommandPart *part)
        {
            TheAxis *axis = TheAxis::self(c);
            if (AMBRO_UNLIKELY(part->code == TheAxis::axis_name)) {
                double req = strtod(part->data, NULL);
                if (isnan(req)) {
                    req = 0.0;
                }
                if (axis->m_relative_positioning) {
                    req += axis->m_req_pos;
                }
                update_new_pos(c, s, req);
            }
        }
        
        static void set_relative_positioning (Context c, bool relative)
        {
            TheAxis *axis = TheAxis::self(c);
            axis->m_relative_positioning = relative;
        }
        
        template <typename TheChannelCommon>
        static void append_position (Context c, TheChannelCommon *cc)
        {
            TheAxis *axis = TheAxis::self(c);
            cc->reply_append_ch(c, TheAxis::axis_name);
            cc->reply_append_ch(c, ':');
            cc->reply_append_double(c, axis->m_req_pos);
        }
    };
    
    using PhysVirtAxisHelperTuple = Tuple<IndexElemListCount<NumPhysVirtAxes, PhysVirtAxisHelper>>;
    
    template <int AxisName>
    using FindPhysVirtAxis = TypeListIndex<
        typename PhysVirtAxisHelperTuple::ElemTypes,
        ComposeFunctions<
            IsEqualFunc<WrapInt<AxisName>>,
            GetMemberType_WrappedAxisName
        >
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
        using TheSoftPwm = SoftPwm<SoftPwmPosition, Context, typename HeaterSpec::OutputPin, HeaterSpec::OutputInvert, typename HeaterSpec::PulseInterval, SoftPwmTimerHandler, HeaterSpec::template TimerTemplate>;
        using TheObserver = TemperatureObserver<ObserverPosition, Context, typename HeaterSpec::TheTemperatureObserverParams, ObserverGetValueCallback, ObserverHandler>;
        using OutputFixedType = typename TheControl::OutputFixedType;
        using PwmPowerData = typename TheSoftPwm::PowerData;
        
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
            TimeType time = c.clock()->getTime(c) + (TimeType)(0.05 * Clock::time_freq);
            o->m_main_control.init(c, time);
            o->m_adc_min = HeaterSpec::Formula::invert(max_safe_temp(), true);
            o->m_adc_max = HeaterSpec::Formula::invert(min_safe_temp(), false);
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
            auto adc_value = c.adc()->template getValue<typename HeaterSpec::AdcPin>(c);
            double value = HeaterSpec::Formula::call(adc_value).doubleValue();
            cc->reply_append_ch(c, ' ');
            cc->reply_append_ch(c, HeaterSpec::Name);
            cc->reply_append_ch(c, ':');
            cc->reply_append_double(c, value);
#ifdef PRINTERMAIN_DEBUG_ADC
            cc->reply_append_ch(c, ' ');
            cc->reply_append_ch(c, HeaterSpec::Name);
            cc->reply_append_pstr(c, AMBRO_PSTR("A:"));
            cc->reply_append_uint32(c, adc_value);
#endif
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
                submit_planner_command(c, &cmd);
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
                cc->reply_append_pstr(c, AMBRO_PSTR(": M" ));
                cc->reply_append_uint32(c, HeaterSpec::SetConfigMCommand);
                TheControl::printConfig(c, cc, &o->m_control_config);
                cc->reply_append_ch(c, '\n');
            }
        }
        
        static void softpwm_timer_handler (typename TheSoftPwm::TimerInstance::HandlerContext c, PwmPowerData *pd)
        {
            Heater *o = self(c);
            o->m_main_control.get_output_for_pwm(c, pd);
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
                TheSoftPwm::computePowerData(OutputFixedType::importBits(0), &o->m_output_pd);
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
                TheSoftPwm::computePowerData(OutputFixedType::importBits(0), &o->m_output_pd);
            }
            
            static void get_output_for_pwm (typename TheSoftPwm::TimerInstance::HandlerContext c, PwmPowerData *pd)
            {
                MainControl *o = self(c);
                Heater *h = Heater::self(c);
                uint16_t adc_value = c.adc()->template getValue<typename HeaterSpec::AdcPin>(c);
                if (AMBRO_LIKELY(adc_value <= h->m_adc_min || adc_value >= h->m_adc_max)) {
                    AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                        h->m_enabled = false;
                        unset(c);
                    }
                }
                AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                    *pd = o->m_output_pd;
                }
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
                    PwmPowerData output_pd;
                    TheSoftPwm::computePowerData(output, &output_pd);
                    AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                        if (o->m_was_not_unset) {
                            o->m_output_pd = output_pd;
                        }
                    }
                }
            }
            
            PwmPowerData m_output_pd;
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
            
            static void get_output_for_pwm (typename TheSoftPwm::TimerInstance::HandlerContext c, PwmPowerData *pd)
            {
                Heater *h = Heater::self(c);
                OutputFixedType control_value = OutputFixedType::importBits(0);
                AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                    uint16_t adc_value = lock_c.adc()->template getValue<typename HeaterSpec::AdcPin>(c);
                    ValueFixedType sensor_value = HeaterSpec::Formula::call(adc_value);
                    if (AMBRO_UNLIKELY(adc_value <= h->m_adc_min || adc_value >= h->m_adc_max)) {
                        h->m_enabled = false;
                    }
                    if (AMBRO_LIKELY(h->m_enabled)) {
                        control_value = h->m_control.addMeasurement(sensor_value, h->m_target, &h->m_control_config);
                    }
                }
                TheSoftPwm::computePowerData(control_value, pd);
            }
        };
        
        bool m_enabled;
        TheControl m_control;
        ControlConfig m_control_config;
        ValueFixedType m_target;
        uint16_t m_adc_min;
        uint16_t m_adc_max;
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
        using TheSoftPwm = SoftPwm<SoftPwmPosition, Context, typename FanSpec::OutputPin, FanSpec::OutputInvert, typename FanSpec::PulseInterval, SoftPwmTimerHandler, FanSpec::template TimerTemplate>;
        using OutputFixedType = FixedPoint<8, false, -8>;
        using PwmPowerData = typename TheSoftPwm::PowerData;
        
        struct ChannelPayload {
            PwmPowerData target_pd;
        };
        
        static Fan * self (Context c)
        {
            return PositionTraverse<typename Context::TheRootPosition, FanPosition<FanIndex>>(c.root());
        }
        
        static void init (Context c)
        {
            Fan *o = self(c);
            TheSoftPwm::computePowerData(OutputFixedType::importBits(0), &o->m_target_pd);
            TimeType time = c.clock()->getTime(c) + (TimeType)(0.05 * Clock::time_freq);
            o->m_softpwm.init(c, time);
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
                TheSoftPwm::computePowerData(OutputFixedType::importDoubleSaturated(target), &UnionGetElem<FanIndex>(&payload->fans)->target_pd);
                submit_planner_command(c, &cmd);
                return false;
            }
            return true;
        }
        
        static void softpwm_timer_handler (typename TheSoftPwm::TimerInstance::HandlerContext c, PwmPowerData *pd)
        {
            Fan *o = self(c);
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                *pd = o->m_target_pd;
            }
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
                o->m_target_pd = payload->target_pd;
            }
        }
        
        PwmPowerData m_target_pd;
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
    using ThePlanner = MotionPlanner<PlannerPosition, Context, MotionPlannerAxes, Params::StepperSegmentBufferSize, Params::LookaheadBufferSizeExp, PlannerPullHandler, PlannerFinishedHandler, PlannerAbortedHandler, MotionPlannerChannels>;
    using PlannerInputCommand = typename ThePlanner::InputCommand;
    
    template <int AxisIndex>
    using HomingStateTupleHelper = typename Axis<AxisIndex>::HomingFeature::HomingState;
    using HomingStateTuple = IndexElemTuple<AxesList, HomingStateTupleHelper>;
    
    AMBRO_STRUCT_IF(ProbeFeature, Params::ProbeParams::enabled) {
        using ProbeParams = typename Params::ProbeParams;
        static const int NumPoints = TypeListLength<typename ProbeParams::ProbePoints>::value;
        static const int ProbeAxisIndex = FindPhysVirtAxis<Params::ProbeParams::ProbeAxis>::value;
        
        static ProbeFeature * self (Context c)
        {
            return PositionTraverse<typename Context::TheRootPosition, ProbeFeaturePosition>(c.root());
        }
        
        static void init (Context c)
        {
            ProbeFeature *o = self(c);
            o->m_current_point = 0xff;
        }
        
        static void deinit (Context c)
        {
            ProbeFeature *o = self(c);
        }
        
        template <typename TheChannelCommon>
        static bool check_command (Context c, TheChannelCommon *cc)
        {
            ProbeFeature *o = self(c);
            if (cc->m_cmd_num == 32) {
                if (!cc->tryUnplannedCommand(c)) {
                    return false;
                }
                AMBRO_ASSERT(o->m_current_point == 0xff)
                init_probe_planner(c, false);
                o->m_current_point = 0;
                o->m_point_state = 0;
                o->m_command_sent = false;
                return false;
            }
            return true;
        }
        
        template <int PlatformAxisIndex>
        struct AxisHelper {
            using PlatformAxis = TypeListGet<typename ProbeParams::PlatformAxesList, PlatformAxisIndex>;
            static const int AxisIndex = FindPhysVirtAxis<PlatformAxis::value>::value;
            using AxisProbeOffset = TypeListGet<typename ProbeParams::ProbePlatformOffset, PlatformAxisIndex>;
            
            static void add_axis (Context c, MoveBuildState *s, uint8_t point_index)
            {
                PointHelperTuple dummy;
                double coord = TupleForOne<double>(point_index, &dummy, Foreach_get_coord());
                move_add_axis<AxisIndex>(c, s, coord + AxisProbeOffset::value());
            }
            
            template <int PointIndex>
            struct PointHelper {
                using Point = TypeListGet<typename ProbeParams::ProbePoints, PointIndex>;
                using PointCoord = TypeListGet<Point, PlatformAxisIndex>;
                
                static double get_coord ()
                {
                    return PointCoord::value();
                }
            };
            
            using PointHelperTuple = IndexElemTuple<typename ProbeParams::ProbePoints, PointHelper>;
        };
        
        using AxisHelperTuple = IndexElemTuple<typename ProbeParams::PlatformAxesList, AxisHelper>;
        
        static void custom_pull_handler (Context c)
        {
            ProbeFeature *o = self(c);
            AMBRO_ASSERT(o->m_current_point != 0xff)
            AMBRO_ASSERT(o->m_point_state <= 4)
            
            if (o->m_command_sent) {
                custom_planner_wait_finished(c);
                return;
            }
            MoveBuildState s;
            move_begin(c, &s);
            double height;
            double speed;
            switch (o->m_point_state) {
                case 0: {
                    AxisHelperTuple dummy;
                    TupleForEachForward(&dummy, Foreach_add_axis(), c, &s, o->m_current_point);
                    height = ProbeParams::ProbeStartHeight::value();
                    speed = ProbeParams::ProbeMoveSpeed::value();
                } break;
                case 1: {
                    height = ProbeParams::ProbeLowHeight::value();
                    speed = ProbeParams::ProbeFastSpeed::value();
                } break;
                case 2: {
                    height = get_height(c) + ProbeParams::ProbeRetractDist::value();
                    speed = ProbeParams::ProbeRetractSpeed::value();
                } break;
                case 3: {
                    height = ProbeParams::ProbeLowHeight::value();
                    speed = ProbeParams::ProbeSlowSpeed::value();
                } break;
                case 4: {
                    height = ProbeParams::ProbeStartHeight::value();
                    speed = ProbeParams::ProbeRetractSpeed::value();
                } break;
            }
            move_add_axis<ProbeAxisIndex>(c, &s, height);
            move_end(c, &s, speed);
            o->m_command_sent = true;
        }
        
        static void custom_finished_handler (Context c)
        {
            ProbeFeature *o = self(c);
            AMBRO_ASSERT(o->m_current_point != 0xff)
            AMBRO_ASSERT(o->m_command_sent)
            
            custom_planner_deinit(c);
            o->m_command_sent = false;
            if (o->m_point_state < 4) {
                if (o->m_point_state == 3) {
                    double height = get_height(c);
                    o->m_samples[o->m_current_point] = height;
                    Tuple<ChannelCommonList> dummy;
                    TupleForEachForwardInterruptible(&dummy, Foreach_run_for_state_command(), c, COMMAND_LOCKED, o, Foreach_report_height(), height);
                }
                o->m_point_state++;
                bool watch_probe = (o->m_point_state == 1 || o->m_point_state == 3);
                init_probe_planner(c, watch_probe);
            } else {
                o->m_current_point++;
                if (o->m_current_point == NumPoints) {
                    o->m_current_point = 0xff;
                    finish_locked(c);
                    return;
                }
                init_probe_planner(c, false);
                o->m_point_state = 0;
            }
        }
        
        static void custom_aborted_handler (Context c)
        {
            ProbeFeature *o = self(c);
            AMBRO_ASSERT(o->m_current_point != 0xff)
            AMBRO_ASSERT(o->m_command_sent)
            AMBRO_ASSERT(o->m_point_state == 1 || o->m_point_state == 3)
            
            custom_finished_handler(c);
        }
        
        template <typename CallbackContext>
        static bool prestep_callback (CallbackContext c)
        {
            return (c.pins()->template get<typename ProbeParams::ProbePin>(c) != Params::ProbeParams::ProbeInvert);
        }
        
        static void init_probe_planner (Context c, bool watch_probe)
        {
            custom_planner_init(c, PLANNER_PROBE, watch_probe);
        }
        
        static double get_height (Context c)
        {
            return GetPhysVirtAxis<ProbeAxisIndex>::self(c)->m_req_pos;
        }
        
        template <typename TheChannelCommon>
        static void report_height (Context c, TheChannelCommon *cc, double height)
        {
            cc->reply_append_pstr(c, AMBRO_PSTR("//ProbeHeight "));
            cc->reply_append_double(c, height);
            cc->reply_append_ch(c, '\n');
        }
        
        uint8_t m_current_point;
        uint8_t m_point_state;
        bool m_command_sent;
        double m_samples[NumPoints];
    } AMBRO_STRUCT_ELSE(ProbeFeature) {
        static void init (Context c) {}
        static void deinit (Context c) {}
        template <typename TheChannelCommon>
        static bool check_command (Context c, TheChannelCommon *cc) { return true; }
        static void custom_pull_handler (Context c) {}
        static void custom_finished_handler (Context c) {}
        static void custom_aborted_handler (Context c) {}
        template <typename CallbackContext>
        static bool prestep_callback (CallbackContext c) { return false; }
    };
    
public:
    static void init (Context c)
    {
        PrinterMain *o = self(c);
        
        o->m_unlocked_timer.init(c, PrinterMain::unlocked_timer_handler);
        o->m_disable_timer.init(c, PrinterMain::disable_timer_handler);
        o->m_force_timer.init(c, PrinterMain::force_timer_handler);
        o->m_watchdog.init(c);
        o->m_blinker.init(c, Params::LedBlinkInterval::value() * Clock::time_freq);
        o->m_steppers.init(c);
        o->m_serial_feature.init(c);
        o->m_sdcard_feature.init(c);
        TupleForEachForward(&o->m_axes, Foreach_init(), c);
        o->m_transform_feature.init(c);
        TupleForEachForward(&o->m_heaters, Foreach_init(), c);
        TupleForEachForward(&o->m_fans, Foreach_init(), c);
        o->m_probe_feature.init(c);
        o->m_inactive_time = Params::DefaultInactiveTime::value() * Clock::time_freq;
        o->m_max_speed = INFINITY;
        o->m_locked = false;
        o->m_planner_state = PLANNER_NONE;
        
        o->m_serial_feature.m_channel_common.reply_append_pstr(c, AMBRO_PSTR("start\nAPrinter\n"));
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        PrinterMain *o = self(c);
        o->debugDeinit(c);
        
        if (o->m_planner_state != PLANNER_NONE) {
            o->m_planner.deinit(c);
        }
        o->m_probe_feature.deinit(c);
        TupleForEachReverse(&o->m_fans, Foreach_deinit(), c);
        TupleForEachReverse(&o->m_heaters, Foreach_deinit(), c);
        TupleForEachReverse(&o->m_axes, Foreach_deinit(), c);
        o->m_sdcard_feature.deinit(c);
        o->m_serial_feature.deinit(c);
        o->m_steppers.deinit(c);
        o->m_blinker.deinit(c);
        o->m_watchdog.deinit(c);
        o->m_force_timer.deinit(c);
        o->m_disable_timer.deinit(c);
        o->m_unlocked_timer.deinit(c);
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
    
    template <typename TSdCardFeatue = SdCardFeature>
    typename TSdCardFeatue::TheSdCard * getSdCard ()
    {
        return &m_sdcard_feature.m_sdcard;
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
        typename SdCardFeature::EventLoopFastEvents,
        JoinTypeLists<
            typename SerialFeature::TheSerial::EventLoopFastEvents,
            JoinTypeLists<
                typename ThePlanner::EventLoopFastEvents,
                TypeListFold<
                    MapTypeList<typename AxesTuple::ElemTypes, GetMemberType_EventLoopFastEvents>,
                    EmptyTypeList,
                    JoinTypeLists
                >
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
                        TupleForEachForwardInterruptible(&o->m_fans, Foreach_check_command(), c, cc) &&
                        o->m_sdcard_feature.check_command(c, cc) &&
                        o->m_probe_feature.check_command(c, cc) 
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
                    cc->reply_append_pstr(c, AMBRO_PSTR("ok"));
                    TupleForEachForward(&o->m_heaters, Foreach_append_value(), c, cc);
                    cc->reply_append_ch(c, '\n');
                    return cc->finishCommand(c, true);
                } break;
                
                case 114: {
                    PhysVirtAxisHelperTuple dummy;
                    TupleForEachForward(&dummy, Foreach_append_position(), c, cc);
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
                    MoveBuildState s;
                    move_begin(c, &s);
                    for (typename TheChannelCommon::GcodePartsSizeType i = 1; i < cc->m_cmd->num_parts; i++) {
                        typename TheChannelCommon::GcodeParserCommandPart *part = &cc->m_cmd->parts[i];
                        PhysVirtAxisHelperTuple dummy;
                        TupleForEachForward(&dummy, Foreach_collect_new_pos(), c, cc, &s, part);
                        if (part->code == 'F') {
                            o->m_max_speed = strtod(part->data, NULL) * Params::SpeedLimitMultiply::value();
                        }
                    }
                    cc->finishCommand(c);
                    move_end(c, &s, o->m_max_speed);
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
                    PhysVirtAxisHelperTuple dummy;
                    TupleForEachForward(&dummy, Foreach_set_relative_positioning(), c, false);
                    return cc->finishCommand(c);
                } break;
                
                case 91: { // relative positioning
                    PhysVirtAxisHelperTuple dummy;
                    TupleForEachForward(&dummy, Foreach_set_relative_positioning(), c, true);
                    return cc->finishCommand(c);
                } break;
                
                case 92: { // set position
                    bool found_axes = false;
                    for (typename TheChannelCommon::GcodePartsSizeType i = 1; i < cc->m_cmd->num_parts; i++) {
                        TupleForEachForward(&o->m_axes, Foreach_set_position(), c, cc, &cc->m_cmd->parts[i], &found_axes);
                    }
                    if (!found_axes) {
                        cc->reply_append_pstr(c, AMBRO_PSTR("Error:not supported\n"));
                    }
                    return cc->finishCommand(c);
                } break;
            } break;
            
            unknown_command:
            default: {
                cc->reply_append_pstr(c, AMBRO_PSTR("Error:Unknown command "));
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
        
        o->m_transform_feature.do_pending_virt_update(c);
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
        AMBRO_ASSERT(o->m_planner_state == PLANNER_RUNNING)
        AMBRO_ASSERT(o->m_planning_pull_pending)
        
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
        AMBRO_ASSERT(m->m_planner_state == PLANNER_RUNNING)
        AMBRO_ASSERT(m->m_planning_pull_pending)
        AMBRO_ASSERT(cc->m_state == COMMAND_LOCKED)
        AMBRO_ASSERT(cc->m_cmd)
        
        work_command<TheChannelCommon>(c);
    }
    
    static void planner_pull_handler (Context c)
    {
        PrinterMain *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_planner_state != PLANNER_NONE)
        AMBRO_ASSERT(!o->m_planning_pull_pending)
        
        o->m_planning_pull_pending = true;
        if (o->m_planner_state == PLANNER_STOPPING) {
            o->m_planner.waitFinished(c);
        } else if (o->m_planner_state == PLANNER_WAITING) {
            o->m_planner_state = PLANNER_RUNNING;
            ChannelCommonTuple dummy;
            TupleForEachForwardInterruptible(&dummy, Foreach_run_for_state_command(), c, COMMAND_LOCKED, o, Foreach_continue_planned_helper());
        } else if (o->m_planner_state == PLANNER_RUNNING) {
            TimeType force_time = c.clock()->getTime(c) + (TimeType)(Params::ForceTimeout::value() * Clock::time_freq);
            o->m_force_timer.appendAt(c, force_time);
        } else {
            AMBRO_ASSERT(o->m_planner_state == PLANNER_PROBE)
            o->m_probe_feature.custom_pull_handler(c);
        }
    }
    
    template <typename TheChannelCommon>
    static void continue_unplanned_helper (Context c, TheChannelCommon *cc)
    {
        PrinterMain *m = PrinterMain::self(c);
        AMBRO_ASSERT(m->m_locked)
        AMBRO_ASSERT(m->m_planner_state == PLANNER_NONE)
        AMBRO_ASSERT(cc->m_state == COMMAND_LOCKED)
        AMBRO_ASSERT(cc->m_cmd)
        
        work_command<TheChannelCommon>(c);
    }
    
    static void planner_finished_handler (Context c)
    {
        PrinterMain *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_planner_state != PLANNER_NONE)
        AMBRO_ASSERT(o->m_planning_pull_pending)
        AMBRO_ASSERT(o->m_planner_state != PLANNER_WAITING)
        
        if (o->m_planner_state == PLANNER_PROBE) {
            return o->m_probe_feature.custom_finished_handler(c);
        }
        
        uint8_t old_state = o->m_planner_state;
        o->m_planner.deinit(c);
        o->m_force_timer.unset(c);
        o->m_planner_state = PLANNER_NONE;
        now_inactive(c);
        
        if (old_state == PLANNER_STOPPING) {
            ChannelCommonTuple dummy;
            TupleForEachForwardInterruptible(&dummy, Foreach_run_for_state_command(), c, COMMAND_LOCKED, o, Foreach_continue_unplanned_helper());
        }
    }
    
    static void planner_aborted_handler (Context c)
    {
        PrinterMain *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_planner_state == PLANNER_PROBE)
        
        TupleForEachForward(&o->m_axes, Foreach_fix_aborted_pos(), c);
        o->m_transform_feature.do_pending_virt_update(c);
        o->m_probe_feature.custom_aborted_handler(c);
    }
    
    static void planner_channel_callback (typename ThePlanner::template Channel<0>::CallbackContext c, PlannerChannelPayload *payload)
    {
        PrinterMain *o = self(c);
        o->debugAccess(c);
        
        TupleForOneBoolOffset<0>(payload->type, &o->m_heaters, Foreach_channel_callback(), c, &payload->heaters) ||
        TupleForOneBoolOffset<TypeListLength<HeatersList>::value>(payload->type, &o->m_fans, Foreach_channel_callback(), c, &payload->fans);
    }
    
    template <int AxisIndex>
    static bool planner_prestep_callback (typename ThePlanner::template Axis<AxisIndex>::StepperCommandCallbackContext c)
    {
        PrinterMain *o = self(c);
        return o->m_probe_feature.prestep_callback(c);
    }
    
    struct MoveBuildState : public TransformFeature::MoveStateExtra {
        double new_pos[NumPhysVirtAxes];
        bool seen_cartesian;
    };
    
    static void move_begin (Context c, MoveBuildState *s)
    {
        PrinterMain *o = self(c);
        PhysVirtAxisHelperTuple dummy;
        TupleForEachForward(&dummy, Foreach_init_new_pos(), c, s);
        s->seen_cartesian = false;
        o->m_transform_feature.init_move_state_extra(c, s);
    }
    
    template <int PhysVirtAxisIndex>
    static void move_add_axis (Context c, MoveBuildState *s, double value)
    {
        PhysVirtAxisHelper<PhysVirtAxisIndex>::update_new_pos(c, s, value);
    }
    
    static void move_end (Context c, MoveBuildState *s, double max_speed)
    {
        PrinterMain *o = self(c);
        o->m_transform_feature.resolve_virt(c, s);
        PlannerInputCommand cmd;
        double distance_squared = 0.0;
        double total_steps = 0.0;
        TupleForEachForward(&o->m_axes, Foreach_process_new_pos(), c, s, max_speed, &distance_squared, &total_steps, &cmd);
        o->m_transform_feature.do_pending_virt_update(c);
        o->m_transform_feature.resolve_virt_post(c, s, &distance_squared);
        cmd.type = 0;
        cmd.rel_max_v_rec = total_steps * (1.0 / (Params::MaxStepsPerCycle::value() * F_CPU * Clock::time_unit));
        if (s->seen_cartesian) {
            cmd.rel_max_v_rec = fmax(cmd.rel_max_v_rec, FloatMakePosOrPosZero(sqrt(distance_squared) / (max_speed * Clock::time_unit)));
        }
        submit_planner_command(c, &cmd);
    }
    
    static void submit_planner_command (Context c, PlannerInputCommand *cmd)
    {
        PrinterMain *o = self(c);
        AMBRO_ASSERT(o->m_planner_state == PLANNER_RUNNING || o->m_planner_state == PLANNER_PROBE)
        AMBRO_ASSERT(o->m_planning_pull_pending)
        
        o->m_planner.commandDone(c, cmd);
        o->m_planning_pull_pending = false;
        o->m_force_timer.unset(c);
    }
    
    static void custom_planner_init (Context c, uint8_t type, bool enable_prestep_callback)
    {
        PrinterMain *o = self(c);
        AMBRO_ASSERT(o->m_locked)
        AMBRO_ASSERT(o->m_planner_state == PLANNER_NONE)
        AMBRO_ASSERT(type == PLANNER_PROBE)
        
        o->m_planner_state = type;
        o->m_planner.init(c, enable_prestep_callback);
        o->m_planning_pull_pending = false;
        now_active(c);
    }
    
    static void custom_planner_deinit (Context c)
    {
        PrinterMain *o = self(c);
        AMBRO_ASSERT(o->m_locked)
        AMBRO_ASSERT(o->m_planner_state == PLANNER_PROBE)
        
        o->m_planner.deinit(c);
        o->m_planner_state = PLANNER_NONE;
        now_inactive(c);
    }
    
    static void custom_planner_wait_finished (Context c)
    {
        PrinterMain *o = self(c);
        AMBRO_ASSERT(o->m_locked)
        AMBRO_ASSERT(o->m_planner_state == PLANNER_PROBE)
        AMBRO_ASSERT(o->m_planning_pull_pending)
        
        o->m_planner.waitFinished(c);
    }
    
    typename Loop::QueuedEvent m_unlocked_timer;
    typename Loop::QueuedEvent m_disable_timer;
    typename Loop::QueuedEvent m_force_timer;
    TheWatchdog m_watchdog;
    TheBlinker m_blinker;
    TheSteppers m_steppers;
    SerialFeature m_serial_feature;
    SdCardFeature m_sdcard_feature;
    AxesTuple m_axes;
    TransformFeature m_transform_feature;
    HeatersTuple m_heaters;
    FansTuple m_fans;
    ProbeFeature m_probe_feature;
    TimeType m_inactive_time;
    TimeType m_last_active_time;
    double m_max_speed;
    bool m_locked;
    uint8_t m_planner_state;
    union {
        struct {
            HomingStateTuple m_homers;
            AxisCountType m_homing_rem_axes;
        };
        struct {
            ThePlanner m_planner;
            bool m_planning_pull_pending;
        };
    };
    
    struct WatchdogPosition : public MemberPosition<Position, TheWatchdog, &PrinterMain::m_watchdog> {};
    struct BlinkerPosition : public MemberPosition<Position, TheBlinker, &PrinterMain::m_blinker> {};
    struct SteppersPosition : public MemberPosition<Position, TheSteppers, &PrinterMain::m_steppers> {};
    template <int AxisIndex> struct AxisPosition : public TuplePosition<Position, AxesTuple, &PrinterMain::m_axes, AxisIndex> {};
    template <int AxisIndex> struct HomingFeaturePosition : public MemberPosition<AxisPosition<AxisIndex>, typename Axis<AxisIndex>::HomingFeature, &Axis<AxisIndex>::m_homing_feature> {};
    template <int AxisIndex> struct HomingStatePosition : public TuplePosition<Position, HomingStateTuple, &PrinterMain::m_homers, AxisIndex> {};
    struct TransformFeaturePosition : public MemberPosition<Position, TransformFeature, &PrinterMain::m_transform_feature> {};
    struct SerialFeaturePosition : public MemberPosition<Position, SerialFeature, &PrinterMain::m_serial_feature> {};
    struct SdCardFeaturePosition : public MemberPosition<Position, SdCardFeature, &PrinterMain::m_sdcard_feature> {};
    struct PlannerPosition : public MemberPosition<Position, ThePlanner, &PrinterMain::m_planner> {};
    template <int HeaterIndex> struct HeaterPosition : public TuplePosition<Position, HeatersTuple, &PrinterMain::m_heaters, HeaterIndex> {};
    template <int HeaterIndex> struct MainControlPosition : public MemberPosition<HeaterPosition<HeaterIndex>, typename Heater<HeaterIndex>::MainControl, &Heater<HeaterIndex>::m_main_control> {};
    template <int FanIndex> struct FanPosition : public TuplePosition<Position, FansTuple, &PrinterMain::m_fans, FanIndex> {};
    struct ProbeFeaturePosition : public MemberPosition<Position, ProbeFeature, &PrinterMain::m_probe_feature> {};
    
    struct BlinkerHandler : public AMBRO_WFUNC_TD(&PrinterMain::blinker_handler) {};
    template <int AxisIndex> struct PlannerGetAxisStepper : public AMBRO_WFUNC_TD(&PrinterMain::template planner_get_axis_stepper<AxisIndex>) {};
    struct PlannerPullHandler : public AMBRO_WFUNC_TD(&PrinterMain::planner_pull_handler) {};
    struct PlannerFinishedHandler : public AMBRO_WFUNC_TD(&PrinterMain::planner_finished_handler) {};
    struct PlannerAbortedHandler : public AMBRO_WFUNC_TD(&PrinterMain::planner_aborted_handler) {};
    struct PlannerChannelCallback : public AMBRO_WFUNC_TD(&PrinterMain::planner_channel_callback) {};
    template <int AxisIndex> struct PlannerPrestepCallback : public AMBRO_WFUNC_TD(&PrinterMain::template planner_prestep_callback<AxisIndex>) {};
    template <int AxisIndex> struct AxisStepperConsumersList {
        using List = JoinTypeLists<
            MakeTypeList<typename ThePlanner::template TheAxisStepperConsumer<AxisIndex>>,
            typename Axis<AxisIndex>::HomingFeature::template MakeAxisStepperConsumersList<typename Axis<AxisIndex>::HomingFeature>
        >;
    };
};

#include <aprinter/EndNamespace.h>

#endif
