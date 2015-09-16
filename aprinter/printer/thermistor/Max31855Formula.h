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

#ifndef APRINTER_MAX31855_FORMULA_H
#define APRINTER_MAX31855_FORMULA_H

#include <aprinter/math/FloatTools.h>
#include <aprinter/printer/Configuration.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename Config, typename FpType, typename Params>
class Max31855Formula {
    using Unscale = APRINTER_FP_CONST_EXPR(16384.0);
    using ZeroValue = APRINTER_FP_CONST_EXPR(8192.0);
    using Resolution = APRINTER_FP_CONST_EXPR(0.25);
    
public:
    template <typename Temp>
    static auto TempToAdc (Temp) -> decltype(((Temp() / Resolution()) + ZeroValue()) / Unscale());
    
    static FpType adcToTemp (Context c, FpType adc)
    {
        return ((adc * Unscale::value()) - (FpType)ZeroValue::value()) * (FpType)Resolution::value();
    }
    
public:
    struct Object {};
};

struct Max31855FormulaService {
    template <typename Context, typename ParentObject, typename Config, typename FpType>
    using Formula = Max31855Formula<Context, ParentObject, Config, FpType, Max31855FormulaService>;
};

#include <aprinter/EndNamespace.h>

#endif
