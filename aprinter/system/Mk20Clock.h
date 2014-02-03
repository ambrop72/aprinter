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

#ifndef AMBROLIB_MK20_CLOCK_H
#define AMBROLIB_MK20_CLOCK_H

#include <stdint.h>
#include <stddef.h>

#include <aprinter/meta/Position.h>
#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/TypeListIndex.h>
#include <aprinter/meta/IndexElemTuple.h>
#include <aprinter/meta/TupleForEach.h>
#include <aprinter/meta/IsEqualFunc.h>
#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/meta/RemoveReference.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/InterruptLock.h>

template <typename FtmSpec, int ChannelIndex>
struct Mk20Clock__IrqCompHelper {
    template <typename IrqTime>
    static void call (IrqTime irq_time) {}
};

#include <aprinter/BeginNamespace.h>

template <uint32_t TScAddr, uint32_t TCntAddr, uint32_t TModAddr, uint32_t TCntinAddr, uint32_t TModeAddr, uint32_t TStatusAddr, uint32_t TScgc6Bit, int TIrq, typename TChannels>
struct Mk20ClockFTM {
    static uint32_t volatile * sc () { return (uint32_t volatile *)TScAddr; }
    static uint32_t volatile * cnt () { return (uint32_t volatile *)TCntAddr; }
    static uint32_t volatile * mod () { return (uint32_t volatile *)TModAddr; }
    static uint32_t volatile * cntin () { return (uint32_t volatile *)TCntinAddr; }
    static uint32_t volatile * mode () { return (uint32_t volatile *)TModeAddr; }
    static uint32_t volatile * status () { return (uint32_t volatile *)TStatusAddr; }
    static uint32_t const Scgc6Bit = TScgc6Bit;
    static const int Irq = TIrq;
    using Channels = TChannels;
};

template <uint32_t TCscAddr, uint32_t TCvAddr>
struct Mk20Clock__Channel {
    static uint32_t volatile * csc () { return (uint32_t volatile *)TCscAddr; }
    static uint32_t volatile * cv () { return (uint32_t volatile *)TCvAddr; }
};

using Mk20ClockFTM0 = Mk20ClockFTM<(uint32_t)&FTM0_SC, (uint32_t)&FTM0_CNT, (uint32_t)&FTM0_MOD, (uint32_t)&FTM0_CNTIN, (uint32_t)&FTM0_MODE, (uint32_t)&FTM0_STATUS, SIM_SCGC6_FTM0, IRQ_FTM0, MakeTypeList<
    Mk20Clock__Channel<(uint32_t)&FTM0_C0SC, (uint32_t)&FTM0_C0V>,
    Mk20Clock__Channel<(uint32_t)&FTM0_C1SC, (uint32_t)&FTM0_C1V>,
    Mk20Clock__Channel<(uint32_t)&FTM0_C2SC, (uint32_t)&FTM0_C2V>,
    Mk20Clock__Channel<(uint32_t)&FTM0_C3SC, (uint32_t)&FTM0_C3V>,
    Mk20Clock__Channel<(uint32_t)&FTM0_C4SC, (uint32_t)&FTM0_C4V>,
    Mk20Clock__Channel<(uint32_t)&FTM0_C5SC, (uint32_t)&FTM0_C5V>,
    Mk20Clock__Channel<(uint32_t)&FTM0_C6SC, (uint32_t)&FTM0_C6V>,
    Mk20Clock__Channel<(uint32_t)&FTM0_C7SC, (uint32_t)&FTM0_C7V>
>>;
using Mk20ClockFTM1 = Mk20ClockFTM<(uint32_t)&FTM1_SC, (uint32_t)&FTM1_CNT, (uint32_t)&FTM1_MOD, (uint32_t)&FTM1_CNTIN, (uint32_t)&FTM1_MODE, (uint32_t)&FTM1_STATUS, SIM_SCGC6_FTM1, IRQ_FTM1, MakeTypeList<
    Mk20Clock__Channel<(uint32_t)&FTM1_C0SC, (uint32_t)&FTM1_C0V>,
    Mk20Clock__Channel<(uint32_t)&FTM1_C1SC, (uint32_t)&FTM1_C1V>
>>;

template <typename, typename, typename, typename, int>
class Mk20ClockInterruptTimer;

template <typename Position, typename Context, int Prescale, typename FtmsList>
class Mk20Clock
: private DebugObject<Context, void>
{
    static_assert(Prescale >= 0, "");
    static_assert(Prescale <= 7, "");
    
    template <typename, typename, typename, typename, int>
    friend class Mk20ClockInterruptTimer;
    
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_init, init)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_init_start, init_start)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_deinit, deinit)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_irq_helper, irq_helper)
    
    static Mk20Clock * self (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
    }
    
public:
    using TimeType = uint32_t;
    
    static constexpr TimeType prescale_divide =
        (Prescale == 0) ? 1 :
        (Prescale == 1) ? 2 :
        (Prescale == 2) ? 4 :
        (Prescale == 3) ? 8 :
        (Prescale == 4) ? 16 :
        (Prescale == 5) ? 32 :
        (Prescale == 6) ? 64 :
        (Prescale == 7) ? 128 : 0;
    
    static constexpr double time_unit = (double)prescale_divide / F_BUS;
    static constexpr double time_freq = (double)F_BUS / prescale_divide;
    
private:
    template <int FtmIndex>
    struct MyFtm {
        using FtmSpec = TypeListGet<FtmsList, FtmIndex>;
        using Channels = typename FtmSpec::Channels;
        
        static void init (Context c)
        {
            SIM_SCGC6 |= FtmSpec::Scgc6Bit;
            *FtmSpec::mode() = FTM_MODE_FTMEN;
            *FtmSpec::sc() = FTM_SC_PS(Prescale) | (FtmIndex == 0 ? FTM_SC_TOIE : 0);
            *FtmSpec::mod() = UINT16_C(0xFFFF);
            *FtmSpec::cntin() = (FtmIndex == 0) ? 1 : 0;
            *FtmSpec::cnt() = 0; // this actually sets CNT to the value in CNTIN above
            *FtmSpec::cntin() = 0;
            NVIC_CLEAR_PENDING(FtmSpec::Irq);
            NVIC_SET_PRIORITY(FtmSpec::Irq, INTERRUPT_PRIORITY);
            NVIC_ENABLE_IRQ(FtmSpec::Irq);
        }
        
        static void init_start (Context c)
        {
            *FtmSpec::sc() |= FTM_SC_CLKS(1);
        }
        
        static void deinit (Context c)
        {
            NVIC_DISABLE_IRQ(FtmSpec::Irq);
            *FtmSpec::sc() = 0;
            *FtmSpec::sc();
            *FtmSpec::sc() = 0;
            *FtmSpec::status();
            *FtmSpec::status() = 0;
            NVIC_CLEAR_PENDING(FtmSpec::Irq);
            SIM_SCGC6 &= ~FtmSpec::Scgc6Bit;
        }
        
        static void irq_handler (InterruptContext<Context> c)
        {
            Mk20Clock *o = self(c);
            
            if (FtmIndex == 0) {
                uint32_t sc = *FtmSpec::sc();
                if (sc & FTM_SC_TOF) {
                    *FtmSpec::sc() = sc & ~FTM_SC_TOF;
                    o->m_offset++;
                }
            }
            *FtmSpec::status();
            *FtmSpec::status() = 0;
            TimeType irq_time = get_time_interrupt(c);
            ChannelsTuple dummy;
            TupleForEachForward(&dummy, Foreach_irq_helper(), c, irq_time);
        }
        
        template <int ChannelIndex>
        struct Channel {
            using ChannelSpec = TypeListGet<Channels, ChannelIndex>;
            
            static void irq_helper (InterruptContext<Context> c, TimeType irq_time)
            {
                Mk20Clock__IrqCompHelper<FtmSpec, ChannelIndex>::call(irq_time);
            }
        };
        
        using ChannelsTuple = IndexElemTuple<Channels, Channel>;
    };
    
    using MyFtmsTuple = IndexElemTuple<FtmsList, MyFtm>;
    
    template <typename FtmSpec>
    using FindFtm = MyFtm<TypeListIndex<FtmsList, IsEqualFunc<FtmSpec>>::value>;
    
public:
    static void init (Context c)
    {
        Mk20Clock *o = self(c);
        
        o->m_offset = 0;
        
        MyFtmsTuple dummy;
        TupleForEachForward(&dummy, Foreach_init(), c);
        TupleForEachForward(&dummy, Foreach_init_start(), c);
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        Mk20Clock *o = self(c);
        o->debugDeinit(c);
        
        MyFtmsTuple dummy;
        TupleForEachReverse(&dummy, Foreach_deinit(), c);
    }
    
    template <typename ThisContext>
    static TimeType getTime (ThisContext c)
    {
        Mk20Clock *o = self(c);
        o->debugAccess(c);
        
        uint16_t offset;
        uint16_t low;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            offset = o->m_offset;
            low = *MyFtm<0>::FtmSpec::cnt();
            if (*MyFtm<0>::FtmSpec::sc() & FTM_SC_TOF) {
                offset++;
                low = *MyFtm<0>::FtmSpec::cnt();
            }
        }
        
        return ((uint32_t)offset << 16) | low;
    }
    
    template <typename FtmSpec>
    static void ftm_irq_handler (InterruptContext<Context> c)
    {
        FindFtm<FtmSpec>::irq_handler(c);
    }
    
private:
    static TimeType get_time_interrupt (Context c)
    {
        Mk20Clock *o = self(c);
        
        uint16_t offset = o->m_offset;
        uint16_t low = *MyFtm<0>::FtmSpec::cnt();
        if (*MyFtm<0>::FtmSpec::sc() & FTM_SC_TOF) {
            offset++;
            low = *MyFtm<0>::FtmSpec::cnt();
        }
        return ((uint32_t)offset << 16) | low;
    }
    
    uint16_t m_offset;
};

#define AMBRO_MK20_CLOCK_FTM_GLOBAL(ftmnum, clock, context) \
extern "C" \
__attribute__((used)) \
void ftm##ftmnum##_isr (void) \
{ \
    (clock).ftm_irq_handler<Mk20ClockFTM##ftmnum>(MakeInterruptContext((context))); \
}

#define AMBRO_MK20_CLOCK_FTM0_GLOBAL(clock, context) AMBRO_MK20_CLOCK_FTM_GLOBAL(0, (clock), (context))
#define AMBRO_MK20_CLOCK_FTM1_GLOBAL(clock, context) AMBRO_MK20_CLOCK_FTM_GLOBAL(1, (clock), (context))

template <typename Position, typename Context, typename Handler, typename TFtmSpec, int TChannelIndex>
class Mk20ClockInterruptTimer
: private DebugObject<Context, void>
{
public:
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using HandlerContext = InterruptContext<Context>;
    using FtmSpec = TFtmSpec;
    static int const ChannelIndex = TChannelIndex;
    
private:
    static Mk20ClockInterruptTimer * self (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
    }
    
    using TheMyFtm = typename Clock::template FindFtm<FtmSpec>;
    using Channel = typename TheMyFtm::template Channel<ChannelIndex>::ChannelSpec;
    
public:
    static void init (Context c)
    {
        Mk20ClockInterruptTimer *o = self(c);
        o->debugInit(c);
        
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = false;
#endif
    }
    
    static void deinit (Context c)
    {
        Mk20ClockInterruptTimer *o = self(c);
        o->debugDeinit(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            *Channel::csc() = 0;
        }
    }
    
    template <typename ThisContext>
    static void setFirst (ThisContext c, TimeType time)
    {
        Mk20ClockInterruptTimer *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(!o->m_running)
        AMBRO_ASSERT(!(*Channel::csc() & FTM_CSC_CHIE))
        
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
            *Channel::cv() = time;
            *Channel::csc();
            *Channel::csc() = FTM_CSC_MSA | FTM_CSC_CHIE;
        }
    }
    
    static void setNext (HandlerContext c, TimeType time)
    {
        Mk20ClockInterruptTimer *o = self(c);
        AMBRO_ASSERT(o->m_running)
        AMBRO_ASSERT((*Channel::csc() & FTM_CSC_CHIE))
        
        o->m_time = time;
        AMBRO_LOCK_T(AtomicTempLock(), c, lock_c) {
            TimeType now = Clock::get_time_interrupt(c);
            now -= time;
            now += clearance;
            if (now < UINT32_C(0x80000000)) {
                time += now;
            }
            *Channel::cv() = time;
        }
    }
    
    template <typename ThisContext>
    static void unset (ThisContext c)
    {
        Mk20ClockInterruptTimer *o = self(c);
        o->debugAccess(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            *Channel::csc() = 0;
        }
        
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = false;
#endif
    }
    
    static void irq_handler (InterruptContext<Context> c, TimeType irq_time)
    {
        Mk20ClockInterruptTimer *o = self(c);
        
        if (!(*Channel::csc() & FTM_CSC_CHIE)) {
            return;
        }
        
        AMBRO_ASSERT(o->m_running)
        
        if ((TimeType)(irq_time - o->m_time) < UINT32_C(0x80000000)) {
            if (!Handler::call(o, c)) {
#ifdef AMBROLIB_ASSERTIONS
                o->m_running = false;
#endif
                *Channel::csc() = 0;
            }
        }
    }
    
private:
    static const TimeType clearance = (128 / Clock::prescale_divide) + 2;
    
    TimeType m_time;
#ifdef AMBROLIB_ASSERTIONS
    bool m_running;
#endif
};

template <typename Position, typename Context, typename Handler>
using Mk20ClockInterruptTimer_Ftm0_Ch0 = Mk20ClockInterruptTimer<Position, Context, Handler, Mk20ClockFTM0, 0>;
template <typename Position, typename Context, typename Handler>
using Mk20ClockInterruptTimer_Ftm0_Ch1 = Mk20ClockInterruptTimer<Position, Context, Handler, Mk20ClockFTM0, 1>;
template <typename Position, typename Context, typename Handler>
using Mk20ClockInterruptTimer_Ftm0_Ch2 = Mk20ClockInterruptTimer<Position, Context, Handler, Mk20ClockFTM0, 2>;
template <typename Position, typename Context, typename Handler>
using Mk20ClockInterruptTimer_Ftm0_Ch3 = Mk20ClockInterruptTimer<Position, Context, Handler, Mk20ClockFTM0, 3>;
template <typename Position, typename Context, typename Handler>
using Mk20ClockInterruptTimer_Ftm0_Ch4 = Mk20ClockInterruptTimer<Position, Context, Handler, Mk20ClockFTM0, 4>;
template <typename Position, typename Context, typename Handler>
using Mk20ClockInterruptTimer_Ftm0_Ch5 = Mk20ClockInterruptTimer<Position, Context, Handler, Mk20ClockFTM0, 5>;
template <typename Position, typename Context, typename Handler>
using Mk20ClockInterruptTimer_Ftm0_Ch6 = Mk20ClockInterruptTimer<Position, Context, Handler, Mk20ClockFTM0, 6>;
template <typename Position, typename Context, typename Handler>
using Mk20ClockInterruptTimer_Ftm0_Ch7 = Mk20ClockInterruptTimer<Position, Context, Handler, Mk20ClockFTM0, 7>;

template <typename Position, typename Context, typename Handler>
using Mk20ClockInterruptTimer_Ftm1_Ch0 = Mk20ClockInterruptTimer<Position, Context, Handler, Mk20ClockFTM1, 0>;
template <typename Position, typename Context, typename Handler>
using Mk20ClockInterruptTimer_Ftm1_Ch1 = Mk20ClockInterruptTimer<Position, Context, Handler, Mk20ClockFTM1, 1>;

#define AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(ftmspec, channel_index, timer, context) \
static_assert( \
    TypesAreEqual<RemoveReference<decltype(timer)>::FtmSpec, ftmspec>::value && \
    RemoveReference<decltype(timer)>::ChannelIndex == channel_index, \
    "Incorrect INTERRUPT_TIMER_GLOBAL macro used" \
); \
template <> \
struct Mk20Clock__IrqCompHelper<ftmspec, channel_index> { \
    template <typename IrqTime> \
    static void call (IrqTime irq_time) \
    { \
        (timer).irq_handler(MakeInterruptContext((context)), irq_time); \
    } \
};

#define AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH0_GLOBAL(timer, context) AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(Mk20ClockFTM0, 0, (timer), (context))
#define AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH1_GLOBAL(timer, context) AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(Mk20ClockFTM0, 1, (timer), (context))
#define AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH2_GLOBAL(timer, context) AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(Mk20ClockFTM0, 2, (timer), (context))
#define AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH3_GLOBAL(timer, context) AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(Mk20ClockFTM0, 3, (timer), (context))
#define AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH4_GLOBAL(timer, context) AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(Mk20ClockFTM0, 4, (timer), (context))
#define AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH5_GLOBAL(timer, context) AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(Mk20ClockFTM0, 5, (timer), (context))
#define AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH6_GLOBAL(timer, context) AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(Mk20ClockFTM0, 6, (timer), (context))
#define AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH7_GLOBAL(timer, context) AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(Mk20ClockFTM0, 7, (timer), (context))

#define AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM1_CH0_GLOBAL(timer, context) AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(Mk20ClockFTM1, 0, (timer), (context))
#define AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM1_CH1_GLOBAL(timer, context) AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(Mk20ClockFTM1, 1, (timer), (context))

#include <aprinter/EndNamespace.h>

#endif
