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

/*
 * $${GENERATED_WARNING}
 */

#include <stdint.h>
#include <stdio.h>

$${PLATFORM_INCLUDES}

static void emergency (void);

#define AMBROLIB_EMERGENCY_ACTION { cli(); emergency(); }
#define AMBROLIB_ABORT_ACTION { while (1); }

#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/WrapDouble.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/system/BusyEventLoop.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/driver/AxisDriver.h>
#include <aprinter/printer/PrinterMain.h>

$${EXTRA_APRINTER_INCLUDES}

using namespace APrinter;

using SpeedLimitMultiply = AMBRO_WRAP_DOUBLE(1.0 / 60.0);
using TheAxisDriverPrecisionParams = $${AxisDriverPrecisionParams};

$${EXTRA_CONSTANTS}

APRINTER_CONFIG_START

$${EXTRA_CONFIG}

APRINTER_CONFIG_END

using PrinterParams = PrinterMainParams<
    /*
     * Common parameters.
     */
    PrinterMainSerialParams<
        UINT32_C($${SerialBaudRate}), // BaudRate,
        $${SerialRecvBufferSizeExp}, // RecvBufferSizeExp
        $${SerialSendBufferSizeExp}, // SendBufferSizeExp
        GcodeParserParams<$${SerialGcodeMaxParts}>, // ReceiveBufferSizeExp
        $${SerialService}
    >,
    $${LedPin}, // LedPin
    LedBlinkInterval, // LedBlinkInterval
    InactiveTime,
    SpeedLimitMultiply, // SpeedLimitMultiply
    MaxStepsPerCycle, // MaxStepsPerCycle
    $${StepperSegmentBufferSize}, // StepperSegmentBufferSize
    $${EventChannelBufferSize}, // EventChannelBufferSize
    $${LookaheadBufferSize}, // LookaheadBufferSize
    $${LookaheadCommitCount}, // LookaheadCommitCount
    ForceTimeout, // ForceTimeout
    $${FpType}, // FpType
    $${EventChannelTimer},
    $${SdCard},
    $${Probe},
    PrinterMainNoCurrentParams,
    $${ConfigManager},
    ConfigList,
    
    /*
     * Axes.
     */
    $${Steppers},
    
    /*
     * Transform and virtual axes.
     */
    PrinterMainNoTransformParams,
    
    /*
     * Heaters.
     */
    $${Heaters},
    
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

$${ADC_CONFIG}

/*
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
*/

$${CLOCK_CONFIG}
$${CLOCK_TCS}

struct MyContext;
struct MyLoopExtraDelay;
struct Program;

using MyDebugObjectGroup = DebugObjectGroup<MyContext, Program>;
using MyClock = $${CLOCK};
using MyLoop = BusyEventLoop<MyContext, Program, MyLoopExtraDelay>;
using MyPins = At91SamPins<MyContext, Program>;
using MyAdc = $${Adc};
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

$${ISRS}

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
    
    __attribute__((used))
    int _write (int file, char *ptr, int len)
    {
#ifndef USB_SERIAL
        if (interrupts_enabled()) {
            MyPrinter::GetSerial::sendWaitFinished(MyContext());
        }
        for (int i = 0; i < len; i++) {
            while (!(UART->UART_SR & UART_SR_TXRDY));
            UART->UART_THR = *(uint8_t *)&ptr[i];
        }
#endif
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
