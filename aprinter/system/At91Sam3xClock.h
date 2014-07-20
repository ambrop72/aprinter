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

#include <aprinter/base/Object.h>
#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/TypeListIndex.h>
#include <aprinter/meta/IndexElemTuple.h>
#include <aprinter/meta/TupleForEach.h>
#include <aprinter/meta/TupleGet.h>
#include <aprinter/meta/IsEqualFunc.h>
#include <aprinter/meta/ComposeFunctions.h>
#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/WrapDouble.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/InterruptLock.h>

template <typename TcSpec, typename Comp>
struct At91Sam3xClock__IrqCompHelper {
    static void call () {}
};

#include <aprinter/BeginNamespace.h>

using At91Sam3xClockDefaultExtraClearance = AMBRO_WRAP_DOUBLE(0.0);

template <uint32_t TAddr, int TId, enum IRQn TIrq>
struct At91Sam3xClockTC {
    static const uint32_t Addr = TAddr;
    static const int Id = TId;
    static const enum IRQn Irq = TIrq;
};

using At91Sam3xClockTC0 = At91Sam3xClockTC<GET_PERIPHERAL_ADDR(TC0) + offsetof(Tc, TC_CHANNEL[0]), ID_TC0, TC0_IRQn>;
using At91Sam3xClockTC1 = At91Sam3xClockTC<GET_PERIPHERAL_ADDR(TC0) + offsetof(Tc, TC_CHANNEL[1]), ID_TC1, TC1_IRQn>;
using At91Sam3xClockTC2 = At91Sam3xClockTC<GET_PERIPHERAL_ADDR(TC0) + offsetof(Tc, TC_CHANNEL[2]), ID_TC2, TC2_IRQn>;
using At91Sam3xClockTC3 = At91Sam3xClockTC<GET_PERIPHERAL_ADDR(TC1) + offsetof(Tc, TC_CHANNEL[0]), ID_TC3, TC3_IRQn>;
using At91Sam3xClockTC4 = At91Sam3xClockTC<GET_PERIPHERAL_ADDR(TC1) + offsetof(Tc, TC_CHANNEL[1]), ID_TC4, TC4_IRQn>;
using At91Sam3xClockTC5 = At91Sam3xClockTC<GET_PERIPHERAL_ADDR(TC1) + offsetof(Tc, TC_CHANNEL[2]), ID_TC5, TC5_IRQn>;
using At91Sam3xClockTC6 = At91Sam3xClockTC<GET_PERIPHERAL_ADDR(TC2) + offsetof(Tc, TC_CHANNEL[0]), ID_TC6, TC6_IRQn>;
using At91Sam3xClockTC7 = At91Sam3xClockTC<GET_PERIPHERAL_ADDR(TC2) + offsetof(Tc, TC_CHANNEL[1]), ID_TC7, TC7_IRQn>;
using At91Sam3xClockTC8 = At91Sam3xClockTC<GET_PERIPHERAL_ADDR(TC2) + offsetof(Tc, TC_CHANNEL[2]), ID_TC8, TC8_IRQn>;

template <size_t TCpRegOffset, uint32_t TCpMask>
struct At91Sam3xClockComp {
    static const size_t CpRegOffset = TCpRegOffset;
    static const uint32_t CpMask = TCpMask;
};

using At91Sam3xClockCompA = At91Sam3xClockComp<offsetof(TcChannel, TC_RA), TC_SR_CPAS>;
using At91Sam3xClockCompB = At91Sam3xClockComp<offsetof(TcChannel, TC_RB), TC_SR_CPBS>;
using At91Sam3xClockCompC = At91Sam3xClockComp<offsetof(TcChannel, TC_RC), TC_SR_CPCS>;

template <typename, typename, typename, typename, typename, typename>
class At91Sam3xClockInterruptTimer;

template <typename Context, typename ParentObject, int Prescale, typename TcsList>
class At91Sam3xClock {
    static_assert(Prescale >= 1, "Prescale must be >=1");
    static_assert(Prescale <= 4, "Prescale must be <=4");
    
    template <typename, typename, typename, typename, typename, typename>
    friend class At91Sam3xClockInterruptTimer;
    
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_init, init)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_deinit, deinit)
    
public:
    struct Object;
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
        
        static void init (Context c)
        {
            pmc_enable_periph_clk(TcSpec::Id);
            ch()->TC_CMR = (Prescale - 1) | TC_CMR_WAVE | TC_CMR_EEVT_XC0;
            ch()->TC_IDR = UINT32_MAX;
            ch()->TC_CCR = TC_CCR_CLKEN | TC_CCR_SWTRG;
            NVIC_ClearPendingIRQ(TcSpec::Irq);
            NVIC_SetPriority(TcSpec::Irq, INTERRUPT_PRIORITY);
            NVIC_EnableIRQ(TcSpec::Irq);
        }
        
        static void deinit (Context c)
        {
            NVIC_DisableIRQ(TcSpec::Irq);
            ch()->TC_CCR = TC_CCR_CLKDIS;
            (void)ch()->TC_SR;
            NVIC_ClearPendingIRQ(TcSpec::Irq);
            pmc_disable_periph_clk(TcSpec::Id);
        }
        
        static void irq_handler (InterruptContext<Context> c)
        {
            (void)ch()->TC_SR;
            At91Sam3xClock__IrqCompHelper<TcSpec, At91Sam3xClockCompA>::call();
            At91Sam3xClock__IrqCompHelper<TcSpec, At91Sam3xClockCompB>::call();
            At91Sam3xClock__IrqCompHelper<TcSpec, At91Sam3xClockCompC>::call();
        }
        
        static TcChannel volatile * ch ()
        {
            return (TcChannel volatile *)TcSpec::Addr;
        }
    };
    
    using MyTcsTuple = IndexElemTuple<TcsList, MyTc>;
    
    template <typename TcSpec>
    using FindTc = MyTc<TypeListIndex<TcsList, IsEqualFunc<TcSpec>>::Value>;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        MyTcsTuple dummy;
        TupleForEachForward(&dummy, Foreach_init(), c);
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->debugDeinit(c);
        
        MyTcsTuple dummy;
        TupleForEachReverse(&dummy, Foreach_deinit(), c);
    }
    
    template <typename ThisContext>
    static TimeType getTime (ThisContext c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        return MyTc<0>::ch()->TC_CV;
    }
    
    template <typename TcSpec>
    static void tc_irq_handler (InterruptContext<Context> c)
    {
        FindTc<TcSpec>::irq_handler(c);
    }
    
public:
    struct Object : public ObjBase<At91Sam3xClock, ParentObject, EmptyTypeList>,
        public DebugObject<Context, void>
    {
        char dummy;
    };
};

#define AMBRO_AT91SAM3X_CLOCK_TC_GLOBAL(tcnum, clock, context) \
extern "C" \
__attribute__((used)) \
void TC##tcnum##_Handler (void) \
{ \
    clock::tc_irq_handler<At91Sam3xClockTC##tcnum>(MakeInterruptContext((context))); \
}

#define AMBRO_AT91SAM3X_CLOCK_TC0_GLOBAL(clock, context) AMBRO_AT91SAM3X_CLOCK_TC_GLOBAL(0, clock, (context))
#define AMBRO_AT91SAM3X_CLOCK_TC1_GLOBAL(clock, context) AMBRO_AT91SAM3X_CLOCK_TC_GLOBAL(1, clock, (context))
#define AMBRO_AT91SAM3X_CLOCK_TC2_GLOBAL(clock, context) AMBRO_AT91SAM3X_CLOCK_TC_GLOBAL(2, clock, (context))
#define AMBRO_AT91SAM3X_CLOCK_TC3_GLOBAL(clock, context) AMBRO_AT91SAM3X_CLOCK_TC_GLOBAL(3, clock, (context))
#define AMBRO_AT91SAM3X_CLOCK_TC4_GLOBAL(clock, context) AMBRO_AT91SAM3X_CLOCK_TC_GLOBAL(4, clock, (context))
#define AMBRO_AT91SAM3X_CLOCK_TC5_GLOBAL(clock, context) AMBRO_AT91SAM3X_CLOCK_TC_GLOBAL(5, clock, (context))
#define AMBRO_AT91SAM3X_CLOCK_TC6_GLOBAL(clock, context) AMBRO_AT91SAM3X_CLOCK_TC_GLOBAL(6, clock, (context))
#define AMBRO_AT91SAM3X_CLOCK_TC7_GLOBAL(clock, context) AMBRO_AT91SAM3X_CLOCK_TC_GLOBAL(7, clock, (context))
#define AMBRO_AT91SAM3X_CLOCK_TC8_GLOBAL(clock, context) AMBRO_AT91SAM3X_CLOCK_TC_GLOBAL(8, clock, (context))

template <typename Context, typename ParentObject, typename Handler, typename TTcSpec, typename TComp, typename ExtraClearance>
class At91Sam3xClockInterruptTimer {
public:
    struct Object;
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using HandlerContext = InterruptContext<Context>;
    using TcSpec = TTcSpec;
    using Comp = TComp;
    
private:
    using TheMyTc = typename Clock::template FindTc<TcSpec>;
    static const uint32_t CpMask = Comp::CpMask;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        o->debugInit(c);
        
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = false;
#endif
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->debugDeinit(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            ch()->TC_IDR = CpMask;
        }
    }
    
    template <typename ThisContext>
    static void setFirst (ThisContext c, TimeType time)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(!o->m_running)
        AMBRO_ASSERT(!(ch()->TC_IMR & CpMask))
        
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
            (void)ch()->TC_SR;
            ch()->TC_IER = CpMask;
        }
    }
    
    static void setNext (HandlerContext c, TimeType time)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_running)
        AMBRO_ASSERT((ch()->TC_IMR & CpMask))
        
        o->m_time = time;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            TimeType now = Clock::template MyTc<0>::ch()->TC_CV;
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
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            ch()->TC_IDR = CpMask;
        }
        
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = false;
#endif
    }
    
    static void irq_handler (InterruptContext<Context> c)
    {
        auto *o = Object::self(c);
        
        if (!(ch()->TC_IMR & CpMask)) {
            return;
        }
        
        AMBRO_ASSERT(o->m_running)
        
        TimeType now = Clock::template MyTc<0>::ch()->TC_CV;
        now -= o->m_time;
        
        if (now < UINT32_C(0x80000000)) {
            if (!Handler::call(c)) {
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
    
    static const TimeType clearance = MaxValue<TimeType>((64 / Clock::prescale_divide) + 2, ExtraClearance::value() * Clock::time_freq);
    
public:
    struct Object : public ObjBase<At91Sam3xClockInterruptTimer, ParentObject, EmptyTypeList>,
        public DebugObject<Context, void>
    {
        TimeType m_time;
#ifdef AMBROLIB_ASSERTIONS
        bool m_running;
#endif
    };
};

template <typename Tc, typename Comp, typename ExtraClearance = At91Sam3xClockDefaultExtraClearance>
struct At91Sam3xClockInterruptTimerService {
    template <typename Context, typename ParentObject, typename Handler>
    using InterruptTimer = At91Sam3xClockInterruptTimer<Context, ParentObject, Handler, Tc, Comp, ExtraClearance>;
};

#define AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(tcspec, comp, timer, context) \
static_assert( \
    TypesAreEqual<timer::TcSpec, tcspec>::Value && \
    TypesAreEqual<timer::Comp, comp>::Value, \
    "Incorrect TCXY macro used" \
); \
template <> \
struct At91Sam3xClock__IrqCompHelper<tcspec, comp> { \
    static void call () \
    { \
        timer::irq_handler(MakeInterruptContext((context))); \
    } \
};

#include <aprinter/EndNamespace.h>

#endif
