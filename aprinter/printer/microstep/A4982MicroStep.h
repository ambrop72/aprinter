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

#ifndef AMBROLIB_A4982_MICROSTEP_H
#define AMBROLIB_A4982_MICROSTEP_H

#include <stdint.h>

#include <aprinter/meta/Position.h>

#include <aprinter/BeginNamespace.h>

template <
    typename TMs1Pin,
    typename TMs2Pin
>
struct A4982MicroStepParams {
    using Ms1Pin = TMs1Pin;
    using Ms2Pin = TMs2Pin;
};

template <typename Position, typename Context, typename Params>
class A4982MicroStep {
public:
    static void init (Context c, uint8_t microsteps)
    {
        set_microsteps(c, microsteps);
        c.pins()->template setOutput<typename Params::Ms1Pin>(c);
        c.pins()->template setOutput<typename Params::Ms2Pin>(c);
    }
    
    static void set_microsteps (Context c, uint8_t microsteps)
    {
        bool ms1;
        bool ms2;
        switch (microsteps) {
            default:
            case 1: {
                ms1 = false;
                ms2 = false;
            } break;
            case 2: {
                ms1 = true;
                ms2 = false;
            } break;
            case 4: {
                ms1 = false;
                ms2 = true;
            } break;
            case 16: {
                ms1 = true;
                ms2 = true;
            } break;
        }
        c.pins()->template set<typename Params::Ms1Pin>(c, ms1);
        c.pins()->template set<typename Params::Ms2Pin>(c, ms2);
    }
};

#include <aprinter/EndNamespace.h>

#endif
