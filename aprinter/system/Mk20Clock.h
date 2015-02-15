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

#include <aprinter/base/Object.h>
#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/IndexElemList.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/IsEqualFunc.h>
#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/GetMemberTypeFunc.h>
#include <aprinter/meta/PowerOfTwo.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/WrapDouble.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/system/Mk20Pins.h>

template <typename Ftm, int ChannelIndex>
struct Mk20Clock__IrqCompHelper {
    template <typename IrqTime>
    static void call (IrqTime irq_time) {}
};

#include <aprinter/BeginNamespace.h>

using Mk20ClockDefaultExtraClearance = AMBRO_WRAP_DOUBLE(0.0);

template <
    uint32_t TScAddr, uint32_t TCntAddr, uint32_t TModAddr, uint32_t TCntinAddr,
    uint32_t TScgc6Bit, int TIrq, typename TChannels
>
struct Mk20ClockFTM {
    static uint32_t volatile * sc () { return (uint32_t volatile *)TScAddr; }
    static uint32_t volatile * cnt () { return (uint32_t volatile *)TCntAddr; }
    static uint32_t volatile * mod () { return (uint32_t volatile *)TModAddr; }
    static uint32_t volatile * cntin () { return (uint32_t volatile *)TCntinAddr; }
    static uint32_t const Scgc6Bit = TScgc6Bit;
    static const int Irq = TIrq;
    using Channels = TChannels;
};

template <uint32_t TCscAddr, uint32_t TCvAddr, typename TPinsList>
struct Mk20Clock__Channel {
    static uint32_t volatile * csc () { return (uint32_t volatile *)TCscAddr; }
    static uint32_t volatile * cv () { return (uint32_t volatile *)TCvAddr; }
    using PinsList = TPinsList;
};

template <typename TPin, uint8_t TAlternateFunction>
struct Mk20Clock__ChannelPin {
    using Pin = TPin;
    static uint8_t const AlternateFunction = TAlternateFunction;
};

using Mk20ClockFTM0 = Mk20ClockFTM<
    (uint32_t)&FTM0_SC, (uint32_t)&FTM0_CNT, (uint32_t)&FTM0_MOD, (uint32_t)&FTM0_CNTIN,
    SIM_SCGC6_FTM0, IRQ_FTM0, MakeTypeList<
        Mk20Clock__Channel<(uint32_t)&FTM0_C0SC, (uint32_t)&FTM0_C0V, MakeTypeList<
            Mk20Clock__ChannelPin<Mk20Pin<Mk20PortA, 3>, 3>,
            Mk20Clock__ChannelPin<Mk20Pin<Mk20PortC, 1>, 4>
        >>,
        Mk20Clock__Channel<(uint32_t)&FTM0_C1SC, (uint32_t)&FTM0_C1V, MakeTypeList<
            Mk20Clock__ChannelPin<Mk20Pin<Mk20PortA, 4>, 3>,
            Mk20Clock__ChannelPin<Mk20Pin<Mk20PortC, 2>, 4>
        >>,
        Mk20Clock__Channel<(uint32_t)&FTM0_C2SC, (uint32_t)&FTM0_C2V, MakeTypeList<
            Mk20Clock__ChannelPin<Mk20Pin<Mk20PortA, 5>, 3>,
            Mk20Clock__ChannelPin<Mk20Pin<Mk20PortC, 3>, 4>
        >>,
        Mk20Clock__Channel<(uint32_t)&FTM0_C3SC, (uint32_t)&FTM0_C3V, MakeTypeList<
            Mk20Clock__ChannelPin<Mk20Pin<Mk20PortC, 4>, 4>
        >>,
        Mk20Clock__Channel<(uint32_t)&FTM0_C4SC, (uint32_t)&FTM0_C4V, MakeTypeList<
            Mk20Clock__ChannelPin<Mk20Pin<Mk20PortD, 4>, 4>
        >>,
        Mk20Clock__Channel<(uint32_t)&FTM0_C5SC, (uint32_t)&FTM0_C5V, MakeTypeList<
            Mk20Clock__ChannelPin<Mk20Pin<Mk20PortA, 0>, 3>,
            Mk20Clock__ChannelPin<Mk20Pin<Mk20PortD, 5>, 4>
        >>,
        Mk20Clock__Channel<(uint32_t)&FTM0_C6SC, (uint32_t)&FTM0_C6V, MakeTypeList<
            Mk20Clock__ChannelPin<Mk20Pin<Mk20PortA, 1>, 3>,
            Mk20Clock__ChannelPin<Mk20Pin<Mk20PortD, 6>, 4>
        >>,
        Mk20Clock__Channel<(uint32_t)&FTM0_C7SC, (uint32_t)&FTM0_C7V, MakeTypeList<
            Mk20Clock__ChannelPin<Mk20Pin<Mk20PortA, 2>, 3>,
            Mk20Clock__ChannelPin<Mk20Pin<Mk20PortD, 7>, 4>
        >>
    >
>;

using Mk20ClockFTM1 = Mk20ClockFTM<
    (uint32_t)&FTM1_SC, (uint32_t)&FTM1_CNT, (uint32_t)&FTM1_MOD, (uint32_t)&FTM1_CNTIN,
    SIM_SCGC6_FTM1, IRQ_FTM1, MakeTypeList<
        Mk20Clock__Channel<(uint32_t)&FTM1_C0SC, (uint32_t)&FTM1_C0V, MakeTypeList<
            Mk20Clock__ChannelPin<Mk20Pin<Mk20PortA, 12>, 3>,
            Mk20Clock__ChannelPin<Mk20Pin<Mk20PortB, 0>, 3>
        >>,
        Mk20Clock__Channel<(uint32_t)&FTM1_C1SC, (uint32_t)&FTM1_C1V, MakeTypeList<
            Mk20Clock__ChannelPin<Mk20Pin<Mk20PortA, 13>, 3>,
            Mk20Clock__ChannelPin<Mk20Pin<Mk20PortB, 1>, 3>
        >>
    >
>;

template <uint8_t PrescaleReduce, uint8_t NumBits>
struct Mk20ClockFtmModeClock {};

template <uint8_t Prescale, uint16_t TopVal>
struct Mk20ClockFtmModeCustom {};

template <typename TFtm, typename TMode = Mk20ClockFtmModeClock<0, 16>>
struct Mk20ClockFtmSpec {
    using Ftm = TFtm;
    using Mode = TMode;
};

template <typename, typename, typename, typename, int, typename>
class Mk20ClockInterruptTimer;

template <typename, typename, typename, int, typename>
class Mk20ClockPwm;

template <typename Context, typename ParentObject, int Prescale, typename FtmsList>
class Mk20Clock {
    static_assert(Prescale >= 0, "");
    static_assert(Prescale <= 7, "");
    
    template <typename, typename, typename, typename, int, typename>
    friend class Mk20ClockInterruptTimer;
    
    template <typename, typename, typename, int, typename>
    friend class Mk20ClockPwm;
    
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_init, init)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_init_start, init_start)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_deinit, deinit)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_irq_helper, irq_helper)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_Ftm, Ftm)
    
public:
    struct Object;
    using TimeType = uint32_t;
    
    static constexpr TimeType prescale_divide = PowerOfTwo<TimeType, Prescale>::Value;
    
    static constexpr double time_unit = (double)prescale_divide / F_BUS;
    static constexpr double time_freq = (double)F_BUS / prescale_divide;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
    template <int FtmIndex>
    struct MyFtm {
        using FtmSpec = TypeListGet<FtmsList, FtmIndex>;
        using Ftm = typename FtmSpec::Ftm;
        using Channels = typename Ftm::Channels;
        
        template <typename TheMode>
        struct ModeHelper;
        
        template <uint8_t PrescaleReduce, uint8_t NumBits>
        struct ModeHelper<Mk20ClockFtmModeClock<PrescaleReduce, NumBits>> {
            static_assert(FtmIndex != 0 || PrescaleReduce == 0, "");
            static_assert(PrescaleReduce <= Prescale, "");
            static_assert(NumBits >= 1, "");
            static_assert(NumBits <= 16, "");
            
            static uint8_t const FtmPrescale = Prescale - PrescaleReduce;
            static uint16_t const TopVal = PowerOfTwoMinusOne<uint16_t, NumBits>::Value;
            static bool const SupportsTimer = true;
            
            template <int ChannelIndex>
            struct Channel {
                static void irq_helper (InterruptContext<Context> c, TimeType irq_time)
                {
                    Mk20Clock__IrqCompHelper<Ftm, ChannelIndex>::call(irq_time);
                }
            };
            
            using ChannelsList = IndexElemList<Channels, Channel>;
            
            static void handle_irq (InterruptContext<Context> c)
            {
                if (FtmIndex == 0) {
                    AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                        uint32_t sc = *Ftm::sc();
                        if (sc & FTM_SC_TOF) {
                            *Ftm::sc() = sc & ~FTM_SC_TOF;
                            Object::self(c)->m_offset += (uint32_t)TopVal + 1;
                        }
                    }
                }
                TimeType irq_time = getTime(c);
                ListForEachForward<ChannelsList>(Foreach_irq_helper(), c, irq_time);
            }
            
            static uint16_t make_target_time (TimeType time)
            {
                return ((time << PrescaleReduce) & TopVal);
            }
        };
        
        template <uint8_t CustomPrescale, uint16_t CustomTopVal>
        struct ModeHelper<Mk20ClockFtmModeCustom<CustomPrescale, CustomTopVal>> {
            static_assert(CustomPrescale <= 7, "");
            static_assert(CustomTopVal >= 1, "");
            
            static uint8_t const FtmPrescale = CustomPrescale;
            static uint16_t const TopVal = CustomTopVal;
            static bool const SupportsTimer = false;
            
            static void handle_irq (InterruptContext<Context> c) {}
        };
        
        using TheModeHelper = ModeHelper<typename FtmSpec::Mode>;
        
        static_assert(FtmIndex != 0 || TheModeHelper::SupportsTimer, "First FTM in FtmsList must be ModeClock.");
        
        static void init (Context c)
        {
            SIM_SCGC6 |= Ftm::Scgc6Bit;
            *Ftm::sc();
            *Ftm::sc() = FTM_SC_PS(TheModeHelper::FtmPrescale) | (FtmIndex == 0 ? FTM_SC_TOIE : 0);
            *Ftm::mod() = TheModeHelper::TopVal;
            *Ftm::cntin() = (FtmIndex == 0) ? 1 : 0;
            *Ftm::cnt() = 0; // this actually sets CNT to the value in CNTIN above
            *Ftm::cntin() = 0;
            NVIC_CLEAR_PENDING(Ftm::Irq);
            NVIC_SET_PRIORITY(Ftm::Irq, INTERRUPT_PRIORITY);
            NVIC_ENABLE_IRQ(Ftm::Irq);
        }
        
        static void init_start (Context c)
        {
            *Ftm::sc() |= FTM_SC_CLKS(1);
        }
        
        static void deinit (Context c)
        {
            NVIC_DISABLE_IRQ(Ftm::Irq);
            *Ftm::sc() = 0;
            *Ftm::sc();
            *Ftm::sc() = 0;
            NVIC_CLEAR_PENDING(Ftm::Irq);
            SIM_SCGC6 &= ~Ftm::Scgc6Bit;
        }
        
        static void irq_handler (InterruptContext<Context> c)
        {
            TheModeHelper::handle_irq(c);
        }
    };
    
    using MyFtmsList = IndexElemList<FtmsList, MyFtm>;
    
    template <typename Ftm>
    using FindFtm = MyFtm<TypeDictListIndexMapped<FtmsList, GetMemberType_Ftm, Ftm>::Value>;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->m_offset = 0;
        
        memory_barrier();
        
        ListForEachForward<MyFtmsList>(Foreach_init(), c);
        ListForEachForward<MyFtmsList>(Foreach_init_start(), c);
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        ListForEachReverse<MyFtmsList>(Foreach_deinit(), c);
        
        memory_barrier();
    }
    
    template <typename ThisContext>
    static TimeType getTime (ThisContext c)
    {
        auto *o = Object::self(c);
        
        uint32_t offset;
        uint32_t low;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            offset = o->m_offset;
            low = *MyFtm<0>::Ftm::cnt();
            if (*MyFtm<0>::Ftm::sc() & FTM_SC_TOF) {
                offset += (uint32_t)MyFtm<0>::TheModeHelper::TopVal + 1;
                low = *MyFtm<0>::Ftm::cnt();
            }
        }
        
        return (offset | low);
    }
    
    template <typename Ftm>
    static void ftm_irq_handler (InterruptContext<Context> c)
    {
        FindFtm<Ftm>::irq_handler(c);
    }
    
public:
    struct Object : public ObjBase<Mk20Clock, ParentObject, MakeTypeList<TheDebugObject>> {
        uint32_t m_offset;
    };
};

#define AMBRO_MK20_CLOCK_FTM_GLOBAL(ftmnum, clock, context) \
extern "C" \
__attribute__((used)) \
void ftm##ftmnum##_isr (void) \
{ \
    clock::ftm_irq_handler<Mk20ClockFTM##ftmnum>(MakeInterruptContext((context))); \
}

template <typename Context, typename ParentObject, typename Handler, typename TFtm, int TChannelIndex, typename ExtraClearance>
class Mk20ClockInterruptTimer {
public:
    struct Object;
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using HandlerContext = InterruptContext<Context>;
    using Ftm = TFtm;
    static int const ChannelIndex = TChannelIndex;
    
private:
    using TheMyFtm = typename Clock::template FindFtm<Ftm>;
    static_assert(TheMyFtm::TheModeHelper::SupportsTimer, "InterruptTimer requires a ModeClock FTM.");
    using Channel = TypeListGet<typename Ftm::Channels, ChannelIndex>;
    using TheDebugObject = DebugObject<Context, Object>;
    
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
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        *Channel::csc() = 0;
        
        memory_barrier();
    }
    
    template <typename ThisContext>
    static void setFirst (ThisContext c, TimeType time)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(!o->m_running)
        AMBRO_ASSERT(!(*Channel::csc() & FTM_CSC_CHIE))
        
        o->m_time = time;
#ifdef AMBROLIB_ASSERTIONS
        o->m_running = true;
#endif
        
        memory_barrier();
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            TimeType now = Clock::getTime(lock_c);
            now -= time;
            now += clearance;
            if (now < UINT32_C(0x80000000)) {
                time += now;
            }
            *Channel::cv() = TheMyFtm::TheModeHelper::make_target_time(time);
            *Channel::csc();
            *Channel::csc() = FTM_CSC_MSA | FTM_CSC_CHIE;
        }
    }
    
    static void setNext (HandlerContext c, TimeType time)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_running)
        AMBRO_ASSERT((*Channel::csc() & FTM_CSC_CHIE))
        
        o->m_time = time;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            TimeType now = Clock::getTime(lock_c);
            now -= time;
            now += clearance;
            if (now < UINT32_C(0x80000000)) {
                time += now;
            }
            *Channel::cv() = TheMyFtm::TheModeHelper::make_target_time(time);
        }
    }
    
    template <typename ThisContext>
    static void unset (ThisContext c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        *Channel::csc() = 0;
        
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
    
    static void irq_handler (InterruptContext<Context> c, TimeType irq_time)
    {
        auto *o = Object::self(c);
        
        uint32_t csc;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            csc = *Channel::csc();
            *Channel::csc() = (csc & ~FTM_CSC_CHF);
        }
        
        if (!(csc & FTM_CSC_CHIE)) {
            return;
        }
        
        AMBRO_ASSERT(o->m_running)
        
        if ((TimeType)(irq_time - o->m_time) < UINT32_C(0x80000000)) {
            if (!Handler::call(c)) {
#ifdef AMBROLIB_ASSERTIONS
                o->m_running = false;
#endif
                *Channel::csc() = 0;
            }
        }
    }
    
private:
    static const TimeType clearance = MaxValue<TimeType>((128 / Clock::prescale_divide) + 2, ExtraClearance::value() * Clock::time_freq);
    
public:
    struct Object : public ObjBase<Mk20ClockInterruptTimer, ParentObject, MakeTypeList<TheDebugObject>> {
        TimeType m_time;
#ifdef AMBROLIB_ASSERTIONS
        bool m_running;
#endif
    };
};

template <typename Ftm, int ChannelIndex, typename ExtraClearance = Mk20ClockDefaultExtraClearance>
struct Mk20ClockInterruptTimerService {
    template <typename Context, typename ParentObject, typename Handler>
    using InterruptTimer = Mk20ClockInterruptTimer<Context, ParentObject, Handler, Ftm, ChannelIndex, ExtraClearance>;
};

#define AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(ftm, channel_index, timer, context) \
static_assert( \
    TypesAreEqual<timer::Ftm, ftm>::Value && \
    timer::ChannelIndex == channel_index, \
    "Incorrect INTERRUPT_TIMER_GLOBAL macro used" \
); \
template <> \
struct Mk20Clock__IrqCompHelper<ftm, channel_index> { \
    template <typename IrqTime> \
    static void call (IrqTime irq_time) \
    { \
        timer::irq_handler(MakeInterruptContext((context)), irq_time); \
    } \
};

template <typename Context, typename ParentObject, typename TFtm, int TChannelIndex, typename TPin>
class Mk20ClockPwm {
public:
    struct Object;
    using Clock = typename Context::Clock;
    using Ftm = TFtm;
    static int const ChannelIndex = TChannelIndex;
    using Pin = TPin;
    
private:
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_Pin, Pin)
    
    using TheMyFtm = typename Clock::template FindFtm<Ftm>;
    static_assert(TheMyFtm::TheModeHelper::TopVal < UINT16_C(0xFFFF), "TopVal must be less than 0xFFFF.");
    using Channel = TypeListGet<typename Ftm::Channels, ChannelIndex>;
    using ChannelPin = TypeDictListGetMapped<typename Channel::PinsList, GetMemberType_Pin, Pin>;
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    using DutyCycleType = ChooseInt<BitsInInt<(TheMyFtm::TheModeHelper::TopVal + 1)>::Value, false>;
    static DutyCycleType const MaxDutyCycle = TheMyFtm::TheModeHelper::TopVal + 1;
    
    static void init (Context c)
    {
        // Force channel output to low by abusing output compare mode.
        *Channel::cv() = 0;
        *Channel::csc() = 0;
        *Channel::csc() = FTM_CSC_MSA | FTM_CSC_ELSB;
        while (!(*Channel::csc() & FTM_CSC_CHF));
        
        // Switch to EPWM mode.
        *Channel::csc() = FTM_CSC_MSB | FTM_CSC_ELSB;
        
        // Enable output on pin.
        Context::Pins::template setOutput<Pin, Mk20PinOutputModeNormal, ChannelPin::AlternateFunction>(c);
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
        
        // Set pin to low.
        Context::Pins::template set<Pin>(c, false);
        Context::Pins::template setOutput<Pin>(c);
        
        // Reset the channel.
        *Channel::csc();
        *Channel::csc() = 0;
    }
    
    template <typename ThisContext>
    static void setDutyCycle (ThisContext c, DutyCycleType duty_cycle)
    {
        AMBRO_ASSERT(duty_cycle <= MaxDutyCycle)
        
        // Update duty cycle. This will be buffered until next
        // counter reset because we've left FTMEN=0.
        *Channel::cv() = duty_cycle;
    }
    
    static void emergencySetOff ()
    {
        Context::Pins::template emergencySet<Pin>(false);
        Context::Pins::template emergencySetOutput<Pin>();
    }
    
public:
    struct Object : public ObjBase<Mk20ClockPwm, ParentObject, MakeTypeList<TheDebugObject>> {};
};

template <typename Ftm, int ChannelIndex, typename Pin>
struct Mk20ClockPwmService {
    template <typename Context, typename ParentObject>
    using Pwm = Mk20ClockPwm<Context, ParentObject, Ftm, ChannelIndex, Pin>;
};

#include <aprinter/EndNamespace.h>

#endif
