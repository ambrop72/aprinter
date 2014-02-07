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

#include <aprinter/platform/at91sam3x/at91sam3x_support.h>

static void emergency (void);

#define AMBROLIB_EMERGENCY_ACTION { cli(); emergency(); }
#define AMBROLIB_ABORT_ACTION { while (1); }

#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/Position.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/system/BusyEventLoop.h>
#include <aprinter/system/At91Sam3xClock.h>
#include <aprinter/system/At91Sam3xPins.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/system/At91Sam3xAdc.h>
#include <aprinter/system/At91Sam3xWatchdog.h>
#include <aprinter/system/At91Sam3xSerial.h>
#include <aprinter/system/At91Sam3xSpi.h>
#include <aprinter/system/AsfUsbSerial.h>
#include <aprinter/devices/SpiSdCard.h>
#include <aprinter/printer/PrinterMain.h>
#include <aprinter/printer/temp_control/PidControl.h>
#include <aprinter/printer/temp_control/BinaryControl.h>
#include <aprinter/printer/arduino_due_pins.h>
#include <generated/AvrThermistorTable_Extruder.h>
#include <generated/AvrThermistorTable_Bed.h>

using namespace APrinter;

using AdcFreq = AMBRO_WRAP_DOUBLE(1000000.0);
using AdcAvgInterval = AMBRO_WRAP_DOUBLE(0.0025);
static uint16_t const AdcSmoothing = 0.95 * 65536.0;

using LedBlinkInterval = AMBRO_WRAP_DOUBLE(0.5);
using DefaultInactiveTime = AMBRO_WRAP_DOUBLE(60.0);
using SpeedLimitMultiply = AMBRO_WRAP_DOUBLE(1.0 / 60.0);
using MaxStepsPerCycle = AMBRO_WRAP_DOUBLE(0.0017);
using ForceTimeout = AMBRO_WRAP_DOUBLE(0.1);
using TheAxisStepperPrecisionParams = AxisStepperDuePrecisionParams;

using XDefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(80.0);
using XDefaultMin = AMBRO_WRAP_DOUBLE(-53.0);
using XDefaultMax = AMBRO_WRAP_DOUBLE(210.0);
using XDefaultMaxSpeed = AMBRO_WRAP_DOUBLE(300.0);
using XDefaultMaxAccel = AMBRO_WRAP_DOUBLE(1500.0);
using XDefaultDistanceFactor = AMBRO_WRAP_DOUBLE(1.0);
using XDefaultCorneringDistance = AMBRO_WRAP_DOUBLE(40.0);
using XDefaultHomeFastMaxDist = AMBRO_WRAP_DOUBLE(280.0);
using XDefaultHomeRetractDist = AMBRO_WRAP_DOUBLE(3.0);
using XDefaultHomeSlowMaxDist = AMBRO_WRAP_DOUBLE(5.0);
using XDefaultHomeFastSpeed = AMBRO_WRAP_DOUBLE(40.0);
using XDefaultHomeRetractSpeed = AMBRO_WRAP_DOUBLE(50.0);
using XDefaultHomeSlowSpeed = AMBRO_WRAP_DOUBLE(5.0);

using YDefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(80.0);
using YDefaultMin = AMBRO_WRAP_DOUBLE(0.0);
using YDefaultMax = AMBRO_WRAP_DOUBLE(155.0);
using YDefaultMaxSpeed = AMBRO_WRAP_DOUBLE(300.0);
using YDefaultMaxAccel = AMBRO_WRAP_DOUBLE(650.0);
using YDefaultDistanceFactor = AMBRO_WRAP_DOUBLE(1.0);
using YDefaultCorneringDistance = AMBRO_WRAP_DOUBLE(40.0);
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
using ZDefaultCorneringDistance = AMBRO_WRAP_DOUBLE(40.0);
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
using EDefaultCorneringDistance = AMBRO_WRAP_DOUBLE(40.0);

using UDefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(660.0);
using UDefaultMin = AMBRO_WRAP_DOUBLE(-40000.0);
using UDefaultMax = AMBRO_WRAP_DOUBLE(40000.0);
using UDefaultMaxSpeed = AMBRO_WRAP_DOUBLE(45.0);
using UDefaultMaxAccel = AMBRO_WRAP_DOUBLE(250.0);
using UDefaultDistanceFactor = AMBRO_WRAP_DOUBLE(1.0);
using UDefaultCorneringDistance = AMBRO_WRAP_DOUBLE(40.0);

using ExtruderHeaterMinSafeTemp = AMBRO_WRAP_DOUBLE(20.0);
using ExtruderHeaterMaxSafeTemp = AMBRO_WRAP_DOUBLE(280.0);
using ExtruderHeaterPulseInterval = AMBRO_WRAP_DOUBLE(0.2);
using ExtruderHeaterControlInterval = ExtruderHeaterPulseInterval;
using ExtruderHeaterPidP = AMBRO_WRAP_DOUBLE(0.047);
using ExtruderHeaterPidI = AMBRO_WRAP_DOUBLE(0.0006);
using ExtruderHeaterPidD = AMBRO_WRAP_DOUBLE(0.17);
using ExtruderHeaterPidIStateMin = AMBRO_WRAP_DOUBLE(0.0);
using ExtruderHeaterPidIStateMax = AMBRO_WRAP_DOUBLE(0.4);
using ExtruderHeaterPidDHistory = AMBRO_WRAP_DOUBLE(0.7);
using ExtruderHeaterObserverInterval = AMBRO_WRAP_DOUBLE(0.5);
using ExtruderHeaterObserverTolerance = AMBRO_WRAP_DOUBLE(3.0);
using ExtruderHeaterObserverMinTime = AMBRO_WRAP_DOUBLE(3.0);

using UxtruderHeaterMinSafeTemp = AMBRO_WRAP_DOUBLE(20.0);
using UxtruderHeaterMaxSafeTemp = AMBRO_WRAP_DOUBLE(280.0);
using UxtruderHeaterPulseInterval = AMBRO_WRAP_DOUBLE(0.2);
using UxtruderHeaterControlInterval = UxtruderHeaterPulseInterval;
using UxtruderHeaterPidP = AMBRO_WRAP_DOUBLE(0.047);
using UxtruderHeaterPidI = AMBRO_WRAP_DOUBLE(0.0006);
using UxtruderHeaterPidD = AMBRO_WRAP_DOUBLE(0.17);
using UxtruderHeaterPidIStateMin = AMBRO_WRAP_DOUBLE(0.0);
using UxtruderHeaterPidIStateMax = AMBRO_WRAP_DOUBLE(0.4);
using UxtruderHeaterPidDHistory = AMBRO_WRAP_DOUBLE(0.7);
using UxtruderHeaterObserverInterval = AMBRO_WRAP_DOUBLE(0.5);
using UxtruderHeaterObserverTolerance = AMBRO_WRAP_DOUBLE(3.0);
using UxtruderHeaterObserverMinTime = AMBRO_WRAP_DOUBLE(3.0);

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

using ProbeOffsetX = AMBRO_WRAP_DOUBLE(-18.0);
using ProbeOffsetY = AMBRO_WRAP_DOUBLE(-31.0);
using ProbeStartHeight = AMBRO_WRAP_DOUBLE(17.0);
using ProbeLowHeight = AMBRO_WRAP_DOUBLE(5.0);
using ProbeRetractDist = AMBRO_WRAP_DOUBLE(1.0);
using ProbeMoveSpeed = AMBRO_WRAP_DOUBLE(120.0);
using ProbeFastSpeed = AMBRO_WRAP_DOUBLE(2.0);
using ProbeRetractSpeed = AMBRO_WRAP_DOUBLE(3.0);
using ProbeSlowSpeed = AMBRO_WRAP_DOUBLE(0.6);
using ProbeP1X = AMBRO_WRAP_DOUBLE(0.0);
using ProbeP1Y = AMBRO_WRAP_DOUBLE(31.0);
using ProbeP2X = AMBRO_WRAP_DOUBLE(0.0);
using ProbeP2Y = AMBRO_WRAP_DOUBLE(155.0);
using ProbeP3X = AMBRO_WRAP_DOUBLE(205.0);
using ProbeP3Y = AMBRO_WRAP_DOUBLE(83.0);

using PrinterParams = PrinterMainParams<
    /*
     * Common parameters.
     */
    PrinterMainSerialParams<
        UINT32_C(250000), // BaudRate,
        8, // RecvBufferSizeExp
        9, // SendBufferSizeExp
        GcodeParserParams<16>, // ReceiveBufferSizeExp
#ifdef USB_SERIAL
        AsfUsbSerial,
        AsfUsbSerialParams
#else
        At91Sam3xSerial,
        At91Sam3xSerialParams
#endif
    >,
    DuePin13, // LedPin
    LedBlinkInterval, // LedBlinkInterval
    DefaultInactiveTime, // DefaultInactiveTime
    SpeedLimitMultiply, // SpeedLimitMultiply
    MaxStepsPerCycle, // MaxStepsPerCycle
    32, // StepperSegmentBufferSize
    32, // EventChannelBufferSize
    28, // LookaheadBufferSize
    10, // LookaheadCommitCount
    ForceTimeout, // ForceTimeout
    double, // FpType
    At91Sam3xClockInterruptTimer_TC0A, // EventChannelTimer
    At91Sam3xWatchdog,
    At91Sam3xWatchdogParams<260>,
    PrinterMainSdCardParams<
        SpiSdCard,
        SpiSdCardParams<
            DuePin4, // SsPin
            At91Sam3xSpi
        >,
        FileGcodeParser, // BINARY: BinaryGcodeParser
        GcodeParserParams<8>, // BINARY: BinaryGcodeParserParams<8>
        2, // BufferBlocks
        256 // MaxCommandSize. BINARY: 43
    >,
    PrinterMainProbeParams<
        MakeTypeList<WrapInt<'X'>, WrapInt<'Y'>>, // PlatformAxesList
        'Z', // ProbeAxis
        DuePin34, // ProbePin,
        At91Sam3xPinInputModePullUp, // ProbePinInputMode
        false, // ProbeInvert,
        MakeTypeList<ProbeOffsetX, ProbeOffsetY>, // ProbePlatformOffset
        ProbeStartHeight,
        ProbeLowHeight,
        ProbeRetractDist,
        ProbeMoveSpeed,
        ProbeFastSpeed,
        ProbeRetractSpeed,
        ProbeSlowSpeed,
        MakeTypeList< // ProbePoints
            MakeTypeList<ProbeP1X, ProbeP1Y>,
            MakeTypeList<ProbeP2X, ProbeP2Y>,
            MakeTypeList<ProbeP3X, ProbeP3Y>
        >
    >,
    
    /*
     * Axes.
     */
    MakeTypeList<
        PrinterMainAxisParams<
            'X', // Name
            DuePinA8, // DirPin
            DuePinA9, // StepPin
            DuePin48, // EnablePin
            true, // InvertDir
            XDefaultStepsPerUnit, // StepsPerUnit
            XDefaultMin, // Min
            XDefaultMax, // Max
            XDefaultMaxSpeed, // MaxSpeed
            XDefaultMaxAccel, // MaxAccel
            XDefaultDistanceFactor, // DistanceFactor
            XDefaultCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                DuePin22, // HomeEndPin
                At91Sam3xPinInputModePullUp, // HomeEndPinInputMode
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
            32, // StepBits
            AxisStepperParams<
                At91Sam3xClockInterruptTimer_TC1A, // StepperTimer,
                TheAxisStepperPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >,
        PrinterMainAxisParams<
            'Y', // Name
            DuePinA10, // DirPin
            DuePinA11, // StepPin
            DuePin46, // EnablePin
            true, // InvertDir
            YDefaultStepsPerUnit, // StepsPerUnit
            YDefaultMin, // Min
            YDefaultMax, // Max
            YDefaultMaxSpeed, // MaxSpeed
            YDefaultMaxAccel, // MaxAccel
            YDefaultDistanceFactor, // DistanceFactor
            YDefaultCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                DuePin24, // HomeEndPin
                At91Sam3xPinInputModePullUp, // HomeEndPinInputMode
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
            32, // StepBits
            AxisStepperParams<
                At91Sam3xClockInterruptTimer_TC2A, // StepperTimer
                TheAxisStepperPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >,
        PrinterMainAxisParams<
            'Z', // Name
            DuePinA12, // DirPin
            DuePinA13, // StepPin
            DuePin44, // EnablePin
            false, // InvertDir
            ZDefaultStepsPerUnit, // StepsPerUnit
            ZDefaultMin, // Min
            ZDefaultMax, // Max
            ZDefaultMaxSpeed, // MaxSpeed
            ZDefaultMaxAccel, // MaxAccel
            ZDefaultDistanceFactor, // DistanceFactor
            ZDefaultCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                DuePin26, // HomeEndPin
                At91Sam3xPinInputModePullUp, // HomeEndPinInputMode
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
            32, // StepBits
            AxisStepperParams<
                At91Sam3xClockInterruptTimer_TC3A, // StepperTimer
                TheAxisStepperPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >,
        PrinterMainAxisParams<
            'E', // Name
            DuePin28, // DirPin
            DuePin36, // StepPin
            DuePin42, // EnablePin
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
                At91Sam3xClockInterruptTimer_TC4A, // StepperTimer
                TheAxisStepperPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >,
        PrinterMainAxisParams<
            'U', // Name
            DuePin41, // DirPin
            DuePin43, // StepPin
            DuePin39, // EnablePin
            true, // InvertDir
            UDefaultStepsPerUnit, // StepsPerUnit
            UDefaultMin, // Min
            UDefaultMax, // Max
            UDefaultMaxSpeed, // MaxSpeed
            UDefaultMaxAccel, // MaxAccel
            UDefaultDistanceFactor, // DistanceFactor
            UDefaultCorneringDistance, // CorneringDistance
            PrinterMainNoHomingParams,
            false, // EnableCartesianSpeedLimit
            32, // StepBits
            AxisStepperParams<
                At91Sam3xClockInterruptTimer_TC8A, // StepperTimer
                TheAxisStepperPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >
    >,
    
    /*
     * Transform and virtual axes.
     */
    PrinterMainNoTransformParams,
    
    /*
     * Heaters.
     */
    MakeTypeList<
        PrinterMainHeaterParams<
            'T', // Name
            104, // SetMCommand
            109, // WaitMCommand
            301, // SetConfigMCommand
            DuePinA1, // AdcPin
            DuePin9, // OutputPin
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
            At91Sam3xClockInterruptTimer_TC5A // TimerTemplate
        >,
        PrinterMainHeaterParams<
            'B', // Name
            140, // SetMCommand
            190, // WaitMCommand
            304, // SetConfigMCommand
            DuePinA0, // AdcPin
            DuePin8, // OutputPin
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
            At91Sam3xClockInterruptTimer_TC5B // TimerTemplate
        >,
        PrinterMainHeaterParams<
            'U', // Name
            404, // SetMCommand
            409, // WaitMCommand
            402, // SetConfigMCommand
            DuePinA2, // AdcPin
            DuePin10, // OutputPin
            true, // OutputInvert
            AvrThermistorTable_Extruder, // Formula
            UxtruderHeaterMinSafeTemp, // MinSafeTemp
            UxtruderHeaterMaxSafeTemp, // MaxSafeTemp
            UxtruderHeaterPulseInterval, // PulseInterval
            UxtruderHeaterControlInterval, // ControlInterval
            PidControl, // Control
            PidControlParams<
                UxtruderHeaterPidP, // PidP
                UxtruderHeaterPidI, // PidI
                UxtruderHeaterPidD, // PidD
                UxtruderHeaterPidIStateMin, // PidIStateMin
                UxtruderHeaterPidIStateMax, // PidIStateMax
                UxtruderHeaterPidDHistory // PidDHistory
            >,
            TemperatureObserverParams<
                UxtruderHeaterObserverInterval, // ObserverInterval
                UxtruderHeaterObserverTolerance, // ObserverTolerance
                UxtruderHeaterObserverMinTime // ObserverMinTime
            >,
            At91Sam3xClockInterruptTimer_TC6A // TimerTemplate
        >
    >,
    
    /*
     * Fans.
     */
    MakeTypeList<
        PrinterMainFanParams<
            106, // SetMCommand
            107, // OffMCommand
            DuePin12, // OutputPin
            false, // OutputInvert
            FanPulseInterval, // PulseInterval
            FanSpeedMultiply, // SpeedMultiply
            At91Sam3xClockInterruptTimer_TC6B // TimerTemplate
        >,
        PrinterMainFanParams<
            406, // SetMCommand
            407, // OffMCommand
            DuePin2, // OutputPin
            false, // OutputInvert
            FanPulseInterval, // PulseInterval
            FanSpeedMultiply, // SpeedMultiply
            At91Sam3xClockInterruptTimer_TC7A // TimerTemplate
        >
    >
>;

// need to list all used ADC pins here
using AdcPins = MakeTypeList<
    At91Sam3xAdcSmoothPin<DuePinA1, AdcSmoothing>,
    At91Sam3xAdcSmoothPin<DuePinA0, AdcSmoothing>,
    At91Sam3xAdcSmoothPin<DuePinA2, AdcSmoothing>
>;

using AdcParams = At91Sam3xAdcParams<
    AdcFreq,
    8, // AdcStartup
    3, // AdcSettling
    0, // AdcTracking
    1, // AdcTransfer
    At91Sam3xAdcAvgParams<AdcAvgInterval>
>;

static const int clock_timer_prescaler = 3;
using ClockTcsList = MakeTypeList<
    At91Sam3xClockTC0,
    At91Sam3xClockTC1,
    At91Sam3xClockTC2,
    At91Sam3xClockTC3,
    At91Sam3xClockTC4,
    At91Sam3xClockTC5,
    At91Sam3xClockTC6,
    At91Sam3xClockTC7,
    At91Sam3xClockTC8
>;

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
using MyClock = At91Sam3xClock<ClockPosition, MyContext, clock_timer_prescaler, ClockTcsList>;
using MyLoop = BusyEventLoop<LoopPosition, LoopExtraPosition, MyContext, MyLoopExtra>;
using MyPins = At91Sam3xPins<PinsPosition, MyContext>;
using MyAdc = At91Sam3xAdc<AdcPosition, MyContext, AdcPins, AdcParams>;
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

AMBRO_AT91SAM3X_CLOCK_TC0_GLOBAL(p.myclock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC1_GLOBAL(p.myclock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC2_GLOBAL(p.myclock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC3_GLOBAL(p.myclock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC4_GLOBAL(p.myclock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC5_GLOBAL(p.myclock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC6_GLOBAL(p.myclock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC7_GLOBAL(p.myclock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC8_GLOBAL(p.myclock, MyContext())

AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC0A_GLOBAL(*p.myprinter.getEventChannelTimer(), MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC1A_GLOBAL(*p.myprinter.getAxisStepper<0>()->getTimer(), MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC2A_GLOBAL(*p.myprinter.getAxisStepper<1>()->getTimer(), MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC3A_GLOBAL(*p.myprinter.getAxisStepper<2>()->getTimer(), MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC4A_GLOBAL(*p.myprinter.getAxisStepper<3>()->getTimer(), MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC8A_GLOBAL(*p.myprinter.getAxisStepper<4>()->getTimer(), MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC5A_GLOBAL(*p.myprinter.getHeaterTimer<0>(), MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC5B_GLOBAL(*p.myprinter.getHeaterTimer<1>(), MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC6A_GLOBAL(*p.myprinter.getHeaterTimer<2>(), MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC6B_GLOBAL(*p.myprinter.getFanTimer<0>(), MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_TC7A_GLOBAL(*p.myprinter.getFanTimer<1>(), MyContext())

#ifndef USB_SERIAL
AMBRO_AT91SAM3X_SERIAL_GLOBAL(*p.myprinter.getSerial(), MyContext())
#endif
AMBRO_AT91SAM3X_SPI_GLOBAL(*p.myprinter.getSdCard()->getSpi(), MyContext())
AMBRO_AT91SAM3X_ADC_GLOBAL(p.myadc, MyContext())

static void emergency (void)
{
    MyPrinter::emergency();
}

extern "C" {
    __attribute__((used))
    int _read (int file, char *ptr, int len)
    {
        return -1;
    }
    
#ifndef USB_SERIAL
    __attribute__((used))
    int _write (int file, char *ptr, int len)
    {
        if (interrupts_enabled()) {
            p.myprinter.getSerial()->sendWaitFinished(MyContext());
        }
        for (int i = 0; i < len; i++) {
            while (!(UART->UART_SR & UART_SR_TXRDY));
            UART->UART_THR = *(uint8_t *)&ptr[i];
        }
        return len;
    }
#endif
    
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

int main ()
{
    platform_init();
    
    MyContext c;
    
    p.d_group.init(c);
    p.myclock.init(c);
    p.myloop.init(c);
    p.mypins.init(c);
    p.myadc.init(c);
    p.myprinter.init(c);
    
    p.myloop.run(c);
}
