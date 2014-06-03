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

#include <aprinter/platform/stm32f4/stm32f4_support.h>

static void emergency (void);

#define AMBROLIB_EMERGENCY_ACTION { cli(); emergency(); }
#define AMBROLIB_ABORT_ACTION { while (1); }

#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/system/BusyEventLoop.h>
#include <aprinter/system/Stm32f4Clock.h>
#include <aprinter/system/Stm32f4Pins.h>
#include <aprinter/system/InterruptLock.h>
//#include <aprinter/system/At91SamAdc.h>
//#include <aprinter/system/At91SamWatchdog.h>
//#include <aprinter/system/At91Sam3xSerial.h>
//#include <aprinter/system/At91Sam3xSpi.h>
//#include <aprinter/system/AsfUsbSerial.h>
//#include <aprinter/devices/SpiSdCard.h>
#include <aprinter/usb/Stm32f4Usb.h>
#include <aprinter/driver/AxisDriver.h>
#include <aprinter/printer/PrinterMain.h>
#include <aprinter/printer/AxisHomer.h>
#include <aprinter/printer/pwm/SoftPwm.h>
#include <aprinter/printer/temp_control/PidControl.h>
#include <aprinter/printer/temp_control/BinaryControl.h>
#include <aprinter/printer/stm32f4discovery_pins.h>

#include <aprinter/meta/WrapDouble.h>

using namespace APrinter;

using AdcFreq = AMBRO_WRAP_DOUBLE(1000000.0);
using AdcAvgInterval = AMBRO_WRAP_DOUBLE(0.0025);
static uint16_t const AdcSmoothing = 0.95 * 65536.0;

using LedBlinkInterval = AMBRO_WRAP_DOUBLE(0.5);
using DefaultInactiveTime = AMBRO_WRAP_DOUBLE(60.0);
using SpeedLimitMultiply = AMBRO_WRAP_DOUBLE(1.0 / 60.0);
using MaxStepsPerCycle = AMBRO_WRAP_DOUBLE(0.0017);
using ForceTimeout = AMBRO_WRAP_DOUBLE(0.1);
//using TheAxisDriverPrecisionParams = AxisDriverDuePrecisionParams;

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
#if 0
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
    At91Sam3xClockInterruptTimerService<At91Sam3xClockTC0, At91Sam3xClockCompA>, // EventChannelTimer
    At91SamWatchdogService<260>,
    PrinterMainSdCardParams<
        SpiSdCard,
        SpiSdCardParams<
            DuePin4, // SsPin
            At91Sam3xSpi
        >,
        BinaryGcodeParser, // BINARY: BinaryGcodeParser
        BinaryGcodeParserParams<8>, // BINARY: BinaryGcodeParserParams<8>
        2, // BufferBlocks
        43 // MaxCommandSize. BINARY: 43
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
                At91SamPinInputModePullUp, // HomeEndPinInputMode
                false, // HomeEndInvert
                false, // HomeDir
                XDefaultHomeFastMaxDist, // HomeFastMaxDist
                XDefaultHomeRetractDist, // HomeRetractDist
                XDefaultHomeSlowMaxDist, // HomeSlowMaxDist
                XDefaultHomeFastSpeed, // HomeFastSpeed
                XDefaultHomeRetractSpeed, // HomeRetractSpeed
                XDefaultHomeSlowSpeed, // HomeSlowSpeed
                AxisHomerService // HomerService
            >,
            true, // EnableCartesianSpeedLimit
            32, // StepBits
            AxisDriverService<
                At91Sam3xClockInterruptTimerService<At91Sam3xClockTC1, At91Sam3xClockCompA>, // StepperTimer,
                TheAxisDriverPrecisionParams // PrecisionParams
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
                At91SamPinInputModePullUp, // HomeEndPinInputMode
                false, // HomeEndInvert
                false, // HomeDir
                YDefaultHomeFastMaxDist, // HomeFastMaxDist
                YDefaultHomeRetractDist, // HomeRetractDist
                YDefaultHomeSlowMaxDist, // HomeSlowMaxDist
                YDefaultHomeFastSpeed, // HomeFastSpeed
                YDefaultHomeRetractSpeed, // HomeRetractSpeed
                YDefaultHomeSlowSpeed, // HomeSlowSpeed
                AxisHomerService // HomerService
            >,
            true, // EnableCartesianSpeedLimit
            32, // StepBits
            AxisDriverService<
                At91Sam3xClockInterruptTimerService<At91Sam3xClockTC2, At91Sam3xClockCompA>, // StepperTimer
                TheAxisDriverPrecisionParams // PrecisionParams
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
                DuePin38, // HomeEndPin
                At91SamPinInputModePullUp, // HomeEndPinInputMode
                false, // HomeEndInvert
                false, // HomeDir
                ZDefaultHomeFastMaxDist, // HomeFastMaxDist
                ZDefaultHomeRetractDist, // HomeRetractDist
                ZDefaultHomeSlowMaxDist, // HomeSlowMaxDist
                ZDefaultHomeFastSpeed, // HomeFastSpeed
                ZDefaultHomeRetractSpeed, // HomeRetractSpeed
                ZDefaultHomeSlowSpeed, // HomeSlowSpeed
                AxisHomerService // HomerService
            >,
            true, // EnableCartesianSpeedLimit
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
            AxisDriverService<
                At91Sam3xClockInterruptTimerService<At91Sam3xClockTC8, At91Sam3xClockCompA>, // StepperTimer
                TheAxisDriverPrecisionParams // PrecisionParams
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
            At91Sam3xClockInterruptTimerService<At91Sam3xClockTC5, At91Sam3xClockCompA> // TimerTemplate
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
            At91Sam3xClockInterruptTimerService<At91Sam3xClockTC5, At91Sam3xClockCompB> // TimerTemplate
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
            PidControlService<
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
            At91Sam3xClockInterruptTimerService<At91Sam3xClockTC6, At91Sam3xClockCompA> // TimerTemplate
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
    At91SamAdcSmoothPin<DuePinA0, AdcSmoothing>,
    At91SamAdcSmoothPin<DuePinA2, AdcSmoothing>
>;

using AdcParams = At91Sam3xAdcParams<
    AdcFreq,
    8, // AdcStartup
    3, // AdcSettling
    0, // AdcTracking
    1, // AdcTransfer
    At91SamAdcAvgParams<AdcAvgInterval>
>;
#endif

static const int clock_timer_prescaler = 31;
using ClockTcsList = MakeTypeList<
    Stm32f4ClockTIM2
>;

struct MyContext;
struct MyLoopExtraDelay;
struct Program;

using MyDebugObjectGroup = DebugObjectGroup<MyContext, Program>;
using MyClock = Stm32f4Clock<MyContext, Program, clock_timer_prescaler, ClockTcsList>;
using MyLoop = BusyEventLoop<MyContext, Program, MyLoopExtraDelay>;
using MyPins = Stm32f4Pins<MyContext, Program>;
//using MyAdc = At91SamAdc<MyContext, Program, AdcPins, AdcParams>;
//using MyPrinter = PrinterMain<MyContext, Program, PrinterParams>;
using MyUsb = Stm32f4Usb<MyContext, Program, Stm32F4UsbInfoFS>;

struct MyContext {
    using DebugGroup = MyDebugObjectGroup;
    using Clock = MyClock;
    using EventLoop = MyLoop;
    using Pins = MyPins;
//    using Adc = MyAdc;
    
    void check () const;
};

//using MyLoopExtra = BusyEventLoopExtra<Program, MyLoop, typename MyPrinter::EventLoopFastEvents>;
using MyLoopExtra = BusyEventLoopExtra<Program, MyLoop, typename MyUsb::EventLoopFastEvents>;
struct MyLoopExtraDelay : public WrapType<MyLoopExtra> {};

struct Program : public ObjBase<void, void, MakeTypeList<
    MyDebugObjectGroup,
    MyClock,
    MyLoop,
    MyPins,
//    MyAdc,
//    MyPrinter,
    MyUsb,
    MyLoopExtra
>> {
    static Program * self (MyContext c);
};

Program p;

Program * Program::self (MyContext c) { return &p; }
void MyContext::check () const {}

#if 0
AMBRO_AT91SAM3X_CLOCK_TC0_GLOBAL(p.myclock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC1_GLOBAL(p.myclock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC2_GLOBAL(p.myclock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC3_GLOBAL(p.myclock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC4_GLOBAL(p.myclock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC5_GLOBAL(p.myclock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC6_GLOBAL(p.myclock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC7_GLOBAL(p.myclock, MyContext())
AMBRO_AT91SAM3X_CLOCK_TC8_GLOBAL(p.myclock, MyContext())

AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC0, At91Sam3xClockCompA, *p.myprinter.getEventChannelTimer(), MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC1, At91Sam3xClockCompA, *p.myprinter.getAxisDriver<0>()->getTimer(), MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC2, At91Sam3xClockCompA, *p.myprinter.getAxisDriver<1>()->getTimer(), MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC3, At91Sam3xClockCompA, *p.myprinter.getAxisDriver<2>()->getTimer(), MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC4, At91Sam3xClockCompA, *p.myprinter.getAxisDriver<3>()->getTimer(), MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC8, At91Sam3xClockCompA, *p.myprinter.getAxisDriver<4>()->getTimer(), MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC5, At91Sam3xClockCompA, *p.myprinter.getHeaterTimer<0>(), MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC5, At91Sam3xClockCompB, *p.myprinter.getHeaterTimer<1>(), MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC6, At91Sam3xClockCompA, *p.myprinter.getHeaterTimer<2>(), MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC6, At91Sam3xClockCompB, *p.myprinter.getFanTimer<0>(), MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC7, At91Sam3xClockCompA, *p.myprinter.getFanTimer<1>(), MyContext())

#ifndef USB_SERIAL
AMBRO_AT91SAM3X_SERIAL_GLOBAL(*p.myprinter.getSerial(), MyContext())
#endif
AMBRO_AT91SAM3X_SPI_GLOBAL(*p.myprinter.getSdCard()->getSpi(), MyContext())
AMBRO_AT91SAM3X_ADC_GLOBAL(p.myadc, MyContext())
#endif

AMBRO_STM32F4_USB_GLOBAL(MyUsb, MyContext())

static void emergency (void)
{
//    MyPrinter::emergency();
}

MyLoop::QueuedEvent timer;
uint32_t next;

uint32_t const Ticks = 0.002 * MyClock::time_freq;

static void timer_handler (MyLoop::QueuedEvent *, MyContext c)
{
    typename MyUsb::State st = MyUsb::getState(c);
    MyPins::template set<DiscoveryPinLedGreen>(c, st == MyUsb::STATE_WAITING_RESET);
    MyPins::template set<DiscoveryPinLedOrange>(c, st == MyUsb::STATE_WAITING_ENUM);
    MyPins::template set<DiscoveryPinLedRed>(c, st == MyUsb::STATE_ENUM_DONE);
    MyPins::template set<DiscoveryPinLedBlue>(c, st == MyUsb::STATE_TEST);
    
    next += Ticks;
    timer.appendAt(c, next);
}

int main ()
{
    platform_init();
    
    MyContext c;
    
    MyDebugObjectGroup::init(c);
    MyClock::init(c);
    MyLoop::init(c);
    MyPins::init(c);
    //MyAdc::init(c);
    //MyPrinter::init(c);
    MyUsb::init(c);
    
    MyPins::template setAlternateFunction<Stm32f4Pin<Stm32f4PortA, 8>, 10>(c);
    MyPins::template setAlternateFunction<Stm32f4Pin<Stm32f4PortA, 10>, 10>(c);
    MyPins::template setAlternateFunction<Stm32f4Pin<Stm32f4PortA, 11>, 10>(c);
    MyPins::template setAlternateFunction<Stm32f4Pin<Stm32f4PortA, 12>, 10>(c);
    
    MyPins::template set<DiscoveryPinLedGreen>(c, 0);
    MyPins::template set<DiscoveryPinLedOrange>(c, 0);
    MyPins::template set<DiscoveryPinLedRed>(c, 0);
    MyPins::template set<DiscoveryPinLedBlue>(c, 0);
    
    MyPins::template setOutput<DiscoveryPinLedGreen>(c);
    MyPins::template setOutput<DiscoveryPinLedOrange>(c);
    MyPins::template setOutput<DiscoveryPinLedRed>(c);
    MyPins::template setOutput<DiscoveryPinLedBlue>(c);
    
    timer.init(c, timer_handler);
    next = MyClock::getTime(c);
    timer.appendAt(c, next);
    
    MyLoop::run(c);
}
