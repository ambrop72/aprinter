aprinter
========

APrinter is a currently experimantal firmware for RepRap 3D printers and is under heavy development.
It supports many controller boards based on AVR, Arduino Due (Atmel ARM) and Teensy 3 (Freescale ARM).

## Implemented features (possibly with bugs)

  * Highly configurable design. Extra heaters, fans and axes can be added easily, and
    PWM frequencies for heaters and fans are individually adjustable unless hardware PWM is used.
  * Runtime configuration system, and configuration storgte to EEPROM.
    Availability depends on chip/board.
  * Delta robot support. Additionally, new geometries can be added easily by defining a transform class.
    Performance will be sub-optimal when using Delta on AVR platforms.
  * SD card printing (reading of sequential blocks only, no filesystem or partition support).
  * Optionally supports a custom packed g-code format for SD printing.
    This results in about 50% size reduction and 15% reduction in main loop processing load (on AVR).
  * Bed probing using a microswitch (prints results, no correction yet).
  * Slave steppers, driven synchronously, can be configured for an axis (e.g. two Z motors driven by separate drivers).
  * For use with multiple extruders, a g-code post-processor is provided to translate tool commands into
    motion of individual axes which the firmware understands. If you have a fan on each extruder, the post-processor can
    control the fans so that only the fan for the current extruder is on.
  * Experimental support for lasers (PWM output with a duty cycle proportional to the current speed).
  * Constant-acceleration motion with look-ahead planning. To speed up calculations, the firmware will only
    calculate a new plan every LookaheadCommitCount commands. Effectively, this allows increasing
    the lookahead count without an asymptotic increase of processing time, only limited by the available RAM.
  * Homing using min- or max-endstops. Can home multiple axes in parallel.
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
  * The software is written in C++11, with some use of G++ specific features such as constexpr math.
  * Extensive use of abstractions for hardware access and program structuring.
    A dependency injection pattern is used to decouple many components from their dependencies
    where appropriate.  Assembly is performed in the main file.
  * Template metaprogramming is used to implement the abstractions efficiently.
    No, I do not care if the average C hacker can't read my code.
  * Hardcoding is avoided where that makes sense, with the help of template metaprogramming.
    For example, the configuration specifies a list of heaters, and it is trivial to add new heaters.
  * No reliance on inefficient libraries such as Arduino.

## Getting started

The basic steps are:

  * Find the target name you need to use.
    The basic supported targets are `melzi`, `ramps13`, `rampsfd`, `radds`, `teensy`, `4pi`.
    You can find more in `config/targets.sh`, including variants of those targets mentioned, as well as
    targets in development. You may have to adjust some variables in the target definition.
  * Locate the main source file corresponding to the target, which is `main/aprinter-SOURCE.cpp`.
    Here, `SOURCE` defaults to the target name if it is not defined in the target definition.
  * Examine the main file and adapt it to your liking.
  * If you want to build using Nix, skip these remaining steps and see the Nix section. In this case, you are on your own with uploading the firmware.
  * To install the toolchain and other dependnecies: `./build.sh <target> install`
  * Note that for AVR, you will still need `avrdude` preinstalled.
  * To build: `./build.sh <target> build`
  * To upload (via the build system, see below if using Nix): `./build.sh <target> upload`

*Melzi.* you will have to set the Debug jumper and play with the reset button to get the upload going.

*Arduino Due.* Make sure you connect via the programming port while uploading.
But switch to the native USB port for actual printer communication.
If you want to use the programming port for communication, see below (this is needed for Udoo).
Some Due clones have a problem resetting. If after uploading, the firmware does
not start (LED doesn't blink), press the reset button.

*RADDS.* On this board, pin 13 (Due's internal LED) is used for one of the FETs, and there is no LED on the RADDS itself. Due to this, the firmware defaults to pin 37 for the LED, where you can install one.

*Teensy 3.* You need to press the button on the board before trying to upload, to put the board into bootloader mode.

*Using the UART Serial.* On Atmel chips, the default is to use the native USB for communication. But it's possible to use the UART instead. Edit `config/targets.sh` and change `USE_USB_SERIAL` to 0 for your target. Alternatively, if you're compiling with Nix, edit `nix/default.nix` like so: `aprinterTestRadds = (aprinterTestFunc "radds" {}).override { forceUartSerial = true; };`.

## Uploading

*RAMPS.* `avrdude -p atmega2560 -P /dev/ttyACM0 -b 115200 -c stk500v2 -D -U "flash:w:$HOME/aprinter-build/aprinter-nixbuild.hex:i"`

## Configuration

Most configuration is specified in the main file of the firmware. After chaning any configuration in the main file, you need to recompile and reflash the firmware for the changes to take affect.

Some degree of runtime configurability is implemented. This is in the form of named configuration values, as defined in the main file.

Boards where runtime configuration is enabled: RADDS, RAMPS-FD, RAMPS, Melzi, 4pi, Teensy 3.

Board-specific notes:

  * RAMPS-FD: I2C EEPROM is used for storage, which is not present on old boards.
  * RAMPS: Due to lack of RAM, some configuration options are not configurable at runtime.
  * 4pi: The internal flash is used for storage, which gets erased every time the device is programmed.

Runtime configuration commands:

  * Print current configuration: `M924`.
  * Get option value: `M925 I<option>`
    Example: `M925 IXMin`
  * Set option value: `M926 I<option> V<value>`
    Example: `M926 IXMin V-20`
    For boolean option, the values are 0 and 1.
    Omit the V argument to set the option to the default value.
  * Set all options to defaults: `M927`
  * Load configuration from storage: `M928`
  * Save configuration to storage: `M929`
  * Apply configuration: `M930`

After changing any configuration (either directly with `M926` or by loading an entire configuration with `M928`), the configuration needs to be applied with `M930`. Only then will the changes take effect. However, when the firmware starts up, the stored configuration is automatically loaded and applied, as if `M928` followed by `M930` was done.

The `M930` command does not alter the current set of configuration values in any way. Rather, it recomputes a set of values in RAM which are derived from the configuration values. This is a one-way operation, there is no way to see what the current applied configuration is.

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

## Building with Nix

It is possible to build the firmware using the [Nix package manager](http://nixos.org/nix/).
In this case, all you need is Nix running on Linux (either NixOS, or Nix installed on another Linux distro).
Nix will take care of any build dependencies.

To build with Nix, run this command from the directory below the `aprinter` source code directory:

```
nix-build aprinter/nix -A <aprinterTarget> -o ~/aprinter-build
```

You need to pick the right `<aprinterTarget>` for you. Consult the file `nix/default.nix` for a list of targets.
Generally, each target uses its own main source file (in the `main` subdirectory), coresponding to the name of the target.

The result of the build will be available in the directory symlink specified using `-o`.

The special target `aprinterTestAll` will build all supported targets, which is useful for development.

*NOTE*: Don't put the output (`-o`) within the source directory. If you do that the build output will be considered part of the source for the next build, and copying it will take a long time.

## Notes on the build systemm

The build system is invoked as follows:

    ./build.sh TARGET [param] ACTION...

where `TARGET` is one of the targets defined in `config/targets.sh` and `ACTION` is any of:

 * `install` - Download and build dependencies, into the `depends` subfolder.
 * `build` - Build the firmware.
 * `upload` - Upload the firmware to the microcontroller.

You can run one or several actions for each target, for example, to build then immediately upload:

    ./build.sh <target> build upload

Some dependencies can be overridden from environment variables or `config/config.sh`. In particular:

 * `CUSTOM_AVR_GCC` and `CUSTOM_ARM_GCC` override the AVR and ARM toolchains, respectively.
   This should be a prefix used to call the tools.
   For example, `${CUSTOM_AVR_GCC}gcc` is called during AVR compilation.
 * `AVRDUDE` overrides the `avrdude` tool, used for upload to AVR boards.

Other notes:

 * NEVER RUN THIS SCRIPT AS ROOT!
 * On Mac OS X `install` action may require `brew install wxmac` before.
 * Most of the build system will work only on Linux.
   On other platforms, custom installation of dependencies and hacking the scripts will be necessary.
 * The AVR toolchain is a [binary by Atmel](http://www.atmel.com/tools/atmelavrtoolchainforlinux.aspx).
 * The ARM toolchain is a binary by the [gcc-arm-embedded](https://launchpad.net/gcc-arm-embedded) project.
 * The `-v` parameter (between targets and actions) will show the commands ran by the script.
 * The script will only work when ran from source directory root.

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

## Delta geomoetry

For delta, consult the `aprinter-teensy3.cpp` main file as an example. Briefly, you need to do the following:

- Add the cartesian axis configuration options at the top of the file (`XMinPos`...`ZMaxSpeed`).
- Replace the existing X,Y,Z axis configuration options with a common set of options for A,B,C tower axes (`ABCInvertDir`...`ABCHomeSlowSpeed`).
- Add the delta transform configuration options (`DeltaDiagonalRod`...`DeltaMaxSplitLength`).
- Change your `PrinterMainAxisParams` X,Y,Z axis configuration to A,B,C. This includes the axis name and all the configuration option names. Make sure to set `EnableCartesianSpeedLimit` to false.
- Add the transform configuration, by replacing `PrinterMainNoTransformParams` with what is in that place in the example.

*NOTE*: Delta will not work well on AVR based platforms due to lack of CPU speed and RAM.

## Slave steppers

Slave steppers are extra steppers assigned to an axis. They will be driven synchronously with the main stepper for the axis.
Actually, the only difference between the main stepper and slave steppers is the way they are specified in the configuration.

To add one or more slave steppers to an axis, specify them in the `PrinterMainAxisParams` after the microstep configuration, as follows.

```
// Add this to the config definition section somewhere.
APRINTER_CONFIG_OPTION_BOOL(XSlave1InvertDir, false, ConfigNoProperties)

PrinterMainAxisParams<
    ...
    PrinterMainNoMicroStepParams, // Don't forget the comma.
    MakeTypeList<
        PrinterMainSlaveStepperParams<
            DuePinA9, // DirPin
            DuePinA10, // StepPin
            DuePinA11, // EnablePin
            XSlave1InvertDir // InvertDir
        >
        // Add more if you need. Don't forget a comma above.
    >
>
```

## Laser support

There is currently experimental support for lasers, more precisely,
for a PWM output whose duty cycle is proportional to the current speed.
The laser configuration parameters are:

- `LaserPower` [W]: The actual power of the laser at full duty cycle.
  You don't strictly have to measure the power, this just serves as a
  reference for everything else, you can use e.g. 1 or 100.
- `MaxPower` [W]: An artificial limit of laser power. Use =`LaserPower` to
  allow the laser to run at full duty cycle. But make sure it's not
  greater than `LaserPower`, that will result in strange behavior.
- `DutyAdjustmentInterval` [s]: Time interval for duty cycle adjustment
  from a timer interrupt (=1/frequency).

A prerequisite for configuring a laser is the availability of hardware PWM and its support by the firmware. See the section "Hardware PWM configuration" for more details.

A laser is configured in the main file as follows:

```
// Add to includes:
#include <aprinter/driver/LaserDriver.h>
#include <aprinter/printer/duty_formula/LinearDutyFormula.h>
...
// Add to configuration options:
APRINTER_CONFIG_OPTION_DOUBLE(LLaserPower, 100.0, ConfigNoProperties)
APRINTER_CONFIG_OPTION_DOUBLE(LMaxPower, 100.0, ConfigNoProperties)
using LDutyAdjustmentInterval = AMBRO_WRAP_DOUBLE(1.0 / 200.0);
...
    // Add to PrinterParams, after the list of fans.
    // Don't forget to add a comma after the fans.
    /*
     * Lasers.
     */
    MakeTypeList<
        PrinterMainLaserParams<
            'L', // Name
            'M', // DensityName
            LLaserPower,
            LMaxPower,
            // Use correct hardware PWM service here!
            Mk20ClockPwmService<Mk20ClockFTM1, 1, TeensyPin17>,
            LinearDutyFormulaService<
                15 // PowerBits
            >,
            LaserDriverService<
                // Use correct timer here!
                Mk20ClockInterruptTimerService<Mk20ClockFTM0, 7>,
                LDutyAdjustmentInterval,
                LaserDriverDefaultPrecisionParams
            >
        >
    >
```

In the g-code interface, *either* of the following parameters can be used in a `G0`/`G1` command to control the laser:
- **L** - Total energy emmitted over the segment [W]. This is the low level interface to the laser.
- **M** - Energy density over the segment [W/mm]. The value is multiplied by the segment length to obtain the effective energy for the segment.

The energy density *M* is cached, so you can specify it in one command, and leave it out for any further commands where you want the same energy density. If *L* is specified, it takes precedence over *M* or its cached value, but it does not alter the *M* cached value.

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

## Hardware PWM configuration

Hardware PWM can be used for heaters, fans or lasers. In all cases a board-specific procedure needs to be followed where a `PWM_SERVICE` expression specifying the PWM configuration is constructed. This is described later, first the application of this to heaters, fans and lasers is shown.

For *heaters and fans*, the `PWM_SERVICE` needs to be wrapped in `HardPwmService` before being used as the PwmService parameter in `PrinterMainHeaterParams` or `PrinterMainFanParams`, like this:

```
#include <aprinter/printer/pwm/HardPwm.h>
...
PrinterMainFanParams<
    ...
    HardPwmService< PWM_SERVICE > // This is the PwmService argument
>
```

On the other hand, for *lasers*, the `PWM_SERVICE` is directly used as the PwmService parameter of `PrinterMainLaserParams`:

```
PrinterMainLaserParams<
    ...
    PWM_SERVICE, // This is the PwmService argument
    ...
>
```

### Arduino Due

- First there's some global setup needed:
```
#include <aprinter/system/At91Sam3xPwm.h>
...
using MyAdc = ...;
using MyPwm = At91Sam3xPwm<MyContext, Program, At91Sam3xPwmParams<0, 0, 0, 0>>;
using MyPrinter = ...;
...
struct MyContext {
...
    using Pwm = MyPwm;
...
};
...
int main ()
{
...
    MyAdc::init(c);
    MyPwm::init(c); // After MyAdc, before MyPrinter!
    MyPrinter::init(c);
...
```

- Select a PWM capable pin. Apparently these are pins 2-13.
- Look into `aprinter/board/arduino_due_pins.h` and find the full name of the pin (e.g. for pin 8: `At91SamPin<At91SamPioC, 22>`).
- Look into `aprinter/system/At91Sam3xPwm.h` and find the entry of the `At91Sam3xPwmConnections` list corresponding to this pin. Note the channel number and the connection type. These are the first and second elements (e.g. 5, 'L').
- Use something like the following as `PWM_SERVICE` (adjust the prescaler/period as needed):

```
At91Sam3xPwmChannelService<
    10, // Prescaler [0, 10]
    1562, // Period [1, 16777215]. Frequency=(F_MCK/2^Prescaler/Period). 1562 gives ~50Hz
    5, // PWM channel number
    DuePin8, // Output pin
    'L' // output connection type (L/H)
>
```

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
