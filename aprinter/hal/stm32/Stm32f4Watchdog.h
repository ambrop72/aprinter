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

#ifndef AMBROLIB_STM32F4_WATCHDOG_H
#define AMBROLIB_STM32F4_WATCHDOG_H

#include <stdint.h>

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Hints.h>

#include <aprinter/BeginNamespace.h>

template <typename Arg>
class Stm32f4Watchdog {
    APRINTER_USE_TYPES1(Arg, (Context, ParentObject, Params))
    
    static uint32_t const PrescalerValue =
        Params::Divider == 4 ? IWDG_PRESCALER_4 :
        Params::Divider == 8 ? IWDG_PRESCALER_8 :
        Params::Divider == 16 ? IWDG_PRESCALER_16 :
        Params::Divider == 32 ? IWDG_PRESCALER_32 :
        Params::Divider == 64 ? IWDG_PRESCALER_64 :
        Params::Divider == 128 ? IWDG_PRESCALER_128 :
        Params::Divider == 256 ? IWDG_PRESCALER_256 :
        UINT32_MAX;
    static_assert(PrescalerValue != UINT32_MAX, "Invalid watchdog Divider value");
    static_assert(Params::Reload <= 0xFFF, "Invalid watchdog Reload value");
    
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    static constexpr double WatchdogTime = Params::Reload / ((double)LSI_VALUE / Params::Divider);
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::init(c);
        
        o->iwdg_handle.Instance = IWDG;
        o->iwdg_handle.Init.Prescaler = PrescalerValue;
        o->iwdg_handle.Init.Reload = Params::Reload;
        
        HAL_IWDG_Init(&o->iwdg_handle);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
    }
    
    template <typename ThisContext>
    static void reset (ThisContext c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        HAL_IWDG_Refresh(&o->iwdg_handle);
    }
    
    APRINTER_NO_RETURN
    static void emergency_abort ()
    {
        while (true);
    }
    
public:
    struct Object : public ObjBase<Stm32f4Watchdog, ParentObject, MakeTypeList<TheDebugObject>> {
        IWDG_HandleTypeDef iwdg_handle;
    };
};

APRINTER_ALIAS_STRUCT_EXT(Stm32f4WatchdogService, (
    APRINTER_AS_VALUE(int, Divider),
    APRINTER_AS_VALUE(uint16_t, Reload)
), (
    APRINTER_ALIAS_STRUCT_EXT(Watchdog, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_VALUE(bool, DebugMode)
    ), (
        using Params = Stm32f4WatchdogService;
        APRINTER_DEF_INSTANCE(Watchdog, Stm32f4Watchdog)
    ))
))

#include <aprinter/EndNamespace.h>

#endif
