aprinter
========

APrinter is a currently experimantal firmware for RepRap 3D printers and is under heavy development.
It supports many controller boards based on AVR, Arduino Due (Atmel ARM) and Teensy 3 (Freescale ARM).

## Implemented features (possibly with bugs)

  * Highly configurable (at compile time) design. Extra heaters, fans and axes can be added easily, and
    PWM frequencies for heaters and fans are individually adjustable.
  * Delta robot support. Additionally, new geometries can be added easily by defining a transform class.
    Performance will be sub-optimal when using Delta on AVR platforms.
  * SD card printing (reading of sequential blocks only, no filesystem or partition support).
  * Optionally supports a custom packed g-code format for SD printing.
    This results in about 50% size reduction and 15% reduction in main loop processing load (on AVR).
  * Bed probing using a microswitch (prints results, no correction yet).
  * For use with multiple extruders, a g-code post-processor is provided to translate tool commands into
    motion of individual axes which the firmware understands. If you have a fan on each extruder, the post-processor can
    control the fans so that only the fan for the current extruder is on.
  * Constant-acceleration motion with look-ahead planning. To speed up calculations, the firmware will only
    calculate a new plan every LookaheadCommitCount commands. Effectively, this allows increasing
    the lookahead count without an asymptotic increase of processing time, only limited by the available RAM.
  * Homing using min- or max-endstops. Can home multiple at once.
    where only 115200 is supported due to unfavourable interrupt priorities.es in parallel.
  * Heater control using PID or on-off control. The thermistor tables need to be generated with a Python script.
    Each heater is configured with a Safe temperature range; aheater is turned off in case its temperature goes
    beyound the safe range.
  * Fan control (any number of fans).
  * Stepper control based on interrupts, with each stepper having its own timer interrupt.
    Step times are computed analytically using the quadratic equation, employing custom assembly
    routines for sqrt and division on AVR. This ensures that steps happen when they should,
    without one stepper ending a move faster than others due to accumulated rounding errors,
    and stopping while all steppers finish.
  * Portable and ported design; three different microcontroller families are already supported.
  * Non-drifting heartbeat LED. Its period is exactly 1 second, subject to the precision of your oscillator.

## Planned features (in the approximate order of priority):

  * Porting to more platforms (LPC, STM32).
  * Runtime configurability and settings in EEPROM.
  * SD card FAT32 support and write support.

## Hardware requirements

Ports have been completed for the following boards:

  * Melzi (atmega1284p only),
  * RAMPS 1.0, 1.1/1.2 or 1.3/1.4 (only RAMPS 1.4 with atmega2560 is tested),
  * RAMPS-FD, RADDS or other setup based on Arduino Due.
  * 4pi.
  * Teensy 3 (no standard board, needs manual wiring).

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

## Using Build system

The build system works as follows:

    ./build.sh TARGET [param] ACTION [param]

where `TARGET` is any of:

 * `melzi`, `ramps13`, `rampsfd`, `teensy`, `stm32f4` or `all`

and `ACTION` is any of:

 * `install`, `build`, `upload`, `clean`, `flush`

If you use the special target `all`, the actions will be applied to all the targets,
for example:

    ./build.sh all build

will compile all targets.

You can run one or several actions for each target, for example:

    ./build.sh melzi build upload

will compile the `melzi` target and upload it.

The `install` action will install the necessary framework and tools in the `depends`
directory, each download checked with a sha256 and not reinstalled if already present.
The `flush` action will delete all those install dependencies, be careful, no warning 
or confirmation is issued upon running that command.
The `clean` action will remove all build files for the given build in the
`build` directory.

Some dependencies can be overridden from environment variables or `config/config.sh`. In particular:

 * `CUSTOM_AVR_GCC` and `CUSTOM_ARM_GCC` override the AVR and ARM toolchains, respectively.
   This should be a prefix used to call the tools.
   For example, `${CUSTOM_AVR_GCC}gcc` is called during AVR compilation.
 * `AVRDUDE` overrides the `avrdude` tool, used for upload to AVR boards.

*Nota Bene*: 

 * NEVER RUN THIS SCRIPT AS ROOT!
 * Most of the build system will work only on Linux.
   On other platforms, custom installation of dependencies and hacking the scripts will be necessary.
 * The AVR toolchain is a [binary by Atmel](http://www.atmel.com/tools/atmelavrtoolchainforlinux.aspx).
 * The ARM toolchain is a binary by the [gcc-arm-embedded](https://launchpad.net/gcc-arm-embedded) project.
 * The `-v` parameter (between targets and actions) will show the commands ran by the script.
 * The script has only been tested when ran from source directory root.

## Extending the build system

The build targets are defined in the file: `config/targets.sh`. If you want to
add a new board with an existing architecture/platform, this is where it belongs.
By default, the target will use the source file `aprinter-TARGET.cpp`,
except if `SOURCE` is defined in the target, in which case it will use `aprinter-SOURCE.cpp`.

## Getting started

  * Find the target name you need to use.
    The basic targets are `melzi`, `ramps13`, `rampsfd`, `radds`, `teensy`, `4pi`.
    You can find more in `config/targets.sh`, including variants of those targets mentioned, as well as
    targets in development. You may have to adjust some variables in the target definition.
  * To install the toolchain and other dependnecies: `./build.sh <target> install`
  * Note that for AVR, you will still need `avrdude` preinstalled.
  * To build: `./build.sh <target> build`
  * To upload: `./build.sh <target> upload`

*Melzi.* you will have to set the Debug jumper and play with the reset button to get the upload going.

*Arduino Due.* Make sure you connect via the programming port while uploading.
But switch to the native USB port for actual printer communication.
Some Due clones have a problem resetting. If after uploading, the firmware does
not start (LED doesn't blink), press the reset button.

*RADDS.* On this board, pin 13 (Due's internal LED) is used for one of the FETs, and there is no LED on the RADDS itself. Due to this, the firmware defaults to pin 37 for the LED, where you can install one.

*Teensy 3.* You need to press the button on the board before trying to upload, to put the board into bootloader mode.

## Configuration

Most configuration is specified in the main file of the firmware. There is no support for EEPROM or other forms of runtime configuration (except for PID parameters, try M136). After chaning any configuration, you need to recompile and reflash the firmware.

For information about specific types of configuration, see the sections about SD cards and multiple extruders.

## Testing it

  * Connect to the printer with Pronterface, and make sure you use the right baud rate.
    Find the right one in your main file. For native USB serial (Teensy 3, Arduino Due, 4pi), the baud rate
    doesn't matter.
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

SD card support is working and enabled by default on all three supported boards (RAMPS1.4, Melzi, RAMPS-FD).

**WARNING.** If you have enabled SD card after it wasn't enabled in the your configuration,
you should [check your RAM usage](README.md#ram-usage).
If there isn't enough RAM available, the firmware will manfunction in unexpected ways, including causing physical damage (axes crashing, heaters starting fires).

Once you boot a firmware with SD support enabled, you will have the following commands available:

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

## Packed gcode

When printing from SD, the firmware can optionally read a custom packed form of gcode, to improve space and processing efficiency. The packing format [is documented](encoding.txt).

The choice between plain text and packed gcode needs to be made at compile time. By default, plain text is used.
To switched to reading packed gcode, adjust the SD card configuration as follows:

```
    PrinterMainSdCardParams<
        ...
        BinaryGcodeParser,
        BinaryGcodeParserParams<8>,
        2, // BufferBlocks
        43 // MaxCommandSize
    >,
```

To pack a gcode file, use the `aprinter_encode.py` script, as follows.

```
python2.7 /path/to/aprinter/aprinter_encode.py --input file.gcode --output file.packed
```

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

The default main files for RAMPS1.4 and RAMPS-FD already support two extruders.
If you want more axes, heaters or fans, specify them the same way that the existing ones are specified.
However, you will need to allocate a hardware *output compare unit* for each axis/heater/fan
that you wish to add. For more information, consult the [OC units section](README.md#output-compare-units).

**WARNING.** After you have enabled new axes or other features, you should [check your RAM usage](README.md#ram-usage).
If there isn't enough RAM available, the firmware will manfunction in unexpected ways, including causing physical damage (axes crashing, heaters starting fires).

**NOTE.** If you do something wrong when modifying your main file,
the result will most likely be an error message several megabytes long.
Yes, that's the price of doing the kind of metaprogramming used in this code.
Don't try to read the message, and instead focus on the code.
If you give up, [ask me for help](README.md#support).

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

- On atmega1284p (Melzi), there's 8 OC units available, named `AvrClockInterruptTimer_TC{0,1,2,3}_OC{A,B}`.
- On atmega2560 (RAMPS), there's 16 OC units available, named `AvrClockInterruptTimer_TC{{0,2}_OC{A,B},{1,3,4,5}_OC{A,B,C}}`.
- On AT91SAM3X8E (Due), there's 27 OC units available, named `At91Sam3xClockInterruptTimer_TC[0-8][A-C]`.

In addition to specifying the OC unit in the `TimerTemplate` parameter, a corresponding `ISRS` or `GLOBAL` macro needs the be invoked in order to set up the interrupt handler. For example, if `AvrClockInterruptTimer_TC4_OCA` is used
for the axis at index 3 (indices start from zero), the corresponding `ISRS` macro invocation is:

```
AMBRO_AVR_CLOCK_INTERRUPT_TIMER_TC4_OCA_ISRS(*p.myprinter.getAxisStepper<3>()->getTimer(), MyContext())
```

For heaters and fans, as wellas for Due as opposed to AVR, consult the existing assignments in your main file. 

For AVR based boards, if you have used an OC unit of a previously unused TC (e.g. you used `TC0_OCA`, and `TC0_OCB` was not already assigned), you will need to initialize this TC in the `main` function, by adding this line after the existing similar lines:

```
    p.myclock.initTC0(c);
```

On AT91SAM3XE (Due), something similar is generally needed,
but since all the TCs are already used in the default configuration, you don't need to do anything.

**WARNING.** On AVR, it is imperative that the interrupt priorities of the OC ISRs are considered.
Basically, you should avoid using TCs with higher priority than the USART RX ISR,
or risk received bytes being randomly dropped, causing print failure when printing from USB.
If that is not possible, at least use those TCs for simple tasks such as fan control, and perhaps heater control.
On atmega2560, those higher-priority TCs are `TC0` and `TC2`.

**NOTE.** On boards based on atmega1284p, all available output compare units are already assined.
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

## Support

If you need help or want to ask me a question, you can find me on Freenode IRC in #reprap (nick ambro718), or you can email me to ambrop7 at gmail dot com. If you have found a bug or have a feature request, you can use the issue tracker.
