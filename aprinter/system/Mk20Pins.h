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

#include <aprinter/meta/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

template <uint32_t TPsorAddr, uint32_t TPcorAddr, uint32_t TPdirAddr, uint32_t TPddrAddr, uint32_t TPcr0Addr>
struct Mk20Port {
    static uint32_t volatile * psor () { return (uint32_t volatile *)TPsorAddr; }
    static uint32_t volatile * pcor () { return (uint32_t volatile *)TPcorAddr; }
    static uint32_t volatile * pdir () { return (uint32_t volatile *)TPdirAddr; }
    static uint32_t volatile * pddr () { return (uint32_t volatile *)TPddrAddr; }
    static uint32_t volatile * pcr0 () { return (uint32_t volatile *)TPcr0Addr; }
};

using Mk20PortA = Mk20Port<(uint32_t)&GPIOA_PSOR, (uint32_t)&GPIOA_PCOR, (uint32_t)&GPIOA_PDIR, (uint32_t)&GPIOA_PDDR, (uint32_t)&PORTA_PCR0>;
using Mk20PortB = Mk20Port<(uint32_t)&GPIOB_PSOR, (uint32_t)&GPIOB_PCOR, (uint32_t)&GPIOB_PDIR, (uint32_t)&GPIOB_PDDR, (uint32_t)&PORTB_PCR0>;
using Mk20PortC = Mk20Port<(uint32_t)&GPIOC_PSOR, (uint32_t)&GPIOC_PCOR, (uint32_t)&GPIOC_PDIR, (uint32_t)&GPIOC_PDDR, (uint32_t)&PORTC_PCR0>;
using Mk20PortD = Mk20Port<(uint32_t)&GPIOD_PSOR, (uint32_t)&GPIOD_PCOR, (uint32_t)&GPIOD_PDIR, (uint32_t)&GPIOD_PDDR, (uint32_t)&PORTD_PCR0>;
using Mk20PortE = Mk20Port<(uint32_t)&GPIOE_PSOR, (uint32_t)&GPIOE_PCOR, (uint32_t)&GPIOE_PDIR, (uint32_t)&GPIOE_PDDR, (uint32_t)&PORTE_PCR0>;

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

template <typename Context, typename ParentObject>
class Mk20Pins {
public:
    struct Object;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        SIM_SCGC5 |= SIM_SCGC5_PORTA | SIM_SCGC5_PORTB | SIM_SCGC5_PORTC | SIM_SCGC5_PORTD | SIM_SCGC5_PORTE;
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->debugDeinit(c);
        SIM_SCGC5 &= ~(SIM_SCGC5_PORTA | SIM_SCGC5_PORTB | SIM_SCGC5_PORTC | SIM_SCGC5_PORTD | SIM_SCGC5_PORTE);
    }
    
    template <typename Pin, typename Mode = Mk20PinInputModeNormal, typename ThisContext>
    static void setInput (ThisContext c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        uint32_t pcr = PORT_PCR_MUX(1);
        if (Mode::PullEnable) {
            pcr |= PORT_PCR_PE;
            if (!Mode::PullDown) {
                pcr |= PORT_PCR_PS;
            }
        }
        Pin::Port::pcr0()[Pin::PinIndex] = pcr;
        
        AMBRO_LOCK_T(AtomicTempLock(), c, lock_c) {
            *Pin::Port::pddr() &= ~(UINT32_C(1) << Pin::PinIndex);
        }
    }
    
    template <typename Pin, typename Mode = Mk20PinOutputModeNormal, uint8_t AlternateFunction = 1, typename ThisContext>
    static void setOutput (ThisContext c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        Pin::Port::pcr0()[Pin::PinIndex] = PORT_PCR_MUX(AlternateFunction) | PORT_PCR_SRE | PORT_PCR_DSE;
        
        AMBRO_LOCK_T(AtomicTempLock(), c, lock_c) {
            *Pin::Port::pddr() |= (UINT32_C(1) << Pin::PinIndex);
        }
    }
    
    template <typename Pin, typename ThisContext>
    static bool get (ThisContext c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        return (*Pin::Port::pdir() & (UINT32_C(1) << Pin::PinIndex));
    }
    
    template <typename Pin, typename ThisContext>
    static void set (ThisContext c, bool x)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
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
    
public:
    struct Object : public ObjBase<Mk20Pins, ParentObject, EmptyTypeList>,
        public DebugObject<Context, void>
    {};
};

#include <aprinter/EndNamespace.h>

#endif
