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

#define AMBROLIB_ABORT_ACTION { sei(); while (1); }

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/system/AvrEventLoop.h>
#include <aprinter/system/AvrClock.h>
#include <aprinter/system/AvrPins.h>
#include <aprinter/system/AvrPinWatcher.h>
#include <aprinter/system/AvrSerial.h>
#include <aprinter/system/AvrLock.h>
#include <aprinter/stepper/Steppers.h>
#include <aprinter/stepper/AxisStepper.h>
#include <aprinter/stepper/AxisSplitter.h>

#define CLOCK_TIMER_PRESCALER 2
#define LED1_PIN AvrPin<AvrPortA, 4>
#define LED2_PIN AvrPin<AvrPortA, 3>
#define WATCH_PIN AvrPin<AvrPortC, 2>
#define X_DIR_PIN AvrPin<AvrPortC, 5>
#define X_STEP_PIN AvrPin<AvrPortD, 7>
#define Y_DIR_PIN AvrPin<AvrPortC, 7>
#define Y_STEP_PIN AvrPin<AvrPortC, 6>
#define XYE_ENABLE_PIN AvrPin<AvrPortD, 6>
#define Z_DIR_PIN AvrPin<AvrPortB, 2>
#define Z_STEP_PIN AvrPin<AvrPortB, 3>
#define Z_ENABLE_PIN AvrPin<AvrPortA, 5>
#define BLINK_INTERVAL .051
#define SERIAL_BAUD 115200
#define SERIAL_RX_BUFFER 63
#define SERIAL_TX_BUFFER 63
#define COMMAND_BUFFER_BITS 4
#define STEPPER_COMMAND_BUFFER_BITS 4
#define NUM_MOVE_ITERS 4
#define SPEED_T_SCALE (0.100*2.0)
#define X_SCALE 1.0
#define Y_SCALE 1.0
#define STEPPERS \
    MakeTypeList< \
        StepperDef<X_DIR_PIN, X_STEP_PIN, XYE_ENABLE_PIN>, \
        StepperDef<Y_DIR_PIN, Y_STEP_PIN, XYE_ENABLE_PIN> \
    >::Type

using namespace APrinter;

struct MyContext;
struct EventLoopParams;
struct PinWatcherHandler;
struct SerialRecvHandler;
struct SerialSendHandler;
struct DriverGetStepperHandler0;
struct DriverGetStepperHandler1;
struct DriverAvailHandler0;
struct DriverAvailHandler1;

typedef DebugObjectGroup<MyContext> MyDebugObjectGroup;
typedef AvrClock<MyContext, CLOCK_TIMER_PRESCALER> MyClock;
typedef AvrEventLoop<EventLoopParams> MyLoop;
typedef AvrPins<MyContext> MyPins;
typedef AvrPinWatcherService<MyContext> MyPinWatcherService;
typedef AvrEventLoopQueuedEvent<MyLoop> MyTimer;
typedef AvrPinWatcher<MyContext, WATCH_PIN, PinWatcherHandler> MyPinWatcher;
typedef AvrSerial<MyContext, uint8_t, SERIAL_RX_BUFFER, SerialRecvHandler, uint8_t, SERIAL_TX_BUFFER, SerialSendHandler> MySerial;
typedef Steppers<MyContext, STEPPERS> MySteppers;
typedef SteppersStepper<MyContext, STEPPERS, 0> MySteppersStepper0;
typedef SteppersStepper<MyContext, STEPPERS, 1> MySteppersStepper1;
typedef AxisSplitter<MyContext, COMMAND_BUFFER_BITS, STEPPER_COMMAND_BUFFER_BITS, MySteppersStepper0, DriverGetStepperHandler0, AvrClockInterruptTimer_TC1_OCA, DriverAvailHandler0> MyAxisSplitter0;
typedef AxisSplitter<MyContext, COMMAND_BUFFER_BITS, STEPPER_COMMAND_BUFFER_BITS, MySteppersStepper1, DriverGetStepperHandler1, AvrClockInterruptTimer_TC1_OCB, DriverAvailHandler1> MyAxisSplitter1;

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
static MyPinWatcher mypinwatcher;
static MySerial myserial;
static bool blink_state;
static MyClock::TimeType next_time;
static MySteppers steppers;
static MyAxisSplitter0 axis_splitter0;
static MyAxisSplitter1 axis_splitter1;
static int num_left0;
static int num_left1;
static bool prev_button;

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
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC1_OCA_ISRS(*axis_splitter0.getAxisStepper()->getTimer(), MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC1_OCB_ISRS(*axis_splitter1.getAxisStepper()->getTimer(), MyContext())

static void add_commands0 (MyContext c)
{
    static const int num_cmds = 6;
    static_assert(PowerOfTwoMinusOne<size_t, COMMAND_BUFFER_BITS>::value >= num_cmds, "");
    float t_scale = SPEED_T_SCALE;
    axis_splitter0.bufferAddCommandTest(c, true, X_SCALE * 20.0, 1.0 * t_scale, X_SCALE * 20.0);
    axis_splitter0.bufferAddCommandTest(c, true, X_SCALE * 120.0, 3.0 * t_scale, X_SCALE * 0.0);
    axis_splitter0.bufferAddCommandTest(c, true, X_SCALE * 20.0, 1.0 * t_scale, X_SCALE * -20.0);
    //axis_splitter0.bufferAddCommandTest(c, true, 0.0, 2.0 * t_scale, 0.0);
    axis_splitter0.bufferAddCommandTest(c, false, X_SCALE * 20.0, 1.0 * t_scale, X_SCALE * 20.0);
    axis_splitter0.bufferAddCommandTest(c, false, X_SCALE * 120.0, 3.0 * t_scale, X_SCALE * 0.0);
    axis_splitter0.bufferAddCommandTest(c, false, X_SCALE * 20.0, 1.0 * t_scale, X_SCALE * -20.0);
    //axis_splitter0.bufferAddCommandTest(c, false, 0.0, 2.0 * t_scale, 0.0);
    //num_left0--;
    axis_splitter0.bufferRequestEvent(c, (num_left0 == 0) ? MyAxisSplitter0::BufferSizeType::maxValue() : MyAxisSplitter0::BufferSizeType::import(num_cmds));
}

static void add_commands1 (MyContext c)
{
    static const int num_cmds = 8;
    static_assert(PowerOfTwoMinusOne<size_t, COMMAND_BUFFER_BITS>::value >= num_cmds, "");
    float t_scale = SPEED_T_SCALE;
    axis_splitter1.bufferAddCommandTest(c, true, Y_SCALE * 20.0, 1.0 * t_scale, Y_SCALE * 20.0);
    axis_splitter1.bufferAddCommandTest(c, true, Y_SCALE * 120.0, 3.0 * t_scale, Y_SCALE * 0.0);
    axis_splitter1.bufferAddCommandTest(c, true, Y_SCALE * 20.0, 1.0 * t_scale, Y_SCALE * -20.0);
    axis_splitter1.bufferAddCommandTest(c, true, 0.0, 2.0 * t_scale, 0.0);
    axis_splitter1.bufferAddCommandTest(c, false, Y_SCALE * 20.0, 1.0 * t_scale, Y_SCALE * 20.0);
    axis_splitter1.bufferAddCommandTest(c, false, Y_SCALE * 120.0, 3.0 * t_scale, Y_SCALE * 0.0);
    axis_splitter1.bufferAddCommandTest(c, false, Y_SCALE * 20.0, 1.0 * t_scale, Y_SCALE * -20.0);
    axis_splitter1.bufferAddCommandTest(c, true, 0.0, 2.0 * t_scale, 0.0);
    //num_left1--;
    axis_splitter1.bufferRequestEvent(c, (num_left1 == 0) ? MyAxisSplitter1::BufferSizeType::maxValue() : MyAxisSplitter1::BufferSizeType::import(num_cmds));
}

static void mytimer_handler (MyTimer *, MyContext c)
{
    blink_state = !blink_state;
    mypins.set<LED1_PIN>(c, blink_state);
    next_time += (MyClock::TimeType)(BLINK_INTERVAL / MyClock::time_unit);
    mytimer.appendAt(c, next_time);
}

static void pinwatcher_handler (MyPinWatcher *, MyContext c, bool state)
{
    mypins.set<LED2_PIN>(c, !state);
    if (!prev_button && state) {
        if (axis_splitter0.isRunning(c) || axis_splitter1.isRunning(c)) {
            if (axis_splitter0.isRunning(c)) {
                axis_splitter0.stop(c);
                axis_splitter0.bufferCancelEvent(c);
                axis_splitter0.clearBuffer(c);
                steppers.getStepper<0>()->enable(c, false);
            }
            if (axis_splitter1.isRunning(c)) {
                axis_splitter1.stop(c);
                axis_splitter1.bufferCancelEvent(c);
                axis_splitter1.clearBuffer(c);
                steppers.getStepper<1>()->enable(c, false);
            }
        } else {
            num_left0 = NUM_MOVE_ITERS;
            num_left1 = NUM_MOVE_ITERS;
            add_commands0(c);
            add_commands1(c);
            MyClock::TimeType start_time = myclock.getTime(c);
            steppers.getStepper<0>()->enable(c, true);
            steppers.getStepper<1>()->enable(c, true);
            axis_splitter0.start(c, start_time);
            axis_splitter1.start(c, start_time);
        }
    }
    prev_button = state;
}

static void serial_recv_handler (MySerial *, MyContext c)
{
}

static void serial_send_handler (MySerial *, MyContext c)
{
}

static MySteppersStepper0 * driver_get_stepper_handler0 (MyAxisSplitter0 *) 
{
    return steppers.getStepper<0>();
}

static MySteppersStepper1* driver_get_stepper_handler1 (MyAxisSplitter1 *)
{
    return steppers.getStepper<1>();
}

static void driver_avail_handler0 (MyAxisSplitter0 *, MyContext c)
{
    AMBRO_ASSERT(axis_splitter0.isRunning(c))
    
    if (num_left0 == 0) {
        axis_splitter0.stop(c);
        steppers.getStepper<0>()->enable(c, false);
    } else {
        add_commands0(c);
    }
}

static void driver_avail_handler1 (MyAxisSplitter1 *, MyContext c)
{
    AMBRO_ASSERT(axis_splitter1.isRunning(c))
    
    if (num_left1 == 0) {
        axis_splitter1.stop(c);
        steppers.getStepper<1>()->enable(c, false);
    } else {
        add_commands1(c);
    }
}

struct PinWatcherHandler : public AMBRO_WFUNC(pinwatcher_handler) {};
struct SerialRecvHandler : public AMBRO_WFUNC(serial_recv_handler) {};
struct SerialSendHandler : public AMBRO_WFUNC(serial_send_handler) {};
struct DriverGetStepperHandler0 : public AMBRO_WFUNC(driver_get_stepper_handler0) {};
struct DriverGetStepperHandler1 : public AMBRO_WFUNC(driver_get_stepper_handler1) {};
struct DriverAvailHandler0 : public AMBRO_WFUNC(driver_avail_handler0) {};
struct DriverAvailHandler1 : public AMBRO_WFUNC(driver_avail_handler1) {};

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
    mypinwatcherservice.init(c);
    mytimer.init(c, mytimer_handler);
    mypinwatcher.init(c);
    myserial.init(c, SERIAL_BAUD);
    setup_uart_stdio();
    printf("HELLO\n");
    steppers.init(c);
    axis_splitter0.init(c);
    axis_splitter1.init(c);
    
    mypins.setOutput<LED1_PIN>(c);
    mypins.setOutput<LED2_PIN>(c);
    mypins.setInput<WATCH_PIN>(c);
    
    MyClock::TimeType ref_time = myclock.getTime(c);
    
    blink_state = false;
    next_time = myclock.getTime(c) + (uint32_t)(BLINK_INTERVAL / MyClock::time_unit);
    mytimer.appendAt(c, next_time);
    prev_button = false;
    
    /*
    uint32_t x = 0;
    do {
        uint16_t my = IntSqrt<29>::call(x);
        if (!((uint32_t)my * my <= x && ((uint32_t)my + 1) * ((uint32_t)my + 1) > x)) {
            printf("%" PRIu32 " BAD my=%" PRIu16 "\n", x, my);
        }
        x++;
    } while (x < ((uint32_t)1 << 29));
    */
    /*
    for (uint32_t i = 0; i < UINT32_C(1000000); i++) {
        uint32_t x;
        *((uint8_t *)&x + 0) = rand();
        *((uint8_t *)&x + 1) = rand();
        *((uint8_t *)&x + 2) = rand();
        *((uint8_t *)&x + 3) = rand() & 0x1F;
        uint16_t my = IntSqrt<29>::call(x);
        if (!((uint32_t)my * my <= x && ((uint32_t)my + 1) * ((uint32_t)my + 1) > x)) {
            printf("%" PRIu32 " BAD my=%" PRIu16 "\n", x, my);
        }
    }
    */
    myloop.run(c);
}
