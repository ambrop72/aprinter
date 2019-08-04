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
#include <stdio.h>
#include <inttypes.h>

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
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Hints.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/printer/Configuration.h>
#include <aprinter/printer/planning/MotionPlanner.h>
#include <aprinter/misc/ClockUtils.h>
#include <aprinter/printer/utils/JsonBuilder.h>
#include <aprinter/printer/utils/ModuleUtils.h>

namespace APrinter {

enum class HeaterType {Extruder, Bed, Chamber};

template <typename ModuleArg>
class AuxControlModule {
    APRINTER_UNPACK_MODULE_ARG(ModuleArg)
    
public:
    struct Object;
    
private:
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
    
    enum class MCommand : uint16_t {
        PrintHeaters = 105,
        WaitHeaters = 116,
        SetExtruderHeater = 104,
        SetWaitExtruderHeater = 109,
        SetBedHeater = 140,
        SetWaitBedHeater = 190,
        SetChamberHeater = 141,
        SetWaitChamberHeater = 191,
        SetFan = 106,
        TurnOffFan = 107,
        PrintAdc = 921,
        ClearError = 922,
        ColdExtrude = 302,
    };

    enum class WaitMode : bool {NoWait, Wait};

    enum class FanCmdType : bool {TurnOff, Set};

    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_ChannelPayload, ChannelPayload)
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        o->waiting_heaters = 0;
        ListFor<HeatersList>([&] APRINTER_TL(heater, heater::init(c)));
        ListFor<FansList>([&] APRINTER_TL(fan, fan::init(c)));
    }
    
    static void deinit (Context c)
    {
        ListForReverse<FansList>([&] APRINTER_TL(fan, fan::deinit(c)));
        ListForReverse<HeatersList>([&] APRINTER_TL(heater, heater::deinit(c)));
    }
    
    static bool check_command (Context c, TheCommand *cmd)
    {
        switch (MCommand(cmd->getCmdNumber(c))) {
            case MCommand::PrintHeaters:
                handle_print_heaters_command(c, cmd);
                return false;
            case MCommand::WaitHeaters:
                handle_wait_heaters_command(c, cmd);
                return false;
            case MCommand::SetExtruderHeater:
                handle_set_heater_command(c, cmd, HeaterType::Extruder, WaitMode::NoWait);
                return false;
            case MCommand::SetWaitExtruderHeater:
                handle_set_heater_command(c, cmd, HeaterType::Extruder, WaitMode::Wait);
                return false;
            case MCommand::SetBedHeater:
                handle_set_heater_command(c, cmd, HeaterType::Bed, WaitMode::NoWait);
                return false;
            case MCommand::SetWaitBedHeater:
                handle_set_heater_command(c, cmd, HeaterType::Bed, WaitMode::Wait);
                return false;
            case MCommand::SetChamberHeater:
                handle_set_heater_command(c, cmd, HeaterType::Chamber, WaitMode::NoWait);
                return false;
            case MCommand::SetWaitChamberHeater:
                handle_set_heater_command(c, cmd, HeaterType::Chamber, WaitMode::Wait);
                return false;
            case MCommand::SetFan:
                handle_set_fan_command(c, cmd, FanCmdType::Set);
                return false;
            case MCommand::TurnOffFan:
                handle_set_fan_command(c, cmd, FanCmdType::TurnOff);
                return false;
            case MCommand::PrintAdc:
                handle_print_adc_command(c, cmd);
                return false;
            case MCommand::ClearError:
                handle_clear_error_command(c, cmd);
                return false;
            case MCommand::ColdExtrude:
                handle_cold_extrude_command(c, cmd);
                return false;
            default:
                return true;
        }
    }
    
    static void emergency ()
    {
        ListFor<HeatersList>([&] APRINTER_TL(heater, heater::emergency()));
        ListFor<FansList>([&] APRINTER_TL(fan, fan::emergency()));
    }
    
    static void check_safety (Context c)
    {
        ListFor<HeatersList>([&] APRINTER_TL(heater, heater::check_safety(c)));
    }
    
    static bool check_move_interlocks (Context c, TheOutputStream *err_output, PhysVirtAxisMaskType move_axes)
    {
        return ListForBreak<HeatersList>([&] APRINTER_TL(heater, return heater::check_move_interlocks(c, err_output, move_axes)));
    }
    
    template <typename TheJsonBuilder>
    static void get_json_status (Context c, TheJsonBuilder *json)
    {
        if (NumHeaters > 0) {
            json->addKeyObject(JsonSafeString{"heaters"});
            ListFor<HeatersList>([&] APRINTER_TL(heater, heater::get_json_status(c, json)));
            json->endObject();
        }
        
        if (NumFans > 0) {
            json->addKeyObject(JsonSafeString{"fans"});
            ListFor<FansList>([&] APRINTER_TL(fan, fan::get_json_status(c, json)));
            json->endObject();
        }
    }
    
private:
    static void print_ch_num (Context c, TheOutputStream *cmd, char ch, uint8_t num)
    {
        cmd->reply_append_ch(c, ch);
        cmd->reply_append_uint32(c, num);
    }
    
    template <typename TheJsonBuilder>
    static void print_json_ch_num (Context c, TheJsonBuilder *json, char ch, uint8_t num)
    {
        char str[8];
        sprintf(str, "%c%" PRIu8, ch, num);
        json->add(JsonSafeString{str});
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

        // The character representing the heater type, including in M105, M116 and M302.
        // It is typically followed by the heater number, e.g. T0, T1, B0, C0.
        inline static constexpr char HeaterTypeChar =
            (HeaterSpec::Type == HeaterType::Extruder) ? 'T' :
            (HeaterSpec::Type == HeaterType::Bed) ? 'B' :
            (HeaterSpec::Type == HeaterType::Chamber) ? 'C' :
            0; /*impossible*/
        
        // An optional alternative character accepted by M116.
        inline static constexpr char AlternateHeaterTypeCharM116 =
            (HeaterSpec::Type == HeaterType::Extruder) ? 'P' : 0;
        
        using ControlInterval = decltype(Config::e(HeaterSpec::ControlInterval::i()));
        APRINTER_MAKE_INSTANCE(TheControl, (HeaterSpec::ControlService::template Control<Context, Object, Config, ControlInterval, FpType>))
        APRINTER_MAKE_INSTANCE(ThePwm, (HeaterSpec::PwmService::template Pwm<Context, Object>))
        APRINTER_MAKE_INSTANCE(TheObserver, (HeaterSpec::ObserverService::template Observer<Context, Object, Config, FpType, ObserverGetValueCallback, ObserverHandler>))
        using PwmDutyCycleData = typename ThePwm::DutyCycleData;
        APRINTER_MAKE_INSTANCE(TheFormula, (HeaterSpec::Formula::template Formula<Context, Object, Config, FpType>))
        APRINTER_MAKE_INSTANCE(TheAnalogInput, (HeaterSpec::AnalogInput::template AnalogInput<Context, Object>))
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
            print_ch_num(c, cmd, HeaterTypeChar, HeaterSpec::Number);
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
            print_ch_num(c, cmd, HeaterTypeChar, HeaterSpec::Number);
            cmd->reply_append_pstr(c, AMBRO_PSTR("A:"));
            cmd->reply_append_fp(c, adc_value.template fpValue<FpType>());
        }
        
        static void clear_error (Context c)
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
        
        template <typename ThisContext>
        static void set_or_unset (ThisContext c, FpType target)
        {
            if (AMBRO_LIKELY(!FloatIsNan(target))) {
                set(c, target);
            } else {
                unset(c, true);
            }
        }
        
        static bool check_set_command (Context c, TheCommand *cmd, HeaterType heater_type,
            WaitMode wait_mode, bool force, uint32_t heater_number, FpType cmd_value, bool *heaters_of_type_found)
        {
            if (heater_type == HeaterSpec::Type) {
                *heaters_of_type_found = true;
            }

            if (heater_type != HeaterSpec::Type || heater_number != HeaterSpec::Number) {
                return true;
            }

            FpType target;

            if (cmd_value >= APRINTER_CFG(Config, CMinSafeTemp, c) && cmd_value <= APRINTER_CFG(Config, CMaxSafeTemp, c)) {
                target = cmd_value;
            } else {
                target = NAN;
            }

            if (wait_mode == WaitMode::NoWait) {
                cmd->finishCommand(c);
            }

            if (wait_mode == WaitMode::Wait || force) {
                set_or_unset(c, target);
            } else {
                auto *planner_cmd = ThePlanner<>::getBuffer(c);
                PlannerChannelPayload *payload = UnionGetElem<PlannerChannelIndex<>::Value>(&planner_cmd->channel_payload);
                payload->type = HeaterIndex;
                UnionGetElem<HeaterIndex>(&payload->heaters)->target = target;
                ThePlanner<>::channelCommandDone(c, PlannerChannelIndex<>::Value + 1);
                ThePrinterMain::submitted_planner_command(c);
            }

            if (wait_mode == WaitMode::Wait) {
                do_wait_heaters(c, cmd, HeaterMask());
            }
            
            return false;
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
            set_or_unset(c, payload->target);
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
            uint32_t number;
            bool found = cmd->find_command_param_uint32(c, HeaterTypeChar, &number);
            if (!found && AlternateHeaterTypeCharM116 != 0) {
                found = cmd->find_command_param_uint32(c, AlternateHeaterTypeCharM116, &number);
            }
            if (found && number == HeaterSpec::Number) {
                *mask |= HeaterMask();
            }
        }

        template <typename TheHeatersMaskType>
        static void update_cold_extrude_mask (Context c, TheCommand *cmd, TheHeatersMaskType *mask)
        {
            uint32_t number;
            if (cmd->find_command_param_uint32(c, HeaterTypeChar, &number) && number == HeaterSpec::Number) {
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
            print_ch_num(c, cmd, HeaterTypeChar, HeaterSpec::Number);
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
            
            print_json_ch_num(c, json, HeaterTypeChar, HeaterSpec::Number);
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
                        print_ch_num(c, err_output, HeaterTypeChar, HeaterSpec::Number);
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
                print_ch_num(c, output, HeaterTypeChar, HeaterSpec::Number);
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

        APRINTER_MAKE_INSTANCE(ThePwm, (FanSpec::PwmService::template Pwm<Context, Object>))
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
        
        static bool check_set_command (Context c, TheCommand *cmd,
            FanCmdType cmd_type, bool force, uint32_t fan_number, FpType cmd_value)
        {
            if (fan_number != FanSpec::Number) {
                return true;
            }

            FpType target;

            if (cmd_type == FanCmdType::TurnOff) {
                // M107 turns off the fan.
                target = 0.0f;
            } else {
                // M106 contsols the fan based on the S parameter (cmd_value).
                if (FloatIsNan(cmd_value)) {
                    // S not supplied (or has NAN value), turn on to 100%.
                    target = 1.0f;
                } else {
                    // S supplied, turn on according to the S value, but we need to divide
                    // it by 255 since the convention is that 0-255 corresponds to 0-100%.
                    target = cmd_value / 255.0f;
                }
            }
            
            cmd->finishCommand(c);
            
            PwmDutyCycleData duty;
            ThePwm::computeDutyCycle(target, &duty);
            
            if (force) {
                ThePwm::setDutyCycle(c, duty);
            } else {
                auto *planner_cmd = ThePlanner<>::getBuffer(c);
                PlannerChannelPayload *payload = UnionGetElem<PlannerChannelIndex<>::Value>(&planner_cmd->channel_payload);
                payload->type = NumHeaters + FanIndex;
                UnionGetElem<FanIndex>(&payload->fans)->duty = duty;
                ThePlanner<>::channelCommandDone(c, PlannerChannelIndex<>::Value + 1);
                ThePrinterMain::submitted_planner_command(c);
            }

            return false;
        }
        
        template <typename TheJsonBuilder>
        static void get_json_status (Context c, TheJsonBuilder *json)
        {
            FpType target = ThePwm::template getCurrentDutyFp<FpType>(c);
            
            print_json_ch_num(c, json, 'F', FanSpec::Number);
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
    struct PlannerChannelSpec : public MotionPlannerChannelSpec<PlannerChannelPayload, PlannerChannelCallback<AuxControlModule>, Params::EventChannelBufferSize, typename Params::EventChannelTimerService> {};
    
    template <typename This=AuxControlModule>
    using ThePlanner = typename This::ThePrinterMain::ThePlanner;
    
    template <typename This=AuxControlModule>
    using PlannerChannelIndex = typename This::ThePrinterMain::template GetPlannerChannelIndex<PlannerChannelSpec>;
    
    static void handle_set_heater_command (Context c, TheCommand *cmd, HeaterType heater_type, WaitMode wait_mode)
    {
        bool force = (wait_mode == WaitMode::NoWait) && cmd->find_command_param(c, 'F', nullptr);

        if ((wait_mode == WaitMode::Wait) ?
            !cmd->tryUnplannedCommand(c) : (!force && !cmd->tryPlannedCommand(c)))
        {
            return;
        }

        uint32_t heater_number = 0;
        bool heater_specified = false;
        if (heater_type == HeaterType::Extruder) {
            heater_specified = cmd->find_command_param_uint32(c, 'T', &heater_number);
        }
        if (!heater_specified) {
            heater_specified = cmd->find_command_param_uint32(c, 'P', &heater_number);
        }

        FpType cmd_value = cmd->get_command_param_fp(c, 'S', NAN);

        bool heaters_of_type_found = false;

        if (ListForBreak<HeatersList>([&] APRINTER_TL(heater,
            return heater::check_set_command(c, cmd, heater_type, wait_mode,
                force, heater_number, cmd_value, &heaters_of_type_found))))
        {
            // Do not report an error if no heater number was explicitly specified and we
            // do not have any heaters of this type, for compatibility with slicers that
            // emit these commands without respect to the presence of heaters.
            if (heater_specified || heaters_of_type_found) {
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
        ListFor<HeatersList>([&] APRINTER_TL(heater, heater::append_value(c, cmd)));
        cmd->reply_append_ch(c, '\n');
    }
    
    static void handle_set_fan_command (Context c, TheCommand *cmd, FanCmdType cmd_type)
    {
        bool force = cmd->find_command_param(c, 'F', nullptr);

        if (!force && !cmd->tryPlannedCommand(c)) {
            return;
        }

        uint32_t fan_number = 0;
        bool fan_specified = cmd->find_command_param_uint32(c, 'P', &fan_number);

        FpType cmd_value =
            (cmd_type == FanCmdType::Set) ? cmd->get_command_param_fp(c, 'S', NAN) : NAN;
        
        if (ListForBreak<FansList>([&] APRINTER_TL(fan,
            return fan::check_set_command(c, cmd, cmd_type, force, fan_number, cmd_value))))
        {
            // Do not report an error if no fan number was explicitly specified and we do
            // not have any fans, for compatibility with slicers that emit these commands
            // without respect to the presence of fans.
            if (fan_specified || NumFans > 0) {
                cmd->reportError(c, AMBRO_PSTR("UnknownFan"));
            }
            cmd->finishCommand(c);
        }
    }
    
    static void handle_wait_heaters_command (Context c, TheCommand *cmd)
    {
        if (!cmd->tryUnplannedCommand(c)) {
            return;
        }
        HeatersMaskType heaters_mask = 0;
        ListFor<HeatersList>([&] APRINTER_TL(heater, heater::update_wait_mask(c, cmd, &heaters_mask)));
        do_wait_heaters(c, cmd, heaters_mask);
    }

    static void do_wait_heaters (Context c, TheCommand *cmd, HeatersMaskType heaters_mask)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->waiting_heaters == 0)
        o->waiting_heaters = 0;
        o->inrange_heaters = 0;
        o->wait_started_time = Clock::getTime(c);
        if (!ListForBreak<HeatersList>([&] APRINTER_TL(heater, return heater::start_wait(c, cmd, heaters_mask)))) {
            cmd->reportError(c, nullptr);
            cmd->finishCommand(c);
            ListFor<HeatersList>([&] APRINTER_TL(heater, heater::stop_wait(c)));
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
        ListFor<HeatersList>([&] APRINTER_TL(heater, heater::append_adc_value(c, cmd)));
        cmd->reply_append_ch(c, '\n');
        cmd->finishCommand(c, true);
    }
    
    static void handle_clear_error_command (Context c, TheCommand *cmd)
    {
        ListFor<HeatersList>([&] APRINTER_TL(heater, heater::clear_error(c)));
        cmd->finishCommand(c);
    }
    
    static void handle_cold_extrude_command (Context c, TheCommand *cmd)
    {
        if (!cmd->find_command_param(c, 'P', nullptr)) {
            cmd->reply_append_pstr(c, AMBRO_PSTR("ColdExtrude:"));
            ListFor<HeatersList>([&] APRINTER_TL(heater, heater::print_cold_extrude(c, cmd)));
            cmd->reply_append_ch(c, '\n');
        } else {
            bool allow = (cmd->get_command_param_uint32(c, 'P', 0) > 0);
            HeatersMaskType heaters_mask = 0;
            ListFor<HeatersList>([&] APRINTER_TL(heater, heater::update_cold_extrude_mask(c, cmd, &heaters_mask)));
            if (heaters_mask == 0) {
                heaters_mask = AllHeatersMask;
            }
            ListFor<HeatersList>([&] APRINTER_TL(heater, heater::set_cold_extrude(c, allow, heaters_mask)));
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
        ListFor<HeatersList>([&] APRINTER_TL(heater, heater::stop_wait(c)));
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
        
        ListForOneBool<HeatersList, 0>(payload->type, [&] APRINTER_TL(heater, heater::channel_callback(c, &payload->heaters))) ||
        ListForOneBool<FansList, NumHeaters>(payload->type, [&] APRINTER_TL(fan, fan::channel_callback(c, &payload->fans)));
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
    APRINTER_AS_VALUE(HeaterType, Type),
    APRINTER_AS_VALUE(uint8_t, Number),
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
    APRINTER_AS_VALUE(uint8_t, Number),
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

}

#endif
