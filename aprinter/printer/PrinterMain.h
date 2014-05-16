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
#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/WrapDouble.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/TypeListLength.h>
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
#include <aprinter/meta/TypeListIndex.h>
#include <aprinter/meta/ComposeFunctions.h>
#include <aprinter/meta/IsEqualFunc.h>
#include <aprinter/meta/FilterTypeList.h>
#include <aprinter/meta/NotFunc.h>
#include <aprinter/meta/PowerOfTwo.h>
#include <aprinter/meta/Object.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/WrapType.h>
#include <aprinter/meta/TupleForEach.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Likely.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/devices/Blinker.h>
#include <aprinter/devices/SoftPwm.h>
#include <aprinter/driver/Steppers.h>
#include <aprinter/driver/AxisDriver.h>
#include <aprinter/printer/AxisHomer.h>
#include <aprinter/printer/GcodeParser.h>
#include <aprinter/printer/BinaryGcodeParser.h>
#include <aprinter/printer/MotionPlanner.h>
#include <aprinter/printer/TemperatureObserver.h>

#include <aprinter/BeginNamespace.h>

template <
    typename TSerial, typename TLedPin, typename TLedBlinkInterval, typename TDefaultInactiveTime,
    typename TSpeedLimitMultiply, typename TMaxStepsPerCycle,
    int TStepperSegmentBufferSize, int TEventChannelBufferSize, int TLookaheadBufferSize,
    int TLookaheadCommitCount,
    typename TForceTimeout, typename TFpType,
    typename TEventChannelTimerService,
    template <typename, typename, typename> class TWatchdogTemplate, typename TWatchdogParams,
    typename TSdCardParams, typename TProbeParams, typename TCurrentParams,
    typename TAxesList, typename TTransformParams, typename THeatersList, typename TFansList,
    typename TLasersList = EmptyTypeList
>
struct PrinterMainParams {
    using Serial = TSerial;
    using LedPin = TLedPin;
    using LedBlinkInterval = TLedBlinkInterval;
    using DefaultInactiveTime = TDefaultInactiveTime;
    using SpeedLimitMultiply = TSpeedLimitMultiply;
    using MaxStepsPerCycle = TMaxStepsPerCycle;
    static int const StepperSegmentBufferSize = TStepperSegmentBufferSize;
    static int const EventChannelBufferSize = TEventChannelBufferSize;
    static int const LookaheadBufferSize = TLookaheadBufferSize;
    static int const LookaheadCommitCount = TLookaheadCommitCount;
    using ForceTimeout = TForceTimeout;
    using FpType = TFpType;
    using EventChannelTimerService = TEventChannelTimerService;
    template <typename X, typename Y, typename Z> using WatchdogTemplate = TWatchdogTemplate<X, Y, Z>;
    using WatchdogParams = TWatchdogParams;
    using SdCardParams = TSdCardParams;
    using ProbeParams = TProbeParams;
    using CurrentParams = TCurrentParams;
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
    template <typename, typename, int, int, typename, typename, typename> class TSerialTemplate,
    typename TSerialParams
>
struct PrinterMainSerialParams {
    static uint32_t const Baud = TBaud;
    static int const RecvBufferSizeExp = TRecvBufferSizeExp;
    static int const SendBufferSizeExp = TSendBufferSizeExp;
    using TheGcodeParserParams = TTheGcodeParserParams;
    template <typename S, typename X, int Y, int Z, typename W, typename Q, typename R> using SerialTemplate = TSerialTemplate<S, X, Y, Z, W, Q, R>;
    using SerialParams = TSerialParams;
};

template <
    char TName,
    typename TDirPin, typename TStepPin, typename TEnablePin, bool TInvertDir,
    typename TDefaultStepsPerUnit, typename TDefaultMin, typename TDefaultMax,
    typename TDefaultMaxSpeed, typename TDefaultMaxAccel,
    typename TDefaultDistanceFactor, typename TDefaultCorneringDistance,
    typename THoming, bool TIsCartesian, int TStepBits,
    typename TTheAxisDriverParams, typename TMicroStep
>
struct PrinterMainAxisParams {
    static char const Name = TName;
    using DirPin = TDirPin;
    using StepPin = TStepPin;
    using EnablePin = TEnablePin;
    static bool const InvertDir = TInvertDir;
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
    using TheAxisDriverParams = TTheAxisDriverParams;
    using MicroStep = TMicroStep;
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
    typename TEndPin, typename TEndPinInputMode, bool TEndInvert, bool THomeDir,
    typename TDefaultFastMaxDist, typename TDefaultRetractDist, typename TDefaultSlowMaxDist,
    typename TDefaultFastSpeed, typename TDefaultRetractSpeed, typename TDefaultSlowSpeed
>
struct PrinterMainHomingParams {
    static bool const Enabled = true;
    using EndPin = TEndPin;
    using EndPinInputMode = TEndPinInputMode;
    static bool const EndInvert = TEndInvert;
    static bool const HomeDir = THomeDir;
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
    typename TSegmentsPerSecond,
    template<typename, typename> class TTransformAlg, typename TTransformAlgParams
>
struct PrinterMainTransformParams {
    static bool const Enabled = true;
    using VirtAxesList = TVirtAxesList;
    using PhysAxesList = TPhysAxesList;
    using SegmentsPerSecond = TSegmentsPerSecond;
    template <typename X, typename Y> using TransformAlg = TTransformAlg<X, Y>;
    using TransformAlgParams = TTransformAlgParams;
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
    typename TEndPin, typename TEndPinInputMode, bool TEndInvert, bool THomeDir,
    typename TFastExtraDist, typename TRetractDist, typename TSlowExtraDist,
    typename TFastSpeed, typename TRetractSpeed, typename TSlowSpeed
>
struct PrinterMainVirtualHomingParams {
    static bool const Enabled = true;
    using EndPin = TEndPin;
    using EndPinInputMode = TEndPinInputMode;
    static bool const EndInvert = TEndInvert;
    static bool const HomeDir = THomeDir;
    using FastExtraDist = TFastExtraDist;
    using RetractDist = TRetractDist;
    using SlowExtraDist = TSlowExtraDist;
    using FastSpeed = TFastSpeed;
    using RetractSpeed = TRetractSpeed;
    using SlowSpeed = TSlowSpeed;
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
    typename TTimerService
>
struct PrinterMainHeaterParams {
    static char const Name = TName;
    static int const SetMCommand = TSetMCommand;
    static int const WaitMCommand = TWaitMCommand;
    static int const SetConfigMCommand = TSetConfigMCommand;
    using AdcPin = TAdcPin;
    using OutputPin = TOutputPin;
    static bool const OutputInvert = TOutputInvert;
    using Formula = TFormula;
    using MinSafeTemp = TMinSafeTemp;
    using MaxSafeTemp = TMaxSafeTemp;
    using PulseInterval = TPulseInterval;
    using ControlInterval = TControlInterval;
    template <typename X, typename Y, typename Z> using Control = TControl<X, Y, Z>;
    using ControlParams = TControlParams;
    using TheTemperatureObserverParams = TTheTemperatureObserverParams;
    using TimerService = TTimerService;
};

template <
    int TSetMCommand, int TOffMCommand,
    typename TOutputPin, bool TOutputInvert, typename TPulseInterval, typename TSpeedMultiply,
    typename TTimerService
>
struct PrinterMainFanParams {
    static int const SetMCommand = TSetMCommand;
    static int const OffMCommand = TOffMCommand;
    using OutputPin = TOutputPin;
    static bool const OutputInvert = TOutputInvert;
    using PulseInterval = TPulseInterval;
    using SpeedMultiply = TSpeedMultiply;
    using TimerService = TTimerService;
};

struct PrinterMainNoSdCardParams {
    static bool const Enabled = false;
};

template <
    template<typename, typename, typename, int, typename, typename> class TSdCard,
    typename TSdCardParams,
    template<typename, typename, typename, typename> class TGcodeParserTemplate,
    typename TTheGcodeParserParams, int TReadBufferBlocks,
    int TMaxCommandSize
>
struct PrinterMainSdCardParams {
    static bool const Enabled = true;
    template <typename X, typename Y, typename Z, int R, typename W, typename Q> using SdCard = TSdCard<X, Y, Z, R, W, Q>;
    using SdCardParams = TSdCardParams;
    template <typename X, typename Y, typename Z, typename W> using GcodeParserTemplate = TGcodeParserTemplate<X, Y, Z, W>;
    using TheGcodeParserParams = TTheGcodeParserParams;
    static int const ReadBufferBlocks = TReadBufferBlocks;
    static int const MaxCommandSize = TMaxCommandSize;
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
    template<typename, typename, typename, typename> class TCurrentTemplate,
    typename TCurrentParams
>
struct PrinterMainCurrentParams {
    static bool const Enabled = true;
    using CurrentAxesList = TCurrentAxesList;
    template <typename X, typename Y, typename Z, typename W> using CurrentTemplate = TCurrentTemplate<X, Y, Z, W>;
    using CurrentParams = TCurrentParams;
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
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_finish_init, finish_init)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_continue_splitclear_helper, continue_splitclear_helper)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_report_height, report_height)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_finish_locked_helper, finish_locked_helper)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_continue_locking_helper, continue_locking_helper)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_continue_planned_helper, continue_planned_helper)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_continue_unplanned_helper, continue_unplanned_helper)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_emergency, emergency)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_start_homing, start_homing)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_start_virt_homing, start_virt_homing)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_prestep_callback, prestep_callback)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_update_homing_mask, update_homing_mask)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_enable_stepper, enable_stepper)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_disable_stepper, disable_stepper)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_do_move, do_move)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_limit_axis_move_speed, limit_axis_move_speed)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_fix_aborted_pos, fix_aborted_pos)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_m119_append_endstop, m119_append_endstop)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_append_value, append_value)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_append_adc_value, append_adc_value)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_check_command, check_command)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_channel_callback, channel_callback)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_print_config, print_config)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_run_for_state_command, run_for_state_command)
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
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(TForeach_init, init)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_ChannelPayload, ChannelPayload)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_EventLoopFastEvents, EventLoopFastEvents)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_WrappedAxisName, WrappedAxisName)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_WrappedPhysAxisIndex, WrappedPhysAxisIndex)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_HomingFeature, HomingFeature)
    
    struct PlannerUnionPlanner;
    struct PlannerUnionHoming;
    struct BlinkerHandler;
    struct PlannerPullHandler;
    struct PlannerFinishedHandler;
    struct PlannerAbortedHandler;
    struct PlannerUnderrunCallback;
    struct PlannerChannelCallback;
    template <int AxisIndex> struct PlannerPrestepCallback;
    template <int AxisIndex> struct AxisDriverConsumersList;
    
    using Loop = typename Context::EventLoop;
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using FpType = typename Params::FpType;
    using ParamsAxesList = typename Params::AxesList;
    using ParamsLasersList = typename Params::LasersList;
    using TransformParams = typename Params::TransformParams;
    using ParamsHeatersList = typename Params::HeatersList;
    using ParamsFansList = typename Params::FansList;
    static const int NumAxes = TypeListLength<ParamsAxesList>::value;
    
    template <typename TheAxis>
    using MakeStepperDef = StepperDef<
        typename TheAxis::DirPin,
        typename TheAxis::StepPin,
        typename TheAxis::EnablePin,
        TheAxis::InvertDir
    >;
    
    using TheWatchdog = typename Params::template WatchdogTemplate<Context, Object, typename Params::WatchdogParams>;
    using TheBlinker = Blinker<Context, Object, typename Params::LedPin, BlinkerHandler>;
    using StepperDefsList = MapTypeList<ParamsAxesList, TemplateFunc<MakeStepperDef>>;
    using TheSteppers = Steppers<Context, Object, StepperDefsList>;
    
    static_assert(Params::LedBlinkInterval::value() < TheWatchdog::WatchdogTime / 2.0, "");
    
    enum {COMMAND_IDLE, COMMAND_LOCKING, COMMAND_LOCKED};
    enum {PLANNER_NONE, PLANNER_RUNNING, PLANNER_STOPPING, PLANNER_WAITING, PLANNER_CUSTOM};
    
    struct MoveBuildState;
    struct SetPositionState;
    
    template <typename ChannelParentObject, typename Channel>
    struct ChannelCommon {
        struct Object;
        using TheGcodeParser = typename Channel::TheGcodeParser;
        using GcodePartsSizeType = typename TheGcodeParser::PartsSizeType;
        using GcodeParserPartRef = typename TheGcodeParser::PartRef;
        
        // channel interface
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            o->m_state = COMMAND_IDLE;
            o->m_cmd = false;
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
                reply_append_pstr(c, AMBRO_PSTR("Error:"));
                reply_append_pstr(c, err);
                reply_append_ch(c, '\n');
                return finishCommand(c);
            }
            if (!Channel::start_command_impl(c)) {
                return finishCommand(c);
            }
            work_command(c, WrapType<ChannelCommon>());
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
        
        static void finishCommand (Context c, bool no_ok = false)
        {
            auto *o = Object::self(c);
            auto *mob = PrinterMain::Object::self(c);
            AMBRO_ASSERT(o->m_cmd)
            AMBRO_ASSERT(o->m_state == COMMAND_IDLE || o->m_state == COMMAND_LOCKED)
            
            Channel::finish_command_impl(c, no_ok);
            o->m_cmd = false;
            if (o->m_state == COMMAND_LOCKED) {
                AMBRO_ASSERT(mob->locked)
                o->m_state = COMMAND_IDLE;
                mob->locked = false;
                if (!mob->unlocked_timer.isSet(c)) {
                    mob->unlocked_timer.prependNowNotAlready(c);
                }
            }
        }
        
        // command interface
        
        static bool tryLockedCommand (Context c)
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
            mob->locked = true;
            return true;
        }
        
        static bool tryUnplannedCommand (Context c)
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
        
        static bool tryPlannedCommand (Context c)
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
        
        static bool trySplitClearCommand (Context c)
        {
            if (!tryLockedCommand(c)) {
                return false;
            }
            return TransformFeature::try_splitclear_command(c);
        }
        
        static bool find_command_param (Context c, char code, GcodeParserPartRef *out_part)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->m_cmd)
            AMBRO_ASSERT(code >= 'A')
            AMBRO_ASSERT(code <= 'Z')
            
            auto num_parts = TheGcodeParser::getNumParts(c);
            for (GcodePartsSizeType i = 0; i < num_parts; i++) {
                GcodeParserPartRef part = TheGcodeParser::getPart(c, i);
                if (TheGcodeParser::getPartCode(c, part) == code) {
                    *out_part = part;
                    return true;
                }
            }
            return false;
        }
        
        static uint32_t get_command_param_uint32 (Context c, char code, uint32_t default_value)
        {
            GcodeParserPartRef part;
            if (!find_command_param(c, code, &part)) {
                return default_value;
            }
            return TheGcodeParser::getPartUint32Value(c, part);
        }
        
        static FpType get_command_param_fp (Context c, char code, FpType default_value)
        {
            GcodeParserPartRef part;
            if (!find_command_param(c, code, &part)) {
                return default_value;
            }
            return TheGcodeParser::template getPartFpValue<FpType>(c, part);
        }
        
        static bool find_command_param_fp (Context c, char code, FpType *out)
        {
            GcodeParserPartRef part;
            if (!find_command_param(c, code, &part)) {
                return false;
            }
            *out = TheGcodeParser::template getPartFpValue<FpType>(c, part);
            return true;
        }
        
        static void reply_poke (Context c)
        {
            Channel::reply_poke_impl(c);
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
        
        static void reply_append_fp (Context c, FpType x)
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
        
        static void reply_append_uint16 (Context c, uint16_t x)
        {
            char buf[6];
#if defined(AMBROLIB_AVR)
            uint8_t len = AMBRO_PGM_SPRINTF(buf, AMBRO_PSTR("%" PRIu16), x);
#else
            uint8_t len = PrintNonnegativeIntDecimal<uint16_t>(x, buf);
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
        
        template <typename Class, typename Func, typename... Args>
        static bool run_for_state_command (Context c, uint8_t state, WrapType<Class>, Func func, Args... args)
        {
            auto *o = Object::self(c);
            
            if (o->m_state == state) {
                func(WrapType<Class>(), c, WrapType<ChannelCommon>(), args...);
                return false;
            }
            return true;
        }
        
        struct Object : public ObjBase<ChannelCommon, ChannelParentObject, EmptyTypeList> {
            uint8_t m_state;
            bool m_cmd;
        };
    };
    
    struct SerialFeature {
        struct Object;
        struct SerialRecvHandler;
        struct SerialSendHandler;
        
        using TheSerial = typename Params::Serial::template SerialTemplate<Context, Object, Params::Serial::RecvBufferSizeExp, Params::Serial::SendBufferSizeExp, typename Params::Serial::SerialParams, SerialRecvHandler, SerialSendHandler>;
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
        }
        
        static bool start_command_impl (Context c)
        {
            auto *o = Object::self(c);
            auto *cco = TheChannelCommon::Object::self(c);
            AMBRO_ASSERT(cco->m_cmd)
            
            bool is_m110 = (TheGcodeParser::getCmdCode(c) == 'M' && TheGcodeParser::getCmdNumber(c) == 110);
            if (is_m110) {
                o->m_line_number = TheChannelCommon::get_command_param_uint32(c, 'L', (TheGcodeParser::getCmd(c)->have_line_number ? TheGcodeParser::getCmd(c)->line_number : -1));
            }
            if (TheGcodeParser::getCmd(c)->have_line_number) {
                if (TheGcodeParser::getCmd(c)->line_number != o->m_line_number) {
                    TheChannelCommon::reply_append_pstr(c, AMBRO_PSTR("Error:Line Number is not Last Line Number+1, Last Line:"));
                    TheChannelCommon::reply_append_uint32(c, (uint32_t)(o->m_line_number - 1));
                    TheChannelCommon::reply_append_ch(c, '\n');
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
                TheChannelCommon::reply_append_pstr(c, AMBRO_PSTR("ok\n"));
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
        struct SdCardInitHandler;
        struct SdCardCommandHandler;
        
        static const int ReadBufferBlocks = Params::SdCardParams::ReadBufferBlocks;
        static const int MaxCommandSize = Params::SdCardParams::MaxCommandSize;
        static const size_t BlockSize = 512;
        static_assert(ReadBufferBlocks >= 2, "");
        static_assert(MaxCommandSize < BlockSize, "");
        static const size_t BufferBaseSize = ReadBufferBlocks * BlockSize;
        using ParserSizeType = ChooseInt<BitsInInt<MaxCommandSize>::value, false>;
        using TheSdCard = typename Params::SdCardParams::template SdCard<Context, Object, typename Params::SdCardParams::SdCardParams, 1, SdCardInitHandler, SdCardCommandHandler>;
        using TheGcodeParser = typename Params::SdCardParams::template GcodeParserTemplate<Context, Object, typename Params::SdCardParams::TheGcodeParserParams, ParserSizeType>;
        using SdCardReadState = typename TheSdCard::ReadState;
        using TheChannelCommon = ChannelCommon<Object, SdCardFeature>;
        enum {SDCARD_NONE, SDCARD_INITING, SDCARD_INITED, SDCARD_RUNNING, SDCARD_PAUSING};
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            TheSdCard::init(c);
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
            TheSdCard::deinit(c);
        }
        
        template <typename CommandChannel>
        static void finish_init (Context c, WrapType<CommandChannel>, uint8_t error_code)
        {
            auto *o = Object::self(c);
            
            if (error_code) {
                CommandChannel::reply_append_pstr(c, AMBRO_PSTR("SD error "));
                CommandChannel::reply_append_uint8(c, error_code);
            } else {
                CommandChannel::reply_append_pstr(c, AMBRO_PSTR("SD blocks "));
                CommandChannel::reply_append_uint32(c, TheSdCard::getCapacityBlocks(c));
            }
            CommandChannel::reply_append_ch(c, '\n');
            CommandChannel::finishCommand(c);
        }
        
        static void sd_card_init_handler (Context c, uint8_t error_code)
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
                o->m_cmd_offset = 0;
                o->m_sd_block = 0;
            }
            ListForEachForwardInterruptible<ChannelCommonList>(LForeach_run_for_state_command(), c, COMMAND_LOCKED, WrapType<SdCardFeature>(), LForeach_finish_init(), error_code);
        }
        
        static void sd_card_command_handler (Context c)
        {
            auto *o = Object::self(c);
            auto *co = TheChannelCommon::Object::self(c);
            AMBRO_ASSERT(o->m_state == SDCARD_RUNNING || o->m_state == SDCARD_PAUSING)
            AMBRO_ASSERT(o->m_length < BufferBaseSize)
            AMBRO_ASSERT(o->m_sd_block < TheSdCard::getCapacityBlocks(c))
            
            bool error;
            if (!TheSdCard::checkReadBlock(c, &o->m_read_state, &error)) {
                return;
            }
            TheSdCard::unsetEvent(c);
            if (o->m_state == SDCARD_PAUSING) {
                o->m_state = SDCARD_INITED;
                return finish_locked(c);
            }
            if (error) {
                SerialFeature::TheChannelCommon::reply_append_pstr(c, AMBRO_PSTR("//SdRdEr\n"));
                SerialFeature::TheChannelCommon::reply_poke(c);
                return start_read(c);
            }
            o->m_sd_block++;
            if (o->m_length == BufferBaseSize - o->m_start) {
                memcpy(o->m_buffer + BufferBaseSize, o->m_buffer, MaxCommandSize - 1);
            }
            o->m_length += BlockSize;
            if (o->m_length < BufferBaseSize && o->m_sd_block < TheSdCard::getCapacityBlocks(c)) {
                start_read(c);
            }
            if (!co->m_cmd && !o->m_eof) {
                o->m_next_event.prependNowNotAlready(c);
            }
        }
        
        static void next_event_handler (typename Loop::QueuedEvent *, Context c)
        {
            auto *o = Object::self(c);
            auto *co = TheChannelCommon::Object::self(c);
            AMBRO_ASSERT(o->m_state == SDCARD_RUNNING)
            AMBRO_ASSERT(!co->m_cmd)
            AMBRO_ASSERT(!o->m_eof)
            
            AMBRO_PGM_P eof_str;
            if (!TheGcodeParser::haveCommand(c)) {
                TheGcodeParser::startCommand(c, (char *)buf_get(c, o->m_start, o->m_cmd_offset), 0);
            }
            ParserSizeType avail = (o->m_length - o->m_cmd_offset > MaxCommandSize) ? MaxCommandSize : (o->m_length - o->m_cmd_offset);
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
            if (o->m_sd_block == TheSdCard::getCapacityBlocks(c)) {
                eof_str = AMBRO_PSTR("//SdEnd\n");
                goto eof;
            }
            return;
        eof:
            SerialFeature::TheChannelCommon::reply_append_pstr(c, eof_str);
            SerialFeature::TheChannelCommon::reply_poke(c);
            o->m_eof = true;
        }
        
        template <typename CommandChannel>
        static bool check_command (Context c, WrapType<CommandChannel>)
        {
            auto *o = Object::self(c);
            
            if (TypesAreEqual<CommandChannel, TheChannelCommon>::value) {
                return true;
            }
            if (CommandChannel::TheGcodeParser::getCmdNumber(c) == 21) {
                if (!CommandChannel::tryUnplannedCommand(c)) {
                    return false;
                }
                if (o->m_state != SDCARD_NONE) {
                    CommandChannel::finishCommand(c);
                    return false;
                }
                TheSdCard::activate(c);
                o->m_state = SDCARD_INITING;
                return false;
            }
            if (CommandChannel::TheGcodeParser::getCmdNumber(c) == 22) {
                if (!CommandChannel::tryUnplannedCommand(c)) {
                    return false;
                }
                CommandChannel::finishCommand(c);
                AMBRO_ASSERT(o->m_state != SDCARD_INITING)
                AMBRO_ASSERT(o->m_state != SDCARD_PAUSING)
                if (o->m_state == SDCARD_NONE) {
                    return false;
                }
                TheGcodeParser::deinit(c);
                o->m_state = SDCARD_NONE;
                o->m_next_event.unset(c);
                TheChannelCommon::maybeCancelLockingCommand(c);
                TheSdCard::deactivate(c);
                return false;
            }
            if (CommandChannel::TheGcodeParser::getCmdNumber(c) == 24) {
                if (!CommandChannel::tryUnplannedCommand(c)) {
                    return false;
                }
                CommandChannel::finishCommand(c);
                if (o->m_state != SDCARD_INITED) {
                    return false;
                }
                o->m_state = SDCARD_RUNNING;
                o->m_eof = false;
                if (o->m_length < BufferBaseSize && o->m_sd_block < TheSdCard::getCapacityBlocks(c)) {
                    start_read(c);
                }
                if (!TheChannelCommon::maybeResumeLockingCommand(c)) {
                    o->m_next_event.prependNowNotAlready(c);
                }
                return false;
            }
            if (CommandChannel::TheGcodeParser::getCmdNumber(c) == 25) {
                if (!CommandChannel::tryUnplannedCommand(c)) {
                    return false;
                }
                if (o->m_state != SDCARD_RUNNING) {
                    CommandChannel::finishCommand(c);
                    return false;
                }
                o->m_next_event.unset(c);
                TheChannelCommon::maybePauseLockingCommand(c);
                if (o->m_length < BufferBaseSize && o->m_sd_block < TheSdCard::getCapacityBlocks(c)) {
                    o->m_state = SDCARD_PAUSING;
                } else {
                    o->m_state = SDCARD_INITED;
                    CommandChannel::finishCommand(c);
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
            AMBRO_ASSERT(!o->m_eof)
            AMBRO_ASSERT(TheGcodeParser::getLength(c) <= o->m_length - o->m_cmd_offset)
            
            o->m_next_event.prependNowNotAlready(c);
            o->m_cmd_offset += TheGcodeParser::getLength(c);
            if (o->m_cmd_offset >= BlockSize) {
                o->m_start += BlockSize;
                if (o->m_start == BufferBaseSize) {
                    o->m_start = 0;
                }
                o->m_length -= BlockSize;
                o->m_cmd_offset -= BlockSize;
                if (o->m_length == BufferBaseSize - BlockSize && o->m_sd_block < TheSdCard::getCapacityBlocks(c)) {
                    start_read(c);
                }
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
        
        static uint8_t * buf_get (Context c, size_t start, size_t count)
        {
            auto *o = Object::self(c);
            
            static_assert(BufferBaseSize <= SIZE_MAX / 2, "");
            size_t x = start + count;
            if (x >= BufferBaseSize) {
                x -= BufferBaseSize;
            }
            return o->m_buffer + x;
        }
        
        static void start_read (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->m_length < BufferBaseSize)
            AMBRO_ASSERT(o->m_sd_block < TheSdCard::getCapacityBlocks(c))
            
            TheSdCard::queueReadBlock(c, o->m_sd_block, buf_get(c, o->m_start, o->m_length), &o->m_read_state);
        }
        
        struct SdCardInitHandler : public AMBRO_WFUNC_TD(&SdCardFeature::sd_card_init_handler) {};
        struct SdCardCommandHandler : public AMBRO_WFUNC_TD(&SdCardFeature::sd_card_command_handler) {};
        
        using EventLoopFastEvents = typename TheSdCard::EventLoopFastEvents;
        using SdChannelCommonList = MakeTypeList<TheChannelCommon>;
        
        struct Object : public ObjBase<SdCardFeature, typename PrinterMain::Object, MakeTypeList<
            TheSdCard,
            TheChannelCommon,
            TheGcodeParser
        >> {
            typename Loop::QueuedEvent m_next_event;
            uint8_t m_state;
            SdCardReadState m_read_state;
            size_t m_start;
            size_t m_length;
            size_t m_cmd_offset;
            bool m_eof;
            uint32_t m_sd_block;
            uint8_t m_buffer[BufferBaseSize + (MaxCommandSize - 1)];
        };
    } AMBRO_STRUCT_ELSE(SdCardFeature) {
        static void init (Context c) {}
        static void deinit (Context c) {}
        template <typename TheChannelCommon>
        static bool check_command (Context c, WrapType<TheChannelCommon>) { return true; }
        using EventLoopFastEvents = EmptyTypeList;
        using SdChannelCommonList = EmptyTypeList;
        struct Object {};
    };
    
    using ChannelCommonList = JoinTypeLists<
        MakeTypeList<typename SerialFeature::TheChannelCommon>,
        typename SdCardFeature::SdChannelCommonList
    >;
    
    template <typename TheAxis, bool Enabled = TheAxis::HomingSpec::Enabled>
    struct HomingHelper {
        static void init (Context c) {}
        template <typename TheChannelCommon>
        static void m119_append_endstop (Context c, WrapType<TheChannelCommon>) {}
    };
    
    template <typename TheAxis>
    struct HomingHelper<TheAxis, true> {
        using HomingSpec = typename TheAxis::HomingSpec;
        
        static void init (Context c)
        {
            Context::Pins::template setInput<typename HomingSpec::EndPin, typename HomingSpec::EndPinInputMode>(c);
        }
        
        template <typename TheChannelCommon>
        static void m119_append_endstop (Context c, WrapType<TheChannelCommon>)
        {
            bool triggered = endstop_is_triggered(c);
            TheChannelCommon::reply_append_ch(c, ' ');
            TheChannelCommon::reply_append_ch(c, TheAxis::AxisName);
            TheChannelCommon::reply_append_ch(c, ':');
            TheChannelCommon::reply_append_ch(c, (triggered ? '1' : '0'));
        }
        
        template <typename TheContext>
        static bool endstop_is_triggered (TheContext c)
        {
            return (Context::Pins::template get<typename HomingSpec::EndPin>(c) != HomingSpec::EndInvert);
        }
    };
    
    template <int TAxisIndex>
    struct Axis {
        struct Object;
        static const int AxisIndex = TAxisIndex;
        using AxisSpec = TypeListGet<ParamsAxesList, AxisIndex>;
        using Stepper = typename TheSteppers::template Stepper<AxisIndex>;
        using TheAxisDriver = AxisDriver<Context, Object, typename AxisSpec::TheAxisDriverParams, Stepper, AxisDriverConsumersList<AxisIndex>>;
        using StepFixedType = FixedPoint<AxisSpec::StepBits, false, 0>;
        using AbsStepFixedType = FixedPoint<AxisSpec::StepBits - 1, true, 0>;
        static const char AxisName = AxisSpec::Name;
        using WrappedAxisName = WrapInt<AxisName>;
        using HomingSpec = typename AxisSpec::Homing;
        using TheHomingHelper = HomingHelper<Axis>;
        
        template <typename ThePrinterMain = PrinterMain>
        struct Lazy {
            static typename ThePrinterMain::PhysVirtAxisMaskType const AxisMask = (typename ThePrinterMain::PhysVirtAxisMaskType)1 << AxisIndex;
        };
        
        AMBRO_STRUCT_IF(HomingFeature, HomingSpec::Enabled) {
            struct Object;
            struct HomerFinishedHandler;
            
            using Homer = AxisHomer<
                Context, Object, TheAxisDriver, AxisSpec::StepBits,
                typename AxisSpec::DefaultDistanceFactor, typename AxisSpec::DefaultCorneringDistance,
                Params::StepperSegmentBufferSize, Params::LookaheadBufferSize, FpType,
                typename HomingSpec::EndPin,
                HomingSpec::EndInvert, HomingSpec::HomeDir, HomerFinishedHandler
            >;
            
            template <typename TheHomingFeature>
            using MakeAxisDriverConsumersList = MakeTypeList<typename TheHomingFeature::Homer::TheAxisDriverConsumer>;
            
            using EventLoopFastEvents = typename Homer::EventLoopFastEvents;
            
            static void deinit (Context c)
            {
                auto *axis = Axis::Object::self(c);
                if (axis->m_state == AXIS_STATE_HOMING) {
                    Homer::deinit(c);
                }
            }
            
            static void start_phys_homing (Context c)
            {
                auto *axis = Axis::Object::self(c);
                auto *mob = PrinterMain::Object::self(c);
                AMBRO_ASSERT(axis->m_state == AXIS_STATE_OTHER)
                AMBRO_ASSERT(mob->m_homing_rem_axes & Lazy<>::AxisMask)
                
                typename Homer::HomingParams params;
                params.fast_max_dist = StepFixedType::template importFpSaturatedRound<FpType>(dist_from_real((FpType)HomingSpec::DefaultFastMaxDist::value()));
                params.retract_dist = StepFixedType::template importFpSaturatedRound<FpType>(dist_from_real((FpType)HomingSpec::DefaultRetractDist::value()));
                params.slow_max_dist = StepFixedType::template importFpSaturatedRound<FpType>(dist_from_real((FpType)HomingSpec::DefaultSlowMaxDist::value()));
                params.fast_speed = speed_from_real((FpType)HomingSpec::DefaultFastSpeed::value());
                params.retract_speed = speed_from_real((FpType)HomingSpec::DefaultRetractSpeed::value());
                params.slow_speed = speed_from_real((FpType)HomingSpec::DefaultSlowSpeed::value());
                params.max_accel = accel_from_real((FpType)AxisSpec::DefaultMaxAccel::value());
                
                Stepper::enable(c);
                Homer::init(c, params);
                axis->m_state = AXIS_STATE_HOMING;
            }
            
            static FpType init_position ()
            {
                return HomingSpec::HomeDir ? max_req_pos() : min_req_pos();
            };
            
            static void homer_finished_handler (Context c, bool success)
            {
                auto *axis = Axis::Object::self(c);
                auto *mob = PrinterMain::Object::self(c);
                AMBRO_ASSERT(axis->m_state == AXIS_STATE_HOMING)
                AMBRO_ASSERT(mob->locked)
                AMBRO_ASSERT(mob->m_homing_rem_axes & Lazy<>::AxisMask)
                
                Homer::deinit(c);
                axis->m_req_pos = init_position();
                axis->m_end_pos = AbsStepFixedType::template importFpSaturatedRound<FpType>(dist_from_real(axis->m_req_pos));
                axis->m_state = AXIS_STATE_OTHER;
                TransformFeature::template mark_phys_moved<AxisIndex>(c);
                mob->m_homing_rem_axes &= ~Lazy<>::AxisMask;
                if (!(mob->m_homing_rem_axes & PhysAxisMask)) {
                    phys_homing_finished(c);
                }
            }
            
            struct HomerFinishedHandler : public AMBRO_WFUNC_TD(&HomingFeature::homer_finished_handler) {};
            
            struct Object : public ObjBase<HomingFeature, typename PlannerUnionHoming::Object, MakeTypeList<
                Homer
            >> {};
        } AMBRO_STRUCT_ELSE(HomingFeature) {
            template <typename TheHomingFeature>
            using MakeAxisDriverConsumersList = MakeTypeList<>;
            using EventLoopFastEvents = EmptyTypeList;
            static void deinit (Context c) {}
            static void start_phys_homing (Context c) {}
            static FpType init_position () { return 0.0f; }
            struct Object {};
        };
        
        AMBRO_STRUCT_IF(MicroStepFeature, AxisSpec::MicroStep::Enabled) {
            struct Object;
            using MicroStep = typename AxisSpec::MicroStep::template MicroStepTemplate<Context, Object, typename AxisSpec::MicroStep::MicroStepParams>;
            
            static void init (Context c)
            {
                MicroStep::init(c, AxisSpec::MicroStep::MicroSteps);
            }
            
            struct Object : public ObjBase<MicroStepFeature, typename Axis::Object, MakeTypeList<
                MicroStep
            >>
            {};
        } AMBRO_STRUCT_ELSE(MicroStepFeature) {
            static void init (Context c) {}
            struct Object {};
        };
        
        enum {AXIS_STATE_OTHER, AXIS_STATE_HOMING};
        
        static FpType dist_from_real (FpType x)
        {
            return (x * (FpType)AxisSpec::DefaultStepsPerUnit::value());
        }
        
        static FpType dist_to_real (FpType x)
        {
            return (x * (FpType)(1.0 / AxisSpec::DefaultStepsPerUnit::value()));
        }
        
        static FpType speed_from_real (FpType v)
        {
            return (v * (FpType)(AxisSpec::DefaultStepsPerUnit::value() / Clock::time_freq));
        }
        
        static FpType accel_from_real (FpType a)
        {
            return (a * (FpType)(AxisSpec::DefaultStepsPerUnit::value() / (Clock::time_freq * Clock::time_freq)));
        }
        
        static FpType clamp_req_pos (FpType req)
        {
            return FloatMax(min_req_pos(), FloatMin(max_req_pos(), req));
        }
        
        static FpType min_req_pos ()
        {
            return FloatMax((FpType)AxisSpec::DefaultMin::value(), dist_to_real((FpType)AbsStepFixedType::minValue().template fpValue<FpType>()));
        }
        
        static FpType max_req_pos ()
        {
            return FloatMin((FpType)AxisSpec::DefaultMax::value(), dist_to_real((FpType)AbsStepFixedType::maxValue().template fpValue<FpType>()));
        }
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            TheAxisDriver::init(c);
            o->m_state = AXIS_STATE_OTHER;
            TheHomingHelper::init(c);
            MicroStepFeature::init(c);
            o->m_req_pos = HomingFeature::init_position();
            o->m_end_pos = AbsStepFixedType::template importFpSaturatedRound<FpType>(dist_from_real(o->m_req_pos));
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
        
        static void enable_stepper (Context c)
        {
            Stepper::enable(c);
        }
        
        static void disable_stepper (Context c)
        {
            Stepper::disable(c);
        }
        
        static void update_new_pos (Context c, MoveBuildState *s, FpType req)
        {
            auto *o = Object::self(c);
            o->m_req_pos = clamp_req_pos(req);
            if (AxisSpec::IsCartesian) {
                s->seen_cartesian = true;
            }
            TransformFeature::template mark_phys_moved<AxisIndex>(c);
        }
        
        template <typename Src, typename AddDistance, typename PlannerCmd>
        static void do_move (Context c, Src new_pos, AddDistance, FpType *distance_squared, FpType *total_steps, PlannerCmd *cmd)
        {
            auto *o = Object::self(c);
            AbsStepFixedType new_end_pos = AbsStepFixedType::template importFpSaturatedRound<FpType>(dist_from_real(new_pos.template get<AxisIndex>()));
            bool dir = (new_end_pos >= o->m_end_pos);
            StepFixedType move = StepFixedType::importBits(dir ? 
                ((typename StepFixedType::IntType)new_end_pos.bitsValue() - (typename StepFixedType::IntType)o->m_end_pos.bitsValue()) :
                ((typename StepFixedType::IntType)o->m_end_pos.bitsValue() - (typename StepFixedType::IntType)new_end_pos.bitsValue())
            );
            if (AMBRO_UNLIKELY(move.bitsValue() != 0)) {
                if (AddDistance::value && AxisSpec::IsCartesian) {
                    FpType delta = dist_to_real(move.template fpValue<FpType>());
                    *distance_squared += delta * delta;
                }
                *total_steps += move.template fpValue<FpType>();
                Stepper::enable(c);
            }
            auto *mycmd = TupleGetElem<AxisIndex>(cmd->axes.axes());
            mycmd->dir = dir;
            mycmd->x = move;
            mycmd->max_v_rec = 1.0f / speed_from_real((FpType)AxisSpec::DefaultMaxSpeed::value());
            mycmd->max_a_rec = 1.0f / accel_from_real((FpType)AxisSpec::DefaultMaxAccel::value());
            o->m_end_pos = new_end_pos;
        }
        
        template <typename PlannerCmd>
        static void limit_axis_move_speed (Context c, FpType time_freq_by_max_speed, PlannerCmd *cmd)
        {
            auto *mycmd = TupleGetElem<AxisIndex>(cmd->axes.axes());
            mycmd->max_v_rec = FloatMax(mycmd->max_v_rec, time_freq_by_max_speed * (FpType)(1.0 / AxisSpec::DefaultStepsPerUnit::value()));
        }
        
        static void fix_aborted_pos (Context c)
        {
            auto *o = Object::self(c);
            using RemStepsType = ChooseInt<AxisSpec::StepBits, true>;
            RemStepsType rem_steps = ThePlanner::template countAbortedRemSteps<AxisIndex, RemStepsType>(c);
            if (rem_steps != 0) {
                o->m_end_pos.m_bits.m_int -= rem_steps;
                o->m_req_pos = dist_to_real(o->m_end_pos.template fpValue<FpType>());
                TransformFeature::template mark_phys_moved<AxisIndex>(c);
            }
        }
        
        static void only_set_position (Context c, FpType value)
        {
            auto *o = Object::self(c);
            o->m_req_pos = clamp_req_pos(value);
            o->m_end_pos = AbsStepFixedType::template importFpSaturatedRound<FpType>(dist_from_real(o->m_req_pos));
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
        
        using EventLoopFastEvents = typename HomingFeature::EventLoopFastEvents;
        
        struct Object : public ObjBase<Axis, typename PrinterMain::Object, MakeTypeList<
            TheAxisDriver,
            MicroStepFeature
        >>
        {
            uint8_t m_state;
            AbsStepFixedType m_end_pos;
            FpType m_req_pos;
            FpType m_old_pos;
            bool m_relative_positioning;
        };
    };
    
    using AxesList = IndexElemList<ParamsAxesList, Axis>;
    
    template <int AxisName>
    using FindAxis = TypeListIndex<
        AxesList,
        ComposeFunctions<
            IsEqualFunc<WrapInt<AxisName>>,
            GetMemberType_WrappedAxisName
        >
    >;
    
    template <typename TheAxis>
    using MakePlannerAxisSpec = MotionPlannerAxisSpec<
        typename TheAxis::TheAxisDriver,
        TheAxis::AxisSpec::StepBits,
        typename TheAxis::AxisSpec::DefaultDistanceFactor,
        typename TheAxis::AxisSpec::DefaultCorneringDistance,
        PlannerPrestepCallback<TheAxis::AxisIndex>
    >;
    
    template <int LaserIndex>
    struct Laser {
        struct Object;
        using LaserSpec = TypeListGet<ParamsLasersList, LaserIndex>;
        using ThePwm = typename LaserSpec::PwmService::template Pwm<Context, Object>;
        using TheDutyFormula = typename LaserSpec::DutyFormulaService::template DutyFormula<typename ThePwm::DutyCycleType, ThePwm::MaxDutyCycle>;
        
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
        
        template <typename TheChannelCommon>
        static bool collect_new_pos (Context c, WrapType<TheChannelCommon>, MoveBuildState *s, typename TheChannelCommon::GcodeParserPartRef part)
        {
            auto *o = Object::self(c);
            char code = TheChannelCommon::TheGcodeParser::getPartCode(c, part);
            if (AMBRO_UNLIKELY(code == LaserSpec::Name)) {
                FpType energy = TheChannelCommon::TheGcodeParser::template getPartFpValue<FpType>(c, part);
                move_add_laser<LaserIndex>(c, s, energy);
                return false;
            }
            if (AMBRO_UNLIKELY(code== LaserSpec::DensityName)) {
                o->density = TheChannelCommon::TheGcodeParser::template getPartFpValue<FpType>(c, part);
                return false;
            }
            return true;
        }
        
        static void handle_automatic_energy (Context c, MoveBuildState *s, FpType distance)
        {
            auto *o = Object::self(c);
            auto *le = TupleGetElem<LaserIndex>(s->laser_extra());
            if (!le->energy_specified) {
                le->energy = FloatMakePosOrPosZero(o->density * distance);
            }
        }
        
        template <typename Src, typename PlannerCmd>
        static void write_planner_cmd (Context c, Src src, PlannerCmd *cmd)
        {
            auto *mycmd = TupleGetElem<LaserIndex>(cmd->axes.lasers());
            mycmd->x = src.template get<LaserIndex>() * (FpType)(1.0 / LaserSpec::LaserPower::value());
            mycmd->max_v_rec = (FpType)(Clock::time_freq / (LaserSpec::MaxPower::value() / LaserSpec::LaserPower::value()));
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
        
        struct Object : public ObjBase<Laser, typename PrinterMain::Object, MakeTypeList<
            ThePwm
        >> {
            FpType density;
        };
    };
    
    using LasersList = IndexElemList<ParamsLasersList, Laser>;
    
    template <typename TheLaser>
    using MakePlannerLaserSpec = MotionPlannerLaserSpec<
        typename TheLaser::LaserSpec::TheLaserDriverService,
        typename TheLaser::PowerInterface
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
        using TheTransformAlg = typename TransformParams::template TransformAlg<typename TransformParams::TransformAlgParams, FpType>;
        using TheSplitter = typename TheTransformAlg::Splitter;
        static int const NumVirtAxes = TheTransformAlg::NumAxes;
        static_assert(TypeListLength<ParamsVirtAxesList>::value == NumVirtAxes, "");
        static_assert(TypeListLength<ParamsPhysAxesList>::value == NumVirtAxes, "");
        
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
            TheTransformAlg::physToVirt(PhysReqPosSrc{c}, VirtReqPosDst{c});
        }
        
        static void handle_virt_move (Context c, MoveBuildState *s, FpType time_freq_by_max_speed)
        {
            auto *o = Object::self(c);
            auto *mob = PrinterMain::Object::self(c);
            AMBRO_ASSERT(mob->planner_state == PLANNER_RUNNING || mob->planner_state == PLANNER_CUSTOM)
            AMBRO_ASSERT(mob->m_planning_pull_pending)
            AMBRO_ASSERT(o->splitting)
            AMBRO_ASSERT(FloatIsPosOrPosZero(time_freq_by_max_speed))
            
            o->virt_update_pending = false;
            TheTransformAlg::virtToPhys(VirtReqPosSrc{c}, PhysReqPosDst{c});
            ListForEachForward<VirtAxesList>(LForeach_clamp_req_phys(), c);
            do_pending_virt_update(c);
            FpType distance_squared = 0.0f;
            ListForEachForward<VirtAxesList>(LForeach_prepare_split(), c, &distance_squared);
            ListForEachForward<SecondaryAxesList>(LForeach_prepare_split(), c, &distance_squared);
            FpType distance = FloatSqrt(distance_squared);
            ListForEachForward<LasersList>(LForeach_handle_automatic_energy(), c, s, distance);
            ListForEachForward<LaserSplitsList>(LForeach_prepare_split(), c, s);
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
            if (IsPhysAxisTransformPhys<WrapInt<PhysAxisIndex>>::value) {
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
                ListForEachForwardInterruptible<ChannelCommonList>(LForeach_run_for_state_command(), c, COMMAND_LOCKED, WrapType<TransformFeature>(), LForeach_continue_splitclear_helper());
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
                    TheTransformAlg::virtToPhys(ArraySrc{virt_pos}, PhysArrayDst{move_pos});
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
                    cmd->axes.rel_max_v_rec = FloatMax(rel_max_v_rec, total_steps * (FpType)(1.0 / (Params::MaxStepsPerCycle::value() * F_CPU * Clock::time_unit)));
                    ThePlanner::axesCommandDone(c);
                    goto submitted;
                }
            } while (o->splitting);
            
            ThePlanner::emptyDone(c);
        submitted:
            submitted_planner_command(c);
        }
        
        template <typename TheChannelCommon>
        static void continue_splitclear_helper (Context c, WrapType<TheChannelCommon>)
        {
            auto *o = Object::self(c);
            auto *cco = TheChannelCommon::Object::self(c);
            AMBRO_ASSERT(cco->m_state == COMMAND_LOCKED)
            AMBRO_ASSERT(!o->splitting)
            AMBRO_ASSERT(!o->splitclear_pending)
            
            work_command(c, WrapType<TheChannelCommon>());
        }
        
        static void handle_set_position (Context c, bool seen_virtual)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(!o->splitting)
            
            if (seen_virtual) {
                o->virt_update_pending = false;
                TheTransformAlg::virtToPhys(VirtReqPosSrc{c}, PhysReqPosDst{c});
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
            static int const PhysAxisIndex = FindAxis<TypeListGet<ParamsPhysAxesList, VirtAxisIndex>::value>::value;
            using ThePhysAxis = Axis<PhysAxisIndex>;
            static_assert(!ThePhysAxis::AxisSpec::IsCartesian, "");
            using WrappedPhysAxisIndex = WrapInt<PhysAxisIndex>;
            using HomingSpec = typename VirtAxisParams::VirtualHoming;
            using TheHomingHelper = HomingHelper<VirtAxis>;
            
            template <typename ThePrinterMain = PrinterMain>
            struct Lazy {
                static typename ThePrinterMain::PhysVirtAxisMaskType const AxisMask = (typename ThePrinterMain::PhysVirtAxisMaskType)1 << (NumAxes + VirtAxisIndex);
            };
            
            static void init (Context c)
            {
                auto *o = Object::self(c);
                o->m_relative_positioning = false;
                TheHomingHelper::init(c);
            }
            
            static void update_new_pos (Context c, MoveBuildState *s, FpType req)
            {
                auto *o = Object::self(c);
                auto *t = TransformFeature::Object::self(c);
                o->m_req_pos = clamp_virt_pos(req);
                t->splitting = true;
            }
            
            static void clamp_req_phys (Context c)
            {
                auto *axis = ThePhysAxis::Object::self(c);
                auto *t = TransformFeature::Object::self(c);
                if (AMBRO_UNLIKELY(!(axis->m_req_pos <= ThePhysAxis::max_req_pos()))) {
                    axis->m_req_pos = ThePhysAxis::max_req_pos();
                    t->virt_update_pending = true;
                } else if (AMBRO_UNLIKELY(!(axis->m_req_pos >= ThePhysAxis::min_req_pos()))) {
                    axis->m_req_pos = ThePhysAxis::min_req_pos();
                    t->virt_update_pending = true;
                }
            }
            
            static void clamp_move_phys (Context c, FpType *move_pos)
            {
                move_pos[PhysAxisIndex] = ThePhysAxis::clamp_req_pos(move_pos[PhysAxisIndex]);
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
                o->m_req_pos = clamp_virt_pos(value);
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
                return FloatMax(accum, FloatAbs(o->m_delta) * (FpType)(Clock::time_freq / VirtAxisParams::MaxSpeed::value()));
            }
            
            static FpType clamp_virt_pos (FpType req)
            {
                return FloatMax(VirtAxisParams::MinPos::value(), FloatMin(VirtAxisParams::MaxPos::value(), req));
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
                
                static bool start_virt_homing (Context c)
                {
                    auto *o = Object::self(c);
                    auto *mo = PrinterMain::Object::self(c);
                    
                    if (!(mo->m_homing_rem_axes & Lazy<>::AxisMask)) {
                        return true;
                    }
                    set_position(c, home_start_pos());
                    custom_planner_init(c, &o->planner_client, true);
                    o->state = 0;
                    o->command_sent = false;
                    return false;
                }
                
                template <typename CallbackContext>
                static bool prestep_callback (CallbackContext c)
                {
                    return !TheHomingHelper::endstop_is_triggered(c);
                }
                
                static void set_position (Context c, FpType value)
                {
                    SetPositionState s;
                    set_position_begin(c, &s);
                    set_position_add_axis<(NumAxes + VirtAxisIndex)>(c, &s, value);
                    set_position_end(c, &s);
                }
                
                static FpType home_start_pos ()
                {
                    return HomingSpec::HomeDir ? VirtAxisParams::MinPos::value() : VirtAxisParams::MaxPos::value();
                }
                
                static FpType home_end_pos ()
                {
                    return HomingSpec::HomeDir ? VirtAxisParams::MaxPos::value() : VirtAxisParams::MinPos::value();
                }
                
                static FpType home_dir ()
                {
                    return HomingSpec::HomeDir ? 1.0f : -1.0f;
                }
                
                struct VirtHomingPlannerClient : public PlannerClient {
                    void pull_handler (Context c)
                    {
                        auto *o = Object::self(c);
                        auto *axis = VirtAxis::Object::self(c);
                        
                        if (o->command_sent) {
                            return custom_planner_wait_finished(c);
                        }
                        MoveBuildState s;
                        move_begin(c, &s);
                        FpType position;
                        FpType speed;
                        switch (o->state) {
                            case 0: {
                                position = home_end_pos() + home_dir() * HomingSpec::FastExtraDist::value();
                                speed = HomingSpec::FastSpeed::value();
                            } break;
                            case 1: {
                                position = home_end_pos() - home_dir() * HomingSpec::RetractDist::value();
                                speed = HomingSpec::RetractSpeed::value();
                            } break;
                            case 2: {
                                position = home_end_pos() + home_dir() * HomingSpec::SlowExtraDist::value();
                                speed = HomingSpec::SlowSpeed::value();
                            } break;
                        }
                        move_add_axis<(NumAxes + VirtAxisIndex)>(c, &s, position);
                        move_end(c, &s, (FpType)Clock::time_freq / speed);
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
                            set_position(c, home_end_pos());
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
                
                struct Object : public ObjBase<HomingFeature, typename VirtAxis::Object, EmptyTypeList> {
                    VirtHomingPlannerClient planner_client;
                    uint8_t state;
                    bool command_sent;
                };
            } AMBRO_STRUCT_ELSE(HomingFeature) {
                static bool start_virt_homing (Context c) { return true; }
                template <typename CallbackContext>
                static bool prestep_callback (CallbackContext c) { return true; }
                struct Object {};
            };
            
            struct Object : public ObjBase<VirtAxis, typename TransformFeature::Object, MakeTypeList<
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
        using IsPhysAxisTransformPhys = WrapBool<(TypeListIndex<
            VirtAxesList,
            ComposeFunctions<
                IsEqualFunc<PhysAxisIndex>,
                GetMemberType_WrappedPhysAxisIndex
            >
        >::value >= 0)>;
        
        using SecondaryAxisIndices = FilterTypeList<
            SequenceList<NumAxes>,
            ComposeFunctions<
                NotFunc,
                TemplateFunc<IsPhysAxisTransformPhys>
            >
        >;
        static int const NumSecondaryAxes = TypeListLength<SecondaryAxisIndices>::value;
        
        template <int SecondaryAxisIndex>
        struct SecondaryAxis {
            static int const AxisIndex = TypeListGet<SecondaryAxisIndices, SecondaryAxisIndex>::value;
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
            
            static void prepare_split (Context c, MoveBuildState *s)
            {
                auto *o = Object::self(c);
                o->energy = TupleGetElem<LaserIndex>(s->laser_extra())->energy;
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
        static void handle_virt_move (Context c, MoveBuildState *s, FpType time_freq_by_max_speed) {}
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
    static PhysVirtAxisMaskType const PhysAxisMask = PowerOfTwoMinusOne<PhysVirtAxisMaskType, NumAxes>::value;
    
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
        
        static void update_new_pos (Context c, MoveBuildState *s, FpType req)
        {
            TheAxis::update_new_pos(c, s, req);
        }
        
        template <typename TheChannelCommon>
        static bool collect_new_pos (Context c, WrapType<TheChannelCommon>, MoveBuildState *s, typename TheChannelCommon::GcodeParserPartRef part)
        {
            auto *axis = TheAxis::Object::self(c);
            if (AMBRO_UNLIKELY(TheChannelCommon::TheGcodeParser::getPartCode(c, part) == TheAxis::AxisName)) {
                FpType req = TheChannelCommon::TheGcodeParser::template getPartFpValue<FpType>(c, part);
                if (axis->m_relative_positioning) {
                    req += axis->m_old_pos;
                }
                update_new_pos(c, s, req);
                return false;
            }
            return true;
        }
        
        static void set_relative_positioning (Context c, bool relative)
        {
            auto *axis = TheAxis::Object::self(c);
            axis->m_relative_positioning = relative;
        }
        
        template <typename TheChannelCommon>
        static void append_position (Context c, WrapType<TheChannelCommon>)
        {
            auto *axis = TheAxis::Object::self(c);
            if (PhysVirtAxisIndex > 0) {
                TheChannelCommon::reply_append_ch(c, ' ');
            }
            TheChannelCommon::reply_append_ch(c, TheAxis::AxisName);
            TheChannelCommon::reply_append_ch(c, ':');
            TheChannelCommon::reply_append_fp(c, axis->m_req_pos);
        }
        
        template <typename TheChannelCommon>
        static void g92_check_axis (Context c, WrapType<TheChannelCommon>, typename TheChannelCommon::GcodeParserPartRef part, SetPositionState *s)
        {
            if (TheChannelCommon::TheGcodeParser::getPartCode(c, part) == TheAxis::AxisName) {
                FpType value = TheChannelCommon::TheGcodeParser::template getPartFpValue<FpType>(c, part);
                set_position_add_axis<PhysVirtAxisIndex>(c, s, value);
            }
        }
        
        template <typename TheChannelCommon>
        static void update_homing_mask (Context c, WrapType<TheChannelCommon>, PhysVirtAxisMaskType *mask, typename TheChannelCommon::GcodeParserPartRef part)
        {
            if (TheChannelCommon::TheGcodeParser::getPartCode(c, part) == TheAxis::AxisName) {
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
        
        template <typename TheChannelCommon>
        static void m119_append_endstop (Context c, WrapType<TheChannelCommon> cc)
        {
            TheAxis::TheHomingHelper::m119_append_endstop(c, cc);
        }
    };
    
    using PhysVirtAxisHelperList = IndexElemListCount<NumPhysVirtAxes, PhysVirtAxisHelper>;
    
    template <int AxisName>
    using FindPhysVirtAxis = TypeListIndex<
        PhysVirtAxisHelperList,
        ComposeFunctions<
            IsEqualFunc<WrapInt<AxisName>>,
            GetMemberType_WrappedAxisName
        >
    >;
    
    template <int HeaterIndex>
    struct Heater {
        struct Object;
        struct SoftPwmTimerHandler;
        struct ObserverGetValueCallback;
        struct ObserverHandler;
        
        using HeaterSpec = TypeListGet<ParamsHeatersList, HeaterIndex>;
        using TheControl = typename HeaterSpec::template Control<typename HeaterSpec::ControlParams, typename HeaterSpec::ControlInterval, FpType>;
        using ControlConfig = typename TheControl::Config;
        using TheSoftPwm = SoftPwm<Context, Object, typename HeaterSpec::OutputPin, HeaterSpec::OutputInvert, typename HeaterSpec::PulseInterval, SoftPwmTimerHandler, typename HeaterSpec::TimerService>;
        using TheObserver = TemperatureObserver<Context, Object, FpType, typename HeaterSpec::TheTemperatureObserverParams, ObserverGetValueCallback, ObserverHandler>;
        using PwmPowerData = typename TheSoftPwm::PowerData;
        using TheFormula = typename HeaterSpec::Formula::template Inner<FpType>;
        using AdcFixedType = typename Context::Adc::FixedType;
        using AdcIntType = typename AdcFixedType::IntType;
        
        static const TimeType ControlIntervalTicks = HeaterSpec::ControlInterval::value() / Clock::time_unit;
        
        // compute the ADC readings corresponding to MinSafeTemp and MaxSafeTemp
        template <typename Temp>
        struct TempToAdcAbs {
            using Result = AMBRO_WRAP_DOUBLE((TheFormula::template TempToAdc<Temp>::Result::value() * PowerOfTwo<double, AdcFixedType::num_bits>::value));
        };
        using InfAdcValueFp = typename TempToAdcAbs<typename HeaterSpec::MaxSafeTemp>::Result;
        using SupAdcValueFp = typename TempToAdcAbs<typename HeaterSpec::MinSafeTemp>::Result;
        static_assert(InfAdcValueFp::value() > 1, "");
        static_assert(SupAdcValueFp::value() < PowerOfTwoMinusOne<AdcIntType, AdcFixedType::num_bits>::value, "");
        static constexpr AdcIntType InfAdcValue = InfAdcValueFp::value();
        static constexpr AdcIntType SupAdcValue = SupAdcValueFp::value();
        
        struct ChannelPayload {
            FpType target;
        };
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            o->m_enabled = false;
            o->m_control_config = TheControl::makeDefaultConfig();
            TimeType time = Clock::getTime(c) + (TimeType)(0.05 * Clock::time_freq);
            TheSoftPwm::computeZeroPowerData(&o->m_output_pd);
            o->m_control_event.init(c, Heater::control_event_handler);
            o->m_control_event.appendAt(c, time + (TimeType)(0.6 * ControlIntervalTicks));
            o->m_was_not_unset = false;
            TheSoftPwm::init(c, time);
            o->m_observing = false;
        }
        
        static void deinit (Context c)
        {
            auto *o = Object::self(c);
            if (o->m_observing) {
                TheObserver::deinit(c);
            }
            TheSoftPwm::deinit(c);
            o->m_control_event.deinit(c);
        }
        
        static FpType adc_to_temp (AdcFixedType adc_value)
        {
            FpType adc_fp = adc_value.template fpValue<FpType>() + (FpType)(0.5 / PowerOfTwo<double, AdcFixedType::num_bits>::value);
            return TheFormula::adc_to_temp(adc_fp);
        }
        
        static FpType get_temp (Context c)
        {
            AdcFixedType adc_value = Context::Adc::template getValue<typename HeaterSpec::AdcPin>(c);
            return adc_to_temp(adc_value);
        }
        
        template <typename TheChannelCommon>
        static void append_value (Context c, WrapType<TheChannelCommon>)
        {
            FpType value = get_temp(c);
            TheChannelCommon::reply_append_ch(c, ' ');
            TheChannelCommon::reply_append_ch(c, HeaterSpec::Name);
            TheChannelCommon::reply_append_ch(c, ':');
            TheChannelCommon::reply_append_fp(c, value);
        }
        
        template <typename TheChannelCommon>
        static void append_adc_value (Context c, WrapType<TheChannelCommon>)
        {
            AdcFixedType adc_value = Context::Adc::template getValue<typename HeaterSpec::AdcPin>(c);
            TheChannelCommon::reply_append_ch(c, ' ');
            TheChannelCommon::reply_append_ch(c, HeaterSpec::Name);
            TheChannelCommon::reply_append_pstr(c, AMBRO_PSTR("A:"));
            TheChannelCommon::reply_append_fp(c, adc_value.template fpValue<FpType>());
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
                TheSoftPwm::computeZeroPowerData(&o->m_output_pd);
            }
        }
        
        template <typename TheChannelCommon>
        static bool check_command (Context c, WrapType<TheChannelCommon> cc)
        {
            auto *o = Object::self(c);
            
            if (TheChannelCommon::TheGcodeParser::getCmdNumber(c) == HeaterSpec::WaitMCommand) {
                if (!TheChannelCommon::tryUnplannedCommand(c)) {
                    return false;
                }
                FpType target = TheChannelCommon::get_command_param_fp(c, 'S', 0.0f);
                if (target >= (FpType)HeaterSpec::MinSafeTemp::value() && target <= (FpType)HeaterSpec::MaxSafeTemp::value()) {
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
            if (TheChannelCommon::TheGcodeParser::getCmdNumber(c) == HeaterSpec::SetMCommand) {
                if (!TheChannelCommon::tryPlannedCommand(c)) {
                    return false;
                }
                FpType target = TheChannelCommon::get_command_param_fp(c, 'S', 0.0f);
                TheChannelCommon::finishCommand(c);
                if (!(target >= (FpType)HeaterSpec::MinSafeTemp::value() && target <= (FpType)HeaterSpec::MaxSafeTemp::value())) {
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
            if (TheChannelCommon::TheGcodeParser::getCmdNumber(c) == HeaterSpec::SetConfigMCommand && TheControl::SupportsConfig) {
                if (!TheChannelCommon::tryUnplannedCommand(c)) {
                    return false;
                }
                TheControl::setConfigCommand(c, cc, &o->m_control_config);
                TheChannelCommon::finishCommand(c);
                return false;
            }
            return true;
        }
        
        template <typename TheChannelCommon>
        static void print_config (Context c, WrapType<TheChannelCommon> cc)
        {
            auto *o = Object::self(c);
            
            if (TheControl::SupportsConfig) {
                TheChannelCommon::reply_append_pstr(c, AMBRO_PSTR("M" ));
                TheChannelCommon::reply_append_uint32(c, HeaterSpec::SetConfigMCommand);
                TheControl::printConfig(c, cc, &o->m_control_config);
                TheChannelCommon::reply_append_ch(c, '\n');
            }
        }
        
        static void softpwm_timer_handler (typename TheSoftPwm::TimerInstance::HandlerContext c, PwmPowerData *pd)
        {
            auto *o = Object::self(c);
            
            AdcFixedType adc_value = Context::Adc::template getValue<typename HeaterSpec::AdcPin>(c);
            bool unsafe = (AMBRO_LIKELY(adc_value.bitsValue() <= InfAdcValue || adc_value.bitsValue() >= SupAdcValue));
            
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                if (unsafe) {
                    unset(lock_c);
                }
                *pd = o->m_output_pd;
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
            Context::Pins::template emergencySet<typename HeaterSpec::OutputPin>(false);
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
            
            o->m_control_event.appendAfterPrevious(c, ControlIntervalTicks);
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
                    o->m_control.init();
                }
                FpType sensor_value = get_temp(c);
                FpType output = o->m_control.addMeasurement(sensor_value, target, &o->m_control_config);
                PwmPowerData output_pd;
                TheSoftPwm::computePowerData(output, &output_pd);
                AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                    if (o->m_was_not_unset) {
                        o->m_output_pd = output_pd;
                    }
                }
            }
        }
        
        struct SoftPwmTimerHandler : public AMBRO_WFUNC_TD(&Heater::softpwm_timer_handler) {};
        struct ObserverGetValueCallback : public AMBRO_WFUNC_TD(&Heater::get_temp) {};
        struct ObserverHandler : public AMBRO_WFUNC_TD(&Heater::observer_handler) {};
        
        struct Object : public ObjBase<Heater, typename PrinterMain::Object, MakeTypeList<
            TheSoftPwm,
            TheObserver
        >> {
            bool m_enabled;
            TheControl m_control;
            ControlConfig m_control_config;
            FpType m_target;
            bool m_observing;
            PwmPowerData m_output_pd;
            typename Loop::QueuedEvent m_control_event;
            bool m_was_not_unset;
        };
    };
    
    template <int FanIndex>
    struct Fan {
        struct Object;
        struct SoftPwmTimerHandler;
        
        using FanSpec = TypeListGet<ParamsFansList, FanIndex>;
        using TheSoftPwm = SoftPwm<Context, Object, typename FanSpec::OutputPin, FanSpec::OutputInvert, typename FanSpec::PulseInterval, SoftPwmTimerHandler, typename FanSpec::TimerService>;
        using PwmPowerData = typename TheSoftPwm::PowerData;
        
        struct ChannelPayload {
            PwmPowerData target_pd;
        };
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            TheSoftPwm::computeZeroPowerData(&o->m_target_pd);
            TimeType time = Clock::getTime(c) + (TimeType)(0.05 * Clock::time_freq);
            TheSoftPwm::init(c, time);
        }
        
        static void deinit (Context c)
        {
            TheSoftPwm::deinit(c);
        }
        
        template <typename TheChannelCommon>
        static bool check_command (Context c, WrapType<TheChannelCommon>)
        {
            if (TheChannelCommon::TheGcodeParser::getCmdNumber(c) == FanSpec::SetMCommand || TheChannelCommon::TheGcodeParser::getCmdNumber(c) == FanSpec::OffMCommand) {
                if (!TheChannelCommon::tryPlannedCommand(c)) {
                    return false;
                }
                FpType target = 0.0f;
                if (TheChannelCommon::TheGcodeParser::getCmdNumber(c) == FanSpec::SetMCommand) {
                    target = 1.0f;
                    if (TheChannelCommon::find_command_param_fp(c, 'S', &target)) {
                        target *= (FpType)FanSpec::SpeedMultiply::value();
                    }
                }
                TheChannelCommon::finishCommand(c);
                PlannerSplitBuffer *cmd = ThePlanner::getBuffer(c);
                PlannerChannelPayload *payload = UnionGetElem<0>(&cmd->channel_payload);
                payload->type = TypeListLength<ParamsHeatersList>::value + FanIndex;
                TheSoftPwm::computePowerData(target, &UnionGetElem<FanIndex>(&payload->fans)->target_pd);
                ThePlanner::channelCommandDone(c, 1);
                submitted_planner_command(c);
                return false;
            }
            return true;
        }
        
        static void softpwm_timer_handler (typename TheSoftPwm::TimerInstance::HandlerContext c, PwmPowerData *pd)
        {
            auto *o = Object::self(c);
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
            auto *o = Object::self(c);
            ChannelPayload *payload = UnionGetElem<FanIndex>(payload_union);
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                o->m_target_pd = payload->target_pd;
            }
        }
        
        struct SoftPwmTimerHandler : public AMBRO_WFUNC_TD(&Fan::softpwm_timer_handler) {};
        
        struct Object : public ObjBase<Fan, typename PrinterMain::Object, MakeTypeList<
            TheSoftPwm
        >> {
            PwmPowerData m_target_pd;
        };
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
    using ThePlanner = MotionPlanner<Context, typename PlannerUnionPlanner::Object, MotionPlannerAxes, Params::StepperSegmentBufferSize, Params::LookaheadBufferSize, Params::LookaheadCommitCount, FpType, PlannerPullHandler, PlannerFinishedHandler, PlannerAbortedHandler, PlannerUnderrunCallback, MotionPlannerChannels, MotionPlannerLasers>;
    using PlannerSplitBuffer = typename ThePlanner::SplitBuffer;
    
    AMBRO_STRUCT_IF(ProbeFeature, Params::ProbeParams::Enabled) {
        struct Object;
        using ProbeParams = typename Params::ProbeParams;
        static const int NumPoints = TypeListLength<typename ProbeParams::ProbePoints>::value;
        static const int ProbeAxisIndex = FindPhysVirtAxis<Params::ProbeParams::ProbeAxis>::value;
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            o->m_current_point = 0xff;
            Context::Pins::template setInput<typename ProbeParams::ProbePin, typename ProbeParams::ProbePinInputMode>(c);
        }
        
        static void deinit (Context c)
        {
        }
        
        template <typename TheChannelCommon>
        static bool check_command (Context c, WrapType<TheChannelCommon>)
        {
            auto *o = Object::self(c);
            if (TheChannelCommon::TheGcodeParser::getCmdNumber(c) == 32) {
                if (!TheChannelCommon::tryUnplannedCommand(c)) {
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
                FpType coord = ListForOneOffset<PointHelperList, 0, FpType>(point_index, LForeach_get_coord());
                move_add_axis<AxisIndex>(c, s, coord + (FpType)AxisProbeOffset::value());
            }
            
            template <int PointIndex>
            struct PointHelper {
                using Point = TypeListGet<typename ProbeParams::ProbePoints, PointIndex>;
                using PointCoord = TypeListGet<Point, PlatformAxisIndex>;
                
                static FpType get_coord ()
                {
                    return (FpType)PointCoord::value();
                }
            };
            
            using PointHelperList = IndexElemList<typename ProbeParams::ProbePoints, PointHelper>;
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
                MoveBuildState s;
                move_begin(c, &s);
                FpType height;
                FpType time_freq_by_speed;
                switch (o->m_point_state) {
                    case 0: {
                        ListForEachForward<AxisHelperList>(LForeach_add_axis(), c, &s, o->m_current_point);
                        height = (FpType)ProbeParams::ProbeStartHeight::value();
                        time_freq_by_speed = (FpType)(Clock::time_freq / ProbeParams::ProbeMoveSpeed::value());
                    } break;
                    case 1: {
                        height = (FpType)ProbeParams::ProbeLowHeight::value();
                        time_freq_by_speed = (FpType)(Clock::time_freq / ProbeParams::ProbeFastSpeed::value());
                    } break;
                    case 2: {
                        height = get_height(c) + (FpType)ProbeParams::ProbeRetractDist::value();
                        time_freq_by_speed = (FpType)(Clock::time_freq / ProbeParams::ProbeRetractSpeed::value());
                    } break;
                    case 3: {
                        height = (FpType)ProbeParams::ProbeLowHeight::value();
                        time_freq_by_speed = (FpType)(Clock::time_freq / ProbeParams::ProbeSlowSpeed::value());
                    } break;
                    case 4: {
                        height = (FpType)ProbeParams::ProbeStartHeight::value();
                        time_freq_by_speed = (FpType)(Clock::time_freq / ProbeParams::ProbeRetractSpeed::value());
                    } break;
                }
                move_add_axis<ProbeAxisIndex>(c, &s, height);
                move_end(c, &s, time_freq_by_speed);
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
                        ListForEachForwardInterruptible<ChannelCommonList>(LForeach_run_for_state_command(), c, COMMAND_LOCKED, WrapType<ProbeFeature>(), LForeach_report_height(), height);
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
        
        template <typename TheChannelCommon>
        static void report_height (Context c, WrapType<TheChannelCommon>, FpType height)
        {
            TheChannelCommon::reply_append_pstr(c, AMBRO_PSTR("//ProbeHeight "));
            TheChannelCommon::reply_append_fp(c, height);
            TheChannelCommon::reply_append_ch(c, '\n');
            TheChannelCommon::reply_poke(c);
        }
        
        struct Object : public ObjBase<ProbeFeature, typename PrinterMain::Object, EmptyTypeList> {
            ProbePlannerClient planner_client;
            uint8_t m_current_point;
            uint8_t m_point_state;
            bool m_command_sent;
            FpType m_samples[NumPoints];
        };
    } AMBRO_STRUCT_ELSE(ProbeFeature) {
        static void init (Context c) {}
        static void deinit (Context c) {}
        template <typename TheChannelCommon>
        static bool check_command (Context c, WrapType<TheChannelCommon>) { return true; }
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
        using Current = typename CurrentParams::template CurrentTemplate<Context, Object, typename CurrentParams::CurrentParams, CurrentChannelsList>;
        
        static void init (Context c)
        {
            Current::init(c);
            ListForEachForward<CurrentAxesList>(LForeach_init(), c);
        }
        
        static void deinit (Context c)
        {
            Current::deinit(c);
        }
        
        template <typename TheChannelCommon>
        static bool check_command (Context c, WrapType<TheChannelCommon> cc)
        {
            if (TheChannelCommon::TheGcodeParser::getCmdNumber(c) == 906) {
                auto num_parts = TheChannelCommon::TheGcodeParser::getNumParts(c);
                for (typename TheChannelCommon::GcodePartsSizeType i = 0; i < num_parts; i++) {
                    typename TheChannelCommon::GcodeParserPartRef part = TheChannelCommon::TheGcodeParser::getPart(c, i);
                    ListForEachForwardInterruptible<CurrentAxesList>(LForeach_check_current_axis(), c, cc, TheChannelCommon::TheGcodeParser::getPartCode(c, part), TheChannelCommon::TheGcodeParser::template getPartFpValue<FpType>(c, part));
                }
                TheChannelCommon::finishCommand(c);
                return false;
            }
            return true;
        }
        
        using EventLoopFastEvents = typename Current::EventLoopFastEvents;
        
        template <int CurrentAxisIndex>
        struct CurrentAxis {
            using CurrentAxisParams = TypeListGet<ParamsCurrentAxesList, CurrentAxisIndex>;
            
            static void init (Context c)
            {
                Current::template setCurrent<CurrentAxisIndex>(c, CurrentAxisParams::DefaultCurrent::value());
            }
            
            template <typename TheChannelCommon>
            static bool check_current_axis (Context c, WrapType<TheChannelCommon>, char axis_name, FpType current)
            {
                if (axis_name == CurrentAxisParams::AxisName) {
                    Current::template setCurrent<CurrentAxisIndex>(c, current);
                    return false;
                }
                return true;
            }
        };
        
        using CurrentAxesList = IndexElemList<ParamsCurrentAxesList, CurrentAxis>;
        
        struct Object : public ObjBase<CurrentFeature, typename PrinterMain::Object, MakeTypeList<
            Current
        >> {};
    } AMBRO_STRUCT_ELSE(CurrentFeature) {
        static void init (Context c) {}
        static void deinit (Context c) {}
        template <typename TheChannelCommon>
        static bool check_command (Context c, WrapType<TheChannelCommon>) { return true; }
        using EventLoopFastEvents = EmptyTypeList;
        struct Object {};
    };
    
public:
    static void init (Context c)
    {
        auto *ob = Object::self(c);
        
        ob->unlocked_timer.init(c, PrinterMain::unlocked_timer_handler);
        ob->disable_timer.init(c, PrinterMain::disable_timer_handler);
        ob->force_timer.init(c, PrinterMain::force_timer_handler);
        TheWatchdog::init(c);
        TheBlinker::init(c, (FpType)(Params::LedBlinkInterval::value() * Clock::time_freq));
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
        ob->inactive_time = (FpType)(Params::DefaultInactiveTime::value() * Clock::time_freq);
        ob->time_freq_by_max_speed = 0.0f;
        ob->underrun_count = 0;
        ob->locked = false;
        ob->planner_state = PLANNER_NONE;
        
        SerialFeature::TheChannelCommon::reply_append_pstr(c, AMBRO_PSTR("start\nAPrinter\n"));
        SerialFeature::TheChannelCommon::reply_poke(c);
        
        ob->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        auto *ob = Object::self(c);
        ob->debugDeinit(c);
        
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
        TheWatchdog::deinit(c);
        ob->force_timer.deinit(c);
        ob->disable_timer.deinit(c);
        ob->unlocked_timer.deinit(c);
    }
    
    using GetWatchdog = TheWatchdog;
    
    using GetSerial = typename SerialFeature::TheSerial;
    
    template <int AxisIndex>
    using GetAxisTimer = typename Axis<AxisIndex>::TheAxisDriver::GetTimer;
    
    template <int HeaterIndex>
    using GetHeaterTimer = typename Heater<HeaterIndex>::TheSoftPwm::GetTimer;
    
    template <int FanIndex>
    using GetFanTimer = typename Fan<FanIndex>::TheSoftPwm::GetTimer;
    
    using GetEventChannelTimer = typename ThePlanner::template GetChannelTimer<0>;
    
    template <typename TSdCardFeatue = SdCardFeature>
    using GetSdCard = typename TSdCardFeatue::TheSdCard;
    
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
    
    using EventLoopFastEvents = JoinTypeLists<
        typename CurrentFeature::EventLoopFastEvents,
        JoinTypeLists<
            typename SdCardFeature::EventLoopFastEvents,
            JoinTypeLists<
                typename SerialFeature::TheSerial::EventLoopFastEvents,
                JoinTypeLists<
                    typename ThePlanner::EventLoopFastEvents,
                    TypeListFold<
                        MapTypeList<AxesList, GetMemberType_EventLoopFastEvents>,
                        EmptyTypeList,
                        JoinTwoTypeLists
                    >
                >
            >
        >
    >;
    
public: // private, see comment on top
    static TimeType time_from_real (FpType t)
    {
        return (FixedPoint<30, false, 0>::template importFpSaturatedRound<FpType>(t * (FpType)Clock::time_freq)).bitsValue();
    }
    
    static void blinker_handler (Context c)
    {
        auto *ob = Object::self(c);
        ob->debugAccess(c);
        
        TheWatchdog::reset(c);
    }
    
    template <typename TheChannelCommon>
    static void work_command (Context c, WrapType<TheChannelCommon> cc)
    {
        auto *ob = Object::self(c);
        auto *cco = TheChannelCommon::Object::self(c);
        AMBRO_ASSERT(cco->m_cmd)
        
        switch (TheChannelCommon::TheGcodeParser::getCmdCode(c)) {
            case 'M': switch (TheChannelCommon::TheGcodeParser::getCmdNumber(c)) {
                default:
                    if (
                        ListForEachForwardInterruptible<HeatersList>(LForeach_check_command(), c, cc) &&
                        ListForEachForwardInterruptible<FansList>(LForeach_check_command(), c, cc) &&
                        SdCardFeature::check_command(c, cc) &&
                        ProbeFeature::check_command(c, cc) &&
                        CurrentFeature::check_command(c, cc)
                    ) {
                        goto unknown_command;
                    }
                    return;
                
                case 110: // set line number
                    return TheChannelCommon::finishCommand(c);
                
                case 17: {
                    if (!TheChannelCommon::tryUnplannedCommand(c)) {
                        return;
                    }
                    ListForEachForward<AxesList>(LForeach_enable_stepper(), c);
                    now_inactive(c);
                    return TheChannelCommon::finishCommand(c);
                } break;
                
                case 18: // disable steppers or set timeout
                case 84: {
                    if (!TheChannelCommon::tryUnplannedCommand(c)) {
                        return;
                    }
                    FpType inactive_time;
                    if (TheChannelCommon::find_command_param_fp(c, 'S', &inactive_time)) {
                        ob->inactive_time = time_from_real(inactive_time);
                        if (ob->disable_timer.isSet(c)) {
                            ob->disable_timer.appendAt(c, ob->last_active_time + ob->inactive_time);
                        }
                    } else {
                        ListForEachForward<AxesList>(LForeach_disable_stepper(), c);
                        ob->disable_timer.unset(c);
                    }
                    return TheChannelCommon::finishCommand(c);
                } break;
                
                case 105: {
                    TheChannelCommon::reply_append_pstr(c, AMBRO_PSTR("ok"));
                    ListForEachForward<HeatersList>(LForeach_append_value(), c, cc);
                    TheChannelCommon::reply_append_ch(c, '\n');
                    return TheChannelCommon::finishCommand(c, true);
                } break;
                
                case 114: {
                    ListForEachForward<PhysVirtAxisHelperList>(LForeach_append_position(), c, cc);
                    TheChannelCommon::reply_append_ch(c, '\n');
                    return TheChannelCommon::finishCommand(c);
                } break;
                
                case 119: {
                    TheChannelCommon::reply_append_pstr(c, AMBRO_PSTR("endstops:"));
                    ListForEachForward<PhysVirtAxisHelperList>(LForeach_m119_append_endstop(), c, cc);
                    TheChannelCommon::reply_append_ch(c, '\n');                    
                    return TheChannelCommon::finishCommand(c, true);
                } break;
                
                case 136: { // print heater config
                    ListForEachForward<HeatersList>(LForeach_print_config(), c, cc);
                    return TheChannelCommon::finishCommand(c);
                } break;
                
#ifdef EVENTLOOP_BENCHMARK
                case 916: { // reset benchmark time
                    if (!TheChannelCommon::tryUnplannedCommand(c)) {
                        return;
                    }
                    Context::EventLoop::resetBenchTime(c);
                    return TheChannelCommon::finishCommand(c);
                } break;
                
                case 917: { // print benchmark time
                    if (!TheChannelCommon::tryUnplannedCommand(c)) {
                        return;
                    }
                    TheChannelCommon::reply_append_uint32(c, Context::EventLoop::getBenchTime(c));
                    TheChannelCommon::reply_append_ch(c, '\n');
                    return TheChannelCommon::finishCommand(c);
                } break;
#endif
                
                case 920: { // get underrun count
                    TheChannelCommon::reply_append_uint32(c, ob->underrun_count);
                    TheChannelCommon::reply_append_ch(c, '\n');
                    TheChannelCommon::finishCommand(c);
                } break;
                
                case 921: { // get heater ADC readings
                    TheChannelCommon::reply_append_pstr(c, AMBRO_PSTR("ok"));
                    ListForEachForward<HeatersList>(LForeach_append_adc_value(), c, cc);
                    TheChannelCommon::reply_append_ch(c, '\n');
                    return TheChannelCommon::finishCommand(c, true);
                } break;
            } break;
            
            case 'G': switch (TheChannelCommon::TheGcodeParser::getCmdNumber(c)) {
                default:
                    goto unknown_command;
                
                case 0:
                case 1: { // buffered move
                    if (!TheChannelCommon::tryPlannedCommand(c)) {
                        return;
                    }
                    MoveBuildState s;
                    move_begin(c, &s);
                    auto num_parts = TheChannelCommon::TheGcodeParser::getNumParts(c);
                    for (typename TheChannelCommon::GcodePartsSizeType i = 0; i < num_parts; i++) {
                        typename TheChannelCommon::GcodeParserPartRef part = TheChannelCommon::TheGcodeParser::getPart(c, i);
                        if (ListForEachForwardInterruptible<PhysVirtAxisHelperList>(LForeach_collect_new_pos(), c, cc, &s, part) &&
                            ListForEachForwardInterruptible<LasersList>(LForeach_collect_new_pos(), c, cc, &s, part)
                        ) {
                            if (TheChannelCommon::TheGcodeParser::getPartCode(c, part) == 'F') {
                                ob->time_freq_by_max_speed = (FpType)(Clock::time_freq / Params::SpeedLimitMultiply::value()) / FloatMakePosOrPosZero(TheChannelCommon::TheGcodeParser::template getPartFpValue<FpType>(c, part));
                            }
                        }
                    }
                    TheChannelCommon::finishCommand(c);
                    move_end(c, &s, ob->time_freq_by_max_speed);
                } break;
                
                case 21: // set units to millimeters
                    return TheChannelCommon::finishCommand(c);
                
                case 28: { // home axes
                    if (!TheChannelCommon::tryUnplannedCommand(c)) {
                        return;
                    }
                    PhysVirtAxisMaskType mask = 0;
                    auto num_parts = TheChannelCommon::TheGcodeParser::getNumParts(c);
                    for (typename TheChannelCommon::GcodePartsSizeType i = 0; i < num_parts; i++) {
                        ListForEachForward<PhysVirtAxisHelperList>(LForeach_update_homing_mask(), c, cc, &mask, TheChannelCommon::TheGcodeParser::getPart(c, i));
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
                    return TheChannelCommon::finishCommand(c);
                } break;
                
                case 91: { // relative positioning
                    ListForEachForward<PhysVirtAxisHelperList>(LForeach_set_relative_positioning(), c, true);
                    return TheChannelCommon::finishCommand(c);
                } break;
                
                case 92: { // set position
                    if (!TheChannelCommon::trySplitClearCommand(c)) {
                        return;
                    }
                    SetPositionState s;
                    set_position_begin(c, &s);
                    auto num_parts = TheChannelCommon::TheGcodeParser::getNumParts(c);
                    for (typename TheChannelCommon::GcodePartsSizeType i = 0; i < num_parts; i++) {
                        ListForEachForward<PhysVirtAxisHelperList>(LForeach_g92_check_axis(), c, cc, TheChannelCommon::TheGcodeParser::getPart(c, i), &s);
                    }
                    set_position_end(c, &s);
                    return TheChannelCommon::finishCommand(c);
                } break;
            } break;
            
            unknown_command:
            default: {
                TheChannelCommon::reply_append_pstr(c, AMBRO_PSTR("Error:Unknown command "));
                TheChannelCommon::reply_append_ch(c, TheChannelCommon::TheGcodeParser::getCmdCode(c));
                TheChannelCommon::reply_append_uint16(c, TheChannelCommon::TheGcodeParser::getCmdNumber(c));
                TheChannelCommon::reply_append_ch(c, '\n');
                return TheChannelCommon::finishCommand(c);
            } break;
        }
    }
    
    template <typename TheChannelCommon>
    static void finish_locked_helper (Context c, WrapType<TheChannelCommon>)
    {
        TheChannelCommon::finishCommand(c);
    }
    
    static void finish_locked (Context c)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->locked)
        
        ListForEachForwardInterruptible<ChannelCommonList>(LForeach_run_for_state_command(), c, COMMAND_LOCKED, WrapType<PrinterMain>(), LForeach_finish_locked_helper());
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
        
        ob->last_active_time = Clock::getTime(c);
        ob->disable_timer.appendAt(c, ob->last_active_time + ob->inactive_time);
        TheBlinker::setInterval(c, (FpType)(Params::LedBlinkInterval::value() * Clock::time_freq));
    }
    
    static void now_active (Context c)
    {
        auto *ob = Object::self(c);
        
        ob->disable_timer.unset(c);
        TheBlinker::setInterval(c, (FpType)((Params::LedBlinkInterval::value() / 2) * Clock::time_freq));
    }
    
    static void set_force_timer (Context c)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->planner_state == PLANNER_RUNNING)
        AMBRO_ASSERT(ob->m_planning_pull_pending)
        
        TimeType force_time = Clock::getTime(c) + (TimeType)(Params::ForceTimeout::value() * Clock::time_freq);
        ob->force_timer.appendAt(c, force_time);
    }
    
    template <typename TheChannelCommon>
    static void continue_locking_helper (Context c, WrapType<TheChannelCommon> cc)
    {
        auto *ob = Object::self(c);
        auto *cco = TheChannelCommon::Object::self(c);
        AMBRO_ASSERT(!ob->locked)
        AMBRO_ASSERT(cco->m_cmd)
        AMBRO_ASSERT(cco->m_state == COMMAND_LOCKING)
        
        work_command(c, cc);
    }
    
    static void unlocked_timer_handler (typename Loop::QueuedEvent *, Context c)
    {
        auto *ob = Object::self(c);
        ob->debugAccess(c);
        
        if (!ob->locked) {
            ListForEachForwardInterruptible<ChannelCommonList>(LForeach_run_for_state_command(), c, COMMAND_LOCKING, WrapType<PrinterMain>(), LForeach_continue_locking_helper());
        }
    }
    
    static void disable_timer_handler (typename Loop::QueuedEvent *, Context c)
    {
        auto *ob = Object::self(c);
        ob->debugAccess(c);
        
        ListForEachForward<AxesList>(LForeach_disable_stepper(), c);
    }
    
    static void force_timer_handler (typename Loop::QueuedEvent *, Context c)
    {
        auto *ob = Object::self(c);
        ob->debugAccess(c);
        AMBRO_ASSERT(ob->planner_state == PLANNER_RUNNING)
        AMBRO_ASSERT(ob->m_planning_pull_pending)
        
        ThePlanner::waitFinished(c);
    }
    
    template <typename TheChannelCommon>
    static void continue_planned_helper (Context c, WrapType<TheChannelCommon> cc)
    {
        auto *ob = Object::self(c);
        auto *cco = TheChannelCommon::Object::self(c);
        AMBRO_ASSERT(ob->locked)
        AMBRO_ASSERT(ob->planner_state == PLANNER_RUNNING)
        AMBRO_ASSERT(ob->m_planning_pull_pending)
        AMBRO_ASSERT(cco->m_state == COMMAND_LOCKED)
        AMBRO_ASSERT(cco->m_cmd)
        
        work_command(c, cc);
    }
    
    static void planner_pull_handler (Context c)
    {
        auto *ob = Object::self(c);
        ob->debugAccess(c);
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
            ob->planner_state = PLANNER_RUNNING;
            ListForEachForwardInterruptible<ChannelCommonList>(LForeach_run_for_state_command(), c, COMMAND_LOCKED, WrapType<PrinterMain>(), LForeach_continue_planned_helper());
        } else if (ob->planner_state == PLANNER_RUNNING) {
            set_force_timer(c);
        } else {
            AMBRO_ASSERT(ob->planner_state == PLANNER_CUSTOM)
            ob->planner_client->pull_handler(c);
        }
    }
    
    template <typename TheChannelCommon>
    static void continue_unplanned_helper (Context c, WrapType<TheChannelCommon> cc)
    {
        auto *ob = Object::self(c);
        auto *cco = TheChannelCommon::Object::self(c);
        AMBRO_ASSERT(ob->locked)
        AMBRO_ASSERT(ob->planner_state == PLANNER_NONE)
        AMBRO_ASSERT(cco->m_state == COMMAND_LOCKED)
        AMBRO_ASSERT(cco->m_cmd)
        
        work_command(c, cc);
    }
    
    static void planner_finished_handler (Context c)
    {
        auto *ob = Object::self(c);
        ob->debugAccess(c);
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
            ListForEachForwardInterruptible<ChannelCommonList>(LForeach_run_for_state_command(), c, COMMAND_LOCKED, WrapType<PrinterMain>(), LForeach_continue_unplanned_helper());
        }
    }
    
    static void planner_aborted_handler (Context c)
    {
        auto *ob = Object::self(c);
        ob->debugAccess(c);
        AMBRO_ASSERT(ob->planner_state == PLANNER_CUSTOM)
        
        ListForEachForward<AxesList>(LForeach_fix_aborted_pos(), c);
        TransformFeature::do_pending_virt_update(c);
        ob->planner_client->aborted_handler(c);
    }
    
    static void planner_underrun_callback (Context c)
    {
        auto *ob = Object::self(c);
        ob->underrun_count++;
    }
    
    static void planner_channel_callback (typename ThePlanner::template Channel<0>::CallbackContext c, PlannerChannelPayload *payload)
    {
        auto *ob = Object::self(c);
        ob->debugAccess(c);
        
        ListForOneBoolOffset<HeatersList, 0>(payload->type, LForeach_channel_callback(), c, &payload->heaters) ||
        ListForOneBoolOffset<FansList, TypeListLength<ParamsHeatersList>::value>(payload->type, LForeach_channel_callback(), c, &payload->fans);
    }
    
    template <int AxisIndex>
    static bool planner_prestep_callback (typename ThePlanner::template Axis<AxisIndex>::StepperCommandCallbackContext c)
    {
        return
            ProbeFeature::prestep_callback(c) ||
            TransformFeature::prestep_callback(c);
    }
    
    template <typename TheLaser>
    struct MoveBuildLaserExtra {
        FpType energy;
        bool energy_specified;
        
        void init ()
        {
            energy = 0.0f;
            energy_specified = false;
        }
    };
    
    using MoveBuildLaserExtraTuple = Tuple<MapTypeList<LasersList, TemplateFunc<MoveBuildLaserExtra>>>;
    
    struct MoveBuildState : public MoveBuildLaserExtraTuple {
        MoveBuildLaserExtraTuple * laser_extra () { return this; }
        bool seen_cartesian;
    };
    
    static void move_begin (Context c, MoveBuildState *s)
    {
        ListForEachForward<PhysVirtAxisHelperList>(LForeach_init_new_pos(), c);
        s->seen_cartesian = false;
        TupleForEachForward(s->laser_extra(), TForeach_init());
    }
    
    template <int PhysVirtAxisIndex>
    static void move_add_axis (Context c, MoveBuildState *s, FpType value)
    {
        PhysVirtAxisHelper<PhysVirtAxisIndex>::update_new_pos(c, s, value);
    }
    
    template <int LaserIndex>
    static void move_add_laser (Context c, MoveBuildState *s, FpType energy)
    {
        auto *le = TupleGetElem<LaserIndex>(s->laser_extra());
        le->energy = FloatMakePosOrPosZero(energy);
        le->energy_specified = true;
    }
    
    struct ReqPosSrc {
        Context m_c;
        template <int Index>
        FpType get () { return Axis<Index>::Object::self(m_c)->m_req_pos; }
    };
    
    struct LaserExtraSrc {
        MoveBuildState *s;
        template <int LaserIndex>
        FpType get () { return TupleGetElem<LaserIndex>(s->laser_extra())->energy; }
    };
    
    static void move_end (Context c, MoveBuildState *s, FpType time_freq_by_max_speed)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->planner_state == PLANNER_RUNNING || ob->planner_state == PLANNER_CUSTOM)
        AMBRO_ASSERT(ob->m_planning_pull_pending)
        AMBRO_ASSERT(FloatIsPosOrPosZero(time_freq_by_max_speed))
        
        if (TransformFeature::is_splitting(c)) {
            TransformFeature::handle_virt_move(c, s, time_freq_by_max_speed);
            return;
        }
        PlannerSplitBuffer *cmd = ThePlanner::getBuffer(c);
        FpType distance_squared = 0.0f;
        FpType total_steps = 0.0f;
        ListForEachForward<AxesList>(LForeach_do_move(), c, ReqPosSrc{c}, WrapBool<true>(), &distance_squared, &total_steps, cmd);
        TransformFeature::do_pending_virt_update(c);
        if (total_steps != 0.0f) {
            cmd->axes.rel_max_v_rec = total_steps * (FpType)(1.0 / (Params::MaxStepsPerCycle::value() * F_CPU * Clock::time_unit));
            if (s->seen_cartesian) {
                FpType distance = FloatSqrt(distance_squared);
                cmd->axes.rel_max_v_rec = FloatMax(cmd->axes.rel_max_v_rec, distance * time_freq_by_max_speed);
                ListForEachForward<LasersList>(LForeach_handle_automatic_energy(), c, s, distance);
            } else {
                ListForEachForward<AxesList>(LForeach_limit_axis_move_speed(), c, time_freq_by_max_speed, cmd);
            }
            ListForEachForward<LasersList>(LForeach_write_planner_cmd(), c, LaserExtraSrc{s}, cmd);
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
    
    struct PlannerUnion {
        struct Object : public ObjUnionBase<PlannerUnion, typename PrinterMain::Object, MakeTypeList<
            PlannerUnionPlanner,
            PlannerUnionHoming
        >> {};
    };
    
    struct PlannerUnionPlanner {
        struct Object : public ObjBase<PlannerUnionPlanner, typename PlannerUnion::Object, MakeTypeList<ThePlanner>> {};
    };
    
    using HomingFeaturesList = MapTypeList<AxesList, GetMemberType_HomingFeature>;
    
    struct PlannerUnionHoming {
        struct Object : public ObjBase<PlannerUnionHoming, typename PlannerUnion::Object, HomingFeaturesList> {};
    };
    
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
            typename Axis<AxisIndex>::HomingFeature::template MakeAxisDriverConsumersList<typename Axis<AxisIndex>::HomingFeature>
        >;
    };
    
public:
    struct Object : public ObjBase<PrinterMain, ParentObject, JoinTypeLists<
        AxesList,
        LasersList,
        HeatersList,
        FansList,
        MakeTypeList<
            TheWatchdog,
            TheBlinker,
            TheSteppers,
            SerialFeature,
            SdCardFeature,
            TransformFeature,
            ProbeFeature,
            CurrentFeature,
            PlannerUnion
        >
    >>,
        public DebugObject<Context, void>
    {
        typename Loop::QueuedEvent unlocked_timer;
        typename Loop::QueuedEvent disable_timer;
        typename Loop::QueuedEvent force_timer;
        TimeType inactive_time;
        TimeType last_active_time;
        FpType time_freq_by_max_speed;
        uint32_t underrun_count;
        bool locked;
        uint8_t planner_state;
        PlannerClient *planner_client;
        bool m_planning_pull_pending;
        PhysVirtAxisMaskType m_homing_rem_axes;
    };
};

#include <aprinter/EndNamespace.h>

#endif
