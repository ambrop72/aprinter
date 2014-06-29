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
#include <aprinter/meta/EnableIf.h>
#include <aprinter/meta/IndexElemList.h>
#include <aprinter/meta/FilterTypeList.h>
#include <aprinter/meta/TemplateFunc.h>
#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/NotFunc.h>
#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/If.h>
#include <aprinter/meta/TypeListIndex.h>
#include <aprinter/meta/IsEqualFunc.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/ComposeFunctions.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/base/DebugObject.h>

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

template <typename Context, typename ParentObject, typename ExprsList>
class ConfigCache {
public:
    struct Object;
    
private:
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_update, update)
    
    template <typename TheExpr>
    using ExprIsConstexpr = WrapBool<TheExpr::IsConstexpr>;
    using ExprIsConstexprFunc = TemplateFunc<ExprIsConstexpr>;
    using ConstexprExprsList = FilterTypeList<ExprsList, ExprIsConstexprFunc>;
    using CachedExprsList = FilterTypeList<ExprsList, ComposeFunctions<NotFunc, ExprIsConstexprFunc>>;
    
    template <int CachedExprIndex>
    struct CachedExprState {
        using TheExpr = TypeListGet<CachedExprsList, CachedExprIndex>;
        using Type = typename TheExpr::Type;
        
        static void update (Context c)
        {
            auto *o = Object::self(c);
            o->value = TheExpr::eval(c);
        }
        
        static Type call (Context c)
        {
            auto *o = Object::self(c);
            return o->value;
        }
        
        struct Object : public ObjBase<CachedExprState, typename ConfigCache::Object, EmptyTypeList> {
            Type value;
        };
    };
    
    using CachedExprStateList = IndexElemList<CachedExprsList, CachedExprState>;
    
    template <typename TheExpr>
    struct CheckExpr {
        static_assert(TypeListIndex<ExprsList, IsEqualFunc<TheExpr>>::Value >= 0, "Expression is not in cache.");
        using Expr = TheExpr;
    };
    
    template <typename TheExpr>
    using GetExpr = If<
        CheckExpr<TheExpr>::Expr::IsConstexpr,
        TheExpr,
        VariableExpr<
            typename TheExpr::Type,
            CachedExprState<TypeListIndex<CachedExprsList, IsEqualFunc<TheExpr>>::Value>
        >
    >;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        ListForEachForward<CachedExprStateList>(Foreach_update(), c);
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->debugDeinit(c);
    }
    
    template <typename TheExpr>
    static GetExpr<TheExpr> getExpr (TheExpr);
    
    #define APRINTER_CFG(TheConfigCache, TheExpr, c) ( \
        decltype(TheConfigCache::getExpr(TheExpr()))::IsConstexpr ? \
        decltype(TheConfigCache::getExpr(TheExpr()))::value() : \
        decltype(TheConfigCache::getExpr(TheExpr()))::eval((c)) \
    )
    
public:
    struct Object : public ObjBase<ConfigCache, ParentObject, CachedExprStateList>,
        public DebugObject<Context, void>
    {};
};

#include <aprinter/EndNamespace.h>

#endif
