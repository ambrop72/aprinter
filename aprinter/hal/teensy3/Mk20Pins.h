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

#ifndef AMBROLIB_MK20_PINS_H
#define AMBROLIB_MK20_PINS_H

#include <stdint.h>

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

#define MK20_PINS_DEFINE_PORT(port) \
struct Mk20Port##port { \
    static uint32_t volatile * psor () { return &GPIO##port##_PSOR; } \
    static uint32_t volatile * pcor () { return &GPIO##port##_PCOR; } \
    static uint32_t volatile * pdir () { return &GPIO##port##_PDIR; } \
    static uint32_t volatile * pddr () { return &GPIO##port##_PDDR; } \
    static uint32_t volatile * pcr0 () { return &PORT##port##_PCR0; } \
};

MK20_PINS_DEFINE_PORT(A)
MK20_PINS_DEFINE_PORT(B)
MK20_PINS_DEFINE_PORT(C)
MK20_PINS_DEFINE_PORT(D)
MK20_PINS_DEFINE_PORT(E)

template <typename TPort, int TPinIndex>
struct Mk20Pin {
    using Port = TPort;
    static const int PinIndex = TPinIndex;
};

template <bool TPullEnable, bool TPullDown>
struct Mk20PinInputMode {
    static bool const PullEnable = TPullEnable;
    static bool const PullDown = TPullDown;
};

using Mk20PinInputModeNormal = Mk20PinInputMode<false, false>;
using Mk20PinInputModePullUp = Mk20PinInputMode<true, false>;
using Mk20PinInputModePullDown = Mk20PinInputMode<true, true>;

using Mk20PinOutputModeNormal = void;

template <typename Arg>
class Mk20Pins {
    using Context      = typename Arg::Context;
    using ParentObject = typename Arg::ParentObject;
    
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    static void init (Context c)
    {
        SIM_SCGC5 |= SIM_SCGC5_PORTA | SIM_SCGC5_PORTB | SIM_SCGC5_PORTC | SIM_SCGC5_PORTD | SIM_SCGC5_PORTE;
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
        SIM_SCGC5 &= ~(SIM_SCGC5_PORTA | SIM_SCGC5_PORTB | SIM_SCGC5_PORTC | SIM_SCGC5_PORTD | SIM_SCGC5_PORTE);
    }
    
    template <typename Pin, typename Mode = Mk20PinInputModeNormal, typename ThisContext>
    static void setInput (ThisContext c)
    {
        TheDebugObject::access(c);
        
        uint32_t pcr = PORT_PCR_MUX(1);
        if (Mode::PullEnable) {
            pcr |= PORT_PCR_PE;
            if (!Mode::PullDown) {
                pcr |= PORT_PCR_PS;
            }
        }
        Pin::Port::pcr0()[Pin::PinIndex] = pcr;
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            *Pin::Port::pddr() &= ~(UINT32_C(1) << Pin::PinIndex);
        }
    }
    
    template <typename Pin, typename Mode = Mk20PinOutputModeNormal, uint8_t AlternateFunction = 1, typename ThisContext>
    static void setOutput (ThisContext c)
    {
        TheDebugObject::access(c);
        
        Pin::Port::pcr0()[Pin::PinIndex] = PORT_PCR_MUX(AlternateFunction) | PORT_PCR_SRE | PORT_PCR_DSE;
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            *Pin::Port::pddr() |= (UINT32_C(1) << Pin::PinIndex);
        }
    }
    
    template <typename Pin, typename ThisContext>
    static bool get (ThisContext c)
    {
        TheDebugObject::access(c);
        
        return (*Pin::Port::pdir() & (UINT32_C(1) << Pin::PinIndex));
    }
    
    template <typename Pin, typename ThisContext>
    static void set (ThisContext c, bool x)
    {
        TheDebugObject::access(c);
        
        if (x) {
            *Pin::Port::psor() = (UINT32_C(1) << Pin::PinIndex);
        } else {
            *Pin::Port::pcor() = (UINT32_C(1) << Pin::PinIndex);
        }
    }
    
    template <typename Pin>
    static void emergencySet (bool x)
    {
        if (x) {
            *Pin::Port::psor() = (UINT32_C(1) << Pin::PinIndex);
        } else {
            *Pin::Port::pcor() = (UINT32_C(1) << Pin::PinIndex);
        }
    }
    
    template <typename Pin>
    static void emergencySetOutput ()
    {
        Pin::Port::pcr0()[Pin::PinIndex] = PORT_PCR_MUX(1) | PORT_PCR_SRE | PORT_PCR_DSE;
        *Pin::Port::pddr() |= (UINT32_C(1) << Pin::PinIndex);
    }
    
public:
    struct Object : public ObjBase<Mk20Pins, ParentObject, MakeTypeList<TheDebugObject>> {};
};

struct Mk20PinsService {
    APRINTER_ALIAS_STRUCT_EXT(Pins, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject)
    ), (
        APRINTER_DEF_INSTANCE(Pins, Mk20Pins)
    ))
};

#include <aprinter/EndNamespace.h>

#endif
