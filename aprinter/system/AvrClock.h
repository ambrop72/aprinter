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

#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
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
    
    void timer1_ovf_isr (AvrInterruptContext<Context> c)
    {
        m_offset++;
    }
    
private:
    volatile uint16_t m_offset;
};

template <typename Context, typename Handler, uint32_t timsk_reg, uint8_t ocie_bit, uint32_t ocr_reg>
class AvrClockInterruptTimer
: private DebugObject<Context, AvrClockInterruptTimer<Context, Handler, timsk_reg, ocie_bit, ocr_reg>>
{
public:
    typedef typename Context::Clock Clock;
    typedef typename Clock::TimeType TimeType;
    typedef AvrInterruptContext<Context> HandlerContext;
    
    void init (Context c)
    {
        this->debugInit(c);
        
        m_lock.init(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        
        avrSoftClearBitReg<timsk_reg>(ocie_bit);
        m_lock.deinit(c);
    }
    
    template <typename ThisContext>
    void set (ThisContext c, TimeType time)
    {
        this->debugAccess(c);
        
        TimeType now = c.clock()->getTime(c);
        TimeType ref = now - Clock::past;
        
        if ((TimeType)(time - ref) < (TimeType)((TimeType)(now + clearance) - ref)) {
            time = now + clearance;
        }
        
        AMBRO_LOCK_T(m_lock, c, lock_c, {
            m_time = time;
#ifdef AMBROLIB_ASSERTIONS
            m_running = true;
#endif
            avrSetReg16<ocr_reg>(time);
            avrSoftSetBitReg<timsk_reg>(ocie_bit);
        });
    }
    
    template <typename ThisContext>
    void unset (ThisContext c)
    {
        this->debugAccess(c);
        
        AMBRO_LOCK_T(m_lock, c, lock_c, {
#ifdef AMBROLIB_ASSERTIONS
            m_running = false;
#endif
            avrSoftClearBitReg<timsk_reg>(ocie_bit);
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
        
#ifdef AMBROLIB_ASSERTIONS
        m_running = false;
#endif
        avrSoftClearBitReg<timsk_reg>(ocie_bit);
        
        return Handler::call(this, c);
    }
    
private:
    static const TimeType clearance = (64 / Clock::prescale_divide) + 2;
    
#ifdef AMBROLIB_ASSERTIONS
    bool m_running;
#endif
    TimeType m_time;
    AvrLock<Context> m_lock;
};

template <typename Context, typename Handler>
using AvrClockInterruptTimer_TC1_OCA = AvrClockInterruptTimer<Context, Handler, _SFR_IO_ADDR(TIMSK1), OCIE1A, _SFR_IO_ADDR(OCR1A)>;

template <typename Context, typename Handler>
using AvrClockInterruptTimer_TC1_OCB = AvrClockInterruptTimer<Context, Handler, _SFR_IO_ADDR(TIMSK1), OCIE1B, _SFR_IO_ADDR(OCR1B)>;

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

#include <aprinter/EndNamespace.h>

#endif
