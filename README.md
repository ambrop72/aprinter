aprinter
========

APrinter is a currently experimantal firmware for RepRap 3D printers and is under heavy development.
It supports many controller boards based on AVR, Arduino Due (Atmel ARM) and Teensy 3 (Freescale ARM).

## Implemented features (possibly with bugs)

  * Highly configurable design. Extra heaters, fans and axes can be added easily, and
    PWM frequencies for heaters and fans are individually adjustable unless hardware PWM is used.
  * Runtime configuration system, and configuration storage to EEPROM.
    Availability depends on chip/board.
  * Delta robot and CoreXY support. Additionally, new geometries can be added easily by defining a transform class.
    Performance will be sub-optimal when using Delta on AVR platforms.
  * SD card printing. Supports reading g-code from FAT32 filesystems, and also directory listing.
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
  * Heater control using PID. There are no thermistor tables, you simply configure the thermistor parameters.
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

## Hardware requirements

Ports have been completed for the following boards:

  * Melzi (atmega1284p only),
  * RAMPS 1.3/1.4 (only RAMPS 1.4 with atmega2560 is tested),
  * RAMPS-FD(*), RADDS.
  * 4pi(*).
  * Teensy 3 (no standard board, needs manual wiring).

The (*) mark means that the board is supported by the firmware code but is currently unavailable
due to a lack of support in the web configuration system. If you want to try the firmware on one of these
boards, plase contact me and I will try to add the support.

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

## APrinter Web Service

The easiest way to get started is using the public web service,
available at [www.aprinter.eu](http://www.aprinter.eu/).
The service allows you to configure the firmware, and will build it at your request.
It should work with any modern web browser, and the only step that needs to be done
locally is uploading the firmware to the board.

If you want to, you can run the service locally, as described next.
A prerequisite is the [Nix package manager](http://nixos.org/nix/) running on Linux.
The service will then be available at `http://127.0.0.1:4000/`.

```
nix-build nix/ -A aprinterService -o ~/aprinter-service
mkdir ~/aprinter-service-temp
~/aprinter-service/bin/aprinter-service
```

Instructions for using the web service:
- Define a "configuration" - either modify or copy an existing configuration.
- Make changes to the configuration.
- Select this configuration in the combo box at the top.
- At the bottom of the page, press the compile button.
- Wait some time for the compilation to complete. You will receive a zip with the firmware image.
- Extract the zip and upload the firmware image to your board.

## Building manually

It is possible to build without using the web service, given a JSON configuration file.
This file would usually be produced by the web GUI, but you're free to manage it manually.
Again, the prerequisite for building is the [Nix package manager](http://nixos.org/nix/) on Linux.

```
python -B config_system/generator/generate.py --nix --config path_to_config.json | nix-build - -o ~/aprinter-service
```

## Uploading

Before you can upload, you need to install the uploading program, which depends on the type of microcontroller:
- AVR: avrdude (install with `nix-env -i avrdude`).
- Atmel ARM: BOSSA (intall with `nix-env -i bossa`).
- Teensy 3: teensy-loader (install with `nix-env -i teensy-loader`).

### RAMPS
```
avrdude -p atmega2560 -P /dev/ttyACM0 -b 115200 -c stk500v2 -D -U "flash:w:$HOME/aprinter-build/aprinter-nixbuild.hex:i"
```

### Melzi

You will have to set the Debug jumper and play with the reset button to get the upload going.

```
avrdude -p atmega1284p -P /dev/ttyUSB0 -b 57600 -c stk500v1 -D -U "flash:w:$HOME/aprinter-build/aprinter-nixbuild.hex:i"
```

### Arduino Due

Make sure you connect via the programming port while uploading (but switch to the native USB port for actual printer communication).

First you need to set baud rate to 1200 to start the bootloader:

```
stty -F /dev/ttyACM0 1200
```

Then upload the firmware using BOSSA (you can use the GUI if you like instead).

```
bossac -p ttyACM0 -U false -i -e -w -v -b "$HOME/aprinter-build/aprinter-nixbuild.bin" -R 
```

Some Due clones have a problem resetting. If after uploading, the firmware does not start (LED doesn't blink), press the reset button.

*NOTE*: You need to **use the native USB port** to connect. The programming port is only used for uploading.
However, it is possible to configure the firmware to use the programming port for communication.
To do that in the web GUI, expand the specific board definition (in the Boards list), expand "Serial parameters" and for the "Backend",
choose "AT91 UART".

### Teensy 3

You need to press the button on the board before trying to upload, to put the board into bootloader mode.

```
teensy-loader -mmcu=mk20dx128 "$HOME/aprinter-build/aprinter-nixbuild.hex"
```

## Runtime configuration

When runtime configuration is available, the values of many parameters can be adjusted at runtime, and the values specified in
the web GUI are used as defaults only.
Notable things that cannot be configured at runtime are various structural aspects such as the presence of devices/features
and hardware mapping of features (including pins).

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

## SD card support

The firmware supports reading G-code from a file in a FAT32 partition on an SD card.
When the SD card is being initialized, the first primary partition with a FAT32 filesystem signature will be used.

SD card support is enabled by default on the following boards: RADDS, RAMPS-FD, RAMPS 1.3, Melzi.

The following SD-card related commands will be available when SD card support is enabled:

- M21 - Initialize the SD card. On success the root directory will become the current directory.
- M22 - Deinitialize the SD card.
- M20 - List the contents of the current directory.
- M23 D<dir> - Change the current directory (<dir> must be a directory in the current directory, or ..).
- M23 R - Change to root directory.
- M23 F<file> - Select file for printing (<file> must be a file in the current directory).
- M24 - Start or resume SD printing.
- M25 - Pause SD printing.
- M26 - Rewind the current file to the beginning.

Example: print the file `gcodes/test.gcode`.

```
M21
M23 Dgcodes
M23 Ftest.gcode
M24
```

Example: restart the same print (whether or not it has completed).

```
M25
// wait for the printer to stop and clear the print surface
M26
M24
```

## Packed gcode

When printing from SD, the firmware can optionally read a custom packed form of gcode, to improve space and processing efficiency. The packing format [is documented](encoding.txt).

The choice between plain text and packed gcode needs to be made at compile time. By default, plain text is used.
To configure packed gcode in the web GUI, go to your Board, "SD card configuration" and set "G-code parser" to "Binary G-code parser".

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

Note that the ability to define new devices depends on the particular board.
In the web GUI, if there is an available "port" (stepper port, PWM output),
it is just a matter of adding a device to the Configuration section.
Otherwise it may or may not be possible to add a new port, based on hardware availability
(timer units, PWM channels).

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

## Coordinate transformation

APrinter has generic support for non-cartesian coordinate systems.
Currently, delta (towers) and CoreXY/H-bot are supported out of the box.

*NOTE*: Delta will not work well on AVR based platforms due to lack of CPU speed and RAM.
CoreXY may or may not.

Generally, the transformation is configured as follows in the web GUI:

- In the Steppers list, define the steppers (actuators) that are involved in the transform,
  with names that are *not* cartesian axes (delta: A, B, C, CoreXY: A, B).
  Make that for each such stepper, "Is cartesian" is set to No.
  Set the position limits correctly (see the sub-section next).
  For delta, enable homing for all three steppers, for CoreXY disable homing for both.
- Define any remaining cartesian steppers. So for CoreXY that would be Z, and none for delta.
  Of course extruders still belong to the Steppers list.
- In the "Coordinate transformation" section, select your particular transformation type.
  More configuration is made available.
- Set up the "Stepper mapping" to map the transformation to the steppers defined earlier.
  Type the stepper name letter to map a stepper.
  Note that you are free to define the mapping in any order, to achieve correct motion.
  Which is a bit tricky woth CoreXY - you may also have to invert stepper direction.
- Set the transformation-type specific parameters. The delta-related parameters mean exactly
  the same as for Marlin, so no further explanation will be given here.
- Configure the cartesian axes. Currently this is just position limits and maximum speed.
  But, for CoreXY, you have the option of enabling homing for cartesian axes
  (which is a different concept than stepper-specific homing).

### Stepper configuration

Steppers involved in the coordinate transform have their own configuration which is generally the same as for other steppers.
In particular they need to be defined the position limits.

For CoreXY, the position limits should not constrain motion as defined by the limits of the cartesian axes.
The following formulas give the minimum required stepper limits to support the cartesian limits
(assuming no reordering of steppers): Arange = [Xmin + Ymin, Xmax + Ymax], Brange = [Xmin - Ymax, Xmax - Ymin].

For delta, the stepper limits should be configured appropriately for the machine.
It helps to know that the A/B/C stepper positions are actually cartesian Z coordinates
of assemblies on the towers, and that the maximum position limit corresponds to
meeting the endstop at the top.

## Slave steppers

Slave steppers are extra steppers assigned to an axis. They will be driven synchronously with the main stepper for the axis.
Actually, the only difference between the main stepper and slave steppers is the way they are specified in the configuration.

In the web GUI, slave steppers can be added in the Stepper section for a particular stepper.
If there is no existing suitable stepper port definition, you will need to add one in the Board configuration.
When doing this, note that for slave steppers, the "Stepper timer" does not need to be defined (set it to "Not defined").

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

A laser is configured in the web GUI as follows:

- In the Board section, add a "Laser port". Give it a name (anything), select the PWM output.
  This must be a hardware PWM output.
  Also select an available timer unit. This is not exactly easy since the timer allocations
  are spread throughout the Board configuration (Event channel timer, stepper timers,
  timers for software PWM outputs).
- In the Configuration section, add a Laser. The default values here should be usable.
  But make sure your new "Laser port" is selected.

In the g-code interface, *either* of the following parameters can be used in a `G0`/`G1` command to control the laser:
- **L** - Total energy emmitted over the segment [W]. This is the low level interface to the laser.
- **M** - Energy density over the segment [W/mm]. The value is multiplied by the segment length to obtain the effective energy for the segment.

The energy density *M* is cached, so you can specify it in one command, and leave it out for any further commands where you want the same energy density.
If *L* is specified, it takes precedence over *M* or its cached value, but it does not alter the *M* cached value.

## Support

If you need help or want to ask me a question, you can find me on Freenode IRC in #reprap (nick ambro718),
or you can email me to ambrop7 at gmail dot com. If you have found a bug or have a feature request, you can use the issue tracker.
