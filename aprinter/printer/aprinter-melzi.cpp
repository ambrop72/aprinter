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
#include <aprinter/printer/PrinterMain.h>
#include <aprinter/printer/thermistor/GenericThermistor.h>
#include <aprinter/printer/temp_control/PidControl.h>
#include <aprinter/printer/temp_control/BinaryControl.h>

using namespace APrinter;

/*
 * Configuration values are in units based on millimeters, seconds and kelvins,
 * unless otherwise specified. If you don't know what some configuration option does,
 * you probably don't need to change it.
 * 
 * Floating point configuration values need to be specified outside of the primary
 * configuration expression by using the AMBRO_WRAP_DOUBLE macro, due to a limitation of
 * of C++ templates (inability to use floating point template arguments).
 */

/*
 * Explanation of common parameters.
 * 
 * BaudRate
 * The baud rate for the serial port. Do not use more than 115200, as that may result in
 * received bytes being dropped due to pending interrupts.
 * 
 * ReceiveBufferSizeExp
 * Size of the serial receive buffer, used for parsing commands as well as holding subsequent
 * commands while a command is being processed.
 * 
 * LedPin, LedBlinkInterval
 * The pin of the heartbeat LED, and the time it takes to toggle the state of the LED.
 * Be careful when raising this - the heartbeat LED code feeds the watchdog timer, which has
 * a timeout of two seconds by default,
 * 
 * DefaultInactiveTime
 * The default time the printer needs to be idle before the steppers are shut down.
 * This time can be adjusted at runtime using the M18 or M84 g-code commands.
 * 
 * SpeedLimitMultiply
 * The euclidean speed limit specified with motion g-codes (F parameter in G0 and G1)
 * is multiplied by this value before being interpreted as a speed in mm/s. You probably
 * want SpeedLimitMultiply=1.0/60.0 so that the F values are interpreted as mm/min.
 * 
 * MaxStepsPerCycle
 * This parameter affects the maximum stepping frequency, which is computed as
 * F_CPU*MaxStepsPerCycle. If this parameter is too high, the printer will malfunction
 * when trying to drive axes at high speeds. Even if stepping works reliably, may still
 * be too high, as there needs to be enough processor time left for tasks other
 * than stepping.
 * 
 * StepperSegmentBufferSize, EventChannelBufferSize
 * The maximum size of the "committed" portions of the buffers, in numbers of segments.
 * These need to be high enough to prevent unwanted underruns.
 * 
 * LookaheadBufferSize
 * The size of the planning buffer. To make the planner look N steps ahead, use
 * LookaheadBufferSize=N+1. A higher value means that the printer will take more previous
 * segments into account when computing the maximum speed at a segment junction.
 * The time complexity of planning increases linearly with this, so don't go too high.
 * If this is too high, you'll see unexpected buffer underruns, which manifest themselves
 * as the printer pausing for a moment and continuing shortly thereafter.
 * 
 * ForceTimeout
 * How much time we wait after a buffered g-code command is received before forcing the
 * planner to begin execution, in case the planner is waiting for the buffer to fill up.
 * This forcing mechanism exists so that the printer responds to user-generated commands
 * in a reasonable amount of time; it is not necessary for actual printing.
 * 
 * EventChannelTimer
 * The interrupt-timer used to implement auxiliary buffered commands, such as
 * set-heater-temperature and set-fan-speed.
 */

using LedBlinkInterval = AMBRO_WRAP_DOUBLE(0.5);
using DefaultInactiveTime = AMBRO_WRAP_DOUBLE(60.0);
using SpeedLimitMultiply = AMBRO_WRAP_DOUBLE(1.0 / 60.0);
using MaxStepsPerCycle = AMBRO_WRAP_DOUBLE(0.00137); // max stepping frequency relative to F_CPU
using ForceTimeout = AMBRO_WRAP_DOUBLE(0.1);
using TheAxisStepperPrecisionParams = AxisStepperAvrPrecisionParams;

/*
 * Explanation of axis-specific parameters.
 * 
 * Name
 * A character identifying the heater; must be upper-case. This is used, among other uses,
 * in the G1 (linear-move) command and in the M114 (get-position) command.
 * 
 * DirPin, StepPin, EnablePin, InvertDir
 * The pins connected to the stepper driver chip which controls this axis.
 * InvertDir defines the relation between the state of DirPin and the logical direction
 * of motion; when InvertDir=false, negative motion corresponds to a low state on DirPin.
 * 
 * StepsPerUnit
 * Defines the conversion ratio between the step position and the logical position,
 * in units of step/mm.
 * 
 * Min, Max
 * The permitted logical position range, in units of mm. If the axis supports homing,
 * when it is homed, the Min or Max position will be assumed, if HomeDir=false or HomeDir=true,
 * respectively. For example, if you use a min-endstop for the X axis, and after you home the
 * X axis, the nozzle is 50mm away from the left end of the bed, you can choose Min=-50 and
 * Max=BedWidth; that will result in the logical position 0 corresponding to the left end
 * of the bed.
 * 
 * MaxSpeed, MaxAccel
 * Maximum speed and maximum acceleration, in units of mm/s and mm/s^2. Note that
 * acceleration will exceed MaxAccel at segment junctions, as controlled by CorneringDistance.
 * 
 * DistanceFactor
 * The segment coordinate differences are multiplied with this before they are used in the
 * planner to compute the cartesian length of a segment. This effectively defines what
 * it means to preserve the speed at a segment junction. Note that this does not effect
 * the semantics of the euclidean speed limit (F parameter in g-codes).
 * 
 * CorneringDistance
 * This parameter affects the calculation of the maximum speed at segment junctions;
 * its semantic is similar to that of the "jerk" setting in some other firmwares.
 * Increase this to allow higher speeds at junctions. Note that the maximum acceleration
 * also affects this calculation, so this parameter should generally be adjusted
 * independently of maximum acceleration. The value is in units of steps; it should probably
 * be in the order of magnitude of the number of (micro)steps corresponding to a full step.
 * 
 * HomeEndPin, HomeEndInvert, HomeDir
 * HomeEndPin specifies the pin connected to the endstop switch. HomeEndInvert specifies how to
 * interpret the value on the pin; with HomeEndInvert=false, high value on HomeEndPin means
 * the switch is pressed. HomeDir specifies the location ofthe endstop switch; use
 * HomeDir=false for a min-endstop and HomeDir=true for a max-endstop.
 * 
 * HomeFastMaxDist, HomeRetractDist, HomeSlowMaxDist,
 * HomeFastSpeed, HomeRetractSpeed, HomeSlowSpeed
 * Homing is split into three portions: the Fast part, where the axis moves toward the endstop,
 * the Retract part, where the axis moves a bit in reverse, and the final Slow part, where
 * the axis again moves toward the endstop to obtain a final position.
 * The Dist parameters specify the maximum travel distance of the respective homing portions.
 * 
 * EnableCartesianSpeedLimit
 * If this is true, this axis will participate in the speed limit based on euclidean distance,
 * that is, the F parameter to g-code motion commands. You probably want
 * EnableCartesianSpeedLimit=true for X, Y and Z, and EnableCartesianSpeedLimit=false for E.
 * Speed limit for E-only motion is not implemented.
 * 
 * StepBits
 * The number of bits used to represent the absolute position of axis as a number of steps.
 * This limits the permitted logical position of the axis. Don't go beyond StepBits=32.
 * 
 * StepperTimer
 * The interrupt-timer to use for stepping this axis.
 */

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

/*
 * Explanation of heater-specific parameters.
 * 
 * Name
 * A character identifying the heater; this affects the output of the M105
 * g-code command (get-heater-temperature).
 * 
 * SetMCommand, WaitMCommand
 * The M-numbers for the set-heater-temperature and wait-heater-temperature g-code
 * commands. For the extruder heater, you want SetMCommand=104, WaitMCommand=109,
 * and for the bed heater, you want SetMCommand=140, WaitMCommand=190.
 * 
 * AdcPin
 * The pin where the temperature is read; this must be a pin supported by the
 * AD converter of your chip. You will also need to list this pin in AdcPins below,
 * so that the ADC driver will actually sample this pin.
 * 
 * OutputPin
 * The pin where the PWM signal is to be generated. This can be any pin,
 * since the PWM is performed in software.
 * 
 * Formula
 * The class which is used to convert ADC readings to temperatures.
 * Assuming you use a thermistor table generated by gen_avr_thermistor_table.py,
 * enter the name of the class in the resulting file (and be sure to #include that
 * file).
 * 
 * MinSafeTemp, MaxSafeTemp
 * The safe temerature range for the heater, in degrees Celsius. When the
 * temperature goes outside of this range, or if the heater is commanded to a
 * temperature outside of this range, the heater is turned off.
 * Note that the temperature range for the thermistor tables is defined
 * during table generation; if the temperature goes beyond the range supported
 * by the thermistor table, it is assumed to be outside of the safe range,
 * and the heater is turned off.
 * 
 * PulseInterval
 * The interval for the PWM signal to the heater. Don't make this too small,
 * as that will reduce the precision of integral computation. If you change
 * this for a heater which uses PID control, you will also want to change
 * PidDHistory exponentially proportionally (see below).
 * 
 * ControlInterval
 * The interval for control calculations. If this is non-zero, control calculations
 * will happen within the main loop, and the PWM interrupt will just pick up the
 * last computed control value. In this case, you probably want to use the same
 * value as PulseInterval, unless PulseInterval is too small.
 * If however ControlInterval is zero, then control calculations will happen as
 * part of the PWM interrupt, and the calculation interval will effectively be
 * PulseInterval.
 * WARNING: you may only use zero here when using BinaryControl or another
 * fast control algorithm. PidControl is too slow to work in interrupt context.
 * 
 * Control
 * The name of the template class which implements the control algorithm.
 * Possible choices are PidControl and BinaryControl.
 * 
 * PidP, PidI, PidD
 * The parameters for PID control of the heater.
 * PidP: the proportional term, in units of 1/K.
 * PidI: the integral term, in units of 1/(Ks).
 * PidD: the derivative term, in units of s/K.
 * Note that all three parameters are in natural units and are independent; that is,
 * the I and D parameters are not relative to P. There is no magic unit changes
 * or anything else happening - the heater power is calculated as:
 *   PidP * error_in_K + PidI * integral_error_in_Ks + PidD * derivative_error_in_K/s
 * and this is interpreted as a fraction which determines the relative pulse time,
 * 0 meaning the heater is off, and 1 meaning it fully powered.
 * 
 * PidIStateMin, PidIStateMax
 * These define the limits of the internal integral state, such that the
 * integral term will always be within this range. As such, these parameters
 * are in units of 1. Keep PidIStateMin non-negative.
 * 
 * PidDHistory
 * This is the "history factor" for derivative approximation.
 * The derivative approximation works like this:
 *   D_0 = 0
 *   D_i = PidDHistory * D_{i-1} + (1 - PidDHistory) * (T_i - T_{i-1}) / PulseInterval
 *     (for i > 0)
 * Hence, greater PidDHistory means greater inertia of derivative approximation.
 * Be aware that the natural semantic of PidDHistory depends on PulseInterval.
 * To approximately preserve the behavior of derivative approximation when changing
 * the PulseInterval as if by multiplication with 'a', raise PidDHistory to the power
 * of 'a'. Unfortunately we can't take care of that automatically due to the
 * difficulty of implementing a constexpr pow().
 * 
 * ObserverInterval, ObserverTolerance, ObserverMinTime
 * These parameters affect the behavior of wait-heater-temperature g-code commands.
 * Upon reception of such a command, the temerature will be observed in samples
 * ObserverInterval seconds apart and the command will complete once the temerature has
 * been within ObserverTolerance kelvins of the target temerature for at least
 * ObserverMinTime seconds.
 * 
 * TimerTemplate
 * The interrupt-timer to use for PWM generation.
 */

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

/*
 * Explanation of fan-specific parameters.
 * 
 * SetMCommand, OffMCommand
 * The M-numbers for the set-fan-speed and turn-off-fan g-code commands.
 * For a single fan, you want SetMCommand=106 and OffMCommand=107.
 * 
 * OutputPin
 * The pin where the PWM signal is to be generated. This can be any pin,
 * since the PWM is performed in software.
 * 
 * PulseInterval
 * The pulse interval, in seconds, for the PWM signal to the fan. Fell free to adjust
 * this to the value where the PWM noise from the fan annoys you the least,
 * but don't make it too small, since PWM is performed in software.
 * 
 * SpeedMultiply
 * This defines the semantic of the control value in the set-fan-speed
 * g-code command; the value in the command is multiplied by this, then
 * interpreted as relative pulse width.
 * 
 * TimerTemplate
 * The interrupt-timer to use for PWM generation.
 */

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
        AvrSerial,
        AvrSerialParams<true>
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
    AvrClockInterruptTimer_TC2_OCA, // EventChannelTimer
    AvrWatchdog,
    AvrWatchdogParams<
        WDTO_2S
    >,
    PrinterMainSdCardParams<
        SpiSdCard,
        SpiSdCardParams<
            AvrPin<AvrPortA, 0>, // SsPin
            AvrSpi
        >,
        FileGcodeParser, // BINARY: BinaryGcodeParser
        GcodeParserParams<8>, // BINARY: BinaryGcodeParserParams<8>
        2, // BufferBlocks
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
                AvrClockInterruptTimer_TC1_OCA, // StepperTimer
                TheAxisStepperPrecisionParams // PrecisionParams
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
                AvrClockInterruptTimer_TC1_OCB, // StepperTimer
                TheAxisStepperPrecisionParams // PrecisionParams
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
                AvrClockInterruptTimer_TC3_OCA, // StepperTimer
                TheAxisStepperPrecisionParams // PrecisionParams
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
            AxisStepperParams<
                AvrClockInterruptTimer_TC3_OCB, // StepperTimer
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
            AvrPin<AvrPortA, 7>, // AdcPin
            AvrPin<AvrPortD, 5>, // OutputPin
            false, // OutputInvert
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
            AvrClockInterruptTimer_TC0_OCA // TimerTemplate
        >,
        PrinterMainHeaterParams<
            'B', // Name
            140, // SetMCommand
            190, // WaitMCommand
            304, // SetConfigMCommand
            AvrPin<AvrPortA, 6>, // AdcPin
            AvrPin<AvrPortD, 4>, // OutputPin
            false, // OutputInvert
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
            AvrClockInterruptTimer_TC0_OCB // TimerTemplate
        >
    >,
    
    /*
     * Fans.
     */
    MakeTypeList<
        PrinterMainFanParams<
            106, // SetMCommand
            107, // OffMCommand
            AvrPin<AvrPortB, 4>, // OutputPin
            false, // OutputInvert
            FanPulseInterval, // PulseInterval
            FanSpeedMultiply, // SpeedMultiply
            AvrClockInterruptTimer_TC2_OCB // TimerTemplate
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
static const int clock_timer_prescaler = 3;

struct MyContext;
struct MyLoopExtraDelay;
struct Program;

using MyDebugObjectGroup = DebugObjectGroup<MyContext, Program>;
using MyClock = AvrClock<MyContext, Program, clock_timer_prescaler>;
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

AMBRO_AVR_CLOCK_ISRS(MyClock, MyContext())
AMBRO_AVR_ADC_ISRS(MyAdc, MyContext())
AMBRO_AVR_SERIAL_ISRS(MyPrinter::GetSerial, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC1_OCA_ISRS(MyPrinter::GetAxisTimer<0>, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC1_OCB_ISRS(MyPrinter::GetAxisTimer<1>, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC3_OCA_ISRS(MyPrinter::GetAxisTimer<2>, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC3_OCB_ISRS(MyPrinter::GetAxisTimer<3>, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC0_OCA_ISRS(MyPrinter::GetHeaterTimer<0>, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC0_OCB_ISRS(MyPrinter::GetHeaterTimer<1>, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC2_OCA_ISRS(MyPrinter::GetEventChannelTimer, MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC2_OCB_ISRS(MyPrinter::GetFanTimer<0>, MyContext())
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
    MyClock::initTC3(c);
    MyClock::initTC0(c);
    MyClock::initTC2(c);
    MyLoop::init(c);
    MyPins::init(c);
    MyAdc::init(c);
    MyPrinter::init(c);
    
    MyLoop::run(c);
}
