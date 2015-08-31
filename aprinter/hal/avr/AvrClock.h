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

#include <aprinter/base/Object.h>
#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/IndexElemList.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/meta/MemberType.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/WrapDouble.h>
#include <aprinter/meta/TypeDict.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Likely.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/hal/avr/AvrPins.h>

#include <aprinter/BeginNamespace.h>

using AvrClockDefaultExtraClearance = AMBRO_WRAP_DOUBLE(0.0);

struct AvrClock__PrescaleMode1 {
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

struct AvrClock__PrescaleMode2 {
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
    static uint8_t const wgm1 = WGM##TcNum##1; \
    static uint8_t const wgm3 = WGM##TcNum##3; \
    static uint16_t volatile * icr () { return &ICR##TcNum; } \
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
    static uint8_t const wgm0 = WGM##TcNum##0; \
    using PrescaleMode = ThePrescaleMode; \
};

#define APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(TcNum, ChannelLetter) \
struct AvrClockTcChannel##TcNum##ChannelLetter { \
    using Tc = AvrClockTc##TcNum; \
    static uint8_t const ocie = OCIE##TcNum##ChannelLetter; \
    static uint8_t const ocf = OCF##TcNum##ChannelLetter; \
    static uint16_t volatile * ocr () { return &OCR##TcNum##ChannelLetter; } \
    static uint8_t const com0 = COM##TcNum##ChannelLetter##0; \
    static uint8_t const com1 = COM##TcNum##ChannelLetter##1; \
};

#define APRINTER_DEFINE_AVR_8BIT_TC_CHANNEL(TcNum, ChannelLetter) \
struct AvrClockTcChannel##TcNum##ChannelLetter { \
    using Tc = AvrClockTc##TcNum; \
    static uint8_t const ocie = OCIE##TcNum##ChannelLetter; \
    static uint8_t const ocf = OCF##TcNum##ChannelLetter; \
    static uint8_t volatile * ocr () { return &OCR##TcNum##ChannelLetter; } \
    static uint8_t const com0 = COM##TcNum##ChannelLetter##0; \
    static uint8_t const com1 = COM##TcNum##ChannelLetter##1; \
};

#ifdef TCNT0
APRINTER_DEFINE_AVR_8BIT_TC(0, AvrClock__PrescaleMode1)
APRINTER_DEFINE_AVR_8BIT_TC_CHANNEL(0, A)
APRINTER_DEFINE_AVR_8BIT_TC_CHANNEL(0, B)
#endif

#ifdef TCNT1
APRINTER_DEFINE_AVR_16BIT_TC(1, AvrClock__PrescaleMode1)
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(1, A)
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(1, B)
#ifdef OCF1C
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(1, C)
#endif
#endif

#ifdef TCNT2
APRINTER_DEFINE_AVR_8BIT_TC(2, AvrClock__PrescaleMode2)
APRINTER_DEFINE_AVR_8BIT_TC_CHANNEL(2, A)
APRINTER_DEFINE_AVR_8BIT_TC_CHANNEL(2, B)
#endif

#ifdef TCNT3
APRINTER_DEFINE_AVR_16BIT_TC(3, AvrClock__PrescaleMode1)
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(3, A)
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(3, B)
#ifdef OCF3C
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(3, C)
#endif
#endif

#ifdef TCNT4
APRINTER_DEFINE_AVR_16BIT_TC(4, AvrClock__PrescaleMode1)
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(4, A)
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(4, B)
#ifdef OCF4C
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(4, C)
#endif
#endif

#ifdef TCNT5
APRINTER_DEFINE_AVR_16BIT_TC(5, AvrClock__PrescaleMode1)
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(5, A)
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(5, B)
#ifdef OCF5C
APRINTER_DEFINE_AVR_16BIT_TC_CHANNEL(5, C)
#endif
#endif

#if defined(__AVR_ATmega164A__) || defined(__AVR_ATmega164PA__) || defined(__AVR_ATmega324A__) || \
    defined(__AVR_ATmega324PA__) || defined(__AVR_ATmega644A__) || defined(__AVR_ATmega644PA__) || \
    defined(__AVR_ATmega128__) || defined(__AVR_ATmega1284P__)

using AvrClock__PinMap = MakeTypeList<
    TypeDictEntry<AvrClockTcChannel0A, AvrPin<AvrPortB, 3>>,
    TypeDictEntry<AvrClockTcChannel0B, AvrPin<AvrPortB, 4>>,
    TypeDictEntry<AvrClockTcChannel1A, AvrPin<AvrPortD, 5>>,
    TypeDictEntry<AvrClockTcChannel1B, AvrPin<AvrPortD, 4>>,
    TypeDictEntry<AvrClockTcChannel2A, AvrPin<AvrPortD, 7>>,
    TypeDictEntry<AvrClockTcChannel2B, AvrPin<AvrPortD, 6>>,
    TypeDictEntry<AvrClockTcChannel3A, AvrPin<AvrPortB, 6>>,
    TypeDictEntry<AvrClockTcChannel3B, AvrPin<AvrPortB, 7>>
>;

#elif defined(__AVR_ATmega640__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega1281__) || \
    defined(__AVR_ATmega2560__) || defined(__AVR_ATmega2561__)

using AvrClock__PinMap = MakeTypeList<
    TypeDictEntry<AvrClockTcChannel0A, AvrPin<AvrPortB, 7>>,
    TypeDictEntry<AvrClockTcChannel0B, AvrPin<AvrPortG, 5>>,
    TypeDictEntry<AvrClockTcChannel1A, AvrPin<AvrPortB, 5>>,
    TypeDictEntry<AvrClockTcChannel1B, AvrPin<AvrPortB, 6>>,
    TypeDictEntry<AvrClockTcChannel1C, AvrPin<AvrPortB, 7>>,
    TypeDictEntry<AvrClockTcChannel2A, AvrPin<AvrPortB, 4>>,
    TypeDictEntry<AvrClockTcChannel2B, AvrPin<AvrPortH, 6>>,
    TypeDictEntry<AvrClockTcChannel3A, AvrPin<AvrPortE, 3>>,
    TypeDictEntry<AvrClockTcChannel3B, AvrPin<AvrPortE, 4>>,
    TypeDictEntry<AvrClockTcChannel3C, AvrPin<AvrPortE, 5>>,
    TypeDictEntry<AvrClockTcChannel4A, AvrPin<AvrPortH, 3>>,
    TypeDictEntry<AvrClockTcChannel4B, AvrPin<AvrPortH, 4>>,
    TypeDictEntry<AvrClockTcChannel4C, AvrPin<AvrPortH, 5>>,
    TypeDictEntry<AvrClockTcChannel5A, AvrPin<AvrPortL, 3>>,
    TypeDictEntry<AvrClockTcChannel5B, AvrPin<AvrPortL, 4>>,
    TypeDictEntry<AvrClockTcChannel5C, AvrPin<AvrPortL, 5>>
>;

#else
#error Your device is not supported by AvrClock.
#endif

struct AvrClockTcModeClock {};

template <uint16_t PrescaleDivide>
struct AvrClockTcMode8BitPwm {};

template <uint16_t PrescaleDivide, uint16_t TopVal>
struct AvrClockTcMode16BitPwm {};

template <typename TTc, typename TMode = AvrClockTcModeClock>
struct AvrClockTcSpec {
    using Tc = TTc;
    using Mode = TMode;
};

template <typename, typename, typename, typename, typename>
class AvrClock16BitInterruptTimer;

template <typename, typename, typename, typename, typename>
class AvrClock8BitInterruptTimer;

template <typename, typename, typename, typename>
class AvrClockPwm;

template <typename Context, typename ParentObject, uint16_t TPrescaleDivide, typename TcsList>
class AvrClock {
    template <typename, typename, typename, typename, typename>
    friend class AvrClock16BitInterruptTimer;

    template <typename, typename, typename, typename, typename>
    friend class AvrClock8BitInterruptTimer;
    
    template <typename, typename, typename, typename>
    friend class AvrClockPwm;
    
public:
    struct Object;
    using ClockTcSpec = TypeListGet<TcsList, 0>;
    using ClockTc = typename ClockTcSpec::Tc;
    static_assert(TypesAreEqual<typename ClockTcSpec::Mode, AvrClockTcModeClock>::Value, "First TC must be AvrClockTcModeClock.");
    static_assert(!ClockTc::Is8Bit, "First TC must be 16-bit.");
    static uint16_t const PrescaleDivide = TPrescaleDivide;
    
    using TimeType = uint32_t;
    static constexpr double time_unit = (double)PrescaleDivide / F_CPU;
    static constexpr double time_freq = (double)F_CPU / PrescaleDivide;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_init, init)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_init_start, init_start)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_deinit, deinit)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_Tc, Tc)
    
    template <int TcIndex>
    struct MyTc {
        using TcSpec = TypeListGet<TcsList, TcIndex>;
        using Tc = typename TcSpec::Tc;
        
        template <typename TheMode, typename Dummy = void>
        struct ModeHelper;
        
        template <typename Dummy>
        struct ModeHelper<AvrClockTcModeClock, Dummy> {
            static bool const IsPwmMode = false;
            
            static void init (Context c)
            {
                *Tc::timsk() = 0;
                *Tc::tccra() = 0;
                *Tc::tccrb() = 0;
                *Tc::tcnt() = (TcIndex == 0) ? 1 : 0;
            }
            
            static void init_start (Context c)
            {
                if (TcIndex == 0) {
                    *Tc::timsk() = (1 << Tc::toie);
                }
                *Tc::tccrb() = Tc::PrescaleMode::template DivToPrescale<PrescaleDivide>::Value;
            }
        };
        
        template <uint16_t PwmPrescaleDivide, typename Dummy>
        struct ModeHelper<AvrClockTcMode8BitPwm<PwmPrescaleDivide>, Dummy> {
            static_assert(Tc::Is8Bit, "");
            
            static bool const IsPwmMode = true;
            
            static void init (Context c)
            {
                *Tc::tccrb() = 0;
                *Tc::timsk() = 0;
                *Tc::tccra() = (1 << Tc::wgm0);
                *Tc::tccrb() = Tc::PrescaleMode::template DivToPrescale<PwmPrescaleDivide>::Value;
            }
            
            static void init_start (Context c) {}
        };
        
        template <uint16_t PwmPrescaleDivide, uint16_t TPwmTopVal, typename Dummy>
        struct ModeHelper<AvrClockTcMode16BitPwm<PwmPrescaleDivide, TPwmTopVal>, Dummy> {
            static_assert(!Tc::Is8Bit, "");
            static_assert(TPwmTopVal >= 1, "");
            
            static bool const IsPwmMode = true;
            static uint16_t const PwmTopVal = TPwmTopVal;
            
            static void init (Context c)
            {
                *Tc::tccrb() = 0;
                *Tc::timsk() = 0;
                *Tc::icr() = PwmTopVal;
                *Tc::tccra() = (1 << Tc::wgm1);
                *Tc::tccrb() = (1 << Tc::wgm3) | Tc::PrescaleMode::template DivToPrescale<PwmPrescaleDivide>::Value;
            }
            
            static void init_start (Context c) {}
        };
        
        using TheModeHelper = ModeHelper<typename TcSpec::Mode>;
        
        static void init (Context c) { TheModeHelper::init(c); }
        static void init_start (Context c) { TheModeHelper::init_start(c); }
        
        static void deinit (Context c)
        {
            *Tc::timsk() = 0;
            *Tc::tccrb() = 0;
        }
    };
    
    using MyTcsList = IndexElemList<TcsList, MyTc>;
    
    template <typename Tc>
    using FindTc = MyTc<TypeListIndexMapped<MyTcsList, GetMemberType_Tc, Tc>::Value>;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->m_offset = 0;
        
        memory_barrier();
        
        ListForEachForward<MyTcsList>(Foreach_init(), c);
        ListForEachForward<MyTcsList>(Foreach_init_start(), c);
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
        
        ListForEachReverse<MyTcsList>(Foreach_deinit(), c);
        
        memory_barrier();
    }
    
    template <typename ThisContext>
    static TimeType getTime (ThisContext c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
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
    struct Object : public ObjBase<AvrClock, ParentObject, MakeTypeList<TheDebugObject>> {
        uint16_t m_offset;
    };
};

template <typename Context, typename ParentObject, typename Handler, typename TTcChannel, typename ExtraClearance>
class AvrClock16BitInterruptTimer {
public:
    struct Object;
    typedef typename Context::Clock Clock;
    typedef typename Clock::TimeType TimeType;
    typedef AtomicContext<Context> HandlerContext;
    using TcChannel = TTcChannel;
    using Tc = typename TcChannel::Tc;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    using MyTc = typename Clock::template FindTc<Tc>;
    static_assert(TypesAreEqual<typename MyTc::TcSpec::Mode, AvrClockTcModeClock>::Value, "TC must be AvrClockTcModeClock.");
    static_assert(!Tc::Is8Bit, "");
    
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
            *Tc::timsk() &= ~(1 << TcChannel::ocie);
        }
    }
    
    template <typename ThisContext>
    static void setFirst (ThisContext c, TimeType time)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(!o->m_running)
        AMBRO_ASSERT(!(*Tc::timsk() & (1 << TcChannel::ocie)))
        
        o->m_time = time;
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = true;
#endif
        
        memory_barrier();
        
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
        TheDebugObject::access(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            *Tc::timsk() &= ~(1 << TcChannel::ocie);
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
    static const TimeType clearance = MaxValue<TimeType>((35 / Clock::PrescaleDivide) + 2, ExtraClearance::value() * Clock::time_freq);
    static const TimeType minus_clearance = -clearance;
    
public:
    struct Object : public ObjBase<AvrClock16BitInterruptTimer, ParentObject, MakeTypeList<TheDebugObject>> {
        TimeType m_time;
#ifdef AMBROLIB_ASSERTIONS
        bool m_running;
#endif
    };
};

template <typename Context, typename ParentObject, typename Handler, typename TTcChannel, typename ExtraClearance>
class AvrClock8BitInterruptTimer {
public:
    struct Object;
    typedef typename Context::Clock Clock;
    typedef typename Clock::TimeType TimeType;
    typedef AtomicContext<Context> HandlerContext;
    using TcChannel = TTcChannel;
    using Tc = typename TcChannel::Tc;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    using MyTc = typename Clock::template FindTc<Tc>;
    static_assert(TypesAreEqual<typename MyTc::TcSpec::Mode, AvrClockTcModeClock>::Value, "TC must be AvrClockTcModeClock.");
    static_assert(Tc::Is8Bit, "");
    
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
            *Tc::timsk() &= ~(1 << TcChannel::ocie);
        }
    }
    
    template <typename ThisContext>
    static void setFirst (ThisContext c, TimeType time)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(!o->m_running)
        AMBRO_ASSERT(!(*Tc::timsk() & (1 << TcChannel::ocie)))
        
        o->m_time = time;
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = true;
#endif
        
        memory_barrier();
        
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
        TheDebugObject::access(c);
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
        TheDebugObject::access(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            *Tc::timsk() &= ~(1 << TcChannel::ocie);
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
    static const TimeType clearance = MaxValue<TimeType>((35 / Clock::PrescaleDivide) + 2, ExtraClearance::value() * Clock::time_freq);
    static const TimeType minus_clearance = -clearance;
    
public:
    struct Object : public ObjBase<AvrClock8BitInterruptTimer, ParentObject, MakeTypeList<TheDebugObject>> {
        TimeType m_time;
#ifdef AMBROLIB_ASSERTIONS
        bool m_running;
#endif
    };
};

template <typename TcChannel, typename ExtraClearance = AvrClockDefaultExtraClearance>
class AvrClockInterruptTimerService {
    AMBRO_STRUCT_IF(BitnessChoice, TcChannel::Tc::Is8Bit) {
        template <typename Context, typename ParentObject, typename Handler>
        using InterruptTimer = AvrClock8BitInterruptTimer<Context, ParentObject, Handler, TcChannel, ExtraClearance>;
    } AMBRO_STRUCT_ELSE(BitnessChoice) {
        template <typename Context, typename ParentObject, typename Handler>
        using InterruptTimer = AvrClock16BitInterruptTimer<Context, ParentObject, Handler, TcChannel, ExtraClearance>;
    };
    
public:
    template <typename Context, typename ParentObject, typename Handler>
    using InterruptTimer = typename BitnessChoice::template InterruptTimer<Context, ParentObject, Handler>;
};

#define AMBRO_AVR_CLOCK_ISRS(FirstTcNum, Clock, context) \
static_assert(TypesAreEqual<Clock::ClockTc, AvrClockTc##FirstTcNum>::Value, "Incorrect FirstTcNum specified in AMBRO_AVR_CLOCK_ISRS."); \
ISR(TIMER##FirstTcNum##_OVF_vect) \
{ \
    Clock::clock_timer_ovf_isr(MakeAtomicContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_ISRS(TcNum, ChannelLetter, Timer, context) \
static_assert(TypesAreEqual<Timer::TcChannel, AvrClockTcChannel##TcNum##ChannelLetter>::Value, "Incorrect AMBRO_AVR_CLOCK_INTERRUPT_TIMER_ISRS macro used."); \
ISR(TIMER##TcNum##_COMP##ChannelLetter##_vect) \
{ \
    Timer::timer_comp_isr(MakeAtomicContext((context))); \
}

/*
 * NOTE concerning hardware PWM.
 * 
 * We're using "Phase Correct PWM mode", because in "Fast PWM" mode,
 * it is impossible to get zero duty cycle without glitches.
 * 
 * See: http://stackoverflow.com/questions/23853066/how-to-achieve-zero-duty-cycle-pwm-in-avr-without-glitches
 */

template <typename Context, typename ParentObject, typename TcChannel, typename Pin>
class AvrClockPwm {
public:
    struct Object;
    using Clock = typename Context::Clock;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    using Tc = typename TcChannel::Tc;
    using MyTc = typename Clock::template FindTc<Tc>;
    static_assert(MyTc::TheModeHelper::IsPwmMode, "TC must be configured in PWM mode.");
    static_assert(TypesAreEqual<Pin, typename TypeDictFind<AvrClock__PinMap, TcChannel>::Result>::Value, "Invalid Pin specified.");
    static uint8_t const ComMask = (1 << TcChannel::com1) | (1 << TcChannel::com0);
    
    template <bool Is8Bit, typename Dummy=void>
    struct BitSpecific {
        static uint32_t const PwmTopValPlus1 = (uint32_t)MyTc::TheModeHelper::PwmTopVal + 1;
        
        using TheDutyCycleType = ChooseInt<BitsInInt<PwmTopValPlus1>::Value>;
        static TheDutyCycleType const TheMaxDutyCycle = PwmTopValPlus1;
        
        template <typename ThisContext>
        static void setDutyCycle (ThisContext c, TheDutyCycleType duty_cycle)
        {
            uint16_t ocr_val = (duty_cycle < MaxDutyCycle) ? duty_cycle : (MaxDutyCycle - 1);
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                *TcChannel::ocr() = ocr_val;
            }
        }
    };
    
    template <typename Dummy>
    struct BitSpecific<true, Dummy> {
        using TheDutyCycleType = uint16_t;
        static TheDutyCycleType const TheMaxDutyCycle = UINT16_C(0x100);
        
        template <typename ThisContext>
        static void setDutyCycle (ThisContext c, TheDutyCycleType duty_cycle)
        {
            *TcChannel::ocr() = (duty_cycle < MaxDutyCycle) ? duty_cycle : (MaxDutyCycle - 1);
        }
    };
    
    using TheBitSpecific = BitSpecific<Tc::Is8Bit>;
    
public:
    using DutyCycleType = typename TheBitSpecific::TheDutyCycleType;
    static DutyCycleType const MaxDutyCycle = TheBitSpecific::TheMaxDutyCycle;
    
    static void init (Context c)
    {
        *TcChannel::ocr() = 0;
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            *Tc::tccra() = (*Tc::tccra() & ~ComMask) | (1 << TcChannel::com1);
            *Tc::tifr() = (1 << TcChannel::ocf);
        }
        
        while (!(*Tc::tifr() & (1 << TcChannel::ocf)));
        
        Context::Pins::template set<Pin>(c, false);
        Context::Pins::template setOutput<Pin>(c);
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            *Tc::tccra() &= ~ComMask;
        }
        
        Context::Pins::template set<Pin>(c, false);
    }
    
    template <typename ThisContext>
    static void setDutyCycle (ThisContext c, DutyCycleType duty_cycle)
    {
        AMBRO_ASSERT(duty_cycle <= MaxDutyCycle)
        
        TheBitSpecific::setDutyCycle(c, duty_cycle);
    }
    
    static void emergencySetOff ()
    {
        *Tc::tccra() &= ~ComMask;
        Context::Pins::template emergencySet<Pin>(false);
    }
    
public:
    struct Object : public ObjBase<AvrClockPwm, ParentObject, MakeTypeList<TheDebugObject>> {};
};

template <typename TcChannel, typename Pin>
struct AvrClockPwmService {
    template <typename Context, typename ParentObject>
    using Pwm = AvrClockPwm<Context, ParentObject, TcChannel, Pin>;
};

#include <aprinter/EndNamespace.h>

#endif
