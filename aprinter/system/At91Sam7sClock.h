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

#ifndef AMBROLIB_AT91SAM7S_CLOCK_H
#define AMBROLIB_AT91SAM7S_CLOCK_H

#include <stdint.h>
#include <stddef.h>

#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/base/Likely.h>
#include <aprinter/system/InterruptLock.h>

void At91Sam7sClock_TC0_Irq (void);
void At91Sam7sClock_TC1_Irq (void);
void At91Sam7sClock_TC2_Irq (void);
void At91Sam7SClock_IrqHandlerTC0A (uint32_t status);
void At91Sam7SClock_IrqHandlerTC0B (uint32_t status);
void At91Sam7SClock_IrqHandlerTC0C (uint32_t status);
void At91Sam7SClock_IrqHandlerTC1A (uint32_t status);
void At91Sam7SClock_IrqHandlerTC1B (uint32_t status);
void At91Sam7SClock_IrqHandlerTC1C (uint32_t status);
void At91Sam7SClock_IrqHandlerTC2A (uint32_t status);
void At91Sam7SClock_IrqHandlerTC2B (uint32_t status);
void At91Sam7SClock_IrqHandlerTC2C (uint32_t status);

#include <aprinter/BeginNamespace.h>

template <typename Context, int Prescale>
class At91Sam7sClock
: private DebugObject<Context, void>
{
    static_assert(Prescale >= 1, "Prescale must be >=1");
    static_assert(Prescale <= 5, "Prescale must be <=5");
    
public:
    typedef uint32_t TimeType;
    
public:
    static constexpr TimeType prescale_divide =
        (Prescale == 1) ? 2 :
        (Prescale == 2) ? 8 :
        (Prescale == 3) ? 32 :
        (Prescale == 4) ? 128 :
        (Prescale == 5) ? 1024 : 0;
    
    static constexpr double time_unit = (double)prescale_divide / F_MCK;
    static constexpr double time_freq = 1.0 / time_unit;
    
    void init (Context c, bool need_tc1, bool need_tc2)
    {
        m_offset = 0;
        m_status[0] = 0;
        m_status[1] = 0;
        m_status[2] = 0;
        m_mask[0] = AT91C_TC_COVFS;
        m_mask[1] = 0;
        m_mask[2] = 0;
        
        at91sam7s_pmc_enable_periph(AT91C_ID_TC0);
        AT91C_BASE_TCB->TCB_TC0.TC_CMR = (Prescale - 1) | AT91C_TC_WAVE | AT91C_TC_EEVT_XC0;
        AT91C_BASE_TCB->TCB_TC0.TC_IDR = UINT32_MAX;
        at91sam7s_aic_register_irq(AT91C_ID_TC0, AT91C_AIC_SRCTYPE_POSITIVE_EDGE, 4, At91Sam7sClock_TC0_Irq);
        at91sam7s_aic_enable_irq(AT91C_ID_TC0);
        AT91C_BASE_TCB->TCB_TC0.TC_IER = AT91C_TC_COVFS;
        
        if (need_tc1) {
            at91sam7s_pmc_enable_periph(AT91C_ID_TC1);
            AT91C_BASE_TCB->TCB_TC1.TC_CMR = (Prescale - 1) | AT91C_TC_WAVE | AT91C_TC_EEVT_XC0;
            AT91C_BASE_TCB->TCB_TC1.TC_IDR = UINT32_MAX;
            at91sam7s_aic_register_irq(AT91C_ID_TC1, AT91C_AIC_SRCTYPE_POSITIVE_EDGE, 4, At91Sam7sClock_TC1_Irq);
            at91sam7s_aic_enable_irq(AT91C_ID_TC1);
        }
        
        if (need_tc2) {
            at91sam7s_pmc_enable_periph(AT91C_ID_TC2);
            AT91C_BASE_TCB->TCB_TC2.TC_CMR = (Prescale - 1) | AT91C_TC_WAVE | AT91C_TC_EEVT_XC0;
            AT91C_BASE_TCB->TCB_TC2.TC_IDR = UINT32_MAX;
            at91sam7s_aic_register_irq(AT91C_ID_TC2, AT91C_AIC_SRCTYPE_POSITIVE_EDGE, 4, At91Sam7sClock_TC2_Irq);
            at91sam7s_aic_enable_irq(AT91C_ID_TC2);
        }
        
        AT91C_BASE_TCB->TCB_TC0.TC_CCR = AT91C_TC_CLKEN | AT91C_TC_SWTRG;
        if (need_tc1) {
            AT91C_BASE_TCB->TCB_TC1.TC_CCR = AT91C_TC_CLKEN | AT91C_TC_SWTRG;
        }
        if (need_tc2) {
            AT91C_BASE_TCB->TCB_TC2.TC_CCR = AT91C_TC_CLKEN | AT91C_TC_SWTRG;
        }
        
        this->debugInit(c);
    }
    
    void deinit (Context c, bool need_tc1, bool need_tc2)
    {
        this->debugDeinit(c);
        
        if (need_tc2) {
            at91sam7s_aic_disable_irq(AT91C_ID_TC2);
            AT91C_BASE_TCB->TCB_TC2.TC_CCR = AT91C_TC_CLKDIS;
            (void)AT91C_BASE_TCB->TCB_TC2.TC_SR;
            at91sam7s_pmc_disable_periph(AT91C_ID_TC2);
        }
        
        if (need_tc1) {
            at91sam7s_aic_disable_irq(AT91C_ID_TC1);
            AT91C_BASE_TCB->TCB_TC1.TC_CCR = AT91C_TC_CLKDIS;
            (void)AT91C_BASE_TCB->TCB_TC1.TC_SR;
            at91sam7s_pmc_disable_periph(AT91C_ID_TC1);
        }
        
        at91sam7s_aic_disable_irq(AT91C_ID_TC0);
        tc()->TC_CCR = AT91C_TC_CLKDIS;
        (void)tc()->TC_SR;
        at91sam7s_pmc_disable_periph(AT91C_ID_TC0);
    }
    
    template <typename ThisContext>
    TimeType getTime (ThisContext c)
    {
        this->debugAccess(c);
        
        TimeType now;
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c, {
            now = m_offset + tc()->TC_CV;
            m_status[0] |= tc()->TC_SR;
            if ((m_status[0] & AT91C_TC_COVFS)) {
                now = (TimeType)(m_offset + tc()->TC_CV) + UINT32_C(0x00010000);
            }
        });
        
        return now;
    }
    
    void tc0_irq (InterruptContext<Context> c)
    {
        uint32_t status = (m_status[0] | AT91C_BASE_TCB->TCB_TC0.TC_SR) & m_mask[0];
        if ((status & AT91C_TC_COVFS)) {
            m_offset += UINT32_C(0x00010000);
        }
        m_status[0] = 0;
        At91Sam7SClock_IrqHandlerTC0A(status);
        At91Sam7SClock_IrqHandlerTC0B(status);
        At91Sam7SClock_IrqHandlerTC0C(status);
    }
    
    void tc1_irq (InterruptContext<Context> c)
    {
        uint32_t status = (m_status[1] | AT91C_BASE_TCB->TCB_TC1.TC_SR) & m_mask[1];
        m_status[1] = 0;
        At91Sam7SClock_IrqHandlerTC1A(status);
        At91Sam7SClock_IrqHandlerTC1B(status);
        At91Sam7SClock_IrqHandlerTC1C(status);
    }
    
    void tc2_irq (InterruptContext<Context> c)
    {
        uint32_t status = (m_status[2] | AT91C_BASE_TCB->TCB_TC2.TC_SR) & m_mask[2];
        m_status[2] = 0;
        At91Sam7SClock_IrqHandlerTC2A(status);
        At91Sam7SClock_IrqHandlerTC2B(status);
        At91Sam7SClock_IrqHandlerTC2C(status);
    }
    
public:
    static AT91S_TC volatile * tc (void)
    {
        return &AT91C_BASE_TCB->TCB_TC0;
    }
    
public:
    TimeType m_offset;
    uint32_t m_status[3];
    uint32_t m_mask[3];
};

template <typename Context, typename Handler, uint32_t tc_addr, uint32_t cp_reg_offset, uint32_t cp_mask, int tc_num>
class At91Sam7sClockInterruptTimer
: private DebugObject<Context, void>
{
public:
    typedef typename Context::Clock Clock;
    typedef typename Clock::TimeType TimeType;
    typedef InterruptContext<Context> HandlerContext;
    
    void init (Context c)
    {
        this->debugInit(c);
        
#ifdef AMBROLIB_ASSERTIONS
        m_running = false;
#endif
    }
    
    void deinit (Context c)
    {
        Clock *clock = c.clock();
        this->debugDeinit(c);
        
        my_tc()->TC_IDR = cp_mask;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c, {
            clock->m_mask[tc_num] &= ~cp_mask;
        });
    }
    
    template <typename ThisContext>
    void set (ThisContext c, TimeType time)
    {
        Clock *clock = c.clock();
        this->debugAccess(c);
        AMBRO_ASSERT(!m_running)
        
        m_time = time;
#ifdef AMBROLIB_ASSERTIONS
        m_running = true;
#endif
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c, {
            TimeType now = clock->m_offset + Clock::tc()->TC_CV;
            clock->m_status[0] |= Clock::tc()->TC_SR;
            if ((clock->m_status[0] & AT91C_TC_COVFS)) {
                now = (TimeType)(clock->m_offset + Clock::tc()->TC_CV) + UINT32_C(0x00010000);
            }
            now -= time;
            now += clearance;
            if (now < UINT32_C(0x80000000)) {
                time += now;
            }
            *my_cp_reg() = time;
            clock->m_status[tc_num] |= my_tc()->TC_SR;
            my_tc()->TC_IER = cp_mask;
            clock->m_status[tc_num] &= ~cp_mask;
            clock->m_mask[tc_num] |= cp_mask;
        });
    }
    
    template <typename ThisContext>
    void unset (ThisContext c)
    {
        Clock *clock = c.clock();
        this->debugAccess(c);
        
        my_tc()->TC_IDR = cp_mask;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c, {
            clock->m_mask[tc_num] &= ~cp_mask;
        });
        
#ifdef AMBROLIB_ASSERTIONS
        m_running = false;
#endif
    }
    
    template <uint32_t check_tc_addr, int check_cp_mask>
    void irq_handler (InterruptContext<Context> c, uint32_t status)
    {
        static_assert(check_tc_addr == tc_addr && check_cp_mask == cp_mask, "incorrect GLOBAL macro used");
        Clock *clock = c.clock();
        
        if (!(status & cp_mask)) {
            return;
        }
        
        AMBRO_ASSERT(m_running)
        
        TimeType now = clock->m_offset + Clock::tc()->TC_CV;
        clock->m_status[0] |= Clock::tc()->TC_SR;
        if ((clock->m_status[0] & AT91C_TC_COVFS)) {
            now = (TimeType)(clock->m_offset + Clock::tc()->TC_CV) + UINT32_C(0x00010000);
        }
        now -= m_time;
        
        if (now < UINT32_C(0x80000000)) {
#ifdef AMBROLIB_ASSERTIONS
            m_running = false;
#endif
            if (!Handler::call(this, c)) {
                my_tc()->TC_IDR = cp_mask;
                clock->m_mask[tc_num] &= ~cp_mask;
            }
        }
    }
    
private:
    static AT91S_TC * my_tc (void)
    {
        return (AT91S_TC *)tc_addr;
    }
    
    static AT91_REG * my_cp_reg (void)
    {
        return (AT91_REG *)(tc_addr + cp_reg_offset);
    }
    
    static const TimeType clearance = (64 / Clock::prescale_divide) + 2;
    
    TimeType m_time;
#ifdef AMBROLIB_ASSERTIONS
    bool m_running;
#endif
};

// Hardcode these due to a compiler bug.
// 20 = offsetof(AT91S_TC, TC_RA)
// 24 = offsetof(AT91S_TC, TC_RB)
// 28 = offsetof(AT91S_TC, TC_RC)

template <typename Context, typename Handler>
using At91Sam7sClockInterruptTimer_TC0A = At91Sam7sClockInterruptTimer<Context, Handler, (uint32_t)AT91C_BASE_TC0, 20, AT91C_TC_CPAS, 0>;

template <typename Context, typename Handler>
using At91Sam7sClockInterruptTimer_TC0B = At91Sam7sClockInterruptTimer<Context, Handler, (uint32_t)AT91C_BASE_TC0, 24, AT91C_TC_CPBS, 0>;

template <typename Context, typename Handler>
using At91Sam7sClockInterruptTimer_TC0C = At91Sam7sClockInterruptTimer<Context, Handler, (uint32_t)AT91C_BASE_TC0, 28, AT91C_TC_CPCS, 0>;

template <typename Context, typename Handler>
using At91Sam7sClockInterruptTimer_TC1A = At91Sam7sClockInterruptTimer<Context, Handler, (uint32_t)AT91C_BASE_TC1, 20, AT91C_TC_CPAS, 1>;

template <typename Context, typename Handler>
using At91Sam7sClockInterruptTimer_TC1B = At91Sam7sClockInterruptTimer<Context, Handler, (uint32_t)AT91C_BASE_TC1, 24, AT91C_TC_CPBS, 1>;

template <typename Context, typename Handler>
using At91Sam7sClockInterruptTimer_TC1C = At91Sam7sClockInterruptTimer<Context, Handler, (uint32_t)AT91C_BASE_TC1, 28, AT91C_TC_CPCS, 1>;

template <typename Context, typename Handler>
using At91Sam7sClockInterruptTimer_TC2A = At91Sam7sClockInterruptTimer<Context, Handler, (uint32_t)AT91C_BASE_TC2, 20, AT91C_TC_CPAS, 2>;

template <typename Context, typename Handler>
using At91Sam7sClockInterruptTimer_TC2B = At91Sam7sClockInterruptTimer<Context, Handler, (uint32_t)AT91C_BASE_TC2, 24, AT91C_TC_CPBS, 2>;

template <typename Context, typename Handler>
using At91Sam7sClockInterruptTimer_TC2C = At91Sam7sClockInterruptTimer<Context, Handler, (uint32_t)AT91C_BASE_TC2, 28, AT91C_TC_CPCS, 2>;

#define AMBRO_AT91SAM7S_CLOCK_GLOBAL(the_clock, context) \
void At91Sam7sClock_TC0_Irq (void) \
{ \
    (the_clock).tc0_irq(MakeInterruptContext((context))); \
} \
void At91Sam7sClock_TC1_Irq (void) \
{ \
    (the_clock).tc1_irq(MakeInterruptContext((context))); \
} \
void At91Sam7sClock_TC2_Irq (void) \
{ \
    (the_clock).tc2_irq(MakeInterruptContext((context))); \
}

#define AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC0A_GLOBAL(the_timer, context) \
void At91Sam7SClock_IrqHandlerTC0A (uint32_t status) \
{ \
    (the_timer).template irq_handler<(uint32_t)AT91C_BASE_TC0, AT91C_TC_CPAS>(MakeInterruptContext((context)), status); \
}

#define AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC0B_GLOBAL(the_timer, context) \
void At91Sam7SClock_IrqHandlerTC0B (uint32_t status) \
{ \
    (the_timer).template irq_handler<(uint32_t)AT91C_BASE_TC0, AT91C_TC_CPBS>(MakeInterruptContext((context)), status); \
}

#define AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC0C_GLOBAL(the_timer, context) \
void At91Sam7SClock_IrqHandlerTC0C (uint32_t status) \
{ \
    (the_timer).template irq_handler<(uint32_t)AT91C_BASE_TC0, AT91C_TC_CPCS>(MakeInterruptContext((context)), status); \
}

#define AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC1A_GLOBAL(the_timer, context) \
void At91Sam7SClock_IrqHandlerTC1A (uint32_t status) \
{ \
    (the_timer).template irq_handler<(uint32_t)AT91C_BASE_TC1, AT91C_TC_CPAS>(MakeInterruptContext((context)), status); \
}

#define AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC1B_GLOBAL(the_timer, context) \
void At91Sam7SClock_IrqHandlerTC1B (uint32_t status) \
{ \
    (the_timer).template irq_handler<(uint32_t)AT91C_BASE_TC1, AT91C_TC_CPBS>(MakeInterruptContext((context)), status); \
}

#define AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC1C_GLOBAL(the_timer, context) \
void At91Sam7SClock_IrqHandlerTC1C (uint32_t status) \
{ \
    (the_timer).template irq_handler<(uint32_t)AT91C_BASE_TC1, AT91C_TC_CPCS>(MakeInterruptContext((context)), status); \
}

#define AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC2A_GLOBAL(the_timer, context) \
void At91Sam7SClock_IrqHandlerTC2A (uint32_t status) \
{ \
    (the_timer).template irq_handler<(uint32_t)AT91C_BASE_TC2, AT91C_TC_CPAS>(MakeInterruptContext((context)), status); \
}

#define AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC2B_GLOBAL(the_timer, context) \
void At91Sam7SClock_IrqHandlerTC2B (uint32_t status) \
{ \
    (the_timer).template irq_handler<(uint32_t)AT91C_BASE_TC2, AT91C_TC_CPBS>(MakeInterruptContext((context)), status); \
}

#define AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC2C_GLOBAL(the_timer, context) \
void At91Sam7SClock_IrqHandlerTC2C (uint32_t status) \
{ \
    (the_timer).template irq_handler<(uint32_t)AT91C_BASE_TC2, AT91C_TC_CPCS>(MakeInterruptContext((context)), status); \
}

#define AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC0A_UNUSED_GLOBAL \
void At91Sam7SClock_IrqHandlerTC0A (uint32_t status) {}

#define AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC0B_UNUSED_GLOBAL \
void At91Sam7SClock_IrqHandlerTC0B (uint32_t status) {}

#define AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC0C_UNUSED_GLOBAL \
void At91Sam7SClock_IrqHandlerTC0C (uint32_t status) {}

#define AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC1A_UNUSED_GLOBAL \
void At91Sam7SClock_IrqHandlerTC1A (uint32_t status) {}

#define AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC1B_UNUSED_GLOBAL \
void At91Sam7SClock_IrqHandlerTC1B (uint32_t status) {}

#define AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC1C_UNUSED_GLOBAL \
void At91Sam7SClock_IrqHandlerTC1C (uint32_t status) {}

#define AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC2A_UNUSED_GLOBAL \
void At91Sam7SClock_IrqHandlerTC2A (uint32_t status) {}

#define AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC2B_UNUSED_GLOBAL \
void At91Sam7SClock_IrqHandlerTC2B (uint32_t status) {}

#define AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC2C_UNUSED_GLOBAL \
void At91Sam7SClock_IrqHandlerTC2C (uint32_t status) {}

#include <aprinter/EndNamespace.h>

#endif
