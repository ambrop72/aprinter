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

#include <aprinter/platform/teensy3/teensy3_support.h>

static void emergency (void);

#define AMBROLIB_EMERGENCY_ACTION { cli(); emergency(); }
#define AMBROLIB_ABORT_ACTION { while (1); }

#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/system/BusyEventLoop.h>
#include <aprinter/system/Mk20Clock.h>
#include <aprinter/system/Mk20Pins.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/system/Mk20Adc.h>
#include <aprinter/system/Mk20Watchdog.h>
#include <aprinter/system/TeensyEeprom.h>
#include <aprinter/system/TeensyUsbSerial.h>
#include <aprinter/devices/SpiSdCard.h>
#include <aprinter/driver/AxisDriver.h>
#include <aprinter/printer/PrinterMain.h>
#include <aprinter/printer/AxisHomer.h>
#include <aprinter/printer/TemperatureObserver.h>
#include <aprinter/printer/pwm/SoftPwm.h>
#include <aprinter/printer/thermistor/GenericThermistor.h>
#include <aprinter/printer/temp_control/PidControl.h>
#include <aprinter/printer/temp_control/BinaryControl.h>
#include <aprinter/printer/config_manager/ConstantConfigManager.h>
#include <aprinter/printer/config_manager/RuntimeConfigManager.h>
#include <aprinter/printer/transform/DeltaTransform.h>
#include <aprinter/printer/config_store/EepromConfigStore.h>
#include <aprinter/board/teensy3_pins.h>

using namespace APrinter;

APRINTER_CONFIG_START

static int const AdcADiv = 3;

using LedBlinkInterval = AMBRO_WRAP_DOUBLE(0.5);
using SpeedLimitMultiply = AMBRO_WRAP_DOUBLE(1.0 / 60.0);
using TheAxisDriverPrecisionParams = AxisDriverDuePrecisionParams;
using EventChannelTimerClearance = AMBRO_WRAP_DOUBLE(0.002);

APRINTER_CONFIG_OPTION_DOUBLE(MaxStepsPerCycle, 0.0017, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ForceTimeout, 0.1, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(InactiveTime, 8.0 * 60.0, ConfigNoProperties)

APRINTER_CONFIG_OPTION_DOUBLE(XMinPos, -INFINITY, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(XMaxPos, INFINITY, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(XMaxSpeed, INFINITY, ConfigNoProperties)

APRINTER_CONFIG_OPTION_DOUBLE(YMinPos, -INFINITY, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(YMaxPos, INFINITY, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(YMaxSpeed, INFINITY, ConfigNoProperties)

APRINTER_CONFIG_OPTION_DOUBLE(ZMinPos, -INFINITY, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ZMaxPos, INFINITY, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ZMaxSpeed, INFINITY, ConfigNoProperties)

APRINTER_CONFIG_OPTION_BOOL(ABCInvertDir, false, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ABCStepsPerUnit, 100.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ABCMin, 0.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ABCMax, 360.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ABCMaxSpeed, 200.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ABCMaxAccel, 9000.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ABCDistanceFactor, 1.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ABCCorneringDistance, 40.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_BOOL(ABCHomeDir, true, ConfigNoProperties)
APRINTER_CONFIG_OPTION_BOOL(ABCHomeEndInvert, false, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ABCHomeFastMaxDist, 363.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ABCHomeRetractDist, 3.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ABCHomeSlowMaxDist, 5.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ABCHomeFastSpeed, 70.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ABCHomeRetractSpeed, 70.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ABCHomeSlowSpeed, 5.0, ConfigNoProperties)

APRINTER_CONFIG_OPTION_BOOL(EInvertDir, false, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(EStepsPerUnit, 928.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(EMin, -40000.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(EMax, 40000.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(EMaxSpeed, 45.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(EMaxAccel, 250.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(EDistanceFactor, 1.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(ECorneringDistance, 40.0, ConfigNoProperties)

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
using BedHeaterPulseInterval = AMBRO_WRAP_DOUBLE(0.3);

using FanSpeedMultiply = AMBRO_WRAP_DOUBLE(1.0 / 255.0);
using FanPulseInterval = AMBRO_WRAP_DOUBLE(0.04);

APRINTER_CONFIG_OPTION_DOUBLE(DeltaDiagonalRod, 214.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(DeltaSmoothRodOffset, 145.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(DeltaEffectorOffset, 19.9, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(DeltaCarriageOffset, 19.5, ConfigNoProperties)
using DeltaSegmentsPerSecond = AMBRO_WRAP_DOUBLE(100.0); 
using DeltaMinSplitLength = AMBRO_WRAP_DOUBLE(0.1); 
using DeltaMaxSplitLength = AMBRO_WRAP_DOUBLE(4.0); 

APRINTER_CONFIG_END

using PrinterParams = PrinterMainParams<
    /*
     * Common parameters.
     */
    PrinterMainSerialParams<
        UINT32_C(0), // BaudRate,
        8, // RecvBufferSizeExp
        8, // SendBufferSizeExp
        GcodeParserParams<16>, // ReceiveBufferSizeExp
        TeensyUsbSerialService
    >,
    TeensyPin13, // LedPin
    LedBlinkInterval, // LedBlinkInterval
    InactiveTime, // InactiveTime
    SpeedLimitMultiply, // SpeedLimitMultiply
    MaxStepsPerCycle, // MaxStepsPerCycle
    32, // StepperSegmentBufferSize
    32, // EventChannelBufferSize
    28, // LookaheadBufferSize
    10, // LookaheadCommitCount
    ForceTimeout, // ForceTimeout
    float, // FpType
    Mk20ClockInterruptTimerService<Mk20ClockFTM0, 0, EventChannelTimerClearance>, // EventChannelTimer
    Mk20WatchdogService<2000, 0>,
    PrinterMainNoSdCardParams,
    PrinterMainNoProbeParams,
    PrinterMainNoCurrentParams,
    RuntimeConfigManagerService<
        EepromConfigStoreService<
            TeensyEepromService<
                2048, // Size
                16 // FakeBlockSize
            >,
            0, // StartBlock
            128 // EndBlock
        >
    >,
    ConfigList,
    
    /*
     * Axes.
     */
    MakeTypeList<
        PrinterMainAxisParams<
            'A', // Name
            TeensyPin1, // DirPin
            TeensyPin0, // StepPin
            TeensyPin8, // EnablePin
            ABCInvertDir,
            ABCStepsPerUnit, // StepsPerUnit
            ABCMin, // Min
            ABCMax, // Max
            ABCMaxSpeed, // MaxSpeed
            ABCMaxAccel, // MaxAccel
            ABCDistanceFactor, // DistanceFactor
            ABCCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                ABCHomeDir,
                AxisHomerService< // HomerService
                    TeensyPin12, // HomeEndPin
                    Mk20PinInputModePullUp, // HomeEndPinInputMode
                    ABCHomeEndInvert,
                    ABCHomeFastMaxDist, // HomeFastMaxDist
                    ABCHomeRetractDist, // HomeRetractDist
                    ABCHomeSlowMaxDist, // HomeSlowMaxDist
                    ABCHomeFastSpeed, // HomeFastSpeed
                    ABCHomeRetractSpeed, // HomeRetractSpeed
                    ABCHomeSlowSpeed // HomeSlowSpeed
                >
            >,
            false, // EnableCartesianSpeedLimit
            32, // StepBits
            AxisDriverService<
                Mk20ClockInterruptTimerService<Mk20ClockFTM0, 1>, // StepperTimer,
                TheAxisDriverPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >,
        PrinterMainAxisParams<
            'B', // Name
            TeensyPin3, // DirPin
            TeensyPin2, // StepPin
            TeensyPin9, // EnablePin
            ABCInvertDir,
            ABCStepsPerUnit, // StepsPerUnit
            ABCMin, // Min
            ABCMax, // Max
            ABCMaxSpeed, // MaxSpeed
            ABCMaxAccel, // MaxAccel
            ABCDistanceFactor, // DistanceFactor
            ABCCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                ABCHomeDir,
                AxisHomerService< // HomerService
                    TeensyPin14, // HomeEndPin
                    Mk20PinInputModePullUp, // HomeEndPinInputMode
                    ABCHomeEndInvert,
                    ABCHomeFastMaxDist, // HomeFastMaxDist
                    ABCHomeRetractDist, // HomeRetractDist
                    ABCHomeSlowMaxDist, // HomeSlowMaxDist
                    ABCHomeFastSpeed, // HomeFastSpeed
                    ABCHomeRetractSpeed, // HomeRetractSpeed
                    ABCHomeSlowSpeed // HomeSlowSpeed
                >
            >,
            false, // EnableCartesianSpeedLimit
            32, // StepBits
            AxisDriverService<
                Mk20ClockInterruptTimerService<Mk20ClockFTM0, 2>, // StepperTimer
                TheAxisDriverPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >,
        PrinterMainAxisParams<
            'C', // Name
            TeensyPin5, // DirPin
            TeensyPin4, // StepPin
            TeensyPin10, // EnablePin
            ABCInvertDir,
            ABCStepsPerUnit, // StepsPerUnit
            ABCMin, // Min
            ABCMax, // Max
            ABCMaxSpeed, // MaxSpeed
            ABCMaxAccel, // MaxAccel
            ABCDistanceFactor, // DistanceFactor
            ABCCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                ABCHomeDir,
                AxisHomerService< // HomerService
                    TeensyPin15, // HomeEndPin
                    Mk20PinInputModePullUp, // HomeEndPinInputMode
                    ABCHomeEndInvert,
                    ABCHomeFastMaxDist, // HomeFastMaxDist
                    ABCHomeRetractDist, // HomeRetractDist
                    ABCHomeSlowMaxDist, // HomeSlowMaxDist
                    ABCHomeFastSpeed, // HomeFastSpeed
                    ABCHomeRetractSpeed, // HomeRetractSpeed
                    ABCHomeSlowSpeed // HomeSlowSpeed
                >
            >,
            false, // EnableCartesianSpeedLimit
            32, // StepBits
            AxisDriverService<
                Mk20ClockInterruptTimerService<Mk20ClockFTM0, 3>, // StepperTimer
                TheAxisDriverPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >,
        PrinterMainAxisParams<
            'E', // Name
            TeensyPin7, // DirPin
            TeensyPin6, // StepPin
            TeensyPin11, // EnablePin
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
                Mk20ClockInterruptTimerService<Mk20ClockFTM0, 4>, // StepperTimer
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
        DeltaTransformService<
            DeltaDiagonalRod,
            DeltaSmoothRodOffset,
            DeltaEffectorOffset,
            DeltaCarriageOffset,
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
            TeensyPinA2, // AdcPin
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
                TeensyPin17, // OutputPin
                true, // OutputInvert
                ExtruderHeaterPulseInterval, // PulseInterval
                Mk20ClockInterruptTimerService<Mk20ClockFTM0, 5> // TimerTemplate
            >
        >
#if 0
        PrinterMainHeaterParams<
            'B', // Name
            140, // SetMCommand
            190, // WaitMCommand
            NOOONE, // AdcPin
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
                NOOONE, // OutputPin
                true, // OutputInvert
                BedHeaterPulseInterval, // PulseInterval
                Mk20ClockInterruptTimerService<Mk20ClockFTM0, 6> // TimerTemplate
            >
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
            FanSpeedMultiply, // SpeedMultiply
            SoftPwmService<
                TeensyPin18, // OutputPin
                false, // OutputInvert
                FanPulseInterval, // PulseInterval
                Mk20ClockInterruptTimerService<Mk20ClockFTM0, 7> // TimerTemplate
            >
        >
    >
>;

// need to list all used ADC pins here
using AdcPins = MakeTypeList<TeensyPinA2>;

static const int clock_timer_prescaler = 4;
using ClockFtmsList = MakeTypeList<
    Mk20ClockFtmSpec<Mk20ClockFTM0>,
    Mk20ClockFtmSpec<Mk20ClockFTM1>
>;

struct MyContext;
struct MyLoopExtraDelay;
struct Program;

using MyDebugObjectGroup = DebugObjectGroup<MyContext, Program>;
using MyClock = Mk20Clock<MyContext, Program, clock_timer_prescaler, ClockFtmsList>;
using MyLoop = BusyEventLoop<MyContext, Program, MyLoopExtraDelay>;
using MyPins = Mk20Pins<MyContext, Program>;
using MyAdc = Mk20Adc<MyContext, Program, AdcPins, AdcADiv>;
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

AMBRO_MK20_CLOCK_FTM_GLOBAL(0, MyClock, MyContext())
AMBRO_MK20_CLOCK_FTM_GLOBAL(1, MyClock, MyContext())

AMBRO_MK20_WATCHDOG_GLOBAL(MyPrinter::GetWatchdog)
AMBRO_MK20_ADC_ISRS(MyAdc, MyContext())
AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(Mk20ClockFTM0, 0, MyPrinter::GetEventChannelTimer, MyContext())
AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(Mk20ClockFTM0, 1, MyPrinter::GetAxisTimer<0>, MyContext())
AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(Mk20ClockFTM0, 2, MyPrinter::GetAxisTimer<1>, MyContext())
AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(Mk20ClockFTM0, 3, MyPrinter::GetAxisTimer<2>, MyContext())
AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(Mk20ClockFTM0, 4, MyPrinter::GetAxisTimer<3>, MyContext())
AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(Mk20ClockFTM0, 5, MyPrinter::GetHeaterPwm<0>::TheTimer, MyContext())
//AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(Mk20ClockFTM0, 6, MyPrinter::GetHeaterPwm<1>::TheTimer, MyContext())
AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(Mk20ClockFTM0, 7, MyPrinter::GetFanPwm<0>::TheTimer, MyContext())

static void emergency (void)
{
    MyPrinter::emergency();
}

extern "C" { void usb_init (void); }

int main ()
{
    usb_init();
    
    MyContext c;
    
    MyDebugObjectGroup::init(c);
    MyClock::init(c);
    MyLoop::init(c);
    MyPins::init(c);
    MyAdc::init(c);
    MyPrinter::init(c);
    
    MyLoop::run(c);
}
