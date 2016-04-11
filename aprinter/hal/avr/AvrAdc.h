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

#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Lock.h>
#include <aprinter/hal/avr/AvrPins.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

template <typename TPin1, typename TPin2, int TGain>
struct AvrAdcDifferentialInput {};

struct AvrAdcVbgPin {};
struct AvrAdcGndPin {};

template <typename Arg>
class AvrAdc {
    using Context        = typename Arg::Context;
    using ParentObject   = typename Arg::ParentObject;
    using ParamsPinsList = typename Arg::PinsList;
    using Params         = typename Arg::Params;
    
    static int const AdcRefSel           = Params::AdcRefSel;
    static int const AdcPrescaler        = Params::AdcPrescaler;
    static int const AdcOverSamplingBits = Params::AdcOverSamplingBits;
    
private:
    static const int NumPins = TypeListLength<ParamsPinsList>::Value;
    
    static_assert(AdcOverSamplingBits >= 0 && AdcOverSamplingBits <= 3, "");
    static const int NumSamplesPerPin = 1 << (AdcOverSamplingBits * 2);
    static const int NumValueBits = 10 + AdcOverSamplingBits;
    
    using MaskType = uint16_t;
    
    template <typename AdcPin>
    struct AdcPinMask {
        static const MaskType Value = 0;
    };
    
    template <typename Pin1, typename Pin2, int Gain>
    struct AdcPinMask<AvrAdcDifferentialInput<Pin1, Pin2, Gain>> {
        static const MaskType Value = (AdcPinMask<Pin1>::Value | AdcPinMask<Pin2>::Value);
    };
    
#if defined(__AVR_ATmega164A__) || defined(__AVR_ATmega164PA__) || defined(__AVR_ATmega324A__) || \
    defined(__AVR_ATmega324PA__) || defined(__AVR_ATmega644A__) || defined(__AVR_ATmega644PA__) || \
    defined(__AVR_ATmega128__) || defined(__AVR_ATmega1284P__)
    
    using AdcList = MakeTypeList<
        AvrPin<AvrPortA, 0>,
        AvrPin<AvrPortA, 1>,
        AvrPin<AvrPortA, 2>,
        AvrPin<AvrPortA, 3>,
        AvrPin<AvrPortA, 4>,
        AvrPin<AvrPortA, 5>,
        AvrPin<AvrPortA, 6>,
        AvrPin<AvrPortA, 7>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 0>, AvrPin<AvrPortA, 0>, 10>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 1>, AvrPin<AvrPortA, 0>, 10>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 0>, AvrPin<AvrPortA, 0>, 200>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 1>, AvrPin<AvrPortA, 0>, 200>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 2>, AvrPin<AvrPortA, 2>, 10>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 3>, AvrPin<AvrPortA, 2>, 10>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 2>, AvrPin<AvrPortA, 2>, 200>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 3>, AvrPin<AvrPortA, 2>, 200>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 0>, AvrPin<AvrPortA, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 1>, AvrPin<AvrPortA, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 2>, AvrPin<AvrPortA, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 3>, AvrPin<AvrPortA, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 4>, AvrPin<AvrPortA, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 5>, AvrPin<AvrPortA, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 6>, AvrPin<AvrPortA, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 7>, AvrPin<AvrPortA, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 0>, AvrPin<AvrPortA, 2>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 1>, AvrPin<AvrPortA, 2>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 2>, AvrPin<AvrPortA, 2>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 3>, AvrPin<AvrPortA, 2>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 4>, AvrPin<AvrPortA, 2>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortA, 5>, AvrPin<AvrPortA, 2>, 1>,
        AvrAdcVbgPin,
        AvrAdcGndPin
    >;
    
    template <int PortPinIndex>
    struct AdcPinMask<AvrPin<AvrPortA, PortPinIndex>> {
        static const MaskType Value = ((MaskType)1 << PortPinIndex);
    };
    
#elif defined(__AVR_ATmega640__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega1281__) || \
    defined(__AVR_ATmega2560__) || defined(__AVR_ATmega2561__)
    
    using AdcList = MakeTypeList<
        AvrPin<AvrPortF, 0>,
        AvrPin<AvrPortF, 1>,
        AvrPin<AvrPortF, 2>,
        AvrPin<AvrPortF, 3>,
        AvrPin<AvrPortF, 4>,
        AvrPin<AvrPortF, 5>,
        AvrPin<AvrPortF, 6>,
        AvrPin<AvrPortF, 7>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 0>, AvrPin<AvrPortF, 0>, 10>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 1>, AvrPin<AvrPortF, 0>, 10>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 0>, AvrPin<AvrPortF, 0>, 200>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 1>, AvrPin<AvrPortF, 0>, 200>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 2>, AvrPin<AvrPortF, 2>, 10>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 3>, AvrPin<AvrPortF, 2>, 10>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 2>, AvrPin<AvrPortF, 2>, 200>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 3>, AvrPin<AvrPortF, 2>, 200>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 0>, AvrPin<AvrPortF, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 1>, AvrPin<AvrPortF, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 2>, AvrPin<AvrPortF, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 3>, AvrPin<AvrPortF, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 4>, AvrPin<AvrPortF, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 5>, AvrPin<AvrPortF, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 6>, AvrPin<AvrPortF, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 7>, AvrPin<AvrPortF, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 0>, AvrPin<AvrPortF, 2>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 1>, AvrPin<AvrPortF, 2>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 2>, AvrPin<AvrPortF, 2>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 3>, AvrPin<AvrPortF, 2>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 4>, AvrPin<AvrPortF, 2>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortF, 5>, AvrPin<AvrPortF, 2>, 1>,
        AvrAdcVbgPin,
        AvrAdcGndPin,
        AvrPin<AvrPortK, 0>,
        AvrPin<AvrPortK, 1>,
        AvrPin<AvrPortK, 2>,
        AvrPin<AvrPortK, 3>,
        AvrPin<AvrPortK, 4>,
        AvrPin<AvrPortK, 5>,
        AvrPin<AvrPortK, 6>,
        AvrPin<AvrPortK, 7>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 0>, AvrPin<AvrPortK, 0>, 10>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 1>, AvrPin<AvrPortK, 0>, 10>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 0>, AvrPin<AvrPortK, 0>, 200>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 1>, AvrPin<AvrPortK, 0>, 200>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 2>, AvrPin<AvrPortK, 2>, 10>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 3>, AvrPin<AvrPortK, 2>, 10>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 2>, AvrPin<AvrPortK, 2>, 200>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 3>, AvrPin<AvrPortK, 2>, 200>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 0>, AvrPin<AvrPortK, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 1>, AvrPin<AvrPortK, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 2>, AvrPin<AvrPortK, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 3>, AvrPin<AvrPortK, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 4>, AvrPin<AvrPortK, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 5>, AvrPin<AvrPortK, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 6>, AvrPin<AvrPortK, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 7>, AvrPin<AvrPortK, 1>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 0>, AvrPin<AvrPortK, 2>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 1>, AvrPin<AvrPortK, 2>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 2>, AvrPin<AvrPortK, 2>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 3>, AvrPin<AvrPortK, 2>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 4>, AvrPin<AvrPortK, 2>, 1>,
        AvrAdcDifferentialInput<AvrPin<AvrPortK, 5>, AvrPin<AvrPortK, 2>, 1>
        
    >;
    
    template <int PortPinIndex>
    struct AdcPinMask<AvrPin<AvrPortF, PortPinIndex>> {
        static const MaskType Value = ((MaskType)1 << PortPinIndex);
    };
    
    template <int PortPinIndex>
    struct AdcPinMask<AvrPin<AvrPortK, PortPinIndex>> {
        static const MaskType Value = ((MaskType)1 << (8 + PortPinIndex));
    };
    
#else
#error Your device is not supported by AvrAdc
#endif
    
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    using FixedType = FixedPoint<NumValueBits, false, -NumValueBits>;
    
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
    
    static void adc_isr (AtomicContext<Context> c)
    {
        ListForEachForwardInterruptible<PinsList>([&] APRINTER_TL(pin, return pin::handle_isr(c)));
    }
    
private:
    static void start_conversion ()
    {
        ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADIE) | (AdcPrescaler << ADPS0);
    }
    
    template <int NumPins, typename Dummy = void>
    struct AdcMaybe {
        static void init (Context c)
        {
            auto *o = Object::self(c);
            
            o->m_current_pin = 0;
            o->m_finished = false;
            o->m_accumulator = 0;
            o->m_counter = 0;
            
            memory_barrier();
            
            MaskType mask = ListForEachForwardAccRes<PinsList>(0, [&] APRINTER_TLA(pin, (MaskType accum), return pin::make_pin_mask(accum)));
            DIDR0 = mask;
#ifdef DIDR2
            DIDR2 = mask >> 8;
#endif
            AdcPin<0>::configure_adc_for_pin();
            start_conversion();
            
            while (!*(volatile bool *)&o->m_finished);
        }
        
        static void deinit (Context c)
        {
            ADCSRA = 0;
            ADCSRB = 0;
            ADMUX = 0;
#ifdef DIDR2
            DIDR2 = 0;
#endif
            DIDR0 = 0;
            
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
        
        static MaskType make_pin_mask (MaskType accum)
        {
            return (accum | AdcPinMask<Pin>::Value);
        }
        
        static void configure_adc_for_pin ()
        {
#ifdef MUX5
            ADMUX = (AdcRefSel << REFS0) | (AdcIndex & 0x1F);
            ADCSRB = (AdcIndex >> 5) << MUX5;
#else
            ADMUX = (AdcRefSel << REFS0) | AdcIndex;
            ADCSRB = 0;
#endif
        }
        
        static bool handle_isr (AtomicContext<Context> c)
        {
            auto *ao = AvrAdc::Object::self(c);
            
            if (ao->m_current_pin != PinIndex) {
                return true;
            }
            handle_isr_for_this_pin(c);
            return false;
        }
        
        static void handle_isr_for_this_pin (AtomicContext<Context> c)
        {
            auto *o = Object::self(c);
            auto *ao = AvrAdc::Object::self(c);
            
            if (AdcOverSamplingBits == 0) {
                o->m_value = ADC;
            } else {
                ao->m_accumulator += ADC;
                ao->m_counter++;
                if (ao->m_counter < NumSamplesPerPin) {
                    start_conversion();
                    return;
                }
                o->m_value = ao->m_accumulator >> AdcOverSamplingBits;
                ao->m_accumulator = 0;
                ao->m_counter = 0;
            }
            
            AdcPin<NextPinIndex>::configure_adc_for_pin();
            start_conversion();
            ao->m_current_pin = NextPinIndex;
            if (PinIndex == NumPins - 1) {
                ao->m_finished = true;
            }
        }
        
        struct Object : public ObjBase<AdcPin, typename AvrAdc::Object, EmptyTypeList> {
            uint16_t m_value;
        };
    };
    
    using PinsList = IndexElemList<ParamsPinsList, AdcPin>;
    
public:
    struct Object : public ObjBase<AvrAdc, ParentObject, JoinTypeLists<
        PinsList,
        MakeTypeList<TheDebugObject>
    >> {
        uint8_t m_current_pin;
        uint16_t m_accumulator;
        uint8_t m_counter;
        bool m_finished;
    };
};

APRINTER_ALIAS_STRUCT_EXT(AvrAdcService, (
    APRINTER_AS_VALUE(int, AdcRefSel),
    APRINTER_AS_VALUE(int, AdcPrescaler),
    APRINTER_AS_VALUE(int, AdcOverSamplingBits)
), (
    APRINTER_ALIAS_STRUCT_EXT(Adc, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(PinsList)
    ), (
        using Params = AvrAdcService;
        APRINTER_DEF_INSTANCE(Adc, AvrAdc)
    ))
))

#define AMBRO_AVR_ADC_ISRS(adc, context) \
ISR(ADC_vect) \
{ \
    adc::adc_isr(MakeAtomicContext(context)); \
}

#include <aprinter/EndNamespace.h>

#endif
