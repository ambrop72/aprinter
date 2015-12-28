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

#ifndef AMBROLIB_GENERIC_THERMISTOR_H
#define AMBROLIB_GENERIC_THERMISTOR_H

#include <aprinter/meta/AliasStruct.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/printer/Configuration.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename Config, typename FpType, typename Params>
class GenericThermistor {
    using One = APRINTER_FP_CONST_EXPR(1.0);
    using RoomTemp = APRINTER_FP_CONST_EXPR(298.15);
    using ZeroCelsiusTemp = APRINTER_FP_CONST_EXPR(273.15);
    
    using RInf = decltype(Config::e(Params::ThermistorR0::i()) * ExprExp(-Config::e(Params::ThermistorBeta::i()) / RoomTemp()));
    
    template <typename Temp>
    static auto FracThermistor (Temp) -> decltype((RInf() * ExprExp(Config::e(Params::ThermistorBeta::i()) / (Temp() + ZeroCelsiusTemp()))) / Config::e(Params::ResistorR::i()));
    
public:
    static bool const NegativeSlope = true;
    
    template <typename Temp>
    static auto TempToAdc (Temp) -> decltype(FracThermistor(Temp()) / (One() + FracThermistor(Temp())));
    
    static FpType adcToTemp (Context c, FpType adc)
    {
        if (!(adc >= APRINTER_CFG(Config, CAdcMaxTemp, c))) {
            return INFINITY;
        }
        if (!(adc <= APRINTER_CFG(Config, CAdcMinTemp, c))) {
            return -INFINITY;
        }
        FpType frac_thermistor = (adc / (1.0f - adc));
        return (APRINTER_CFG(Config, CThermistorBeta, c) / (FloatLog(frac_thermistor) + APRINTER_CFG(Config, CLogRByRInf, c))) - 273.15f;
    }
    
private:
    using CAdcMinTemp = decltype(ExprCast<FpType>(TempToAdc(Config::e(Params::MinTemp::i()))));
    using CAdcMaxTemp = decltype(ExprCast<FpType>(TempToAdc(Config::e(Params::MaxTemp::i()))));
    using CThermistorBeta = decltype(ExprCast<FpType>(Config::e(Params::ThermistorBeta::i())));
    using CLogRByRInf = decltype(ExprCast<FpType>(ExprLog(Config::e(Params::ResistorR::i()) / RInf())));
    
public:
    struct Object {};
    
    using ConfigExprs = MakeTypeList<CAdcMinTemp, CAdcMaxTemp, CThermistorBeta, CLogRByRInf>;
};

APRINTER_ALIAS_STRUCT_EXT(GenericThermistorService, (
    APRINTER_AS_TYPE(ResistorR),
    APRINTER_AS_TYPE(ThermistorR0),
    APRINTER_AS_TYPE(ThermistorBeta),
    APRINTER_AS_TYPE(MinTemp),
    APRINTER_AS_TYPE(MaxTemp)
), (
    template <typename Context, typename ParentObject, typename Config, typename FpType>
    using Formula = GenericThermistor<Context, ParentObject, Config, FpType, GenericThermistorService>;
))

#include <aprinter/EndNamespace.h>

#endif
