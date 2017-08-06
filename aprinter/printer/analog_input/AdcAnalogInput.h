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

#ifndef APRINTER_ADC_ANALOG_INPUT_H
#define APRINTER_ADC_ANALOG_INPUT_H

#include <aprinter/meta/ServiceUtils.h>

namespace APrinter {

template <typename Arg>
class AdcAnalogInput {
    using Context = typename Arg::Context;
    using Params  = typename Arg::Params;
    
    using TheAdc = typename Context::Adc;
    
public:
    static bool const IsRounded = false;
    
    using FixedType = typename TheAdc::FixedType;
    
    static bool isValueInvalid (FixedType value)
    {
        return false;
    }
    
    static void init (Context c)
    {
    }
    
    static void deinit (Context c)
    {
    }
    
    static FixedType getValue (Context c)
    {
        return TheAdc::template getValue<typename Params::AdcPin>(c);
    }
    
    static void check_safety (Context c)
    {
    }
    
public:
    struct Object {};
};

APRINTER_ALIAS_STRUCT_EXT(AdcAnalogInputService, (
    APRINTER_AS_TYPE(AdcPin)
), (
    APRINTER_ALIAS_STRUCT_EXT(AnalogInput, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject)
    ), (
        using Params = AdcAnalogInputService;
        APRINTER_DEF_INSTANCE(AnalogInput, AdcAnalogInput)
    ))
))

}

#endif
