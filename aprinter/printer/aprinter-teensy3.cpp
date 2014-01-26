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

#include <aprinter/platform/teensy3/teensy3_support.h>

static void emergency (void);

#define AMBROLIB_EMERGENCY_ACTION { cli(); emergency(); }
#define AMBROLIB_ABORT_ACTION { while (1); }

#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/Position.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/system/BusyEventLoop.h>
#include <aprinter/system/Mk20Clock.h>
#include <aprinter/system/Mk20Pins.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/system/Mk20Adc.h>
#include <aprinter/system/Mk20Watchdog.h>
#include <aprinter/system/TeensyUsbSerial.h>
#include <aprinter/devices/SpiSdCard.h>
#include <aprinter/printer/PrinterMain.h>
#include <aprinter/printer/PidControl.h>
#include <aprinter/printer/BinaryControl.h>
#include <aprinter/printer/DeltaTransform.h>
#include <aprinter/printer/teensy3_pins.h>
#include <generated/AvrThermistorTable_Extruder.h>
#include <generated/AvrThermistorTable_Bed.h>

using namespace APrinter;

static int const AdcADiv = 3;

using LedBlinkInterval = AMBRO_WRAP_DOUBLE(0.5);
using DefaultInactiveTime = AMBRO_WRAP_DOUBLE(60.0);
using SpeedLimitMultiply = AMBRO_WRAP_DOUBLE(1.0 / 60.0);
using MaxStepsPerCycle = AMBRO_WRAP_DOUBLE(0.0017);
using ForceTimeout = AMBRO_WRAP_DOUBLE(0.1);
using TheAxisStepperPrecisionParams = AxisStepperDuePrecisionParams;

using ABCDefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(100.0);
using ABCDefaultMin = AMBRO_WRAP_DOUBLE(0.0);
using ABCDefaultMax = AMBRO_WRAP_DOUBLE(360.0);
using ABCDefaultMaxSpeed = AMBRO_WRAP_DOUBLE(200.0);
using ABCDefaultMaxAccel = AMBRO_WRAP_DOUBLE(9000.0);
using ABCDefaultDistanceFactor = AMBRO_WRAP_DOUBLE(1.0);
using ABCDefaultCorneringDistance = AMBRO_WRAP_DOUBLE(40.0);
using ABCDefaultHomeFastMaxDist = AMBRO_WRAP_DOUBLE(363.0);
using ABCDefaultHomeRetractDist = AMBRO_WRAP_DOUBLE(3.0);
using ABCDefaultHomeSlowMaxDist = AMBRO_WRAP_DOUBLE(5.0);
using ABCDefaultHomeFastSpeed = AMBRO_WRAP_DOUBLE(70.0);
using ABCDefaultHomeRetractSpeed = AMBRO_WRAP_DOUBLE(70.0);
using ABCDefaultHomeSlowSpeed = AMBRO_WRAP_DOUBLE(5.0);

using EDefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(928.0);
using EDefaultMin = AMBRO_WRAP_DOUBLE(-40000.0);
using EDefaultMax = AMBRO_WRAP_DOUBLE(40000.0);
using EDefaultMaxSpeed = AMBRO_WRAP_DOUBLE(45.0);
using EDefaultMaxAccel = AMBRO_WRAP_DOUBLE(250.0);
using EDefaultDistanceFactor = AMBRO_WRAP_DOUBLE(1.0);
using EDefaultCorneringDistance = AMBRO_WRAP_DOUBLE(40.0);

using ExtruderHeaterMinSafeTemp = AMBRO_WRAP_DOUBLE(20.0);
using ExtruderHeaterMaxSafeTemp = AMBRO_WRAP_DOUBLE(280.0);
using ExtruderHeaterPulseInterval = AMBRO_WRAP_DOUBLE(0.2);
using ExtruderHeaterControlInterval = ExtruderHeaterPulseInterval;
using ExtruderHeaterPidP = AMBRO_WRAP_DOUBLE(0.047);
using ExtruderHeaterPidI = AMBRO_WRAP_DOUBLE(0.0006);
using ExtruderHeaterPidD = AMBRO_WRAP_DOUBLE(0.17);
using ExtruderHeaterPidIStateMin = AMBRO_WRAP_DOUBLE(0.0);
using ExtruderHeaterPidIStateMax = AMBRO_WRAP_DOUBLE(0.2);
using ExtruderHeaterPidDHistory = AMBRO_WRAP_DOUBLE(0.7);
using ExtruderHeaterObserverInterval = AMBRO_WRAP_DOUBLE(0.5);
using ExtruderHeaterObserverTolerance = AMBRO_WRAP_DOUBLE(3.0);
using ExtruderHeaterObserverMinTime = AMBRO_WRAP_DOUBLE(3.0);

using BedHeaterMinSafeTemp = AMBRO_WRAP_DOUBLE(20.0);
using BedHeaterMaxSafeTemp = AMBRO_WRAP_DOUBLE(120.0);
using BedHeaterPulseInterval = AMBRO_WRAP_DOUBLE(0.3);
using BedHeaterControlInterval = AMBRO_WRAP_DOUBLE(0.3);
using BedHeaterPidP = AMBRO_WRAP_DOUBLE(1.0);
using BedHeaterPidI = AMBRO_WRAP_DOUBLE(0.012);
using BedHeaterPidD = AMBRO_WRAP_DOUBLE(2.5);
using BedHeaterPidIStateMin = AMBRO_WRAP_DOUBLE(0.0);
using BedHeaterPidIStateMax = AMBRO_WRAP_DOUBLE(1.0);
using BedHeaterPidDHistory = AMBRO_WRAP_DOUBLE(0.8);
using BedHeaterObserverInterval = AMBRO_WRAP_DOUBLE(0.5);
using BedHeaterObserverTolerance = AMBRO_WRAP_DOUBLE(1.5);
using BedHeaterObserverMinTime = AMBRO_WRAP_DOUBLE(3.0);

using FanSpeedMultiply = AMBRO_WRAP_DOUBLE(1.0 / 255.0);
using FanPulseInterval = AMBRO_WRAP_DOUBLE(0.04);

using DeltaDiagonalRod = AMBRO_WRAP_DOUBLE(214.0);
using DeltaSmoothRodOffset = AMBRO_WRAP_DOUBLE(145.0);
using DeltaEffectorOffset = AMBRO_WRAP_DOUBLE(19.9);
using DeltaCarriageOffset = AMBRO_WRAP_DOUBLE(19.5);
using DeltaRadius = AMBRO_WRAP_DOUBLE(DeltaSmoothRodOffset::value() - DeltaEffectorOffset::value() - DeltaCarriageOffset::value());
using DeltaSplitLength = AMBRO_WRAP_DOUBLE(2.0);
using DeltaTower1X = AMBRO_WRAP_DOUBLE(DeltaRadius::value() * -0.8660254037844386);
using DeltaTower1Y = AMBRO_WRAP_DOUBLE(DeltaRadius::value() * -0.5);
using DeltaTower2X = AMBRO_WRAP_DOUBLE(DeltaRadius::value() * 0.8660254037844386);
using DeltaTower2Y = AMBRO_WRAP_DOUBLE(DeltaRadius::value() * -0.5);
using DeltaTower3X = AMBRO_WRAP_DOUBLE(DeltaRadius::value() * 0.0);
using DeltaTower3Y = AMBRO_WRAP_DOUBLE(DeltaRadius::value() * 1.0);

using PrinterParams = PrinterMainParams<
    /*
     * Common parameters.
     */
    PrinterMainSerialParams<
        UINT32_C(0), // BaudRate,
        8, // RecvBufferSizeExp
        9, // SendBufferSizeExp
        GcodeParserParams<16>, // ReceiveBufferSizeExp
        TeensyUsbSerial,
        TeensyUsbSerialParams
    >,
    Mk20Pin<Mk20PortC, 5>, // LedPin
    LedBlinkInterval, // LedBlinkInterval
    DefaultInactiveTime, // DefaultInactiveTime
    SpeedLimitMultiply, // SpeedLimitMultiply
    MaxStepsPerCycle, // MaxStepsPerCycle
    24, // StepperSegmentBufferSize
    24, // EventChannelBufferSize
    16, // LookaheadBufferSize
    6, // LookaheadCommitCount
    ForceTimeout, // ForceTimeout
    Mk20ClockInterruptTimer_Ftm0_Ch0, // EventChannelTimer
    Mk20Watchdog,
    Mk20WatchdogParams<2000, 0>,
    PrinterMainNoSdCardParams,
    PrinterMainNoProbeParams,
    
    /*
     * Axes.
     */
    MakeTypeList<
        PrinterMainAxisParams<
            'A', // Name
            Mk20Pin<Mk20PortD, 0>, // DirPin
            Mk20Pin<Mk20PortB, 17>, // StepPin
            Mk20Pin<Mk20PortB, 16>, // EnablePin
            true, // InvertDir
            ABCDefaultStepsPerUnit, // StepsPerUnit
            ABCDefaultMin, // Min
            ABCDefaultMax, // Max
            ABCDefaultMaxSpeed, // MaxSpeed
            ABCDefaultMaxAccel, // MaxAccel
            ABCDefaultDistanceFactor, // DistanceFactor
            ABCDefaultCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                Mk20Pin<Mk20PortC, 3>, // HomeEndPin
                false, // HomeEndInvert
                true, // HomeDir
                ABCDefaultHomeFastMaxDist, // HomeFastMaxDist
                ABCDefaultHomeRetractDist, // HomeRetractDist
                ABCDefaultHomeSlowMaxDist, // HomeSlowMaxDist
                ABCDefaultHomeFastSpeed, // HomeFastSpeed
                ABCDefaultHomeRetractSpeed, // HomeRetractSpeed
                ABCDefaultHomeSlowSpeed // HomeSlowSpeed
            >,
            false, // EnableCartesianSpeedLimit
            32, // StepBits
            AxisStepperParams<
                Mk20ClockInterruptTimer_Ftm0_Ch1, // StepperTimer,
                TheAxisStepperPrecisionParams // PrecisionParams
            >
        >,
        PrinterMainAxisParams<
            'B', // Name
            Mk20Pin<Mk20PortD, 7>, // DirPin
            Mk20Pin<Mk20PortA, 13>, // StepPin
            Mk20Pin<Mk20PortA, 12>, // EnablePin
            true, // InvertDir
            ABCDefaultStepsPerUnit, // StepsPerUnit
            ABCDefaultMin, // Min
            ABCDefaultMax, // Max
            ABCDefaultMaxSpeed, // MaxSpeed
            ABCDefaultMaxAccel, // MaxAccel
            ABCDefaultDistanceFactor, // DistanceFactor
            ABCDefaultCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                Mk20Pin<Mk20PortC, 4>, // HomeEndPin
                false, // HomeEndInvert
                true, // HomeDir
                ABCDefaultHomeFastMaxDist, // HomeFastMaxDist
                ABCDefaultHomeRetractDist, // HomeRetractDist
                ABCDefaultHomeSlowMaxDist, // HomeSlowMaxDist
                ABCDefaultHomeFastSpeed, // HomeFastSpeed
                ABCDefaultHomeRetractSpeed, // HomeRetractSpeed
                ABCDefaultHomeSlowSpeed // HomeSlowSpeed
            >,
            false, // EnableCartesianSpeedLimit
            32, // StepBits
            AxisStepperParams<
                Mk20ClockInterruptTimer_Ftm0_Ch2, // StepperTimer
                TheAxisStepperPrecisionParams // PrecisionParams
            >
        >,
        PrinterMainAxisParams<
            'C', // Name
            Mk20Pin<Mk20PortD, 3>, // DirPin
            Mk20Pin<Mk20PortD, 2>, // StepPin
            Mk20Pin<Mk20PortD, 4>, // EnablePin
            false, // InvertDir
            ABCDefaultStepsPerUnit, // StepsPerUnit
            ABCDefaultMin, // Min
            ABCDefaultMax, // Max
            ABCDefaultMaxSpeed, // MaxSpeed
            ABCDefaultMaxAccel, // MaxAccel
            ABCDefaultDistanceFactor, // DistanceFactor
            ABCDefaultCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                Mk20Pin<Mk20PortC, 6>, // HomeEndPin
                false, // HomeEndInvert
                true, // HomeDir
                ABCDefaultHomeFastMaxDist, // HomeFastMaxDist
                ABCDefaultHomeRetractDist, // HomeRetractDist
                ABCDefaultHomeSlowMaxDist, // HomeSlowMaxDist
                ABCDefaultHomeFastSpeed, // HomeFastSpeed
                ABCDefaultHomeRetractSpeed, // HomeRetractSpeed
                ABCDefaultHomeSlowSpeed // HomeSlowSpeed
            >,
            false, // EnableCartesianSpeedLimit
            32, // StepBits
            AxisStepperParams<
                Mk20ClockInterruptTimer_Ftm0_Ch3, // StepperTimer
                TheAxisStepperPrecisionParams // PrecisionParams
            >
        >,
        PrinterMainAxisParams<
            'E', // Name
            Mk20Pin<Mk20PortC, 0>, // DirPin
            Mk20Pin<Mk20PortD, 1>, // StepPin
            Mk20Pin<Mk20PortC, 7>, // EnablePin
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
                Mk20ClockInterruptTimer_Ftm0_Ch4, // StepperTimer
                TheAxisStepperPrecisionParams // PrecisionParams
            >
        >
    >,
    
    /*
     * Transform and virtual axes.
     */
    PrinterMainTransformParams<
        MakeTypeList<WrapInt<'X'>, WrapInt<'Y'>, WrapInt<'Z'>>,
        MakeTypeList<WrapInt<'A'>, WrapInt<'B'>, WrapInt<'C'>>,
        DeltaTransform,
        DeltaTransformParams<
            DeltaDiagonalRod,
            DeltaTower1X,
            DeltaTower1Y,
            DeltaTower2X,
            DeltaTower2Y,
            DeltaTower3X,
            DeltaTower3Y,
            DeltaSplitLength
        >
    >,
    
    /*
     * Heaters.
     */
    MakeTypeList<
        PrinterMainHeaterParams<
            'T', // Name
            104, // SetMCommand
            109, // WaitMCommand
            301, // SetConfigMCommand
            TeensyPinA2, // AdcPin
            Mk20Pin<Mk20PortC, 1>, // OutputPin
            true, // OutputInvert
            AvrThermistorTable_Extruder, // Formula
            ExtruderHeaterMinSafeTemp, // MinSafeTemp
            ExtruderHeaterMaxSafeTemp, // MaxSafeTemp
            ExtruderHeaterPulseInterval, // PulseInterval
            ExtruderHeaterControlInterval, // ControlInterval
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
            Mk20ClockInterruptTimer_Ftm0_Ch5 // TimerTemplate
        >
#if 0
        PrinterMainHeaterParams<
            'B', // Name
            140, // SetMCommand
            190, // WaitMCommand
            304, // SetConfigMCommand
            NOOONE, // AdcPin
            NOOONE, // OutputPin
            true, // OutputInvert
            AvrThermistorTable_Bed, // Formula
            BedHeaterMinSafeTemp, // MinSafeTemp
            BedHeaterMaxSafeTemp, // MaxSafeTemp
            BedHeaterPulseInterval, // PulseInterval
            BedHeaterControlInterval, // ControlInterval
            PidControl, // Control
            PidControlParams<
                BedHeaterPidP, // PidP
                BedHeaterPidI, // PidI
                BedHeaterPidD, // PidD
                BedHeaterPidIStateMin, // PidIStateMin
                BedHeaterPidIStateMax, // PidIStateMax
                BedHeaterPidDHistory // PidDHistory
            >,
            TemperatureObserverParams<
                BedHeaterObserverInterval, // ObserverInterval
                BedHeaterObserverTolerance, // ObserverTolerance
                BedHeaterObserverMinTime // ObserverMinTime
            >,
            Mk20ClockInterruptTimer_Ftm0_Ch6 // TimerTemplate
        >
#endif
    >,
    
    /*
     * Fans.
     */
    MakeTypeList<
        PrinterMainFanParams<
            106, // SetMCommand
            107, // OffMCommand
            Mk20Pin<Mk20PortC, 3>, // OutputPin
            false, // OutputInvert
            FanPulseInterval, // PulseInterval
            FanSpeedMultiply, // SpeedMultiply
            Mk20ClockInterruptTimer_Ftm0_Ch7 // TimerTemplate
        >
    >
>;

// need to list all used ADC pins here
using AdcPins = MakeTypeList<TeensyPinA2>;

static const int clock_timer_prescaler = 4;
using ClockFtmsList = MakeTypeList<Mk20ClockFTM0, Mk20ClockFTM1>;

struct MyContext;
struct MyLoopExtra;
struct Program;
struct ClockPosition;
struct LoopPosition;
struct PinsPosition;
struct AdcPosition;
struct PrinterPosition;
struct LoopExtraPosition;

using ProgramPosition = RootPosition<Program>;
using MyDebugObjectGroup = DebugObjectGroup<MyContext>;
using MyClock = Mk20Clock<ClockPosition, MyContext, clock_timer_prescaler, ClockFtmsList>;
using MyLoop = BusyEventLoop<LoopPosition, LoopExtraPosition, MyContext, MyLoopExtra>;
using MyPins = Mk20Pins<PinsPosition, MyContext>;
using MyAdc = Mk20Adc<AdcPosition, MyContext, AdcPins, AdcADiv>;
using MyPrinter = PrinterMain<PrinterPosition, MyContext, PrinterParams>;

struct MyContext {
    using DebugGroup = MyDebugObjectGroup;
    using Clock = MyClock;
    using EventLoop = MyLoop;
    using Pins = MyPins;
    using Adc = MyAdc;
    using TheRootPosition = ProgramPosition;
    
    MyDebugObjectGroup * debugGroup () const;
    MyClock * clock () const;
    MyLoop * eventLoop () const;
    MyPins * pins () const;
    MyAdc * adc () const;
    Program * root () const;
    void check () const;
};

struct MyLoopExtra : public BusyEventLoopExtra<LoopExtraPosition, MyLoop, typename MyPrinter::EventLoopFastEvents> {};

struct Program {
    MyDebugObjectGroup d_group;
    MyClock myclock;
    MyLoop myloop;
    MyPins mypins;
    MyAdc myadc;
    MyPrinter myprinter;
    MyLoopExtra myloopextra;
};

struct ClockPosition : public MemberPosition<ProgramPosition, MyClock, &Program::myclock> {};
struct LoopPosition : public MemberPosition<ProgramPosition, MyLoop, &Program::myloop> {};
struct PinsPosition : public MemberPosition<ProgramPosition, MyPins, &Program::mypins> {};
struct AdcPosition : public MemberPosition<ProgramPosition, MyAdc, &Program::myadc> {};
struct PrinterPosition : public MemberPosition<ProgramPosition, MyPrinter, &Program::myprinter> {};
struct LoopExtraPosition : public MemberPosition<ProgramPosition, MyLoopExtra, &Program::myloopextra> {};

Program p;

MyDebugObjectGroup * MyContext::debugGroup () const { return &p.d_group; }
MyClock * MyContext::clock () const { return &p.myclock; }
MyLoop * MyContext::eventLoop () const { return &p.myloop; }
MyPins * MyContext::pins () const { return &p.mypins; }
MyAdc * MyContext::adc () const { return &p.myadc; }
Program * MyContext::root () const { return &p; }
void MyContext::check () const {}

AMBRO_MK20_CLOCK_FTM0_GLOBAL(p.myclock, MyContext())
AMBRO_MK20_CLOCK_FTM1_GLOBAL(p.myclock, MyContext())

AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH0_GLOBAL(*p.myprinter.getEventChannelTimer(), MyContext())
AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH1_GLOBAL(*p.myprinter.getAxisStepper<0>()->getTimer(), MyContext())
AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH2_GLOBAL(*p.myprinter.getAxisStepper<1>()->getTimer(), MyContext())
AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH3_GLOBAL(*p.myprinter.getAxisStepper<2>()->getTimer(), MyContext())
AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH4_GLOBAL(*p.myprinter.getAxisStepper<3>()->getTimer(), MyContext())
AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH5_GLOBAL(*p.myprinter.getHeaterTimer<0>(), MyContext())
//AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH6_GLOBAL(*p.myprinter.getHeaterTimer<1>(), MyContext())
AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH7_GLOBAL(*p.myprinter.getFanTimer<0>(), MyContext())

static void emergency (void)
{
    MyPrinter::emergency();
}

extern "C" { void usb_init (void); }

int main ()
{
    usb_init();
    
    MyContext c;
    
    p.d_group.init(c);
    p.myclock.init(c);
    p.myloop.init(c);
    p.mypins.init(c);
    p.myadc.init(c);
    p.myprinter.init(c);
    
    p.myloop.run(c);
}
