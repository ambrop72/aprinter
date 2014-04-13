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
#include <aprinter/meta/Object.h>
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
#include <aprinter/printer/thermistor/GenericThermistor.h>
#include <aprinter/printer/temp_control/PidControl.h>
#include <aprinter/printer/temp_control/BinaryControl.h>
#include <aprinter/printer/transform/CoreXyTransform.h>
#include <aprinter/printer/microstep/A4988MicroStep.h>
#include <aprinter/printer/teensy3_pins.h>

using namespace APrinter;

static int const AdcADiv = 3;

using LedBlinkInterval = AMBRO_WRAP_DOUBLE(0.5);
using DefaultInactiveTime = AMBRO_WRAP_DOUBLE(60.0);
using SpeedLimitMultiply = AMBRO_WRAP_DOUBLE(1.0 / 60.0);
using MaxStepsPerCycle = AMBRO_WRAP_DOUBLE(0.0017);
using ForceTimeout = AMBRO_WRAP_DOUBLE(0.1);
using TheAxisStepperPrecisionParams = AxisStepperDuePrecisionParams;

// Cartesian axes invloved in CoreXY are X and Y.
// Any configuration here is related to a cartesian
// axis, not to a "corresponding stepper". Actually,
// anything that is specific to a stepper will be named
// with A or B, and not misnamed with X or Y.
// NOTE: Min and Max here are not implemented properly yet,
// they are only used to compute stepper limits.
// NOTE: The speed limits here are used in addition to the
// stepper speed limits. Specifying INFINITY will not limit
// the speed at cartesian axis level.

using XMinPos = AMBRO_WRAP_DOUBLE(0.0);
using XMaxPos = AMBRO_WRAP_DOUBLE(750.0);
using XMaxSpeed = AMBRO_WRAP_DOUBLE(INFINITY);

using YMinPos = AMBRO_WRAP_DOUBLE(0.0);
using YMaxPos = AMBRO_WRAP_DOUBLE(750.0);
using YMaxSpeed = AMBRO_WRAP_DOUBLE(INFINITY);

// CoreXY steppers are called A and B.

using ADefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(200.0);
using ADefaultMin = AMBRO_WRAP_DOUBLE(1.01 * (XMinPos::value() + YMinPos::value()));
using ADefaultMax = AMBRO_WRAP_DOUBLE(1.01 * (XMaxPos::value() + YMaxPos::value()));
using ADefaultMaxSpeed = AMBRO_WRAP_DOUBLE(200.0);
using ADefaultMaxAccel = AMBRO_WRAP_DOUBLE(1500.0);
using ADefaultDistanceFactor = AMBRO_WRAP_DOUBLE(1.0);
using ADefaultCorneringDistance = AMBRO_WRAP_DOUBLE(40.0);

using BDefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(200.0);
using BDefaultMin = AMBRO_WRAP_DOUBLE(1.01 * (XMinPos::value() - YMaxPos::value()));
using BDefaultMax = AMBRO_WRAP_DOUBLE(1.01 * (XMaxPos::value() - YMinPos::value()));
using BDefaultMaxSpeed = AMBRO_WRAP_DOUBLE(200.0);
using BDefaultMaxAccel = AMBRO_WRAP_DOUBLE(1500.0);
using BDefaultDistanceFactor = AMBRO_WRAP_DOUBLE(1.0);
using BDefaultCorneringDistance = AMBRO_WRAP_DOUBLE(40.0);

// Z is configured as a normal cartesian axis, not invloved
// in the CoreXY transform.

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

using ExtruderHeaterThermistorResistorR = AMBRO_WRAP_DOUBLE(4700.0);
using ExtruderHeaterThermistorR0 = AMBRO_WRAP_DOUBLE(100000.0);
using ExtruderHeaterThermistorBeta = AMBRO_WRAP_DOUBLE(3960.0);
using ExtruderHeaterThermistorMinTemp = AMBRO_WRAP_DOUBLE(10.0);
using ExtruderHeaterThermistorMaxTemp = AMBRO_WRAP_DOUBLE(300.0);
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

using DummySegmentsPerSecond = AMBRO_WRAP_DOUBLE(0.0);

using PrinterParams = PrinterMainParams<
    /*
     * Common parameters.
     */
    PrinterMainSerialParams<
        UINT32_C(0), // BaudRate,
        8, // RecvBufferSizeExp
        8, // SendBufferSizeExp
        GcodeParserParams<16>, // ReceiveBufferSizeExp
        TeensyUsbSerial,
        TeensyUsbSerialParams
    >,
    TeensyPin13, // LedPin
    LedBlinkInterval, // LedBlinkInterval
    DefaultInactiveTime, // DefaultInactiveTime
    SpeedLimitMultiply, // SpeedLimitMultiply
    MaxStepsPerCycle, // MaxStepsPerCycle
    32, // StepperSegmentBufferSize
    32, // EventChannelBufferSize
    28, // LookaheadBufferSize
    10, // LookaheadCommitCount
    ForceTimeout, // ForceTimeout
    float, // FpType
    Mk20ClockInterruptTimer_Ftm0_Ch0, // EventChannelTimer
    Mk20Watchdog,
    Mk20WatchdogParams<2000, 0>,
    PrinterMainNoSdCardParams,
    PrinterMainNoProbeParams,
    PrinterMainNoCurrentParams,
    
    /*
     * Axes.
     */
    MakeTypeList<
        PrinterMainAxisParams<
            'A', // Name
            TeensyPin13, // DirPin
            TeensyPin14, // StepPin
            TeensyPin19, // EnablePin
            false, // InvertDir
            ADefaultStepsPerUnit, // StepsPerUnit
            ADefaultMin, // Min
            ADefaultMax, // Max
            ADefaultMaxSpeed, // MaxSpeed
            ADefaultMaxAccel, // MaxAccel
            ADefaultDistanceFactor, // DistanceFactor
            ADefaultCorneringDistance, // CorneringDistance
            PrinterMainNoHomingParams,
            false, // EnableCartesianSpeedLimit
            32, // StepBits
            AxisStepperParams<
                Mk20ClockInterruptTimer_Ftm0_Ch1, // StepperTimer,
                TheAxisStepperPrecisionParams // PrecisionParams
            >,
            PrinterMainMicroStepParams<
               A4988MicroStep, // MicroStepTemplate
               A4988MicroStepParams< // MicroStepParams
                   TeensyPin7, // Ms1Pin
                   TeensyPin8, // Ms2Pin
                   TeensyPin9 // Ms3Pin
               >,
               16 // MicroSteps
           >
        >,
        PrinterMainAxisParams<
            'B', // Name
            TeensyPin15, // DirPin
            TeensyPin16, // StepPin
            TeensyPin20, // EnablePin
            false, // InvertDir
            BDefaultStepsPerUnit, // StepsPerUnit
            BDefaultMin, // Min
            BDefaultMax, // Max
            BDefaultMaxSpeed, // MaxSpeed
            BDefaultMaxAccel, // MaxAccel
            BDefaultDistanceFactor, // DistanceFactor
            BDefaultCorneringDistance, // CorneringDistance
            PrinterMainNoHomingParams,
            false, // EnableCartesianSpeedLimit
            32, // StepBits
            AxisStepperParams<
                Mk20ClockInterruptTimer_Ftm0_Ch2, // StepperTimer
                TheAxisStepperPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >,
        PrinterMainAxisParams<
            'Z', // Name
            TeensyPin0, // DirPin
            TeensyPin0, // StepPin
            TeensyPin0, // EnablePin
            false, // InvertDir
            ZDefaultStepsPerUnit, // StepsPerUnit
            ZDefaultMin, // Min
            ZDefaultMax, // Max
            ZDefaultMaxSpeed, // MaxSpeed
            ZDefaultMaxAccel, // MaxAccel
            ZDefaultDistanceFactor, // DistanceFactor
            ZDefaultCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                TeensyPin15, // HomeEndPin
                Mk20PinInputModePullUp, // HomeEndPinInputMode
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
                Mk20ClockInterruptTimer_Ftm0_Ch3, // StepperTimer
                TheAxisStepperPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >,
        PrinterMainAxisParams<
            'E', // Name
            TeensyPin0, // DirPin
            TeensyPin0, // StepPin
            TeensyPin0, // EnablePin
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
                XMaxSpeed
            >,
            PrinterMainVirtualAxisParams<
                'Y', // Name
                YMinPos,
                YMaxPos,
                YMaxSpeed
            >
        >,
        MakeTypeList<WrapInt<'A'>, WrapInt<'B'>>,
        DummySegmentsPerSecond,
        CoreXyTransform,
        CoreXyTransformParams
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
            TeensyPin17, // OutputPin
            true, // OutputInvert
            GenericThermistor< // Thermistor
                ExtruderHeaterThermistorResistorR,
                ExtruderHeaterThermistorR0,
                ExtruderHeaterThermistorBeta,
                ExtruderHeaterThermistorMinTemp,
                ExtruderHeaterThermistorMaxTemp
            >,
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
            GenericThermistor< // Thermistor
                BedHeaterThermistorResistorR,
                BedHeaterThermistorR0,
                BedHeaterThermistorBeta,
                BedHeaterThermistorMinTemp,
                BedHeaterThermistorMaxTemp
            >,
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
            TeensyPin18, // OutputPin
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

AMBRO_MK20_CLOCK_FTM0_GLOBAL(MyClock, MyContext())
AMBRO_MK20_CLOCK_FTM1_GLOBAL(MyClock, MyContext())

AMBRO_MK20_WATCHDOG_GLOBAL(MyPrinter::GetWatchdog)
AMBRO_MK20_ADC_ISRS(MyAdc, MyContext())
AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH0_GLOBAL(MyPrinter::GetEventChannelTimer, MyContext())
AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH1_GLOBAL(MyPrinter::GetAxisTimer<0>, MyContext())
AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH2_GLOBAL(MyPrinter::GetAxisTimer<1>, MyContext())
AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH3_GLOBAL(MyPrinter::GetAxisTimer<2>, MyContext())
AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH4_GLOBAL(MyPrinter::GetAxisTimer<3>, MyContext())
AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH5_GLOBAL(MyPrinter::GetHeaterTimer<0>, MyContext())
//AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH6_GLOBAL(MyPrinter::GetHeaterTimer<1>, MyContext())
AMBRO_MK20_CLOCK_INTERRUPT_TIMER_FTM0_CH7_GLOBAL(MyPrinter::GetFanTimer<0>, MyContext())

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
