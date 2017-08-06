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

#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Lock.h>
#include <aprinter/hal/teensy3/Mk20Pins.h>
#include <aprinter/system/InterruptLock.h>

namespace APrinter {

struct Mk20AdcUnsupportedInput {};

//template <typename Context, typename ParentObject, typename ParamsPinsList, int ADiv>
template <typename Arg>
class Mk20Adc {
    using Context        = typename Arg::Context;
    using ParentObject   = typename Arg::ParentObject;
    using ParamsPinsList = typename Arg::PinsList;
    using Params         = typename Arg::Params;
    
    static int const ADiv = Params::ADiv;
    static_assert(ADiv >= 0 && ADiv <= 3, "");
    
private:
    static const int NumPins = TypeListLength<ParamsPinsList>::Value;
    
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
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    using FixedType = FixedPoint<16, false, -16>;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        AdcMaybe<NumPins>::init(c);
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        AdcMaybe<NumPins>::deinit(c);
    }
    
    template <typename Pin, typename ThisContext>
    static FixedType getValue (ThisContext c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        static const int PinIndex = TypeListIndex<ParamsPinsList, Pin>::Value;
        
        uint16_t value;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            value = AdcPin<PinIndex>::Object::self(c)->m_value;
        }
        
        return FixedType::importBits(value);
    }
    
    static void adc_isr (InterruptContext<Context> c)
    {
        ListForBreak<PinsList>([&] APRINTER_TL(pin, return pin::handle_isr(c)));
    }
    
private:
    template <int NumPins, typename Dummy = void>
    struct AdcMaybe {
        static void init (Context c)
        {
            auto *o = Object::self(c);
            
            o->m_current_pin = 0;
            o->m_finished = false;
            
            ListFor<PinsList>([&] APRINTER_TL(pin, pin::init(c)));
            
            memory_barrier();
            
            SIM_SCGC6 |= SIM_SCGC6_ADC0;
            ADC0_CFG1 = ADC_CFG1_MODE(3) | ADC_CFG1_ADLSMP | ADC_CFG1_ADIV(ADiv);
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
            
            memory_barrier();
        }
    };
    
    template <typename Dummy>
    struct AdcMaybe<0, Dummy> {
        static void init (Context c) {}
        static void deinit (Context c) {}
    };
    
    template <int PinIndex>
    struct AdcPin {
        struct Object;
        using Pin = TypeListGet<ParamsPinsList, PinIndex>;
        static const int AdcIndex = TypeListIndex<AdcList, Pin>::Value;
        static const int NextPinIndex = (PinIndex + 1) % NumPins;
        
        static void init (Context c)
        {
            Pin::Port::pcr0()[Pin::PinIndex] = PORT_PCR_MUX(0);
        }
        
        static bool handle_isr (InterruptContext<Context> c)
        {
            auto *o = Object::self(c);
            auto *ao = Mk20Adc::Object::self(c);
            
            if (ao->m_current_pin != PinIndex) {
                return true;
            }
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                o->m_value = ADC0_RA;
            }
            ADC0_SC1A = ADC_SC1_AIEN | ADC_SC1_ADCH(AdcPin<NextPinIndex>::AdcIndex);
            ao->m_current_pin = NextPinIndex;
            if (PinIndex == NumPins - 1) {
                ao->m_finished = true;
            }
            return false;
        }
        
        struct Object : public ObjBase<AdcPin, typename Mk20Adc::Object, EmptyTypeList> {
            uint16_t m_value;
        };
    };
    
    using PinsList = IndexElemList<ParamsPinsList, AdcPin>;
    
public:
    struct Object : public ObjBase<Mk20Adc, ParentObject, JoinTypeLists<
        PinsList,
        MakeTypeList<TheDebugObject>
    >> {
        uint8_t m_current_pin;
        bool m_finished;
    };
};

APRINTER_ALIAS_STRUCT_EXT(Mk20AdcService, (
    APRINTER_AS_VALUE(int, ADiv)
), (
    APRINTER_ALIAS_STRUCT_EXT(Adc, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(PinsList)
    ), (
        using Params = Mk20AdcService;
        APRINTER_DEF_INSTANCE(Adc, Mk20Adc)
    ))
))

#define AMBRO_MK20_ADC_ISRS(adc, context) \
extern "C" \
__attribute__((used)) \
void adc0_isr (void) \
{ \
    adc::adc_isr(MakeInterruptContext(context)); \
}

}

#endif
