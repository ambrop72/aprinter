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
#include <aprinter/meta/IndexElemList.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/FixedPoint.h>
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

template <typename Context, typename ParentObject, typename ParamsPinsList, int AdcRefSel, int AdcPrescaler>
class AvrAdc {
private:
    static const int NumPins = TypeListLength<ParamsPinsList>::Value;
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_make_pin_mask, make_pin_mask)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_handle_isr, handle_isr)
    
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
    using FixedType = FixedPoint<10, false, -10>;
    
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
        ListForEachForwardInterruptible<PinsList>(LForeach_handle_isr(), c);
    }
    
private:
    template <int NumPins, typename Dummy = void>
    struct AdcMaybe {
        static void init (Context c)
        {
            auto *o = Object::self(c);
            
            o->m_current_pin = 0;
            o->m_finished = false;
            
            memory_barrier();
            
            MaskType mask = ListForEachForwardAccRes<PinsList>(0, LForeach_make_pin_mask());
            DIDR0 = mask;
#ifdef DIDR2
            DIDR2 = mask >> 8;
#endif
#ifdef MUX5
            ADMUX = (AdcRefSel << REFS0) | (AdcPin<0>::AdcIndex & 0x1F);
            ADCSRB = (AdcPin<0>::AdcIndex >> 5) << MUX5;
#else
            ADMUX = (AdcRefSel << REFS0) | AdcPin<0>::AdcIndex;
            ADCSRB = 0;
#endif
            ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADIE) | (AdcPrescaler << ADPS0);
            
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
        
        static bool handle_isr (AtomicContext<Context> c)
        {
            auto *o = Object::self(c);
            auto *ao = AvrAdc::Object::self(c);
            
            if (ao->m_current_pin != PinIndex) {
                return true;
            }
            o->m_value = ADC;
#ifdef MUX5
            ADMUX = (AdcRefSel << REFS0) | (AdcPin<NextPinIndex>::AdcIndex & 0x1F);
            ADCSRB = (AdcPin<NextPinIndex>::AdcIndex >> 5) << MUX5;
#else
            ADMUX = (AdcRefSel << REFS0) | AdcPin<NextPinIndex>::AdcIndex;
#endif
            ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADIE) | (AdcPrescaler << ADPS0);
            ao->m_current_pin = NextPinIndex;
            if (PinIndex == NumPins - 1) {
                ao->m_finished = true;
            }
            return false;
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
        bool m_finished;
    };
};

#define AMBRO_AVR_ADC_ISRS(adc, context) \
ISR(ADC_vect) \
{ \
    adc::adc_isr(MakeAtomicContext(context)); \
}

#include <aprinter/EndNamespace.h>

#endif
