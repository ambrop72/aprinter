/*
 * Copyright (c) 2013 Ambroz Bizjak
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

#ifndef AMBROLIB_HARD_PWM_H
#define AMBROLIB_HARD_PWM_H

#include <stdint.h>

#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Lock.h>

namespace APrinter {

template <typename Arg>
class HardPwm {
    using Context      = typename Arg::Context;
    using ParentObject = typename Arg::ParentObject;
    using Params       = typename Arg::Params;
    
public:
    struct Object;
    using ThePwm = typename Params::PwmService::template Pwm<Context, Object>;
    
private:
    using DutyCycleType = typename ThePwm::DutyCycleType;
    using DutyFixedType = FixedPoint<BitsInInt<ThePwm::MaxDutyCycle>::Value, false, 0>;
    
public:
    struct DutyCycleData {
        DutyCycleType duty;
    };
    
    template <typename TheTimeType>
    static void init (Context c, TheTimeType start_time)
    {
        auto *o = Object::self(c);
        ThePwm::init(c);
        o->duty = 0;
    }
    
    static void deinit (Context c)
    {
        ThePwm::deinit(c);
    }
    
    static void computeZeroDutyCycle (DutyCycleData *duty)
    {
        duty->duty = 0;
    }
    
    template <typename FpType>
    static void computeDutyCycle (FpType frac, DutyCycleData *duty)
    {
        duty->duty = FixedMin(DutyFixedType::importBits(ThePwm::MaxDutyCycle), DutyFixedType::importFpSaturatedRound(frac * ThePwm::MaxDutyCycle)).bitsValue();
    }
    
    template <typename ThisContext>
    static void setDutyCycle (ThisContext c, DutyCycleData duty)
    {
        auto *o = Object::self(c);
        
        ThePwm::setDutyCycle(c, duty.duty);
        o->duty = duty.duty;
    }
    
    template <typename FpType>
    static FpType getCurrentDutyFp (Context c)
    {
        auto *o = Object::self(c);
        
        DutyCycleType duty;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            duty = o->duty;
        }
        
        return duty / (FpType)ThePwm::MaxDutyCycle;
    }
    
    static void emergency ()
    {
        ThePwm::emergencySetOff();
    }
    
public:
    struct Object : public ObjBase<HardPwm, ParentObject, MakeTypeList<
        ThePwm
    >> {
        DutyCycleType duty;
    };
};

APRINTER_ALIAS_STRUCT_EXT(HardPwmService, (
    APRINTER_AS_TYPE(PwmService)
), (
    APRINTER_ALIAS_STRUCT_EXT(Pwm, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject)
    ), (
        using Params = HardPwmService;
        APRINTER_DEF_INSTANCE(Pwm, HardPwm)
    ))
))

}

#endif
