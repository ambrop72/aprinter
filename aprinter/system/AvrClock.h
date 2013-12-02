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

#include <aprinter/meta/Position.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Likely.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/system/AvrIo.h>

#include <aprinter/BeginNamespace.h>

template <typename Position, typename Context, int Prescale>
class AvrClock
: private DebugObject<Context, void>
{
    static_assert(Prescale >= 1, "Prescale must be >=1");
    static_assert(Prescale <= 5, "Prescale must be <=5");
    
    static AvrClock * self (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
    }
    
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
    
    static void init (Context c)
    {
        AvrClock *o = self(c);
        o->m_offset = 0;
        TIMSK1 = 0;
        TCCR1A = 0;
        TCCR1B = (uint16_t)Prescale;
        TIMSK1 = (1 << TOIE1);
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        AvrClock *o = self(c);
        o->debugDeinit(c);
        TIMSK1 = 0;
        TCCR1B = 0;
    }
    
    template <typename ThisContext>
    static TimeType getTime (ThisContext c)
    {
        AvrClock *o = self(c);
        o->debugAccess(c);
        
        uint16_t now_high;
        uint16_t now_low;
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            now_high = o->m_offset;
            asm volatile (
                "    lds %A[now_low],%[tcnt1]+0\n"
                "    sbis %[tifr1],%[tov1]\n"
                "    rjmp no_overflow_%=\n"
                "    lds %A[now_low],%[tcnt1]+0\n"
                "    subi %A[now_high],-1\n"
                "    sbci %B[now_high],-1\n"
                "no_overflow_%=:\n"
                "    lds %B[now_low],%[tcnt1]+1\n"
            : [now_low] "=&r" (now_low),
            [now_high] "=&d" (now_high)
            : "[now_high]" (now_high),
            [tcnt1] "n" (_SFR_MEM_ADDR(TCNT1)),
            [tifr1] "I" (_SFR_IO_ADDR(TIFR1)),
            [tov1] "n" (TOV1)
            );
        }
        
        return ((uint32_t)now_high << 16) | now_low;
    }
    
#ifdef TCNT3
    static void initTC3 (Context c)
    {
        AvrClock *o = self(c);
        o->debugAccess(c);
        
        TIMSK3 = 0;
        TCCR3A = 0;
        TCCR3B = (uint16_t)Prescale;
        TCNT3 = TCNT1 - 1;
    }
    
    static void deinitTC3 (Context c)
    {
        AvrClock *o = self(c);
        o->debugAccess(c);
        
        TIMSK3 = 0;
        TCCR3B = 0;
    }
#endif

#ifdef TCNT4
    static void initTC4 (Context c)
    {
        AvrClock *o = self(c);
        o->debugAccess(c);
        
        TIMSK4 = 0;
        TCCR4A = 0;
        TCCR4B = (uint16_t)Prescale;
        TCNT4 = TCNT1 - 1;
    }
    
    static void deinitTC4 (Context c)
    {
        AvrClock *o = self(c);
        o->debugAccess(c);
        
        TIMSK4 = 0;
        TCCR4B = 0;
    }
#endif

#ifdef TCNT5
    static void initTC5 (Context c)
    {
        AvrClock *o = self(c);
        o->debugAccess(c);
        
        TIMSK5 = 0;
        TCCR5A = 0;
        TCCR5B = (uint16_t)Prescale;
        TCNT5 = TCNT1 - 1;
    }
    
    static void deinitTC5 (Context c)
    {
        AvrClock *o = self(c);
        o->debugAccess(c);
        
        TIMSK5 = 0;
        TCCR5B = 0;
    }
#endif

#ifdef TCNT0
    static const int TC0Prescale = Prescale;
    
    static void initTC0 (Context c)
    {
        AvrClock *o = self(c);
        o->debugAccess(c);
        
        TIMSK0 = 0;
        TCCR0A = 0;
        TCCR0B = TC0Prescale;
        TCNT0 = TCNT1 - 1;
    }
    
    static void deinitTC0 (Context c)
    {
        AvrClock *o = self(c);
        o->debugAccess(c);
        
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
    
    static void initTC2 (Context c)
    {
        AvrClock *o = self(c);
        o->debugAccess(c);
        
        TIMSK2 = 0;
        TCCR2A = 0;
        TCCR2B = TC2Prescale;
        TCNT2 = TCNT1 - 1;
    }
    
    static void deinitTC2 (Context c)
    {
        AvrClock *o = self(c);
        o->debugAccess(c);
        
        TIMSK2 = 0;
        TCCR2B = 0;
    }
#endif
    
    static void timer1_ovf_isr (InterruptContext<Context> c)
    {
        AvrClock *o = self(c);
        o->m_offset++;
    }
    
public:
    volatile uint16_t m_offset;
};

template <typename Position, typename Context, typename Handler, uint32_t timsk_reg, uint8_t ocie_bit, uint32_t ocr_reg, uint8_t ocf_bit>
class AvrClock16BitInterruptTimer
: private DebugObject<Context, void>
{
    static AvrClock16BitInterruptTimer * self (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
    }
    
public:
    typedef typename Context::Clock Clock;
    typedef typename Clock::TimeType TimeType;
    typedef InterruptContext<Context> HandlerContext;
    
    static void init (Context c)
    {
        AvrClock16BitInterruptTimer *o = self(c);
        o->debugInit(c);
        
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = false;
#endif
    }
    
    static void deinit (Context c)
    {
        AvrClock16BitInterruptTimer *o = self(c);
        o->debugDeinit(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            avrSoftClearBitReg<timsk_reg>(ocie_bit);
        }
    }
    
    template <typename ThisContext>
    static void setFirst (ThisContext c, TimeType time)
    {
        AvrClock16BitInterruptTimer *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(!o->m_running)
        AMBRO_ASSERT(!(avrGetBitReg<timsk_reg, ocie_bit>()))
        
        o->m_time = time;
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = true;
#endif
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            uint16_t now_high = lock_c.clock()->m_offset;
            uint16_t now_low;
            
            asm volatile (
                "    lds %A[now_low],%[tcnt1]+0\n"
                "    sbis %[tifr1],%[tov1]\n"
                "    rjmp no_overflow_%=\n"
                "    lds %A[now_low],%[tcnt1]+0\n"
                "    subi %A[now_high],-1\n"
                "    sbci %B[now_high],-1\n"
                "no_overflow_%=:\n"
                "    lds %B[now_low],%[tcnt1]+1\n"
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
                "    ori %[now_low],1<<%[ocie_bit]\n"
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
                  [tcnt1] "n" (_SFR_MEM_ADDR(TCNT1)),
                  [tifr1] "I" (_SFR_IO_ADDR(TIFR1)),
                  [tov1] "n" (TOV1),
                  [ocr] "n" (ocr_reg + __SFR_OFFSET),
                  [timsk] "n" (timsk_reg + __SFR_OFFSET),
                  [ocie_bit] "n" (ocie_bit)
            );
        }
    }
    
    static void setNext (HandlerContext c, TimeType time)
    {
        AvrClock16BitInterruptTimer *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_running)
        AMBRO_ASSERT((avrGetBitReg<timsk_reg, ocie_bit>()))
        
        o->m_time = time;
        
        uint16_t now_high = c.clock()->m_offset;
        uint16_t now_low;
        
        asm volatile (
            "    lds %A[now_low],%[tcnt1]+0\n"
            "    sbis %[tifr1],%[tov1]\n"
            "    rjmp no_overflow_%=\n"
            "    lds %A[now_low],%[tcnt1]+0\n"
            "    subi %A[now_high],-1\n"
            "    sbci %B[now_high],-1\n"
            "no_overflow_%=:\n"
            "    lds %B[now_low],%[tcnt1]+1\n"
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
              [tcnt1] "n" (_SFR_MEM_ADDR(TCNT1)),
              [tifr1] "I" (_SFR_IO_ADDR(TIFR1)),
              [tov1] "n" (TOV1),
              [ocr] "n" (ocr_reg + __SFR_OFFSET)
        );
    }
    
    template <typename ThisContext>
    static void unset (ThisContext c)
    {
        AvrClock16BitInterruptTimer *o = self(c);
        o->debugAccess(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            avrSoftClearBitReg<timsk_reg>(ocie_bit);
#ifdef AMBROLIB_ASSERTIONS
            o->m_running = false;
#endif
        }
    }
    
    template <uint32_t check_ocr_reg>
    static void timer_comp_isr (InterruptContext<Context> c)
    {
        static_assert(check_ocr_reg == ocr_reg, "incorrect ISRS macro used");
        AvrClock16BitInterruptTimer *o = self(c);
        AMBRO_ASSERT(o->m_running)
        AMBRO_ASSERT((avrGetBitReg<timsk_reg, ocie_bit>()))
        
        uint16_t now_low;
        uint16_t now_high = c.clock()->m_offset;
        
        asm volatile (
            "    lds %A[now_low],%[tcnt1]+0\n"
            "    sbis %[tifr1],%[tov1]\n"
            "    rjmp no_overflow_%=\n"
            "    lds %A[now_low],%[tcnt1]+0\n"
            "    subi %A[now_high],-1\n"
            "    sbci %B[now_high],-1\n"
            "no_overflow_%=:\n"
            "    lds %B[now_low],%[tcnt1]+1\n"
            "    sub %A[now_low],%A[time]\n"
            "    sbc %B[now_low],%B[time]\n"
            "    sbc %A[now_high],%C[time]\n"
            "    sbc %B[now_high],%D[time]\n"
            : [now_low] "=&r" (now_low),
              [now_high] "=&d" (now_high)
            : "[now_high]" (now_high),
              [time] "r" (o->m_time),
              [tcnt1] "n" (_SFR_MEM_ADDR(TCNT1)),
              [tifr1] "I" (_SFR_IO_ADDR(TIFR1)),
              [tov1] "n" (TOV1)
        );
        
        if (now_high < UINT16_C(0x8000)) {
            if (!Handler::call(o, c)) {
#ifdef AMBROLIB_ASSERTIONS
                o->m_running = false;
#endif
                avrSoftClearBitReg<timsk_reg>(ocie_bit);
            }
        }
    }
    
private:
    static const TimeType clearance = (35 / Clock::prescale_divide) + 2;
    static const TimeType minus_clearance = -clearance;
    
    TimeType m_time;
#ifdef AMBROLIB_ASSERTIONS
    bool m_running;
#endif
};

template <typename Position, typename Context, typename Handler, uint32_t timsk_reg, uint8_t ocie_bit, uint32_t ocr_reg, uint8_t ocf_bit>
class AvrClock8BitInterruptTimer
: private DebugObject<Context, void>
{
    static AvrClock8BitInterruptTimer * self (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
    }
    
public:
    typedef typename Context::Clock Clock;
    typedef typename Clock::TimeType TimeType;
    typedef InterruptContext<Context> HandlerContext;
    
    static void init (Context c)
    {
        AvrClock8BitInterruptTimer *o = self(c);
        o->debugInit(c);
        
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = false;
#endif
    }
    
    static void deinit (Context c)
    {
        AvrClock8BitInterruptTimer *o = self(c);
        o->debugDeinit(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            avrSoftClearBitReg<timsk_reg>(ocie_bit);
        }
    }
    
    template <typename ThisContext>
    static void setFirst (ThisContext c, TimeType time)
    {
        AvrClock8BitInterruptTimer *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(!o->m_running)
        AMBRO_ASSERT(!(avrGetBitReg<timsk_reg, ocie_bit>()))
        
        o->m_time = time;
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = true;
#endif
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            uint16_t now_high = lock_c.clock()->m_offset;
            uint16_t now_low;
            
            asm volatile (
                "    lds %A[now_low],%[tcnt1]+0\n"
                "    sbis %[tifr1],%[tov1]\n"
                "    rjmp no_overflow_%=\n"
                "    lds %A[now_low],%[tcnt1]+0\n"
                "    subi %A[now_high],-1\n"
                "    sbci %B[now_high],-1\n"
                "no_overflow_%=:\n"
                "    lds %B[now_low],%[tcnt1]+1\n"
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
                "    ori %[now_low],1<<%[ocie_bit]\n"
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
                  [tcnt1] "n" (_SFR_MEM_ADDR(TCNT1)),
                  [tifr1] "I" (_SFR_IO_ADDR(TIFR1)),
                  [tov1] "n" (TOV1),
                  [ocr] "n" (ocr_reg + __SFR_OFFSET),
                  [timsk] "n" (timsk_reg + __SFR_OFFSET),
                  [ocie_bit] "n" (ocie_bit)
            );
        }
    }
    
    static void setNext (HandlerContext c, TimeType time)
    {
        AvrClock8BitInterruptTimer *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_running)
        AMBRO_ASSERT((avrGetBitReg<timsk_reg, ocie_bit>()))
        
        o->m_time = time;
    
        uint16_t now_high = c.clock()->m_offset;
        uint16_t now_low;
        
        asm volatile (
            "    lds %A[now_low],%[tcnt1]+0\n"
            "    sbis %[tifr1],%[tov1]\n"
            "    rjmp no_overflow_%=\n"
            "    lds %A[now_low],%[tcnt1]+0\n"
            "    subi %A[now_high],-1\n"
            "    sbci %B[now_high],-1\n"
            "no_overflow_%=:\n"
            "    lds %B[now_low],%[tcnt1]+1\n"
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
              [tcnt1] "n" (_SFR_MEM_ADDR(TCNT1)),
              [tifr1] "I" (_SFR_IO_ADDR(TIFR1)),
              [tov1] "n" (TOV1),
              [ocr] "n" (ocr_reg + __SFR_OFFSET)
        );
    }
    
    template <typename ThisContext>
    static void unset (ThisContext c)
    {
        AvrClock8BitInterruptTimer *o = self(c);
        o->debugAccess(c);
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            avrSoftClearBitReg<timsk_reg>(ocie_bit);
#ifdef AMBROLIB_ASSERTIONS
            o->m_running = false;
#endif
        }
    }
    
    template <uint32_t check_ocr_reg>
    static void timer_comp_isr (InterruptContext<Context> c)
    {
        static_assert(check_ocr_reg == ocr_reg, "incorrect ISRS macro used");
        AvrClock8BitInterruptTimer *o = self(c);
        AMBRO_ASSERT(o->m_running)
        AMBRO_ASSERT((avrGetBitReg<timsk_reg, ocie_bit>()))
        
        uint16_t now_low;
        uint16_t now_high = c.clock()->m_offset;
        
        asm volatile (
            "    lds %A[now_low],%[tcnt1]+0\n"
            "    sbis %[tifr1],%[tov1]\n"
            "    rjmp no_overflow_%=\n"
            "    lds %A[now_low],%[tcnt1]+0\n"
            "    subi %A[now_high],-1\n"
            "    sbci %B[now_high],-1\n"
            "no_overflow_%=:\n"
            "    lds %B[now_low],%[tcnt1]+1\n"
            "    sub %A[now_low],%A[time]\n"
            "    sbc %B[now_low],%B[time]\n"
            "    sbc %A[now_high],%C[time]\n"
            "    sbc %B[now_high],%D[time]\n"
            : [now_low] "=&r" (now_low),
              [now_high] "=&d" (now_high)
            : "[now_high]" (now_high),
              [time] "r" (o->m_time),
              [tcnt1] "n" (_SFR_MEM_ADDR(TCNT1)),
              [tifr1] "I" (_SFR_IO_ADDR(TIFR1)),
              [tov1] "n" (TOV1)
        );
        
        if (now_high < UINT16_C(0x8000)) {
            if (!Handler::call(o, c)) {
#ifdef AMBROLIB_ASSERTIONS
                o->m_running = false;
#endif
                avrSoftClearBitReg<timsk_reg>(ocie_bit);
            }
        }
    }
    
private:
    static const TimeType clearance = (35 / Clock::prescale_divide) + 2;
    static const TimeType minus_clearance = -clearance;
    
    TimeType m_time;
#ifdef AMBROLIB_ASSERTIONS
    bool m_running;
#endif
};

template <typename Position, typename Context, typename Handler>
using AvrClockInterruptTimer_TC1_OCA = AvrClock16BitInterruptTimer<Position, Context, Handler, _SFR_IO_ADDR(TIMSK1), OCIE1A, _SFR_IO_ADDR(OCR1A), OCF1A>;

template <typename Position, typename Context, typename Handler>
using AvrClockInterruptTimer_TC1_OCB = AvrClock16BitInterruptTimer<Position, Context, Handler, _SFR_IO_ADDR(TIMSK1), OCIE1B, _SFR_IO_ADDR(OCR1B), OCF1B>;
#ifdef OCR1C
template <typename Position, typename Context, typename Handler>
using AvrClockInterruptTimer_TC1_OCC = AvrClock16BitInterruptTimer<Position, Context, Handler, _SFR_IO_ADDR(TIMSK1), OCIE1C, _SFR_IO_ADDR(OCR1C), OCF1C>;
#endif

#ifdef TCNT3
template <typename Position, typename Context, typename Handler>
using AvrClockInterruptTimer_TC3_OCA = AvrClock16BitInterruptTimer<Position, Context, Handler, _SFR_IO_ADDR(TIMSK3), OCIE3A, _SFR_IO_ADDR(OCR3A), OCF3A>;

template <typename Position, typename Context, typename Handler>
using AvrClockInterruptTimer_TC3_OCB = AvrClock16BitInterruptTimer<Position, Context, Handler, _SFR_IO_ADDR(TIMSK3), OCIE3B, _SFR_IO_ADDR(OCR3B), OCF3B>;
#ifdef OCR3C
template <typename Position, typename Context, typename Handler>
using AvrClockInterruptTimer_TC3_OCC = AvrClock16BitInterruptTimer<Position, Context, Handler, _SFR_IO_ADDR(TIMSK3), OCIE3C, _SFR_IO_ADDR(OCR3C), OCF3C>;
#endif
#endif

#ifdef TCNT4
template <typename Position, typename Context, typename Handler>
using AvrClockInterruptTimer_TC4_OCA = AvrClock16BitInterruptTimer<Position, Context, Handler, _SFR_IO_ADDR(TIMSK4), OCIE4A, _SFR_IO_ADDR(OCR4A), OCF4A>;

template <typename Position, typename Context, typename Handler>
using AvrClockInterruptTimer_TC4_OCB = AvrClock16BitInterruptTimer<Position, Context, Handler, _SFR_IO_ADDR(TIMSK4), OCIE4B, _SFR_IO_ADDR(OCR4B), OCF4B>;
#ifdef OCR4C
template <typename Position, typename Context, typename Handler>
using AvrClockInterruptTimer_TC4_OCC = AvrClock16BitInterruptTimer<Position, Context, Handler, _SFR_IO_ADDR(TIMSK4), OCIE4C, _SFR_IO_ADDR(OCR4C), OCF4C>;
#endif
#endif

#ifdef TCNT5
template <typename Position, typename Context, typename Handler>
using AvrClockInterruptTimer_TC5_OCA = AvrClock16BitInterruptTimer<Position, Context, Handler, _SFR_IO_ADDR(TIMSK5), OCIE5A, _SFR_IO_ADDR(OCR5A), OCF5A>;

template <typename Position, typename Context, typename Handler>
using AvrClockInterruptTimer_TC5_OCB = AvrClock16BitInterruptTimer<Position, Context, Handler, _SFR_IO_ADDR(TIMSK5), OCIE5B, _SFR_IO_ADDR(OCR5B), OCF5B>;
#ifdef OCR5C
template <typename Position, typename Context, typename Handler>
using AvrClockInterruptTimer_TC5_OCC = AvrClock16BitInterruptTimer<Position, Context, Handler, _SFR_IO_ADDR(TIMSK5), OCIE5C, _SFR_IO_ADDR(OCR5C), OCF5C>;
#endif
#endif

#ifdef TCNT0
template <typename Position, typename Context, typename Handler>
using AvrClockInterruptTimer_TC0_OCA = AvrClock8BitInterruptTimer<Position, Context, Handler, _SFR_IO_ADDR(TIMSK0), OCIE0A, _SFR_IO_ADDR(OCR0A), OCF0A>;

template <typename Position, typename Context, typename Handler>
using AvrClockInterruptTimer_TC0_OCB = AvrClock8BitInterruptTimer<Position, Context, Handler, _SFR_IO_ADDR(TIMSK0), OCIE0B, _SFR_IO_ADDR(OCR0B), OCF0B>;
#endif

#ifdef TCNT2
template <typename Position, typename Context, typename Handler>
using AvrClockInterruptTimer_TC2_OCA = AvrClock8BitInterruptTimer<Position, Context, Handler, _SFR_IO_ADDR(TIMSK2), OCIE2A, _SFR_IO_ADDR(OCR2A), OCF2A>;

template <typename Position, typename Context, typename Handler>
using AvrClockInterruptTimer_TC2_OCB = AvrClock8BitInterruptTimer<Position, Context, Handler, _SFR_IO_ADDR(TIMSK2), OCIE2B, _SFR_IO_ADDR(OCR2B), OCF2B>;
#endif

#define AMBRO_AVR_CLOCK_ISRS(avrclock, context) \
ISR(TIMER1_OVF_vect) \
{ \
    (avrclock).timer1_ovf_isr(MakeInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC1_OCA_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER1_COMPA_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR1A)>(MakeInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC1_OCB_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER1_COMPB_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR1B)>(MakeInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC1_OCC_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER1_COMPC_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR1C)>(MakeInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC3_OCA_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER3_COMPA_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR3A)>(MakeInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC3_OCB_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER3_COMPB_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR3B)>(MakeInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC3_OCC_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER3_COMPC_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR3C)>(MakeInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC4_OCA_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER4_COMPA_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR4A)>(MakeInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC4_OCB_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER4_COMPB_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR4B)>(MakeInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC4_OCC_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER4_COMPC_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR4C)>(MakeInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC5_OCA_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER5_COMPA_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR5A)>(MakeInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC5_OCB_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER5_COMPB_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR5B)>(MakeInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC5_OCC_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER5_COMPC_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR5C)>(MakeInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC0_OCA_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER0_COMPA_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR0A)>(MakeInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC0_OCB_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER0_COMPB_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR0B)>(MakeInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC2_OCA_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER2_COMPA_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR2A)>(MakeInterruptContext((context))); \
}

#define AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC2_OCB_ISRS(avrclockinterrupttimer, context) \
ISR(TIMER2_COMPB_vect) \
{ \
    (avrclockinterrupttimer).timer_comp_isr<_SFR_IO_ADDR(OCR2B)>(MakeInterruptContext((context))); \
}

#include <aprinter/EndNamespace.h>

#endif
