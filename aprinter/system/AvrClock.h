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

#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Likely.h>
#include <aprinter/system/AvrLock.h>
#include <aprinter/system/AvrIo.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, int Prescale>
class AvrClock
: private DebugObject<Context, AvrClock<Context, Prescale>>
{
    static_assert(Prescale >= 1, "Prescale must be >=1");
    static_assert(Prescale <= 5, "Prescale must be <=5");
    
public:
    typedef uint32_t TimeType;
    
public:
    static constexpr TimeType prescale_divide =
        (Prescale == 1) ? 1 :
        (Prescale == 2) ? 8 :
        (Prescale == 3) ? 64 :
        (Prescale == 4) ? 256 :
        (Prescale == 5) ? 1024 : 0;
    
    static constexpr double time_unit = (double)prescale_divide / F_CPU;
    static constexpr double time_freq = 1.0 / time_unit;
    
    void init (Context c)
    {
        m_offset = 0;
        TIMSK1 = 0;
        TCCR1A = 0;
        TCCR1B = (uint16_t)Prescale;
        TIMSK1 = (1 << TOIE1);
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        TIMSK1 = 0;
        TCCR1B = 0;
    }
    
    template <typename ThisContext>
    TimeType getTime (ThisContext c)
    {
        this->debugAccess(c);
        
        uint16_t now_high;
        uint16_t now_low;
        
        AMBRO_LOCK_T(AvrTempLock(), c, lock_c, {
            now_high = m_offset;
            uint8_t tmp;
            asm volatile (
                "    lds %A[now_low],%[tcnt1]+0\n"
                "    lds %B[now_low],%[tcnt1]+1\n"
                "    in %[tmp],%[tifr1]\n"
                "    andi %[tmp],1<<%[tov1]\n"
                "    breq no_overflow_%=\n"
                "    subi %A[now_high],-1\n"
                "    sbci %B[now_high],-1\n"
                "    lds %A[now_low],%[tcnt1]+0\n"
                "    lds %B[now_low],%[tcnt1]+1\n"
                "no_overflow_%=:\n"
            : [now_low] "=&r" (now_low),
            [now_high] "=&d" (now_high),
            [tmp] "=&d" (tmp)
            : "[now_high]" (now_high),
            [tcnt1] "n" (_SFR_MEM_ADDR(TCNT1)),
            [tifr1] "I" (_SFR_IO_ADDR(TIFR1)),
            [tov1] "n" (TOV1)
            );
        });
        
        return ((uint32_t)now_high << 16) | now_low;
    }
    
#ifdef TCNT3
    void initTC3 (Context c)
    {
        this->debugAccess(c);
        
        TIMSK3 = 0;
        TCCR3A = 0;
        TCCR3B = (uint16_t)Prescale;
        TCNT3 = TCNT1 - 1;
    }
    
    void deinitTC3 (Context c)
    {
        this->debugAccess(c);
        
        TIMSK3 = 0;
        TCCR3B = 0;
    }
#endif

#ifdef TCNT0
    static const int TC0Prescale = Prescale;
    
    void initTC0 (Context c)
    {
        this->debugAccess(c);
        
        TIMSK0 = 0;
        TCCR0A = 0;
        TCCR0B = TC0Prescale;
        TCNT0 = TCNT1 - 1;
    }
    
    void deinitTC0 (Context c)
    {
        this->debugAccess(c);
        
        TIMSK0 = 0;
        TCCR0B = 0;
    }
#endif
    
#ifdef TCNT2
    static const int TC2Prescale =
        (Prescale == 1) ? 1 :
        (Prescale == 2) ? 2 :
        (Prescale == 3) ? 4 :
        (Prescale == 4) ? 6 :
        (Prescale == 5) ? 7 : 0;
    
    void initTC2 (Context c)
    {
        this->debugAccess(c);
        
        TIMSK2 = 0;
        TCCR2A = 0;
        TCCR2B = TC2Prescale;
        TCNT2 = TCNT1 - 1;
    }
    
    void deinitTC2 (Context c)
    {
        this->debugAccess(c);
        
        TIMSK2 = 0;
        TCCR2B = 0;
    }
#endif
    
    void timer1_ovf_isr (AvrInterruptContext<Context> c)
    {
        m_offset++;
    }
    
public:
    volatile uint16_t m_offset;
};

template <typename Context, typename Handler, uint32_t timsk_reg, uint8_t ocie_bit, uint32_t ocr_reg, uint8_t ocf_bit>
class AvrClock16BitInterruptTimer
: private DebugObject<Context, void>
{
public:
    typedef typename Context::Clock Clock;
    typedef typename Clock::TimeType TimeType;
    typedef AvrInterruptContext<Context> HandlerContext;
    
    void init (Context c)
    {
        this->debugInit(c);
        
#ifdef AMBROLIB_ASSERTIONS
        m_running = false;
#endif
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        
        AMBRO_LOCK_T(AvrTempLock(), c, lock_c, {
            avrSoftClearBitReg<timsk_reg>(ocie_bit);
        });
    }
    
    template <typename ThisContext>
    void set (ThisContext c, TimeType time)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(!m_running)
        
        static const TimeType minus_clearance = -clearance;
        
        AMBRO_LOCK_T(AvrTempLock(), c, lock_c, {
            uint16_t now_high = lock_c.clock()->m_offset;
            uint16_t now_low;
            uint8_t tmp;
            
            asm volatile (
                "    lds %A[now_low],%[tcnt1]+0\n"
                "    lds %B[now_low],%[tcnt1]+1\n"
                "    in %[tmp],%[tifr1]\n"
                "    andi %[tmp],1<<%[tov1]\n"
                "    breq no_overflow_%=\n"
                "    subi %A[now_high],-1\n"
                "    sbci %B[now_high],-1\n"
                "    lds %A[now_low],%[tcnt1]+0\n"
                "    lds %B[now_low],%[tcnt1]+1\n"
                "no_overflow_%=:\n"
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
                "    adc %C[time],%A[now_high]\n"
                "    adc %D[time],%B[now_high]\n"
                "no_saturation_%=:\n"
                "    sts %[ocr]+1,%B[time]\n"
                "    sts %[ocr]+0,%A[time]\n"
                "    lds %[tmp],%[timsk]\n"
                "    ori %[tmp],1<<%[ocie_bit]\n"
                "    sts %[timsk],%[tmp]\n"
                : [now_low] "=&d" (now_low),
                  [now_high] "=&d" (now_high),
                  [tmp] "=&d" (tmp),
                  [time] "=&r" (time)
                : "[now_high]" (now_high),
                  "[time]" (time),
                  [mcA] "n" ((minus_clearance >> 0) & 0xFF),
                  [mcB] "n" ((minus_clearance >> 8) & 0xFF),
                  [mcC] "n" ((minus_clearance >> 16) & 0xFF),
                  [mcD] "n" ((minus_clearance >> 24) & 0xFF),
                  [tcnt1] "n" (_SFR_MEM_ADDR(TCNT1)),
                  [tifr1] "I" (_SFR_IO_ADDR(TIFR1)),
                  [tov1] "n" (TOV1),
                  [ocr] "n" (ocr_reg + __SFR_OFFSET),
                  [timsk] "n" (timsk_reg + __SFR_OFFSET),
                  [ocie_bit] "n" (ocie_bit)
            );
            
            m_time = time;
#ifdef AMBROLIB_ASSERTIONS
            m_running = true;
#endif
        });
    }
    
    template <typename ThisContext>
    void unset (ThisContext c)
    {
        this->debugAccess(c);
        
        AMBRO_LOCK_T(AvrTempLock(), c, lock_c, {
            avrSoftClearBitReg<timsk_reg>(ocie_bit);
#ifdef AMBROLIB_ASSERTIONS
            m_running = false;
#endif
        });
    }
    
    template <uint32_t check_ocr_reg>
    void timer_comp_isr (AvrInterruptContext<Context> c)
    {
        static_assert(check_ocr_reg == ocr_reg, "incorrect ISRS macro used");
        AMBRO_ASSERT(m_running)
        
        uint16_t now_low;
        uint16_t now_high = c.clock()->m_offset;
        uint8_t tmp;
        
        asm volatile (
                "    lds %A[now_low],%[tcnt1]+0\n"
                "    lds %B[now_low],%[tcnt1]+1\n"
                "    in %[tmp],%[tifr1]\n"
                "    andi %[tmp],1<<%[tov1]\n"
                "    breq no_overflow_%=\n"
                "    subi %A[now_high],-1\n"
                "    sbci %B[now_high],-1\n"
                "    lds %A[now_low],%[tcnt1]+0\n"
                "    lds %B[now_low],%[tcnt1]+1\n"
                "no_overflow_%=:\n"
                "    sub %A[now_low],%A[time]\n"
                "    sbc %B[now_low],%B[time]\n"
                "    sbc %A[now_high],%C[time]\n"
                "    sbc %B[now_high],%D[time]\n"
            : [now_low] "=&r" (now_low),
              [now_high] "=&d" (now_high),
              [tmp] "=&d" (tmp)
            : "[now_high]" (now_high),
              [time] "r" (m_time),
              [tcnt1] "n" (_SFR_MEM_ADDR(TCNT1)),
              [tifr1] "I" (_SFR_IO_ADDR(TIFR1)),
              [tov1] "n" (TOV1)
        );
        
        if (now_high < UINT16_C(0x8000)) {
#ifdef AMBROLIB_ASSERTIONS
            m_running = false;
#endif
            if (!Handler::call(this, c)) {
                avrSoftClearBitReg<timsk_reg>(ocie_bit);
            }
        }
    }
    
private:
    static const TimeType clearance = (35 / Clock::prescale_divide) + 2;
    
    TimeType m_time;
#ifdef AMBROLIB_ASSERTIONS
    bool m_running;
#endif
};

template <typename Context, typename Handler, uint32_t timsk_reg, uint8_t ocie_bit, uint32_t ocr_reg, uint8_t ocf_bit>
class AvrClock8BitInterruptTimer
: private DebugObject<Context, void>
{
public:
    typedef typename Context::Clock Clock;
    typedef typename Clock::TimeType TimeType;
    typedef AvrInterruptContext<Context> HandlerContext;
    
    void init (Context c)
    {
        this->debugInit(c);
        
#ifdef AMBROLIB_ASSERTIONS
        m_running = false;
#endif
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        
        AMBRO_LOCK_T(AvrTempLock(), c, lock_c, {
            avrSoftClearBitReg<timsk_reg>(ocie_bit);
        });
    }
    
    template <typename ThisContext>
    void set (ThisContext c, TimeType time)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(!m_running)
        
        static const TimeType minus_clearance = -clearance;
        
        AMBRO_LOCK_T(AvrTempLock(), c, lock_c, {
            uint16_t now_high = lock_c.clock()->m_offset;
            uint16_t now_low;
            uint8_t tmp;
            
            asm volatile (
                "    lds %A[now_low],%[tcnt1]+0\n"
                "    lds %B[now_low],%[tcnt1]+1\n"
                "    in %[tmp],%[tifr1]\n"
                "    andi %[tmp],1<<%[tov1]\n"
                "    breq no_overflow_%=\n"
                "    subi %A[now_high],-1\n"
                "    sbci %B[now_high],-1\n"
                "    lds %A[now_low],%[tcnt1]+0\n"
                "    lds %B[now_low],%[tcnt1]+1\n"
                "no_overflow_%=:\n"
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
                "    adc %C[time],%A[now_high]\n"
                "    adc %D[time],%B[now_high]\n"
                "no_saturation_%=:\n"
                "    sts %[ocr],%A[time]\n"
                "    lds %[tmp],%[timsk]\n"
                "    ori %[tmp],1<<%[ocie_bit]\n"
                "    sts %[timsk],%[tmp]\n"
                : [now_low] "=&d" (now_low),
                  [now_high] "=&d" (now_high),
                  [tmp] "=&d" (tmp),
                  [time] "=&r" (time)
                : "[now_high]" (now_high),
                  "[time]" (time),
                  [mcA] "n" ((minus_clearance >> 0) & 0xFF),
                  [mcB] "n" ((minus_clearance >> 8) & 0xFF),
                  [mcC] "n" ((minus_clearance >> 16) & 0xFF),
                  [mcD] "n" ((minus_clearance >> 24) & 0xFF),
                  [tcnt1] "n" (_SFR_MEM_ADDR(TCNT1)),
                  [tifr1] "I" (_SFR_IO_ADDR(TIFR1)),
                  [tov1] "n" (TOV1),
                  [ocr] "n" (ocr_reg + __SFR_OFFSET),
                  [timsk] "n" (timsk_reg + __SFR_OFFSET),
                  [ocie_bit] "n" (ocie_bit)
            );
            
            m_time = time;
#ifdef AMBROLIB_ASSERTIONS
            m_running = true;
#endif
        });
    }
    
    template <typename ThisContext>
    void unset (ThisContext c)
    {
        this->debugAccess(c);
        
        AMBRO_LOCK_T(AvrTempLock(), c, lock_c, {
            avrSoftClearBitReg<timsk_reg>(ocie_bit);
#ifdef AMBROLIB_ASSERTIONS
            m_running = false;
#endif
        });
    }
    
    template <uint32_t check_ocr_reg>
    void timer_comp_isr (AvrInterruptContext<Context> c)
    {
        static_assert(check_ocr_reg == ocr_reg, "incorrect ISRS macro used");
        AMBRO_ASSERT(m_running)
        
        uint16_t now_low;
        uint16_t now_high = c.clock()->m_offset;
        uint8_t tmp;
        
        asm volatile (
                "    lds %A[now_low],%[tcnt1]+0\n"
                "    lds %B[now_low],%[tcnt1]+1\n"
                "    in %[tmp],%[tifr1]\n"
                "    andi %[tmp],1<<%[tov1]\n"
                "    breq no_overflow_%=\n"
                "    subi %A[now_high],-1\n"
                "    sbci %B[now_high],-1\n"
                "    lds %A[now_low],%[tcnt1]+0\n"
                "    lds %B[now_low],%[tcnt1]+1\n"
                "no_overflow_%=:\n"
                "    sub %A[now_low],%A[time]\n"
                "    sbc %B[now_low],%B[time]\n"
                "    sbc %A[now_high],%C[time]\n"
                "    sbc %B[now_high],%D[time]\n"
            : [now_low] "=&r" (now_low),
              [now_high] "=&d" (now_high),
              [tmp] "=&d" (tmp)
            : "[now_high]" (now_high),
              [time] "r" (m_time),
              [tcnt1] "n" (_SFR_MEM_ADDR(TCNT1)),
              [tifr1] "I" (_SFR_IO_ADDR(TIFR1)),
              [tov1] "n" (TOV1)
        );
        
        if (now_high < UINT16_C(0x8000)) {
#ifdef AMBROLIB_ASSERTIONS
            m_running = false;
#endif
            if (!Handler::call(this, c)) {
                avrSoftClearBitReg<timsk_reg>(ocie_bit);
            }
        }
    }
    
private:
    static const TimeType clearance = (35 / Clock::prescale_divide) + 2;
    
    TimeType m_time;
#ifdef AMBROLIB_ASSERTIONS
    bool m_running;
#endif
};

template <typename Context, typename Handler>
using AvrClockInterruptTimer_TC1_OCA = AvrClock16BitInterruptTimer<Context, Handler, _SFR_IO_ADDR(TIMSK1), OCIE1A, _SFR_IO_ADDR(OCR1A), OCF1A>;

template <typename Context, typename Handler>
using AvrClockInterruptTimer_TC1_OCB = AvrClock16BitInterruptTimer<Context, Handler, _SFR_IO_ADDR(TIMSK1), OCIE1B, _SFR_IO_ADDR(OCR1B), OCF1B>;

#ifdef TCNT3
template <typename Context, typename Handler>
using AvrClockInterruptTimer_TC3_OCA = AvrClock16BitInterruptTimer<Context, Handler, _SFR_IO_ADDR(TIMSK3), OCIE3A, _SFR_IO_ADDR(OCR3A), OCF3A>;

template <typename Context, typename Handler>
using AvrClockInterruptTimer_TC3_OCB = AvrClock16BitInterruptTimer<Context, Handler, _SFR_IO_ADDR(TIMSK3), OCIE3B, _SFR_IO_ADDR(OCR3B), OCF3B>;
#endif

#ifdef TCNT0
template <typename Context, typename Handler>
using AvrClockInterruptTimer_TC0_OCA = AvrClock8BitInterruptTimer<Context, Handler, _SFR_IO_ADDR(TIMSK0), OCIE0A, _SFR_IO_ADDR(OCR0A), OCF0A>;

template <typename Context, typename Handler>
using AvrClockInterruptTimer_TC0_OCB = AvrClock8BitInterruptTimer<Context, Handler, _SFR_IO_ADDR(TIMSK0), OCIE0B, _SFR_IO_ADDR(OCR0B), OCF0B>;
#endif

#ifdef TCNT2
template <typename Context, typename Handler>
using AvrClockInterruptTimer_TC2_OCA = AvrClock8BitInterruptTimer<Context, Handler, _SFR_IO_ADDR(TIMSK2), OCIE2A, _SFR_IO_ADDR(OCR2A), OCF2A>;

template <typename Context, typename Handler>
using AvrClockInterruptTimer_TC2_OCB = AvrClock8BitInterruptTimer<Context, Handler, _SFR_IO_ADDR(TIMSK2), OCIE2B, _SFR_IO_ADDR(OCR2B), OCF2B>;
#endif

#define AMBRO_AVR_CLOCK_ISRS(avrclock, context) \
ISR(TIMER1_OVF_vect) \
{ \
    (avrclock).timer1_ovf_isr(MakeAvrInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC1_OCA_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER1_COMPA_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR1A)>(MakeAvrInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC1_OCB_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER1_COMPB_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR1B)>(MakeAvrInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC3_OCA_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER3_COMPA_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR3A)>(MakeAvrInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC3_OCB_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER3_COMPB_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR3B)>(MakeAvrInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC0_OCA_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER0_COMPA_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR0A)>(MakeAvrInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC0_OCB_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER0_COMPB_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR0B)>(MakeAvrInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC2_OCA_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER2_COMPA_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR2A)>(MakeAvrInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC2_OCB_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER2_COMPB_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR2B)>(MakeAvrInterruptContext((context))); \
}

#include <aprinter/EndNamespace.h>

#endif
