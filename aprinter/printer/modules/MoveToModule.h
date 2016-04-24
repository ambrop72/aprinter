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

#ifndef APRINTER_MOVE_TO_MODULE_H
#define APRINTER_MOVE_TO_MODULE_H

#include <stdint.h>

#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/FuncUtils.h>
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Assert.h>
#include <aprinter/printer/Configuration.h>
#include <aprinter/printer/ServiceList.h>
#include <aprinter/printer/utils/ModuleUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename ModuleArg>
class MoveToModule {
    APRINTER_UNPACK_MODULE_ARG(ModuleArg)
    
public:
    struct Object;
    
private:
    using FpType = typename ThePrinterMain::FpType;
    using Config = typename ThePrinterMain::Config;
    using TheCommand = typename ThePrinterMain::TheCommand;
    
    using MoveSpecList = typename Params::MoveSpecList;
    static int const NumMoves = TypeListLength<MoveSpecList>::Value;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->move_index = -1;
    }
    
    template <int MoveIndex>
    static bool startHook (Context c, WrapInt<MoveIndex>, TheCommand *err_output)
    {
        if (!APRINTER_CFG(Config, typename MoveHelper<MoveIndex>::CEnabled, c)) {
            return true;
        }
        start_for_move(c, MoveIndex, err_output);
        return false;
    }
    
private:
    static void start_for_move (Context c, int8_t move_index, TheCommand *err_output)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->move_index == -1)
        AMBRO_ASSERT(move_index >= 0 && move_index < NumMoves)
        
        o->err_output = err_output;
        o->move_index = move_index;
        o->command_sent = false;
        o->move_error = false;
        ThePrinterMain::custom_planner_init(c, &o->planner_client, false);
    }
    
    static void move_end_callback (Context c, bool error)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->move_index >= 0 && o->move_index < NumMoves)
        
        if (error) {
            o->move_error = true;
        }
    }
    
    class ThePlannerClient : public ThePrinterMain::PlannerClient {
    private:
        void pull_handler (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->move_index >= 0 && o->move_index < NumMoves)
            
            if (o->command_sent) {
                return ThePrinterMain::custom_planner_wait_finished(c);
            }
            o->command_sent = true;
            ThePrinterMain::move_begin(c);
            FpType speed;
            ListForOne<MoveHelperList>(o->move_index, [&] APRINTER_TL(helper, helper::fill_move_command(c, &speed)));
            ThePrinterMain::move_set_max_speed(c, speed);
            return ThePrinterMain::move_end(c, o->err_output, MoveToModule::move_end_callback);
        }
        
        void finished_handler (Context c, bool aborted)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->move_index >= 0 && o->move_index < NumMoves)
            
            ThePrinterMain::custom_planner_deinit(c);
            int8_t move_index = o->move_index;
            o->move_index = -1;
            return ListForOne<MoveHelperList>(move_index, [&] APRINTER_TL(helper, helper::hook_completed(c, o->move_error)));
        }
    };
    
    template <int MoveIndex>
    struct MoveHelper {
        struct Object;
        using TheMoveSpec = TypeListGet<MoveSpecList, MoveIndex>;
        using CoordSpecList = typename TheMoveSpec::CoordSpecList;
        
        using CSpeed = decltype(ExprCast<FpType>(Config::e(TheMoveSpec::Speed::i())));
        using CEnabled = decltype(ExprCast<bool>(Config::e(TheMoveSpec::Enabled::i())));
        using ConfigExprs = MakeTypeList<CSpeed, CEnabled>;
        
        static void fill_move_command (Context c, FpType *speed)
        {
            *speed = APRINTER_CFG(Config, CSpeed, c);
            ListFor<CoordHelperList>([&] APRINTER_TL(helper, helper::fill_coordinate(c)));
        }
        
        static void hook_completed (Context c, bool error)
        {
            return ThePrinterMain::template hookCompletedByProvider<typename TheMoveSpec::HookType>(c, error);
        }
        
        template <int CoordIndex>
        struct CoordHelper {
            using TheCoord = TypeListGet<CoordSpecList, CoordIndex>;
            static int const AxisIndex = ThePrinterMain::template FindPhysVirtAxis<TheCoord::AxisName>::Value;
            
            using CValue = decltype(ExprCast<FpType>(Config::e(TheCoord::Value::i())));
            using ConfigExprs = MakeTypeList<CValue>;
            
            static void fill_coordinate (Context c)
            {
                ThePrinterMain::template move_add_axis<AxisIndex>(c, APRINTER_CFG(Config, CValue, c));
            }
            
            struct Object : public ObjBase<CoordHelper, typename MoveHelper::Object, EmptyTypeList> {};
        };
        using CoordHelperList = IndexElemList<CoordSpecList, CoordHelper>;
        
        struct Object : public ObjBase<MoveHelper, typename MoveToModule::Object, CoordHelperList> {};
    };
    using MoveHelperList = IndexElemList<MoveSpecList, MoveHelper>;
    
public:
    struct Object : public ObjBase<MoveToModule, ParentObject, MoveHelperList> {
        ThePlannerClient planner_client;
        TheCommand *err_output;
        int8_t move_index;
        bool command_sent;
        bool move_error;
    };
};

APRINTER_ALIAS_STRUCT(MoveCoordSpec, (
    APRINTER_AS_VALUE(char, AxisName),
    APRINTER_AS_TYPE(Value)
))

APRINTER_ALIAS_STRUCT(MoveSpec, (
    APRINTER_AS_TYPE(HookType),
    APRINTER_AS_VALUE(int8_t, HookPriority),
    APRINTER_AS_TYPE(Enabled),
    APRINTER_AS_TYPE(Speed),
    APRINTER_AS_TYPE(CoordSpecList)
))

APRINTER_ALIAS_STRUCT_EXT(MoveToModuleService, (
    APRINTER_AS_TYPE(MoveSpecList)
), (
    APRINTER_MODULE_TEMPLATE(MoveToModuleService, MoveToModule)
    
    template <typename IndexAndMoveSpec>
    using MakeMoveHookService = ServiceDefinition<typename IndexAndMoveSpec::Value::HookType, IndexAndMoveSpec::Value::HookPriority, typename IndexAndMoveSpec::Key>;
    
    using ProvidedServices = MapTypeList<TypeListEnumerate<MoveSpecList>, TemplateFunc<MakeMoveHookService>>;
))

#include <aprinter/EndNamespace.h>

#endif
