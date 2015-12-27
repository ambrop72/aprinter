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

#ifndef APRINTER_HOOK_EXECUTOR_H
#define APRINTER_HOOK_EXECUTOR_H

#include <stdint.h>

#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/MemberType.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>

#include <aprinter/BeginNamespace.h>

template <typename THookType, typename TDispatcher, typename TCompletedHandler>
struct HookDefinition {
    using HookType = THookType;
    using Dispatcher = TDispatcher;
    using CompletedHandler = TCompletedHandler;
};

template <typename Context, typename ParentObject, typename HookDefinitionList>
class HookExecutor {
public:
    struct Object;
    
private:
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_init, init)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_deinit, deinit)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_dispatchHook, dispatchHook)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_HookType, HookType)
    
public:
    static void init (Context c)
    {
        ListForEachForward<HookList>(Foreach_init(), c);
    }
    
    static void deinit (Context c)
    {
        ListForEachForward<HookList>(Foreach_deinit(), c);
    }
    
    template <typename HookType>
    static void startHook (Context c)
    {
        return GetHookForType<HookType>::startHook(c);
    }
    
    template <typename HookType>
    static bool hookIsRunning (Context c)
    {
        return GetHookForType<HookType>::hookIsRunning(c);
    }
    
    template <typename HookType>
    static void hookCompletedByProvider (Context c, bool error)
    {
        return GetHookForType<HookType>::hookCompletedByProvider(c, error);
    }
    
private:
    template <int HookIndex, typename HookProviders>
    struct Hook {
        struct Object;
        using TheHookDefinition = TypeListGet<HookDefinitionList, HookIndex>;
        using HookType = typename TheHookDefinition::HookType;
        using Dispatcher = typename TheHookDefinition::Dispatcher;
        using CompletedHandler = typename TheHookDefinition::CompletedHandler;
        static int const NumHookProviders = TypeListLength<HookProviders>::Value;
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            
            o->event.init(c, APRINTER_CB_STATFUNC_T(&Hook::event_handler));
            o->current_provider = -1;
        }
        
        static void deinit (Context c)
        {
            auto *o = Object::self(c);
            
            o->event.deinit(c);
        }
        
        static void startHook (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->current_provider == -1)
            
            o->current_provider = 0;
            o->error = false;
            o->event.prependNowNotAlready(c);
        }
        
        static void hookCompletedByProvider (Context c, bool error)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->current_provider >= 0)
            AMBRO_ASSERT(o->current_provider < NumHookProviders)
            AMBRO_ASSERT(!o->event.isSet(c))
            
            if (error) {
                o->error = true;
            }
            o->current_provider++;
            o->event.prependNowNotAlready(c);
        }
        
        static bool hookIsRunning (Context c)
        {
            auto *o = Object::self(c);
            
            return (o->current_provider >= 0);
        }
        
        template <int ProviderIndex>
        struct ProviderHelper {
            using TheProvider = TypeListGet<HookProviders, ProviderIndex>;
            
            static bool dispatchHook (Context c)
            {
                return Dispatcher::template dispatchHookToProvider<HookType, TheProvider>(c);
            }
        };
        using ProviderHelperList = IndexElemList<HookProviders, ProviderHelper>;
        
        static void event_handler (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->current_provider >= 0)
            AMBRO_ASSERT(o->current_provider <= NumHookProviders)
            
            if (!o->error) {
                while (o->current_provider < NumHookProviders) {
                    if (!ListForOne<ProviderHelperList, 0, bool>(o->current_provider, Foreach_dispatchHook(), c)) {
                        return;
                    }
                    o->current_provider++;
                }
            }
            
            o->current_provider = -1;
            return CompletedHandler::call(c, o->error);
        }
        
        struct Object : public ObjBase<Hook, typename HookExecutor::Object, EmptyTypeList> {
            typename Context::EventLoop::QueuedEvent event;
            int8_t current_provider;
            bool error;
        };
    };
    
    template <int HookIndex>
    struct Hook<HookIndex, EmptyTypeList> {
        using TheHookDefinition = TypeListGet<HookDefinitionList, HookIndex>;
        using HookType = typename TheHookDefinition::HookType;
        using CompletedHandler = typename TheHookDefinition::CompletedHandler;
        
        static void init (Context c) {}
        
        static void startHook (Context c)
        {
            return CompletedHandler::call(c, false);
        }
        
        static bool hookIsRunning (Context c)
        {
            return false;
        }
        
        struct Object {};
    };
    
    template <typename TheHookDefinition>
    using GetHookProviders = typename TheHookDefinition::Dispatcher::template GetHookProviders<typename TheHookDefinition::HookType>;
    
    template <int HookIndex>
    using HookForIndex = Hook<HookIndex, GetHookProviders<TypeListGet<HookDefinitionList, HookIndex>>>;
    
    using HookList = IndexElemList<HookDefinitionList, HookForIndex>;
    
    template <typename HookType>
    using GetHookForType = TypeListGetMapped<HookList, GetMemberType_HookType, HookType>;
    
public:
    struct Object : public ObjBase<HookExecutor, ParentObject, HookList> {};
};

#include <aprinter/EndNamespace.h>

#endif
