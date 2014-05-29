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
#include <aprinter/meta/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/system/BusyEventLoop.h>
#include <aprinter/system/At91Sam3xClock.h>
#include <aprinter/system/At91SamPins.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/system/At91SamAdc.h>
#include <aprinter/system/At91SamWatchdog.h>
#include <aprinter/system/At91Sam3xSerial.h>
#include <aprinter/system/At91SamSpi.h>
#include <aprinter/system/AsfUsbSerial.h>
#include <aprinter/devices/SpiSdCard.h>
#include <aprinter/driver/AxisDriver.h>
#include <aprinter/printer/PrinterMain.h>
#include <aprinter/printer/pwm/SoftPwm.h>
#include <aprinter/printer/thermistor/GenericThermistor.h>
#include <aprinter/printer/temp_control/PidControl.h>
#include <aprinter/printer/temp_control/BinaryControl.h>
#include <aprinter/printer/arduino_due_pins.h>
#include <aprinter/printer/transform/DeltaTransform.h>

using namespace APrinter;

using AdcFreq = AMBRO_WRAP_DOUBLE(1000000.0);
using AdcAvgInterval = AMBRO_WRAP_DOUBLE(0.0025);
static uint16_t const AdcSmoothing = 0.95 * 65536.0;

using LedBlinkInterval = AMBRO_WRAP_DOUBLE(0.5);
using DefaultInactiveTime = AMBRO_WRAP_DOUBLE(60.0);
using SpeedLimitMultiply = AMBRO_WRAP_DOUBLE(1.0 / 60.0);
using MaxStepsPerCycle = AMBRO_WRAP_DOUBLE(0.0017);
using ForceTimeout = AMBRO_WRAP_DOUBLE(0.1);
using TheAxisDriverPrecisionParams = AxisDriverDuePrecisionParams;
using EventChannelTimerClearance = AMBRO_WRAP_DOUBLE(0.002);

using XMinPos = AMBRO_WRAP_DOUBLE(-INFINITY);
using XMaxPos = AMBRO_WRAP_DOUBLE(INFINITY);
using XMaxSpeed = AMBRO_WRAP_DOUBLE(INFINITY);

using YMinPos = AMBRO_WRAP_DOUBLE(-INFINITY);
using YMaxPos = AMBRO_WRAP_DOUBLE(INFINITY);
using YMaxSpeed = AMBRO_WRAP_DOUBLE(INFINITY);

using ZMinPos = AMBRO_WRAP_DOUBLE(-INFINITY);
using ZMaxPos = AMBRO_WRAP_DOUBLE(INFINITY);
using ZMaxSpeed = AMBRO_WRAP_DOUBLE(INFINITY);

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

using EDefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(450.0);
using EDefaultMin = AMBRO_WRAP_DOUBLE(-40000.0);
using EDefaultMax = AMBRO_WRAP_DOUBLE(40000.0);
using EDefaultMaxSpeed = AMBRO_WRAP_DOUBLE(200.0);
using EDefaultMaxAccel = AMBRO_WRAP_DOUBLE(9000.0);
using EDefaultDistanceFactor = AMBRO_WRAP_DOUBLE(1.0);
using EDefaultCorneringDistance = AMBRO_WRAP_DOUBLE(40.0);

using ExtruderHeaterThermistorResistorR = AMBRO_WRAP_DOUBLE(4700.0);
using ExtruderHeaterThermistorR0 = AMBRO_WRAP_DOUBLE(100000.0);
using ExtruderHeaterThermistorBeta = AMBRO_WRAP_DOUBLE(3960.0);
using ExtruderHeaterThermistorMinTemp = AMBRO_WRAP_DOUBLE(10.0);
using ExtruderHeaterThermistorMaxTemp = AMBRO_WRAP_DOUBLE(300.0);
using ExtruderHeaterMinSafeTemp = AMBRO_WRAP_DOUBLE(20.0);
using ExtruderHeaterMaxSafeTemp = AMBRO_WRAP_DOUBLE(280.0);
using ExtruderHeaterPulseInterval = AMBRO_WRAP_DOUBLE(0.1);
using ExtruderHeaterControlInterval = ExtruderHeaterPulseInterval;
using ExtruderHeaterPidP = AMBRO_WRAP_DOUBLE(0.047);
using ExtruderHeaterPidI = AMBRO_WRAP_DOUBLE(0.0007);
using ExtruderHeaterPidD = AMBRO_WRAP_DOUBLE(0.14);
using ExtruderHeaterPidIStateMin = AMBRO_WRAP_DOUBLE(0.0);
using ExtruderHeaterPidIStateMax = AMBRO_WRAP_DOUBLE(0.4);
using ExtruderHeaterPidDHistory = AMBRO_WRAP_DOUBLE(0.837);
using ExtruderHeaterObserverInterval = AMBRO_WRAP_DOUBLE(0.5);
using ExtruderHeaterObserverTolerance = AMBRO_WRAP_DOUBLE(3.0);
using ExtruderHeaterObserverMinTime = AMBRO_WRAP_DOUBLE(3.0);

using BedHeaterThermistorResistorR = AMBRO_WRAP_DOUBLE(4700.0);
using BedHeaterThermistorR0 = AMBRO_WRAP_DOUBLE(10000.0);
using BedHeaterThermistorBeta = AMBRO_WRAP_DOUBLE(3480.0);
using BedHeaterThermistorMinTemp = AMBRO_WRAP_DOUBLE(10.0);
using BedHeaterThermistorMaxTemp = AMBRO_WRAP_DOUBLE(150.0);
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

using ProbeOffsetX = AMBRO_WRAP_DOUBLE(0.0);
using ProbeOffsetY = AMBRO_WRAP_DOUBLE(0.0);
using ProbeStartHeight = AMBRO_WRAP_DOUBLE(17.0);
using ProbeLowHeight = AMBRO_WRAP_DOUBLE(5.0);
using ProbeRetractDist = AMBRO_WRAP_DOUBLE(1.0);
using ProbeMoveSpeed = AMBRO_WRAP_DOUBLE(120.0);
using ProbeFastSpeed = AMBRO_WRAP_DOUBLE(2.0);
using ProbeRetractSpeed = AMBRO_WRAP_DOUBLE(3.0);
using ProbeSlowSpeed = AMBRO_WRAP_DOUBLE(0.6);
using ProbeR = AMBRO_WRAP_DOUBLE(60.0);
using ProbeP1X = AMBRO_WRAP_DOUBLE(ProbeR::value() * 1.0);
using ProbeP1Y = AMBRO_WRAP_DOUBLE(ProbeR::value() * 0.0);
using ProbeP2X = AMBRO_WRAP_DOUBLE(ProbeR::value() * -0.5);
using ProbeP2Y = AMBRO_WRAP_DOUBLE(ProbeR::value() * 0.866);
using ProbeP3X = AMBRO_WRAP_DOUBLE(ProbeR::value() * -0.5);
using ProbeP3Y = AMBRO_WRAP_DOUBLE(ProbeR::value() * -0.866);

using DeltaDiagonalRod = AMBRO_WRAP_DOUBLE(214.0);
using DeltaSmoothRodOffset = AMBRO_WRAP_DOUBLE(145.0);
using DeltaEffectorOffset = AMBRO_WRAP_DOUBLE(19.9);
using DeltaCarriageOffset = AMBRO_WRAP_DOUBLE(19.5);
using DeltaRadius = AMBRO_WRAP_DOUBLE(DeltaSmoothRodOffset::value() - DeltaEffectorOffset::value() - DeltaCarriageOffset::value());
using DeltaSegmentsPerSecond = AMBRO_WRAP_DOUBLE(100.0);
using DeltaMinSplitLength = AMBRO_WRAP_DOUBLE(0.1);
using DeltaMaxSplitLength = AMBRO_WRAP_DOUBLE(4.0);
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
        UINT32_C(115200), // BaudRate,
        8, // RecvBufferSizeExp
        9, // SendBufferSizeExp
        GcodeParserParams<16>, // ReceiveBufferSizeExp
#ifdef USB_SERIAL
        AsfUsbSerialService
#else
        At91Sam3xSerialService
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
    At91Sam3xClockInterruptTimerService<At91Sam3xClockTC0, At91Sam3xClockCompA, EventChannelTimerClearance>, // EventChannelTimer
    At91SamWatchdogService<260>,
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
        At91SamPinInputModePullUp, // ProbePinInputMode
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
    PrinterMainNoCurrentParams,
    
    /*
     * Axes.
     */
    MakeTypeList<
        PrinterMainAxisParams<
            'A', // Name
            DuePinA8, // DirPin
            DuePinA9, // StepPin
            DuePin48, // EnablePin
            false, // InvertDir
            ABCDefaultStepsPerUnit, // StepsPerUnit
            ABCDefaultMin, // Min
            ABCDefaultMax, // Max
            ABCDefaultMaxSpeed, // MaxSpeed
            ABCDefaultMaxAccel, // MaxAccel
            ABCDefaultDistanceFactor, // DistanceFactor
            ABCDefaultCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                DuePin22, // HomeEndPin
                At91SamPinInputModePullUp, // HomeEndPinInputMode
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
            AxisDriverService<
                At91Sam3xClockInterruptTimerService<At91Sam3xClockTC1, At91Sam3xClockCompA>, // StepperTimer,
                TheAxisDriverPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >,
        PrinterMainAxisParams<
            'B', // Name
            DuePinA10, // DirPin
            DuePinA11, // StepPin
            DuePin46, // EnablePin
            false, // InvertDir
            ABCDefaultStepsPerUnit, // StepsPerUnit
            ABCDefaultMin, // Min
            ABCDefaultMax, // Max
            ABCDefaultMaxSpeed, // MaxSpeed
            ABCDefaultMaxAccel, // MaxAccel
            ABCDefaultDistanceFactor, // DistanceFactor
            ABCDefaultCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                DuePin24, // HomeEndPin
                At91SamPinInputModePullUp, // HomeEndPinInputMode
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
            AxisDriverService<
                At91Sam3xClockInterruptTimerService<At91Sam3xClockTC2, At91Sam3xClockCompA>, // StepperTimer
                TheAxisDriverPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >,
        PrinterMainAxisParams<
            'C', // Name
            DuePinA12, // DirPin
            DuePinA13, // StepPin
            DuePin44, // EnablePin
            false, // InvertDir
            ABCDefaultStepsPerUnit, // StepsPerUnit
            ABCDefaultMin, // Min
            ABCDefaultMax, // Max
            ABCDefaultMaxSpeed, // MaxSpeed
            ABCDefaultMaxAccel, // MaxAccel
            ABCDefaultDistanceFactor, // DistanceFactor
            ABCDefaultCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                DuePin26, // HomeEndPin
                At91SamPinInputModePullUp, // HomeEndPinInputMode
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
            AxisDriverService<
                At91Sam3xClockInterruptTimerService<At91Sam3xClockTC3, At91Sam3xClockCompA>, // StepperTimer
                TheAxisDriverPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >,
        PrinterMainAxisParams<
            'E', // Name
            DuePin47, // DirPin
            DuePin32, // StepPin
            DuePin45, // EnablePin
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
            AxisDriverService<
                At91Sam3xClockInterruptTimerService<At91Sam3xClockTC4, At91Sam3xClockCompA>, // StepperTimer
                TheAxisDriverPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >
    >,
    
    /*
     * Transform and virtual axes.
     */
    PrinterMainTransformParams<
        MakeTypeList<
            PrinterMainVirtualAxisParams<
                'X', // Name
                XMinPos,
                XMaxPos,
                XMaxSpeed,
                PrinterMainNoVirtualHomingParams
            >,
            PrinterMainVirtualAxisParams<
                'Y', // Name
                YMinPos,
                YMaxPos,
                YMaxSpeed,
                PrinterMainNoVirtualHomingParams
            >,
            PrinterMainVirtualAxisParams<
                'Z', // Name
                ZMinPos,
                ZMaxPos,
                ZMaxSpeed,
                PrinterMainNoVirtualHomingParams
            >
        >,
        MakeTypeList<WrapInt<'A'>, WrapInt<'B'>, WrapInt<'C'>>,
        DeltaSegmentsPerSecond,
        DeltaTransform,
        DeltaTransformParams<
            DeltaDiagonalRod,
            DeltaTower1X,
            DeltaTower1Y,
            DeltaTower2X,
            DeltaTower2Y,
            DeltaTower3X,
            DeltaTower3Y,
            DistanceSplitterParams<DeltaMinSplitLength, DeltaMaxSplitLength>
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
            DuePinA1, // AdcPin
            GenericThermistor< // Thermistor
                ExtruderHeaterThermistorResistorR,
                ExtruderHeaterThermistorR0,
                ExtruderHeaterThermistorBeta,
                ExtruderHeaterThermistorMinTemp,
                ExtruderHeaterThermistorMaxTemp
            >,
            ExtruderHeaterMinSafeTemp, // MinSafeTemp
            ExtruderHeaterMaxSafeTemp, // MaxSafeTemp
            ExtruderHeaterControlInterval, // ControlInterval
            PidControlService<
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
            SoftPwmService<
                DuePin9, // OutputPin
                true, // OutputInvert
                ExtruderHeaterPulseInterval, // PulseInterval
                At91Sam3xClockInterruptTimerService<At91Sam3xClockTC5, At91Sam3xClockCompA> // TimerTemplate
            >
        >,
        PrinterMainHeaterParams<
            'B', // Name
            140, // SetMCommand
            190, // WaitMCommand
            304, // SetConfigMCommand
            DuePinA0, // AdcPin
            GenericThermistor< // Thermistor
                BedHeaterThermistorResistorR,
                BedHeaterThermistorR0,
                BedHeaterThermistorBeta,
                BedHeaterThermistorMinTemp,
                BedHeaterThermistorMaxTemp
            >,
            BedHeaterMinSafeTemp, // MinSafeTemp
            BedHeaterMaxSafeTemp, // MaxSafeTemp
            BedHeaterControlInterval, // ControlInterval
            PidControlService<
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
            SoftPwmService<
                DuePin8, // OutputPin
                true, // OutputInvert
                BedHeaterPulseInterval, // PulseInterval
                At91Sam3xClockInterruptTimerService<At91Sam3xClockTC5, At91Sam3xClockCompB> // TimerTemplate
            >
        >
    >,
    
    /*
     * Fans.
     */
    MakeTypeList<
        PrinterMainFanParams<
            106, // SetMCommand
            107, // OffMCommand
            FanSpeedMultiply, // SpeedMultiply
            SoftPwmService<
                DuePin12, // OutputPin
                false, // OutputInvert
                FanPulseInterval, // PulseInterval
                At91Sam3xClockInterruptTimerService<At91Sam3xClockTC6, At91Sam3xClockCompB> // TimerTemplate
            >
        >,
        PrinterMainFanParams<
            406, // SetMCommand
            407, // OffMCommand
            FanSpeedMultiply, // SpeedMultiply
            SoftPwmService<
                DuePin2, // OutputPin
                false, // OutputInvert
                FanPulseInterval, // PulseInterval
                At91Sam3xClockInterruptTimerService<At91Sam3xClockTC7, At91Sam3xClockCompA> // TimerTemplate
            >
        >
    >
>;

// need to list all used ADC pins here
using AdcPins = MakeTypeList<
    At91SamAdcSmoothPin<DuePinA1, AdcSmoothing>,
    At91SamAdcSmoothPin<DuePinA0, AdcSmoothing>
>;

using AdcParams = At91Sam3xAdcParams<
    AdcFreq,
    8, // AdcStartup
    3, // AdcSettling
    0, // AdcTracking
    1, // AdcTransfer
    At91SamAdcAvgParams<AdcAvgInterval>
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
struct MyLoopExtraDelay;
struct Program;

using MyDebugObjectGroup = DebugObjectGroup<MyContext, Program>;
using MyClock = At91Sam3xClock<MyContext, Program, clock_timer_prescaler, ClockTcsList>;
using MyLoop = BusyEventLoop<MyContext, Program, MyLoopExtraDelay>;
using MyPins = At91SamPins<MyContext, Program>;
using MyAdc = At91SamAdc<MyContext, Program, AdcPins, AdcParams>;
using MyPrinter = PrinterMain<MyContext, Program, PrinterParams>;

struct MyContext {
    using DebugGroup = MyDebugObjectGroup;
    using Clock = MyClock;
    using EventLoop = MyLoop;
    using Pins = MyPins;
    using Adc = MyAdc;
    
    void check () const;
};

using MyLoopExtra = BusyEventLoopExtra<Program, MyLoop, typename MyPrinter::EventLoopFastEvents>;
struct MyLoopExtraDelay : public WrapType<MyLoopExtra> {};

struct Program : public ObjBase<void, void, MakeTypeList<
    MyDebugObjectGroup,
    MyClock,
    MyLoop,
    MyPins,
    MyAdc,
    MyPrinter,
    MyLoopExtra
>> {
    static Program * self (MyContext c);
};

Program p;

Program * Program::self (MyContext c) { return &p; }
void MyContext::check () const {}

AMBRO_AT91SAM3X_CLOCK_TC0_GLOBAL(MyClock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC1_GLOBAL(MyClock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC2_GLOBAL(MyClock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC3_GLOBAL(MyClock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC4_GLOBAL(MyClock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC5_GLOBAL(MyClock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC6_GLOBAL(MyClock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC7_GLOBAL(MyClock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC8_GLOBAL(MyClock, MyContext())

AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC0, At91Sam3xClockCompA, MyPrinter::GetEventChannelTimer, MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC1, At91Sam3xClockCompA, MyPrinter::GetAxisTimer<0>, MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC2, At91Sam3xClockCompA, MyPrinter::GetAxisTimer<1>, MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC3, At91Sam3xClockCompA, MyPrinter::GetAxisTimer<2>, MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC4, At91Sam3xClockCompA, MyPrinter::GetAxisTimer<3>, MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC5, At91Sam3xClockCompA, MyPrinter::GetHeaterPwm<0>::TheTimer, MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC5, At91Sam3xClockCompB, MyPrinter::GetHeaterPwm<1>::TheTimer, MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC6, At91Sam3xClockCompB, MyPrinter::GetFanPwm<0>::TheTimer, MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC7, At91Sam3xClockCompA, MyPrinter::GetFanPwm<1>::TheTimer, MyContext())

#ifndef USB_SERIAL
AMBRO_AT91SAM3X_SERIAL_GLOBAL(MyPrinter::GetSerial, MyContext())
#endif
AMBRO_AT91SAM3X_SPI_GLOBAL(MyPrinter::GetSdCard<>::GetSpi, MyContext())
AMBRO_AT91SAM3X_ADC_GLOBAL(MyAdc, MyContext())

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
            MyPrinter::GetSerial::sendWaitFinished(MyContext());
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
    
    MyDebugObjectGroup::init(c);
    MyClock::init(c);
    MyLoop::init(c);
    MyPins::init(c);
    MyAdc::init(c);
    MyPrinter::init(c);
    
    MyLoop::run(c);
}
