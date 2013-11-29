aprinter
========

APrinter is a currently experimantal firmware for RepRap 3D printers and is under heavy development.
It supports many controller boards based on AVR, as well as Arduino Due.

## Implemented features (possibly with bugs)

  * SD card printing (reading of sequential blocks only, no filesystem or partition support).
  * Serial communication using the defacto RepRap protocol. Maximum baud rate on AVR is 115200.
  * Homing, including homing of multiple axes at the same time. Either min- or max- endstops can be used.
    Endstops are only used during homing and not for detecting collisions.
  * Line motion with acceleration control and speed limit (F parameter to G0/G1).
    The speed is automatically limited to not overload the MCU with interrupts.
    However the semantic of F differs somehow from other firmwares; the speed limit is interpreted as
    euclidean if G1/G0 specifies at least one of X/Y/Z, and as extruder speed limit otherwise.
  * Look-ahead to preserve some speed on corners. By default, on AVR, planning takes the previous 3 moves into account.
    This can be increased in the configuration, but this
    also increases the chance of buffer underruns , which cause the print to temporarily pause while the buffer refills.
    Look-ahead is very memory-hungry in its current state.
  * Heater control using PID or on-off control. The thermistor tables need to be generated with a Python script.
  * Safe temperature range. A heater is turned off in case its temperature goes beyound the safe range.
  * Fan control.
  * Starting and feeding of the watchdog timer.
  * Emergency shutdown of motors and heaters in case of an assertion failure
    (if assertions are enabled with -DAMBROLIB_ASSERTIONS).
  * Non-drifting heartbeat LED. Its period is exactly 1 second, subject to the precision of your oscillator.

## Planned features (in the approximate order of priority):

  * Further optimization of the planning and stepping code, both for speed and memory usage.
  * Runtime configurability and settings in EEPROM.
  * SD card FAT32 support and write support.

## Hardware requirements

Ports have been completed for the following boards:

  * Melzi (atmega1284p only),
  * RAMPS 1.0, 1.1/1.2 or 1.3/1.4 (only RAMPS 1.4 with atmega2560 is tested),
  * RAMPS-FD or other setup based on Arduino Due.

However, any AVR satisfying the following should work, possibly requiring minor adjustments in the code:

  * 8kB of SRAM,
  * 128kB of flash,
  * 4 timers (any one of these can be either 8- or 16-bit).

## Coding style

  * Extreme attention to detail and bugless code. Lots of assertions (the proven kind, not guesses).
  * The software is written in C++11.
  * Extensive use of abstractions for hardware access and program structuring.
    It should be possible to port the software on new platforms with little work.
    In fact, the firmware already works on Arduino Due, which is based on an Atmel ARM microcontroller.
  * Template metaprogramming is used to implement the abstractions efficiently.
    No, I do not care if the average C hacker can't read my code.
  * Hardcoding is avoided where that makes sense, with the help of template metaprogramming.
    For example, the configuration specifies a list of heaters, and it is trivial to add new heaters.
  * No reliance on inefficient libraries such as Arduino.

## Building (AVR)

  * Make sure you have avr-g++ 4.8.1 or newer.
    Since it's not that easy to build or get that, I provide a
    (hopefully) portable build for Linux: https://docs.google.com/file/d/0Bx9devQE0OqbWTh4VUFvOWhlNEU/edit?usp=sharing
    Sorry about the giant file size, there were some problems with stripping the binaries.
    To use the toolchain, extract it somewhere and modify your PATH as follows:
    `export PATH=/path/to/toolchain/bin:$PATH`
  * Find or create a main file for your board. The main files for supported boards can be
    found at `aprinter/printer/aprinter-BoardName.cpp`.
  * Find or create a compile script for your board, named `compile-BoardName.sh`.
  * Also find or create your flash script, `flash-BoardName.sh`, and set the correct the serial port.
  * Run your compile script to compile the code. This needs to be run from within the source directory.
  * Run your flash script to upload the code to your MCU.
    If you're using Melzi, you will have to set the Debug jumper and play with the reset button
    to get the upload going.

## Building (Due)

  * Obtain a gcc toolchain for ARM Cortex M3, including a C++ compiler.
    Download the [gcc-arm-embedded](https://launchpad.net/gcc-arm-embedded) toolchain.
    Version 4_7-2013q3-20130916-linux has been tested.
    Alternatively, if you're on Gentoo, you can easily build the toolchain yourself:
    `USE="-fortran -openmp" crossdev -s4  --genv 'EXTRA_ECONF="--disable-libstdcxx-time"' armv7m-softfloat-eabi`
  * Download the [Atmel Software Framework](http://www.atmel.com/tools/AVRSOFTWAREFRAMEWORK.aspx).
    Version 3.12.1 has been tested.
  * Edit `compile-rampsfd.sh` and adjust `CROSS` and `ASF_DIR` appropriately.
  * Run `compile-rampsfd.sh` to build the firmware. This needs to be run from within the source directory.
  * Download Arduino 1.5 in order to get the `bossac` program. Note that the vanilla `bossac` will not work.
  * Edit `flash-rampsfd.sh` to set the location of the `bossac` program and the serial port corresponding
    to the programming port of the Due.
  * With the board connected using the programming port, press the erase button, then the reset button, and finally run `flash-rampsfd.sh` to upload the firmware to your board.

## Configuration

Most configuration is specified in the main file of the firmware. There is no support for EEPROM or other forms of runtime configuration (except for PID parameters, try M136). After chaning any configuration, you need to recompile and reflash the firmware.

However, thermistors are configured by generating the appropriate thermistor tables, as opposed to in the main file.
To configure thermistors, regenerate the thermistor tables in the generated/ folder using the Python script
`gen_avr_thermistor_table.py`. The generated thermistor table files include the command that can be used to regenerate them, so you can use this as a starting point.

For information about specific types of configuration, see the sections about SD cards and multiple extruders.

## Testing it

  * Connect to the printer with Pronterface, and make sure you use baud rate 115200 (for AVR) or 250000 (for Due).
  * Try homing and some basic motion.
  * Check the current temperatures (M105).
  * Only try turning on the heaters once you've verified that the temperatures are being reported correctly.
    Be aware that if you generate a thermistor table with the wrong beta value,
    the room teperature will be reported correctly, but other temperatures will be incorrect
    (possibly lower, and you risk burning the heater in that case).
    Obviously, take safety precausions here. I'm not responsible if your house burns down as a result of
    using my software, or for any other damage.

## SD card support

The firmware supports reading G-code from an SD card. However, the G-code needs to be written directly to the SD card in sequential blocks, starting with the first block (where the partition table would normally reside).

On Due/RAMPS-FD, SD card support is enabled by default.
On the other hand, to enable SD card for AVR based boards, some changes need to be done in your main file:
```
...
// add these includes
#include <aprinter/devices/SpiSdCard.h>
#include <aprinter/system/AvrSpi.h>
...
    // remove "PrinterMainNoSdCardParams," and replace it with this (don't forget the trailing comma):
    // The SsPin below is for RAMPS1.4. For Melzi, use AvrPin<AvrPortB, 4>.
    PrinterMainSdCardParams<
        SpiSdCard,
        SpiSdCardParams<
            AvrPin<AvrPortB, 0>, // SsPin
            AvrSpi
        >,
        GcodeParserParams<8>,
        2, // BufferBlocks
        100 // MaxCommandSize
    >,
...
// add this to where the other ISR macros are
AMBRO_AVR_SPI_ISRS(*p.myprinter.getSdCard()->getSpi(), MyContext())
...
```

**WARNING.** After you have added the new features, you should [check your RAM usage](README.md#ram-usage).
If there isn't enough RAM available, the firmware will manfunction in unexpected ways, including causing physical damage (axes crashing, heaters starting fires).

Once you've flashed firmware wirh these changes, you will have the following commands available:

- M21 - Initializes the SD card.
- M22 - Deinitializes the SD card.
- M24 - Starts/resumes SD printing. If this is after M21, g-code is read from the first block, otherwise from where it was paused.
- M25 - Pauses SD printing.

To print from SD card, you need to:

- Add an `EOF` line to the end of the g-code. This will allow the printer to know where the g-code ends.
- Make sure there aren't any excessively long lines in the g-code. In particular, Cura's `CURA_PROFILE_STRING` is a problem.
  You can fix that by putting the `EOF` in the "end gcode", so it ends up before the `CURA_PROFILE_STRING`.
- Write the prepared g-code to the SD card device node (not partition). Obviously, this will destroy any existing data on your card.
- Insert the card into the printer, then issue M21 to initialize the card, and M24 to begin printing.

The `DeTool.py` postprocessor can also prepare the g-code for SD card printing; more about this in the next section.

## Multi-extruder configuration

While the firmware allows any number of axes, heaters and fans, it does not, by design, implement tool change commands.
All axes, heaters and fans are independent, and the firmware does not know anything about their physical association:

- Extra extruder axes are independently controlled through their identification letter. Recommened letters for extra extruders are U and V.
- Extra heaters also need an identification letter, which is sent when the printer reports temperatures to the host.
  Additionally, each heater needs three M-codes, for `set-heater-temperature`, `wait-heater-temperature` and `set-config`.
  The recommended choices for the second extruder heater are U,M404,M409,M402, and for the third extruder heater V,M414,M419,M412, respectively.
- Extra fans need their `set-fan-speed` and `turn-fan-off` M-codes.
  The recommended choices for the second extruder fan are M406,M407, and for the third extruder fan M416,M417, respectively.

The included `DeTool.py` script can be used to convert tool-using g-code to a format which the firmware understands, but more about that will be explained later.
First, you need to configure you firmware for multiple extruders in the first place.

Open your main file and add the new axes, heaters and fans. In particular, if you're adding a second extruder to a Ramps1.3/1.4 board, use the following:

```
...
// Add these floating point constants for the second extuder axis,
// after the section for the first extruder (EDefault...).
using UDefaultStepsPerUnit = AMBRO_WRAP_DOUBLE(928.0);
using UDefaultMin = AMBRO_WRAP_DOUBLE(-40000.0);
using UDefaultMax = AMBRO_WRAP_DOUBLE(40000.0);
using UDefaultMaxSpeed = AMBRO_WRAP_DOUBLE(45.0);
using UDefaultMaxAccel = AMBRO_WRAP_DOUBLE(250.0);
using UDefaultDistanceFactor = AMBRO_WRAP_DOUBLE(1.0);
using UDefaultCorneringDistance = AMBRO_WRAP_DOUBLE(32.0);
...
        // Add this to the list of axes, and don't forget a comma above.
        PrinterMainAxisParams<
            'U', // Name
            MegaPin34, // DirPin
            MegaPin36, // StepPin
            MegaPin30, // EnablePin
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
                AvrClockInterruptTimer_TC0_OCB // StepperTimer
            >
        >
...
        // Add this to the list of heaters, and don't forget a comma above.
        PrinterMainHeaterParams<
            'U', // Name
            404, // SetMCommand
            409, // WaitMCommand
            402, // SetConfigMCommand
            MegaPinA15, // AdcPin
            MegaPin9, // OutputPin
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
            AvrClockInterruptTimer_TC0_OCA // TimerTemplate
        >
...
        // If you have a separate fan for the second extruder, add this to the list of fans.
        // Again, don't forget a comma above.
        PrinterMainFanParams<
            406, // SetMCommand
            407, // OffMCommand
            MegaPin5, // OutputPin
            FanPulseInterval, // PulseInterval
            FanSpeedMultiply, // SpeedMultiply
            AvrClockInterruptTimer_TC2_OCA // TimerTemplate
        >
...
// Add this to the list of ISRs.
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC0_OCB_ISRS(*p.myprinter.getAxisStepper<4>()->getTimer(), MyContext())
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC0_OCA_ISRS(*p.myprinter.getHeaterTimer<2>(), MyContext())
// If you added a fan above, also this:
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC2_OCA_ISRS(*p.myprinter.getFanTimer<1>(), MyContext())
...
    // add this to main where the extra TC's are inited
    p.myclock.initTC0(c);
    p.myclock.initTC2(c); // if you added the fan
...
```

As you can see, each axis, heater and fan requires the assignment of a Timer/Counter (TC) output compare (OC) unit. For more information, consult the [OC units section](README.md#output-compare-units).

**WARNING.** After you have enabled SD card support, you should [check your RAM usage](README.md#ram-usage).
If there isn't enough RAM available, the firmware will manfunction in unexpected ways, including causing physical damage (axes crashing, heaters starting fires).

## The DeTool g-code postprocessor

The `DeTool.py` script can either be called from command line, or used as a plugin from `Cura`.
In the latter case, you can install it by copying (or linking) it into `Cura/plugins` in the Cura installation folder.

To run the script, you will need to provide it with a list of physical extruders, which includes the names of their axes,
as understood by your firmware, as well as the offset added to the coordinates.
Futher, you will need to define a mapping from tool indices to physical extruders.
The command line syntax of the script is as follows.

```
usage: DeTool.py [-h] --input InputFile --output OutputFile
                 --tool-travel-speed Speedmm/s --physical AxisName OffsetX
                 OffsetY OffsetZ --tool ToolIndex PhysicalIndexFrom0
                 [--fan FanSpeedCmd PhysicalIndexFrom0 SpeedMultiplier]
                 [--sdcard]
```

For example, if you have two extruder axes, E and U, the U nozzle being offset 10mm to the right, and you want to map the T0 tool to U, and T1 to E,
you can pass this to the script:

```
--physical E 0.0 0.0 0.0 --physical U -10.0 0.0 0.0 --tool 0 1 --tool 1 0
```

The argument `--tool 0 1` means to associate T0 to the second physical extruder (with the letter E, in this case).\
Note that the offset in `--physical` is what the script adds to the position in the source file, that is, the negative of the nozzle offset.

The script also processes fan control commands (M106 and M107) to appropriate fans based on the selected tool.
For example, if your first fan (M106) blows under the first extruder, and the second fan (M406) under the second extruder,
you will want to pass:

```
--fan M106 0 1.0 --fan M406 1 1.0
```

If the fans are not equally powerful, you can adjust the `SpeedMultiplier` to scale the speed of specific fans.

## Output compare units

Various features of the firmware, in particular, each axis, heater and fan,
require the allocation of a timer output compare unit of the microcontroller.
The output compare units are are internally used to raise interrupts at programmed times, which, for example,
send step signals to a stepper motor driver, or toggle a pin to perform software PWM control of a heater or fan.

The firmware is designed in a way that makes it very easy to assign OC units to features that require them.
Each feature has a parameter in its configuration expression, called `TimerTemplate`,
which specifies the OC unit to be used. The available OC units depend on the particular microcontroller.
For example:

- On atmega1284p (Melzi), there's 8 OC units available, named `AvrClockInterruptTimer_TC[0-3]_OC[A-B]`.
- On atmega2560 (RAMPS), there's 12 OC units available, named `AvrClockInterruptTimer_TC[0-5]_OC[A-B]`.
- On AT91SAM3X8E (Due), there's 27 OC units available, named `At91Sam3xClockInterruptTimer_TC[0-8][A-C]`.

In addition to specifying the OC unit in the `TimerTemplate` parameter, a corresponding `ISRS` or `GLOBAL` macro needs the be invoked in order to set up the interrupt handler. For example, if `AvrClockInterruptTimer_TC3_OCB` is used
for the axis at index 3 (indices start from zero), the corresponding `ISRS` macro invocation is:

```
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC3_OCB_ISRS(*p.myprinter.getAxisStepper<3>()->getTimer(), MyContext())
```

For heaters and fans, as wellas for Due as opposed to AVR, consult the existing assignments in your main file. 

For AVR based boards, if you have used an OC unit of a previously unused TC (e.g. you used `TC0_OCA`, and `TC0_OCB` was not already assigned), you will need to initialize this TC in the `main` function, by adding this line after the existing similar lines:

```
    p.myclock.initTC0(c);
```

On AT91SAM3XE (Due), something similar is generally needed,
but since all the TCs are already used in the default configuration, you don't need to do anything.

**NOTE:** on boards based on atmega1284p, all available output compare units are already assined.
As such, it is not possible to add any extra axes/heaters/fans.
However, it would be easy to implement multiplexing so that one hardware OC units would act as two or more virtual OC units,
at the cost of some overhead in the ISRs.

## RAM usage

If you add new functionality to your configuration,
it is a good idea to check if your microcontroller still has enough RAM.
This is mostly a problem on atmega2560, with only 8KiB RAM; the atmega1284p has 16KiB, and AT91SAM3X8E has 96KiB.
In any case, the RAM usage can be checked using the `size` program of your cross compilation toolchain. For example:

```
$ avr-size aprinter.elf 
   text	   data	    bss	    dec	    hex	filename
 108890	     10	   7285	 116185	  1c5d9	aprinter.elf
```

The number we're interested in here is the sum of `data` and `bss`. You should also reserve around 900 bytes for the stack. So, on atmega2560, your data+bss shoudln't exceed 7300 bytes. If you fall short, you can reduce your RAM usage by lowering `StepperSegmentBufferSize` and `EventChannelBufferSize` (preferably keeping them equal),
as well as lowering `LookaheadBufferSize`. Alternatively, port your printer to Due/RAMPS-FD ;)
