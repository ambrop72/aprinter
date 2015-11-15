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

#ifndef AMBROLIB_AVR_PINS_H
#define AMBROLIB_AVR_PINS_H

#include <stdint.h>

#include <avr/sfr_defs.h>
#include <avr/io.h>

#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/FuncUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Inline.h>
#include <aprinter/hal/avr/AvrIo.h>

#include <aprinter/BeginNamespace.h>

#define AMBRO_DEFINE_AVR_PORT(ClassName, PortReg, PinReg, DdrReg) \
struct ClassName { \
    static uint8_t getPin () { return PinReg; } \
    static const uint32_t port_io_addr = _SFR_IO_ADDR(PortReg); \
    static const uint32_t ddr_io_addr = _SFR_IO_ADDR(DdrReg); \
};

#ifdef PORTA
AMBRO_DEFINE_AVR_PORT(AvrPortA, PORTA, PINA, DDRA)
#endif
#ifdef PORTB
AMBRO_DEFINE_AVR_PORT(AvrPortB, PORTB, PINB, DDRB)
#endif
#ifdef PORTC
AMBRO_DEFINE_AVR_PORT(AvrPortC, PORTC, PINC, DDRC)
#endif
#ifdef PORTD
AMBRO_DEFINE_AVR_PORT(AvrPortD, PORTD, PIND, DDRD)
#endif
#ifdef PORTE
AMBRO_DEFINE_AVR_PORT(AvrPortE, PORTE, PINE, DDRE)
#endif
#ifdef PORTF
AMBRO_DEFINE_AVR_PORT(AvrPortF, PORTF, PINF, DDRF)
#endif
#ifdef PORTG
AMBRO_DEFINE_AVR_PORT(AvrPortG, PORTG, PING, DDRG)
#endif
#ifdef PORTH
AMBRO_DEFINE_AVR_PORT(AvrPortH, PORTH, PINH, DDRH)
#endif
#ifdef PORTI
AMBRO_DEFINE_AVR_PORT(AvrPortI, PORTI, PINI, DDRI)
#endif
#ifdef PORTJ
AMBRO_DEFINE_AVR_PORT(AvrPortJ, PORTJ, PINJ, DDRJ)
#endif
#ifdef PORTK
AMBRO_DEFINE_AVR_PORT(AvrPortK, PORTK, PINK, DDRK)
#endif
#ifdef PORTL
AMBRO_DEFINE_AVR_PORT(AvrPortL, PORTL, PINL, DDRL)
#endif

using AvrPorts = FilterTypeList<
    MakeTypeList<
#ifdef PORTA
        AvrPortA,
#endif
#ifdef PORTB
        AvrPortB,
#endif
#ifdef PORTC
        AvrPortC,
#endif
#ifdef PORTD
        AvrPortD,
#endif
#ifdef PORTE
        AvrPortE,
#endif
#ifdef PORTF
        AvrPortF,
#endif
#ifdef PORTG
        AvrPortG,
#endif
#ifdef PORTH
        AvrPortH,
#endif
#ifdef PORTI
        AvrPortI,
#endif
#ifdef PORTJ
        AvrPortJ,
#endif
#ifdef PORTK
        AvrPortK,
#endif
#ifdef PORTL
        AvrPortL,
#endif
        void
    >,
    ComposeFunctions<
        NotFunc,
        IsEqualFunc<void>
    >
>;

template <typename TPort, int PortPin>
struct AvrPin {
    typedef TPort Port;
    static const int port_pin = PortPin;
};

template <bool TPullUp>
struct AvrPinInputMode {
    static bool const PullUp = TPullUp;
};

using AvrPinInputModeNormal = AvrPinInputMode<false>;
using AvrPinInputModePullUp = AvrPinInputMode<true>;

template <typename Context, typename ParentObject>
class AvrPins {
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    static void init (Context c)
    {
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
    }
    
    template <typename Pin, typename Mode = AvrPinInputModeNormal, typename ThisContext>
    static void setInput (ThisContext c)
    {
        TheDebugObject::access(c);
        
        avrClearBitReg<Pin::Port::ddr_io_addr, Pin::port_pin>(c);
        if (Mode::PullUp) {
            avrSetBitReg<Pin::Port::port_io_addr, Pin::port_pin>(c);
        } else {
            avrClearBitReg<Pin::Port::port_io_addr, Pin::port_pin>(c);
        }
    }
    
    template <typename Pin, typename ThisContext>
    static void setOutput (ThisContext c)
    {
        TheDebugObject::access(c);
        
        avrSetBitReg<Pin::Port::ddr_io_addr, Pin::port_pin>(c);
    }
    
    template <typename Pin, typename ThisContext>
    AMBRO_ALWAYS_INLINE
    static bool get (ThisContext c)
    {
        TheDebugObject::access(c);
        
        return (Pin::Port::getPin() & (1 << Pin::port_pin));
    }
    
    template <typename Pin, typename ThisContext>
    AMBRO_ALWAYS_INLINE
    static void set (ThisContext c, bool x)
    {
        TheDebugObject::access(c);
        
        if (x) {
            avrSetBitReg<Pin::Port::port_io_addr, Pin::port_pin>(c);
        } else {
            avrClearBitReg<Pin::Port::port_io_addr, Pin::port_pin>(c);
        }
    }
    
    template <typename Pin>
    static void emergencySet (bool x)
    {
        if (x) {
            avrUnknownSetBitReg<Pin::Port::port_io_addr, Pin::port_pin>();
        } else {
            avrUnknownClearBitReg<Pin::Port::port_io_addr, Pin::port_pin>();
        }
    }
    
public:
    struct Object : public ObjBase<AvrPins, ParentObject, MakeTypeList<TheDebugObject>> {};
};

#include <aprinter/EndNamespace.h>

#endif
