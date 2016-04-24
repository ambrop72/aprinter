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
#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/Union.h>
#include <aprinter/meta/UnionGet.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/meta/PowerOfTwo.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/Expr.h>
#include <aprinter/meta/CallIfExists.h>
#include <aprinter/meta/MemberType.h>
#include <aprinter/meta/ListCollect.h>
#include <aprinter/meta/TypeDict.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Hints.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/OneOf.h>
#include <aprinter/base/LoopUtils.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/printer/utils/Blinker.h>
#include <aprinter/printer/actuators/Steppers.h>
#include <aprinter/printer/actuators/StepperGroup.h>
#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/printer/planning/MotionPlanner.h>
#include <aprinter/printer/Configuration.h>
#include <aprinter/printer/ServiceList.h>
#include <aprinter/printer/utils/GcodeCommand.h>
#include <aprinter/printer/OutputStream.h>
#include <aprinter/printer/HookExecutor.h>
#include <aprinter/printer/utils/JsonBuilder.h>
#include <aprinter/printer/utils/ModuleUtils.h>

#include <aprinter/BeginNamespace.h>

APRINTER_ALIAS_STRUCT(PrinterMainParams, (
    APRINTER_AS_TYPE(LedPin),
    APRINTER_AS_TYPE(LedBlinkInterval),
    APRINTER_AS_TYPE(InactiveTime),
    APRINTER_AS_VALUE(size_t, ExpectedResponseLength),
    APRINTER_AS_VALUE(size_t, ExtraSendBufClearance),
    APRINTER_AS_VALUE(size_t, MaxMsgSize),
    APRINTER_AS_TYPE(SpeedLimitMultiply),
    APRINTER_AS_TYPE(MaxStepsPerCycle),
    APRINTER_AS_VALUE(int, StepperSegmentBufferSize),
    APRINTER_AS_VALUE(int, LookaheadBufferSize),
    APRINTER_AS_VALUE(int, LookaheadCommitCount),
    APRINTER_AS_TYPE(ForceTimeout),
    APRINTER_AS_TYPE(FpType),
    APRINTER_AS_TYPE(WatchdogService),
    APRINTER_AS_TYPE(ConfigManagerService),
    APRINTER_AS_TYPE(ConfigList),
    APRINTER_AS_TYPE(AxesList),
    APRINTER_AS_TYPE(TransformParams),
    APRINTER_AS_TYPE(LasersList),
    APRINTER_AS_TYPE(ModulesList)
))

APRINTER_ALIAS_STRUCT(PrinterMainAxisParams, (
    APRINTER_AS_VALUE(char, Name),
    APRINTER_AS_TYPE(DefaultStepsPerUnit),
    APRINTER_AS_TYPE(DefaultMin),
    APRINTER_AS_TYPE(DefaultMax),
    APRINTER_AS_TYPE(DefaultMaxSpeed),
    APRINTER_AS_TYPE(DefaultMaxAccel),
    APRINTER_AS_TYPE(DefaultDistanceFactor),
    APRINTER_AS_TYPE(DefaultCorneringDistance),
    APRINTER_AS_TYPE(Homing),
    APRINTER_AS_VALUE(bool, IsCartesian),
    APRINTER_AS_VALUE(bool, IsExtruder),
    APRINTER_AS_VALUE(int, StepBits),
    APRINTER_AS_TYPE(TheAxisDriverService),
    APRINTER_AS_TYPE(SlaveSteppersList)
))

APRINTER_ALIAS_STRUCT(PrinterMainSlaveStepperParams, (
    APRINTER_AS_TYPE(TheStepperDef)
))

struct PrinterMainNoHomingParams {
    static bool const Enabled = false;
};

APRINTER_ALIAS_STRUCT_EXT(PrinterMainHomingParams, (
    APRINTER_AS_TYPE(HomeDir),
    APRINTER_AS_TYPE(HomeOffset),
    APRINTER_AS_TYPE(HomerService)
), (
    static bool const Enabled = true;
))

struct PrinterMainNoTransformParams {
    static const bool Enabled = false;
};

APRINTER_ALIAS_STRUCT_EXT(PrinterMainTransformParams, (
    APRINTER_AS_TYPE(VirtAxesList),
    APRINTER_AS_TYPE(PhysAxesList),
    APRINTER_AS_TYPE(TransformService),
    APRINTER_AS_TYPE(SplitterService)
), (
    static bool const Enabled = true;
))

APRINTER_ALIAS_STRUCT(PrinterMainVirtualAxisParams, (
    APRINTER_AS_VALUE(char, Name),
    APRINTER_AS_TYPE(MinPos),
    APRINTER_AS_TYPE(MaxPos),
    APRINTER_AS_TYPE(MaxSpeed)
))

APRINTER_ALIAS_STRUCT(PrinterMainLaserParams, (
    APRINTER_AS_VALUE(char, Name),
    APRINTER_AS_VALUE(char, DensityName),
    APRINTER_AS_TYPE(LaserPower),
    APRINTER_AS_TYPE(MaxPower),
    APRINTER_AS_TYPE(PwmService),
    APRINTER_AS_TYPE(DutyFormulaService),
    APRINTER_AS_TYPE(TheLaserDriverService)
))

template <typename MainArg>
class PrinterMain {
    using Context      = typename MainArg::Context;
    using ParentObject = typename MainArg::ParentObject;
    using Params       = typename MainArg::Params;
    
public:
    struct Object;
    
public:
    using ReservedAxisNames = MakeTypeList<WrapInt<'F'>, WrapInt<'T'>, WrapInt<'P'>, WrapInt<'S'>, WrapInt<'R'>>;
    
    static constexpr double SpeedRatioMin() { return 0.1; }
    static constexpr double SpeedRatioMax() { return 10.0; }
    
private:
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_WrappedAxisName, WrappedAxisName)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_WrappedPhysAxisIndex, WrappedPhysAxisIndex)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_HomingState, HomingState)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_TheModule, TheModule)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_TheStepper, TheStepper)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_TheStepperDef, TheStepperDef)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_PlannerAxisSpec, PlannerAxisSpec)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_PlannerLaserSpec, PlannerLaserSpec)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_HookType, HookType)
    APRINTER_DEFINE_MEMBER_TYPE(MemberType_ConfigExprs, ConfigExprs)
    APRINTER_DEFINE_MEMBER_TYPE(MemberType_ProvidedServices, ProvidedServices)
    APRINTER_DEFINE_MEMBER_TYPE(MemberType_MotionPlannerChannels, MotionPlannerChannels)
    APRINTER_DEFINE_MEMBER_TYPE(MemberType_CorrectionFeature, CorrectionFeature)
    APRINTER_DEFINE_MEMBER_TYPE(MemberType_HookDefinitionList, HookDefinitionList)
    APRINTER_DEFINE_CALL_IF_EXISTS(CallIfExists_init, init)
    APRINTER_DEFINE_CALL_IF_EXISTS(CallIfExists_deinit, deinit)
    APRINTER_DEFINE_CALL_IF_EXISTS(CallIfExists_check_command, check_command)
    APRINTER_DEFINE_CALL_IF_EXISTS(CallIfExists_check_g_command, check_g_command)
    APRINTER_DEFINE_CALL_IF_EXISTS(CallIfExists_configuration_changed, configuration_changed)
    APRINTER_DEFINE_CALL_IF_EXISTS(CallIfExists_emergency, emergency)
    APRINTER_DEFINE_CALL_IF_EXISTS(CallIfExists_check_safety, check_safety)
    APRINTER_DEFINE_CALL_IF_EXISTS(CallIfExists_m119_append_endstop, m119_append_endstop)
    APRINTER_DEFINE_CALL_IF_EXISTS(CallIfExists_prestep_callback, prestep_callback)
    APRINTER_DEFINE_CALL_IF_EXISTS(CallIfExists_check_move_interlocks, check_move_interlocks)
    APRINTER_DEFINE_CALL_IF_EXISTS(CallIfExists_planner_underrun, planner_underrun)
    APRINTER_DEFINE_CALL_IF_EXISTS(CallIfExists_get_json_status, get_json_status)
    
    struct PlannerUnion;
    struct PlannerUnionPlanner;
    struct PlannerUnionHoming;
    struct ConfigManagerHandler;
    struct BlinkerHandler;
    struct PlannerPullHandler;
    struct PlannerFinishedHandler;
    struct PlannerAbortedHandler;
    struct PlannerUnderrunCallback;
    struct DelayedConfigExprs;
    
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using ParamsAxesList = typename Params::AxesList;
    using ParamsLasersList = typename Params::LasersList;
    using TransformParams = typename Params::TransformParams;
    using ParamsModulesList = typename Params::ModulesList;
    
    using TheDebugObject = DebugObject<Context, Object>;
    using TheWatchdog = typename Params::WatchdogService::template Watchdog<Context, Object>;
    APRINTER_MAKE_INSTANCE(TheConfigCache, (ConfigCacheArg<Context, Object, DelayedConfigExprs>))
    APRINTER_MAKE_INSTANCE(TheBlinker, (BlinkerArg<Context, Object, typename Params::LedPin, BlinkerHandler>))
    
public:
    APRINTER_MAKE_INSTANCE(TheConfigManager, (Params::ConfigManagerService::template ConfigManager<Context, Object, typename Params::ConfigList, PrinterMain, ConfigManagerHandler>))
    
public:
    using FpType = typename Params::FpType;
    using Config = ConfigFramework<TheConfigManager, TheConfigCache>;
    static const int NumAxes = TypeListLength<ParamsAxesList>::Value;
    static const bool IsTransformEnabled = TransformParams::Enabled;
    
private:
    template <typename TheAxisSpec>
    using StepperDefsForAxis = MapTypeList<typename TheAxisSpec::SlaveSteppersList, GetMemberType_TheStepperDef>;
    
    using StepperDefsByAxis = MapTypeList<ParamsAxesList, TemplateFunc<StepperDefsForAxis>>;
    
    APRINTER_MAKE_INSTANCE(TheSteppers, (SteppersArg<Context, Object, Config, JoinTypeListList<StepperDefsByAxis>>))
    
    template <int AxisIndex, int AxisStepperIndex>
    using GetStepper = typename TheSteppers::template Stepper<(GetJoinedListOffset<StepperDefsByAxis, AxisIndex>::Value + AxisStepperIndex)>;
    
private:
    static_assert(Params::LedBlinkInterval::value() < TheWatchdog::WatchdogTime / 2.0, "");
    
public:
    using TimeConversion = APRINTER_FP_CONST_EXPR(Clock::time_freq);
    
private:
    using MaxStepsPerCycle = decltype(Config::e(Params::MaxStepsPerCycle::i()));
    
    using CInactiveTimeTicks = decltype(ExprCast<TimeType>(Config::e(Params::InactiveTime::i()) * TimeConversion()));
    using CForceTimeoutTicks = decltype(ExprCast<TimeType>(Config::e(Params::ForceTimeout::i()) * TimeConversion()));
    
    using MyConfigExprs = MakeTypeList<CInactiveTimeTicks, CForceTimeoutTicks>;
    
    enum {COMMAND_IDLE, COMMAND_WAITBUF, COMMAND_WAITBUF_PAUSED, COMMAND_LOCKING, COMMAND_LOCKING_PAUSED, COMMAND_LOCKED};
    enum {PLANNER_NONE, PLANNER_RUNNING, PLANNER_STOPPING, PLANNER_WAITING, PLANNER_CUSTOM};
    
private:
    using ServicesList = ListCollect<ParamsModulesList, MemberType_ProvidedServices>;
    
    template <typename Entry>
    using GetServiceEntryServiceType = typename Entry::Value::ServiceType;
    
    using ServicesDictUnsorted = ListGroup<ServicesList, TemplateFunc<GetServiceEntryServiceType>>;
    
    template <typename Entry1, typename Entry2>
    using IsServicePriorityLesser = WrapBool<(Entry1::Value::Priority < Entry2::Value::Priority)>;
    
    template <typename ServiceGroup>
    using SortServiceGroup = TypeDictEntry<typename ServiceGroup::Key, ListSort<typename ServiceGroup::Value, IsServicePriorityLesser>>;
    
    using ServicesDict = MapTypeList<ServicesDictUnsorted, TemplateFunc<SortServiceGroup>>;
    
public:
    template <typename ServiceType>
    using GetServiceProviders = TypeDictGetOrDefault<ServicesDict, ServiceType, EmptyTypeList>;
    
    template <typename ServiceType>
    using HasServiceProvider = WrapBool<TypeDictFind<ServicesDict, ServiceType>::Found>;
    
    template <typename ServiceType>
    struct FindServiceProvider {
        using FindResult = TypeDictFind<ServicesDict, ServiceType>;
        static_assert(FindResult::Found, "The requested service type is not provided by any module.");
        static int const ModuleIndex = TypeListGet<typename FindResult::Result, 0>::Key::Value;
    };
    
public:
    static_assert(Params::ExpectedResponseLength >= 60, "");
    static_assert(Params::ExtraSendBufClearance >= 60, "");
    static_assert(Params::MaxMsgSize >= 50, "");
    static_assert(Params::MaxMsgSize <= Params::ExtraSendBufClearance, "");
    
    static size_t const ExpectedResponseLength = Params::ExpectedResponseLength;
    static size_t const ExtraSendBufClearance = Params::ExtraSendBufClearance;
    static size_t const CommandSendBufClearance = ExpectedResponseLength + ExtraSendBufClearance;
    static size_t const MaxMsgSize = Params::MaxMsgSize;
    
public:
    using TheOutputStream = OutputStream<Context, FpType>;
    
public:
    class CommandStreamCallback {
    public:
        virtual bool start_command_impl (Context c) { return true; }
        virtual void finish_command_impl (Context c) = 0;
        virtual void reply_poke_impl (Context c) = 0;
        virtual void reply_append_buffer_impl (Context c, char const *str, size_t length) = 0;
#if AMBRO_HAS_NONTRANSPARENT_PROGMEM
        virtual void reply_append_pbuffer_impl (Context c, AMBRO_PGM_P pstr, size_t length) = 0;
#endif
        virtual size_t get_send_buf_avail_impl (Context c) = 0;
    };
    
    class SendBufEventCallback {
    public:
        virtual bool request_send_buf_event_impl (Context c, size_t length) = 0;
        virtual void cancel_send_buf_event_impl (Context c) = 0;
    };
    
    class CommandStream : public TheOutputStream {
        friend PrinterMain;
        
    public:
        using TheGcodeCommand = GcodeCommand<Context, FpType>;
        
        void init (Context c, CommandStreamCallback *callback, SendBufEventCallback *buf_callback)
        {
            auto *mo = Object::self(c);
            AMBRO_ASSERT(callback)
            AMBRO_ASSERT(buf_callback)
            
            m_state = COMMAND_IDLE;
            m_poke_overhead = 0;
            m_callback = callback;
            m_buf_callback = buf_callback;
            m_cmd = nullptr;
            m_accept_msg = true;
            m_error = false;
            m_refuse_on_error = false;
            m_auto_ok_and_poke = true;
            m_send_buf_event_handler = nullptr;
            m_captured_command_handler = nullptr;
            mo->command_stream_list.prepend(this);
        }
        
        void deinit (Context c)
        {
            auto *mo = Object::self(c);
            
            if (m_captured_command_handler) {
                auto func = m_captured_command_handler;
                m_captured_command_handler = nullptr;
                func(c, nullptr);
            }
            
            mo->command_stream_list.remove(this);
        }
        
        void setAcceptMsg (Context c, bool accept_msg)
        {
            m_accept_msg = accept_msg;
        }
        
        void setAutoOkAndPoke (Context c, bool auto_ok_and_poke)
        {
            m_auto_ok_and_poke = auto_ok_and_poke;
        }
        
        void setPokeOverhead (Context c, uint8_t overhead_bytes)
        {
            m_poke_overhead = overhead_bytes;
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
            
            if (m_callback->get_send_buf_avail_impl(c) < CommandSendBufClearance) {
                if (m_buf_callback->request_send_buf_event_impl(c, CommandSendBufClearance)) {
                    m_state = COMMAND_WAITBUF;
                    return;
                }
                // Bad luck. Shouldn't happen anyway because the send buffer should
                // be at least as large as CommandSendBufClearance.
            }
            
            return process_command(c);
        }
        
        bool canCancelOrPause (Context c)
        {
            return (!m_cmd || (m_state != COMMAND_IDLE && m_state != COMMAND_LOCKED));
        }
        
        void maybePauseCommand (Context c)
        {
            AMBRO_ASSERT(canCancelOrPause(c))
            
            if (m_cmd) {
                if (m_state == COMMAND_WAITBUF) {
                    m_buf_callback->cancel_send_buf_event_impl(c);
                    m_state = COMMAND_WAITBUF_PAUSED;
                }
                else if (m_state == COMMAND_LOCKING) {
                    m_state = COMMAND_LOCKING_PAUSED;
                }
            }
        }
        
        bool maybeResumeCommand (Context c)
        {
            auto *mo = Object::self(c);
            AMBRO_ASSERT(!m_cmd || (m_state == COMMAND_WAITBUF_PAUSED || m_state == COMMAND_LOCKING_PAUSED))
            
            if (m_cmd) {
                if (m_state == COMMAND_WAITBUF_PAUSED) {
                    bool res = m_buf_callback->request_send_buf_event_impl(c, CommandSendBufClearance);
                    AMBRO_ASSERT(res)
                    m_state = COMMAND_WAITBUF;
                } else {
                    m_state = COMMAND_LOCKING;
                    if (!mo->unlocked_timer.isSet(c)) {
                        mo->unlocked_timer.prependNowNotAlready(c);
                    }
                }
            }
            
            return bool(m_cmd);
        }
        
        void maybeCancelCommand (Context c)
        {
            AMBRO_ASSERT(canCancelOrPause(c))
            
            if (m_cmd) {
                if (m_state == COMMAND_WAITBUF) {
                    m_buf_callback->cancel_send_buf_event_impl(c);
                }
                m_state = COMMAND_IDLE;
                m_cmd = nullptr;
            }
        }
        
        bool tryCancelCommand (Context c)
        {
            if (canCancelOrPause(c)) {
                maybeCancelCommand(c);
            }
            return !hasCommand(c);
        }
        
        void reportSendBufEventDirectly (Context c)
        {
            AMBRO_ASSERT(m_state == COMMAND_WAITBUF || m_state == COMMAND_LOCKED)
            AMBRO_ASSERT(m_state != COMMAND_WAITBUF || !m_send_buf_event_handler)
            AMBRO_ASSERT(m_state != COMMAND_LOCKED || m_send_buf_event_handler)
            
            if (m_state == COMMAND_WAITBUF) {
                m_state = COMMAND_IDLE;
                
                return process_command(c);
            } else {
                auto handler = m_send_buf_event_handler;
                m_send_buf_event_handler = nullptr;
                
                return handler(c);
            }
        }
        
        bool haveError (Context c)
        {
            return m_error;
        }
        
        void clearError (Context c)
        {
            m_error = false;
        }
        
        TheGcodeCommand * getGcodeCommand (Context c)
        {
            return m_cmd;
        }
        
        CommandStreamCallback * getCallback (Context c)
        {
            return m_callback;
        }
        
    public:
        using PartsSizeType = typename TheGcodeCommand::PartsSizeType;
        using PartRef = typename TheGcodeCommand::PartRef;
        using SendBufEventHandler = void (*) (Context);
        
        APRINTER_NO_INLINE
        void reportError (Context c, AMBRO_PGM_P errstr)
        {
            AMBRO_ASSERT(m_cmd)
            AMBRO_ASSERT(m_state == COMMAND_IDLE || m_state == COMMAND_LOCKED)
            
            m_error = true;
            if (errstr) {
                this->reply_append_error(c, errstr);
            }
        }
        
        APRINTER_NO_INLINE
        void finishCommand (Context c, bool no_ok=false)
        {
            auto *mo = Object::self(c);
            AMBRO_ASSERT(m_cmd)
            AMBRO_ASSERT(m_state == COMMAND_IDLE || m_state == COMMAND_LOCKED)
            AMBRO_ASSERT(!m_send_buf_event_handler)
            
            if (m_auto_ok_and_poke) {
                if (!no_ok) {
                    this->reply_append_pstr(c, AMBRO_PSTR("ok\n"));
                }
                m_callback->reply_poke_impl(c);
            }
            
            m_callback->finish_command_impl(c);
            m_cmd = nullptr;
            if (m_state == COMMAND_LOCKED) {
                AMBRO_ASSERT(mo->locked)
                m_state = COMMAND_IDLE;
                unlock(c);
            }
        }
        
        APRINTER_NO_INLINE
        bool tryLockedCommand (Context c)
        {
            auto *mo = Object::self(c);
            AMBRO_ASSERT(m_cmd)
            AMBRO_ASSERT(m_state == COMMAND_IDLE || m_state == COMMAND_LOCKING || m_state == COMMAND_LOCKED)
            AMBRO_ASSERT(m_state != COMMAND_LOCKING || !mo->locked)
            AMBRO_ASSERT(m_state != COMMAND_LOCKED || mo->locked)
            
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
        
        APRINTER_NO_INLINE
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
        
        APRINTER_NO_INLINE
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
        
    public:
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
        
    public:
        void reply_poke (Context c)
        {
            m_callback->reply_poke_impl(c);
        }
        
        void reply_append_buffer (Context c, char const *str, size_t length)
        {
            m_callback->reply_append_buffer_impl(c, str, length);
        }
        
#if AMBRO_HAS_NONTRANSPARENT_PROGMEM
        void reply_append_pbuffer (Context c, AMBRO_PGM_P pstr, size_t length)
        {
            m_callback->reply_append_pbuffer_impl(c, pstr, length);
        }
#endif
        
    public:
        APRINTER_NO_INLINE
        bool requestSendBufEvent (Context c, size_t length, SendBufEventHandler handler)
        {
            AMBRO_ASSERT(m_state == COMMAND_LOCKED)
            AMBRO_ASSERT(!m_send_buf_event_handler)
            AMBRO_ASSERT(length > 0)
            AMBRO_ASSERT(handler)
            
            if (!m_buf_callback->request_send_buf_event_impl(c, length + CommandSendBufClearance)) {
                return false;
            }
            m_send_buf_event_handler = handler;
            return true;
        }
        
        APRINTER_NO_INLINE
        void cancelSendBufEvent (Context c)
        {
            AMBRO_ASSERT(m_state == COMMAND_LOCKED)
            AMBRO_ASSERT(m_send_buf_event_handler)
            
            m_buf_callback->cancel_send_buf_event_impl(c);
            m_send_buf_event_handler = nullptr;
        }
        
    public:
        APRINTER_NO_INLINE
        bool find_command_param (Context c, char code, PartRef *out_part)
        {
            for (auto i : LoopRangeAuto(getNumParts(c))) {
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
        
        APRINTER_NO_INLINE
        uint32_t get_command_param_uint32 (Context c, char code, uint32_t default_value)
        {
            PartRef part;
            if (!find_command_param(c, code, &part)) {
                return default_value;
            }
            return getPartUint32Value(c, part);
        }
        
        APRINTER_NO_INLINE
        FpType get_command_param_fp (Context c, char code, FpType default_value)
        {
            PartRef part;
            if (!find_command_param(c, code, &part)) {
                return default_value;
            }
            return getPartFpValue(c, part);
        }
        
        APRINTER_NO_INLINE
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
        
        APRINTER_NO_INLINE
        bool find_command_param_fp (Context c, char code, FpType *out)
        {
            PartRef part;
            if (!find_command_param(c, code, &part)) {
                return false;
            }
            *out = getPartFpValue(c, part);
            return true;
        }
        
    public:
        // NOTE: Null cmd means the stream is gone, do not stopCapture.
        // In this case this is an in-line callback from deinit(), so no funny business there.
        using CapturedCommandHandler = void (*) (Context c, CommandStream *cmd);
        
        void startCapture (Context c, CapturedCommandHandler captured_command_handler)
        {
            AMBRO_ASSERT(!m_captured_command_handler)
            AMBRO_ASSERT(captured_command_handler)
            
            m_captured_command_handler = captured_command_handler;
        }
        
        // NOTE: It is advised to only stopCapture as part of handling a captured command.
        void stopCapture (Context c)
        {
            AMBRO_ASSERT(m_captured_command_handler)
            
            m_captured_command_handler = nullptr;
        }
        
    private:
        void process_command (Context c)
        {
            PartsSizeType num_parts = m_cmd->getNumParts(c);
            if (num_parts < 0) {
                if (num_parts == GCODE_ERROR_NO_PARTS) {
                    return finishCommand(c, true);
                }
                
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
            
            return work_command(c, this);
        }
        
    private:
        uint8_t m_state : 4;
        bool m_accept_msg : 1;
        bool m_error : 1;
        bool m_refuse_on_error : 1;
        bool m_auto_ok_and_poke : 1;
        uint8_t m_poke_overhead;
        CommandStreamCallback *m_callback;
        SendBufEventCallback *m_buf_callback;
        TheGcodeCommand *m_cmd;
        SendBufEventHandler m_send_buf_event_handler;
        CapturedCommandHandler m_captured_command_handler;
        DoubleEndedListNode<CommandStream> m_list_node;
    };
    
public:
    using TheCommand = CommandStream;
    using CommandPartRef = typename TheCommand::PartRef;
    
private:
    class MsgOutputStream : public TheOutputStream {
        friend PrinterMain;
        
    public:
        template <typename... Args>
        void print (Context c, char const *fmt, Args... args)
        {
            auto *o = Object::self(c);
            
            if (o->msg_length < MaxMsgSize) {
                snprintf(o->msg_buffer + o->msg_length, MaxMsgSize - o->msg_length, fmt, args...);
                o->msg_length += strlen(o->msg_buffer + o->msg_length);
                AMBRO_ASSERT(o->msg_length < MaxMsgSize)
            }
        }
        
        template <typename... Args>
        void println (Context c, char const *fmt, Args... args)
        {
            print(c, fmt, args...);
            reply_poke(c);
        }
        
        void poke_with_inhibit (Context c, CommandStream *inhibit_stream)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->msg_length <= MaxMsgSize)
            
            if (o->msg_length == 0) {
                return;
            }
            
            if (o->msg_buffer[o->msg_length - 1] != '\n') {
                if (o->msg_length < MaxMsgSize) {
                    o->msg_buffer[o->msg_length] = '\n';
                    o->msg_length++;
                } else {
                    o->msg_buffer[MaxMsgSize - 1] = '\n';
                }
            }
            
            for (CommandStream *stream = o->command_stream_list.first(); stream; stream = o->command_stream_list.next(stream)) {
                if (stream != inhibit_stream && stream->m_accept_msg) {
                    // Ensure that we only write a message to the stream if it has space to accept it
                    // entirely, and additionally, if a command is active on the stream, that we do not
                    // use up the reserved buffer space for its response. Take into account also any
                    // overhead of performing a poke.
                    size_t need_space = o->msg_length;
                    if (stream->m_cmd && (stream->m_state != COMMAND_WAITBUF && stream->m_state != COMMAND_WAITBUF_PAUSED)) {
                        need_space += ExpectedResponseLength + stream->m_poke_overhead;
                    }
                    if (stream->m_callback->get_send_buf_avail_impl(c) >= need_space) {
                        stream->reply_append_buffer(c, o->msg_buffer, o->msg_length);
                        stream->reply_poke(c);
                    }
                }
            }
            
            o->msg_length = 0;
        }
        
    private:
        void reply_poke (Context c)
        {
            poke_with_inhibit(c, nullptr);
        }
        
        void reply_append_buffer (Context c, char const *str, size_t length)
        {
            auto *o = Object::self(c);
            
            size_t write_length = MinValue(length, (size_t)(MaxMsgSize - o->msg_length));
            memcpy(o->msg_buffer + o->msg_length, str, write_length);
            o->msg_length += write_length;
        }
        
#if AMBRO_HAS_NONTRANSPARENT_PROGMEM
        void reply_append_pbuffer (Context c, AMBRO_PGM_P pstr, size_t length)
        {
            auto *o = Object::self(c);
            
            size_t write_length = MinValue(length, (size_t)(MaxMsgSize - o->msg_length));
            AMBRO_PGM_MEMCPY(o->msg_buffer + o->msg_length, pstr, write_length);
            o->msg_length += write_length;
        }
#endif
    };
    
public:
    using MoveEndCallback = void(*)(Context c, bool error);
    
    struct GenericHookDispatcher;
    
private:
    template <int ModuleIndex>
    struct Module {
        struct Object;
        using ModuleSpec = TypeListGet<ParamsModulesList, ModuleIndex>;
        struct Arg : public ModuleTemplateArg<Context, Object, PrinterMain, ModuleSpec> {};
        using TheModule = typename ModuleSpec::template Module<Arg>;
        
        static void init (Context c)
        {
            CallIfExists_init::template call_void<TheModule>(c);
        }
        
        static void deinit (Context c)
        {
            CallIfExists_deinit::template call_void<TheModule>(c);
        }
        
        static bool check_command (Context c, TheCommand *cmd)
        {
            return CallIfExists_check_command::template call_ret<TheModule, bool, true>(c, cmd);
        }
        
        static bool check_g_command (Context c, TheCommand *cmd)
        {
            return CallIfExists_check_g_command::template call_ret<TheModule, bool, true>(c, cmd);
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
        AMBRO_ALWAYS_INLINE
        static bool prestep_callback (CallbackContext c)
        {
            return !CallIfExists_prestep_callback::template call_ret<TheModule, bool, false>(c);
        }
        
        template <typename ThePhysVirtAxisMaskType>
        static bool check_move_interlocks (Context c, TheOutputStream *err_output, ThePhysVirtAxisMaskType move_axes)
        {
            return CallIfExists_check_move_interlocks::template call_ret<TheModule, bool, true>(c, err_output, move_axes);
        }
        
        static void planner_underrun (Context c)
        {
            CallIfExists_planner_underrun::template call_void<TheModule>(c);
        }
        
        template <typename TheJsonBuilder>
        static void get_json_status (Context c, TheJsonBuilder *json)
        {
            CallIfExists_get_json_status::template call_void<TheModule>(c, json);
        }
        
        struct Object : public ObjBase<Module, typename PrinterMain::Object, MakeTypeList<
            TheModule
        >> {};
    };
    using ModulesList = IndexElemList<ParamsModulesList, Module>;
    
public:
    template <int ModuleIndex>
    using GetModule = typename Module<ModuleIndex>::TheModule;
    
    template <typename Provider>
    using GetProviderModule = GetModule<Provider::Key::Value>;
    
    template <typename ServiceType>
    using GetServiceProviderModule = GetModule<FindServiceProvider<ServiceType>::ModuleIndex>;
    
    template <typename This=PrinterMain>
    using GetFsAccess = typename This::template GetServiceProviderModule<ServiceList::FsAccessService>::template GetFsAccess<>;
    
    using ModuleClassesList = MapTypeList<ModulesList, GetMemberType_TheModule>;
    
private:
    template <typename DefaultService, typename ServiceProviderId, typename ServiceProviderMember>
    using GetServiceFromModuleOrDefault = FuncCall<
        IfFunc<
            TemplateFunc<HasServiceProvider>,
            ComposeFunctions<typename ServiceProviderMember::Get, TemplateFunc<GetServiceProviderModule>>,
            ConstantFunc<DefaultService>
        >,
        ServiceProviderId
    >;
    
private:
    template <int TAxisIndex>
    struct Axis {
        struct Object;
        
        static const int AxisIndex = TAxisIndex;
        using AxisSpec = TypeListGet<ParamsAxesList, AxisIndex>;
        using SlaveSteppersList = typename AxisSpec::SlaveSteppersList;
        static const char AxisName = AxisSpec::Name;
        static_assert(NameCharIsValid<AxisName, ReservedAxisNames>::Value, "Axis name not allowed");
        using WrappedAxisName = WrapInt<AxisName>;
        using HomingSpec = typename AxisSpec::Homing;
        static bool const IsExtruder = AxisSpec::IsExtruder;
        
        struct LazySteppersList;
        APRINTER_MAKE_INSTANCE(TheStepperGroup, (StepperGroupArg<Context, LazySteppersList>))
        
        template <typename ThePrinterMain=PrinterMain> struct DelayedAxisDriverConsumersList;
        APRINTER_MAKE_INSTANCE(TheAxisDriver, (AxisSpec::TheAxisDriverService::template Driver<Context, Object, TheStepperGroup, DelayedAxisDriverConsumersList<>>))
        
        using StepFixedType = FixedPoint<AxisSpec::StepBits, false, 0>;
        using AbsStepFixedType = FixedPoint<AxisSpec::StepBits - 1, true, 0>;
        
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
        
        struct PlannerPrestepCallback;
        struct PlannerAxisSpec : public MotionPlannerAxisSpec<
            TheAxisDriver,
            AxisSpec::StepBits,
            decltype(Config::e(AxisSpec::DefaultDistanceFactor::i())),
            decltype(Config::e(AxisSpec::DefaultCorneringDistance::i())),
            PlannerMaxSpeedRec,
            PlannerMaxAccelRec,
            PlannerPrestepCallback
        > {};
        
        AMBRO_STRUCT_IF(HomingFeature, HomingSpec::Enabled) {
            struct Object;
            
            using HomerGeneral = typename HomingSpec::HomerService::template HomerGeneral<
                Context, PrinterMain, AxisSpec::StepBits, Params::StepperSegmentBufferSize,
                Params::LookaheadBufferSize, MaxStepsPerCycle, decltype(Config::e(AxisSpec::DefaultMaxAccel::i())),
                DistConversion, TimeConversion, decltype(Config::e(HomingSpec::HomeDir::i()))
            >;
            
            APRINTER_MAKE_INSTANCE(HomerGlobal, (HomerGeneral::template HomerGlobal<Object>))
            
            struct HomingState {
                struct Object;
                struct HomerFinishedHandler;
                
                APRINTER_MAKE_INSTANCE(Homer, (HomerGeneral::template Homer<Object, HomerGlobal, TheAxisDriver, HomerFinishedHandler>))
                
                static void homer_finished_handler (Context c, bool success)
                {
                    auto *axis = Axis::Object::self(c);
                    auto *mob = PrinterMain::Object::self(c);
                    AMBRO_ASSERT(mob->axis_homing & AxisMask())
                    AMBRO_ASSERT(mob->locked)
                    
                    Homer::deinit(c);
                    axis->m_req_pos = APRINTER_CFG(Config, CInitPosition, c);
                    forward_update_pos(c);
                    
                    mob->axis_homing &= ~AxisMask();
                    TransformFeature::template mark_phys_moved<AxisIndex>(c);
                    if (!success) {
                        mob->homing_error = true;
                    }
                    
                    if (mob->axis_homing == 0) {
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
                
                if ((mob->homing_req_axes & AxisMask())) {
                    TheStepperGroup::enable(c);
                    HomingState::Homer::init(c, get_locked(c));
                    mob->axis_homing |= AxisMask();
                }
            }
            
            using InitPosition = decltype(ExprIf(Config::e(HomingSpec::HomeDir::i()), MaxReqPos(), MinReqPos()) + Config::e(HomingSpec::HomeOffset::i()));
            
            static void m119_append_endstop (Context c, TheCommand *cmd)
            {
                bool triggered = HomerGlobal::endstop_is_triggered(c);
                cmd->reply_append_ch(c, ' ');
                cmd->reply_append_ch(c, AxisName);
                cmd->reply_append_ch(c, ':');
                cmd->reply_append_ch(c, (triggered ? '1' : '0'));
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
            static void m119_append_endstop (Context c, TheCommand *cmd) {}
            struct Object {};
        };
        
        using HomingState = typename HomingFeature::HomingState;
        
        template <int AxisStepperIndex>
        struct AxisStepper {
            struct Object;
            using StepperSpec = TypeListGet<SlaveSteppersList, AxisStepperIndex>;
            using TheStepper = GetStepper<AxisIndex, AxisStepperIndex>;
            
            static void init (Context c)
            {
            }
            
            struct Object : public ObjBase<AxisStepper, typename Axis::Object, EmptyTypeList> {};
        };
        using AxisSteppersList = IndexElemList<SlaveSteppersList, AxisStepper>;
        
        struct LazySteppersList {
            using List = MapTypeList<AxisSteppersList, GetMemberType_TheStepper>;
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
            ListFor<AxisSteppersList>([&] APRINTER_TL(stepper, stepper::init(c)));
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
                TheStepperGroup::enable(c);
            } else {
                TheStepperGroup::disable(c);
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
        static void do_move (Context c, bool add_distance, FpType *distance_squared, PlannerCmd *cmd)
        {
            auto *o = Object::self(c);
            
            AbsStepFixedType old_end_pos = o->m_end_pos;
            forward_update_pos(c);
            
            bool dir = (o->m_end_pos >= old_end_pos);
            StepFixedType move = StepFixedType::importBits(dir ? 
                ((typename StepFixedType::IntType)o->m_end_pos.bitsValue() - (typename StepFixedType::IntType)old_end_pos.bitsValue()) :
                ((typename StepFixedType::IntType)old_end_pos.bitsValue() - (typename StepFixedType::IntType)o->m_end_pos.bitsValue())
            );
            
            if (AMBRO_UNLIKELY(move.bitsValue() != 0)) {
                if (add_distance && AxisSpec::IsCartesian) {
                    FpType delta = move.template fpValue<FpType>() * APRINTER_CFG(Config, CDistConversionRec, c);
                    *distance_squared += delta * delta;
                }
                TheStepperGroup::enable(c);
            }
            
            auto *mycmd = TupleGetElem<AxisIndex>(cmd->axes.axes());
            mycmd->dir = dir;
            mycmd->x = move;
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
        
        static void emergency ()
        {
            TheStepperGroup::emergency();
        }
        
        static void m119_append_endstop (Context c, TheCommand *cmd)
        {
            HomingFeature::m119_append_endstop(c, cmd);
        }
        
        AMBRO_ALWAYS_INLINE
        static bool planner_prestep_callback (typename TheAxisDriver::CommandCallbackContext c)
        {
            return !ListForBreak<ModulesList>([&] APRINTER_TL(module, return module::prestep_callback(c)));
        }
        struct PlannerPrestepCallback : public AMBRO_WFUNC_TD(&Axis::planner_prestep_callback) {};
        
        template <typename ThePrinterMain>
        struct DelayedAxisDriverConsumersList {
            using List = JoinTypeLists<
                MakeTypeList<typename ThePrinterMain::ThePlanner::template TheAxisDriverConsumer<AxisIndex>>,
                typename HomingFeature::AxisDriverConsumersList
            >;
        };
        
        using CDistConversion = decltype(ExprCast<FpType>(DistConversion()));
        using CDistConversionRec = decltype(ExprCast<FpType>(ExprRec(DistConversion())));
        using CMinReqPos = decltype(ExprCast<FpType>(MinReqPos()));
        using CMaxReqPos = decltype(ExprCast<FpType>(MaxReqPos()));
        using CInitPosition = decltype(ExprCast<FpType>(HomingFeature::InitPosition::e()));
        
        using ConfigExprs = MakeTypeList<CDistConversion, CDistConversionRec, CMinReqPos, CMaxReqPos, CInitPosition>;
        
        struct Object : public ObjBase<Axis, typename PrinterMain::Object, JoinTypeLists<
            MakeTypeList<
                TheAxisDriver,
                HomingFeature
            >,
            AxisSteppersList
        >>
        {
            AbsStepFixedType m_end_pos;
            FpType m_req_pos;
            FpType m_old_pos;
        };
    };
    
    using AxesList = IndexElemList<ParamsAxesList, Axis>;
    
    template <char AxisName>
    using FindAxis = TypeListIndexMapped<AxesList, GetMemberType_WrappedAxisName, WrapInt<AxisName>>;
    
    template <int LaserIndex>
    struct Laser {
        struct Object;
        
        using LaserSpec = TypeListGet<ParamsLasersList, LaserIndex>;
        static_assert(NameCharIsValid<LaserSpec::Name, ReservedAxisNames>::Value, "Laser name not allowed");
        static_assert(NameCharIsValid<LaserSpec::DensityName, ReservedAxisNames>::Value, "Laser-density name not allowed");
        
        using ThePwm = typename LaserSpec::PwmService::template Pwm<Context, Object>;
        using TheDutyFormula = typename LaserSpec::DutyFormulaService::template DutyFormula<typename ThePwm::DutyCycleType, ThePwm::MaxDutyCycle>;
        
        using MaxPower = decltype(Config::e(LaserSpec::MaxPower::i()));
        using LaserPower = decltype(Config::e(LaserSpec::LaserPower::i()));
        using PlannerMaxSpeedRec = decltype(TimeConversion() / (MaxPower() / LaserPower()));
        
        struct PowerInterface;
        struct PlannerLaserSpec : public MotionPlannerLaserSpec<typename LaserSpec::TheLaserDriverService, PowerInterface, PlannerMaxSpeedRec> {};
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            o->density = 0.0f;
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
    
public:
    struct PlannerClient {
        virtual void pull_handler (Context c) = 0;
        virtual void finished_handler (Context c, bool aborted) = 0;
    };
    
public:
    AMBRO_STRUCT_IF(TransformFeature, TransformParams::Enabled) {
        friend PrinterMain;
        
    public:
        struct Object;
        
    private:
        using ParamsVirtAxesList = typename TransformParams::VirtAxesList;
        using ParamsPhysAxesList = typename TransformParams::PhysAxesList;
        APRINTER_MAKE_INSTANCE(TheTransformAlg, (TransformParams::TransformService::template Transform<Context, Object, Config, FpType>))
        APRINTER_MAKE_INSTANCE(TheSplitterClass, (TransformParams::SplitterService::template Splitter<Context, Object, Config, FpType>))
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
            ListFor<VirtAxesList>([&] APRINTER_TL(axis, axis::init(c)));
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
        
        static bool update_phys_from_virt (Context c)
        {
            bool success;
            if (TheCorrectionService::CorrectionEnabled) {
                FpType temp_virt_pos[NumVirtAxes];
                TheCorrectionService::do_correction(c, VirtReqPosSrc{c}, ArrayDst{temp_virt_pos}, WrapBool<false>());
                success = TheTransformAlg::virtToPhys(c, ArraySrc{temp_virt_pos}, PhysReqPosDst{c});
            } else {
                success = TheTransformAlg::virtToPhys(c, VirtReqPosSrc{c}, PhysReqPosDst{c});
            }
            if (success) {
                success = ListForBreak<VirtAxesList>([&] APRINTER_TL(axis, return axis::check_phys_limits(c)));
            }
            return success;
        }
        
        static void handle_virt_move (Context c, FpType time_freq_by_max_speed, TheCommand *move_err_output, MoveEndCallback callback, bool is_positioning_move)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->splitting)
            
            o->move_err_output = move_err_output;
            o->move_end_callback = callback;
            
            o->virt_update_pending = false;
            bool transform_success = update_phys_from_virt(c);
            
            if (!transform_success) {
                restore_all_pos_from_old(c);
                return handle_transform_error(c);
            }
            
            FpType distance_squared = 0.0f;
            ListFor<VirtAxesList>([&] APRINTER_TL(axis, axis::prepare_split(c, &distance_squared)));
            ListFor<SecondaryAxesList>([&] APRINTER_TL(axis, axis::prepare_split(c, &distance_squared)));
            FpType distance = FloatSqrt(distance_squared);
            
            ListFor<LasersList>([&] APRINTER_TL(laser, laser::handle_automatic_energy(c, distance, is_positioning_move)));
            ListFor<LaserSplitsList>([&] APRINTER_TL(split, split::prepare_split(c)));
            
            FpType distance_based_max_v_rec = distance * time_freq_by_max_speed;
            FpType base_max_v_rec = ListForFold<VirtAxesList>(distance_based_max_v_rec, [&] APRINTER_TLA(axis, (FpType accum), return axis::limit_virt_axis_speed(accum, c)));
            base_max_v_rec = FloatMax(base_max_v_rec, ThePlanner::getBuffer(c)->axes.rel_max_v_rec);
            if (AMBRO_UNLIKELY(base_max_v_rec != distance_based_max_v_rec)) {
                time_freq_by_max_speed = base_max_v_rec / distance;
            }
            
            o->splitter.start(c, distance, base_max_v_rec, time_freq_by_max_speed);
            o->frac = 0.0f;
            
            return do_split(c);
        }
        
        static void handle_transform_error (Context c)
        {
            auto *o = Object::self(c);
            auto *mob = PrinterMain::Object::self(c);
            AMBRO_ASSERT(o->splitting)
            AMBRO_ASSERT(mob->planner_state != PLANNER_NONE)
            AMBRO_ASSERT(mob->m_planning_pull_pending)
            
            o->move_err_output->reply_append_error(c, AMBRO_PSTR("Transform"));
            o->move_err_output->reply_poke(c);
            
            correct_after_aborted_move(c);
            
            ThePlanner::emptyDone(c);
            submitted_planner_command(c);
            
            return o->move_end_callback(c, true);
        }
        
        static void correct_after_aborted_move (Context c)
        {
            auto *o = Object::self(c);
            o->virt_update_pending = false;
            o->splitting = false;
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
            
            FpType prev_frac = o->frac;
            FpType rel_max_v_rec;
            FpType saved_phys_req_pos[NumAxes];
            
            if (o->splitter.pull(c, &rel_max_v_rec, &o->frac)) {
                ListFor<AxesList>([&] APRINTER_TL(axis, axis::save_req_pos(c, saved_phys_req_pos)));
                
                FpType saved_virt_req_pos[NumVirtAxes];
                ListFor<VirtAxesList>([&] APRINTER_TL(axis, axis::save_req_pos(c, saved_virt_req_pos)));
                ListFor<VirtAxesList>([&] APRINTER_TL(axis, axis::compute_split(c, o->frac)));
                bool transform_success = update_phys_from_virt(c);
                ListFor<VirtAxesList>([&] APRINTER_TL(axis, axis::restore_req_pos(c, saved_virt_req_pos)));
                
                if (!transform_success) {
                    // Compute actual positions based on prev_frac.
                    ListFor<VirtAxesList>([&] APRINTER_TL(axis, axis::compute_split(c, prev_frac)));
                    update_phys_from_virt(c);
                    ListFor<SecondaryAxesList>([&] APRINTER_TL(axis, axis::compute_split(c, prev_frac, saved_phys_req_pos)));
                    return handle_transform_error(c);
                }
                
                ListFor<SecondaryAxesList>([&] APRINTER_TL(axis, axis::compute_split(c, o->frac, saved_phys_req_pos)));
            } else {
                o->frac = 1.0f;
                o->splitting = false;
            }
            
            PlannerSplitBuffer *cmd = ThePlanner::getBuffer(c);
            ListFor<AxesList>([&] APRINTER_TL(axis, axis::do_move(c, false, (FpType *)0, cmd)));
            if (o->splitting) {
                ListFor<AxesList>([&] APRINTER_TL(axis, axis::restore_req_pos(c, saved_phys_req_pos)));
            }
            ListFor<LasersList>([&] APRINTER_TL(laser, laser::write_planner_cmd(c, LaserSplitSrc{c, o->frac, prev_frac}, cmd)));
            cmd->axes.rel_max_v_rec = rel_max_v_rec;
            
            ThePlanner::axesCommandDone(c);
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
                // We don't call the move_end_callback.
            }
            do_pending_virt_update(c);
        }
        
        static bool handle_set_position (Context c, TheCommand *err_output)
        {
            auto *o = Object::self(c);
            
            if (o->splitting) {
                o->splitting = false;
                o->virt_update_pending = false;
                bool transform_success = update_phys_from_virt(c);
                if (!transform_success) {
                    err_output->reply_append_error(c, AMBRO_PSTR("Transform"));
                    err_output->reply_poke(c);
                    return false;
                }
            } else {
                do_pending_virt_update(c);
            }
            return true;
        }
        
    public:
        static void handle_corrections_change (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(!o->splitting)
            
            update_virt_from_phys(c);
        }
        
    public:
        template <int VirtAxisIndex>
        class VirtAxis {
            friend PrinterMain;
            
        public:
            struct Object;
            
        private:
            using VirtAxisParams = TypeListGet<ParamsVirtAxesList, VirtAxisIndex>;
            static char const AxisName = VirtAxisParams::Name;
            static_assert(NameCharIsValid<AxisName, ReservedAxisNames>::Value, "Virt-axis name not allowed");
            static int const PhysAxisIndex = FindAxis<TypeListGet<ParamsPhysAxesList, VirtAxisIndex>::Value>::Value;
            using ThePhysAxis = Axis<PhysAxisIndex>;
            static_assert(!ThePhysAxis::AxisSpec::IsCartesian, "");
            using WrappedPhysAxisIndex = WrapInt<PhysAxisIndex>;
            static bool const IsExtruder = false;
            
        public:
            using CMinPos = decltype(ExprCast<FpType>(Config::e(VirtAxisParams::MinPos::i())));
            using CMaxPos = decltype(ExprCast<FpType>(Config::e(VirtAxisParams::MaxPos::i())));
            
        private:
            using CMaxSpeedFactor = decltype(ExprCast<FpType>(TimeConversion() / Config::e(VirtAxisParams::MaxSpeed::i())));
            
        public:
            using ConfigExprs = MakeTypeList<CMinPos, CMaxPos, CMaxSpeedFactor>;
            
        private:
            template <typename ThePrinterMain=PrinterMain>
            static constexpr typename ThePrinterMain::PhysVirtAxisMaskType AxisMask () { return (PhysVirtAxisMaskType)1 << (NumAxes + VirtAxisIndex); }
            
            static void init (Context c)
            {
                auto *mo = PrinterMain::Object::self(c);
                AMBRO_ASSERT(!(mo->axis_relative & AxisMask()))
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
            
            static FpType limit_virt_axis_speed (FpType accum, Context c)
            {
                auto *o = Object::self(c);
                return FloatMax(accum, FloatAbs(o->m_delta) * APRINTER_CFG(Config, CMaxSpeedFactor, c));
            }
            
            static FpType clamp_virt_pos (Context c, FpType req)
            {
                return FloatMax(APRINTER_CFG(Config, CMinPos, c), FloatMin(APRINTER_CFG(Config, CMaxPos, c), req));
            }
            
            static void start_phys_homing (Context c) {}
            
        public:
            struct Object : public ObjBase<VirtAxis, typename TransformFeature::Object, MakeTypeList<
                TheTransformAlg,
                TheSplitterClass
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
            TheCommand *move_err_output;
            MoveEndCallback move_end_callback;
        };
    } AMBRO_STRUCT_ELSE(TransformFeature) {
        static int const NumVirtAxes = 0;
        static void init (Context c) {}
        static void handle_virt_move (Context c, FpType time_freq_by_max_speed, TheCommand *move_err_output, MoveEndCallback callback, bool is_positioning_move) {}
        static void correct_after_aborted_move (Context c) {}
        template <int PhysAxisIndex>
        static void mark_phys_moved (Context c) {}
        static void do_pending_virt_update (Context c) {}
        static bool is_splitting (Context c) { return false; }
        static void do_split (Context c) {}
        static void handle_aborted (Context c) {}
        static bool handle_set_position (Context c, TheCommand *err_output) { return true; }
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
    
    template <int PhysVirtAxisIndex, typename This=PrinterMain>
    using GetVirtAxis = typename This::TransformFeature::template VirtAxis<GetVirtAxisVirtIndex<PhysVirtAxisIndex>::Value>;
    
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
        
        static void collect_axis_mask (char axis_name, PhysVirtAxisMaskType *mask)
        {
            if (AMBRO_UNLIKELY(axis_name == TheAxis::AxisName)) {
                *mask |= AxisMask;
            }
        }
        
        static bool collect_new_pos (Context c, TheCommand *cmd, CommandPartRef part, PhysVirtAxisMaskType axis_relative)
        {
            auto *axis = TheAxis::Object::self(c);
            
            if (AMBRO_UNLIKELY(cmd->getPartCode(c, part) == TheAxis::AxisName)) {
                FpType req = cmd->getPartFpValue(c, part);
                if ((axis_relative & AxisMask)) {
                    req += axis->m_old_pos;
                }
                move_add_axis<PhysVirtAxisIndex>(c, req);
                return false;
            }
            return true;
        }
        
        static void set_relative_positioning (Context c, bool relative, bool extruders_only)
        {
            auto *mo = PrinterMain::Object::self(c);
            if (!extruders_only || TheAxis::IsExtruder) {
                if (relative) {
                    mo->axis_relative |= AxisMask;
                } else {
                    mo->axis_relative &= ~AxisMask;
                }
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
        
        template <typename TheJsonBuilder>
        static void get_json_status (Context c, TheJsonBuilder *json)
        {
            auto *axis = TheAxis::Object::self(c);
            json->addKeyObject(JsonSafeChar{TheAxis::AxisName});
            json->addSafeKeyVal("pos", JsonDouble{axis->m_req_pos});
            json->endObject();
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
        
        static void start_phys_homing (Context c)
        {
            TheAxis::start_phys_homing(c);
        }
    };
    
public:
    using PhysVirtAxisHelperList = IndexElemListCount<NumPhysVirtAxes, PhysVirtAxisHelper>;
    
    template <char AxisName>
    using FindPhysVirtAxis = TypeListIndexMapped<PhysVirtAxisHelperList, GetMemberType_WrappedAxisName, WrapInt<AxisName>>;
    
    template <char AxisName>
    using GetPhysVirtAxisByName = PhysVirtAxisHelper<FindPhysVirtAxis<AxisName>::Value>;
    
private:
    using MotionPlannerChannelsDict = ListCollect<ModuleClassesList, MemberType_MotionPlannerChannels>;
    
    using MotionPlannerChannels = TypeDictValues<MotionPlannerChannelsDict>;
    using MotionPlannerAxes = MapTypeList<AxesList, GetMemberType_PlannerAxisSpec>;
    using MotionPlannerLasers = MapTypeList<LasersList, GetMemberType_PlannerLaserSpec>;
    
public:
    APRINTER_MAKE_INSTANCE(ThePlanner, (MotionPlannerArg<
        Context, typename PlannerUnionPlanner::Object, Config, MotionPlannerAxes, Params::StepperSegmentBufferSize,
        Params::LookaheadBufferSize, Params::LookaheadCommitCount, FpType, MaxStepsPerCycle,
        PlannerPullHandler, PlannerFinishedHandler, PlannerAbortedHandler, PlannerUnderrunCallback,
        MotionPlannerChannels, MotionPlannerLasers
    >))
    using PlannerSplitBuffer = typename ThePlanner::SplitBuffer;
    
    template <typename PlannerChannelSpec>
    using GetPlannerChannelIndex = TypeListIndex<MotionPlannerChannels, PlannerChannelSpec>;
    
private:
    struct DummyCorrectionService {
        static bool const CorrectionEnabled = false;
        template <typename Src, typename Dst, bool Reverse> static void do_correction (Context c, Src src, Dst dst, WrapBool<Reverse>) {}
    };
    using TheCorrectionService = GetServiceFromModuleOrDefault<DummyCorrectionService, typename ServiceList::CorrectionService, MemberType_CorrectionFeature>;
    
private:
    AMBRO_STRUCT_IF(LoadConfigFeature, TheConfigManager::HasStore) {
        static void start_loading (Context c)
        {
            lock(c);
            TheConfigManager::startOperation(c, TheConfigManager::OperationType::LOAD);
        }
    } AMBRO_STRUCT_ELSE(LoadConfigFeature) {
        static void start_loading (Context c) {}
    };
    
private:
    static void virtual_homing_hook_completed (Context c, bool error)
    {
        auto *ob = PrinterMain::Object::self(c);
        AMBRO_ASSERT(!ob->homing_error)
        
        ob->homing_error = error;
        if (ob->homing_error || !ob->homing_default) {
            return homing_finished(c);
        }
        return TheHookExecutor::template startHook<ServiceList::AfterDefaultHomingHookService>(c);
    }
    
    static void after_default_homing_hook_completed (Context c, bool error)
    {
        auto *ob = PrinterMain::Object::self(c);
        AMBRO_ASSERT(!ob->homing_error)
        AMBRO_ASSERT(ob->homing_default)
        
        ob->homing_error = error;
        return homing_finished(c);
    }
    
public:
    struct GenericHookDispatcher {
        template <typename HookType>
        using GetHookProviders = GetServiceProviders<HookType>;
        
        template <typename HookType, typename TheServiceProvider>
        static bool dispatchHookToProvider (Context c)
        {
            using TheModule = GetModule<TheServiceProvider::Key::Value>;
            return TheModule::startHook(c, typename TheServiceProvider::Value::UserId(), get_locked(c));
        }
    };
    
private:
    using MyHooks = MakeTypeList<
        HookDefinition<ServiceList::VirtualHomingHookService,      GenericHookDispatcher, AMBRO_WFUNC_T(&PrinterMain::virtual_homing_hook_completed)>,
        HookDefinition<ServiceList::AfterDefaultHomingHookService, GenericHookDispatcher, AMBRO_WFUNC_T(&PrinterMain::after_default_homing_hook_completed)>
    >;
    
    using ModulesHooks = TypeDictValues<ListCollect<ModuleClassesList, MemberType_HookDefinitionList>>;
    
    APRINTER_MAKE_INSTANCE(TheHookExecutor, (HookExecutorArg<Context, typename PrinterMain::Object, JoinTypeLists<MyHooks, ModulesHooks>>))
    
public:
    template <typename HookType>
    static void startHookByInitiator (Context c)
    {
        static_assert(TypeListFindMapped<ModulesHooks, GetMemberType_HookType, HookType>::Found, "This hook is not defined by any module");
        return TheHookExecutor::template startHook<HookType>(c);
    }
    
public:
    template <typename HookType>
    static void hookCompletedByProvider (Context c, bool error)
    {
        return TheHookExecutor::template hookCompletedByProvider<HookType>(c, error);
    }
    
    template <typename HookType>
    static bool hookIsRunning (Context c)
    {
        return TheHookExecutor::template hookIsRunning<HookType>(c);
    }
    
    static void getHomingRequest (Context c, PhysVirtAxisMaskType *req_axes, bool *default_homing)
    {
        AMBRO_ASSERT(hookIsRunning<ServiceList::VirtualHomingHookService>(c) || hookIsRunning<ServiceList::AfterDefaultHomingHookService>(c))
        auto *ob = PrinterMain::Object::self(c);
        
        *req_axes = ob->homing_req_axes;
        *default_homing = ob->homing_default;
    }
    
public:
    static void init (Context c)
    {
        auto *ob = Object::self(c);
        
        TheWatchdog::init(c);
        TheConfigManager::init(c);
        TheConfigCache::init(c);
        TheConfigManager::clearApplyPending(c);
        ob->unlocked_timer.init(c, APRINTER_CB_STATFUNC_T(&PrinterMain::unlocked_timer_handler));
        ob->disable_timer.init(c, APRINTER_CB_STATFUNC_T(&PrinterMain::disable_timer_handler));
        ob->force_timer.init(c, APRINTER_CB_STATFUNC_T(&PrinterMain::force_timer_handler));
        ob->command_stream_list.init();
        ob->msg_length = 0;
        TheBlinker::init(c, (FpType)(Params::LedBlinkInterval::value() * TimeConversion::value()));
        TheSteppers::init(c);
        ob->axis_homing = 0;
        ob->axis_relative = 0;
        ListFor<AxesList>([&] APRINTER_TL(axis, axis::init(c)));
        ListFor<LasersList>([&] APRINTER_TL(laser, laser::init(c)));
        TransformFeature::init(c);
        ob->time_freq_by_max_speed = 0.0f;
        ob->speed_ratio_rec = 1.0f;
        ob->locked = false;
        ob->planner_state = PLANNER_NONE;
        TheHookExecutor::init(c);
        ListFor<ModulesList>([&] APRINTER_TL(module, module::init(c)));
        
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
        ListForReverse<ModulesList>([&] APRINTER_TL(module, module::deinit(c)));
        TheHookExecutor::deinit(c);
        ListForReverse<LasersList>([&] APRINTER_TL(laser, laser::deinit(c)));
        ListForReverse<AxesList>([&] APRINTER_TL(axis, axis::deinit(c)));
        TheSteppers::deinit(c);
        TheBlinker::deinit(c);
        AMBRO_ASSERT(ob->command_stream_list.isEmpty())
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
    
    APRINTER_NO_INLINE
    static void emergency ()
    {
        ListFor<AxesList>([] APRINTER_TL(axis, axis::emergency()));
        ListFor<LasersList>([] APRINTER_TL(laser, laser::emergency()));
        ListFor<ModulesList>([] APRINTER_TL(module, module::emergency()));
    }
    
    static TheCommand * get_locked (Context c)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->locked)
        
        return get_command_in_state(c, COMMAND_LOCKED, true);
    }
    
    static MsgOutputStream * get_msg_output (Context c)
    {
        auto *ob = Object::self(c);
        return &ob->msg_output_stream;
    }
    
    APRINTER_NO_INLINE
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
        
        ListFor<ModulesList>([&] APRINTER_TL(module, module::check_safety(c)));
        TheWatchdog::reset(c);
    }
    struct BlinkerHandler : public AMBRO_WFUNC_TD(&PrinterMain::blinker_handler) {};
    
    APRINTER_NO_INLINE
    static void work_command (Context c, TheCommand *cmd)
    {
        auto *ob = Object::self(c);
        
        char cmd_code = cmd->getCmdCode(c);
        uint16_t cmd_number = cmd->getCmdNumber(c);
        
        if (AMBRO_UNLIKELY(cmd_code == 'M' && (cmd_number == 932 || cmd_number == 933))) {
            if (cmd_number == 932) {
                cmd->m_error = false;
                cmd->m_refuse_on_error = true;
            } else {
                cmd->m_refuse_on_error = false;
            }
            return cmd->finishCommand(c);
        }
        
        if (AMBRO_UNLIKELY(cmd->m_error) && cmd->m_refuse_on_error) {
            cmd->reply_append_error(c, AMBRO_PSTR("PreviousCommandFailed"));
            return cmd->finishCommand(c);
        }
        
        if (AMBRO_UNLIKELY(bool(cmd->m_captured_command_handler))) {
            return cmd->m_captured_command_handler(c, cmd);
        }
        
        switch (cmd_code) {
            case 'M': switch (cmd_number) {
                default:
                    if (
                        TheConfigManager::checkCommand(c, cmd) &&
                        ListForBreak<ModulesList>([&] APRINTER_TL(module, return module::check_command(c, cmd)))
                    ) {
                        goto unknown_command;
                    }
                    return;
                
                case 17: // enable steppers (all steppers if no args, or specific ones)
                case 18: // disable steppers (all steppers if no args, or specific ones)
                case 84: { // same as 18
                    if (!cmd->tryUnplannedCommand(c)) {
                        return;
                    }
                    bool is_enable = (cmd_number == 17);
                    enable_disable_command_common(c, is_enable, cmd);
                    if (is_enable) {
                        now_inactive(c);
                    } else {
                        ob->disable_timer.unset(c);
                    }
                    return cmd->finishCommand(c);
                } break;
                
                case 82:   // extruders to absolute positioning
                case 83: { // extruders to relative positioning
                    bool relative = (cmd_number == 83);
                    ListFor<PhysVirtAxisHelperList>([&] APRINTER_TL(axis, axis::set_relative_positioning(c, relative, true)));
                    return cmd->finishCommand(c);
                } break;
                
                case 114: {
                    ListFor<PhysVirtAxisHelperList>([&] APRINTER_TL(axis, axis::append_position(c, cmd)));
                    cmd->reply_append_ch(c, '\n');
                    return cmd->finishCommand(c);
                } break;
                
                case 119: {
                    cmd->reply_append_pstr(c, AMBRO_PSTR("endstops:"));
                    ListFor<AxesList>([&] APRINTER_TL(axis, axis::m119_append_endstop(c, cmd)));
                    ListFor<ModulesList>([&] APRINTER_TL(module, module::m119_append_endstop(c, cmd)));
                    cmd->reply_append_ch(c, '\n');                    
                    return cmd->finishCommand(c);
                } break;
                
                case 220: {
                    CommandPartRef part;
                    if (cmd->find_command_param(c, 'S', &part)) {
                        FpType ratio_rec = FloatMakePosOrPosZero(100.0f / cmd->getPartFpValue(c, part));
                        ratio_rec = FloatMin((FpType)(1.0f/SpeedRatioMin()), FloatMax((FpType)(1.0f/SpeedRatioMax()), ratio_rec));
                        ob->speed_ratio_rec = ratio_rec;
                    } else {
                        cmd->reply_append_pstr(c, AMBRO_PSTR("Speed factor override: "));
                        cmd->reply_append_fp(c, 100.0f / ob->speed_ratio_rec);
                        cmd->reply_append_ch(c, '\n');
                    }
                    return cmd->finishCommand(c);
                } break;
                
                case 400: {
                    if (!cmd->tryUnplannedCommand(c)) {
                        return;
                    }
                    return cmd->finishCommand(c);
                } break;
                
                case 930: { // apply configuration
                    if (!cmd->tryUnplannedCommand(c)) {
                        return;
                    }
                    update_configuration(c);
                    return cmd->finishCommand(c);
                } break;
            } break;
            
            case 'G': switch (cmd_number) {
                default:
                    if (ListForBreak<ModulesList>([&] APRINTER_TL(module, return module::check_g_command(c, cmd)))) {
                        goto unknown_command;
                    }
                    return;
                
                case 0:   // rapid move
                case 1:   // linear move
                case 4: { // dwell
                    bool is_rapid_move = (cmd_number == 0);
                    bool is_dwell      = (cmd_number == 4);
                    
                    if (!cmd->tryPlannedCommand(c)) {
                        return;
                    }
                    
                    move_begin(c);
                    
                    // Determine absolute/relative interpretation of positions.
                    PhysVirtAxisMaskType axis_relative = ob->axis_relative;
                    char const *r_param = cmd->get_command_param_str(c, 'R', nullptr);
                    if (r_param) {
                        axis_relative = 0;
                        while (*r_param) {
                            ListFor<PhysVirtAxisHelperList>([&] APRINTER_TL(axis, axis::collect_axis_mask(*r_param, &axis_relative)));
                            r_param++;
                        }
                    }
                    
                    bool seen_t = false;
                    
                    for (auto i : LoopRangeAuto(cmd->getNumParts(c))) {
                        CommandPartRef part = cmd->getPart(c, i);
                        
                        // Handle axis positions.
                        if (!is_dwell && !ListForBreak<PhysVirtAxisHelperList>([&] APRINTER_TL(axis, return axis::collect_new_pos(c, cmd, part, axis_relative)))) {
                            continue;
                        }
                        
                        // Handle laser energy/density.
                        if (!ListForBreak<LasersList>([&] APRINTER_TL(laser, return laser::collect_new_pos(c, cmd, part)))) {
                            continue;
                        }
                        
                        // Handle other codes.
                        
                        char code = cmd->getPartCode(c, part);
                        
                        if (!is_dwell && code == 'F') {
                            ob->time_freq_by_max_speed = (FpType)(TimeConversion::value() / Params::SpeedLimitMultiply::value()) / FloatMakePosOrPosZero(cmd->getPartFpValue(c, part));
                        }
                        else if (!is_dwell && code == 'T') {
                            FpType nominal_time_ticks = FloatMakePosOrPosZero(cmd->getPartFpValue(c, part) * (FpType)TimeConversion::value() * ob->speed_ratio_rec);
                            move_set_nominal_time(c, nominal_time_ticks);
                            seen_t = true;
                        }
                        else if (is_dwell && (code == 'P' || code == 'S')) {
                            FpType dwell_time = cmd->getPartFpValue(c, part);
                            if (code == 'P') {
                                dwell_time /= 1000.0f;
                            }
                            FpType dwell_time_ticks = FloatMakePosOrPosZero(dwell_time * (FpType)TimeConversion::value());
                            move_set_nominal_time(c, dwell_time_ticks);
                        }
                    }
                    
                    if (!is_dwell) {
                        if (AMBRO_LIKELY(!seen_t)) {
                            move_set_max_speed_opt(c, ob->time_freq_by_max_speed);
                        }
                    }
                    
                    return move_end(c, get_locked(c), PrinterMain::normal_move_end_callback, is_rapid_move);
                } break;
                
                case 28: { // home axes
                    if (!cmd->tryUnplannedCommand(c)) {
                        return;
                    }
                    AMBRO_ASSERT(ob->axis_homing == 0)
                    PhysVirtAxisMaskType req_axes = 0;
                    for (auto i : LoopRangeAuto(cmd->getNumParts(c))) {
                        ListFor<PhysVirtAxisHelperList>([&] APRINTER_TL(axis, axis::update_homing_mask(c, cmd, &req_axes, cmd->getPart(c, i))));
                    }
                    if (req_axes == 0) {
                        ob->homing_default = true;
                        ob->homing_req_axes = -1;
                    } else {
                        ob->homing_default = false;
                        ob->homing_req_axes = req_axes;
                    }
                    ob->homing_error = false;
                    now_active(c);
                    ListFor<PhysVirtAxisHelperList>([&] APRINTER_TL(axis, axis::start_phys_homing(c)));
                    if (ob->axis_homing == 0) {
                        return phys_homing_finished(c);
                    }
                } break;
                
                case 90:   // absolute positioning
                case 91: { // relative positioning
                    bool relative = (cmd_number == 91);
                    ListFor<PhysVirtAxisHelperList>([&] APRINTER_TL(axis, axis::set_relative_positioning(c, relative, false)));
                    return cmd->finishCommand(c);
                } break;
                
                case 92: { // set position
                    // We use tryPlannedCommand to make sure that the TransformFeature
                    // is not splitting while we adjust the positions.
                    if (!cmd->tryPlannedCommand(c)) {
                        return;
                    }
                    set_position_begin(c);
                    for (auto i : LoopRangeAuto(cmd->getNumParts(c))) {
                        ListFor<PhysVirtAxisHelperList>([&] APRINTER_TL(axis, axis::g92_check_axis(c, cmd, cmd->getPart(c, i))));
                    }
                    if (!set_position_end(c, cmd)) {
                        cmd->reportError(c, nullptr);
                    }
                    return cmd->finishCommand(c);
                } break;
            } break;
            
            unknown_command:
            default: {
                cmd->reportError(c, nullptr);
                cmd->reply_append_pstr(c, AMBRO_PSTR("Error:Unknown command "));
                cmd->reply_append_ch(c, cmd_code);
                cmd->reply_append_uint32(c, cmd_number);
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
        AMBRO_ASSERT(ob->axis_homing == 0)
        
        TransformFeature::do_pending_virt_update(c);
        if (ob->homing_error) {
            return homing_finished(c);
        }
        return TheHookExecutor::template startHook<ServiceList::VirtualHomingHookService>(c);
    }
    
    static void homing_finished (Context c)
    {
        auto *ob = Object::self(c);
        
        now_inactive(c);
        auto *cmd = get_locked(c);
        if (ob->homing_error) {
            cmd->reportError(c, nullptr);
        }
        cmd->finishCommand(c);
    }
    
    static void normal_move_end_callback (Context c, bool error)
    {
        auto *cmd = get_locked(c);
        if (error) {
            cmd->reportError(c, nullptr);
        }
        cmd->finishCommand(c);
    }
    
public:
    static void now_active (Context c)
    {
        auto *ob = Object::self(c);
        
        ob->disable_timer.unset(c);
        TheBlinker::setInterval(c, (FpType)((Params::LedBlinkInterval::value() / 2) * TimeConversion::value()));
    }
    
    static void now_inactive (Context c)
    {
        auto *ob = Object::self(c);
        
        TimeType now = Clock::getTime(c);
        ob->disable_timer.appendAt(c, now + APRINTER_CFG(Config, CInactiveTimeTicks, c));
        TheBlinker::setInterval(c, (FpType)(Params::LedBlinkInterval::value() * TimeConversion::value()));
    }
    
private:
    static void set_force_timer (Context c)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->planner_state == PLANNER_RUNNING)
        AMBRO_ASSERT(ob->m_planning_pull_pending)
        
        TimeType force_time = Clock::getTime(c) + APRINTER_CFG(Config, CForceTimeoutTicks, c);
        ob->force_timer.appendAt(c, force_time);
    }
    
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
        TheDebugObject::access(c);
        
        ListFor<AxesList>([&] APRINTER_TL(axis, axis::enable_disable_stepper(c, false)));
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
            return TransformFeature::do_split(c);
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
    struct PlannerPullHandler : public AMBRO_WFUNC_TD(&PrinterMain::planner_pull_handler) {};
    
    static void planner_finished_handler (Context c)
    {
        auto *ob = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(ob->planner_state != PLANNER_NONE)
        AMBRO_ASSERT(ob->m_planning_pull_pending)
        AMBRO_ASSERT(ob->planner_state != PLANNER_WAITING)
        
        if (ob->planner_state == PLANNER_CUSTOM) {
            ob->custom_planner_deinit_allowed = true;
            return ob->planner_client->finished_handler(c, false);
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
    struct PlannerFinishedHandler : public AMBRO_WFUNC_TD(&PrinterMain::planner_finished_handler) {};
    
    static void planner_aborted_handler (Context c)
    {
        auto *ob = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(ob->planner_state == PLANNER_CUSTOM)
        
        ListFor<AxesList>([&] APRINTER_TL(axis, axis::fix_aborted_pos(c)));
        TransformFeature::handle_aborted(c);
        ob->custom_planner_deinit_allowed = true;
        
        return ob->planner_client->finished_handler(c, true);
    }
    struct PlannerAbortedHandler : public AMBRO_WFUNC_TD(&PrinterMain::planner_aborted_handler) {};
    
    static void planner_underrun_callback (Context c)
    {
        ListFor<ModulesList>([&] APRINTER_TL(module, module::planner_underrun(c)));
    }
    struct PlannerUnderrunCallback : public AMBRO_WFUNC_TD(&PrinterMain::planner_underrun_callback) {};
    
public:
    static void move_begin (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(!TransformFeature::is_splitting(c))
        
        o->custom_planner_deinit_allowed = false;
        
        o->move_seen_cartesian = false;
        o->move_axes = 0;
        o->move_time_freq_by_max_speed = 0.0f;
        
        save_all_pos_to_old(c);
        
        ListFor<LasersList>([&] APRINTER_TL(laser, laser::prepare_laser_for_move(c)));
        
        PlannerSplitBuffer *cmd = ThePlanner::getBuffer(c);
        cmd->axes.rel_max_v_rec = 0.0f;
    }
    
    template <int PhysVirtAxisIndex>
    static void move_add_axis (Context c, FpType value, bool ignore_limits=false)
    {
        auto *o = Object::self(c);
        
        o->move_axes |= PhysVirtAxisHelper<PhysVirtAxisIndex>::AxisMask;
        PhysVirtAxisHelper<PhysVirtAxisIndex>::update_new_pos(c, value, ignore_limits);
    }
    
    template <int LaserIndex>
    static void move_add_laser (Context c, FpType energy)
    {
        auto *laser = Laser<LaserIndex>::Object::self(c);
        laser->move_energy = FloatMakePosOrPosZero(energy);
        laser->move_energy_specified = true;
    }
    
    static void move_set_nominal_time (Context c, FpType nominal_time_ticks)
    {
        AMBRO_ASSERT(FloatIsPosOrPosZero(nominal_time_ticks))
        
        PlannerSplitBuffer *cmd = ThePlanner::getBuffer(c);
        cmd->axes.rel_max_v_rec = nominal_time_ticks;
    }
    
    static void move_set_max_speed (Context c, FpType max_speed)
    {
        auto *o = Object::self(c);
        
        o->move_time_freq_by_max_speed = ((FpType)TimeConversion::value() / FloatMakePosOrPosZero(max_speed)) * o->speed_ratio_rec;
    }
    
    static void move_set_max_speed_opt (Context c, FpType time_freq_by_max_speed)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(FloatIsPosOrPosZero(time_freq_by_max_speed))
        
        o->move_time_freq_by_max_speed = time_freq_by_max_speed * o->speed_ratio_rec;
    }
    
    static void move_end (Context c, TheCommand *err_output, MoveEndCallback callback, bool is_rapid_move=true)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(ob->planner_state == PLANNER_RUNNING || ob->planner_state == PLANNER_CUSTOM)
        AMBRO_ASSERT(ob->m_planning_pull_pending)
        AMBRO_ASSERT(err_output)
        AMBRO_ASSERT(callback)
        
        if (!ListForBreak<ModulesList>([&] APRINTER_TL(module, return module::check_move_interlocks(c, err_output, ob->move_axes)))) {
            restore_all_pos_from_old(c);
            TransformFeature::correct_after_aborted_move(c);
            ThePlanner::emptyDone(c);
            submitted_planner_command(c);
            return callback(c, true);
        }
        
        if (TransformFeature::is_splitting(c)) {
            return TransformFeature::handle_virt_move(c, ob->move_time_freq_by_max_speed, err_output, callback, is_rapid_move);
        }
        
        PlannerSplitBuffer *cmd = ThePlanner::getBuffer(c);
        FpType distance_squared = 0.0f;
        ListFor<AxesList>([&] APRINTER_TL(axis, axis::do_move(c, true, &distance_squared, cmd)));
        TransformFeature::do_pending_virt_update(c);
        if (ob->move_seen_cartesian) {
            FpType distance = FloatSqrt(distance_squared);
            cmd->axes.rel_max_v_rec = FloatMax(cmd->axes.rel_max_v_rec, distance * ob->move_time_freq_by_max_speed);
            ListFor<LasersList>([&] APRINTER_TL(laser, laser::handle_automatic_energy(c, distance, is_rapid_move)));
        } else {
            ListFor<AxesList>([&] APRINTER_TL(axis, axis::limit_axis_move_speed(c, ob->move_time_freq_by_max_speed, cmd)));
        }
        ListFor<LasersList>([&] APRINTER_TL(laser, laser::write_planner_cmd(c, LaserExtraSrc{c}, cmd)));
        ThePlanner::axesCommandDone(c);
        submitted_planner_command(c);
        return callback(c, false);
    }
    
    static void set_position_begin (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->locked)
        AMBRO_ASSERT(!TransformFeature::is_splitting(c))
        
        save_all_pos_to_old(c);
    }
    
    template <int PhysVirtAxisIndex>
    static void set_position_add_axis (Context c, FpType value)
    {
        PhysVirtAxisHelper<PhysVirtAxisIndex>::update_new_pos(c, value, false);
    }
    
    static bool set_position_end (Context c, TheCommand *err_output)
    {
        AMBRO_ASSERT(err_output)
        
        if (!TransformFeature::handle_set_position(c, err_output)) {
            restore_all_pos_from_old(c);
            return false;
        }
        
        ListFor<AxesList>([&] APRINTER_TL(axis, axis::forward_update_pos(c)));
        return true;
    }
    
private:
    struct LaserExtraSrc {
        Context m_c;
        template <int LaserIndex>
        FpType get () { return Laser<LaserIndex>::Object::self(m_c)->move_energy; }
    };
    
    static void save_all_pos_to_old (Context c)
    {
        ListFor<PhysVirtAxisHelperList>([&] APRINTER_TL(axis, axis::save_pos_to_old(c)));
    }
    
    static void restore_all_pos_from_old (Context c)
    {
        ListFor<PhysVirtAxisHelperList>([&] APRINTER_TL(axis, axis::restore_pos_from_old(c)));
    }
    
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
    
public:
    template <typename TheJsonBuilder>
    static void get_json_status (Context c, TheJsonBuilder *json)
    {
        auto *o = Object::self(c);
        
        char status_code = 'I';
        if (o->planner_state != OneOf(PLANNER_NONE, PLANNER_CUSTOM)) {
            status_code = (o->planner_state == PLANNER_STOPPING) ? 'D' : ThePlanner::getStatusCode(c);
        }
        if (status_code == 'I' && o->locked) {
            status_code = 'B';
        }
        
        json->addSafeKeyVal("status", JsonSafeChar{status_code});
        json->addSafeKeyVal("speedRatio", JsonDouble{1.0f / o->speed_ratio_rec});
        
        CallIfExists_get_json_status::template call_void<TheConfigManager>(c, json);
        
        json->addKeyObject(JsonSafeString{"axes"});
        ListFor<PhysVirtAxisHelperList>([&] APRINTER_TL(axis, axis::get_json_status(c, json)));
        json->endObject();
        
        ListFor<ModulesList>([&] APRINTER_TL(module, module::get_json_status(c, json)));
    }
    
private:
    static void config_manager_handler (Context c, bool success)
    {
        auto *ob = Object::self(c);
        AMBRO_ASSERT(TheConfigManager::HasStore)
        AMBRO_ASSERT(ob->locked)
        
        update_configuration(c);
        unlock(c);
        auto msg = success ? AMBRO_PSTR("//LoadConfigOk\n") : AMBRO_PSTR("//LoadConfigErr\n");
        print_pgm_string(c, msg);
    }
    struct ConfigManagerHandler : public AMBRO_WFUNC_TD(&PrinterMain::config_manager_handler) {};
    
    static void update_configuration (Context c)
    {
        TheConfigCache::update(c);
        TheConfigManager::clearApplyPending(c);
        ListFor<AxesList>([&] APRINTER_TL(axis, axis::forward_update_pos(c)));
        ListFor<ModulesList>([&] APRINTER_TL(module, module::configuration_changed(c)));
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
            ListFor<AxesList>([&] APRINTER_TL(axis, axis::enable_disable_stepper(c, enable)));
        } else {
            for (auto i : LoopRangeAuto(num_parts)) {
                ListFor<AxesList>([&] APRINTER_TL(axis, axis::enable_disable_stepper_specific(c, enable, cmd, cmd->getPart(c, i))));
            }
        }
    }
    
private:
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
            PlannerUnion,
            TheHookExecutor
        >
    >> {
        typename Context::EventLoop::QueuedEvent unlocked_timer;
        typename Context::EventLoop::TimedEvent disable_timer;
        typename Context::EventLoop::TimedEvent force_timer;
        DoubleEndedList<CommandStream, &CommandStream::m_list_node, false> command_stream_list;
        MsgOutputStream msg_output_stream;
        FpType time_freq_by_max_speed;
        FpType speed_ratio_rec;
        FpType move_time_freq_by_max_speed;
        size_t msg_length;
        bool locked : 1;
        uint8_t planner_state : 3;
        bool m_planning_pull_pending : 1;
        bool move_seen_cartesian : 1;
        bool custom_planner_deinit_allowed : 1;
        bool homing_error : 1;
        bool homing_default : 1;
        PlannerClient *planner_client;
        PhysVirtAxisMaskType axis_homing;
        PhysVirtAxisMaskType axis_relative;
        union {
            PhysVirtAxisMaskType homing_req_axes;
            PhysVirtAxisMaskType move_axes;
        };
        char msg_buffer[MaxMsgSize];
    };
};

APRINTER_ALIAS_STRUCT_EXT(PrinterMainArg, (
    APRINTER_AS_TYPE(Context),
    APRINTER_AS_TYPE(ParentObject),
    APRINTER_AS_TYPE(Params)
), (
    APRINTER_DEF_INSTANCE(PrinterMainArg, PrinterMain)
))

#include <aprinter/EndNamespace.h>

#endif
