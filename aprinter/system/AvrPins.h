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

#include <aprinter/base/DebugObject.h>

#include <aprinter/BeginNamespace.h>

#define AMBRO_DEFINE_AVR_PORT(ClassName, PortReg, PinReg, DdrReg, PcMskReg, PcIeBit) \
struct ClassName { \
    static uint8_t getPin () { return PinReg; } \
    static const uint32_t port_io_addr = _SFR_IO_ADDR(PortReg); \
    static const uint32_t ddr_io_addr = _SFR_IO_ADDR(DdrReg); \
    static const uint32_t pcmsk_io_addr = _SFR_IO_ADDR(PcMskReg); \
    static const uint8_t pcie_bit = PcIeBit; \
};

#ifdef PORTA
AMBRO_DEFINE_AVR_PORT(AvrPortA, PORTA, PINA, DDRA, PCMSK0, PCIE0)
#endif
#ifdef PORTB
AMBRO_DEFINE_AVR_PORT(AvrPortB, PORTB, PINB, DDRB, PCMSK1, PCIE1)
#endif
#ifdef PORTC
AMBRO_DEFINE_AVR_PORT(AvrPortC, PORTC, PINC, DDRC, PCMSK2, PCIE2)
#endif
#ifdef PORTD
AMBRO_DEFINE_AVR_PORT(AvrPortD, PORTD, PIND, DDRD, PCMSK3, PCIE3)
#endif
#ifdef PORTE
AMBRO_DEFINE_AVR_PORT(AvrPortE, PORTE, PINE, DDRE, PCMSK4, PCIE4)
#endif

template <typename TPort, int PortPin>
struct AvrPin {
    typedef TPort Port;
    static const int port_pin = PortPin;
};

template <typename Context>
class AvrPins
: private DebugObject<Context, AvrPins<Context>>
{
public:
    void init (Context c)
    {
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
    }
    
    template <typename Pin, typename ThisContext>
    void setInput (ThisContext c)
    {
        this->debugAccess(c);
        
        asm("cbi %0,%1" :: "i" (Pin::Port::ddr_io_addr), "i" (Pin::port_pin));
    }
    
    template <typename Pin, typename ThisContext>
    void setOutput (ThisContext c)
    {
        this->debugAccess(c);
        
        asm("sbi %0,%1" :: "i" (Pin::Port::ddr_io_addr), "i" (Pin::port_pin));
    }
    
    template <typename Pin, typename ThisContext>
    bool get (ThisContext c)
    {
        this->debugAccess(c);
        
        return (Pin::Port::getPin() & (1 << Pin::port_pin));
    }
    
    template <typename Pin, typename ThisContext>
    void set (ThisContext c, bool x)
    {
        this->debugAccess(c);
        
        if (x) {
            asm("sbi %0,%1" :: "i" (Pin::Port::port_io_addr), "i" (Pin::port_pin));
        } else {
            asm("cbi %0,%1" :: "i" (Pin::Port::port_io_addr), "i" (Pin::port_pin));
        }
    }
};

#include <aprinter/EndNamespace.h>

#endif
