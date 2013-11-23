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

#ifndef AMBROLIB_AT91SAM3X_ADC_H
#define AMBROLIB_AT91SAM3X_ADC_H

#include <stdint.h>
#include <sam/drivers/pmc/pmc.h>

#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/IndexElemTuple.h>
#include <aprinter/meta/TypeListIndex.h>
#include <aprinter/meta/IsEqualFunc.h>
#include <aprinter/meta/TypeListLength.h>
#include <aprinter/meta/TupleForEach.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/Position.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/system/At91Sam3xPins.h>

#include <aprinter/BeginNamespace.h>

struct At91Sam3xAdcTempInput {};

template <typename TAdcFreq, uint8_t TAdcStartup, uint8_t TAdcSettling, uint8_t TAdcTracking, uint8_t TAdcTransfer>
struct At91Sam3xAdcParams {
    using AdcFreq = TAdcFreq;
    static const uint8_t AdcStartup = TAdcStartup;
    static const uint8_t AdcSettling = TAdcSettling;
    static const uint8_t AdcTracking = TAdcTracking;
    static const uint8_t AdcTransfer = TAdcTransfer;
};

template <typename Position, typename Context, typename PinsList, typename Params>
class At91Sam3xAdc
: private DebugObject<Context, void>
{
    static_assert(Params::AdcFreq::value() >= 1000000.0, "");
    static_assert(Params::AdcFreq::value() <= 20000000.0, "");
    static_assert(Params::AdcStartup < 16, "");
    static_assert(Params::AdcSettling < 4, "");
    static_assert(Params::AdcTracking < 16, "");
    static_assert(Params::AdcTransfer < 4, "");
    
    static const int32_t AdcPrescal = ((F_MCK / (2.0 * Params::AdcFreq::value())) - 1.0) + 0.5;
    static_assert(AdcPrescal >= 0, "");
    static_assert(AdcPrescal <= 255, "");
    
    static const int NumPins = TypeListLength<PinsList>::value;
    
    using AdcList = MakeTypeList<
        At91Sam3xPin<At91Sam3xPioA, 2>,
        At91Sam3xPin<At91Sam3xPioA, 3>,
        At91Sam3xPin<At91Sam3xPioA, 4>,
        At91Sam3xPin<At91Sam3xPioA, 6>,
        At91Sam3xPin<At91Sam3xPioA, 22>,
        At91Sam3xPin<At91Sam3xPioA, 23>,
        At91Sam3xPin<At91Sam3xPioA, 24>,
        At91Sam3xPin<At91Sam3xPioA, 16>,
        At91Sam3xPin<At91Sam3xPioB, 12>,
        At91Sam3xPin<At91Sam3xPioB, 13>,
        At91Sam3xPin<At91Sam3xPioB, 17>,
        At91Sam3xPin<At91Sam3xPioB, 18>,
        At91Sam3xPin<At91Sam3xPioB, 19>,
        At91Sam3xPin<At91Sam3xPioB, 20>,
        At91Sam3xPin<At91Sam3xPioB, 21>,
        At91Sam3xAdcTempInput
    >;
    
    static At91Sam3xAdc * self (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
    }
    
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_make_pin_mask, make_pin_mask)
    
public:
    static void init (Context c)
    {
        At91Sam3xAdc *o = self(c);
        AdcMaybe<NumPins>::init(c);
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        At91Sam3xAdc *o = self(c);
        o->debugDeinit(c);
        AdcMaybe<NumPins>::deinit(c);
    }
    
    template <typename Pin, typename ThisContext>
    static uint16_t getValue (ThisContext c)
    {
        At91Sam3xAdc *o = self(c);
        o->debugAccess(c);
        
        static const int AdcIndex = TypeListIndex<AdcList, IsEqualFunc<Pin>>::value;
        return ADC->ADC_CDR[AdcIndex];
    }
    
private:
    template <int NumPins, typename Dummy = void>
    struct AdcMaybe {
        static void init (Context c)
        {
            PinsTuple dummy;
            uint32_t mask = TupleForEachForwardAccRes(&dummy, 0, Foreach_make_pin_mask());
            
            pmc_enable_periph_clk(ID_ADC);
            ADC->ADC_CHDR = UINT32_MAX;
            ADC->ADC_CHER = mask;
            ADC->ADC_MR = ADC_MR_LOWRES | ADC_MR_FREERUN | ADC_MR_PRESCAL(AdcPrescal) |
                          ((uint32_t)Params::AdcStartup << ADC_MR_STARTUP_Pos) |
                          ((uint32_t)Params::AdcSettling << ADC_MR_SETTLING_Pos) |
                          ADC_MR_TRACKTIM(Params::AdcTracking) |
                          ADC_MR_TRANSFER(Params::AdcTransfer);
            ADC->ADC_CR = ADC_CR_START;
        }
        
        static void deinit (Context c)
        {
            ADC->ADC_MR = 0;
            ADC->ADC_CHDR = UINT32_MAX;
            pmc_disable_periph_clk(ID_ADC);
        }
    };
    
    template <typename Dummy>
    struct AdcMaybe<0, Dummy> {
        static void init (Context c) {}
        static void deinit (Context c) {}
    };
    
    template <int PinIndex>
    struct AdcPin {
        using Pin = TypeListGet<PinsList, PinIndex>;
        static const int AdcIndex = TypeListIndex<AdcList, IsEqualFunc<Pin>>::value;
        
        static uint32_t make_pin_mask (uint32_t accum)
        {
            return (accum | ((uint32_t)1 << AdcIndex));
        }
    };
    
    using PinsTuple = IndexElemTuple<PinsList, AdcPin>;
};

#include <aprinter/EndNamespace.h>

#endif
