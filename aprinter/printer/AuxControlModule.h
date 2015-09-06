/*
 * Copyright (c) 2015 Ambroz Bizjak
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

#ifndef APRINTER_AUX_CONTROL_MODULE_H
#define APRINTER_AUX_CONTROL_MODULE_H

#include <stdint.h>
#include <math.h>

#include <aprinter/meta/FuncUtils.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/IndexElemList.h>
#include <aprinter/meta/PowerOfTwo.h>
#include <aprinter/meta/Union.h>
#include <aprinter/meta/UnionGet.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/MemberType.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Likely.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/printer/Configuration.h>
#include <aprinter/printer/MotionPlanner.h>
#include <aprinter/misc/ClockUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename TThePrinterMain, typename Params>
class AuxControlModule {
public:
    struct Object;
    
private:
    using ThePrinterMain = TThePrinterMain;
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using TheClockUtils = ClockUtils<Context>;
    using Config = typename ThePrinterMain::Config;
    using TheCommand = typename ThePrinterMain::TheCommand;
    using FpType = typename ThePrinterMain::FpType;
    using TimeConversion = typename ThePrinterMain::TimeConversion;
    
    using ParamsHeatersList = typename Params::HeatersList;
    using ParamsFansList = typename Params::FansList;
    static int const NumHeaters = TypeListLength<ParamsHeatersList>::Value;
    static int const NumFans = TypeListLength<ParamsFansList>::Value;
    
    using CWaitTimeoutTicks = decltype(ExprCast<TimeType>(Config::e(Params::WaitTimeout::i()) * TimeConversion()));
    using CWaitReportPeriodTicks = decltype(ExprCast<TimeType>(Config::e(Params::WaitReportPeriod::i()) * TimeConversion()));
    
    static int const SetHeaterCommand = 104;
    static int const SetFanCommand = 106;
    static int const OffFanCommand = 107;
    
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_init, init)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_deinit, deinit)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_emergency, emergency)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_check_safety, check_safety)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_append_adc_value, append_adc_value)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_update_wait_mask, update_wait_mask)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_start_wait, start_wait)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_stop_wait, stop_wait)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_channel_callback, channel_callback)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_append_value, append_value)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_check_set_command, check_set_command)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_check_command, check_command)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_ChannelPayload, ChannelPayload)
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        o->waiting_heaters = 0;
        ListForEachForward<HeatersList>(LForeach_init(), c);
        ListForEachForward<FansList>(LForeach_init(), c);
    }
    
    static void deinit (Context c)
    {
        ListForEachReverse<FansList>(LForeach_deinit(), c);
        ListForEachReverse<HeatersList>(LForeach_deinit(), c);
    }
    
    static bool check_command (Context c, TheCommand *cmd)
    {
        if (cmd->getCmdNumber(c) == SetHeaterCommand) {
            handle_set_heater_command(c, cmd);
            return false;
        }
        if (cmd->getCmdNumber(c) == 105) {
            handle_print_heaters_command(c, cmd);
            return false;
        }
        if (cmd->getCmdNumber(c) == SetFanCommand || cmd->getCmdNumber(c) == OffFanCommand) {
            handle_set_fan_command(c, cmd, cmd->getCmdNumber(c) == OffFanCommand);
            return false;
        }
        if (cmd->getCmdNumber(c) == 116) {
            handle_wait_heaters_command(c, cmd);
            return false;
        }
        if (cmd->getCmdNumber(c) == 921) {
            handle_print_adc_command(c, cmd);
            return false;
        }
        return ListForEachForwardInterruptible<HeatersList>(LForeach_check_command(), c, cmd) &&
               ListForEachForwardInterruptible<FansList>(LForeach_check_command(), c, cmd);
    }
    
    static void emergency ()
    {
        ListForEachForward<HeatersList>(LForeach_emergency());
        ListForEachForward<FansList>(LForeach_emergency());
    }
    
    static void check_safety (Context c)
    {
        ListForEachForward<HeatersList>(LForeach_check_safety(), c);
    }
    
private:
    template <typename Name>
    static void print_name (Context c, TheCommand *cmd)
    {
        cmd->reply_append_ch(c, Name::Letter);
        if (Name::Number != 0) {
            cmd->reply_append_uint8(c, Name::Number);
        }
    }
    
    template <typename Name>
    static bool match_name (Context c, TheCommand *cmd)
    {
        typename TheCommand::PartRef part;
        return cmd->find_command_param(c, Name::Letter, &part) && cmd->getPartUint32Value(c, part) == Name::Number;
    }
    
    template <int HeaterIndex>
    struct Heater {
        struct Object;
        struct ObserverGetValueCallback;
        struct ObserverHandler;
        
        using HeaterSpec = TypeListGet<ParamsHeatersList, HeaterIndex>;
        using ControlInterval = decltype(Config::e(HeaterSpec::ControlInterval::i()));
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
        using InfAdcValueFp = decltype(ExprFmax(AdcFpLowLimit(), TempToAdcAbs(Config::e(HeaterSpec::MaxSafeTemp::i()))));
        using SupAdcValueFp = decltype(ExprFmin(AdcFpHighLimit(), TempToAdcAbs(Config::e(HeaterSpec::MinSafeTemp::i()))));
        
        using CMinSafeTemp = decltype(ExprCast<FpType>(Config::e(HeaterSpec::MinSafeTemp::i())));
        using CMaxSafeTemp = decltype(ExprCast<FpType>(Config::e(HeaterSpec::MaxSafeTemp::i())));
        using CInfAdcValue = decltype(ExprCast<AdcIntType>(InfAdcValueFp()));
        using CSupAdcValue = decltype(ExprCast<AdcIntType>(SupAdcValueFp()));
        using CControlIntervalTicks = decltype(ExprCast<TimeType>(ControlInterval() * TimeConversion()));
        
        struct ChannelPayload {
            FpType target;
        };
        
        template <typename This=AuxControlModule>
        static constexpr typename This::HeatersMaskType HeaterMask () { return (HeatersMaskType)1 << HeaterIndex; }
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            o->m_enabled = false;
            o->m_target = 0.0f;
            o->m_was_not_unset = false;
            TimeType time = Clock::getTime(c) + (TimeType)(0.05 * TimeConversion::value());
            o->m_control_event.init(c, APRINTER_CB_STATFUNC_T(&Heater::control_event_handler));
            o->m_control_event.appendAt(c, time + (APRINTER_CFG(Config, CControlIntervalTicks, c) / 2));
            ThePwm::init(c, time);
            TheObserver::init(c);
        }
        
        static void deinit (Context c)
        {
            auto *o = Object::self(c);
            TheObserver::deinit(c);
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
            auto *o = Object::self(c);
            
            FpType value = get_temp(c);
            FpType target = NAN;
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                if (o->m_enabled) {
                    target = o->m_target;
                }
            }
            
            cmd->reply_append_ch(c, ' ');
            print_name<typename HeaterSpec::Name>(c, cmd);
            cmd->reply_append_ch(c, ':');
            cmd->reply_append_fp(c, value);
            cmd->reply_append_pstr(c, AMBRO_PSTR(" /"));
            cmd->reply_append_fp(c, target);
        }
        
        static void append_adc_value (Context c, TheCommand *cmd)
        {
            AdcFixedType adc_value = get_adc(c);
            cmd->reply_append_ch(c, ' ');
            print_name<typename HeaterSpec::Name>(c, cmd);
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
        
        static bool check_set_command (Context c, TheCommand *cmd, bool use_default)
        {
            if (!use_default ? match_name<typename HeaterSpec::Name>(c, cmd) : (HeaterSpec::SetMCommand != 0 && SetHeaterCommand == HeaterSpec::SetMCommand)) {
                handle_set_command(c, cmd);
                return false;
            }
            return true;
        }
        
        static bool check_command (Context c, TheCommand *cmd)
        {
            if (HeaterSpec::SetMCommand != 0 && HeaterSpec::SetMCommand != SetHeaterCommand && cmd->getCmdNumber(c) == HeaterSpec::SetMCommand) {
                if (cmd->tryPlannedCommand(c)) {
                    handle_set_command(c, cmd);
                }
                return false;
            }
            return true;
        }
        
        static void handle_set_command (Context c, TheCommand *cmd)
        {
            FpType target = cmd->get_command_param_fp(c, 'S', 0.0f);
            cmd->finishCommand(c);
            if (!(target >= APRINTER_CFG(Config, CMinSafeTemp, c) && target <= APRINTER_CFG(Config, CMaxSafeTemp, c))) {
                target = NAN;
            }
            auto *planner_cmd = ThePlanner<>::getBuffer(c);
            PlannerChannelPayload *payload = UnionGetElem<PlannerChannelIndex<>::Value>(&planner_cmd->channel_payload);
            payload->type = HeaterIndex;
            UnionGetElem<HeaterIndex>(&payload->heaters)->target = target;
            ThePlanner<>::channelCommandDone(c, PlannerChannelIndex<>::Value + 1);
            ThePrinterMain::submitted_planner_command(c);
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
            auto *mo = AuxControlModule::Object::self(c);
            AMBRO_ASSERT(TheObserver::isObserving(c))
            AMBRO_ASSERT(mo->waiting_heaters & HeaterMask())
            
            if (state) {
                mo->inrange_heaters |= HeaterMask();
            } else {
                mo->inrange_heaters &= ~HeaterMask();
            }
            check_wait_completion(c);
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
        
        static void control_event_handler (Context c)
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
            
            if (TheObserver::isObserving(c) && !enabled) {
                fail_wait(c);
            }
            
            maybe_report(c);
        }
        
        template <typename TheHeatersMaskType>
        static void update_wait_mask (Context c, TheCommand *cmd, TheHeatersMaskType *mask)
        {
            if (match_name<typename HeaterSpec::Name>(c, cmd)) {
                *mask |= HeaterMask();
            }
        }
        
        template <typename TheHeatersMaskType>
        static void start_wait (Context c, TheHeatersMaskType mask)
        {
            auto *o = Object::self(c);
            auto *mo = AuxControlModule::Object::self(c);
            
            if ((mask & HeaterMask())) {
                FpType target = NAN;
                AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                    if (o->m_enabled) {
                        target = o->m_target;
                    }
                }
                
                if (!isnan(target)) {
                    mo->waiting_heaters |= HeaterMask();
                    TheObserver::startObserving(c, target);
                }
            }
        }
        
        static void stop_wait (Context c)
        {
            auto *mo = AuxControlModule::Object::self(c);
            
            if ((mo->waiting_heaters & HeaterMask())) {
                TheObserver::stopObserving(c);
            }
        }
        
        struct ObserverGetValueCallback : public AMBRO_WFUNC_TD(&Heater::get_temp) {};
        struct ObserverHandler : public AMBRO_WFUNC_TD(&Heater::observer_handler) {};
        
        struct Object : public ObjBase<Heater, typename AuxControlModule::Object, MakeTypeList<
            TheControl,
            ThePwm,
            TheObserver,
            TheFormula
        >> {
            uint8_t m_enabled : 1;
            uint8_t m_was_not_unset : 1;
            FpType m_target;
            typename Context::EventLoop::TimedEvent m_control_event;
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
        
        static bool check_set_command (Context c, TheCommand *cmd, bool is_turn_off, bool use_default)
        {
            if (!use_default ? match_name<typename FanSpec::Name>(c, cmd) : (
                !is_turn_off ?
                (FanSpec::SetMCommand != 0 && SetFanCommand == FanSpec::SetMCommand) :
                (FanSpec::OffMCommand != 0 && OffFanCommand == FanSpec::OffMCommand)
            )) {
                handle_set_command(c, cmd, is_turn_off);
                return false;
            }
            return true;
        }
        
        static bool check_command (Context c, TheCommand *cmd)
        {
            if ((FanSpec::SetMCommand != 0 && FanSpec::SetMCommand != SetFanCommand && cmd->getCmdNumber(c) == FanSpec::SetMCommand) ||
                (FanSpec::OffMCommand != 0 && FanSpec::OffMCommand != OffFanCommand && cmd->getCmdNumber(c) == FanSpec::OffMCommand)
            ) {
                if (cmd->tryPlannedCommand(c)) {
                    handle_set_command(c, cmd, cmd->getCmdNumber(c) == FanSpec::OffMCommand);
                }
                return false;
            }
            return true;
        }
        
        static void handle_set_command (Context c, TheCommand *cmd, bool is_turn_off)
        {
            FpType target = 0.0f;
            if (!is_turn_off) {
                target = 1.0f;
                if (cmd->find_command_param_fp(c, 'S', &target)) {
                    target *= (FpType)FanSpec::SpeedMultiply::value();
                }
            }
            cmd->finishCommand(c);
            auto *planner_cmd = ThePlanner<>::getBuffer(c);
            PlannerChannelPayload *payload = UnionGetElem<PlannerChannelIndex<>::Value>(&planner_cmd->channel_payload);
            payload->type = NumHeaters + FanIndex;
            ThePwm::computeDutyCycle(target, &UnionGetElem<FanIndex>(&payload->fans)->duty);
            ThePlanner<>::channelCommandDone(c, PlannerChannelIndex<>::Value + 1);
            ThePrinterMain::submitted_planner_command(c);
        }
        
        static void emergency ()
        {
            ThePwm::emergency();
        }
        
        template <typename ThisContext, typename TheChannelPayloadUnion>
        static void channel_callback (ThisContext c, TheChannelPayloadUnion *payload_union)
        {
            ChannelPayload *payload = UnionGetElem<FanIndex>(payload_union);
            ThePwm::setDutyCycle(c, payload->duty);
        }
        
        struct Object : public ObjBase<Fan, typename AuxControlModule::Object, MakeTypeList<
            ThePwm
        >> {};
    };
    
    using HeatersList = IndexElemList<ParamsHeatersList, Heater>;
    using FansList = IndexElemList<ParamsFansList, Fan>;
    
    using HeatersChannelPayloadUnion = Union<MapTypeList<HeatersList, GetMemberType_ChannelPayload>>;
    using FansChannelPayloadUnion = Union<MapTypeList<FansList, GetMemberType_ChannelPayload>>;
    
    using HeatersMaskType = ChooseInt<MaxValue(1, NumHeaters), false>;
    static HeatersMaskType const AllHeatersMask = PowerOfTwoMinusOne<HeatersMaskType, NumHeaters>::Value;
    
    struct PlannerChannelPayload {
        uint8_t type;
        union {
            HeatersChannelPayloadUnion heaters;
            FansChannelPayloadUnion fans;
        };
    };
    
    template <typename This=AuxControlModule> struct PlannerChannelCallback;
    using PlannerChannelSpec = MotionPlannerChannelSpec<PlannerChannelPayload, PlannerChannelCallback<>, Params::EventChannelBufferSize, typename Params::EventChannelTimerService>;
    
    template <typename This=AuxControlModule>
    using ThePlanner = typename This::ThePrinterMain::ThePlanner;
    
    template <typename This=AuxControlModule>
    using PlannerChannelIndex = typename This::ThePrinterMain::template GetPlannerChannelIndex<PlannerChannelSpec>;
    
    static void handle_set_heater_command (Context c, TheCommand *cmd)
    {
        if (!cmd->tryPlannedCommand(c)) {
            return;
        }
        if (ListForEachForwardInterruptible<HeatersList>(LForeach_check_set_command(), c, cmd, false) &&
            ListForEachForwardInterruptible<HeatersList>(LForeach_check_set_command(), c, cmd, true)
        ) {
            if (NumHeaters > 0) {
                cmd->reportError(c, AMBRO_PSTR("UnknownHeater"));
            }
            cmd->finishCommand(c);
        }
    }
    
    static void handle_print_heaters_command (Context c, TheCommand *cmd)
    {
        cmd->reply_append_pstr(c, AMBRO_PSTR("ok"));
        print_heaters(c, cmd);
        cmd->finishCommand(c, true);
    }
    
    static void print_heaters (Context c, TheCommand *cmd)
    {
        ListForEachForward<HeatersList>(LForeach_append_value(), c, cmd);
        cmd->reply_append_ch(c, '\n');
    }
    
    static void handle_set_fan_command (Context c, TheCommand *cmd, bool is_turn_off)
    {
        if (!cmd->tryPlannedCommand(c)) {
            return;
        }
        if (ListForEachForwardInterruptible<FansList>(LForeach_check_set_command(), c, cmd, is_turn_off, false) &&
            ListForEachForwardInterruptible<FansList>(LForeach_check_set_command(), c, cmd, is_turn_off, true)
        ) {
            if (NumFans > 0) {
                cmd->reportError(c, AMBRO_PSTR("UnknownFan"));
            }
            cmd->finishCommand(c);
        }
    }
    
    static void handle_wait_heaters_command (Context c, TheCommand *cmd)
    {
        auto *o = Object::self(c);
        if (!cmd->tryUnplannedCommand(c)) {
            return;
        }
        AMBRO_ASSERT(o->waiting_heaters == 0)
        HeatersMaskType heaters_mask = 0;
        ListForEachForward<HeatersList>(LForeach_update_wait_mask(), c, cmd, &heaters_mask);
        if (heaters_mask == 0) {
            heaters_mask = AllHeatersMask;
        }
        o->waiting_heaters = 0;
        o->inrange_heaters = 0;
        o->wait_started_time = Clock::getTime(c);
        ListForEachForward<HeatersList>(LForeach_start_wait(), c, heaters_mask);
        if (o->waiting_heaters != heaters_mask) {
            cmd->reportError(c, AMBRO_PSTR("HeaterNotEnabled"));
            cmd->finishCommand(c);
            ListForEachForward<HeatersList>(LForeach_stop_wait(), c);
            o->waiting_heaters = 0;
            return;
        }
        if (o->waiting_heaters) {
            o->report_poll_timer.setTo(o->wait_started_time);
            ThePrinterMain::now_active(c);
        } else {
            cmd->finishCommand(c);
        }
    }
    
    static void handle_print_adc_command (Context c, TheCommand *cmd)
    {
        cmd->reply_append_pstr(c, AMBRO_PSTR("ok"));
        ListForEachForward<HeatersList>(LForeach_append_adc_value(), c, cmd);
        cmd->reply_append_ch(c, '\n');
        cmd->finishCommand(c, true);
    }
    
    static void complete_wait (Context c, AMBRO_PGM_P errstr)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->waiting_heaters)
        
        TheCommand *cmd = ThePrinterMain::get_locked(c);
        if (errstr) {
            cmd->reportError(c, errstr);
        }
        cmd->finishCommand(c);
        ListForEachForward<HeatersList>(LForeach_stop_wait(), c);
        o->waiting_heaters = 0;
        ThePrinterMain::now_inactive(c);
    }
    
    static void check_wait_completion (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->waiting_heaters)
        
        bool reached = o->inrange_heaters == o->waiting_heaters;
        bool timed_out = (TimeType)(Clock::getTime(c) - o->wait_started_time) >= APRINTER_CFG(Config, CWaitTimeoutTicks, c);
        
        if (reached || timed_out) {
            complete_wait(c, timed_out ? AMBRO_PSTR("WaitTimedOut") : nullptr);
        }
    }
    
    static void fail_wait (Context c)
    {
        complete_wait(c, AMBRO_PSTR("HeaterDisabled"));
    }
    
    static void maybe_report (Context c)
    {
        auto *o = Object::self(c);
        if (o->waiting_heaters && o->report_poll_timer.isExpired(c)) {
            o->report_poll_timer.addTime(APRINTER_CFG(Config, CWaitReportPeriodTicks, c));
            auto *output = ThePrinterMain::get_msg_output(c);
            output->reply_append_pstr(c, AMBRO_PSTR("//HeatProgress"));
            print_heaters(c, output);
            output->reply_poke(c);
        }
    }
    
    template <typename This>
    static void planner_channel_callback (typename ThePlanner<This>::template Channel<PlannerChannelIndex<This>::Value>::CallbackContext c, PlannerChannelPayload *payload)
    {
        auto *ob = Object::self(c);
        
        ListForOneBoolOffset<HeatersList, 0>(payload->type, LForeach_channel_callback(), c, &payload->heaters) ||
        ListForOneBoolOffset<FansList, NumHeaters>(payload->type, LForeach_channel_callback(), c, &payload->fans);
    }
    template <typename This> struct PlannerChannelCallback : public AMBRO_WFUNC_TD(&AuxControlModule::template planner_channel_callback<This>) {};
    
public:
    template <typename This=AuxControlModule>
    using GetEventChannelTimer = typename ThePlanner<This>::template GetChannelTimer<PlannerChannelIndex<This>::Value>;
    
    template <int HeaterIndex>
    using GetHeaterPwm = typename Heater<HeaterIndex>::ThePwm;
    
    template <int FanIndex>
    using GetFanPwm = typename Fan<FanIndex>::ThePwm;
    
    using MotionPlannerChannels = MakeTypeList<PlannerChannelSpec>;
    
    using ConfigExprs = MakeTypeList<CWaitTimeoutTicks, CWaitReportPeriodTicks>;
    
public:
    struct Object : public ObjBase<AuxControlModule, ParentObject, JoinTypeLists<
        HeatersList,
        FansList
    >> {
        HeatersMaskType waiting_heaters;
        HeatersMaskType inrange_heaters;
        TimeType wait_started_time;
        typename TheClockUtils::PollTimer report_poll_timer;
    };
};

template <
    char TLetter,
    uint8_t TNumber
>
struct AuxControlName {
    static char const Letter = TLetter;
    static uint8_t const Number = TNumber;
};

template <
    typename TName,
    int TSetMCommand,
    typename TAdcPin,
    typename TFormula,
    typename TMinSafeTemp,
    typename TMaxSafeTemp,
    typename TControlInterval,
    typename TControlService,
    typename TObserverService,
    typename TPwmService
>
struct AuxControlModuleHeaterParams {
    using Name = TName;
    static int const SetMCommand = TSetMCommand;
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
    typename TName,
    int TSetMCommand,
    int TOffMCommand,
    typename TSpeedMultiply,
    typename TPwmService
>
struct AuxControlModuleFanParams {
    using Name = TName;
    static int const SetMCommand = TSetMCommand;
    static int const OffMCommand = TOffMCommand;
    using SpeedMultiply = TSpeedMultiply;
    using PwmService = TPwmService;
};

template <
    int TEventChannelBufferSize,
    typename TEventChannelTimerService,
    typename TWaitTimeout,
    typename TWaitReportPeriod,
    typename THeatersList,
    typename TFansList
>
struct AuxControlModuleService {
    static int const EventChannelBufferSize = TEventChannelBufferSize;
    using EventChannelTimerService = TEventChannelTimerService;
    using WaitTimeout = TWaitTimeout;
    using WaitReportPeriod = TWaitReportPeriod;
    using HeatersList = THeatersList;
    using FansList = TFansList;
    
    template <typename Context, typename ParentObject, typename ThePrinterMain>
    using Module = AuxControlModule<Context, ParentObject, ThePrinterMain, AuxControlModuleService>;
};

#include <aprinter/EndNamespace.h>

#endif
