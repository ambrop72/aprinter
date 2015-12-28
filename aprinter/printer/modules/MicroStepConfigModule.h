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

#ifndef APRINTER_MICROSTEP_CONFIG_MODULE_H
#define APRINTER_MICROSTEP_CONFIG_MODULE_H

#include <stdint.h>

#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/AliasStruct.h>
#include <aprinter/base/Object.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ThePrinterMain, typename Params>
class MicroStepConfigModule {
public:
    struct Object;
    
private:
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_init, init)
    
public:
    static void init (Context c)
    {
        ListForEachForward<MicroStepAxisList>(LForeach_init(), c);
    }
    
private:
    template <int MicroStepAxisIndex>
    struct MicroStepAxis {
        struct Object;
        using TheSpec = TypeListGet<typename Params::MicroStepAxisList, MicroStepAxisIndex>;
        using TheMicroStep = typename TheSpec::MicroStepService::template MicroStep<Context, Object>;
        
        static void init (Context c)
        {
            TheMicroStep::init(c, TheSpec::MicroSteps);
        }
        
        struct Object : public ObjBase<MicroStepAxis, typename MicroStepConfigModule::Object, MakeTypeList<
            TheMicroStep
        >> {};
    };
    using MicroStepAxisList = IndexElemList<typename Params::MicroStepAxisList, MicroStepAxis>;
    
public:
    struct Object : public ObjBase<MicroStepConfigModule, ParentObject, MicroStepAxisList> {};
};

APRINTER_ALIAS_STRUCT(MicroStepAxisParams, (
    APRINTER_AS_TYPE(MicroStepService),
    APRINTER_AS_VALUE(uint8_t, MicroSteps)
))

APRINTER_ALIAS_STRUCT_EXT(MicroStepConfigModuleService, (
    APRINTER_AS_TYPE(MicroStepAxisList)
), (
    APRINTER_MODULE_TEMPLATE(MicroStepConfigModuleService, MicroStepConfigModule)
))

#include <aprinter/EndNamespace.h>

#endif
