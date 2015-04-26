#include <stdint.h>
#include <stdio.h>

#include <aprinter/platform/stm32f4/stm32f4_support.h>
#include <usbd_core.h>
#include <aprinter/platform/stm32f4/usbd_desc.h>
#include <aprinter/platform/stm32f4/usbd_instance.h>
#include <aprinter/platform/stm32f4/usbd_cdc_interface.h>

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
#include <aprinter/system/Stm32f4Watchdog.h>

#if defined(STM32F429xx)
#include <aprinter/board/stm32f429i_discovery_pins.h>
#elif defined(STM32F407xx)
#include <aprinter/board/stm32f4discovery_pins.h>
#else
#error not supported
#endif

using namespace APrinter;

struct MyContext;
struct MyLoopExtraDelay;
struct Program;

using MyDebugObjectGroup = DebugObjectGroup<MyContext, Program>;
using MyClock = Stm32f4Clock<
    MyContext,
    Program,
    34,
    MakeTypeList<
        Stm32f4ClockTIM2
    >
>;
using MyLoop = BusyEventLoop<MyContext, Program, MyLoopExtraDelay>;
using MyPins = Stm32f4Pins<MyContext, Program>;
using MyWatchdog = Stm32f4WatchdogService<16, 0xFFF>::Watchdog<MyContext, Program>;

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
    MyLoopExtra,
    MyWatchdog
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

AMBRO_STM32F4_CLOCK_TC_GLOBAL(2, MyClock, MyContext())

int main ()
{
    // Basic initialization (STM HAL, clock).
    platform_init();
    
    // APrinter stuff initialization.
    
    MyContext c;
    
    MyDebugObjectGroup::init(c);
    MyClock::init(c);
    MyLoop::init(c);
    MyPins::init(c);
    MyWatchdog::init(c);
    
    // USB serial port initialization.
    
    if (USBD_Init(&USBD_Device, &VCP_Desc, 0) != USBD_OK) {
        while (1);
    }
    
    if (USBD_RegisterClass(&USBD_Device, USBD_CDC_CLASS) != USBD_OK) {
        while (1);
    }
    
    if (USBD_CDC_RegisterInterface(&USBD_Device, &USBD_CDC_fops) != USBD_OK) {
        while (1);
    }
    
    if (USBD_Start(&USBD_Device) != USBD_OK) {
        while (1);
    }
    
    // Go on bliking the LEDs.
    
    MyPins::setOutput<DiscoveryPinLedGreen>(c);
    MyPins::setOutput<DiscoveryPinLedRed>(c);
    
    bool state = false;
    MyClock::TimeType time = MyClock::getTime(c);
    
    while (1) {
        MyWatchdog::reset(c);
        
        state = !state;
        MyPins::set<DiscoveryPinLedGreen>(c, state);
        MyPins::set<DiscoveryPinLedRed>(c, !state);
        
        time += (MyClock::TimeType)(MyClock::time_freq * 0.5);
        while ((uint32_t)(time - MyClock::getTime(c)) < UINT32_C(0x80000000));
    }
    
    //MyLoop::run(c);
}
