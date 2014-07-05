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
#include <aprinter/printer/AxisHomer.h>
#include <aprinter/printer/TemperatureObserver.h>
#include <aprinter/printer/pwm/SoftPwm.h>
#include <aprinter/printer/thermistor/GenericThermistor.h>
#include <aprinter/printer/temp_control/PidControl.h>
#include <aprinter/printer/temp_control/BinaryControl.h>
#include <aprinter/printer/config_manager/ConstexprConfigManager.h>
#include <aprinter/printer/config_manager/RuntimeConfigManager.h>
#include <aprinter/board/arduino_due_pins.h>

using namespace APrinter;

APRINTER_CONFIG_START

using AdcFreq = AMBRO_WRAP_DOUBLE(1000000.0);
using AdcAvgInterval = AMBRO_WRAP_DOUBLE(0.0025);
static uint16_t const AdcSmoothing = 0.95 * 65536.0;

using LedBlinkInterval = AMBRO_WRAP_DOUBLE(0.5);
using SpeedLimitMultiply = AMBRO_WRAP_DOUBLE(1.0 / 60.0);
using MaxStepsPerCycle = AMBRO_WRAP_DOUBLE(0.0017);
using ForceTimeout = AMBRO_WRAP_DOUBLE(0.1);
using TheAxisDriverPrecisionParams = AxisDriverDuePrecisionParams;
using EventChannelTimerClearance = AMBRO_WRAP_DOUBLE(0.002);

APRINTER_CONFIG_OPTION_DOUBLE(InactiveTime, 8.0 * 60.0)

APRINTER_CONFIG_OPTION_BOOL(XInvertDir, false)
APRINTER_CONFIG_OPTION_DOUBLE(XStepsPerUnit, 2.0 * 80.0)
APRINTER_CONFIG_OPTION_DOUBLE(XMin, -53.0)
APRINTER_CONFIG_OPTION_DOUBLE(XMax, 210.0)
APRINTER_CONFIG_OPTION_DOUBLE(XMaxSpeed, 300.0)
APRINTER_CONFIG_OPTION_DOUBLE(XMaxAccel, 1500.0)
APRINTER_CONFIG_OPTION_DOUBLE(XDistanceFactor, 1.0)
APRINTER_CONFIG_OPTION_DOUBLE(XCorneringDistance, 40.0)
APRINTER_CONFIG_OPTION_DOUBLE(XHomeFastMaxDist, 280.0)
APRINTER_CONFIG_OPTION_DOUBLE(XHomeRetractDist, 3.0)
APRINTER_CONFIG_OPTION_DOUBLE(XHomeSlowMaxDist, 5.0)
APRINTER_CONFIG_OPTION_DOUBLE(XHomeFastSpeed, 40.0)
APRINTER_CONFIG_OPTION_DOUBLE(XHomeRetractSpeed, 50.0)
APRINTER_CONFIG_OPTION_DOUBLE(XHomeSlowSpeed, 5.0)

APRINTER_CONFIG_OPTION_BOOL(YInvertDir, false)
APRINTER_CONFIG_OPTION_DOUBLE(YStepsPerUnit, 2.0 * 80.0)
APRINTER_CONFIG_OPTION_DOUBLE(YMin, 0.0)
APRINTER_CONFIG_OPTION_DOUBLE(YMax, 157.0)
APRINTER_CONFIG_OPTION_DOUBLE(YMaxSpeed, 300.0)
APRINTER_CONFIG_OPTION_DOUBLE(YMaxAccel, 650.0)
APRINTER_CONFIG_OPTION_DOUBLE(YDistanceFactor, 1.0)
APRINTER_CONFIG_OPTION_DOUBLE(YCorneringDistance, 40.0)
APRINTER_CONFIG_OPTION_DOUBLE(YHomeFastMaxDist, 200.0)
APRINTER_CONFIG_OPTION_DOUBLE(YHomeRetractDist, 3.0)
APRINTER_CONFIG_OPTION_DOUBLE(YHomeSlowMaxDist, 5.0)
APRINTER_CONFIG_OPTION_DOUBLE(YHomeFastSpeed, 40.0)
APRINTER_CONFIG_OPTION_DOUBLE(YHomeRetractSpeed, 50.0)
APRINTER_CONFIG_OPTION_DOUBLE(YHomeSlowSpeed, 5.0)

APRINTER_CONFIG_OPTION_BOOL(ZInvertDir, true)
APRINTER_CONFIG_OPTION_DOUBLE(ZStepsPerUnit, 2.0 * 4000.0)
APRINTER_CONFIG_OPTION_DOUBLE(ZMin, 0.0)
APRINTER_CONFIG_OPTION_DOUBLE(ZMax, 100.0)
APRINTER_CONFIG_OPTION_DOUBLE(ZMaxSpeed, 3.0)
APRINTER_CONFIG_OPTION_DOUBLE(ZMaxAccel, 30.0)
APRINTER_CONFIG_OPTION_DOUBLE(ZDistanceFactor, 1.0)
APRINTER_CONFIG_OPTION_DOUBLE(ZCorneringDistance, 40.0)
APRINTER_CONFIG_OPTION_DOUBLE(ZHomeFastMaxDist, 101.0)
APRINTER_CONFIG_OPTION_DOUBLE(ZHomeRetractDist, 0.8)
APRINTER_CONFIG_OPTION_DOUBLE(ZHomeSlowMaxDist, 1.2)
APRINTER_CONFIG_OPTION_DOUBLE(ZHomeFastSpeed, 2.0)
APRINTER_CONFIG_OPTION_DOUBLE(ZHomeRetractSpeed, 2.0)
APRINTER_CONFIG_OPTION_DOUBLE(ZHomeSlowSpeed, 0.6)

APRINTER_CONFIG_OPTION_BOOL(EInvertDir, false)
APRINTER_CONFIG_OPTION_DOUBLE(EStepsPerUnit, 2.0 * 928.0)
APRINTER_CONFIG_OPTION_DOUBLE(EMin, -40000.0)
APRINTER_CONFIG_OPTION_DOUBLE(EMax, 40000.0)
APRINTER_CONFIG_OPTION_DOUBLE(EMaxSpeed, 45.0)
APRINTER_CONFIG_OPTION_DOUBLE(EMaxAccel, 250.0)
APRINTER_CONFIG_OPTION_DOUBLE(EDistanceFactor, 1.0)
APRINTER_CONFIG_OPTION_DOUBLE(ECorneringDistance, 40.0)

APRINTER_CONFIG_OPTION_BOOL(UInvertDir, false)
APRINTER_CONFIG_OPTION_DOUBLE(UStepsPerUnit, 2.0 * 660.0)
APRINTER_CONFIG_OPTION_DOUBLE(UMin, -40000.0)
APRINTER_CONFIG_OPTION_DOUBLE(UMax, 40000.0)
APRINTER_CONFIG_OPTION_DOUBLE(UMaxSpeed, 45.0)
APRINTER_CONFIG_OPTION_DOUBLE(UMaxAccel, 250.0)
APRINTER_CONFIG_OPTION_DOUBLE(UDistanceFactor, 1.0)
APRINTER_CONFIG_OPTION_DOUBLE(UCorneringDistance, 40.0)

APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterThermistorResistorR, 4700.0)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterThermistorR0, 100000.0)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterThermistorBeta, 3960.0)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterThermistorMinTemp, 10.0)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterThermistorMaxTemp, 300.0)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterMinSafeTemp, 20.0)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterMaxSafeTemp, 280.0)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterControlInterval, 0.2)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterPidP, 0.047)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterPidI, 0.0006)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterPidD, 0.17)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterPidIStateMin, 0.0)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterPidIStateMax, 0.4)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterPidDHistory, 0.7)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterObserverInterval, 0.5)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterObserverTolerance, 3.0)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterObserverMinTime, 3.0)
using ExtruderHeaterPulseInterval = AMBRO_WRAP_DOUBLE(0.2);

APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterThermistorResistorR, 4700.0)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterThermistorR0, 100000.0)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterThermistorBeta, 3960.0)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterThermistorMinTemp, 10.0)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterThermistorMaxTemp, 300.0)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterMinSafeTemp, 20.0)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterMaxSafeTemp, 280.0)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterControlInterval, 0.2)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterPidP, 0.047)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterPidI, 0.0006)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterPidD, 0.17)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterPidIStateMin, 0.0)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterPidIStateMax, 0.4)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterPidDHistory, 0.7)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterObserverInterval, 0.5)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterObserverTolerance, 3.0)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterObserverMinTime, 3.0)
using UxtruderHeaterPulseInterval = AMBRO_WRAP_DOUBLE(0.2);

APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterThermistorResistorR, 4700.0)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterThermistorR0, 10000.0)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterThermistorBeta, 3480.0)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterThermistorMinTemp, 10.0)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterThermistorMaxTemp, 150.0)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterMinSafeTemp, 20.0)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterMaxSafeTemp, 120.0)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterControlInterval, 0.3)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterPidP, 1.0)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterPidI, 0.012)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterPidD, 2.5)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterPidIStateMin, 0.0)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterPidIStateMax, 1.0)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterPidDHistory, 0.8)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterObserverInterval, 0.5)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterObserverTolerance, 1.5)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterObserverMinTime, 3.0)
using BedHeaterPulseInterval = AMBRO_WRAP_DOUBLE(0.3);

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

APRINTER_CONFIG_END

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
    DuePin37, // LedPin
    LedBlinkInterval, // LedBlinkInterval
    InactiveTime,
    SpeedLimitMultiply, // SpeedLimitMultiply
    MaxStepsPerCycle, // MaxStepsPerCycle
    32, // StepperSegmentBufferSize
    32, // EventChannelBufferSize
    28, // LookaheadBufferSize
    10, // LookaheadCommitCount
    ForceTimeout, // ForceTimeout
    double, // FpType
    At91Sam3xClockInterruptTimerService<At91Sam3xClockTC0, At91Sam3xClockCompA, EventChannelTimerClearance>, // EventChannelTimerService
    At91SamWatchdogService<260>,
    PrinterMainSdCardParams<
        SpiSdCardService< // SdCardService
            DuePin4, // SsPin
            At91SamSpiService<At91Sam3xSpiDevice> // SpiService
        >,
        FileGcodeParser, // BINARY: BinaryGcodeParser
        GcodeParserParams<8>, // BINARY: BinaryGcodeParserParams<8>
        2048, // BufferBaseSize
        256 // MaxCommandSize. BINARY: 43
    >,
    PrinterMainProbeParams<
        MakeTypeList<WrapInt<'X'>, WrapInt<'Y'>>, // PlatformAxesList
        'Z', // ProbeAxis
        DuePin38, // ProbePin,
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
    RuntimeConfigManagerService<
        925, // GetConfigMCommand
        926 // SetConfigMCommand
    >,
    ConfigList,
    
    /*
     * Axes.
     */
    MakeTypeList<
        PrinterMainAxisParams<
            'X', // Name
            DuePin23, // DirPin
            DuePin24, // StepPin
            DuePin26, // EnablePin
            XInvertDir,
            XStepsPerUnit, // StepsPerUnit
            XMin, // Min
            XMax, // Max
            XMaxSpeed, // MaxSpeed
            XMaxAccel, // MaxAccel
            XDistanceFactor, // DistanceFactor
            XCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                false, // HomeDir
                AxisHomerService< // HomerService
                    DuePin28, // HomeEndPin
                    At91SamPinInputModePullUp, // HomeEndPinInputMode
                    false, // HomeEndInvert
                    XHomeFastMaxDist, // HomeFastMaxDist
                    XHomeRetractDist, // HomeRetractDist
                    XHomeSlowMaxDist, // HomeSlowMaxDist
                    XHomeFastSpeed, // HomeFastSpeed
                    XHomeRetractSpeed, // HomeRetractSpeed
                    XHomeSlowSpeed // HomeSlowSpeed
                >
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
            DuePin16, // DirPin
            DuePin17, // StepPin
            DuePin22, // EnablePin
            YInvertDir,
            YStepsPerUnit, // StepsPerUnit
            YMin, // Min
            YMax, // Max
            YMaxSpeed, // MaxSpeed
            YMaxAccel, // MaxAccel
            YDistanceFactor, // DistanceFactor
            YCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                false, // HomeDir
                AxisHomerService< // HomerService
                    DuePin30, // HomeEndPin
                    At91SamPinInputModePullUp, // HomeEndPinInputMode
                    false, // HomeEndInvert
                    YHomeFastMaxDist, // HomeFastMaxDist
                    YHomeRetractDist, // HomeRetractDist
                    YHomeSlowMaxDist, // HomeSlowMaxDist
                    YHomeFastSpeed, // HomeFastSpeed
                    YHomeRetractSpeed, // HomeRetractSpeed
                    YHomeSlowSpeed // HomeSlowSpeed
                >
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
            DuePin3, // DirPin
            DuePin2, // StepPin
            DuePin15, // EnablePin
            ZInvertDir,
            ZStepsPerUnit, // StepsPerUnit
            ZMin, // Min
            ZMax, // Max
            ZMaxSpeed, // MaxSpeed
            ZMaxAccel, // MaxAccel
            ZDistanceFactor, // DistanceFactor
            ZCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                false, // HomeDir
                AxisHomerService< // HomerService
                    DuePin32, // HomeEndPin
                    At91SamPinInputModePullUp, // HomeEndPinInputMode
                    false, // HomeEndInvert
                    ZHomeFastMaxDist, // HomeFastMaxDist
                    ZHomeRetractDist, // HomeRetractDist
                    ZHomeSlowMaxDist, // HomeSlowMaxDist
                    ZHomeFastSpeed, // HomeFastSpeed
                    ZHomeRetractSpeed, // HomeRetractSpeed
                    ZHomeSlowSpeed // HomeSlowSpeed
                >
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
            DuePinA6, // DirPin
            DuePinA7, // StepPin
            DuePinA8, // EnablePin
            EInvertDir,
            EStepsPerUnit, // StepsPerUnit
            EMin, // Min
            EMax, // Max
            EMaxSpeed, // MaxSpeed
            EMaxAccel, // MaxAccel
            EDistanceFactor, // DistanceFactor
            ECorneringDistance, // CorneringDistance
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
            DuePinA9, // DirPin
            DuePinA10, // StepPin
            DuePinA11, // EnablePin
            UInvertDir,
            UStepsPerUnit, // StepsPerUnit
            UMin, // Min
            UMax, // Max
            UMaxSpeed, // MaxSpeed
            UMaxAccel, // MaxAccel
            UDistanceFactor, // DistanceFactor
            UCorneringDistance, // CorneringDistance
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
            DuePinA0, // AdcPin
            GenericThermistorService< // Thermistor
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
            TemperatureObserverService<
                ExtruderHeaterObserverInterval, // ObserverInterval
                ExtruderHeaterObserverTolerance, // ObserverTolerance
                ExtruderHeaterObserverMinTime // ObserverMinTime
            >,
            SoftPwmService<
                DuePin13, // OutputPin
                false, // OutputInvert
                ExtruderHeaterPulseInterval, // PulseInterval
                At91Sam3xClockInterruptTimerService<At91Sam3xClockTC5, At91Sam3xClockCompA> // TimerTemplate
            >
        >,
        PrinterMainHeaterParams<
            'B', // Name
            140, // SetMCommand
            190, // WaitMCommand
            304, // SetConfigMCommand
            DuePinA4, // AdcPin
            GenericThermistorService< // Thermistor
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
            TemperatureObserverService<
                BedHeaterObserverInterval, // ObserverInterval
                BedHeaterObserverTolerance, // ObserverTolerance
                BedHeaterObserverMinTime // ObserverMinTime
            >,
            SoftPwmService<
                DuePin7, // OutputPin
                false, // OutputInvert
                BedHeaterPulseInterval, // PulseInterval
                At91Sam3xClockInterruptTimerService<At91Sam3xClockTC5, At91Sam3xClockCompB> // TimerTemplate
            >
        >,
        PrinterMainHeaterParams<
            'U', // Name
            404, // SetMCommand
            409, // WaitMCommand
            402, // SetConfigMCommand
            DuePinA1, // AdcPin
            GenericThermistorService< // Thermistor
                UxtruderHeaterThermistorResistorR,
                UxtruderHeaterThermistorR0,
                UxtruderHeaterThermistorBeta,
                UxtruderHeaterThermistorMinTemp,
                UxtruderHeaterThermistorMaxTemp
            >,
            UxtruderHeaterMinSafeTemp, // MinSafeTemp
            UxtruderHeaterMaxSafeTemp, // MaxSafeTemp
            UxtruderHeaterControlInterval, // ControlInterval
            PidControlService<
                UxtruderHeaterPidP, // PidP
                UxtruderHeaterPidI, // PidI
                UxtruderHeaterPidD, // PidD
                UxtruderHeaterPidIStateMin, // PidIStateMin
                UxtruderHeaterPidIStateMax, // PidIStateMax
                UxtruderHeaterPidDHistory // PidDHistory
            >,
            TemperatureObserverService<
                UxtruderHeaterObserverInterval, // ObserverInterval
                UxtruderHeaterObserverTolerance, // ObserverTolerance
                UxtruderHeaterObserverMinTime // ObserverMinTime
            >,
            SoftPwmService<
                DuePin12, // OutputPin
                false, // OutputInvert
                UxtruderHeaterPulseInterval, // PulseInterval
                At91Sam3xClockInterruptTimerService<At91Sam3xClockTC6, At91Sam3xClockCompA> // TimerTemplate
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
                DuePin9, // OutputPin
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
                DuePin8, // OutputPin
                false, // OutputInvert
                FanPulseInterval, // PulseInterval
                At91Sam3xClockInterruptTimerService<At91Sam3xClockTC7, At91Sam3xClockCompA> // TimerTemplate
            >
        >
    >
>;

// need to list all used ADC pins here
using AdcPins = MakeTypeList<
    At91SamAdcSmoothPin<DuePinA0, AdcSmoothing>,
    At91SamAdcSmoothPin<DuePinA4, AdcSmoothing>,
    At91SamAdcSmoothPin<DuePinA1, AdcSmoothing>
>;

using AdcParams = At91SamAdcParams<
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
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC8, At91Sam3xClockCompA, MyPrinter::GetAxisTimer<4>, MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC5, At91Sam3xClockCompA, MyPrinter::GetHeaterPwm<0>::TheTimer, MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC5, At91Sam3xClockCompB, MyPrinter::GetHeaterPwm<1>::TheTimer, MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC6, At91Sam3xClockCompA, MyPrinter::GetHeaterPwm<2>::TheTimer, MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC6, At91Sam3xClockCompB, MyPrinter::GetFanPwm<0>::TheTimer, MyContext())
AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC7, At91Sam3xClockCompA, MyPrinter::GetFanPwm<1>::TheTimer, MyContext())

#ifndef USB_SERIAL
AMBRO_AT91SAM3X_SERIAL_GLOBAL(MyPrinter::GetSerial, MyContext())
#endif
AMBRO_AT91SAM3X_SPI_GLOBAL(MyPrinter::GetSdCard<>::GetSpi, MyContext())
AMBRO_AT91SAM_ADC_GLOBAL(MyAdc, MyContext())

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
