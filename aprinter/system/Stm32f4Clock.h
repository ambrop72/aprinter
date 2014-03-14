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

#include <aprinter/meta/Object.h>
#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/TypeListIndex.h>
#include <aprinter/meta/IndexElemList.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/IsEqualFunc.h>
#include <aprinter/meta/ComposeFunctions.h>
#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/meta/TypeListLength.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/InterruptLock.h>

template <typename TcSpec, typename Comp>
struct Stm32f4Clock__IrqCompHelper {
    static void call () {}
};

#include <aprinter/BeginNamespace.h>

template <bool TIs32Bit, uint32_t TAddr, int TClockType, uint32_t TClockId, enum IRQn TIrq>
struct Stm32f4ClockTC {
    static bool const Is32Bit = TIs32Bit;
    static TIM_TypeDef * tim () { return (TIM_TypeDef *)TAddr; }
    static int const ClockType = TClockType;
    static uint32_t const ClockId = TClockId;
    static enum IRQn const Irq = TIrq;
};

// We don't work with (not need) APB2 timers.

//using Stm32f4ClockTIM1 = Stm32f4ClockTC<false, TIM1_BASE, 2, RCC_APB2Periph_TIM1, TIM1_CC_IRQn>;
using Stm32f4ClockTIM2 = Stm32f4ClockTC<true, TIM2_BASE, 1, RCC_APB1Periph_TIM2, TIM2_IRQn>;
using Stm32f4ClockTIM3 = Stm32f4ClockTC<false, TIM3_BASE, 1, RCC_APB1Periph_TIM3, TIM3_IRQn>;
using Stm32f4ClockTIM4 = Stm32f4ClockTC<false, TIM4_BASE, 1, RCC_APB1Periph_TIM4, TIM4_IRQn>;
using Stm32f4ClockTIM5 = Stm32f4ClockTC<true, TIM5_BASE, 1, RCC_APB1Periph_TIM5, TIM5_IRQn>;
//using Stm32f4ClockTIM8 = Stm32f4ClockTC<false, TIM8_BASE, 2, RCC_APB2Periph_TIM8, TIM8_CC_IRQn>;
//using Stm32f4ClockTIM9 = Stm32f4ClockTC<false, TIM9_BASE, 2, RCC_APB2Periph_TIM9, TIM1_BRK_TIM9_IRQn>;
//using Stm32f4ClockTIM10 = Stm32f4ClockTC<false, TIM10_BASE, 2, RCC_APB2Periph_TIM10, TIM1_UP_TIM10_IRQn>;
//using Stm32f4ClockTIM11 = Stm32f4ClockTC<false, TIM11_BASE, 2, RCC_APB2Periph_TIM11, TIM1_TRG_COM_TIM11_IRQn>;
using Stm32f4ClockTIM12 = Stm32f4ClockTC<false, TIM12_BASE, 1, RCC_APB1Periph_TIM12, TIM8_BRK_TIM12_IRQn>;
using Stm32f4ClockTIM13 = Stm32f4ClockTC<false, TIM13_BASE, 1, RCC_APB1Periph_TIM13, TIM8_UP_TIM13_IRQn>;
using Stm32f4ClockTIM14 = Stm32f4ClockTC<false, TIM14_BASE, 1, RCC_APB1Periph_TIM14, TIM8_TRG_COM_TIM14_IRQn>;

template <
    size_t TCcmrOffset,
    int TCcmrBitOffset,
    int TCcerBitOffset,
    size_t TCcrOffset,
    uint16_t TCcieBit
>
struct Stm32f4Clock__Comp {
    static size_t const CcmrOffset = TCcmrOffset;
    static int const CcmrBitOffset = TCcmrBitOffset;
    static int const CcerBitOffset = TCcerBitOffset;
    static size_t const CcrOffset = TCcrOffset;
    static uint16_t const CcieBit = TCcieBit;
};

using Stm32f4Clock__Comp1 = Stm32f4Clock__Comp<offsetof(TIM_TypeDef, CCMR1), 0, 0, offsetof(TIM_TypeDef, CCR1), TIM_DIER_CC1IE>;
using Stm32f4Clock__Comp2 = Stm32f4Clock__Comp<offsetof(TIM_TypeDef, CCMR1), 8, 4, offsetof(TIM_TypeDef, CCR2), TIM_DIER_CC2IE>;
using Stm32f4Clock__Comp3 = Stm32f4Clock__Comp<offsetof(TIM_TypeDef, CCMR2), 0, 8, offsetof(TIM_TypeDef, CCR3), TIM_DIER_CC3IE>;
using Stm32f4Clock__Comp4 = Stm32f4Clock__Comp<offsetof(TIM_TypeDef, CCMR2), 8, 12, offsetof(TIM_TypeDef, CCR4), TIM_DIER_CC4IE>;

template <typename Context, typename ParentObject, uint16_t Prescale, typename ParamsTcsList>
class Stm32f4Clock {
    static_assert(TypeListLength<ParamsTcsList>::value > 0, "Need at least one timer.");
    static_assert(TypeListGet<ParamsTcsList, 0>::Is32Bit, "First timer must be 32-bit.");
    
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_init, init)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_deinit, deinit)
    
public:
    struct Object;
    using TimeType = uint32_t;
    
    static constexpr TimeType prescale_divide = (TimeType)Prescale + 1;
    
    static constexpr double time_unit = (double)prescale_divide / F_TIMERS1;
    static constexpr double time_freq = (double)F_TIMERS1 / prescale_divide;
    
private:
    template <int TTcIndex>
    struct MyTc {
        static int const TcIndex = TTcIndex;
        using TcSpec = TypeListGet<ParamsTcsList, TcIndex>;
        
        static void init (Context c)
        {
            if (TcSpec::ClockType == 1) {
                RCC_APB1PeriphClockCmd(TcSpec::ClockId, ENABLE);
            } else if (TcSpec::ClockType == 2) {
                RCC_APB2PeriphClockCmd(TcSpec::ClockId, ENABLE);
            }
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
            if (TcSpec::ClockType == 1) {
                RCC_APB1PeriphClockCmd(TcSpec::ClockId, DISABLE);
            } else if (TcSpec::ClockType == 2) {
                RCC_APB2PeriphClockCmd(TcSpec::ClockId, DISABLE);
            }
        }
        
        static void irq_handler (InterruptContext<Context> c)
        {
            Stm32f4Clock__IrqCompHelper<TcSpec, Stm32f4Clock__Comp1>::call();
            Stm32f4Clock__IrqCompHelper<TcSpec, Stm32f4Clock__Comp2>::call();
            Stm32f4Clock__IrqCompHelper<TcSpec, Stm32f4Clock__Comp3>::call();
            Stm32f4Clock__IrqCompHelper<TcSpec, Stm32f4Clock__Comp4>::call();
        }
    };
    
    using MyTcsList = IndexElemList<ParamsTcsList, MyTc>;
    
    template <typename TcSpec>
    using FindTc = MyTc<TypeListIndex<ParamsTcsList, IsEqualFunc<TcSpec>>::value>;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        ListForEachForward<MyTcsList>(LForeach_init(), c);
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->debugDeinit(c);
        
        ListForEachReverse<MyTcsList>(LForeach_deinit(), c);
    }
    
    template <typename ThisContext>
    static TimeType getTime (ThisContext c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        return MyTc<0>::TcSpec::tim()->CNT;
    }
    
    template <typename TcSpec>
    static void tc_irq_handler (InterruptContext<Context> c)
    {
        FindTc<TcSpec>::irq_handler(c);
    }
    
public:
    struct Object : public ObjBase<Stm32f4Clock, ParentObject, EmptyTypeList>,
        public DebugObject<Context, void>
    {};
};

#if 0

#define AMBRO_AT91SAM3U_CLOCK_TC_GLOBAL(tcnum, clock, context) \
extern "C" \
__attribute__((used)) \
void TC##tcnum##_Handler (void) \
{ \
    (clock).tc_irq_handler<Stm32f4ClockTC##tcnum>(MakeInterruptContext((context))); \
}

#define AMBRO_AT91SAM3U_CLOCK_TC0_GLOBAL(clock, context) AMBRO_AT91SAM3U_CLOCK_TC_GLOBAL(0, (clock), (context))
#define AMBRO_AT91SAM3U_CLOCK_TC1_GLOBAL(clock, context) AMBRO_AT91SAM3U_CLOCK_TC_GLOBAL(1, (clock), (context))
#define AMBRO_AT91SAM3U_CLOCK_TC2_GLOBAL(clock, context) AMBRO_AT91SAM3U_CLOCK_TC_GLOBAL(2, (clock), (context))

template <typename Position, typename Context, typename Handler, typename TTcSpec, typename TComp>
class Stm32f4ClockInterruptTimer
: private DebugObject<Context, void>
{
public:
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using HandlerContext = InterruptContext<Context>;
    using TcSpec = TTcSpec;
    using Comp = TComp;
    
private:
    AMBRO_MAKE_SELF(Context, Stm32f4ClockInterruptTimer, Position)
    using TheMyTc = typename Clock::template FindTc<TcSpec>;
    static const uint32_t CpMask = Comp::CpMask;
    
public:
    static void init (Context c)
    {
        Stm32f4ClockInterruptTimer *o = self(c);
        o->debugInit(c);
        
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = false;
#endif
    }
    
    static void deinit (Context c)
    {
        Stm32f4ClockInterruptTimer *o = self(c);
        o->debugDeinit(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            ch()->TC_IDR = CpMask;
        }
    }
    
    template <typename ThisContext>
    static void setFirst (ThisContext c, TimeType time)
    {
        Stm32f4ClockInterruptTimer *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(!o->m_running)
        AMBRO_ASSERT(!(ch()->TC_IMR & CpMask))
        
        o->m_time = time;
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = true;
#endif
        
        AMBRO_LOCK_T(AtomicTempLock(), c, lock_c) {
            TimeType now = Clock::get_time_interrupt(c);
            now -= time;
            now += clearance;
            if (now < UINT32_C(0x80000000)) {
                time += now;
            }
            *my_cp_reg() = time;
            uint32_t sr = ch()->TC_SR;
            if (TheMyTc::TcIndex == 0 && (sr & TC_SR_COVFS)) {
                Clock::self(c)->m_offset++;
            }
            ch()->TC_IER = CpMask;
        }
    }
    
    static void setNext (HandlerContext c, TimeType time)
    {
        Stm32f4ClockInterruptTimer *o = self(c);
        AMBRO_ASSERT(o->m_running)
        AMBRO_ASSERT((ch()->TC_IMR & CpMask))
        
        o->m_time = time;
        AMBRO_LOCK_T(AtomicTempLock(), c, lock_c) {
            TimeType now = Clock::get_time_interrupt(c);
            now -= time;
            now += clearance;
            if (now < UINT32_C(0x80000000)) {
                time += now;
            }
            *my_cp_reg() = time;
        }
    }
    
    template <typename ThisContext>
    static void unset (ThisContext c)
    {
        Stm32f4ClockInterruptTimer *o = self(c);
        o->debugAccess(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            ch()->TC_IDR = CpMask;
        }
        
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = false;
#endif
    }
    
    static void irq_handler (InterruptContext<Context> c, TimeType irq_time)
    {
        Stm32f4ClockInterruptTimer *o = self(c);
        
        if (!(ch()->TC_IMR & CpMask)) {
            return;
        }
        
        AMBRO_ASSERT(o->m_running)
        
        if ((TimeType)(irq_time - o->m_time) < UINT32_C(0x80000000)) {
            if (!Handler::call(o, c)) {
#ifdef AMBROLIB_ASSERTIONS
                o->m_running = false;
#endif
                ch()->TC_IDR = CpMask;
            }
        }
    }
    
private:
    static TcChannel volatile * ch (void)
    {
        return TheMyTc::ch();
    }
    
    static RwReg volatile * my_cp_reg (void)
    {
        return (RwReg volatile *)(TcSpec::Addr + Comp::CpRegOffset);
    }
    
    static const TimeType clearance = (64 / Clock::prescale_divide) + 2;
    
    TimeType m_time;
#ifdef AMBROLIB_ASSERTIONS
    bool m_running;
#endif
};

template <typename Position, typename Context, typename Handler>
using Stm32f4ClockInterruptTimer_TC0A = Stm32f4ClockInterruptTimer<Position, Context, Handler, Stm32f4ClockTC0, Stm32f4Clock__CompA>;
template <typename Position, typename Context, typename Handler>
using Stm32f4ClockInterruptTimer_TC0B = Stm32f4ClockInterruptTimer<Position, Context, Handler, Stm32f4ClockTC0, Stm32f4Clock__CompB>;
template <typename Position, typename Context, typename Handler>
using Stm32f4ClockInterruptTimer_TC0C = Stm32f4ClockInterruptTimer<Position, Context, Handler, Stm32f4ClockTC0, Stm32f4Clock__CompC>;

template <typename Position, typename Context, typename Handler>
using Stm32f4ClockInterruptTimer_TC1A = Stm32f4ClockInterruptTimer<Position, Context, Handler, Stm32f4ClockTC1, Stm32f4Clock__CompA>;
template <typename Position, typename Context, typename Handler>
using Stm32f4ClockInterruptTimer_TC1B = Stm32f4ClockInterruptTimer<Position, Context, Handler, Stm32f4ClockTC1, Stm32f4Clock__CompB>;
template <typename Position, typename Context, typename Handler>
using Stm32f4ClockInterruptTimer_TC1C = Stm32f4ClockInterruptTimer<Position, Context, Handler, Stm32f4ClockTC1, Stm32f4Clock__CompC>;

template <typename Position, typename Context, typename Handler>
using Stm32f4ClockInterruptTimer_TC2A = Stm32f4ClockInterruptTimer<Position, Context, Handler, Stm32f4ClockTC2, Stm32f4Clock__CompA>;
template <typename Position, typename Context, typename Handler>
using Stm32f4ClockInterruptTimer_TC2B = Stm32f4ClockInterruptTimer<Position, Context, Handler, Stm32f4ClockTC2, Stm32f4Clock__CompB>;
template <typename Position, typename Context, typename Handler>
using Stm32f4ClockInterruptTimer_TC2C = Stm32f4ClockInterruptTimer<Position, Context, Handler, Stm32f4ClockTC2, Stm32f4Clock__CompC>;

#define AMBRO_AT91SAM3U_CLOCK_INTERRUPT_TIMER_GLOBAL(tcspec, comp, timer, context) \
static_assert( \
    TypesAreEqual<RemoveReference<decltype(timer)>::TcSpec, tcspec>::value  && \
    TypesAreEqual<RemoveReference<decltype(timer)>::Comp, comp>::value, \
    "Incorrect TCXY macro used" \
); \
template <> \
struct Stm32f4Clock__IrqCompHelper<tcspec, comp> { \
    static void call () \
    { \
        (timer).irq_handler(MakeInterruptContext((context))); \
    } \
};

#define AMBRO_AT91SAM3U_CLOCK_INTERRUPT_TIMER_TC0A_GLOBAL(timer, context) AMBRO_AT91SAM3U_CLOCK_INTERRUPT_TIMER_GLOBAL(Stm32f4ClockTC0, Stm32f4Clock__CompA, (timer), (context))
#define AMBRO_AT91SAM3U_CLOCK_INTERRUPT_TIMER_TC0B_GLOBAL(timer, context) AMBRO_AT91SAM3U_CLOCK_INTERRUPT_TIMER_GLOBAL(Stm32f4ClockTC0, Stm32f4Clock__CompB, (timer), (context))
#define AMBRO_AT91SAM3U_CLOCK_INTERRUPT_TIMER_TC0C_GLOBAL(timer, context) AMBRO_AT91SAM3U_CLOCK_INTERRUPT_TIMER_GLOBAL(Stm32f4ClockTC0, Stm32f4Clock__CompC, (timer), (context))

#define AMBRO_AT91SAM3U_CLOCK_INTERRUPT_TIMER_TC1A_GLOBAL(timer, context) AMBRO_AT91SAM3U_CLOCK_INTERRUPT_TIMER_GLOBAL(Stm32f4ClockTC1, Stm32f4Clock__CompA, (timer), (context))
#define AMBRO_AT91SAM3U_CLOCK_INTERRUPT_TIMER_TC1B_GLOBAL(timer, context) AMBRO_AT91SAM3U_CLOCK_INTERRUPT_TIMER_GLOBAL(Stm32f4ClockTC1, Stm32f4Clock__CompB, (timer), (context))
#define AMBRO_AT91SAM3U_CLOCK_INTERRUPT_TIMER_TC1C_GLOBAL(timer, context) AMBRO_AT91SAM3U_CLOCK_INTERRUPT_TIMER_GLOBAL(Stm32f4ClockTC1, Stm32f4Clock__CompC, (timer), (context))

#define AMBRO_AT91SAM3U_CLOCK_INTERRUPT_TIMER_TC2A_GLOBAL(timer, context) AMBRO_AT91SAM3U_CLOCK_INTERRUPT_TIMER_GLOBAL(Stm32f4ClockTC2, Stm32f4Clock__CompA, (timer), (context))
#define AMBRO_AT91SAM3U_CLOCK_INTERRUPT_TIMER_TC2B_GLOBAL(timer, context) AMBRO_AT91SAM3U_CLOCK_INTERRUPT_TIMER_GLOBAL(Stm32f4ClockTC2, Stm32f4Clock__CompB, (timer), (context))
#define AMBRO_AT91SAM3U_CLOCK_INTERRUPT_TIMER_TC2C_GLOBAL(timer, context) AMBRO_AT91SAM3U_CLOCK_INTERRUPT_TIMER_GLOBAL(Stm32f4ClockTC2, Stm32f4Clock__CompC, (timer), (context))

#endif

#include <aprinter/EndNamespace.h>

#endif
