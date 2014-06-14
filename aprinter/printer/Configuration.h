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

#ifndef APRINTER_CONFIGURATION_H
#define APRINTER_CONFIGURATION_H

#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/WrapDouble.h>
#include <aprinter/meta/TypeListBuilder.h>
#include <aprinter/meta/Expr.h>
#include <aprinter/meta/Object.h>

#define APRINTER_CONFIG_START \
template <typename Option> struct ConfigOptionExprHelper; \
APRINTER_START_LIST(ConfigList)

#define APRINTER_CONFIG_OPTION_GENERIC(Name, Type, DefaultValue) \
struct Name##__Option : public APrinter::ConfigOption<Type, DefaultValue> {}; \
using Name = ConfigOptionExprHelper<Name##__Option>; \
APRINTER_ADD_TO_LIST(ConfigList, Name##__Option)

#define APRINTER_CONFIG_OPTION_SIMPLE(Name, Type, DefaultValue) \
using Name##__DefaultValue = APrinter::WrapValue<Type, (DefaultValue)>; \
APRINTER_CONFIG_OPTION_GENERIC(Name, Type, Name##__DefaultValue)

#define APRINTER_CONFIG_OPTION_DOUBLE(Name, DefaultValue) \
using Name##__DefaultValue = AMBRO_WRAP_DOUBLE(DefaultValue); \
APRINTER_CONFIG_OPTION_GENERIC(Name, double, Name##__DefaultValue)

#define APRINTER_CONFIG_END \
APRINTER_END_LIST(ConfigList)

#define APRINTER_CONFIG_FINALIZE(TheConfigManager) \
template <typename Option> struct ConfigOptionExprHelper : public TheConfigManager::OptionExpr<Option> {};

#include <aprinter/BeginNamespace.h>

template <typename TType, typename TDefaultValue>
struct ConfigOption {
    using Type = TType;
    using DefaultValue = TDefaultValue;
};

template <typename Context, typename ParentObject, typename ConfigList>
class ConfigManager {
public:
    struct Object;
    
    template <typename Option>
    using OptionExpr = ConstantExpr<typename Option::Type, typename Option::DefaultValue>;
    
    static void init (Context c)
    {
    }
    
    static void deinit (Context c)
    {
    }
    
    struct Object : public ObjBase<ConfigManager, ParentObject, EmptyTypeList> {};
};

#include <aprinter/EndNamespace.h>

#endif
