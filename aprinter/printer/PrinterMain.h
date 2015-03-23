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
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/IndexElemTuple.h>
#include <aprinter/meta/TupleGet.h>
#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/WrapDouble.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/IndexElemList.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/JoinTypeLists.h>
#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/Union.h>
#include <aprinter/meta/UnionGet.h>
#include <aprinter/meta/GetMemberTypeFunc.h>
#include <aprinter/meta/TypeListFold.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/ComposeFunctions.h>
#include <aprinter/meta/IsEqualFunc.h>
#include <aprinter/meta/FilterTypeList.h>
#include <aprinter/meta/NotFunc.h>
#include <aprinter/meta/PowerOfTwo.h>
#include <aprinter/base/Object.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/WrapType.h>
#include <aprinter/meta/ConstexprMath.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/Expr.h>
#include <aprinter/meta/JoinTypeListList.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Likely.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Optimize.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/devices/Blinker.h>
#include <aprinter/driver/StepperGroups.h>
#include <aprinter/printer/GcodeParser.h>
#include <aprinter/printer/BinaryGcodeParser.h>
#include <aprinter/printer/MotionPlanner.h>
#include <aprinter/printer/Configuration.h>
#include <aprinter/printer/Command.h>
#include <aprinter/printer/InputCommon.h>

#include <aprinter/BeginNamespace.h>

#define PRINTERMAIN_SERIAL_OK_STR "ok\n"

template <
    typename TSerial, typename TLedPin, typename TLedBlinkInterval, typename TInactiveTime,
    typename TSpeedLimitMultiply, typename TMaxStepsPerCycle,
    int TStepperSegmentBufferSize, int TEventChannelBufferSize, int TLookaheadBufferSize,
    int TLookaheadCommitCount,
    typename TForceTimeout, typename TFpType,
    typename TEventChannelTimerService,
    typename TWatchdogService,
    typename TSdCardParams, typename TProbeParams, typename TCurrentParams,
    typename TConfigManagerService,
    typename TConfigList,
    typename TAxesList, typename TTransformParams, typename THeatersList, typename TFansList,
    typename TLasersList = EmptyTypeList
>
struct PrinterMainParams {
    using Serial = TSerial;
    using LedPin = TLedPin;
    using LedBlinkInterval = TLedBlinkInterval;
    using InactiveTime = TInactiveTime;
    using SpeedLimitMultiply = TSpeedLimitMultiply;
    using MaxStepsPerCycle = TMaxStepsPerCycle;
    static int const StepperSegmentBufferSize = TStepperSegmentBufferSize;
    static int const EventChannelBufferSize = TEventChannelBufferSize;
    static int const LookaheadBufferSize = TLookaheadBufferSize;
    static int const LookaheadCommitCount = TLookaheadCommitCount;
    using ForceTimeout = TForceTimeout;
    using FpType = TFpType;
    using EventChannelTimerService = TEventChannelTimerService;
    using WatchdogService = TWatchdogService;
    using SdCardParams = TSdCardParams;
    using ProbeParams = TProbeParams;
    using CurrentParams = TCurrentParams;
    using ConfigManagerService = TConfigManagerService;
    using ConfigList = TConfigList;
    using AxesList = TAxesList;
    using TransformParams = TTransformParams;
    using HeatersList = THeatersList;
    using FansList = TFansList;
    using LasersList = TLasersList;
};

template <
    uint32_t TBaud,
    int TRecvBufferSizeExp, int TSendBufferSizeExp,
    typename TTheGcodeParserParams,
    typename TSerialService
>
struct PrinterMainSerialParams {
    static uint32_t const Baud = TBaud;
    static int const RecvBufferSizeExp = TRecvBufferSizeExp;
    static int const SendBufferSizeExp = TSendBufferSizeExp;
    using TheGcodeParserParams = TTheGcodeParserParams;
    using SerialService = TSerialService;
};

template <
    char TName,
    typename TDirPin, typename TStepPin, typename TEnablePin, typename TInvertDir,
    typename TDefaultStepsPerUnit, typename TDefaultMin, typename TDefaultMax,
    typename TDefaultMaxSpeed, typename TDefaultMaxAccel,
    typename TDefaultDistanceFactor, typename TDefaultCorneringDistance,
    typename THoming, bool TIsCartesian, int TStepBits,
    typename TTheAxisDriverService, typename TMicroStep,
    typename TSlaveSteppersList = EmptyTypeList
>
struct PrinterMainAxisParams {
    static char const Name = TName;
    using DirPin = TDirPin;
    using StepPin = TStepPin;
    using EnablePin = TEnablePin;
    using InvertDir = TInvertDir;
    using DefaultStepsPerUnit = TDefaultStepsPerUnit;
    using DefaultMin = TDefaultMin;
    using DefaultMax = TDefaultMax;
    using DefaultMaxSpeed = TDefaultMaxSpeed;
    using DefaultMaxAccel = TDefaultMaxAccel;
    using DefaultDistanceFactor = TDefaultDistanceFactor;
    using DefaultCorneringDistance = TDefaultCorneringDistance;
    using Homing = THoming;
    static bool const IsCartesian = TIsCartesian;
    static int const StepBits = TStepBits;
    using TheAxisDriverService = TTheAxisDriverService;
    using MicroStep = TMicroStep;
    using SlaveSteppersList = TSlaveSteppersList;
};

template <
    typename TDirPin, typename TStepPin, typename TEnablePin, typename TInvertDir
>
struct PrinterMainSlaveStepperParams {
    using DirPin = TDirPin;
    using StepPin = TStepPin;
    using EnablePin = TEnablePin;
    using InvertDir = TInvertDir;
};

struct PrinterMainNoMicroStepParams {
    static bool const Enabled = false;
};

template <
    template<typename, typename, typename> class TMicroStepTemplate,
    typename TMicroStepParams,
    uint8_t TMicroSteps
>
struct PrinterMainMicroStepParams {
    static bool const Enabled = true;
    template <typename X, typename Y, typename Z> using MicroStepTemplate = TMicroStepTemplate<X, Y, Z>;
    using MicroStepParams = TMicroStepParams;
    static uint8_t const MicroSteps = TMicroSteps;
};

struct PrinterMainNoHomingParams {
    static bool const Enabled = false;
};

template <
    typename THomeDir,
    typename THomerService
>
struct PrinterMainHomingParams {
    static bool const Enabled = true;
    using HomeDir = THomeDir;
    using HomerService = THomerService;
};

struct PrinterMainNoTransformParams {
    static const bool Enabled = false;
};

template <
    typename TVirtAxesList, typename TPhysAxesList,
    typename TSegmentsPerSecond,
    typename TTransformService
>
struct PrinterMainTransformParams {
    static bool const Enabled = true;
    using VirtAxesList = TVirtAxesList;
    using PhysAxesList = TPhysAxesList;
    using SegmentsPerSecond = TSegmentsPerSecond;
    using TransformService = TTransformService;
};

template <
    char TName, typename TMinPos, typename TMaxPos,
    typename TMaxSpeed, typename TVirtualHoming
>
struct PrinterMainVirtualAxisParams {
    static char const Name = TName;
    using MinPos = TMinPos;
    using MaxPos = TMaxPos;
    using MaxSpeed = TMaxSpeed;
    using VirtualHoming = TVirtualHoming;
};

struct PrinterMainNoVirtualHomingParams {
    static bool const Enabled = false;
};

template <
    typename TEndPin, typename TEndPinInputMode, typename TEndInvert, typename THomeDir,
    typename TFastExtraDist, typename TRetractDist, typename TSlowExtraDist,
    typename TFastSpeed, typename TRetractSpeed, typename TSlowSpeed
>
struct PrinterMainVirtualHomingParams {
    static bool const Enabled = true;
    using EndPin = TEndPin;
    using EndPinInputMode = TEndPinInputMode;
    using EndInvert = TEndInvert;
    using HomeDir = THomeDir;
    using FastExtraDist = TFastExtraDist;
    using RetractDist = TRetractDist;
    using SlowExtraDist = TSlowExtraDist;
    using FastSpeed = TFastSpeed;
    using RetractSpeed = TRetractSpeed;
    using SlowSpeed = TSlowSpeed;
};

template <
    char TName, int TSetMCommand, int TWaitMCommand,
    typename TAdcPin,
    typename TFormula,
    typename TMinSafeTemp, typename TMaxSafeTemp,
    typename TControlInterval,
    typename TControlService,
    typename TObserverService,
    typename TPwmService
>
struct PrinterMainHeaterParams {
    static char const Name = TName;
    static int const SetMCommand = TSetMCommand;
    static int const WaitMCommand = TWaitMCommand;
    using AdcPin = TAdcPin;
    using Formula = TFormula;
    using MinSafeTemp = TMinSafeTemp;
    using MaxSafeTemp = TMaxSafeTemp;
    using ControlInterval = TControlInterval;
    using ControlService = TControlService;
    using ObserverService = TObserverService;
    using PwmService = TPwmService;
};

template <
    int TSetMCommand, int TOffMCommand,
    typename TSpeedMultiply,
    typename TPwmService
>
struct PrinterMainFanParams {
    static int const SetMCommand = TSetMCommand;
    static int const OffMCommand = TOffMCommand;
    using SpeedMultiply = TSpeedMultiply;
    using PwmService = TPwmService;
};

struct PrinterMainNoSdCardParams {
    static bool const Enabled = false;
};

template <
    typename TInputService,
    template<typename, typename, typename, typename> class TGcodeParserTemplate,
    typename TTheGcodeParserParams,
    size_t TBufferBaseSize,
    size_t TMaxCommandSize
>
struct PrinterMainSdCardParams {
    static bool const Enabled = true;
    using InputService = TInputService;
    template <typename X, typename Y, typename Z, typename W> using GcodeParserTemplate = TGcodeParserTemplate<X, Y, Z, W>;
    using TheGcodeParserParams = TTheGcodeParserParams;
    static size_t const BufferBaseSize = TBufferBaseSize;
    static size_t const MaxCommandSize = TMaxCommandSize;
};

struct PrinterMainNoProbeParams {
    static bool const Enabled = false;
};

template <
    typename TPlatformAxesList,
    char TProbeAxis,
    typename TProbePin,
    typename TProbePinInputMode,
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
    static bool const Enabled = true;
    using PlatformAxesList = TPlatformAxesList;
    static char const ProbeAxis = TProbeAxis;
    using ProbePin = TProbePin;
    using ProbePinInputMode = TProbePinInputMode;
    static bool const ProbeInvert = TProbeInvert;
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

struct PrinterMainNoCurrentParams {
    static bool const Enabled = false;
};

template <
    typename TCurrentAxesList,
    typename TCurrentService
>
struct PrinterMainCurrentParams {
    static bool const Enabled = true;
    using CurrentAxesList = TCurrentAxesList;
    using CurrentService = TCurrentService;
};

template <
    char TAxisName,
    typename TDefaultCurrent,
    typename TParams
>
struct PrinterMainCurrentAxis {
    static char const AxisName = TAxisName;
    using DefaultCurrent = TDefaultCurrent;
    using Params = TParams;
};

template <
    char TName,
    char TDensityName,
    typename TLaserPower,
    typename TMaxPower,
    typename TPwmService,
    typename TDutyFormulaService,
    typename TTheLaserDriverService
>
struct PrinterMainLaserParams {
    static char const Name = TName;
    static char const DensityName = TDensityName;
    using LaserPower = TLaserPower;
    using MaxPower = TMaxPower;
    using PwmService = TPwmService;
    using DutyFormulaService = TDutyFormulaService;
    using TheLaserDriverService = TTheLaserDriverService;
};

template <typename Context, typename ParentObject, typename Params>
class PrinterMain {
public:
    struct Object;
    
public: // private, workaround gcc bug, http://stackoverflow.com/questions/22083662/c-strange-is-private-error
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_init, init)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_deinit, deinit)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_limit_virt_axis_speed, limit_virt_axis_speed)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_clamp_req_phys, clamp_req_phys)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_clamp_move_phys, clamp_move_phys)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_prepare_split, prepare_split)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_compute_split, compute_split)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_get_final_split, get_final_split)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_finish_set_position, finish_set_position)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_emergency, emergency)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_start_homing, start_homing)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_start_virt_homing, start_virt_homing)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_prestep_callback, prestep_callback)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_update_homing_mask, update_homing_mask)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_enable_disable_stepper, enable_disable_stepper)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_enable_disable_stepper_specific, enable_disable_stepper_specific)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_do_move, do_move)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_limit_axis_move_speed, limit_axis_move_speed)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_fix_aborted_pos, fix_aborted_pos)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_m119_append_endstop, m119_append_endstop)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_append_value, append_value)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_append_adc_value, append_adc_value)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_check_command, check_command)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_channel_callback, channel_callback)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_get_command_in_state_helper, get_command_in_state_helper)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_add_axis, add_axis)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_check_current_axis, check_current_axis)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_get_coord, get_coord)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_append_position, append_position)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_collect_new_pos, collect_new_pos)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_set_relative_positioning, set_relative_positioning)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_g92_check_axis, g92_check_axis)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_init_new_pos, init_new_pos)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_handle_automatic_energy, handle_automatic_energy)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_write_planner_cmd, write_planner_cmd)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_check_safety, check_safety)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_apply_default, apply_default)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_begin_move, begin_move)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_ChannelPayload, ChannelPayload)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_WrappedAxisName, WrappedAxisName)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_WrappedPhysAxisIndex, WrappedPhysAxisIndex)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_HomingState, HomingState)
    APRINTER_DECLARE_COLLECTIBLE(Collectible_EventLoopFastEvents, EventLoopFastEvents)
    APRINTER_DECLARE_COLLECTIBLE(Collectible_ConfigExprs, ConfigExprs)
    
    struct PlannerUnionPlanner;
    struct PlannerUnionHoming;
    struct ConfigManagerHandler;
    struct BlinkerHandler;
    struct PlannerPullHandler;
    struct PlannerFinishedHandler;
    struct PlannerAbortedHandler;
    struct PlannerUnderrunCallback;
    struct PlannerChannelCallback;
    template <int AxisIndex> struct PlannerPrestepCallback;
    template <int AxisIndex> struct AxisDriverConsumersList;
    struct DelayedConfigExprs;
    
    using Loop = typename Context::EventLoop;
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using FpType = typename Params::FpType;
    using ParamsAxesList = typename Params::AxesList;
    using ParamsLasersList = typename Params::LasersList;
    using TransformParams = typename Params::TransformParams;
    using ParamsHeatersList = typename Params::HeatersList;
    using ParamsFansList = typename Params::FansList;
    static const int NumAxes = TypeListLength<ParamsAxesList>::Value;
    using CommandType = Command<Context, FpType>;
    
    using TheDebugObject = DebugObject<Context, Object>;
    using TheWatchdog = typename Params::WatchdogService::template Watchdog<Context, Object>;
    using TheConfigManager = typename Params::ConfigManagerService::template ConfigManager<Context, Object, typename Params::ConfigList, PrinterMain, ConfigManagerHandler>;
    using TheConfigCache = ConfigCache<Context, Object, DelayedConfigExprs>;
    using Config = ConfigFramework<TheConfigManager, TheConfigCache>;
    using TheBlinker = Blinker<Context, Object, typename Params::LedPin, BlinkerHandler>;
    
    template <typename TheSlaveStepper>
    using MakeSlaveStepperDef = StepperDef<
        typename TheSlaveStepper::DirPin,
        typename TheSlaveStepper::StepPin,
        typename TheSlaveStepper::EnablePin,
        decltype(Config::e(TheSlaveStepper::InvertDir::i))
    >;
    
    template <typename TheAxis>
    using MakeStepperGroupParams = StepperGroupParams<
        JoinTypeLists<
            MakeTypeList<
                StepperDef<
                    typename TheAxis::DirPin,
                    typename TheAxis::StepPin,
                    typename TheAxis::EnablePin,
                    decltype(Config::e(TheAxis::InvertDir::i))
                >
            >,
            MapTypeList<typename TheAxis::SlaveSteppersList, TemplateFunc<MakeSlaveStepperDef>>
        >
    >;
    
    using StepperGroupParamsList = MapTypeList<ParamsAxesList, TemplateFunc<MakeStepperGroupParams>>;
    using TheSteppers = StepperGroups<Context, Object, Config, StepperGroupParamsList>;
    
    static_assert(Params::LedBlinkInterval::value() < TheWatchdog::WatchdogTime / 2.0, "");
    
    using TimeConversion = APRINTER_FP_CONST_EXPR(Clock::time_freq);
    using TimeRevConversion = APRINTER_FP_CONST_EXPR(Clock::time_unit);
    using FCpu = APRINTER_FP_CONST_EXPR(F_CPU);
    
    using CInactiveTimeTicks = decltype(ExprCast<TimeType>(Config::e(Params::InactiveTime::i) * TimeConversion()));
    using CStepSpeedLimitFactor = decltype(ExprCast<FpType>(ExprRec(Config::e(Params::MaxStepsPerCycle::i) * FCpu() * TimeRevConversion())));
    using CForceTimeoutTicks = decltype(ExprCast<TimeType>(Config::e(Params::ForceTimeout::i) * TimeConversion()));
    
    using MyConfigExprs = MakeTypeList<CInactiveTimeTicks, CStepSpeedLimitFactor, CForceTimeoutTicks>;
    
    enum {COMMAND_IDLE, COMMAND_LOCKING, COMMAND_LOCKED};
    enum {PLANNER_NONE, PLANNER_RUNNING, PLANNER_STOPPING, PLANNER_WAITING, PLANNER_CUSTOM};
    
    struct SetPositionState;
    
    using TheCommand = CommandType;
    using CommandPartRef = typename TheCommand::PartRef;
    
    template <typename ChannelParentObject, typename Channel>
    struct ChannelCommon {
        struct Object;
        struct CommandImpl;
        using TheGcodeParser = typename Channel::TheGcodeParser;
        using GcodeParserPartRef = typename TheGcodeParser::PartRef;
        
        static CommandImpl * impl (Context c)
        {
            auto *o = Object::self(c);
            return &o->m_cmd_impl;
        }
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            o->m_state = COMMAND_IDLE;
            o->m_cmd = false;
            o->m_send_buf_event_handler = NULL;
        }
        
        static void startCommand (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->m_state == COMMAND_IDLE)
            AMBRO_ASSERT(!o->m_cmd)
            
            o->m_cmd = true;
            if (TheGcodeParser::getNumParts(c) < 0) {
                AMBRO_PGM_P err = AMBRO_PSTR("unknown error");
                switch (TheGcodeParser::getNumParts(c)) {
                    case TheGcodeParser::ERROR_NO_PARTS: err = AMBRO_PSTR("empty command"); break;
                    case TheGcodeParser::ERROR_TOO_MANY_PARTS: err = AMBRO_PSTR("too many parts"); break;
                    case TheGcodeParser::ERROR_INVALID_PART: err = AMBRO_PSTR("invalid part"); break;
                    case TheGcodeParser::ERROR_CHECKSUM: err = AMBRO_PSTR("incorrect checksum"); break;
                    case TheGcodeParser::ERROR_RECV_OVERRUN: err = AMBRO_PSTR("receive buffer overrun"); break;
                }
                impl(c)->reply_append_pstr(c, AMBRO_PSTR("Error:"));
                impl(c)->reply_append_pstr(c, err);
                impl(c)->reply_append_ch(c, '\n');
                return impl(c)->finishCommand(c);
            }
            if (!Channel::start_command_impl(c)) {
                return impl(c)->finishCommand(c);
            }
            work_command(c, impl(c));
        }
        
        static void maybePauseLockingCommand (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(!o->m_cmd || o->m_state == COMMAND_LOCKING)
            AMBRO_ASSERT(o->m_cmd || o->m_state == COMMAND_IDLE)
            
            o->m_state = COMMAND_IDLE;
        }
        
        static bool maybeResumeLockingCommand (Context c)
        {
            auto *o = Object::self(c);
            auto *mob = PrinterMain::Object::self(c);
            AMBRO_ASSERT(o->m_state == COMMAND_IDLE)
            
            if (!o->m_cmd) {
                return false;
            }
            o->m_state = COMMAND_LOCKING;
            if (!mob->unlocked_timer.isSet(c)) {
                mob->unlocked_timer.prependNowNotAlready(c);
            }
            return true;
        }
        
        static void maybeCancelLockingCommand (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->m_state != COMMAND_LOCKED)
            
            o->m_state = COMMAND_IDLE;
            o->m_cmd = false;
        }
        
        static void reportSendBufEventDirectly (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->m_state == COMMAND_LOCKED)
            AMBRO_ASSERT(o->m_send_buf_event_handler)
            
            auto handler = o->m_send_buf_event_handler;
            o->m_send_buf_event_handler = NULL;
            
            return handler(c);
        }
        
        template <int State>
        static bool get_command_in_state_helper (Context c, WrapInt<State>, TheCommand **out_cmd)
        {
            auto *o = Object::self(c);
            
            if (o->m_state == State) {
                *out_cmd = impl(c);
                return false;
            }
            return true;
        }
        
        struct CommandImpl : public TheCommand {
            void finishCommand (Context c, bool no_ok = false)
            {
                auto *o = Object::self(c);
                auto *mob = PrinterMain::Object::self(c);
                AMBRO_ASSERT(o->m_cmd)
                AMBRO_ASSERT(o->m_state == COMMAND_IDLE || o->m_state == COMMAND_LOCKED)
                AMBRO_ASSERT(!o->m_send_buf_event_handler)
                
                Channel::finish_command_impl(c, no_ok);
                o->m_cmd = false;
                if (o->m_state == COMMAND_LOCKED) {
                    AMBRO_ASSERT(mob->locked)
                    o->m_state = COMMAND_IDLE;
                    unlock(c);
                }
            }
            
            bool tryLockedCommand (Context c)
            {
                auto *o = Object::self(c);
                auto *mob = PrinterMain::Object::self(c);
                AMBRO_ASSERT(o->m_state != COMMAND_LOCKING || !mob->locked)
                AMBRO_ASSERT(o->m_state != COMMAND_LOCKED || mob->locked)
                AMBRO_ASSERT(o->m_cmd)
                
                if (o->m_state == COMMAND_LOCKED) {
                    return true;
                }
                if (mob->locked) {
                    o->m_state = COMMAND_LOCKING;
                    return false;
                }
                o->m_state = COMMAND_LOCKED;
                lock(c);
                return true;
            }
            
            bool tryUnplannedCommand (Context c)
            {
                auto *mob = PrinterMain::Object::self(c);
                
                if (!tryLockedCommand(c)) {
                    return false;
                }
                AMBRO_ASSERT(mob->planner_state == PLANNER_NONE || mob->planner_state == PLANNER_RUNNING)
                if (mob->planner_state == PLANNER_NONE) {
                    return true;
                }
                mob->planner_state = PLANNER_STOPPING;
                if (mob->m_planning_pull_pending) {
                    ThePlanner::waitFinished(c);
                    mob->force_timer.unset(c);
                }
                return false;
            }
            
            bool tryPlannedCommand (Context c)
            {
                auto *mob = PrinterMain::Object::self(c);
                
                if (!tryLockedCommand(c)) {
                    return false;
                }
                AMBRO_ASSERT(mob->planner_state == PLANNER_NONE || mob->planner_state == PLANNER_RUNNING)
                if (mob->planner_state == PLANNER_NONE) {
                    ThePlanner::init(c, false);
                    mob->planner_state = PLANNER_RUNNING;
                    mob->m_planning_pull_pending = false;
                    now_active(c);
                }
                if (mob->m_planning_pull_pending) {
                    return true;
                }
                mob->planner_state = PLANNER_WAITING;
                return false;
            }
            
            bool trySplitClearCommand (Context c)
            {
                if (!tryLockedCommand(c)) {
                    return false;
                }
                return TransformFeature::try_splitclear_command(c);
            }
            
            char getCmdCode (Context c)
            {
                return TheGcodeParser::getCmdCode(c);
            }
            
            uint16_t getCmdNumber (Context c)
            {
                return TheGcodeParser::getCmdNumber(c);
            }
            
            typename TheCommand::PartsSizeType getNumParts (Context c)
            {
                return TheGcodeParser::getNumParts(c);
            }
            
            CommandPartRef getPart (Context c, typename TheCommand::PartsSizeType i)
            {
                return CommandPartRef{TheGcodeParser::getPart(c, i)};
            }
            
            char getPartCode (Context c, CommandPartRef part)
            {
                return TheGcodeParser::getPartCode(c, (GcodeParserPartRef)part.ptr);
            }
            
            FpType getPartFpValue (Context c, CommandPartRef part)
            {
                return TheGcodeParser::template getPartFpValue<FpType>(c, (GcodeParserPartRef)part.ptr);
            }
            
            uint32_t getPartUint32Value (Context c, CommandPartRef part)
            {
                return TheGcodeParser::getPartUint32Value(c, (GcodeParserPartRef)part.ptr);
            }
            
            char const * getPartStringValue (Context c, CommandPartRef part)
            {
                return TheGcodeParser::getPartStringValue(c, (GcodeParserPartRef)part.ptr);
            }
            
            void reply_poke (Context c)
            {
                Channel::reply_poke_impl(c);
            }
            
            void reply_append_buffer (Context c, char const *str, uint8_t length)
            {
                Channel::reply_append_buffer_impl(c, str, length);
            }
            
            void reply_append_ch (Context c, char ch)
            {
                Channel::reply_append_ch_impl(c, ch);
            }
            
            void reply_append_pbuffer (Context c, AMBRO_PGM_P pstr, uint8_t length)
            {
                Channel::reply_append_pbuffer_impl(c, pstr, length);
            }
            
            bool requestSendBufEvent (Context c, size_t length, typename TheCommand::SendBufEventHandler handler)
            {
                auto *o = Object::self(c);
                AMBRO_ASSERT(o->m_state == COMMAND_LOCKED)
                AMBRO_ASSERT(!o->m_send_buf_event_handler)
                AMBRO_ASSERT(length > 0)
                AMBRO_ASSERT(handler)
                
                if (!Channel::request_send_buf_event_impl(c, length)) {
                    return false;
                }
                o->m_send_buf_event_handler = handler;
                return true;
            }
            
            void cancelSendBufEvent (Context c)
            {
                auto *o = Object::self(c);
                AMBRO_ASSERT(o->m_state == COMMAND_LOCKED)
                AMBRO_ASSERT(o->m_send_buf_event_handler)
                
                Channel::cancel_send_buf_event_impl(c);
                o->m_send_buf_event_handler = NULL;
            }
        };
        
        struct Object : public ObjBase<ChannelCommon, ChannelParentObject, EmptyTypeList> {
            CommandImpl m_cmd_impl;
            uint8_t m_state;
            bool m_cmd;
            typename TheCommand::SendBufEventHandler m_send_buf_event_handler;
        };
    };
    
    struct SerialFeature {
        struct Object;
        struct SerialRecvHandler;
        struct SerialSendHandler;
        
        static size_t const MaxFinishLen = sizeof(PRINTERMAIN_SERIAL_OK_STR) - 1;
        
        using TheSerial = typename Params::Serial::SerialService::template Serial<Context, Object, Params::Serial::RecvBufferSizeExp, Params::Serial::SendBufferSizeExp, SerialRecvHandler, SerialSendHandler>;
        using RecvSizeType = typename TheSerial::RecvSizeType;
        using SendSizeType = typename TheSerial::SendSizeType;
        using TheGcodeParser = GcodeParser<Context, Object, typename Params::Serial::TheGcodeParserParams, typename RecvSizeType::IntType, GcodeParserTypeSerial>;
        using TheChannelCommon = ChannelCommon<Object, SerialFeature>;
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            TheSerial::init(c, Params::Serial::Baud);
            TheGcodeParser::init(c);
            TheChannelCommon::init(c);
            o->m_recv_next_error = 0;
            o->m_line_number = 1;
        }
        
        static void deinit (Context c)
        {
            TheGcodeParser::deinit(c);
            TheSerial::deinit(c);
        }
        
        static void serial_recv_handler (Context c)
        {
            auto *o = Object::self(c);
            auto *cco = TheChannelCommon::Object::self(c);
            
            if (cco->m_cmd) {
                return;
            }
            if (!TheGcodeParser::haveCommand(c)) {
                TheGcodeParser::startCommand(c, TheSerial::recvGetChunkPtr(c), o->m_recv_next_error);
                o->m_recv_next_error = 0;
            }
            bool overrun;
            RecvSizeType avail = TheSerial::recvQuery(c, &overrun);
            if (TheGcodeParser::extendCommand(c, avail.value())) {
                return TheChannelCommon::startCommand(c);
            }
            if (overrun) {
                TheSerial::recvConsume(c, avail);
                TheSerial::recvClearOverrun(c);
                TheGcodeParser::resetCommand(c);
                o->m_recv_next_error = TheGcodeParser::ERROR_RECV_OVERRUN;
            }
        }
        
        static void serial_send_handler (Context c)
        {
            TheChannelCommon::reportSendBufEventDirectly(c);
        }
        
        static bool start_command_impl (Context c)
        {
            auto *o = Object::self(c);
            auto *cco = TheChannelCommon::Object::self(c);
            AMBRO_ASSERT(cco->m_cmd)
            
            bool is_m110 = (TheGcodeParser::getCmdCode(c) == 'M' && TheGcodeParser::getCmdNumber(c) == 110);
            if (is_m110) {
                o->m_line_number = TheChannelCommon::impl(c)->get_command_param_uint32(c, 'L', (TheGcodeParser::getCmd(c)->have_line_number ? TheGcodeParser::getCmd(c)->line_number : -1));
            }
            if (TheGcodeParser::getCmd(c)->have_line_number) {
                if (TheGcodeParser::getCmd(c)->line_number != o->m_line_number) {
                    TheChannelCommon::impl(c)->reply_append_pstr(c, AMBRO_PSTR("Error:Line Number is not Last Line Number+1, Last Line:"));
                    TheChannelCommon::impl(c)->reply_append_uint32(c, (uint32_t)(o->m_line_number - 1));
                    TheChannelCommon::impl(c)->reply_append_ch(c, '\n');
                    return false;
                }
            }
            if (TheGcodeParser::getCmd(c)->have_line_number || is_m110) {
                o->m_line_number++;
            }
            return true;
        }
        
        static void finish_command_impl (Context c, bool no_ok)
        {
            auto *o = Object::self(c);
            auto *cco = TheChannelCommon::Object::self(c);
            AMBRO_ASSERT(cco->m_cmd)
            
            if (!no_ok) {
                TheChannelCommon::impl(c)->reply_append_pstr(c, AMBRO_PSTR(PRINTERMAIN_SERIAL_OK_STR));
            }
            TheSerial::sendPoke(c);
            TheSerial::recvConsume(c, RecvSizeType::import(TheGcodeParser::getLength(c)));
            TheSerial::recvForceEvent(c);
        }
        
        static void reply_poke_impl (Context c)
        {
            TheSerial::sendPoke(c);
        }
        
        static void reply_append_buffer_impl (Context c, char const *str, uint8_t length)
        {
            SendSizeType avail = TheSerial::sendQuery(c);
            if (length > avail.value()) {
                length = avail.value();
            }
            while (length > 0) {
                char *chunk_data = TheSerial::sendGetChunkPtr(c);
                uint8_t chunk_length = TheSerial::sendGetChunkLen(c, SendSizeType::import(length)).value();
                memcpy(chunk_data, str, chunk_length);
                TheSerial::sendProvide(c, SendSizeType::import(chunk_length));
                str += chunk_length;
                length -= chunk_length;
            }
        }
        
        static void reply_append_pbuffer_impl (Context c, AMBRO_PGM_P pstr, uint8_t length)
        {
            SendSizeType avail = TheSerial::sendQuery(c);
            if (length > avail.value()) {
                length = avail.value();
            }
            while (length > 0) {
                char *chunk_data = TheSerial::sendGetChunkPtr(c);
                uint8_t chunk_length = TheSerial::sendGetChunkLen(c, SendSizeType::import(length)).value();
                AMBRO_PGM_MEMCPY(chunk_data, pstr, chunk_length);
                TheSerial::sendProvide(c, SendSizeType::import(chunk_length));
                pstr += chunk_length;
                length -= chunk_length;
            }
        }
        
        static void reply_append_ch_impl (Context c, char ch)
        {
            if (TheSerial::sendQuery(c).value() > 0) {
                *TheSerial::sendGetChunkPtr(c) = ch;
                TheSerial::sendProvide(c, SendSizeType::import(1));
            }
        }
        
        static bool request_send_buf_event_impl (Context c, size_t length)
        {
            if (length > SendSizeType::maxIntValue() - MaxFinishLen) {
                return false;
            }
            TheSerial::sendRequestEvent(c, SendSizeType::import(length + MaxFinishLen));
            return true;
        }
        
        static void cancel_send_buf_event_impl (Context c)
        {
            TheSerial::sendRequestEvent(c, SendSizeType::import(0));
        }
        
        struct SerialRecvHandler : public AMBRO_WFUNC_TD(&SerialFeature::serial_recv_handler) {};
        struct SerialSendHandler : public AMBRO_WFUNC_TD(&SerialFeature::serial_send_handler) {};
        
        struct Object : public ObjBase<SerialFeature, typename PrinterMain::Object, MakeTypeList<
            TheSerial,
            TheGcodeParser,
            TheChannelCommon
        >> {
            int8_t m_recv_next_error;
            uint32_t m_line_number;
        };
    };
    
    AMBRO_STRUCT_IF(SdCardFeature, Params::SdCardParams::Enabled) {
        struct Object;
        struct InputActivateHandler;
        struct InputReadHandler;
        
        using TheInput = typename Params::SdCardParams::InputService::template Input<Context, Object, InputClientParams<InputActivateHandler, InputReadHandler>>;
        static const size_t BufferBaseSize = Params::SdCardParams::BufferBaseSize;
        static const size_t MaxCommandSize = Params::SdCardParams::MaxCommandSize;
        static_assert(MaxCommandSize > 0, "");
        static_assert(BufferBaseSize >= TheInput::NeedBufAvail + (MaxCommandSize - 1), "");
        static const size_t WrapExtraSize = MaxCommandSize - 1;
        using ParserSizeType = ChooseInt<BitsInInt<MaxCommandSize>::Value, false>;
        using TheGcodeParser = typename Params::SdCardParams::template GcodeParserTemplate<Context, Object, typename Params::SdCardParams::TheGcodeParserParams, ParserSizeType>;
        using TheChannelCommon = ChannelCommon<Object, SdCardFeature>;
        enum {SDCARD_NONE, SDCARD_INITING, SDCARD_INITED, SDCARD_RUNNING, SDCARD_PAUSING};
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            TheInput::init(c);
            TheChannelCommon::init(c);
            o->m_next_event.init(c, SdCardFeature::next_event_handler);
            o->m_state = SDCARD_NONE;
        }
        
        static void deinit (Context c)
        {
            auto *o = Object::self(c);
            if (o->m_state != SDCARD_NONE && o->m_state != SDCARD_INITING) {
                TheGcodeParser::deinit(c);
            }
            o->m_next_event.deinit(c);
            TheInput::deinit(c);
        }
        
        static void input_activate_handler (Context c, uint8_t error_code)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->m_state == SDCARD_INITING)
            
            if (error_code) {
                o->m_state = SDCARD_NONE;
            } else {
                o->m_state = SDCARD_INITED;
                TheGcodeParser::init(c);
                o->m_start = 0;
                o->m_length = 0;
            }
            
            TheCommand *cmd = get_locked(c);
            if (error_code) {
                cmd->reply_append_pstr(c, AMBRO_PSTR("SD error "));
                cmd->reply_append_uint8(c, error_code);
            } else {
                cmd->reply_append_pstr(c, AMBRO_PSTR("SD inited"));
            }
            cmd->reply_append_ch(c, '\n');
            cmd->finishCommand(c);
        }
        struct InputActivateHandler : public AMBRO_WFUNC_TD(&SdCardFeature::input_activate_handler) {};
        
        static void input_read_handler (Context c, bool error, size_t bytes_read)
        {
            auto *o = Object::self(c);
            auto *co = TheChannelCommon::Object::self(c);
            AMBRO_ASSERT(o->m_state == SDCARD_RUNNING || o->m_state == SDCARD_PAUSING)
            buf_sanity(c);
            AMBRO_ASSERT(o->m_reading)
            
            o->m_reading = false;
            if (!error) {
                size_t write_offset = buf_add(o->m_start, o->m_length);
                if (write_offset < WrapExtraSize) {
                    memcpy(o->m_buffer + BufferBaseSize + write_offset, o->m_buffer + write_offset, MinValue(bytes_read, WrapExtraSize - write_offset));
                }
                if (bytes_read > BufferBaseSize - write_offset) {
                    memcpy(o->m_buffer + BufferBaseSize, o->m_buffer, MinValue(bytes_read - (BufferBaseSize - write_offset), WrapExtraSize));
                }
                o->m_length += bytes_read;
            }
            if (o->m_state == SDCARD_PAUSING) {
                o->m_state = SDCARD_INITED;
                return finish_locked(c);
            }
            if (error) {
                SerialFeature::TheChannelCommon::impl(c)->reply_append_pstr(c, AMBRO_PSTR("//SdRdEr\n"));
                SerialFeature::TheChannelCommon::impl(c)->reply_poke(c);
                return start_read(c);
            }
            if (can_read(c)) {
                start_read(c);
            }
            if (!co->m_cmd && !o->m_eof) {
                o->m_next_event.prependNowNotAlready(c);
            }
        }
        struct InputReadHandler : public AMBRO_WFUNC_TD(&SdCardFeature::input_read_handler) {};
        
        static void next_event_handler (typename Loop::QueuedEvent *, Context c)
        {
            auto *o = Object::self(c);
            auto *co = TheChannelCommon::Object::self(c);
            AMBRO_ASSERT(o->m_state == SDCARD_RUNNING)
            buf_sanity(c);
            AMBRO_ASSERT(!co->m_cmd)
            AMBRO_ASSERT(!o->m_eof)
            
            AMBRO_PGM_P eof_str;
            if (!TheGcodeParser::haveCommand(c)) {
                TheGcodeParser::startCommand(c, (char *)(o->m_buffer + o->m_start), 0);
            }
            ParserSizeType avail = MinValue(MaxCommandSize, o->m_length);
            if (TheGcodeParser::extendCommand(c, avail)) {
                if (TheGcodeParser::getNumParts(c) == TheGcodeParser::ERROR_EOF) {
                    eof_str = AMBRO_PSTR("//SdEof\n");
                    goto eof;
                }
                return TheChannelCommon::startCommand(c);
            }
            if (avail == MaxCommandSize) {
                eof_str = AMBRO_PSTR("//SdLnEr\n");
                goto eof;
            }
            if (TheInput::eofReached(c)) {
                eof_str = AMBRO_PSTR("//SdEnd\n");
                goto eof;
            }
            return;
        eof:
            SerialFeature::TheChannelCommon::impl(c)->reply_append_pstr(c, eof_str);
            SerialFeature::TheChannelCommon::impl(c)->reply_poke(c);
            o->m_eof = true;
        }
        
        static bool check_command (Context c, TheCommand *cmd)
        {
            auto *o = Object::self(c);
            
            if (cmd == TheChannelCommon::impl(c)) {
                return true;
            }
            if (cmd->getCmdNumber(c) == 21) {
                if (!cmd->tryUnplannedCommand(c)) {
                    return false;
                }
                if (o->m_state != SDCARD_NONE) {
                    cmd->finishCommand(c);
                    return false;
                }
                TheInput::activate(c);
                o->m_state = SDCARD_INITING;
                return false;
            }
            if (cmd->getCmdNumber(c) == 22) {
                if (!cmd->tryUnplannedCommand(c)) {
                    return false;
                }
                cmd->finishCommand(c);
                AMBRO_ASSERT(o->m_state != SDCARD_INITING)
                AMBRO_ASSERT(o->m_state != SDCARD_PAUSING)
                if (o->m_state == SDCARD_NONE) {
                    return false;
                }
                TheGcodeParser::deinit(c);
                o->m_state = SDCARD_NONE;
                o->m_next_event.unset(c);
                TheChannelCommon::maybeCancelLockingCommand(c);
                TheInput::deactivate(c);
                return false;
            }
            if (cmd->getCmdNumber(c) == 24) {
                if (!cmd->tryUnplannedCommand(c)) {
                    return false;
                }
                cmd->finishCommand(c);
                if (o->m_state != SDCARD_INITED) {
                    return false;
                }
                o->m_state = SDCARD_RUNNING;
                o->m_eof = false;
                o->m_reading = false;
                if (can_read(c)) {
                    start_read(c);
                }
                if (!TheChannelCommon::maybeResumeLockingCommand(c)) {
                    o->m_next_event.prependNowNotAlready(c);
                }
                return false;
            }
            if (cmd->getCmdNumber(c) == 25) {
                if (!cmd->tryUnplannedCommand(c)) {
                    return false;
                }
                if (o->m_state != SDCARD_RUNNING) {
                    cmd->finishCommand(c);
                    return false;
                }
                o->m_next_event.unset(c);
                TheChannelCommon::maybePauseLockingCommand(c);
                if (o->m_reading) {
                    o->m_state = SDCARD_PAUSING;
                } else {
                    o->m_state = SDCARD_INITED;
                    cmd->finishCommand(c);
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
            auto *o = Object::self(c);
            auto *co = TheChannelCommon::Object::self(c);
            AMBRO_ASSERT(co->m_cmd)
            AMBRO_ASSERT(o->m_state == SDCARD_RUNNING)
            buf_sanity(c);
            AMBRO_ASSERT(!o->m_eof)
            AMBRO_ASSERT(TheGcodeParser::getLength(c) <= o->m_length)
            
            size_t cmd_len = TheGcodeParser::getLength(c);
            o->m_next_event.prependNowNotAlready(c);
            o->m_start = buf_add(o->m_start, cmd_len);
            o->m_length -= cmd_len;
            if (!o->m_reading && can_read(c)) {
                start_read(c);
            }
        }
        
        static void reply_poke_impl (Context c)
        {
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
        
        static bool request_send_buf_event_impl (Context c, size_t length)
        {
            return false;
        }
        
        static void cancel_send_buf_event_impl (Context c)
        {
        }
        
        static bool can_read (Context c)
        {
            auto *o = Object::self(c);
            return TheInput::canRead(c, BufferBaseSize - o->m_length);
        }
        
        static size_t buf_add (size_t start, size_t count)
        {
            static_assert(BufferBaseSize <= SIZE_MAX / 2, "");
            size_t x = start + count;
            if (x >= BufferBaseSize) {
                x -= BufferBaseSize;
            }
            return x;
        }
        
        static void start_read (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(!o->m_reading)
            AMBRO_ASSERT(can_read(c))
            
            o->m_reading = true;
            size_t write_offset = buf_add(o->m_start, o->m_length);
            TheInput::startRead(c, BufferBaseSize - o->m_length, BufferBaseSize - write_offset, o->m_buffer + write_offset, o->m_buffer);
        }
        
        static void buf_sanity (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->m_start < BufferBaseSize)
            AMBRO_ASSERT(o->m_length <= BufferBaseSize)
        }
        
        using SdChannelCommonList = MakeTypeList<TheChannelCommon>;
        
        struct Object : public ObjBase<SdCardFeature, typename PrinterMain::Object, MakeTypeList<
            TheInput,
            TheChannelCommon,
            TheGcodeParser
        >> {
            typename Loop::QueuedEvent m_next_event;
            uint8_t m_state;
            size_t m_start;
            size_t m_length;
            bool m_eof;
            bool m_reading;
            uint8_t m_buffer[BufferBaseSize + WrapExtraSize];
        };
    } AMBRO_STRUCT_ELSE(SdCardFeature) {
        static void init (Context c) {}
        static void deinit (Context c) {}
        static bool check_command (Context c, TheCommand *cmd) { return true; }
        using SdChannelCommonList = EmptyTypeList;
        struct Object {};
    };
    
    using ChannelCommonList = JoinTypeLists<
        MakeTypeList<typename SerialFeature::TheChannelCommon>,
        typename SdCardFeature::SdChannelCommonList
    >;
    
    template <int TAxisIndex>
    struct Axis {
        struct Object;
        static const int AxisIndex = TAxisIndex;
        using AxisSpec = TypeListGet<ParamsAxesList, AxisIndex>;
        using Stepper = typename TheSteppers::template Stepper<AxisIndex>;
        using TheAxisDriver = typename AxisSpec::TheAxisDriverService::template AxisDriver<Context, Object, Stepper, AxisDriverConsumersList<AxisIndex>>;
        using StepFixedType = FixedPoint<AxisSpec::StepBits, false, 0>;
        using AbsStepFixedType = FixedPoint<AxisSpec::StepBits - 1, true, 0>;
        static const char AxisName = AxisSpec::Name;
        using WrappedAxisName = WrapInt<AxisName>;
        using HomingSpec = typename AxisSpec::Homing;
        
        using DistConversion = decltype(Config::e(AxisSpec::DefaultStepsPerUnit::i));
        using SpeedConversion = decltype(Config::e(AxisSpec::DefaultStepsPerUnit::i) / TimeConversion());
        using AccelConversion = decltype(Config::e(AxisSpec::DefaultStepsPerUnit::i) / (TimeConversion() * TimeConversion()));
        
        using AbsStepFixedTypeMin = APRINTER_FP_CONST_EXPR(AbsStepFixedType::minValue().fpValueConstexpr());
        using AbsStepFixedTypeMax = APRINTER_FP_CONST_EXPR(AbsStepFixedType::maxValue().fpValueConstexpr());
        
        using MinReqPos = decltype(ExprFmax(Config::e(AxisSpec::DefaultMin::i), AbsStepFixedTypeMin() / DistConversion()));
        using MaxReqPos = decltype(ExprFmin(Config::e(AxisSpec::DefaultMax::i), AbsStepFixedTypeMax() / DistConversion()));
        
        using PlannerMaxSpeedRec = decltype(ExprRec(Config::e(AxisSpec::DefaultMaxSpeed::i) * SpeedConversion()));
        using PlannerMaxAccelRec = decltype(ExprRec(Config::e(AxisSpec::DefaultMaxAccel::i) * AccelConversion()));
        
        template <typename ThePrinterMain = PrinterMain>
        struct Lazy {
            static typename ThePrinterMain::PhysVirtAxisMaskType const AxisMask = (typename ThePrinterMain::PhysVirtAxisMaskType)1 << AxisIndex;
        };
        
        AMBRO_STRUCT_IF(HomingFeature, HomingSpec::Enabled) {
            struct Object;
            
            using HomerInstance = typename HomingSpec::HomerService::template Instance<
                Context, Config, FpType, AxisSpec::StepBits, Params::StepperSegmentBufferSize,
                Params::LookaheadBufferSize, decltype(Config::e(AxisSpec::DefaultMaxAccel::i)),
                DistConversion, TimeConversion, decltype(Config::e(HomingSpec::HomeDir::i))
            >;
            
            using HomerGlobal = typename HomerInstance::template HomerGlobal<Object>;
            
            struct HomingState {
                struct Object;
                struct HomerFinishedHandler;
                
                using Homer = typename HomerInstance::template Homer<Object, HomerGlobal, TheAxisDriver, HomerFinishedHandler>;
                
                static void homer_finished_handler (Context c, bool success)
                {
                    auto *hf = HomingFeature::Object::self(c);
                    auto *axis = Axis::Object::self(c);
                    auto *mob = PrinterMain::Object::self(c);
                    AMBRO_ASSERT(hf->homing)
                    AMBRO_ASSERT(mob->locked)
                    AMBRO_ASSERT(mob->m_homing_rem_axes & Lazy<>::AxisMask)
                    
                    Homer::deinit(c);
                    axis->m_req_pos = APRINTER_CFG(Config, CInitPosition, c);
                    axis->m_end_pos = AbsStepFixedType::importFpSaturatedRound(axis->m_req_pos * APRINTER_CFG(Config, CDistConversion, c));
                    hf->homing = false;
                    TransformFeature::template mark_phys_moved<AxisIndex>(c);
                    mob->m_homing_rem_axes &= ~Lazy<>::AxisMask;
                    if (!(mob->m_homing_rem_axes & PhysAxisMask)) {
                        phys_homing_finished(c);
                    }
                }
                
                struct HomerFinishedHandler : public AMBRO_WFUNC_TD(&HomingState::homer_finished_handler) {};
                
                struct Object : public ObjBase<HomingState, typename PlannerUnionHoming::Object, MakeTypeList<
                    Homer
                >> {};
            };
            
            using AxisDriverConsumersList = MakeTypeList<typename HomingState::Homer::TheAxisDriverConsumer>;
            
            static void init (Context c)
            {
                auto *o = Object::self(c);
                HomerGlobal::init(c);
                o->homing = false;
            }
            
            static void deinit (Context c)
            {
                auto *o = Object::self(c);
                if (o->homing) {
                    HomingState::Homer::deinit(c);
                }
            }
            
            static void start_phys_homing (Context c)
            {
                auto *o = Object::self(c);
                auto *mob = PrinterMain::Object::self(c);
                AMBRO_ASSERT(!o->homing)
                AMBRO_ASSERT(mob->m_homing_rem_axes & Lazy<>::AxisMask)
                
                Stepper::enable(c);
                HomingState::Homer::init(c);
                o->homing = true;
            }
            
            using InitPosition = decltype(ExprIf(Config::e(HomingSpec::HomeDir::i), MaxReqPos(), MinReqPos()));
            
            template <typename ThisContext>
            static bool endstop_is_triggered (ThisContext c)
            {
                return HomerGlobal::endstop_is_triggered(c);
            }
            
            struct Object : public ObjBase<HomingFeature, typename Axis::Object, MakeTypeList<
                HomerGlobal
            >> {
                bool homing;
            };
        } AMBRO_STRUCT_ELSE(HomingFeature) {
            struct HomingState { struct Object {}; };
            using AxisDriverConsumersList = EmptyTypeList;
            static void init (Context c) {}
            static void deinit (Context c) {}
            static void start_phys_homing (Context c) {}
            using InitPosition = APRINTER_FP_CONST_EXPR(0.0);
            template <typename ThisContext>
            static bool endstop_is_triggered (ThisContext c) { return false; }
            struct Object {};
        };
        
        using HomingState = typename HomingFeature::HomingState;
        
        AMBRO_STRUCT_IF(MicroStepFeature, AxisSpec::MicroStep::Enabled) {
            struct Object;
            using MicroStep = typename AxisSpec::MicroStep::template MicroStepTemplate<Context, Object, typename AxisSpec::MicroStep::MicroStepParams>;
            
            static void init (Context c)
            {
                MicroStep::init(c, AxisSpec::MicroStep::MicroSteps);
            }
            
            struct Object : public ObjBase<MicroStepFeature, typename Axis::Object, MakeTypeList<
                MicroStep
            >> {};
        } AMBRO_STRUCT_ELSE(MicroStepFeature) {
            static void init (Context c) {}
            struct Object {};
        };
        
        static FpType clamp_req_pos (Context c, FpType req)
        {
            return FloatMax(APRINTER_CFG(Config, CMinReqPos, c), FloatMin(APRINTER_CFG(Config, CMaxReqPos, c), req));
        }
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            TheAxisDriver::init(c);
            HomingFeature::init(c);
            MicroStepFeature::init(c);
            o->m_req_pos = APRINTER_CFG(Config, CInitPosition, c);
            o->m_end_pos = AbsStepFixedType::importFpSaturatedRound(o->m_req_pos * APRINTER_CFG(Config, CDistConversion, c));
            o->m_relative_positioning = false;
        }
        
        static void deinit (Context c)
        {
            HomingFeature::deinit(c);
            TheAxisDriver::deinit(c);
        }
        
        static void start_phys_homing (Context c)
        {
            HomingFeature::start_phys_homing(c);
        }
        
        static void enable_disable_stepper (Context c, bool enable)
        {
            if (enable) {
                Stepper::enable(c);
            } else {
                Stepper::disable(c);
            }
        }
        
        static void enable_disable_stepper_specific (Context c, bool enable, TheCommand *cmd, CommandPartRef part)
        {
            if (cmd->getPartCode(c, part) == AxisName) {
                enable_disable_stepper(c, enable);
            }
        }
        
        static void update_new_pos (Context c, FpType req)
        {
            auto *o = Object::self(c);
            auto *mo = PrinterMain::Object::self(c);
            o->m_req_pos = clamp_req_pos(c, req);
            if (AxisSpec::IsCartesian) {
                mo->move_seen_cartesian = true;
            }
            TransformFeature::template mark_phys_moved<AxisIndex>(c);
        }
        
        template <typename Src, typename AddDistance, typename PlannerCmd>
        AMBRO_OPTIMIZE_SPEED
        static void do_move (Context c, Src new_pos, AddDistance, FpType *distance_squared, FpType *total_steps, PlannerCmd *cmd)
        {
            auto *o = Object::self(c);
            AbsStepFixedType new_end_pos = AbsStepFixedType::importFpSaturatedRound(new_pos.template get<AxisIndex>() * APRINTER_CFG(Config, CDistConversion, c));
            bool dir = (new_end_pos >= o->m_end_pos);
            StepFixedType move = StepFixedType::importBits(dir ? 
                ((typename StepFixedType::IntType)new_end_pos.bitsValue() - (typename StepFixedType::IntType)o->m_end_pos.bitsValue()) :
                ((typename StepFixedType::IntType)o->m_end_pos.bitsValue() - (typename StepFixedType::IntType)new_end_pos.bitsValue())
            );
            if (AMBRO_UNLIKELY(move.bitsValue() != 0)) {
                if (AddDistance::Value && AxisSpec::IsCartesian) {
                    FpType delta = move.template fpValue<FpType>() * APRINTER_CFG(Config, CDistConversionRec, c);
                    *distance_squared += delta * delta;
                }
                *total_steps += move.template fpValue<FpType>();
                Stepper::enable(c);
            }
            auto *mycmd = TupleGetElem<AxisIndex>(cmd->axes.axes());
            mycmd->dir = dir;
            mycmd->x = move;
            o->m_end_pos = new_end_pos;
        }
        
        template <typename PlannerCmd>
        static void limit_axis_move_speed (Context c, FpType time_freq_by_max_speed, PlannerCmd *cmd)
        {
            auto *mycmd = TupleGetElem<AxisIndex>(cmd->axes.axes());
            FpType max_v_rec = time_freq_by_max_speed * APRINTER_CFG(Config, CDistConversionRec, c);
            cmd->axes.rel_max_v_rec = FloatMax(cmd->axes.rel_max_v_rec, mycmd->x.template fpValue<FpType>() * max_v_rec);
        }
        
        static void fix_aborted_pos (Context c)
        {
            auto *o = Object::self(c);
            using RemStepsType = ChooseInt<AxisSpec::StepBits, true>;
            RemStepsType rem_steps = ThePlanner::template countAbortedRemSteps<AxisIndex, RemStepsType>(c);
            if (rem_steps != 0) {
                o->m_end_pos.m_bits.m_int -= rem_steps;
                o->m_req_pos = o->m_end_pos.template fpValue<FpType>() * APRINTER_CFG(Config, CDistConversionRec, c);
                TransformFeature::template mark_phys_moved<AxisIndex>(c);
            }
        }
        
        static void only_set_position (Context c, FpType value)
        {
            auto *o = Object::self(c);
            o->m_req_pos = clamp_req_pos(c, value);
            o->m_end_pos = AbsStepFixedType::importFpSaturatedRound(o->m_req_pos * APRINTER_CFG(Config, CDistConversion, c));
        }
        
        static void set_position (Context c, FpType value, bool *seen_virtual)
        {
            only_set_position(c, value);
            TransformFeature::template mark_phys_moved<AxisIndex>(c);
        }
        
        static void emergency ()
        {
            Stepper::emergency();
        }
        
        using CDistConversion = decltype(ExprCast<FpType>(DistConversion()));
        using CDistConversionRec = decltype(ExprCast<FpType>(ExprRec(DistConversion())));
        using CMinReqPos = decltype(ExprCast<FpType>(MinReqPos()));
        using CMaxReqPos = decltype(ExprCast<FpType>(MaxReqPos()));
        using CInitPosition = decltype(ExprCast<FpType>(HomingFeature::InitPosition::e()));
        
        using ConfigExprs = MakeTypeList<CDistConversion, CDistConversionRec, CMinReqPos, CMaxReqPos, CInitPosition>;
        
        struct Object : public ObjBase<Axis, typename PrinterMain::Object, MakeTypeList<
            TheAxisDriver,
            HomingFeature,
            MicroStepFeature
        >>
        {
            AbsStepFixedType m_end_pos;
            FpType m_req_pos;
            FpType m_old_pos;
            bool m_relative_positioning;
        };
    };
    
    using AxesList = IndexElemList<ParamsAxesList, Axis>;
    
    template <int AxisName>
    using FindAxis = TypeListIndexMapped<
        AxesList,
        GetMemberType_WrappedAxisName,
        WrapInt<AxisName>
    >;
    
    template <typename TheAxis>
    using MakePlannerAxisSpec = MotionPlannerAxisSpec<
        typename TheAxis::TheAxisDriver,
        TheAxis::AxisSpec::StepBits,
        decltype(Config::e(TheAxis::AxisSpec::DefaultDistanceFactor::i)),
        decltype(Config::e(TheAxis::AxisSpec::DefaultCorneringDistance::i)),
        typename TheAxis::PlannerMaxSpeedRec,
        typename TheAxis::PlannerMaxAccelRec,
        PlannerPrestepCallback<TheAxis::AxisIndex>
    >;
    
    template <int LaserIndex>
    struct Laser {
        struct Object;
        using LaserSpec = TypeListGet<ParamsLasersList, LaserIndex>;
        using ThePwm = typename LaserSpec::PwmService::template Pwm<Context, Object>;
        using TheDutyFormula = typename LaserSpec::DutyFormulaService::template DutyFormula<typename ThePwm::DutyCycleType, ThePwm::MaxDutyCycle>;
        
        using MaxPower = decltype(Config::e(LaserSpec::MaxPower::i));
        using LaserPower = decltype(Config::e(LaserSpec::LaserPower::i));
        using PlannerMaxSpeedRec = decltype(TimeConversion() / (MaxPower() / LaserPower()));
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            o->density = 0.0;
            ThePwm::init(c);
        }
        
        static void deinit (Context c)
        {
            ThePwm::deinit(c);
        }
        
        static bool collect_new_pos (Context c, TheCommand *cmd, CommandPartRef part)
        {
            auto *o = Object::self(c);
            char code = cmd->getPartCode(c, part);
            if (AMBRO_UNLIKELY(code == LaserSpec::Name)) {
                FpType energy = cmd->getPartFpValue(c, part);
                move_add_laser<LaserIndex>(c, energy);
                return false;
            }
            if (AMBRO_UNLIKELY(code== LaserSpec::DensityName)) {
                o->density = cmd->getPartFpValue(c, part);
                return false;
            }
            return true;
        }
        
        static void handle_automatic_energy (Context c, FpType distance, bool is_positioning_move)
        {
            auto *o = Object::self(c);
            if (!o->move_energy_specified && !is_positioning_move) {
                o->move_energy = FloatMakePosOrPosZero(o->density * distance);
            }
        }
        
        template <typename Src, typename PlannerCmd>
        static void write_planner_cmd (Context c, Src src, PlannerCmd *cmd)
        {
            auto *mycmd = TupleGetElem<LaserIndex>(cmd->axes.lasers());
            mycmd->x = src.template get<LaserIndex>() * APRINTER_CFG(Config, CLaserPowerRec, c);
        }
        
        static void begin_move (Context c)
        {
            auto *o = Object::self(c);
            o->move_energy = 0.0f;
            o->move_energy_specified = false;
        }
        
        static void emergency ()
        {
            ThePwm::emergencySetOff();
        }
        
        struct PowerInterface {
            using PowerFixedType = typename TheDutyFormula::PowerFixedType;
            
            template <typename ThisContext>
            static void setPower (ThisContext c, PowerFixedType power)
            {
                ThePwm::setDutyCycle(c, TheDutyFormula::powerToDuty(power));
            }
        };
        
        using CLaserPowerRec = decltype(ExprCast<FpType>(ExprRec(LaserPower())));
        
        using ConfigExprs = MakeTypeList<CLaserPowerRec>;
        
        struct Object : public ObjBase<Laser, typename PrinterMain::Object, MakeTypeList<
            ThePwm
        >> {
            FpType density;
            FpType move_energy;
            bool move_energy_specified;
        };
    };
    
    using LasersList = IndexElemList<ParamsLasersList, Laser>;
    
    template <typename TheLaser>
    using MakePlannerLaserSpec = MotionPlannerLaserSpec<
        typename TheLaser::LaserSpec::TheLaserDriverService,
        typename TheLaser::PowerInterface,
        typename TheLaser::PlannerMaxSpeedRec
    >;
    
    struct PlannerClient {
        virtual void pull_handler (Context c) = 0;
        virtual void finished_handler (Context c) = 0;
        virtual void aborted_handler (Context c) = 0;
    };
    
    AMBRO_STRUCT_IF(TransformFeature, TransformParams::Enabled) {
        struct Object;
        using ParamsVirtAxesList = typename TransformParams::VirtAxesList;
        using ParamsPhysAxesList = typename TransformParams::PhysAxesList;
        using TheTransformAlg = typename TransformParams::TransformService::template Transform<Context, Object, Config, FpType>;
        using TheSplitter = typename TheTransformAlg::Splitter;
        static int const NumVirtAxes = TheTransformAlg::NumAxes;
        static_assert(TypeListLength<ParamsVirtAxesList>::Value == NumVirtAxes, "");
        static_assert(TypeListLength<ParamsPhysAxesList>::Value == NumVirtAxes, "");
        
        struct PhysReqPosSrc {
            Context m_c;
            template <int Index>
            FpType get () { return Axis<VirtAxis<Index>::PhysAxisIndex>::Object::self(m_c)->m_req_pos; }
        };
        
        struct PhysReqPosDst {
            Context m_c;
            template <int Index>
            void set (FpType x) { Axis<VirtAxis<Index>::PhysAxisIndex>::Object::self(m_c)->m_req_pos = x; }
        };
        
        struct VirtReqPosSrc {
            Context m_c;
            template <int Index>
            FpType get () { return VirtAxis<Index>::Object::self(m_c)->m_req_pos; }
        };
        
        struct VirtReqPosDst {
            Context m_c;
            template <int Index>
            void set (FpType x) { VirtAxis<Index>::Object::self(m_c)->m_req_pos = x; }
        };
        
        struct ArraySrc {
            FpType const *m_arr;
            template <int Index>
            FpType get () { return m_arr[Index]; }
        };
        
        struct PhysArrayDst {
            FpType *m_arr;
            template <int Index>
            void set (FpType x) { m_arr[VirtAxis<Index>::PhysAxisIndex] = x; }
        };
        
        struct LaserSplitSrc {
            Context c;
            FpType frac;
            FpType prev_frac;
            
            template <int LaserIndex>
            FpType get ()
            {
                return (frac - prev_frac) * LaserSplit<LaserIndex>::Object::self(c)->energy;
            }
        };
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            ListForEachForward<VirtAxesList>(LForeach_init(), c);
            update_virt_from_phys(c);
            o->virt_update_pending = false;
            o->splitclear_pending = false;
            o->splitting = false;
        }
        
        static void update_virt_from_phys (Context c)
        {
            TheTransformAlg::physToVirt(c, PhysReqPosSrc{c}, VirtReqPosDst{c});
        }
        
        AMBRO_OPTIMIZE_SPEED
        static void handle_virt_move (Context c, FpType time_freq_by_max_speed, bool is_positioning_move)
        {
            auto *o = Object::self(c);
            auto *mob = PrinterMain::Object::self(c);
            AMBRO_ASSERT(mob->planner_state == PLANNER_RUNNING || mob->planner_state == PLANNER_CUSTOM)
            AMBRO_ASSERT(mob->m_planning_pull_pending)
            AMBRO_ASSERT(o->splitting)
            AMBRO_ASSERT(FloatIsPosOrPosZero(time_freq_by_max_speed))
            
            o->virt_update_pending = false;
            TheTransformAlg::virtToPhys(c, VirtReqPosSrc{c}, PhysReqPosDst{c});
            ListForEachForward<VirtAxesList>(LForeach_clamp_req_phys(), c);
            do_pending_virt_update(c);
            FpType distance_squared = 0.0f;
            ListForEachForward<VirtAxesList>(LForeach_prepare_split(), c, &distance_squared);
            ListForEachForward<SecondaryAxesList>(LForeach_prepare_split(), c, &distance_squared);
            FpType distance = FloatSqrt(distance_squared);
            ListForEachForward<LasersList>(LForeach_handle_automatic_energy(), c, distance, is_positioning_move);
            ListForEachForward<LaserSplitsList>(LForeach_prepare_split(), c);
            FpType base_max_v_rec = ListForEachForwardAccRes<VirtAxesList>(distance * time_freq_by_max_speed, LForeach_limit_virt_axis_speed(), c);
            FpType min_segments_by_distance = (FpType)(TransformParams::SegmentsPerSecond::value() * Clock::time_unit) * time_freq_by_max_speed;
            o->splitter.start(distance, base_max_v_rec, min_segments_by_distance);
            o->frac = 0.0;
            do_split(c);
        }
        
        template <int PhysAxisIndex>
        static void mark_phys_moved (Context c)
        {
            auto *o = Object::self(c);
            if (IsPhysAxisTransformPhys<WrapInt<PhysAxisIndex>>::Value) {
                o->virt_update_pending = true;
            }
        }
        
        static void do_pending_virt_update (Context c)
        {
            auto *o = Object::self(c);
            if (AMBRO_UNLIKELY(o->virt_update_pending)) {
                o->virt_update_pending = false;
                update_virt_from_phys(c);
            }
        }
        
        static bool is_splitting (Context c)
        {
            auto *o = Object::self(c);
            return o->splitting;
        }
        
        static void split_more (Context c)
        {
            auto *o = Object::self(c);
            auto *mob = PrinterMain::Object::self(c);
            AMBRO_ASSERT(o->splitting)
            AMBRO_ASSERT(mob->planner_state != PLANNER_NONE)
            AMBRO_ASSERT(mob->m_planning_pull_pending)
            
            do_split(c);
            if (!o->splitting && o->splitclear_pending) {
                AMBRO_ASSERT(mob->locked)
                AMBRO_ASSERT(mob->planner_state == PLANNER_RUNNING)
                o->splitclear_pending = false;
                TheCommand *cmd = get_locked(c);
                work_command(c, cmd);
            }
        }
        
        static bool try_splitclear_command (Context c)
        {
            auto *o = Object::self(c);
            auto *mob = PrinterMain::Object::self(c);
            AMBRO_ASSERT(mob->locked)
            AMBRO_ASSERT(!o->splitclear_pending)
            
            if (!o->splitting) {
                return true;
            }
            o->splitclear_pending = true;
            return false;
        }
        
        AMBRO_OPTIMIZE_SPEED
        static void do_split (Context c)
        {
            auto *o = Object::self(c);
            auto *mob = PrinterMain::Object::self(c);
            AMBRO_ASSERT(o->splitting)
            AMBRO_ASSERT(mob->planner_state != PLANNER_NONE)
            AMBRO_ASSERT(mob->m_planning_pull_pending)
            
            do {
                FpType prev_frac = o->frac;
                FpType rel_max_v_rec;
                FpType move_pos[NumAxes];
                if (o->splitter.pull(&rel_max_v_rec, &o->frac)) {
                    FpType virt_pos[NumVirtAxes];
                    ListForEachForward<VirtAxesList>(LForeach_compute_split(), c, o->frac, virt_pos);
                    TheTransformAlg::virtToPhys(c, ArraySrc{virt_pos}, PhysArrayDst{move_pos});
                    ListForEachForward<VirtAxesList>(LForeach_clamp_move_phys(), c, move_pos);
                    ListForEachForward<SecondaryAxesList>(LForeach_compute_split(), c, o->frac, move_pos);
                } else {
                    o->frac = 1.0;
                    o->splitting = false;
                    ListForEachForward<VirtAxesList>(LForeach_get_final_split(), c, move_pos);
                    ListForEachForward<SecondaryAxesList>(LForeach_get_final_split(), c, move_pos);
                }
                PlannerSplitBuffer *cmd = ThePlanner::getBuffer(c);
                FpType total_steps = 0.0f;
                ListForEachForward<AxesList>(LForeach_do_move(), c, ArraySrc{move_pos}, WrapBool<false>(), (FpType *)0, &total_steps, cmd);
                if (total_steps != 0.0f) {
                    ListForEachForward<LasersList>(LForeach_write_planner_cmd(), c, LaserSplitSrc{c, o->frac, prev_frac}, cmd);
                    cmd->axes.rel_max_v_rec = FloatMax(rel_max_v_rec, total_steps * APRINTER_CFG(Config, CStepSpeedLimitFactor, c));
                    ThePlanner::axesCommandDone(c);
                    goto submitted;
                }
            } while (o->splitting);
            
            ThePlanner::emptyDone(c);
        submitted:
            submitted_planner_command(c);
        }
        
        static void handle_set_position (Context c, bool seen_virtual)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(!o->splitting)
            
            if (seen_virtual) {
                o->virt_update_pending = false;
                TheTransformAlg::virtToPhys(c, VirtReqPosSrc{c}, PhysReqPosDst{c});
                ListForEachForward<VirtAxesList>(LForeach_finish_set_position(), c);
            }
            do_pending_virt_update(c);
        }
        
        static bool start_virt_homing (Context c)
        {
            return ListForEachForwardInterruptible<VirtAxesList>(LForeach_start_virt_homing(), c);
        }
        
        template <typename CallbackContext>
        static bool prestep_callback (CallbackContext c)
        {
            return !ListForEachForwardInterruptible<VirtAxesList>(LForeach_prestep_callback(), c);
        }
        
        template <int VirtAxisIndex>
        struct VirtAxis {
            struct Object;
            using VirtAxisParams = TypeListGet<ParamsVirtAxesList, VirtAxisIndex>;
            static int const AxisName = VirtAxisParams::Name;
            static int const PhysAxisIndex = FindAxis<TypeListGet<ParamsPhysAxesList, VirtAxisIndex>::Value>::Value;
            using ThePhysAxis = Axis<PhysAxisIndex>;
            static_assert(!ThePhysAxis::AxisSpec::IsCartesian, "");
            using WrappedPhysAxisIndex = WrapInt<PhysAxisIndex>;
            using HomingSpec = typename VirtAxisParams::VirtualHoming;
            
            template <typename ThePrinterMain = PrinterMain>
            struct Lazy {
                static typename ThePrinterMain::PhysVirtAxisMaskType const AxisMask = (typename ThePrinterMain::PhysVirtAxisMaskType)1 << (NumAxes + VirtAxisIndex);
            };
            
            static void init (Context c)
            {
                auto *o = Object::self(c);
                o->m_relative_positioning = false;
                HomingFeature::init(c);
            }
            
            static void update_new_pos (Context c, FpType req)
            {
                auto *o = Object::self(c);
                auto *t = TransformFeature::Object::self(c);
                o->m_req_pos = clamp_virt_pos(c, req);
                t->splitting = true;
            }
            
            static void clamp_req_phys (Context c)
            {
                auto *axis = ThePhysAxis::Object::self(c);
                auto *t = TransformFeature::Object::self(c);
                if (AMBRO_UNLIKELY(!(axis->m_req_pos <= APRINTER_CFG(Config, typename ThePhysAxis::CMaxReqPos, c)))) {
                    axis->m_req_pos = APRINTER_CFG(Config, typename ThePhysAxis::CMaxReqPos, c);
                    t->virt_update_pending = true;
                } else if (AMBRO_UNLIKELY(!(axis->m_req_pos >= APRINTER_CFG(Config, typename ThePhysAxis::CMinReqPos, c)))) {
                    axis->m_req_pos = APRINTER_CFG(Config, typename ThePhysAxis::CMinReqPos, c);
                    t->virt_update_pending = true;
                }
            }
            
            static void clamp_move_phys (Context c, FpType *move_pos)
            {
                move_pos[PhysAxisIndex] = ThePhysAxis::clamp_req_pos(c, move_pos[PhysAxisIndex]);
            }
            
            static void prepare_split (Context c, FpType *distance_squared)
            {
                auto *o = Object::self(c);
                o->m_delta = o->m_req_pos - o->m_old_pos;
                *distance_squared += o->m_delta * o->m_delta;
            }
            
            static void compute_split (Context c, FpType frac, FpType *virt_pos)
            {
                auto *o = Object::self(c);
                auto *t = TransformFeature::Object::self(c);
                virt_pos[VirtAxisIndex] = o->m_old_pos + (frac * o->m_delta);
            }
            
            static void get_final_split (Context c, FpType *move_pos)
            {
                auto *axis = ThePhysAxis::Object::self(c);
                move_pos[PhysAxisIndex] = axis->m_req_pos;
            }
            
            static void set_position (Context c, FpType value, bool *seen_virtual)
            {
                auto *o = Object::self(c);
                o->m_req_pos = clamp_virt_pos(c, value);
                *seen_virtual = true;
            }
            
            static void finish_set_position (Context c)
            {
                auto *axis = ThePhysAxis::Object::self(c);
                auto *t = TransformFeature::Object::self(c);
                FpType req = axis->m_req_pos;
                ThePhysAxis::only_set_position(c, req);
                if (axis->m_req_pos != req) {
                    t->virt_update_pending = true;
                }
            }
            
            static FpType limit_virt_axis_speed (FpType accum, Context c)
            {
                auto *o = Object::self(c);
                return FloatMax(accum, FloatAbs(o->m_delta) * APRINTER_CFG(Config, CMaxSpeedFactor, c));
            }
            
            static FpType clamp_virt_pos (Context c, FpType req)
            {
                return FloatMax(APRINTER_CFG(Config, CMinPos, c), FloatMin(APRINTER_CFG(Config, CMaxPos, c), req));
            }
            
            static bool start_virt_homing (Context c)
            {
                return HomingFeature::start_virt_homing(c);
            }
            
            template <typename CallbackContext>
            static bool prestep_callback (CallbackContext c)
            {
                return HomingFeature::prestep_callback(c);
            }
            
            static void start_phys_homing (Context c) {}
            
            AMBRO_STRUCT_IF(HomingFeature, HomingSpec::Enabled) {
                struct Object;
                
                static void init (Context c)
                {
                    Context::Pins::template setInput<typename HomingSpec::EndPin, typename HomingSpec::EndPinInputMode>(c);
                }
                
                template <typename ThisContext>
                static bool endstop_is_triggered (ThisContext c)
                {
                    return (Context::Pins::template get<typename HomingSpec::EndPin>(c) != APRINTER_CFG(Config, CEndInvert, c));
                }
                
                static bool start_virt_homing (Context c)
                {
                    auto *o = Object::self(c);
                    auto *mo = PrinterMain::Object::self(c);
                    
                    if (!(mo->m_homing_rem_axes & Lazy<>::AxisMask)) {
                        return true;
                    }
                    set_position(c, home_start_pos(c));
                    custom_planner_init(c, &o->planner_client, true);
                    o->state = 0;
                    o->command_sent = false;
                    return false;
                }
                
                template <typename CallbackContext>
                static bool prestep_callback (CallbackContext c)
                {
                    return !endstop_is_triggered(c);
                }
                
                static void set_position (Context c, FpType value)
                {
                    SetPositionState s;
                    set_position_begin(c, &s);
                    set_position_add_axis<(NumAxes + VirtAxisIndex)>(c, &s, value);
                    set_position_end(c, &s);
                }
                
                static FpType home_start_pos (Context c)
                {
                    return APRINTER_CFG(Config, CHomeDir, c) ? APRINTER_CFG(Config, CMinPos, c) : APRINTER_CFG(Config, CMaxPos, c);
                }
                
                static FpType home_end_pos (Context c)
                {
                    return APRINTER_CFG(Config, CHomeDir, c) ? APRINTER_CFG(Config, CMaxPos, c) : APRINTER_CFG(Config, CMinPos, c);
                }
                
                static FpType home_dir (Context c)
                {
                    return APRINTER_CFG(Config, CHomeDir, c) ? 1.0f : -1.0f;
                }
                
                struct VirtHomingPlannerClient : public PlannerClient {
                    void pull_handler (Context c)
                    {
                        auto *o = Object::self(c);
                        auto *axis = VirtAxis::Object::self(c);
                        
                        if (o->command_sent) {
                            return custom_planner_wait_finished(c);
                        }
                        move_begin(c);
                        FpType position;
                        FpType speed;
                        switch (o->state) {
                            case 0: {
                                position = home_end_pos(c) + home_dir(c) * APRINTER_CFG(Config, CFastExtraDist, c);
                                speed = APRINTER_CFG(Config, CFastSpeed, c);
                            } break;
                            case 1: {
                                position = home_end_pos(c) - home_dir(c) * APRINTER_CFG(Config, CRetractDist, c);
                                speed = APRINTER_CFG(Config, CRetractSpeed, c);
                            } break;
                            case 2: {
                                position = home_end_pos(c) + home_dir(c) * APRINTER_CFG(Config, CSlowExtraDist, c);
                                speed = APRINTER_CFG(Config, CSlowSpeed, c);
                            } break;
                        }
                        move_add_axis<(NumAxes + VirtAxisIndex)>(c, position);
                        move_end(c, (FpType)TimeConversion::value() / speed);
                        o->command_sent = true;
                    }
                    
                    void finished_handler (Context c)
                    {
                        auto *o = Object::self(c);
                        auto *mo = PrinterMain::Object::self(c);
                        AMBRO_ASSERT(o->state < 3)
                        AMBRO_ASSERT(o->command_sent)
                        
                        custom_planner_deinit(c);
                        if (o->state != 1) {
                            set_position(c, home_end_pos(c));
                        }
                        o->state++;
                        o->command_sent = false;
                        if (o->state < 3) {
                            return custom_planner_init(c, &o->planner_client, o->state == 2);
                        }
                        mo->m_homing_rem_axes &= ~Lazy<>::AxisMask;
                        work_virt_homing(c);
                    }
                    
                    void aborted_handler (Context c)
                    {
                        finished_handler(c);
                    }
                };
                
                using CEndInvert = decltype(ExprCast<bool>(Config::e(HomingSpec::EndInvert::i)));
                using CHomeDir = decltype(ExprCast<bool>(Config::e(HomingSpec::HomeDir::i)));
                using CFastExtraDist = decltype(ExprCast<FpType>(Config::e(HomingSpec::FastExtraDist::i)));
                using CRetractDist = decltype(ExprCast<FpType>(Config::e(HomingSpec::RetractDist::i)));
                using CSlowExtraDist = decltype(ExprCast<FpType>(Config::e(HomingSpec::SlowExtraDist::i)));
                using CFastSpeed = decltype(ExprCast<FpType>(Config::e(HomingSpec::FastSpeed::i)));
                using CRetractSpeed = decltype(ExprCast<FpType>(Config::e(HomingSpec::RetractSpeed::i)));
                using CSlowSpeed = decltype(ExprCast<FpType>(Config::e(HomingSpec::SlowSpeed::i)));
                
                using ConfigExprs = MakeTypeList<CEndInvert, CHomeDir, CFastExtraDist, CRetractDist, CSlowExtraDist, CFastSpeed, CRetractSpeed, CSlowSpeed>;
                
                struct Object : public ObjBase<HomingFeature, typename VirtAxis::Object, EmptyTypeList> {
                    VirtHomingPlannerClient planner_client;
                    uint8_t state;
                    bool command_sent;
                };
            } AMBRO_STRUCT_ELSE(HomingFeature) {
                static void init (Context c) {}
                template <typename ThisContext>
                static bool endstop_is_triggered (ThisContext c) { return false; }
                static bool start_virt_homing (Context c) { return true; }
                template <typename CallbackContext>
                static bool prestep_callback (CallbackContext c) { return true; }
                struct Object {};
            };
            
            using CMinPos = decltype(ExprCast<FpType>(Config::e(VirtAxisParams::MinPos::i)));
            using CMaxPos = decltype(ExprCast<FpType>(Config::e(VirtAxisParams::MaxPos::i)));
            using CMaxSpeedFactor = decltype(ExprCast<FpType>(TimeConversion() / Config::e(VirtAxisParams::MaxSpeed::i)));
            
            using ConfigExprs = MakeTypeList<CMinPos, CMaxPos, CMaxSpeedFactor>;
            
            struct Object : public ObjBase<VirtAxis, typename TransformFeature::Object, MakeTypeList<
                TheTransformAlg,
                HomingFeature
            >>
            {
                FpType m_req_pos;
                FpType m_old_pos;
                FpType m_delta;
                bool m_relative_positioning;
            };
        };
        
        using VirtAxesList = IndexElemList<ParamsVirtAxesList, VirtAxis>;
        
        template <typename PhysAxisIndex>
        using IsPhysAxisTransformPhys = WrapBool<TypeListFindMapped<
            VirtAxesList,
            GetMemberType_WrappedPhysAxisIndex,
            PhysAxisIndex
        >::Found>;
        
        using SecondaryAxisIndices = FilterTypeList<
            SequenceList<NumAxes>,
            ComposeFunctions<
                NotFunc,
                TemplateFunc<IsPhysAxisTransformPhys>
            >
        >;
        static int const NumSecondaryAxes = TypeListLength<SecondaryAxisIndices>::Value;
        
        template <int SecondaryAxisIndex>
        struct SecondaryAxis {
            static int const AxisIndex = TypeListGet<SecondaryAxisIndices, SecondaryAxisIndex>::Value;
            using TheAxis = Axis<AxisIndex>;
            
            static void prepare_split (Context c, FpType *distance_squared)
            {
                auto *axis = TheAxis::Object::self(c);
                if (TheAxis::AxisSpec::IsCartesian) {
                    FpType delta = axis->m_req_pos - axis->m_old_pos;
                    *distance_squared += delta * delta;
                }
            }
            
            static void compute_split (Context c, FpType frac, FpType *move_pos)
            {
                auto *axis = TheAxis::Object::self(c);
                auto *t = TransformFeature::Object::self(c);
                move_pos[AxisIndex] = axis->m_old_pos + (frac * (axis->m_req_pos - axis->m_old_pos));
            }
            
            static void get_final_split (Context c, FpType *move_pos)
            {
                auto *axis = TheAxis::Object::self(c);
                move_pos[AxisIndex] = axis->m_req_pos;
            }
        };
        
        using SecondaryAxesList = IndexElemList<SecondaryAxisIndices, SecondaryAxis>;
        
        template <int LaserIndex>
        struct LaserSplit {
            struct Object;
            
            static void prepare_split (Context c)
            {
                auto *o = Object::self(c);
                auto *laser = Laser<LaserIndex>::Object::self(c);
                o->energy = laser->move_energy;
            }
            
            struct Object : public ObjBase<LaserSplit, typename TransformFeature::Object, EmptyTypeList> {
                FpType energy;
            };
        };
        
        using LaserSplitsList = IndexElemList<ParamsLasersList, LaserSplit>;
        
        struct Object : public ObjBase<TransformFeature, typename PrinterMain::Object, JoinTypeLists<
            VirtAxesList,
            LaserSplitsList
        >> {
            bool virt_update_pending;
            bool splitclear_pending;
            bool splitting;
            FpType frac;
            TheSplitter splitter;
        };
    } AMBRO_STRUCT_ELSE(TransformFeature) {
        static int const NumVirtAxes = 0;
        static void init (Context c) {}
        static void handle_virt_move (Context c, FpType time_freq_by_max_speed, bool is_positioning_move) {}
        template <int PhysAxisIndex>
        static void mark_phys_moved (Context c) {}
        static void do_pending_virt_update (Context c) {}
        static bool is_splitting (Context c) { return false; }
        static void split_more (Context c) {}
        static bool try_splitclear_command (Context c) { return true; }
        static void handle_set_position (Context c, bool seen_virtual) {}
        static bool start_virt_homing (Context c) { return true; }
        template <typename CallbackContext>
        static bool prestep_callback (CallbackContext c) { return false; }
        struct Object {};
    };
    
    static int const NumPhysVirtAxes = NumAxes + TransformFeature::NumVirtAxes;
    using PhysVirtAxisMaskType = ChooseInt<NumPhysVirtAxes, false>;
    static PhysVirtAxisMaskType const PhysAxisMask = PowerOfTwoMinusOne<PhysVirtAxisMaskType, NumAxes>::Value;
    
    template <bool IsVirt, int PhysVirtAxisIndex>
    struct GetPhysVirtAxisHelper {
        using Type = Axis<PhysVirtAxisIndex>;
    };
    
    template <int PhysVirtAxisIndex>
    struct GetPhysVirtAxisHelper<true, PhysVirtAxisIndex> {
        using Type = typename TransformFeature::template VirtAxis<(PhysVirtAxisIndex - NumAxes)>;
    };
    
    template <int PhysVirtAxisIndex>
    using GetPhysVirtAxis = typename GetPhysVirtAxisHelper<(PhysVirtAxisIndex >= NumAxes), PhysVirtAxisIndex>::Type;
    
    template <int PhysVirtAxisIndex>
    struct PhysVirtAxisHelper {
        using TheAxis = GetPhysVirtAxis<PhysVirtAxisIndex>;
        using WrappedAxisName = WrapInt<TheAxis::AxisName>;
        static PhysVirtAxisMaskType const AxisMask = TheAxis::template Lazy<>::AxisMask;
        
        static void init_new_pos (Context c)
        {
            auto *axis = TheAxis::Object::self(c);
            axis->m_old_pos = axis->m_req_pos;
        }
        
        static void update_new_pos (Context c, FpType req)
        {
            TheAxis::update_new_pos(c, req);
        }
        
        static bool collect_new_pos (Context c, TheCommand *cmd, CommandPartRef part)
        {
            auto *axis = TheAxis::Object::self(c);
            if (AMBRO_UNLIKELY(cmd->getPartCode(c, part) == TheAxis::AxisName)) {
                FpType req = cmd->getPartFpValue(c, part);
                if (axis->m_relative_positioning) {
                    req += axis->m_old_pos;
                }
                update_new_pos(c, req);
                return false;
            }
            return true;
        }
        
        static void set_relative_positioning (Context c, bool relative)
        {
            auto *axis = TheAxis::Object::self(c);
            axis->m_relative_positioning = relative;
        }
        
        static void append_position (Context c, TheCommand *cmd)
        {
            auto *axis = TheAxis::Object::self(c);
            if (PhysVirtAxisIndex > 0) {
                cmd->reply_append_ch(c, ' ');
            }
            cmd->reply_append_ch(c, TheAxis::AxisName);
            cmd->reply_append_ch(c, ':');
            cmd->reply_append_fp(c, axis->m_req_pos);
        }
        
        static void g92_check_axis (Context c, TheCommand *cmd, CommandPartRef part, SetPositionState *s)
        {
            if (cmd->getPartCode(c, part) == TheAxis::AxisName) {
                FpType value = cmd->getPartFpValue(c, part);
                set_position_add_axis<PhysVirtAxisIndex>(c, s, value);
            }
        }
        
        static void update_homing_mask (Context c, TheCommand *cmd, PhysVirtAxisMaskType *mask, CommandPartRef part)
        {
            if (cmd->getPartCode(c, part) == TheAxis::AxisName) {
                *mask |= AxisMask;
            }
        }
        
        static void start_homing (Context c, PhysVirtAxisMaskType mask)
        {
            auto *m = PrinterMain::Object::self(c);
            if (!TheAxis::HomingSpec::Enabled || !(mask & AxisMask)) {
                return;
            }
            m->m_homing_rem_axes |= AxisMask;
            TheAxis::start_phys_homing(c);
        }
        
        static void m119_append_endstop (Context c, TheCommand *cmd)
        {
            if (TheAxis::HomingSpec::Enabled) {
                bool triggered = TheAxis::HomingFeature::endstop_is_triggered(c);
                cmd->reply_append_ch(c, ' ');
                cmd->reply_append_ch(c, TheAxis::AxisName);
                cmd->reply_append_ch(c, ':');
                cmd->reply_append_ch(c, (triggered ? '1' : '0'));
            }
        }
    };
    
    using PhysVirtAxisHelperList = IndexElemListCount<NumPhysVirtAxes, PhysVirtAxisHelper>;
    
    template <int AxisName>
    using FindPhysVirtAxis = TypeListIndexMapped<
        PhysVirtAxisHelperList,
        GetMemberType_WrappedAxisName,
        WrapInt<AxisName>
    >;
    
    template <int HeaterIndex>
    struct Heater {
        struct Object;
        struct ObserverGetValueCallback;
        struct ObserverHandler;
        
        using HeaterSpec = TypeListGet<ParamsHeatersList, HeaterIndex>;
        using ControlInterval = decltype(Config::e(HeaterSpec::ControlInterval::i));
        using TheControl = typename HeaterSpec::ControlService::template Control<Context, Object, Config, ControlInterval, FpType>;
        using ThePwm = typename HeaterSpec::PwmService::template Pwm<Context, Object>;
        using TheObserver = typename HeaterSpec::ObserverService::template Observer<Context, Object, Config, FpType, ObserverGetValueCallback, ObserverHandler>;
        using PwmDutyCycleData = typename ThePwm::DutyCycleData;
        using TheFormula = typename HeaterSpec::Formula::template Formula<Context, Object, Config, FpType>;
        using AdcFixedType = typename Context::Adc::FixedType;
        using AdcIntType = typename AdcFixedType::IntType;
        
        // compute the ADC readings corresponding to MinSafeTemp and MaxSafeTemp
        using AdcRange = APRINTER_FP_CONST_EXPR((PowerOfTwo<double, AdcFixedType::num_bits>::Value));
        template <typename Temp>
        static auto TempToAdcAbs (Temp) -> decltype(TheFormula::TempToAdc(Temp()) * AdcRange());
        using AdcFpLowLimit = APRINTER_FP_CONST_EXPR(1.0 + 0.1);
        using AdcFpHighLimit = APRINTER_FP_CONST_EXPR((PowerOfTwoMinusOne<AdcIntType, AdcFixedType::num_bits>::Value - 0.1));
        using InfAdcValueFp = decltype(ExprFmax(AdcFpLowLimit(), TempToAdcAbs(Config::e(HeaterSpec::MaxSafeTemp::i))));
        using SupAdcValueFp = decltype(ExprFmin(AdcFpHighLimit(), TempToAdcAbs(Config::e(HeaterSpec::MinSafeTemp::i))));
        
        using CMinSafeTemp = decltype(ExprCast<FpType>(Config::e(HeaterSpec::MinSafeTemp::i)));
        using CMaxSafeTemp = decltype(ExprCast<FpType>(Config::e(HeaterSpec::MaxSafeTemp::i)));
        using CInfAdcValue = decltype(ExprCast<AdcIntType>(InfAdcValueFp()));
        using CSupAdcValue = decltype(ExprCast<AdcIntType>(SupAdcValueFp()));
        using CControlIntervalTicks = decltype(ExprCast<TimeType>(ControlInterval() * TimeConversion()));
        
        struct ChannelPayload {
            FpType target;
        };
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            o->m_enabled = false;
            TimeType time = Clock::getTime(c) + (TimeType)(0.05 * TimeConversion::value());
            o->m_control_event.init(c, Heater::control_event_handler);
            o->m_control_event.appendAt(c, time + (APRINTER_CFG(Config, CControlIntervalTicks, c) / 2));
            o->m_was_not_unset = false;
            ThePwm::init(c, time);
            o->m_observing = false;
        }
        
        static void deinit (Context c)
        {
            auto *o = Object::self(c);
            if (o->m_observing) {
                TheObserver::deinit(c);
            }
            ThePwm::deinit(c);
            o->m_control_event.deinit(c);
        }
        
        static FpType adc_to_temp (Context c, AdcFixedType adc_value)
        {
            FpType adc_fp = adc_value.template fpValue<FpType>() + (FpType)(0.5 / PowerOfTwo<double, AdcFixedType::num_bits>::Value);
            return TheFormula::adcToTemp(c, adc_fp);
        }
        
        template <typename ThisContext>
        static AdcFixedType get_adc (ThisContext c)
        {
            return Context::Adc::template getValue<typename HeaterSpec::AdcPin>(c);
        }
        
        static bool adc_is_unsafe (Context c, AdcFixedType adc_value)
        {
            return (adc_value.bitsValue() <= APRINTER_CFG(Config, CInfAdcValue, c) || adc_value.bitsValue() >= APRINTER_CFG(Config, CSupAdcValue, c));
        }
        
        static FpType get_temp (Context c)
        {
            return adc_to_temp(c, get_adc(c));
        }
        
        static void append_value (Context c, TheCommand *cmd)
        {
            FpType value = get_temp(c);
            cmd->reply_append_ch(c, ' ');
            cmd->reply_append_ch(c, HeaterSpec::Name);
            cmd->reply_append_ch(c, ':');
            cmd->reply_append_fp(c, value);
        }
        
        static void append_adc_value (Context c, TheCommand *cmd)
        {
            AdcFixedType adc_value = get_adc(c);
            cmd->reply_append_ch(c, ' ');
            cmd->reply_append_ch(c, HeaterSpec::Name);
            cmd->reply_append_pstr(c, AMBRO_PSTR("A:"));
            cmd->reply_append_fp(c, adc_value.template fpValue<FpType>());
        }
        
        template <typename ThisContext>
        static void set (ThisContext c, FpType target)
        {
            auto *o = Object::self(c);
            
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                o->m_target = target;
                o->m_enabled = true;
            }
        }
        
        template <typename ThisContext>
        static void unset (ThisContext c)
        {
            auto *o = Object::self(c);
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                o->m_enabled = false;
                o->m_was_not_unset = false;
                PwmDutyCycleData duty;
                ThePwm::computeZeroDutyCycle(&duty);
                ThePwm::setDutyCycle(lock_c, duty);
            }
        }
        
        static bool check_command (Context c, TheCommand *cmd)
        {
            auto *o = Object::self(c);
            
            if (cmd->getCmdNumber(c) == HeaterSpec::WaitMCommand) {
                if (!cmd->tryUnplannedCommand(c)) {
                    return false;
                }
                FpType target = cmd->get_command_param_fp(c, 'S', 0.0f);
                if (target >= APRINTER_CFG(Config, CMinSafeTemp, c) && target <= APRINTER_CFG(Config, CMaxSafeTemp, c)) {
                    set(c, target);
                } else {
                    unset(c);
                }
                AMBRO_ASSERT(!o->m_observing)
                TheObserver::init(c, target);
                o->m_observing = true;
                now_active(c);
                return false;
            }
            if (cmd->getCmdNumber(c) == HeaterSpec::SetMCommand) {
                if (!cmd->tryPlannedCommand(c)) {
                    return false;
                }
                FpType target = cmd->get_command_param_fp(c, 'S', 0.0f);
                cmd->finishCommand(c);
                if (!(target >= APRINTER_CFG(Config, CMinSafeTemp, c) && target <= APRINTER_CFG(Config, CMaxSafeTemp, c))) {
                    target = NAN;
                }
                PlannerSplitBuffer *cmd = ThePlanner::getBuffer(c);
                PlannerChannelPayload *payload = UnionGetElem<0>(&cmd->channel_payload);
                payload->type = HeaterIndex;
                UnionGetElem<HeaterIndex>(&payload->heaters)->target = target;
                ThePlanner::channelCommandDone(c, 1);
                submitted_planner_command(c);
                return false;
            }
            return true;
        }
        
        static void check_safety (Context c)
        {
            AdcFixedType adc_value = get_adc(c);
            if (adc_is_unsafe(c, adc_value)) {
                unset(c);
            }
        }
        
        static void observer_handler (Context c, bool state)
        {
            auto *o = Object::self(c);
            auto *mob = PrinterMain::Object::self(c);
            AMBRO_ASSERT(o->m_observing)
            AMBRO_ASSERT(mob->locked)
            
            if (!state) {
                return;
            }
            TheObserver::deinit(c);
            o->m_observing = false;
            now_inactive(c);
            finish_locked(c);
        }
        
        static void emergency ()
        {
            ThePwm::emergency();
        }
        
        template <typename ThisContext, typename TheChannelPayloadUnion>
        static void channel_callback (ThisContext c, TheChannelPayloadUnion *payload_union)
        {
            ChannelPayload *payload = UnionGetElem<HeaterIndex>(payload_union);
            if (AMBRO_LIKELY(!isnan(payload->target))) {
                set(c, payload->target);
            } else {
                unset(c);
            }
        }
        
        static void control_event_handler (typename Loop::QueuedEvent *, Context c)
        {
            auto *o = Object::self(c);
            
            o->m_control_event.appendAfterPrevious(c, APRINTER_CFG(Config, CControlIntervalTicks, c));
            
            AdcFixedType adc_value = get_adc(c);
            if (adc_is_unsafe(c, adc_value)) {
                unset(c);
            }
            
            bool enabled;
            FpType target;
            bool was_not_unset;
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                enabled = o->m_enabled;
                target = o->m_target;
                was_not_unset = o->m_was_not_unset;
                o->m_was_not_unset = enabled;
            }
            if (AMBRO_LIKELY(enabled)) {
                if (!was_not_unset) {
                    TheControl::init(c);
                }
                FpType sensor_value = adc_to_temp(c, adc_value);
                FpType output = TheControl::addMeasurement(c, sensor_value, target);
                PwmDutyCycleData duty;;
                ThePwm::computeDutyCycle(output, &duty);
                AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                    if (o->m_was_not_unset) {
                        ThePwm::setDutyCycle(lock_c, duty);
                    }
                }
            }
        }
        
        struct ObserverGetValueCallback : public AMBRO_WFUNC_TD(&Heater::get_temp) {};
        struct ObserverHandler : public AMBRO_WFUNC_TD(&Heater::observer_handler) {};
        
        struct Object : public ObjBase<Heater, typename PrinterMain::Object, MakeTypeList<
            TheControl,
            ThePwm,
            TheObserver,
            TheFormula
        >> {
            bool m_enabled;
            FpType m_target;
            bool m_observing;
            typename Loop::QueuedEvent m_control_event;
            bool m_was_not_unset;
        };
        
        using ConfigExprs = MakeTypeList<CMinSafeTemp, CMaxSafeTemp, CInfAdcValue, CSupAdcValue, CControlIntervalTicks>;
    };
    
    template <int FanIndex>
    struct Fan {
        struct Object;
        
        using FanSpec = TypeListGet<ParamsFansList, FanIndex>;
        using ThePwm = typename FanSpec::PwmService::template Pwm<Context, Object>;
        using PwmDutyCycleData = typename ThePwm::DutyCycleData;
        
        struct ChannelPayload {
            PwmDutyCycleData duty;
        };
        
        static void init (Context c)
        {
            TimeType time = Clock::getTime(c) + (TimeType)(0.05 * TimeConversion::value());
            ThePwm::init(c, time);
        }
        
        static void deinit (Context c)
        {
            ThePwm::deinit(c);
        }
        
        static bool check_command (Context c, TheCommand *cmd)
        {
            if (cmd->getCmdNumber(c) == FanSpec::SetMCommand || cmd->getCmdNumber(c) == FanSpec::OffMCommand) {
                if (!cmd->tryPlannedCommand(c)) {
                    return false;
                }
                FpType target = 0.0f;
                if (cmd->getCmdNumber(c) == FanSpec::SetMCommand) {
                    target = 1.0f;
                    if (cmd->find_command_param_fp(c, 'S', &target)) {
                        target *= (FpType)FanSpec::SpeedMultiply::value();
                    }
                }
                cmd->finishCommand(c);
                PlannerSplitBuffer *cmd = ThePlanner::getBuffer(c);
                PlannerChannelPayload *payload = UnionGetElem<0>(&cmd->channel_payload);
                payload->type = TypeListLength<ParamsHeatersList>::Value + FanIndex;
                ThePwm::computeDutyCycle(target, &UnionGetElem<FanIndex>(&payload->fans)->duty);
                ThePlanner::channelCommandDone(c, 1);
                submitted_planner_command(c);
                return false;
            }
            return true;
        }
        
        static void emergency ()
        {
            ThePwm::emergency();
        }
        
        template <typename ThisContext, typename TheChannelPayloadUnion>
        AMBRO_OPTIMIZE_SPEED
        static void channel_callback (ThisContext c, TheChannelPayloadUnion *payload_union)
        {
            ChannelPayload *payload = UnionGetElem<FanIndex>(payload_union);
            ThePwm::setDutyCycle(c, payload->duty);
        }
        
        struct Object : public ObjBase<Fan, typename PrinterMain::Object, MakeTypeList<
            ThePwm
        >> {};
    };
    
    using HeatersList = IndexElemList<ParamsHeatersList, Heater>;
    using FansList = IndexElemList<ParamsFansList, Fan>;
    
    using HeatersChannelPayloadUnion = Union<MapTypeList<HeatersList, GetMemberType_ChannelPayload>>;
    using FansChannelPayloadUnion = Union<MapTypeList<FansList, GetMemberType_ChannelPayload>>;
    
    struct PlannerChannelPayload {
        uint8_t type;
        union {
            HeatersChannelPayloadUnion heaters;
            FansChannelPayloadUnion fans;
        };
    };
    
    using MotionPlannerChannels = MakeTypeList<MotionPlannerChannelSpec<PlannerChannelPayload, PlannerChannelCallback, Params::EventChannelBufferSize, typename Params::EventChannelTimerService>>;
    using MotionPlannerAxes = MapTypeList<AxesList, TemplateFunc<MakePlannerAxisSpec>>;
    using MotionPlannerLasers = MapTypeList<LasersList, TemplateFunc<MakePlannerLaserSpec>>;
    using ThePlanner = MotionPlanner<Context, typename PlannerUnionPlanner::Object, Config, MotionPlannerAxes, Params::StepperSegmentBufferSize, Params::LookaheadBufferSize, Params::LookaheadCommitCount, FpType, PlannerPullHandler, PlannerFinishedHandler, PlannerAbortedHandler, PlannerUnderrunCallback, MotionPlannerChannels, MotionPlannerLasers>;
    using PlannerSplitBuffer = typename ThePlanner::SplitBuffer;
    
    AMBRO_STRUCT_IF(ProbeFeature, Params::ProbeParams::Enabled) {
        struct Object;
        using ProbeParams = typename Params::ProbeParams;
        static const int NumPoints = TypeListLength<typename ProbeParams::ProbePoints>::Value;
        static const int ProbeAxisIndex = FindPhysVirtAxis<Params::ProbeParams::ProbeAxis>::Value;
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            o->m_current_point = 0xff;
            Context::Pins::template setInput<typename ProbeParams::ProbePin, typename ProbeParams::ProbePinInputMode>(c);
        }
        
        static void deinit (Context c)
        {
        }
        
        static bool check_command (Context c, TheCommand *cmd)
        {
            auto *o = Object::self(c);
            if (cmd->getCmdNumber(c) == 32) {
                if (!cmd->tryUnplannedCommand(c)) {
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
            static const int AxisIndex = FindPhysVirtAxis<PlatformAxis::Value>::Value;
            using AxisProbeOffset = TypeListGet<typename ProbeParams::ProbePlatformOffset, PlatformAxisIndex>;
            
            static void add_axis (Context c, uint8_t point_index)
            {
                FpType coord = ListForOneOffset<PointHelperList, 0, FpType>(point_index, LForeach_get_coord(), c);
                move_add_axis<AxisIndex>(c, coord + APRINTER_CFG(Config, CAxisProbeOffset, c));
            }
            
            template <int PointIndex>
            struct PointHelper {
                using Point = TypeListGet<typename ProbeParams::ProbePoints, PointIndex>;
                using PointCoord = TypeListGet<Point, PlatformAxisIndex>;
                
                static FpType get_coord (Context c) { return APRINTER_CFG(Config, CPointCoord, c); }
                
                using CPointCoord = decltype(ExprCast<FpType>(Config::e(PointCoord::i)));
                using ConfigExprs = MakeTypeList<CPointCoord>;
                
                struct Object : public ObjBase<PointHelper, typename AxisHelper::Object, EmptyTypeList> {};
            };
            
            using PointHelperList = IndexElemList<typename ProbeParams::ProbePoints, PointHelper>;
            
            using CAxisProbeOffset = decltype(ExprCast<FpType>(Config::e(AxisProbeOffset::i)));
            using ConfigExprs = MakeTypeList<CAxisProbeOffset>;
            
            struct Object : public ObjBase<AxisHelper, typename ProbeFeature::Object, PointHelperList> {};
        };
        
        using AxisHelperList = IndexElemList<typename ProbeParams::PlatformAxesList, AxisHelper>;
        
        struct ProbePlannerClient : public PlannerClient {
            void pull_handler (Context c)
            {
                auto *o = Object::self(c);
                AMBRO_ASSERT(o->m_current_point != 0xff)
                AMBRO_ASSERT(o->m_point_state <= 4)
                
                if (o->m_command_sent) {
                    custom_planner_wait_finished(c);
                    return;
                }
                move_begin(c);
                FpType height;
                FpType time_freq_by_speed;
                switch (o->m_point_state) {
                    case 0: {
                        ListForEachForward<AxisHelperList>(LForeach_add_axis(), c, o->m_current_point);
                        height = APRINTER_CFG(Config, CProbeStartHeight, c);
                        time_freq_by_speed = APRINTER_CFG(Config, CProbeMoveSpeedFactor, c);
                    } break;
                    case 1: {
                        height = APRINTER_CFG(Config, CProbeLowHeight, c);
                        time_freq_by_speed = APRINTER_CFG(Config, CProbeFastSpeedFactor, c);
                    } break;
                    case 2: {
                        height = get_height(c) + APRINTER_CFG(Config, CProbeRetractDist, c);
                        time_freq_by_speed = APRINTER_CFG(Config, CProbeRetractSpeedFactor, c);
                    } break;
                    case 3: {
                        height = APRINTER_CFG(Config, CProbeLowHeight, c);
                        time_freq_by_speed = APRINTER_CFG(Config, CProbeSlowSpeedFactor, c);
                    } break;
                    case 4: {
                        height = APRINTER_CFG(Config, CProbeStartHeight, c);
                        time_freq_by_speed = APRINTER_CFG(Config, CProbeRetractSpeedFactor, c);
                    } break;
                }
                move_add_axis<ProbeAxisIndex>(c, height);
                move_end(c, time_freq_by_speed);
                o->m_command_sent = true;
            }
            
            void finished_handler (Context c)
            {
                auto *o = Object::self(c);
                AMBRO_ASSERT(o->m_current_point != 0xff)
                AMBRO_ASSERT(o->m_command_sent)
                
                custom_planner_deinit(c);
                o->m_command_sent = false;
                if (o->m_point_state < 4) {
                    if (o->m_point_state == 3) {
                        FpType height = get_height(c);
                        o->m_samples[o->m_current_point] = height;
                        report_height(c, get_locked(c), height);
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
            
            void aborted_handler (Context c)
            {
                auto *o = Object::self(c);
                AMBRO_ASSERT(o->m_current_point != 0xff)
                AMBRO_ASSERT(o->m_command_sent)
                AMBRO_ASSERT(o->m_point_state == 1 || o->m_point_state == 3)
                
                finished_handler(c);
            }
        };
        
        template <typename CallbackContext>
        static bool prestep_callback (CallbackContext c)
        {
            return (Context::Pins::template get<typename ProbeParams::ProbePin>(c) != Params::ProbeParams::ProbeInvert);
        }
        
        static void init_probe_planner (Context c, bool watch_probe)
        {
            auto *o = Object::self(c);
            custom_planner_init(c, &o->planner_client, watch_probe);
        }
        
        static FpType get_height (Context c)
        {
            return GetPhysVirtAxis<ProbeAxisIndex>::Object::self(c)->m_req_pos;
        }
        
        static void report_height (Context c, TheCommand *cmd, FpType height)
        {
            cmd->reply_append_pstr(c, AMBRO_PSTR("//ProbeHeight "));
            cmd->reply_append_fp(c, height);
            cmd->reply_append_ch(c, '\n');
            cmd->reply_poke(c);
        }
        
        using CProbeStartHeight = decltype(ExprCast<FpType>(Config::e(ProbeParams::ProbeStartHeight::i)));
        using CProbeLowHeight = decltype(ExprCast<FpType>(Config::e(ProbeParams::ProbeLowHeight::i)));
        using CProbeRetractDist = decltype(ExprCast<FpType>(Config::e(ProbeParams::ProbeRetractDist::i)));
        using CProbeMoveSpeedFactor = decltype(ExprCast<FpType>(TimeConversion() / Config::e(ProbeParams::ProbeMoveSpeed::i)));
        using CProbeFastSpeedFactor = decltype(ExprCast<FpType>(TimeConversion() / Config::e(ProbeParams::ProbeFastSpeed::i)));
        using CProbeRetractSpeedFactor = decltype(ExprCast<FpType>(TimeConversion() / Config::e(ProbeParams::ProbeRetractSpeed::i)));
        using CProbeSlowSpeedFactor = decltype(ExprCast<FpType>(TimeConversion() / Config::e(ProbeParams::ProbeSlowSpeed::i)));
        
        using ConfigExprs = MakeTypeList<CProbeStartHeight, CProbeLowHeight, CProbeRetractDist, CProbeMoveSpeedFactor, CProbeFastSpeedFactor, CProbeRetractSpeedFactor, CProbeSlowSpeedFactor>;
        
        struct Object : public ObjBase<ProbeFeature, typename PrinterMain::Object, AxisHelperList> {
            ProbePlannerClient planner_client;
            uint8_t m_current_point;
            uint8_t m_point_state;
            bool m_command_sent;
            FpType m_samples[NumPoints];
        };
    } AMBRO_STRUCT_ELSE(ProbeFeature) {
        static void init (Context c) {}
        static void deinit (Context c) {}
        static bool check_command (Context c, TheCommand *cmd) { return true; }
        template <typename CallbackContext>
        static bool prestep_callback (CallbackContext c) { return false; }
        struct Object {};
    };
    
    AMBRO_STRUCT_IF(CurrentFeature, Params::CurrentParams::Enabled) {
        struct Object;
        using CurrentParams = typename Params::CurrentParams;
        using ParamsCurrentAxesList = typename CurrentParams::CurrentAxesList;
        template <typename ChannelAxisParams>
        using MakeCurrentChannel = typename ChannelAxisParams::Params;
        using CurrentChannelsList = MapTypeList<ParamsCurrentAxesList, TemplateFunc<MakeCurrentChannel>>;
        using Current = typename CurrentParams::CurrentService::template Current<Context, Object, Config, FpType, CurrentChannelsList>;
        
        static void init (Context c)
        {
            Current::init(c);
            apply_default(c);
        }
        
        static void deinit (Context c)
        {
            Current::deinit(c);
        }
        
        static void apply_default (Context c)
        {
            ListForEachForward<CurrentAxesList>(LForeach_apply_default(), c);
        }
        
        static bool check_command (Context c, TheCommand *cmd)
        {
            if (cmd->getCmdNumber(c) == 906) {
                auto num_parts = cmd->getNumParts(c);
                for (decltype(num_parts) i = 0; i < num_parts; i++) {
                    CommandPartRef part = cmd->getPart(c, i);
                    ListForEachForwardInterruptible<CurrentAxesList>(LForeach_check_current_axis(), c, cmd, cmd->getPartCode(c, part), cmd->getPartFpValue(c, part));
                }
                cmd->finishCommand(c);
                return false;
            }
            return true;
        }
        
        template <int CurrentAxisIndex>
        struct CurrentAxis {
            using CurrentAxisParams = TypeListGet<ParamsCurrentAxesList, CurrentAxisIndex>;
            
            static void apply_default (Context c)
            {
                Current::template setCurrent<CurrentAxisIndex>(c, APRINTER_CFG(Config, CCurrent, c));
            }
            
            static bool check_current_axis (Context c, TheCommand *cmd, char axis_name, FpType current)
            {
                if (axis_name == CurrentAxisParams::AxisName) {
                    Current::template setCurrent<CurrentAxisIndex>(c, current);
                    return false;
                }
                return true;
            }
            
            using CCurrent = decltype(ExprCast<FpType>(Config::e(CurrentAxisParams::DefaultCurrent::i)));
            using ConfigExprs = MakeTypeList<CCurrent>;
            
            struct Object : public ObjBase<CurrentAxis, typename CurrentFeature::Object, EmptyTypeList> {};
        };
        
        using CurrentAxesList = IndexElemList<ParamsCurrentAxesList, CurrentAxis>;
        
        struct Object : public ObjBase<CurrentFeature, typename PrinterMain::Object, JoinTypeLists<
            CurrentAxesList,
            MakeTypeList<
                Current
            >
        >> {};
    } AMBRO_STRUCT_ELSE(CurrentFeature) {
        static void init (Context c) {}
        static void deinit (Context c) {}
        static void apply_default (Context c) {}
        static bool check_command (Context c, TheCommand *cmd) { return true; }
        struct Object {};
    };
    
    AMBRO_STRUCT_IF(LoadConfigFeature, TheConfigManager::HasStore) {
        static void start_loading (Context c)
        {
            lock(c);
            TheConfigManager::startOperation(c, TheConfigManager::OperationType::LOAD);
        }
    } AMBRO_STRUCT_ELSE(LoadConfigFeature) {
        static void start_loading (Context c) {}
    };
    
public:
    static void init (Context c)
    {
        auto *ob = Object::self(c);
        
        TheWatchdog::init(c);
        TheConfigManager::init(c);
        TheConfigCache::init(c);
        ob->unlocked_timer.init(c, PrinterMain::unlocked_timer_handler);
        ob->disable_timer.init(c, PrinterMain::disable_timer_handler);
        ob->force_timer.init(c, PrinterMain::force_timer_handler);
        TheBlinker::init(c, (FpType)(Params::LedBlinkInterval::value() * TimeConversion::value()));
        TheSteppers::init(c);
        SerialFeature::init(c);
        SdCardFeature::init(c);
        ListForEachForward<AxesList>(LForeach_init(), c);
        ListForEachForward<LasersList>(LForeach_init(), c);
        TransformFeature::init(c);
        ListForEachForward<HeatersList>(LForeach_init(), c);
        ListForEachForward<FansList>(LForeach_init(), c);
        ProbeFeature::init(c);
        CurrentFeature::init(c);
        ob->time_freq_by_max_speed = 0.0f;
        ob->underrun_count = 0;
        ob->locked = false;
        ob->planner_state = PLANNER_NONE;
        
        SerialFeature::TheChannelCommon::impl(c)->reply_append_pstr(c, AMBRO_PSTR("start\nAPrinter\n"));
        SerialFeature::TheChannelCommon::impl(c)->reply_poke(c);
        
        LoadConfigFeature::start_loading(c);
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *ob = Object::self(c);
        TheDebugObject::deinit(c);
        
        if (ob->planner_state != PLANNER_NONE) {
            ThePlanner::deinit(c);
        }
        CurrentFeature::deinit(c);
        ProbeFeature::deinit(c);
        ListForEachReverse<FansList>(LForeach_deinit(), c);
        ListForEachReverse<HeatersList>(LForeach_deinit(), c);
        ListForEachReverse<LasersList>(LForeach_deinit(), c);
        ListForEachReverse<AxesList>(LForeach_deinit(), c);
        SdCardFeature::deinit(c);
        SerialFeature::deinit(c);
        TheSteppers::deinit(c);
        TheBlinker::deinit(c);
        ob->force_timer.deinit(c);
        ob->disable_timer.deinit(c);
        ob->unlocked_timer.deinit(c);
        TheConfigCache::deinit(c);
        TheConfigManager::deinit(c);
        TheWatchdog::deinit(c);
    }
    
    using GetConfigManager = TheConfigManager;
    
    using GetWatchdog = TheWatchdog;
    
    using GetSerial = typename SerialFeature::TheSerial;
    
    template <int AxisIndex>
    using GetAxisTimer = typename Axis<AxisIndex>::TheAxisDriver::GetTimer;
    
    template <int HeaterIndex>
    using GetHeaterPwm = typename Heater<HeaterIndex>::ThePwm;
    
    template <int FanIndex>
    using GetFanPwm = typename Fan<FanIndex>::ThePwm;
    
    using GetEventChannelTimer = typename ThePlanner::template GetChannelTimer<0>;
    
    template <typename TSdCardFeatue = SdCardFeature>
    using GetInput = typename TSdCardFeatue::TheInput;
    
    template <typename TCurrentFeatue = CurrentFeature>
    using GetCurrent = typename TCurrentFeatue::Current;
    
    template <int LaserIndex>
    using GetLaserDriver = typename ThePlanner::template Laser<LaserIndex>::TheLaserDriver;
    
    static void emergency ()
    {
        ListForEachForward<AxesList>(LForeach_emergency());
        ListForEachForward<LasersList>(LForeach_emergency());
        ListForEachForward<HeatersList>(LForeach_emergency());
        ListForEachForward<FansList>(LForeach_emergency());
    }
    
    using EventLoopFastEvents = ObjCollect<MakeTypeList<PrinterMain>, Collectible_EventLoopFastEvents, true>;
        
    static void finish_locked (Context c)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->locked)
        
        TheCommand *cmd = get_locked(c);
        cmd->finishCommand(c);
    }
    
    static CommandType * get_locked (Context c)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->locked)
        
        return get_command_in_state<COMMAND_LOCKED, true>(c);
    }
    
public: // private, see comment on top
    static TimeType time_from_real (FpType t)
    {
        return (FixedPoint<30, false, 0>::importFpSaturatedRound(t * (FpType)TimeConversion::value())).bitsValue();
    }
    
    static void blinker_handler (Context c)
    {
        auto *ob = Object::self(c);
        TheDebugObject::access(c);
        
        ListForEachForward<HeatersList>(LForeach_check_safety(), c);
        TheWatchdog::reset(c);
    }
    
    AMBRO_OPTIMIZE_SPEED
    static void work_command (Context c, TheCommand *cmd)
    {
        auto *ob = Object::self(c);
        
        switch (cmd->getCmdCode(c)) {
            case 'M': switch (cmd->getCmdNumber(c)) {
                default:
                    if (
                        ListForEachForwardInterruptible<HeatersList>(LForeach_check_command(), c, cmd) &&
                        ListForEachForwardInterruptible<FansList>(LForeach_check_command(), c, cmd) &&
                        SdCardFeature::check_command(c, cmd) &&
                        ProbeFeature::check_command(c, cmd) &&
                        CurrentFeature::check_command(c, cmd) &&
                        TheConfigManager::checkCommand(c, cmd)
                    ) {
                        goto unknown_command;
                    }
                    return;
                
                case 110: // set line number
                    return cmd->finishCommand(c);
                
                case 17: { // enable steppers (all steppers if no args, or specific ones)
                    if (!cmd->tryUnplannedCommand(c)) {
                        return;
                    }
                    enable_disable_command_common(c, true, cmd);
                    now_inactive(c);
                    return cmd->finishCommand(c);
                } break;
                
                case 18: // disable steppers (all steppers if no args, or specific ones)
                case 84: {
                    if (!cmd->tryUnplannedCommand(c)) {
                        return;
                    }
                    enable_disable_command_common(c, false, cmd);
                    ob->disable_timer.unset(c);
                    return cmd->finishCommand(c);
                } break;
                
                case 105: {
                    cmd->reply_append_pstr(c, AMBRO_PSTR("ok"));
                    ListForEachForward<HeatersList>(LForeach_append_value(), c, cmd);
                    cmd->reply_append_ch(c, '\n');
                    return cmd->finishCommand(c, true);
                } break;
                
                case 114: {
                    ListForEachForward<PhysVirtAxisHelperList>(LForeach_append_position(), c, cmd);
                    cmd->reply_append_ch(c, '\n');
                    return cmd->finishCommand(c);
                } break;
                
                case 119: {
                    cmd->reply_append_pstr(c, AMBRO_PSTR("endstops:"));
                    ListForEachForward<PhysVirtAxisHelperList>(LForeach_m119_append_endstop(), c, cmd);
                    cmd->reply_append_ch(c, '\n');                    
                    return cmd->finishCommand(c, true);
                } break;
                
                case 400: {
                    if (!cmd->tryUnplannedCommand(c)) {
                        return;
                    }
                    return cmd->finishCommand(c);
                } break;
                
#ifdef EVENTLOOP_BENCHMARK
                case 916: { // reset benchmark time
                    if (!cmd->tryUnplannedCommand(c)) {
                        return;
                    }
                    Context::EventLoop::resetBenchTime(c);
                    return cmd->finishCommand(c);
                } break;
                
                case 917: { // print benchmark time
                    if (!cmd->tryUnplannedCommand(c)) {
                        return;
                    }
                    cmd->reply_append_uint32(c, Context::EventLoop::getBenchTime(c));
                    cmd->reply_append_ch(c, '\n');
                    return cmd->finishCommand(c);
                } break;
#endif
                
                case 920: { // get underrun count
                    cmd->reply_append_uint32(c, ob->underrun_count);
                    cmd->reply_append_ch(c, '\n');
                    cmd->finishCommand(c);
                } break;
                
                case 921: { // get heater ADC readings
                    cmd->reply_append_pstr(c, AMBRO_PSTR("ok"));
                    ListForEachForward<HeatersList>(LForeach_append_adc_value(), c, cmd);
                    cmd->reply_append_ch(c, '\n');
                    return cmd->finishCommand(c, true);
                } break;
                
                case 930: { // apply configuration
                    if (!cmd->tryUnplannedCommand(c)) {
                        return;
                    }
                    TheConfigCache::update(c);
                    CurrentFeature::apply_default(c);
                    return cmd->finishCommand(c);
                } break;
            } break;
            
            case 'G': switch (cmd->getCmdNumber(c)) {
                default:
                    goto unknown_command;
                
                case 0:
                case 1: { // buffered move
                    if (!cmd->tryPlannedCommand(c)) {
                        return;
                    }
                    move_begin(c);
                    auto num_parts = cmd->getNumParts(c);
                    for (decltype(num_parts) i = 0; i < num_parts; i++) {
                        CommandPartRef part = cmd->getPart(c, i);
                        if (ListForEachForwardInterruptible<PhysVirtAxisHelperList>(LForeach_collect_new_pos(), c, cmd, part) &&
                            ListForEachForwardInterruptible<LasersList>(LForeach_collect_new_pos(), c, cmd, part)
                        ) {
                            if (cmd->getPartCode(c, part) == 'F') {
                                ob->time_freq_by_max_speed = (FpType)(TimeConversion::value() / Params::SpeedLimitMultiply::value()) / FloatMakePosOrPosZero(cmd->getPartFpValue(c, part));
                            }
                        }
                    }
                    bool is_positioning_move = (cmd->getCmdNumber(c) == 0);
                    cmd->finishCommand(c);
                    move_end(c, ob->time_freq_by_max_speed, is_positioning_move);
                } break;
                
                case 21: // set units to millimeters
                    return cmd->finishCommand(c);
                
                case 28: { // home axes
                    if (!cmd->tryUnplannedCommand(c)) {
                        return;
                    }
                    PhysVirtAxisMaskType mask = 0;
                    auto num_parts = cmd->getNumParts(c);
                    for (decltype(num_parts) i = 0; i < num_parts; i++) {
                        ListForEachForward<PhysVirtAxisHelperList>(LForeach_update_homing_mask(), c, cmd, &mask, cmd->getPart(c, i));
                    }
                    if (mask == 0) {
                        mask = -1;
                    }
                    ob->m_homing_rem_axes = 0;
                    now_active(c);
                    ListForEachForward<PhysVirtAxisHelperList>(LForeach_start_homing(), c, mask);
                    if (!(ob->m_homing_rem_axes & PhysAxisMask)) {
                        return phys_homing_finished(c);
                    }
                } break;
                
                case 90: { // absolute positioning
                    ListForEachForward<PhysVirtAxisHelperList>(LForeach_set_relative_positioning(), c, false);
                    return cmd->finishCommand(c);
                } break;
                
                case 91: { // relative positioning
                    ListForEachForward<PhysVirtAxisHelperList>(LForeach_set_relative_positioning(), c, true);
                    return cmd->finishCommand(c);
                } break;
                
                case 92: { // set position
                    if (!cmd->trySplitClearCommand(c)) {
                        return;
                    }
                    SetPositionState s;
                    set_position_begin(c, &s);
                    auto num_parts = cmd->getNumParts(c);
                    for (decltype(num_parts) i = 0; i < num_parts; i++) {
                        ListForEachForward<PhysVirtAxisHelperList>(LForeach_g92_check_axis(), c, cmd, cmd->getPart(c, i), &s);
                    }
                    set_position_end(c, &s);
                    return cmd->finishCommand(c);
                } break;
            } break;
            
            unknown_command:
            default: {
                cmd->reply_append_pstr(c, AMBRO_PSTR("Error:Unknown command "));
                cmd->reply_append_ch(c, cmd->getCmdCode(c));
                cmd->reply_append_uint16(c, cmd->getCmdNumber(c));
                cmd->reply_append_ch(c, '\n');
                return cmd->finishCommand(c);
            } break;
        }
    }
    
    static void lock (Context c)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(!ob->locked)
        
        ob->locked = true;
    }
    
    static void unlock (Context c)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->locked)
        
        ob->locked = false;
        if (!ob->unlocked_timer.isSet(c)) {
            ob->unlocked_timer.prependNowNotAlready(c);
        }
    }
    
    static void phys_homing_finished (Context c)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->locked)
        AMBRO_ASSERT(!(ob->m_homing_rem_axes & PhysAxisMask))
        
        TransformFeature::do_pending_virt_update(c);
        work_virt_homing(c);
    }
    
    static void work_virt_homing (Context c)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->locked)
        AMBRO_ASSERT(!(ob->m_homing_rem_axes & PhysAxisMask))
        
        if (!TransformFeature::start_virt_homing(c)) {
            return;
        }
        now_inactive(c);
        finish_locked(c);
    }
    
    static void now_inactive (Context c)
    {
        auto *ob = Object::self(c);
        
        TimeType now = Clock::getTime(c);
        ob->disable_timer.appendAt(c, now + APRINTER_CFG(Config, CInactiveTimeTicks, c));
        TheBlinker::setInterval(c, (FpType)(Params::LedBlinkInterval::value() * TimeConversion::value()));
    }
    
    static void now_active (Context c)
    {
        auto *ob = Object::self(c);
        
        ob->disable_timer.unset(c);
        TheBlinker::setInterval(c, (FpType)((Params::LedBlinkInterval::value() / 2) * TimeConversion::value()));
    }
    
    static void set_force_timer (Context c)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->planner_state == PLANNER_RUNNING)
        AMBRO_ASSERT(ob->m_planning_pull_pending)
        
        TimeType force_time = Clock::getTime(c) + APRINTER_CFG(Config, CForceTimeoutTicks, c);
        ob->force_timer.appendAt(c, force_time);
    }
    
    static void unlocked_timer_handler (typename Loop::QueuedEvent *, Context c)
    {
        auto *ob = Object::self(c);
        TheDebugObject::access(c);
        
        if (!ob->locked) {
            TheCommand *cmd = get_command_in_state<COMMAND_LOCKING, false>(c);
            if (cmd) {
                work_command(c, cmd);
            }
        }
    }
    
    static void disable_timer_handler (typename Loop::QueuedEvent *, Context c)
    {
        auto *ob = Object::self(c);
        TheDebugObject::access(c);
        
        ListForEachForward<AxesList>(LForeach_enable_disable_stepper(), c, false);
    }
    
    static void force_timer_handler (typename Loop::QueuedEvent *, Context c)
    {
        auto *ob = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(ob->planner_state == PLANNER_RUNNING)
        AMBRO_ASSERT(ob->m_planning_pull_pending)
        
        ThePlanner::waitFinished(c);
    }
    
    AMBRO_OPTIMIZE_SPEED
    static void planner_pull_handler (Context c)
    {
        auto *ob = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(ob->planner_state != PLANNER_NONE)
        AMBRO_ASSERT(!ob->m_planning_pull_pending)
        
        ob->m_planning_pull_pending = true;
        if (TransformFeature::is_splitting(c)) {
            TransformFeature::split_more(c);
            return;
        }
        if (ob->planner_state == PLANNER_STOPPING) {
            ThePlanner::waitFinished(c);
        } else if (ob->planner_state == PLANNER_WAITING) {
            AMBRO_ASSERT(ob->locked)
            ob->planner_state = PLANNER_RUNNING;
            TheCommand *cmd = get_locked(c);
            work_command(c, cmd);
        } else if (ob->planner_state == PLANNER_RUNNING) {
            set_force_timer(c);
        } else {
            AMBRO_ASSERT(ob->planner_state == PLANNER_CUSTOM)
            ob->planner_client->pull_handler(c);
        }
    }
    
    AMBRO_OPTIMIZE_SPEED
    static void planner_finished_handler (Context c)
    {
        auto *ob = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(ob->planner_state != PLANNER_NONE)
        AMBRO_ASSERT(ob->m_planning_pull_pending)
        AMBRO_ASSERT(ob->planner_state != PLANNER_WAITING)
        
        if (ob->planner_state == PLANNER_CUSTOM) {
            return ob->planner_client->finished_handler(c);
        }
        
        uint8_t old_state = ob->planner_state;
        ThePlanner::deinit(c);
        ob->force_timer.unset(c);
        ob->planner_state = PLANNER_NONE;
        now_inactive(c);
        
        if (old_state == PLANNER_STOPPING) {
            TheCommand *cmd = get_locked(c);
            work_command(c, cmd);
        }
    }
    
    static void planner_aborted_handler (Context c)
    {
        auto *ob = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(ob->planner_state == PLANNER_CUSTOM)
        
        ListForEachForward<AxesList>(LForeach_fix_aborted_pos(), c);
        TransformFeature::do_pending_virt_update(c);
        ob->planner_client->aborted_handler(c);
    }
    
    static void planner_underrun_callback (Context c)
    {
        auto *ob = Object::self(c);
        ob->underrun_count++;
        
#ifdef AXISDRIVER_DETECT_OVERLOAD
        if (ThePlanner::axisOverloadOccurred(c)) {
            SerialFeature::TheChannelCommon::impl(c)->reply_append_pstr(c, AMBRO_PSTR("//AxisOverload\n"));
        } else {
            SerialFeature::TheChannelCommon::impl(c)->reply_append_pstr(c, AMBRO_PSTR("//NoOverload\n"));
        }
        SerialFeature::TheChannelCommon::impl(c)->reply_poke(c);
#endif
    }
    
    AMBRO_OPTIMIZE_SPEED
    static void planner_channel_callback (typename ThePlanner::template Channel<0>::CallbackContext c, PlannerChannelPayload *payload)
    {
        auto *ob = Object::self(c);
        TheDebugObject::access(c);
        
        ListForOneBoolOffset<HeatersList, 0>(payload->type, LForeach_channel_callback(), c, &payload->heaters) ||
        ListForOneBoolOffset<FansList, TypeListLength<ParamsHeatersList>::Value>(payload->type, LForeach_channel_callback(), c, &payload->fans);
    }
    
    template <int AxisIndex>
    static bool planner_prestep_callback (typename ThePlanner::template Axis<AxisIndex>::StepperCommandCallbackContext c)
    {
        return
            ProbeFeature::prestep_callback(c) ||
            TransformFeature::prestep_callback(c);
    }
    
    static void move_begin (Context c)
    {
        auto *ob = Object::self(c);
        ListForEachForward<PhysVirtAxisHelperList>(LForeach_init_new_pos(), c);
        ob->move_seen_cartesian = false;
        ListForEachForward<LasersList>(LForeach_begin_move(), c);
    }
    
    template <int PhysVirtAxisIndex>
    static void move_add_axis (Context c, FpType value)
    {
        PhysVirtAxisHelper<PhysVirtAxisIndex>::update_new_pos(c, value);
    }
    
    template <int LaserIndex>
    static void move_add_laser (Context c, FpType energy)
    {
        auto *laser = Laser<LaserIndex>::Object::self(c);
        laser->move_energy = FloatMakePosOrPosZero(energy);
        laser->move_energy_specified = true;
    }
    
    struct ReqPosSrc {
        Context m_c;
        template <int Index>
        FpType get () { return Axis<Index>::Object::self(m_c)->m_req_pos; }
    };
    
    struct LaserExtraSrc {
        Context m_c;
        template <int LaserIndex>
        FpType get () { return Laser<LaserIndex>::Object::self(m_c)->move_energy; }
    };
    
    AMBRO_OPTIMIZE_SPEED
    static void move_end (Context c, FpType time_freq_by_max_speed, bool is_positioning_move=true)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->planner_state == PLANNER_RUNNING || ob->planner_state == PLANNER_CUSTOM)
        AMBRO_ASSERT(ob->m_planning_pull_pending)
        AMBRO_ASSERT(FloatIsPosOrPosZero(time_freq_by_max_speed))
        
        if (TransformFeature::is_splitting(c)) {
            TransformFeature::handle_virt_move(c, time_freq_by_max_speed, is_positioning_move);
            return;
        }
        PlannerSplitBuffer *cmd = ThePlanner::getBuffer(c);
        FpType distance_squared = 0.0f;
        FpType total_steps = 0.0f;
        ListForEachForward<AxesList>(LForeach_do_move(), c, ReqPosSrc{c}, WrapBool<true>(), &distance_squared, &total_steps, cmd);
        TransformFeature::do_pending_virt_update(c);
        if (total_steps != 0.0f) {
            cmd->axes.rel_max_v_rec = total_steps * APRINTER_CFG(Config, CStepSpeedLimitFactor, c);
            if (ob->move_seen_cartesian) {
                FpType distance = FloatSqrt(distance_squared);
                cmd->axes.rel_max_v_rec = FloatMax(cmd->axes.rel_max_v_rec, distance * time_freq_by_max_speed);
                ListForEachForward<LasersList>(LForeach_handle_automatic_energy(), c, distance, is_positioning_move);
            } else {
                ListForEachForward<AxesList>(LForeach_limit_axis_move_speed(), c, time_freq_by_max_speed, cmd);
            }
            ListForEachForward<LasersList>(LForeach_write_planner_cmd(), c, LaserExtraSrc{c}, cmd);
            ThePlanner::axesCommandDone(c);
        } else {
            ThePlanner::emptyDone(c);
        }
        submitted_planner_command(c);
    }
    
    struct SetPositionState {
        bool seen_virtual;
    };
    
    static void set_position_begin (Context c, SetPositionState *s)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->locked)
        AMBRO_ASSERT(!TransformFeature::is_splitting(c))
        
        s->seen_virtual = false;
    }
    
    template <int PhysVirtAxisIndex>
    static void set_position_add_axis (Context c, SetPositionState *s, FpType value)
    {
        PhysVirtAxisHelper<PhysVirtAxisIndex>::TheAxis::set_position(c, value, &s->seen_virtual);
    }
    
    static void set_position_end (Context c, SetPositionState *s)
    {
        TransformFeature::handle_set_position(c, s->seen_virtual);
    }
    
    static void submitted_planner_command (Context c)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->planner_state != PLANNER_NONE)
        AMBRO_ASSERT(ob->m_planning_pull_pending)
        
        ob->m_planning_pull_pending = false;
        ob->force_timer.unset(c);
    }
    
    static void custom_planner_init (Context c, PlannerClient *planner_client, bool enable_prestep_callback)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->locked)
        AMBRO_ASSERT(ob->planner_state == PLANNER_NONE)
        
        ob->planner_state = PLANNER_CUSTOM;
        ob->planner_client = planner_client;
        ThePlanner::init(c, enable_prestep_callback);
        ob->m_planning_pull_pending = false;
        now_active(c);
    }
    
    static void custom_planner_deinit (Context c)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->locked)
        AMBRO_ASSERT(ob->planner_state == PLANNER_CUSTOM)
        
        ThePlanner::deinit(c);
        ob->planner_state = PLANNER_NONE;
        now_inactive(c);
    }
    
    static void custom_planner_wait_finished (Context c)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->locked)
        AMBRO_ASSERT(ob->planner_state == PLANNER_CUSTOM)
        AMBRO_ASSERT(ob->m_planning_pull_pending)
        
        ThePlanner::waitFinished(c);
    }
    
    static void config_manager_handler (Context c, bool success)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(TheConfigManager::HasStore)
        AMBRO_ASSERT(ob->locked)
        
        if (success) {
            TheConfigCache::update(c);
        }
        unlock(c);
        auto msg = success ? AMBRO_PSTR("//LoadConfigOk\n") : AMBRO_PSTR("//LoadConfigErr\n");
        SerialFeature::TheChannelCommon::impl(c)->reply_append_pstr(c, msg);
        SerialFeature::TheChannelCommon::impl(c)->reply_poke(c);
    }
    
    template <int State, bool Must>
    static TheCommand * get_command_in_state (Context c)
    {
        TheCommand *cmd;
        bool res = ListForEachForwardInterruptible<ChannelCommonList>(LForeach_get_command_in_state_helper(), c, WrapInt<State>(), &cmd);
        if (Must) {
            AMBRO_ASSERT(!res)
        } else {
            if (res) {
                return NULL;
            }
        }
        return cmd;
    }
    
    static void enable_disable_command_common (Context c, bool enable, TheCommand *cmd)
    {
        auto num_parts = cmd->getNumParts(c);
        if (num_parts == 0) {
            ListForEachForward<AxesList>(LForeach_enable_disable_stepper(), c, enable);
        } else {
            for (decltype(num_parts) i = 0; i < num_parts; i++) {
                ListForEachForward<AxesList>(LForeach_enable_disable_stepper_specific(), c, enable, cmd, cmd->getPart(c, i));
            }
        }
    }
    
    struct PlannerUnion {
        struct Object : public ObjUnionBase<PlannerUnion, typename PrinterMain::Object, MakeTypeList<
            PlannerUnionPlanner,
            PlannerUnionHoming
        >> {};
    };
    
    struct PlannerUnionPlanner {
        struct Object : public ObjBase<PlannerUnionPlanner, typename PlannerUnion::Object, MakeTypeList<ThePlanner>> {};
    };
    
    using HomingStateList = MapTypeList<AxesList, GetMemberType_HomingState>;
    
    struct PlannerUnionHoming {
        struct Object : public ObjBase<PlannerUnionHoming, typename PlannerUnion::Object, HomingStateList> {};
    };
    
    struct ConfigManagerHandler : public AMBRO_WFUNC_TD(&PrinterMain::config_manager_handler) {};
    struct BlinkerHandler : public AMBRO_WFUNC_TD(&PrinterMain::blinker_handler) {};
    struct PlannerPullHandler : public AMBRO_WFUNC_TD(&PrinterMain::planner_pull_handler) {};
    struct PlannerFinishedHandler : public AMBRO_WFUNC_TD(&PrinterMain::planner_finished_handler) {};
    struct PlannerAbortedHandler : public AMBRO_WFUNC_TD(&PrinterMain::planner_aborted_handler) {};
    struct PlannerUnderrunCallback : public AMBRO_WFUNC_TD(&PrinterMain::planner_underrun_callback) {};
    struct PlannerChannelCallback : public AMBRO_WFUNC_TD(&PrinterMain::planner_channel_callback) {};
    template <int AxisIndex> struct PlannerPrestepCallback : public AMBRO_WFUNC_TD(&PrinterMain::template planner_prestep_callback<AxisIndex>) {};
    template <int AxisIndex> struct AxisDriverConsumersList {
        using List = JoinTypeLists<
            MakeTypeList<typename ThePlanner::template TheAxisDriverConsumer<AxisIndex>>,
            typename Axis<AxisIndex>::HomingFeature::AxisDriverConsumersList
        >;
    };
    struct DelayedConfigExprs {
        using List = JoinTypeLists<
            MyConfigExprs,
            ObjCollect<
                JoinTypeLists<
                    AxesList,
                    LasersList,
                    HeatersList,
                    MakeTypeList<
                        TheSteppers,
                        TransformFeature,
                        ProbeFeature,
                        CurrentFeature,
                        PlannerUnion
                    >
                >,
                Collectible_ConfigExprs
            >
        >;
    };
    
public:
    struct Object : public ObjBase<PrinterMain, ParentObject, JoinTypeLists<
        AxesList,
        LasersList,
        HeatersList,
        FansList,
        MakeTypeList<
            TheDebugObject,
            TheWatchdog,
            TheConfigManager,
            TheConfigCache,
            TheBlinker,
            TheSteppers,
            SerialFeature,
            SdCardFeature,
            TransformFeature,
            ProbeFeature,
            CurrentFeature,
            PlannerUnion
        >
    >> {
        typename Loop::QueuedEvent unlocked_timer;
        typename Loop::QueuedEvent disable_timer;
        typename Loop::QueuedEvent force_timer;
        FpType time_freq_by_max_speed;
        uint32_t underrun_count;
        bool locked;
        uint8_t planner_state;
        PlannerClient *planner_client;
        bool m_planning_pull_pending;
        PhysVirtAxisMaskType m_homing_rem_axes;
        bool move_seen_cartesian;
    };
};

#include <aprinter/EndNamespace.h>

#endif
