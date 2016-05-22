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
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/FuncUtils.h>
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/meta/MemberType.h>
#include <aprinter/meta/PowerOfTwo.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/hal/teensy3/Mk20Pins.h>

template <typename Ftm, int ChannelIndex>
struct Mk20Clock__IrqCompHelper {
    static void call () {}
};

#include <aprinter/BeginNamespace.h>

using Mk20ClockDefaultExtraClearance = AMBRO_WRAP_DOUBLE(0.0);

#define MK20_CLOCK_DEFINE_FTM(ftm_num, channels_list) \
struct Mk20ClockFTM##ftm_num { \
    static uint32_t volatile * sc () { return &FTM##ftm_num##_SC; } \
    static uint32_t volatile * cnt () { return &FTM##ftm_num##_CNT; } \
    static uint32_t volatile * mod () { return &FTM##ftm_num##_MOD; } \
    static uint32_t const Scgc6Bit = SIM_SCGC6_FTM##ftm_num; \
    static const int Irq = IRQ_FTM##ftm_num; \
    using Channels = channels_list; \
};

#define MK20_CLOCK_DEFINE_CHANNEL(ftm_num, chan_num, pins_list) \
struct Mk20Clock__Channel##ftm_num##_##chan_num { \
    static uint32_t volatile * csc () { return &FTM##ftm_num##_C##chan_num##SC; } \
    static uint32_t volatile * cv () { return &FTM##ftm_num##_C##chan_num##V; } \
    using PinsList = pins_list; \
};

template <typename TPin, uint8_t TAlternateFunction>
struct Mk20Clock__ChannelPin {
    using Pin = TPin;
    static uint8_t const AlternateFunction = TAlternateFunction;
};

using Mk20Clock__0_0_Pins = MakeTypeList<Mk20Clock__ChannelPin<Mk20Pin<Mk20PortA, 3>, 3>, Mk20Clock__ChannelPin<Mk20Pin<Mk20PortC, 1>, 4>>;
MK20_CLOCK_DEFINE_CHANNEL(0, 0, Mk20Clock__0_0_Pins)
using Mk20Clock__0_1_Pins = MakeTypeList<Mk20Clock__ChannelPin<Mk20Pin<Mk20PortA, 4>, 3>, Mk20Clock__ChannelPin<Mk20Pin<Mk20PortC, 2>, 4>>;
MK20_CLOCK_DEFINE_CHANNEL(0, 1, Mk20Clock__0_1_Pins)
using Mk20Clock__0_2_Pins = MakeTypeList<Mk20Clock__ChannelPin<Mk20Pin<Mk20PortA, 5>, 3>, Mk20Clock__ChannelPin<Mk20Pin<Mk20PortC, 3>, 4>>;
MK20_CLOCK_DEFINE_CHANNEL(0, 2, Mk20Clock__0_2_Pins)
using Mk20Clock__0_3_Pins = MakeTypeList<Mk20Clock__ChannelPin<Mk20Pin<Mk20PortC, 4>, 4>>;
MK20_CLOCK_DEFINE_CHANNEL(0, 3, Mk20Clock__0_3_Pins)
using Mk20Clock__0_4_Pins = MakeTypeList<Mk20Clock__ChannelPin<Mk20Pin<Mk20PortD, 4>, 4>>;
MK20_CLOCK_DEFINE_CHANNEL(0, 4, Mk20Clock__0_4_Pins)
using Mk20Clock__0_5_Pins = MakeTypeList<Mk20Clock__ChannelPin<Mk20Pin<Mk20PortA, 0>, 3>, Mk20Clock__ChannelPin<Mk20Pin<Mk20PortD, 5>, 4>>;
MK20_CLOCK_DEFINE_CHANNEL(0, 5, Mk20Clock__0_5_Pins)
using Mk20Clock__0_6_Pins = MakeTypeList<Mk20Clock__ChannelPin<Mk20Pin<Mk20PortA, 1>, 3>, Mk20Clock__ChannelPin<Mk20Pin<Mk20PortD, 6>, 4>>;
MK20_CLOCK_DEFINE_CHANNEL(0, 6, Mk20Clock__0_6_Pins)
using Mk20Clock__0_7_Pins = MakeTypeList<Mk20Clock__ChannelPin<Mk20Pin<Mk20PortA, 2>, 3>, Mk20Clock__ChannelPin<Mk20Pin<Mk20PortD, 7>, 4>>;
MK20_CLOCK_DEFINE_CHANNEL(0, 7, Mk20Clock__0_7_Pins)
using Mk20Clock__Ftm0Channels = MakeTypeList<
    Mk20Clock__Channel0_0, Mk20Clock__Channel0_1, Mk20Clock__Channel0_2, Mk20Clock__Channel0_3,
    Mk20Clock__Channel0_4, Mk20Clock__Channel0_5, Mk20Clock__Channel0_6, Mk20Clock__Channel0_7
>;
MK20_CLOCK_DEFINE_FTM(0, Mk20Clock__Ftm0Channels)

using Mk20Clock__1_0_Pins = MakeTypeList<Mk20Clock__ChannelPin<Mk20Pin<Mk20PortA, 12>, 3>, Mk20Clock__ChannelPin<Mk20Pin<Mk20PortB, 0>, 3>>;
MK20_CLOCK_DEFINE_CHANNEL(1, 0, Mk20Clock__1_0_Pins)
using Mk20Clock__1_1_Pins = MakeTypeList<Mk20Clock__ChannelPin<Mk20Pin<Mk20PortA, 13>, 3>, Mk20Clock__ChannelPin<Mk20Pin<Mk20PortB, 1>, 3>>;
MK20_CLOCK_DEFINE_CHANNEL(1, 1, Mk20Clock__1_1_Pins)
using Mk20Clock__Ftm1Channels = MakeTypeList<Mk20Clock__Channel1_0, Mk20Clock__Channel1_1>;
MK20_CLOCK_DEFINE_FTM(1, Mk20Clock__Ftm1Channels)

template <uint8_t PrescaleReduce, uint8_t NumBits>
struct Mk20ClockFtmModeClock {};

template <uint8_t Prescale, uint16_t TopVal>
struct Mk20ClockFtmModeCustom {};

template <typename TFtm, typename TMode = Mk20ClockFtmModeClock<0, 16>>
struct Mk20ClockFtmSpec {
    using Ftm = TFtm;
    using Mode = TMode;
};

template <typename>
class Mk20ClockInterruptTimer;

template <typename, typename, typename, int, typename>
class Mk20ClockPwm;

template <typename Arg>
class Mk20Clock {
    using Context      = typename Arg::Context;
    using ParentObject = typename Arg::ParentObject;
    using FtmsList     = typename Arg::FtmsList;
    using Params       = typename Arg::Params;
    
    static int const Prescale = Params::Prescale;
    static_assert(Prescale >= 0, "");
    static_assert(Prescale <= 7, "");
    
    template <typename>
    friend class Mk20ClockInterruptTimer;
    
    template <typename, typename, typename, int, typename>
    friend class Mk20ClockPwm;
    
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
                static void irq_helper (InterruptContext<Context> c)
                {
                    Mk20Clock__IrqCompHelper<Ftm, ChannelIndex>::call();
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
                ListFor<ChannelsList>([&] APRINTER_TL(channel, channel::irq_helper(c)));
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
            // Note: using CNTIN different than 0 is not allowed in Output Compare mode by the specs.
            NVIC_CLEAR_PENDING(Ftm::Irq);
            NVIC_SET_PRIORITY(Ftm::Irq, INTERRUPT_PRIORITY);
            NVIC_ENABLE_IRQ(Ftm::Irq);
        }
        
        static void init_start (Context c)
        {
            *Ftm::sc() |= FTM_SC_CLKS(1);
            
            // See explanation in init.
            if (FtmIndex == 0) {
                while (*MyFtm<0>::Ftm::cnt() == 0);
            }
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
    using FindFtm = MyFtm<TypeListIndexMapped<FtmsList, GetMemberType_Ftm, Ftm>::Value>;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->m_offset = 0;
        
        memory_barrier();
        
        ListFor<MyFtmsList>([&] APRINTER_TL(tc, tc::init(c)));
        
        // We need to make sure that timers other than the first timer
        // (which is used as a reference clock) never count in advance of
        // the first timer. They must be synchronized in step with or lag
        // behind the first timer slightly. This ensures that their
        // channel-match interrupts do not occur before the target time
        // has been achieved according to the reference, which would
        // make us lose events.
        // Typically we would ensure this by starting the first timer
        // with the initial value 1 and then starting other timers with
        // the initial value 0 (accounting for possibly different internal
        // prescaler states of timers). However, this hardware does not allow
        // configuring the initial value (CNTIN) for Output Compare mode.
        // Due to this restriction, we start the first timer at 0, wait for
        // it to increment, then start the other timers at zero.
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            ListFor<MyFtmsList>([&] APRINTER_TL(tc, tc::init_start(c)));
        }
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        ListForReverse<MyFtmsList>([&] APRINTER_TL(tc, tc::deinit(c)));
        
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

APRINTER_ALIAS_STRUCT_EXT(Mk20ClockService, (
    APRINTER_AS_VALUE(int, Prescale)
), (
    APRINTER_ALIAS_STRUCT_EXT(Clock, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(FtmsList)
    ), (
        using Params = Mk20ClockService;
        APRINTER_DEF_INSTANCE(Clock, Mk20Clock)
    ))
))

#define AMBRO_MK20_CLOCK_FTM_GLOBAL(ftmnum, clock, context) \
extern "C" \
__attribute__((used)) \
void ftm##ftmnum##_isr (void) \
{ \
    clock::ftm_irq_handler<Mk20ClockFTM##ftmnum>(MakeInterruptContext((context))); \
}

template <typename Arg>
class Mk20ClockInterruptTimer {
    using Context      = typename Arg::Context;
    using ParentObject = typename Arg::ParentObject;
    using Handler      = typename Arg::Handler;
    using Params       = typename Arg::Params;
    
public:
    struct Object;
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    using HandlerContext = InterruptContext<Context>;
    using Ftm = typename Params::Ftm;
    static int const ChannelIndex = Params::ChannelIndex;
    using ExtraClearance = typename Params::ExtraClearance;
    
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
        
        *Channel::csc() = FTM_CSC_MSA;
        
        o->m_cv = 0;
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
        
        /* IMPORTANT NOTE
         * 
         * The CnV register (compare value) is buffered by the hardware and will
         * only be updated at the next timer period, in our configuration.
         * 
         * Also take note of the following detail mentioned in the manual:
         *   "If FTMEN=0, this write coherency mechanism may be manually reset by
         *    writing to the CnSC register whether BDM mode is active or not".
         * 
         * For us this means that after writing to CnV, the write will effectively
         * be nullified if we write to CnSC too soon after that!
         * 
         * To avoid this issue, we do the following:
         * - Here in setFirst() we write to CnV only after configuring CnSC.
         * - Wherever we write to CnV we also store to memory the CnV value
         *   we are writing.
         * - In irq_handler, when we write to CnSC to clear the CHF flag, we
         *   then write the stored CnV value to CnV, since we may just have
         *   cleared the CnV write buffer before a pending CnV update has completed.
         */
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            *Channel::csc(); // read so the next write clears the CHF flag
            *Channel::csc() = FTM_CSC_MSA | FTM_CSC_CHIE;
            
            TimeType now = Clock::getTime(lock_c);
            now -= time;
            now += clearance;
            if (now < UINT32_C(0x80000000)) {
                time += now;
            }
            o->m_cv = TheMyFtm::TheModeHelper::make_target_time(time);
            *Channel::cv() = o->m_cv;
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
            o->m_cv = TheMyFtm::TheModeHelper::make_target_time(time);
            *Channel::cv() = o->m_cv;
        }
    }
    
    template <typename ThisContext>
    static void unset (ThisContext c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        *Channel::csc() = FTM_CSC_MSA;
        
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
    
    static void irq_handler (InterruptContext<Context> c)
    {
        auto *o = Object::self(c);
        
        uint32_t csc;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            csc = *Channel::csc();
            if ((csc & FTM_CSC_CHF)) {
                *Channel::csc() = (csc & ~FTM_CSC_CHF);
                *Channel::cv() = o->m_cv; // See note in setFirst().
            }
        }
        
        if (!(csc & FTM_CSC_CHIE)) {
            return;
        }
        
        AMBRO_ASSERT(o->m_running)
        
        if ((TimeType)(Clock::getTime(c) - o->m_time) < UINT32_C(0x80000000)) {
            if (!Handler::call(c)) {
#ifdef AMBROLIB_ASSERTIONS
                o->m_running = false;
#endif
                *Channel::csc() = FTM_CSC_MSA;
            }
        }
    }
    
private:
    static const TimeType clearance = MaxValue<TimeType>((64 / Clock::prescale_divide) + 2, ExtraClearance::value() * Clock::time_freq);
    
public:
    struct Object : public ObjBase<Mk20ClockInterruptTimer, ParentObject, MakeTypeList<TheDebugObject>> {
        TimeType m_time;
        uint16_t m_cv;
#ifdef AMBROLIB_ASSERTIONS
        bool m_running;
#endif
    };
};

APRINTER_ALIAS_STRUCT_EXT(Mk20ClockInterruptTimerService, (
    APRINTER_AS_TYPE(Ftm),
    APRINTER_AS_VALUE(int, ChannelIndex),
    APRINTER_AS_TYPE(ExtraClearance)
), (
    APRINTER_ALIAS_STRUCT_EXT(InterruptTimer, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(Handler)
    ), (
        using Params = Mk20ClockInterruptTimerService;
        APRINTER_DEF_INSTANCE(InterruptTimer, Mk20ClockInterruptTimer)
    ))
))

#define AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(ftm, channel_index, timer, context) \
static_assert( \
    TypesAreEqual<timer::Ftm, ftm>::Value && \
    timer::ChannelIndex == channel_index, \
    "Incorrect INTERRUPT_TIMER_GLOBAL macro used" \
); \
template <> \
struct Mk20Clock__IrqCompHelper<ftm, channel_index> { \
    static void call () \
    { \
        timer::irq_handler(MakeInterruptContext((context))); \
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
    using ChannelPin = TypeListGetMapped<typename Channel::PinsList, GetMemberType_Pin, Pin>;
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    using DutyCycleType = ChooseIntForMax<TheMyFtm::TheModeHelper::TopVal + 1, false>;
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
