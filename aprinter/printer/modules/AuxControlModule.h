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
#include <aprinter/meta/PowerOfTwo.h>
#include <aprinter/meta/Union.h>
#include <aprinter/meta/UnionGet.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/MemberType.h>
#include <aprinter/meta/If.h>
#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/AliasStruct.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Hints.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/printer/Configuration.h>
#include <aprinter/printer/MotionPlanner.h>
#include <aprinter/misc/ClockUtils.h>
#include <aprinter/printer/utils/JsonBuilder.h>
#include <aprinter/printer/utils/ModuleUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename TThePrinterMain, typename Params>
class AuxControlModule {
public:
    struct Object;
    
public:
    using ReservedHeaterFanNames = MakeTypeList<WrapInt<'S'>>;
    
private:
    using ThePrinterMain = TThePrinterMain;
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using TheClockUtils = ClockUtils<Context>;
    using Config = typename ThePrinterMain::Config;
    using TheOutputStream = typename ThePrinterMain::TheOutputStream;
    using TheCommand = typename ThePrinterMain::TheCommand;
    using FpType = typename ThePrinterMain::FpType;
    using TimeConversion = typename ThePrinterMain::TimeConversion;
    using PhysVirtAxisMaskType = typename ThePrinterMain::PhysVirtAxisMaskType;
    
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
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_clear_error, clear_error)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_update_wait_mask, update_wait_mask)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_start_wait, start_wait)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_stop_wait, stop_wait)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_channel_callback, channel_callback)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_append_value, append_value)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_check_set_command, check_set_command)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_check_command, check_command)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_check_move_interlocks, check_move_interlocks)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_print_cold_extrude, print_cold_extrude)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_set_cold_extrude, set_cold_extrude)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_get_json_status, get_json_status)
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
        if (cmd->getCmdNumber(c) == 922) {
            handle_clear_error_command(c, cmd);
            return false;
        }
        if (cmd->getCmdNumber(c) == 302) {
            handle_cold_extrude_command(c, cmd);
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
    
    static bool check_move_interlocks (Context c, TheOutputStream *err_output, PhysVirtAxisMaskType move_axes)
    {
        return ListForEachForwardInterruptible<HeatersList>(LForeach_check_move_interlocks(), c, err_output, move_axes);
    }
    
    template <typename TheJsonBuilder>
    static void get_json_status (Context c, TheJsonBuilder *json)
    {
        if (NumHeaters > 0) {
            json->addKeyObject(JsonSafeString{"heaters"});
            ListForEachForward<HeatersList>(LForeach_get_json_status(), c, json);
            json->endObject();
        }
        
        if (NumFans > 0) {
            json->addKeyObject(JsonSafeString{"fans"});
            ListForEachForward<FansList>(LForeach_get_json_status(), c, json);
            json->endObject();
        }
    }
    
private:
    template <typename Name>
    static void print_name (Context c, TheOutputStream *cmd)
    {
        cmd->reply_append_ch(c, Name::Letter);
        if (Name::Number != 0) {
            cmd->reply_append_uint32(c, Name::Number);
        }
    }
    
    template <typename Name, typename TheJsonBuilder>
    static void print_json_name (Context c, TheJsonBuilder *json)
    {
        if (Name::Number == 0) {
            json->add(JsonSafeChar{Name::Letter});
        } else {
            char str[3] = {Name::Letter, '0' + Name::Number, '\0'};
            json->add(JsonSafeString{str});
        }
    }
    
    template <typename Name>
    static bool match_name (Context c, TheCommand *cmd)
    {
        typename TheCommand::PartRef part;
        return cmd->find_command_param(c, Name::Letter, &part) && cmd->getPartUint32Value(c, part) == Name::Number;
    }
    
    struct HeaterState {
        FpType current;
        FpType target;
        bool error;
    };
    
    template <int HeaterIndex>
    struct Heater {
        struct Object;
        struct ObserverGetValueCallback;
        struct ObserverHandler;
        
        using HeaterSpec = TypeListGet<ParamsHeatersList, HeaterIndex>;
        static_assert(NameCharIsValid<HeaterSpec::Name::Letter, ReservedHeaterFanNames>::Value, "Heater name not allowed");
        
        using ControlInterval = decltype(Config::e(HeaterSpec::ControlInterval::i()));
        using TheControl = typename HeaterSpec::ControlService::template Control<Context, Object, Config, ControlInterval, FpType>;
        using ThePwm = typename HeaterSpec::PwmService::template Pwm<Context, Object>;
        using TheObserver = typename HeaterSpec::ObserverService::template Observer<Context, Object, Config, FpType, ObserverGetValueCallback, ObserverHandler>;
        using PwmDutyCycleData = typename ThePwm::DutyCycleData;
        using TheFormula = typename HeaterSpec::Formula::template Formula<Context, Object, Config, FpType>;
        using TheAnalogInput = typename HeaterSpec::AnalogInput::template AnalogInput<Context, Object>;
        using AdcFixedType = typename TheAnalogInput::FixedType;
        using AdcIntType = typename AdcFixedType::IntType;
        using MinSafeTemp = decltype(Config::e(HeaterSpec::MinSafeTemp::i()));
        using MaxSafeTemp = decltype(Config::e(HeaterSpec::MaxSafeTemp::i()));
        
        // compute the ADC readings corresponding to MinSafeTemp and MaxSafeTemp
        using AdcRange = APRINTER_FP_CONST_EXPR((PowerOfTwo<double, AdcFixedType::num_bits>::Value));
        template <typename Temp>
        static auto TempToAdcAbs (Temp) -> decltype(TheFormula::TempToAdc(Temp()) * AdcRange());
        using AdcFpLowLimit = APRINTER_FP_CONST_EXPR(1.0 + 0.1);
        using AdcFpHighLimit = APRINTER_FP_CONST_EXPR((PowerOfTwoMinusOne<AdcIntType, AdcFixedType::num_bits>::Value - 0.1));
        using InfAdcSafeTemp = If<TheFormula::NegativeSlope, decltype(MaxSafeTemp()), decltype(MinSafeTemp())>;
        using SupAdcSafeTemp = If<TheFormula::NegativeSlope, decltype(MinSafeTemp()), decltype(MaxSafeTemp())>;
        using InfAdcValueFp = decltype(ExprFmax(AdcFpLowLimit(), TempToAdcAbs(InfAdcSafeTemp())));
        using SupAdcValueFp = decltype(ExprFmin(AdcFpHighLimit(), TempToAdcAbs(SupAdcSafeTemp())));
        
        using CMinSafeTemp = decltype(ExprCast<FpType>(MinSafeTemp()));
        using CMaxSafeTemp = decltype(ExprCast<FpType>(MaxSafeTemp()));
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
            o->m_was_not_unset = false;
            o->m_report_thermal_runaway = false;
            o->m_target = NAN;
            TimeType time = Clock::getTime(c) + (TimeType)(0.05 * TimeConversion::value());
            o->m_control_event.init(c, APRINTER_CB_STATFUNC_T(&Heater::control_event_handler));
            o->m_control_event.appendAt(c, time + (APRINTER_CFG(Config, CControlIntervalTicks, c) / 2));
            ThePwm::init(c, time);
            TheObserver::init(c);
            TheAnalogInput::init(c);
            ColdExtrusionFeature::init(c);
        }
        
        static void deinit (Context c)
        {
            auto *o = Object::self(c);
            TheAnalogInput::deinit(c);
            TheObserver::deinit(c);
            ThePwm::deinit(c);
            o->m_control_event.deinit(c);
        }
        
        static FpType adc_to_temp (Context c, AdcFixedType adc_value)
        {
            if (TheAnalogInput::isValueInvalid(adc_value)) {
                return NAN;
            }
            FpType adc_fp = adc_value.template fpValue<FpType>();
            if (!TheAnalogInput::IsRounded) {
                adc_fp += (FpType)(0.5 / PowerOfTwo<double, AdcFixedType::num_bits>::Value);
            }
            return TheFormula::adcToTemp(c, adc_fp);
        }
        
        static AdcFixedType get_adc (Context c)
        {
            return TheAnalogInput::getValue(c);
        }
        
        static bool adc_is_unsafe (Context c, AdcFixedType adc_value)
        {
            return
                TheAnalogInput::isValueInvalid(adc_value) ||
                adc_value.bitsValue() <= APRINTER_CFG(Config, CInfAdcValue, c) ||
                adc_value.bitsValue() >= APRINTER_CFG(Config, CSupAdcValue, c);
        }
        
        static FpType get_temp (Context c)
        {
            AdcFixedType adc_raw = get_adc(c);
            return adc_to_temp(c, adc_raw);
        }
        struct ObserverGetValueCallback : public AMBRO_WFUNC_TD(&Heater::get_temp) {};
        
        static HeaterState get_state (Context c)
        {
            auto *o = Object::self(c);
            
            HeaterState st;
            st.current = get_temp(c);
            bool enabled;
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                st.target = o->m_target;
                enabled = o->m_enabled;
            }
            st.error = (!FloatIsNan(st.target) && !enabled);
            return st;
        }
        
        static void append_value (Context c, TheOutputStream *cmd)
        {
            HeaterState st = get_state(c);
            
            cmd->reply_append_ch(c, ' ');
            print_name<typename HeaterSpec::Name>(c, cmd);
            cmd->reply_append_ch(c, ':');
            cmd->reply_append_fp(c, st.current);
            cmd->reply_append_pstr(c, AMBRO_PSTR(" /"));
            cmd->reply_append_fp(c, st.target);
            if (st.error) {
                cmd->reply_append_pstr(c, AMBRO_PSTR(",err"));
            }
        }
        
        static void append_adc_value (Context c, TheCommand *cmd)
        {
            AdcFixedType adc_value = get_adc(c);
            cmd->reply_append_ch(c, ' ');
            print_name<typename HeaterSpec::Name>(c, cmd);
            cmd->reply_append_pstr(c, AMBRO_PSTR("A:"));
            cmd->reply_append_fp(c, adc_value.template fpValue<FpType>());
        }
        
        static void clear_error (Context c, TheCommand *cmd)
        {
            auto *o = Object::self(c);
            
            FpType target;
            bool enabled;
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                target = o->m_target;
                enabled = o->m_enabled;
            }
            
            if (!FloatIsNan(target) && !enabled) {
                set(c, target);
            }
        }
        
        template <typename ThisContext>
        static void set (ThisContext c, FpType target)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(!FloatIsNan(target))
            
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                o->m_target = target;
                o->m_enabled = true;
            }
        }
        
        template <typename ThisContext>
        static void unset (ThisContext c, bool orderly)
        {
            auto *o = Object::self(c);
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                if (orderly) {
                    o->m_target = NAN;
                } else if (o->m_enabled) {
                    o->m_report_thermal_runaway = true;
                }
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
            TheAnalogInput::check_safety(c);
            
            AdcFixedType adc_value = get_adc(c);
            if (adc_is_unsafe(c, adc_value)) {
                unset(c, false);
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
        struct ObserverHandler : public AMBRO_WFUNC_TD(&Heater::observer_handler) {};
        
        static void emergency ()
        {
            ThePwm::emergency();
        }
        
        template <typename ThisContext, typename TheChannelPayloadUnion>
        static void channel_callback (ThisContext c, TheChannelPayloadUnion *payload_union)
        {
            ChannelPayload *payload = UnionGetElem<HeaterIndex>(payload_union);
            if (AMBRO_LIKELY(!FloatIsNan(payload->target))) {
                set(c, payload->target);
            } else {
                unset(c, true);
            }
        }
        
        static void control_event_handler (Context c)
        {
            auto *o = Object::self(c);
            
            o->m_control_event.appendAfterPrevious(c, APRINTER_CFG(Config, CControlIntervalTicks, c));
            
            AdcFixedType adc_value = get_adc(c);
            if (adc_is_unsafe(c, adc_value)) {
                unset(c, false);
            }
            
            bool enabled;
            FpType target;
            bool was_not_unset;
            bool report_thermal_runaway;
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                enabled = o->m_enabled;
                target = o->m_target;
                was_not_unset = o->m_was_not_unset;
                o->m_was_not_unset = enabled;
                report_thermal_runaway = o->m_report_thermal_runaway;
                o->m_report_thermal_runaway = false;
            }
            if (AMBRO_LIKELY(enabled)) {
                if (!was_not_unset) {
                    TheControl::init(c);
                }
                FpType sensor_value = adc_to_temp(c, adc_value);
                if (!FloatIsNan(sensor_value)) {
                    FpType output = TheControl::addMeasurement(c, sensor_value, target);
                    PwmDutyCycleData duty;
                    ThePwm::computeDutyCycle(output, &duty);
                    AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                        if (o->m_was_not_unset) {
                            ThePwm::setDutyCycle(lock_c, duty);
                        }
                    }
                }
            }
            
            if (report_thermal_runaway) {
                auto *output = ThePrinterMain::get_msg_output(c);
                output->reply_append_pstr(c, AMBRO_PSTR("//"));
                print_heater_error(c, output, AMBRO_PSTR("HeaterThermalRunaway"));
                output->reply_poke(c);
            }
            
            if (TheObserver::isObserving(c) && !enabled) {
                TheCommand *cmd = ThePrinterMain::get_locked(c);
                print_heater_error(c, cmd, AMBRO_PSTR("HeaterThermalRunaway"));
                complete_wait(c, true, nullptr);
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
        static bool start_wait (Context c, TheCommand *cmd, TheHeatersMaskType mask)
        {
            auto *o = Object::self(c);
            auto *mo = AuxControlModule::Object::self(c);
            
            if ((mask & HeaterMask()) || mask == 0) {
                FpType target;
                bool enabled;
                AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                    target = o->m_target;
                    enabled = o->m_enabled;
                }
                
                if (!FloatIsNan(target)) {
                    if (!enabled) {
                        print_heater_error(c, cmd, AMBRO_PSTR("HeaterThermalRunaway"));
                        return false;
                    }
                    mo->waiting_heaters |= HeaterMask();
                    TheObserver::startObserving(c, target);
                }
                else if ((mask & HeaterMask())) {
                    print_heater_error(c, cmd, AMBRO_PSTR("HeaterNotEnabled"));
                    return false;
                }
            }
            
            return true;
        }
        
        static void print_heater_error (Context c, TheOutputStream *cmd, AMBRO_PGM_P errstr)
        {
            cmd->reply_append_pstr(c, AMBRO_PSTR("Error:"));
            cmd->reply_append_pstr(c, errstr);
            cmd->reply_append_ch(c, ':');
            print_name<typename HeaterSpec::Name>(c, cmd);
            cmd->reply_append_ch(c, '\n');
        }
        
        static void stop_wait (Context c)
        {
            auto *mo = AuxControlModule::Object::self(c);
            
            if ((mo->waiting_heaters & HeaterMask())) {
                TheObserver::stopObserving(c);
            }
        }
        
        static bool check_move_interlocks (Context c, TheOutputStream *err_output, PhysVirtAxisMaskType move_axes)
        {
            return ColdExtrusionFeature::check_move_interlocks(c, err_output, move_axes);
        }
        
        template <typename TheJsonBuilder>
        static void get_json_status (Context c, TheJsonBuilder *json)
        {
            HeaterState st = get_state(c);
            
            print_json_name<typename HeaterSpec::Name>(c, json);
            json->entryValue();
            json->startObject();
            json->addSafeKeyVal("current", JsonDouble{st.current});
            json->addSafeKeyVal("target", JsonDouble{st.target});
            json->addSafeKeyVal("error", JsonBool{st.error});
            json->endObject();
        }
        
        static void print_cold_extrude (Context c, TheOutputStream *output)
        {
            ColdExtrusionFeature::print_cold_extrude(c, output);
        }
        
        template <typename TheHeatersMaskType>
        static void set_cold_extrude (Context c, bool allow, TheHeatersMaskType heaters_mask)
        {
            ColdExtrusionFeature::set_cold_extrude(c, allow, heaters_mask);
        }
        
        AMBRO_STRUCT_IF(ColdExtrusionFeature, HeaterSpec::ColdExtrusion::Enabled) {
            template <typename AxisName, typename AccumMask>
            using ExtrudersMaskFoldFunc = WrapValue<PhysVirtAxisMaskType, (AccumMask::Value | ThePrinterMain::template GetPhysVirtAxisByName<AxisName::Value>::AxisMask)>;
            using ExtrudersMask = TypeListFold<typename HeaterSpec::ColdExtrusion::ExtruderAxes, WrapValue<PhysVirtAxisMaskType, 0>, ExtrudersMaskFoldFunc>;
            
            using CMinExtrusionTemp = decltype(ExprCast<FpType>(Config::e(HeaterSpec::ColdExtrusion::MinExtrusionTemp::i())));
            using ConfigExprs = MakeTypeList<CMinExtrusionTemp>;
            
            static void init (Context c)
            {
                auto *o = Object::self(c);
                o->cold_extrusion_allowed = false;
            }
            
            static bool check_move_interlocks (Context c, TheOutputStream *err_output, PhysVirtAxisMaskType move_axes)
            {
                auto *o = Object::self(c);
                if (!o->cold_extrusion_allowed && (move_axes & ExtrudersMask::Value)) {
                    FpType temp = get_temp(c);
                    if (!(temp >= APRINTER_CFG(Config, CMinExtrusionTemp, c)) || isinf(temp)) {
                        err_output->reply_append_pstr(c, AMBRO_PSTR("Error:"));
                        err_output->reply_append_pstr(c, AMBRO_PSTR("ColdExtrusionPrevented:"));
                        print_name<typename HeaterSpec::Name>(c, err_output);
                        err_output->reply_append_ch(c, '\n');
                        return false;
                    }
                }
                return true;
            }
            
            static void print_cold_extrude (Context c, TheOutputStream *output)
            {
                auto *o = Object::self(c);
                output->reply_append_ch(c, ' ');
                print_name<typename HeaterSpec::Name>(c, output);
                output->reply_append_ch(c, '=');
                output->reply_append_ch(c, o->cold_extrusion_allowed ? '1' : '0');
            }
            
            template <typename TheHeatersMaskType>
            static void set_cold_extrude (Context c, bool allow, TheHeatersMaskType heaters_mask)
            {
                auto *o = Object::self(c);
                if ((heaters_mask & HeaterMask())) {
                    o->cold_extrusion_allowed = allow;
                }
            }
            
            struct Object : public ObjBase<ColdExtrusionFeature, typename Heater::Object, EmptyTypeList> {
                bool cold_extrusion_allowed;
            };
        }
        AMBRO_STRUCT_ELSE(ColdExtrusionFeature) {
            static void init (Context c) {}
            static bool check_move_interlocks (Context c, TheOutputStream *err_output, PhysVirtAxisMaskType move_axes) { return true; }
            static void print_cold_extrude (Context c, TheOutputStream *output) {}
            template <typename TheHeatersMaskType>
            static void set_cold_extrude (Context c, bool allow, TheHeatersMaskType heaters_mask) {}
            struct Object {};
        };
        
        struct Object : public ObjBase<Heater, typename AuxControlModule::Object, MakeTypeList<
            TheControl,
            ThePwm,
            TheObserver,
            TheFormula,
            TheAnalogInput,
            ColdExtrusionFeature
        >> {
            uint8_t m_enabled : 1;
            uint8_t m_was_not_unset : 1;
            uint8_t m_report_thermal_runaway : 1;
            FpType m_target;
            typename Context::EventLoop::TimedEvent m_control_event;
        };
        
        using ConfigExprs = MakeTypeList<CMinSafeTemp, CMaxSafeTemp, CInfAdcValue, CSupAdcValue, CControlIntervalTicks>;
    };
    
    template <int FanIndex>
    struct Fan {
        struct Object;
        
        using FanSpec = TypeListGet<ParamsFansList, FanIndex>;
        static_assert(NameCharIsValid<FanSpec::Name::Letter, ReservedHeaterFanNames>::Value, "Fan name not allowed");
        
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
        
        template <typename TheJsonBuilder>
        static void get_json_status (Context c, TheJsonBuilder *json)
        {
            FpType target = ThePwm::template getCurrentDutyFp<FpType>(c);
            
            print_json_name<typename FanSpec::Name>(c, json);
            json->entryValue();
            json->startObject();
            json->addSafeKeyVal("target", JsonDouble{target});
            json->endObject();
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
    
    static void print_heaters (Context c, TheOutputStream *cmd)
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
        o->waiting_heaters = 0;
        o->inrange_heaters = 0;
        o->wait_started_time = Clock::getTime(c);
        if (!ListForEachForwardInterruptible<HeatersList>(LForeach_start_wait(), c, cmd, heaters_mask)) {
            cmd->reportError(c, nullptr);
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
    
    static void handle_clear_error_command (Context c, TheCommand *cmd)
    {
        ListForEachForward<HeatersList>(LForeach_clear_error(), c, cmd);
        cmd->finishCommand(c);
    }
    
    static void handle_cold_extrude_command (Context c, TheCommand *cmd)
    {
        if (!cmd->find_command_param(c, 'P', nullptr)) {
            cmd->reply_append_pstr(c, AMBRO_PSTR("ColdExtrude:"));
            ListForEachForward<HeatersList>(LForeach_print_cold_extrude(), c, cmd);
            cmd->reply_append_ch(c, '\n');
        } else {
            bool allow = (cmd->get_command_param_uint32(c, 'P', 0) > 0);
            HeatersMaskType heaters_mask = 0;
            ListForEachForward<HeatersList>(LForeach_update_wait_mask(), c, cmd, &heaters_mask);
            if (heaters_mask == 0) {
                heaters_mask = AllHeatersMask;
            }
            ListForEachForward<HeatersList>(LForeach_set_cold_extrude(), c, allow, heaters_mask);
        }
        cmd->finishCommand(c);
    }
    
    static void complete_wait (Context c, bool error, AMBRO_PGM_P errstr)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->waiting_heaters)
        
        TheCommand *cmd = ThePrinterMain::get_locked(c);
        if (error) {
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
            complete_wait(c, timed_out, AMBRO_PSTR("WaitTimedOut"));
        }
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
        
        ListForOneBool<HeatersList, 0>(payload->type, LForeach_channel_callback(), c, &payload->heaters) ||
        ListForOneBool<FansList, NumHeaters>(payload->type, LForeach_channel_callback(), c, &payload->fans);
    }
    template <typename This> struct PlannerChannelCallback : public AMBRO_WFUNC_TD(&AuxControlModule::template planner_channel_callback<This>) {};
    
public:
    template <typename This=AuxControlModule>
    using GetEventChannelTimer = typename ThePlanner<This>::template GetChannelTimer<PlannerChannelIndex<This>::Value>;
    
    template <int HeaterIndex>
    using GetHeaterPwm = typename Heater<HeaterIndex>::ThePwm;
    
    template <int HeaterIndex>
    using GetHeaterAnalogInput = typename Heater<HeaterIndex>::TheAnalogInput;
    
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

APRINTER_ALIAS_STRUCT(AuxControlName, (
    APRINTER_AS_VALUE(char, Letter),
    APRINTER_AS_VALUE(uint8_t, Number)
))

struct AuxControlNoColdExtrusionParams {
    static bool const Enabled = false;
};

APRINTER_ALIAS_STRUCT_EXT(AuxControlColdExtrusionParams, (
    APRINTER_AS_TYPE(MinExtrusionTemp),
    APRINTER_AS_TYPE(ExtruderAxes)
), (
    static bool const Enabled = true;
))

APRINTER_ALIAS_STRUCT(AuxControlModuleHeaterParams, (
    APRINTER_AS_TYPE(Name),
    APRINTER_AS_VALUE(int, SetMCommand),
    APRINTER_AS_TYPE(AnalogInput),
    APRINTER_AS_TYPE(Formula),
    APRINTER_AS_TYPE(MinSafeTemp),
    APRINTER_AS_TYPE(MaxSafeTemp),
    APRINTER_AS_TYPE(ControlInterval),
    APRINTER_AS_TYPE(ControlService),
    APRINTER_AS_TYPE(ObserverService),
    APRINTER_AS_TYPE(PwmService),
    APRINTER_AS_TYPE(ColdExtrusion)
))

APRINTER_ALIAS_STRUCT(AuxControlModuleFanParams, (
    APRINTER_AS_TYPE(Name),
    APRINTER_AS_VALUE(int, SetMCommand),
    APRINTER_AS_VALUE(int, OffMCommand),
    APRINTER_AS_TYPE(SpeedMultiply),
    APRINTER_AS_TYPE(PwmService)
))

APRINTER_ALIAS_STRUCT_EXT(AuxControlModuleService, (
    APRINTER_AS_VALUE(int, EventChannelBufferSize),
    APRINTER_AS_TYPE(EventChannelTimerService),
    APRINTER_AS_TYPE(WaitTimeout),
    APRINTER_AS_TYPE(WaitReportPeriod),
    APRINTER_AS_TYPE(HeatersList),
    APRINTER_AS_TYPE(FansList)
), (
    APRINTER_MODULE_TEMPLATE(AuxControlModuleService, AuxControlModule)
))

#include <aprinter/EndNamespace.h>

#endif
