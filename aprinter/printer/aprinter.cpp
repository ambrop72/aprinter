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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#define AMBROLIB_ABORT_ACTION { cli(); while (1); }

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/system/AvrEventLoop.h>
#include <aprinter/system/AvrClock.h>
#include <aprinter/system/AvrPins.h>
#include <aprinter/system/AvrPinWatcher.h>
#include <aprinter/system/AvrSerial.h>
#include <aprinter/system/AvrLock.h>
#include <aprinter/stepper/Steppers.h>
#include <aprinter/stepper/AxisStepper.h>
#include <aprinter/stepper/AxisSplitter.h>
#include <aprinter/stepper/AxisSharer.h>
#include <aprinter/stepper/AxisHomer.h>

#define CLOCK_TIMER_PRESCALER 2
#define LED1_PIN AvrPin<AvrPortA, 4>
#define X_DIR_PIN AvrPin<AvrPortC, 5>
#define X_STEP_PIN AvrPin<AvrPortD, 7>
#define X_STOP_PIN AvrPin<AvrPortC, 2>
#define Y_DIR_PIN AvrPin<AvrPortC, 7>
#define Y_STEP_PIN AvrPin<AvrPortC, 6>
#define Y_STOP_PIN AvrPin<AvrPortC, 3>
#define XYE_ENABLE_PIN AvrPin<AvrPortD, 6>
#define Z_DIR_PIN AvrPin<AvrPortB, 2>
#define Z_STEP_PIN AvrPin<AvrPortB, 3>
#define Z_ENABLE_PIN AvrPin<AvrPortA, 5>
#define Z_STOP_PIN AvrPin<AvrPortC, 4>
#define BLINK_INTERVAL .051
#define SERIAL_BAUD 115200
#define SERIAL_RX_BUFFER 63
#define SERIAL_TX_BUFFER 63
#define STEPPER_COMMAND_BUFFER_BITS 4
#define SPEED_T_SCALE (0.092*2.0)
#define X_SCALE 1.2
#define Y_SCALE 1.0
#define STEPPERS \
    MakeTypeList< \
        StepperDef<X_DIR_PIN, X_STEP_PIN, XYE_ENABLE_PIN>, \
        StepperDef<Y_DIR_PIN, Y_STEP_PIN, XYE_ENABLE_PIN> \
    >

using namespace APrinter;

struct MyContext;
struct EventLoopParams;
struct PinWatcherHandler0;
struct PinWatcherHandler1;
struct PinWatcherHandler2;
struct SerialRecvHandler;
struct SerialSendHandler;
struct DriverGetStepperHandler0;
struct DriverGetStepperHandler1;
struct HomerGetSharerHandler0;
struct HomerFinishedHandler0;

typedef DebugObjectGroup<MyContext> MyDebugObjectGroup;
typedef AvrClock<MyContext, CLOCK_TIMER_PRESCALER> MyClock;
typedef AvrEventLoop<EventLoopParams> MyLoop;
typedef AvrPins<MyContext> MyPins;
typedef AvrPinWatcherService<MyContext> MyPinWatcherService;
typedef AvrEventLoopQueuedEvent<MyLoop> MyTimer;
typedef AvrPinWatcher<MyContext, X_STOP_PIN, PinWatcherHandler0> MyPinWatcher0;
typedef AvrPinWatcher<MyContext, Y_STOP_PIN, PinWatcherHandler1> MyPinWatcher1;
typedef AvrPinWatcher<MyContext, Z_STOP_PIN, PinWatcherHandler2> MyPinWatcher2;
typedef AvrSerial<MyContext, uint8_t, SERIAL_RX_BUFFER, SerialRecvHandler, uint8_t, SERIAL_TX_BUFFER, SerialSendHandler> MySerial;
typedef Steppers<MyContext, STEPPERS> MySteppers;
typedef SteppersStepper<MyContext, STEPPERS, 0> MySteppersStepper0;
typedef SteppersStepper<MyContext, STEPPERS, 1> MySteppersStepper1;
using StepperParams0 = AxisStepperParams<STEPPER_COMMAND_BUFFER_BITS, AvrClockInterruptTimer_TC1_OCA>;
using StepperParams1 = AxisStepperParams<STEPPER_COMMAND_BUFFER_BITS, AvrClockInterruptTimer_TC1_OCB>;
typedef AxisSharer<MyContext, StepperParams0, MySteppersStepper0, DriverGetStepperHandler0> MyAxisSharer0;
typedef AxisSharer<MyContext, StepperParams1, MySteppersStepper1, DriverGetStepperHandler1> MyAxisSharer1;
typedef AxisSharerUser<MyContext, StepperParams0, MySteppersStepper0, DriverGetStepperHandler0> MyAxisUser0;
typedef AxisSharerUser<MyContext, StepperParams1, MySteppersStepper1, DriverGetStepperHandler1> MyAxisUser1;
using AbsVelFixedType = FixedPoint<15, false, -15-4>;
using AbsAccFixedType = FixedPoint<15, false, -15-24>;
typedef AxisHomer<MyContext, MyAxisSharer0, AbsVelFixedType, AbsAccFixedType, X_STOP_PIN, false, true, HomerFinishedHandler0> MyHomer0;

struct MyContext {
    typedef MyDebugObjectGroup DebugGroup;
    typedef AvrLock<MyContext> Lock;
    typedef MyClock Clock;
    typedef MyLoop EventLoop;
    typedef MyPins Pins;
    typedef MyPinWatcherService PinWatcherService;
    
    MyDebugObjectGroup * debugGroup () const;
    MyClock * clock () const;
    MyLoop * eventLoop () const;
    MyPins * pins () const;
    MyPinWatcherService * pinWatcherService () const;
};

struct EventLoopParams {
    typedef MyContext Context;
};

static MyDebugObjectGroup d_group;
static MyClock myclock;
static MyLoop myloop;
static MyPins mypins;
static MyPinWatcherService mypinwatcherservice;
static MyTimer mytimer;
static MyPinWatcher0 mypinwatcher0;
static MyPinWatcher1 mypinwatcher1;
static MyPinWatcher2 mypinwatcher2;
static bool pin_prev0;
static bool pin_prev1;
static bool pin_prev2;
static MySerial myserial;
static bool blink_state;
static MyClock::TimeType next_time;
static MySteppers steppers;
static MyAxisSharer0 axis_sharer0;
static MyAxisSharer1 axis_sharer1;
static MyAxisUser0 axis_user0;
static MyAxisUser1 axis_user1;
static MyHomer0 homer0;
static int index0;
static int index1;
static int cnt0;
static int cnt1;
static int eof;
static int full;
static int empty;
static bool active;
static bool stepping;
static bool paused;
static MyClock::TimeType pause_time;

MyDebugObjectGroup * MyContext::debugGroup () const
{
    return &d_group;
}

MyClock * MyContext::clock () const
{
    return &myclock;
}

MyLoop * MyContext::eventLoop () const
{
    return &myloop;
}

MyPins * MyContext::pins () const
{
    return &mypins;
}

MyPinWatcherService * MyContext::pinWatcherService () const
{
    return &mypinwatcherservice;
}

AMBRO_AVR_CLOCK_ISRS(myclock, MyContext())
AMBRO_AVR_PIN_WATCHER_ISRS(mypinwatcherservice, MyContext())
AMBRO_AVR_SERIAL_ISRS(myserial, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC1_OCA_ISRS(*axis_sharer0.getTimer(), MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC1_OCB_ISRS(*axis_sharer1.getTimer(), MyContext())

static void mytimer_handler (MyTimer *, MyContext c)
{
    blink_state = !blink_state;
    mypins.set<LED1_PIN>(c, blink_state);
    next_time += (MyClock::TimeType)(BLINK_INTERVAL / MyClock::time_unit);
    mytimer.appendAt(c, next_time);
}

static void full_common (MyContext c, int inc)
{
    AMBRO_ASSERT(active)
    AMBRO_ASSERT(!stepping)
    
    full += inc;
    if (full == 2 && !paused) {
        MyClock::TimeType pause_duration = myclock.getTime(c) - pause_time;
        axis_user0.getAxis(c)->addTime(c, pause_duration);
        axis_user1.getAxis(c)->addTime(c, pause_duration);
        axis_user0.getAxis(c)->startStepping(c);
        axis_user1.getAxis(c)->startStepping(c);
        empty = 0;
        stepping = true;
    }
}

static void empty_common (MyContext c, int inc)
{
    AMBRO_ASSERT(active)
    
    empty += inc;
    if (empty == 2) {
        axis_user0.getAxis(c)->stop(c);
        axis_user1.getAxis(c)->stop(c);
        axis_user0.deactivate(c);
        axis_user1.deactivate(c);
        steppers.getStepper<0>()->enable(c, false);
        steppers.getStepper<1>()->enable(c, false);
        active = false;
        stepping = false;
    }
}

static void endstop_common (MyContext c)
{
    bool new_paused = (mypins.get<X_STOP_PIN>(c) || mypins.get<Y_STOP_PIN>(c));
    if (new_paused == paused) {
        return;
    }
    
    if (!paused) {
        paused = true;
        if (active && stepping) {
            pause_time = myclock.getTime(c);
            axis_user0.getAxis(c)->stopStepping(c);
            axis_user1.getAxis(c)->stopStepping(c);
            full = eof;
            stepping = false;
        }
    } else {
        paused = false;
        if (active) {
            full_common(c, 0);
        }
    }
}

static void finished_homing (MyContext c)
{
    steppers.getStepper<0>()->enable(c, false);
    mypinwatcher0.init(c);
    mypinwatcher1.init(c);
}

static void pinwatcher_handler0 (MyPinWatcher0 *, MyContext c, bool state)
{
    endstop_common(c);
}

static void pinwatcher_handler1 (MyPinWatcher1 *, MyContext c, bool state)
{
    endstop_common(c);
}

static void pinwatcher_handler2 (MyPinWatcher2 *, MyContext c, bool state)
{
    bool press = !pin_prev2 && state;
    pin_prev2 = state;
    if (!press) {
        return;
    }
    
    if (active) {
        empty_common(c, 2 - empty);
    } else {
        if (homer0.isRunning(c)) {
            homer0.stop(c);
            finished_homing(c);
        }
        
        AMBRO_ASSERT(!active)
        AMBRO_ASSERT(!stepping)
        active = true;
        index0 = 0;
        index1 = 0;
        cnt0 = 0;
        cnt1 = 0;
        eof = 0;
        full = 0;
        empty = 0;
        pause_time = myclock.getTime(c);
        steppers.getStepper<0>()->enable(c, true);
        steppers.getStepper<1>()->enable(c, true);
        axis_user0.activate(c);
        axis_user1.activate(c);
        axis_user0.getAxis(c)->start(c, pause_time);
        axis_user1.getAxis(c)->start(c, pause_time);
    }
}

static void serial_recv_handler (MySerial *, MyContext c)
{
}

static void serial_send_handler (MySerial *, MyContext c)
{
}

static MySteppersStepper0 * driver_get_stepper_handler0 (MyAxisSharer0 *) 
{
    return steppers.getStepper<0>();
}

static MySteppersStepper1* driver_get_stepper_handler1 (MyAxisSharer1 *)
{
    return steppers.getStepper<1>();
}

static void pull_cmd_handler0 (MyAxisUser0 *, MyContext c)
{
    AMBRO_ASSERT(active)
    
    if (cnt0 == 7 * 6) {
        eof++;
        if (!stepping) {
            full_common(c, 1);
        }
        return;
    }
    float t_scale = SPEED_T_SCALE * X_SCALE;
    switch (index0) {
        case 0:
            axis_user0.getAxis(c)->commandDoneTest(c, true, X_SCALE * 20.0, 1.0 * t_scale, X_SCALE * 20.0);
            break;
        case 1:
            axis_user0.getAxis(c)->commandDoneTest(c, true, X_SCALE * 120.0, 3.0 * t_scale, X_SCALE * 0.0);
            break;
        case 2:
            axis_user0.getAxis(c)->commandDoneTest(c, true, X_SCALE * 20.0, 1.0 * t_scale, X_SCALE * -20.0);
            break;
        case 3:
            axis_user0.getAxis(c)->commandDoneTest(c, false, X_SCALE * 20.0, 1.0 * t_scale, X_SCALE * 20.0);
            break;
        case 4:
            axis_user0.getAxis(c)->commandDoneTest(c, false, X_SCALE * 120.0, 3.0 * t_scale, X_SCALE * 0.0);
            break;
        case 5:
            axis_user0.getAxis(c)->commandDoneTest(c, false, X_SCALE * 20.0, 1.0 * t_scale, X_SCALE * -20.0);
            break;
    }
    index0 = (index0 + 1) % 6;
    cnt0++;
}

static void pull_cmd_handler1 (MyAxisUser1 *, MyContext c)
{
    AMBRO_ASSERT(active)
    
    if (cnt1 == 6 * 8) {
        eof++;
        if (!stepping) {
            full_common(c, 1);
        }
        return;
    }
    float t_scale = SPEED_T_SCALE * Y_SCALE;
    switch (index1) {
        case 0:
            axis_user1.getAxis(c)->commandDoneTest(c, true, Y_SCALE * 20.0, 1.0 * t_scale, Y_SCALE * 20.0);
            break;
        case 1:
            axis_user1.getAxis(c)->commandDoneTest(c, true, Y_SCALE * 120.0, 3.0 * t_scale, Y_SCALE * 0.0);
            break;
        case 2:
            axis_user1.getAxis(c)->commandDoneTest(c, true, Y_SCALE * 20.0, 1.0 * t_scale, Y_SCALE * -20.0);
            break;
        case 3:
            axis_user1.getAxis(c)->commandDoneTest(c, true, Y_SCALE * 0.0, 2.0 * t_scale, Y_SCALE * 0.0);
            break;
        case 4:
            axis_user1.getAxis(c)->commandDoneTest(c, false, Y_SCALE * 20.0, 1.0 * t_scale, Y_SCALE * 20.0);
            break;
        case 5:
            axis_user1.getAxis(c)->commandDoneTest(c, false, Y_SCALE * 120.0, 3.0 * t_scale, Y_SCALE * 0.0);
            break;
        case 6:
            axis_user1.getAxis(c)->commandDoneTest(c, false, Y_SCALE * 20.0, 1.0 * t_scale, Y_SCALE * -20.0);
            break;
        case 7:
            axis_user1.getAxis(c)->commandDoneTest(c, false, Y_SCALE * 0.0, 2.0 * t_scale, Y_SCALE * 0.0);
            break;
    }
    index1 = (index1 + 1) % 8;
    cnt1++;
}

static void buffer_full_handler0 (MyAxisUser0 *, MyContext c)
{
    full_common(c, 1);
}

static void buffer_full_handler1 (MyAxisUser1 *, MyContext c)
{
    full_common(c, 1);
}

static void buffer_empty_handler0 (MyAxisUser0 *, MyContext c)
{
    empty_common(c, 1);
}

static void buffer_empty_handler1 (MyAxisUser1 *, MyContext c)
{
    empty_common(c, 1);
}

static void homer_finisher_handler0 (MyHomer0 *, MyContext c, bool success)
{
    printf("homing: %d\n", (int)success);
    finished_homing(c);
}

struct PinWatcherHandler0 : public AMBRO_WFUNC(pinwatcher_handler0) {};
struct PinWatcherHandler1 : public AMBRO_WFUNC(pinwatcher_handler1) {};
struct PinWatcherHandler2 : public AMBRO_WFUNC(pinwatcher_handler2) {};
struct SerialRecvHandler : public AMBRO_WFUNC(serial_recv_handler) {};
struct SerialSendHandler : public AMBRO_WFUNC(serial_send_handler) {};
struct DriverGetStepperHandler0 : public AMBRO_WFUNC(driver_get_stepper_handler0) {};
struct DriverGetStepperHandler1 : public AMBRO_WFUNC(driver_get_stepper_handler1) {};
struct HomerFinishedHandler0 : public AMBRO_WFUNC(homer_finisher_handler0) {};

FILE uart_output;

static int uart_putchar (char ch, FILE *stream)
{
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = ch;
    return 1;
}

static void setup_uart_stdio ()
{
    uart_output.put = uart_putchar;
    uart_output.flags = _FDEV_SETUP_WRITE;
    stdout = &uart_output;
    stderr = &uart_output;
}

int main ()
{
    MyContext c;
    
    d_group.init(c);
    myclock.init(c);
#ifdef TCNT3
    myclock.initTC3(c);
#endif
    myloop.init(c);
    mypins.init(c);
    mypins.setInput<X_STOP_PIN>(c);
    mypins.setInput<Z_STOP_PIN>(c);
    mypins.setOutput<LED1_PIN>(c);
    mypinwatcherservice.init(c);
    mytimer.init(c, mytimer_handler);
    mypinwatcher2.init(c);
    myserial.init(c, SERIAL_BAUD);
    setup_uart_stdio();
    printf("HELLO\n");
    steppers.init(c);
    axis_sharer0.init(c);
    axis_sharer1.init(c);
    axis_user0.init(c, &axis_sharer0, pull_cmd_handler0, buffer_full_handler0, buffer_empty_handler0);
    axis_user1.init(c, &axis_sharer1, pull_cmd_handler1, buffer_full_handler1, buffer_empty_handler1);
    homer0.init(c, &axis_sharer0);
    
    MyClock::TimeType ref_time = myclock.getTime(c);
    
    pin_prev0 = false;
    pin_prev1 = false;
    pin_prev2 = false;
    blink_state = false;
    active = false;
    stepping = false;
    paused = false;
    next_time = myclock.getTime(c) + (uint32_t)(BLINK_INTERVAL / MyClock::time_unit);
    mytimer.appendAt(c, next_time);
    
    float unit_mm = 1.0 / 0.0125;
    float unit_sec = 20000000.0 / 8.0;
    
    typename MyHomer0::HomingParams params;
    params.max_accel = AbsAccFixedType::importDouble(500.0 * (unit_mm / (unit_sec * unit_sec)));
    params.fast_max_dist = MyHomer0::StepFixedType::importDouble(300.0 * unit_mm);
    params.retract_dist = MyHomer0::StepFixedType::importDouble(3.0 * unit_mm);
    params.slow_max_dist = MyHomer0::StepFixedType::importDouble(5.0 * unit_mm);
    params.fast_speed = AbsVelFixedType::importDouble(40.0 * (unit_mm / unit_sec));
    params.retract_speed = AbsVelFixedType::importDouble(50.0 * (unit_mm / unit_sec));
    params.slow_speed = AbsVelFixedType::importDouble(5.0 * (unit_mm / unit_sec));
    
    steppers.getStepper<0>()->enable(c, true);
    homer0.start(c, params);
    
    myloop.run(c);
}
