#include <stdint.h>
#include <stdio.h>

#include <aprinter/platform/stm32f4/stm32f4_support.h>

#include <stm32f4xx_hal_iwdg.h>

static void emergency (void);

#define AMBROLIB_EMERGENCY_ACTION { cli(); emergency(); }
#define AMBROLIB_ABORT_ACTION { while (1); }

#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/WrapDouble.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/system/BusyEventLoop.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/system/Stm32f4Clock.h>
#include <aprinter/system/Stm32f4Pins.h>

#include <aprinter/board/stm32f429i_discovery_pins.h>

using namespace APrinter;

struct MyContext;
struct MyLoopExtraDelay;
struct Program;

using MyDebugObjectGroup = DebugObjectGroup<MyContext, Program>;
using MyClock = Stm32f4Clock<
    MyContext,
    Program,
    31,
    MakeTypeList<
        Stm32f4ClockTIM2
    >
>;
using MyLoop = BusyEventLoop<MyContext, Program, MyLoopExtraDelay>;
using MyPins = Stm32f4Pins<MyContext, Program>;

struct MyContext {
    using DebugGroup = MyDebugObjectGroup;
    using Clock = MyClock;
    using EventLoop = MyLoop;

    using Pins = MyPins;

    void check () const;
};

using MyLoopExtra = BusyEventLoopExtra<Program, MyLoop, EmptyTypeList>;
struct MyLoopExtraDelay : public WrapType<MyLoopExtra> {};

struct Program : public ObjBase<void, void, MakeTypeList<
    MyDebugObjectGroup,
    MyClock,
    MyLoop,
    MyPins,
    MyLoopExtra
>> {
    static Program * self (MyContext c);
};

Program p;

Program * Program::self (MyContext c) { return &p; }
void MyContext::check () const {}

static void emergency (void)
{
}

extern "C" __attribute__((used)) void __cxa_pure_virtual(void)
{
    AMBRO_ASSERT_ABORT("pure virtual function call")
}

int main ()
{
    platform_init();
    
    MyContext c;
    
    MyDebugObjectGroup::init(c);
    MyClock::init(c);
    MyLoop::init(c);
    MyPins::init(c);
    
    MyPins::setOutput<DiscoveryPinLedGreen>(c);
    MyPins::setOutput<DiscoveryPinLedRed>(c);
    
    bool state = false;
    MyClock::TimeType time = MyClock::getTime(c);
    
    while (1) {
        state = !state;
        MyPins::set<DiscoveryPinLedGreen>(c, state);
        MyPins::set<DiscoveryPinLedRed>(c, !state);
        
        time += (MyClock::TimeType)(MyClock::time_freq * 0.5);
        while ((uint32_t)(time - MyClock::getTime(c)) < UINT32_C(0x80000000));
    }

    MyLoop::run(c);
}
