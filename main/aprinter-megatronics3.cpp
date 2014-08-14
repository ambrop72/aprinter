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

#include <avr/io.h>

#include <aprinter/platform/avr/avr_support.h>

static void emergency (void);

#define AMBROLIB_EMERGENCY_ACTION { cli(); emergency(); }
#define AMBROLIB_ABORT_ACTION { while (1); }

#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/system/BusyEventLoop.h>
#include <aprinter/system/AvrClock.h>
#include <aprinter/system/AvrPins.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/system/AvrAdc.h>
#include <aprinter/system/AvrWatchdog.h>
#include <aprinter/system/AvrSerial.h>
#include <aprinter/system/AvrSpi.h>
#include <aprinter/devices/SpiSdCard.h>
#include <aprinter/driver/AxisDriver.h>
#include <aprinter/printer/PrinterMain.h>
#include <aprinter/printer/AxisHomer.h>
#include <aprinter/printer/TemperatureObserver.h>
#include <aprinter/printer/pwm/SoftPwm.h>
#include <aprinter/printer/pwm/HardPwm.h>
#include <aprinter/printer/thermistor/GenericThermistor.h>
#include <aprinter/printer/temp_control/PidControl.h>
#include <aprinter/printer/temp_control/BinaryControl.h>
#include <aprinter/printer/config_manager/ConstantConfigManager.h>
#include <aprinter/board/arduino_mega_pins.h>

using namespace APrinter;

APRINTER_CONFIG_START

using LedBlinkInterval = AMBRO_WRAP_DOUBLE(0.5);
using SpeedLimitMultiply = AMBRO_WRAP_DOUBLE(1.0 / 60.0);
using TheAxisDriverPrecisionParams = AxisDriverAvrPrecisionParams;
using EventChannelTimerClearance = AMBRO_WRAP_DOUBLE(0.002);

APRINTER_CONFIG_OPTION_DOUBLE(MaxStepsPerCycle, 0.00137, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ForceTimeout, 0.1, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(InactiveTime, 8.0 * 60.0, ConfigNoProperties)

APRINTER_CONFIG_OPTION_BOOL(XInvertDir, true, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(XStepsPerUnit, 80.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(XMin, -53.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(XMax, 210.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(XMaxSpeed, 300.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(XMaxAccel, 1500.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(XDistanceFactor, 1.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(XCorneringDistance, 40.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_BOOL(XHomeDir, false, ConfigNoProperties)
APRINTER_CONFIG_OPTION_BOOL(XHomeEndInvert, false, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(XHomeFastMaxDist, 280.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(XHomeRetractDist, 3.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(XHomeSlowMaxDist, 5.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(XHomeFastSpeed, 40.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(XHomeRetractSpeed, 50.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(XHomeSlowSpeed, 5.0, ConfigNoProperties)

APRINTER_CONFIG_OPTION_BOOL(YInvertDir, true, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(YStepsPerUnit, 80.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(YMin, 0.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(YMax, 155.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(YMaxSpeed, 300.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(YMaxAccel, 650.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(YDistanceFactor, 1.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(YCorneringDistance, 40.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_BOOL(YHomeDir, false, ConfigNoProperties)
APRINTER_CONFIG_OPTION_BOOL(YHomeEndInvert, false, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(YHomeFastMaxDist, 200.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(YHomeRetractDist, 3.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(YHomeSlowMaxDist, 5.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(YHomeFastSpeed, 40.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(YHomeRetractSpeed, 50.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(YHomeSlowSpeed, 5.0, ConfigNoProperties)

APRINTER_CONFIG_OPTION_BOOL(ZInvertDir, false, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ZStepsPerUnit, 4000.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ZMin, 0.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ZMax, 100.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ZMaxSpeed, 3.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ZMaxAccel, 30.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ZDistanceFactor, 1.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ZCorneringDistance, 40.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_BOOL(ZHomeDir, false, ConfigNoProperties)
APRINTER_CONFIG_OPTION_BOOL(ZHomeEndInvert, false, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ZHomeFastMaxDist, 101.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ZHomeRetractDist, 0.8, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ZHomeSlowMaxDist, 1.2, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ZHomeFastSpeed, 2.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ZHomeRetractSpeed, 2.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ZHomeSlowSpeed, 0.6, ConfigNoProperties)

APRINTER_CONFIG_OPTION_BOOL(EInvertDir, true, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(EStepsPerUnit, 928.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(EMin, -40000.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(EMax, 40000.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(EMaxSpeed, 45.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(EMaxAccel, 250.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(EDistanceFactor, 1.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ECorneringDistance, 40.0, ConfigNoProperties)

APRINTER_CONFIG_OPTION_BOOL(UInvertDir, true, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UStepsPerUnit, 660.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UMin, -40000.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UMax, 40000.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UMaxSpeed, 45.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UMaxAccel, 250.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UDistanceFactor, 1.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UCorneringDistance, 55.0, ConfigNoProperties)

APRINTER_CONFIG_OPTION_BOOL(VInvertDir, true, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VStepsPerUnit, 660.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VMin, -40000.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VMax, 40000.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VMaxSpeed, 45.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VMaxAccel, 250.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VDistanceFactor, 1.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VCorneringDistance, 55.0, ConfigNoProperties)

APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterThermistorResistorR, 4700.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterThermistorR0, 100000.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterThermistorBeta, 3960.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterThermistorMinTemp, 10.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterThermistorMaxTemp, 300.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterMinSafeTemp, 20.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterMaxSafeTemp, 280.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterControlInterval, 0.2, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterPidP, 0.047, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterPidI, 0.0006, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterPidD, 0.17, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterPidIStateMin, 0.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterPidIStateMax, 0.4, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterPidDHistory, 0.7, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterObserverInterval, 0.5, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterObserverTolerance, 3.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ExtruderHeaterObserverMinTime, 3.0, ConfigNoProperties)
using ExtruderHeaterPulseInterval = AMBRO_WRAP_DOUBLE(0.2);

APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterThermistorResistorR, 4700.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterThermistorR0, 100000.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterThermistorBeta, 3960.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterThermistorMinTemp, 10.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterThermistorMaxTemp, 300.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterMinSafeTemp, 20.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterMaxSafeTemp, 280.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterControlInterval, 0.2, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterPidP, 0.047, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterPidI, 0.0006, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterPidD, 0.17, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterPidIStateMin, 0.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterPidIStateMax, 0.4, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterPidDHistory, 0.7, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterObserverInterval, 0.5, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterObserverTolerance, 3.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(UxtruderHeaterObserverMinTime, 3.0, ConfigNoProperties)

APRINTER_CONFIG_OPTION_DOUBLE(VxtruderHeaterThermistorResistorR, 4700.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VxtruderHeaterThermistorR0, 100000.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VxtruderHeaterThermistorBeta, 3960.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VxtruderHeaterThermistorMinTemp, 10.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VxtruderHeaterThermistorMaxTemp, 300.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VxtruderHeaterMinSafeTemp, 20.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VxtruderHeaterMaxSafeTemp, 280.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VxtruderHeaterControlInterval, 0.2, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VxtruderHeaterPidP, 0.047, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VxtruderHeaterPidI, 0.0006, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VxtruderHeaterPidD, 0.17, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VxtruderHeaterPidIStateMin, 0.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VxtruderHeaterPidIStateMax, 0.4, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VxtruderHeaterPidDHistory, 0.7, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VxtruderHeaterObserverInterval, 0.5, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VxtruderHeaterObserverTolerance, 3.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(VxtruderHeaterObserverMinTime, 3.0, ConfigNoProperties)

APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterThermistorResistorR, 4700.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterThermistorR0, 10000.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterThermistorBeta, 3480.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterThermistorMinTemp, 10.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterThermistorMaxTemp, 150.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterMinSafeTemp, 20.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterMaxSafeTemp, 120.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterControlInterval, 0.3, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterPidP, 1.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterPidI, 0.012, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterPidD, 2.5, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterPidIStateMin, 0.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterPidIStateMax, 1.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterPidDHistory, 0.8, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterObserverInterval, 0.5, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterObserverTolerance, 1.5, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(BedHeaterObserverMinTime, 3.0, ConfigNoProperties)

using FanSpeedMultiply = AMBRO_WRAP_DOUBLE(1.0 / 255.0);

APRINTER_CONFIG_OPTION_DOUBLE(ProbeOffsetX, -18.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ProbeOffsetY, -31.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ProbeStartHeight, 17.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ProbeLowHeight, 5.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ProbeRetractDist, 1.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ProbeMoveSpeed, 120.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ProbeFastSpeed, 2.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ProbeRetractSpeed, 3.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ProbeSlowSpeed, 0.6, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ProbeP1X, 0.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ProbeP1Y, 31.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ProbeP2X, 0.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ProbeP2Y, 155.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ProbeP3X, 205.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ProbeP3Y, 83.0, ConfigNoProperties)

APRINTER_CONFIG_END

using PrinterParams = PrinterMainParams<
    /*
     * Common parameters.
     */
    PrinterMainSerialParams<
        UINT32_C(250000), // BaudRate
        7, // RecvBufferSizeExp
        7, // SendBufferSizeExp
        GcodeParserParams<8>, // ReceiveBufferSizeExp
        AvrSerialService<true>
    >,
    MegaPin13, // LedPin
    LedBlinkInterval, // LedBlinkInterval
    InactiveTime, // InactiveTime
    SpeedLimitMultiply, // SpeedLimitMultiply
    MaxStepsPerCycle, // MaxStepsPerCycle
    20, // StepperSegmentBufferSize
    20, // EventChannelBufferSize
    15, // LookaheadBufferSize
    9, // LookaheadCommitCount
    ForceTimeout, // ForceTimeout
    double, // FpType
    AvrClockInterruptTimerService<AvrClockTcChannel1B, EventChannelTimerClearance>, // EventChannelTimer
    AvrWatchdogService<
        WDTO_2S
    >,
    PrinterMainSdCardParams<
        SpiSdCardService< // SdCardService
            AvrPin<AvrPortB, 0>, // SsPin
            AvrSpiService< // SpiService
                32 // SpiSpeedDiv
            >
        >,
        FileGcodeParser, // BINARY: BinaryGcodeParser
        GcodeParserParams<8>, // BINARY: BinaryGcodeParserParams<8>
        850, // BufferBaseSize
        100 // MaxCommandSize. BINARY: 43
    >,
    PrinterMainProbeParams<
        MakeTypeList<WrapInt<'X'>, WrapInt<'Y'>>, // PlatformAxesList
        'Z', // ProbeAxis
        MegaPin19, // ProbePin,
        AvrPinInputModePullUp, // ProbePinInputMode
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
    ConstantConfigManagerService,
    ConfigList,
    
    /*
     * Axes.
     */
    MakeTypeList<
        PrinterMainAxisParams<
            'X', // Name
            MegaPin57, // DirPin
            MegaPin58, // StepPin
            MegaPin59, // EnablePin
            XInvertDir,
            XStepsPerUnit, // StepsPerUnit
            XMin, // Min
            XMax, // Max
            XMaxSpeed, // MaxSpeed
            XMaxAccel, // MaxAccel
            XDistanceFactor, // DistanceFactor
            XCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                XHomeDir,
                AxisHomerService< // HomerService
                    MegaPin37, // HomeEndPin
                    AvrPinInputModePullUp, // HomeEndPinInputMode
                    XHomeEndInvert,
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
                AvrClockInterruptTimerService<AvrClockTcChannel3A>, // StepperTimer
                TheAxisDriverPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >,
        PrinterMainAxisParams<
            'Y', // Name
            MegaPin17, // DirPin
            MegaPin5, // StepPin
            MegaPin4, // EnablePin
            YInvertDir,
            YStepsPerUnit, // StepsPerUnit
            YMin, // Min
            YMax, // Max
            YMaxSpeed, // MaxSpeed
            YMaxAccel, // MaxAccel
            YDistanceFactor, // DistanceFactor
            YCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                YHomeDir,
                AxisHomerService< // HomerService
                    MegaPin41, // HomeEndPin
                    AvrPinInputModePullUp, // HomeEndPinInputMode
                    YHomeEndInvert,
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
                AvrClockInterruptTimerService<AvrClockTcChannel3B>, // StepperTimer
                TheAxisDriverPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >,
        PrinterMainAxisParams<
            'Z', // Name
            MegaPin11, // DirPin
            MegaPin16, // StepPin
            MegaPin3, // EnablePin
            ZInvertDir,
            ZStepsPerUnit, // StepsPerUnit
            ZMin, // Min
            ZMax, // Max
            ZMaxSpeed, // MaxSpeed
            ZMaxAccel, // MaxAccel
            ZDistanceFactor, // DistanceFactor
            ZCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                ZHomeDir,
                AxisHomerService< // HomerService
                    MegaPin18, // HomeEndPin
                    AvrPinInputModePullUp, // HomeEndPinInputMode
                    ZHomeEndInvert,
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
                AvrClockInterruptTimerService<AvrClockTcChannel3C>, // StepperTimer
                TheAxisDriverPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >,
        PrinterMainAxisParams<
            'E', // Name
            MegaPin27, // DirPin
            MegaPin28, // StepPin
            MegaPin29, // EnablePin
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
                AvrClockInterruptTimerService<AvrClockTcChannel5A>, // StepperTimer
                TheAxisDriverPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >,
        PrinterMainAxisParams<
            'U', // Name
            MegaPin24, // DirPin
            MegaPin25, // StepPin
            MegaPin26, // EnablePin
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
                AvrClockInterruptTimerService<AvrClockTcChannel5B>, // StepperTimer
                TheAxisDriverPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >,
        PrinterMainAxisParams<
            'V', // Name
            MegaPin60, // DirPin
            MegaPin22, // StepPin
            MegaPin23, // EnablePin
            VInvertDir,
            VStepsPerUnit, // StepsPerUnit
            VMin, // Min
            VMax, // Max
            VMaxSpeed, // MaxSpeed
            VMaxAccel, // MaxAccel
            VDistanceFactor, // DistanceFactor
            VCorneringDistance, // CorneringDistance
            PrinterMainNoHomingParams,
            false, // EnableCartesianSpeedLimit
            32, // StepBits
            AxisDriverService<
                AvrClockInterruptTimerService<AvrClockTcChannel5C>, // StepperTimer
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
            MegaPinA15, // AdcPin
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
                MegaPin2, // OutputPin
                false, // OutputInvert
                ExtruderHeaterPulseInterval, // PulseInterval
                AvrClockInterruptTimerService<AvrClockTcChannel1A> // TimerTemplate
            >
        >,
        PrinterMainHeaterParams<
            'B', // Name
            140, // SetMCommand
            190, // WaitMCommand
            MegaPinA14, // AdcPin
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
            HardPwmService<
                AvrClock8BitPwmService<AvrClockTcChannel2A, MegaPin10>
            >
        >,
        PrinterMainHeaterParams<
            'U', // Name
            404, // SetMCommand
            409, // WaitMCommand
            MegaPinA13, // AdcPin
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
            HardPwmService<
                AvrClock16BitPwmService<AvrClockTcChannel4C, MegaPin8>
            >
        >,
        PrinterMainHeaterParams<
            'V', // Name
            414, // SetMCommand
            419, // WaitMCommand
            MegaPinA12, // AdcPin
            GenericThermistorService< // Thermistor
                VxtruderHeaterThermistorResistorR,
                VxtruderHeaterThermistorR0,
                VxtruderHeaterThermistorBeta,
                VxtruderHeaterThermistorMinTemp,
                VxtruderHeaterThermistorMaxTemp
            >,
            VxtruderHeaterMinSafeTemp, // MinSafeTemp
            VxtruderHeaterMaxSafeTemp, // MaxSafeTemp
            VxtruderHeaterControlInterval, // ControlInterval
            PidControlService<
                VxtruderHeaterPidP, // PidP
                VxtruderHeaterPidI, // PidI
                VxtruderHeaterPidD, // PidD
                VxtruderHeaterPidIStateMin, // PidIStateMin
                VxtruderHeaterPidIStateMax, // PidIStateMax
                VxtruderHeaterPidDHistory // PidDHistory
            >,
            TemperatureObserverService<
                VxtruderHeaterObserverInterval, // ObserverInterval
                VxtruderHeaterObserverTolerance, // ObserverTolerance
                VxtruderHeaterObserverMinTime // ObserverMinTime
            >,
            HardPwmService<
                AvrClock8BitPwmService<AvrClockTcChannel2B, MegaPin9>
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
            HardPwmService<
                AvrClock16BitPwmService<AvrClockTcChannel4A, MegaPin6>
            >
        >,
        PrinterMainFanParams<
            406, // SetMCommand
            407, // OffMCommand
            FanSpeedMultiply, // SpeedMultiply
            HardPwmService<
                AvrClock16BitPwmService<AvrClockTcChannel4B, MegaPin7>
            >
        >
    >
>;

// need to list all used ADC pins here
using AdcPins = MakeTypeList<
    MegaPinA15,
    MegaPinA14,
    MegaPinA13,
    MegaPinA12
>;

static const int AdcRefSel = 1;
static const int AdcPrescaler = 7;

static const int ClockPrescaleDivide = 64;
using ClockTcsList = MakeTypeList<
    AvrClockTcSpec<AvrClockTc1>,
    AvrClockTcSpec<AvrClockTc3>,
    AvrClockTcSpec<AvrClockTc5>,
    AvrClockTcSpec<AvrClockTc2, AvrClockTcMode8BitPwm<1024>>,
    AvrClockTcSpec<AvrClockTc4, AvrClockTcMode16BitPwm<64, 0xfff>>
>;

struct MyContext;
struct MyLoopExtraDelay;
struct Program;

using MyDebugObjectGroup = DebugObjectGroup<MyContext, Program>;
using MyClock = AvrClock<MyContext, Program, ClockPrescaleDivide, ClockTcsList>;
using MyLoop = BusyEventLoop<MyContext, Program, MyLoopExtraDelay>;
using MyPins = AvrPins<MyContext, Program>;
using MyAdc = AvrAdc<MyContext, Program, AdcPins, AdcRefSel, AdcPrescaler>;
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
    uint16_t end;
    
    static Program * self (MyContext c);
};

Program p;

Program * Program::self (MyContext c) { return &p; }
void MyContext::check () const { AMBRO_ASSERT_FORCE(p.end == UINT16_C(0x1234)) }

AMBRO_AVR_CLOCK_ISRS(1, MyClock, MyContext())
AMBRO_AVR_ADC_ISRS(MyAdc, MyContext())
AMBRO_AVR_SERIAL_ISRS(MyPrinter::GetSerial, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_ISRS(3, A, MyPrinter::GetAxisTimer<0>, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_ISRS(3, B, MyPrinter::GetAxisTimer<1>, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_ISRS(3, C, MyPrinter::GetAxisTimer<2>, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_ISRS(5, A, MyPrinter::GetAxisTimer<3>, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_ISRS(5, B, MyPrinter::GetAxisTimer<4>, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_ISRS(5, C, MyPrinter::GetAxisTimer<5>, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_ISRS(1, A, MyPrinter::GetHeaterPwm<0>::TheTimer, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_ISRS(1, B, MyPrinter::GetEventChannelTimer, MyContext())
AMBRO_AVR_SPI_ISRS(MyPrinter::GetSdCard<>::GetSpi, MyContext())
AMBRO_AVR_WATCHDOG_GLOBAL

FILE uart_output;

static int uart_putchar (char ch, FILE *stream)
{
    MyPrinter::GetSerial::sendWaitFinished(MyContext());
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
    
    p.end = UINT16_C(0x1234);
    MyDebugObjectGroup::init(c);
    MyClock::init(c);
    MyLoop::init(c);
    MyPins::init(c);
    MyAdc::init(c);
    MyPrinter::init(c);
    
    MyLoop::run(c);
}
