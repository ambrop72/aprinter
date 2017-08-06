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

#ifndef AMBROLIB_PID_CONTROL_H
#define AMBROLIB_PID_CONTROL_H

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Hints.h>
#include <aprinter/printer/Configuration.h>

namespace APrinter {

template <typename Arg>
class PidControl {
    using Context             = typename Arg::Context;
    using ParentObject        = typename Arg::ParentObject;
    using Config              = typename Arg::Config;
    using MeasurementInterval = typename Arg::MeasurementInterval;
    using FpType              = typename Arg::FpType;
    using Params              = typename Arg::Params;
    
    using One = APRINTER_FP_CONST_EXPR(1.0);
    
    using CIntegralFactor = decltype(ExprCast<FpType>(MeasurementInterval() * Config::e(Params::I::i())));
    using CIStateMin = decltype(ExprCast<FpType>(Config::e(Params::IStateMin::i())));
    using CIStateMax = decltype(ExprCast<FpType>(Config::e(Params::IStateMax::i())));
    using CDHistory = decltype(ExprCast<FpType>(Config::e(Params::DHistory::i())));
    using CC5 = decltype(ExprCast<FpType>((One() - Config::e(Params::DHistory::i())) * Config::e(Params::D::i()) / MeasurementInterval()));
    using CP = decltype(ExprCast<FpType>(Config::e(Params::P::i())));
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->first = true;
        o->integral = 0.0f;
        o->derivative = 0.0f;
    }
    
    static FpType addMeasurement (Context c, FpType value, FpType target)
    {
        auto *o = Object::self(c);
        
        FpType err = target - value;
        if (AMBRO_LIKELY(!o->first)) {
            o->integral += APRINTER_CFG(Config, CIntegralFactor, c) * err;
            o->integral = FloatMax(APRINTER_CFG(Config, CIStateMin, c), FloatMin(APRINTER_CFG(Config, CIStateMax, c), o->integral));
            o->derivative = (APRINTER_CFG(Config, CDHistory, c) * o->derivative) + (APRINTER_CFG(Config, CC5, c) * (o->last - value));
        }
        o->first = false;
        o->last = value;
        return (APRINTER_CFG(Config, CP, c) * err) + o->integral + o->derivative;
    }
    
public:
    struct Object : public ObjBase<PidControl, ParentObject, EmptyTypeList> {
        bool first;
        FpType last;
        FpType integral;
        FpType derivative;
    };
    
    using ConfigExprs = MakeTypeList<CIntegralFactor, CIStateMin, CIStateMax, CDHistory, CC5, CP>;
};

APRINTER_ALIAS_STRUCT_EXT(PidControlService, (
    APRINTER_AS_TYPE(P),
    APRINTER_AS_TYPE(I),
    APRINTER_AS_TYPE(D),
    APRINTER_AS_TYPE(IStateMin),
    APRINTER_AS_TYPE(IStateMax),
    APRINTER_AS_TYPE(DHistory)
), (
    APRINTER_ALIAS_STRUCT_EXT(Control, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(Config),
        APRINTER_AS_TYPE(MeasurementInterval),
        APRINTER_AS_TYPE(FpType)
    ), (
        using Params = PidControlService;
        APRINTER_DEF_INSTANCE(Control, PidControl)
    ))
))

}

#endif
