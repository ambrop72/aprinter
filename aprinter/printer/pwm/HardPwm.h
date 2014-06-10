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

#include <aprinter/meta/Object.h>
#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/base/Lock.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename Params>
class HardPwm {
public:
    struct Object;
    using ThePwm = typename Params::PwmService::template Pwm<Context, Object>;
    
private:
    using DutyFixedType = FixedPoint<BitsInInt<ThePwm::MaxDutyCycle>::Value, false, 0>;
    
public:
    struct DutyCycleData {
        typename ThePwm::DutyCycleType duty;
    };
    
    template <typename TheTimeType>
    static void init (Context c, TheTimeType start_time)
    {
        ThePwm::init(c);
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
        ThePwm::setDutyCycle(c, duty.duty);
    }
    
    static void emergency ()
    {
        ThePwm::emergencySetOff();
    }
    
public:
    struct Object : public ObjBase<HardPwm, ParentObject, MakeTypeList<
        ThePwm
    >> {};
};

template <typename TPwmService>
struct HardPwmService {
    using PwmService = TPwmService;
    
    template <typename Context, typename ParentObject>
    using Pwm = HardPwm<Context, ParentObject, HardPwmService>;
};

#include <aprinter/EndNamespace.h>

#endif
