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

#ifndef AMBROLIB_AVR_CLOCK_H
#define AMBROLIB_AVR_CLOCK_H

#include <stdint.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sfr_defs.h>

#include <aprinter/meta/Object.h>
#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/IndexElemList.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Likely.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

struct AvrClockPrescaleMode1 {
    template <uint16_t Div>
    struct DivToPrescale {
        static uint8_t const Value =
            (Div == 1) ? 1 :
            (Div == 8) ? 2 :
            (Div == 64) ? 3 :
            (Div == 256) ? 4 :
            (Div == 1024) ? 5 :
            0;
        static_assert(Value != 0, "");
    };
};

struct AvrClockPrescaleMode2 {
    template <uint16_t Div>
    struct DivToPrescale {
        static uint8_t const Value =
            (Div == 1) ? 1 :
            (Div == 8) ? 2 :
            (Div == 32) ? 3 :
            (Div == 64) ? 4 :
            (Div == 128) ? 5 :
            (Div == 256) ? 6 :
            (Div == 1024) ? 7 :
            0;
        static_assert(Value != 0, "");
    };
};

#define APRINTER_DEFINE_AVR_16BIT_TC(TcNum, ThePrescaleMode) \
struct AvrClockTc##TcNum { \
    static bool const Is8Bit = false; \
    static uint8_t volatile * timsk () { return &TIMSK##TcNum; } \
    static uint8_t volatile * tccra () { return &TCCR##TcNum##A; } \
    static uint8_t volatile * tccrb () { return &TCCR##TcNum##B; } \
    static uint8_t volatile * tifr () { return &TIFR##TcNum; } \
    static uint16_t volatile * tcnt () { return &TCNT##TcNum; } \
    static uint8_t const toie = TOIE##TcNum; \
    static uint8_t const tov = TOV##TcNum; \
    using PrescaleMode = ThePrescaleMode; \
};

#define APRINTER_DEFINE_AVR_8BIT_TC(TcNum, ThePrescaleMode) \
struct AvrClockTc##TcNum { \
    static bool const Is8Bit = true; \
    static uint8_t volatile * timsk () { return &TIMSK##TcNum; } \
    static uint8_t volatile * tccra () { return &TCCR##TcNum##A; } \
    static uint8_t volatile * tccrb () { return &TCCR##TcNum##B; } \
    static uint8_t volatile * tifr () { return &TIFR##TcNum; } \
    static uint8_t volatile * tcnt () { return &TCNT##TcNum; } \
    static uint8_t const toie = TOIE##TcNum; \
    static uint8_t const tov = TOV##TcNum; \
    using PrescaleMode = ThePrescaleMode; \
};

#define APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(TcNum, ChannelLetter) \
struct AvrClockTcChannel##TcNum##ChannelLetter { \
    using Tc = AvrClockTc##TcNum; \
    static uint8_t const ocie = OCIE##TcNum##ChannelLetter; \
    static uint8_t const ocf = OCF##TcNum##ChannelLetter; \
    static uint16_t volatile * ocr () { return &OCR##TcNum##ChannelLetter; } \
};

#define APRINTER_DEFINE_AVR_8BIT_TC_CHANNEL(TcNum, ChannelLetter) \
struct AvrClockTcChannel##TcNum##ChannelLetter { \
    using Tc = AvrClockTc##TcNum; \
    static uint8_t const ocie = OCIE##TcNum##ChannelLetter; \
    static uint8_t const ocf = OCF##TcNum##ChannelLetter; \
    static uint8_t volatile * ocr () { return &OCR##TcNum##ChannelLetter; } \
};

#ifdef TCNT0
APRINTER_DEFINE_AVR_8BIT_TC(0, AvrClockPrescaleMode1)
APRINTER_DEFINE_AVR_8BIT_TC_CHANNEL(0, A)
APRINTER_DEFINE_AVR_8BIT_TC_CHANNEL(0, B)
#endif

#ifdef TCNT1
APRINTER_DEFINE_AVR_16BIT_TC(1, AvrClockPrescaleMode1)
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(1, A)
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(1, B)
#ifdef OCF1C
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(1, C)
#endif
#endif

#ifdef TCNT2
APRINTER_DEFINE_AVR_8BIT_TC(2, AvrClockPrescaleMode2)
APRINTER_DEFINE_AVR_8BIT_TC_CHANNEL(2, A)
APRINTER_DEFINE_AVR_8BIT_TC_CHANNEL(2, B)
#endif

#ifdef TCNT3
APRINTER_DEFINE_AVR_16BIT_TC(3, AvrClockPrescaleMode1)
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(3, A)
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(3, B)
#ifdef OCF3C
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(3, C)
#endif
#endif

#ifdef TCNT4
APRINTER_DEFINE_AVR_16BIT_TC(4, AvrClockPrescaleMode1)
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(4, A)
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(4, B)
#ifdef OCF4C
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(4, C)
#endif
#endif

#ifdef TCNT5
APRINTER_DEFINE_AVR_16BIT_TC(5, AvrClockPrescaleMode1)
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(5, A)
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(5, B)
#ifdef OCF5C
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(5, C)
#endif
#endif

template <typename Context, typename ParentObject, uint16_t TPrescaleDivide, typename TcsList>
class AvrClock {
public:
    using ClockTc = TypeListGet<TcsList, 0>;
    static_assert(!ClockTc::Is8Bit, "First TC must be 16-bit.");
    struct Object;
    using TimeType = uint32_t;
    static uint16_t const PrescaleDivide = TPrescaleDivide;
    static constexpr double time_unit = (double)PrescaleDivide / F_CPU;
    static constexpr double time_freq = (double)F_CPU / PrescaleDivide;
    
private:
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_init, init)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_init_start, init_start)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_deinit, deinit)
    
    template <int TcIndex>
    struct MyTc {
        using TcSpec = TypeListGet<TcsList, TcIndex>;
        
        static void init (Context c)
        {
            *TcSpec::timsk() = 0;
            *TcSpec::tccra() = 0;
            *TcSpec::tccrb() = 0;
            *TcSpec::tcnt() = (TcIndex == 0) ? 1 : 0;
        }
        
        static void init_start (Context c)
        {
            if (TcIndex == 0) {
                *TcSpec::timsk() = (1 << TcSpec::toie);
            }
            *TcSpec::tccrb() = TcSpec::PrescaleMode::template DivToPrescale<PrescaleDivide>::Value;
        }
        
        static void deinit (Context c)
        {
            *TcSpec::timsk() = 0;
            *TcSpec::tccrb() = 0;
        }
    };
    
    using MyTcsList = IndexElemList<TcsList, MyTc>;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->m_offset = 0;
        
        ListForEachForward<MyTcsList>(Foreach_init(), c);
        ListForEachForward<MyTcsList>(Foreach_init_start(), c);
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->debugDeinit(c);
        
        ListForEachReverse<MyTcsList>(Foreach_deinit(), c);
    }
    
    template <typename ThisContext>
    static TimeType getTime (ThisContext c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        uint32_t now;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            uint16_t offset = o->m_offset;
            asm volatile (
                "    movw %C[now],%A[offset]\n"
                "    lds %A[now],%[tcnt]+0\n"
                "    sbis %[tifr],%[tov]\n"
                "    rjmp no_overflow_%=\n"
                "    lds %A[now],%[tcnt]+0\n"
                "    subi %C[now],-1\n"
                "    sbci %D[now],-1\n"
                "no_overflow_%=:\n"
                "    lds %B[now],%[tcnt]+1\n"
            : [now] "=&d" (now)
            : [offset] "r" (offset),
              [tcnt] "n" (_SFR_MEM_ADDR(*ClockTc::tcnt())),
              [tifr] "I" (_SFR_IO_ADDR(*ClockTc::tifr())),
              [tov] "n" (ClockTc::tov)
            );
        }
        return now;
    }
    
    static void clock_timer_ovf_isr (AtomicContext<Context> c)
    {
        auto *o = Object::self(c);
        o->m_offset++;
    }
    
public:
    struct Object : public ObjBase<AvrClock, ParentObject, EmptyTypeList>,
        public DebugObject<Context, void>
    {
        uint16_t m_offset;
    };
};

template <typename Context, typename ParentObject, typename Handler, typename TTcChannel>
class AvrClock16BitInterruptTimer {
public:
    struct Object;
    typedef typename Context::Clock Clock;
    typedef typename Clock::TimeType TimeType;
    typedef AtomicContext<Context> HandlerContext;
    using TcChannel = TTcChannel;
    using Tc = typename TcChannel::Tc;
    
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
            *Tc::timsk() &= ~(1 << TcChannel::ocie);
        }
    }
    
    template <typename ThisContext>
    static void setFirst (ThisContext c, TimeType time)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(!o->m_running)
        AMBRO_ASSERT(!(*Tc::timsk() & (1 << TcChannel::ocie)))
        
        o->m_time = time;
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = true;
#endif
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            uint16_t now_high = Clock::Object::self(c)->m_offset;
            uint16_t now_low;
            
            asm volatile (
                "    lds %A[now_low],%[tcnt]+0\n"
                "    sbis %[tifr],%[tov]\n"
                "    rjmp no_overflow_%=\n"
                "    lds %A[now_low],%[tcnt]+0\n"
                "    subi %A[now_high],-1\n"
                "    sbci %B[now_high],-1\n"
                "no_overflow_%=:\n"
                "    lds %B[now_low],%[tcnt]+1\n"
                "    sub %A[now_low],%A[time]\n"
                "    sbc %B[now_low],%B[time]\n"
                "    sbc %A[now_high],%C[time]\n"
                "    sbc %B[now_high],%D[time]\n"
                "    subi %A[now_low],%[mcA]\n"
                "    sbci %B[now_low],%[mcB]\n"
                "    sbci %A[now_high],%[mcC]\n"
                "    sbci %B[now_high],%[mcD]\n"
                "    brmi no_saturation_%=\n"
                "    add %A[time],%A[now_low]\n"
                "    adc %B[time],%B[now_low]\n"
                "no_saturation_%=:\n"
                "    sts %[ocr]+1,%B[time]\n"
                "    sts %[ocr]+0,%A[time]\n"
                "    lds %A[now_low],%[timsk]\n"
                "    ori %[now_low],1<<%[ocie]\n"
                "    sts %[timsk],%[now_low]\n"
                : [now_low] "=&d" (now_low),
                  [now_high] "=&d" (now_high),
                  [time] "=&r" (time)
                : "[now_high]" (now_high),
                  "[time]" (time),
                  [mcA] "n" ((minus_clearance >> 0) & 0xFF),
                  [mcB] "n" ((minus_clearance >> 8) & 0xFF),
                  [mcC] "n" ((minus_clearance >> 16) & 0xFF),
                  [mcD] "n" ((minus_clearance >> 24) & 0xFF),
                  [tcnt] "n" (_SFR_MEM_ADDR(*Clock::ClockTc::tcnt())),
                  [tifr] "I" (_SFR_IO_ADDR(*Clock::ClockTc::tifr())),
                  [tov] "n" (Clock::ClockTc::tov),
                  [ocr] "n" (_SFR_MEM_ADDR(*TcChannel::ocr())),
                  [timsk] "n" (_SFR_MEM_ADDR(*Tc::timsk())),
                  [ocie] "n" (TcChannel::ocie)
            );
        }
    }
    
    static void setNext (HandlerContext c, TimeType time)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_running)
        AMBRO_ASSERT(*Tc::timsk() & (1 << TcChannel::ocie))
        
        o->m_time = time;
        
        uint16_t now_high = Clock::Object::self(c)->m_offset;
        uint16_t now_low;
        
        asm volatile (
            "    lds %A[now_low],%[tcnt]+0\n"
            "    sbis %[tifr],%[tov]\n"
            "    rjmp no_overflow_%=\n"
            "    lds %A[now_low],%[tcnt]+0\n"
            "    subi %A[now_high],-1\n"
            "    sbci %B[now_high],-1\n"
            "no_overflow_%=:\n"
            "    lds %B[now_low],%[tcnt]+1\n"
            "    sub %A[now_low],%A[time]\n"
            "    sbc %B[now_low],%B[time]\n"
            "    sbc %A[now_high],%C[time]\n"
            "    sbc %B[now_high],%D[time]\n"
            "    subi %A[now_low],%[mcA]\n"
            "    sbci %B[now_low],%[mcB]\n"
            "    sbci %A[now_high],%[mcC]\n"
            "    sbci %B[now_high],%[mcD]\n"
            "    brmi no_saturation_%=\n"
            "    add %A[time],%A[now_low]\n"
            "    adc %B[time],%B[now_low]\n"
            "no_saturation_%=:\n"
            "    sts %[ocr]+1,%B[time]\n"
            "    sts %[ocr]+0,%A[time]\n"
            : [now_low] "=&d" (now_low),
              [now_high] "=&d" (now_high),
              [time] "=&r" (time)
            : "[now_high]" (now_high),
              "[time]" (time),
              [mcA] "n" ((minus_clearance >> 0) & 0xFF),
              [mcB] "n" ((minus_clearance >> 8) & 0xFF),
              [mcC] "n" ((minus_clearance >> 16) & 0xFF),
              [mcD] "n" ((minus_clearance >> 24) & 0xFF),
              [tcnt] "n" (_SFR_MEM_ADDR(*Clock::ClockTc::tcnt())),
              [tifr] "I" (_SFR_IO_ADDR(*Clock::ClockTc::tifr())),
              [tov] "n" (Clock::ClockTc::tov),
              [ocr] "n" (_SFR_MEM_ADDR(*TcChannel::ocr()))
        );
    }
    
    template <typename ThisContext>
    static void unset (ThisContext c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            *Tc::timsk() &= ~(1 << TcChannel::ocie);
#ifdef AMBROLIB_ASSERTIONS
            o->m_running = false;
#endif
        }
    }
    
    static void timer_comp_isr (AtomicContext<Context> c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_running)
        AMBRO_ASSERT(*Tc::timsk() & (1 << TcChannel::ocie))
        
        uint16_t now_low;
        uint16_t now_high = Clock::Object::self(c)->m_offset;
        
        asm volatile (
            "    lds %A[now_low],%[tcnt]+0\n"
            "    sbis %[tifr],%[tov]\n"
            "    rjmp no_overflow_%=\n"
            "    lds %A[now_low],%[tcnt]+0\n"
            "    subi %A[now_high],-1\n"
            "    sbci %B[now_high],-1\n"
            "no_overflow_%=:\n"
            "    lds %B[now_low],%[tcnt]+1\n"
            "    sub %A[now_low],%A[time]\n"
            "    sbc %B[now_low],%B[time]\n"
            "    sbc %A[now_high],%C[time]\n"
            "    sbc %B[now_high],%D[time]\n"
            : [now_low] "=&r" (now_low),
              [now_high] "=&d" (now_high)
            : "[now_high]" (now_high),
              [time] "r" (o->m_time),
              [tcnt] "n" (_SFR_MEM_ADDR(*Clock::ClockTc::tcnt())),
              [tifr] "I" (_SFR_IO_ADDR(*Clock::ClockTc::tifr())),
              [tov] "n" (Clock::ClockTc::tov)
        );
        
        if (now_high < UINT16_C(0x8000)) {
            if (!Handler::call(c)) {
#ifdef AMBROLIB_ASSERTIONS
                o->m_running = false;
#endif
                *Tc::timsk() &= ~(1 << TcChannel::ocie);
            }
        }
    }
    
private:
    static const TimeType clearance = (35 / Clock::PrescaleDivide) + 2;
    static const TimeType minus_clearance = -clearance;
    
public:
    struct Object : public ObjBase<AvrClock16BitInterruptTimer, ParentObject, EmptyTypeList>,
        public DebugObject<Context, void>
    {
        TimeType m_time;
#ifdef AMBROLIB_ASSERTIONS
        bool m_running;
#endif
    };
};

template <typename Context, typename ParentObject, typename Handler, typename TTcChannel>
class AvrClock8BitInterruptTimer {
public:
    struct Object;
    typedef typename Context::Clock Clock;
    typedef typename Clock::TimeType TimeType;
    typedef AtomicContext<Context> HandlerContext;
    using TcChannel = TTcChannel;
    using Tc = typename TcChannel::Tc;
    
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
            *Tc::timsk() &= ~(1 << TcChannel::ocie);
        }
    }
    
    template <typename ThisContext>
    static void setFirst (ThisContext c, TimeType time)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(!o->m_running)
        AMBRO_ASSERT(!(*Tc::timsk() & (1 << TcChannel::ocie)))
        
        o->m_time = time;
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = true;
#endif
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            uint16_t now_high = Clock::Object::self(c)->m_offset;
            uint16_t now_low;
            
            asm volatile (
                "    lds %A[now_low],%[tcnt]+0\n"
                "    sbis %[tifr],%[tov]\n"
                "    rjmp no_overflow_%=\n"
                "    lds %A[now_low],%[tcnt]+0\n"
                "    subi %A[now_high],-1\n"
                "    sbci %B[now_high],-1\n"
                "no_overflow_%=:\n"
                "    lds %B[now_low],%[tcnt]+1\n"
                "    sub %A[now_low],%A[time]\n"
                "    sbc %B[now_low],%B[time]\n"
                "    sbc %A[now_high],%C[time]\n"
                "    sbc %B[now_high],%D[time]\n"
                "    subi %A[now_low],%[mcA]\n"
                "    sbci %B[now_low],%[mcB]\n"
                "    sbci %A[now_high],%[mcC]\n"
                "    sbci %B[now_high],%[mcD]\n"
                "    brmi no_saturation_%=\n"
                "    add %A[time],%A[now_low]\n"
                "no_saturation_%=:\n"
                "    sts %[ocr],%A[time]\n"
                "    lds %A[now_low],%[timsk]\n"
                "    ori %[now_low],1<<%[ocie]\n"
                "    sts %[timsk],%[now_low]\n"
                : [now_low] "=&d" (now_low),
                  [now_high] "=&d" (now_high),
                  [time] "=&r" (time)
                : "[now_high]" (now_high),
                  "[time]" (time),
                  [mcA] "n" ((minus_clearance >> 0) & 0xFF),
                  [mcB] "n" ((minus_clearance >> 8) & 0xFF),
                  [mcC] "n" ((minus_clearance >> 16) & 0xFF),
                  [mcD] "n" ((minus_clearance >> 24) & 0xFF),
                  [tcnt] "n" (_SFR_MEM_ADDR(*Clock::ClockTc::tcnt())),
                  [tifr] "I" (_SFR_IO_ADDR(*Clock::ClockTc::tifr())),
                  [tov] "n" (Clock::ClockTc::tov),
                  [ocr] "n" (_SFR_MEM_ADDR(*TcChannel::ocr())),
                  [timsk] "n" (_SFR_MEM_ADDR(*Tc::timsk())),
                  [ocie] "n" (TcChannel::ocie)
            );
        }
    }
    
    static void setNext (HandlerContext c, TimeType time)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_running)
        AMBRO_ASSERT(*Tc::timsk() & (1 << TcChannel::ocie))
        
        o->m_time = time;
        
        uint16_t now_high = Clock::Object::self(c)->m_offset;
        uint16_t now_low;
        
        asm volatile (
            "    lds %A[now_low],%[tcnt]+0\n"
            "    sbis %[tifr],%[tov]\n"
            "    rjmp no_overflow_%=\n"
            "    lds %A[now_low],%[tcnt]+0\n"
            "    subi %A[now_high],-1\n"
            "    sbci %B[now_high],-1\n"
            "no_overflow_%=:\n"
            "    lds %B[now_low],%[tcnt]+1\n"
            "    sub %A[now_low],%A[time]\n"
            "    sbc %B[now_low],%B[time]\n"
            "    sbc %A[now_high],%C[time]\n"
            "    sbc %B[now_high],%D[time]\n"
            "    subi %A[now_low],%[mcA]\n"
            "    sbci %B[now_low],%[mcB]\n"
            "    sbci %A[now_high],%[mcC]\n"
            "    sbci %B[now_high],%[mcD]\n"
            "    brmi no_saturation_%=\n"
            "    add %A[time],%A[now_low]\n"
            "no_saturation_%=:\n"
            "    sts %[ocr],%A[time]\n"
            : [now_low] "=&d" (now_low),
              [now_high] "=&d" (now_high),
              [time] "=&r" (time)
            : "[now_high]" (now_high),
              "[time]" (time),
              [mcA] "n" ((minus_clearance >> 0) & 0xFF),
              [mcB] "n" ((minus_clearance >> 8) & 0xFF),
              [mcC] "n" ((minus_clearance >> 16) & 0xFF),
              [mcD] "n" ((minus_clearance >> 24) & 0xFF),
              [tcnt] "n" (_SFR_MEM_ADDR(*Clock::ClockTc::tcnt())),
              [tifr] "I" (_SFR_IO_ADDR(*Clock::ClockTc::tifr())),
              [tov] "n" (Clock::ClockTc::tov),
              [ocr] "n" (_SFR_MEM_ADDR(*TcChannel::ocr()))
        );
    }
    
    template <typename ThisContext>
    static void unset (ThisContext c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            *Tc::timsk() &= ~(1 << TcChannel::ocie);
#ifdef AMBROLIB_ASSERTIONS
            o->m_running = false;
#endif
        }
    }
    
    static void timer_comp_isr (AtomicContext<Context> c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_running)
        AMBRO_ASSERT(*Tc::timsk() & (1 << TcChannel::ocie))
        
        uint16_t now_low;
        uint16_t now_high = Clock::Object::self(c)->m_offset;
        
        asm volatile (
            "    lds %A[now_low],%[tcnt]+0\n"
            "    sbis %[tifr],%[tov]\n"
            "    rjmp no_overflow_%=\n"
            "    lds %A[now_low],%[tcnt]+0\n"
            "    subi %A[now_high],-1\n"
            "    sbci %B[now_high],-1\n"
            "no_overflow_%=:\n"
            "    lds %B[now_low],%[tcnt]+1\n"
            "    sub %A[now_low],%A[time]\n"
            "    sbc %B[now_low],%B[time]\n"
            "    sbc %A[now_high],%C[time]\n"
            "    sbc %B[now_high],%D[time]\n"
            : [now_low] "=&r" (now_low),
              [now_high] "=&d" (now_high)
            : "[now_high]" (now_high),
              [time] "r" (o->m_time),
              [tcnt] "n" (_SFR_MEM_ADDR(*Clock::ClockTc::tcnt())),
              [tifr] "I" (_SFR_IO_ADDR(*Clock::ClockTc::tifr())),
              [tov] "n" (Clock::ClockTc::tov)
        );
        
        if (now_high < UINT16_C(0x8000)) {
            if (!Handler::call(c)) {
#ifdef AMBROLIB_ASSERTIONS
                o->m_running = false;
#endif
                *Tc::timsk() &= ~(1 << TcChannel::ocie);
            }
        }
    }
    
private:
    static const TimeType clearance = (35 / Clock::PrescaleDivide) + 2;
    static const TimeType minus_clearance = -clearance;
    
public:
    struct Object : public ObjBase<AvrClock8BitInterruptTimer, ParentObject, EmptyTypeList>,
        public DebugObject<Context, void>
    {
        TimeType m_time;
#ifdef AMBROLIB_ASSERTIONS
        bool m_running;
#endif
    };
};

template <typename TcChannel>
class AvrClockInterruptTimerService {
    AMBRO_STRUCT_IF(BitnessChoice, TcChannel::Tc::Is8Bit) {
        template <typename Context, typename ParentObject, typename Handler>
        using InterruptTimer = AvrClock8BitInterruptTimer<Context, ParentObject, Handler, TcChannel>;
    } AMBRO_STRUCT_ELSE(BitnessChoice) {
        template <typename Context, typename ParentObject, typename Handler>
        using InterruptTimer = AvrClock16BitInterruptTimer<Context, ParentObject, Handler, TcChannel>;
    };
    
public:
    template <typename Context, typename ParentObject, typename Handler>
    using InterruptTimer = typename BitnessChoice::template InterruptTimer<Context, ParentObject, Handler>;
};

#define AMBRO_AVR_CLOCK_ISRS(FirstTcNum, Clock, context) \
static_assert(TypesAreEqual<Clock::ClockTc, AvrClockTc##FirstTcNum>::value, "Incorrect FirstTcNum specified in AMBRO_AVR_CLOCK_ISRS."); \
ISR(TIMER##FirstTcNum##_OVF_vect) \
{ \
    Clock::clock_timer_ovf_isr(MakeAtomicContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_ISRS(TcNum, ChannelLetter, Timer, context) \
static_assert(TypesAreEqual<Timer::TcChannel, AvrClockTcChannel##TcNum##ChannelLetter>::value, "Incorrect AMBRO_AVR_CLOCK_INTERRUPT_TIMER_ISRS macro used."); \
ISR(TIMER##TcNum##_COMP##ChannelLetter##_vect) \
{ \
    Timer::timer_comp_isr(MakeAtomicContext((context))); \
}

#include <aprinter/EndNamespace.h>

#endif
