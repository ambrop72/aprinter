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

#include <aprinter/platform/at91sam7s/at91sam7s_support.h>

static void emergency (void);

#define AMBROLIB_EMERGENCY_ACTION { cli(); emergency(); }
#define AMBROLIB_ABORT_ACTION { while (1); }

#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/Position.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/system/BusyEventLoop.h>
#include <aprinter/system/At91Sam7sClock.h>
#include <aprinter/system/At91Sam7sPins.h>
#include <aprinter/system/InterruptLock.h>
//#include <aprinter/system/AvrAdc.h>
#include <aprinter/system/At91Sam7sWatchdog.h>
#include <aprinter/system/At91Sam7sSerial.h>
#include <aprinter/devices/PidControl.h>
#include <aprinter/devices/BinaryControl.h>
#include <aprinter/printer/PrinterMain.h>
//#include <generated/AvrThermistorTable_Extruder.h>
//#include <generated/AvrThermistorTable_Bed.h>

using namespace APrinter;

using LedBlinkInterval = AMBRO_WRAP_DOUBLE(0.5);
using DefaultInactiveTime = AMBRO_WRAP_DOUBLE(60.0);
using SpeedLimitMultiply = AMBRO_WRAP_DOUBLE(1.0 / 60.0);
using MaxStepsPerCycle = AMBRO_WRAP_DOUBLE(0.00137); // max stepping frequency relative to F_CPU
using ForceTimeout = AMBRO_WRAP_DOUBLE(0.1);

using XDefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(80.0);
using XDefaultMin = AMBRO_WRAP_DOUBLE(-53.0);
using XDefaultMax = AMBRO_WRAP_DOUBLE(210.0);
using XDefaultMaxSpeed = AMBRO_WRAP_DOUBLE(300.0);
using XDefaultMaxAccel = AMBRO_WRAP_DOUBLE(800.0);
using XDefaultDistanceFactor = AMBRO_WRAP_DOUBLE(1.0);
using XDefaultCorneringDistance = AMBRO_WRAP_DOUBLE(32.0);
using XDefaultHomeFastMaxDist = AMBRO_WRAP_DOUBLE(280.0);
using XDefaultHomeRetractDist = AMBRO_WRAP_DOUBLE(3.0);
using XDefaultHomeSlowMaxDist = AMBRO_WRAP_DOUBLE(5.0);
using XDefaultHomeFastSpeed = AMBRO_WRAP_DOUBLE(40.0);
using XDefaultHomeRetractSpeed = AMBRO_WRAP_DOUBLE(50.0);
using XDefaultHomeSlowSpeed = AMBRO_WRAP_DOUBLE(5.0);

using YDefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(80.0);
using YDefaultMin = AMBRO_WRAP_DOUBLE(0.0);
using YDefaultMax = AMBRO_WRAP_DOUBLE(170.0);
using YDefaultMaxSpeed = AMBRO_WRAP_DOUBLE(300.0);
using YDefaultMaxAccel = AMBRO_WRAP_DOUBLE(600.0);
using YDefaultDistanceFactor = AMBRO_WRAP_DOUBLE(1.0);
using YDefaultCorneringDistance = AMBRO_WRAP_DOUBLE(32.0);
using YDefaultHomeFastMaxDist = AMBRO_WRAP_DOUBLE(200.0);
using YDefaultHomeRetractDist = AMBRO_WRAP_DOUBLE(3.0);
using YDefaultHomeSlowMaxDist = AMBRO_WRAP_DOUBLE(5.0);
using YDefaultHomeFastSpeed = AMBRO_WRAP_DOUBLE(40.0);
using YDefaultHomeRetractSpeed = AMBRO_WRAP_DOUBLE(50.0);
using YDefaultHomeSlowSpeed = AMBRO_WRAP_DOUBLE(5.0);

using ZDefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(4000.0);
using ZDefaultMin = AMBRO_WRAP_DOUBLE(0.0);
using ZDefaultMax = AMBRO_WRAP_DOUBLE(100.0);
using ZDefaultMaxSpeed = AMBRO_WRAP_DOUBLE(3.0);
using ZDefaultMaxAccel = AMBRO_WRAP_DOUBLE(30.0);
using ZDefaultDistanceFactor = AMBRO_WRAP_DOUBLE(1.0);
using ZDefaultCorneringDistance = AMBRO_WRAP_DOUBLE(32.0);
using ZDefaultHomeFastMaxDist = AMBRO_WRAP_DOUBLE(101.0);
using ZDefaultHomeRetractDist = AMBRO_WRAP_DOUBLE(0.8);
using ZDefaultHomeSlowMaxDist = AMBRO_WRAP_DOUBLE(1.2);
using ZDefaultHomeFastSpeed = AMBRO_WRAP_DOUBLE(2.0);
using ZDefaultHomeRetractSpeed = AMBRO_WRAP_DOUBLE(2.0);
using ZDefaultHomeSlowSpeed = AMBRO_WRAP_DOUBLE(0.6);

using EDefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(928.0);
using EDefaultMin = AMBRO_WRAP_DOUBLE(-40000.0);
using EDefaultMax = AMBRO_WRAP_DOUBLE(40000.0);
using EDefaultMaxSpeed = AMBRO_WRAP_DOUBLE(45.0);
using EDefaultMaxAccel = AMBRO_WRAP_DOUBLE(250.0);
using EDefaultDistanceFactor = AMBRO_WRAP_DOUBLE(1.0);
using EDefaultCorneringDistance = AMBRO_WRAP_DOUBLE(32.0);

using ExtruderHeaterMinSafeTemp = AMBRO_WRAP_DOUBLE(20.0);
using ExtruderHeaterMaxSafeTemp = AMBRO_WRAP_DOUBLE(280.0);
using ExtruderHeaterPulseInterval = AMBRO_WRAP_DOUBLE(0.2);
using ExtruderHeaterPidP = AMBRO_WRAP_DOUBLE(0.047);
using ExtruderHeaterPidI = AMBRO_WRAP_DOUBLE(0.0006);
using ExtruderHeaterPidD = AMBRO_WRAP_DOUBLE(0.17);
using ExtruderHeaterPidIStateMin = AMBRO_WRAP_DOUBLE(0.0);
using ExtruderHeaterPidIStateMax = AMBRO_WRAP_DOUBLE(0.12);
using ExtruderHeaterPidDHistory = AMBRO_WRAP_DOUBLE(0.7);
using ExtruderHeaterObserverInterval = AMBRO_WRAP_DOUBLE(0.5);
using ExtruderHeaterObserverTolerance = AMBRO_WRAP_DOUBLE(3.0);
using ExtruderHeaterObserverMinTime = AMBRO_WRAP_DOUBLE(3.0);

using BedHeaterMinSafeTemp = AMBRO_WRAP_DOUBLE(20.0);
using BedHeaterMaxSafeTemp = AMBRO_WRAP_DOUBLE(120.0);
using BedHeaterPulseInterval = AMBRO_WRAP_DOUBLE(2.0);
using BedHeaterObserverInterval = AMBRO_WRAP_DOUBLE(0.5);
using BedHeaterObserverTolerance = AMBRO_WRAP_DOUBLE(1.5);
using BedHeaterObserverMinTime = AMBRO_WRAP_DOUBLE(3.0);

using FanSpeedMultiply = AMBRO_WRAP_DOUBLE(1.0 / 255.0);
using FanPulseInterval = AMBRO_WRAP_DOUBLE(0.04);

using PrinterParams = PrinterMainParams<
    /*
     * Common parameters.
     */
    PrinterMainSerialParams<
        UINT32_C(115200), // BaudRate
        GcodeParserParams<8>, // ReceiveBufferSizeExp
        At91Sam7sSerial,
        At91Sam7sSerialParams
    >,
    At91Sam7sPin<31>, // LedPin
    LedBlinkInterval, // LedBlinkInterval
    DefaultInactiveTime, // DefaultInactiveTime
    SpeedLimitMultiply, // SpeedLimitMultiply
    MaxStepsPerCycle, // MaxStepsPerCycle
    20, // StepperSegmentBufferSize
    20, // EventChannelBufferSize
    4, // LookaheadBufferSize
    ForceTimeout, // ForceTimeout
    At91Sam7sClockInterruptTimer_TC2A, // EventChannelTimer
    At91Sam7sWatchdog,
    At91Sam7sWatchdogParams,
    
    /*
     * Axes.
     */
    MakeTypeList<
        PrinterMainAxisParams<
            'X', // Name
            At91Sam7sPin<12>, // DirPin
            At91Sam7sPin<11>, // StepPin
            At91Sam7sPin<10>, // EnablePin
            true, // InvertDir
            XDefaultStepsPerUnit, // StepsPerUnit
            XDefaultMin, // Min
            XDefaultMax, // Max
            XDefaultMaxSpeed, // MaxSpeed
            XDefaultMaxAccel, // MaxAccel
            XDefaultDistanceFactor, // DistanceFactor
            XDefaultCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                At91Sam7sPin<9>, // HomeEndPin
                false, // HomeEndInvert
                false, // HomeDir
                XDefaultHomeFastMaxDist, // HomeFastMaxDist
                XDefaultHomeRetractDist, // HomeRetractDist
                XDefaultHomeSlowMaxDist, // HomeSlowMaxDist
                XDefaultHomeFastSpeed, // HomeFastSpeed
                XDefaultHomeRetractSpeed, // HomeRetractSpeed
                XDefaultHomeSlowSpeed // HomeSlowSpeed
            >,
            true, // EnableCartesianSpeedLimit
            24, // StepBits
            AxisStepperParams<
                At91Sam7sClockInterruptTimer_TC0A // StepperTimer
            >
        >,
        PrinterMainAxisParams<
            'Y', // Name
            At91Sam7sPin<8>, // DirPin
            At91Sam7sPin<7>, // StepPin
            At91Sam7sPin<1>, // EnablePin
            true, // InvertDir
            YDefaultStepsPerUnit, // StepsPerUnit
            YDefaultMin, // Min
            YDefaultMax, // Max
            YDefaultMaxSpeed, // MaxSpeed
            YDefaultMaxAccel, // MaxAccel
            YDefaultDistanceFactor, // DistanceFactor
            YDefaultCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                At91Sam7sPin<0>, // HomeEndPin
                false, // HomeEndInvert
                false, // HomeDir
                YDefaultHomeFastMaxDist, // HomeFastMaxDist
                YDefaultHomeRetractDist, // HomeRetractDist
                YDefaultHomeSlowMaxDist, // HomeSlowMaxDist
                YDefaultHomeFastSpeed, // HomeFastSpeed
                YDefaultHomeRetractSpeed, // HomeRetractSpeed
                YDefaultHomeSlowSpeed // HomeSlowSpeed
            >,
            true, // EnableCartesianSpeedLimit
            24, // StepBits
            AxisStepperParams<
                At91Sam7sClockInterruptTimer_TC0B // StepperTimer
            >
        >,
        PrinterMainAxisParams<
            'Z', // Name
            At91Sam7sPin<4>, // DirPin
            At91Sam7sPin<27>, // StepPin
            At91Sam7sPin<28>, // EnablePin
            false, // InvertDir
            ZDefaultStepsPerUnit, // StepsPerUnit
            ZDefaultMin, // Min
            ZDefaultMax, // Max
            ZDefaultMaxSpeed, // MaxSpeed
            ZDefaultMaxAccel, // MaxAccel
            ZDefaultDistanceFactor, // DistanceFactor
            ZDefaultCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                At91Sam7sPin<29>, // HomeEndPin
                false, // HomeEndInvert
                false, // HomeDir
                ZDefaultHomeFastMaxDist, // HomeFastMaxDist
                ZDefaultHomeRetractDist, // HomeRetractDist
                ZDefaultHomeSlowMaxDist, // HomeSlowMaxDist
                ZDefaultHomeFastSpeed, // HomeFastSpeed
                ZDefaultHomeRetractSpeed, // HomeRetractSpeed
                ZDefaultHomeSlowSpeed // HomeSlowSpeed
            >,
            true, // EnableCartesianSpeedLimit
            24, // StepBits
            AxisStepperParams<
                At91Sam7sClockInterruptTimer_TC0C // StepperTimer
            >
        >,
        PrinterMainAxisParams<
            'E', // Name
            At91Sam7sPin<30>, // DirPin
            At91Sam7sPin<3>, // StepPin
            At91Sam7sPin<2>, // EnablePin
            true, // InvertDir
            EDefaultStepsPerUnit, // StepsPerUnit
            EDefaultMin, // Min
            EDefaultMax, // Max
            EDefaultMaxSpeed, // MaxSpeed
            EDefaultMaxAccel, // MaxAccel
            EDefaultDistanceFactor, // DistanceFactor
            EDefaultCorneringDistance, // CorneringDistance
            PrinterMainNoHomingParams,
            false, // EnableCartesianSpeedLimit
            32, // StepBits
            AxisStepperParams<
                At91Sam7sClockInterruptTimer_TC1A // StepperTimer
            >
        >
    >,
    
    /*
     * Heaters.
     */
    MakeTypeList<
    /*
        PrinterMainHeaterParams<
            'T', // Name
            104, // SetMCommand
            109, // WaitMCommand
            At91Sam7sPin<17>, // AdcPin
            At91Sam7sPin<26>, // OutputPin
            AvrThermistorTable_Extruder, // Formula
            ExtruderHeaterMinSafeTemp, // MinSafeTemp
            ExtruderHeaterMaxSafeTemp, // MaxSafeTemp
            ExtruderHeaterPulseInterval, // PulseInterval
            PidControl, // Control
            PidControlParams<
                ExtruderHeaterPidP, // PidP
                ExtruderHeaterPidI, // PidI
                ExtruderHeaterPidD, // PidD
                ExtruderHeaterPidIStateMin, // PidIStateMin
                ExtruderHeaterPidIStateMax, // PidIStateMax
                ExtruderHeaterPidDHistory // PidDHistory
            >,
            TemperatureObserverParams<
                ExtruderHeaterObserverInterval, // ObserverInterval
                ExtruderHeaterObserverTolerance, // ObserverTolerance
                ExtruderHeaterObserverMinTime // ObserverMinTime
            >,
            At91Sam7sClockInterruptTimer_TC1B // TimerTemplate
        >,
        PrinterMainHeaterParams<
            'B', // Name
            140, // SetMCommand
            190, // WaitMCommand
            At91Sam7sPin<18>, // AdcPin
            At91Sam7sPin<25>, // OutputPin
            AvrThermistorTable_Bed, // Formula
            BedHeaterMinSafeTemp, // MinSafeTemp
            BedHeaterMaxSafeTemp, // MaxSafeTemp
            BedHeaterPulseInterval, // PulseInterval
            BinaryControl, // Control
            BinaryControlParams,
            TemperatureObserverParams<
                BedHeaterObserverInterval, // ObserverInterval
                BedHeaterObserverTolerance, // ObserverTolerance
                BedHeaterObserverMinTime // ObserverMinTime
            >,
            At91Sam7sClockInterruptTimer_TC1C // TimerTemplate
        >
        */
    >,
    
    /*
     * Fans.
     */
    MakeTypeList<
        PrinterMainFanParams<
            106, // SetMCommand
            107, // OffMCommand
            At91Sam7sPin<24>, // OutputPin
            FanPulseInterval, // PulseInterval
            FanSpeedMultiply, // SpeedMultiply
            At91Sam7sClockInterruptTimer_TC2B // TimerTemplate
        >
    >
>;
/*
// need to list all used ADC pins here
using AdcPins = MakeTypeList<
    AvrPin<AvrPortA, 6>,
    AvrPin<AvrPortA, 7>
>;
*/
//static const int AdcRefSel = 1;
//static const int AdcPrescaler = 7;
static const int clock_timer_prescaler = 4;

struct MyContext;
struct EventLoopParams;
struct PrinterPosition;

using MyDebugObjectGroup = DebugObjectGroup<MyContext>;
using MyClock = At91Sam7sClock<MyContext, clock_timer_prescaler>;
using MyLoop = BusyEventLoop<EventLoopParams>;
using MyPins = At91Sam7sPins<MyContext>;
//using MyAdc = AvrAdc<MyContext, AdcPins, AdcRefSel, AdcPrescaler>;
using MyPrinter = PrinterMain<PrinterPosition, MyContext, PrinterParams>;

struct MyContext {
    using DebugGroup = MyDebugObjectGroup;
    using Lock = InterruptLock<MyContext>;
    using Clock = MyClock;
    using EventLoop = MyLoop;
    using Pins = MyPins;
//    using Adc = MyAdc;
    using TheRootPosition = PrinterPosition;
    
    MyDebugObjectGroup * debugGroup () const;
    MyClock * clock () const;
    MyLoop * eventLoop () const;
    MyPins * pins () const;
//    MyAdc * adc () const;
    MyPrinter * root () const;
};

struct EventLoopParams {
    typedef MyContext Context;
};

struct PrinterPosition : public RootPosition<MyPrinter> {};

static MyDebugObjectGroup d_group;
static MyClock myclock;
static MyLoop myloop;
static MyPins mypins;
//static MyAdc myadc;
static MyPrinter myprinter;

MyDebugObjectGroup * MyContext::debugGroup () const { return &d_group; }
MyClock * MyContext::clock () const { return &myclock; }
MyLoop * MyContext::eventLoop () const { return &myloop; }
MyPins * MyContext::pins () const { return &mypins; }
//MyAdc * MyContext::adc () const { return &myadc; }
MyPrinter * MyContext::root () const { return &myprinter; }

AMBRO_AT91SAM7S_CLOCK_GLOBAL(myclock, MyContext())
//AMBRO_AVR_ADC_ISRS(myadc, MyContext())
AMBRO_AT91SAM7S_SERIAL_GLOBAL(*myprinter.getSerial(), MyContext())
AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC0A_GLOBAL(*myprinter.getAxisStepper<0>()->getTimer(), MyContext())
AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC0B_GLOBAL(*myprinter.getAxisStepper<1>()->getTimer(), MyContext())
AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC0C_GLOBAL(*myprinter.getAxisStepper<2>()->getTimer(), MyContext())
AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC1A_GLOBAL(*myprinter.getAxisStepper<3>()->getTimer(), MyContext())
AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC1B_UNUSED_GLOBAL
AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC1C_UNUSED_GLOBAL
//AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC1B_GLOBAL(*myprinter.getHeaterTimer<0>(), MyContext())
//AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC1C_GLOBAL(*myprinter.getHeaterTimer<1>(), MyContext())
AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC2A_GLOBAL(*myprinter.getEventChannelTimer(), MyContext())
AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC2B_GLOBAL(*myprinter.getFanTimer<0>(), MyContext())
AMBRO_AT91SAM7S_CLOCK_INTERRUPT_TIMER_TC2C_UNUSED_GLOBAL
//AMBRO_AVR_WATCHDOG_GLOBAL

extern "C" {

__attribute__((used))
int _read (int file, char *ptr, int len)
{
    return -1;
}

__attribute__((used))
int _write (int file, char *ptr, int len)
{
    myprinter.getSerial()->sendWaitFinished();
    for (int i = 0; i < len; i++) {
        while (!(AT91C_BASE_US0->US_CSR & AT91C_US_TXRDY));
        AT91C_BASE_US0->US_THR = *(uint8_t *)&ptr[i];
    }
    return len;
}

__attribute__((used))
int _close (int file)
{
    return -1;
}

__attribute__((used))
int _fstat (int file, struct stat * st)
{
    return -1;
}

__attribute__((used))
int _isatty (int fd)
{
    return 1;
}

__attribute__((used))
int _lseek (int file, int ptr, int dir)
{
    return -1;
}

}

static void emergency (void)
{
    MyPrinter::emergency();
}

int main ()
{
    MyContext c;
    
    d_group.init(c);
    myclock.init(c, true, true);
    myloop.init(c);
    mypins.init(c);
    //myadc.init(c);
    myprinter.init(c);
    
    myloop.run(c);
}
