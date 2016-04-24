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

#ifndef APRINTER_VIRTUAL_HOMING_MODULE
#define APRINTER_VIRTUAL_HOMING_MODULE

#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Hints.h>
#include <aprinter/printer/Configuration.h>
#include <aprinter/printer/ServiceList.h>
#include <aprinter/printer/utils/ModuleUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename ModuleArg>
class VirtualHomingModule {
    APRINTER_UNPACK_MODULE_ARG(ModuleArg)
    
public:
    struct Object;
    
private:
    using FpType = typename ThePrinterMain::FpType;
    using Config = typename ThePrinterMain::Config;
    using TheCommand = typename ThePrinterMain::TheCommand;
    using PhysVirtAxisMaskType = typename ThePrinterMain::PhysVirtAxisMaskType;
    using VirtHomingAxisParamsList = typename Params::VirtHomingAxisParamsList;
    
    enum class State {IDLE, RUNNING};
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        o->event.init(c, APRINTER_CB_STATFUNC_T(&VirtualHomingModule::event_handler));
        o->state = State::IDLE;
        ListFor<VirtHomingAxisList>([&] APRINTER_TL(axis, axis::init(c)));
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->event.deinit(c);
    }
    
    static void m119_append_endstop (Context c, TheCommand *cmd)
    {
        ListFor<VirtHomingAxisList>([&] APRINTER_TL(axis, axis::m119_append_endstop(c, cmd)));
    }
    
    template <typename CallbackContext>
    AMBRO_ALWAYS_INLINE
    static bool prestep_callback (CallbackContext c)
    {
        return !ListForBreak<VirtHomingAxisList>([&] APRINTER_TL(axis, return axis::prestep_callback(c)));
    }
    
    static bool startHook (Context c, DummyServiceUserId, TheCommand *err_output)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == State::IDLE)
        
        bool homing_default;
        ThePrinterMain::getHomingRequest(c, &o->homing_axes, &homing_default);
        
        if (homing_default) {
            ListFor<VirtHomingAxisList>([&] APRINTER_TL(axis, axis::remove_nondefault_axes(c)));
        }
        
        o->state = State::RUNNING;
        o->err_output = err_output;
        o->homing_error = false;
        o->event.prependNowNotAlready(c);
        
        return false;
    }
    
private:
    static void event_handler (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == State::RUNNING)
        
        if (o->homing_error ||
            ListForBreak<VirtHomingAxisList>([&] APRINTER_TL(axis, return axis::start_virt_homing(c))) ||
            o->homing_error
        ) {
            o->state = State::IDLE;
            return ThePrinterMain::template hookCompletedByProvider<ServiceList::VirtualHomingHookService>(c, o->homing_error);
        }
    }
    
    template <int VirtHomingAxisIndex>
    struct VirtHomingAxis {
        struct Object;
        
        using HomingSpec = TypeListGet<VirtHomingAxisParamsList, VirtHomingAxisIndex>;
        static char const AxisName = HomingSpec::AxisName;
        static int const AxisIndex = ThePrinterMain::template FindPhysVirtAxis<AxisName>::Value;
        using PhysVirtAxis = typename ThePrinterMain::template PhysVirtAxisHelper<AxisIndex>;
        using VirtAxis = typename ThePrinterMain::template GetVirtAxis<AxisIndex>;
        
        static void init (Context c)
        {
            Context::Pins::template setInput<typename HomingSpec::EndPin, typename HomingSpec::EndPinInputMode>(c);
        }
        
        static void m119_append_endstop (Context c, TheCommand *cmd)
        {
            bool triggered = endstop_is_triggered(c);
            cmd->reply_append_ch(c, ' ');
            cmd->reply_append_ch(c, AxisName);
            cmd->reply_append_ch(c, ':');
            cmd->reply_append_ch(c, (triggered ? '1' : '0'));
        }
        
        template <typename CallbackContext>
        AMBRO_ALWAYS_INLINE
        static bool prestep_callback (CallbackContext c)
        {
            return !endstop_is_triggered(c);
        }
        
        template <typename ThisContext>
        static bool endstop_is_triggered (ThisContext c)
        {
            return (Context::Pins::template get<typename HomingSpec::EndPin>(c) != APRINTER_CFG(Config, CEndInvert, c));
        }
        
        static void remove_nondefault_axes (Context c)
        {
            auto *mo = VirtualHomingModule::Object::self(c);
            
            if (!APRINTER_CFG(Config, CByDefault, c)) {
                mo->homing_axes &= ~PhysVirtAxis::AxisMask;
            }
        }
        
        static bool start_virt_homing (Context c)
        {
            auto *o = Object::self(c);
            auto *mo = VirtualHomingModule::Object::self(c);
            
            if (!(mo->homing_axes & PhysVirtAxis::AxisMask)) {
                return true;
            }
            set_position(c, home_start_pos(c));
            if (!mo->homing_error) {
                ThePrinterMain::custom_planner_init(c, &o->planner_client, true);
                o->state = 0;
                o->command_sent = false;
            }
            return false;
        }
        
        static void set_position (Context c, FpType value)
        {
            auto *mo = VirtualHomingModule::Object::self(c);
            
            ThePrinterMain::set_position_begin(c);
            ThePrinterMain::template set_position_add_axis<AxisIndex>(c, value);
            if (!ThePrinterMain::set_position_end(c, mo->err_output)) {
                mo->homing_error = true;
            }
        }
        
        static FpType home_start_pos (Context c)
        {
            return APRINTER_CFG(Config, CHomeDir, c) ? APRINTER_CFG(Config, typename VirtAxis::CMinPos, c) : APRINTER_CFG(Config, typename VirtAxis::CMaxPos, c);
        }
        
        static FpType home_end_pos (Context c)
        {
            return APRINTER_CFG(Config, CHomeDir, c) ? APRINTER_CFG(Config, typename VirtAxis::CMaxPos, c) : APRINTER_CFG(Config, typename VirtAxis::CMinPos, c);
        }
        
        static FpType home_dir (Context c)
        {
            return APRINTER_CFG(Config, CHomeDir, c) ? 1.0f : -1.0f;
        }
        
        static void virt_homing_move_end_callback (Context c, bool error)
        {
            auto *mo = VirtualHomingModule::Object::self(c);
            if (error) {
                mo->homing_error = true;
            }
        }
        
        class VirtHomingPlannerClient : public ThePrinterMain::PlannerClient {
        private:
            void pull_handler (Context c)
            {
                auto *o = Object::self(c);
                auto *mo = VirtualHomingModule::Object::self(c);
                
                if (o->command_sent) {
                    return ThePrinterMain::custom_planner_wait_finished(c);
                }
                ThePrinterMain::move_begin(c);
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
                ThePrinterMain::template move_add_axis<AxisIndex>(c, position, ignore_limits);
                ThePrinterMain::move_set_max_speed(c, speed);
                o->command_sent = true;
                return ThePrinterMain::move_end(c, mo->err_output, VirtHomingAxis::virt_homing_move_end_callback);
            }
            
            void finished_handler (Context c, bool aborted)
            {
                auto *o = Object::self(c);
                auto *mo = VirtualHomingModule::Object::self(c);
                AMBRO_ASSERT(o->state < 3)
                AMBRO_ASSERT(o->command_sent)
                
                ThePrinterMain::custom_planner_deinit(c);
                if (o->state != 1) {
                    set_position(c, home_end_pos(c));
                }
                
                if (!mo->homing_error) {
                    if ((o->state == 1) ? endstop_is_triggered(c) : !aborted) {
                        mo->homing_error = true;
                        auto *cmd = mo->err_output;
                        cmd->reply_append_error(c, (o->state == 1) ? AMBRO_PSTR("EndstopTriggeredAfterRetract") : AMBRO_PSTR("EndstopNotTriggered"));
                        cmd->reply_poke(c);
                    }
                }
                
                if (mo->homing_error || o->state == 2) {
                    mo->homing_axes &= ~PhysVirtAxis::AxisMask;
                    mo->event.prependNowNotAlready(c);
                    return;
                }
                
                o->state++;
                o->command_sent = false;
                ThePrinterMain::custom_planner_init(c, &o->planner_client, o->state == 2);
            }
        };
        
        using CByDefault = decltype(ExprCast<bool>(Config::e(HomingSpec::ByDefault::i())));
        using CEndInvert = decltype(ExprCast<bool>(Config::e(HomingSpec::EndInvert::i())));
        using CHomeDir = decltype(ExprCast<bool>(Config::e(HomingSpec::HomeDir::i())));
        using CFastExtraDist = decltype(ExprCast<FpType>(Config::e(HomingSpec::FastExtraDist::i())));
        using CRetractDist = decltype(ExprCast<FpType>(Config::e(HomingSpec::RetractDist::i())));
        using CSlowExtraDist = decltype(ExprCast<FpType>(Config::e(HomingSpec::SlowExtraDist::i())));
        using CFastSpeed = decltype(ExprCast<FpType>(Config::e(HomingSpec::FastSpeed::i())));
        using CRetractSpeed = decltype(ExprCast<FpType>(Config::e(HomingSpec::RetractSpeed::i())));
        using CSlowSpeed = decltype(ExprCast<FpType>(Config::e(HomingSpec::SlowSpeed::i())));
        
        using ConfigExprs = MakeTypeList<CByDefault, CEndInvert, CHomeDir, CFastExtraDist, CRetractDist, CSlowExtraDist, CFastSpeed, CRetractSpeed, CSlowSpeed>;
        
        struct Object : public ObjBase<VirtHomingAxis, typename VirtualHomingModule::Object, EmptyTypeList> {
            VirtHomingPlannerClient planner_client;
            uint8_t state;
            bool command_sent;
        };
    };
    using VirtHomingAxisList = IndexElemList<VirtHomingAxisParamsList, VirtHomingAxis>;
    
public:
    struct Object : public ObjBase<VirtualHomingModule, ParentObject, VirtHomingAxisList> {
        typename Context::EventLoop::QueuedEvent event;
        State state;
        bool homing_error;
        PhysVirtAxisMaskType homing_axes;
        TheCommand *err_output;
    };
};

APRINTER_ALIAS_STRUCT(VirtualHomingModuleAxisParams, (
    APRINTER_AS_VALUE(char, AxisName),
    APRINTER_AS_TYPE(ByDefault),
    APRINTER_AS_TYPE(EndPin),
    APRINTER_AS_TYPE(EndPinInputMode),
    APRINTER_AS_TYPE(EndInvert),
    APRINTER_AS_TYPE(HomeDir),
    APRINTER_AS_TYPE(FastExtraDist),
    APRINTER_AS_TYPE(RetractDist),
    APRINTER_AS_TYPE(SlowExtraDist),
    APRINTER_AS_TYPE(FastSpeed),
    APRINTER_AS_TYPE(RetractSpeed),
    APRINTER_AS_TYPE(SlowSpeed)
))

APRINTER_ALIAS_STRUCT_EXT(VirtualHomingModuleService, (
    APRINTER_AS_TYPE(VirtHomingAxisParamsList)
), (
    APRINTER_MODULE_TEMPLATE(VirtualHomingModuleService, VirtualHomingModule)
    
    using ProvidedServices = MakeTypeList<
        ServiceDefinition<ServiceList::VirtualHomingHookService, 0>
    >;
))

#include <aprinter/EndNamespace.h>

#endif
