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

static void emergency (void);

#define AMBROLIB_EMERGENCY_ACTION { cli(); emergency(); }
#define AMBROLIB_ABORT_ACTION { while (1); }

#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/Position.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/system/AvrEventLoop.h>
#include <aprinter/system/AvrClock.h>
#include <aprinter/system/AvrPins.h>
#include <aprinter/system/AvrPinWatcher.h>
#include <aprinter/system/AvrLock.h>
#include <aprinter/system/AvrAdc.h>
#include <aprinter/system/AvrWatchdog.h>
#include <aprinter/devices/PidControl.h>
#include <aprinter/devices/BinaryControl.h>
#include <aprinter/printer/PrinterMain.h>
#include <generated/AvrThermistorTable_Extruder.h>
#include <generated/AvrThermistorTable_Bed.h>

using namespace APrinter;

static const int AdcRefSel = 1;
static const int AdcPrescaler = 7;
static const int clock_timer_prescaler = 3;

using LedBlinkInterval = AMBRO_WRAP_DOUBLE(0.5);
using DefaultInactiveTime = AMBRO_WRAP_DOUBLE(60.0);
using SpeedLimitMultiply = AMBRO_WRAP_DOUBLE(1.0 / 60.0);
using MaxStepsPerCycle = AMBRO_WRAP_DOUBLE(0.00137);
using ForceTimeout = AMBRO_WRAP_DOUBLE(0.1);

using XDefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(80.0);
using XDefaultMaxSpeed = AMBRO_WRAP_DOUBLE(300.0);
using XDefaultMaxAccel = AMBRO_WRAP_DOUBLE(800.0);
using XDefaultDistanceFactor = AMBRO_WRAP_DOUBLE(1.0);
using XDefaultCorneringDistance = AMBRO_WRAP_DOUBLE(32.0);
using XDefaultMin = AMBRO_WRAP_DOUBLE(-53.0);
using XDefaultMax = AMBRO_WRAP_DOUBLE(210.0);
using XDefaultHomeFastMaxDist = AMBRO_WRAP_DOUBLE(280.0);
using XDefaultHomeRetractDist = AMBRO_WRAP_DOUBLE(3.0);
using XDefaultHomeSlowMaxDist = AMBRO_WRAP_DOUBLE(5.0);
using XDefaultHomeFastSpeed = AMBRO_WRAP_DOUBLE(40.0);
using XDefaultHomeRetractSpeed = AMBRO_WRAP_DOUBLE(50.0);
using XDefaultHomeSlowSpeed = AMBRO_WRAP_DOUBLE(5.0);

using YDefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(80.0);
using YDefaultMaxSpeed = AMBRO_WRAP_DOUBLE(300.0);
using YDefaultMaxAccel = AMBRO_WRAP_DOUBLE(600.0);
using YDefaultDistanceFactor = AMBRO_WRAP_DOUBLE(1.0);
using YDefaultCorneringDistance = AMBRO_WRAP_DOUBLE(32.0);
using YDefaultMin = AMBRO_WRAP_DOUBLE(0.0);
using YDefaultMax = AMBRO_WRAP_DOUBLE(170.0);
using YDefaultHomeFastMaxDist = AMBRO_WRAP_DOUBLE(200.0);
using YDefaultHomeRetractDist = AMBRO_WRAP_DOUBLE(3.0);
using YDefaultHomeSlowMaxDist = AMBRO_WRAP_DOUBLE(5.0);
using YDefaultHomeFastSpeed = AMBRO_WRAP_DOUBLE(40.0);
using YDefaultHomeRetractSpeed = AMBRO_WRAP_DOUBLE(50.0);
using YDefaultHomeSlowSpeed = AMBRO_WRAP_DOUBLE(5.0);

using ZDefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(4000.0);
using ZDefaultMaxSpeed = AMBRO_WRAP_DOUBLE(3.0);
using ZDefaultMaxAccel = AMBRO_WRAP_DOUBLE(30.0);
using ZDefaultDistanceFactor = AMBRO_WRAP_DOUBLE(1.0);
using ZDefaultCorneringDistance = AMBRO_WRAP_DOUBLE(32.0);
using ZDefaultMin = AMBRO_WRAP_DOUBLE(0.0);
using ZDefaultMax = AMBRO_WRAP_DOUBLE(100.0);
using ZDefaultHomeFastMaxDist = AMBRO_WRAP_DOUBLE(101.0);
using ZDefaultHomeRetractDist = AMBRO_WRAP_DOUBLE(0.8);
using ZDefaultHomeSlowMaxDist = AMBRO_WRAP_DOUBLE(1.2);
using ZDefaultHomeFastSpeed = AMBRO_WRAP_DOUBLE(2.0);
using ZDefaultHomeRetractSpeed = AMBRO_WRAP_DOUBLE(2.0);
using ZDefaultHomeSlowSpeed = AMBRO_WRAP_DOUBLE(0.6);

using EDefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(928.0);
using EDefaultMaxSpeed = AMBRO_WRAP_DOUBLE(45.0);
using EDefaultMaxAccel = AMBRO_WRAP_DOUBLE(250.0);
using EDefaultDistanceFactor = AMBRO_WRAP_DOUBLE(1.0);
using EDefaultCorneringDistance = AMBRO_WRAP_DOUBLE(32.0);
using EDefaultMin = AMBRO_WRAP_DOUBLE(-10000.0);
using EDefaultMax = AMBRO_WRAP_DOUBLE(10000.0);

/*
 * NOTE: The natural semantic of ExtruderHeaterPidDHistory
 * is sensitive to changes in ExtruderHeaterPulseInterval.
 * When you change ExtruderHeaterPulseInterval as if by multiplication with
 * 'a', raise ExtruderHeaterPidDHistory to the power of 'a'.
 */
using ExtruderHeaterPulseInterval = AMBRO_WRAP_DOUBLE(0.2);
using ExtruderHeaterMinSafeTemp = AMBRO_WRAP_DOUBLE(20.0);
using ExtruderHeaterMaxSafeTemp = AMBRO_WRAP_DOUBLE(280.0);
using ExtruderHeaterPidP = AMBRO_WRAP_DOUBLE(0.047);
using ExtruderHeaterPidI = AMBRO_WRAP_DOUBLE(0.0006);
using ExtruderHeaterPidD = AMBRO_WRAP_DOUBLE(0.1);
using ExtruderHeaterPidIStateMin = AMBRO_WRAP_DOUBLE(0.0);
using ExtruderHeaterPidIStateMax = AMBRO_WRAP_DOUBLE(0.12);
using ExtruderHeaterPidDHistory = AMBRO_WRAP_DOUBLE(0.7);
using ExtruderHeaterObserverInterval = AMBRO_WRAP_DOUBLE(0.5);
using ExtruderHeaterObserverTolerance = AMBRO_WRAP_DOUBLE(3.0);
using ExtruderHeaterObserverMinTime = AMBRO_WRAP_DOUBLE(3.0);

using BedHeaterPulseInterval = AMBRO_WRAP_DOUBLE(2.0);
using BedHeaterMinSafeTemp = AMBRO_WRAP_DOUBLE(20.0);
using BedHeaterMaxSafeTemp = AMBRO_WRAP_DOUBLE(120.0);
using BedHeaterObserverInterval = AMBRO_WRAP_DOUBLE(0.5);
using BedHeaterObserverTolerance = AMBRO_WRAP_DOUBLE(1.5);
using BedHeaterObserverMinTime = AMBRO_WRAP_DOUBLE(3.0);

using FanSpeedMultiply = AMBRO_WRAP_DOUBLE(1.0 / 255.0);
using FanPulseInterval = AMBRO_WRAP_DOUBLE(0.04);

using PrinterParams = PrinterMainParams<
    PrinterMainSerialParams<
        UINT32_C(57600), // baud rate. Don't increase, or serial will fail randomly.
        GcodeParserParams<8> // receive buffer size exponent
    >,
    AvrPin<AvrPortA, 4>, // LED pin
    LedBlinkInterval,
    DefaultInactiveTime,
    SpeedLimitMultiply,
    MaxStepsPerCycle,
    16, // stepper segment buffer size
    2, // lookahead buffer size exponent
    ForceTimeout,
    AvrWatchdog,
    AvrWatchdogParams<
        WDTO_2S // watchdot timeout
    >,
    16, // event channel buffer size
    AvrClockInterruptTimer_TC2_OCA,
    MakeTypeList<
        PrinterMainAxisParams<
            'X', // axis name
            AvrPin<AvrPortC, 5>, // dir pin
            AvrPin<AvrPortD, 7>, // step pin
            AvrPin<AvrPortD, 6>, // enable pin
            true, // invert dir
            24, // step bits
            AxisStepperParams<
                AvrClockInterruptTimer_TC1_OCA // stepper timer
            >,
            XDefaultStepsPerUnit, // default steps per unit
            XDefaultMaxSpeed, // default max speed
            XDefaultMaxAccel, // default max acceleration
            XDefaultDistanceFactor,
            XDefaultCorneringDistance,
            XDefaultMin,
            XDefaultMax,
            true, // enable cartesian speed limit
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
            24, // step bits
            AxisStepperParams<
                AvrClockInterruptTimer_TC1_OCB // stepper timer
            >,
            YDefaultStepsPerUnit, // default steps per unit
            YDefaultMaxSpeed, // default max speed
            YDefaultMaxAccel, // default max acceleration
            YDefaultDistanceFactor,
            YDefaultCorneringDistance,
            YDefaultMin,
            YDefaultMax,
            true, // enable cartesian speed limit
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
            24, // step bits
            AxisStepperParams<
                AvrClockInterruptTimer_TC3_OCA // stepper timer
            >,
            ZDefaultStepsPerUnit, // default steps per unit
            ZDefaultMaxSpeed, // default max speed
            ZDefaultMaxAccel, // default max acceleration
            ZDefaultDistanceFactor,
            ZDefaultCorneringDistance,
            ZDefaultMin,
            ZDefaultMax,
            true, // enable cartesian speed limit
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
            true, // invert dir
            24, // step bits
            AxisStepperParams<
                AvrClockInterruptTimer_TC3_OCB // stepper timer
            >,
            EDefaultStepsPerUnit, // default steps per unit
            EDefaultMaxSpeed, // default max speed
            EDefaultMaxAccel, // default max acceleration
            EDefaultDistanceFactor,
            EDefaultCorneringDistance,
            EDefaultMin,
            EDefaultMax,
            false, // enable cartesian speed limit
            PrinterMainNoHomingParams
        >
    >,
    MakeTypeList<
        PrinterMainHeaterParams<
            'T', // controlee name
            104, // set M command
            109, // wait M command
            AvrPin<AvrPortA, 7>, // analog sensor pin
            AvrThermistorTable_Extruder, // sensor interpretation formula
            AvrPin<AvrPortD, 5>, // output pin
            ExtruderHeaterPulseInterval,
            ExtruderHeaterMinSafeTemp,
            ExtruderHeaterMaxSafeTemp,
            PidControl,
            PidControlParams<
                ExtruderHeaterPidP,
                ExtruderHeaterPidI,
                ExtruderHeaterPidD,
                ExtruderHeaterPidIStateMin,
                ExtruderHeaterPidIStateMax,
                ExtruderHeaterPidDHistory
            >,
            AvrClockInterruptTimer_TC0_OCA,
            TemperatureObserverParams<
                ExtruderHeaterObserverInterval,
                ExtruderHeaterObserverTolerance,
                ExtruderHeaterObserverMinTime
            >
        >,
        PrinterMainHeaterParams<
            'B', // controlee name
            140, // set M command
            190, // wait M command
            AvrPin<AvrPortA, 6>, // analog sensor pin
            AvrThermistorTable_Bed, // sensor interpretation formula
            AvrPin<AvrPortD, 4>, // output pin
            BedHeaterPulseInterval,
            BedHeaterMinSafeTemp,
            BedHeaterMaxSafeTemp,
            BinaryControl,
            BinaryControlParams,
            AvrClockInterruptTimer_TC0_OCB,
            TemperatureObserverParams<
                BedHeaterObserverInterval,
                BedHeaterObserverTolerance,
                BedHeaterObserverMinTime
            >
        >
    >,
    MakeTypeList<
        PrinterMainFanParams<
            106, // set M command
            107, // off M command
            FanSpeedMultiply,
            AvrPin<AvrPortB, 4>,
            FanPulseInterval,
            AvrClockInterruptTimer_TC2_OCB
        >
    >
>;

// need to list all used ADC pins here
using AdcPins = MakeTypeList<
    AvrPin<AvrPortA, 6>,
    AvrPin<AvrPortA, 7>
>;

struct MyContext;
struct EventLoopParams;
struct PrinterPosition;

using MyDebugObjectGroup = DebugObjectGroup<MyContext>;
using MyClock = AvrClock<MyContext, clock_timer_prescaler>;
using MyLoop = AvrEventLoop<EventLoopParams>;
using MyPins = AvrPins<MyContext>;
using MyPinWatcherService = AvrPinWatcherService<MyContext>;
using MyAdc = AvrAdc<MyContext, AdcPins, AdcRefSel, AdcPrescaler>;
using MyPrinter = PrinterMain<PrinterPosition, MyContext, PrinterParams>;

struct MyContext {
    using DebugGroup = MyDebugObjectGroup;
    using Lock = AvrLock<MyContext>;
    using Clock = MyClock;
    using EventLoop = MyLoop;
    using Pins = MyPins;
    using PinWatcherService = MyPinWatcherService;
    using Adc = MyAdc;
    
    MyDebugObjectGroup * debugGroup () const;
    MyClock * clock () const;
    MyLoop * eventLoop () const;
    MyPins * pins () const;
    MyPinWatcherService * pinWatcherService () const;
    MyAdc * adc () const;
};

struct EventLoopParams {
    typedef MyContext Context;
};

struct PrinterPosition : public RootPosition<MyPrinter> {};

static MyDebugObjectGroup d_group;
static MyClock myclock;
static MyLoop myloop;
static MyPins mypins;
static MyPinWatcherService mypinwatcherservice;
static MyAdc myadc;
static MyPrinter myprinter;

MyDebugObjectGroup * MyContext::debugGroup () const { return &d_group; }
MyClock * MyContext::clock () const { return &myclock; }
MyLoop * MyContext::eventLoop () const { return &myloop; }
MyPins * MyContext::pins () const { return &mypins; }
MyPinWatcherService * MyContext::pinWatcherService () const { return &mypinwatcherservice; }
MyAdc * MyContext::adc () const { return &myadc; }

AMBRO_AVR_CLOCK_ISRS(myclock, MyContext())
AMBRO_AVR_PIN_WATCHER_ISRS(mypinwatcherservice, MyContext())
AMBRO_AVR_ADC_ISRS(myadc, MyContext())
AMBRO_AVR_SERIAL_ISRS(*myprinter.getSerial(), MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC1_OCA_ISRS(*myprinter.getAxisStepper<0>()->getTimer(), MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC1_OCB_ISRS(*myprinter.getAxisStepper<1>()->getTimer(), MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC3_OCA_ISRS(*myprinter.getAxisStepper<2>()->getTimer(), MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC3_OCB_ISRS(*myprinter.getAxisStepper<3>()->getTimer(), MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC0_OCA_ISRS(*myprinter.getHeaterTimer<0>(), MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC0_OCB_ISRS(*myprinter.getHeaterTimer<1>(), MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC2_OCA_ISRS(*myprinter.getEventChannelTimer(), MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC2_OCB_ISRS(*myprinter.getFanTimer<0>(), MyContext())
AMBRO_AVR_WATCHDOG_GLOBAL

FILE uart_output;

static int uart_putchar (char ch, FILE *stream)
{
    myprinter.getSerial()->sendWaitFinished();
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

static void emergency (void)
{
    MyPrinter::emergency();
}

int main ()
{
    sei();
    setup_uart_stdio();
    
    MyContext c;
    
    d_group.init(c);
    myclock.init(c);
    myclock.initTC3(c);
    myclock.initTC0(c);
    myclock.initTC2(c);
    myloop.init(c);
    mypins.init(c);
    mypinwatcherservice.init(c);
    myadc.init(c);
    myprinter.init(c);
    
    myloop.run(c);
}
