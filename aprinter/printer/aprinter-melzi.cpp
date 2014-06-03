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
#include <avr/interrupt.h>

static void emergency (void);

#define AMBROLIB_EMERGENCY_ACTION { cli(); emergency(); }
#define AMBROLIB_ABORT_ACTION { while (1); }

#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/Object.h>
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
#include <aprinter/printer/pwm/SoftPwm.h>
#include <aprinter/printer/pwm/HardPwm.h>
#include <aprinter/printer/thermistor/GenericThermistor.h>
#include <aprinter/printer/temp_control/PidControl.h>
#include <aprinter/printer/temp_control/BinaryControl.h>

using namespace APrinter;

using LedBlinkInterval = AMBRO_WRAP_DOUBLE(0.5);
using DefaultInactiveTime = AMBRO_WRAP_DOUBLE(60.0);
using SpeedLimitMultiply = AMBRO_WRAP_DOUBLE(1.0 / 60.0);
using MaxStepsPerCycle = AMBRO_WRAP_DOUBLE(0.00137); // max stepping frequency relative to F_CPU
using ForceTimeout = AMBRO_WRAP_DOUBLE(0.1);
using TheAxisDriverPrecisionParams = AxisDriverAvrPrecisionParams;
using EventChannelTimerClearance = AMBRO_WRAP_DOUBLE(0.002);

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

using ExtruderHeaterThermistorResistorR = AMBRO_WRAP_DOUBLE(4700.0);
using ExtruderHeaterThermistorR0 = AMBRO_WRAP_DOUBLE(100000.0);
using ExtruderHeaterThermistorBeta = AMBRO_WRAP_DOUBLE(3960.0);
using ExtruderHeaterThermistorMinTemp = AMBRO_WRAP_DOUBLE(10.0);
using ExtruderHeaterThermistorMaxTemp = AMBRO_WRAP_DOUBLE(300.0);
using ExtruderHeaterMinSafeTemp = AMBRO_WRAP_DOUBLE(20.0);
using ExtruderHeaterMaxSafeTemp = AMBRO_WRAP_DOUBLE(280.0);
using ExtruderHeaterControlInterval = AMBRO_WRAP_DOUBLE(0.2);
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

using PrinterParams = PrinterMainParams<
    /*
     * Common parameters.
     */
    PrinterMainSerialParams<
        UINT32_C(115200), // BaudRate
        7, // RecvBufferSizeExp
        8, // SendBufferSizeExp
        GcodeParserParams<8>, // ReceiveBufferSizeExp
        AvrSerialService<true>
    >,
    AvrPin<AvrPortA, 4>, // LedPin
    LedBlinkInterval, // LedBlinkInterval
    DefaultInactiveTime, // DefaultInactiveTime
    SpeedLimitMultiply, // SpeedLimitMultiply
    MaxStepsPerCycle, // MaxStepsPerCycle
    32, // StepperSegmentBufferSize
    32, // EventChannelBufferSize
    16, // LookaheadBufferSize
    8, // LookaheadCommitCount
    ForceTimeout, // ForceTimeout
    double, // FpType
    AvrClockInterruptTimerService<AvrClockTcChannel2A, EventChannelTimerClearance>, // EventChannelTimer
    AvrWatchdogService<
        WDTO_2S
    >,
    PrinterMainSdCardParams<
        SpiSdCardService< // SdCardService
            AvrPin<AvrPortA, 0>, // SsPin
            AvrSpiService< // SpiService
                32 // SpiSpeedDiv
            >
        >,
        FileGcodeParser, // BINARY: BinaryGcodeParser
        GcodeParserParams<8>, // BINARY: BinaryGcodeParserParams<8>
        2048, // BufferBaseSize
        100 // MaxCommandSize. BINARY: 43
    >,
    PrinterMainNoProbeParams,
    PrinterMainNoCurrentParams,
    
    /*
     * Axes.
     */
    MakeTypeList<
        PrinterMainAxisParams<
            'X', // Name
            AvrPin<AvrPortC, 5>, // DirPin
            AvrPin<AvrPortD, 7>, // StepPin
            AvrPin<AvrPortD, 6>, // EnablePin
            true, // InvertDir
            XDefaultStepsPerUnit, // StepsPerUnit
            XDefaultMin, // Min
            XDefaultMax, // Max
            XDefaultMaxSpeed, // MaxSpeed
            XDefaultMaxAccel, // MaxAccel
            XDefaultDistanceFactor, // DistanceFactor
            XDefaultCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                AvrPin<AvrPortC, 2>, // HomeEndPin
                AvrPinInputModePullUp, // HomeEndPinInputMode
                false, // HomeEndInvert
                false, // HomeDir
                AxisHomerService< // HomerService
                    XDefaultHomeFastMaxDist, // HomeFastMaxDist
                    XDefaultHomeRetractDist, // HomeRetractDist
                    XDefaultHomeSlowMaxDist, // HomeSlowMaxDist
                    XDefaultHomeFastSpeed, // HomeFastSpeed
                    XDefaultHomeRetractSpeed, // HomeRetractSpeed
                    XDefaultHomeSlowSpeed // HomeSlowSpeed
                >
            >,
            true, // EnableCartesianSpeedLimit
            32, // StepBits
            AxisDriverService<
                AvrClockInterruptTimerService<AvrClockTcChannel0A>, // StepperTimer
                TheAxisDriverPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >,
        PrinterMainAxisParams<
            'Y', // Name
            AvrPin<AvrPortC, 7>, // DirPin
            AvrPin<AvrPortC, 6>, // StepPin
            AvrPin<AvrPortD, 6>, // EnablePin
            true, // InvertDir
            YDefaultStepsPerUnit, // StepsPerUnit
            YDefaultMin, // Min
            YDefaultMax, // Max
            YDefaultMaxSpeed, // MaxSpeed
            YDefaultMaxAccel, // MaxAccel
            YDefaultDistanceFactor, // DistanceFactor
            YDefaultCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                AvrPin<AvrPortC, 3>, // HomeEndPin
                AvrPinInputModePullUp, // HomeEndPinInputMode
                false, // HomeEndInvert
                false, // HomeDir
                AxisHomerService< // HomerService
                    YDefaultHomeFastMaxDist, // HomeFastMaxDist
                    YDefaultHomeRetractDist, // HomeRetractDist
                    YDefaultHomeSlowMaxDist, // HomeSlowMaxDist
                    YDefaultHomeFastSpeed, // HomeFastSpeed
                    YDefaultHomeRetractSpeed, // HomeRetractSpeed
                    YDefaultHomeSlowSpeed // HomeSlowSpeed
                >
            >,
            true, // EnableCartesianSpeedLimit
            32, // StepBits
            AxisDriverService<
                AvrClockInterruptTimerService<AvrClockTcChannel0B>, // StepperTimer
                TheAxisDriverPrecisionParams // PrecisionParams
            >,
            PrinterMainNoMicroStepParams
        >,
        PrinterMainAxisParams<
            'Z', // Name
            AvrPin<AvrPortB, 2>, // DirPin
            AvrPin<AvrPortB, 3>, // StepPin
            AvrPin<AvrPortA, 5>, // EnablePin
            false, // InvertDir
            ZDefaultStepsPerUnit, // StepsPerUnit
            ZDefaultMin, // Min
            ZDefaultMax, // Max
            ZDefaultMaxSpeed, // MaxSpeed
            ZDefaultMaxAccel, // MaxAccel
            ZDefaultDistanceFactor, // DistanceFactor
            ZDefaultCorneringDistance, // CorneringDistance
            PrinterMainHomingParams<
                AvrPin<AvrPortC, 4>, // HomeEndPin
                AvrPinInputModePullUp, // HomeEndPinInputMode
                false, // HomeEndInvert
                false, // HomeDir
                AxisHomerService< // HomerService
                    ZDefaultHomeFastMaxDist, // HomeFastMaxDist
                    ZDefaultHomeRetractDist, // HomeRetractDist
                    ZDefaultHomeSlowMaxDist, // HomeSlowMaxDist
                    ZDefaultHomeFastSpeed, // HomeFastSpeed
                    ZDefaultHomeRetractSpeed, // HomeRetractSpeed
                    ZDefaultHomeSlowSpeed // HomeSlowSpeed
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
            'E', // Name
            AvrPin<AvrPortB, 0>, // DirPin
            AvrPin<AvrPortB, 1>, // StepPin
            AvrPin<AvrPortD, 6>, // EnablePin
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
                AvrClockInterruptTimerService<AvrClockTcChannel3B>, // StepperTimer
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
            AvrPin<AvrPortA, 7>, // AdcPin
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
            HardPwmService<
                AvrClock16BitPwmService<AvrClockTcChannel1A, AvrPin<AvrPortD, 5>>
            >
        >,
        PrinterMainHeaterParams<
            'B', // Name
            140, // SetMCommand
            190, // WaitMCommand
            304, // SetConfigMCommand
            AvrPin<AvrPortA, 6>, // AdcPin
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
            HardPwmService<
                AvrClock16BitPwmService<AvrClockTcChannel1B, AvrPin<AvrPortD, 4>>
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
                AvrPin<AvrPortB, 4>, // OutputPin
                false, // OutputInvert
                FanPulseInterval, // PulseInterval
                AvrClockInterruptTimerService<AvrClockTcChannel2B> // TimerTemplate
            >
        >
    >
>;

// need to list all used ADC pins here
using AdcPins = MakeTypeList<
    AvrPin<AvrPortA, 6>,
    AvrPin<AvrPortA, 7>
>;

static const int AdcRefSel = 1;
static const int AdcPrescaler = 7;

static const int ClockPrescaleDivide = 64;
using ClockTcsList = MakeTypeList<
    AvrClockTcSpec<AvrClockTc3>,
    AvrClockTcSpec<AvrClockTc0>,
    AvrClockTcSpec<AvrClockTc2>,
    AvrClockTcSpec<AvrClockTc1, AvrClockTcMode16BitPwm<64, 0xfff>>
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

AMBRO_AVR_CLOCK_ISRS(3, MyClock, MyContext())
AMBRO_AVR_ADC_ISRS(MyAdc, MyContext())
AMBRO_AVR_SERIAL_ISRS(MyPrinter::GetSerial, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_ISRS(0, A, MyPrinter::GetAxisTimer<0>, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_ISRS(0, B, MyPrinter::GetAxisTimer<1>, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_ISRS(3, A, MyPrinter::GetAxisTimer<2>, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_ISRS(3, B, MyPrinter::GetAxisTimer<3>, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_ISRS(2, A, MyPrinter::GetEventChannelTimer, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_ISRS(2, B, MyPrinter::GetFanPwm<0>::TheTimer, MyContext())
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
