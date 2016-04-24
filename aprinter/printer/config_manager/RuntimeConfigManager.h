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
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>

#include <aprinter/meta/Expr.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/FuncUtils.h>
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/ConstexprHash.h>
#include <aprinter/meta/ConstexprCrc32.h>
#include <aprinter/meta/ConstexprString.h>
#include <aprinter/meta/StaticArray.h>
#include <aprinter/meta/MemberType.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/LoopUtils.h>
#include <aprinter/misc/StringTools.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/printer/Configuration.h>
#include <aprinter/printer/utils/JsonBuilder.h>

#include <aprinter/BeginNamespace.h>

static bool RuntimeConfigManager__compare_option (char const *name, ProgPtr<char> optname)
{
    while (1) {
        char c = AsciiToLower(*name);
        char d = AsciiToLower(*optname);
        if (c != d) {
            return false;
        }
        if (c == '\0') {
            return true;
        }
        ++name;
        ++optname;
    }
}

struct RuntimeConfigManagerNoStoreService {};

template <typename Arg>
class RuntimeConfigManager {
    using Context           = typename Arg::Context;
    using ParentObject      = typename Arg::ParentObject;
    using ConfigOptionsList = typename Arg::ConfigOptionsList;
    using ThePrinterMain    = typename Arg::ThePrinterMain;
    using Handler           = typename Arg::Handler;
    using Params            = typename Arg::Params;
    
public:
    struct Object;
    
private:
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_Type, Type)
    
    template <typename TheOption>
    using OptionIsNotConstant = WrapBool<(!TypeListFind<typename TheOption::Properties, ConfigPropertyConstant>::Found)>;
    using StoreService = typename Params::StoreService;
    using FormatHasher = ConstexprHash<ConstexprCrc32>;
    
    static int const DumpConfigMCommand = 503;
    static int const GetConfigMCommand = 925;
    static int const SetConfigMCommand = 926;
    static int const ResetAllConfigMCommand = 502;
    static int const LoadConfigMCommand = 501;
    static int const SaveConfigMCommand = 500;
    
    static int const MaxDumpLineLen = 60;
    
public:
    using RuntimeConfigOptionsList = FilterTypeList<ConfigOptionsList, TemplateFunc<OptionIsNotConstant>>;
    static int const NumRuntimeOptions = TypeListLength<RuntimeConfigOptionsList>::Value;
    static bool const HasStore = !TypesAreEqual<StoreService, RuntimeConfigManagerNoStoreService>::Value;
    enum class OperationType {LOAD, STORE};
    
private:
    using TypesList = TypeListRemoveDuplicates<MapTypeList<RuntimeConfigOptionsList, GetMemberType_Type>>;
    
    template <typename Type>
    using GetTypeIndex = TypeListIndex<TypesList, Type>;
    
    template <typename This=RuntimeConfigManager>
    using TheCommand = typename This::ThePrinterMain::TheCommand;
    
    template <typename TheType, typename Dummy=void>
    struct TypeSpecific;
    
    template <typename Dummy>
    struct TypeSpecific<double, Dummy> {
        static size_t const MaxStringValueLength = 30;
        static constexpr char const * TypeName() { return "double"; }
        
        static void get_value_cmd (Context c, TheCommand<> *cmd, double value)
        {
            cmd->reply_append_fp(c, value);
        }
        
        static void set_value_cmd (Context c, TheCommand<> *cmd, double *value, double default_value)
        {
            *value = cmd->get_command_param_fp(c, 'V', default_value);
        }
        
        static void get_value_str (double value, char *out_str)
        {
            AMBRO_PGM_SPRINTF(out_str, AMBRO_PSTR("%.17g"), value);
        }
        
        static void set_value_str (double *value, char const *in_str)
        {
            *value = StrToFloat<double>(in_str, nullptr);
        }
    };
    
    template <typename Dummy>
    struct TypeSpecific<bool, Dummy> {
        static size_t const MaxStringValueLength = 1;
        static constexpr char const * TypeName() { return "bool"; }
        
        static void get_value_cmd (Context c, TheCommand<> *cmd, bool value)
        {
            cmd->reply_append_uint32(c, value);
        }
        
        static void set_value_cmd (Context c, TheCommand<> *cmd, bool *value, bool default_value)
        {
            *value = cmd->get_command_param_uint32(c, 'V', default_value);
        }
        
        static void get_value_str (bool value, char *out_str)
        {
            strcpy(out_str, value ? "1" : "0");
        }
        
        static void set_value_str (bool *value, char const *in_str)
        {
            *value = (strcmp(in_str, "0") != 0);
        }
    };
    
    template <typename TypeSpec>
    struct GenericTypeSpecific {
    private:
        using ConfigType = typename TypeSpec::ConfigType;
        
    public:
        static size_t const MaxStringValueLength = TypeSpec::MaxStringValueLength;
        static constexpr char const * TypeName() { return TypeSpec::TypeName(); }
        
        static void get_value_cmd (Context c, TheCommand<> *cmd, ConfigType value)
        {
            char str[MaxStringValueLength+1];
            TypeSpec::print_value(value, str);
            cmd->reply_append_str(c, str);
        }
        
        static void set_value_cmd (Context c, TheCommand<> *cmd, ConfigType *value, ConfigType default_value)
        {
            char const *str = cmd->get_command_param_str(c, 'V', nullptr);
            if (!str || !TypeSpec::parse_value(str, value)) {
                *value = default_value;
            }
        }
        
        static void get_value_str (ConfigType value, char *out_str)
        {
            TypeSpec::print_value(value, out_str);
        }
        
        static void set_value_str (ConfigType *value, char const *in_str)
        {
            if (!TypeSpec::parse_value(in_str, value)) {
                *value = ConfigType();
            }
        }
    };
    
    struct MacAddressTypeSpec {
        using ConfigType = ConfigTypeMacAddress;
        static size_t const MaxStringValueLength = 17;
        static constexpr char const * TypeName() { return "mac_addr"; }
        
        static void print_value (ConfigTypeMacAddress value, char *out_str)
        {
            sprintf(out_str, "%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8,
                    value.mac_addr[0], value.mac_addr[1], value.mac_addr[2],
                    value.mac_addr[3], value.mac_addr[4], value.mac_addr[5]);
        }
        
        static bool parse_value (char const *str, ConfigTypeMacAddress *out_value)
        {
            for (auto i : LoopRange<size_t>(ConfigTypeMacAddress::Size)) {
                if (*str == '\0') {
                    return false;
                }
                
                char *end;
                long int val = strtol(str, &end, 16);
                
                if (!(*end == ':' || *end == '\0')) {
                    return false;
                }
                
                if (!(val >= 0 && val <= 255)) {
                    return false;
                }
                
                out_value->mac_addr[i] = val;
                
                str = (*end == '\0') ? end : (end + 1);
            }
            
            if (*str != '\0') {
                return false;
            }
            
            return true;
        }
    };
    template <typename Dummy>
    struct TypeSpecific<ConfigTypeMacAddress, Dummy> : public GenericTypeSpecific<MacAddressTypeSpec> {};
    
    struct IpAddressTypeSpec {
        using ConfigType = ConfigTypeIpAddress;
        static size_t const MaxStringValueLength = 15;
        static constexpr char const * TypeName() { return "ip_addr"; }
        
        static void print_value (ConfigTypeIpAddress value, char *out_str)
        {
            sprintf(out_str, "%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8,
                    value.ip_addr[0], value.ip_addr[1], value.ip_addr[2], value.ip_addr[3]);
        }
        
        static bool parse_value (char const *str, ConfigTypeIpAddress *out_value)
        {
            for (auto i : LoopRange<size_t>(ConfigTypeIpAddress::Size)) {
                if (*str == '\0') {
                    return false;
                }
                
                char *end;
                long int val = strtol(str, &end, 10);
                
                if (!(*end == '.' || *end == '\0')) {
                    return false;
                }
                
                if (!(val >= 0 && val <= 255)) {
                    return false;
                }
                
                out_value->ip_addr[i] = val;
                
                str = (*end == '\0') ? end : (end + 1);
            }
            
            if (*str != '\0') {
                return false;
            }
            
            return true;
        }
    };
    template <typename Dummy>
    struct TypeSpecific<ConfigTypeIpAddress, Dummy> : public GenericTypeSpecific<IpAddressTypeSpec> {};
    
    template <int TypeIndex, typename Dummy=void>
    struct TypeGeneral {
        using Type = TypeListGet<TypesList, TypeIndex>;
        using TheTypeSpecific = TypeSpecific<Type>;
        using OptionsList = FilterTypeList<RuntimeConfigOptionsList, ComposeFunctions<IsEqualFunc<Type>, GetMemberType_Type>>;
        using PrevTypeGeneral = TypeGeneral<(TypeIndex - 1)>;
        static int const NumOptions = TypeListLength<OptionsList>::Value;
        static int const OptionCounter = PrevTypeGeneral::OptionCounter + NumOptions;
        
        template <typename Option>
        using OptionIndex = TypeListIndex<OptionsList, Option>;
        
        template <int OptionIndex>
        struct NameTableElem {
            using TheConfigOption = TypeListGet<OptionsList, OptionIndex>;
            static constexpr ProgPtr<char> value () { return ProgPtr<char>::Make(TheConfigOption::name()); }
        };
        
        template <int OptionIndex>
        struct DefaultTableElem {
            using TheConfigOption = TypeListGet<OptionsList, OptionIndex>;
            static constexpr Type value () { return TheConfigOption::DefaultValue::value(); }
        };
        
        using NameTable = StaticArray<ProgPtr<char>, NumOptions, NameTableElem>;
        using DefaultTable = StaticArray<Type, NumOptions, DefaultTableElem>;
        
        static int find_option (char const *name)
        {
            for (auto i : LoopRange<int>(NumOptions)) {
                if (RuntimeConfigManager__compare_option(name, NameTable::readAt(i))) {
                    return i;
                }
            }
            return -1;
        }
        
        static void reset_config (Context c)
        {
            auto *o = Object::self(c);
            
            for (auto i : LoopRange<int>(NumOptions)) {
                o->values[i] = DefaultTable::readAt(i);
            }
        }
        
        template <typename This=RuntimeConfigManager>
        static bool get_set_cmd (Context c, TheCommand<This> *cmd, bool get_it, char const *name)
        {
            auto *o = Object::self(c);
            auto *mo = RuntimeConfigManager::Object::self(c);
            
            int index = find_option(name);
            if (index < 0) {
                return true;
            }
            if (get_it) {
                TheTypeSpecific::get_value_cmd(c, cmd, o->values[index]);
            } else {
                TheTypeSpecific::set_value_cmd(c, cmd, &o->values[index], DefaultTable::readAt(index));
                mo->apply_pending = true;
            }
            return false;
        }
        
        template <typename This=RuntimeConfigManager>
        static bool dump_options_helper (Context c, TheCommand<This> *cmd, int global_option_index)
        {
            auto *o = Object::self(c);
            auto *mo = RuntimeConfigManager::Object::self(c);
            AMBRO_ASSERT(global_option_index >= PrevTypeGeneral::OptionCounter)
            
            if (global_option_index < OptionCounter) {
                int index = global_option_index - PrevTypeGeneral::OptionCounter;
                cmd->reply_append_pstr(c, NameTable::readAt(index).m_ptr);
                cmd->reply_append_pstr(c, AMBRO_PSTR(" V"));
                TheTypeSpecific::get_value_cmd(c, cmd, o->values[index]);
                return false;
            }
            return true;
        }
        
        static bool set_by_strings (Context c, char const *name, char const *set_value)
        {
            auto *o = Object::self(c);
            
            int index = find_option(name);
            if (index < 0) {
                return true;
            }
            TheTypeSpecific::set_value_str(&o->values[index], set_value);
            return false;
        }
        
        static bool get_string_helper (Context c, int global_option_index, char *output, size_t output_avail)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(global_option_index >= PrevTypeGeneral::OptionCounter)
            
            if (global_option_index < OptionCounter) {
                int index = global_option_index - PrevTypeGeneral::OptionCounter;
                size_t name_length = AMBRO_PGM_STRLEN(NameTable::readAt(index).m_ptr);
                if (output_avail < name_length + 1 + TheTypeSpecific::MaxStringValueLength + 1) {
                    *output = '\0';
                } else {
                    AMBRO_PGM_MEMCPY(output, NameTable::readAt(index).m_ptr, name_length);
                    output += name_length;
                    *output++ = '=';
                    TheTypeSpecific::get_value_str(o->values[index], output);
                }
                return false;
            }
            return true;
        }
        
        static bool get_type_helper (Context c, int global_option_index, char const **option_type)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(global_option_index >= PrevTypeGeneral::OptionCounter)
            
            if (global_option_index < OptionCounter) {
                *option_type = TheTypeSpecific::TypeName();
                return false;
            }
            return true;
        }
        
        struct Object : public ObjBase<TypeGeneral, typename RuntimeConfigManager::Object, EmptyTypeList> {
            Type values[NumOptions];
        };
    };
    
    template <typename Dummy>
    struct TypeGeneral<(-1), Dummy> {
        static int const OptionCounter = 0;
    };
    
    using TypeGeneralList = IndexElemList<TypesList, DedummyIndexTemplate<TypeGeneral>::template Result>;
    
    template <typename Option>
    struct OptionHelper {
        using Type = typename Option::Type;
        using TheTypeGeneral = TypeGeneral<GetTypeIndex<Type>::Value>;
        static int const GeneralIndex = TheTypeGeneral::template OptionIndex<Option>::Value;
        
        static Type * value (Context c)
        {
            return &TheTypeGeneral::Object::self(c)->values[GeneralIndex];
        }
        
        static Type call (Context c)
        {
            return *value(c);
        }
    };
    
    struct HashInitial {
        static constexpr FormatHasher CurrentHash = FormatHasher();
    };
    
    template <typename Option, typename PrevHash>
    struct HashStep {
        static constexpr FormatHasher CurrentHash = PrevHash::CurrentHash.addUint32(GetTypeIndex<typename Option::Type>::Value).addString(Option::name(), ConstexprStrlen(Option::name()));
    };
    
    using HashFinal = TypeListFold<RuntimeConfigOptionsList, HashInitial, HashStep>;
    
    template <typename Option>
    using OptionExprRuntime = VariableExpr<typename Option::Type, OptionHelper<Option>>;
    
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
        APRINTER_MAKE_INSTANCE(TheStore, (StoreService::template Store<Context, Object, RuntimeConfigManager, ThePrinterMain, StoreHandler>))
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
        
        template <typename This=RuntimeConfigManager>
        static bool checkCommand (Context c, TheCommand<This> *cmd)
        {
            auto *o = Object::self(c);
            
            auto cmd_num = cmd->getCmdNumber(c);
            if (cmd_num == LoadConfigMCommand || cmd_num == SaveConfigMCommand) {
                if (!cmd->tryLockedCommand(c)) {
                    return false;
                }
                OperationType type = (cmd_num == LoadConfigMCommand) ? OperationType::LOAD : OperationType::STORE;
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
                auto *cmd = ThePrinterMain::get_locked(c);
                if (!success) {
                    cmd->reportError(c, AMBRO_PSTR("Store"));
                }
                cmd->finishCommand(c);
            } else {
                Handler::call(c, success);
            }
        }
        struct StoreHandler : public AMBRO_WFUNC_TD(&StoreFeature::store_handler) {};
        
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
        template <typename This=RuntimeConfigManager>
        static bool checkCommand (Context c, TheCommand<This> *cmd) { return true; }
    };
    
    static void reset_all_config (Context c)
    {
        auto *o = Object::self(c);
        
        ListFor<TypeGeneralList>([&] APRINTER_TL(type, type::reset_config(c)));
        o->apply_pending = true;
    }
    
    static void work_dump (Context c)
    {
        auto *o = Object::self(c);
        
        TheCommand<> *cmd = ThePrinterMain::get_locked(c);
        if (o->dump_current_option == NumRuntimeOptions) {
            goto finish;
        }
        if (!cmd->requestSendBufEvent(c, MaxDumpLineLen, RuntimeConfigManager::send_buf_event_handler)) {
            cmd->reportError(c, AMBRO_PSTR("Dump"));
            goto finish;
        }
        return;
    finish:
        cmd->finishCommand(c);
    }
    
    static void send_buf_event_handler (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->dump_current_option < NumRuntimeOptions)
        
        TheCommand<> *cmd = ThePrinterMain::get_locked(c);
        cmd->reply_append_pstr(c, AMBRO_PSTR("M926 I"));
        ListForBreak<TypeGeneralList>([&] APRINTER_TL(type, return type::dump_options_helper(c, cmd, o->dump_current_option)));
        cmd->reply_append_ch(c, '\n');
        cmd->reply_poke(c);
        o->dump_current_option++;
        work_dump(c);
    }
    
public:
    static constexpr uint32_t FormatHash = HashFinal::CurrentHash.end();
    
    static void init (Context c)
    {
        reset_all_config(c);
        StoreFeature::init(c);
    }
    
    static void deinit (Context c)
    {
        StoreFeature::deinit(c);
    }
    
    template <typename This=RuntimeConfigManager>
    static bool checkCommand (Context c, TheCommand<This> *cmd)
    {
        auto *o = Object::self(c);
        
        auto cmd_num = cmd->getCmdNumber(c);
        if (cmd_num == GetConfigMCommand || cmd_num == SetConfigMCommand || cmd_num == ResetAllConfigMCommand) {
            if (cmd_num == ResetAllConfigMCommand) {
                reset_all_config(c);
            } else {
                bool get_it = (cmd_num == GetConfigMCommand);
                char const *name = cmd->get_command_param_str(c, 'I', "");
                if (ListForBreak<TypeGeneralList>([&] APRINTER_TL(type, return type::get_set_cmd(c, cmd, get_it, name)))) {
                    cmd->reportError(c, AMBRO_PSTR("UnknownOption"));
                } else if (get_it) {
                    cmd->reply_append_ch(c, '\n');
                }
            }
            cmd->finishCommand(c);
            return false;
        }
        if (cmd_num == DumpConfigMCommand) {
            if (!cmd->tryLockedCommand(c)) {
                return false;
            }
            o->dump_current_option = 0;
            work_dump(c);
            return false;
        }
        return StoreFeature::checkCommand(c, cmd);
    }
    
    template <typename Option>
    static void setOptionValue (Context c, Option, typename Option::Type value)
    {
        auto *o = Object::self(c);
        static_assert(OptionIsNotConstant<Option>::Value, "");
        
        *OptionHelper<Option>::value(c) = value;
        o->apply_pending = true;
    }
    
    template <typename Option>
    static typename Option::Type getOptionValue (Context c, Option)
    {
        static_assert(OptionIsNotConstant<Option>::Value, "");
        
        return *OptionHelper<Option>::value(c);
    }
    
    static bool setOptionByStrings (Context c, char const *option_name, char const *option_value)
    {
        auto *o = Object::self(c);
        
        bool res = !ListForBreak<TypeGeneralList>([&] APRINTER_TL(type, return type::set_by_strings(c, option_name, option_value)));
        if (res) {
            o->apply_pending = true;
        }
        return res;
    }
    
    static void getOptionString (Context c, int option_index, char *output, size_t output_avail)
    {
        AMBRO_ASSERT(option_index >= 0)
        AMBRO_ASSERT(option_index < NumRuntimeOptions)
        AMBRO_ASSERT(output_avail > 0)
        
        ListForBreak<TypeGeneralList>([&] APRINTER_TL(type, return type::get_string_helper(c, option_index, output, output_avail)));
    }
    
    static void getOptionType (Context c, int option_index, char const **option_type)
    {
        AMBRO_ASSERT(option_index >= 0)
        AMBRO_ASSERT(option_index < NumRuntimeOptions)
        
        ListForBreak<TypeGeneralList>([&] APRINTER_TL(type, return type::get_type_helper(c, option_index, option_type)));
    }
    
    template <typename TheStoreFeature = StoreFeature>
    static void startOperation (Context c, OperationType type)
    {
        return TheStoreFeature::start_operation(c, type, false);
    }
    
    static void clearApplyPending (Context c)
    {
        auto *o = Object::self(c);
        o->apply_pending = false;
    }
    
    template <typename TheJsonBuilder>
    static void get_json_status (Context c, TheJsonBuilder *json)
    {
        auto *o = Object::self(c);
        json->addSafeKeyVal("configDirty", JsonBool{o->apply_pending});
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
    >> {
        int dump_current_option;
        bool apply_pending;
    };
};

APRINTER_ALIAS_STRUCT_EXT(RuntimeConfigManagerService, (
    APRINTER_AS_TYPE(StoreService)
), (
    APRINTER_ALIAS_STRUCT_EXT(ConfigManager, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(ConfigOptionsList),
        APRINTER_AS_TYPE(ThePrinterMain),
        APRINTER_AS_TYPE(Handler)
    ), (
        using Params = RuntimeConfigManagerService;
        APRINTER_DEF_INSTANCE(ConfigManager, RuntimeConfigManager)
    ))
))

#include <aprinter/EndNamespace.h>

#endif
