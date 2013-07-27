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
#include <math.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#define AMBROLIB_ABORT_ACTION { cli(); while (1); }

#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/system/AvrEventLoop.h>
#include <aprinter/system/AvrClock.h>
#include <aprinter/system/AvrPins.h>
#include <aprinter/system/AvrPinWatcher.h>
#include <aprinter/system/AvrLock.h>
#include <aprinter/stepper/PrinterMain.h>

using namespace APrinter;

static const int clock_timer_prescaler = 2;
using StepVelType = FixedPoint<11, false, -11-4>;
using StepAccType = FixedPoint<11, false, -11-24>;
static const int stepper_command_buffer_size_exp = 3;

using LedBlinkInterval = AMBRO_WRAP_DOUBLE(0.5);
using DefaultInactiveTime = AMBRO_WRAP_DOUBLE(60.0);

using XDefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(80.0);
using XDefaultMaxSpeed = AMBRO_WRAP_DOUBLE(80.0);
using XDefaultMaxAccel = AMBRO_WRAP_DOUBLE(500.0);
using XDefaultOffset = AMBRO_WRAP_DOUBLE(53.0);
using XDefaultLimit = AMBRO_WRAP_DOUBLE(210.0);
using XDefaultHomeFastMaxDist = AMBRO_WRAP_DOUBLE(280.0);
using XDefaultHomeRetractDist = AMBRO_WRAP_DOUBLE(3.0);
using XDefaultHomeSlowMaxDist = AMBRO_WRAP_DOUBLE(5.0);
using XDefaultHomeFastSpeed = AMBRO_WRAP_DOUBLE(40.0);
using XDefaultHomeRetractSpeed = AMBRO_WRAP_DOUBLE(50.0);
using XDefaultHomeSlowSpeed = AMBRO_WRAP_DOUBLE(5.0);

using YDefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(80.0);
using YDefaultMaxSpeed = AMBRO_WRAP_DOUBLE(80.0);
using YDefaultMaxAccel = AMBRO_WRAP_DOUBLE(500.0);
using YDefaultOffset = AMBRO_WRAP_DOUBLE(0.0);
using YDefaultLimit = AMBRO_WRAP_DOUBLE(170.0);
using YDefaultHomeFastMaxDist = AMBRO_WRAP_DOUBLE(280.0);
using YDefaultHomeRetractDist = AMBRO_WRAP_DOUBLE(3.0);
using YDefaultHomeSlowMaxDist = AMBRO_WRAP_DOUBLE(5.0);
using YDefaultHomeFastSpeed = AMBRO_WRAP_DOUBLE(40.0);
using YDefaultHomeRetractSpeed = AMBRO_WRAP_DOUBLE(50.0);
using YDefaultHomeSlowSpeed = AMBRO_WRAP_DOUBLE(5.0);

using ZDefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(4000.0);
using ZDefaultMaxSpeed = AMBRO_WRAP_DOUBLE(2.0);
using ZDefaultMaxAccel = AMBRO_WRAP_DOUBLE(30.0);
using ZDefaultOffset = AMBRO_WRAP_DOUBLE(0.0);
using ZDefaultLimit = AMBRO_WRAP_DOUBLE(100.0);
using ZDefaultHomeFastMaxDist = AMBRO_WRAP_DOUBLE(100.0);
using ZDefaultHomeRetractDist = AMBRO_WRAP_DOUBLE(0.8);
using ZDefaultHomeSlowMaxDist = AMBRO_WRAP_DOUBLE(1.2);
using ZDefaultHomeFastSpeed = AMBRO_WRAP_DOUBLE(2.0);
using ZDefaultHomeRetractSpeed = AMBRO_WRAP_DOUBLE(2.0);
using ZDefaultHomeSlowSpeed = AMBRO_WRAP_DOUBLE(0.6);

using EDefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(928.0);
using EDefaultMaxSpeed = AMBRO_WRAP_DOUBLE(10.0);
using EDefaultMaxAccel = AMBRO_WRAP_DOUBLE(250.0);
using EDefaultOffset = AMBRO_WRAP_DOUBLE(100.0);
using EDefaultLimit = AMBRO_WRAP_DOUBLE(INFINITY);

using PrinterParams = PrinterMainParams<
    PrinterMainSerialParams<
        UINT32_C(115200), // baud rate
        GcodeParserParams<8> // receive buffer size exponent
    >,
    AvrPin<AvrPortA, 4>, // LED pin
    LedBlinkInterval,
    DefaultInactiveTime,
    MakeTypeList<
        PrinterMainAxisParams<
            'X', // axis name
            AvrPin<AvrPortC, 5>, // dir pin
            AvrPin<AvrPortD, 7>, // step pin
            AvrPin<AvrPortD, 6>, // enable pin
            true, // invert dir
            AxisStepperParams<
                stepper_command_buffer_size_exp,
                AvrClockInterruptTimer_TC1_OCA // stepper timer
            >,
            StepVelType, // velocity type
            StepAccType, // acceleration type
            XDefaultStepsPerUnit, // default steps per unit
            XDefaultMaxSpeed, // default max speed
            XDefaultMaxAccel, // default max acceleration
            XDefaultOffset,
            XDefaultLimit,
            PrinterMainHomingParams<
                AvrPin<AvrPortC, 2>, // endstop pin
                false, // invert endstop value
                false, // home direction (false=negative)
                XDefaultHomeFastMaxDist,
                XDefaultHomeRetractDist,
                XDefaultHomeSlowMaxDist,
                XDefaultHomeFastSpeed,
                XDefaultHomeRetractSpeed,
                XDefaultHomeSlowSpeed
            >
        >,
        PrinterMainAxisParams<
            'Y', // axis name
            AvrPin<AvrPortC, 7>, // dir pin
            AvrPin<AvrPortC, 6>, // step pin
            AvrPin<AvrPortD, 6>, // enable pin
            true, // invert dir
            AxisStepperParams<
                stepper_command_buffer_size_exp,
                AvrClockInterruptTimer_TC1_OCB // stepper timer
            >,
            StepVelType, // velocity type
            StepAccType, // acceleration type
            YDefaultStepsPerUnit, // default steps per unit
            YDefaultMaxSpeed, // default max speed
            YDefaultMaxAccel, // default max acceleration
            YDefaultOffset,
            YDefaultLimit,
            PrinterMainHomingParams<
                AvrPin<AvrPortC, 3>, // endstop pin
                false, // invert endstop value
                false, // home direction (false=negative)
                YDefaultHomeFastMaxDist,
                YDefaultHomeRetractDist,
                YDefaultHomeSlowMaxDist,
                YDefaultHomeFastSpeed,
                YDefaultHomeRetractSpeed,
                YDefaultHomeSlowSpeed
            >
        >,
        PrinterMainAxisParams<
            'Z', // axis name
            AvrPin<AvrPortB, 2>, // dir pin
            AvrPin<AvrPortB, 3>, // step pin
            AvrPin<AvrPortA, 5>, // enable pin
            false, // invert dir
            AxisStepperParams<
                stepper_command_buffer_size_exp,
                AvrClockInterruptTimer_TC3_OCA // stepper timer
            >,
            StepVelType, // velocity type
            StepAccType, // acceleration type
            ZDefaultStepsPerUnit, // default steps per unit
            ZDefaultMaxSpeed, // default max speed
            ZDefaultMaxAccel, // default max acceleration
            ZDefaultOffset,
            ZDefaultLimit,
            PrinterMainHomingParams<
                AvrPin<AvrPortC, 4>, // endstop pin
                false, // invert endstop value
                false, // home direction (false=negative)
                ZDefaultHomeFastMaxDist,
                ZDefaultHomeRetractDist,
                ZDefaultHomeSlowMaxDist,
                ZDefaultHomeFastSpeed,
                ZDefaultHomeRetractSpeed,
                ZDefaultHomeSlowSpeed
            >
        >,
        PrinterMainAxisParams<
            'E', // axis name
            AvrPin<AvrPortB, 0>, // dir pin
            AvrPin<AvrPortB, 1>, // step pin
            AvrPin<AvrPortD, 6>, // enable pin
            false, // invert dir
            AxisStepperParams<
                stepper_command_buffer_size_exp,
                AvrClockInterruptTimer_TC3_OCB // stepper timer
            >,
            StepVelType, // velocity type
            StepAccType, // acceleration type
            EDefaultStepsPerUnit, // default steps per unit
            EDefaultMaxSpeed, // default max speed
            EDefaultMaxAccel, // default max acceleration
            EDefaultOffset,
            EDefaultLimit,
            PrinterMainNoHomingParams
        >
    >
>;

struct MyContext;
struct EventLoopParams;

using MyDebugObjectGroup = DebugObjectGroup<MyContext>;
using MyClock = AvrClock<MyContext, clock_timer_prescaler>;
using MyLoop = AvrEventLoop<EventLoopParams>;
using MyPins = AvrPins<MyContext>;
using MyPinWatcherService = AvrPinWatcherService<MyContext>;
using MyPrinter = PrinterMain<MyContext, PrinterParams>;

struct MyContext {
    using DebugGroup = MyDebugObjectGroup;
    using Lock = AvrLock<MyContext>;
    using Clock = MyClock;
    using EventLoop = MyLoop;
    using Pins = MyPins;
    using PinWatcherService = MyPinWatcherService;
    
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
static MyPrinter myprinter;

MyDebugObjectGroup * MyContext::debugGroup () const { return &d_group; }
MyClock * MyContext::clock () const { return &myclock; }
MyLoop * MyContext::eventLoop () const { return &myloop; }
MyPins * MyContext::pins () const { return &mypins; }
MyPinWatcherService * MyContext::pinWatcherService () const { return &mypinwatcherservice; }

AMBRO_AVR_CLOCK_ISRS(myclock, MyContext())
AMBRO_AVR_PIN_WATCHER_ISRS(mypinwatcherservice, MyContext())
AMBRO_AVR_SERIAL_ISRS(*myprinter.getSerial(), MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC1_OCA_ISRS(*myprinter.template getSharer<0>()->getTimer(), MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC1_OCB_ISRS(*myprinter.template getSharer<1>()->getTimer(), MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC3_OCA_ISRS(*myprinter.template getSharer<2>()->getTimer(), MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC3_OCB_ISRS(*myprinter.template getSharer<3>()->getTimer(), MyContext())

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
    setup_uart_stdio();
    
    MyContext c;
    
    d_group.init(c);
    myclock.init(c);
#ifdef TCNT3
    myclock.initTC3(c);
#endif
    myloop.init(c);
    mypins.init(c);
    mypinwatcherservice.init(c);
    myprinter.init(c);
    
    myloop.run(c);
}
