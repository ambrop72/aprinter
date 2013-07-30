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

#ifndef AMBROLIB_AVR_ADC_H
#define AMBROLIB_AVR_ADC_H

#include <stdint.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/IndexElemTuple.h>
#include <aprinter/meta/TypeListIndex.h>
#include <aprinter/meta/IsEqualFunc.h>
#include <aprinter/meta/TupleGet.h>
#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/meta/TypeListLength.h>
#include <aprinter/meta/TupleForEach.h>
#include <aprinter/meta/WrapMember.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/AvrPins.h>
#include <aprinter/system/AvrLock.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename PinsList, int AdcRefSel, int AdcPrescaler>
class AvrAdc
: private DebugObject<Context, void>
{
private:
    static const int NumPins = TypeListLength<PinsList>::value;
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_make_pin_mask, make_pin_mask)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_handle_isr, handle_isr)
    
public:
    void init (Context c)
    {
        AdcMaybe<NumPins>::init(this, c);
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        AdcMaybe<NumPins>::deinit(this, c);
    }
    
    template <typename Pin>
    uint16_t getValue (Context c)
    {
        this->debugAccess(c);
        
        static const int PinIndex = TypeListIndex<PinsList, IsEqualFunc<Pin>>::value;
        
        uint16_t value;
        AMBRO_LOCK_T(AvrTempLock(), c, lock_c, {
            value = TupleGetElem<PinIndex>(&m_pins)->m_value;
        });
        
        return value;
    }
    
    template <typename Pin>
    double getFracValue (Context c)
    {
        return (getValue<Pin>(c) / 1024.0);
    }
    
    void adc_isr (AvrInterruptContext<Context> c)
    {
        TupleForEachForwardInterruptible(&m_pins, Foreach_handle_isr(), c);
    }
    
private:
    template <int NumPins, typename Dummy = void>
    struct AdcMaybe {
        static void init (AvrAdc *o, Context c)
        {
            o->m_current_pin = 0;
            o->m_finished = false;
            
            uint8_t mask = 0;
            TupleForEachForward(&o->m_pins, Foreach_make_pin_mask(), &mask);
            DIDR0 = mask;
            ADMUX = (AdcRefSel << REFS0) | AdcPin<0>::Pin::port_pin;
            ADCSRB = 0;
            ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADIE) | (AdcPrescaler << ADPS0);
            
            while (!*(volatile bool *)&o->m_finished);
        }
        
        static void deinit (AvrAdc *o, Context c)
        {
            ADCSRA = 0;
            ADCSRB = 0;
            ADMUX = 0;
            DIDR0 = 0;
        }
    };
    
    template <typename Dummy>
    struct AdcMaybe<0, Dummy> {
        static void init (AvrAdc *o, Context c) {}
        static void deinit (AvrAdc *o, Context c) {}
    };
    
    template <int PinIndex>
    struct AdcPin {
        using Pin = TypeListGet<PinsList, PinIndex>;
        static_assert(TypesAreEqual<typename Pin::Port, AvrPortA>::value, "");
        static_assert(Pin::port_pin < 8, "");
        static const int NextPinIndex = (PinIndex + 1) % NumPins;
        
        AvrAdc * parent ()
        {
            return AMBRO_WMEMB_TD(&AvrAdc::m_pins)::container(TupleGetTuple<PinIndex, PinsTuple>(this));
        }
        
        void make_pin_mask (uint8_t *out)
        {
            *out |= (1 << Pin::port_pin);
        }
        
        bool handle_isr (AvrInterruptContext<Context> c)
        {
            if (parent()->m_current_pin == PinIndex) {
                uint16_t adc = ADC;
                ADMUX = (AdcRefSel << REFS0) | AdcPin<NextPinIndex>::Pin::port_pin;
                ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADIE) | (AdcPrescaler << ADPS0);
                m_value = adc;
                parent()->m_current_pin = NextPinIndex;
                if (PinIndex == NumPins - 1) {
                    parent()->m_finished = true;
                }
                return false;
            }
            return true;
        }
        
        uint16_t m_value;
    };
    
    using PinsTuple = IndexElemTuple<PinsList, AdcPin>;
    
    PinsTuple m_pins;
    uint8_t m_current_pin;
    bool m_finished;
};

#define AMBRO_AVR_ADC_ISRS(adc, context) \
ISR(ADC_vect) \
{ \
    (adc).adc_isr(MakeAvrInterruptContext(context)); \
}

#include <aprinter/EndNamespace.h>

#endif
