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

#ifndef AMBROLIB_AT91SAM_ADC_H
#define AMBROLIB_AT91SAM_ADC_H

#include <stdint.h>
#include <sam/drivers/pmc/pmc.h>

#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/IndexElemList.h>
#include <aprinter/meta/TypeListIndex.h>
#include <aprinter/meta/IsEqualFunc.h>
#include <aprinter/meta/TypeListLength.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/Object.h>
#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/GetMemberTypeFunc.h>
#include <aprinter/meta/MapTypeList.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/TypeListFold.h>
#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/JoinTypeLists.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/system/At91SamPins.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

#if defined(__SAM3X8E__)

struct At91Sam3xAdcTempInput {};

template <
    typename TAdcFreq, uint8_t TAdcStartup, uint8_t TAdcSettling, uint8_t TAdcTracking, uint8_t TAdcTransfer,
    typename TAvgParams
>
struct At91Sam3xAdcParams {
    using AdcFreq = TAdcFreq;
    static const uint8_t AdcStartup = TAdcStartup;
    static const uint8_t AdcSettling = TAdcSettling;
    static const uint8_t AdcTracking = TAdcTracking;
    static const uint8_t AdcTransfer = TAdcTransfer;
    using AvgParams = TAvgParams;
};

#elif defined(__SAM3U4E__)

template <
    typename TAdcFreq, uint8_t TAdcStartup, uint8_t TAdcShtim,
    typename TAvgParams
>
struct At91Sam3uAdcParams {
    using AdcFreq = TAdcFreq;
    static const uint8_t AdcStartup = TAdcStartup;
    static const uint8_t AdcShtim = TAdcShtim;
    using AvgParams = TAvgParams;
};

#else
#error "Unsupported device."
#endif

struct At91SamAdcNoAvgParams {
    static const bool Enabled = false;
};

template <
    typename TAvgInterval
>
struct At91SamAdcAvgParams {
    static const bool Enabled = true;
    using AvgInterval = TAvgInterval;
};

template <typename TPin, uint16_t TSmoothFactor>
struct At91SamAdcSmoothPin {};

template <typename Context, typename ParentObject, typename ParamsPinsList, typename Params>
class At91SamAdc {
#if defined(__SAM3X8E__)
    static_assert(Params::AdcFreq::value() >= 1000000.0, "");
    static_assert(Params::AdcFreq::value() <= 20000000.0, "");
    static_assert(Params::AdcStartup < 16, "");
    static_assert(Params::AdcSettling < 4, "");
    static_assert(Params::AdcTracking < 16, "");
    static_assert(Params::AdcTransfer < 4, "");
#elif defined(__SAM3U4E__)
    static_assert(Params::AdcFreq::value() >= 1000000.0, "");
    static_assert(Params::AdcFreq::value() <= 20000000.0, "");
    static_assert(Params::AdcStartup < 256, "");
    static_assert(Params::AdcShtim < 16, "");
#endif
    
    static const int32_t AdcPrescal = ((F_MCK / (2.0 * Params::AdcFreq::value())) - 1.0) + 0.5;
    static_assert(AdcPrescal >= 0, "");
    static_assert(AdcPrescal <= 255, "");
    
    static const int NumPins = TypeListLength<ParamsPinsList>::value;
    
#if defined(__SAM3X8E__)
    using AdcList = MakeTypeList<
        At91SamPin<At91SamPioA, 2>,
        At91SamPin<At91SamPioA, 3>,
        At91SamPin<At91SamPioA, 4>,
        At91SamPin<At91SamPioA, 6>,
        At91SamPin<At91SamPioA, 22>,
        At91SamPin<At91SamPioA, 23>,
        At91SamPin<At91SamPioA, 24>,
        At91SamPin<At91SamPioA, 16>,
        At91SamPin<At91SamPioB, 12>,
        At91SamPin<At91SamPioB, 13>,
        At91SamPin<At91SamPioB, 17>,
        At91SamPin<At91SamPioB, 18>,
        At91SamPin<At91SamPioB, 19>,
        At91SamPin<At91SamPioB, 20>,
        At91SamPin<At91SamPioB, 21>,
        At91Sam3xAdcTempInput
    >;
#elif defined(__SAM3U4E__)
    using AdcList = MakeTypeList<
        At91SamPin<At91SamPioA, 22>,
        At91SamPin<At91SamPioA, 30>,
        At91SamPin<At91SamPioB, 3>,
        At91SamPin<At91SamPioB, 4>,
        At91SamPin<At91SamPioC, 15>,
        At91SamPin<At91SamPioC, 16>,
        At91SamPin<At91SamPioC, 17>,
        At91SamPin<At91SamPioC, 18>
    >;
#endif
    
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_init, init)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_make_pin_mask, make_pin_mask)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_calc_avg, calc_avg)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_Pin, Pin)
    
    AMBRO_STRUCT_IF(AvgFeature, Params::AvgParams::Enabled) {
        struct Object;
        using Clock = typename Context::Clock;
        using TimeType = typename Clock::TimeType;
        static TimeType const Interval = Params::AvgParams::AvgInterval::value() / Clock::time_unit;
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            o->m_next = Clock::getTime(c) + Interval;
        }
        
        static void work (InterruptContext<Context> c)
        {
            auto *o = Object::self(c);
            if ((TimeType)(Clock::getTime(c) - o->m_next) < UINT32_C(0x80000000)) {
                ListForEachForward<PinsList>(LForeach_calc_avg(), c);
                o->m_next += Interval;
            }
        }
        
        struct Object : public ObjBase<AvgFeature, typename At91SamAdc::Object, EmptyTypeList> {
            TimeType m_next;
        };
    } AMBRO_STRUCT_ELSE(AvgFeature) {
        static void init (Context c) {}
        static void work (InterruptContext<Context> c) {}
        struct Object {};
    };
    
public:
    struct Object;
    using FixedType = FixedPoint<12, false, -12>;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        if (NumPins > 0) {
            AvgFeature::init(c);
            ListForEachForward<PinsList>(LForeach_init(), c);
#if defined(__SAM3X8E__)
            pmc_enable_periph_clk(ID_ADC);
            ADC->ADC_CHDR = UINT32_MAX;
            ADC->ADC_CHER = ListForEachForwardAccRes<PinsList>(0, LForeach_make_pin_mask());
            ADC->ADC_MR = ADC_MR_PRESCAL(AdcPrescal) |
                          ((uint32_t)Params::AdcStartup << ADC_MR_STARTUP_Pos) |
                          ((uint32_t)Params::AdcSettling << ADC_MR_SETTLING_Pos) |
                          ADC_MR_TRACKTIM(Params::AdcTracking) |
                          ADC_MR_TRANSFER(Params::AdcTransfer);
            ADC->ADC_IDR = UINT32_MAX;
            NVIC_ClearPendingIRQ(ADC_IRQn);
            NVIC_SetPriority(ADC_IRQn, INTERRUPT_PRIORITY);
            NVIC_EnableIRQ(ADC_IRQn);
            ADC->ADC_IER = (uint32_t)1 << MaxAdcIndex;
            ADC->ADC_CR = ADC_CR_START;
#elif defined(__SAM3U4E__)
            pmc_enable_periph_clk(ID_ADC12B);
            ADC12B->ADC12B_CHDR = UINT32_MAX;
            ADC12B->ADC12B_CHER = ListForEachForwardAccRes<PinsList>(0, LForeach_make_pin_mask());
            ADC12B->ADC12B_MR = ADC12B_MR_PRESCAL(AdcPrescal) |
                                ADC12B_MR_STARTUP(Params::AdcStartup) | ADC12B_MR_SHTIM(Params::AdcShtim);
            ADC12B->ADC12B_IDR = UINT32_MAX;
            NVIC_ClearPendingIRQ(ADC12B_IRQn);
            NVIC_SetPriority(ADC12B_IRQn, INTERRUPT_PRIORITY);
            NVIC_EnableIRQ(ADC12B_IRQn);
            ADC12B->ADC12B_IER = (uint32_t)1 << MaxAdcIndex;
            ADC12B->ADC12B_CR = ADC12B_CR_START;
#endif
            
        }
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->debugDeinit(c);
        if (NumPins > 0) {
#if defined(__SAM3X8E__)
            NVIC_DisableIRQ(ADC_IRQn);
            ADC->ADC_IDR = UINT32_MAX;
            NVIC_ClearPendingIRQ(ADC_IRQn);
            ADC->ADC_MR = 0;
            ADC->ADC_CHDR = UINT32_MAX;
            pmc_disable_periph_clk(ID_ADC);
#elif defined(__SAM3U4E__)
            NVIC_DisableIRQ(ADC12B_IRQn);
            ADC12B->ADC12B_IDR = UINT32_MAX;
            NVIC_ClearPendingIRQ(ADC12B_IRQn);
            ADC12B->ADC12B_MR = 0;
            ADC12B->ADC12B_CHDR = UINT32_MAX;
            pmc_disable_periph_clk(ID_ADC12B);
#endif
        }
    }
    
    template <typename Pin, typename ThisContext>
    static FixedType getValue (ThisContext c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        static int const PinIndex = TypeListIndex<FlatPinsList, IsEqualFunc<Pin>>::value;
        return FixedType::importBits(AdcPin<PinIndex>::get_value(c));
    }
    
    static void adc_isr (InterruptContext<Context> c)
    {
        AvgFeature::work(c);
#if defined(__SAM3X8E__)
        ADC->ADC_CDR[MaxAdcIndex];
        ADC->ADC_CR = ADC_CR_START;
#elif defined(__SAM3U4E__)
        ADC12B->ADC12B_CDR[MaxAdcIndex];
        ADC12B->ADC12B_CR = ADC12B_CR_START;
#endif
    }
    
private:
    template <int PinIndex>
    struct AdcPin {
        struct Object;
        template <typename TheListPin> struct Helper;
        
        using ListPin = TypeListGet<ParamsPinsList, PinIndex>;
        using TheHelper = Helper<ListPin>;
        
        static uint32_t make_pin_mask (uint32_t accum)
        {
            return (accum | ((uint32_t)1 << AdcIndex));
        }
        
        static void init (Context c)
        {
            return TheHelper::init(c);
        }
        
        template <typename ThisContext>
        static void calc_avg (ThisContext c)
        {
            return TheHelper::calc_avg(c);
        }
        
        template <typename ThisContext>
        static uint16_t get_value (ThisContext c)
        {
            return TheHelper::get_value(c);
        }
        
        template <typename TheListPin>
        struct Helper {
            using RealPin = TheListPin;
            static void init (Context c) {}
            template <typename ThisContext>
            static void calc_avg (ThisContext c) {}
            
            template <typename ThisContext>
            static uint16_t get_value (ThisContext c)
            {
#if defined(__SAM3X8E__)
                return ADC->ADC_CDR[AdcIndex];
#elif defined(__SAM3U4E__)
                return ADC12B->ADC12B_CDR[AdcIndex];
#endif
            }
            
            struct Object {};
        };
        
        template <typename ThePin, uint16_t TheSmoothFactor>
        struct Helper<At91SamAdcSmoothPin<ThePin, TheSmoothFactor>> {
            struct Object;
            using RealPin = ThePin;
            static_assert(TheSmoothFactor > 0, "");
            static_assert(TheSmoothFactor < 65536, "");
            static_assert(Params::AvgParams::Enabled, "");
            
            static void init (Context c)
            {
                auto *o = Object::self(c);
                o->m_state = 0;
            }
            
            template <typename ThisContext>
            static void calc_avg (ThisContext c)
            {
                auto *o = Object::self(c);
                uint16_t cdr;
#if defined(__SAM3X8E__)
                cdr = ADC->ADC_CDR[AdcIndex];
#elif defined(__SAM3U4E__)
                cdr = ADC12B->ADC12B_CDR[AdcIndex];
#endif
                uint32_t new_state = (((uint64_t)(65536 - TheSmoothFactor) * ((uint32_t)cdr << 16)) + ((uint64_t)TheSmoothFactor * o->m_state)) >> 16;
                AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                    o->m_state = new_state;
                }
            }
            
            template <typename ThisContext>
            static uint16_t get_value (ThisContext c)
            {
                auto *o = Object::self(c);
                uint32_t value;
                AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                    value = o->m_state;
                }
                return ((value >> 16) + ((value >> 15) & 1));
            }
            
            struct Object : public ObjBase<TheHelper, typename AdcPin::Object, EmptyTypeList> {
                uint32_t m_state;
            };
        };
        
        using Pin = typename TheHelper::RealPin;
        static int const AdcIndex = TypeListIndex<AdcList, IsEqualFunc<Pin>>::value;
        
        struct Object : public ObjBase<AdcPin, typename At91SamAdc::Object, MakeTypeList<
            TheHelper
        >> {};
    };
    
    using PinsList = IndexElemList<ParamsPinsList, AdcPin>;
    using FlatPinsList = MapTypeList<PinsList, GetMemberType_Pin>;
    
    template <typename ListElem, typename AccumValue>
    using MaxAdcIndexFoldFunc = WrapInt<max(AccumValue::value, ListElem::AdcIndex)>;
    static int const MaxAdcIndex = TypeListFold<PinsList, WrapInt<0>, MaxAdcIndexFoldFunc>::value;
    
public:
    struct Object : public ObjBase<At91SamAdc, ParentObject, JoinTypeLists<
        PinsList,
        MakeTypeList<
            AvgFeature
        >
    >>,
        public DebugObject<Context, void>
    {};
};


#if defined(__SAM3X8E__)

#define AMBRO_AT91SAM3X_ADC_GLOBAL(adc, context) \
extern "C" \
__attribute__((used)) \
void ADC_Handler (void) \
{ \
    adc::adc_isr(MakeInterruptContext(context)); \
}

#elif defined(__SAM3U4E__)

#define AMBRO_AT91SAM3U_ADC_GLOBAL(adc, context) \
extern "C" \
__attribute__((used)) \
void ADC12B_Handler (void) \
{ \
    adc::adc_isr(MakeInterruptContext(context)); \
}

#endif

#include <aprinter/EndNamespace.h>

#endif
