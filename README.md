aprinter
========

APrinter is a currently experimantal firmware for RepRap 3D printers and is under heavy development.
It supports many controller boards based on AVR, as well as Arduino Due.

## Implemented features (possibly with bugs)

  * SD card printing (reading of sequential blocks only, no filesystem or partition support).
  * Serial communication using the defacto RepRap protocol. Maximum baud rate on AVR is 115200.
  * Homing, including homing of multiple axes at the same time. Either min- or max- endstops can be used.
  * Line motion with acceleration control and cartesian speed limit (F parameter).
    Speed limit in case of E-only motion is not implemented.
    The speed is automatically limited to not overload the MCU with interrupts.
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

## Building it (AVR)

  * Make sure you have avr-g++ 4.8.1 or newer.
    Since it's not that easy to build or get that, I provide a
    (hopefully) portable build for Linux: https://docs.google.com/file/d/0Bx9devQE0OqbWTh4VUFvOWhlNEU/edit?usp=sharing
    Sorry about the giant file size, there were some problems with stripping the binaries.
    To use the toolchain, extract it somewhere and modify your PATH as follows:
    export PATH=/path/to/toolchain/bin:$PATH
  * Edit compile.sh and adjust MCU, F_CPU and MAIN to reflect your board.
    If your compiler is not available as avr-g++, adjust CROSS appropriately.
  * Open aprinter/printer/aprinter-YourBoard.cpp and adjust the configuration.
    If you don't know what something means, you probably don't need to change it.
    All units are based on millimeters and seconds.
    NOTE: documentation of configuration parameters is present in aprinter-melzi.cpp only.
  * Regenerate the thermistor tables inside the generated/ folder to match your thermistor and resistor types.
    You can find the regeneration command inside the files themselves.
    The python script mentioned prints the code to stdout, you need to pipe it into the appropriate file.
  * Run compile.sh to compile the code.
  * Upload the code to your MCU, however you normally do that; see flash.sh for an example.
    If you're only used to uploading with the Arduino IDE, you can enable the verbose upload option in its preferences,
    and it will print out the avrdude command when you try to upload a sketch.

## Building it (Due)

  * Obtain a gcc toolchain for ARM Cortex M3, including a C++ compiler.
    Download the [gcc-arm-embedded](https://launchpad.net/gcc-arm-embedded) toolchain.
    Version 4_7-2013q3-20130916-linux has been tested.
    Alternatively, if you're on Gentoo, you can easily build the toolchain yourself:
    `USE="-fortran -openmp" crossdev -s4  --genv 'EXTRA_ECONF="--disable-libstdcxx-time"' armv7m-softfloat-eabi`
  * Download the [Atmel Software Framework](http://www.atmel.com/tools/AVRSOFTWAREFRAMEWORK.aspx).
    Version 3.12.1 has been tested.
  * Edit `compile-rampsfd.sh` and adjust `CROSS` and `ASF_DIR` appropriately.
  * Run `compile-rampsfd.sh` to build the firmware.
  * Download Arduino 1.5 then edit `flash-rampsfd.sh` to point it to the location of the `bossac` program.
  * Connect your Due board via the programming port.
  * Press the erase button, then the reset button, and finally run `flash-rampsfd.sh` to upload the firmware to your board.

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

The firmware supports reading G-code from an SD card. However, the G-code needs to be written directly to the SD card in sequential blocks, starting with the first block.

**NOTE**: SD card is not implemented on Due yet.

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

**Additionally**, you may need to reduce your buffer sizes to accomodate the RAM requirements of the SD code.
To check, compile the firmware, then run `avr-size aprinter.elf`.
The `bss` number tells you how much data memory is used.
Also consider that there should be about 900 bytes left for the stack.
Therefore, on atmega2560, which has 8KiB of SRAM, your `bss` should not be higher than 7300.
You can reduce your RAM usage by lowering `StepperSegmentBufferSize` and `EventChannelBufferSize` (preferably keeping them equal),
as well as lowering `LookaheadBufferSize`.

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

As you can see, each axis, heater and fan requires the assignment of a Timer/Counter (TC) output compare (OC) unit.
The OC unit is used to step an axis, or generate a PWM signal for a heater or fan.
In the default configuration for Ramps, both of the two output compare units (OCA and OCB) on TC1, TC3, TC4 and TC5 are already assigned.
Here, we used TC0/OCA and TC0/OCB for the new axis and heater, and TC2/OCA for the new fan.

**NOTE:** on boards based on atmega1284p, all available output compare units are already assined.
As such, it is not possible to add any extra axes/heaters/fans.
However, it would be easy to implement multiplexing so that one hardware OC units would act as two or more virtual OC units,
at the cost of some overhead in the ISRs.

**NOTE:** keep track of your RAM usage. See the SD card section to check how to check and reduce RAM usage.

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
