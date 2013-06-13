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
    static constexpr TimeType past = UINT32_C(0x20000000);
    
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
        
        if (IsAvrInterruptContext<ThisContext>::value) {
            uint16_t offset = m_offset;
            uint16_t tcnt = TCNT1;
            __sync_synchronize();
            if ((TIFR1 & (1 << TOV1))) {
                return (((TimeType)(m_offset + 1) << 16) + TCNT1);
            } else {
                return (((TimeType)offset << 16) + tcnt);
            }
        } else {
            while (1) {
                uint16_t offset1 = m_offset;
                __sync_synchronize();
                uint16_t tcnt = TCNT1;
                __sync_synchronize();
                uint16_t offset2 = m_offset;
                if (offset1 == offset2) {
                    return (((TimeType)offset1 << 16) + tcnt);
                }
            }
        }
    }
    
    template <typename ThisContext>
    void handleOverflow (ThisContext c)
    {
        if (IsAvrInterruptContext<ThisContext>::value) {
            if ((TIFR1 & (1 << TOV1))) {
                m_offset++;
                TIFR1 |= (1 << TOV1);
            }
        }
    }
    
    void initTC3 (Context c)
    {
        this->debugAccess(c);
        
        TIMSK3 = 0;
        TCCR3A = 0;
        TCCR3B = (uint16_t)Prescale;
        TCNT3 = TCNT1 + sync_clearance; // TODO
    }
    
    void deinitTC3 (Context c)
    {
        this->debugAccess(c);
        
        TIMSK3 = 0;
        TCCR3B = 0;
    }
    
    void timer1_ovf_isr (AvrInterruptContext<Context> c)
    {
        m_offset++;
    }
    
private:
    static const TimeType sync_clearance = 16 / prescale_divide;
    
public:
    volatile uint16_t m_offset;
};

template <typename Context, typename Handler, uint32_t timsk_reg, uint8_t ocie_bit, uint32_t ocr_reg, uint32_t tifr_reg, uint8_t ocf_bit>
class AvrClockInterruptTimer
: private DebugObject<Context, AvrClockInterruptTimer<Context, Handler, timsk_reg, ocie_bit, ocr_reg, tifr_reg, ocf_bit>>
{
public:
    typedef typename Context::Clock Clock;
    typedef typename Clock::TimeType TimeType;
    typedef AvrInterruptContext<Context> HandlerContext;
    
    void init (Context c)
    {
        this->debugInit(c);
        
        m_lock.init(c);
#ifdef AMBROLIB_ASSERTIONS
        m_running = false;
#endif
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        
        avrSoftClearBitReg<timsk_reg>(ocie_bit);
        avrSetBitReg<tifr_reg>(ocf_bit);
        m_lock.deinit(c);
    }
    
    template <typename ThisContext>
    void set (ThisContext c, TimeType time)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(!m_running)
        
#if 1
        TimeType time_plus_past = time + (TimeType)Clock::past;
        TimeType clearance_plus_past = (TimeType)clearance + Clock::past;
        
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            uint16_t now_high = lock_c.clock()->m_offset;
            uint16_t now_low;
            uint8_t tmp;
            
            asm volatile (
                "    lds %A[now_low],%[tcnt]+0\n"
                "    lds %B[now_low],%[tcnt]+1\n"
                "    in %[tmp],%[tifr1]\n"
                "    andi %[tmp],1<<%[tov]\n"
                "    breq no_overflow_%=\n"
                "    subi %A[now_high],-1\n"
                "    sbci %B[now_high],-1\n"
                "    lds %A[now_low],%[tcnt]+0\n"
                "    lds %B[now_low],%[tcnt]+1\n"
                "no_overflow_%=:\n"
                "    sub %A[time_plus_past],%A[now_low]\n"
                "    sbc %B[time_plus_past],%B[now_low]\n"
                "    sbc %C[time_plus_past],%A[now_high]\n"
                "    sbc %D[time_plus_past],%B[now_high]\n"
                "    sub %A[time_plus_past],%A[clearance_plus_past]\n"
                "    sbc %B[time_plus_past],%B[clearance_plus_past]\n"
                "    sbc %C[time_plus_past],%C[clearance_plus_past]\n"
                "    sbc %D[time_plus_past],%D[clearance_plus_past]\n"
                "    brcc no_saturation_%=\n"
                "    sub %A[time],%A[time_plus_past]\n"
                "    sbc %B[time],%B[time_plus_past]\n"
                "    sbc %C[time],%C[time_plus_past]\n"
                "    sbc %D[time],%D[time_plus_past]\n"
                "no_saturation_%=:\n"
                "    sts %[ocr]+1,%B[time]\n"
                "    sts %[ocr]+0,%A[time]\n"
                "    lds %[tmp],%[timsk]\n"
                "    ori %[tmp],1<<%[ocie_bit]\n"
                "    sts %[timsk],%[tmp]\n"
                
                : [now_low] "=&r" (now_low),
                  [now_high] "=&a" (now_high),
                  [tmp] "=&a" (tmp),
                  [time_plus_past] "=&r" (time_plus_past),
                  [time] "=&r" (time)
                : "[now_high]" (now_high),
                  "[time_plus_past]" (time_plus_past),
                  "[time]" (time),
                  [clearance_plus_past] "r" (clearance_plus_past),
                  [tcnt] "n" (_SFR_MEM_ADDR(TCNT1)),
                  [tifr1] "I" (_SFR_IO_ADDR(TIFR1)),
                  [tov] "n" (TOV1),
                  [ocr] "n" (ocr_reg + __SFR_OFFSET),
                  [timsk] "n" (timsk_reg + __SFR_OFFSET),
                  [ocie_bit] "n" (ocie_bit),
                  [tifr] "I" (tifr_reg),
                  [ocf_bit] "n" (ocf_bit)
            );
            
            m_time = time;
#ifdef AMBROLIB_ASSERTIONS
            m_running = true;
#endif
        });
#endif
    }
    
    template <typename ThisContext>
    void unset (ThisContext c)
    {
        this->debugAccess(c);
        
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            avrSoftClearBitReg<timsk_reg>(ocie_bit);
            avrSetBitReg<tifr_reg>(ocf_bit);
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
        
        TimeType now = c.clock()->getTime(c);
        TimeType ref = now - Clock::past;
        
        if ((TimeType)Clock::past < (TimeType)(m_time - ref)) {
            return;
        }
        
        avrSoftClearBitReg<timsk_reg>(ocie_bit);
#ifdef AMBROLIB_ASSERTIONS
        m_running = false;
#endif
        
        return Handler::call(this, c);
    }
    
private:
    static const TimeType clearance = (35 / Clock::prescale_divide) + 2;
    
    TimeType m_time;
    AvrLock<Context> m_lock;
#ifdef AMBROLIB_ASSERTIONS
    bool m_running;
#endif
};

template <typename Context, typename Handler>
using AvrClockInterruptTimer_TC1_OCA = AvrClockInterruptTimer<Context, Handler, _SFR_IO_ADDR(TIMSK1), OCIE1A, _SFR_IO_ADDR(OCR1A), _SFR_IO_ADDR(TIFR1), OCF1A>;

template <typename Context, typename Handler>
using AvrClockInterruptTimer_TC1_OCB = AvrClockInterruptTimer<Context, Handler, _SFR_IO_ADDR(TIMSK1), OCIE1B, _SFR_IO_ADDR(OCR1B), _SFR_IO_ADDR(TIFR1), OCF1B>;

template <typename Context, typename Handler>
using AvrClockInterruptTimer_TC3_OCA = AvrClockInterruptTimer<Context, Handler, _SFR_IO_ADDR(TIMSK3), OCIE3A, _SFR_IO_ADDR(OCR3A), _SFR_IO_ADDR(TIFR3), OCF3A>;

template <typename Context, typename Handler>
using AvrClockInterruptTimer_TC3_OCB = AvrClockInterruptTimer<Context, Handler, _SFR_IO_ADDR(TIMSK3), OCIE3B, _SFR_IO_ADDR(OCR3B), _SFR_IO_ADDR(TIFR3), OCF3B>;

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

#include <aprinter/EndNamespace.h>

#endif
