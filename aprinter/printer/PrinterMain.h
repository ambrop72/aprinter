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
#include <aprinter/meta/FuncUtils.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/TupleGet.h>
#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/IndexElemList.h>
#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/Union.h>
#include <aprinter/meta/UnionGet.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/PowerOfTwo.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/Expr.h>
#include <aprinter/meta/If.h>
#include <aprinter/meta/CallIfExists.h>
#include <aprinter/meta/MemberType.h>
#include <aprinter/meta/ListCollect.h>
#include <aprinter/meta/TypeDict.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Likely.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Callback.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/math/PrintInt.h>
#include <aprinter/devices/Blinker.h>
#include <aprinter/driver/StepperGroups.h>
#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/printer/MotionPlanner.h>
#include <aprinter/printer/Configuration.h>
#include <aprinter/printer/ServiceList.h>
#include <aprinter/printer/GcodeCommand.h>

#include <aprinter/BeginNamespace.h>

template <
    typename TLedPin, typename TLedBlinkInterval, typename TInactiveTime,
    typename TSpeedLimitMultiply, typename TMaxStepsPerCycle,
    int TStepperSegmentBufferSize, int TLookaheadBufferSize,
    int TLookaheadCommitCount,
    typename TForceTimeout, typename TFpType,
    typename TWatchdogService,
    typename TConfigManagerService,
    typename TConfigList,
    typename TAxesList, typename TTransformParams,
    typename TLasersList, typename TModulesList
>
struct PrinterMainParams {
    using LedPin = TLedPin;
    using LedBlinkInterval = TLedBlinkInterval;
    using InactiveTime = TInactiveTime;
    using SpeedLimitMultiply = TSpeedLimitMultiply;
    using MaxStepsPerCycle = TMaxStepsPerCycle;
    static int const StepperSegmentBufferSize = TStepperSegmentBufferSize;
    static int const LookaheadBufferSize = TLookaheadBufferSize;
    static int const LookaheadCommitCount = TLookaheadCommitCount;
    using ForceTimeout = TForceTimeout;
    using FpType = TFpType;
    using WatchdogService = TWatchdogService;
    using ConfigManagerService = TConfigManagerService;
    using ConfigList = TConfigList;
    using AxesList = TAxesList;
    using TransformParams = TTransformParams;
    using LasersList = TLasersList;
    using ModulesList = TModulesList;
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
    typename TTransformService,
    typename TSplitterService
>
struct PrinterMainTransformParams {
    static bool const Enabled = true;
    using VirtAxesList = TVirtAxesList;
    using PhysAxesList = TPhysAxesList;
    using TransformService = TTransformService;
    using SplitterService = TSplitterService;
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
    
private:
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_init, init)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_deinit, deinit)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_limit_virt_axis_speed, limit_virt_axis_speed)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_check_phys_limits, check_phys_limits)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_prepare_split, prepare_split)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_compute_split, compute_split)
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
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_check_command, check_command)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_append_position, append_position)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_collect_new_pos, collect_new_pos)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_set_relative_positioning, set_relative_positioning)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_g92_check_axis, g92_check_axis)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_handle_automatic_energy, handle_automatic_energy)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_write_planner_cmd, write_planner_cmd)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_check_safety, check_safety)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_prepare_laser_for_move, prepare_laser_for_move)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_save_pos_to_old, save_pos_to_old)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_restore_pos_from_old, restore_pos_from_old)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_save_req_pos, save_req_pos)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_restore_req_pos, restore_req_pos)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_configuration_changed, configuration_changed)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_reverse_update_pos, reverse_update_pos)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_forward_update_pos, forward_update_pos)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_WrappedAxisName, WrappedAxisName)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_WrappedPhysAxisIndex, WrappedPhysAxisIndex)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_HomingState, HomingState)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_TheModule, TheModule)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_CorrectionFeature, CorrectionFeature)
    APRINTER_DEFINE_MEMBER_TYPE(MemberType_EventLoopFastEvents, EventLoopFastEvents)
    APRINTER_DEFINE_MEMBER_TYPE(MemberType_ConfigExprs, ConfigExprs)
    APRINTER_DEFINE_MEMBER_TYPE(MemberType_ProvidedServices, ProvidedServices)
    APRINTER_DEFINE_MEMBER_TYPE(MemberType_MotionPlannerChannels, MotionPlannerChannels)
    APRINTER_DEFINE_CALL_IF_EXISTS(CallIfExists_check_command, check_command)
    APRINTER_DEFINE_CALL_IF_EXISTS(CallIfExists_configuration_changed, configuration_changed)
    APRINTER_DEFINE_CALL_IF_EXISTS(CallIfExists_emergency, emergency)
    APRINTER_DEFINE_CALL_IF_EXISTS(CallIfExists_check_safety, check_safety)
    APRINTER_DEFINE_CALL_IF_EXISTS(CallIfExists_m119_append_endstop, m119_append_endstop)
    APRINTER_DEFINE_CALL_IF_EXISTS(CallIfExists_prestep_callback, prestep_callback)
    
    struct PlannerUnion;
    struct PlannerUnionPlanner;
    struct PlannerUnionHoming;
    struct ConfigManagerHandler;
    struct BlinkerHandler;
    struct PlannerPullHandler;
    struct PlannerFinishedHandler;
    struct PlannerAbortedHandler;
    struct PlannerUnderrunCallback;
    template <int AxisIndex> struct PlannerPrestepCallback;
    template <int AxisIndex> struct AxisDriverConsumersList;
    struct DelayedConfigExprs;
    
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using ParamsAxesList = typename Params::AxesList;
    using ParamsLasersList = typename Params::LasersList;
    using TransformParams = typename Params::TransformParams;
    using ParamsModulesList = typename Params::ModulesList;
    
    using TheDebugObject = DebugObject<Context, Object>;
    using TheWatchdog = typename Params::WatchdogService::template Watchdog<Context, Object>;
    using TheConfigManager = typename Params::ConfigManagerService::template ConfigManager<Context, Object, typename Params::ConfigList, PrinterMain, ConfigManagerHandler>;
    using TheConfigCache = ConfigCache<Context, Object, DelayedConfigExprs>;
    using TheBlinker = Blinker<Context, Object, typename Params::LedPin, BlinkerHandler>;
    
public:
    using FpType = typename Params::FpType;
    using Config = ConfigFramework<TheConfigManager, TheConfigCache>;
    static const int NumAxes = TypeListLength<ParamsAxesList>::Value;
    static const bool IsTransformEnabled = TransformParams::Enabled;
    
private:
    template <typename TheSlaveStepper>
    using MakeSlaveStepperDef = StepperDef<
        typename TheSlaveStepper::DirPin,
        typename TheSlaveStepper::StepPin,
        typename TheSlaveStepper::EnablePin,
        decltype(Config::e(TheSlaveStepper::InvertDir::i()))
    >;
    
    template <typename TheAxis>
    using MakeStepperGroupParams = StepperGroupParams<
        JoinTypeLists<
            MakeTypeList<
                StepperDef<
                    typename TheAxis::DirPin,
                    typename TheAxis::StepPin,
                    typename TheAxis::EnablePin,
                    decltype(Config::e(TheAxis::InvertDir::i()))
                >
            >,
            MapTypeList<typename TheAxis::SlaveSteppersList, TemplateFunc<MakeSlaveStepperDef>>
        >
    >;
    
    using StepperGroupParamsList = MapTypeList<ParamsAxesList, TemplateFunc<MakeStepperGroupParams>>;
    using TheSteppers = StepperGroups<Context, Object, Config, StepperGroupParamsList>;
    
    static_assert(Params::LedBlinkInterval::value() < TheWatchdog::WatchdogTime / 2.0, "");
    
public:
    using TimeConversion = APRINTER_FP_CONST_EXPR(Clock::time_freq);
    using TimeRevConversion = APRINTER_FP_CONST_EXPR(Clock::time_unit);
    using FCpu = APRINTER_FP_CONST_EXPR(F_CPU);
    
private:
    using CInactiveTimeTicks = decltype(ExprCast<TimeType>(Config::e(Params::InactiveTime::i()) * TimeConversion()));
    using CStepSpeedLimitFactor = decltype(ExprCast<FpType>(ExprRec(Config::e(Params::MaxStepsPerCycle::i()) * FCpu() * TimeRevConversion())));
    using CForceTimeoutTicks = decltype(ExprCast<TimeType>(Config::e(Params::ForceTimeout::i()) * TimeConversion()));
    
    using MyConfigExprs = MakeTypeList<CInactiveTimeTicks, CStepSpeedLimitFactor, CForceTimeoutTicks>;
    
    enum {COMMAND_IDLE, COMMAND_LOCKING, COMMAND_LOCKED};
    enum {PLANNER_NONE, PLANNER_RUNNING, PLANNER_STOPPING, PLANNER_WAITING, PLANNER_CUSTOM};
    
public:
    class CommandStreamCallback {
    public:
        virtual bool start_command_impl (Context c) = 0;
        virtual void finish_command_impl (Context c, bool no_ok) = 0;
        virtual void reply_poke_impl (Context c) = 0;
        virtual void reply_append_buffer_impl (Context c, char const *str, AMBRO_PGM_P pstr, size_t length) = 0;
        virtual void reply_append_ch_impl (Context c, char ch) = 0;
        virtual bool request_send_buf_event_impl (Context c, size_t length) = 0;
        virtual void cancel_send_buf_event_impl (Context c) = 0;
    };
    
    class CommandStream {
        friend PrinterMain;
        
    public:
        using TheGcodeCommand = GcodeCommand<Context, FpType>;
        
        void init (Context c, CommandStreamCallback *callback)
        {
            auto *mo = Object::self(c);
            
            m_state = COMMAND_IDLE;
            m_callback = callback;
            m_cmd = nullptr;
            m_error = false;
            m_send_buf_event_handler = nullptr;
            mo->command_stream_list.prepend(this);
        }
        
        void deinit (Context c)
        {
            auto *mo = Object::self(c);
            
            mo->command_stream_list.remove(this);
        }
        
        bool hasCommand (Context c)
        {
            return (bool)m_cmd;
        }
        
        void startCommand (Context c, TheGcodeCommand *cmd)
        {
            AMBRO_ASSERT(m_state == COMMAND_IDLE)
            AMBRO_ASSERT(!m_cmd)
            AMBRO_ASSERT(cmd)
            
            m_cmd = cmd;
            
            PartsSizeType num_parts = m_cmd->getNumParts(c);
            if (num_parts < 0) {
                AMBRO_PGM_P err = AMBRO_PSTR("unknown error");
                switch (num_parts) {
                    case GCODE_ERROR_NO_PARTS:       err = AMBRO_PSTR("empty command");          break;
                    case GCODE_ERROR_TOO_MANY_PARTS: err = AMBRO_PSTR("too many parts");         break;
                    case GCODE_ERROR_INVALID_PART:   err = AMBRO_PSTR("invalid part");           break;
                    case GCODE_ERROR_CHECKSUM:       err = AMBRO_PSTR("incorrect checksum");     break;
                    case GCODE_ERROR_RECV_OVERRUN:   err = AMBRO_PSTR("receive buffer overrun"); break;
                    case GCODE_ERROR_BAD_ESCAPE:     err = AMBRO_PSTR("bad escape sequence");    break;
                }
                reportError(c, err);
                return finishCommand(c);
            }
            
            if (!m_callback->start_command_impl(c)) {
                return finishCommand(c);
            }
            
            work_command(c, this);
        }
        
        void maybePauseLockingCommand (Context c)
        {
            AMBRO_ASSERT(!m_cmd || m_state == COMMAND_LOCKING)
            AMBRO_ASSERT(m_cmd || m_state == COMMAND_IDLE)
            
            m_state = COMMAND_IDLE;
        }
        
        bool maybeResumeLockingCommand (Context c)
        {
            auto *mo = Object::self(c);
            AMBRO_ASSERT(m_state == COMMAND_IDLE)
            
            if (!m_cmd) {
                return false;
            }
            m_state = COMMAND_LOCKING;
            if (!mo->unlocked_timer.isSet(c)) {
                mo->unlocked_timer.prependNowNotAlready(c);
            }
            return true;
        }
        
        void maybeCancelLockingCommand (Context c)
        {
            AMBRO_ASSERT(m_state != COMMAND_LOCKED)
            
            m_state = COMMAND_IDLE;
            m_cmd = nullptr;
        }
        
        void reportSendBufEventDirectly (Context c)
        {
            AMBRO_ASSERT(m_state == COMMAND_LOCKED)
            AMBRO_ASSERT(m_send_buf_event_handler)
            
            auto handler = m_send_buf_event_handler;
            m_send_buf_event_handler = nullptr;
            
            return handler(c);
        }
        
        bool haveError (Context c)
        {
            return m_error;
        }
        
        void clearError (Context c)
        {
            AMBRO_ASSERT(m_state == COMMAND_IDLE)
            
            m_error = false;
        }
        
    public:
        using PartsSizeType = typename TheGcodeCommand::PartsSizeType;
        using PartRef = typename TheGcodeCommand::PartRef;
        using SendBufEventHandler = void (*) (Context);
        
        void reportError (Context c, AMBRO_PGM_P errstr)
        {
            AMBRO_ASSERT(m_cmd)
            AMBRO_ASSERT(m_state == COMMAND_IDLE || m_state == COMMAND_LOCKED)
            
            m_error = true;
            if (errstr) {
                reply_append_pstr(c, AMBRO_PSTR("Error:"));
                reply_append_pstr(c, errstr);
                reply_append_ch(c, '\n');
            }
        }
        
        void finishCommand (Context c, bool no_ok = false)
        {
            auto *mo = Object::self(c);
            AMBRO_ASSERT(m_cmd)
            AMBRO_ASSERT(m_state == COMMAND_IDLE || m_state == COMMAND_LOCKED)
            AMBRO_ASSERT(!m_send_buf_event_handler)
            
            m_callback->finish_command_impl(c, no_ok);
            m_cmd = nullptr;
            if (m_state == COMMAND_LOCKED) {
                AMBRO_ASSERT(mo->locked)
                m_state = COMMAND_IDLE;
                unlock(c);
            }
        }
        
        bool tryLockedCommand (Context c)
        {
            auto *mo = Object::self(c);
            AMBRO_ASSERT(m_state != COMMAND_LOCKING || !mo->locked)
            AMBRO_ASSERT(m_state != COMMAND_LOCKED || mo->locked)
            AMBRO_ASSERT(m_cmd)
            
            if (m_state == COMMAND_LOCKED) {
                return true;
            }
            if (mo->locked) {
                m_state = COMMAND_LOCKING;
                return false;
            }
            m_state = COMMAND_LOCKED;
            lock(c);
            return true;
        }
        
        bool tryUnplannedCommand (Context c)
        {
            auto *mo = Object::self(c);
            
            if (!tryLockedCommand(c)) {
                return false;
            }
            AMBRO_ASSERT(mo->planner_state == PLANNER_NONE || mo->planner_state == PLANNER_RUNNING)
            if (mo->planner_state == PLANNER_NONE) {
                return true;
            }
            mo->planner_state = PLANNER_STOPPING;
            if (mo->m_planning_pull_pending) {
                ThePlanner::waitFinished(c);
                mo->force_timer.unset(c);
            }
            return false;
        }
        
        bool tryPlannedCommand (Context c)
        {
            auto *mo = Object::self(c);
            
            if (!tryLockedCommand(c)) {
                return false;
            }
            AMBRO_ASSERT(mo->planner_state == PLANNER_NONE || mo->planner_state == PLANNER_RUNNING)
            if (mo->planner_state == PLANNER_NONE) {
                ThePlanner::init(c, false);
                mo->planner_state = PLANNER_RUNNING;
                mo->m_planning_pull_pending = false;
                now_active(c);
            }
            if (mo->m_planning_pull_pending) {
                return true;
            }
            mo->planner_state = PLANNER_WAITING;
            return false;
        }
        
        char getCmdCode (Context c)
        {
            return m_cmd->getCmdCode(c);
        }
        
        uint16_t getCmdNumber (Context c)
        {
            return m_cmd->getCmdNumber(c);
        }
        
        PartsSizeType getNumParts (Context c)
        {
            return m_cmd->getNumParts(c);
        }
        
        PartRef getPart (Context c, PartsSizeType i)
        {
            return m_cmd->getPart(c, i);
        }
        
        char getPartCode (Context c, PartRef part)
        {
            return m_cmd->getPartCode(c, part);
        }
        
        FpType getPartFpValue (Context c, PartRef part)
        {
            return m_cmd->getPartFpValue(c, part);
        }
        
        uint32_t getPartUint32Value (Context c, PartRef part)
        {
            return m_cmd->getPartUint32Value(c, part);
        }
        
        char const * getPartStringValue (Context c, PartRef part)
        {
            return m_cmd->getPartStringValue(c, part);
        }
        
        void reply_poke (Context c)
        {
            m_callback->reply_poke_impl(c);
        }
        
        void reply_append_buffer (Context c, char const *str, size_t length)
        {
            m_callback->reply_append_buffer_impl(c, str, nullptr, length);
        }
        
        void reply_append_pbuffer (Context c, AMBRO_PGM_P pstr, size_t length)
        {
            m_callback->reply_append_buffer_impl(c, nullptr, pstr, length);
        }
        
        void reply_append_ch (Context c, char ch)
        {
            m_callback->reply_append_ch_impl(c, ch);
        }
        
        bool requestSendBufEvent (Context c, size_t length, SendBufEventHandler handler)
        {
            AMBRO_ASSERT(m_state == COMMAND_LOCKED)
            AMBRO_ASSERT(!m_send_buf_event_handler)
            AMBRO_ASSERT(length > 0)
            AMBRO_ASSERT(handler)
            
            if (!m_callback->request_send_buf_event_impl(c, length)) {
                return false;
            }
            m_send_buf_event_handler = handler;
            return true;
        }
        
        void cancelSendBufEvent (Context c)
        {
            AMBRO_ASSERT(m_state == COMMAND_LOCKED)
            AMBRO_ASSERT(m_send_buf_event_handler)
            
            m_callback->cancel_send_buf_event_impl(c);
            m_send_buf_event_handler = nullptr;
        }
        
        bool find_command_param (Context c, char code, PartRef *out_part)
        {
            PartsSizeType num_parts = getNumParts(c);
            for (PartsSizeType i = 0; i < num_parts; i++) {
                PartRef part = getPart(c, i);
                if (getPartCode(c, part) == code) {
                    if (out_part) {
                        *out_part = part;
                    }
                    return true;
                }
            }
            return false;
        }
        
        uint32_t get_command_param_uint32 (Context c, char code, uint32_t default_value)
        {
            PartRef part;
            if (!find_command_param(c, code, &part)) {
                return default_value;
            }
            return getPartUint32Value(c, part);
        }
        
        FpType get_command_param_fp (Context c, char code, FpType default_value)
        {
            PartRef part;
            if (!find_command_param(c, code, &part)) {
                return default_value;
            }
            return getPartFpValue(c, part);
        }
        
        char const * get_command_param_str (Context c, char code, char const *default_value)
        {
            PartRef part;
            if (!find_command_param(c, code, &part)) {
                return default_value;
            }
            char const *str = getPartStringValue(c, part);
            if (!str) {
                return default_value;
            }
            return str;
        }
        
        bool find_command_param_fp (Context c, char code, FpType *out)
        {
            PartRef part;
            if (!find_command_param(c, code, &part)) {
                return false;
            }
            *out = getPartFpValue(c, part);
            return true;
        }
        
        void reply_append_str (Context c, char const *str)
        {
            reply_append_buffer(c, str, strlen(str));
        }
        
        void reply_append_pstr (Context c, AMBRO_PGM_P pstr)
        {
            reply_append_pbuffer(c, pstr, AMBRO_PGM_STRLEN(pstr));
        }
        
        void reply_append_fp (Context c, FpType x)
        {
            char buf[30];
#if defined(AMBROLIB_AVR)
            uint8_t len = AMBRO_PGM_SPRINTF(buf, AMBRO_PSTR("%g"), x);
            reply_append_buffer(c, buf, len);
#else        
            FloatToStrSoft(x, buf);
            reply_append_buffer(c, buf, strlen(buf));
#endif
        }
        
        void reply_append_uint32 (Context c, uint32_t x)
        {
            char buf[11];
#if defined(AMBROLIB_AVR)
            uint8_t len = AMBRO_PGM_SPRINTF(buf, AMBRO_PSTR("%" PRIu32), x);
#else
            uint8_t len = PrintNonnegativeIntDecimal<uint32_t>(x, buf);
#endif
            reply_append_buffer(c, buf, len);
        }
        
        void reply_append_uint16 (Context c, uint16_t x)
        {
            char buf[6];
#if defined(AMBROLIB_AVR)
            uint8_t len = AMBRO_PGM_SPRINTF(buf, AMBRO_PSTR("%" PRIu16), x);
#else
            uint8_t len = PrintNonnegativeIntDecimal<uint16_t>(x, buf);
#endif
            reply_append_buffer(c, buf, len);
        }
        
        void reply_append_uint8 (Context c, uint8_t x)
        {
            char buf[4];
#if defined(AMBROLIB_AVR)
            uint8_t len = AMBRO_PGM_SPRINTF(buf, AMBRO_PSTR("%" PRIu8), x);
#else
            uint8_t len = PrintNonnegativeIntDecimal<uint8_t>(x, buf);
#endif
            reply_append_buffer(c, buf, len);
        }
        
    private:
        uint8_t m_state : 4;
        bool m_error : 1;
        CommandStreamCallback *m_callback;
        TheGcodeCommand *m_cmd;
        SendBufEventHandler m_send_buf_event_handler;
        DoubleEndedListNode<CommandStream> m_list_node;
    };
    
public:
    using TheCommand = CommandStream;
    using CommandPartRef = typename TheCommand::PartRef;
    
public:
    using MoveEndCallback = void(*)(Context c, bool error);
    
private:
    template <int ModuleIndex>
    struct Module {
        struct Object;
        using ModuleSpec = TypeListGet<ParamsModulesList, ModuleIndex>;
        using TheModule = typename ModuleSpec::template Module<Context, Object, PrinterMain>;
        
        static void init (Context c)
        {
            TheModule::init(c);
        }
        
        static void deinit (Context c)
        {
            TheModule::deinit(c);
        }
        
        static bool check_command (Context c, TheCommand *cmd)
        {
            return CallIfExists_check_command::template call_ret<TheModule, bool, true>(c, cmd);
        }
        
        static void configuration_changed (Context c)
        {
            CallIfExists_configuration_changed::template call_void<TheModule>(c);
        }
        
        static void emergency ()
        {
            CallIfExists_emergency::template call_void<TheModule>();
        }
        
        static void check_safety (Context c)
        {
            CallIfExists_check_safety::template call_void<TheModule>(c);
        }
        
        static void m119_append_endstop (Context c, TheCommand *cmd)
        {
            CallIfExists_m119_append_endstop::template call_void<TheModule>(c, cmd);
        }
        
        template <typename CallbackContext>
        static bool prestep_callback (CallbackContext c)
        {
            return !CallIfExists_prestep_callback::template call_ret<TheModule, bool, false>(c);
        }
        
        struct Object : public ObjBase<Module, typename PrinterMain::Object, MakeTypeList<
            TheModule
        >> {};
    };
    using ModulesList = IndexElemList<ParamsModulesList, Module>;
    
    using ServicesDict = ListGroup<ListCollect<ParamsModulesList, MemberType_ProvidedServices>>;
    
    template <typename ServiceType>
    using HasServiceProvider = WrapBool<TypeDictFind<ServicesDict, ServiceType>::Found>;
    
    template <typename ServiceType>
    struct FindServiceProvider {
        using FindResult = TypeDictFind<ServicesDict, ServiceType>;
        static_assert(FindResult::Found, "The requested service type is not provided by any module.");
        static int const ModuleIndex = TypeListGet<typename FindResult::Result, 0>::Value;
    };
    
public:
    template <int ModuleIndex>
    using GetModule = typename Module<ModuleIndex>::TheModule;
    
    template <typename ServiceType>
    using GetServiceProviderModule = GetModule<FindServiceProvider<ServiceType>::ModuleIndex>;
    
    template <typename This=PrinterMain>
    using GetFsAccess = typename This::template GetServiceProviderModule<ServiceList::FsAccessService>::template GetFsAccess<>;
    
private:
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
        
        using DistConversion = decltype(Config::e(AxisSpec::DefaultStepsPerUnit::i()));
        using SpeedConversion = decltype(Config::e(AxisSpec::DefaultStepsPerUnit::i()) / TimeConversion());
        using AccelConversion = decltype(Config::e(AxisSpec::DefaultStepsPerUnit::i()) / (TimeConversion() * TimeConversion()));
        
        using AbsStepFixedTypeMin = APRINTER_FP_CONST_EXPR(AbsStepFixedType::minValue().fpValueConstexpr());
        using AbsStepFixedTypeMax = APRINTER_FP_CONST_EXPR(AbsStepFixedType::maxValue().fpValueConstexpr());
        
        using MinReqPos = decltype(ExprFmax(Config::e(AxisSpec::DefaultMin::i()), AbsStepFixedTypeMin() / DistConversion()));
        using MaxReqPos = decltype(ExprFmin(Config::e(AxisSpec::DefaultMax::i()), AbsStepFixedTypeMax() / DistConversion()));
        
        using PlannerMaxSpeedRec = decltype(ExprRec(Config::e(AxisSpec::DefaultMaxSpeed::i()) * SpeedConversion()));
        using PlannerMaxAccelRec = decltype(ExprRec(Config::e(AxisSpec::DefaultMaxAccel::i()) * AccelConversion()));
        
        template <typename ThePrinterMain=PrinterMain>
        static constexpr typename ThePrinterMain::PhysVirtAxisMaskType AxisMask () { return (PhysVirtAxisMaskType)1 << AxisIndex; }
        
        AMBRO_STRUCT_IF(HomingFeature, HomingSpec::Enabled) {
            struct Object;
            
            using HomerInstance = typename HomingSpec::HomerService::template Instance<
                Context, Config, FpType, AxisSpec::StepBits, Params::StepperSegmentBufferSize,
                Params::LookaheadBufferSize, decltype(Config::e(AxisSpec::DefaultMaxAccel::i())),
                DistConversion, TimeConversion, decltype(Config::e(HomingSpec::HomeDir::i()))
            >;
            
            using HomerGlobal = typename HomerInstance::template HomerGlobal<Object>;
            
            struct HomingState {
                struct Object;
                struct HomerFinishedHandler;
                
                using Homer = typename HomerInstance::template Homer<Object, HomerGlobal, TheAxisDriver, HomerFinishedHandler>;
                
                static void homer_finished_handler (Context c, bool success)
                {
                    auto *axis = Axis::Object::self(c);
                    auto *mob = PrinterMain::Object::self(c);
                    AMBRO_ASSERT(mob->axis_homing & AxisMask())
                    AMBRO_ASSERT(mob->locked)
                    AMBRO_ASSERT(mob->m_homing_rem_axes & AxisMask())
                    
                    Homer::deinit(c);
                    axis->m_req_pos = APRINTER_CFG(Config, CInitPosition, c);
                    axis->m_end_pos = AbsStepFixedType::importFpSaturatedRound(axis->m_req_pos * APRINTER_CFG(Config, CDistConversion, c));
                    mob->axis_homing &= ~AxisMask();
                    TransformFeature::template mark_phys_moved<AxisIndex>(c);
                    mob->m_homing_rem_axes &= ~AxisMask();
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
                auto *mob = PrinterMain::Object::self(c);
                AMBRO_ASSERT(!(mob->axis_homing & AxisMask()))
                HomerGlobal::init(c);
            }
            
            static void deinit (Context c)
            {
                auto *mob = PrinterMain::Object::self(c);
                if (mob->axis_homing & AxisMask()) {
                    HomingState::Homer::deinit(c);
                }
            }
            
            static void start_phys_homing (Context c)
            {
                auto *mob = PrinterMain::Object::self(c);
                AMBRO_ASSERT(!(mob->axis_homing & AxisMask()))
                AMBRO_ASSERT(mob->m_homing_rem_axes & AxisMask())
                
                Stepper::enable(c);
                HomingState::Homer::init(c);
                mob->axis_homing |= AxisMask();
            }
            
            using InitPosition = decltype(ExprIf(Config::e(HomingSpec::HomeDir::i()), MaxReqPos(), MinReqPos()));
            
            template <typename ThisContext>
            static bool endstop_is_triggered (ThisContext c)
            {
                return HomerGlobal::endstop_is_triggered(c);
            }
            
            struct Object : public ObjBase<HomingFeature, typename Axis::Object, MakeTypeList<
                HomerGlobal
            >> {};
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
            auto *mob = PrinterMain::Object::self(c);
            AMBRO_ASSERT(!(mob->axis_relative & AxisMask()))
            TheAxisDriver::init(c);
            HomingFeature::init(c);
            MicroStepFeature::init(c);
            o->m_req_pos = APRINTER_CFG(Config, CInitPosition, c);
            forward_update_pos(c);
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
        
        static void update_new_pos (Context c, FpType req, bool ignore_limits)
        {
            auto *o = Object::self(c);
            auto *mo = PrinterMain::Object::self(c);
            o->m_req_pos = ignore_limits ? req : clamp_req_pos(c, req);
            if (AxisSpec::IsCartesian) {
                mo->move_seen_cartesian = true;
            }
            TransformFeature::template mark_phys_moved<AxisIndex>(c);
        }
        
        static void save_req_pos (Context c, FpType *data)
        {
            auto *o = Object::self(c);
            data[AxisIndex] = o->m_req_pos;
        }
        
        static void restore_req_pos (Context c, FpType const *data)
        {
            auto *o = Object::self(c);
            o->m_req_pos = data[AxisIndex];
        }
        
        template <typename PlannerCmd>
        static void do_move (Context c, bool add_distance, FpType *distance_squared, FpType *total_steps, PlannerCmd *cmd)
        {
            auto *o = Object::self(c);
            AbsStepFixedType new_end_pos = AbsStepFixedType::importFpSaturatedRound(o->m_req_pos * APRINTER_CFG(Config, CDistConversion, c));
            bool dir = (new_end_pos >= o->m_end_pos);
            StepFixedType move = StepFixedType::importBits(dir ? 
                ((typename StepFixedType::IntType)new_end_pos.bitsValue() - (typename StepFixedType::IntType)o->m_end_pos.bitsValue()) :
                ((typename StepFixedType::IntType)o->m_end_pos.bitsValue() - (typename StepFixedType::IntType)new_end_pos.bitsValue())
            );
            if (AMBRO_UNLIKELY(move.bitsValue() != 0)) {
                if (add_distance && AxisSpec::IsCartesian) {
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
                reverse_update_pos(c);
                TransformFeature::template mark_phys_moved<AxisIndex>(c);
            }
        }
        
        static void forward_update_pos (Context c)
        {
            auto *o = Object::self(c);
            o->m_end_pos = AbsStepFixedType::importFpSaturatedRound(o->m_req_pos * APRINTER_CFG(Config, CDistConversion, c));
        }
        
        static void reverse_update_pos (Context c)
        {
            auto *o = Object::self(c);
            o->m_req_pos = o->m_end_pos.template fpValue<FpType>() * APRINTER_CFG(Config, CDistConversionRec, c);
        }
        
        static void set_position_try (Context c, FpType value)
        {
            auto *o = Object::self(c);
            o->m_req_pos = clamp_req_pos(c, value);
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
        };
    };
    
    using AxesList = IndexElemList<ParamsAxesList, Axis>;
    
    template <int AxisName>
    using FindAxis = TypeListIndexMapped<AxesList, GetMemberType_WrappedAxisName, WrapInt<AxisName>>;
    
    template <typename TheAxis>
    using MakePlannerAxisSpec = MotionPlannerAxisSpec<
        typename TheAxis::TheAxisDriver,
        TheAxis::AxisSpec::StepBits,
        decltype(Config::e(TheAxis::AxisSpec::DefaultDistanceFactor::i())),
        decltype(Config::e(TheAxis::AxisSpec::DefaultCorneringDistance::i())),
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
        
        using MaxPower = decltype(Config::e(LaserSpec::MaxPower::i()));
        using LaserPower = decltype(Config::e(LaserSpec::LaserPower::i()));
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
        
        static void prepare_laser_for_move (Context c)
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
    
public:
    struct PlannerClient {
        virtual void pull_handler (Context c) = 0;
        virtual void finished_handler (Context c) = 0;
        virtual void aborted_handler (Context c) = 0;
    };
    
public:
    AMBRO_STRUCT_IF(TransformFeature, TransformParams::Enabled) {
        friend PrinterMain;
        
    public:
        struct Object;
        
    private:
        using ParamsVirtAxesList = typename TransformParams::VirtAxesList;
        using ParamsPhysAxesList = typename TransformParams::PhysAxesList;
        using TheTransformAlg = typename TransformParams::TransformService::template Transform<Context, Object, Config, FpType>;
        using TheSplitterClass = typename TransformParams::SplitterService::template Splitter<Context, Object, Config, FpType>;
        using TheSplitter = typename TheSplitterClass::Splitter;
        
    public:
        static int const NumVirtAxes = TheTransformAlg::NumAxes;
        
    private:
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
        
        struct ArraySrc {
            FpType const *m_arr;
            template <int Index> FpType get () { return m_arr[Index]; }
        };
        
        struct ArrayDst {
            FpType *m_arr;
            template <int Index> void set (FpType x) { m_arr[Index] = x; }
        };
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            ListForEachForward<VirtAxesList>(LForeach_init(), c);
            update_virt_from_phys(c);
            o->virt_update_pending = false;
            o->splitting = false;
        }
        
        static void update_virt_from_phys (Context c)
        {
            if (TheCorrectionService::CorrectionEnabled) {
                FpType temp_virt_pos[NumVirtAxes];
                TheTransformAlg::physToVirt(c, PhysReqPosSrc{c}, ArrayDst{temp_virt_pos});
                TheCorrectionService::do_correction(c, ArraySrc{temp_virt_pos}, VirtReqPosDst{c}, WrapBool<true>());
            } else {
                TheTransformAlg::physToVirt(c, PhysReqPosSrc{c}, VirtReqPosDst{c});
            }
        }
        
        static void update_phys_from_virt (Context c)
        {
            if (TheCorrectionService::CorrectionEnabled) {
                FpType temp_virt_pos[NumVirtAxes];
                TheCorrectionService::do_correction(c, VirtReqPosSrc{c}, ArrayDst{temp_virt_pos}, WrapBool<false>());
                TheTransformAlg::virtToPhys(c, ArraySrc{temp_virt_pos}, PhysReqPosDst{c});
            } else {
                TheTransformAlg::virtToPhys(c, VirtReqPosSrc{c}, PhysReqPosDst{c});
            }
        }
        
        static bool check_all_phys_limits (Context c)
        {
            return ListForEachForwardInterruptible<VirtAxesList>(LForeach_check_phys_limits(), c);
        }
        
        static void handle_virt_move (Context c, FpType time_freq_by_max_speed, MoveEndCallback callback, bool is_positioning_move)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->splitting)
            
            o->move_end_callback = callback;
            o->virt_update_pending = false;
            update_phys_from_virt(c);
            
            if (!check_all_phys_limits(c)) {
                ListForEachForward<PhysVirtAxisHelperList>(LForeach_restore_pos_from_old(), c);
                return handle_transform_error(c);
            }
            
            FpType distance_squared = 0.0f;
            ListForEachForward<VirtAxesList>(LForeach_prepare_split(), c, &distance_squared);
            ListForEachForward<SecondaryAxesList>(LForeach_prepare_split(), c, &distance_squared);
            FpType distance = FloatSqrt(distance_squared);
            
            ListForEachForward<LasersList>(LForeach_handle_automatic_energy(), c, distance, is_positioning_move);
            ListForEachForward<LaserSplitsList>(LForeach_prepare_split(), c);
            
            FpType base_max_v_rec = ListForEachForwardAccRes<VirtAxesList>(distance * time_freq_by_max_speed, LForeach_limit_virt_axis_speed(), c);
            o->splitter.start(c, distance, base_max_v_rec, time_freq_by_max_speed);
            o->frac = 0.0;
            
            return do_split(c);
        }
        
        static void handle_transform_error (Context c)
        {
            auto *o = Object::self(c);
            auto *mob = PrinterMain::Object::self(c);
            AMBRO_ASSERT(o->splitting)
            AMBRO_ASSERT(mob->planner_state != PLANNER_NONE)
            AMBRO_ASSERT(mob->m_planning_pull_pending)
            
            print_pgm_string(c, AMBRO_PSTR("//Error:Transform\n"));
            
            o->virt_update_pending = false;
            o->splitting = false;
            
            ThePlanner::emptyDone(c);
            submitted_planner_command(c);
            
            return o->move_end_callback(c, true);
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
                FpType saved_virt_rex_pos[NumVirtAxes];
                FpType saved_phys_req_pos[NumAxes];
                
                if (o->splitter.pull(c, &rel_max_v_rec, &o->frac)) {
                    ListForEachForward<AxesList>(LForeach_save_req_pos(), c, saved_phys_req_pos);
                    
                    FpType saved_virt_req_pos[NumVirtAxes];
                    ListForEachForward<VirtAxesList>(LForeach_save_req_pos(), c, saved_virt_req_pos);
                    ListForEachForward<VirtAxesList>(LForeach_compute_split(), c, o->frac);
                    update_phys_from_virt(c);
                    ListForEachForward<VirtAxesList>(LForeach_restore_req_pos(), c, saved_virt_req_pos);
                    
                    if (!check_all_phys_limits(c)) {
                        // Compute actual positions based on prev_frac.
                        ListForEachForward<VirtAxesList>(LForeach_compute_split(), c, prev_frac);
                        update_phys_from_virt(c);
                        ListForEachForward<SecondaryAxesList>(LForeach_compute_split(), c, prev_frac, saved_phys_req_pos);
                        return handle_transform_error(c);
                    }
                    
                    ListForEachForward<SecondaryAxesList>(LForeach_compute_split(), c, o->frac, saved_phys_req_pos);
                } else {
                    o->frac = 1.0;
                    o->splitting = false;
                }
                
                PlannerSplitBuffer *cmd = ThePlanner::getBuffer(c);
                FpType total_steps = 0.0f;
                ListForEachForward<AxesList>(LForeach_do_move(), c, false, (FpType *)0, &total_steps, cmd);
                if (o->splitting) {
                    ListForEachForward<AxesList>(LForeach_restore_req_pos(), c, saved_phys_req_pos);
                }
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
            if (!o->splitting) {
                return o->move_end_callback(c, false);
            }
        }
        
        static void handle_aborted (Context c)
        {
            auto *o = Object::self(c);
            
            if (o->splitting) {
                o->virt_update_pending = true;
                o->splitting = false;
            }
            do_pending_virt_update(c);
        }
        
        static bool handle_set_position (Context c)
        {
            auto *o = Object::self(c);
            
            if (o->splitting) {
                o->splitting = false;
                o->virt_update_pending = false;
                update_phys_from_virt(c);
                if (!check_all_phys_limits(c)) {
                    print_pgm_string(c, AMBRO_PSTR("//Error:Transform\n"));
                    return false;
                }
            } else {
                do_pending_virt_update(c);
            }
            return true;
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
        
    public:
        static void handle_corrections_change (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(!o->splitting)
            
            update_virt_from_phys(c);
        }
        
    private:
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
            
            template <typename ThePrinterMain=PrinterMain>
            static constexpr typename ThePrinterMain::PhysVirtAxisMaskType AxisMask () { return (PhysVirtAxisMaskType)1 << (NumAxes + VirtAxisIndex); }
            
            static void init (Context c)
            {
                auto *mo = PrinterMain::Object::self(c);
                AMBRO_ASSERT(!(mo->axis_relative & AxisMask()))
                HomingFeature::init(c);
            }
            
            static void update_new_pos (Context c, FpType req, bool ignore_limits)
            {
                auto *o = Object::self(c);
                auto *t = TransformFeature::Object::self(c);
                o->m_req_pos = ignore_limits ? req : clamp_virt_pos(c, req);
                t->splitting = true;
            }
            
            static bool check_phys_limits (Context c)
            {
                auto *axis = ThePhysAxis::Object::self(c);
                auto *t = TransformFeature::Object::self(c);
                return AMBRO_LIKELY(axis->m_req_pos >= APRINTER_CFG(Config, typename ThePhysAxis::CMinReqPos, c)) &&
                       AMBRO_LIKELY(axis->m_req_pos <= APRINTER_CFG(Config, typename ThePhysAxis::CMaxReqPos, c));
            }
            
            static void prepare_split (Context c, FpType *distance_squared)
            {
                auto *o = Object::self(c);
                o->m_delta = o->m_req_pos - o->m_old_pos;
                *distance_squared += o->m_delta * o->m_delta;
            }
            
            static void save_req_pos (Context c, FpType *data)
            {
                auto *o = Object::self(c);
                data[VirtAxisIndex] = o->m_req_pos;
            }
            
            static void restore_req_pos (Context c, FpType const *data)
            {
                auto *o = Object::self(c);
                o->m_req_pos = data[VirtAxisIndex];
            }
            
            static void compute_split (Context c, FpType frac)
            {
                auto *o = Object::self(c);
                o->m_req_pos = o->m_old_pos + (frac * o->m_delta);
            }
            
            static void set_position_try (Context c, FpType value)
            {
                auto *o = Object::self(c);
                auto *t = TransformFeature::Object::self(c);
                o->m_req_pos = clamp_virt_pos(c, value);
                t->splitting = true;
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
                    
                    if (!(mo->m_homing_rem_axes & AxisMask())) {
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
                    set_position_begin(c);
                    set_position_add_axis<(NumAxes + VirtAxisIndex)>(c, value);
                    set_position_end(c);
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
                
                static void virt_homing_move_end_callback (Context c, bool error)
                {
                    // TBD: Handle error.
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
                        bool ignore_limits = false;
                        switch (o->state) {
                            case 0: {
                                position = home_end_pos(c) + home_dir(c) * APRINTER_CFG(Config, CFastExtraDist, c);
                                speed = APRINTER_CFG(Config, CFastSpeed, c);
                                ignore_limits = true;
                            } break;
                            case 1: {
                                position = home_end_pos(c) - home_dir(c) * APRINTER_CFG(Config, CRetractDist, c);
                                speed = APRINTER_CFG(Config, CRetractSpeed, c);
                            } break;
                            case 2: {
                                position = home_end_pos(c) + home_dir(c) * APRINTER_CFG(Config, CSlowExtraDist, c);
                                speed = APRINTER_CFG(Config, CSlowSpeed, c);
                                ignore_limits = true;
                            } break;
                        }
                        move_add_axis<(NumAxes + VirtAxisIndex)>(c, position, ignore_limits);
                        o->command_sent = true;
                        return move_end(c, (FpType)TimeConversion::value() / speed, HomingFeature::virt_homing_move_end_callback);
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
                        mo->m_homing_rem_axes &= ~AxisMask();
                        work_virt_homing(c);
                    }
                    
                    void aborted_handler (Context c)
                    {
                        finished_handler(c);
                    }
                };
                
                using CEndInvert = decltype(ExprCast<bool>(Config::e(HomingSpec::EndInvert::i())));
                using CHomeDir = decltype(ExprCast<bool>(Config::e(HomingSpec::HomeDir::i())));
                using CFastExtraDist = decltype(ExprCast<FpType>(Config::e(HomingSpec::FastExtraDist::i())));
                using CRetractDist = decltype(ExprCast<FpType>(Config::e(HomingSpec::RetractDist::i())));
                using CSlowExtraDist = decltype(ExprCast<FpType>(Config::e(HomingSpec::SlowExtraDist::i())));
                using CFastSpeed = decltype(ExprCast<FpType>(Config::e(HomingSpec::FastSpeed::i())));
                using CRetractSpeed = decltype(ExprCast<FpType>(Config::e(HomingSpec::RetractSpeed::i())));
                using CSlowSpeed = decltype(ExprCast<FpType>(Config::e(HomingSpec::SlowSpeed::i())));
                
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
            
            using CMinPos = decltype(ExprCast<FpType>(Config::e(VirtAxisParams::MinPos::i())));
            using CMaxPos = decltype(ExprCast<FpType>(Config::e(VirtAxisParams::MaxPos::i())));
            using CMaxSpeedFactor = decltype(ExprCast<FpType>(TimeConversion() / Config::e(VirtAxisParams::MaxSpeed::i())));
            
            using ConfigExprs = MakeTypeList<CMinPos, CMaxPos, CMaxSpeedFactor>;
            
        public:
            struct Object : public ObjBase<VirtAxis, typename TransformFeature::Object, MakeTypeList<
                TheTransformAlg,
                TheSplitterClass,
                HomingFeature
            >>
            {
                FpType m_req_pos;
                FpType m_old_pos;
                FpType m_delta;
            };
        };
        
        using VirtAxesList = IndexElemList<ParamsVirtAxesList, VirtAxis>;
        
        template <typename PhysAxisIndex>
        using IsPhysAxisTransformPhys = WrapBool<
            TypeListFindMapped<VirtAxesList, GetMemberType_WrappedPhysAxisIndex, PhysAxisIndex>::Found
        >;
        
        using SecondaryAxisIndices = FilterTypeList<
            SequenceList<NumAxes>,
            ComposeFunctions<NotFunc, TemplateFunc<IsPhysAxisTransformPhys>>
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
            
            static void compute_split (Context c, FpType frac, FpType const *saved_phys_req_pos)
            {
                auto *axis = TheAxis::Object::self(c);
                axis->m_req_pos = axis->m_old_pos + (frac * (saved_phys_req_pos[AxisIndex] - axis->m_old_pos));
            }
        };
        
        using SecondaryAxesList = IndexElemList<SecondaryAxisIndices, SecondaryAxis>;
        
        template <int LaserIndex>
        struct LaserSplit {
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
        
    public:
        struct Object : public ObjBase<TransformFeature, typename PrinterMain::Object, JoinTypeLists<
            VirtAxesList,
            LaserSplitsList
        >> {
            bool virt_update_pending;
            bool splitting;
            FpType frac;
            TheSplitter splitter;
            MoveEndCallback move_end_callback;
        };
    } AMBRO_STRUCT_ELSE(TransformFeature) {
        static int const NumVirtAxes = 0;
        static void init (Context c) {}
        static void handle_virt_move (Context c, FpType time_freq_by_max_speed, MoveEndCallback callback, bool is_positioning_move) {}
        template <int PhysAxisIndex>
        static void mark_phys_moved (Context c) {}
        static void do_pending_virt_update (Context c) {}
        static bool is_splitting (Context c) { return false; }
        static void do_split (Context c) {}
        static void handle_aborted (Context c) {}
        static bool handle_set_position (Context c) { return true; }
        static bool start_virt_homing (Context c) { return true; }
        template <typename CallbackContext>
        static bool prestep_callback (CallbackContext c) { return false; }
        struct Object {};
    };
    
public:
    static int const NumPhysVirtAxes = NumAxes + TransformFeature::NumVirtAxes;
    using PhysVirtAxisMaskType = ChooseInt<NumPhysVirtAxes, false>;
    static PhysVirtAxisMaskType const PhysAxisMask = PowerOfTwoMinusOne<PhysVirtAxisMaskType, NumAxes>::Value;
    
    template <int PhysVirtAxisIndex>
    using IsVirtAxis = WrapBool<(PhysVirtAxisIndex >= NumAxes)>;
    
    template <int PhysVirtAxisIndex>
    using GetVirtAxisVirtIndex = WrapInt<(PhysVirtAxisIndex - NumAxes)>;
    
private:
    template <bool IsVirt, int PhysVirtAxisIndex>
    struct GetPhysVirtAxisHelper {
        using Type = Axis<PhysVirtAxisIndex>;
    };
    
    template <int PhysVirtAxisIndex>
    struct GetPhysVirtAxisHelper<true, PhysVirtAxisIndex> {
        using Type = typename TransformFeature::template VirtAxis<(PhysVirtAxisIndex - NumAxes)>;
    };
    
    template <int PhysVirtAxisIndex>
    using GetPhysVirtAxis = typename GetPhysVirtAxisHelper<IsVirtAxis<PhysVirtAxisIndex>::Value, PhysVirtAxisIndex>::Type;
    
public:
    template <int PhysVirtAxisIndex>
    struct PhysVirtAxisHelper {
        friend PrinterMain;
        
    private:
        using TheAxis = GetPhysVirtAxis<PhysVirtAxisIndex>;
        
    public:
        static char const AxisName = TheAxis::AxisName;
        using WrappedAxisName = WrapInt<AxisName>;
        static PhysVirtAxisMaskType const AxisMask = TheAxis::AxisMask();
        
        static FpType get_position (Context c)
        {
            auto *axis = TheAxis::Object::self(c);
            return axis->m_req_pos;
        }
        
    private:
        static void save_pos_to_old (Context c)
        {
            auto *axis = TheAxis::Object::self(c);
            axis->m_old_pos = axis->m_req_pos;
        }
        
        static void restore_pos_from_old (Context c)
        {
            auto *axis = TheAxis::Object::self(c);
            axis->m_req_pos = axis->m_old_pos;
        }
        
        static void update_new_pos (Context c, FpType req, bool ignore_limits)
        {
            TheAxis::update_new_pos(c, req, ignore_limits);
        }
        
        static bool collect_new_pos (Context c, TheCommand *cmd, CommandPartRef part)
        {
            auto *axis = TheAxis::Object::self(c);
            auto *mo = PrinterMain::Object::self(c);
            
            if (AMBRO_UNLIKELY(cmd->getPartCode(c, part) == TheAxis::AxisName)) {
                FpType req = cmd->getPartFpValue(c, part);
                if (mo->axis_relative & AxisMask) {
                    req += axis->m_old_pos;
                }
                move_add_axis<PhysVirtAxisIndex>(c, req);
                return false;
            }
            return true;
        }
        
        static void set_relative_positioning (Context c, bool relative)
        {
            auto *mo = PrinterMain::Object::self(c);
            if (relative) {
                mo->axis_relative |= AxisMask;
            } else {
                mo->axis_relative &= ~AxisMask;
            }
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
        
        static void g92_check_axis (Context c, TheCommand *cmd, CommandPartRef part)
        {
            if (cmd->getPartCode(c, part) == TheAxis::AxisName) {
                FpType value = cmd->getPartFpValue(c, part);
                set_position_add_axis<PhysVirtAxisIndex>(c, value);
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
    
public:
    using PhysVirtAxisHelperList = IndexElemListCount<NumPhysVirtAxes, PhysVirtAxisHelper>;
    
    template <int AxisName>
    using FindPhysVirtAxis = TypeListIndexMapped<PhysVirtAxisHelperList, GetMemberType_WrappedAxisName, WrapInt<AxisName>>;
    
private:
    using ModuleClassesList = MapTypeList<ModulesList, GetMemberType_TheModule>;
    using MotionPlannerChannelsDict = ListCollect<ModuleClassesList, MemberType_MotionPlannerChannels>;
    
    using MotionPlannerChannels = TypeDictValues<MotionPlannerChannelsDict>;
    using MotionPlannerAxes = MapTypeList<AxesList, TemplateFunc<MakePlannerAxisSpec>>;
    using MotionPlannerLasers = MapTypeList<LasersList, TemplateFunc<MakePlannerLaserSpec>>;
    
public:
    using ThePlanner = MotionPlanner<Context, typename PlannerUnionPlanner::Object, Config, MotionPlannerAxes, Params::StepperSegmentBufferSize, Params::LookaheadBufferSize, Params::LookaheadCommitCount, FpType, PlannerPullHandler, PlannerFinishedHandler, PlannerAbortedHandler, PlannerUnderrunCallback, MotionPlannerChannels, MotionPlannerLasers>;
    using PlannerSplitBuffer = typename ThePlanner::SplitBuffer;
    
    template <typename PlannerChannelSpec>
    using GetPlannerChannelIndex = TypeListIndex<MotionPlannerChannels, PlannerChannelSpec>;
    
private:
    struct DummyCorrectionService {
        static bool const CorrectionEnabled = false;
        template <typename Src, typename Dst, bool Reverse> static void do_correction (Context c, Src src, Dst dst, WrapBool<Reverse>) {}
    };
    
    using TheCorrectionService = FuncCall<
        IfFunc<
            TemplateFunc<HasServiceProvider>,
            ComposeFunctions<GetMemberType_CorrectionFeature, TemplateFunc<GetServiceProviderModule>>,
            ConstantFunc<DummyCorrectionService>
        >,
        typename ServiceList::CorrectionService
    >;
    
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
        ob->unlocked_timer.init(c, APRINTER_CB_STATFUNC_T(&PrinterMain::unlocked_timer_handler));
        ob->disable_timer.init(c, APRINTER_CB_STATFUNC_T(&PrinterMain::disable_timer_handler));
        ob->force_timer.init(c, APRINTER_CB_STATFUNC_T(&PrinterMain::force_timer_handler));
        TheBlinker::init(c, (FpType)(Params::LedBlinkInterval::value() * TimeConversion::value()));
        TheSteppers::init(c);
        ob->axis_homing = 0;
        ob->axis_relative = 0;
        ListForEachForward<AxesList>(LForeach_init(), c);
        ListForEachForward<LasersList>(LForeach_init(), c);
        TransformFeature::init(c);
        ob->time_freq_by_max_speed = 0.0f;
        ob->underrun_count = 0;
        ob->locked = false;
        ob->planner_state = PLANNER_NONE;
        ob->command_stream_list.init();
        ListForEachForward<ModulesList>(LForeach_init(), c);
        
        print_pgm_string(c, AMBRO_PSTR("start\nAPrinter\n"));
        
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
        ListForEachReverse<ModulesList>(LForeach_deinit(), c);
        AMBRO_ASSERT(ob->command_stream_list.isEmpty())
        ListForEachReverse<LasersList>(LForeach_deinit(), c);
        ListForEachReverse<AxesList>(LForeach_deinit(), c);
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
    
    template <int AxisIndex>
    using GetAxisTimer = typename Axis<AxisIndex>::TheAxisDriver::GetTimer;
    
    template <int LaserIndex>
    using GetLaserDriver = typename ThePlanner::template Laser<LaserIndex>::TheLaserDriver;
    
    static void emergency ()
    {
        ListForEachForward<AxesList>(LForeach_emergency());
        ListForEachForward<LasersList>(LForeach_emergency());
        ListForEachForward<ModulesList>(LForeach_emergency());
    }
    
    static void finish_locked (Context c)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->locked)
        
        TheCommand *cmd = get_locked(c);
        cmd->finishCommand(c);
    }
    
    static TheCommand * get_locked (Context c)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->locked)
        
        return get_command_in_state(c, COMMAND_LOCKED, true);
    }
    
    static TheCommand * get_msg_output (Context c)
    {
        using SerialModule = GetServiceProviderModule<ServiceList::SerialService>;
        return SerialModule::get_serial_stream(c);
    }
    
    static void print_pgm_string (Context c, AMBRO_PGM_P msg)
    {
        auto *output = get_msg_output(c);
        output->reply_append_pstr(c, msg);
        output->reply_poke(c);
    }
    
private:
    static void blinker_handler (Context c)
    {
        auto *ob = Object::self(c);
        TheDebugObject::access(c);
        
        ListForEachForward<ModulesList>(LForeach_check_safety(), c);
        TheWatchdog::reset(c);
    }
    
    static void work_command (Context c, TheCommand *cmd)
    {
        auto *ob = Object::self(c);
        
        switch (cmd->getCmdCode(c)) {
            case 'M': switch (cmd->getCmdNumber(c)) {
                default:
                    if (
                        TheConfigManager::checkCommand(c, cmd) &&
                        ListForEachForwardInterruptible<ModulesList>(LForeach_check_command(), c, cmd)
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
                
                case 114: {
                    ListForEachForward<PhysVirtAxisHelperList>(LForeach_append_position(), c, cmd);
                    cmd->reply_append_ch(c, '\n');
                    return cmd->finishCommand(c);
                } break;
                
                case 119: {
                    cmd->reply_append_pstr(c, AMBRO_PSTR("endstops:"));
                    ListForEachForward<PhysVirtAxisHelperList>(LForeach_m119_append_endstop(), c, cmd);
                    ListForEachForward<ModulesList>(LForeach_m119_append_endstop(), c, cmd);
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
                
                case 918: { // test assertions
                    uint32_t magic = cmd->get_command_param_uint32(c, 'M', 0);
                    if (magic != UINT32_C(122345)) {
                        cmd->reportError(c, AMBRO_PSTR("BadMagic"));
                    } else {
                        if (cmd->find_command_param(c, 'F', nullptr)) {
                            AMBRO_ASSERT_FORCE(0)
                        } else {
                            AMBRO_ASSERT(0)
                        }
                    }
                    cmd->finishCommand(c);
                } break;
                
                case 920: { // get underrun count
                    cmd->reply_append_uint32(c, ob->underrun_count);
                    cmd->reply_append_ch(c, '\n');
                    cmd->finishCommand(c);
                } break;
                
                case 930: { // apply configuration
                    if (!cmd->tryUnplannedCommand(c)) {
                        return;
                    }
                    TheConfigCache::update(c);
                    ListForEachForward<ModulesList>(LForeach_configuration_changed(), c);
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
                    return move_end(c, ob->time_freq_by_max_speed, PrinterMain::normal_move_end_callback, is_positioning_move);
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
                    // We use tryPlannedCommand to make sure that the TransformFeature
                    // is not splitting while we adjust the positions.
                    if (!cmd->tryPlannedCommand(c)) {
                        return;
                    }
                    set_position_begin(c);
                    auto num_parts = cmd->getNumParts(c);
                    for (decltype(num_parts) i = 0; i < num_parts; i++) {
                        ListForEachForward<PhysVirtAxisHelperList>(LForeach_g92_check_axis(), c, cmd, cmd->getPart(c, i));
                    }
                    set_position_end(c);
                    return cmd->finishCommand(c);
                } break;
            } break;
            
            unknown_command:
            default: {
                cmd->reportError(c, nullptr);
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
    
    static void normal_move_end_callback (Context c, bool error)
    {
        // TBD handle error
    }
    
public:
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
    
private:
    static void unlocked_timer_handler (Context c)
    {
        auto *ob = Object::self(c);
        TheDebugObject::access(c);
        
        if (!ob->locked) {
            TheCommand *cmd = get_command_in_state(c, COMMAND_LOCKING, false);
            if (cmd) {
                work_command(c, cmd);
            }
        }
    }
    
    static void disable_timer_handler (Context c)
    {
        auto *ob = Object::self(c);
        TheDebugObject::access(c);
        
        ListForEachForward<AxesList>(LForeach_enable_disable_stepper(), c, false);
    }
    
    static void force_timer_handler (Context c)
    {
        auto *ob = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(ob->planner_state == PLANNER_RUNNING)
        AMBRO_ASSERT(ob->m_planning_pull_pending)
        
        ThePlanner::waitFinished(c);
    }
    
    static void planner_pull_handler (Context c)
    {
        auto *ob = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(ob->planner_state != PLANNER_NONE)
        AMBRO_ASSERT(!ob->m_planning_pull_pending)
        
        ob->m_planning_pull_pending = true;
        if (TransformFeature::is_splitting(c)) {
            TransformFeature::do_split(c);
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
    
    static void planner_finished_handler (Context c)
    {
        auto *ob = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(ob->planner_state != PLANNER_NONE)
        AMBRO_ASSERT(ob->m_planning_pull_pending)
        AMBRO_ASSERT(ob->planner_state != PLANNER_WAITING)
        
        if (ob->planner_state == PLANNER_CUSTOM) {
            ob->custom_planner_deinit_allowed = true;
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
        TransformFeature::handle_aborted(c);
        ob->custom_planner_deinit_allowed = true;
        
        return ob->planner_client->aborted_handler(c);
    }
    
    static void planner_underrun_callback (Context c)
    {
        auto *ob = Object::self(c);
        ob->underrun_count++;
        
#ifdef AXISDRIVER_DETECT_OVERLOAD
        auto *output = get_msg_output(c);
        if (ThePlanner::axisOverloadOccurred(c)) {
            output->reply_append_pstr(c, AMBRO_PSTR("//AxisOverload\n"));
        } else {
            output->reply_append_pstr(c, AMBRO_PSTR("//NoOverload\n"));
        }
        output->reply_poke(c);
#endif
    }
    
    template <int AxisIndex>
    static bool planner_prestep_callback (typename ThePlanner::template Axis<AxisIndex>::StepperCommandCallbackContext c)
    {
        return TransformFeature::prestep_callback(c) ||
               !ListForEachForwardInterruptible<ModulesList>(LForeach_prestep_callback(), c);
    }
    
public:
    static void move_begin (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(!TransformFeature::is_splitting(c))
        
        o->move_seen_cartesian = false;
        o->custom_planner_deinit_allowed = false;
        ListForEachForward<PhysVirtAxisHelperList>(LForeach_save_pos_to_old(), c);
        ListForEachForward<LasersList>(LForeach_prepare_laser_for_move(), c);
    }
    
    template <int PhysVirtAxisIndex>
    static void move_add_axis (Context c, FpType value, bool ignore_limits=false)
    {
        PhysVirtAxisHelper<PhysVirtAxisIndex>::update_new_pos(c, value, ignore_limits);
    }
    
    template <int LaserIndex>
    static void move_add_laser (Context c, FpType energy)
    {
        auto *laser = Laser<LaserIndex>::Object::self(c);
        laser->move_energy = FloatMakePosOrPosZero(energy);
        laser->move_energy_specified = true;
    }
    
    static void move_end (Context c, FpType time_freq_by_max_speed, MoveEndCallback callback, bool is_positioning_move=true)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->planner_state == PLANNER_RUNNING || ob->planner_state == PLANNER_CUSTOM)
        AMBRO_ASSERT(ob->m_planning_pull_pending)
        AMBRO_ASSERT(FloatIsPosOrPosZero(time_freq_by_max_speed))
        AMBRO_ASSERT(callback)
        
        if (TransformFeature::is_splitting(c)) {
            TransformFeature::handle_virt_move(c, time_freq_by_max_speed, callback, is_positioning_move);
            return;
        }
        
        PlannerSplitBuffer *cmd = ThePlanner::getBuffer(c);
        FpType distance_squared = 0.0f;
        FpType total_steps = 0.0f;
        ListForEachForward<AxesList>(LForeach_do_move(), c, true, &distance_squared, &total_steps, cmd);
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
        return callback(c, false);
    }
    
    static void set_position_begin (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->locked)
        AMBRO_ASSERT(!TransformFeature::is_splitting(c))
        
        ListForEachForward<PhysVirtAxisHelperList>(LForeach_save_pos_to_old(), c);
    }
    
    template <int PhysVirtAxisIndex>
    static void set_position_add_axis (Context c, FpType value)
    {
        PhysVirtAxisHelper<PhysVirtAxisIndex>::update_new_pos(c, value, false);
    }
    
    static void set_position_end (Context c)
    {
        if (!TransformFeature::handle_set_position(c)) {
            ListForEachForward<PhysVirtAxisHelperList>(LForeach_restore_pos_from_old(), c);
            return;
        }
        ListForEachForward<AxesList>(LForeach_forward_update_pos(), c);
    }
    
private:
    struct LaserExtraSrc {
        Context m_c;
        template <int LaserIndex>
        FpType get () { return Laser<LaserIndex>::Object::self(m_c)->move_energy; }
    };
    
public:
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
        ob->custom_planner_deinit_allowed = true;
        now_active(c);
    }
    
    static void custom_planner_deinit (Context c)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->locked)
        AMBRO_ASSERT(ob->planner_state == PLANNER_CUSTOM)
        AMBRO_ASSERT(ob->custom_planner_deinit_allowed)
        
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
    
private:
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
        auto *output = get_msg_output(c);
        output->reply_append_pstr(c, msg);
        output->reply_poke(c);
    }
    
    static TheCommand * get_command_in_state (Context c, int state, bool must)
    {
        auto *ob = Object::self(c);
        
        for (CommandStream *stream = ob->command_stream_list.first(); stream; stream = ob->command_stream_list.next(stream)) {
            if (stream->m_state == state) {
                return stream;
            }
        }
        AMBRO_ASSERT(!must)
        return nullptr;
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
    
    struct PlannerUnionHoming {
        struct Object : public ObjBase<PlannerUnionHoming, typename PlannerUnion::Object, MapTypeList<AxesList, GetMemberType_HomingState>> {};
    };
    
    struct ConfigManagerHandler : public AMBRO_WFUNC_TD(&PrinterMain::config_manager_handler) {};
    struct BlinkerHandler : public AMBRO_WFUNC_TD(&PrinterMain::blinker_handler) {};
    struct PlannerPullHandler : public AMBRO_WFUNC_TD(&PrinterMain::planner_pull_handler) {};
    struct PlannerFinishedHandler : public AMBRO_WFUNC_TD(&PrinterMain::planner_finished_handler) {};
    struct PlannerAbortedHandler : public AMBRO_WFUNC_TD(&PrinterMain::planner_aborted_handler) {};
    struct PlannerUnderrunCallback : public AMBRO_WFUNC_TD(&PrinterMain::planner_underrun_callback) {};
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
                    ModulesList,
                    MakeTypeList<
                        TheSteppers,
                        TransformFeature,
                        PlannerUnion
                    >
                >,
                MemberType_ConfigExprs
            >
        >;
    };
    
public:
    using EventLoopFastEvents = ObjCollect<MakeTypeList<PrinterMain>, MemberType_EventLoopFastEvents, true>;
    
    struct Object : public ObjBase<PrinterMain, ParentObject, JoinTypeLists<
        AxesList,
        LasersList,
        ModulesList,
        MakeTypeList<
            TheDebugObject,
            TheWatchdog,
            TheConfigManager,
            TheConfigCache,
            TheBlinker,
            TheSteppers,
            TransformFeature,
            PlannerUnion
        >
    >> {
        typename Context::EventLoop::QueuedEvent unlocked_timer;
        typename Context::EventLoop::TimedEvent disable_timer;
        typename Context::EventLoop::TimedEvent force_timer;
        FpType time_freq_by_max_speed;
        uint32_t underrun_count;
        uint8_t locked : 1;
        uint8_t planner_state : 3;
        uint8_t m_planning_pull_pending : 1;
        uint8_t move_seen_cartesian : 1;
        uint8_t custom_planner_deinit_allowed : 1;
        PlannerClient *planner_client;
        PhysVirtAxisMaskType axis_homing;
        PhysVirtAxisMaskType axis_relative;
        PhysVirtAxisMaskType m_homing_rem_axes;
        DoubleEndedList<CommandStream, &CommandStream::m_list_node, false> command_stream_list;
    };
};

#include <aprinter/EndNamespace.h>

#endif
