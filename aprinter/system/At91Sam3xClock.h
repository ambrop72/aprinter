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

#ifndef AMBROLIB_AT91SAM3X_CLOCK_H
#define AMBROLIB_AT91SAM3X_CLOCK_H

#include <stdint.h>
#include <stddef.h>
#include <sam/drivers/pmc/pmc.h>

#include <aprinter/meta/Position.h>
#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/TypeListIndex.h>
#include <aprinter/meta/IndexElemTuple.h>
#include <aprinter/meta/TuplePosition.h>
#include <aprinter/meta/TupleForEach.h>
#include <aprinter/meta/TupleGet.h>
#include <aprinter/meta/IsEqualFunc.h>
#include <aprinter/meta/ComposeFunctions.h>
#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/meta/RemoveReference.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/InterruptLock.h>

template <typename TcSpec, typename Comp>
struct At91Sam3xClock__IrqCompHelper {
    static void call (uint32_t status) {}
};

#include <aprinter/BeginNamespace.h>

template <uint32_t TAddr, int TId, enum IRQn TIrq>
struct At91Sam3xClockTC {
    static const uint32_t Addr = TAddr;
    static const int Id = TId;
    static const enum IRQn Irq = TIrq;
};

using At91Sam3xClockTC0 = At91Sam3xClockTC<(uint32_t)(&TC0->TC_CHANNEL[0]), ID_TC0, TC0_IRQn>;
using At91Sam3xClockTC1 = At91Sam3xClockTC<(uint32_t)(&TC0->TC_CHANNEL[1]), ID_TC1, TC1_IRQn>;
using At91Sam3xClockTC2 = At91Sam3xClockTC<(uint32_t)(&TC0->TC_CHANNEL[2]), ID_TC2, TC2_IRQn>;
using At91Sam3xClockTC3 = At91Sam3xClockTC<(uint32_t)(&TC1->TC_CHANNEL[0]), ID_TC3, TC3_IRQn>;
using At91Sam3xClockTC4 = At91Sam3xClockTC<(uint32_t)(&TC1->TC_CHANNEL[1]), ID_TC4, TC4_IRQn>;
using At91Sam3xClockTC5 = At91Sam3xClockTC<(uint32_t)(&TC1->TC_CHANNEL[2]), ID_TC5, TC5_IRQn>;
using At91Sam3xClockTC6 = At91Sam3xClockTC<(uint32_t)(&TC2->TC_CHANNEL[0]), ID_TC6, TC6_IRQn>;
using At91Sam3xClockTC7 = At91Sam3xClockTC<(uint32_t)(&TC2->TC_CHANNEL[1]), ID_TC7, TC7_IRQn>;
using At91Sam3xClockTC8 = At91Sam3xClockTC<(uint32_t)(&TC2->TC_CHANNEL[2]), ID_TC8, TC8_IRQn>;

struct At91Sam3xClock__CompA {
    static const size_t CpRegOffset = 20;
    static const uint32_t CpMask = TC_SR_CPAS;
};
struct At91Sam3xClock__CompB {
    static const size_t CpRegOffset = 24;
    static const uint32_t CpMask = TC_SR_CPBS;
};
struct At91Sam3xClock__CompC {
    static const size_t CpRegOffset = 28;
    static const uint32_t CpMask = TC_SR_CPCS;
};

template <typename, typename, typename, typename, typename>
class At91Sam3xClockInterruptTimer;

template <typename Position, typename Context, int Prescale, typename TcsList>
class At91Sam3xClock
: private DebugObject<Context, void>
{
    static_assert(Prescale >= 1, "Prescale must be >=1");
    static_assert(Prescale <= 4, "Prescale must be <=5");
    
    template <typename, typename, typename, typename, typename>
    friend class At91Sam3xClockInterruptTimer;
    
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_init, init)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_deinit, deinit)
    
    template <int TcIndex> struct TcPosition;
    
    static At91Sam3xClock * self (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
    }
    
public:
    using TimeType = uint32_t;
    
    static constexpr TimeType prescale_divide =
        (Prescale == 1) ? 2 :
        (Prescale == 2) ? 8 :
        (Prescale == 3) ? 32 :
        (Prescale == 4) ? 128 : 0;
    
    static constexpr double time_unit = (double)prescale_divide / F_MCK;
    static constexpr double time_freq = (double)F_MCK / prescale_divide;
    
private:
    template <int TcIndex>
    struct MyTc {
        using TcSpec = TypeListGet<TcsList, TcIndex>;
        
        static MyTc * self (Context c)
        {
            return PositionTraverse<typename Context::TheRootPosition, TcPosition<TcIndex>>(c.root());
        }
        
        static void init (Context c)
        {
            MyTc *o = self(c);
            o->m_status = 0;
            o->m_mask = 0;
            pmc_enable_periph_clk(TcSpec::Id);
            ch()->TC_CMR = (Prescale - 1) | TC_CMR_WAVE | TC_CMR_EEVT_XC0;
            ch()->TC_IDR = UINT32_MAX;
            ch()->TC_CCR = TC_CCR_CLKEN | TC_CCR_SWTRG;
            NVIC_ClearPendingIRQ(TcSpec::Irq);
            NVIC_SetPriority(TcSpec::Irq, 4);
            NVIC_EnableIRQ(TcSpec::Irq);
        }
        
        static void deinit (Context c)
        {
            MyTc *o = self(c);
            NVIC_DisableIRQ(TcSpec::Irq);
            ch()->TC_CCR = TC_CCR_CLKDIS;
            (void)ch()->TC_SR;
            NVIC_ClearPendingIRQ(TcSpec::Irq);
            pmc_disable_periph_clk(TcSpec::Id);
        }
        
        static void irq_handler (InterruptContext<Context> c)
        {
            MyTc *o = self(c);
            uint32_t status = o->m_status | ch()->TC_SR;
            o->m_status = 0;
            At91Sam3xClock__IrqCompHelper<TcSpec, At91Sam3xClock__CompA>::call(status);
            At91Sam3xClock__IrqCompHelper<TcSpec, At91Sam3xClock__CompB>::call(status);
            At91Sam3xClock__IrqCompHelper<TcSpec, At91Sam3xClock__CompC>::call(status);
        }
        
        static TcChannel volatile * ch ()
        {
            return (TcChannel volatile *)TcSpec::Addr;
        }
        
        uint32_t m_status;
        uint32_t m_mask;
    };
    
    using MyTcsTuple = IndexElemTuple<TcsList, MyTc>;
    
    template <typename TcSpec>
    using FindTc = MyTc<TypeListIndex<TcsList, IsEqualFunc<TcSpec>>::value>;
    
public:
    static void init (Context c)
    {
        At91Sam3xClock *o = self(c);
        
        TupleForEachForward(&o->m_tcs, Foreach_init(), c);
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        At91Sam3xClock *o = self(c);
        o->debugDeinit(c);
        
        TupleForEachReverse(&o->m_tcs, Foreach_deinit(), c);
    }
    
    template <typename ThisContext>
    static TimeType getTime (ThisContext c)
    {
        At91Sam3xClock *o = self(c);
        o->debugAccess(c);
        
        return MyTc<0>::ch()->TC_CV;
    }
    
    template <typename TcSpec>
    static void tc_irq_handler (InterruptContext<Context> c)
    {
        FindTc<TcSpec>::irq_handler(c);
    }
    
private:
    MyTcsTuple m_tcs;
    
    template <int TcIndex> struct TcPosition : public TuplePosition<Position, MyTcsTuple, &At91Sam3xClock::m_tcs, TcIndex> {};
};

#define AMBRO_AT91SAM3X_CLOCK_TC_GLOBAL(tcnum, clock, context) \
__attribute__((used)) \
void TC##tcnum##_Handler (void) \
{ \
    (clock).tc_irq_handler<At91Sam3xClockTC##tcnum>(MakeInterruptContext((context))); \
}

#define AMBRO_AT91SAM3X_CLOCK_TC0_GLOBAL(clock, context) AMBRO_AT91SAM3X_CLOCK_TC_GLOBAL(0, (clock), (context))
#define AMBRO_AT91SAM3X_CLOCK_TC1_GLOBAL(clock, context) AMBRO_AT91SAM3X_CLOCK_TC_GLOBAL(1, (clock), (context))
#define AMBRO_AT91SAM3X_CLOCK_TC2_GLOBAL(clock, context) AMBRO_AT91SAM3X_CLOCK_TC_GLOBAL(2, (clock), (context))
#define AMBRO_AT91SAM3X_CLOCK_TC3_GLOBAL(clock, context) AMBRO_AT91SAM3X_CLOCK_TC_GLOBAL(3, (clock), (context))
#define AMBRO_AT91SAM3X_CLOCK_TC4_GLOBAL(clock, context) AMBRO_AT91SAM3X_CLOCK_TC_GLOBAL(4, (clock), (context))
#define AMBRO_AT91SAM3X_CLOCK_TC5_GLOBAL(clock, context) AMBRO_AT91SAM3X_CLOCK_TC_GLOBAL(5, (clock), (context))
#define AMBRO_AT91SAM3X_CLOCK_TC6_GLOBAL(clock, context) AMBRO_AT91SAM3X_CLOCK_TC_GLOBAL(6, (clock), (context))
#define AMBRO_AT91SAM3X_CLOCK_TC7_GLOBAL(clock, context) AMBRO_AT91SAM3X_CLOCK_TC_GLOBAL(7, (clock), (context))
#define AMBRO_AT91SAM3X_CLOCK_TC8_GLOBAL(clock, context) AMBRO_AT91SAM3X_CLOCK_TC_GLOBAL(8, (clock), (context))

template <typename Position, typename Context, typename Handler, typename TTcSpec, typename TComp>
class At91Sam3xClockInterruptTimer
: private DebugObject<Context, void>
{
public:
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using HandlerContext = InterruptContext<Context>;
    using TcSpec = TTcSpec;
    using Comp = TComp;
    
private:
    static At91Sam3xClockInterruptTimer * self (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
    }
    
    using TheMyTc = typename Clock::template FindTc<TcSpec>;
    static const uint32_t CpMask = Comp::CpMask;
    
public:
    static void init (Context c)
    {
        At91Sam3xClockInterruptTimer *o = self(c);
        o->debugInit(c);
        
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = false;
#endif
    }
    
    static void deinit (Context c)
    {
        At91Sam3xClockInterruptTimer *o = self(c);
        TheMyTc *mtc = TheMyTc::self(c);
        o->debugDeinit(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            ch()->TC_IDR = CpMask;
            mtc->m_mask &= ~CpMask;
        }
    }
    
    template <typename ThisContext>
    static void set (ThisContext c, TimeType time)
    {
        At91Sam3xClockInterruptTimer *o = self(c);
        TheMyTc *mtc = TheMyTc::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(!o->m_running)
        
        o->m_time = time;
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = true;
#endif
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            TimeType now = Clock::template MyTc<0>::ch()->TC_CV;
            now -= time;
            now += clearance;
            if (now < UINT32_C(0x80000000)) {
                time += now;
            }
            *my_cp_reg() = time;
            mtc->m_status = (ch()->TC_SR | mtc->m_status) & ~CpMask;
            mtc->m_mask |= CpMask;
            ch()->TC_IER = CpMask;
        }
    }
    
    template <typename ThisContext>
    static void unset (ThisContext c)
    {
        At91Sam3xClockInterruptTimer *o = self(c);
        TheMyTc *mtc = TheMyTc::self(c);
        o->debugAccess(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            ch()->TC_IDR = CpMask;
            mtc->m_mask &= ~CpMask;
        }
        
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = false;
#endif
    }
    
    static void irq_handler (InterruptContext<Context> c, uint32_t status)
    {
        At91Sam3xClockInterruptTimer *o = self(c);
        TheMyTc *mtc = TheMyTc::self(c);
        
        if (!(mtc->m_mask & status & CpMask)) {
            return;
        }
        
        AMBRO_ASSERT(o->m_running)
        
        TimeType now = Clock::template MyTc<0>::ch()->TC_CV;
        now -= o->m_time;
        
        if (now < UINT32_C(0x80000000)) {
#ifdef AMBROLIB_ASSERTIONS
            o->m_running = false;
#endif
            if (!Handler::call(o, c)) {
                ch()->TC_IDR = CpMask;
                mtc->m_mask &= ~CpMask;
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
using At91Sam3xClockInterruptTimer_TC0A = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC0, At91Sam3xClock__CompA>;
template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC0B = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC0, At91Sam3xClock__CompB>;
template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC0C = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC0, At91Sam3xClock__CompC>;

template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC1A = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC1, At91Sam3xClock__CompA>;
template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC1B = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC1, At91Sam3xClock__CompB>;
template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC1C = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC1, At91Sam3xClock__CompC>;

template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC2A = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC2, At91Sam3xClock__CompA>;
template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC2B = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC2, At91Sam3xClock__CompB>;
template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC2C = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC2, At91Sam3xClock__CompC>;

template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC3A = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC3, At91Sam3xClock__CompA>;
template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC3B = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC3, At91Sam3xClock__CompB>;
template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC3C = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC3, At91Sam3xClock__CompC>;

template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC4A = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC4, At91Sam3xClock__CompA>;
template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC4B = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC4, At91Sam3xClock__CompB>;
template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC4C = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC4, At91Sam3xClock__CompC>;

template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC5A = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC5, At91Sam3xClock__CompA>;
template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC5B = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC5, At91Sam3xClock__CompB>;
template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC5C = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC5, At91Sam3xClock__CompC>;

template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC6A = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC6, At91Sam3xClock__CompA>;
template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC6B = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC6, At91Sam3xClock__CompB>;
template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC6C = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC6, At91Sam3xClock__CompC>;

template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC7A = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC7, At91Sam3xClock__CompA>;
template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC7B = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC7, At91Sam3xClock__CompB>;
template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC7C = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC7, At91Sam3xClock__CompC>;

template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC8A = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC8, At91Sam3xClock__CompA>;
template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC8B = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC8, At91Sam3xClock__CompB>;
template <typename Position, typename Context, typename Handler>
using At91Sam3xClockInterruptTimer_TC8C = At91Sam3xClockInterruptTimer<Position, Context, Handler, At91Sam3xClockTC8, At91Sam3xClock__CompC>;

#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(tcspec, comp, timer, context) \
static_assert( \
    TypesAreEqual<RemoveReference<decltype(timer)>::TcSpec, tcspec>::value  && \
    TypesAreEqual<RemoveReference<decltype(timer)>::Comp, comp>::value, \
    "Incorrect TCXY macro used" \
); \
template <> \
struct At91Sam3xClock__IrqCompHelper<tcspec, comp> { \
    static void call (uint32_t status) \
    { \
        (timer).irq_handler(MakeInterruptContext((context)), status); \
    } \
};

#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC0A_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC0, At91Sam3xClock__CompA, (timer), (context))
#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC0B_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC0, At91Sam3xClock__CompB, (timer), (context))
#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC0C_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC0, At91Sam3xClock__CompC, (timer), (context))

#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC1A_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC1, At91Sam3xClock__CompA, (timer), (context))
#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC1B_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC1, At91Sam3xClock__CompB, (timer), (context))
#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC1C_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC1, At91Sam3xClock__CompC, (timer), (context))

#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC2A_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC2, At91Sam3xClock__CompA, (timer), (context))
#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC2B_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC2, At91Sam3xClock__CompB, (timer), (context))
#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC2C_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC2, At91Sam3xClock__CompC, (timer), (context))

#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC3A_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC3, At91Sam3xClock__CompA, (timer), (context))
#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC3B_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC3, At91Sam3xClock__CompB, (timer), (context))
#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC3C_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC3, At91Sam3xClock__CompC, (timer), (context))

#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC4A_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC4, At91Sam3xClock__CompA, (timer), (context))
#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC4B_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC4, At91Sam3xClock__CompB, (timer), (context))
#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC4C_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC4, At91Sam3xClock__CompC, (timer), (context))

#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC5A_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC5, At91Sam3xClock__CompA, (timer), (context))
#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC5B_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC5, At91Sam3xClock__CompB, (timer), (context))
#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC5C_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC5, At91Sam3xClock__CompC, (timer), (context))

#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC6A_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC6, At91Sam3xClock__CompA, (timer), (context))
#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC6B_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC6, At91Sam3xClock__CompB, (timer), (context))
#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC6C_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC6, At91Sam3xClock__CompC, (timer), (context))

#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC7A_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC7, At91Sam3xClock__CompA, (timer), (context))
#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC7B_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC7, At91Sam3xClock__CompB, (timer), (context))
#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC7C_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC7, At91Sam3xClock__CompC, (timer), (context))

#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC8A_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC8, At91Sam3xClock__CompA, (timer), (context))
#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC8B_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC8, At91Sam3xClock__CompB, (timer), (context))
#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC8C_GLOBAL(timer, context) AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC8, At91Sam3xClock__CompC, (timer), (context))

#include <aprinter/EndNamespace.h>

#endif
