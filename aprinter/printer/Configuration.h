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
#include <aprinter/base/Object.h>
#include <aprinter/meta/IndexElemList.h>
#include <aprinter/meta/FuncUtils.h>
#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/If.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/TypeList.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/ProgramMemory.h>

#define APRINTER_CONFIG_START \
APRINTER_START_LIST(ConfigList)

#define APRINTER_CONFIG_OPTION_GENERIC(Name, Type, DefaultValue, Properties) \
constexpr char AMBRO_PROGMEM const Name##__OptionName[] = #Name; \
struct Name : public APrinter::ConfigOption<Name, Type, DefaultValue, Name##__OptionName, Properties> {}; \
APRINTER_ADD_TO_LIST(ConfigList, Name)

#define APRINTER_CONFIG_OPTION_SIMPLE(Name, Type, DefaultValue, Properties) \
using Name##__DefaultValue = APrinter::WrapValue<Type, (DefaultValue)>; \
APRINTER_CONFIG_OPTION_GENERIC(Name, Type, Name##__DefaultValue, Properties)

#define APRINTER_CONFIG_OPTION_BOOL(Name, DefaultValue, Properties) \
APRINTER_CONFIG_OPTION_SIMPLE(Name, bool, DefaultValue, Properties)

#define APRINTER_CONFIG_OPTION_DOUBLE(Name, DefaultValue, Properties) \
using Name##__DefaultValue = AMBRO_WRAP_DOUBLE(DefaultValue); \
APRINTER_CONFIG_OPTION_GENERIC(Name, double, Name##__DefaultValue, Properties)

#define APRINTER_CONFIG_END \
APRINTER_END_LIST(ConfigList)

#define APRINTER_CFG(TheConfig, TheExpr, c) ( \
    decltype(TheConfig::getExpr(TheExpr()))::IsConstexpr ? \
    decltype(TheConfig::getHelper(TheExpr()))::value() : \
    decltype(TheConfig::getHelper(TheExpr()))::eval(c) \
)

#include <aprinter/BeginNamespace.h>

template <typename TIdentity, typename TType, typename TDefaultValue, char AMBRO_PROGMEM const *TOptionName, typename TProperties>
struct ConfigOption {
    using Identity = TIdentity;
    using Type = TType;
    using DefaultValue = TDefaultValue;
    using Properties = TProperties;
    
    static constexpr Identity i ();
    static constexpr char AMBRO_PROGMEM const * name () { return TOptionName; }
};

using ConfigNoProperties = EmptyTypeList;

template <typename... Properties>
using ConfigProperties = MakeTypeList<Properties...>;

struct ConfigPropertyConstant {};

template <typename Context, typename ParentObject, typename DelayedExprsList>
class ConfigCache {
public:
    struct Object;
    
private:
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_update, update)
    
    using TheDebugObject = DebugObject<Context, Object>;
    using MyExprsList = typename DelayedExprsList::List;
    template <typename TheExpr>
    using ExprIsConstexpr = WrapBool<TheExpr::IsConstexpr>;
    using ExprIsConstexprFunc = TemplateFunc<ExprIsConstexpr>;
    using ConstexprExprsList = FilterTypeList<MyExprsList, ExprIsConstexprFunc>;
    using CachedExprsList = TypeListRemoveDuplicates<FilterTypeList<MyExprsList, ComposeFunctions<NotFunc, ExprIsConstexprFunc>>>;
    
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
        static_assert(TypeListFind<MyExprsList, TheExpr>::Found, "Expression is not in cache.");
        using Expr = TheExpr;
    };
    
    template <typename TheExpr>
    using GetExprVariableHelper = VariableExpr<
        typename TheExpr::Type,
        CachedExprState<TypeListIndex<CachedExprsList, TheExpr>::Value>
    >;
    
    template <typename TheExpr>
    using GetExpr = FuncCall<
        IfFunc<
            ConstantFunc<WrapBool<CheckExpr<TheExpr>::Expr::IsConstexpr>>,
            ConstantFunc<TheExpr>,
            TemplateFunc<GetExprVariableHelper>
        >,
        TheExpr
    >;
    
    template <bool IsConstexpr, typename TheGetExpr>
    struct GetHelper;
    
    template <typename TheGetExpr>
    struct GetHelper<true, TheGetExpr> {
        static constexpr typename TheGetExpr::Type value ()
        {
            return TheGetExpr::value();
        }
        
        static typename TheGetExpr::Type eval (Context c);
    };
    
    template <typename TheGetExpr>
    struct GetHelper<false, TheGetExpr> {
        static typename TheGetExpr::Type value ();
        
        static typename TheGetExpr::Type eval (Context c)
        {
            return TheGetExpr::eval(c);
        }
    };
    
public:
    static void init (Context c)
    {
        ListForEachForward<CachedExprStateList>(Foreach_update(), c);
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
    }
    
    static void update (Context c)
    {
        TheDebugObject::access(c);
        
        ListForEachForward<CachedExprStateList>(Foreach_update(), c);
    }
    
    template <typename TheExpr>
    static GetExpr<TheExpr> getExpr (TheExpr);
    
    template <typename TheExpr>
    static GetHelper<GetExpr<TheExpr>::IsConstexpr, GetExpr<TheExpr>> getHelper (TheExpr);
    
public:
    struct Object : public ObjBase<ConfigCache, ParentObject, JoinTypeLists<
        CachedExprStateList,
        MakeTypeList<TheDebugObject>
    >> {};
};

template <typename TheConfigManager, typename TheConfigCache>
struct ConfigFramework {
    template <typename Option>
    static decltype(TheConfigManager::e(Option())) e (Option);
    
    template <typename TheExpr, typename TheConfigCacheLazy = TheConfigCache>
    static decltype(TheConfigCacheLazy::getExpr(TheExpr())) getExpr (TheExpr);
    
    template <typename TheExpr, typename TheConfigCacheLazy = TheConfigCache>
    static decltype(TheConfigCacheLazy::getHelper(TheExpr())) getHelper (TheExpr);
};

#include <aprinter/EndNamespace.h>

#endif
