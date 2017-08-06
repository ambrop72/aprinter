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

#ifndef APRINTER_MOTOR_CURRENT_MODULE_H
#define APRINTER_MOTOR_CURRENT_MODULE_H

#include <aprinter/meta/FuncUtils.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/LoopUtils.h>
#include <aprinter/printer/Configuration.h>
#include <aprinter/printer/utils/ModuleUtils.h>

namespace APrinter {

template <typename ModuleArg>
class MotorCurrentModule {
    APRINTER_UNPACK_MODULE_ARG(ModuleArg)
    
public:
    struct Object;
    
private:
    using Config = typename ThePrinterMain::Config;
    using TheCommand = typename ThePrinterMain::TheCommand;
    using FpType = typename ThePrinterMain::FpType;
    
    using ParamsCurrentAxesList = typename Params::CurrentAxesList;
    template <typename ChannelAxisParams>
    using MakeCurrentChannel = typename ChannelAxisParams::Params;
    using CurrentChannelsList = MapTypeList<ParamsCurrentAxesList, TemplateFunc<MakeCurrentChannel>>;
    APRINTER_MAKE_INSTANCE(Current, (Params::CurrentService::template Current<Context, Object, Config, FpType, CurrentChannelsList>))
    
public:
    static void init (Context c)
    {
        Current::init(c);
        apply_default(c);
    }
    
    static void deinit (Context c)
    {
        Current::deinit(c);
    }
    
    static bool check_command (Context c, TheCommand *cmd)
    {
        if (cmd->getCmdNumber(c) == 906) {
            for (auto i : LoopRangeAuto(cmd->getNumParts(c))) {
                auto part = cmd->getPart(c, i);
                ListFor<CurrentAxesList>([&] APRINTER_TL(axis, axis::check_current_axis(c, cmd, cmd->getPartCode(c, part), cmd->getPartFpValue(c, part))));
            }
            cmd->finishCommand(c);
            return false;
        }
        return true;
    }
    
    static void configuration_changed (Context c)
    {
        apply_default(c);
    }
    
    using GetDriver = Current;
    
private:
    static void apply_default (Context c)
    {
        ListFor<CurrentAxesList>([&] APRINTER_TL(axis, axis::apply_default(c)));
    }
    
    template <int CurrentAxisIndex>
    struct CurrentAxis {
        using CurrentAxisParams = TypeListGet<ParamsCurrentAxesList, CurrentAxisIndex>;
        
        static void apply_default (Context c)
        {
            Current::template setCurrent<CurrentAxisIndex>(c, APRINTER_CFG(Config, CCurrent, c));
        }
        
        static void check_current_axis (Context c, TheCommand *cmd, char axis_name, FpType current)
        {
            if (axis_name == CurrentAxisParams::AxisName) {
                Current::template setCurrent<CurrentAxisIndex>(c, current);
            }
        }
        
        using CCurrent = decltype(ExprCast<FpType>(Config::e(CurrentAxisParams::DefaultCurrent::i())));
        using ConfigExprs = MakeTypeList<CCurrent>;
        
        struct Object : public ObjBase<CurrentAxis, typename MotorCurrentModule::Object, EmptyTypeList> {};
    };
    using CurrentAxesList = IndexElemList<ParamsCurrentAxesList, CurrentAxis>;
    
public:
    struct Object : public ObjBase<MotorCurrentModule, ParentObject, JoinTypeLists<
        CurrentAxesList,
        MakeTypeList<
            Current
        >
    >> {};
};

APRINTER_ALIAS_STRUCT(MotorCurrentAxisParams, (
    APRINTER_AS_VALUE(char, AxisName),
    APRINTER_AS_TYPE(DefaultCurrent),
    APRINTER_AS_TYPE(Params)
))

APRINTER_ALIAS_STRUCT_EXT(MotorCurrentModuleService, (
    APRINTER_AS_TYPE(CurrentAxesList),
    APRINTER_AS_TYPE(CurrentService)
), (
    APRINTER_MODULE_TEMPLATE(MotorCurrentModuleService, MotorCurrentModule)
))

}

#endif
