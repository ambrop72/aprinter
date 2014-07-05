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
#include <aprinter/base/ProgramMemory.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ConfigOptionsList, typename Params>
class RuntimeConfigManager {
public:
    struct Object;
    
private:
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_init, init)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_get_value_cmd, get_value_cmd)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_set_value_cmd, set_value_cmd)
    
    template <int ConfigOptionIndex>
    struct ConfigOptionState {
        using TheConfigOption = TypeListGet<ConfigOptionsList, ConfigOptionIndex>;
        using Type = typename TheConfigOption::Type;
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            o->value = TheConfigOption::DefaultValue::value();
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
    
    using ConfigOptionStateList = IndexElemList<ConfigOptionsList, ConfigOptionState>;
    
    template <typename Option>
    using OptionExpr = VariableExpr<typename Option::Type, ConfigOptionState<TypeListIndex<ConfigOptionsList, IsEqualFunc<Option>>::Value>>;
    
public:
    static void init (Context c)
    {
        ListForEachForward<ConfigOptionStateList>(Foreach_init(), c);
    }
    
    static void deinit (Context c)
    {
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
        return true;
    }
    
    template <typename Option>
    static OptionExpr<Option> e (Option);
    
public:
    struct Object : public ObjBase<RuntimeConfigManager, ParentObject, ConfigOptionStateList> {};
};

template <
    int TGetConfigMCommand,
    int TSetConfigMCommand
>
struct RuntimeConfigManagerService {
    static int const GetConfigMCommand = TGetConfigMCommand;
    static int const SetConfigMCommand = TSetConfigMCommand;
    
    template <typename Context, typename ParentObject, typename ConfigOptionsList>
    using ConfigManager = RuntimeConfigManager<Context, ParentObject, ConfigOptionsList, RuntimeConfigManagerService>;
};

#include <aprinter/EndNamespace.h>

#endif
