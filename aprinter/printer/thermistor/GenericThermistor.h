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

#include <math.h>

#include <aprinter/meta/WrapDouble.h>
#include <aprinter/math/FloatTools.h>

#include <aprinter/BeginNamespace.h>

template <
    typename ResistorR,
    typename ThermistorR0,
    typename ThermistorBeta,
    typename MinTemp,
    typename MaxTemp
>
struct GenericThermistor {
    template <typename FpType>
    class Inner {
        using RInf = AMBRO_WRAP_DOUBLE(ThermistorR0::value() * __builtin_exp(-ThermistorBeta::value() / 298.15));
        
    public:
        template <typename Temp>
        class TempToAdc {
            using FracThermistor = AMBRO_WRAP_DOUBLE((RInf::value() * __builtin_exp(ThermistorBeta::value() / (Temp::value() + 273.15))) / ResistorR::value());
        public:
            using Result = AMBRO_WRAP_DOUBLE(FracThermistor::value() / (1.0 + FracThermistor::value()));
        };
        
        static FpType adc_to_temp (FpType adc)
        {
            if (!(adc >= (FpType)TempToAdc<MaxTemp>::Result::value())) {
                return INFINITY;
            }
            if (!(adc <= (FpType)TempToAdc<MinTemp>::Result::value())) {
                return -INFINITY;
            }
            FpType frac_thermistor = (adc / (1.0f - adc));
            return ((FpType)ThermistorBeta::value() / (FloatLog(frac_thermistor) + (FpType)__builtin_log(ResistorR::value() / RInf::value()))) - 273.15f;
        }
    };
};

#include <aprinter/EndNamespace.h>

#endif
