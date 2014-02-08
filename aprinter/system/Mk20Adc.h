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

#ifndef AMBROLIB_MK20_ADC_H
#define AMBROLIB_MK20_ADC_H

#include <stdint.h>

#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/IndexElemTuple.h>
#include <aprinter/meta/TypeListIndex.h>
#include <aprinter/meta/IsEqualFunc.h>
#include <aprinter/meta/TupleGet.h>
#include <aprinter/meta/TypeListLength.h>
#include <aprinter/meta/TupleForEach.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/Position.h>
#include <aprinter/meta/TuplePosition.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/Mk20Pins.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

struct Mk20AdcUnsupportedInput {};

template <typename Position, typename Context, typename PinsList, int ADiv>
class Mk20Adc
: private DebugObject<Context, void>
{
    static_assert(ADiv >= 0 && ADiv <= 3, "");
    
private:
    AMBRO_MAKE_SELF(Context, Mk20Adc, Position)
    template <int PinIndex> struct PinPosition;
    
    static const int NumPins = TypeListLength<PinsList>::value;
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_init, init)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_handle_isr, handle_isr)
    
    using AdcList = MakeTypeList<
        Mk20AdcUnsupportedInput,
        Mk20AdcUnsupportedInput,
        Mk20AdcUnsupportedInput,
        Mk20AdcUnsupportedInput,
        Mk20Pin<Mk20PortC, 2>,
        Mk20Pin<Mk20PortD, 1>,
        Mk20Pin<Mk20PortD, 5>,
        Mk20Pin<Mk20PortD, 6>,
        Mk20Pin<Mk20PortB, 0>,
        Mk20Pin<Mk20PortB, 1>,
        Mk20AdcUnsupportedInput,
        Mk20AdcUnsupportedInput,
        Mk20Pin<Mk20PortB, 2>,
        Mk20Pin<Mk20PortB, 3>,
        Mk20Pin<Mk20PortC, 0>,
        Mk20Pin<Mk20PortC, 1>
    >;
    
public:
    static void init (Context c)
    {
        Mk20Adc *o = self(c);
        AdcMaybe<NumPins>::init(c);
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        Mk20Adc *o = self(c);
        o->debugDeinit(c);
        AdcMaybe<NumPins>::deinit(c);
    }
    
    template <typename Pin, typename ThisContext>
    static uint16_t getValue (ThisContext c)
    {
        Mk20Adc *o = self(c);
        o->debugAccess(c);
        
        static const int PinIndex = TypeListIndex<PinsList, IsEqualFunc<Pin>>::value;
        
        uint16_t value;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            value = TupleGetElem<PinIndex>(&o->m_pins)->m_value;
        }
        
        return value;
    }
    
    static void adc_isr (InterruptContext<Context> c)
    {
        Mk20Adc *o = self(c);
        TupleForEachForwardInterruptible(&o->m_pins, Foreach_handle_isr(), c);
    }
    
private:
    template <int NumPins, typename Dummy = void>
    struct AdcMaybe {
        static void init (Context c)
        {
            Mk20Adc *o = self(c);
            
            o->m_current_pin = 0;
            o->m_finished = false;
            
            TupleForEachForward(&o->m_pins, Foreach_init(), c);
            
            SIM_SCGC6 |= SIM_SCGC6_ADC0;
            ADC0_CFG1 = ADC_CFG1_MODE(2) | ADC_CFG1_ADLSMP | ADC_CFG1_ADIV(ADiv);
            ADC0_CFG2 = ADC_CFG2_MUXSEL;
            ADC0_SC2 = 0;
            ADC0_SC3 = 0;
            NVIC_CLEAR_PENDING(IRQ_ADC0);
            NVIC_SET_PRIORITY(IRQ_ADC0, INTERRUPT_PRIORITY);
            NVIC_ENABLE_IRQ(IRQ_ADC0);
            ADC0_SC1A = ADC_SC1_AIEN | ADC_SC1_ADCH(AdcPin<0>::AdcIndex);
            
            while (!*(volatile bool *)&o->m_finished);
        }
        
        static void deinit (Context c)
        {
            NVIC_DISABLE_IRQ(IRQ_ADC0);
            ADC0_SC1A = ADC_SC1_ADCH(0x1F);
            NVIC_CLEAR_PENDING(IRQ_ADC0);
            SIM_SCGC6 &= ~SIM_SCGC6_ADC0;
        }
    };
    
    template <typename Dummy>
    struct AdcMaybe<0, Dummy> {
        static void init (Context c) {}
        static void deinit (Context c) {}
    };
    
    template <int PinIndex>
    struct AdcPin {
        AMBRO_MAKE_SELF(Context, AdcPin, PinPosition<PinIndex>)
        using Pin = TypeListGet<PinsList, PinIndex>;
        static const int AdcIndex = TypeListIndex<AdcList, IsEqualFunc<Pin>>::value;
        static const int NextPinIndex = (PinIndex + 1) % NumPins;
        
        static void init (Context c)
        {
            Pin::Port::pcr0()[Pin::PinIndex] = PORT_PCR_MUX(0);
        }
        
        static bool handle_isr (InterruptContext<Context> c)
        {
            AdcPin *o = self(c);
            Mk20Adc *a = Mk20Adc::self(c);
            
            if (a->m_current_pin != PinIndex) {
                return true;
            }
            o->m_value = ADC0_RA;
            ADC0_SC1A = ADC_SC1_AIEN | ADC_SC1_ADCH(AdcPin<NextPinIndex>::AdcIndex);
            a->m_current_pin = NextPinIndex;
            if (PinIndex == NumPins - 1) {
                a->m_finished = true;
            }
            return false;
        }
        
        uint16_t m_value;
    };
    
    using PinsTuple = IndexElemTuple<PinsList, AdcPin>;
    
    PinsTuple m_pins;
    uint8_t m_current_pin;
    bool m_finished;
    
    template <int PinIndex> struct PinPosition : public TuplePosition<Position, PinsTuple, &Mk20Adc::m_pins, PinIndex> {};
};

#define AMBRO_MK20_ADC_ISRS(adc, context) \
extern "C" \
__attribute__((used)) \
void adc0_isr (void) \
{ \
    (adc).adc_isr(MakeInterruptContext(context)); \
}

#include <aprinter/EndNamespace.h>

#endif
