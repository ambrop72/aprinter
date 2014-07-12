/*
 * Copyright (c) 2014 Ambroz Bizjak
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

#ifndef APRINTER_RUNTIME_CONFIG_MANAGER_H
#define APRINTER_RUNTIME_CONFIG_MANAGER_H

#include <stdint.h>
#include <string.h>

#include <aprinter/meta/Expr.h>
#include <aprinter/meta/Object.h>
#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/IndexElemList.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/TypeListIndex.h>
#include <aprinter/meta/IsEqualFunc.h>
#include <aprinter/meta/WrapType.h>
#include <aprinter/meta/FilterTypeList.h>
#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/TemplateFunc.h>
#include <aprinter/meta/IfFunc.h>
#include <aprinter/meta/FuncCall.h>
#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/meta/JoinTypeLists.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Assert.h>
#include <aprinter/printer/Configuration.h>

#include <aprinter/BeginNamespace.h>

struct RuntimeConfigManagerNoStoreService {};

template <typename Context, typename ParentObject, typename ConfigOptionsList, typename ThePrinterMain, typename Params>
class RuntimeConfigManager {
public:
    struct Object;
    
private:
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_init, init)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_get_value_cmd, get_value_cmd)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_set_value_cmd, set_value_cmd)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_reset_config, reset_config)
    
    template <typename TheOption>
    using OptionIsNotConstant = WrapBool<(TypeListIndex<typename TheOption::Properties, IsEqualFunc<ConfigPropertyConstant>>::Value < 0)>;
    
    using StoreService = typename Params::StoreService;
    
public:
    using RuntimeConfigOptionsList = FilterTypeList<ConfigOptionsList, TemplateFunc<OptionIsNotConstant>>;
    
private:
    template <int ConfigOptionIndex>
    struct ConfigOptionState {
        using TheConfigOption = TypeListGet<RuntimeConfigOptionsList, ConfigOptionIndex>;
        using Type = typename TheConfigOption::Type;
        
        static void init (Context c)
        {
            reset_config(c);
        }
        
        static Type call (Context c)
        {
            auto *o = Object::self(c);
            return o->value;
        }
        
        template <typename CommandChannel>
        static bool get_value_cmd (Context c, WrapType<CommandChannel> cc, char const *name)
        {
            if (strcmp(TheConfigOption::name(), name) != 0) {
                return true;
            }
            TypeSpecific<Type>::get_value_cmd(c, cc);
            return false;
        }
        
        template <typename CommandChannel>
        static bool set_value_cmd (Context c, WrapType<CommandChannel> cc, char const *name)
        {
            if (strcmp(TheConfigOption::name(), name) != 0) {
                return true;
            }
            TypeSpecific<Type>::set_value_cmd(c, cc);
            return false;
        }
        
        static void reset_config (Context c)
        {
            auto *o = Object::self(c);
            o->value = TheConfigOption::DefaultValue::value();
        }
        
        template <typename TheType, typename Dummy = void>
        struct TypeSpecific;
        
        template <typename Dummy>
        struct TypeSpecific<double, Dummy> {
            template <typename CommandChannel>
            static void get_value_cmd (Context c, WrapType<CommandChannel>)
            {
                auto *o = Object::self(c);
                CommandChannel::reply_append_fp(c, o->value);
            }
            
            template <typename CommandChannel>
            static void set_value_cmd (Context c, WrapType<CommandChannel>)
            {
                auto *o = Object::self(c);
                o->value = CommandChannel::get_command_param_fp(c, 'V', TheConfigOption::DefaultValue::value());
            }
        };
        
        template <typename Dummy>
        struct TypeSpecific<bool, Dummy> {
            template <typename CommandChannel>
            static void get_value_cmd (Context c, WrapType<CommandChannel>)
            {
                auto *o = Object::self(c);
                CommandChannel::reply_append_uint8(c, o->value);
            }
            
            template <typename CommandChannel>
            static void set_value_cmd (Context c, WrapType<CommandChannel>)
            {
                auto *o = Object::self(c);
                o->value = CommandChannel::get_command_param_uint32(c, 'V', TheConfigOption::DefaultValue::value());
            }
        };
        
        struct Object : public ObjBase<ConfigOptionState, typename RuntimeConfigManager::Object, EmptyTypeList> {
            Type value;
        };
    };
    
    using ConfigOptionStateList = IndexElemList<RuntimeConfigOptionsList, ConfigOptionState>;
    
    template <typename Option>
    using FindOptionState = ConfigOptionState<TypeListIndex<RuntimeConfigOptionsList, IsEqualFunc<Option>>::Value>;
    
    template <typename Option>
    using OptionExprRuntime = VariableExpr<typename Option::Type, FindOptionState<Option>>;
    
    template <typename Option>
    using OptionExprConstant = ConstantExpr<typename Option::Type, typename Option::DefaultValue>;
    
    template <typename Option>
    using OptionExpr = FuncCall<
        IfFunc<
            TemplateFunc<OptionIsNotConstant>,
            TemplateFunc<OptionExprRuntime>,
            TemplateFunc<OptionExprConstant>
        >,
        Option
    >;
    
    AMBRO_STRUCT_IF(StoreFeature, (!TypesAreEqual<StoreService, RuntimeConfigManagerNoStoreService>::Value)) {
        struct StoreHandler;
        struct Object;
        using TheStore = typename StoreService::template Store<Context, Object, RuntimeConfigManager, StoreHandler>;
        enum {STATE_IDLE, STATE_LOADING, STATE_SAVING};
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            
            TheStore::init(c);
            o->state = STATE_IDLE;
        }
        
        static void deinit (Context c)
        {
            TheStore::deinit(c);
        }
        
        template <typename CommandChannel>
        static bool checkCommand (Context c, WrapType<CommandChannel>)
        {
            auto *o = Object::self(c);
            
            auto cmd_num = CommandChannel::TheGcodeParser::getCmdNumber(c);
            if (cmd_num == Params::LoadConfigMCommand || cmd_num == Params::SaveConfigMCommand) {
                if (!CommandChannel::tryLockedCommand(c)) {
                    return false;
                }
                AMBRO_ASSERT(o->state == STATE_IDLE)
                if (cmd_num == Params::LoadConfigMCommand) {
                    TheStore::startReading(c);
                    o->state = STATE_LOADING;
                } else {
                    TheStore::startWriting(c);
                    o->state = STATE_SAVING;
                }
                return false;
            }
            return true;
        }
        
        static void store_handler (Context c, bool success)
        {
            ThePrinterMain::run_for_locked(c, FinishCommandHelper(), success);
        }
        
        struct FinishCommandHelper {
            template <typename CommandChannel>
            void operator() (Context c, WrapType<CommandChannel>, bool success)
            {
                auto *o = Object::self(c);
                AMBRO_ASSERT(o->state == STATE_LOADING || o->state == STATE_SAVING)
                
                if (!success) {
                    CommandChannel::reply_append_pstr(c, AMBRO_PSTR("error:Store\n"));
                }
                o->state = STATE_IDLE;
                CommandChannel::finishCommand(c);
            }
        };
        
        struct StoreHandler : public AMBRO_WFUNC_TD(&StoreFeature::store_handler) {};
        
        struct Object : public ObjBase<StoreFeature, typename RuntimeConfigManager::Object, MakeTypeList<
            TheStore
        >> {
            uint8_t state;
        };
    } AMBRO_STRUCT_ELSE(StoreFeature) {
        struct Object {};
        static void init (Context c) {}
        static void deinit (Context c) {}
        template <typename CommandChannel>
        static bool checkCommand (Context c, WrapType<CommandChannel> cc) { return true; }
    };
    
public:
    static void init (Context c)
    {
        ListForEachForward<ConfigOptionStateList>(Foreach_init(), c);
        StoreFeature::init(c);
    }
    
    static void deinit (Context c)
    {
        StoreFeature::deinit(c);
    }
    
    template <typename CommandChannel>
    static bool checkCommand (Context c, WrapType<CommandChannel> cc)
    {
        if (CommandChannel::TheGcodeParser::getCmdNumber(c) == Params::GetConfigMCommand) {
            char const *name = CommandChannel::get_command_param_str(c, 'I', "");
            if (ListForEachForwardInterruptible<ConfigOptionStateList>(Foreach_get_value_cmd(), c, cc, name)) {
                CommandChannel::reply_append_pstr(c, AMBRO_PSTR("Error:Unknown option\n"));
            } else {
                CommandChannel::reply_append_ch(c, '\n');
            }
            CommandChannel::finishCommand(c);
            return false;
        }
        if (CommandChannel::TheGcodeParser::getCmdNumber(c) == Params::SetConfigMCommand) {
            char const *name = CommandChannel::get_command_param_str(c, 'I', "");
            if (ListForEachForwardInterruptible<ConfigOptionStateList>(Foreach_set_value_cmd(), c, cc, name)) {
                CommandChannel::reply_append_pstr(c, AMBRO_PSTR("Error:Unknown option\n"));
            }
            CommandChannel::finishCommand(c);
            return false;
        }
        if (CommandChannel::TheGcodeParser::getCmdNumber(c) == Params::ResetAllConfigMCommand) {
            ListForEachForward<ConfigOptionStateList>(Foreach_reset_config(), c);
            CommandChannel::finishCommand(c);
            return false;
        }
        return StoreFeature::checkCommand(c, cc);
    }
    
    template <typename Option>
    static void setOptionValue (Context c, Option, typename Option::Type value)
    {
        static_assert(OptionIsNotConstant<Option>::Value, "");
        
        auto *opt = FindOptionState<Option>::Object::self(c);
        opt->value = value;
    }
    
    template <typename Option>
    static typename Option::Type getOptionValue (Context c, Option)
    {
        static_assert(OptionIsNotConstant<Option>::Value, "");
        
        auto *opt = FindOptionState<Option>::Object::self(c);
        return opt->value;
    }
    
    template <typename Option>
    static OptionExpr<Option> e (Option);
    
    template <typename TheStoreFeature = StoreFeature>
    using GetStore = typename TheStoreFeature::TheStore;
    
public:
    struct Object : public ObjBase<RuntimeConfigManager, ParentObject, JoinTypeLists<
        ConfigOptionStateList,
        MakeTypeList<
            StoreFeature
        >
    >> {};
};

template <
    int TGetConfigMCommand,
    int TSetConfigMCommand,
    int TResetAllConfigMCommand,
    int TLoadConfigMCommand,
    int TSaveConfigMCommand,
    typename TStoreService
>
struct RuntimeConfigManagerService {
    static int const GetConfigMCommand = TGetConfigMCommand;
    static int const SetConfigMCommand = TSetConfigMCommand;
    static int const ResetAllConfigMCommand = TResetAllConfigMCommand;
    static int const LoadConfigMCommand = TLoadConfigMCommand;
    static int const SaveConfigMCommand = TSaveConfigMCommand;
    using StoreService = TStoreService;
    
    template <typename Context, typename ParentObject, typename ConfigOptionsList, typename ThePrinterMain>
    using ConfigManager = RuntimeConfigManager<Context, ParentObject, ConfigOptionsList, ThePrinterMain, RuntimeConfigManagerService>;
};

#include <aprinter/EndNamespace.h>

#endif
