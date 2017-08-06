/*
 * Copyright (c) 2015 Ambroz Bizjak, Armin van der Togt
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

#ifndef APRINTER_PT_RTD_FORMULA_H
#define APRINTER_PT_RTD_FORMULA_H

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/printer/Configuration.h>

namespace APrinter {

template <typename Arg>
class PtRtdFormula {
    using Context      = typename Arg::Context;
    using Config       = typename Arg::Config;
    using FpType       = typename Arg::FpType;
    using Params       = typename Arg::Params;
    
    using One = APRINTER_FP_CONST_EXPR(1.0);
    using Two = APRINTER_FP_CONST_EXPR(2.0);
    
    // For temperatures above zero: Rt = R0 * (1 + aT - bT^2)
    template <typename Temp>
    static auto ResistanceAtTemp (Temp) -> decltype(Config::e(Params::PtR0::i()) * (One() + Config::e(Params::PtA::i()) * Temp() - Config::e(Params::PtB::i()) * Temp() * Temp()));
    
    template <typename Temp>
    static auto FracRTD (Temp) -> decltype(ResistanceAtTemp(Temp()) / Config::e(Params::ResistorR::i()));

public:
    static bool const NegativeSlope = false;
    
    template <typename Temp>
    static auto TempToAdc (Temp) -> decltype(FracRTD(Temp()) / (One() + FracRTD(Temp())));

    static FpType adcToTemp (Context c, FpType adc)
    {
        if (!(adc <= APRINTER_CFG(Config, CAdcMaxTemp, c))) {
            return INFINITY;
        }
        if (!(adc >= APRINTER_CFG(Config, CAdcMinTemp, c))) {
            return -INFINITY;
        }
        FpType frac_rtd = (adc / (1.0f - adc));
        return (
            APRINTER_CFG(Config, CPtA, c) -
            FloatSqrt(
                FloatSquare(APRINTER_CFG(Config, CPtA, c))
                + 2.0f * APRINTER_CFG(Config, CTwoPtB, c) * (1.0f - frac_rtd * APRINTER_CFG(Config, CResistorRByR0, c))
            )
        ) / APRINTER_CFG(Config, CTwoPtB, c);
    }

private:
    using CAdcMinTemp = decltype(ExprCast<FpType>(TempToAdc(Config::e(Params::MinTemp::i()))));
    using CAdcMaxTemp = decltype(ExprCast<FpType>(TempToAdc(Config::e(Params::MaxTemp::i()))));
    using CResistorRByR0 = decltype(ExprCast<FpType>(Config::e(Params::ResistorR::i()) / Config::e(Params::PtR0::i())));
    using CPtA = decltype(ExprCast<FpType>(Config::e(Params::PtA::i())));
    using CTwoPtB = decltype(ExprCast<FpType>(Two() * Config::e(Params::PtB::i())));

public:
    struct Object {};

    using ConfigExprs = MakeTypeList<CAdcMinTemp, CAdcMaxTemp, CResistorRByR0, CPtA, CTwoPtB>;
};

APRINTER_ALIAS_STRUCT_EXT(PtRtdFormulaService, (
    APRINTER_AS_TYPE(ResistorR),
    APRINTER_AS_TYPE(PtR0),
    APRINTER_AS_TYPE(PtA),
    APRINTER_AS_TYPE(PtB),
    APRINTER_AS_TYPE(MinTemp),
    APRINTER_AS_TYPE(MaxTemp)
), (
    APRINTER_ALIAS_STRUCT_EXT(Formula, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(Config),
        APRINTER_AS_TYPE(FpType)
    ), (
        using Params = PtRtdFormulaService;
        APRINTER_DEF_INSTANCE(Formula, PtRtdFormula)
    ))
))

}

#endif
