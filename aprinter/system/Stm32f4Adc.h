/*
 * Copyright (c) 2015 Ambroz Bizjak
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

#ifndef AMBROLIB_STM32F4_ADC_H
#define AMBROLIB_STM32F4_ADC_H

#include <stdint.h>

#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/IndexElemList.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/TypeDict.h>
#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/MemberType.h>
#include <aprinter/meta/If.h>
#include <aprinter/meta/FuncUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/system/Stm32f4Pins.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

#define STM32F4ADC_DEFINE_SUBADC(AdcDefName, AdcNumber, DmaNumber, DmaStreamNumber, DmaChannelNumber) \
struct AdcDefName { \
    using Number = WrapInt<AdcNumber>; \
    static void adc_clk_enable () { __HAL_RCC_ADC##AdcNumber##_CLK_ENABLE(); } \
    static void adc_clk_disable () { __HAL_RCC_ADC##AdcNumber##_CLK_DISABLE(); } \
    static ADC_TypeDef * adc () { return ADC##AdcNumber; } \
    static void dma_clk_enable () { __HAL_RCC_DMA##DmaNumber##_CLK_ENABLE(); } \
    static DMA_Stream_TypeDef * dma_stream () { return DMA##DmaNumber##_Stream##DmaStreamNumber; } \
    static uint32_t const DmaChannelSelection = DMA_CHANNEL_##DmaChannelNumber; \
};

template <typename Context, typename ParentObject, typename ParamsPinsList, int ClockDivider, int SampleTimeSelection>
class Stm32f4Adc {
    static_assert(ClockDivider == 2 || ClockDivider == 4 || ClockDivider == 6 || ClockDivider == 8, "");
    static_assert(SampleTimeSelection >= 0 && SampleTimeSelection <= 7, "");
    
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_init, init)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_deinit, deinit)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_start, start)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_handle_irq, handle_irq)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_Number, Number)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_NumAdcPinsWrapped, NumAdcPinsWrapped)
    
    template <typename TAdcList, int TInputNumber>
    struct AdcMapping {
        using AdcList = TAdcList;
        static int const InputNumber = TInputNumber;
    };
    
    template <int...Numbers>
    using AdcNumbers = MakeTypeList<WrapInt<Numbers>...>;
    
#if defined(STM32F429xx) || defined(STM32F407xx)
    STM32F4ADC_DEFINE_SUBADC(AdcDef1, 1, 2, 0, 0)
    STM32F4ADC_DEFINE_SUBADC(AdcDef2, 2, 2, 2, 1)
    STM32F4ADC_DEFINE_SUBADC(AdcDef3, 3, 2, 1, 2)
    using AdcDefList = MakeTypeList<AdcDef1, AdcDef2, AdcDef3>;
    using PinDefList = MakeTypeList<
        TypeDictEntry<Stm32f4Pin<Stm32f4PortF, 3>,  AdcMapping<AdcNumbers<3>,     9>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortF, 4>,  AdcMapping<AdcNumbers<3>,     14>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortF, 5>,  AdcMapping<AdcNumbers<3>,     15>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortF, 6>,  AdcMapping<AdcNumbers<3>,     4>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortF, 7>,  AdcMapping<AdcNumbers<3>,     5>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortF, 8>,  AdcMapping<AdcNumbers<3>,     6>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortF, 9>,  AdcMapping<AdcNumbers<3>,     7>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortF, 10>, AdcMapping<AdcNumbers<3>,     8>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortC, 0>,  AdcMapping<AdcNumbers<1,2,3>, 10>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortC, 1>,  AdcMapping<AdcNumbers<1,2,3>, 11>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortC, 2>,  AdcMapping<AdcNumbers<1,2,3>, 12>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortC, 3>,  AdcMapping<AdcNumbers<1,2,3>, 13>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortA, 0>,  AdcMapping<AdcNumbers<1,2,3>, 0>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortA, 1>,  AdcMapping<AdcNumbers<1,2,3>, 1>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortA, 2>,  AdcMapping<AdcNumbers<1,2,3>, 2>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortA, 3>,  AdcMapping<AdcNumbers<1,2,3>, 3>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortA, 4>,  AdcMapping<AdcNumbers<1,2>,   4>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortA, 5>,  AdcMapping<AdcNumbers<1,2>,   5>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortA, 6>,  AdcMapping<AdcNumbers<1,2>,   6>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortA, 7>,  AdcMapping<AdcNumbers<1,2>,   7>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortC, 4>,  AdcMapping<AdcNumbers<1,2>,   14>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortC, 5>,  AdcMapping<AdcNumbers<1,2>,   15>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortB, 0>,  AdcMapping<AdcNumbers<1,2>,   8>>,
        TypeDictEntry<Stm32f4Pin<Stm32f4PortB, 1>,  AdcMapping<AdcNumbers<1,2>,   9>>
    >;
#else
#error Chip not supported by Stm32f4Adc
#endif
    
    template <typename AdcNumber>
    using GetAdcIndexForNumber = TypeListIndexMapped<AdcDefList, GetMemberType_Number, AdcNumber>;
    
    static int const MaxPinsPerAdc = 16;
    static int const AdcPrescalerCode = (ClockDivider / 2) - 1;
    
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
    // We assign the requested pins (ParamsPinsList) to the ADCs in a greedy manner.
    // This means that we go through the pins in order, and assign each pin to the
    // first ADC which can use this pin as input and does not yet have all 16 inputs
    // assigned.
    // Sometime we may want to extend this to allow the user to specify the
    // assignments. Because the greedy algorithm mail fail to assign all pins even
    // though a solution exists.
    
    template <int PinIndex, typename Dummy=void>
    struct AssignPin {
        using PinDef = TypeListGet<ParamsPinsList, PinIndex>;
        using PrevAssignments = typename AssignPin<(PinIndex - 1)>::Assignments;
        using PinMappingFindRes = TypeDictFind<PinDefList, PinDef>;
        static_assert(PinMappingFindRes::Found, "Invalid pin specified for ADC");
        using PinMapping = typename PinMappingFindRes::Result;
        using PossibleAdcIndices = MapTypeList<typename PinMapping::AdcList, TemplateFunc<GetAdcIndexForNumber>>;
        template <typename AdcIndex> using CanUseAdc = WrapBool<(TypeListLength<TypeListGet<PrevAssignments, AdcIndex::Value>>::Value < MaxPinsPerAdc)>;
        using AssignFindRes = TypeListFindMapped<PossibleAdcIndices, TemplateFunc<CanUseAdc>, WrapBool<true>>;
        static_assert(AssignFindRes::Found, "No ADC available to assign pin to");
        static int const AssignedAdcIndex = TypeListGet<PossibleAdcIndices, AssignFindRes::Result::Value>::Value;
        template <int AdcIndex> using AssignmentsForAdc = If<
            (AdcIndex == AssignedAdcIndex),
            ConsTypeList<WrapInt<PinIndex>, TypeListGet<PrevAssignments, AdcIndex>>,
            TypeListGet<PrevAssignments, AdcIndex>
        >;
        using Assignments = IndexElemList<AdcDefList, AssignmentsForAdc>;
    };
    
    template <typename Dummy>
    struct AssignPin<-1, Dummy> {
        template <int AdcIndex> using AssignmentsForAdc = EmptyTypeList;
        using Assignments = IndexElemList<AdcDefList, AssignmentsForAdc>;
    };
    
    using PinAssignments = typename AssignPin<(TypeListLength<ParamsPinsList>::Value - 1)>::Assignments;
    
    template <int AdcIndex>
    struct Adc {
        using AdcDef = TypeListGet<AdcDefList, AdcIndex>;
        using AssignedPinIndices = TypeListGet<PinAssignments, AdcIndex>;
        static int const NumAdcPins = TypeListLength<AssignedPinIndices>::Value;
        static_assert(NumAdcPins <= MaxPinsPerAdc, "");
        using NumAdcPinsWrapped = WrapInt<NumAdcPins>;
        
        static void init (Context c)
        {
            auto *o = Object::self(c);
            
            AdcDef::dma_clk_enable();
            AdcDef::adc_clk_enable();
            
            o->dma = DMA_HandleTypeDef();
            o->dma.Instance = AdcDef::dma_stream();
            o->dma.Init.Channel = AdcDef::DmaChannelSelection;
            o->dma.Init.Direction = DMA_PERIPH_TO_MEMORY;
            o->dma.Init.PeriphInc = DMA_PINC_DISABLE;
            o->dma.Init.MemInc = DMA_MINC_ENABLE;
            o->dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
            o->dma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
            o->dma.Init.Mode = DMA_CIRCULAR;
            o->dma.Init.Priority = DMA_PRIORITY_HIGH;
            o->dma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
            if (HAL_DMA_Init(&o->dma) != HAL_OK) {
                AMBRO_ASSERT_ABORT("HAL_DMA_Init failed");
            }
            
            AdcDef::adc()->CR1 = ADC_CR1_SCAN | ADC_CR1_OVRIE;
            AdcDef::adc()->CR2 = ADC_CR2_CONT | ADC_CR2_DMA | ADC_CR2_DDS | ADC_CR2_EOCS;
            AdcDef::adc()->SMPR1 = 0;
            AdcDef::adc()->SMPR2 = 0;
            AdcDef::adc()->SQR1 = (uint32_t)(NumAdcPins - 1) << 20;
            AdcDef::adc()->SQR2 = 0;
            AdcDef::adc()->SQR3 = 0;
            AdcDef::adc()->JSQR = 0;
            
            ListForEachForward<AdcPinList>(LForeach_init(), c);
            
            AdcDef::adc()->CR2 |= ADC_CR2_ADON;
        }
        
        static void start (Context c)
        {
            auto *o = Object::self(c);
            
            HAL_DMA_Abort(&o->dma);
            
            if (HAL_DMA_Start(&o->dma, (uint32_t)&AdcDef::adc()->DR, (uint32_t)&o->adc_values, NumAdcPins) != HAL_OK) {
                AMBRO_ASSERT_ABORT("HAL_DMA_Start failed");
            }
            
            AdcDef::adc()->SR &= ~ADC_SR_OVR;
            AdcDef::adc()->CR2 |= ADC_CR2_SWSTART;
        }
        
        static void deinit (Context c)
        {
            auto *o = Object::self(c);
            
            AdcDef::adc()->CR1 = 0;
            AdcDef::adc()->CR2 = 0;
            
            HAL_DMA_Abort(&o->dma);
            HAL_DMA_DeInit(&o->dma);
            
            AdcDef::adc_clk_disable();
        }
        
        static void handle_irq (InterruptContext<Context> c)
        {
            if (AdcDef::adc()->SR & ADC_SR_OVR) {
                start(c);
            }
        }
        
        template <int AdcPinIndex>
        struct AdcPin {
            static int const PinIndex = TypeListGet<AssignedPinIndices, AdcPinIndex>::Value;
            using PinDef = TypeListGet<ParamsPinsList, PinIndex>;
            using TheAssignPin = AssignPin<PinIndex>;
            static int const SqrIndex = AdcPinIndex / 6;
            static int const SqrOffset = AdcPinIndex % 6;
            static int const SmprIndex = AdcPinIndex / 10;
            static int const SmprOffset = AdcPinIndex % 10;
            
            static void init (Context c)
            {
                Object::self(c)->adc_values[AdcPinIndex] = 0;
                
                Context::Pins::template setAnalog<PinDef>(c);
                
                *sqr_reg() |= (uint32_t)TheAssignPin::PinMapping::InputNumber << (5 * SqrOffset);
                *smpr_reg() |= (uint32_t)SampleTimeSelection << (3 * SmprOffset);
            }
            
            static uint32_t volatile * sqr_reg ()
            {
                return SqrIndex == 0 ? &AdcDef::adc()->SQR3 :
                       SqrIndex == 1 ? &AdcDef::adc()->SQR2 :
                       SqrIndex == 2 ? &AdcDef::adc()->SQR1 : nullptr;
            }
            
            static uint32_t volatile * smpr_reg ()
            {
                return SmprIndex == 0 ? &AdcDef::adc()->SMPR2 :
                       SmprIndex == 1 ? &AdcDef::adc()->SMPR1 : nullptr;
            }
        };
        using AdcPinList = IndexElemList<AssignedPinIndices, AdcPin>;
        
        struct Object : public ObjBase<Adc, typename Stm32f4Adc::Object, EmptyTypeList> {
            DMA_HandleTypeDef dma;
            uint16_t adc_values[NumAdcPins];
        };
    };
    using AllAdcList = IndexElemList<AdcDefList, Adc>;
    
    using UsedAdcList = FilterTypeList<
        AllAdcList,
        ComposeFunctions<
            NotFunc,
            ComposeFunctions<
                IsEqualFunc<WrapInt<0>>,
                GetMemberType_NumAdcPinsWrapped
            >
        >
    >;
    
public:
    using FixedType = FixedPoint<12, false, -12>;
    
    static void init (Context c)
    {
        ADC->CCR = ((uint32_t)AdcPrescalerCode << 16);
        
        ListForEachForward<UsedAdcList>(LForeach_init(), c);
        
        NVIC_ClearPendingIRQ(ADC_IRQn);
        NVIC_SetPriority(ADC_IRQn, INTERRUPT_PRIORITY);
        
        ListForEachForward<UsedAdcList>(LForeach_start(), c);
        
        memory_barrier();
        NVIC_EnableIRQ(ADC_IRQn);
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
        
        NVIC_DisableIRQ(ADC_IRQn);
        memory_barrier();
        
        ListForEachForward<UsedAdcList>(LForeach_deinit(), c);
        
        NVIC_ClearPendingIRQ(ADC_IRQn);
    }
    
    template <typename Pin, typename ThisContext>
    static FixedType getValue (ThisContext c)
    {
        TheDebugObject::access(c);
        
        static int const PinIndex = TypeListIndex<ParamsPinsList, Pin>::Value;
        using PinAdc = Adc<AssignPin<PinIndex>::AssignedAdcIndex>;
        static int const AdcPinIndex = TypeListIndex<typename PinAdc::AssignedPinIndices, WrapInt<PinIndex>>::Value;
        auto *adc_o = PinAdc::Object::self(c);
        memory_barrier_dma();
        return FixedType::importBits(((uint16_t volatile *)adc_o->adc_values)[AdcPinIndex]);
    }
    
    static void handle_irq (InterruptContext<Context> c)
    {
        ListForEachForward<UsedAdcList>(LForeach_handle_irq(), c);
    }
    
public:
    struct Object : public ObjBase<Stm32f4Adc, ParentObject, JoinTypeLists<
        UsedAdcList,
        MakeTypeList<TheDebugObject>
    >> {};
};

#define APRINTER_STM32F4_ADC_GLOBAL(adc, context) \
extern "C" \
__attribute__((used)) \
void ADC_IRQHandler (void) \
{ \
    adc::handle_irq(MakeInterruptContext(context)); \
}

#include <aprinter/EndNamespace.h>

#endif
