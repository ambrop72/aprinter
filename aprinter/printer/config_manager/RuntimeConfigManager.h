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
#include <aprinter/meta/DedummyIndexTemplate.h>
#include <aprinter/meta/ConstexprHash.h>
#include <aprinter/meta/ConstexprCrc32.h>
#include <aprinter/meta/ConstexprString.h>
#include <aprinter/meta/TypeListLength.h>
#include <aprinter/meta/StaticArray.h>
#include <aprinter/meta/GetMemberTypeFunc.h>
#include <aprinter/meta/ComposeFunctions.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Assert.h>
#include <aprinter/printer/Configuration.h>

#include <aprinter/BeginNamespace.h>

struct RuntimeConfigManagerNoStoreService {};

template <typename Context, typename ParentObject, typename ConfigOptionsList, typename ThePrinterMain, typename Handler, typename Params>
class RuntimeConfigManager {
public:
    struct Object;
    
private:
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_reset_config, reset_config)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_get_set_cmd, get_set_cmd)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_Type, Type)
    
    template <typename TheOption>
    using OptionIsNotConstant = WrapBool<(TypeListIndex<typename TheOption::Properties, IsEqualFunc<ConfigPropertyConstant>>::Value < 0)>;
    using StoreService = typename Params::StoreService;
    using FormatHasher = ConstexprHash<ConstexprCrc32>;
    using SupportedTypesList = MakeTypeList<double, bool>;
    
public:
    using RuntimeConfigOptionsList = FilterTypeList<ConfigOptionsList, TemplateFunc<OptionIsNotConstant>>;
    static int const NumRuntimeOptions = TypeListLength<RuntimeConfigOptionsList>::Value;
    static bool const HasStore = !TypesAreEqual<StoreService, RuntimeConfigManagerNoStoreService>::Value;
    enum class OperationType {LOAD, STORE};
    
    template <typename Type>
    using GetTypeNumber = WrapInt<(
        TypesAreEqual<Type, double>::Value ? 1 :
        TypesAreEqual<Type, bool>::Value ? 2 :
        -1
    )>;
    
private:
    template <typename TheType, typename Dummy=void>
    struct TypeSpecific;
    
    template <typename Dummy>
    struct TypeSpecific<double, Dummy> {
        template <typename CommandChannel>
        static void get_value_cmd (Context c, WrapType<CommandChannel>, double value)
        {
            CommandChannel::reply_append_fp(c, value);
        }
        
        template <typename CommandChannel>
        static void set_value_cmd (Context c, WrapType<CommandChannel>, double *value, double default_value)
        {
            *value = CommandChannel::get_command_param_fp(c, 'V', default_value);
        }
    };
    
    template <typename Dummy>
    struct TypeSpecific<bool, Dummy> {
        template <typename CommandChannel>
        static void get_value_cmd (Context c, WrapType<CommandChannel>, bool value)
        {
            CommandChannel::reply_append_uint8(c, value);
        }
        
        template <typename CommandChannel>
        static void set_value_cmd (Context c, WrapType<CommandChannel>, bool *value, bool default_value)
        {
            *value = CommandChannel::get_command_param_uint32(c, 'V', default_value);
        }
    };
    
    template <int TypeIndex>
    struct TypeGeneral {
        using Type = TypeListGet<SupportedTypesList, TypeIndex>;
        using TheTypeSpecific = TypeSpecific<Type>;
        using OptionsList = FilterTypeList<RuntimeConfigOptionsList, ComposeFunctions<IsEqualFunc<Type>, GetMemberType_Type>>;
        static int const NumOptions = TypeListLength<OptionsList>::Value;
        
        template <typename Option>
        using OptionIndex = TypeListIndex<OptionsList, IsEqualFunc<Option>>;
        
        template <int OptionIndex>
        struct NameTableElem {
            using TheConfigOption = TypeListGet<OptionsList, OptionIndex>;
            static constexpr char const * value () { return TheConfigOption::name(); }
        };
        
        template <int OptionIndex>
        struct DefaultTableElem {
            using TheConfigOption = TypeListGet<OptionsList, OptionIndex>;
            static constexpr Type value () { return TheConfigOption::DefaultValue::value(); }
        };
        
        using NameTable = StaticArray<char const *, NumOptions, NameTableElem>;
        using DefaultTable = StaticArray<Type, NumOptions, DefaultTableElem>;
        
        static int find_option (char const *name)
        {
            for (int i = 0; i < NumOptions; i++) {
                if (!strcmp(NameTable::data[i], name)) {
                    return i;
                }
            }
            return -1;
        }
        
        static void reset_config (Context c)
        {
            auto *o = Object::self(c);
            for (int i = 0; i < NumOptions; i++) {
                o->values[i] = DefaultTable::data[i];
            }
        }
        
        template <typename CommandChannel>
        static bool get_set_cmd (Context c, WrapType<CommandChannel> cc, bool get_it, char const *name)
        {
            auto *o = Object::self(c);
            int index = find_option(name);
            if (index < 0) {
                return true;
            }
            if (get_it) {
                TheTypeSpecific::get_value_cmd(c, cc, o->values[index]);
            } else {
                TheTypeSpecific::set_value_cmd(c, cc, &o->values[index], DefaultTable::data[index]);
            }
            return false;
        }
        
        struct Object : public ObjBase<TypeGeneral, typename RuntimeConfigManager::Object, EmptyTypeList> {
            Type values[NumOptions];
        };
    };
    
    using TypeGeneralList = IndexElemList<SupportedTypesList, TypeGeneral>;
    
    template <int ConfigOptionIndex, typename Dummy0=void>
    struct ConfigOptionState {
        using TheConfigOption = TypeListGet<RuntimeConfigOptionsList, ConfigOptionIndex>;
        using Type = typename TheConfigOption::Type;
        using PrevOption = ConfigOptionState<(ConfigOptionIndex - 1)>;
        static constexpr FormatHasher CurrentHash = PrevOption::CurrentHash.addUint32(GetTypeNumber<Type>::Value).addString(TheConfigOption::name(), ConstexprStrlen(TheConfigOption::name()));
        using TheTypeGeneral = TypeGeneral<TypeListIndex<SupportedTypesList, IsEqualFunc<Type>>::Value>;
        static int const GeneralIndex = TheTypeGeneral::template OptionIndex<TheConfigOption>::Value;
        
        static Type * value (Context c) { return &TheTypeGeneral::Object::self(c)->values[GeneralIndex]; }
        
        static Type call (Context c)
        {
            return *value(c);
        }
    };
    
    template <typename Dummy>
    struct ConfigOptionState<(-1), Dummy> {
        static constexpr FormatHasher CurrentHash = FormatHasher();
    };
    
    using ConfigOptionStateList = IndexElemList<RuntimeConfigOptionsList, DedummyIndexTemplate<ConfigOptionState>::template Result>;
    
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
    
    AMBRO_STRUCT_IF(StoreFeature, HasStore) {
        struct Object;
        struct StoreHandler;
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
                OperationType type = (cmd_num == Params::LoadConfigMCommand) ? OperationType::LOAD : OperationType::STORE;
                start_operation(c, type, true);
                return false;
            }
            return true;
        }
        
        static void start_operation (Context c, OperationType type, bool from_command)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->state == STATE_IDLE)
            
            if (type == OperationType::LOAD) {
                TheStore::startReading(c);
                o->state = STATE_LOADING;
            } else {
                TheStore::startWriting(c);
                o->state = STATE_SAVING;
            }
            o->from_command = from_command;
        }
        
        static void store_handler (Context c, bool success)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->state == STATE_LOADING || o->state == STATE_SAVING)
            
            o->state = STATE_IDLE;
            if (o->from_command) {
                ThePrinterMain::run_for_locked(c, FinishCommandHelper(), success);
            } else {
                Handler::call(c, success);
            }
        }
        struct StoreHandler : public AMBRO_WFUNC_TD(&StoreFeature::store_handler) {};
        
        struct FinishCommandHelper {
            template <typename CommandChannel>
            void operator() (Context c, WrapType<CommandChannel>, bool success)
            {
                if (!success) {
                    CommandChannel::reply_append_pstr(c, AMBRO_PSTR("error:Store\n"));
                }
                CommandChannel::finishCommand(c);
            }
        };
        
        struct Object : public ObjBase<StoreFeature, typename RuntimeConfigManager::Object, MakeTypeList<
            TheStore
        >> {
            uint8_t state;
            bool from_command;
        };
    } AMBRO_STRUCT_ELSE(StoreFeature) {
        struct Object {};
        static void init (Context c) {}
        static void deinit (Context c) {}
        template <typename CommandChannel>
        static bool checkCommand (Context c, WrapType<CommandChannel> cc) { return true; }
    };
    
    static void reset_all_config (Context c)
    {
        ListForEachForward<TypeGeneralList>(Foreach_reset_config(), c);
    }
    
public:
    static constexpr uint32_t FormatHash = ConfigOptionState<(TypeListLength<ConfigOptionStateList>::Value - 1)>::CurrentHash.end();
    
    static void init (Context c)
    {
        reset_all_config(c);
        StoreFeature::init(c);
    }
    
    static void deinit (Context c)
    {
        StoreFeature::deinit(c);
    }
    
    template <typename CommandChannel>
    static bool checkCommand (Context c, WrapType<CommandChannel> cc)
    {
        auto cmd_num = CommandChannel::TheGcodeParser::getCmdNumber(c);
        if (cmd_num == Params::GetConfigMCommand || cmd_num == Params::SetConfigMCommand || cmd_num == Params::ResetAllConfigMCommand) {
            if (cmd_num == Params::ResetAllConfigMCommand) {
                reset_all_config(c);
            } else {
                bool get_it = (cmd_num == Params::GetConfigMCommand);
                char const *name = CommandChannel::get_command_param_str(c, 'I', "");
                if (ListForEachForwardInterruptible<TypeGeneralList>(Foreach_get_set_cmd(), c, cc, get_it, name)) {
                    CommandChannel::reply_append_pstr(c, AMBRO_PSTR("Error:Unknown option\n"));
                } else if (get_it) {
                    CommandChannel::reply_append_ch(c, '\n');
                }
            }
            CommandChannel::finishCommand(c);
            return false;
        }
        return StoreFeature::checkCommand(c, cc);
    }
    
    template <typename Option>
    static void setOptionValue (Context c, Option, typename Option::Type value)
    {
        static_assert(OptionIsNotConstant<Option>::Value, "");
        
        *FindOptionState<Option>::value(c) = value;
    }
    
    template <typename Option>
    static typename Option::Type getOptionValue (Context c, Option)
    {
        static_assert(OptionIsNotConstant<Option>::Value, "");
        
        return *FindOptionState<Option>::value(c);
    }
    
    template <typename TheStoreFeature = StoreFeature>
    static void startOperation (Context c, OperationType type)
    {
        return TheStoreFeature::start_operation(c, type, false);
    }
    
    template <typename Option>
    static OptionExpr<Option> e (Option);
    
    template <typename TheStoreFeature = StoreFeature>
    using GetStore = typename TheStoreFeature::TheStore;
    
public:
    struct Object : public ObjBase<RuntimeConfigManager, ParentObject, JoinTypeLists<
        TypeGeneralList,
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
    
    template <typename Context, typename ParentObject, typename ConfigOptionsList, typename ThePrinterMain, typename Handler>
    using ConfigManager = RuntimeConfigManager<Context, ParentObject, ConfigOptionsList, ThePrinterMain, Handler, RuntimeConfigManagerService>;
};

#include <aprinter/EndNamespace.h>

#endif
