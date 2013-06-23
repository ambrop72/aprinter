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
#include <aprinter/devices/SoftPwm.h>
#include <aprinter/driver/Steppers.h>
#include <aprinter/driver/AxisStepper.h>
#include <aprinter/driver/AxisController.h>

#define CLOCK_TIMER_PRESCALER 2
#define LED1_PIN AvrPin<AvrPortA, 4>
#define LED2_PIN AvrPin<AvrPortA, 3>
#define WATCH_PIN AvrPin<AvrPortC, 2>
#define SERVO_PIN AvrPin<AvrPortA, 1>
#define SERVO2_PIN AvrPin<AvrPortD, 2>
#define X_DIR_PIN AvrPin<AvrPortC, 5>
#define X_STEP_PIN AvrPin<AvrPortD, 7>
#define Y_DIR_PIN AvrPin<AvrPortC, 7>
#define Y_STEP_PIN AvrPin<AvrPortC, 6>
#define XYE_ENABLE_PIN AvrPin<AvrPortD, 6>
#define Z_DIR_PIN AvrPin<AvrPortB, 2>
#define Z_STEP_PIN AvrPin<AvrPortB, 3>
#define Z_ENABLE_PIN AvrPin<AvrPortA, 5>
#define BLINK_INTERVAL .051
#define SERVO_PULSE_INTERVAL UINT32_C(20000)
#define SERVO_PULSE_MIN .00115
#define SERVO_PULSE_MAX .00185
#define SERIAL_BAUD 115200
#define SERIAL_RX_BUFFER 63
#define SERIAL_TX_BUFFER 63
#define SERIAL_GEN_LENGTH 3000
#define COMMAND_BUFFER_BITS 4
#define NUM_MOVE_ITERS 4
#define SPEED_T_SCALE (0.104*2.0)
#define INTERRUPT_TIMER_TIME 1.0
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
typedef SoftPwm<MyContext, SERVO_PIN, SERVO_PULSE_INTERVAL> MySoftPwm;
typedef SoftPwm<MyContext, SERVO2_PIN, SERVO_PULSE_INTERVAL> MySoftPwm2;
typedef AvrSerial<MyContext, uint8_t, SERIAL_RX_BUFFER, SerialRecvHandler, uint8_t, SERIAL_TX_BUFFER, SerialSendHandler> MySerial;
typedef Steppers<MyContext, STEPPERS> MySteppers;
typedef SteppersStepper<MyContext, STEPPERS, 0> MySteppersStepper0;
typedef SteppersStepper<MyContext, STEPPERS, 1> MySteppersStepper1;
typedef AxisStepper<MyContext, COMMAND_BUFFER_BITS, MySteppersStepper0, DriverGetStepperHandler0, AvrClockInterruptTimer_TC1_OCA, DriverAvailHandler0> MyAxisStepper0;
typedef AxisStepper<MyContext, COMMAND_BUFFER_BITS, MySteppersStepper1, DriverGetStepperHandler1, AvrClockInterruptTimer_TC1_OCB, DriverAvailHandler1> MyAxisStepper1;

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
static bool servo_mode;
static MySoftPwm mysoftpwm;
static MySoftPwm2 mysoftpwm2;
static MySerial myserial;
static uint32_t gen_rem;
static bool blink_state;
static MyClock::TimeType next_time;
static MySteppers steppers;
static MyAxisStepper0 axis_stepper0;
static MyAxisStepper1 axis_stepper1;
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
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC1_OCA_ISRS(*axis_stepper0.getTimer(), MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC1_OCB_ISRS(*axis_stepper1.getTimer(), MyContext())

static void write_to_serial (MyContext c, const char *str)
{
    size_t str_length = strlen(str);
    
    uint8_t rem_length = myserial.sendQuery(c);
    if (rem_length > str_length) {
        rem_length = str_length;
    }
    
    while (rem_length > 0) {
        char *data = myserial.sendGetChunkPtr(c);
        uint8_t length = myserial.sendGetChunkLen(c, rem_length);
        memcpy(data, str, length);
        str += length;
        myserial.sendProvide(c, length);
        rem_length -= length;
    }
}

static void update_servos (MyContext c)
{
    if (servo_mode) {
        mysoftpwm.setOnTime(c, SERVO_PULSE_MAX / MyClock::time_unit);
        mysoftpwm2.setOnTime(c, SERVO_PULSE_MAX / MyClock::time_unit);
    } else {
        mysoftpwm.setOnTime(c, SERVO_PULSE_MIN / MyClock::time_unit);
        mysoftpwm2.setOnTime(c, SERVO_PULSE_MIN / MyClock::time_unit);
    }
}

static void add_commands0 (MyContext c)
{
    float t_scale = SPEED_T_SCALE;
    axis_stepper0.bufferProvideTest(c, true, 20.0, 1.0 * t_scale, 20.0);
    axis_stepper0.bufferProvideTest(c, true, 40.0, 1.0 * t_scale, 0.0);
    axis_stepper0.bufferProvideTest(c, true, 20.0, 1.0 * t_scale, -20.0);
    axis_stepper0.bufferProvideTest(c, false, 20.0, 1.0 * t_scale, 20.0);
    axis_stepper0.bufferProvideTest(c, false, 40.0, 1.0 * t_scale, 0.0);
    axis_stepper0.bufferProvideTest(c, false, 20.0, 1.0 * t_scale, -20.0);
    num_left0--;
    axis_stepper0.bufferRequestEvent(c, (num_left0 == 0) ? MyAxisStepper0::BufferBoundedType::maxValue() : MyAxisStepper0::BufferBoundedType::import(6));
}

static void add_commands1 (MyContext c)
{
    float t_scale = SPEED_T_SCALE;
    axis_stepper1.bufferProvideTest(c, true, 20.0, 1.0 * t_scale, 20.0);
    axis_stepper1.bufferProvideTest(c, true, 40.0, 1.0 * t_scale, 0.0);
    axis_stepper1.bufferProvideTest(c, true, 20.0, 1.0 * t_scale, -20.0);
    axis_stepper1.bufferProvideTest(c, false, 20.0, 1.0 * t_scale, 20.0);
    axis_stepper1.bufferProvideTest(c, false, 40.0, 1.0 * t_scale, 0.0);
    axis_stepper1.bufferProvideTest(c, false, 20.0, 1.0 * t_scale, -20.0);
    num_left1--;
    axis_stepper1.bufferRequestEvent(c, (num_left1 == 0) ? MyAxisStepper1::BufferBoundedType::maxValue() : MyAxisStepper1::BufferBoundedType::import(6));
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
        if (axis_stepper0.isRunning(c) || axis_stepper1.isRunning(c)) {
            if (axis_stepper0.isRunning(c)) {
                axis_stepper0.stop(c);
            }
            if (axis_stepper1.isRunning(c)) {
                axis_stepper1.stop(c);
            }
        } else {
            axis_stepper0.clearBuffer(c);
            axis_stepper1.clearBuffer(c);
            num_left0 = NUM_MOVE_ITERS;
            num_left1 = NUM_MOVE_ITERS;
            add_commands0(c);
            add_commands1(c);
            MyClock::TimeType start_time = myclock.getTime(c);
            steppers.getStepper<0>()->enable(c, true);
            steppers.getStepper<1>()->enable(c, true);
            axis_stepper0.start(c, start_time);
            axis_stepper1.start(c, start_time);
        }
    }
    prev_button = state;
}

static void serial_recv_handler (MySerial *, MyContext c)
{
    bool overrun;
    uint8_t rem_length = myserial.recvQuery(c, &overrun);
    
    bool saw_magic = false;
    
    while (rem_length > 0) {
        char *data = myserial.recvGetChunkPtr(c);
        uint8_t length = myserial.recvGetChunkLen(c, rem_length);
        for (size_t i = 0; i < length; i++) {
            servo_mode = data[i];
            if (data[i] == 0x41) {
                saw_magic = true;
            }
        }
        myserial.recvConsume(c, length);
        rem_length -= length;
    }
    
    if (overrun) {
        myserial.recvClearOverrun(c);
    }
    
    update_servos(c);
    
    if (saw_magic) {
        gen_rem = SERIAL_GEN_LENGTH;
        myserial.sendRequestEvent(c, 1);
    } else {
        write_to_serial(c, "OK\n");
    }
}

static void serial_send_handler (MySerial *, MyContext c)
{
    AMBRO_ASSERT(gen_rem > 0)
    
    uint8_t rem_length = myserial.sendQuery(c);
    if (rem_length > gen_rem) {
        rem_length = gen_rem;
    }
    
    gen_rem -= rem_length;
    
    while (rem_length > 0) {
        char *data = myserial.sendGetChunkPtr(c);
        uint8_t length = myserial.sendGetChunkLen(c, rem_length);
        memset(data, 'G', length);
        myserial.sendProvide(c, length);
        rem_length -= length;
    }
    
    if (gen_rem > 0) {
        myserial.sendRequestEvent(c, 1);
    }
}

static MySteppersStepper0 * driver_get_stepper_handler0 (MyAxisStepper0 *) 
{
    return steppers.getStepper<0>();
}

static MySteppersStepper1* driver_get_stepper_handler1 (MyAxisStepper1 *)
{
    return steppers.getStepper<1>();
}

static void driver_avail_handler0 (MyAxisStepper0 *, MyContext c)
{
    if (num_left0 == 0) {
        axis_stepper0.stop(c);
        steppers.getStepper<0>()->enable(c, false);
    } else {
        add_commands0(c);
    }
}

static void driver_avail_handler1 (MyAxisStepper1 *, MyContext c)
{
    if (num_left1 == 0) {
        axis_stepper1.stop(c);
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
    myclock.initTC3(c);
    myloop.init(c);
    mypins.init(c);
    mypinwatcherservice.init(c);
    mytimer.init(c, mytimer_handler);
    mypinwatcher.init(c);
    mysoftpwm.init(c);
    mysoftpwm2.init(c);
    myserial.init(c, SERIAL_BAUD);
    setup_uart_stdio();
    steppers.init(c);
    axis_stepper0.init(c);
    axis_stepper1.init(c);
    
    mypins.setOutput<LED1_PIN>(c);
    mypins.setOutput<LED2_PIN>(c);
    mypins.setInput<WATCH_PIN>(c);
    
    MyClock::TimeType ref_time = myclock.getTime(c);
    
    blink_state = false;
    next_time = myclock.getTime(c) + (uint32_t)(BLINK_INTERVAL / MyClock::time_unit);
    mytimer.appendAt(c, next_time);
    servo_mode = false;
    mysoftpwm.setOnTime(c, SERVO_PULSE_MIN / MyClock::time_unit);
    mysoftpwm2.setOnTime(c, SERVO_PULSE_MIN / MyClock::time_unit);
    //mysoftpwm.enable(c, ref_time);
    //mysoftpwm2.enable(c, ref_time + (MyClock::TimeType)(((SERVO_PULSE_INTERVAL*0.000001)/2) / MyClock::time_unit));
    gen_rem = 0;
    prev_button = false;
    
    /*
    uint32_t x = 0;
    do {
        uint16_t my = IntSqrt<uint32_t>::call(x);
        if (!((uint32_t)my * my <= x && (my == UINT16_MAX || ((uint32_t)my + 1) * ((uint32_t)my + 1) > x))) {
            printf("%" PRIu32 " BAD my=%" PRIu16 "\n", x, my);
        }
        x++;
    } while (x != 0);
    */
    
    /*
    printf("going\n");
    uint32_t sum = 0;
    uint32_t x = 1;
    do {
        //sum += UINT32_C(0xFEDCBA98) / x;
        sum += IntDivide<uint32_t, uint32_t>::call(UINT32_C(0xFEDCBA98), x);
        x++;
    } while (x < UINT32_C(500000));
    printf("hi %" PRIu32 "\n", sum);
    */
    
    /*
    for (uint32_t i = 0; i < UINT32_C(400000); i++) {
        uint32_t x;
        *((uint8_t *)&x + 0) = rand();
        *((uint8_t *)&x + 1) = rand();
        *((uint8_t *)&x + 2) = rand();
        *((uint8_t *)&x + 3) = rand();
        uint32_t y;
        *((uint8_t *)&y + 0) = rand();
        *((uint8_t *)&y + 1) = rand();
        *((uint8_t *)&y + 2) = rand();
        *((uint8_t *)&y + 3) = rand();
        if (y == 0) {
            continue;
        }
        if (IntDivide<uint32_t, uint32_t>::call(x, y) != x / y) {
            printf("ERROR %" PRIu32 " / %" PRIu32 "\n", x, y);
        }
    }
    */
    
    /*
    for (uint32_t i = 0; i < UINT32_C(400000); i++) {
        uint32_t x;
        *((uint8_t *)&x + 0) = rand();
        *((uint8_t *)&x + 1) = rand();
        *((uint8_t *)&x + 2) = rand();
        *((uint8_t *)&x + 3) = rand();
        uint16_t y;
        *((uint8_t *)&y + 0) = rand();
        *((uint8_t *)&y + 1) = rand();
        if (y == 0) {
            continue;
        }
        if (IntDivide<uint32_t, uint16_t>::call(x, y) != x / y) {
            printf("ERROR %" PRIu32 " / %" PRIu16 "\n", x, y);
        }
    }
    */
    /*
    for (uint32_t i = 0; i < UINT32_C(400000); i++) {
        uint32_t x;
        *((uint8_t *)&x + 0) = rand();
        *((uint8_t *)&x + 1) = rand();
        *((uint8_t *)&x + 2) = rand();
        *((uint8_t *)&x + 3) = rand();
        uint16_t y;
        *((uint8_t *)&y + 0) = rand();
        *((uint8_t *)&y + 1) = rand();
        if (y == 0) {
            continue;
        }
        uint32_t low;
        uint16_t high;
        mul_32_16(x, y, &low, &high);
        uint64_t res = ((uint64_t)high << 32) | low;
        if (res != (uint64_t)x * y) {
            printf("ERROR %" PRIu32 " * %" PRIu16 "\n", x, y);
        }
    }
    */
    myloop.run(c);
    
#ifdef AMBROLIB_SUPPORT_QUIT
    axis_stepper1.deinit(c);
    axis_stepper0.deinit(c);
    steppers.deinit(c);
    myserial.deinit(c);
    mysoftpwm2.deinit(c);
    mysoftpwm.deinit(c);
    mypinwatcher.deinit(c);
    mytimer.deinit(c);
    mypinwatcherservice.deinit(c);
    mypins.deinit(c);
    myloop.deinit(c);
    myclock.deinitTC3(c);
    myclock.deinit(c);
    d_group.deinit(c);
#endif
}
