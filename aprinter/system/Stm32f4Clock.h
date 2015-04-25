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

#ifndef AMBROLIB_STM32F4_CLOCK_H
#define AMBROLIB_STM32F4_CLOCK_H

#include <stdint.h>
#include <stddef.h>

#include <aprinter/base/Object.h>
#include <aprinter/base/JoinTokens.h>
#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/IndexElemList.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/WrapDouble.h>
#include <aprinter/meta/TypeDict.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/InterruptLock.h>

template <typename TcSpec, typename Comp>
struct Stm32f4Clock__IrqCompHelper {
    static void call () {}
};

#include <aprinter/BeginNamespace.h>

using Stm32f4ClockDefaultExtraClearance = AMBRO_WRAP_DOUBLE(0.0);

#define STM32F4CLOCK_DEFINE_TC(tc_num, is_32bit, clock_type) \
struct Stm32f4ClockTIM##tc_num { \
    static bool const Is32Bit = is_32bit; \
    static TIM_TypeDef * tim () { return TIM##tc_num; } \
    static int const ClockType = clock_type; \
    static void enable_clock () { __HAL_RCC_TIM##tc_num##_CLK_ENABLE(); } \
    static void disable_clock () { __HAL_RCC_TIM##tc_num##_CLK_DISABLE(); } \
    static IRQn_Type const Irq = APRINTER_JOIN(STM32F4CLOCK_IRQ_FOR_TIM##tc_num, IRQn); \
};

// Note: these have _ at the end so that TIMn macros don't get in the way.
#define STM32F4CLOCK_IRQ_FOR_TIM1 TIM1_CC_
#define STM32F4CLOCK_IRQ_FOR_TIM2 TIM2_
#define STM32F4CLOCK_IRQ_FOR_TIM3 TIM3_
#define STM32F4CLOCK_IRQ_FOR_TIM4 TIM4_
#define STM32F4CLOCK_IRQ_FOR_TIM5 TIM5_
#define STM32F4CLOCK_IRQ_FOR_TIM8 TIM8_CC_
#define STM32F4CLOCK_IRQ_FOR_TIM9 TIM1_BRK_TIM9_
#define STM32F4CLOCK_IRQ_FOR_TIM10 TIM1_UP_TIM10_
#define STM32F4CLOCK_IRQ_FOR_TIM11 TIM1_TRG_COM_TIM11_
#define STM32F4CLOCK_IRQ_FOR_TIM12 TIM8_BRK_TIM12_
#define STM32F4CLOCK_IRQ_FOR_TIM13 TIM8_UP_TIM13_
#define STM32F4CLOCK_IRQ_FOR_TIM14 TIM8_TRG_COM_TIM14_

// For these commented-out timers, the clock enable/disable macros are missing in stm32f4xx_hal_rcc.h.
STM32F4CLOCK_DEFINE_TC(1,  false, 2)
STM32F4CLOCK_DEFINE_TC(2,  true,  1)
STM32F4CLOCK_DEFINE_TC(3,  false, 1)
STM32F4CLOCK_DEFINE_TC(4,  false, 1)
STM32F4CLOCK_DEFINE_TC(5,  true,  1)
//STM32F4CLOCK_DEFINE_TC(8,  false, 2)
STM32F4CLOCK_DEFINE_TC(9,  false, 2)
STM32F4CLOCK_DEFINE_TC(10, false, 2)
STM32F4CLOCK_DEFINE_TC(11, false, 2)
//STM32F4CLOCK_DEFINE_TC(12, false, 1)
//STM32F4CLOCK_DEFINE_TC(13, false, 1)
//STM32F4CLOCK_DEFINE_TC(14, false, 1)

#define STM32F4CLOCK_DEFINE_COMP(comp_num, ccmr_num, ccmr_bit_offset, ccer_bit_offset, ccie_bit) \
struct Stm32f4ClockComp##comp_num { \
    static uint32_t volatile * ccmr (TIM_TypeDef *tim) { return &tim->CCMR##ccmr_num; } \
    static int const CcmrBitOffset = ccmr_bit_offset; \
    static int const CcerBitOffset = ccer_bit_offset; \
    static uint32_t volatile * ccr (TIM_TypeDef *tim) { return &tim->CCR##comp_num; } \
    static uint32_t const CcieBit = ccie_bit; \
};

STM32F4CLOCK_DEFINE_COMP(1, 1, 0, 0,  TIM_DIER_CC1IE)
STM32F4CLOCK_DEFINE_COMP(2, 1, 8, 4,  TIM_DIER_CC2IE)
STM32F4CLOCK_DEFINE_COMP(3, 2, 0, 8,  TIM_DIER_CC3IE)
STM32F4CLOCK_DEFINE_COMP(4, 2, 8, 12, TIM_DIER_CC4IE)

template <typename Context, typename ParentObject, uint16_t Prescale, typename ParamsTcsList>
class Stm32f4Clock {
    static_assert(TypeListLength<ParamsTcsList>::Value > 0, "Need at least one timer.");
    static_assert(TypeListGet<ParamsTcsList, 0>::Is32Bit, "First timer must be 32-bit.");
    static_assert(TypeListGet<ParamsTcsList, 0>::ClockType == 1, "First timer must be APB1-clocked");
    
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_init, init)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_deinit, deinit)
    
public:
    struct Object;
    using TimeType = uint32_t;
    
    static constexpr TimeType prescale_divide = (TimeType)Prescale + 1;
    static constexpr double apb1_timers_freq = F_CPU / APB1_TIMERS_DIV;
    
    static constexpr double time_unit = (double)prescale_divide / apb1_timers_freq;
    static constexpr double time_freq = (double)apb1_timers_freq / prescale_divide;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
    template <int TTcIndex>
    struct MyTc {
        static int const TcIndex = TTcIndex;
        using TcSpec = TypeListGet<ParamsTcsList, TcIndex>;
        
        static void init (Context c)
        {
            TcSpec::enable_clock();
            TcSpec::tim()->CR1 = 0;
            TcSpec::tim()->CR2 = 0;
            TcSpec::tim()->SMCR = 0;
            TcSpec::tim()->DIER = 0;
            TcSpec::tim()->SR = 0;
            TcSpec::tim()->CCMR1 = 0;
            TcSpec::tim()->CCMR2 = 0;
            TcSpec::tim()->CCER = 0;
            TcSpec::tim()->PSC = Prescale;
            TcSpec::tim()->ARR = TcSpec::Is32Bit ? UINT32_MAX : UINT16_MAX;
            TcSpec::tim()->EGR = TIM_EGR_UG;
            TcSpec::tim()->CNT = (TcIndex == 1);
            TcSpec::tim()->CR1 = TIM_CR1_CEN;
            NVIC_ClearPendingIRQ(TcSpec::Irq);
            NVIC_SetPriority(TcSpec::Irq, INTERRUPT_PRIORITY);
            NVIC_EnableIRQ(TcSpec::Irq);
        }
        
        static void deinit (Context c)
        {
            NVIC_DisableIRQ(TcSpec::Irq);
            TcSpec::tim()->CR1 = 0;
            TcSpec::tim()->SR = 0;
            NVIC_ClearPendingIRQ(TcSpec::Irq);
            TcSpec::disable_clock();
        }
        
        static void irq_handler (InterruptContext<Context> c)
        {
            Stm32f4Clock__IrqCompHelper<TcSpec, Stm32f4ClockComp1>::call();
            Stm32f4Clock__IrqCompHelper<TcSpec, Stm32f4ClockComp2>::call();
            Stm32f4Clock__IrqCompHelper<TcSpec, Stm32f4ClockComp3>::call();
            Stm32f4Clock__IrqCompHelper<TcSpec, Stm32f4ClockComp4>::call();
        }
    };
    
    using MyTcsList = IndexElemList<ParamsTcsList, MyTc>;
    
    template <typename TcSpec>
    using FindTc = MyTc<TypeListIndex<ParamsTcsList, TcSpec>::Value>;
    
public:
    static void init (Context c)
    {
        ListForEachForward<MyTcsList>(LForeach_init(), c);
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
        
        ListForEachReverse<MyTcsList>(LForeach_deinit(), c);
    }
    
    template <typename ThisContext>
    static TimeType getTime (ThisContext c)
    {
        TheDebugObject::access(c);
        
        return MyTc<0>::TcSpec::tim()->CNT;
    }
    
    template <typename TcSpec>
    static void tc_irq_handler (InterruptContext<Context> c)
    {
        FindTc<TcSpec>::irq_handler(c);
    }
    
public:
    struct Object : public ObjBase<Stm32f4Clock, ParentObject, MakeTypeList<TheDebugObject>> {};
};

#define AMBRO_STM32F4_CLOCK_TC_GLOBAL(tc_num, clock, context) \
extern "C" \
__attribute__((used)) \
void APRINTER_JOIN(STM32F4CLOCK_IRQ_FOR_TIM##tc_num, Handler) (void) \
{ \
    clock::tc_irq_handler<Stm32f4ClockTIM##tc_num>(MakeInterruptContext((context))); \
}

template <typename Context, typename ParentObject, typename Handler, typename TTcSpec, typename TComp, typename ExtraClearance>
class Stm32f4ClockInterruptTimer {
public:
    struct Object;
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using HandlerContext = InterruptContext<Context>;
    using TcSpec = TTcSpec;
    using Comp = TComp;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::init(c);
        
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = false;
#endif
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            TcSpec::tim()->DIER &= ~Comp::CcieBit;
        }
        
        memory_barrier();
    }
    
    template <typename ThisContext>
    AMBRO_OPTIMIZE_SPEED
    static void setFirst (ThisContext c, TimeType time)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(!o->m_running)
        AMBRO_ASSERT(!(TcSpec::tim()->DIER & Comp::CcieBit))
        
        o->m_time = time;
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = true;
#endif
        memory_barrier();
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            TimeType now = Clock::template MyTc<0>::TcSpec::tim()->CNT;
            now -= time;
            now += clearance;
            if (now < UINT32_C(0x80000000)) {
                time += now;
            }
            *ccr_reg() = time;
            *ccmr_reg() = (*ccmr_reg() & ~((uint32_t)255 << Comp::CcmrBitOffset)) | ((uint32_t)0 << Comp::CcmrBitOffset);
            TcSpec::tim()->CCER = (TcSpec::tim()->CCER & ~((uint32_t)15 << Comp::CcerBitOffset)) | ((uint32_t)0 << Comp::CcerBitOffset);
            TcSpec::tim()->DIER |= Comp::CcieBit;
        }
    }
    
    AMBRO_OPTIMIZE_SPEED
    static void setNext (HandlerContext c, TimeType time)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_running)
        AMBRO_ASSERT(TcSpec::tim()->DIER & Comp::CcieBit)
        
        o->m_time = time;
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            TimeType now = Clock::template MyTc<0>::TcSpec::tim()->CNT;
            now -= time;
            now += clearance;
            if (now < UINT32_C(0x80000000)) {
                time += now;
            }
            *ccr_reg() = time;
        }
    }
    
    template <typename ThisContext>
    AMBRO_OPTIMIZE_SPEED
    static void unset (ThisContext c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            TcSpec::tim()->DIER &= ~Comp::CcieBit;
        }
        
        memory_barrier();
        
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = false;
#endif
    }
    
    template <typename ThisContext>
    static TimeType getLastSetTime (ThisContext c)
    {
        auto *o = Object::self(c);
        
        return o->m_time;
    }
    
    AMBRO_OPTIMIZE_SPEED
    static void irq_handler (InterruptContext<Context> c)
    {
        auto *o = Object::self(c);
        
        if (!(TcSpec::tim()->DIER & Comp::CcieBit)) {
            return;
        }
        
        AMBRO_ASSERT(o->m_running)
        
        TimeType now = Clock::template MyTc<0>::TcSpec::tim()->CNT;
        now -= o->m_time;
        
        if (now < UINT32_C(0x80000000)) {
            if (!Handler::call(c)) {
#ifdef AMBROLIB_ASSERTIONS
                o->m_running = false;
#endif
                AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                    TcSpec::tim()->DIER &= ~Comp::CcieBit;
                }
            }
        }
    }
    
private:
    static uint32_t volatile * ccr_reg (void)
    {
        return Comp::ccr(TcSpec::tim());
    }
    
    static uint32_t volatile * ccmr_reg (void)
    {
        return Comp::ccmr(TcSpec::tim());
    }
    
    static const TimeType clearance = MaxValue<TimeType>((64 / Clock::prescale_divide) + 2, ExtraClearance::value() * Clock::time_freq);
    
public:
    struct Object : public ObjBase<Stm32f4ClockInterruptTimer, ParentObject, MakeTypeList<TheDebugObject>> {
        TimeType m_time;
#ifdef AMBROLIB_ASSERTIONS
        bool m_running;
#endif
    };
};

template <typename Tc, typename Comp, typename ExtraClearance = Stm32f4ClockDefaultExtraClearance>
struct Stm32f4ClockInterruptTimerService {
    template <typename Context, typename ParentObject, typename Handler>
    using InterruptTimer = Stm32f4ClockInterruptTimer<Context, ParentObject, Handler, Tc, Comp, ExtraClearance>;
};

#define AMBRO_STM32F4_CLOCK_INTERRUPT_TIMER_GLOBAL(tcspec, comp, timer, context) \
static_assert( \
    TypesAreEqual<timer::TcSpec, tcspec>::Value && \
    TypesAreEqual<timer::Comp, comp>::Value, \
    "Incorrect INTERRUPT_TIMER_GLOBA macro used" \
); \
template <> \
struct Stm32f4Clock__IrqCompHelper<tcspec, comp> { \
    static void call () \
    { \
        timer::irq_handler(MakeInterruptContext((context))); \
    } \
};

#include <aprinter/EndNamespace.h>

#endif
